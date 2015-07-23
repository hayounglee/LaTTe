/*
 * java.net.PlainDatagramSocketImpl.c
 *
 * Copyright (c) 1996,97 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "lib-license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.co.uk>
 *
 * Modified by MASS Laboratory, SNU, 1999
 */

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "config-io.h"
#include <native.h>
#include "../core/java.lang.stubs/Integer.h"
#include "../core/java.io.stubs/FileDescriptor.h"
#include "java.net.stubs/DatagramPacket.h"
#include "java.net.stubs/PlainDatagramSocketImpl.h"
#include "java.net.stubs/InetAddress.h"
#include "java.net.stubs/SocketOptions.h"
#include "nets.h"
#include "kthread.h"
#include "runtime/locks.h"

/*
 * Create a datagram socket.
 */
void
java_net_PlainDatagramSocketImpl_datagramSocketCreate(struct Hjava_net_PlainDatagramSocketImpl* this)
{
	int fd;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	unhand(unhand(this)->fd)->fd = fd;
	if (fd < 0) {
		SignalError(NULL, "java.net.SocketException", SYS_ERROR);
	}
}

/*
 * Bind a port to the socket.
 */
void
java_net_PlainDatagramSocketImpl_bind(struct Hjava_net_PlainDatagramSocketImpl* this, jint port, struct Hjava_net_InetAddress* laddr)
{
	int r;
	struct sockaddr_in addr;

	//
	// inserted by doner
	// - This function must be synchronized.
	//

	_lockMutex( (Hjava_lang_Object*)this );

#if defined(BSD44)
	addr.sin_len = sizeof(addr);
#endif
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(unhand(laddr)->address);

	r = bind(unhand(unhand(this)->fd)->fd, (struct sockaddr*)&addr, sizeof(addr));
	if (r < 0) {
		SignalError(NULL, "java.net.SocketException", SYS_ERROR);
	}

	_unlockMutex( (Hjava_lang_Object*)this );
}

void
java_net_PlainDatagramSocketImpl_send(struct Hjava_net_PlainDatagramSocketImpl* this, struct Hjava_net_DatagramPacket* pkt)
{
	int r;
	struct sockaddr_in addr;

#if defined(BSD44)
	addr.sin_len = sizeof(addr);
#endif
	addr.sin_family = AF_INET;
	addr.sin_port = htons(unhand(pkt)->port);
	addr.sin_addr.s_addr = htonl(unhand(unhand(pkt)->address)->address);

	r = sendto(unhand(unhand(this)->fd)->fd, unhand(unhand(pkt)->buf)->body, unhand(pkt)->length, 0, (struct sockaddr*)&addr, sizeof(addr));
	if (r < 0) {
		SignalError(NULL, "java.net.SocketException", SYS_ERROR);
	}
}

jint
java_net_PlainDatagramSocketImpl_peek(struct Hjava_net_PlainDatagramSocketImpl* this, struct Hjava_net_InetAddress* addr)
{
	int r;
	struct sockaddr_in saddr;
	size_t alen = sizeof(saddr);

	//
	// inserted by doner
	// - This function must be synchronized.
	//

	_lockMutex( (Hjava_lang_Object*)this );

	r = recvfrom(unhand(unhand(this)->fd)->fd, 0, 0, MSG_PEEK, (struct sockaddr*)&saddr, &alen);
	if (r < 0) {
		SignalError(NULL, "java.net.SocketException", SYS_ERROR);
	}

	unhand(addr)->address = ntohl(saddr.sin_addr.s_addr);

	_unlockMutex( (Hjava_lang_Object*)this );

	return (r);
}

void
java_net_PlainDatagramSocketImpl_receive(struct Hjava_net_PlainDatagramSocketImpl* this, struct Hjava_net_DatagramPacket* pkt)
{
	int r;
	struct sockaddr_in addr;
	size_t alen = sizeof(addr);
	struct Hjava_net_InetAddress **fromaddr;

	//
	// inserted by doner
	// - This function must be synchronized.
	//
	_lockMutex( (Hjava_lang_Object*)this );

	r = recvfrom(unhand(unhand(this)->fd)->fd, unhand(unhand(pkt)->buf)->body, unhand(pkt)->length, 0, (struct sockaddr*)&addr, &alen);
	if (r < 0) {
		SignalError(NULL, "java.net.SocketException", SYS_ERROR);
	}

	unhand(pkt)->length = r;
	unhand(pkt)->port = ntohs(addr.sin_port);
	fromaddr = &unhand(pkt)->address;
	if (*fromaddr == 0) {
		*fromaddr = (struct Hjava_net_InetAddress *)
				AllocObject("java/net/InetAddress");
	}
	unhand(*fromaddr)->address = ntohl(addr.sin_addr.s_addr);

	_unlockMutex( (Hjava_lang_Object*)this );
}

/*
 * Close the socket.
 */
void
java_net_PlainDatagramSocketImpl_datagramSocketClose(struct Hjava_net_PlainDatagramSocketImpl* this)
{
	int r;

	if (unhand(unhand(this)->fd)->fd != -1) {
		r = close(unhand(unhand(this)->fd)->fd);
		unhand(unhand(this)->fd)->fd = -1;
		if (r < 0) {
			SignalError(NULL, "java.net.SocketException", SYS_ERROR);
		}
	}
}


void
java_net_PlainDatagramSocketImpl_socketSetOption(struct Hjava_net_PlainDatagramSocketImpl* this, jint v1, struct Hjava_lang_Object* v2)
{

	struct Hjava_net_InetAddress* addrp;
	struct Hjava_lang_Integer* intp;
	int v, r;
	struct sockaddr_in addr;

	switch(v1) {
	case java_net_SocketOptions_SO_BINDADDR:
		/* JAVA takes care */
		abort();
		break;
	case java_net_SocketOptions_SO_REUSEADDR:
		intp = (struct Hjava_lang_Integer*)v2;
		v = unhand(intp)->value;
		r = setsockopt(unhand(unhand(this)->fd)->fd, SOL_SOCKET, SO_REUSEADDR, (void *) &v, sizeof(v));
		if (r < 0) {
			SignalError(NULL, "java.net.SocketException", SYS_ERROR);    
		}
		break;
	case java_net_SocketOptions_IP_MULTICAST_IF:
#if defined(IP_MULTICAST_IF)
		addrp = (struct Hjava_net_InetAddress*)v2;
#if defined(BSD44)
		addr.sin_len = sizeof(addr);
#endif
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(unhand(addrp)->address);
		r = setsockopt(unhand(unhand(this)->fd)->fd, IPPROTO_IP, IP_MULTICAST_IF, (void *) &addr, sizeof(addr));
		if (r < 0) {
			SignalError(NULL, "java.net.SocketException", SYS_ERROR);
		}
#else
		SignalError(NULL, "java.net.SocketException", "Not supported");
#endif
		break;
	case java_net_SocketOptions_SO_TIMEOUT:
		/* JAVA takes care */
		abort();
		break;
	default:
		abort();
	} 
}

jint
java_net_PlainDatagramSocketImpl_socketGetOption(struct Hjava_net_PlainDatagramSocketImpl* this, jint val)
{
	int r;
	int v, s;
	struct sockaddr_in addr;

	switch(val) {
	case java_net_SocketOptions_SO_BINDADDR:
		r = ntohl(INADDR_ANY);
		break;
	case java_net_SocketOptions_IP_MULTICAST_IF:
#if defined(IP_MULTICAST_IF)
		r = getsockopt(unhand(unhand(this)->fd)->fd, IPPROTO_IP, IP_MULTICAST_IF, (void *) &addr, &s);
		if (r < 0) {
			SignalError(NULL, "java.net.SocketException", SYS_ERROR);
			return 0;
		}
		r = ntohl(addr.sin_addr.s_addr);    
#else
		SignalError(NULL, "java.net.SocketException", "Not supported");
#endif
		break;
	case java_net_SocketOptions_SO_REUSEADDR:
		r = getsockopt(unhand(unhand(this)->fd)->fd, SOL_SOCKET, SO_REUSEADDR, (void *) &v, &s);
		if (r < 0) {
			SignalError(NULL, "java.net.SocketException", SYS_ERROR);
		}
		r = v;
		break;
	case java_net_SocketOptions_SO_TIMEOUT:
		/* java takes care */
		abort();
		break;
	default:
		abort();
	} 
	return r;
}

/*
 * Join multicast group
 */
void
java_net_PlainDatagramSocketImpl_join(struct Hjava_net_PlainDatagramSocketImpl* this, struct Hjava_net_InetAddress* laddr)
{
#if defined(IP_ADD_MEMBERSHIP)
	int r;
	struct ip_mreq ipm;

	ipm.imr_multiaddr.s_addr = htonl(unhand(laddr)->address);
	ipm.imr_interface.s_addr = htonl(INADDR_ANY);

	r = setsockopt(unhand(unhand(this)->fd)->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *) &ipm, sizeof(ipm));
	if (r < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}
#else
	SignalError(NULL, "java.net.SocketException", "Not supported");
#endif
}

/*
 * leave multicast group
 */
void
java_net_PlainDatagramSocketImpl_leave(struct Hjava_net_PlainDatagramSocketImpl* this, struct Hjava_net_InetAddress* laddr)
{
#if defined(IP_DROP_MEMBERSHIP)
	int r;
	struct ip_mreq ipm;

	ipm.imr_multiaddr.s_addr = htonl(unhand(laddr)->address);
	ipm.imr_interface.s_addr = htonl(INADDR_ANY);

	r = setsockopt(unhand(unhand(this)->fd)->fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void *) &ipm, sizeof(ipm));
	if (r < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}
#else
	SignalError(NULL, "java.net.SocketException", "Not supported");
#endif
}

/*
 * set multicast-TTL
 */
void
java_net_PlainDatagramSocketImpl_setTTL(struct Hjava_net_PlainDatagramSocketImpl* this, jbool ttl)
{
#if defined(IP_MULTICAST_TTL)
	int r;
	unsigned char v = (unsigned char)ttl;

	r = setsockopt(unhand(unhand(this)->fd)->fd, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &v, sizeof(v));
	if (r < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}
#else
	SignalError(NULL, "java.net.SocketException", "Not supported");
#endif
}

/*
 * get multicast-TTL
 */
jbyte
java_net_PlainDatagramSocketImpl_getTTL(struct Hjava_net_PlainDatagramSocketImpl* this)
{
#if defined(IP_MULTICAST_TTL)
	unsigned char v;
	int s;
	int r;

	r = getsockopt(unhand(unhand(this)->fd)->fd, IPPROTO_IP, IP_MULTICAST_TTL, &v, &s);
	if (r < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}
	return (jbyte)v;
#else
	SignalError(NULL, "java.net.SocketException", "Not supported");
#endif
}
