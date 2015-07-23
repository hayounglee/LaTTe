/* gc.h
   Interface to the memory management subsystem.
   
   Written by: Yoo C. Chung <chungyc@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Laboratory, Seoul National University.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef _gc_h_INCLUDED
#define _gc_h_INCLUDED

#include <stdlib.h>
#include "gtypes.h"

/* Object header overhead. */
#define GC_HEAD			sizeof(gc_head)

/* Maximum size that is considered "small".
   Must be smaller than 2^16.
   FIXME: performance will probably vary wildly with this. */
#define GC_MAX_SMALL_OBJECT_SIZE	1024

/* Define to enable the garbage collector - recommended */
#define GC_ENABLE

#define GC_DISABLED		0
#define GC_ENABLED		1

/* Size of alignment. */
#define MEMALIGN		sizeof(double)

/* Align sizes to alignment. */
#define	ROUNDUPALIGN(V)		(((uintp)(V) + MEMALIGN - 1) & -MEMALIGN)

/* Declare types defined externally. */
struct Hjava_lang_Class;

typedef uint32 gc_head;		/* Type of header prepended to objects. */

/* Structure for each memory type. */
typedef struct {
    void (*walk)(void*);	/* Walk function.  Only useful for roots. */
    void (*final)(void*);	/* Finalizer function. */
} gc_type;

/* Memory types. */
extern gc_type gc_string_object;
extern gc_type gc_no_walk;
extern gc_type gc_fixed;
extern gc_type gc_prim_array;
extern gc_type gc_ref_array;
extern gc_type gc_class_object;
extern gc_type gc_finalizing_object;
extern gc_type gc_method;
extern gc_type gc_field;
extern gc_type gc_static_data;
extern gc_type gc_dispatch_table;
extern gc_type gc_bytecode;
extern gc_type gc_exception_table;
extern gc_type gc_constant;
extern gc_type gc_utf8const;
extern gc_type gc_interface;
extern gc_type gc_jit_code;
extern gc_type gc_lock;
extern gc_type gc_eit;
extern gc_type gc_closed_hash_table;
extern gc_type gc_thread;
extern gc_type gc_finalizing_string;

/* Statistics for the memory manager. */
extern struct gc_stats {
    /* Sizes of the various heap areas. */
    size_t small_size, large_size, fixed_size;

    /* Various allocation statistics.  Reset after each GC. */
    size_t small_alloc, large_alloc, fixed_alloc;

    /* Garbage collector statistics.  Reset before each GC.*/
    size_t small_marked, small_freed, large_marked, large_freed;

    /* The number of garbage collections done so far. */
    int iterations;

    /* Timings for the garbage collector.  Only collected when
       explicitly told to do so.  In units of seconds. */
    double mark, sort, sweep, gc;
    double total_mark, total_sort, total_sweep, total_gc;
} gc_stats;

/* Whether to do garbage collection.  Set to GC_DISABLED or GC_ENABLED. */
extern int gc_mode;

/* Whether to execute all finalizers on program exit. */
extern int gc_finalize_on_exit;

extern void gc_init (void);		/* Initialize the garbage collector. */
extern void gc_fixed_init (void);	/* Initialize the memory manager. */

/* Allocate small object directly from heap.
   SIZE includes the header overhead, and must be aligned. */
extern void* gc_malloc_small (size_t size);

/* Allocate large object.
   SIZE includes the header overhead, and must be aligned. */
extern void* gc_malloc_large (size_t size);

extern void* gc_malloc (size_t, gc_type*);	/* Allocate typed memory. */

extern void* gc_malloc_fixed (size_t size);	/* Allocate fixed memory. */
extern void* gc_realloc_fixed (void*, size_t);	/* Resize fixed memory. */
extern void gc_free (void*);			/* Free fixed memory. */

#define	gc_calloc(a, b, c)	gc_malloc((a) * (b), (c))
#define	gc_calloc_fixed(a, b)	gc_malloc_fixed((a) * (b))
#define	gc_free_fixed(a)	gc_free(a)

extern void gc_attach (void*, gc_type*);	/* Attach permanent root. */
extern void gc_set_finalizer (void*, gc_type*);	/* Set finalizer. */

extern void gc_invoke (int);		/* Invoke collection. */
extern void gc_finalize_invoke (void);	/* Invoke finalizers. */

extern void gc_main (void);		/* Function for GC thread. */
extern void gc_finalize_main (void);	/* Function for finalizer thread. */

/* Walking functions. */
extern void gc_walk_ref_array (void*);
extern void gc_walk_class (void*);
extern void gc_walk_null (void*);
extern void gc_walk_conservative (void*, uint32);
extern void gc_walk_thread (void*);

/* Finalizing functions. */
extern void gc_finalize_object (void*);	/* Implemented in object.c */
extern void gc_finalize_thread (void*);	/* Implemented in thread.c */
extern void gc_finalize_string (void*);	/* Implemented in string.c */

/* Execute all finalizers when gc_finalize_on_exit is true.
   Should only be run on program exit. */
extern void gc_execute_finalizers (void);

/* Create the walk function for the given class. */
extern void gc_create_walker (struct Hjava_lang_Class*);

/* This is not implemented.  This is just to make the macro GC_WRITE()
   available so that it would be "easier" to change to an incremental
   garbage collector without much of a rewrite.  (Who am I kidding?)  */
#ifdef GC_INCREMENTAL
extern void gc_add_reference (void*, void*);
#define	GC_WRITE(_o, _p)	gc_add_reference (_o, _p)
#else
#define	GC_WRITE(_o, _p)
#endif

#endif /* not _gc_h_INCLUDED */
