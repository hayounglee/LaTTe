/* gc.c
   Memory management subsystem.
   
   Written by: Yoo C. Chung <chungyc@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Laboratory, Seoul National University
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

/* This file is composed of the following parts:

   - Common declarations and utilities.

   - Region manager.

   - Manual memory manager.

   - Implementation of the explicitly managed heap.

   - Implementation of the small object area.

   - Implementation of the large object area.

   - Support for finalization.

   - Implementation of the garbage collector.

   - Type specific walking functions.

   The explicitly managed heap, the small object area, and the large
   object area all obtain large "region"s of memory from the region
   manager.  The region manager gives and takes memory in units of 1MB
   to 4MB, which are block aligned.

   The manual memory manager is used for both the explicitly managed
   heap and the large object area.

   The explicitly managed heap is implemented by simply using the
   manual memory manager.  Fixed memory are allocated from this area.
   Since it maintained separately from the garbage collected heap, no
   memory requests for fixed memory can cause a garbage collection.

   The large object area is also implemented using the manual memory
   manager.  Automatically managed memory chunks larger than one to
   two kilobytes are allocated in this area.  References to objects
   allocated in this area are maintained in a separate hash table, in
   order to support conservative garbage collection.

   The small object area uses a memory allocator of its own.
   Automatically managed memory chunks smaller than one to two
   kilobytes are allocated in this area.  Objects allocated in this
   area do not straddle block boundaries in order to support
   conservative garbage collection.

   The garbage collector uses non-incremental mark and sweep.

   The size of the heap only includes the garbage collected heap, and
   ignores the explicitly managed heap.  On the other hand, the heap
   limit takes the explicitly managed heap into consideration.  It
   would be more accurate to say "heap size" and "memory limit", but
   it is a historical artifact that the memory size limit is specified
   by GC_HEAP_LIMIT.  This limit is enforced in the region manager.

   The term "block" is used as meaning a page or two.  (It doesn't
   really matter what the exact value of it is, as long as it's large
   enough and is a power of two.)

   In a multiprocessor environment, it would be better to use separate
   free areas and/or free lists for each thread.  However, LaTTe uses
   user-level threads, so doing so may not be worthwhile.  (In fact,
   in a previous incarnation, it was actually very slightly slower.) */

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include "config.h"
#include "gtypes.h"
#include "flags.h"
#include "classMethod.h"
#include "baseClasses.h"
#include "exception.h"
#include "exception_handler.h"
#include "errors.h"
#include "thread.h"
#include "locks.h"
#include "gc.h"


/*****************************************************************/
/* Common declarations and utilities                             */
/*****************************************************************/

/* The default sizes for the heap. */
#define	MAX_HEAPSIZE		(512*1024*1024)
#define INITIAL_HEAPSIZE	(8*1024*1024)

/* The following bits are used to store the status of each object, and
   are stored in the preheaders.  The second bit is only used by the
   small object area (for supporting conservative marking).  The last
   one is only used by the manual memory manager. */
#define MARK_BIT	1	/* Mark bit. */
#define FREE_BIT	2	/* Bit marking the memory is free. */
#define PREV_BIT	4	/* Bit marking the previous memory in use. */

#define SIZE_MASK	(~0x7)	/* Masks out lower bits in preheader. */

/* Memory object types. */
#define	GC_OBJECT_NORMAL	((void*)0)
#define	GC_OBJECT_ROOT		((void*)1)
#define	GC_OBJECT_FIXED		((void*)2)

/* All objects with finalizers have types larger than this value. */
#define GC_OBJECT_SPECIAL	GC_OBJECT_FIXED

/* States of the mark phase. */
#define MARK_STATE_COLLECT	0	/* Making set of live objects. */
#define MARK_STATE_NORMAL	1	/* Traditional marking. */
#define MARK_STATE_OVERFLOW	2	/* Mark stack overflow occurred. */

/* Region types. */
#define GC_REGION_DUMMY		0		/* Dummy region. */
#define GC_REGION_SMALL		1		/* Small object area. */
#define GC_REGION_LARGE		2		/* Large object area. */
#define GC_REGION_FIXED		4		/* Fixed memory area. */

#define GC_BLOCKSIZE		4096		/* Size of blocks. */
#define GC_REGIONSIZE		(2*1024*1024)	/* Unit size of regions. */

/* Align sizes down to block sizes. */
#define ROUNDDOWNBLOCKSIZE(V)	\
	((uintp)(V) & ~(GC_BLOCKSIZE - 1))

/* Align sizes up to block sizes. */
#define	ROUNDUPBLOCKSIZE(V)	\
	(((uintp)(V) + GC_BLOCKSIZE - 1) & -GC_BLOCKSIZE)

/* Align sizes up to region sizes. */
#define	ROUNDUPREGIONSIZE(V)	\
	(((uintp)(V) + GC_REGIONSIZE - 1) & -GC_REGIONSIZE)

/* All objects have a preheader prepended to them.  When the lower
   bits are masked out using SIZE_MASK, the result is the size of the
   object.  The following macro returns the preheader from a pointer.

   If there could be any guarantee that fixed memory will never be
   referenced by any automatically managed object, then this header
   could be removed since the mark bit could be put in the dispatch
   table pointer and the sizes also can be reverse engineered from the
   dispatch table pointer.  Too bad there are enough cases where fixed
   memory is referenced, so that it would be a risk to do this. */
#define HEADER(p)		((gc_head*)(p)-1)

/* Obtain the size of an object. */
#define SIZE(p)			(*HEADER(p) & SIZE_MASK)

/* Macros for protecting critical sections of the memory manager.

   FIXME: intsDisable() and intsRestore() are not sufficient!  But we
   can't even use lockMutex() because gc_malloc() is used before the
   thread system is initialized.

   To see why this is not sufficient, think of the following
   situation:

   thread A blocks and thread B awakes
   thread B calls gc_malloc(), which calls invokeGC()
   invokeGC() blocks B and awakes the GC thread
   the GC thread does GC
   the GC thread blocks and wakes another thread
   the newly awaken thread may be thread A
   thread A then calls gc_malloc()

   In this situation, since intsDisable() does not block, we end up in
   a situation where both thread A and thread B are in the critical
   sections of gc_malloc().

   Fortunately, since the thread system is currently not preemptive,
   we can arrange the code such that it would be OK for more than two
   threads to be running inside the critical sections.  In fact, if
   there were no timer interrupts preempting threads, these may be
   empty. */
#define LOCK()			intsDisable()
#define UNLOCK()		intsRestore()

/* Obtain the current heap size. */
#define HEAPSIZE(stats)		((stats).small_size + (stats).large_size)

/* Obtain the current size of the memory obtained from system. */
#define MEMORYSIZE(stats)		\
	((stats).small_size + (stats).large_size + (stats).fixed_size)

/* Walk the references in the object referred to by P. */
#define WALK(p)	(((Hjava_lang_Object*)(p))->dtable->class->walk)(p)

/* Current size limit of heap.  Garbage collection is triggered when
   the heap size reaches this limit and a normal object cannot be
   allocated.  This limit is expanded as necessary.  Note that the
   size of the explicitly managed heap is not included in this limit.
   Also note that this variable may not contain the actual size of the
   heap (it is only used as a hint as to when expand the heap instead
   of doing a premature garbage collection). */
static size_t heap_size;

/* Initial heap size. */
size_t gc_heap_allocation_size = INITIAL_HEAPSIZE;

/* The memory size limit. */
size_t gc_heap_limit = MAX_HEAPSIZE;

/* Memory management subsystem statistics. */
struct gc_stats gc_stats;

/* Throw an out of memory exception. */
extern void throwOutOfMemory (void) __attribute__ ((noreturn));

/* The following functions are defined in the garbage collector. */
static void mark (void*);		/* Mark object. */
static void walk_mark_stack (void);	/* Mark reachable objects. */

/* Name        : gc_malloc
   Description : A general-purpose memory allocator.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     This does all the extra work necessary for allocating memory,
     such as setting finalizers, selecting the heap to use, etc. */
void*
gc_malloc (size_t size, gc_type *type)
{
    void *p;

    if (type->final == GC_OBJECT_ROOT) {
	p = gc_malloc_fixed(size);
	gc_attach(p, type);
    } else if (type->final == GC_OBJECT_FIXED)
	p = gc_malloc_fixed(size);
    else {
	size_t asize;

	asize = ROUNDUPALIGN(size+GC_HEAD); /* Add header and alignment. */

	if (asize < GC_MAX_SMALL_OBJECT_SIZE)
	    p = gc_malloc_small(asize);
	else
	    p = gc_malloc_large(size);

	if (type->final != GC_OBJECT_NORMAL)
	    gc_set_finalizer(p, type);
    }

    assert(p != NULL);

    return p;
}


/*****************************************************************/
/* Region manager                                                */
/*****************************************************************/

/* The region manager maintains information for each region separate
   from the regions themselves.  Since regions are never freed in the
   current implementation, a new region is always allocated from the
   system whenever there is a new request.

   The regions in the region table are sorted in increasing address
   order.  The region table will be looked up with a binary search,
   and it contains information such as the start and end addresses,
   type, and other items for each region.  The start address is
   inclusive, while the end address is exclusive.

   In this incarnation of the region manager, the region table will be
   a fixed array of about a thousand elements.  Assuming that each
   region is about 2MB, it can hold up to 2GB, which should be enough
   for almost anyone.  In the future, this limit may be removed using
   another approach to maintain the region table.

   Since a non-moving collector will be used, it will be unlikely that
   a region will be completely emptied of objects, so freeing of
   regions has not been implemented yet. */

#define GC_REGION_TABLE_SIZE	1024	/* Size of region table. */

/* Structure containing information for each region. */
struct gc_region {
    void *start, *end;		/* Address range of region. */
    int type;			/* Type of memory contained in region. */

    /* Links for a doubly-linked list used by external components. */
    struct gc_region *prev, *next;

    /* For maintaining other data that might be used by external components. */
    void *data;
};

/* Number of entries in the region table. */
static int region_table_size;

/* Region table. */
static struct gc_region *regions[GC_REGION_TABLE_SIZE];

/* The pool from which region entries are allocated. */
static struct gc_region region_pool[GC_REGION_TABLE_SIZE];

/* Free list of region entries. */
static struct gc_region *free_regions;

/* Initialize region manager. */
static void region_init (void);

/* Find the entry in the region table from the memory address. */
static struct gc_region* region_find (void*);

/* Allocate a new region. */
static struct gc_region* region_allocate (size_t size, int type);

/* Merge the two given regions. */
static void region_merge (struct gc_region*, struct gc_region*);

/* Name        : region_init
   Description : Initialize the region manager.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static void
region_init (void)
{
    int i;

    /* Construct the free list. */
    for (i = 0; i < GC_REGION_TABLE_SIZE; i++) {
	region_pool[i].prev = &region_pool[i-1];
	region_pool[i].next = &region_pool[i+1];
    }

    /* Set the free list. */
    region_pool[0].prev = NULL;
    region_pool[GC_REGION_TABLE_SIZE-1].next = NULL;
    free_regions = &region_pool[0];
}

/* Name        : region_find
   Description : Find the entry for the region containing a pointer.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Post-condition:
     If there is no corresponding region, return NULL. */
static struct gc_region*
region_find (void *p)
{
    int lo, hi;

    /* Use binary search. */
    lo = 0;
    hi = region_table_size-1;
    while (lo < hi) {
	int mid;

	mid = (lo + hi + 1) / 2;
	if (p < regions[mid]->start)
	    hi = mid - 1;
	else
	    lo = mid;
    }

    /* REGION_FIND is called only during a garbage collection (for
       conservative pointer finding), so there should be at least one
       entry in the region table since garbage collection occurs only
       after the machine is initialized. */
    assert(lo == hi);

    if (p >= regions[lo]->start && p < regions[lo]->end)
	return regions[lo];
    else
	return NULL;
}

/* Name        : region_allocate
   Description : Allocate a region.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     SIZE must be a multiple of GC_REGIONSIZE.
   Post-condition:
     Returns NULL if no region can be allocated. */
static struct gc_region*
region_allocate (size_t size, int type)
{
    int i;
    void *region;
    struct gc_region *entry;

    assert(size > 0);
    assert(size % GC_REGIONSIZE == 0);
    assert(region_table_size < GC_REGION_TABLE_SIZE);

    /* Check if we have reached the heap limit. */
    if (size + MEMORYSIZE(gc_stats) > gc_heap_limit)
	return NULL;

    /* Allocate memory from the system. */
    while (1) {
	size_t miss;

	region = sbrk(size);

	if (region == (void*)(-1))
	    return NULL;	/* sbrk() failed */

	/* Make sure the memory is aligned on a block boundary. */
	miss = GC_BLOCKSIZE - ((uintp)region % GC_BLOCKSIZE);
	if (miss == GC_BLOCKSIZE)
	    break;
	else
	    sbrk(-size+miss);
    }

    /* Allocate new region entry. */
    entry = free_regions;
    free_regions = entry->next;
    entry->start = region;
    entry->end = (char*)region + size;
    entry->type = type;

    /* Shift appropriate region entries. */
    i = region_table_size - 1;
    while (i >= 0 && regions[i]->start > region) {
	regions[i+1] = regions[i];
	i--;
    }

    /* Insert new region into table. */
    regions[i+1] = entry;
    region_table_size++;

    return entry;
}

/* Name        : region_merge
   Description : Merge two regions.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static void
region_merge (struct gc_region *a, struct gc_region *b)
{
    /* FIXME: implement */
}


/*****************************************************************/
/* Manual memory manager                                         */
/*****************************************************************/

/* The manual memory manager implements a best-fit allocation policy
   using segregated free lists.  It uses a linear distribution of size
   classes for lower sizes and a power of two distribution of size
   classes at higher sizes.

   Since the manager is used by both the explicitly managed heap and
   the large object area, an extra argument that specifies properties
   of the heap to manage is given to the manager functions, instead of
   using shared data structures.

   Unlike the small object area, the term "free lump" is used instead
   of "free chunk", to avoid confusion and naming clashes.  Each free
   lump looks like the following:

     header
     next link
     previous link
     ...
     size of lump

   The last lump in a contiguous region will be followed by two
   headers, which will simulate two following adjacent objects. */

/* The sentinel for a segregated free list. */
#define SIZECLASS_SENTINEL	((void*)(-1))

/* These are used to calculate size classes. */
#define SIZECLASS_MAX_SMALLSIZE	1024
#define SIZECLASS_MAX_EXPONENT	16

/* Number of size classes. */
#define SIZE_CLASSES	\
	((SIZECLASS_MAX_SMALLSIZE / (2*MEMALIGN)) + SIZECLASS_MAX_EXPONENT + 1)

/* Pointer to the next and previous free memory lump in the free list. */
#define NEXT(p)	(*(void**)(p))
#define PREV(p)	(*((void**)(p) + 1))

/* Pointer to next and previous adjacent free lumps. */
#define NEXT_LUMP(p)	((char*)(p) + SIZE(p))
#define PREV_LUMP(p)	((char*)(p) - *((gc_head*)(p)-2))

/* Footer for a free memory lump. */
#define FOOTER(p, size)	((gc_head*)((char*)(p) + (size) - 2*GC_HEAD))

/* Structure containing information for each heap. */
struct gc_heap {
    void *lists[SIZE_CLASSES+1];	/* Segregated free lists. */
    size_t *size, *alloc;		/* Placeholders for statistics. */
    int type;				/* Region type. */
};

/* Calculate size class. */
static int size_class (size_t);

/* Find a suitable free lump. */
static void* find_freelump (struct gc_heap *heap, size_t size, int class);

/* Insert lump into free list. */
static void insert_freelump (struct gc_heap *heap, void *lump, int class);

/* Remove lump from free list. */
static void remove_freelump (struct gc_heap *heap, void *lump, int class);

/* Initialize heap information structure. */
static void mem_init (struct gc_heap*, size_t*, size_t*, int);

/* Allocate memory from heap. */
static void* mem_allocate (struct gc_heap*, size_t);

/* Resize given memory within heap. */
static void* mem_resize (struct gc_heap*, void*, size_t);

/* Free memory in heap. */
static void mem_free (struct gc_heap*, void*);

/* Expand given heap to accomodate new size.  Return negative value if
   not possible to do so. */
static int mem_expand (struct gc_heap*, size_t);

/* Name        : size_class
   Description : Calculate size class.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     It is assumed that SIZE is aligned.
   Notes:
     At lower sizes, a linear distribution is used.  At higher sizes
     (>1024), a power of two distribution is used until some maximum
     size is reached. */
static int
size_class (size_t size)
{
    int c;

    assert(size > 0);
    assert(size % (2 * MEMALIGN) == 0);

    /* The linear distribution. */
    if (size <= SIZECLASS_MAX_SMALLSIZE)
	return size / (2 * MEMALIGN);

    /* Check for the largest size class. */
    if (size > (SIZECLASS_MAX_SMALLSIZE * (1 << SIZECLASS_MAX_EXPONENT)))
	return SIZE_CLASSES - 1;

    /* Otherwise use a power of two distribution. */
    c = SIZECLASS_MAX_SMALLSIZE / (2 * MEMALIGN);
    for (; size > SIZECLASS_MAX_SMALLSIZE; size = size / 2)
	c++;

    assert(c > SIZECLASS_MAX_SMALLSIZE / (2*MEMALIGN));
    assert(c < SIZE_CLASSES - 1);

    return c;
}

/* Name        : find_freelump
   Description : Find a suitable free lump.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     SIZE must include alignment and header overhead.
   Post-condition:
     Returns NULL if no lump can be found. */
static void*
find_freelump (struct gc_heap *heap, size_t size, int class)
{
    void *p;

    assert(class < SIZE_CLASSES);
    assert(size % (2*MEMALIGN) == 0);
    assert(size_class(size) == class);

    p = heap->lists[class];
    while (p != NULL && SIZE(p) < size)
	p = NEXT(p);

    if (p != NULL)
	return p;	/* Found. */

    /* Try to find lump in larger size classes. */
    class++;
    while (heap->lists[class] == NULL)
	class++;

    /* If not the sentinel list, then a larger size class that can
       accomodate the request has been found.  Since it is a larger
       size class, any node in the list can satisfy the request. */
    if (class < SIZE_CLASSES)
	return heap->lists[class];	/* Found. */
    else
	return NULL;			/* Not found. */
}

/* Name        : insert_freelump
   Description : Insert lump into free list.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     Each free list is in order of increasing size. */
static void
insert_freelump (struct gc_heap *heap, void *lump, int class)
{
    size_t size;
    void *prev, *node;

    size = SIZE(lump);

    assert(class < SIZE_CLASSES);
    assert(size % (2 * MEMALIGN) == 0);
    assert(size_class(size) == class);

    /* Find appropriate position in free list. */
    prev = NULL;
    node = heap->lists[class];
    while (node != NULL && SIZE(node) < size) {
	prev = node;
	node = NEXT(node);
    }

    PREV(lump) = prev;
    NEXT(lump) = node;

    if (prev == NULL)
	heap->lists[class] = lump;
    else
	NEXT(prev) = lump;

    if (node != NULL)
	PREV(node) = lump;
}

/* Name        : remove_freelump
   Description : Remove lump from free list.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static void
remove_freelump (struct gc_heap *heap, void *lump, int class)
{
    assert(class < SIZE_CLASSES);
    assert(SIZE(lump) % (2 * MEMALIGN) == 0);
    assert(size_class(SIZE(lump)) == class);

    if (PREV(lump) == NULL)
	heap->lists[class] = NEXT(lump);
    else
	NEXT(PREV(lump)) = NEXT(lump);

    if (NEXT(lump) != NULL)
	PREV(NEXT(lump)) = PREV(lump);
}

/* Name        : mem_init
   Description : Initialize a manual memory manager.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static void
mem_init (struct gc_heap *heap, size_t *size, size_t *alloc, int type)
{
    int i;

    for (i = 0; i < SIZE_CLASSES; i++)
	heap->lists[i] = NULL;

    heap->lists[SIZE_CLASSES] = SIZECLASS_SENTINEL;  /* Set sentinel. */
    heap->type = type;
    heap->size = size;
    heap->alloc = alloc;
}

/* Name        : mem_allocate
   Description : Manually allocate memory.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static void*
mem_allocate (struct gc_heap *heap, size_t size)
{
    int class;
    void *p;

#ifdef TRANSLATOR
    /* Add extra padding to avoid a bug.  FIXME: do a real fix! */
    size += MEMALIGN;
#endif /* TRANSLATOR */

    /* Round up to twice the alignment (so as to assure that space for
       the links are available), while including space for the
       header. */
    size = ((uintp)(size + GC_HEAD) + 2*MEMALIGN - 1) & -(2*MEMALIGN);
    class = size_class(size);

    p = find_freelump(heap, size, class);
    if (p != NULL) {
	assert(!(*HEADER(NEXT_LUMP(p)) & PREV_BIT));
	assert(SIZE(p) >= size);

	remove_freelump(heap, p, size_class(SIZE(p)));

	/* Split lump if it's too large. */
	if (SIZE(p) > size) {
	    void *t;
	    size_t s;

	    t = (char*)p + size;
	    s = SIZE(p) - size;

	    *HEADER(t) = *FOOTER(t, s) = s;
	    insert_freelump(heap, t, size_class(s));

	    *HEADER(p) = size | (*HEADER(p) & ~SIZE_MASK);
	}

	assert(size == SIZE(p));

	/* Mark the lump as used. */
	*HEADER(NEXT_LUMP(p)) |= PREV_BIT;

	memset(p, 0, size - GC_HEAD);

	*(heap->alloc) += size;	/* Update statistics. */
    }

    return p;
}

/* Name        : mem_resize
   Description : Resize a memory lump.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static void*
mem_resize (struct gc_heap *heap, void *p, size_t size)
{
    void *t;

    if (p == NULL)
	return mem_allocate(heap, size);

    /* Allocate new memory lump.  FIXME: Try to find and coalesce
       adjacent lumps instead of simply allocating new memory and
       freeing the old one. */
    t = mem_allocate(heap, size);
    if (t == NULL)
	return NULL;

    if (size > SIZE(p) - GC_HEAD)
	memcpy(t, p, SIZE(p) - GC_HEAD);
    else
	memcpy(t, p, size);

    mem_free(heap, p);

    *(heap->alloc) += size;  /* Update statistics. */

    return t;
}

/* Name        : mem_free
   Description : Manually free a memory chunk.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static void
mem_free (struct gc_heap *heap, void *p)
{
    if (p == NULL)
	return;

    assert(SIZE(p) % (2 * MEMALIGN) == 0);
    assert(*HEADER(NEXT_LUMP(p)) & PREV_BIT);

    if (!(*HEADER(p) & PREV_BIT)) {
	/* Coalesce with previous adjacent lump. */
	void *prev;

	prev = PREV_LUMP(p);
	remove_freelump(heap, prev, size_class(SIZE(prev)));
	*HEADER(prev) = (SIZE(prev) + SIZE(p)) | (*HEADER(prev) & ~SIZE_MASK);
	p = prev;
    }

    if (!(*HEADER(NEXT_LUMP(NEXT_LUMP(p))) & PREV_BIT)) {
	/* Coalesce with next adjacent lump. */
	void *next;

	next = NEXT_LUMP(p);
	remove_freelump(heap, next, size_class(SIZE(next)));
	*HEADER(p) = (SIZE(p) + SIZE(next)) | (*HEADER(p) & ~SIZE_MASK);
    }

    *FOOTER(p, SIZE(p)) = SIZE(p);
    *HEADER(NEXT_LUMP(p)) &= ~PREV_BIT;

    insert_freelump(heap, p, size_class(SIZE(p)));
}

/* Name        : mem_expand
   Description : Expand the specified heap.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Post-condition:
     Returns a negative value if the heap cannot be expanded. */
static int
mem_expand (struct gc_heap *heap, size_t size)
{
    void *p;
    size_t rsize;
    struct gc_region *region;

    /* Include SIZE and the various headers in the region size. */
    rsize = ROUNDUPREGIONSIZE(size + 6 * MEMALIGN);

    /* FIXME: implement region merging. */
    region = region_allocate(rsize, heap->type);
    if (region == NULL)
	return -1;	/* Failure. */

    /* Set header and footer for new free space.  The first object is
       on a double-aligned boundary (not *that* double), in the belief
       that it would improve cache performance. */
    size = rsize - 6*MEMALIGN;
    p = (char*)region->start + 2*MEMALIGN;
    *HEADER(p) = size | PREV_BIT;
    *FOOTER(p, size) = size;

    insert_freelump(heap, p, size_class(size));

    /* Set headers for following dummy objects. */
    p = NEXT_LUMP(p);
    *HEADER(p) = MEMALIGN;
    *HEADER(NEXT_LUMP(p)) = PREV_BIT;

    assert(p < region->end);

    *(heap->size) += rsize;	/* Update statistics. */

    return 0;	/* Success. */
}


/*****************************************************************/
/* Fixed heap manager                                            */
/*****************************************************************/

/* The explicitly managed heap simply uses the manual memory manager
   to manage memory. */

/* Information for the explicitly managed heap. */
static struct gc_heap fixed_heap;

/* Name        : gc_malloc_fixed
   Description : Allocate memory from explicitly managed heap.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
void*
gc_malloc_fixed (size_t size)
{
    void *p;

    LOCK();

    p = mem_allocate(&fixed_heap, size);
    if (p == NULL) {
	int r;

	/* Expand fixed heap if possible. */
	r = mem_expand(&fixed_heap, size);
	if (r < 0) {
	    UNLOCK();
	    throwOutOfMemory();
	}

	p = mem_allocate(&fixed_heap, size);

	assert(p != NULL);
    }

    *HEADER(p) |= MARK_BIT; /* Fixed memory is always considered marked. */

    UNLOCK();

    return p;
}

/* Name        : gc_realloc_fixed
   Description : Resize explicitly managed memory.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Post-condition:
     Returns the pointer to the new piece of memory. */
void*
gc_realloc_fixed (void *mem, size_t size)
{
    void *p;

    LOCK();

    p = mem_resize(&fixed_heap, mem, size);
    if (p == NULL) {
	int r;

	/* Expand fixed head if possible. */
	r = mem_expand(&fixed_heap, size);
	if (r < 0) {
	    UNLOCK();
	    throwOutOfMemory();
	}

	p = mem_resize(&fixed_heap, mem, size);

	assert(p != NULL);
    }

    *HEADER(p) |= MARK_BIT; /* Fixed memory is always considered marked. */

    UNLOCK();

    return p;
}

/* Name        : gc_free
   Description : Free explicitly managed memory.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
void
gc_free (void *p)
{
    LOCK();
    mem_free(&fixed_heap, p);
    UNLOCK();
}


/*****************************************************************/
/* Small object area manager                                     */
/*****************************************************************/

/* In the most common case, memory is allocated by using simple
   pointer increments within the current free memory chunk.  In case
   the current free memory chunk is unable to satisfy the memory
   allocation request, the largest free chunk in the free list is used
   as the new current free memory chunk.

   Free chunks are maintained with a linked list, with the links
   embedded within the chunks themselves.  Embedding links isn't any
   problem cache-wise since the chunks are almost certainly to be used
   as soon as the links are visited.

   As can be seen in expand_heap(), the heap expansion heuristics
   require the amount of allocations done to work properly.  However,
   it would be very expensive to update the statistics for each
   allocation, since allocation is very cheap.  Fortunately, there is
   a clever way to work around this problem.

   Instead of updating the statistics when each allocation is done,
   add the size of the new free memory chunk when it is switched in,
   and decrement the amount of memory left unallocated in the memory
   chunk when it is switched out.  It is easy to see that this is
   accurate. */

/* Size for each block usage table. */
#define SMALL_BLOCKS_TABLE_SIZE		\
	(128*(GC_REGIONSIZE/(GC_BLOCKSIZE*sizeof(unsigned)*CHAR_BIT)))

/* Number of entries in the free chunk list index. */
#define SMALL_CHUNKS_LIST_INDEX_SIZE	(GC_BLOCKSIZE/MEMALIGN)

/* Size of radix to be used in radix sort. */
#define RADIX_SIZE	2048

/* Number of bits in the above radix. */
#define RADIX_BITS	11

/* Mask used to obtain the radix for a radix sort. */
#define RADIX_MASK	0x7ff

/* Unused memory "chunks" that are presumably sprinkled throughout the
   small object area. */
struct gc_small_chunk {
    size_t size;			/* Size of chunk. */
    struct gc_small_chunk *next;	/* For linked list. */
};

/* A group of adjacent blocks located in the small object area.  These
   groups are maintained in a singly linked list. */
struct gc_small_block {
    size_t size;	/* Size of memory occupied by blocks. */
    struct gc_small_block *next;	/* For linked list. */
};

/* Free chunk list index table used while sweeping the small object
   area.  This enables constant time insertions into the list. */
struct small_chunk_table {
    struct {
	struct gc_small_chunk head_chunk;
	struct gc_small_chunk *tail;
    } table[SMALL_CHUNKS_LIST_INDEX_SIZE];
};

/* The range of the current free area. */
static gc_head *small_cursor, *small_bound;

/* The free chunk list. */
static struct gc_small_chunk *small_chunks;

/* The free blocks list.  This *must* be in increasing order of
   address so that sweeping the small object area can be done
   properly. */
static struct gc_small_block *small_blocks;

/* List of regions in the small object area.  This *must* be in
   increasing order of address so that sweeping the small object area
   can be done properly. */
static struct gc_region *small_regions_head, *small_regions_tail;

/* Expand the small object area. */
static void expand_small_area (void);

/* Get a free block in the small object area. */
static void* get_small_block (void);

/* The real implementation for the slow allocator. */
static void* get_small_chunk (size_t);

/* A slower allocator when the free area is unable to satisfy request. */
static void* slow_small_allocate (size_t);

/* Prepare the small object area for a garbage collection. */
static void prepare_gc_small (void);

/* Check whether a pointer points to an object in the small object area. */
static int is_small_object (void*, struct gc_region*);

/* Sort the set of live objects. */
static void** sort_objects (void**, int);

/* Initialize a free chunk list index. */
static void init_freeindex (struct small_chunk_table*);

/* Insert a free chunk into the free chunk list index. */
static void insert_freechunk (struct small_chunk_table*, gc_head*, size_t);

/* Consolidate a free chunk list index into a single list. */
static struct gc_small_chunk* merge_freeindex (struct small_chunk_table*);

/* Insert a range of free memory into the free chunk and blocks list. */
static void insert_freemem (struct small_chunk_table*,
			    struct gc_small_block**, void*, void*);

/* Sweep the small object area. */
static void sweep_small (int, void**, int);

/* Do selective sweeping. */
static void sweep_small_selective (void**, int);

/* Do traditional sweeping. */
static void sweep_small_normal (void);

/* Walk marked objects in small object area. */
static void walk_small_objects (void);

/* Name        : gc_malloc_small
   Description : Allocate memory from small object area.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     SIZE must include preheader overhead and be aligned. */
void*
gc_malloc_small (size_t size)
{
    gc_head *tmp;

    assert(size > 0);
    assert(size % MEMALIGN == 0);
    assert(size < GC_MAX_SMALL_OBJECT_SIZE);

    LOCK();

    /* Allocate memory. */
    tmp = small_cursor;
    small_cursor = (gc_head*)((char*)tmp + size);

    /* Check if allocation is valid. */
    if (small_cursor > small_bound) {
	/* Use slower allocator instead. */
	small_cursor = (gc_head*)((char*)small_cursor - size);
	tmp = slow_small_allocate(size);
    }

    memset(tmp, 0, size);
    *tmp = size;	/* Set object header. */

    UNLOCK();

    return tmp+1;
}

/* Name        : expand_small_area
   Description : Expand the small object area.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     The free blocks list *must* be empty when this is called.  This
     may fail, but it doesn't notify the caller of problems. */
static void
expand_small_area (void)
{
    /* The initial memory to use for the block tables. */
    static unsigned init_table[SMALL_BLOCKS_TABLE_SIZE];

    /* The current table where block usage tables are parceled out. */
    static unsigned *table = init_table;

    /* The next portion of the above table to use. */
    static int table_pos;

    struct gc_region *region, *node, *next;

    assert(small_blocks == NULL);

    region = region_allocate(GC_REGIONSIZE, GC_REGION_SMALL);
    if (region == NULL)
	return;

    /* FIXME: support region merging. */

    if (table_pos >= SMALL_BLOCKS_TABLE_SIZE) {
	/* Allocate new memory for the block usage tables. */
	EH_NATIVE_DURING
	    table = gc_malloc_fixed(SMALL_BLOCKS_TABLE_SIZE*sizeof(unsigned));
	EH_NATIVE_HANDLER
	    UNLOCK();	/* Undo the lock done in gc_malloc_small(). */
	    throwExternalException((Hjava_lang_Object*)captive_exception);
	EH_NATIVE_ENDHANDLER

	table_pos = 0;
    }

    /* Assign memory for the block usage table. */
    region->data = &table[table_pos];
    table_pos += GC_REGIONSIZE / (GC_BLOCKSIZE * CHAR_BIT);

    /* Insert the new region into the region list.  Since it's likely
       that memory newly allocated from the system will be above the
       currently used memory, we'll start searching from the tail of
       the region list.

       Such a linear insertion is deemed OK since there shouldn't be
       too many regions. */
    next = NULL;
    node = small_regions_tail;
    while (node != NULL) {
	if (node->start < region->start)
	    break;

	next = node;
	node = node->prev;
    }

    region->prev = node;
    if (node == NULL)
	/* Insert into head of region list. */
	small_regions_head = region;
    else
	node->next = region;

    region->next = next;
    if (next == NULL)
	/* Insert into tail of list. */
	small_regions_tail = region;
    else
	next->prev = region;

    /* Insert the blocks in the region into the free blocks list. */
    small_blocks = region->start;
    small_blocks->size = GC_REGIONSIZE;
    small_blocks->next = NULL;

    /* Update statistics. */
    gc_stats.small_size += GC_REGIONSIZE;
}

/* Name        : get_small_block
   Description : Obtain a free block from the free blocks list.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     It is the caller that is responsible for checking whether the
     free blocks list is empty.
   Notes:
     Should be called only by get_small_chunk(). */
static void*
get_small_block (void)
{
    struct gc_small_block *block;

    assert(small_blocks != NULL);
    assert(small_blocks->size > 0);
    assert(small_blocks->size % GC_BLOCKSIZE == 0);
    assert((unsigned)small_blocks % GC_BLOCKSIZE == 0);

    block = small_blocks;

    /* Remember the plural in "free blocks list"?  That's because each
       node in the list is most likely not just a single block.  So we
       don't always switch to the next node on the list. */
    if (block->size == GC_BLOCKSIZE) {
	/* We've used up this entry.  Switch to the next node. */
	small_blocks = block->next;
    } else {
	/* Reset size and location of the block. */
	small_blocks = (struct gc_small_block*)((char*)block + GC_BLOCKSIZE);
	small_blocks->size = block->size - GC_BLOCKSIZE;
	small_blocks->next = block->next;
    }

    return block;
}

/* Name        : get_small_chunk
   Description : The actual implementation for the slow allocator.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Post-condition:
     Returns NULL if no memory can be allocated.
   Notes:
     Should be called only by slow_small_allocate(). */
static void*
get_small_chunk (size_t size)
{
    gc_head *cursor, *bound;

    assert(size > 0);
    assert(size % MEMALIGN == 0);

    if (small_chunks != NULL && (small_chunks->size & ~FREE_BIT) >= size) {
	/* There is a free chunk that can satisfy the request.  We
	   know this since the free chunk list is sorted in decreasing
	   order of size.  So use it. */
	size_t chunk_size;

	chunk_size = small_chunks->size & ~FREE_BIT;
	cursor = (gc_head*)small_chunks;
	bound = (gc_head*)((char*)small_chunks + chunk_size);
	small_chunks = small_chunks->next;
    } else if (small_blocks != NULL) {
	/* Use the available free block. */
	cursor = get_small_block();
	bound = (gc_head*)((char*)cursor + (GC_BLOCKSIZE - GC_HEAD));
	cursor = (gc_head*)((char*)cursor + (MEMALIGN - GC_HEAD));
    } else
	return NULL;

    small_cursor = (gc_head*)((char*)cursor + size);
    small_bound = bound;

    /* Update the allocation statistics. */
    gc_stats.small_alloc += (char*)small_bound - (char*)cursor;

    return cursor;
}

/* Name        : slow_small_allocate
   Description : A slower memory allocator for the small object area.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     To be used if the default fast allocator fails, and is *only* to
     be called in that case. */
static void*
slow_small_allocate (size_t size)
{
    void *p;

    assert(size > 0);
    assert(size % MEMALIGN == 0);

    /* Set up the current free area so that garbage collection works. */
    if (small_cursor < small_bound) {
	*small_cursor = ((char*)small_bound - (char*)small_cursor) | FREE_BIT;

	/* Update allocation statistics. */
	gc_stats.small_alloc -= (char*)small_bound - (char*)small_cursor;
    }

    p = get_small_chunk(size);
    if (p == NULL) {
	/* The first attempt failed.  Try to make more memory. */

	if (HEAPSIZE(gc_stats) + GC_REGIONSIZE <= heap_size) {
	    /* Expand heap. */
	    expand_small_area();
	    p = get_small_chunk(size);
	    if (p == NULL) {
		/* We haven't hit the heap limit, nor even the size
                   we've set for the heap size.  Instead the *system*
                   is being selfish and hadn't given us any resources.
                   Nothing we can do about that ... */
		UNLOCK(); /* Undo the lock done in gc_malloc_small(). */
		throwOutOfMemory();
	    }
	} else {
	    /* Clear cursor and bound pointers so that we can later
	       find out if another thread reset them. */
	    small_cursor = small_bound = NULL;

	    /* Try garbage collection. */
	    gc_invoke(1);

	    /* Since another thread may have been running while the
	       above call was made, it is possible that the cursor and
	       bound pointers are already set.  FIXME: unnecessary
	       when GC_LOCK() actually works as intended */
	    if (small_cursor < small_bound) {
		/* Slack remains.  Reset free area header. */
		*small_cursor =
		    ((char*)small_bound - (char*)small_cursor) | FREE_BIT;

		/* Update allocation statistics. */
		gc_stats.small_alloc -=
		    (char*)small_bound - (char*)small_cursor;
	    }

	    p = get_small_chunk(size);
	    if (p == NULL) {
		/* Still didn't work.  Expand heap. */
		expand_small_area();
		p = get_small_chunk(size);
		if (p == NULL) {
		    /* Ugh, it *still* didn't work, because the heap
                       limit was reached while trying to expand the
                       heap.  There's nothing else for us to do. */
		    UNLOCK(); /* Undo the lock done in gc_malloc_small(). */
		    throwOutOfMemory();
		}
	    }
	}
    }

    assert(p != NULL);

    return p;
}

/* Name        : prepare_gc_small
   Description : Prepare the small object area for a garbage collection.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static void
prepare_gc_small (void)
{
    struct gc_region *region;
    struct gc_small_block *block;

    /* Set up the header in the current free area so that sweeping
       during garbage collection works. */
    if (small_cursor < small_bound) {
	*small_cursor = ((char*)small_bound - (char*)small_cursor) | FREE_BIT;

	/* Update allocation statistics. */
	gc_stats.small_alloc -= (char*)small_bound - (char*)small_cursor;
    }

    small_cursor = small_bound = NULL;

    /* Mark used blocks in the block usage tables.
       This is used for supporting conservative marking. */
    region = small_regions_head;
    block = small_blocks;
    while (region != NULL) {
	unsigned *table;
	void *start, *end;

	table = region->data;
	start = region->start;
	end = region->end;

	/* Initially mark all blocks in the region as used. */
	memset(table, ~0, GC_REGIONSIZE / (GC_BLOCKSIZE * CHAR_BIT));

	while ((void*)block >= start && (void*)block < end) {
	    int i;

	    /* Unmark the free blocks.  FIXME: set 0 for contiguous
	       blocks, instead of setting all of them separately */
	    for (i = 0; i < block->size; i += GC_BLOCKSIZE) {
		int pos, index, offset;

		pos = ((char*)block + i - (char*)start) / GC_BLOCKSIZE;
		index = pos / (sizeof(unsigned)*CHAR_BIT);
		offset = pos % (sizeof(unsigned)*CHAR_BIT);
		table[index] &= ~(1 << offset);
	    }

	    block = block->next;
	}

	region = region->next;
    }
}

/* Name        : is_small_object
   Description : Check if pointer points to object in small object area.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     Works only after prepare_gc_small() has been done. */
static int
is_small_object (void *p, struct gc_region *region)
{
    int pos, index, offset;
    void *block;
    unsigned *table;
    gc_head *header, *bound;

    assert(region != NULL);

    /* Basic checks. */
    if (p == NULL || (unsigned)p & (MEMALIGN-1))
	return 0;

    /* Check if the block containing the pointer is in use. */
    table = region->data;
    pos = ((char*)p - (char*)region->start) / GC_BLOCKSIZE;
    index = pos / (sizeof(unsigned)*CHAR_BIT);
    offset = pos % (sizeof(unsigned)*CHAR_BIT);
    if (!(table[index] & (1 << offset)))
	return 0;

    /* Check if the pointer begins at the starting point of an object. */
    block = (void*)ROUNDDOWNBLOCKSIZE(p);
    header = (gc_head*)((char*)block + (MEMALIGN - GC_HEAD));
    bound = (gc_head*)p - 1;
    while (header < bound) {
	size_t size;

	size = *header & SIZE_MASK;

	assert(size > 0);
	assert(size % MEMALIGN == 0);

	header = (gc_head*)((char*)header+size);
    }

    return (header == bound) && !(*header & FREE_BIT);
}

/* Name        : sort_objects
   Description : Sort the set of live objects.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     The array containing the set must be able to contain each object
     at least twice.
   Post-condition:
     The array containing the sorted set is returned.
   Notes:
     Radix sort is used, with radix 8192. */
static void**
sort_objects (void **set, int n)
{
    static int counts[RADIX_SIZE];

    int shift;
    void **dump;
    struct timeval start, end;

    if (flag_time)
	gettimeofday(&start, NULL);

    /* Set initial output array. */
    dump = set + n;

    /* Use counting sort on each radix. */
    for (shift = 0; shift < sizeof(void*)*CHAR_BIT; shift += RADIX_BITS) {
	int j, m;
	void **tmp;

	/* Clear the counts. */
	for (j = 0; j < RADIX_SIZE; j++)
	    counts[j] = 0;

	/* Count the number of each radix. */
	for (j = 0; j < n; j++) {
	    int radix;

	    radix = ((uintp)set[j] >> shift) & RADIX_MASK;
	    counts[radix]++;
	}

	/* Set the positions for each radix count. */
	for (m = 0, j = 0; j < RADIX_SIZE; j++) {
	    int count;

	    count = counts[j];
	    counts[j] = m;
	    m = m + count;
	}

	/* Create an array sorted on the radix. */
	for (j = 0; j < n; j++) {
	    int radix;

	    radix = ((uintp)set[j] >> shift) & RADIX_MASK;
	    dump[counts[radix]++] = set[j];
	}

	/* Exchange the arrays. */
	tmp = set;
	set = dump;
	dump = tmp;
    }

    if (flag_time) {
	gettimeofday(&end, NULL);
	gc_stats.sort =
	    (double)(end.tv_sec - start.tv_sec)
	    + (end.tv_usec - start.tv_usec) / 1000000.0;
	gc_stats.total_sort += gc_stats.sort;
    }

    return set;
}

/* Name        : init_freeindex
   Description : Initialize a free chunk list index table.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static void
init_freeindex (struct small_chunk_table *table)
{
    int i;

    for (i = 0; i < SMALL_CHUNKS_LIST_INDEX_SIZE; i++) {
	table->table[i].head_chunk.next = NULL;
	table->table[i].tail = &table->table[i].head_chunk;
    }
}

/* Name        : insert_freechunk
   Description : Insert a free chunk into the free chunk list index table.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static void
insert_freechunk (struct small_chunk_table *table, gc_head *chunk, size_t size)
{
    int index;
    struct gc_small_chunk *node;

    assert(chunk != NULL);
    assert(size > 0 && size < GC_BLOCKSIZE);
    assert(size % MEMALIGN == 0);

    index = size/MEMALIGN;
    node = (struct gc_small_chunk*)chunk;

    assert(index < SMALL_CHUNKS_LIST_INDEX_SIZE);

    /* Insert into free chunk list. */
    table->table[index].tail->next = node;
    table->table[index].tail = node;

    node->size = size | FREE_BIT;

    assert(*chunk == (size | FREE_BIT));
}

/* Name        : merge_freeindex
   Description : Merge the free chunk index table into a single list.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     A singly-linked list is returned, which is sorted in decreasing
     order of size. */
static struct gc_small_chunk*
merge_freeindex(struct small_chunk_table *table)
{
    int i;
    struct gc_small_chunk dummy;	/* Acts as initial head of list. */
    struct gc_small_chunk *tail;

    dummy.next = NULL;
    tail = &dummy;

    for (i = SMALL_CHUNKS_LIST_INDEX_SIZE-1; i > 0; i--) {
	if (table->table[i].head_chunk.next != NULL) {
	    /* Append to list. */
	    tail->next = table->table[i].head_chunk.next;
	    tail = table->table[i].tail;
	}
    }

    tail->next = NULL;

    return dummy.next;
}

/* Name        : insert_freemem
   Description : Insert a range of free memory into the free lists.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     This is meant to be a convenience function so that a given range
     of memory can be properly divided and inserted into the free
     chunk list and the free blocks list appropriately.

     The given memory range is first divided into three parts: the
     free chunk located in the first block, the contiguous blocks in
     the middle, and the free chunk located in the last block.  Some
     or even all of these may be empty.

     These three parts are then freed independently. */
static void
insert_freemem (struct small_chunk_table *table,
		struct gc_small_block **tail_block,
		void *start, void *end)
{
    void *cursor;
    void *first_start, *first_end;
    void *middle_start, *middle_end;
    void *last_start, *last_end;

    /* If the starting address is adjacent to a block boundary, then
       move it to the boundary itself. */
    if ((uintp)start % GC_BLOCKSIZE == MEMALIGN - GC_HEAD)
	start = (char*)start - (MEMALIGN - GC_HEAD);
    else if ((uintp)start % GC_BLOCKSIZE == GC_BLOCKSIZE - GC_HEAD)
	start = (char*)start + GC_HEAD;

    /* If the ending address is adjacent to a block boundary, then
       move it to the boundary itself. */
    if ((uintp)end % GC_BLOCKSIZE == MEMALIGN - GC_HEAD)
	end = (char*)end - (MEMALIGN - GC_HEAD);
    else if ((uintp)end % GC_BLOCKSIZE == GC_BLOCKSIZE - GC_HEAD)
	end = (char*)end + GC_HEAD;

    if (start == end)
	/* No memory to free. */
	return;

    cursor = start;
    first_start = first_end = NULL;
    middle_start = middle_end = NULL;
    last_start = last_end = NULL;

    /* Shear off the first part.  This never starts at a block boundary. */
    if ((uintp)cursor % GC_BLOCKSIZE != 0) {
	void *block_end;

	first_start = cursor;

	block_end = (void*)ROUNDUPBLOCKSIZE((uintp)cursor);
	if (end < block_end)
	    first_end = end;
	else
	    first_end = (char*)block_end - GC_HEAD;

	cursor = block_end;
    }

    /* Shear off the middle part. */
    if (cursor < (void*)ROUNDDOWNBLOCKSIZE((uintp)end)) {
	void *last;

	assert((uintp)cursor % GC_BLOCKSIZE == 0);

	last = (void*)ROUNDDOWNBLOCKSIZE((uintp)end);
	middle_start = cursor;
	middle_end = last;
	cursor = last;
    }

    /* Set the last part.  This never ends at a block boundary. */
    if (cursor < end) {
	assert((uintp)end % GC_BLOCKSIZE != 0);
	assert((uintp)cursor % GC_BLOCKSIZE == 0);

	last_start = (char*)cursor + GC_HEAD;
	last_end = end;
    }

    /* Free the memory for the first part. */
    if (first_start < first_end)
	insert_freechunk(table, first_start,
			 (char*)first_end - (char*)first_start);

    /* Free the memory for the middle part. */
    if (middle_start < middle_end) {
	struct gc_small_block *block;

	block = (struct gc_small_block*)middle_start;
	block->size = (char*)middle_end - (char*)middle_start;
	block->next = NULL;
	(*tail_block)->next = block;
	*tail_block = block;

	assert((uintp)block->size % GC_BLOCKSIZE == 0);
    }

    /* Free the memory for the last part. */
    if (last_start < last_end)
	insert_freechunk(table, last_start,
			 (char*)last_end - (char*)last_start);
}

/* Name        : sweep_small
   Description : Sweep the small object area.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     STATE is the state of the mark stack, which determines the
     sweeping algorithm. */
static void
sweep_small (int state, void **objects, int n)
{
    if (state == MARK_STATE_COLLECT) {
	sweep_small_selective(objects, n);
    } else {
	assert(state == MARK_STATE_NORMAL);

	sweep_small_normal();
    }
}

/* Name        : sweep_small_selective
   Description : Selectively sweep the small object area.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     The set of live objects must be contained in MARK_STACK. */
static void
sweep_small_selective (void **objects, int n)
{
    static struct small_chunk_table index;

    int i;
    struct gc_small_block dummy_block;
    struct gc_small_block *tail_block;
    struct gc_region *region;

    /* Insert sentinels into the set of live objects. */
    objects[n++] = NULL;
    objects[n++] = (void*)(~0);

    objects = sort_objects(objects, n);

    i = 0;
    region = small_regions_head;
    dummy_block.next = NULL;
    tail_block = &dummy_block;
    init_freeindex(&index);

    /* Sweep each region. */
    while (region != NULL) {
	void *region_start, *region_end, *region_first, *region_last;

	region_start = region->start;
	region_end = region->end;

	/* Skip objects that appear before the region. */
	while (objects[i] < region_start)
	    i++;

	if (objects[i] > region_end) {
	    /* There are no live objects in the region.
	       Free all the blocks in it, and skip to next region. */
	    struct gc_small_block *block;

	    block = region_start;
	    block->size = (char*)region_end - (char*)region_start;
	    block->next = NULL;
	    tail_block->next = block;
	    tail_block = block;
	    gc_stats.small_freed += block->size;

	    region = region->next;
	    continue;
	}

	/* The first and last location an object in the region could
           start and end at. */
	region_first = (char*)region_start + (MEMALIGN - GC_HEAD);
	region_last = (char*)region_end - GC_HEAD;

	assert(objects[i] > region_start);
	assert(objects[i] < region_end);

	/* Unmark the first object. */
	*HEADER(objects[i]) &= ~MARK_BIT;

	/* Free memory before the first live object in the region. */
	if ((char*)objects[i] - GC_HEAD > (char*)region_first) {
	    insert_freemem(&index, &tail_block,
			   region_first, HEADER(objects[i]));
	    gc_stats.small_freed +=
		(char*)HEADER(objects[i]) - (char*)region_first;
	}

	/* Free the memory between the live objects in the region. */
	while (objects[i+1] < region_end) {
	    size_t size;
	    void *object_start, *object_end;

	    assert(objects[i] < objects[i+1]);
	    assert(objects[i] > region_start);
	    assert(objects[i] < region_end);
	    assert(objects[i+1] > region_start);
	    assert(objects[i+1] < region_end);

	    /* Unmark the current object. */
	    *HEADER(objects[i+1]) &= ~MARK_BIT;

	    size = SIZE(objects[i]);
	    object_end = (char*)HEADER(objects[i]) + size;
	    object_start = HEADER(objects[i+1]);
	    if (object_end < object_start) {
		insert_freemem(&index, &tail_block, object_end, object_start);
		gc_stats.small_freed +=
		    (char*)object_start - (char*)object_end;
	    }

	    gc_stats.small_marked += size;

	    i++;
	}

	/* Free memory after the last live object in the region. */
	{
	    void *object_end;

	    assert(objects[i] > region_start);
	    assert(objects[i] < region_end);

	    object_end = (char*)HEADER(objects[i]) + SIZE(objects[i]);
	    if (object_end < region_last) {
		insert_freemem(&index, &tail_block, object_end, region_last);
		gc_stats.small_freed += (char*)region_last - (char*)object_end;
	    }

	    gc_stats.small_marked += SIZE(objects[i]);
	}

	region = region->next;
    }

    assert(i < n);

    small_blocks = dummy_block.next;
    small_chunks = merge_freeindex(&index);
}

/* Name        : sweep_small_normal
   Description : Sweep the small object area normally.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     All live objects must have been marked. */
static void
sweep_small_normal (void)
{
    static struct small_chunk_table index;

    int slack_block_size;

    struct gc_region *region;
    struct gc_small_block dummy_block;
    struct gc_small_block *slack_block, *tail_block;

    init_freeindex(&index);
    region = small_regions_head;

    dummy_block.next = NULL;
    tail_block = &dummy_block;

    /* Sweep each region. */
    while (region != NULL) {
	unsigned *table;
	struct gc_small_block *block;

	table = region->data;
	block = region->start;
	slack_block = block;
	slack_block_size = 0;

	/* Sweep each block. */
	while ((void*)block < region->end) {
	    int pos, ind, off;
	    size_t slack_size;
	    gc_head *header, *bound, *slack;

	    /* Check if block is used. */
	    pos = ((char*)block - (char*)region->start) / GC_BLOCKSIZE;
	    ind = pos / (sizeof(unsigned)*CHAR_BIT);
	    off = pos % (sizeof(unsigned)*CHAR_BIT);
	    if (!(table[ind] & (1 << off))) {
		/* Add to slack block and skip to next block. */
		slack_block_size += GC_BLOCKSIZE;
		block = (struct gc_small_block*)((char*)block + GC_BLOCKSIZE);
		continue;
	    }

	    /* Traverse each chunk in the block. */
	    header = (gc_head*)((char*)block + (MEMALIGN - GC_HEAD));
	    slack = header;
	    bound = (gc_head*)((char*)block + (GC_BLOCKSIZE - GC_HEAD));
	    slack_size = 0;
	    while (header < bound) {
		size_t size;

		size = *header & SIZE_MASK;

		assert(size > 0);
		assert(size % MEMALIGN == 0);

		if (*header & MARK_BIT) {
		    /* Process marked object. */
		    assert(size < GC_MAX_SMALL_OBJECT_SIZE);
		    assert(!(*header & FREE_BIT));

		    if (slack_size > 0) {
			/* Insert slack into free chunk list index. */
			insert_freechunk(&index, slack, slack_size);
			gc_stats.small_freed += slack_size;
			slack_size = 0;	/* Reset slack size. */
		    }

		    gc_stats.small_marked += size;
		    *header &= ~MARK_BIT;
		    slack = (gc_head*)((char*)header + size);
		} else {
		    /* Unused memory. */
		    slack_size += size;	/* Extend slack. */
		}

		/* Go to next chunk. */
		header = (gc_head*)((char*)header + size);
	    }

	    if (slack_size > 0
		&& (char*)slack == (char*)block + (MEMALIGN - GC_HEAD)) {
		/* The entire block is free. */
		slack_block_size += GC_BLOCKSIZE;
	    } else {
		if (slack_size > 0) {
		    /* Handle the last free chunk in the block. */
		    insert_freechunk(&index, slack, slack_size);
		    gc_stats.small_freed += slack_size;
		}

		if (slack_block_size > 0) {
		    /* Insert the slack block into free blocks list. */
		    slack_block->size = slack_block_size;
		    slack_block->next = NULL;
		    tail_block->next = slack_block;
		    tail_block = slack_block;
		    gc_stats.small_freed += slack_block_size;
		    slack_block_size = 0;
		}

		/* Set beginning of next potential free blocks. */
		slack_block =
		    (struct gc_small_block*)((char*)block + GC_BLOCKSIZE);
	    }

	    assert(header == bound);

	    /* Go to next block. */
	    block = (struct gc_small_block*)((char*)block + GC_BLOCKSIZE);
	}

	if (slack_block_size > 0) {
	    /* Insert into free blocks list. */
	    slack_block->size = slack_block_size;
	    slack_block->next = NULL;
	    tail_block->next = slack_block;
	    tail_block = slack_block;
	    gc_stats.small_freed += slack_block_size;
	}

	region = region->next;
    }

    small_chunks = merge_freeindex(&index);
    small_blocks = dummy_block.next;
}

/* Name        : walk_small_objects
   Description : Walk marked objects in small object area.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     This is used for handling mark stack overflows. */
static void
walk_small_objects (void)
{
    struct gc_region *region;

    region = small_regions_head;
    while (region != NULL) {
	unsigned *table;
	struct gc_small_block *block;

	table = region->data;
	block = region->start;

	while ((void*)block < region->end) {
	    int pos, ind, off;
	    gc_head *header, *bound;

	    /* Check if block is used. */
	    pos = ((char*)block - (char*)region->start) / GC_BLOCKSIZE;
	    ind = pos / (sizeof(unsigned)*CHAR_BIT);
	    off = pos % (sizeof(unsigned)*CHAR_BIT);
	    if (!(table[ind] & (1 << off))) {
		block = (struct gc_small_block*)((char*)block + GC_BLOCKSIZE);
		continue;	/* Skip block. */
	    }

	    /* Traverse each object in block. */
	    header = (gc_head*)((char*)block + (MEMALIGN - GC_HEAD));
	    bound = (gc_head*)((char*)block + (GC_BLOCKSIZE - GC_HEAD));
	    while (header < bound) {
		size_t size;

		size = *header & SIZE_MASK;

		assert(size > 0);
		assert(size % MEMALIGN == 0);

		if (*header & MARK_BIT)
		    WALK((char*)header + GC_HEAD);

		header = (gc_head*)((char*)header + size);
	    }

	    block = (struct gc_small_block*)((char*)block + GC_BLOCKSIZE);
	}

	region = region->next;
    }
}


/*****************************************************************/
/* Large object area manager                                     */
/*****************************************************************/

/* The large object area uses the manual memory manager to manage
   memory.  All objects allocated within this area are referenced by a
   central hash table in order to support conservative garbage
   collection.

   A closed hash table is used, with extra space after each object to
   hold the chained links.  There is a single sentinel for each chain
   which denotes the start and end of the list. */

/* Size of hash table of large objects. */
#define LARGE_SET_SIZE		16381

/* Chained node for hash table to be appended to large objects. */
struct large_set_node {
    void *object;			/* The actual address of the object. */
    struct large_set_node *prev, *next;	/* Chained links. */
};

/* The hash table of large objects. */
static struct large_set_node large_objects[LARGE_SET_SIZE];

/* Information for the large object area. */
static struct gc_heap large_heap;

static int large_set_hash (void*);	/* Get hash value for object. */
static int large_set_exists (void*);	/* Check if object is in set. */
static void large_set_init (void);	/* Initialize large object set. */
static void large_set_add (void*, size_t);	/* Add object to set. */
static void sweep_large (int);		/* Sweep large object area. */

/* Walk marked objects in large object area. */
static void walk_large_objects (void);

/* Name        : large_set_hash
   Description : Calculate a hash code for a large object.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static int
large_set_hash (void *p)
{
    return (unsigned)p / MEMALIGN;
}

/* Name        : large_set_init
   Description : Reset the large object table.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static void
large_set_init (void)
{
    int i;

    for (i = 0; i < LARGE_SET_SIZE; i++) {
	/* Set sentinels. */
	large_objects[i].object = NULL;
	large_objects[i].prev = large_objects[i].next = &large_objects[i];
    }
}

/* Name        : large_set_exists
   Description : Check if pointer is in large object table.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr */
static int
large_set_exists (void *p)
{
    struct large_set_node *node;

    node = large_objects[large_set_hash(p) % LARGE_SET_SIZE].next;
    while (node->object != NULL) {
	if (node->object == p)
	    return 1;	/* Found. */

	node = node->next;
    }

    return 0;	/* Not found. */
}

/* Name        : large_set_add
   Description : Add a pointer to the large object table.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     The given object must include extra space for the chained links.
     SIZE is the original size of the object, not including the links. */
static void
large_set_add (void *p, size_t size)
{
    int align, hash;
    struct large_set_node *node;

    hash = large_set_hash(p) % LARGE_SET_SIZE;

    /* Find chained links, while considering alignment. */
    align = __alignof__(struct large_set_node);
    node = (struct large_set_node*)(((uintp)p + size + align - 1) & -align);

    /* Set links. */
    node->object = p;
    node->prev = &large_objects[hash];
    node->next = large_objects[hash].next;
    large_objects[hash].next->prev = node;
    large_objects[hash].next = node;
}

/* Name        : gc_malloc_large
   Description : Allocate a large object.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
void*
gc_malloc_large (size_t size)
{
    void *p;
    size_t alloc_size;

    LOCK();

    /* Actual size to be allocated. */
    alloc_size = size + sizeof(struct large_set_node);

    p = mem_allocate(&large_heap, alloc_size);
    if (p == NULL) {
	if (HEAPSIZE(gc_stats) + size < heap_size) {
	    /* Expand heap since we haven't reached the size limit. */
	    int r;

	    r = mem_expand(&large_heap, alloc_size);
	    if (r < 0) {
		UNLOCK();
		throwOutOfMemory();
	    }

	    p = mem_allocate(&large_heap, alloc_size);

	    assert(p != NULL);
	} else {
	    /* Garbage collect and try again. */
	    gc_invoke(1);
	    p = mem_allocate(&large_heap, alloc_size);

	    if (p == NULL) {
		/* Failed again!  Expand heap. */
		int r;

		r = mem_expand(&large_heap, alloc_size);
		if (r < 0) {
		    UNLOCK();
		    throwOutOfMemory();
		}

		p = mem_allocate(&large_heap, alloc_size);

		assert(p != NULL);
	    }
	}
    }

    assert(p != NULL);

    large_set_add(p, size);

    UNLOCK();

    return p;
}

/* Name        : sweep_large
   Description : Sweep large object area.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     This function doesn't care about the marking stack state. */
static void
sweep_large (int state)
{
    int i;
    struct large_set_node *node;

    /* Remove unmarked objects from hash table and free them. */
    for (i = 0; i < LARGE_SET_SIZE; i++) {
	node = large_objects[i].next;
	while (node->object != NULL) {
	    void *object;
	    struct large_set_node *next;

	    assert(node != NULL);
	    assert(node != &large_objects[i]);
	    assert(node->prev != NULL);
	    assert(node->next != NULL);

	    next = node->next;
	    object = node->object;

	    if (*HEADER(object) & MARK_BIT) {
		*HEADER(object) &= ~MARK_BIT;
		gc_stats.large_marked += SIZE(object);
	    } else {
		gc_stats.large_freed += SIZE(object);
		node->prev->next = node->next;
		node->next->prev = node->prev;
		mem_free(&large_heap, object);
	    }

	    node = next;
	}
    }
}

/* Name        : walk_large_objects
   Description : Walk marked objects in large object area.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     This is used for handling mark stack overflows. */
static void
walk_large_objects (void)
{
    int i;
    struct large_set_node *node;

    for (i = 0; i < LARGE_SET_SIZE; i++) {
	node = large_objects[i].next;
	while (node->object != NULL) {
	    assert(node != NULL);
	    assert(node != &large_objects[i]);
	    assert(node->prev != NULL);
	    assert(node->next != NULL);

	    if (*HEADER(node->object) & MARK_BIT)
		WALK(node->object);

	    node = node->next;
	}
    }
}


/*****************************************************************/
/* Finalization support                                          */
/*****************************************************************/

/* Objects with finalizers are maintained in a linked list.  During
   garbage collection, unreachable objects in this list are moved to a
   separate list and then marked separately, since finalization may
   revive objects so that we don't want to free these objects quite
   yet.  The finalizer thread then finalizes these unreachable
   objects.

   Finalization is done in a stack-like matter.  In other words,
   object that have been put into the finalization list later would be
   finalized earlier.  I'm not sure avoiding the potential "denial of
   finalization" problem would be important ... */

/* Linked list node for objects with finalizers. */
struct finalize_node {
    void *object;		/* The object with the finalizer. */
    void (*final)(void*);	/* The finalizer. */
    struct finalize_node *next;	/* Next node in linked list. */
};

static struct finalize_node *has_final;	/* Objects with finalizers. */
static struct finalize_node *do_final;	/* Objects to be finalized. */

static quickLock finalman;		/* For synchronizing finalization. */
static quickLock final_list_lock;	/* Synchronize access to do_final. */

int gc_finalize_on_exit;	/* Execute all finalizers on exit? */

/* Mark objects with finalizers. */
static void walk_finals (void);

/* Invoke a finalizer for an object. */
static void invoke_finalizer (void *object, void (*final)(void*));

/* Name        : gc_set_finalizer
   Description : Set a finalizer for an object.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     The object must previously not have had a finalizer. */
void
gc_set_finalizer (void *p, gc_type *type)
{
    struct finalize_node *node;

    assert((void*)type->final > GC_OBJECT_SPECIAL);

    /* FIXME: This could be inlined within the object itself, but the
       delayed setting of finalizers for strings and threads makes
       this somewhat difficult. */
    node = gc_malloc_fixed(sizeof(struct finalize_node));
    node->object = p;
    node->final = type->final;

    lockMutex(&final_list_lock);
    node->next = has_final;
    has_final = node;
    unlockMutex(&final_list_lock);
}

/* Name        : gc_finalize_invoke
   Description : Suggest that finalization of dead objects be done.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     Execution of the finalizers after this returns is not guaranteed. */
void
gc_finalize_invoke (void)
{
    lockMutex(&finalman);

    /* Wakeup the finalizer thread.  We don't wait for it to complete
       since we don't have to and its effect on the whole system is
       negligible (I think). */
    signalCond(&finalman);

    unlockMutex(&finalman);
}

/* Name        : gc_finalize_main
   Description : Main loop for finalizer thread.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
void
gc_finalize_main (void)
{
    /* Threads are presumably started with interrupts disabled. */
    intsRestore();

    lockMutex(&finalman);
    while (1) {
	waitCond(&finalman, 0);

	/* Finalize the objects in DO_FINAL. */
	while (do_final != NULL) {
	    void *object;
	    void (*finalizer)(void*);
	    struct finalize_node *node;

	    /* Lock each node extraction from DO_FINAL.  This is done
               here instead of outside of the loop, since a finalizer
               may allocate normal objects, so no locks should
               surround the execution of the finalizer.  Otherwise
               deadlocks may result.

	       The assignment to OBJECT should prevent premature
	       freeing of the object during garbage collection since
	       it is conservative. */
	    lockMutex(&final_list_lock);
	    node = do_final;
	    object = node->object;
	    finalizer = node->final;
	    do_final = node->next;
	    gc_free(node);
	    unlockMutex(&final_list_lock);

	    invoke_finalizer(object, finalizer);
	}
    }
}

/* Name        : gc_execute_finalizers
   Description : Execute all finalizers when gc_finalize_on_exit is true.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     Should only be called by the exiting function exitInternal(). */
void
gc_execute_finalizers (void)
{
    struct finalize_node *node;

    if (!gc_finalize_on_exit)
	return;

    gc_finalize_on_exit = 0; /* The function should be entered only once. */

    lockMutex(&final_list_lock);

    gc_mode = GC_DISABLED; /* We don't want a GC on program exit. */

    /* Execute all finalizers on DO_FINAL. */
    node = do_final;
    while (node != NULL) {
	invoke_finalizer(node->object, node->final);
	node = node->next;
    }

    /* Execute all finalizers on HAS_FINAL. */
    node = has_final;
    while (node != NULL) {
	/* Don't finalize the current thread yet. */
	if (node->object != currentThread)
	    invoke_finalizer(node->object, node->final);

	node = node->next;
    }

    unlockMutex(&final_list_lock);

    /* Finalize current thread. */
    invoke_finalizer((Hjava_lang_Object*)currentThread, gc_finalize_thread);
}

/* Name        : walk_finals
   Description : Walk objects with finalizers.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     All reachable objects must have been marked. */
static void
walk_finals (void)
{
    struct finalize_node *node, *next;

    lockMutex(&final_list_lock);

    /* Moved unmarked objects in HAS_FINAL to DO_FINAL.  The marked
       objects are put into a new HAS_FINAL list. */
    node = has_final;
    has_final = NULL;
    while (node != NULL) {
	next = node->next;

	if (*HEADER(node) & MARK_BIT) {
	    node->next = has_final;
	    has_final = node;
	} else {
	    node->next = do_final;
	    do_final = node;
	}

	node = next;
    }

    /* Put the objects in DO_FINAL into the mark stack, since these
       unmarked objects may revive during finalization.  This is done
       separately from the above since DO_FINAL may not have been
       empty at the start of this function. */
    node = do_final;
    while (node != NULL) {
	mark(node->object);
	node = node->next;
    }

    unlockMutex(&final_list_lock);

    /* Process the mark stack again to handle the newly pushed objects. */
    walk_mark_stack();
}

/* Name        : invoke_finalizer
   Description : Invoke a finalizer for an object.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     Uncaught exceptions raised during finalization are ignored. */
static void
invoke_finalizer (void *object, void (*final)(void*))
{
    EH_NATIVE_DURING
	final(object);
    EH_NATIVE_HANDLER
	/* Ignore exceptions. */
    EH_NATIVE_ENDHANDLER
}


/*****************************************************************/
/* Garbage collector                                             */
/*****************************************************************/

/* The garbage collector employs mark and sweep.  For the small object
   area, the sweeping algorithm is selected adaptively depending on
   the heap occupancy.  Garbage in the large object area is always
   collected using the traditional sweeping algorithm.  The fixed heap
   is ignored.

   The actual implementations for the sweeping algorithms for the
   small and large object area can be found in their respective
   sections.

   Root objects are registered separately with gc_attach().  This is
   maintained using a linked list.  However, since root objects never
   revert back to normal objects, there is no bound on the number of
   root objects there can be, and there would be a rather high
   overhead if a node is allocated for each root object, root objects
   are grouped into "root bundles".  Then a linked list of root
   bundles is used to maintain the root objects. */

/* Maximum number of objects in a root bundle. */
#define ROOTBUNDLESIZE		116

/* Initial size for the mark stack.

   FIXME: This must always be larger than some large value when using
   the translator for some unknown reason.  However, it is not due to
   problems with the mark stack overflow handling, but a problem the
   fixed heap memory allocator has with the translator.  Whether this
   is due to a bug in the translator or memory manager is as of yet
   unknown. */
#define INITIAL_MARK_STACK_SIZE	(256*1024)

/* A bundle of root objects. */
struct gc_root_bundle {
    int size;			/* Number of objects in bundle. */

    /* Next node in root bundle list. */
    struct gc_root_bundle *next;

    /* Set of root objects. */
    struct {
	void *object;		/* The object itself. */
	void (*walk)(void*);	/* The walking function for the object. */
    } roots[ROOTBUNDLESIZE];
};

static int gc_running;		/* Is the garbage collector running? */
static quickLock gcman;		/* For synchronizing the garbage collector. */

/* The initial root bundle. */
static struct gc_root_bundle init_root_bundle;

/* Set of root objects. */
static struct gc_root_bundle *roots = &init_root_bundle;

static int mark_state;		/* State of the marking phase. */

static void **mark_stack;	/* The mark stack. */
static int mark_stack_size;	/* Size of mark stack. */

/* Use global registers for the following variables if possible, since
   they are used so often and hence it would be worthwhile to put them
   in registers.  This should be OK since no external function, except
   for gettimeofday() and lockMutex(), is called in the duration these
   variables are used. */
#if sparc
register void **mark_top	asm("%g5");
register void **mark_cursor	asm("%g6");
register void **mark_bound	asm("%g7");
#else /* not sparc */
static void **mark_top;		/* Top of mark stack. */
static void **mark_cursor;	/* The lowest unwalked marked object. */
static void **mark_bound;	/* Bound for mark stack. */
#endif /* not sparc */

static void **mark_save_cursor;	/* Used for state changes. */

int gc_mode = GC_DISABLED;			/* Do garbage collection? */

static int is_object (void*);		/* Check if reference is valid. */

static void mark_potential (void*);	/* Mark from potential reference. */

/* Suggest a size limit for the set of live objects. */
static int mark_stack_suggest_bound (int heap_size);

static void walk_roots (void);		/* Mark root objects. */
static void walk_mark_stack (void);	/* Mark reachable objects. */

/* Handle mark stack overflow. */
static void handle_stack_overflow (void);

static void mark_phase (void);		/* Execute mark phase. */
static void sweep_phase (void);		/* Execute sweep phase. */

/* Name        : gc_fixed_init
   Description : Initialize memory management system.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     This must be called before using the memory manager. */
void
gc_fixed_init (void)
{
    assert(MEMALIGN > ~SIZE_MASK);

    region_init();
    mem_init(&fixed_heap, &gc_stats.fixed_size,
	     &gc_stats.fixed_alloc, GC_REGION_FIXED);
    mem_init(&large_heap, &gc_stats.large_size,
	     &gc_stats.large_alloc, GC_REGION_LARGE);
}

/* Name        : gc_init
   Description : Initialize the garbage collector.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     gc_fixed_init() has already been called.
   Notes:
     Settings for GC_HEAP_ALLOCATION_SIZE will take effect only when
     this is called, not before nor after. */
void
gc_init (void)
{
    large_set_init();

    heap_size = ROUNDUPREGIONSIZE(gc_heap_allocation_size);

    /* Allocate the mark stack. */
    mark_stack_size = INITIAL_MARK_STACK_SIZE;
    mark_stack = gc_malloc_fixed(sizeof(void*)*INITIAL_MARK_STACK_SIZE);
}

/* Name        : gc_attach
   Description : Attach a root object to the garbage collector.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     The object must not be from the garbage collected heap.
     The memory type must have an appropriate walk function. */
void
gc_attach (void *root, gc_type *type)
{
    assert(root != NULL);
    assert(roots != NULL);

    LOCK();

    if (roots->size >= ROOTBUNDLESIZE) {
	/* Add new root bundle to the root bundle list. */
	struct gc_root_bundle *bundle;

	EH_NATIVE_DURING
	    bundle = gc_malloc_fixed(sizeof(struct gc_root_bundle));
	EH_NATIVE_HANDLER
	    UNLOCK();
	    throwExternalException((Hjava_lang_Object*)captive_exception);
	EH_NATIVE_ENDHANDLER

	bundle->size = 0;
	bundle->next = roots;
	roots = bundle;
    }

    /* Add new root object. */
    roots->roots[roots->size].object = root;
    roots->roots[roots->size].walk = type->walk;
    roots->size++;

    UNLOCK();
}

/* Name        : is_object
   Description : Check if a pointer points to a valid object.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static int
is_object (void *p)
{
    struct gc_region *region;

    region = region_find(p);

    if (region == NULL)
	return 0;

    if (region->type == GC_REGION_SMALL)
	return is_small_object(p, region);
    else if (region->type == GC_REGION_LARGE)
	return large_set_exists(p);
    else
	return 0;
}

/* Name        : mark
   Description : Mark a reachable object.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     Tried to keep this a leaf procedure. */
static void
mark (void *p)
{
    if (p == NULL || (*HEADER(p) & MARK_BIT))
	return;

    assert(is_object(p));

    *HEADER(p) |= MARK_BIT; /* Mark P. */

    /* Check for mark stack overflow. */
    if (mark_top < mark_bound) {
	/* Push P onto mark stack. */
	mark_top++;
	*mark_top = p;
    } else {
	/* Oops, an overflow occurred ... */
	if (mark_state == MARK_STATE_COLLECT) {
	    /* Give up trying to construct set of live objects. */
	    mark_save_cursor = mark_cursor;
	    mark_cursor = mark_top + 2;
	    mark_bound = mark_stack + mark_stack_size - 1;
	    mark_state = MARK_STATE_NORMAL;

	    /* Push P onto mark stack. */
	    mark_top++;
	    *mark_top = p;
	} else {
	    /* Now this is a real overflow .. */
	    assert(mark_state != MARK_STATE_COLLECT);

	    mark_state = MARK_STATE_OVERFLOW;
	}
    }
}

/* Name        : mark_potential
   Description : Mark an object from a potential reference.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static void
mark_potential (void *p)
{
    if (is_object(p))
	mark(p);
}

/* Name        : mark_stack_suggest_bound
   Description : Calculate a recommended bound for the mark stack.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     If there are only a few live objects, selective sweeping is
     better, but traditional sweeping is better when there are a lot
     of them.  This function recommends the boundary between the two
     cases. */
static int
mark_stack_suggest_bound (int heap_size)
{
    size_t b;

    b = heap_size / 128;
    if (2 * b >= mark_stack_size)
	b = mark_stack_size / 2;

    return b;
}

/* Name        : handle_stack_overflow
   Description : Handle mark stack overflow.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     Go through the entire heap and walk all marked objects.  Since an
     object can be marked only once, and this function guarantees to
     mark at least a few reachable but still unmarked objects,
     walk_mark_stack() will eventually terminate correctly.  This is
     very slow, but mark stack overflows should occur only in
     extremely abnormal cases, so it should not be much of a
     problem.

     FIXME: test this */
static void
handle_stack_overflow (void)
{
    walk_large_objects();
    walk_small_objects();
}

/* Name        : walk_roots
   Description : Walk root objects.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     Objects referenced from the roots are put into the mark stack. */
static void
walk_roots (void)
{
    Hjava_lang_Thread *tid;
    struct gc_root_bundle *bundle;

    /* Walk root objects. */
    for (bundle = roots; bundle != NULL; bundle = bundle->next) {
	int i;

	for (i = 0; i < bundle->size; i++)
	    (bundle->roots[i].walk)(bundle->roots[i].object);
    }

    /* Walk global variables. */
    mark(OutOfMemoryError);

    /* Walk the thread stacks. */
    for (tid = liveThreads; tid != NULL; tid = TCTX(tid)->nextlive) {
	mark(tid);
	gc_walk_thread(tid);
    }
}

/* Name        : walk_mark_stack
   Description : Mark the reachable objects using a mark stack.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static void
walk_mark_stack (void)
{
    if (mark_state == MARK_STATE_COLLECT) {
	/* Construct the set of live objects.  MARK_CURSOR will be
           artificially increased beyond MARK_TOP if there seems to be
           too many live objects. */
	while (mark_cursor <= mark_top) {
	    WALK(*mark_cursor);
	    mark_cursor++;
	}

	/* If the state didn't change, simply return. */
	if (mark_state == MARK_STATE_COLLECT)
	    return;

	assert(mark_state == MARK_STATE_NORMAL);
	assert(mark_bound == mark_stack + mark_stack_size - 1);

	/* Moves boundary set to conserve memory.  I'm not sure if the
	   increase in time would be worth the decrease in space,
	   though ... */
	memmove(mark_stack, mark_save_cursor,
		(char*)mark_top - (char*)mark_save_cursor + sizeof(void*));
	mark_top = mark_stack + (mark_top - mark_save_cursor);
    }

    while (1) {
	/* Mark the reachable objects. */
	while (mark_top >= mark_stack) {
	    void *p;

	    p = *mark_top;
	    mark_top--;
	    WALK(p);
	}

	/* Check for overflow. */
	if (mark_state == MARK_STATE_OVERFLOW) {
	    /* Handle mark stack overflow.
	       FIXME: perhaps the stack should be expanded */
	    if (flag_gc)
		fprintf(stderr, "<GC: mark stack overflow occurred>\n");
	    mark_state = MARK_STATE_NORMAL;
	    handle_stack_overflow();
	} else {
	    assert(mark_state == MARK_STATE_NORMAL);
	    return;
	}
    }
}

/* Name        : mark_phase
   Description : Execute the mark phase.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static void
mark_phase (void)
{
    struct timeval start, end;
    void **bound;

    if (flag_time) {
	gettimeofday(&start, NULL);
    }

    mark_state = MARK_STATE_NORMAL;
    mark_top = &mark_stack[-1];
    mark_bound = mark_stack + mark_stack_size - 1;

    walk_roots();

    bound = mark_stack + mark_stack_suggest_bound(HEAPSIZE(gc_stats));

    assert(bound < mark_bound);

    if (mark_top < bound) {
	mark_cursor = &mark_stack[0];
	mark_bound = bound;
	mark_state = MARK_STATE_COLLECT;
    }

    assert(mark_bound < mark_stack + mark_stack_size);

    walk_mark_stack();
    walk_finals();

    if (flag_time) {
	gettimeofday(&end, NULL);
	gc_stats.mark =
	    (double)(end.tv_sec - start.tv_sec)
	    + (end.tv_usec - start.tv_usec) / 1000000.0;
	gc_stats.total_mark += gc_stats.mark;
    }
}

/* Name        : sweep_phase
   Description : Sweep the heap for garbage.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static void
sweep_phase (void)
{
    struct timeval start, end;

    if (flag_time)
	gettimeofday(&start, NULL);

    sweep_small(mark_state, mark_stack, mark_top - mark_stack + 1);
    sweep_large(mark_state);

    if (flag_time) {
	gettimeofday(&end, NULL);
	gc_stats.sweep =
	    (float)(end.tv_sec - start.tv_sec)
	    + ((end.tv_usec - start.tv_usec) / 1000000.0);
	gc_stats.total_sweep += gc_stats.sweep;
    }
}

/* Name        : expand_heap
   Description : Expand size limit of garbage collected heap.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     The actual GC has just finished, but the allocation statistics
     have not been reset yet.
   Notes:
     This expands the size limit of the garbage collected heap
     according to some heuristics, which are necessary to avoid
     spending too much time in garbage collection.  It doesn't really
     have to do anything, though.

     A lot of other heuristics should also be explored. */
static void
expand_heap (void)
{
    int alloced, marked, amount, target;

    /* Expand the heap so that the amount of allocated memory should
       be larger than the amount of marked memory in the next garbage
       collection. */
    alloced = gc_stats.small_alloc;
    marked = gc_stats.small_marked;
    amount = marked - alloced;

    if (amount > 0) {
	amount = ROUNDUPREGIONSIZE(amount);
	target = amount + heap_size;
    } else
	target = heap_size;

    /* Make sure the expansion does not force the memory size to go
       over the limit.  The GC_HEAP_LIMIT is rounded up to region
       sizes just in case it suddenly changed. */
    gc_heap_limit = ROUNDUPREGIONSIZE(gc_heap_limit);
    if (target + gc_stats.fixed_size > gc_heap_limit)
	heap_size = gc_heap_limit - gc_stats.fixed_size;
    else
	heap_size = target;
}

/* Name        : gc_invoke
   Description : Wakeup the garbage collection thread.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     LACK is non-zero iff. this was called because of lack of space. */
void
gc_invoke (int lack)
{
    if (gc_mode == GC_DISABLED)
	return;

    LOCK();
    lockMutex(&gcman);

    /* If the garbage collector is already running, wait for it to
       finish instead of trying to invoke garbage collection again. */
    if (!gc_running) {
	if (lack)
	    gc_running = 2;
	else
	    gc_running = 1;

	signalCond(&gcman);
    }
    waitCond(&gcman, 0);

    unlockMutex(&gcman);
    UNLOCK();
}

/* Name        : gc_main
   Description : The main loop for the garbage collection thread.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
void
gc_main (void)
{
    struct timeval start, end;

    /* All threads presumably start with interrupts disabled. */
    intsRestore();

    lockMutex(&gcman);

    while (1) {
	gc_running = 0;
	waitCond(&gcman, 0);

	assert(gc_running);

	if (flag_time)
	    gettimeofday(&start, NULL);

	prepare_gc_small();

	/* Reset statistics. */
	gc_stats.small_marked = gc_stats.large_marked = 0;
	gc_stats.small_freed = gc_stats.large_freed = 0;
	gc_stats.iterations++;

	mark_phase();
	sweep_phase();

	/* If GC_RUNNING is set to 2, then GC was triggered by an out
           of space condition, so expand heap according to
           heuristics. */
	if (gc_running == 2)
	    expand_heap();

	if (flag_time) {
	    gettimeofday(&end, NULL);
	    gc_stats.gc =
		(double)(end.tv_sec - start.tv_sec)
		+ ((end.tv_usec - start.tv_usec) / 1000000.0);
	    gc_stats.total_gc += gc_stats.gc;
	}

	if (flag_gc)
	    fprintf(stderr,
		    "<GC %d: heap %dK, alloc %dK, "
		    "mark %dK, free %dK, fixed %dK>\n",
		    gc_stats.iterations,
		    HEAPSIZE(gc_stats) / 1024,
		    (gc_stats.small_alloc + gc_stats.large_alloc) / 1024,
		    (gc_stats.small_marked + gc_stats.large_marked) / 1024,
		    (gc_stats.small_freed + gc_stats.large_freed) / 1024,
		    gc_stats.fixed_size / 1024);

	/* Reset statistics. */
	gc_stats.small_alloc = 0;
	gc_stats.large_alloc = 0;
	gc_stats.fixed_alloc = 0;

	if (do_final != NULL)
	    gc_finalize_invoke();

	broadcastCond(&gcman);
    }
}


/*****************************************************************/
/* Type specific walking functions                               */
/*****************************************************************/

gc_type gc_no_walk =		{ gc_walk_null, GC_OBJECT_NORMAL };
gc_type gc_fixed =		{ gc_walk_null, GC_OBJECT_FIXED };
gc_type gc_prim_array =		{ gc_walk_null, GC_OBJECT_NORMAL };
gc_type gc_ref_array =		{ gc_walk_ref_array, GC_OBJECT_NORMAL };
gc_type gc_class_object =	{ gc_walk_class, GC_OBJECT_ROOT };
gc_type gc_finalizing_object =	{ gc_walk_null, gc_finalize_object };
gc_type gc_method =		{ gc_walk_null, GC_OBJECT_FIXED };
gc_type gc_field =		{ gc_walk_null, GC_OBJECT_FIXED };
gc_type gc_static_data =	{ gc_walk_null, GC_OBJECT_FIXED };
gc_type gc_dispatch_table =	{ gc_walk_null, GC_OBJECT_FIXED };
gc_type gc_bytecode =		{ gc_walk_null, GC_OBJECT_FIXED };
gc_type gc_exception_table =	{ gc_walk_null, GC_OBJECT_FIXED };
gc_type gc_constant =		{ gc_walk_null, GC_OBJECT_FIXED };
gc_type gc_utf8const =		{ gc_walk_null, GC_OBJECT_FIXED };
gc_type gc_interface =		{ gc_walk_null, GC_OBJECT_FIXED };
gc_type gc_jit_code =		{ gc_walk_null, GC_OBJECT_FIXED };
gc_type gc_lock =		{ gc_walk_null, GC_OBJECT_FIXED };
gc_type gc_eit =		{ gc_walk_null, GC_OBJECT_FIXED };
gc_type gc_closed_hash_table =	{ gc_walk_null, GC_OBJECT_FIXED };
gc_type gc_thread =		{ gc_walk_null, gc_finalize_thread };
gc_type gc_finalizing_string =	{ gc_walk_null, gc_finalize_string };

/* Name        : gc_walk_ref_array
   Description : Walk a reference array.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
void
gc_walk_ref_array (void *p)
{
    Hjava_lang_Object *arr;
    Hjava_lang_Object **ptr, **end;

    arr = (Hjava_lang_Object*)p;
    ptr = OBJARRAY_DATA(arr);
    end = ptr + ARRAY_SIZE(arr);

    while (ptr < end)
	mark(*(ptr++));
}

/* Name        : gc_walk_class
   Description : Walk a class object.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
void
gc_walk_class (void *p)
{
    Hjava_lang_Class *class;

    class = (Hjava_lang_Class*)p;
    mark(class->loader);

    if (class->state >= CSTATE_PREPARED) {
	Field *end, *fld;
	constants *info;
	int i;

	/* Mark the resolved constant strings. */
	info = &class->constants;
	for (i = 1; i < info->size; i++)
	    if (info->tags[i] == CONSTANT_ResolvedString)
		mark((void*)info->data[i]);

	/* Walk the static reference fields. */
	fld = CLASS_SFIELDS(class);
	end = fld + CLASS_NSFIELDS(class);
	for (; fld < end; fld++)
	    if (FIELD_ISREF(fld) && fld->type != ClassClass)
		mark(*(void**)FIELD_ADDRESS(fld));
    }
}

/* Name        : gc_walk_null
   Description : Walk an object with no references.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
void
gc_walk_null (void *p)
{
    /* Do nothing. */
}

/* Name        : gc_walk_conservative
   Description : Walk a region of memory conservatively.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
void
gc_walk_conservative (void *base, uint32 size)
{
    void **p, **end;

    p = base;
    end = (void**)((char*)base+size);

    while (p < end)
	mark_potential(*(p++));
}

/* Name        : gc_walk_thread
   Description : Walk the stack for a thread.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
void
gc_walk_thread (void *base)
{
    Hjava_lang_Thread *tid;

    tid = (Hjava_lang_Thread*)base;

    if (unhand(tid)->PrivateInfo != 0) {
	ctx* ct;

	ct = TCTX(tid);

	/* Nothing in context worth looking at except the stack */
#if defined(STACK_GROWS_UP)
	gc_walk_conservative(ct->stackBase, ct->restorePoint - ct->stackBase);
	gc_walk_Conservative(ct->jstackBase,
			     ct->jrestorePoint - ct->jstackBase);
#elif defined(USE_TRANSLATOR_STACK)
	if (ct->curStack == NULL)
	    gc_walk_conservative(ct->restorePoint,
				 ct->stackEnd - ct->restorePoint);
	else
	    gc_walk_conservative(ct->curStack, ct->stackEnd - ct->curStack);
#else /* not defined(USE_TRANSLATOR_STACK) */
	gc_walk_conservative(ct->restorePoint,
			     ct->stackEnd - ct->restorePoint);
#endif /* not defined(USE_TRANSLATOR_STACK) */

#ifdef INTERPRETER
	gc_walk_conservative(ct->jrestorePoint,
			     ct->jstackEnd - ct->jrestorePoint);
#endif
    }
}


/* Create walk functions at run-time. */

#if sparc

/* Classes are never unloaded.  Thus there is no need to free space
   allocated to walk functions, so there is no need to keep track of
   buffers that have been completely filled.

   Of course, this must change if class unloading is implemented. */

/* The alignment for code fragments. */
#define CODEALIGN	32

/* Round up to code alignment. */
#define ROUNDUPCODE(p)	(((uintp)(p) + (CODEALIGN - 1)) & -CODEALIGN)

/* The bound of the buffer where walk functions are stored. */
static void* buffer_bound;

/* The pointer pointing to the current position in the buffer. */
static void* buffer_point;

/* Name        : allocate_walker_space
   Description : Allocate space for a walk function.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static void*
allocate_walker_space (size_t size)
{
    void *p;

    size = ROUNDUPCODE(size);

    if ((char*)buffer_bound - (char*)buffer_point < size) {
	/* Get new buffer. */
	size_t bsize;

	bsize = ROUNDUPBLOCKSIZE(size + CODEALIGN);
	buffer_point = gc_malloc_fixed(bsize);
	buffer_bound = (char*)buffer_point + bsize;
	buffer_point = (void*)ROUNDUPCODE(buffer_point);
    }

    p = buffer_point;
    buffer_point = (char*)buffer_point + size;

    return p;
}

/*   save         : insn_RRC(2, 0x3c, REG_sp, REG_sp, 0);
     add_int_cont : insn_RRC(2, 0x10, w, r, o);
                    - w = dest reg, r = source reg, o = const
     call         : insn_call( disp )
     ret          : insn_RRC(2, 0x38, REG_g0, REG_i7, 8);
     restore      : insn_RRR(0x3D, REG_g0, REG_g0, REG_g0); */
#define REG_g0			0
#define REG_i0			24
#define REG_o0			8
#define REG_i7			31
#define REG_sp			14
#define MASKL13BITS		0x00001FFF
#define insn_call(disp)				\
	0x40000000 | ((disp) & 0x3FFFFFFF)
#define insn_RRC(op, op2, dst, rs1, cnst)	\
	0x00002000 | ((op) << 30)		\
	| ((dst) << 25) | ((op2) << 19)		\
	| ((rs1) << 14) | ((cnst) & MASKL13BITS)
#define insn_RRR(op, dst, rs1, rs2)		\
	0x80000000 | ((dst) << 25)		\
	| ((op) << 19) | ((rs1) << 14) | (rs2)
#define ldst_RRC(op, dst, rs1, cnst)		\
	0xC0002000 | ((dst) << 25)		\
	| ((op) << 19) | ((rs1) << 14)		\
	| ((cnst) & MASKL13BITS)

/* Name        : gc_create_walker
   Description : Create a walk function for a class.
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Notes:
     This is the version optimized for the SPARC. */
void
gc_create_walker (struct Hjava_lang_Class *class)
{
    int disp;
    unsigned *code;
    Field *fld, *start;
    Hjava_lang_Class *c;

    class->walk = allocate_walker_space(12 + class->refField*8);
    code = (unsigned*)class->walk;

    /* Function prologue. */
    *(code++) = insn_RRC(2, 0x3c, REG_sp, REG_sp, -0x70);

    /* Call mark() for each reference field. */
    for (c = class; c != NULL; c = c->superclass) {
	/* Walk from end of object in a probably misguided attempt to
	   reduce cache misses. */
	start = CLASS_IFIELDS(c);
	if (start == NULL)
	    continue;

	fld = start + (CLASS_NIFIELDS(c)-1);
	for (; fld >= start; fld--)
	    if (FIELD_ISREF(fld)) {
		disp = ((int)mark - (int)code)/4;
		*(code++) = insn_call(disp);
		*(code++) = ldst_RRC(0, REG_o0, REG_i0, FIELD_OFFSET(fld));
	    }
    }

    /* Function epilogue. */
    *(code++) = insn_RRC(2, 0x38, REG_g0, REG_i7, 8);
    *(code++) = insn_RRR(0x3D, REG_g0, REG_g0, REG_g0);
}

#else /* not sparc */

/* Name        : object_walker
   Description : Portable version of a object walk function.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     A platform-specific walk function generated at run-time is
     faster.  However, out of memory situations or a platform without
     appropriate support may require such a portable version to
     exist. */
static void
object_walker (void *object)
{
    Hjava_lang_Class *c;
    Hjava_lang_Object *o;

    /* Call mark() for each reference field. */
    o = (Hjava_lang_Object*)object;
    for (c = OBJECT_CLASS(o); c != NULL; c = c->superclass) {
	Field *fld, *start;

	start = CLASS_IFIELDS(c);
	if (start == NULL)
	    continue;

	fld = start + (CLASS_NIFIELDS(c)-1);
	for (; fld >= start; fld--)
	    if (FIELD_ISREF(fld))
		mark(*(void**)((char*)object + FIELD_OFFSET(fld)));
    }
}

/* Name        : gc_create_walker
   Description : Return walking function for class.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     This is the portable version. */
void
gc_create_walker (struct Hjava_lang_Class *class)
{
    class->walk = object_walker;
}

#endif /* not sparc */
