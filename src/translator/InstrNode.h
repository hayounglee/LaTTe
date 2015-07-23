/* InstrNode.h
   
   This is a header file to provide the structure of a translated code.
   The intermediate code is a SPARC code which contains symbolic registers
   instead of physical registers.
   An InstrNode is used to make a CFG of a method.
   
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties.
 */

#ifndef __INSTRNODE_H__
#define __INSTRNODE_H__

#include "flags.h"
#include "reg.h"
#include "SPARC_instr.h"
#include <stdio.h>
#include <stdarg.h>
#include <strings.h>
#include "fast_mem_allocator.h"
#include "basic.h"


struct InstrToResolve;


//
// operation format
// This is based on the SPARC instruction formats.
// Each different instruction format corresponds to one format among these.
// To get a real operation format whose length is 2 bits,
// use 'OP_FORMAT_MASK'.
//

typedef enum {
    FORMAT_1,           // call
    FORMAT_2,           // sethi
    FORMAT_3,           // branch on cc
    FORMAT_4,           // load instruction which use three registers
    FORMAT_5,           // src,  imm  -> dst (including load)
    FORMAT_6,           // src1, src2 -> dst (excluding load)
    FORMAT_7,           // store instructions which use three registers
    FORMAT_8,	        // store instructions which use two registers
    FORMAT_9,	        // trap instruction


    INVALID_FORMAT
} OpFormat;

//
// structure for instruction fields
//
typedef union InstrFields {
    struct {
	Reg    destReg;	// always 'o7'
	int        disp30; // function address
    } format_1;		// for 'call'
    struct {
	Reg    destReg;
	int        imm22;
    } format_2;		// for 'sethi'
    struct {
	Reg    srcReg;	// For SPARC v8, this is always implicit 'cc'.
	int         disp22; // branch target
    } format_3;		// for branch instructions
    struct {
	Reg    destReg;
	Reg    srcReg_1;
	Reg    srcReg_2;
    } format_4;		// for load instructions which use only registers
    struct {
	Reg    destReg;
	Reg    srcReg;
	short       simm13;
    } format_5;		// for instructions which use immediate
    struct {
	Reg    destReg;
	Reg    srcReg_1;
	Reg    srcReg_2;
    } format_6;		// for remaining instructions which use only registers
    struct {
	Reg    srcReg_3;
	Reg    srcReg_1;
	Reg    srcReg_2;
    } format_7;		// for store instructions which use three registers
    struct {
	Reg    srcReg_2;
	Reg    srcReg_1;
	short       simm13;
    } format_8;
    struct {
	short       intrpNum;
    } format_9;


} InstrFields;


//
// InstrNode Type
//
#define  DELAY_SLOT_INSTR    0x1
#define  RESOLVE_INSTR       0x2
#define  PINNED_INSTR        0x4


//
// function call information
// The pseudo registers used as arguments and as return value are listed.
// This structure is used for 'jmpl' and 'call'.
//
typedef struct ArgInfo {
    int    numOfArgs;	        // number of pseudo registers used as arguments
    int*   argVars;		// pseudo registers used as arguments
} ArgInfo;

typedef struct RetInfo {
    int    numOfRets;		// number of pseudo registers used as return value
    int    retVars[2];	// pseudo registers used as return value
} RetInfo;

typedef struct FuncInfo {
    RetInfo    retInfo;
    ArgInfo    argInfo;
    bool       needCSIUpdate;
#ifdef INLINE_CACHE
    int    dtable_offset;
#endif
} FuncInfo;

//
// structure to represent the intermediate and the native SPARC code
// in the control flow graph
// The translate stage 2 will use this structure intensively.
//
#define DEFAULT_SUCC_NUM   4
#define DEFAULT_PRED_NUM   4

typedef enum {
    NO_NEED_MAP_INFO = 0,	/* only stores call chain */
    NEED_MAP_INFO,		/* stores all variable map */
    NEED_LOCAL0_MAP_INFO,	/* stores local 0 map */
    NEED_RET_ADDR_INFO		/* deprecated, same as NEED_MAP_INFO */
} MapInfo;

static const int INSTR_NEEDMAPINFO_MASK 	= 0x0003;
static const int INSTR_EXCEPTION_GENERATABLE 	= 0x0004;
static const int INSTR_LOOP_HEADER 		= 0x0008;
static const int INSTR_INLINE_CACHE_START 	= 0x0010;
static const int INSTR_VIRTUAL_CALL_START 	= 0x0020;
static const int INSTR_VOLATILE			= 0x0040;
static const int INSTR_EXCEPTION_RETURN		= 0x0080;

typedef struct InstrNode {
    uint8          instrType; // instruction type

    //
    // Now the lists of predecessors and successors
    // are implemented with array. - doner
    //
    struct InstrNode** predecessors;
    int                numOfPredecessors;
    int                maxNumOfPredecessors;

    struct InstrNode** successors;
    int                numOfSuccessors;
    int                maxNumOfSuccessors;


    OpFormat           format;	// instruction type
    int                instrCode;

    InstrFields        fields;     // instruction fields

    uint32             bytecodePC; // bytecode offset
    // corresponding to this instruction

    int32              nativeOffset;   // native code offset


    /* replacement of several bool variables such as
       needMapInfo : indicate whether need the record of variable map
       exceptionGeneratable : indicate whether generate an exception
       loopHeader : indiate whether loop header or not
       isStartOfInlineCache : indicate whether start of inline cache
       isStartOfVirtualCallSequence : indicate whether start of virtual call
       isVolatile : indicate whether volatile or not
       exceptionReturn : whether it is exception returning call or not */
    uint8 properties;

    int                *varMap;

    struct InlineGraph *graph;

    // for register allocation
    int                visited;	/* note that the type is not boolean. The integer
				   number is used to identify the edge to the node. */
    int                preferredDest;
    int                lastUsedRegs[3];

    struct InstrToResolve*    resolveEntry;
    
    void*              h_map;	// for h mapping
    word*              live;    // live information for local variables
    int                vn; // value number

#ifdef TYPE_ANALYSIS
    struct TypedValue* myTypedValue;
    void*              mv2tv;  // for typed value mapping
    int32              retStackTop;
    int32              operandStackTop;
    int32              invokeType;
#endif

    void*              instrInfo; // pointer to a structure containing additional
    // information about the instruction
    // currently this field is used for 'call' and 'return'.
    // 'FuncInfo' and 'RetInfo' is used for them.
#ifdef DYNAMIC_CHA
    void*              dchaInfo;
#endif
    int                arrayIndex; // index in CFG array.

#ifndef NDEBUG
    /* >0 if this is in the loop under consideration */
    int                loop_instr_id;
#endif

/////////////////////////////////////////////////////////////////////
// VLIW JIT specific...

#ifdef __GNUC__
    Field*  field[0];      // Please assure that no more field follows "field".
#else
    Field*  field;
#endif
} InstrNode;

//struct CFG; 
//typedef struct CFG CFG;

void         Instr_Init(InstrNode* c_instr);

// methods for graph manipulation
void         Instr_AddPred(InstrNode* c_instr, InstrNode* pred);
InstrNode*   Instr_SetPred(InstrNode* c_instr, int index, InstrNode* pred);
int          Instr_GetNumOfPreds(InstrNode* c_instr);
InstrNode*   Instr_GetPred(InstrNode* c_instr, int index);
void         Instr_RemoveAllPreds(InstrNode* c_instr);
void         Instr_RemovePred(InstrNode* c_instr, InstrNode* pred);
bool         Instr_IsPred(InstrNode* c_instr, InstrNode* pred);
int          Instr_GetPredIndex(InstrNode* c_instr, InstrNode* pred);

void         Instr_AddSucc(InstrNode* c_instr, InstrNode* succ);
InstrNode*   Instr_SetSucc(InstrNode* c_instr, int index, InstrNode* succ);
int          Instr_GetNumOfSuccs(InstrNode* c_instr);
InstrNode*   Instr_GetSucc(InstrNode* c_instr, int index);
void         Instr_RemoveAllSuccs(InstrNode* c_instr);
void         Instr_RemoveSucc(InstrNode* c_instr, InstrNode* succ);
bool         Instr_IsSucc(InstrNode* c_instr, InstrNode* succ);
int          Instr_GetSuccIndex(InstrNode* c_instr, InstrNode* succ);


void         Instr_connect_instruction(InstrNode* previous_instr, InstrNode* new_instr);
void         Instr_disconnect_instruction(InstrNode* first_instr, InstrNode* second_instr);

void         Instr_MarkVisited(InstrNode* c_instr);
void         Instr_UnmarkVisted(InstrNode* c_instr);
bool         Instr_IsVisited(InstrNode* c_instr);
int          Instr_GetVisitedNum(InstrNode * c_instr);

FuncInfo*    Instr_GetFuncInfo(InstrNode * c_instr);
struct InlineGraph *Instr_GetInlineGraph(InstrNode *c_instr);
void         Instr_SetInlineGraph(InstrNode *c_instr, struct InlineGraph *graph);

// instruction creation methods
// After creation, user should register the intruction into a cfg.
InstrNode*   create_format1_instruction(int         instr_code,
                                        int         disp30,
                                        InstrOffset bytecode_pc);
InstrNode*   create_format2_instruction(int         instr_code,
                                        Reg         dest_reg,
                                        int         imm22,
                                        InstrOffset bytecode_pc);
InstrNode*   create_format3_instruction(int         instr_code,
                                        InstrOffset bytecode_pc);
InstrNode*   create_format4_instruction(int         instr_code,
                                        Reg         dest_reg,
                                        Reg         src_reg_1,
                                        Reg         src_reg_2,
                                        InstrOffset bytecode_pc);
InstrNode*   create_format5_instruction(int         instr_code,
                                        Reg         dest_reg,
                                        Reg         src_reg,
                                        short       simm13,
                                        InstrOffset bytecode_pc);
InstrNode*   create_format6_instruction(int         instr_code,
                                        Reg         dest_reg,
                                        Reg         src_reg_1,
                                        Reg         src_reg_2,
                                        InstrOffset bytecode_pc);
InstrNode*   create_format7_instruction(int         instr_code,
                                        Reg         src_reg_3,
                                        Reg         src_reg_1,
                                        Reg         src_reg_2,
                                        InstrOffset bytecode_pc);
InstrNode*   create_format8_instruction(int         instr_code,
                                        Reg         src_reg_2,
                                        Reg         src_reg_1,
                                        short       simm13,
                                        InstrOffset bytecode_pc);
InstrNode*   create_format9_instruction(int         instr_code,
                                        short       intrp_num,
                                        InstrOffset bytecode_pc);



//
// some useful instruction generation functions
//
InstrNode*   Instr_create_copy(Reg from_reg, Reg to_reg);
InstrNode*   Instr_create_store(Reg from_reg, Reg to_reg);
InstrNode*   Instr_create_load(Reg from_reg, Reg to_reg);
InstrNode*   Instr_create_nop();
InstrNode*   Instr_create_region_dummy();

InstrNode*   Instr_Clone(InstrNode* c_instr);


InstrOffset  Instr_GetBytecodePC(InstrNode* c_instr);


OpFormat     Instr_GetFormat(InstrNode* c_instr);
InstrFields* Instr_GetFields(InstrNode* c_instr);


// get/set an informat about an instruction
void         Instr_SetDelayed(InstrNode* c_instr);
bool         Instr_IsDelayed(InstrNode* c_instr);
void         Instr_SetUnresolved(InstrNode* c_instr);
void         Instr_UnsetUnresolved(InstrNode* c_instr);
bool         Instr_IsUnresolved(InstrNode* c_instr);

#ifdef INLINE_CACHE
int          Instr_GetDTableIndex(InstrNode*);
#endif INLINE_CACHE
int          Instr_GetCode(InstrNode * c_instr);

int          Instr_GetNativeOffset(InstrNode * c_instr);
void         Instr_SetNativeOffset(InstrNode * c_instr, int native_offset);

int          Instr_GetNumOfDestRegs(InstrNode* c_instr);
Reg          Instr_GetDestReg(InstrNode* c_instr, int index);
void         Instr_SetDestReg(InstrNode* c_instr, int index, Reg dest_reg);

int          Instr_GetNumOfSrcRegs(InstrNode* c_instr);
Reg          Instr_GetSrcReg(InstrNode* c_instr, int index);
void         Instr_SetSrcReg(InstrNode* c_instr, int index, Reg src_reg);

bool         Instr_IsICopy(InstrNode* c_instr);
bool         Instr_IsFCopy(InstrNode* c_instr);
bool         Instr_IsReturn(InstrNode* c_instr);
bool         Instr_IsDirectCall(InstrNode* c_instr);
bool         Instr_IsIndirectCall(InstrNode* c_instr);
bool         Instr_IsCall(InstrNode* c_instr);
bool         Instr_IsBranch(InstrNode* c_instr);
bool         Instr_IsMemAccess(InstrNode* c_instr);
bool         Instr_IsTrap(InstrNode* c_instr);
bool         Instr_IsIBranch(InstrNode* c_instr);
bool         Instr_IsFBranch(InstrNode* c_instr);
void         Instr_PredictTaken(InstrNode* c_instr);
void         Instr_PredictUnTaken(InstrNode* c_instr);
bool	     Instr_IsExceptionReturn(InstrNode *c_instr);
void	     Instr_SetExceptionReturn(InstrNode *c_instr);
#define Instr_IsUMul(x) (Instr_GetCode((x)) == UMUL)
#define Instr_IsSDiv(x) (Instr_GetCode((x)) == SDIV)

bool         Instr_IsRestore(InstrNode* c_instr); // This method is specific to SPARC.
bool         Instr_IsCTI(InstrNode* c_instr); // instruction having delay slot
bool         Instr_IsNop(InstrNode* c_instr);
bool         Instr_IsRegionDummy(InstrNode* c_instr);
bool         Instr_IsSetcc(InstrNode* c_instr);
bool         Instr_IsTableJump(InstrNode* c_instr);
bool         Instr_IsLoad(InstrNode* c_instr);
bool         Instr_IsStore(InstrNode* c_instr);
bool         Instr_IsCondMove(InstrNode* c_instr);

bool         Instr_IsSame(InstrNode* instr1, InstrNode* instr2);

/* method inline related functions */
/* only valid before register allocation */
bool         Instr_IsMethodEnd(InstrNode *instr);
bool         Instr_IsMethodStart(InstrNode *instr);

Reg          Instr_GetPreferredDest(InstrNode* c_instr);
void         Instr_SetPreferredDest(InstrNode* c_instr,
                                    Reg preferred_dest);

bool         Instr_IsJoin(InstrNode* c_instr);

void         Instr_SetLastUseOfSrc(InstrNode* c_instr, int index);
bool         Instr_IsLastUse(InstrNode* c_instr, Reg reg);

void         Instr_SetAnnulling(InstrNode* c_instr);

void         Instr_Register_to_jump_table(InstrNode** jump_table,
                                          InstrNode*  c_instr,
                                          int         index);


void*        Instr_GetH(InstrNode* c_instr);
void         Instr_SetH(InstrNode* c_instr, void* h_map);

int          Instr_GetVN(InstrNode * c_instr);
void         Instr_SetVN(InstrNode * c_instr, int vn);

word *       Instr_GetLive(InstrNode *);
void         Instr_SetLive(InstrNode *, word *);


int          Instr_IsRegionEnd(InstrNode * inst);

void         Instr_SetLoopHeader(InstrNode* c_instr);
bool         Instr_IsLoopHeader(InstrNode* c_instr);

void         Instr_SetStartOfInlineCache(InstrNode *c_instr);
bool         Instr_IsStartOfInlineCache(InstrNode *c_instr);

void         Instr_SetStartOfVirtualCallSequence(InstrNode *c_instr);
bool         Instr_IsStartOfVirtualCallSequence(InstrNode *c_instr);

void         Instr_SetVolatile(InstrNode *c_instr);
bool         Instr_IsVolatile(InstrNode *c_instr);


void         Instr_SetResolveInfo(InstrNode* c_instr, struct InstrToResolve* entry);
struct InstrToResolve*  Instr_GetResolveInfo(InstrNode* c_instr);


//
// call information management
//
FuncInfo*    FuncInfo_alloc(void);
RetInfo*     RetInfo_alloc(void);
ArgInfo*     ArgInfo_alloc(void);

void         FuncInfo_init(FuncInfo* func_info,
                            int       num_of_args,
                            int*      arg_vars,
                            int       num_of_rets,
                            int*      ret_vars);
void         RetInfo_init(RetInfo* ret_info,
                           int      num_of_rets,
                           int*     ret_vars);
void         ArgInfo_init(ArgInfo* arg_info,
                           int      num_of_args,
                           int*     arg_vars);
FuncInfo*    Instr_make_func_info(Method* meth, int ops_top, int arg_size);


void         Instr_SetInstrInfo(InstrNode* c_instr, void* instr_info);

int          Instr_GetNumOfArgs(InstrNode* c_instr);
int          Instr_GetArg(InstrNode* c_instr, int index);
int          Instr_GetNumOfRets(InstrNode* c_instr);
int          Instr_GetRet(InstrNode* c_instr, int index);


//
// for exception handling
//
void         Instr_SetExceptionGeneratable(InstrNode* c_instr);
void	     Instr_UnsetExceptionGeneratable(InstrNode *c_instr);
bool         Instr_IsExceptionGeneratable(InstrNode* c_instr);
void         Instr_SetNeedMapInfo(InstrNode* c_instr);
void         Instr_UnsetNeedMapInfo(InstrNode* c_instr);
bool         Instr_NeedMapInfo(InstrNode* c_instr);

#ifdef TYPE_ANALYSIS
void         Instr_ChangeCallTarget(InstrNode* c_instr, void* new_target);
Var          Instr_GetReceiverReferenceVariable(InstrNode* c_instr);
#endif


char*        Instr_GetOpName(InstrNode* c_instr);
void         Instr_Print(InstrNode* c_instr, FILE* fp, char tail);
void         Instr_PrintToStdout(InstrNode* c_instr);
void         Instr_PrintInXvcg(InstrNode* c_instr, FILE* fp);

bool	     Instr_IsIInc(InstrNode* instr);

#include "InstrNode.ic"

#endif __INSTRNODE_H__

