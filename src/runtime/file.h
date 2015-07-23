/*
 * file.h
 * File support routines.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 */

#ifndef __file_h
#define __file_h

typedef struct _classFile {
	unsigned char*	base;
	unsigned char*	buf;
	int		size;
	int		type;
} classFile;

#define	readu1(c,f)	(*(c) = (f)->buf[0], (f)->buf += 1)
#define	readu2(c,f)	(*(c) = ((f)->buf[0] << 8) | (f)->buf[1], (f)->buf += 2)
#define	readu4(c,f)	(*(c) = ((f)->buf[0] << 24)|((f)->buf[1] << 16)|\
				((f)->buf[2] << 8)|(f)->buf[3], (f)->buf += 4)

#define	readm(b,l,s,f)	(memcpy(b, (f)->buf, (l)*(s)), (f)->buf += (l)*(s))
#define	seekm(f,l)	((f)->buf += (l))

#endif
