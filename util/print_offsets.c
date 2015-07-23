/* print_offsets.c
   Output assembly constants for interpreter.
   
   Written by: Yoo C. Chung <chungyc@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Laboratory, Seoul National University.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#include <stdio.h>
#include "config.h"
#include "gtypes.h"
#include "classMethod.h"
#include "object.h"

void
print (const char *str, int n)
{
    printf("%-20s\t.assigna %4d\n", str, n);
}

int
main (int argc, char *argv[])
{
    Array array;
    Method method;
    Hjava_lang_Class class;

#if defined(INTERPRETER) && !defined(TRANSLATOR)
    print("ONLY_INTERPRETER", 1);
#else
    print("ONLY_INTERPRETER", 0);
#endif

#ifdef METHOD_COUNT
    print("OFFSET_MCOUNT", (char*)&method.count - (char*)&method);
#endif

    print("OFFSET_ARRAYSIZE", (char*)&array.length - (char*)&array);
    print("OFFSET_ARRAYDATA", ARRAY_DATA_OFFSET);
    print("OFFSET_CONSTANTS", (char*)&class.constants.data - (char*)&class);
    print("OFFSET_CLASS", (char*)&method.class - (char*)&method);
    print("OFFSET_ARGDISP", (char*)&method.argdisp - (char*)&method);
    print("OFFSET_LOCALSZ", (char*)&method.localsz - (char*)&method);
    print("OFFSET_IDX", (char*)&method.idx - (char*)&method);
    print("OFFSET_IIDX", (char*)&method.iidx - (char*)&method);
    print("OFFSET_NATIVE", (char*)&method.ncode - (char*)&method);
    print("OFFSET_BCODE", (char*)&method.bcode - (char*)&method);
    print("OFFSET_FLAGS", (char*)&method.accflags - (char*)&method);
    print("OFFSET_POOL", (char*)&method.resolved - (char*)&method);
    print("OFFSET_DTABLE", DTABLE_METHODOFFSET);
    print("OFFSET_NCALLER", (char*)&method.native_caller - (char*)&method);

    return 0;
}
