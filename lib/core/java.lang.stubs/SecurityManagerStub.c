/* DO NOT EDIT THIS FILE - it is machine generated */
#include <StubPreamble.h>

/* Stubs for class java_lang_SecurityManager */

/* SYMBOL: java_lang_SecurityManager_getClassContext()[Ljava/lang/Class; */
void
Kaffe_java_lang_SecurityManager_getClassContext_stub(stack_item* _P_, stack_item* _R_)
{
	extern HArrayOfObject* java_lang_SecurityManager_getClassContext(void*);
	HArrayOfObject* ret = java_lang_SecurityManager_getClassContext(_P_[0].p);
	return_ref(ret);
}

/* SYMBOL: java_lang_SecurityManager_currentClassLoader()Ljava/lang/ClassLoader; */
void
Kaffe_java_lang_SecurityManager_currentClassLoader_stub(stack_item* _P_, stack_item* _R_)
{
	extern struct Hjava_lang_ClassLoader* java_lang_SecurityManager_currentClassLoader(void*);
	struct Hjava_lang_ClassLoader* ret = java_lang_SecurityManager_currentClassLoader(_P_[0].p);
	return_ref(ret);
}

/* SYMBOL: java_lang_SecurityManager_classDepth(Ljava/lang/String;)I */
void
Kaffe_java_lang_SecurityManager_classDepth_stub(stack_item* _P_, stack_item* _R_)
{
	extern jint java_lang_SecurityManager_classDepth(void*, void*);
	jint ret = java_lang_SecurityManager_classDepth(_P_[1].p, _P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_lang_SecurityManager_classLoaderDepth()I */
void
Kaffe_java_lang_SecurityManager_classLoaderDepth_stub(stack_item* _P_, stack_item* _R_)
{
	extern jint java_lang_SecurityManager_classLoaderDepth(void*);
	jint ret = java_lang_SecurityManager_classLoaderDepth(_P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_lang_SecurityManager_currentLoadedClass0()Ljava/lang/Class; */
void
Kaffe_java_lang_SecurityManager_currentLoadedClass0_stub(stack_item* _P_, stack_item* _R_)
{
	extern struct Hjava_lang_Class* java_lang_SecurityManager_currentLoadedClass0(void*);
	struct Hjava_lang_Class* ret = java_lang_SecurityManager_currentLoadedClass0(_P_[0].p);
	return_ref(ret);
}
