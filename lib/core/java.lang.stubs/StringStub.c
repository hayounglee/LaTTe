/* DO NOT EDIT THIS FILE - it is machine generated */
#include <StubPreamble.h>

/* Stubs for class java_lang_String */

/* SYMBOL: java_lang_String_intern()Ljava/lang/String; */
void
Kaffe_java_lang_String_intern_stub(stack_item* _P_, stack_item* _R_)
{
	extern struct Hjava_lang_String* java_lang_String_intern(void*);
	struct Hjava_lang_String* ret = java_lang_String_intern(_P_[0].p);
	return_ref(ret);
}
