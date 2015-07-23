/*
 * path.h
 * Path support routines.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 */

#ifndef __paths_h
#define __paths_h

/* Maximum internal path length */
#if !defined(MAXPATHLEN)
#define	MAXPATHLEN	1024
#endif

#if defined(__WIN32__) || defined(__OS2__)
#define	PATHSEP		';'
#define	DIRSEP		"\\"
#elif defined(__amigaos__)
#define PATHSEP		';'
#define DIRSEP		"/"
#else
#define	PATHSEP		':'
#define	DIRSEP		"/"
#endif

#endif
