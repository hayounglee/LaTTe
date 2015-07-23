/* labels.c
 * Manage the code labels and links.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>
 */

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "gtypes.h"
#include "labels.h"
#include "code-analyse.h"
#include "itypes.h"
#include "seq.h"
#include "constpool.h"
#include "gc.h"
#include "md.h"

label* firstLabel;
label* lastLabel;
label* currLabel;

extern int maxPush;
extern int maxLocal;
extern int maxArgs;
extern int maxStack;
extern int maxTemp;

void
resetLabels(void)
{
	currLabel = firstLabel;
}

void
linkLabels(uintp codebase)
{
	long dest, from, val;
	int* place;
	label* l;

	for (l = firstLabel; l != currLabel; l = l->next) {

		/* Ignore this label if it hasn't been used */
		if (l->type == Lnull) {
			continue;
		}

		/* Find destination of label */
		dest = l->to;
		switch (l->type & Ltomask) {
		case Lgeneral:
		case Lexternal:
			break;
		case Lcode:
			assert(INSNPC(dest) != (uintp)-1);
			dest = INSNPC(dest) + codebase;
			break;
		case Lconstant:
			dest = ((constpool*)dest)->at;
			break;
		case Linternal:
			dest += codebase;
			break;
		default:
			goto unhandled;
		}

		/* Find the source of the reference */
		switch (l->type & Lfrommask) {
		case Labsolute:
			from = 0;
			break;
		case Lrelative:
			from = l->from + codebase;
			break;
		default:
			goto unhandled;
		}

		/* Find place to store result */
		place = (int*)(l->at + codebase);

		/* Install result */
		val = dest - from;
		switch (l->type & Ltypemask) {
		case Lquad:
			*(int64*)place = val;
			break;
		case Llong:
			assert(!(l->type & Lrangecheck)
			       || sizeof(val) == 4
			       || (val >= -0x80000000L && val < 0x80000000L));
			*place = val;
			break;
		case Llong30:
			*place = ((*place & 0xC0000000)
				  | ((val / 4) & 0x3FFFFFFF));
			break;
		case Llong22:
			*place = ((*place & 0xFFC00000)
				  | ((val / 4) & 0x003FFFFF));
			break;
		case Llong22x10:
			place[0] = ((place[0] & 0xFFC00000)
				    | ((val >> 10) & 0x003FFFFF));
			place[1] = ((place[1] & 0xFFFFFC00)
				    | (val & 0x000003FF));
			break;

		case Llong21:
			assert(!(l->type & Lrangecheck)
			       || (val % 4 == 0
				   && val >= -0x400000 && val < 0x400000));

			*place = ((*place & 0xFFE00000)
				  | ((val / 4) & 0x001FFFFF));
			break;

		case Llong16x16:
			from = (short)val;
			dest = (val - from) >> 16;

			assert(!(l->type & Lrangecheck)
			       || ((((dest & 0xFFFF) << 16 | (from & 0xFFFF))
				    ^ 0x80008000) - 0x80008000) == val);

			place[0] = (place[0] & 0xFFFF0000) | (dest & 0xFFFF);
			place[1] = (place[1] & 0xFFFF0000) | (from & 0xFFFF);
			break;

		case Lshort16:
			assert(!(l->type & Lrangecheck)	
			       || val == (short)val);
			*(short *)place = val;
			break;

		case Lframe:
		case Lnegframe:
			LABEL_FRAMESIZE(l, place);
			break;

		default:
		unhandled:
			printf("Label type 0x%x not supported (%p).\n",
			       l->type, l);
			ABORT();
		}
	}
}

/*
 * Allocate a new label.
 */
label*
newLabel(void)
{
	int i;
	label* ret = currLabel;

	if (ret == 0) {
		/* Allocate chunk of label elements */
		ret = gc_calloc_fixed(ALLOCLABELNR, sizeof(label));

		/* Attach to current chain */
		if (lastLabel == 0) {
			firstLabel = ret;
		}
		else {
			lastLabel->next = ret;
		}
		lastLabel = &ret[ALLOCLABELNR-1];

		/* Link elements into list */
		for (i = 0; i < ALLOCLABELNR-1; i++) {
			ret[i].next = &ret[i+1];
		}
		ret[ALLOCLABELNR-1].next = 0;
	}
	currLabel = ret->next;
	return (ret);
}
