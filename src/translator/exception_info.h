/* exception_info.h
   
   Header file for exception handling

   To translate exception handlers, we must keep some informations
   generated during translation of normal flow.  These informations
   are a table of (native PC, bytecode PC, local variable map) for
   every exception generatable instructions.  Using this table
   information, we can find exeption handler and can give hints for
   register allocation of exception handler.
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
               SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __EXCEPTION_INFO_H__
#define __EXCEPTION_INFO_H__

#include <assert.h>
#include "gtypes.h"
#include "basic.h"

struct MethodInstance;

/* `ExceptionInfo' structure
   An entry of exception_info_table */
typedef struct ExceptionInfo {
    uint32 nativePC;
    uint32 bytecodePC;

    int *varMap;	/* offset value is in methodInstance structure */

    struct MethodInstance *methodInstance;
} ExceptionInfo;

/* `ExceptionInfoTable' structure */
typedef struct ExceptionInfoTable {
    int size;
    int numOfInfos;

    ExceptionInfo *info;
} ExceptionInfoTable;

ExceptionInfoTable *EIT_alloc();
void EIT_init(ExceptionInfoTable *table, int init_size);
void EIT_insert(ExceptionInfoTable *table,
		uint32 native_pc,
		uint32 bytecode_pc,
		int *var_map,
		struct MethodInstance *instance);
ExceptionInfo *EIT_find(ExceptionInfoTable *table,
			uint32 native_pc);
int EIT_GetNativePC(ExceptionInfo *this);
int EIT_GetBytecodePC(ExceptionInfo *this);
struct MethodInstance *EIT_GetMethodInstance(ExceptionInfo *this);
int *EIT_GetVariableMap(ExceptionInfo *this);

#include "exception_info.ic"

#endif __EXCEPTION_INFO_H__

