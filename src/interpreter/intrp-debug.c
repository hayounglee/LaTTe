/* intrp-debug.c
   Undocumented support routines for debugging bytecode interpreter.
   
   Written by: Yoo C. Chung <chungyc@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Laboratory, Seoul National University.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

/* The functions defined in this file are as well documented as the
   research on the word "sdljkfhslu" (i.e. not documented at all).
   Use at your own risk.

   Some functions work only on SPARC Solaris. */

#include <stdio.h>
#include "config.h"
#include "classMethod.h"

const char *op_names[] = {
    "NOP", "ACONST_NULL", "ICONST_M1", "ICONST_0", "ICONST_1",
    "ICONST_2", "ICONST_3", "ICONST_4", "ICONST_5", "LCONST_0",
    "LCONST_1", "FCONST_0", "FCONST_1", "FCONST_2", "DCONST_0",
    "DCONST_1", "BIPUSH", "SIPUSH", "LDC", "LDC_W",
    "LDC2_W", "ILOAD", "LLOAD", "FLOAD", "DLOAD",
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
    "I2B", "I2C", "I2S", "LCMP", "FCMPL",
    "FCMPG", "DCMPL", "DCMPG", "IFEQ", "IFNE",
    "IFLT", "IFGE", "IFGT", "IFLE", "IF_ICMPEQ",
    "IF_ICMPNE", "IF_ICMPLT", "IF_ICMPGE", "IF_ICMPGT", "IF_ICMPLE",
    "IF_ACMPEQ", "IF_ACMPNE", "GOTO", "JSR", "RET",
    "TABLESWITCH", "LOOKUPSWITCH", "IRETURN", "LRETURN", "FRETURN",
    "DRETURN", "ARETURN", "RETURN", "GETSTATIC", "PUTSTATIC",
    "GETFIELD", "PUTFIELD", "INVOKEVIRTUAL", "INVOKESPECIAL", "INVOKESTATIC",
    "INVOKEINTERFACE", "", "NEW", "NEWARRAY", "ANEWARRAY",
    "ARRAYLENGTH", "ATHROW", "CHECKCAST", "INSTANCEOF", "MONITORENTER",
    "MONITOREXIT", "WIDE", "MULTINEWARRAY", "IFNULL", "IFNONNULL",
    "GOTO_W", "JSR_W", "", "", "",
    "", "", "", "", "",
    "", "WIDE ILOAD", "WIDE LLOAD", "WIDE FLOAD", "WIDE DLOAD",
    "WIDE ALOAD", "WIDE ISTORE", "WIDE LSTORE", "WIDE FSTORE", "WIDE DSTORE",
    "WIDE ASTORE", "WIDE RET", "WIDE IINC", "GETFIELD (w)", "GETFIELD (s)",
    "GETFIELD (b)", "GETFIELD (c)", "GETFIELD (d)", "GETSTATIC (w)",
    "GETSTATIC (s)",
    "GETSTATIC (b)", "GETSTATIC (c)", "GETSTATIC (d)", "PUTFIELD (w)",
    "PUTFIELD (s)",
    "PUTFIELD (b)", "PUTFIELD (c)", "PUTFIELD (d)", "PUTSTATIC (w)",
    "PUTSTATIC (s)",
    "PUTSTATIC (b)", "PUTSTATIC (c)", "PUTSTATIC (d)"
};

void
intrp_print_vars (Method *m, unsigned *locals)
{
    int i;

    printf("locals (%p):", locals);
    for (i = 0; i < m->localsz; i++)
	printf(" 0x%x", locals[-i]);
    printf("\n");
}

void
intrp_print_stack (unsigned *fp, unsigned *top)
{
    unsigned *cursor;

    printf("stack (%p):", top);
    for (cursor = fp-1; cursor >= top; cursor--)
	printf(" 0x%x", *cursor);
    printf("\n");
}

void
intrp_print_int (int n)
{
    printf("int: %d\n", n);
}

void
intrp_print_bpc (Method *m, void *pc)
{
    printf("method: %s.%s (%s)\n",
	   m->class->name->data, m->name->data, m->signature->data);
    printf("BPC: %d\n", (char*)pc-(char*)m->bcode);
}

void
intrp_print_call (Method *m)
{
    printf("interpreter called for %s.%s (%s)\n",
	   m->class->name->data, m->name->data, m->signature->data);
}

void
intrp_print_return (Method *m)
{
    printf("interpreter returning from %s.%s (%s)\n",
	   m->class->name->data, m->name->data, m->signature->data);
}

void
intrp_print_native (Method *m)
{
    printf("calling native method %s.%s (%s)\n",
	   m->class->name->data, m->name->data, m->signature->data);
}

void
intrp_print_opcode (int n)
{
    printf("OP: %3d (%s)\n", n, op_names[n]);
}

void
intrp_print_chain (void *FP, Method *m, unsigned char *pc,
		   int *locals, int *top)
{
    int i, *cursor;

    printf("printing interpreter call chain:\n");
    while (m != NULL) {
	printf("  method %s.%s (%s)\n",
	       m->class->name->data, m->name->data, m->signature->data);

	printf("    BPC: %d, opcode: %s\n",
	       pc - (unsigned char*)m->bcode, op_names[*pc]);

	printf("    locals:");
	for (i = 0; i < m->localsz; i++)
	    printf(" 0x%x", locals[-i]);
	printf("\n");

	printf("    stack:");
	for (cursor = (int*)FP-1; cursor >= top; cursor--)
	    printf(" 0x%x", *cursor);
	printf("\n");

	m = *(Method**)((char*)FP+16);
	pc = *(char**)((char*)FP+12);
	locals = *(int**)((char*)FP+4);
	top = *(int**)((char*)FP+8);
	FP = *(void**)((char*)FP+24);
    }
}

void _interpret_start (void);

void
intrp_print_current_opcode (void *pc)
{
    int op;

    op = ((char*)pc-(char*)_interpret_start)/256;
    printf("current opcode = %d (%s)\n", op, op_names[op]);
}
