/* -*- C -*-
 *
 * Copyright (c) 2010
 * MIT Lincoln Laboratory
 * Massachusetts Institute of Technology
 *
 * All Rights Reserved
 *
 * FILE: clustering_util.h
 *
 * ORIGINAL AUTHOR: Timothy J. Hazen
 *
 */

#ifndef LL_CLUSTERING_UTIL_INCLUDED
#define LL_CLUSTERING_UTIL_INCLUDED

/***************************************************************************************************/

typedef struct IV_PAIR {
  int index;
  float value;
} IV_PAIR;

typedef struct IV_PAIR_ARRAY {
  int num_pairs;
  IV_PAIR **pairs;
  IV_PAIR *data;
} IV_PAIR_ARRAY;

typedef struct TREE_NODE {
  int node_index;
  int cluster_index;
  char *label;
  float score;
  float height;
  float left_side;
  float right_side;
  int mark;
  int leaves;
  struct TREE_NODE *left_child;
  struct TREE_NODE *right_child;
  struct TREE_NODE *array_ptr;
} TREE_NODE;

typedef struct TREE_PLOT_PARAMETERS {
  char *font;
  int fontsize;
  int convert;
  int rotate;
  int label_nodes;
  char *ps_out;
  int height;
  int width;
  int leaf_width;
  float scale;
  int label_space;
  int margin;
} TREE_PLOT_PARAMETERS;

typedef struct LDA_FEATURE_VECTORS {
  int num_vectors; // Number of feature vectors (i.e., number of documents)
  int num_topics; // Number of underlying topics in LDA representation
  float **vectors; // Feature vectors represented as LDA topic distributions
} LDA_FEATURE_VECTORS;

#define NO_WEIGHTING 0 
#define IDF_WEIGHTING 1
#define LLR_WEIGHTING 2

#define KL_MEAS 0
#define IP_MEAS 1
#define COS_MEAS 2

#define MIN_DIST 0
#define AVG_DIST 1
#define MAX_DIST 2
#define TOT_DIST 3

/***************************************************************************************************/

/* Function prototypes */
char **load_file_list_from_file ( int *num_files_ptr, char *input_filename );
SPARSE_FEATURE_VECTORS *create_normalized_feature_vectors ( SPARSE_FEATURE_VECTORS *feature_vectors,
							    float df_cutoff, float tf_cutoff, int smooth, int weighting, int root );
void normalize_feature_vectors ( SPARSE_FEATURE_VECTORS *feature_vectors,
				 float df_cutoff, float tf_cutoff, int smooth, int weighting, int root );
void learn_feature_weights ( SPARSE_FEATURE_VECTORS *feature_vectors,
			     float df_cutoff, float tf_cutoff, int smooth, int weighting, int root );
float **compute_cosine_similarity_matrix ( SPARSE_FEATURE_VECTORS *feature_vectors, int log_dist, int verbose );
void apply_l2_norm_to_feature_vectors ( SPARSE_FEATURE_VECTORS *feature_vectors );
float compute_sparse_vector_dot_product ( SPARSE_FEATURE_VECTOR *vector_i, 
					  SPARSE_FEATURE_VECTOR *vector_j );
void apply_feature_weights_to_feature_vectors ( SPARSE_FEATURE_VECTORS *feature_vectors );
void prune_zero_weight_features_from_feature_vectors ( SPARSE_FEATURE_VECTORS *feature_vectors );

LDA_FEATURE_VECTORS *load_lda_feature_vectors ( char *lda_vectors_in );
float **compute_lda_cosine_similarity_matrix ( LDA_FEATURE_VECTORS *feature_vectors );
float **compute_topic_prob_similarity_matrix ( LDA_FEATURE_VECTORS *feature_vectors );
void convert_similarity_matrix_to_distance_matrix ( float **matrix, int num_dims, float ceiling );
float **compute_kl_divergence_matrix ( LDA_FEATURE_VECTORS *feature_vectors ); 
void add_in_similarity_matrix ( float **full_matrix, float **matrix, int num_dims );
float **interpolate_similarity_matrices ( float **matrix1, float **matrix2, int num_dims, float weight1 );

void create_tk_plotting_file( TREE_NODE *root, TREE_PLOT_PARAMETERS *param, FILE *fp );
void create_tk_commands_for_node( TREE_NODE *node, TREE_PLOT_PARAMETERS *param, FILE *fp);
int find_longest_label(TREE_NODE *node);
void find_cluster_labels(TREE_NODE *node, int node_id, int child,
			 char ***list_ptr, int *n_ptr, char ***ptr );

int *kmeans_clustering ( SPARSE_FEATURE_VECTORS *feature_vectors, int num_clusters, int max_iter );

TREE_NODE *bottom_up_cluster ( float **matrix, int ndims, char **labels, int dist_metric );
void print_cluster_tree( TREE_NODE *node ); 
void save_cluster_tree( TREE_NODE *node, FILE *fp );
void save_cluster_trees( TREE_NODE **nodes, int num_trees, FILE *fp );
TREE_NODE *load_cluster_tree( FILE *fp );
TREE_NODE **load_cluster_trees( int *num_trees, FILE *fp );
void save_distance_matrix ( float **matrix, char **labels, int num_elements, FILE *fp );
float **load_distance_matrix ( int *dims_ptr, char ***labels_ptr, FILE *fp);
void mark_top_clusters_in_tree(TREE_NODE *node, int num_to_mark); 
int label_clusters_in_tree(TREE_NODE *node, int num_to_label);
int *assign_vector_labels_from_cluster_tree ( TREE_NODE *cluster_tree, int num_vectors );
float *create_sorted_list_of_node_heights ( TREE_NODE *node ); 
void score_clusters_in_tree ( TREE_NODE *node ); 
void mark_best_scoring_clusters_in_tree ( TREE_NODE *node );
void scale_tree_heights ( TREE_NODE *node, float scale );
void clear_non_terminal_labels ( TREE_NODE *node );
void mark_all_nodes_in_tree ( TREE_NODE *node ); 
void free_cluster_tree ( TREE_NODE *node ); 

IV_PAIR_ARRAY *create_iv_pair_array ( int num );
void free_iv_pair_array ( IV_PAIR_ARRAY *array );
int cmp_iv_pair ( const void *p1, const void *p2 );
void sort_iv_pair_array ( IV_PAIR_ARRAY *array ); 

/***************************************************************************************************/

#endif /* LL_CLUSTERING_UTIL_INCLUDED */










