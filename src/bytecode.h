/*
 * bytecode.h
 * Bytecode defines.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 * Modified by MASS Laboratory, SNU, 1999.
 */

#ifndef __bytecode_h
#define __bytecode_h

#include "basic.h"
#include "gtypes.h"

#define NOP 0
#define ACONST_NULL 1
#define ICONST_M1 2
#define ICONST_0 3
#define ICONST_1 4
#define ICONST_2 5
#define ICONST_3 6
#define ICONST_4 7
#define ICONST_5 8
#define LCONST_0 9
#define LCONST_1 10
#define FCONST_0 11
#define FCONST_1 12
#define FCONST_2 13
#define DCONST_0 14
#define DCONST_1 15
#define BIPUSH 16
#define SIPUSH 17
#define LDC1 18
#define LDC2 19
#define LDC2W 20
#define ILOAD 21
#define LLOAD 22
#define FLOAD 23
#define DLOAD 24
#define ALOAD 25
#define ILOAD_0 26
#define ILOAD_1 27
#define ILOAD_2 28
#define ILOAD_3 29
#define LLOAD_0 30
#define LLOAD_1 31
#define LLOAD_2 32
#define LLOAD_3 33
#define FLOAD_0 34
#define FLOAD_1 35
#define FLOAD_2 36
#define FLOAD_3 37
#define DLOAD_0 38
#define DLOAD_1 39
#define DLOAD_2 40
#define DLOAD_3 41
#define ALOAD_0 42
#define ALOAD_1 43
#define ALOAD_2 44
#define ALOAD_3 45
#define IALOAD 46
#define LALOAD 47
#define FALOAD 48
#define DALOAD 49
#define AALOAD 50
#define BALOAD 51
#define CALOAD 52
#define SALOAD 53
#define ISTORE 54
#define LSTORE 55
#define FSTORE 56
#define DSTORE 57
#define ASTORE 58
#define ISTORE_0 59
#define ISTORE_1 60
#define ISTORE_2 61
#define ISTORE_3 62
#define LSTORE_0 63
#define LSTORE_1 64
#define LSTORE_2 65
#define LSTORE_3 66
#define FSTORE_0 67
#define FSTORE_1 68
#define FSTORE_2 69
#define FSTORE_3 70
#define DSTORE_0 71
#define DSTORE_1 72
#define DSTORE_2 73
#define DSTORE_3 74
#define ASTORE_0 75
#define ASTORE_1 76
#define ASTORE_2 77
#define ASTORE_3 78
#define IASTORE 79
#define LASTORE 80
#define FASTORE 81
#define DASTORE 82
#define AASTORE 83
#define BASTORE 84
#define CASTORE 85
#define SASTORE 86
#define POP 87
#define POP2 88
#define DUP 89
#define DUP_X1 90
#define DUP_X2 91
#define DUP2 92
#define DUP2_X1 93
#define DUP2_X2 94
#define SWAP 95
#define IADD 96
#define LADD 97
#define FADD 98
#define DADD 99
#define ISUB 100
#define LSUB 101
#define FSUB 102
#define DSUB 103
#define IMUL 104
#define LMUL 105
#define FMUL 106
#define DMUL 107
#define IDIV 108
#define LDIV 109
#define FDIV 110
#define DDIV 111
#define IREM 112
#define LREM 113
#define FREM 114
#define DREM 115
#define INEG 116
#define LNEG 117
#define FNEG 118
#define DNEG 119
#define ISHL 120
#define LSHL 121
#define ISHR 122
#define LSHR 123
#define IUSHR 124
#define LUSHR 125
#define IAND 126
#define LAND 127
#define IOR 128
#define LOR 129
#define IXOR 130
#define LXOR 131
#define IINC 132
#define I2L 133
#define I2F 134
#define I2D 135
#define L2I 136
#define L2F 137
#define L2D 138
#define F2I 139
#define F2L 140
#define F2D 141
#define D2I 142
#define D2L 143
#define D2F 144
#define INT2BYTE 145
#define INT2CHAR 146
#define INT2SHORT 147
#define LCMP 148
#define FCMPL 149
#define FCMPG 150
#define DCMPL 151
#define DCMPG 152
#define IFEQ 153
#define IFNE 154
#define IFLT 155
#define IFGE 156
#define IFGT 157
#define IFLE 158
#define IF_ICMPEQ 159
#define IF_ICMPNE 160
#define IF_ICMPLT 161
#define IF_ICMPGE 162
#define IF_ICMPGT 163
#define IF_ICMPLE 164
#define IF_ACMPEQ 165
#define IF_ACMPNE 166
#define GOTO 167
#define JSR 168
#define RET 169
#define TABLESWITCH 170
#define LOOKUPSWITCH 171
#define IRETURN 172
#define LRETURN 173
#define FRETURN 174
#define DRETURN 175
#define ARETURN 176
#define RETURN 177
#define GETSTATIC 178
#define PUTSTATIC 179
#define GETFIELD 180
#define PUTFIELD 181
#define INVOKEVIRTUAL 182
#define INVOKESPECIAL 183
#define INVOKESTATIC 184
#define INVOKEINTERFACE 185
#define NEW 187
#define NEWARRAY 188
#define ANEWARRAY 189
#define ARRAYLENGTH 190
#define ATHROW 191
#define CHECKCAST 192
#define INSTANCEOF 193
#define MONITORENTER 194
#define MONITOREXIT 195
#define WIDE 196
#define MULTIANEWARRAY 197
#define IFNULL 198
#define IFNONNULL 199
#define GOTO_W 200
#define JSR_W 201
#define BREAKPOINT 202

/* The following two instructions are not used by the new interpreter. */
/*
 * These optimised instructions are only used within the interpreter.
 *  Instructions are converted during the code analysis phase rather than
 *  during execution.  This means we must always verify our code (not a bad
 *  thing anyhow).  However, it allow us increase the interpreters execution
 *  speed by removing symbolic constants without infringing Sun's patent.
 */
#define	CHECKCAST_FAST 254
#define	INSTANCEOF_FAST 255

/* Internal opcodes used by the interpreter. */
#define ILOAD_W		211
#define LLOAD_W		212
#define FLOAD_W		213
#define DLOAD_W		214
#define ALOAD_W		215
#define ISTORE_W	216
#define LSTORE_W	217
#define FSTORE_W	218
#define DSTORE_W	219
#define ASTORE_W	220
#define RET_W		221
#define IINC_W		222
#define GETFIELD_W	223
#define GETFIELD_S	224
#define GETFIELD_B	225
#define GETFIELD_C	226
#define GETFIELD_D	227
#define GETSTATIC_W	228
#define GETSTATIC_S	229
#define GETSTATIC_B	230
#define GETSTATIC_C	231
#define GETSTATIC_D	232
#define PUTFIELD_W	233
#define PUTFIELD_S	234
#define PUTFIELD_B	235
#define PUTFIELD_C	236
#define PUTFIELD_D	237
#define PUTSTATIC_W	238
#define PUTSTATIC_S	239
#define PUTSTATIC_B	240
#define PUTSTATIC_C	241
#define PUTSTATIC_D	242

/*
  Utility functions to manipulate bytecode
*/

int BCode_get_opcode_len(byte opcode);
char *BCode_get_opcode_name(byte opcode);
bool BCode_is_if_cond_bytecode(byte opcode);
int8 BCode_get_int8(byte *bcode);
uint8 BCode_get_uint8(byte *bcode);
int16 BCode_get_int16(byte *bcode);
uint16 BCode_get_uint16(byte *bcode);
int32 BCode_get_int32(byte *bcode);
uint32 BCode_get_uint32(byte *bcode);

#include "bytecode.ic"

#endif
