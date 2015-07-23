/* CFG.h
   
   The main data structure for the translation, "CFG", is defined
   and some other data structures used in CFG are also defined.
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties.
 */

#ifndef __CFG_H__
#define __CFG_H__

#include "InstrNode.h"
#include "reg.h"
#include "utils.h"
#include "basic.h"
#include "classMethod.h"

struct TranslationInfo;
//
// data structure for instruction resolution at the stage 4
//
// Instructions which should be resolved at the stage 4 are:
//         call
//         branch instructions
//         memory operations
//         spilled operations
//
typedef struct InstrToResolve {
    struct InstrToResolve*   next; // link to next entry
    InstrNode*               instr; // instruction to resolve
    int                      instrCode; // instruction code for efficiency

// for branch instrucitons, dataToResolve is not needed actually.
// This information from 'instr's second successor.

    void*                   dataToResolve;
//      union {
//	    int*       funcAddr; // for 'call'
//	    InstrNode* branchDest; // for branch instructions
//	    int        dataOffset; // for memory operations/'sethi'/'or'
//	    int        stackOffset; // for spilled operations/'save'
//      } dataToResolve;
} InstrToResolve;

InstrToResolve*   create_resolve_instr_entry( InstrNode*  instr,
                                              void*       resolve_data );
InstrNode*        get_resolve_instr( InstrToResolve* instr_entry );
void*             get_resolve_data( InstrToResolve* instr_entry );
int               get_resolve_instr_name( InstrToResolve* instr_entry );


//
// data structure for data segment initialization at the stage 4
//
typedef enum DataType {
    NORMAL,			// normal data
    JUMP_TABLE,			// jump table
    LOOKUP_TABLE			// lookup table
} DataType;

typedef struct DataEntry {
    struct DataEntry*   next;

    DataType            type;	// data type
    int                 offset; // offset from the start of data segment
    int                 size;	// data size in bytes

//      union {
//	    int*          initialDatas;	// data to initialize
//	    InstrNode**   destInstrs; // for jump tables
//      } initialVaule;
    void*               initialValue;
} DataEntry;

DataEntry*   create_data_entry( DataType type, int offset, int size, void* initial_value );
DataType     get_data_type( DataEntry* data_entry );
int          get_data_offset( DataEntry* data_entry );
int          get_data_size( DataEntry* data_entry );
void*        get_initial_value( DataEntry* data_entry );



//
// LocalVariableInfo structure
//    - to remember local variable usage and mapping
//
typedef struct LocalVariableInfo {
    struct LocalVariableInfo*   next;
    Reg                     localVariable;
    Reg                     allocatedRegister; // physical/spill register
} LocalVariableInfo;

void    insert_lv_info( LocalVariableInfo** list_header,
                        LocalVariableInfo* new_lv_info );
void    update_lv_info( LocalVariableInfo** list_header,
                        LocalVariableInfo* new_lv_info );
LocalVariableInfo*   find_lv_info( LocalVariableInfo* list_header, unsigned lv_num );

void       LVI_init( LocalVariableInfo* lv_info );
void       LVI_set_lv_register( LocalVariableInfo* lv_info, Reg lv_reg );
Reg   LVI_get_lv_register( LocalVariableInfo* lv_info );
void       LVI_set_physical_register( LocalVariableInfo* lv_info, Reg phy_reg );
Reg   LVI_get_physical_register( LocalVariableInfo* lv_info );


//
// FinallyBlockInfo structure
//   - for finally blocks in a method
//
typedef struct FinallyBlockInfo {
    InstrOffset     headerPC;	// bytecode pc of entry instruction of finally block
    InstrNode*      header;	// instruction node of entry of finally block

    InstrNode*      lastInstr; // last instruction node of finally block

    //
    // list of pairs of GOSUB(jsr) instrunction node and the original instruction
    // lexically next to GOSUB(jsr)
    //
    // This list will be contructed at the stage 2 and used at the stage 3
    // At the start of stage 3, a finally block will be connected to GOSUB instructions
    // and the original successor of GOSUB will be disconnected.
    // Reg allocation works with this modified version of control flow graph.
    // The register allocation will be consistent on the boundary between main flow
    // and finally block.
    // At the end of stage 3, the original control flow graph will be restored.
    //
    ilist*              GOSUB_List;
    InstrOffset jsrPC;
    InstrNode*  gosub;
    //
    // for the translate stage 2
    // When the translation for this finally block,
    // the operand stack top when this finally block is called should be known.
    //
    int             operandStackTop;
} FinallyBlockInfo;

void          FBI_init( FinallyBlockInfo* fb_info );

void          FBI_SetHeader( FinallyBlockInfo* fb_info, InstrNode* header );
InstrNode*    FBI_GetHeader( FinallyBlockInfo* fb_info );
void          FBI_set_operand_stack_top_at_start( FinallyBlockInfo* fb_info,
                                                  int               operand_stack_top );
int           FBI_get_operand_stack_top_at_start( FinallyBlockInfo* fb_info );
InstrOffset   FBI_get_first_pc( FinallyBlockInfo* fb_info );


//
// for the stage 3
//
void          FBI_add_GOSUB( FinallyBlockInfo* fb_info, InstrNode* gosub );
ilist*        FBI_get_GOSUB_list_iterator( FinallyBlockInfo* fb_info );
void          FBI_set_last_FB_instruction( FinallyBlockInfo* fb_info,
                                           InstrNode* last_instr );
InstrNode*    FBI_get_last_FB_instruction( FinallyBlockInfo* fb_info );


						  

//
// traslated intermediate and native instruction control flow graph structure
//
struct CFG {
    struct InlineGraph *graphRoot;

    //
    // This information is set while generating the function prologue code.
    //
    int                    numOfParams; // number of input parameters which is <= 6
    // When > 6, explicit load instructions are
    // generated at the head of the function.
    // This can be more refined.
    int*                   paramVars;// pseudo register used as input parameters

    ReturnType             retType; // return type

    // information for finally blocks in a method
    int                    maxNumOfFinallyBlocks;
    int                    numOfFinallyBlocks;
    FinallyBlockInfo*      finallyBlocks;

    // information for an exception handler translation
//      int*                   localVarTypes;

    // stuffs related to stack frame
    int          maxNumOfArgs;// maximum number of arguments passed from the method
    // This value can be determined after stage 2
    // CHANGED!!!
    // this value varies during phase3.
    int          spillRegNum;	// spill register number generated on register allocation
    int          numberOfLocalVariables;
    int          numberOfStackVariables;
    int          numberOfTemporaryVariables;

    int          stackSize;     // native stack size
    int          numOfNodes;	// number of nodes in the whole control flow graph

    InstrNode*   root;	// for main flow


    // for the stage 4
    int          textSize;	// text segment size in bytes
    // The value can be bigger than the real size.
    int          dataSize;	// data segment size in bytes


    InstrToResolve*   resolveInstrList;
    DataEntry*        dataList;

    // list of instructions
    int               maxSizeOfList;
    int               sizeOfList;
    InstrNode**       instrList;


    //
    // for exception handling
    //
    int               numOfMapNeedInstrs;// number of instructions that need local
    int               sizeOfMapPool;
};

typedef struct CFG CFG;

CFG*         CFG_Init( struct InlineGraph *IG_root );

int          CFG_GetNumOfParameters( CFG* cfg );
int          CFG_GetParameter( CFG* cfg, int index );
ReturnType   CFG_GetRetType( CFG* cfg );


bool         CFG_IsBBStart( CFG* cfg, InstrNode* c_instr );
bool         CFG_IsBBEnd( CFG* cfg, InstrNode* c_instr );

int          CFG_GetNumOfLocalVars(CFG *cfg);
void         CFG_SetNumOfLocalVars(CFG *cfg, int);
int          CFG_GetNumOfTempVars(CFG *cfg);
void         CFG_SetNumOfTempVars(CFG *cfg, int );

int          CFG_GetNumOfStorageLocations(CFG* cfg);
int          CFG_GetNumOfSpillRegs(CFG * cfg);

int          CFG_GetNumOfVars(CFG* cfg);

void         CFG_RegisterInstr( CFG* cfg, InstrNode* c_instr );
void         CFG_DeregisterInstr( CFG* cfg, InstrNode* c_instr );

void         CFG_ExpandTextSegByOneWord( CFG* cfg );

int          CFG_GetNumOfInstrs( CFG* cfg );
InstrNode*   CFG_GetRoot( CFG* cfg );
void         CFG_SetRoot( CFG* cfg, InstrNode* instr );

void*        CFG_GetTextSeg( CFG* cfg );
int          CFG_GetTextSegSize( CFG* cfg );
void         CFG_SetTextSegSize( CFG* cfg , int size);
int          CFG_GetDataSegSize( CFG* cfg );
int          CFG_GetStackSize( CFG* cfg );
void         CFG_SetStackSize( CFG* cfg, int size );
void         add_new_resolve_instr( CFG* cfg, InstrNode* c_instr, void* resolve_data );
InstrToResolve*   get_resolve_instr_list_iterator( CFG* cfg );


// This function sets the visited fields of instructions to false.
void         CFG_PrepareTraversal( CFG* cfg );

#ifdef TYPE_ANALYSIS
void         CFG_NullifyMV2TV( CFG* cfg );
#endif

// wrapper functions for instruction creation methods
InstrNode*   register_format1_instruction( CFG*        cfg,
                                           InstrNode*  p_instr,
                                           InstrOffset bytecode_pc,
                                           int         instr_code,
                                           int         disp30 );
InstrNode*   register_format2_instruction( CFG*        cfg,
                                           InstrNode*  p_instr,
                                           InstrOffset bytecode_pc,
                                           int         instr_code,
                                           Reg     dest_reg,
                                           int         imm22 );
InstrNode*   register_format3_instruction( CFG*        cfg,
                                           InstrNode*  p_instr,
                                           InstrOffset bytecode_pc,
                                           int         instr_code );
InstrNode*   register_format4_instruction( CFG*        cfg,
                                           InstrNode*  p_instr,
                                           InstrOffset bytecode_pc,
                                           int         instr_code,
                                           Reg     dest_reg,
                                           Reg     src_reg_1,
                                           Reg     src_reg_2 );
InstrNode*   register_format5_instruction( CFG*        cfg,
                                           InstrNode*  p_instr,
                                           InstrOffset bytecode_pc,
                                           int         instr_code,
                                           Reg     dest_reg,
                                           Reg     src_reg,
                                           short       simm13 );
InstrNode*   register_format6_instruction( CFG*        cfg,
                                           InstrNode*  p_instr,
                                           InstrOffset bytecode_pc,
                                           int         instr_code,
                                           Reg     dest_reg,
                                           Reg     src_reg_1,
                                           Reg     src_reg_2 );
InstrNode*   register_format7_instruction( CFG*        cfg,
                                           InstrNode*  p_instr,
                                           InstrOffset bytecode_pc,
                                           int         instr_code,
                                           Reg    src_reg_3,
                                           Reg    src_reg_1,
                                           Reg    src_reg_2 );
InstrNode*   register_format8_instruction( CFG*        cfg,
                                           InstrNode*  p_instr,
                                           InstrOffset bytecode_pc,
                                           int         instr_code,
                                           Reg    src_reg_2,
                                           Reg    src_reg_1,
                                           short       simm13 );
InstrNode*   register_format9_instruction( CFG*        cfg,
                                           InstrNode*  p_instr,
                                           InstrOffset bytecode_pc,
                                           int         instr_code,
                                           short       intrp_num );




InstrNode*   register_nop_instruction( CFG*        cfg,
                                       InstrNode*  p_instr,
                                       InstrOffset bytecode_pc );


//
// insert/remove instruction methods
//
void         CFG_InsertInstrAsSucc( CFG*        cfg,
				    InstrNode*  c_instr,
				    int         succ_index,
				    InstrNode*  new_instr );

InstrNode*   CFG_InsertInstrAsSinglePred( CFG*        cfg,
					  InstrNode*  c_instr,
					  InstrNode*  new_instr );


//
// for use at stage 3
//
InstrNode*   CFG_InsertStoreAsSucc( CFG*        cfg,
                                        InstrNode*  c_instr,
                                        int         succ_index,
                                        Reg    from_reg,
                                        Reg    to_reg );
InstrNode*   CFG_InsertStoreAsSinglePred( CFG*        cfg,
                                                 InstrNode*  c_instr,
                                                 Reg    from_reg,
                                                 Reg    to_reg );
InstrNode*   CFG_InsertStore( CFG*        cfg,
			      InstrNode*  p_instr,
			      InstrNode*  c_instr,
			      Reg    from_reg,
			      Reg    to_reg );


InstrNode*   CFG_InsertLoadAsSucc( CFG*        cfg,
				   InstrNode*  c_instr,
				   int         succ_index,
				   Reg    from_reg,
				   Reg    to_reg );
InstrNode*   CFG_InsertLoadAsSinglePred( CFG*        cfg,
					 InstrNode*  c_instr,
					 Reg    from_reg,
					 Reg    to_reg );
InstrNode*   CFG_InsertLoad( CFG*        cfg,
			     InstrNode*  p_instr,
			     InstrNode*  c_instr,
			     Reg    from_reg,
			     Reg    to_reg );


InstrNode*   CFG_InsertCopyAsSucc( CFG*        cfg,
				   InstrNode*  c_instr,
				   int         succ_index,
				   Reg    from_reg,
				   Reg    to_reg );
InstrNode*   CFG_InsertCopyAsSinglePred( CFG*        cfg,
					 InstrNode*  c_instr,
					 Reg    from_reg,
					 Reg    to_reg );
InstrNode*   CFG_InsertCopy( CFG*        cfg,
			     InstrNode*  p_instr,
			     InstrNode*  c_instr,
			     Reg    from_reg,
			     Reg    to_reg );

InstrNode*   CFG_InsertGeneralCopyAsSucc( CFG*        cfg,
					  InstrNode*  c_instr,
					  int         succ_index,
					  Reg    from_reg,
					  Reg    to_reg );
InstrNode*   CFG_InsertGeneralCopyAsSinglePred( CFG*        cfg,
						InstrNode*  c_instr,
						Reg    from_reg,
						Reg    to_reg );
InstrNode*   CFG_InsertGeneralCopy( CFG*        cfg,
				    InstrNode*  p_instr,
				    InstrNode*  c_instr,
				    Reg    from_reg,
				    Reg    to_reg );



// mainly for copy elimination
void         CFG_RemoveInstr( CFG* cfg, InstrNode* c_instr );
void         CFG_RemoveBranch( CFG* cfg, InstrNode *c_instr, int succ_index );


// for finally blocks
void                   CFG_InitFinallyBlocks( CFG* cfg );
void                   CFG_RegisterFinallyBlock( CFG*        cfg,
						 InstrOffset bytecode_pc,
						 InstrNode*  gosub,
						 int         operand_stack_top );
FinallyBlockInfo*      CFG_GetFinallyBlockInfo( CFG* cfg, int index );
FinallyBlockInfo*      CFG_FindFinallyBlockInfo( CFG* cfg, InstrOffset header_pc );
int                    CFG_GetNumOfFinallyBlocks( CFG* cfg );
void                   CFG_ConnectFinallyBlock(CFG* cfg, int index);



// for the calculation of the stack frame size
void         CFG_UpdateNumOfMaxArgs( CFG* cfg, int arg_num );


//
// for exception handling
//
void         CFG_MarkExceptionGeneratableInstr( CFG* cfg, InstrNode* c_instr );
int          CFG_GetNumOfMapNeedInstrs( CFG* cfg );
int          CFG_GetSizeOfMapPool( CFG* cfg );
void         CFG_SetSizeOfMapPool( CFG* cfg, int size );
void         CFG_IncreaseNumOfMapNeedInstrs( CFG *cfg );
void         CFG_ExpandMapPool( CFG *cfg, int size );

// for debugging
void         CFG_PrintInXvcg( CFG* cfg, FILE* file );
void         CFG_Print( CFG * cfg, FILE* file );

// align specifier
#define ALIGN_WORD        4
#define ALIGN_DOUBLE_WORD 8

int          CFG_AddNewDataEntry( CFG*     cfg,
                                 DataType type,
                                 int      size,
                                 void*    initial_value,
                                 int      alignment);
DataEntry*   CFG_GetDataListIterator( CFG* cfg );

struct InlineGraph *CFG_GetIGRoot(CFG *cfg);

int CFG_get_var_number(Var var);

void CFG_set_variable_offsets(CFG *cfg, struct TranslationInfo *info);

/* registering instruction is simplified by definitions 
   Pre-condition :
       cfg should have appropriate CFG structure pointer
       c_instr should be predessor of created instruction.
       pc should have bytecode pc value.
   Post-condition :
       c_instr will have newly created instruction.
*/
#define APPEND_INSTR1(opcode, disp30) \
    c_instr = register_format1_instruction(cfg, c_instr, pc, opcode, disp30)
#define APPEND_INSTR2(opcode, dr, imm22) \
    c_instr = register_format2_instruction(cfg, c_instr, pc, opcode, \
                                           dr, imm22)
#define APPEND_INSTR3(opcode) \
    c_instr = register_format3_instruction(cfg, c_instr, pc, opcode)
#define APPEND_INSTR4(opcode, dr, sr1, sr2) \
    c_instr = register_format4_instruction(cfg, c_instr, pc, opcode, \
                                           dr, sr1, sr2)
#define APPEND_INSTR5(opcode, dr, sr, imm13) \
    c_instr = register_format5_instruction(cfg, c_instr, pc, opcode, \
                                           dr, sr, imm13)
#define APPEND_INSTR6(opcode, dr, sr1, sr2) \
    c_instr = register_format6_instruction(cfg, c_instr, pc, opcode, \
                                           dr, sr1, sr2)
#define APPEND_INSTR7(opcode, sr3, sr1, sr2) \
    c_instr = register_format7_instruction(cfg, c_instr, pc, opcode, \
                                           sr3, sr1, sr2)
#define APPEND_INSTR8(opcode, sr2, sr1, imm13) \
    c_instr = register_format8_instruction(cfg, c_instr, pc, opcode, \
                                           sr2, sr1, imm13)
#define APPEND_INSTR9(opcode, intrp) \
    c_instr = register_format9_instruction(cfg, c_instr, pc, opcode, intrp)
#define APPEND_NOP() \
    c_instr = register_nop_instruction(cfg, c_instr, pc)

#include "CFG.ic"

#endif __CFG_H__
