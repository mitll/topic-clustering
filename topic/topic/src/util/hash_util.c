/* -*- C -*-
 *
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CNTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)hash_func.c	5.2 (Berkeley) 9/4/91";
#endif /* LIBC_SCCS and not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "util/hash_util.h"


#ifdef WIN32
typedef unsigned char u_char;
#endif

/******************************* HASH FUNCTIONS **************************/
/*
 * Assume that we've already split the bucket to which this key hashes,
 * calculate that bucket, and check that in fact we did already split it.
 *
 * This came from ejb's hsearch.
 */

#define PRIME1		37
#define PRIME2		1048583

unsigned int
hash1(HDBMDATUM hkey)
{
	register int h;
	register u_char *key = (u_char *)hkey.dat_ptr;
	register int len = hkey.dat_len;


	h = 0;
	/* Convert string to integer */
	while (len--)
		h = h * PRIME1 ^ (*key++ - ' ');
	h %= PRIME2;
	return ((unsigned int) h);
}

/*
 * Phong's linear congruential hash
 */
#define dcharhash(h, c)	((h) = 0x63c63cd9*(h) + 0x9c39c33d + (c))

unsigned int
hash2(HDBMDATUM hkey)
{
	register u_char *e, c;
	register int h;
	register u_char *key = (u_char *)hkey.dat_ptr;
	register int len = hkey.dat_len;

	e = key + len;
	for (h = 0; key != e;) {
		c = *key++;
		if (!c && key > e)
			break;
		dcharhash(h, c);
	}
	return ((unsigned int) h);
}

/*
 * This is INCREDIBLY ugly, but fast.  We break the string up into 8 byte
 * units.  On the first time through the loop we get the "leftover bytes"
 * (strlen % 8).  On every other iteration, we perform 8 HASHC's so we handle
 * all 8 bytes.  Essentially, this saves us 7 cmp & branch instructions.  If
 * this routine is heavily used enough, it's worth the ugly coding.
 *
 * OZ's original sdbm hash
 */
unsigned int
hash3(HDBMDATUM hkey)
{
	register int n, loop;
	register u_char *key = (u_char *)hkey.dat_ptr;
	register int len = hkey.dat_len;

#define HASHC   n = *key++ + 65599 * n

	n = 0;
	if (len > 0) {
		loop = (len + 8 - 1) >> 3;

		switch (len & (8 - 1)) {
		case 0:
			do {	/* All fall throughs */
				HASHC;
		case 7:
				HASHC;
		case 6:
				HASHC;
		case 5:
				HASHC;
		case 4:
				HASHC;
		case 3:
				HASHC;
		case 2:
				HASHC;
		case 1:
				HASHC;
			} while (--loop);
		}

	}
	return ((unsigned int) n);
}

/* Hash function from Chris Torek. */
unsigned int
hash4(HDBMDATUM hkey)
{
	register int h, loop;
	register u_char *key = (u_char *)hkey.dat_ptr;
	register int len = hkey.dat_len;

#define HASH4a   h = (h << 5) - h + *key++;
#define HASH4b   h = (h << 5) + h + *key++;
#define HASH4 HASH4b

	h = 0;
	if (len > 0) {
		loop = (len + 8 - 1) >> 3;

		switch (len & (8 - 1)) {
		case 0:
			do {	/* All fall throughs */
				HASH4;
		case 7:
				HASH4;
		case 6:
				HASH4;
		case 5:
				HASH4;
		case 4:
				HASH4;
		case 3:
				HASH4;
		case 2:
				HASH4;
		case 1:
				HASH4;
			} while (--loop);
		}

	}
	return ((unsigned int) h);
}

unsigned int
compute_hash(const void* p, unsigned int len, unsigned int seed)
{
    unsigned const char *cp = (unsigned const char *)p;
    unsigned int i, x = seed;
    
    for (i = 0; i < len; i++)
        x = 0x63c63cd9*x + 0x9c39c33d + cp[i];

    return x;
}

/********************************************************************************
 *
 * The following code (with some minor changes) comes from 
 * uxc.cso.uiuc.edu:/usenet/c-news/libc/hdbm.c
 *
 *******************************************************************************/

/*-------------------------------------------------------------------------*/
/* uxc.cso.uiuc.edu:/usenet/c-news/libc/hdbmint.h (internals)              */

#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

#define BADTBL(tbl)	(((tbl)->ht_magic&BYTEMASK) != HASHMAG)

#define HASHMAG  0257
#define BYTEMASK 0377

#define HASHENT struct hashent

HASHENT {
	HASHENT	*he_next;		/* in hash chain */
	HDBMDATUM he_key;		/* to verify a match */
	HDBMDATUM he_data;
};

HASHTABLE {
	HASHENT **ht_addr;		/* array of HASHENT pointers */
	unsigned ht_size;
	char	ht_magic;
	unsigned (*ht_hash)(HDBMDATUM hdbmdatum);
};

static void hefree(HASHENT *hp);
static HASHENT *healloc();

/*-------------------------------------------------------------------------*/

/* tunable parameters */
#define RETAIN 300              /* retain & recycle this many HASHENTs */

/* fundamental constants */
#define YES 1
#define NO 0

static HASHENT *hereuse = NULL;
static int reusables = 0;

/* size is a crude guide to size */
HASHTABLE *hdbmcreate(unsigned size, unsigned (*hashfunc)(HDBMDATUM key))
{
  register HASHTABLE *tbl;
  register HASHENT **hepp;
  /*
   * allocate HASHTABLE and (HASHENT *) array together to reduce the
   * number of malloc calls.  this idiom ensures correct alignment of
   * the array.
   * dmr calls the one-element array trick `unwarranted chumminess with
   * the compiler' though.
   */
  register struct alignalloc {
    HASHTABLE ht;
    HASHENT *hepa[1];	/* longer than it looks */
  } *aap;

  aap = (struct alignalloc *)
    malloc(sizeof *aap + ((size==0?1:size)-1)*sizeof(HASHENT *));
  if (aap == NULL)
    return NULL;
  tbl = &aap->ht;
  tbl->ht_size = (size == 0? 1: size);	/* size of 0 is nonsense */
  tbl->ht_magic = (char)HASHMAG;
  tbl->ht_hash = hashfunc;

  if (hashfunc == NULL) {
    fprintf (stderr, "Warning: hdbmcreate: hash function cannot be NULL -- aborting");
    return NULL;
  }

  tbl->ht_addr = hepp = aap->hepa;
  while (size-- > 0)
    hepp[size] = NULL;
  return tbl;
}

/*
 * free all the memory associated with tbl, erase the pointers to it, and
 * invalidate tbl to prevent further use via other pointers to it.
 */
void hdbmdestroy(register HASHTABLE *tbl)
{
  register unsigned idx;
  register HASHENT *hp, *next;
  register HASHENT **hepp;
  register unsigned int tblsize;

  if (tbl == NULL || BADTBL(tbl))
    return;
  tblsize = tbl->ht_size;
  hepp = tbl->ht_addr;
  for (idx = 0; idx < tblsize; idx++) {
    for (hp = hepp[idx]; hp != NULL; hp = next) {
      next = hp->he_next;
      hp->he_next = NULL;
      hefree(hp);
    }
    hepp[idx] = NULL;
  }

  tbl->ht_magic = 0;		/* de-certify this table */
  tbl->ht_addr = NULL;

  free((char *)tbl);
}

/*
 * The returned value is the address of the pointer that refers to the
 * found object.  Said pointer may be NULL if the object was not found;
 * if so, this pointer should be updated with the address of the object
 * to be inserted, if insertion is desired.
 */
static HASHENT **hdbmfind(HASHTABLE *tbl, HDBMDATUM key)
{
  register HASHENT *hp, *prevhp = NULL;
  register char *hpkeydat, *keydat = key.dat_ptr;
  register int keylen = key.dat_len;
  register HASHENT **hepp;
  register unsigned size; 

  if (BADTBL(tbl))
    return NULL;
  size = tbl->ht_size;
  if (size == 0)			/* paranoia: avoid division by zero */
    size = 1;
  hepp = &tbl->ht_addr[(*tbl->ht_hash)(key) % size];
  for (hp = *hepp; hp != NULL; prevhp = hp, hp = hp->he_next) {
    hpkeydat = hp->he_key.dat_ptr;
    if ((int)hp->he_key.dat_len == keylen && hpkeydat[0] == keydat[0] &&
	memcmp(hpkeydat, keydat, keylen) == 0)
      break;
  }
  /* assert: *(returned value) == hp */
  return (prevhp == NULL? hepp: &prevhp->he_next);
}

/* allocate a hash entry */
static HASHENT *healloc()
{
  register HASHENT *hp;

  if (hereuse == NULL)
    return (HASHENT *)malloc(sizeof(HASHENT));
  /* pull the first reusable one off the pile */
  hp = hereuse;
  hereuse = hereuse->he_next;
  hp->he_next = NULL;			/* prevent accidents */
  reusables--;
  return hp;
}

void hefree(register HASHENT *hp)     /* free a hash entry */
{
  if (reusables >= RETAIN) {  		/* compost heap is full? 	*/
    free((char *)hp);			  	/* yup, just pitch this one 	*/
  } else {					/* no, just stash for reuse 	*/
    ++reusables;
    hp->he_next = hereuse;
    hereuse = hp;
  }
}

int hdbmstore(HASHTABLE *tbl, HDBMDATUM key, HDBMDATUM data)
{
  register HASHENT *hp;
  register HASHENT **nextp;

  if (BADTBL(tbl))
    return NO;
  nextp = hdbmfind(tbl, key);
  if (nextp == NULL)
    return NO;
  hp = *nextp;
  if (hp == NULL) {			/* absent; allocate an entry */
    hp = healloc();
    if (hp == NULL)
      return NO;
    hp->he_next = NULL;
    hp->he_key = key;
    *nextp = hp;			/* append to hash chain */
  }
  hp->he_data = data;	/* supersede any old data for this key */
  return YES;
}

/* return any existing entry for key; otherwise call allocator to make one */
HDBMDATUM hdbmentry(HASHTABLE *tbl, HDBMDATUM key, HDBMDATUM (*allocator)(HDBMDATUM))
{
  register HASHENT *hp;
  register HASHENT **nextp;
  static HDBMDATUM errdatum = { NULL, 0 };

  if (BADTBL(tbl))
    return errdatum;
  nextp = hdbmfind(tbl, key);
  if (nextp == NULL)
    return errdatum;
  hp = *nextp;
  if (hp == NULL) {			/* absent; allocate an entry */
    hp = healloc();
    if (hp == NULL)
      return errdatum;
    hp->he_next = NULL;
    hp->he_key = key;
    hp->he_data = (*allocator)(key);
    *nextp = hp;			/* append to hash chain */
  }
  return hp->he_data;
}

int hdbmdelete(HASHTABLE *tbl, HDBMDATUM key)
{
  register HASHENT *hp;
  register HASHENT **nextp;

  nextp = hdbmfind(tbl, key);
  if (nextp == NULL)
    return NO;
  hp = *nextp;
  if (hp == NULL)				/* absent */
    return NO;
  *nextp = hp->he_next;			/* skip this entry */
  hp->he_next = NULL;
  hp->he_data.dat_ptr = hp->he_key.dat_ptr = NULL;
  hefree(hp);
  return YES;
}

/* data corresponding to key */
HDBMDATUM hdbmfetch(HASHTABLE *tbl, HDBMDATUM key)
{
  register HASHENT *hp;
  register HASHENT **nextp;
  static HDBMDATUM errdatum = { NULL, 0 };

  if (BADTBL(tbl))
    return errdatum;
  nextp = hdbmfind(tbl, key);
  if (nextp == NULL)
    return errdatum;
  hp = *nextp;
  if (hp == NULL)				/* absent */
    return errdatum;
  else
    return hp->he_data;
}

/*
 * visit each entry by calling nodefunc at each, with key, data and hook as
 * arguments.  hook is an attempt to allow side-effects and reentrancy at
 * the same time.
 */
void hdbmwalk(HASHTABLE *tbl, void (*nodefunc)(HDBMDATUM, HDBMDATUM, char *), register char *hook)
{
  register unsigned idx;
  register HASHENT *hp;
  register HASHENT **hepp;
  register unsigned int tblsize;

  if (BADTBL(tbl))
    return;
  hepp = tbl->ht_addr;
  tblsize = tbl->ht_size;
  for (idx = 0; idx < tblsize; idx++)
    for (hp = hepp[idx]; hp != NULL; hp = hp->he_next)
      (*nodefunc)(hp->he_key, hp->he_data, hook);
}

/**************************************************************************/

/* The following functions are wrapper functions added by T. J. Hazen
   at MIT Lincoln Laboratory for basic string to index hash mapping 
   operations */

/* --------------------------------------------------------
   This function stores a string,index pair in a hashtable.
   -------------------------------------------------------- */
void store_hashtable_string_index (HASHTABLE *ht, char *string, int index)
{
  HDBMDATUM index_datum, string_datum; 
  
  int *ptr = (int *) malloc(sizeof(int));		
  ptr[0] = index;

  index_datum.dat_ptr = (char *) ptr;
  index_datum.dat_len = sizeof(int);

  string_datum.dat_ptr = strdup(string);
  string_datum.dat_len = strlen(string)+1;

  if ( ! hdbmstore(ht, string_datum, index_datum) ) {
    fprintf ( stderr, "ERROR: Failed to store token '%s' in hash\n", string );
    exit(-1);
  } 
  
  return;

}

/* ---------------------------------------------------------------
   This function retrieves an index for a string from a hashtable.
   --------------------------------------------------------------- */
int get_hashtable_string_index (HASHTABLE *ht, char *string)
{
  HDBMDATUM string_datum, index_datum;
  int *ptr;
  
  string_datum.dat_ptr = string;
  string_datum.dat_len = strlen(string)+1;

  index_datum = hdbmfetch(ht, string_datum);
  if (index_datum.dat_ptr != NULL) {
    ptr = (int *) index_datum.dat_ptr;
    return ptr[0];
  }
  else return -1;
}

/* --------------------------------------------------------------------------
   This function fills in a 2-d char array with the strings in the hash table
   -------------------------------------------------------------------------- */
void fill_in_string_array_with_hash_entries (HASHTABLE *ht, char **strings, int num_strings )
{
  register unsigned index;
  register HASHENT *hash_entry;
  register HASHENT **hash_array;
  register unsigned int table_size;

  if (BADTBL(ht))
    return;
  hash_array = ht->ht_addr;
  table_size = ht->ht_size;
  for (index = 0; index < table_size; index++) {
    for( hash_entry = hash_array[index]; hash_entry != NULL; hash_entry = hash_entry->he_next) {
      char *string = hash_entry->he_key.dat_ptr;
      int *value_ptr = (int *)hash_entry->he_data.dat_ptr;
      if ( *value_ptr >= 0 && *value_ptr < num_strings ) {
	strings[*value_ptr] = strdup(string);
      }
    }
  }	

  return;

}	
      


/* 
  for Emacs...
  Local Variables:
  mode: c
  fill-column: 110
  comment-column: 80
  c-tab-always-indent: t
  c-indent-level: 2
  c-continued-statement-offset: 2
  c-brace-offset: -2
  c-argdecl-indent: 2
  c-label-offset: -2
  End:
*/

