/*
 * java.net.SocketInputStream.c
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
#include "config-io.h"
#include "config-mem.h"
#include <native.h>
#include "../core/java.io.stubs/FileDescriptor.h"
#include "../core/java.io.stubs/FileInputStream.h"
#include "java.net.stubs/SocketImpl.h"
#include "java.net.stubs/SocketInputStream.h"
#include "nets.h"
#include "kthread.h"
#include "runtime/support.h"

jint
java_net_SocketInputStream_socketRead(struct Hjava_net_SocketInputStream* this, HArrayOfByte* buf, jint offset, jint len)
{
	int r;

	r = read(unhand(unhand(unhand(this)->impl)->fd)->fd, &unhand(buf)->body[offset], len);
	if (r < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}
	else if (r == 0) {
		return (-1);	/* EOF */
	}
	else {
		return (r);
	}
}
