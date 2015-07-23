/*
 * sparc/trampolines.c
 * Sparc trampolines codes for various occasions.
 *
 * Copyright (c) 1996,97 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Richard Henderson <rth@tamu.edu>
 * Modified by Tim Wilkinons <tim@tjwassoc.co.uk>
 */

#include "config.h"

#if defined(TRAMPOLINE_FUNCTION)
/*
 * If we have an explit function defined then use that.
 */
TRAMPOLINE_FUNCTION()
 
#else
/*
 * Otherwise we'll try to construct one.
 */
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

asm(
	START_ASM_FUNC() C_FUNC_NAME(sparc_do_fixup_trampoline) "\n"
C_FUNC_NAME(sparc_do_fixup_trampoline) ":			\n
	save	%sp,-96,%sp					\n
	ld	[%i7+8],%o0					\n
	call	" C_FUNC_NAME(fixupTrampoline) "		\n
	mov	%g1,%i7						\n
	jmp	%o0						\n
	restore"
	END_ASM_FUNC()
);
#endif

