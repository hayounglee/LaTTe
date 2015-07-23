/*
 * java.lang.Throwable.c
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "lib-license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 * Modified by MASS Laboratory, Seoul National University, 1999.
 */

#include <stdio.h>
#include <assert.h>
#include <native.h>
#include "runtime/classMethod.h"
#include "runtime/support.h"
#include "java.io.stubs/FileDescriptor.h"
#include "java.io.stubs/FileOutputStream.h"
#include "java.io.stubs/PrintStream.h"
#include "java.lang.stubs/Throwable.h"

extern Hjava_lang_Object* buildStackTrace(void*);

/*
 * Print a line for a stack trace.
 */
static void
print_trace_line (Method* method, int bpc, Hjava_lang_Object *target)
{
	/* No overflows should occur since we're explicitly specifying
           the maximum width of strings in sprintf(). */
	char buf[220];

	Hjava_lang_Object *carray;

	if (method == NULL)
		return;

	/* Construct the string to output.
	   FIXME: consider line numbers */
	if (bpc == -1)
		sprintf(buf, "\tat %.80s.%.80s(compiled code)",
			CLASS_CNAME(method->class), method->name->data);
	else
		sprintf(buf, "\tat %.80s.%.80s(bpc: %d)",
			CLASS_CNAME(method->class), method->name->data, bpc);

	carray = makeJavaCharArray(buf, strlen(buf));
	do_execute_java_method(0, target, "println", "([C)V", 0, 0, carray);
}

/*
 * Fill in stack trace information - don't know what thought.
 */
struct Hjava_lang_Throwable*
java_lang_Throwable_fillInStackTrace(struct Hjava_lang_Throwable* o)
{
	unhand(o)->backtrace = buildStackTrace(0);
	return (o);
}

/*
 * Dump the stack trace to the given stream.
 */
void
java_lang_Throwable_printStackTrace0(struct Hjava_lang_Throwable* o,
				     struct Hjava_lang_Object* p)
{
	jword *trace;

	trace = (jword*)unhand(o)->backtrace;

	if (trace != NULL) {
		int i, n;

		n = (int)trace[1];

		for (i = 0; i < n; i++) {
			int bpc;
			Method *meth;

			meth = (Method*)trace[2*i+2];
			bpc = (int)trace[2*i+3];
			print_trace_line(meth, bpc, p);
		}

		do_execute_java_method(0, p, "flush", "()V", 0, 0);
	}
}
