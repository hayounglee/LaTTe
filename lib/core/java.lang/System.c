/*
 * java.lang.System.c
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

#define	DBG(s)

#include "config.h"
#include "config-std.h"
#include "config-io.h"
#include "config-mem.h"
#if defined(HAVE_SYS_UTSNAME_H)
#include <sys/utsname.h>
#endif
#if defined(HAVE_PWD_H)
#include <pwd.h>
#endif
#include "gtypes.h"
#include "access.h"
#include "runtime/object.h"
#include "runtime/constants.h"
#include "runtime/baseClasses.h"
#include "runtime/classMethod.h"
#include "runtime/support.h"
#include "runtime/soft.h"
#include "system.h"
#include "defs.h"
#include "java.io.stubs/InputStream.h"
#include "java.io.stubs/PrintStream.h"
#include "java.lang.stubs/System.h"
#include <native.h>

static char cwdpath[MAXPATHLEN];

extern jlong currentTime(void);
extern userProperty* userProperties;
extern char* realClassPath;

/*
 * Copy one part of an array to another.
 */
void
java_lang_System_arraycopy(struct Hjava_lang_Object* src, jint srcpos, struct Hjava_lang_Object* dst, jint dstpos, jint len)
{
	char* in;
	char* out;
	int elemsz;
	Hjava_lang_Class* sclass;
	Hjava_lang_Class* dclass;

	sclass = OBJECT_CLASS(src);
	dclass = OBJECT_CLASS(dst);

	/* Must be arrays */
	if (! Class_IsArrayType(sclass) || ! Class_IsArrayType(dclass)) {
		SignalError(0, "java.lang.ArrayStoreException", "");
	}

	/* Make sure we'll keep in the array boundaries */
	if ((srcpos < 0 || srcpos + len > ARRAY_SIZE(src)) ||
	    (dstpos < 0 || dstpos + len > ARRAY_SIZE(dst)) ||
	    (len < 0)) {
		SignalError(0, "java.lang.ArrayIndexOutOfBoundsException", "");
	}

	sclass = Class_GetComponentType(sclass);
	dclass = Class_GetComponentType(dclass);
	elemsz = TYPE_SIZE(sclass);

	len *= elemsz;
	srcpos *= elemsz;
	dstpos *= elemsz;

	in = &((char*)ARRAY_DATA(src))[srcpos];
	out = &((char*)ARRAY_DATA(dst))[dstpos];

	if (sclass == dclass) {
//#if defined(HAVE_MEMMOVE)
/*
#ifdef 0
		memmove((void*)out, (void*)in, len);
#else
*/
		/* Do it ourself */
#if defined(HAVE_MEMCPY)
		if (src != dst) {
			memcpy((void*)out, (void*)in, len);
		}
		else
#endif
		if (out < in) {
			/* Copy forwards */
			for (; len > 0; len--) {
				*out++ = *in++;
			}
		}
		else {
			/* Copy backwards */
			out += len;
			in += len;
			for (; len > 0; len--) {
				*--out = *--in;
			}
		}
/*
#endif
*/
		return;
	}

	if (Class_IsPrimitiveType(sclass) || Class_IsPrimitiveType(dclass))
		SignalError(0, "java.lang.ArrayStoreException", "");
	
	for (; len > 0; len -= sizeof(Hjava_lang_Object**)) {
		Hjava_lang_Object* val = *(Hjava_lang_Object**)in;
		if (val != NULL && !soft_instanceof(dclass, val)) {
			SignalError(0, "java.lang.ArrayStoreException", "");
		}
		*(Hjava_lang_Object**)out = val;
		in += sizeof(Hjava_lang_Object*);
		out += sizeof(Hjava_lang_Object*);
	}
}

/*
 * Initialise system properties to their defaults.
 */
struct Hjava_util_Properties*
java_lang_System_initProperties(struct Hjava_util_Properties* p)
{
	int r;
	char* jhome;
	char* dir;
	userProperty* prop;
#if defined(HAVE_SYS_UTSNAME_H)
	struct utsname system;
#endif
#if defined(HAVE_PWD_H)
	struct passwd* pw;
#endif

	/* Add the default properties:
	 * java.version		Java version number
	 * java.vendor          Java vendor specific string
	 * java.vendor.url      Java vendor URL
	 * java.home            Java installation directory
	 * java.class.version   Java class version number
	 * java.class.path      Java classpath
	 * os.name              Operating System Name
	 * os.arch              Operating System Architecture
	 * os.version           Operating System Version
	 * file.separator       File separator ("/" on Unix)
	 * path.separator       Path separator (":" on Unix)
	 * line.separator       Line separator ("\n" on Unix)
	 * user.name            User account name
	 * user.home            User home directory
	 * user.dir             User's current working directory
	 */

	setProperty(p, "java.version", vm_version);
	setProperty(p, "java.vendor", vm_vendor);
	setProperty(p, "java.vendor.url", vm_vendor_url);

	jhome = getenv(LATTEHOME);
	if (jhome == 0) {
		jhome = ".";
	}
	setProperty(p, "java.home", jhome);

	setProperty(p, "java.class.version", vm_class_version);
	setProperty(p, "java.class.path", realClassPath);
	setProperty(p, "file.separator", file_seperator);
	setProperty(p, "path.separator", path_seperator);
	setProperty(p, "line.separator", line_seperator);

#if defined(HAVE_GETCWD)
	dir = getcwd(cwdpath, MAXPATHLEN);
#elif defined(HAVE_GETWD)
	dir = getwd(cwdpath);
#else
	dir = 0;	/* Cannot get current directory */
#endif
	if (dir == 0) {
		dir = ".";
	}
	setProperty(p, "user.dir", dir);

#if defined(HAVE_SYS_UTSNAME_H) && defined(HAVE_UNAME)
	/* Setup system properties */
	r = uname(&system);
	assert(r >= 0);
	setProperty(p, "os.name", system.sysname);
	setProperty(p, "os.arch", system.machine);
	setProperty(p, "os.version", system.version);
#else
	setProperty(p, "os.name", "Unknown");
	setProperty(p, "os.arch", "Unknown");
	setProperty(p, "os.version", "Unknown");
#endif
#if defined(HAVE_PWD_H) && defined(HAVE_GETUID)
	/* Setup user properties */
	pw = getpwuid(getuid());
	if (pw != 0) {
		setProperty(p, "user.name", pw->pw_name);
		setProperty(p, "user.home", pw->pw_dir);
	}
	else
#endif
	{
		setProperty(p, "user.name", "Unknown");
		setProperty(p, "user.home", "Unknown");
	}

	/* JDK 1.1 */
	setProperty(p, "file.encoding.pkg", "sun.io");

	/* Add in the awt.Toolkit we will be using */
#if defined(HAVE_PACKAGE_BISS_NET_COM)
	setProperty(p, "awt.toolkit", "biss.awt.kernel.Toolkit");
#elif defined(HAVE_PACKAGE_EPFL_CH)
	setProperty(p, "awt.toolkit", "kaffe.awt.simple.SimpleToolkit");
#endif
#if defined(HAVE_AWT_TOOLKIT_BISS) || defined(HAVE_AWT_TOOLKIT_SAWT)
	jresources = malloc(strlen(jhome) + strlen("/lib") + 1);
	strcpy(jresources, jhome);
	strcat(jresources, "/lib");
	setProperty(p, "java.resources", jresources);
	free(jresources);
#endif

	/* Now process user defined properties */
	for (prop = userProperties; prop != 0; prop = prop->next) {
		setProperty(p, prop->key, prop->value);
	}

	return (p);
}

/*
 * Return current time.
 */
jlong
java_lang_System_currentTimeMillis(void)
{
	return (currentTime());
}

/*
 * Set the stdin stream.
 */
void
java_lang_System_setIn0(struct Hjava_io_InputStream* stream)
{
	*(struct Hjava_io_InputStream**)FIELD_ADDRESS(&CLASS_SFIELDS(SystemClass)[0]) = stream;
}

/*
 * Set the stdout stream.
 */
void
java_lang_System_setOut0(struct Hjava_io_PrintStream* stream)
{
	*(struct Hjava_io_PrintStream**)FIELD_ADDRESS(&CLASS_SFIELDS(SystemClass)[1]) = stream;
}

/*
 * Set the error stream.
 */
void
java_lang_System_setErr0(struct Hjava_io_PrintStream* stream)
{
	*(struct Hjava_io_PrintStream**)FIELD_ADDRESS(&CLASS_SFIELDS(SystemClass)[2]) = stream;
}

extern jint java_lang_Object_hashCode(struct Hjava_lang_Object*);

jint
java_lang_System_identityHashCode(struct Hjava_lang_Object* o)
{
       return (java_lang_Object_hashCode(o));
}
