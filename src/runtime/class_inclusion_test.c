/* class_inclusion_test.c
   
   This is an implementation of class inclusion test.
   
   Written by: Junpyo Lee <walker@altair.snu.ac.kr>
   
   Copyright (C) 2000 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */
#include <assert.h>
#include <stdlib.h>
#include <alloca.h>
#include "config.h"
#include "gtypes.h"
#include "exception.h"
#include "classMethod.h"
#include "gc.h"

extern Hjava_lang_Class* ObjectClass;
#define  DBG( s )
#define CIT_MALLOC(SZ, UNIT)  (UNIT*)gc_malloc_fixed((SZ)*sizeof(UNIT))

#define INITIAL_BITVEC_SIZE 5
#define EXPAND_BITVEC_SIZE  5

static int column_idx = 0;
static int bitvec_size = INITIAL_BITVEC_SIZE;


/* classList is used in two cases.
   1. when BM must be expanded.
   2. when array class is loaded. */
typedef struct _classList {
    Hjava_lang_Class* class;
    struct _classList* next;
} classList;

static classList *NormalHead = NULL;
static classList *ArrayHead = NULL;
static int32     num_of_array_classes = 0;


// static uint32 bit_array[] = {
//     0x80000000, 0x40000000, 0x20000000, 0x10000000,
//     0x08000000, 0x04000000, 0x02000000, 0x01000000, 
//     0x00800000, 0x00400000, 0x00200000, 0x00100000,
//     0x00080000, 0x00040000, 0x00020000, 0x00010000, 
//     0x00008000, 0x00004000, 0x00002000, 0x00001000,
//     0x00000800, 0x00000400, 0x00000200, 0x00000100, 
//     0x00000080, 0x00000040, 0x00000020, 0x00000010,
//     0x00000008, 0x00000004, 0x00000002, 0x00000001 
// };

static inline void
set_ith_bit(uint32* row, int32 pos)
{
    row[pos >> 5] |= (0x1 << (31-(pos & 0x1f)));
//    row[pos >> 5] |= bit_array[pos & 0x1f];
}

static inline bool
is_ith_bit_set(uint32* row, int32 pos)
{
    return row[pos >> 5] & (0x1 << (31-(pos & 0x1f)));
//    return row[pos >> 5] & bit_array[pos & 0x1f];
}

static void
_expand_binary_matrix_in_class_list(classList* cl, int prev_size)
{
    while (cl) {
        TypeRep* type_rep = Class_GetTypeRep(cl->class);
        uint32* prev_row = type_rep->row;

        type_rep->row = CIT_MALLOC(bitvec_size,uint32);
        memcpy(type_rep->row, prev_row, prev_size * sizeof(uint32));

        if (Class_GetVirtualTable(cl->class))
          Class_GetVirtualTable(cl->class)->row = type_rep->row;

        cl = cl->next;
    }
}

static void
expand_binary_matrix()
{
    int prev_size = bitvec_size;
    
    bitvec_size += EXPAND_BITVEC_SIZE;

    _expand_binary_matrix_in_class_list(NormalHead, prev_size);
    _expand_binary_matrix_in_class_list(ArrayHead, prev_size);
}

inline static int32
_make_encoding(int32 pos)
{
    return ((pos >> 3) << 8) | (0x1 << (7-(pos & 0x7)));
}

static void
_update_array_class_hierarchy(Hjava_lang_Class* class)
{
    Hjava_lang_Class* elem_type = Class_GetElementType(class);
    short       dim = Class_GetDimension(class);
    TypeRep*    type_rep = Class_GetTypeRep(class);
    TypeRep*    elem_type_rep = Class_GetTypeRep(elem_type);
    classList*  cl = ArrayHead;

    /* For array classes whose component is reference type, 
       we set encoding field to make AASTORE faster */
    if (!Class_IsPrimitiveType(Class_GetComponentType(class))) {
        int32 encoding =  
            _make_encoding(Class_GetTypeRep(Class_GetComponentType(class))->pos);
        Class_SetEncoding(class, encoding);
    }

    while (cl) {
        Hjava_lang_Class* tmp_class = cl->class;
        Hjava_lang_Class* tmp_elem_type = Class_GetElementType(tmp_class);
        short tmp_dim = Class_GetDimension(tmp_class);
        TypeRep* tmp_type_rep = Class_GetTypeRep(tmp_class);
        TypeRep* tmp_elem_type_rep = Class_GetTypeRep(tmp_elem_type);

        if (elem_type == ObjectClass) {
            if (tmp_dim >= dim)
              set_ith_bit(tmp_type_rep->row, type_rep->pos);
        } else if (tmp_elem_type == ObjectClass) {
            if (dim >= tmp_dim)
              set_ith_bit(type_rep->row, tmp_type_rep->pos);
        } else if (dim == tmp_dim) {
            if (Class_IsPrimitiveType(elem_type) 
                || Class_IsPrimitiveType(tmp_elem_type));

            else if (is_ith_bit_set(elem_type_rep->row, tmp_elem_type_rep->pos))
              set_ith_bit(type_rep->row, tmp_type_rep->pos);
            else if (is_ith_bit_set(tmp_elem_type_rep->row, elem_type_rep->pos))
              set_ith_bit(tmp_type_rep->row, type_rep->pos);
        }
        cl = cl->next;
    }

    /* add this class to the array class list */
    cl = CIT_MALLOC(1, classList);
    cl->class = class;
    cl->next = ArrayHead;
    ArrayHead = cl;
}

static void
_update_normal_class_hierarchy(Hjava_lang_Class* class)
{
    int                 num_of_interfaces =
	                  Class_GetNumOfImplementedInterfaces(class);
    Hjava_lang_Class**  interfaces = Class_GetImplementedInterfaces(class);
    TypeRep*            type_rep = Class_GetTypeRep(class);
    classList*          cl;

    int                 i;

    for (i = 0; i < num_of_interfaces; i++) {
        Hjava_lang_Class* interface = interfaces[i];
        int j;
        for (j = 0; j < bitvec_size; j++) {
            type_rep->row[j] |= Class_GetTypeRep(interface)->row[j];
        }
    }

    /* add this class to the normal class list */
    cl = CIT_MALLOC(1, classList);
    cl->class = class;
    cl->next = NormalHead;
    NormalHead = cl;

}
        
/* Name        : CIT_update_class_hierarchy
   Description : When a class is loaded, the corresponding row of 
                 the class should be set appropriately.
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition: 
       This function should be called only once per class.
       In case of normal class, a superclass calls this function 
       earlier than a subclass.
   
   Post-condition:
   
   Notes:  */
void
CIT_update_class_hierarchy(Hjava_lang_Class* class)
{
    Hjava_lang_Class*   superclass = Class_GetSuperclass(class);
    TypeRep *type_rep = Class_GetTypeRep(class);

    if (column_idx >= bitvec_size * 32)
      expand_binary_matrix();

    type_rep->pos = column_idx++;

//     /* row need not be allocated for interface */
//     if (Class_IsInterfaceType(class))
//       return;

    type_rep->row = CIT_MALLOC(bitvec_size,uint32);
    if (Class_GetVirtualTable(class))
      Class_GetVirtualTable(class)->row = type_rep->row;

    if (superclass != NULL)
      memcpy(type_rep->row, Class_GetTypeRep(superclass)->row, 
             bitvec_size * sizeof(uint32));
    /* type inclusion teset is reflexive. */
    set_ith_bit(type_rep->row, type_rep->pos);


    if (Class_IsArrayType(class))
      _update_array_class_hierarchy(class);
    else /* normal class */
      _update_normal_class_hierarchy(class);

}
/* Name        : CIT_is_subclass
   Description : check whether class1 is a subclass of class2
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
bool
CIT_is_subclass(Hjava_lang_Class* class1, Hjava_lang_Class* class2)
{
    TypeRep* type_rep1 = Class_GetTypeRep(class1);
    TypeRep* type_rep2 = Class_GetTypeRep(class2);

    return is_ith_bit_set(type_rep1->row, type_rep2->pos);
}

