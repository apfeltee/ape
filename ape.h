
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

#define OBJECT_PATTERN 0xfff8000000000000
#define OBJECT_HEADER_MASK 0xffff000000000000
#define OBJECT_ALLOCATED_HEADER 0xfffc000000000000
#define OBJECT_BOOL_HEADER 0xfff9000000000000
#define OBJECT_NULL_PATTERN 0xfffa000000000000

#define VALDICT_INVALID_IX UINT_MAX
#define DICT_INVALID_IX UINT_MAX
#define DICT_INITIAL_SIZE 32
#define VM_STACK_SIZE 2048
#define VM_MAX_GLOBALS 2048
#define VM_MAX_FRAMES 2048
#define VM_THIS_STACK_SIZE 2048
#define GCMEM_POOL_SIZE 2048
#define GCMEM_POOLS_NUM 3
#define GCMEM_SWEEP_INTERVAL 128

#define NATIVE_FN_MAX_DATA_LEN 24
#define OBJECT_STRING_BUF_SIZE 24

#define ERRORS_MAX_COUNT 16
#define APE_ERROR_MESSAGE_MAX_LENGTH 255


#define valdict_make(allocator, key_type, val_type) valdict_make_(allocator, sizeof(key_type), sizeof(val_type))

#define array_make(allocator, type) array_make_(allocator, sizeof(type))

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

#ifdef APE_DEBUG
    #define APE_ASSERT(x) assert((x))
    #define APE_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
    #define APE_LOG(...) ape_log(APE_FILENAME, __LINE__, __VA_ARGS__)
#else
    #define APE_ASSERT(x) ((void)0)
    #define APE_LOG(...) ((void)0)
#endif

#ifdef COLLECTIONS_DEBUG
    #define COLLECTIONS_ASSERT(x) assert(x)
#else
    #define COLLECTIONS_ASSERT(x)
#endif

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

enum ApeObjectType_t
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

    TOKEN_PLUS_ASSIGN,
    TOKEN_MINUS_ASSIGN,
    TOKEN_ASTERISK_ASSIGN,
    TOKEN_SLASH_ASSIGN,
    TOKEN_PERCENT_ASSIGN,
    TOKEN_BIT_AND_ASSIGN,
    TOKEN_BIT_OR_ASSIGN,
    TOKEN_BIT_XOR_ASSIGN,
    TOKEN_LSHIFT_ASSIGN,
    TOKEN_RSHIFT_ASSIGN,

    TOKEN_QUESTION,

    TOKEN_PLUS,
    TOKEN_PLUS_PLUS,
    TOKEN_MINUS,
    TOKEN_MINUS_MINUS,
    TOKEN_BANG,
    TOKEN_ASTERISK,
    TOKEN_SLASH,

    TOKEN_LT,
    TOKEN_LTE,
    TOKEN_GT,
    TOKEN_GTE,

    TOKEN_EQ,
    TOKEN_NOT_EQ,

    TOKEN_AND,
    TOKEN_OR,

    TOKEN_BIT_AND,
    TOKEN_BIT_OR,
    TOKEN_BIT_XOR,
    TOKEN_LSHIFT,
    TOKEN_RSHIFT,

    // Delimiters
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
    TOKEN_COLON,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_DOT,
    TOKEN_PERCENT,

    // Keywords
    TOKEN_FUNCTION,
    TOKEN_CONST,
    TOKEN_VAR,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_RETURN,
    TOKEN_WHILE,
    TOKEN_BREAK,
    TOKEN_FOR,
    TOKEN_IN,
    TOKEN_CONTINUE,
    TOKEN_NULL,
    TOKEN_IMPORT,
    TOKEN_RECOVER,

    // Identifiers and literals
    TOKEN_IDENT,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_TEMPLATE_STRING,

    TOKEN_TYPE_MAX
};


enum ApeOperator_t
{
    OPERATOR_NONE,
    OPERATOR_ASSIGN,
    OPERATOR_PLUS,
    OPERATOR_MINUS,
    OPERATOR_BANG,
    OPERATOR_ASTERISK,
    OPERATOR_SLASH,
    OPERATOR_LT,
    OPERATOR_LTE,
    OPERATOR_GT,
    OPERATOR_GTE,
    OPERATOR_EQ,
    OPERATOR_NOT_EQ,
    OPERATOR_MODULUS,
    OPERATOR_LOGICAL_AND,
    OPERATOR_LOGICAL_OR,
    OPERATOR_BIT_AND,
    OPERATOR_BIT_OR,
    OPERATOR_BIT_XOR,
    OPERATOR_LSHIFT,
    OPERATOR_RSHIFT,
};

enum ApeExprType_t
{
    EXPRESSION_NONE,
    EXPRESSION_IDENT,
    EXPRESSION_NUMBER_LITERAL,
    EXPRESSION_BOOL_LITERAL,
    EXPRESSION_STRING_LITERAL,
    EXPRESSION_NULL_LITERAL,
    EXPRESSION_ARRAY_LITERAL,
    EXPRESSION_MAP_LITERAL,
    EXPRESSION_PREFIX,
    EXPRESSION_INFIX,
    EXPRESSION_FUNCTION_LITERAL,
    EXPRESSION_CALL,
    EXPRESSION_INDEX,
    EXPRESSION_ASSIGN,
    EXPRESSION_LOGICAL,
    EXPRESSION_TERNARY,
};


enum ApeStatementType_t
{
    STATEMENT_NONE,
    STATEMENT_DEFINE,
    STATEMENT_IF,
    STATEMENT_RETURN_VALUE,
    STATEMENT_EXPRESSION,
    STATEMENT_WHILE_LOOP,
    STATEMENT_BREAK,
    STATEMENT_CONTINUE,
    STATEMENT_FOREACH,
    STATEMENT_FOR_LOOP,
    STATEMENT_BLOCK,
    STATEMENT_IMPORT,
    STATEMENT_RECOVER,
};



enum ApeSymbolType_t
{
    SYMBOL_NONE = 0,
    SYMBOL_MODULE_GLOBAL,
    SYMBOL_LOCAL,
    SYMBOL_APE_GLOBAL,
    SYMBOL_FREE,
    SYMBOL_FUNCTION,
    SYMBOL_THIS,
};


enum ApeOpcodeValue_t
{
    OPCODE_NONE = 0,
    OPCODE_CONSTANT,
    OPCODE_ADD,
    OPCODE_POP,
    OPCODE_SUB,
    OPCODE_MUL,
    OPCODE_DIV,
    OPCODE_MOD,
    OPCODE_TRUE,
    OPCODE_FALSE,
    OPCODE_COMPARE,
    OPCODE_COMPARE_EQ,
    OPCODE_EQUAL,
    OPCODE_NOT_EQUAL,
    OPCODE_GREATER_THAN,
    OPCODE_GREATER_THAN_EQUAL,
    OPCODE_MINUS,
    OPCODE_BANG,
    OPCODE_JUMP,
    OPCODE_JUMP_IF_FALSE,
    OPCODE_JUMP_IF_TRUE,
    OPCODE_NULL,
    OPCODE_GET_MODULE_GLOBAL,
    OPCODE_SET_MODULE_GLOBAL,
    OPCODE_DEFINE_MODULE_GLOBAL,
    OPCODE_ARRAY,
    OPCODE_MAP_START,
    OPCODE_MAP_END,
    OPCODE_GET_THIS,
    OPCODE_GET_INDEX,
    OPCODE_SET_INDEX,
    OPCODE_GET_VALUE_AT,
    OPCODE_CALL,
    OPCODE_RETURN_VALUE,
    OPCODE_RETURN,
    OPCODE_GET_LOCAL,
    OPCODE_DEFINE_LOCAL,
    OPCODE_SET_LOCAL,
    OPCODE_GET_APE_GLOBAL,
    OPCODE_FUNCTION,
    OPCODE_GET_FREE,
    OPCODE_SET_FREE,
    OPCODE_CURRENT_FUNCTION,
    OPCODE_DUP,
    OPCODE_NUMBER,
    OPCODE_LEN,
    OPCODE_SET_RECOVER,
    OPCODE_OR,
    OPCODE_XOR,
    OPCODE_AND,
    OPCODE_LSHIFT,
    OPCODE_RSHIFT,
    OPCODE_MAX,
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
typedef int32_t ApeInt_t;
typedef uint32_t ApeUInt_t;
typedef int8_t ApeShort_t;
typedef uint8_t ApeUShort_t;
typedef size_t ApeSize_t;

typedef enum /**/ApeTokenType_t ApeTokenType_t;
typedef enum /**/ApeStatementType_t ApeStatementType_t;
typedef enum /**/ApeOperator_t ApeOperator_t;
typedef enum /**/ApeSymbolType_t ApeSymbolType_t;
typedef enum /**/ApeObjectType_t ApeObjectType_t;
typedef enum /**/ApeExprType_t ApeExprType_t;
typedef enum /**/ApeOpcodeValue_t ApeOpcodeValue_t;
typedef enum /**/ApePrecedence_t ApePrecedence_t;
typedef enum /**/ApeObjectType_t ApeObjectType_t;
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
typedef struct /**/ApeFunction_t ApeFunction_t;
typedef struct /**/ApeNativeFunction_t ApeNativeFunction_t;
typedef struct /**/ApeExternalData_t ApeExternalData_t;
typedef struct /**/ApeObjectError_t ApeObjectError_t;
typedef struct /**/ApeObjectString_t ApeObjectString_t;
typedef struct /**/ApeObjectData_t ApeObjectData_t;
typedef struct /**/ApeSymbol_t ApeSymbol_t;
typedef struct /**/ApeBlockScope_t ApeBlockScope_t;
typedef struct /**/ApeSymbolTable_t ApeSymbolTable_t;
typedef struct /**/ApeOpcodeDefinition_t ApeOpcodeDefinition_t;
typedef struct /**/ApeCompilationResult_t ApeCompilationResult_t;
typedef struct /**/ApeCompilationScope_t ApeCompilationScope_t;
typedef struct /**/ApeObjectDataPool_t ApeObjectDataPool_t;
typedef struct /**/ApeGCMemory_t ApeGCMemory_t;
typedef struct /**/ApeTracebackItem_t ApeTracebackItem_t;
typedef struct /**/ApeTraceback_t ApeTraceback_t;
typedef struct /**/ApeFrame_t ApeFrame_t;
typedef struct /**/ApeValDictionary_t ApeValDictionary_t;
typedef struct /**/ApeDictionary_t ApeDictionary_t;
typedef struct /**/ApeArray_t ApeArray_t;
typedef struct /**/ApePtrDictionary_t ApePtrDictionary_t;
typedef struct /**/ApePtrArray_t ApePtrArray_t;
typedef struct /**/ApeStringBuffer_t ApeStringBuffer_t;
typedef struct /**/ApeGlobalStore_t ApeGlobalStore_t;
typedef struct /**/ApeModule_t ApeModule_t;
typedef struct /**/ApeFileScope_t ApeFileScope_t;
typedef struct /**/ApeCompiler_t ApeCompiler_t;
typedef struct /**/ApeNativeFuncWrapper_t ApeNativeFuncWrapper_t;


typedef ApeObject_t (*ApeWrappedNativeFunc_t)(ApeContext_t*, void*, int, ApeObject_t*);
typedef ApeObject_t (*ApeNativeFunc_t)(ApeVM_t*, void*, int, ApeObject_t*);

typedef size_t (*ApeIOStdoutWriteFunc_t)(void*, const void*, size_t);
typedef char* (*ApeIOReadFunc_t)(void*, const char*);
typedef size_t (*ApeIOWriteFunc_t)(void* context, const char*, const char*, size_t);
typedef void* (*ApeMemAllocFunc_t)(void*, size_t);
typedef void (*ApeMemFreeFunc_t)(void*, void*);
typedef unsigned long (*ApeDataHashFunc_t)(const void*);
typedef bool (*ApeDataEqualsFunc_t)(const void*, const void*);
typedef void* (*ApeDataCallback_t)(void*);


typedef ApeExpression_t* (*ApeRightAssocParseFNCallback_t)(ApeParser_t* p);
typedef ApeExpression_t* (*ApeLeftAssocParseFNCallback_t)(ApeParser_t* p, ApeExpression_t* expr);


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

    bool repl_mode;// allows redefinition of symbols

    double max_execution_time_ms;
    bool max_execution_time_set;
};

struct ApeTimer_t
{
#if defined(APE_POSIX)
    int64_t start_offset;
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
    char message[APE_ERROR_MESSAGE_MAX_LENGTH];
    ApePosition_t pos;
    ApeTraceback_t* traceback;
};

struct ApeErrorList_t
{
    ApeError_t errors[ERRORS_MAX_COUNT];
    int count;
};

struct ApeToken_t
{
    ApeTokenType_t type;
    const char* literal;
    int len;
    ApePosition_t pos;
};

struct ApeCompiledFile_t
{
    ApeAllocator_t* alloc;
    char* dir_path;
    char* path;
    ApePtrArray_t * lines;
};

struct ApeLexer_t
{
    ApeAllocator_t* alloc;
    ApeErrorList_t* errors;
    const char* input;
    int input_len;
    int position;
    int next_position;
    char ch;
    int line;
    int column;
    ApeCompiledFile_t* file;
    bool failed;
    bool continue_template_string;
    struct
    {
        int position;
        int next_position;
        char ch;
        int line;
        int column;
    } prev_token_state;
    ApeToken_t prev_token;
    ApeToken_t cur_token;
    ApeToken_t peek_token;
};

struct ApeCodeblock_t
{
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
    bool is_postfix;
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
    ApeExpression_t* if_true;
    ApeExpression_t* if_false;
};

struct ApeIdent_t
{
    ApeAllocator_t* alloc;
    char* value;
    ApePosition_t pos;
};

struct ApeExpression_t
{
    ApeAllocator_t* alloc;
    ApeExprType_t type;
    union
    {
        ApeIdent_t* ident;
        ApeFloat_t number_literal;
        bool bool_literal;
        char* string_literal;
        ApePtrArray_t * array;
        ApeMapLiteral_t map;
        ApePrefixExpr_t prefix;
        ApeInfixExpr_t infix;
        ApeFnLiteral_t fn_literal;
        ApeCallExpr_t call_expr;
        ApeIndexExpr_t index_expr;
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
    ApeIdent_t* error_ident;
    ApeCodeblock_t* body;
};

struct ApeStatement_t
{
    ApeAllocator_t* alloc;
    ApeStatementType_t type;
    union
    {
        ApeDefineStmt_t define;
        ApeIfStmt_t if_statement;
        ApeExpression_t* return_value;
        ApeExpression_t* expression;
        ApeWhileLoopStmt_t while_loop;
        ApeForeachStmt_t foreach;
        ApeForLoopStmt_t for_loop;
        ApeCodeblock_t* block;
        ApeImportStmt_t import;
        ApeRecoverStmt_t recover;
    };
    ApePosition_t pos;
};

struct ApeParser_t
{
    ApeAllocator_t* alloc;
    const ApeConfig_t* config;
    ApeLexer_t lexer;
    ApeErrorList_t* errors;

    ApeRightAssocParseFNCallback_t right_assoc_parse_fns[TOKEN_TYPE_MAX];
    ApeLeftAssocParseFNCallback_t left_assoc_parse_fns[TOKEN_TYPE_MAX];

    int depth;
};

struct ApeObject_t
{
    uint64_t internal_padding;
    //char internal_padding[64 * 8];
    union
    {
        // assumes no pointer exceeds 48 bits
        //uintptr_t handle;
        uint64_t handle;
        ApeFloat_t number;
    };
};


struct ApeFunction_t
{
    union
    {
        ApeObject_t* free_vals_allocated;
        ApeObject_t free_vals_buf[2];
    };
    union
    {
        char* name;
        const char* const_name;
    };
    ApeCompilationResult_t* comp_result;
    int num_locals;
    int num_args;
    ApeSize_t free_vals_count;
    bool owns_data;
};


struct ApeExternalData_t
{
    void* data;
    ApeDataCallback_t data_destroy_fn;
    ApeDataCallback_t data_copy_fn;
};

struct ApeObjectError_t
{
    char* message;
    ApeTraceback_t* traceback;
};

struct ApeObjectString_t
{
    union
    {
        char* value_allocated;
        char value_buf[OBJECT_STRING_BUF_SIZE];
    };
    unsigned long hash;
    bool is_allocated;
    int capacity;
    int length;
};

struct ApeNativeFunction_t
{
    char* name;
    ApeNativeFunc_t native_funcptr;
    //ApeUShort_t data[NATIVE_FN_MAX_DATA_LEN];
    void* data;
    int data_len;
};


struct ApeNativeFuncWrapper_t
{
    ApeWrappedNativeFunc_t wrapped_funcptr;
    ApeContext_t* ape;
    void* data;
};


struct ApeObjectData_t
{
    ApeGCMemory_t* mem;
    union
    {
        ApeObjectString_t string;
        ApeObjectError_t error;
        ApeArray_t* array;
        ApeValDictionary_t * map;
        ApeFunction_t function;
        ApeNativeFunction_t native_function;
        ApeExternalData_t external;
    };
    bool gcmark;
    ApeObjectType_t type;
};

struct ApeSymbol_t
{
    ApeAllocator_t* alloc;
    ApeSymbolType_t type;
    char* name;
    int index;
    bool assignable;
};

struct ApeBlockScope_t
{
    ApeAllocator_t* alloc;
    ApeDictionary_t * store;
    int offset;
    int num_definitions;
};

struct ApeSymbolTable_t
{
    ApeAllocator_t* alloc;
    ApeSymbolTable_t* outer;
    ApeGlobalStore_t* global_store;
    ApePtrArray_t * block_scopes;
    ApePtrArray_t * free_symbols;
    ApePtrArray_t * module_global_symbols;
    int max_num_definitions;
    int module_global_offset;
};

struct ApeOpcodeDefinition_t
{
    const char* name;
    int num_operands;
    int operand_widths[2];
};

struct ApeCompilationResult_t
{
    ApeAllocator_t* alloc;
    ApeUShort_t* bytecode;
    ApePosition_t* src_positions;
    int count;
};

struct ApeCompilationScope_t
{
    ApeAllocator_t* alloc;
    ApeCompilationScope_t* outer;
    ApeArray_t * bytecode;
    ApeArray_t * src_positions;
    ApeArray_t * break_ip_stack;
    ApeArray_t * continue_ip_stack;
    ApeOpByte_t last_opcode;
};

struct ApeObjectDataPool_t
{
    ApeObjectData_t* datapool[GCMEM_POOL_SIZE];
    int count;
};

struct ApeGCMemory_t
{
    ApeAllocator_t* alloc;
    int allocations_since_sweep;
    ApePtrArray_t * objects;
    ApePtrArray_t * objects_back;
    ApeArray_t * objects_not_gced;
    ApeObjectDataPool_t data_only_pool;
    ApeObjectDataPool_t pools[GCMEM_POOLS_NUM];
};

struct ApeTracebackItem_t
{
    char* function_name;
    ApePosition_t pos;
};

struct ApeTraceback_t
{
    ApeAllocator_t* alloc;
    ApeArray_t * items;
};

struct ApeFrame_t
{
    ApeObject_t function;
    int ip;
    int base_pointer;
    const ApePosition_t* src_positions;
    ApeUShort_t* bytecode;
    int src_ip;
    int bytecode_size;
    int recover_ip;
    bool is_recovering;
};

struct ApeVM_t
{
    ApeAllocator_t* alloc;
    const ApeConfig_t* config;
    ApeGCMemory_t* mem;
    ApeErrorList_t* errors;
    ApeGlobalStore_t* global_store;
    ApeObject_t globals[VM_MAX_GLOBALS];
    ApeSize_t globals_count;
    ApeObject_t stack[VM_STACK_SIZE];
    int sp;
    ApeObject_t this_stack[VM_THIS_STACK_SIZE];
    int this_sp;
    ApeFrame_t frames[VM_MAX_FRAMES];
    ApeSize_t frames_count;
    ApeObject_t last_popped;
    ApeFrame_t* current_frame;
    bool running;
    ApeObject_t operator_oveload_keys[OPCODE_MAX];
};

struct ApeValDictionary_t
{
    ApeAllocator_t* alloc;
    size_t key_size;
    size_t val_size;
    unsigned int* cells;
    unsigned long* hashes;
    void* keys;
    void* values;
    unsigned int* cell_ixs;
    unsigned int count;
    unsigned int item_capacity;
    unsigned int cell_capacity;
    ApeDataHashFunc_t _hash_key;
    ApeDataEqualsFunc_t _keys_equals;
};

struct ApeDictionary_t
{
    ApeAllocator_t* alloc;
    unsigned int* cells;
    unsigned long* hashes;
    char** keys;
    void** values;
    unsigned int* cell_ixs;
    unsigned int count;
    unsigned int item_capacity;
    unsigned int cell_capacity;
    ApeDataCallback_t copy_fn;
    ApeDataCallback_t destroy_fn;
};

struct ApeArray_t
{
    ApeAllocator_t* alloc;
    unsigned char* arraydata;
    unsigned char* data_allocated;
    unsigned int count;
    unsigned int capacity;
    size_t element_size;
    bool lock_capacity;
};

struct ApePtrDictionary_t
{
    ApeAllocator_t* alloc;
    ApeValDictionary_t vd;
};

struct ApePtrArray_t
{
    ApeAllocator_t* alloc;
    ApeArray_t arr;
};

struct ApeStringBuffer_t
{
    ApeAllocator_t* alloc;
    bool failed;
    char* stringdata;
    size_t capacity;
    size_t len;
};

struct ApeGlobalStore_t
{
    ApeAllocator_t* alloc;
    ApeDictionary_t * symbols;
    ApeArray_t * objects;
};

struct ApeModule_t
{
    ApeAllocator_t* alloc;
    char* name;
    ApePtrArray_t * symbols;
};

struct ApeFileScope_t
{
    ApeAllocator_t* alloc;
    ApeParser_t* parser;
    ApeSymbolTable_t* symbol_table;
    ApeCompiledFile_t* file;
    ApePtrArray_t * loaded_module_names;
};

struct ApeCompiler_t
{
    ApeAllocator_t* alloc;
    const ApeConfig_t* config;
    ApeGCMemory_t* mem;
    ApeErrorList_t* errors;
    ApePtrArray_t * files;
    ApeGlobalStore_t* global_store;
    ApeArray_t * constants;
    ApeCompilationScope_t* compilation_scope;
    ApePtrArray_t * file_scopes;
    ApeArray_t * src_positions_stack;
    ApeDictionary_t * modules;
    ApeDictionary_t * string_constants_positions;
};


struct ApeProgram_t
{
    ApeContext_t* ape;
    ApeCompilationResult_t* comp_res;
};

struct ApeContext_t
{
    ApeAllocator_t alloc;
    ApeGCMemory_t* mem;
    ApePtrArray_t * files;
    ApeGlobalStore_t* global_store;
    ApeCompiler_t* compiler;
    ApeVM_t* vm;
    ApeErrorList_t errors;
    ApeConfig_t config;
    ApeAllocator_t custom_allocator;
};


#ifdef __cplusplus
    APE_EXTERNC_BEGIN
#endif

#include "prot.inc"

#ifdef __cplusplus
    APE_EXTERNC_END
#endif

