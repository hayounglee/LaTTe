/* interpreter.S
   Bytecode interpreter for the LaTTe Java virtual machine.

   Written by Yoo C. Chung <chungyc@altair.snu.ac.kr>

   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

/* Only the SPARC is supported by this interpreter. */

	.radix d

	! Global definitions
	.global intrp_execute
	.global _interpret_start
	.global _interpret_end

	.include "offsets.S"

DISP	.EQU	256	! Maximum size of each opcode implementation
SDISP	.EQU	8	! log_2 DISP

	!! Register definitions
METHOD	.REG	(%i0)	! Method structure
ORIGIN	.REG	(%i1)	! Beginning of opcode implementations
PC	.REG	(%i2)	! Address containing the current bytecode
TOP	.REG	(%i3)	! Operand stack top
LOCALS	.REG	(%i4)	! Local variables
TARGET	.REG	(%i5)	! Next opcode to execute
FP	.REG	(%l0)	! Java stack frame pointer
POOL	.REG	(%l1)	! Resolved pool
FAKEI	.REG	(%l2)	! Instruction which trampolines start with

	!! Declare that an opcode is being defined
	.macro DECLARE opcode
	\(\(.org)) _interpret_start + \opcode * DISP
	.endm

	!! Load local variable into register
	.macro LOAD index, register
	ld [ LOCALS - (\index * 4) ], \register
	.endm

	!! Store local variable from register
	.macro STORE index, register
	st \register, [ LOCALS - (\index * 4)]
	.endm

	!! Load local variable into register
	.macro LOAD_W index, high, low
	ld [ LOCALS - (\index * 4)], \high
	ld [ LOCALS - ((\index * 4) + 4)], \low
	.endm

	!! Store local variable from register
	.macro STORE_W index, high, low
	st \high, [ LOCALS - (\index * 4)]
	st \low, [ LOCALS - ((\index * 4) + 4)]
	.endm

	!! Load local variable into register, with index in a register
	.macro RLOAD index, register
	sll \index, 2, \index
	neg \index
	ld [ LOCALS + \index ], \register
	.endm

	!! Store local variable from register, with index in a register
	.macro RSTORE index, register
	sll \index, 2, \index
	neg \index
	st \register, [ LOCALS + \index ]
	.endm

	!! Load local variable into register, with index in a register
	.macro RLOAD_W index, high, low
	sll \index, 2, \index
	neg \index
	ld [ LOCALS + \index ], \high
	add \index, -4, \index
	ld [ LOCALS + \index ], \low
	.endm

	!! Store local variable from register, with index in a register
	.macro RSTORE_W index, high, low
	sll \index, 2, \index
	neg \index
	st \high, [ LOCALS + \index ]
	add \index, -4, \index
	st \low, [ LOCALS + \index ]
	.endm

	!! Load next bytecode to execute
	.macro LOAD_NEXT offset=1
	ldub [ PC + \offset ], TARGET
	.endm

	!! Jump to next bytecode
	.macro NEXT offset=1
	sll TARGET, SDISP, TARGET
	jmp ORIGIN + TARGET
	.aif \offset ne 0
	add PC, \offset, PC	! delay slot
	.aelse
	nop
	.aendi
	.endm

	!! Push a word onto the operand stack
	.macro PUSH reg
	st \reg, [ TOP - 4 ]
	add TOP, -4, TOP
	.endm

	!! Push double word onto the operand stack
	.macro PUSH_W high, low
	st \low, [ TOP - 8 ]
	st \high, [ TOP - 4 ]
	add TOP, -8, TOP
	.endm

	!! Get index adjacent to bytecode
	.macro INDEX reg, offset=1
	ldub [ PC + \offset ], \reg
	.endm

	!! Get wide index adjacent to bytecode
	.macro INDEX_W reg, temp, offset=1
	ldub [ PC + \offset ], \reg
	ldub [ PC + (\offset + 1) ], \temp
	sll \reg, 8, \reg
	or \reg, \temp, \reg
	.endm

	!! Extra actions taken at branch
	.macro BRANCH_ACTION
	! Do nothing for now
	.endm

	!! Branch by offset
	.macro BRANCH_OFFSET offset=1
	INDEX_W %o1, %o2, \offset
	sll %o1, 16, %o1	! sign extend index
	sra %o1, 16, %o1
	BRANCH_ACTION
	ldub [ PC + %o1 ], TARGET
	sll TARGET, SDISP, TARGET
	jmp ORIGIN + TARGET
	add PC, %o1, PC		! delay slot
	.endm

	!! Call function
	!! Must be used if the callee might invoke the interpreter.
	!! or if it possible for threads to switch within the call
	.macro CALL_FUNC func
	sethi %hi(jstack_point), %l3
	call \func
	st TOP, [ %l3 + %lo(jstack_point) ]
	.endm

	.macro GET_RESOLVED index, address
	sll \index, 2, \address
	add POOL, \address, \address
	.endm

	!! Resolve constant pool reference
	!! Resolved item goes into %o0
	.macro RESOLVE_POOL func
	INDEX_W %o1, %o0	! load constant pool index
	GET_RESOLVED %o1, %o2
	ld [ %o2 ], %o0		! get resolved item
	cmp %o0, %g0		! check whether it is indeed resolved
	bne 1f
	nop
	! resolve constant pool item
	mov METHOD, %o0
	CALL_FUNC \func
1:	! back to normal flow
	.endm

	!! Restore caller registers
	.macro RESTORE_CALLER
	ld [ FP + 4 ], LOCALS
	ld [ FP + 8 ], TOP
	ld [ FP + 28 ], PC	! load target address
	ld [ FP + 16 ], METHOD
	ld [ FP + 20 ], POOL
	.endm

	!! Return from method
	.macro METHOD_RETURN
	cmp METHOD, %g0
	be 1f
	ld [ FP + 24 ], FP	! delay slot

	! return to interpreted code
	LOAD_NEXT 0
	NEXT 0

1:	! return to native code
	sethi %hi(jstack_point), %o1
	st FP, [ %o1 + %lo(jstack_point) ]
	ret
	restore
	.endm

	!! Maintain method run count
	.macro METH_COUNT
	.aif \&ONLY_INTERPRETER ne 1
	! Load method run count
	ld [ METHOD + \&OFFSET_MCOUNT ], %l3
	cmp %l3, 0
	bne,a 2f
	sub %l3, 1, %l3		! delay slot

	! Translate bytecode to native code
	mov METHOD, %o0
	CALL_FUNC intrp_translate_method

	! Save stack top
	sethi %hi(jstack_point), %l3
	st TOP, [ %l3 + %lo(jstack_point) ]

	! Restore caller registers
	mov METHOD, %l4
	RESTORE_CALLER

	! Call translated code
	ld [ %l4 + \&OFFSET_NCALLER ], %l5
	mov %o0, %o1		! set native code address argument
	mov %l4, %o0		! set method argument
	sub TOP, 4, %o2		! set local variables address argument
	call %l5
	nop
	sub TOP, %o0, TOP

	METHOD_RETURN

2:	! Update method run count
	st %l3, [ METHOD + \&OFFSET_MCOUNT ]
	.aendi
	.endm


	/* Interpret a given method.
	   The equivalent C function prototype would be:

	   void interpret (Method *m, void *args, void* bcode);

	   where,
	     m		- The method to be interpreted
	     args	- Memory containing arguments 
	                  The return value also goes in here.
	     bcode	- Bytecode address to execute */
intrp_execute:
	save %sp, -96, %sp

	! copy arguments to appropriate registers
	! METHOD and PC are already set
	mov %i1, LOCALS
	ld [ METHOD + \&OFFSET_POOL ], POOL
	sethi %hi(_interpret_start), ORIGIN
	or ORIGIN, %lo(_interpret_start), ORIGIN
	sethi %hi(h'8410000f), FAKEI
	or FAKEI, %lo(h'8410000f), FAKEI

	! Set up fake caller Java stack frame
	add LOCALS, 4, %o2
	sethi %hi(jstack_point), %o0
	ld [ %o0 + %lo(jstack_point) ], %o1
	add %o1, -48, FP
	st %g0, [ FP + 4 ]
	st %o2, [ FP + 8 ]	! fake stack for return value
	st %g0, [ FP + 16 ]
	st %o1, [ FP + 24 ]	! save previous stack top
	mov FP, TOP

	METH_COUNT

	! If method is synchronized, lock object or class
	lduh [ METHOD + \&OFFSET_FLAGS ], %o1	! load flags
	and %o1, h'20, %o2
	cmp %o2, %g0		! check if synchronized
	be 2f
	and %o1, h'8, %o2
	cmp %o2, %g0		! check if instance method
	bne,a 1f
	ld [ METHOD + \&OFFSET_CLASS ], %o0	! load class (delay slot)
	LOAD 0, %o0		! load object
1:	! synchronize
	CALL_FUNC _lockMutex

2:	! Start excuting bytecodes
	LOAD_NEXT 0
	NEXT 0

	! Opcode implementations exist after this point
_interpret_start:

	DECLARE 0	! NOP
	LOAD_NEXT
	NEXT

	DECLARE 1	! ACONST_NULL
	LOAD_NEXT
	PUSH %g0
	NEXT

	!! Push small integer onto operand stack
	.macro PUSH_INTCONST n
	LOAD_NEXT
	.aif \n eq 0
	st %g0, [ TOP - 4]
	.aelse
	.aif \n lt 0
	sethi %hi(-1), %o0
	or %o0, %lo(-1), %o0
	.aelse
	mov \n, %o0
	.aendi
	st %o0, [ TOP - 4 ]
	.aendi
	add TOP, -4, TOP
	NEXT
	.endm

	DECLARE 2	! ICONST_M1
	PUSH_INTCONST -1

	DECLARE 3	! ICONST_0
	PUSH_INTCONST 0

	DECLARE 4	! ICONST_1
	PUSH_INTCONST 1

	DECLARE 5	! ICONST_2
	PUSH_INTCONST 2

	DECLARE 6	! ICONST_3
	PUSH_INTCONST 3

	DECLARE 7	! ICONST_4
	PUSH_INTCONST 4

	DECLARE 8	! ICONST_5
	PUSH_INTCONST 5

	!! Push a small long integer onto operand stack
	.macro PUSH_LONGCONST n
	LOAD_NEXT
	st %g0, [ TOP - 4 ]
	.aif \n eq 0
	st %g0, [ TOP - 8 ]
	.aelse
	mov \n, %o0
	st %o0, [ TOP - 8 ]
	.aendi
	add TOP, -8, TOP
	NEXT
	.endm

	DECLARE 9	! LCONST_0
	PUSH_LONGCONST 0

	DECLARE 10	! LCONST_1
	PUSH_LONGCONST 1

	!! Push constant float onto operand stack
	.macro PUSH_FLOATCONST n
	LOAD_NEXT
	.aif \n eq 0
	st %g0, [ TOP - 4 ]
	.aelse
	.aif \n eq 1
	sethi %hi(127 * 8388608), %o0	! 2^23 = 8388608
	.aelse
	.aif \n eq 2
	sethi %hi(128 * 8388608), %o0	! 2^23 = 8388608
	.aelse
	.err
	.aendi
	.aendi
	st %o0, [ TOP - 4 ]
	.aendi
	add TOP, -4, TOP
	NEXT
	.endm

	DECLARE 11	! FCONST_0
	PUSH_FLOATCONST 0

	DECLARE 12	! FCONST_1
	PUSH_FLOATCONST 1

	DECLARE 13	! FCONST_2
	PUSH_FLOATCONST 2

	.macro PUSH_DOUBLECONST n
	LOAD_NEXT
	st %g0, [ TOP - 8 ]
	.aif \n eq 0
	st %g0, [ TOP - 4 ]
	.aelse
	.aif \n eq 1
	sethi %hi(1023 * 1048576), %o0	! 2^20 = 1048576
	st %o0, [ TOP - 4 ]
	.aelse
	.err
	.aendi
	.aendi
	add TOP, -8, TOP
	NEXT
	.endm

	DECLARE 14	! DCONST_0
	PUSH_DOUBLECONST 0

	DECLARE 15	! DCONST_1
	PUSH_DOUBLECONST 1

	DECLARE 16	! BIPUSH
	INDEX %o0
	sll %o0, 24, %o0	! sign extend
	sra %o0, 24, %o0
	LOAD_NEXT 2
	PUSH %o0
	NEXT 2

	DECLARE 17	! SIPUSH
	INDEX_W %o0, %o1
	sll %o0, 16, %o0	! sign extend
	sra %o0, 16, %o0
	LOAD_NEXT 3
	PUSH %o0
	NEXT 3

	DECLARE 18	! LDC
	LOAD_NEXT 2
	INDEX %o0
	ld [ METHOD + \&OFFSET_CLASS ], %o1
	sll %o0, 2, %o0
	add %o1, \&OFFSET_CONSTANTS, %o1
	ld [ %o1 ], %o1
	ld [ %o1 + %o0 ], %o2
	PUSH %o2
	NEXT 2

	DECLARE 19	! LDC_W
	LOAD_NEXT 3
	INDEX_W %o0, %o1
	ld [ METHOD + \&OFFSET_CLASS ], %o1
	sll %o0, 2, %o0
	add %o1, \&OFFSET_CONSTANTS, %o1
	ld [ %o1 ], %o1
	ld [ %o1 + %o0 ], %o2
	PUSH %o2
	NEXT 3

	DECLARE 20	! LDC2_W
	LOAD_NEXT 3
	INDEX_W %o0, %o1
	ld [ METHOD + \&OFFSET_CLASS ], %o2
	sll %o0, 2, %o0
	add %o0, 4, %o1
	add %o2, \&OFFSET_CONSTANTS, %o2
	ld [ %o2 ], %o2
	ld [ %o2 + %o0 ], %o3
	ld [ %o2 + %o1 ], %o4
	PUSH_W %o3, %o4
	NEXT 3

	!! Load word from local variable and push onto operand stack
	.macro LOAD_WORD
	LOAD_NEXT 2
	INDEX %o0
	RLOAD %o0, %o1
	PUSH %o1
	NEXT 2
	.endm

	!! Load double word from local variable and push onto operand stack
	.macro LOAD_DWORD
	LOAD_NEXT 2
	INDEX %o0
	RLOAD_W %o0, %o1, %o2
	PUSH_W %o1, %o2
	NEXT 2
	.endm

	DECLARE 21	! ILOAD
	LOAD_WORD

	DECLARE 22	! LLOAD
	LOAD_DWORD

	DECLARE 23	! FLOAD
	LOAD_WORD

	DECLARE 24	! DLOAD
	LOAD_DWORD

	DECLARE 25	! ALOAD
	LOAD_WORD

	!! Load word from fixed local and push onto operand stack
	.macro LOAD_WORD_INDEXED n
	LOAD_NEXT
	LOAD \n, %o0
	PUSH %o0
	NEXT
	.endm

	!! Load double word from fixed local and push onto operand stack
	.macro LOAD_DWORD_INDEXED n
	LOAD_NEXT
	LOAD_W \n, %o1, %o2
	PUSH_W %o1, %o2
	NEXT
	.endm

	DECLARE 26	! ILOAD_0
	LOAD_WORD_INDEXED 0

	DECLARE 27	! ILOAD_1
	LOAD_WORD_INDEXED 1

	DECLARE 28	! ILOAD_2
	LOAD_WORD_INDEXED 2

	DECLARE 29	! ILOAD_3
	LOAD_WORD_INDEXED 3

	DECLARE 30	! LLOAD_0
	LOAD_DWORD_INDEXED 0

	DECLARE 31	! LLOAD_1
	LOAD_DWORD_INDEXED 1

	DECLARE 32	! LLOAD_2
	LOAD_DWORD_INDEXED 2

	DECLARE 33	! LLOAD_3
	LOAD_DWORD_INDEXED 3

	DECLARE 34	! FLOAD_0
	LOAD_WORD_INDEXED 0

	DECLARE 35	! FLOAD_1
	LOAD_WORD_INDEXED 1

	DECLARE 36	! FLOAD_2
	LOAD_WORD_INDEXED 2

	DECLARE 37	! FLOAD_3
	LOAD_WORD_INDEXED 3

	DECLARE 38	! DLOAD_0
	LOAD_DWORD_INDEXED 0

	DECLARE 39	! DLOAD_1
	LOAD_DWORD_INDEXED 1

	DECLARE 40	! DLOAD_2
	LOAD_DWORD_INDEXED 2

	DECLARE 41	! DLOAD_3
	LOAD_DWORD_INDEXED 3

	DECLARE 42	! ALOAD_0
	LOAD_WORD_INDEXED 0

	DECLARE 43	! ALOAD_1
	LOAD_WORD_INDEXED 1

	DECLARE 44	! ALOAD_2
	LOAD_WORD_INDEXED 2

	DECLARE 45	! ALOAD_3
	LOAD_WORD_INDEXED 3

	!! Check array bounds
	.macro CHECK_ARRAY_BOUND array, index, temp
	ld [ \array + \&OFFSET_ARRAYSIZE ], \temp
	cmp \index, 0
	tl 17
	cmp \index, \temp
	tge 17
	.endm

	!! Push word within an array onto operand stack
	.macro ALOAD_WORD scale=2 instr=ld
	LOAD_NEXT
	ld [ TOP + 4 ], %o0		! get array reference
	ld [ TOP ], %o1			! get index
	CHECK_ARRAY_BOUND %o0, %o1, %o5
	.aif \scale ne 0
	sll %o1, \scale, %o1
	.aendi
	add %o1, \&OFFSET_ARRAYDATA, %o1
	\instr [ %o0 + %o1 ], %o2	! load value
	st %o2, [ TOP + 4 ]
	add TOP, 4, TOP
	NEXT
	.endm

	!! Push double word within an array onto operand stack
	.macro ALOAD_DWORD
	LOAD_NEXT
	ld [ TOP + 4 ], %o0	! get array reference
	ld [ TOP ], %o1		! get index
	CHECK_ARRAY_BOUND %o0, %o1, %o5
	sll %o1, 3, %o1
	add %o1, \&OFFSET_ARRAYDATA, %o1
	ld [ %o0 + %o1 ], %o2	! load high word
	add %o1, 4, %o1
	ld [ %o0 + %o1 ], %o3	! load low word
	st %o2, [ TOP + 4 ]
	st %o3, [ TOP ]
	NEXT
	.endm

	DECLARE 46	! IALOAD
	ALOAD_WORD

	DECLARE 47	! LALOAD
	ALOAD_DWORD

	DECLARE 48	! FALOAD
	ALOAD_WORD

	DECLARE 49	! DALOAD
	ALOAD_DWORD

	DECLARE 50	! AALOAD
	ALOAD_WORD

	DECLARE 51	! BALOAD
	ALOAD_WORD 0, ldsb

	DECLARE 52	! CALOAD
	ALOAD_WORD 1, lduh

	DECLARE 53	! SALOAD
	ALOAD_WORD 1, ldsh

	!! Store word into local variable
	.macro STORE_WORD
	LOAD_NEXT 2
	INDEX %o0
	ld [ TOP ], %o1
	RSTORE %o0, %o1
	add TOP, 4, TOP
	NEXT 2
	.endm

	!! Store double word into local variable
	.macro STORE_DWORD
	LOAD_NEXT 2
	INDEX %o0
	ld [ TOP + 4 ], %o1
	ld [ TOP ], %o2
	RSTORE_W %o0, %o1, %o2
	add TOP, 8, TOP
	NEXT 2
	.endm

	DECLARE 54	! ISTORE
	STORE_WORD

	DECLARE 55	! LSTORE
	STORE_DWORD

	DECLARE 56	! FSTORE
	STORE_WORD

	DECLARE 57	! DSTORE
	STORE_DWORD

	DECLARE 58	! ASTORE
	STORE_WORD

	!! Store word into fixed local variable
	.macro STORE_WORD_INDEXED n
	LOAD_NEXT
	ld [ TOP ], %o0
	STORE \n, %o0
	add TOP, 4, TOP
	NEXT
	.endm

	!! Store double word into fixed local variable
	.macro STORE_DWORD_INDEXED n
	LOAD_NEXT
	ld [ TOP + 4 ], %o0
	ld [ TOP ], %o1
	STORE_W \n, %o0, %o1
	add TOP, 8, TOP
	NEXT
	.endm

	DECLARE 59	! ISTORE_0
	STORE_WORD_INDEXED 0

	DECLARE 60	! ISTORE_1
	STORE_WORD_INDEXED 1

	DECLARE 61	! ISTORE_2
	STORE_WORD_INDEXED 2

	DECLARE 62	! ISTORE_3
	STORE_WORD_INDEXED 3

	DECLARE 63	! LSTORE_0
	STORE_DWORD_INDEXED 0

	DECLARE 64	! LSTORE_1
	STORE_DWORD_INDEXED 1

	DECLARE 65	! LSTORE_2
	STORE_DWORD_INDEXED 2

	DECLARE 66	! LSTORE_3
	STORE_DWORD_INDEXED 3

	DECLARE 67	! FSTORE_0
	STORE_WORD_INDEXED 0

	DECLARE 68	! FSTORE_1
	STORE_WORD_INDEXED 1

	DECLARE 69	! FSTORE_2
	STORE_WORD_INDEXED 2

	DECLARE 70	! FSTORE_3
	STORE_WORD_INDEXED 3

	DECLARE 71	! DSTORE_0
	STORE_DWORD_INDEXED 0

	DECLARE 72	! DSTORE_1
	STORE_DWORD_INDEXED 1

	DECLARE 73	! DSTORE_2
	STORE_DWORD_INDEXED 2

	DECLARE 74	! DSTORE_3
	STORE_DWORD_INDEXED 3

	DECLARE 75	! ASTORE_0
	STORE_WORD_INDEXED 0

	DECLARE 76	! ASTORE_1
	STORE_WORD_INDEXED 1

	DECLARE 77	! ASTORE_2
	STORE_WORD_INDEXED 2

	DECLARE 78	! ASTORE_3
	STORE_WORD_INDEXED 3

	!! Store word into array
	.macro ASTORE_WORD scale=2, instr=st
	LOAD_NEXT
	ld [ TOP + 8 ], %o0	! get array reference
	ld [ TOP + 4 ], %o1	! get index
	ld [ TOP ], %o2		! get value
	CHECK_ARRAY_BOUND %o0, %o1, %o5
	.aif \scale ne 0
	sll %o1, \scale, %o1
	.aendi
	add %o1, \&OFFSET_ARRAYDATA, %o1
	\instr %o2, [ %o0 + %o1 ]
	add TOP, 12, TOP
	NEXT
	.endm

	!! Store double word into array
	.macro ASTORE_DWORD
	LOAD_NEXT
	ld [ TOP + 12 ], %o0	! get array reference
	ld [ TOP + 8 ], %o1	! get index
	ld [ TOP + 4 ], %o3	! get high value
	ld [ TOP ], %o4		! get low value
	CHECK_ARRAY_BOUND %o0, %o1, %o5
	sll %o1, 3, %o1
	add %o1, \&OFFSET_ARRAYDATA, %o1
	add %o1, 4, %o2
	st %o3, [ %o0 + %o1 ]
	st %o4, [ %o0 + %o2 ]
	add TOP, 16, TOP
	NEXT
	.endm

	DECLARE 79	! IASTORE
	ASTORE_WORD

	DECLARE 80	! LASTORE
	ASTORE_DWORD

	DECLARE 81	! FASTORE
	ASTORE_WORD

	DECLARE 82	! DASTORE
	ASTORE_DWORD

	DECLARE 83	! AASTORE
	LOAD_NEXT
	ld [ TOP + 8 ], %l3	! get array reference
	ld [ TOP + 4 ], %l4	! get index
	ld [ TOP ], %l5		! get value
	CHECK_ARRAY_BOUND %l3, %l4, %o3
	mov %l5, %o1
	mov %l3, %o0
	sethi %hi(jstack_point), %o3
	call soft_checkarraystore
	st TOP, [ %o3 + %lo(jstack_point) ]
	sll %l4, 2, %l4
	add %l4, \&OFFSET_ARRAYDATA, %l4
	st %l5, [ %l3 + %l4 ]
	add TOP, 12, TOP
	NEXT

	DECLARE 84	! BASTORE
	ASTORE_WORD 0, stb

	DECLARE 85	! CASTORE
	ASTORE_WORD 1, sth

	DECLARE 86	! SASTORE
	ASTORE_WORD 1, sth

	DECLARE 87	! POP
	LOAD_NEXT
	add TOP, 4, TOP
	NEXT

	DECLARE 88	! POP2
	LOAD_NEXT
	add TOP, 8, TOP
	NEXT

	DECLARE 89	! DUP
	LOAD_NEXT
	ld [ TOP ], %o0
	PUSH %o0
	NEXT

	DECLARE 90	! DUP_X1
	LOAD_NEXT
	ld [ TOP ], %o0
	ld [ TOP + 4 ], %o1
	st %o0, [ TOP + 4 ]
	st %o1, [ TOP ]
	st %o0, [ TOP - 4 ]
	add TOP, -4, TOP
	NEXT

	DECLARE 91	! DUP_X2
	LOAD_NEXT
	ld [ TOP ], %o0
	ld [ TOP + 4 ], %o1
	ld [ TOP + 8 ], %o2
	st %o0, [ TOP + 8 ]
	st %o2, [ TOP + 4 ]
	st %o1, [ TOP ]
	st %o0, [ TOP - 4 ]
	add TOP, -4, TOP
	NEXT

	DECLARE 92	! DUP2
	LOAD_NEXT
	ld [ TOP ], %o0
	ld [ TOP + 4 ], %o1
	st %o1, [ TOP - 4 ]
	st %o0, [ TOP - 8 ]
	add TOP, -8, TOP
	NEXT

	DECLARE 93	! DUP2_X1
	LOAD_NEXT
	ld [ TOP ], %o0
	ld [ TOP + 4 ], %o1
	ld [ TOP + 8 ], %o2
	st %o1, [ TOP + 8 ]
	st %o0, [ TOP + 4 ]
	st %o2, [ TOP ]
	st %o1, [ TOP - 4 ]
	st %o0, [ TOP - 8 ]
	add TOP, -8, TOP
	NEXT

	DECLARE 94	! DUP2_X2
	LOAD_NEXT
	ld [ TOP ], %o0
	ld [ TOP + 4 ], %o1
	ld [ TOP + 8 ], %o2
	ld [ TOP + 12 ], %o3
	st %o1, [ TOP + 12 ]
	st %o0, [ TOP + 8 ]
	st %o3, [ TOP + 4 ]
	st %o2, [ TOP ]
	st %o1, [ TOP - 4 ]
	st %o0, [ TOP - 8 ]
	add TOP, -8, TOP
	NEXT

	DECLARE 95	! SWAP
	LOAD_NEXT
	ld [ TOP ], %o0
	ld [ TOP + 4 ], %o1
	st %o0, [ TOP + 4 ]
	st %o1, [ TOP ]
	NEXT

	!! Integer arithmetic
	.macro IARITH instr
	LOAD_NEXT
	ld [ TOP ], %o0
	ld [ TOP + 4 ], %o1
	\instr %o1, %o0, %o2
	st %o2, [ TOP + 4 ]
	add TOP, 4, TOP
	NEXT
	.endm

	!! Long integer arithmetic with two instructions
	.macro LARITH instr1, instr2
	LOAD_NEXT
	ld [ TOP  ], %o1
	ld [ TOP + 8 ], %o3
	ld [ TOP + 4 ], %o0
	ld [ TOP + 12 ], %o2
	\instr1 %o3, %o1, %o1
	\instr2 %o2, %o0, %o0
	st %o1, [ TOP + 8 ]
	st %o0, [ TOP + 12 ]
	add TOP, 8, TOP
	NEXT
	.endm

	!! Long integer arithmetic with function call
	.macro LARITH_FUNC func
	LOAD_NEXT
	ld [ TOP + 12], %o0
	ld [ TOP + 8 ], %o1
	ld [ TOP + 4], %o2
	call \func
	ld [ TOP ], %o3		! delay slot
	st %o0, [ TOP + 12 ]
	st %o1, [ TOP + 8 ]
	add TOP, 8, TOP
	NEXT
	.endm

	!! Single precision floating point arithmetic
	.macro FARITH instr
	LOAD_NEXT
	ld [ TOP ], %f0
	ld [ TOP + 4 ], %f1
	\instr %f1, %f0, %f2
	st %f2, [ TOP + 4 ]
	add TOP, 4, TOP
	NEXT
	.endm

	!! Double precision floating point arithmetic
	.macro DARITH instr
	LOAD_NEXT
	ld [ TOP + 4 ], %f0
	ld [ TOP ], %f1
	ld [ TOP + 12 ], %f2
	ld [ TOP + 8 ], %f3
	\instr %f2, %f0, %f4
	st %f4, [ TOP + 12 ]
	st %f5, [ TOP + 8 ]
	add TOP, 8, TOP
	NEXT
	.endm

	DECLARE 96	! IADD
	IARITH add

	DECLARE 97	! LADD
	LARITH addcc, addc

	DECLARE 98	! FADD
	FARITH fadds

	DECLARE 99	! DADD
	DARITH faddd

	DECLARE 100	! ISUB
	IARITH sub

	DECLARE 101	! LSUB
	LARITH subcc, subc

	DECLARE 102	! FSUB
	FARITH fsubs

	DECLARE 103	! DSUB
	DARITH fsubd

	DECLARE 104	! IMUL
	IARITH smul

	DECLARE 105	! LMUL
	LARITH_FUNC soft_lmul

	DECLARE 106	! FMUL
	FARITH fmuls

	DECLARE 107	! DMUL
	DARITH fmuld

	DECLARE 108	! IDIV
	LOAD_NEXT
	ld [ TOP ], %o0
	ld [ TOP + 4 ], %o1
	sra %o1, 31, %o3
	wr %g0, %o3, %y
	sdiv %o1, %o0, %o1
	st %o1, [ TOP + 4 ]
	add TOP, 4, TOP
	NEXT

	DECLARE 109	! LDIV
	LARITH_FUNC __divdi3	! FIXME: should not use internal gcc function

	DECLARE 110	! FDIV
	FARITH fdivs

	DECLARE 111	! DDIV
	DARITH fdivd

	DECLARE 112	! IREM
	LOAD_NEXT
	ld [ TOP + 4 ], %o0
	call soft_rem
	ld [ TOP ], %o1		! delay slot
	st %o0, [ TOP + 4]
	add TOP, 4, TOP
	NEXT

	DECLARE 113	! LREM
	LARITH_FUNC __moddi3	! FIXME: should not use internal gcc function

	DECLARE 114	! FREM
	LOAD_NEXT
	ld [ TOP + 4 ], %o0
	call soft_frem
	ld [ TOP ], %o1		! delay slot
	st %f0, [ TOP + 4 ]
	add TOP, 4, TOP
	NEXT

	DECLARE 115	! DREM
	LOAD_NEXT
	ld [ TOP + 12 ], %o0
	ld [ TOP + 8 ], %o1
	ld [ TOP + 4 ], %o2
	call fmod
	ld [ TOP ], %o3		! delay slot
	st %f0, [ TOP + 12 ]
	st %f1, [ TOP + 8 ]
	add TOP, 8, TOP
	NEXT

	DECLARE 116	! INEG
	LOAD_NEXT
	ld [ TOP ], %o0
	sub %g0, %o0, %o1
	st %o1, [ TOP ]
	NEXT

	DECLARE 117	! LNEG
	LOAD_NEXT
	ld [ TOP ], %o0
	ld [ TOP + 4 ], %o1
	subcc %g0, %o0, %o0
	subx %g0, %o1, %o1
	st %o0, [ TOP ]
	st %o1, [ TOP + 4 ]
	NEXT

	DECLARE 118	! FNEG
	LOAD_NEXT
	ld [ TOP ], %f0
	fnegs %f0, %f0
	st %f0, [ TOP ]
	NEXT

	DECLARE 119	! DNEG
	LOAD_NEXT
	ld [ TOP + 4 ], %f0
	ld [ TOP ], %f1
	fnegd %f0, %f0
	st %f0, [ TOP + 4 ]
	st %f1, [ TOP ]
	NEXT

	!! Shift long value
	.macro LONG_SHIFT call
	LOAD_NEXT
	ld [ TOP ], %o2		! load shift amount
	ld [ TOP + 8 ], %o0	! load high value
	call \call
	ld [ TOP + 4 ], %o1	! load low value, delay slot
	st %o0, [ TOP + 8 ]
	st %o1, [ TOP + 4 ]
	add TOP, 4, TOP
	NEXT
	.endm

	DECLARE 120	! ISHL
	IARITH sll

	DECLARE 121	! LSHL
	LONG_SHIFT soft_lshll

	DECLARE 122	! ISHR
	IARITH sra

	DECLARE 123	! LSHR
	LONG_SHIFT soft_ashrl

	DECLARE 124	! IUSHR
	IARITH srl

	DECLARE 125	! LUSHR
	LONG_SHIFT soft_lshrl

	DECLARE 126	! IAND
	IARITH and

	DECLARE 127	! LAND
	LARITH and, and

	DECLARE 128	! IOR
	IARITH or

	DECLARE 129	! LOR
	LARITH or, or

	DECLARE 130	! IXOR
	IARITH xor

	DECLARE 131	! LXOR
	LARITH xor, xor

	DECLARE 132	! IINC
	LOAD_NEXT 3
	INDEX %o0		! get index
	INDEX %o1, 2		! get constant
	sll %o0, 2, %o0
	neg %o0
	ld [ LOCALS + %o0 ], %o2
	sll %o1, 24, %o1	! sign extend
	sra %o1, 24, %o1
	add %o2, %o1, %o2
	st %o2, [ LOCALS + %o0 ]
	NEXT 3

	DECLARE 133	! I2L
	LOAD_NEXT
	ld [ TOP ], %o0
	sra %o0, 31, %o1
	st %o0, [ TOP - 4 ]
	st %o1, [ TOP ]
	add TOP, -4, TOP
	NEXT

	DECLARE 134	! I2F
	LOAD_NEXT
	ld [ TOP ], %f0
	fitos %f0, %f1
	st %f1, [ TOP ]
	NEXT

	DECLARE 135	! I2D
	LOAD_NEXT
	ld [ TOP ], %f0
	fitod %f0, %f2
	st %f2, [ TOP ]
	st %f3, [ TOP - 4 ]
	add TOP, -4, TOP
	NEXT

	DECLARE 136	! L2I
	LOAD_NEXT
	ld [ TOP ], %o0
	st %o0, [ TOP + 4 ]
	add TOP, 4, TOP
	NEXT

	DECLARE 137	! L2F
	LOAD_NEXT
	ld [ TOP + 4 ], %f0
	ld [ TOP ], %f1
	fxtos %f0, %f2
	st %f2, [ TOP + 4 ]
	add TOP, 4, TOP
	NEXT

	DECLARE 138	! L2D
	LOAD_NEXT
	ld [ TOP + 4 ], %f0
	ld [ TOP ], %f1
	fxtod %f0, %f2
	st %f2, [ TOP + 4 ]
	st %f3, [ TOP ]
	NEXT

	DECLARE 139	! F2I
	LOAD_NEXT
	ld [ TOP ], %f0
	fstoi %f0, %f1
	st %f1, [ TOP ]
	NEXT

	DECLARE 140	! F2L
	LOAD_NEXT
	ld [ TOP ], %f0
	fstox %f0, %f2
	st %f2, [ TOP ]
	st %f3, [ TOP - 4 ]
	add TOP, -4, TOP
	NEXT

	DECLARE 141	! F2D
	LOAD_NEXT
	ld [ TOP ], %f0
	fstod %f0, %f2
	st %f2, [ TOP ]
	st %f3, [ TOP - 4 ]
	add TOP, -4, TOP
	NEXT

	DECLARE 142	! D2I
	LOAD_NEXT
	ld [ TOP + 4 ], %f0
	ld [ TOP ], %f1
	fdtoi %f0, %f2
	st %f2, [ TOP + 4 ]
	add TOP, 4, TOP
	NEXT

	DECLARE 143	! D2L
	LOAD_NEXT
	ld [ TOP + 4 ], %f0
	ld [ TOP ], %f1
	fdtox %f0, %f2
	st %f2, [ TOP + 4 ]
	st %f3, [ TOP ]
	NEXT

	DECLARE 144	! D2F
	LOAD_NEXT
	ld [ TOP + 4 ], %f0
	ld [ TOP ], %f1
	fdtos %f0, %f2
	st %f2, [ TOP + 4 ]
	add TOP, 4, TOP
	NEXT

	!! Narrow integer type
	.macro NARROW_INT instr, scale
	LOAD_NEXT
	ld [ TOP ], %o0
	sll %o0, \scale, %o0
	\instr %o0, \scale, %o0
	st %o0, [ TOP ]
	NEXT
	.endm

	DECLARE 145	! I2B
	NARROW_INT sra, 24

	DECLARE 146	! I2C
	NARROW_INT srl, 16

	DECLARE 147	! I2S
	NARROW_INT sra, 16

	DECLARE 148	! LCMP
	LOAD_NEXT
	ld [ TOP + 12], %o2
	ld [ TOP + 8 ], %o3
	ld [ TOP + 4], %o0
	call soft_lcmp
	ld [ TOP ], %o1		! delay slot
	st %o0, [ TOP + 12 ]
	add TOP, 12, TOP
	NEXT

	DECLARE 149	! FCMPL
	LOAD_NEXT
	ld [ TOP + 4 ], %f0
	ld [ TOP ], %f1
	fcmps %fcc0, %f0, %f1
	movg %fcc0, 2, %o0
	move %fcc0, 1, %o0
	movul %fcc0, 0, %o0
	sub %o0, 1, %o0
	st %o0, [ TOP + 4 ]
	add TOP, 4, TOP
	NEXT

	DECLARE 150	! FCMPG
	LOAD_NEXT
	ld [ TOP + 4 ], %f0
	ld [ TOP ], %f1
	fcmps %fcc0, %f0, %f1
	movug %fcc0, 2, %o0
	move %fcc0, 1, %o0
	movl %fcc0, 0, %o0
	sub %o0, 1, %o0
	st %o0, [ TOP + 4 ]
	add TOP, 4, TOP
	NEXT

	DECLARE 151	! DCMPL
	LOAD_NEXT
	ld [ TOP + 12 ], %f0
	ld [ TOP + 8 ], %f1
	ld [ TOP + 4 ], %f2
	ld [ TOP ], %f3
	fcmpd %fcc0, %f0, %f2
	movg %fcc0, 2, %o0
	move %fcc0, 1, %o0
	movul %fcc0, 0, %o0
	sub %o0, 1, %o0
	st %o0, [ TOP + 12 ]
	add TOP, 12, TOP
	NEXT

	DECLARE 152	! DCMPG
	LOAD_NEXT
	ld [ TOP + 12 ], %f0
	ld [ TOP + 8 ], %f1
	ld [ TOP + 4 ], %f2
	ld [ TOP ], %f3
	fcmpd %fcc0, %f0, %f2
	movug %fcc0, 2, %o0
	move %fcc0, 1, %o0
	movl %fcc0, 0, %o0
	sub %o0, 1, %o0
	st %o0, [ TOP + 12 ]
	add TOP, 12, TOP
	NEXT

	!! Branch depending on top value in operand stack
	.macro BRANCH_COND instr
	ld [ TOP ], %o0		! load value
	cmp %o0, %g0
	\instr 1f		! branch on condition
	add TOP, 4, TOP		! delay slot
	LOAD_NEXT 3
	NEXT 3
1:	! Branch to specified offset
	BRANCH_OFFSET
	.endm

	DECLARE 153	! IFEQ
	BRANCH_COND be

	DECLARE 154	! IFNE
	BRANCH_COND bne

	DECLARE 155	! IFLT
	BRANCH_COND bl

	DECLARE 156	! IFGE
	BRANCH_COND bge

	DECLARE 157	! IFGT
	BRANCH_COND bg

	DECLARE 158	! IFLE
	BRANCH_COND ble

	!! Branch depending on comparisons of two ints
	.macro BRANCH_ICOND instr
	ld [ TOP + 4 ], %o0
	ld [ TOP ], %o1
	cmp %o0, %o1
	\instr 2f
	add TOP, 8, TOP
	LOAD_NEXT 3
	NEXT 3
2:	! Branch to specified offset
	BRANCH_OFFSET
	.endm

	DECLARE 159	! IF_ICMPEQ
	BRANCH_ICOND be

	DECLARE 160	! IF_ICMPNE
	BRANCH_ICOND bne

	DECLARE 161	! IF_ICMPLT
	BRANCH_ICOND bl

	DECLARE 162	! IF_ICMPGE
	BRANCH_ICOND bge

	DECLARE 163	! IF_ICMPGT
	BRANCH_ICOND bg

	DECLARE 164	! IF_ICMPLE
	BRANCH_ICOND ble

	DECLARE 165	! IF_ACMPEQ
	BRANCH_ICOND be

	DECLARE 166	! IF_ACMPNE
	BRANCH_ICOND bne

	DECLARE 167	! GOTO
	BRANCH_OFFSET

	DECLARE 168	! JSR
	add PC, 3, %o0
	PUSH %o0		! push return address onto stack
	BRANCH_OFFSET

	DECLARE 169	! RET
	INDEX %o0
	RLOAD %o0, PC
	BRANCH_ACTION
	ldub [ PC ], TARGET
	sll TARGET, SDISP, TARGET
	jmp ORIGIN + TARGET
	nop

	DECLARE 170	! TABLESWITCH
	ld [ TOP ], %o0
	call intrp_tableswitch
	mov PC, %o1	! delay slot
	add PC, %o0, PC
	ldub [ PC ], TARGET
	sll TARGET, SDISP, TARGET
	jmp ORIGIN + TARGET
	add TOP, 4, TOP

	DECLARE 171	! LOOKUPSWITCH
	ld [ TOP ], %o0
	call intrp_lookupswitch
	mov PC, %o1	! delay slot
	add PC, %o0, PC
	ldub [ PC ], TARGET
	sll TARGET, SDISP, TARGET
	jmp ORIGIN + TARGET
	add TOP, 4, TOP

	!! Prepare for returning from method
	!! Uses registers %o0, %o1, %o2
	.macro PREPARE_RETURN
	! If method is synchronized, unlock object or class
	lduh [ METHOD + \&OFFSET_FLAGS ], %o1	! load flags
	and %o1, h'20, %o2
	cmp %o2, %g0		! check if synchronized
	be 2f
	and %o1, h'8, %o2
	cmp %o2, %g0		! check if instance method
	bne,a 1f
	ld [ METHOD + \&OFFSET_CLASS ], %o0	! load class (delay slot)
	LOAD 0, %o0		! load object
1:	! synchronize
	CALL_FUNC _unlockMutex

2:	! restore caller registers
	RESTORE_CALLER
	.endm

	DECLARE 172	! IRETURN
	ld [ TOP ], %o4
	PREPARE_RETURN
	PUSH %o4
	METHOD_RETURN

	DECLARE 173	! LRETURN
	ld [ TOP + 4 ], %o4
	ld [ TOP ], %o5
	PREPARE_RETURN
	PUSH_W %o4, %o5
	METHOD_RETURN

	DECLARE 174	! FRETURN
	ld [ TOP ], %o4
	PREPARE_RETURN
	PUSH %o4
	METHOD_RETURN

	DECLARE 175	! DRETURN
	ld [ TOP + 4 ], %o4
	ld [ TOP ], %o5
	PREPARE_RETURN
	PUSH_W %o4, %o5
	METHOD_RETURN

	DECLARE 176	! ARETURN
	ld [ TOP ], %o4
	PREPARE_RETURN
	PUSH %o4
	METHOD_RETURN

	DECLARE 177	! RETURN
	PREPARE_RETURN
	METHOD_RETURN

	!! Resolve field reference
	.macro RESOLVE_FIELD func
	INDEX_W %o1, %o0	! load constant pool index
	GET_RESOLVED %o1, %o2
	mov METHOD, %o0
	CALL_FUNC \func		! resolve constant pool item
	.endm

	!! Rewrite bytecode and jump to real implementation for field
	!! access opcodes.
	.macro REWRITE_FIELD_ACCESS resolver, opbase
	RESOLVE_FIELD \resolver	! find field type
	add %o0, \opbase, %o1	! get internal opcode
	sll %o1, SDISP, TARGET
	jmp ORIGIN + TARGET	! jump to real implementation
	stb %o1, [ PC ]		! replace bytecode, delay slot
	.endm

	DECLARE 178	! GETSTATIC
	REWRITE_FIELD_ACCESS intrp_resolve_static_field, 228

	DECLARE 179	! PUTSTATIC
	REWRITE_FIELD_ACCESS intrp_resolve_static_field, 238

	DECLARE 180	! GETFIELD
	REWRITE_FIELD_ACCESS intrp_resolve_field, 223

	DECLARE 181	! PUTFIELD
	REWRITE_FIELD_ACCESS intrp_resolve_field, 233

	!! Invoke method
	!! offset is offset to next bytecode instruction to run
	!! %o0 should contain method pointer
	!! %o1 should contain native code pointer
	!! %o2 contains arguments displacement if (\nargs neq 0)
	.macro INVOKE_METHOD nargs=0, offset=3
	.aif \nargs eq 0
	ldsh [ %o0 + \&OFFSET_ARGDISP ], %o2	! load arguments displacement
	.aendi
	ld [ %o1 ], %o3		! load first instruction
	add PC, \offset, TARGET	! set bytecode counter
	cmp %o3, FAKEI		! check whether trampoline
	be interpret_invoke
	add TOP, %o2, %o2	! delay slot

	! Call native code
	add %o2, 4, TOP
	ld [ %o0 + \&OFFSET_NCALLER ], %l5
	sethi %hi(jstack_point), %l3
	st TOP, [ %l3 + %lo(jstack_point) ]
	call %l5
	nop
	sub TOP, %o0, TOP
	mov TARGET, PC
	LOAD_NEXT 0
	NEXT 0
	.endm

	DECLARE 182	! INVOKEVIRTUAL
	RESOLVE_POOL intrp_resolve_method
	ldsh [ %o0 + \&OFFSET_IDX ], %o1	! load method index
	lduh [ %o0 + \&OFFSET_ARGDISP ], %o2
	ld [ TOP + %o2 ], %o3		! load object reference
	sll %o1, 2, %o1			! calculate real index
	add %o1, \&OFFSET_DTABLE, %o1
	ld [ %o3 ], %o3			! load dispatch table
	ld [ %o3 + %o1 ], %o1		! load native code pointer
	INVOKE_METHOD 1

	DECLARE 183	! INVOKESPECIAL
	RESOLVE_POOL intrp_resolve_method
	ld [ %o0 + \&OFFSET_NATIVE ], %o1	! load native code pointer
	INVOKE_METHOD

	DECLARE 184	! INVOKESTATIC
	RESOLVE_POOL intrp_resolve_method
	ld [ %o0 + \&OFFSET_NATIVE ], %o1	! load native code pointer
	INVOKE_METHOD

	DECLARE 185	! INVOKEINTERFACE
	RESOLVE_POOL intrp_resolve_method
	ld [ %o0 + \&OFFSET_IIDX ], %o1	! load method index
	lduh [ %o0 + \&OFFSET_ARGDISP ], %o2
	sll %o1, 2, %o1			! calculate real index
!	add %o1, \&OFFSET_DTABLE, %o1
	neg %o1
	ld [ TOP + %o2 ], %o3		! load object reference
	ld [ %o3 ], %o3			! load dispatch table
	ld [ %o3 + %o1 ], %o1		! load native code pointer
	INVOKE_METHOD 1, 5

	DECLARE 187	! NEW
	LOAD_NEXT 3
	RESOLVE_POOL intrp_resolve_class
	! allocate object
	CALL_FUNC newObject
	PUSH %o0
	NEXT 3

	DECLARE 188	! NEWARRAY
	LOAD_NEXT 2
	INDEX %o0		! get type
	ld [ TOP ], %o1
	CALL_FUNC soft_newarray
	st %o0, [ TOP ]
	NEXT 2

	DECLARE 189	! ANEWARRAY
	LOAD_NEXT 3
	RESOLVE_POOL intrp_resolve_class
	! allocate reference array
	ld [ TOP ], %o1		! get array size
	CALL_FUNC newArray
	st %o0, [ TOP ]
	NEXT 3

	DECLARE 190	! ARRAYLENGTH
	LOAD_NEXT
	ld [ TOP ], %o0		! load array reference
	ld [ %o0 + \&OFFSET_ARRAYSIZE ], %o1
	st %o1, [ TOP ]
	NEXT

	DECLARE 191	! ATHROW
	ld [ TOP ], %o0
	CALL_FUNC throwExternalException
	! doesn't return here

	DECLARE 192	! CHECKCAST
	LOAD_NEXT 3
	RESOLVE_POOL intrp_resolve_class
	! check class
	ld [ TOP ], %o1
	CALL_FUNC soft_checkcast
	NEXT 3

	DECLARE 193	! INSTANCEOF
	LOAD_NEXT 3
	RESOLVE_POOL intrp_resolve_class
	! check class
	ld [ TOP ], %o1
	CALL_FUNC soft_instanceof
	st %o0, [ TOP ]
	NEXT 3

	DECLARE 194	! MONITORENTER
	LOAD_NEXT
	ld [ TOP ], %o0
	add TOP, 4, TOP
	CALL_FUNC _lockMutex
	NEXT

	DECLARE 195	! MONITOREXIT
	LOAD_NEXT
	ld [ TOP ], %o0
	add TOP, 4, TOP
	CALL_FUNC _unlockMutex
	NEXT

	DECLARE 196	! WIDE
	ldub [ PC + 1 ], %o0	! get instruction to widen
	cmp %o0, 25
	ble,a 1f		! xLOAD instruction
	add %o0, 190, %o1	! get internal opcode, delay slot
	cmp %o0, 58
	ble,a 2f		! xSTORE instruction
	add %o0, 162, %o2	! get internal opcode, delay slot
	cmp %o0, 132		! IINC instruction
	be,a 3f
	mov 222, %o1		! 222 is internal opcode for WIDE IINC
	! handle RET instruction
	mov 221, %o1		! 221 is internal opcode for WIDE RET
	sll %o1, SDISP, TARGET
	jmp ORIGIN + TARGET	! jump to real implementation
	stb %o1, [ PC ]		! replace bytecode, delay slot
1:	! handle xLOAD instruction
	sll %o1, SDISP, TARGET
	jmp ORIGIN + TARGET	! jump to real implementation
	stb %o1, [ PC ]		! replace bytecode, delay slot
2:	! handle xSTORE instruction
	sll %o2, SDISP, TARGET
	jmp ORIGIN + TARGET	! jump to real implementation
	stb %o2, [ PC ]		! replace bytecode, delay slot
3:	! handle IINC instruction
	sll %o1, SDISP, TARGET
	jmp ORIGIN + TARGET	! jump to real implementation
	stb %o1, [ PC ]		! replace bytecode, delay slot

	DECLARE 197	! MULTIANEWARRAY
	LOAD_NEXT 4
	RESOLVE_POOL intrp_resolve_class
	! allocate multidimensional array
	INDEX %o1, 3		! get dimension
	mov TOP, %o2
	sll %o1, 2, %o3
	add TOP, %o3, TOP
	CALL_FUNC intrp_multianewarray
	st %o0, [ TOP - 4 ]
	add TOP, -4, TOP
	NEXT 4

	DECLARE 198	! IFNULL
	BRANCH_COND be

	DECLARE 199	! IFNONNULL
	BRANCH_COND bne

	.macro BRANCH_WIDE
	ldub [ PC + 1 ], %o0	! load offset
	ldub [ PC + 2 ], %o1
	ldub [ PC + 3 ], %o2
	ldub [ PC + 4 ], %o3
	sll %o0, 24, %o0	! construct offset
	sll %o1, 16, %o1
	sll %o2, 8, %o2
	or %o0, %o1, %o0
	or %o2, %o3, %o2
	or %o0, %o2, %o0	! %o0 now contains branch offset
	BRANCH_ACTION
	ldub [ PC + %o0 ], TARGET
	sll TARGET, SDISP, TARGET
	jmp ORIGIN + TARGET
	add PC, %o0, PC
	.endm

	DECLARE 200	! GOTO_W
	BRANCH_WIDE

	DECLARE 201	! JSR_W
	add PC, 5, %o0
	PUSH %o0
	BRANCH_WIDE

	!! Implementation dependent opcodes
interpret_internal:

	!! Load word to local variable with wide index
	.macro LOAD_WORD_W
	LOAD_NEXT 4
	INDEX_W %o0, %o2, 2
	RLOAD %o0, %o1
	PUSH %o1
	NEXT 4
	.endm

	!! Load double word to local variable with wide index
	.macro LOAD_DWORD_W
	LOAD_NEXT 4
	INDEX_W %o0, %o1, 2
	RLOAD_W %o0, %o1, %o2
	PUSH_W %o1, %o2
	NEXT 4
	.endm

	DECLARE 211	! WIDE ILOAD
	LOAD_WORD_W

	DECLARE 212	! WIDE LLOAD
	LOAD_DWORD_W

	DECLARE 213	! WIDE FLOAD
	LOAD_WORD_W

	DECLARE 214	! WIDE DLOAD
	LOAD_DWORD_W

	DECLARE 215	! WIDE ALOAD
	LOAD_WORD_W

	!! Store word into local variable with wide index
	.macro STORE_WORD_W
	LOAD_NEXT 4
	INDEX_W %o0, %o1, 2
	ld [ TOP ], %o1
	RSTORE %o0, %o1
	add TOP, 4, TOP
	NEXT 4
	.endm

	!! Store double word into local variable with wide index
	.macro STORE_DWORD_W
	LOAD_NEXT 4
	INDEX_W %o0, %o1, 2
	ld [ TOP + 4 ], %o1
	ld [ TOP ], %o2
	RSTORE_W %o0, %o1, %o2
	add TOP, 8, TOP
	NEXT 4
	.endm

	DECLARE 216	! WIDE ISTORE
	STORE_WORD_W

	DECLARE 217	! WIDE LSTORE
	STORE_DWORD_W

	DECLARE 218	! WIDE FSTORE
	STORE_WORD_W

	DECLARE 219	! WIDE DSTORE
	STORE_DWORD_W

	DECLARE 220	! WIDE ASTORE
	STORE_WORD_W

	DECLARE 221	! WIDE RET
	INDEX_W %o0, %01, 2
	RLOAD %o0, PC
	BRANCH_ACTION
	ldub [ PC ], TARGET
	sll TARGET, SDISP, TARGET
	jmp ORIGIN + TARGET
	nop

	DECLARE 222	! WIDE IINC
	LOAD_NEXT 6
	INDEX_W %o0, %o2, 2	! get index
	INDEX_W %o1, %o2, 4	! get constant
	sll %o0, 2, %o0
	neg %o0
	ld [ LOCALS + %o0 ], %o2
	sll %o1, 16, %o1	! sign extend
	sra %o1, 16, %o1
	add %o2, %o1, %o2
	st %o2, [ LOCALS + %o0 ]
	NEXT 6

	!! Load field into word
	.macro LOAD_FIELD address, instr
	\instr [ \address ], %o0
	st %o0, [ TOP - 4 ]
	add TOP, -4, TOP
	.endm

	!! Load field into double word
	.macro LOAD_FIELD_W address
	ld [ \address ], %o0
	ld [ \address + 4 ], %o1
	st %o0, [ TOP - 4 ]
	st %o1, [ TOP - 8 ]
	add TOP, -8, TOP
	.endm

	!! Store word into field
	.macro STORE_FIELD address, instr
	ld [ TOP ], %o0
	add TOP, 4, TOP
	\instr %o0, [ \address ]
	.endm

	!! Store double word into field
	.macro STORE_FIELD_W address
	ld [ TOP + 4 ], %o0
	ld [ TOP ], %o1
	add TOP, 8, TOP
	st %o0, [ \address ]
	st %o1, [ \address + 4 ]
	.endm

	!! Obtain address for instance field
	.macro GET_IFIELD_ADDRESS reg, offset=0
	INDEX_W %o0, %o1	! get index
	GET_RESOLVED %o0, %o1
	ld [ %o1 ], %o1		! get offset
	ld [ TOP + \offset ], %o3
	add %o3, %o1, \reg
	.endm

	!! Load instance field to operand stack
	.macro LOAD_IFIELD instr
	LOAD_NEXT 3
	GET_IFIELD_ADDRESS %o3
	add TOP, 4, TOP
	LOAD_FIELD %o3, \instr
	NEXT 3
	.endm

	DECLARE 223	! GETFIELD (word)
	LOAD_IFIELD ld

	DECLARE 224	! GETFIELD (short)
	LOAD_IFIELD ldsh

	DECLARE 225	! GETFIELD (byte)
	LOAD_IFIELD ldsb

	DECLARE 226	! GETFIELD (char)
	LOAD_IFIELD lduh

	DECLARE 227	! GETFIELD (double)
	LOAD_NEXT 3
	GET_IFIELD_ADDRESS %o3
	add TOP, 4, TOP
	LOAD_FIELD_W %o3
	NEXT 3

	!! Load static field to operand stack
	.macro LOAD_SFIELD instr
	LOAD_NEXT 3
	INDEX_W %o0, %o1	! get index
	GET_RESOLVED %o0, %o3	! get address
	ld [ %o3 ], %o3
	LOAD_FIELD %o3, \instr
	NEXT 3
	.endm

	DECLARE 228	! GETSTATIC (word)
	LOAD_SFIELD ld

	DECLARE 229	! GETSTATIC (short)
	LOAD_SFIELD ldsh

	DECLARE 230	! GETSTATIC (byte)
	LOAD_SFIELD ldsb

	DECLARE 231	! GETSTATIC (char)
	LOAD_SFIELD lduh

	DECLARE 232	! GETSTATIC (double)
	LOAD_NEXT 3
	INDEX_W %o0, %o1	! get index
	GET_RESOLVED %o0, %o3	! get address
	ld [ %o3 ], %o3
	LOAD_FIELD_W %o3
	NEXT 3

	!! Set instance field from operand stack
	.macro STORE_IFIELD instr
	LOAD_NEXT 3
	GET_IFIELD_ADDRESS %o3, 4
	STORE_FIELD %o3, \instr
	add TOP, 4, TOP
	NEXT 3
	.endm

	DECLARE 233	! PUTFIELD (word)
	STORE_IFIELD st

	DECLARE 234	! PUTFIELD (short)
	STORE_IFIELD sth

	DECLARE 235	! PUTFIELD (byte)
	STORE_IFIELD stb

	DECLARE 236	! PUTFIELD (char)
	STORE_IFIELD sth

	DECLARE 237	! PUTFIELD (double)
	LOAD_NEXT 3
	GET_IFIELD_ADDRESS %o3, 8
	STORE_FIELD_W %o3
	add TOP, 4, TOP
	NEXT 3

	!! Set static field from operand stack
	.macro STORE_SFIELD instr
	LOAD_NEXT 3
	INDEX_W %o0, %o1	! get index
	GET_RESOLVED %o0, %o3	! get address
	ld [ %o3 ], %o3
	STORE_FIELD %o3, \instr
	NEXT 3
	.endm

	DECLARE 238	! PUTSTATIC (word)
	STORE_SFIELD st

	DECLARE 239	! PUTSTATIC (short)
	STORE_SFIELD sth

	DECLARE 240	! PUTSTATIC (byte)
	STORE_SFIELD stb

	DECLARE 241	! PUTSTATIC (char)
	STORE_SFIELD sth

	DECLARE 242	! PUTSTATIC (double)
	LOAD_NEXT 3
	INDEX_W %o0, %o1	! get index
	GET_RESOLVED %o0, %o3	! get address
	ld [ %o3 ], %o3
	STORE_FIELD_W %o3
	NEXT 3

	!! Invoke interpreted method
	!! %o0 - method pointer
	!! %o1 - native code pointer
	!! %o2 - address of first argument
interpret_invoke:
	ld [ %o1 + 12 ], %o0	! load real method pointer from trampoline
	add %o2, 4, TOP

	! get new frame pointer
	lduh [ %o0 + \&OFFSET_LOCALSZ ], %o4
	sll %o4, 2, %o4
	sub %o2, %o4, %o4
	add %o4, -48, %o4

	ld [ %o0 + \&OFFSET_BCODE ], %o3	! load bytecode pointer

1:	! Save registers
	st LOCALS, [ %o4 + 4 ]
	st TOP, [ %o4 + 8 ]
	st PC, [ %o4 + 12 ]
	st METHOD, [ %o4 + 16 ]
	st POOL, [ %o4 + 20 ]
	st FP, [ %o4 + 24 ]
	st TARGET, [ %o4 + 28 ]

	! Set registers
	mov %o0, METHOD
	mov %o4, FP
	mov %o2, LOCALS
	mov FP, TOP
	mov %o3, PC
	ld [ METHOD + \&OFFSET_POOL ], POOL

	METH_COUNT

	! If method is synchronized, lock object or class
	lduh [ METHOD + \&OFFSET_FLAGS ], %o1	! load flags
	and %o1, h'20, %o2
	cmp %o2, %g0		! check if synchronized
	be 2f
	and %o1, h'8, %o2
	cmp %o2, %g0		! check if instance method
	bne,a 1f
	ld [ METHOD + \&OFFSET_CLASS ], %o0	! load class (delay slot)
	LOAD 0, %o0		! load object
1:	! synchronize
	CALL_FUNC _lockMutex

2:	! Jump to start of bytecode
	LOAD_NEXT 0
	NEXT 0


	! Opcode implementations exist before this point
_interpret_end:

	.end
