/* bytecode_profile.c
   profile bytecode execution behavior
   
   Written by: Suhyun Kim <zelo@i.am>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include "config.h"
#include "bytecode.h"
#include "bytecode_profile.h"


typedef struct BytecodeProfile {
    Method *method;
    unsigned int bytecodeLen;
    unsigned long *bytecodeFreq;
    unsigned char *bytecode;
    unsigned long long methodFreq;
} BytecodeProfile;

#define Pbc_MAX_METHOD_NUMBER 1024 * 4

BytecodeProfile method_bytecode[Pbc_MAX_METHOD_NUMBER] = {{0,},};
unsigned long bytecode_freq[256] = {0,};
unsigned char bytecode_sort[256] = {0,};


// file-static function declaration...
static unsigned Pbc_hash(Method *method); 
static void     Pbc_sort_profile();



//////////////////////////////////////////////////////////////////////
//  function definitions
//////////////////////////////////////////////////////////////////////

/* Name        : Pbc_hash
   Description : get a hash value
   Maintainer  : Suhyun Kim <zelo@i.am> */
static
unsigned
Pbc_hash(Method *method)
{
    int key = (int) method;
    return (key >> 8) % Pbc_MAX_METHOD_NUMBER;
}



/* Name        : Pbc_init_profile
   Description : initialize bytecode profile system
   Maintainer  : Suhyun Kim <zelo@i.am>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
Pbc_init_profile(Method *method)
{
    unsigned int key = Pbc_hash(method);
    unsigned int i, j, k;

    for(i = key, j = 0; (i <= Pbc_MAX_METHOD_NUMBER) && \
            method_bytecode[i].method != NULL; i++) {
        if ((i == key) && (j == 1)) {
            printf("Warning no space in method_bytecode!!!\n");
            return;
        }
        if (i == Pbc_MAX_METHOD_NUMBER) {
            i = 0; j = 1;
        }
    }

    method_bytecode[i].method = method;
    method_bytecode[i].methodFreq = 0;

    method_bytecode[i].bytecodeFreq = 
        malloc((method->bcodelen + 1) * sizeof(unsigned long));

    method_bytecode[i].bytecode = 
        malloc((method->bcodelen + 1) * sizeof(unsigned long));

    bzero(method_bytecode[i].bytecodeFreq, \
          (method->bcodelen + 1) * sizeof(unsigned long));
    method_bytecode[i].bytecodeLen = \
        method->bcodelen;

    for(k = 0; k < method->bcodelen; k++) {
        method_bytecode[i].bytecode[k] = method->bcode[k];
    }
}



/* Name        : Pbc_increase_frequency
   Description : increase the frequency of a given bytecode instruction
   Maintainer  : Suhyun Kim <zelo@i.am>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
Pbc_increase_frequency(Method *method, int bpc)
{
    unsigned int key = Pbc_hash(method);
    unsigned int i, j;

    for(i = key, j = 0; (i <= Pbc_MAX_METHOD_NUMBER) && \
            method_bytecode[i].method != method; i++) {
        if ((i == key) && (j == 1)) {
            printf("Warning no such method in method_bytecode!!!\n");
            return;
        }
        if (i == Pbc_MAX_METHOD_NUMBER) {
            i = 0; j = 1;
        }
    }

    method_bytecode[i].bytecodeFreq[bpc]++;
    method_bytecode[i].methodFreq++;
    bytecode_freq[method_bytecode[i].bytecode[bpc]]++;
}



/* Name        : Pbc_print_profile
   Description : print collected profile information
   Maintainer  : Suhyun Kim <zelo@i.am>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
void
Pbc_print_profile()
{
    unsigned int i, j, bpc;
    FILE *file_out;
    unsigned long long total_bytefreq = 0;
    unsigned long long total_methfreq = 0;

    Pbc_sort_profile();

    for(i = 0; i < 256; i++) {
        total_bytefreq += bytecode_freq[i];
    }

    for(i = 0; i < Pbc_MAX_METHOD_NUMBER; i++) {
        total_methfreq += method_bytecode[i].methodFreq;
    }

/*      file_out = fopen("latte.profile", "wb"); */
    file_out = stderr;

    if( file_out == NULL ){
        fprintf( stderr, "Can't open latte.profile.\n" ); 
        exit(1); 
    }

    for(i = 0; i < Pbc_MAX_METHOD_NUMBER; i++) {
        if (method_bytecode[i].methodFreq <= 0) continue;
        fprintf(file_out, "Method %s %s %s profile %llu ( %5.2f ).\n", 
                method_bytecode[i].method->class->name->data,
                method_bytecode[i].method->name->data,
                method_bytecode[i].method->signature->data,
                method_bytecode[i].methodFreq,
                method_bytecode[i].methodFreq / (total_methfreq / 100.0));
        for(bpc = 0; bpc < method_bytecode[i].bytecodeLen; bpc++) {
            if (method_bytecode[i].bytecodeFreq[bpc] > 0) {
                fprintf(file_out, "%s %s %s bpc %d : %lu\n", 
                        method_bytecode[i].method->class->name->data,
                        method_bytecode[i].method->name->data,
                        method_bytecode[i].method->signature->data,
                        bpc,
                        method_bytecode[i].bytecodeFreq[bpc]);
            }
        }
    }

    for(j = 0; j < 256; j++) {
        if ((bytecode_freq[bytecode_sort[j]] == 0) || \
            (BCode_get_opcode_name(bytecode_sort[j]) == NULL)) continue;
        fprintf(file_out, "BYTECODE %s : %lu times( %5.2f ).\n", \
                BCode_get_opcode_name(bytecode_sort[j]), \
                bytecode_freq[bytecode_sort[j]], \
                bytecode_freq[bytecode_sort[j]] / (total_bytefreq / 100.0));
    }
    fclose(file_out);
}



/* Name        : Pbc_sort_profile
   Description : sort profile information to help printing
   Maintainer  : Suhyun Kim <zelo@i.am>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
static
void
Pbc_sort_profile()
{
    unsigned int i, j;
    BytecodeProfile temporary;
    unsigned char temp_freq;
    
    for(i = 0; i < Pbc_MAX_METHOD_NUMBER; i++) {
        for(j = i + 1; j < Pbc_MAX_METHOD_NUMBER; j++) {
            if (method_bytecode[i].methodFreq < \
                method_bytecode[j].methodFreq) {
                temporary = method_bytecode[i];
                method_bytecode[i] = method_bytecode[j];
                method_bytecode[j] = temporary;
            }
        }
    }

    for(i = 0; i < 256; i++) {
        bytecode_sort[i] = i;
    }

    for(i = 0; i < 256; i++) {
        for(j = i + 1; j < 256; j++) {
            if (bytecode_freq[bytecode_sort[i]] < \
                bytecode_freq[bytecode_sort[j]]) {
                temp_freq = bytecode_sort[i];
                bytecode_sort[i] = bytecode_sort[j];
                bytecode_sort[j] = temp_freq;
            }
        }
    }
}
