/* -*- C -*-
 *
 * Copyright (c) 2008
 * Massachusetts Institute of Technology
 *
 * All Rights Reserved
 *
 * FILE: classifier_util.h
 *
 */

#ifndef LL_CLASSIFIER_INCLUDED
#define LL_CLASSIFIER_INCLUDED

typedef struct FILE_LIST {
  int num_files;
  char **filenames;
} FILE_LIST;

typedef struct CLASS_SET {
  int num_classes;
  char **class_names;
  HASHTABLE *class_name_to_class_index_hash;
} CLASS_SET;

typedef struct FEATURE_SET {
  int num_features;
  char **feature_names;
  HASHTABLE *feature_name_to_index_hash;
  float *feature_weights;
  int *num_words;
} FEATURE_SET;

typedef struct SPARSE_FEATURE_VECTOR {
  char *filename; // Name of file containing the feature vector
  int set_id; // Used to subdivide full set into partitions
  int num_labels; // Number of class labels that this token is a positive example of
  int *class_ids; // Array of class indices corresponding to class labels for this token
  int class_id; // Primary class label for vector	
  int num_features; // Number of non zero features in feature vector
  int *feature_indices; // Indices of non-zero features in full feature vector
  float *feature_values; // Values of non-zero features in full feature vector
  float total_sum; // Sum of feature values (i.e., total word count)
} SPARSE_FEATURE_VECTOR;

typedef struct SPARSE_FEATURE_VECTORS {
  int num_vectors;
  int num_sets;
  SPARSE_FEATURE_VECTOR **vectors;
  FEATURE_SET *feature_set; // Pointer to the corresponding set of features
  CLASS_SET *class_set; // Pointer to the corresponding set of classes
} SPARSE_FEATURE_VECTORS;

// The basic form of a linear classifier is:
// S(x) = Ax+b
// S(x) produces a vector of class scores for x
// x is the test vector
// A is a routing matrix
// b is a set of score offsets
// x may be normalized (L1 or L2) and have feature weights applied (w*x)  
typedef struct LINEAR_CLASSIFIER {
  int num_classes;
  int num_features;
  char *norm_type;
  float *offsets;
  float **matrix;
  FEATURE_SET *features;
  CLASS_SET *classes;
} LINEAR_CLASSIFIER;

// Support vector machine structeral definitions:
// score(x,c) = b_c + a_c * sum_i ( w_i,c * K(v_i,x) )
// b_c : score offset for class c
// a_c : score scale for class c
// w_i,c : weight of individual support vector for class c
// Note that we assume L1 norm of positive weights is 1, i.e. sum_i(w_i,c=pos) = 1
// and the L1 norm of negative weights is -1, i.e. sum_i(w_i,c=neg) = -1
// The tradition standard support vector weight is thus a_c * w_i,c
typedef struct SVM_PARAMETERS {
  int num_classes;
  int num_vectors;
  float **support_vector_weights; // SVM alpha weights (L1 normed to 1 on positive & negative sides)
  float *class_scales; // SVM scale applied to each support vector w
  float *class_offsets; // SVM offset (this is the negative of the standard SVM b value)
  int *vector_index_map;
  CLASS_SET *classes;
} SVM_PARAMETERS;

typedef struct VECTOR_ERROR_STATS {
  int correct;
  int best_class;
  int correct_class;
  float correct_score;
  int worst_correct_class;
  float worst_correct_score;
  int best_incorrect_class;
  float best_incorrect_score;
  float error; // Score diff between best incorrect class and correct class
  float misclassification; // Value of misclassification measure M(x)
  float loss; // Value of loss function l(x)
  float *class_weights; // Psuedo posterior scales of incorrect classes
  float *incorrect_class_weights; // Psuedo posterior scales of incorrect classes
  float *correct_class_weights; // Psuedo posterior scales of correct classes
} VECTOR_ERROR_STATS;


FILE_LIST *read_file_list_from_file ( char *list_filename ); 
SPARSE_FEATURE_VECTORS *load_sparse_feature_vectors ( char *list_filename, FEATURE_SET *feature_set, CLASS_SET *class_set);
SPARSE_FEATURE_VECTORS *load_sparse_feature_vectors_combined (char *count_fn, FEATURE_SET *feature_set, CLASS_SET *class_set);
SPARSE_FEATURE_VECTOR *load_sparse_feature_vector ( char *filename, FEATURE_SET *feature_set );
SPARSE_FEATURE_VECTOR *load_sparse_feature_vector_combined (char *substrings[], int num_substrings, FEATURE_SET *feature_set);
SPARSE_FEATURE_VECTORS *copy_sparse_feature_vectors ( SPARSE_FEATURE_VECTORS *orig_feature_vectors );
SPARSE_FEATURE_VECTOR *copy_sparse_feature_vector (SPARSE_FEATURE_VECTOR *orig_vector);
void free_sparse_feature_vectors ( SPARSE_FEATURE_VECTORS *feature_vectors );
void free_sparse_feature_vector ( SPARSE_FEATURE_VECTOR *vector );
void partition_feature_vectors_into_sets ( SPARSE_FEATURE_VECTORS *feature_vectors, int num_sets );
void L1_normalize_sparse_feature_vectors ( SPARSE_FEATURE_VECTORS *feature_vectors );
void L2_normalize_sparse_feature_vectors ( SPARSE_FEATURE_VECTORS *feature_vectors );
void remove_zero_weight_features ( FEATURE_SET *features );
void prune_zero_weight_features_from_feature_vectors ( SPARSE_FEATURE_VECTORS *feature_vectors );

float *extract_feature_counts_from_sparse_feature_vectors ( SPARSE_FEATURE_VECTORS *feature_vectors );

FEATURE_SET *create_feature_set_from_file ( char *count_fn, float min_feature_count, FEATURE_SET *stop_list );
FEATURE_SET *create_feature_set_from_file_list ( char *list_filename, float min_feature_count, FEATURE_SET *stop_list );
void augment_feature_set_from_file_list ( char *list_filename,  FEATURE_SET *feature_set, FEATURE_SET *stop_list );
FEATURE_SET *load_feature_set ( char *filename );
void free_feature_set ( FEATURE_SET *feature_set );
void save_feature_set ( FEATURE_SET *features, char *filename );
void add_word_count_info_into_feature_set ( FEATURE_SET *features, FEATURE_SET *stop_list );

CLASS_SET *create_class_set_from_file_list ( char *list_filename );
void free_class_set (CLASS_SET *class_set);

int *load_feature_vector_class_indices ( char *list_filename, CLASS_SET *class_set, int *count );
float *compute_MAP_estimated_distribution_with_uniform_prior ( float *counts, 
							       int num_features, 
							       float tau );
float *compute_MAP_estimated_distribution ( float *counts, float *priors,
					    int num_features, float tau );
void normalize_feature_weights (float *feature_weights, int num_features);
int feature_vector_class_cmp(const void *a, const void *b);
int feature_vector_set_cmp(const void *a, const void *b);

#endif /* LL_CLASSIFIER_UTIL_INCLUDED */


/* 
  for Emacs...
  Local Variables:
  mode: c
  fill-column: 110
  comment-column: 80
  c-tab-always-indent: nil
  c-indent-level: 2
  c-continued-statement-offset: 2
  c-brace-offset: -2
  c-argdecl-indent: 2
  c-label-offset: -2
  End:
*/

