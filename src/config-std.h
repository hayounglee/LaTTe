/*
 * config-std.h
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>
 */

#ifndef __config_std_h
#define __config_std_h

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#endif
#if defined(HAVE_SYS_TYPES_H)
#include <sys/types.h>
#endif
#if defined(HAVE_WINNT_H)
#include <winnt.h>
#endif
#if defined(HAVE_WINTYPES_H)
#include <wintypes.h>
#endif
#if defined(HAVE_WTYPES_H)
#include <wtypes.h>
#endif
#if defined(HAVE_WINBASE_H)
#include <winbase.h>
#endif
#if defined(HAVE_BSD_LIBC_H)
#include <bsd/libc.h>
#endif

#undef	__NORETURN__
#if defined(__GNUC__)
#define	__NORETURN__ __attribute__((noreturn))
#else
#define	__NORETURN__
#endif

#endif
