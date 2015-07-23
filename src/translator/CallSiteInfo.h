#ifndef _CallSiteInfo_h_
#define _CallSiteInfo_h_

#include "gtypes.h"
#include "basic.h"

//
// `CallSiteInfo' structure
// - This structure will be an entry of `call_site_info_table'.
//
// typedef struct CallSiteInfo_t {
//     bpc; 
//     unsigned npcAndState;
//     struct CallSiteInfo_t* next; 
// } CallSiteInfo;
typedef unsigned CallSiteInfo;
 
//
// `CallSiteInfoTable' structure
// - A table of CallSiteInfo
//
// Implemented as an array. 
// I considered two choices:
// * array
// * open hash (8 byte unit?)
// Assuming (pessimisticly) there may be one call per bytecode 8 bytes, 
// an open hash will take 1 + 3 words per 8 bytes, while an array
// will consume 8 words per 8 bytes. But array implementation may be faster
// in general, and easy to implement. ^^
// So we adopted array implementation. 
//
typedef CallSiteInfo CallSiteInfoTable; 


void     CSIT_Insert(CallSiteInfoTable *csit, uint32 bpc, uint32 npc); 

enum { CSI_UNKNOWN = 0, CSI_IFINLINE, CSI_JMPL }; 
unsigned find_appropriate_virtual_call_implementation_type(Method *method, 
                                                           uint32 bpc); 

struct Hjava_lang_Class*  find_receiver_type(Method *method, uint32 bpc);

#ifdef CUSTOMIZATION
CallSiteInfo*      CSI_get_arbitrary_CSIT(Method* method);
#endif

#include "CallSiteInfo.ic"

#endif /* _CallSiteInfo_h_ */

