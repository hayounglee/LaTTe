/* stackTrace.c
   Handle stack traces.
   
   Written by: Yoo C. Chung <chungyc@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Laboratory, Seoul National University
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

/* Items to move outside of this file are surrounded by this define.
   FIXME: Items surrounded by this define should be moved. */
#define STACKTRACE_ALIEN

#include "config.h"
#include "jtypes.h"
#include "md.h"
#include "classMethod.h"
#include "baseClasses.h"
#include "thread.h"
#include "gc.h"
#include "stackTrace.h"

#ifdef INTERPRETER
#include "java_lang_Throwable.h"
#include "interpreter.h"
#endif /* INTERPRETER */

#ifdef TRANSLATOR
#include "method_inlining.h"
#include "exception_info.h"
#endif

/* Used for filling class contexts. */
struct class_context {
    int size;
    Hjava_lang_Object *context;
};

/* Used for finding class loaders. */
struct class_loader_search {
    int depth;
    Hjava_lang_Class *loader;
};

/* Used for finding classes. */
struct class_search {
    char *name;
    int depth;
    Hjava_lang_Class *class;
};

/* Used for filling stack traces. */
struct stack_trace {
    int size;
    jword *trace;
};

/* When walking stacks, functions of this type are given as arguments
   to walk_stack().  This accepts the method pointer executing the
   current stack frame, the bytecode PC, and an extra function
   dependent pointer, and returns a non-zero value iff. stack walking
   should be stopped.  Functions of this type may, and probably will,
   have side effects. */
typedef int (*guard) (Method*, int, void*);

/* Support for walking stack traces by the garbage collector. */
static struct Hjava_lang_Class trace_class = { walk: gc_walk_null };
static struct _dispatchTable trace_dtable = { class: &trace_class };

static void walk_stack (exceptionFrame*, guard, void*);

static int fill_in_context (Method*, int, void*);
static int find_class_loader (Method*, int, void*);
static int find_class (Method*, int, void*);
static int count_valid_frame (Method*, int, void*);
static int fill_in_trace (Method*, int, void*);

/* FIXME: the functions here should be moved elsewhere */
#ifdef STACKTRACE_ALIEN

#ifdef INTERPRETER
extern exceptionFrame* intrp_walk_stack (exceptionFrame*, guard, void*);
#endif

#ifdef TRANSLATOR
extern exceptionFrame* kaffe_walk_stack (MethodInstance*, exceptionFrame*,
					 guard, void*);
extern exceptionFrame* MI_walk_stack (MethodInstance*, exceptionFrame*,
				      guard, void*);
#endif /* TRANSLATOR */

#ifdef INTERPRETER
/* Name        : intrp_walk_stack
   Description : Walk interpreter stack.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     FRAME is the starting frame.
     FUNC is the function which determines when to stop.
     INFO is an argument to FUNC.
   Post-condition:   
     Either the caller frame or NULL is returned.  The latter happens
     when the stack should no longer be walked. */
exceptionFrame*
intrp_walk_stack (exceptionFrame *frame, guard func, void *info)
{
    char *bpc;
    void *fp;	/* Interpreter frame pointer. */
    Method *m;

    frame = (exceptionFrame*)frame->retbp;
    fp = (void*)frame->lcl[0];
    m = (Method*)frame->arg[0];
    bpc = (char*)frame->arg[2];

    while (m != NULL) {
	if (func(m, (char*)bpc - (char*)m->bcode, info) != 0)
	    return NULL;

	/* FIXME: change this to easier to read code once integrated
           into intrp-exception.c */
	bpc = (char*)((void**)fp)[3];	/* Get caller bytecode PC. */
	m = (Method*)((void**)fp)[4];	/* Get caller method. */
	fp = ((void**)fp)[6];		/* Get caller frame pointer. */
    }

    return (exceptionFrame*)frame->retbp;
}
#endif /* INTERPRETER */

#ifdef TRANSLATOR
/* Name        : kaffe_walk_stack
   Description : Walk stack for methods translated by Kaffe's translator.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     INSTANCE is the method instance.
     BASE is the starting stack frame.
     FUNC is the function which determines when to stop.
     INFO is an argument to FUNC.
   Post-condition:
     Either the caller frame or NULL is returned.  The latter happens
     when the stack should no longer be walked. */
exceptionFrame*
kaffe_walk_stack (MethodInstance *instance, exceptionFrame *base,
		  guard func, void *info)
{
    if (func(MI_GetMethod(instance), -1, info) != 0)
	return NULL;

    return (exceptionFrame*)base->retbp;
}

/* Name        : MI_walk_stack
   Description : Walk stack for translated methods.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     INSTANCE is the method instance.
     BASE is the starting stack frame.
     FUNC is the function which determines when to stop.
     INFO is an argument to FUNC.
   Post-condition:
     Either the caller frame or NULL is returned.  The latter happens
     when the stack should no longer be walked. */
exceptionFrame*
MI_walk_stack (MethodInstance *instance, exceptionFrame *base,
	       guard func, void *info)
{
    ExceptionInfoTable *eitable;

    eitable = MI_GetExceptionInfoTable(instance);

    if (eitable != NULL) {
	ExceptionInfo *einfo;

	einfo = EIT_find(eitable, PCFRAME(base));

	if (EIT_GetNativePC(einfo) != 0) {
	    /* Traverse each inlined method included in the current
               stack frame. */
	    int bpc;
	    MethodInstance *member;

	    bpc = EIT_GetBytecodePC(einfo);
	    member = EIT_GetMethodInstance(einfo);

	    do {
		if (func(MI_GetMethod(member), bpc, info))
		    return NULL;

		bpc = MI_GetRetBPC(member);
		member = MI_GetCaller(member);
	    } while (member != NULL);
	}
    }

    return (exceptionFrame*)base->retbp;
}
#endif /* TRANSLATOR */

#endif /* STACKTRACE_ALIEN */

/* Name        : getClassContext
   Description : Return class context.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     I'm not really sure what this is about.  (It's just a rewritten
     version of the original function included in Kaffe.)  So it is
     also completely untested. */
Hjava_lang_Object*
getClassContext (void *bulk)
{
    register exceptionFrame *frame asm("%sp");
    int count;
    struct class_context context;

    count = 0;
    walk_stack(frame, count_valid_frame, &count);

    context.size = 0;
    context.context = newRefArray(ClassClass, count);
    walk_stack((exceptionFrame*)frame->retbp, fill_in_context, &context);

    return context.context;
}

/* Name        : getClassWithLoader
   Description : Find a class with class loader and its associated stack depth.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     I'm not really sure what this is about.  (It's just a rewritten
     version of the original function included in Kaffe.)  So it is
     also completely untested.  */
Hjava_lang_Class*
getClassWithLoader (int *depth)
{
    register exceptionFrame *frame asm("%sp");
    struct class_loader_search searcher;

    searcher.depth = -1;
    searcher.loader = NULL;

    walk_stack((exceptionFrame*)frame->retbp, find_class_loader, &searcher);

    if (searcher.loader == NULL)
	*depth = -1;

    return searcher.loader;
}

/* Name        : classDepth
   Description : Search for stack depth at which class is used.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     I'm not really sure what this is about.  (It's just a rewritten
     version of the original function included in Kaffe.)  So it is
     also completely untested.  */
jint
classDepth (char *name)
{
    register exceptionFrame *frame asm("%sp");
    struct class_search searcher;

    searcher.name = name;
    searcher.depth = 0;
    searcher.class = NULL;
    walk_stack((exceptionFrame*)frame->retbp, find_class, &searcher);

    if (searcher.class == NULL)
	return -1;
    else
	return searcher.depth;
}

/* Name        : buildStackTrace
   Description : Build stack trace.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     BASE is the starting stack frame for the trace.

   Post-condition:
     The return value is a value that should never be used except in
     printStackTrace0().  It is an array (*not* a Java array!) which
     stores the the method pointer and the bytecode PC (if any) pair,
     and starts with the number of entries in the trace.  For example,
     the stack trace

       method1 (BPC: 10)
       method2 (BPC: 12)

     would be stored as [ 2, &method1, 10, &method2, 12 ] (or
     something like that).  BPCs for compiled methods are -1.  Method
     pointers for native code are NULL.

     This is done to improve the speed of generating exceptions, since
     this should be much more faster than filling in a array of
     strings.  This is justified since a lot of exceptions are usually
     discarded without printing their stack trace.

   Notes:
     Never, *ever*, should anything use the result returned from this
     function, with the sole exception of printStackTrace0() and its
     closely associated functions. */
Hjava_lang_Object*
buildStackTrace (exceptionFrame *base)
{
    int depth;
    struct stack_trace trace;

    asm volatile ("ta 3"); /* Spill registers. */

    /* Set base stack frame if it's not already set. */
    if (base == NULL) {
	register exceptionFrame *frame asm("%sp");

	base = (exceptionFrame*)frame->retbp;
    }

    /* Count stack depth. */
    depth = 0;
    walk_stack(base, count_valid_frame, &depth);

    /* The memory for the stack trace is allocated as memory not to be
       walked since it does not refer to memory managed by the garbage
       collector. */
    trace.trace = gc_malloc((2*depth+2) * sizeof(jword), &gc_no_walk);
    ((Hjava_lang_Object*)trace.trace)->dtable = &trace_dtable;
    trace.trace[1] = (jword)depth;
    trace.size = 0;

    /* Fill in stack trace. */
    walk_stack(base, fill_in_trace, &trace);

    return (Hjava_lang_Object*)trace.trace;
}

/* Name        : walk_stack
   Description : Walk the stack, processing each stack frame.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     BASE is the stack frame where walking should start.
     FUNC is the guard function.
     INFO is a pointer that may be used by FUNC. */
static void
walk_stack (exceptionFrame *base, guard func, void *info)
{
    exceptionFrame *frame;

    frame = base;
    while (frame != NULL && FRAMEOKAY(frame)) {
#ifdef TRANSLATOR
	MethodInstance *instance;
#endif

#ifdef INTERPRETER
	if (intrp_inside_interpreter((void*)PCFRAME(frame))) {
	    frame = intrp_walk_stack(frame, func, info);
	    continue;
	}
#endif /* INTERPREER */

#ifdef TRANSLATOR
	instance = findMethodFromPC(PCFRAME(frame));
	if (instance == NULL) {
	    NEXTFRAME(frame);
	    continue;
	}

#ifndef INTERPRETER
	if (!MI_IsLaTTeTranslated(instance)) {
	    frame = kaffe_walk_stack(instance, frame, func, info);
	    continue;
	}
#endif /* not INTERPRETER */

	/* Count depth for inlined methods. */
	frame = MI_walk_stack(instance, frame, func, info);
#else /* not TRANSLATOR */
	NEXTFRAME(frame);
#endif /* not TRANSLATOR */
    }
}

/* Name        : fill_in_context
   Description : Fill in class context.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static int
fill_in_context (Method *m, int bpc, void *info)
{
    struct class_context *context;

    context = (struct class_context*)info;

    OBJARRAY_DATA(context->context)[context->size] =
	(Hjava_lang_Object*)m->class;

    context->size++;

    fprintf(stderr,"class %s, loader %p\n",
	    CLASS_CNAME(m->class), m->class->loader);

    return 0;	/* Always continue walking stack. */
}

/* Name        : find_class_loader
   Description : Find class with class loader.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static int
find_class_loader (Method *m, int bpc, void *info)
{
    struct class_loader_search *searcher;

    searcher = (struct class_loader_search*)info;

    if (m->class->loader != NULL) {
	searcher->loader = m->class;
	return 1;
    } else {
	searcher->depth++;
	return 0;
    }
}

/* Name        : find_class
   Description : Find class.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static int
find_class (Method *m, int bpc, void *info)
{
    struct class_search *searcher;

    searcher = (struct class_search*)info;
    if (strcmp(CLASS_CNAME(m->class), searcher->name) == 0) {
	searcher->class = m->class;
	return 1;
    } else {
	searcher->depth++;
	return 0;
    }
}

/* Name        : count_valid_frame
   Description : Increase count for each valid stack frame.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static int
count_valid_frame (Method *m, int bpc, void *info)
{
    (*((int*)info))++;
    return 0;	/* Always continue walking stack. */
}

/* Name        : fill_in_trace
   Description : Fill in stack trace.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr> */
static int
fill_in_trace (Method *m, int bpc, void *info)
{
    struct stack_trace *trace;

    trace = (struct stack_trace*)info;
    trace->trace[2*trace->size+2] = (jword)m;
    trace->trace[2*trace->size+3] = (jword)bpc;
    trace->size++;

    return 0;	/* Always continue walking stack. */
}
