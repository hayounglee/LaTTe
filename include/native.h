/*
 * native.h
 * Native method support.
 *
 * Copyright (c) 1996,97 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.co.uk>
 */

#ifndef __native_h
#define __native_h

#undef	__NORETURN__
#if defined(__GNUC__)
#define	__NORETURN__	__attribute__((noreturn))
#else
#define	__NORETURN__
#endif

#include <stdlib.h>
#include <jtypes.h>

struct _methods;
struct Hjava_lang_Class;

#include <java_lang_Object.h>

/* Build an object handle */
#define	HandleTo(class)					\
	typedef struct H##class {			\
		Hjava_lang_Object base;			\
		struct Class##class data[1];		\
	} H##class

/* Turn a handle into the real thing */
#define	unhand(o)	((o)->data)

/* Some internal machine object conversions to "standard" types. */
typedef	struct Hjava_lang_Class*	HClass;

/* Include array types */
#include "Arrays.h"

/* Get the strings */
#include <java_lang_String.h>

/* Various useful function prototypes */
struct _strconst;
extern char*	javaString2CString(struct Hjava_lang_String*, char*, int);
extern char*	makeCString(struct Hjava_lang_String*);
extern struct Hjava_lang_String* makeJavaString(char*, int);
extern int	equalUtf8JavaStrings(struct _strconst*, Hjava_lang_String*);

extern jword	do_execute_java_method(void*, struct Hjava_lang_Object*, char*, char*, struct _methods*, int, ...);
extern jword	do_execute_java_class_method(char*, char*, char*, ...);
extern Hjava_lang_Object* execute_java_constructor(void*, char*, struct Hjava_lang_Class*, char*, ...);

extern void			SignalError(void*, char*, char*) __NORETURN__;

extern Hjava_lang_Object*	AllocObject(char*);
extern Hjava_lang_Object*	AllocArray(int, int);
extern Hjava_lang_Object*	AllocObjectArray(int, char*);

extern void			addNativeMethod(char*, void*);
extern void			classname2pathname(char*, char*);

/* Redirect the malloc/free functions */
extern void*			gc_malloc_fixed(size_t);
extern void			gc_free(void*);

#define	malloc(A)		gc_malloc_fixed(A)
#define	calloc(A, B)		gc_malloc_fixed((A)*(B))
#define	free(A)			gc_free(A)

#endif
