/* fast_mem_allocator.c
   This file contains implementations for fast memory allocator.
   
   Written by: Yang, Byung-Sun <scdoner@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "fast_mem_allocator.h"
#include <assert.h>
#include <strings.h>


#define NUM_OF_TOLERABLE_OVERFLOWS  500
#define INITIAL_HEAP_SIZE           (1024 * 50)

#define DBG(s) 
#define RDBG(s)  
#define PDBG(s) 

PDBG(static int    total_used_memory;);


//
// initialized memory allocator
//

static char*         Init_Heap_Stack[NUM_OF_TOLERABLE_OVERFLOWS];
static char*         Current_Init_Heap;
static int           Top_Of_Init_Heap_Stack;
static int           Left_Space_On_Current_Init_Heap;
static int           Init_Heap_Size;

static char          First_Init_Heap[ INITIAL_HEAP_SIZE ];

/* Name        : FMA_start
   Description : start fast memory allocator
   Maintainer  : Yang, Byung-Sun <scdoner@altair.snu.ac.kr>
   Pre-Condition:
       Before this function call, all the heaps in Init_Heap_Stack must be NULL.
   Post-Condition:
       Init_Heap_Stack[0] points to the pre-allocated heap from system.
       Init_Heap_Stack[0] must not be NULL.
       Current_Init_Heap points to the pre-allocated heap.
       Left_Space_On_Current_Init_Heap is set to INITIAL_AVAILABLE_SPACE.
       The allocated Current_Memory_Heap is initialized to zero.
       Top_Of_Init_Heap_Stack is set to 0.
   Notes:
       To use a fast initialized memory allocator, user must call this
       function to make the allocator work. This function preallocates
       some heap which the allocator will use to take up user request.
       The preallocated heap size is PREALLOC_HEAP_SIZE. */
void
FMA_start(void)
{
    int   i;

    PDBG(total_used_memory = 0;);

    for (i = 1; i < NUM_OF_TOLERABLE_OVERFLOWS; i++) {
        Init_Heap_Stack[i] = NULL;
    }

    Current_Init_Heap = Init_Heap_Stack[0] = First_Init_Heap;
    Left_Space_On_Current_Init_Heap = Init_Heap_Size = INITIAL_HEAP_SIZE;
    Top_Of_Init_Heap_Stack = 0;
    bzero(First_Init_Heap, INITIAL_HEAP_SIZE);

    RDBG(printf("start fast initialized memory allocator:"
                " initial heap size = %d from %p to %p\n",
                Init_Heap_Size,
                Current_Init_Heap, Current_Init_Heap + Init_Heap_Size));
}


/* Name        : FMA_allocate
   Description : allocate a memory chunk
   Maintainer  : Yang, Byung-Sun <scdoner@altair.snu.ac.kr>
   Pre-Condition:
       'Init_Memory_Heap' must have been initialized with
       FMA_start().
       size - a memory size to allocate
       'size' must be less than PREALLOC_HEAP_SIZE
   Post-Condition:
       If the allocation failed, it returns NULL. Otherwise, it
       returns a pointer to the contiguous allocated chunk from
       Init_Memory_Heap.  The new chunk is initialized to zero.
       'Left_Space_On_Current_Init_Heap' is decreased by 'size'.
       'Current_Init_Heap' points to the current heap which has been used to
       allocate the request.
       If the preallocated heap is overflowed, additional space is
       allocated from system. The pointer to this new allocated heap
       is recored in 'Init_Heap_Stack'. The 'Current_Init_Heap' points
       to the new allocated heap.  If overflow occurs more than
       NUM_OF_TOLERABLE_OVERFLOWS, a real overflow occurs. The
       allocator returns NULL.
   Notes:
       The default memory allocation system using 'malloc'/'calloc' is
       somewhat inefficient when small memory blocks are allocated
       frequently.  To cure this problem, I preallocate a large
       initialized memory heap with 'calloc' which is pointed by
       Init_Memory_Heap.  This segment is initialized to zero for
       convenience.  With this function, user can suballocate needed
       memory chunks.  The preallocated space will be consumed as this
       call is invoked.  When overflow occurs, the function allocates
       an additional space from the system.  The small memory segment
       which is allocated by this function call must not be freed by
       user.
       The size are aligned by 8 to prevent alignment error. -
       doner */
void*
FMA_allocate(int size)
{
    int    first_free_element_index =
        Init_Heap_Size - Left_Space_On_Current_Init_Heap;

    assert(size > 0 && size < 0x0fffffff);

    DBG(printf("required size = %d, heap size = %d, left size = %d\n",
               size, Init_Heap_Size, Left_Space_On_Current_Init_Heap));

    size = (size + 8) & -8;

    PDBG(total_used_memory += size;);

    if (Left_Space_On_Current_Init_Heap < size) {
        // allocate new initialized heap from system with 'calloc' call.
        if (++Top_Of_Init_Heap_Stack < NUM_OF_TOLERABLE_OVERFLOWS) {

            // calculate new heap size
            while ((Init_Heap_Size *= 2) < size);

            if (Init_Heap_Size >= 0x0fffffff) {
                Init_Heap_Size = 0x0fffffff;
            }

            Current_Init_Heap = Init_Heap_Stack[Top_Of_Init_Heap_Stack] =
//	    malloc(Init_Heap_Size);
//	 bzero(Current_Init_Heap, Init_Heap_Size);
                calloc(Init_Heap_Size, 1);
//         gc_malloc_fixed(Init_Heap_Size);
            Left_Space_On_Current_Init_Heap = Init_Heap_Size - size;

            assert(Current_Init_Heap != 0);

            RDBG(printf("overflow : new heap size = %d from %p to %p\n", Init_Heap_Size,
                        Current_Init_Heap, Current_Init_Heap + Init_Heap_Size));

            return (void*) &Current_Init_Heap[0];
        } else {			// a real overflow occurs
            assert(0 && "memory subsystem full!\n");
            return NULL;
        }

        PDBG(total_used_memory += (Left_Space_On_Current_Init_Heap + size););
    } else {
        Left_Space_On_Current_Init_Heap -= size;
        return (void*) &Current_Init_Heap[first_free_element_index];
    }
}


/* Name        : FMA_end
   Description : end fast memory allocator
   Maintainer  : Yang, Byung-Sun <scdoner@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
       The preallocated heaps on the Init_Heap_Stack are freed.
   Notes:  
       This function frees all heaps from system.  By this, all the
       objects user has created with this fast memory allocator will
       be deleted completely.  */
void
FMA_end()
{
    while (Top_Of_Init_Heap_Stack > 0) {
        RDBG(printf("freeing %p\n", Init_Heap_Stack[Top_Of_Init_Heap_Stack]););
        free((void*) Init_Heap_Stack[Top_Of_Init_Heap_Stack--]);
//      gc_free_fixed((void*) Init_Heap_Stack[Top_Of_Init_Heap_Stack--]);
    }

    PDBG(printf("total used memory is %u bytes for this duration.\n",
                total_used_memory););
}


/*
#ifdef 0

//
// uninitialized memory allocator
//

static char*         Uninit_Heap_Stack[NUM_OF_TOLERABLE_OVERFLOWS];
static char*         Current_Uninit_Heap;
static int           Top_Of_Uninit_Heap_Stack;
static int           Left_Space_On_Current_Uninit_Heap;
static int           Uninit_Heap_Size;
static int           Num_Of_Uninit_Heap_Clients = 0;

///
/// Function Name : start_fast_uninit_mem_allocator
/// Author : Yang, Byung-Sun
/// Input
///      void
/// Output
///      Uninit_Heap_Stack[0] points to the pre-allocated heap from system.
///      Current_Uninit_Heap points to the pre-allocated heap.
/// Pre-Condition
///      Before this function call, all the heaps in Uninit_Heap_Stack must be NULL.
/// Post-Condition
///      Uninit_Heap_Stack[0] must not be NULL.
///      Left_Space_On_Current_Uninit_Heap is set to INITIAL_AVAILABLE_SPACE.
///      Top_Of_Uninit_Heap_Stack is set to 0.
/// Description
///      To use a fast initialized memory allocator, user must call this
///      function to make the allocator work. This function preallocates
///      some heap which the allocator will use to take up user request.
///      The preallocated heap size is PREALLOC_HEAP_SIZE.
///
void
start_fast_uninit_mem_allocator()
{
   int   i;

   if (Num_Of_Uninit_Heap_Clients++ == 0) {
      for (i = 0; i < NUM_OF_TOLERABLE_OVERFLOWS; i++) {
	 Uninit_Heap_Stack[i] = NULL;
      }

      Uninit_Heap_Stack[0] = (char*) malloc(BASE_ELEM_SIZE * PREALLOC_HEAP_SIZE);
      Current_Uninit_Heap = Uninit_Heap_Stack[0];
      Left_Space_On_Current_Uninit_Heap = Uninit_Heap_Size = PREALLOC_HEAP_SIZE;
      Top_Of_Uninit_Heap_Stack = 0;
   }
}

///
/// Function Name : allocate_uninit_mem
/// Author : Yang, Byung-Sun
/// Input
///     size - a memory size to allocate
/// Output
///     return a pointer to allocated memory chunk
/// Pre-Condition
///     'Uninit_Memory_Heap' must have been initialized with
///     FMA_start().
///     'size' must be less than PREALLOC_HEAP_SIZE
/// Post-Condition
///     If the allocation failed, it returns NULL. Otherwise, it returns a pointer
///     to the contiguous allocated chunk from Uninit_Memory_Heap.
///     'Left_Space_On_Current_Uninit_Heap' is decreased by 'size'.
///     'Current_Uninit_Heap' points to the current heap which has been used to
///     allocate the request.
///     If the preallocated heap is overflowed, additional space is allocated from
///     system. The pointer to this new allocated heap is recored
///     in 'Uninit_Heap_Stack'. The 'Current_Uninit_Heap' points to
///     the new allocated heap.
///     If overflow occurs more than NUM_OF_TOLERABLE_OVERFLOWS, a real overflow
///     occurs. The allocator returns NULL.
/// Description
///     The default memory allocation system using 'malloc'/'calloc' is somewhat
///     inefficient when small memory blocks are allocated frequently.
///     To cure this problem, I preallocate
///     a large uninitialized memory heap with 'malloc' which is pointed by
///     Uninit_Memory_Heap.
///     With this function, user can suballocate needed memory chunks. The preallocated
///     space will be consumed as this call is invoked. When overflow occurs, 
///     the function allocates an additional space from the system.
///     The small memory segment which is allocated by this function call
///     must not be freed by user.
///

void*
allocate_uninit_mem(int size)
{
   int    first_free_element_index =
      Uninit_Heap_Size - Left_Space_On_Current_Uninit_Heap;

   size = (size + 8) & -8;

   if ((Left_Space_On_Current_Uninit_Heap -= size) < 0) {	// overflow occurs
      // allocate new initialized heap from system with 'calloc' call.
      if (++Top_Of_Uninit_Heap_Stack < NUM_OF_TOLERABLE_OVERFLOWS) {
	 Uninit_Heap_Size *= 2;
	 Current_Uninit_Heap = Uninit_Heap_Stack[Top_Of_Uninit_Heap_Stack] =
	    malloc(BASE_ELEM_SIZE * Uninit_Heap_Size);
	 Left_Space_On_Current_Uninit_Heap = Uninit_Heap_Size - size;

	 return (void*) &Current_Uninit_Heap[0];
      } else {			// a real overflow occurs
	 return NULL;
      }
   } else {
      return (void*) &Current_Uninit_Heap[first_free_element_index];
   }
}


///
/// Function Name : end_fast_uninit_mem_allocator
/// Author : Yang, Byung-Sun
/// Input
///      void
/// Output
///      The preallocated heaps on the Uninit_Heap_Stack are freed.
/// Pre-Condition
/// Post-Condition
/// Description
///      This function frees all heaps from system.
///      By this, all the objects user has created with this fast memory allocator
///      will be deleted completely.
///

void
end_fast_uninit_mem_allocator()
{
   if (--Num_Of_Uninit_Heap_Clients == 0) {
      while (Top_Of_Uninit_Heap_Stack >= 0) {
	 free(Uninit_Heap_Stack[Top_Of_Uninit_Heap_Stack--]);
      }
   }
}

#endif 0
*/
