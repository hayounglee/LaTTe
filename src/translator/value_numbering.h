/* value_numbering.h
   
   Interface to value numbering
   
   Written by: Junpyo Lee <walker@masslab.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __VALUE_NUMBERING_H__
#define __VALUE_NUMBERING_H__

#include "fma.h"
#include "InstrNode.h"

/* Perform CSE in the region given entry node and initial map */
void VN_optimize_region(fma_session*, InstrNode*, a_status*);

#endif /* __VALUE_NUMBERING_H__ */
