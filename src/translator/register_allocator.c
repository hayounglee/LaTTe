/* register_allocator.c
   
   The register allocation is implemented in this file.
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <macros.h>

#include "config.h"

#include "reg.h"

#include "CFG.h"
#include "InstrNode.h"

#include "utils.h"
#include "flags.h"
#include "bit_vector.h"
#include "reg_alloc_util.h"
#include "live_analysis.h"
#include "AllocStat.h"

#include "fast_mem_allocator.h"

#include "exception_info.h"
#include "method_inlining.h"

#include "translate.h"

#ifdef TYPE_ANALYSIS
#include "TypeAnalysis.h"
#endif


extern int num, exception_num;

#define DBG(s)    
#define EDBG(s)   
#define ZDBG(s)
#define HDBG(s)   

CFG*        cfg;
AllocStat*  RegAlloc_Exception_Returning_Map;
int         RegAlloc_Exception_Caller_Local_NO;
int         RegAlloc_Exception_Caller_Stack_NO;

static Var exception_return_var[2];
static VarType exception_return_type;


/* BEGIN utility functions */


#define MAX_REGION_HEADER 2048

#define MAX_LIVE_VARIABLES 2048
#define MAX_REGION_DUMMYS  (MAX_REGION_HEADER)

#define MAX_REGION_INSTRS  2400
int        number_of_region_headers;
InstrNode* region_headers[MAX_REGION_HEADER];
int        region_instrs[MAX_REGION_HEADER];
char       dirty[2048];


FILE* outout = stdout;

#ifdef JP_EXAMINE_SPILL
int num_spill;
#endif

extern int     offset_of_local_var;

bool           no_record_need;

static int*    map_pool;

static int     map_pool_index;
static int     map_pool_size;

static
void
prepare_to_record_local_var_maps(CFG* cfg)
{
    map_pool_size = CFG_GetNumOfMapNeedInstrs(cfg) * CFG_GetNumOfVars(cfg);

    if (!flag_no_loopopt && flag_looppeeling) {
        /* Loop peeling creates more exceptionable instructions. 
           So I just increase the number of such instructions.
           How do you think this is harmful to the performance? */
        /* The number of map need instructions is increased.
           So, the size of map pool also need to be increased.
           But, I doubt if this is correct.             -hacker */
        map_pool_size *= 2;
    }

    if (map_pool_size > 0) {
        map_pool = (int *) FMA_calloc(map_pool_size * sizeof(int));
    }

    map_pool_index = 0;
}

static
void
record_caller_variable_map(InlineGraph *graph, AllocStat *h)
{
    InstrNode *instr;
    MethodInstance *mi;
    int local_offset;
    int stack_offset;
    int temp_offset;
    int caller_local_no;
    int caller_stack_no;
    int temporary_no;
    
    int *var_map;
    int var_idx;

    Var var;
    VarType var_type;
    
    int i;
    
    local_offset = 0;
    stack_offset = CFG_GetNumOfLocalVars(cfg);
    temp_offset = CFG_GetNumOfLocalVars(cfg) + CFG_GetNumOfStackVars(cfg);
    
    instr = IG_GetTail(graph);
    
    switch (Instr_NeedMapInfo(instr)) {
      case NEED_MAP_INFO:
      case NEED_RET_ADDR_INFO:
	caller_local_no = IG_GetLocalOffset(graph);
	caller_stack_no = IG_GetStackOffset(graph);
	temporary_no = MAX_TEMP_VAR;
	break;
	
      default:
	assert(false && "control should not be here!\n");
    }
    
    // record local variables
    var_map = map_pool + map_pool_index;
    var_idx = 0;
    
    for (i = local_offset; i < local_offset + caller_local_no; i++) {
	int vi = var_idx + i - local_offset;
	
        var_map[vi] = h->map[i];

        if (var_map[vi] != -1) {
            var = AllocStat_FindRMap(h, var_map[vi], 0);
            var_type = Var_GetType(var);

            assert(var_type >= 0x0 && var_type <= 0xf);
            var_map[vi] |= var_type << 28;
        }
    }

    var_idx += caller_local_no;

    for (i = stack_offset; i < stack_offset + caller_stack_no; i++) {
	int vi = var_idx + i - stack_offset;
	
        var_map[vi] = h->map[i];

        if (var_map[vi] != -1) {
            var = AllocStat_FindRMap(h, var_map[vi], 0);
            var_type = Var_GetType(var);

            assert(var_type >= 0x0 && var_type <= 0xf);
            var_map[vi] |= var_type << 28;
        }
    }

    var_idx += caller_stack_no;
    
    for (i = temp_offset; i < temp_offset + temporary_no; i++) {
	int vi = var_idx + i - temp_offset;
	
        var_map[vi] = h->map[i];

        if (var_map[vi] != -1) {
            var = AllocStat_FindRMap(h, var_map[vi], 0);
            var_type = Var_GetType(var);

            assert(var_type >= 0x0 && var_type <= 0xf);
            var_map[vi] |= var_type << 28;
        }
    }

    var_idx += temporary_no;

    mi = IG_GetMI(graph);
    MI_SetCallerVarMap(mi, var_map);
    map_pool_index += var_idx;

    assert(map_pool_index <= map_pool_size);
}

static
void
record_variable_map(InstrNode* instr, AllocStat* h)
{
    int *var_map;
    int var_idx;
    
    InlineGraph *graph;

    int local_offset;
    int stack_offset;
    int temp_offset;

    int caller_local_no;
    int caller_stack_no;

    int local_no;

    int temporary_no;
    
    Var     var;
    VarType var_type;

    MapInfo map_info;

    int i;
    
    if (Instr_IsMethodEnd(instr)) {
        record_caller_variable_map(Instr_GetInlineGraph(instr), h);
        return;
    }

    local_offset = 0;
    stack_offset = CFG_GetNumOfLocalVars(cfg);
    temp_offset = CFG_GetNumOfLocalVars(cfg) + CFG_GetNumOfStackVars(cfg);

    map_info = Instr_NeedMapInfo(instr);

    graph = Instr_GetInlineGraph(instr);

    switch (map_info) {
      case NEED_MAP_INFO:
      case NEED_RET_ADDR_INFO:
	caller_local_no = IG_GetLocalOffset(graph);
	caller_stack_no = IG_GetStackOffset(graph);
	local_no = Method_GetLocalSize(IG_GetMethod(graph));
	temporary_no = MAX_TEMP_VAR;
	break;

      case NEED_LOCAL0_MAP_INFO:
	caller_local_no = 0;
	caller_stack_no = 0;
	local_no = 1;
	temporary_no = 0;
	break;

/*        case NEED_RET_ADDR_INFO: */
/*  	caller_local_no = 0; */
/*  	caller_stack_no = 0; */
/*  	local_no = 0; */
/*  	temporary_no = 0; */
/*  	break; */

      default:
	assert(false && "control should not be here!\n");
    }
    
    var_map = map_pool + map_pool_index;
    var_idx = 0;
    
    // save local variables
    for (i = local_offset; i < local_offset + caller_local_no + local_no; i++) {
	int vi = var_idx + i - local_offset;
	
        var_map[vi] = h->map[i];

        if (var_map[vi] != -1) {
            var = AllocStat_FindRMap(h, var_map[vi], 0);
            var_type = Var_GetType(var);

            assert(var_type >= 0x0 && var_type <= 0xf);
            var_map[vi] |= var_type << 28;
        }
    }    

    var_idx += caller_local_no + local_no;
    
    // save stack variables
    for (i = stack_offset; i < stack_offset + caller_stack_no; i++) {
	int vi = var_idx + i - stack_offset;

        var_map[vi] = h->map[i];

        if (var_map[vi] != -1) {
            var = AllocStat_FindRMap(h, var_map[vi], 0);
            var_type = Var_GetType(var);

            assert(var_type >= 0x0 && var_type <= 0xf);
            var_map[vi] |= var_type << 28;
        }
    }    

    var_idx += caller_stack_no;
    
    // save temporary variables
    for (i = temp_offset; i < temp_offset + temporary_no; i++) {
	int vi = var_idx + i - temp_offset;
	
        var_map[vi] = h->map[i];

        if (var_map[vi] != -1) {
            var = AllocStat_FindRMap(h, var_map[vi], 0);
            var_type = Var_GetType(var);

            assert(var_type >= 0x0 && var_type <= 0xf);
            var_map[vi] |= var_type << 28;
        }
    }

    var_idx += temporary_no;
    
    instr->varMap = var_map;
    map_pool_index += var_idx;

    assert(map_pool_index <= map_pool_size);
}

/* END utility functions */



/* BEGIN RegAlloc_backward_sweep related */

typedef struct {
    InstrNode * exit;
    int succ; 
    bool is_map;
} boundary;

#define  MAX_REGION_BOUNDARIES    10000

static int number_of_region_boundary;
static boundary region_boundary[MAX_REGION_BOUNDARIES];

static
void
RegAlloc_add_region_boundary(InstrNode *instr, int s, bool mapped)
{
    assert(number_of_region_boundary < MAX_REGION_BOUNDARIES);
    region_boundary[number_of_region_boundary].exit = instr;
    region_boundary[number_of_region_boundary].succ = s;
    region_boundary[number_of_region_boundary].is_map = mapped;
    number_of_region_boundary++;
}

static map * RegAlloc_backward_sweep_initial_tbl = NULL;

static
void
RegAlloc_update_preferred_assignment(map * tbl, InstrNode * inst)
{
    int dest = Instr_GetDestReg(inst, 0);

    if (Var_IsVariable(dest)) {
        int preferred_reg = map_Find(tbl, dest);
        int src_reg;
        
        /* valid preferred register is selected above. */
        if (Reg_IsHW(preferred_reg)) {
            /* valid mapping h[dest] == preferred_reg is found */
            Instr_SetPreferredDest(inst, preferred_reg);
        
            //if (! flag_no_copy_coalescing) 
            {
                if ((Instr_IsICopy(inst) 
                     && Var_IsVariable(src_reg=Instr_GetSrcReg(inst, 0)))
                    || 
                    (Instr_IsFCopy(inst) 
                     && Var_IsVariable(src_reg=Instr_GetSrcReg(inst, 1)))) {
                    map_Insert(tbl, src_reg, preferred_reg);
                }  
            }

            map_Erase(tbl, dest);
        }
    }
    
    if (Instr_IsCall(inst)) {
        int i;

        /* call has a semantic like "z = f(a,b,..)".
	   So z has to be removed from the map entry if call has
	   return values. This was the cause of the error in 
	   TestGeoMean2Run.class. 1998. 2. 25. by jp  */
        for (i = 0; i < Instr_GetNumOfRets(inst); i++) {
            map_Erase(tbl, Instr_GetRet(inst, i));
        }

        //
        // inserted by doner - 98/7/14
        //
        // def rst1
        // def rst2
        // call func1 (rst2)->()
        // call func2 (rst1)->()
        //
        // The current implementation of RegAlloc_backward_sweep makes the
        // preferred assignment of 'def rst1' %o0 which generates a
        // copy before the first call instruction.
        // Therefore, from %o0 to %o0+arg_num-1 should be free
        // before assignment of an argument variable.
        // This can be made more efficient later ^^;.
        //
        for (i = 0; i < CFG_GetNumOfVars(cfg); i++) {
            if (tbl->map[i] >= o0 && tbl->map[i] < o6) {
                tbl->map[i] = -1;
            }
        }

        for (i = 0; i < Instr_GetNumOfArgs(inst); i++) {
            int type = Var_GetType(Instr_GetArg(inst, i));

            assert(Var_IsVariable(Instr_GetArg(inst, i)));

            if (type == T_INT || type == T_REF || type == T_LONG ||
                 type == T_IVOID) {
                map_Insert(tbl, Instr_GetArg(inst, i),
                            Reg_get_parameter(i));
            }
        }
    }

    if (Instr_IsReturn(inst)) {
        int ret_var;
        int ret_num = Instr_GetNumOfRets(inst);

        //
        // modified by doner
        // - FVOID is not inserted in map.
        //
        if (ret_num > 0) {
            ret_var = Instr_GetRet(inst, 0);
            assert(Var_IsVariable(ret_var));
            map_Insert(tbl, ret_var, Reg_get_returning(Var_GetType(ret_var)));

            if (ret_num > 1 && Var_GetType(ret_var) != T_DOUBLE) {
                ret_var = Instr_GetRet(inst, 1);
                assert(Var_IsVariable(ret_var));
                map_Insert(tbl, ret_var, Reg_get_returning(Var_GetType(ret_var)));
            }
        }
    }
}



/* 
   Name        : RegAlloc_backward_sweep
   Description : In order to reduce the copies, the calling convention
                 and the result of the register allocation from other regions
                 are analyzed by "RegAlloc_backward_sweep".
   Author      : Seongbae Park <ramdrive@altair.snu.ac.kr>
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: 
 */
static
map * 
RegAlloc_backward_sweep(InstrNode * prev, InstrNode * inst)
{
    map * tbl = NULL;
    int i;

    if (Instr_GetNumOfSuccs(inst) > 0) {
        // If you want to apply profile information, you should rearrange
        // the order of itable_merge()
        InstrNode * succ = Instr_GetSucc(inst, 0);
        
        if (Instr_IsRegionEnd(succ)) {
            AllocStat* h = Instr_GetH(succ); 
            if (h) {
                tbl = map_Clone_from_alloc_status(h);
                if (tbl == NULL) {
                    tbl = RegAlloc_backward_sweep_initial_tbl;
                }
                RegAlloc_add_region_boundary(inst, 0, true);
            } else {
                RegAlloc_add_region_boundary(inst, 0, false);
            }
        } else {
            /* No need to merge here */
            tbl = RegAlloc_backward_sweep(inst, succ);
        }
        
        for (i = 1; i < Instr_GetNumOfSuccs(inst); i++) {
            succ = Instr_GetSucc(inst, i);
            if (Instr_IsRegionEnd(succ)) {
                AllocStat* h = Instr_GetH(succ); 
                if (h) {
                    map* tbl1 = map_Clone_from_alloc_status(h);
                    tbl = map_merge(tbl, tbl1);
#ifdef JP_UNFAVORED_DEST
                    init_live_var_info(tbl->map);
#endif
                    RegAlloc_add_region_boundary(inst, i, true);
                } else {
#ifdef JP_UNFAVORED_DEST
                    /* at the end of a region, you have to init live var 
                    information */
                    init_live_var_info(NULL);
#endif
                    RegAlloc_add_region_boundary(inst, i, false);
                }
            } else {
                // Get map from successor
                map * tbl2 = RegAlloc_backward_sweep(inst, succ);
                // Merge map information
                tbl = map_merge(tbl, tbl2);
            }
        }
    } else {
        RegAlloc_add_region_boundary(NULL, 0, true);
    }

    if (tbl == NULL) {
        if (Instr_IsExceptionReturn(inst)) {
/*          if (RegAlloc_Exception_Returning_Map != NULL */
/*  	    && Instr_GetNumOfSuccs(inst) == 0) { */
            assert(RegAlloc_Exception_Returning_Map != NULL);
            tbl = 
                map_Clone_from_alloc_status(RegAlloc_Exception_Returning_Map);
            return tbl;
        } else {
            tbl = map_Init(map_alloc());
        }
    }

    RegAlloc_update_preferred_assignment(tbl, inst);

    return tbl;
}

/* 
   Name        : reverse_RegAlloc_backward_sweep
   Description : routine for incremental backward sweep
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: 
 */
static
void 
reverse_RegAlloc_backward_sweep(InstrNode * instr, int succ)
{
    map * tbl;
    InstrNode * i = instr;

    assert(instr);
    assert(Instr_GetNumOfSuccs(instr) > succ);
    assert(Instr_GetH(Instr_GetSucc(instr, succ)) != NULL);

    tbl = map_Clone_from_alloc_status(Instr_GetH(Instr_GetSucc(instr, succ)));

    while(! Instr_IsVisited(i)) {
        RegAlloc_update_preferred_assignment(tbl, i);

        assert(Instr_GetNumOfPreds(i) == 1);

        i = Instr_GetPred(i, 0);
    }
}


/* 
   Name        : RegAlloc_perform_incremental_backward_sweep
   Description : Incremental backward sweep
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:
   In case of hammok code, the allocation result of one path can be used for
   the other path. For this, backward sweep must be performed incrementally
   after the forward sweep of one path is completed.
 */
static
void
RegAlloc_perform_incremental_backward_sweep(InstrNode* next_region, int path_id)
{
    int i;
    assert(path_id < number_of_region_boundary);

    for (i = number_of_region_boundary - 1; i > path_id;i--) {
        if (region_boundary[i].exit != NULL
            && Instr_GetSucc(region_boundary[i].exit, 
                           region_boundary[i].succ) == next_region) {
            reverse_RegAlloc_backward_sweep(region_boundary[i].exit,
                                    region_boundary[i].succ);
        }
    }
}
/* END RegAlloc_backward_sweep related */

/* BEGIN RegAlloc_forward_sweep */
static
void
RegAlloc_insert_dummy_instrs(InstrNode *prev, InstrNode *join, int succ,
                             InstrNode * old_path, InstrNode * new_path)
{
    int i, inserted = 0;
    int n_of_preds = Instr_GetNumOfPreds(join);
    InstrNode **preds = (InstrNode **)alloca(sizeof(InstrNode*) * n_of_preds);
    assert(prev);

    Instr_AddPred(new_path, prev); 
    Instr_SetSucc(prev, succ, new_path);
    Instr_SetPred(join, Instr_GetPredIndex(join, prev), new_path);
    Instr_AddSucc(new_path, join);

    for (i = 0; i < n_of_preds; i++) {
        preds[i] = Instr_GetPred(join, i);
    }

    for (i = 0; i < n_of_preds; i++) {
        InstrNode * pred = preds[i];
        
        assert(pred);

        if ((
            Instr_GetVisitedNum(pred) > Instr_GetSuccIndex(pred, join))
            && pred != new_path) {
            inserted = 1;
            Instr_AddPred(old_path, pred);
            Instr_SetSucc(pred, Instr_GetSuccIndex(pred, join), old_path);
            Instr_RemovePred(join, pred);
        }
    }

    assert(inserted);
    Instr_AddSucc(old_path, join);
    Instr_AddPred(join, old_path);

    assert(Instr_IsPred(join, old_path));
    assert(! prev || Instr_IsPred(join, new_path));
    assert(Instr_IsSucc(old_path, join));
    assert(! prev || Instr_IsSucc(new_path, join));
}

static
void
RegAlloc_insert_dummy_instrs_for_split(InstrNode *join, InstrNode * old_path)
{
    int i, inserted = 0, non_visited = 0;
    int n_of_preds = Instr_GetNumOfPreds(join);
    InstrNode **preds = (InstrNode**)alloca(sizeof(InstrNode*) * n_of_preds);

    for (i = 0; i < n_of_preds; i++) {
        preds[i] = Instr_GetPred(join, i);
    }
    for (i = 0; i < n_of_preds; i++) {
        InstrNode * pred = preds[i];
        
        assert(pred);

        if (
            Instr_IsVisited(pred)) {
            inserted = 1;
            Instr_AddPred(old_path, pred);
            Instr_SetSucc(pred, Instr_GetSuccIndex(pred, join), old_path);
            Instr_RemovePred(join, pred);
        } else {
            non_visited ++;
        }
    }

    assert(inserted);
    assert(non_visited);
    Instr_AddSucc(old_path, join);
    Instr_AddPred(join, old_path);

    assert(Instr_IsPred(join, old_path));
    assert(Instr_IsSucc(old_path, join));
}

static
void
RegAlloc_reconcile_allocation(AllocStat * h, InstrNode *prev, int succ_index)
{
    int i, r;
    InstrNode*   join = Instr_GetSucc(prev, succ_index);
    AllocStat*    old_h = Instr_GetH(join);
    iqueue*      work_list = iq_init(iq_alloc(),
				     CFG_GetNumOfStorageLocations(cfg));
    InstrNode    i_old_path, i_new_path;
    InstrNode *  old_path = &i_old_path, * new_path = &i_new_path;

    assert(AllocStat_CheckValidity(h));
    assert(AllocStat_CheckValidity(old_h));

    bzero(old_path, sizeof(InstrNode));
    bzero(new_path, sizeof(InstrNode));

    Instr_MarkVisited(old_path);
    Instr_MarkVisited(new_path);

    assert(cfg && prev && join && h);
    assert(old_h);

    // To generate the copies and spills easily,
    // insert dummy instructions before join points.
    RegAlloc_insert_dummy_instrs(prev, join, succ_index, old_path, new_path);

    for (r = 1; r < CFG_GetNumOfStorageLocations(cfg); r++) {
	if (Reg_IsAvailable(r) 
            && AllocStat_GetRefCount(old_h, r) > 0) {
	    iq_enq(work_list, r);
	    // Check if r is double register - this is an overhead.
	    if (Var_GetType(AllocStat_FindRMap(old_h, r, 0))==T_DOUBLE) {
		// skip pair one
		r++;
	    }
	}
    }

    while(iq_size(work_list)) {
	int    r, old_r2 = -1;
	int    *vars;
	int    initial_refcount;
	
	r = iq_deq(work_list);

	initial_refcount = AllocStat_GetRefCount(old_h, r);

	vars = (int *) alloca(sizeof(int) * initial_refcount);

	// make a snapshot of variables.
	for (i = 0; i < initial_refcount; i++) {
	    vars[i] = AllocStat_FindRMap(old_h, r, i);
	}

	for (i = 0; i < initial_refcount; i++) {
	    int v = vars[i];
	    int r2 = AllocStat_Find(h, v); 
	    // reg in path 2 (h)

	    if (! Reg_IsRegister(r2)) {
		// v -> r in h, but no v in old_h
		// Assume v is already dead. So we remove v->r;
		//
		// maybe error, for finally block
		AllocStat_Erase(old_h, v);
	    } else if (Reg_IsRegister(old_r2)) {
		if (old_r2 == r2) {
		    // consistent case - just changing mapping
		    // because the value is already copied or loaded into
		    // the register r.
		    AllocStat_ChangeMappedReg(h, v, r);
		} else {
		    // inconsistency - copy to new target
		    int new_r = AllocStat_FindFreeStorage(old_h, Var_GetType(v));
		    RegAlloc_generate_general_copy(old_path,
						   Var_GetType(v),
						   r, new_r);

		    AllocStat_ChangeMappedReg(old_h, v, new_r);
		    iq_enq(work_list, new_r);
		}
	    } else if (r2 == r) {
		// allocated in the same register - do nothing
		old_r2 = r;
	    } else {
		// allocated in the different register

		//
		// modified by doner
		// - For double registers,
		//   two registers must be freed atomically.
		//
		if (Var_GetType(v) == T_DOUBLE) {
		    RegAlloc_guarantee_double_registers_free(h, new_path, r);
		    assert(!AllocStat_GetRefCount(h , r) && !AllocStat_GetRefCount(h, r + 1));
		} else {
		    RegAlloc_guarantee_register_free(h, new_path, r);
		    assert(! AllocStat_GetRefCount(h, r));
		}

		RegAlloc_generate_general_copy(new_path,
							 Var_GetType(v),
							 r2, r);
		AllocStat_ChangeMappedReg(h, v, r);
		old_r2 = r2;
	    }
	}
    }

    CFG_RemoveInstr(NULL, old_path);
    CFG_RemoveInstr(NULL, new_path);

    assert(AllocStat_CheckValidity(h));
    assert(AllocStat_CheckValidity(old_h));
}


static
void
RegAlloc_split_variables(InstrNode * join, AllocStat * h)
{
    int r;
    InstrNode  i_old_path;
    InstrNode* old_path = &i_old_path;

    bzero(old_path, sizeof(InstrNode));
    Instr_MarkVisited(old_path);

    assert(AllocStat_CheckValidity(h));

    RegAlloc_insert_dummy_instrs_for_split(join, old_path);

    /*
    {
	int    n_of_preds = Instr_GetNumOfPreds(old_path);
	InstrNode** preds =
	    (InstrNode**)alloca(sizeof(InstrNode*) * n_of_preds);
	int    i;


	for (i = 0; i < n_of_preds; i++) {
	    preds[i] = Instr_GetPred(old_path, i);
	}
	for (i = 0; i < n_of_preds; i++) {
	    if (Instr_IsNop( preds[i] )) {
		assert( Instr_IsLoopHeader( preds[i] ) );
		CFG_RemoveInstr( cfg, preds[i] );
	    }
	}
    }
    */
    
    for (r = 1; r < Reg_number; r++) {
        while (AllocStat_GetRefCount(h, r) > 1) {
            int v = AllocStat_FindRMap(h, r, 1);
            int new_r;

            //
            // inserted by doner
            // - If v == -1, r is mapped to FVOID. Therefore skip it.
            //
            assert(v != 0);
            if (v == -1) {
                continue;
            }

            new_r = AllocStat_FindFreeReg(h, Var_GetType(v));

            if (Reg_IsHW(new_r)) {
                RegAlloc_generate_copy(old_path, Var_GetType(v), r, new_r);
                AllocStat_ChangeMappedReg(h, v, new_r);
            } else {
                int temp_r = AllocStat_FindFreeSpill(h, Var_GetType(v));
                RegAlloc_generate_spill(old_path, Var_GetType(v), r, temp_r);
                AllocStat_ChangeMappedReg(h, v, temp_r);
            }
        }
    }

    for (r = Reg_number; r < CFG_GetNumOfStorageLocations(cfg); r++) {
        while (AllocStat_GetRefCount(h, r) > 1) {
            int v = AllocStat_FindRMap(h, r, 1);
            int new_r;

            //
            // inserted by doner
            // - If v == -1, r is mapped to FVOID. Therefore skip it.
            //
            assert(v != 0);
            if (v == -1) {
                continue;
            }

            new_r = AllocStat_FindFreeReg(h, Var_GetType(v));

            if (Reg_IsHW(new_r)) {
                RegAlloc_generate_promotion(old_path, Var_GetType(v), 
                                  r, new_r);
                AllocStat_ChangeMappedReg(h, v, new_r);
            } else {
                // While splitting, we are short of free registers.
                // We should split them into spill locations.
                new_r = AllocStat_FindFreeSpill(h, Var_GetType(v));
                RegAlloc_generate_general_copy(old_path, Var_GetType(v),
                                               r, new_r);
                AllocStat_ChangeMappedReg(h, v, new_r);
            }
        }

    }

    assert(AllocStat_CheckValidity(h));

    CFG_RemoveInstr(NULL, old_path); /* NULL means that "old_path" is temporary. */
}


//////  /////   /////   //    //    //    /////   ////
//     //   //  //  //  //    //   ////   //  //  // //
////   //   //  /////   // // //  //  //  /////   //  // 
//     //   //  //  //   //////  //////// //  //  //  // 
//      /////   //   //  //  //  //    // //   // /////

 //// //      // /////// /////// //////
//    //      // //      //      //   //
 //// //  //  // /////// /////// //   //
    // ////////  //      //      //////
    //  //  //   //      //      //
/////   //  //   /////// /////// //


inline
static
int RegAlloc_get_stack_offset_of_arg(int i)
{
    return 68 + i * 4;
}


static
void
RegAlloc_prepare_allocation(InstrNode * instr, AllocStat *h)
{
    int i, r; 
#ifdef JP_EXAMINE
    int num_copy_int_to_arg = 0;
    int num_save_caller_in_callee = 0;
    int num_save_caller_in_mem = 0;
    int num_save_float_in_mem = 0;
    int num_spill_arg_to_callee = 0;
    int num_spill_arg_to_mem = 0;
    int num_copy_float_to_arg = 0;
    int num_copy_double_to_arg = 0;
#endif

    if (Instr_IsCall(instr)) {
        for (r = o0, i = 0; r <= o5; r++, i++) {

            int arg_var = -1; 
            int from_r = -1; 
            int need_copy = 0;

            assert(r == i + o0);

            // Check whether copy is necessary
            // for keeping calling convention
            if (i < Instr_GetNumOfArgs(instr)) {
                
                arg_var = Instr_GetArg(instr, i);
                from_r = AllocStat_Find(h, arg_var);
                if (from_r != r) { 
                    need_copy = 1;
                }

                // If a caller save register is used,
                // spill or move the caller save register.
                /* 1 in the reference count means the call instruction */
                if ((need_copy == 0 && AllocStat_GetRefCount(h, r) > 1)
                    || (need_copy == 1 && AllocStat_GetRefCount(h, r) > 0)) 
                {       
                    
                    int org_v = AllocStat_FindRMap(h, r, 0);
                    int new_r =
                        AllocStat_FindFreeNonCallerSaveReg(h, Var_GetType(org_v));
                        
                    if (Reg_IsHW(new_r)) {
#ifdef JP_EXAMINE
                        num_spill_arg_to_callee++;
#endif
                        RegAlloc_copy_register(h, instr, r, new_r);
                    } else {
#ifdef JP_EXAMINE
                        num_spill_arg_to_mem++;
#endif  
                        RegAlloc_spill_register(h, instr, r);
                    }
                        
                    assert(AllocStat_GetRefCount(h, r) == 0);
                }

                //
                // inserted by doner
                // - When an out register is mapped to a variable already,
                //   if arg_var is a DOUBLE, r+1 should be considered.
                //
                if (Var_GetType(arg_var) == T_DOUBLE && r < o5) {
                    assert(Var_GetType(Instr_GetArg(instr, i + 1))
			    == T_FVOID);

                    if (AllocStat_GetRefCount(h, r + 1) > 0) {
                        int   org_v = AllocStat_FindRMap(h, r + 1, 0);
                        int   new_r =
                            AllocStat_FindFreeNonCallerSaveReg(h,
                                                                  Var_GetType(org_v));

                        assert(org_v != -1);

                        if (Reg_IsHW(new_r)) {
                            RegAlloc_copy_register(h, instr, r + 1, new_r);
                        } else {
                            RegAlloc_spill_register(h, instr, r + 1);
                        }

                        assert(AllocStat_GetRefCount(h, r + 1) == 0);
                    }
                }
                    
                // Call parameter becomes dead when it meets a call operation.
                AllocStat_Erase(h, arg_var);
                
                if (need_copy) {
                    // Need to generate copy for calling parameter
                    int type = Var_GetType(arg_var);
                    
                    assert(Reg_IsRegister(from_r));
                    assert(from_r != r);
                    assert(r == Reg_get_parameter(i));
                    
                    //
                    // modified by doner
                    //
                    if (Reg_IsHW(from_r)) {
                        switch (type) {
                            
                          case T_FLOAT: {
                              int   temp_r = AllocStat_FindFreeSpill(h, T_FLOAT);
#ifdef JP_EXAMINE
                              num_copy_float_to_arg++;
#endif
                              RegAlloc_generate_spill(instr, T_FLOAT, from_r, temp_r);
                              RegAlloc_generate_promotion(instr, T_INT, temp_r, r);
			      /* When multiple variables are mapped to a float
				 register and one of them are used for
				 an argument, the remaining variables had
				 better be mapped to the spill that was used
				 for the tranfer between the float register
				 and the argument register. Because the float
				 register will be spilled because it is a
				 caller-save register. - doner at 9/17/99 */
			      if (AllocStat_GetRefCount(h, from_r) > 0) {
				  AllocStat_ChangeReg(h, from_r, temp_r);
			      }
                          }
                          break;
			case T_DOUBLE: {
                              int   temp_r;
#ifdef JP_EXAMINE
                              num_copy_double_to_arg++;
#endif
                              if (r < o0 + 5) {
                                  /* use store double */
                                  temp_r = AllocStat_FindFreeSpill(h, T_DOUBLE);
                                  RegAlloc_generate_spill(instr, T_DOUBLE, from_r, temp_r);
                                  RegAlloc_generate_promotion(instr, T_INT, temp_r, r + 1);
                                  RegAlloc_generate_promotion(instr, T_INT, temp_r + 1, r);
				  if (AllocStat_GetRefCount(h, from_r) > 0) {
				      AllocStat_ChangeReg(h, from_r, temp_r);
				  }
                              } else if (r == o5) {
                                  InstrNode *new_instr;
                                  /* upper half->o5, lower half->mem */
                                  temp_r = AllocStat_FindFreeSpill(h, T_FLOAT);
                                  RegAlloc_generate_spill(instr, T_FLOAT, from_r,
                                                  temp_r);
                                  RegAlloc_generate_promotion(instr, T_INT,
                                                    temp_r, r);
                                  new_instr = 
                                      create_format8_instruction(STF,
                                                                 from_r + 1,
                                                                 sp,
                                                                 RegAlloc_get_stack_offset_of_arg(i+1),
                                                                 -1);
                                  CFG_InsertInstrAsSinglePred(cfg, 
                                                                           instr, 
                                                                           new_instr);
                              } else { assert(0); }

                              /* skip pair register */
                              r++, i++;
                              break;
                          }
                          
                          case T_FVOID:
                            assert(0 && "T_FVOID has been mistreated!");
                            break;

                         default:
#ifdef JP_EXAMINE
                            num_copy_int_to_arg++;
#endif 
                            RegAlloc_generate_copy(instr, T_INT, from_r, r);
                        } /* end of switch */
                    } else {
                        /* the variable was in the memory */
                        switch (type) {
                        
                          case T_FLOAT:
                            RegAlloc_generate_promotion(instr, T_INT,
                                              from_r, r);
                            break;

                          case T_DOUBLE:
                            RegAlloc_generate_promotion(instr, T_INT,
                                              from_r + 1, r);

                            if (r < o0 + 5) {
                                /* load two values in integer registers */
                                RegAlloc_generate_promotion(instr, T_INT,
                                                  from_r, r + 1);
                            } else if (r == o5) {
                                /* load upper half in o5 and 
                                move lower half in memory into a memory slot of 
                                param. passing */
                                InstrNode* new_instr;
                                RegAlloc_generate_promotion(instr, T_INT,
                                                            from_r, g1);
                                new_instr = 
                                    create_format8_instruction(ST, g1, sp,
                                                               RegAlloc_get_stack_offset_of_arg(i + 1),
                                                               -1);
                                CFG_InsertInstrAsSinglePred(cfg, instr,
                                                            new_instr);
                            } else { 
                                assert(0); 
                            }
                            r++, i++;
                            break;

                          case T_FVOID:
                            assert(0 && "T_FVOID should not be here!");
                            break;

                          default:
                            RegAlloc_generate_promotion(instr, T_INT,
                                                        from_r, r);
                            break;
                        }
                    }
                }
            } else if (AllocStat_GetRefCount(h, r) > 0) {
                /* not argument register, but caller-saves register 
                that is live across function call */
                int org_v = AllocStat_FindRMap(h, r, 0);
                int new_r =
                    AllocStat_FindFreeNonCallerSaveReg(h, Var_GetType(org_v));

                if (Reg_IsHW(new_r)) {
#ifdef JP_EXAMINE
                    num_spill_arg_to_callee++;
#endif
                    RegAlloc_copy_register(h, instr, r, new_r);
                } else {
#ifdef JP_EXAMINE
                    num_spill_arg_to_mem++;
#endif
                    RegAlloc_spill_register(h, instr, r);
                }

                assert(AllocStat_GetRefCount(h, r) == 0);
            }
        }
        
        //
        // inserted by doner
        //
        for (; i < Instr_GetNumOfArgs(instr); i++) {
            /* usually, i==6 here.
	       But if the upper half of double is passed in
	       %o5, i is 7 */
            InstrNode*    new_instr = NULL;
            int           arg_var; 
            int           from_r;
            
            arg_var = Instr_GetArg(instr, i);
            from_r = AllocStat_Find(h, arg_var);

            assert(from_r != -1);

            AllocStat_Erase(h, arg_var);

            if (Reg_IsHW(from_r)) {
                switch (Var_GetType(arg_var)) {
                  case T_INT: case T_LONG: case T_IVOID: case T_REF:
                    new_instr = 
                        create_format8_instruction(ST, from_r, sp,
                                                   68 + i * 4, -1);
                    break;

                  case T_FLOAT: 
                    new_instr = 
                        create_format8_instruction(STF, from_r, sp,
                                                   68 + i * 4, -1);
                    break;

                  case T_DOUBLE:
                    if (i % 2 != 0) { // 68 + i * 4 is a multiple of 8.
                        /* generate STDF */
                        new_instr = create_format8_instruction(STDF, from_r, sp,
                                                               68 + i * 4, -1);
                    } else {
                        new_instr = create_format8_instruction(STF, from_r, sp,
                                                               68 + i * 4, -1);
                        CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);

                        new_instr = create_format8_instruction(STF, from_r + 1, sp,
                                                               68 + (i + 1) * 4, -1);
                    }
                    i++;
                    break;

                  case T_FVOID:
                    assert(0&&"FVOID?");
                    break;

                  default:
                    assert(0 && "strange variable type!");
                    break;
                }

                CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
            } else {
                switch (Var_GetType(arg_var)) {
                  case T_INT: case T_LONG: case T_IVOID: case T_REF: case T_FLOAT:
                    // now use %g1 as the scratch register
                    new_instr = create_format5_instruction(LD, g1, fp,
                                                           RegAlloc_get_spill_offset(from_r), -1);
                    CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
                
                    new_instr = create_format8_instruction(ST, g1, sp,
                                                           68 + i * 4, -1);
                    CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
                    break;
		    
                  case T_DOUBLE:
                    new_instr = create_format5_instruction(LD, g1, fp,
                                                           RegAlloc_get_spill_offset(from_r+1), -1);
                    CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
                
                    new_instr = create_format8_instruction(ST, g1, sp,
                                                           68 + i * 4, -1);
                    CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);

                    i++;
                    new_instr = create_format5_instruction(LD, g1, fp,
                                                           RegAlloc_get_spill_offset(from_r), -1);
                    CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
                
                    new_instr = create_format8_instruction(ST, g1, sp,
                                                           68 + i * 4, -1);
                    CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
                    break;

                  case T_FVOID:
                    assert(0 && "FVOID?");
                    break;

                  default:
                    assert(0 && "strange variable type!");
                    break;
                }
            }
        }

        for (r = f0; r <= flast; r++) {
            /* If float variable is used for argument passing
            and the call was the last use, a redundant spill
            instruction is generated here.!!! -> Let's modify! */
            if (AllocStat_GetRefCount(h, r) > 0) {   
#ifdef JP_EXAMINE
                num_save_float_in_mem++;
#endif
                /* You don't have to skip FVOID since 
                spilling f_(n) makes ref count of f_(n+1) zero */

                RegAlloc_spill_register(h, instr, r);
            }
            assert(AllocStat_GetRefCount(h, r) == 0);
        }

    } else if (Instr_IsReturn(instr)) {
        // Copy generation for returning value.
        for (i = 0; i < Instr_GetNumOfRets(instr); i++) {
            int var = Instr_GetRet(instr, i);
            int org_r = AllocStat_Find(h, var);
            int new_r = Reg_get_returning(Var_GetType(var));
            int type = Var_GetType(var);

            if (org_r != new_r) {
                if (Reg_IsHW(org_r)) {
                    /* register to register */
                    switch (type) {
                      case T_FVOID:
                        assert(0 && "FVOID?");
                        break;

		    default:
                        /* copy is sufficient for all */
                        RegAlloc_generate_copy(instr, type, org_r, new_r);
                        break;
                    } 
                } else {
                    /* mem to register */
                    switch (type) {
                      case T_FVOID:
                        assert(0 && "FVOID?");
                        break;
                      case T_DOUBLE:
                        RegAlloc_generate_promotion(instr, type, 
                                                    org_r, new_r);
                        /* not here : i++; */
                        break;
                      default:
                        RegAlloc_generate_promotion(instr, type, 
                                                    org_r, new_r);
                        break;
                    }
                }
            }
            /* skip odd-numbered float for double */
            if (type==T_DOUBLE) i++;
        }
    } else if (Instr_IsExceptionReturn(instr)) {
/*      } else if (Instr_GetNumOfSuccs(instr) == 0  */
/*                 && RegAlloc_Exception_Returning_Map != NULL  */
/*                 && Instr_GetCode(instr) == CALL) { */
        assert(RegAlloc_Exception_Returning_Map != NULL);
        
        for(i = 0; i < CFG_GetNumOfVars(cfg); i++) {
            Reg org_reg;
            Reg new_reg;

            org_reg = AllocStat_Find(h, i);
            new_reg = AllocStat_Find(RegAlloc_Exception_Returning_Map, i);

            if (org_reg != new_reg && new_reg != -1) {
                RegAlloc_generate_general_copy(instr, exception_return_type,
                                               org_reg, new_reg);
            }

/*              if (exception_return_type == T_LONG) { */
/*                  org_reg = AllocStat_Find(h, exception_return_var[1]); */
/*                  new_reg = AllocStat_Find(RegAlloc_Exception_Returning_Map, */
/*                                           exception_return_var[1]); */

/*                  if (org_reg != new_reg) */
/*                    RegAlloc_generate_copy(instr, T_IVOID, org_reg, new_reg); */
/*              } */
        }
    }

#ifdef JP_EXAMINE
    if (num_copy_int_to_arg) 
      printf("copy int->arg(%d)\n", num_copy_int_to_arg);
    if (num_save_caller_in_callee) 
      printf("copy caller->callee(%d)\n", num_save_caller_in_callee);
    if (num_save_caller_in_mem)      
      printf("store caller->mem(%d)\n", num_save_caller_in_callee);
    if (num_save_float_in_mem) 
      printf("store float->mem(%d)\n", num_save_caller_in_mem);
    if (num_spill_arg_to_callee)
      printf("copy arg->callee(%d)\n", num_spill_arg_to_callee);
    if (num_spill_arg_to_mem)
      printf("store arg->mem(%d)\n", num_spill_arg_to_mem);
    if (num_copy_float_to_arg)
      printf("store float->mem & load mem->arg(%d)\n", num_copy_float_to_arg);
    if (num_copy_double_to_arg) 
      printf("store double->mem & load mem->arg(%d)\n", num_copy_double_to_arg);
#endif
}


static
void
RegAlloc_finish_allocation(InstrNode * instr, AllocStat *h)
{
    if (Instr_IsCall(instr)) {
        // Insert new mapping for return value of 'call'.
        int i;
        for (i = 0; i < Instr_GetNumOfRets(instr); i++) {
            int var = Instr_GetRet(instr, i);

            AllocStat_Insert(h, var, Reg_get_returned(Var_GetType(var)));

            // - T_FVOID does not enter 'h' map.
            if (Var_GetType(var) == T_DOUBLE) {
                break;
            }
        }
    }
}

///
/// Function Name : RegAlloc_select_destination
/// Author : ramdrive
/// Input : CFG, instruction, itable, reference count
/// Output : destination register
/// Pre-Condition : valid CFG, instruction, itable, refcount
/// Post-Condition : physical register is returned.
/// Description
///    
///     Select apropriate hardware register for destination.
///     Selected register must be free or otherwize, should be freed.
static
int
RegAlloc_select_destination(AllocStat *h, InstrNode *instr)
{
    int dest_var = Instr_GetDestReg(instr, 0);
    int preferred = Instr_GetPreferredDest(instr);
    int r; // scratch

    // If the destination register is already a hardware register,
    // just skip it. Precolored registers includes 
    // fp, sp, i7, o7, g0, 
    if (Reg_IsHW(dest_var)) {
        return dest_var;
    }

    // Try preferred register
    if (Reg_IsHW(preferred)
        && AllocStat_GetRefCount(h, preferred) == 0) {

        // Preferred registers are always general purpose registers.
        // If current dest_var is any floating point type,
        // we should not use preferred register.
        if (Var_GetType(dest_var) != T_FLOAT
            && Var_GetType(dest_var) != T_DOUBLE) {
            return preferred;
        }
        if (Reg_GetType(preferred) == T_FLOAT) {
          return preferred;
        }
    }

    // select arbitrary free register
    r = AllocStat_FindFreeReg(h, Var_GetType(dest_var));

    // We found a free hardware register.
    if (Reg_IsHW(r)) return r;

    // No free register. We need to spill 
    if (Reg_IsHW(preferred)) {
        // Try to spill preferred register.
        RegAlloc_spill_register(h, instr, preferred);
        return preferred;
    }

    // No preferred register.
    // Spill any register.
    return RegAlloc_spill_non_src_register(h, instr, Var_GetType(dest_var));
}

///
/// Function Name : RegAlloc_allocate_registers
/// Author : ramdrive
/// Input : instruction , allocation status
/// Output : nothing
/// Pre-Condition
///     Calling convention is already kept by RegAlloc_prepare_allocation.
/// Post-Condition
///     All source and destination registers are hardware registers.
/// Description
/// 
///     Allocate registers for source and destination registers.
///
static
int
RegAlloc_allocate_registers(InstrNode * instr, AllocStat *h) {

    int i, v, dest_r;
    int dest_v = Instr_GetDestReg(instr, 0);

    //
    //   for (;;) ;
    //   In this case, 'nop' points to its region dummy.
    //   Since the region dummy will be removed after register allocation,
    //   the 'nop' must be deleted.     
    //
    if (Instr_IsNop(instr) &&
	Instr_GetSucc(instr, 0) != Instr_GetPred(instr, 0)) {
        for (i = 0; Var_IsVariable(v = Instr_GetSrcReg(instr, i)); i++) {
            if (Instr_IsLastUse(instr, v) && AllocStat_Find(h, v) != -1) AllocStat_Erase(h, v);
        }
        return true;
    }

    //if (! flag_no_copy_coalescing && Var_IsVariable(dest_v)) {
    if (Var_IsVariable(dest_v)) {

        if ((Instr_IsICopy(instr)
             && ((v = Instr_GetSrcReg(instr, 0)) || 1))
            || (Instr_IsFCopy(instr)
                && ((v = Instr_GetSrcReg(instr, 1)) || 1))) {
            if (Var_IsVariable(v)) {
                int reg = AllocStat_Find(h, v);

                // Instruction is a copy.
                assert(Reg_IsRegister(AllocStat_Find(h, v)));

                if (Instr_IsLastUse(instr, v)) {
                    AllocStat_Erase(h, v);
                }

                AllocStat_Insert(h, dest_v, reg);

                if (!flag_no_copy_coalescing) {
                    // instruction will be deleted.
                    return true;
                } else {    
                    if (Instr_IsICopy(instr)) {
                        Instr_SetSrcReg(instr, 0, g0);
                        Instr_SetDestReg(instr, 0, g0);
                    } else {
                        Instr_SetSrcReg(instr, 1, f0);
                        Instr_SetDestReg(instr, 0, f0);
                    }
                    return false;
                }
            }
        }
    }  

    // Normal case. Allocating source & destination register.

    // Allocate source registers.
    for (i = 0; i < Instr_GetNumOfSrcRegs(instr); i++) {
        v = Instr_GetSrcReg(instr, i);
        if (Var_IsVariable(v)) {

            int r = AllocStat_Find(h, v);

            if (Reg_IsSpill(r)) {
                if (Instr_IsCall(instr)) {
                    r = AllocStat_FindFreeNonCallerSaveReg(h, Var_GetType(v));
                    if (! Reg_IsHW(r)) {
                        RegAlloc_spill_register(h, instr, l0);
                        r = l0;
                    }
                } else {
                    r = AllocStat_FindFreeReg(h, Var_GetType(v));

                    if (! Reg_IsHW(r)) {
                        r = RegAlloc_spill_non_src_register(h, instr,
                                                            Var_GetType(v));
                    }
                }
                RegAlloc_promote_to_register(h, instr, v, r);
            }
            assert(Reg_IsHW(r));
            Instr_SetSrcReg(instr, i, r);
        }
    }

    /* Erase mapping here, after all source registers have been set ! */
    for (i = 0; i < 3; i++) {    // Is this always 3 ? Or less.
        /* I don't like direct access to lastUsedRegs...
        But, what else can I do? */
        if (Var_IsVariable(v = instr->lastUsedRegs[i])) {
            AllocStat_Erase(h,v);
        }
    }

    // Allocate destination register.
    if (Reg_IsHW(Instr_GetDestReg(instr, 0))) {
        return false;
    }
    if (! Var_IsVariable(Instr_GetDestReg(instr, 0))) {
        return false;
    }

    // special case handling
    // if double is passed as 2 separated spill location,
    // the type of dest_v can be T_FVOID.
    // if so, we must avoid insert the variable into the map
    // -hacker
    if ((Var_GetType(dest_v) == T_FVOID) 
        && (Instr_GetBytecodePC(instr) == 0)) {
        assert(Instr_GetNumOfPreds(instr) == 1);
        dest_r = Instr_GetDestReg(Instr_GetPred(instr, 0), 0) + 1;
        Instr_SetDestReg(instr, 0, dest_r);
    } else {
        dest_r = RegAlloc_select_destination(h, instr);
        assert(Reg_IsHW(dest_r));
        Instr_SetDestReg(instr, 0, dest_r);
        AllocStat_Insert(h, dest_v, dest_r);
    }

    return false;
}

static inline
void
update_callee_method_end_info(InlineGraph *graph, InstrNode *instr,
                              InstrNode *ninstr) {
    InlineGraph *ngraph;

    if (graph == NULL || IG_GetTail(graph) != instr) return;

    IG_SetTail(graph, ninstr);
    
    for(ngraph = IG_GetCallee(graph); ngraph == NULL;
        ngraph = IG_GetNextCallee(graph)) {
        update_callee_method_end_info(ngraph, instr, ninstr);
    }
}

static inline
void
update_method_end_info(InstrNode *instr, InstrNode *ninstr) {
    InlineGraph *graph = Instr_GetInlineGraph(instr);
    InlineGraph *ngraph;

    /* update callers */
    for(ngraph = graph; IG_GetTail(ngraph) == instr;
        ngraph = IG_GetCaller(ngraph)) IG_SetTail(ngraph, ninstr);
    
    /* update callees */
    update_callee_method_end_info(graph, instr, ninstr);
}

static int RegAlloc_forward_sweep_current_path_id;

/* 
   Name        : RegAlloc_forward_sweep
   Description : the main register allocation function
   Maintainer  : Byung-Sun Yang <scdoner@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: 
 */
static
void
RegAlloc_forward_sweep(InstrNode * instr, AllocStat *h)
{
    int deleted;

    Instr_MarkVisited(instr);
    RegAlloc_prepare_allocation(instr, h);
    deleted = RegAlloc_allocate_registers(instr, h);
    if (! deleted) RegAlloc_finish_allocation(instr, h);


    //
    // inserted by doner
    // - for exception handling
    //
    if (Instr_NeedMapInfo(instr) != NO_NEED_MAP_INFO) {
        record_variable_map(instr, h);
    }

    if (Instr_IsMethodEnd(instr) && instr->bytecodePC == -9) {
        AllocStat_RemoveDeadVars(h, instr);
    }

    assert(AllocStat_CheckValidity(h));

    if (Instr_GetNumOfSuccs(instr) == 1) {
        InstrNode * succ = Instr_GetSucc(instr, 0);
        if (Instr_IsRegionEnd(succ)) {
            if (! Instr_GetH(succ)) {
                Instr_SetH(succ, AllocStat_Clone(h));
                if (! flag_no_backward_sweep) {
                    RegAlloc_perform_incremental_backward_sweep(succ,
                            RegAlloc_forward_sweep_current_path_id);
                }
            } else {
                RegAlloc_reconcile_allocation(h, instr, 0);
            }
        } else {
            RegAlloc_forward_sweep(succ, h);
        }
    } else if (Instr_GetNumOfSuccs(instr) > 1) {
        int i = 0;
        for (i = 0; i < Instr_GetNumOfSuccs(instr); i++) {
            InstrNode * succ = Instr_GetSucc(instr, i);
            if (i > 0) RegAlloc_forward_sweep_current_path_id ++;
            if (i > 0) Instr_MarkVisited(instr);

            if (Instr_IsRegionEnd(succ)) {
                if (! Instr_GetH(succ)) {
                    Instr_SetH(succ, AllocStat_Clone(h));

                    if (! flag_no_backward_sweep) {
                        RegAlloc_perform_incremental_backward_sweep(succ,
                            RegAlloc_forward_sweep_current_path_id);
                    }
                } else {
                    RegAlloc_reconcile_allocation(AllocStat_Clone(h), instr, i);
                }
            } else {
                AllocStat * h2 = AllocStat_Clone(h);
		AllocStat_RemoveDeadVars(h2, succ);
                RegAlloc_forward_sweep(succ, h2);
            }
        }
    }

    // Delete instruction by DFS order.
    if (deleted) {
        InstrNode *n_instr;
#if 0        
        InlineGraph *graph;
        InlineGraph *caller;
#endif 0        

        assert(Instr_GetNumOfSuccs(instr) == 1);
        n_instr = Instr_GetSucc(instr, 0);

	assert(!Instr_IsJoin(instr) && !Instr_IsRegionDummy(instr));
	
	/*
        if (Instr_IsJoin(instr) && Instr_GetH(instr) 
            && !Instr_GetH(n_instr)) {
            // If deleted instruction is a join point,
            // that is, a region header, 
            // then we should store h map to successor.
            // (really?)
            // Yes. Because of the loop. 
            assert(Instr_GetH(instr) != NULL);
            assert(Instr_GetNumOfSuccs(instr) == 1);
            
            Instr_SetH(n_instr, Instr_GetH(instr));
	    }
	*/

        if (Instr_IsMethodEnd(instr)) {
            update_method_end_info(instr, n_instr);
#if 0            
            graph = Instr_GetInlineGraph(instr);

            assert(n_instr->graph == NULL || Instr_NeedMapInfo(n_instr) == 0);

            Instr_SetInlineGraph(n_instr, graph);

            for (caller = graph; 
                 IG_GetTail(caller) == instr; 
                 caller = IG_GetCaller(caller)) {
                IG_SetTail(caller, n_instr);
            }
#endif 0
        }

	/*
	if (Instr_IsLoopHeader(instr)) {
	    if (Instr_IsRegionEnd(n_instr)) {
		InstrNode* nop_instr = Instr_create_nop();
		CFG_InsertInstrAsSucc( cfg, instr, 0, nop_instr );
		Instr_MarkVisited( nop_instr );
		n_instr = nop_instr;
	    }
	    Instr_SetLoopHeader(n_instr);
	}
	*/
	
        CFG_RemoveInstr(cfg, instr);
    }
}
    
/* END RegAlloc_forward_sweep */




/* BEGIN allocator */
int
RegAlloc_detect_region_headers(InstrNode *instr)
{
    int i;
    int num_of_instrs = 0;

    Instr_MarkVisited(instr);

    for (i = 0; i < Instr_GetNumOfSuccs(instr); i++) {
        InstrNode *p = Instr_GetSucc(instr, i);
        if (! Instr_IsVisited(p)) {
            num_of_instrs += RegAlloc_detect_region_headers(p);
        }
    }

    num_of_instrs++;

    if (Instr_IsRegionEnd(instr)) {
        assert(number_of_region_headers < MAX_REGION_HEADER);
        region_instrs[number_of_region_headers] = num_of_instrs;
        region_headers[number_of_region_headers++] = instr;
        return 0;
    }
    
    return num_of_instrs;
}

/* This method corrupts the region_headers[] array assuming that the
   previous regions are already treated, so we don't have to keep them
   in this array. If you refer to the array in other places than
   RegAlloc_perform_fast_register_allocation(), you have to fix
   here. FIXME. */
static
int
RegAlloc_update_region_headers(InstrNode** additional_region_headers,
                               int num_additional_region_headers,
                               InstrNode* new_loop_entry,
                               int cur_region_id)
{
    int j;
    for (j=0; j<num_additional_region_headers; j++) {
        region_headers[cur_region_id+j] = additional_region_headers[j];
    }
    region_headers[cur_region_id+num_additional_region_headers] = 
        new_loop_entry;
    return cur_region_id+num_additional_region_headers+1;
}




static int        number_of_region_dummys;
static InstrNode* region_dummys[MAX_REGION_DUMMYS];

void
RegAlloc_register_region_dummy(InstrNode* dummy)
{
    assert(Instr_IsRegionDummy(dummy));
    assert(number_of_region_dummys < MAX_REGION_DUMMYS);

    region_dummys[number_of_region_dummys++] = dummy;
}

InstrNode*
RegAlloc_insert_region_dummy(CFG* cfg, InstrNode* region_header)
{
    InstrNode*   dummy = Instr_create_region_dummy();
    
    CFG_InsertInstrAsSinglePred(cfg, region_header, dummy);
    RegAlloc_register_region_dummy(dummy);
    return dummy;
}

inline
static
void
RegAlloc_insert_region_dummys(CFG* cfg)
{
    int   i;
    number_of_region_dummys = 0;
    
    for (i = 0; i < number_of_region_headers - 1; i++) {
	region_headers[i] =
	    RegAlloc_insert_region_dummy(cfg, region_headers[i]);
    }
}

static
void
RegAlloc_remove_region_dummys(CFG* cfg)
{
    int   i;
    
    for (i = 0; i < number_of_region_dummys; i++) {
	CFG_RemoveInstr(cfg, region_dummys[i]);
    }
}


///
/// Function Name : RegAlloc_perform_fast_register_allocation
/// Author : ramdrive
/// Input : control flow graph
/// Output
///     No return value
///     Allocate & assign physical registers to symbolic.
///     Determine stack_size of CFG.
/// Pre-Condition
///     Global variable "region_headers" should be null.
/// Post-Condition
///     Global variable "region_headers" should be null also.
/// Description
///     Fast register allocator driver function.
///     Traverse regions,
///     and call RegAlloc_backward_sweep() & RegAlloc_forward_sweep() once for
///     each region.
///
///     
static
void
RegAlloc_perform_fast_register_allocation(InstrNode * root, AllocStat * h)
{
    int i;
    SMA_Session _session;

    int next_region_to_loop_optimize;

#ifdef JP_EXAMINE_SPILL
    num_spill = 0;
#endif
    assert(cfg && root && h);

    // Acquire array of region headers in reverse post order.
    // "root" is always region header. 
    // Except for root, every region headers are join points.
    CFG_PrepareTraversal(cfg);
    number_of_region_headers = 0;
    
    region_instrs[number_of_region_headers] 
        = RegAlloc_detect_region_headers(root);
    region_headers[number_of_region_headers++] = root;

    //
    // In order to prevent a region header from being deleted
    // as a result of register allocation or other optimization,
    // I determined to insert a dummy instruction at the head of each region.
    // This dummy instruction must survive any types of optimizations.
    // After the register allocation and optimization is done,
    // the region dummy instructions will be removed.
    // - doner, 9/30/99
    //
    RegAlloc_insert_region_dummys(cfg);
    
    if (! flag_no_live_analysis) {
        CFG_PrepareTraversal(cfg);
        LA_live_analysis(root);
    }


    CFG_PrepareTraversal(cfg);
    Instr_SetH(root, h);


    SMA_InitSession(&_session, 1024 * 16);

    next_region_to_loop_optimize = number_of_region_headers;

    for (i = number_of_region_headers - 1; i >= 0 ; i--) {
        int j;
        InstrNode * instr = region_headers[i];
        AllocStat *h2;

#ifdef JP_EXTENDED_REGION
        if (instr->is_region_header == false) {
            printf("Skip region : ");
            Instr_Print(instr, outout, '\n');
            continue;
        }
#endif
        for (j = 0; j < Instr_GetNumOfPreds(instr); j++) {
            if (! Instr_IsVisited(Instr_GetPred(instr, j)))  {
                RegAlloc_split_variables(instr, Instr_GetH(instr));
                Instr_SetLoopHeader(instr);
                break;
            }
        }

        if (i < next_region_to_loop_optimize) {
            extern int number_of_vn_var;
            number_of_vn_var = 0;   // Enforce initialization of vn_var for loop optimization.
            // redundancy elimination, constant folding & propagation
            if ((! flag_no_regionopt && flag_no_regionopt_num != num)
                || flag_regionopt_num == num) {

                // Optimization phases should *NOT* call
                // Instr_MarkVisited() for any instructions. 
                // It will make stage3 fail. // by ramdrive
                void VN_optimize_region(SMA_Session *, InstrNode *,
                                        AllocStat *); 

                SMA_RestartSession(&_session);

                if (! flag_no_cse) {
                    VN_optimize_region(&_session, instr, Instr_GetH(instr));
                }
                if (! flag_no_dce || flag_dce_num == num) {
                    LA_eliminate_dead_code_in_region(&_session, instr);
                }
            }
        }

        /* LoopOpt is better performed after CSE and constant
            propagation.  Many loop-invariants are instructions that load
            the constants, so they are naturally removed by constant
            propagation. */
        if ((!flag_no_loopopt || flag_loopopt_num == num)
            && Instr_IsLoopHeader(instr) && i<next_region_to_loop_optimize) 
        {
            /* Try LoopOpt only when instr is not set to be a loop header
            yet. After LoopOpt, the optimized loop will already be
            marked to be "loop header", which will prevent the loop
            from being optimized again. */
            extern
                int LoopOpt_optimize_loop(CFG* cfg, InstrNode* instr,
                                          InstrNode** i1, InstrNode** i2,
                                          InstrNode*** additional_region_headers);
            /* optimize_loop may alter head instr after code motion */
            InstrNode *new_region_entry, *new_loop_header, **additional_region_headers;
            int num_additional_region_headers = 
                LoopOpt_optimize_loop(cfg, instr, &new_region_entry,
                                      &new_loop_header, &additional_region_headers);
            if (num_additional_region_headers >= 0) {
                next_region_to_loop_optimize = i;
                instr = new_region_entry;
                Instr_SetLoopHeader(new_loop_header);
                /* Update region_headers to include additional region headers. */
                i = RegAlloc_update_region_headers(additional_region_headers, 
                                                   num_additional_region_headers,
                                                   new_loop_header, i);
                /* instr is a temporary region header which will be subsumed
                by the previous region. Set the new_loop_header to be 
                the i-th region header and increment i to repeat the new loop. */
            }
        }
        number_of_region_boundary = 0;

        if (Instr_GetH(instr) != NULL) {
            RegAlloc_backward_sweep_initial_tbl = 
                map_Clone_from_alloc_status(Instr_GetH(instr));
        } else {
            RegAlloc_backward_sweep_initial_tbl = NULL;
        }

        if (! flag_no_backward_sweep) {
            RegAlloc_backward_sweep(NULL, instr);
        }
        
#ifdef JP_STATISTICS
        /* find out how many coalescings are passed to the next region */
        {
            int k, l;
            AllocStat* h = Instr_GetH(instr);
            printf("A region:(%d):",num);
            Instr_Print(instr, outout, ':');
            for (k=0; k<CFG_GetNumOfVars(cfg); k++) {
                if ((l=h->refcount[k]) > 1) {
                    printf("%s(", Var_get_var_reg_name(k));
                    for (--l; l>=0; l--) {
                        printf("%s,",Var_get_var_reg_name(h->rmap[k][l]));
                    }
                    printf(")");
                }
            }
            printf("\n");
        }
#endif
        RegAlloc_forward_sweep_current_path_id = 0;
        h2 = AllocStat_Clone(Instr_GetH(instr));
	AllocStat_RemoveDeadVars(h2, instr);
        RegAlloc_forward_sweep(instr, AllocStat_Clone(Instr_GetH(instr)));
    }
    
    SMA_EndSession(&_session);


    RegAlloc_remove_region_dummys(cfg);
	
#ifdef JP_EXAMINE_SPILL
    if (num_spill>0) printf("num = %d, exception_num = %d, Spill = %d\n", 
                            num, exception_num, num_spill);
#endif
}



/* END allocator */




static
AllocStat *
RegAlloc_precolor_for_function_entry(InstrNode * root)
{
    int i;
    AllocStat * h = AllocStat_Init(AllocStat_alloc());
    InstrNode * succ = Instr_GetSucc(root, 0);

    assert(Instr_GetCode(root) == SPARC_SAVE);

    //
    // i6,i7,o6, and o7 are reserved - never used by allocator.
    //
    // SP - o6, ORET - o7
    // FP - i6, IRET - i7
    h->refcount[o6] = h->refcount[o7]
        = h->refcount[i6] = h->refcount[i7] = 1;
    // g0 is also reserved - allocator does not use g0.
    h->refcount[g0] = 1;
    
    /* floating point register */
    for (i = 0; i < CFG_GetNumOfParameters(cfg); i++) {
        int var = CFG_GetParameter(cfg, i);
        int r = Reg_get_argument(i);
        InstrNode * new_instr;

        // An argument is in a register.
        switch (Var_GetType(var)) {
          case T_INT: case T_REF: case T_LONG: case T_IVOID:
            if (Reg_IsHW(r)) {
                AllocStat_Insert(h, var, r);
            } else {
                if (i <= 13) {
                    // 6 <= i <= 13
                    new_instr =
                        create_format5_instruction(LD, var, fp,
                                                   RegAlloc_get_stack_offset_of_arg(i), 0);
                    CFG_InsertInstrAsSinglePred(cfg, succ, new_instr);
                } else {
                    // 14 <= i <= 18
                    new_instr =
                        create_format5_instruction(LD, var, fp,
                                                   RegAlloc_get_stack_offset_of_arg(i), 0);
                    CFG_InsertInstrAsSinglePred(cfg, succ, new_instr);
                }
            }
            break;
          case T_FLOAT: 
            if (Reg_IsHW(r)) {
                RegAlloc_convert_int_to_float(h, r, var, succ);
            } else {
                RegAllloc_load_float(RegAlloc_get_stack_offset_of_arg(i),
                                     var, succ);
            }
            break;
          case T_DOUBLE:
            if (r == i5) {
                {
                    RegAlloc_convert_int_to_float(h, r, f2, succ);
                    RegAllloc_load_float(RegAlloc_get_stack_offset_of_arg(i+1),
                                         f3, succ);
                    AllocStat_Insert(h, var, f2);
                }
            } else if (Reg_IsHW(r)) {
                RegAlloc_convert_long_to_double(h, r, var, succ);
            } else {
                /* since RegAlloc_get_stack_offset_of_arg returns the positive number,
                we do not touch the order of double register */ 
                /* check if the operands in stack are consecutive */
                assert(RegAlloc_get_stack_offset_of_arg(i) + 4
		       == RegAlloc_get_stack_offset_of_arg(i + 1));

                RegAlloc_load_double(RegAlloc_get_stack_offset_of_arg(i),
                                     var, succ);
            }

            /* This is enough. pair register's ref count is increased */
//            AllocStat_Insert(h, var, f_r);

            i++;
//            f_r += 2;   /* double align */
            break;
          default:
            assert(0);
        }
    }

    return h;
}

static
AllocStat*
RegAlloc_precolor_for_exception_handler_entry(CFG *cfg, TranslationInfo *info)
{
    int *gen_var_map;
    int caller_local_no;
    int caller_stack_no;
    int local_no;
    int local_offset;
    int stack_offset;
    int ret_reg;
    int r;
    int i;
    int ret_var_no;
    int ret_type;
    
    AllocStat* h = AllocStat_alloc();

    h = AllocStat_Init(h);
    h->map = (int *)gc_malloc_fixed(CFG_GetNumOfVars(cfg) * sizeof(int));
    for (i = 0; i < CFG_GetNumOfVars(cfg); i++)
	h->map[i] = -1;

    Var_prepare_creation(CFG_GetIGRoot(cfg));

    caller_local_no = TI_GetCallerLocalNO(info);
    caller_stack_no = TI_GetCallerStackNO(info);
    local_no = Method_GetLocalSize(IG_GetMethod(CFG_GetIGRoot(cfg)));

    local_offset = CFG_GetNumOfLocalVars(cfg);
    stack_offset = CFG_GetNumOfStackVars(cfg);
    
    gen_var_map = TI_GetGenMap(info);

    /* caller's local variables */
    for (i = 0; i < caller_local_no; i++) {
        Reg reg;
        Var var;
        VarType var_type;

        reg = gen_var_map[i];
        var_type = (reg & 0xf0000000) >> 28;
        reg &= 0x0fffffff;

        if (reg != 0x0fffffff) {
            var = Var_make(var_type, ST_LOCAL, i - caller_local_no);

            AllocStat_Insert(h, var, reg);
            
            if (var_type == T_DOUBLE) {
                i++;
            }
        }
    }

    /* local variables */
    for (i = 0; i < local_no; i++) {
	Reg reg;
	Var var;
	VarType var_type;
	
        reg = gen_var_map[i + caller_local_no];
	var_type = (reg & 0xf0000000) >> 28;
        reg &= 0x0fffffff;

        if (reg != 0x0fffffff) {
            var = Var_make(var_type, ST_LOCAL, i);

            AllocStat_Insert(h, var, reg);
            
            if (var_type == T_DOUBLE) {
                i++;
            }
        }
    }

    /* caller's stack variables */
    for (i = 0; i < caller_stack_no; i++) {
	Reg reg;
	Var var;
	VarType var_type;
	int stack_offset;
	
	stack_offset = caller_local_no + local_no;
        reg = gen_var_map[i + stack_offset];
        var_type = (reg & 0xf0000000) >> 28;
        reg &= 0x0fffffff;

        if (reg != 0x0fffffff) {
            var = Var_make(var_type, ST_STACK, i - caller_stack_no);
            
            AllocStat_Insert(h, var, reg);
            
            if (var_type == T_DOUBLE) {
                i++;
            }
        }
    }

    /* temporary variables */
    if (TI_GetReturnAddr(info) != 0) {
        for (i = 0; i < MAX_TEMP_VAR; i++) {
	    Reg reg;
	    Var var;
	    VarType var_type;
	    VarStorageType storage_type;
	    int var_no;
	    int temp_offset;

	    temp_offset = caller_local_no + local_no + caller_stack_no;
	    reg = gen_var_map[i + temp_offset];
	    var_type = (reg & 0xf0000000) >> 28;
	    storage_type = (i < 11) ? ST_TEMP : ST_VN;
	    var_no = (i < 11) ? i : i - 11;
            reg &= 0x0fffffff;

            if (reg != 0x0fffffff) {
                var = Var_make(var_type, storage_type, var_no);
            
                AllocStat_Insert(h, var, reg);
            
                if (var_type == T_DOUBLE) {
                    i++;
                }
            }
        }
    }

    // split coalesced local variables
    for (i = 0; i < local_no; i++) {
	int local_offset = caller_local_no;
	
        r = AllocStat_Find(h, i + local_offset);
        
        if (r != -1) {
            if (AllocStat_GetRefCount(h, r) > 1) {
                int j;
                int var = -1;
                int new_reg;

                for (j = 0; j < AllocStat_GetRefCount(h, r); j++) {
                    var = AllocStat_FindRMap(h, r, j);
                    if (CFG_get_var_number(var) == i) break;
                }

		assert(var != -1);
                
                new_reg = 
                    AllocStat_FindFreeNonCallerSaveReg(h, Var_GetType(var));
                if (new_reg == -1) {
                    new_reg = AllocStat_FindFreeSpill(h, Var_GetType(var));
                }
                
                AllocStat_Erase(h, var);
                AllocStat_Insert(h, var, new_reg);
            }
        }
    }

    if (TI_GetReturnStackTop(info) != -1) {
	int stack_offset;
	int *ret_map;

	ret_map = TI_GetRetMap(info);
	stack_offset = caller_local_no;
        ret_var_no = TI_GetReturnStackTop(info);
        ret_reg = ret_map[ret_var_no + stack_offset];
    } else {
        ret_reg = -1;
	ret_var_no = -1;
    }

    ret_type = (ret_reg & 0xf0000000) >> 28;
    ret_reg &= 0x0fffffff;

    if (ret_reg != 0x0fffffff) {
	int ret_offset;
	
	ret_offset = ret_var_no - caller_stack_no;
	
        exception_return_var[0] =
	    Var_make(ret_type, ST_STACK, ret_offset);

	exception_return_type = ret_type;

	if (ret_type == T_LONG)
	    exception_return_var[1] =
		Var_make(T_IVOID, ST_STACK, ret_offset + 1);
    } else {
	exception_return_type = -1;
    }

    

    //
    // i6,i7,o6, and o7 are reserved.
    //
    // SP - o6, ORET - o7
    // FP - i6, IRET - i7
    h->refcount[o6] = h->refcount[o7]
        = h->refcount[i6] = h->refcount[i7] = 1;

    TI_SetInitMap(info, (void *) h->map);

    return AllocStat_Clone(h);
}

void
RegAlloc_make_exception_return_map(TranslationInfo *info)
{
    int *ret_map;
    int caller_local_no;
    int caller_stack_no;
    int local_no;

    Reg reg;
    Var var;
    
    VarType var_type;
    VarStorageType storage_type;

    int i;

    ret_map = TI_GetRetMap(info);
    caller_local_no = TI_GetCallerLocalNO(info);
    caller_stack_no = TI_GetCallerStackNO(info);
    local_no = Method_GetLocalSize(TI_GetRootMethod(info));

    Var_prepare_creation(CFG_GetIGRoot(cfg));

    RegAlloc_Exception_Returning_Map = AllocStat_Init(AllocStat_alloc());
    RegAlloc_Exception_Caller_Local_NO = caller_local_no;
    RegAlloc_Exception_Caller_Stack_NO = caller_stack_no;

    for (i = 0; i < caller_local_no; i++) {
	int local_offset;

	local_offset = 0;
        reg = ret_map[i + local_offset];
        var_type = (reg & 0xf0000000) >> 28;
        reg &= 0x0fffffff;

        if (reg != 0x0fffffff) {
            var = Var_make(var_type, ST_LOCAL, i - caller_local_no);

            AllocStat_Insert(RegAlloc_Exception_Returning_Map, var, reg);
         
            if (var_type == T_DOUBLE) i++;
        }
    }

    for (i = 0; i < caller_stack_no; i++) {
	int stack_offset;
	
	stack_offset = caller_local_no;
        reg = ret_map[i + stack_offset];
        var_type = (reg & 0xf0000000) >> 28;
        reg &= 0x0fffffff;

        if (reg != 0x0fffffff) {
            var = Var_make(var_type, ST_STACK, i - caller_stack_no);

            AllocStat_Insert(RegAlloc_Exception_Returning_Map, var, reg);
            
            if (var_type == T_DOUBLE) i++;
        }
    }

    if (TI_GetReturnAddr(info) != 0) {
        for (i = 0; i < MAX_TEMP_VAR; i++) {
            int var_no;
	    int temp_offset;

	    temp_offset = caller_local_no + caller_stack_no;
            reg = ret_map[i + temp_offset];
            var_type = (reg & 0xf0000000) >> 28;
            reg &= 0x0fffffff;
            storage_type = (i < 11) ? ST_TEMP : ST_VN;
            var_no = (i < 11) ? i : i - 11;

            if (reg != 0x0fffffff) {
                var = Var_make(var_type, storage_type, var_no);

                AllocStat_Insert(RegAlloc_Exception_Returning_Map, var, reg);
            
                if (var_type == T_DOUBLE) i++;
            }
        }
    }
}

AllocStat *
RegAlloc_perform_precoloring(CFG *cfg, TranslationInfo *info)
{
    AllocStat *init_h;

    if (TI_IsFromException(info) == false) {
        init_h = RegAlloc_precolor_for_function_entry(CFG_GetRoot(cfg));
    } else {
        init_h = RegAlloc_precolor_for_exception_handler_entry(cfg, info);
    }

    return init_h;
}

static
void
RegAlloc_attach_live_info(CFG *cfg, InlineGraph *graph)
{
    InlineGraph *callee;

    if (IG_GetDepth(graph) != 0)
	LA_attach_live_info_for_inlining(cfg, graph);

    for (callee = IG_GetCallee(graph); callee != NULL;
         callee = IG_GetNextCallee(callee)) {
        RegAlloc_attach_live_info(cfg, callee);
    }
}


#ifdef JP_UNFAVORED_DEST
#include "bit_vector.h"
extern int number_of_words_for_variable;
#endif

inline
static
void
RegAlloc_initialize(int var_num)
{
#ifdef JP_UNFAVORED_DEST    
    /* number of words needed to make bit vectors for variables */
    number_of_words_for_variable = (var_num+31)/32;
#endif 
}

//
// translation phase3 merged version
//
void 
RegAlloc_allocate_registers_on_CFG(CFG *cfg, AllocStat *init_h)
{
    // attach live information to each inlining graph
    RegAlloc_attach_live_info(cfg, CFG_get_IG_root(cfg));

    // initialize allocation
    RegAlloc_initialize(CFG_GetNumOfVars(cfg));

    // for exception handling
    prepare_to_record_local_var_maps(cfg);

    // allocate registers
    RegAlloc_perform_fast_register_allocation(CFG_GetRoot(cfg), init_h);

    cfg->spillRegNum = CFG_GetNumOfSpillRegs(cfg);

    CFG_SetSizeOfMapPool(cfg, map_pool_index);
}
