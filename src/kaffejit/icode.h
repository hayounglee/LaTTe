/* sparc/jit-icode.h
 * Define the instructions which are present on the SPARC.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 */

#ifndef __sparc_jit_icode_h
#define __sparc_jit_icode_h

/*
 * Size of longs compared to refs.
 */
#define	pusharg_long_idx_inc	2

/*
 * Define the range checking macros.
 */
#define	__intconst_rangecheck(v)	((v) >= -4096 && (v) <= 4095)
#define	__refconst_rangecheck(v)	__intconst_rangecheck(v)

/*
 * These must be defined for any architecture.
 */
#define	HAVE_spill_int			spill_Rxx
#define	HAVE_reload_int			reload_Rxx
#define	HAVE_spill_float		fspill_Rxx
#define	HAVE_reload_float		freload_Rxx
#define	HAVE_spill_double		fspilll_Rxx
#define	HAVE_reload_double		freloadl_Rxx
#define	HAVE_prologue			prologue_xLC
#define	HAVE_epilogue			epilogue_xxx
#define HAVE_exception_prologue		eprologue_xxx

#define	HAVE_move_int_const_rangecheck(v)        (1)

#define	HAVE_move_int_const		move_RxC
#define	HAVE_move_int			move_RxR
#define	HAVE_move_float			fmove_RxR
#define	HAVE_move_double		fmovel_RxR
#define	HAVE_move_label_const		move_RxL
#define	HAVE_move_ref			move_RxR
#define	HAVE_move_any			move_RxR

#define	HAVE_add_int			add_RRR
#define	HAVE_sub_int			sub_RRR
#define	HAVE_and_int			and_RRR
#define	HAVE_or_int			or_RRR
#define	HAVE_xor_int			xor_RRR
#define	HAVE_ashr_int			ashr_RRR
#define	HAVE_lshr_int			lshr_RRR
#define	HAVE_lshl_int			lshl_RRR

#define	HAVE_add_float			fadd_RRR
#define	HAVE_sub_float			fsub_RRR
#define	HAVE_mul_float			fmul_RRR
#define	HAVE_div_float			fdiv_RRR

#define	HAVE_add_double			faddl_RRR
#define	HAVE_sub_double			fsubl_RRR
#define	HAVE_mul_double			fmull_RRR
#define	HAVE_div_double			fdivl_RRR

#define	HAVE_add_ref			add_RRR

#define	HAVE_load_int			load_RxR
#define	HAVE_store_int			store_xRR
#define	HAVE_load_ref			load_RxR
#define	HAVE_store_ref			store_xRR

#define	HAVE_load_float			fload_RxR
#define	HAVE_store_float		fstore_xRR
#define	HAVE_load_double		floadl_RxR
#define	HAVE_store_double		fstorel_xRR

#define	HAVE_pusharg_int		push_xRC
#define	HAVE_pusharg_float		fpush_xRC
#define	HAVE_pusharg_double		fpushl_xRC
#define	HAVE_pusharg_ref		push_xRC
#define	HAVE_popargs			popargs_xxC

#define	HAVE_cmp_int			cmp_xRR
#define	HAVE_cmp_ref			cmp_xRR

#define	HAVE_branch			branch_xCC
#define	HAVE_branch_indirect		branch_indirect_xRC
#define	HAVE_call_ref			call_xCC
#define	HAVE_call			call_xRC
#define	HAVE_ret			ret_xxx
#define	HAVE_return_int			return_Rxx
#define	HAVE_return_long		returnl_Rxx
#define	HAVE_return_float		freturn_Rxx
#define	HAVE_return_double		freturnl_Rxx
#define	HAVE_return_ref			return_Rxx
#define	HAVE_returnarg_int		returnarg_xxR
#define	HAVE_returnarg_long		returnargl_xxR
#define	HAVE_returnarg_float		freturnarg_xxR
#define	HAVE_returnarg_double		freturnargl_xxR
#define	HAVE_returnarg_ref		returnarg_xxR

#define	HAVE_set_label			set_label_xxC
#define	HAVE_build_key			set_word_xxC
#define	HAVE_build_code_ref		set_wordpc_xxC

#define	HAVE_cvt_int_double		cvtid_RxR
#define	HAVE_cvt_float_int		cvtfi_RxR
#define	HAVE_cvt_double_int		cvtdi_RxR
#define	HAVE_cvt_int_float		cvtif_RxR
#define	HAVE_cvt_float_double		cvtfd_RxR
#define	HAVE_cvt_double_float		cvtdf_RxR

/*
 * These are sometimes optional (if long operators are defined)
 */
#define	HAVE_adc_int			adc_RRR
#define	HAVE_sbc_int			sbc_RRR

/*
 * These are optional but help to optimise the code generated.
 */
#define HAVE_add_int_const		add_RRC
#define HAVE_sub_int_const		sub_RRC 
#define HAVE_cmp_int_const		cmp_xRC
#define HAVE_cmp_ref_const		cmp_xRC
#define HAVE_load_offset_int		load_RRC
#define HAVE_load_offset_ref		load_RRC
#define HAVE_store_offset_int		store_xRRC
#define	HAVE_lshl_int_const		lshl_RRC

#define	HAVE_add_int_const_rangecheck(v)	__intconst_rangecheck(v)
#define	HAVE_sub_int_const_rangecheck(v)	__intconst_rangecheck(v)
#define	HAVE_cmp_int_const_rangecheck(v)	__intconst_rangecheck(v)
#define	HAVE_load_offset_int_rangecheck(v)	__intconst_rangecheck(v)
#define	HAVE_store_offset_int_rangecheck(v)	__intconst_rangecheck(v)
#define	HAVE_lshl_int_const_rangecheck(v)	__intconst_rangecheck(v)

#define	HAVE_cmp_ref_const_rangecheck(v)	__refconst_rangecheck(v)
#define	HAVE_load_offset_ref_rangecheck(v)	__refconst_rangecheck(v)

#define	HAVE_load_byte			loadb_RxR
#define	HAVE_load_char			loadc_RxR
#define	HAVE_load_short			loads_RxR
#define	HAVE_store_byte			storeb_xRR
#define	HAVE_store_char			stores_xRR
#define	HAVE_store_short		stores_xRR

/*
 * These are optional if the architecture supports them.
 */

#undef	HAVE_move_float_const
#undef	HAVE_move_double_const

#undef	HAVE_swap_int
#undef	HAVE_neg_int
#undef	HAVE_mul_int
#undef	HAVE_div_int
#undef	HAVE_rem_int
#undef	HAVE_mul_int_const
#undef	HAVE_and_int_const
#undef	HAVE_ashr_int_const
#undef	HAVE_lshr_int_const

#undef	HAVE_cmpg_float
#undef	HAVE_cmpg_double
#undef	HAVE_cmpl_float
#undef	HAVE_cmpl_double

#undef	HAVE_move_long_const
#undef	HAVE_move_long

#undef	HAVE_add_long
#undef	HAVE_sub_long
#undef	HAVE_mul_long
#undef	HAVE_div_long
#undef	HAVE_rem_long
#undef	HAVE_neg_long
#undef	HAVE_and_long
#undef	HAVE_or_long
#undef	HAVE_xor_long
#undef	HAVE_ashr_long
#undef	HAVE_lshl_long
#undef	HAVE_lshr_long

#undef	HAVE_load_long
#undef	HAVE_store_long

#undef	HAVE_pusharg_int_const
#undef	HAVE_pusharg_long

#undef	HAVE_cmp_long

#undef	HAVE_neg_float
#undef	HAVE_neg_double
#undef	HAVE_rem_float
#undef	HAVE_rem_double

#undef	HAVE_cvt_long_double
#undef	HAVE_cvt_float_long
#undef	HAVE_cvt_double_long
#undef	HAVE_cvt_long_float
#undef	HAVE_cvt_int_long
#undef	HAVE_cvt_int_byte
#undef	HAVE_cvt_int_char
#undef	HAVE_cvt_int_short
#undef	HAVE_cvt_long_int

#endif
