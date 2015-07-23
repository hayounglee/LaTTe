/*
 * baseClasses.h
 * Handle base classes.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 * Modified by MASS Laboratory, Seoul National University, 1999.
 */

#ifndef __baseclasses_h
#define __baseclasses_h

#include "object.h"
#include "native.h"
#include "java_lang_String.h"
#include "java_lang_Throwable.h"

#define STRING_SIZE(STR) \
		javaStringLength(STR)
#define STRING_DATA(STR) \
		&(unhand(unhand(STR)->value)->body[unhand(STR)->offset])

#define	STRINGCLASS	"java/lang/String"
#define	STRINGINIT	"<init>"
#define	STRINGINITSIG	"([C)V"

#define	OBJECTCLASS	"java/lang/Object"
#define	CLASSCLASS	"java/lang/Class"

#define	OBJECTCLASSSIG	"Ljava/lang/Object;"

void initBaseClasses(void);

extern struct Hjava_lang_Class*	ClassClass;
extern struct Hjava_lang_Class*	ObjectClass;
extern struct Hjava_lang_Class*	StringClass;
extern struct Hjava_lang_Class*	SystemClass;
extern struct Hjava_lang_Class*	SerialClass;
extern struct Hjava_lang_Class*	ErrorClass;

#endif
