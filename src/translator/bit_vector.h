//
// File Name : bit_vector.h
// Author    : Yang, Byung-Sun
//
// Description
//       This is a very simple implementation of bit vector which will be used
//       in the translation stage 1 and 2.
//
#ifndef __BIT_VECTOR_H__
#define __BIT_VECTOR_H__

#include "basic.h"
#include "gtypes.h"


//
// utility functions
//

extern const word  Mask_For_Word[32];

void     BV_SetBit( word* bit_vector, int index );
bool     BV_IsSet( word* bit_vector, int index );
void     BV_ClearBit( word* bit_vector, int index );
void     BV_Copy( word* dest, word* src, int size ); // size in words
void     BV_OR( word* dest, word* src, int size );
void     BV_AND( word * dest, word * src, int size );
void     BV_Init( word* bit_vector, int size );
void     BV_ClearAll(word *, int );
void     BV_SetAll( word* bit_vector, int size );



#include "bit_vector.ic"

#endif __BIT_VECTOR_H__
