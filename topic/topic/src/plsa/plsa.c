/* -*- C -*-
 *
 * Copyright (c) 2010
 * MIT Lincoln Laboratory
 * Massachusetts Institute of Technology
 *
 * All Rights Reserved
 *
 * FILE: plsa.c
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
#include "plsa/plsa.h"
#include "porter_stemmer/porter_stemmer.h"

/**********************************************************************/

static SIG_WORDS *create_signature_words_struct ( int num_sig_words ); 
static void clear_signature_words_struct ( SIG_WORDS *signature_words );
static void free_signature_words_struct ( SIG_WORDS *signature_words );
static void bubble_sort_word_into_sig_word_list ( int index, float score, char *stem, 
						  SIG_WORDS *signature_words );
static void remove_substrings_from_sig_word_list ( SIG_WORDS *signature_words, FEATURE_SET *features );
static int substring (int i, int j, FEATURE_SET *features);
static void estimate_P_z_in_plsa_model ( PLSA_MODEL *plsa_model );
static void estimate_P_w_in_plsa_model ( PLSA_MODEL *plsa_model );

/**********************************************************************/

// Initialize and train a PLSA model from pre-clustered data
PLSA_MODEL *train_plsa_model_from_labels ( SPARSE_FEATURE_VECTORS *feature_vectors, 
					   int *labels, int num_topics, 
					   float alpha, float beta, int max_iter, 
					   float conv_threshold, int hard_init )
{

  PLSA_MODEL *plsa_model = initialize_plsa_model (feature_vectors, labels, num_topics, alpha, beta, hard_init );
  estimate_plsa_model ( plsa_model, feature_vectors, alpha, beta, max_iter, conv_threshold, -1, 1 );

  return plsa_model;
}



// Assign feature vectors to initial clusters using agglomerative clustering
int *deterministic_clustering ( SPARSE_FEATURE_VECTORS *feature_vectors, int num_clusters )
{
  // Cluster the documents
  TREE_NODE *cluster_tree = create_document_cluster_tree ( feature_vectors );
  
  // Assign cluster labels to feature vectors
  int *vector_labels = extract_cluster_labels_from_cluster_tree ( cluster_tree, feature_vectors->num_vectors, num_clusters );
  
  // Free the cluster tree here
  free_cluster_tree ( cluster_tree );
  
  return vector_labels;
}

int *extract_cluster_labels_from_cluster_tree ( TREE_NODE *cluster_tree, int num_labels, int num_clusters )
{
  // Assign cluster labels to feature vectors
  label_clusters_in_tree ( cluster_tree, num_clusters );

  // Assign cluster labels to feature vectors
  int *labels = assign_vector_labels_from_cluster_tree ( cluster_tree, num_labels );
  
  return labels;
}


// Create a document cluster tree
TREE_NODE *create_document_cluster_tree ( SPARSE_FEATURE_VECTORS *feature_vectors ) 
{

  // Compute cosine similarity matrix
  float **matrix = compute_cosine_similarity_matrix ( feature_vectors, 1, 1 );
  
  // Do bottom up clustering to seed PLSA
  TREE_NODE *cluster_tree = bottom_up_cluster( matrix, feature_vectors->num_vectors, NULL, AVG_DIST );
  free2d((char **)matrix);
  
  return cluster_tree;
}



// Assign feature vectors to initial clusters using random initialization
// of cluster centroid with clusters formed by assigning feature vectors 
// to nearest centroid
int *random_clustering ( SPARSE_FEATURE_VECTORS *feature_vectors, int num_clusters )
{

  printf ("(Doing randomized clustering of feature vectors..."); fflush(stdout);
  int num_vectors = feature_vectors->num_vectors;
  int *seed_map = (int *) calloc(num_vectors, sizeof(int));
  int i, j, tmp;
  int *vector_labels = (int *) calloc(num_vectors, sizeof(int));
  srand((unsigned)(time(0)));

  // Randomly select feature vectors to serve as cluster centroids
  for ( i=0; i<num_vectors; i++ ) {
    seed_map[i] = i;
  }
  for ( i=0; i<num_clusters; i++ ) {
    j = (int) (num_vectors * (rand() / (RAND_MAX + 1.0)));
    if ( j==num_clusters) j--;
    tmp = seed_map[i];
    seed_map[i] = seed_map[j];
    seed_map[j] = tmp;
  }

  // Create an independent copy of the centroid vectors
  SPARSE_FEATURE_VECTORS *centroid_vectors = 
    (SPARSE_FEATURE_VECTORS *) malloc(sizeof(SPARSE_FEATURE_VECTORS));
  centroid_vectors->num_vectors = num_clusters;
  centroid_vectors->vectors = (SPARSE_FEATURE_VECTOR **) calloc(num_clusters, 
								sizeof(SPARSE_FEATURE_VECTOR *));
  for ( i=0; i<num_clusters; i++ ) {
    centroid_vectors->vectors[i] = copy_sparse_feature_vector ( feature_vectors->vectors[seed_map[i]] );
  }

  // Apply feature vector weighting and normalization to centroid vectors 
  // in preparation for cosine similarity comparisons
  if ( feature_vectors->feature_set == NULL ) 
    die ("No feature set specified for feature vectors\n");
  if ( feature_vectors->feature_set->feature_weights == NULL ) 
    die ("No feature weights specified for feature vectors\n");
  centroid_vectors->feature_set = feature_vectors->feature_set;
  apply_feature_weights_to_feature_vectors ( centroid_vectors );
  apply_l2_norm_to_feature_vectors ( centroid_vectors );

  // Apply the feature weights again. These would normally be applied
  // on the features vector side before L2 normalization. However,
  // because we only need to preserve the relative ranking of the 
  // centroids and don't need an exact cosine similarity computation
  // we can push the feature side weights into the centroid and skip
  // the feature side L2 norm.
  apply_feature_weights_to_feature_vectors ( centroid_vectors );

  // Find the best matching cluster for each feature vector
  SPARSE_FEATURE_VECTOR *vector_i, *vector_j;
  int best_cluster;
  float similarity;
  float max_similarity=0;
  for ( i=0; i<num_vectors; i++ ) {
    vector_i = feature_vectors->vectors[i];
    best_cluster=-1;
    max_similarity=0;
    for ( j=0; j<num_clusters; j++ ) {
      vector_j = centroid_vectors->vectors[j];
      similarity = compute_sparse_vector_dot_product ( vector_i, vector_j );
      if ( best_cluster == -1 || similarity > max_similarity ) {
	best_cluster = j;
	max_similarity = similarity;
      }
    }
    vector_labels[i] = best_cluster;
  }

  for ( i=0; i<num_vectors; i++ ) {
    if ( vector_labels[i] == -1 ) {
      printf ("Vector %d still has vector label of -1?!?\n", i);
    }
  }
  
  free(seed_map);
  for ( i=0; i<num_clusters; i++ ) {
    free(centroid_vectors->vectors[i]);
  }
  free(centroid_vectors);
  printf("done)\n");

  return vector_labels;
    
}


// Initialize the PLSA model using pre-labeled vectors
PLSA_MODEL *initialize_plsa_model ( SPARSE_FEATURE_VECTORS *feature_vectors, int *vector_labels, 
				    int num_topics, float alpha, float beta, int hard_init )
{
  printf("(Initializing PLSA model..."); fflush(stdout);
  int d,i,t,w,z;
  double count, denom, total_word_count, sum;
  FEATURE_SET *features = feature_vectors->feature_set;
  CLASS_SET *classes = feature_vectors->class_set;

  // Set up variables
  int num_features = features->num_features;
  int num_documents = feature_vectors->num_vectors;
  SPARSE_FEATURE_VECTOR **vectors = feature_vectors->vectors;
  SPARSE_FEATURE_VECTOR *vector;
  float **P_z_given_d = (float **) calloc2d( num_topics, num_documents, sizeof(float));
  float **P_w_given_z = (float **) calloc2d( num_features, num_topics, sizeof(float));
  float *num_words_in_d = (float *) calloc( num_documents, sizeof(float));
  float *P_w = (float *) calloc( num_features, sizeof(float));
  float *P_z = (float *) calloc( num_topics, sizeof(float));
  
  // Create PLSA model structure
  PLSA_MODEL *plsa_model = (PLSA_MODEL *) malloc(sizeof(PLSA_MODEL));
  plsa_model->P_z_given_d =  P_z_given_d;
  plsa_model->P_w_given_z = P_w_given_z;
  plsa_model->P_z = P_z;
  plsa_model->P_w = P_w;
  plsa_model->num_words_in_d = num_words_in_d;
  plsa_model->num_topics = num_topics;
  plsa_model->num_features = num_features;
  plsa_model->num_documents = num_documents;
  plsa_model->features = features;
  plsa_model->classes = classes;
  plsa_model->class_indices = NULL;
  plsa_model->doc_P_of_class = NULL;
  plsa_model->word_P_of_class = NULL;
  plsa_model->alpha = alpha;
  plsa_model->beta = beta;
  

  // Collect raw counts for P(w), P(z), and P(w|z)
  printf("."); fflush(stdout);
  total_word_count = 0;
  for ( d=0; d<num_documents; d++ ) {
    z = vector_labels[d];
    if ( z<0 || z>=num_topics ) {
      printf("Topic index %d is out of range for document %d of %d?!?\n",z,d,num_documents);
      vector = vectors[d];
      printf("Document %d: %s\n",d,vector->filename);
      printf("Num features: %d\n", vector->num_features);
      die("Topic index %d is out of range for document %d of %d?!?\n",z,d,num_documents);
    }
    vector = vectors[d];
    float doc_word_count = 0.0;
    for ( i=0; i<vector->num_features; i++ ) {
      w = vector->feature_indices[i];
      count = vector->feature_values[i];
      P_w_given_z[w][z] += count;
      P_w[w] += count;
      P_z[z] += count;
      doc_word_count += count;
    }
    total_word_count += doc_word_count;
    num_words_in_d[d] = doc_word_count;
    // if ( num <= 0.0 ) die ("No features in doc %d?!?\n",d);
  }
  

  // Compute initial P(z) estimates
  printf("."); fflush(stdout);
  sum = 0;
  for ( z=0; z<num_topics; z++ ) {
    P_z[z] += alpha;
    sum += P_z[z];
  }
  for ( z=0; z<num_topics; z++ ) {
    P_z[z] = P_z[z] / sum;
  }

  // Compute initial P(w|z) estimates 
  printf("."); fflush(stdout);
  for ( z=0; z<num_topics; z++ ) {
    sum = 0.0;
    for ( w=0; w<num_features; w++ ) {
      P_w_given_z[w][z] += beta;
      sum += (double)P_w_given_z[w][z];
    }
    for ( w=0; w<num_features; w++ ) {
      P_w_given_z[w][z] =  (float)(((double)P_w_given_z[w][z])/sum);
    }
  }

  // Compute P(w) estimates 
  printf("."); fflush(stdout);
  sum = 0;
  for ( w=0; w<num_features; w++ ) {
    P_w[w] += beta;
    sum += P_w[w];
  }
  for ( w=0; w<num_features; w++ ) {
    P_w[w] =  P_w[w] / sum;
  }

  // Estimate initial P(z|d) 
  printf("."); fflush(stdout);
  float tmp;
  if ( hard_init ) {
    for ( d=0; d<num_documents; d++ ) {
      z = vector_labels[d];
      P_z_given_d[z][d] = 1.0;
    }
    /* Below is the original hard init code with alpha smoothing
       ...this functionality is now used only for reference models
       so alpha smoothing is now turned if during hard initialization
       ****
    // Use initial vector labels with alpha smoothing to estimate P(z|d)
    denom = 1.0 + (((float)num_topics)*alpha);
    for ( d=0; d<num_documents; d++ ) {
      z = vector_labels[d];
      P_z_given_d[z][d] = 1.0;
      for ( z=0; z<num_topics; z++ ) {
        P_z_given_d[z][d] += (P_z_given_d[z][d] + alpha)/denom;
      }
    }
    **** */
  } else { 
    if ( 1 ) {
      // Do a fast approximation of P(z|d) from P(w|z) and P(z) 
      for ( d=0; d<num_documents; d++ ) {
	vector = vectors[d];
	denom=0;
	for ( i=0; i<vector->num_features; i++ ) {
	  w = vector->feature_indices[i];
	  count = vector->feature_values[i];
	  for ( z=0; z<num_topics; z++ ) {
	    tmp = count * P_w_given_z[w][z] * P_z[z];
	    P_z_given_d[z][d] += tmp;
	    denom += tmp;
	  }
	}
	for ( z=0; z<num_topics; z++ ) {
	  P_z_given_d[z][d] = P_z_given_d[z][d]/denom;
	}
      }
    } else {
      // This option just initializes all P(z|d) with P(z)
      for ( d=0; d<num_documents; d++ ) {
	for ( z=0; z<num_topics; z++ ) {
	  P_z_given_d[z][d] = P_z[z];
	}
      }
    }
  }

  if ( classes != NULL ) {
    printf("."); fflush(stdout);
    int num_classes = classes->num_classes;
    int *true_class_indices = (int *) calloc(num_documents, sizeof(int));
    float *doc_P_of_class = (float *) calloc(num_classes, sizeof(float));
    float *word_P_of_class = (float *) calloc(num_classes, sizeof(float));

    for ( d=0; d<num_documents; d++ ) {
      vector = vectors[d];
      true_class_indices[d] = vector->class_id;
      doc_P_of_class[vector->class_id] += 1.0;
      word_P_of_class[vector->class_id] += vector->total_sum;
    }
    denom = (float)num_documents;
    for ( t=0; t<num_classes; t++ ) {
      doc_P_of_class[t] = doc_P_of_class[t]/denom;
      word_P_of_class[t] = word_P_of_class[t]/total_word_count;
    }

    plsa_model->class_indices = true_class_indices;
    plsa_model->doc_P_of_class = doc_P_of_class;
    plsa_model->word_P_of_class = word_P_of_class;

  }
  printf("done)\n");

  return plsa_model;
  

}


PLSA_MODEL *copy_plsa_model ( PLSA_MODEL *plsa_model_orig )
{
  if ( plsa_model_orig == NULL ) return NULL;

  PLSA_MODEL *plsa_model_copy = (PLSA_MODEL *) malloc ( sizeof(PLSA_MODEL) );
  
  int num_topics = plsa_model_orig->num_topics;
  int num_features = plsa_model_orig->num_features;
  int num_documents = plsa_model_orig->num_documents;

  plsa_model_copy->num_topics = num_topics;
  plsa_model_copy->num_features = num_features;
  plsa_model_copy->num_documents = num_documents;
  plsa_model_copy->alpha = plsa_model_orig->alpha;
  plsa_model_copy->beta = plsa_model_orig->beta;
  
  float **P_z_given_d = (float **) copy2d( (char **)plsa_model_orig->P_z_given_d, num_topics, 
					   num_documents, sizeof(float));
  plsa_model_copy->P_z_given_d = P_z_given_d;

  float **P_w_given_z = (float **) copy2d( (char **)plsa_model_orig->P_w_given_z, num_features, 
					   num_topics, sizeof(float));
  plsa_model_copy->P_w_given_z = P_w_given_z;

  plsa_model_copy->global_word_scores = NULL; // This is not part of the model so don't copy it
  
  
  return plsa_model_copy;

}

void free_plsa_model ( PLSA_MODEL *plsa_model )
{
  if ( plsa_model == NULL ) return;

  if ( plsa_model->P_z_given_d != NULL ) free2d((char **)plsa_model->P_z_given_d);
  if ( plsa_model->P_w_given_z != NULL ) free2d((char **)plsa_model->P_w_given_z);
  free(plsa_model);

  return;

}


// Perform EM estimation of pre-initialized PLSA model on data
void estimate_plsa_model ( PLSA_MODEL *plsa_model, SPARSE_FEATURE_VECTORS *feature_vectors, 
			   float alpha, float beta, int max_iter, float conv_threshold,
			   int ignore_set, int verbose )
{

  if ( conv_threshold < 0 ) die("Convergence threshold can not be negative\n");

  int num_topics = plsa_model->num_topics;
  int num_features = plsa_model->num_features;
  int num_documents = plsa_model->num_documents;

  if ( feature_vectors->num_vectors != num_documents )
    die ("ERROR in estimate_plsa_model: # of feature vectors (%d) != # of documents (%d)!?!\n",
	 feature_vectors->num_vectors, num_documents);

  int i, d,w,z;

  SPARSE_FEATURE_VECTOR **vectors = feature_vectors->vectors;
  SPARSE_FEATURE_VECTOR *vector;
  float **P_z_given_d = plsa_model->P_z_given_d;
  float **P_w_given_z = plsa_model->P_w_given_z;
  float total_num_w = 0;

  if ( verbose ) printf("(Training %d topic PLSA model...",num_topics); fflush(stdout);

  time_t start_time, end_time;
  time(&start_time);

  // Compute initial likelihood
  float denom;
  float tmp;

  float num_w_in_d = 0;
  float L = 0.0;

  for ( d=0; d<num_documents; d++ ) {
    vector = vectors[d];
    if ( ignore_set == -1 || vector->set_id != ignore_set ) {
      denom = 0;
      for ( i=0; i<vector->num_features; i++ ) {
	w = vector->feature_indices[i];
	num_w_in_d = vector->feature_values[i];
	tmp = 0;
	for ( z=0; z<num_topics; z++ ) {
	  tmp += P_w_given_z[w][z] * P_z_given_d[z][d];
	}
	L += num_w_in_d * logf(tmp);
      }
      total_num_w += vector->total_sum;
    }
  }
  L = L/total_num_w;
  //printf("%.3f...",L);fflush(stdout);
  float prev_L = L;

  // Set up variables for iterative PLSA training
  float **new_P_z_given_d = (float **) calloc2d ( num_topics, num_documents, sizeof(float));
  float **new_P_w_given_z = (float **) calloc2d( num_features, num_topics, sizeof(float));

  float **tmp_P_z_given_d, **tmp_P_w_given_z;
  int iter;
  float *P_z_given_d_w = (float *)calloc(num_topics, sizeof(float));
  int stop = 0;
  int stop_count = 0;

  // Do iterative PLSA training
  for ( iter=0; iter<max_iter && !stop; iter++ ) {
    if ( verbose) printf("%d...", iter); fflush(stdout);

    // Do EM updates for this iteration

    // Initialize P'(w|z) with the beta smoothing parameter
    for ( z=0; z<num_topics; z++ ) {
      for ( w=0; w<num_features; w++ ) {
	new_P_w_given_z[w][z] = beta;
      }
    }      

    // Loop through documents
    for ( d=0; d<num_documents; d++ ) {
      vector = vectors[d];
      if ( ignore_set == -1 || vector->set_id != ignore_set ) {
	// Initialize P'(z|d) with the alpha smoothing parameter
	for ( z=0; z<num_topics; z++ ) { 
	  new_P_z_given_d[z][d] = alpha;
	}

	// Loop through word features w in this document d
	for ( i=0; i<vector->num_features; i++ ) {
	  w = vector->feature_indices[i];
	  num_w_in_d = vector->feature_values[i];

	  // Learn P(z|d,w) for each topic z
	  denom = 0;
	  for ( z=0; z<num_topics; z++ ) {
	     P_z_given_d_w[z]  = P_w_given_z[w][z] * P_z_given_d[z][d];
	     denom += P_z_given_d_w[z];
	  }
	  for ( z=0; z<num_topics; z++ ) P_z_given_d_w[z] = P_z_given_d_w[z]/denom;
	  
	  // Incorporate statistics collected from this w and d
	  for ( z=0; z<num_topics; z++ ) {
	    tmp = num_w_in_d * P_z_given_d_w[z];
	    new_P_w_given_z[w][z] += tmp;
	    new_P_z_given_d[z][d] += tmp;
	  }
	}

	// Do final normalization for P'(z|d)
	denom = 0;
	for ( z=0; z<num_topics; z++ ) denom += new_P_z_given_d[z][d];
	for ( z=0; z<num_topics; z++ ) new_P_z_given_d[z][d] = new_P_z_given_d[z][d]/denom;

      }
    }
    
    // Do final normalization for P'(w|z)
    for ( z=0; z<num_topics; z++ ) {
      denom = 0;
      for ( w=0; w<num_features; w++ ) denom += new_P_w_given_z[w][z];
      for ( w=0; w<num_features; w++ ) new_P_w_given_z[w][z] = new_P_w_given_z[w][z]/denom;
    }
    
    // Swap the working space and stored model pointers 
    tmp_P_z_given_d = P_z_given_d;
    tmp_P_w_given_z = P_w_given_z;
    P_z_given_d = new_P_z_given_d;
    P_w_given_z = new_P_w_given_z;
    new_P_z_given_d = tmp_P_z_given_d;
    new_P_w_given_z = tmp_P_w_given_z;
    plsa_model->P_z_given_d = P_z_given_d;
    plsa_model->P_w_given_z = P_w_given_z;

    // Compute the likelihood for this iteration
    L = 0;
    for ( d=0; d<num_documents; d++ ) {
      vector = vectors[d];
      if ( ignore_set == -1 || vector->set_id != ignore_set ) {
	denom = 0;
	for ( i=0; i<vector->num_features; i++ ) {
	  w = vector->feature_indices[i];
	  num_w_in_d = vector->feature_values[i];
	  tmp = 0;
	  for ( z=0; z<num_topics; z++ ) tmp += P_w_given_z[w][z] * P_z_given_d[z][d];
	  L += num_w_in_d * logf(tmp);
	}
      }
    }
    L = L/total_num_w;			       
    // printf("%.3f...",L);fflush(stdout);

    // Check if convergence criterion has been reached.
    // The likelihood change must stay below the convergance
    // threshold for 10 straight iterations
    if ( L - prev_L < conv_threshold ) { 
      stop_count++;
    } else if ( stop_count > 0 ) {
      stop_count--;
    }
    if ( stop_count >= 10 ) stop = 1;
    prev_L = L;
  }
  
  // Estimated P(z) by document count
  //for (z=0; z<num_topics; z++ ) P_z[z] = 0;

  // Estimated P(z) by word count
  estimate_P_z_in_plsa_model(plsa_model);

  /*
  float num_in_train_set = alpha*((float)num_topics);
  for (z=0; z<num_topics; z++ ) P_z[z] = alpha;
  for ( d=0; d<num_documents; d++ ) {
    vector = vectors[d];
    for (z=0; z<num_topics; z++ ) {
      P_z[z] += vector->total_sum * P_z_given_d[z][d];
    }
    num_in_train_set += vector->total_sum;
  }
  for (z=0; z<num_topics; z++ ) {
    P_z[z] = P_z[z]/num_in_train_set;
  }
  */

  /*
  // Estimate the held-out set likelihood here
  float L_d = 0.0;
  if ( ignore_set != -1 ) {
    // First estimate P_Z over training set
    float *P_z = (float *) calloc(num_topics, sizeof(float));
    float num_in_train_set = 0;
    for ( d=0; d<num_documents; d++ ) {
      vector = vectors[d];
      if ( vector->set_id != ignore_set ) {
	for (z=0; z<num_topics; z++ ) {
	  P_z[z] += vector->total_sum * P_z_given_d[z][d];
	}
	num_in_train_set += vector->total_sum;
      }
    }
    for (z=0; z<num_topics; z++ ) {
      P_z[z] = P_z[z]/num_in_train_set;
    }
    
    // Initialize P_z_given_d for held-out set
    for ( d=0; d<num_documents; d++ ) {
      vector = vectors[d];
      if ( vector->set_id == ignore_set ) {
	for (z=0; z<num_topics; z++ ) {
	  P_z_given_d[z][d] = P_z[z];
	}
      }
    }
    
    // Do inference on held out set to compute held-out likelihood
    total_num_w = 0;
    float *numer_P_z_given_d = P_z; // reuse already allocated P_z vector
    for ( d=0; d<num_documents; d++ ) {
      vector = vectors[d];
      if ( vector->set_id == ignore_set ) {
	int prev_L_d=0;
	int stop_iter = 0;
	for ( iter = 0; !stop_iter; iter++ ) {
	  L_d = 0;
	  for ( z=0; z<num_topics; z++ ) numer_P_z_given_d[z] = 0;
	  for ( i=0; i<vector->num_features; i++ ) {
	    w = vector->feature_indices[i];
	    num_w_in_d = vector->feature_values[i];
	    denom = 0;
	    for ( z=0; z<num_topics; z++ ) {
	      P_z_given_d_w[z] +	      tmp = P_w_given_z[w][z] * P_z_given_d[z][d];
	      denom += P_w_given_z[w][z] * P_z_given_d[z][d];
	    for ( z=0; z<num_topics; z++ ) {
	      P_z_given_d_w[z]  = P_w_given_z[w][z] * P_z_given_d[z][d] / denom;
	      numer_P_z_given_d[z] += num_w_in_d * P_z_given_d_w;
	    }
	    L_d += num_w_in_d * logf(denom);
	  }
	  for ( z=0; z<num_topics; z++ ) {
	    P_z_given_d[z][d] = numer_P_z_given_d[z]/vector->total_sum;
	  }
	  if ( iter>0 ) {
	    if ( iter>0 && ((L_d - prev_L_d)/vector->total_sum < .0001) ) stop_iter = 1;
	  }
	  prev_L_d = L_d;
	}	
	L += L_d;
	total_num_w += vector->total_sum;
      }
    }
    
    L = L/total_num_w;
    free(P_z);
  }
  */

  time(&end_time);
  if ( verbose ) {
    double total_time = difftime(end_time,start_time);
    double avg_time = total_time/((double)iter);
    printf("done in %d seconds...",(int)total_time);
    printf("avg time per iteration=%.1f seconds...",avg_time);
    printf("avg likelihood=%.6f over %.3f total words)\n",L,total_num_w);
  }
  plsa_model->avg_likelihood = L;
  plsa_model->total_likelihood = L*total_num_w;
  plsa_model->total_words = total_num_w;
  
  free2d((char**)new_P_z_given_d);
  free2d((char**)new_P_w_given_z);

  return;
}


static void estimate_P_z_in_plsa_model ( PLSA_MODEL *plsa_model )
{
  int num_topics = plsa_model->num_topics;
  int num_documents = plsa_model->num_documents;
  float alpha = plsa_model->alpha;
  if ( plsa_model->P_z == NULL ) {
    plsa_model->P_z = (float *) calloc(num_topics, sizeof(float));
  } 
  float *P_z =  plsa_model->P_z;
  float **P_z_given_d =  plsa_model->P_z_given_d;
  float num_in_train_set = alpha*((float)num_topics);
  int z, d;
  float count;
  for (z=0; z<num_topics; z++ ) P_z[z] = alpha;
  for ( d=0; d<num_documents; d++ ) {
    count = plsa_model->num_words_in_d[d];
    for (z=0; z<num_topics; z++ ) {
      P_z[z] += count * P_z_given_d[z][d];
    }
    num_in_train_set += count;
  }
  for (z=0; z<num_topics; z++ ) {
    P_z[z] = P_z[z]/num_in_train_set;
  }

  return;

}

/**********************************************************************/

static void estimate_P_w_in_plsa_model ( PLSA_MODEL *plsa_model )
{
  int num_topics = plsa_model->num_topics;
  int num_features = plsa_model->num_features;
  if ( plsa_model->P_z == NULL )  estimate_P_z_in_plsa_model(plsa_model);
  float *P_z =  plsa_model->P_z;
  if ( plsa_model->P_w == NULL ) {
    plsa_model->P_w = (float *) calloc(num_features, sizeof(float));
  } 
  float *P_w =  plsa_model->P_w;
  float **P_w_given_z =  plsa_model->P_w_given_z;
  int w, z;
  for ( w=0; w<num_features; w++ ) {
    for (z=0; z<num_topics; z++ ) {
      P_w[w] += P_w_given_z[w][z] * P_z[z];
    }
  }

  return;

}

/**********************************************************************/


void write_plsa_model_to_file( char *fileout, PLSA_MODEL *plsa_model )
{
  FILE *fp = fopen_safe(fileout, "w");
  dump_float(plsa_model->alpha, fp);
  dump_float(plsa_model->beta, fp);
  dump_2d_float_array(plsa_model->P_w_given_z, plsa_model->num_features,
		      plsa_model->num_topics, fp);
  dump_2d_float_array(plsa_model->P_z_given_d, plsa_model->num_topics,
		      plsa_model->num_documents, fp);
  dump_strings ( plsa_model->features->feature_names, plsa_model->num_features, fp);
  dump_float_array ( plsa_model->num_words_in_d, plsa_model->num_documents, fp);
  dump_float_array ( plsa_model->P_w, plsa_model->num_features, fp);
  dump_float_array ( plsa_model->P_z, plsa_model->num_topics, fp);
  fclose(fp);
  return;
}

PLSA_MODEL *load_plsa_model_from_file( char *filein )
{
  PLSA_MODEL *plsa_model = (PLSA_MODEL *) calloc(1, sizeof(PLSA_MODEL));
  int num_features, num_topics, num_documents;

  FILE *fp = fopen_safe(filein, "r");
  plsa_model->alpha = load_float(fp);
  plsa_model->beta = load_float(fp);
  plsa_model->P_w_given_z = load_2d_float_array( &num_features, &num_topics, fp);
  plsa_model->num_features = num_features;
  plsa_model->num_topics = num_topics;
  plsa_model->P_z_given_d = load_2d_float_array( &num_topics, &num_documents, fp);
  plsa_model->num_documents = num_documents;
  if ( plsa_model->num_topics != num_topics ) 
    die ("load_plsa_model_from_file: # topics in P(w|z) (%d) != # topics in P(z|d) (%d)?!?\n",
	 plsa_model->num_topics, num_topics);
  char **feature_names = load_strings ( &num_features, fp );
  if ( plsa_model->num_features != num_features ) 
    die ("load_plsa_model_from_file: # featuress in P(w|z) (%d) != # features in feature list (%d)?!?\n",
	 plsa_model->num_topics, num_topics);

  HASHTABLE *hash =  hdbmcreate( (unsigned)num_features, hash2);
  int i;
  for ( i=0; i<num_features; i++ ) {
    store_hashtable_string_index (hash, feature_names[i], i);
  }
  
  FEATURE_SET *features = (FEATURE_SET *) malloc(sizeof(FEATURE_SET));
  features->num_features = num_features;
  features->feature_names = feature_names;
  features->feature_weights = (float *)calloc(num_features, sizeof(float));
  features->feature_name_to_index_hash = hash;
  plsa_model->features = features;

  plsa_model->num_words_in_d = (float *) calloc( num_documents, sizeof(float));
  if ( fread(  plsa_model->num_words_in_d, sizeof(float), num_documents, fp) != num_documents ) {
    free(plsa_model->num_words_in_d);
    plsa_model->num_words_in_d = NULL;
    warn("PLSA input model file does not contain word counts.\n");
  } 
  fclose(fp);

  // If possible count the total number of words in the document collection
  plsa_model->total_words = 0.0;
  if ( plsa_model->num_words_in_d != NULL ) {
    int d;
    for ( d=0; d<num_documents; d++ ) plsa_model->total_words += plsa_model->num_words_in_d[d];
  } else {
    plsa_model->total_words = -1.0;
  }

  // Add P(z) and P(w) estimates into model
  estimate_P_z_in_plsa_model(plsa_model);
  estimate_P_w_in_plsa_model(plsa_model);

  // Initialize everything else to NULL for now
  plsa_model->z_mapping = NULL;
  plsa_model->z_inverse_mapping = NULL;
  plsa_model->classes = NULL;
  plsa_model->class_indices = NULL;
  plsa_model->doc_P_of_class = NULL;
  plsa_model->word_P_of_class = NULL;
  plsa_model->global_word_scores = NULL;
  plsa_model->avg_likelihood = 0.0;
  plsa_model->total_likelihood = 0.0;

  return plsa_model;
}


/**********************************************************************/

void write_plsa_posteriors_to_file(char *fileout, PLSA_MODEL *plsa_model )
{
  
  FILE *fp = fopen_safe(fileout, "w");
  dump_2d_float_array(plsa_model->P_z_given_d, plsa_model->num_topics,
		      plsa_model->num_documents, fp);
  fclose(fp);
  return;
}


float **load_plsa_posteriors_from_file( char *filein, int *num_topics_ptr, int *num_docs_ptr )
{
  FILE *fp = fopen_safe(filein, "r");
  float **array = load_2d_float_array( num_topics_ptr, num_docs_ptr, fp);
  fclose(fp);

  return array;
}

/**********************************************************************/

void write_plsa_unigram_models_to_file(char *fileout, PLSA_MODEL *plsa_model )
{
  
  FILE *fp = fopen_safe(fileout, "w");
  dump_2d_float_array(plsa_model->P_w_given_z, plsa_model->num_features,
		      plsa_model->num_topics, fp);
  fclose(fp);
  return;
}

float **load_plsa_unigram_models_from_file( char *filein, int *num_features_ptr, int *num_topics_ptr )
{
  FILE *fp = fopen_safe(filein, "r");
  float **array = load_2d_float_array( num_features_ptr, num_topics_ptr, fp);
  fclose(fp);

  return array;
}

/**********************************************************************/


// Computes H(z) from plsa model
float compute_plsa_topic_entropy ( PLSA_MODEL *plsa_model )
{
  int num_latent_topics = plsa_model->num_topics;
  int num_documents = plsa_model->num_documents;
  float **P_z_given_d = plsa_model->P_z_given_d;
  float *P_z = (float *) calloc ( num_latent_topics, sizeof(float) );
  float H_z = 0;
  int d, z;
  float N_d = (float)num_documents;

  // Compute P(z)
  for ( d=0; d<num_documents; d++ ) {
    for ( z=0; z<num_latent_topics; z++ ) {
      P_z[z] += P_z_given_d[z][d]/N_d;
    }
  }

  // Compute H(z)
  for ( z=0; z<num_latent_topics; z++ ) {
    if ( P_z[z] > 0.0 ) {
      H_z += - P_z[z] * logf ( P_z[z] );
    }
  }
  free(P_z);

  return H_z;

}

/**********************************************************************/

float **compute_joint_latent_truth_counts ( PLSA_MODEL *plsa_model )
{

  CLASS_SET *classes = plsa_model->classes;
  if ( classes == NULL ) die("True class set not specified in PLSA model\n");

  int *class_indices = plsa_model->class_indices;
  if ( class_indices == NULL ) die("True class indices for data not specified in PLSA model\n");

  int num_true_topics = classes->num_classes;
  int num_latent_topics = plsa_model->num_topics;
  int num_documents = plsa_model->num_documents;
  float **P_z_given_d = plsa_model->P_z_given_d;

  float **latent_truth_counts = (float **) calloc2d(num_latent_topics, 
						    num_true_topics,
						    sizeof(float));
  int d,z,t;
  for ( d=0; d<num_documents; d++ ) {
    t = class_indices[d];
    for ( z=0; z<num_latent_topics; z++ ) {
      latent_truth_counts[z][t] += plsa_model->num_words_in_d[d] * P_z_given_d[z][d];
    }
  }
  
  return latent_truth_counts;

}


// Compute aggregate P(z|t) over document collection
float **map_plsa_to_truth ( PLSA_MODEL *plsa_model )
{
  if ( plsa_model->classes == NULL ) {
    die ("PLSA model does not contain mapping of document to truth topics\n");
  }

  CLASS_SET *classes = plsa_model->classes;
  int num_true_topics = classes->num_classes;
  int num_latent_topics = plsa_model->num_topics;
  float **latent_to_truth_mapping = compute_joint_latent_truth_counts ( plsa_model );

  float sum;
  int z, t;
  for ( z=0; z<num_latent_topics; z++ ) {
    sum = 0;
    for ( t=0; t<num_true_topics; t++ ) {
      sum += latent_to_truth_mapping[z][t];
    }
    for ( t=0; t<num_true_topics; t++ ) {
      latent_to_truth_mapping[z][t] = latent_to_truth_mapping[z][t]/sum;
    }
  }

  return latent_to_truth_mapping;

}

float **map_truth_to_plsa ( PLSA_MODEL *plsa_model )
{
  if ( plsa_model->classes == NULL ) {
    die ("PLSA model does not contain mapping of document to truth topics\n");
  }

  CLASS_SET *classes = plsa_model->classes;
  int num_true_topics = classes->num_classes;
  int num_latent_topics = plsa_model->num_topics;

  float **tmp = compute_joint_latent_truth_counts ( plsa_model );
  float **truth_to_latent_mapping = (float **) calloc2d(num_true_topics,
							num_latent_topics, 
							sizeof(float));
  int z,t;
  for ( t=0; t<num_true_topics; t++ ) {
    for ( z=0; z<num_latent_topics; z++ ) {
      truth_to_latent_mapping[t][z] = tmp[z][t];
    }
  }
  free2d((char **)tmp);
  
  float sum;
  for ( t=0; t<num_true_topics; t++ ) {
    sum = 0;
    for ( z=0; z<num_latent_topics; z++ ) {
      sum += truth_to_latent_mapping[t][z];
    }
    for ( z=0; z<num_latent_topics; z++ ) {
      truth_to_latent_mapping[t][z] = truth_to_latent_mapping[t][z]/sum;
    }
  }

  return truth_to_latent_mapping;

}

/**********************************************************************/

// Compute similarity matrix for documents in PLSA
// If log_dist is set, similarity is converted to distance using -log()
float **compute_similarity_matrix_from_plsa_model ( PLSA_MODEL *plsa_model, int log_dist )
{
  int num_topics = plsa_model->num_topics;
  int num_documents = plsa_model->num_documents;
  float **P_z_given_d = plsa_model->P_z_given_d;

  float **matrix = (float **)calloc2d(num_documents, num_documents, sizeof(float));
  
  int i, j, z;
  float min=1.0;
  
  for ( i=0; i<num_documents; i++ ) {
    matrix[i][i] = 1.0;
    for ( j=i+1; j<num_documents; j++ ) {
      for ( z=0; z<num_topics; z++ ) {
	matrix[i][j] += P_z_given_d[z][i] * P_z_given_d[z][j];
      }
      matrix[j][i] = matrix[i][j];
      if ( matrix[i][j] > 0 && matrix[i][j] < min ) min = matrix[i][j];
    }
  }

  if ( log_dist ) {
    float max = - 1.25 * logf(min);
    for ( i=0; i<num_documents; i++ ) {
      matrix[i][i] = 0.0;
      for ( j=i+1; j<num_documents; j++ ) {
	if ( matrix[i][j] == 0.0 ) matrix[i][j] = max;
	else matrix[i][j] = -logf(matrix[i][j]);
	matrix[j][i] = matrix[i][j];
      }
    }
  }

  return matrix;

}

LINEAR_CLASSIFIER *train_naive_bayes_classifier_over_plsa_topics ( SPARSE_FEATURE_VECTORS *feature_vectors,
								   PLSA_MODEL *plsa_model ) 
{
  printf("(Training naive Bayes classifier...\n"); fflush(stdout);

  // Extract training data info
  SPARSE_FEATURE_VECTOR **vectors = feature_vectors->vectors;
  FEATURE_SET *features = feature_vectors->feature_set;
  int num_documents = feature_vectors->num_vectors;
  int num_features = features->num_features;
  int num_topics = plsa_model->num_topics;
  float **P_z_given_d = plsa_model->P_z_given_d;
  float **P_w_given_z = plsa_model->P_w_given_z;

  float **topic_counts = (float **) calloc2d (num_topics, num_features, sizeof(float));
  float **not_topic_counts = (float **) calloc2d (num_topics, num_features, sizeof(float));
  float *global_counts = (float *) calloc (num_features, sizeof(float));
  float tau = 1.0;
  int index;
  float value;
  SPARSE_FEATURE_VECTOR *vector;
  int i, d, w, z;
  float count;
  float sum;

  // Collect counts from feature vectors
  for ( d = 0; d < num_documents; d++ ) {
    vector = vectors[d];
    for ( i = 0; i < vector->num_features; i++ ) {
      w = vector->feature_indices[i];
      count = vector->feature_values[i];
      global_counts[w] += count;
      for ( z=0, sum=0; z<num_topics; z++ ) {
	sum += P_w_given_z[w][z] * P_z_given_d[z][d];
      }
      for ( z=0; z<num_topics; z++ ) {
	topic_counts[z][w] += count *  P_w_given_z[w][z] * P_z_given_d[z][d] / sum;
      }
    }
  }
  
  // Compute counts of features not in each class
  for (z=0; z<num_topics; z++) {
    for ( w=0; w<num_features; w++) {
      not_topic_counts[z][w] = global_counts[w] - topic_counts[z][w];
    }
  }

  // Get MAP estimate of prior likelihoods of features
  float *Pmap_w 
    = compute_MAP_estimated_distribution_with_uniform_prior ( global_counts, num_features, tau );
  
  // Get MAP estimate of class and anti-class dependent likelihoods of features
  float **Pmap_w_given_z = (float **) calloc (num_topics, sizeof(float *));
  float **Pmap_w_given_not_z = (float **) calloc (num_topics, sizeof(float *));
  for ( z = 0; z<num_topics; z++ ) {
    Pmap_w_given_z[z] 
      = compute_MAP_estimated_distribution ( topic_counts[z], Pmap_w, num_features, tau );
    Pmap_w_given_not_z[z]
      = compute_MAP_estimated_distribution ( not_topic_counts[z], Pmap_w, num_features, tau );
  }

  // Create log likelihood ratio scores for features: P(F|C)/P(F|!C)
  float **matrix = (float **) calloc2d(num_topics, num_features, sizeof(float));
  for ( z=0; z<num_topics; z++ ) {
    for ( w=0; w<num_features; w++ ) {
      matrix[z][w] = logf(Pmap_w_given_z[z][w]/Pmap_w_given_not_z[z][w]);
    }
  }
  
  int num_summary_words = 10;
  int *best_words_index = (int *) calloc(num_summary_words, sizeof(int));
  float *best_words_value = (float *) calloc(num_summary_words, sizeof(float));
  int stop;
  for ( z=0; z<num_topics; z++ ) {
    for ( i=0; i<num_summary_words; i++ ) {
      best_words_index[i] = -1;
    }
    for ( w=0; w<num_features; w++ ) {
      i = num_summary_words-1;
      index = w;
      value = matrix[z][w]*topic_counts[z][w];
      stop = 1;
      if ( best_words_index[i] == -1 || best_words_value[i] < matrix[z][w] ) {
	best_words_index[i] = index;
	best_words_value[i] = value;
	if ( i > 0 ) stop = 0;
	while ( !stop ) {
	  if ( best_words_index[i-1] == -1 || best_words_value[i-1] < value ) {
	    best_words_index[i] = best_words_index[i-1];
	    best_words_value[i] = best_words_value[i-1];
	    best_words_index[i-1] = index;
	    best_words_value[i-1] = value;
	    i--;
	    if ( i == 0 ) stop = 1;
	  } else {
	    stop = 1;
	  }
	}
      } 
    }
    printf ("Topic %d:",z);
    for ( i=0; i<num_summary_words; i++ ) {
      w = best_words_index[i];
      printf(" %s", features->feature_names[w]);
    }
    printf("\n");
  }

  // Populate the linear classifier structure
  LINEAR_CLASSIFIER *classifier = (LINEAR_CLASSIFIER *) malloc(sizeof(LINEAR_CLASSIFIER));
  classifier->num_classes = num_topics;
  classifier->num_features = num_features;
  classifier->offsets = (float *) calloc(num_topics,sizeof(float));
  classifier->matrix = matrix;
  classifier->features = features;
  classifier->classes = (CLASS_SET *) malloc;
  
  // Clean up
  free2d((char **)topic_counts);
  free2d((char **)not_topic_counts);
  free(global_counts);
  for ( z = 0; z<num_topics; z++ ) {
    free(Pmap_w_given_z[z]);
    free(Pmap_w_given_not_z[z]);
  }
  free(Pmap_w_given_z);
  free(Pmap_w_given_not_z);
  free(Pmap_w);

  printf("done)\n");

  return classifier;

}

// This function computes various evaluation metrics
// comparing the PLSA clusters and the true topic labels
// IC(Z,T) = I(Z,T)-H(Z|T)/H(T)
PLSA_EVAL_METRICS *compute_plsa_to_truth_metrics ( PLSA_MODEL *plsa_model )
{
  CLASS_SET *classes = plsa_model->classes;
  int num_true_topics = classes->num_classes;
  int num_latent_topics = plsa_model->num_topics;
  int num_documents = plsa_model->num_documents;
  float **P_z_given_d = plsa_model->P_z_given_d;
  float *P_z = (float *) calloc ( num_latent_topics, sizeof(float) );
  float *P_t = (float *) calloc ( num_true_topics, sizeof(float) );
  float **P_z_t = (float **) calloc2d(num_latent_topics, num_true_topics, sizeof(float));
  int d,z,t;
  float N_d = (float)num_documents;
  float log_scale = 1.0/logf(2.0);

  if ( plsa_model->class_indices == NULL ) 
    die ("Can't evaluate PLSA model with topic labels\n");


  // Compute P(z,t), P(z), and P(t)
  int *topic_indices = plsa_model->class_indices;
  for ( d=0; d<num_documents; d++ ) {
    t = topic_indices[d];
    P_t[t] += 1/N_d;
    for ( z=0; z<num_latent_topics; z++ ) {
      P_z_t[z][t] += P_z_given_d[z][d]/N_d;
      P_z[z] += P_z_given_d[z][d]/N_d;
    }
  }
  
  // Compute I(Z,T), the mutual information of Z and T
  float I_z_t = 0;
  for ( z=0; z<num_latent_topics; z++ ) {
    for ( t=0; t<num_true_topics; t++ ) {
      if ( P_z_t[z][t] > 0.0 ) {
	I_z_t += P_z_t[z][t] * log_scale * logf ( P_z_t[z][t] / ( P_z[z] * P_t[t] ) );
      }
    }
  }
  free2d((char **)P_z_t);

  // Compute H(z), the entropy of Z
  float H_z = compute_distribution_entropy ( P_z, num_latent_topics);
  free(P_z);

  // Compute H(t), the entropy of T
  float H_t = compute_distribution_entropy ( P_t, num_true_topics);
  free(P_t);

  // Fill in the metrics into the structure to be returned
  PLSA_EVAL_METRICS *eval_metrics = (PLSA_EVAL_METRICS *) malloc(sizeof(PLSA_EVAL_METRICS));
  eval_metrics->H_T = H_t;
  eval_metrics->H_Z = H_z;
  eval_metrics->I = I_z_t;
  eval_metrics->Pzt = I_z_t/H_z;    // This is the "precision" of the latent topic to true topic mapping
  eval_metrics->Ptz = I_z_t/H_t;    // This is the "recall" of the true topic to latent topic mapping
  eval_metrics->NMI = (2*I_z_t)/(H_z+H_t);   // This is the information theory equivalent to F-score
  eval_metrics->IC = ((2*I_z_t)-H_z)/H_t;
  eval_metrics->P = sqrt(eval_metrics->Pzt * eval_metrics->Ptz);
  return eval_metrics;

}

float compute_distribution_entropy ( float *P, int num ) 
{
  int i;
  float log_scale = 1.0/logf(2.0);
  float H = 0;

  for ( i=0; i<num; i++ ) {
    if ( P[i] > 0.0 ) H += - P[i] * log_scale * logf ( P[i] );
  }

  return H;
}


PLSA_SUMMARY *summarize_plsa_model ( PLSA_MODEL *plsa_model, int stem_list )
{  

  // Extract training data info
  int num_documents = plsa_model->num_documents;
  CLASS_SET *classes = plsa_model->classes;

  FEATURE_SET *features = plsa_model->features;
  int num_features = features->num_features;
  int num_topics = plsa_model->num_topics;        // Number of PLSA latent topics in Z (not true topics in T)
  float **P_z_given_d = plsa_model->P_z_given_d;
  float **P_w_given_z = plsa_model->P_w_given_z;
  int num_summary_words = 10;                     // Hard-coded for now...should this be an input variable?

  // If the model doesn't yet contain P_z(z) then compute it
  if ( plsa_model->P_z == NULL ) estimate_P_z_in_plsa_model(plsa_model);
  float *P_z = plsa_model->P_z;

  // If the model doesn't yet contain P_w(w) then compute it
  if ( plsa_model->P_w == NULL ) estimate_P_w_in_plsa_model(plsa_model);
  float *P_w = plsa_model->P_w;

  // Create output structure
  PLSA_SUMMARY *summary = (PLSA_SUMMARY *) malloc(sizeof(PLSA_SUMMARY));
  summary->features = features ;
  summary->classes = classes ;
  summary->num_topics = num_topics; // Number of PLSA latent topics in Z (not true topics in T)
  summary->num_summary_features = num_summary_words;
  summary->summary_features = (int **) calloc2d(num_topics, num_summary_words, sizeof(int));
  summary->P_z = NULL;
  summary->z_to_D_purity = NULL;
  summary->z_score = NULL;
  summary->z_to_T_purity = (float *) calloc(num_topics, sizeof(float));
  summary->Z_to_T_mapping = NULL;
  summary->T_to_Z_mapping = NULL;

  int i, d, w, z, t;

  // Create the structure for ranking words by topical importance
  IV_PAIR_ARRAY *iv_pair_array = create_iv_pair_array (num_features);
  IV_PAIR **global_word_scores =  iv_pair_array->pairs;
  plsa_model->global_word_scores = global_word_scores;
  
  // If we have known truth topic labels T for the data create the mappings from Z to T
  float **latent_to_truth_mapping = NULL;
  float **truth_to_latent_mapping = NULL;
  float *P_of_class =  NULL;
  float H_T = 0;
  if ( classes != NULL) {
    latent_to_truth_mapping = map_plsa_to_truth ( plsa_model );
    truth_to_latent_mapping = map_truth_to_plsa ( plsa_model );
    P_of_class = plsa_model->doc_P_of_class;
    H_T = compute_distribution_entropy ( P_of_class, classes->num_classes);
    summary->Z_to_T_mapping = latent_to_truth_mapping;
    summary->T_to_Z_mapping = truth_to_latent_mapping;
  } 

  // Compute latent topic statistics for each z in Z
  float *doc_purity = (float *) calloc(num_topics, sizeof(float));
  float *topic_score = (float *) calloc(num_topics, sizeof(float));
  float prob;
  float log_scale = 1.0/logf(2.0);
  for ( z=0; z<num_topics; z++ ) {
    // Compute the Z->D purity score for this topic
    float num=0;
    float den=0;
    for ( d=0; d<num_documents; d++ ) {
      if ( P_z_given_d[z][d] > 0.0 ) {
	num += P_z_given_d[z][d] * log_scale * logf (P_z_given_d[z][d]);
	den += P_z_given_d[z][d];
      }
    }
    if ( num != 0.0 ) {
      doc_purity[z] = powf(2.0, (num/den));
    } else {
      doc_purity[z] = 1.0;
    }
    // Compute the total topical importance score for this topic
    topic_score[z] = 100 * P_z[z] * doc_purity[z];
  }

  summary->P_z = copy_float_array(P_z, num_topics);
  summary->z_to_D_purity = doc_purity;
  summary->z_score = topic_score;

  // Create signature word summaries for each z in Z
  SIG_WORDS *signature_words = create_signature_words_struct (2*num_summary_words);
  float H_T_given_z;
  char *stem;
  for ( z=0; z<num_topics; z++ ) {
    if ( latent_to_truth_mapping != NULL ) {
      // Compute H(T|z)
      H_T_given_z = 0;
      for ( t=0; t<classes->num_classes; t++ ) {
	prob = latent_to_truth_mapping[z][t];
	if ( prob > 0.0 ) H_T_given_z +=  - prob * log_scale * logf(prob); 
      }
      // Compute topical purity: ( H(T) - H(T|z) ) / H(T);
      summary->z_to_T_purity[z] = ( H_T - H_T_given_z ) / H_T;
    }

    // For each topic, find words which maximize I(w;z)
    clear_signature_words_struct ( signature_words );
    float score;
    for ( w=0; w<num_features; w++ ) {
      // Score this word by I(w;z)
      float P_z_and_w = P_w_given_z[w][z] * P_z[z];
      if ( P_z_and_w == 0 ) {
	score = 0;
      } else {
	score = P_z_and_w * logf ( P_w_given_z[w][z] / P_w[w] );
      }
      global_word_scores[w]->value += score;

      // Sort into signature word list
      stem = strdup(features->feature_names[w]);
      porter_stem_string(stem);
      bubble_sort_word_into_sig_word_list ( w, score, stem, signature_words );
      free(stem);
    }
    remove_substrings_from_sig_word_list ( signature_words, features );
    remove_substrings_from_sig_word_list ( signature_words, features );
    for ( i=0; i<num_summary_words; i++ ) {
      if ( i<signature_words->num_words ) {
	// Store feature in summary structure
	w = signature_words->word_indices[i];
	summary->summary_features[z][i] = w;
      } else {
	summary->summary_features[z][i] = -1;
      }
    }
  }

  // Create list of latent topics sorted by z score
  float *sorted_scores = (float *) calloc(num_topics, sizeof(float));
  int *index_map = (int *) calloc (num_topics, sizeof(int));
  for (z=0; z<num_topics; z++) {
    index_map[z] = z;
    sorted_scores[z] = summary->z_score[z];
  }
  int z1, z2;
  float score;
  for ( z1=0; z1<num_topics-1; z1++) {
    for ( z2=num_topics-1; z2>z1; z2-- ) {
      if ( sorted_scores[z2] > sorted_scores[z1] ) {
	z = index_map[z2];
	score = sorted_scores[z2];
	index_map[z2] = index_map[z1];
	sorted_scores[z2] = sorted_scores[z1];
	index_map[z1] = z;
	sorted_scores[z1] = score;
      }
    }
  }
  summary->sorted_topics = index_map;
  plsa_model->z_mapping = index_map;
  free(sorted_scores);

  // Clean up
  free_signature_words_struct ( signature_words );

  return summary;


}

void print_plsa_summary ( PLSA_SUMMARY *summary, int eval_topics, char *file_out )
{
  
  FEATURE_SET *features = summary->features;
  CLASS_SET *classes = summary->classes;
  float **latent_to_truth_mapping = summary->Z_to_T_mapping;
  int num_topics = summary->num_topics;
  int num_summary_features = summary->num_summary_features;
  int z;

  // Sort latent topics by the topic scores sorting function
  float *sorted_scores = (float *) calloc(num_topics, sizeof(float));
  int *index_map = (int *) calloc (num_topics, sizeof(int));
  for (z=0; z<num_topics; z++) {
    index_map[z] = z;
    sorted_scores[z] = summary->z_score[z];
  }
  int z1, z2;
  float score;
  for ( z1=0; z1<num_topics-1; z1++) {
    for ( z2=num_topics-1; z2>z1; z2-- ) {
      if ( sorted_scores[z2] > sorted_scores[z1] ) {
	z = index_map[z2];
	score = sorted_scores[z2];
	index_map[z2] = index_map[z1];
	sorted_scores[z2] = sorted_scores[z1];
	index_map[z1] = z;
	sorted_scores[z1] = score;
      }
    }
  }

  FILE *fp = stdout;
  if ( file_out != NULL ) {
    printf ("(Writing summary to %s...", file_out);
    fp = fopen_safe ( file_out, "w" );
  }

  // Print out the characterization of the document collection
  fprintf(fp,"***********************************\n");
  fprintf(fp,"*** Document Collection Summary ***\n");
  fprintf(fp,"***********************************\n");
  fprintf(fp,"---- ------ ----- ------ ----- ");
  if ( eval_topics ) fprintf(fp,"------ ");
  fprintf(fp," ----------------\n");
  fprintf(fp,"            Topic    Doc  %% of ");
  if ( eval_topics ) fprintf (fp," Topic"); 
  fprintf(fp,"\n");
  fprintf(fp,"   #  Index Score Purity  Docs ");
  if ( eval_topics ) fprintf (fp,"Purity "); 
  fprintf(fp," Summary\n");
  fprintf(fp,"---- ------ ----- ------ ----- ");
  if ( eval_topics ) fprintf(fp,"------ ");
  fprintf(fp," ----------------\n");
  
  int i, f, w, t;
  for ( i=0; i<num_topics; i++ ) {
    z = index_map[i];
    fprintf (fp,"%4d (%4d) %5.2f  %5.3f %5.2f ", 
	     i+1, z, summary->z_score[z], summary->z_to_D_purity[z], 100*summary->P_z[z] );
    if ( eval_topics ) fprintf (fp," %5.3f ", summary->z_to_T_purity[z]);
    for ( f=0; f<num_summary_features; f++ ) {
      w = summary->summary_features[z][f];
      if ( w != -1 ) fprintf(fp," %s", features->feature_names[w]);
    }
    fprintf(fp,"\n");

    // If we have truth labels print out the actual
    // topics associated with this latent topic
    if ( eval_topics && latent_to_truth_mapping != NULL ) {
      int best_t=0;
      float count = latent_to_truth_mapping[z][0];
      float count_sum = count;
      float max_count = count;
      for ( t=1; t<classes->num_classes; t++ ) {
	count = latent_to_truth_mapping[z][t];
	count_sum += count;
	if ( count > max_count ) {
	  best_t = t;
	  max_count = count;
	}
      }
      float threshold = .075;
      fprintf(fp, "                                       True topics (%%): ");
      fprintf(fp, " %s:%.1f", 
	      classes->class_names[best_t],
	      100*latent_to_truth_mapping[z][best_t]/count_sum);
      for ( t=0; t<classes->num_classes; t++ ) {
	if ( t != best_t && latent_to_truth_mapping[z][t]/count_sum >= threshold ) {
	  fprintf(fp, " %s:%.1f", 
		  classes->class_names[t],
		  100*latent_to_truth_mapping[z][t]/count_sum);
	}
      }
      fprintf(fp,"\n");
    }
  }
  if ( file_out != NULL ) {
    printf("done)\n");
    fclose(fp);
  }

}

static SIG_WORDS *create_signature_words_struct ( int num_words ) 
{
  SIG_WORDS *signature_words = (SIG_WORDS *) malloc (sizeof(SIG_WORDS));
  
  signature_words->num_words = 0;
  signature_words->num_allocated = num_words;
  signature_words->word_indices = (int *) calloc ( num_words, sizeof(int));
  signature_words->word_scores = (float *) calloc ( num_words, sizeof(float));
  signature_words->word_stems = (char **) calloc ( num_words, sizeof(char *));

  return signature_words;

}

static void clear_signature_words_struct ( SIG_WORDS *signature_words ) 
{
  int i;
  
  signature_words->num_words = 0;
  for ( i=0; i<signature_words->num_allocated; i++ ) {
    signature_words->word_indices[i] = -1;
    signature_words->word_scores[i] = 0;
    if ( signature_words->word_stems[i] != NULL ) {
      free(signature_words->word_stems[i]);
      signature_words->word_stems[i] = NULL;
    }
  }
  return;
}

static void free_signature_words_struct ( SIG_WORDS *signature_words ) 
{
  int i;
  
  if ( signature_words == NULL ) return;

  if ( signature_words->word_stems != NULL ) {
    for ( i=0; i<signature_words->num_allocated; i++ )
      if ( signature_words->word_stems[i] != NULL )
	free(signature_words->word_stems[i]);
    free(signature_words->word_stems );
  }

  if ( signature_words->word_indices != NULL ) 
    free(signature_words->word_indices);

  if ( signature_words->word_scores != NULL ) 
    free(signature_words->word_scores);

  free (signature_words);
  return;
}

static void bubble_sort_word_into_sig_word_list ( int index, float score, char *stem, 
						  SIG_WORDS *signature_words )
{
  int num_words = signature_words->num_words;            // Number of words currently in list
  int num_allocated = signature_words->num_allocated;    // Max number of words allowed in list
  int *word_indices = signature_words->word_indices;
  float *word_scores = signature_words->word_scores;
  char **word_stems = signature_words->word_stems;

  // The current signature word list is presorted...
  // so if the list is full and the new word isn't better
  // scoring than the lowest scoring word in the list
  // then just return and do nothing
  if ( ( num_words == num_allocated ) && 
       ( score < word_scores[num_words-1] ) ) {
    return;
  }

  if ( stem == NULL ) die("Word stem is NULL in bubble_sort_word_into_sig_word_list\n");

  // Go through existing list to see if the current 
  // word is a stem match with a word in the list
  // or if there is an empty slot in the list
  int i=0;
  int stop = 0;
  char *tmp;
  while ( i < num_allocated && !stop ) {
    // Check to see if we've hit an empty slot in the list
    // and, if so, insert the word into the list
    if ( word_indices[i] == -1 ) { 
      word_indices[i] = index;
      word_scores[i] = score;
      if ( word_stems[i] != NULL ) free(word_stems[i]);
      word_stems[i] = strdup(stem);
      stop = 1; 
      signature_words->num_words = i+1;
    } 
    
    // Check if we've hit a word with the same stem
    else if ( strcmp ( stem, word_stems[i] ) == 0 ) {
      if ( score > word_scores[i] ) {
	// Put current word in place of lower scoring 
	// word having the same stem.
	word_indices[i] = index;
	word_scores[i] = score;
      } else {
	// A better scoring word with the same stem exists
	// so ignore this word and return
	return;
      }
      stop = 1;
    } 
    
    // Otherwise move on to the next item in the list
    else { i++; }
  } 

  // If this is a word with a new stem see if it needs to be added into the list
  if ( i == num_allocated ) {
    i--;

    // If the word is not better than anything in current list then return
    if ( score < word_scores[i] ) return;
    
    // Otherwise replace the last word in the list with this new word
    word_indices[i] = index;
    word_scores[i] = score;
    if ( word_stems[i] != NULL ) free(word_stems[i]);
    word_stems[i] = strdup(stem);
  }

  // Now, if needed, bubble sort the word up the list
  while ( i > 0 && score > word_scores[i-1] ) {
    tmp = word_stems[i];
    word_stems[i] = word_stems[i-1];
    word_indices[i] = word_indices[i-1];
    word_scores[i] = word_scores[i-1];
    word_stems[i-1] = tmp;
    word_indices[i-1] = index;
    word_scores[i-1] = score;
    i--;
  } 

  return;

}

static void remove_substrings_from_sig_word_list ( SIG_WORDS *signature_words, FEATURE_SET *features )
{
  int num_words = signature_words->num_words;
  int *word_indices = signature_words->word_indices;
  float *word_scores = signature_words->word_scores;
  char **word_stems = signature_words->word_stems;

  int i, j, k;
  int w_i, w_j;
  int ss;

  for ( i=0; i<num_words; i++ ) {
    w_i = word_indices[i];
  }  

  for ( i=0; i<num_words-1; i++ ) {
    for ( j=i+1; j<num_words; j++ ) {
      w_i = word_indices[i];
      w_j = word_indices[j];
      ss = substring ( w_j, w_i, features );
      if ( ss == -1 ) {
	// w_i is a substring of w_j so move w_j into w_i's slot
	// This step is potentially more dangerous...should we do it?
	word_indices[i] = word_indices[j];
	word_scores[i] = word_scores[j];
	if ( word_stems[i] != NULL ) free(word_stems[i]);
	word_stems[i] = word_stems[j];;
	word_stems[j] = NULL;
      }
      
      if ( ss != 0 ) {
	// Delete w_j from the list
	// If ss == 1 then w_j is a substring 
	// of w_i so keep w_i and delete w_j
	// If ss == -1 then we have already moved
	// w_j into w_i's slot
	if ( word_stems[j] != NULL ) free(word_stems[j]);
	for ( k=j; k<num_words-1; k++ ) {
	  word_indices[k] = word_indices[k+1];
	  word_scores[k] = word_scores[k+1];
	  word_stems[k] = word_stems[k+1];
	}
	word_indices[k] = -1;
	word_scores[k] = -1;
	word_stems[k] = NULL;
	num_words--;
      }
    }
  }

  signature_words->num_words = num_words;
  return;

}


static int substring (int i, int j, FEATURE_SET *features) 
{
  int length_i = strlen(features->feature_names[i]);
  int length_j = strlen(features->feature_names[j]);
  if ( length_i == length_j ) return 0;
  char *string_i = (char *) calloc(length_i+5, sizeof(char));
  char *string_j = (char *) calloc(length_j+5, sizeof(char));
  sprintf(string_i, "_%s_", features->feature_names[i]);
  sprintf(string_j, "_%s_", features->feature_names[j]);
  if ( ( length_i > length_j ) && ( strstr(string_i, string_j) != NULL ) ) return -1;
  else if ( ( length_j > length_i ) && ( strstr(string_j, string_i) != NULL ) ) return 1;
  return 0;
}

void write_topically_ranked_words_to_file ( PLSA_MODEL *plsa_model, char *file_out ) 
{
  FEATURE_SET *features = plsa_model->features;
  int num_features = features->num_features;
  IV_PAIR **global_word_scores = plsa_model->global_word_scores;
  int i, w;
  float score;
  float *P_w = plsa_model->P_w;
  float total_words = plsa_model->total_words;

  if ( global_word_scores == NULL ) {
    die("Can't compute topical ranking of words without first doing PLSA summarization\n");
  }

  qsort(global_word_scores, num_features, sizeof(IV_PAIR *), cmp_iv_pair);

  if ( file_out != NULL ) {
    FILE *fp = fopen_safe(file_out, "w"); 
    for ( i=0; i<num_features; i++ ) {
      w = global_word_scores[i]->index;
      score = global_word_scores[i]->value;
      fprintf(fp, "%s %.8f %.3f\n",features->feature_names[w], score, P_w[w]*total_words);
    }
    fclose(fp);
  }
  
  printf("--------------------------------------------------------------------\n");
  
  printf("Top 50 Globally important topic words:\n");
  for ( i=0; i<50; i++ ) {
    w = global_word_scores[i]->index;
    score = global_word_scores[i]->value;
    printf("%3d score=%.6f count=%6.2f word=%s\n",i+1,score,
	   P_w[w]*total_words,features->feature_names[w]);
  }

  printf("--------------------------------------------------------------------\n");

  return;

}

