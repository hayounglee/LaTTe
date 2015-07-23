/*
 * java.io.ObjectInputStream.c
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "lib-license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.co.uk>, 1997.
 *
 * Modified by MASS Laboratory, SNU, 1999
 */

#include "config.h"
#include "config-std.h"
#include "config-io.h"
#include "config-mem.h"
#include <native.h>
#include "defs.h"
#include "files.h"
#include "system.h"
#include "java.io.stubs/ObjectInputStream.h"
#include "gtypes.h"
#include "runtime/classMethod.h"
#include "runtime/support.h"
#include "runtime/lookup.h"

struct Hjava_lang_Class*
java_io_ObjectInputStream_loadClass0(struct Hjava_io_ObjectInputStream* stream, struct Hjava_lang_Class* cls, struct Hjava_lang_String* str)
{
	unimp("Serializable interface not implemented");
}

void
java_io_ObjectInputStream_inputClassFields(struct Hjava_io_ObjectInputStream* stream, struct Hjava_lang_Object* obj, struct Hjava_lang_Class* cls, HArrayOfInt* arr)
{
	unimp("Serializable interface not implemented");
}

struct Hjava_lang_Object*
java_io_ObjectInputStream_allocateNewObject(struct Hjava_lang_Class* cls, struct Hjava_lang_Class* cls2)
{
	unimp("Serializable interface not implemented");
}

struct Hjava_lang_Object*
java_io_ObjectInputStream_allocateNewArray(struct Hjava_lang_Class* cls, jint sz)
{
	unimp("Serializable interface not implemented");
}

jbool
java_io_ObjectInputStream_invokeObjectReader(struct Hjava_io_ObjectInputStream* stream, struct Hjava_lang_Object* obj, struct Hjava_lang_Class* cls)
{
	unimp("Serializable interface not implemented");
}
