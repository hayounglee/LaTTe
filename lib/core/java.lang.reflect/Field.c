/*
 * java.lang.reflect.Field.c
 *
 * Copyright (c) 1996,97 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.co.uk>
 *
 * Modified by MASS Laboratory, SNU, 1999
 */

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "gtypes.h"
#include "access.h"
#include "runtime/constants.h"
#include "runtime/object.h"
#include "runtime/classMethod.h"
#include "runtime/itypes.h"
#include "runtime/support.h"
#include "runtime/soft.h"
#include "java.io.stubs/InputStream.h"
#include "java.io.stubs/PrintStream.h"
#include "java.lang.stubs/System.h"
#include "java.lang.reflect.stubs/Field.h"
#include <native.h>
#include "defs.h"

static
char*
getFieldAddress(Hjava_lang_reflect_Field* this, struct Hjava_lang_Object* obj)
{
	Hjava_lang_Class* clas;
	Field* fld;

	clas = (Hjava_lang_Class*)unhand(this)->clazz;
	fld = CLASS_FIELDS(clas) + unhand(this)->slot;

	if (unhand(this)->slot < CLASS_NSFIELDS(clas)) {
		return (FIELD_ADDRESS(fld));
	}
	else {
		if (obj == NULL) {
			SignalError(0, "java.lang.NullPointerException", "");
		}
		if  (!soft_instanceof(fld->type, obj)) {
			SignalError(0,"java.lang.IllegalArgumentException","");
		}
		return (((char*)(obj+1)) + FIELD_OFFSET(fld));
	}
}

struct Hjava_lang_Object*
java_lang_reflect_Field_get(Hjava_lang_reflect_Field* this, struct Hjava_lang_Object* obj)
{
	Hjava_lang_Class* clas;
	Field* fld;
	char* base;

	clas = (Hjava_lang_Class*) unhand(this)->clazz;
	fld = CLASS_FIELDS(clas) + unhand(this)->slot;
	base = getFieldAddress(this, obj);
	if (Class_IsPrimitiveType(fld->type)) {  /* FIXME */
		unimp("Field.get not implemented for primitive fields");
	}
	return (*(struct Hjava_lang_Object**)base);
}

void
java_lang_reflect_Field_set(Hjava_lang_reflect_Field* this, struct Hjava_lang_Object* obj, struct Hjava_lang_Object* value)
{
	Hjava_lang_Class* clas;
	Field* fld;
	char* base;

	clas = (Hjava_lang_Class*) unhand(this)->clazz;
	fld = CLASS_FIELDS(clas) + unhand(this)->slot;
	base = getFieldAddress(this, obj);
	if (Class_IsPrimitiveType(fld->type)) {  /* FIXME */
		unimp("Field.set not implemented for primitive fields");
	}
	if (fld->accflags & ACC_FINAL) {
		SignalError(0,"java.lang.IllegalAccessException", "");
	}
	if (value != NULL && ! soft_instanceof(fld->type, value)) {
		SignalError(0,"java.lang.IllegalArgumentException", "");
	}
	*(struct Hjava_lang_Object**)base = value;
}

jint 
java_lang_reflect_Field_getModifiers(struct Hjava_lang_reflect_Field *arg1) {
    unimp("Field not fully implemented yet");
}

jbool 
java_lang_reflect_Field_getBoolean(struct Hjava_lang_reflect_Field *arg1, 
                                   struct Hjava_lang_Object *arg2) {
    unimp("Field not fully implemented yet");
}

jbyte 
java_lang_reflect_Field_getByte(struct Hjava_lang_reflect_Field *arg1, 
                                struct Hjava_lang_Object *arg2) {
    unimp("Field not fully implemented yet");
}

jchar 
java_lang_reflect_Field_getChar(struct Hjava_lang_reflect_Field *arg1, 
                                struct Hjava_lang_Object *arg2) {
    unimp("Field not fully implemented yet");
}

jshort 
java_lang_reflect_Field_getShort(struct Hjava_lang_reflect_Field *arg1, 
                                 struct Hjava_lang_Object *arg2) {
    unimp("Field not fully implemented yet");
}

jint 
java_lang_reflect_Field_getInt(struct Hjava_lang_reflect_Field *arg1, 
                               struct Hjava_lang_Object *arg2) {
    unimp("Field not fully implemented yet");
}

jlong 
java_lang_reflect_Field_getLong(struct Hjava_lang_reflect_Field *arg1, 
                                struct Hjava_lang_Object *arg2) {
    unimp("Field not fully implemented yet");
}

jfloat 
java_lang_reflect_Field_getFloat(struct Hjava_lang_reflect_Field *arg1, 
                                 struct Hjava_lang_Object *arg2) {
    unimp("Field not fully implemented yet");
}

jdouble 
java_lang_reflect_Field_getDouble(struct Hjava_lang_reflect_Field *arg1, 
                                  struct Hjava_lang_Object *arg2) {
    unimp("Field not fully implemented yet");
}

void
java_lang_reflect_Field_setBoolean(struct Hjava_lang_reflect_Field *arg1, 
                                   struct Hjava_lang_Object *arg2, 
                                   jbool arg3) {
    unimp("Field not fully implemented yet");
}

void 
java_lang_reflect_Field_setByte(struct Hjava_lang_reflect_Field *arg1, 
                                struct Hjava_lang_Object *arg2, 
                                jbyte arg3) {
    unimp("Field not fully implemented yet");
}

void 
java_lang_reflect_Field_setChar(struct Hjava_lang_reflect_Field *arg1, 
                                struct Hjava_lang_Object *arg2, 
                                jchar arg3) {
    unimp("Field not fully implemented yet");
}

void 
java_lang_reflect_Field_setShort(struct Hjava_lang_reflect_Field *arg1, 
                                 struct Hjava_lang_Object *arg2, 
                                 jshort arg3) {
    unimp("Field not fully implemented yet");
}

void 
java_lang_reflect_Field_setInt(struct Hjava_lang_reflect_Field *arg1, 
                               struct Hjava_lang_Object *arg2, 
                               jint arg3) {
    unimp("Field not fully implemented yet");
}

void 
java_lang_reflect_Field_setLong(struct Hjava_lang_reflect_Field *arg1, 
                                struct Hjava_lang_Object *arg2, 
                                jlong arg3) {
    unimp("Field not fully implemented yet");
}

void 
java_lang_reflect_Field_setFloat(struct Hjava_lang_reflect_Field *arg1, 
                                 struct Hjava_lang_Object *arg2, 
                                 jfloat arg3) {
    unimp("Field not fully implemented yet");
}

void 
java_lang_reflect_Field_setDouble(struct Hjava_lang_reflect_Field *arg1, 
                                  struct Hjava_lang_Object *arg2, 
                                  jdouble arg3) {
    unimp("Field not fully implemented yet");
}
