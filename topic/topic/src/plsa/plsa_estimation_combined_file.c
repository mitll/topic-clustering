/* -*- C -*-
 *
 * Copyright (c) 2010
 * MIT Lincoln Laboratory
 * Massachusetts Institute of Technology
 *
 * All Rights Reserved
 *
 * FILE: plsa_estimation_combined_file.c
 * Adapted from TJ Hazen's code to use a "combined" file -- a file with document counts per line.
 * BC: 5/09/13
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "util/basic_util.h"
#include "util/args_util.h"
#include "util/hash_util.h"
#include "classifiers/classifier_util.h"
#include "plsa/clustering_util.h"
#include "plsa/plsa.h"
#include "porter_stemmer/porter_stemmer.h"

/* Function prototypes */
char **create_labels_list ( SPARSE_FEATURE_VECTORS *feature_vectors, CLASS_SET *classes );
void evaluate_plsa_summary_against_reference ( PLSA_SUMMARY *summary, PLSA_SUMMARY *reference, int use_stemming );

void characterize_words_by_topical_importance ( PLSA_MODEL *plsa_model, FEATURE_SET *features, 
						float *feature_counts, char *file_out );
void create_jackknife_partitions ( SPARSE_FEATURE_VECTORS *feature_vectors, int num_sets );
PLSA_MODEL *construct_reference_plsa_model ( SPARSE_FEATURE_VECTORS *feature_vectors );

/* Main Program */
int main(int argc, char **argv)
{
  // Set up argument table
  ARG_TABLE *argtab = NULL;
  argtab = llspeech_new_string_arg(argtab, "vector_list_in", NULL,
				   "Input file containing a list of labeled feature vector files");
  argtab = llspeech_new_string_arg(argtab, "feature_list_in", NULL, 
				  "List of terms to use in feature set");
  argtab = llspeech_new_string_arg(argtab, "stop_list_in", NULL, 
				  "List of terms to exclude from feature set");
  argtab = llspeech_new_string_arg(argtab, "plsa_model_out", NULL,
				   "Output file containing PLSA topic unigram models");
  argtab = llspeech_new_string_arg(argtab, "feature_list_out", NULL, 
				   "Output file containing list of terms used in feature set");
  argtab = llspeech_new_string_arg(argtab, "ranked_words_out", NULL,
				   "Output file containing words ranked by topical importance");
  argtab = llspeech_new_float_arg(argtab, "df_cutoff", 0.5, 
				  "Exclude terms that happen in greater than this fraction of vectors");
  argtab = llspeech_new_float_arg(argtab, "tf_cutoff", 5.0, 
				  "Exclude terms that occur this number of times or fewer in the data");
  argtab = llspeech_new_float_arg(argtab, "alpha", 0.001, 
				  "Smoothing parameter for topic model P(z|d)");
  argtab = llspeech_new_float_arg(argtab, "beta", 0.001, 
				  "Smoothing parameter for word model P(w|z)");
  argtab = llspeech_new_int_arg(argtab, "num_topics", -1,
				"Number of latent PLSA topics");
  argtab = llspeech_new_int_arg(argtab, "max_iter", 500,
				"Maximum number of PLSA training iterations");
  argtab = llspeech_new_float_arg(argtab, "convergence", 0.001,
				"Average likelihood convergence threshhold");
  argtab = llspeech_new_flag_arg(argtab, "random", "Do a random seeding initialization of the PLSA topics");
  argtab = llspeech_new_flag_arg(argtab, "list_stemming", "Do Porter stemming to remove redundant signature words");
  argtab = llspeech_new_flag_arg(argtab, "jackknife", "Compute test likelihood on jackknifed partitions");
  argtab = llspeech_new_flag_arg(argtab, "summarize", "Generate a summary of the data from the PLSA model");
  argtab = llspeech_new_flag_arg(argtab, "reference", "Generate reference PLSA model from truth labels");
  argtab = llspeech_new_flag_arg(argtab, "eval_topics", "Evaluation PLSA topics angainst reference labels");
  

  /* Parse the command line arguments */ 
  argc = llspeech_args(argc, argv, argtab);

  // Extract command line argument settings
  char *vector_list_in = (char *) llspeech_get_string_arg(argtab, "vector_list_in");
  char *feature_list_in = (char *) llspeech_get_string_arg(argtab, "feature_list_in");
  char *stop_list_in = (char *) llspeech_get_string_arg(argtab, "stop_list_in");
  char *plsa_model_out = (char *) llspeech_get_string_arg(argtab, "plsa_model_out");
  char *feature_list_out = (char *) llspeech_get_string_arg(argtab, "feature_list_out");
  char *ranked_words_out = (char *) llspeech_get_string_arg(argtab, "ranked_words_out");
  float df_cutoff = llspeech_get_float_arg(argtab, "df_cutoff");
  float tf_cutoff = llspeech_get_float_arg(argtab, "tf_cutoff");
  float alpha = llspeech_get_float_arg(argtab, "alpha");
  float beta = llspeech_get_float_arg(argtab, "beta");
  int num_topics = llspeech_get_int_arg(argtab, "num_topics");
  int max_iter = llspeech_get_int_arg(argtab, "max_iter");
  float conv_threshold = llspeech_get_float_arg(argtab, "convergence");
  int random = llspeech_get_flag_arg(argtab, "random");
  int stem_list = llspeech_get_flag_arg(argtab, "list_stemming");
  int jackknife = llspeech_get_flag_arg(argtab, "jackknife");
  int summarize = llspeech_get_flag_arg(argtab, "summarize");
  int reference = llspeech_get_flag_arg(argtab, "reference");
  int eval_topics = llspeech_get_flag_arg(argtab, "eval_topics");

  time_t begin_time, start_time, end_time;

  if ( vector_list_in == NULL ) {
    fprintf ( stderr, "\nArgument list:\n");
    llspeech_args_prusage(argtab);
    die ( "Must specify argument -vector_list_in\n");
  }

  if ( alpha < 0 ) die ( "-alpha parameter cannot be negative\n");
  if ( beta < 0 ) die ( "-beta parameter cannot be negative\n");
  if ( max_iter < 0 ) die ( "-max_iter parameter must non-negative\n");
  if ( num_topics < 1 ) die ( "-num_topics parameters must be set to a positive value\n");

  time(&begin_time);

  FEATURE_SET *stop_list = NULL;
  if (stop_list_in != NULL) {
    printf ("(Loading stop list..."); fflush(stdout);
    stop_list = load_feature_set ( stop_list_in );
    printf("done)\n");
  }

  FEATURE_SET *features = NULL;
  if ( feature_list_in != NULL ) {
    printf ("(Loading feature list..."); fflush(stdout);
    features = load_feature_set ( feature_list_in );
    printf("done)\n");
  }

  // Load feature set from features observed in training data 
  if ( features == NULL ) {
    printf("(Creating feature set from training files..."); fflush(stdout);
    time(&start_time);
    features = create_feature_set_from_file (vector_list_in, 0, stop_list);
    time(&end_time);
    printf("done in %d seconds)\n",(int)difftime(end_time,start_time));
  }
  
  // Add some count info into the feature set about multiword units
  add_word_count_info_into_feature_set (features, stop_list);

  // Load list of classes
  CLASS_SET *classes = NULL;
  if (eval_topics) {
    classes = create_class_set_from_file_list (vector_list_in);
  }

  printf("classes is : %p\n", classes);
  
  // Load training set feature vectors
  printf("(Loading feature vectors..."); fflush(stdout);
  time(&start_time);
  SPARSE_FEATURE_VECTORS *feature_vectors = load_sparse_feature_vectors_combined (vector_list_in, features, classes);
  time(&end_time);
  printf("done in %d seconds)\n",(int)difftime(end_time,start_time));
  
  time(&end_time);
  printf ("(Total load time: %d seconds)\n",(int)difftime(end_time,begin_time));
  time(&begin_time);

  // Learn feature weights for features
  printf("(Learning feature weights..."); fflush(stdout);
  learn_feature_weights ( feature_vectors, df_cutoff, tf_cutoff, 0, IDF_WEIGHTING, 0 ); 
  printf("done)\n");

  // Prune features whose feature weight is zero 
  // from feature set and feature vectors
  printf("(Prune zero weight features..."); fflush(stdout);
  prune_zero_weight_features_from_feature_vectors(feature_vectors);
  printf("done)\n");

  // Save the pruned feature set to file if requested
  if ( feature_list_out != NULL ) {
    printf("(Writing feature set to file '%s'...",feature_list_out); fflush(stdout);
    save_feature_set ( features, feature_list_out );
    printf("done)\n");
  }

  // Compute initial assignments of vectors to clusters 
  int *vector_labels = NULL;
  if ( random ) {
    //vector_labels = random_clustering ( feature_vectors, num_topics );
    vector_labels = kmeans_clustering ( feature_vectors, num_topics, 20 );
  } else {
    printf("(Applying feature weights..."); fflush(stdout);
    apply_feature_weights_to_feature_vectors ( feature_vectors );
    printf ("done)\n");

    vector_labels = deterministic_clustering ( feature_vectors, num_topics );

    printf("(Remove zero weight features from feature set..."); fflush(stdout);
    remove_zero_weight_features ( features );
    printf ("done)\n");
    
    printf("(Reloading feature vectors..."); fflush(stdout);
    free_sparse_feature_vectors ( feature_vectors );
    feature_vectors = load_sparse_feature_vectors ( vector_list_in, features, classes );
    printf("done)\n");
  } 
  
  time(&end_time);
  printf ("(Total preprocessing time: %d seconds)\n",(int)difftime(end_time,begin_time));
  time(&begin_time);

  // Estimating the PLSA model
  PLSA_MODEL *plsa_model = train_plsa_model_from_labels ( feature_vectors, vector_labels, 
							  num_topics, alpha, beta, max_iter,
							  conv_threshold, 0 );

  time(&end_time);
  printf ("(Total training time: %d seconds)\n",(int)difftime(end_time,begin_time));


  // Print out evaluation metrics
  if ( eval_topics ) {
    PLSA_EVAL_METRICS *plsa_eval_metrics = compute_plsa_to_truth_metrics ( plsa_model );
    float H_T_Z = plsa_eval_metrics->H_T - plsa_eval_metrics->I;
    float H_Z_T = plsa_eval_metrics->H_Z - plsa_eval_metrics->I;
    float EIR = ( H_T_Z + H_Z_T ) / plsa_eval_metrics->H_T;
    printf("--- Model Evaluation ---\n");
    printf("H(T):        %.3f\n",plsa_eval_metrics->H_T);
    printf("H(Z):        %.3f\n",plsa_eval_metrics->H_Z);
    printf("I(Z;T):      %.3f\n",plsa_eval_metrics->I);
    printf("H(T|Z):      %.3f\n",H_T_Z);
    printf("H(Z|T):      %.3f\n",H_Z_T);
    printf("EIR(Z;T):    %.3f    = ( H(T|Z) + H(Z|T) ) / H(T)\n",EIR);
    printf("H-Precision: %.3f    = I(Z;T) / H(Z)\n",plsa_eval_metrics->Pzt);
    printf("H-Recall:    %.3f    = I(Z;T) / H(T)\n",plsa_eval_metrics->Ptz);
    printf("NMI(Z;T):    %.3f    = 2 * I(Z;T) / ( H(Z)+ H(T) ) {'F-score'}\n",plsa_eval_metrics->NMI);
    printf("Training likelihood: %.4f\n",plsa_model->avg_likelihood);
    printf("------------------------\n");
  }

  float jackknife_likelihood = 0;
  if ( jackknife ) {
    // Create jackknife partitions
    int num_partitions = 10;
    printf("Estimating test set likelihood with jackknife training over %d heldout partitions:\n",num_partitions);
    create_jackknife_partitions ( feature_vectors, num_partitions );
    int partition;
    float jackknife_words = 0;
    for ( partition = 0; partition<num_partitions; partition++ ) {
      PLSA_MODEL *partition_plsa_model = copy_plsa_model ( plsa_model );
      estimate_plsa_model ( partition_plsa_model, feature_vectors, alpha, beta, 10, 0.001, partition, 1 );
      jackknife_likelihood += partition_plsa_model->total_likelihood;
      jackknife_words += partition_plsa_model->total_words;
      free_plsa_model(partition_plsa_model);
    }
    jackknife_likelihood = jackknife_likelihood/jackknife_words;
    printf("Average heldout likelihood : %.4f\n",jackknife_likelihood);
    printf("--------------------------------------------------------------------\n");
  }

  PLSA_SUMMARY *plsa_summary = NULL;
  if ( summarize || ranked_words_out != NULL ) {
    plsa_summary = summarize_plsa_model ( plsa_model, stem_list );
    if ( summarize ) print_plsa_summary ( plsa_summary, eval_topics, NULL );  
    if ( ranked_words_out != NULL ) {
      float *feature_counts = extract_feature_counts_from_sparse_feature_vectors ( feature_vectors );
      characterize_words_by_topical_importance ( plsa_model, feature_vectors->feature_set, 
						 feature_counts, ranked_words_out );
    }
  }

  PLSA_MODEL *ref_plsa_model = NULL;
  PLSA_SUMMARY *ref_summary = NULL;
  if ( reference ) {
    ref_plsa_model = construct_reference_plsa_model (feature_vectors);
    ref_summary = summarize_plsa_model ( ref_plsa_model, stem_list );
    print_plsa_summary ( ref_summary, 1, NULL );  
  }

  if ( summarize && reference ) {
    evaluate_plsa_summary_against_reference ( plsa_summary, ref_summary, 1 );
  }

  if ( plsa_model_out != NULL ) {
    write_plsa_model_to_file ( plsa_model_out, plsa_model );
  }

  return 0;

}

char **create_labels_list ( SPARSE_FEATURE_VECTORS *feature_vectors, CLASS_SET *classes )
{
  int num_vectors = feature_vectors->num_vectors;
  char **class_names = classes->class_names;
  char **labels = (char **) calloc (num_vectors, sizeof(char *));

  int i, id;
  for ( i=0; i<num_vectors; i++) {
    id = feature_vectors->vectors[i]->class_id;
    labels[i] = strdup(class_names[id]);
  }

  return labels;
}

void characterize_words_by_topical_importance ( PLSA_MODEL *plsa_model, FEATURE_SET *features,
						float *feature_counts, char *file_out )
{
  int num_features = features->num_features;
  IV_PAIR **global_word_scores = plsa_model->global_word_scores;
  int i, w;
  float score;

  qsort(global_word_scores, num_features, sizeof(IV_PAIR *), cmp_iv_pair);

  if ( file_out != NULL ) {
    FILE *fp = fopen_safe(file_out, "w"); 
    for ( i=0; i<num_features; i++ ) {
      w = global_word_scores[i]->index;
      score = global_word_scores[i]->value;
      fprintf(fp, "%s %.8f %.3f\n",features->feature_names[w], score, feature_counts[w]);
    }
    fclose(fp);
  }
  
  printf("--------------------------------------------------------------------\n");

  printf("Top 500 Globally important topic words:\n");
  for ( i=0; i<500; i++ ) {
    w = global_word_scores[i]->index;
    score = global_word_scores[i]->value;
    printf("%3d score=%.6f count=%6.2f word=%s\n",i+1,score,
	   feature_counts[w],features->feature_names[w]);
  }

  printf("--------------------------------------------------------------------\n");


}

void create_jackknife_partitions ( SPARSE_FEATURE_VECTORS *feature_vectors, int num_sets )
{
  int num_vectors = feature_vectors->num_vectors;
  SPARSE_FEATURE_VECTOR **vectors = feature_vectors->vectors;
  
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

PLSA_MODEL *construct_reference_plsa_model ( SPARSE_FEATURE_VECTORS *feature_vectors )
{
  int num_topics = feature_vectors->class_set->num_classes;
  int num_vectors = feature_vectors->num_vectors;
  SPARSE_FEATURE_VECTOR **vectors = feature_vectors->vectors;
  int *vector_labels = (int *) calloc(num_vectors, sizeof(int));
  int i;

  for ( i=0; i<num_vectors; i++ ) {
    vector_labels[i] = vectors[i]->class_id;
  }
  
  PLSA_MODEL *ref_plsa_model = initialize_plsa_model ( feature_vectors, vector_labels, num_topics, 0, 0, 1 );
  
  return ref_plsa_model;
}

void evaluate_plsa_summary_against_reference ( PLSA_SUMMARY *summary, PLSA_SUMMARY *reference, int use_stemming )
{
  int i, w, z, v;
  char *stem;

  // Create hash table of stemmed summary words in summary
  int max_summary_features = summary->num_topics * summary->num_summary_features;
  HASHTABLE *summary_feature_hash = hdbmcreate( max_summary_features, hash2); 
  int summary_count = 0;
  int *summary_indices = (int *) calloc(max_summary_features, sizeof(int));
  for ( z=0; z<summary->num_topics; z++ ) {
    for ( i=0; i<summary->num_summary_features; i++ ) {
      w = summary->summary_features[z][i];
      if ( w >= 0 ) {
	stem = strdup(summary->features->feature_names[w]);
	if ( use_stemming ) porter_stem_string(stem);
	v = get_hashtable_string_index (summary_feature_hash, stem );
	if ( v == -1 ) {
	  store_hashtable_string_index (summary_feature_hash, stem, summary_count); 
	  summary_indices[summary_count] = w;
	  summary_count++;
	}
	free(stem);
      }
    }
  }
  printf("%d unique stems in summary\n",summary_count);
  
  // Create hash table of stemmed summary words in reference
  int max_reference_features = reference->num_topics * reference->num_summary_features;
  HASHTABLE *reference_feature_hash = hdbmcreate( max_reference_features, hash2); 
  int reference_count = 0;
  int *reference_indices = (int *) calloc(max_summary_features, sizeof(int));
  for ( z=0; z<reference->num_topics; z++ ) {
    for ( i=0; i<reference->num_summary_features; i++ ) {
      w = reference->summary_features[z][i];
      if ( w >= 0 ) {
	stem = strdup(reference->features->feature_names[w]);
	if ( use_stemming ) porter_stem_string(stem);
	v = get_hashtable_string_index (reference_feature_hash, stem );
	if ( v == -1 ) {
	  store_hashtable_string_index (reference_feature_hash, stem, reference_count); 
	  reference_indices[reference_count] = w;
	  reference_count++;
	}
	free(stem);
      }
    }
  }
  printf("%d unique stems in reference\n",reference_count);

  int summary_hits = 0;
  int summary_false_alarms = 0;
  for ( i=0; i<summary_count; i++ ) {
    w = summary_indices[i];
    stem = strdup(summary->features->feature_names[w]);
    if ( use_stemming ) porter_stem_string(stem);
    v = get_hashtable_string_index (reference_feature_hash, stem );
    if ( v == -1 ) {
      summary_false_alarms++;
      // printf("Summary FA: %s\n",summary->features->feature_names[w]);
    } else { 
      summary_hits++;
    }
  }
  float precision = ((float)summary_hits)/((float)summary_count);
  printf("Summary hits: %d\n",summary_hits);
  printf("Summary FAs : %d\n",summary_false_alarms);

  int reference_hits = 0;
  int reference_misses = 0;
  for ( i=0; i<reference_count; i++ ) {
    w = reference_indices[i];
    stem = strdup(summary->features->feature_names[w]);
    if ( use_stemming ) porter_stem_string(stem);
    v = get_hashtable_string_index (summary_feature_hash, stem );
    if ( v == -1 ) {
      reference_misses++;
      // printf("Reference Miss: %s\n", summary->features->feature_names[w]);
    } else { 
      reference_hits++;
    }
  }
  float recall = ((float)reference_hits)/((float)reference_count);
  printf("Reference hits  : %d\n",reference_hits);
  printf("Reference misses: %d\n",reference_misses);

  float fscore = 2*(precision*recall)/(precision+recall);

  float error_ratio = ((float)(summary_false_alarms+reference_misses))/((float)reference_count);

  printf("Summary precision  : %.3f\n",precision);
  printf("Summary recall     : %.3f\n",recall);
  printf("Summary F-score    : %.3f\n",fscore);
  printf("Summary error ratio: %.3f\n",error_ratio);


}
