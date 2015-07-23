/* translate.h
   
   declaration of LaTTe JIT translation function and TranslationInfo
   
   Written by: SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __TRANSLATE_H__
#define __TRANSLATE_H__

#include "basic.h"
#include "gtypes.h"

struct Hjava_lang_Class;
struct SpecializedMethod_t;
struct MethodInstance;

typedef enum {
    TR_FROM_ORIGINAL, TR_FROM_OBJECT, TR_FROM_EXCEPTION
} TRInfo;


/* This contains variable informations related translation */
typedef struct TranslationInfo {
    /* informations to translation */
    /* root method and translation starting point. */
    Method *rootMethod;
    int startBytecodePC;

    /* tells where translation is called. */
    TRInfo info;

    /* variable map at generated site */
    int *genMap;
    /* variable map at return point */
    int *retMap;

    int callerLocalNO;
    int callerStackNO;

    /* typechecking information */
    struct Hjava_lang_Class *checkType;

    /* returning information */
    void *returnAddr;
    int returnStackTop;

    /* information from translation */
    nativecode *nativeCode;
    int *initMap;		/* returned from stage3 */
    int localVarNO;		/* must be set during translation */
    int stackVarNO;

    struct MethodInstance *rootInstance;
    
    /* customization related */
#ifdef CUSTOMIZATION
    struct SpecializedMethod_t *sm;
#endif /* CUSTOMIZATION */
} TranslationInfo;

/* function declaration in translate.ic */
void TI_SetRootMethod(TranslationInfo *info ,Method *method);
Method *TI_GetRootMethod(TranslationInfo *info);
void TI_SetStartBytecodePC(TranslationInfo *info ,int bpc);
int TI_GetStartBytecodePC(TranslationInfo *info);
void TI_SetGenMap(TranslationInfo *info ,int *map);
int *TI_GetGenMap(TranslationInfo *info);
void TI_SetRetMap(TranslationInfo *info ,int *map);
int *TI_GetRetMap(TranslationInfo *info);
void TI_SetCallerLocalNO(TranslationInfo *info ,int no);
int TI_GetCallerLocalNO(TranslationInfo *info);
void TI_SetCallerStackNO(TranslationInfo *info ,int no);
int TI_GetCallerStackNO(TranslationInfo *info);
void TI_SetCheckType(TranslationInfo *info ,struct Hjava_lang_Class *type);
struct Hjava_lang_Class *TI_GetCheckType(TranslationInfo *info);
void TI_SetReturnAddr(TranslationInfo *info ,void *addr);
void *TI_GetReturnAddr(TranslationInfo *info);
void TI_SetReturnStackTop(TranslationInfo *info ,int stack_top);
int TI_GetReturnStackTop(TranslationInfo *info);
void TI_SetNativeCode(TranslationInfo *info ,nativecode *code);
nativecode *TI_GetNativeCode(TranslationInfo *info);
void TI_SetInitMap(TranslationInfo *info ,int *map);
int *TI_GetInitMap(TranslationInfo *info);
void TI_SetLocalVarNO(TranslationInfo *info ,int number);
int TI_GetLocalVarNO(TranslationInfo *info);
void TI_SetStackVarNO(TranslationInfo *info ,int number);
int TI_GetStackVarNO(TranslationInfo *info);
bool TI_IsFromOriginal(TranslationInfo *info);
void TI_SetFromOriginal(TranslationInfo *info);
bool TI_IsFromObject(TranslationInfo *info);
void TI_SetFromObject(TranslationInfo *info);
bool TI_IsFromException(TranslationInfo *info);
void TI_SetFromException(TranslationInfo *info);
struct MethodInstance *TI_GetRootMethodInstance(TranslationInfo *info);
void TI_SetRootMethodInstance(TranslationInfo *info,
			      struct MethodInstance *mi);

#ifdef CUSTOMIZATION
void TI_SetSM(TranslationInfo *info ,struct SpecializedMethod_t *sm);
struct SpecializedMethod_t *TI_GetSM(TranslationInfo *info);
#endif /* CUSTOMIZATION */

/* driver function for LaTTe JIT translation */
void translate(TranslationInfo *info);

#include "translate.ic"

#endif /* __TRANSLATE_H__ */

