#include <string.h>
#include "config.h"
#include "cell_node.h"

CellNode *cell_table[CELL_TABLE_SIZE];

#undef INLINE
#define INLINE
#include "cell_node.ic"

