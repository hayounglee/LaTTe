/* AllocStat.h
   
   data structures for the status of register allocation
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties.
 */

#ifndef __ALLOCSTAT_H__
#define __ALLOCSTAT_H__

#include "basic.h"


//
// map from a variable to a physical register
//
typedef struct map {
    int *map;
} map;

typedef struct AllocStat {	/* data structure for allocation status */
    int * map;			/* variable number --> physical register */
    int ** rmap;		/* physical register -> variables */
    unsigned int * refcount;
} AllocStat;

map*               map_Init( map* h );
map*               map_alloc();
map*               map_Clone_from_alloc_status( AllocStat* h );
map*               map_Clone( map* h );
void               map_Insert( map* h, int v, int p );
int                map_Find( map* h, int v );
void               map_Erase( map* h, int v );
map*               map_merge( map* h1, map* h2 );


AllocStat*          AllocStat_Init( AllocStat* h );
AllocStat*          AllocStat_InitBy_gc_malloc_fixed( AllocStat* h );
AllocStat*          AllocStat_alloc();
AllocStat*          AllocStat_alloc_by_gc_malloc_fixed();
AllocStat*          AllocStat_Clone( AllocStat* h );
bool               AllocStat_IsEqual( AllocStat* h1, AllocStat* h2 );
int                AllocStat_FindRMap( AllocStat* h, int r, int cnt );
void               AllocStat_AppendRMap( AllocStat* h, int v, int r );
void               AllocStat_RemoveRMap( AllocStat* h, int v, int r );
void               AllocStat_Insert( AllocStat* h, int v, int r );
int                AllocStat_Find( AllocStat* h, int v );
void               AllocStat_Erase( AllocStat* h, int v );
int                AllocStat_GetRefCount( AllocStat* h, int r );
int*               AllocStat_Find_variables( AllocStat* h, int r );
void               AllocStat_ChangeMappedReg( AllocStat* h, int v, int r );
void               AllocStat_ChangeReg( AllocStat* h, int r1, int r2 );
void               AllocStat_DumpMap( AllocStat* h );
void               AllocStat_DumpRMap( AllocStat* h );

#ifdef JP_UNFAVORED_DEST
int                AllocStat_FindFavoredFreeReg( AllocStat* h, VarType type,
						 uint32* unfavored_dest );
#endif

int                AllocStat_FindFreeReg( AllocStat* h, VarType type );
int                AllocStat_FindFreeSpill( AllocStat* h, VarType type );
int                AllocStat_FindFreeStorage( AllocStat* h, VarType type );
int                AllocStat_FindFreeNonCallerSaveReg( AllocStat* h,
						       VarType type );
bool               AllocStat_CheckValidity( AllocStat* h );

void               AllocStat_RemoveDeadVars( AllocStat* h, InstrNode* instr );

#include "AllocStat.ic"

#endif // !__ALLOCSTAT_H__
