/* register_allocator.h
   
   header file for the register allocator
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties.
 */

#ifndef __REGISTER_ALLOCATOR_H__
#define __REGISTER_ALLOCATOR_H__
struct CFG;
struct TranslationInfo;
struct AllocStat;
struct InstrNode;

void RegAlloc_allocate_registers_on_CFG(struct CFG *cfg, 
                                        struct AllocStat *init_map); 
void RegAlloc_make_exception_return_map(struct TranslationInfo *info);

struct AllocStat* 
RegAlloc_perform_precoloring(struct CFG *cfg, struct TranslationInfo *info);

void RegAlloc_register_region_dummy(struct InstrNode* dummy);

InstrNode*
RegAlloc_insert_region_dummy(CFG* cfg, InstrNode* region_header);

#endif // __REGISTER_ALLOCATOR_H__
