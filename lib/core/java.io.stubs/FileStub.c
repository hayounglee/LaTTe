/* DO NOT EDIT THIS FILE - it is machine generated */
#include <StubPreamble.h>

/* Stubs for class java_io_File */

/* SYMBOL: java_io_File_exists0()Z */
void
Kaffe_java_io_File_exists0_stub(stack_item* _P_, stack_item* _R_)
{
	extern jbool java_io_File_exists0(void*);
	jbool ret = java_io_File_exists0(_P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_io_File_canWrite0()Z */
void
Kaffe_java_io_File_canWrite0_stub(stack_item* _P_, stack_item* _R_)
{
	extern jbool java_io_File_canWrite0(void*);
	jbool ret = java_io_File_canWrite0(_P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_io_File_canRead0()Z */
void
Kaffe_java_io_File_canRead0_stub(stack_item* _P_, stack_item* _R_)
{
	extern jbool java_io_File_canRead0(void*);
	jbool ret = java_io_File_canRead0(_P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_io_File_isFile0()Z */
void
Kaffe_java_io_File_isFile0_stub(stack_item* _P_, stack_item* _R_)
{
	extern jbool java_io_File_isFile0(void*);
	jbool ret = java_io_File_isFile0(_P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_io_File_isDirectory0()Z */
void
Kaffe_java_io_File_isDirectory0_stub(stack_item* _P_, stack_item* _R_)
{
	extern jbool java_io_File_isDirectory0(void*);
	jbool ret = java_io_File_isDirectory0(_P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_io_File_lastModified0()J */
void
Kaffe_java_io_File_lastModified0_stub(stack_item* _P_, stack_item* _R_)
{
	extern jlong java_io_File_lastModified0(void*);
	jlong ret = java_io_File_lastModified0(_P_[0].p);
	return_long(ret);
}

/* SYMBOL: java_io_File_length0()J */
void
Kaffe_java_io_File_length0_stub(stack_item* _P_, stack_item* _R_)
{
	extern jlong java_io_File_length0(void*);
	jlong ret = java_io_File_length0(_P_[0].p);
	return_long(ret);
}

/* SYMBOL: java_io_File_mkdir0()Z */
void
Kaffe_java_io_File_mkdir0_stub(stack_item* _P_, stack_item* _R_)
{
	extern jbool java_io_File_mkdir0(void*);
	jbool ret = java_io_File_mkdir0(_P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_io_File_renameTo0(Ljava/io/File;)Z */
void
Kaffe_java_io_File_renameTo0_stub(stack_item* _P_, stack_item* _R_)
{
	extern jbool java_io_File_renameTo0(void*, void*);
	jbool ret = java_io_File_renameTo0(_P_[1].p, _P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_io_File_delete0()Z */
void
Kaffe_java_io_File_delete0_stub(stack_item* _P_, stack_item* _R_)
{
	extern jbool java_io_File_delete0(void*);
	jbool ret = java_io_File_delete0(_P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_io_File_rmdir0()Z */
void
Kaffe_java_io_File_rmdir0_stub(stack_item* _P_, stack_item* _R_)
{
	extern jbool java_io_File_rmdir0(void*);
	jbool ret = java_io_File_rmdir0(_P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_io_File_list0()[Ljava/lang/String; */
void
Kaffe_java_io_File_list0_stub(stack_item* _P_, stack_item* _R_)
{
	extern HArrayOfObject* java_io_File_list0(void*);
	HArrayOfObject* ret = java_io_File_list0(_P_[0].p);
	return_ref(ret);
}

/* SYMBOL: java_io_File_canonPath(Ljava/lang/String;)Ljava/lang/String; */
void
Kaffe_java_io_File_canonPath_stub(stack_item* _P_, stack_item* _R_)
{
	extern struct Hjava_lang_String* java_io_File_canonPath(void*, void*);
	struct Hjava_lang_String* ret = java_io_File_canonPath(_P_[1].p, _P_[0].p);
	return_ref(ret);
}

/* SYMBOL: java_io_File_isAbsolute()Z */
void
Kaffe_java_io_File_isAbsolute_stub(stack_item* _P_, stack_item* _R_)
{
	extern jbool java_io_File_isAbsolute(void*);
	jbool ret = java_io_File_isAbsolute(_P_[0].p);
	return_int(ret);
}