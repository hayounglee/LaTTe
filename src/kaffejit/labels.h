/* labels.h
 * Manage the labelling system.  These are used to provide the necessary
 * interlinking of branches and subroutine calls.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>
 */

#ifndef __label_h
#define __label_h

#define	Lnull		0x00	/* Unused label */

#define	Ltypemask	0x0F	/* Label type mask */
#define Lquad		0x01	/* Label is 64 bits long */
#define	Llong		0x02	/* Label is 32 bits long */
#define	Llong30		0x03	/* Label is 30 bits long (SPARC) */
#define	Llong22		0x04	/* Label is 22 bits long (SPARC) */
#define	Llong22x10	0x05	/* Label is split into 22 and 10 bits (SPARC) */
#define Llong21		0x06	/* Label is 21 bits long (Alpha) */
#define Llong16x16	0x07	/* Label is split into 16 bit parts (Alpha) */
#define Lshort16	0x08	/* Label is 16 bits long (Alpha) */
#define	Lframe		0x09	/* Label is the frame size */
#define Lnegframe	0x0A	/* Label is the negative frame size */

/* Modifications to "to"  */
#define Ltomask		0x1F0
#define	Lgeneral	0x010	/* Label references general code */
#define Lexternal	0x020	/* Label references external routine */
#define	Lcode		0x040	/* Label references bytecode offset */
#define Lconstant	0x080	/* Label references a constpool element */
#define	Linternal	0x100	/* Label references internal routine */

/* Modifications to "from" */
#define Lfrommask	0x600
#define	Labsolute	0x200	/* Absolute value */
#define	Lrelative	0x400	/* Relative value to place of insertion */

#define Lrangecheck	0x1000	/* Check for overflow in the fixup */

typedef struct _label_ {
	struct _label_*	next;
	uintp		at;
	uintp		to;
	uintp		from;
	int		type;
} label;

#define	ALLOCLABELNR	1024

void linkLabels(uintp);
label* newLabel(void);
void resetLabels(void);

#endif
