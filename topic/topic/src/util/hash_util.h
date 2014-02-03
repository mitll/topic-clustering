/* -*- C -*-
 *
 * Copyright (c) 2009
 * Massachusetts Institute of Technology
 *
 * All Rights Reserved
 *
 * FILE: hash_util.h
 *
 */


/* Note: This file includes function prototypes and structure
   definitions used by code in hash_util.c in which is copyrighted
   by the The Regents of the University of California. See 
   hash_util.c for the conditions on use and redistribution
   for the functions contained in hash_util.c */

#ifndef HASH_UTIL_INCLUDED
#define HASH_UTIL_INCLUDED

#include <stdio.h>
#include <stdarg.h>

#define HDBMDATUM struct hdbmdatum
HDBMDATUM {
	char	*dat_ptr;
	unsigned dat_len;
};

#ifndef HASHTABLE
#define HASHTABLE struct hdbmtable
#endif

extern HASHTABLE *hdbmcreate(unsigned, unsigned (*)(HDBMDATUM));
extern void      hdbmdestroy(HASHTABLE*);
extern void      hdbmwalk(HASHTABLE*, void (*)(HDBMDATUM, HDBMDATUM, char*), char*);
extern int       hdbmstore(HASHTABLE*, HDBMDATUM, HDBMDATUM);
extern int       hdbmdelete(HASHTABLE*, HDBMDATUM);
extern HDBMDATUM hdbmfetch(HASHTABLE*, HDBMDATUM);
extern HDBMDATUM hdbmentry(HASHTABLE*, HDBMDATUM, HDBMDATUM (*)(HDBMDATUM));

unsigned int hash1(HDBMDATUM key);
unsigned int hash2(HDBMDATUM key);
unsigned int hash3(HDBMDATUM key);
unsigned int hash4(HDBMDATUM key);
unsigned int compute_hash(const void* p, unsigned int len, unsigned int seed);

void store_hashtable_string_index (HASHTABLE *ht, char *string, int index);
int  get_hashtable_string_index (HASHTABLE *ht, char *string);
void fill_in_string_array_with_hash_entries (HASHTABLE *ht, char **strings, int num_strings );

#endif  /* HASH_UTIL_INCLUDED */

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

