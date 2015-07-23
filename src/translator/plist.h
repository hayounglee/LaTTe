/* plist.h
   
   Pointer List implementation used in loading uninitialized class
   loading for GETSTATIC/PUTSTATIC
   
   Written by: SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __PLIST_H__
#define __PLIST_H__

#include "basic.h"
#include "gc.h"

typedef struct PList {
    struct PList *next;
    void *data;
} PList;    

PList *PList_GetNext(PList *l);
void *PList_GetValue(PList *l);
void PList_SetNext(PList *l, PList *n);
void PList_SetValue(PList *l, void *p);
PList *PList_alloc(void *p, PList *next);
void PList_free(PList *l);
bool PList_find(PList *l, void *p);
PList *PList_insert(PList *l, void *p);

#include "plist.ic"

#endif /* __PLIST_H__ */
