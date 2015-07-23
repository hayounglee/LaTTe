/* translator_driver_stub.c
   assembly functions which call translator driver functions.
   
   Written by: Junpyo Lee <walker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#include "config.h"

#if !defined(START_ASM_FUNC)
#define START_ASM_FUNC() ".text\n\t.align 4\n\t.global "
#endif
#if !defined(END_ASM_FUNC)
#define END_ASM_FUNC() ""
#endif
#if defined(HAVE_UNDERSCORED_C_NAMES)
#define C_FUNC_NAME(FUNC) "_" #FUNC
#else
#define C_FUNC_NAME(FUNC) #FUNC
#endif


#ifdef CUSTOMIZATION

/*======================================================================
  dispatch_method_with_object
======================================================================*/
asm(
	START_ASM_FUNC() C_FUNC_NAME(dispatch_method_with_object) "\n"
C_FUNC_NAME(dispatch_method_with_object) ":			\n
	save	%sp,-96,%sp					\n
	mov	%i0, %o0					\n
	ld	[%i7+8],%o1					\n
	call	"C_FUNC_NAME(TD_fixup_trampoline_with_object)"	\n
	mov	%g1,%i7						\n
	jmp	%o0						\n
	restore"
	END_ASM_FUNC()
);

/*======================================================================
  dispatch_method_with_SM

  %g2 : dispatch table offset
  %o7 : return address - 8 (the address of CALL instruction)
======================================================================*/
asm(
    START_ASM_FUNC() C_FUNC_NAME(dispatch_method_with_SM) "\n"
C_FUNC_NAME(dispatch_method_with_SM) ":			\n"
    "save   %sp, -96, %sp\n"
    "mov    %g2, %l2\n"       /* %g2 has offset in dtable */
    "ld     [%i7+8],%o0\n"    /* get the specialized method instance pointer */
    "call " C_FUNC_NAME(TD_fixup_trampoline_with_SM) "\n"
    "mov    %g1,%i7\n" 
    "sub    %o0,%i7,%g1\n"    /* offset = translated code - caller_addr */
    "srl    %g1,2,%g1\n"      /* offset/=4 */
    "sethi  %hi(0x40000000),%g2\n"    /* mnemonic of call */
    "or     %g2,%g1,%g1\n"    /* new assembly is made */
    "st     %g1,[%i7]\n"      /* replace old assembly */
    "iflush %i7\n"            /* flush */
    "mov    %l2, %g2\n"       /* restore %g2: it can point typecheck code */
    "jmp    %o0\n"
    "restore\n"
);

#ifdef INLINE_CACHE
/*======================================================================
  dispatch_method_with_CC

  %g2 : dispatch table offset
  %o7 : return address - 8 (the address of CALL instruction)
======================================================================*/
asm(
    START_ASM_FUNC() C_FUNC_NAME(dispatch_method_with_CC) "\n"
C_FUNC_NAME(dispatch_method_with_CC) ":			\n"
    "save   %sp, -96, %sp\n"
    "mov    %g2, %l2\n"       /* %g2 has offset in dtable */
    "ld     [%i7+8],%o0\n"    /* get the specialized method instance pointer */
    "mov    %i0, %o1\n"       /* move this pointer to argument 1 */
    "call " C_FUNC_NAME(TD_fixup_trampoline_with_CC) "\n"
    "mov    %g1,%i7\n" 
    "sub    %o0,%i7,%g1\n"    /* offset = translated code - caller_addr */
    "srl    %g1,2,%g1\n"      /* offset/=4 */
    "sethi  %hi(0x40000000),%g2\n"    /* mnemonic of call */
    "or     %g2,%g1,%g1\n"    /* new assembly is made */
    "st     %g1,[%i7]\n"      /* replace old assembly */
    "iflush %i7\n"            /* flush */
    "mov    %l2, %g2\n"       /* restore %g2: it can point typecheck code */
    "jmp    %o0\n"
    "restore\n"
);
#endif

#else /* not CUSTOMIZATION */
/*======================================================================
  dispatch_method_with_object
======================================================================*/
asm(
	START_ASM_FUNC() C_FUNC_NAME(dispatch_method_with_object) "\n"
C_FUNC_NAME(dispatch_method_with_object) ":			\n
	save	%sp,-96,%sp					\n
	ld	[%i7+8],%o0					\n
	call	"C_FUNC_NAME(fixupTrampoline)"	                \n
	mov	%g1,%i7						\n
	jmp	%o0						\n
	restore"
	END_ASM_FUNC()
);

/*======================================================================
  static_method_dispatcher

  %g2 : dispatch table offset
  %o7 : return address - 8 (the address of CALL instruction)
======================================================================*/
asm(
    START_ASM_FUNC() C_FUNC_NAME(static_method_dispatcher) "\n"
C_FUNC_NAME(static_method_dispatcher) ":			\n"
    "save   %sp, -96, %sp\n"
    "ld     [%i7+8],%g2\n"     /* get the method pointer */
    "lduh   [%g2+8],%g3\n"     /* get the access flags of the method */
    "sethi  %hi(0x4000),%g4\n" /* TRANSLATED 0x4000 */
    "and    %g3,%g4,%g4\n"
    "cmp    %g4,%g0\n"         /* if method is translated already */
    "bne    ALREADY_TRANSLATED\n"
    "mov    %g1,%i7\n"        /* set the return address */
    /* delay slot */
    "add    %g2,%g0,%o0\n"         /* method pointer is an argument to fixupTramploine */
    "call " C_FUNC_NAME(fixupTrampoline) "\n"
    "nop\n"
    "ba SET_AND_JUMP\n"
    "add    %o0,%g0,%g3\n"
    /* the method is already translated */
    "ALREADY_TRANSLATED:\n"
    "ld     [%g2+16],%g3\n"   /* get the translated code */
    /* the contents of caller is set and jump to the translated code */
    "SET_AND_JUMP:\n"
    "sub    %g3,%i7,%g2\n"    /* offset = translated code - caller_addr */
    "srl    %g2,2,%g2\n"      /* offset/=4 */
    "sethi  %hi(0x40000000),%g4\n"    /* mnemonic of call */
    "or     %g4,%g2,%g2\n"    /* new assembly is made */
    "st     %g2,[%i7]\n"      /* replace old assembly */
    "iflush %i7\n"            /* flush */
    "jmp    %g3\n"
    "restore\n"
);

#endif /* not CUSTOMIZATIOn */



#ifdef INLINE_CACHE
/*======================================================================
 dispatch_method_with_inlinecache

  %o0 - object reference
  %o7 - retrun address
  %g2 - method offset 
  %g4 - direct/indirect flag
  %g6 - return address
======================================================================*/
asm(
    ".section \".text\"	\n"
    ".align	4\n"
    ".global dispatch_method_with_inlinecache\n" 

    "dispatch_method_with_inlinecache:\n"
    "   ld      [%o0], %g0\n"        //for exception handling
    "	save	%sp, -96, %sp\n"    //register saved(%o0->i0) 
    "	mov	%i0, %o0\n"             //%o0 :object reference
    "	call	TD_fixup_trampoline_with_inlinecache\n"
    "	mov     %g2, %o1\n"        //%o1 :method pointer
    "	mov	%o0, %g3\n"             //%g3 :pointer to the translated code
    "	restore\n"
    //%o7 : return address
    //%g3 : translated code pointer
    //temporary use : %g2, %g5
    "	sub	%g3, %o7, %g2\n"            //offset = callee - call site
    "	srl	%g2, 2, %g2	\n"             //offset /= 4
    "	sethi	%hi(0x40000000), %g5\n" 
    "	or	%g5, %g2, %g2\n"            //%g2 : call instruction
    "	st	%g2, [%o7]\n"               //replace old call intrcution
    "   flush   %o7\n"                      //self modifying code      
    "	jmpl    %g3,%g0\n   "               //jump to the function
    "   nop\n"
);

/*======================================================================
 fixup_failed_type_check

   %o0 - object reference
   %g1 - method offset 
   %g2 - return address
======================================================================*/
asm(
    ".section \".text\"	\n"
    ".align	4\n"
    ".global fixup_failed_type_check\n" 

    "fixup_failed_type_check:\n"
    "   mov     %o7, %g4\n"
    "   mov     %g3, %o7\n"
    //ld    [%o0],%g1
    "   sethi   %hi(0xC2020000),%g3\n"
//    "   or      %g3,%lo(0xC2020000),%g3\n"
    "   st      %g3,[%o7-8]\n"          //replace instruction
    //ld    [%g1+offset],%g1
    "   sethi   %hi(0xC2006000),%g3\n"
//    "   or      %g3,%lo(0xC2006000),%g3\n"
    "   sethi   %hi(0x1FFF),%g1\n"
    "   or      %g1,%lo(0x1FFF),%g1\n"
    "   and     %g2,%g1,%g1\n"       //make simm13
    "   or      %g3,%g1,%g3\n"          //%g1:method offset(byte)
    "   st      %g3,[%o7-4]\n"          //replace instruction
    //jmpl  %g1
    "   sethi   %hi(0x9FC06000),%g3\n"  //0x9FC06020
    "   or      %g3,%lo(0x9FC06000),%g3\n" //0x9FC06020
    "   st      %g3,[%o7]\n"            //replace instruction
    "   flush   %o7-8\n"                //self-modifying code
    "   flush   %o7\n"                  //   -> flush icache
    //jump to the real function
    "   ld      [%o0],%g3\n"
    "   ld      [%g3+%g2],%g3\n"
    "   jmp     %g3\n"
    "   nop\n"
);
#endif /* INLINE_CACHE */
