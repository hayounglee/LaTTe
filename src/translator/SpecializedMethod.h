/* SpecializedMethod.h
   
   Declare SpecializedMethod structure
   
   Written by: Junpyo Lee <walker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __SpecializedMethod_h__
#define __SpecializedMethod_h__

#include "gtypes.h"
#include "CallSiteInfo.h"
#include "classMethod.h"
#include "ArgSet.h"
#include "basic.h"

#ifdef INLINE_CACHE
struct TypeChecker_t;
#endif

#define GeneralType ((Hjava_lang_Class*)0)
#define SpecialType ((Hjava_lang_Class*)1)

/* SpecializedMethod 
   - When customization or specialization is used, a method can be translated 
   multiple times according to the receiver type or resolved arguments. 
   This structure supports multiple translation of a method  */
typedef struct SpecializedMethod_t {
    Hjava_lang_Class*   receiverType;
    ArgSet*             argset;
    void*               ncode;
    Method*             method;
    CallSiteInfo*       callSiteInfoTable; /* array[bpc] */
    int                 count;	        /* Method run count */
    int                 tr_level;	/* Number of retranslations done */
    void*               trampCode;
#ifdef INLINE_CACHE
    struct TypeChecker_t*        tc;
#endif
} SpecializedMethod;


typedef struct SpecializedMethodSet_t {
    int32               capacity;
    int32               count;
    SpecializedMethod** set;
} SpecializedMethodSet;



/* CallContext
   - For each call site, this structure is a way to deliver 
   the characteristics(called method, known argument type, and so on). 
   But currently, this structure is used only in the call sites where
   called method is not resolved, but there are some known argument type */
typedef struct CallContext_t {
    ArgSet*     knownSet;
    int32       methIdx;
    void*       trampCode;
} CallContext;


// static methods
bool    SM_is_method_translated(Method* meth, Hjava_lang_Class* class);
bool    SM_is_method_generally_translated(Method* meth);

SpecializedMethod* SM_make_SpecializedMethod(Method* meth, 
                                             ArgSet* as, 
                                             Hjava_lang_Class* type);


// member functions which are not inlined
ArgSet* SM_MakeArgSetFromSM(SpecializedMethod* sm);
void*   SM_MakeTrampCode(SpecializedMethod* sm);

Method* CC_GetMethod(CallContext* cc, Hjava_lang_Class* type);
void*   CC_MakeTrampCode(CallContext* cc);



#include "SpecializedMethod.ic"

#endif /* __SpecializedMethod_h__ */

