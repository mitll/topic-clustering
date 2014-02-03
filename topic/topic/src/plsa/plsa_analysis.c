/* -*- C -*-
 *
 * Copyright (c) 2011
 * MIT Lincoln Laboratory
 * Massachusetts Institute of Technology
 *
 * All Rights Reserved
 *
 * FILE: plsa_analysis.c
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
void load_feature_vector_info_into_plsa_model ( PLSA_MODEL *plsa_model, char *vector_list_in );
void evaluate_plsa_summary_against_reference ( PLSA_SUMMARY *summary, PLSA_SUMMARY *reference, int use_stemming );

void characterize_words_by_topical_importance ( PLSA_MODEL *plsa_model, FEATURE_SET *features, char *file_out );
float *extract_class_probs ( SPARSE_FEATURE_VECTORS *feature_vectors, CLASS_SET *classes );
void create_jackknife_partitions ( SPARSE_FEATURE_VECTORS *feature_vectors, int num_sets );
void find_best_story_for_topic_and_class ( PLSA_MODEL *plsa_model, int z,
					   SPARSE_FEATURE_VECTORS *feature_vectors, char *class_label );
int **find_best_stories_map ( PLSA_MODEL *plsa_model, int *class_indices, int num_classes );

char **create_latent_topic_labels_list ( PLSA_SUMMARY *summary, int summary_size );
void plot_topic_cluster_tree ( TREE_NODE *cluster_tree );
float **compute_topic_bhattacharyya_distance_matrix ( PLSA_MODEL *plsa_model );
float **compute_topic_inner_product_distance_matrix ( PLSA_MODEL *plsa_model );
float **compute_topic_intersection_distance_matrix ( PLSA_MODEL *plsa_model );
float **compute_topic_chebyshev_distance_matrix ( PLSA_MODEL *plsa_model );
float **compute_topic_soergel_distance_matrix ( PLSA_MODEL *plsa_model );
float **compute_topic_kulczynski_distance_matrix ( PLSA_MODEL *plsa_model );
int **compute_ranking_matrix_from_distance_matrix(float **topic_dist_matrix, int dim );

void write_z2t_counts_to_file ( PLSA_MODEL *plsa_model, char *filename );
void write_doc_to_topic_map_to_file ( PLSA_MODEL *plsa_model, char *filename ); 
void write_plsa_lm_matrix_to_file ( PLSA_MODEL *plsa_model, char *filename );
void write_d2z_counts_to_file ( PLSA_MODEL *plsa_model, char *filename );

/* Main Program */
int main(int argc, char **argv)
{

  // Set up argument table
  ARG_TABLE *argtab = NULL;
  argtab = llspeech_new_string_arg(argtab, "plsa_model_in", NULL,
				   "Output file containing PLSA topic unigram models");
  argtab = llspeech_new_string_arg(argtab, "vector_list_in", NULL,
				   "Input file containing a list of labeled feature vector files");
  argtab = llspeech_new_string_arg(argtab, "d2z_out", NULL,
				   "Output matrix of probability of topics per document");
  argtab = llspeech_new_string_arg(argtab, "z2t_out", NULL,
				   "Output matrix of words per latent topic mapped to each true topic");
  argtab = llspeech_new_string_arg(argtab, "lm_out", NULL,
				   "Output matrix of latent topic languauge models P(w|z)");
  argtab = llspeech_new_string_arg(argtab, "zdist_out", NULL,
				   "Output matrix of latent topic distance matrix");
  argtab = llspeech_new_string_arg(argtab, "zrank_out", NULL,
				   "Output matrix of latent topic distance rankings");
  argtab = llspeech_new_string_arg(argtab, "doc_map_out", NULL,
				   "Output file mapping best document for each class/latent topic pair");
  argtab = llspeech_new_string_arg(argtab, "summary_out", NULL,
				   "Output file with PLSA model summary");
  argtab = llspeech_new_string_arg(argtab, "ranked_words_out", NULL,
				   "Output file with words ranked by topical importance");
  argtab = llspeech_new_flag_arg(argtab, "cluster_topics", 
				 "Generate hierarchically clustered topic tree");
  argtab = llspeech_new_flag_arg(argtab, "eval_topics", 
				 "Evaluate latent topics against true topics");
  argtab = llspeech_new_flag_arg(argtab, "summarize", 
				 "Print summary of PLSA model to screen");

    
  // Parse the command line arguments 
  argc = llspeech_args(argc, argv, argtab);

  // Extract command line argument settings
  char *vector_list_in = (char *) llspeech_get_string_arg(argtab, "vector_list_in");
  char *plsa_model_in = (char *) llspeech_get_string_arg(argtab, "plsa_model_in");
  char *d2z_out = (char *) llspeech_get_string_arg(argtab, "d2z_out");
  char *z2t_out = (char *) llspeech_get_string_arg(argtab, "z2t_out");
  char *zdist_out = (char *) llspeech_get_string_arg(argtab, "zdist_out");
  char *zrank_out = (char *) llspeech_get_string_arg(argtab, "zrank_out");
  char *lm_out = (char *) llspeech_get_string_arg(argtab, "lm_out");
  char *doc_map_out = (char *) llspeech_get_string_arg(argtab, "doc_map_out");
  char *summary_out = (char *) llspeech_get_string_arg(argtab, "summary_out");
  char *ranked_words_out = (char *) llspeech_get_string_arg(argtab, "ranked_words_out");
  int cluster_topics = (int) llspeech_get_flag_arg(argtab, "cluster_topics");
  int eval_topics = (int) llspeech_get_flag_arg(argtab, "eval_topics");
  int summarize = (int) llspeech_get_flag_arg(argtab, "summarize");

  // Check if all arguments are specified properly
  if ( plsa_model_in == NULL ) {
    fprintf ( stderr, "\nArgument list:\n");
    llspeech_args_prusage(argtab);
    die ( "Must specify argument -plsa_model_in\n");
  }

  if ( ( z2t_out != NULL || doc_map_out != NULL || eval_topics == 1 ) 
       && vector_list_in == NULL ) {
    fprintf ( stderr, "\nArgument list:\n");
    llspeech_args_prusage(argtab);
    if ( z2t_out != NULL ) die ( "Must specify argument -vector_list_in with -z2t_out\n");
    else if ( doc_map_out != NULL ) die ( "Must specify argument -vector_list_in with -doc_map_out\n");
    else die ( "Must specify argument -vector_list_in with -eval_topics\n");
  }

  // Load the PLSA model
  printf("(Loading PLSA model..."); fflush(stdout);
  PLSA_MODEL *plsa_model = load_plsa_model_from_file(plsa_model_in);
  printf ("model contains %d words, %d topics, and %d documents...", 
	  plsa_model->num_features, plsa_model->num_topics, plsa_model->num_documents);
  printf("done)\n");

  // Load the true class info about the data into the PLSA model
  if ( vector_list_in != NULL ) {
    printf("(Loading feature vector info..."); fflush(stdout);
    load_feature_vector_info_into_plsa_model ( plsa_model, vector_list_in );
    printf("done)\n");
  }

  // Summarize the model
  printf("(Summarizing PLSA model..."); fflush(stdout);
  PLSA_SUMMARY *plsa_summary = summarize_plsa_model ( plsa_model, 1 );
  int *z_mapping = plsa_model->z_mapping;
  printf("done)\n");
  
  if ( summary_out ) {
    printf("(Writing summary to file..."); fflush(stdout); 
    print_plsa_summary ( plsa_summary, eval_topics, summary_out );
    printf("done)\n");
  } 

  // Write requested output files
  write_plsa_lm_matrix_to_file ( plsa_model, lm_out );
  write_z2t_counts_to_file ( plsa_model, z2t_out);
  write_doc_to_topic_map_to_file ( plsa_model, doc_map_out );
  write_d2z_counts_to_file ( plsa_model, d2z_out ); 

  // Compute distance matrix between topics
  float **topic_dist_matrix = NULL;
  if ( zrank_out != NULL || 
       zdist_out != NULL ||
       cluster_topics) {
    topic_dist_matrix = compute_topic_kulczynski_distance_matrix(plsa_model);
    //topic_dist_matrix = compute_topic_soergel_distance_matrix(plsa_model);
    //topic_dist_matrix = compute_topic_inner_product_distance_matrix(plsa_model);
    //topic_dist_matrix = compute_topic_intersection_distance_matrix(plsa_model);
    //topic_dist_matrix = compute_topic_bhattacharyya_distance_matrix(plsa_model);
  }

  // Write out topice distance matrix to file
  int i, j, z, z_i, z_j;
  if ( zdist_out != NULL ) {
    FILE *fp = fopen_safe ( zdist_out, "w" );
    for ( i=0; i<plsa_model->num_topics; i++ ) {
      z_i = z_mapping[i];
      for ( j=0; j<plsa_model->num_topics; j++ ) {
	z_j = z_mapping[i];
	if ( j > 0 ) fprintf( fp, " ");
	fprintf( fp, "%.6f", topic_dist_matrix[z_i][z_j] );
      }
      fprintf (fp, "\n");
    }
  }

  // Write out top ranked document for each pair of document labels and latent topics
  // Index is from [1..N} for use in MATLAB
  if ( zrank_out != NULL ) {
    int **topic_sim_rankings = compute_ranking_matrix_from_distance_matrix (topic_dist_matrix,
									    plsa_model->num_topics);
    int *inverse_z_mapping = (int *) calloc(plsa_model->num_topics, sizeof(int));
    for ( i=0; i<plsa_model->num_topics; i++ ) {
      z = z_mapping[i];
      inverse_z_mapping[z] = i;
    }
    FILE *fp = fopen_safe ( zrank_out, "w" );
    for ( i=0; i<plsa_model->num_topics; i++ ) {
      z_i = z_mapping[i];
      for ( j=0; j<plsa_model->num_topics; j++ ) {
	if ( j > 0 ) fprintf( fp, " ");
	fprintf( fp, "%d", inverse_z_mapping[topic_sim_rankings[z_i][j]]+1);
      }
      fprintf (fp, "\n");
    }
  } 

  // Create hierarchical cluster tree of topics from topic distance matrix 
  if ( cluster_topics ) {
    char **topic_labels = create_latent_topic_labels_list ( plsa_summary, 5 );
    TREE_NODE *cluster_tree = bottom_up_cluster( topic_dist_matrix, plsa_model->num_topics,
						 topic_labels, MAX_DIST );
    plot_topic_cluster_tree ( cluster_tree );
  }
  
  // Write out ranked list of most topically relevant words
  if ( ranked_words_out != NULL ) {
    write_topically_ranked_words_to_file ( plsa_model, ranked_words_out ); 
  }

  // Print out the corpus-wide topic summary
  if ( summarize ) {
    printf ("\n");
    print_plsa_summary ( plsa_summary, eval_topics, NULL );
  }
  
  // Print out statistical evaluation of latent topics against document labels
  if ( eval_topics ) {
    PLSA_EVAL_METRICS *plsa_eval_metrics = compute_plsa_to_truth_metrics ( plsa_model );
    float H_T_Z = plsa_eval_metrics->H_T - plsa_eval_metrics->I;
    float H_Z_T = plsa_eval_metrics->H_Z - plsa_eval_metrics->I;
    float EIR = ( H_T_Z + H_Z_T ) / plsa_eval_metrics->H_T;
    printf("\n");
    printf("--------------------\n");
    printf("  Model Evaluation  \n");
    printf("--------------------\n");
    printf("# features:   %6d\n",plsa_model->num_features);
    printf("H(T):         %6.3f\n",plsa_eval_metrics->H_T);
    printf("H(Z):         %6.3f\n",plsa_eval_metrics->H_Z);
    printf("I(Z;T):       %6.3f\n",plsa_eval_metrics->I);
    printf("H(T|Z):       %6.3f\n",H_T_Z);
    printf("H(Z|T):       %6.3f\n",H_Z_T);
    printf("EIR(Z;T):     %6.3f    = ( H(T|Z) + H(Z|T) ) / H(T)\n",EIR);
    printf("IC(Z;T):      %6.3f    = 1 - EIR(Z;T)\n",plsa_eval_metrics->IC);
    printf("H-Precision:  %6.3f    = I(Z;T) / H(Z)\n",plsa_eval_metrics->Pzt);
    printf("H-Recall:     %6.3f    = I(Z;T) / H(T)\n",plsa_eval_metrics->Ptz);
    printf("NMI(Z;T):     %6.3f    = 2 * I(Z;T) / ( H(Z)+ H(T) ) {'F-score'}\n",plsa_eval_metrics->NMI);
    printf("P-score:      %6.3f    = I(Z;T) / SQRT( H(Z) * H(T) )\n",plsa_eval_metrics->P);
    printf("--------------------\n");
  }
  return 0;
  
}

void write_plsa_lm_matrix_to_file ( PLSA_MODEL *plsa_model, char *filename )
{
  
  if ( filename == NULL ) return;

  printf("(Writing PLSA LM matrix to file..."); fflush(stdout);
  FILE *fp = fopen_safe ( filename, "w" );
  int z, w;
  for ( z=0; z<plsa_model->num_topics; z++ ) {
    for ( w=0; w<plsa_model->num_features; w++ ) {
      if ( w>0 ) fprintf( fp, " ");
      fprintf( fp, "%e", plsa_model->P_w_given_z[w][z]);
    }
    fprintf(fp,"\n");
  }
  fclose(fp);
  printf("done)\n");

  return;
  
}

void write_z2t_counts_to_file ( PLSA_MODEL *plsa_model, char *filename ) 
{
  if ( filename == NULL ) return;
 
  printf("(Writing z2t counts to file..."), fflush(stdout);
  float **latent_to_truth_matrix = compute_joint_latent_truth_counts ( plsa_model );

  CLASS_SET *classes = plsa_model->classes;
  int num_true_topics = classes->num_classes;
  int num_latent_topics = plsa_model->num_topics;
  int i, t, z;
  int *z_mapping = plsa_model->z_mapping;
  if ( z_mapping == NULL ) {
    z_mapping = (int *) calloc(num_latent_topics, sizeof(int));
    for ( z=0; z<num_latent_topics; z++) z_mapping[z] = z;
  }

  FILE *fp = fopen_safe ( filename, "w" );
  for ( t=0; t<num_true_topics; t++ ) {
    fprintf (fp, "%s", classes->class_names[t]);
    for ( i=0; i<num_latent_topics; i++ ) {
      z = z_mapping[i];
      fprintf (fp, " %.6f", latent_to_truth_matrix[z][t]);
    }
    fprintf (fp, "\n");
  }
  fclose(fp);
  
  free2d((char **)latent_to_truth_matrix);
  printf("done)\n");

  return;

}

void write_d2z_counts_to_file ( PLSA_MODEL *plsa_model, char *filename ) 
{
  if ( filename == NULL ) return;
 
  printf("(Writing d2z probabilities to file..."), fflush(stdout);
  int *z_mapping = plsa_model->z_mapping;
  int num_topics = plsa_model->num_topics;
  int d, i, z;
  if ( z_mapping == NULL ) {
    z_mapping = (int *) calloc(num_topics, sizeof(int));
    for ( z=0; z<num_topics; z++) z_mapping[z] = z;
  }
  FILE *fp = fopen_safe ( filename, "w" );
  for ( d=0; d<plsa_model->num_documents; d++ ) {
    for ( i=0; i<num_topics; i++ ) {
      z = z_mapping[i];
      // fprintf( fp, " %.6f", plsa_model->num_words_in_d[d] * plsa_model->P_z_given_d[z][d] );
      fprintf( fp, " %.6f", plsa_model->P_z_given_d[z][d] );
    }
    fprintf (fp, "\n");
  }
  fclose(fp);
    
  printf("done)\n");

  return;

}

void write_doc_to_topic_map_to_file ( PLSA_MODEL *plsa_model, char *filename ) 
{
  if ( filename == NULL ) return;
  
  CLASS_SET *classes = plsa_model->classes;
  int num_true_topics = classes->num_classes;
  int num_latent_topics = plsa_model->num_topics;
  int i, t, z;
  int *z_mapping = plsa_model->z_mapping;
  if ( z_mapping == NULL ) {
    z_mapping = (int *) calloc(num_latent_topics, sizeof(int));
    for ( z=0; z<num_latent_topics; z++) z_mapping[z] = z;
  }

  printf("(Writing best document to topic pair mapping to file..."), fflush(stdout);
  int **stories_map = find_best_stories_map(plsa_model, plsa_model->class_indices, num_true_topics);
  FILE *fp = fopen_safe ( filename, "w" );
  for ( t=0; t<num_true_topics; t++ ) {
    fprintf (fp, "%s", classes->class_names[t]);
    for ( i=0; i<num_latent_topics; i++ ) {
      z = z_mapping[i];
      fprintf (fp, " %d", stories_map[z][t]);
    }
    fprintf (fp, "\n");
  }
  fclose(fp);

  free2d((char **)stories_map);
  printf("done)\n");
  

  return;
}

void load_feature_vector_info_into_plsa_model ( PLSA_MODEL *plsa_model, char *vector_list_in ) 
{
  if ( plsa_model == NULL ) die ( "PLSA model is NULL?!?\n");
  if ( vector_list_in == NULL ) die ( "Feature vector file name is NULL?!?\n");

  int num_documents = plsa_model->num_documents;  
  CLASS_SET *classes  = create_class_set_from_file_list ( vector_list_in );
  int num_classes = classes->num_classes;
  int tmp_count;
  int *class_indices = load_feature_vector_class_indices ( vector_list_in, classes, &tmp_count);

  if ( tmp_count != num_documents )
    die ("Number of documents in PLSA model (%d) doesn't match feature vector list (%d)\n",
	 num_documents, tmp_count);
  
  float *doc_P_of_class = (float *) calloc ( num_classes, sizeof(float) );
  int d;
  float denom = ((float)num_documents);
  for ( d=0; d<num_documents; d++ ) {
    doc_P_of_class[class_indices[d]] += 1.0/denom;
  }
	
  plsa_model->classes = classes;
  plsa_model->class_indices = class_indices;
  plsa_model->doc_P_of_class = doc_P_of_class;

  return;
  
}

int **compute_ranking_matrix_from_distance_matrix(float **topic_dist_matrix, int dim )
{
  int **ranking_matrix = (int **) calloc2d (dim, dim, sizeof(int));
  int i, j;
  for ( i=0; i<dim; i++ ) {
    IV_PAIR_ARRAY *array = create_iv_pair_array ( dim );
    for ( j=0; j<dim; j++ ) {
      array->pairs[j]->value = -topic_dist_matrix[i][j];
    }
    sort_iv_pair_array(array);
    for ( j=0; j<dim; j++ ) {
      ranking_matrix[i][j] = array->pairs[j]->index;
    }
    free_iv_pair_array(array);
  }
  return ranking_matrix;
  
}

void plot_topic_cluster_tree ( TREE_NODE *cluster_tree )
{
  TREE_PLOT_PARAMETERS *param = (TREE_PLOT_PARAMETERS *) malloc(sizeof(TREE_PLOT_PARAMETERS));
  param->rotate = 1;
  param->label_nodes = 0;
  param->fontsize = 12;
  param->ps_out = strdup("temp___plot.ps");
  param->font = (char *) calloc( 40, sizeof(char) );
  sprintf ( param->font , "-*-helvetica-medium-r-normal--%d-*", param->fontsize );
  param->label_space = find_longest_label(cluster_tree) *  param->fontsize;

  FILE *fp = fopen_safe ( "temp___plot.tk", "w" );
  create_tk_plotting_file( cluster_tree, param, fp );
  fclose(fp);
  if ( ! system ( "chmod 755 temp___plot.tk" ) ) { 
    die ("Can't chmod temporary file: temp__plot.tk\n");
  }
  if ( ! system( "temp___plot.tk" ) ) {
    die ("Can't execute temporary file: temp__plot.tk\n");
  }
  if ( ! system ( "rm temp___plot.tk" ) ) {
    die ("Can't remove temporary file: temp__plot.tk\n");
  }
  
  return;

}


float **compute_topic_bhattacharyya_distance_matrix ( PLSA_MODEL *plsa_model )   
{
  float **P_w_given_z = plsa_model->P_w_given_z;
  int num_w = plsa_model->num_features;
  int num_z = plsa_model->num_topics;
  float **D = (float **) calloc2d (num_z, num_z, sizeof(float));
  int i, j, w;
  float dist;
  for ( i=0; i<num_z; i++ ) {
    D[i][i]=0;
    for ( j=i+1; j<num_z; j++ ) {
      dist = 0;
      for ( w=0; w<num_w; w++ ) dist += sqrtf(P_w_given_z[w][i] * P_w_given_z[w][j]);
      dist = -logf (dist);
      D[i][j] = dist;
      D[j][i] = dist;
    }  
  }
  return D;
}

float **compute_topic_inner_product_distance_matrix ( PLSA_MODEL *plsa_model )
{
  float **P_w_given_z = plsa_model->P_w_given_z;
  int num_w = plsa_model->num_features;
  int num_z = plsa_model->num_topics;
  float **D = (float **) calloc2d (num_z, num_z, sizeof(float));
  int i, j, w;
  float dist;
  for ( i=0; i<num_z; i++ ) {
    D[i][i]=0;
    for ( j=i+1; j<num_z; j++ ) {
      dist = 0;
      for ( w=0; w<num_w; w++ ) dist += P_w_given_z[w][i] * P_w_given_z[w][j];
      dist = -logf (dist);
      D[i][j] = dist;
      D[j][i] = dist;
    }  
  }
  return D;
}

float **compute_topic_intersection_distance_matrix ( PLSA_MODEL *plsa_model )
{
  float **P_w_given_z = plsa_model->P_w_given_z;
  int num_w = plsa_model->num_features;
  int num_z = plsa_model->num_topics;
  float **D = (float **) calloc2d (num_z, num_z, sizeof(float));
  int i, j, w;
  float dist;
  for ( i=0; i<num_z; i++ ) {
    D[i][i]=0;
    for ( j=i+1; j<num_z; j++ ) {
      dist = 0;
      for ( w=0; w<num_w; w++ ) {
	if ( P_w_given_z[w][i] < P_w_given_z[w][j] ) {
	  dist += P_w_given_z[w][i];
	} else {
	  dist += P_w_given_z[w][j];
	}
      }
      dist = -logf (dist);
      D[i][j] = dist;
      D[j][i] = dist;
    }  
  }
  return D;
}

float **compute_topic_chebyshev_distance_matrix ( PLSA_MODEL *plsa_model )
{
  float **P_w_given_z = plsa_model->P_w_given_z;
  int num_w = plsa_model->num_features;
  int num_z = plsa_model->num_topics;
  float **D = (float **) calloc2d (num_z, num_z, sizeof(float));
  int i, j, w;
  float dist, max_dist;
  for ( i=0; i<num_z; i++ ) {
    D[i][i]=0;
    for ( j=i+1; j<num_z; j++ ) {
      max_dist = 0;
      for ( w=0; w<num_w; w++ ) {
	dist = P_w_given_z[w][i] - P_w_given_z[w][j];
	if ( dist > max_dist ) {
	  max_dist = dist;
	} else if ( -dist > max_dist ) {
	  max_dist = -dist;
	}
      }
      D[i][j] = max_dist;
      D[j][i] = max_dist;
    }  
  }
  return D;
}

float **compute_topic_soergel_distance_matrix ( PLSA_MODEL *plsa_model )
{
  float **P_w_given_z = plsa_model->P_w_given_z;
  int num_w = plsa_model->num_features;
  int num_z = plsa_model->num_topics;
  float **D = (float **) calloc2d (num_z, num_z, sizeof(float));
  int i, j, w;
  float numer, denom, dist;
  for ( i=0; i<num_z; i++ ) {
    D[i][i]=0;
    for ( j=i+1; j<num_z; j++ ) {
      numer = 0;
      denom = 0;
      for ( w=0; w<num_w; w++ ) {
	if ( P_w_given_z[w][i] > P_w_given_z[w][j] ) {
	  numer += P_w_given_z[w][i] - P_w_given_z[w][j];
	  denom += P_w_given_z[w][i];
	} else {
	  numer += P_w_given_z[w][j] - P_w_given_z[w][i];
	  denom += P_w_given_z[w][j];
	}
      }
      dist = numer/denom;
      D[i][j] = dist;
      D[j][i] = dist;
    }  
  }
  return D;
}

float **compute_topic_kulczynski_distance_matrix ( PLSA_MODEL *plsa_model )
{
  float **P_w_given_z = plsa_model->P_w_given_z;
  int num_w = plsa_model->num_features;
  int num_z = plsa_model->num_topics;
  float **D = (float **) calloc2d (num_z, num_z, sizeof(float));
  int i, j, w;
  float numer, denom, dist;
  for ( i=0; i<num_z; i++ ) {
    D[i][i]=0;
    for ( j=i+1; j<num_z; j++ ) {
      numer = 0;
      denom = 0;
      for ( w=0; w<num_w; w++ ) {
	if ( P_w_given_z[w][i] > P_w_given_z[w][j] ) {
	  numer += P_w_given_z[w][i] - P_w_given_z[w][j];
	  denom += P_w_given_z[w][j];
	} else {
	  numer += P_w_given_z[w][j] - P_w_given_z[w][i];
	  denom += P_w_given_z[w][i];
	}
      }
      dist = numer/denom;
      D[i][j] = dist;
      D[j][i] = dist;
    }  
  }
  return D;
}

char **create_latent_topic_labels_list ( PLSA_SUMMARY *summary, int summary_size ) 
{
  FEATURE_SET *features = summary->features;
  int num_words = summary->num_summary_features;
  if ( summary_size < num_words ) num_words = summary_size;
  int num_topics = summary->num_topics;
  char **topic_labels = (char **) calloc ( num_topics, sizeof(char *));
  int z, f, w;
  for ( z=0; z<num_topics; z++ ) {
    int string_length = 1;
    for ( f=0; f<num_words; f++ ) {
      w = summary->summary_features[z][f];
      string_length += strlen(features->feature_names[w]) + 1;
    }
    topic_labels[z] = (char *) calloc(string_length, sizeof(char));
    for ( f=0; f<num_words; f++ ) {
      w = summary->summary_features[z][f];
      if ( f == 0 ) strcpy(topic_labels[z], features->feature_names[w]);
      else {
	strcat(topic_labels[z], ",");
	strcat(topic_labels[z], features->feature_names[w]);
      }
    }
  }

  return topic_labels;

}

void find_best_story_for_topic_and_class ( PLSA_MODEL *plsa_model, int z,
					   SPARSE_FEATURE_VECTORS *feature_vectors, char *class_label ) 
{
  // Check validity of class label
  CLASS_SET *classes = feature_vectors->class_set ;
  HASHTABLE *class_hash = classes->class_name_to_class_index_hash;
  int class_id = get_hashtable_string_index (class_hash, class_label);
  if ( class_id == -1 ) {
    die ("Label '%s' is not a valid class label\n",class_label);
  }

  // Check validity of latent topic id
  if ( z<0 || z>plsa_model->num_topics-1 ) {
    die ("Latent topic id %d is not valid\n",z);
  }

  int best_index = -1;
  float best_score = 0;
  float score;

  SPARSE_FEATURE_VECTOR **vectors = feature_vectors->vectors;
  int num_vectors = feature_vectors->num_vectors;
  int d;
  for ( d=0; d<num_vectors; d++ ) {
    if ( vectors[d]->class_id == class_id ) {
      score = plsa_model->P_z_given_d[z][d];
      if ( best_index == -1 || score > best_score ) {
	best_index = d;
	best_score = score;
      }
    }
  }
  
  printf ("[%s] Best document index: %d (score=%f)\n",class_label,best_index, best_score);

  return;

}



int **find_best_stories_map ( PLSA_MODEL *plsa_model, int *class_indices, int num_classes )
{
  int num_topics = plsa_model->num_topics;
  int num_documents = plsa_model->num_documents;
  int **map = (int **) calloc2d ( num_topics, num_classes, sizeof(int));
  
  int d, t, z;
  int best_index;
  float best_score, score;
  for ( t=0; t<num_classes; t++ ) {
    for ( z=0; z<num_topics; z++ ) {
      best_index = -1;
      best_score = 0;
      for ( d=0; d<num_documents; d++ ) {
	if ( class_indices[d] == t ) {
	  score = plsa_model->P_z_given_d[z][d];
	  if ( best_index == -1 || score > best_score ) {
	    best_index = d;
	    best_score = score;
	  }
	}
	map[z][t] = best_index+1; // Add 1 for matlab indexing
      }
    }
  }
  
  return map;

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

void characterize_words_by_topical_importance ( PLSA_MODEL *plsa_model, 
						FEATURE_SET *features,
						char *file_out )
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
      fprintf(fp, "%s %.8f\n",features->feature_names[w], score);
    }
    fclose(fp);
  }
  
  printf("--------------------------------------------------------------------\n");

  printf("Top 100 Globally important topic words:\n");
  for ( i=0; i<100; i++ ) {
    w = global_word_scores[i]->index;
    score = global_word_scores[i]->value;
    printf("%3d: (%.6f) %s\n",i+1,score,features->feature_names[w]);
  }

  printf("--------------------------------------------------------------------\n");


}

float *extract_class_probs ( SPARSE_FEATURE_VECTORS *feature_vectors, CLASS_SET *classes )
{
  int num_classes = classes->num_classes;
  int num_documents = feature_vectors->num_vectors;
  float *P_of_class = (float *) calloc(num_classes, sizeof(float));
  int c, d;

  for ( d=0; d<num_documents; d++ ) {
    c = feature_vectors->vectors[d]->class_id;
    P_of_class[c] += 1.0;
  }

  float norm = (float)num_documents;
  for ( c=0; c<num_classes; c++ ) {
    P_of_class[c] = P_of_class[c]/norm;
  }

  return P_of_class;

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

