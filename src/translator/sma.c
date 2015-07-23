/* sma.c
   
   This file contains implementations for session-based memory allocator.
   We can define a session.  A session represents a fraction of the heap
   that we can allocate memory blocks. 
   Memory blocks allocated by a session are only live during the session
   is live.  If a session is ended or restarted, every memory blocks
   allocated become unavailable. i.e. After calling SMA_RestartSession() 
   or SMA_EndSession(), memory blocks allocated by the session
   should not be used at all.
   
   Written by: Seongbae Park <ramdrive@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>

#include "config.h"
#include "sma.h"


#ifdef NDEBUG
#define DBG(s) ((void)0)
#else
// #define DBG(s)  s 
#define DBG(s) ((void)0)
#endif


// file-static function declarations...
static void SMA_SetCurrentHeap(SMA_Session *session, int index); 
static void SMA_AllocateNewHeap(SMA_Session *session, int index, int size); 
static void SMA_IncreaseHeap(SMA_Session *session, int size); 


/* Name        : SMA_SetCurrentHeap
   Description : set the current heap of the session to a heap block pointed by "index"
   Maintainer  : Seongbae Park <ramdrive@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
static void 
SMA_SetCurrentHeap(SMA_Session *session, int index) 
{
    DBG(fprintf(stderr,"SMA: SMA_SetCurrentHeap\n"););
    DBG(fprintf(stderr,"SMA: currentHeapIndex = %d\n", index););

    assert(index < session->heapStackSize);

    session->currentHeapIndex = index;
    session->currentHeap = session->heapStack[index];
    session->currentHeapSize = session->heapSizeStack[index];
    session->currentHeapFreeSize = session->currentHeapSize;
}


/* Name        : SMA_AllocateNewHeap
   Description : allocate a new heap block , size "size" at "index"
   Maintainer  : Seongbae Park <ramdrive@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
static void 
SMA_AllocateNewHeap(SMA_Session *session, int index, int size) 
{
    assert(index >= 0 && index < session->heapStackSize);
    assert(size > 0);

    DBG(fprintf(stderr, "SMA: SMA_AllocateNewHeap(session = 0x%x, index = %d, size = %d)\n", 
                session, index, size););

    if (session->heapStack[index] != NULL) {
        free(session->heapStack[index]);
    }

    // valloc -> aligned by page size
    // hopefully help the performance
    session->heapStack[index] = valloc(size);
    session->heapSizeStack[index] = size;

    if (session->heapStack[index] == 0) {
        fprintf(stderr, "SMA: SMA_AllocateNewHeap(0x%x,%d,%d)\n",
                (unsigned)session, index, size);
        fprintf(stderr, "SMA: insufficient memory - exit program\n");
        exit(1);
    }
}


/* Name        : SMA_InitSession
   Description : initialize SMA_Session
   Maintainer  : Seongbae Park <ramdrive@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
SMA_InitSession(SMA_Session * session, int initial_size)
{
    int i;
   
    DBG(fprintf(stderr,"SMA: SMA_InitSession\n"););
    assert(session != 0);

    for (i = 0; i < SMA_MAX_SIZE_OF_HEAP_STACK; i++) {
        session->heapStack[i] = NULL;
        session->heapSizeStack[i] = 0;
    }
    session->heapStackSize = 1;

    SMA_AllocateNewHeap(session, 0, initial_size);
    SMA_SetCurrentHeap(session, 0);
}


/* Name        : SMA_RestartSession
   Description : restart session
   Maintainer  : Seongbae Park <ramdrive@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
SMA_RestartSession(SMA_Session * session)
{
    DBG(fprintf(stderr, "SMA: SMA_RestartSession\n"););

    session->currentHeap = session->heapStack[0];
    session->currentHeapIndex = 0;
    session->currentHeapSize = session->heapSizeStack[0];
    session->currentHeapFreeSize = session->currentHeapSize;
}


/* Name        : SMA_IncreaseHeap
   Description : increase heap by minimum "size"
   Maintainer  : Seongbae Park <ramdrive@altair.snu.ac.kr>
   Pre-condition:
       size - size of requested memory block
   Post-condition:
   
   Notes:  */
static void 
SMA_IncreaseHeap(SMA_Session *session, int size)
{
   DBG(fprintf(stderr,"SMA: SMA_IncreaseHeap(session = 0x%x, size = %d)\n", 
               session, size););

   if (session->currentHeapIndex < session->heapStackSize - 1) {
       // already has a next heap block
       SMA_SetCurrentHeap(session, session->currentHeapIndex + 1);

       if (size > session->currentHeapFreeSize) {
           // The current heap block is not enough large.
           // Replace current heap block
           int new_heap_size = session->currentHeapSize * 2;

           while (new_heap_size < size) new_heap_size *= 2;
           SMA_AllocateNewHeap(session, session->currentHeapIndex,
                               new_heap_size);
           SMA_SetCurrentHeap(session, session->currentHeapIndex);

           assert(session->currentHeapFreeSize > size);
       }
   } else {
       // Heap stack overflow, allocate a new heap block 
       int new_heap_size = session->currentHeapSize * 2;

       session->heapStackSize++;
       while (new_heap_size < size) new_heap_size *= 2;
       SMA_AllocateNewHeap(session, session->heapStackSize - 1,
                           new_heap_size);
       SMA_SetCurrentHeap(session , session->heapStackSize - 1);

       assert(session->currentHeapFreeSize > size);
   }
}


/* Name        : SMA_Alloc
   Description : allocate "size" byte(s) of memory 
   Maintainer  : Seongbae Park <ramdrive@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void*
SMA_Alloc(SMA_Session * session, int size) 
{
    int free_index;

    DBG(fprintf(stderr,"SMA: SMA_Alloc(session = 0x%x, size = %d)\n", 
                session, size););

    // assume 8 byte align
    size = (size + 8) & -8;

    assert(size < INT_MAX / 2);

    if (session->currentHeapFreeSize < size) {
        // no space left on current heap block 
        // increase heap size
        SMA_IncreaseHeap(session, size);
    }
 
    free_index = session->currentHeapSize - session->currentHeapFreeSize;
    session->currentHeapFreeSize -= size;

    DBG(fprintf(stderr,"SMA: SMA_Alloc() = 0x%x\n",
                &(session->currentHeap[free_index])););

    return (void *) &(session->currentHeap[free_index]);
}


/* Name        : SMA_EndSession
   Description : return all heap blocks allocated by the session
   Maintainer  : Seongbae Park <ramdrive@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void 
SMA_EndSession(SMA_Session * session)
{
    int i;

    DBG(
         fprintf(stderr,"SMA: SMA_EndSession(session = 0x%x)\n", 
                 session);
         fprintf(stderr,"SMA: heapStackSize = %d\n",
                 session->heapStackSize);
    );

    for(i = 0; i < session->heapStackSize; i++) {
        free((void*) session->heapStack[i]);
    }
}

