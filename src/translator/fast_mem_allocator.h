/* fast_mem_allocator.h
   This is an interface file which can make a fast memory
   allocator available.  Use this allocator if 
   * you allocate small memory segments frequently, and
   * they are used only during translation not during run-time. 
   The memory allocated by this allocator will be freed at the end of 
   translating a method. 
   If you want to know more details, the description of allocate_init_mem 
   will be helpful. 

   Written by: Yang, Byung-Sun <scdoner@altair.snu.ac.kr>

   Copyright (C) 1999 MASS Lab., Seoul, Korea.

   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __FAST_MEM_ALLOCATOR_H__
#define __FAST_MEM_ALLOCATOR_H__


//
// for initialized memory allcator
// The nested use of initialized memory allocator is prohibited.
//
void    FMA_start();
void*   FMA_allocate(int size);
void    FMA_end();

#undef FMA_calloc
#define FMA_calloc(size)   FMA_allocate(size)


// no longer supported
// //
// // for uninitialized memory allocator
// // The nested use of initialized memory allocator is prohibited.
// //
// void    FMA_start_fast_uninit_mem_allocator();
// void*   FMA_allocate_uninit_mem( int size );
// void    FMA_end_fast_uninit_mem_allocator();

// #undef FMA_fmalloc
// #define FMA_fmalloc( size )   FMA_allocate_uninit_mem( size )


#endif __FAST_MEM_ALLOCATOR_H__
