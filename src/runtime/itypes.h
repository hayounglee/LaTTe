/*
 * itypes.h
 * Internal types.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 * Modified by MASS Laboratory, Seoul National University, 1999.
 */

#ifndef __itypes_h
#define __itypes_h

#include "gtypes.h"

#define	TYPE_Boolean	4
#define	TYPE_Char	5
#define	TYPE_Float	6
#define	TYPE_Double	7
#define	TYPE_Byte	8
#define	TYPE_Short	9
#define	TYPE_Int	10
#define	TYPE_Long	11
#define	TYPE_Ref	15
#define	TYPE_Bad	17

#define	MAXTYPES	16

extern struct Hjava_lang_Class* types[MAXTYPES];

extern struct Hjava_lang_Class* intClass;
extern struct Hjava_lang_Class* longClass;
extern struct Hjava_lang_Class* booleanClass;
extern struct Hjava_lang_Class* charClass;
extern struct Hjava_lang_Class* floatClass; 
extern struct Hjava_lang_Class* doubleClass;
extern struct Hjava_lang_Class* byteClass; 
extern struct Hjava_lang_Class* shortClass;     
extern struct Hjava_lang_Class* voidClass;

#define	TYPE_CLASS(t)		types[t]

void finishTypes(void);

#endif
