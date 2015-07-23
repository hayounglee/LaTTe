/* MapVar2TValue.h
   
   Declare MapVar2TValue structure.
   
   Written by: Suhyun Kim <zelo@i.am>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef _MapVar2TValue_h_
#define _MapVar2TValue_h_

#include "basic.h"

/* map from variable to TypedValue */
typedef struct MapVar2TValue {
    struct TypedValue* *table; // array[0..size]
    int size; 
} MapVar2TValue; 


// static methods...
MapVar2TValue*  MV2TV_new(int num_of_variables); 
MapVar2TValue*  MV2TV_clone(MapVar2TValue* source); 

// member methods...
int             MV2TV_GetSize(MapVar2TValue* this); 
TypedValue*     MV2TV_Get(MapVar2TValue* this, Var key); 
void            MV2TV_Put(MapVar2TValue* this, Var key, TypedValue* value); 
/* I don't like the following two methods but they are practical. 
   These are identical with above two methods if get_var_num(key)==var_num */
TypedValue*     MV2TV_GetByVarNum(MapVar2TValue* this, int var_num); 
void            MV2TV_PutByVarNum(MapVar2TValue* this, int var_num, TypedValue* value); 
/* nullify the map */
void            MV2TV_Clear(MapVar2TValue* this); 


#include "MapVar2TValue.ic"

#endif /* _MapVar2TValue_h_ */
