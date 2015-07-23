/* machine.h
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>
 */

#ifndef __machine_h
#define __machine_h

/* -------------------------------------------------------------------- */

/* Instructions */
#define	define_insn(code)	break;					\
				case code :

/* Stack */
#define	push(_i)		stackno -= (_i)
#define	pop(_i)			stackno += (_i)

/* PC */
#define	getpc(_p)		base[pc+1+(_p)]
#define	getcode(_p)		base[(_p)]
#define	adjustpc(_p)		npc = pc + (_p)

#define current_class()		(meth->class)

/* -------------------------------------------------------------------- */
/* Methods */

#define	get_method_info(idx)  callee = getMethodSignatureClass(idx, meth)

#define	method_name()						\
			(slot_alloctmp(cnst),			\
			move_ref_const(cnst, cinfo.mtag),	\
			cnst)
#define	method_sig()	(callee->signature)
#define	method_idx()	(callee->idx)
#define method_method()	(callee)
#define method_class()  (callee->class)
#define	get_dispatch_table(mtable)				\
			move_ref_const(mtable, callee->class->dtable)

#define	method_nargs()		(callee->in)
#define	method_dtable_offset	(OBJECT_DTABLE_OFFSET)
#define	method_returntype()	(callee->rettype)

/* -------------------------------------------------------------------- */
/* Fields */

#define	get_field_info(_idx)	field = getField((_idx), false, meth, &crinfo)
#define	get_static_field_info(_idx) field = getField((_idx), true, meth, &crinfo)

#define field_class()		(crinfo)

/* -------------------------------------------------------------------- */
/* Classes */

#define	get_class_info(_idx)	crinfo = getClass((_idx), meth->class)

#define	class_object()		(crinfo)

/* -------------------------------------------------------------------- */
/* Objects */

#define	object_array_offset	(ARRAY_DATA_OFFSET)
#define	object_array_length	(ARRAY_SIZE_OFFSET)

/* -------------------------------------------------------------------- */
/* Switches */

#define	switchpair_size		8
#define	switchpair_addr		4
#define	switchtable_shift	2

/* Provide write barrier support for incremental GC */
#if defined(GC_INCREMENTAL)
#define	SOFT_ADDREFERENCE(_f, _t)	 softcall_addreference(_f, _t)
#define	SOFT_ADDREFERENCE_STATIC(_f, _t) softcall_addreference_static(_f, _t)
#else
#define	SOFT_ADDREFERENCE(_f, _t)
#define	SOFT_ADDREFERENCE_STATIC(_f, _t)
#endif

struct _methods;
void kaffe_translate(struct _methods*);

typedef struct _nativeCodeInfo {
	void*	mem;
	int	memlen;
	void*	code;
	int	codelen;
} nativeCodeInfo;

#endif
