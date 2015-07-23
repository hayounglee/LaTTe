/*
 * java.lang.reflect.Constructor.c
 *
 * Copyright (c) 1996,97 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.co.uk>
 *
 * Modified by MASS Laboratory, SNU, 1999
 */

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "gtypes.h"
#include "access.h"
#include "runtime/constants.h"
#include "runtime/object.h"
#include "runtime/classMethod.h"
#include "runtime/itypes.h"
#include "runtime/support.h"
#include "java.lang.reflect.stubs/Constructor.h"
#include <native.h>
#include "defs.h"

jint
java_lang_reflect_Constructor_getModifiers(struct Hjava_lang_reflect_Constructor* this)
{
	Hjava_lang_Class* clazz;
	jint slot;

	clazz = unhand(this)->clazz;
	slot = unhand(this)->slot;

	assert(slot < clazz->nmethods);

	return (clazz->methods[slot].accflags);
}

struct Hjava_lang_Object*
java_lang_reflect_Constructor_newInstance(struct Hjava_lang_reflect_Constructor* this, HArrayOfObject* obj)
{
	unimp("java.lang.reflect.Constructor:newInstance not implemented");
	return (0);
}
