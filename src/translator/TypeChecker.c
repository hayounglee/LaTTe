/* TypeChecker.c
   Support for inline cache.
   
   Written by: Junpyo Lee <walker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */



#include <assert.h>
#include "config.h"
#include "classMethod.h"
#include "md.h"
#include "TypeChecker.h"
#include "SPARC_instr.h"
#include "reg.h"
#include "translator_driver.h"

#undef INLINE
#define INLINE
#include "TypeChecker.ic"

/* Name        : TC_MakeHeader
   Description : make typecheck code
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
   Notes:  
     if text_seg is NULL, allocate memory for check code 

   ==========================================================
   [generated code]
      ld    [%o0], %g3
      sethi %hi(dtable), %g4
      or    %g4, %lo(dtable), %g4
      cmp   %g3, %g4
      be    proc[target]
      add   %o7, %g3
      call  fixup_failed_type_check
      nop
      0(reserved) * 6
      check type
      method
    proc: 
      sethi %hi(target), %g3
      jmp   %g3+lo(target) 
      nop
   ====================> changed!!!
   [generated code]
    fixup:
      call  fixup_failed_type_check
      nop
      0(reserved) * 6
      check type
      method 
--->  ld    [%o0], %g3
      sethi %hi(dtable), %g4
      or    %g4, %lo(dtable), %g4
      cmp   %g3, %g4
      bne   fixup
      add   %o7, %g3
      call  target
      add   %g3, %g0, %o7
   ========================================================== */
void*
TC_MakeHeader(TypeChecker* tc, Method* meth, unsigned* code)
{
    unsigned* start_addr;
    unsigned* ret_addr;
    bool  need_icache_flush = false;

    assert(tc);
    assert(tc->body);
    assert(tc->checkType);

    if (code == NULL) {
        code = 
            (unsigned*) gc_malloc(PROLOGUE_CODE_SIZE + ADDITIONAL_CODE_SIZE_FOR_DISTANT_TARGET, &gc_jit_code);
        need_icache_flush = true;
    }

    start_addr = code;
    *code = assemble_instr1(CALL, (unsigned*)fixup_failed_type_check - code);
    code++;
    *code++ = assemble_nop();

    code = ret_addr = start_addr + OFFSET_OF_CODE_START_FROM_TOP;
    // store type
    code[-2] = (unsigned)TC_GetCheckType(tc);
    // store method pointer
    code[-1] = (unsigned)meth;

    *code++ = assemble_instr5(LD, g3, o0, 0);
    *code++ = assemble_instr2(SETHI, g4, HI(TC_GetCheckType(tc)->dtable));
    *code++ = assemble_instr5(OR, g4, g4, LO(TC_GetCheckType(tc)->dtable));
    *code++ = assemble_instr6(SUBCC, g0, g3, g4);
    *code++ = assemble_instr3(BNE, -14) | PREDICT_NOT_TAKEN;
    *code++ = assemble_instr5(ADD, g3, o7, 0);

    if (need_icache_flush) { //separate typechecking code
        *code = assemble_instr1(CALL, (unsigned*)TC_GetBody(tc) - code);
        code++;
        *code++ = assemble_instr5(ADD, o7, g3, 0);
        FLUSH_DCACHE(start_addr, code);
    }

    registerTypeCheckCode((uintp)start_addr, (uintp)code);

    TC_SetHeader(tc, (void*)ret_addr);

    return (void*)ret_addr;
}

/* Name        : TC_fixup_TypeChecker_headers
   Description : When a method is retranslated, all the TypeCheckers' bodies 
                 which are  equal to 'ocode' have to be changed to 'ncode'. 
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
   Notes:  */
void
TC_fixup_TypeChecker_headers(Method* meth, void* ocode, void* ncode) 
{
    TypeCheckerSet* tcs = meth->tcs;
    int i;

    for (i = 0; i < TCS_GetSize(tcs); i++){
        TypeChecker* tc = TCS_GetIdxElement(tcs,i);
        if (TC_GetBody(tc) == ocode) {
            unsigned* code = (unsigned*)TC_GetHeader(tc);
            TC_SetBody(tc, ncode);
            code += OFFSET_OF_CODE_START_FROM_BOTTOM;
            *code = assemble_instr1(CALL, (unsigned*)ncode - code);
            FLUSH_DCACHE(code, code+1);
        }
    }
}

/* Name        : TC_make_TypeChecker
   Description : make TypeChecker instance check type & compiled code
                 if corresponding instance exists, just return it
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
   Notes:  */
TypeChecker*
TC_make_TypeChecker(Method* meth, Hjava_lang_Class* type, void* code)
{
    TypeCheckerSet* tcs = meth->tcs;
    TypeChecker* tc;

    tc = TCS_GetTypeChecker(tcs, type, code);
    if (tc == NULL) {
        tc = TC_new(type, code);
        TCS_AddTC(tcs, tc);
    }
    assert(tc);

    return tc;
}

