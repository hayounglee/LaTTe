/* exception_info.c
   
   The exception information table implemented as a closed hash table
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
               SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */
#include "config.h"
#include "method_inlining.h"
#include "exception_info.h"
#include "gc.h"
#include <stdio.h>
#include <assert.h>

#undef INLINE
#define INLINE
#include "exception_info.ic"

#define MALLOC    gc_malloc_fixed
#define FREE      gc_free_fixed

#define EDBG( s )    


//
// In current implementation, there will be only one global table in JVM.
//
//static ExceptionInfoTable    table;

//
// Now, there will be one table per method at a given time.
// - doner

ExceptionInfoTable*
EIT_alloc()
{
//    return &table;
    return (ExceptionInfoTable*) gc_malloc( sizeof( ExceptionInfoTable ),
                                            &gc_eit );
}


#define    DEFAULT_SIZE      61

void
EIT_init( ExceptionInfoTable* table, int initial_size )
{
    if ( initial_size == 0 ) {
        table->size = DEFAULT_SIZE;
    } else {
        table->size = initial_size;
    }

    table->numOfInfos = 0;

//    table->info = MALLOC( DEFAULT_SIZE * sizeof( ExceptionInfo ) );
//    bzero( table->info, DEFAULT_SIZE * sizeof( ExceptionInfo ) );
    table->info = gc_malloc( table->size * sizeof( ExceptionInfo ),
                             &gc_closed_hash_table );

    EDBG(
        printf( "exception info table(size = %d) is initialized.\n", table->size );
        printf( "The memory range is between %p and %p.\n",
                table->info, ((char*) table->info) + table->size * sizeof( ExceptionInfo ) );
        );
}





inline static
unsigned int
hash_val( uint32 native_pc, int table_size )
{
    return native_pc % (unsigned) table_size;
}



///
/// Function Name : EIT_find
/// Author : Yang, Byung-Sun
/// Input
///       table - exception information table
///       native_pc - key to the table
/// Output
///       If an entry corresponding to 'native_pc' exists,
///       a pointer to this will be returned.
///       If not, a pointer to a exception info structure whose native_pc is zero
///       will be returned. This can be used to insert a new exception information.
/// Pre-Condition
/// Post-Condition
/// Description
///
ExceptionInfo*
EIT_find( ExceptionInfoTable* table, uint32 native_pc )
{
    int     i = 0;
    unsigned int     cur_cell;

    if ( native_pc == 0 ) return NULL;

    cur_cell = hash_val( native_pc, table->size );

    EDBG( printf( "EIT: find - hash_val = %d\n", cur_cell ); );

    while ( table->info[cur_cell].nativePC != 0 &&
            table->info[cur_cell].nativePC != native_pc ) {
        cur_cell += 2 * (++i) - 1; // use quadratic probing (i^2)
        if (cur_cell >= table->size) {
            cur_cell -= table->size;
        }
    }

    EDBG( printf( "EIT: find - found cur_cell = %d\n", cur_cell ); );

    return &table->info[cur_cell];
}


#include "prime.h"

static
void
rehash( ExceptionInfoTable* table )
{
    ExceptionInfo*    old_info = table->info;
    int               old_size = table->size;
    int               i;
    ExceptionInfo*    info;

    table->size = get_next_prime( old_size * 2 );

    EDBG( printf( "rehashing : old size = %d, new size = %d\n",
                  old_size, table->size ); );

//    table->info = (ExceptionInfo*) MALLOC( table->size * sizeof( ExceptionInfo ) );
//    bzero( table->info, table->size * sizeof( ExceptionInfo ) );
    table->info =
	(ExceptionInfo*) gc_malloc( table->size * sizeof( ExceptionInfo ),
				    &gc_closed_hash_table );
    assert( table->info != NULL );

    for (i = 0; i < old_size; i++) {
        if (old_info[i].nativePC != 0) {
            info = EIT_find( table, old_info[i].nativePC );
            assert( info->nativePC == 0 );
            *info = old_info[i];
        }
    }
//    gc_free ( old_info );
//    FREE( old_info );
}



void
EIT_insert( ExceptionInfoTable  *table,
            uint32              native_pc,
            uint32              bytecode_pc,
            int                 *var_map,
            MethodInstance      *instance ) {
    ExceptionInfo*    info;

    if ( (table->numOfInfos++) * 5 > table->size * 3 ) { // load factor is 0.6
        rehash( table );
    }

    EDBG( printf( "EIT:insert (%x, %d)\n", native_pc, bytecode_pc ); );

    info = EIT_find( table, native_pc );
    info->nativePC = native_pc;
    info->bytecodePC = bytecode_pc;
    info->varMap = var_map;
    info->methodInstance = instance;
}
