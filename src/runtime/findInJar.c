/*
 * findInJar.c
 * Search the CLASSPATH for the given class or property name.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 */

#define	FDBG(s)
#define ZDBG(s)
#define	PDBG(s)
#define	CDBG(s)

#include "config.h"
#include "config-std.h"
#include "config-io.h"
#include "config-mem.h"
#include "gtypes.h"
#include "file.h"
#include "exception.h"
#include "zipfile.h"
#include "readClass.h"
#include "paths.h"
#include "flags.h"
#include "errors.h"
#include "lerrno.h"
#include "locks.h"
#include "external.h"
#include "classMethod.h"

#define	MAXBUF		256
#define	MAXPATHELEM	16

#define	CP_INVALID	0
#define	CP_ZIPFILE	1
#define	CP_DIR		2
#define	CP_SOFILE	3

#define	CLASSSYMPREFIX	"__CLASS_"

#define	IS_ZIP(B)	((B)[0] == 'P' && (B)[1] == 'K')

static struct {
	int	type;
	char*	path;
	union {
		ZipFile	zipf;
		struct { 
			int	loaded;
		} sof;
	} u;
} classpath[MAXPATHELEM+1];

char* realClassPath;

void initClasspath(void);
classFile findInJar(char*);

static int getClasspathType(char*);

/*
 * Find the named class in a directory or zip file.
 */
Hjava_lang_Class*
findClass(Hjava_lang_Class* class, char* cname)
{
	char buf[MAXBUF];
	classFile hand;

	/* Look for the class */
CDBG(	printf("Scanning for class %s\n", cname);		)

	strcpy(buf, cname);
	strcat(buf, ".class");

	/* Find class in Jar file */
	hand = findInJar(buf);
	switch (hand.type) {
	case CP_DIR:
	case CP_ZIPFILE:
		if (flag_classload) {
			fprintf(stderr, "Loading class '%s'.\n", cname);
		}
		class = readClass(class, &hand, NULL);
		if (hand.base != 0) {
			gc_free_fixed(hand.base);
		}
		return (class);

	case CP_SOFILE:
		if (flag_classload) {
			fprintf(stderr, "Registering class '%s'.\n", cname);
		}
		class = (Hjava_lang_Class*)hand.base;
		registerClass(class);
		return (class);

	default:
		break;
	}

	/*
	 * Certain classes are essential.  If we don't find them then
	 * abort.
	 */
	if (strcmp(cname, "java/lang/ClassNotFoundException") == 0 ||
	    strcmp(cname, "java/lang/Object") == 0) {
		fprintf(stderr, "Cannot find essential class '%s' in class library ... aborting.\n", cname);
		ABORT();
	}
	return (0);
}

/*
 * Locate the given name in the CLASSPATH.
 */
classFile
findInJar(char* cname)
{
	static quickLock jarlock;
	int i;
	char buf[MAXBUF];
	int fp;
	struct stat sbuf;
	classFile hand;
	ZipDirectory *zipd;
	int j;

	/* Look for the class */
CDBG(	printf("Scanning for element %s\n", cname);		)

	/* One into the jar at once */
	lockMutex(&jarlock);

	for (i = 0; classpath[i].path != 0; i++) {
		hand.type = classpath[i].type;
		switch (classpath[i].type) {
		case CP_ZIPFILE:
ZDBG(			printf("Opening zip file %s for %s\n", classpath[i].path, cname); )
			if (classpath[i].u.zipf.central_directory == 0) {
				classpath[i].u.zipf.fd = open(classpath[i].path, O_RDONLY|O_BINARY);
				if (classpath[i].u.zipf.fd < 0) {
					break;
				}
				if (read_zip_archive(&classpath[i].u.zipf) != 0) {
					close(classpath[i].u.zipf.fd);
					break;
				}
				close(classpath[i].u.zipf.fd);
			}

			zipd = (ZipDirectory*)classpath[i].u.zipf.central_directory;
			for (j = 0; j < classpath[i].u.zipf.count; j++, zipd = ZIPDIR_NEXT(zipd)) {

ZDBG(				printf ("%d: size:%d, name(#%d)%s, offset:%d\n", i, zipd->size, zipd->filename_length, ZIPDIR_FILENAME (zipd), zipd->filestart);  )
				if (cname[0] == ZIPDIR_FILENAME(zipd)[0] && strcmp(cname, ZIPDIR_FILENAME(zipd)) == 0) {

ZDBG(					printf("FOUND!!\n");		)

					/* If size is negative then this
					 * is compressed !!
					 */
					if (zipd->size == -1) {
						throwException(IOException("cannot handle compressed entry in JAR/ZIP file"));
					}

#if defined(HAVE_MMAP) && defined(HAVE_UNALIGNEDACCESS)
					hand.size = zipd->size;
					hand.base = NULL;
					hand.buf = classpath[i].u.zipf.mmap_base + zipd->filestart;
					goto okay;
#else
					fp = open(classpath[i].path, O_RDONLY|O_BINARY);
					lseek(fp, zipd->filestart, SEEK_SET);
					hand.size = zipd->size;
					goto found;
#endif
				}
			}
			break;

		case CP_DIR:
			strcpy(buf, classpath[i].path);
			strcat(buf, DIRSEP);
			strcat(buf, cname);
FDBG(			printf("Opening java file %s for %s\n", buf, cname); )
			fp = open(buf, O_RDONLY|O_BINARY);
			if (fp < 0 || fstat(fp, &sbuf) < 0) {
				break;
			}
			hand.size = sbuf.st_size;

			found:;
			hand.base = gc_malloc_fixed(hand.size);
			hand.buf = hand.base;

			i = 0;
			while (i < hand.size) {
				j = read(fp, hand.buf, hand.size - i);
				if (j >= 0) {
					i += j;
				}
				else if (!(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
					throwException(IOException("failed to read JAR data"));
				}
			}
			close(fp);
			goto okay;

		case CP_SOFILE:
			if (classpath[i].u.sof.loaded == 0) {
				if (loadNativeLibrary(classpath[i].path) < 0) {
					break;
				}
				classpath[i].u.sof.loaded = 1;
			}
			strcat(buf, CLASSSYMPREFIX);
			strcat(buf, cname);
			hand.base = loadNativeLibrarySym(buf);
			if (hand.base == NULL) {
				break;
			}
			goto okay;

		/* Ignore bad entries */
		default:
			break;
		}
	}
	/* If we call out the loop then we didn't find anything */
	hand.type = CP_INVALID;

	okay:;
	unlockMutex(&jarlock);

	return (hand);
}

/*
 * Initialise class path.
 */
void
initClasspath(void)
{
	char* cp;
	int i;

	/* Work on a copy of realClassPath.  The original will be
           necessary when initializing system properties. */
	cp = gc_malloc_fixed(strlen(realClassPath) + 1);
	strcpy(cp, realClassPath);

PDBG(	printf("initClasspath(): '%s'\n", cp);				)
	for (i = 0; cp != 0 && i < MAXPATHELEM; i++) {
		classpath[i].path = cp;
		cp = strchr(cp, PATHSEP);
		if (cp != 0) {
			*cp = 0;
			cp++;
		}

		classpath[i].type = getClasspathType(classpath[i].path);

PDBG(		printf("path '%s' type %d\n", classpath[i].path, classpath[i].type); )
	}
	i++;
	classpath[i].path = 0;

	gc_free(cp);
}

/*
 * Work out what kind of thing this path points at.
 */
static
int
getClasspathType(char* path)
{
	int h;
	int c;
	char buf[2];
	struct stat sbuf;

	if (stat(path, &sbuf) < 0) {
		return (CP_INVALID);
	}

	if (S_ISDIR(sbuf.st_mode)) {
		return (CP_DIR);
	}

	h = open(path, O_RDONLY);
	if (h < 0) {
		return (CP_INVALID);
	}

	c = read(h, buf, sizeof(buf));
	close(h);
	if (c != sizeof(buf)) {
		return (CP_INVALID);
	}

	if (IS_ZIP(buf)) {
		return (CP_ZIPFILE);
	}

	/* We should work out some way to check this ... */
	return (CP_SOFILE);
#if 0
	return (CP_INVALID);
#endif
}
