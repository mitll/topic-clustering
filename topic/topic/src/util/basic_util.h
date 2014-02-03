/* -*- C -*-
 *
 * Copyright (c) 2008
 * Massachusetts Institute of Technology
 *
 * All Rights Reserved
 *
 * FILE: basic_util.h
 *
 */

#ifndef BASIC_UTIL_INCLUDED
#define BASIC_UTIL_INCLUDED

FILE *fopen_safe(const char *file, const char *direction);
void fread_safe(void *ptr, size_t size, size_t count, FILE *stream);
void fwrite_safe(void *ptr, size_t size, size_t count, FILE *stream);

void dump_strings ( char **strings, int num_strings, FILE *fp );
void dump_string_array ( char **strings, int num_strings, FILE *fp );
void dump_string(char *string, FILE *fp);

char *load_string(FILE *fp);
char **load_string_array ( int num_strings, FILE *fp );
char **load_strings ( int *num_strings, FILE *fp );

void dump_int(int value, FILE *fp);	
int load_int(FILE *fp);

void dump_float(float value, FILE *fp);	
float load_float(FILE *fp);
void dump_float_array(float *values, int length, FILE *fp);
float *load_float_array(int length, FILE *fp); 
float *copy_float_array (float *array, int length);
void dump_2d_float_array(float **array, int dim1, int dim2, FILE *fp);
float **load_2d_float_array(int *dim1_ptr, int *dim2_ptr, FILE *fp);

char **calloc2d(int dim1, int dim2, int size);
char **copy2d(char **orig, int dim1, int dim2, int size);
void free2d(char **matrix);

char **split_string (char *string_in, const char *split_delim, int *num_out);
int count_lines_in_file (FILE *fp, int *max_line_length);

void sort_float_array ( float *array, int num, int decreasing );

void die(char *format, ...);
void warn(char *format, ...);

#endif  /* BASIC_UTIL_INCLUDED */

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

