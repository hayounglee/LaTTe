/*
 * java.lang.Runtime.c
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "lib-license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 *
 * Modified by MASS Laboratory, SNU, 1999
 */

#define	DBG(s)
#define CDBG(s)

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "config-io.h"
#include "gtypes.h"
#include <native.h>
#include "files.h"
#include "defs.h"
#include "java.lang.stubs/Runtime.h"
#include "runtime/external.h"
#include "runtime/gc.h"
#include "runtime/locks.h"
#include "runtime/support.h"

//
// inserted by doner
//
#define         LIBRARY_PREFIX  "/lib" VMNAME "_"

extern char* libraryPath;
extern size_t gc_heap_limit;

/*
 * Initialise the linker and return the search path for shared libraries.
 */
struct Hjava_lang_String*
java_lang_Runtime_initializeLinkerInternal(struct Hjava_lang_Runtime* this)
{
	struct Hjava_lang_String*   ret_val;

	//
	// inserted by doner
	// - This function must be synchronized.
	//
	_lockMutex( (Hjava_lang_Object*)this );

	ret_val = makeJavaString(libraryPath, strlen(libraryPath));

	_unlockMutex( (Hjava_lang_Object*)this );

	return ret_val;
}

/*
 * Construct a library name.
 */
struct Hjava_lang_String*
java_lang_Runtime_buildLibName(struct Hjava_lang_Runtime* this, struct Hjava_lang_String* s1, struct Hjava_lang_String* s2)
{
	char lib[MAXLIBPATH];
	char str[MAXPATHLEN];

	/*
	 * Note. Although the code below will build a library string, if
	 * it doesn't fit into the buffer, it will truncate the path
	 * silently.
	 */
	javaString2CString(s1, str, sizeof(str));
	strncpy(lib, str, MAXLIBPATH-1);
	strncat(lib, LIBRARY_PREFIX, MAXLIBPATH-1);
	javaString2CString(s2, str, sizeof(str));
	strncat(lib, str, MAXLIBPATH-1);
	strncat(lib, LIBRARYSUFFIX, MAXLIBPATH-1);
	lib[MAXLIBPATH-1] = 0;

	return (makeJavaString(lib, strlen(lib)));
}

/*
 * Load in a library file.
 */
jint
java_lang_Runtime_loadFileInternal(struct Hjava_lang_Runtime* this, struct Hjava_lang_String* s1)
{
	char lib[MAXPATHLEN];
	int r;

	javaString2CString(s1, lib, sizeof(lib));

	r = loadNativeLibrary(lib);

	return (r == 0 ? 1 : 0);
}

/*
 * Exit - is this just a thread or the whole thing?
 */
void
java_lang_Runtime_exitInternal(struct Hjava_lang_Runtime* r, jint v)
{
	EXIT(v);
}

/*
 * Exec another program.
 */
struct Hjava_lang_Process*
java_lang_Runtime_execInternal(struct Hjava_lang_Runtime* this, HArrayOfObject* argv, HArrayOfObject* arge)
{
	struct Hjava_lang_Process* child;

	child = (struct Hjava_lang_Process*)execute_java_constructor(0, "java.lang.UNIXProcess", 0, "([Ljava/lang/String;[Ljava/lang/String;)V", argv, arge);

	return (child);
}

/*
 * Free memory.
 */
jlong
java_lang_Runtime_freeMemory(struct Hjava_lang_Runtime* this)
{
	/* This is a particularly inaccurate guess - it's basically how
	 * much more memory we can claim from the heap, and ignores any
	 * free memory already within the GC system.
	 * Well it'll do for now.
	 */
	return (gc_heap_limit - (gc_stats.small_size + gc_stats.large_size));
}

/*
 * Total memory.
 */
jlong
java_lang_Runtime_totalMemory(struct Hjava_lang_Runtime* this)
{
	return (gc_heap_limit);
}

/*
 * Run the garbage collector.
 */
void
java_lang_Runtime_gc(struct Hjava_lang_Runtime* this)
{
	gc_invoke(0);
}

/*
 * Run any pending finialized methods.
 *  Finalising is part of the garbage collection system - so just run that.
 */
void
java_lang_Runtime_runFinalization(struct Hjava_lang_Runtime* this)
{
	gc_finalize_invoke();
}

/*
 * Enable/disable tracing of instructions.
 */
void
java_lang_Runtime_traceInstructions(struct Hjava_lang_Runtime* this, jbool on)
{
	unimp("java.lang.Runtime:traceInstructions unimplemented");
}

/*
 * Enable/disable tracing of method calls.
 */
void
java_lang_Runtime_traceMethodCalls(struct Hjava_lang_Runtime* this, jbool on)
{
	unimp("java.lang.Runtime:traceMethodCalls unimplemented");
}

void
java_lang_Runtime_runFinalizersOnExit0(jbool on)
{
	gc_finalize_on_exit = (on == true);
}
