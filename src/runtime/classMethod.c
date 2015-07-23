/*
 * classMethod.c
 * Dictionary of classes, methods and fields.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 * Modified by MASS Laboratory, SNU, 1999.
 */

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "gtypes.h"
#include "access.h"
#include "object.h"
#include "classMethod.h"
#include "code.h"
#include "file.h"
#include "readClass.h"
#include "baseClasses.h"
#include "thread.h"
#include "itypes.h"
#include "errors.h"
#include "exception.h"
#include "md.h"
#include "external.h"
#include "lookup.h"
#include "support.h"
#include "gc.h"
#include "locks.h"
#include "class_inclusion_test.h"
#include "exception_handler.h"

#ifdef CUSTOMIZATION
#include "SpecializedMethod.h"
#endif /* CUSTOMIZATION */

#ifdef INLINE_CACHE
#include "TypeChecker.h"
#endif

#ifdef INTERPRETER
#include "interpreter.h"
#include "flags.h"
#endif /* INTERPRETER */

#ifdef METHOD_COUNT
#include "flags.h"
#endif

#ifdef TRANSLATOR
#include "translator_driver.h"
#include "method_inlining.h"
#include "exception_info.h"
#include "translate.h"
#include "cell_node.h"
#endif /* TRANSLATOR */

#ifdef DYNAMIC_CHA
#include "dynamic_cha.h"
#endif

#undef INLINE
#define INLINE
#include "classMethod.ic"

#define DBG(s)
#define	CDBG(s)
#define	MDBG(s)
#define	FDBG(s)
#define	LDBG(s)

#define	CLASSHASHSZ		256	/* Must be a power of two */
static Hjava_lang_Class* classPool[CLASSHASHSZ];


unsigned long total_interface = 1;

extern Hjava_lang_Class* findClass(Hjava_lang_Class*, char*);
extern void verify2(Hjava_lang_Class*);
extern void verify3(Hjava_lang_Class*);

static Hjava_lang_Class* simpleLookupClass(Utf8Const*, Hjava_lang_ClassLoader*);
static Hjava_lang_Class* internalAddClass(Hjava_lang_Class*, Utf8Const*, int, int, Hjava_lang_ClassLoader*);

static void buildDispatchTable(Hjava_lang_Class*);
static void buildInterfaceTable(Hjava_lang_Class*);
static void allocStaticFields(Hjava_lang_Class*);
static void resolveObjectFields(Hjava_lang_Class*);
static void resolveStaticFields(Hjava_lang_Class*);
static void resolveConstants(Hjava_lang_Class*);

#if !defined(ALIGNMENT_OF_SIZE)
#define	ALIGNMENT_OF_SIZE(S)	(S)
#endif

/*
 * Process all the stage of a classes initialisation.  We can provide
 * a state to aim for (so we don't have to do this all at once).  This
 * is called by various parts of the machine in order to load, link
 * and initialise the class.  Putting it all together here makes it a damn
 * sight easier to understand what's happening.
 */
void
processClass(Hjava_lang_Class* class, int tostate)
{
    int i;
    Method* meth;
    Hjava_lang_Class* super;

//    assert(tostate == CSTATE_LINKED || tostate == CSTATE_OK);

#define	SET_CLASS_STATE(S)	class->state = (S)
#define	DO_CLASS_STATE(S)	if ((S) > class->state && (S) <= tostate)

    /* Lock the class while we do this to avoid more than one
     * thread doing this at once.  Simply return if we're in
     * CSTATE_DOING_INIT, in order to avoid suffering from a
     * deadlock (e.g. GC called during class initialization could
     * cause a deadlock because of an attempt to initialize the
     * same class during finalization).  */
//    if (class->state == CSTATE_DOING_INIT)
//      return;
//    else
    lockMutexInt(&class->loadingLock);
    
    DO_CLASS_STATE(CSTATE_LOADED) {
        
        /* Class not loaded - now would be a good time */
        if (findClass(class, class->name->data) == 0) {
            unlockMutexInt(& class->loadingLock);
            throwException(NoClassDefFoundError(class->name->data));
        }
    }

    DO_CLASS_STATE(CSTATE_PREPARED) {
        
        /* Check for circular dependent classes */
        if (class->state == CSTATE_DOING_PREPARE) {
            unlockMutexInt(& class->loadingLock);
            throwException(ClassCircularityError);
        }

        SET_CLASS_STATE(CSTATE_DOING_PREPARE);
        
        /* Allocate any static space required by class and initialise
         * the space with any constant values.
         */
        allocStaticFields(class);

        /* Load and link the super class */
        if (class->superclass) {
	    //
	    // Very interesting!!!
	    // When a class is loaded by a special loader,
	    // its superclasses must be loaded with the class's loader.
	    // FIXME:
	    // I'm not sure if LaTTe's type inferring system works correctly
	    // when two classes have the same name and different loaders.
	    // - doner
	    //
            class->superclass =
                getClass((uintp)class->superclass, class);

            processClass(class->superclass, CSTATE_LINKED);
            CLASS_FSIZE(class) = CLASS_FSIZE(class->superclass);

            if (class->superclass->accflags & ACC_FINAL) {
                unlockMutexInt(& class->loadingLock);
                throwException(LinkageError);
            }

        }
        
        /* Load all the implemented interfaces. */
        for (i = 0;  i < class->interface_len;  i++) {
            uintp iface = (uintp)class->interfaces[i];
            class->interfaces[i] = getClass(iface, class);
        }

        resolveObjectFields(class);
        resolveStaticFields(class);

        // NEW_INTERFACETABLE is new version of NEW_INVOKEINTERFACE
#ifdef NEW_INTERFACETABLE
        /* Build dispatch table */
        buildDispatchTable(class);
        if ((class->accflags & ACC_INTERFACE) == 0) {
            buildInterfaceTable(class);
        }

#else
        /* Build dispatch table */
        if ((class->accflags & ACC_INTERFACE) == 0) {
            buildDispatchTable(class);
        }
        buildInterfaceTable(class);

#endif // NEW_INTERFACETABLE

        SET_CLASS_STATE(CSTATE_PREPARED);
    }

    DO_CLASS_STATE(CSTATE_LINKED) {

        /* Okay, flag we're doing link */
        SET_CLASS_STATE(CSTATE_DOING_LINK);
        
        /* Second stage verification - check the class format is okay */
        verify2(class);

        /* Third stage verification - check the bytecode is okay */
        verify3(class);

        CIT_update_class_hierarchy(class);

        /* And note that it's done */
        SET_CLASS_STATE(CSTATE_LINKED);
    }

    DO_CLASS_STATE(CSTATE_OK) {

	//
	// inserted by doner 99/8/5
	// In some cases, 'processClass' is called again by the same thread
	// initializing the class.
	// If so, just return.
	// Since the finalization can cause DEADLOCK, we disable GC during class init.
	//
	if (class->state == CSTATE_DOING_INIT) {
	    unlockMutexInt(& class->loadingLock);
	    return;
	}

        /* Initialise the constants */
        resolveConstants(class);

        SET_CLASS_STATE(CSTATE_DOING_INIT);

        /* According to Java Language Specification, class lock should
           be released before initialization process */
	unlockMutexInt(&class->loadingLock);

        super = class->superclass;
        if (super != NULL && super->state != CSTATE_OK) {
            processClass(super, CSTATE_OK);
        }

        /* Initialize all the implemented interfaces. */
        for (i = 0;  i < class->interface_len;  i++) {
            Hjava_lang_Class *interface;

            interface = class->interfaces[i];
            if (interface->state != CSTATE_OK)
              processClass(interface, CSTATE_OK);
        }

DBG(		printf("Initialising %s static %d\n", class->name->data,
			CLASS_SSIZE(class)); fflush(stdout);		)

        meth = find_clinit(class);
    
        if (meth != NULL) {
	    Hjava_lang_Object *err;
	    Hjava_lang_Class *eclass;

	    EH_NATIVE_DURING
#ifdef INTERPRETER
		int args[meth->localsz];   /* Local variable frame */

		/* Interpret static initializer. */
		intrp_execute(meth, &args[meth->localsz-1], meth->bcode);
#else /* not INTERPRETER */
		CALL_KAFFE_METHOD(METHOD_NATIVECODE(meth), 0);
#endif /* not INTERPRETER */
	    EH_NATIVE_HANDLER

		/* FIXME: label the class erroneous. */

		/* If the uncaught exception is not an instance of
		   Error, then the class loader is supposed to throw
		   an ExceptionInInitializerError instead.  Otherwise,
		   just throw the exception again. */
		eclass =
		    Object_GetClass((Hjava_lang_Object*)captive_exception);
		if (CIT_is_subclass(eclass, ErrorClass) || eclass == ErrorClass)
		    err = (Hjava_lang_Object*)captive_exception;
		else
		    err = ExceptionInInitializerError(captive_exception);
	        throwExternalException(err);
	    EH_NATIVE_ENDHANDLER
        }

        /* According to Java Language Specification, class lock should
           be obtained after initialization process finishes normally */
	lockMutexInt(&class->loadingLock);

	SET_CLASS_STATE(CSTATE_OK);
    }

    /* Unlock class now we're done */
    unlockMutexInt(& class->loadingLock);
}

Hjava_lang_Class*
addClass(Hjava_lang_Class* cl, constIndex c, constIndex s, u2 flags, Hjava_lang_ClassLoader* loader)
{
    constants* pool;

    pool = CLASS_CONSTANTS(cl);

	/* Find the name of the class */
    if (pool->tags[c] != CONSTANT_Class) {
CDBG(		printf("addClass: not a class.\n");			)
		return (0);
	}
    return(internalAddClass(cl, WORD2UTF(pool->data[c]), flags, s, loader));
}

#if INTERN_UTF8CONSTS
static
int
hashClassName(Utf8Const *name)
{
    int len = (name->length+1) >> 1;  /* Number of shorts */
    uint16 *ptr = (uint16*) name->data;
    int hash = len;
    while (--len >= 0)
        hash = (hash << 1) ^ *ptr++;
    return hash;
}
#else
#define hashClassName(NAME) ((NAME)->hash)
#endif

/*
 * Take a prepared class and register it with the class pool.
 */
void
registerClass(Hjava_lang_Class* cl)
{
    Hjava_lang_Class** clp;
    uint32 hash;

    hash = hashClassName(cl->name) & (CLASSHASHSZ-1);

    /* All classes are entered in the global classPool.
     * This is done, even for classes using a ClassLoader, following
     * the prescription of the "Java Language Specification"
     * (section 12.2:  Loading of Classes and Interfaces). */

    clp = &classPool[hash];
#ifdef DEBUG
    while (*clp != 0) {
        assert(! equalUtf8Consts (cl->name, (*clp)->name)
               || loader != (*clp)->loader);
        clp = &(*clp)->next;
    }
#endif
    /* Add into list. */
    cl->next = *clp;
    *clp = cl; 

}

static
Hjava_lang_Class*
internalAddClass(Hjava_lang_Class* cl, Utf8Const* name, int flags, int su, Hjava_lang_ClassLoader* loader)
{
    cl->name = name;
    CLASS_METHODS(cl) = NULL;
    CLASS_NMETHODS(cl) = 0;
    cl->superclass = (Hjava_lang_Class*)(uintp)su;
    cl->msize = 0;
    CLASS_FIELDS(cl) = 0;
    CLASS_FSIZE(cl) = 0;
    cl->accflags = flags;
    cl->dtable = 0;
#ifdef NEW_INVOKEINTERFACE
    cl->itable = 0;
#endif NEW_INVOKEINTERFACE
    cl->interfaces = 0;
    cl->interface_len = 0;
    cl->state = CSTATE_LOADED;
    cl->final = false;
    cl->loader = loader;

    registerClass(cl);

    return (cl);
}

Method*
addMethod(Hjava_lang_Class* c, method_info* m)
{
    constIndex nc;
    constIndex sc;
    Method* mt;
    constants* pool;
    Utf8Const* name;
    Utf8Const* signature;

    pool = CLASS_CONSTANTS (c);

    nc = m->name_index;
    if (pool->tags[nc] != CONSTANT_Utf8) {
MDBG(		printf("addMethod: no method name.\n");			)
		return (0);
    }
    sc = m->signature_index;
    if (pool->tags[sc] != CONSTANT_Utf8) {
MDBG(		printf("addMethod: no signature name.\n");		)
		return (0);
    }
    name = WORD2UTF (pool->data[nc]);
    signature = WORD2UTF (pool->data[sc]);

    if (equalUtf8Consts (name, final_name)
        && equalUtf8Consts (signature, void_signature)) {
        c->final = true;
    }

#ifdef DEBUG
    /* Search down class for method name - don't allow duplicates */
    for (ni = CLASS_NMETHODS(c), mt = CLASS_METHODS(c); --ni >= 0;) {
        assert(! equalUtf8Consts (name, mt->name)
               || ! equalUtf8Consts (signature, mt->signature));        
    }       
#endif

MDBG(	printf("Adding method %s:%s%s (%x)\n", c->name->data, WORD2UTF(pool->data[nc])->data, WORD2UTF(pool->data[sc])->data, m->access_flags);	)

    mt = &c->methods[c->nmethods++];
    mt->name = name;
    mt->signature = signature;
    mt->class = c;
    if (mt->class->accflags & ACC_FINAL) {
        mt->accflags = m->access_flags | ACC_FINAL;
    } else {
        mt->accflags = m->access_flags;
    }
    mt->bcode = 0;
    mt->stacksz = 0;
    mt->localsz = 0;
    mt->exception_table = 0;

#ifdef CUSTOMIZATION
#if defined(INTERPRETER) && defined(METHOD_COUNT)
    mt->count = The_Retranslation_Threshold;
#endif /* INTERPRETER && METHOD_COUNT */

#else /* not CUSTOMIZATION */

#ifdef METHOD_COUNT
    mt->count = The_Retranslation_Threshold;
#endif /* METHOD_COUNT */

#endif /* not CUSTOMIZATION */

#ifdef INTERPRETER
    mt->resolved = c->resolved;
    mt->native_caller = intrp_native_caller(mt);
#endif /* INTERPRETER */

    countInsAndOuts(signature->data, &mt->in, &mt->out, &mt->rettype);

    mt->iidx = -1; //initialize interface index

#ifdef INLINE_CACHE
    mt->tcs = TCS_new(3);
#endif

#ifdef CUSTOMIZATION
    // 3 is initial value
    mt->sms = SMS_new(3);
#endif

    return (mt);
}

Field*
addField(Hjava_lang_Class* c, field_info* f)
{
    constIndex nc;
    constIndex sc;
    Field* ft;
    constants* pool;
    int index;

    pool = CLASS_CONSTANTS (c);

    nc = f->name_index;
    if (pool->tags[nc] != CONSTANT_Utf8) {
FDBG(		printf("addField: no field name.\n");			)
        return (0);
    }

    --CLASS_FSIZE(c);
    if (f->access_flags & ACC_STATIC) {
        index = CLASS_NSFIELDS(c)++;
    }
    else {
        index = CLASS_FSIZE(c) + CLASS_NSFIELDS(c);
    }
    ft = &CLASS_FIELDS(c)[index];

FDBG(	printf("Adding field %s:%s\n", c->name, pool->data[nc].v.tstr);	)

    sc = f->signature_index;
    if (pool->tags[sc] != CONSTANT_Utf8) {
FDBG(		printf("addField: no signature name.\n");		)
        return (0);
    }
    ft->name = WORD2UTF (pool->data[nc]);
    FIELD_TYPE(ft) = (Hjava_lang_Class*)CLASS_CONST_UTF8(c, sc);
    ft->accflags = f->access_flags | FIELD_UNRESOLVED_FLAG;
    FIELD_CONSTIDX(ft) = 0;

    return (ft);
}

void
setFieldValue(Field* ft, u2 idx)
{
    /* Set value index */
    FIELD_CONSTIDX(ft) = idx;
    ft->accflags |= FIELD_CONSTANT_VALUE;
}

void
finishFields(Hjava_lang_Class *cl)
{
    Field tmp;
    Field* fld;
    int n;

    /* Reverse the instance fields, so they are in "proper" order. */
    fld = CLASS_IFIELDS(cl);
    n = CLASS_NIFIELDS(cl);
    while (n > 1) {
        tmp = fld[0];
        fld[0] = fld[n-1];
        fld[n-1] = tmp;
        fld++;
        n -= 2;
    }
}

void
addMethodCode(Method* m, Code* c)
{
    m->bcode = c->code;
    m->bcodelen = c->code_length;
    m->stacksz = c->max_stack;
    m->localsz = c->max_locals;
    m->exception_table = c->exception_table;
    
#ifdef EXCEPTION_HACK
    CheckBytecodes(m);
#endif // EXCEPTION_HACK
}

void
addInterfaces(Hjava_lang_Class* c, int inr, Hjava_lang_Class** inf)
{
    assert(inr > 0);

    c->interfaces = inf;
    c->interface_len = inr;
}

static char loadClassName[] = "loadClass";
static char loadClassSig[] = "(Ljava/lang/String;Z)Ljava/lang/Class;";

/*
 * Lookup a named class, loading it if necessary.
 * The name is as used in class files, e.g. "java/lang/String".
 */
Hjava_lang_Class*
loadClass(Utf8Const* name, Hjava_lang_ClassLoader* loader)
{
    Hjava_lang_Class* class;

    class = simpleLookupClass(name, loader);
    if (class != NULL) {
        return (class);
    }

    /* Failed to find class, so must now load it */

    if (loader != NULL) {
        Hjava_lang_String* str;
                
LDBG(		printf("classLoader: loading %s\n", name->data);)
        str = makeReplaceJavaStringFromUtf8(name->data, name->length, '/', '.');
        class = (Hjava_lang_Class*)do_execute_java_method (NULL, (Hjava_lang_Object*)loader, loadClassName, loadClassSig, 0, false, str, true);
LDBG(		printf("classLoader: done %i\n", class->state);			)
        if (class == NULL) {
            throwException(ClassNotFoundException(name->data));
        }
    }
    else {
        class = (struct Hjava_lang_Class*)newObject(ClassClass);
        class->name = name;
        class->state = CSTATE_UNLOADED;
    }

    processClass(class, CSTATE_LINKED);
    return (class);
}

void
loadStaticClass(Hjava_lang_Class* class, char* name)
{
    class->name = makeUtf8Const(name, -1);
    class->state = CSTATE_UNLOADED;
    processClass(class, CSTATE_LINKED);
    gc_attach(class, &gc_class_object);
    class->head.dtable = ClassClass->dtable;
#ifndef NEW_INTERFACETABLE
    class->head.itable = ClassClass->itable;
#endif // NEW_INTERFACETABLE
}

Hjava_lang_Class*
lookupClass(char* name)
{
    Hjava_lang_Class* class;
    
    class = loadClass(makeUtf8Const (name, strlen (name)), NULL);
    if (class->state != CSTATE_OK) {
        processClass(class, CSTATE_OK);
    }
    return (class);
}



static
void
resolveObjectFields(Hjava_lang_Class* class)
{
    int fsize;
    int align;
    char* sig;
    Field* fld;
    int n;
    int offset;

    /* Find start of new fields in this object.  If start is zero, we must
     * allow for the object headers.
     */
    offset = CLASS_FSIZE(class);
    if (offset == 0) {
        offset = sizeof(Hjava_lang_Object);
    }
    else {
        class->refField = class->superclass->refField;
    }

    /* Find the largest alignment in this class */
    align = 1;
    fld = CLASS_IFIELDS(class);
    n = CLASS_NIFIELDS(class);
    for (; --n >= 0; fld++) {
        if (FIELD_RESOLVED(fld)) {
            fsize = FIELD_SIZE(fld);
        }
        else {
            sig = ((Utf8Const*)FIELD_TYPE(fld))->data;
            if (sig[0] == 'L' || sig[0] == '[') {
                fsize = PTR_TYPE_SIZE;
            }
            else {
                FIELD_TYPE(fld) = classFromSig(&sig, class->loader);
                fsize = TYPE_PRIM_SIZE(FIELD_TYPE(fld));
                fld->accflags &= ~FIELD_UNRESOLVED_FLAG;
            }
            FIELD_SIZE(fld) = fsize;
        }
        
        /* Work out alignment for this size entity */
        fsize = ALIGNMENT_OF_SIZE(fsize);
        
        /* If field is bigger than biggest alignment, change
         * biggest alignment
         */
        if (fsize > align) {
            align = fsize;
        }
    }

    /* Align start of this class's data */
    offset = ((offset + align - 1) / align) * align;

    /* Now work out where to put each field */
    fld = CLASS_IFIELDS(class);
    n = CLASS_NIFIELDS(class);
    for (; --n >= 0; fld++) {
        if (FIELD_RESOLVED(fld)) {
            fsize = FIELD_SIZE(fld);
        }
        else {
            /* Unresolved fields must be object references */
            fsize = PTR_TYPE_SIZE;
        }
        /* Align field */
        align = ALIGNMENT_OF_SIZE(fsize);
        offset = ((offset + align - 1) / align) * align;
        FIELD_OFFSET(fld) = offset;
        if (FIELD_ISREF(fld)) {
            class->refField++;
        }
        offset += fsize;
    }
    
    CLASS_FSIZE(class) = offset;
}

/*
 * Allocate the space for the static class data.
 */
static
void
allocStaticFields(Hjava_lang_Class* class)
{
    int fsize;
    int align;
    char* sig;
    uint8* mem;
    int offset;
    int n;
    Field* fld;
    
    /* No static fields */
    if (CLASS_NSFIELDS(class) == 0) {
        return;
    }

    /* Calculate size and position of static data */
    offset = 0;
    n = CLASS_NSFIELDS(class);
    fld = CLASS_SFIELDS(class);
    for (; --n >= 0; fld++) {
        sig = ((Utf8Const*)FIELD_TYPE(fld))->data;
        if (sig[0] == 'L' || sig[0] == '[') {
            fsize = PTR_TYPE_SIZE;
        }
        else {
            assert(!FIELD_RESOLVED(fld));
            FIELD_TYPE(fld) = classFromSig(&sig, class->loader);
            fsize = TYPE_PRIM_SIZE(FIELD_TYPE(fld));
            fld->accflags &= ~FIELD_UNRESOLVED_FLAG;
        }
        /* Align field offset */
        if (fsize < 4) {
            fsize = 4;
        }
        align = fsize;
        offset = ((offset + align - 1) / align) * align;
        FIELD_SIZE(fld) = offset;
        offset += fsize;
    }

    /* Allocate memory required */
    mem = gc_malloc(offset, &gc_static_data);

    /* Rewalk the fields, pointing them at the relevant memory and/or
     * setting any constant values.
     */
    fld = CLASS_SFIELDS(class);
    n = CLASS_NSFIELDS(class);
    for (; --n >= 0; fld++) {
        offset = FIELD_SIZE(fld);
        FIELD_SIZE(fld) = FIELD_CONSTIDX(fld);	/* Keep idx in size */
        FIELD_ADDRESS(fld) = mem + offset;
    }
}

static
void
resolveStaticFields(Hjava_lang_Class* class)
{
    uint8* mem;
    constants* pool;
    Field* fld;
    int idx;
    int n;

    pool = CLASS_CONSTANTS(class);
    fld = CLASS_SFIELDS(class);
    n = CLASS_NSFIELDS(class);
    for (; --n >= 0; fld++) {
        if ((fld->accflags & FIELD_CONSTANT_VALUE) != 0) {
            mem = FIELD_ADDRESS(fld);
            idx = FIELD_SIZE(fld);

            switch (CONST_TAG(idx, pool)) {
            case CONSTANT_Integer:
            case CONSTANT_Float:
                *(jint*)mem = CLASS_CONST_INT(class, idx);
                FIELD_SIZE(fld) = 4; /* ??? */
                break;

            case CONSTANT_Long:
            case CONSTANT_Double:
                *(jlong*)mem = CLASS_CONST_LONG(class, idx);
                FIELD_SIZE(fld) = 8; /* ??? */
                break;

            case CONSTANT_String:
                pool->data[idx] = (ConstSlot)Utf8Const2JavaString(WORD2UTF(pool->data[idx]));
                pool->tags[idx] = CONSTANT_ResolvedString;
                /* ... fall through ... */
            case CONSTANT_ResolvedString:
                *(jref*)mem = (jref)CLASS_CONST_DATA(class, idx);
                FIELD_SIZE(fld) = PTR_TYPE_SIZE;
                break;
            }
        }
    }
}

#ifdef NEW_INTERFACETABLE
static void
_findmethod(Hjava_lang_Class *base, Hjava_lang_Class *class) {
    int i, j, k;

    for(i = 0; i < class->interface_len; i++) {
        for(j = 0; j < class->interfaces[i]->nmethods; j++) {
            Method *i_method = &(class->interfaces[i]->methods[j]);

            if (!equalUtf8Consts(i_method->name, init_name)) {
                Hjava_lang_Class *superclass;

                for(superclass = base; superclass != NULL; 
                     superclass = superclass->superclass) {

                    Method *mt = superclass->methods;

                    for(k = 0; k < superclass->nmethods; k++) {
                        if (equalUtf8Consts(i_method->name, mt[k].name) 
                             && equalUtf8Consts(i_method->signature, 
                                                 mt[k].signature)) {

                            mt[k].iidx = i_method->iidx;

                            *(((int *)base->dtable) - (mt[k].iidx)) = 
                                (int) base->dtable->method[mt[k].idx];
                            *(((int *)base->methodsInDtable) - (mt[k].iidx)) = 
                                (int) base->methodsInDtable[mt[k].idx];
                            goto foundimatch;
                        }
                    }
                }
                assert(0);      // no match found strange!!!
foundimatch:;
            }
        }
        if (class->interfaces[i]->interface_len != 0) {
            _findmethod(base, class->interfaces[i]);
        }
    }
}

static
void
buildInterfaceTable(Hjava_lang_Class *class)
{
// 	Method* meth;
// 	void** mtab;
// 	int i, j, k;
//     int midx, iidx;
    Hjava_lang_Class *superclass;
//     int method_offset = sizeof(Hjava_lang_Class *), name_offset, sig_offset;
//     interfaceTable *itable;


/* make interface table here */
//	class->itable = (interfaceTable *) gc_malloc(sizeof(interfaceTable) + class->isize * sizeof(void*), &gcDispatchTable);
//	class->itable = (interfaceTable *) gc_malloc(class->ITsize * sizeof(void*), &gcDispatchTable);

//	class->itable->class = class;

/* for now, interface table consumes more memory for speed
 * one traversal is reduced..
 * if we optimize this for memory, we need one more traversal
 * for finding name and signature of interface method
 */

/* filling interface table */
/* first if any superclass has implemented interface */
/* from superclass's interface table */
    for(superclass = class; superclass != NULL; 
         superclass = superclass->superclass) {
        _findmethod(class, superclass);
    }

    FLUSH_DCACHE(((int *)class->dtable - (class->ITsize)), class->dtable);
}

#else

static inline
void
_findmethod(Hjava_lang_Class *base, Hjava_lang_Class *class) {
    int i, j, k;

    for(i = 0; i < class->interface_len; i++) {
        for(j = 0; j < class->interfaces[i]->nmethods; j++) {
            Method *i_method = &(class->interfaces[i]->methods[j]);

            if (!equalUtf8Consts(i_method->name, init_name)) {
                base->itable->name[i_method->idx] = i_method->name;
                base->itable->signature[i_method->idx] = i_method->signature;
            }
        }
        if (class->interfaces[i]->interface_len != 0) {
            _findmethod(base, class->interfaces[i]);
        }
    }
}

static
void
buildInterfaceTable(Hjava_lang_Class* class)
{
    Method* meth;
    void** mtab;
    int i, j, k;
    int midx, iidx;
    Hjava_lang_Class *superclass;
    int method_offset, name_offset, sig_offset;
    
    method_offset = sizeof(Hjava_lang_Class *);

    /*
     if this class is interface assign new offset to methods (no dtable needed)
     if this class is normal class(implement or hierachy)
     get largest interface within
     superclass's interface size and interface's interface size
    */

    if ((class->accflags & ACC_INTERFACE) != 0) {
        // if interface extends another interface inherits the interface offset
        if (class->interface_len) {
            for(i = 0; i < class->interface_len; i++) {
                if (class->ITsize < class->interfaces[i]->ITsize) {
                    class->ITsize = class->interfaces[i]->ITsize;
                }
            }
            for(i = 0, meth = CLASS_METHODS(class); 
                i < CLASS_NMETHODS(class); meth++, i++) {
                int if_found = 0;

                for(j = 0; j < class->interface_len; j++) {
                    Method *meth_tmp;
                    for(meth_tmp = CLASS_METHODS(class->interfaces[j]), 
                            k = CLASS_NMETHODS(class->interfaces[j]); 
                        --k > 0; meth_tmp++) {
                        if (equalUtf8Consts(meth->name, meth_tmp->name) 
                             && equalUtf8Consts(meth->signature, 
                                                meth_tmp->signature)) {
                            meth->idx = meth_tmp->idx;
                            if_found = 1;
                        }
                    }
                }
                if (if_found == 0) {
                    class->ITsize = total_interface;
                    meth->idx = total_interface++;
                }
            }
        } else {
            // new interface is loaded.. now append interface index
            for(meth = CLASS_METHODS(class), i = 0; 
                i < CLASS_NMETHODS(class); meth++, i++) {
                class->ITsize = total_interface;
                meth->idx = total_interface++;
            }
        }
        return;
    } else {
        if (class->superclass) {
            class->ITsize = class->superclass->ITsize;
        }
        for(i = 0; i < class->interface_len; i++) {
            if (class->ITsize < class->interfaces[i]->ITsize) {
                class->ITsize = class->interfaces[i]->ITsize;
            }
        }
    }

    /* make interface table here */
    class->itable = (interfaceTable *) 
        gc_malloc(sizeof(interfaceTable) + 
                  class->ITsize * sizeof(void*), 
                  &gc_dispatch_table);
    class->itable->name = (Utf8Const **) 
        gc_malloc(sizeof(Utf8Const *) * 
                  class->ITsize, 
                  &gc_dispatch_table);
    class->itable->signature = (Utf8Const **) 
        gc_malloc(sizeof(Utf8Const *) * 
                  class->ITsize, 
                  &gc_dispatch_table);

    class->itable->class = class;
    mtab = class->itable->method;

    /* 
     for now, interface table consumes more memory for speed
     one traversal is reduced..
     if we optimize this for memory, we need one more traversal
     for finding name and signature of interface method
    */

    /* filling interface table */
    /* first fill name and signature */
    /* from superclass's interface table */
    if (class->superclass) {
        if (class->superclass->ITsize) {
            for(i = 0; i < class->superclass->ITsize; i++) {
                class->itable->name[i] = class->superclass->itable->name[i];
                class->itable->signature[i] = \
                    class->superclass->itable->signature[i];
            }
        }
    }

    /* from implemented interface */
    _findmethod(class, class);

    /* fill interface table's real value */
    /* from interface table go through each methods */
    for(i = 0; i < class->ITsize; i++) {
        if (class->itable->name[i] == NULL) continue;
        /* find matched method table entry and copy it to interface table */
        for(superclass = class; superclass != NULL; \
                superclass = superclass->superclass) {
            Method *mt = superclass->methods;
            for(j = 0; j < superclass->nmethods; j++) {
                if (equalUtf8Consts (class->itable->name[i], mt[j].name) &&
                    equalUtf8Consts (class->itable->signature[i], \
                                     mt[j].signature)) {
                    class->itable->method[i] = \
                        class->dtable->method[mt[j].idx];
                    goto foundimatch;
                }
            }
        }
foundimatch:;
    }

    FLUSH_DCACHE(class->itable, &class->itable[class->ITsize])
}
#endif // NEW_INTERFACETABLE

static
void
buildDispatchTable(Hjava_lang_Class* class)
{
    Method* meth;
    void** mtab;
    int i, j, k;
    int ntramps = 0;
#if defined(HAVE_TRAMPOLINE)
    methodTrampoline* tramp;
#endif

#ifdef NEW_INTERFACETABLE
    //
    // if this class is interface assign new offset to methods
    // (no dtable needed)
    // if this class is normal class(implement or hierachy)
    // get largest interface within
    // superclass's interface size and interface's interface size
    // class->ITsize is the number of this class' interface table
    // total_interface is the number of total interface methods
    //
    if ((class->accflags & ACC_INTERFACE) != 0) {
        // if interface extends another interface inherits the interface offset
        if (class->interface_len) {
            for(i = 0; i < class->interface_len; i++) {
                if (class->ITsize < class->interfaces[i]->ITsize) {
                    class->ITsize = class->interfaces[i]->ITsize;
                }
            }
            for(i = 0, meth = CLASS_METHODS(class); 
                i < CLASS_NMETHODS(class); meth++, i++) {
                int if_found = 0;
                for(j = 0; j < class->interface_len; j++) {
                    Method *meth_tmp;
                    for(meth_tmp = CLASS_METHODS(class->interfaces[j]), 
                            k = CLASS_NMETHODS(class->interfaces[j]); 
                        --k > 0; meth_tmp++) {
                        if (equalUtf8Consts(meth->name, meth_tmp->name) 
                             && equalUtf8Consts(meth->signature, 
                                                 meth_tmp->signature)) {
                            meth->iidx = meth_tmp->iidx;
                            if_found = 1;
                        }
                    }
                }
                if (if_found == 0) {
                    class->ITsize = total_interface;
                    meth->iidx = total_interface++;
                }
            }
        } else {
            // new interface is loaded.. now append interface index
            for(meth = CLASS_METHODS(class), i = 0; 
                i < CLASS_NMETHODS(class); meth++, i++) {
                class->ITsize = total_interface;
                meth->iidx = total_interface++;
            }
        }
        return;
    } else {
        if (class->superclass) {
            class->ITsize = class->superclass->ITsize;
        }
        for(i = 0; i < class->interface_len; i++) {
            if (class->ITsize < class->interfaces[i]->ITsize) {
                class->ITsize = class->interfaces[i]->ITsize;
            }
        }
    }
#endif NEW_INTERFACETABLE

    if (class->superclass != NULL) {
        /* This may not be the best place to do this but at least
           it is centralized.  */

        class->subclass_next = class->superclass->subclass_head;
        class->superclass->subclass_head = class;

        class->msize = class->superclass->msize;
    }
    else {
        class->msize = 0;
    }

    meth = CLASS_METHODS(class);
    i = CLASS_NMETHODS(class);
    for (; --i >= 0; meth++) {
        Hjava_lang_Class* super = class->superclass;
        ntramps++;

        if ((meth->accflags & ACC_STATIC)
            || equalUtf8Consts(meth->name, constructor_name)) {
            meth->idx = -1;
            continue;
        }
        /* Search superclasses for equivalent method name.
         * If found extract its index nr.
         */
        for (; super != NULL;  super = super->superclass) {
            int j = CLASS_NMETHODS(super);
            Method* mt = CLASS_METHODS(super);
            for (; --j >= 0;  ++mt) {
                if (equalUtf8Consts (mt->name, meth->name)
                    && equalUtf8Consts (mt->signature,
                                        meth->signature)) {
                    meth->idx = mt->idx;
#ifdef DYNAMIC_CHA
                    mt->isOverridden = true;
                    DCHA_fixup_speculative_call_site(mt);
#endif
                    goto foundmatch;
                }
            }
        }
        /* No match found so allocate a new index number */
        meth->idx = class->msize++;
      foundmatch:;
    }
    
#if defined(HAVE_TRAMPOLINE)
    /* Allocate the dispatch table and this class' trampolines all in
       one block of memory.  This is primarily done for GC reasons in
       that I didn't want to add another slot on class just for holding
       the trampolines, but it also works out for space reasons.  */

#ifdef NEW_INTERFACETABLE
    class->dtable = (dispatchTable *) 
        gc_malloc(class->ITsize * sizeof(void *) + sizeof(dispatchTable) 
                   + class->msize * sizeof(void *)
                   + ntramps * sizeof(methodTrampoline), &gc_dispatch_table);
    class->dtable = (dispatchTable *) ((int *)class->dtable + class->ITsize);

    class->methodsInDtable = 
        (Method**)gc_malloc_fixed((class->ITsize+class->msize)*sizeof(Method*));
    class->methodsInDtable = 
        (Method**)((int*)class->methodsInDtable+class->ITsize);

#else /* not NEW_INTERFACETABLE */
    class->dtable = (dispatchTable *) 
        gc_malloc(sizeof(dispatchTable) + class->msize * sizeof(void *) 
                   + ntramps * sizeof(methodTrampoline), &gc_dispatch_table);

#endif /* not NEW_INTERFACETABLE */

    tramp = (methodTrampoline*) &class->dtable->method[class->msize];

#else /* not HAVE_TRAMPOLINE */
    class->dtable = (dispatchTable *)
        gc_malloc(sizeof(dispatchTable) + (class->msize * sizeof(void*)), 
                   &gc_dispatch_table);
#endif /* not HAVE_TRAMPOLINE */

    class->dtable->class = class;
    mtab = class->dtable->method;

    if (Class_IsArrayType(class)){
        assert (class->refField == 0);
    }
    else if (class->refField == 0) {
        class->walk = gc_walk_null;
    }
    else {
        gc_create_walker(class);
    }

    // set inherited part of class->methodsInDtable
    if (class->superclass != NULL){
        Method** super_mtab = (Method**) class->superclass->methodsInDtable;
        Method** this_mtab = (Method**) class->methodsInDtable;
        for (i = 0; i < class->superclass->msize; i++) {
            assert(super_mtab[i]);
            this_mtab[i] = super_mtab[i];
        }
    }

    meth = CLASS_METHODS(class);
    i = CLASS_NMETHODS(class);

    /* An evil hack to avoid spaghetti if-defs.  Muhahahaha! */
#if defined(INTERPRETER) && !defined(TRANSLATOR)
#define dispatch_method_with_object sparc_do_fixup_trampoline
#endif

#if defined(HAVE_TRAMPOLINE)
    for (; --i >= 0; meth++) {
        Method** this_mtab = (Method**) class->methodsInDtable;
         /* Install inherited methods into dispatch class->methodsInDtable */
        if (meth->idx >= 0) {
            this_mtab[meth->idx] = meth;
        } else {
            // static method or classs initializer 
            //   - this is for kaffe translater
#ifdef INTERPRETER
            if (The_Use_Interpreter_Flag && intrp_interpretp(meth)) {
                void *caller = intrp_interpreter_caller(meth);
                FILL_IN_INTRP_TRAMPOLINE(tramp, meth, caller);
            } else {
                FILL_IN_JIT_TRAMPOLINE(tramp, meth,
                                       dispatch_method_with_object);
            }
#else /* not INTERPRETER */
            FILL_IN_JIT_TRAMPOLINE(tramp, meth, dispatch_method_with_object);
#endif /* not INTERPRETER */
            TRAMPOLINE_CODE_IN_DTABLE(meth) = (nativecode *)tramp;
            tramp++;
        }
    }

    // fill trampoline code in dispatch table
    for (i = 0; i < class->msize; i++){
        Method* meth = class->methodsInDtable[i];
        if (TRAMPOLINE_CODE_IN_DTABLE(meth)) {
            mtab[i] = (nativecode *)TRAMPOLINE_CODE_IN_DTABLE(meth);
        } else {
#ifdef INTERPRETER
            if (The_Use_Interpreter_Flag && intrp_interpretp(meth)) {
                void *caller = intrp_interpreter_caller(meth);
                FILL_IN_INTRP_TRAMPOLINE(tramp, meth, caller);
            } else {
                FILL_IN_JIT_TRAMPOLINE(tramp, meth,
                                       dispatch_method_with_object);
            }
#else /* not INTERPRETER */
            FILL_IN_JIT_TRAMPOLINE(tramp, meth, dispatch_method_with_object);
#endif /* not INTERPRETER */
            mtab[i] = (nativecode *)tramp;
            // TRAMPOLINE_CODE_IN_DTABLE(meth) == METHOD_NATIVECODE(meth)
            TRAMPOLINE_CODE_IN_DTABLE(meth) = (nativecode *)tramp;
            tramp++;

            // With customization, METHOD_NATIVECODE(meth) always points 
            // trampoline code. 
            // Exception: native method & class initializer, in these cases,
            // METHOD_NATIVECODE(meth) points loaded native code or translated
            // code
        }
    }
    FLUSH_DCACHE(class->dtable, tramp);

#else /* not HAVE_TRAMPOLINE */
    for (;  --i >= 0; meth++) {
        if (meth->idx >= 0) {
            mtab[meth->idx] = meth;
        }
    }
#endif /* not HAVE_TRAMPOLINE */


    /* Check for undefined abstract methods if class is not abstract.
     * See "Java Language Specification" (1996) section 12.3.2. */
    if ((class->accflags & ACC_ABSTRACT) == 0) {
        for (i = class->msize - 1; i >= 0; i--) {
            if (mtab[i] == NULL) {
                throwException(AbstractMethodError);
            }
        }
    }
}

#if defined(HAVE_TRAMPOLINE)

/*
 * Fixup a trampoline, replacing the trampoline with the real code.
 */
static void
_fixup_dispatch_table(Hjava_lang_Class* c,
                      Method* meth,
                      nativecode *ncode,
                      int idx)
{
    assert(c != NULL);

    if (c->methodsInDtable[idx] == meth) {
        c->dtable->method[idx] = ncode;
        for (c = c->subclass_head; c; c = c->subclass_next) {
            _fixup_dispatch_table(c, meth, ncode, idx);
        }
    }
}

static void
_fixupDispatchTable(Hjava_lang_Class *c, Method* meth,
                    nativecode *ncode, int idx) 
{
    Hjava_lang_Class*    temp;

    // find the class which really defines this method
    for (temp = c;
         temp->superclass != NULL 
             && temp->superclass->msize > idx 
             && temp->superclass->methodsInDtable[idx] == meth;
         temp = temp->superclass);

    _fixup_dispatch_table(temp, meth, ncode, idx);
}

static void
_fixup_interface_table(Hjava_lang_Class *c, Method* meth,
                       nativecode *ncode, int idx) 
{
    assert(c != NULL);

    if (*(((int*)c->methodsInDtable) - (idx+1)) == (int) meth) {
        *(((int *)c->dtable) - (idx + 1)) = (int) ncode;
        for (c = c->subclass_head; c; c = c->subclass_next) {
            _fixup_interface_table(c, meth, ncode, idx);
        }
    }
}

static void 
_fixupInterfaceTable(Hjava_lang_Class *c, Method* meth,
                     nativecode *ncode, int iidx) {
    Hjava_lang_Class *temp;

    /* if this class has no interface table return */
    if (c->ITsize == 0) return;

    /* find out interface table index */

    /* if this method is not interface method related to this class */
    if (iidx > c->ITsize) return;

    for(temp = c; 
        temp->superclass != NULL 
            && temp->superclass->ITsize >= iidx 
            &&*(((int *)temp->superclass->methodsInDtable)- iidx)==(int)meth;
        temp = temp->superclass);

    _fixup_interface_table(temp, meth, ncode, iidx);
}
    
/* Name        : fixupDtableItable
   Description : fixup dtable & itable according to given type & new code
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes: every class has a table which has same size array as dtable
          but the value in it is method pointer.
          itable & dtable fixing are using this table to fixup.
 */
void
fixupDtableItable(Method* meth, Hjava_lang_Class* class, void* ncode)
{
#ifdef CUSTOMIZATION
    if (SM_is_special_type(class) || meth->idx < 0) return;
    if (!SM_is_general_type(class)){
        if (class->methodsInDtable[meth->idx] == meth)
          class->dtable->method[meth->idx] = ncode;
        if (class->ITsize > 0 && meth->iidx != -1
             && *(((int*)class->methodsInDtable) - meth->iidx) == (int)meth)
          *(((int *)class->dtable) - meth->iidx) = (int)ncode;
    } else {
#endif
        _fixupDispatchTable(meth->class, meth, ncode, meth->idx);
        _fixupInterfaceTable(meth->class, meth, ncode, meth->iidx);
#ifdef CUSTOMIZATION
    }
#endif
}

// static
// void
// _fixup_dispatch_table(Hjava_lang_Class* c,
//                        nativecode *ocode,
//                        nativecode *ncode,
//                        int idx)
// {
//     assert(c != NULL);

//     if (c->dtable->method[idx] == ocode) {
//         c->dtable->method[idx] = ncode;
//         for (c = c->subclass_head; c; c = c->subclass_next) {
//             _fixup_dispatch_table(c, ocode, ncode, idx);
//         }
//     }
// }

// //static
// void
// fixupDispatchTable(Hjava_lang_Class *c, nativecode *ocode, 
//                     nativecode *ncode, int idx) {
//     Hjava_lang_Class*    temp;


//     //
//     // find the class which really defines this method
//     //
//     for (temp = c;
//           temp->superclass != NULL &&
//               temp->superclass->msize > idx &&
//               temp->superclass->dtable->method[ idx ] == ocode;
//           temp = temp->superclass) {
//         ;
//     }

//     _fixup_dispatch_table(temp, ocode, ncode, idx);

// }

// #ifdef NEW_INTERFACETABLE
// static void
// _fixup_interface_table(Hjava_lang_Class *c, nativecode *ocode,
//                         nativecode *ncode, int idx) {
//     assert(c != NULL);

//     if (*(((int *)c->dtable) - (idx + 1)) == (int) ocode) {
//         *(((int *)c->dtable) - (idx + 1)) = (int) ncode;
//         for (c = c->subclass_head; c; c = c->subclass_next) {
//             _fixup_interface_table(c, ocode, ncode, idx);
//         }
//     }
// }

// void 
// fixupInterfaceTable(Hjava_lang_Class *c, nativecode *ocode,
//                     nativecode *ncode, int iidx) {
//     Hjava_lang_Class *temp;

//     /* if this class has no interface table return */
//     if (c->ITsize == 0) return;

//     /* find out interface table index */

//     /* if this method is not interface method related to this class */
//     if (iidx > c->ITsize) return;

//     for(temp = c; temp->superclass != NULL && 
//             temp->superclass->ITsize >= iidx &&
//             *(((int *)temp->superclass->dtable) - iidx) == (int) ocode;
//         temp = temp->superclass);

//     _fixup_interface_table(temp, ocode, ncode, iidx);
// }
    
// #else /* not NEW_INTERFACETABLE */

// static void
// _fixup_interface_table(Hjava_lang_Class *c, nativecode *ocode,
//                         nativecode *ncode, int idx) {
//     assert(c != NULL);

// 	if (c->itable->method[idx] == ocode) {
// 		c->itable->method[idx] = ncode;
// 		for (c = c->subclass_head; c; c = c->subclass_next) {
// 			_fixup_interface_table(c, ocode, ncode, idx);
// 		}
// 	}
// }

// void 
// fixupInterfaceTable(Hjava_lang_Class *c, nativecode *ocode,
//                      nativecode *ncode, Method *m) {
//     int idx;
//     Hjava_lang_Class *temp;

//     /* if this class has no interface table return */
//     if (c->ITsize == 0) return;

//     /* find out interface table index */
//     for(idx = 0; idx < c->ITsize; idx++) {
//         if (equalUtf8Consts(c->itable->name[idx], m->name) &&
//             equalUtf8Consts(c->itable->signature[idx], m->signature))
//           break;
//     }

//     /* if this method is not interface method related to this class */
//     if (idx == c->ITsize) return;

//     for(temp = c; temp->superclass != NULL && 
//             temp->superclass->ITsize > idx &&
//             temp->superclass->itable->method[ idx ] == ocode;
//         temp = temp->superclass);

//     _fixup_interface_table(temp, ocode, ncode, idx);
// }
    
// #endif /* not NEW_INTERFACETABLE */

/*
 * Trampolines come in here - do the translation and replace the trampoline.
 */
nativecode *
fixupTrampoline(Method* meth)
{
#ifdef CUSTOMIZATION
    assert(0 && "non-reachable");
    return NULL;

#else /* not CUSTOMIZATION */
    nativecode *ocode;

    /* If this class needs initializing, do it now.  */
    if (meth->class->state != CSTATE_OK) {
        processClass(meth->class, CSTATE_OK);
    }

    /* Generate code on demand.  */
    if (!METHOD_TRANSLATED(meth)) {
#ifdef TRANSLATOR
        TranslationInfo *info
            = (TranslationInfo *) alloca(sizeof(TranslationInfo));
        ocode = METHOD_NATIVECODE(meth);

        bzero(info, sizeof(TranslationInfo));
        TI_SetRootMethod(info, meth);

        translate(info);

        METHOD_NATIVECODE(meth) = TI_GetNativeCode(info);
#else /* not TRANSLATOR */
	assert(Method_IsNative(meth));

	ocode = METHOD_NATIVECODE(meth);
	native(meth);
#endif /* not TRANSLATOR */

        /* Update the dtable entries for all classes if this isn't a
           static method.  */
        fixupDtableItable(meth, NULL, METHOD_NATIVECODE(meth));
    }

    return (METHOD_NATIVECODE(meth));

#endif /* not CUSTOMIZATION */

}

#endif /* not (TRANSLATOR && HAVE_TRAMPOLINE) */

/*
 * Initialise the constants.
 * First we make sure all the constant strings are converted to java strings.
 */
static
void
resolveConstants(Hjava_lang_Class* class)
{
    int idx;
    constants* pool;
    
    assert(class->state == CSTATE_LINKED);
    class->state = CSTATE_DOING_CONSTINIT;

    /* Scan constant pool and convert any constant strings into true
     * java strings.
     */
    pool = CLASS_CONSTANTS (class);
    for (idx = 0; idx < pool->size; idx++) {
        if (pool->tags[idx] == CONSTANT_String) {
            pool->data[idx] = (ConstSlot)Utf8Const2JavaString(WORD2UTF(pool->data[idx]));
            pool->tags[idx] = CONSTANT_ResolvedString;
        }

	/* The translator is not reentrant, so problems might arise if
           classes aren't resolved, especially when custom class
           loaders are used.  FIXME: This should not be resolved at
           class loading time! */
#ifdef TRANSLATOR
        else if (pool->tags[idx] == CONSTANT_Class) {
            getClass(idx, class);
        }
#endif /* TRANSLATOR */
    }

    class->state = CSTATE_CONSTINIT;
}

static
Hjava_lang_Class*
simpleLookupClass(Utf8Const* name, Hjava_lang_ClassLoader* loader)
{
    Hjava_lang_Class* clp;

    for (clp = classPool[hashClassName(name) & (CLASSHASHSZ-1)]; clp != 0 && !(equalUtf8Consts(name, clp->name) && loader == clp->loader); clp = clp->next)
		;
    return (clp);
}

/*
 * Lookup a named field.
 */
Field*
lookupClassField(Hjava_lang_Class* clp, Utf8Const* name, bool isStatic)
{
    Field* fptr;
    int n;
    
    /* Search down class for field name */
    if (isStatic) {
        fptr = CLASS_SFIELDS(clp);
        n = CLASS_NSFIELDS(clp);
    }
    else {
        fptr = CLASS_IFIELDS(clp);
        n = CLASS_NIFIELDS(clp);
    }
    while (--n >= 0) {
        if (equalUtf8Consts (name, fptr->name)) {
            if (!FIELD_RESOLVED(fptr)) {
                char* sig;
                sig = ((Utf8Const*)FIELD_TYPE(fptr))->data;
                FIELD_TYPE(fptr) = classFromSig(&sig, clp->loader);
                fptr->accflags &= ~FIELD_UNRESOLVED_FLAG;
            }
            return (fptr);
        }
        fptr++;
    }
FDBG(	printf("Class:field lookup failed %s:%s\n", c, f);		)
    return (0);
}

/*
 * Determine the number of arguments and return values from the
 * method signature.
 */
void
countInsAndOuts(char* str, short* ins, short* outs, char* outtype)
{
    *ins = sizeofSig(&str, false);
    *outtype = str[0];
    *outs = sizeofSig(&str, false);
}

/*
 * Calculate size of data item based on signature.
 */
int
sizeofSig(char** strp, bool want_wide_refs)
{
    int count;
    int c;

    count = 0;
    for (;;) {
        c = sizeofSigItem(strp, want_wide_refs);
        if (c == -1) {
            return (count);
        }
        count += c;
    }
}

/*
 * Calculate size (in words) of a signature item.
 */
int
sizeofSigItem(char** strp, bool want_wide_refs)
{
    int count;
    char* str;

    for (str = *strp; ; str++) {
        switch (*str) {
        case '(':
            continue;
        case 0:
        case ')':
            count = -1;
            break;
        case 'V':
            count = 0;
            break;
        case 'I':
        case 'Z':
        case 'S':
        case 'B':
        case 'C':
        case 'F':
            count = 1;
            break;
        case 'D':
        case 'J':
            count = 2;
            break;
        case '[':
            count = want_wide_refs ? sizeof(void*) / sizeof(int32) : 1;
        arrayofarray:
            str++;
            if (*str == 'L') {
                while (*str != ';') {
                    str++;
                }
            }
            else if (*str == '[') {
                goto arrayofarray;
            }
            break;
        case 'L':
            count = want_wide_refs ? sizeof(void*) / sizeof(int32) : 1;
            /* Skip to end of reference */
            while (*str != ';') {
                str++;
            }
            break;
        default:
            ABORT();
        }
        
        *strp = str + 1;
        return (count);
    }
}

/*
 * Find (or create) an array class with component type C.
 */
Hjava_lang_Class*
lookupArray(Hjava_lang_Class* c)
{
    Utf8Const *array_name;
    char sig[CLASSMAXSIG];  /* FIXME! unchecked fixed buffer! */
    Hjava_lang_Class* array_class;
    Hjava_lang_Class* temp_class;
    int array_dim = 1;

    array_class = Class_GetArrayType(c);
    if (array_class) {
        return (array_class);
    }
    
    if (Class_IsPrimitiveType(c)) {
        sprintf (sig, "[%c", CLASS_PRIM_SIG(c));
    }
    else {
        char* cname = CLASS_CNAME (c);
        sprintf (sig, cname[0] == '[' ? "[%s" : "[L%s;", cname);
    }

    array_name = makeUtf8Const (sig, -1);
    array_class = simpleLookupClass(array_name, c->loader);

    if (array_class == NULL) {
        array_class = (Hjava_lang_Class*)newObject(ClassClass);
        array_class = internalAddClass(array_class, array_name, 0, 0, c->loader);
        array_class->superclass = ObjectClass;
        buildDispatchTable(array_class);

        //Now, set walk function
        if (Class_IsPrimitiveType(c)) 
            array_class->walk = gc_walk_null;
        else 
            array_class->walk = gc_walk_ref_array;

        Class_SetComponentType(array_class, c);
        Class_SetArrayType(c, array_class);

        array_class->state = CSTATE_OK;

        temp_class = c;
        while (Class_IsArrayType(temp_class)){
            temp_class = Class_GetComponentType(temp_class);
            array_dim++;
        }

        Class_SetElementType(array_class, temp_class);
        Class_SetDimension(array_class, array_dim);
        assert(temp_class != 0);
        
        CIT_update_class_hierarchy(array_class);
    }

    return (array_class);
}

#ifdef TRANSLATOR
/*
 * Find method containing pc.
 */
MethodInstance *
findMethodFromPC(uintp pc)
{
    return (MethodInstance *) CN_find_data((unsigned int) pc);
}

void
registerMethod(uintp start_pc, uintp end_pc, void *mi)
{
    CN_register_data(start_pc, end_pc, mi, MethodBody);
}

void
registerTypeCheckCode(uintp start_pc, uintp end_pc)
{
    CN_register_data(start_pc, end_pc, NULL, TypeCheckCode);
}

#endif /* TRANSLATOR */


/* Name        : get_method_from_class_and_index
   Description : Get method pointer using dtable index
   Maintainer  : Junpyo Lee <walker@altair.snu.ac.kr>
   Pre-condition:
   
   Post-condition:
   
   Notes:  */
Method* 
get_method_from_class_and_index(Hjava_lang_Class* class, int idx) 
{
    assert((idx >= 0 && Class_GetNumOfMethodInDispatchTable(class) > idx)
            || (idx < 0 && Class_GetNumOfMethodInInterfaceTable(class) >= (-idx)));

    if (idx >= 0) 
      return class->methodsInDtable[idx];
    else 
      return *(Method**)((int*)class->methodsInDtable + idx);


}

/*
  Manipulation methods for Class, Method, and Fields
*/
jexceptionEntry*
Method_GetExceptionHandler(Method* method,
			    uint16  pc,
			    Hjava_lang_Class* exception)
{
    jexception*       exception_table;
    jexceptionEntry*  exception_entry;
    int                  i;

    exception_table = Method_GetExceptionTable(method);

    if (exception_table == NULL) {
        // no exception handler
        return NULL;
    }

    // now look up the exception table
    exception_entry = exception_table->entry;
    for (i = 0; i < exception_table->length; i++) {
        InstrOffset   start_pc = exception_entry[i].start_pc;
        InstrOffset   end_pc = exception_entry[i].end_pc;

        if (pc >= start_pc && pc < end_pc) {
            // If catch_type index is 0, this is called for all exceptions.
            // This is used to implement finally clause.

            if (exception_entry[i].catch_idx == 0) {
                return &exception_entry[i];
            }

            // resolve the exception class if necessary
            if (exception_entry[i].catch_type == NULL) {
                exception_entry[i].catch_type = 
                    getClass(exception_entry[i].catch_idx,
			     Method_GetDefiningClass(method));
            }

            if (CIT_is_subclass(exception, exception_entry[i].catch_type)) {
                return &exception_entry[i];
            }
        }
    }

    return NULL;
}

ReturnType
Method_GetRetType(Method* method)
{
    switch(method->rettype) {
      case 'V':
        return RT_NO_RETURN;
      case 'I':
      case 'Z':
      case 'S':
      case 'B':
      case 'C':
        return RT_INTEGER;

      case 'J':
        return RT_LONG;

      case 'F':
        return RT_FLOAT;

      case 'D':
        return RT_DOUBLE;

      case 'L':
      case '[':
        return RT_REFERENCE;

      default:
        assert(false);
        exit(1);
    }
}
