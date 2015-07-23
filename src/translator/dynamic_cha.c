/* dynamic_cha.c
   
   Perform class hierachy analysis at runtime.
   
   Written by: Junpyo Lee <walker@altair.snu.ac.kr>
   
   Copyright (C) 2000 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#include "config.h"
#include "gtypes.h"
#include "classMethod.h"
#include "dynamic_cha.h"
#include "gc.h"
#include "SPARC_instr.h"
#include "md.h"

#define INITIAL_SET_SIZE 5

#define DCHA_ALLOC(size) gc_malloc_fixed(size)
#define DCHA_FREE(addr)  gc_free(addr)

#define DBG(s)

static void
_fixup_call_site(unsigned* addr, unsigned info)
{
    int target = info >> 2;
    int mode = info & 0x3;

    if (mode == DCHA_SPEC_CALL) {
        assert((*addr & CALL) == CALL);

        *addr = assemble_instr1(CALL, target);
    } else {
        assert((*addr & SPARC_NOP) == SPARC_NOP); //nop using add opcode

        *addr = assemble_instr3(BA, target) | ANNUL_BIT;
    }

    FLUSH_DCACHE(addr, addr+1);
}

void 
DCHA_fixup_speculative_call_site(Method* meth)
{
    CallSiteSet* css = meth->callSiteSet;
    int i;

    // if there is not corresponding call site set, then just return.
    if (css == NULL) return;

    for (i = 0; i < css->count; i++) {
        _fixup_call_site(css->set[i].addr, css->set[i].info);
    }
}


void 
DCHA_add_speculative_call_site(Method* meth, uint32* addr, 
                               uint32 target, SpecMode mode)
{
    CallSiteSet* css = meth->callSiteSet;

    assert(mode == DCHA_SPEC_CALL || mode == DCHA_SPEC_INLINE);

    if (css == NULL) {
        css = DCHA_ALLOC(sizeof(CallSiteSet));
        css->set = DCHA_ALLOC(INITIAL_SET_SIZE * sizeof(CallSiteEntry));
        css->capacity = INITIAL_SET_SIZE;
        css->count = 0;
        
        meth->callSiteSet = css;
    }

    if (css->count >= css->capacity) {
        CallSiteEntry* oldset = css->set;
        int oldcapacity = css->capacity;

        css->capacity = oldcapacity * 2;
        css->set = 
            DCHA_ALLOC(css->capacity * sizeof(CallSiteEntry));
        memcpy(css->set, oldset, oldcapacity * sizeof(CallSiteEntry));
        DCHA_FREE(oldset);
    }

DBG(    printf("%s.%s.%s %d\n", meth->class->name->data,
           meth->name->data, meth->signature->data, css->count);)

    css->set[css->count].addr = addr;
    css->set[css->count++].info = target | mode;
}
