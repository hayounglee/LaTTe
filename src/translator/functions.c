/* functions.c

   This file contains some utility functions which will be called by
   translated code. These are handled as function call mainly because
   it is too complicated to make as assembly sequence.

   Written by: SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */
#include <stdlib.h>
#include <assert.h>
#include "config.h"
#include "classMethod.h"
#include "class_inclusion_test.h"
#include "errors.h"
#include "exception.h"

/* Name        : bin_search
   Description : Used for translating LOOKUPSWITCH bytecode
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
int
RT_bin_search( int* key_array, int size, int key )
{
    int   low, high, mid;

    low = 0;
    high = size - 1;

    while (low <= high) {
        mid = (low + high) / 2;
        if (key_array[mid] > key) {
            high = mid - 1;
        } else if (key_array[mid] < key) {
            low = mid + 1;
        } else {
            return mid;
        }
    }

    return -1;
}


/* Name        : is_instanceof
   Description : Used for translating INSTANCEOF bytecode
                 Alternatively, we can generate code sequences.
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
int
RT_is_instanceof( Hjava_lang_Class* class, Hjava_lang_Object* obj )
{
    if (obj != NULL && CIT_is_subclass(Object_GetClass(obj), class)) {
        return 1;
    } else {
        return 0;
    }
}

/* Name        : check_cast
   Description : Used for translating CHECKCAST bytecode
                 Alternatively, we can generate code sequences.
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
RT_check_cast( Hjava_lang_Class* class, Hjava_lang_Object* obj )
{
    if (obj == NULL || CIT_is_subclass(Object_GetClass(obj), class)) {
        return;
    }

    throwException( ClassCastException );
}

