/* pseudo_reg.h
   interfaces to making pseudo variables
   
   Written by: SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __PSEUDO_REG_H__
#define __PSEUDO_REG_H__

#include "basic.h"
#include "gtypes.h"

typedef int Var;

/* variable type information
   T_INT : integer type
   T_LONG : first word of long type 
   T_CC : conditional register, not used in SPARC
   T_FLOAT : float type
   T_DOUBLE : first word of double type
   T_REF : reference type, the same as integer type in SPARC
   T_IVOID : second word of long type
   T_FVOID : second word of double type  */
typedef enum {
    T_INT = 1,
    T_LONG = 2,
    T_CC = 3,
    T_FLOAT = 4,
    T_DOUBLE = 5,
    T_REF = 6,
    T_IVOID = 7,  /* always follows T_LONG */
    T_FVOID = 8,  /* always follows T_DOUBLE */
    T_INVALID
} VarType;

typedef enum {
    ST_STACK = 1,
    ST_LOCAL,
    ST_TEMP,
    ST_VN,
    ST_INVALID
} VarStorageType;

/* making variable macros.
   first character is variable type : I, L, F, D, IV, FV
   last character is variable location : S, L, T */
#define IS(a)  (Var_make_stack(T_INT, a))
#define IL(a)  (Var_make_local(T_INT, a))
#define IT(a)  (Var_make_temp(T_INT, a))

#define LS(a)  (Var_make_stack(T_LONG, a))
#define LL(a)  (Var_make_local(T_LONG, a))
#define LT(a)  (Var_make_temp(T_LONG, a))

#define FS(a)  (Var_make_stack(T_FLOAT, a))
#define FL(a)  (Var_make_local(T_FLOAT, a))
#define FT(a)  (Var_make_temp(T_FLOAT, a))

#define DS(a)  (Var_make_stack(T_DOUBLE, a))
#define DL(a)  (Var_make_local(T_DOUBLE, a))
#define DT(a)  (Var_make_temp(T_DOUBLE, a))

#define RS(a)  (Var_make_stack(T_REF, a))
#define RL(a)  (Var_make_local(T_REF, a))
#define RT(a)  (Var_make_temp(T_REF, a))

#define IVS(a) (Var_make_stack(T_IVOID, a))
#define IVL(a) (Var_make_local(T_IVOID, a))
#define IVT(a) (Var_make_temp(T_IVOID, a))

#define FVS(a) (Var_make_stack(T_FVOID, a))
#define FVL(a) (Var_make_local(T_FVOID, a))
#define FVT(a) (Var_make_temp(T_FVOID, a))

#define CC     (Var_make_temp(T_CC, 10))

struct InlineGraph;
struct TranslationInfo;

void Var_prepare_creation(struct InlineGraph *inline_graph);
Var Var_make(VarType type, VarStorageType storage_type, int number);
Var Var_make_local(VarType type, int number);
Var Var_make_stack(VarType type, int number);
Var Var_make_temp(VarType type, int number);
Var Var_make_vn(VarType type, int number);
VarStorageType Var_GetStorageType(Var variable);
VarType Var_GetType(Var variable);
bool Var_IsVariable(Var variable);
char *Var_GetName(Var variable);
char *Var_get_var_reg_name(int var_reg);

extern int Var_offset_of_stack_var;
extern int Var_offset_of_local_var;
extern int Var_offset_of_temp_var;

#include "pseudo_reg.ic"

#endif __PSEUDO_REG_H__
