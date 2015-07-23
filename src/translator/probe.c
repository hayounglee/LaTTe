#include "config.h"
#include "probe.h"

#ifdef NEW_COUNT_RISC

#define PROFILE_HASH_TABLE_SIZE 1024 * 1024
typedef struct RiscCountElement {
    Method  *method;
    int     pc;
    long    count;
} RISCCountElement;

RISCCountElement profileHashTable[PROFILE_HASH_TABLE_SIZE] = {{0,},};
int elementCount = 0;

void ProfileRISCInstr( Method *, int, int );

void
insert_probe_code( CFG *cfg, InstrNode *instr, int instrNO ) {
    // generated code as below
    // save     sp, -96, sp
    // add      g1, g0, l0
    // add      g2, g0, l1
    // add      g3, g0, l2
    // add      g4, g0, l3
    // add      g5, g0, l4
    // add      g6, g0, l5
    // add      g7, g0, l6
    // rd       %ccr, l7
    // sethi    %hi(method), o0
    // or       o0, %lo(method), o0
    // rd       %pc, o1
    // sethi    %hi(instrNO), o2
    // or       o2, %lo(instrNO), o2
    // call     ProfileRISCInstr
    // nop
    // wr       l7, %ccr
    // add      l0, g0, g1
    // add      l1, g0, g2
    // add      l2, g0, g3
    // add      l3, g0, g4
    // add      l4, g0, g5
    // add      l5, g0, g6
    // add      l6, g0, g7
    // restore
    InstrNode *c_instr = instr;
    Method *method = IG_get_method( CFG_get_IG_root( cfg ) );

    c_instr = create_format5_instruction( SPARC_SAVE, SP, SP, -96, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format6_instruction( ADD, l0, g1, g0, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format6_instruction( ADD, l1, g2, g0, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format6_instruction( ADD, l2, g3, g0, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format6_instruction( ADD, l3, g4, g0, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format6_instruction( ADD, l4, g5, g0, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format6_instruction( ADD, l5, g6, g0, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format6_instruction( ADD, l6, g7, g0, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format6_instruction( RDY, l7, 2, 0, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format2_instruction( SETHI, o0, HI( method ), -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );
    
    c_instr = create_format5_instruction( OR, o0, o0, LO( method ), -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format6_instruction( RDY, o1, 5, 0, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format2_instruction( SETHI, o2, HI( instrNO ), -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );
    
    c_instr = create_format5_instruction( OR, o2, o2, LO( instrNO ), -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format1_instruction( CALL, 
                                          (int) ProfileRISCInstr, -23);
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    add_new_resolve_instr( cfg, c_instr, (void *) ProfileRISCInstr );
    mark_visited( c_instr );

    c_instr = create_format2_instruction( SETHI, g0, 0, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    set_delay_slot_instr( c_instr );
    mark_visited( c_instr );

    c_instr = create_format6_instruction( ADD, g1, l0, g0, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format6_instruction( ADD, g2, l1, g0, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format6_instruction( ADD, g3, l2, g0, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format6_instruction( ADD, g4, l3, g0, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format6_instruction( ADD, g5, l4, g0, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format6_instruction( ADD, g6, l5, g0, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format6_instruction( ADD, g7, l6, g0, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format6_instruction( WRY, 2, l7, 0, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );

    c_instr = create_format6_instruction( RESTORE, g0, g0, g0, -23 );
    insert_instruction_as_single_predecessor( cfg, instr, c_instr );
    mark_visited( c_instr );
}

int
GetHashNumber( Method *method, int pc ) {
    int index = ((unsigned int ) method * pc) % PROFILE_HASH_TABLE_SIZE;
    int i;
    
    assert( index < PROFILE_HASH_TABLE_SIZE );

    if ( profileHashTable[index].method == method
         && profileHashTable[index].pc == pc ) return index;

    for( i = index + 1; i != index; i++ ) {
        if ( ( profileHashTable[i].method == method
               && profileHashTable[i].pc == pc ) 
             || ( profileHashTable[i].method == NULL ) )return i;

        if ( i == PROFILE_HASH_TABLE_SIZE ) i = 0;
    }

    assert( 0 && "hash table full! recompile with increased hash table size" );

    return 0;
}

void 
ProfileRISCInstr( Method *method, int pc, int instrNO ) {
    int index = GetHashNumber( method, pc );

    if ( profileHashTable[index].method == NULL ) {
        // 새로운 element 추가
        assert( profileHashTable[index].pc == 0 );
        assert( profileHashTable[index].count == 0 );

        elementCount++;

        profileHashTable[index].method = method;
        profileHashTable[index].pc = pc;
        profileHashTable[index].count = instrNO;
    } else {
        // 기존의 element
        profileHashTable[index].count += instrNO;
    }
    
}

typedef struct MethodRISCCount {
    Method                  *method;
    long long               count;
    struct MethodRISCCount  *next;
} MethodRISCCount;

int MethodRISCCountNO = 0;

MethodRISCCount *
IncreaseMethodCount( MethodRISCCount *root, Method *method, int count ) {
    MethodRISCCount *element;

    for( element = root; element != NULL; element = element->next ) {
        if ( element->method == method ) break;
    }

    if ( element == NULL ) {
        element = (MethodRISCCount *) malloc( sizeof( MethodRISCCount) );
        element->method = method;
        element->count = count;
        element->next = root;
        MethodRISCCountNO++;
        return element;
    } else {
        element->count += count;
        return root;
    }
    return NULL;
}

void
PrintRISCInstrProfile() {
    int i, j;
    long long totalCount = 0;
    MethodRISCCount *root = NULL;
    MethodRISCCount *methodCountArray;
    MethodRISCCount *element;

    RISCCountElement *extracted = 
        (RISCCountElement *) alloca( sizeof(RISCCountElement) * elementCount );

    for( i = 0, j = 0; i < PROFILE_HASH_TABLE_SIZE; i++ ) {
        if ( profileHashTable[i].count != 0 ) {
            extracted[j] = profileHashTable[i];
            j++;
            totalCount += profileHashTable[i].count;
            root = IncreaseMethodCount( root, 
                                        profileHashTable[i].method, 
                                        profileHashTable[i].count );
        }
    }

    methodCountArray = (MethodRISCCount *) alloca( sizeof(MethodRISCCount) 
                                                   * MethodRISCCountNO );

    for( element = root, i = 0; 
         element != NULL; 
         element = element->next, i++ ) {
        methodCountArray[i] = *element;
    }

    for( i = 0; i < MethodRISCCountNO; i++ ) {
        for( j = i + 1; j < MethodRISCCountNO; j++ ) {
            if ( methodCountArray[i].count < methodCountArray[j].count ) {
                MethodRISCCount temporary = methodCountArray[i];
                methodCountArray[i] = methodCountArray[j];
                methodCountArray[j] = temporary;
            }
        }
    }

    for( i = 0; i < MethodRISCCountNO; i++ ) {
        if ( methodCountArray[i].count == 0 ) break;
        
        fprintf( stderr, "METHOD %s %s %s : %lld\n",
                 methodCountArray[i].method->class->name->data,
                 methodCountArray[i].method->name->data,
                 methodCountArray[i].method->signature->data,
                 methodCountArray[i].count );
    }

    // sort hash table
    for( i = 0; i < elementCount; i++ ) {
        for( j = i + 1; j < elementCount; j++ ) {
            if ( extracted[i].count < extracted[j].count ) {
                RISCCountElement element = extracted[j];
                extracted[j] = extracted[i];
                extracted[i] = element;
            }
        }
    }

    for( i = 0; i < elementCount; i++ ) {
        if ( extracted[i].count == 0 ) break;

        fprintf( stderr, "BB %s %s %s : 0x%x : %ld\n",
                 extracted[i].method->class->name->data,
                 extracted[i].method->name->data,
                 extracted[i].method->signature->data,
                 extracted[i].pc,
                 extracted[i].count );
    }

    fprintf( stderr, "TOTAL : %lld\n", totalCount );
}
#else
// old versions profile codes.
#ifdef COUNT_BYTECODE
#define	YDBG(s) s
#else
#define YDBG(s)
#endif COUNT_BYTECODE
#ifdef COUNT_RISC_INSTR
#define CDBG(s) s
#else
#define CDBG(s)
#endif

#ifdef COUNT_RISC_INSTR
unsigned long long num_of_executed_risc_instrs[MAX_METHOD_NUMBER] = {0, };
Method *risc_count_method[MAX_METHOD_NUMBER] = {NULL, };
int risc_count_method_num = 0;
int total_risc_count_method_num = 0;
#endif COUNT_RISC_INSTR
#ifdef COUNT_BYTECODE
extern unsigned long long num_of_executed_bytecode_instrs[MAX_METHOD_NUMBER];
extern Method *bytecode_count_method[MAX_METHOD_NUMBER];
extern int bytecode_count_method_num;
#endif COUNT_BYTECODE

void
insert_probe_code( CFG* cfg, InstrNode* instr, int num_of_bb_instrs )
{
//
// The probe code is as follows :
//      bg      GREATER
//      nop
//      be      EQUAL
//      nop
//LESS :
//      sethi   %hi(&num_of_executed_risc_instrs), %g1
//      or      %g1, %lo(&num_of_executed_risc_instrs), %g1
//      ldd     [%g1], %g2
//      addcc   %g3, num_of_bb_instrs, %g3
//      addx    %g2, 0, %g2
//      std     %g2, [%g1]
//      subcc   %g0, 1, %g0
//      ba      END
//      nop
//GREATER :
//      sethi   %hi(&num_of_executed_risc_instrs), %g1
//      or      %g1, %lo(&num_of_executed_risc_instrs), %g1
//      ldd     [%g1], %g2
//      addcc   %g3, num_of_bb_instrs, %g3
//      addx    %g2, 0, %g2
//      std     %g2, [%g1]
//      subcc   %g0, -1, %g0
//      ba      END
//      nop
//EQUAL :
//      sethi   %hi(&num_of_executed_risc_instrs), %g1
//      or      %g1, %lo(&num_of_executed_risc_instrs), %g1
//      ldd     [%g1], %g2
//      addcc   %g3, num_of_bb_instrs, %g3
//      addx    %g2, 0, %g2
//      std     %g2, [%g1]
//      subcc   %g0, %g0, %g0
//      ba      END
//      nop
//END :
//
#ifdef COUNT_RISC_INSTR
    InstrNode*    new_instr;
    InstrNode *branch_instr1, *branch_instr2, *branch_instr3, *branch_instr4, *branch_instr5;

    assert( num_of_bb_instrs >= 0 );

    branch_instr1 = create_format3_instruction(BG, -5);
    insert_instruction_as_single_predecessor(cfg, instr, branch_instr1);
    disconnect_instruction(branch_instr1, instr);
    add_new_resolve_instr(cfg, branch_instr1, NULL);
    mark_visited(branch_instr1);

    new_instr = create_format2_instruction(SETHI, g0, 0, -5);
    insert_instruction_as_single_predecessor(cfg, branch_instr1, new_instr);
    set_delay_slot_instr( new_instr );
    mark_visited( new_instr );


    branch_instr2 = register_format3_instruction(cfg, branch_instr1, -5, BE);
    add_new_resolve_instr(cfg, branch_instr2, NULL);
    mark_visited(branch_instr2);

    new_instr = create_format2_instruction(SETHI, g0, 0, -5);
    insert_instruction_as_single_predecessor(cfg, branch_instr2, new_instr);
    set_delay_slot_instr(new_instr);
    mark_visited(new_instr);

// LESS :

    new_instr = register_format2_instruction(cfg, branch_instr2, -5, SETHI, g1, \
                                             ((unsigned) &num_of_executed_risc_instrs[risc_count_method_num]) >> 10);
    mark_visited(new_instr);

    new_instr = register_format5_instruction(cfg, new_instr, -5, OR, g1, g1, \
                                             ((unsigned) &num_of_executed_risc_instrs[risc_count_method_num]) & 0x3ff);
    mark_visited(new_instr);

    new_instr = register_format4_instruction(cfg, new_instr, -5, LDD, g2, g1, g0);
    mark_visited(new_instr);

    new_instr = register_format2_instruction(cfg, new_instr, -5, SETHI, g4, ((unsigned) num_of_bb_instrs) >> 10);
    mark_visited(new_instr);

    new_instr = register_format5_instruction(cfg, new_instr, -5, OR, g4, g4, ((unsigned) num_of_bb_instrs) & 0x3ff);
    mark_visited(new_instr);

    new_instr = register_format6_instruction(cfg, new_instr, -5, ADDCC, g3, g3, g4);
    mark_visited(new_instr);

    new_instr = register_format5_instruction(cfg, new_instr, -5, ADDX, g2, g2, 0);
    mark_visited(new_instr);

    new_instr = register_format7_instruction(cfg, new_instr, -5, STD, g2, g1, g0);
    mark_visited(new_instr);

    new_instr = register_format5_instruction(cfg, new_instr, -5, SUBCC, g0, g0, 1);
    mark_visited(new_instr);

    branch_instr3 = register_format3_instruction(cfg, new_instr, -5, BA);
    add_new_resolve_instr(cfg, branch_instr3, NULL);
    connect_instruction(branch_instr3, instr);
    mark_visited(branch_instr3);

    new_instr = create_format2_instruction(SETHI, g0, 0, -5);
    insert_instruction_as_single_predecessor(cfg, branch_instr3, new_instr);
    set_delay_slot_instr(new_instr);
    mark_visited(new_instr);

// GREATER :

    new_instr = register_format2_instruction(cfg, branch_instr1, -5, SETHI, g1, \
                                             ((unsigned) &num_of_executed_risc_instrs[risc_count_method_num]) >> 10);
    mark_visited(new_instr);

    new_instr = register_format5_instruction(cfg, new_instr, -5, OR, g1, g1, \
                                             ((unsigned) &num_of_executed_risc_instrs[risc_count_method_num]) & 0x3ff);
    mark_visited(new_instr);

    new_instr = register_format4_instruction(cfg, new_instr, -5, LDD, g2, g1, g0);
    mark_visited(new_instr);

    new_instr = register_format2_instruction(cfg, new_instr, -5, SETHI, g4, ((unsigned) num_of_bb_instrs) >> 10);
    mark_visited(new_instr);

    new_instr = register_format5_instruction(cfg, new_instr, -5, OR, g4, g4, ((unsigned) num_of_bb_instrs) & 0x3ff);
    mark_visited(new_instr);

    new_instr = register_format6_instruction(cfg, new_instr, -5, ADDCC, g3, g3, g4);
    mark_visited(new_instr);

    new_instr = register_format5_instruction(cfg, new_instr, -5, ADDX, g2, g2, 0);
    mark_visited(new_instr);

    new_instr = register_format7_instruction(cfg, new_instr, -5, STD, g2, g1, g0);
    mark_visited(new_instr);

    new_instr = register_format5_instruction(cfg, new_instr, -5, SUBCC, g0, g0, -1);
    mark_visited(new_instr);

    branch_instr4 = register_format3_instruction(cfg, new_instr, -5, BA);
    add_new_resolve_instr(cfg, branch_instr4, NULL);
    connect_instruction(branch_instr4, instr);
    mark_visited(branch_instr4);

    new_instr = create_format2_instruction(SETHI, g0, 0, -5);
    insert_instruction_as_single_predecessor(cfg, branch_instr4, new_instr);
    set_delay_slot_instr(new_instr);
    mark_visited(new_instr);

// EQUAL :
    new_instr = register_format2_instruction(cfg, branch_instr2, -5, SETHI, g1, \
                                             ((unsigned) &num_of_executed_risc_instrs[risc_count_method_num]) >> 10);
    mark_visited(new_instr);

    new_instr = register_format5_instruction(cfg, new_instr, -5, OR, g1, g1, \
                                             ((unsigned) &num_of_executed_risc_instrs[risc_count_method_num]) & 0x3ff);
    mark_visited(new_instr);

    new_instr = register_format4_instruction(cfg, new_instr, -5, LDD, g2, g1, g0);
    mark_visited(new_instr);

    new_instr = register_format2_instruction(cfg, new_instr, -5, SETHI, g4, ((unsigned) num_of_bb_instrs) >> 10);
    mark_visited(new_instr);

    new_instr = register_format5_instruction(cfg, new_instr, -5, OR, g4, g4, ((unsigned) num_of_bb_instrs) & 0x3ff);
    mark_visited(new_instr);

    new_instr = register_format6_instruction(cfg, new_instr, -5, ADDCC, g3, g3, g4);
    mark_visited(new_instr);

    new_instr = register_format5_instruction(cfg, new_instr, -5, ADDX, g2, g2, 0);
    mark_visited(new_instr);

    new_instr = register_format7_instruction(cfg, new_instr, -5, STD, g2, g1, g0);
    mark_visited(new_instr);

    new_instr = register_format5_instruction(cfg, new_instr, -5, SUBCC, g0, g0, 0);
    mark_visited(new_instr);

    branch_instr5 = register_format3_instruction(cfg, new_instr, -5, BA);
    add_new_resolve_instr(cfg, branch_instr5, NULL);
    connect_instruction(branch_instr5, instr);
    mark_visited(branch_instr5);

    new_instr = create_format2_instruction(SETHI, g0, 0, -5);
    insert_instruction_as_single_predecessor(cfg, branch_instr5, new_instr);
    set_delay_slot_instr(new_instr);
    mark_visited(new_instr);
#endif COUNT_RISC_INSTR
}

void
print_risc_count()
{
#if defined(COUNT_RISC_INSTR) || defined(COUNT_BYTECODE)
    int i, j;
    unsigned long long total_instrs;
    Method *method_tmp;
    unsigned long long num_tmp;
#endif /* COUNT_RISC_INSTR or COUNT_BYTECODE */
#ifdef COUNT_RISC_INSTR
    total_instrs = 0;
    printf("=== Counted RISC Instructions ===\n");
// sorting..
    for(i = 0; i < MAX_METHOD_NUMBER; i++) {
        for(j = i + 1; j < MAX_METHOD_NUMBER; j++) {
            if (num_of_executed_risc_instrs[i] < num_of_executed_risc_instrs[j]) {
                method_tmp = risc_count_method[i];
                risc_count_method[i] = risc_count_method[j];
                risc_count_method[j] = method_tmp;
                num_tmp = num_of_executed_risc_instrs[i];
                num_of_executed_risc_instrs[i] = num_of_executed_risc_instrs[j];
                num_of_executed_risc_instrs[j] = num_tmp;
            }
        }
    }
    for(i = 0; i < MAX_METHOD_NUMBER; i++) {
        if (risc_count_method[i] != NULL) {
            total_instrs += num_of_executed_risc_instrs[i];
            printf("%s %s %s : %llu\n", risc_count_method[i]->class->name->data, \
                   risc_count_method[i]->name->data, \
                   risc_count_method[i]->signature->data, \
                   num_of_executed_risc_instrs[i]);
        }
    }
	printf("total risc instructions : %llu\n", total_instrs);
#endif COUNT_RISC_INSTR
#ifdef COUNT_BYTECODE
    total_instrs = 0;
    printf("=== Counted Bytecode Instructions ===\n");
// sorting..
    for(i = 0; i < MAX_METHOD_NUMBER; i++) {
        for(j = i + 1; j < MAX_METHOD_NUMBER; j++) {
            if (num_of_executed_bytecode_instrs[i] < num_of_executed_bytecode_instrs[j]) {
                method_tmp = bytecode_count_method[i];
                bytecode_count_method[i] = bytecode_count_method[j];
                bytecode_count_method[j] = method_tmp;
                num_tmp = num_of_executed_bytecode_instrs[i];
                num_of_executed_bytecode_instrs[i] = num_of_executed_bytecode_instrs[j];
                num_of_executed_bytecode_instrs[j] = num_tmp;
            }
        }
    }
    for(i = 0; i < MAX_METHOD_NUMBER; i++) {
        if (bytecode_count_method[i] != NULL) {
            total_instrs += num_of_executed_bytecode_instrs[i];
            printf("%s %s %s : %llu\n", bytecode_count_method[i]->class->name->data, \
                   bytecode_count_method[i]->name->data, \
                   bytecode_count_method[i]->signature->data, \
                   num_of_executed_bytecode_instrs[i]);
        }
    }
    printf("total bytecode instructions : %llu\n", total_instrs);
#endif COUNT_BYTECODE
}

#ifdef COUNT_BYTECODE
unsigned 
GetHash(Method *method) {
    int Key = (int) method;
    return (Key >> 8) % MAX_METHOD_NUMBER;
}

unsigned
GetNewHash(Method *method) {
    int key_tmp, key_it;

    for(key_tmp = GetHash(method), key_it = 0; \
            (key_it != 1) || (key_tmp != GetHash(method)); key_tmp++) {
        if (key_tmp == MAX_METHOD_NUMBER) {
            key_it = 1;
            key_tmp = 0;
        }
        if (bytecode_count_method[key_tmp] == NULL) return key_tmp;
    }
    return -1;
}

unsigned
GetInHash(Method *method) {
    int key_tmp, key_it;

    for(key_tmp = GetHash(method), key_it = 0; \
            (key_it != 1) || (key_tmp != GetHash(method)); key_tmp++) {
        if (key_tmp == MAX_METHOD_NUMBER) {
            key_it = 1;
            key_tmp = 0;
        }
        if (bytecode_count_method[key_tmp] == method) return key_tmp;
    }
    return -1;
}
#endif COUNT_BYTECODE
#endif // NEW_COUNT_RISC
