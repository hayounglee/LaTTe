/* DO NOT EDIT THIS FILE - it is machine generated */
#include <StubPreamble.h>

/* Stubs for class java_io_ObjectOutputStream */

/* SYMBOL: java_io_ObjectOutputStream_outputClassFields(Ljava/lang/Object;Ljava/lang/Class;[I)V */
void
Kaffe_java_io_ObjectOutputStream_outputClassFields_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_io_ObjectOutputStream_outputClassFields(void*, void*, void*, HArrayOfInt*);
	java_io_ObjectOutputStream_outputClassFields(_P_[3].p, _P_[2].p, _P_[1].p, _P_[0].p);
}

/* SYMBOL: java_io_ObjectOutputStream_invokeObjectWriter(Ljava/lang/Object;Ljava/lang/Class;)Z */
void
Kaffe_java_io_ObjectOutputStream_invokeObjectWriter_stub(stack_item* _P_, stack_item* _R_)
{
	extern jbool java_io_ObjectOutputStream_invokeObjectWriter(void*, void*, void*);
	jbool ret = java_io_ObjectOutputStream_invokeObjectWriter(_P_[2].p, _P_[1].p, _P_[0].p);
	return_int(ret);
}
