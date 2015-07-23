/* icode.c
 * Define the instructions.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>
 */

#include "config.h"
#include "config-std.h"
#include "gtypes.h"
#include "slots.h"
#include "seq.h"
#include "registers.h"
#include "basecode.h"
#include "labels.h"
#include "constpool.h"
#include "icode.h"
#include "soft.h"
#include "access.h"
#include "object.h"
#include "constants.h"
#include "classMethod.h"
#include "gc.h"
#include "flags.h"
#include "itypes.h"
#include "md.h"
#include "codeproto.h"

#if defined(WORDS_BIGENDIAN)
#define	LSLOT(_s)	((_s)+1)
#define	HSLOT(_s)	(_s)
#else
#define	LSLOT(_s)	(_s)
#define	HSLOT(_s)	((_s)+1)
#endif

void startBlock(sequence*);
void endBlock(sequence*);
void prepareFunctionCall(sequence*);
void fixupFunctionCall(sequence*);
void syncRegisters(sequence*);
void nowritebackSlot(sequence*);

extern uint32 pc;
extern uint32 npc;
extern int maxPush;
extern int maxArgs;
extern int maxTemp;
extern int maxLocal;
extern int maxStack;
extern int isStatic;

bool used_ieee_rounding;
bool used_ieee_division;

#define	MAXLABTAB	64
label* labtab[MAXLABTAB];

/* ----------------------------------------------------------------------- */
/* Register loads and spills.						   */
/*									   */

#if defined(HAVE_spill_int)
void
spill_int(SlotInfo* src)
{
	void HAVE_spill_int(sequence*);
	sequence s;
	seq_dst(&s) = src;
	seq_value(&s, 1) = SLOT2FRAMEOFFSET(src);
	HAVE_spill_int(&s);
}
#endif

#if defined(HAVE_reload_int)
void
reload_int(SlotInfo* dst)
{
	void HAVE_reload_int(sequence*);
	sequence s;
	seq_dst(&s) = dst;
	seq_value(&s, 1) = SLOT2FRAMEOFFSET(dst);
	HAVE_reload_int(&s);
}
#endif

#if defined(HAVE_spill_ref)
void
spill_ref(SlotInfo* src)
{
	void HAVE_spill_ref(sequence*);
	sequence s;
	seq_dst(&s) = src;
	seq_value(&s, 1) = SLOT2FRAMEOFFSET(src);
	HAVE_spill_ref(&s);
}
#endif

#if defined(HAVE_reload_ref)
void
reload_ref(SlotInfo* dst)
{
	void HAVE_reload_ref(sequence*);
	sequence s;
	seq_dst(&s) = dst;
	seq_value(&s, 1) = SLOT2FRAMEOFFSET(dst);
	HAVE_reload_ref(&s);
}
#endif

#if defined(HAVE_spill_long)
void
spill_long(SlotInfo* src)
{
	void HAVE_spill_long(sequence*);
	sequence s;
	seq_dst(&s) = src;
	seq_value(&s, 1) = SLOT2FRAMEOFFSET(src);
	HAVE_spill_long(&s);
}
#endif

#if defined(HAVE_reload_long)
void
reload_long(SlotInfo* dst)
{
	void HAVE_reload_long(sequence*);
	sequence s;
	seq_dst(&s) = dst;
	seq_value(&s, 1) = SLOT2FRAMEOFFSET(dst);
	HAVE_reload_long(&s);
}
#endif

#if defined(HAVE_spill_float)
void
spill_float(SlotInfo* src)
{
	void HAVE_spill_float(sequence*);
	sequence s;
	seq_dst(&s) = src;
	seq_value(&s, 1) = SLOT2FRAMEOFFSET(src);
	HAVE_spill_float(&s);
}
#endif

#if defined(HAVE_reload_float)
void
reload_float(SlotInfo* dst)
{
	void HAVE_reload_float(sequence*);
	sequence s;
	seq_dst(&s) = dst;
	seq_value(&s, 1) = SLOT2FRAMEOFFSET(dst);
	HAVE_reload_float(&s);
}
#endif

#if defined(HAVE_spill_double)
void
spill_double(SlotInfo* src)
{
	void HAVE_spill_double(sequence*);
	sequence s;
	seq_dst(&s) = src;
	seq_value(&s, 1) = SLOT2FRAMEOFFSET(src);
	HAVE_spill_double(&s);
}
#endif

#if defined(HAVE_reload_double)
void
reload_double(SlotInfo* dst)
{
	void HAVE_reload_double(sequence*);
	sequence s;
	seq_dst(&s) = dst;
	seq_value(&s, 1) = SLOT2FRAMEOFFSET(dst);
	HAVE_reload_double(&s);
}
#endif

/* ----------------------------------------------------------------------- */
/* Prologues and epilogues.						   */
/*									   */

void
prologue(Method* meth)
{
	label* l;

	used_ieee_rounding = false;

	l = newLabel();
	l->type = Lnull; /* Lframe|Labsolute|Lgeneral */
	l->at = 0;
	l->to = 0;
	l->from = 0;

	/* Emit prologue code */
	slot_const_const(0, (jword)l, (jword)meth, HAVE_prologue, Tnull);
}

void
exception_prologue(void)
{
	label* l;

	l = newLabel();
	l->type = Lnull; /* Lframe|Labsolute|Lgeneral */
	l->at = 0;
	l->to = 0;
	l->from = 0;

	/* Emit exception prologue code */
	slot_const_const(0, (jword)l, 0, HAVE_exception_prologue, Tnull);
}

void
epilogue(void)
{
	slot_slot_slot(0, 0, 0, HAVE_epilogue, Tnull);
}


/* ----------------------------------------------------------------------- */
/* Conditional monitor management.					   */
/*									   */

void
mon_enter(methods* meth, SlotInfo* obj)
{
	/* Emit monitor entry if required */
	end_basic_block();
	if ((meth->accflags & ACC_STATIC) != 0) {
		pusharg_ref_const(meth->class, 0);
	}
	else {
		pusharg_ref(obj, 0);
	}
	call_soft(soft_monitorenter);
	popargs();
	start_basic_block();
}

void
mon_exit(methods* meth, SlotInfo* obj)
{
	end_basic_block();
	if ((meth->accflags & ACC_STATIC) != 0) {
		pusharg_ref_const(meth->class, 0);
	}
	else {
		pusharg_ref(obj, 0);
	}
	call_soft(soft_monitorexit);
	popargs();
	start_basic_block();
}

/* ----------------------------------------------------------------------- */
/* Basic block and instruction management.				   */
/*									   */

void
_start_basic_block(void)
{
	_slot_const_const(0, 0, 0, startBlock, Tnull);
}

void
_end_basic_block(uintp stk, uintp temp)
{
	_slot_const_const(0, stk, temp, endBlock, Tnull);
}

void
_start_instruction(uintp pc)
{
	void startInsn(sequence*);

	_slot_const_const(0, 0, pc, startInsn, Tnull);
}

void
_start_exception_block(uintp stk)
{
	/* Exception blocks act like function returns - the return
	 * value is the exception object.
	 */
	start_basic_block();
	exception_prologue();
	return_ref(&localinfo[stk]);
}

void
_fixup_function_call(void)
{
	_slot_const_const(0, 0, 0, fixupFunctionCall, Tnull);
}

void
_prepare_function_call(uintp stk, uintp temp)
{
	_slot_const_const(0, stk, temp, prepareFunctionCall, Tnull);
}

void
_slot_nowriteback(SlotInfo* slt)
{
	_slot_const_const(slt, 0, 0, nowritebackSlot, Tnull);
}

void 
_syncRegisters(uintp stk, uintp temp)
{
	_slot_const_const(0, stk, temp, syncRegisters, Tnull);
}


/* ----------------------------------------------------------------------- */
/* Moves.								   */
/*									   */

void
move_int_const(SlotInfo* dst, jint val)
{
#if defined(HAVE_move_int_const)
	if (HAVE_move_int_const_rangecheck(val)) {
		slot_slot_const(dst, 0, val, HAVE_move_int_const, Tconst);
		dst->v.tint = val;
	}
	else
#endif
	{
		constpool *c;
		label* l;
		SlotInfo* tmp;

		c = newConstant(CPint, val);
		l = newLabel();
		l->type = Lconstant;
		l->at = 0;
		l->to = (uintp)c;
		l->from = 0;

		slot_alloctmp(tmp);
		move_label_const(tmp, l);
		load_int(dst, tmp);
	}
}

void
move_ref_const(SlotInfo* dst, void *val)
{
#if defined(HAVE_move_ref_const)
	if (HAVE_move_ref_const_rangecheck(val)) {
		slot_slot_const(dst, 0, (jword)val, HAVE_move_ref_const, Tconst);
		dst->v.tint = (jword)val;
	}
	else
#endif
	{
		constpool *c;
		label* l;
		SlotInfo* tmp;

		c = newConstant(CPref, val);
		l = newLabel();
		l->type = Lconstant;
		l->at = 0;
		l->to = (uintp)c;
		l->from = 0;

		slot_alloctmp(tmp);
		move_label_const(tmp, l);
		load_ref(dst, tmp);
	}
}

void
move_long_const(SlotInfo* dst, jlong val)
{
#if defined(HAVE_move_long_const)
	if (HAVE_move_long_const_rangecheck(val)) {
		lslot_slot_lconst(dst, 0, val, HAVE_move_long_const, Tconst);
		dst->v.tlong = val;
	}
	else {
		constpool *c;
		label* l;
		SlotInfo* tmp;

		c = newConstant(CPlong, val);
		l = newLabel();
		l->type = Lconstant;
		l->at = 0;
		l->to = (uintp)c;
		l->from = 0;

		slot_alloctmp(tmp);
		move_label_const(tmp, l);
		load_long(dst, tmp);
	}
#else
	move_int_const(LSLOT(dst), (jint)(val & 0xFFFFFFFF));
	move_int_const(HSLOT(dst), (jint)((val >> 32) & 0xFFFFFFFF));
#endif
}

void
move_float_const(SlotInfo* dst, float val)
{
#if defined(HAVE_move_float_const)
	if (HAVE_move_float_const_rangecheck(val)) {
		slot_slot_fconst(dst, 0, val, HAVE_move_float_const, Tconst);
		dst->v.tdouble = val;
	}
	else
#endif
	{
		constpool *c;
		label* l;
		SlotInfo* tmp;

		c = newConstant(CPfloat, val);
		l = newLabel();
		l->type = Lconstant;
		l->at = 0;
		l->to = (uintp)c;
		l->from = 0;

		slot_alloctmp(tmp);
		move_label_const(tmp, l);
		load_float(dst, tmp);
	}
}

void
move_double_const(SlotInfo* dst, jdouble val)
{
#if defined(HAVE_move_double_const)
	if (HAVE_move_double_const_rangecheck(val)) {
		lslot_slot_fconst(dst, 0, val, HAVE_move_double_const, Tconst);
		dst->v.tdouble = val;
	}
	else
#endif
	{
		constpool *c;
		label* l;
		SlotInfo* tmp;

		c = newConstant(CPdouble, val);
		l = newLabel();
		l->type = Lconstant;
		l->at = 0;
		l->to = (uintp)c;
		l->from = 0;

		slot_alloctmp(tmp);
		move_label_const(tmp, l);
		load_double(dst, tmp);
	}
}

#if defined(HAVE_move_any)
void
move_any(SlotInfo* dst, SlotInfo* src)
{
	slot_slot_slot(dst, 0, src, HAVE_move_any, Tcopy);
}
#endif

#if defined(HAVE_move_int)
void
move_int(SlotInfo* dst, SlotInfo* src)
{
	slot_slot_slot(dst, 0, src, HAVE_move_int, Tcopy);
}
#endif

void
move_ref(SlotInfo* dst, SlotInfo* src)
{
	slot_slot_slot(dst, 0, src, HAVE_move_ref, Tcopy);
}

void
move_anylong(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_move_anylong)
	lslot_lslot_lslot(dst, 0, src, HAVE_move_anylong, Tcopy);
#else
	assert(LSLOT(dst) != HSLOT(src));
	move_any(LSLOT(dst), LSLOT(src));
	move_any(HSLOT(dst), HSLOT(src));
#endif
}

void
move_long(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_move_long)
	lslot_lslot_lslot(dst, 0, src, HAVE_move_long, Tcopy);
#else
	assert(LSLOT(dst) != HSLOT(src));
	move_int(LSLOT(dst), LSLOT(src));
	move_int(HSLOT(dst), HSLOT(src));
#endif
}

#if defined(HAVE_move_float)
void
move_float(SlotInfo* dst, SlotInfo* src)
{
	slot_slot_slot(dst, 0, src, HAVE_move_float, Tcopy);
}
#endif

#if defined(HAVE_move_double)
void
move_double(SlotInfo* dst, SlotInfo* src)
{
	lslot_lslot_lslot(dst, 0, src, HAVE_move_double, Tcopy);
}
#endif

#if defined(HAVE_move_label_const)
void
move_label_const(SlotInfo* dst, label* lab)
{
	slot_slot_const(dst, 0, (jword)lab, HAVE_move_label_const, Tnull);
}
#endif

void
swap_any(SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_swap_any)
	slot_slot_slot(src, 0, src2, HAVE_swap_any, Tcomplex);
#else
	SlotInfo* tmp;
	slot_alloctmp(tmp);
	move_ref(tmp, src);
	move_ref(src, src2);
	move_ref(src2, tmp);
#endif
}

/* ----------------------------------------------------------------------- */
/* Arithmetic operators - add, sub, etc.				   */
/*									   */


#if defined(HAVE_adc_int)
void
adc_int(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
	slot_slot_slot(dst, src, src2, HAVE_adc_int, Tcomplex);
}
#endif

void
add_int_const(SlotInfo* dst, SlotInfo* src, jint val)
{
#if defined(HAVE_add_int_const)
	if (HAVE_add_int_const_rangecheck(val)) {
		slot_slot_const(dst, src, val, HAVE_add_int_const, Taddregconst);
	}
	else
#endif
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		move_int_const(tmp, val);
		add_int(dst, src, tmp);
	}
}

#if defined(HAVE_add_int)
void
add_int(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
	slot_slot_slot(dst, src, src2, HAVE_add_int, Tcomplex);
}
#endif

#if defined(HAVE_add_ref)
void
add_ref(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
	slot_slot_slot(dst, src, src2, HAVE_add_ref, Tcomplex);
}
#endif

void
add_ref_const(SlotInfo* dst, SlotInfo* src, jint val)
{
#if defined(HAVE_add_ref_const)
	if (HAVE_add_ref_const_rangecheck(val)) {
		slot_slot_const(dst, src, val, HAVE_add_ref_const, Taddregconst);
	}
	else
#endif
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		move_int_const(tmp, val);
		add_ref(dst, src, tmp);
	}
}

void
add_long(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_add_long)
	lslot_lslot_lslot(dst, src, src2, HAVE_add_long, Tcomplex);
#else
	add_int(LSLOT(dst), LSLOT(src), LSLOT(src2));
	adc_int(HSLOT(dst), HSLOT(src), HSLOT(src2));
#endif
}

#if defined(HAVE_add_long_const)
void
add_long_const(SlotInfo* dst, SlotInfo* src, jlong val)
{
	if (HAVE_add_long_const_rangecheck(val)) {
		lslot_lslot_lconst(dst, src, val, HAVE_add_long_const, Taddregconst);
	}
	else
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		move_long_const(tmp, val);
		add_long(dst, src, tmp);
	}
}
#endif

#if defined(HAVE_add_float)
void
add_float(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
	slot_slot_slot(dst, src, src2, HAVE_add_float, Tcomplex);
}
#endif

#if defined(HAVE_add_double)
void
add_double(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
	lslot_lslot_lslot(dst, src, src2, HAVE_add_double, Tcomplex);
}
#endif

#if defined(HAVE_sbc_int)
void
sbc_int(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
	slot_slot_slot(dst, src, src2, HAVE_sbc_int, Tcomplex);
}
#endif

void
sub_int_const(SlotInfo* dst, SlotInfo* src, jint val)
{
#if defined(HAVE_sub_int_const)
	if (HAVE_sub_int_const_rangecheck(val)) {
		slot_slot_const(dst, src, val, HAVE_sub_int_const, Tcomplex);
	}
	else
#endif
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		move_int_const(tmp, val);
		sub_int(dst, src, tmp);
	}
}

#if defined(HAVE_sub_int)
void
sub_int(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
	slot_slot_slot(dst, src, src2, HAVE_sub_int, Tcomplex);
}
#endif

void
sub_long(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_sub_long)
	lslot_lslot_lslot(dst, src, src2, HAVE_sub_long, Tcomplex);
#else
	sub_int(LSLOT(dst), LSLOT(src), LSLOT(src2));
	sbc_int(HSLOT(dst), HSLOT(src), HSLOT(src2));
#endif
}

#if defined(HAVE_sub_long_const)
void
sub_long_const(SlotInfo* dst, SlotInfo* src, jlong val)
{
	if (HAVE_sub_long_const_rangecheck(val)) {
		lslot_lslot_lconst(dst, src, val, HAVE_sub_long_const, Tcomplex);
	}
	else
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		move_long_const(tmp, val);
		sub_long(dst, src, tmp);
	}
}
#endif

#if defined(HAVE_sub_float)
void
sub_float(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
	slot_slot_slot(dst, src, src2, HAVE_sub_float, Tcomplex);
}
#endif

#if defined(HAVE_sub_double)
void
sub_double(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
	lslot_lslot_lslot(dst, src, src2, HAVE_sub_double, Tcomplex);
}
#endif

void
mul_int_const(SlotInfo* dst, SlotInfo* src, jint val)
{
#if defined(HAVE_mul_int_const)
	if (HAVE_mul_int_const_rangecheck(val)) {
		slot_slot_const(dst, src, val, HAVE_mul_int_const, Tcomplex);
	}
	else
#endif
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		move_int_const(tmp, val);
		mul_int(dst, src, tmp);
	}
}

void
mul_int(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_mul_int)
	slot_slot_slot(dst, src, src2, HAVE_mul_int, Tcomplex);
#else
	end_basic_block();
	pusharg_int(src2, 1);
	pusharg_int(src, 0);
	call_soft(soft_mul);
	popargs();
	start_basic_block();
	return_int(dst);
#endif
}

void
mul_long(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_mul_long)
	lslot_lslot_lslot(dst, src, src2, HAVE_mul_long, Tcomplex);
#else
	end_basic_block();
	pusharg_long(src2, pusharg_long_idx_inc);
	pusharg_long(src, 0);
	call_soft(soft_lmul);
	popargs();
	start_basic_block();
	return_long(dst);
#endif
}

#if defined(HAVE_mul_long_const)
void
mul_long_const(SlotInfo* dst, SlotInfo* src, jlong val)
{
	if (HAVE_mul_long_const_rangecheck(val)) {
		lslot_lslot_lconst(dst, src, val, HAVE_mul_long_const, Tcomplex);
	}
	else
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		move_long_const(tmp, val);
		mul_long(dst, src, tmp);
	}
}
#endif

#if defined(HAVE_mul_float)
void
mul_float(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
	slot_slot_slot(dst, src, src2, HAVE_mul_float, Tcomplex);
}
#endif

#if defined(HAVE_mul_double)
void
mul_double(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
	lslot_lslot_lslot(dst, src, src2, HAVE_mul_double, Tcomplex);
}
#endif

void
div_int(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_div_int)
	slot_slot_slot(dst, src, src2, HAVE_div_int, Tcomplex);
#else
	end_basic_block();
	pusharg_int(src2, 1);
	pusharg_int(src, 0);
	call_soft(soft_div);
	popargs();
	start_basic_block();
	return_int(dst);
#endif
}

void
div_long(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_div_long)
	lslot_lslot_lslot(dst, src, src2, HAVE_div_long, Tcomplex);
#else
	end_basic_block();
	pusharg_long(src2, pusharg_long_idx_inc);
	pusharg_long(src, 0);
	call_soft(soft_ldiv);
	popargs();
	start_basic_block();
	return_long(dst);
#endif
}

void
div_float(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_div_float)
	used_ieee_division = true;
	slot_slot_slot(dst, src, src2, HAVE_div_float, Tcomplex);
#else
	end_basic_block();
	pusharg_float(src2, 1);
	pusharg_float(src, 0);
	call_soft(soft_fdiv);
	popargs();
	start_basic_block();
	return_float(dst);
#endif
}

void
div_double(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_div_double)
	used_ieee_division = true;
	lslot_lslot_lslot(dst, src, src2, HAVE_div_double, Tcomplex);
#else
	end_basic_block();
	pusharg_double(src2, pusharg_long_idx_inc);
	pusharg_double(src, 0);
	call_soft(soft_fdivl);
	popargs();
	start_basic_block();
	return_double(dst);
#endif
}

void
rem_int(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_rem_int)
	slot_slot_slot(dst, src, src2, HAVE_rem_int, Tcomplex);
#else
	end_basic_block();
	pusharg_int(src2, 1);
	pusharg_int(src, 0);
	call_soft(soft_rem);
	popargs();
	start_basic_block();
	return_int(dst);
#endif
}

void
rem_long(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_rem_long)
	lslot_lslot_lslot(dst, src, src2, HAVE_rem_long, Tcomplex);
#else
	end_basic_block();
	pusharg_long(src2, pusharg_long_idx_inc);
	pusharg_long(src, 0);
	call_soft(soft_lrem);
	popargs();
	start_basic_block();
	return_long(dst);
#endif
}

void
rem_float(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_rem_float)
	used_ieee_division = true;
	slot_slot_slot(dst, src, src2, HAVE_rem_float, Tcomplex);
#else
	end_basic_block();
	pusharg_float(src2, 1);
	pusharg_float(src, 0);
	call_soft(soft_frem);
	popargs();
	end_basic_block();
	return_float(dst);
#endif
}

void
rem_double(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_rem_double)
	used_ieee_division = true;
	lslot_lslot_lslot(dst, src, src2, HAVE_rem_double, Tcomplex);
#else
	end_basic_block();
	pusharg_double(src2, pusharg_long_idx_inc);
	pusharg_double(src, 0);
	call_soft(soft_freml);
	popargs();
	end_basic_block();
	return_double(dst);
#endif
}

void
neg_int(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_neg_int)
	slot_slot_slot(dst, 0, src, HAVE_neg_int, Tcomplex);
#else
	SlotInfo* zero;
	slot_alloctmp(zero);
	move_int_const(zero, 0);
	sub_int(dst, zero, src);
#endif
}

#if defined(HAVE_ngc_int)
void
ngc_int(SlotInfo* dst, SlotInfo* src)
{
	slot_slot_slot(dst, 0, src, HAVE_ngc_int, Tcomplex);
}
#endif

void
neg_long(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_neg_long)
	lslot_lslot_lslot(dst, 0, src, HAVE_neg_long, Tcomplex);
#elif defined(HAVE_ngc_int)
	neg_int(LSLOT(dst), LSLOT(src));
	ngc_int(HSLOT(dst), HSLOT(src));
#elif defined(HAVE_sbc_int)
	SlotInfo* zero;
	slot_alloctmp(zero);
	move_int_const(zero, 0);
	sub_int(LSLOT(dst), zero, LSLOT(src));
	sbc_int(HSLOT(dst), zero, HSLOT(src));
#elif defined(HAVE_adc_int_const)
	neg_int(LSLOT(dst), LSLOT(src));
	adc_int_const(HSLOT(dst), HSLOT(src), 0);
	neg_int(HSLOT(dst), HSLOT(dst));
#else
	ABORT();
#endif
}

void
neg_float(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_neg_float)
	lslot_lslot_lslot(dst, 0, src, HAVE_neg_float, Tcomplex);
#else
	SlotInfo* tmp;
	slot_alloctmp(tmp);
	move_float_const(tmp, 0);
	sub_float(dst, tmp, src);
#endif
}

void
neg_double(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_neg_double)
	lslot_lslot_lslot(dst, 0, src, HAVE_neg_double, Tcomplex);
#else
	SlotInfo* tmp;
	slot_alloc2tmp(tmp);
	move_double_const(tmp, 0);
	sub_double(dst, tmp, src);
#endif
}


/* ----------------------------------------------------------------------- */
/* Logical operators - and, or, etc.					   */
/*									   */

void
and_int_const(SlotInfo* dst, SlotInfo* src, jint val)
{
#if defined(HAVE_and_int_const)
	if (HAVE_and_int_const_rangecheck(val)) {
		slot_slot_const(dst, src, val, HAVE_and_int_const, Tcomplex);
	}
	else
#endif
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		move_int_const(tmp, val);
		and_int(dst, src, tmp);
	}
}

#if defined(HAVE_and_int)
void
and_int(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
	slot_slot_slot(dst, src, src2, HAVE_and_int, Tcomplex);
}
#endif

void
and_long(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_and_long)
	lslot_lslot_lslot(dst, src, src2, HAVE_and_long, Tcomplex);
#else
	and_int(LSLOT(dst), LSLOT(src), LSLOT(src2));
	and_int(HSLOT(dst), HSLOT(src), HSLOT(src2));
#endif
}

#if defined(HAVE_and_long_const)
void
and_long_const(SlotInfo* dst, SlotInfo* src, jlong val)
{
	if (HAVE_and_long_const_rangecheck(val)) {
		lslot_lslot_lconst(dst, src, val, HAVE_and_long_const, Tcomplex);
	}
	else
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		move_long_const(tmp, val);
		and_long(dst, src, tmp);
	}
}
#endif

#if defined(HAVE_or_int)
void
or_int(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
	slot_slot_slot(dst, src, src2, HAVE_or_int, Tcomplex);
}
#endif

void
or_long(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_or_long)
	lslot_lslot_lslot(dst, src, src2, HAVE_or_long, Tcomplex);
#else
	or_int(LSLOT(dst), LSLOT(src), LSLOT(src2));
	or_int(HSLOT(dst), HSLOT(src), HSLOT(src2));
#endif
}

#if defined(HAVE_xor_int)
void
xor_int(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
	slot_slot_slot(dst, src, src2, HAVE_xor_int, Tcomplex);
}
#endif

void
xor_long(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_xor_long)
	lslot_lslot_lslot(dst, src, src2, HAVE_xor_long, Tcomplex);
#else
	xor_int(LSLOT(dst), LSLOT(src), LSLOT(src2));
	xor_int(HSLOT(dst), HSLOT(src), HSLOT(src2));
#endif
}

void
lshl_int_const(SlotInfo* dst, SlotInfo* src, jint val)
{
#if defined(HAVE_lshl_int_const)
	if (HAVE_lshl_int_const_rangecheck(val)) {
		slot_slot_const(dst, src, val, HAVE_lshl_int_const, Tcomplex);
	}
	else
#endif
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		move_int_const(tmp, val);
		lshl_int(dst, src, tmp);
	}
}

#if defined(HAVE_lshl_int)
void
lshl_int(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
	slot_slot_slot(dst, src, src2, HAVE_lshl_int, Tcomplex);
}
#endif

#if defined(HAVE_lshl_long_const)
void
lshl_long_const(SlotInfo* dst, SlotInfo* src, jint val)
{
	if (HAVE_lshl_long_const_rangecheck(val)) {
		slot_slot_const(dst, src, val, HAVE_lshl_long_const, Tcomplex);
	}
	else
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		move_int_const(tmp, val);
		lshl_int(dst, src, tmp);
	}
}
#endif

void
lshl_long(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_lshl_long)
	lslot_lslot_slot(dst, src, src2, HAVE_lshl_long, Tcomplex);
#else
	end_basic_block();
	pusharg_int(src2, pusharg_long_idx_inc);
	pusharg_long(src, 0);
	call_soft(soft_lshll);
	popargs();
	start_basic_block();
	return_long(dst);
#endif
}

void
ashr_int_const(SlotInfo* dst, SlotInfo* src, jint val)
{
#if defined(HAVE_ashr_int_const)
	if (HAVE_ashr_int_const_rangecheck(val)) {
		slot_slot_const(dst, src, val, HAVE_ashr_int_const, Tcomplex);
	}
	else
#endif
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		move_int_const(tmp, val);
		ashr_int(dst, src, tmp);
	}
}

#if defined(HAVE_ashr_int)
void
ashr_int(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
	slot_slot_slot(dst, src, src2, HAVE_ashr_int, Tcomplex);
}
#endif

void
ashr_long(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_ashr_long)
	lslot_lslot_slot(dst, src, src2, HAVE_ashr_long, Tcomplex);
#else
	end_basic_block();
	pusharg_int(src2, pusharg_long_idx_inc);
	pusharg_long(src, 0);
	call_soft(soft_ashrl);
	popargs();
	start_basic_block();
	return_long(dst);
#endif
}

void
lshr_int_const(SlotInfo* dst, SlotInfo* src, jint val)
{
#if defined(HAVE_lshr_int_const)
	if (HAVE_lshr_int_const_rangecheck(val)) {
		slot_slot_const(dst, src, val, HAVE_lshr_int_const, Tcomplex);
	}
	else
#endif
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		move_int_const(tmp, val);
		lshr_int(dst, src, tmp);
	}
}

#if defined(HAVE_lshr_int)
void
lshr_int(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
	slot_slot_slot(dst, src, src2, HAVE_lshr_int, Tcomplex);
}
#endif

void
lshr_long(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_lshr_long)
	lslot_lslot_slot(dst, src, src2, HAVE_lshr_long, Tcomplex);
#else
	end_basic_block();
	pusharg_int(src2, pusharg_long_idx_inc);
	pusharg_long(src, 0);
	call_soft(soft_lshrl);
	popargs();
	start_basic_block();
	return_long(dst);
#endif
}




/* ----------------------------------------------------------------------- */
/* Load and store.							   */
/*									   */

#if defined(HAVE_load_int)
void
load_int(SlotInfo* dst, SlotInfo* src)
{
	slot_slot_slot(dst, 0, src, HAVE_load_int, Tload);
}
#endif

void
load_offset_int(SlotInfo* dst, SlotInfo* src, jint offset)
{
	if (offset == 0) {
		load_int(dst, src);
	}
	else
#if defined(HAVE_load_offset_int)
	if (HAVE_load_offset_int_rangecheck(offset)) {
		slot_slot_const(dst, src, offset, HAVE_load_offset_int, Tload);
	}
	else
#endif
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		add_ref_const(tmp, src, offset);
		load_int(dst, tmp);
	}
}

#if defined(HAVE_load_ref)
void
load_ref(SlotInfo* dst, SlotInfo* src)
{
	slot_slot_slot(dst, 0, src, HAVE_load_ref, Tload);
}
#endif

#if defined(HAVE_load_offset_ref)
void
load_offset_ref(SlotInfo* dst, SlotInfo* src, jint offset)
{
	if (offset == 0) {
		load_ref(dst, src);
	}
	else
#if defined(HAVE_load_offset_ref)
	if (HAVE_load_offset_ref_rangecheck(offset)) {
		slot_slot_const(dst, src, offset, HAVE_load_offset_ref, Tload);
	}
	else
#endif
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		add_ref_const(tmp, src, offset);
		load_ref(dst, tmp);
	}
}
#endif

void
load_long(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_load_long)
	lslot_lslot_lslot(dst, 0, src, HAVE_load_long, Tload);
#else
	SlotInfo* tmp;

	slot_alloctmp(tmp);
	add_ref_const(tmp, src, 4);
	/* Don't use LSLOT & HSLOT here */
	load_int(dst, src);
	load_int(dst+1, tmp);
#endif
}

#if defined(HAVE_load_offset_long)
void
load_offset_long(SlotInfo* dst, SlotInfo* src, jint offset)
{
	if (offset == 0) {
		load_long(dst, src);
	}
	else
	if (HAVE_load_offset_long_rangecheck(offset)) {
		lslot_lslot_const(dst, src, offset, HAVE_load_offset_long, Tload);
	}
	else
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		add_ref_const(tmp, src, offset);
		load_long(dst, tmp);
	}
}
#endif

#if defined(HAVE_load_float)
void
load_float(SlotInfo* dst, SlotInfo* src)
{
	slot_slot_slot(dst, 0, src, HAVE_load_float, Tload);
}
#endif

#if defined(HAVE_load_double)
void
load_double(SlotInfo* dst, SlotInfo* src)
{
	lslot_lslot_slot(dst, 0, src, HAVE_load_double, Tload);
}
#endif

void
load_byte(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_load_byte)
	slot_slot_slot(dst, 0, src, HAVE_load_byte, Tload);
#else
	load_int(dst, src);
	lshl_int_const(dst, dst, 8 * (sizeof(jint) - sizeof(jbyte)));
	ashr_int_const(dst, dst, 8 * (sizeof(jint) - sizeof(jbyte)));
#endif
}

void
load_char(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_load_char)
	slot_slot_slot(dst, 0, src, HAVE_load_char, Tload);
#else
	load_int(dst, src);
	and_int_const(dst, dst, (1 << (8 * sizeof(jchar))) - 1);
#endif
}

void
load_short(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_load_short)
	slot_slot_slot(dst, 0, src, HAVE_load_short, Tload);
#else
	load_int(dst, src);
	lshl_int_const(dst, dst, 8 * (sizeof(jint) - sizeof(jshort)));
	ashr_int_const(dst, dst, 8 * (sizeof(jint) - sizeof(jshort)));
#endif
}

void
load_code_ref(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_load_code_ref)
	slot_slot_slot(dst, 0, src, HAVE_load_code_ref, Tload);
#else
	load_ref(dst, src);
#endif
}

void
load_key(SlotInfo* dst, SlotInfo* src)
{
	load_int(dst, src);
}

#if defined(HAVE_store_int)
void
store_int(SlotInfo* dst, SlotInfo* src)
{
	slot_slot_slot(0, dst, src, HAVE_store_int, Tstore);
}
#endif

void
store_offset_int(SlotInfo* dst, jint offset, SlotInfo* src)
{
	if (offset == 0) {
		store_int(dst, src);
	}
	else
#if defined(HAVE_store_offset_int)
	if (HAVE_store_offset_int_rangecheck(offset)) {
		slot_slot_const(src, dst, offset, HAVE_store_offset_int, Tstore);
	}
	else
#endif
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		add_ref_const(tmp, dst, offset);
		store_int(tmp, src);
	}
}

#if defined(HAVE_store_ref)
void
store_ref(SlotInfo* dst, SlotInfo* src)
{
	slot_slot_slot(0, dst, src, HAVE_store_ref, Tstore);
}
#endif

void
store_offset_ref(SlotInfo* dst, jint offset, SlotInfo* src)
{
	if (offset == 0) {
		store_ref(dst, src);
	}
	else
#if defined(HAVE_store_offset_ref)
	if (HAVE_store_offset_ref_rangecheck(offset)) {
		slot_slot_const(src, dst, offset, HAVE_store_offset_ref, Tstore);
	}
	else
#endif
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		add_ref_const(tmp, dst, offset);
		store_ref(tmp, src);
	}
}

void
store_long(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_store_long)
	lslot_lslot_lslot(0, dst, src, HAVE_store_long, Tstore);
#else
	SlotInfo* tmp;

	slot_alloctmp(tmp);
	add_ref_const(tmp, dst, 4);
	/* Don't use LSLOT & HSLOT here */
	store_int(dst, src);
	store_int(tmp, src+1);
#endif
}

#if defined(HAVE_store_offset_long)
void
store_offset_long(SlotInfo* dst, jint offset, SlotInfo* src)
{
	if (offset == 0) {
		store_long(dst, src);
	}
	else
	if (HAVE_store_offset_long_rangecheck(offset)) {
		lslot_lslot_const(src, dst, offset, HAVE_store_offset_long, Tstore);
	}
	else
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		add_ref_const(tmp, dst, offset);
		store_int(tmp, src);
	}
}
#endif

#if defined(HAVE_store_float)
void
store_float(SlotInfo* dst, SlotInfo* src)
{
	slot_slot_slot(0, dst, src, HAVE_store_float, Tstore);
}
#endif

#if defined(HAVE_store_double)
void
store_double(SlotInfo* dst, SlotInfo* src)
{
	slot_slot_lslot(0, dst, src, HAVE_store_double, Tstore);
}
#endif

void
store_byte(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_store_byte)
	slot_slot_slot(0, dst, src, HAVE_store_byte, Tstore);
#else
	/* FIXME -- this is unlikely to work as-is as it doesn't
	   allow for alignment requirements on the integer load.  */
	SlotInfo* tmp;
	SlotInfo* tmp2;
	slot_alloctmp(tmp);
	slot_alloctmp(tmp2);
	and_int_const(tmp, src, (1 << (8 * sizeof(jbyte))) - 1);
	load_int(tmp2, dst);
	and_int_const(tmp2, tmp2, -(1 << (8 * sizeof(jbyte))));
	or_int(tmp2, tmp2, tmp);
	store_int(dst, tmp2);
#endif
}

void
store_char(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_store_char)
	slot_slot_slot(0, dst, src, HAVE_store_char, Tstore);
#else
	/* FIXME -- this is unlikely to work as-is as it doesn't
	   allow for alignment requirements on the integer load.  */
	SlotInfo* tmp;
	SlotInfo* tmp2;
	slot_alloctmp(tmp);
	slot_alloctmp(tmp2);
	and_int_const(tmp, src, (1 << (8 * sizeof(jchar))) - 1);
	load_int(tmp2, dst);
	and_int_const(tmp2, tmp2, -(1 << (8 * sizeof(jchar))));
	or_int(tmp2, tmp2, tmp);
	store_int(dst, tmp2);
#endif
}

void
store_short(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_store_short)
	slot_slot_slot(0, dst, src, HAVE_store_short, Tstore);
#else
	/* FIXME -- this is unlikely to work as-is as it doesn't
	   allow for alignment requirements on the integer load.  */
	SlotInfo* tmp;
	SlotInfo* tmp2;
	slot_alloctmp(tmp);
	slot_alloctmp(tmp2);
	and_int_const(tmp, src, (1 << (8 * sizeof(jshort))) - 1);
	load_int(tmp2, dst);
	and_int_const(tmp2, tmp2, -(1 << (8 * sizeof(jshort))));
	or_int(tmp2, tmp2, tmp);
	store_int(dst, tmp2);
#endif
}


/* ----------------------------------------------------------------------- */
/* Function argument management.					   */
/*									   */

void
pusharg_int_const(int val, int idx)
{
#if defined(HAVE_pusharg_int_const)
	if (HAVE_pusharg_int_const_rangecheck(val)) {
		slot_const_const(0, val, idx, HAVE_pusharg_int_const, Tnull);
		argcount += 1;
	}
	else
#endif
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		move_int_const(tmp, val);
		pusharg_int(tmp, idx);
	}
}

#if defined(HAVE_pusharg_int)
void
pusharg_int(SlotInfo* src, int idx)
{
	slot_slot_const(0, src, idx, HAVE_pusharg_int, Tnull);
	argcount += 1;
}
#endif

#if defined(HAVE_pusharg_ref)
void
pusharg_ref(SlotInfo* src, int idx)
{
	slot_slot_const(0, src, idx, HAVE_pusharg_ref, Tnull);
	argcount += 1;
}
#endif

void
pusharg_ref_const(void* val, int idx)
{
#if defined(HAVE_pusharg_ref_const)
	if (HAVE_pusharg_ref_const_rangecheck((jword)val)) {
		slot_const_const(0, (jword)val, idx, HAVE_pusharg_ref_const, Tnull);
		argcount += 1;
	}
	else
#endif
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		move_ref_const(tmp, val);
		pusharg_ref(tmp, idx);
	}
}

#if defined(HAVE_pusharg_float)
void
pusharg_float(SlotInfo* src, int idx)
{
	slot_slot_const(0, src, idx, HAVE_pusharg_float, Tnull);
	argcount += 1;
}
#endif

#if defined(HAVE_pusharg_double)
void
pusharg_double(SlotInfo* src, int idx)
{
	lslot_lslot_const(0, src, idx, HAVE_pusharg_double, Tnull);
	argcount += pusharg_long_idx_inc;
}
#endif

void
pusharg_long(SlotInfo* src, int idx)
{
#if defined(HAVE_pusharg_long)
	lslot_lslot_const(0, src, idx, HAVE_pusharg_long, Tnull);
	argcount += pusharg_long_idx_inc;
#else
	/* Don't use LSLOT & HSLOT here */
	pusharg_int(src+1, idx+1);
	pusharg_int(src, idx);
#endif
}

void
popargs(void)
{
 	if (argcount != 0) {
#if defined(HAVE_popargs)
		slot_slot_const(0, 0, argcount, HAVE_popargs, Tnull);
#endif
		if (argcount > maxPush) {
			maxPush = argcount;
		}
		argcount = 0;
	}
}



/* ----------------------------------------------------------------------- */
/* Control flow changes.						   */
/*									   */

#if defined(HAVE_branch)
void
branch(label* dst, int type)
{
	slot_const_const(0, (jword)dst, type, HAVE_branch, Tnull);
}
#endif

void
cbranch_int(SlotInfo* s1, SlotInfo* s2, label* dst, int type)
{
#if defined(HAVE_cbranch_int)
	slot_slot_slot_const_const(0, s1, s2, (jword)dst, type,
				   HAVE_cbranch_int, Tcomplex);
#else
	cmp_int(0, s1, s2);
	branch(dst, type);
#endif
}

void
cbranch_int_const(SlotInfo* s1, jint val, label* dst, int type)
{
#if defined(HAVE_cbranch_int_const)
	if (HAVE_cbranch_int_const_rangecheck(val)) {
		slot_slot_const_const_const(0, s1, val, (jword)dst, type,
					    HAVE_cbranch_int_const, Tcomplex);
	}
	else
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		move_int_const(tmp, val);
		cbranch_int(s1, tmp, dst, type);
	}
#else
	cmp_int_const(0, s1, val);
	branch(dst, type);
#endif
}

void
cbranch_ref(SlotInfo* s1, SlotInfo* s2, label* dst, int type)
{
#if defined(HAVE_cbranch_ref)
	slot_slot_slot_const_const(0, s1, s2, (jword)dst, type,
				   HAVE_cbranch_ref, Tcomplex);
#else
	cmp_ref(0, s1, s2);
	branch(dst, type);
#endif
}

void
cbranch_ref_const(SlotInfo* s1, void *val, label* dst, int type)
{
#if defined(HAVE_cbranch_ref_const)
	if (HAVE_cbranch_ref_const_rangecheck((jword)val)) {
		slot_slot_const_const_const(0, s1, (jword)val, (jword)dst, type,
					    HAVE_cbranch_ref_const, Tcomplex);
	}
	else
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		move_ref_const(tmp, val);
		cbranch_ref(s1, tmp, dst, type);
	}
#else
	cmp_ref_const(0, s1, val);
	branch(dst, type);
#endif
}

#if defined(HAVE_branch_indirect)
void
branch_indirect(SlotInfo* dst)
{
	slot_slot_const(0, dst, ba, HAVE_branch_indirect, Tnull);
}
#endif

#if defined(HAVE_call)
void
call(SlotInfo* dst)
{
	slot_slot_const(0, dst, ba, HAVE_call, Tnull);
}
#endif

void
call_ref(void *routine)
{
	label* l = newLabel();

#if defined(HAVE_call_ref)
	l->type = Lexternal;
	l->at = 0;
	l->to = (uintp)routine;	/* What place does it goto */
	l->from = 0;

	slot_const_const(0, (jword)l, ba, HAVE_call_ref, Tnull);
#else
	constpool* c;
	SlotInfo* tmp;

	c = newConstant(CPref, routine);
	l->type = Lconstant;
	l->at = 0;
	l->to = (uintp)c;
	l->from = 0;

	slot_alloctmp(tmp);
	move_label_const(tmp, l);
	load_ref(tmp, tmp);
	call(tmp);
#endif
}

void
call_indirect_const(void *ptr)
{
#if defined(HAVE_call_indirect_const)
	slot_const_const(0, (jword)ptr, ba, HAVE_call_indirect_const, Tnull);
#else
	SlotInfo *tmp;
	slot_alloctmp(tmp);
	move_ref_const(tmp, ptr);
	load_ref(tmp, tmp);
	call(tmp);
#endif
}

void
call_soft(void *routine)
{
#if defined(HAVE_call_soft)
	label* l = newLabel();
	l->type = Labsolute|Lexternal;
	l->at = 0;
	l->to = (uintp)routine;	/* What place does it goto */
	l->from = 0;

	slot_const_const(0, (jword)l, ba, HAVE_call_soft, Tnull);
#else
	call_ref(routine);
#endif
}


#if defined(HAVE_ret)
void
ret(void)
{
	slot_slot_slot(0, 0, 0, HAVE_ret, Tnull);
}
#endif

#if defined(HAVE_return_int)
void
return_int(SlotInfo* dst)
{
	slot_slot_slot(dst, 0, 0, HAVE_return_int, Tnull);
}
#endif

#if defined(HAVE_return_ref)
void
return_ref(SlotInfo* dst)
{
	slot_slot_slot(dst, 0, 0, HAVE_return_ref, Tnull);
}
#endif

#if defined(HAVE_return_long)
void
return_long(SlotInfo* dst)
{
	lslot_lslot_lslot(dst, 0, 0, HAVE_return_long, Tnull);
}
#endif

#if defined(HAVE_return_float)
void
return_float(SlotInfo* dst)
{
	slot_slot_slot(dst, 0, 0, HAVE_return_float, Tnull);
}
#endif

#if defined(HAVE_return_double)
void
return_double(SlotInfo* dst)
{
	lslot_lslot_lslot(dst, 0, 0, HAVE_return_double, Tnull);
}
#endif

#if defined(HAVE_returnarg_int)
void
returnarg_int(SlotInfo* src)
{
	slot_slot_slot(0, 0, src, HAVE_returnarg_int, Tcopy);
}
#endif

#if defined(HAVE_returnarg_ref)
void
returnarg_ref(SlotInfo* src)
{
	slot_slot_slot(0, 0, src, HAVE_returnarg_ref, Tcopy);
}
#endif

#if defined(HAVE_returnarg_long)
void
returnarg_long(SlotInfo* src)
{
	lslot_lslot_lslot(0, 0, src, HAVE_returnarg_long, Tcopy);
}
#endif

#if defined(HAVE_returnarg_float)
void
returnarg_float(SlotInfo* src)
{
	slot_slot_slot(0, 0, src, HAVE_returnarg_float, Tcopy);
}
#endif

#if defined(HAVE_returnarg_double)
void
returnarg_double(SlotInfo* src)
{
	lslot_lslot_lslot(0, 0, src, HAVE_returnarg_double, Tcopy);
}
#endif


/* ----------------------------------------------------------------------- */
/* Labels.								   */
/*									   */

label*
reference_label(int32 i, int32 n)
{
	label* l;

	assert(n < MAXLABTAB);
	if (labtab[n] == 0) {
		l = newLabel();
		labtab[n] = l;
		l->type = Lnull;
		l->at = 0;
		l->from = 0;
		l->to = 0;
	}
	else {
		l = labtab[n];
		labtab[n] = 0;
	}
	return (l);
}

label*
reference_code_label(uintp offset)
{
	label* l = newLabel();
	l->at = 0;		/* Where is the jump */
	l->to = offset;		/* What place does it goto */
	l->from = 0;
	l->type = Lcode;
	return (l);
}

label*
reference_table_label(int32 n)
{
	label* l;

	assert(n < MAXLABTAB);
	if (labtab[n] == 0) {
		l = newLabel();
		labtab[n] = l;
		l->type = Lnull;
		l->at = 0;
		l->from = 0;
		l->to = 0;
	}
	else {
		l = labtab[n];
		labtab[n] = 0;
	}
	return (l);
}

SlotInfo*
stored_code_label(SlotInfo* dst)
{
	return (dst);
}

SlotInfo*
table_code_label(SlotInfo* dst)
{
	return (dst);
}

#if defined(HAVE_set_label)
void
set_label(int i, int n)
{
	assert(n < MAXLABTAB);
	if (labtab[n] == 0) {
		labtab[n] = newLabel();
		labtab[n]->type = Linternal;
		labtab[n]->at = 0;
		labtab[n]->from = 0;
		labtab[n]->to = 0;
		slot_slot_const(0, 0, (jword)labtab[n], HAVE_set_label, Tnull);
	}
	else {
		assert(labtab[n]->type == Lnull);
		labtab[n]->type = Linternal;
		slot_slot_const(0, 0, (jword)labtab[n], HAVE_set_label, Tnull);
		labtab[n] = 0;
	}
}
#endif

#if defined(HAVE_build_code_ref)
label*
build_code_ref(uint8* pos, uintp pc)
{
	label* l;
	jint offset;

	offset = (pos[0] * 0x01000000 + pos[1] * 0x00010000 +
		  pos[2] * 0x00000100 + pos[3] * 0x00000001);
	l = reference_code_label(pc+offset);

	slot_slot_const(0, 0, (jword)l, HAVE_build_code_ref, Tnull);
	return (l);
}
#endif

#if defined(HAVE_build_key)
void
build_key(uint8* pos)
{
	jint val = (pos[0] * 0x01000000 + pos[1] * 0x00010000 +
		    pos[2] * 0x00000100 + pos[3] * 0x00000001);

	slot_slot_const(0, 0, val, HAVE_build_key, Tnull);
}
#endif


/* ----------------------------------------------------------------------- */
/* Comparisons.								   */
/*									   */

#if defined(HAVE_cmp_int)
void
cmp_int_const(SlotInfo* dst, SlotInfo* src, jint val)
{
#if defined(HAVE_cmp_int_const)
	if (HAVE_cmp_int_const_rangecheck(val)) {
		slot_slot_const(dst, src, val, HAVE_cmp_int_const, Tcomplex);
	}
	else
#endif
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		move_int_const(tmp, val);
		cmp_int(dst, src, tmp);
	}
}

void
cmp_int(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
	slot_slot_slot(dst, src, src2, HAVE_cmp_int, Tcomplex);
}
#endif

#if defined(HAVE_cmp_ref)
void
cmp_ref_const(SlotInfo* dst, SlotInfo* src, void* val)
{
#if defined(HAVE_cmp_ref_const)
	if (HAVE_cmp_ref_const_rangecheck((jword)val)) {
		slot_slot_const(dst, src, (jword)val, HAVE_cmp_ref_const, Tcomplex);
	}
	else
#endif
	{
		SlotInfo* tmp;
		slot_alloctmp(tmp);
		move_ref_const(tmp, val);
		cmp_ref(dst, src, tmp);
	}
}

void
cmp_ref(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
	slot_slot_slot(dst, src, src2, HAVE_cmp_ref, Tcomplex);
}
#endif

void
lcmp(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_lcmp)
	slot_lslot_lslot(dst, src, src2, HAVE_lcmp, Tcomplex);
#else
	end_basic_block();
	pusharg_long(src2, pusharg_long_idx_inc);
	pusharg_long(src, 0);
	call_soft(soft_lcmp);
	popargs();
	start_basic_block();
	return_int(dst);
#endif
}

void
cmpl_float(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_cmpl_float)
	slot_slot_slot(dst, src, src2, HAVE_cmpl_float, Tcomplex);
#else
	end_basic_block();
	pusharg_float(src2, 1);
	pusharg_float(src, 0);
	call_soft(soft_fcmpl);
	popargs();
	start_basic_block();
	return_int(dst);
#endif
}

void
cmpl_double(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_cmpl_double)
	slot_lslot_lslot(dst, src, src2, HAVE_cmpl_double, Tcomplex);
#else
	end_basic_block();
	pusharg_double(src2, pusharg_long_idx_inc);
	pusharg_double(src, 0);
	call_soft(soft_dcmpl);
	popargs();
	start_basic_block();
	return_int(dst);
#endif
}

void
cmpg_float(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_cmpg_float)
	slot_slot_slot(dst, src, src2, HAVE_cmpg_float, Tcomplex);
#else
	end_basic_block();
	pusharg_float(src2, 1);
	pusharg_float(src, 0);
	call_soft(soft_fcmpg);
	popargs();
	start_basic_block();
	return_int(dst);
#endif
}

void
cmpg_double(SlotInfo* dst, SlotInfo* src, SlotInfo* src2)
{
#if defined(HAVE_cmpg_double)
	slot_lslot_lslot(dst, src, src2, HAVE_cmpg_double, Tcomplex);
#else
	end_basic_block();
	pusharg_double(src2, pusharg_long_idx_inc);
	pusharg_double(src, 0);
	call_soft(soft_dcmpg);
	popargs();
	start_basic_block();
	return_int(dst);
#endif
}

/* ----------------------------------------------------------------------- */
/* Conversions.								   */
/*									   */

void
cvt_int_long(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_cvt_int_long)
	lslot_lslot_slot(dst, 0, src, HAVE_cvt_int_long, Tcomplex);
#else
	move_int(LSLOT(dst), src);
	ashr_int_const(HSLOT(dst), src, (8 * sizeof(jint)) - 1);
#endif
}

void
cvt_int_float(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_cvt_int_float)
	slot_slot_slot(dst, 0, src, HAVE_cvt_int_float, Tcomplex);
#else
	end_basic_block();
	pusharg_int(src, 0);
	call_soft(soft_cvtif);
	popargs();
	start_basic_block();
	return_float(dst);
#endif
}

void
cvt_int_double(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_cvt_int_double)
	lslot_lslot_slot(dst, 0, src, HAVE_cvt_int_double, Tcomplex);
#else
	end_basic_block();
	pusharg_int(src, 0);
	call_soft(soft_cvtid);
	popargs();
	start_basic_block();
	return_double(dst);
#endif
}

void
cvt_long_int(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_cvt_long_int)
	slot_slot_lslot(dst, 0, src, HAVE_cvt_long_int, Tcomplex);
#else
	move_int(dst, LSLOT(src));
#endif
}

void
cvt_long_float(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_cvt_long_float)
	slot_slot_lslot(dst, 0, src, HAVE_cvt_long_float, Tcomplex);
#else
	end_basic_block();
	pusharg_long(src, 0);
	call_soft(soft_cvtlf);
	popargs();
	start_basic_block();
	return_float(dst);
#endif
}

void
cvt_long_double(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_cvt_long_double)
	lslot_lslot_lslot(dst, 0, src, HAVE_cvt_long_double, Tcomplex);
#else
	end_basic_block();
	pusharg_long(src, 0);
	call_soft(soft_cvtld);
	popargs();
	start_basic_block();
	return_double(dst);
#endif
}

void
cvt_float_int(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_cvt_float_int)
	used_ieee_rounding = true;
	slot_slot_slot(dst, 0, src, HAVE_cvt_float_int, Tcomplex);
#else
	end_basic_block();
	pusharg_float(src, 0);
	call_soft(soft_cvtfi);
	popargs();
	start_basic_block();
	return_int(dst);
#endif
}

void
cvt_float_long(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_cvt_float_long)
	used_ieee_rounding = true;
	lslot_lslot_slot(dst, 0, src, HAVE_cvt_float_long, Tcomplex);
#else
	end_basic_block();
	pusharg_float(src, 0);
	call_soft(soft_cvtfl);
	popargs();
	start_basic_block();
	return_long(dst);
#endif
}

void
cvt_float_double(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_cvt_float_double)
	lslot_lslot_slot(dst, 0, src, HAVE_cvt_float_double, Tcomplex);
#else
	end_basic_block();
	pusharg_float(src, 0);
	call_soft(soft_cvtfd);
	popargs();
	start_basic_block();
	return_double(dst);
#endif
}

void
cvt_double_int(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_cvt_double_int)
	used_ieee_rounding = true;
	slot_slot_lslot(dst, 0, src, HAVE_cvt_double_int, Tcomplex);
#else
	end_basic_block();
	pusharg_double(src, 0);
	call_soft(soft_cvtdi);
	popargs();
	start_basic_block();
	return_int(dst);
#endif
}

void
cvt_double_long(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_cvt_double_long)
	used_ieee_rounding = true;
	lslot_lslot_lslot(dst, 0, src, HAVE_cvt_double_long, Tcomplex);
#else
	end_basic_block();
	pusharg_double(src, 0);
	call_soft(soft_cvtdl);
	popargs();
	start_basic_block();
	return_long(dst);
#endif
}

void
cvt_double_float(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_cvt_double_float)
	slot_slot_lslot(dst, 0, src, HAVE_cvt_double_float, Tcomplex);
#else
	end_basic_block();
	pusharg_double(src, 0);
	call_soft(soft_cvtdf);
	popargs();
	start_basic_block();
	return_float(dst);
#endif
}

void
cvt_int_byte(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_cvt_int_byte)
	slot_slot_slot(dst, 0, src, HAVE_cvt_int_byte, Tcomplex);
#else
	lshl_int_const(dst, src, 8 * (sizeof(jint) - sizeof(jbyte)));
	ashr_int_const(dst, dst, 8 * (sizeof(jint) - sizeof(jbyte)));
#endif
}

void
cvt_int_char(SlotInfo* dst, SlotInfo* src)
{
	and_int_const(dst, src, (1 << (8 * sizeof(jchar))) - 1);
}

void
cvt_int_short(SlotInfo* dst, SlotInfo* src)
{
#if defined(HAVE_cvt_int_short)
	slot_slot_slot(dst, 0, src, HAVE_cvt_int_short, Tcomplex);
#else
	lshl_int_const(dst, src, 8 * (sizeof(jint) - sizeof(jshort)));
	ashr_int_const(dst, dst, 8 * (sizeof(jint) - sizeof(jshort)));
#endif
}



/* ----------------------------------------------------------------------- */
/* Breakpoints.								   */
/*									   */

void
breakpoint()
{
	ABORT();
}


/* ----------------------------------------------------------------------- */
/* Soft calls.								   */
/*									   */

void
softcall_lookupmethod(SlotInfo* dst, Method* meth, SlotInfo* obj)
{
	/* 'obj' must be written back since it will be reused */
	prepare_function_call();
	pusharg_ref_const(meth, 1);
	pusharg_ref(obj, 0);
	call_soft(soft_lookupmethod);
	popargs();
	fixup_function_call();
	return_ref(dst);
}

void
softcall_badarrayindex(void)
{
	sync_registers();
	call_soft(soft_badarrayindex);
}

void
softcall_nullpointer(void)
{
	sync_registers();
	call_soft(soft_nullpointer);
}

void
softcall_new(SlotInfo* dst, Hjava_lang_Class* classobj)
{
	prepare_function_call();
	pusharg_ref_const(classobj, 0);
	call_soft(soft_new);
	popargs();
	fixup_function_call();
	return_ref(dst);
}

void
softcall_newarray(SlotInfo* dst, SlotInfo* size, int type)
{
	slot_nowriteback(size);
	prepare_function_call();
	pusharg_int(size, 1);
	pusharg_int_const(type, 0);
	call_soft(soft_newarray);
	popargs();
	fixup_function_call();
	return_ref(dst);
}

void
softcall_anewarray(SlotInfo* dst, SlotInfo* size, Hjava_lang_Class* type)
{
	slot_nowriteback(size);
	prepare_function_call();
	pusharg_int(size, 1);
	pusharg_ref_const(type, 0);
	call_soft(soft_anewarray);
	popargs();
	fixup_function_call();
	return_ref(dst);
}

void
softcall_multianewarray(SlotInfo* dst, int size, SlotInfo* stktop, Hjava_lang_Class* classobj)
{
	int i;

	prepare_function_call();
	for (i = 0; i < size; i++) {
		pusharg_int(&stktop[i], 1+size-i);
	}
	pusharg_int_const(size, 1);
	pusharg_ref_const(classobj, 0);
	call_soft(soft_multianewarray);
	popargs();
	fixup_function_call();
	return_ref(dst);
}

void
softcall_athrow(SlotInfo* obj)
{
	slot_nowriteback(obj);
	prepare_function_call();
	pusharg_ref(obj, 0);
	call_soft(soft_athrow);
	popargs();
	fixup_function_call();
}

void
softcall_checkcast(SlotInfo* obj, Hjava_lang_Class* class)
{
	/* Must keep 'obj' */
	prepare_function_call();
	pusharg_ref(obj, 1);
	pusharg_ref_const(class, 0);
	call_soft(soft_checkcast);
	popargs();
	fixup_function_call();
}

#if !defined(HAVE_TRAMPOLINE)
void
softcall_get_method_code (SlotInfo* dst, SlotInfo* method)
{
	slot_nowriteback(method);
	prepare_function_call();
	pusharg_ref(method, 0);
	call_soft(soft_get_method_code);
	popargs();
	fixup_function_call();
	return_ref(dst);
}

void
softcall_get_method_code_const (SlotInfo* dst, Method* method)
{
#ifdef CUSTOMIZATION
    if ( ! isMethodGenerallyTranslated( method ) ) {
        prepare_function_call();
        pusharg_ref_const(method, 0);
        call_soft(soft_get_method_code);
        popargs();
        fixup_function_call();
        return_ref(dst);
    } else {
        assert( getGenerallyTranslatedCode( method ) );
        move_ref_const (dst, getGenerallyTranslatedCode( method ) );
    }
#else
    if (!METHOD_TRANSLATED(method)) {
        prepare_function_call();
        pusharg_ref_const(method, 0);
        call_soft(soft_get_method_code);
        popargs();
        fixup_function_call();
        return_ref(dst);
    }
    else {
        move_ref_const (dst, method->ncode);
    }
#endif
}
#endif

void
softcall_instanceof(SlotInfo* dst, SlotInfo* obj, Hjava_lang_Class* class)
{
	slot_nowriteback(obj);
	prepare_function_call();
	pusharg_ref(obj, 1);
	pusharg_ref_const(class, 0);
	call_soft(soft_instanceof);
	popargs();
	fixup_function_call();
	return_int(dst);
}

void
softcall_monitorenter(SlotInfo* mon)
{
	slot_nowriteback(mon);
	prepare_function_call();
	pusharg_ref(mon, 0);
	call_soft(soft_monitorenter);
	popargs();
	fixup_function_call();
}

void
softcall_monitorexit(SlotInfo* mon)
{
	slot_nowriteback(mon);
	prepare_function_call();
	pusharg_ref(mon, 0);
	call_soft(soft_monitorexit);
	popargs();
	fixup_function_call();
}

void
softcall_initialise_class(Hjava_lang_Class* c)
{
	prepare_function_call();
	pusharg_ref_const(c, 0);
	call_soft(soft_initialise_class);
	popargs();
	fixup_function_call();
}

void
softcall_checkarraystore(SlotInfo* array, SlotInfo* obj)
{
	prepare_function_call();
	pusharg_ref(obj, 1);
	pusharg_ref(array, 0);
	call_soft(soft_checkarraystore);
	popargs();
	fixup_function_call();
}

#if defined(GC_INCREMENTAL)
void
softcall_addreference(SlotInfo* from, SlotInfo* to)
{
	prepare_function_call();
	pusharg_ref(to, 1);
	pusharg_ref(from, 0);
	call_ref(soft_addreference);
	popargs();
	fixup_function_call();
}

void
softcall_addreference_static(void* from, SlotInfo* to)
{
	prepare_function_call();
	pusharg_ref(to, 1);
	pusharg_ref_const(from, 0);
	call_soft(soft_addreference);
	popargs();
	fixup_function_call();
}
#endif
