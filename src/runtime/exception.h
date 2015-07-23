/*
 * exception.h
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 * Modified by MASS Laboratory, SNU, 1999.
 */

#ifndef __exception_h
#define __exception_h

#include <stdlib.h>
#include "basic.h"
#include "gtypes.h"

struct _exceptionFrame;
struct Hjava_lang_Class;
struct Hjava_lang_Object;

typedef struct _jexceptionEntry {
    uintp start_pc;
    uintp end_pc;
    uintp handler_pc;
    constIndex catch_idx;
    struct Hjava_lang_Class *catch_type;
} jexceptionEntry;

typedef struct _jexception {
    uint32 length;
    jexceptionEntry entry[1];
} jexception;

struct _exceptionFrame;

void throwException(struct Hjava_lang_Object*) __attribute__ ((noreturn));
void throwExternalException(struct Hjava_lang_Object*)
    __attribute__ ((noreturn));

struct Hjava_lang_Object* buildStackTrace(struct _exceptionFrame*);

extern void catchSignal(int, void*);

extern void nullException();
extern void arithmeticException();

#if defined(__WIN32__)
#define SIG_T   void(*)()
#else
#define SIG_T   void*
#endif

/* some getter and setters of jexceptionEntry and jexception */
uint32 JE_GetLength(jexception *this);
jexceptionEntry *JE_GetEntry(jexception *this, int index);

uintp JEE_GetStartPC(jexceptionEntry *this);
uintp JEE_GetEndPC(jexceptionEntry *this);
uintp JEE_GetHandlerPC(jexceptionEntry *this);
constIndex JEE_GetCatchIDX(jexceptionEntry *this);
struct Hjava_lang_Class *JEE_GetCatchType(jexceptionEntry *this);
void JEE_SetCatchType(jexceptionEntry *this, struct Hjava_lang_Class *class);

#include "exception.ic"

#endif
