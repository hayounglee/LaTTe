/* live_analysis.h
   
   Interface to live analysis & dead code elimination
   
   Written by: Junpyo Lee <walker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */
#ifndef __LIVE_ANALYSIS_H__
#define __LIVE_ANALYSIS_H__

#include "sma.h"

struct CFG;
struct InlineGraph;

/* Perform live analysis for the whole CFG given root node */
void LA_live_analysis(InstrNode *);

/* delete any dead code in the region */
void LA_eliminate_dead_code_in_region(SMA_Session*,InstrNode *);

/* make variables that are used in inlined method dead at exit */
void LA_attach_live_info_for_inlining(struct CFG *cfg,
				      struct InlineGraph *graph);

#endif /* __LIVE_ANALYSIS_H__ */
