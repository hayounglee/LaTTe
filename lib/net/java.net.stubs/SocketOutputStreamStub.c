/* DO NOT EDIT THIS FILE - it is machine generated */
#include <StubPreamble.h>

/* Stubs for class java_net_SocketOutputStream */

/* SYMBOL: java_net_SocketOutputStream_socketWrite([BII)V */
void
Kaffe_java_net_SocketOutputStream_socketWrite_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_net_SocketOutputStream_socketWrite(void*, HArrayOfByte*, jint, jint);
	java_net_SocketOutputStream_socketWrite(_P_[3].p, _P_[2].p, _P_[1].i, _P_[0].i);
}
