

/* AllocStat.c
   
   functions to manipulate the register allocation status
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties.
 */

#include <stdio.h>
#include <strings.h>
#include <assert.h>
#include "reg.h"
#include "utils.h"
#include "fast_mem_allocator.h"
#include "CFG.h"
#include "flags.h"
#include "AllocStat.h"
#include "reg_alloc_util.h"
#include "bit_vector.h"


extern CFG *cfg;

extern char dirty[];


#undef INLINE
#define INLINE
#include "AllocStat.ic"


map*
map_merge(map *h1, map *h2)
{
    int i;

    if ( h1 == NULL && h2 == NULL ) {
        return NULL;
    } else if ( h1 == NULL && h2 != NULL ) {
        return h2;
    } else if ( h1 != NULL && h2 == NULL ) {
        return h1;
    };

    for(i = 0; i < CFG_GetNumOfVars(cfg); i++) {
        if ( h1->map[i] >= 0 ) {
            h2->map[i] = h1->map[i];
        };
    };
    return h2;
};


//
// map and reverse map
//
AllocStat*
AllocStat_Init(AllocStat *h)
{
    int i;

    assert(h);

    h->map = (int *)FMA_calloc( CFG_GetNumOfVars(cfg) * sizeof(int) );
    h->rmap = (int **)FMA_calloc( CFG_GetNumOfStorageLocations(cfg)
			       * sizeof(int *) );
    h->refcount = (unsigned int *)
        FMA_calloc( CFG_GetNumOfStorageLocations(cfg) * sizeof(unsigned int));

    for(i = 0; i < CFG_GetNumOfVars(cfg); i++) {
        h->map[i] = -1;
    };
    return h;
};

AllocStat* 
AllocStat_InitBy_gc_malloc_fixed(AllocStat *h)
{
    int i;

    assert(h);

    h->map = (int *)gc_malloc_fixed( CFG_GetNumOfVars(cfg)
				     * sizeof(int) );
    h->rmap = (int **)
        gc_malloc_fixed( CFG_GetNumOfStorageLocations(cfg) * sizeof(int *) );
    h->refcount = (unsigned int *)
        gc_malloc_fixed( CFG_GetNumOfStorageLocations(cfg)
			 * sizeof(unsigned int) );

    for(i = 0; i < CFG_GetNumOfVars(cfg); i++) {
        h->map[i] = -1;
    };
    return h;
}

AllocStat*
AllocStat_Clone(AllocStat *h)
{
    int i;
    AllocStat * ret = AllocStat_alloc();

    assert(h);

    bcopy(h, ret, sizeof(AllocStat));
    AllocStat_Init(ret);
    bcopy(h->map, ret->map, sizeof(int) * CFG_GetNumOfVars(cfg) );
    bcopy(h->refcount, ret->refcount,
          sizeof(unsigned int) * CFG_GetNumOfStorageLocations(cfg));

    for(i = 0; i < CFG_GetNumOfStorageLocations(cfg); i++) {
        if ( h->refcount[i] == 1 ) {
            ret->rmap[i] = h->rmap[i];
        } else if ( h->refcount[i] > 1 ) {
            //
            // modified by doner
            // - For FVOID, refcount can be more than 1.
            //   But map and rmap do not contain any entries for it.
            //
            if (h->rmap[i]) {

                ret->rmap[i] = (int *)FMA_calloc( sizeof(int) *
                                               h->refcount[i] );
                bcopy(h->rmap[i], ret->rmap[i], 
                      sizeof(int) * h->refcount[i] );
            } else {
                assert( h->refcount[i] == h->refcount[i-1] ); // FVOID goes with DOUBLE.
            };
        };
    };

    return ret;
};

bool 
AllocStat_IsEqual( AllocStat* h1, AllocStat* h2 )
{
    int i;

    assert( h1 && h2 );

    for( i = 0; i < CFG_GetNumOfVars(cfg); ++i ){
        if( h1->map[i] != h2->map[i] ) return false;
    }

    for( i = 0; i < CFG_GetNumOfStorageLocations(cfg); ++i ){
        if( h1->refcount[i] != h2->refcount[i] ) return false; 

        if( h1->refcount[i] == 1 ){
            if( h1->rmap[i] != h2->rmap[i] ) return false;
        } else if( h1->refcount[i] > 1 ){
            // - For FVOID, refcount can be more than 1.
            //   But map and rmap do not contain any entries for it.
            if( h1->rmap[i] ){
                int j;
                if( ! h2->rmap[i] ) return false; 
                for( j = 0; j < h1->refcount[i]; ++j ){
                    if( h1->rmap[i][j] != h2->rmap[i][j] ) return false; 
                }
            } else {
                if( h2->rmap[i] ) return false; 
            }
        }
    }

    return true;
} 

int
AllocStat_FindRMap(AllocStat *h, int r, int cnt)
{
    assert( (unsigned)r < CFG_GetNumOfStorageLocations(cfg) );
    assert( cnt < h->refcount[r] );

    //
    // modified by doner
    // - consider FVOID
    //
    switch( h->refcount[r] ) {
      case 0:
        return -1;

      case 1:
        if ( h->rmap[r] ) {
            return (int)h->rmap[r];
        } else {
            return -1;
        };

      default:
        if ( h->rmap[r] ) {
            return (h->rmap[r])[cnt];
        } else {
            return -1;
        };
    };
    return -1;
};

void
AllocStat_AppendRMap(AllocStat *h, int v, int r)
{
    int *t;

    assert( CFG_get_var_number(v) >= 0
            && CFG_get_var_number(v) < CFG_GetNumOfVars(cfg) );
    assert( r > 0 && r < CFG_GetNumOfStorageLocations(cfg) );

    //
    // inserted by doner
    // - T_FVOID should not be in 'h' map.
    //
    assert( Var_GetType( v ) != T_FVOID );


    switch( h->refcount[r] ) {
      case 0:
        h->rmap[r] = (int *)v;
        break;
      case 1:
        t = (int *)FMA_calloc( sizeof(int) * 2 );
        t[0] = (int) h->rmap[r];
        t[1] = v;
        h->rmap[r] = t;
        break;
      default:
        t = (int *)FMA_calloc( sizeof(int) * (h->refcount[r] + 1) );
        bcopy( h->rmap[r], t, sizeof(int) * h->refcount[r] );
        t[ h->refcount[r] ] = v;
        h->rmap[r] = t;
    };

    h->refcount[r]++;

    //
    // appended by doner
    // - refcount of T_FVOID should be considered when T_DOUBLE is considered.
    // Why? - to find free spill register and etc.
    //
    /* increase the reference count of pair register */
    if (Var_GetType( v ) == T_DOUBLE) {
        h->refcount[r+1]++;
        assert(h->refcount[r] == h->refcount[r+1]);
    }
};

void
AllocStat_RemoveRMap(AllocStat *h, int v, int r)
{
    int *t, i;
    int v_in_rmap = -1;
    int v_var_number;

    assert( CFG_get_var_number(v) >= 0 && 
            CFG_get_var_number(v) < CFG_GetNumOfVars(cfg) );
    assert( r > 0 && r < CFG_GetNumOfStorageLocations(cfg) );
    assert( h->refcount[r] > 0 );

    if ( h->refcount[r] == 1 ) {
        v_in_rmap = (int)h->rmap[r];
        h->rmap[r] = 0;
    } else if ( h->refcount[r] == 2 ) { 
        v_var_number = CFG_get_var_number(v);
        if ( CFG_get_var_number((h->rmap[r])[0]) == v_var_number ) {
            v_in_rmap = (h->rmap[r])[0];
            h->rmap[r] = (int *)((h->rmap[r])[1]);
        } else {
            v_in_rmap = (h->rmap[r])[1];
            h->rmap[r] = (int *)((h->rmap[r])[0]);
        };
    } else if ( h->refcount[r] > 2 ) {
        t = h->rmap[r];
        v_var_number = CFG_get_var_number(v);
        for(i = 0; i < h->refcount[r]; i++) {
          if ( CFG_get_var_number(t[i]) == v_var_number ) {
              v_in_rmap = t[i];
              if ( i < h->refcount[r] - 1 ) {
                  bcopy(& t[i + 1], & t[i],
                         (h->refcount[r] - i - 1) * sizeof(int));
              };
              break;
          };
        };
    };

    h->refcount[r]--;

    //
    // appended by doner
    // - T_FVOID should be considered when T_DOUBLE is considered.
    //
    if (Var_GetType( v_in_rmap ) == T_DOUBLE) {
        h->refcount[r+1]--;
        assert(h->refcount[r] == h->refcount[r+1]);
    };
};

void
AllocStat_Insert(AllocStat *h, int v, int r)
{
    assert( CFG_get_var_number(v) >= 0
            && CFG_get_var_number(v) < CFG_GetNumOfVars(cfg) );
    assert( r > 0 && r < CFG_GetNumOfStorageLocations(cfg) );

    //
    // inserted by doner
    // - T_FVOID should not be in 'h' map.
    //
    assert( Var_GetType( v ) != T_FVOID );

    if ( h->map[ CFG_get_var_number(v) ] > 0 ) {
        AllocStat_RemoveRMap(h, v, h->map[ CFG_get_var_number(v) ] );
    };
    h->map[CFG_get_var_number(v)] = r;
    AllocStat_AppendRMap(h, v, r);

    dirty[r] = 1; // for callee save register. Not used now.
};


/* copy r1->r2 */
void
AllocStat_ChangeReg(AllocStat *h, int r1, int r2)
{
    int i = 0, *t;

    assert( Reg_IsRegister(r1) && Reg_IsRegister(r2) );

    assert( AllocStat_GetRefCount(h, r1) > 0 );
    assert( AllocStat_GetRefCount(h, r2) == 0 );

    h->rmap[r2] = h->rmap[r1];
    h->rmap[r1] = NULL;
    h->refcount[r2] = h->refcount[r1];
    h->refcount[r1] = 0;

    t = AllocStat_Find_variables(h, r2);

    /* double check */
    if (Var_GetType(t[0]) == T_DOUBLE) {
        /* modify rmap and refcount for odd-numbered reg of double */
        h->refcount[r2+1] = h->refcount[r1+1];
        h->refcount[r1+1] = 0;
        assert(h->refcount[r2] == h->refcount[r2+1]);
    }

    for(i = 0; i < h->refcount[r2]; i++) {
        assert( h->map[ CFG_get_var_number(t[i]) ] == r1 );
        h->map[ CFG_get_var_number(t[i]) ] = r2;
    };    

    assert( AllocStat_GetRefCount(h, r1) == 0 );
    assert( AllocStat_GetRefCount(h, r2) > 0 );
};


void
AllocStat_DumpMap(AllocStat *h)
{
    int i;

    for(i = 0; i < CFG_GetNumOfVars(cfg); i++) {
        if ( h->map[i] > 0 ) {
            printf("%d -> %s(%d)\n",
                   i,
                   Var_get_var_reg_name( h->map[i] ),
                   h->map[i] );
        };
    };
};

void
AllocStat_DumpRMap(AllocStat *h)
{
    int r;

    for(r = 1; r < CFG_GetNumOfStorageLocations(cfg); r++) {
        if ( h->refcount[r] && Reg_IsAvailable(r) ) {
            int i = 0;
            printf("%s -> ", Var_get_var_reg_name(r) );
            while( i < h->refcount[r] ) {
                printf("%s ", Var_get_var_reg_name(AllocStat_FindRMap(h, r, i++)));
            };
            printf("\n");
        };
    };
};

#ifdef JP_UNFAVORED_DEST
int
AllocStat_FindFavoredFreeReg(AllocStat *h, VarType type,
			     word* unfavored_dest) 
{
    int r;

    switch( type ) {
      case T_INT: case T_REF: case T_IVOID: 
        // Prefer local registers for random selection.
        // local, caller save, callee save 
        // Assume SPARC register window.
        // so, there's no callee save register.
        for(r = l0; r <= l7; r++) {
            if ( h->refcount[r] == 0 && 
                 !is_set(unfavored_dest, r)) return r;
        };
        for(r = i0; r < i6; r++) {
            if ( h->refcount[r] == 0 && 
                 !is_set(unfavored_dest, r)) return r;
        };
        for(r = o0; r < o6; r++) {
            if ( h->refcount[r] == 0 && 
                 !is_set(unfavored_dest, r)) return r;
        };
        break;
      case T_LONG:
        for(r = l0; r < l7; r += 2) {
            if ( h->refcount[r] == 0 && 
                 !is_set(unfavored_dest, r)) return r;
        };
        for(r = i0; r < i5; r += 2) {
            if ( h->refcount[r] == 0 && 
                 !is_set(unfavored_dest, r)) return r;
        };
        for(r = o0; r < o5; r += 2) {
            if ( h->refcount[r] == 0 && 
                 !is_set(unfavored_dest, r)) return r;
        };
        break;
      case T_FLOAT: case T_FVOID:
        for(r = f0; r <= f31; r++) {
            if ( h->refcount[r] == 0 && 
                 !is_set(unfavored_dest, r)) return r;
        };
        break;
      case T_DOUBLE:
        for(r = f0; r <= f30; r += 2 ) {
            if ( h->refcount[r] == 0 && h->refcount[r + 1] == 0 &&
                 !is_set(unfavored_dest, r) && !is_set(unfavored_dest, r)) 
              return r;
        };
        break;
      default:
        assert(0 && "register type unknown");

    };
    return -1;
};
#endif

int
AllocStat_FindFreeReg(AllocStat *h, VarType type)
{
    int r;

    switch( type ) {
      case T_INT: case T_REF: case T_IVOID: case T_LONG:
        // Prefer local registers for random selection.
        // local, caller save, callee save 
        // Assume SPARC register window.
        // so, there's no callee save register.
        for(r = l0; r <= l7; r++) {
            if ( h->refcount[r] == 0 ) return r;
        };
        for(r = i0; r < i6; r++) {
            if ( h->refcount[r] == 0 ) return r;
        };
        for(r = o0; r < o6; r++) {
            if ( h->refcount[r] == 0 ) return r;
        };
        break;
      case T_FLOAT: case T_FVOID:
        for(r = f0; r <= f31; r++) {
            if ( h->refcount[r] == 0 ) return r;
        };
        break;
      case T_DOUBLE:
        for(r = f0; r <= f31; r += 2 ) {
            /* Yes, check the pair by refering to the reference count */
            if ( h->refcount[r] == 0 && h->refcount[r + 1] == 0 ) return r;
        };
        break;
      default:
        assert(0 && "register type unknown");

    };
    return -1;
};

int
AllocStat_FindFreeSpill(AllocStat *h, VarType type)
{
    int r;
    switch( type ) {
      case T_INT: case T_REF: case T_LONG: case T_IVOID:
      case T_FLOAT: case T_FVOID: 
        for(r = Reg_number;
            r < CFG_GetNumOfStorageLocations(cfg); r++) 
        {
            if ( h->refcount[r] == 0 ) 
              return r;
        };
        break;
      case T_DOUBLE: 
        // We can use the spill location from number_of_register + 1 (really?)
        r = Reg_number;
        if ( RegAlloc_get_spill_offset(r + 1) % 8 != 0 ) r++;
        for(; r < CFG_GetNumOfStorageLocations(cfg) - 1;
            r += 2 ) {
            /* Yes, check the pair for the memory location */
            if ( h->refcount[r] == 0  && h->refcount[r + 1] == 0) {
                assert( RegAlloc_get_spill_offset(r + 1) % 8 == 0);
                return r;
            };
        };
        break;
      default:
        assert(0 && "register type unknown");
    };

    assert( 0 && "internal error:: no more spill register");
    return -1;
};


int
AllocStat_FindFreeNonCallerSaveReg(AllocStat *h, VarType t)
{
    int r;
    switch( t ) {
      case T_INT: case T_REF: case T_LONG: case T_IVOID:
        /* Prefer local registers for random selection. */
        /* local, caller save, callee save */
        /* I'm not sure that we would prefer caller than callee. */
        for(r = l0; r <= l7; r++) {
            if ( h->refcount[r] == 0 ) return r;
        };
        for(r = i0; r < i6; r++) {
            if ( h->refcount[r] == 0 ) return r;
        };
        break;
      case T_FLOAT: case T_DOUBLE: case T_FVOID:
        return -1;
      default:
        assert(0);
    };

    return -1;
};

bool
AllocStat_CheckValidity(AllocStat *h)
{
    int  r, i, var = 0;  
    bool is_double;

    for(r = 0; r < Reg_number; r++) {
        if ( Reg_IsAvailable(r) ) {
            is_double = false;
            for(i = 0; i < AllocStat_GetRefCount(h,r); i++) {
                if ( r < f0 && 
                     (var=AllocStat_Find( h, AllocStat_FindRMap( h, r, i ) )) != r ) return false;
                else if (Var_GetType(var)==T_DOUBLE) is_double = true;
            };
            /* skip double 2n+1 */
            if (is_double) r++;
        };
    };
    for(r = Reg_number; r < CFG_GetNumOfStorageLocations(cfg); r++) {
        is_double = false;
        for(i = 0; i < AllocStat_GetRefCount(h,r); i++) {
            if ( AllocStat_Find( h, (var=AllocStat_FindRMap(h, r, i))) != r ) return false;
            else if (Var_GetType(var)==T_DOUBLE) is_double = true; 
        };
        /* skip double 2n+1 */
        if (is_double) r++;
    };
    return true;
};

void
AllocStat_RemoveDeadVars(AllocStat *h, InstrNode * instr)
{
    int i;
    word * lv = Instr_GetLive(instr);

    if ( lv != NULL ) {
        for( i = 0; i < CFG_GetNumOfVars(cfg); i++) {
            if ( ! BV_IsSet( lv, i ) && AllocStat_Find( h, i ) != -1 ) {
                AllocStat_Erase(h, i);            
            };
        };
    };
};
