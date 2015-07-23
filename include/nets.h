/*
 * nets.h
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 */

#ifndef __nets_h
#define __nets_h

#include "config.h"
#include "config-mem.h"
#include <errno.h>

extern int errno;
extern int h_errno;

#define	MAXHOSTNAME	128

#if defined(HAVE_STRERROR)
#define	SYS_ERROR	strerror(errno)
#else
extern char* sys_errlist[];
#define	SYS_ERROR	sys_errlist[errno]
#endif

#if defined(HAVE_HSTRERROR)
#define	SYS_HERROR	hstrerror(h_errno)
#else
#define	SYS_HERROR	"Network error"
#endif

#endif
