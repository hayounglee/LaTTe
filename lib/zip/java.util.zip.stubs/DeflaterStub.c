/* DO NOT EDIT THIS FILE - it is machine generated */
#include <StubPreamble.h>

/* Stubs for class java_util_zip_Deflater */

/* SYMBOL: java_util_zip_Deflater_setDictionary([BII)V */
void
Kaffe_java_util_zip_Deflater_setDictionary_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_util_zip_Deflater_setDictionary(void*, HArrayOfByte*, jint, jint);
	java_util_zip_Deflater_setDictionary(_P_[3].p, _P_[2].p, _P_[1].i, _P_[0].i);
}

/* SYMBOL: java_util_zip_Deflater_deflate([BII)I */
void
Kaffe_java_util_zip_Deflater_deflate_stub(stack_item* _P_, stack_item* _R_)
{
	extern jint java_util_zip_Deflater_deflate(void*, HArrayOfByte*, jint, jint);
	jint ret = java_util_zip_Deflater_deflate(_P_[3].p, _P_[2].p, _P_[1].i, _P_[0].i);
	return_int(ret);
}

/* SYMBOL: java_util_zip_Deflater_getAdler()I */
void
Kaffe_java_util_zip_Deflater_getAdler_stub(stack_item* _P_, stack_item* _R_)
{
	extern jint java_util_zip_Deflater_getAdler(void*);
	jint ret = java_util_zip_Deflater_getAdler(_P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_util_zip_Deflater_getTotalIn()I */
void
Kaffe_java_util_zip_Deflater_getTotalIn_stub(stack_item* _P_, stack_item* _R_)
{
	extern jint java_util_zip_Deflater_getTotalIn(void*);
	jint ret = java_util_zip_Deflater_getTotalIn(_P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_util_zip_Deflater_getTotalOut()I */
void
Kaffe_java_util_zip_Deflater_getTotalOut_stub(stack_item* _P_, stack_item* _R_)
{
	extern jint java_util_zip_Deflater_getTotalOut(void*);
	jint ret = java_util_zip_Deflater_getTotalOut(_P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_util_zip_Deflater_reset()V */
void
Kaffe_java_util_zip_Deflater_reset_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_util_zip_Deflater_reset(void*);
	java_util_zip_Deflater_reset(_P_[0].p);
}

/* SYMBOL: java_util_zip_Deflater_end()V */
void
Kaffe_java_util_zip_Deflater_end_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_util_zip_Deflater_end(void*);
	java_util_zip_Deflater_end(_P_[0].p);
}

/* SYMBOL: java_util_zip_Deflater_init(Z)V */
void
Kaffe_java_util_zip_Deflater_init_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_util_zip_Deflater_init(void*, jbool);
	java_util_zip_Deflater_init(_P_[1].p, _P_[0].i);
}
