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

#ifndef __threadCalls_h
#define __threadCalls_h

struct sockaddr;

extern int threadedFileDescriptor(int);
extern int threadedSocket(int, int, int);
extern int threadedOpen(const char*, int, int);
extern int threadedConnect(int, struct sockaddr*, size_t);
extern int threadedAccept(int, struct sockaddr*, size_t*);
extern ssize_t threadedRead(int, char*, size_t);
extern ssize_t threadedWrite(int, const char*, size_t);
exern ssize_t threadedRecvfrom(int, char*, size_t, int, struct sockaddr*, size_t*);


#endif
