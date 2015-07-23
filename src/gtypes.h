/*
 * gtypes.h
 * General types.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 */

#ifndef __gtype_h
#define __gtype_h

#include "config.h"
#include "jtypes.h"

#if defined(__STDC__)
typedef signed char		int8;
#elif defined(__CHAR_UNSIGNED__)
#error "no signed char type"
#else
typedef	char			int8;
#endif

typedef	unsigned char		uint8;

#if SIZEOF_SHORT == 2
typedef	short			int16;
typedef	unsigned short		uint16;
#else
#error "sizeof(short) must be 2"
#endif

#if SIZEOF_INT == 4
typedef	int			int32;
typedef	unsigned int		uint32;
#elif SIZEOF_LONG == 4
typedef	long			int32;
typedef	unsigned long		uint32;
#else
#error "sizeof(int) or sizeof(long) must be 4"
#endif

#if SIZEOF_LONG == 8
typedef	long			int64;
typedef	unsigned long		uint64;
#elif SIZEOF_LONG_LONG == 8
typedef	long long		int64;
typedef	unsigned long long	uint64;
#elif SIZEOF___INT64 == 8
typedef	__int64			int64;
typedef	unsigned __int64	uint64;
#else
#error "sizeof(long) or sizeof(long long) or sizeof(__int64) must be 8"
#endif

#if SIZEOF_VOIDP == 4
typedef uint32			uintp;
#elif SIZEOF_VOIDP == 8
typedef uint64			uintp;
#else
#error "sizeof(void*) must be 4 or 8"
#endif

typedef enum _bool {
	false	= 0,
	true	= 1
} bool;

typedef uint8			u1;
typedef uint16			u2;
typedef uint32			u4;

typedef	u1		nativecode;
typedef u2		accessFlags;
typedef u2		constIndex;

typedef uint8           byte;
typedef uint32          word;
typedef int16           ShortOffset;
typedef int32           InstrOffset;
typedef uint16          ConstIndex;


typedef struct _strconst Utf8Const;
/* typedef struct Hjava_lang_Class Class; */
typedef struct _methods Method;
typedef struct Array Array;
typedef struct _fields Field;

#define PTR_TYPE_SIZE	SIZEOF_VOIDP

struct _constants;
struct _methodTable;
struct _dispatchTable;
struct _jexception;

/* If INTERN_UTF8CONSTS is 1, then all Utf8Const objects are interned in
 * a hashtable, thus you can compare them with ==.
 */
//#define INTERN_UTF8CONSTS	0
#define INTERN_UTF8CONSTS	1

struct _strconst {
#if !INTERN_UTF8CONSTS
	/* If we're not interning, store hash for fast comparisons. */
	uint16			hash;
#endif
	uint16			length;
	char			data[1]; /* In Utf8 format, with final '\0'. */
};

#define	SHIFT_jchar		1
#define	SHIFT_jbyte		0
#define	SHIFT_jshort		1
#define	SHIFT_jint		2
#define	SHIFT_jlong		3
#define	SHIFT_jfloat		2
#define	SHIFT_jdouble		3

#if SIZEOF_VOIDP == 4
#define	SHIFT_jref		2
#elif SIZEOF_VOIDP == 8
#define	SHIFT_jref		3
#endif

#define	EXIT(X)	exitInternal(X)
#define	ABORT()	abort()

extern void exitInternal (int status);

#endif
