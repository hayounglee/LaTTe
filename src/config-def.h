/* config-def.h
   Set definitions according to configuration.
   
   Written by: Yoo C. Chung <chungyc@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#define OPTIM_LOOKUPSWITCH 1
#define OPTIM_INVOKESTATIC 1
#define JP_OPTIM_INVOKE 1
#define OPTIM_INVOKE 1

/* Work done by scdoner. */
#define NO_REDUNDANT_EDGE 1	/* unique edge for jump tables */
#define NEW_TRANSLATOR_LOCK 1
#define PRECISE_MONITOREXIT 1
#define OUTLINING 1

/* Work done by hacker. */
#define NEW_INTERFACETABLE 1	/* interface table implementation */
#define NEW_CODE_ALLOC 1
#define NEW_VAR_OFFSET 1
#define EXCEPTION_EDGE_CONNECT 1 /* connects exception edge if possible */
#define USE_CODE_ALLOCATOR 1
#define SPLIT_TEXT_DATA 1
#undef RETRAN_CALLER
#undef HANDLER_INLINE 
#undef UPDATED_RETRAN
#undef NEW_COUNT_RISC
#undef USE_KAFFE_TRANSLATOR

/* Work done by walker. */
#define LOCK_OPT 1	/* only lock_info is included to each object */
#define NEW_NULL_CHECK 1
#define W_MERGE 1
#define DELAY_PROLOGUE_CODE_GENERATION 1
#define USE_TRAP_FOR_ARRAY_BOUND_CHECK 1
#define VARIABLE_RET_COUNT 1
#define GIVEUP_INHERIT 1
#define INLINE_INVOKEINTERFACE 1
#define UPDATE_INCLUSION_TEST 1
#define UPDATE_CONSTANT_PROPAGATION 1
#define DFS_CODE_PLACEMENT 1

#if defined(INTERPRETER) && !defined(TRANSLATOR)
#undef DYNAMIC_CHA
#else /* not defined(INTERPRETER) && !defined(TRANSLATOR) */
#define DYNAMIC_CHA 1
#endif /* not defined(INTERPRETER) && !defined(TRANSLATOR) */

#undef CUSTOMIZATION_DBG
#undef EXPAND_INLINE_CACHE

/* Work done by chungyc. */
#if defined(INTERPRETER) && !defined(TRANSLATOR)
#undef METHOD_COUNT		/* maintain method count */
#else /* not defined(INTERPRETER) && !defined(TRANSLATOR) */
#define METHOD_COUNT 1		/* maintain method count */
#endif /* not defined(INTERPRETER) && !defined(TRANSLATOR) */

/* Other flags. */
#define NEW_LOCK_HANDLING 1
#define EFFICIENT_SUBCLASS_INFERRING 1
#define USE_INLINE 1
#define CHECK_NULL_POINTER 1
