/* machine.c
 * Translate the Kaffe instruction set to the native one.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>
 * Modified by MASS Laboratory, SNU, 1999.
 */

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "gtypes.h"
#include "bytecode.h"
#include "slots.h"
#include "registers.h"
#include "seq.h"
#include "gc.h"
#include "machine.h"
#include "basecode.h"
#include "icode.h"
#include "labels.h"
#include "constpool.h"
#include "codeproto.h"
#include "checks.h"
#include "access.h"
#include "object.h"
#include "constants.h"
#include "baseClasses.h"
#include "classMethod.h"
#include "code.h"
#include "access.h"
#include "lookup.h"
#include "exception.h"
#include "flags.h"
#include "errors.h"
#include "md.h"
#include "locks.h"
#include "code-analyse.h"
#include "external.h"
#include "soft.h"
#include "SpecializedMethod.h"
#include "method_inlining.h"

#define	DBG(s)
#define	MDBG(s) 
#define	SDBG(s)


/*
 * Define information about this engine.
 */
extern char* engine_name;
extern char* engine_version;

int stackno;
int maxStack;
int maxLocal;
int maxTemp;
int maxArgs;
int maxPush;
int isStatic;

int tmpslot;
int argcount = 0;		/* Function call argument count */
uint32 pc;
uint32 npc;

/* Define CREATE_NULLPOINTER_CHECKS in md.h when your machine cannot use the
 * MMU for detecting null pointer accesses */
#if defined(CREATE_NULLPOINTER_CHECKS)
#define CHECK_NULL(_i, _s, _n)                                  \
      cbranch_ref_const_ne((_s), 0, reference_label(_i, _n)); \
      softcall_nullpointer();                                 \
      set_label(_i, _n)
#else
#define CHECK_NULL(_i, _s, _n)
#endif

/* Unit in which code block is increased when overrun */
#define	ALLOCCODEBLOCKSZ	8192
/* Codeblock redzone - allows for safe overrun when generating instructions */
#define	CODEBLOCKREDZONE	256

nativecode* codeblock;
int codeblock_size;
static int code_generated;
static int bytecode_processed;
static int codeperbytecode;

/* Mutex to protect the translator from multiple entry */
static quickLock translatorlock;

int CODEPC;

extern unsigned long long jitStat;

extern int enable_readonce;

void	initInsnSequence(int, int, int);
void	finishInsnSequence(nativeCodeInfo*);
static void generateInsnSequence(void);
static void installMethodCode(Method*, nativeCodeInfo*);

void	endBlock(sequence*);
void	startBlock(sequence*);
//jlong	currentTime(void);

// hacker modified for gctime flag
extern unsigned long long translation_total;
extern int num;

/*
 * Translate a method into native code.
 */
void
kaffe_translate(Method* meth)
{
    int wide;
    
    jint low;
    jint high;
    jlong tmpl;
    int idx;
    SlotInfo* tmp;
    SlotInfo* tmp2;
    SlotInfo* mtable;
    char* str;

    uint8* base;
    int len;
    Method *callee;
    Field *field;
    Hjava_lang_Class* crinfo;

    nativeCodeInfo ncode;

    struct timeval tms;
    struct timeval tme;
    unsigned long long time;

    // hacker modified for gctime flag
    struct timeval translate_start;
#ifdef TTTT
    struct timeval t1, t2, t3, t4;
#endif
    /* Only one in the translator at once. Must check the translation
     * hasn't been done by someone else once we get it.
     */
    gc_mode = GC_DISABLED;
    _lockMutex(&translatorlock);
    
#ifdef CUSTOMIZATION
    if ( SM_is_method_generally_translated( meth ) ){
        _unlockMutex(&translatorlock);
        return;
    }
#else
    if (METHOD_TRANSLATED(meth)) {
        _unlockMutex(&translatorlock);
        return;
    }
#endif

DBG(	printf("callinfo = 0x%x\n", &cinfo);	)

    /* If this code block is native, then just set it up and return */
    if ((meth->accflags & ACC_NATIVE) != 0) {
        native(meth);
        KAFFEJIT_TO_NATIVE(meth);
        /* Note that this is a real function not a trampoline.  */
        _unlockMutex(&translatorlock);
        return;
    }

    // hacker modified for gctime flag
    if (flag_time > 0) 
      gettimeofday(&translate_start, NULL);
	
    if (flag_jit) {
        gettimeofday(&tms, 0);
    }

    num++;	/* for debugging method no is increased as LaTTe JIT */
    
    maxLocal = meth->localsz;
    maxStack = meth->stacksz;
    str = meth->signature->data;
   
    maxArgs = sizeofSig(&str, false);

    if (meth->accflags & ACC_STATIC) {
        isStatic = 1;
    }
    else {
        isStatic = 0;
        maxArgs += 1;
    }

    base = (uint8 *)meth->bcode;
    len = meth->bcodelen;

    /* Scan the code and determine the basic blocks */
    verifyMethod(meth);

    /***************************************/
    /* Next reduce bytecode to native code */
    /***************************************/

    initInsnSequence(codeperbytecode * len, meth->localsz, meth->stacksz);
    
    start_basic_block();
    start_function();
    monitor_enter();
    if (IS_STARTOFBASICBLOCK(0)) {
        end_basic_block();
        start_basic_block();
    }
#ifdef TTTT
    if (flag_jit) {
        gettimeofday(&t1, 0);
    }
   
#endif
    wide = 0;
    for (pc = 0; pc < len; pc = npc) {

        assert(stackno <= maxStack+maxLocal);

        npc = pc + insnLen[base[pc]];
        
        /* Handle WIDEned instructions */
        if (wide) {
            switch(base[pc]) {
              case ILOAD:
              case FLOAD:
              case ALOAD:
              case LLOAD:
              case DLOAD:
              case ISTORE:
              case FSTORE:
              case ASTORE:
              case LSTORE:
              case DSTORE:
                npc += 1;
                break;
              case IINC:
                npc += 2;
                break;
              default:
                fprintf(stderr, "Badly widened instruction %d\n", base[pc]);
                throwException(VerifyError);
                break;
            }
        }
        
        start_instruction();

        /* Note start of exception handling blocks */
        if (IS_STARTOFEXCEPTION(pc)) {
            stackno = meth->localsz + meth->stacksz - 1;
            start_exception_block();
        }

        switch (base[pc]) {
          default:
            fprintf(stderr, "Unknown bytecode %d\n", base[pc]);
            throwException(VerifyError);
            break;
#include "kaffe.def"
        }

        /* Note maximum number of temp slots used and reset it */
        if (tmpslot > maxTemp) {
            maxTemp = tmpslot;
        }
        tmpslot = 0;

        if (IS_STARTOFBASICBLOCK(npc)) {
            end_basic_block();
            start_basic_block();
            stackno = STACKPOINTER(npc);
        }

        generateInsnSequence();
    }
#ifdef TTTT
    if (flag_jit) {
        gettimeofday(&t2, 0);
    }
#endif
    assert(maxTemp < MAXTEMPS);

    finishInsnSequence(&ncode);
#ifdef TTTT
    if (flag_jit) {
        gettimeofday(&t3, 0);
    }
#endif
    installMethodCode(meth, &ncode);
#ifdef TTTT
    if (flag_jit) {
        gettimeofday(&t4, 0);
    }
#endif

// #ifdef NEW_JIT
// 	meth->JIT_level = KAFFE_JIT;
// #endif NEW_JIT

    tidyVerifyMethod();

MDBG( printf("Method %s %s %s (%d) at %p\n", meth->class->name->data,
             meth->name->data, meth->signature->data, num, meth->ncode); )
 
    if (flag_jit) {
        gettimeofday(&tme, 0);
        time = (tme.tv_sec - tms.tv_sec)*1000000 + (tme.tv_usec-tms.tv_usec);
        jitStat += time;
        printf("kaffe  : %30s.%20s %10lluus (%10lluus)\n",
               CLASS_CNAME(meth->class),
               meth->name->data,
               time, jitStat);
#ifdef TTTT
        time = (t1.tv_sec - tms.tv_sec)*1000000 + (t1.tv_usec-tms.tv_usec);
        printf(" %llu ", time );
        time = (t2.tv_sec - t1.tv_sec)*1000000 + (t2.tv_usec-t1.tv_usec);
        printf(" %llu", time );
        time = (t3.tv_sec - t2.tv_sec)*1000000 + (t3.tv_usec-t2.tv_usec);
        printf(" %llu", time );
        time = (t4.tv_sec - t3.tv_sec)*1000000 + (t4.tv_usec-t3.tv_usec);
        printf(" %llu\n", time );

#endif

    }

    // hacker modified for gctime flag
    if (flag_time > 0) {
        unsigned long long translate_time;
        struct timeval translate_finish;

        gettimeofday(&translate_finish, NULL);
        translate_time = (translate_finish.tv_sec - translate_start.tv_sec) \
            * 1000 + (translate_finish.tv_usec - translate_start.tv_usec) \
            / 1000;
//      printf("Translate %s:%s takes %llu usec.\n", 
//             meth->name->data, meth->class->name->data, 
//             translate_time);
        translation_total += translate_time;
    }


    
    _unlockMutex(&translatorlock);
    gc_mode = GC_ENABLED;

}

/*
 * Generate the code.
 */
void
finishInsnSequence(nativeCodeInfo* code)
{
	uint32 constlen;
	nativecode* methblock;
	nativecode* codebase;

	/* Emit pending instructions */
	generateInsnSequence();

	/* Okay, put this into malloc'ed memory */
	constlen = nConst * sizeof(union _constpoolval);
	methblock = gc_malloc(constlen + CODEPC, &gc_jit_code);
	codebase = methblock + constlen;
	memcpy(codebase, codeblock, CODEPC);
	gc_free(codeblock);

SDBG(	printf("%s.%s: allocated %d, needed %d\n", meth->class->name->data, meth->name->data, meth->codelen * codeperbytecode, constlen + CODEPC);	)

	/* Establish any code constants */
	establishConstants(methblock);

	/* Link it */
	linkLabels((uintp)codebase);

	/* Note info on the compiled code for later installation */
	code->mem = methblock;
	code->memlen = constlen + CODEPC;
	code->code = codebase;
	code->codelen = CODEPC;
}

/*
 * Install the compiled code in the method.
 */
static
void
installMethodCode(Method* meth, nativeCodeInfo* code)
{
    int i;
    jexceptionEntry* e;
    MethodInstance *methodInstance;

    /* Work out new estimate of code per bytecode */
    code_generated += code->memlen;
    bytecode_processed += meth->bcodelen;
    codeperbytecode = code_generated / bytecode_processed;

    assert( ((int) code->code) % 4 == 0 );

    GC_WRITE(meth, code->mem);
#ifdef CUSTOMIZATION
    {
        SpecializedMethod* sm;
        sm = SMS_GetTypeElement( meth->sms, GeneralType );
        if (sm == NULL) {
            sm = SM_new(GeneralType, meth);
            SMS_AddSM(meth->sms, sm);
        }

        SM_SetNativeCode( sm, code->code );
    }
#endif
    METHOD_NATIVECODE(meth) = code->code;

    meth->accflags |= ACC_TRANSLATED;

    methodInstance = MI_alloc();
    MI_SetMethod( methodInstance, meth );
    MI_SetKaffeTranslated( methodInstance );

    registerMethod( (uintp) code->code, 
		    (uintp) (code->code + code->codelen),
		    methodInstance );

    /* Flush code out of cache */
    FLUSH_DCACHE(code->code, code->code + code->codelen);

    /* Translate exception table and make it available */
    if (meth->exception_table != 0) {
        for (i = 0; i < meth->exception_table->length; i++) {
            e = &meth->exception_table->entry[i];
            e->start_pc = INSNPC(e->start_pc) + (uintp)code->code;
            e->end_pc = INSNPC(e->end_pc) + (uintp)code->code;
            e->handler_pc = INSNPC(e->handler_pc) + (uintp)code->code;
        }
    }

    /* Translate line numbers table */
    if (meth->lines != 0) {
        for (i = 0; i < meth->lines->length; i++) {
            meth->lines->entry[i].start_pc = INSNPC(meth->lines->entry[i].start_pc) + (uintp)code->code;
        }
    }
}

/*
 * Init instruction generation.
 */
void
initInsnSequence(int codesize, int localsz, int stacksz)
{
	/* Clear various counters */
	tmpslot = 0;
	maxTemp = 0;
	maxPush = 0;  
	stackno = localsz + stacksz;
	maxTemp = MAXTEMPS - 1; /* XXX */ 
	npc = 0;

	initSeq();
	initRegisters();
	initSlots(stackno);
	resetLabels();

	localinfo = &slotinfo[0];
	tempinfo = &localinfo[stackno];

	/* Before generating code, try to guess how much space we'll need. */
	codeblock_size = codesize;
	if (codeblock_size < ALLOCCODEBLOCKSZ) {
		codeblock_size = ALLOCCODEBLOCKSZ;
	}
	codeblock = gc_malloc_fixed(codeblock_size + CODEBLOCKREDZONE);
	CODEPC = 0;
}

/*
 * Generate instructions from current set of sequences.
 */
static
void
generateInsnSequence(void)
{
	sequence* t;

	for (t = firstSeq; t != currSeq; t = t->next) {

		/* If we overrun the codeblock, reallocate and continue.  */
		if (CODEPC >= codeblock_size) {
			codeblock_size += ALLOCCODEBLOCKSZ;
			codeblock = gc_realloc_fixed(codeblock, codeblock_size + CODEBLOCKREDZONE);
//			codeblock = gc_realloc_fixed(codeblock, codeblock_size + CODEBLOCKREDZONE, codeblock_size-ALLOCCODEBLOCKSZ + CODEBLOCKREDZONE);
		}

		/* Generate sequences */
		(*(t->func))(t);
	}

	/* Reset */
	initSeq();
}

/*
 * Start a new instruction.
 */
void
startInsn(sequence* s)
{
	SET_INSNPC(const_int(2), CODEPC);
}

/*
 * Mark slot not to be written back.
 */
void
nowritebackSlot(sequence* s)
{
	seq_slot(s, 0)->modified |= rnowriteback;
}

/*
 * Start a new basic block.
 */
void
startBlock(sequence* s)
{
	int i;

	/* Invalidate all slots - don't use clobberRegister which will
	 * flush them - we do not want to do that even if they are dirty.
	 */
	for (i = maxslot - 1; i >= 0; i--) {
		if (slotinfo[i].regno != NOREG) {
			register_invalidate(slotinfo[i].regno);
			slot_invalidate(&slotinfo[i]);
		}
	}
}

/*
 * Fixup after a function call.
 */
void
fixupFunctionCall(sequence* s)
{
	int i;

	/* Invalidate all slots - don't use clobberRegister which will
	 * flush them - we do not want to do that even if they are dirty.
	 */
	for (i = maxslot - 1; i >= 0; i--) {
		if (slotinfo[i].regno != NOREG && (reginfo[slotinfo[i].regno].flags & Rnosaveoncall) == 0) {
			register_invalidate(slotinfo[i].regno);
			slot_invalidate(&slotinfo[i]);
		}
	}
}

/*
 * End a basic block.
 */
void
endBlock(sequence* s)
{
	int stkno;
	int tmpslot;
	int i;

	/* Spill locals */
	for (i = 0; i < maxLocal; i++) {
		if ((localinfo[i].modified & rwrite) != 0 && localinfo[i].regno != NOREG) {
			if ((localinfo[i].modified & rnowriteback) == 0) {
				spill(&localinfo[i]);
				localinfo[i].modified = 0;
			}
			else {
				localinfo[i].modified &= ~rnowriteback;
			}
		}
	}

	/* Spill stack */
	stkno = const_int(1);
	for (i = stkno; i < maxStack+maxLocal; i++) {
		if ((localinfo[i].modified & rwrite) != 0 && localinfo[i].regno != NOREG) {
			if ((localinfo[i].modified & rnowriteback) == 0) {
				spill(&localinfo[i]);
				localinfo[i].modified = 0;
			}
			else {
				localinfo[i].modified &= ~rnowriteback;
			}
		}
	}

	/* Spill temps currently in use */
	tmpslot = const_int(2);
	for (i = 0; i < tmpslot; i++) {
		if ((tempinfo[i].modified & rwrite) != 0 && tempinfo[i].regno != NOREG) {
			if ((tempinfo[i].modified & rnowriteback) == 0) {
				spill(&tempinfo[i]);
				tempinfo[i].modified = 0;
			}
			else {
				tempinfo[i].modified &= ~rnowriteback;
			}
		}
	}
}

/*
 * Prepare register state for function call.
 */
void
prepareFunctionCall(sequence* s)
{
	int stkno;
	int tmpslot;
	int i;

	/* Spill locals */
	for (i = 0; i < maxLocal; i++) {
		if ((localinfo[i].modified & rwrite) != 0 && localinfo[i].regno != NOREG && (reginfo[localinfo[i].regno].flags & Rnosaveoncall) == 0) {
			spill(&localinfo[i]);
			localinfo[i].modified = 0;
		}
	}

	/* Spill stack */
	stkno = const_int(1);
	for (i = stkno; i < maxStack+maxLocal; i++) {
		if ((localinfo[i].modified & rwrite) != 0 && localinfo[i].regno != NOREG && (reginfo[localinfo[i].regno].flags & Rnosaveoncall) == 0) {
			spill(&localinfo[i]);
			localinfo[i].modified = 0;
		}
	}

	/* Spill temps currently in use */
	tmpslot = const_int(2);
	for (i = 0; i < tmpslot; i++) {
		if ((tempinfo[i].modified & rwrite) != 0 && tempinfo[i].regno != NOREG && (reginfo[tempinfo[i].regno].flags & Rnosaveoncall) == 0) {
			spill(&tempinfo[i]);
			tempinfo[i].modified = 0;
		}
	}
}

/*
 * Sync register state back to memory (but don't clean anything).
 */
void
syncRegisters(sequence* s)
{
	int stkno;
	int tmpslot;
	int i;
	int old_ro;

	old_ro = enable_readonce;
	enable_readonce = 0;

	/* Spill locals */
	for (i = 0; i < maxLocal; i++) {
		if ((localinfo[i].modified & rwrite) != 0 && localinfo[i].regno != NOREG && (reginfo[localinfo[i].regno].flags & Rnosaveoncall) == 0) {
			spill(&localinfo[i]);
		}
	}

	/* Spill stack */
	stkno = const_int(1);
	for (i = stkno; i < maxLocal+maxStack; i++) {
		if ((localinfo[i].modified & rwrite) != 0 && localinfo[i].regno != NOREG && (reginfo[localinfo[i].regno].flags & Rnosaveoncall) == 0) {
			spill(&localinfo[i]);
		}
	}

	/* Spill temps currently in use */
	tmpslot = const_int(2);
	for (i = 0; i < tmpslot; i++) {
		if ((tempinfo[i].modified & rwrite) != 0 && tempinfo[i].regno != NOREG && (reginfo[tempinfo[i].regno].flags & Rnosaveoncall) == 0) {
			spill(&tempinfo[i]);
		}
	}

	enable_readonce = old_ro;
}
