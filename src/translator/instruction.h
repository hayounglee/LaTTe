#include "default.h"



void* get_h(void* c_instr);

void set_h(void* c_instr, void* itable);

void *get_successor(void* c_instr, int n);

int get_number_of_successor(void* c_instr);

int get_destination_register(void* c_instr);

void set_destination_register(void* c_instr, int dest_reg);

int get_source_register(void* c_instr, int n);
    
void set_source_register(void* c_instr, int n, int src_reg);

bool is_copy(void* c_instr);

bool is_return(void* c_instr);

bool is_call(void* c_instr);

int get_preferred_assignment(void* c_instr);

void set_preferred_assignment(void* c_instr, int preferred_dest);

bool is_join(void* c_instr);

bool is_last_use(void* c_instr, int reg);
