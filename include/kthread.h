/*
 * kthread.h
 * Define the threading used in the system.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 */

#ifndef __kthread_h
#define __kthread_h

#include "md.h"

#if !defined(USE_NATIVE_THREADS)
/*
 * We 'magically' thread various calls.
 */
struct sockaddr;
extern int threadedFileDescriptor(int);
extern int threadedSocket(int, int, int);
extern int threadedOpen(const char*, int, int);
extern int threadedConnect(int, struct sockaddr*, size_t);
extern int threadedAccept(int, struct sockaddr*, size_t*);
extern ssize_t threadedRead(int, char*, size_t);
extern ssize_t threadedWrite(int, const char*, size_t);
extern ssize_t threadedRecvfrom(int, char*, size_t, int, struct sockaddr*, size_t*);

#define	fixfd(_fd)			threadedFileDescriptor(_fd)
#define	open(_path, _flgs, _mode)	threadedOpen(_path, _flgs, _mode)
#define	read(_fd, _addr, _len)		threadedRead(_fd, _addr, _len)
#define	write(_fd, _addr, _len)		threadedWrite(_fd, _addr, _len)
#define	socket(_af, _type, _prt)	threadedSocket(_af, _type, _prt)
#define	connect(_fd, _addr, _len)	threadedConnect(_fd, _addr, _len)
#define	accept(_fd, _addr, _len)	threadedAccept(_fd, _addr, _len)
#define	recvfrom(_fd, _buf, _len, _flgs, _addr, _addlen) \
		threadedRecvfrom(_fd, _buf, _len, _flgs, _addr, _addlen)

#else

#define	fixfd(_fd)			(_fd)

#endif

#endif
