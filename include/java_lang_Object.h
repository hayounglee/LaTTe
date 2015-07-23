/* java_lang_Object.h
 * Java's base class - the Object.
 *
 * Copyright (c) 1997 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.co.uk>
 */

/* java_lang_Object.h
   
   definition of Java object in LaTTe
   
   Modified by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties.
 */

#ifndef __java_lang_object_h
#define __java_lang_object_h

struct _dispatchTable;
struct Hjava_lang_Class;

typedef struct Hjava_lang_Object {
	struct _dispatchTable*  dtable;

	unsigned int		lock_info;
} Hjava_lang_Object;

#endif
