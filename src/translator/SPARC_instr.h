//
// File Name : SPARC_instr.h
// Author    : Yang, Byung-Sun
//
// This defines the SPARC instructions.
// This is based on the SPARC version 8 manual.
// Opcodes except the format specifier at 31:30 are
// op(2 bits), op2(3 bits), op3(6 bits), cond(4 bits), and opf(9 bits).
// To define each instructions,
// I divided 32 bit field into five parts like follows.
//       opf(28:20) | cond(19:16) | op3(10:5) | op2(4:2) | op(1:0)
// op, op2, and op3 can be accessed with one instruciton without sethi.
//

#ifndef __SPARC_INSTR_H__
#define __SPARC_INSTR_H__

//
// several masks to get a field from instruction code
//
#define  OP_MASK     0x3
#define  OP2_MASK    0x1c
#define  OP3_MASK    0x7e0
#define  COND_MASK   0xf0000
#define  OPF_MASK    0x1ff00000

// several useful macros
#define HI(val)  (((unsigned) (val)) >> 10)
#define LO(val)  (((unsigned) (val)) & 0x3ff)

#define MASK_DISP19 0x7ffff
#define MAX_DISP19 ((int)0x3ffff)
#define MIN_DISP19 ((int)0xfffc0000)

#define MASK_SIMM13 0x1fff
#define MAX_SIMM13 ((int)0xfff)
#define MIN_SIMM13 ((int)0xfffff000)

// some macro methods
inline extern int get_op_field( int instr_code )   { return instr_code & OP_MASK; }
inline extern int get_op2_field( int instr_code )  { return instr_code & OP2_MASK; }
inline extern int get_op3_field( int instr_code )  { return instr_code & OP3_MASK; }
inline extern int get_cond_field( int instr_code ) { return instr_code & COND_MASK; }
inline extern int get_opf_field( int instr_code )  { return instr_code & OPF_MASK; }

//
// flush instruction
//
#define FLUSH  ((2 << 30) | (0x3b << 19))  /* flush opcode */

//
// load integer instructions
//
#define  LDSB   (0x9 << 19 | 0x3 << 30)
#define  LDSH   (0xa << 19 | 0x3 << 30)
#define  LDUB   (0x1 << 19 | 0x3 << 30) 
#define  LDUH   (0x2 << 19 | 0x3 << 30)
#define  LD     (0x0 << 19 | 0x3 << 30)
#define  LDD    (0x3 << 19 | 0x3 << 30)

//
// load floating-point instructions
//
#define  LDF    (0x20 << 19 | 0x3 << 30)
#define  LDDF   (0x23 << 19 | 0x3 << 30)

//
// store integer instructions
//
#define  STB    (0x5 << 19 | 0x3 << 30)
#define  STH    (0x6 << 19 | 0x3 << 30)
#define  ST     (0x4 << 19 | 0x3 << 30)
#define  STD    (0x7 << 19 | 0x3 << 30)

//
// store floating-point instructions
//
#define  STF    (0x24 << 19 | 0x3 << 30)
#define  STDF   (0x27 << 19 | 0x3 << 30)

//
// swap register with memory
//
#define  SPARC_SWAP   (0xf << 19 | 0x01 << 13 | 0x3 << 30)

//
// sethi instruction
//
#define  SETHI  (0x4 << 22 | 0x0 << 30)

//
// nop instruction (special case of sethi instruction)
//
#define  SPARC_NOP    (0x4 << 22 | 0x0 << 30)

//all instructions which set icc has the following bit
//actually, the instructions also modify xcc.
#define  MASK_ICC     (0xc1800000)
#define  GROUP_ICC    (0x2 << 30 | 0x10 << 19)

//
// logical instructions
//
#define  AND    (0x1 << 19  | 0x2 << 30)
#define  ANDCC  (0x11 << 19 | 0x2 << 30)
#define  ANDN   (0x5 << 19  | 0x2 << 30)
#define  ANDNCC (0x15 << 19 | 0x2 << 30)
#define  OR     (0x2 << 19  | 0x2 << 30)
#define  ORCC   (0x12 << 19 | 0x2 << 30)
#define  ORN    (0x6 << 19  | 0x2 << 30)
#define  ORNCC  (0x16 << 19 | 0x2 << 30)
#define  XOR    (0x3 << 19  | 0x2 << 30)
#define  XORCC  (0x13 << 19 | 0x2 << 30)
#define  XNOR   (0x7 << 19  | 0x2 << 30)
#define  XNORCC (0x17 << 19 | 0x2 << 30)

//
// shift instructions
//
#define  SLL    (0x25 << 19 | 0x2 << 30)
#define  SRL    (0x26 << 19 | 0x2 << 30)
#define  SRA    (0x27 << 19 | 0x2 << 30)
//64-bit instructions
#define  SLLX   (0x25 << 19 | 0x1 << 12 | 0x2 << 30)
#define  SRLX   (0x26 << 19 | 0x1 << 12 | 0x2 << 30)
#define  SRAX   (0x27 << 19 | 0x1 << 12 | 0x2 << 30)

//
// add instructions
//
#define  ADD    (0x0 << 19  | 0x2 << 30)
#define  ADDCC  (0x10 << 19 | 0x2 << 30)
#define  ADDX   (0x8 << 19  | 0x2 << 30)
#define  ADDXCC (0x18 << 19 | 0x2 << 30)

//
// substract instructions
//
#define  SUB    (0x4 << 19  | 0x2 << 30)
#define  SUBCC  (0x14 << 19 | 0x2 << 30)
#define  SUBX   (0xc << 19  | 0x2 << 30)
#define  SUBXCC (0x1c << 19 | 0x2 << 30) 

//
// multiply instructions
//
#define  UMUL   (0xa << 19 | 0x2 << 30)
#define  SMUL   (0xb << 19 | 0x2 << 30)
#define  UMULCC (0x1a << 19 | 0x2 << 30)
#define  SMULCC (0x1b << 19 | 0x2 << 30)
//64-bit instructions
#define  MULX   (0x9 << 19 | 0x2 << 30)

//
// divide instrucitons
//
#define  UDIV   (0xe << 19  | 0x2 << 30)
#define  SDIV   (0xf << 19  | 0x2 << 30)
#define  UDIVCC (0x1e << 19 | 0x2 << 30)
#define  SDIVCC (0x1f << 19 | 0x2 << 30)
//64-bit instructions
#define  SDIVX  (0x2d << 19 | 0x2 << 30)
#define  UDIVX  (0xd << 19 | 0x2 << 30)

//
// read state register instruction
//
#define  RDY    (0x28 << 19 | 0x2 << 30)

//
// write state register instruction
//
#define  WRY    (0x30 << 19 | 0x2 << 30)


//
// save and restore
//
#define  SPARC_SAVE   (0x3cUL << 19 | 0x2UL << 30)
#define  RESTORE (0x3dUL << 19| 0x2UL << 30)

//
// branch on integer condition codes instructions
#define  MASK_BRANCH   0xc1c00000
//#define  GROUP_IBRANCH  0x00800000

//#define  BA     (0x8 << 25 | 0x2 << 22 | 0x0 << 30)
//#define  BN     (0x0 << 25 | 0x2 << 22 | 0x0 << 30)
//#define  BNE    (0x9 << 25 | 0x2 << 22 | 0x0 << 30)
//#define  BE     (0x1 << 25 | 0x2 << 22 | 0x0 << 30)
//#define  BG     (0xa << 25 | 0x2 << 22 | 0x0 << 30)
//#define  BLE    (0x2 << 25 | 0x2 << 22 | 0x0 << 30)
//#define  BGE    (0xb << 25 | 0x2 << 22 | 0x0 << 30)
//#define  BL     (0x3 << 25 | 0x2 << 22 | 0x0 << 30)
//#define  BGU    (0xc << 25 | 0x2 << 22 | 0x0 << 30)

//#define  BLEU   (0x4 << 25 | 0x2 << 22 | 0x0 << 30)
//#define  BCC    (0xd << 25 | 0x2 << 22 | 0x0 << 30)
//#define  BCS    (0x5 << 25 | 0x2 << 22 | 0x0 << 30)
//#define  BPOS   (0xe << 25 | 0x2 << 22 | 0x0 << 30)
//#define  BNEG   (0x6 << 25 | 0x2 << 22 | 0x0 << 30)
//#define  BVC    (0xf << 25 | 0x2 << 22 | 0x0 << 30)
//#define  BVS    (0x7 << 25 | 0x2 << 22 | 0x0 << 30)


//
// branch with prediction (SPARC v9 - dependent)
//
// - Currently, only use "icc" for the condition code
//         by doner
// - Now, "xcc" is also used.  by walker
//         

// All integer branch with predication has `001' between 24-22 bit.
#define  GROUP_IBRANCH  0x00400000
#define  PREDICT_TAKEN   0x80000
#define  PREDICT_NOT_TAKEN 0x0


#define  USE_XCC  (0x2 << 20)

#define  BA     (0x8 << 25 | 0x1 << 22 | 0x0 << 30 | 0x1 << 19)
#define  BN     (0x0 << 25 | 0x1 << 22 | 0x0 << 30)
#define  BNE    (0x9 << 25 | 0x1 << 22 | 0x0 << 30)
#define  BE     (0x1 << 25 | 0x1 << 22 | 0x0 << 30)
#define  BG     (0xa << 25 | 0x1 << 22 | 0x0 << 30)
#define  BLE    (0x2 << 25 | 0x1 << 22 | 0x0 << 30)
#define  BGE    (0xb << 25 | 0x1 << 22 | 0x0 << 30)
#define  BL     (0x3 << 25 | 0x1 << 22 | 0x0 << 30)
#define  BGU    (0xc << 25 | 0x1 << 22 | 0x0 << 30)

#define  BLEU   (0x4 << 25 | 0x1 << 22 | 0x0 << 30)
#define  BCC    (0xd << 25 | 0x1 << 22 | 0x0 << 30)
#define  BCS    (0x5 << 25 | 0x1 << 22 | 0x0 << 30)
#define  BPOS   (0xe << 25 | 0x1 << 22 | 0x0 << 30)
#define  BNEG   (0x6 << 25 | 0x1 << 22 | 0x0 << 30)
#define  BVC    (0xf << 25 | 0x1 << 22 | 0x0 << 30)
#define  BVS    (0x7 << 25 | 0x1 << 22 | 0x0 << 30)


/* This is a kind of trick. SPEC_INLIE is treated as a branch instruction, but 
   at the last moment, it is switched to the add instruction */
#define SPEC_INLINE (BN)





//
// branch on floating-point condition codes instruction
//#define  GROUP_FBRANCH    0x01800000

//#define  FBA    (0x8 << 25 | 0x6 << 22 | 0x0 << 30)
//#define  FBN    (0x0 << 25 | 0x6 << 22 | 0x0 << 30)
//#define  FBU    (0x7 << 25 | 0x6 << 22 | 0x0 << 30)
//#define  FBG    (0x6 << 25 | 0x6 << 22 | 0x0 << 30)
//#define  FBUG   (0x5 << 25 | 0x6 << 22 | 0x0 << 30)
//#define  FBL    (0x4 << 25 | 0x6 << 22 | 0x0 << 30)
//#define  FBUL   (0x3 << 25 | 0x6 << 22 | 0x0 << 30)
//#define  FBLG   (0x2 << 25 | 0x6 << 22 | 0x0 << 30)
//#define  FBNE   (0x1 << 25 | 0x6 << 22 | 0x0 << 30)
//#define  FBE    (0x9 << 25 | 0x6 << 22 | 0x0 << 30)
//#define  FBUE   (0xa << 25 | 0x6 << 22 | 0x0 << 30)
//#define  FBGE   (0xb << 25 | 0x6 << 22 | 0x0 << 30)
//#define  FBUGE  (0xc << 25 | 0x6 << 22 | 0x0 << 30)
//#define  FBLE   (0xd << 25 | 0x6 << 22 | 0x0 << 30)
//#define  FBULE  (0xe << 25 | 0x6 << 22 | 0x0 << 30)
//#define  FBO    (0xf << 25 | 0x6 << 22 | 0x0 << 30)


//
// floating-point branch with prediction
//
#define  GROUP_FBRANCH    0x01400000

#define  FBA    (0x8 << 25 | 0x5 << 22 | 0x0 << 30 | 0x1 << 19)
#define  FBN    (0x0 << 25 | 0x5 << 22 | 0x0 << 30)
#define  FBU    (0x7 << 25 | 0x5 << 22 | 0x0 << 30)
#define  FBG    (0x6 << 25 | 0x5 << 22 | 0x0 << 30)
#define  FBUG   (0x5 << 25 | 0x5 << 22 | 0x0 << 30)
#define  FBL    (0x4 << 25 | 0x5 << 22 | 0x0 << 30)
#define  FBUL   (0x3 << 25 | 0x5 << 22 | 0x0 << 30)
#define  FBLG   (0x2 << 25 | 0x5 << 22 | 0x0 << 30)
#define  FBNE   (0x1 << 25 | 0x5 << 22 | 0x0 << 30)
#define  FBE    (0x9 << 25 | 0x5 << 22 | 0x0 << 30)
#define  FBUE   (0xa << 25 | 0x5 << 22 | 0x0 << 30)
#define  FBGE   (0xb << 25 | 0x5 << 22 | 0x0 << 30)
#define  FBUGE  (0xc << 25 | 0x5 << 22 | 0x0 << 30)
#define  FBLE   (0xd << 25 | 0x5 << 22 | 0x0 << 30)
#define  FBULE  (0xe << 25 | 0x5 << 22 | 0x0 << 30)
#define  FBO    (0xf << 25 | 0x5 << 22 | 0x0 << 30)


// 
// annul bit for branch operation
#define  ANNUL_BIT  (1<<29)

//
// trap instructions
//
#define  MASK_TRAP  0xc1f80000
#define  GROUP_TRAP 0x81d00000

#define  TA     (0x2 << 30 | 0x8 << 25 | 0x3a << 19 | 0x1 << 13)
#define  TN     (0x2 << 30 | 0x0 << 25 | 0x3a << 19 | 0x1 << 13)
#define  TNE    (0x2 << 30 | 0x9 << 25 | 0x3a << 19 | 0x1 << 13)
#define  TE     (0x2 << 30 | 0x1 << 25 | 0x3a << 19 | 0x1 << 13)
#define  TG     (0x2 << 30 | 0xa << 25 | 0x3a << 19 | 0x1 << 13)
#define  TLE    (0x2 << 30 | 0x2 << 25 | 0x3a << 19 | 0x1 << 13)
#define  TGE    (0x2 << 30 | 0xb << 25 | 0x3a << 19 | 0x1 << 13)
#define  TL     (0x2 << 30 | 0x3 << 25 | 0x3a << 19 | 0x1 << 13)
#define  TGU    (0x2 << 30 | 0xc << 25 | 0x3a << 19 | 0x1 << 13)
#define  TLEU   (0x2 << 30 | 0x4 << 25 | 0x3a << 19 | 0x1 << 13)
#define  TCC    (0x2 << 30 | 0xd << 25 | 0x3a << 19 | 0x1 << 13)
#define  TCS    (0x2 << 30 | 0x5 << 25 | 0x3a << 19 | 0x1 << 13)
#define  TPOS   (0x2 << 30 | 0xe << 25 | 0x3a << 19 | 0x1 << 13)
#define  TNEG   (0x2 << 30 | 0x6 << 25 | 0x3a << 19 | 0x1 << 13)
#define  TVC    (0x2 << 30 | 0xf << 25 | 0x3a << 19 | 0x1 << 13)
#define  TVS    (0x2 << 30 | 0x7 << 25 | 0x3a << 19 | 0x1 << 13)



//
// call and link
//
#define  CALL   (0x1 << 30)
// This is a pseudo instruction to implement 'jsr'.
// It is actually the same as 'CALL'.
#define  GOSUB  (0x1 << 30 | 0x01 )
#define  GOSUB_SETHI   (0x1 << 30 | 0x02 )
#define  DUMMY_OP  (0x1 << 30 | 0x03 )
#define  JMPL   (0x38 << 19 | 0x2 << 30)

#define  INVOKE (0x1 << 30 | 0x04) /* pseudo call instruction */


// load from or store to FSR
#define LDFSR   (0x3 << 30 | 0x21 << 19 )
#define STFSR   (0x3 << 30 | 0x25 << 19 )

#define GROUP_CONVERT ( 0x34 << 19 )
//
// convert 32-bit integer to floating-point instructions
//
#define  FITOS  (0x0c4 << 5 | 0x34 << 19 | 0x2 << 30) 
#define  FITOD  (0x0c8 << 5 | 0x34 << 19 | 0x2 << 30)

//
// convert 64-integer to floating-point instructions
//
#define  FXTOS  (0x084 << 5 | 0x34 << 19 | 0x2 << 30) 
#define  FXTOD  (0x088 << 5 | 0x34 << 19 | 0x2 << 30)

// 
// convert floating-point to integer instructions
//
#define  FSTOI  (0x0d1 << 5 | 0x34 << 19 | 0x2 << 30) 
#define  FSTOX  (0x081 << 5 | 0x34 << 19 | 0x2 << 30)
#define  FDTOI  (0x0d2 << 5 | 0x34 << 19 | 0x2 << 30)
#define  FDTOX  (0x082 << 5 | 0x34 << 19 | 0x2 << 30)

//
// convert between floating-point formats instructions
//
#define  FSTOD  (0x0c9 << 5 | 0x34 << 19 | 0x2 << 30)
#define  FDTOS  (0x0c6 << 5 | 0x34 << 19 | 0x2 << 30)

//
// floating-point move instructions
//
#define  FMOVS  (0x1 << 5 | 0x34 << 19 | 0x2 << 30)
#define  FMOVD  (0x2 << 5 | 0x34 << 19 | 0x2 << 30)
#define  FNEGS  (0x5 << 5 | 0x34 << 19 | 0x2 << 30)
#define  FNEGD  (0x6 << 5 | 0x34 << 19 | 0x2 << 30)
#define  FABSS  (0x9 << 5 | 0x34 << 19 | 0x2 << 30)

//
// floating-point add and subtract instructions
//
#define  FADDS  (0x041 << 5 | 0x34 << 19 | 0x2 << 30)
#define  FADDD  (0x042 << 5 | 0x34 << 19 | 0x2 << 30)
#define  FSUBS  (0x045 << 5 | 0x34 << 19 | 0x2 << 30)
#define  FSUBD  (0x046 << 5 | 0x34 << 19 | 0x2 << 30)

//
// floating-point multiply and divide instructions
//
#define  FMULS  (0x049 << 5 | 0x34 << 19 | 0x2 << 30) 
#define  FMULD  (0x04a << 5 | 0x34 << 19 | 0x2 << 30)
#define  FDIVS  (0x04d << 5 | 0x34 << 19 | 0x2 << 30)
#define  FDIVD  (0x04e << 5 | 0x34 << 19 | 0x2 << 30)

//
// floating-point compare instructions
//
#define  FCMPS  (0x051 << 5 | 0x35 << 19 | 0x2 << 30)
#define  FCMPD  (0x052 << 5 | 0x35 << 19 | 0x2 << 30)



//
// conditional move instruction (SPARC v9)
//
// These instructions are considered as normal format5/6 instructions.
#define  COND_MOVE_MASK   0xc1f80000
#define  GROUP_COND_MOVE  (0x2 << 30 | 0x2c << 19)


#define  MOVA     (0x8 << 14 | 0x2c << 19 | 0x2 << 30)
#define  MOVN     (0x0 << 14 | 0x2c << 19 | 0x2 << 30)
#define  MOVNE    (0x9 << 14 | 0x2c << 19 | 0x2 << 30)
#define  MOVE     (0x1 << 14 | 0x2c << 19 | 0x2 << 30)
#define  MOVG     (0xa << 14 | 0x2c << 19 | 0x2 << 30)
#define  MOVLE    (0x2 << 14 | 0x2c << 19 | 0x2 << 30)
#define  MOVGE    (0xb << 14 | 0x2c << 19 | 0x2 << 30)
#define  MOVL     (0x3 << 14 | 0x2c << 19 | 0x2 << 30)
#define  MOVGU    (0xc << 14 | 0x2c << 19 | 0x2 << 30)

#define  MOVLEU   (0x4 << 14 | 0x2c << 19 | 0x2 << 30)
#define  MOVCC    (0xd << 14 | 0x2c << 19 | 0x2 << 30)
#define  MOVCS    (0x5 << 14 | 0x2c << 19 | 0x2 << 30)
#define  MOVPOS   (0xe << 14 | 0x2c << 19 | 0x2 << 30)
#define  MOVNEG   (0x6 << 14 | 0x2c << 19 | 0x2 << 30)
#define  MOVVC    (0xf << 14 | 0x2c << 19 | 0x2 << 30)
#define  MOVVS    (0x7 << 14 | 0x2c << 19 | 0x2 << 30)

#define  MOVFA    MOVA
#define  MOVFN    MOVFN
#define  MOVFU    MOVVS
#define  MOVFG    MOVNEG
#define  MOVFUG   MOVCS
#define  MOVFL    MOVLEU
#define  MOVFUL   MOVL
#define  MOVFLG   MOVLE
#define  MOVFNE   MOVE
#define  MOVFE    MOVNE
#define  MOVFUE   MOVG
#define  MOVFGE   MOVGE
#define  MOVFUGE  MOVGU
#define  MOVFLE   MOVCC
#define  MOVFULE  MOVPOS
#define  MOVFO    MOVVC



//
// cc registers
//
#define  FCC0     0
#define  FCC1     1
#define  FCC2     2
#define  FCC3     3
#define  ICC      0x080
#define  XCC      0x082

#include "basic.h"
#include "SPARC_instr.ic"

#endif __SPARC_INSTR_H__
