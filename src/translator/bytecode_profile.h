/* bytecode_profile.h
   profile bytecode execution behavior
   
   Written by: Suhyun Kim <zelo@i.am>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __bytecode_profile_h__
#define __bytecode_profile_h__

#include "classMethod.h"
// #include "gtypes.h"

void Pbc_init_profile(Method *method);
void Pbc_increase_frequency(Method *method, int bpc);
void Pbc_print_profile();

#endif // __bytecode_profile_h__

