/* method_inlining.h
   
   Header file for method inlining
   
   Written by: SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */
#ifndef _METHOD_INLINING_H_
#define _METHOD_INLINING_H_

#include <stdlib.h>
#include <assert.h>
#include "basic.h"
#include "gtypes.h"
#include "fast_mem_allocator.h"

enum JITLevel {KAFFE_JIT = 1, AGGRESSIVE_JIT = 2};

/* some structure declaration for avaid warning */
struct _methods;
struct ExceptionInfoTable;
struct TranslationInfo;
struct InstrNode;
struct CFG;
struct ArgSet_t;

/* method instance structure
   contains some runtime information
   such as call chain and variable map  */
typedef struct MethodInstance {
    struct _methods *method;

    int retBPC;
    void *retNativePC;

    /* caller's map information at the end of inlined method */
    int *callerVarMap;
    int callerLocalNO;
    int callerStackNO;

    /* for tracking call chain within a method */
    struct MethodInstance *caller;

    int retStackTop;

    char JITLevel;

    /* exception related information */
    struct ExceptionInfoTable *exceptionInfoTable;

    void **translatedEHCodes;
    void **initEHVarMap;
    int *localVarNO;
    int *stackVarNO;
} MethodInstance;

/* inline graph structure for method inlining
   This data structure is only used during translation.
   callee informations are made as linked list */
typedef struct InlineGraph {
    MethodInstance *methodInstance;

    /* depth information */
    int depth;

    /* for merging caller and callee into one CFG structure */
    struct InstrNode *instr;
    struct InstrNode *head, *tail;

    /* for graph manipulation */
    struct InlineGraph *caller;
    struct InlineGraph *callee;
    struct InlineGraph *nextCallee;

#ifdef TYPE_ANALYSIS
    /* for type analysis */
    struct ArgSet_t *argset;
    int    startInstrNum;
#endif
} InlineGraph;

/* accessors defined in method_inlining.ic file */
MethodInstance *MI_alloc(void);
struct _methods *MI_GetMethod(MethodInstance *this);
void MI_SetMethod(MethodInstance *this ,struct _methods *method);
int MI_GetRetBPC(MethodInstance *this);
void MI_SetRetBPC(MethodInstance *this ,int bpc);
void *MI_GetRetNativePC(MethodInstance *this);
void MI_SetRetNativePC(MethodInstance *this, void *pc);
int *MI_GetCallerVarMap(MethodInstance *this);
void MI_SetCallerVarMap(MethodInstance *this, int *var_map);
int MI_GetCallerLocalNO(MethodInstance *this);
void MI_SetCallerLocalNO(MethodInstance *this, int local_no);
int MI_GetCallerStackNO(MethodInstance *this);
void MI_SetCallerStackNO(MethodInstance *this, int stack_no);
void MI_SetCaller(MethodInstance *this ,MethodInstance *caller);
MethodInstance *MI_GetCaller(MethodInstance *this);
void MI_SetRetAddr(MethodInstance *MI ,int retNativePC);
int MI_GetRetAddr(MethodInstance *this);
void MI_SetRetStackTop(MethodInstance *this ,int stackTop);
int MI_GetRetStackTop(MethodInstance *this);
struct ExceptionInfoTable *MI_GetExceptionInfoTable(MethodInstance *this);
bool MI_IsEmptyTranslatedEHCodes(MethodInstance *this);
void MI_AllocTranslatedEHCodes(MethodInstance *this);
nativecode *MI_GetTranslatedEHCode(MethodInstance *this ,int idx);
void MI_SetTranslatedEHCode(MethodInstance *this ,int idx ,nativecode *code);
void *MI_GetInitEHVarMap(MethodInstance *this ,int idx);
void MI_SetInitEHVarMap(MethodInstance *this ,int idx ,void *map);
int MI_GetLocalVarNO(MethodInstance *this ,int idx);
void MI_SetLocalVarNO(MethodInstance *this ,int idx ,int localVarNO);
int MI_GetStackVarNO(MethodInstance *this ,int idx);
void MI_SetLaTTeTranslated(MethodInstance *this);
bool MI_IsLaTTeTranslated(MethodInstance *this);
void MI_SetKaffeTranslated(MethodInstance *this);
bool MI_IsKaffeTranslated(MethodInstance *this);
void IG_SetMI(InlineGraph *this ,MethodInstance *instance);
MethodInstance *IG_GetMI(InlineGraph *this);
InlineGraph *IG_alloc(void);
InlineGraph *IG_init(struct _methods *method);
void IG_SetBPC(InlineGraph *this ,int bpc);
int IG_GetBPC(InlineGraph *this);
void IG_SetDepth(InlineGraph *this ,int depth);
int IG_GetDepth(InlineGraph *this);
InlineGraph *IG_GetCaller(InlineGraph *this);
void IG_SetCaller(InlineGraph *this ,InlineGraph *caller);
InlineGraph *IG_GetCallee(InlineGraph *this);
void IG_SetCallee(InlineGraph *this ,InlineGraph *callee);
InlineGraph *IG_GetNextCallee(InlineGraph *this);
void IG_SetNextCallee(InlineGraph *this ,InlineGraph *nextCallee);
struct _methods *IG_GetMethod(InlineGraph *this);
void IG_SetCallerCalleeRelation(InlineGraph *this ,InlineGraph *callee);
int IG_GetStackTop(InlineGraph *this);
void IG_SetStackTop(InlineGraph *this ,int stack_top);
void IG_SetHead(InlineGraph *this ,struct InstrNode *head);
void IG_SetTail(InlineGraph *this ,struct InstrNode *tail);
void IG_SetInstr(InlineGraph *this ,struct InstrNode *instr);
struct InstrNode *IG_GetHead(InlineGraph *this);
struct InstrNode *IG_GetTail(InlineGraph *this);
struct InstrNode *IG_GetInstr(InlineGraph *this);
int IG_GetLocalOffset(InlineGraph *graph);
int IG_GetStackOffset(InlineGraph *graph);
void IG_SetLocalOffset(InlineGraph *graph, int offset);
void IG_SetStackOffset(InlineGraph *graph, int offset);
void IG_DeregisterMethods(InlineGraph *graph);

struct ArgSet_t *IG_GetArgSet(InlineGraph *graph);
void IG_SetArgSet(InlineGraph *graph ,struct ArgSet_t *argset);

/* functions defined in method_inlining.c files */
bool IG_IsMethodInlinable(InlineGraph *this ,struct _methods *callee);
void IG_RegisterMethod(InlineGraph *this ,struct _methods *called_method ,
		       int bpc ,struct ArgSet_t *arg_set ,
		       struct InstrNode *instr ,int stack_top);
void IG_print_inline_info(struct CFG *cfg);
void IG_print_inline_info_as_xvcg(struct CFG *cfg);

extern int The_Method_Inlining_Max_Size;
extern int The_Method_Inlining_Max_Depth;

/* inline functions */
#include "method_inlining.ic"

#endif _METHOD_INLINING_H_

