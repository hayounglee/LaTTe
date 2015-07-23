/*
 * flags.h
 * Various configuration flags.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 */

#ifndef __flags_h
#define __flags_h

extern int flag_verify;
extern int flag_gc;
extern int flag_classload;
extern int flag_jit;
extern int flag_translated_method;
extern int flag_vcg;
extern int flag_code;
extern int flag_exception;
extern int flag_called_method;
extern int flag_tr_time;
extern int flag_old_join;
extern int flag_time;
extern int flag_call_stat;
extern unsigned gc_heap_allocation_size;
extern unsigned gc_heap_limit;


extern int flag_no_cse; // CSE
extern int flag_no_regionopt; // region optimization(can be changed any time) 
extern int The_Use_Regionopt_Option; // command line option
extern int flag_no_regionopt_num; // region optimization num
extern int flag_regionopt_num; // region optimization num
extern int flag_no_dce; // dead code elimination
extern int flag_dce_num; 
extern int flag_no_backward_sweep;
extern int flag_no_live_analysis;
extern int flag_no_copy_coalescing;

extern int flag_no_loopopt; // LICM, register promotion
extern int The_Use_Loopopt_Option; // command line option
extern int flag_loopopt_num;
extern int flag_looppeeling;



// #ifdef JP_OPTIM_INVOKE_DBG
// extern int vi_limit;
// #endif
extern int flag_inlining;
extern int The_Use_Method_Inline_Option;  // command line option

//made by walker
extern int flag_opt_checkcast;
extern int flag_opt_instanceof;

#ifdef METHOD_COUNT
/* Flag for retranslation based on method run count threshold. */
extern int flag_adapt;
extern int The_Retranslation_Threshold;
extern int The_Max_Retranslation_Threshold;

// to control retranslation
extern int The_Final_Translation_Flag; 
extern int The_Need_Retranslation_Flag; 
#endif

#ifdef TYPE_ANALYSIS
extern int The_Use_TypeAnalysis_Flag;
extern int flag_type_analysis;
#endif

#ifdef CUSTOMIZATION
extern int The_Use_Customization_Flag;
extern int The_Use_Specialization_Flag;
#endif

#ifdef INTERPRETER
extern int The_Use_Interpreter_Flag;
#endif

#endif
