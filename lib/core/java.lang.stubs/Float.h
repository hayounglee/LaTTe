/* DO NOT EDIT THIS FILE - it is machine generated */
#include <native.h>

#ifndef _Included_java_lang_Float
#define _Included_java_lang_Float

#ifdef __cplusplus
extern "C" {
#endif

/* Header for class java_lang_Float */

typedef struct Classjava_lang_Float {
#define java_lang_Float_POSITIVE_INFINITY Inf
#define java_lang_Float_NEGATIVE_INFINITY -Inf
#define java_lang_Float_NaN NaN
#define java_lang_Float_MAX_VALUE 3.40282e+38
#define java_lang_Float_MIN_VALUE 1.4013e-45
  jfloat value;
} Classjava_lang_Float;
HandleTo(java_lang_Float);

extern jint java_lang_Float_floatToIntBits(jfloat);
extern jfloat java_lang_Float_intBitsToFloat(jint);

#ifdef __cplusplus
}
#endif

#endif