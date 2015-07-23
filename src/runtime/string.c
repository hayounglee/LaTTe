/*
 * string.c
 * Manage the various forms of strings, Utf8 and Unicode.
 *
 * Copyright (c) 1996 Cygnus Support
 * Copyright (c) 1996,97 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Per Bothner <bothner@cygnus.com>
 * Written by Tim Wilkinson <tim@tjwassoc.co.uk>
 */

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "jtypes.h"
#include "constants.h"
#include "object.h"
#include "itypes.h"
#include "locks.h"
#include "classMethod.h"
#include "baseClasses.h"

int32 hashChars (jchar* ptr, int length);

#if INTERN_UTF8CONSTS

static Utf8Const** utf8hash = NULL;
static int utf8hash_size = 0;  /* Number of slots available in utf8hash.
			        * Assumed be power of 2! */
static int utf8hash_count = 0;  /* Number of slots used in utf8hash. */
#define HASH_CHARS(STR, LEN) hashChars(STR, LEN)

#else

/* To be compatible with Utf8Const 16-bit hash values. */
#define HASH_CHARS(STR, LEN) (uint16) hashChars(STR, LEN)

#endif


static Hjava_lang_String** strhash = NULL;
static int strhash_count = 0;  /* Number of slots used in strhash. */
static int strhash_size = 0;  /* Number of slots available in strhash.
			       * Assumed be power of 2! */

#define DELETED_STRING ((Hjava_lang_String*)(~0))
#define SET_STRING_IS_INTERNED(STR) \
		gc_set_finalizer((STR), &gc_finalizing_string)

static int strLengthUtf8 (char* str, int len);
Hjava_lang_String** findInternSlot(jchar*, int, int);
void rehashStrings(void);


/* Count the number of Unicode chars encoded in a given Ut8 string. */
inline static
int
strLengthUtf8(char* str, int len)
{
	register unsigned char* ptr;
	register unsigned char* limit;
	int str_length;

	ptr = (unsigned char*) str;
	limit = ptr + len;
	str_length = 0;
	for (; ptr < limit; str_length++) {
		if (UTF8_GET (ptr, limit) < 0) {
			return (-1);
		}
	}
	return (str_length);
}

/* Calculate a hash value for a string encoded in Utf8 format.
 * This returns the same hash value as specified or java.lang.String.hashCode.
 */
int32
hashUtf8String (char* str, int len)
{
    //
    // Why should this function return the same hash value as
    // java.lang.String.hashCode?
    //
    int    i;
    int32  hash = 0;

    for ( i = 0; i < len; i++ ) {
        hash = ( hash * 37 ) + str[ i ];
    }

    return hash;
}

void 
finalizeUtf8const(void* mem){
    unsigned int hash;
	register unsigned int index;
	int step;
	Utf8Const* m;
    int len;

    m = (Utf8Const*)mem;    
    len = m->length;
    hash = hashUtf8String( m->data, len);
    index = hash % utf8hash_size;
    step = 0;

    while (utf8hash[index] == (Utf8Const *) -1 ||
           !( utf8hash[ index ]->length == len &&
              memcmp( utf8hash[ index ]->data, m->data, len ) == 0 ) )  {
        index += 2 * (++step) - 1; // use quadratic probing
        index %= utf8hash_size;
    }
    assert(utf8hash[index] != NULL);
    utf8hash_count--;
    utf8hash[index] = (Utf8Const *) -1;
}

  
#if INTERN_UTF8CONSTS
/* Find the correct slot in utf8hash for the given string. */
static
Utf8Const**
find_utf8hash (char* s, int len)
{	
	unsigned int hash;
	register unsigned int index;
	int step;


    hash = hashUtf8String( s, len );
    index = hash % utf8hash_size;
    step = 0;

    while ( utf8hash[ index ] != NULL && (utf8hash[index] == (Utf8Const *) -1 ||
            !( utf8hash[ index ]->length == len &&
               memcmp( utf8hash[ index ]->data, s, len ) == 0 ) )) {
        index += 2 * (++step) - 1; // use quadratic probing
        index %= utf8hash_size;
    }

    return &utf8hash[ index ];

}
#endif

#include "prime.h"

Utf8Const*
makeUtf8Const (char* s, int len)
{
	Utf8Const* m;
#if INTERN_UTF8CONSTS
	Utf8Const** ptr;
#endif
	if (len < 0) {
		len = strlen (s);
	}
#if INTERN_UTF8CONSTS

    if (3 * utf8hash_count >= 2 * utf8hash_size) { // load factor is 2/3.

		if (utf8hash == NULL) {
			utf8hash_size = 1021;

			utf8hash =
				gc_calloc(utf8hash_size, sizeof(Utf8Const*),
					  &gc_fixed);
		}
		else {	/* Re-hash */
			register int i = utf8hash_size;
			register Utf8Const** ptr = utf8hash + i;
			register Utf8Const** old_hash = utf8hash;
			utf8hash_size = get_next_prime( utf8hash_size * 2 );
			utf8hash = gc_calloc( utf8hash_size,
					      sizeof( Utf8Const* ),
					      &gc_fixed );

			while (--i >= 0) {
//				if (*--ptr != NULL) {
				if (*--ptr > 0) {
					*find_utf8hash((*ptr)->data,
						       (*ptr)->length) = *ptr;
				}
			}
			gc_free(old_hash);
		}
	}
	ptr = find_utf8hash (s, len);
	if (*ptr != 0) {
		return (*ptr);
	}
#endif
	m = (Utf8Const*)gc_malloc(sizeof(Utf8Const) + len + 1, &gc_utf8const);
	memcpy (m->data, s, len);
	m->data[len] = 0;
	m->length = len;
#if INTERN_UTF8CONSTS
	*ptr = m;
	utf8hash_count++;
#else
	m->hash = (uint16) hashUtf8String (s, len);
#endif
	return (m);
}

Hjava_lang_String*
makeReplaceJavaStringFromUtf8(unsigned char* ptr, int len, int from_ch, int to_ch)
{
	Hjava_lang_String* obj;
	register unsigned char* limit;
	jchar* chrs;
	int ch;

	limit = ptr + len;
	obj = (Hjava_lang_String*)newObject(StringClass);
	obj->data[0].count = strLengthUtf8 (ptr, len);
	obj->data[0].value =
	    (HArrayOfChar*)newPrimArray(TYPE_CLASS(TYPE_Char), len);
	obj->data[0].offset = 0;
	chrs = STRING_DATA(obj);

	for (; ptr < limit;) {
		ch = UTF8_GET (ptr, limit);
		if (ch == from_ch) {
			ch = to_ch;
		}
		*chrs++ = ch;
	}

	return (obj);
}

Hjava_lang_String*
Utf8Const2JavaString(Utf8Const* str)
{
	Hjava_lang_String* obj;
	jchar *chrs;
	jchar buffer[100];
	Hjava_lang_Object* array;
	int hash;
	register unsigned char* data;
	register unsigned char* limit;
	int length;
	Hjava_lang_String** ptr;

	data = str->data;
	limit = data + str->length;
	length = strLengthUtf8(data, str->length);

	if (length <= (sizeof(buffer) / sizeof(jchar))) {
		chrs = buffer;
		array = NULL;
	}
	else {
		array = newPrimArray(charClass, length);
		chrs = (jchar*)ARRAY_DATA(array);
	}

	while (data < limit) {
		*chrs++ = UTF8_GET(data, limit);
        }
	chrs -= length;

	if (4 * strhash_count >= 3 * strhash_size) {
		rehashStrings();
	}
#if INTERN_UTF8CONSTS
	hash = HASH_CHARS(chrs, length);
#else
	hash = str->hash;
#endif
	ptr = findInternSlot (chrs, length, hash);
	if (*ptr != NULL && *ptr != DELETED_STRING) {
		return *ptr;
	}
	strhash_count++;
	if (array == NULL) {
		array = newPrimArray(charClass, length);
		chrs = (jchar*) ARRAY_DATA(array);
		memcpy (chrs, buffer, sizeof(jchar)*length);
	}
	obj = (Hjava_lang_String*)newObject(StringClass);
	obj->data[0].count = length;
	obj->data[0].value = (HArrayOfChar*)array;
	obj->data[0].offset = 0;
	*ptr = obj;
	SET_STRING_IS_INTERNED(obj);
	return (obj);
}

/* Return true iff a Utf8Const string is equal to a Java String. */

int
equalUtf8JavaStrings(Utf8Const* a, Hjava_lang_String* b)
{
	jchar* data = STRING_DATA(b);
	register unsigned char* ptr = a->data;
        register unsigned char* limit = ptr + a->length;
	int len = STRING_SIZE(b);
	if (len != a->length)
		return 0;
	while (--len >= 0) {
		if (*data++ != UTF8_GET(ptr, limit))
			return 0;
	}
	return 1;
}

/* Find a slot where the string with elements DATA, length LEN,
   and hash HASH should go in the strhash table of interned strings. */
Hjava_lang_String**
findInternSlot (jchar* data, int len, int hash)
{
	int start_index = hash & (strhash_size - 1);
	int deleted_index = -1;
	
	register int index = start_index;
	/* step must be non-zero, and relatively prime with strhash_size. */
	int step = 8 * hash + 7;
	for (;;) {
		register Hjava_lang_String** ptr = &strhash[index];
		if (*ptr == NULL) {
			if (deleted_index >= 0) {
				return (&strhash[deleted_index]);
			}
			else {
				return (ptr);
			}
		}
		else if (*ptr == DELETED_STRING) {
			deleted_index = index;
		}
		else if (STRING_SIZE(*ptr) == len
			 && memcmp(STRING_DATA(*ptr), data, 2*len) == 0) {
			return (ptr);
		}
		index = (index + step) & (strhash_size - 1);
		if (index == start_index) {
			ABORT();
		}
	}
}

/* Calculate a hash code for the string starting at PTR at given LENGTH.
   This uses the same formula as specified for java.lang.String.hash. */

int32
hashChars (jchar* ptr, int length)
{
	register jchar* limit = ptr + length;
       int32 hash = *ptr;
	if (length <= 15) {
               while (++ptr < limit) {
                       hash = (37 * hash) + *ptr;
		}
	}
	else {
               int skip = length / 8;
               while ((ptr += skip) < limit) {
                       hash = (39 * hash) + *ptr;
		}
	}
	return (hash);
}

Hjava_lang_String**
findInternSlotFromString (Hjava_lang_String* str)
{
	jchar* data = STRING_DATA(str);
	int length = STRING_SIZE(str);
	return (findInternSlot(data, length, HASH_CHARS (data, length)));
}

void
rehashStrings(void)
{
	if (strhash == NULL) {
		strhash_size = 1024;
		/* Note we do *not* want the GC scanning strhash,
		   so we use gc_calloc_fixed instead of gc_malloc. */
		strhash = gc_calloc_fixed(strhash_size, sizeof(Hjava_lang_String*));
	}
	else {
		register int i;
		register Hjava_lang_String** ptr;

		i = strhash_size;
		ptr = strhash + i;
		strhash_size *= 2;
		strhash = gc_calloc_fixed(strhash_size, sizeof (Hjava_lang_String*));

		while (--i >= 0) {
			int hash;
			int index;
			int step;

			--ptr;
			if (*ptr == NULL || *ptr == DELETED_STRING) {
				continue;
			}
			/* This is faster equivalent of
			 * *findInternSlotFromString (*ptr) = *ptr; */
			hash = HASH_CHARS(STRING_DATA(*ptr),STRING_SIZE(*ptr));
			index = hash & (strhash_size - 1);
			step = 8 * hash + 7;
			for (;;) {
				if (strhash[index] == NULL) {
					strhash[index] = *ptr;
					break;
				}
				index = (index + step) & (strhash_size - 1);
			}
		}
		gc_free_fixed(ptr); /* Old value of strhash. */
	}
}

Hjava_lang_String*
internJavaString(Hjava_lang_String* str)
{
	Hjava_lang_String** ptr;

	if (4 * strhash_count >= 3 * strhash_size) {
		rehashStrings();
	}
	ptr = findInternSlotFromString (str);
	if (*ptr != NULL && *ptr != DELETED_STRING) {
		return *ptr;
	}
	SET_STRING_IS_INTERNED(str);
	strhash_count++;
	*ptr = str;
	return (str);
}

/* Called by String fake finalizer. */
void
uninternJavaString(Hjava_lang_String* str)
{
	Hjava_lang_String** ptr = findInternSlotFromString(str);
	if (*ptr == NULL || *ptr == DELETED_STRING) {
		return;
	}
	*ptr = DELETED_STRING;
	strhash_count--;
}

/*
 * Convert an Java string to a C string.
 */
char*
javaString2CString(struct Hjava_lang_String* js, char* cs, int len)
{
	jchar* chrs;

	if (len <= 0) {
		cs = NULL;
	}
	else if (js == NULL) {
		cs[0] = 0;
	}
	else {
		chrs = STRING_DATA(js);
		len--;
		if (len > STRING_SIZE(js)) {
			len = STRING_SIZE(js);
		}
		cs[len] = 0;
		while (--len >= 0) {
			*cs++ = (char)*chrs++;
		}
	}
	return (cs);
}

/*
 * Convert a Java string into a malloced C string buffer.
 */
char*
makeCString(Hjava_lang_String* js)
{
	char* str;

	str = gc_malloc_fixed(STRING_SIZE(js) + 1);
	if (str != 0) {
		javaString2CString(js, str, STRING_SIZE(js) + 1);
	}
	return (str);
}

/*
 * Convert a C string into a Java String.
 */
Hjava_lang_String*
makeJavaString(char* cs, int len)
{
	Hjava_lang_String* obj;
	jchar* chrs;

	obj = (Hjava_lang_String*)newObject(StringClass);
	/* FIXME - should intern string literals */
	obj->data[0].count = len;
	obj->data[0].value =
	    (HArrayOfChar*)newPrimArray(TYPE_CLASS(TYPE_Char), len);
	obj->data[0].offset = 0;
	chrs = STRING_DATA(obj);

	while (--len >= 0) {
		*chrs++ = *(unsigned char*)cs++;
	}
	return (obj);
}


/*
 * Convert a C string into a Java char array.
 */
Hjava_lang_Object*
makeJavaCharArray(char* cs, int len)
{
	Hjava_lang_Object* obj;
	int i;

	obj = newPrimArray(TYPE_CLASS(TYPE_Char), len);
	if (cs != NULL) {
		jchar *ptr = (jchar*) ARRAY_DATA(obj);
		for (i = 0;  i < len;  i++) {
			*ptr++ = *(unsigned char*)cs++;
		}
	}
	return (obj);
}

/*
 * Finalize an interned string.
 */
void
gc_finalize_string(void* mem)
{
	Hjava_lang_String* str;

	str = (Hjava_lang_String*)mem;

	assert(OBJECT_CLASS(&str->base) == StringClass);
	uninternJavaString(str);
}

#if !defined(HAVE_STRDUP)
/*
 * Some systems don't support strdup.
 */
char*
strdup(char* str)
{
	char* s;

	if (str == NULL) {
		return (NULL);
	}
	s = gc_malloc_fixed(strlen(str) + 1);
	strcpy(s, str);
	return (s);
}
#endif
