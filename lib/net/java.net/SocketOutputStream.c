/*
 * java.net.SocketOutputStream.c
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

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "config-io.h"
#include <native.h>
#include "../core/java.io.stubs/FileDescriptor.h"
#include "../core/java.io.stubs/FileOutputStream.h"
#include "java.net.stubs/SocketImpl.h"
#include "java.net.stubs/SocketOutputStream.h"
#include "nets.h"
#include "kthread.h"

void
java_net_SocketOutputStream_socketWrite(struct Hjava_net_SocketOutputStream* this, HArrayOfByte* buf, jint offset, jint len)
{
	int r;

	if (unhand(unhand(unhand(this)->impl)->fd)->fd < 0) {
		/* SignalError(NULL, "java.io.IOException", SYS_ERROR); */
		return;
	}
	r = write(unhand(unhand(unhand(this)->impl)->fd)->fd, &unhand(buf)->body[offset], len);
	if (r < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}
}
