/* mem_allocator_for_code.c
   memory allocator for JIT-compiled code
   
   Written by: SeungIl Lee <hacker@altair.snu.ac.kr>
   Rewritten by: Suhyun Kim <zelo@i.am>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#include <stdio.h>
#include "config.h"
#include "mem_allocator_for_code.h"
#include <assert.h>
#include <unistd.h>

#ifdef NEW_CODE_ALLOC

static const int CMA_ALLOCATION_SIZE = 1024 * 256; // size to allocate at once.
static char *CMA_Start_Of_Free_Mem = NULL; // to make sure pointer arithmetic
static int CMA_Allocated_Size = 0;



/* Name        : CMA_allocate_cache_aligned_memory
   Description : allocate cache aligned memory chunk
   Maintainer  : Suhyun Kim <zelo@i.am>
   Pre-condition:
   * cache is aligned at BLOCK_SIZE(32) bytes. ^^
   Post-condition:
   
   Notes:
   * sbrk seems not to return aligned memory chunk. So we allocate
   a little more memory and return only aligned area. (a litte memory waste) */
static inline
void*
CMA_allocate_cache_aligned_memory(int size)
{
    static const int BLOCK_SIZE = 32;   // in bytes...

    return (void*) (((int) sbrk(size + BLOCK_SIZE) 
            + BLOCK_SIZE - 1) & (- BLOCK_SIZE));
}


void
CMA_initialize_mem_allocator()
{
    CMA_Start_Of_Free_Mem =
        CMA_allocate_cache_aligned_memory(CMA_ALLOCATION_SIZE);
}


void *
CMA_allocate_code_block(int size)
{
    static const int ALIGN_SIZE = 32;   // in bytes...
    int code_size = (size + ALIGN_SIZE-1) & -ALIGN_SIZE;
        // ALIGN_SIZE bytes aligned
    int previous_allocated_size = CMA_Allocated_Size;

    CMA_Allocated_Size += code_size;

    if (CMA_Allocated_Size <= CMA_ALLOCATION_SIZE) {
        assert(previous_allocated_size % ALIGN_SIZE == 0);

        return CMA_Start_Of_Free_Mem + previous_allocated_size;
    } else {
        if (code_size > CMA_ALLOCATION_SIZE) {
            return CMA_allocate_cache_aligned_memory(code_size*4);
        }
        // code_size <= CMA_ALLOCATION_SIZE
        CMA_initialize_mem_allocator();

        CMA_Allocated_Size = code_size;
        return CMA_Start_Of_Free_Mem;
    }

    // just to avoid warning.
    return NULL;
}

#endif
