/* DO NOT EDIT THIS FILE - it is machine generated */
#include <native.h>

#ifndef _Included_java_io_ObjectInputStream
#define _Included_java_io_ObjectInputStream

#ifdef __cplusplus
extern "C" {
#endif

/* Header for class java_io_ObjectInputStream */

typedef struct Classjava_io_ObjectInputStream {
  struct Hjava_io_InputStream* in;
  jint count;
  jbool blockDataMode;
  HArrayOfByte* buffer;
  struct Hjava_io_DataInputStream* dis;
  struct Hjava_io_IOException* abortIOException;
  struct Hjava_lang_ClassNotFoundException* abortClassNotFoundException;
  struct Hjava_lang_Object* currentObject;
  struct Hjava_io_ObjectStreamClass* currentClassDesc;
  struct Hjava_lang_Class* currentClass;
  HArrayOfObject* classdesc;
  HArrayOfObject* classes;
  jint spClass;
  struct Hjava_util_Vector* wireHandle2Object;
  jint nextWireOffset;
  struct Hjava_util_Vector* callbacks;
  jint recursionDepth;
  jbyte currCode;
  jbool enableResolve;
} Classjava_io_ObjectInputStream;
HandleTo(java_io_ObjectInputStream);

extern struct Hjava_lang_Class* java_io_ObjectInputStream_loadClass0(struct Hjava_io_ObjectInputStream*, struct Hjava_lang_Class*, struct Hjava_lang_String*);
extern void java_io_ObjectInputStream_inputClassFields(struct Hjava_io_ObjectInputStream*, struct Hjava_lang_Object*, struct Hjava_lang_Class*, HArrayOfInt*);
extern struct Hjava_lang_Object* java_io_ObjectInputStream_allocateNewObject(struct Hjava_lang_Class*, struct Hjava_lang_Class*);
extern struct Hjava_lang_Object* java_io_ObjectInputStream_allocateNewArray(struct Hjava_lang_Class*, jint);
extern jbool java_io_ObjectInputStream_invokeObjectReader(struct Hjava_io_ObjectInputStream*, struct Hjava_lang_Object*, struct Hjava_lang_Class*);

#ifdef __cplusplus
}
#endif

#endif
