/*
 * support.c
 * Native language support (excluding string routines).
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 * Modified by MASS Laboratory, SNU, 1999.
 */

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include <stdarg.h>
#include "jtypes.h"
#include "access.h"
#include "object.h"
#include "constants.h"
#include "baseClasses.h"
#include "classMethod.h"
#include "lookup.h"
#include "errors.h"
#include "exception.h"
#include "support.h"
#include "md.h"
#include "itypes.h"
#include "external.h"

#if !defined(HAVE_GETTIMEOFDAY)
#include <sys/timeb.h>
#endif

#define	MAXEXCEPTIONLEN		200
#define	ERROR_SIGNATURE		"(Ljava/lang/String;)V"

/* Anchor point for user defined properties */
userProperty* userProperties;

#if defined(NO_SHARED_LIBRARIES)
/* Internal native functions */
static nativeFunction null_funcs[1];
nativeFunction* native_funcs = null_funcs;
#endif

/*
 * Call a Java method from native code.
 */
jword
do_execute_java_method(void* ee, Hjava_lang_Object* obj, char* method_name, char* signature, Method* mb, int isStaticCall, ...)
{
    char* sig;
    int args;
    va_list argptr;
    jword retval;
    Hjava_lang_Class* type = OBJECT_CLASS( obj );

    if ( mb == NULL ){
        mb = findMethod(type,
                        makeUtf8Const(method_name, -1),
                        makeUtf8Const(signature, -1));

    }

    /* No method or static - throw exception */
    if ( mb == NULL || (mb->accflags & ACC_STATIC) != 0) {
        throwException(NoSuchMethodError);
    }

    /* Calculate number of arguments */
    sig = signature;
    args = sizeofSig(&sig, false);

    /* Make the call */
    va_start(argptr, isStaticCall);
    CALL_KAFFE_METHOD_VARARGS(METHOD_NATIVECODE(mb), obj, args, argptr, retval);
    va_end(argptr);
    return (retval);
}

jlong
jlong_do_execute_java_method(void* ee, Hjava_lang_Object* obj, char* method_name, char* signature, Method* mb, int isStaticCall, ...)
{
    char* sig;
    int args;
    va_list argptr;
    jlong retval;
    Hjava_lang_Class* type = OBJECT_CLASS( obj );

    if ( mb == NULL ){
        mb = findMethod(type,
                        makeUtf8Const(method_name, -1),
                        makeUtf8Const(signature, -1));

    }

    /* No method or static - throw exception */
    if ( mb == NULL || (mb->accflags & ACC_STATIC) != 0) {
        throwException(NoSuchMethodError);
    }

    /* Calculate number of arguments */
    sig = signature;
    args = sizeofSig(&sig, false);

    /* Make the call */
    va_start(argptr, isStaticCall);
    CALL_KAFFE_LMETHOD_VARARGS(METHOD_NATIVECODE(mb), obj, args, argptr, retval);
    va_end(argptr);

    return (retval);
}

jfloat
jfloat_do_execute_java_method(void* ee, Hjava_lang_Object* obj, char* method_name, char* signature, Method* mb, int isStaticCall, ...)
{
    char* sig;
    int args;
    va_list argptr;
    jfloat retval;

    Hjava_lang_Class* type = OBJECT_CLASS( obj );

    if ( mb == NULL ){
        mb = findMethod(type,
                        makeUtf8Const(method_name, -1),
                        makeUtf8Const(signature, -1));

    }

    /* No method or static - throw exception */
    if ( mb == NULL || (mb->accflags & ACC_STATIC) != 0) {
        throwException(NoSuchMethodError);
    }

    /* Calculate number of arguments */
    sig = signature;
    args = sizeofSig(&sig, false);
    
    /* Make the call */
    va_start(argptr, isStaticCall);
    CALL_KAFFE_FMETHOD_VARARGS(METHOD_NATIVECODE(mb), obj, args, argptr, retval);
    va_end(argptr);

    return (retval);
}

jdouble
jdouble_do_execute_java_method(void* ee, Hjava_lang_Object* obj, char* method_name, char* signature, Method* mb, int isStaticCall, ...)
{
    char* sig;
    int args;
    va_list argptr;
    jdouble retval;

    Hjava_lang_Class* type = OBJECT_CLASS( obj );

    if ( mb == NULL ){
        mb = findMethod(type,
                        makeUtf8Const(method_name, -1),
                        makeUtf8Const(signature, -1));

    }

    /* No method or static - throw exception */
    if ( mb == NULL || (mb->accflags & ACC_STATIC) != 0) {
        throwException(NoSuchMethodError);
    }

    /* Calculate number of arguments */
    sig = signature;
    args = sizeofSig(&sig, false);

    /* Make the call */
    va_start(argptr, isStaticCall);
    CALL_KAFFE_DMETHOD_VARARGS(METHOD_NATIVECODE(mb), obj, args, argptr, retval);
    va_end(argptr);

    return (retval);
}

/*
 * Call a Java static method on a class from native code.
 */
jword
do_execute_java_class_method(char* cname, char* method_name, char* signature, ...)
{
    char* sig;
    int args;
    va_list argptr;
    Hjava_lang_Class* class;
    Method* mb;
    jword retval;
    char cnname[CLASSMAXSIG];	/* Unchecked buffer - FIXME! */

    /* Convert "." to "/" */
    classname2pathname(cname, cnname);

    class = lookupClass(cnname);
    assert(class != 0);

    mb = findMethod(class,
                    makeUtf8Const(method_name, -1),
                    makeUtf8Const(signature, -1));

    if (mb == 0) {
        throwException(NoSuchMethodError);
    }
    /* Method must be static to invoke it here */
    if ((mb->accflags & ACC_STATIC) == 0) {
        throwException(NoSuchMethodError);
    }

    /* Calculate number of arguments */
    sig = signature;
    args = sizeofSig(&sig, false);

    /* Make the call */
    va_start(argptr, signature);
    CALL_KAFFE_STATIC_VARARGS(METHOD_NATIVECODE(mb), args, argptr, retval);
    va_end(argptr);

    return (retval);
}

#define	MAIN		"main"
#define	MAINSIG		"([Ljava/lang/String;)V"
#define MAINARGSIZE     1

/* 
   Name        : do_execute_java_main_method
   Description : driver to executre main method
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:
   This function is almost similar to "do_execute_java_class_method".
   But it checks if the main method is "public and static".
 */
void
do_execute_java_main_method(char* cname, ...)
{
    va_list argptr;
    Hjava_lang_Class* class;
    Method* mb;
    jword retval;
    char cnname[CLASSMAXSIG];	/* Unchecked buffer - FIXME! */

    /* Convert "." to "/" */
    classname2pathname(cname, cnname);

    class = lookupClass(cnname);
    assert(class != 0);

    mb = findMethod(class,
                    makeUtf8Const(MAIN, -1),
                    makeUtf8Const(MAINSIG, -1));

    if (mb == 0) {
	fprintf(stderr, "In class %s: void main(String argv[]) is not defined\n", cname);
	exit(1);
    }
    /* Method must be static to invoke it here */
    if ((mb->accflags & ACC_STATIC) == 0 ||
	(mb->accflags & ACC_PUBLIC) == 0) {
	fprintf(stderr, "In class %s: main must be public and static\n", cname);
	exit(1);
    }

    /* Make the call */
    va_start(argptr, cname);
    CALL_KAFFE_STATIC_VARARGS(METHOD_NATIVECODE(mb), MAINARGSIZE, argptr, retval);
    va_end(argptr);
}

/*
 * Allocate an object and execute the constructor.
 */
Hjava_lang_Object*
execute_java_constructor(void* ee, char* cname, Hjava_lang_Class* cc, char* signature, ...)
{
    int args;
    Hjava_lang_Object* obj;
    char* sig;
    va_list argptr;
    Method* mb;
    char buf[MAXEXCEPTIONLEN];
    jword retval;

    if (cc == 0) {
        /* Convert "." to "/" */
        classname2pathname(cname, buf);

        cc = lookupClass (buf);
        assert(cc != 0);
    }

    /* We cannot construct interfaces or abstract classes */
    if ((cc->accflags & (ACC_INTERFACE|ACC_ABSTRACT)) != 0) {
        throwException(InstantiationException(cc->name->data));
    }

    if (cc->state != CSTATE_OK) {
        processClass(cc, CSTATE_OK);
    }

    mb = findMethod(cc,
                    constructor_name,
                    makeUtf8Const(signature, -1));


    /* No method  - throw exception */
    if ( mb == NULL ) {
        throwException(NoSuchMethodError);
    }

    obj = newObject(cc);
    assert(obj != 0);

    /* Calculate number of arguments */
    sig = signature;
    args = sizeofSig(&sig, false);

    /* Make the call */
    va_start(argptr, signature);
    CALL_KAFFE_METHOD_VARARGS(METHOD_NATIVECODE(mb), obj, args, argptr, retval);
    va_end(argptr);

    return (obj);
}

/*
 * Signal an error by creating the object and throwing the exception.
 */
void
SignalError(void* ee, char* cname, char* str)
{
    Hjava_lang_Object* obj;

    obj = execute_java_constructor(ee, cname, 0, 
                                   ERROR_SIGNATURE, 
                                   makeJavaString(str, strlen(str)));
    throwException(obj);
}

/*
 * Convert a class name to a path name.
 */
void
classname2pathname(char* from, char* to)
{
	int i;

	/* Convert any '.' in name to '/' */
	for (i = 0; from[i] != 0; i++) {
		if (from[i] == '.') {
			to[i] = '/';
		}
		else {
			to[i] = from[i];
		}
	}
	to[i] = 0;
}

/*
 * Return current time in milliseconds.
 */
jlong
currentTime(void)
{
	jlong tme;

#if defined(HAVE_GETTIMEOFDAY)
	struct timeval tm;
	gettimeofday(&tm, 0);
	tme = (((jlong)tm.tv_sec * (jlong)1000) + ((jlong)tm.tv_usec / (jlong)1000));
#elif defined(HAVE_FTIME)
	struct timeb tm;
	ftime(&tm);
	tme = (((jlong)tm.time * (jlong)1000) + (jlong)tm.millitm);
#else
	tme = 0;
#endif
	return (tme);
}

/*
 * Set a property to a value.
 */
void
setProperty(void* properties, char* key, char* value)
{
	Hjava_lang_String* jkey;
	Hjava_lang_String* jvalue;

	jkey = makeJavaString(key, strlen(key));
	jvalue = makeJavaString(value, strlen(value));

	do_execute_java_method(0, properties, "put",
		"(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;",
		0, false, jkey, jvalue);
}

/*
 * Allocate a new object of the given class name.
 */
Hjava_lang_Object*
AllocObject(char* classname)
{
	Hjava_lang_Class* class;

	class = lookupClass(classname);
	assert(class != 0);

	return (newObject(class));
}

/*
 * Allocate a new array of a given size and type.
 */
Hjava_lang_Object*
AllocArray(int len, int type)
{
	return (newPrimArray(TYPE_CLASS(type), len));
}

/*
 * Allocate a new array of the given size and class name.
 */
Hjava_lang_Object*
AllocObjectArray(int sz, char* classname)
{
	char* esig;

	if (sz < 0) {
		throwException(NegativeArraySizeException);
	}
        esig = classname;
        return (newRefArray(classFromSig(&esig, NULL), sz));

}

/*
 * Used to generate exception for unimplemented features.
 */
void
unimp(char* mess)
{
	SignalError(0,"java.lang.InternalError", mess);
}

/*
 * Print messages.
 */
void
kprintf(FILE* out, const char* mess, ...)
{
	va_list argptr;

	va_start(argptr, mess);
	vfprintf(out, mess, argptr);
	va_end(argptr);
}

/* Internal implementation of exit(). */
void
exitInternal (int status)
{
#if 1
	/* Forget about setting exit codes in finalizers. */
	gc_execute_finalizers();
	exit(status);
#else
	/* Horrible, horrible hacks.  Please don't understand this.
	   FIXME: There seems to be a race condition. */

	static volatile int exited = 0;
	static volatile int exit_code = 0;
	int do_exit;

	if (exited)
		do_exit = 0;
	else {
		exited = 1;
		do_exit = 1;
	}

	exit_code = status; /* Set exit code. */

	gc_execute_finalizers();

	if (do_exit)
		exit(exit_code);
#endif /* 0 */
}

#if defined(NO_SHARED_LIBRARIES)
/*
 * Register an user function statically linked in the binary.
 */
void
addNativeMethod(char* name, void* func)
{
	static int funcs_nr = 0;
	static int funcs_max = 0;

	/* If we run out of space, reallocate */
	if (funcs_nr + 1 >= funcs_max) {
		funcs_max += NATIVE_FUNC_INCREMENT;
		if (native_funcs != null_funcs) {
//			native_funcs = gc_realloc_fixed(native_funcs, funcs_max * sizeof(nativeFunction), (funcs_max-NATIVE_FUNC_INCREMENT) * sizeof(nativeFunction));
			native_funcs = gc_realloc_fixed(native_funcs, funcs_max * sizeof(nativeFunction));
		}
		else {
			native_funcs = gc_malloc_fixed(NATIVE_FUNC_INCREMENT * sizeof(nativeFunction));
		}
	}
	native_funcs[funcs_nr].name = gc_malloc_fixed(strlen(name) + 1);
	strcpy(native_funcs[funcs_nr].name, name);
	native_funcs[funcs_nr].func = func;
	funcs_nr++;
	native_funcs[funcs_nr].name = 0;
	native_funcs[funcs_nr].func = 0;
}
#endif
