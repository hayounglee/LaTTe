/*
 * external.c
 * Handle method calls to other languages.
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
#define	LDBG(s)

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "gtypes.h"
#include "access.h"
#include "object.h"
#include "constants.h"
#include "classMethod.h"
#include "external.h"
#include "errors.h"
#include "exception.h"
#include "slib.h"
#include "paths.h"
#include "support.h"

#if defined(NO_SHARED_LIBRARIES)
#include "../lib/external_native.h"
#endif

#if defined(NO_SHARED_LIBRARIES)
#define	STUB_PREFIX		""
#define	STUB_PREFIX_LEN		0
#define	STUB_POSTFIX		""

/*
 * Some version of dlsym need an underscore, some don't.
 */
#else /* not defined(NO_SHARED_LIBRARIES) */

#if defined(HAVE_DYN_UNDERSCORE)
#define	STUB_PREFIX		"_"
#define	STUB_PREFIX_LEN		1
#else
#define	STUB_PREFIX		""
#define	STUB_PREFIX_LEN		0
#endif
#define	STUB_POSTFIX		""

static struct {
	LIBRARYHANDLE	desc;
	char*		name;
	int		ref;
} libHandle[MAXLIBS];
#endif /* not defined(NO_SHARED_LIBRARIES) */

char* libraryPath = "";

void *loadNativeLibrarySym(char*);

/*
 * Error stub function.  Point unresolved link errors here to avoid
 * problems.
 */
static
void*
error_stub(void)
{
	return (0);
}

void
initNative(void)
{
#if !defined(NO_SHARED_LIBRARIES)
	char lib[MAXLIBPATH];
	char* ptr;
	char* nptr;

	ptr = getenv(LIBRARYPATH);
	if (ptr == 0) {
#if defined(DEFAULT_LIBRARYPATH)
		ptr = DEFAULT_LIBRARYPATH;
#else
		fprintf(stderr, "No library path set.\n");
		return;
#endif
	}
	libraryPath = gc_malloc_fixed(strlen(ptr) + 1);
	strcpy(libraryPath, ptr);

	LIBRARYINIT();

	/* Find the default library */
	for (ptr = libraryPath; ptr != 0; ptr = nptr) {
		nptr = strchr(ptr, PATHSEP);
		if (nptr == 0) {
			strcpy(lib, ptr);
		}
		else if (nptr == ptr) {
			nptr++;
			continue;
		}
		else {
			strncpy(lib, ptr, nptr - ptr);
			lib[nptr-ptr] = 0;
			nptr++;
		}
		strcat(lib, DIRSEP);
		strcat(lib, NATIVELIBRARY);
		strcat(lib, LIBRARYSUFFIX);

		if (loadNativeLibrary(lib) != -1) {
			return;
		}
	}
	fprintf(stderr, "Failed to locate native library in path:\n");
	fprintf(stderr, "\t%s\n", libraryPath);
	fprintf(stderr, "Aborting.\n");
	fflush(stderr);
	EXIT(1);
#else
	int i;

	/* Initialise the native function table */
	for (i = 0; default_natives[i].name != 0; i++) {
		addNativeMethod(default_natives[i].name, default_natives[i].func);
	}
#endif
}

int
loadNativeLibrary(char* lib)
{
#ifndef NO_SHARED_LIBRARIES
	int i;

	/* Find a library handle.  If we find the library has already
	 * been loaded, don't bother to get it again, just increase the
	 * reference count.
	 */
	for (i = 0; i < MAXLIBS; i++) {
		if (libHandle[i].desc == 0) {
			goto open;
		}
		if (strcmp(libHandle[i].name, lib) == 0) {
			libHandle[i].ref++;
			return (0);
		}
	}
	return (-1);

	/* Open the library */
	open:
        LIBRARYLOAD(libHandle[i].desc, lib);

	if (libHandle[i].desc == 0) {
LDBG(		printf("Library load failed: %s\n", LIBRARYERROR());	)
		return (-1);
	}

	libHandle[i].ref = 1;
	libHandle[i].name = gc_malloc_fixed(strlen(lib) + 1);
	strcpy(libHandle[i].name, lib);

#endif /* not NO_SHARED_LIBRARIES */

	return (0);
}

/*
 * Get pointer to symbol from symbol name.
 */
void*
loadNativeLibrarySym(char* name)
{
	void* func;

	func = NULL;
	LIBRARYFUNCTION(func, name);

	return (func);
}

void
native(Method* m)
{
	char stub[MAXSTUBLEN];
	char* ptr;
	int i;
	void* func;

	/* Construct the stub name */
	strcpy(stub, STUB_PREFIX);
	ptr = m->class->name->data;
	for (i = STUB_PREFIX_LEN; *ptr != 0; ptr++, i++) {
		if (*ptr == '/') {
			stub[i] = '_';
		}
		else {
			stub[i] = *ptr;
		}
	}
	stub[i] = '_';
	stub[i+1] = 0;
	strcat(stub, m->name->data);
	strcat(stub, STUB_POSTFIX);

DBG(	printf("Method = %s.%s%s\n", m->class->name->data, m->name->data, m->signature->data);)
DBG(	printf("Native stub = '%s'\n", stub);fflush(stdout);		)

	/* Find the native method */
	func = loadNativeLibrarySym(stub);
	if (func != 0) {
		/* Fill it in */
		METHOD_NATIVECODE(m) = func;
		m->accflags |= ACC_TRANSLATED;
		return;
	}

	fprintf(stderr, "Failed to locate native function:\n\t%s.%s%s\n",
		m->class->name->data, m->name->data, m->signature->data);
	fflush(stderr);
	METHOD_NATIVECODE(m) = (void*)error_stub;
	m->accflags |= ACC_TRANSLATED;

        throwException(UnsatisfiedLinkError);
}

/*
 * Return the library path.
 */
char*
getLibraryPath(void)
{
	return (libraryPath);
}
