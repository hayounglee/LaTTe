/* SpecializedMethod.c
   
   Support for multiple method instance
   
   Written by: Junpyo Lee <walker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#include <assert.h>
#include "config.h"
#include "md.h"
#include "flags.h"
#include "classMethod.h"
#include "SpecializedMethod.h"
#include "translator_driver.h"
#include "bytecode_analysis.h"
// to instantiate inline functions for home-less modules...
#undef INLINE
#define INLINE
#include "SpecializedMethod.ic"


/* Name        : SM_is_method_translated
   Description : check if the method is translated for the receiver type
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
   Notes:  */
bool
SM_is_method_translated(Method* meth, Hjava_lang_Class* type) 
{
    SpecializedMethod* sm;

    assert(meth);
    sm = SMS_GetTypeElement(meth->sms, type);
    if (sm && SM_GetNativeCode(sm) != NULL) return true;
    else return false;
}

/* Name        : SM_is_method_generally_translated
   Description : check if the method is translated as not specialized version
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
   Notes:  */
bool
SM_is_method_generally_translated(Method* meth) 
{
    return SM_is_method_translated(meth, GeneralType);
}


/* Name        : _make_SpecializedMethod_with_type
   Description : make SpecializedMethod instance using type
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
   Notes:  */
static SpecializedMethod* 
_make_SpecializedMethod_with_type(Method* meth, Hjava_lang_Class* type)
{
    SpecializedMethodSet* sms = meth->sms;
    SpecializedMethod* sm;

    sm = SMS_GetTypeElement(sms, type);
    if (sm == NULL) {
        sm = SM_new(type, meth);
        SMS_AddSM(meth->sms, sm);
    }
    return sm;
}

/* Name        : make_SpecializedMethod_with_ArgSet
   Description : make SpecializedMethod instance whose type is SpecialType 
                 using argument set
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
   Notes:  */
static SpecializedMethod* 
_make_SpecializedMethod_with_ArgSet(Method* meth, ArgSet* argset)
{
    SpecializedMethodSet* sms = meth->sms;
    SpecializedMethod* sm;

    sm = SMS_GetSpecialElement(sms, argset);

    if (sm == NULL) {
        sm = SM_new(SpecialType, meth);
        SM_SetArgSet(sm, argset);
        SMS_AddSM(sms, sm);
    }

    return sm;
}

/* Name        : SM_make_SpecializedMethod
   Description : make SpecializedMethod from instance argset & receiver type
                 if corresponding instance exists, just return it
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
   Notes:  */
SpecializedMethod*
SM_make_SpecializedMethod(Method* meth, ArgSet* as, Hjava_lang_Class* type) 
{
    SpecializedMethod* sm = NULL;

    // To use Method_HaveUnresolved(meth), bytecode analysis must be done.
    // Caution!! if this method is not translated, it can be a overhead.
    if (Method_GetBcodeInfo(meth) == NULL
        && !Method_IsNative(meth))
      BA_bytecode_analysis(meth);

    //Some methods are excluded from customization & specialization
    if (!Method_HaveUnresolved(meth) 
        || equalUtf8Consts(meth->name, constructor_name) 
        || Method_IsNative(meth)) {
        as = NULL;
        type = GeneralType;
    }

    if (!The_Use_Specialization_Flag)
      as = NULL;
    if (!The_Use_Customization_Flag)
      type = GeneralType;

    // make specialized method instance when more than two argument
    // types are known at compile time
    if (as != NULL && AS_GetCount(as) >= 2)
      sm = _make_SpecializedMethod_with_ArgSet(meth, as);
    else
      sm = _make_SpecializedMethod_with_type(meth, type);

    assert(sm);
    return sm;
}

/* Name        : SM_MakeArgSetFromSM
   Description : make ArgSet instance from SpecializedMethod
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
   Notes:  */
ArgSet*
SM_MakeArgSetFromSM(SpecializedMethod* sm) 
{
    ArgSet* argset;

    if (sm == NULL || SM_IsGeneralType(sm)) {
        return NULL;
    } else if (SM_IsSpecialType(sm)) {
        assert(SM_GetArgSet(sm));
        assert(AS_GetSize(SM_GetArgSet(sm)) != 0);

        return SM_GetArgSet(sm);
    }

    argset = AS_new(1);
    AS_SetArgType(argset, 0, SM_GetReceiverType(sm));

    return argset;
}


/* Name        : SM_MakeTrampCode
   Description : make corresponding trampoline code to SpecializedMethod 
                 instance
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
   Notes:  */
void*
SM_MakeTrampCode(SpecializedMethod* sm) 
{
    methodTrampoline* tramp;

    assert(sm);
    if(SM_GetTrampCode(sm) != NULL)
      return SM_GetTrampCode(sm);

    tramp = (methodTrampoline*)gc_malloc(sizeof(methodTrampoline),
					 &gc_jit_code);
        
    FILL_IN_JIT_TRAMPOLINE(tramp, sm, dispatch_method_with_SM);

    SM_SetTrampCode(sm, &tramp->code[0]);

    FLUSH_DCACHE(&tramp->code[0], &tramp->code[3]);

    return (void*)&tramp->code[0];
}

#ifdef INLINE_CACHE
/* Name        : CC_GetMethod
   Description : get method from type and method index in CallContext instance
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
   Notes:  */
Method*
CC_GetMethod(CallContext* cc, Hjava_lang_Class* type) 
{
    int meth_idx = CC_GetMethodIdx(cc);
    meth_idx = meth_idx > 0 ? meth_idx-DTABLE_METHODINDEX : meth_idx;
    return get_method_from_class_and_index(type, meth_idx);
}


/* Name        : CC_MakeTrampCode
   Description : make corresponding trampoline code to CallContext instance
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition: trampoline code of cc must be null
   Post-condition:
   Notes:  */
void*
CC_MakeTrampCode(CallContext* cc) 
{
    methodTrampoline* tramp;

    assert(cc);
    assert(CC_GetTrampCode(cc) == NULL);

    tramp = (methodTrampoline*)gc_malloc(sizeof(methodTrampoline),
					 &gc_jit_code);
        
    FILL_IN_JIT_TRAMPOLINE(tramp, cc, dispatch_method_with_CC);

    CC_SetTrampCode(cc, &tramp->code[0]);

    FLUSH_DCACHE(&tramp->code[0], &tramp->code[3]);

    return (void*)&tramp->code[0];
}
#endif
