/* class_inclusion_test.h

   To determine subclass relationship between two classes,
   a bit vector is managed by JVM.
   When a class is loaded and linked, JVM check if this class is a subclass
   of each class which has been already loaded.
   And JVM updates a bit table and when queried, use this table.

   
   Written by: Junpyo Lee <walker@altair.snu.ac.kr>
   
   Copyright (C) 2000 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */
#ifndef __CLASS_INCLUSION_TEST_H__
#define __CLASS_INCLUSION_TEST_H__

#include "gtypes.h"

struct Hjava_lang_Class;
struct Hjava_lang_Object;

void CIT_update_class_hierarchy(struct Hjava_lang_Class*);
bool CIT_is_subclass(struct Hjava_lang_Class*, struct Hjava_lang_Class*);


#endif __CLASS_INCLUSION_TEST_H__
