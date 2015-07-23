/*
 * support.h
 * Various support routines.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 * Modified by MASS Laboratory, SNU, 1999.
 */

#ifndef __support_h
#define	__support_h

#include <stdio.h>

/* For user defined properties */
typedef struct _userProperty {
        char*                   key;
        char*                   value;
        struct _userProperty*   next;
} userProperty;

typedef struct _nativeFunction {
	char*			name;
	void*			func;
} nativeFunction;

#define	NATIVE_FUNC_INCREMENT	(256)

extern nativeFunction* native_funcs;

extern userProperty* userProperties;

struct Hjava_lang_String;

extern void		setProperty(void*, char*, char*);
extern void		classname2pathname(char*, char*);
extern struct Hjava_lang_Object* makeJavaCharArray(char*, int);
extern struct Hjava_lang_String* makeJavaString(char*, int);
extern char*		javaString2CString(struct Hjava_lang_String*, char*, int);
extern char*		makeCString(struct Hjava_lang_String*);
extern struct Hjava_lang_String* makeReplaceJavaStringFromUtf8(unsigned char*, int, int, int);
extern jword		do_execute_java_method(void*, Hjava_lang_Object*, char*, char*, struct _methods*, int, ...);
extern jword		do_execute_java_class_method(char* cname, char* method_name, char* signature, ...);
extern void             do_execute_java_main_method(char* cname, ...);

extern Hjava_lang_Object* execute_java_constructor(void*, char*, struct Hjava_lang_Class*, char*, ...);
extern jlong		currentTime(void);
extern void		addNativeMethod(char*, void*);

#define	jboolean_do_execute_java_method	do_execute_java_method
#define	jbyte_do_execute_java_method	do_execute_java_method
#define	jchar_do_execute_java_method	do_execute_java_method
#define	jshort_do_execute_java_method	do_execute_java_method
#define	jint_do_execute_java_method	do_execute_java_method
#define	jref_do_execute_java_method	do_execute_java_method

extern jlong		jlong_do_execute_java_method(void*, Hjava_lang_Object*, char*, char*, struct _methods*, int, ...);
extern jfloat		jfloat_do_execute_java_method(void*, Hjava_lang_Object*, char*, char*, struct _methods*, int, ...);
extern jdouble		jdouble_do_execute_java_method(void*, Hjava_lang_Object*, char*, char*, struct _methods*, int, ...);

struct _strconst;
extern void SignalError(void *, char *, char *);
extern void unimp(char*) __attribute__ ((noreturn));
extern struct Hjava_lang_String* Utf8Const2JavaString(struct _strconst*);
extern void kprintf(FILE*, const char*, ...);

#endif
