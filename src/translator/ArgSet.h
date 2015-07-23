/* ArgSet.h
   Declare ArgSet structure
   
   Written by: Junpyo Lee <walker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __ArgSet_h__
#define __ArgSet_h__

#include "basic.h"
#include "classMethod.h"

typedef struct ArgSet_t {
    int32               size;  /* size of arguments */
    int32               count; /* size of non-null element in argType array */
    Hjava_lang_Class**  argType; /* array whose length is equal to size */
} ArgSet;

/* allocate new ArgSet instance */
ArgSet* AS_new(int32);

/* free ArgSet instance */
void    AS_free(ArgSet*);

/* make ArgSet from Type */
ArgSet* AS_make_from_type(Hjava_lang_Class* type);


/* get the size of arguments */
int32   AS_GetSize(ArgSet*);

/* get the size of arguments whose types are known */
int32   AS_GetCount(ArgSet*);

/* get the idx-th argument type */
Hjava_lang_Class* AS_GetArgType(ArgSet*, int32);

/* set the idx-th argument type */
void    AS_SetArgType(ArgSet*, int32, Hjava_lang_Class*);

/* check if two ArgSets are identical */
bool    AS_IsEqualArgSet(ArgSet*, ArgSet*);


#include "ArgSet.ic"

#endif /* __ArgSet_h__ */
