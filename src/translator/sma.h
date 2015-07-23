/* sma.h
   
   Session-based memory allocator. (see sma.c for more info)
   
   Written by: Seongbae Park <ramdrive@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __SMA_H__
#define __SMA_H__

// determined at maximum allowable heap size
#define SMA_MAX_SIZE_OF_HEAP_STACK 15
// preferred to be multiple of the page size
#define SMA_INITIAL_HEAP_SIZE       (1024 * 16)

struct SMA_Session_t {

   int    heapStackSize;
   char * heapStack[ SMA_MAX_SIZE_OF_HEAP_STACK ]; 
   int    heapSizeStack[ SMA_MAX_SIZE_OF_HEAP_STACK ];

   char * currentHeap;

   int  currentHeapIndex;
   int  currentHeapSize;
   int  currentHeapFreeSize;
};

typedef struct SMA_Session_t SMA_Session;

//
// for initialized memory allcator
// The nested use of initialized memory allocator is prohibited.
//
void    SMA_InitSession(SMA_Session * session, int initial_size);
void    SMA_EndSession(SMA_Session * session);
void*   SMA_Alloc(SMA_Session *session, int size);
void    SMA_RestartSession(SMA_Session *);

#endif __SMA_H__
