/*
 * java.util.zip.Deflater.c
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "lib-license.terms" for information on usage and
 * redistribution of this file.  */

#define	DBG(s)

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include <native.h>
#include "runtime/locks.h"
#include "Deflater.h"

#if defined(HAVE_LIBZ) && defined(HAVE_ZLIB_H)

#include <zconf.h>
#include <zlib.h>

#define	WSIZE		0x8000
#define	WSIZEBITS	15

#define GET_STREAM(THIS)        (*(z_stream**)&unhand(this)->strm)

void
java_util_zip_Deflater_setDictionary(struct Hjava_util_zip_Deflater* this, HArrayOfByte* buf, jint from, jint len)
{
	int r;
        z_stream* dstream;

	//
	// inserted by doner
	// - This function must be synchronized.
	//

	_lockMutex( (Hjava_lang_Object*)this );

        dstream = GET_STREAM(this);

	r = deflateSetDictionary (dstream, &unhand(buf)->body[from], len);
	if (r < 0) {
		SignalError(0, "java.lang.Error", dstream->msg ? dstream->msg : "unknown error");
	}

	_unlockMutex( (Hjava_lang_Object*)this );
}

jint
java_util_zip_Deflater_deflate(struct Hjava_util_zip_Deflater* this, HArrayOfByte* buf, jint off, jint len)
{
	int r;
	int ilen;
        z_stream* dstream;
	jint   ret_val;

	//
	// inserted by doner
	// - This function must be synchronized.
	//
	
	_lockMutex( (Hjava_lang_Object*)this );

        dstream = GET_STREAM(this);

	ilen = unhand(this)->len;

	dstream->next_in = &unhand(unhand(this)->buf)->body[unhand(this)->off];
	dstream->avail_in = ilen;

	dstream->next_out = &unhand(buf)->body[off];
	dstream->avail_out = len;

	r = deflate(dstream, unhand(this)->finish ? Z_FINISH : Z_NO_FLUSH);

DBG(	printf("Deflate: in %d left %d out %d status %d\n", ilen, dstream->avail_in, len - dstream->avail_out, r);	)

	switch (r) {
	case Z_OK:
		break;

	case Z_STREAM_END:
		unhand(this)->finished = 1;
		break;

	default:
		SignalError(0, "java.lang.Error", dstream->msg ? dstream->msg : "unknown error");
	}

	unhand(this)->off += (ilen - dstream->avail_in);
	unhand(this)->len = dstream->avail_in;

	ret_val = len - dstream->avail_out;

	_unlockMutex( (Hjava_lang_Object*)this );

	return ret_val;
}

jint
java_util_zip_Deflater_getAdler(struct Hjava_util_zip_Deflater* this)
{
	jint   ret_val;

	//
	// This function must be synchronized.
	//

	_lockMutex( (Hjava_lang_Object*)this );

	ret_val = GET_STREAM( this )->adler;

	_unlockMutex( (Hjava_lang_Object*)this );

	return ret_val;
}

jint
java_util_zip_Deflater_getTotalIn(struct Hjava_util_zip_Deflater* this)
{
	jint    ret_val;

	//
	// This function must be synchronized.
	//

	_lockMutex( (Hjava_lang_Object*)this );

	ret_val = GET_STREAM( this )->total_in;

	_unlockMutex( (Hjava_lang_Object*)this );

	return ret_val;
}

jint
java_util_zip_Deflater_getTotalOut(struct Hjava_util_zip_Deflater* this)
{
	jint   ret_val;

	//
	// This function must be synchronized.
	//

	_lockMutex( (Hjava_lang_Object*)this );

	ret_val = GET_STREAM( this )->total_out;

	_unlockMutex( (Hjava_lang_Object*)this );

	return ret_val;
}

void
java_util_zip_Deflater_reset(struct Hjava_util_zip_Deflater* this)
{
	//
	// This function must be synchronized.
	//

	_lockMutex( (Hjava_lang_Object*)this );

	deflateReset(GET_STREAM(this));

	unhand(this)->finish = 0;
	unhand(this)->finished = 0;

	_unlockMutex( (Hjava_lang_Object*)this );
}

void
java_util_zip_Deflater_end(struct Hjava_util_zip_Deflater* this)
{
	z_stream* dstream;

	//
	// This function must be synchronized.
	//

	_lockMutex( (Hjava_lang_Object*)this );

	dstream = GET_STREAM(this);
	deflateEnd(dstream);
	free(dstream);

	_unlockMutex( (Hjava_lang_Object*)this );
}

void
java_util_zip_Deflater_init(struct Hjava_util_zip_Deflater* this, jbool val)
{
	int r;
    z_stream* dstream;

	dstream = malloc(sizeof(z_stream));
	dstream->next_in = 0;
	dstream->zalloc = 0;
	dstream->zfree = 0;
	dstream->opaque = 0;

	r = deflateInit2(dstream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -WSIZEBITS, 9, Z_DEFAULT_STRATEGY);
	if (r != Z_OK) {
		SignalError(0, "java.lang.Error", dstream ? dstream->msg : "");
	}

	GET_STREAM(this) = dstream;
}

#endif
