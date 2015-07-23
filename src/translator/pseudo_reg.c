/* pseudo_reg.c
   
   pseudo register (also called as variable) related functions
   
   Written by: SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */
#include "config.h"
#include "pseudo_reg.h"
#include "method_inlining.h"
#include "CFG.h"

#undef INLINE
#define INLINE
#include "pseudo_reg.ic"

int Var_offset_of_stack_var;
int Var_offset_of_local_var;
int Var_offset_of_temp_var;

/* actual function definitions are in pseudo_reg.ic */

/* Name        : Var_prepare_creation
   Description : set variable offsets before making pseudo registers
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
Var_prepare_creation(struct InlineGraph *graph)
{
    Var_offset_of_local_var = IG_GetLocalOffset(graph);
    Var_offset_of_stack_var = IG_GetStackOffset(graph);
    Var_offset_of_temp_var = 0;
}

/* Name        : Var_GetName
   Description : return the name of variable
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
char *
Var_GetName(Var var)
{
    VarType var_type;
    VarStorageType storage_type;
    char *type_name, *storage_name;
    char *_var_reg_name;

    _var_reg_name = (char *) FMA_calloc(10 * sizeof(char));
    
    var_type = Var_GetType(var);
    storage_type = Var_GetStorageType(var);

    assert(Var_IsVariable(var));

    type_name = NULL;
    
    switch(var_type) {
      case T_INT:
	type_name = "i";
	break;
      case T_LONG:
	type_name = "l";
	break;
      case T_CC:
	type_name = "c";
	break;
      case T_FLOAT:
	type_name = "f";
	break;
      case T_DOUBLE:
	type_name = "d";
	break;
      case T_REF:
	type_name = "r";
	break;
      case T_IVOID:
	type_name = "iv";
	break;
      case T_FVOID:
	type_name = "fv";
	break;
      default:
	assert(false);
	break;
    }

    assert(type_name != NULL);

    storage_name = NULL;
    
    switch(storage_type) {
      case ST_STACK:
	storage_name = "st";
	break;
      case ST_LOCAL:
	storage_name = "lc";
	break;
      case ST_TEMP:
	storage_name = "tp";
	break;
      case ST_VN:
	storage_name = "vn";
	break;
      default:
	assert(false);
	break;
    }

    assert(storage_name != NULL);
    
    sprintf(_var_reg_name, "%s%s%d", type_name, storage_name,
	    CFG_get_var_number(var));

    return _var_reg_name;
}

/* Name        : Var_get_var_reg_name
   Description : return variable or register name
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
char *
Var_get_var_reg_name(int var_reg)
{
    if (Var_IsVariable(var_reg)) return Var_GetName(var_reg);
    
    return Reg_GetName(var_reg);
}


/* Name        : Var_make
   Description : make pseudo variable based on type, storage location,
                 and number
   Maintainer  : SeungIl Lee <hacker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
Var
Var_make(VarType type, VarStorageType storage_type, int number)
{
   switch (storage_type) {
     case ST_LOCAL:
       number += Var_offset_of_local_var;
       break;

     case ST_STACK:
       number += Var_offset_of_stack_var;
       break;

     case ST_TEMP:
       number += Var_offset_of_temp_var;
       break;
     case ST_VN:
       number += Var_offset_of_temp_var + 11;

     case ST_INVALID:
       break;

     default:
       assert(0);
       break;
   }

   assert((number & VAR_NUM_MASK) == number);

   return ((((unsigned) type) << TYPE_SHIFT_SIZE) 
           | (((unsigned) storage_type)) << STORAGE_TYPE_SHIFT_SIZE 
           | ((unsigned) number));
}
