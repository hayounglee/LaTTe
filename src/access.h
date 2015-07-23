/*
 * access.h
 * Access flags.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 */

#ifndef __access_h
#define __access_h

#define	ACC_PUBLIC		    0x0001
#define	ACC_PRIVATE		    0x0002
#define	ACC_PROTECTED	    0x0004
#define	ACC_STATIC		    0x0008
#define	ACC_FINAL		    0x0010
#define	ACC_SYNCHRONISED	0x0020
#define	ACC_VOLATILE		0x0040
#define	ACC_TRANSIENT		0x0080
#define	ACC_NATIVE		    0x0100
#define	ACC_INTERFACE		0x0200
#define	ACC_ABSTRACT		0x0400

#define	ACC_TRANSLATED		0x4000
#define	ACC_VERIFIED		0x8000


#endif
