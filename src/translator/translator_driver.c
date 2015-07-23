/* translator_driver.c
   Interface to the LaTTe translator.
   
   Written by: Junpyo Lee <walker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#include "config.h"
#include <assert.h>
#include "classMethod.h"
#include "translate.h"
#include <strings.h>
#ifdef USE_KAFFE_TRANSLATOR
#include "machine.h"
#endif /* USE_KAFFE_TRANSLATOR */

#ifdef INLINE_CACHE
#include "TypeChecker.h"
#endif

#ifdef CUSTOMIZATION
#include "SpecializedMethod.h"
#include "ArgSet.h"
/* Name        : TD_invoke_translator
   Description : make TranslationInfo instance using SpecializedMethod 
                 instance and translate with it.
                 type is used to determine whether making check code or not.
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
   Notes:  */
void 
TD_invoke_translator(SpecializedMethod* sm, Hjava_lang_Class* type) 
{
    TranslationInfo *info
        = (TranslationInfo *) alloca(sizeof(TranslationInfo));
    Method* meth = SM_GetMethod(sm);

    // If this class needs initializing, do it now.
    if (meth->class->state != CSTATE_OK) {
        processClass(meth->class, CSTATE_OK);
    }

    bzero(info, sizeof(TranslationInfo));

    TI_SetRootMethod(info, meth);
    TI_SetSM(info, sm);
    
    if (type != NULL 
        && !SM_is_general_type(type)
        && !SM_is_special_type(type)) 
      TI_SetCheckType(info, type);

#ifdef USE_KAFFE_TRANSLATOR
    {
        void *ocode, *ncode;

        ocode = METHOD_NATIVECODE(meth);
        kaffe_translate(meth);
        ncode = METHOD_NATIVECODE(meth);

        SM_SetNativeCode(sm, ncode);
        
        fixupDtableItable(meth, SM_GetReceiverType(sm), ncode);
    }
#else /* not USE_KAFFE_TRANSLATOR */   
    translate(info);

    assert(SM_GetNativeCode(sm));

    //fixing-up dtable and itable
    fixupDtableItable(meth, SM_GetReceiverType(sm), SM_GetNativeCode(sm));
#endif /* not USE_KAFFE_TRANSLATOR */    
}

/* Name        : Fixup functions
   Description : 
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
   Notes: 
    1. General characteristics
       - driver functions for translation
       - make SpecializedMethod instance and pass it to the translator
       - according to the call site, 'return value' is either 
         method body or type check header
       - fixing-up dtable and itable is done in translator
    2. Sorts
       - TD_fixup_trampoline_with_SM : only resolved call sites
       - TD_fixup_trampoline_with_CC : target for specialization but the method
                                       is not resolved
       - TD_fixup_trampoline_with_inlinecache : inline cache call sites
                                                input(receiver, method idx)
       - TD_fixup_trampoline_with_object : the call is done ld/ld/jmpl
   */
void*
TD_fixup_trampoline_with_SM(SpecializedMethod* sm) 
{
    void* ncode = NULL;

    ncode = SM_GetNativeCode(sm);

    if (ncode == NULL){
        //translate the method
        TD_invoke_translator(sm, SM_GetReceiverType(sm));
        ncode = SM_GetNativeCode(sm);
        assert(ncode);
    }
    return ncode;
}



void*
TD_fixup_trampoline_with_object(Hjava_lang_Object* obj, Method* meth)
{
    void* ncode;
    SpecializedMethod* sm;
    Hjava_lang_Class* type;

    if (meth->accflags & ACC_STATIC)
      type = GeneralType;
    else
      type = Object_GetClass(obj);

    sm = SM_make_SpecializedMethod(meth, NULL, type);

    ncode = SM_GetNativeCode(sm);
    if (ncode == NULL) {    // Generate code on demand.
        TD_invoke_translator(sm, type);
        ncode = SM_GetNativeCode(sm);
        assert(ncode);
    }
    return ncode;
}



#ifdef INLINE_CACHE
void*
TD_fixup_trampoline_with_CC(CallContext* cc, Hjava_lang_Object* obj)
{
    Method* meth;
    Hjava_lang_Class* type;
    SpecializedMethod* sm;
    TypeChecker* tc;

    type = Object_GetClass(obj);
    meth = CC_GetMethod(cc, type);

    // make SpecializedMethod using type and known argument set
    AS_SetArgType(CC_GetKnownSet(cc), 0, type); // add receiver into argset
    sm = SM_make_SpecializedMethod(meth, CC_GetKnownSet(cc), type);

    if (SM_GetNativeCode(sm) == NULL)
      TD_invoke_translator(sm, type);

    // make type-checker from type and method body
    tc = TC_make_TypeChecker(meth, type, SM_GetNativeCode(sm));
    if (TC_GetHeader(tc) == NULL)
        return TC_MakeHeader(tc, meth, NULL);
    else 
        return TC_GetHeader( tc );
}

void*
TD_fixup_trampoline_with_inlinecache(Hjava_lang_Object* obj, int offset)
{
    SpecializedMethod* sm;
    TypeChecker* tc;
    Hjava_lang_Class* type;
    Method* meth;
    int meth_idx;

    if (offset >= 0)
      meth_idx = offset/4 - DTABLE_METHODINDEX;
    else
      meth_idx = offset/4;

    type = Object_GetClass(obj);
    meth = get_method_from_class_and_index(type, meth_idx);

    sm = SM_make_SpecializedMethod(meth, NULL, type);
    if (SM_GetNativeCode(sm) == NULL)
      TD_invoke_translator(sm, type);

    // make type-checker from type and method body
    tc = TC_make_TypeChecker(meth, type, SM_GetNativeCode(sm));
    if (TC_GetHeader(tc) == NULL)
        return TC_MakeHeader(tc, meth, NULL);
    else 
        return TC_GetHeader(tc);
}

#endif /* INLINE_CACHE */
#else /* not CUSTOMIZATION */

#ifdef INLINE_CACHE
void*
TD_fixup_trampoline_with_inlinecache(Hjava_lang_Object* obj, int offset)
{
    TypeChecker* tc;
    Hjava_lang_Class* type;
    Method* meth;
    int meth_idx;

    if (offset >= 0)
      meth_idx = offset/4 - DTABLE_METHODINDEX;
    else
      meth_idx = offset/4;

    type = Object_GetClass(obj);
    meth = get_method_from_class_and_index(type, meth_idx);

    if (!Method_IsTranslated(meth)) {
        TranslationInfo *info
            = (TranslationInfo *) alloca(sizeof(TranslationInfo));
        bzero(info, sizeof(TranslationInfo));
        TI_SetRootMethod(info, meth);
        translate(info);

        METHOD_NATIVECODE(meth) = TI_GetNativeCode(info);
    }

    // make type-checker from type and method body
    tc = TC_make_TypeChecker(meth, type, Method_GetNativeCode(meth));
    if (TC_GetHeader(tc) == NULL)
        return TC_MakeHeader(tc, meth, NULL);
    else 
        return TC_GetHeader(tc);
}
#endif /* INLINE_CACHE */
#endif /* not CUSTOMIZATION */
