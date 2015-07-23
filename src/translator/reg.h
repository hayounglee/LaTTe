/* reg.h
   
   physical register related functions
   
   Written by: SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __REG_H__
#define __REG_H__

#include "basic.h"
#include "gtypes.h"
#include "pseudo_reg.h"
#include "fast_mem_allocator.h"

typedef Var Reg;
typedef VarType RegType;

/* sparc physical registers */
enum {
    g0 = 0, g1, g2, g3, g4, g5, g6, g7, /* 0-7 */
    o0 = 8, o1, o2, o3 ,o4, o5, o6, o7, /* 8-15 */
    l0 = 16,l1, l2, l3, l4, l5, l6, l7, /* 16-23 */
    i0 = 24,i1, i2, i3, i4, i5, i6, i7, /* 24-31 */
    glast = i7, 

    f0 = 32, f1, f2, f3, f4, f5, f6, f7, /* 32-39 */
    f8 = 40, f9, f10, f11, f12, f13, f14, f15,
    f16= 48, f17, f18, f19, f20, f21, f22, f23, 
    f24= 56, f25, f26, f27, f28, f29, f30, f31, /* 56-63 */
    flfirst = f2, flast = f31, 

    cc0= 64, cc1, cc2, cc3, cc4, cc5, cc6, cc7, /* 64-71 */
    cclast = cc7, 
    Reg_number
};


/* stack pointer and frame pointer */
enum { sp = o6, fp = i6 };

#define IRET i7
#define ORET o7
#define SP  sp
#define FP  fp

bool Reg_IsHW(Reg reg);
bool Reg_IsInteger(Reg reg);
bool Reg_IsFloat(Reg reg);
bool Reg_IsSpill(Reg reg);
bool Reg_IsRegister(Reg reg);
bool Reg_IsAvailable(Reg reg);
bool Reg_IsParameter(Reg reg);
Reg Reg_get_parameter(int idx);
Reg Reg_get_argument(int idx);
char *Reg_GetName(Reg reg);
RegType Reg_GetType(Reg reg);
Reg Reg_get_returning(VarType type);
Reg Reg_get_returned(VarType type);

#include "reg.ic"

#endif /* __REG_H__ */
