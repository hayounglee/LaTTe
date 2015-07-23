/*
 * constants.c
 * Constant management.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 * Modified by MASS Laboratory, SNU, 1999.
 */

#define	RDBG(s)

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "gtypes.h"
#include "access.h"
#include "constants.h"
#include "classMethod.h"
#include "file.h"
#include "errors.h"
#include "exception.h"

/*
 * Read in constant pool from opened file.
 */
void
readConstantPool(Hjava_lang_Class* this, classFile* fp)
{
	constants* info = CLASS_CONSTANTS (this);
	ConstSlot* pool;
	u1* tags;
	int i;
	int j;
	u1 type;
	u2 len;
	u2 d2, d2b;
	u4 d4, d4b;
	bool error;

	readu2(&info->size, fp);
RDBG(	printf("constant_pool_count=%d\n", info->size);	)

	/* Allocate space for tags and data */
	pool = gc_malloc((sizeof(ConstSlot) + sizeof(u1)) * info->size,
			 &gc_constant);

#ifdef INTERPRETER
	this->resolved = gc_malloc_fixed(sizeof(void*) * info->size);
#endif

	tags = (u1*)&pool[info->size];
	GC_WRITE(this, pool);
	info->data = pool;
	info->tags = tags;

	error = false;
	pool[0] = 0;
	tags[0] = CONSTANT_Unknown;
	for (i = 1; i < info->size; i++) {

		readu1(&type, fp);
RDBG(		printf("Constant type %d\n", type);			)
		tags[i] = type;

		switch (type) {
		case CONSTANT_Utf8:
			readu2(&len, fp);
			pool[i] = (ConstSlot) makeUtf8Const (fp->buf, len);
			fp->buf += len;
			break;
		case CONSTANT_Class:
		case CONSTANT_String:
			readu2(&d2, fp);
			pool[i] = d2;
			break;

		case CONSTANT_Fieldref:
		case CONSTANT_Methodref:
		case CONSTANT_InterfaceMethodref:
		case CONSTANT_NameAndType:
			readu2(&d2, fp);
			readu2(&d2b, fp);
			pool[i] = (d2b << 16) | d2;
			break;

		case CONSTANT_Integer:
		case CONSTANT_Float:
			readu4(&d4, fp);
			pool[i] = d4;
			break;

		case CONSTANT_Long:
		case CONSTANT_Double:
			readu4(&d4, fp);
			readu4(&d4b, fp);
#if SIZEOF_VOIDP == 8
			pool[i] = WORDS_TO_LONG(d4, d4b);
			i++;
			pool[i] = 0;
#else
			pool[i] = d4;
			i++;
			pool[i] = d4b;
#endif
			tags[i] = CONSTANT_Unknown;
			break;

		default:
			throwException(ClassFormatError);
			break;
		}
	}

	/* Perform some constant pool optimisations to allow for the
	 * use of pre-compiled classes.
	 */
	for (i = 1; i < info->size; i++) {
		switch (info->tags[i]) {
		case CONSTANT_Class:
		case CONSTANT_String:
			j = CLASS_NAME(i, info);
			if (info->tags[j] == CONSTANT_Utf8) {
				/* Rewrite so points directly at string */
				info->data[i] = info->data[j];
			}
			else {
				/* Set this tag so it will generate an error
				 * during verification.
				 */
				info->tags[i] = CONSTANT_Error;
			}
			break;

		default:
			/* Ignore the rest */
			break;
		}
	}
}

