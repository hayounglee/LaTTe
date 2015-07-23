/* dynamic_cha.h
   
   Header for dynamic class hierachy analysis
   
   Written by: Junpyo Lee <walker@altair.snu.ac.kr>
   
   Copyright (C) 2000 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __DYNAMIC_CHA_H__
#define __DYNAMIC_CHA_H__

#include "gtypes.h"
#include "basic.h"

struct _methods;

typedef enum { 
    DCHA_SPEC_CALL = 0, DCHA_SPEC_INLINE = 1 
} SpecMode;

// In case of SPEC_CALL, 
//     info = target address | SPEC_CALL
//     if the method is overridden, change the target of call instruction.
// In case of SPEC_INLINE, 
//     info = relative address | SPEC_INLINE
//     if the method is overridden, change the nop instruction into
//     "ba" instruction with relative address.

typedef struct CallSiteEntry {
    uint32* addr;
    uint32  info;
} CallSiteEntry;

typedef struct CallSiteSet {
    CallSiteEntry* set;
    int capacity;
    int count;
} CallSiteSet;

/* When the specified method is overridden, all call sites which presumes 
   that method is final have to be fixed. */
void DCHA_fixup_speculative_call_site(struct _methods*);

/* add the specified address to the list which contains call sites which must 
   be fixed when the specified method is overridden. */
void DCHA_add_speculative_call_site(struct _methods*, uint32*, 
                                    uint32, SpecMode);

#endif /* __DYNAMIC_CHA_H__ */
