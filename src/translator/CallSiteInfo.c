#include <assert.h>
#include "config.h"
#include "classMethod.h"
#include "CallSiteInfo.h"
#include "SPARC_instr.h"
#include "method_inlining.h"
#include "TypeChecker.h"
#include "translator_driver.h"
#ifdef CUSTOMIZATION
#include "SpecializedMethod.h"
#endif


//////////////////////////////////////////////////////////////////////
// static utility functions
//////////////////////////////////////////////////////////////////////

static inline
Method*
find_called_method_from_call_site( unsigned* call_site_addr )
{
    unsigned disp30;
    unsigned* called_method_start_addr;
    Method* method;

    assert( ((*call_site_addr) & CALL) == CALL );

    // low 30bits of call op. is immediate field
    disp30 = (*call_site_addr) & 0x3fffffff;

    // target address of call op. = current pc + 4*disp30
    called_method_start_addr = call_site_addr + disp30;

    method = (Method*)*(called_method_start_addr-1);

    return method;
}

static inline
Hjava_lang_Class*
find_receiver_type_from_call_site( unsigned* call_site_addr )
{
    unsigned disp30;
    unsigned* called_method_start_addr;
    Hjava_lang_Class* type;

    assert( ((*call_site_addr) & CALL) == CALL );

    // low 30bits of call op. is immediate field
    disp30 = (*call_site_addr) & 0x3fffffff;

    // target address of call op. = current pc + 4*disp30
    called_method_start_addr = call_site_addr + disp30;

    type = (Hjava_lang_Class*)*(called_method_start_addr - 2);

    return type;
}


static inline
int
get_inline_cache_status( unsigned* call_site_addr )
{
    unsigned call_op = *call_site_addr;

    //
    // the operation at call_site_addr is
    // 1. call
    //     if (target address of the call == 'dispatch_method')
    //       return 0
    //     else 
    //       return 1
    // 2. jmpl
    //     return 2
    //
    if ( (call_op & CALL) == CALL ) {
        // calculate target address
        unsigned* target_addr = call_site_addr + (call_op & 0x3fffffff);

        if ( target_addr == (unsigned*)dispatch_method_with_inlinecache ) 
          return 0; 
        else 
          return 1;

    } else { 
        assert ( (call_op & JMPL) == JMPL );
        
        return 2;
    }

}



//////////////////////////////////////////////////////////////////////
// static functions
//////////////////////////////////////////////////////////////////////

static inline
unsigned 
CSI_GetState( CallSiteInfo* csi )
{
    return *csi & 0x3; 
}

static inline
void* 
CSI_GetPointer( CallSiteInfo* csi )
{
    return (void*)(*csi & 0xfffffffc); 
}

static inline
void 
CSI_SetState( CallSiteInfo* csi, unsigned new_state )
{
    assert( new_state < 4 ); 
    *csi = (CallSiteInfo)CSI_GetPointer( csi ) | new_state; 
}

static inline
void 
CSI_SetPointer( CallSiteInfo* csi, void* pointer )
{
    assert( (unsigned)pointer % 4 == 0 ); 
    *csi = (CallSiteInfo)pointer | CSI_GetState( csi );
}

static inline
CallSiteInfo* 
CSIT_Find( CallSiteInfoTable *csit, uint32 bpc )
{
    CallSiteInfo* result; 

    assert( csit ); 
    assert( bpc >= 0 ); 

    result = &csit[bpc]; 
// CSIT_Find() can be called in CSIT_Insert(). 
//     assert( CSI_GetPointer(result) != 0 
//             && (unsigned)CSI_GetPointer( result ) % 4 == 0 );
    assert( CSI_GetState( result ) != 3 ); 

    return result; 
}


/* Name        : CSIT_Insert
   Description : insert call site address to an CSIT. 
   Maintainer  : Suhyun Kim <zelo@i.am>
   Pre-condition:
   
   Post-condition:
   
   Notes:
   *** about when to be called ***
   This function should be called for every call site whose address is
   updated. That is, every point CSIT_find will be called ?

   Currently, this needs to be called only for "call dispatch_method".
   But for those call sites in inlined methods or methods other than the
   outermost method, this function need not and should not be called
   since there will be no reference. */
void
CSIT_Insert( CallSiteInfoTable *csit, uint32 bpc, uint32 npc )
{
    assert( csit ); 
    assert( bpc >= 0 ); 
    assert( npc != 0 && npc % 4 == 0 ); 
    assert( CSI_GetState( CSIT_Find( csit, bpc ) ) == 0 ); 
    // CSI_SetPointer( CSIT_Find( csit, bpc ), npc ); 
    csit[bpc] = npc;    // a simpler form ^^
}


/*
bpc -> virtual call implementation type

This function is called only for calls that are not statically resolved.

if( There is no data ){
    // call dispatch_method
    keep inline cache; 
} else if( monomorphic site so far: biased to only one method ){
    // any other call
    if( appropriate to inline ){
        if-inline; 
    } else {
        use ld-ld-jmpl;
    }
} else { // polymorphic site(more than one)
    // jmpl
    use ld-ld-jmpl; 
}


*/

/* Name        : find_appropriate_virtual_call_implementation_type
   Description : bpc -> virtual call implementation type
   Maintainer  : Suhyun Kim <zelo@i.am>
   Pre-condition:
       This function is called only for calls that are not statically resolved.
   Post-condition:
       Return Value: 
       if 0(CSI_UNKNOWN), use inline cache,
       else if CSI_JMPL, use ld-ld-jmpl,
       else a pointer to Method to if-inline.    
   Notes:  */
unsigned
find_appropriate_virtual_call_implementation_type( Method *method, uint32 bpc )
{
    CallSiteInfo* call_site_info; 
    unsigned state; 

    // if not using adaptive compilation 
    // all virtual call uses virtual method table
//    if ( flag_adapt == 0 ) return CSI_JMPL;

    if ( method->callSiteInfoTable == NULL ) return CSI_UNKNOWN;

    call_site_info = CSIT_Find( method->callSiteInfoTable, bpc ); 
    state = CSI_GetState( call_site_info ); 

    if( state == CSI_UNKNOWN ){
        unsigned new_state = -1;    // to avoid uninitialization warning
        unsigned *call_site_addr;

        call_site_addr = (unsigned*)CSI_GetPointer( call_site_info ); 

        if( call_site_addr == NULL ){
            // access before call_site_addr update
            return CSI_UNKNOWN; 
        }

        switch( get_inline_cache_status( call_site_addr ) ){
          // case "call dispatch_table":
          case 0:
            return CSI_UNKNOWN; 
            break; 

          // case "other calls":
          case 1: // single target...
          {
            new_state = CSI_IFINLINE; 
            break; 
          }

          // case "jmpl":
          case 2: // multiple target...
            new_state = CSI_JMPL;
            break; 
          default:
            assert(0);
            break; 
        }

        CSI_SetState( call_site_info, new_state ); 
        state = new_state; 
    } 
    else if ( state == CSI_IFINLINE ) {
        unsigned *call_site_addr;

        call_site_addr = (unsigned*)CSI_GetPointer( call_site_info ); 
        if ( get_inline_cache_status( call_site_addr ) == 2 ){// change to jmpl
            CSI_SetState( call_site_info, CSI_JMPL ); 
            state = CSI_JMPL;
        }
    }

    if( state == CSI_IFINLINE ){
        return (unsigned)find_called_method_from_call_site( (unsigned*)CSI_GetPointer( call_site_info ) );
    } else {
        assert( state == CSI_JMPL ); 
        return state; 
    }
}

Hjava_lang_Class* 
find_receiver_type(Method *method, uint32 bpc)
{
    CallSiteInfo* call_site_info; 

    if ( method->callSiteInfoTable == NULL ) return NULL;
    call_site_info = CSIT_Find( method->callSiteInfoTable, bpc ); 

    if( CSI_GetState( call_site_info ) != CSI_IFINLINE )
      return NULL;

    return find_receiver_type_from_call_site( (unsigned*)CSI_GetPointer( call_site_info ) );
}

#ifdef CUSTOMIZATION
CallSiteInfo*
CSI_get_arbitrary_CSIT(Method* method) 
{
    SpecializedMethodSet* sms = method->sms;

    if (SMS_GetSize(sms) == 0) 
      return NULL;
    else 
      return SMS_GetIdxElement(sms,0)->callSiteInfoTable;
}
#endif
