/*
 * java.lang.Double.c
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "lib-license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 *
 * Modified by MASS Laboratory, SNU, 1999
 */

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include <math.h>
#include "gtypes.h"
#include "runtime/locks.h"
#include "runtime/classMethod.h"
#include "defs.h"
#include "java.lang.stubs/Double.h"
#include <native.h>

/*
 * Convert double to string.
 */
struct Hjava_lang_String*
java_lang_Double_toString(double val)
{
	char str[MAXNUMLEN];

	sprintf(str, "%g", val);
	return (makeJavaString(str, strlen(str)));
}

/*
 * Convert string to double object.
 */
double
java_lang_Double_valueOf0(struct Hjava_lang_String* str)
{
	double value;
	char buf[MAXNUMLEN];
	char* endbuf;

	javaString2CString(str, buf, sizeof(buf));

#if defined(HAVE_STRTOD)
	value = strtod(buf, &endbuf);
	if (*endbuf != 0) {
		SignalError(0, "java.lang.NumberFormatException", "Bad double format");
	}
#else
	/* Fall back on old atof - no error checking */
	value = atof(buf);
#endif

	return (value);
}

/*
 * Convert string into double class. (JDK 1.0.2)
 */
struct Hjava_lang_Double*
java_lang_Double_valueOf(struct Hjava_lang_String* str)
{
	struct Hjava_lang_Double* obj;
	obj = (struct Hjava_lang_Double*)execute_java_constructor(0, "java.lang.Double", 0, "()V");
	unhand(obj)->value = java_lang_Double_valueOf0(str);
	return (obj);
}

/*
 * Convert double to jlong.
 */
jlong
java_lang_Double_doubleToLongBits(double val)
{
	return (*(jlong*)&val);
}

/*
 * Convert jlong to double.
 */
double
java_lang_Double_longBitsToDouble(jlong val)
{
	return (*(double*)&val);
}
