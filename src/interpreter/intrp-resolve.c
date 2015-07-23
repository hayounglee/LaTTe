/* intrp-resolve.c
   Constant pool resolution for the bytecode interpreter.
   
   Written by: Yoo C. Chung <chungyc@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Laboratory, Seoul National University.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

/* All functions in here are passed a pointer to somewhere in the
   resolved pool, and should fill it in appropriately after
   resolution. */

#include <stdlib.h>
#include "config.h"
#include "gtypes.h"
#include "classMethod.h"
#include "lookup.h"


void* intrp_resolve_class (Method*, int, void**);
void* intrp_resolve_method (Method*, int, void**);
int   intrp_resolve_field (Method*, int, void**);
int   intrp_resolve_static_field (Method*, int, void**);


/* Name        : intrp_resolve_class
   Description : Resolve class reference.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     getClass() should probably be called directly by the interpreter. */
void*
intrp_resolve_class (Method* m, int i, void **resolved)
{
    Hjava_lang_Class *class;

    class = getClass(i, m->class);
    *resolved = class;

    /* Initialize class if it hasn't been already. */
    if (class->state != CSTATE_OK && !Class_IsPrimitiveType(class))
	processClass(class, CSTATE_OK);

    return class;
}

/* Name        : intrp_resolve_method
   Description : Resolve method reference.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     The method in the argument is *not* the method to be resolved! */
void*
intrp_resolve_method (Method* m, int index, void **resolved)
{
    Method* method;
    method = getMethodSignatureClass(index, m);

    /* Obtain the argument displacement (i.e. the amount to displace
       from the operand stack top to obtain the address of the first
       argument in the method invocation).  INFO.IN does not include
       the invoking object. */
    if (Method_IsStatic(method))
	method->argdisp = method->in*4-4;
    else
	method->argdisp = method->in*4;

    *resolved = (void*)method;

    /* Initialize class if it hasn't been already. */
    if (method->class->state != CSTATE_OK
	&& !Class_IsPrimitiveType(method->class))
	processClass(method->class, CSTATE_OK);

    return method;
}

/* Name        : intrp_resolve_field
   Description : Resolve instance field reference.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     Returns field width (byte, char, etc.) */
int
intrp_resolve_field (Method *m, int index, void **resolved)
{
    Field *field;
    Hjava_lang_Class *class;

    field = getField(index, false, m, &class);

    /* Push offset onto resolved pool. */
    *resolved = (void*)FIELD_OFFSET(field);

    /* Initialize class if it hasn't been already. */
    if (class->state != CSTATE_OK)
	processClass(class, CSTATE_OK);

    /* Return integer based on field type.  This integer will be used
       to select the appropriate "real" opcode for accessing
       fields. */
    if (FIELD_ISREF(field))
	return 0;
    else
	switch (CLASS_PRIM_SIG(FIELD_TYPE(field))) {
	  case 'F':
	  case 'I': return 0;
	  case 'S': return 1;
	  case 'Z':
	  case 'B': return 2;
	  case 'C': return 3;
	  case 'J':
	  case 'D': return 4;
	  default: return 0;
	}
}

/* Name        : intrp_resolve_static_field
   Description : Resolve static field reference.
   Maintainer  : Yoo C. Chung <chungyc@altair.snu.ac.kr>
   Notes:
     Returns field width (byte, char, etc.) */
int
intrp_resolve_static_field (Method *m, int index, void **resolved)
{
    Field *field;
    Hjava_lang_Class *class;

    field = getField(index, true, m, &class);

    /* Push address onto resolved pool. */
    *resolved = (void*)FIELD_ADDRESS(field);

    /* Initialize class if it hasn't been already. */
    if (class->state != CSTATE_OK)
	processClass(class, CSTATE_OK);

    /* Return integer based on field type.  This integer will be used
       to select the appropriate "real" opcode for accessing
       fields. */
    if (FIELD_ISREF(field))
	return 0;
    else
	switch (CLASS_PRIM_SIG(FIELD_TYPE(field))) {
	  case 'F':
	  case 'I': return 0;
	  case 'S': return 1;
	  case 'Z':
	  case 'B': return 2;
	  case 'C': return 3;
	  case 'J':
	  case 'D': return 4;
	  default: return 0;
	}
}
