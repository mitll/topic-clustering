/* -*- C -*-
 *
 * Copyright (c) 2010
 * MIT Lincoln Laboratory
 * Massachusetts Institute of Technology
 *
 * All Rights Reserved
 *
 * FILE: plsa.h
 *
 * ORIGINAL AUTHOR: Timothy J. Hazen
 *
 */

#ifndef LL_PLSA_INCLUDED
#define LL_PLSA_INCLUDED

#include "classifiers/classifier_util.h"

typedef struct PLSA_MODEL {
  // Model parameters
  int num_topics;
  int num_features;
  int num_documents;
  float alpha;
  float beta;
  float **P_z_given_d;
  float **P_w_given_z;
  float *num_words_in_d;
  float *P_w;             // Added this to help with summarization
  float *P_z;             // Added this to help with summarization
  int *z_mapping;         // Mapping of ranked topics to indexed topics
  int *z_inverse_mapping; // Mapping of indexed topics to ranked topics

  // Feature set
  FEATURE_SET *features;
  
  // Class set info...if class labels for data are known
  CLASS_SET *classes;
  int *class_indices;
  float *doc_P_of_class;  // P(t) based on document units
  float *word_P_of_class; // P(t) based on word units

  // Model statistics
  IV_PAIR **global_word_scores;
  float avg_likelihood;
  float total_likelihood;
  float total_words;

} PLSA_MODEL;

typedef struct PLSA_EVAL_METRICS {
  float H_T;    // Entropy of true topic distribution: H(T)
  float H_Z;    // Entropy of latent topic distribution: H(Z) 
  float I;      // Mutual information between Z and T: I(Z;Y)
  float NMI;    // Normalized mutual information between Z and T: 2*I(Z;t)/(H(Z)+H(T)) 
  float IC;     // Information coverage of Z over T: (I(Z;T)-H(Z|T))/H(T) = (2*I(Z;T)-H(Z))/H(T)
  float Pzt;    // Purity of latent topics to true topics mapping: I(Z;T)/H(Z)
  float Ptz;    // Purity of true topics to latent topics mapping: I(Z;T)/H(T)
  float P;      // P-score: (I(Z;T)^2)/(H(T)*H(Z))
} PLSA_EVAL_METRICS;

typedef struct SIG_WORDS {
  int num_words;
  int num_allocated;
  int *word_indices;
  float *word_scores;
  char **word_stems;
} SIG_WORDS;

typedef struct PLSA_EVALUATION {
  PLSA_MODEL *plsa_model;     // PLSA model from which summary is generated
  FEATURE_SET *features;      // Set of features (i.e., words) used in data
  CLASS_SET *classes;         // Set of true topic classes T (if they are known)
  int num_topics;             // Number of topics in PLSA model
  float *z_to_T_purity;       // ( H(T) - H(T|z) / H(T) ) for each latent topic z
  float **Z_to_T_mapping;     // Distribution mapping of latent topics to true topics
  float **T_to_Z_mapping;     // Distribution mapping of true topics to latent topics
} PLSA_EVALUATION;


typedef struct PLSA_SUMMARY {
  PLSA_MODEL *plsa_model;     // PLSA model from which summary is generated
  FEATURE_SET *features;      // Set of features (i.e., words) used in data
  CLASS_SET *classes;         // Set of true topic classes T (if they are known)
  int num_topics;             // Number of topics in PLSA model
  int num_summary_features;   // Number of summary features per topic
  int **summary_features;     // Collection of selected summary features for each topic
  float *P_z;                 // P(z) for each topic as determined from PLSA model
  float *z_to_D_purity;       // z to D purity measure for each latent topic z
  float *z_score;             // 100 * P(z) * z->D purity
  float *z_to_T_purity;       // ( H(T) - H(T|z) / H(T) ) for each latent topic z
  float **Z_to_T_mapping;     // Distribution mapping of latent topics to true topics
  float **T_to_Z_mapping;     // Distribution mapping of true topics to latent topics
  int *sorted_topics;         // Indices of latent topics sorted by z_score
} PLSA_SUMMARY;

/***************************************************************************************************/

/* Function prototypes */

PLSA_MODEL *train_plsa_model_from_labels ( SPARSE_FEATURE_VECTORS *feature_vectors, int *labels, 
					   int num_topics, float alpha, float beta, int max_iter, 
					   float conv_threshold, int hard_init );

void write_plsa_model_to_file( char *fileout, PLSA_MODEL *plsa_model );
PLSA_MODEL *load_plsa_model_from_file( char *filein );

void write_plsa_posteriors_to_file(char *fileout, PLSA_MODEL *plsa_model );
float **load_plsa_posteriors_from_file( char *filein, int *num_topics_ptr, int *num_docs_ptr );

void write_plsa_unigram_models_to_file(char *fileout, PLSA_MODEL *plsa_model );
float **load_plsa_unigram_models_from_file( char *filein, int *num_features_ptr, int *num_topics_ptr );

void free_plsa_model ( PLSA_MODEL *plsa_model );
PLSA_MODEL *copy_plsa_model ( PLSA_MODEL *plsa_model_orig );
PLSA_MODEL *initialize_plsa_model ( SPARSE_FEATURE_VECTORS *feature_vectors, int *vector_labels, 
				    int num_topics, float alpha, float beta, int hard_init );

void estimate_plsa_model ( PLSA_MODEL *plsa_model, SPARSE_FEATURE_VECTORS *feature_vectors, 
			   float alpha, float beta, int max_iter, float conv_threshold,
			   int ignore_set, int verbose );

PLSA_SUMMARY *summarize_plsa_model ( PLSA_MODEL *plsa_model, int stem_list);
void print_plsa_summary ( PLSA_SUMMARY *summary, int eval_topics, char *file_out );
void write_topically_ranked_words_to_file ( PLSA_MODEL *plsa_model, char *file_out ); 

float compute_plsa_topic_entropy ( PLSA_MODEL *plsa_model );
PLSA_EVAL_METRICS *compute_plsa_to_truth_metrics ( PLSA_MODEL *plsa_model );
float **compute_joint_latent_truth_counts ( PLSA_MODEL *plsa_model );
float **map_plsa_to_truth ( PLSA_MODEL *plsa_model );
float **map_truth_to_plsa ( PLSA_MODEL *plsa_model );

float **compute_similarity_matrix_from_plsa_model ( PLSA_MODEL *plsa_model, int log_dist );
int *deterministic_clustering ( SPARSE_FEATURE_VECTORS *feature_vectors, int num_clusters );
int *random_clustering ( SPARSE_FEATURE_VECTORS *feature_vectors, int num_clusters );
int *extract_cluster_labels_from_cluster_tree ( TREE_NODE *cluster_tree, int num_labels, int num_clusters );
TREE_NODE *create_document_cluster_tree ( SPARSE_FEATURE_VECTORS *feature_vectors );
float compute_distribution_entropy ( float *P, int num );

LINEAR_CLASSIFIER *train_naive_bayes_classifier_over_plsa_topics ( SPARSE_FEATURE_VECTORS *feature_vectors,
								   PLSA_MODEL *plsa_model ); 

/***************************************************************************************************/

#endif /* LL_PLSA_INCLUDED */
