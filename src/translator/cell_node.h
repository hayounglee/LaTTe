/* cell_node.h
   
   Header file for cell node implementation
   cell node structure can be used for finding range based datas.
   Currently, method address look up uses this data structure.
   
   Written by: Suhyun Kim <zelo@altair.snu.ac.kr>
               SeungIl Lee <hacker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */
#ifndef __CELL_NODE_H__
#define __CELL_NDDE_H__

#include "gtypes.h"
#include "basic.h"

#define CELL_TABLE_SIZE 8192
#define CELL_SIZE       256     /* 2^CELL_ALIGN */
#define CELL_ALIGN      8

typedef enum Origin {
    MethodBody,
    TypeCheckCode 
} Origin;

typedef struct CellNode {
    uintp startAddr; /* inclusive */
    uintp endAddr;   /* exclusive */
    
    void *data;

    Origin org;

    struct CellNode *next;
} CellNode;

void CN_initialize(void);
void CN_register_data(uintp start_addr , uintp end_addr , void *data, Origin org);

void *CN_find_data(uintp addr);
bool CN_is_from_type_check_code(uintp pc);

#include "cell_node.ic"

#endif __FIND_METHOD_H__
