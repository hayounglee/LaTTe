/*
 * java.lang.SecurityManager.c
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
#include "gtypes.h"
#include "access.h"
#include "md.h"
#include "runtime/constants.h"
#include "runtime/object.h"
#include "runtime/classMethod.h"
#include "runtime/itypes.h"
#include <native.h>
#include "defs.h"
#include <stdlib.h>
#include "java.lang.stubs/SecurityManager.h"
#include "runtime/stackTrace.h"
#include "runtime/support.h"

extern struct Hjava_lang_Class* getClassWithLoader(int*);

HArrayOfObject* /* HArrayOfClass */
java_lang_SecurityManager_getClassContext(struct Hjava_lang_SecurityManager* this)
{
	return ((HArrayOfObject*)getClassContext(NULL));
}

struct Hjava_lang_ClassLoader*
java_lang_SecurityManager_currentClassLoader(struct Hjava_lang_SecurityManager* this)
{
	int depth;
	struct Hjava_lang_Class* class;

	class = getClassWithLoader(&depth);
	if (class != NULL) {
		return ((struct Hjava_lang_ClassLoader*)(class->loader));
	}
	else {
		return NULL;
	}
}

jint
java_lang_SecurityManager_classDepth(struct Hjava_lang_SecurityManager* this, struct Hjava_lang_String* str)
{
	char buf[MAXNAMELEN];

	javaString2CString(str, buf, sizeof(buf));
	classname2pathname(buf, buf);

	return (classDepth(buf));
}

jint
java_lang_SecurityManager_classLoaderDepth(struct Hjava_lang_SecurityManager* this)
{
	int depth;

	(void)getClassWithLoader(&depth);

	return (depth);
}

struct Hjava_lang_Class*
java_lang_SecurityManager_currentLoadedClass0(struct Hjava_lang_SecurityManager* this)
{
	unimp("java.lang.SecurityManager:currentLoadedClass0 unimplemented");
}
