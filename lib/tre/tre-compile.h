/*
  tre-compile.h: Regex compilation definitions

  This software is released under a BSD-style license.
  See the file LICENSE for details and copyright.

*/


#ifndef TRE_COMPILE_H
#define TRE_COMPILE_H 1

typedef struct {
  int position;
#if 0 /* [i_a] must be able to carry the full span of all [Unicode] character codes *PLUS* these 'specials': TAG, PARAMETER, BACKREF, ASSERTION and EMPTY */
  tre_cint_t code_min;
  tre_cint_t code_max;
#else
  int code_min;
  int code_max;
#endif
  int *tags;
  int assertions;
  tre_ctype_t classt;
  tre_ctype_t *neg_classes;
  int backref;
  int *params;
} tre_pos_and_tags_t;

#endif /* TRE_COMPILE_H */

/* EOF */
