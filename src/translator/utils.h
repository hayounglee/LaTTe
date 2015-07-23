/* utils.h
   
   Implementation of Queue and Linked List
   
   Written by: Junpyo Lee <walker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __UTILS_H__
#define __UTILS_H__

#include "gtypes.h"
#include "basic.h"
#include "fast_mem_allocator.h"

//
// integer queue by array
//
typedef struct {
    int *a;
    int head;
    int size;
    int max_size;
} iqueue;


//
// integer linked list
//
struct ilist_node {
    int value;
    struct ilist_node * next;
};

typedef struct ilist_node ilist;

iqueue*    iq_alloc();
iqueue*    iq_init(iqueue* q, int sz);
void       iq_enq(iqueue* q, int v);
int        iq_deq(iqueue* q);
int        iq_size(iqueue* q);
ilist*     ilist_get_next(ilist* i);
int        ilist_get_value(ilist* i);
void       ilist_set_next(ilist* i, ilist* n);
void       ilist_set_value(ilist*i, int v);
ilist*     ilist_alloc(int value, ilist* next);
void       ilist_free(ilist* s);
int        ilist_size(ilist* l);
bool       ilist_find(ilist* l, int value);
ilist*     ilist_insert(ilist* l, int val);
ilist*     ilist_insert_sorted(ilist* l, int value);
ilist*     ilist_push(ilist* l, int value);
ilist*     ilist_enq(ilist*l, int value);
ilist*     ilist_pop(ilist* l, int* pvalue);
ilist*     ilist_deq(ilist* l, int* pvalue);
ilist*     ilist_erase(ilist*l, int value);



#include "utils.ic"

#endif  __UTILS_H__
