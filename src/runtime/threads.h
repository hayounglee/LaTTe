/*
 * sparc/threads.h
 * Sparc threading information.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 * Modified by MASS Laboratory, SNU, 1999.
 */

#ifndef __sparc_threads_h
#define __sparc_threads_h

/**/
/* Thread handling */
/**/
#define	USE_INTERNAL_THREADS

//#define	THREADSTACKSIZE		(32 * 1024)
#define	THREADSTACKSIZE		(128 * 1024)

/* Context switching code for the interpreter stack. */
#ifdef INTERPRETER
#define INTRP_SWITCH_CTX(to, from) do {				\
		(from)->jrestorePoint = jstack_point;		\
		jstack_point = (to)->jrestorePoint;		\
	} while (0)
#else /* not INTERPRETER */
#define INTRP_SWITCH_CTX(to, from)	do {} while (0) /* Do nothing. */
#endif /* not INTERPRETER */

#ifdef NEW_CTX_SWITCH

#define	THREADSWITCH(to, from) {				\
	int regstore[6];					\
	register void* fr asm("%l1");				\
	register void* tr asm("%l2");				\
	register int* store asm("%l3");				\
	INTRP_SWITCH_CTX(to, from);				\
	fr = &from->restorePoint;				\
	tr = &to->restorePoint;					\
	store = regstore;					\
	asm volatile ("						\n\
		call 1f						\n\
		st %%fp,[%2+4]					\n\
								\n\
1:		add %%o7,%%lo(2f-1b+12),%%l0			\n\
								\n\
		ta 3						\n\
		st %%l0,[%2+0]					\n\
		st %%sp,[%2+8]					\n\
		st %%i7,[%2+16]					\n\
		st %2,[%0]					\n\
							        \n\
		ld [%1],%2					\n\
		ld [%2+8],%%sp					\n\
		ld [%2+4],%%fp					\n\
		ld [%2+0],%%l0					\n\
								\n\
		jmpl %%l0,%%g0					\n\
2:		ld [%2+16],%%i7					\n\
		" : : "r" (fr), "r" (tr), "r" (store)		  \
		: "i0", "i1", "i2", "i3", "i4", "i5", "i7",	  \
		  "l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7", \
		  "o0", "o1", "o2", "o3", "o4", "o5", "o7",	  \
		  "g1", "g2", "g3", "g4", "g5", "g6", "g7"	  \
		);						  \
	}

#else

#define	THREADSWITCH(to, from) {				\
	int regstore[6];					\
	register void* fr asm("%l1");				\
	register void* tr asm("%l2");				\
	register int* store asm("%l3");				\
	INTRP_SWITCH_CTX(to, from);				\
	fr = &from->restorePoint;				\
	tr = &to->restorePoint;					\
	store = regstore;					\
	asm volatile ("						\n\
		call 1f						\n\
		nop						\n\
1:								\n\
		add %%o7,%%lo(2f-1b+8),%%l0			\n\
								\n\
		ta 3						\n\
		st %%l0,[%2+0]					\n\
		st %%fp,[%2+4]					\n\
		st %%sp,[%2+8]					\n\
		st %%l7,[%2+12]					\n\
		st %%i7,[%2+16]					\n\
		st %2,[%0]					\n\
								\n\
		ld [%1],%2					\n\
		ld [%2+16],%%i7					\n\
		ld [%2+12],%%l7					\n\
		ld [%2+8],%%sp					\n\
		ld [%2+4],%%fp					\n\
		ld [%2+0],%%l0					\n\
								\n\
		jmpl %%l0,%%g0					\n\
		nop 						\n\
2:		nop						\n\
		" : : "r" (fr), "r" (tr), "r" (store)		  \
		: "i0", "i1", "i2", "i3", "i4", "i5", "i7",	  \
		  "l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7", \
		  "o0", "o1", "o2", "o3", "o4", "o5", "o7",	  \
		  "g1", "g2", "g3", "g4", "g5", "g6", "g7"	  \
		);						  \
	}

#endif NEW_CTX_SWITCH


#define THREADINIT(to, func) {                                  \
                int* regstore = (int*)((to)->stackEnd - (6 * 4));\
                (to)->restorePoint = (void*)regstore;		\
                regstore[0] = (int)func;			\
                regstore[1] = ((int)regstore) - (16 * 4);	\
                regstore[2] = regstore[1];			\
                regstore[3] = 0;				\
                regstore[4] = 0;				\
        }

#define	THREADINFO(ee)						\
		do {						\
			void** ptr;				\
			int i;					\
			asm("ta 3");				\
			asm("mov %%sp,%0" : "=r" (ptr));	\
			for (i = 0; i != 2; i++) {              \
				ptr = (void**)ptr[14];		\
			}					\
			(ee)->restorePoint = 0;			\
			(ee)->stackEnd = (void*)ptr;		\
			(ee)->stackBase = (ee)->stackEnd - threadStackSize;\
			(ee)->flags = THREAD_FLAGS_NOSTACKALLOC;\
		} while(0)

#define	THREADFRAMES(tid, cnt)					\
		do {						\
			void** ptr;				\
			cnt = 0;				\
			if (tid == currentThread) {		\
				asm("ta 3");			\
				asm("mov %%sp,%0" : "=r" (ptr));\
			}					\
			else {					\
				ptr = ((void***)TCTX(tid)->restorePoint)[2];\
			}					\
			while (*ptr != 0) {			\
				cnt++;				\
				ptr = (void**)ptr[14];		\
			}					\
		} while (0)

#endif
