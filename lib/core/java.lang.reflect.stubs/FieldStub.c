/* DO NOT EDIT THIS FILE - it is machine generated */
#include <StubPreamble.h>

/* Stubs for class java_lang_reflect_Field */

/* SYMBOL: java_lang_reflect_Field_getModifiers()I */
void
Kaffe_java_lang_reflect_Field_getModifiers_stub(stack_item* _P_, stack_item* _R_)
{
	extern jint java_lang_reflect_Field_getModifiers(void*);
	jint ret = java_lang_reflect_Field_getModifiers(_P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_lang_reflect_Field_get(Ljava/lang/Object;)Ljava/lang/Object; */
void
Kaffe_java_lang_reflect_Field_get_stub(stack_item* _P_, stack_item* _R_)
{
	extern struct Hjava_lang_Object* java_lang_reflect_Field_get(void*, void*);
	struct Hjava_lang_Object* ret = java_lang_reflect_Field_get(_P_[1].p, _P_[0].p);
	return_ref(ret);
}

/* SYMBOL: java_lang_reflect_Field_getBoolean(Ljava/lang/Object;)Z */
void
Kaffe_java_lang_reflect_Field_getBoolean_stub(stack_item* _P_, stack_item* _R_)
{
	extern jbool java_lang_reflect_Field_getBoolean(void*, void*);
	jbool ret = java_lang_reflect_Field_getBoolean(_P_[1].p, _P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_lang_reflect_Field_getByte(Ljava/lang/Object;)B */
void
Kaffe_java_lang_reflect_Field_getByte_stub(stack_item* _P_, stack_item* _R_)
{
	extern jbyte java_lang_reflect_Field_getByte(void*, void*);
	jbyte ret = java_lang_reflect_Field_getByte(_P_[1].p, _P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_lang_reflect_Field_getChar(Ljava/lang/Object;)C */
void
Kaffe_java_lang_reflect_Field_getChar_stub(stack_item* _P_, stack_item* _R_)
{
	extern jchar java_lang_reflect_Field_getChar(void*, void*);
	jchar ret = java_lang_reflect_Field_getChar(_P_[1].p, _P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_lang_reflect_Field_getShort(Ljava/lang/Object;)S */
void
Kaffe_java_lang_reflect_Field_getShort_stub(stack_item* _P_, stack_item* _R_)
{
	extern jshort java_lang_reflect_Field_getShort(void*, void*);
	jshort ret = java_lang_reflect_Field_getShort(_P_[1].p, _P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_lang_reflect_Field_getInt(Ljava/lang/Object;)I */
void
Kaffe_java_lang_reflect_Field_getInt_stub(stack_item* _P_, stack_item* _R_)
{
	extern jint java_lang_reflect_Field_getInt(void*, void*);
	jint ret = java_lang_reflect_Field_getInt(_P_[1].p, _P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_lang_reflect_Field_getLong(Ljava/lang/Object;)J */
void
Kaffe_java_lang_reflect_Field_getLong_stub(stack_item* _P_, stack_item* _R_)
{
	extern jlong java_lang_reflect_Field_getLong(void*, void*);
	jlong ret = java_lang_reflect_Field_getLong(_P_[1].p, _P_[0].p);
	return_long(ret);
}

/* SYMBOL: java_lang_reflect_Field_getFloat(Ljava/lang/Object;)F */
void
Kaffe_java_lang_reflect_Field_getFloat_stub(stack_item* _P_, stack_item* _R_)
{
	extern jfloat java_lang_reflect_Field_getFloat(void*, void*);
	jfloat ret = java_lang_reflect_Field_getFloat(_P_[1].p, _P_[0].p);
	return_float(ret);
}

/* SYMBOL: java_lang_reflect_Field_getDouble(Ljava/lang/Object;)D */
void
Kaffe_java_lang_reflect_Field_getDouble_stub(stack_item* _P_, stack_item* _R_)
{
	extern jdouble java_lang_reflect_Field_getDouble(void*, void*);
	jdouble ret = java_lang_reflect_Field_getDouble(_P_[1].p, _P_[0].p);
	return_double(ret);
}

/* SYMBOL: java_lang_reflect_Field_set(Ljava/lang/Object;Ljava/lang/Object;)V */
void
Kaffe_java_lang_reflect_Field_set_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_lang_reflect_Field_set(void*, void*, void*);
	java_lang_reflect_Field_set(_P_[2].p, _P_[1].p, _P_[0].p);
}

/* SYMBOL: java_lang_reflect_Field_setBoolean(Ljava/lang/Object;Z)V */
void
Kaffe_java_lang_reflect_Field_setBoolean_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_lang_reflect_Field_setBoolean(void*, void*, jbool);
	java_lang_reflect_Field_setBoolean(_P_[2].p, _P_[1].p, _P_[0].i);
}

/* SYMBOL: java_lang_reflect_Field_setByte(Ljava/lang/Object;B)V */
void
Kaffe_java_lang_reflect_Field_setByte_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_lang_reflect_Field_setByte(void*, void*, jbyte);
	java_lang_reflect_Field_setByte(_P_[2].p, _P_[1].p, _P_[0].i);
}

/* SYMBOL: java_lang_reflect_Field_setChar(Ljava/lang/Object;C)V */
void
Kaffe_java_lang_reflect_Field_setChar_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_lang_reflect_Field_setChar(void*, void*, jchar);
	java_lang_reflect_Field_setChar(_P_[2].p, _P_[1].p, _P_[0].i);
}

/* SYMBOL: java_lang_reflect_Field_setShort(Ljava/lang/Object;S)V */
void
Kaffe_java_lang_reflect_Field_setShort_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_lang_reflect_Field_setShort(void*, void*, jshort);
	java_lang_reflect_Field_setShort(_P_[2].p, _P_[1].p, _P_[0].i);
}

/* SYMBOL: java_lang_reflect_Field_setInt(Ljava/lang/Object;I)V */
void
Kaffe_java_lang_reflect_Field_setInt_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_lang_reflect_Field_setInt(void*, void*, jint);
	java_lang_reflect_Field_setInt(_P_[2].p, _P_[1].p, _P_[0].i);
}

/* SYMBOL: java_lang_reflect_Field_setLong(Ljava/lang/Object;J)V */
void
Kaffe_java_lang_reflect_Field_setLong_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_lang_reflect_Field_setLong(void*, void*, jlong);
	java_lang_reflect_Field_setLong(_P_[3].p, _P_[2].p, _P_[0].l);
}

/* SYMBOL: java_lang_reflect_Field_setFloat(Ljava/lang/Object;F)V */
void
Kaffe_java_lang_reflect_Field_setFloat_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_lang_reflect_Field_setFloat(void*, void*, jfloat);
	java_lang_reflect_Field_setFloat(_P_[2].p, _P_[1].p, _P_[0].f);
}

/* SYMBOL: java_lang_reflect_Field_setDouble(Ljava/lang/Object;D)V */
void
Kaffe_java_lang_reflect_Field_setDouble_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_lang_reflect_Field_setDouble(void*, void*, jdouble);
	java_lang_reflect_Field_setDouble(_P_[3].p, _P_[2].p, _P_[0].d);
}
