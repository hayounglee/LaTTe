/*
 * external_native.h
 * Wrap up the calls between Kaffe and native method calls for systems
 *  which don't support shared libraries.
 *
 * Copyright (c) 1997 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Modified by Tim Wilkinson <tim@tjwassoc.demon.co.uk>
 */

#ifndef __external_native_h
#define __external_native_h

#if defined(NO_SHARED_LIBRARIES)

#define	KAFFE_NATIVE_PROTOTYPE(_f)	extern void _f();
#define	KAFFE_NATIVE_METHOD(_n)		{ #_n, _n },

#define	KAFFE_NATIVE(_f)	KAFFE_NATIVE_PROTOTYPE(_f)

#include "external_wrappers.h"

#undef	KAFFE_NATIVE
#define	KAFFE_NATIVE(_f)	KAFFE_NATIVE_METHOD(_f)

nativeFunction default_natives[] = {

#include "external_wrappers.h"

	{ 0, 0 }
};

#undef	KAFFE_NATIVE

#endif

#endif
