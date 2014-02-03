/* -*- C -*-
 *
 * Copyright (c) 1994, 2004, 2008
 * Massachusetts Institute of Technology
 *
 * All Rights Reserved
 *
 *	FILE: basic_util.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "util/basic_util.h"

static int cmp_float_decreasing(const void *a, const void *b);
static int cmp_float_increasing(const void *a, const void *b);

char **split_string (char *string_in, const char *split_delim, int *num_out)
{
  char *string = strdup(string_in);
  char *next;
  int count=0;
  char **strings_out = NULL;
  *num_out = 0;

  // First count how many substrings will reults from the split
  next = strtok(string, split_delim);
  if ( next == NULL ) return NULL;
  while ( next != NULL ) {
    count++;
    next = strtok(NULL, split_delim);
  }

  // Allocate an array for the substrings
  strings_out = (char **) calloc(count, sizeof(char *));
  *num_out = count;

  // Reparse the string and extract the substrings
  count = 0;
  string = strdup(string_in);
  next = strtok(string, split_delim);
  while ( next != NULL ) {
    strings_out[count] = strdup(next);
    count++;
    next = strtok(NULL, split_delim);
  }

  free(string);

  return strings_out;

}


int count_lines_in_file (FILE *fp, int *max_line_length)
{
  int c;
  int line_count=0;
  int char_count = 0;
  int max_char_count = 0;

  rewind(fp);
  while ( (c=fgetc(fp)) != EOF ) {
    char_count++;
    if ( c == '\n' ) {
      line_count++;
      if ( char_count > max_char_count ) {
	max_char_count = char_count;
      }
      char_count = 0;
    }
  }
  if ( char_count > max_char_count ) {
    max_char_count = char_count;
  }
  rewind(fp);

  if ( max_line_length != NULL ) *max_line_length = max_char_count;
  return line_count;
  
}

/* Opens a file, exiting if it fails. */
FILE *fopen_safe(const char *filename, const char *mode)
{
  FILE *fp;

  fp = fopen(filename, mode);
  if ( fp == NULL ) {
    fprintf(stderr, "\nError: Couldn't open file '%s' for '%s'\n\n", filename, mode);
    exit(-1);
  }
  return (fp);
}

/* Reads data from a file, exiting if it fails. */
void fread_safe(void *ptr, size_t size, size_t count, FILE *fp)
{
  if ( fread( ptr, size, count, fp) != count) {
    fprintf ( stderr, "\nError: Couldn't fread from file\n\n");
    exit(-1);
  }
  return;
}

/* Writes data to a file, exiting if it fails. */
void fwrite_safe(void *ptr, size_t size, size_t count, FILE *fp)
{
  if ( fwrite( ptr, size, count, fp) != count) {
    fprintf ( stderr, "\nError: Couldn't fwrite to file\n\n");
    exit(-1);
  }
  return;
}

/********************************************************************/

void dump_strings ( char **strings, int num_strings, FILE *fp )
{
  dump_int ( num_strings, fp );
  dump_string_array ( strings, num_strings, fp );
  return;
}

char **load_strings ( int *num_strings, FILE *fp )
{
  int num;
  num = load_int ( fp );
  if ( num < 0 || isnan(num) || isinf(num) ) 
    die ("load_strings: improper number of strings to load: %d\n",num);  
  char **strings = load_string_array ( num, fp );
  *num_strings = num;
  return strings;
}

void dump_string_array ( char **strings, int num_strings, FILE *fp )
{
  int i;
  for ( i=0; i<num_strings; i++ ) dump_string(strings[i], fp);
  return;
}

char **load_string_array ( int num_strings, FILE *fp )
{
  int i;
  char **strings = (char **) calloc(num_strings, sizeof(char *));
  for ( i=0; i<num_strings; i++ ) strings[i] = load_string(fp);
  return strings;
}

void dump_string ( char *string, FILE *fp ) {
  int len;
  if (string == NULL) {
    dump_int(-1, fp);
  } else {
    len = strlen(string);
    dump_int(len, fp);
    fwrite_safe(string, sizeof(char), len + 1, fp);
  }
  return;
}

char *load_string( FILE *fp )
{ 
  int len = load_int(fp);
  if ( len < 0 || isnan(len) || isinf(len) ) 
    die ("load_string: improper number of strings to load: %d\n",len);
  char *string = (char *) calloc (len+2, sizeof(char));
  fread_safe(string, sizeof(char), len + 1, fp);
  return string;
}

/********************************************************************/

void dump_int ( int value, FILE *fp ) 
{
  fwrite_safe( &value, sizeof(int), 1, fp );
  return;
}

int load_int ( FILE *fp ) 
{
  int value;
  fread_safe( &value, sizeof(int), 1, fp );
  return value;
}

void dump_float ( float value, FILE *fp ) 
{
  fwrite_safe( &value, sizeof(float), 1, fp );
  return;
}

float load_float ( FILE *fp ) 
{
  float value;
  fread_safe( &value, sizeof(float), 1, fp );
  return value;
}

void dump_float_array ( float *values, int length, FILE *fp ) 
{
  fwrite_safe(values, sizeof(float), length, fp);
  return;
}

float *load_float_array ( int length, FILE *fp ) 
{
  float *values = (float *) calloc(length, sizeof(float));
  fread_safe(values, sizeof(float), length, fp);
  return values;
}

float *copy_float_array ( float *array, int length ) 
{
  float *new_array = (float *) calloc(length, sizeof(float));
  memcpy(new_array, array, length*sizeof(float));
  return new_array;
}

void dump_2d_float_array(float **array, int dim1, int dim2, FILE *fp) {
  dump_int(dim1, fp);
  dump_int(dim2, fp);
  if (dim1 > 0 && dim2 > 0) {
    fwrite_safe(*array, sizeof(float), dim1 * dim2, fp);
  }
  return;
}

float **load_2d_float_array(int *dim1_ptr, int *dim2_ptr, FILE *fp) 
{
  int dim1, dim2;
  float **array;

  dim1 = load_int(fp);
  dim2 = load_int(fp);

  if ( dim1 < 0 || isnan(dim1) || isinf(dim1) ) 
    die ("load_2d_float_array: Bad value for dimension 1: %d\n",dim1);

  if ( dim2 < 0 || isnan(dim2) || isinf(dim2) ) 
    die ("load_2d_float_array: Bad value for dimension 2: %d\n",dim2);

  array = (float **)calloc2d(dim1, dim2, sizeof(float));
  fread_safe(*array, sizeof(float), dim1*dim2, fp);

  if (dim1_ptr != NULL) *dim1_ptr = dim1;
  if (dim2_ptr != NULL) *dim2_ptr = dim2;

  return array;
}

/********************************************************************/

char **calloc2d(int ndim1, int ndim2, int size)
{
  int i;		
  unsigned nelem;	
  char *p;
  char **pp;

  /* Compute total number of elements needed for the */
  /* two-dimensional matrix */
  nelem = (unsigned) ndim1 * ndim2;

  /* Allocate the memory needed for the matrix */
  p = (char *) calloc(nelem, (unsigned) size);

  /* If the allocation were not successful, return a NULL pointer */
  if (p == NULL)
    return (NULL);

  /* Now allocate a table for referencing the matrix memory */
  pp = (char **) calloc((unsigned) ndim1, (unsigned) sizeof(char *));

  /* If the allocation were not successful, return a NULL */
  /* pointer and free the previously allocated memory */
  if (pp == NULL) {
    free(p);
    return (NULL);
  }

  /* Fill the table with locations to where each row begins */
  for (i = 0; i < ndim1; i++)
    pp[i] = p + (i * ndim2 * size);

  return((char **) pp);
}

/********************************************************************/

char **copy2d(char **orig, int ndim1, int ndim2, int size) 
{

  /* Allocate the new 2d array*/
  char **copy = (char **) calloc2d( ndim1, ndim2, size );
  if ( copy == NULL ) return NULL;
  
  /* Compute total size of the 2d array */
  unsigned matrix_size = (unsigned) ( ndim1 * ndim2 * size);

  /* Copy the orig matrix into the new matrix */
  memcpy(*copy, *orig, matrix_size);

  return((char **) copy);

}

/********************************************************************/

void free2d(char **matrix)
{
  /* Free the matrix */
  if (matrix != NULL && *matrix != NULL)
    free(*matrix);
  if (matrix != NULL)
    free(matrix);
  return;
}

/********************************************************************/

void sort_float_array ( float *array, int num, int decreasing )
{
  if ( decreasing ) qsort(array, num, sizeof(float), cmp_float_decreasing );
  else qsort(array, num, sizeof(float), cmp_float_increasing );
  return;
}


static int cmp_float_increasing(const void *a, const void *b)
{
    const float *ia = (const float *)a;
    const float *ib = (const float *)b;
    if ( *ia  > *ib ) return 1; 
    else if ( *ia  < *ib ) return -1; 
    else return 0;
}

static int cmp_float_decreasing(const void *a, const void *b)
{
    const float *ia = (const float *)a;
    const float *ib = (const float *)b;
    if ( *ia  < *ib ) return 1; 
    else if ( *ia  > *ib ) return -1; 
    else return 0;
}



/********************************************************************/

/* Fatal errors and warnings */

void die(char *format, ...)
{
  va_list ap;
  va_start(ap,format);
  fprintf (stderr, "Error: ");
  vfprintf(stderr,format,ap);
  va_end(ap);
  exit (-1);
}

void warn(char *format, ...)
{
  va_list ap;
  va_start(ap,format);
  fprintf (stderr, "Warning: ");
  vfprintf(stderr,format,ap);
  va_end(ap);
  return;
}
