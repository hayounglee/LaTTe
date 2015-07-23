/* DO NOT EDIT THIS FILE - it is machine generated */
#include <native.h>

#ifndef _Included_java_net_PlainSocketImpl
#define _Included_java_net_PlainSocketImpl

#ifdef __cplusplus
extern "C" {
#endif

/* Header for class java_net_PlainSocketImpl */

typedef struct Classjava_net_PlainSocketImpl {
  struct Hjava_io_FileDescriptor* fd;
  struct Hjava_net_InetAddress* address;
  jint port;
  jint localport;
  jint timeout;
#define java_net_PlainSocketImpl_socksServerProp "socksProxyHost"
#define java_net_PlainSocketImpl_socksPortProp "socksProxyPort"
#define java_net_PlainSocketImpl_socksDefaultPortStr "1080"
} Classjava_net_PlainSocketImpl;
HandleTo(java_net_PlainSocketImpl);

extern void java_net_PlainSocketImpl_socketCreate(struct Hjava_net_PlainSocketImpl*, jbool);
extern void java_net_PlainSocketImpl_socketConnect(struct Hjava_net_PlainSocketImpl*, struct Hjava_net_InetAddress*, jint);
extern void java_net_PlainSocketImpl_socketBind(struct Hjava_net_PlainSocketImpl*, struct Hjava_net_InetAddress*, jint);
extern void java_net_PlainSocketImpl_socketListen(struct Hjava_net_PlainSocketImpl*, jint);
extern void java_net_PlainSocketImpl_socketAccept(struct Hjava_net_PlainSocketImpl*, struct Hjava_net_SocketImpl*);
extern jint java_net_PlainSocketImpl_socketAvailable(struct Hjava_net_PlainSocketImpl*);
extern void java_net_PlainSocketImpl_socketClose(struct Hjava_net_PlainSocketImpl*);
extern void java_net_PlainSocketImpl_initProto();
extern void java_net_PlainSocketImpl_socketSetOption(struct Hjava_net_PlainSocketImpl*, jint, jbool, struct Hjava_lang_Object*);
extern jint java_net_PlainSocketImpl_socketGetOption(struct Hjava_net_PlainSocketImpl*, jint);

#ifdef __cplusplus
}
#endif

#endif