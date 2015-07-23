/*
 * java.io.ObjectOutputStream.c
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
#include "java.io.stubs/ObjectOutputStream.h"
#include "runtime/support.h"
#include "runtime/classMethod.h"
#include "runtime/lookup.h"

void
java_io_ObjectOutputStream_outputClassFields(struct Hjava_io_ObjectOutputStream* stream, struct Hjava_lang_Object* obj, struct Hjava_lang_Class* cls, HArrayOfInt* arr)
{
	unimp("Serializable interface not implemented");
}

jbool
java_io_ObjectOutputStream_invokeObjectWriter(struct Hjava_io_ObjectOutputStream* stream, struct Hjava_lang_Object* obj, struct Hjava_lang_Class* cls)
{
	unimp("Serializable interface not implemented");
}
