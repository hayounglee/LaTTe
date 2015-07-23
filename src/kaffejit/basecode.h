/* basecode.h
 * Define the base instructions macros.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>
 */

#ifndef __basecode_h
#define __basecode_h

extern int argcount;

/* -------------------------------------------------------------------- */
/* Branches */

#define	ba			0	/* Always */
#define	beq			1	/* Equal */
#define	blt			2	/* Less than */
#define	ble			3	/* Less than or equal */
#define	bgt			4	/* Greater than */
#define	bge			5	/* Greater than or equal */
#define	bne			6	/* Not equal */
#define	bn			7	/* Never */
#define	bindirect		8	/* Indirect */
#define	bult			9	/* Unsigned less than */
#define	buge			10	/* Unsigned greater than or equal */

#define branch_a(l)			branch(l, ba)

#define cbranch_int_eq(s1, s2, l)	cbranch_int(s1, s2, l, beq)
#define cbranch_int_ne(s1, s2, l)	cbranch_int(s1, s2, l, bne)
#define cbranch_int_lt(s1, s2, l)	cbranch_int(s1, s2, l, blt)
#define cbranch_int_le(s1, s2, l)	cbranch_int(s1, s2, l, ble)
#define cbranch_int_gt(s1, s2, l)	cbranch_int(s1, s2, l, bgt)
#define cbranch_int_ge(s1, s2, l)	cbranch_int(s1, s2, l, bge)
#define cbranch_int_ult(s1, s2, l)	cbranch_int(s1, s2, l, bult)

#define cbranch_int_const_eq(s1, s2, l)	cbranch_int_const(s1, s2, l, beq)
#define cbranch_int_const_ne(s1, s2, l)	cbranch_int_const(s1, s2, l, bne)
#define cbranch_int_const_lt(s1, s2, l)	cbranch_int_const(s1, s2, l, blt)
#define cbranch_int_const_le(s1, s2, l)	cbranch_int_const(s1, s2, l, ble)
#define cbranch_int_const_gt(s1, s2, l)	cbranch_int_const(s1, s2, l, bgt)
#define cbranch_int_const_ge(s1, s2, l)	cbranch_int_const(s1, s2, l, bge)
#define cbranch_int_const_ult(s1, s2, l) cbranch_int_const(s1, s2, l, bult)

#define cbranch_ref_eq(s1, s2, l)	cbranch_ref(s1, s2, l, beq)
#define cbranch_ref_ne(s1, s2, l)	cbranch_ref(s1, s2, l, bne)

#define cbranch_ref_const_eq(s1, s2, l)  cbranch_ref_const(s1, s2, l, beq)
#define cbranch_ref_const_ne(s1, s2, l)  cbranch_ref_const(s1, s2, l, bne)

/* -------------------------------------------------------------------- */
/* Basic blocks and instructions */

#define	start_instruction()	_start_instruction(pc)
#define	start_function()	prologue(meth)
#define	start_basic_block()	_start_basic_block()
#define	end_basic_block()	_end_basic_block(stackno, tmpslot)
#define	end_function()		epilogue()
#define	start_exception_block()	_start_exception_block(stackno)
#define	prepare_function_call()	_prepare_function_call(stackno, tmpslot)
#define	fixup_function_call()	_fixup_function_call()
#define	sync_registers()	_syncRegisters(stackno, tmpslot)

/* -------------------------------------------------------------------- */
/* Conditional monitors */

#define	monitor_enter() \
	(meth->accflags & ACC_SYNCHRONISED ? mon_enter(meth, local(0)) : 0)
#define	monitor_exit()	\
	(meth->accflags & ACC_SYNCHRONISED ? mon_exit(meth, local(0)) : 0)

/* -------------------------------------------------------------------- */
/* Instruction formats */

#define	slot_const_const(dst, src, src2, func, t)			\
	{								\
		void func(sequence*);					\
		_slot_const_const((dst), (src), (src2), (func), (t));	\
	}
void _slot_const_const(SlotInfo*, jword, jword, ifunc, int);

#define	slot_slot_const(dst, src, src2, func, t)			\
	{								\
		void func(sequence*);					\
		_slot_slot_const((dst), (src), (src2), (func), (t));	\
	}
void _slot_slot_const(SlotInfo*, SlotInfo*, jword, ifunc, int);

#define	slot_slot_fconst(dst, src, src2, func, t)			\
	{								\
		void func(sequence*);					\
		_slot_slot_fconst((dst), (src), (src2), (func), (t));	\
	}
void _slot_slot_fconst(SlotInfo*, SlotInfo*, double, ifunc, int);

#define	slot_slot_slot(dst, src, src2, func, t)				\
	{								\
		void func(sequence*);					\
		_slot_slot_slot((dst), (src), (src2), (func), (t));	\
	}
void _slot_slot_slot(SlotInfo*, SlotInfo*, SlotInfo*, ifunc, int);

#define	lslot_lslot_const(dst, src, src2, func, t)			\
	{								\
		void func(sequence*);					\
		_lslot_lslot_const((dst), (src), (src2), (func), (t));	\
	}
void _lslot_lslot_const(SlotInfo*, SlotInfo*, jword, ifunc, int);

#define	lslot_lslot_lconst(dst, src, src2, func, t)			\
	{								\
		void func(sequence*);					\
		_lslot_lslot_lconst((dst), (src), (src2), (func), (t));	\
	}
void _lslot_lslot_lconst(SlotInfo*, SlotInfo*, jlong, ifunc, int);

#define	lslot_slot_lconst(dst, src, src2, func, t)			\
	{								\
		void func(sequence*);					\
		_lslot_slot_lconst((dst), (src), (src2), (func), (t));	\
	}
void _lslot_slot_lconst(SlotInfo*, SlotInfo*, jlong, ifunc, int);

#define	lslot_slot_fconst(dst, src, src2, func, t)			\
	{								\
		void func(sequence*);					\
		_lslot_slot_fconst((dst), (src), (src2), (func), (t));	\
	}
void _lslot_slot_fconst(SlotInfo*, SlotInfo*, double, ifunc, int);

#define	lslot_lslot_lslot(dst, src, src2, func, t)			\
	{								\
		void func(sequence*);					\
		_lslot_lslot_lslot((dst), (src), (src2), (func), (t));	\
	}
void _lslot_lslot_lslot(SlotInfo*, SlotInfo*, SlotInfo*, ifunc, int);

#define	lslot_lslot_slot(dst, src, src2, func, t)			\
	{								\
		void func(sequence*);					\
		_lslot_lslot_slot((dst), (src), (src2), (func), (t));	\
	}
void _lslot_lslot_slot(SlotInfo*, SlotInfo*, SlotInfo*, ifunc, int);

#define	slot_slot_lslot(dst, src, src2, func, t)			\
	{								\
		void func(sequence*);					\
		_slot_slot_lslot((dst), (src), (src2), (func), (t));	\
	}
void _slot_slot_lslot(SlotInfo*, SlotInfo*, SlotInfo*, ifunc, int);

#define	slot_lslot_lslot(dst, src, src2, func, t)			\
	{								\
		void func(sequence*);					\
		_slot_lslot_lslot((dst), (src), (src2), (func), (t));	\
	}
void _slot_lslot_lslot(SlotInfo*, SlotInfo*, SlotInfo*, ifunc, int);

#define	slot_slot_slot_const_const(dst, src, src2, src3, src4, func, t)	\
	{								\
		void func(sequence*);					\
		_slot_slot_slot_const_const((dst), (src), (src2),	\
					    (src3), (src4), (func), (t)); \
	}
void _slot_slot_slot_const_const(SlotInfo*, SlotInfo*, SlotInfo*,
				 jword, jword, ifunc, int);

#define	slot_slot_const_const_const(dst, src, src2, src3, src4, func, t) \
	{								\
		void func(sequence*);					\
		_slot_slot_const_const_const((dst), (src), (src2),	\
					     (src3), (src4), (func), (t)); \
	}
void _slot_slot_const_const_const(SlotInfo*, SlotInfo*, jword,
				  jword, jword, ifunc, int);

/* -------------------------------------------------------------------- */

#endif
