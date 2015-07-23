/* TypeAnalysis.c
   
   Type analysis
   
   Written by: Suhyun Kim <zelo@i.am>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#include "config.h"
#include "TypedValue.h"
#include "CFG.h"
#include "MapVar2TValue.h"
#include "method_inlining.h"
#include "CallSiteInfo.h"
#include "InvokeType.h"
#include "code_sequences.h"
#include "ArgSet.h"

extern InlineGraph* CFGGen_current_ig_node;

#include "reg.h"
#include "translator_driver.h"


// The Global Variables !!!!!!!
int *TA_Num_Of_Defs;    // array[number of local variables]; 
extern CFG * cfg;       // Uhaha !
extern int        number_of_region_headers;
extern InstrNode *region_headers[];

static MapVar2TValue *TA_Local_Var_Analysis; 
static int num_of_local_vars;
static int num_of_vars;

static void TA_type_analysis_traversal(InstrNode* instr, MapVar2TValue* map); 

#undef INLINE
#define INLINE
#include "TypedValue.ic"
#include "MapVar2TValue.ic"

/* Name        : Instr_GetNumSemanticDstRegs
   Description : Get number of destination registers. For call instruction,
                 number of return values is returned.
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
   Notes:  */
static inline
int
Instr_GetNumSemanticDstRegs(InstrNode* instr)
{
    if (Instr_IsCall(instr)) {
        switch (Instr_GetNumOfRets(instr)) {
          case 0:
            return 0;
          case 1:
            return 1;
          case 2:
            if (Var_GetType(Instr_GetRet(instr, 0)) == T_DOUBLE)
              return 1;
            assert(Var_GetType(Instr_GetRet(instr, 0)) == T_LONG);
            return 2; 
          default:
            assert(0 && "What returns more than 2 ?");
        }
    } 

    return Instr_GetNumOfDestRegs(instr);
}

/* Name        : Instr_GetSemanticDstReg
   Description : Get destination register. For call instruction, 
                 return value is returned instead of 'o7' register
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
   Notes:  */
static inline
Reg
Instr_GetSemanticDstReg(InstrNode* instr, int index)
{
    assert(index < Instr_GetNumSemanticDstRegs(instr));
    if (Instr_IsCall(instr)) {
        assert(index < 2);
        return Instr_GetRet(instr, index);
    }
    return Instr_GetDestReg(instr, index);
}

void
TA_make_if_inlining_code(CFG *cfg, 
                         InstrNode *instr,
                         InlineGraph *graph, 
                         Method *called_method,
                         int dtable_offset,
                         Hjava_lang_Class* type,
                         ArgSet* argset);
void
TA_make_ld_ld_jmpl_code(CFG *cfg, InstrNode *instr, int dtable_offset);

void
TA_make_inline_cache_code(CFG* cfg, InstrNode* instr, int dtable_offset, void* target);


/* Name        : TA_make_ArgSet_from_MapVar2TValue
   Description : make ArgSet instance from map
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
   Notes:  */
static ArgSet*
TA_make_ArgSet_from_MapVar2TValue(MapVar2TValue* map, 
                                  int32 arg_start_offset, 
                                  int32 arg_size)
{
    int i;
    ArgSet* argset;

    // additional space for this pointer : specialization when this pointer
    // is not resolved...
    argset = AS_new(arg_size);
    for (i = 0; i < arg_size; i++){
        TypedValue* tv = MV2TV_GetByVarNum(map, arg_start_offset+i);
        if ((tv != NULL) 
            && (TV_GetTypeSource(tv) != TVTS_FROM_UNKNOWN_ARG))
          AS_SetArgType(argset, i, TV_GetType(tv));
    }

    return argset;
}


/* Name        : TA_make_init_map_from_ArgSet
   Description : make initial map from ArgSet
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition:
   Notes:  */
static MapVar2TValue*
TA_make_init_map_from_ArgSet(ArgSet* argset, 
                             int32 cur_offset, 
                             int32 num_of_vars)
{
    MapVar2TValue* init_map =
            MV2TV_new(num_of_vars); 
    int32 i;

    if (argset == NULL) return init_map;

    for(i = 0; i < AS_GetSize(argset); i++){
        TypedValue* tv;
        if (AS_GetArgType(argset,i) != NULL){
            tv = TV_new(AS_GetArgType(argset, i));
            TV_SetTypeSource(tv, TVTS_FROM_KNOWN_ARG);
            MV2TV_PutByVarNum(init_map, cur_offset+i, tv);
        }
    }

    return init_map;
}



/* Name        : TA_type_analysis_core
   Description : recursive core of TA_type_analysis
   Maintainer  : Suhyung Kim <zelo@i.am>
   Pre-condition: instr - The root instruction of the input region 

   Post-condition: the input region will have hopefully less dynamic call
   Notes: 
    What should we do if the destination is a hardware register ??
    Think about double & long !!

    PSEUDO CODE : for more details see CallOpt.tex.
     if (instr is the start of a virtual call sequence 
         && the type of the receiver is known) {
         transform the sequence to a static version; 
     }
     if (instr is a copy) {
         copy the typed value of src to dst; 
         map[dst] = map[src]; 
     }
     if(instr has a destination){
         map[dst] = TV_get_new_typed_value(instr); 
     }   
*/
static
void
TA_type_analysis_core(InstrNode* instr, MapVar2TValue* map)
{
    bool switch_to_nop = false;

    if (Instr_IsNop(instr)) {
        // just to quicken nop case. 
        return; 
    }

    // check ...
    if (Instr_IsStartOfInvokeSequence(instr)) {
        bool resolved = false;
        bool need_null_check = true;
        bool bypass_generation = false;
        InvokeType type = Instr_GetInvokeType(instr);
        Method* callee_method = Instr_GetCalledMethod(instr);
        Hjava_lang_Class* receiver_type = NULL;

        int meth_idx;
        Var receiver;
        TypedValue *tv;

        switch_to_nop = true;
        meth_idx = 0; //meaningless, just to avoid warning
        receiver = Instr_GetReceiverReferenceVariable(instr);
        if (receiver != INVALID_REG)
          tv = MV2TV_Get(map, receiver); 
        else
          tv = NULL;
        switch(type) {
          case InvokeVirtualType:
            meth_idx = Method_GetDTableIndex(callee_method) + DTABLE_METHODINDEX;
            if (tv != NULL) {
                Method* resolved_method;
                receiver_type = TV_GetType(tv); 

                if (Method_GetDTableIndex(callee_method) >= Class_GetNumOfMethodInDispatchTable(receiver_type)) {
                    bypass_generation = true;
                } else {
                    resolved_method = 
                        get_method_from_class_and_index(receiver_type,
                                                        meth_idx-DTABLE_METHODINDEX);
                    if (!equalUtf8Consts(callee_method->name, 
                                         resolved_method->name)
                        || !equalUtf8Consts(callee_method->signature, 
                                            resolved_method->signature)) {
                        //for given type this pass can't be executed...
                        bypass_generation = true;
                    } else {
                        resolved = true;
                        need_null_check = false;
                        
                        //reset callee_method
                        callee_method = resolved_method;
                    }
                }
            } else if (callee_method->accflags & (ACC_FINAL|ACC_PRIVATE)) {
                resolved = true;
            }
            break;

          case InvokeSpecialType:
            if (tv != NULL) {
                //receiver_type of invokespecial is always general type
                //to avoid the method is repeatedly translated...
                receiver_type = TV_GetType(tv); 
                need_null_check = false;
            }
            // callee_method is always statically resolved...
            resolved = true;
            break;

          case InvokeStaticType:
            need_null_check = false;
            resolved = true;
            break;

          case InvokeInterfaceType: 
            meth_idx = - Method_GetITableIndex(callee_method);
            if (tv != NULL) {
                Method* resolved_method;
                receiver_type = TV_GetType(tv); 

                if (Method_GetITableIndex(callee_method) > Class_GetNumOfMethodInInterfaceTable(receiver_type)) {
                    bypass_generation = true;
                } else {
                    resolved_method = 
                        get_method_from_class_and_index(receiver_type,
                                                        meth_idx);
                    if (!equalUtf8Consts(callee_method->name, 
                                         resolved_method->name)
                        || !equalUtf8Consts(callee_method->signature, 
                                            resolved_method->signature)) {
                        //for given type this pass can't be executed...
                        bypass_generation = true;
                    } else {
                        resolved = true;
                        need_null_check = false;

                        //reset callee_method
                        callee_method = resolved_method;
                    }
                }
            } else if (callee_method->accflags & (ACC_FINAL|ACC_PRIVATE)) {
                resolved = true;
            }
            break;
          default: assert (0 && "It can't occur");
        }

        // the following variables are set in previous step
        // - meth_idx : if invokeinterface, meth_idx < 0, otherwise >=0
        // - need_null_check : 
        //     if this pointer is not resolved for invokespecial and
        //     final or private method
        // - resolved : called method is resolved or not
        // - callee_method : if (resolved) callee_method is correct
        //                   otherwise only name & signature are the same
        // - receiver_type : if this pointer is resolved, it is set to 
        //                   correct value. otherwise its value is GeneralType
        if (!bypass_generation) {
            InstrNode* pred = Instr_GetPred(instr, 0);
            int pc = Instr_GetBytecodePC(instr);
            ArgSet* argset = NULL;

            assert(Instr_IsNop(pred));
            Instr_disconnect_instruction(pred, instr);
            if (Instr_GetFuncInfo(instr)->argInfo.numOfArgs == 0)
              argset = NULL;
            else 
              argset = TA_make_ArgSet_from_MapVar2TValue(map,
                 CFG_get_var_number(Instr_GetFuncInfo(instr)->argInfo.argVars[0]),
                 Instr_GetFuncInfo(instr)->argInfo.numOfArgs);
            pred =
                CSeq_make_corresponding_invoke_sequence(cfg, pred, pc,
                                                        callee_method,
                                                        Instr_GetOperandStackTop(instr),
                                                        receiver_type,
                                                        argset,
                                                        Instr_GetFuncInfo(instr),
                                                        meth_idx,
                                                        need_null_check,
                                                        resolved);
            Instr_connect_instruction(pred, instr);
        }
    }

    switch (Instr_GetNumSemanticDstRegs(instr)) {
      case 0:
        // nothing to do
        break; 

      case 1: 
      {
        int dst = Instr_GetSemanticDstReg(instr, 0);

        if (Var_IsVariable(dst)) {
            int src; 
            if ((Instr_IsICopy(instr) 
                 && ((src = Instr_GetSrcReg(instr, 0)) || 1)) 
                ||(Instr_IsFCopy(instr) 
                   && ((src = Instr_GetSrcReg(instr, 1)) || 1))) {
                if (Var_IsVariable(src)) {
                    // instr is a copy.
                    MV2TV_Put(map, dst, MV2TV_Get(map,src)); 
                }
            } else {
                MV2TV_Put(map, dst, TV_get_new_typed_value(instr,0)); 
            }
        } else {
            // what should we do if the destination is a hardware register !?
        }

        break; 
      }

      case 2:
        assert(Instr_IsCall(instr)); 
        assert(Var_GetType(Instr_GetSemanticDstReg(instr, 0)) == T_LONG);
        // There is no known case that return a valid TypedValue. 
        MV2TV_Put(map, Instr_GetSemanticDstReg(instr,0), NULL); 
        MV2TV_Put(map, Instr_GetSemanticDstReg(instr,1), NULL); 
        break; 

      default:
        assert(0); 
        break; 
    }

    if (switch_to_nop)
      Instr_SwitchToNop(instr);
}


static void
TA_merge_at_join(InstrNode* join_instr, MapVar2TValue* new_map)
{
    int i; 
    MapVar2TValue *old_map = Instr_GetMV2TV(join_instr); 

    assert(old_map->size == new_map->size); 

#ifndef NDEBUG
    for (i = 0; i < MV2TV_GetSize(TA_Local_Var_Analysis); ++i) {
        TypedValue *tv = MV2TV_GetByVarNum(TA_Local_Var_Analysis,i); 
        assert(tv == NULL 
               || (TV_GetType(tv) == TV_GetType(MV2TV_GetByVarNum(old_map,i))
                   && TV_GetType(tv) == TV_GetType(MV2TV_GetByVarNum(new_map,i)))); 
    }
#endif

    for (i = 0; i < old_map->size; ++i) {
        TypedValue *old_tv = MV2TV_GetByVarNum(old_map,i); 
        if (old_tv != NULL) {
            TypedValue *new_tv = MV2TV_GetByVarNum(new_map,i);
            if(new_tv == NULL 
               || TV_GetType(old_tv) != TV_GetType(new_tv)) {
                MV2TV_PutByVarNum(old_map, i, NULL); 
            } 
        }
    }
}



/* Name        : TA_traversal_core
   Description : recursively traverse the region and call TA_type_analysis_core
   Maintainer  : Suhyun Kim <zelo@i.am>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
static inline
void
TA_traversal_core(InstrNode* instr, MapVar2TValue* map)
{
    int i; 

    // traversal stuffs...
    for(i = 1; i < Instr_GetNumOfSuccs(instr); i++) {
        InstrNode * succ = Instr_GetSucc(instr, i);
        if (Instr_IsRegionEnd(succ)) {
            if (Instr_GetMV2TV(succ)) {
                TA_merge_at_join(succ, map); 
            } else {
                Instr_SetMV2TV(succ, MV2TV_clone(map)); 
            }
            continue; 
        } else {
            TA_type_analysis_traversal(succ, MV2TV_clone(map)); 
        }
    }
    if (Instr_GetNumOfSuccs(instr) >= 1) {
        InstrNode * succ = Instr_GetSucc(instr, 0);
        if (Instr_IsRegionEnd(succ)) {
            if (Instr_GetMV2TV(succ)) {
                TA_merge_at_join(succ, map); 
            } else {
                Instr_SetMV2TV(succ, map); 
            }
        } else {
            TA_type_analysis_traversal(succ, map); 
        }
    }
}


/* Name        : TA_type_analysis_traversal
   Description : recursively traverse the region and call TA_type_analysis_core
   Maintainer  : Suhyun Kim <zelo@i.am>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
static void
TA_type_analysis_traversal(InstrNode* instr, MapVar2TValue* map)
{
    Instr_MarkVisited(instr); 

    TA_type_analysis_core(instr, map); 

    TA_traversal_core(instr, map); 
}


static inline
void
TA_local_var_analysis(MapVar2TValue* map)
{
    extern int     Var_offset_of_local_var;

    int a_local; 

    TA_Local_Var_Analysis = MV2TV_new(num_of_vars); 

    for (a_local = 0; 
         a_local < Method_GetLocalSize(IG_GetMethod(CFGGen_current_ig_node)); 
         a_local++) {

         assert(TA_Num_Of_Defs[a_local] != 0 
                || MV2TV_GetByVarNum(map,a_local+Var_offset_of_local_var) == NULL);
	 
       if (TA_Num_Of_Defs[a_local] == 1 
           && MV2TV_GetByVarNum(map, a_local+Var_offset_of_local_var) != NULL){
           MV2TV_PutByVarNum(TA_Local_Var_Analysis, 
                             a_local+Var_offset_of_local_var, 
                             MV2TV_GetByVarNum(map,a_local+Var_offset_of_local_var)); 
       }
    }
}

/* Name        : TA_init_traversal
   Description : a TA_type_analysis_traversal specialized for the first region
   Maintainer  : Suhyun Kim <zelo@i.am>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
static void
TA_init_traversal(InstrNode* instr, MapVar2TValue* map)
{
    Instr_MarkVisited(instr); 

    TA_type_analysis_core(instr, map); 

    if (Instr_GetNumOfSuccs(instr) > 1) {
        // The end of the first basic block !!
        TA_local_var_analysis(map); 
        TA_traversal_core(instr, map); 
        return; 
    }

    // traversal stuffs...
    if (Instr_GetNumOfSuccs(instr) == 1) {
        InstrNode * succ = Instr_GetSucc(instr, 0);
        if (Instr_IsRegionEnd(succ)) { 
            // The (other) end of the first basic block !!
            TA_local_var_analysis(map); 
            if (Instr_GetMV2TV(succ)) {
                assert(0 && "I don't think this is possible."); 
                TA_merge_at_join(succ, map); 
            } else {
                Instr_SetMV2TV(succ, map); 
            }
        } else {
            TA_init_traversal(succ, map); 
        }
    }
}



/* Name        : TA_prepare_type_analysis
   Description : set variable offset & make region headers
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   Post-condition: num_of_local_vars & num_of_vars are set.
   Notes:  */
static void
TA_prepare_type_analysis(CFG* cfg, InlineGraph* graph)
{
    extern void RegAlloc_detect_region_headers(InstrNode *instr);

    assert(IG_GetHead(graph) != NULL);
    
    CFG_set_variable_offsets(cfg, NULL);

    number_of_region_headers = 0;
    RegAlloc_detect_region_headers(IG_GetHead(graph)); 
    region_headers[number_of_region_headers++] = IG_GetHead(graph);

    CFG_PrepareTraversalFromStart(cfg, IG_GetStartInstrNum(graph));

    // static variables
    num_of_vars = CFG_GetNumOfVars(cfg);
    num_of_local_vars = CFG_GetNumOfLocalVars(cfg);
}

static MapVar2TValue*
TA_make_init_map(InlineGraph* graph)
{
    int start_offset;

    if (IG_GetDepth(graph) == 0)
      start_offset = 0; // offset_of_local_var is 0
    else 
      start_offset = num_of_local_vars + IG_GetStackTop(graph);

    return TA_make_init_map_from_ArgSet(IG_GetArgSet(graph), 
                                        start_offset, 
                                        num_of_vars);
}


//-----------------------------------------------------------------
// Name : TA_type_analysis
// Responsible : Suhyun Kim <zelo@i.am>
// Description : transform as many dynamic call as possible to static call. 
// Pre-Condition :
//  * instr : The root instruction of the input region 
// Post-Condition :
//  * the input region will have hopefully less dynamic call. 
// NOTE :
//  * special things you want to say. 
//-----------------------------------------------------------------
// void
// TA_type_analysis(InstrNode* instr)
// {
//     // prepare an empty map 
//     MapVar2TValue *map_at_root = MV2TV_new(CFG_get_number_of_variables(cfg));

//     TA_type_analysis_traversal(instr, map_at_root); 
// }



/* Name        : TA_type_analysis
   Description : transform as many dynamic call as possible to static call or
                 inlined version. 
   Maintainer  : Suhyun Kim <zelo@i.am>
   Pre-condition:
      graph : of which argset must be set.
   Post-condition:
      the input graph will have hopefully less dynamic call. 
   Notes:  */
void
TA_type_analysis(InlineGraph* graph)
{
    int i; 
    MapVar2TValue *init_map;
    InstrNode* root_instr = IG_GetHead(graph);

    // set static[global] variables & make region headers
    TA_prepare_type_analysis(cfg, graph);

    // traverse for the first region
    init_map = TA_make_init_map(graph);
    Instr_SetMV2TV(root_instr, init_map); 
    TA_init_traversal(root_instr, MV2TV_clone(Instr_GetMV2TV(root_instr))); 

    // traverse for the other regions
    for (i = number_of_region_headers - 2; i >= 0 ; i--) {
        int j; 
        InstrNode * instr = region_headers[i];

        for (j = 0; j < Instr_GetNumOfPreds(instr); j++) {
            if (!Instr_IsVisited(Instr_GetPred(instr, j))) {
                // FIXME: cloning seems to be unnecessary...
                Instr_SetMV2TV(instr, MV2TV_clone(TA_Local_Var_Analysis)); 
                break;
            }
        }

        // transform as many dynamic call as possible to static call. 
        TA_type_analysis_traversal(instr, MV2TV_clone(Instr_GetMV2TV(instr)));
    }
}
