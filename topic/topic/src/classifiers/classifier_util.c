/* -*- C -*-
 *
 * Copyright (c) 2008
 * Massachusetts Institute of Technology
 *
 * All Rights Reserved
 *
 *	FILE: classifier_util.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "util/basic_util.h"
#include "util/hash_util.h"
#include "classifiers/classifier_util.h"

static void prune_features_based_on_counts ( char **filenames, int num_files, 
					     FEATURE_SET *features, float min_count );
static void prune_features_based_on_counts_combined_file ( FILE *fp, int num_files, 
							   FEATURE_SET *features, float min_count, int max_line_length);
static int load_sparse_vector_if_binary_file ( SPARSE_FEATURE_VECTOR *feature_vector,
					       HASHTABLE *feature_name_to_index_hash,
					       FILE *fp );

/*******************************************************************************************/

FILE_LIST *read_file_list_from_file ( char *list_filename ) 
{
  // Getting list of training files
  FILE *fp = fopen_safe( list_filename, "r" );

  int num_files, max_line_length;
  num_files = count_lines_in_file (fp, &max_line_length);
  max_line_length++;
  char *line = (char *) calloc(max_line_length+1, sizeof(char));;
  char **filenames = (char **) calloc((size_t)(size_t)num_files, sizeof(char *));
  int i = 0;
  char *next;
  while ( fgets ( line, max_line_length, fp ) ) {
    next = strtok(line, " \r\t\n");
    if ( next == NULL ) die ( "Bad line (Line: %d) in file '%s'\n",i+1,(char *)list_filename);
    filenames[i++] = strdup(next);
  }
  fclose(fp);
  free(line);

  FILE_LIST *file_list = (FILE_LIST *) malloc(sizeof(file_list));
  file_list->num_files = num_files;
  file_list->filenames = filenames;
  return file_list;

}

/*******************************************************************************************/

CLASS_SET *create_class_set_from_file_list ( char *list_filename )
{
  // Open the list file and count the lines and max line length
  FILE *fp = fopen_safe( list_filename, "r" );
  int max_line_length;
  count_lines_in_file (fp, &max_line_length);
  max_line_length++;
  char *line = (char *)calloc(max_line_length+1,sizeof(char));

  // Create the hash table and fill it in with discovered class names
  HASHTABLE *class_hash = hdbmcreate( 100, hash2); 
  int line_num = 1;
  char *next;
  int num_classes=0;
  int class_id;
  while ( fgets ( line, max_line_length, fp ) ) {
    // First element is the filename...which we will ignore
    next = strtok(line, " \r\t\n");
    if ( next == NULL ) die ( "Bad line (Line: %d) in file '%s'\n", line_num, (char *)list_filename);
    
    // Get the next substring which will be the name of 
    // the first class in the list of classes for this file
    next = strtok(NULL, " \r\t\n");
    if ( next == NULL ) die ( "Bad line (Line: %d) in file '%s'\n", line_num, (char *)list_filename);

    while ( next != NULL ) {
      // See if we've seen this class before
      class_id = get_hashtable_string_index (class_hash, next);

      // If we haven't seen this class add it into the hash table
      if ( class_id == -1 ) {
	store_hashtable_string_index (class_hash, next, num_classes);
	num_classes++;
      }
      
      // Get the next class in this list
      next = strtok(NULL, " \r\t\n");
    }
    line_num++;
  }
  fclose(fp);
  free(line);

  // Fill in the class set structure
  CLASS_SET *classes = (CLASS_SET *) calloc((size_t)1, sizeof(CLASS_SET));
  classes->num_classes = num_classes;
  classes->class_name_to_class_index_hash = class_hash;
  classes->class_names = (char **) calloc((size_t)num_classes, sizeof(char *));
  fill_in_string_array_with_hash_entries (class_hash, classes->class_names, num_classes );

  return classes;

}

/*******************************************************************************************/

void free_class_set (CLASS_SET *class_set)
{
  hdbmdestroy(class_set->class_name_to_class_index_hash);
  int i;
  for ( i=0; i<class_set->num_classes; i++ ) {
    free(class_set->class_names[i]);
  }
  free(class_set->class_names);
  free(class_set);
  return;
}

// 
// create_feature_set_from_file
// This function creates a "feature set"
// This is really just a hash mapping from all the possible words (excluding stop words) to
// indices 
//
FEATURE_SET *create_feature_set_from_file ( char *count_fn, float min_feature_count, FEATURE_SET *stop_list )
{
  FILE *fp = fopen_safe(count_fn, "r" );
  int num_files, max_line_length;
  num_files = count_lines_in_file (fp, &max_line_length);
  max_line_length++;
  
  char **filenames = (char **) calloc((size_t)num_files, sizeof(char *));
  HASHTABLE *stop_list_hash = NULL;
  if ( stop_list != NULL && stop_list->feature_name_to_index_hash != NULL ) {
    stop_list_hash = stop_list->feature_name_to_index_hash;
  }

  int i = 0;
  char *next;
  char *line = (char *)calloc(max_line_length+1,sizeof(char));
  while (fgets (line, max_line_length, fp)) {
    next = strtok(line, " \r\t\n");
    if ( next == NULL ) die ( "Bad line (Line: %d) in file '%s'\n", i+1, count_fn);
    filenames[i++] = strdup(next);
  }
  free(line);

  FEATURE_SET *features = (FEATURE_SET *) malloc(sizeof(FEATURE_SET));
  features->num_features = 0;
  HASHTABLE *hash = hdbmcreate(1000, hash2);
  features->feature_name_to_index_hash = hash;
  
  int num_features = 0;
  char *feature_line = (char *) calloc(max_line_length+1, sizeof(char));
  char *cur_tok = (char *) calloc(max_line_length+1, sizeof(char));
  rewind(fp);
  char *rest, *string, *rest_tok, *word;
  int index;

  // This section goes through all of the word tokens and 
  // creates a hash only -- no counts are collected
  for (i=0; i<num_files; i++) {
     fgets(feature_line, max_line_length, fp);
     string = feature_line;
     next = strtok_r(string, " \r\t\n", &rest);  // Filename
     string = rest;
     next = strtok_r(string, " \r\t\n", &rest);  // First count feature
     string = rest;
     while (next!=NULL) {
	strcpy(cur_tok, next);
	word = strtok_r(cur_tok, "|", &rest_tok);
	index = get_hashtable_string_index (hash, word);
	if ( index == -1) {
	  if (stop_list_hash==NULL || get_hashtable_string_index(stop_list_hash, word)==-1 ) {
	    index = num_features;
	    store_hashtable_string_index (hash, word, index);
	    num_features++;
	  }
	}
	next = strtok_r(string, " \r\t\n", &rest);  // First count feature
	string = rest;
     }
  }
  free(feature_line);
  free(cur_tok);

  features->num_features = num_features;
  features->feature_names = (char **) calloc((size_t)num_features, sizeof(char *));
  features->feature_weights = (float *) calloc((size_t)num_features, sizeof(float));
  for (i=0; i<num_features; i++) 
     features->feature_weights[i] = 1.0;

  fill_in_string_array_with_hash_entries (hash, features->feature_names, num_features);

  rewind(fp);
  // Not used: min_feature_count set to 0 in current code
  // prune_features_based_on_counts_combined_file (fp, num_files, features, min_feature_count, max_line_length);

  fclose(fp);

  return features;

}

FEATURE_SET *create_feature_set_from_file_list ( char *list_filename, float min_feature_count, FEATURE_SET *stop_list )
{
  FILE *fp = fopen_safe( list_filename, "r" );
  int num_files, max_line_length;
  num_files = count_lines_in_file (fp, &max_line_length);
  max_line_length++;
  
  char **filenames = (char **) calloc((size_t)num_files, sizeof(char *));
  HASHTABLE *stop_list_hash = NULL;
  if ( stop_list != NULL && stop_list->feature_name_to_index_hash != NULL ) {
    stop_list_hash = stop_list->feature_name_to_index_hash;
  }

  int i = 0;
  char *next;
  char *line = (char *)calloc(max_line_length+1,sizeof(char));
  while ( fgets ( line, max_line_length, fp ) ) {
    next = strtok(line, " \r\t\n");
    if ( next == NULL ) die ( "Bad line (Line: %d) in file '%s'\n",i+1,list_filename);
    filenames[i++] = strdup(next);
  }
  fclose(fp);
  free(line);

  FEATURE_SET *features = (FEATURE_SET *) malloc(sizeof(FEATURE_SET));
  features->num_features = 0;
  HASHTABLE *hash = hdbmcreate(1000, hash2);
  features->feature_name_to_index_hash = hash;
  
  int num_features = 0;
    
  for (i=0; i<num_files; i++) {
    fp = fopen_safe(filenames[i], "r");
    int num_lines = count_lines_in_file (fp, &max_line_length);
    if ( num_lines > 0 ) {
      max_line_length++;
      char *feature_line = (char *) calloc(max_line_length+1, sizeof(char));
      char *string, *next;
      int index;
      while (fgets (feature_line, max_line_length, fp)) {
	string = feature_line;
	next = strtok ( string, " \r\t\n");
	if ( next == NULL ) die ("Bad file format in file %s\n", filenames[i]);
	index = get_hashtable_string_index (hash, next);
	if ( index == -1 ) {
	  if ( ( stop_list_hash == NULL ) || 
	       ( get_hashtable_string_index (stop_list_hash, next) == -1 ) ) {
	    index = num_features;
	    store_hashtable_string_index (hash, next, index);
	    num_features++;
	  }
	}
      }
      free(feature_line);
    }
    fclose(fp);

  }

  features->num_features = num_features;
  features->feature_names = (char **) calloc((size_t)num_features, sizeof(char *));
  features->feature_weights = (float *) calloc((size_t)num_features, sizeof(float));
  for (i=0; i<num_features; i++) 
     features->feature_weights[i] = 1.0;

  fill_in_string_array_with_hash_entries (hash, features->feature_names, num_features );
  prune_features_based_on_counts (filenames, num_files, features, min_feature_count );

  return features;

}

/*******************************************************************************************/

void augment_feature_set_from_file_list ( char *list_filename, FEATURE_SET *features, FEATURE_SET *stop_list )
{
  FILE *fp = fopen_safe( list_filename, "r" );
  int num_files, max_line_length;
  num_files = count_lines_in_file (fp, &max_line_length);
  max_line_length++;
  char *line = (char *) calloc(max_line_length+1, sizeof(char));
  char **filenames = (char **) calloc((size_t)num_files, sizeof(char *));
  HASHTABLE *stop_list_hash = NULL;
  if ( stop_list != NULL && stop_list->feature_name_to_index_hash != NULL ) {
    stop_list_hash = stop_list->feature_name_to_index_hash;
  }

  int i = 0;
  char *next;
  while ( fgets ( line, max_line_length, fp ) ) {
    next = strtok(line, " \r\t\n");
    if ( next == NULL ) die ( "Bad line (Line: %d) in file '%s'\n",i+1,list_filename);
    filenames[i++] = strdup(next);
  }
  fclose(fp);
  free(line);

  int num_features = features->num_features;
  HASHTABLE *hash = features->feature_name_to_index_hash;
    
  for ( i=0; i<num_files; i++ ) {
    fp = fopen_safe( filenames[i], "r" );
    int num_lines = count_lines_in_file (fp, &max_line_length);
    if ( num_lines > 0 ) {
      max_line_length++;
      char *feature_line = (char *)calloc(max_line_length+1,sizeof(char));
      char *string, *next;
      while ( fgets ( feature_line, max_line_length, fp ) ) {
	string = feature_line;
	next = strtok ( string, " \r\t\n");
	if ( ( stop_list_hash == NULL ) || ( get_hashtable_string_index (stop_list_hash, next) == -1 ) ) {
	  int index = get_hashtable_string_index (hash, next);
	  if ( index == -1 ) {
	    index = num_features;
	    store_hashtable_string_index (hash, next, index);
	    num_features++;
	  }
	}
      }
      free(feature_line);
    }
    fclose(fp);
  }

  if ( features->feature_weights != NULL ) free ( features->feature_weights );
  if ( features->feature_names != NULL ) {
    for ( i=0; i<features->num_features; i++ ) {
      if (features->feature_names[i] != NULL ) free(features->feature_names[i]);
    }
    free(features->feature_names);
  }


  features->num_features = num_features;
  features->feature_weights = (float *) calloc(num_features, sizeof(float));
  for ( i=0; i<num_features; i++ ) features->feature_weights[i] = 1.0;
  features->feature_names = (char **) calloc(num_features, sizeof(char *));

  fill_in_string_array_with_hash_entries (hash, features->feature_names, num_features );
  
  return;

}

/*******************************************************************************************/

static void prune_features_based_on_counts_combined_file (FILE *fp, int num_files, FEATURE_SET *features, float min_count, int max_line_length)
{
  if ( min_count <= 0.0 ) 
     return;

  die("not ported yet ...\n");

  int init_num_features = features->num_features;
  float *init_feature_counts = (float *) calloc((size_t)init_num_features, sizeof(float));
  char **init_feature_names = features->feature_names;
  HASHTABLE *hash = features->feature_name_to_index_hash;

  printf("got here ...\n");

  int i, index;
  char *feature_line = (char *) calloc(max_line_length+1,sizeof(char));
  char *cur_tok = (char *) calloc(max_line_length+1, sizeof(char));
  char *rest, *string, *rest_tok, *word, *next, *cur_tok_tmp;
  float count_value;

  for (i=0; i<num_files; i++ ) {
     fgets(feature_line, max_line_length, fp);
     string = feature_line;
     next = strtok_r(string, " \r\t\n", &rest);  // Filename
     string = rest;
     next = strtok_r(string, " \r\t\n", &rest);  // First count feature
     string = rest;
     cur_tok_tmp = cur_tok;
     while (next!=NULL) {
	strcpy(cur_tok_tmp, next);
	word = strtok_r(cur_tok_tmp, "|", &rest_tok);
	cur_tok_tmp = rest_tok;
	index = get_hashtable_string_index (hash, word);
	printf("index = %d\n", index);
	if ( index > -1 && index <init_num_features) {
	   word = strtok_r(cur_tok_tmp, "|", &rest_tok);
	   count_value =  atof(word);
	   printf("index = %d, count_value = %f\n", index, count_value);
	   if (isnan(count_value))
	      die ("Nan detected in file : %s\n", feature_line);
	   if (isinf(count_value))
	      die ("Inf detected in file : %s\n", feature_line);
	   init_feature_counts[index] += count_value;
	}
	next = strtok_r(string, " \r\t\n", &rest);  // First count feature
	string = rest;
     }
  }

  int prune = 0;
  int num_new_features = 0;
  for (i=0; i<init_num_features; i++ ) {
    if ( init_feature_counts[i] <= min_count ) {
      prune = 1;
    } else {
      num_new_features++;
    }
  }

  printf("checking on pruning!\n");

  if ( !prune ) {
    free(init_feature_counts);
    return;
  }

  printf("need to prune!\n");
  exit(0);

  // If anything needs to be pruned then rebuild the hash from scratch
  hdbmdestroy(hash);
  hash = hdbmcreate( (unsigned) num_new_features, hash2);
  
  // Initialize the new list and add a <filler> model
  num_new_features++;
  char **new_feature_names = (char **) calloc ((size_t)num_new_features, sizeof(char *));
  float *new_feature_weights = (float *) calloc ((size_t)num_new_features, sizeof(float));
  new_feature_names[0]  = strdup ("<filler>");
  new_feature_weights[0] = 1.0;
  store_hashtable_string_index (hash, new_feature_names[0], 0);
  num_new_features = 1;

  for ( i=0; i<init_num_features; i++ ) {
    if ( init_feature_counts[i] > min_count ) {
      new_feature_names[num_new_features] = strdup(init_feature_names[i]);
      new_feature_weights[num_new_features] = 1.0;
      store_hashtable_string_index (hash, init_feature_names[i], num_new_features);
      num_new_features++;
    }
    free(init_feature_names[i]); 
  }
  free(features->feature_names);
  free(features->feature_weights);
  free(init_feature_counts);
  
  features->num_features = num_new_features;
  features->feature_names = new_feature_names;
  features->feature_weights = new_feature_weights;
  features->feature_name_to_index_hash = hash;

}

static void prune_features_based_on_counts ( char **filenames, int num_files, 
					    FEATURE_SET *features, float min_count )
{
  if ( min_count <= 0.0 ) return;

  int init_num_features = features->num_features;
  float *init_feature_counts = (float *) calloc((size_t)init_num_features, sizeof(float));
  char **init_feature_names = features->feature_names;
  HASHTABLE *hash = features->feature_name_to_index_hash;

  int i;
  FILE *fp; 
  int max_line_length;
  for ( i=0; i<num_files; i++ ) {
    fp = fopen_safe( filenames[i], "r" );
    int num_lines = count_lines_in_file (fp, &max_line_length);
    if ( num_lines > 0 ) {
      max_line_length++;
      char *feature_line = (char *) calloc(max_line_length+1,sizeof(char));
      char *string, *next;
      while ( fgets ( feature_line, max_line_length, fp ) ) {
	string = feature_line;
	next = strtok ( string, " \r\t\n");
	int index = get_hashtable_string_index (hash, next);
	// If the feature is being used then record its value
	if ( index > -1 && index <init_num_features && (next = strtok(NULL," \r\t\n")) ) {
	  float count_value =  atof(next);
	  if ( isnan(count_value) )
	    die ("Nan detected in file '%s': %s\n", filenames[i], feature_line);
	  if ( isinf(count_value) )
	    die ("Inf detected in file '%s': %s\n", filenames[i], feature_line);
	  init_feature_counts[index] += count_value;
	}
      }
      free(feature_line);
    }
    fclose(fp);
  }

  int prune = 0;
  int num_new_features = 0;
  for (i=0; i<init_num_features; i++ ) {
    if ( init_feature_counts[i] <= min_count ) {
      prune = 1;
    } else {
      num_new_features++;
    }
  }

  if ( !prune ) {
    free(init_feature_counts);
    return;
  }

  // If anything needs to be pruned then rebuild the hash from scratch
  hdbmdestroy(hash);
  hash = hdbmcreate( (unsigned)num_new_features, hash2);
  
  // Initialize the new list and add a <filler> model
  num_new_features++;
  char **new_feature_names = (char **) calloc ((size_t)num_new_features, sizeof(char *));
  float *new_feature_weights = (float *) calloc ((size_t)num_new_features, sizeof(float));
  new_feature_names[0]  = strdup ("<filler>");
  new_feature_weights[0] = 1.0;
  store_hashtable_string_index (hash, new_feature_names[0], 0);
  num_new_features = 1;

  for ( i=0; i<init_num_features; i++ ) {
    if ( init_feature_counts[i] > min_count ) {
      new_feature_names[num_new_features] = strdup(init_feature_names[i]);
      new_feature_weights[num_new_features] = 1.0;
      store_hashtable_string_index (hash, init_feature_names[i], num_new_features);
      num_new_features++;
    }
    free(init_feature_names[i]); 
  }
  free(features->feature_names);
  free(features->feature_weights);
  free(init_feature_counts);
  
  features->num_features = num_new_features;
  features->feature_names = new_feature_names;
  features->feature_weights = new_feature_weights;
  features->feature_name_to_index_hash = hash;

}

/*******************************************************************************************/

SPARSE_FEATURE_VECTORS *copy_sparse_feature_vectors ( SPARSE_FEATURE_VECTORS *orig_feature_vectors ) 
{

  SPARSE_FEATURE_VECTORS *new_feature_vectors;
  new_feature_vectors = (SPARSE_FEATURE_VECTORS *) malloc(sizeof(SPARSE_FEATURE_VECTORS));

  int num_vectors = orig_feature_vectors->num_vectors;
  new_feature_vectors->num_vectors = num_vectors;
  new_feature_vectors->num_sets = orig_feature_vectors->num_sets;
  new_feature_vectors->feature_set = orig_feature_vectors->feature_set;
  new_feature_vectors->class_set = orig_feature_vectors->class_set;
  
  new_feature_vectors->vectors = (SPARSE_FEATURE_VECTOR **) calloc((size_t)num_vectors, 
								   sizeof(SPARSE_FEATURE_VECTOR *));
  
  int v;
  for ( v=0; v<num_vectors; v++ ) {
    new_feature_vectors->vectors[v] = copy_sparse_feature_vector ( orig_feature_vectors->vectors[v] );
  }
  
  return new_feature_vectors;


}

SPARSE_FEATURE_VECTOR *copy_sparse_feature_vector (SPARSE_FEATURE_VECTOR *orig_vector)
{
  SPARSE_FEATURE_VECTOR *new_vector = (SPARSE_FEATURE_VECTOR *) malloc(sizeof(SPARSE_FEATURE_VECTOR));

  if ( orig_vector->filename != NULL )  new_vector->filename = strdup(orig_vector->filename);
  else new_vector->filename = NULL;

  new_vector->set_id = orig_vector->set_id;
  new_vector->class_id = orig_vector->class_id;
  new_vector->num_labels = orig_vector->num_labels;
  if ( orig_vector->class_ids != NULL ) {
    new_vector->class_ids = (int *) calloc(new_vector->num_labels, sizeof(int));
    memcpy ( new_vector->class_ids, orig_vector->class_ids, new_vector->num_labels*sizeof(int));
  } else {
    new_vector->class_ids = NULL;
  }
  new_vector->num_features = orig_vector->num_features;
  
  if ( orig_vector->feature_indices != NULL ) {
    new_vector->feature_indices = (int *) calloc(new_vector->num_features, sizeof(int));
    memcpy ( new_vector->feature_indices, orig_vector->feature_indices, new_vector->num_features*sizeof(int));
  } else {
    new_vector->feature_indices = NULL;
  }

  if ( orig_vector->feature_values != NULL ) {
    new_vector->feature_values = (float *) calloc(new_vector->num_features, sizeof(float));
    memcpy ( new_vector->feature_values, orig_vector->feature_values, new_vector->num_features*sizeof(float));
  } else {
    new_vector->feature_values = NULL;
  }

  return new_vector;

}

/*******************************************************************************************/

SPARSE_FEATURE_VECTORS *load_sparse_feature_vectors_combined (char *count_fn, FEATURE_SET *feature_set, CLASS_SET *class_set)
{

  // Check the feature_set and class_set args
  if (feature_set == NULL) 
     die ("Feature set passed into load_sparse_feature_vectors is NULL\n");
  
  if (class_set != NULL) 
     die("Unimplemented feature.\n");

  // Open the file list and count the lines and max line length
  FILE *fp = fopen_safe(count_fn, "r");
  int num_vectors, max_line_length;
  num_vectors = count_lines_in_file (fp, &max_line_length);
  max_line_length += 2;
  if (num_vectors == 0) 
     die ("Specified file is empty: %s\n", count_fn);

  printf("number of vectors: %d\n", num_vectors);

  // Allocate the space for the feature vectors
  SPARSE_FEATURE_VECTORS *feature_vectors;
  feature_vectors = (SPARSE_FEATURE_VECTORS *) malloc(sizeof(SPARSE_FEATURE_VECTORS));
  feature_vectors->num_vectors = num_vectors;
  feature_vectors->num_sets = -1; // This we be set elsewhere
  feature_vectors->vectors = (SPARSE_FEATURE_VECTOR **) calloc((size_t)num_vectors, sizeof(SPARSE_FEATURE_VECTOR *));
  feature_vectors->feature_set = feature_set;
  feature_vectors->class_set = class_set;
  
  // Go through the count file loading vectors
  char *line = (char *) calloc(max_line_length+3, sizeof(char));
  int num_files = 0;
  char **substrings;
  int num_substrings;
  int i;
  float step_size = ((float)num_vectors/10);
  float next_step = step_size;
  int step = 1;
  while (fgets (line, max_line_length, fp)) {
    if ( ((float)num_files) > next_step ) {
      printf("%d%%...",step*10); fflush(stdout);
      next_step += step_size;
      step++;
    }
    substrings = split_string( line, " \n\r\t", &num_substrings );
    if (num_substrings == 0) 
      die ("Bad format in line %d of file '%s' \n", num_files+1, count_fn);

    // Grab the features from the sparse feature vector
    feature_vectors->vectors[num_files] = load_sparse_feature_vector_combined (substrings, num_substrings, feature_set);

    if ( class_set == NULL ) {
      feature_vectors->vectors[num_files]->class_ids = NULL;
    } else {
       die("Unimplemented feature.\n");
    }

    // Free the substrings and the substring array
    for (i=0; i<num_substrings; i++) {
      if (substrings[i]!=NULL) 
	 free(substrings[i]);
    }
    free(substrings);
    feature_vectors->vectors[num_files]->set_id = -1;
    num_files++;

  }
  
  fclose(fp);

  if (line!=NULL)
	  free(line);

  return feature_vectors;

}


SPARSE_FEATURE_VECTORS *load_sparse_feature_vectors ( char *list_filename,
						      FEATURE_SET *feature_set,
						      CLASS_SET *class_set)
{

  // Check the feature_set and class_set args
  if ( feature_set == NULL ) die ("Feature set passed into load_sparse_feature_vectors is NULL\n");
  
  HASHTABLE *class_hash = NULL;
  if ( class_set != NULL ) class_hash = class_set->class_name_to_class_index_hash;

  // Open the file list and count the lines and max line length
  FILE *fp = fopen_safe( list_filename, "r" );
  int num_vectors, max_line_length;
  num_vectors = count_lines_in_file (fp, &max_line_length);
  max_line_length += 2;
  if ( num_vectors == 0 ) die ("Specified file is empty: %s\n",list_filename);

  // Allocate the space for the feature vectors
  SPARSE_FEATURE_VECTORS *feature_vectors;
  feature_vectors = (SPARSE_FEATURE_VECTORS *) malloc(sizeof(SPARSE_FEATURE_VECTORS));
  feature_vectors->num_vectors = num_vectors;
  feature_vectors->num_sets = -1; // This we be set elsewhere
  feature_vectors->vectors = (SPARSE_FEATURE_VECTOR **) calloc((size_t)num_vectors, sizeof(SPARSE_FEATURE_VECTOR *));
  feature_vectors->feature_set = feature_set;
  feature_vectors->class_set = class_set;
  
  // Go through the filelist loading file and class labels
  char *line = (char *) calloc(max_line_length+3, sizeof(char));
  int class_id;
  int num_files = 0;
  char **substrings;
  int num_substrings;
  int i;
  int num_labels;
  float step_size = ((float)num_vectors/10);
  float next_step = step_size;
  int step = 1;
  while ( fgets ( line, max_line_length, fp ) ) {
    if ( ((float)num_files) > next_step ) {
      printf("%d%%...",step*10); fflush(stdout);
      next_step += step_size;
      step++;
    }
    substrings = split_string( line, " \n\r\t", &num_substrings );
    if ( num_substrings == 0 ) 
      die ("Bad format in line %d of file '%s' \n", num_files+1, list_filename);

    num_labels = num_substrings - 1;
    
    // Load the features from the file listed in the first substring
    // printf("loading: %s\n", substrings[0]);
    feature_vectors->vectors[num_files] = load_sparse_feature_vector (substrings[0], feature_set);

    // If we have a set of known classes then read in the class labels associated with this file
    if ( class_set == NULL ) {
      feature_vectors->vectors[num_files]->class_ids = NULL;
    } else {
      feature_vectors->vectors[num_files]->num_labels = num_labels;
      if ( num_labels > 0 ) {
	feature_vectors->vectors[num_files]->class_ids = (int *) calloc((size_t)num_labels, sizeof(int));
      } else {
	feature_vectors->vectors[num_files]->class_ids = NULL;
      }
      
      for ( i=1; i<num_substrings; i++ ) {
	// Get the index of the class label
	class_id = get_hashtable_string_index (class_hash, substrings[i]);
	if ( class_id == -1 ) {
	  die ("Unknown class name '%s' for file '%s'\n", substrings[i], substrings[0]);
	}

	// Store the index of this class label and then free the class label substring
	if ( i == 1 ) feature_vectors->vectors[num_files]->class_id = class_id;
	int j;
	for ( j=0; j<i-1; j++ ) {
	  if ( feature_vectors->vectors[num_files]->class_ids[j] == class_id ) {
	    die ("Class repeated for file: %s\n",substrings[0]);
	  }
	}
	feature_vectors->vectors[num_files]->class_ids[i-1] = class_id;
      }
    } 
    // Free the substrings and the substring array
    for ( i=0; i<num_substrings; i++ ) {
      if ( substrings[i] != NULL ) free(substrings[i]);
    }
    free(substrings);
    
    feature_vectors->vectors[num_files]->set_id = -1;
    num_files++;
  }
  fclose(fp);

  if ( line != NULL ) free(line);

  return feature_vectors;

}

SPARSE_FEATURE_VECTOR *load_sparse_feature_vector_combined (char *substrings[], int num_substrings, FEATURE_SET *feature_set)
{
  SPARSE_FEATURE_VECTOR *feature_vector;
  HASHTABLE *feature_name_to_index_hash = feature_set->feature_name_to_index_hash;
  int num_features;
  int *feature_indices;
  float *feature_values;
  float total_sum;
  char filler[10];

  strcpy (filler, "<filler>");
  int filler_index = get_hashtable_string_index(feature_name_to_index_hash, filler);  

  // Initialize the feature vector structure
  feature_vector = (SPARSE_FEATURE_VECTOR *) malloc(sizeof(SPARSE_FEATURE_VECTOR));
  feature_vector->filename = strdup(substrings[0]);
  feature_vector->num_labels = -1; // This gets filled in by the calling function
  feature_vector->class_id = -1; // This gets filled in by the calling function
  feature_vector->class_ids = NULL; // This gets filled in by the calling function
  feature_vector->set_id = -1; // This gets filled in by the calling function
  feature_vector->num_features = 0;
  feature_vector->total_sum = 0.0;
  feature_vector->feature_indices = NULL;
  feature_vector->feature_values = NULL;

  num_features = num_substrings-1;
  if (num_features == 0) 
     return feature_vector;
  feature_indices = (int *) calloc((size_t)num_features, sizeof(int));
  feature_values = (float *) calloc((size_t)num_features, sizeof(float));

  // Go through the lines in the file one by 
  // one extracting the feature values
  num_features = 0;
  total_sum = 0;
  int index;
  int i, j;
  int stop_loop;
  float value;
  char *cur_tok, *rest_tok, * word;

  for (j=1; j<num_substrings; j++) {
     cur_tok = substrings[j];
     word = strtok_r(cur_tok, "|", &rest_tok);
     cur_tok = rest_tok;

    // Lookup the array index for the feature name
    index = get_hashtable_string_index(feature_name_to_index_hash, word);

    // If the feature name is not found see if we are using a filler feature
    if (index == -1) 
       index = filler_index;
    
    // If the feature is being used then record its value
    if (index > -1 && index < feature_set->num_features) {
       word = strtok_r(cur_tok, "|", &rest_tok);
       cur_tok = rest_tok;

       if (word != NULL) {
	  value = atof(word);

	  if (isnan(value))
	     die ("Nan detected in file : %s\n", substrings[j]);
	  if ( isinf(value) )
	     die ("Nan detected in file : %s\n", substrings[j]);

	  // Sort the feature into place
	  i=num_features;
	  stop_loop = 0;
	  while ( i>0 && !stop_loop ) {
	     if (index < feature_indices[i-1]) {
		feature_indices[i] = feature_indices[i-1]; 
		feature_values[i] = feature_values[i-1];
		i--;
	     } else {
		stop_loop = 1;
	     }
	  }
	  feature_indices[i] = index;
	  feature_values[i] = value;
	  num_features++;
	  total_sum += value; 
       }
    }
  }

  feature_vector->num_features = num_features;
  feature_vector->total_sum = total_sum;
  feature_vector->feature_indices = feature_indices;
  feature_vector->feature_values = feature_values;

  return feature_vector;

}

SPARSE_FEATURE_VECTOR *load_sparse_feature_vector ( char *filename, FEATURE_SET *feature_set )
{
  SPARSE_FEATURE_VECTOR *feature_vector;
  HASHTABLE *feature_name_to_index_hash = feature_set->feature_name_to_index_hash;
  int num_features, max_line_length;
  int *feature_indices;
  float *feature_values;
  float total_sum;
  char filler[10];

  strcpy (filler, "<filler>");
  int filler_index = get_hashtable_string_index(feature_name_to_index_hash, filler);  

  // Initialize the feature vector structure
  feature_vector = (SPARSE_FEATURE_VECTOR *) malloc(sizeof(SPARSE_FEATURE_VECTOR));
  feature_vector->filename = strdup(filename);
  feature_vector->num_labels = -1; // This gets filled in by the calling function
  feature_vector->class_id = -1; // This gets filled in by the calling function
  feature_vector->class_ids = NULL; // This gets filled in by the calling function
  feature_vector->set_id = -1; // This gets filled in by the calling function
  feature_vector->num_features = 0;
  feature_vector->total_sum = 0.0;
  feature_vector->feature_indices = NULL;
  feature_vector->feature_values = NULL;

  // Open the feature vector file and count lines and max line length
  FILE *fp = fopen_safe(filename, "r");

  int debug = 0;
  // char *debug_str = strdup("fe-03-02742");
  // if ( strstr(filename, debug_str) ) debug = 1;

  if ( load_sparse_vector_if_binary_file ( feature_vector, feature_name_to_index_hash, fp ) ) 
    return feature_vector;

  num_features = count_lines_in_file (fp, &max_line_length);
  if ( num_features == 0 ) return feature_vector;
  max_line_length += 1;
  feature_indices = (int *) calloc((size_t)num_features, sizeof(int));
  feature_values = (float *) calloc((size_t)num_features, sizeof(float));

  // Go through the lines in the file one by 
  // one extracting the feature values
  num_features = 0;
  total_sum = 0;
  int index;
  int i;
  int stop_loop;
  float value;
  char *line = (char *) calloc(max_line_length+2, sizeof(char));
  char *string, *next;
  while ( fgets ( line, max_line_length+1, fp ) ) {
    string = line;
    //printf("string='%s'...",string); fflush(stdout);
    next = strtok( string, " \r\t\n");
    //printf("next='%s'...",next); fflush(stdout);
   
    //printf("G1..."); fflush(stdout);


    if ( debug ) printf ( "%s", next);

    // Lookup the array index for the feature name
    index = get_hashtable_string_index(feature_name_to_index_hash, next);

    //printf("G2..."); fflush(stdout);

    // If the feature name is not found see if we are using a filler feature
    if ( index == -1 ) index = filler_index;
    
    // If the feature is being used then record its value
    if ( index > -1 && index < feature_set->num_features ) {
      //printf("G3..."); fflush(stdout);
      if ( (next = strtok(NULL," \r\t\n")) ) {
	
	//printf("next='%s'...",next); fflush(stdout);
	value = atof(next);
	//printf("value='%f'...",value); fflush(stdout);
	

	if ( isnan(value) )
	  die ("Nan detected in file '%s': %s\n", filename, line);
	if ( isinf(value) )
	  die ("Nan detected in file '%s': %s\n", filename, line);
	//printf("G4..."); fflush(stdout);

	// Sort the feature into place
	i=num_features;
	//printf("G5..."); fflush(stdout);
        stop_loop = 0;
	//printf("G6..."); fflush(stdout);
	while ( i>0 && !stop_loop ) {
	  // printf("i=%d...",i); fflush(sAtdout);
	  if ( index < feature_indices[i-1] ) {
	    feature_indices[i] = feature_indices[i-1]; 
	    feature_values[i] = feature_values[i-1];
	    i--;
	  } else {
	    stop_loop = 1;
	  }
	}
	feature_indices[i] = index;
	feature_values[i] = value;
	num_features++;
	total_sum += value; 
      }
    }

    
    if ( debug ) printf ( "\n");
  }
  fclose(fp);
  free(line);

  feature_vector->num_features = num_features;
  feature_vector->total_sum = total_sum;
  feature_vector->feature_indices = feature_indices;
  feature_vector->feature_values = feature_values;

  return feature_vector;

}

static int load_sparse_vector_if_binary_file ( SPARSE_FEATURE_VECTOR *feature_vector,
					       HASHTABLE *feature_name_to_index_hash,
					       FILE *fp )
{

  // Check to see if the filler index is being used
  char filler[10];
  strcpy (filler, "<filler>");
  int filler_index = get_hashtable_string_index(feature_name_to_index_hash, filler);  

  // First check first line of file to see if this is a binary file
  rewind(fp);
  int c;
  int stop = 0;
  int char_count = 0;
  while ( (c=fgetc(fp)) != EOF && !stop ) {
    if ( c == '\n' ) stop = 1;
    char_count++;
  }
  rewind(fp);
  char_count++;
  char *line = (char *)calloc(char_count+1, sizeof(char));
  if ( ! fgets ( line, char_count, fp ) ) {
    // If it's an empty file, rewind and return without loading
    rewind(fp);
    return 0;
  }
  // If it's not a binary file, rewind and return without loading
  if ( strcmp ( line, "BINARY_VECTOR\n" ) ) {
    rewind(fp);
    return 0;
  }
  free(line);

  // Load the full feature vector
  int num_features = load_int(fp);
  float *values = load_float_array (num_features, fp);
  char **names = load_string_array (num_features, fp);
  fclose(fp);

  int *feature_indices = (int *) calloc(num_features, sizeof(int));
  float *feature_values = (float *) calloc(num_features, sizeof(float));
  int count =0;
  int i, j;

  int index;
  for ( i=0; i<num_features; i++ ) {

    // Lookup the array index for the feature name
    index = get_hashtable_string_index(feature_name_to_index_hash, names[i]);
    free(names[i]);

    // If the feature name is not found assign it to the filler index
    if ( index == -1 ) index = filler_index;

    // If the feature is being used then record its index and value
    if ( index > -1 ) {

      j = count;
      // Sort the features by index if requested
      if ( 0 ) {
	stop = 0;
	while ( j>0 && !stop ) {
	  if ( index < feature_indices[j-1] ) {
	    feature_indices[j] = feature_indices[j-1];
	    feature_values[j] = feature_values[j-1];
	    j--;
	  } else {
	    stop = 1;
	  }
	}
      } 
      feature_indices[j] = index;
      feature_values[j] = values[i];
      count++;
    }
  }

  free(names);
  free(values);

  feature_vector->num_features = count;
  feature_vector->feature_indices = feature_indices;
  feature_vector->feature_values = feature_values;

  return 1;


}

float *extract_feature_counts_from_sparse_feature_vectors ( SPARSE_FEATURE_VECTORS *feature_vectors )
{
  SPARSE_FEATURE_VECTOR **vectors = feature_vectors->vectors;
  int num_vectors = feature_vectors->num_vectors;
  FEATURE_SET *features = feature_vectors->feature_set;
  int num_features = features->num_features;
  int i, v, index;
  float count;
  float *counts = (float *) calloc(num_features, sizeof(float));

  if ( vectors != NULL ) {
    // Collect counts from feature vectors
    for ( v = 0; v < num_vectors; v++ ) {
      for ( i = 0; i < vectors[v]->num_features; i++ ) {
	index = vectors[v]->feature_indices[i];
	count = vectors[v]->feature_values[i];
	counts[index] += count;
      }
    }
      
  }
  return counts;
}

/*******************************************************************************************/

void free_sparse_feature_vectors ( SPARSE_FEATURE_VECTORS *feature_vectors )
{
  int i;
  if ( feature_vectors->vectors != NULL ) {
    for ( i=0; i<feature_vectors->num_vectors; i++ )
      free_sparse_feature_vector(feature_vectors->vectors[i]);
    free(feature_vectors);
    feature_vectors = NULL;
  }
  return;
}

void free_sparse_feature_vector ( SPARSE_FEATURE_VECTOR *vector )
{
  if ( vector != NULL ) {
    if ( vector->filename != NULL ) free(vector->filename);
    if ( vector->class_ids != NULL ) free(vector->class_ids);
    if ( vector->feature_indices != NULL ) free(vector->feature_indices);
    if ( vector->feature_values != NULL ) free(vector->feature_values);
    free(vector);
    vector = NULL;
  }
  return;
}

/*******************************************************************************************/

void partition_feature_vectors_into_sets ( SPARSE_FEATURE_VECTORS *feature_vectors, int num_sets )
{
  int num_vectors = feature_vectors->num_vectors;
  SPARSE_FEATURE_VECTOR **vectors = feature_vectors->vectors;
  
  // Sort by class ID
  qsort ( vectors, (size_t)num_vectors, sizeof(SPARSE_FEATURE_VECTOR *), feature_vector_class_cmp );

  // Assign each feature vector to a set
  int i;
  int set=0;
  for ( i=0; i<num_vectors; i++ ) {
    vectors[i]->set_id = set;
    set++;
    if ( set >= num_sets ) set = 0;
  }

  // Sort by set assignment
  qsort ( vectors, (size_t)num_vectors, sizeof(SPARSE_FEATURE_VECTOR *), feature_vector_set_cmp );
 
  feature_vectors->num_sets = num_sets;

  return;

}

/*******************************************************************************************/

int feature_vector_class_cmp(const void *a, const void *b)
{
  const SPARSE_FEATURE_VECTOR **va = (const SPARSE_FEATURE_VECTOR **)a;
  const SPARSE_FEATURE_VECTOR **vb = (const SPARSE_FEATURE_VECTOR **)b;

  if ( (*va)->class_id > (*vb)->class_id ) return 1; 
  else if ( (*va)->class_id  < (*vb)->class_id ) return -1; 
  else return 0;
}

int feature_vector_set_cmp(const void *a, const void *b)
{
  const SPARSE_FEATURE_VECTOR **va = (const SPARSE_FEATURE_VECTOR **)a;
  const SPARSE_FEATURE_VECTOR **vb = (const SPARSE_FEATURE_VECTOR **)b;
  if ( (*va)->set_id > (*vb)->set_id ) return 1; 
  else if ( (*va)->set_id  < (*va)->set_id ) return -1; 
  else return 0;
}

/*******************************************************************************************/

void L1_normalize_sparse_feature_vectors ( SPARSE_FEATURE_VECTORS *feature_vectors )
{
  int i, j;
  // Normalize feature vectors so total sum equals 1
  // This is for converting counts into relative frequencies
  for ( i=0; i<feature_vectors->num_vectors; i++ ) {
    SPARSE_FEATURE_VECTOR *feature_vector = feature_vectors->vectors[i];
    float *values = feature_vector->feature_values;
    float sum = 0;
    for ( j=0; j<feature_vector->num_features; j++ ) sum += values[j];
    for ( j=0; j<feature_vector->num_features; j++ ) values[j] = values[j]/sum;
  }
  return;
}

void L2_normalize_sparse_feature_vectors ( SPARSE_FEATURE_VECTORS *feature_vectors )
{
  int i, j;
  // L2 normalize feature vectors to unit length vectors
  for ( i=0; i<feature_vectors->num_vectors; i++ ) {
    SPARSE_FEATURE_VECTOR *feature_vector = feature_vectors->vectors[i];
    float *values = feature_vector->feature_values;
    float sum = 0;
    for ( j=0; j<feature_vector->num_features; j++ ) sum += values[j]*values[j];
    for ( j=0; j<feature_vector->num_features; j++ ) values[j] = values[j]/sqrt(sum);
  }
  return;
}

/*******************************************************************************************/
FEATURE_SET *load_feature_set ( char *filename )
{
  FEATURE_SET *feature_set = (FEATURE_SET *) malloc(sizeof(FEATURE_SET));
  FILE *fp;
  int num_features;
  int max_line_length;

  // Open the feature file and count the lines
  // There should be one line per feature
  fp = fopen_safe( filename, "r" );
  num_features = count_lines_in_file (fp, &max_line_length);
  feature_set->num_features = num_features;

  // Allocate space for loading in feature set information
  max_line_length++;
  char *line = (char *) calloc(max_line_length+1, sizeof(char));
  char *token_string = (char *) calloc(max_line_length+1, sizeof(char));
  char **feature_names = (char **) calloc ((size_t)num_features, sizeof(char *));
  float *feature_weights = (float *) calloc ((size_t)num_features, sizeof(float));

  // Go through the file's lines one-by-one loading each feature
  char *string, *next;
  int file_contains_weights = -1;
  num_features = 0;
  while ( fgets ( line, max_line_length, fp ) ) {
    // Copy the word (ignoring white space) into the word array
    strcpy(token_string, line);
    string = token_string;
    next = strtok( string, " \r\t\n");
    if ( next == NULL ) {
      die ("Empty line in file '%s': Line %d\n", filename, num_features+1);
    }
    feature_names[num_features] = strdup(next);
    // See if the line also contains a weight
    next = strtok (NULL," \r\t\n");
    if ( next != NULL) {
      if ( file_contains_weights == 0 ) {
	die ("Inconsistent formatting in file '%s': %s\n", filename, line);
      }
      file_contains_weights = 1;
      feature_weights[num_features] = atof(next);
      if ( isnan(feature_weights[num_features]) )
	die ("Nan detected in file '%s': %s\n", filename, line);
      if ( isinf(feature_weights[num_features]) )
	die ("Inf detected in file '%s': %s\n", filename, line);
      if ( strtok(NULL," \r\t\n") ) {
	die ( "Bad line format in file '%s': %s\n", filename, line);
      }
    } else {
      if (file_contains_weights == 1) {
	die ("Inconsistent formatting in file '%s': %s\n", filename, line);
      }
      file_contains_weights = 0;
      feature_weights[num_features] = 1.0;
    }
    num_features++;
  }
  fclose(fp);
  free(line);
  free(token_string);

  feature_set->feature_names = feature_names;

  HASHTABLE *hash =  hdbmcreate( (unsigned)num_features, hash2);
  int i;
  for ( i=0; i<num_features; i++ ) {
    store_hashtable_string_index (hash, feature_names[i], i);
  }
  feature_set->feature_name_to_index_hash = hash;
  feature_set->feature_weights = feature_weights;
  
  return feature_set;
}

void free_feature_set ( FEATURE_SET *feature_set )
{
  int i;
  for (i=0; i<feature_set->num_features; i++ ) free(feature_set->feature_names[i]);
  free(feature_set->feature_names);
  free(feature_set->feature_weights);
  hdbmdestroy(feature_set->feature_name_to_index_hash);
  free(feature_set);
  return;
}


void save_feature_set ( FEATURE_SET *features, char *filename )
{
  FILE *fp;
  int i;
  
  fp = fopen_safe(filename, "w");

  for ( i=0; i<features->num_features; i++ ) {
    fprintf (fp, "%s %f \n", features->feature_names[i], features->feature_weights[i]);
  }

  fclose(fp);
}


void add_word_count_info_into_feature_set ( FEATURE_SET *features, FEATURE_SET *stop_list )
{
  int use_stop_list = 0;
  int i;  

  HASHTABLE *stop_list_hash = NULL;
  if ( stop_list != NULL && stop_list->feature_name_to_index_hash != NULL ) {
    stop_list_hash = stop_list->feature_name_to_index_hash;
    use_stop_list = 1;
  }

  features->num_words = (int *) calloc(features->num_features, sizeof(int));
  
  for ( i=0; i<features->num_features; i++ ) {
    char *feature_name = strdup(features->feature_names[i]);
    
    int content_word_count = 0;
    int total_word_count = 0;
    char *next = strtok ( feature_name, "_" );
    while ( next != NULL ) {
      total_word_count++;
      if ( ( ! use_stop_list ) || 
	   ( get_hashtable_string_index(stop_list_hash, next) == -1 ) ) {
	content_word_count++;
      }
      next = strtok ( NULL, "_" );
    }
    free(feature_name);
    
        
    features->num_words[i] = total_word_count;
    
  }

  return;

}


/*******************************************************************************************/

int *load_feature_vector_class_indices ( char *list_filename, CLASS_SET *class_set, int *count )
{

  // Check the feature_set and class_set args
  if ( class_set == NULL ) die ("Class set passed into load_feature_vector_class_indices is NULL\n");
  
  HASHTABLE *class_hash = class_set->class_name_to_class_index_hash;

  // Open the file list and count the lines and max line length
  FILE *fp = fopen_safe( list_filename, "r" );
  int num_vectors, max_line_length;
  num_vectors = count_lines_in_file (fp, &max_line_length);
  max_line_length += 2;
  if ( num_vectors == 0 ) die ("Specified file is empty: %s\n",list_filename);

  // Allocate the space for the array of class indices
  int *class_indices = (int *) calloc(num_vectors, sizeof(int));
  
  // Go through the filelist loading file and class labels
  char *line = (char *) calloc(max_line_length+3, sizeof(char));
  char **substrings;
  int num_substrings;
  int i;
  int vector_count = 0;
  int num_labels;
  float step_size = ((float)num_vectors/10);
  float next_step = step_size;
  int step = 1;
  while ( fgets ( line, max_line_length, fp ) ) {
    if ( ((float)vector_count) > next_step ) {
      printf("%d%%...",step*10); fflush(stdout);
      next_step += step_size;
      step++;
    }
    substrings = split_string( line, " \n\r\t", &num_substrings );
    if ( num_substrings < 1 ) 
      die ("Bad format in line %d of file '%s' \n", vector_count+1, list_filename);

    // Save the first class index associated with this feature vector
    num_labels = num_substrings - 1;
    if ( num_labels > 0 ) {
      class_indices[vector_count] = get_hashtable_string_index (class_hash, substrings[1]);

      if ( class_indices[num_vectors] == -1 ) {
	die ("Unknown class name '%s' for file '%s'\n", substrings[1], substrings[0]);
      }
      vector_count++;

    } else {
      die ("No class associated with for file '%s'\n", substrings[0]);
    }
    
    // Free the substrings and the substring array
    for ( i=0; i<num_substrings; i++ ) {
      if ( substrings[i] != NULL ) free(substrings[i]);
    }
    free(substrings);
    
  }
  fclose(fp);

  if ( line != NULL ) free(line);

  *count = num_vectors;

  return class_indices;

}

/*******************************************************************************************/

float *compute_MAP_estimated_distribution_with_uniform_prior ( float *counts, 
							       int num_features, 
							       float tau )
{
  float total_count=0;
  int i;
  float *P = (float *) calloc ((size_t)num_features, sizeof(float));

  for ( i=0; i<num_features; i++ ) {
    total_count += counts[i];
  }

  float denom = total_count + (((float)num_features)*tau);

  for ( i=0; i<num_features; i++ ) {
    P[i] = (counts[i] + tau) / denom;
  }
  
  return P;

}

/*******************************************************************************************/

float *compute_MAP_estimated_distribution ( float *counts, float *priors,
					    int num_features, float tau )
{
  float total_count=0;
  int i;
  float *P = (float *) calloc ((size_t)num_features, sizeof(float));

  for ( i=0; i<num_features; i++ ) {
    total_count += counts[i];
  }

  float denom = total_count + (((float)num_features)*tau);
  float lambda = total_count/denom;

  for ( i=0; i<num_features; i++ ) {
    P[i] = lambda * ( counts[i] / total_count );
    P[i] += (1-lambda) * priors[i];
    if ( P[i] == 0 ) die ("Zero probability for feature %d?!?\n",i);
  }
  
  return P;

}

/*******************************************************************************************/

void normalize_feature_weights (float *feature_weights, int num_features)
{
  int i;
  float total_weight=0;
  float norm_factor;
 
  for ( i=0; i<num_features; i++ ) {
    if ( feature_weights[i] < 0.0 ) total_weight += -feature_weights[i];
    else total_weight += feature_weights[i];
  }
  norm_factor = ((float)num_features)/total_weight;

  for ( i=0; i<num_features; i++ ) {
    feature_weights[i] = norm_factor * feature_weights[i];
  }
  
  return;

}

/*******************************************************************************************/

void prune_zero_weight_features_from_feature_vectors ( SPARSE_FEATURE_VECTORS *feature_vectors ) 
{
  
  // First find how many non-zero weighted features exisit in the feature set
  FEATURE_SET *features = feature_vectors->feature_set;
  float *old_feature_weights = features->feature_weights;
  int i, j, k;
  int old_num_features = features->num_features;
  int new_num_features = 0;
  for ( i=0; i<old_num_features; i++ ) 
    if ( features->feature_weights[i] > 0.0 ) 
      new_num_features++;

  // Create new feature set structures for non-zero weighted features only and
  // create a mapping from the old feature indices to the new features indices
  char **new_feature_names = (char **) calloc(new_num_features, sizeof(char *));
  float *new_feature_weights = (float *) calloc(new_num_features, sizeof(float));
  int *new_num_words = NULL; 
  if ( features->num_words != NULL ) 
    new_num_words = (int *) calloc( new_num_features, sizeof(int));

  HASHTABLE *new_hash = hdbmcreate( 1000, hash2);
  int *old_to_new_mapping = (int *) calloc(old_num_features, sizeof(int));
  for ( i=0, j=0; i<old_num_features; i++ ) {
    if ( old_feature_weights[i] > 0.0 ) {
      new_feature_names[j] = features->feature_names[i];
      new_feature_weights[j] = old_feature_weights[i];
      store_hashtable_string_index (new_hash, new_feature_names[j], j);
      if ( features->num_words != NULL ) new_num_words[j] = features->num_words[i];
      old_to_new_mapping[i] = j;
      j++;
    } else {
      old_to_new_mapping[i] = -1;
      free(features->feature_names[i]);
    }
  }

  // Free the old feature data structures
  free(features->feature_names);
  free(features->feature_weights);
  hdbmdestroy(features->feature_name_to_index_hash);  
  if ( features->num_words != NULL ) free( features->num_words );

  // Point to the new feature data structures
  features->num_features = new_num_features;
  features->feature_names = new_feature_names;
  features->feature_weights = new_feature_weights;
  features->feature_name_to_index_hash = new_hash;
  features->num_words = new_num_words;

  // Advance through the feature vectors themselves creating 
  // new feature vectors mapped to new feature set
  int index;
  SPARSE_FEATURE_VECTOR *vector;
  int non_zero_count;

  for ( i=0; i<feature_vectors->num_vectors; i++ ) {
    // Create new feature vectors for the non-zero weighted features
    vector = feature_vectors->vectors[i];
    non_zero_count = 0;
    for ( j=0; j<vector->num_features; j++ ) {
      if ( old_to_new_mapping[vector->feature_indices[j]] != -1 ) non_zero_count++;
    }    
    int *new_indices = (int *) calloc(non_zero_count,sizeof(int));
    float *new_values = (float *) calloc(non_zero_count,sizeof(float));

    // Set the total feature sum to zero for recounting
    vector->total_sum = 0;

    // Fill in the new feature vectors with mapped indices
    // and free the old feature vectors
    for ( j=0, k=0; j<vector->num_features; j++ ) {
      index = vector->feature_indices[j];
      if ( old_to_new_mapping[index] != -1 ) {
	new_indices[k] = old_to_new_mapping[index];
	new_values[k] = vector->feature_values[j];
	vector->total_sum += new_values[k];
	k++;
      }
    }

    free(vector->feature_indices);
    free(vector->feature_values);
    vector->feature_indices = new_indices;
    vector->feature_values = new_values;
    vector->num_features = non_zero_count;
  }

  return;

}

void remove_zero_weight_features ( FEATURE_SET *features )
{
  hdbmdestroy(features->feature_name_to_index_hash);
  HASHTABLE *hash = hdbmcreate( 1000, hash2);

  int i, j;
  int old_num_features = features->num_features;
  int new_num_features = 0;
  
  for ( i=0; i<old_num_features; i++ ) 
    if ( features->feature_weights[i] > 0.0 ) 
      new_num_features++;

  char **new_feature_names = (char **) calloc(new_num_features, sizeof(char *));
  float *new_feature_weights = (float *) calloc(new_num_features, sizeof(float));

  for ( i=0, j=0; i<old_num_features; i++ ) {
    if ( features->feature_weights[i] > 0.0 ) {
      new_feature_names[j] = features->feature_names[i];
      new_feature_weights[j] = features->feature_weights[i];
      store_hashtable_string_index (hash, new_feature_names[j], j);
      j++;
    } else {
      free(features->feature_names[i]);
    }
  }
  free(features->feature_names);
  free(features->feature_weights);

  features->num_features = new_num_features;
  features->feature_names = new_feature_names;
  features->feature_weights = new_feature_weights;
  features->feature_name_to_index_hash = hash;

  return;

}

/*******************************************************************************************/
