/*
 * errors.h
 * Error return codes.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 */

#ifndef __errors_h
#define __errors_h

#include "classMethod.h"
#include "support.h"

#define NEW_LANG_EXCEPTION(NAME) \
  execute_java_constructor(0, "java.lang." #NAME, 0, "()V")

#define NEW_LANG_EXCEPTION_MESSAGE(NAME, MESS) \
  execute_java_constructor(0, "java.lang." #NAME, 0, "(Ljava/lang/String;)V", \
	makeJavaString(MESS, strlen(MESS)))

#define NEW_IO_EXCEPTION(NAME) \
  execute_java_constructor(0, "java.io." #NAME, 0, "()V")

#define NEW_IO_EXCEPTION_MESSAGE(NAME, MESS) \
  execute_java_constructor(0, "java.io." #NAME, 0, "(Ljava/lang/String;)V", \
	makeJavaString(MESS, strlen(MESS)))

#define ExceptionInInitializerError(E) \
	execute_java_constructor(0, "java.lang.ExceptionInInitializerError", \
	                         0, "(Ljava/lang/Throwable;)V", (E))

#define NoClassDefFoundError(M) NEW_LANG_EXCEPTION_MESSAGE(NoClassDefFoundError, M)
#define NoSuchMethodError NEW_LANG_EXCEPTION(NoSuchMethodError)
#define ClassFormatError NEW_LANG_EXCEPTION(ClassFormatError)
#define LinkageError NEW_LANG_EXCEPTION(LinkageError)
#define ClassNotFoundException(M) NEW_LANG_EXCEPTION_MESSAGE(ClassNotFoundException, M)
#define NoSuchFieldError NEW_LANG_EXCEPTION(NoSuchFieldError)
#define UnsatisfiedLinkError NEW_LANG_EXCEPTION(UnsatisfiedLinkError)
#define VirtualMachineError NEW_LANG_EXCEPTION(VirtualMachineError)
#define ClassCircularityError NEW_LANG_EXCEPTION(ClassCircularityError)
#define IncompatibleClassChangeError NEW_LANG_EXCEPTION(IncompatibleClassChangeError)
#define IllegalAccessError NEW_LANG_EXCEPTION(IllegalAccessError)
#define NegativeArraySizeException NEW_LANG_EXCEPTION(NegativeArraySizeException)
#define ClassCastException NEW_LANG_EXCEPTION(ClassCastException)
#define IllegalMonitorStateException NEW_LANG_EXCEPTION(IllegalMonitorStateException)
#define NullPointerException NEW_LANG_EXCEPTION(NullPointerException)
#define ArrayIndexOutOfBoundsException NEW_LANG_EXCEPTION(ArrayIndexOutOfBoundsException)
#define ArrayStoreException NEW_LANG_EXCEPTION(ArrayStoreException)
//#define ArithmeticException NEW_LANG_EXCEPTION(ArithmeticException)
#define ArithmeticException NEW_LANG_EXCEPTION_MESSAGE(ArithmeticException,"/ by zero" )
#define AbstractMethodError NEW_LANG_EXCEPTION(AbstractMethodError)
#define VerifyError NEW_LANG_EXCEPTION(VerifyError)
#define ThreadDeath NEW_LANG_EXCEPTION(ThreadDeath)
#define	InstantiationException(M) NEW_LANG_EXCEPTION_MESSAGE(InstantiationException, M)
#define	IOException(M) NEW_IO_EXCEPTION_MESSAGE(IOException, M)

/* Since we might not have enough memory to allocate the exception
   when we really need the exception, preallocate OutOfMemoryError in
   initialiseKaffe(). */
extern Hjava_lang_Object *OutOfMemoryError;

extern Hjava_lang_Class *ErrorClass;

#endif
