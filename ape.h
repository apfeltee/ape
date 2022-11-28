
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
    #define APE_EXTERNC_BEGIN extern "C" {
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

//#include <sys/time.h>
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


#define APE_CONF_INVALID_VALDICT_IX UINT_MAX
#define APE_CONF_INVALID_STRDICT_IX UINT_MAX
#define APE_CONF_DICT_INITIAL_SIZE (16)
#define APE_CONF_SIZE_VM_STACK (1024)
#define APE_CONF_SIZE_VM_MAXGLOBALS (512*4)
#define APE_CONF_SIZE_MAXFRAMES (512*4)
#define APE_CONF_SIZE_VM_THISSTACK (512*4)
#define APE_CONF_SIZE_GCMEM_POOLSIZE (512*4)
#define APE_CONF_SIZE_GCMEM_POOLCOUNT ((3) + 1)
#define APE_CONF_CONST_GCMEM_SWEEPINTERVAL (128/64)

#define APE_CONF_SIZE_NATFN_MAXDATALEN (16*2)
#define APE_CONF_SIZE_STRING_BUFSIZE (4)

#define APE_CONF_SIZE_ERRORS_MAXCOUNT (16)
#define APE_CONF_SIZE_ERROR_MAXMSGLENGTH (128)


#define valdict_make(ctx, key_type, val_type) \
    ape_make_valdict_actual(ctx, sizeof(key_type), sizeof(val_type))

#define array_make(allocator, type) \
    ape_make_valarray_actual(allocator, sizeof(type))

#define APE_STREQ(a, b) (strcmp((a), (b)) == 0)
#define APE_STRNEQ(a, b, n) (strncmp((a), (b), (n)) == 0)
#define APE_ARRAY_LEN(array) ((int)(sizeof(array) / sizeof(array[0])))

#if 1
#define APE_DBLEQ(a, b) \
    (fabs((a) - (b)) < DBL_EPSILON)
#else
    #define APE_DBLEQ(a, b) \
        ((a) == (b))
#endif

#define APE_CALL(vm, function_name, ...) \
    vm_call(vm, (function_name), sizeof((ApeObject_t[]){ __VA_ARGS__ }) / sizeof(ApeObject_t), (ApeObject_t[]){ __VA_ARGS__ })

#define APE_ASSERT(x) assert((x))

#ifdef APE_DEBUG
    #define APE_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
    #define APE_LOG(...) ape_log(APE_FILENAME, __LINE__, __VA_ARGS__)
#else
    #define APE_LOG(...) ((void)0)
#endif

#ifdef COLLECTIONS_DEBUG
    #define COLLECTIONS_ASSERT(x) assert(x)
#else
    #define COLLECTIONS_ASSERT(x)
#endif

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

/**/
#define ape_object_value_type(obj) \
    ((obj).handle->type)

#define ape_object_value_istype(o, t) \
    (ape_object_value_type(o) == (t))

#define ape_object_value_isnumber(o) \
    ape_object_value_istype(o, APE_OBJECT_NUMBER)

#define ape_object_value_isnumeric(o) \
    (ape_object_value_istype(o, APE_OBJECT_NUMBER) || ape_object_value_istype(o, APE_OBJECT_BOOL))

#define ape_object_value_isnull(o) \
    ape_object_value_istype(o, APE_OBJECT_NULL)

#define ape_object_value_isstring(o) \
    ape_object_value_istype(o, APE_OBJECT_STRING)

#define ape_object_value_iscallable(o) \
    (ape_object_value_istype(o, APE_OBJECT_NATIVE_FUNCTION) || ape_object_value_istype(o, APE_OBJECT_FUNCTION))


enum ApeErrorType_t
{
    APE_ERROR_NONE = 0,
    APE_ERROR_PARSING,
    APE_ERROR_COMPILATION,
    APE_ERROR_RUNTIME,
    APE_ERROR_TIMEOUT,
    APE_ERROR_ALLOCATION,
    APE_ERROR_USER,// from ape_add_error() or ape_add_errorf()
};

enum ApeObjType_t
{
    APE_OBJECT_NONE = 0,
    APE_OBJECT_ERROR = 1 << 0,
    APE_OBJECT_NUMBER = 1 << 1,
    APE_OBJECT_BOOL = 1 << 2,
    APE_OBJECT_STRING = 1 << 3,
    APE_OBJECT_NULL = 1 << 4,
    APE_OBJECT_NATIVE_FUNCTION = 1 << 5,
    APE_OBJECT_ARRAY = 1 << 6,
    APE_OBJECT_MAP = 1 << 7,
    APE_OBJECT_FUNCTION = 1 << 8,
    APE_OBJECT_EXTERNAL = 1 << 9,
    APE_OBJECT_FREED = 1 << 10,
    APE_OBJECT_ANY = 0xffff,// for checking types with &
};

enum ApeTokenType_t
{
    TOKEN_INVALID = 0,
    TOKEN_EOF,

    // Operators
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

    // Delimiters
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

    // Keywords
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
    TOKEN_KWIMPORT,
    TOKEN_KWRECOVER,

    // Identifiers and literals
    TOKEN_VALIDENT,
    TOKEN_VALNUMBER,
    TOKEN_VALSTRING,
    TOKEN_VALTPLSTRING,

    // MUST be last
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

enum ApeExprType_t
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
};

enum ApeStmtType_t
{
    APE_STMT_NONE,
    APE_STMT_DEFINE,
    APE_STMT_IF,
    APE_STMT_RETURNVALUE,
    APE_STMT_EXPRESSION,
    APE_STMT_WHILELOOP,
    APE_STMT_BREAK,
    APE_STMT_CONTINUE,
    APE_STMT_FOREACH,
    APE_STMT_FORLOOP,
    APE_STMT_BLOCK,
    APE_STMT_IMPORT,
    APE_STMT_RECOVER,
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
    APE_OPCODE_MAX,
};

enum ApePrecedence_t
{
    PRECEDENCE_LOWEST = 0,
    PRECEDENCE_ASSIGN,// a = b
    PRECEDENCE_TERNARY,// a ? b : c
    PRECEDENCE_LOGICAL_OR,// ||
    PRECEDENCE_LOGICAL_AND,// &&
    PRECEDENCE_BIT_OR,// |
    PRECEDENCE_BIT_XOR,// ^
    PRECEDENCE_BIT_AND,// &
    PRECEDENCE_EQUALS,// == !=
    PRECEDENCE_LESSGREATER,// >, >=, <, <=
    PRECEDENCE_SHIFT,// << >>
    PRECEDENCE_SUM,// + -
    PRECEDENCE_PRODUCT,// * / %
    PRECEDENCE_PREFIX,// -x !x ++x --x
    PRECEDENCE_INCDEC,// x++ x--
    PRECEDENCE_POSTFIX,// myFunction(x) x["foo"] x.foo
    PRECEDENCE_HIGHEST
};

typedef uint8_t ApeOpByte_t;
typedef double ApeFloat_t;
typedef int64_t ApeInt_t;
typedef uint64_t ApeUInt_t;
typedef int8_t ApeShort_t;
typedef uint8_t ApeUShort_t;
typedef size_t ApeSize_t;

typedef enum /**/ApeTokenType_t ApeTokenType_t;
typedef enum /**/ApeStmtType_t ApeStmtType_t;
typedef enum /**/ApeOperator_t ApeOperator_t;
typedef enum /**/ApeSymbolType_t ApeSymbolType_t;
typedef enum /**/ApeExprType_t ApeExprType_t;
typedef enum /**/ApeOpcodeValue_t ApeOpcodeValue_t;
typedef enum /**/ApePrecedence_t ApePrecedence_t;
typedef enum /**/ApeObjType_t ApeObjType_t;
typedef enum /**/ApeErrorType_t ApeErrorType_t;
typedef struct /**/ApeContext_t ApeContext_t;
typedef struct /**/ApeProgram_t ApeProgram_t;
typedef struct /**/ApeVM_t ApeVM_t;
typedef struct /**/ApeConfig_t ApeConfig_t;
typedef struct /**/ApePosition_t ApePosition_t;
typedef struct /**/ApeTimer_t ApeTimer_t;
typedef struct /**/ApeAllocator_t ApeAllocator_t;
typedef struct /**/ApeError_t ApeError_t;
typedef struct /**/ApeErrorList_t ApeErrorList_t;
typedef struct /**/ApeToken_t ApeToken_t;
typedef struct /**/ApeCompiledFile_t ApeCompiledFile_t;
typedef struct /**/ApeLexer_t ApeLexer_t;
typedef struct /**/ApeCodeblock_t ApeCodeblock_t;
typedef struct /**/ApeMapLiteral_t ApeMapLiteral_t;
typedef struct /**/ApePrefixExpr_t ApePrefixExpr_t;
typedef struct /**/ApeInfixExpr_t ApeInfixExpr_t;
typedef struct /**/ApeIfCase_t ApeIfCase_t;
typedef struct /**/ApeFnLiteral_t ApeFnLiteral_t;
typedef struct /**/ApeCallExpr_t ApeCallExpr_t;
typedef struct /**/ApeIndexExpr_t ApeIndexExpr_t;
typedef struct /**/ApeAssignExpr_t ApeAssignExpr_t;
typedef struct /**/ApeLogicalExpr_t ApeLogicalExpr_t;
typedef struct /**/ApeTernaryExpr_t ApeTernaryExpr_t;
typedef struct /**/ApeIdent_t ApeIdent_t;
typedef struct /**/ApeExpression_t ApeExpression_t;
typedef struct /**/ApeDefineStmt_t ApeDefineStmt_t;
typedef struct /**/ApeIfStmt_t ApeIfStmt_t;
typedef struct /**/ApeWhileLoopStmt_t ApeWhileLoopStmt_t;
typedef struct /**/ApeForeachStmt_t ApeForeachStmt_t;
typedef struct /**/ApeForLoopStmt_t ApeForLoopStmt_t;
typedef struct /**/ApeImportStmt_t ApeImportStmt_t;
typedef struct /**/ApeRecoverStmt_t ApeRecoverStmt_t;
typedef struct /**/ApeStatement_t ApeStatement_t;
typedef struct /**/ApeParser_t ApeParser_t;
typedef struct /**/ApeObject_t ApeObject_t;
typedef struct /**/ApeScriptFunction_t ApeScriptFunction_t;
typedef struct /**/ApeNativeFunction_t ApeNativeFunction_t;
typedef struct /**/ApeExternalData_t ApeExternalData_t;
typedef struct /**/ApeObjError_t ApeObjError_t;
typedef struct /**/ApeObjString_t ApeObjString_t;
typedef struct /**/ApeObjData_t ApeObjData_t;
typedef struct /**/ApeSymbol_t ApeSymbol_t;
typedef struct /**/ApeBlockScope_t ApeBlockScope_t;
typedef struct /**/ApeSymTable_t ApeSymTable_t;
typedef struct /**/ApeOpcodeDef_t ApeOpcodeDef_t;
typedef struct /**/ApeCompResult_t ApeCompResult_t;
typedef struct /**/ApeCompScope_t ApeCompScope_t;
typedef struct /**/ApeObjPool_t ApeObjPool_t;
typedef struct /**/ApeGCMemory_t ApeGCMemory_t;
typedef struct /**/ApeTracebackItem_t ApeTracebackItem_t;
typedef struct /**/ApeTraceback_t ApeTraceback_t;
typedef struct /**/ApeFrame_t ApeFrame_t;
typedef struct /**/ApeValDict_t ApeValDict_t;
typedef struct /**/ApeStrDict_t ApeStrDict_t;
typedef struct /**/ApeValArray_t ApeValArray_t;
typedef struct /**/ApePtrArray_t ApePtrArray_t;
typedef struct /**/ApeWriter_t ApeWriter_t;
typedef struct /**/ApeGlobalStore_t ApeGlobalStore_t;
typedef struct /**/ApeModule_t ApeModule_t;
typedef struct /**/ApeFileScope_t ApeFileScope_t;
typedef struct /**/ApeCompiler_t ApeCompiler_t;
typedef struct /**/ApeNativeFuncWrapper_t ApeNativeFuncWrapper_t;


typedef ApeObject_t (*ApeWrappedNativeFunc_t)(ApeContext_t*, void*, ApeSize_t, ApeObject_t*);
typedef ApeObject_t (*ApeNativeFunc_t)(ApeVM_t*, void*, ApeSize_t, ApeObject_t*);

typedef size_t (*ApeIOStdoutWriteFunc_t)(void*, const void*, size_t);
typedef char* (*ApeIOReadFunc_t)(void*, const char*);
typedef size_t (*ApeIOWriteFunc_t)(void* context, const char*, const char*, size_t);
typedef size_t (*ApeIOFlushFunc_t)(void* context, const char*, const char*, size_t);

typedef size_t (*ApeWriterWriteFunc_t)(ApeContext_t*, void*, const char*, size_t);
typedef void   (*ApeWriterFlushFunc_t)(ApeContext_t*, void*);

typedef void* (*ApeMemAllocFunc_t)(void*, size_t);
typedef void (*ApeMemFreeFunc_t)(void*, void*);
typedef unsigned long (*ApeDataHashFunc_t)(const void*);
typedef bool (*ApeDataEqualsFunc_t)(const void*, const void*);
typedef void* (*ApeDataCallback_t)(void*);


typedef ApeExpression_t* (*ApeRightAssocParseFNCallback_t)(ApeParser_t* p);
typedef ApeExpression_t* (*ApeLeftAssocParseFNCallback_t)(ApeParser_t* p, ApeExpression_t* expr);


struct ApeScriptFunction_t
{
    ApeObject_t* free_vals_allocated;
    union
    {
        char* name;
        const char* const_name;
    };
    ApeCompResult_t* comp_result;
    ApeSize_t num_locals;
    ApeSize_t num_args;
    ApeSize_t free_vals_count;
    bool owns_data;
};


struct ApeExternalData_t
{
    void* data;
    ApeDataCallback_t data_destroy_fn;
    ApeDataCallback_t data_copy_fn;
};

struct ApeObjError_t
{
    char* message;
    ApeTraceback_t* traceback;
};

struct ApeObjString_t
{
    union
    {
        char* value_allocated;
        char value_buf[APE_CONF_SIZE_STRING_BUFSIZE];
    };
    unsigned long hash;
    bool is_allocated;
    ApeSize_t capacity;
    ApeSize_t length;
};

struct ApeNativeFunction_t
{
    char* name;
    ApeNativeFunc_t native_funcptr;

    void* data;
    ApeSize_t data_len;
};


struct ApeNativeFuncWrapper_t
{
    ApeWrappedNativeFunc_t wrapped_funcptr;
    ApeContext_t* context;
    void* data;
};

struct ApeObjData_t
{
    ApeContext_t* context; 
    ApeGCMemory_t* mem;
    union
    {
        bool valbool;
        ApeFloat_t valnum;
        ApeObjString_t valstring;
        ApeObjError_t valerror;
        ApeValArray_t* valarray;
        ApeValDict_t* valmap;
        ApeScriptFunction_t valscriptfunc;
        ApeNativeFunction_t valnatfunc;
        ApeExternalData_t valextern;
    };
    bool gcmark;
    ApeObjType_t type;
};

struct ApeObject_t
{
    ApeObjType_t type;
    ApeObjData_t* handle;
    
};

struct ApeObjPool_t
{
    ApeObjData_t* datapool[APE_CONF_SIZE_GCMEM_POOLSIZE];
    ApeSize_t count;
};

struct ApePosition_t
{
    const ApeCompiledFile_t* file;
    int line;
    int column;
};

struct ApeConfig_t
{
    struct
    {
        struct
        {
            ApeIOStdoutWriteFunc_t write;
            void* context;
        } write;
    } stdio;

    struct
    {
        struct
        {
            ApeIOReadFunc_t read_file;
            void* context;
        } read_file;

        struct
        {
            ApeIOWriteFunc_t write_file;
            void* context;
        } write_file;
    } fileio;

    bool replmode;// allows redefinition of symbols
    double max_execution_time_ms;
    bool max_execution_time_set;
    bool dumpast;
    bool dumpbytecode;
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

struct ApeAllocator_t
{
    ApeMemAllocFunc_t malloc;
    ApeMemFreeFunc_t free;
    void* ctx;
};

struct ApeError_t
{
    ApeErrorType_t type;
    char message[APE_CONF_SIZE_ERROR_MAXMSGLENGTH];
    ApePosition_t pos;
    ApeTraceback_t* traceback;
};

struct ApeErrorList_t
{
    ApeError_t errors[APE_CONF_SIZE_ERRORS_MAXCOUNT];
    ApeSize_t count;
};

struct ApeToken_t
{
    ApeTokenType_t type;
    const char* literal;
    ApeSize_t len;
    ApePosition_t pos;
};

struct ApeCompiledFile_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    char* dirpath;
    char* path;
    ApePtrArray_t * lines;
};

struct ApeLexer_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    ApeErrorList_t* errors;
    const char* input;
    ApeSize_t inputlen;
    ApeSize_t position;
    ApeSize_t nextposition;
    char ch;
    ApeInt_t line;
    ApeInt_t column;
    ApeCompiledFile_t* file;
    bool failed;
    bool continue_template_string;
    struct
    {
        ApeSize_t position;
        ApeSize_t nextposition;
        char ch;
        ApeInt_t line;
        ApeInt_t column;
    } prevtokenstate;
    ApeToken_t prevtoken;
    ApeToken_t curtoken;
    ApeToken_t peektoken;
};

struct ApeCodeblock_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    ApePtrArray_t * statements;
};

struct ApeMapLiteral_t
{
    ApePtrArray_t * keys;
    ApePtrArray_t * values;
};


struct ApePrefixExpr_t
{
    ApeOperator_t op;
    ApeExpression_t* right;
};

struct ApeInfixExpr_t
{
    ApeOperator_t op;
    ApeExpression_t* left;
    ApeExpression_t* right;
};

struct ApeIfCase_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    ApeExpression_t* test;
    ApeCodeblock_t* consequence;
};

struct ApeFnLiteral_t
{
    char* name;
    ApePtrArray_t * params;
    ApeCodeblock_t* body;
};

struct ApeCallExpr_t
{
    ApeExpression_t* function;
    ApePtrArray_t * args;
};

struct ApeIndexExpr_t
{
    ApeExpression_t* left;
    ApeExpression_t* index;
};

struct ApeAssignExpr_t
{
    ApeExpression_t* dest;
    ApeExpression_t* source;
    bool ispostfix;
};

struct ApeLogicalExpr_t
{
    ApeOperator_t op;
    ApeExpression_t* left;
    ApeExpression_t* right;
};

struct ApeTernaryExpr_t
{
    ApeExpression_t* test;
    ApeExpression_t* iftrue;
    ApeExpression_t* iffalse;
};

struct ApeIdent_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    char* value;
    ApePosition_t pos;
};

struct ApeExpression_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    ApeExprType_t type;
    union
    {
        ApeIdent_t* ident;
        ApeFloat_t numberliteral;
        bool boolliteral;
        char* stringliteral;
        ApePtrArray_t * array;
        ApeMapLiteral_t map;
        ApePrefixExpr_t prefix;
        ApeInfixExpr_t infix;
        ApeFnLiteral_t fnliteral;
        ApeCallExpr_t callexpr;
        ApeIndexExpr_t indexexpr;
        ApeAssignExpr_t assign;
        ApeLogicalExpr_t logical;
        ApeTernaryExpr_t ternary;
    };
    ApePosition_t pos;
};

struct ApeDefineStmt_t
{
    ApeIdent_t* name;
    ApeExpression_t* value;
    bool assignable;
};

struct ApeIfStmt_t
{
    ApePtrArray_t * cases;
    ApeCodeblock_t* alternative;
};

struct ApeWhileLoopStmt_t
{
    ApeExpression_t* test;
    ApeCodeblock_t* body;
};

struct ApeForeachStmt_t
{
    ApeIdent_t* iterator;
    ApeExpression_t* source;
    ApeCodeblock_t* body;
};

struct ApeForLoopStmt_t
{
    ApeStatement_t* init;
    ApeExpression_t* test;
    ApeExpression_t* update;
    ApeCodeblock_t* body;
};

struct ApeImportStmt_t
{
    char* path;
};

struct ApeRecoverStmt_t
{
    ApeIdent_t* errorident;
    ApeCodeblock_t* body;
};

struct ApeStatement_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    ApeStmtType_t type;
    union
    {
        ApeDefineStmt_t define;
        ApeIfStmt_t ifstatement;
        ApeExpression_t* returnvalue;
        ApeExpression_t* expression;
        ApeWhileLoopStmt_t whileloop;
        ApeForeachStmt_t foreach;
        ApeForLoopStmt_t forloop;
        ApeCodeblock_t* block;
        ApeImportStmt_t import;
        ApeRecoverStmt_t recover;
    };
    ApePosition_t pos;
};

struct ApeParser_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    const ApeConfig_t* config;
    ApeLexer_t lexer;
    ApeErrorList_t* errors;
    ApeRightAssocParseFNCallback_t rightassocparsefns[TOKEN_TYPE_MAX];
    ApeLeftAssocParseFNCallback_t leftassocparsefns[TOKEN_TYPE_MAX];
    ApeSize_t depth;
};

struct ApeSymbol_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    ApeSymbolType_t type;
    char* name;
    ApeInt_t index;
    bool assignable;
};

struct ApeBlockScope_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    ApeStrDict_t * store;
    ApeInt_t offset;
    ApeSize_t numdefinitions;
};

struct ApeSymTable_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    ApeSymTable_t* outer;
    ApeGlobalStore_t* globalstore;
    ApePtrArray_t * blockscopes;
    ApePtrArray_t * freesymbols;
    ApePtrArray_t * module_global_symbols;
    ApeSize_t maxnumdefinitions;
    ApeInt_t module_global_offset;
};

struct ApeOpcodeDef_t
{
    const char* name;
    ApeSize_t num_operands;
    ApeInt_t operand_widths[2];
};

struct ApeCompResult_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    ApeUShort_t* bytecode;
    ApePosition_t* srcpositions;
    ApeSize_t count;
};

struct ApeCompScope_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    ApeCompScope_t* outer;
    ApeValArray_t * bytecode;
    ApeValArray_t * srcpositions;
    ApeValArray_t * breakipstack;
    ApeValArray_t * continueipstack;
    ApeOpByte_t lastopcode;
};

struct ApeGCMemory_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    ApeSize_t allocations_since_sweep;
    ApePtrArray_t * objects;
    ApePtrArray_t * objects_back;
    ApeValArray_t * objects_not_gced;
    ApeObjPool_t data_only_pool;
    ApeObjPool_t pools[APE_CONF_SIZE_GCMEM_POOLCOUNT];
};

struct ApeTracebackItem_t
{
    char* function_name;
    ApePosition_t pos;
};

struct ApeTraceback_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    ApeValArray_t * items;
};

struct ApeFrame_t
{
    ApeObject_t function;
    ApeInt_t ip;
    ApeInt_t base_pointer;
    ApePosition_t* srcpositions;
    ApeUShort_t* bytecode;
    ApeInt_t src_ip;
    ApeSize_t bytecode_size;
    ApeInt_t recover_ip;
    bool is_recovering;
};

struct ApeVM_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    const ApeConfig_t* config;
    ApeGCMemory_t* mem;
    ApeErrorList_t* errors;
    ApeGlobalStore_t* globalstore;
    ApeObject_t globals[APE_CONF_SIZE_VM_MAXGLOBALS];
    ApeSize_t globals_count;
    ApeObject_t stack[APE_CONF_SIZE_VM_STACK];
    int sp;
    ApeObject_t this_stack[APE_CONF_SIZE_VM_THISSTACK];
    int this_sp;
    ApeFrame_t frames[APE_CONF_SIZE_MAXFRAMES];
    ApeSize_t frames_count;
    ApeObject_t last_popped;
    ApeFrame_t* current_frame;
    bool running;
    ApeObject_t operator_oveload_keys[APE_OPCODE_MAX];
};

struct ApeValDict_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    ApeSize_t key_size;
    ApeSize_t val_size;
    unsigned int* cells;
    unsigned long* hashes;
    void* keys;
    void* values;
    unsigned int* cell_ixs;
    ApeSize_t count;
    ApeSize_t item_capacity;
    ApeSize_t cell_capacity;
    ApeDataHashFunc_t _hash_key;
    ApeDataEqualsFunc_t _keys_equals;
    ApeDataCallback_t copy_fn;
    ApeDataCallback_t destroy_fn;
};

struct ApeStrDict_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    unsigned int* cells;
    unsigned long* hashes;
    char** keys;
    void** values;
    unsigned int* cell_ixs;
    ApeSize_t count;
    ApeSize_t item_capacity;
    ApeSize_t cell_capacity;
    ApeDataCallback_t copy_fn;
    ApeDataCallback_t destroy_fn;
};

struct ApeValArray_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    unsigned char* arraydata;
    unsigned char* data_allocated;
    ApeSize_t count;
    ApeSize_t capacity;
    ApeSize_t element_size;
    bool lock_capacity;
};

struct ApePtrArray_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    ApeValArray_t arr;
};

struct ApeWriter_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    bool failed;
    char* stringdata;
    ApeSize_t capacity;
    ApeSize_t len;
    void* iohandle;
    ApeWriterWriteFunc_t iowriter;
    ApeWriterFlushFunc_t ioflusher;
    bool iomustclose;
    bool iomustflush;
};

struct ApeGlobalStore_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    ApeStrDict_t * symbols;
    ApeValArray_t * objects;
};

struct ApeModule_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    char* name;
    ApePtrArray_t * symbols;
};

struct ApeFileScope_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    ApeParser_t* parser;
    ApeSymTable_t* symbol_table;
    ApeCompiledFile_t* file;
    ApePtrArray_t * loadedmodulenames;
};

struct ApeCompiler_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    const ApeConfig_t* config;
    ApeGCMemory_t* mem;
    ApeErrorList_t* errors;
    ApePtrArray_t * files;
    ApeGlobalStore_t* globalstore;
    ApeValArray_t * constants;
    ApeCompScope_t* compilation_scope;
    ApePtrArray_t * filescopes;
    ApeValArray_t * srcpositionsstack;
    ApeStrDict_t * modules;
    ApeStrDict_t * stringconstantspositions;
};


struct ApeProgram_t
{
    ApeContext_t* context;
    ApeCompResult_t* comp_res;
};

struct ApeContext_t
{
    ApeAllocator_t alloc;
    ApeGCMemory_t* mem;
    ApePtrArray_t * files;
    ApeGlobalStore_t* globalstore;
    ApeCompiler_t* compiler;
    ApeVM_t* vm;
    ApeErrorList_t errors;
    ApeConfig_t config;
    ApeAllocator_t custom_allocator;
};


#ifdef __cplusplus
    APE_EXTERNC_BEGIN
#endif


static inline ApeObject_t object_make_from_data(ApeContext_t* ctx, ApeObjType_t type, ApeObjData_t* data)
{
    ApeObject_t object;
    object.type = type;
    data->context = ctx;
    object.handle = data;
    object.handle->type = type;
    return object;
}

#include "prot.inc"

#ifdef __cplusplus
    APE_EXTERNC_END
#endif

