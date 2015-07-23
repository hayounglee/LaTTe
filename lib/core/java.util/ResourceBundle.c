/*
 * java.util.ResourceBundle.c
 *
 * Copyright (c) 1996,97 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "lib-license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.co.uk>
 */

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "config-io.h"
#include <native.h>
#include "java.util.stubs/ResourceBundle.h"

/*
 * What the hell does this do anyway - all I know is that this
 * keeps it happy. FIXME
 */
HArrayOfObject*
java_util_ResourceBundle_getClassContext(struct Hjava_util_ResourceBundle* this)
{
	HArrayOfObject* array;

	array = (HArrayOfObject*)AllocObjectArray(3, "Ljava/lang/Class;");

	return (array);
}
