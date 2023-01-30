
#pragma once
/*
SPDX-License-Identifier: MIT

ape
https://github.com/kgabis/ape
Copyright (c) 2021 Krzysztof Gabis

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifdef _MSC_VER
    #ifndef _CRT_SECURE_NO_WARNINGS
        #define _CRT_SECURE_NO_WARNINGS
    #endif /* _CRT_SECURE_NO_WARNINGS */
#endif /* _MSC_VER */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>

#if defined(__linux__)
    #define APE_LINUX
    #define APE_POSIX
#elif defined(_WIN32)
    #define APE_WINDOWS
#elif(defined(__APPLE__) && defined(__MACH__))
    #define APE_APPLE
    #define APE_POSIX
#elif defined(__EMSCRIPTEN__)
    #define APE_EMSCRIPTEN
#endif

#if defined(__unix__)
    #include <unistd.h>
    #if defined(_POSIX_VERSION)
        #define APE_POSIX
    #endif
#endif

#if defined(__STRICT_ANSI__)
    #define APE_CCENV_ANSIMODE 1
#endif

#if defined(APE_CCENV_ANSIMODE)
    #if defined(__GNUC__)
        #define APE_INLINE __inline__ __attribute__((__gnu_inline__))
    #else
        #define APE_INLINE
    #endif
#else
    #define APE_INLINE inline
#endif

#define APE_CONF_INVALID_VALDICT_IX UINT_MAX
#define APE_CONF_INVALID_STRDICT_IX UINT_MAX

#define APE_CONF_PLAINLIST_CAPACITY_ADD 1

#define APE_CONF_SIZE_VM_THISSTACK (512 / 4)

#define APE_CONF_SIZE_NATFN_MAXDATALEN (16 * 2)
#define APE_CONF_SIZE_STRING_BUFSIZE (32)

#define APE_CONF_SIZE_ERRORS_MAXCOUNT (4)
#define APE_CONF_SIZE_ERROR_MAXMSGLENGTH (80)

#define APE_STREQ(a, b) (strcmp((a), (b)) == 0)
#define APE_STRNEQ(a, b, n) (strncmp((a), (b), (n)) == 0)
#define APE_ARRAY_LEN(array) ((int)(sizeof(array) / sizeof(array[0])))

#define APE_DEBUG 0

#if 1
    #define APE_DBLEQ(a, b) (fabs((a) - (b)) < DBL_EPSILON)
#else
    #define APE_DBLEQ(a, b) ((a) == (b))
#endif

#define APE_CALL(vm, function_name, ...) \
    vm_call( \
        vm, \
        (function_name), \
        sizeof((ApeObject[]){__VA_ARGS__}) / sizeof(ApeObject), \
        (ApeObject[]){ __VA_ARGS__ })

#define APE_ASSERT(x) assert((x))

/*
* get type of object
*/
#if 1
    #define ape_object_value_type(obj) \
        ( \
            ((obj).handle == NULL) ? \
            ((obj).type) : \
            ((ApeObjType)((obj).handle->datatype)) \
        )
#else
    #define ape_object_value_type(obj) (obj).type
#endif

/*
* helper to check object type to another
*/
#define ape_object_value_istype(o, t) \
    (ape_object_value_type(o) == (t))

/*
* is this object a number?
*/
#define ape_object_type_isnumber(t) \
    ( \
        (t == APE_OBJECT_FIXEDNUMBER) || \
        (t == APE_OBJECT_FLOATNUMBER) \
    )
#define ape_object_value_isnumber(o) \
    ape_object_type_isnumber(ape_object_value_type(o))

/*
* is this object number-like?
*/
#define ape_object_type_isnumeric(t) \
    ( \
        ((t) == APE_OBJECT_FIXEDNUMBER) || \
        ((t) == APE_OBJECT_FLOATNUMBER) || \
        ((t) == APE_OBJECT_BOOL) \
    )

#define ape_object_value_isnumeric(o) \
    (ape_object_type_isnumeric(ape_object_value_type(o)))

#define ape_object_value_isfixednumber(o) \
    (ape_object_value_type(o) == APE_OBJECT_FIXEDNUMBER)

#define ape_object_value_isfloatnumber(o) \
    (ape_object_value_type(o) == APE_OBJECT_FLOATNUMBER)
    

/*
* is this object null?
*/
#define ape_object_value_isnull(o) \
    ( \
        ((o).type == APE_OBJECT_NONE) || \
        ape_object_value_istype(o, APE_OBJECT_NULL) \
    )

/*
* is this object a string?
*/
#define ape_object_value_isstring(o) \
    ape_object_value_istype(o, APE_OBJECT_STRING)

/*
* is this object an array?
*/
#define ape_object_value_isarray(o) \
    ape_object_value_istype(o, APE_OBJECT_ARRAY)

/*
* is this object something that can be called like a function?
*/
#define ape_object_value_iscallable(o) \
    ( \
        ape_object_value_istype(o, APE_OBJECT_NATIVEFUNCTION) || \
        ape_object_value_istype(o, APE_OBJECT_SCRIPTFUNCTION) \
    )


#define _ape_if(x) (x) ?
#define _ape_then(x) (x) :
#define _ape_else(x) (x)


/*
* get boolean value from this object
*/
#define ape_object_value_asbool(obj) \
    ((obj).valbool)

/*
* get number value from this object
*/

#define ape_object_value_asfixednumber(obj) \
    ((obj).valfixednum)

#define ape_object_value_asfloatnumber(obj) \
    ((obj).valfloatnum)

#define ape_object_value_asnumber(obj) \
    ( \
        _ape_if(ape_object_value_type(obj) == APE_OBJECT_FLOATNUMBER) \
        _ape_then(ape_object_value_asfloatnumber(obj)) \
        _ape_else( \
            _ape_if(ape_object_value_type(obj) == APE_OBJECT_FIXEDNUMBER) \
            _ape_then(ape_object_value_asfixednumber(obj)) \
            _ape_else( \
                _ape_if(ape_object_value_type(obj) == APE_OBJECT_BOOL) \
                _ape_then((obj).valbool) \
                _ape_else(0) \
            ) \
        ) \
    )

/*
* get internal GC data from this object. may be NULL!
*/
#define ape_object_value_allocated_data(obj) \
    ((obj).handle)

/*
* is this object using allocated data?
* assumes everything is allocated. not ideal, tbh.
*/
#define ape_object_value_isallocated(o) \
    (true)

/*
* get script function value from this object
*/
#define ape_object_value_asscriptfunction(obj) \
    (&(ape_object_value_allocated_data((obj))->valscriptfunc))

/*
* get native function value from this object.
* does *not* return plain function pointer, but an object.
* best not to attempt to call directly, though.
*/
#define ape_object_value_asnativefunction(obj) \
    (&(ape_object_value_allocated_data((obj))->valnatfunc))

/*
* get external data value from this object.
*/
#define ape_object_value_asexternal(obj) \
    (&(ape_object_value_allocated_data((obj))->valextern))

/*
* get gc memory handle from this object.
* may be NULL! 
*/
#define ape_object_value_getmem(obj) \
    (ape_object_value_allocated_data(obj)->mem)


//#if defined(APE_DEBUG)
#if 1
    #define ape_allocator_alloc(alc, sz) \
        ape_allocator_alloc_real(alc, #sz, __FUNCTION__, __FILE__, __LINE__, sz)

    #define ape_allocator_realloc(alc, ptr, oldsz, newsz) \
        ape_allocator_realloc_real(alc, #newsz, __FUNCTION__, __FILE__, __LINE__, ptr, oldsz, newsz)
#else
    #define ape_allocator_alloc(alc, sz) \
        ape_allocator_alloc_real(alc, sz)
#endif


enum ApeErrorType
{
    APE_ERROR_NONE = 0,
    APE_ERROR_PARSING,
    APE_ERROR_COMPILATION,
    APE_ERROR_RUNTIME,
    APE_ERROR_TIMEOUT,
    APE_ERROR_ALLOCATION,
    /* from ape_add_error() or ape_add_errorf() */
    APE_ERROR_USER,
};

enum ApeObjType
{
    APE_OBJECT_NONE = 0,
    APE_OBJECT_ERROR = 1 << 0,
    APE_OBJECT_FLOATNUMBER = 1 << 1,
    APE_OBJECT_FIXEDNUMBER = 1 << 2,
    APE_OBJECT_NUMERIC = APE_OBJECT_FLOATNUMBER | APE_OBJECT_FIXEDNUMBER,
    APE_OBJECT_BOOL = 1 << 3,
    APE_OBJECT_STRING = 1 << 4,
    APE_OBJECT_NULL = 1 << 5,
    APE_OBJECT_NATIVEFUNCTION = 1 << 6,
    APE_OBJECT_ARRAY = 1 << 7,
    APE_OBJECT_MAP = 1 << 8,
    APE_OBJECT_SCRIPTFUNCTION = 1 << 9,
    APE_OBJECT_EXTERNAL = 1 << 10,
    APE_OBJECT_FREED = 1 << 11,
    /* for checking types with & */
    APE_OBJECT_ANY = 0xffff,
};

enum ApeAstTokType
{
    TOKEN_INVALID = 0,
    TOKEN_EOF,

    /* Operators */
    TOKEN_ASSIGN,
    TOKEN_ASSIGNPLUS,
    TOKEN_ASSIGNMINUS,
    TOKEN_ASSIGNSTAR,
    TOKEN_ASSIGNSLASH,
    TOKEN_ASSIGNMODULO,
    TOKEN_ASSIGNBITAND,
    TOKEN_ASSIGNBITOR,
    TOKEN_ASSIGNBITXOR,
    TOKEN_ASSIGNLEFTSHIFT,
    TOKEN_ASSIGNRIGHTSHIFT,
    TOKEN_OPQUESTION,
    TOKEN_OPPLUS,
    TOKEN_OPINCREASE,
    TOKEN_OPMINUS,
    TOKEN_OPDECREASE,
    TOKEN_OPNOT,
    TOKEN_OPSTAR,
    TOKEN_OPSLASH,
    TOKEN_OPLESSTHAN,
    TOKEN_OPLESSEQUAL,
    TOKEN_OPGREATERTHAN,
    TOKEN_OPGREATERTEQUAL,
    TOKEN_OPEQUAL,
    TOKEN_OPNOTEQUAL,
    TOKEN_OPAND,
    TOKEN_OPOR,
    TOKEN_OPBITNOT,
    TOKEN_OPBITAND,
    TOKEN_OPBITOR,
    TOKEN_OPBITXOR,
    TOKEN_OPLEFTSHIFT,
    TOKEN_OPRIGHTSHIFT,

    /* Delimiters */
    TOKEN_OPCOMMA,
    TOKEN_OPSEMICOLON,
    TOKEN_OPCOLON,
    TOKEN_OPLEFTPAREN,
    TOKEN_OPRIGHTPAREN,
    TOKEN_OPLEFTBRACE,
    TOKEN_OPRIGHTBRACE,
    TOKEN_OPLEFTBRACKET,
    TOKEN_OPRIGHTBRACKET,
    TOKEN_OPDOT,
    TOKEN_OPMODULO,

    /* Keywords */
    TOKEN_KWFUNCTION,
    TOKEN_KWCONST,
    TOKEN_KWVAR,
    TOKEN_KWTRUE,
    TOKEN_KWFALSE,
    TOKEN_KWIF,
    TOKEN_KWELSE,
    TOKEN_KWRETURN,
    TOKEN_KWWHILE,
    TOKEN_KWBREAK,
    TOKEN_KWFOR,
    TOKEN_KWIN,
    TOKEN_KWCONTINUE,
    TOKEN_KWNULL,
    TOKEN_KWINCLUDE,
    TOKEN_KWIMPORT,
    TOKEN_KWRECOVER,

    /* Identifiers and literals */
    TOKEN_VALIDENT,
    TOKEN_VALNUMBER,
    TOKEN_VALSTRING,
    TOKEN_VALTPLSTRING,

    /* MUST be last */
    TOKEN_TYPE_MAX
};

enum ApeOperator
{
    APE_OPERATOR_NONE,
    APE_OPERATOR_ASSIGN,
    APE_OPERATOR_PLUS,
    APE_OPERATOR_MINUS,
    APE_OPERATOR_NOT,
    APE_OPERATOR_STAR,
    APE_OPERATOR_SLASH,
    APE_OPERATOR_LESSTHAN,
    APE_OPERATOR_LESSEQUAL,
    APE_OPERATOR_GREATERTHAN,
    APE_OPERATOR_GREATEREQUAL,
    APE_OPERATOR_EQUAL,
    APE_OPERATOR_NOTEQUAL,
    APE_OPERATOR_MODULUS,
    APE_OPERATOR_LOGICALAND,
    APE_OPERATOR_LOGICALOR,
    APE_OPERATOR_BITNOT,
    APE_OPERATOR_BITAND,
    APE_OPERATOR_BITOR,
    APE_OPERATOR_BITXOR,
    APE_OPERATOR_LEFTSHIFT,
    APE_OPERATOR_RIGHTSHIFT,
};

enum ApeAstExprType
{
    APE_EXPR_NONE,
    APE_EXPR_IDENT,
    APE_EXPR_LITERALNUMBER,
    APE_EXPR_LITERALBOOL,
    APE_EXPR_LITERALSTRING,
    APE_EXPR_LITERALNULL,
    APE_EXPR_LITERALARRAY,
    APE_EXPR_LITERALMAP,
    APE_EXPR_PREFIX,
    APE_EXPR_INFIX,
    APE_EXPR_LITERALFUNCTION,
    APE_EXPR_CALL,
    APE_EXPR_INDEX,
    APE_EXPR_ASSIGN,
    APE_EXPR_LOGICAL,
    APE_EXPR_TERNARY,
    APE_EXPR_DEFINE,
    APE_EXPR_IFELSE,
    APE_EXPR_RETURNVALUE,
    APE_EXPR_EXPRESSION,
    APE_EXPR_WHILELOOP,
    APE_EXPR_BREAK,
    APE_EXPR_CONTINUE,
    APE_EXPR_FOREACH,
    APE_EXPR_FORLOOP,
    APE_EXPR_BLOCK,
    APE_EXPR_INCLUDE,
    APE_EXPR_RECOVER,
};

enum ApeSymbolType
{
    APE_SYMBOL_NONE = 0,
    APE_SYMBOL_MODULEGLOBAL,
    APE_SYMBOL_LOCAL,
    APE_SYMBOL_CONTEXTGLOBAL,
    APE_SYMBOL_FREE,
    APE_SYMBOL_FUNCTION,
    APE_SYMBOL_THIS,
};

enum ApeOpcodeValue
{
    APE_OPCODE_NONE = 0,
    APE_OPCODE_CONSTANT,
    APE_OPCODE_ADD,
    APE_OPCODE_POP,
    APE_OPCODE_SUB,
    APE_OPCODE_MUL,
    APE_OPCODE_DIV,
    APE_OPCODE_MOD,
    APE_OPCODE_TRUE,
    APE_OPCODE_FALSE,
    APE_OPCODE_COMPAREPLAIN,
    APE_OPCODE_COMPAREEQUAL,
    APE_OPCODE_ISEQUAL,
    APE_OPCODE_NOTEQUAL,
    APE_OPCODE_GREATERTHAN,
    APE_OPCODE_GREATEREQUAL,
    APE_OPCODE_MINUS,
    APE_OPCODE_NOT,
    APE_OPCODE_JUMP,
    APE_OPCODE_JUMPIFFALSE,
    APE_OPCODE_JUMPIFTRUE,
    APE_OPCODE_NULL,
    APE_OPCODE_GETMODULEGLOBAL,
    APE_OPCODE_SETMODULEGLOBAL,
    APE_OPCODE_DEFMODULEGLOBAL,
    APE_OPCODE_MKARRAY,
    APE_OPCODE_MAPSTART,
    APE_OPCODE_MAPEND,
    APE_OPCODE_GETTHIS,
    APE_OPCODE_GETINDEX,
    APE_OPCODE_SETINDEX,
    APE_OPCODE_GETVALUEAT,
    APE_OPCODE_CALL,
    APE_OPCODE_RETURNVALUE,
    APE_OPCODE_RETURNNOTHING,
    APE_OPCODE_GETLOCAL,
    APE_OPCODE_DEFLOCAL,
    APE_OPCODE_SETLOCAL,
    APE_OPCODE_GETCONTEXTGLOBAL,
    APE_OPCODE_MKFUNCTION,
    APE_OPCODE_GETFREE,
    APE_OPCODE_SETFREE,
    APE_OPCODE_CURRENTFUNCTION,
    APE_OPCODE_DUP,
    APE_OPCODE_MKNUMBER,
    APE_OPCODE_LEN,
    APE_OPCODE_SETRECOVER,
    APE_OPCODE_BITNOT,
    APE_OPCODE_BITOR,
    APE_OPCODE_BITXOR,
    APE_OPCODE_BITAND,
    APE_OPCODE_LEFTSHIFT,
    APE_OPCODE_RIGHTSHIFT,
    APE_OPCODE_IMPORT,
    APE_OPCODE_MAX,
};

enum ApeAstPrecedence
{
    PRECEDENCE_LOWEST = 0,
    /* a = b */
    PRECEDENCE_ASSIGN,
    /* a ? b : c */
    PRECEDENCE_TERNARY,
    /* || */
    PRECEDENCE_LOGICAL_OR,
    /* && */
    PRECEDENCE_LOGICAL_AND,
    /* | */
    PRECEDENCE_BIT_OR,
    /* ^ */
    PRECEDENCE_BIT_XOR,
    /* & */
    PRECEDENCE_BIT_AND,
    /* == != */
    PRECEDENCE_EQUALS,
    /* >, >=, <, <= */
    PRECEDENCE_LESSGREATER,
    /* << >> */
    PRECEDENCE_SHIFT,
    /* + - */
    PRECEDENCE_SUM,
    /* * / % */
    PRECEDENCE_PRODUCT,
    /* -x !x ++x --x */
    PRECEDENCE_PREFIX,
    /* x++ x-- */
    PRECEDENCE_INCDEC,
    /* myFunction(x) x["foo"] x.foo */
    PRECEDENCE_POSTFIX,

    /* MUST be last */
    PRECEDENCE_HIGHEST
};

typedef uint64_t ApeOpByte;
typedef double ApeFloat;
typedef int64_t ApeInt;
typedef uint64_t ApeUInt;
typedef int8_t ApeShort;
typedef uint8_t ApeUShort;
typedef size_t ApeSize;

typedef enum /**/ ApeAstTokType ApeAstTokType;
typedef enum /**/ ApeOperator ApeOperator;
typedef enum /**/ ApeSymbolType ApeSymbolType;
typedef enum /**/ ApeAstExprType ApeAstExprType;
typedef enum /**/ ApeOpcodeValue ApeOpcodeValue;
typedef enum /**/ ApeAstPrecedence ApeAstPrecedence;
typedef enum /**/ ApeObjType ApeObjType;
typedef enum /**/ ApeErrorType ApeErrorType;
typedef struct /**/ ApeContext ApeContext;
typedef struct /**/ ApeAstProgram ApeAstProgram;
typedef struct /**/ ApeVM ApeVM;
typedef struct /**/ ApeConfig ApeConfig;
typedef struct /**/ ApePosition ApePosition;
typedef struct /**/ ApeAllocator ApeAllocator;
typedef struct /**/ ApeError ApeError;
typedef struct /**/ ApeErrorList ApeErrorList;
typedef struct /**/ ApeAstToken ApeAstToken;
typedef struct /**/ ApeAstCompFile ApeAstCompFile;
typedef struct /**/ ApeAstLexer ApeAstLexer;
typedef struct /**/ ApeAstBlockExpr ApeAstBlockExpr;
typedef struct /**/ ApeAstLiteralMapExpr ApeAstLiteralMapExpr;
typedef struct /**/ ApeAstPrefixExpr ApeAstPrefixExpr;
typedef struct /**/ ApeAstInfixExpr ApeAstInfixExpr;
typedef struct /**/ ApeAstIfCaseExpr ApeAstIfCaseExpr;
typedef struct /**/ ApeAstLiteralFuncExpr ApeAstLiteralFuncExpr;
typedef struct /**/ ApeAstCallExpr ApeAstCallExpr;
typedef struct /**/ ApeAstIndexExpr ApeAstIndexExpr;
typedef struct /**/ ApeAstAssignExpr ApeAstAssignExpr;
typedef struct /**/ ApeAstLogicalExpr ApeAstLogicalExpr;
typedef struct /**/ ApeAstTernaryExpr ApeAstTernaryExpr;
typedef struct /**/ ApeAstIdentExpr ApeAstIdentExpr;
typedef struct /**/ ApeAstExpression ApeAstExpression;
typedef struct /**/ ApeAstDefineExpr ApeAstDefineExpr;
typedef struct /**/ ApeAstIfExpr ApeAstIfExpr;
typedef struct /**/ ApeAstWhileLoopExpr ApeAstWhileLoopExpr;
typedef struct /**/ ApeAstForeachExpr ApeAstForeachExpr;
typedef struct /**/ ApeAstForExpr ApeAstForExpr;
typedef struct /**/ ApeAstIncludeExpr ApeAstIncludeExpr;
typedef struct /**/ ApeAstRecoverExpr ApeAstRecoverExpr;
typedef struct /**/ ApeAstParser ApeAstParser;
typedef struct /**/ ApeObject ApeObject;
typedef struct /**/ ApeScriptFunction ApeScriptFunction;
typedef struct /**/ ApeNativeFunction ApeNativeFunction;
typedef struct /**/ ApeExternalData ApeExternalData;
typedef struct /**/ ApeObjError ApeObjError;
typedef struct /**/ ApeObjString ApeObjString;
typedef struct /**/ ApeGCObjData ApeGCObjData;
typedef struct /**/ ApeSymbol ApeSymbol;
typedef struct /**/ ApeAstBlockScope ApeAstBlockScope;
typedef struct /**/ ApeSymTable ApeSymTable;
typedef struct /**/ ApeOpcodeDef ApeOpcodeDef;
typedef struct /**/ ApeAstCompResult ApeAstCompResult;
typedef struct /**/ ApeAstCompScope ApeAstCompScope;
typedef struct /**/ ApeGCObjPool ApeGCObjPool;
typedef struct /**/ ApeGCMemory ApeGCMemory;
typedef struct /**/ ApeTracebackItem ApeTracebackItem;
typedef struct /**/ ApeTraceback ApeTraceback;
typedef struct /**/ ApeFrame ApeFrame;
typedef struct /**/ ApeValDict ApeValDict;
typedef struct /**/ ApeStrDict ApeStrDict;
typedef struct /**/ ApeValArray ApeValArray;
typedef struct /**/ ApePtrArray ApePtrArray;
typedef struct /**/ ApeWriter ApeWriter;
typedef struct /**/ ApeGlobalStore ApeGlobalStore;
typedef struct /**/ ApeModule ApeModule;
typedef struct /**/ ApeAstFileScope ApeAstFileScope;
typedef struct /**/ ApeAstCompiler ApeAstCompiler;
typedef struct /**/ ApeNativeFuncWrapper ApeNativeFuncWrapper;
typedef struct /**/ ApeNativeItem ApeNativeItem;
typedef struct /**/ ApeObjMemberItem ApeObjMemberItem;
typedef struct /**/ ApePseudoClass ApePseudoClass;
typedef struct /**/ ApeArgCheck ApeArgCheck;
typedef struct /**/ ApeMemPool ApeMemPool;
typedef struct /**/ ApeExecState ApeExecState;


typedef ApeObject (*ApeWrappedNativeFunc)(ApeContext*, void*, ApeSize, ApeObject*);
typedef ApeObject (*ApeNativeFuncPtr)(ApeVM*, void*, ApeSize, ApeObject*);

typedef size_t (*ApeIOStdoutWriteFunc)(ApeContext*, const void*, size_t);
typedef char* (*ApeIOReadFunc)(ApeContext*, const char*, long int, size_t*);
typedef size_t (*ApeIOWriteFunc)(ApeContext*, const char*, const char*, size_t);
typedef size_t (*ApeIOFlushFunc)(ApeContext*, const char*, const char*, size_t);

typedef size_t (*ApeWriterWriteFunc)(ApeContext*, void*, const char*, size_t);
typedef void (*ApeWriterFlushFunc)(ApeContext*, void*);

typedef void* (*ApeMemAllocFunc)(ApeContext*, void*, size_t);
typedef void (*ApeMemFreeFunc)(ApeContext*, void*, void*);
typedef unsigned long (*ApeDataHashFunc)(const void*);
typedef bool (*ApeDataEqualsFunc)(const void*, const void*);
typedef void* (*ApeDataCallback)(ApeContext*, void*);


typedef ApeAstExpression* (*ApeRightAssocParseFNCallback)(ApeAstParser* p);
typedef ApeAstExpression* (*ApeLeftAssocParseFNCallback)(ApeAstParser* p, ApeAstExpression* expr);


struct ApeArgCheck
{
    ApeVM* vm;
    const char* name;
    bool haveminargs;
    bool counterror;
    ApeSize minargs;
    ApeSize argc;
    ApeObject* args;
};

struct ApeNativeItem
{
    const char* name;
    ApeNativeFuncPtr fn;
};

/*
* a class that isn't.
* this merely keeps a pointer (very important!) to a StrDict, that contain the actual fields.
* don't ever initialize fndictref directly, as it would never get freed.
*/
struct ApePseudoClass
{
    ApeContext* context;
    const char* classname;
    ApeStrDict* fndictref;
};

struct ApeObjMemberItem
{
    const char* name;
    bool isfunction;
    ApeNativeFuncPtr fn;
};

struct ApeScriptFunction
{
    ApeObject* freevals;
    union
    {
        char* name;
        const char* const_name;
    };
    ApeAstCompResult* compiledcode;
    ApeSize numlocals;
    ApeSize numargs;
    ApeSize numfreevals;
    bool owns_data;
};

struct ApeExternalData
{
    void* data;
    ApeDataCallback fndestroy;
    ApeDataCallback fncopy;
};

struct ApeObjError
{
    char* message;
    ApeTraceback* traceback;
};

struct ApeObjString
{
    //union
    //{
    char* valalloc;
    //char  valstack[APE_CONF_SIZE_STRING_BUFSIZE];
    //};
    unsigned long hash;
    //bool          is_allocated;
    //ApeSize     capacity;
    //ApeSize     length;
};

struct ApeNativeFunction
{
    char* name;
    ApeNativeFuncPtr nativefnptr;
    void* dataptr;
    ApeSize datalen;
};


struct ApeNativeFuncWrapper
{
    ApeWrappedNativeFunc wrappedfnptr;
    ApeContext* context;
    void* data;
};

/**
// don't access these struct fields directly - use the provided macros.
// some fields (specifically, .type and .handle->type) may depend on eachother,
// and since .handle can sometimes be NULL, it's just easier to use the specific macros,
// since they already take care of those issues for you.
*/
struct ApeObject
{
    ApeObjType type;
    union
    {
        bool valbool;
        ApeInt valfixednum;
        ApeFloat valfloatnum;
        /* technically redundant, but regrettably needed for some tools. always NULL. */
        void* valnull;
    };
    ApeGCObjData* handle;
};

struct ApeGCObjData
{
    ApeContext* context;
    ApeGCMemory* mem;
    union
    {
        ApeObjString valstring;
        ApeObjError valerror;
        ApeValArray* valarray;
        ApeValDict* valmap;
        ApeScriptFunction valscriptfunc;
        ApeNativeFunction valnatfunc;
        ApeExternalData valextern;
    };
    bool gcmark;
    ApeObjType datatype;
};

struct ApePosition
{
    const ApeAstCompFile* file;
    int line;
    int column;
};


struct ApeValDict
{
    ApeContext* context;
    ApeSize keysize;
    ApeSize valsize;
    unsigned int* cells;
    unsigned long* hashes;
    void** keys;
    void** values;
    unsigned int* cellindices;
    ApeSize count;
    ApeSize itemcap;
    ApeSize cellcap;
    ApeDataHashFunc fnhashkey;
    ApeDataEqualsFunc fnequalkeys;
    ApeDataCallback fnvalcopy;
    ApeDataCallback fnvaldestroy;
};

struct ApeStrDict
{
    ApeContext* context;
    unsigned int* cells;
    unsigned long* hashes;
    char** keys;
    void** values;
    unsigned int* cellindices;
    ApeSize count;
    ApeSize itemcap;
    ApeSize cellcap;
    ApeDataCallback fnstrcopy;
    ApeDataCallback fnstrdestroy;
};



struct ApeWriter
{
    ApeContext* context;
    bool failed;
    char* datastring;
    ApeSize datacapacity;
    ApeSize datalength;
    void* iohandle;
    ApeWriterWriteFunc iowriter;
    ApeWriterFlushFunc ioflusher;
    bool iomustclose;
    bool iomustflush;
};

struct ApeMemPool
{
    bool available;

    ApeInt totalalloccount;
    ApeInt totalmapped;
    ApeInt totalbytes;

    bool enabledebug;
    bool debughndmustclose;
    FILE* debughandle;

    /* actual pool count */
    ApeSize poolcount;

    /* pool array length (2^x ceil of ct) */
    ApeSize poolarrlength;

    /* minimum pool size */
    ApeSize minpoolsize;

    /* maximum pool size */
    ApeSize maxpoolsize;

    /* page size, typically 4096 */
    ApeSize pgsize;

    /* pools */
    void** pools;

    /* chunk size for each pool */
    int* psizes;

    /* heads for pools' free lists */
    void* heads[1];
};

struct ApeAllocator
{
    ApeContext* context;
    ApeMemAllocFunc fnmalloc;
    ApeMemFreeFunc fnfree;
    void* optr;
    ApeMemPool* pool;
    bool ready;
};

struct ApeError
{
    short errtype;
    char message[APE_CONF_SIZE_ERROR_MAXMSGLENGTH];
    ApePosition pos;
    ApeTraceback* traceback;
};

struct ApeErrorList
{
    ApeError errors[APE_CONF_SIZE_ERRORS_MAXCOUNT];
    ApeSize count;
};

struct ApeTracebackItem
{
    char* function_name;
    ApePosition pos;
};

struct ApeTraceback
{
    ApeContext* context;
    ApeValArray* items;
};

struct ApeAstToken
{
    ApeAstTokType toktype;
    const char* literal;
    ApeSize len;
    ApePosition pos;
};

struct ApeAstCompFile
{
    ApeContext* context;
    char* dirpath;
    char* path;
    ApePtrArray* lines;
};

struct ApeAstLexer
{
    ApeContext* context;
    ApeErrorList* errors;
    const char* input;
    ApeSize inputlen;
    ApeSize position;
    ApeSize nextposition;
    char ch;
    ApeInt line;
    ApeInt column;
    ApeAstCompFile* file;
    bool failed;
    bool continue_template_string;
    struct
    {
        ApeSize position;
        ApeSize nextposition;
        char ch;
        ApeInt line;
        ApeInt column;
    } prevtokenstate;
    ApeAstToken prevtoken;
    ApeAstToken curtoken;
    ApeAstToken peektoken;
};

struct ApeAstBlockExpr
{
    ApeContext* context;
    ApePtrArray* statements;
};

struct ApeAstPrefixExpr
{
    ApeOperator op;
    ApeAstExpression* right;
};

struct ApeAstInfixExpr
{
    ApeOperator op;
    ApeAstExpression* left;
    ApeAstExpression* right;
};

struct ApeAstIfCaseExpr
{
    ApeContext* context;
    ApeAstExpression* test;
    ApeAstBlockExpr* consequence;
};

struct ApeAstLiteralFuncExpr
{
    char* name;
    ApePtrArray* params;
    ApeAstBlockExpr* body;
};

struct ApeAstCallExpr
{
    ApeAstExpression* function;
    ApePtrArray* args;
};

struct ApeAstIndexExpr
{
    ApeAstExpression* left;
    ApeAstExpression* index;
};

struct ApeAstAssignExpr
{
    ApeAstExpression* dest;
    ApeAstExpression* source;
    bool ispostfix;
};

struct ApeAstLogicalExpr
{
    ApeOperator op;
    ApeAstExpression* left;
    ApeAstExpression* right;
};

struct ApeAstTernaryExpr
{
    ApeAstExpression* test;
    ApeAstExpression* iftrue;
    ApeAstExpression* iffalse;
};

struct ApeAstIdentExpr
{
    ApeContext* context;
    char* value;
    ApePosition pos;
};

struct ApeAstDefineExpr
{
    ApeAstIdentExpr* name;
    ApeAstExpression* value;
    bool assignable;
};

struct ApeAstIfExpr
{
    ApePtrArray* cases;
    ApeAstBlockExpr* alternative;
};

struct ApeAstWhileLoopExpr
{
    ApeAstExpression* test;
    ApeAstBlockExpr* body;
};

struct ApeAstForeachExpr
{
    ApeAstIdentExpr* iterator;
    ApeAstExpression* source;
    ApeAstBlockExpr* body;
};

struct ApeAstForExpr
{
    ApeAstExpression* init;
    ApeAstExpression* test;
    ApeAstExpression* update;
    ApeAstBlockExpr* body;
};

struct ApeAstIncludeExpr
{
    char* path;
    ApeAstExpression* left;
};

struct ApeAstRecoverExpr
{
    ApeAstIdentExpr* errorident;
    ApeAstBlockExpr* body;
};

struct ApeAstLiteralMapExpr
{
    ApePtrArray* keys;
    ApePtrArray* values;
};


struct ApeAstExpression
{
    ApeContext* context;
    ApeAstExprType extype;
    bool stringwasallocd;
    ApeSize stringlitlength;
    union
    {
        ApeAstIdentExpr* exident;
        ApeFloat exliteralnumber;
        bool exliteralbool;
        char* exliteralstring;
        ApePtrArray* exarray;
        ApeAstLiteralMapExpr exmap;
        ApeAstPrefixExpr exprefix;
        ApeAstInfixExpr exinfix;
        ApeAstLiteralFuncExpr exliteralfunc;
        ApeAstCallExpr excall;
        ApeAstIndexExpr exindex;
        ApeAstAssignExpr exassign;
        ApeAstLogicalExpr exlogical;
        ApeAstTernaryExpr externary;
        ApeAstDefineExpr exdefine;
        ApeAstIfExpr exifstmt;
        ApeAstExpression* exreturn;
        ApeAstExpression* exexpression;
        ApeAstWhileLoopExpr exwhilestmt;
        ApeAstForeachExpr exforeachstmt;
        ApeAstForExpr exforstmt;
        ApeAstBlockExpr* exblock;
        ApeAstIncludeExpr exincludestmt;
        ApeAstRecoverExpr exrecoverstmt;
    };
    ApePosition pos;
};

struct ApeAstParser
{
    ApeContext* context;
    const ApeConfig* config;
    ApeAstLexer lexer;
    ApeErrorList* errors;
    ApeSize depth;
};

struct ApeSymbol
{
    ApeContext* context;
    ApeSymbolType symtype;
    char* name;
    ApeSize index;
    bool assignable;
};

struct ApeAstBlockScope
{
    ApeContext* context;
    ApeStrDict* store;
    ApeInt offset;
    ApeSize numdefinitions;
};

struct ApeSymTable
{
    ApeContext* context;
    ApeSymTable* outer;
    ApeGlobalStore* globalstore;
    ApePtrArray* blockscopes;
    ApePtrArray* freesymbols;
    ApePtrArray* modglobalsymbols;
    ApeSize maxnumdefinitions;
    ApeInt modglobaloffset;
};

struct ApeOpcodeDef
{
    const char* name;
    ApeSize operandcount;
    ApeInt operandwidths[2];
};


struct ApeModule
{
    ApeContext* context;
    char* name;
    ApePtrArray* modsymbols;
};

struct ApeAstFileScope
{
    ApeContext* context;
    ApeAstParser* parser;
    ApeSymTable* symtable;
    ApeAstCompFile* file;
    ApePtrArray* loadedmodulenames;
};

struct ApeAstCompResult
{
    ApeContext* context;
    ApeUShort* bytecode;
    ApePosition* srcpositions;
    ApeSize count;
};

struct ApeAstCompScope
{
    ApeContext* context;
    ApeAstCompScope* outer;
    ApeValArray* bytecode;
    ApeValArray* srcpositions;
    ApeValArray* breakipstack;
    ApeValArray* continueipstack;
    ApeOpByte lastopcode;
};

struct ApeAstCompiler
{
    ApeContext* context;
    const ApeConfig* config;
    ApeGCMemory* mem;
    ApeErrorList* errors;
    ApePtrArray* files;
    ApeGlobalStore* globalstore;
    ApeValArray* constants;
    ApeAstCompScope* compilation_scope;
    ApePtrArray* filescopes;
    ApeValArray* srcpositionsstack;
    ApeStrDict* modules;
    ApeStrDict* stringconstantspositions;
};

struct ApeAstProgram
{
    ApeContext* context;
    ApeAstCompResult* comp_res;
};

struct ApeGlobalStore
{
    ApeContext* context;
    ApeStrDict* named;
    ApeValArray* objects;
};

struct ApeFrame
{
    bool allocated;
    ApeObject function;
    ApePosition* srcpositions;
    ApeUShort* bytecode;

    ApeInt ip;
    //short ip;

    ApeInt basepointer;
    //short basepointer;

    ApeInt srcip;
    //short srcip;

    ApeSize bcsize;
    //short bcsize;

    ApeInt recoverip;
    //short recoverip;

    bool isrecovering;
};


struct ApeExecState
{
    ApeFrame* frame;
    ApeFrame newframe;
    ApeOpcodeValue opcode;
    ApeValArray * constants;
};


struct ApeVM
{
    ApeContext* context;
    const ApeConfig* config;
    ApeExecState estate;
    ApeGCMemory* mem;
    ApeErrorList* errors;
    ApeGlobalStore* globalstore;

    ApeObject overloadkeys[APE_OPCODE_MAX];

    ApeValDict* globalobjects;

    ApeValDict* stackobjects;
    int stackptr;

    ApeObject thisobjects[APE_CONF_SIZE_VM_THISSTACK];
    int thisptr;

    //DequeList_t* frameobjects;
    intptr_t* frameobjects;
    ApeSize countframes;
    ApeFrame* lastframe;

    ApeObject lastpopped;
    ApeFrame* currentframe;

    bool running;
};

struct ApeConfig
{
    struct
    {
        ApeIOStdoutWriteFunc write;
    } stdio;

    struct
    {
        ApeIOReadFunc fnreadfile;
        ApeIOWriteFunc fnwritefile;
    } fileio;

    /* allows redefinition of symbols */
    bool replmode;
    double max_execution_time_ms;
    bool max_execution_time_set;
    bool runafterdump;
    bool dumpast;
    bool dumpbytecode;
    bool dumpstack;
};


struct ApeContext
{
    ApeAllocator alloc;
    /* allocator that abstract malloc/free - TODO: not used right now, because it's broken */
    ApeAllocator custom_allocator;

    /* a writer instance that writes to stderr. use with ape_context_debug* */
    ApeWriter* debugwriter;
    ApeWriter* stdoutwriter;

    /* object memory */
    ApeGCMemory* mem;

    /* files being compiled, whether as main script, or include(), import(), etc */
    ApePtrArray* files;

    /* array that stores pointers to pseudoclasses */
    ApePtrArray* pseudoclasses;

    /* contains the typemapping of pseudoclasses where key is the typename */
    ApeStrDict* classmapping;

    /* globally defined object */
    ApeGlobalStore* globalstore;

    /* the main compiler instance - may spawn additional compiler instances */
    ApeAstCompiler* compiler;

    /* the main VM instance - may spawn additional VM instances */
    ApeVM* vm;

    /* the current list of errors (if any) */
    ApeErrorList errors;

    /* the runtime configuration */
    ApeConfig config;

    /**
    * stored member function mapping. string->funcpointer
    */

    /* string member functions */
    ApeStrDict* objstringfuncs;

    /* array member functions */
    ApeStrDict* objarrayfuncs;
};

/*
#ifdef __cplusplus
APE_EXTERNC_BEGIN
#endif
*/

/*
* one day, when the API doesn't change as often as it does right now, this will
* contains the actual prototypes, commented and all.
* but until then, prot.inc is generated, and then included. sorry.
*/
#include "prot.inc"

static APE_INLINE ApeObject object_make_from_data(ApeContext* ctx, ApeObjType type, ApeGCObjData* data)
{
    ApeObject object;
    object.type = type;
    data->context = ctx;
    data->datatype = type;
    object.handle = data;
    object.handle->datatype = type;
    return object;
}

static APE_INLINE ApeObject ape_object_make_floatnumber(ApeContext* ctx, ApeFloat val)
{
    ApeObject rt;
    (void)ctx;
    rt.handle = NULL;
    rt.type = APE_OBJECT_FLOATNUMBER;
    rt.valfloatnum = val;
    return rt;
}

static APE_INLINE ApeObject ape_object_make_fixednumber(ApeContext* ctx, ApeInt val)
{
    ApeObject rt;
    (void)ctx;
    rt.handle = NULL;
    rt.type = APE_OBJECT_FIXEDNUMBER;
    rt.valfixednum = val;
    return rt;
}

static APE_INLINE ApeObject ape_object_make_bool(ApeContext* ctx, bool val)
{
    ApeObject rt;
    (void)ctx;
    rt.handle = NULL;
    rt.type = APE_OBJECT_BOOL;
    rt.valbool = val;
    return rt;
}

static APE_INLINE ApeObject ape_object_make_null(ApeContext* ctx)
{
    ApeObject rt;
    (void)ctx;
    rt.handle = NULL;
    rt.type = APE_OBJECT_NULL;
    rt.valnull = NULL;
    return rt;
}

static APE_INLINE void ape_args_init(ApeVM* vm, ApeArgCheck* check, const char* name, ApeSize argc, ApeObject* args)
{
    check->vm = vm;
    check->name = name;
    check->haveminargs = false;
    check->counterror = false;
    check->minargs = 0;
    check->argc = argc;
    check->args = args;
}

static APE_INLINE void ape_args_raisetyperror(ApeArgCheck* check, ApeSize ix, int typ)
{
    char* extypestr;
    const char* typestr;
    typestr = ape_object_value_typename(ape_object_value_type(check->args[ix]));
    extypestr = ape_object_value_typeunionname(check->vm->context, (ApeObjType)typ);
    ape_vm_adderror(check->vm, APE_ERROR_RUNTIME, "invalid arguments: function '%s' expects argument #%d to be a %s, but got %s instead", check->name, ix, extypestr, typestr);
    ape_allocator_free(&check->vm->context->alloc, extypestr);
}

static APE_INLINE bool ape_args_checkoptional(ApeArgCheck* check, ApeSize ix, int typ, bool raisetyperror)
{
    check->counterror = false;
    if((check->argc == 0) || (check->argc <= ix))
    {
        check->counterror = true;
        return false;
    }
    if(!(ape_object_value_type(check->args[ix]) & typ))
    {
        if(raisetyperror)
        {
            ape_args_raisetyperror(check, ix, typ);
        }
        return false;
    }
    return true;
}

static APE_INLINE bool ape_args_check(ApeArgCheck* check, ApeSize ix, int typ)
{
    ApeSize ixval;
    if(!ape_args_checkoptional(check, ix, typ, false))
    {
        if(check->counterror)
        {
            ixval = ix;
            if(check->haveminargs)
            {
                ixval = check->minargs;
            }
            else
            {
            }
            ape_vm_adderror(check->vm, APE_ERROR_RUNTIME, "invalid arguments: function '%s' expects at least %d arguments", check->name, ixval);
        }
        else
        {
            ape_args_raisetyperror(check, ix, typ);
        }
    }
    return true;
}


/*
#ifdef __cplusplus
APE_EXTERNC_END
#endif
*/
