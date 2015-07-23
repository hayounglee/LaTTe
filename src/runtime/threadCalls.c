/*
 * threadCalls.c
 * Support for threaded ops which may block (read, write, connect, etc.).
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 */

#define	DBG(s)

#include "config.h"
#include "config-std.h"
#include "config-io.h"
#include "config-mem.h"
#include "lerrno.h"
#include "object.h"
#include "thread.h"
#include "md.h"

/*
 * We only need this stuff is we are using the internal thread system.
 */
#if !defined(USE_NATIVE_THREADS)

/* These are undefined because we do not yet support async I/O */
#undef	F_SETOWN
#undef	FIOSETOWN
#undef	O_ASYNC
#undef	FIOASYNC

/*
 * Create a threaded file descriptor.
 */
int
threadedFileDescriptor(int fd)
{
	/* Don't use non-blocking IO for the moment, since this may
           affect the invoking process.  (As an example, bash would
           start consuming all the CPU after executing LaTTe.)

	   FIXME: enable non-blocking IO, and reset the file
	   descriptors when exiting LaTTe */
#if !defined(BLOCKING_CALLS) && 0
	int r;
#if defined(HAVE_IOCTL) && defined(FIOASYNC)
	int on = 1;
#endif
	int pid;

	/* Make non-blocking */
#if defined(HAVE_FCNTL) && defined(O_NONBLOCK)
	r = fcntl(fd, F_GETFL, 0);
	r = fcntl(fd, F_SETFL, r|O_NONBLOCK);
#elif defined(HAVE_IOCTL) && defined(FIONBIO)
	r = ioctl(fd, FIONBIO, &on);
#else
	r = 0;
#endif
	if (r < 0) {
		return (r);
	}

	/* Allow socket to signal this process when new data is available */
	pid = getpid();
#if defined(HAVE_FCNTL) && defined(F_SETOWN)
	r = fcntl(fd, F_SETOWN, pid);
#elif defined(HAVE_IOCTL) && defined(FIOSETOWN)
	r = ioctl(fd, FIOSETOWN, &pid);
#else
	r = 0;
#endif
	if (r < 0) {
		return (r);
	}

#if defined(HAVE_FCNTL) && defined(O_ASYNC)
	r = fcntl(fd, F_GETFL, 0);
	r = fcntl(fd, F_SETFL, r|O_ASYNC);
#elif defined(HAVE_IOCTL) && defined(FIOASYNC)
	r = ioctl(fd, FIOASYNC, &on);
#else
	r = 0;
#endif
	if (r < 0) {
		return (r);
	}
#endif
	return (fd);
}

/*
 * Threaded create socket.
 */
int
threadedSocket(int af, int type, int proto)
{
	int fd;

	fd = socket(af, type, proto);
	return (threadedFileDescriptor(fd));
}

/*
 * Threaded file open.
 */
int
threadedOpen(const char* path, int flags, int mode)
{
	int fd;

	fd = open(path, flags, mode);
	return (threadedFileDescriptor(fd));
}

/*
 * Threaded socket connect.
 */
int
threadedConnect(int fd, struct sockaddr* addr, size_t len)
{
	int r;

#if !defined(BLOCKING_CALLS)
	r = connect(fd, addr, len);
	if ((r < 0) && (errno == EINPROGRESS || errno == EALREADY || errno == EWOULDBLOCK || errno == EINTR)) {
		blockOnFile(fd, TH_CONNECT);
		r = 0; /* Assume it's okay when we get released */
	}
#else
	do {
		r = connect(fd, addr, len);
	} while ((r < 0) && (errno == EINTR || errno == EALREADY));
	if (r < 0 && errno == EISCONN) {
		r = 0;
	}
#endif

	return (r);
}

/*
 * Threaded socket accept.
 */
int
threadedAccept(int fd, struct sockaddr* addr, size_t* len)
{
	int r;

	for (;;) {
#if defined(BLOCKING_CALLS)
		blockOnFile(fd, TH_ACCEPT);
#endif
		r = accept(fd, addr, len);
		if (r >= 0 || !(errno == EINPROGRESS || errno == EALREADY || errno == EWOULDBLOCK)) {
			break;
		}
#if !defined(BLOCKING_CALLS)
		blockOnFile(fd, TH_ACCEPT);
#endif
	}
	return (threadedFileDescriptor(r));
}

/*
 * Read but only if we can.
 */
ssize_t
threadedRead(int fd, void* buf, size_t len)
{
	ssize_t r;

#if defined(BLOCKING_CALLS)
	if (blockOnFile(fd, TH_READ) < 0) {
		return (-1);
	}
#endif
	for (;;) {
		r = read(fd, buf, len);
		if (r >= 0 || !(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
			return (r);
		}
		if (blockOnFile(fd, TH_READ) < 0) {
			return (-1);
		}
	}
}

/*
 * Write but only if we can.
 */
ssize_t
threadedWrite(int fd, const void* buf, size_t len)
{
	ssize_t r;
	const void* ptr;

	ptr = buf;
	r = 1;

	while (len > 0 && r > 0) {
#if defined(BLOCKING_CALLS)
		if (blockOnFile(fd, TH_WRITE) < 0) {
			return (-1);
		}
#endif
		r = (ssize_t)write(fd, ptr, len);
		if (r >= 0) {
			ptr = (void*)((uint8*)ptr + r);
			len -= r;
		}
		else if (!(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
			return (-1);
		}
		else {
#if !defined(BLOCKING_CALLS)
			if (blockOnFile(fd, TH_WRITE) < 0) {
				return (-1);
			}
#endif
			r = 1;
		}
	}
	return ((uint8*)ptr - (uint8*)buf);
}

/*
 * Recvfrom but only if we can.
 */
ssize_t 
threadedRecvfrom(int fd, char* buf, size_t len, int flags, struct sockaddr* from, size_t* fromlen)
{
	ssize_t r;
 
#if defined(BLOCKING_CALLS)
	if (blockOnFile(fd, TH_READ) < 0) {
		return (-1);
	}
#endif
		for (;;) {
		r = recvfrom(fd, buf, len, flags, from, fromlen);
		if (r >= 0 || !(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
			return (r);
		}
		if (blockOnFile(fd, TH_READ) < 0) {
			return (-1);
		}
	}
}

#endif
