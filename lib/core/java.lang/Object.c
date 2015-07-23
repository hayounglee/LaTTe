/*
 * java.lang.Object.c
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
#include "runtime/locks.h"
#include "runtime/soft.h"
#include "runtime/object.h"
#include <native.h>
#include "runtime/constants.h"
#include "runtime/classMethod.h"	/* Don't need java.lang.Object.h */

/*
 * Generate object hash code.
 */
jint
java_lang_Object_hashCode(struct Hjava_lang_Object* o)
{
	/* Hash code is object's address */
	return ((jint)(jword)o);
}

/*
 * Return class object for this object.
 */
struct Hjava_lang_Class*
java_lang_Object_getClass(struct Hjava_lang_Object* o)
{
	return (OBJECT_CLASS(o));
}

/*
 * Notify threads waiting here.
 */
void
java_lang_Object_notifyAll(struct Hjava_lang_Object* o)
{
	broadcastCond(o);
}

/*
 * Notify a thread waiting here.
 */
void
java_lang_Object_notify(struct Hjava_lang_Object* o)
{
	signalCond(o);
}

/*
 * Clone me.
 */
struct Hjava_lang_Object*
java_lang_Object_clone(struct Hjava_lang_Object* o)
{
	Hjava_lang_Object* obj;
	Hjava_lang_Class* class;
	static Hjava_lang_Class* cloneClass;

	/* * Lookup cloneable class interface if we don't have it  */
	if (cloneClass == 0) {
		cloneClass = lookupClass("java/lang/Cloneable");
	}

	class = OBJECT_CLASS(o);

	if (!Class_IsArrayType(class)) {
		/* Check class is cloneable and throw exception if it isn't */
		if (soft_instanceof(cloneClass, o) == 0) {
			SignalError(0, "java.lang.CloneNotSupportedException", class->name->data);
		}
		/* Clone an object */
		obj = newObject(class);
		memcpy(OBJECT_DATA(obj), OBJECT_DATA(o), CLASS_FSIZE(class) - sizeof(Hjava_lang_Object));
	}
	else {
		/* Clone an array */
		obj = newArray(Class_GetComponentType(class), ARRAY_SIZE(o));
		memcpy(ARRAY_DATA(obj), ARRAY_DATA(o),
		       ARRAY_SIZE(o) * TYPE_SIZE(Class_GetComponentType(class)));
	}
	return (obj);
}

/*
 * Wait for this object to be notified.
 */
void
java_lang_Object_wait(struct Hjava_lang_Object* o, jlong timeout)
{
	waitCond(o, timeout);
}
