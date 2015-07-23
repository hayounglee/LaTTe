/*
 * baseClasses.c
 * Handle base classes.
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
#include "config-signal.h"
#include "gtypes.h"
#include "access.h"
#include "object.h"
#include "constants.h"
#include "classMethod.h"
#include "baseClasses.h"
#include "thread.h"
#include "lookup.h"
#include "exception.h"
#include "errors.h"
#include "itypes.h"
#include "md.h"

#include "class_inclusion_test.h"
#include "exception_handler.h"

#ifdef TRANSLATOR
#include "cell_node.h"
#endif

#if defined(TRANSLATOR) && defined(USE_CODE_ALLOCATOR)
#include "mem_allocator_for_code.h"
#endif

Utf8Const* init_name;
Utf8Const* void_signature;
Utf8Const* final_name;
Utf8Const* constructor_name;
Utf8Const* Code_name;
Utf8Const* LineNumberTable_name;
Utf8Const* ConstantValue_name;

Utf8Const* null_pointer_exception_name;
Utf8Const* array_index_out_of_bounds_exception_name;
Utf8Const* negative_array_size_exception_name;
Utf8Const* array_store_exception_name;
Utf8Const* arithmetic_exception_name;
Utf8Const* object_class_name;
Utf8Const* throwable_class_name;
Utf8Const* exception_class_name;
Utf8Const* runtime_exception_class_name;
Utf8Const* index_out_of_bounds_exception_name;


Hjava_lang_Class* ClassClass;
Hjava_lang_Class* StringClass;
Hjava_lang_Class* ObjectClass;
Hjava_lang_Class* SystemClass;
Hjava_lang_Class* SerialClass;
Hjava_lang_Class* ErrorClass;

Hjava_lang_Object *OutOfMemoryError;

#define SYSTEMCLASS "java/lang/System"
#define	SERIALCLASS "java/io/Serializable"

#define	INIT			"<clinit>"
#define	VOIDSIG			"()V"
#define	FINAL			"finalize"
#define	CONSTRUCTOR_NAME	"<init>"

/* Initialisation prototypes */
void initClasspath(void);
void initExceptions(void);
void initNative(void);
void initThreads(void);
void initTypes(void);

/*
 * Initialise the machine.
 */
void
initialiseLaTTe(void)
{
#ifdef TRANSLATOR
	/* Initialize method lookup table. */
	CN_initialize();
#endif /* TRANSLATOR */

	/* Initialize memory allocator */
	gc_init();

	/* Machine specific initialisation first */
#if defined(INIT_MD)
	INIT_MD();
#endif

#if defined(TRANSLATOR) && defined(USE_CODE_ALLOCATOR)
	CMA_initialize_mem_allocator();
#endif

	/* Setup CLASSPATH */
	initClasspath();

	/* Init native support */
	initNative();

	/* Create the initialise and finalize names and signatures. */
	init_name = makeUtf8Const(INIT, -1);
	void_signature = makeUtf8Const(VOIDSIG, -1);
	final_name = makeUtf8Const(FINAL, -1);
	constructor_name = makeUtf8Const(CONSTRUCTOR_NAME, -1);
	Code_name = makeUtf8Const("Code", -1);
	LineNumberTable_name = makeUtf8Const("LineNumberTable", -1);
	ConstantValue_name = makeUtf8Const("ConstantValue", -1);

	null_pointer_exception_name =
		makeUtf8Const( "java/lang/NullPointerException", -1 );
	array_index_out_of_bounds_exception_name =
		makeUtf8Const( "java/lang/ArrayIndexOutOfBoundsException", -1 );
	negative_array_size_exception_name =
		makeUtf8Const( "java/lang/NegativeArraySizeException", -1 );
	array_store_exception_name =
		makeUtf8Const( "java/lang/ArrayStoreException", -1 );
	arithmetic_exception_name =
		makeUtf8Const( "java/lang/ArithmeticException", -1 );
	object_class_name =
		makeUtf8Const( "java/lang/ObjectException", -1 );
	throwable_class_name =
		makeUtf8Const( "java/lang/Throwable", -1 );
	exception_class_name =
		makeUtf8Const( "java/lang/Exception", -1 );
	runtime_exception_class_name =
		makeUtf8Const( "java/lang/RuntimeException", -1 );
	index_out_of_bounds_exception_name =
		makeUtf8Const( "java/lang/IndexOutOfBounds", -1 );

	/* Read in base classes */
	initBaseClasses();

	/* Setup exceptions */
	initExceptions();

	/* Init thread support */
	initThreads();

	/* Allocate this exception while we can. */
	OutOfMemoryError = NEW_LANG_EXCEPTION(OutOfMemoryError);

	/* Initialise the System */
	do_execute_java_class_method("java.lang.System", "initializeSystemClass", "()V");
}

/*
 * We need to use certain classes in the internal machine so we better
 * get them in now in a known way so we can refer back to them.
 * Currently we need java/lang/Object, java/lang/Class, java/lang/String
 * and java/lang/System.
 */
void
initBaseClasses(void)
{
	/* Primitive types */
	initTypes();

#define ALLOC_CLASS()	\
	gc_malloc(sizeof(Hjava_lang_Class), &gc_class_object)

	/* Allocate memory for base classes. */
	ObjectClass	= ALLOC_CLASS();
	SerialClass	= ALLOC_CLASS();
	ClassClass	= ALLOC_CLASS();
	StringClass	= ALLOC_CLASS();
	SystemClass	= ALLOC_CLASS();

#undef ALLOC_CLASS

	/* The base types */
	loadStaticClass(ObjectClass, OBJECTCLASS);
	loadStaticClass(SerialClass, SERIALCLASS);
	loadStaticClass(ClassClass, CLASSCLASS);
	loadStaticClass(StringClass, STRINGCLASS);
	loadStaticClass(SystemClass, SYSTEMCLASS);

	/* We must to a little cross tidying */
	ObjectClass->head.dtable = ClassClass->dtable;
	SerialClass->head.dtable = ClassClass->dtable;
	ClassClass->head.dtable = ClassClass->dtable;
#ifndef NEW_INTERFACETABLE
	ObjectClass->head.itable = ClassClass->itable;
	SerialClass->head.itable = ClassClass->itable;
	ClassClass->head.itable = ClassClass->itable;
#endif // NEW_INTERFACETABLE

	/* Fixup primitive types */
	finishTypes();
}

/*
 * Setup the internal exceptions.
 */

void
initExceptions( void )
{
	catchSignal( SIGSEGV, EH_null_exception );
	catchSignal( SIGBUS, EH_null_exception );
	catchSignal( SIGFPE, EH_arithmetic_exception );
	catchSignal( SIGPIPE, SIG_IGN );
	catchSignal( SIGILL, EH_trap_handler );

	ErrorClass = gc_malloc(sizeof(Hjava_lang_Class), &gc_class_object);
	loadStaticClass(ErrorClass, "java/lang/Error");
}

