/* TypeChecker.h
   Support for inline cache
   
   Written by: Junpyo Lee <walker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __TypeChecker_h__
#define __TypeChecker_h__

#include "basic.h"
#include "classMethod.h"

/* type check code size in word and in byte */
#define  TYPE_CHECK_CODE_INSTR_NUM  16
#define  TYPE_CHECK_CODE_SIZE       (TYPE_CHECK_CODE_INSTR_NUM * 4)

/* profile code size in word and in byte 
   - currently no profile code is inserted */
#define  PROFILE_CODE_INSTR_NUM     0
#define  PROFILE_CODE_SIZE          (PROFILE_CODE_INSTR_NUM * 4)

/* prologue code size in word and in byte 
   - currently the same as type check code */
#define  PROLOGUE_CODE_INSTR_NUM    (TYPE_CHECK_CODE_INSTR_NUM+PROFILE_CODE_INSTR_NUM)
#define  PROLOGUE_CODE_SIZE         (TYPE_CHECK_CODE_SIZE + PROFILE_CODE_SIZE)


/* Type check code layout
    call  fixup_failed_type_check
    nop
    0(reserved) * 6
    check type
    method 
    ld    [%o0], %g3            ---> start of TC
    sethi %hi(dtable), %g4
    or    %g4, %lo(dtable), %g4
    cmp   %g3, %g4
    bne   fixup
    add   %o7, %g3
    call  target                ---> when target is not followed by TC
    add   %g3, %g0, %o7
 */
#define  OFFSET_OF_CODE_START_FROM_TOP 10
#define  OFFSET_OF_CODE_START_FROM_BOTTOM 6
#define  ADDITIONAL_CODE_SIZE_FOR_DISTANT_TARGET 8

/* TypeChecker 
   - when method is called through inline cache, checking receiver type 
   is needed. if type check is confirmed, then jump to the corresponding 
   method body. Otherwise, change call site to 'ld/ld/jmpl' sequence */
typedef struct TypeChecker_t {
    Hjava_lang_Class*   checkType; /* check type */
    void*               header;    /* address of check code */
    void*               body;      /* address of method code */
} TypeChecker;


/* TypeCheckerSet
   - set of TypeCheckers */
typedef struct TypeCheckerSet_t {
    int32               capacity;
    int32               count;
    TypeChecker**       set;
} TypeCheckerSet;

#include "TypeChecker.ic"




/* Make corresponding checking code */
void* TC_MakeHeader(TypeChecker* tc, Method* meth, unsigned* code);

/* Make TypeChecker whose checkType is 'type' and body is 'code'.
   If already exists, then just return it. */
TypeChecker* TC_make_TypeChecker(Method* meth, Hjava_lang_Class* type, void* code);

/* When a method is retranslated, all the TypeCheckers' bodies which are 
   equal to 'ocode' have to be changed to 'ncode'. */
void TC_fixup_TypeChecker_headers(Method* meth, void* ocode, void* ncode);

#endif /* __TypeChecker_h__ */
