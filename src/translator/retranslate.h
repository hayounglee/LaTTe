#ifndef _retranslate_h_
#define _retranslate_h_

#ifdef METHOD_COUNT

#define COUNT_CODE_SIZE (11 * 4)

/* Retranslate given method.  Transfers control to new code after
   translation is finished. */
#ifdef CUSTOMIZATION
#include "SpecializedMethod.h"

void retranslate( SpecializedMethod *sm, Hjava_lang_Object* obj );
void generate_count_code(SpecializedMethod* m, unsigned* code, unsigned* target);

#else

void retranslate( Method *method );
void generate_count_code(Method* m, unsigned* code, unsigned* target);

#endif

/* Print number of translated methods and retranslated methods.
   (Currently ignores kaffe_translate()'ed methods.) */
void print_trstats( void );
#endif /* METHOD_COUNT */

#endif
