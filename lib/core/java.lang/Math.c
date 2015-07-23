/*
 * java.lang.Math.c
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "lib-license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 */

#include "config.h"
#include "config-std.h"
#include <native.h>
#include <math.h>
#include "config-math.h"
#include "java.lang.stubs/Math.h"

double
java_lang_Math_sin(double v)
{
	return (sin(v));
}

double
java_lang_Math_cos(double v)
{
	return (cos(v));
}

double
java_lang_Math_tan(double v)
{
	return (tan(v));
}

double
java_lang_Math_asin(double v)
{
	return (asin(v));
}

double
java_lang_Math_acos(double v)
{
	return (acos(v));
}

double
java_lang_Math_atan(double v)
{
	return (atan(v));
}

double
java_lang_Math_exp(double v)
{
	return (exp(v));
}

double
java_lang_Math_log(double v)
{
	return (log(v));
}

double
java_lang_Math_sqrt(double v)
{
	return (sqrt(v));
}

double
java_lang_Math_IEEEremainder(double v1, double v2)
{
	return (remainder(v1, v2));
}

double
java_lang_Math_ceil(double v)
{
	return (ceil(v));
}

double
java_lang_Math_floor(double v)
{
	return (floor(v));
}

double
java_lang_Math_rint(double v)
{
	return (rint(v));
}

double
java_lang_Math_atan2(double v1, double v2)
{
	return (atan2(v1, v2));
}

double
java_lang_Math_pow(double v1, double v2)
{
	return (pow(v1, v2));
}
