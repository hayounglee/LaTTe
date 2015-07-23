/* value_numbering.c
   
   Implementation of CSE using value numbering
   
   Written by: ramdrive <ramdrive@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#include "config.h"
#include "flags.h"
#include "InstrNode.h"
#include "CFG.h"
#include "bit_vector.h"
#include "AllocStat.h"
#include "sma.h"


extern CFG *cfg;
static int bitvec_size;
static SMA_Session * this_session;

// type of RHS
enum {
    RHS_CONST,
    RHS_UNDEF,
    RHS_UNARY,
    RHS_BINARY,
    RHS_INVALID
};

#define RHS_MAX_FIELD_SIZE 3
#define RHS_INDEX_VALUE 0
#define RHS_INDEX_UNDEF 0
#define RHS_INDEX_INSTRCODE 0
#define RHS_INDEX_OP1 1
#define RHS_INDEX_OP2 2

//
// type       field[0]     field[1]   field[2]
// RHS_CONST  const_value 
// RHS_UNDEF  undef_count
// RHS_UNARY  instr_code   op1
// RHS_BINARY instr_code   op1        op2
//
struct rhs_t {
  int type;
  int vn;
  struct rhs_t * next;
  InstrNode * source;
  int count;
  int field[3];
};
typedef struct rhs_t rhs;

// allocate memory for rhs
static rhs* rhs_alloc(int type); 

// initialize r with type RHS_CONST and constant value 'value'
static rhs* rhs_init_const(rhs* r, int value);
// initialize r with type RHS_UNDEF and undef count 'count'
// We can distinguish undefs by 'count'
static rhs* rhs_init_undef(rhs* r);
// initialize r with type RHS_BINARY and 'code', 'op1','op2'
static rhs* rhs_init_binary(rhs* r, int code, int op1, int op2);

// get a type of rhs
static int rhs_get_type(rhs* r);
// get a value number of rhs
static int rhs_get_vn(rhs* r);
// get a constant value. only valid for RHS_CONST
static int rhs_get_value(rhs* r);
// get a instruction code. only valid for RHS_UNARY and RHS_BINARY
static int rhs_get_code(rhs* r);
// get a 'n'th operand. only valid for RHS_UNARY and RHS_BINARY
static int rhs_get_operand(rhs* r, int n);
static void rhs_print(rhs *r);

// increase redundancy count
static void rhs_increase_redundancy_count(rhs *r);
// get redundancy count
static int rhs_get_redundancy_count(rhs *r);

//
// return 1 if r1 and r2 is same
//        0 otherwise 
static int rhs_is_equal(rhs* r1, rhs* r2);



// closed hashing

typedef struct {
    int table_size;
    int num_of_max_elements; 
    int num_of_elements;
    rhs ** tbl;
} rhs_hash;

#define RHS_HASH_DEFAULT_TABLE_SIZE 128
#define RHS_HASH_EMPTY ((void *)0)
#define RHS_HASH_DELETED ((void *)1)

static rhs_hash * rhs_hash_init(rhs_hash *);

static rhs *rhs_hash_get_at(rhs_hash *rh, unsigned i);
static void rhs_hash_set_at(rhs_hash *rh, unsigned i, rhs *r);
static int rhs_hash_probe(int i);
static unsigned rhs_hash_hash(rhs *r);

static rhs *rhs_hash_find(rhs_hash *rh, rhs *r);
static void rhs_hash_insert(rhs_hash *rh, rhs *r);
static void rhs_hash_copy(rhs_hash *rh_src, rhs_hash *rh_dst);

//
// scoped rhs table
//

enum { GREATEREQUAL, EQUAL, NOTEQUAL };
enum { KNOWN_TRUE, KNOWN_FALSE, UNKNOWN };
typedef struct {
    int op;
    rhs * left;
    rhs * right;
} inequality;

#define MAX_BRANCH_DEPTH 10000
typedef struct {
    int top;
    inequality stack[MAX_BRANCH_DEPTH];
} cond_stack;

static inline cond_stack *cond_stack_init(cond_stack *stk) 
{
    stk->top = 0;
    return stk;
}

static inline void cond_stack_push(cond_stack *stk, inequality * cond) 
{
    assert(stk->top >= 0 && stk->top < MAX_BRANCH_DEPTH);
    stk->stack[stk->top].op = cond->op;
    stk->stack[stk->top].left = cond->left;
    stk->stack[stk->top++].right = cond->right;
}

static inline inequality * cond_stack_at(cond_stack *stk, int index) 
{
    assert(index >= 0 && index < stk->top);
    return stk->stack + index;
}

static inline int cond_stack_get_top(cond_stack *stk) 
{
    return stk->top;
}

static inline void cond_stack_set_top(cond_stack *stk, int top) 
{
    stk->top = top;
}

// for debugging purpose
void cond_stack_print(cond_stack *stk) 
{
    int i;
    for (i = 0; i < stk->top; i++) {
        printf("Stack[%d]:\n",i);
        printf("  Left :"); rhs_print(stk->stack[i].left);
        printf("  Right:"); rhs_print(stk->stack[i].right);
    }
}
//
// table (rhs, vn) -> needless; each rhs has a vn field
//

//
// table (vn, RHS) -> array of rhs *, maximum vn is MAX_VN
//
#define MAX_VN 10000

//
// table (variable, vn) -> use map
//

//
// table (vn, vn variable) -> use map
//


//
// undef number management
//
static void undef_init(void);
static int  undef_new(void);

//
// value number management
//
static void vn_init(void);
static int vn_new(void);
static int is_vn(int vn);


// allocate memory for rhs
static rhs* rhs_alloc(int type) 
{
    return (rhs *)SMA_Alloc(this_session, sizeof(rhs));
}

// initialize r with type RHS_CONST and constant value 'value'
static rhs *rhs_init_const(rhs *r, int value) 
{
    r->type = RHS_CONST;
    r->vn = vn_new();
    r->next = 0;
    r->source = 0;
    r->count = 0;
    r->field[RHS_INDEX_VALUE] = value;    
    return r;
}
// initialize r with type RHS_UNDEF and undef count 'count'
// We can distinguish undefs by 'count'
static rhs *rhs_init_undef(rhs *r) 
{
    r->type = RHS_UNDEF;
    r->vn = vn_new();
    r->next = 0;
    r->source = 0;
    r->count = 0; 
    r->field[RHS_INDEX_UNDEF] = undef_new();
    return r;
}
// initialize r with type RHS_BINARY and 'code', 'op1','op2'
static rhs *rhs_init_binary(rhs *r, int code, int op1, int op2) 
{
    r->type = RHS_BINARY;
    r->vn = vn_new();
    r->next = 0;
    r->source = 0;

    r->count = 0;
    r->field[RHS_INDEX_INSTRCODE] = code;
    r->field[RHS_INDEX_OP1] = op1;
    r->field[RHS_INDEX_OP2] = op2;
    return r;
}

// get a type of rhs
static int rhs_get_type(rhs *r) 
{
    return r->type;
}

// get a value number of rhs
static int rhs_get_vn(rhs *r) 
{
    return r->vn;
}

static InstrNode *rhs_get_source(rhs *r) 
{
    return r->source;
}

static void rhs_set_source(rhs *r, InstrNode *instr) 
{
    r->source = instr;
}

static void rhs_increase_redundancy_count(rhs *r) 
{
    r->count++;
}

static int rhs_get_redundancy_count(rhs *r) 
{
    return r->count;
}

// get a constant value. only valid for RHS_CONST
static int rhs_get_value(rhs *r) 
{
    return r->field[RHS_INDEX_VALUE];
}

static void rhs_set_value(rhs *r, int val) 
{
    r->field[RHS_INDEX_VALUE] = val;
}

// get a identifier for undef. only valid for RHS_UNDEF
static int rhs_get_count(rhs *r) 
{
    return r->count;
}

// get a instruction code. only valid for RHS_UNARY and RHS_BINARY
static int rhs_get_code(rhs *r) 
{
    return r->field[RHS_INDEX_INSTRCODE];
}

// get a 'n'th operand. only valid for RHS_UNARY and RHS_BINARY
static int rhs_get_operand(rhs *r, int n) 
{
    assert((rhs_get_type(r) == RHS_UNARY)
            || (rhs_get_type(r) == RHS_BINARY));

    return r->field[RHS_INDEX_OP1 + n];
}

//
// return 1 if r1 and r2 is same
//        0 otherwise 
static int rhs_is_equal(rhs *r1, rhs *r2) 
{
    if (rhs_get_type(r1) != rhs_get_type(r2)) {
        return false;
    }
    switch(rhs_get_type(r1)) {
      case RHS_CONST: 
        return rhs_get_value(r1) == rhs_get_value(r2);
      case RHS_UNDEF:
        return rhs_get_count(r1) == rhs_get_count(r2);
      case RHS_UNARY:
        return (rhs_get_code(r1) == rhs_get_code(r2))
            && (rhs_get_operand(r1,0) == rhs_get_operand(r2,0));
      case RHS_BINARY:
        return (rhs_get_code(r1) == rhs_get_code(r2))
            && (rhs_get_operand(r1,0) == rhs_get_operand(r2,0))
            && (rhs_get_operand(r1,1) == rhs_get_operand(r2,1));
    }

    assert(0 && "unknown rhs type");
    return 0;
}


static void rhs_print(rhs *r) 
{
    char *Instr_GetOpName();

    InstrNode inst;
    switch (rhs_get_type(r)) {
      case RHS_CONST:
        printf("%d: CONST(%d) [%d]\n", rhs_get_vn(r), rhs_get_value(r),
               rhs_get_redundancy_count(r));
        return;
      case RHS_UNDEF:
        printf("%d: UNDEF(%d) [%d]\n", rhs_get_vn(r), rhs_get_count(r),
               rhs_get_redundancy_count(r));
        return;
      case RHS_UNARY:
        inst.instrCode = rhs_get_code(r);
        printf("%d: UNARY:%s(%d) [%d]\n", rhs_get_vn(r),
               Instr_GetOpName(&inst), rhs_get_operand(r,0),
               rhs_get_redundancy_count(r));
        return;
      case RHS_BINARY:
        inst.instrCode = rhs_get_code(r);
        printf("%d: BINARY:%s(%d,%d) [%d]\n", rhs_get_vn(r),
               Instr_GetOpName(&inst),
               rhs_get_operand(r,0), rhs_get_operand(r,1),
               rhs_get_redundancy_count(r));
        return;
    }

    assert(0 && "never reach here.");
    return;
}


//
// rhs hash table
//

static rhs_hash * rhs_hash_init(rhs_hash *rh) 
{
    rh->table_size = RHS_HASH_DEFAULT_TABLE_SIZE;
    rh->num_of_max_elements = rh->table_size / 2;
    rh->num_of_elements = 0;
    rh->tbl = (rhs**)SMA_Alloc(this_session, sizeof(rhs*) * RHS_HASH_DEFAULT_TABLE_SIZE);
    bzero(rh->tbl, sizeof(rhs *) * RHS_HASH_DEFAULT_TABLE_SIZE);
    return rh;
}


static rhs *rhs_hash_get_at(rhs_hash *rh, unsigned i) 
{
    return rh->tbl[i % rh->table_size];
}

static void rhs_hash_set_at(rhs_hash *rh, unsigned i, rhs *r) 
{
//    assert(i >= 0 && i < rh->table_size);
//    assert(rh->tbl[i] == RHS_HASH_EMPTY || rh->tbl[i] == RHS_HASH_DELETED);
    rh->tbl[i % rh->table_size] = r;
}

static int rhs_hash_probe(int i) 
{
    return i * i;
}

static unsigned rhs_hash_hash(rhs *r) 
{
    switch (rhs_get_type(r)) {
      case RHS_CONST:
        return 0x01101001 ^ rhs_get_value(r);
      case RHS_UNDEF:
        return 0x10010110 ^ rhs_get_count(r);
      case RHS_UNARY:
        return (rhs_get_code(r) >> 16) ^ rhs_get_code(r) ^ rhs_get_operand(r, 0);
      case RHS_BINARY:
        return ((rhs_get_code(r) >> 16) ^ rhs_get_code(r) ^ rhs_get_operand(r, 0))
            + rhs_get_operand(r, 1);
    }

    return 0;
}

static void rhs_hash_rehash(rhs_hash *rh) 
{
    int i;
    rhs_hash new_hash;

    new_hash.table_size = rh->table_size * 2;
    new_hash.num_of_max_elements = new_hash.table_size;
    new_hash.num_of_elements = 0;
    new_hash.tbl = (rhs **)SMA_Alloc(this_session, sizeof(rhs *) * new_hash.table_size);
    bzero(new_hash.tbl, sizeof(rhs *) * new_hash.table_size);
    
    for (i = 0; i < rh->table_size; i++) {
        if (rh->tbl[i] != RHS_HASH_EMPTY &&
             rh->tbl[i] != RHS_HASH_DELETED) {

            rhs_hash_insert(& new_hash, rh->tbl[i]);
        }
    }

    rh->table_size = new_hash.table_size;
    rh->num_of_max_elements = new_hash.num_of_max_elements;
    rh->num_of_elements = new_hash.num_of_elements;
    rh->tbl = new_hash.tbl;
}

static void rhs_hash_insert(rhs_hash *rh, rhs *r) 
{
    int i ;
    unsigned key = rhs_hash_hash(r);
    rhs * f;

    f = rhs_hash_get_at(rh, key);
    if (f == RHS_HASH_EMPTY || f == RHS_HASH_DELETED) {
        rhs_hash_set_at(rh, key, r);
        return;
    }

    for (i = 1; i < rh->table_size; i++) {
        unsigned new_key = key + rhs_hash_probe(i);
        f = rhs_hash_get_at(rh, new_key);
        if (f == RHS_HASH_EMPTY || f == RHS_HASH_DELETED) {
            rhs_hash_set_at(rh, new_key, r);
            if (rh->num_of_max_elements <= ++(rh->num_of_elements)) { 
                rhs_hash_rehash(rh);
            }
            return;
        }
    }

    rhs_hash_rehash(rh);
    rhs_hash_insert(rh, r);
}

static rhs * rhs_hash_find(rhs_hash * rh, rhs * r) 
{
    int i;
    unsigned key = rhs_hash_hash(r) % rh->table_size;

    for (i = 0; i < rh->table_size;i++) {
        rhs * f = rhs_hash_get_at(rh, key + rhs_hash_probe(i));
        if (f == RHS_HASH_DELETED) continue;
        if (f == RHS_HASH_EMPTY) return NULL;
        if (rhs_is_equal(r, f)) {
            return f;
        }
    }

    return NULL;
}


static void rhs_hash_copy(rhs_hash *rh_src, rhs_hash *rh_dst) 
{
    rh_dst->table_size = rh_src->table_size;
    rh_dst->num_of_max_elements = rh_src->num_of_max_elements;
    rh_dst->num_of_elements = rh_src->num_of_elements;
    rh_dst->tbl = SMA_Alloc(this_session, sizeof(rhs*) * rh_src->table_size);
    memcpy(rh_dst->tbl, rh_src->tbl, rh_src->table_size * sizeof(rhs*));
}

//for debugging purpose
void rhs_hash_print(rhs_hash *rh) 
{
    int i;

    for (i = 0; i < rh->table_size; i++) {
        rhs *f = rhs_hash_get_at(rh, i);
        if (f != RHS_HASH_DELETED && f != RHS_HASH_EMPTY) {
            rhs_print(f);
        }
    }
}

//
// undef number management
//
static int number_of_current_undef;
static void undef_init(void) 
{
    // 0 is invalid
    number_of_current_undef = 1;
}

static int  undef_new(void) 
{
    return number_of_current_undef++;
}

//
// value number management
//
static int number_of_current_vn;
static void vn_init(void) 
{
    // 0 is invalid vn.
    number_of_current_vn = 1;
}

static int vn_new(void) 
{
    assert(number_of_current_vn < MAX_VN);
    return number_of_current_vn++;
}

static int is_vn(int vn) 
{
    return (vn > 0 && vn < number_of_current_vn);
}
//
//
//
// Value numbering 
//
//
//
//
//
//
//

extern void Instr_SetVN(InstrNode *instr, int vn);
extern int Instr_GetVN(InstrNode *instr);


static rhs ** vn_to_rhs_array;
static int * vn_to_vn_var_array;

int number_of_vn_var;

static rhs * get_rhs_from_vn(int vn) 
{
    return vn_to_rhs_array[vn];
}

static int get_vn_var_from_vn(int vn) 
{
    assert(is_vn(vn));
    return vn_to_vn_var_array[vn];
}

static void set_vn_var_from_vn(int vn, int var) 
{
    vn_to_vn_var_array[vn] = var;
}

static void vnvar_init(void) 
{
    number_of_vn_var = 0;
}

static int vnvar_new(void) 
{
    return number_of_vn_var++;
}

static int vnvar_get_number_of_vnvar(void) 
{
    return number_of_vn_var;
}

static int get_or_create_vn_var_from_vn(int vn, int dest_v) 
{
    int var;
    if (Var_IsVariable(get_vn_var_from_vn(vn))) {
        return get_vn_var_from_vn(vn);
    }
    if (vnvar_get_number_of_vnvar() >= MAX_VN_VAR) return -1;

    var = Var_make_vn(Var_GetType(dest_v), vnvar_new());
    set_vn_var_from_vn(vn, var);
    return var;
}

//
//
//
static int register_rhs(rhs_hash *rh, rhs *r) 
{
    int vn = rhs_get_vn(r);
    vn_to_rhs_array[vn] = r;
    rhs_hash_insert(rh, r);
    return vn;
}

static int get_or_create_vn_from_const(rhs_hash *rh, int);

static int get_or_create_vn_from_var(rhs_hash *rh, map *var2vn_map, int var) 
{
    int vn;

    if (Var_IsVariable(var)) {
        vn = map_Find(var2vn_map, var);
        if (vn > 0) return vn;
    } else if (var == g0) {
        return get_or_create_vn_from_const(rh, 0);
    }

    vn = register_rhs(rh, rhs_init_undef(rhs_alloc(RHS_UNDEF)));
    map_Insert(var2vn_map, var, vn);
    return vn;
}

static int get_or_create_vn_from_const(rhs_hash *rh, int val) 
{
    rhs _r;
    rhs *r = &_r;
    rhs * ret;

    r->type = RHS_CONST;
    rhs_set_value(r, val);

    ret = rhs_hash_find(rh, r);

    if (ret) return rhs_get_vn(ret);

    return register_rhs(rh, rhs_init_const(rhs_alloc(RHS_CONST), val));
}

static int create_vn_from_undef(rhs_hash *rh) 
{
    return register_rhs(rh, rhs_init_undef(rhs_alloc(RHS_UNDEF)));
}


static int get_or_create_vn_from_imm(rhs_hash *rh, InstrNode *instr) 
{
    return (Instr_IsUnresolved(instr)) ?
        create_vn_from_undef(rh) :
        (Instr_GetFormat(instr) == FORMAT_5) ? 
        get_or_create_vn_from_const(rh, instr->fields.format_5.simm13) :
        (Instr_GetFormat(instr) == FORMAT_8) ?
        get_or_create_vn_from_const(rh, instr->fields.format_8.simm13) :
        (Instr_GetFormat(instr) == FORMAT_2) ?
        get_or_create_vn_from_const(rh, instr->fields.format_2.imm22<<10):
        0 ;
}


static bool
is_commutative(InstrNode* c_instr) 
{
    int opcode = Instr_GetCode(c_instr);

    return (opcode == ADD
             || opcode == OR
             || opcode == AND
             || opcode == SMUL
             || opcode == UMUL
             || opcode == XOR
             || opcode == XNOR);
}
 
static bool
is_alu_op(InstrNode* c_instr) 
{
    int opcode = Instr_GetCode(c_instr);

    return (opcode == ADD
             || opcode == SUB 
             || opcode == OR
             || opcode == AND
             || opcode == SMUL
             || opcode == UMUL
             || opcode == SDIV
             || opcode == UDIV
             || opcode == SLL
             || opcode == SRA
             || opcode == SRL
             || opcode == XOR
             || opcode == XNOR);
}
static bool
can_change_reg_to_imm(InstrNode* c_instr) 
{
    int opcode = Instr_GetCode(c_instr);

    return (Instr_GetFormat(c_instr) == FORMAT_6 &&
             (opcode == SUBCC || is_alu_op(c_instr)  || opcode == ADDCC));
}

static bool
is_constant(InstrNode *instr, rhs_hash *rh, map *var_to_vn_map, int* const_val)
{
    int src_var1, src_var2, dst_var;
    rhs *rhs1, *rhs2;
    int opcode = Instr_GetCode(instr);
    int  imm_value = 0;
    
    if (!is_alu_op(instr)) return false;

    dst_var = Instr_GetDestReg(instr, 0);

    src_var1 = Instr_GetSrcReg(instr, 0);
    rhs1 = get_rhs_from_vn(get_or_create_vn_from_var(
        rh, var_to_vn_map, src_var1));

    if (rhs_get_type(rhs1) != RHS_CONST) return false;

    if (Instr_GetFormat(instr) == FORMAT_6){
        src_var2 = Instr_GetSrcReg(instr, 1);
        rhs2 = get_rhs_from_vn(
            get_or_create_vn_from_var(rh, var_to_vn_map, src_var2));

        if (rhs_get_type(rhs2) == RHS_CONST){
            imm_value =  rhs_get_value(rhs2);
        }
        else return false;
    }
    else if (!Instr_IsUnresolved(instr)) {
        imm_value = instr->fields.format_5.simm13;
    }

    switch(opcode){
      case ADD : *const_val = rhs_get_value(rhs1)+imm_value; break;
      case SUB : *const_val = rhs_get_value(rhs1)-imm_value; break;
      case AND : *const_val = rhs_get_value(rhs1)&imm_value; break;
      case OR  : *const_val = rhs_get_value(rhs1)|imm_value; break;
      case XOR : *const_val = rhs_get_value(rhs1)^imm_value; break;
      case XNOR: *const_val = ~(rhs_get_value(rhs1)^imm_value); break;
      case SMUL : 
        *const_val = rhs_get_value(rhs1)*imm_value; break;
      case UMUL : 
        *const_val = (unsigned)rhs_get_value(rhs1)*(unsigned)imm_value; break;
      case SDIV : case UDIV : return false;
      case SLL  : 
        *const_val = ((unsigned)rhs_get_value(rhs1))<<imm_value; break;
      case SRL  : 
        *const_val = ((unsigned)rhs_get_value(rhs1))>>imm_value; break;
      case SRA  : 
        *const_val = rhs_get_value(rhs1)>>imm_value; break;
      default:
        assert(0);
    }
    
    return true;

}

//For format 6 instruction, if either the type of vn of its operand
//is constant, then we change the type to format 5.
static void
constant_propagation(InstrNode *instr, rhs_hash *rh, map * var_to_vn_map)
{
    int src_var1, src_var2;
    rhs *rhs1, *rhs2;

    if (Instr_IsICopy(instr) || Instr_IsFCopy(instr))return;

    if (!can_change_reg_to_imm(instr) 
        && !Instr_IsStore(instr)
        && !Instr_IsLoad(instr)) return;

    switch (Instr_GetFormat(instr)){
//
// FORMAT_4 : load  r1,r2,r3
// FORMAT_5 : alu   r1,imm,r2 load r1,imm,r2
// FORAMT_6 : alu   r1,r2,r3
// FORMAT_7 : store r1,r2,r3
// FORMAT_8 : store r1,r2,imm     
//
      case FORMAT_4 :
      case FORMAT_6 :
        src_var1 = Instr_GetSrcReg(instr, 0);
        src_var2 = Instr_GetSrcReg(instr, 1);
        if (src_var2 == g0) return;

        rhs1 = get_rhs_from_vn(get_or_create_vn_from_var(
            rh, var_to_vn_map, src_var1));
        rhs2 = get_rhs_from_vn(get_or_create_vn_from_var(
            rh, var_to_vn_map, src_var2));

//         assert(rhs_get_type(rhs1) != RHS_CONST 
//                 || rhs_get_type(rhs2) != RHS_CONST);
        if (rhs_get_type(rhs2) == RHS_CONST 
            && rhs_get_value(rhs2) < (0x1<<12) 
            && rhs_get_value(rhs2) > ((-1)<<12)) {
            int org_reg;
            int const_val;

            org_reg = Instr_GetSrcReg(instr, 1);
            const_val = rhs_get_value(rhs2);

//            printf("num : %d   value : %d\nFrom : ",
//                   num, rhs_get_value(rhs2));stdout_instr(instr);

            instr->fields.format_5.simm13 = const_val;
            instr->format = FORMAT_5;

#ifdef UPDATE_CONSTANT_PROPAGATION
            if (Instr_GetCode(instr) == SMUL) {
                int dst_reg = Instr_GetDestReg(instr,0);
                int src_reg = Instr_GetSrcReg(instr,0);
                if (const_val == 0) {
                    //->sethi %dst, 0
                    if(instr->lastUsedRegs[0] == src_reg){
                        InstrNode* new_instr
                            = create_format6_instruction(ADD, g0,
                                                          org_reg, g0 , -5);
                        Instr_SetLastUseOfSrc(new_instr, 0);
                        CFG_InsertInstrAsSinglePred(
                            cfg, instr, new_instr);
                    }
                    instr->format = FORMAT_2;
                    instr->instrCode = SETHI;
                    instr->fields.format_2.destReg = dst_reg;
                    instr->fields.format_2.imm22 = 0;
                    instr->lastUsedRegs[0] = instr->lastUsedRegs[1] = 0;
                } else if (const_val == 1) {
                    //->mov %dst, %src
                    instr->instrCode = ADD;
                    instr->fields.format_5.simm13 = 0;
                } else if (const_val > 1) {
                    int i;
                    for (i = 1; i < 13; i++) {
                        if (const_val == (1<<i)) {
                            instr->instrCode = SLL;
                            instr->fields.format_5.simm13 = i;
                        } else if (const_val < (1<<i)) {
                            break;
                        }
                    }
                }
            } else if (Instr_GetCode(instr) == SDIV) {
                int src_reg = Instr_GetSrcReg(instr,0);
                if (const_val == 1){
                    //->mov %dst, %src
                    instr->instrCode = ADD;
                    instr->fields.format_5.simm13 = 0;
                    if(instr->predecessors[0]->instrCode == WRY) 
                      instr->predecessors[0]->instrCode = ADD;
                      
                } else if (const_val == 2) {
                    InstrNode* new_instr;
                    instr->instrCode = SRA;
                    instr->fields.format_5.simm13 = 1;
                    instr->fields.format_5.srcReg = g1;
                    if(instr->predecessors[0]->instrCode == WRY) 
                      instr->predecessors[0]->instrCode = ADD;
                    if (instr->lastUsedRegs[0] == src_reg) {
                        new_instr = create_format6_instruction(ADD, g0, src_reg, g0 , -5);
                        CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
                        instr->lastUsedRegs[0] = 0;
                                
                    }
                    new_instr = create_format5_instruction(SRL, g1, src_reg, 0x1f, -8);
                    CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
                    new_instr = create_format6_instruction(ADD, g1, src_reg, g1, -8);
                    CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);

                }
            }
#endif /* UPDATE_CONSTANT_PROPAGATION */

//            instr->lastUsedRegs[1] = 0;

//            printf("  To : ");stdout_instr(instr);

            if (instr->lastUsedRegs[1] == org_reg) {
                InstrNode * new_instr
                    = create_format6_instruction(ADD, g0,
                                                  org_reg, g0 , -5);
                Instr_SetLastUseOfSrc(new_instr, 0);
                CFG_InsertInstrAsSinglePred(
                    cfg, instr, new_instr);
            }
            instr->lastUsedRegs[1] = 0;
            
        } else if (0 && rhs_get_type(rhs1) == RHS_CONST
            && (is_commutative(instr) || Instr_GetCode(instr) == SUBCC)
            && rhs_get_value(rhs1) < (0x1<<12) 
            && rhs_get_value(rhs1) > ((-1)<<12)){
            int org_reg;

            org_reg = Instr_GetSrcReg(instr, 0);

            // for subcc, branch condition must be changed.
            if (Instr_GetCode(instr) == SUBCC){
                InstrNode* succ = Instr_GetSucc(instr, 0);

                if (!Instr_IsIBranch(succ)) break;
                
                assert(Instr_GetDestReg(instr, 0) == g0);

                switch(Instr_GetCode(succ)){
                  case BNE: case BE: break;
                  case BG:  succ->instrCode = BLE; break;
                  case BLE: succ->instrCode = BG ; break;
                  case BL:  succ->instrCode = BGE; break;
                  case BGE: succ->instrCode = BL;  break;
                  case BGU: succ->instrCode = BLEU; break;
                  case BCS: succ->instrCode = BCC; break;
                  case BCC: succ->instrCode = BCS; break;
                    
                  default: assert(0);
                }    
            }         
            assert(Instr_GetSrcReg(instr, 1) != g0);

            instr->fields.format_5.srcReg = Instr_GetSrcReg(instr, 1);
            instr->fields.format_5.simm13 = rhs_get_value(rhs1);
            instr->format = FORMAT_5;


            if (instr->lastUsedRegs[0] == org_reg) {
                InstrNode * new_instr
                    = create_format6_instruction(ADD, g0,
                                                  org_reg, g0 , -5);
                Instr_SetLastUseOfSrc(new_instr, 0);
                CFG_InsertInstrAsSinglePred(
                    cfg, instr, new_instr);
            }

            instr->lastUsedRegs[0] = instr->lastUsedRegs[1];
            instr->lastUsedRegs[1] = 0;
            
        }

        break;

      case FORMAT_7:
        src_var1 = Instr_GetSrcReg(instr, 0);
        src_var2 = Instr_GetSrcReg(instr, 1);
        if (src_var2 == g0) return;

        rhs1 = get_rhs_from_vn(get_or_create_vn_from_var(
            rh, var_to_vn_map, src_var1));
        rhs2 = get_rhs_from_vn(get_or_create_vn_from_var(
            rh, var_to_vn_map, src_var2));
        if (rhs_get_type(rhs2) == RHS_CONST 
            && rhs_get_value(rhs2) < (0x1<<12) 
            && rhs_get_value(rhs2) > ((-1)<<12)){
            int org_reg;

            org_reg = Instr_GetSrcReg(instr, 1);

//            printf("num : %d   value : %d\nFrom : ",
//                   num, rhs_get_value(rhs2));stdout_instr(instr);

            instr->format = FORMAT_8;
            instr->fields.format_8.simm13 = rhs_get_value(rhs2);
//            instr->lastUsedRegs[1] = 0;

//            printf("  To : ");stdout_instr(instr);

            if (instr->lastUsedRegs[1] == org_reg) {
                InstrNode * new_instr
                    = create_format6_instruction(ADD, g0,
                                                  org_reg, g0 , -5);
                Instr_SetLastUseOfSrc(new_instr, 0);
                CFG_InsertInstrAsSinglePred(
                    cfg, instr, new_instr);
            }
            instr->lastUsedRegs[1] = instr->lastUsedRegs[2];
            instr->lastUsedRegs[2] = 0;

        }
        break;
      default :
    }
}



static int is_memory_load(rhs *r) 
{
    InstrNode instr;
    instr.instrCode = rhs_get_code(r);
    return Instr_IsLoad(&instr);
}

static int is_memory_dependent(rhs *store, rhs *r) 
{
    InstrNode n;
    n.instrCode = rhs_get_code(r);

    if (! Instr_IsLoad(&n)) {
        return 0;
    }

    return rhs_get_operand(store,0) == rhs_get_operand(r,0)
   && rhs_get_operand(store,1) == rhs_get_operand(r,1);
//    return 1;
}

static int is_removable_instruction(InstrNode *instr) 
{
    switch (Instr_GetCode(instr)) {
      case SDIV: case UDIV: case RDY: case WRY:
      case SUBCC: case ADDCC: case ADDX: case SUBX:
      case RESTORE: case SPARC_SAVE:
        return 0;
    }
    return 1;
}



static void assign_vn(InstrNode *instr, rhs_hash *rh, map *var_to_vn_map) 
{
    int const_val;

    int dst_var = Instr_GetDestReg(instr, 0);

    if (!is_removable_instruction(instr)) {
        if (Var_IsVariable(dst_var)) {
            map_Erase(var_to_vn_map, dst_var);
        }
        return;
    } else if (Instr_IsCall(instr)) {
        int i;
           
        for (i = 0; i < rh->table_size; i++) {
            rhs * r = rhs_hash_get_at(rh, i);
            if (r != RHS_HASH_EMPTY && r != RHS_HASH_DELETED 
                && is_memory_load(r)
                // only dtable access and array size load
                && Instr_GetSrcReg(rhs_get_source(r), 1) != g0) {
//               stdout_instr(r->source);
                rhs_hash_set_at(rh, i, RHS_HASH_DELETED);
            }
        }

        for (i = 0; i < Instr_GetNumOfRets(instr); i++) {
            map_Erase(var_to_vn_map, Instr_GetRet(instr, i));
        }

        return;        
    } else if (Instr_IsStore(instr)) {
        // check memory dependencies and
        // remove dependent memory load RHSs from the table

        // for each memory load rhs in rhs_hash, 
        //    if there's memory dependency between load and store
        //        remove load from rhs_hash
        int i;
        rhs store_rhs;
        int vn1, vn2;

        if (Var_IsVariable(Instr_GetSrcReg(instr, 0))) {
            vn1 = get_or_create_vn_from_var(rh, var_to_vn_map,
                                            Instr_GetSrcReg(instr, 0));
        } else {
            vn1 = create_vn_from_undef(rh);
        }

        if (Instr_GetFormat(instr) == FORMAT_7) {
            if (Var_IsVariable(Instr_GetSrcReg(instr, 1))) {
                vn2 = get_or_create_vn_from_var(rh, var_to_vn_map,
                                                Instr_GetSrcReg(instr, 1));
            } else {
                vn2 = create_vn_from_undef(rh);
            }
        } else {
            vn2 = get_or_create_vn_from_imm(rh, instr);
        }

        rhs_init_binary(&store_rhs, Instr_GetCode(instr), vn1, vn2);

        for (i = 0; i < rh->table_size; i++) {
            rhs * r = rhs_hash_get_at(rh, i);
            if (r != RHS_HASH_EMPTY 
                && r != RHS_HASH_DELETED 
                && is_memory_dependent(&store_rhs, r)) {
                rhs_hash_set_at(rh, i, RHS_HASH_DELETED);
            }
        }

        return;
    } else if (!Var_IsVariable(dst_var)) {
        return;
    } else if (Instr_IsICopy(instr)) {
        int src_var = Instr_GetSrcReg(instr,0);

        map_Insert(var_to_vn_map, dst_var, 
                   get_or_create_vn_from_var(rh, var_to_vn_map, src_var));
        return;
    } else if (Instr_IsFCopy(instr)) {
        int src_var = Instr_GetSrcReg(instr,1);
        map_Insert(var_to_vn_map, dst_var, 
                   get_or_create_vn_from_var(rh, var_to_vn_map, src_var));
        return;
    } else if (Instr_IsLoad(instr)) {
        int vn1;
        int vn2;
        rhs r;
        rhs *ret;


        if (Var_IsVariable(Instr_GetSrcReg(instr, 0))) {
            vn1 = get_or_create_vn_from_var(rh, var_to_vn_map,
                                            Instr_GetSrcReg(instr, 0));
        } else {
            vn1 = create_vn_from_undef(rh);
        }

        if (Instr_GetFormat(instr) == FORMAT_4) {
            if (Var_IsVariable(Instr_GetSrcReg(instr, 1))) {
                vn2 = get_or_create_vn_from_var(rh, var_to_vn_map,
                                                Instr_GetSrcReg(instr, 1));
            } else if (Instr_GetSrcReg(instr, 1) == g0) {
                vn2 =  get_or_create_vn_from_const(rh, 0);
            } else {
                vn2 = create_vn_from_undef(rh);
            }
        } else {
            vn2 = get_or_create_vn_from_imm(rh, instr);
        }
        
        rhs_init_binary(&r, Instr_GetCode(instr), vn1, vn2);
        ret = rhs_hash_find(rh, &r);

        if (ret) {
            rhs_increase_redundancy_count(ret);
            Instr_SetVN(instr, rhs_get_vn(ret));
        } else {
            ret = rhs_init_binary(rhs_alloc(RHS_BINARY),
                                  Instr_GetCode(instr),
                                  vn1, vn2);
            rhs_set_source(ret, instr);
            Instr_SetVN(instr, register_rhs(rh, ret));
        }

        map_Insert(var_to_vn_map, dst_var, rhs_get_vn(ret));
        return;
    } else if (Instr_GetCode(instr) == SETHI) {
        map_Insert(var_to_vn_map, dst_var,
                   get_or_create_vn_from_imm(rh, instr));
        return;
    } else if (is_constant(instr, rh, var_to_vn_map, &const_val)) {
        map_Insert(var_to_vn_map, dst_var,
                   get_or_create_vn_from_const(rh, const_val));
        return;
    } else if (Instr_GetFormat(instr) == FORMAT_5) {
        // op rs, imm, rd
        int vn1 = get_or_create_vn_from_var(rh, var_to_vn_map,
                                            Instr_GetSrcReg(instr, 0));
        int vn2 = get_or_create_vn_from_imm(rh, instr);
        rhs r;
        rhs *ret;

        rhs_init_binary(&r, Instr_GetCode(instr), vn1, vn2);

        ret = rhs_hash_find(rh, &r);

        if (ret) {
            rhs_increase_redundancy_count(ret);
            Instr_SetVN(instr, rhs_get_vn(ret));
        } else {
            ret = rhs_init_binary(rhs_alloc(RHS_BINARY),
                                  Instr_GetCode(instr),
                                  vn1, vn2);
            rhs_set_source(ret, instr);
            Instr_SetVN(instr, register_rhs(rh, ret));
        }

        map_Insert(var_to_vn_map, dst_var, rhs_get_vn(ret));

        return;
    } else if (Instr_GetFormat(instr) == FORMAT_6) {
        // op rs, rt, rd

        int vn1 = get_or_create_vn_from_var(rh, var_to_vn_map,
                                            Instr_GetSrcReg(instr, 0));
        int vn2 = get_or_create_vn_from_var(rh, var_to_vn_map,
                                            Instr_GetSrcReg(instr, 1));
        rhs r;
        rhs *ret;

        rhs_init_binary(&r, Instr_GetCode(instr), vn1, vn2);
        ret = rhs_hash_find(rh, &r);

        if (ret) {
            rhs_increase_redundancy_count(ret);
            Instr_SetVN(instr, rhs_get_vn(ret));
        } else {
            ret = rhs_init_binary(rhs_alloc(RHS_BINARY),
                                  Instr_GetCode(instr),
                                  vn1, vn2);
            rhs_set_source(ret, instr);
            Instr_SetVN(instr, register_rhs(rh, ret));
        }
        
        map_Insert(var_to_vn_map, dst_var, rhs_get_vn(ret));

        return;
    }

//    assert(0 && "never reach here");
    return;
}

static void 
eliminate_redundancy(InstrNode *instr, map *var2vn_map, word *live)
{
    int vn = Instr_GetVN(instr);

    if (vn) {
        rhs * r = get_rhs_from_vn(vn);
        assert(r);

        if (rhs_get_count(r) > 0) {
            // redundant RHS
            if (rhs_get_source(r) == instr) {
                // source instruction
                int dest_v = Instr_GetDestReg(instr, 0);
                int vn_var = get_vn_var_from_vn(Instr_GetVN(instr));

                if (Var_IsVariable(vn_var)) {
                    InstrNode * new_instr;
                    assert(BV_IsSet(live, CFG_get_var_number(vn_var)));
                    if (Var_GetType(dest_v) == T_DOUBLE) {
                        new_instr = create_format6_instruction(
                            FMOVD, vn_var, g0, dest_v, -4);
                    } else if (Var_GetType(dest_v) == T_FLOAT) {
                        new_instr = create_format6_instruction(
                            FMOVS, vn_var, g0, dest_v, -4);
                    } else {
                        new_instr = create_format6_instruction(
                            ADD, vn_var, dest_v, g0, -4);
                    }
                    CFG_InsertInstrAsSucc(cfg, instr, 0, new_instr);
                }
                                         
            } else {
                // redundant instruction
                
                int dest_v = Instr_GetDestReg(instr, 0);
                int vn = Instr_GetVN(instr);
                int vn_var = get_or_create_vn_var_from_vn(vn, dest_v);

                assert(is_vn(vn));
                if (Var_IsVariable(vn_var)) {
                    InstrNode * new_instr;

                    if (Var_GetType(vn_var) == T_FLOAT) {
                        new_instr = create_format6_instruction(
                            FMOVS, dest_v, g0, vn_var, -4);
                        if (! BV_IsSet(live, CFG_get_var_number(vn_var))) {
                            Instr_SetLastUseOfSrc(new_instr, 1);
                            BV_SetBit(live, CFG_get_var_number(vn_var));
                        }
                    } else if (Var_GetType(vn_var) == T_DOUBLE) {
                        new_instr = create_format6_instruction(
                            FMOVD, dest_v, g0, vn_var, -4);
                        if (! BV_IsSet(live, CFG_get_var_number(vn_var))) {
                            Instr_SetLastUseOfSrc(new_instr, 1);
                            BV_SetBit(live, CFG_get_var_number(vn_var));
                        }
                    } else {
                        new_instr = create_format6_instruction(
                            ADD, dest_v, vn_var, g0, -4);
                        if (! BV_IsSet(live, CFG_get_var_number(vn_var))) {
                            Instr_SetLastUseOfSrc(new_instr, 0);
                            BV_SetBit(live, CFG_get_var_number(vn_var));
                        }
                    }
                    CFG_InsertInstrAsSucc(cfg, instr, 0, new_instr);
                    //CFG_RemoveInstr(cfg, instr);
                    
                    instr->instrCode = ADD;
                    Instr_SetDestReg(instr, 0, g0);
                }
            }
        }
    }
}

#ifdef USE_TRAP_FOR_ARRAY_BOUND_CHECK 
static void remove_tleu(InstrNode* tleu) 
{
    InstrNode* pred = Instr_GetPred(tleu, 0);
    assert(Instr_GetCode(tleu) == TLEU);

    while(Instr_GetCode(pred) != SUBCC) {
        assert(Instr_GetNumOfPreds(pred) == 1);
        pred = Instr_GetPred(pred,0);
    }
    Instr_SwitchToNop(tleu);
    pred->instrCode = ADD; //for last use information
}
#else /* not USE_TRAP_FOR_ARRAY_BOUND_CHECK */

static void remove_bcs(InstrNode * bcs) 
{
    InstrNode * pred = Instr_GetPred(bcs, 0);
    assert(Instr_GetCode(bcs) == BCS);

//    fprintf(stderr, "%s.%s: bcs removed\n",
//           Method_GetDefiningClass(cfg->method)->name->data,
//           cfg->method->name->data);

    while (Instr_GetCode(pred) != SUBCC) {
        assert(Instr_GetNumOfPreds(pred) == 1);
        pred = Instr_GetPred(pred,0);
    }

    CFG_RemoveBranch(cfg, bcs, 1);
    pred->instrCode = ADD;
}

static void remove_bcc(InstrNode * bcc) 
{
    InstrNode * pred = Instr_GetPred(bcc, 0);
    assert(Instr_GetCode(bcc) == BCC);

//    fprintf(stderr, "%s.%s: bcc removed\n",
//           Method_GetDefiningClass(cfg->method)->name->data,
//           cfg->method->name->data);

    while (Instr_GetCode(pred) != SUBCC) {
        assert(Instr_GetNumOfPreds(pred) == 1);
        pred = Instr_GetPred(pred,0);
    }

    CFG_RemoveBranch(cfg, bcc, 1);
    pred->instrCode = ADD;
}
#endif /* not USE_TRAP_FOR_ARRAY_BOUND_CHECK */

static int is_already_boundary_checked(cond_stack *cc_stack, inequality *eq) 
{
    int i;
    for (i = cond_stack_get_top(cc_stack) - 1; i >= 0; i--) {
        inequality * eq2 = cond_stack_at(cc_stack, i);
        if (rhs_is_equal(eq->right, eq2->right) &&
             rhs_is_equal(eq->left, eq2->left)) {
            return 1;
        }
    }
    return 0;
}

static int 
eliminate_branch(InstrNode *instr, rhs_hash * rh, 
                 map* var_to_vn_map, cond_stack *cc_stack) 
{
    static inequality prev_cond;

    if (Instr_GetCode(instr) == SUBCC) {
        prev_cond.left = 
            vn_to_rhs_array[
                get_or_create_vn_from_var(rh, var_to_vn_map,
                                          Instr_GetSrcReg(instr, 0))
           ];

        if (Instr_GetFormat(instr) == FORMAT_6) {
            prev_cond.right = 
                vn_to_rhs_array[
                    get_or_create_vn_from_var(rh, var_to_vn_map, 
                                              Instr_GetSrcReg(instr, 1))
               ];
        } else {
            prev_cond.right = 
                vn_to_rhs_array[get_or_create_vn_from_imm(rh, instr)];
        }
#ifdef USE_TRAP_FOR_ARRAY_BOUND_CHECK 
    } else if (Instr_GetCode(instr) == TLEU) {
        assert(instr->fields.format_9.intrpNum == 0x11);
        if (is_already_boundary_checked(cc_stack, &prev_cond)) {
            remove_tleu(instr);
            return 1;
        }
#else /* not USE_TRAP_FOR_ARRAY_BOUND_CHECK */
    } else if (Instr_GetCode(instr) == BCC
        && Instr_GetCode(Instr_GetSucc(instr, 0)) == TA) {

        if (is_already_boundary_checked(cc_stack, &prev_cond)) {
            // eliminate subcc & tcc pair
            remove_bcc(instr);
            return 1;
        }
#endif /* not USE_TRAP_FOR_ARRAY_BOUND_CHECK */
        // Boundary checking necessary
        cond_stack_push(cc_stack, &prev_cond);
    } else if (Instr_IsIBranch(instr)) {
        // Untill now, I don't care for branches but only TCC instructions.
        // Later, I'll eliminate real branches.
    }

    return 0;
}

static void
traverse_region(InstrNode *instr, rhs_hash *rh, 
                map * var_to_vn_map, cond_stack *cc_stack, word * live) 
{
    int deleted = 0;

    // DFS traversal - assign vn to each rhs

    constant_propagation(instr, rh, var_to_vn_map);
    assign_vn(instr, rh, var_to_vn_map);
    deleted = eliminate_branch(instr, rh, var_to_vn_map, cc_stack);

    if (Instr_GetNumOfSuccs(instr) == 1) {
        if (! Instr_IsRegionEnd(Instr_GetSucc(instr, 0))) {
            traverse_region(Instr_GetSucc(instr,0),
                             rh, var_to_vn_map, cc_stack, live);
        }
    } else if (Instr_GetNumOfSuccs(instr) == 2) {
        // Don't propagate values over tableswitch or lookupswitch
        int i;
        
        for (i = 0; i < Instr_GetNumOfSuccs(instr); i++) {
            InstrNode * succ = Instr_GetSucc(instr, i);
            if (!Instr_IsRegionEnd(succ)) {
                int old_number_of_vn = number_of_current_vn;
                int old_cc_stack_top = cond_stack_get_top(cc_stack);
                word live2[bitvec_size];
                rhs_hash rhash;

                BV_ClearAll(live2, bitvec_size);
                rhs_hash_copy(rh, &rhash);
                traverse_region(succ, &rhash, map_Clone(var_to_vn_map),
                                 cc_stack, live2);
                BV_OR(live, live2, bitvec_size);

                number_of_current_vn = old_number_of_vn;
                cond_stack_set_top(cc_stack, old_cc_stack_top);
            } else {
                // The target of the branch is a join point.
                InstrNode * new_instr = 
                    create_format6_instruction(ADD, g0,g0,g0, -10);
                CFG_InsertInstrAsSucc(cfg, instr, i, new_instr);
                Instr_UnmarkVisted(new_instr);
            }
        }
    } else {
        int i;
        
        for (i = 0; i < Instr_GetNumOfSuccs(instr); i++) {
            InstrNode * succ = Instr_GetSucc(instr, i);
            if (!Instr_IsRegionEnd(succ)) {
                int old_number_of_vn = number_of_current_vn;
                int old_cc_stack_top = cond_stack_get_top(cc_stack);
                word live2[bitvec_size];
                rhs_hash rhash;

                BV_ClearAll(live2, bitvec_size);
                rhs_hash_init(&rhash);
                traverse_region(succ, &rhash, map_Clone(var_to_vn_map),
                                cc_stack, live2);
                BV_OR(live, live2, bitvec_size);

                number_of_current_vn = old_number_of_vn;
                cond_stack_set_top(cc_stack, old_cc_stack_top);
            } else {
                // The target of the branch is a join point.
                InstrNode * new_instr = 
                    create_format6_instruction(ADD, g0,g0,g0, -10);
                CFG_InsertInstrAsSucc(cfg, instr, i, new_instr);
                Instr_UnmarkVisted(new_instr);
            }
        }
    }

    // reverse DFS order - eliminate redundancy

    if (!deleted) {
        eliminate_redundancy(instr, var_to_vn_map, live);
    }
}

int has_spill_in_h(AllocStat *h) 
{
    int r;
    for (r = Reg_number;
         r < CFG_GetNumOfStorageLocations(cfg);
         r++) {
        if (AllocStat_GetRefCount(h, r) > 0) return 1;
    }
    return 0;
}


/* Name        : VN_optimize_region
   Description : perform CSE in the region given entry node and initial map
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void 
VN_optimize_region(SMA_Session * session, InstrNode * header, AllocStat *h) 
{
    map _var_to_vn_map;
    map* var_to_vn_map = &_var_to_vn_map;
    int _vn_to_vn_var_array[MAX_VN];
    rhs* _vn_to_rhs_array[MAX_VN];  // (vn, rhs)
    rhs_hash rh;
    cond_stack cc_stk;
    word live[bitvec_size =
             (CFG_GetNumOfVars(cfg) - 1) / 32 + 1];

    //
    // initialize
    //
    this_session = session;

    vn_to_rhs_array = _vn_to_rhs_array;
    bzero(vn_to_rhs_array, sizeof(rhs *) * MAX_VN);

    map_Init(var_to_vn_map);
 
    vn_to_vn_var_array = _vn_to_vn_var_array;
    bzero(vn_to_vn_var_array, sizeof(int) * MAX_VN);

    vn_init();
    vnvar_init();
    undef_init();

    cond_stack_init(&cc_stk);

    rhs_hash_init(&rh);

    BV_ClearAll(live, bitvec_size);
    //
    //

//    if (has_spill_in_h(h)) {
//        return;
//    }

    traverse_region(header, &rh, var_to_vn_map, &cc_stk, live);
}

/*
 * $Log: value_numbering.c,v $
 * Revision 1.7  1999/09/27 07:20:59  chungyc
 * Removed '--enable-licm' configuration option.
 *
 * Revision 1.6  1999/09/14 10:28:16  walker
 * Make LaTTe compilable without TYPE_ANALYSIS.
 *
 * Revision 1.5  1999/09/08 01:44:35  scdoner
 * Var_make function has been divided into Var_make_stack, Var_make_local,
 * and so on. This separation removes the switch case statements, which
 * makes the function more suitable for inlining.
 *
 * Revision 1.4  1999/09/03 09:45:15  walker
 * make it compilable.
 *
 * Revision 1.3  1999/09/01 07:58:25  walker
 * - definition of INLINE is removed from .h files except basic.h
 * - definition of IRET, ORET, ... are included in reg.h
 * - Reg_number => Reg_number
 *
 * Revision 1.2  1999/08/30 09:53:56  walker
 * removing semicolon after '}'.
 *
 * Revision 1.1  1999/08/27 04:26:41  walker
 * File names are changed.
 * vn.[ch]->value_numbering.[ch]
 * live.[ch]->live_analysis.[ch]
 *
 * Revision 1.3  1999/08/08 16:33:56  walker
 * Bug fix.
 * 1. strength reduction : divide->shift change is not possible in case that
 *    divisor is negative
 * 2. The shift_size of LSHR, LSHL, LUSHR must be ANDed with '0x3f'.
 *
 * Revision 1.2  1999/08/07 06:49:14  scdoner
 * Bugs in value numbering and dead code elimination have been fixed.
 * Related Bugs-70 and 68.
 * Now, if an instruction is exception-generatable, it will not be
 * removed by DE.
 * When an instruction is replaced as a result of constant propagation,
 * the lastUsedRegs must be updated correctly. I fixed a bug in this.
 *
 * Revision 1.1.1.1  1999/07/29 11:25:30  chungyc
 * Migrated from 'old_latte' (which was previously 'latte')
 *
 * Revision 1.3.4.1  1999/07/09 10:15:34  walker
 * OO optimizations are added. -walker
 *
 * Revision 1.3  1999/04/10 02:28:22  jp
 * Loop optimization (LICM and register promotion) is implemented.
 * Loop peeling is also done. Two flags are appended: -noloopopt, -looppeeling
 * and new_jit/licm.c is created. -Jinpyo
 *
 * Revision 1.2  1999/03/16 06:24:26  walker
 * 1. Constant propagation is improved.
 * 2. When translating exception handler, a dummy root is inserted at making
 *    prologue code and is deleted after delay slot filling
 *    - walker
 *
 * Revision 1.1  1999/01/28 12:11:18  chungyc
 * Changed Makefiles.
 * Replaced garbage collector.
 * Added CSE.
 * Minor changes in phase 2.
 *
 */

