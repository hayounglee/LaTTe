/* reg.c
   
   physical register related functions
   
   Written by: SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */
#include "config.h"
#include "reg.h"

/* sparc physical registers */
int _is_available_register[] = {
    0, 0, 0, 0, 0, 0, 0, 0, /* g0-g7 : not available */
    1, 1, 1, 1, 1, 1, 0, 0, /* o0-o7 */
    1, 1, 1, 1, 1, 1, 1, 1, /* l0-l7 */
    1, 1, 1, 1, 1, 1, 0, 0, /* i0-i7 */
    
    1, 1, 1, 1, 1, 1, 1, 1, /* f0-f7 */
    1, 1, 1, 1, 1, 1, 1, 1, /* f8-f15 */
    1, 1, 1, 1, 1, 1, 1, 1, /* f16-f23 */
    1, 1, 1, 1, 1, 1, 1, 1, /* f24-31 */
    1, 1, 1, 1, 1, 1, 1, 1, /* cc0-cc7 */
    REG_INVALID
};

int _parameter_registers[] = {
    o0, o1, o2, o3, o4, o5,
    REG_INVALID
};

int _argument_registers[] = {
    i0, i1, i2, i3, i4, i5,
    REG_INVALID
};

char *_reg_names[] = {
    "g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7",
    "o0", "o1", "o2", "o3", "o4", "o5", "sp", "o7",
    "l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7",
    "i0", "i1", "i2", "i3", "i4", "i5", "fp", "i7",
    "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
    "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15", 
    "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
    "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31"
};

#undef INLINE
#define INLINE
#include "reg.ic"

/* actual function definitions are in reg.ic  */
