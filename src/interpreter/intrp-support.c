/* intrp-support.c
   Support routines for bytecode interpreter.
   
   Written by: Yoo C. Chung <chungyc@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Laboratory, Seoul National University.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#include <assert.h>
#include <stdlib.h>
#include "config.h"
#include "config-std.h"
#include "gtypes.h"
#include "md.h"
#include "classMethod.h"
#include "exception.h"
#include "errors.h"
#include "../translator/SPARC_instr.h"
#include "../translator/reg.h"

#ifdef TRANSLATOR
#include "translator_driver.h"
#include "bytecode.h"
#endif /* TRANSLATOR */

#define ROUNDUPTOPADDING(p)	((void*)(((unsigned)(p) + (4-1)) & ~3))

static void* generate_interpreter_caller (short ins, char rettype);
static void* generate_native_caller (short ins, char rettype);

int intrp_tableswitch (int value, int *table);
int intrp_lookupswitch (int value, int *table);
int intrp_native_width (Method *m);
void* intrp_multianewarray (Hjava_lang_Class*, int, int*);

int intrp_interpretp (Method *m);
void* intrp_translate_method (Method* m);
void* intrp_interpreter_caller (Method *m);
void* intrp_native_caller (Method *m);


/* Name        : intrp_tableswitch
   Description : Find appropriate offset from tableswitch instruction.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     Returns offset to jump to. */
int
intrp_tableswitch (int value, int *table)
{
    int lo, hi;
    int *list;

    /* Get address of start of table. */
    list = ROUNDUPTOPADDING((char*)table+1);

    lo = list[1];
    hi = list[2];

    if (value >= lo && value <= hi)
	return list[value-lo+3];
    else
	return list[0];
}

/* Name        : intrp_lookupswitch
   Description : Find appropriate offset from lookupswitch instruction.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     Returns offset to jump to. */
int
intrp_lookupswitch (int value, int *table)
{
    int i, n;
    int *list;

    /* Get address of start of table. */
    list = ROUNDUPTOPADDING((char*)table+1);

    n = list[1]; /* Get number of pairs. */

    /* Find matching pair. */
    for (i = 0; i < n; i++)
	if (list[2+2*i] == value)
	    return list[3+2*i];

    /* If there were no matching pairs. */
    return list[0];
}

/* Name        : intrp_multianewarray
   Description : Allocate multidimensional array for interpreter.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     SIZES points to an array of integers holding the size of the
     array for each dimension in reverse order. */
void*
intrp_multianewarray (Hjava_lang_Class *class, int ndim, int *sizes)
{
    int i;
    int dims[ndim+1];

    /* Put the dimension sizes into proper order in DIMS. */
    for (i = 0; i < ndim; i++) {
	dims[i] = sizes[ndim-i-1];
	if (dims[i] < 0)
	    throwException(NegativeArraySizeException);
    }

    dims[ndim] = -1; /* Set end of DIMS. */

    return newMultiArray(class, dims);
}

/* Name        : intrp_interpretp
   Description : Determine if method should be interpreted.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     The method must not have previously been interpreted.
   Notes:
     This function tries to decide if it would be better to initially
     interpret a method or to translate it from the get-go.  Those
     familiar with Lisp should recognize the origin of the name of the
     function. */
int
intrp_interpretp (Method *m)
{
#ifdef TRANSLATOR
    int i, length;
    signed char *bytecode;
#endif /* TRANSLATOR */

    /* Native methods obviously cannot be interpreted. */
    if (Method_IsNative(m))
	return 0;

#ifdef TRANSLATOR
    /* Try to detect loops, and forget about interpreting if there are
       any.  We don't accurately detect loops, but just assume that
       any back-edges results in a loop. */
    length = m->bcodelen;
    bytecode = m->bcode;

    for (i = 0; i < length;) {
	int *list;
	unsigned char opcode;

	assert(i >= 0);
	assert(i < m->bcodelen);

	/* Search for branches that might go backwards. */
	opcode = bytecode[i];
	switch (opcode) {
	  case IFEQ:	case IFNE:	case IFLT:
	  case IFGE:	case IFGT:	case IFLE:
	  case IF_ICMPEQ:	case IF_ICMPNE:
	  case IF_ICMPLT:	case IF_ICMPGE:
	  case IF_ICMPGT:	case IF_ICMPLE:
	  case IF_ACMPEQ:	case IF_ACMPNE:
	  case IFNULL:	case IFNONNULL:	case GOTO:
	    if ((short)(bytecode[i+1] << 8 | bytecode[i+2]) < 0)
		return 0; /* Found a back-edge. */

	    /* Skip to next bytecode. */
	    i += 3;
	    break;

	  case GOTO_W:
	    if ((signed char)bytecode[i+1] < 0)
		return 0; /* Found a back-edge. */

	    /* Skip to next bytecode. */
	    i += 5;
	    break;

	  case TABLESWITCH:
	    /* Simply ignore branches from TABLESWITCH for now.
	       Skip to next bytecode. */
	    list = ROUNDUPTOPADDING((char*)(bytecode+i)+1);
	    i = ((i+4) & ~3) + (list[2]-list[1]+4)*4;
	    break;

	  case LOOKUPSWITCH:
	    /* Simply ignore branches from LOOKUPSWITCH for now.
	       Skip to next bytecode. */
	    list = ROUNDUPTOPADDING((char*)(bytecode+i)+1);
	    i = ((i+4) & ~3) + (list[1]+1)*8;
	    break;

	  case WIDE: case ILOAD_W ... IINC_W:
	    /* WIDE instructions have different offsets from the default. */
	    if ((unsigned char)bytecode[i+1] == IINC)
		i += 5;
	    else
		i += 3;
	    break;

	  default:
	    /* Skip to next bytecode. */
	    i = i + BCode_get_opcode_len(opcode);
	    break;
	}
    }
#endif /* TRANSLATOR */

    return 1; /* Interpret by default. */
}

#ifdef TRANSLATOR
/* Name        : intrp_translate_method
   Description : Fix method so that translated version is used next time.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Post-Condition:
     Returns pointer to translated code. */
void*
intrp_translate_method (Method *m)
{
    methodTrampoline *tramp;

    tramp = (methodTrampoline*)METHOD_NATIVECODE(m);

    /* Replace trampoline only if it's an interpreter trampoline. */
    if (tramp->code[0] == 0x8410000f) {

#ifdef CUSTOMIZATION
	FILL_IN_JIT_TRAMPOLINE(tramp, m, dispatch_method_with_object);
#else /* not CUSTOMIZATION */
	FILL_IN_JIT_TRAMPOLINE(tramp, m, sparc_do_fixup_trampoline);
#endif /* not CUSTOMIZATION */

	FLUSH_DCACHE(tramp, &tramp->meth);
    }

    return (void*)tramp;
}
#endif /* TRANSLATOR */

/* Name        : intrp_interpreter_caller
   Description : Return code which calls interpreter from translated code.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     The main thing the returned code does is to match the calling
     conventions between translated code and interpreted code.  This
     code may be shared between many instances. */
void*
intrp_interpreter_caller (Method *m)
{
    /* The number of Java arguments are guaranteed to be less than 256. */
    static void **int_callers[256]; /* For methods returning single word. */
    static void **flt_callers[256]; /* For methods returning float. */
    static void **lng_callers[256]; /* For methods returning double word. */
    static void **dbl_callers[256]; /* For methods returning double float. */
    static void **vod_callers[256]; /* For methods with no return value. */

    short in, out;
    char rettype;

    countInsAndOuts(m->signature->data, &in, &out, &rettype);

    if (!Method_IsStatic(m))
	in += 1;	/* Include object pointer for non-static method. */

    /* Return appropriate caller if it has already been generated.
       Otherwise generate it and return the new caller. */
    switch (rettype) {
      case 'J':
        if (lng_callers[in] == NULL)
	    lng_callers[in] = generate_interpreter_caller(in, rettype);
	return lng_callers[in];

      case 'F':
        if (flt_callers[in] == NULL)
	    flt_callers[in] = generate_interpreter_caller(in, rettype);
	return flt_callers[in];

      case 'D':
	if (dbl_callers[in] == NULL)
	    dbl_callers[in] = generate_interpreter_caller(in, rettype);
	return dbl_callers[in];

      case 'V':
	if (vod_callers[in] == NULL)
	    vod_callers[in] = generate_interpreter_caller(in, rettype);
	return vod_callers[in];

      default:
	/* The rest should be single word return values. */
	if (int_callers[in] == NULL)
	    int_callers[in] = generate_interpreter_caller(in, rettype);
	return int_callers[in];
    }
}

/* Name        : generate_interpreter_caller
   Description : Generates interpreter caller.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     This functions creates code that calls the interpreter from
     translated code.

     The code looks like this:

	save %sp, -80, %sp

	ld [ %i7 + 8 ], %o0			! load method pointer

	! load top of interpreter stack
	sethi %hi(jstack_point), %l3
	or %l3, %lo(jstack_point), %l3
	ld [ %l3 ], %l5

	! reserve space for local variables on interpreter stack
	lduh [ %o0 + offsetof(Method, localsz) ], %l2
	sll %l2, 2, %l2
	! add extra 8 bytes to accomodate potential return value
	add %l2, 12, %l2
	sub %l5, %l2, %l4
	st %l4, [ %l3 ]				! store stack top

	! store arguments up to the the sixth one
	! local variables are stored starting from (%l5 - 4)
	st %i0, [ %l5 - 4 ]
	st %i1, [ %l5 - 8 ]
	...

	! store extra arguments
	ld [ %fp + 92 ], %l0
	st %l0, [ %l5 - 28 ]
	ld [ %fp + 96 ], %l0
	st %l0, [ %l5 - 32 ]
	...

	mov %g2, %i7				! set return address
	add %l5, -4, %o1			! set locals pointer

	! call interpreter
	call interpret
	ld [ %o0 + \&OFFSET_BCODE ], %o2	! delay slot

	! reset interpreter stack top
	st %l5, [ %l3 ]

	! set return value
	ld [ %l5 - 4 ], (%i0|%f0)
	ld [ %l5 - 8 ], (%i1|%f1)		! only for long values

	ret
	restore
*/
static void*
generate_interpreter_caller (short ins, char rettype)
{
    int i;
    int code_size;	/* Size of caller code. */
    int save_space;	/* Size of stack frame. */
    unsigned *code;	/* The code. */
    unsigned *cursor;	/* Position within code block. */

    extern uint8 *jstack_point;
    extern void intrp_execute (Method *m, void *args, void *bytecode);

    save_space = -128;	/* Set size of stack frame. */
    code_size = 18*4;	/* Size of code that always goes into caller. */

    /* Set size of code which stores arguments. */
    if (ins <= 6)
	code_size += ins*4;
    else
	code_size += 6*4 + 8*(ins-6);

    /* Set size of code storing return value. */
    if (rettype == 'V')
	code_size += 0;
    else if (rettype == 'J' || rettype == 'D')
	code_size += 8;
    else
	code_size += 4;

    /* Allocate memory to put code in. */
    code = gc_malloc_fixed(code_size);

    cursor = code;

    *cursor++ =
	SPARC_SAVE | (sp << 25) | (sp << 14)
	| (1 << 13) | (0x1fff & save_space);

    *cursor++ = LD | (o0 << 25) | (i7 << 14) | (1 << 13) | 8;

    *cursor++ = SETHI | (l3 << 25) | HI(&jstack_point);
    *cursor++ = OR | (l3 << 25) | (l3 << 14) | (1 << 13) | LO(&jstack_point);
    *cursor++ = LD | (l5 << 25) | (l3 << 14) | (1 << 13);

    *cursor++ =
	LDUH | (l2 << 25) | (o0 << 14) | (1 << 13) | offsetof(Method, localsz);
    *cursor++ =	SLL | (l2 << 25) | (l2 << 14) | (1 << 13) | 2;
    *cursor++ = ADD | (l2 << 25) | (l2 << 14) | (1 << 13) | 12;
    *cursor++ = SUB | (l4 << 25) | (l5 << 14) | l2;
    *cursor++ = ST | (l4 << 25) | (l3 << 14) | (1 << 13);

    for (i = 0; i < ins && i < 6; i++)
	*cursor++ =
	    ST | ((i0+i) << 25) | (l5 << 14) | (1 << 13) | ((-i*4-4) & 0x1fff);

    for (i = 6; i < ins; i++) {
	*cursor++ =
	    LD | (l0 << 25) | (fp << 14) | (1 << 13) | (92+(i-6)*4);
	*cursor++ =
	    ST | (l0 << 25) | (l5 << 14) | (1 << 13) | ((-i*4-4) & 0x1fff);
    }

    *cursor++ = OR | (i7 << 25) | (g2 << 14) | (1 << 13);
    *cursor++ = ADD | (o1 << 25) | (l5 << 14) | (1 << 13) | (-4 & 0x1fff);

    *cursor = CALL | ((((int)intrp_execute - (int)cursor) / 4) & 0x3FFFFFFF);
    cursor++;
    *cursor++ =
	LD | (o2 << 25) | (o0 << 14) | (1 << 13) | (offsetof(Method, bcode));

    *cursor++ = ST | (l5 << 25) | (l3 << 14) | (1 << 13);

    if (rettype == 'V') {
	/* Do nothing. */
    } else if (rettype == 'F')
	*cursor++ = LDF | (f0 << 25) | (l5 << 14) | (1 << 13) | (-4 & 0x1fff);
    else if (rettype == 'J') {
	*cursor++ = LD | (i0 << 25) | (l5 << 14) | (1 << 13) | (-4 & 0x1fff);
	*cursor++ = LD | (i1 << 25) | (l5 << 14) | (1 << 13) | (-8 & 0x1fff);
    } else if (rettype == 'D') {
	*cursor++ = LDF | (f0 << 25) | (l5 << 14) | (1 << 13) | (-4 & 0x1fff);
	*cursor++ = LDF | (f1 << 25) | (l5 << 14) | (1 << 13) | (-8 & 0x1fff);
    } else
	*cursor++ = LD | (i0 << 25) | (l5 << 14) | (1 << 13) | (-4 & 0x1fff);

    *cursor++ = JMPL | (g0 << 25) | (i7 << 14) | (1 << 13) | 8;
    *cursor++ = RESTORE;

    FLUSH_DCACHE(code, cursor);

    assert((char*)cursor - (char*)code <= code_size);

    return code;
}

/* Name        : intrp_native_caller
   Description : Return code which calls native code from interpreter.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     The code returned matches the calling conventions of the
     interpreter with that of the native code.  The code may be shared
     among many methods. */
void*
intrp_native_caller (Method *m)
{
    /* The number of Java arguments are guaranteed to be less than 256. */
    static void **int_callers[256]; /* For methods returning single word. */
    static void **flt_callers[256]; /* For methods returning float. */
    static void **lng_callers[256]; /* For methods returning double word. */
    static void **dbl_callers[256]; /* For methods returning double float. */
    static void **vod_callers[256]; /* For methods with no return value. */

    short in, out;
    char rettype;

    countInsAndOuts(m->signature->data, &in, &out, &rettype);

    if (!Method_IsStatic(m))
	in += 1;	/* Include object pointer for non-static method. */

    /* Return appropriate caller if it has already been generated.
       Otherwise generate it and return the new caller. */
    switch (rettype) {
      case 'J':
        if (lng_callers[in] == NULL)
	    lng_callers[in] = generate_native_caller(in, rettype);
	return lng_callers[in];

      case 'F':
	if (flt_callers[in] == NULL)
	    flt_callers[in] = generate_native_caller(in, rettype);
	return flt_callers[in];

      case 'D':
	if (dbl_callers[in] == NULL)
	    dbl_callers[in] = generate_native_caller(in, rettype);
	return dbl_callers[in];

      case 'V':
	if (vod_callers[in] == NULL)
	    vod_callers[in] = generate_native_caller(in, rettype);
	return vod_callers[in];

      default:
	/* The rest should be single word return values. */
	if (int_callers[in] == NULL)
	    int_callers[in] = generate_native_caller(in, rettype);
	return int_callers[in];
    }
}

/* Name        : generate_native_caller
   Description : Generates native code caller.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:
     This creates code that is used by the interpreter to call native
     code.

     The code looks like this:

     	! %i0 should contain the method pointer
	! %i1 should contain the native code address
	! %i2 should contain address of first argument

	save %sp, -??, %sp

	! Pop arguments to registers
	ld [ %i2 ], %o0
	ld [ %i2 - 4 ], %o1
	...

	! Push other arguments onto stack frame
	ld [ %i2 - 24 ], %l0
	st %l0, [ %sp + 92 ]
	ld [ %i2 - 28 ], %l0
	st %l0, [ %sp + 96 ]
	...

	call %i1			! call native code
	nop

	! Store return value
	st (%f0|%o0), [ %i2 ]
	st (%f1|%o1), [ %i2 - 4 ]	! only for long value

	mov ??, %i0			! returns width of return value

	ret
	restore
*/
static void*
generate_native_caller (short ins, char rettype)
{
    int i;
    int code_size;	/* Size of caller code. */
    int save_space;	/* Size of stack frame. */
    unsigned *code;	/* The code. */
    unsigned *cursor;	/* Position within code block. */

    save_space = -128 - (ins/4+1)*16;	/* Set size of stack frame. */

    code_size = 6*4; /* Size of code that always goes into caller. */

    /* Set size of code which stores arguments. */
    if (ins <= 6)
	code_size += ins*4;
    else
	code_size += 6*4 + 8*(ins-6);

    /* Set size of code storing return value. */
    if (rettype == 'V')
	code_size += 0;
    else if (rettype == 'J' || rettype == 'D')
	code_size += 8;
    else
	code_size += 4;

    /* Allocate memory to put code in. */
    code = gc_malloc_fixed(code_size);

    cursor = code;

    *cursor++ =
	SPARC_SAVE | (sp << 25) | (sp << 14)
	| (1 << 13) | (0x1fff & save_space);

    for (i = 0; i < ins && i < 6; i++)
	*cursor++ =
	    LD | ((o0+i) << 25) | (i2 << 14) | (1 << 13) | ((-i*4) & 0x1fff);

    for (i = 6; i < ins; i++) {
	*cursor++ =
	    LD | (l0 << 25) | (i2 << 14) | (1 << 13) | ((-i*4) & 0x1fff);
	*cursor++ =
	    ST | (l0 << 25) | (sp << 14) | (1 << 13) | (92 + (i-6)*4);
    }

    *cursor++ = JMPL | (o7 << 25) | (i1 << 14) | (1 << 13);
    *cursor++ = SPARC_NOP;

    if (rettype == 'V')
	*cursor++ = OR | (i0 << 25) | (g0 << 14) | (1 << 13) | 0;
    else if (rettype == 'F') {
	*cursor++ = STF | (f0 << 25) | (i2 << 14) | (1 << 13);
	*cursor++ = OR | (i0 << 25) | (g0 << 14) | (1 << 13) | 4;
    } else if (rettype == 'J') {
	*cursor++ = ST | (o0 << 25) | (i2 << 14) | (1 << 13);
	*cursor++ = ST | (o1 << 25) | (i2 << 14) | (1 << 13) | (-4 & 0x1fff);
	*cursor++ = OR | (i0 << 25) | (g0 << 14) | (1 << 13) | 8;
    } else if (rettype == 'D') {
	*cursor++ = STF | (f0 << 25) | (i2 << 14) | (1 << 13);
	*cursor++ = STF | (f1 << 25) | (i2 << 14) | (1 << 13) | (-4 & 0x1fff);
	*cursor++ = OR | (i0 << 25) | (g0 << 14) | (1 << 13) | 8;
    } else {
	*cursor++ = ST | (o0 << 25) | (i2 << 14) | (1 << 13);
	*cursor++ = OR | (i0 << 25) | (g0 << 14) | (1 << 13) | 4;
    }

    *cursor++ = JMPL | (g0 << 25) | (i7 << 14) | (1 << 13) | 8;
    *cursor++ = RESTORE;

    FLUSH_DCACHE(code, cursor);

    assert((char*)cursor - (char*)code <= code_size);

    return code;
}

#ifndef NDEBUG
/* Name        : intrp_dummy
   Description : Used to link in debugging support for interpreter.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     The only thing this does is to make a reference to a debugging
     function for the interpreter so that the debugging support for
     the interpreter gets linked. */
void
intrp_dummy (void)
{
    void intrp_print_int (int);

    intrp_print_int(0);
}
#endif /* not NDEBUG */
