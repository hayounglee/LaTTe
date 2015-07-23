/*
 * code.c
 * Process a new code attribute.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 * Modified by MASS Laboratory, SNU, 1999.
 */

#define	DBG(s)

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "jtypes.h"
#include "file.h"
#include "errors.h"
#include "bytecode.h"
#include "code.h"
#include "access.h"
#include "object.h"
#include "classMethod.h"
#include "readClass.h"
#include "exception.h"

void
addCode(Method* m, uint32 len, classFile* fp)
{
	Code c;
	int i;
	u2 i2;
	u2 elen;

	readu2(&c.max_stack, fp);
	readu2(&c.max_locals, fp);
	readu4(&c.code_length, fp);
DBG(	printf("Max stack = %d\n", c.max_stack);	)
DBG(	printf("Max locals = %d\n", c.max_locals);	)
DBG(	printf("Code length = %d\n", c.code_length);	)
	if (c.code_length > 0) {
		c.code = gc_malloc(c.code_length, &gc_bytecode);
		readm(c.code, c.code_length, sizeof(byte), fp);
	}
	else {
		c.code = 0;
	}
	readu2(&elen, fp);
DBG(	printf("Exception table length = %d\n", elen);	)
	if (elen > 0) {
	    c.exception_table =
		gc_malloc(sizeof(jexception) +
			  ((elen - 1) * sizeof(jexceptionEntry)),
			  &gc_exception_table);
		c.exception_table->length = elen;
		for (i = 0; i < elen; i++) {
			readu2(&i2, fp);
			c.exception_table->entry[i].start_pc = i2;
			readu2(&i2, fp);
			c.exception_table->entry[i].end_pc = i2;
			readu2(&i2, fp);
			c.exception_table->entry[i].handler_pc = i2;
			readu2(&i2, fp);
			c.exception_table->entry[i].catch_idx = i2;
			c.exception_table->entry[i].catch_type = NULL;

		}
	} else {
		c.exception_table = 0;
	}
	GC_WRITE(m, c.code);
	GC_WRITE(m, c.exception_table);
	addMethodCode(m, &c);

	readAttributes(fp, m->class, m);
}

/*
 * Read in line numbers assocated with code.
 */
void
addLineNumbers(Method* m, uint32 len, classFile* fp)
{
	lineNumbers* lines;
	int i;
	u2 nr;
	u2 data;

	readu2(&nr, fp);

	lines = gc_malloc_fixed(sizeof(lineNumbers)
				+ sizeof(lineNumberEntry) * nr);
	lines->length = nr;
	for (i = 0; i < nr; i++) {
		readu2(&data, fp);
		lines->entry[i].start_pc = data;
		readu2(&data, fp);
		lines->entry[i].line_nr = data;
	}

	/* Attach lines to method */
	GC_WRITE(m, lines);
	m->lines = lines;
}
