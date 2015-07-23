/*
 * java.lang.ClassLoader.c
 *
 * Copyright (c) 1996,97 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "lib-license.terms" for information on usage and redistribution
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
#include "runtime/file.h"
#include "runtime/readClass.h"
#include "runtime/constants.h"
#include "runtime/classMethod.h"
#include "runtime/baseClasses.h"
#include "runtime/object.h"
#include "runtime/itypes.h"
#include "runtime/errors.h"
#include "runtime/exception.h"
#include "runtime/exception_handler.h"
#include <native.h>
#include "defs.h"

extern classFile findInJar(char*);

/*
 * Initialise this class loader.
 */
void
java_lang_ClassLoader_init(struct Hjava_lang_ClassLoader* this)
{
	/* Does nothing */
}

/* 
   Name        : java_lang_ClassLoader_defineClass0
   Description : Translate an array of bytes into a class
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: I'm not sure this implementation fully satisfies JVM specification.
   The loading is too complex.....
 */

struct Hjava_lang_Class*
java_lang_ClassLoader_defineClass0(struct Hjava_lang_ClassLoader* this, struct Hjava_lang_String* name, HArrayOfByte* data, jint offset, jint length)
{
    Hjava_lang_Class* clazz;
    classFile hand;
    hand.base = &unhand(data)->body[offset];
    hand.buf = hand.base;
    hand.size = length;
	
    clazz = (Hjava_lang_Class*)newObject(ClassClass);
    clazz = readClass(clazz, &hand, this);

    processClass(clazz, CSTATE_PREPARED);
    return clazz;
}


/*
 * Resolve classes reference by this class.
 */
void
java_lang_ClassLoader_resolveClass0(struct Hjava_lang_ClassLoader* this, struct Hjava_lang_Class* class)
{
	processClass(class, CSTATE_LINKED);
}

/*
 * Load a system class.
 */
struct Hjava_lang_Class*
java_lang_ClassLoader_findSystemClass0(struct Hjava_lang_ClassLoader* this, struct Hjava_lang_String* str)
{
	Hjava_lang_Class* class;
	int len = javaStringLength(str);
	Utf8Const* c;
	char* name;
#if INTERN_UTF8CONSTS
	char buffer[100];
	if (len <= 100) {
		name = buffer;
	}
	else {
		name = malloc (len);
	}
#else
	c = malloc(sizeof(Utf8Const) + len + 1);
	name = c->data;
#endif
        javaString2CString(str, name, len+1);
	classname2pathname (name, name);
#if INTERN_UTF8CONSTS
	c = makeUtf8Const (name, len);
	if (name != buffer) {
		free(name);
	}
#else /* ! INTERN_UTF8CONSTS */
	c->length = len;
	c->hash = (uint16) hashUtf8String (name, len);
#endif /* ! INTERN_UTF8CONSTS */

	EH_NATIVE_DURING
		class = loadClass(c, 0);
	EH_NATIVE_HANDLER
		/* Failed to load class ... */
		throwException(ClassNotFoundException(name));
	EH_NATIVE_ENDHANDLER

	return class;
}

/*
 * Locate the requested resource in the current Jar files and create a
 *  stream to the data.
 */
struct Hjava_io_InputStream*
java_lang_ClassLoader_getSystemResourceAsStream0(struct Hjava_lang_String* str)
{
	char* name;
	struct Hjava_io_InputStream* in;
	classFile hand;
	HArrayOfByte* data;

	in = NULL;
	name = makeCString(str);
	hand = findInJar(name);
	free(name);
	if (hand.type == 0) {
		return (NULL);
	}

	/* Copy data from returned buffer into Java byte array.  Be nice
	 * to avoid this copy but we cannot for the moment.
	 */
	data = (HArrayOfByte*)AllocArray(hand.size, TYPE_Byte);
	memcpy(unhand(data)->body, hand.buf, hand.size);
	if (hand.base != NULL) {
		free(hand.base);
	}

	/* Create input stream using byte array */
	in = (struct Hjava_io_InputStream*)execute_java_constructor(NULL, "java.io.ByteArrayInputStream", NULL, "([B)V", data);
	return (in);
}

/*
 * ???
 */
struct Hjava_lang_String*
java_lang_ClassLoader_getSystemResourceAsName0(struct Hjava_lang_String* str)
{
	return (NULL);
}
