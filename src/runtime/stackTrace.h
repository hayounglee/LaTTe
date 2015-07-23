/*
 * stackTrace.h
 *
 * Copyright (c) 1996,97 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Peter Nagy <tegla@katalin.csoma.elte.hu>
 * Modified by MASS Laboratory, Seoul National University
 */

#ifndef __stacktrace_h
#define __stacktrace_h

Hjava_lang_Object*	buildStackTrace(struct _exceptionFrame* base);
Hjava_lang_Object*	getClassContext(void* bulk);
Hjava_lang_Class*	getClassWithLoader(int* depth);
jint			classDepth(char* name);

#endif
