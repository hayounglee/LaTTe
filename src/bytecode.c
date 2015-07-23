/* bytecode.c
   
   c file to store two arrays for bcode manipulation
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties.
 */
#include "config.h"
#include "gtypes.h"
#include "bytecode.h"

#undef INLINE
#define INLINE
#include "bytecode.ic"

const int BCode_opcode_lengths[256] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	/*   0 */
	2, 3, 2, 3, 3, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, /*  16 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /*  32 */
	1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, /*  48 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /*  64 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /*  80 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /*  96 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 112 */
	1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 128 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, /* 144 */
	3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 0, 0, 1, 1, 1, 1, /* 160 */
	1, 1, 3, 3, 3, 3, 3, 3, 3, 5, 1, 3, 2, 3, 1, 1, /* 176 */
	3, 3, 1, 1, 1, 4, 3, 3, 5, 5, 1, 1, 1, 1, 1, 1, /* 192 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, /* 208 */
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, /* 224 */
	3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3	/* 240 */
};

char *BCode_opcode_names[256] = {
    "NOP", "ACONST_NULL", "ICONST_M1", "ICONST_0", "ICONST_1",
    "ICONST_2", "ICONST_3", "ICONST_4", "ICONST_5", "LCONST_0",
    "LCONST_1", "FCONST_0", "FCONST_1", "FCONST_2", "DCONST_0",
    "DCONST_1", "BIPUSH", "SIPUSH", "LDC1", "LDC2",
    "LDC2W", "ILOAD", "LLOAD", "FLOAD", "DLOAD",
    "ALOAD", "ILOAD_0", "ILOAD_1", "ILOAD_2", "ILOAD_3",
    "LLOAD_0", "LLOAD_1", "LLOAD_2", "LLOAD_3", "FLOAD_0",
    "FLOAD_1", "FLOAD_2", "FLOAD_3", "DLOAD_0", "DLOAD_1",
    "DLOAD_2", "DLOAD_3", "ALOAD_0", "ALOAD_1", "ALOAD_2",
    "ALOAD_3", "IALOAD", "LALOAD", "FALOAD", "DALOAD",
    "AALOAD", "BALOAD", "CALOAD", "SALOAD", "ISTORE",
    "LSTORE", "FSTORE", "DSTORE", "ASTORE", "ISTORE_0",
    "ISTORE_1", "ISTORE_2", "ISTORE_3", "LSTORE_0", "LSTORE_1",
    "LSTORE_2", "LSTORE_3", "FSTORE_0", "FSTORE_1", "FSTORE_2",
    "FSTORE_3", "DSTORE_0", "DSTORE_1", "DSTORE_2", "DSTORE_3",
    "ASTORE_0", "ASTORE_1", "ASTORE_2", "ASTORE_3", "IASTORE",
    "LASTORE", "FASTORE", "DASTORE", "AASTORE", "BASTORE",
    "CASTORE", "SASTORE", "POP", "POP2", "DUP",
    "DUP_X1", "DUP_X2", "DUP2", "DUP2_X1", "DUP2_X2",
    "SWAP", "IADD", "LADD", "FADD", "DADD",
    "ISUB", "LSUB", "FSUB", "DSUB", "IMUL",
    "LMUL", "FMUL", "DMUL", "IDIV", "LDIV",
    "FDIV", "DDIV", "IREM", "LREM", "FREM",
    "DREM", "INEG", "LNEG", "FNEG", "DNEG",
    "ISHL", "LSHL", "ISHR", "LSHR", "IUSHR",
    "LUSHR", "IAND", "LAND", "IOR", "LOR",
    "IXOR", "LXOR", "IINC", "I2L", "I2F",
    "I2D", "L2I", "L2F", "L2D", "F2I",
    "F2L", "F2D", "D2I", "D2L", "D2F",
    "INT2BYTE", "INT2CHAR", "INT2SHORT", "LCMP", "FCMPL",
    "FCMPG", "DCMPL", "DCMPG", "IFEQ", "IFNE",
    "IFLT", "IFGE", "IFGT", "IFLE", "IF_ICMPEQ",
    "IF_ICMPNE", "IF_ICMPLT", "IF_ICMPGE", "IF_ICMPGT", "IF_ICMPLE",
    "IF_ACMPEQ", "IF_ACMPNE", "GOTO", "JSR", "RET",
    "TABLESWITCH", "LOOKUPSWITCH", "IRETURN", "LRETURN", "FRETURN",
    "DRETURN", "ARETURN", "RETURN", "GETSTATIC", "PUTSTATIC",
    "GETFIELD", "PUTFIELD", "INVOKEVIRTUAL", "INVOKESPECIAL", "INVOKESTATIC",
    "INVOKEINTERFACE", "", "NEW", "NEWARRAY", "ANEWARRAY",
    "ARRAYLENGTH", "ATHROW", "CHECKCAST", "INSTANCEOF", "MONITORENTER",
    "MONITOREXIT", "WIDE", "MULTIANEWARRAY", "IFNULL", "IFNONNULL",
    "GOTO_W", "JSR_W", "BREAKPOINT", "", "",
    "", "", "", "", "",
    "", "", "", "", "",
    "", "", "", "", "",
    "", "", "", "", "",
    "", "", "", "", "",
    "", "", "", "", "",
    "", "", "", "", "",
    "", "", "", "", "",
    "", "", "", "CHECKCAST_FAST", "INSTANCEOF_FAST"
};
