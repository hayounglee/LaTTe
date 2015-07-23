//
// File Name : virtual_call_profile.c
// Author : hacker
// Description
//    contains various functions related virtual call profile
//

#include "config.h"
#include "virtual_call_profile.h"

CallSite call_sites[MAX_VIRTUAL_CALL_NUM];
int current_call_site_no = 0;

int
get_new_method_id(Method *caller, int bpc) {
    int i;

    call_sites[current_call_site_no].method = caller;
    call_sites[current_call_site_no].bpc = bpc;
    call_sites[current_call_site_no].list = NULL;

    return current_call_site_no++;
}

void
update_virtual_call_info(int call_site_id, unsigned *target_addr) {
    if ((*target_addr & 0xc1f80000) & SPARC_SAVE) {
        TargetList *list_node;

        for(list_node = call_sites[call_site_id].list; list_node != NULL;
            list_node = list_node->next) {
            if (target_addr == list_node->addr) return;
        }

        list_node = (TargetList *) gc_malloc_fixed(sizeof(TargetList));
        list_node->addr = target_addr;
        list_node->next = call_sites[call_site_id].list;
        call_sites[call_site_id].list = list_node;
    }
}

void
print_virtual_call_info(void) {
    int i;

    fprintf(stderr, "call site class name sig : bpc : count\n");

    for(i = 0; i < current_call_site_no; i++) {
        int count;
        TargetList *list_node;

        for(count = 0, list_node = call_sites[i].list; list_node != NULL;
            count++, list_node = list_node->next);
        
        fprintf(stderr, "call site %s %s %s : %d : %d\n",
                call_sites[i].method->class->name->data,
                call_sites[i].method->name->data,
                call_sites[i].method->signature->data,
                call_sites[i].bpc,
                count);
    }
}
