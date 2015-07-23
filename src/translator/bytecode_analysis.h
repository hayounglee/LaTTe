/* bytecode_analysis.h
   
   Header file for bytecode analysis
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
               SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __BYTECODE_ANALYSIS_H__
#define __BYTECODE_ANALYSIS_H__

/* some structure declaration for avaid warning */
struct _methods;

void BA_bytecode_analysis(struct _methods *method);

#endif /* __BYTECODE_ANALYSIS_H__ */
