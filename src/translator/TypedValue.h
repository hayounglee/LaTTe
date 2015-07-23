/* TypedValue.h
   
   Declare TypedValue structure
   
   Written by: Suhyun Kim <zelo@i.am>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef _TypedValue_h_
#define _TypedValue_h_

#include "basic.h"
#include "classMethod.h"
#include "InstrNode.h"

/* The origin of TypedValue */
typedef enum { 
    TVTS_FROM_NEW = 1, 
    TVTS_FROM_KNOWN_ARG,
    TVTS_FROM_UNKNOWN_ARG 
} TVTypeSource; 

typedef struct TypedValue {
    Hjava_lang_Class* type; 
    TVTypeSource source; 
} TypedValue; 

/* make new instance of TypedValue whose type is 'type' */
TypedValue*     TV_new(Hjava_lang_Class* type);
/* get TypedValue defined by 'instr' */
TypedValue*     TV_get_new_typed_value(InstrNode* instr, int index); 


Hjava_lang_Class*   TV_GetType(TypedValue* tv); 
void            TV_SetTypeSource(TypedValue* tv, TVTypeSource type_source);
TVTypeSource    TV_GetTypeSource(TypedValue* tv);

#include "TypedValue.ic"

#endif /* _TypedValue_h_ */
