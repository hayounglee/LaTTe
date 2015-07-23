/*
 * soft.c
 * Soft instruction support.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 * Modified by MASS Laboratory, SNU, 1999.
 */

#define	MDBG(s)
#define	ADBG(s)
#define	CDBG(s)
#define	IDBG(s)

#if MDBG(1) - 1 == 0
#undef CDBG
#define	CDBG(s) s
#endif

#include "config.h"
#include "config-std.h"
#include "config-math.h"
#include "config-mem.h"
#include <stdarg.h>
#include "jtypes.h"
#include "bytecode.h"
#include "access.h"
#include "object.h"
#include "constants.h"
#include "classMethod.h"
#include "baseClasses.h"
#include "lookup.h"
#include "errors.h"
#include "exception.h"
#include "locks.h"
#include "soft.h"
#include "external.h"
#include "thread.h"
#include "baseClasses.h"
#include "flags.h"
#include "itypes.h"

#ifdef TRANSLATOR

#include "method_inlining.h"
#include "exception_info.h"
#include "translate.h"

#endif /* TRANSLATOR */

/*
 * soft_new
 */
void*
soft_new(Hjava_lang_Class* c)
{
	Hjava_lang_Object* obj;

	processClass(c, CSTATE_OK);
	obj = newObject(c);

ADBG(	printf("New object of type %s (%d,%x)\n", c->name->data, c->fsize, obj);
		fflush(stdout);						)

	return (obj);
}

/*
 * soft_newarray
 */
void*
soft_newarray(jint type, jint size)
{
	Hjava_lang_Object* obj;

	if (size < 0) {
		throwException(NegativeArraySizeException);
	}

	obj = newPrimArray(TYPE_CLASS(type), size);

ADBG(	printf("New object of %d type (%d,%x)\n", type, size, obj);
	fflush(stdout);							)

	return (obj);
}

/*
 * soft_anewarray
 */
void*
soft_anewarray(Hjava_lang_Class* elclass, jint size)
{
	Hjava_lang_Object* obj;

	if (size < 0) {
		throwException(NegativeArraySizeException);
	}

	obj = newRefArray(elclass, size);

ADBG(	printf("New array object of %s type (%d,%x)\n", elclass->name->data, size, obj); fflush(stdout);							)

	return (obj);
}

/*
 * soft_multianewarray.
 */
#define	MAXDIMS	16

#if defined(TRANSLATOR)
void*
soft_multianewarray(Hjava_lang_Class* class, jint dims, ...)
{
	va_list ap;
	int array[MAXDIMS];
	int i;
	jint arg;
	Hjava_lang_Object* obj;
	int* arraydims;

        if (dims < MAXDIMS) {
		arraydims = array;
	}
	else {
		arraydims = gc_calloc_fixed(dims+1, sizeof(int));
	}

	/* Extract the dimensions into an array */
	va_start(ap, dims);
	for (i = 0; i < dims; i++) {
		arg = va_arg(ap, jint);
		if (arg < 0) {
                        throwException(NegativeArraySizeException);
		}
		arraydims[i] = arg;
	}
//	arraydims[i] = 0;
	arraydims[i] = -1;
	va_end(ap);

	/* Mmm, okay now build the array using the wonders of recursion */
        obj = newMultiArray(class, arraydims);

	if (arraydims != array) {
		gc_free_fixed(arraydims);
	}

	/* Return the base object */
	return (obj);
}
#endif

/*
 * soft_monitorenter.
 */
void
soft_monitorenter(Hjava_lang_Object* o)
{
	lockMutex(o);
}

/*
 * soft_monitorexit.
 */
void
soft_monitorexit(Hjava_lang_Object* o)
{
	unlockMutex(o);
}

/*
 * soft_lookupmethod.
 */
void*
soft_lookupmethod(Hjava_lang_Object* obj, Method* meth)
{
    Hjava_lang_Class* cls = OBJECT_CLASS (obj);

    meth = findMethod(cls, meth->name, meth->signature);
    if (meth == 0) {
        throwException(NoSuchMethodError);
    }

#if defined(TRANSLATOR) && defined(HAVE_TRAMPOLINE)
    return (METHOD_NATIVECODE(meth));
#else
    return (meth);
#endif
}

/*
 * soft_lookupinterfacemethod
 * Lookup an interface method, given its name and signature.  Throw exceptions
 * if the method found doesn't fit with wht invokeinterface is expecting.
 */
void*
soft_lookupinterfacemethod(Hjava_lang_Class* class, Utf8Const* name, Utf8Const* signature)
{
	processClass(class, CSTATE_LINKED);
	for (; class != 0; class = class->superclass) {
		Method* mptr = findMethodLocal(class, name, signature);
		if (mptr != NULL) {
			if (mptr->accflags & ACC_STATIC) {
				throwException(IncompatibleClassChangeError);
			}
			if (mptr->accflags & ACC_ABSTRACT) {
				throwException(AbstractMethodError);
			}
			if (!(mptr->accflags & ACC_PUBLIC)) {
				throwException(IllegalAccessError);
			}
#if defined(TRANSLATOR) && defined(HAVE_TRAMPOLINE)
			return (METHOD_NATIVECODE(mptr));
#else
			return (mptr);
#endif
		}
	}
	throwException(IncompatibleClassChangeError);
}

#if defined(TRANSLATOR)
nativecode*
soft_get_method_code (Method* meth)
{
    /* If this class needs initialising then do it now */
    if (meth->class->state != CSTATE_OK) {
        processClass(meth->class, CSTATE_OK);
    }

    /*
     * Generate code on demand.
     */
    if (!METHOD_TRANSLATED(meth)) {
        TranslationInfo *info
            = (TranslationInfo *) alloca( sizeof(TranslationInfo *) );
        
        memset( info, 0, sizeof(TranslationInfo) );

        TI_SetRootMethod( info, meth );
        translate( info );
        METHOD_NATIVECODE(meth) = TI_GetNativeCode( info );
    }

CDBG(	printf("Calling %s:%s%s @ %p\n",
		meth->class->name->data, meth->name->data,
		meth->signature->data, METHOD_NATIVECODE(meth));
		fflush(stdout);						)

    return (METHOD_NATIVECODE(meth));
}
#endif /* TRANSLATOR */

/*
 * soft_checkcast.
 */
void*
soft_checkcast(Hjava_lang_Class* c, Hjava_lang_Object* o)
{
	if (o != NULL && ! soft_instanceof(c, o)) {
		throwException(ClassCastException);
	}
	return (o);
}

/*
 * soft_instanceof.
 */
jint
soft_instanceof(Hjava_lang_Class* c, Hjava_lang_Object* o)
{
	/* Null object are never instances of anything */
	if (o == 0) {
		return (0);
	}

	return (instanceof(c, Object_GetClass(o)));
}

jint
instanceof(Hjava_lang_Class* c, Hjava_lang_Class* oc)
{
	/* We seperate the casting to objects and the casting to arrays. */
	if (Class_IsArrayType(c)) {
		return (instanceof_array(c, oc));
	}
	else {
		return (instanceof_class(c, oc));
	}
}

jint
instanceof_class(Hjava_lang_Class* c, Hjava_lang_Class* oc)
{
	int i;

	if (oc == c) {
		return (1);
	}

	if (oc == 0) {
		return (0);
	}

	if (instanceof_class(c, oc->superclass)) {
		return (1);
	}

        for (i = 0; i < oc->interface_len; i++) {
		Hjava_lang_Class* ic = oc->interfaces[i];
		/* Interface's may not be prepared it they are preloaded.
		 * We added the if ... check in to speed the general case.
		 */
		if (ic->state < CSTATE_PREPARED) {
			processClass(ic, CSTATE_PREPARED);
		}
                if (instanceof_class(c, ic)) {
                        return (1);
                }
        }

        return (0);
}

jint
instanceof_array(Hjava_lang_Class* c, Hjava_lang_Class* oc)
{
	/* Skip as many arrays of arrays as we can.  We stop when we find
	 * a base class in either. */
	while (Class_IsArrayType(c) && Class_IsArrayType(oc)) {
		c = Class_GetComponentType(c);
		oc = Class_GetComponentType(oc);
	}

	/* If we are still casting to an array then we have failed already */
	if (Class_IsArrayType(c))
		return (0);

	/* If a base type, they must match exact. */
	if (Class_IsPrimitiveType(c)) {
		return (c == oc);
	}

	/* Casting to an object of some description. */
	if (Class_IsArrayType(oc)) {
		/* The only thing we can cast an array to is java/lang/Object.
		 * Checking this here willl save time.
		 */
		return (c == ObjectClass);
	}

	/* Cannot cast to a primitive class. */
	if (Class_IsPrimitiveType(oc))
		return (0);

	/* Casting one object to another.  */
	return (instanceof_class(c, oc));
}

/*
 * soft_athrow.
 */
void
soft_athrow(Hjava_lang_Object* o)
{
	throwExternalException(o);
}

/*
 * soft_badarrayindex.
 */
void
soft_badarrayindex(void)
{
	throwException(ArrayIndexOutOfBoundsException);
}

/*
 * soft_nullpointer.
 */
void
soft_nullpointer(void)
{
	throwException(NullPointerException);
}

/*
 * soft_initialise_class.
 */
void
soft_initialise_class(Hjava_lang_Class* c)
{
	if (c->state != CSTATE_OK) {
		processClass(c, CSTATE_OK);
	}
}

/*
 * Check we can store 'obj' into the 'array'.
 */
void
soft_checkarraystore(Hjava_lang_Object* array, Hjava_lang_Object* obj)
{
	if (obj != 0 
            && soft_instanceof(Class_GetComponentType(Object_GetClass(array)), obj) == 0) {
		throwException(ArrayStoreException);
	}
}

/*
 * soft_dcmpg
 */
jint
soft_dcmpg(jdouble v1, jdouble v2)
{
	jint ret;
	if ((!isinf(v1) && isnan(v1)) || (!isinf(v2) && isnan(v2))) {
		ret = 1;
	}
	else if (v1 > v2) {
		ret = 1;
	}
	else if (v1 == v2) {
		ret = 0;
	}
	else {
		ret = -1;
	}

	return (ret);
}

/*
 * soft_dcmpl
 */
jint
soft_dcmpl(jdouble v1, jdouble v2)
{
        jint ret;
	if ((!isinf(v1) && isnan(v1)) || (!isinf(v2) && isnan(v2))) {
		ret = -1;
	}
        else if (v1 > v2) {
                ret = 1;
        }
        else if (v1 == v2) {
                ret = 0;
        }
        else {
                ret = -1;
        }
	return (ret);
}

/*
 * soft_fcmpg
 */
jint
soft_fcmpg(jfloat v1, jfloat v2)
{
        jint ret;
	if ((!isinf(v1) && isnan(v1)) || (!isinf(v2) && isnan(v2))) {
		ret = 1;
	}
        else if (v1 > v2) {
                ret = 1;
        }
        else if (v1 == v2) {
                ret = 0;
        }
        else {
                ret = -1;
        }
	return (ret);
}

/*
 * soft_fcmpg
 */
jint
soft_fcmpl(jfloat v1, jfloat v2)
{
        jint ret;
	if ((!isinf(v1) && isnan(v1)) || (!isinf(v2) && isnan(v2))) {
		ret = -1;
	}
        else if (v1 > v2) {
                ret = 1;
        }
        else if (v1 == v2) {
                ret = 0;
        }
        else {
                ret = -1;
        }
	return (ret);
}

jlong
soft_lmul(jlong v1, jlong v2)
{
	return (v1 * v2);
}

jlong
soft_ldiv(jlong v1, jlong v2)
{
    if (v2 == 0) throwException(ArithmeticException);
    return (v1 / v2);
}

jlong
soft_lrem(jlong v1, jlong v2)
{
    if (v2 == 0) throwException(ArithmeticException);
    return (v1 % v2);
}

jfloat
soft_fdiv(jfloat v1, jfloat v2)
{
	jfloat k = 1e300;

	if (v2 == 0.0) {
		if (v1 > 0) {
			return (k*k);
		}
		else {
			return (-k*k);
		}
	}
	return (v1 / v2);
}

jdouble
soft_fdivl(jdouble v1, jdouble v2)
{
	jdouble k = 1e300;

	if (v2 == 0.0) {
		if (v1 > 0) {
			return (k*k);
		}
		else {
			return (-k*k);
		}
	}
	return (v1 / v2);
}

jfloat
soft_frem(jfloat v1, jfloat v2)
{
	return (fmod(v1, v2));
}

jdouble
soft_freml(jdouble v1, jdouble v2)
{
	return (fmod(v1, v2));
}

jlong
soft_lshll(jlong v1, jint v2)
{
	return (v1 << (v2 & 63));
}

jlong
soft_ashrl(jlong v1, jint v2)
{
	return (v1 >> (v2 & 63));
}

jlong
soft_lshrl(jlong v1, jint v2)
{
	return (((uint64)v1) >> (v2 & 63));
}

jint
soft_lcmp(jlong v1, jlong v2)
{
	jlong lcc = v2 - v1;
	if (lcc < 0) {
		return (-1);
	}
	else if (lcc > 0) {
		return (1);
	}
	else {
		return (0);
	}
}

jint
soft_mul(jint v1, jint v2)
{
	return (v1*v2);
}

jint
soft_div(jint v1, jint v2)
{
	return (v1/v2);
}

jint
soft_rem(jint v1, jint v2)
{
	return (v1%v2);
}

jfloat
soft_cvtlf(jlong v)
{
	return ((jfloat)v);
}

jdouble
soft_cvtld(jlong v)
{
	return ((jdouble)v);
}

/*
 * The following functions round the float/double to an int/long.
 * They round the value toward zero.
 */

jlong
soft_cvtfl(jfloat v)
{
	if (v < 0.0) {
		return ((jlong)ceil(v));
	}
	else {
		return ((jlong)floor(v));
	}
}

jlong
soft_cvtdl(jdouble v)
{
	if (v < 0.0) {
		return ((jlong)ceil(v));
	}
	else {
		return ((jlong)floor(v));
	}
}

jint
soft_cvtfi(jfloat v)
{
	if (v < 0.0) {
		return ((jint)ceil(v));
	}
	else {
		return ((jint)floor(v));
	}
}

jint
soft_cvtdi(jdouble v)
{
	if (v < 0.0) {
		return ((jint)ceil(v));
	}
	else {
		return ((jint)floor(v));
	}
}
