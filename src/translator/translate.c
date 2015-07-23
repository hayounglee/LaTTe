/* translate.c
   
   implementation of LaTTe JIT compilation system
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
               SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#include <assert.h>
#include "config.h"
#include "gtypes.h"
#include "locks.h"
#include "fast_mem_allocator.h"
#include "external.h"

#include "CFG.h"
#include "method_inlining.h"
#include "exception_info.h"

#include "translate.h"

#include "cfg_generator.h"
#include "AllocStat.h"
#include "register_allocator.h"
#include "code_gen.h"
#include "plist.h"

#include "flags.h"
#ifdef USE_TRANSLATOR_STACK
#include "thread.h"
//#include "native.h"
#endif

#include "SPARC_instr.h"
#ifdef METHOD_COUNT
#include "classMethod.h"
#include "reg.h"
static int l_trans, f_trans, n_retrans;
#endif

#ifdef CUSTOMIZATION
#include "SpecializedMethod.h"
#endif
#ifdef INLINE_CACHE
#include "TypeChecker.h"
#endif

#ifdef BYTECODE_PROFILE
#include "bytecode_profile.h"
#endif BYTECODE_PROFILE


unsigned long long jitStat = 0L;

extern void*    translator_stack;


#undef INLINE
#define INLINE
#include "translate.ic"
#include "SPARC_instr.ic"

#define  DBG(s)   
#define TDBG(s)   
#define PDBG(s) 


//
// lock to make the translation is only once at a given point
//
static quickLock      translator_lock;

int   num = 0;
int   exception_num = 4000;

FILE * time_log;

static unsigned long total_size = 0;

// hacker modified for gctime flag
unsigned long long translation_total = 0;

void*  translator_stack;

/* cfg is global variable which is not preferred */
CFG *cfg;
InlineGraph *CFGGen_current_ig_node;

PList *uninitialized_class_list = NULL;

inline static
void
print_translated_method(Method *method, int num) 
{
    fprintf(stderr, "translate begins for %s of %s(%d)\n",
            Method_GetName(method)->data,
            Class_GetName(Method_GetDefiningClass(method))->data,
            num);
    fprintf(stderr, "bytecode size = %d\ttotal size = %lu\n",
            Method_GetByteCodeLen(method),
            (total_size += Method_GetByteCodeLen(method)));
}

inline static
void
print_translated_method2(CFG *cfg) 
{
    Method *method = IG_GetMethod(CFG_GetIGRoot(cfg));

    fprintf(stderr, "text code size = %i\n", 
            CFG_GetTextSegSize(cfg));
    fprintf(stderr, "translate ends for %s of %s\n",
             Method_GetName(method)->data,
             Class_GetName(Method_GetDefiningClass(method))->data);
}

inline static
void
print_time_log(Method *method,
                struct timeval tv_st0, struct timeval tv_st1,
                struct timeval tv_st2, struct timeval tv_st3,
                struct timeval tv_st4, struct timeval tv_st5) {
    if (!time_log) time_log = fopen("time.log", "wa");

    fprintf(time_log, "%s %s %s  %luu %luu %luu %luu %luu %luu\n",
            method->class->name->data,
            method->name->data,
            method->signature->data,
            (tv_st1.tv_sec - tv_st0.tv_sec) * 1000000L 
            + (tv_st1.tv_usec - tv_st0.tv_usec),
            (tv_st2.tv_sec - tv_st1.tv_sec) * 1000000L 
            + (tv_st2.tv_usec - tv_st1.tv_usec),
            (tv_st3.tv_sec - tv_st2.tv_sec) * 1000000L 
            + (tv_st3.tv_usec - tv_st2.tv_usec),
            (tv_st4.tv_sec - tv_st3.tv_sec) * 1000000L 
            + (tv_st4.tv_usec - tv_st3.tv_usec),
            (tv_st5.tv_sec - tv_st4.tv_sec) * 1000000L 
            + (tv_st5.tv_usec - tv_st4.tv_usec),
            (tv_st5.tv_sec - tv_st0.tv_sec) * 1000000L
            + (tv_st0.tv_usec - tv_st0.tv_usec));
}

#ifdef CUSTOMIZATION
inline static
void
print_jit_stats(TranslationInfo *info, struct timeval tv_st0, 
                 struct timeval tv_st5) 
{
    Method* method = TI_GetRootMethod(info);
    SpecializedMethod* sm = TI_GetSM(info);
    unsigned long long time = (tv_st5.tv_sec - tv_st0.tv_sec) * 1000000L
        + (tv_st5.tv_usec - tv_st0.tv_usec);

    jitStat += time;
    fprintf(stderr, "newjit : %s %s %s %llu us (%llu us) ",
             method->class->name->data,
             method->name->data,
             method->signature->data,
             time, jitStat);
    if(Method_IsStatic(method)) fprintf(stderr, "S");
    if(TI_IsFromException(info)) fprintf(stderr, "[E] ");
    else if (sm != NULL && sm->tr_level > 0) fprintf(stderr, "[R] ");
    else fprintf(stderr, "[I] ");

    if (sm != NULL){
        if (SM_IsGeneralType(sm)) {
            fprintf(stderr, "GeneralType\n");
        } else if(SM_IsSpecialType(sm)){
            fprintf(stderr, "SpecialType %d\n", AS_GetCount(sm->argset));
        } else {
            fprintf(stderr, "%s\n", SM_GetReceiverType(sm)->name->data);
        }
    } else {
        fprintf(stderr, "\n");
    }
}
#else
inline static
void
print_jit_stats(Method *method, struct timeval tv_st0, 
                 struct timeval tv_st5) {
    unsigned long long time = (tv_st5.tv_sec - tv_st0.tv_sec) * 1000000L
        + (tv_st5.tv_usec - tv_st0.tv_usec);

    jitStat += time;
    fprintf(stderr, "newjit : %s %s %s %llu us (%llu us) \n",
             method->class->name->data,
             method->name->data,
             method->signature->data,
             time, jitStat);
}
#endif

/* Name        : translate
   Description : actual translation function which will process all
                 stages of JIT compilation
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
		 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
static
void
_translate(TranslationInfo *info) 
{
    Method *method;
    AllocStat    *init_map;
    
    // for debugging
    struct timeval tv_st0;
    struct timeval tv_st1;
    struct timeval tv_st2;
    struct timeval tv_st3;
    struct timeval tv_st4;
    struct timeval tv_st5;

    char file_name[256];
    FILE *file;

    num++;

    method = TI_GetRootMethod(info);

    if (TI_IsFromException(info) == false) {
#ifdef CUSTOMIZATION
        assert(TI_GetSM(info));
        if (SM_GetNativeCode(TI_GetSM(info)) != NULL) {
            TI_SetNativeCode(info, SM_GetNativeCode(TI_GetSM(info)));
            return;
        }
#else
        if (Method_IsTranslated(method)) {
            TI_SetNativeCode(info, Method_GetNativeCode(method));
            return;
        }
#endif
        if (Method_IsNative(method)) {
            Hjava_lang_Class* type;
            void* ncode;

#ifdef CUSTOMIZATION
            SpecializedMethod* sm = TI_GetSM(info);
            assert(sm != NULL);
            assert(SM_IsGeneralType(sm));
#endif

            type = TI_GetCheckType(info);

            //check if native method is loaded
            if (!Method_IsTranslated(method)) { 
                if (flag_translated_method) {
                    fprintf(stderr, "NATIVE METHOD : %s\n", method->name->data);
                }
                native(method);
            }
            ncode = METHOD_NATIVECODE(method);
#ifdef CUSTOMIZATION
            SM_SetNativeCode(sm, ncode);
#endif
            TI_SetNativeCode(info, ncode);

#ifdef INLINE_CACHE
            if(type != NULL){
                TypeChecker* tc = TC_make_TypeChecker(method, type, ncode);
                if (TC_GetHeader(tc) == NULL)
                  TC_MakeHeader(tc, method, NULL);
            }
#endif
            return;
        }
    }


    // some miscellaneous flags
    if (flag_time | flag_tr_time | flag_jit) {
        gettimeofday(&tv_st0, NULL);
    }

    if (flag_translated_method) {
        print_translated_method(method, num);
    }


#ifdef BYTECODE_PROFILE
    Pbc_init_profile(method);
#endif BYTECODE_PROFILE

    // starting memory allocation system
    FMA_start();

    if (flag_tr_time) {
        gettimeofday(&tv_st1, NULL);
    }

    // this function is replacement of phase1 & phase2
    CFGGen_generate_CFG(info);

    if (flag_tr_time) {
        gettimeofday(&tv_st2, NULL);
        tv_st3 = tv_st2;
    }

    CFG_set_variable_offsets(cfg, info);

    if (flag_vcg) {
        sprintf(file_name, "phase2_result_%d", num);
        file = fopen(file_name, "w");
        CFG_PrintInXvcg(cfg, file);
        fclose(file);
        IG_print_inline_info_as_xvcg(cfg);
    }

    if (flag_code) {
        sprintf(file_name, "phase2_asm_%d", num);
        file = fopen(file_name, "w");
        CFG_Print(cfg, file);
        fclose(file);
        IG_print_inline_info(cfg);
    }

    if (TI_GetReturnAddr(info) != 0) {
	RegAlloc_make_exception_return_map(info);
    } else {
        extern AllocStat *RegAlloc_Exception_Returning_Map;
        RegAlloc_Exception_Returning_Map = NULL;
    }

    // translation phase3
    // register allocation
    init_map = RegAlloc_perform_precoloring(cfg, info);
    
    RegAlloc_allocate_registers_on_CFG(cfg, init_map);

    if (flag_tr_time) {
        gettimeofday(&tv_st4, NULL);
    }

    if (flag_vcg) {
        sprintf(file_name, "phase3_result_%d", num);
        file = fopen(file_name, "w");
        CFG_PrintInXvcg(cfg, file);
        fclose(file);
    }

    if (flag_code) {
        sprintf(file_name, "phase3_asm_%d", num);
        file = fopen(file_name, "w");
        CFG_Print(cfg, file);
        fclose(file);
    }

    // translation phase4
    // extrace real codes
    CodeGen_generate_code(info);

// ifdef DEBUG    
#ifdef UPDATED_RETRAN    
    assert(The_Need_Retranslation_Flag != The_Final_Translation_Flag);
#endif // UPDATED_RETRAN
    l_trans += The_Need_Retranslation_Flag;
    f_trans += !The_Need_Retranslation_Flag;
// endif DEBUG

    if (flag_vcg) {
        sprintf(file_name, "phase4_result_%d", num);
        file = fopen(file_name, "w");
        CFG_PrintInXvcg(cfg, file);
        fclose(file);
    }

    if (flag_code) {
        sprintf(file_name, "phase4_asm_%d", num);
        file = fopen(file_name, "w");
        CFG_Print(cfg, file);
        fclose(file);
    }

    if (flag_translated_method) {
        print_translated_method2(cfg);
    }

    // finish memory system
    FMA_end();

    if (flag_tr_time | flag_time | flag_jit) {
        gettimeofday(&tv_st5, NULL);

        if (flag_tr_time) {
            print_time_log(method, tv_st0, tv_st1, tv_st2, tv_st3, 
                            tv_st4, tv_st5);
        }
        
        if (flag_time) {
            // calculate tr time in micro seconds.
            unsigned long long tr_time 
                = (tv_st5.tv_sec - tv_st0.tv_sec) * 1000 
                + (tv_st5.tv_usec - tv_st0.tv_usec) / 1000;
            translation_total += tr_time;
        }

        if (flag_jit) {
#ifdef CUSTOMIZATION
            print_jit_stats(info, tv_st0, tv_st5);
#else
            print_jit_stats(method, tv_st0, tv_st5);
#endif
        }
    }

    /* passing root method instance to exception manager */
    TI_SetRootMethodInstance(info, IG_GetMI(CFG_GetIGRoot(cfg)));
    
    return;
}

inline static
void
process_unloaded_classes() {
    PList *uninit_class_list;

    for(uninit_class_list = uninitialized_class_list; 
         uninit_class_list != NULL;
         uninit_class_list = PList_GetNext(uninit_class_list)) {
        processClass(PList_GetValue(uninit_class_list), CSTATE_OK);
    }

    /* FIXME : because of some unknown bug, we comment out free routine */
//  PList_free(uninitialized_class_list);
}

void
translate(TranslationInfo *info) {
    register void *stack_pointer asm("%sp");
    static void *old_sp;        // This should not be a stack-allocated value.

    gc_mode = GC_DISABLED;

    _lockMutex(&translator_lock);

    uninitialized_class_list = NULL;

    old_sp = stack_pointer;
    stack_pointer = translator_stack;

    _translate(info);

    stack_pointer = old_sp;

    _unlockMutex(&translator_lock);
    gc_mode = GC_ENABLED;

    process_unloaded_classes();
}



#ifdef METHOD_COUNT

/* Replace beginning of old code with call site fixer.

   Assembly code is as follows:

   save %sp, -96, %sp

   ; store address of new code
   sethi %hi(<new code>), %l0
   or %l0, %lo(<new code>), %l0

   ; construct call op
   sethi %hi(<call opcode>), %l2
   sub %l0, %i7, %l1
   srl %l1, 2, %l1
   or %l1, %l2, %l1

   ; fix call site
   st %l1, [%i7]
   flush %i7

   ; restore register window and jump to start of new code
   jmp %l0
   restore %g0, %g0, %g0

   The following code writes the above at the beginning of the old
   code. */

void
generate_fixed_call_fixup(nativecode* ocode, nativecode* ncode)
{
    unsigned* instr = (unsigned*)ocode;

    *(instr++) =
      SPARC_SAVE | (sp << 25) | (sp << 14) | (1 << 13) | (0x1fff & -96);

    *(instr++) = SETHI | (l0 << 25) | HI(ncode);
    *(instr++) = OR | (l0 << 25) | (l0 << 14) | (1 << 13) | LO(ncode);

    *(instr++) = SETHI | (l2 << 25) | HI(CALL);
    *(instr++) = SUB | (l1 << 25) | (l0 << 14) | i7;
    *(instr++) = SRL | (l1 << 25) | (l1 << 14) | (1 << 13) | 2;
    *(instr++) = OR | (l1 << 25) | (l1 << 14) | l2;

    *(instr++) = ST | (l1 << 25) | (i7 << 14) |  (1 << 13);
    *(instr++) = FLUSH | (i7 << 14) | (1 << 13);

    *(instr++) = JMPL | (g0 << 25) | (l0 << 14) | (1 << 13) | 0;
    *(instr++) = RESTORE | (g0 << 25) | (g0 << 14) | g0;


    FLUSH_DCACHE(ocode, instr);
}

void
generate_fixed_call_fixup_for_check_code(nativecode* ocode, nativecode* ncode)
{
    unsigned* instr;
    unsigned* _ocode = (unsigned*) ocode;
    unsigned* _ncode = (unsigned*) ncode;

    _ocode = _ocode - OFFSET_OF_CODE_START_FROM_BOTTOM;
    _ncode = _ncode - OFFSET_OF_CODE_START_FROM_BOTTOM;

    instr = _ocode;
    
    *(instr++) =
      SPARC_SAVE | (sp << 25) | (sp << 14) | (1 << 13) | (0x1fff & -96);

    *(instr++) = SETHI | (l0 << 25) | HI(_ncode);
    *(instr++) = OR | (l0 << 25) | (l0 << 14) | (1 << 13) | LO(_ncode);
    *(instr++) = BA | OFFSET_OF_CODE_START_FROM_BOTTOM;
    *(instr++) = SPARC_NOP;
    *(instr++) = 0;

    FLUSH_DCACHE(_ocode, instr);
}

#ifdef RETRAN_CALLER
typedef struct CallerInfo {
    Method *caller;
    int count;
} CallerInfo;

static
Method *
selectRetranslationMethod(Method *method, int depth) {
    CallerInfo *callers = 
        (CallerInfo *) alloca(sizeof(CallerInfo) 
                               * The_Retranslation_Threshold);
    int callerNO = 1;
    int i, j;

//     fprintf(stderr, "Now seeking ");
//     fprintf(stderr, "%s.%s.%s", 
//              method->class->name->data,
//              method->name->data,
//              method->signature->data);
//     fprintf(stderr, " method's callers\n");
    
    if (depth == 0) return method;

    // reserve slot 0 for native method
    callers[0].caller = NULL;

    // collect callers Method structure
    for(i = 0; i < The_Retranslation_Threshold; i++) {
        void *addr = method->callers[i];
        MethodInstance *instance = findMethodFromPC((uintp) addr);
        if (instance != NULL) {
            Method *caller = MI_get_method(instance);

            for(j = 1; j < callerNO; j++) {
                if (callers[j].caller == caller) {
                    callers[j].count++;
                }
            }

            if (j == callerNO) {
                callers[callerNO].caller = caller;
                callers[callerNO].count = 1;
                callerNO++;
            }
        } else {
            // native methods
            callers[0].count++;
        }
    }

    // sort collected Method structure
    for(i = 0; i < callerNO; i++) {
        for(j = i + 1; j < callerNO; j++) {
            if (callers[i].count < callers[j].count) {
                CallerInfo swapTMP;

                swapTMP = callers[j];
                callers[j] = callers[i];
                callers[i] = swapTMP;
            }
        }
    }

    // select retranslation method

//     fprintf(stderr, "selected caller : ");
//     if (callers[0].caller == NULL) fprintf(stderr, "null\n");
//     else fprintf(stderr, "%s %s %s\n",
//                   callers[0].caller->class->name->data,
//                   callers[0].caller->name->data,
//                   callers[0].caller->signature->data);

    // if frequent caller is native method, translate itself.
    if (callers[0].caller == NULL) return method;
    if (equalUtf8Consts(callers[0].caller->name, init_name)) return method;

    // if this method is inlinable to frequent caller, translate caller.
    if (method_is_inlinable(callers[0].caller, method)
         && callers[0].caller->tr_level == 0) 
      return selectRetranslationMethod(callers[0].caller, depth - 1);
    else
      return method;

}

#endif // RETRAN_CALLER

/* Retranslate given method.  Transfers control to new code after
   translation is finished. */

void
#ifdef CUSTOMIZATION
retranslate(SpecializedMethod *sm, Hjava_lang_Object* obj)
#else
retranslate(Method *method)
#endif
{
    nativecode *ocode, *ncode;
    TranslationInfo info = {NULL,};
#ifdef CUSTOMIZATION
    Hjava_lang_Class* class = SM_GetReceiverType(sm);
    Method *selectedMethod = SM_GetMethod(sm);
#else
    Method *selectedMethod = method;
#endif


    TI_SetRootMethod(&info, selectedMethod);
    /* Increase method count so that some other thread wouldn't try to
       retranslate the method while the translating thread is
       sleeping. */
#ifdef CUSTOMIZATION
    sm->count++;

    n_retrans++; /* Increase retranlation count. */
    sm->tr_level++; /* Increase translation level. */

    /* Set method to untranslated state. */
    ocode = SM_GetNativeCode(sm);
    SM_SetNativeCode(sm, NULL);

    TI_SetSM(&info, sm);

#ifdef INLINE_CACHE
    if (SM_GetTypeChecker(sm) != NULL)
      TI_SetCheckType(&info, TC_GetCheckType(SM_GetTypeChecker(sm)));
#endif
#else
    method->count++;

    n_retrans++; /* Increase retranlation count. */
    selectedMethod->tr_level++; /* Increase translation level. */

    /* Set method to untranslated state. */
    ocode = METHOD_NATIVECODE(selectedMethod);
    selectedMethod->accflags &= ~ACC_TRANSLATED;
    METHOD_NATIVECODE(selectedMethod) = NULL;
#endif

    /* Do the actual retranslation. */
    translate(&info);

#ifdef CUSTOMIZATION
    ncode = TI_GetNativeCode(&info);
    assert(ncode != NULL);
#else
    ncode = METHOD_NATIVECODE(selectedMethod) = TI_GetNativeCode(&info);
#endif    
    assert(ocode != ncode);

#ifdef CUSTOMIZATION
    /* Update dispatch tables. */
    fixupDtableItable(selectedMethod, class, ncode);
#else
    fixupDtableItable(selectedMethod, NULL, ncode);

#endif

#ifdef INLINE_CACHE
    TC_fixup_TypeChecker_headers(selectedMethod, ocode, ncode);
#endif

    generate_fixed_call_fixup(ocode, ncode);

#ifdef INLINE_CACHE
    generate_fixed_call_fixup_for_check_code(ocode, ncode);
#endif
    /* FIXME: we probably should be freeing all the obsolete data
       structures such as exception information tables (like above,
       just leaving the old data structures alone does not seem to be
       too much of an overhead).  Also, the address range for the old
       code should be deregistered (not really necessary currently,
       but it will be a must when we free the slack at the end of old
       code). */

#ifdef CUSTOMIZATION
    sm->count = 0;  
#else
    method->count = 0; /* Reset method run count. */
    ncode = method->ncode;
#endif /* CUSTOMIZATION */

    /* Transfer control to new code.  I hate using a global register,
       but there isn't much of a choice.  The extra restore is to
       restore the register window of the caller. */
    asm volatile ("mov %0, %%g1" :  : "r" (ncode));
    asm volatile ("restore\n\t"
		  "jmp %g1\n\t"
		  "restore\n\t");
}

void
#ifdef CUSTOMIZATION
generate_count_code(SpecializedMethod* m, unsigned* code, unsigned* target)
#else
generate_count_code(Method* m, unsigned* code, unsigned* target)
#endif
{
#ifdef VARIABLE_RET_COUNT 
    int ret_threshold;
    ret_threshold = The_Retranslation_Threshold + 
#ifdef CUSTOMIZATION
        Method_GetBranchNO(SM_GetMethod(m))*2;
#else
        Method_GetBranchNO(m)*2;
#endif
    if (ret_threshold > The_Max_Retranslation_Threshold) 
      ret_threshold = The_Max_Retranslation_Threshold;

    m->count = ret_threshold;
#else /* not VARIABLE_RET_COUNT */
    m->count = The_Retranslation_Threshold;
#endif /* not VARIABLE_RET_COUNT */

    if (code == NULL) {
        assert(0 && "currently not reachable");
    }

    // load method or Specialized method pointer
    *code++ = assemble_instr2(SETHI, g1, HI(m));
    *code++ = assemble_instr5(OR, g1, g1, LO(m));
    // load counter varible
    *code++ = assemble_instr5(LD, g2, g1, (char*)(&m->count)-(char*)m);
    *code++ = assemble_instr5(SUBCC, g0, g2, 0);
    *code++ = assemble_instr3(BNE, 6);
    *code++ = assemble_instr5(SUB, g2, g2, 1); //delay slot
    // call retranslate
    *code++ = assemble_instr5(SPARC_SAVE, SP, SP, -96);
    *code++ = assemble_instr5(ADD, o0, g1, 0);
    *code = assemble_instr1(CALL, ((int)retranslate - (int)code) >> 2);
    code++;
    *code++ = assemble_instr5(ADD, o1, i0, 0);
    // store changed called count
    *code++ = assemble_instr8(ST, g2, g1, (char*)(&m->count)-(char*)m);

    assert(code == target);
}

/* Print translation statistics (number of virgin translations and
   retranslations).

   Ignore translation of exception handlers*/
void
print_trstats (void)
{
    int t_trans = l_trans + f_trans - n_retrans;

    fprintf(stderr, "light translations    = %d (%3.2f %%)\n", 
             l_trans, (float) l_trans / t_trans * 100);
    fprintf(stderr, "full traslations      = %d (%3.2f %%)\n",
             f_trans - n_retrans, 
             (float) (f_trans - n_retrans) / t_trans * 100);
//     fprintf(stderr, "full translations     = %d (%3.2f %%)\n", 
//              f_trans, (float) f_trans / t_trans * 100);
    fprintf(stderr, "retranslations        = %d (%3.2f %%, %3.2f %%)\n", 
             n_retrans, (float) n_retrans / (t_trans) * 100,
             (float) n_retrans / l_trans * 100);
}
#endif /* METHOD_COUNT */
