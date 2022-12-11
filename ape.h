
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

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef _MSC_VER
    #ifndef _CRT_SECURE_NO_WARNINGS
        #define _CRT_SECURE_NO_WARNINGS
    #endif /* _CRT_SECURE_NO_WARNINGS */
#endif /* _MSC_VER */

#ifdef __cplusplus
    #define APE_EXTERNC_BEGIN \
        extern "C"            \
        {
    #define APE_EXTERNC_END }
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
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

#define APE_VERSION_MAJOR 0
#define APE_VERSION_MINOR 14
#define APE_VERSION_PATCH 0

#define APE_VERSION_STRING "0.14.0"

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
#define APE_CONF_DICT_INITIAL_SIZE (2)
#define APE_CONF_ARRAY_INITIAL_CAPACITY (64/8)
//#define APE_CONF_ARRAY_INITIAL_CAPACITY (2)

#define APE_CONF_MAP_INITIAL_CAPACITY (64/8)
//#define APE_CONF_MAP_INITIAL_CAPACITY (2)

#define APE_CONF_PLAINLIST_CAPACITY_ADD 32

#define APE_CONF_SIZE_VM_STACK (1024)
#define APE_CONF_SIZE_VM_MAXGLOBALS (512 / 4)
#define APE_CONF_SIZE_MAXFRAMES (512 / 4)
#define APE_CONF_SIZE_VM_THISSTACK (512 / 4)

#define APE_CONF_SIZE_NATFN_MAXDATALEN (16 * 2)
#define APE_CONF_SIZE_STRING_BUFSIZE (4)

#define APE_CONF_SIZE_ERRORS_MAXCOUNT (16)
#define APE_CONF_SIZE_ERROR_MAXMSGLENGTH (80)


/* decreasing these incurs higher memory use */
#define APE_CONF_SIZE_GCMEM_POOLSIZE (512 * 4)
#define APE_CONF_SIZE_GCMEM_POOLCOUNT ((3) + 1)
#define APE_CONF_CONST_GCMEM_SWEEPINTERVAL (128 / 1)


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
    vm_call(vm, (function_name), sizeof((ApeObject_t[]){ __VA_ARGS__ }) / sizeof(ApeObject_t), (ApeObject_t[]){ __VA_ARGS__ })

#define APE_ASSERT(x) assert((x))

#if defined(APE_DEBUG) && (APE_DEBUG == 1)
    #define APE_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
    #define APE_LOG(...) ape_log(APE_FILENAME, __LINE__, __VA_ARGS__)
#else
    #define APE_LOG(...) ((void)0)
#endif

#if 0
    #define APE_CHECK_ARGS(vm, generate_error, argc, args, ...)                                                                  \
        ape_args_check((vm), (generate_error), (argc), (args), (sizeof((ApeObjType_t[]){ __VA_ARGS__ }) / sizeof(ApeObjType_t)), \
                       ((ApeObjType_t*)((ApeObjType_t[]){ __VA_ARGS__ })))
#else
    #define APE_CHECK_ARGS(vm, generate_error, argc, args, ...) \
        ape_args_check((vm), (generate_error), (argc), (args), (sizeof((int[]){ __VA_ARGS__ }) / sizeof(int)), ape_args_make_typarray(__VA_ARGS__, -1))
#endif


/**/
#define ape_object_value_type(obj) \
    (((obj).handle == NULL) ? APE_OBJECT_NULL : ((ApeObjType_t)((obj).handle->datatype)))

#define ape_object_value_istype(o, t) \
    (ape_object_value_type(o) == (t))

#define ape_object_value_isnumber(o) \
    ape_object_value_istype(o, APE_OBJECT_NUMBER)

#define ape_object_value_isnumeric(o) \
    (ape_object_value_istype(o, APE_OBJECT_NUMBER) || ape_object_value_istype(o, APE_OBJECT_BOOL))

#define ape_object_value_isnull(o) \
    (((o).type == APE_OBJECT_NONE) || ape_object_value_istype(o, APE_OBJECT_NULL))

#define ape_object_value_isstring(o) \
    ape_object_value_istype(o, APE_OBJECT_STRING)

#define ape_object_value_iscallable(o) \
    (ape_object_value_istype(o, APE_OBJECT_NATIVEFUNCTION) || ape_object_value_istype(o, APE_OBJECT_SCRIPTFUNCTION))


#define make_fn_data(ctx, name, fnc, dataptr, datasize) \
    ape_object_make_nativefuncmemory(ctx, name, fnc, dataptr, datasize)

#define make_fn(ctx, name, fnc) \
    make_fn_data(ctx, name, fnc, NULL, 0)

#define make_fn_entry_data(ctx, map, name, fnc, dataptr, datasize) \
    ape_object_map_setnamedvalue(map, name, make_fn(ctx, name, fnc))

#define make_fn_entry(ctx, map, name, fnc) \
    make_fn_entry_data(ctx, map, name, fnc, NULL, 0)

#define ape_make_valdict(ctx, key_type, val_type) \
    ape_make_valdict_actual(ctx, sizeof(key_type), sizeof(val_type))

#define ape_make_valarray(allocator, type) \
    ape_make_valarray_actual(allocator, sizeof(type))

/**/
#define ape_object_value_allocated_data(obj) \
    ((obj).handle)

/**/
#define ape_object_value_isallocated(o) \
    (true)

#define ape_object_value_asnumber(obj) \
    (ape_object_value_allocated_data((obj))->valnum)

#define ape_object_value_asbool(obj) \
    (ape_object_value_allocated_data((obj))->valbool)

#define ape_object_value_asfunction(obj) \
    (&(ape_object_value_allocated_data((obj))->valscriptfunc))

#define ape_object_value_asnativefunction(obj) \
    (&(ape_object_value_allocated_data((obj))->valnatfunc))

#define ape_object_value_asexternal(obj) \
    (&(ape_object_value_allocated_data((obj))->valextern))

#define ape_object_value_getmem(obj) \
    (ape_object_value_allocated_data(obj)->mem)


enum ApeErrorType_t
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

enum ApeObjType_t
{
    APE_OBJECT_NONE = 0,
    APE_OBJECT_ERROR = 1 << 0,
    APE_OBJECT_NUMBER = 1 << 1,
    APE_OBJECT_BOOL = 1 << 2,
    APE_OBJECT_STRING = 1 << 3,
    APE_OBJECT_NULL = 1 << 4,
    APE_OBJECT_NATIVEFUNCTION = 1 << 5,
    APE_OBJECT_ARRAY = 1 << 6,
    APE_OBJECT_MAP = 1 << 7,
    APE_OBJECT_SCRIPTFUNCTION = 1 << 8,
    APE_OBJECT_EXTERNAL = 1 << 9,
    APE_OBJECT_FREED = 1 << 10,
    /* for checking types with & */
    APE_OBJECT_ANY = 0xffff,
};

enum ApeAstTokType_t
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

enum ApeOperator_t
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
    APE_OPERATOR_BITAND,
    APE_OPERATOR_BITOR,
    APE_OPERATOR_BITXOR,
    APE_OPERATOR_LEFTSHIFT,
    APE_OPERATOR_RIGHTSHIFT,
};

enum ApeAstExprType_t
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
    APE_EXPR_IF,
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

enum ApeSymbolType_t
{
    APE_SYMBOL_NONE = 0,
    APE_SYMBOL_MODULEGLOBAL,
    APE_SYMBOL_LOCAL,
    APE_SYMBOL_CONTEXTGLOBAL,
    APE_SYMBOL_FREE,
    APE_SYMBOL_FUNCTION,
    APE_SYMBOL_THIS,
};

enum ApeOpcodeValue_t
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
    APE_OPCODE_BITOR,
    APE_OPCODE_BITXOR,
    APE_OPCODE_BITAND,
    APE_OPCODE_LEFTSHIFT,
    APE_OPCODE_RIGHTSHIFT,
    APE_OPCODE_IMPORT,
    APE_OPCODE_MAX,
};

enum ApeAstPrecedence_t
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

typedef uint64_t ApeOpByte_t;
typedef double   ApeFloat_t;
typedef int64_t  ApeInt_t;
typedef uint64_t ApeUInt_t;
typedef int8_t   ApeShort_t;
typedef uint8_t  ApeUShort_t;
typedef size_t   ApeSize_t;

typedef enum /**/ ApeAstTokType_t           ApeAstTokType_t;
typedef enum /**/ ApeOperator_t            ApeOperator_t;
typedef enum /**/ ApeSymbolType_t          ApeSymbolType_t;
typedef enum /**/ ApeAstExprType_t            ApeAstExprType_t;
typedef enum /**/ ApeOpcodeValue_t         ApeOpcodeValue_t;
typedef enum /**/ ApeAstPrecedence_t          ApeAstPrecedence_t;
typedef enum /**/ ApeObjType_t             ApeObjType_t;
typedef enum /**/ ApeErrorType_t           ApeErrorType_t;
typedef struct /**/ ApeContext_t           ApeContext_t;
typedef struct /**/ ApeAstProgram_t           ApeAstProgram_t;
typedef struct /**/ ApeVM_t                ApeVM_t;
typedef struct /**/ ApeConfig_t            ApeConfig_t;
typedef struct /**/ ApePosition_t          ApePosition_t;
typedef struct /**/ ApeTimer_t             ApeTimer_t;
typedef struct /**/ ApeAllocator_t         ApeAllocator_t;
typedef struct /**/ ApeError_t             ApeError_t;
typedef struct /**/ ApeErrorList_t         ApeErrorList_t;
typedef struct /**/ ApeAstToken_t             ApeAstToken_t;
typedef struct /**/ ApeAstCompFile_t          ApeAstCompFile_t;
typedef struct /**/ ApeAstLexer_t             ApeAstLexer_t;
typedef struct /**/ ApeAstBlockExpr_t         ApeAstBlockExpr_t;
typedef struct /**/ ApeAstLiteralMapExpr_t        ApeAstLiteralMapExpr_t;
typedef struct /**/ ApeAstPrefixExpr_t        ApeAstPrefixExpr_t;
typedef struct /**/ ApeAstInfixExpr_t         ApeAstInfixExpr_t;
typedef struct /**/ ApeAstIfCaseExpr_t            ApeAstIfCaseExpr_t;
typedef struct /**/ ApeAstLiteralFuncExpr_t         ApeAstLiteralFuncExpr_t;
typedef struct /**/ ApeAstCallExpr_t          ApeAstCallExpr_t;
typedef struct /**/ ApeAstIndexExpr_t         ApeAstIndexExpr_t;
typedef struct /**/ ApeAstAssignExpr_t        ApeAstAssignExpr_t;
typedef struct /**/ ApeAstLogicalExpr_t       ApeAstLogicalExpr_t;
typedef struct /**/ ApeAstTernaryExpr_t       ApeAstTernaryExpr_t;
typedef struct /**/ ApeAstIdentExpr_t             ApeAstIdentExpr_t;
typedef struct /**/ ApeAstExpression_t        ApeAstExpression_t;
typedef struct /**/ ApeAstDefineExpr_t        ApeAstDefineExpr_t;
typedef struct /**/ ApeAstIfExpr_t            ApeAstIfExpr_t;
typedef struct /**/ ApeAstWhileLoopExpr_t     ApeAstWhileLoopExpr_t;
typedef struct /**/ ApeAstForeachExpr_t       ApeAstForeachExpr_t;
typedef struct /**/ ApeAstForExpr_t       ApeAstForExpr_t;
typedef struct /**/ ApeAstIncludeExpr_t       ApeAstIncludeExpr_t;
typedef struct /**/ ApeAstRecoverExpr_t       ApeAstRecoverExpr_t;
typedef struct /**/ ApeAstParser_t            ApeAstParser_t;
typedef struct /**/ ApeObject_t            ApeObject_t;
typedef struct /**/ ApeScriptFunction_t    ApeScriptFunction_t;
typedef struct /**/ ApeNativeFunction_t    ApeNativeFunction_t;
typedef struct /**/ ApeExternalData_t      ApeExternalData_t;
typedef struct /**/ ApeObjError_t          ApeObjError_t;
typedef struct /**/ ApeObjString_t         ApeObjString_t;
typedef struct /**/ ApeGCObjData_t           ApeGCObjData_t;
typedef struct /**/ ApeSymbol_t            ApeSymbol_t;
typedef struct /**/ ApeAstBlockScope_t        ApeAstBlockScope_t;
typedef struct /**/ ApeSymTable_t          ApeSymTable_t;
typedef struct /**/ ApeOpcodeDef_t         ApeOpcodeDef_t;
typedef struct /**/ ApeAstCompResult_t        ApeAstCompResult_t;
typedef struct /**/ ApeAstCompScope_t         ApeAstCompScope_t;
typedef struct /**/ ApeGCObjPool_t           ApeGCObjPool_t;
typedef struct /**/ ApeGCMemory_t          ApeGCMemory_t;
typedef struct /**/ ApeTracebackItem_t     ApeTracebackItem_t;
typedef struct /**/ ApeTraceback_t         ApeTraceback_t;
typedef struct /**/ ApeFrame_t             ApeFrame_t;
typedef struct /**/ ApeValDict_t           ApeValDict_t;
typedef struct /**/ ApeStrDict_t           ApeStrDict_t;
typedef struct /**/ ApeValArray_t          ApeValArray_t;
typedef struct /**/ ApePtrArray_t          ApePtrArray_t;
typedef struct /**/ ApeWriter_t            ApeWriter_t;
typedef struct /**/ ApeGlobalStore_t       ApeGlobalStore_t;
typedef struct /**/ ApeModule_t            ApeModule_t;
typedef struct /**/ ApeAstFileScope_t         ApeAstFileScope_t;
typedef struct /**/ ApeAstCompiler_t          ApeAstCompiler_t;
typedef struct /**/ ApeNativeFuncWrapper_t ApeNativeFuncWrapper_t;
typedef struct /**/ ApeNativeItem_t        ApeNativeItem_t;
typedef struct /**/ ApeObjMemberItem_t     ApeObjMemberItem_t;
typedef struct /**/ ApePseudoClass_t       ApePseudoClass_t;
typedef struct /**/ ApeArgCheck_t          ApeArgCheck_t;

#define APE_USE_ALIST 1
typedef struct Vector_t Vector_t;
typedef uint32_t VectIndex_t;
typedef int32_t VectStatus_t;

#if defined(APE_USE_ALIST) && (APE_USE_ALIST != 0)
    #if (APE_USE_ALIST == 1)
        typedef void* MemList_t;
    #elif (APE_USE_ALIST == 2) 
        typedef Vector_t MemList_t;
    #endif
#else
    typedef struct ApePtrArray_t MemList_t;
#endif

typedef ApeObject_t (*ApeWrappedNativeFunc_t)(ApeContext_t*, void*, ApeSize_t, ApeObject_t*);
typedef ApeObject_t (*ApeNativeFuncPtr_t)(ApeVM_t*, void*, ApeSize_t, ApeObject_t*);

typedef size_t (*ApeIOStdoutWriteFunc_t)(void*, const void*, size_t);
typedef char* (*ApeIOReadFunc_t)(void*, const char*);
typedef size_t (*ApeIOWriteFunc_t)(void* context, const char*, const char*, size_t);
typedef size_t (*ApeIOFlushFunc_t)(void* context, const char*, const char*, size_t);

typedef size_t (*ApeWriterWriteFunc_t)(ApeContext_t*, void*, const char*, size_t);
typedef void (*ApeWriterFlushFunc_t)(ApeContext_t*, void*);

typedef void* (*ApeMemAllocFunc_t)(void*, size_t);
typedef void (*ApeMemFreeFunc_t)(void*, void*);
typedef unsigned long (*ApeDataHashFunc_t)(const void*);
typedef bool (*ApeDataEqualsFunc_t)(const void*, const void*);
typedef void* (*ApeDataCallback_t)(void*);


typedef ApeAstExpression_t* (*ApeRightAssocParseFNCallback_t)(ApeAstParser_t* p);
typedef ApeAstExpression_t* (*ApeLeftAssocParseFNCallback_t)(ApeAstParser_t* p, ApeAstExpression_t* expr);


struct ApeArgCheck_t
{
    ApeVM_t*     vm;
    const char*  name;
    bool         haveminargs;
    bool         counterror;
    ApeSize_t    minargs;
    ApeSize_t    argc;
    ApeObject_t* args;
};

struct ApeNativeItem_t
{
    const char*        name;
    ApeNativeFuncPtr_t fn;
};

/*
* a class that isn't.
* this merely keeps a pointer (very important!) to a StrDict, that contain the actual fields.
* don't ever initialize fndictref directly, as it would never get freed.
*/
struct ApePseudoClass_t
{
    ApeContext_t* context;
    const char*   classname;
    ApeStrDict_t* fndictref;
};

struct ApeObjMemberItem_t
{
    const char*        name;
    bool               isfunction;
    ApeNativeFuncPtr_t fn;
};

struct ApeScriptFunction_t
{
    ApeObject_t* freevals;
    union
    {
        char*       name;
        const char* const_name;
    };
    ApeAstCompResult_t* compiledcode;
    ApeSize_t        numlocals;
    ApeSize_t        numargs;
    ApeSize_t        numfreevals;
    bool             owns_data;
};

struct ApeExternalData_t
{
    void*             data;
    ApeDataCallback_t data_destroy_fn;
    ApeDataCallback_t data_copy_fn;
};

struct ApeObjError_t
{
    char*           message;
    ApeTraceback_t* traceback;
};

struct ApeObjString_t
{
    union
    {
        char* value_allocated;
        char  value_buf[APE_CONF_SIZE_STRING_BUFSIZE];
    };
    unsigned long hash;
    bool          is_allocated;
    ApeSize_t     capacity;
    ApeSize_t     length;
};

struct ApeNativeFunction_t
{
    char*              name;
    ApeNativeFuncPtr_t nativefnptr;
    void*              dataptr;
    ApeSize_t          datalen;
};


struct ApeNativeFuncWrapper_t
{
    ApeWrappedNativeFunc_t wrappedfnptr;
    ApeContext_t*          context;
    void*                  data;
};

struct ApeObject_t
{
    ApeObjType_t  type;
    ApeGCObjData_t* handle;
};

struct ApePosition_t
{
    const ApeAstCompFile_t* file;
    int                  line;
    int                  column;
};

struct ApeTimer_t
{
#if defined(APE_POSIX)
    ApeInt_t start_offset;
#elif defined(APE_WINDOWS)
    double pc_frequency;
#endif
    double start_time_ms;
};


struct ApeValDict_t
{
    ApeContext_t*       context;
    ApeSize_t           keysize;
    ApeSize_t           valsize;
    unsigned int*       cells;
    unsigned long*      hashes;
    void**              keys;
    void**              values;
    unsigned int*       cellindices;
    ApeSize_t           count;
    ApeSize_t           itemcap;
    ApeSize_t           cellcap;
    ApeDataHashFunc_t   fnhashkey;
    ApeDataEqualsFunc_t fnequalkeys;
    ApeDataCallback_t   fnvalcopy;
    ApeDataCallback_t   fnvaldestroy;
};

struct ApeStrDict_t
{
    ApeContext_t*     context;
    unsigned int*     cells;
    unsigned long*    hashes;
    char**            keys;
    void**            values;
    unsigned int*     cellindices;
    ApeSize_t         count;
    ApeSize_t         itemcap;
    ApeSize_t         cellcap;
    ApeDataCallback_t fnstrcopy;
    ApeDataCallback_t fnstrdestroy;
};

struct ApeValArray_t
{
    ApeContext_t*  context;
    unsigned char* arraydata;
    unsigned char* allocdata;
    ApeSize_t      count;
    ApeSize_t      capacity;
    ApeSize_t      elemsize;
    bool           lock_capacity;
};

struct ApePtrArray_t
{
    ApeContext_t* context;
    ApeValArray_t arr;
};

struct ApeWriter_t
{
    ApeContext_t*        context;
    bool                 failed;
    char*                datastring;
    ApeSize_t            datacapacity;
    ApeSize_t            datalength;
    void*                iohandle;
    ApeWriterWriteFunc_t iowriter;
    ApeWriterFlushFunc_t ioflusher;
    bool                 iomustclose;
    bool                 iomustflush;
};

struct ApeAllocator_t
{
    ApeMemAllocFunc_t fnmalloc;
    ApeMemFreeFunc_t  fnfree;
    void*             ctx;
};

struct ApeError_t
{
    short  errtype;
    char            message[APE_CONF_SIZE_ERROR_MAXMSGLENGTH];
    ApePosition_t   pos;
    ApeTraceback_t* traceback;
};

struct ApeErrorList_t
{
    ApeError_t errors[APE_CONF_SIZE_ERRORS_MAXCOUNT];
    ApeSize_t  count;
};

struct ApeTracebackItem_t
{
    char*         function_name;
    ApePosition_t pos;
};

struct ApeTraceback_t
{
    ApeContext_t*  context;
    ApeValArray_t* items;
};


struct ApeAstToken_t
{
    ApeAstTokType_t toktype;
    const char*    literal;
    ApeSize_t      len;
    ApePosition_t  pos;
};

struct ApeAstCompFile_t
{
    ApeContext_t*  context;
    char*          dirpath;
    char*          path;
    ApePtrArray_t* lines;
};

struct ApeAstLexer_t
{
    ApeContext_t*   context;
    ApeErrorList_t* errors;
    const char*     input;
    ApeSize_t       inputlen;
    ApeSize_t       position;
    ApeSize_t       nextposition;
    char            ch;
    ApeInt_t        line;
    ApeInt_t        column;
    ApeAstCompFile_t*  file;
    bool            failed;
    bool            continue_template_string;
    struct
    {
        ApeSize_t position;
        ApeSize_t nextposition;
        char      ch;
        ApeInt_t  line;
        ApeInt_t  column;
    } prevtokenstate;
    ApeAstToken_t prevtoken;
    ApeAstToken_t curtoken;
    ApeAstToken_t peektoken;
};

struct ApeAstBlockExpr_t
{
    ApeContext_t*  context;
    ApePtrArray_t* statements;
};

struct ApeAstPrefixExpr_t
{
    ApeOperator_t    op;
    ApeAstExpression_t* right;
};

struct ApeAstInfixExpr_t
{
    ApeOperator_t    op;
    ApeAstExpression_t* left;
    ApeAstExpression_t* right;
};

struct ApeAstIfCaseExpr_t
{
    ApeContext_t*    context;
    ApeAstExpression_t* test;
    ApeAstBlockExpr_t*  consequence;
};

struct ApeAstLiteralFuncExpr_t
{
    char*           name;
    ApePtrArray_t*  params;
    ApeAstBlockExpr_t* body;
};

struct ApeAstCallExpr_t
{
    ApeAstExpression_t* function;
    ApePtrArray_t*   args;
};

struct ApeAstIndexExpr_t
{
    ApeAstExpression_t* left;
    ApeAstExpression_t* index;
};

struct ApeAstAssignExpr_t
{
    ApeAstExpression_t* dest;
    ApeAstExpression_t* source;
    bool             ispostfix;
};

struct ApeAstLogicalExpr_t
{
    ApeOperator_t    op;
    ApeAstExpression_t* left;
    ApeAstExpression_t* right;
};

struct ApeAstTernaryExpr_t
{
    ApeAstExpression_t* test;
    ApeAstExpression_t* iftrue;
    ApeAstExpression_t* iffalse;
};

struct ApeAstIdentExpr_t
{
    ApeContext_t* context;
    char*         value;
    ApePosition_t pos;
};

struct ApeAstDefineExpr_t
{
    ApeAstIdentExpr_t*      name;
    ApeAstExpression_t* value;
    bool             assignable;
};

struct ApeAstIfExpr_t
{
    ApePtrArray_t*  cases;
    ApeAstBlockExpr_t* alternative;
};

struct ApeAstWhileLoopExpr_t
{
    ApeAstExpression_t* test;
    ApeAstBlockExpr_t*  body;
};

struct ApeAstForeachExpr_t
{
    ApeAstIdentExpr_t*      iterator;
    ApeAstExpression_t* source;
    ApeAstBlockExpr_t*  body;
};

struct ApeAstForExpr_t
{
    ApeAstExpression_t* init;
    ApeAstExpression_t* test;
    ApeAstExpression_t* update;
    ApeAstBlockExpr_t*  body;
};

struct ApeAstIncludeExpr_t
{
    char*            path;
    ApeAstExpression_t* left;
};

struct ApeAstRecoverExpr_t
{
    ApeAstIdentExpr_t*     errorident;
    ApeAstBlockExpr_t* body;
};

struct ApeAstLiteralMapExpr_t
{
    ApePtrArray_t* keys;
    ApePtrArray_t* values;
};


struct ApeAstExpression_t
{
    ApeContext_t* context;
    ApeAstExprType_t extype;
    bool          stringwasallocd;
    ApeSize_t     stringlitlength;
    union
    {
        ApeAstIdentExpr_t*        exident;
        ApeFloat_t         exliteralnumber;
        bool               exliteralbool;
        char*              exliteralstring;
        ApePtrArray_t*     exarray;
        ApeAstLiteralMapExpr_t    exmap;
        ApeAstPrefixExpr_t    exprefix;
        ApeAstInfixExpr_t     exinfix;
        ApeAstLiteralFuncExpr_t     exliteralfunc;
        ApeAstCallExpr_t      excall;
        ApeAstIndexExpr_t     exindex;
        ApeAstAssignExpr_t    exassign;
        ApeAstLogicalExpr_t   exlogical;
        ApeAstTernaryExpr_t   externary;
        ApeAstDefineExpr_t    exdefine;
        ApeAstIfExpr_t        exifstmt;
        ApeAstExpression_t*   exreturn;
        ApeAstExpression_t*   exexpression;
        ApeAstWhileLoopExpr_t exwhilestmt;
        ApeAstForeachExpr_t   exforeachstmt;
        ApeAstForExpr_t   exforstmt;
        ApeAstBlockExpr_t*    exblock;
        ApeAstIncludeExpr_t   exincludestmt;
        ApeAstRecoverExpr_t   exrecoverstmt;
    };
    ApePosition_t pos;
};

struct ApeAstParser_t
{
    ApeContext_t*                  context;
    const ApeConfig_t*             config;
    ApeAstLexer_t                     lexer;
    ApeErrorList_t*                errors;
    ApeSize_t                      depth;
};

struct ApeSymbol_t
{
    ApeContext_t*   context;
    ApeSymbolType_t symtype;
    char*           name;
    ApeSize_t       index;
    bool            assignable;
};

struct ApeAstBlockScope_t
{
    ApeContext_t* context;
    ApeStrDict_t* store;
    ApeInt_t      offset;
    ApeSize_t     numdefinitions;
};

struct ApeSymTable_t
{
    ApeContext_t*     context;
    ApeSymTable_t*    outer;
    ApeGlobalStore_t* globalstore;
    ApePtrArray_t*    blockscopes;
    ApePtrArray_t*    freesymbols;
    ApePtrArray_t*    modglobalsymbols;
    ApeSize_t         maxnumdefinitions;
    ApeInt_t          modglobaloffset;
};

struct ApeOpcodeDef_t
{
    const char* name;
    ApeSize_t   operandcount;
    ApeInt_t    operandwidths[2];
};


struct ApeModule_t
{
    ApeContext_t*  context;
    char*          name;
    ApePtrArray_t* symbols;
};

struct ApeAstFileScope_t
{
    ApeContext_t*  context;
    ApeAstParser_t*   parser;
    ApeSymTable_t* symtable;
    ApeAstCompFile_t* file;
    ApePtrArray_t* loadedmodulenames;
};

struct ApeAstCompResult_t
{
    ApeContext_t*  context;
    ApeUShort_t*   bytecode;
    ApePosition_t* srcpositions;
    ApeSize_t      count;
};

struct ApeAstCompScope_t
{
    ApeContext_t*   context;
    ApeAstCompScope_t* outer;
    ApeValArray_t*  bytecode;
    ApeValArray_t*  srcpositions;
    ApeValArray_t*  breakipstack;
    ApeValArray_t*  continueipstack;
    ApeOpByte_t     lastopcode;
};

struct ApeAstCompiler_t
{
    ApeContext_t*      context;
    const ApeConfig_t* config;
    ApeGCMemory_t*     mem;
    ApeErrorList_t*    errors;
    ApePtrArray_t*     files;
    ApeGlobalStore_t*  globalstore;
    ApeValArray_t*     constants;
    ApeAstCompScope_t*    compilation_scope;
    ApePtrArray_t*     filescopes;
    ApeValArray_t*     srcpositionsstack;
    ApeStrDict_t*      modules;
    ApeStrDict_t*      stringconstantspositions;
};

struct ApeAstProgram_t
{
    ApeContext_t*    context;
    ApeAstCompResult_t* comp_res;
};

struct ApeGlobalStore_t
{
    ApeContext_t*  context;
    ApeStrDict_t*  symbols;
    ApeValArray_t* objects;
};

struct ApeGCObjData_t
{
    ApeContext_t*  context;
    ApeGCMemory_t* mem;
    union
    {
        bool                valbool;
        ApeFloat_t          valnum;
        ApeObjString_t      valstring;
        ApeObjError_t       valerror;
        ApeValArray_t*      valarray;
        ApeValDict_t*       valmap;
        ApeScriptFunction_t valscriptfunc;
        ApeNativeFunction_t valnatfunc;
        ApeExternalData_t   valextern;
    };
    bool         gcmark;
    ApeObjType_t datatype;
};

struct ApeGCObjPool_t
{
    ApeGCObjData_t* datapool[APE_CONF_SIZE_GCMEM_POOLSIZE];
    ApeSize_t     count;
};

struct ApeGCMemory_t
{
    ApeContext_t*   context;
    ApeAllocator_t* alloc;
    ApeSize_t       allocations_since_sweep;
    ApeGCObjData_t** frontobjects;
    ApeGCObjData_t** backobjects;
    ApeValArray_t*  objects_not_gced;
    ApeGCObjPool_t    data_only_pool;
    ApeGCObjPool_t    pools[APE_CONF_SIZE_GCMEM_POOLCOUNT];
};

struct ApeFrame_t
{
    ApeObject_t    function;
    ApeInt_t       ip;
    ApeInt_t       basepointer;
    ApePosition_t* srcpositions;
    ApeUShort_t*   bytecode;
    ApeInt_t       srcip;
    ApeSize_t      bcsize;
    ApeInt_t       recoverip;
    bool           isrecovering;
};

struct ApeVM_t
{
    ApeContext_t*      context;
    ApeAllocator_t*    alloc;
    const ApeConfig_t* config;
    ApeGCMemory_t*     mem;
    ApeErrorList_t*    errors;
    ApeGlobalStore_t*  globalstore;

    ApeObject_t overloadkeys[APE_OPCODE_MAX];

    ApeValDict_t* globalobjects;

    //ApeObject_t stackobjects[APE_CONF_SIZE_VM_STACK];
    ApeValDict_t* stackobjects;

    int         stackptr;

    ApeObject_t thisobjects[APE_CONF_SIZE_VM_THISSTACK];
    int         thisptr;

    ApeFrame_t frameobjects[APE_CONF_SIZE_MAXFRAMES];
    
    ApeSize_t  countframes;

    ApeObject_t lastpopped;
    ApeFrame_t* currentframe;

    bool running;
};

struct ApeConfig_t
{
    struct
    {
        struct
        {
            ApeIOStdoutWriteFunc_t write;
            void*                  context;
        } write;
    } stdio;

    struct
    {
        struct
        {
            ApeIOReadFunc_t fnreadfile;
            void*           context;
        } ioreader;

        struct
        {
            ApeIOWriteFunc_t fnwritefile;
            void*            context;
        } iowriter;
    } fileio;

    /* allows redefinition of symbols */
    bool   replmode;
    double max_execution_time_ms;
    bool   max_execution_time_set;
    bool   runafterdump;
    bool   dumpast;
    bool   dumpbytecode;
    bool   dumpstack;
};


struct ApeContext_t
{
    ApeAllocator_t alloc;

    /* a writer instance that writes to stderr. use with ape_context_debug* */
    ApeWriter_t* debugwriter;
    ApeWriter_t* stdoutwriter;

    /* object memory */
    ApeGCMemory_t* mem;

    /* files being compiled, whether as main script, or include(), import(), etc */
    ApePtrArray_t* files;

    /* array that stores pointers to pseudoclasses */
    ApePtrArray_t* pseudoclasses;

    /* contains the typemapping of pseudoclasses where key is the typename */
    ApeStrDict_t* classmapping;

    /* globally defined object */
    ApeGlobalStore_t* globalstore;

    /* the main compiler instance - may spawn additional compiler instances */
    ApeAstCompiler_t* compiler;

    /* the main VM instance - may spawn additional VM instances */
    ApeVM_t* vm;

    /* the current list of errors (if any) */
    ApeErrorList_t errors;

    /* the runtime configuration */
    ApeConfig_t config;

    /* allocator that abstract malloc/free - TODO: not used right now, because it's broken */
    ApeAllocator_t custom_allocator;

    /**
    * stored member function mapping. string->funcpointer
    */

    /* string member functions */
    ApeStrDict_t* objstringfuncs;

    /* array member functions */
    ApeStrDict_t* objarrayfuncs;
};

/*
#ifdef __cplusplus
APE_EXTERNC_BEGIN
#endif
*/

static APE_INLINE ApeObject_t object_make_from_data(ApeContext_t* ctx, ApeObjType_t type, ApeGCObjData_t* data)
{
    ApeObject_t object;
    object.type = type;
    data->context = ctx;
    data->datatype = type;
    object.handle = data;
    object.handle->datatype = type;
    return object;
}

#include "prot.inc"

/*
#ifdef __cplusplus
APE_EXTERNC_END
#endif
*/
