/* virtual_call_profile.h
   contains various data structures related virtual call profile
   
   Written by: Suhyun Kim <zelo@i.am>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#include "classMethod.h"
#include "SPARC_instr.h"
#include <stdio.h>

#ifndef _VIRTUAL_CALL_PROFILE_H_
#define _VIRTUAL_CALL_PROFILE_H_

#define MAX_VIRTUAL_CALL_NUM (1024 * 1024)

typedef struct TargetList_t {
    unsigned *addr;
    struct TargetList_t *next;
} TargetList;

typedef struct CallSite_t {
    Method *method;
    int bpc;
    TargetList *list;
} CallSite;

int get_new_method_id(Method *caller, int bpc);
void update_virtual_call_info(int call_site_id, unsigned *target_addr);
void print_virtual_call_info(void);

#endif _VIRTUAL_CALL_PROFILE_H_

