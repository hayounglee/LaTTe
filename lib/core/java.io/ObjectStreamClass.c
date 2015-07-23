/*
 * java.io.ObjectStreamClass.c
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
#include "java.io.stubs/ObjectStreamClass.h"
#include "java.io.stubs/ObjectStreamField.h"
#include "java.lang.reflect.stubs/Field.h"
#include "runtime/support.h"
#include "runtime/classMethod.h"
#include "runtime/support.h"
#include "runtime/lookup.h"

jint
java_io_ObjectStreamClass_getClassAccess(struct Hjava_lang_Class* cls)
{
	unimp("Serializable interface not implemented");
}

HArrayOfObject*
java_io_ObjectStreamClass_getMethodSignatures(struct Hjava_lang_Class* cls)
{
	unimp("Serializable interface not implemented");
}

jint
java_io_ObjectStreamClass_getMethodAccess(struct Hjava_lang_Class* cls, struct Hjava_lang_String* str)
{
	unimp("Serializable interface not implemented");
}

HArrayOfObject*
java_io_ObjectStreamClass_getFieldSignatures(struct Hjava_lang_Class* cls)
{
	unimp("Serializable interface not implemented");
}

jint
java_io_ObjectStreamClass_getFieldAccess(struct Hjava_lang_Class* cls, struct Hjava_lang_String* str)
{
	unimp("Serializable interface not implemented");
}

HArrayOfObject*
java_io_ObjectStreamClass_getFields0(struct Hjava_io_ObjectStreamClass* stream, struct Hjava_lang_Class* cls)
{
	unimp("Serializable interface not implemented");
}

jlong
java_io_ObjectStreamClass_getSerialVersionUID(struct Hjava_lang_Class* cls)
{
	unimp("Serializable interface not implemented");
}

jbool
java_io_ObjectStreamClass_hasWriteObject(struct Hjava_lang_Class* cls)
{
	unimp("Serializable interface not implemented");
}
