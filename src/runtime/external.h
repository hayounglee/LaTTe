/*
 * external.h
 * Handle method calls to other languages.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 * Modified by MASS Laboratory, SNU, 1999.
 */

#ifndef __external_h
#define __external_h

#define	MAXSTUBLEN	1024
#define	MAXLIBPATH	1024
#define	MAXLIBS		16

/* Default library info. */
#if !defined(LIBRARYPATH)
#define	LIBRARYPATH	"LD_LIBRARY_PATH"
#endif

#define NATIVELIBRARY   "lib" VMNAME "_core"

/*
 * Provide defaults if these aren't defined.
 */
#if !defined(CALL_KAFFE_LMETHOD_VARARGS)
#define CALL_KAFFE_LMETHOD_VARARGS(A,B,C,D,E)				\
		CALL_KAFFE_METHOD_VARARGS(A,B,C,D,E)
#endif
#if !defined(CALL_KAFFE_FMETHOD_VARARGS)
#define CALL_KAFFE_FMETHOD_VARARGS(A,B,C,D,E)				\
		CALL_KAFFE_METHOD_VARARGS(A,B,C,D,E)
#endif
#if !defined(CALL_KAFFE_DMETHOD_VARARGS)
#define CALL_KAFFE_DMETHOD_VARARGS(A,B,C,D,E)				\
		CALL_KAFFE_METHOD_VARARGS(A,B,C,D,E)
#endif
#if !defined(CALL_KAFFE_LSTATIC_VARARGS)
#define CALL_KAFFE_LSTATIC_VARARGS(A,B,C,D)				\
		CALL_KAFFE_STATIC_VARARGS(A,B,C,D)
#endif
#if !defined(CALL_KAFFE_FSTATIC_VARARGS)
#define CALL_KAFFE_FSTATIC_VARARGS(A,B,C,D)				\
		CALL_KAFFE_STATIC_VARARGS(A,B,C,D)
#endif
#if !defined(CALL_KAFFE_DSTATIC_VARARGS)
#define CALL_KAFFE_DSTATIC_VARARGS(A,B,C,D)				\
		CALL_KAFFE_STATIC_VARARGS(A,B,C,D)
#endif

struct _methods;

void	initNative(void);
int	loadNativeLibrary(char*);
void*	loadNativeLibrarySym(char*);
void	native(struct _methods*);
void	addNativeFunc(char*, void*);

#endif
