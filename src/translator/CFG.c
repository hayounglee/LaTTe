/* CFG.c
   
   This is the implementation file for the control flow graph.
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties.
 */

#include "config.h"
#include "classMethod.h"
#include "exception_info.h"
#include "CFG.h"
#include "method_inlining.h"
#include "translate.h"

#define DBG( s )

#undef INLINE
#define INLINE
#include "CFG.ic"


//
// for LocalVariableInfo structure
//


///
/// Function Name : update_lv_info
/// Author : Yang, Byung-Sun
/// Input
///        list_header - linked list header of local variable informations
///        new_lv_info - new local variable information
/// Output
///        The local variable information with the same local variable entry
///        as 'new_lv_info' is searched and replaced with 'new_lv_info'.
/// Pre-Condition
///        In the linked list, a local variable information with the same entry
///        num is already inserted.
/// Post-Condition
///        The replaced element will be garbage collected.
///        Therefore not freed explicitly.
/// Description
///       This function should be used with a vector which records whether
///       a local variable information is already inserted in the list.
///
void
update_lv_info( LocalVariableInfo** list_header, LocalVariableInfo* new_lv_info )
{
    unsigned      entry_num = CFG_get_var_number( new_lv_info->localVariable );
    LocalVariableInfo*   previous = NULL;
    LocalVariableInfo*   iterator = *list_header;

    while (iterator != NULL) {
        if (entry_num == CFG_get_var_number( iterator->localVariable )) {
            if (previous != NULL) {
                new_lv_info->next = iterator->next;
                iterator->next = NULL;
                previous->next = new_lv_info;
            } else {
                new_lv_info->next = (*list_header)->next;
                (*list_header)->next = NULL;
                *list_header = new_lv_info;
            }
            return;
        }

        previous = iterator;
        iterator = iterator->next;
    }

    // If there is no the similar element, attach it at the end of list.
    if (previous == NULL) {
        *list_header = new_lv_info;
    } else {
        previous->next = new_lv_info;
    }
}

LocalVariableInfo*
find_lv_info( LocalVariableInfo* list_header, unsigned lv_num )
{
    LocalVariableInfo*   iterator = list_header;

    while (iterator != NULL) {
        if (lv_num == CFG_get_var_number( iterator->localVariable )) {
            return iterator;
        }

        iterator = iterator->next;
    }

    return NULL;
}





//
// CFG structure
//

///
/// Function Name : CFG_RegisterInstr
/// Author : Yang, Byung-Sun
/// Input
///      cfg - control flow graph
///      c_instr - instruction to register into cfg
/// Output
///      The number of nodes in the cfg is increased by 1
///      and the text size is increased also by 4.
///      The instruction is listed in 'instrList' of cfg.
/// Pre-Condition
///      When a new instruction is created, this function should be called.
/// Post-Condition
/// Description
///      This function is for cfg to remember the number of node it contains.
///      At normal case, one node corresponds one word after code generation.
///      But some special cases such as long addition in which one node corresponds
///      to two word after code generation, the text size should increased by 8.
///      To cope with these cases, user should use the function,
///      'CFG_ExpandTextSegByOneWord' explicitly.
///
void
CFG_RegisterInstr( CFG* cfg, InstrNode* c_instr )
{
    assert( c_instr != NULL );

    cfg->numOfNodes++;

    if (cfg->sizeOfList >= cfg->maxSizeOfList) {
        InstrNode ** t = 
            (InstrNode **)FMA_allocate(cfg->maxSizeOfList * 2 *
                                            sizeof(InstrNode *));

        memcpy(t, cfg->instrList, sizeof(InstrNode*) * cfg->maxSizeOfList);

        cfg->maxSizeOfList *= 2;
        cfg->instrList = t;
    };
    c_instr->arrayIndex = cfg->sizeOfList;
    cfg->instrList[cfg->sizeOfList++] = c_instr;
}


///
/// Function Name : CFG_RemoveInstr
/// Author : Yang, Byung-Sun
/// Input
///      cfg - control flow graph to be considered
///      c_instr - instruction to remove
/// Output
///      c_instr is removed from the control flow graph and each successor of c_instr
///      has now c_instr's predecessors as its predecessors.
/// Pre-Condition
///      The fast memory allocation system must be active.
/// Post-Condition
/// Description
///      This function is for the case that an instruction can be eliminated
///      as a result of optimization or other activities.
///      I think this function incurs some overheads.
///
void
CFG_RemoveInstr( CFG* cfg, InstrNode* c_instr )
{
    int          i;
    InstrNode*   pred;
    InstrNode*   succ;
    int          index;

    //
    // currently, I consider only an instruction which has one successor.
    // - doner, 97/11/24
    //

    assert( Instr_GetNumOfSuccs( c_instr ) == 1 );
    assert( Instr_GetNumOfPreds( c_instr ) != 0 );

    succ = Instr_GetSucc( c_instr, 0 );
    pred = Instr_GetPred( c_instr, 0 );
    assert( succ != c_instr && pred != c_instr);

    index = Instr_GetSuccIndex( pred, c_instr );
    Instr_SetSucc( pred, index, succ );

    Instr_SetPred( succ, Instr_GetPredIndex( succ, c_instr ), pred );


    // delete c_instr from the successor lists of its predecessors and
    // attach the successors of c_instr to the predecessors
    for (i = 1; i < c_instr->numOfPredecessors; i++) {
        pred = Instr_GetPred( c_instr, i );

        // 'succ' is now pred's successor instead of c_instr.
        index = Instr_GetSuccIndex( pred, c_instr );
        Instr_SetSucc( pred, index, succ );

        // 'pred' is now succ's predecessor instead of c_instr.
        Instr_AddPred( succ, pred );
    }

    //
    // If c_instr is a loop header, now succ becomes a new loop header.
    //
    if (Instr_IsLoopHeader( c_instr )) {
        Instr_SetLoopHeader( succ );
    }

    Instr_RemoveAllSuccs( c_instr );
    Instr_RemoveAllPreds( c_instr );

    if ( cfg ) CFG_DeregisterInstr( cfg, c_instr );
}


///
/// Function Name : CFG_RemoveBranch
/// Author : Seongbae Park
/// Input
///      cfg - control flow graph to be considered
///      c_instr - instruction to remove
/// Output
///      c_instr is removed from the control flow graph
///      And one successor(succ_index) of c_instr has now
///       c_instr's predecessors as its predecessors.
/// Pre-Condition
///      The fast memory allocation system must be active.
/// Post-Condition
/// Description
///      This function is for the case that an instruction can be eliminated
///      as a result of optimization or other activities.
///      I think this function incurs some overheads.  - ByungSun
///     Modified for branch elimination - Ramdrive
void
CFG_RemoveBranch( CFG* cfg, InstrNode* c_instr, int succ_index )
{
    int          i;
    InstrNode*   pred;
    InstrNode*   succ;
    int          index;

    //
    // currently, I consider only an instruction which has one successor.
    // - doner, 97/11/24
    // modified for removing branch instructions (with more than on successors)
    // - ramdrive, 98/11/4 

    assert( Instr_GetNumOfPreds( c_instr ) != 0 );

    succ = Instr_GetSucc( c_instr, succ_index );

    pred = Instr_GetPred( c_instr, 0 );

    index = Instr_GetSuccIndex( pred, c_instr );
    Instr_SetSucc( pred, index, succ );

    Instr_SetPred( succ, Instr_GetPredIndex( succ, c_instr ), pred );


    // delete c_instr from the successor lists of its predecessors and
    // attach the successors of c_instr to the predecessors
    for (i = 1; i < c_instr->numOfPredecessors; i++) {
        pred = Instr_GetPred( c_instr, i );

        // 'succ' is now pred's successor instead of c_instr.
        index = Instr_GetSuccIndex( pred, c_instr );
        Instr_SetSucc( pred, index, succ );

        // 'pred' is now succ's predecessor instead of c_instr.
        Instr_AddPred( succ, pred );
    }

    //
    // If c_instr is a loop header, now succ becomes a new loop header.
    //
    if (Instr_IsLoopHeader( c_instr )) {
        Instr_SetLoopHeader( succ );
    }
    Instr_RemoveAllPreds( c_instr );
}


///
/// Function Name : CFG_InsertInstrAsSucc
/// Author : Yang, Byung-Sun
/// Input
///      cfg - control flow graph
///      c_instr - current instruction
///      succ_index - index of successor
///      new_instr - new instruction to be c_instr's 'succ_index'th successor
/// Output
///      'new_instr' becomes the 'succ_index'th successor of 'c_instr'.
///      The old succesor becomes the 'new_instr's successor.
/// Pre-Condition
///      'c_instr' should have successors more than 'succ_index'
/// Post-Condition
/// Description
///
void
CFG_InsertInstrAsSucc( CFG*        cfg,
                                 InstrNode*  c_instr,
                                 int         succ_index,
                                 InstrNode*  new_instr )
{
    InstrNode*    old_succ;
    int           index;

    // replace the successor of c_instr with 'new_instr'
    old_succ = Instr_SetSucc( c_instr, succ_index, new_instr );
    Instr_AddPred( new_instr, c_instr );

    // get the predecessor index of c_instr at 'old_succ'
    index = Instr_GetPredIndex( old_succ, c_instr );
    assert( index >= 0 );

    // replace the index'th predecessor of 'old_succ' with 'new_instr'
    Instr_SetPred( old_succ, index, new_instr );
    Instr_AddSucc( new_instr, old_succ );

    CFG_RegisterInstr( cfg, new_instr );
}


///
/// Function Name : CFG_InsertInstrAsSinglePred
/// Author : Yang, Byung-Sun
/// Input
///       cfg - control flow graph
///       c_instr - current instruction node
///       new_instr - new instruction to be a single predecessor of 'c_instr'
/// Output
///       'new_instr' becomes a single predecessor of 'c_instr'.
///       The old predecessors of 'c_instr' are now predecessos of 'new_instr'.
/// Pre-Condition
/// Post-Condition
/// Description
///
InstrNode*
CFG_InsertInstrAsSinglePred( CFG*       cfg,
                                          InstrNode* c_instr,
                                          InstrNode* new_instr )
{
    InstrNode**    predecessors = c_instr->predecessors;
    int            succ_index;
    int            num_of_preds = Instr_GetNumOfPreds( c_instr );
    int            i;

    // The successor of predecessors of 'c_instr' now becomes 'new_instr'.
    for (i = 0; i < num_of_preds; i++) {
        succ_index = Instr_GetSuccIndex( predecessors[i], c_instr );
        assert( succ_index >= 0 );

        Instr_SetSucc( predecessors[i], succ_index, new_instr );
    }

    // 'new_instr' have predecessors of 'c_instr'.
    new_instr->predecessors = predecessors;
    new_instr->maxNumOfPredecessors = c_instr->maxNumOfPredecessors;
    new_instr->numOfPredecessors = c_instr->numOfPredecessors;

    // now 'c_instr' has a single predecessor, 'new_instr'.
    c_instr->predecessors = (InstrNode**) FMA_allocate( sizeof( InstrNode* ) * 1 );
    c_instr->maxNumOfPredecessors = 1;
    c_instr->numOfPredecessors = 1;
    c_instr->predecessors[0] = new_instr;

    if ( Instr_GetH( c_instr ) ) {
        Instr_SetH( new_instr, Instr_GetH( c_instr ) );
        Instr_SetH( c_instr, NULL );
    }

    Instr_AddSucc( new_instr, c_instr );

    CFG_RegisterInstr( cfg, new_instr );

    return new_instr;
}



//
// for control flow graph, CFG
//

///
/// Function Name : CFG_PrintInXvcg
/// Author : ramdrive
/// Input : Already opened FILE pointer and CFG pointer.
/// Output : No side effect. Print out CFG as xvcg format.
/// Pre-Condition : fp != NULL, cfg != NULL
/// Post-Condition : nothing
/// Description 
///     For debugging support.
///
void
CFG_PrintInXvcg(CFG* cfg, FILE* fp)
{
    int   i;
    InlineGraph *IG_root = CFG_get_IG_root( cfg );
    Method *method = IG_GetMethod( IG_root );

    assert(fp);
    assert(cfg);

    CFG_PrepareTraversal(cfg);

    fprintf(fp, "graph: {\n");
    fprintf(fp, "   layoutalgorithm: minbackward\n");
    fprintf(fp, "   finetuning: yes\n");

    
    //
    // print the main flow - doner
    //
    fprintf( fp, "    node: { title: \"mf\" label: \"Main Flow of %s:%s\n %s (%p)",
             Class_GetName( Method_GetDefiningClass( method ) )->data,
             Method_GetName( method )->data,
             Method_GetSignature( method )->data,
             method );
    fprintf(fp,"(");
    for(i = 0; i < CFG_GetNumOfParameters(cfg);i++) {
        fprintf(fp, " %s", Var_get_var_reg_name(CFG_GetParameter(cfg, i) ) );
    };

    fprintf(fp, ") local : %d stack : %d \"} ", CFG_GetNumOfLocalVars(cfg),
	    CFG_GetNumOfStackVars(cfg));
    fprintf( fp, "    edge: { thickness: 3"
             " sourcename: \"mf\" targetname: \"%p\" }\n", cfg->root );

    Instr_PrintInXvcg( cfg->root, fp );

    //
    // print finally blocks - doner
    //
    for (i = 0; i < CFG_GetNumOfFinallyBlocks( cfg ); i++) {
        FinallyBlockInfo*   fb_info = CFG_GetFinallyBlockInfo( cfg, i );

        fprintf( fp, "    node: {title: \"fb %d\" label: \"Finally Block %d\" }",
                 i, i );

        fprintf( fp, "    edge: {thickness: 3"
                 " sourcename: \"fb %d\" targetname: \"%p\" }\n",
                 i, FBI_GetHeader( fb_info ) );

	Instr_PrintInXvcg( FBI_GetHeader( fb_info ), fp );
    }


    fprintf(fp, "}\n");
};


///
/// Function Name : CFG_Print
/// Author : ramdrive
/// Input : Already opened FILE pointer and CFG pointer.
/// Output : No side effect. Print out CFG as ASCII format.
/// Pre-Condition : fp != NULL, cfg != NULL
/// Post-Condition : nothing
/// Description 
///     For debugging support.
///
static void _Instr_Print(CFG* cfg, InstrNode* instr, FILE* fp);
void
CFG_Print( CFG * cfg, FILE* fp )
{
    InlineGraph *IG_root = CFG_get_IG_root( cfg );
    Method *method = IG_GetMethod( IG_root );
    int i;

    assert(fp);
    assert(cfg);

    CFG_PrepareTraversal(cfg);

    fprintf( fp, "Main Flow of %s:%s\n %s (%p)",
             Class_GetName( Method_GetDefiningClass( method ) )->data,
             Method_GetName( method )->data,
             Method_GetSignature( method )->data,
             method );
    fprintf(fp,"(");
    for(i = 0; i < CFG_GetNumOfParameters(cfg);i++) {
        fprintf(fp, " %s", Var_get_var_reg_name(CFG_GetParameter(cfg, i) ) );
    };
    fprintf(fp,") \n" );

    _Instr_Print(cfg, cfg->root, fp);

    //
    // print finally blocks - doner
    //
    for (i = 0; i < CFG_GetNumOfFinallyBlocks( cfg ); i++) {
        FinallyBlockInfo*   fb_info = CFG_GetFinallyBlockInfo( cfg, i );

        fprintf( fp, "\nFinally Block %d\n", i );

        _Instr_Print( cfg, FBI_GetHeader( fb_info ), fp );
    }

    fprintf(fp, "\n");
}

static
void
_Instr_Print( CFG* cfg, InstrNode * instr, FILE* fp )
{
    int i;
    InstrNode* header;

    assert( instr );

    if ( Instr_IsVisited(instr) ) return;

    Instr_MarkVisited( instr );
    
    fprintf(fp, "\nL0x%08x:\n    ", (unsigned) instr);
    fprintf(fp, "[%8x] ", (unsigned) instr);
    Instr_Print(instr, fp, '\n');
    
    header = instr;
    /* scan the basic block */
    while (Instr_GetNumOfSuccs(instr) == 1&&
           Instr_GetNumOfPreds(Instr_GetSucc(instr,0)) == 1)
    {
        instr = Instr_GetSucc(instr, 0);
        fprintf(fp, "    ");
        fprintf(fp, "[%8x] ", (unsigned) instr);
        Instr_Print(instr, fp, '\n');
        Instr_MarkVisited( instr );
    }

    for(i = 0; i < Instr_GetNumOfSuccs(instr); i++) {
        assert( Instr_IsPred( Instr_GetSucc(instr, i), instr ) );
        fprintf(fp, "    succ(%d) : L0x%08x",
                i, (unsigned) Instr_GetSucc(instr, i));
        if ( i % 2 == 1 ) fprintf( fp, "\n" );
    };
    if ( i % 2 == 1 ) fprintf(fp, "\n");
    
    for(i = 0; i < Instr_GetNumOfSuccs(instr); i++) {
        assert( Instr_IsPred( Instr_GetSucc(instr, i), instr ) );
        _Instr_Print( cfg, Instr_GetSucc(instr, i), fp );
    };
};


///
/// Function Name : CFG_InsertStoreAsSucc
/// Author : Yang, Byung-Sun
/// Input
///        cfg - control flow graph
///        c_instr - current instruction after which the store instruction
///                  will be inserted
///        succ_index - successor index at which the store instruction will
///                     be inserted
///        from_reg - physical register to be spilled
///        to_reg - spill register
/// Output
///        A store instruction will be inserted as 'succ_index'th successor
///        of c_instr.
/// Pre-Condition
/// Post-Condition
/// Description
///        This function will be used mainly at phase 3 to make the register
///        allocation consisten at joins.
///
InstrNode*
CFG_InsertStoreAsSucc( CFG*        cfg,
                           InstrNode*  c_instr,
                           int         succ_index,
                           Reg    from_reg,
                           Reg    to_reg )
{
    InstrNode*    new_instr;

    new_instr = Instr_create_store( from_reg, to_reg );

    CFG_InsertInstrAsSucc( cfg, c_instr, succ_index, new_instr );

    return new_instr;
}


///
/// Function Name : CFG_InsertStoreAsSinglePred
/// Author : Yang, Byung-Sun
/// Input
///      cfg - control flow graph to be considered
///      c_instr - instruction before which 'store' operation will be inserted
///      from_reg - physical register to be spilled
///      to_reg - spill register which is converted to a stack entry
/// Output
///      A spill instruction will be generated and its pointer is returned.
/// Pre-Condition
///      to_reg's type should be ST_SPILL.
/// Post-Condition
/// Description
///      Now, the area for local variable spill when exception occurs, is also
///      spill area and the number of spill register is given considering local
///      variables.
///      So, 'store' instruction is as follows.
///           st from_reg, [fp - 8 - 4 * reg_num ]
///
InstrNode*
CFG_InsertStoreAsSinglePred( CFG*         cfg,
                                    InstrNode*   c_instr,
                                    Reg     from_reg,
                                    Reg     to_reg )
{
    InstrNode* new_instr;

    // create a store instruction
    new_instr = Instr_create_store( from_reg, to_reg );

    // insert new_instr before c_instr
    CFG_InsertInstrAsSinglePred( cfg, c_instr, new_instr );

    return new_instr;
}

///
/// Function Name : CFG_InsertLoadAsSucc
/// Author : Yang, Byung-Sun
/// Input
///      cfg - control flow graph
///      c_instr - current instruction after which load instruction will be inserted.
///      succ_index - successor index where load will be inserted.
///      from_reg - spill register
///      to_reg - physical register
/// Output
///      A despill instruction is generated after c_instr.
/// Pre-Condition
/// Post-Condition
/// Description
///
InstrNode*
CFG_InsertLoadAsSucc( CFG*        cfg,
                          InstrNode*  c_instr,
                          int         succ_index,
                          Reg    from_reg,
                          Reg    to_reg )
{
    InstrNode*    new_instr;

    new_instr = Instr_create_load( from_reg, to_reg );

    CFG_InsertInstrAsSucc( cfg, c_instr, succ_index, new_instr );

    return new_instr;
}


///
/// Function Name : CFG_InsertLoadAsSinglePred
/// Author : Yang, Byung-Sun
/// Input
///      cfg - control flow graph to be considered
///      c_instr - instruction before which 'store' operation will be inserted
///      from_reg - spill register to be loaded
///      to_reg - destination register
/// Output
///      A reload instruction for the previous spill will be generated and
///      its pointer is returned.
/// Pre-Condition
///      from_reg's type should be ST_SPILL.
///      to_reg should be a physical register.
/// Post-Condition
/// Description
///      At the stage 3 for register allocation, spill code can be generated.
///      This function is for reloading of spilled register.
///      The real reload instruction is 'load'.
///           ld  [fp-8-4*(num of local variables + reg_num)], to_reg
///      ========================>
///      As 'CFG_InsertStore', now the real reload instruction is
///           ld  [fp- 8 - 4 * reg_num], to_reg
///
InstrNode*
CFG_InsertLoadAsSinglePred( CFG*        cfg,
                                   InstrNode*  c_instr,
                                   Reg    from_reg,
                                   Reg    to_reg )
{
    InstrNode* new_instr;

    // create a load instruction
    new_instr = Instr_create_load( from_reg, to_reg );

    CFG_InsertInstrAsSinglePred( cfg, c_instr, new_instr );

    return new_instr;
}


///
/// Function Name : CFG_InsertCopyAsSucc
/// Author : Yang, Byung-Sun
/// Input
///      cfg - control flow graph
///      c_instr - current instruction after which a copy will be inserted
///      succ_index - successor index of the copy
///      from_reg - source physical register
///      to_reg - destination physical register
/// Output
///      A copy instruction is inserted as the succ_index'th succesor of c_instr.
/// Pre-Condition
/// Post-Condition
/// Description
///
InstrNode*
CFG_InsertCopyAsSucc( CFG*        cfg,
                          InstrNode*  c_instr,
                          int         succ_index,
                          Reg    from_reg,
                          Reg    to_reg )
{
    InstrNode*    new_instr;

    new_instr = Instr_create_copy( from_reg, to_reg );

    CFG_InsertInstrAsSucc( cfg, c_instr, succ_index, new_instr );

    return new_instr;
}

///
/// Function Name : CFG_InsertCopyAsSinglePred
/// Author : Yang, Byung-Sun
/// Input
///      cfg - control flow graph
///      c_instr - current_instruction after which a copy will be inserted
///      from_reg - physical source register
///      to_reg - physical destination register
/// Output
///      A copy instruction is inserted as a single predecessor of c_instr.
/// Pre-Condition
/// Post-Condition
/// Description
///
InstrNode*
CFG_InsertCopyAsSinglePred( CFG*        cfg,
                                   InstrNode*  c_instr,
                                   Reg    from_reg,
                                   Reg    to_reg )
{
    InstrNode*    new_instr;

    new_instr = Instr_create_copy( from_reg, to_reg );

    CFG_InsertInstrAsSinglePred( cfg, c_instr, new_instr );

    return new_instr;
}


///
/// Function Name : CFG_InsertCopy
/// Author : Yang, Byung-Sun
/// Input
///      cfg - control flow graph to be considered
///      p_instr - previous instruction
///      c_instr - current instruction
///      from_reg - source register
///      to_reg - destination register
/// Output
///      A new copy instruction is inserted between p_instr and c_instr
///      and its pointer is returned.
/// Pre-Condition
///      Both 'from_reg' and 'to_reg' should be physical registers.
/// Post-Condition
/// Description
///      'add  from_reg, %g0, to_reg', or 'fmov from_reg, to_reg' is inserted
///      before c_instr.
///
InstrNode*
CFG_InsertCopy( CFG*        cfg,
                         InstrNode*  p_instr,
                         InstrNode*  c_instr,
                         Reg    from_reg,
                         Reg    to_reg )
{
    assert( Reg_IsHW( from_reg ) &&
            Reg_IsHW( to_reg ) );

    assert( Instr_IsPred( c_instr, p_instr ) );

    return CFG_InsertCopyAsSucc( cfg,
                                     p_instr,
                                     Instr_GetSuccIndex( p_instr, c_instr ),
                                     from_reg, to_reg );
}

///
/// Function Name : CFG_InsertGeneralCopyAsSucc
/// Author : Yang, Byung-Sun
/// Input
///       cfg - control flow graph
///       c_instr - current instruction after which a copy will be inserted
///       succ_index - successor index
///       from_reg - source register
///       to_reg - destination register
/// Output
///       An appropriate copy instruction will be generated according to the
///       types of registers.
/// Pre-Condition
///       This function is also used at the stage 3.
/// Post-Condition
/// Description
///       from_reg:physical && to_reg:physical ===> copy from_reg, to_reg
///       from_reg:physical && to_reg:spill    ===> store from_reg, to_reg
///       from_reg:spill    && to_reg:physical ===> load  from_reg, to_reg
///       from_reg:spill    && to_reg:spill    ===> load  from_reg, g1
///                                                 store g1, to_reg
///       At the 4th case, pointer to the store instruction is returned.
///
InstrNode*
CFG_InsertGeneralCopyAsSucc( CFG*        cfg,
                                  InstrNode*  c_instr,
                                  int         succ_index,
                                  Reg    from_reg,
                                  Reg    to_reg )
{
    if (Reg_IsHW( from_reg )) {
        if (Reg_IsHW( to_reg )) {
            return CFG_InsertCopyAsSucc( cfg, c_instr, succ_index,
                                             from_reg, to_reg );
        } else {
            return CFG_InsertStoreAsSucc( cfg, c_instr, succ_index,
                                              from_reg, to_reg );
        }
    } else {
        if (Reg_IsHW( to_reg )) {
            return CFG_InsertLoadAsSucc( cfg, c_instr, succ_index,
                                             from_reg, to_reg );
        } else {
            InstrNode*    new_instr;

            new_instr = CFG_InsertLoadAsSucc( cfg, c_instr, succ_index,
                                                  from_reg, g1 );
            return CFG_InsertStoreAsSucc( cfg, new_instr, 0, g1, to_reg );
        }
    }
}

///
/// Function Name : CFG_InsertGeneralCopyAsSinglePred
/// Author : Yang, Byung-Sun
/// Input
///       cfg - control flow graph
///       c_instr - current instruction before which a copy will be inserted
///       from_reg - source register
///       to_reg - destination register
/// Output
///       An appropriate copy instruction will be generated.
/// Pre-Condition
/// Post-Condition
/// Description
///
InstrNode*
CFG_InsertGeneralCopyAsSinglePred( CFG*        cfg,
                                           InstrNode*  c_instr,
                                           Reg    from_reg,
                                           Reg    to_reg )
{
    if (Reg_IsHW( from_reg )) {
        if (Reg_IsHW( to_reg )) {
            return CFG_InsertCopyAsSinglePred( cfg, c_instr,
                                                      from_reg, to_reg );
        } else {
            return CFG_InsertStoreAsSinglePred( cfg, c_instr,
                                                       from_reg, to_reg );
        }
    } else {
        if (Reg_IsHW( to_reg )) {
            return CFG_InsertLoadAsSinglePred( cfg, c_instr,
                                                      from_reg, to_reg );
        } else {
            CFG_InsertLoadAsSinglePred( cfg, c_instr,
                                               from_reg, g1 );
            return CFG_InsertStoreAsSinglePred( cfg, c_instr,
                                                       g1, to_reg );
        }
    }
}

InstrNode*
CFG_InsertGeneralCopy( CFG*        cfg,
                                 InstrNode*  p_instr,
                                 InstrNode*  c_instr,
                                 Reg    from_reg,
                                 Reg    to_reg )
{
    assert( Instr_IsPred( c_instr, p_instr ) );

    return CFG_InsertGeneralCopyAsSucc( cfg,
                                             p_instr,
                                             Instr_GetSuccIndex( p_instr, c_instr ),
                                             from_reg,
                                             to_reg );
}


///
/// Function Name : CFG_RegisterFinallyBlock
/// Author : Yang, Byung-Sun
/// Input
///       cfg - control flow graph data structure
///       bytecode_pc - bytecode pc of the entry of the finally block
/// Output
///       When the finally block is first found, it is registered into 'cfg'.
/// Pre-Condition
/// Post-Condition
/// Description
///       This function is used in the stage 1 of the translation.
///       When a 'jsr' is found, its destination is a finally block entry.
///       The finally block should be recorded in CFG structure.
///       When the finally block is alread recorded in CFG, nothing occurs.
///
#define DEFAULT_FINALLY_BLOCK_NUM   5

void
CFG_RegisterFinallyBlock( CFG* cfg, InstrOffset header_pc, 
			  InstrNode*  gosub, int operand_stack_top )
{
    if (cfg->numOfFinallyBlocks == 0) {
        cfg->finallyBlocks =
            (FinallyBlockInfo*) FMA_allocate( sizeof( FinallyBlockInfo ) *
                                                   DEFAULT_FINALLY_BLOCK_NUM );
        cfg->maxNumOfFinallyBlocks = DEFAULT_FINALLY_BLOCK_NUM;
    } else if (cfg->numOfFinallyBlocks == cfg->maxNumOfFinallyBlocks) {
        FinallyBlockInfo*  new_info =
            (FinallyBlockInfo*) FMA_allocate( sizeof( FinallyBlockInfo ) *
                                                   cfg->maxNumOfFinallyBlocks * 2 );

        memcpy( new_info, cfg->finallyBlocks,
                sizeof( FinallyBlockInfo ) * cfg->numOfFinallyBlocks );
        cfg->finallyBlocks = new_info;
        cfg->maxNumOfFinallyBlocks *= 2;
    }

    cfg->finallyBlocks[cfg->numOfFinallyBlocks].headerPC = header_pc;
    cfg->finallyBlocks[cfg->numOfFinallyBlocks].gosub = gosub;
    cfg->finallyBlocks[cfg->numOfFinallyBlocks++].operandStackTop = operand_stack_top;
}

void
CFG_ConnectFinallyBlock( CFG*  cfg, int index )
{
    FinallyBlockInfo *fbi = &cfg->finallyBlocks[index];

    InstrNode *gosub = fbi->gosub;
    InstrNode *succ = Instr_GetSucc( gosub, 0 );

    assert( index <= cfg->numOfFinallyBlocks );
    assert( Instr_GetCode( gosub ) == GOSUB );
    assert( Instr_GetNumOfSuccs( gosub ) == 1 );

    
    Instr_disconnect_instruction( gosub, succ );

    Instr_connect_instruction( gosub, fbi->header );

    // In jck test suites, there is a case when ret does not exist for jsr.
    if ( fbi->lastInstr != NULL )
      Instr_connect_instruction( fbi->lastInstr, succ );

    CFG_RemoveInstr( cfg, gosub );
    
}


FinallyBlockInfo*
CFG_FindFinallyBlockInfo( CFG* cfg, InstrOffset header_pc )
{
    int    i;

    for (i = 0; i < cfg->numOfFinallyBlocks; i++) {
        if (cfg->finallyBlocks[i].jsrPC == header_pc) {
            return &cfg->finallyBlocks[i];
        }
    }

    return NULL;
}



//
// for exception handling
//

inline static
bool
check_exception_handler(InlineGraph *graph, int bpc) {
    /* reached root method; no method has exception handlers */
    if (graph == NULL) return false;
    else {
        Method *method = IG_GetMethod(graph);
        jexception *etable = Method_GetExceptionTable(method);
        int8 *binfo = Method_GetBcodeInfo(method);

        /* method is in call chain generated from try block */
        if (etable != NULL && (binfo[bpc] & METHOD_WITHIN_TRY)) return true;
        else {
            /* no method is in call chain of try block so far,
               now check callers */
            return check_exception_handler(IG_GetCaller(graph),
                                           IG_GetBPC(graph));
        }
    }
}

inline
void
CFG_MarkExceptionGeneratableInstr( CFG* cfg, InstrNode* c_instr )
{
    /* FIXME : global variable, which should be set before calling
       this function. Generally, this variable is set before going
       into cfg_generate function of every method */
    extern InlineGraph *CFGGen_current_ig_node;
    InlineGraph *graph = CFGGen_current_ig_node; /* just for good look */
    Method      *method;
    jexception  *etable;
    int         bpc;
    /* maximum possible size is set. now, actual runtime memory is set
       accurately after filling variable maps */
/*      int         size = CFG_GetNumOfVars( cfg ); */
    int8        *binfo;

    /* every potential exception generating instruction should have
       appropriate bytecode pc value for recording variable maps */
    bpc = Instr_GetBytecodePC(c_instr);
    method = IG_GetMethod(graph);
    etable = Method_GetExceptionTable(method);
    binfo = Method_GetBcodeInfo(method);

    /* set flag of instruction node */
    Instr_SetExceptionGeneratable(c_instr); 

    /* let which method owns this instruction be known. */
    Instr_SetInlineGraph(c_instr, graph); 

    /* if this is the last instruction of inlined method and one of
       callers are within call chain of try block, caller variable map
       should be recorded */
    if (Instr_IsMethodEnd(c_instr)) {
        Instr_SetNeedMapInfo(c_instr);
        CFG_IncreaseNumOfMapNeedInstrs(cfg);
#if 0        
	if (check_exception_handler(graph, bpc)) {
	    Instr_SetNeedMapInfo(c_instr);
	    CFG_IncreaseNumOfMapNeedInstrs(cfg);
	    return;
	} else {
            Instr_SetNeedMapInfo(c_instr);
            CFG_IncreaseNumOfMapNeedInstrs(cfg);
            return;
        }
#endif 0        
    }

    if (etable != NULL && (binfo[bpc] & METHOD_WITHIN_TRY)) {
        /* current method has try block */
        if (Instr_IsMemAccess(c_instr)) {
            /* for memory access instruction, variable maps are stored
               only if it can be caught by one of exception handlers */
            if (Method_HaveNullException(method)) {
                Instr_SetNeedMapInfo(c_instr);
                CFG_IncreaseNumOfMapNeedInstrs(cfg);
            }
        } else {
            /* non-memory access instruction */
            Instr_SetNeedMapInfo(c_instr);
            CFG_IncreaseNumOfMapNeedInstrs(cfg);
        }
    } else if (Method_IsSynchronized(method) && !Method_IsStatic(method)) {
        /* for non-static synchronized method should preserve variable
           map of local 0 */
        Instr_SetNeedLocal0MapInfo(c_instr);
        CFG_IncreaseNumOfMapNeedInstrs(cfg);
    } else if (check_exception_handler(graph, bpc)) {
        /* one of caller methods has exception handlers */
        Instr_SetNeedMapInfo(c_instr);
        CFG_IncreaseNumOfMapNeedInstrs(cfg);
    }
}

static int local_var_no;
static int stack_var_no;

void
_set_variable_offsets(InlineGraph *graph, int local_offset, int stack_offset)
{
    InlineGraph *callee;
    Method *method;

    int local_size;
    int stack_size;
    int depth;

    assert(graph);

    method = IG_GetMethod(graph);
    depth = IG_GetDepth(graph);

    local_size = Method_GetLocalSize(method);
    stack_size = Method_GetStackSize(method);

    if (local_var_no < local_size + local_offset) {
        local_var_no = local_size + local_offset;
    }

    if (stack_var_no < stack_size + stack_offset) {
        stack_var_no = stack_size + stack_offset;
    }

    for(callee = IG_GetCallee(graph); callee != NULL;
	callee = IG_GetNextCallee(callee)) {
        _set_variable_offsets(callee,
			      local_offset + local_size,
			      stack_offset + stack_size);
    }
}

/* Name        : CFG_set_variable_offsets
   Description : set local variables and stack variables numbers
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
CFG_set_variable_offsets(CFG *cfg, TranslationInfo *info) {
    InlineGraph *graph = CFG_GetIGRoot(cfg);

    if (info != NULL) {
        local_var_no = TI_GetCallerLocalNO(info);
        stack_var_no = TI_GetCallerStackNO(info);
    } else {
	local_var_no = 0;
	stack_var_no = 0;
    }

    _set_variable_offsets(graph, local_var_no, stack_var_no);

    if (info != NULL) {
        TI_SetLocalVarNO(info, local_var_no);
        TI_SetStackVarNO(info, stack_var_no);
    }

    CFG_SetNumOfLocalVars(cfg, local_var_no);
    CFG_SetNumOfStackVars(cfg, stack_var_no);
    CFG_SetNumOfTempVars(cfg, MAX_TEMP_VAR);
}

#undef DBG

