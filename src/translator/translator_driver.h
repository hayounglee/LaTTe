/* translator_driver.h
   Interface to the LaTTe translator.
   
   Written by: Junpyo Lee <walker@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties. */

#ifndef __TRANSLATOR_DRIVER_H__
#define __TRANSLATOR_DRIVER_H__

struct Hjava_lang_Object;
struct Hjava_lang_Class;
struct CallContext_t;
struct SpecializedMethod_t;
struct _methods;

/* Invoke translator */
void    TD_invoke_translator(struct SpecializedMethod_t*, struct Hjava_lang_Class*);

/* Invoke translator from trampoline code */
void*   TD_fixup_trampoline_with_SM(struct SpecializedMethod_t*);
#ifdef INLINE_CACHE
void*   TD_fixup_trampoline_with_CC(struct CallContext_t*, struct Hjava_lang_Object*);
void*   TD_fixup_trampoline_with_inlinecache(struct Hjava_lang_Object*, int);
#endif
void*   TD_fixup_trampoline_with_object(struct Hjava_lang_Object*, Method*);

// /* Invoke translator from native function */
// void*   TD_get_translated_code(Method*, Hjava_lang_Class*);
// void*   TD_get_generally_translated_code(Method*);

/* Assembly stub functions between trampoline code and translator driver */
void    dispatch_method_with_SM();
void    dispatch_method_with_object();
#ifdef INLINE_CACHE
void    dispatch_method_with_CC();
void    dispatch_method_with_inlinecache();
void    fixup_failed_type_check();
#endif


#endif /* __TRANSLATOR_DRIVER_H__ */
