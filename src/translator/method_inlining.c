/* method_inlining.c
   
   functions for method inlining
   
   Written by: SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */
#include <stdio.h>
#include "config.h"
#include "flags.h"
#include "method_inlining.h"
#include "classMethod.h"
#include "ArgSet.h"
#include "InstrNode.h"
#include "CFG.h"

/* inline functions */
#undef INLINE
#define INLINE 
#include "method_inlining.ic"

int The_Method_Inlining_Max_Size = 50;
int The_Method_Inlining_Max_Depth = 5;

/* static functions declarations */
static void _print_inline_info (FILE *fp , InlineGraph *root );
static void _print_inline_info_as_xvcg (FILE *fp , InlineGraph *root );

/* Name        : IG_IsMethodInlinable
   Description : determine whether inline this method or not
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: following are conditions we use for determination
       1. the bytecode length of it is too large
       2. inlining call depth
       3. the method is native
       4. the class of it is not processed to OK state
       5. exception limitation (turn on off by compilation flag)  */
bool
IG_IsMethodInlinable(InlineGraph *this, Method *callee)
{
    int code_len;
    int call_depth;
    Hjava_lang_Class *callee_class;
    Method *caller;
    
    assert(this);
    assert(callee);

    caller = IG_GetMethod(this);
    code_len = Method_GetByteCodeLen(callee);
    callee_class = Method_GetDefiningClass(callee);
    call_depth = IG_GetDepth(this);
    
    return (flag_inlining
	    && (Method_GetByteCodeLen(callee) < The_Method_Inlining_Max_Size)
	    && !Method_IsNative(callee)
	    && (Class_GetState(callee_class) == CSTATE_OK)
	    && (call_depth < The_Method_Inlining_Max_Depth)
#ifndef HANDLER_INLINE
	    && (Method_GetExceptionTable(callee) == NULL)
	    && (Method_GetExceptionTable(caller) == NULL)
	    && !Method_IsSynchronized(callee)
	    && !Method_IsSynchronized(caller)
#endif /* HANDLER_INLINE */
	);
}

/* Name        : IG_RegisterMethod
   Description : register method for inlining
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
IG_RegisterMethod(InlineGraph *this, Method *called_method, int bpc,
		  ArgSet *arg_set, InstrNode *instr, int stack_top)
{
    InlineGraph *callee;

    callee = IG_init(called_method);

    IG_SetBPC(callee, bpc);
    IG_SetCallerCalleeRelation(this, callee);
    IG_SetStackTop(callee, stack_top);
    IG_SetInstr(callee, instr);
    IG_SetArgSet(callee, arg_set);
}

/* Name        : IG_print_inline_info*
   Description : prints out method inlining information
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: IG_print_inline_info outputs normal texts, while
          IG_print_inline_info_as_xvcg outputs vcg file format  */
extern int num;

static
void
_print_inline_info(FILE *fp, InlineGraph *root) {
    InlineGraph *graph;
    Method *method;
    int bpc;
    int i;

    method = IG_GetMethod(root);
    bpc = IG_GetBPC(root);
    
    for(i = 0; i < IG_GetDepth(root); i++) {
        fprintf(fp, "->");
    }

    fprintf(fp, "depth %d (%d) : %s %s %s\n",
             IG_GetDepth(root),
             IG_GetBPC(root),
             method->class->name->data,
             method->name->data,
             method->signature->data);

    if (IG_GetCallee(root) != NULL) {
        for(graph = IG_GetCallee(root); 
             graph != NULL;
             graph = IG_GetNextCallee(graph)) {
            _print_inline_info(fp, graph);
        }
    }
}

void
IG_print_inline_info(CFG *cfg) {
    InlineGraph *root;
    char filename[256];
    FILE *fp;

    root = CFG_GetIGRoot(cfg);

    if (IG_GetCallee(root) == NULL) return;

    sprintf(filename, "inline_info_%d", num);
    fp = fopen(filename, "w");

    _print_inline_info(fp, root);

    fclose(fp);
}

static
void
_print_inline_info_as_xvcg(FILE *fp, InlineGraph *root) {
    InlineGraph *graph;
    Method *method;
    int bpc;

    method = IG_GetMethod(root);
    bpc = IG_GetBPC(root);
    fprintf(fp, "   node: { title: \"%p\" label: \"", root);

    fprintf(fp, "%s:%s\n%s\n",
             method->class->name->data,
             method->name->data,
             method->signature->data);

    fprintf(fp, "\" }\n");

    if (IG_GetCallee(root) != NULL) {
        for(graph = IG_GetCallee(root); 
             graph != NULL;
             graph = IG_GetNextCallee(graph)) {
            fprintf(fp, "   edge: { thickness: 3 "
                     " sourcename: \"%p\" targetname: \"%p\""
                     " label: \"%d\" } \n",
                     root, graph, IG_GetBPC(graph));
            _print_inline_info_as_xvcg(fp, graph);
        }
    }
}

void
IG_print_inline_info_as_xvcg(CFG *cfg) {
    InlineGraph *root;
    char filename[256];
    FILE *fp;
    Method *method;
    int i;

    root = CFG_GetIGRoot(cfg);
    method = IG_GetMethod(root);
    
    if (IG_GetCallee(root) == NULL) return;

    sprintf(filename, "inline_info_vcg_%d", num);
    fp = fopen(filename, "w");

    fprintf(fp, "graph: {\n");
    fprintf(fp, "   layoutalgorithm: minbackward\n");
    fprintf(fp, "   display_edge_labels: yes\n");
    fprintf(fp, "   finetuning: yes\n");

    fprintf(fp, "   node: { title: \"mf\" label: \"Inline Graph of %s:%s\n",
             method->class->name->data,
             method->name->data);
    fprintf(fp, "%s (%p)", method->signature->data, method);
    fprintf(fp, "(");

    for(i = 0; i < CFG_GetNumOfParameters(cfg);i++) {
        fprintf(fp, " %s", Var_get_var_reg_name(CFG_GetParameter(cfg, i)));
    };
    
    fprintf(fp, ")\n");

    fprintf(fp, "Total local variables : %d\n", 
             CFG_GetNumOfLocalVars(cfg));
    fprintf(fp, "Total stack variables : %d\n",
             CFG_GetNumOfStackVars(cfg));

    fprintf(fp, "\"}\n");
    fprintf(fp, "   edge: { thickness : 3 "
             " sourcename: \"mf\" targetname: \"%p\" }\n", root);

    _print_inline_info_as_xvcg(fp, root);

    fprintf(fp, "}\n");

    fclose(fp);
}
