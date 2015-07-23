/* InvokeType.h
   Type of invoke instruction(-virtual, -static, -special, -interface)
   definition

   Written by: Junpyo Lee <walker@altair.snu.ac.kr>

   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __InvokeType_h__
#define __InvokeType_h__
/* the origin of INVOKE instruction.
   When using 'type analysis', 'invoke-' instruction is just changed to
   pseudo instruction, 'INVOKE', in phase2, and the real translation is done 
   during 'type analysis'. So there must be a way to pass the origin of
   'INVOKE' instruction from phase2 to 'type analyzer'. */
typedef enum InvokeType_t {
    InvokeVirtualType,
    InvokeSpecialType,
    InvokeStaticType,
    InvokeInterfaceType 
} InvokeType;

#endif /* __InvokeType_h__ */
