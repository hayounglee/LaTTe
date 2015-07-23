/* DO NOT EDIT THIS FILE - it is machine generated */
#include <StubPreamble.h>

/* Stubs for class java_lang_ClassLoader */

/* SYMBOL: java_lang_ClassLoader_init()V */
void
Kaffe_java_lang_ClassLoader_init_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_lang_ClassLoader_init(void*);
	java_lang_ClassLoader_init(_P_[0].p);
}

/* SYMBOL: java_lang_ClassLoader_defineClass0(Ljava/lang/String;[BII)Ljava/lang/Class; */
void
Kaffe_java_lang_ClassLoader_defineClass0_stub(stack_item* _P_, stack_item* _R_)
{
	extern struct Hjava_lang_Class* java_lang_ClassLoader_defineClass0(void*, void*, HArrayOfByte*, jint, jint);
	struct Hjava_lang_Class* ret = java_lang_ClassLoader_defineClass0(_P_[4].p, _P_[3].p, _P_[2].p, _P_[1].i, _P_[0].i);
	return_ref(ret);
}

/* SYMBOL: java_lang_ClassLoader_resolveClass0(Ljava/lang/Class;)V */
void
Kaffe_java_lang_ClassLoader_resolveClass0_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_lang_ClassLoader_resolveClass0(void*, void*);
	java_lang_ClassLoader_resolveClass0(_P_[1].p, _P_[0].p);
}

/* SYMBOL: java_lang_ClassLoader_findSystemClass0(Ljava/lang/String;)Ljava/lang/Class; */
void
Kaffe_java_lang_ClassLoader_findSystemClass0_stub(stack_item* _P_, stack_item* _R_)
{
	extern struct Hjava_lang_Class* java_lang_ClassLoader_findSystemClass0(void*, void*);
	struct Hjava_lang_Class* ret = java_lang_ClassLoader_findSystemClass0(_P_[1].p, _P_[0].p);
	return_ref(ret);
}

/* SYMBOL: java_lang_ClassLoader_getSystemResourceAsStream0(Ljava/lang/String;)Ljava/io/InputStream; */
void
Kaffe_java_lang_ClassLoader_getSystemResourceAsStream0_stub(stack_item* _P_, stack_item* _R_)
{
	extern struct Hjava_io_InputStream* java_lang_ClassLoader_getSystemResourceAsStream0(void*);
	struct Hjava_io_InputStream* ret = java_lang_ClassLoader_getSystemResourceAsStream0(_P_[0].p);
	return_ref(ret);
}

/* SYMBOL: java_lang_ClassLoader_getSystemResourceAsName0(Ljava/lang/String;)Ljava/lang/String; */
void
Kaffe_java_lang_ClassLoader_getSystemResourceAsName0_stub(stack_item* _P_, stack_item* _R_)
{
	extern struct Hjava_lang_String* java_lang_ClassLoader_getSystemResourceAsName0(void*);
	struct Hjava_lang_String* ret = java_lang_ClassLoader_getSystemResourceAsName0(_P_[0].p);
	return_ref(ret);
}
