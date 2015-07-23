/*
 * flags.c
 * Various configuration flags.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 * Modified by MASS Laboratory, SNU, 1999.
 */

#include "config.h"
#include "flags.h"

/*
 * Specify which version of Java we can use.
 */
char* java_version = "1.1.6";
int java_major_version = 1;
int java_minor_verison = 1;

/*
 * Specify what and how bytecode should be verified.
 */
int flag_verify = 0;

/*
 * Specify whether the garbage collector is noisy.
 */
int flag_gc = 0;
/*
 * Specify where class loading in noisy.
 */
int flag_classload;

/*
 * Specify whether the JIT system (if in use) is noisy.
 */
int flag_jit = 0;


//
// Specify whether prints the translated methods.
//
int flag_translated_method = 0;

// 
// Specify whether prints the called methods.
//
int flag_called_method = 0;

//
// Specify whether prints the vcg for methods
//
int flag_vcg = 0;

//
// Specify whether to print the asm file for methods
//
int flag_code = 0;

//
// Specify whether prints the generated exception
//
int flag_exception = 0;

//
// Specify whether calculate the time for translation
//
int flag_tr_time = 0;

//
// Specify whether to use old make_regsters_consistent()
//
int flag_old_join = 1;

//
// Specify whether to generate the call statistics
//
int flag_call_stat;



// #ifdef JP_OPTIM_INVOKE_DBG
// int vi_limit = 0;
// #endif

// turn on/off the optimization
int flag_no_regionopt;
int The_Use_Regionopt_Option = 1; // command line option
int flag_no_regionopt_num = 0;
int flag_regionopt_num = 0;
int flag_no_cse = 0;

// turn on/off dead code elimination
int flag_no_dce = 0;

int flag_dce_num = 0;

// turn on/off loop optimization
int flag_no_loopopt = 0;
int The_Use_Loopopt_Option = 1; // command line option
int flag_loopopt_num = 0;
int flag_looppeeling = 1;   // default: no loop peeling

// turn on/off the backward sweep
int flag_no_backward_sweep = 0;

// turn on/off the simple live analysis
int flag_no_live_analysis = 0;

// turn on/off the copy coalescing
int flag_no_copy_coalescing = 0;

int flag_time = 0;

int flag_inlining;
int The_Use_Method_Inline_Option = 1;

int flag_opt_checkcast = 1;
int flag_opt_instanceof = 1;

#ifdef METHOD_COUNT
/* Flag for retranslation based on method run count threshold. */
int flag_adapt = 0;

/* Number of times to run method before retranslating. */
int The_Retranslation_Threshold = 5;

// to control retranslation
// whether the current translation is the final translation for the method
int The_Final_Translation_Flag; 
// whether this method needs(or can benefit from) retranslation. 
int The_Need_Retranslation_Flag; 

int The_Max_Retranslation_Threshold = 50;
#endif

#ifdef TYPE_ANALYSIS
int The_Use_TypeAnalysis_Flag = 1;
int flag_type_analysis;
#endif

#ifdef CUSTOMIZATION
int The_Use_Customization_Flag = 1;
int The_Use_Specialization_Flag = 0;
#endif

#ifdef INTERPRETER
int The_Use_Interpreter_Flag = 1;
#endif
