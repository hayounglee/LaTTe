/*
 * object.c
 * Handle create and subsequent garbage collection of objects.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 * Modified by MASS Laboratory, SNU, 1999.
 */

#define	DBG(s)
#define	ADBG(s)

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "gtypes.h"
#include "access.h"
#include "object.h"
#include "constants.h"
#include "classMethod.h"
#include "lookup.h"
#include "itypes.h"
#include "baseClasses.h"
#include "errors.h"
#include "exception.h"
#include "itypes.h"
#include "md.h"
#include "external.h"
#include "gc.h"

extern Hjava_lang_Class* ThreadClass;

void throwOutOfMemory ();

/*
 * Create a new Java object.
 * If the object is a Class then we make it a root (for the moment).
 * Otherwise, if the object has a finalize method we look it up and store
 * it with the object for it's later use.  Otherwise the object is normal.
 */
Hjava_lang_Object*
newObject(Hjava_lang_Class* class)
{
	Hjava_lang_Object* obj;
	int sz;

	if (class == ClassClass) {
		sz = sizeof(Hjava_lang_Class);
		obj = gc_malloc( sz, &gc_class_object );
	} else {
		sz = CLASS_FSIZE(class);
		if (class->final == false) {
			sz = ROUNDUPALIGN(sz+GC_HEAD);
			if (sz < GC_MAX_SMALL_OBJECT_SIZE)
				obj = gc_malloc_small( sz );
			else
				obj = gc_malloc_large( sz );
		} else {
			obj = gc_malloc( sz, &gc_finalizing_object );
		}
	}

        /* Fill in object information */
	obj->dtable = class->dtable;

	return (obj);
}

/* Allocate a new array whose elements are ELCLASS (a primitive type),
   and that has COUNT elements. */
Hjava_lang_Object*
newPrimArray(Hjava_lang_Class* elclass, int count)
{
	int size;
	Hjava_lang_Class* class;
	Hjava_lang_Object* obj = NULL;

        if (count < 0)
		throwException(NegativeArraySizeException);

	/* Calculate object size */
	size = (count * TYPE_SIZE(elclass)) + sizeof(Array);
	class = lookupArray(elclass);

	size = ROUNDUPALIGN(size+GC_HEAD);
	if (size < 0)
		throwOutOfMemory(); /* Ha!  An overflow! */
	else if (size < GC_MAX_SMALL_OBJECT_SIZE)
		obj = gc_malloc_small( size );
	else
		obj = gc_malloc_large( size );

	obj->dtable = class->dtable;

	ARRAY_SIZE(obj) = count;

	return (obj);
}

/* Allocate a new array whose elements are ELCLASS (an Object type),
   and that has COUNT elements. */
Hjava_lang_Object*
newRefArray(Hjava_lang_Class* elclass, int count)
{
	int size;
	Hjava_lang_Class* class;
	Hjava_lang_Object* obj = NULL;

        if (count < 0)
		throwException(NegativeArraySizeException);

	size = (count * PTR_TYPE_SIZE) + sizeof(Array);
	class = lookupArray(elclass);

	size = ROUNDUPALIGN(size+GC_HEAD);
	if (size < 0)
		throwOutOfMemory(); /* Ha!  An overflow! */
	else if (size < GC_MAX_SMALL_OBJECT_SIZE)
		obj = gc_malloc_small( size );
	else
		obj = gc_malloc_large( size );

	obj->dtable = class->dtable;
        ((Array*)obj)->encoding = Class_GetEncoding(class);

	ARRAY_SIZE(obj) = count;

	return (obj);
}

/* Allocate a new array whose elements are ELCLASS (a primitive type),
   and that has COUNT elements. */
Hjava_lang_Object*
_newPrimArray(int count, int shift)
{
	int size;
	Hjava_lang_Object* obj = NULL;

        if (count < 0)
		throwException(NegativeArraySizeException);

	/* Calculate object size */
	size = (count << shift) + sizeof(Array);

	size = ROUNDUPALIGN(size+GC_HEAD);
	if (size < 0)
		throwOutOfMemory(); /* Ha!  An overflow! */
	else if (size < GC_MAX_SMALL_OBJECT_SIZE)
		obj = gc_malloc_small( size );
	else
		obj = gc_malloc_large( size );

	ARRAY_SIZE(obj) = count;

	return (obj);
}

Hjava_lang_Object*
_newRefArray( int count )
{
	int size;
	Hjava_lang_Object* obj = NULL;

        if (count < 0)
		throwException(NegativeArraySizeException);

	size = (count * PTR_TYPE_SIZE) + sizeof(Array);

	size = ROUNDUPALIGN(size+GC_HEAD);
	if (size < 0)
		throwOutOfMemory(); /* Ha!  An overflow! */
	else if (size < GC_MAX_SMALL_OBJECT_SIZE)
		obj = gc_malloc_small( size );
	else
		obj = gc_malloc_large( size );

	ARRAY_SIZE(obj) = count;

	return (obj);
}

Hjava_lang_Object*
newMultiArray(Hjava_lang_Class* clazz, int* dims)
{
	Hjava_lang_Object* obj;
	Hjava_lang_Object** array;
	int i;

	obj = newArray(Class_GetComponentType(clazz), dims[0]);
	if (dims[1] >= 0) {
		array = OBJARRAY_DATA(obj);
		for (i = 0; i < dims[0]; i++) {
			array[i] =
			    newMultiArray(Class_GetComponentType(clazz), &dims[1]);
		}
	}

	return (obj);
}

/*
 * Allocate a new array, of whatever types.
 */
Hjava_lang_Object*
newArray(Hjava_lang_Class* eltype, int count)
{
	if (Class_IsPrimitiveType(eltype)) {
		return (newPrimArray(eltype, count));
	} else {
		return (newRefArray(eltype, count));
	}
}

/*
 * Object requires finalising - find the method and call it.
 */
void
gc_finalize_object(void* ob)
{
    Method* final;
    
    final = findMethod(OBJECT_CLASS((Hjava_lang_Object*)ob),
		       final_name, void_signature);
    CALL_KAFFE_METHOD(METHOD_NATIVECODE(final), (Hjava_lang_Object*)ob);
}
