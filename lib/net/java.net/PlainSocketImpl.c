/*
 * java.net.PlainSocketImpl.c
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
#include <netinet/in.h>
#include <native.h>
#include "../core/java.io.stubs/FileDescriptor.h"
#include "../core/java.lang.stubs/Integer.h"
#include "java.net.stubs/SocketImpl.h"
#include "java.net.stubs/InetAddress.h"
#include "java.net.stubs/PlainSocketImpl.h"
#include "java.net.stubs/SocketOptions.h"
#include "nets.h"
#include "kthread.h"
#include "runtime/locks.h"

/*
 * Create a stream or datagram socket.
 */
void
java_net_PlainSocketImpl_socketCreate(struct Hjava_net_PlainSocketImpl* this, jbool stream)
{
	int fd;
	int type;

	if (stream == 0) {
		type = SOCK_DGRAM;
	}
	else {
		type = SOCK_STREAM;
	}

	fd = socket(AF_INET, type, 0);
	if (fd < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}
	unhand(unhand(this)->fd)->fd = fd;
}

/*
 * Connect the socket to someone.
 */
void
java_net_PlainSocketImpl_socketConnect(struct Hjava_net_PlainSocketImpl* this, struct Hjava_net_InetAddress* daddr, jint dport)
{
	int fd;
	int r;
	struct sockaddr_in addr;
	size_t alen;

#if defined(BSD44)
	addr.sin_len = sizeof(addr);
#endif
	addr.sin_family = AF_INET;
	addr.sin_port = htons(dport);
	addr.sin_addr.s_addr = htonl(unhand(daddr)->address);

	fd = unhand(unhand(this)->fd)->fd;
	r = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
	if (r < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}

	/* Enter information into socket object */
	alen = sizeof(addr);
	r = getsockname(fd, (struct sockaddr*)&addr, &alen);
	if (r < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}

	unhand(this)->address = daddr;
	unhand(this)->port = dport;
	unhand(this)->localport = ntohs(addr.sin_port);
}

/*
 * Bind this socket to an address.
 */
void
java_net_PlainSocketImpl_socketBind(struct Hjava_net_PlainSocketImpl* this, struct Hjava_net_InetAddress* laddr, jint lport)
{
	int r;
	struct sockaddr_in addr;
	int fd;
	int on = 1;
	size_t alen;

#if defined(BSD44)
	addr.sin_len = sizeof(addr);
#endif
	addr.sin_family = AF_INET;
	addr.sin_port = htons(lport);
	addr.sin_addr.s_addr = unhand(laddr)->address;

	fd = unhand(unhand(this)->fd)->fd;

	/* Allow rebinding to socket - ignore errors */
	(void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *) &on, sizeof(on));
	r = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
	if (r < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}

	/* Enter information into socket object */
	unhand(this)->address = laddr;
	if (lport == 0) {
		alen = sizeof(addr);
		r = getsockname(fd, (struct sockaddr*)&addr, &alen);
		if (r < 0) {
			SignalError(NULL, "java.io.IOException", SYS_ERROR);
		}
		lport = ntohs(addr.sin_port);
	}
	unhand(this)->localport = lport;
}

/*
 * Turn this socket into a listener.
 */
void
java_net_PlainSocketImpl_socketListen(struct Hjava_net_PlainSocketImpl* this, jint count)
{
	int r;

	r = listen(unhand(unhand(this)->fd)->fd, count);
	if (r < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}
}

/*
 * Accept a connection.
 */
void
java_net_PlainSocketImpl_socketAccept(struct Hjava_net_PlainSocketImpl* this, struct Hjava_net_SocketImpl* sock)
{
	int r;
	size_t alen;
	struct sockaddr_in addr;

	alen = sizeof(addr);
#if defined(BSD44)
	addr.sin_len = sizeof(addr);
#endif
	addr.sin_family = AF_INET;
	addr.sin_port = htons(unhand(sock)->localport);
	addr.sin_addr.s_addr = unhand(unhand(sock)->address)->address;

	r = accept(unhand(unhand(this)->fd)->fd, (struct sockaddr*)&addr, &alen);
	if (r < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}
	unhand(unhand(sock)->fd)->fd = r;

	/* Enter information into socket object */
	alen = sizeof(addr);
	r = getpeername(r, (struct sockaddr*)&addr, &alen);
	if (r < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}

	unhand(unhand(sock)->address)->address = ntohl(addr.sin_addr.s_addr);
	unhand(sock)->port = ntohs(addr.sin_port);
}

/*
 * Return how many bytes can be read without blocking.
 */
jint
java_net_PlainSocketImpl_socketAvailable(struct Hjava_net_PlainSocketImpl* this)
{
	int r;
	jint len;

#if defined(HAVE_IOCTL) && defined(FIONREAD)
	r = ioctl(unhand(unhand(this)->fd)->fd, FIONREAD, &len);
	if (r < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}
#else
	/* This uses select() to work out if we can read - but what
	 * happens at the end of file?
	 */
	static struct timeval tm = { 0, 0 };
	int fd;
	fd_set rd;

	fd = unhand(unhand(this)->fd)->fd;
	FD_ZERO(&rd);
	FD_SET(fd, &rd);
	r = select(fd+1, &rd, NULL, NULL, &tm);
	if (r == 1) {
		len = 1;
	}
	else {
		len = 0;
	}
#endif
	return (len);
}

/*
 * Close this socket.
 */
void
java_net_PlainSocketImpl_socketClose(struct Hjava_net_PlainSocketImpl* this)
{
	int r;

	if (unhand(unhand(this)->fd)->fd != -1) {
		r = close(unhand(unhand(this)->fd)->fd);
		unhand(unhand(this)->fd)->fd = -1;
		if (r < 0) {
			SignalError(NULL, "java.io.IOException", SYS_ERROR);
		}
	}
}

void
java_net_PlainSocketImpl_initProto(struct Hjava_net_PlainSocketImpl* this)
{
	/* ??? */
}

void
java_net_PlainSocketImpl_socketSetOption(struct Hjava_net_PlainSocketImpl* this, jint v1, jbool v2, struct Hjava_lang_Object* v3)
{
	struct Hjava_lang_Integer* intp;
	int v;
	int r;

	switch(v1) {
	case java_net_SocketOptions_SO_REUSEADDR:
		intp = (struct Hjava_lang_Integer*)v3;
		v = unhand(intp)->value;
		r = setsockopt(unhand(unhand(this)->fd)->fd, SOL_SOCKET, SO_REUSEADDR, (void *) &v, sizeof(v));
		if(r < 0) {
			SignalError(NULL, "java.net.SocketException", SYS_ERROR);    
		}
		break;

	case java_net_SocketOptions_TCP_NODELAY:
		/* What does this do ??? */
		break;

	case java_net_SocketOptions_IP_MULTICAST_IF:
	case java_net_SocketOptions_SO_BINDADDR: /* JAVA takes care */
	case java_net_SocketOptions_SO_TIMEOUT: /* JAVA takes care */
	default:
		SignalError(NULL, "java.net.SocketException", "Unimplemented socket option");    
		break;
	} 
}

jint
java_net_PlainSocketImpl_socketGetOption(struct Hjava_net_PlainSocketImpl* this, jint val)
{
	int r;
	int v;
	int s;

	switch(val) {
	case java_net_SocketOptions_SO_BINDADDR:
		r = ntohl(INADDR_ANY);
		break;

	case java_net_SocketOptions_SO_REUSEADDR:
		r = getsockopt(unhand(unhand(this)->fd)->fd, SOL_SOCKET, SO_REUSEADDR, (void *) &v, &s);
		if(r < 0) {
			SignalError(NULL, "java.net.SocketException", SYS_ERROR);    
		}
		r = v;
		break;

	case java_net_SocketOptions_IP_MULTICAST_IF:
	case java_net_SocketOptions_SO_TIMEOUT: /* java takes care */
	default:
		SignalError(NULL, "java.net.SocketException", "Unimplemented socket option");    
	} 
	return (r);
}
