/*
 * java.lang.String.c
 *
 * Copyright (c) 1996,97 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "lib-license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.co.uk>
 */

#include <native.h>

extern Hjava_lang_String* internJavaString(Hjava_lang_String*);

Hjava_lang_String*
java_lang_String_intern(Hjava_lang_String* str)
{
	return (internJavaString(str));
}

