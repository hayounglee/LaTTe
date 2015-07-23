/*
 * lookup.h
 * Various lookup calls for resolving objects, methods, exceptions, etc.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 */

#ifndef __lookup_h
#define __lookup_h

struct _dispatchTable;

typedef struct _callInfo {
	short			in;
	short			out;
	char*			signature;
	char			rettype;
} callInfo;

typedef struct _exceptionInfo {
	uintp			handler;
	Hjava_lang_Class*	class;
	Method*			method;
} exceptionInfo;

Method *getMethodSignatureClass(constIndex, Method*);
void	getMethodArguments(constIndex, Method*, callInfo*);
Field  *getField(constIndex, bool, Method*, Hjava_lang_Class**);

void	checkAccess(Hjava_lang_Class*, Hjava_lang_Class*, accessFlags);

Method* find_clinit(Hjava_lang_Class* class);

Method* findMethod(Hjava_lang_Class*, Utf8Const*, Utf8Const*);
Method* findMethodLocal(Hjava_lang_Class*, Utf8Const*, Utf8Const*);
void	findExceptionInMethod(uintp, Hjava_lang_Class*, exceptionInfo*);
bool	findExceptionBlockInMethod(uintp, Hjava_lang_Class*, Method*, exceptionInfo*);

#endif
