/* exception_handler.c
   
   Includes implementation of exception handling mechanism for our JIT
   compilation system
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
               SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

/* FIXME: beware of pointer handling. It may not work correctly in 64
          bit system */

#include <stdio.h>
#include <alloca.h>
#include "config.h"
#include "exception_handler.h"
#include "exception.h"
#include "native.h"
#include "errors.h"
#include "thread.h"
#include "file.h"
#include "constants.h"
#include "stackTrace.h"
#include "lookup.h"
#include "flags.h"
#include "locks.h"
#include "java_lang_Throwable.h"
#include "../translator/SPARC_instr.h"
#include "../translator/reg.h"

#ifdef INTERPRETER
#include "interpreter.h"
#endif
#ifdef INLINE_CACHE
#include "cell_node.h"
#endif
#ifdef TRANSLATOR
#include "exception_info.h"
#include "translate.h"
#include "method_inlining.h"
#include "class_inclusion_test.h"
#include "reg_alloc_util.h"
#include "translator_driver.h"


#endif /* TRANSLATOR */

#define OWNER_ID_MASK   0xffff

/* saved area for out registers */
static uint64 _out_registers[3];/* in order to use "ldd" */
static uint32* out_registers = (uint32*) _out_registers;

#ifdef TRANSLATOR

static int find_handler(Method *method, int bpc, Hjava_lang_Class *eclass);
static void handle_latte_methods(MethodInstance *mi,
				 exceptionFrame *frame,
				 Hjava_lang_Class *ec,
				 Hjava_lang_Throwable *eo,
				 MethodInstance *gen,
				 void *generated_site,
				 exceptionFrame *gen_sp);

#if defined(TRANSLATOR) && !defined(INTERPRETER)
static void handle_kaffe_methods(exceptionFrame *frame,
				 Hjava_lang_Class *eclass,
				 Hjava_lang_Throwable *e);
#endif /* defined(TRANSLATOR) && !defined(INTERPRETER) */

static void exit_monitor(MethodInstance *mi, ExceptionInfo *info,
			 exceptionFrame *frame);
static void invoke_exception_handler(Hjava_lang_Throwable *eobject,
				     exceptionFrame *frame,
				     MethodInstance *mi, 
				     int entry_idx,
				     jexceptionEntry *entry,
				     ExceptionInfo *info,
				     MethodInstance *catcher,
				     MethodInstance *generator,
				     void *generated_site,
				     exceptionFrame *generated_sp);
static void adjust_variable_values(MethodInstance *mi, ExceptionInfo *info,
				   int *variable_map, int local_no,
				   int stack_no,
				   exceptionFrame *sp);
static void copy_register_value(int *org_sp, int *org_fp, int *sp, int *fp,
				int org_reg, int new_reg, uint32 *saved_out);
static int append_copy_code(int *resolve_code, int code_pc,
			    int org_reg, int new_reg);
static void attach_direct_connect_codes(Hjava_lang_Throwable *eobject,
					Hjava_lang_Class *catch_type,
					void *translated_code,
					void *generated_site,
					MethodInstance *mi,
					ExceptionInfo *info,
					int *variable_map,
					int local_no, int stack_no);

#endif /* TRANSLATOR */

/* Name        : EH_set_native_handler
   Description : Set a native exception handler for the current thread.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     HANDLER is the address of the native exception handler.
     START and END specify the native code range where exceptions should
       be caught from.
   Post-condition:
     The list of native exception handlers is set appropriately for
     the current thread. */
void
EH_set_native_handler (struct _EH_native_handler *handler, void *fp)
{
    handler->fp = fp;
    handler->parent = TCTX(currentThread)->native_handlers;
    TCTX(currentThread)->native_handlers = handler;
}

/* Name        : EH_release_native_handler
   Description : Release a native exception handler.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Pre-condition:
     HANDLER is the native exception handler to be released.
   Notes:
     Care should be taken so that this is always called when the
     handler is no longer necessary, such as after when code in an
     exception handling domain executes normally, or when restoring a
     stack state of an ancestor.  The reason is that the exception
     handling system looks only at the most recently added native
     exception handler. */
void
EH_release_native_handler (struct _EH_native_handler *handler)
{
    assert(TCTX(currentThread)->native_handlers == handler);

    TCTX(currentThread)->native_handlers = handler->parent;
}

/* Name        : is_frame_ok
   Description : check stack pointer and frame pointer to find if
                 current stack frame is within thread stack area
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
inline static
bool
is_frame_ok(exceptionFrame* frame)
{
    return (frame->retbp >= (int) TCTX(currentThread)->stackBase &&
	    frame->retbp < (int) TCTX(currentThread)->stackEnd &&
	    frame->retpc != 0);
}


/* Name        : EH_exception_manager
   Description : exception_manager function has duty of finding,
                 translating, and transfer control to appropriate
                 exception handler
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
		 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   	exception_object : exception object thrown
	stack_frame : stack pointer value when the exception is
	              generated
   Post-condition:
   
   Notes:  */
void
EH_exception_manager(Hjava_lang_Throwable *eobject, 
		     exceptionFrame *stack_frame)
{
    Hjava_lang_Class *exception_class;
    exceptionFrame *frame;
    struct _EH_native_handler *native_handler;

#ifdef TRANSLATOR
    MethodInstance *generator;
    void *generated_site;
    exceptionFrame *generated_sp;
#endif /* TRANSLATOR */

    blockInts = 1;

    intsRestore();

    assert(TCTX(currentThread)->status == THREAD_RUNNING);

#ifdef TRANSLATOR
    /* Record what method generates exception and where it occurs */
    generated_site = (void *) stack_frame->retpc;
    generated_sp = (exceptionFrame *) stack_frame->retbp;
    generator = findMethodFromPC((uintp) generated_site);
#endif /* TRANSLATOR */

    /* If exception object is null, switch it to a
       NullPointerException. */
    if (eobject == NULL) {
        eobject = (Hjava_lang_Throwable *) NullPointerException;
    }

    /* get exception class from exception object. TEMPORARY */
    exception_class = ((Hjava_lang_Object *) eobject)->dtable->class;

    /* for debugging */
    if (flag_exception) {
        fprintf(stderr, "exception manager works for %s.\n",
		exception_class->name->data);
    }

    frame = stack_frame;
    native_handler = TCTX(currentThread)->native_handlers;
    while (is_frame_ok(frame)) {
	/* Handle exception generated in native code. */
	if (frame == native_handler->fp) {
	    native_handler->exception = eobject;
	    EH_release_native_handler(native_handler);

	    /* Jump to exception handler. */
	    longjmp(native_handler->state, 1);
	}

#ifdef INTERPRETER
	/* Handle exception generated in interpreter. */
	if (intrp_inside_interpreter((void*)frame->retpc)) {
	    int handpc;

	    handpc = intrp_find_handler(frame, exception_class);
	    if (handpc != -1)
		intrp_invoke_handler(frame, eobject, handpc);
	    else
		frame = intrp_unwind_stack(frame);

	    continue;
	}
#endif /* INTERPRETER */

#ifdef TRANSLATOR
	{
	    /* Handle translated method. */
	    MethodInstance *instance;

	    instance = findMethodFromPC(frame->retpc);

	    if (instance != NULL) {
#ifdef INTERPRETER
		/* Handle translated method. */
		handle_latte_methods(instance, frame, exception_class,
				     eobject, generator, generated_site,
				     generated_sp);
#else /* not INTERPRETER */
		if (!MI_IsLaTTeTranslated(instance))
		    handle_kaffe_methods(frame, exception_class, eobject);
		else
		    handle_latte_methods(instance, frame,
					 exception_class, eobject,
					 generator, generated_site,
					 generated_sp);
#endif /* not INTERPRETER */
	    }
	}
#endif /* TRANSLATOR */

	frame = (exceptionFrame *)frame->retbp; /* Unwind stack. */

    }

#ifdef REG_WINDOW_DBG
    do_not_check_stack_ptr = false;
#endif

    //
    // No handler for this exception is in this thread.
    // Call the uncaught exception method on the group of this thread.
    //
    do_execute_java_method(0,
			   (Hjava_lang_Object*) unhand(currentThread)->group,
			   "uncaughtException",
			   "(Ljava/lang/Thread;Ljava/lang/Throwable;)V",
			   0,
			   0,
			   currentThread,
			   eobject);

    killThread();
}

#ifdef TRANSLATOR

/* Name        : find_handler
   Description : check exception table of method if it has appropriate
                 exception handler
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
static
int
find_handler(Method *method, int bpc, Hjava_lang_Class *eclass)
{
    jexception *etable;
    jexceptionEntry *entry;
    int i;

    etable = Method_GetExceptionTable(method);

    if (etable == NULL) return -1;

    for(i = 0; i < JE_GetLength(etable); i++) {
	int start_pc, end_pc;
	
	entry = JE_GetEntry(etable, i);
	start_pc = JEE_GetStartPC(entry);
	end_pc = JEE_GetEndPC(entry);

	if (start_pc <= bpc && bpc < end_pc) {
	    Hjava_lang_Class *catch_class;
	    
	    if (JEE_GetCatchIDX(entry) == 0) return i;

	    catch_class = JEE_GetCatchType(entry);

	    /* resolve class if not resolved.
	       I'm not sure if this is right moment to resolve a class. */
	    if (catch_class == NULL) {
		catch_class = getClass(JEE_GetCatchIDX(entry),
				       Method_GetDefiningClass(method));
		JEE_SetCatchType(entry, catch_class);
	    }
	    
	    if (CIT_is_subclass(eclass, catch_class)) return i;
	}
    }

    return -1;
}

/* Name        : handle_latte_methods
   Description : handle the codes that are translated by LaTTe JIT
                 compilation system
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
       generator : actually exception generated method instance
       mi : outer method instance of current stack frame
   Post-condition:
   
   Notes: frame is the stack point of callee of the current method  */
static
void
handle_latte_methods(MethodInstance *mi, exceptionFrame *frame,
		     Hjava_lang_Class *eclass,
		     Hjava_lang_Throwable *eobject,
		     MethodInstance *generator,
		     void *generated_site,
		     exceptionFrame *generated_sp)
{
    /* LaTTe exception handling sequence. */
    ExceptionInfoTable *info_table;

    info_table = MI_GetExceptionInfoTable(mi);
    
    if (info_table != NULL) {
	ExceptionInfo *info;
	
	info = EIT_find(info_table, frame->retpc);
	
	if (EIT_GetNativePC(info) != 0) {
	    MethodInstance *actual_instance;
            int bpc;

	    actual_instance = EIT_GetMethodInstance(info);
            bpc = EIT_GetBytecodePC(info);
	    
	    do {
		Method *m;
		jexceptionEntry *entry;
		int entry_index;
		
		m = MI_GetMethod(actual_instance);
			    
		if (flag_exception) /* for debugging */
		    fprintf(stderr, "backtrace at %s %s %s\n",
			    m->class->name->data, m->name->data,
			    m->signature->data);

		entry_index = find_handler(m, bpc, eclass);

		if (entry_index != -1) {
		    entry = JE_GetEntry(Method_GetExceptionTable(m),
					entry_index);
		    /* appropriate exception handler found */
		    invoke_exception_handler(eobject,
					     (exceptionFrame *) frame->retbp,
					     actual_instance,
					     entry_index, entry, info, mi,
					     generator, generated_site,
					     generated_sp);
		}

		/* unlock monitor if needed */
		if (Method_IsSynchronized(m))
		    exit_monitor(actual_instance, info,
				 (exceptionFrame *) frame->retbp);

                bpc = MI_GetRetBPC(actual_instance);
		actual_instance = MI_GetCaller(actual_instance);
	    } while(actual_instance != NULL);
	}
    }
}

#if defined(TRANSLATOR) && !defined(INTERPRETER)
/* Name        : handle_kaffe_methodss
   Description : handle the codes that are translated by Kaffe JIT
                 compilation system
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
static
void
handle_kaffe_methods(exceptionFrame *frame,
		     Hjava_lang_Class *exception_class,
		     Hjava_lang_Throwable *exception)
{
    exceptionInfo einfo;
    Hjava_lang_Object *obj;
    
    /* Kaffe's exception handling sequence. */
    findExceptionInMethod(frame->retpc, 
			  exception_class, &einfo);
    if (flag_exception) {
	fprintf(stderr, "exception handler by kaffe JIT\n");
    }

    /* Find the sync. object */
    if (einfo.method == 0 
	|| (einfo.method->accflags & ACC_SYNCHRONISED) == 0) {
	obj = 0;
    } else if (einfo.method->accflags & ACC_STATIC) {
	obj = &einfo.class->head;
    } else {
	obj = FRAMEOBJECT(frame);
    }
          
    /* Handler found */     
    if (einfo.handler != 0) {
	if (flag_exception) printf("CALL_KAFFE_EXCEPTION\n");

	CALL_KAFFE_EXCEPTION(frame, einfo, exception);
    }
    if (obj != 0) _unlockMutex(obj);
}
#endif /* defined(TRANSLATOR) && !defined(INTERPRETER) */

static
int
calculate_gen_map_size(MethodInstance *mi)
{
    return MI_GetCallerLocalNO(mi) + MI_GetCallerStackNO(mi) + MAX_TEMP_VAR
        + Method_GetLocalSize(MI_GetMethod(mi));
}

/* Name        : manipulate_variable_maps
   Description : creats gen & ret variable map information for translation
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
       mi is actual(inner) method instance
       catcher is outer method instance
   Post-condition:
   
   Notes:  */
static
void
manipulate_variable_maps(MethodInstance *mi, int *gen_map,
                         ExceptionInfo *info)
{
    int im_local_no;	/* Total local variable boundary */
    int im_stack_no;	/* Total stack variable boundary */
    int gm_local_no;
    int gm_stack_no;

    int local_no;
    int tmp_no;

    int *info_map = EIT_GetVariableMap(info);

    MethodInstance *ginstance = EIT_GetMethodInstance(info);

    int gen_map_idx = 0;
    int i;

    
    im_local_no = MI_GetCallerLocalNO(ginstance)
        + Method_GetLocalSize(MI_GetMethod(ginstance));
    im_stack_no = MI_GetCallerStackNO(ginstance)
        + Method_GetStackSize(MI_GetMethod(ginstance));

    gm_local_no = MI_GetCallerLocalNO(mi);
    gm_stack_no = MI_GetCallerStackNO(mi);

    local_no = Method_GetLocalSize(MI_GetMethod(mi));
    tmp_no = MAX_TEMP_VAR;

    for(i = 0; i < gm_local_no + local_no; i++) {
        gen_map[gen_map_idx++] = info_map[i];
    }

    for(i = im_local_no; i < im_local_no + gm_stack_no; i++) {
        gen_map[gen_map_idx++] = info_map[i];
    }

    for(i = im_local_no + im_stack_no;
        i < im_local_no + im_stack_no + MAX_TEMP_VAR; i++) {
        gen_map[gen_map_idx++] = info_map[i];
    }
}

/* Name        : invoke_exception_handler
   Description : transfer control into found exception handler.
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
       mi is actual method instance
       catcher is outer method instance
   Post-condition:
   
   Notes: translate bytecodes of exception handler if needed.  also,
          try direct connection if possible. frame is the stack point
          of the current method. (NOT callee!)  */
static
void
invoke_exception_handler(Hjava_lang_Throwable *eobject,
			 exceptionFrame *frame, MethodInstance *mi, 
			 int entry_idx, jexceptionEntry *entry,
			 ExceptionInfo *info, MethodInstance *catcher,
			 MethodInstance *generator, void *generated_site,
			 exceptionFrame *generated_sp)
{
    Method *method;
    void *translated_code;
    int *variable_map;
    int stack_no, local_no;
    
    method = MI_GetMethod(mi);

    if (MI_IsEmptyTranslatedEHCodes(mi)) MI_AllocTranslatedEHCodes(mi);
    
    translated_code = MI_GetTranslatedEHCode(mi, entry_idx);

    if (translated_code == NULL) {
	/* now exception handler codes will be translated into native
           codes */
	MethodInstance *handler_instance;
	TranslationInfo tiinfo = {0, };
        int *gen_map;

	TI_SetRootMethod(&tiinfo, method);

        if (mi == catcher) {
            TI_SetGenMap(&tiinfo, EIT_GetVariableMap(info));
        } else {
            gen_map = (int *) alloca(sizeof(int) * calculate_gen_map_size(mi));

            manipulate_variable_maps(mi, gen_map, info);

            TI_SetGenMap(&tiinfo, gen_map);
        }

	TI_SetRetMap(&tiinfo, MI_GetCallerVarMap(mi));
	TI_SetStartBytecodePC(&tiinfo, JEE_GetHandlerPC(entry));
	TI_SetFromException(&tiinfo);
	TI_SetCallerLocalNO(&tiinfo, MI_GetCallerLocalNO(mi));
	TI_SetCallerStackNO(&tiinfo, MI_GetCallerStackNO(mi));

	if (MI_GetCaller(mi) == NULL) {
	    TI_SetReturnAddr(&tiinfo, 0);
	    TI_SetReturnStackTop(&tiinfo, -1);
	} else {
	    TI_SetReturnAddr(&tiinfo, MI_GetRetNativePC(mi));
	    TI_SetReturnStackTop(&tiinfo, MI_GetRetStackTop(mi));
	}

	translate(&tiinfo);

	handler_instance = TI_GetRootMethodInstance(&tiinfo);

	/* have exception information as common between method A and
           exception handler of method A */
	handler_instance->translatedEHCodes = mi->translatedEHCodes;
	handler_instance->initEHVarMap = mi->initEHVarMap;
	handler_instance->localVarNO = mi->localVarNO;
	handler_instance->stackVarNO = mi->stackVarNO;
	
	translated_code = TI_GetNativeCode(&tiinfo);
	variable_map = TI_GetInitMap(&tiinfo);
	local_no = TI_GetLocalVarNO(&tiinfo);
	stack_no = TI_GetStackVarNO(&tiinfo);
	MI_SetTranslatedEHCode(mi, entry_idx, translated_code);
	MI_SetInitEHVarMap(mi, entry_idx, variable_map);
	MI_SetLocalVarNO(mi, entry_idx, local_no);
	MI_SetStackVarNO(mi, entry_idx, stack_no);
    } else {
	variable_map = MI_GetInitEHVarMap(mi, entry_idx);
	local_no = MI_GetLocalVarNO(mi, entry_idx);
	stack_no = MI_GetStackVarNO(mi, entry_idx);
    }

    if (flag_exception)
	fprintf(stderr, "%dth exception handler of %s %s %s\n",
		entry_idx, method->class->name->data,
		method->name->data, method->signature->data);

    adjust_variable_values(mi, info, variable_map, local_no, stack_no,
			   (exceptionFrame *) frame);

    /* if generated method and catching method is the same method and
       it is thrown via athrow statement, try direct connection */
    if (generator == catcher
	&& ((*((int *)generated_site) & 0xc0000000) == (0x1 << 30))
	&& ((int) frame == (int) generated_sp)
	&& ((*((int *)generated_site) & 0x3fffffff)
	    == (((unsigned)(((int) throwExternalException)
			    - ((int) generated_site))) >> 2))) {

	attach_direct_connect_codes(eobject, JEE_GetCatchType(entry),
				    translated_code, generated_site,
				    mi, info, variable_map,
				    local_no, stack_no);

    }
    
    asm volatile (
	"ta      3				\n
         add     %0, %%g0, %%g1			\n
         add     %2, %%g0, %%fp			\n
         add     %3, %%g0, %%g3			\n
         restore %1,   0,  %%g2			\n
         ldd     [%%g3],    %%o0		\n
         ldd     [%%g3+8],  %%o2		\n
         ldd     [%%g3+16], %%o4		\n
         jmp     %%g1				\n
         ldd     [%%sp+56], %%i6		\n"
	: /* no outputs */
	: "r" (translated_code), "r" (eobject),
	  "r" (frame), "r" (out_registers)
	: "g1", "g2", "g3", "o0", "o1", "o2", "o3", "o4", "o5");
}

/* Name        : adjust_variable_values
   Description : match variable maps before transfer control to
                 exception handler
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
static
void
adjust_variable_values(MethodInstance *mi, ExceptionInfo *info,
		       int *variable_map, int local_no, int stack_no,
		       exceptionFrame *sp)
{
    MethodInstance *ginstance;
    int *saved_sp, *saved_fp;
    int caller_local_no, caller_stack_no;
    int gcaller_local_no, gcaller_stack_no, glocal_no;
    int *original_variable_map;
    int org_reg, new_reg;
    int *fp;
    uint64 _saved_out[3];
    uint32 *saved_out = (uint32 *) _saved_out;
    int i;

    original_variable_map = EIT_GetVariableMap(info);
    ginstance = EIT_GetMethodInstance(info);
    gcaller_local_no = MI_GetCallerLocalNO(ginstance);
    gcaller_stack_no = MI_GetCallerStackNO(ginstance);
    glocal_no = Method_GetLocalSize(MI_GetMethod(ginstance));

    caller_local_no = MI_GetCallerLocalNO(mi);
    caller_stack_no = MI_GetCallerStackNO(mi);
    
    /* save stack frame */
    fp = (int *) sp->retbp;
    saved_sp = (int *) alloca((int) fp - (int) sp);
    saved_fp = (int *) ((int) saved_sp + ((int) fp - (int) sp));
    memcpy(saved_sp, sp, (int) fp - (int) sp);
    memcpy(saved_out, out_registers, sizeof(_saved_out));
    
    /* match caller's local variables */
    for(i = 0; i < caller_local_no; i++) {
	int org_reg = original_variable_map[i] & 0x0fffffff;
	int new_reg = variable_map[i];

	if (new_reg != -1 && org_reg != 0x0fffffff && org_reg != new_reg)
	    copy_register_value(saved_sp, saved_fp, (int *) sp, fp,
				org_reg, new_reg, saved_out);
    }
    
    /* match caller's stack variables */
    for(i = 0; i < caller_stack_no; i++) {
	int offset = gcaller_local_no + glocal_no;
	    
	org_reg = original_variable_map[i + offset] & 0x0fffffff;
	new_reg = variable_map[i + local_no];

	if (new_reg != -1 && org_reg != 0x0fffffff && org_reg != new_reg)
	    copy_register_value(saved_sp, saved_fp, (int *) sp, fp,
				org_reg, new_reg, saved_out);
    }

    /* match caller's temporary variables */
    if (MI_GetCaller(mi) != NULL) {
	for(i = 0; i < MAX_TEMP_VAR; i++) {
	    int offset = gcaller_local_no + gcaller_stack_no + glocal_no;
	    
	    org_reg = original_variable_map[i + offset] & 0x0fffffff;
	    new_reg = variable_map[i + local_no + stack_no];

	    if (new_reg != -1 && org_reg != 0x0fffffff && org_reg != new_reg)
		copy_register_value(saved_sp, saved_fp, (int *) sp, fp,
				    org_reg, new_reg, saved_out);
	}
    }

    /* match local variables */
    for(i = 0; i < Method_GetLocalSize(MI_GetMethod(mi)); i++) {
	org_reg = original_variable_map[i + gcaller_local_no] & 0x0fffffff;
	new_reg = variable_map[i + caller_local_no];

	if (new_reg != -1 && org_reg != 0x0fffffff && org_reg != new_reg)
	    copy_register_value(saved_sp, saved_fp, (int *) sp, fp,
				org_reg, new_reg, saved_out);
    }
}

/* Name        : copy_register_value
   Description : appropriately adjust values in stack frame.
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: Currently we do not handle floating point register  */
static
void
copy_register_value(int *org_sp, int *org_fp, int *sp, int *fp,
		    int org_reg, int new_reg, uint32 *saved_out)
{
    if (org_reg >= l0 && org_reg <= i7) {
	/* org_reg is in or local register */
	int org_reg_offset = org_reg - l0;
	int org_reg_value = *(org_sp + org_reg_offset);
	
	if (new_reg >= l0 && new_reg <= i7) {
	    /* new_reg is in or local register */
	    int new_reg_offset = new_reg - l0;

	    *(sp + new_reg_offset) = org_reg_value;
	} else if (new_reg >= o0 && new_reg <= o7) {
	    /* new_reg is out register */

	    out_registers[new_reg - o0] = org_reg_value;
	} else {
	    /* new_reg should be in spill area */
	    int new_reg_offset = RegAlloc_get_spill_offset(new_reg) >> 2;
	    
	    assert(Reg_IsSpill(new_reg));

	    *(fp + new_reg_offset) = org_reg_value;
	}
    } else if (org_reg >= o0 && org_reg <= o7) {
	/* org_reg is out register */
	int org_reg_value = saved_out[org_reg - o0];

	if (new_reg >= l0 && new_reg <= i7) {
	    /* new_reg is in or local register */
	    int new_reg_offset = new_reg - l0;

	    *(sp + new_reg_offset) = org_reg_value;
	} else if (new_reg >= o0 && new_reg <= o7) {
	    /* new_reg is out register */

	    out_registers[new_reg - o0] = org_reg_value;
	} else {
	    /* new_reg should be in spill area */
	    int new_reg_offset = RegAlloc_get_spill_offset(new_reg) >> 2;
	    
	    assert(Reg_IsSpill(new_reg));

	    *(fp + new_reg_offset) = org_reg_value;
	}
    } else {
	/* org_reg should be in spill area */
	int org_reg_offset = RegAlloc_get_spill_offset(org_reg) >> 2;
	int org_reg_value = *(org_fp + org_reg_offset);
	
	assert(Reg_IsSpill(org_reg));

	if (new_reg >= l0 && new_reg <= i7) {
	    /* new_reg is in or local register */
	    int new_reg_offset = new_reg - l0;

	    *(sp + new_reg_offset) = org_reg_value;
	} else if (new_reg >= o0 && new_reg <= o7) {
	    /* new_reg is out register */

	    out_registers[new_reg - o0] = org_reg_value;
	} else {
	    /* new_reg should be in spill area */
	    int new_reg_offset = RegAlloc_get_spill_offset(new_reg) >> 2;
	    
	    assert(Reg_IsSpill(org_reg));

	    *(fp + new_reg_offset) = org_reg_value;
	}
    }
}


/* Name        : append_copy_code
   Description : append copy instructions before entry
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
static
int
append_copy_code(int *resolve_code, int code_pc, int org_reg, int new_reg)
{
    if (org_reg >= l0 && org_reg <= i7) {
	/* org_reg is in or local register */

	if (new_reg >= l0 && new_reg <= i7) {
	    /* new_reg is in or local register */

	    resolve_code[code_pc++] = ADD | (new_reg << 25) | (org_reg << 14);
	} else {
	    /* new_reg should be in spill area */
	    int new_reg_offset = RegAlloc_get_spill_offset(new_reg);
	    
	    assert(Reg_IsSpill(new_reg));

	    resolve_code[code_pc++] = ST | (org_reg << 25) | (fp << 14)
		| (1 << 13) | (new_reg_offset & 0x1fff);
	}
    } else {
	/* org_reg should be in spill area */
	int org_reg_offset = RegAlloc_get_spill_offset(org_reg);
	
	assert(Reg_IsSpill(org_reg));

	if (new_reg >= l0 && new_reg <= i7) {
	    /* new_reg is in or local register */
	    resolve_code[code_pc++] = LD | (new_reg << 25) | (fp << 14)
		| (1 << 13) | (org_reg_offset & 0x1fff);
	} else {
	    /* new_reg should be in spill area */
	    int new_reg_offset = RegAlloc_get_spill_offset(new_reg);
	    
	    assert(Reg_IsSpill(org_reg));

	    resolve_code[code_pc++] = LD | (g3 << 25) | (fp << 14)
		| (1 << 13) | (org_reg_offset & 0x1fff);
	    resolve_code[code_pc++] = ST | (g3 << 25) | (fp << 14)
		| (1 << 13) | (new_reg_offset & 0x1fff);
	}
    }

    return code_pc;
}

/* Name        : attach_direct_connect_codes
   Description : create direct connecting code
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
static
void
attach_direct_connect_codes(Hjava_lang_Throwable *eobject,
			    Hjava_lang_Class *catch_type,
			    void *translated_code, void *generated_site,
			    MethodInstance *mi, ExceptionInfo *info,
			    int *variable_map, int local_no, int stack_no)
{
    MethodInstance *ginstance;
    int *resolve_code;
    int code_pc;
    int code_size;
    int caller_local_no, caller_stack_no;
    int gcaller_local_no, gcaller_stack_no, glocal_no;
    int local_size;
    int *original_variable_map;
    int org_map_size;
    int org_reg, new_reg;
    int i;

    ginstance = EIT_GetMethodInstance(info);
    gcaller_local_no = MI_GetCallerLocalNO(ginstance);
    gcaller_stack_no = MI_GetCallerStackNO(ginstance);
    glocal_no = Method_GetLocalSize(MI_GetMethod(ginstance));

    caller_local_no = MI_GetCallerLocalNO(mi);
    caller_stack_no = MI_GetCallerStackNO(mi);
    local_size = Method_GetLocalSize(MI_GetMethod(mi));

    org_map_size = gcaller_local_no + gcaller_stack_no
	+ glocal_no + MAX_TEMP_VAR;
    original_variable_map = (int *) alloca(sizeof(int) * org_map_size);

    memcpy(original_variable_map, EIT_GetVariableMap(info),
	   org_map_size * sizeof(int));

    code_size = (caller_local_no + caller_stack_no + MAX_TEMP_VAR) * 2;
    resolve_code = (int *) gc_malloc_fixed(code_size * sizeof(void *));
    bzero(resolve_code, code_size);
    code_pc = 0;

    //
    // following codes are generated.
    //
    // sethi %hi(exception_class_dtable), g2
    // or    g2, %lo(exception_class_dtable), g2
    // ld    [o0], g3           <- get dtable
    // subcc g2, g3, g0
    // beq,a _matched_case
    // add   o0, g0, g2         <- delay slot
    // call throwExternalException
    // nop
    // _matched_case:
    //  ... resolving codes ...
    // call translated_code
    // nop
    //

    if (catch_type != NULL) {
	void *dtable = (void *)catch_type->dtable;

	resolve_code[code_pc++] = SETHI | (g2 << 25) | HI(dtable);
	resolve_code[code_pc++] = OR | (g2 << 25) | (g2 << 14) | (1 << 13)
	    | LO(dtable);
        resolve_code[code_pc++] = LD | (g3 << 25) | (o0 << 14) | (1 << 13) | 0;
        resolve_code[code_pc++] = SUBCC | (g0 << 25) | (g2 << 14) | g3;
        resolve_code[code_pc++] = BE | ANNUL_BIT | PREDICT_TAKEN | (4);
        resolve_code[code_pc++] = ADD| (g2 << 25) | (o0 << 14);
        resolve_code[code_pc] = CALL
	    | (((unsigned)(((int) throwExternalException)
			   - ((int) &resolve_code[code_pc]))) >> 2);
        code_pc++;
        resolve_code[code_pc++] = SPARC_NOP;
    } else {
	resolve_code[code_pc++] = ADD|(g2<<25)|(o0<<14);
    }

    /* match caller's local variables */
    for(i = 0; i < caller_local_no; i++) {
	org_reg = original_variable_map[i] & 0x0fffffff;
	new_reg = variable_map[i];

	if (new_reg != -1 && org_reg != 0x0fffffff && org_reg != new_reg)
	    code_pc = append_copy_code(resolve_code, code_pc,
				       org_reg, new_reg);
    }
    
    /* match caller's stack variables */
    for(i = 0; i < caller_local_no; i++) {
	int offset = gcaller_local_no
	    + Method_GetLocalSize(MI_GetMethod(ginstance));
	    
	org_reg = original_variable_map[i + offset] & 0x0fffffff;
	new_reg = variable_map[i + local_no];

	if (new_reg != -1 && org_reg != 0x0fffffff && org_reg != new_reg)
	    code_pc = append_copy_code(resolve_code, code_pc,
				       org_reg, new_reg);
    }

    /* match caller's temporary variables */
    if (MI_GetCaller(mi) != NULL) {
	for(i = 0; i < caller_local_no; i++) {
	    int offset = gcaller_local_no + gcaller_stack_no + glocal_no;
	    
	    org_reg = original_variable_map[i + offset] & 0x0fffffff;
	    new_reg = variable_map[i + local_no + stack_no];

	    if (new_reg != -1 && org_reg != 0x0fffffff && org_reg != new_reg)
		code_pc = append_copy_code(resolve_code, code_pc,
					   org_reg, new_reg);
	}
    }

    /* match local variables */
    for(i = 0; i < local_size; i++) {
	org_reg = original_variable_map[i + gcaller_local_no] & 0x0fffffff;
	new_reg = variable_map[i + caller_local_no];

	if (new_reg != -1 && org_reg != 0x0fffffff && org_reg != new_reg) {
	    /* local variable mapped register can be used by other
               variable because we split local variable before entry
               of exception handler */
	    int j;

	    for(j = i + 1; j < local_size; j++) {
		int tmp_var;
		int tmp_var_type;

		tmp_var = original_variable_map[j + gcaller_local_no];
		tmp_var_type = tmp_var & 0xf000000;
		tmp_var &= 0x0ffffff;

		if (tmp_var == new_reg) {
		    int k;
		    int variable_no;
		    int free_spill;

		    variable_no = local_no + stack_no + MAX_TEMP_VAR;
		    
		    for(k = 0, free_spill = Reg_number;
			k < org_map_size; k++) {
			int spill_var;

			spill_var = original_variable_map[k] & 0x0fffffff;
			if (spill_var != 0x0fffffff
			    && spill_var >= free_spill) {
			    free_spill = spill_var + 1;
			    if (RegAlloc_get_spill_offset(free_spill + 1) % 8)
				free_spill++;
			}
		    }

		    code_pc = append_copy_code(resolve_code, code_pc,
					       tmp_var, free_spill);
		    original_variable_map[j + gcaller_local_no] =
			tmp_var_type | free_spill;
		    
		}
	    }

	    code_pc = append_copy_code(resolve_code, code_pc,
				       org_reg, new_reg);
	}
    }

    resolve_code[code_pc] = CALL
	| (((unsigned) (((int) translated_code)
			- ((int) &resolve_code[code_pc]))) >> 2);
    code_pc++;
    resolve_code[code_pc++] = SPARC_NOP;

    assert(code_pc < code_size);

    *((int *) generated_site) = CALL
	| (((unsigned) (((int) resolve_code) - ((int) generated_site))) >> 2);

    FLUSH_DCACHE(resolve_code, &resolve_code[code_pc]);
    FLUSH_DCACHE(generated_site, generated_site + 4);
}

/* Name        : exit_monitor
   Description : unlock locked object
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
static
void
exit_monitor(MethodInstance *mi, ExceptionInfo *info, exceptionFrame *frame)
{
    Method *method;
    Hjava_lang_Object *object = NULL;
    
    method = MI_GetMethod(mi);

    if (Method_IsStatic(method)) {
	object = (Hjava_lang_Object *) Method_GetDefiningClass(method);

    } else {
	int *variable_map;
	int local0_offset;
	int local0;
	
	variable_map = EIT_GetVariableMap(info);
	local0_offset = 0;
	local0 = variable_map[local0_offset] & 0x0fffffff;

	if (local0 >= l0 && local0 <= i7) {
	    /* object is in local or in register */
	    object = (Hjava_lang_Object *) *(((int *)frame) + local0 - l0);
	} else if (local0 >= o0 && local0 <= o7) {
	    object = (Hjava_lang_Object *) out_registers[local0 - o0];
	} else {
	    /* object is in spill area */
	    int *fp;

	    assert(Reg_IsSpill(local0));

	    fp = (int *) ((exceptionFrame *) frame)->retbp;
	    object = (Hjava_lang_Object *)
		(fp + RegAlloc_get_spill_offset(local0));
	}
    }

    assert(object != NULL);
    
    if ((object->lock_info & OWNER_ID_MASK) == currentThreadID)
	_unlockMutex(object);
}

#endif /* TRANSLATOR */


/* Name        : EH_trap_handler
   Description : catch traps generated by JIT compiled code
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
EH_trap_handler(int sig, siginfo_t *siginfo, ucontext_t *ctx)
{
    Hjava_lang_Throwable *eobject;
    exceptionFrame stack_frame;
    uint32 trap_instr;
    int trap_number;

    trap_instr = *((uint32*) ctx->uc_mcontext.gregs[REG_PC]);
    catchSignal(SIGILL, EH_trap_handler);

    // local/in registers should have been spilled off to the stack.
    assert(ctx->uc_mcontext.gwins == NULL);

    stack_frame.retbp = ctx->uc_mcontext.gregs[REG_SP];
    stack_frame.retpc = ctx->uc_mcontext.gregs[REG_PC];

    //
    // save out registers
    //
    out_registers[0] = ctx->uc_mcontext.gregs[REG_O0];
    out_registers[1] = ctx->uc_mcontext.gregs[REG_O1];
    out_registers[2] = ctx->uc_mcontext.gregs[REG_O2];
    out_registers[3] = ctx->uc_mcontext.gregs[REG_O3];
    out_registers[4] = ctx->uc_mcontext.gregs[REG_O4];
    out_registers[5] = ctx->uc_mcontext.gregs[REG_O5];

    // extract trap number
    trap_number = trap_instr & 0xff;

    switch (trap_number) {
      case (0x10):
	eobject = (Hjava_lang_Throwable*) NullPointerException;
        break;

      case (0x11):
	eobject = (Hjava_lang_Throwable*) ArrayIndexOutOfBoundsException;
        break;

      case (0x12):
        eobject = (Hjava_lang_Throwable*) NegativeArraySizeException;
        break;

      case (0x13):
        eobject = (Hjava_lang_Throwable*) ArrayStoreException;
        break;
      case (0x14):
        eobject = (Hjava_lang_Throwable*) ClassCastException;
        break;

#ifdef PRECISE_MONITOREXIT
      case (0x15):
	eobject = (Hjava_lang_Throwable*) IllegalMonitorStateException;
        break;
#endif PRECISE_MONITOREXIT

      default:
#ifdef REG_WINDOW_DBG
        printf("Trap instr : 0x%x, PC : 0x%x\n", trap_instr, ctx->uc_mcontext.gregs[REG_PC]);
#endif /* REG_WINDOW_DBG */
        eobject = NULL;
        assert(0 && "STRANGE TRAP");
    }

    //
    // build stack trace
    //
    unhand(eobject)->backtrace = buildStackTrace(&stack_frame);


    //
    // call system exception manager
    //
    EH_exception_manager(eobject, &stack_frame);
}


/* Name        : EH_null_exception
   Description : catch SIGSEGV for null pointer exception
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
EH_null_exception(int sig, siginfo_t *siginfo, ucontext_t *ctx)
{
    Hjava_lang_Throwable *eobject;
    exceptionFrame stack_frame;
    uint32 trap_instr;
    
    trap_instr = *((uint32*) ctx->uc_mcontext.gregs[REG_PC]);

    catchSignal(SIGSEGV, EH_null_exception);

    // local/in registers should have been spilled off to the stack.
    assert(ctx->uc_mcontext.gwins == NULL);

    stack_frame.retbp = ctx->uc_mcontext.gregs[REG_SP];
    stack_frame.retpc = ctx->uc_mcontext.gregs[REG_PC];


#ifdef INLINE_CACHE
    // if null pointer exception is occured in dispatch method, 
    // retpc is set to return addres rather than current pc
    if (stack_frame.retpc == (int) (dispatch_method_with_inlinecache)
        || CN_is_from_type_check_code(stack_frame.retpc)) {
        stack_frame.retpc = ctx->uc_mcontext.gregs[REG_O7];
    }
#endif /* INLINE_CACHE */

    if (flag_exception) printf("null_exception is called\n");

    //
    // save out registers
    //
    out_registers[0] = ctx->uc_mcontext.gregs[REG_O0];
    out_registers[1] = ctx->uc_mcontext.gregs[REG_O1];
    out_registers[2] = ctx->uc_mcontext.gregs[REG_O2];
    out_registers[3] = ctx->uc_mcontext.gregs[REG_O3];
    out_registers[4] = ctx->uc_mcontext.gregs[REG_O4];
    out_registers[5] = ctx->uc_mcontext.gregs[REG_O5];

    eobject = (Hjava_lang_Throwable*) NullPointerException;

    unhand(eobject)->backtrace = buildStackTrace(&stack_frame);

    EH_exception_manager(eobject, &stack_frame);
}

/* Bug fix for precise conversion from floating point number to
   integer Conditions:

   1. NaN -> 0
   2. +Inf or larger value than max int -> max int
   3. -Inf or smaller value than min int -> min int

   Solutions:
   - Use TEM field in FSR(floating-point status register)
     to enable exceptions when operand of conversion instruction is
     contained in above three cases.

     -> before and after each conversion instruction, instructions
        which set and unset corresponding bit in FSR must be inserted.

   - If exception occurs, some field in FSR is set and SIGFPE raised
     and npc is set to pc(current instruction is next instruction of
     conversion instr.)

   - To inpect previous instruction, we can know the correct
     destination value.

   - Generating fixup code and return to normal flow. */

static int *new_data;
static long long _new_data;

/* restore floating point register values */
#define RESTORE_FLOAT_REGISTERS(float_register_values)\
    asm volatile(                                     \
          "ldd   [%0], %%f0                       \n                                      ldd   [%0+8], %%f2                     \n                                      ldd   [%0+16], %%f4                    \n                                      ldd   [%0+24], %%f6                    \n                                      ldd   [%0+32], %%f8                    \n                                      ldd   [%0+40], %%f10                   \n                                      ldd   [%0+48], %%f12                   \n                                      ldd   [%0+56], %%f14                   \n                                      ldd   [%0+64], %%f16                   \n                                      ldd   [%0+72], %%f18                   \n                                      ldd   [%0+80], %%f20                   \n                                      ldd   [%0+88], %%f22                   \n                                      ldd   [%0+96], %%f24                   \n                                      ldd   [%0+104], %%f26                  \n                                      ldd   [%0+112], %%f28                  \n                                      ldd   [%0+120], %%f30                  \n   " \
    : : "r" (float_register_values) :                 \
                 "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",            \
                 "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",      \
                 "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",    \
                 "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31")
#define WDBG(s) 

/* Name        : set_new_data_for_float
   Description : set floating point register appropriate value
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
static
void
set_new_data_for_float(unsigned *src_reg_val)
{
    float fval = *(float*)src_reg_val;

    if (src_reg_val[0] == 0x7fc00000){
        new_data[0] = 0;
        new_data[1] = 0;
	WDBG(fprintf(stderr, "NaN\n");)
    } else if (src_reg_val[0] == 0xff800000){
        new_data[0] = 0x80000000;
        new_data[1] = 0x0;
	WDBG(fprintf(stderr, "negative inf\n");)
    } else if (src_reg_val[0] == 0x7f800000){
        new_data[0] = 0x7fffffff;
        new_data[1] = 0xffffffff;
	WDBG(fprintf(stderr, "positive inf\n");)
    } else if (fval < 0){
        new_data[0] = 0x80000000;
        new_data[1] = 0x0;
	WDBG(fprintf(stderr, "negative large value\n");)
    } else if (fval > 0){
        new_data[0] = 0x7fffffff;
        new_data[1] = 0xffffffff;
	WDBG(fprintf(stderr, "positive large value\n");)
    } else {
        assert(0 && "Unexpected value");
    }
}       

/* Name        : set_new_data_for_double
   Description : set double register appropriate value
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
static
void
set_new_data_for_double(unsigned *src_reg_val)
{
    double dval;

    dval = *((double *) src_reg_val);
    
    if (src_reg_val[0] == 0x7ff80000){
        new_data[0] = 0;
        new_data[1] = 0;
	WDBG(fprintf(stderr, "NaN\n");)
    } else if (src_reg_val[0] == 0xfff00000){
        new_data[0] = 0x80000000;
        new_data[1] = 0x0;
	WDBG(fprintf(stderr, "negative inf\n");)
    } else if (src_reg_val[0] == 0x7ff00000){
        new_data[0] = 0x7fffffff;
        new_data[1] = 0xffffffff;
	WDBG(fprintf(stderr, "positive inf\n");)
    } else if (dval < 0){
        new_data[0] = 0x80000000;
        new_data[1] = 0x0;
	WDBG(fprintf(stderr, "negative large value\n");)
    } else if (dval > 0){
        new_data[0] = 0x7fffffff;
        new_data[1] = 0xffffffff;
	WDBG(fprintf(stderr, "positive large value\n");)
    } else {
        assert(0 && "Unexpected value");
    }
}
       
#undef WDBG


/* Name        : EH_arithmetic_exception
   Description : handle SIGFPE for arithmetic exception
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
                 SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
EH_arithmetic_exception(int sig, siginfo_t *siginfo, ucontext_t *ctx)
{
    Hjava_lang_Throwable *eobject;
    exceptionFrame stack_frame;
    uint32 trap_instr;
    uint32 *cur_pc;
    uint32 prev_instr;
    unsigned FSR;	/* floating point state register */

    trap_instr = *((uint32*) ctx->uc_mcontext.gregs[REG_PC]);
    cur_pc    = ((uint32*) ctx->uc_mcontext.gregs[REG_PC]);
    prev_instr = *(cur_pc - 1);

    // local/in registers should have been spilled off to the stack.
    assert(ctx->uc_mcontext.gwins == NULL);

    catchSignal(SIGFPE, EH_arithmetic_exception);

    FSR = (unsigned)ctx->uc_mcontext.fpregs.fpu_fsr;

    if (FSR & 0x10) {
	/* conversion instruction raised this exception, set
	   appropriate value to the float register and continue
	   execution */
        bool double_type = false;
        unsigned src_reg_val[2];
        unsigned dst_reg_num;

        new_data = (int*)&_new_data;

        src_reg_val[0] = (unsigned)
	    ctx->uc_mcontext.fpregs.fpu_fr.fpu_regs[prev_instr & 0x1f];
        src_reg_val[1] = (unsigned)
	    ctx->uc_mcontext.fpregs.fpu_fr.fpu_regs[(prev_instr & 0x1f) + 1];
 
        dst_reg_num = (unsigned)((prev_instr>>25) & 0x1f);

        if ((prev_instr & FSTOI) == FSTOI){
            double_type = false;
            set_new_data_for_float(src_reg_val);
        } else if ((prev_instr & FSTOX) == FSTOX){
            double_type = true;
            set_new_data_for_float(src_reg_val);
        } else if ((prev_instr & FDTOI) == FDTOI){
            double_type = false;
            set_new_data_for_double(src_reg_val);
        } else if ((prev_instr & FDTOX) == FDTOX){
            double_type = true;
            set_new_data_for_double(src_reg_val);
        } else {        
            assert(0 && "Unexpected case!");

        }            

        if (double_type == false){
            ctx->uc_mcontext.fpregs.fpu_fr.fpu_regs[dst_reg_num] = new_data[0];
        } else {
            ctx->uc_mcontext.fpregs.fpu_fr.fpu_regs[dst_reg_num] = new_data[0];
            ctx->uc_mcontext.fpregs.fpu_fr.fpu_regs[dst_reg_num + 1]
		= new_data[1];
        }
            
        //restore floating point register value
        RESTORE_FLOAT_REGISTERS(ctx->uc_mcontext.fpregs.fpu_fr.fpu_regs);
        //jump to normal flow
	asm volatile (
	    "ta      3			\n
             add     %1, %%g0, %%fp	\n
             jmp     %0			\n
             rett    %0+4"
	    : /* no outputs */
	    : "r" (cur_pc), "r" (ctx->uc_mcontext.gregs[REG_SP]));
    }

    if (flag_exception) printf("arithmetic_exception is called\n");

#ifdef TRANSLATOR
    // Two assumtions here for exception handling.
    // 1. only native method can have no save instruction
    // 2. native method do not catch exceptions
    // If these assumtions are broken, this code should be modified.
    if (findMethodFromPC(ctx->uc_mcontext.gregs[REG_PC]) == NULL) {
        stack_frame.retpc = ctx->uc_mcontext.gregs[REG_O7];
    } else {
        stack_frame.retpc = ctx->uc_mcontext.gregs[REG_PC];
    }
#else /* not TRANSLATOR */
    stack_frame.retpc = ctx->uc_mcontext.gregs[REG_PC];
#endif /* not TRANSLATOR */

    // make exception frame for stack backtracing
    stack_frame.retbp = ctx->uc_mcontext.gregs[REG_SP];

    //
    // save out registers
    //
    out_registers[0] = ctx->uc_mcontext.gregs[REG_O0];
    out_registers[1] = ctx->uc_mcontext.gregs[REG_O1];
    out_registers[2] = ctx->uc_mcontext.gregs[REG_O2];
    out_registers[3] = ctx->uc_mcontext.gregs[REG_O3];
    out_registers[4] = ctx->uc_mcontext.gregs[REG_O4];
    out_registers[5] = ctx->uc_mcontext.gregs[REG_O5];

    eobject = (Hjava_lang_Throwable*) ArithmeticException;
    unhand(eobject)->backtrace = buildStackTrace(&stack_frame);

    EH_exception_manager(eobject, &stack_frame);
}

/* floating point register save routine */
#define SAVE_FLOAT_REGISTERS(float_register_values)   \
asm volatile("                         \n                                          std   %%f0, [%0]                   \n                                          std   %%f2, [%0+8]                 \n                                          std   %%f4, [%0+16]                \n                                          std   %%f6, [%0+24]                \n                                          std   %%f8, [%0+32]                \n                                          std   %%f10, [%0+40]               \n                                          std   %%f12, [%0+48]               \n                                          std   %%f14, [%0+56]               \n                                          std   %%f16, [%0+64]               \n                                          std   %%f18, [%0+72]               \n                                          std   %%f20, [%0+80]               \n                                          std   %%f22, [%0+88]               \n                                          std   %%f24, [%0+96]               \n                                          std   %%f26, [%0+104]              \n                                          std   %%f28, [%0+112]              \n                                          std   %%f30, [%0+120]              \n" \
: : "r" (float_register_values) )


#ifdef TRANSLATOR
/* Name        : print_variable_map_info
   Description : debugging function
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: prints map informations
          The map size should be correct!! */
void
print_variable_map_info(int *map, int map_size)
{
    int i;

    for(i = 0; i < map_size; i++) {
	int map_value;

	map_value = map[i] & 0x0fffffff;

	if (map_value != 0x0fffffff) {
	    fprintf(stderr, "variable no %d : %s\n",
		    i, Reg_GetName(map_value));
	}
    }
}
#endif /* TRANSLATOR */
