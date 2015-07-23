/*
 * classMethod.h
 * Class, method and field tables.
 *
 * Copyright (c) 1996 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>, 1996.
 * Modified by MASS Laboratory, SNU, 1999.
 */

#ifndef __classmethod_h
#define __classmethod_h

#include "basic.h"
#include "access.h"
#include "gtypes.h"
#include "object.h"
#include "constants.h"
#include "access.h"
#include "itypes.h"

#ifdef CUSTOMIZATION
struct SpecializedMethodSet_t;
struct SpecializedMethod_t;
struct CallContext_t;
#endif /* CUSTOMIZATION */

#ifdef INLINE_CACHE
struct TypeCheckerSet_t;
#endif

#ifdef DYNAMIC_CHA
#include "translator/dynamic_cha.h"
#endif

#define	MAXMETHOD		64

/* Class state */
#define	CSTATE_UNLOADED		0
#define	CSTATE_LOADED		1
#define	CSTATE_PRELOADED	2
#define CSTATE_DOING_PREPARE	3
#define CSTATE_PREPARED		4
#define	CSTATE_DOING_LINK	5
#define CSTATE_LINKED		6
#define	CSTATE_DOING_CONSTINIT	7
#define	CSTATE_CONSTINIT	8
#define	CSTATE_DOING_INIT	9
#define	CSTATE_OK		10
#define CSTATE_ERROR            11

// For class inclusion test
typedef struct _TypeRep {
    int32       pos;
    uint32*     row;
} TypeRep;

typedef struct Hjava_lang_ClassLoader {
    int	dummy;
} Hjava_lang_ClassLoader;

typedef struct Hjava_lang_Class {
    Hjava_lang_Object head;		/* A class is an object too */

    /* Used to chain classes in the same hash bucket of classPool. */
    struct Hjava_lang_Class *next;

    Utf8Const *name;
    accessFlags accflags;

    /* If non-NULL, a pointer to the superclass.
     * However, if state < CSTATE_DOING_PREPARE, then
     * (int) superclass is a constant pool index. */
    struct Hjava_lang_Class *superclass;

    /* Heads a list of all loaded subclasses and threads through
     * subclass_next.  This is used to update all dtable entries all
     * at once when resolving trampolines.  */
    struct Hjava_lang_Class *subclass_head;
    struct Hjava_lang_Class *subclass_next;

    struct _constants constants;

#ifdef INTERPRETER
    void *resolved; /* Pool of resolved constants. */
#endif

    /* For regular classes, an array of the methods defined in this class.
       For array types, used for CLASS_ELEMENT_TYPE.
       For primitive types, used by CLASS_ARRAY_CACHE. */
    Method *methods;
    short nmethods;

    /* Number of methods in the dtable. */
    /* If CLASS_IS_PRIMITIVE, then the CLASS_PRIM_SIG. */
    short msize;

    /* Pointer to array of Fields, on for each field.
       Static fields come first. */
    Field *fields;

    /* The size of the non-static fields, in bytes.
       For a primitive type, the length in bytes.
       Also used temporarily while reading fields.  */
    int	bfsize;

    /* Number of fields, including static fields. */
    short nfields;
    /* Number of static fields. */
    short nsfields;

    struct _dispatchTable *dtable;
    Method **methodsInDtable;

    struct Hjava_lang_Class **interfaces;
    Hjava_lang_ClassLoader *loader;
    short interface_len;
    char state;
    char final;
    
    /* For class inclusion test */
    TypeRep type_rep; 

    /* lock for class loading */
    int loadingLock;

    int ITsize;			/* interface table size */

    int refField;
    void (*walk)(void*);
    struct Hjava_lang_Class* array_cache;
} Hjava_lang_Class;


/**********************************************************
  Utility functions to manipulate the class structure
**********************************************************/

Utf8Const *Class_GetName(Hjava_lang_Class *class);
Hjava_lang_Class *Class_GetSuperclass(Hjava_lang_Class *class);
int Class_GetNumOfImplementedInterfaces(Hjava_lang_Class *class);
Hjava_lang_Class **Class_GetImplementedInterfaces(Hjava_lang_Class *class);
int Class_GetNumOfDefinedMethods(Hjava_lang_Class *class);
Method *Class_GetDefinedMethods(Hjava_lang_Class *class);
int8 Class_GetState(Hjava_lang_Class *class);
int Class_GetNumOfFields(Hjava_lang_Class *class);  // including static
Field *Class_GetFields(Hjava_lang_Class *class);
int Class_GetNumOfStaticFields(Hjava_lang_Class *class);
int Class_GetNumOfMethodsInDispatchTable(Hjava_lang_Class *class);
int Class_GetNumOfMethodsInInterfaceTable(Hjava_lang_Class *class);
// vitual table = dispatch table + interface table
struct _dispatchTable *Class_GetVirtualTable(Hjava_lang_Class *class);
struct _constants *Class_GetConstantPool(Hjava_lang_Class *class);
int Class_GetPrimarySignature(Hjava_lang_Class *class);
Hjava_lang_Class *Class_get_class_object(char *class_name);
Hjava_lang_Class *Class_get_primary_class(int prim_type);
byte Class_GetCPEntryType(Hjava_lang_Class *class, int cp_index);
word Class_GetCPEntryData(Hjava_lang_Class *class, int cp_index);

Hjava_lang_Class *Object_GetClass(Hjava_lang_Object *obj);
struct _dispatchTable *Object_GetVirtualTable(Hjava_lang_Object *obj);

bool Class_IsInterfaceType(Hjava_lang_Class* class);
bool Class_IsPrimitiveType(Hjava_lang_Class* class);
bool Class_IsArrayType(Hjava_lang_Class* class);
TypeRep* Class_GetTypeRep(Hjava_lang_Class* class);
Hjava_lang_Class* Class_GetArrayType(Hjava_lang_Class* class);
void Class_SetArrayType(Hjava_lang_Class* class, Hjava_lang_Class* array);
Hjava_lang_Class* Class_GetElementType(Hjava_lang_Class* class);
void Class_SetElementType(Hjava_lang_Class* class, Hjava_lang_Class* elem);
Hjava_lang_Class* Class_GetComponentType(Hjava_lang_Class* class);
void Class_SetComponentType(Hjava_lang_Class* class, Hjava_lang_Class* comp);
short Class_GetDimension(Hjava_lang_Class* class);
void Class_SetDimension(Hjava_lang_Class* class, short dim);
int Class_GetEncoding(Hjava_lang_Class* class);
void Class_SetEncoding(Hjava_lang_Class* class, int encoding);

#define METHOD_TRANSLATED(meth)		((meth)->accflags & ACC_TRANSLATED)
#define	METHOD_NATIVECODE(meth)		((meth)->ncode)

#define TRAMPOLINE_CODE_IN_DTABLE(meth)   ((meth)->ncode)
#define ADDR_OF_NATIVE_METHOD(meth)       ((meth)->ncode)

#define METHOD_HAVE_BRANCH	0x0001
#define METHOD_HAVE_RETURN	0x0002
#define METHOD_HAVE_NULLEH	0x0004
#define METHOD_HAVE_INVOKE	0x0008
#define METHOD_HAVE_UNRESOLVED	0x0010
#define METHOD_HAVE_ATHROW	0x0020

typedef struct _methods {
    Utf8Const *name;
    Utf8Const *signature;

    // resolved signature datas
    short in, out;
    char  rettype;

    accessFlags accflags;
    short       idx;	/* Index into class->dtable */
    u2          stacksz;
    u2          localsz;

    // ncode has trampoline code first 
    // next, it will changed to native code
    nativecode *ncode;

    // I think ncode is enough.

    unsigned char *bcode;
    int            bcodelen;

    Hjava_lang_Class    *class;
    struct _lineNumbers *lines;
    struct _jexception  *exception_table;

#ifdef INTERPRETER
    /* Support for the interpreter. */
    short		argdisp; /* Displacement to first argument. */
    void*		resolved; /* Pool of resolved constants. */

    /* Code for calling native code from interpreter. */
    void*		native_caller;
#endif /* INTERPRETER */


    int iidx;

#ifdef INLINE_CACHE
    // for call site information
    unsigned*		callSiteInfoTable; // array[bpc];
    struct TypeCheckerSet_t *      tcs; /* set of typecheck code */
#endif

#ifdef CUSTOMIZATION
    struct SpecializedMethodSet_t *sms; /* multiple instances of this method */
#ifdef INTERPRETER
    int count;
#endif /* INTERPRETER */

#else /* not CUSTOMIZATION */

#ifdef METHOD_COUNT
    int count;		/* Method run count */
    int tr_level;	/* Number of retranslations done */
#endif /* METHOD_COUNT */

#endif /* not CUSTOMIZATION */

    /* bytecode informations */
    uint8 bcodeInfo;
    uint8 *bcodeInfos;

#ifdef DYNAMIC_CHA
    bool        isOverridden;
    CallSiteSet* callSiteSet;
#endif
#ifdef VARIABLE_RET_COUNT
    int numOfBranches;
#endif /* VARIABLE_RET_COUNT */
} methods;

typedef struct _dispatchTable {
    Hjava_lang_Class*	class;
    uint32*             row;
    void*		method[1];
} dispatchTable;

typedef enum ReturnType {
    RT_NO_RETURN,
    RT_INTEGER,
    RT_LONG,
    RT_REFERENCE,
    RT_FLOAT,
    RT_DOUBLE
} ReturnType;

struct _jexceptionEntry;


/*******************************************************************
   Method manipulation functions
********************************************************************/ 
Utf8Const *Method_GetName(Method *method);
Utf8Const *Method_GetSignature(Method *method);
accessFlags Method_GetAccessFlags(Method *method);
int Method_GetDTableIndex(Method *method);
int Method_GetITableIndex(Method *method);
int Method_GetStackSize(Method *method);
int Method_GetLocalSize(Method *method);
nativecode *Method_GetNativeCode(Method *method);
void Method_SetNativeCode(Method *method, nativecode *ncode);
uint8 *Method_GetByteCode(Method *method);
uint8 *Method_GetReferredByteCode(Method *method, int offset);
int Method_GetByteCodeLen(Method *method);
Hjava_lang_Class *Method_GetDefiningClass(Method *method);
int Method_GetArgSize(Method *method);
ReturnType Method_GetRetType(Method *method);
struct _jexception *Method_GetExceptionTable(Method *method);
struct _jexceptionEntry *Method_GetExceptionHandler(Method *method, uint16 pc,
						    Hjava_lang_Class *e);
Hjava_lang_Class *Method_GetReferredClass(Method *method, int cp_index);
Method *Method_GetReferredMethod(Method *method, int cp_index);

bool Method_IsNative(Method *method);
bool Method_IsTranslated(Method *method);
bool Method_IsStatic(Method *method);
bool Method_IsSynchronized(Method *method);

#define METHOD_JOIN_POINT 0x0001
#define METHOD_WITHIN_TRY 0x0002

void Method_SetBcodeInfo(Method *method, uint8 *bcode);
uint8 *Method_GetBcodeInfo(Method *method);

bool Method_HaveBranch(Method *method);
void Method_SetHaveBranch(Method *method);
bool Method_HaveReturn(Method *method);
void Method_SetHaveReturn(Method *method);
bool Method_HaveNullException(Method *method);
void Method_SetHaveNullException(Method *method);
bool Method_HaveInvoke(Method *method);
void Method_SetHaveInvoke(Method *method);
bool Method_HaveUnresolved(Method *method);
void Method_SetHaveUnresolved(Method *method);
bool Method_HaveUnresolved(Method *method);
void Method_SetHaveAthrow(Method *method);
bool Method_HaveAthrow(Method *method);

#ifdef VARIABLE_RET_COUNT
void Method_IncreaseBranchNO(Method *method);
int Method_GetBranchNO(Method *method);
#endif /* VARIABLE_RET_COUNT */

#define	CLASS_STATE		((int)&(((Hjava_lang_Class*)0)->state))
#define	DTABLE_CLASS		0
#define	DTABLE_METHODOFFSET	((int)&(((dispatchTable*)0)->method[0]))
#define DTABLE_METHODINDEX      DTABLE_METHODOFFSET/(sizeof(unsigned))
#define	DTABLE_METHODSIZE	(sizeof(void*))
#define DTABLE_ROWOFFSET        ((int)&(((dispatchTable*)0)->row))
#define ARRAY_ENCODINGOFFSET    ((int)&(((Array*)0)->encoding))

typedef struct _fields {
    Utf8Const*		name;
    /* The type of the field, if FIELD_RESOLVED.
       If !FIELD_RESOLVED:  The fields signature as a (Utf8Const*). */
    Hjava_lang_Class*	type;
    accessFlags		accflags;
    u2			bsize;		/* in bytes */
    union {
        int		boffset;	/* offset in bytes (object) */
        void*		addr;		/* address (static) */
        u2		idx;		/* constant value index */
    } info;
} fields;

#define FIELD_UNRESOLVED_FLAG	0x8000
#define	FIELD_CONSTANT_VALUE	0x4000

#define FIELD_RESOLVED(FLD)	(! ((FLD)->accflags & FIELD_UNRESOLVED_FLAG))

/* Type of field FLD.  Only valid if FIELD_RESOLVED(FLD). */
#define FIELD_TYPE(FLD)		((FLD)->type)
/* Size of field FLD, in bytes. */
#define FIELD_SIZE(FLD)		((FLD)->bsize)
#define FIELD_WSIZE(FLD)	((FLD)->bsize <= sizeof(jint) ? 1 : 2)
#define FIELD_OFFSET(FLD)	((FLD)->info.boffset)
#define FIELD_ADDRESS(FLD)	((FLD)->info.addr)
#define	FIELD_CONSTIDX(FLD)	((FLD)->info.idx)
#define FIELD_ISREF(FLD)	(!Class_IsPrimitiveType(FIELD_TYPE(FLD)))

/****************************************************************************
     FIELD MANIPULATION FUNCTIONS
*****************************************************************************/
Utf8Const *Field_GetName(Field *field);
Hjava_lang_Class *Field_GetType(Field *field);
accessFlags Field_GetAccessFlags(Field *field);
int Field_GetSize(Field *field); // in bytes
int Field_GetOffset(Field *field); // for non-static fields
void *Field_GetStaticAddr(Field *field); // only for static fields
bool Field_IsReference(Field *field);
bool Field_IsPrimary(Field *field);

#define	CLASSMAXSIG		256

struct _Code;
struct _method_info;
struct _field_info;
struct _classFile;

#define CLASS_METHODS(CLASS)  ((CLASS)->methods)
#define CLASS_NMETHODS(CLASS)  ((CLASS)->nmethods)

/* An array containing all the Fields, static fields first. */
#define CLASS_FIELDS(CLASS)   ((CLASS)->fields)

/* An array containing all the static Fields. */
#define CLASS_SFIELDS(CLASS)  ((CLASS)->fields)

/* An array containing all the instance (non-static) Fields. */
#define CLASS_IFIELDS(CL)     (&(CL)->fields[CLASS_NSFIELDS(CL)])

/* Total number of fields (instance and static). */
#define CLASS_NFIELDS(CLASS)  ((CLASS)->nfields)
/* Number of instance (non-static) fields. */
#define CLASS_NIFIELDS(CLASS) ((CLASS)->nfields - (CLASS)->nsfields)
/* Number of static fields. */
#define CLASS_NSFIELDS(CLASS) ((CLASS)->nsfields)

/* Size of a class fields (not counting header), in words. */
#define CLASS_WFSIZE(CLASS)   ((CLASS)->bfsize / sizeof(jint))

/* Size of a class fields (not counting header), in bytes. */
#define CLASS_FSIZE(CLASS)    ((CLASS)->bfsize)

#define OBJECT_CLASS(OBJ)     ((OBJ)->dtable->class)

#define CLASS_CNAME(CL)  ((CL)->name->data)
#define _PRIMITIVE_DTABLE ((struct _dispatchTable*)(-1))
// #define CLASS_IS_PRIMITIVE(CL) ((CL)->dtable == _PRIMITIVE_DTABLE)

/* Assuming CLASS_IS_PRIMITIVE(CL), return the 1-letter signature code. */
#define CLASS_PRIM_SIG(CL) ((CL)->msize)

// Now these macros are replaced by inline function in classMethod.ic
// - walker.
// #define CLASS_IS_ARRAY(CL)  (CLASS_CNAME(CL)[0] == '[')

// /* For an array type, the types of the components. */
// #define CLASS_COMPONENT_TYPE(ARRAYCLASS) (*(Hjava_lang_Class**)&(ARRAYCLASS)->methods)
// /* For an array type, the types of the elements. */
// #define CLASS_ELEMENT_TYPE(ARRAYCLASS) (*(Hjava_lang_Class**)&(ARRAYCLASS)->fields)

// /* Used by the lookupArray function. */
// //#define CLASS_ARRAY_CACHE(PRIMTYPE) (*(Hjava_lang_Class**)&(PRIMTYPE)->methods)
// #define CLASS_ARRAY_CACHE(PRIMTYPE) ((PRIMTYPE)->array_cache)

#define TYPE_PRIM_SIZE(CL) ((CL)->bfsize)
#define TYPE_SIZE(CL) \
  (Class_IsPrimitiveType(CL) ? TYPE_PRIM_SIZE (CL) : PTR_TYPE_SIZE)



/*
  *'processClass' is the core of the class initialiser and can prepare a
  *class from the cradle to the grave.
 */
void			processClass(Hjava_lang_Class*, int);

Hjava_lang_Class*	loadClass(Utf8Const*, Hjava_lang_ClassLoader*);

void			loadStaticClass(Hjava_lang_Class*, char*);

Hjava_lang_Class*	addClass(Hjava_lang_Class*, constIndex, constIndex, u2, Hjava_lang_ClassLoader*);
Method*			addMethod(Hjava_lang_Class*, struct _method_info*);
void 			addMethodCode(Method*, struct _Code*);
Field *       		addField(Hjava_lang_Class*, struct _field_info*);
void			addCode(Method*, uint32, struct _classFile*);
void			addInterfaces(Hjava_lang_Class*, int, Hjava_lang_Class**);
void			setFieldValue(Field*, u2);

Hjava_lang_Class*	lookupClass(char*);
Hjava_lang_Class*	lookupArray(Hjava_lang_Class*);
Hjava_lang_Class*	lookupObjectArrayClass(Hjava_lang_Class*);
Field*			lookupClassField(Hjava_lang_Class*, Utf8Const*, bool);

Hjava_lang_Class*	getClass(constIndex, Hjava_lang_Class*);

void			countInsAndOuts(char*, short*, short*, char*);
int			sizeofSig(char**, bool);
int			sizeofSigItem(char**, bool);
void			establishMethod(Method*);
Hjava_lang_Class*	classFromSig(char**, Hjava_lang_ClassLoader*);
void			finishFields(Hjava_lang_Class*);
struct MethodInstance*	findMethodFromPC(uintp);
void			registerMethod(uintp, uintp, void *);
void                    registerTypeCheckCode(uintp start_pc, uintp end_pc);
Utf8Const*		makeUtf8Const(char*, int);




void            fixupDispatchTable(Hjava_lang_Class *c,
                                    nativecode *ocode,
                                    nativecode *ncode,
                                    int idx);

#if defined(NEW_INTERFACETABLE)
void fixupInterfaceTable (Hjava_lang_Class *c, nativecode *ocode, 
                          nativecode *ncode, int iidx);
#else /* not defined(NEW_INTERFACETABLE) */
void fixupInterfaceTable (Hjava_lang_Class *c, nativecode *ocode,
                          nativecode *ncode, Method *m);
#endif /* not defined(NEW_INTERFACETABLE) */

void    fixupDtableItable(Method *meth, Hjava_lang_Class *class, void *ncode);

Method *get_method_from_class_and_index(Hjava_lang_Class *class, int idx);

void registerClass(Hjava_lang_Class *cl);

extern Utf8Const *init_name;		/* "<clinit>" */
extern Utf8Const *constructor_name;	/* "<init>" */
extern Utf8Const *final_name;		/* "finalize" */
extern Utf8Const *void_signature;	/* "()V" */
extern Utf8Const *Code_name;		/* "Code" */
extern Utf8Const *LineNumberTable_name;	/* "LineNumberTable" */
extern Utf8Const *ConstantValue_name;	/* "ConstantValue" */

extern Utf8Const *null_pointer_exception_name;
extern Utf8Const *array_index_out_of_bounds_exception_name;
extern Utf8Const *negative_array_size_exception_name;
extern Utf8Const *array_store_exception_name;
extern Utf8Const *arithmetic_exception_name;
extern Utf8Const *object_class_name;
extern Utf8Const *throwable_class_name;
extern Utf8Const *exception_class_name;
extern Utf8Const *runtime_exception_class_name;
extern Utf8Const *index_out_of_bounds_exception_name;

#include "classMethod.ic"

#endif
