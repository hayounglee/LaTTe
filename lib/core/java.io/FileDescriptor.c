/*
 * java.io.FileDescriptor.c
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "lib-license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 */

#include "config.h"
#include "config-std.h"
#include "config-io.h"
#include "java.io.stubs/FileDescriptor.h"
#include "kthread.h"
#include "runtime/support.h"

/*
 * Initialise a file descriptor to the given file nr.
 */
struct Hjava_io_FileDescriptor*
java_io_FileDescriptor_initSystemFD(struct Hjava_io_FileDescriptor* this, jint i)
{
	unhand(this)->fd = fixfd(i);
	return (this);
}

/*
 * Is this file descriptor valid ?
 */
jbool
java_io_FileDescriptor_valid(struct Hjava_io_FileDescriptor* this)
{
	if (unhand(this)->fd >= 0) {
		return (1);
	}
	else {
		return (0);
	}
}

/*
 * XXX UNIMPLEMENTED
 */
void
java_io_FileDescriptor_sync(struct Hjava_io_FileDescriptor* this)
{
	unimp("java.io.File:sync unimplemented");
}
