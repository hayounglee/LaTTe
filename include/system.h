/*
 * system.h
 * Defines for this system.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 *
 * Modified by MASS Laboratory, SNU, 1999
 */

#ifndef __system_h
#define __system_h

#define	vm_version		VMVERSION
#define	vm_vendor		"MASS Laboratory, SNU"
#define	vm_vendor_url		"http://altair.snu.ac.kr/latte/"
#define	vm_class_version	"1"

#if defined(unix)
#define	file_seperator		"/"
#define	path_seperator		":"
#define	line_seperator		"\n"
#elif defined(__WIN32__) || defined(__OS2__)
#define	file_seperator		"\\"
#define	path_seperator		";"
#define	line_seperator		"\r\n"
#elif defined(__amigaos__)
#define	file_seperator		"/"
#define	path_seperator		";"
#define	line_seperator		"\n"
#else
#error "Seperators undefined for this system"
#endif

#define	LATTEHOME		"LATTEHOME"
#define	LATTECLASSPATH		"CLASSPATH"

#endif
