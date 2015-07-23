/* mem_allocator_for_code.h
   memory allocator for JIT-compiled code
   
   Written by: SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __MEM_ALLOCATOR_FOR_CODE_H__
#define __MEM_ALLOCATOR_FOR_CODE_H__

/* This file was originally introduced by Junpyo Lee in the hope that
   the performance improves or at least the variation is reduced. 
   But the result was not so good and he decided not to use this file. 
   Later SeungIl needed something similar and implemented whole 
   independent thing yet used the same interface. 
   zelo@i.am removed the legacy of Junpyo's work completely to clean 
   the code.  If you want to see the code, check out an old version. ^^ */

void  CMA_initialize_mem_allocator();
void* CMA_allocate_code_block(int);


#endif __MEM_ALLOCATOR_FOR_CODE_H__
