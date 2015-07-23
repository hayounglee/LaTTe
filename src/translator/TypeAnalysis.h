/* TypeAnalysis.h
   Interface to the type analyzer
   
   Written by: Suhyun Kim <zelo@i.am>
   Modified by: Junpyo Lee <walker@altair.snu.ac.kr>

   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __TypeAnalysis_h__
#define __TypeAnalysis_h__

// The Global Variables !!!!!!!
extern int *TA_Num_Of_Defs;     // array[number of local variables]; 


/* Perform type analysis */
void TA_type_analysis(struct InlineGraph*);


#endif /* __TypeAnalysis_h__ */
