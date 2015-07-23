/* functions.h

   This header file contains some declarations of
   functions in the file "functions.c".
   
   Written by: SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef _FUNCTIONS_H_
#define _FUNCTIONS_H_

int RT_bin_search(int *key_array, int size, int key);
int RT_is_instanceof(Hjava_lang_Class *class, Hjava_lang_Object *obj);
void RT_check_cast(Hjava_lang_Class *class, Hjava_lang_Object *obj);
/* some gcc specific functions */
extern long long    __divdi3( long long a, long long b );
extern long long    __moddi3( long long a, long long b );
extern long long    __ashldi3( long long a, int b );
extern long long    __ashrdi3( long long a, int b );
extern long long    __lshrdi3( long long a, int b );
extern float        __floatdisf( long long a );
extern double       __floatdidf( long long a );
extern long long    __fixsfdi( float a );
extern long long    __fixdfdi( double a );
extern long long    __muldi3( long long a, long long b );


#endif _FUNCTIONS_H_
