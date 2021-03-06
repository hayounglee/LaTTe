/* DO NOT EDIT THIS FILE - it is machine generated */
#include <StubPreamble.h>

/* Stubs for class java_net_PlainDatagramSocketImpl */

/* SYMBOL: java_net_PlainDatagramSocketImpl_bind(ILjava/net/InetAddress;)V */
void
Kaffe_java_net_PlainDatagramSocketImpl_bind_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_net_PlainDatagramSocketImpl_bind(void*, jint, void*);
	java_net_PlainDatagramSocketImpl_bind(_P_[2].p, _P_[1].i, _P_[0].p);
}

/* SYMBOL: java_net_PlainDatagramSocketImpl_send(Ljava/net/DatagramPacket;)V */
void
Kaffe_java_net_PlainDatagramSocketImpl_send_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_net_PlainDatagramSocketImpl_send(void*, void*);
	java_net_PlainDatagramSocketImpl_send(_P_[1].p, _P_[0].p);
}

/* SYMBOL: java_net_PlainDatagramSocketImpl_peek(Ljava/net/InetAddress;)I */
void
Kaffe_java_net_PlainDatagramSocketImpl_peek_stub(stack_item* _P_, stack_item* _R_)
{
	extern jint java_net_PlainDatagramSocketImpl_peek(void*, void*);
	jint ret = java_net_PlainDatagramSocketImpl_peek(_P_[1].p, _P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_net_PlainDatagramSocketImpl_receive(Ljava/net/DatagramPacket;)V */
void
Kaffe_java_net_PlainDatagramSocketImpl_receive_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_net_PlainDatagramSocketImpl_receive(void*, void*);
	java_net_PlainDatagramSocketImpl_receive(_P_[1].p, _P_[0].p);
}

/* SYMBOL: java_net_PlainDatagramSocketImpl_setTTL(B)V */
void
Kaffe_java_net_PlainDatagramSocketImpl_setTTL_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_net_PlainDatagramSocketImpl_setTTL(void*, jbyte);
	java_net_PlainDatagramSocketImpl_setTTL(_P_[1].p, _P_[0].i);
}

/* SYMBOL: java_net_PlainDatagramSocketImpl_getTTL()B */
void
Kaffe_java_net_PlainDatagramSocketImpl_getTTL_stub(stack_item* _P_, stack_item* _R_)
{
	extern jbyte java_net_PlainDatagramSocketImpl_getTTL(void*);
	jbyte ret = java_net_PlainDatagramSocketImpl_getTTL(_P_[0].p);
	return_int(ret);
}

/* SYMBOL: java_net_PlainDatagramSocketImpl_join(Ljava/net/InetAddress;)V */
void
Kaffe_java_net_PlainDatagramSocketImpl_join_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_net_PlainDatagramSocketImpl_join(void*, void*);
	java_net_PlainDatagramSocketImpl_join(_P_[1].p, _P_[0].p);
}

/* SYMBOL: java_net_PlainDatagramSocketImpl_leave(Ljava/net/InetAddress;)V */
void
Kaffe_java_net_PlainDatagramSocketImpl_leave_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_net_PlainDatagramSocketImpl_leave(void*, void*);
	java_net_PlainDatagramSocketImpl_leave(_P_[1].p, _P_[0].p);
}

/* SYMBOL: java_net_PlainDatagramSocketImpl_datagramSocketCreate()V */
void
Kaffe_java_net_PlainDatagramSocketImpl_datagramSocketCreate_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_net_PlainDatagramSocketImpl_datagramSocketCreate(void*);
	java_net_PlainDatagramSocketImpl_datagramSocketCreate(_P_[0].p);
}

/* SYMBOL: java_net_PlainDatagramSocketImpl_datagramSocketClose()V */
void
Kaffe_java_net_PlainDatagramSocketImpl_datagramSocketClose_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_net_PlainDatagramSocketImpl_datagramSocketClose(void*);
	java_net_PlainDatagramSocketImpl_datagramSocketClose(_P_[0].p);
}

/* SYMBOL: java_net_PlainDatagramSocketImpl_socketSetOption(ILjava/lang/Object;)V */
void
Kaffe_java_net_PlainDatagramSocketImpl_socketSetOption_stub(stack_item* _P_, stack_item* _R_)
{
	extern void java_net_PlainDatagramSocketImpl_socketSetOption(void*, jint, void*);
	java_net_PlainDatagramSocketImpl_socketSetOption(_P_[2].p, _P_[1].i, _P_[0].p);
}

/* SYMBOL: java_net_PlainDatagramSocketImpl_socketGetOption(I)I */
void
Kaffe_java_net_PlainDatagramSocketImpl_socketGetOption_stub(stack_item* _P_, stack_item* _R_)
{
	extern jint java_net_PlainDatagramSocketImpl_socketGetOption(void*, jint);
	jint ret = java_net_PlainDatagramSocketImpl_socketGetOption(_P_[1].p, _P_[0].i);
	return_int(ret);
}
