/*
 * java.lang.Float.c
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
#include "runtime/classMethod.h"
#include <native.h>
#include "defs.h"
#include "java.lang.stubs/Float.h"

/*
 * Convert float into a string.
 */
struct Hjava_lang_String*
java_lang_Float_toString(jfloat val)
{
	char str[MAXNUMLEN];

	sprintf(str, "%g", val);
	return (makeJavaString(str, strlen(str)));
}

struct Hjava_lang_Double;
extern double java_lang_Double_valueOf0(struct Hjava_lang_String*);

/*
 * Convert string to float object. (JDK 1.0.2)
 */
struct Hjava_lang_Float*
java_lang_Float_valueOf(struct Hjava_lang_String* str)
{
	struct Hjava_lang_Float* obj;
	obj = (struct Hjava_lang_Float*)execute_java_constructor(0, "java.lang.Float", 0, "()V");
	unhand(obj)->value = java_lang_Double_valueOf0(str);
	return (obj);
}

/*
 * Convert float to bits.
 */
jint
java_lang_Float_floatToIntBits(jfloat val)
{
	return (*(jint*)&val);
}

/*
 * Convert bits to float.
 */
float
java_lang_Float_intBitsToFloat(jint val)
{
	return (*(jfloat*)&val);
}
