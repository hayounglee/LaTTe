//
// File Name : probe.h
// Author    : Yang, Byung-Sun
// Description
//    This is a header file for probing.
//

#ifndef __PROBE_H__
#define __PROBE_H__

#include "CFG.h"
#include "InstrNode.h"

#define MAX_METHOD_NUMBER (2 * 1024)

extern unsigned long long     num_of_executed_risc_instrs[MAX_METHOD_NUMBER];


void   insert_probe_code( CFG* cfg, InstrNode* instr, int num_of_bb_instrs );
void   print_risc_count( void );
unsigned GetNewHash(Method *method);
unsigned GetInHash(Method *method);
unsigned GetRiscHashNO( Method *method, void *instr );
void IncreaseRiscCount( int hashNO );

#endif __PROBE_H__
