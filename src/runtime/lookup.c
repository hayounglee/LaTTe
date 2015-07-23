/*
 * lookup.c
 * Various lookup calls for resolving objects, methods, exceptions, etc.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 * Modified by MASS Laboratory, SNU, 1999.
 */

#include <strings.h>
#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "jtypes.h"
#include "errors.h"
#include "access.h"
#include "object.h"
#include "constants.h"
#include "classMethod.h"
#include "lookup.h"
#include "exception.h"
#include "thread.h"
#include "baseClasses.h"
#include "class_inclusion_test.h"

#ifdef TRANSLATOR
#include "SpecializedMethod.h"
#include "translate.h"
#include "method_inlining.h"
#endif /* TRANSLATOR */

#ifdef INTERPRETER
#include "interpreter.h"
#endif

#if defined(TRANSLATOR) && !defined(INTERPRETER)
#include "machine.h"
#endif

#define DBG(s)
#define FDBG(s)
#define EDBG(s)

static void throwNoSuchMethodError(void);
static void throwAbstractMethodError(void);

/*
 * Lookup a method reference and get the various interesting bits.
 */
Method *
getMethodSignatureClass(constIndex idx, Method* meth) {
    constants* pool;
    constIndex ci;
    constIndex ni;
    constIndex nin;
    constIndex nis;
    Hjava_lang_Class* class;
    Utf8Const* name;
    Utf8Const* sig;
    Method* mptr;
    Hjava_lang_Class* cptr;
    int nmethods;
    dispatchTable* dtable;

    pool = CLASS_CONSTANTS(meth->class);
    if (pool->tags[idx] != CONSTANT_Methodref &&
        pool->tags[idx] != CONSTANT_InterfaceMethodref) {
DBG(		printf("No Methodref found\n");				)
                throwNoSuchMethodError();
    }

    ni = METHODREF_NAMEANDTYPE(idx, pool);
    nin = NAMEANDTYPE_NAME(ni, pool);
    name = WORD2UTF(pool->data[nin]);
    nis = NAMEANDTYPE_SIGNATURE(ni, pool);
    sig = WORD2UTF(pool->data[nis]);

    ci = METHODREF_CLASS(idx, pool);
    if (pool->tags[ci] == CONSTANT_Class) {
        getClass(ci, meth->class);
    }
    class = CLASS_CLASS(ci, pool);
    processClass(class, CSTATE_LINKED);

DBG(	printf("getMethodSignatureClass(%s,%s,%s)\n",
		class->name->data, name->data, sig->data);		)

    if (((pool->tags[idx] == CONSTANT_Methodref)
         &&(class->accflags & ACC_INTERFACE))
        ||((pool->tags[idx] == CONSTANT_InterfaceMethodref)
           &&!(class->accflags & ACC_INTERFACE))) {
        throwException(IncompatibleClassChangeError);
    }

    /*
     * We now have a complete class and method signature.  We can
     * Now work out some things we might need to know, like no. of
     * arguments, address of function, etc.
     */

    /* Find method */
    for (cptr = class; cptr != 0; cptr = cptr->superclass) {
        nmethods = CLASS_NMETHODS(cptr);
        for (mptr = CLASS_METHODS(cptr); --nmethods >= 0; ++mptr) {
            if (equalUtf8Consts (name, mptr->name)
                && equalUtf8Consts (sig, mptr->signature)) {
                /* Found method - but if it's abstract, point
                 * it at some error code.
                 */
                if ((mptr->accflags & ACC_ABSTRACT) != 0) {
                    METHOD_NATIVECODE(mptr) =
                        (void*)throwAbstractMethodError;
                    mptr->accflags |= ACC_NATIVE;
                }

		checkAccess(meth->class, cptr, mptr->accflags);

                return mptr;
            }
        }
    }       

    /* Not found - allocate a dummy method which will generate an exception
     * and use that for any calls.
     */
    dtable = (dispatchTable *) 
        gc_malloc(sizeof(dispatchTable) 
                  + ((class->msize + 1) * sizeof(void*)), 
                  &gc_dispatch_table);

    for (nmethods = 0; nmethods < class->msize; nmethods++) {
        dtable->method[nmethods] = class->dtable->method[nmethods];
    }

    class->msize++;
    class->dtable = dtable;
    GC_WRITE(class, dtable);

    mptr = gc_malloc(sizeof(Method), &gc_method);
    dtable->method[nmethods] = mptr;
    mptr->accflags = ACC_NATIVE;
    mptr->name = name;
    mptr->signature = sig;
    METHOD_NATIVECODE(mptr) = (void*)throwNoSuchMethodError;
    mptr->class = class;
    mptr->idx = nmethods;

#ifdef INTERPRETER
    mptr->native_caller = intrp_native_caller(mptr);
#endif

    countInsAndOuts(sig->data, &mptr->in, &mptr->out, &mptr->rettype);

    return mptr;
}

/*
 * Extra argument information from a class method.
 */
void
getMethodArguments(constIndex idx, Method* meth, callInfo* call)
{
	constants* pool = CLASS_CONSTANTS (meth->class);
	constIndex ni;
	constIndex nis;
	Utf8Const* sig;

	if (pool->tags[idx] != CONSTANT_Methodref &&
	    pool->tags[idx] != CONSTANT_InterfaceMethodref) {
DBG(		printf("No Methodref found\n");				)
                throwNoSuchMethodError();
	}

	ni = METHODREF_NAMEANDTYPE(idx, pool);
	nis = NAMEANDTYPE_SIGNATURE(ni, pool);
	sig = WORD2UTF(pool->data[nis]);

	call->signature = sig->data;
	countInsAndOuts(sig->data, &call->in, &call->out, &call->rettype);
DBG(	printf("in = %d, out = %d\n", call->in, call->out);		)

}

Hjava_lang_Class*
getClass(constIndex idx, Hjava_lang_Class* this)
{
	constants* pool;
	Utf8Const *name;
	Hjava_lang_Class* class;

	pool = CLASS_CONSTANTS(this);
	if (pool->tags[idx] == CONSTANT_ResolvedClass) {
		return (CLASS_CLASS (idx, pool));
	}
	if (pool->tags[idx] != CONSTANT_Class) {
		throwException(ClassFormatError);
	}
	name = WORD2UTF(pool->data[idx]);
	/* Find the specified class.  We cannot use 'loadClassOrArray' here
	 * because the name is *not* a signature.
	 */
	if (name->data[0] == '[') {
		char* el_name = &name->data[1];
		Hjava_lang_Class* el_class = classFromSig(&el_name, this->loader);
		class = lookupArray(el_class);
	}
	else {
		class = loadClass(name, this->loader);
	}
	if (class == 0) {
		throwException(ClassNotFoundException(name->data));
	}
	pool->data[idx] = (ConstSlot)class;
	pool->tags[idx] = CONSTANT_ResolvedClass;

	/* Check access permissions.  If array, use actual class. */
	if (Class_IsArrayType(class)) {
		Hjava_lang_Class *realclass;

		realclass = Class_GetElementType(class);
		checkAccess(this, realclass, realclass->accflags);
	} else {
		checkAccess(this, class, class->accflags);
	}

	return (class);
}

Field*
getField(constIndex idx, bool isStatic, Method* meth, Hjava_lang_Class** retclass)
{
	constants* pool = CLASS_CONSTANTS(meth->class);
	constIndex ci;
	constIndex ni;
	constIndex nin;
	constIndex nis;
	Field* field;
	Hjava_lang_Class* class;

	if (pool->tags[idx] != CONSTANT_Fieldref) {
FDBG(		printf("No Fieldref found\n");				)
                throwException(NoSuchFieldError);
	}

	ci = FIELDREF_CLASS(idx, pool);
	ni = FIELDREF_NAMEANDTYPE(idx, pool);
	nin = NAMEANDTYPE_NAME(ni, pool);
	nis = NAMEANDTYPE_SIGNATURE(ni, pool);
	if (pool->tags[ci] == CONSTANT_Class) {
		getClass(ci, meth->class);
	}
	class = CLASS_CLASS (ci, pool);

FDBG(	printf("*** getField(%s,%s,%s)\n",
	       class->name->data, WORD2UTF(pool->data[nin])->data,
	       WORD2UTF(pool->data[nis])->data); )

	field = lookupClassField(class, WORD2UTF(pool->data[nin]), isStatic);
	if (field == 0) {
FDBG(		printf("Field not found\n");				)
                throwException(NoSuchFieldError);
	}

	checkAccess(meth->class, class, field->accflags);

	(*retclass) = class;
	return (field);
}

Method*
find_clinit( Hjava_lang_Class* class )
{
    Method*   method;
    int       n = CLASS_NMETHODS( class );

    for ( method = CLASS_METHODS( class );
          --n >= 0;
          method++ ) {
        if ( equalUtf8Consts( init_name, method->name )
             && equalUtf8Consts( void_signature, method->signature ) ) {
#if !defined(INTERPRETER)
#ifdef CUSTOMIZATION
            SMS_AddSM( method->sms, SM_new( GeneralType, method ) );
#endif
            kaffe_translate( method ); 
#endif /* not INTERPRETER */

            return method;
        }
    }

    return NULL;
}

/*
 * Check whether a class A can use an item in class B.
 */
void
checkAccess(Hjava_lang_Class* a, Hjava_lang_Class* b, accessFlags flags)
{
	assert(!Class_IsArrayType(a));
	assert(!Class_IsArrayType(b));

	/* The translator is unable to handle exceptions gracefully,
           so do not do access permission checks when the translator
           is built.

	   FIXME: Access checks should always be done. */
#ifndef TRANSLATOR
	if (flags & ACC_PUBLIC) {
		return;
	}

	if (flags & ACC_PRIVATE) {
		if (a != b) {
			throwException(IllegalAccessError);
		} else {
			return;
		}
	}

	if (flags & ACC_PROTECTED) {
		if (CIT_is_subclass(a, b)) {
			return;
		}

		/* Otherwise, the same access restrictions for the
                   default access mode apply. */
	}

	/* Check for default access. */
	{
		char *aname, *bname;
		char *apkgend, *bpkgend;

		/* Check for an easy to check case first. */
		if (a == b) {
			return;
		}

		/* Otherwise check if A and B are same package. */

		/* Obtain package names. */
		aname = CLASS_CNAME(a);
		bname = CLASS_CNAME(b);
		apkgend = strrchr(aname, '/');
		bpkgend = strrchr(bname, '/');

		/* Handle classes in the unnamed package. */
		apkgend = (apkgend == NULL)? aname: apkgend;
		bpkgend = (bpkgend == NULL)? bname: bpkgend;

		if ((apkgend - aname) != (bpkgend - bname)
		    || strncmp(aname, bname, apkgend - aname) != 0) {
			throwException(IllegalAccessError);
		}
	}
#endif /* TRANSLATOR */
}

/*
 * Lookup a method (and translate) in the specified class.
 */
Method*
findMethodLocal(Hjava_lang_Class* class, Utf8Const* name, Utf8Const* signature)
{
    Method* mptr;
    int n;
    /*
     * Lookup method - this could be alot more efficient but never mind.
     * Also there is no attempt to honour PUBLIC, PRIVATE, etc.
     */
    n = CLASS_NMETHODS(class);
    for (mptr = CLASS_METHODS(class); --n >= 0; ++mptr) {
        if (equalUtf8Consts (name, mptr->name)
            && equalUtf8Consts (signature, mptr->signature)) {
            return (mptr);
        }
    }
    return (NULL);
}

/*
 * Lookup a method (and translate) in the specified class.
 */
Method*
findMethod(Hjava_lang_Class* class, Utf8Const* name, Utf8Const* signature)
{
    /*
     * Waz CSTATE_LINKED - Must resolve constants before we do any
     * translation.  Might not be right though ... XXX
     */
    if ( class->state < CSTATE_OK ) {
        processClass( class, CSTATE_OK );
    }

    /*
     * Lookup method - this could be alot more efficient but never mind.
     * Also there is no attempt to honour PUBLIC, PRIVATE, etc.
     */
    for (; class != 0; class = class->superclass) {
        Method* mptr = findMethodLocal(class, name, signature);
        if (mptr != NULL) {
            return mptr;
        }
    }
    return (0);
}

#if defined(TRANSLATOR)
/*
 * Find exception in method.
 */
void
findExceptionInMethod(uintp pc, Hjava_lang_Class* class, exceptionInfo* info)
{
    MethodInstance *ptr_mi;
	Method* ptr;

	info->handler = 0;
	info->class = 0;
	info->method = 0;

	ptr_mi = findMethodFromPC(pc);
    ptr = MI_GetMethod(ptr_mi);

	if (ptr != 0) {
		if (findExceptionBlockInMethod(pc, class, ptr, info) == true) {
			return;
		}
	}
EDBG(	printf("Exception not found.\n");				)
}
#endif

/*
 * Look for exception block in method.
 */
bool
findExceptionBlockInMethod(uintp pc, Hjava_lang_Class* class, Method* ptr, exceptionInfo* info)
{
	jexceptionEntry* eptr;
	int i;

	/* Stash method & class */
	info->method = ptr;
	info->class = ptr->class;

	eptr = &ptr->exception_table->entry[0];

EDBG(	printf("Nr of exceptions = %d\n", ptr->exception_table.length); )

	/* Right method - look for exception */
	if (ptr->exception_table == 0) {
		return (false);
	}
	for (i = 0; i < ptr->exception_table->length; i++) {
		uintp start_pc = eptr[i].start_pc;
		uintp end_pc = eptr[i].end_pc;
		uintp handler_pc = eptr[i].handler_pc;

EDBG(		printf("Exceptions %x (%x-%x)\n", pc, start_pc, end_pc); )
		if (pc < start_pc || pc > end_pc) {
			continue;
		}
EDBG(		printf("Found exception 0x%x\n", handler_pc); )

		/* Found exception - is it right type */
		if (eptr[i].catch_idx == 0) {
			info->handler = handler_pc;
			return (true);
		}
		/* Resolve catch class if necessary */
		if (eptr[i].catch_type == NULL) {
			eptr[i].catch_type = getClass(eptr[i].catch_idx, ptr->class);
		}

        if (CIT_is_subclass( class, eptr[i].catch_type )) {
            info->handler = handler_pc;
            return true;
        }
	}
	return (false);
}

static
void
throwNoSuchMethodError(void)
{
	throwException(NoSuchMethodError);
}

static
void
throwAbstractMethodError(void)
{
	throwException(AbstractMethodError);
}


