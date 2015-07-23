/*
 * java.lang.Class.c
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "lib-license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
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
#include "runtime/errors.h"
#include "runtime/exception.h"
#include "runtime/exception_handler.h"
#include "java.io.stubs/InputStream.h"
#include "java.io.stubs/PrintStream.h"
#include "java.lang.stubs/System.h"
#include "java.lang.reflect.stubs/Field.h"
#include <native.h>
#include "defs.h"

/*
 * Convert string name to class object.
 */
struct Hjava_lang_Class*
java_lang_Class_forName(struct Hjava_lang_String* str)
{
	Hjava_lang_Class* clazz;
	char buf[MAXNAMELEN];

	/* Get string and convert '.' to '/' */
	javaString2CString(str, buf, sizeof(buf));
	classname2pathname(buf, buf);

	EH_NATIVE_DURING
		clazz = loadClass(makeUtf8Const (buf, strlen(buf)), 0);
	EH_NATIVE_HANDLER
		/* Failed to load class ... */
		throwException(ClassNotFoundException(buf));
	EH_NATIVE_ENDHANDLER

	processClass(clazz, CSTATE_OK);

	return (clazz);
}

/*
 * Convert class to string name.
 */
struct Hjava_lang_String*
java_lang_Class_getName(struct Hjava_lang_Class* c)
{
	char buffer[100];
	struct Hjava_lang_String* str;
	int len;
	char* name;
	int i;
	char* ptr;
	char ch;

	len = c->name->length;
	name = len > 100 ? (char*)malloc(len) : buffer;
	ptr = c->name->data;
	for (i = 0; i < len; i++) {
		ch = *ptr++;
		if (ch == '/') {
			ch = '.';
		}
		name[i] = ch;
	}
	str = makeJavaString(name, len);
	if (name != buffer) {
		free (name);
	}
	return (str);
}

/*
 * Create a new instance of the derived class.
 */
struct Hjava_lang_Object*
java_lang_Class_newInstance(struct Hjava_lang_Class* this)
{
	return (execute_java_constructor(0, 0, this, "()V"));
}

/*
 * Return super class.
 */
struct Hjava_lang_Class*
java_lang_Class_getSuperclass(struct Hjava_lang_Class* this)
{
	return (this->superclass);
}

HArrayOfObject* /* [Ljava.lang.Class; */
java_lang_Class_getInterfaces(struct Hjava_lang_Class* this)
{
	HArrayOfObject* obj;
	struct Hjava_lang_Class** ifaces;
	int i;

	obj = (HArrayOfObject*)AllocObjectArray(this->interface_len, "Ljava/lang/Class");
	ifaces = (struct Hjava_lang_Class**)unhand(obj)->body;
	for (i = 0; i < this->interface_len; i++) {
		ifaces[i] = this->interfaces[i];
	}

	return (obj);
}

/*
 * Return the class loader which loaded me.
 */
struct Hjava_lang_ClassLoader*
java_lang_Class_getClassLoader(struct Hjava_lang_Class* this)
{
	return (this->loader);
}

/*
 * Is the class an interface?
 */
jbool
java_lang_Class_isInterface(struct Hjava_lang_Class* this)
{
	return ((this->accflags & ACC_INTERFACE) ? 1 : 0);
}

jbool
java_lang_Class_isPrimitive(struct Hjava_lang_Class* this)
{
	return (Class_IsPrimitiveType(this));
}

jbool
java_lang_Class_isArray(struct Hjava_lang_Class* this)
{
	return (Class_IsArrayType(this));
}

Hjava_lang_Class*
java_lang_Class_getComponentType(struct Hjava_lang_Class* this)
{
	if (Class_IsArrayType(this)) {
		return (Class_GetComponentType(this));
	}
	else {
		return ((Hjava_lang_Class*)0);
	}
}

jbool
java_lang_Class_isAssignableFrom(struct Hjava_lang_Class* this, struct Hjava_lang_Class* cls)
{
	return (instanceof(this, cls));
}

/*
 * Get primitive class from class name (JDK 1.1)
 */
struct Hjava_lang_Class*
java_lang_Class_getPrimitiveClass(struct Hjava_lang_String* name)
{
	jchar* chrs;

	chrs = &unhand(unhand(name)->value)->body[unhand(name)->offset];
	switch (chrs[0]) {
	case 'b':
		if (chrs[1] == 'y') {
			return (byteClass);
		}
		if (chrs[1] == 'o') {
			return (booleanClass);
		}
		break;
	case 'c':
		return (charClass);
	case 'd':
		return (doubleClass);
	case 'f':
		return (floatClass);
	case 'i':
		return (intClass);
	case 'l':
		return (longClass);
	case 's':
		return (shortClass);
	case 'v':
		return (voidClass);
	}
	return(NULL);
}

/*
 * Is object instance of this class?
 */
jbool
java_lang_Class_isInstance(struct Hjava_lang_Class* this, struct Hjava_lang_Object* obj)
{
	return (soft_instanceof(this, obj));
}

jint
java_lang_Class_getModifiers(struct Hjava_lang_Class* this)
{
	return (this->accflags);
}

jint
java_lang_Class_getSigners()
{
	unimp("java.lang.Class:getSigners unimplemented");
}

jint
java_lang_Class_setSigners()
{
	unimp("java.lang.Class:setSigners unimplemented");
}

jint
java_lang_Class_getMethods0()
{
	unimp("java.lang.Class:getMethods0 unimplemented");
}

jint
java_lang_Class_getConstructors0()
{
	unimp("java.lang.Class:getConstructors0 unimplemented");
}

jint
java_lang_Class_getMethod0()
{
	unimp("java.lang.Class:getMethod0 unimplemented");
}

jint
java_lang_Class_getConstructor0()
{
	unimp("java.lang.Class:getConstructor0 unimplemented");
}

static
Hjava_lang_reflect_Field*
makeField(struct Hjava_lang_Class* clazz, int slot)
{
	Hjava_lang_reflect_Field* field;
	Field* fld;

	fld = CLASS_FIELDS((Hjava_lang_Class*)clazz) + slot;
	field = (Hjava_lang_reflect_Field*)AllocObject("java/lang/reflect/Field");
	unhand(field)->clazz = (struct Hjava_lang_Class*) clazz;
	unhand(field)->slot = slot;
	unhand(field)->type = (struct Hjava_lang_Class*) fld->type;
	unhand(field)->name = Utf8Const2JavaString(fld->name);
	return (field);
}

HArrayOfObject*
java_lang_Class_getFields0(struct Hjava_lang_Class* clazz, int declared)
{
	int count;
	Hjava_lang_Class* clas;
	Field* fld;
	Hjava_lang_reflect_Field** ptr;
	HArrayOfObject* array;
	int i;

	if (declared) {
		count = CLASS_NFIELDS((Hjava_lang_Class*)clazz);
	}
	else {
		count = 0;
		clas = (Hjava_lang_Class*) clazz;
		for (; clas != NULL; clas = clas->superclass) {
			fld = CLASS_FIELDS(clas);
			i = CLASS_NFIELDS(clas);
			for ( ; --i >= 0;  ++fld) {
				if (fld->accflags & ACC_PUBLIC) {
					count++;
				}
			}
		}
	}
	array = (HArrayOfObject*)AllocObjectArray(count, "Ljava/lang/reflect/Field;");
	ptr = (Hjava_lang_reflect_Field**) ARRAY_DATA(array) + count;
	clas = (Hjava_lang_Class*) clazz;
	do {
		fld = CLASS_FIELDS(clas);
		i = CLASS_NFIELDS(clas);
		for ( ; --i >= 0;  ++fld) {
			if (! (fld->accflags & ACC_PUBLIC) && ! declared) {
				continue;
			}
			*--ptr = makeField(clas, i);
		}
		clas = clas->superclass;
	} while (!declared && clas != NULL);

	return (array);
}

Hjava_lang_reflect_Field*
java_lang_Class_getField0(struct Hjava_lang_Class* clazz, struct Hjava_lang_String* name, int declared)
{
	Hjava_lang_Class* clas;

	clas = (Hjava_lang_Class*) clazz;
	do {
		Field* fld = CLASS_FIELDS(clas);
		int n = CLASS_NFIELDS(clas);
		int i;
		for (i = 0;  i < n;  ++fld, ++i) {
			if (((fld->accflags & ACC_PUBLIC) || declared)
			    && equalUtf8JavaStrings (fld->name, name)) {
				return makeField(clas, i);
			}
		}
		clas = clas->superclass;
	} while (!declared && clas != NULL);
	SignalError(0, "java.lang.NoSuchFieldException", ""); /* FIXME */
}
