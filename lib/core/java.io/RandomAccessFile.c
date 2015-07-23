/*
 * java.io.RandomAccessFile.c
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "lib-license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 */

#include "config.h"
#include <stdio.h>
#include <assert.h>
#include "config-std.h"
#include "config-io.h"
#include "config-mem.h"
#include "defs.h"
#include "files.h"
#include "java.io.stubs/RandomAccessFile.h"
#include "java.io.stubs/FileDescriptor.h"
#include "kthread.h"

/*
 * Open a file for random access.
 */
void
java_io_RandomAccessFile_open(struct Hjava_io_RandomAccessFile* this, struct Hjava_lang_String* name, jbool rw)
{
	int fd;
	char str[MAXPATHLEN];

	javaString2CString(name, str, sizeof(str));

	fd = open(str, (rw == 0 ? O_RDONLY : O_RDWR|O_CREAT)|O_BINARY, 0666);
	unhand(unhand(this)->fd)->fd = fd;
	if (fd < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}
}

/*
 * Return length of file.
 */
jlong
java_io_RandomAccessFile_length(struct Hjava_io_RandomAccessFile* this)
{
	struct stat buf;
	int r;

	r = fstat(unhand(unhand(this)->fd)->fd, &buf);
	if (r < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}
	return (off_t2jlong(buf.st_size));
}

/*
 * Seek into file.
 */
void
java_io_RandomAccessFile_seek(struct Hjava_io_RandomAccessFile* this, jlong pos)
{
	int r;

	r = lseek(unhand(unhand(this)->fd)->fd, jlong2off_t(pos), SEEK_SET);
	if (r < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}
}

/*
 * Read in bytes from file.
 */
jint
java_io_RandomAccessFile_readBytes(struct Hjava_io_RandomAccessFile* this, HArrayOfByte* bytes, jint off, jint len)
{
	jint ret;

	/* Adjust length */
	len = (len < obj_length(bytes) - off ? len : obj_length(bytes) - off);

	ret = read(unhand(unhand(this)->fd)->fd, &unhand(bytes)->body[off], len);
	if (ret < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}
	return (ret > 0 ? ret : -1);
}

/*
 * Read a byte from file.
 */
jint
java_io_RandomAccessFile_read(struct Hjava_io_RandomAccessFile* this)
{
	jint ret;
	unsigned char byte;

	ret = read(unhand(unhand(this)->fd)->fd, &byte, 1);
	if (ret < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}

	return (ret > 0 ? byte : -1);
}

/*
 * Write a byte to file.
 */
void
java_io_RandomAccessFile_write(struct Hjava_io_RandomAccessFile* this, jint data)
{
	jint ret;
	unsigned char byte;

	byte = data;

	ret = write(unhand(unhand(this)->fd)->fd, &byte, 1);
	if (ret < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}
}

/*
 * Write a number of bytes to file.
 */
void
java_io_RandomAccessFile_writeBytes(struct Hjava_io_RandomAccessFile* this, HArrayOfByte* bytes, jint off, jint len)
{
	jint ret;

	ret = write(unhand(unhand(this)->fd)->fd, &unhand(bytes)->body[off], len);
	if (ret < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}
}

/*
 * Get current file position.
 */
jlong
java_io_RandomAccessFile_getFilePointer(struct Hjava_io_RandomAccessFile* this)
{
	off_t r;

	r = lseek(unhand(unhand(this)->fd)->fd, 0, SEEK_CUR);
	if (r < 0) {
		SignalError(NULL, "java.io.IOException", SYS_ERROR);
	}
	return (off_t2jlong(r));
}

/*
 * Close file.
 */
void
java_io_RandomAccessFile_close(struct Hjava_io_RandomAccessFile* this)
{
	int r;

	if (unhand(unhand(this)->fd)->fd >= 0) {
		r = close(unhand(unhand(this)->fd)->fd);
		unhand(unhand(this)->fd)->fd = -1;
		if (r < 0) {
			SignalError(NULL, "java.io.IOException", SYS_ERROR);
		}
	}
}
