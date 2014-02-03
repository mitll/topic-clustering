/* -*- C -*-
 *
 * Copyright (c) 1996, 2004, 2010, 2012
 * MIT Lincoln Laboratory
 * Massachusetts Institute of Technology
 *
 * All Rights Reserved
 *
 * FILE: clustering_util.c
 *
 * ORIGINAL AUTHOR: Timothy J. Hazen
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "util/basic_util.h"
#include "util/hash_util.h"
#include "classifiers/classifier_util.h"
#include "plsa/clustering_util.h"

/***************************************************************************************************/

typedef struct CLUSTERING_DATA {
  int num_elements;
  int *tree_node_clusters;
  int *active_node_to_tree_node_mapping;
  float **active_dist;
  float **original_dist;
  int *nearest_neighbor_index;
  float *nearest_neighbor_dist;
} CLUSTERING_DATA;

static void merge_clusters ( TREE_NODE **nodes_ptr, CLUSTERING_DATA *clust_data, int dist_metric,
			     int active_index_1, int active_index_2, int next_tree_index );
static void max_dist_update ( float **dist, int *mapping, int num_elements, int active_index_1, int active_index_2 );
static void min_dist_update ( float **dist, int *mapping, int num_elements, int active_index_1, int active_index_2 );
static void avg_dist_update ( CLUSTERING_DATA *clust_data, int active_index_1, int active_index_2 );
static void tot_dist_update ( CLUSTERING_DATA *clust_data, int active_index_1, int active_index_2 );
static void count_leaves_in_tree(TREE_NODE *node) ;
static void compute_tree_graphics_parameters(TREE_NODE *node, int leaves_to_left);
static int fill_in_node_heights(TREE_NODE *node, float *heights, int next, int max);
static void mark_nodes(TREE_NODE *node, float height );
static int find_and_label_cluster_nodes ( TREE_NODE *node, float cutoff_height, int current_label );
static void label_cluster_nodes (TREE_NODE *node, int cluster_label );
static void label_leaf_node (TREE_NODE *node, int cluster_label );
static void recursively_find_leaf_cluster_labels ( TREE_NODE *node, int *labels, int num_vectors);
static void free_all_strings_in_tree ( TREE_NODE *node );

/***************************************************************************************************/

SPARSE_FEATURE_VECTORS *create_normalized_feature_vectors ( SPARSE_FEATURE_VECTORS *feature_vectors,
							    float df_cutoff, float tf_cutoff, int smooth, 
							    int weighting, int root ) 
{

  SPARSE_FEATURE_VECTORS *new_feature_vectors = copy_sparse_feature_vectors ( feature_vectors );
  normalize_feature_vectors ( new_feature_vectors, df_cutoff, tf_cutoff, smooth, weighting, root );

  return new_feature_vectors;

}

void normalize_feature_vectors ( SPARSE_FEATURE_VECTORS *feature_vectors, float df_cutoff, 
				 float tf_cutoff, int smooth, int weighting, int root ) 
{

  learn_feature_weights ( feature_vectors, df_cutoff, tf_cutoff, smooth, weighting, root );
  apply_feature_weights_to_feature_vectors ( feature_vectors );

  return;

}

// This function learns TF-IDF or TF-LLR features weights from
// from the collection of feature vectors and stores these weights
// in the feature set.
// This function does not apply the weights to the feature vectors
void learn_feature_weights ( SPARSE_FEATURE_VECTORS *feature_vectors, float df_cutoff, 
			     float tf_cutoff, int smooth, int weighting, int root ) 
{

  // Initialize the feature weight array and estimate the IDF
  // counts from expected counts.
  FEATURE_SET *features = feature_vectors->feature_set;
  int num_features = features->num_features;
  int num_vectors = feature_vectors->num_vectors;
  int i, j;

  float *word_counts = (float *) calloc (num_features, sizeof(float));
  float *doc_counts = (float *) calloc (num_features, sizeof(float));

  float *weights = features->feature_weights;

  float value;
  int index;
  SPARSE_FEATURE_VECTOR *vector;
  
  // Count the total estimated count of each word over the corpus 
  // and the estimated number of documents each word appearances in
  for ( i=0; i<num_vectors; i++ ) {
    vector = feature_vectors->vectors[i];
    for ( j=0; j<vector->num_features; j++ ) {
      index = vector->feature_indices[j];
      value = vector->feature_values[j];
      if ( index != -1 ) {
	word_counts[index] += value;
	if ( value>1.0) value = 1.0;
	doc_counts[index] += value;
      }
    }
  }

  float total_count = 0;
  for ( i=0; i<num_features; i++ ) {
    total_count += word_counts[i];
  }

  float tf_scale = logf(.5)/tf_cutoff;
  float df_scale = logf(.5)/logf(df_cutoff);
  float df = 0;
  int df_ignore_count = 0;
  int tf_ignore_count = 0;

  // Set the normalizing weight to the expected IDF weight 
  // Use with a floor of 0.01 to avoid DF values of 0
  for ( i=0; i<num_features; i++ ) {
 
    // Apply a floor on estimated doc freq so IDF doesn't get too big
    if (doc_counts[i]<=0.01) doc_counts[i] = 0.01;
      
    // This is the estimated document frequency
    df = doc_counts[i]/((float)num_vectors);

    if ( weighting == LLR_WEIGHTING ) {
      // This is a likelihood ratio weighting
      weights[i] = -logf(word_counts[i]/total_count);
    } else if ( weighting == IDF_WEIGHTING ) {
      // This is the standard IDF weighting used by IR community
      weights[i] = -logf(df);
    } else {
      weights[i] = 1.0;
    }
    
    if ( !smooth ) {
      // Ignore high document frequency and low word frequency words
      if ( (df >= df_cutoff ) || ( word_counts[i] <= tf_cutoff ) ) {
	if ( df >= df_cutoff ) df_ignore_count++;
	if ( word_counts[i] <= tf_cutoff ) tf_ignore_count++;
	weights[i] = 0;
      }
    } else {
      if ( tf_cutoff > 0 ) {
	// Apply smooth cutoff deweighting to low frequency terms
	weights[i] = weights[i] * ( 1 - expf( tf_scale * word_counts[i] ));
      }
      
      if ( df_cutoff < 1.0 && df_cutoff > 0.0 ) {
	// Apply smooth cutoff deweighting to high document frequency terms
	weights[i] = weights[i] * ( 1 - expf( df_scale * logf ( df ) ) );
      }
    }      
    
    if (root ) {
      // Take square root so weight is effectively applied only 
      // once in cosine simularity measure and not twice
      weights[i] = sqrtf(weights[i]);
    }

  }  
  
  //if ( !smooth ) printf ("Ignoring %d high DF terms and %d low TF terms - Using %d terms...", 
  //			 df_ignore_count, tf_ignore_count, 
  //			 num_features-df_ignore_count-tf_ignore_count); fflush(stdout);
  

  free(word_counts);
  free(doc_counts);

  return;

}

// This function applies the feature weights in the feature set 
// directly to the vectors, so the weights don't need to be
// applied on the fly during later computations
void apply_feature_weights_to_feature_vectors ( SPARSE_FEATURE_VECTORS *feature_vectors ) 
{
  FEATURE_SET *features = feature_vectors->feature_set;
  float *weights = features->feature_weights;
  int i, j;
  
  // Advance through the data accumulating the counts for each feature
  float value;
  int index;
  SPARSE_FEATURE_VECTOR *vector;
  
  for ( i=0; i<feature_vectors->num_vectors; i++ ) {
    vector = feature_vectors->vectors[i];
    for ( j=0; j<vector->num_features; j++ ) {
      // Index is the word index into the vocabulary
      index = vector->feature_indices[j];
      // Value is the term frequency in the document
      value = vector->feature_values[j];
      if ( index != -1 ) {
	vector->feature_values[j] = value * weights[index];
      }
    }
  }

  return;

}

/**********************************************************************/

// If log_dist is set, similarity is converted to distance using -log()
float **compute_cosine_similarity_matrix ( SPARSE_FEATURE_VECTORS *feature_vectors, int log_dist, int verbose ) 
{

  // L2 norm training vectors
  if ( verbose ) {
    printf("(Computing cosine similarity matrix...normalizing..."); fflush(stdout);
  }
  apply_l2_norm_to_feature_vectors ( feature_vectors );
  
  int num_vectors = feature_vectors->num_vectors;
  float **matrix = (float **) calloc2d (num_vectors, num_vectors, sizeof(float));
  
  SPARSE_FEATURE_VECTOR *vector_i, *vector_j;
  int i, j;
  float step_size = ((float)(num_vectors*num_vectors)+num_vectors)/20.0;
  int step_count = 1;
  float loop_count = 0;
  float current_step = step_size;
  
  float min_sim = 1.0;
  if ( verbose ) {
    printf("computing..."); fflush(stdout);
  }
  for (i=0; i<num_vectors; i++ ) {

    vector_i = feature_vectors->vectors[i];

    for ( j=i; j<num_vectors; j++ ) {

      // print out progress in computing matrix
      if ( verbose ) {
	if ( loop_count > current_step ) {
	  printf("."); fflush(stdout);
	  step_count++;
	  current_step += step_size;
	}
	loop_count++;
      }

      vector_j = feature_vectors->vectors[j];

      // We compute the vector dot product assuming vectors have already been L2 normed
      matrix[i][j] = compute_sparse_vector_dot_product ( vector_i, vector_j );
      if ( matrix[i][j] > 1.0 ) matrix[i][j] = 1.0;
      else if ( matrix[i][j] > 0.0 && matrix[i][j] < min_sim ) min_sim = matrix[i][j];
      matrix[j][i] = matrix[i][j];
    }
  }

  if ( log_dist ) {
    if ( verbose ) {
      printf("converting to distances..."); fflush(stdout);
    }
    float max_dist = 1.25 * -logf(min_sim);
    for (i=0; i<num_vectors; i++ ) {
      for ( j=i; j<num_vectors; j++ ) {
	if ( matrix[i][j] == 0.0 ) matrix[i][j] = max_dist;	
	else matrix[i][j] = -logf(matrix[i][j]);
	matrix[j][i] = matrix[i][j];
      }
    }
  }
 

  if ( verbose ) printf("done)\n");

  return matrix;

  

}

// This function applies L2 normalization to a set of sparse feature vectors
void apply_l2_norm_to_feature_vectors ( SPARSE_FEATURE_VECTORS *feature_vectors ) 
{

  float value;
  int index;
  SPARSE_FEATURE_VECTOR *vector;
  
  int num_vectors = feature_vectors->num_vectors;
  
  int i, j;
  for ( i=0; i<num_vectors; i++ ) {
    vector = feature_vectors->vectors[i];
    float squared_sum = 0;
    for ( j=0; j< vector->num_features; j++ ) {
      index = vector->feature_indices[j];
      if ( index != -1 ) {
	value = vector->feature_values[j];
	squared_sum += value*value;
      }
    }
    float norm = sqrtf(squared_sum);
    for ( j=0; j< vector->num_features; j++ ) {
      index = vector->feature_indices[j];
      if ( index != -1 ) {
	vector->feature_values[j] = vector->feature_values[j] / norm;
      } else {
	vector->feature_values[j] = 0;
      }
    }
  }      
  
  return;

}

// This function computes a sparse vector dot product
float compute_sparse_vector_dot_product ( SPARSE_FEATURE_VECTOR *vector_i, 
					  SPARSE_FEATURE_VECTOR *vector_j ) 
{
  
  int i = 0;
  int j = 0;
  int index_i, index_j;

  float result = 0;
  
  while ( i < vector_i->num_features &&
	  j < vector_j->num_features ) {

    index_i = vector_i->feature_indices[i];
    index_j = vector_j->feature_indices[j];

    if ( index_i == index_j ) {
      result += vector_i->feature_values[i] * vector_j->feature_values[j];
      i++;
      j++;
    } else if ( index_i < index_j ) {
      i++;
    } else {
      j++;
    }

  }
 
  return result;

}

/*************************************************************************/

void create_tk_plotting_file( TREE_NODE *root, TREE_PLOT_PARAMETERS *param, FILE *fp )
{
  float ratio = 0.5;
  int max_height = 1000;
  int margin = 30;
  param->margin = margin;

  param->leaf_width = param->fontsize + 2;

  /* Figure out the size of canvas needed for the clustering plot */
  if ( param->rotate ) {
    /* Set plotting region */
    param->height = ( root->leaves - 1 ) * param->leaf_width ;
    param->width = param->height * ratio ;
    if ( param->width > max_height ) param->width = max_height;
    /* Figure out height scaling factor */
    param->scale = ((float)param->width)/root->height;
    /* Add a border to the plot region */
    param->width += (2*margin) + param->label_space;
    param->height += (2*margin);
  }
  else {
    /* Set plotting region */
    param->width = ( root->leaves - 1 ) * param->leaf_width ;
    param->height = param->width * ratio ;
    if ( param->height > max_height ) param->height = max_height;
    /* Figure out height scaling factor */
    param->scale = ((float)param->height)/root->height;
    /* Add a border to the plot region */
    param->width += (2*margin);
    param->height += (2*margin) + param->label_space;  
  }  
  

  /* Create the initial canvas with scrollbars and a quit button */
  fprintf ( fp, "#!/usr/bin/wish -f\n" );
  fprintf ( fp, "\n# Create a canvas which is %d pixels wide by %d pixels high\n", 
	    param->width, param->height );
  fprintf ( fp, "frame .c\n" );
  fprintf ( fp, "canvas .c.canvas ");
  if ( param->width > 1000 ) fprintf( fp, "-width 1000 ");
  else fprintf( fp, "-width %d ", param->width );
  if ( param->height > 800 ) fprintf( fp, "-height 800 ");
  else fprintf( fp, "-height %d ", param->height );
  fprintf( fp, "-scrollregion { 0 0 %d %d } ", param->width, param->height );
  if( param->width > 1000 ) fprintf ( fp, "-xscrollcommand [list .c.xscroll set] " );
  if( param->height > 800 ) fprintf ( fp, "-yscrollcommand [list .c.yscroll set] " );
  fprintf ( fp, "\n" );
  if( param->width > 1000 ) {
    fprintf ( fp, "scrollbar .c.xscroll -orient horizontal -command [list .c.canvas xview]\n" );
    fprintf ( fp, "pack .c.xscroll -side bottom -fill x\n" );
  }
  if( param->height > 800 ) {
    fprintf ( fp, "scrollbar .c.yscroll -orient vertical -command [list .c.canvas yview]\n" );
    fprintf ( fp, "pack .c.yscroll -side right -fill y\n" );
  }
  fprintf ( fp, "pack .c.canvas -side left -fill both -expand true\n" );
  fprintf ( fp, "pack .c -side top -fill both -expand true\n" );
  fprintf ( fp, "button .quit -text { Quit } -command { exit }\npack .quit\n\n" );
  fprintf ( fp, "# Draw the cluster tree\n");

  /* Plot the cluster tree on the canvas */
  create_tk_commands_for_node( root, param, fp );

  /* Create a postscript version of the plot */
  if ( param->ps_out != NULL ) {
    fprintf ( fp, "\n# Create a postscript version of the plot\n" );
    fprintf ( fp, "foreach size { 8 10 11 12 14 18 } {\n" );
    fprintf ( fp, "    set fontMap(techphonetic-$size) [list TechPhonetic $size]\n" );
    fprintf ( fp, "    set fontMap(-*-helvetica-medium-r-normal--$size-*)" );
    fprintf ( fp, " [list Helvetica $size]\n}\n" );
    fprintf ( fp, ".c.canvas postscript -file %s -fontmap fontMap -rotate %d ", 
	      param->ps_out, (!param->rotate) );
    fprintf ( fp, "-x 0 -y 0 -height %d -width %d\n", 
	      param->height, param->width );
  }

}

void create_tk_commands_for_node( TREE_NODE *node, TREE_PLOT_PARAMETERS *param, FILE *fp)
{
  int x1, x2, y1, y2, text_x, text_y, height;
  float scale = param->scale;
  int rotate = param->rotate;
  int margin = param->margin;

  /* If tree is rotated then tree height corresponds to plot width */
  if ( rotate ) height = param->width;
  else height = param->height;

  /* Plot a nonterminal node and recurse on its children */
  if ( node->leaves > 1 ) {
    if ( node->mark != -1 ) {
      x1 = margin + ((int)(rintf( (node->left_side - 1) * param->leaf_width )));
      x2 = margin + ((int)(rintf( (node->right_side - 1) * param->leaf_width )));
      y1 = height - margin - param->label_space - ((int)(rintf( scale * node->height)));
      if ( rotate ) 
	fprintf ( fp, ".c.canvas create line %d %d %d %d", y1, x1, y1, x2);
      else 
	fprintf ( fp, ".c.canvas create line %d %d %d %d", x1, y1, x2, y1);
      if ( node->mark ) fprintf ( fp, " -fill red");
      fprintf(fp,"\n");
      if ( param->label_nodes ) {
	text_y = y1+1;
	text_x = (x1+x2)/2;
	if (strcmp(node->label,"(no label)") != 0) { 
	  if ( rotate ) {
	    if ( node->leaves == 1 ) {
	      fprintf ( fp, ".c.canvas create text %d %d -text \"%s\" -anchor w -font %s\n", 
			text_y, text_x, node->label, param->font);
	    } else {
	      if ( strlen(node->label)*(param->fontsize/2) > text_y ) 
		text_y = strlen(node->label)*(param->fontsize/2); 
	      fprintf ( fp, ".c.canvas create text %d %d -text \"%s\" -anchor e -font %s\n", 
			text_y-1, text_x-(param->leaf_width/2), node->label, param->font);
	    }
	  } else {
	    if ( node->leaves == 1 ) {
	      fprintf ( fp, ".c.canvas create text %d %d -text \"%s\" -anchor n -font %s\n", 
			text_x, text_y, node->label, param->font);
	    } else {
	      if ( strlen(node->label)*(param->fontsize/2) > text_y ) 
		text_y = strlen(node->label)*(param->fontsize/2); 
	      fprintf ( fp, ".c.canvas create text %d %d -text \"%s\" -anchor s -font %s\n", 
			text_x-(param->leaf_width/2), text_y-1, node->label, param->font);
	    }
	  }
	}
      }
      y2 = height - margin - param->label_space - ((int)(rintf( scale * node->left_child->height)));
      if ( rotate ) 
	fprintf ( fp, ".c.canvas create line %d %d %d %d", y1, x1, y2, x1);
      else 
	fprintf ( fp, ".c.canvas create line %d %d %d %d", x1, y1, x1, y2);
      if ( node->mark ) fprintf (fp," -fill red");
      fprintf(fp,"\n");
      y2 = height - margin - param->label_space - ((int)(rintf( scale * node->right_child->height)));
      if ( rotate ) 
	fprintf ( fp, ".c.canvas create line %d %d %d %d", y1, x2, y2, x2);
      else 
	fprintf ( fp, ".c.canvas create line %d %d %d %d", x2, y1, x2, y2);
      if ( node->mark ) fprintf (fp," -fill red");
      fprintf(fp,"\n");
    }
    create_tk_commands_for_node( node->left_child, param, fp);
    create_tk_commands_for_node( node->right_child, param, fp);
  }

  /* Label a terminal node */
  else {
    text_x = margin + ((int)(rintf( (node->left_side-1) * param->leaf_width )));
    text_y = height - margin + 5 - param->label_space;
    if ( rotate ) 
      fprintf ( fp, ".c.canvas create text %d %d -text \"%s\" -anchor w -font %s\n", 
		text_y, text_x, node->label, param->font);
    else
      fprintf ( fp, ".c.canvas create text %d %d -text \"%s\" -anchor n -font %s\n", 
		text_x, text_y, node->label, param->font);
  }
}

int find_longest_label(TREE_NODE *node)
{
  int l, r;

  if( node->leaves > 1 ) {
    l = find_longest_label( node->left_child );
    r = find_longest_label( node->right_child );
    if ( l > r ) return l;
    else return r;
  }
  else return strlen(node->label);

}

void find_cluster_labels(TREE_NODE *node, int node_id, int child,
			 char ***list_ptr, int *n_ptr, char ***ptr )
{
  if( node->node_index == node_id ) {
    child = 1;
    *n_ptr = node->leaves;
    *list_ptr = (char **) calloc (node->leaves, sizeof(char *));
    *ptr = *list_ptr;
  }

  if(node->leaves > 1) {
    find_cluster_labels(node->left_child, node_id, child, list_ptr, n_ptr, ptr);
    find_cluster_labels(node->right_child, node_id, child, list_ptr, n_ptr, ptr);
  }
  else if ( child == 1 ) {
    **ptr = strdup(node->label);
    (*ptr)++;
  }

}

/*************************************************************************/

LDA_FEATURE_VECTORS *load_lda_feature_vectors ( char *lda_vectors_in )
{
  
  LDA_FEATURE_VECTORS *lda_feature_vectors;
  lda_feature_vectors = (LDA_FEATURE_VECTORS *) malloc ( sizeof(LDA_FEATURE_VECTORS) );

  FILE *fp = fopen_safe( lda_vectors_in, "r" );
  int max_line_length;
  int num_vectors = count_lines_in_file (fp, &max_line_length);
  max_line_length++;
  
  int vector_count = 0;
  int topic_count = 0;
  int num_topics = 0;
  float **vectors = NULL;
  char *line = (char *)calloc(max_line_length+1,sizeof(char));
  char *next;
  // First count the number of topics and allocate space for feature vectors
  if ( fgets ( line, max_line_length, fp ) ) {
    next = strtok(line, " \t\n");
    if ( next == NULL ) die ( "Empty line (Line: 0) in file '%s'\n", lda_vectors_in);
    while ( next != NULL ) {
      num_topics++;
      next = strtok( NULL, " \t\n");
    }
    vectors = (float **) calloc2d ( num_vectors, num_topics, sizeof(float));
  } else {
    die ( "Empty file '%s'\n", lda_vectors_in );
  }

  // Now fill in the feature vector values
  rewind(fp);
  vector_count = 0;
  while ( fgets ( line, max_line_length, fp ) ) {
    topic_count = 0;
    next = strtok(line, " \t\n");
    while ( next != NULL ) {
      if ( topic_count >= num_topics ) 
	die ( "Differing numbers of topics in lines 1 and %d of file '%s'\n", 
	      vector_count+1, lda_vectors_in );
      vectors[vector_count][topic_count] = atof(next);
      topic_count++;
      next = strtok(NULL, " \t\n");
    }
    vector_count++;
  }
  fclose(fp);

  int i, j;
  float vector_sum;
  for ( i=0; i<num_vectors; i++ ) {
    vector_sum = 0;
    for ( j=0; j<num_topics; j++ ) {
      vector_sum += vectors[i][j];
    }
    for ( j=0; j<num_topics; j++ ) {
      vectors[i][j] = vectors[i][j]/vector_sum;
    }
  }

  lda_feature_vectors->num_vectors = num_vectors;
  lda_feature_vectors->num_topics = num_topics;
  lda_feature_vectors->vectors = vectors;

  return lda_feature_vectors;

}

/**********************************************************************/

float **compute_topic_prob_similarity_matrix ( LDA_FEATURE_VECTORS *feature_vectors ) 
{
  
  float **vectors = feature_vectors->vectors;
  int num_vectors = feature_vectors->num_vectors;
  int num_topics = feature_vectors->num_topics;
  float **matrix = (float **) calloc2d (num_vectors, num_vectors, sizeof(float));

  int i, j, k;
  for (i=0; i<num_vectors; i++ ) {
    for ( j=i; j<num_vectors; j++ ) {
      for ( k=0; k<num_topics; k++ ) {
	matrix[i][j] += vectors[i][k] * vectors[j][k] ;
      }
      matrix[j][i] = matrix[i][j];
    }
  }

  return matrix;
}

/**********************************************************************/

float **compute_lda_cosine_similarity_matrix ( LDA_FEATURE_VECTORS *feature_vectors ) 
{
  
  float **vectors = feature_vectors->vectors;
  int num_vectors = feature_vectors->num_vectors;
  int num_topics = feature_vectors->num_topics;
  float **matrix = (float **) calloc2d (num_vectors, num_vectors, sizeof(float));

  int i, j, k;
  // Apply l2 norm to feature vectors
  for ( i=0; i<num_vectors; i++ ) {
    float squared_sum = 0;
    for ( j=0; j<num_topics; j++ ) squared_sum += vectors[i][j]*vectors[i][j];
    float norm = sqrtf(squared_sum);
    for ( j=0; j<num_topics; j++ ) vectors[i][j] = vectors[i][j] / norm;
  }      
  
  for (i=0; i<num_vectors; i++ ) {
    for ( j=i; j<num_vectors; j++ ) {
      for ( k=0; k<num_topics; k++ ) {
	matrix[i][j] += vectors[i][k] * vectors[j][k] ;
      }
      matrix[j][i] = matrix[i][j];
    }
  }

  return matrix;
}

/**********************************************************************/

float **compute_kl_divergence_matrix ( LDA_FEATURE_VECTORS *feature_vectors ) 
{
  
  float **vectors = feature_vectors->vectors;
  int num_vectors = feature_vectors->num_vectors;
  int num_topics = feature_vectors->num_topics;
  float **matrix = (float **) calloc2d (num_vectors, num_vectors, sizeof(float));

  int i, j, k;
  for (i=0; i<num_vectors; i++ ) {
    for ( j=i; j<num_vectors; j++ ) {
      for ( k=0; k<num_topics; k++ ) {
	matrix[i][j] += 0.5*(vectors[i][k] * logf(vectors[i][k]/vectors[j][k]));
	matrix[i][j] += 0.5*(vectors[j][k] * logf(vectors[j][k]/vectors[i][k]));
      }
      matrix[j][i] = matrix[i][j];
    }
  }

  return matrix;
}

/**********************************************************************/

void convert_similarity_matrix_to_distance_matrix ( float **matrix, int num_dims, float ceiling )
{
  int i, j;

  float min = 1.0;
  for ( i=0; i<num_dims-1; i++ ) {
    for ( j=i; j<num_dims; j++ ) {
      if ( matrix[i][j] != matrix[j][i] ) die ("Distance matrix is not symmetric?!?\n");
      if ( matrix[i][j] > 1.0 ) die ("Similarity matrix has value greater than 1 (%f)!?!\n", matrix[i][j]);
      if ( matrix[i][j] < 0.0 ) die ("Similarity matrix has value less than 0 (%f)!?!\n", matrix[i][j]);
      if ( matrix[i][j] < min && matrix[i][j] != 0.0 ) min = matrix[i][j];
    }
  }

  for ( i=0; i<num_dims; i++ ) {
    for ( j=i; j<num_dims; j++ ) {
      if ( matrix[i][j] == 0.0 ) matrix[i][j] = ceiling;
      else { 
	matrix[i][j] = -logf ( matrix[i][j] );
	if ( matrix[i][j] > ceiling ) matrix[i][j] = ceiling;
      }
      matrix[j][i] = matrix[i][j];
    } 
  }
  
  return;

}
 
/**********************************************************************/

void add_in_similarity_matrix ( float **full_matrix, float **matrix, int num_dims )
{
  int i, j;

  for ( i=0; i<num_dims; i++ ) {
    for ( j=0; j<num_dims; j++ ) {
      full_matrix[i][j] += matrix[i][j];
    }
  }
  
  return;

}

float **interpolate_similarity_matrices ( float **matrix1, float **matrix2, int num_dims, float weight1 )
{
  int i, j;

  if ( ( weight1 < 0 ) ||
       ( weight1 > 1 ) )
    die ("interpolate_matrices: interpolation weight must be between 0 and 1\n");

  float **new_matrix = (float **) calloc2d ( num_dims, num_dims, sizeof(float));

  float weight2 = 1.0 - weight1;
  for ( i=0; i<num_dims; i++ ) {
    new_matrix[i][i] += (weight1 * matrix1[i][i] ) + (weight2 * matrix2[i][i]);
    for ( j=i+1; j<num_dims; j++ ) {
      new_matrix[i][j] += (weight1 * matrix1[i][j] ) + (weight2 * matrix2[i][j]);
      new_matrix[j][i] = new_matrix[i][j];
    }
  }
  
  return new_matrix;

}

/**********************************************************************/



/**************************************************************************************

 The routine bottom_up_cluster takes 5 arguments:
          matrix:  a symmetric distance matrix (of size num_elements
                   by num_elements) containing the distances between 
		   each of the elements that are to be clustered.
    num_elements:  the number of elements to be clustered.
          labels:  an array of pointers to labels for each of the elements.
                   if labels is NULL, the feature vector indices will be used as labels
     dist_metric:  type of distance metric to use for deciding which
                   cluster to merge. The possibilities are:
                       MIN_DIST - The smallest minimum distance between elements
                                  from each of two different clusters.
                       AVG_DIST - The smallest average distance between all elements 
		                  from each of two clusters.
                       MAX_DIST - The smallest maximum distance between elements 
                                  from each of two different clusters.
		       TOT_DIST - The smallest increase in total distortion contributed
                                  by merging two different clusters

 The routine returns a pointer to the root node in the resulting hierarchical				  
 structure. The structure TREE_NODE is used to represent each node.

**************************************************************************************/
                          
TREE_NODE *bottom_up_cluster( float **matrix, int num_elements, 
			      char **labels, int dist_metric )
{
  int i, j;
  int active_index_1, active_index_2;
  float min_dist = 0;
  int min_index;
  char label[100];

  // These arrays keep track of current clustering state
  CLUSTERING_DATA clust_data;
  clust_data.num_elements = num_elements;
  clust_data.tree_node_clusters = (int *) calloc ( num_elements, sizeof(int));
  clust_data.active_node_to_tree_node_mapping = (int *) calloc ( num_elements, sizeof(int));
  clust_data.active_dist = (float **) calloc2d (num_elements, num_elements, sizeof(float));
  clust_data.nearest_neighbor_index = (int *) calloc ( num_elements, sizeof(int));
  clust_data.nearest_neighbor_dist = (float *) calloc ( num_elements, sizeof(float));
  clust_data.original_dist = matrix;

  // The initial element wise distance matrix is copied into the working distance matrix
  for ( i=0; i<num_elements; i++ ) {
    memcpy (clust_data.active_dist[i], matrix[i], num_elements*sizeof(float));
  }

  // Tree has a leaf for every element to be clustered
  int num_leaves = num_elements; 

  // This is the number of nodes in a binary tree with num_leaves leaves
  int num_nodes = ( 2 * num_leaves ) - 1 ;

  // This allocates the tree structure for storing
  // the learned hierachical clustering 
  TREE_NODE *nodes = (TREE_NODE *) calloc ( num_nodes , sizeof(TREE_NODE));

  // Initialize the tree's leaf nodes and initial clustering structure
  for( i=0; i<num_leaves; i++) {

    // Initialize the state of the active clustering structure
    clust_data.active_node_to_tree_node_mapping[i] = i;
    clust_data.tree_node_clusters[i] = i;

    // Initialize the leaf nodes of the clustering tree
    nodes[i].height = 0;
    nodes[i].node_index = i;
    nodes[i].cluster_index = -1;
    if ( labels == NULL ) {
      sprintf(label, "%d", i);
      nodes[i].label = strdup(label);
    } else {
      nodes[i].label = strdup(labels[i]);
    }
    nodes[i].left_child = NULL;
    nodes[i].right_child = NULL;
    nodes[i].mark = 0;

    // Find the nearest neighbor for each leaf node
    min_index = -1;
    min_dist = -1;
    for (j=0; j<num_leaves;j++ ) {
      if ( j != i ) {
	if ( min_index == -1 || clust_data.active_dist[i][j] < min_dist ) {
	  min_index = j;
	  min_dist = clust_data.active_dist[i][j];
	}
      }
    }
    clust_data.nearest_neighbor_index[i] = min_index;
    clust_data.nearest_neighbor_dist[i] = min_dist;
  }
  
  // Initialize remaining tree nodes
  for( i=num_leaves ; i<num_nodes; i++ ) {
    nodes[i].label = (char *) calloc ( 12 , sizeof(char) );
    nodes[i].mark = 0;
    strcpy ( nodes[i].label , "(no label)" );
  }
  
  // This is the cluster index for the cluster to be formed
  int next_cluster = num_leaves;
  
  // Okay....let's do the bottom up clustering now...
  printf("(Clustering..."); fflush(stdout);


  float step_size = ((float)num_leaves)/10.0;
  int step_count = 1;
  float current_step = step_size;
  for( i=num_leaves; i>1; i-- ) {
     
    // Print out incremental progress
    if ( ((float)(num_leaves-i))>current_step ) {
      printf("%d%%...",10*step_count); fflush(stdout);
      step_count++;
      current_step += step_size;
    }

    // Find find the next 2 clusters to be merged
    min_index = -1;
    min_dist = -1.0;
    for ( j=0; j<num_elements; j++ ) {
      if ( clust_data.nearest_neighbor_index[j] != -1 ) {
	if ( min_index == -1 ) { 
	  min_index = j;
	  min_dist = clust_data.nearest_neighbor_dist[j];
	}
	else if ( clust_data.nearest_neighbor_dist[j] < min_dist ) {
	  min_index = j;
	  min_dist = clust_data.nearest_neighbor_dist[j];
	}
      }
    }

    // These are the indices of the clusters in the active 
    // cluster structure to be merged
    active_index_1 = min_index;
    active_index_2 = clust_data.nearest_neighbor_index[min_index];

    merge_clusters ( &nodes, &clust_data, dist_metric, active_index_1, active_index_2, next_cluster );
    
    next_cluster++;
  }
  printf("done)\n");

  /* Specify the root of the tree */
  next_cluster--;
  TREE_NODE *root = &nodes[next_cluster];
  
  /* Give the root node the pointer to the node 
     array so it can be deallocated later */
  root->array_ptr = &nodes[0];

  /* Count leaves in each sub tree */
  count_leaves_in_tree(root);

  /* Compute tree graphing parameters */
  compute_tree_graphics_parameters(root, 0);

  /* Return a pointer to the root node of the tree */
  return root;

}

/************************************************************************/

static void merge_clusters ( TREE_NODE **nodes_ptr, CLUSTERING_DATA *clust_data, int dist_metric,
			     int active_index_1, int active_index_2, int next_tree_index )
{
  TREE_NODE *nodes = *nodes_ptr;
  
  // These are the corresponding indices of the nodes of
  // the clusters in the cluster tree structure
  int tree_index_1 = clust_data->active_node_to_tree_node_mapping[active_index_1];
  int tree_index_2 = clust_data->active_node_to_tree_node_mapping[active_index_2];

  // Fill in info for newly created node
  nodes[next_tree_index].height = clust_data->active_dist[active_index_1][active_index_2];
  if ( dist_metric == TOT_DIST) nodes[next_tree_index].height = logf(1+nodes[next_tree_index].height);
  nodes[next_tree_index].node_index = next_tree_index;
  nodes[next_tree_index].right_child = &nodes[tree_index_1];
  nodes[next_tree_index].left_child = &nodes[tree_index_2];
 
  // Update array of tree node indices for each element
  int i, j;
  for(i=0; i<clust_data->num_elements; i++) 
    if( (clust_data->tree_node_clusters[i] == tree_index_1) || 
	(clust_data->tree_node_clusters[i] == tree_index_2) ) 
      clust_data->tree_node_clusters[i] = next_tree_index;

  clust_data->active_node_to_tree_node_mapping[active_index_1] = next_tree_index;
  clust_data->active_node_to_tree_node_mapping[active_index_2] = -1;
  
  // Update the active distance matrix
  float **dist = clust_data->active_dist;
  int *mapping = clust_data->active_node_to_tree_node_mapping;
  int num_elements = clust_data->num_elements;
  if ( dist_metric == MAX_DIST ) {
    max_dist_update ( dist, mapping, num_elements, active_index_1, active_index_2 );
  } else if ( dist_metric == MIN_DIST ) {
    min_dist_update ( dist, mapping, num_elements, active_index_1, active_index_2 );
  } else if ( dist_metric == AVG_DIST ) {
    avg_dist_update ( clust_data, active_index_1, active_index_2 );
  } else if ( dist_metric == TOT_DIST ) {
    tot_dist_update ( clust_data, active_index_1, active_index_2 );
  } else {
    die ("Not implemented yet!");
  }

  // Update the nearest neighbor info
  int *nn_index = clust_data->nearest_neighbor_index;
  float *nn_dist = clust_data->nearest_neighbor_dist;

  nn_index[active_index_2] = -1;
  nn_dist[active_index_2] = -1;
  for ( i=0; i<num_elements; i++ ) {
    // Redo the nearest neighbor info for the merged cluster and any
    // any cluster whose nearest neighbor was one of the merged clusters
    if ( i == active_index_1 || nn_index[i] == active_index_1 || nn_index[i] == active_index_2 ) {
      nn_index[i] = -1;
      nn_dist[i] = -1;
      for ( j=0; j<num_elements; j++ ) {
	if ( j != i && mapping[j] != -1 ) {
	  if ( nn_index[i] == -1 || dist[i][j] < nn_dist[i] ) {
	    nn_index[i] = j;
	    nn_dist[i] = dist[i][j];
	  }
	}
      }
    }
  }
    
  return;

}


/************************************************************************/

static void max_dist_update ( float **dist, int *mapping, int num_elements, 
			      int active_index_1, int active_index_2 ) 
{
  int i;

  // Update the distance matrix and bookkeeping variables
  // First let's compute the distances for the new cluster
  // We will use active_index_1 as the active index for the
  // new cluster

  dist[active_index_1][active_index_1] = dist[active_index_1][active_index_2];
  dist[active_index_1][active_index_2] = -1;
  dist[active_index_2][active_index_1] = -1;
  dist[active_index_2][active_index_2] = -1;
  
  for ( i=0; i<num_elements; i++ ) {

    // Update the distance matrix
    if ( i != active_index_1 && i != active_index_2 && mapping[i] != -1 ) {
      // Update the new cluster's distance to this element
      if ( dist[i][active_index_2] > dist[i][active_index_1] ) {
	dist[i][active_index_1] = dist[i][active_index_2];
	dist[active_index_1][i] = dist[active_index_2][i];
      }
      // With merging, the second cluster's active row & column are no longer needed
      dist[i][active_index_2] = -1;
      dist[active_index_2][i] = -1;
    } 
  } 
  
  return;

}


/************************************************************************/

static void min_dist_update ( float **dist, int *mapping, int num_elements, int active_index_1, int active_index_2 ) 
{
  int i;

  // Update the distance matrix and bookkeeping variables
  // First let's compute the distances for the new cluster
  // We will use active_index_1 as the active index for the
  // new cluster

  dist[active_index_1][active_index_1] = dist[active_index_1][active_index_2];
  dist[active_index_1][active_index_2] = -1;
  dist[active_index_2][active_index_1] = -1;
  dist[active_index_2][active_index_2] = -1;
  
  for ( i=0; i<num_elements; i++ ) {

    // Update the distance matrix
    if ( i != active_index_1 && i != active_index_2 && mapping[i] != -1 ) {
      // Update the new cluster's distance to this element
      if ( dist[i][active_index_2] < dist[i][active_index_1] ) {
	dist[i][active_index_1] = dist[i][active_index_2];
	dist[active_index_1][i] = dist[active_index_2][i];
      }
      // With merging, the second cluster's active row & column are no longer needed
      dist[i][active_index_2] = -1;
      dist[active_index_2][i] = -1;
    } 
  } 
  
  return;

}

/************************************************************************/

static void avg_dist_update ( CLUSTERING_DATA *clust_data, int active_index_1, int active_index_2 )
{

  int num_elements = clust_data->num_elements;
  float **active_dist = clust_data->active_dist;
  float **original_dist = clust_data->original_dist;
  int *active_node_to_tree_node_mapping = clust_data->active_node_to_tree_node_mapping;
  
  int new_tree_index = active_node_to_tree_node_mapping[active_index_1];
  int *leaf_to_tree_node_mapping = clust_data->tree_node_clusters;

  // Update the distance matrix and bookkeeping variables
  // We will use active_index_1 as the active index for the
  // new cluster

  active_dist[active_index_1][active_index_1] = active_dist[active_index_1][active_index_2];
  active_dist[active_index_1][active_index_2] = -1;
  active_dist[active_index_2][active_index_1] = -1;
  active_dist[active_index_2][active_index_2] = -1;
  
  int tree_index;
  int index_j, index_k;
  int i, j, k;
  for ( i=0; i<num_elements; i++ ) {
    // Update the distance matrix
    if ( i != active_index_1 && i != active_index_2 && active_node_to_tree_node_mapping[i] != -1 ) {

      // Update the new cluster's distance to the current active cluster i 
      tree_index = active_node_to_tree_node_mapping[i];

      // Compute the average distortion of merging this tree 
      // node with the newly created tree node
      float distance = 0;
      float dist_count = 0;
      for ( j=0; j<num_elements; j++ ) {
	index_j = leaf_to_tree_node_mapping[j];
	if ( index_j == tree_index || index_j == new_tree_index ) {
	  for ( k=j+1; k<num_elements; k++ ) {
	    index_k = leaf_to_tree_node_mapping[k];
	    if ( index_k == tree_index  || index_k == new_tree_index ) {
	      distance += original_dist[j][k];
	      dist_count += 1.0;
	    }
	  }
	}
      }
      active_dist[i][active_index_1] = distance/dist_count;
      active_dist[active_index_1][i] = active_dist[i][active_index_1];
      active_dist[i][active_index_2] = -1;
      active_dist[active_index_2][i] = -1;
    } 
  } 
  
  return;

}

/************************************************************************/

static void tot_dist_update ( CLUSTERING_DATA *clust_data, int active_index_1, int active_index_2 )
{

  int num_elements = clust_data->num_elements;
  float **active_dist = clust_data->active_dist;
  float **original_dist = clust_data->original_dist;
  int *active_node_to_tree_node_mapping = clust_data->active_node_to_tree_node_mapping;
  
  int new_tree_index = active_node_to_tree_node_mapping[active_index_1];
  int *leaf_to_tree_node_mapping = clust_data->tree_node_clusters;

  // Update the distance matrix and bookkeeping variables
  // We will use active_index_1 as the active index for the
  // new cluster

  active_dist[active_index_1][active_index_1] = active_dist[active_index_1][active_index_2];
  active_dist[active_index_1][active_index_2] = -1;
  active_dist[active_index_2][active_index_1] = -1;
  active_dist[active_index_2][active_index_2] = -1;
  
  int tree_index;
  int index_j, index_k;
  int i, j, k;
  for ( i=0; i<num_elements; i++ ) {
    // Update the distance matrix
    if ( i != active_index_1 && i != active_index_2 && active_node_to_tree_node_mapping[i] != -1 ) {

      // Update the new cluster's distance to the current active cluster i 
      tree_index = active_node_to_tree_node_mapping[i];

      // Compute the average distortion of merging this tree 
      // node with the newly created tree node
      float distance = 0;
      for ( j=0; j<num_elements; j++ ) {
	index_j = leaf_to_tree_node_mapping[j];
	if ( index_j == tree_index || index_j == new_tree_index ) {
	  for ( k=j+1; k<num_elements; k++ ) {
	    index_k = leaf_to_tree_node_mapping[k];
	    if ( index_k == tree_index  || index_k == new_tree_index ) {
	      distance += original_dist[j][k];
	    }
	  }
	}
      }
      active_dist[i][active_index_1] = distance;
      active_dist[active_index_1][i] = distance;
      active_dist[i][active_index_2] = -1;
      active_dist[active_index_2][i] = -1;
    } 
  } 
  
  return;

}


/************************************************************************/

void print_cluster_tree(TREE_NODE *node) 
{
  if( node->leaves > 1 ) {
    if ( node->mark == 1 ) printf ("+Node " );
    else printf( "-Node " );
    printf( "%d", node->node_index );
    if ( strcmp(node->label,"(no label)") ) 
      printf ( "(%s)", node->label );
    printf (" splits into nodes ");
    printf ( "%d", node->left_child->node_index );
    if ( strcmp(node->left_child->label,"(no label)") ) 
      printf ( "(%s)", node->left_child->label );
    printf (" and ");
    printf ( "%d", node->right_child->node_index );
    if ( strcmp(node->right_child->label,"(no label)") ) 
      printf ( "(%s)", node->right_child->label );
    printf ("  (Dist=%f)", node->height);
    printf ( "\n" );
    print_cluster_tree(node->left_child);
    print_cluster_tree(node->right_child);
  }
}

/*****************************************************************************/

static void count_leaves_in_tree(TREE_NODE *node) 
{

  /* If node has no children it is a leaf */
  if ((node->left_child == NULL) && (node->right_child == NULL)) node->leaves = 1;

  /* If node has 2 children then count the leaves of its children */
  else if ((node->left_child != NULL) && (node->right_child != NULL)) {
    count_leaves_in_tree(node->left_child);
    count_leaves_in_tree(node->right_child);
    node->leaves = node->left_child->leaves + node->right_child->leaves;
  }

  /* If node has only one child it is an illegal node - not full branching! */
  else 
    die ( "count_leaves_in_tree: Illegal tree structure - tree is not full branching" );

}

/*****************************************************************************/

void mark_top_clusters_in_tree(TREE_NODE *node, int num_to_mark) 
{
  
  if ( num_to_mark < 2 ) return;
  if ( num_to_mark > node->leaves - 1 ) num_to_mark = node->leaves-1;
  float *heights = create_sorted_list_of_node_heights ( node ); 
  mark_nodes ( node, heights[num_to_mark-1] );
  free(heights);
  return;
  
}

/*****************************************************************************/

int label_clusters_in_tree(TREE_NODE *node, int num_to_label ) 
{
  if ( node == NULL ) return 0;

  if ( num_to_label < 2 ) return 0;
  if ( num_to_label > node->leaves ) num_to_label = node->leaves;
  float *heights = create_sorted_list_of_node_heights ( node ); 

  while ( num_to_label < node->leaves && heights[num_to_label-2] == heights[num_to_label-1] )
    num_to_label++;

  find_and_label_cluster_nodes ( node, heights[num_to_label-1], 1 );
  free(heights);
  return num_to_label;

}

/*****************************************************************************/

int *assign_vector_labels_from_cluster_tree ( TREE_NODE *cluster_tree, int num_vectors )
{
  int *labels = (int *) calloc (num_vectors, sizeof(int));
  int i;
  for ( i=0; i<num_vectors; i++ ) labels[i] = -1;
  
  recursively_find_leaf_cluster_labels ( cluster_tree, labels, num_vectors);

  return labels;
}

static void recursively_find_leaf_cluster_labels ( TREE_NODE *node, int *labels, int num_vectors ) 
{

  if ( node->node_index < num_vectors && node->leaves == 1 ) {
    if ( node->cluster_index > 0 ) labels[node->node_index] = node->cluster_index - 1;
    else die("Invalid cluster index (%d) for node (%d) leaves:%d mark:%d height:%f\n",
	     node->cluster_index,node->node_index,node->leaves,node->mark,node->height);
    return;
  }

  if ( node->left_child != NULL ) 
    recursively_find_leaf_cluster_labels ( node->left_child, labels, num_vectors );
  if ( node->right_child != NULL ) 
    recursively_find_leaf_cluster_labels ( node->right_child, labels, num_vectors );

  return;
}

/*****************************************************************************/

static void mark_nodes(TREE_NODE *node, float height )
{
  if ( node == NULL ) return;

  if ( node->height > height ) {
    node->mark = 1;
    mark_nodes (node->left_child, height);
    mark_nodes (node->right_child, height);
  }

  return;

}

/*****************************************************************************/

static int find_and_label_cluster_nodes ( TREE_NODE *node, float cutoff_height, int current_label )
{

  if ( node == NULL ) return current_label;

  if ( node->height > cutoff_height ) {

    node->mark = 1;

    if ( node->left_child->height <= cutoff_height ) {
      label_cluster_nodes ( node->left_child, current_label );
      current_label++;
    }  else { 
      current_label = find_and_label_cluster_nodes (node->left_child, cutoff_height, current_label );
    }

    if ( node->right_child->height <= cutoff_height ) {
      label_cluster_nodes ( node->right_child, current_label );
      current_label++;
    }  else { 
      current_label = find_and_label_cluster_nodes (node->right_child, cutoff_height, current_label );
    }
  }

  return current_label;

}

static void label_cluster_nodes (TREE_NODE *node, int cluster_label )
{
  if ( node == NULL ) return;

  char *label=NULL;
  if ( node->leaves > 1 ) {
    label = (char *) calloc(10, sizeof(char));
    sprintf (label, "%d", cluster_label);
    node->label = label;
    label_cluster_nodes ( node->left_child, cluster_label );
    label_cluster_nodes ( node->right_child, cluster_label );
  } else {
    label_leaf_node ( node, cluster_label );
  }

  node->cluster_index = cluster_label;
  
  return;

}

static void label_leaf_node (TREE_NODE *node, int cluster_label ) 
{
  char *label=NULL;

  if ( node == NULL ) return;
  else if ( node->leaves <= 0 ) return;
  else if ( node->leaves > 1 ) label_cluster_nodes (node, cluster_label); 
  else {
    if ( node->label != NULL ) {
      label = (char *) calloc(13+strlen(node->label), sizeof(char));
      sprintf (label, "(%d)_%s", cluster_label, node->label);
      free(node->label);
    } else {
      label = (char *) calloc(10, sizeof(char));
      sprintf (label, "%d", cluster_label);
    }
    node->label = label;
  } 
  
  node->cluster_index = cluster_label;

  return;

}


/*****************************************************************************/

static void compute_tree_graphics_parameters(TREE_NODE *node, int leaves_to_left) 
{  
  if(node->leaves == 1) {
    node->left_side = leaves_to_left + 1;
    node->right_side = leaves_to_left + 1;
  }
  else {
    compute_tree_graphics_parameters(node->left_child, leaves_to_left);
    compute_tree_graphics_parameters(node->right_child, leaves_to_left + node->left_child->leaves);
    node->left_side = 0.5 * (node->left_child->left_side + node->left_child->right_side);
    node->right_side = 0.5 * (node->right_child->left_side + node->right_child->right_side);
  }

}


/*****************************************************************************/

/* The following commands are for saving and loading cluster trees */

void save_cluster_trees( TREE_NODE **nodes, int num_trees, FILE *fp )
{
  int i;

  dump_int ( num_trees , fp );
  for ( i=0; i<num_trees; i++ ) {
    save_cluster_tree ( nodes[i] , fp );
  }

}

void save_cluster_tree( TREE_NODE *node, FILE *fp )
{
  dump_string( node->label, fp );
  dump_int( node->node_index, fp );
  dump_float( node->height, fp );
  dump_float( node->left_side, fp );
  dump_float( node->right_side, fp );
  dump_int( node->leaves, fp );
  if ( node->leaves>1 ) {
    save_cluster_tree( node->left_child , fp );
    save_cluster_tree( node->right_child , fp );
  }
}

TREE_NODE **load_cluster_trees( int *num_trees, FILE *fp )
{
  int i;
  TREE_NODE **nodes;

  *num_trees = load_int( fp );
  nodes = (TREE_NODE **) calloc( *num_trees, sizeof(TREE_NODE *));
  for ( i=0; i<*num_trees; i++ ) {
    nodes[i] = load_cluster_tree ( fp );
  }
  return nodes;

}

TREE_NODE *load_cluster_tree( FILE *fp )
{
  TREE_NODE *node = (TREE_NODE *) malloc ( sizeof(TREE_NODE));

  node->label = load_string( fp );
  node->node_index = load_int( fp );
  node->height = load_float( fp );
  node->left_side = load_float( fp );
  node->right_side = load_float( fp );
  node->leaves = load_int( fp );
  if ( node->leaves > 1 ) {
    node->left_child = load_cluster_tree( fp );
    node->right_child = load_cluster_tree( fp );
  }
  return node;
}

/*****************************************************************************/

/* The following two commands are for save and loading distance matrices */

void save_distance_matrix ( float **matrix, char **labels, int num_elements, FILE *fp )
{
  dump_2d_float_array ( matrix, num_elements, num_elements, fp );
  dump_strings ( labels, num_elements, fp );
}

float **load_distance_matrix ( int *dims_ptr, char ***labels_ptr, FILE *fp)
{
  int dim1, dim2;
  
  float **matrix = load_2d_float_array ( &dim1, &dim2, fp );
  if ( dim1 != dim2 ) 
    die( "load_distance_matrix: Distance matrix is not square\n" );
  *dims_ptr = dim1;
  *labels_ptr = load_strings ( &dim1, fp );
  if (dim1 != *dims_ptr) 
    die ( "load_distance_matrix: List of labels (%d) not same size as matrix (%d)",dim1, dim2 );

  return matrix;
}

/*****************************************************************************/

IV_PAIR_ARRAY *create_iv_pair_array ( int num )
{
  IV_PAIR **pairs = (IV_PAIR **) calloc (num, sizeof(IV_PAIR *));
  IV_PAIR *data = (IV_PAIR *) malloc( num * sizeof(IV_PAIR) );
  int i;
  for ( i=0; i<num; i++ ) {
    pairs[i] = &(data[i]);
    pairs[i]->index = i;
    pairs[i]->value = 0.0;
  }

  IV_PAIR_ARRAY *array = (IV_PAIR_ARRAY *) malloc(sizeof(IV_PAIR_ARRAY));
  array->num_pairs = num;
  array->pairs = pairs;
  array->data = data;

  return array;
}

void free_iv_pair_array ( IV_PAIR_ARRAY *array )
{

  if ( array == NULL ) return;

  if ( array->data != NULL ) free(array->data);
  if ( array->pairs != NULL ) free(array->pairs);
  free(array);

  return;

}
/*****************************************************************************/

// This function compares index/value pairs by value from biggest to smallest
void sort_iv_pair_array ( IV_PAIR_ARRAY *array ) 
{
  qsort(array->pairs, array->num_pairs, sizeof(IV_PAIR *), cmp_iv_pair);
  return;
}


// This function compares index/value pairs by value 
// When used with qsort it sorts biggest value to
// start of array and smallest value to end of array
int cmp_iv_pair ( const void *p1, const void *p2 ) 
{
  IV_PAIR *iv1 = *(IV_PAIR **)p1;
  IV_PAIR *iv2 = *(IV_PAIR **)p2;
  if ( iv1->value < iv2->value ) return 1;
  else if ( iv2->value < iv1->value ) return -1;
  return 0;
}

/*****************************************************************************/


float *create_sorted_list_of_node_heights ( TREE_NODE *node ) 
{
  int num_nodes = node->leaves - 1;
  float *heights = (float *) calloc ( num_nodes, sizeof(float) );
  int count = fill_in_node_heights ( node, heights, 0, num_nodes );
  if ( count != num_nodes ) 
    printf ("Huh?!? count (%d) != num_nodes (%d)\n", count, num_nodes );
  sort_float_array(heights, num_nodes, 1);

  return heights;

}

static int fill_in_node_heights(TREE_NODE *node, float *heights, int next, int max ) 
{
  if ( node == NULL || node->leaves <= 1 ) return next;
  if ( next >= max ) die ("fill_in_node_heights: node count (%d) exceeds pre-specified limit (%d)\n",next+1,max);

  heights[next] = node->height;
  next++;

  next = fill_in_node_heights (node->left_child, heights, next, max);
  next = fill_in_node_heights (node->right_child, heights, next, max);

  return next;
}


/***************************************************************************************************/

void score_clusters_in_tree ( TREE_NODE *node ) 
{
  if ( node == NULL ) return;

  if ( node->leaves == 1 ) {
    node->score = 0;
    return;
  }

  //node->score = logf((float)node->leaves)*expf(-node->height);
  node->score = logf((float)node->leaves)*(1-node->height);
  score_clusters_in_tree ( node->left_child );
  score_clusters_in_tree ( node->right_child );

  return;

}

void mark_best_scoring_clusters_in_tree ( TREE_NODE *node ) 
{

  if ( node == NULL ) return;

  if ( node->leaves == 1 ) return;

  if ( node->height > 0.9 ) {
    if (node->label != NULL) free(node->label);
    node->label = strdup("(no label)");
    node->mark = 0;
    mark_best_scoring_clusters_in_tree(node->left_child);
    mark_best_scoring_clusters_in_tree(node->right_child);
    return;
  }

  if ( ( node->score > node->left_child->score ) && 
       ( node->score > node->right_child->score ) ) {
    clear_non_terminal_labels ( node->left_child );
    clear_non_terminal_labels ( node->right_child );
    mark_all_nodes_in_tree ( node );
    return;
  }

  node->mark = 0;
  if (node->label != NULL) free(node->label);
  node->label = strdup("(no label)");
  mark_best_scoring_clusters_in_tree(node->left_child);
  mark_best_scoring_clusters_in_tree(node->right_child);
  
  return;

}


/***************************************************************************************************/


void scale_tree_heights ( TREE_NODE *node, float scale ) 
{
  if ( node == NULL ) return;
  node->height = node->height * scale;

  if ( node->leaves >  1 ) {
    scale_tree_heights ( node->left_child, scale);
    scale_tree_heights ( node->right_child, scale);
  }

  return;
}

void clear_non_terminal_labels ( TREE_NODE *node ) 
{
  if ( node == NULL ) return;
  if ( node->leaves == 1 ) return;
  
  if ( node->label != NULL ) free(node->label);
  node->label = strdup("(no label)");
  clear_non_terminal_labels ( node->left_child );
  clear_non_terminal_labels ( node->right_child );

  return;
}

void mark_all_nodes_in_tree ( TREE_NODE *node ) 
{
  if ( node == NULL ) return;
  node->mark = 1;
  if ( node->leaves == 1 ) return;
  mark_all_nodes_in_tree ( node->left_child );
  mark_all_nodes_in_tree ( node->right_child );
  return;
}

void free_cluster_tree ( TREE_NODE *node ) 
{
  if ( node == NULL ) return;
  free_all_strings_in_tree ( node );
  if ( node->array_ptr != NULL ) free(node->array_ptr);
  return;
}

static void free_all_strings_in_tree ( TREE_NODE *node ) 
{
  if ( node == NULL ) return;
  if ( node->label != NULL ) free(node->label);
  if ( node->left_child != NULL ) free_all_strings_in_tree (  node->left_child );
  if ( node->right_child != NULL ) free_all_strings_in_tree (  node->right_child );
  return;
}


/***************************************************************************************************/


// Assign feature vectors to initial clusters using random initialization
// of cluster centroid with clusters formed by assigning feature vectors 
// to nearest centroid
int *kmeans_clustering ( SPARSE_FEATURE_VECTORS *feature_vectors, int num_clusters, int max_iter )
{
  int i, j, k;

  printf ("(Doing randomized kmeans clustering of feature vectors..."); fflush(stdout);

  // Checking for missing information
  if ( feature_vectors->feature_set == NULL ) 
    die ("No feature set specified for feature vectors\n");

  if ( feature_vectors->feature_set->feature_weights == NULL ) 
    die ("No feature weights specified for feature vectors\n");

  // Compute the feature vector L2 norm factors...we store these
  // so we don't have to alter the feature vectors themselves
  int num_vectors = feature_vectors->num_vectors;
  int num_features = feature_vectors->feature_set->num_features;
  SPARSE_FEATURE_VECTOR *vector;  
  int *indices;
  float *values;
  float *weights = feature_vectors->feature_set->feature_weights;
  float *vector_l2_norms = (float *)calloc(num_vectors, sizeof(float));
  float squared_sum = 0;
  float norm = 1;
  float weighted_value;
  for ( i=0; i<num_vectors; i++ ) {
    vector = feature_vectors->vectors[i];
    indices = vector->feature_indices;
    values = vector->feature_values;
    squared_sum = 0;
    for ( j=0; j<vector->num_features; j++  ) {
      weighted_value = values[j] * weights[indices[j]];
      squared_sum += weighted_value;
    }
    vector_l2_norms[i] = sqrtf(squared_sum);
  }

  // Set up centroid vectors...to simplify things these vectors 
  // are full vectors not sparse vectors
  int *vector_labels = (int *) calloc(num_vectors, sizeof(int));
  float **centroids = (float **)calloc2d(num_clusters,num_features,sizeof(float));
  size_t centroids_size = (size_t)(num_clusters*num_features*sizeof(float));
  int *centroid_counts = (int *)calloc(num_clusters,sizeof(float));
  float *centroid_min_similarity = (float *)calloc(num_clusters, sizeof(float));

  // Randomly select feature vectors to serve as cluster centroids
  int *seed_map = (int *) calloc(num_vectors, sizeof(int));
  srand((unsigned)(time(0)));
  for ( i=0; i<num_vectors; i++ ) {
    seed_map[i] = i;
  }
  for ( i=0; i<num_clusters; i++ ) {
    j = (int) (num_vectors * (rand() / (RAND_MAX + 1.0)));
    if ( j==num_clusters) j--;
    k = seed_map[i];
    seed_map[i] = seed_map[j];
    seed_map[j] = k;
  }

  // Copy the randomly selected vectors into the centroid vectors
  for ( i=0; i<num_clusters; i++ ) {
    vector = feature_vectors->vectors[seed_map[i]];
    indices = vector->feature_indices;
    values = vector->feature_values;
    for ( j=0; j<vector->num_features; j++ ) {
      centroids[i][indices[j]] = values[j];
    }
  }
  free(seed_map);

  int iter;
  int stop = 0;

  float *centroid;
  int best_cluster;
  float similarity;
  float max_similarity;
  int swap_count;

  // Start k-means iterations
  for ( iter=0; stop != 1  && iter<max_iter; iter++ ) {

    // On the first iteration the cluster centroids are pre-initialized
    // but after that we must compute them from the previous round of
    // vector to centroid assignments
    if ( iter > 0 ) {
      // Zero out the centroid vectors
      memset(centroids[0], 0, centroids_size);
      for ( i=0; i<num_vectors; i++ ) {
	centroid = centroids[vector_labels[i]]; // This is the current centroid assignment
	vector = feature_vectors->vectors[i];
	indices = vector->feature_indices;
	values = vector->feature_values;
	for ( j=0; j<vector->num_features; j++  ) {
	  centroid[indices[j]] += values[j];
	}
      }
    } 

    // Apply feature weighting to centroids
    for ( i=0; i<num_clusters; i++ ) {
      for ( j=0; j<num_features; j++ ) {
	centroids[i][j] = centroids[i][j] * weights[j];
      }
    }

    // Apply L2 normalization to feature centroids
    for ( i=0; i<num_clusters; i++ ) {
      squared_sum = 0;
      for ( j=0; j<num_features; j++ ) squared_sum += centroids[i][j] * centroids[i][j];
      norm = sqrtf(squared_sum);
      for ( j=0; j<num_features; j++ ) centroids[i][j] = centroids[i][j] / norm;
    }

    // Apply the feature weights to centroids again. These would normally 
    // be applied on the feature vector side before L2 normalization. 
    // However, so we don't alter the feature vectors themselves we shift
    // this weighting to the centroids and we apply feature vector L2 norm
    // after the fact
    for ( i=0; i<num_clusters; i++ ) {
      for ( j=0; j<num_features; j++ ) {
	centroids[i][j] = centroids[i][j] * weights[j];
      }
    }

    // Reset the centroid statistics vectors
    memset(centroid_counts, 0, num_clusters*sizeof(float));
    for ( i=0; i<num_clusters; i++ ) centroid_min_similarity[i] = 1.0;

    // Find the best matching cluster for each feature vector
    max_similarity=0;
    swap_count=0;
    
    for ( i=0; i<num_vectors; i++ ) {
      vector = feature_vectors->vectors[i];
      indices = vector->feature_indices;
      values = vector->feature_values;
      best_cluster = -1;
      max_similarity = 0;

      // Find the best matching cluster centroid for the current vector
      for ( j=0; j<num_clusters; j++ ) {
 
	// Compute the cosine similarity between vector i and cluster centroid [j].
	// Vector weights for both the centroid and the vectors are built into
	// the centroid. The L2 norm of centroid is also built into centroid vector.
	// The L2 norm of each vector is applied hear after the vector dot product.
 	centroid = centroids[j];
	similarity = 0;
	for ( k=0; k<vector->num_features; k++ ) {
	  similarity += values[k] * centroid[indices[k]];
	}
	similarity = similarity/vector_l2_norms[i];

	// Keep track of the best cluster for this vector
	if ( best_cluster == -1 || similarity > max_similarity ) {
	  best_cluster = j;
	  max_similarity = similarity;
	}
      }

      if ( max_similarity < centroid_min_similarity[best_cluster] ) 
	centroid_min_similarity[best_cluster] = max_similarity;

      centroid_counts[best_cluster]++;

      // If the current best cluster is different from the
      // previous one, then perform and count the swap
      if ( best_cluster != vector_labels[i] ) {
	vector_labels[i] = best_cluster;
	swap_count++;
      }
    }
    
    printf ("%d...",swap_count);

    if ( swap_count == 0 ) stop = 1;

  }
   
						   
  free2d((char **)centroids);
  printf("done)\n");
   
  //for ( i=0; i<num_clusters; i++ ) 
  //  printf ("[%3d] %d %.3f\n", i, centroid_counts[i],centroid_min_similarity[i]);

  return vector_labels;
    
}


