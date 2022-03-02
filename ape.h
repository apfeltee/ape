
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

#define CHECK_ARGS(vm, generate_error, argc, args, ...) \
    check_args((vm), (generate_error), (argc), (args),  \
               sizeof((ApeObjectType_t[]){ __VA_ARGS__ }) / sizeof(ApeObjectType_t), (ApeObjectType_t[]){ __VA_ARGS__ })


#define APE_CALL(ape, function_name, ...) \
    ape_call((ape), (function_name), sizeof((ApeObject_t[]){ __VA_ARGS__ }) / sizeof(ApeObject_t), (ApeObject_t[]){ __VA_ARGS__ })

#define APE_CHECK_ARGS(ape, generate_error, argc, args, ...) \
    ape_check_args((ape), (generate_error), (argc), (args), sizeof((int[]){ __VA_ARGS__ }) / sizeof(int), (int[]){ __VA_ARGS__ })


#define dict(TYPE) ApeDictionary_t

#define dict_make(alloc, copy_fn, destroy_fn) \
    dict_make_(alloc, (ApeDictItemCopyFNCallback_t)(copy_fn), (ApeDictItemDestroyFNCallback_t)(destroy_fn))

#define valdict(KEY_TYPE, VALUE_TYPE) ApeValDictionary_t

#define valdict_make(allocator, key_type, val_type) valdict_make_(allocator, sizeof(key_type), sizeof(val_type))

#define ptrdict(KEY_TYPE, VALUE_TYPE) ApePtrDictionary_t

#define array(TYPE) ApeArray_t

#define array_make(allocator, type) array_make_(allocator, sizeof(type))
#define array_destroy_with_items(arr, fn) array_destroy_with_items_(arr, (ApeArrayItemDeinitFNCallback_t)(fn))
#define array_clear_and_deinit_items(arr, fn) array_clear_and_deinit_items_(arr, (ApeArrayItemDeinitFNCallback_t)(fn))

#define ptrarray(TYPE) ApePtrArray_t

#define ptrarray_destroy_with_items(arr, fn) ptrarray_destroy_with_items_(arr, (ApePtrArrayItemDestroyFNCallback_t)(fn))
#define ptrarray_clear_and_destroy_items(arr, fn) ptrarray_clear_and_destroy_items_(arr, (ApePtrArrayItemDestroyFNCallback_t)(fn))

#define ptrarray_copy_with_items(arr, copy_fn, destroy_fn) \
    ptrarray_copy_with_items_(arr, (ApePtrArrayItemCopyFNCallback_t)(copy_fn), (ApePtrArrayItemDestroyFNCallback_t)(destroy_fn))


#define APE_STREQ(a, b) (strcmp((a), (b)) == 0)
#define APE_STRNEQ(a, b, n) (strncmp((a), (b), (n)) == 0)
#define APE_ARRAY_LEN(array) ((int)(sizeof(array) / sizeof(array[0])))
#define APE_DBLEQ(a, b) (fabs((a) - (b)) < DBL_EPSILON)

#ifdef APE_DEBUG
    #define APE_ASSERT(x) assert((x))
    #define APE_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
    #define APE_LOG(...) ape_log(APE_FILENAME, __LINE__, __VA_ARGS__)
#else
    #define APE_ASSERT(x) ((void)0)
    #define APE_LOG(...) ((void)0)
#endif

#define COLLECTIONS_AMALGAMATED

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

enum ApeExpr_type_t
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

typedef uint8_t opcode_t;


typedef enum ApeTokenType_t ApeTokenType_t;
typedef enum ApeStatementType_t ApeStatementType_t;
typedef enum ApeOperator_t ApeOperator_t;
typedef enum ApeSymbolType_t ApeSymbolType_t;
typedef enum ApeObjectType_t ApeObjectType_t;
typedef enum ApeExpr_type_t ApeExpr_type_t;
typedef enum ApeOpcodeValue_t ApeOpcodeValue_t;
typedef enum ApePrecedence_t ApePrecedence_t;
typedef enum ApeObjectType_t ApeObjectType_t;
typedef enum ApeErrorType_t ApeErrorType_t;
typedef struct ApeContext_t ApeContext_t;
typedef struct ApeProgram_t ApeProgram_t;
typedef struct ApeVM_t ApeVM_t;
typedef struct ApeConfig_t ApeConfig_t;
typedef struct ApePosition_t ApePosition_t;
typedef struct ApeTimer_t ApeTimer_t;
typedef struct ApeAllocator_t ApeAllocator_t;
typedef struct ApeError_t ApeError_t;
typedef struct ApeErrorList_t ApeErrorList_t;
typedef struct ApeToken_t ApeToken_t;
typedef struct ApeCompiledFile_t ApeCompiledFile_t;
typedef struct ApeLexer_t ApeLexer_t;
typedef struct ApeCodeblock_t ApeCodeblock_t;
typedef struct ApeMapLiteral_t ApeMapLiteral_t;
typedef struct ApePrefixExpr_t ApePrefixExpr_t;
typedef struct ApeInfixExpr_t ApeInfixExpr_t;
typedef struct ApeIfCase_t ApeIfCase_t;
typedef struct ApeFnLiteral_t ApeFnLiteral_t;
typedef struct ApeCallExpr_t ApeCallExpr_t;
typedef struct ApeIndexExpr_t ApeIndexExpr_t;
typedef struct ApeAssignExpr_t ApeAssignExpr_t;
typedef struct ApeLogicalExpr_t ApeLogicalExpr_t;
typedef struct ApeTernaryExpr_t ApeTernaryExpr_t;
typedef struct ApeIdent_t ApeIdent_t;
typedef struct ApeExpression_t ApeExpression_t;
typedef struct ApeDefineStmt_t ApeDefineStmt_t;
typedef struct ApeIfStmt_t ApeIfStmt_t;
typedef struct ApeWhileLoopStmt_t ApeWhileLoopStmt_t;
typedef struct ApeForeachStmt_t ApeForeachStmt_t;
typedef struct ApeForLoopStmt_t ApeForLoopStmt_t;
typedef struct ApeImportStmt_t ApeImportStmt_t;
typedef struct ApeRecoverStmt_t ApeRecoverStmt_t;
typedef struct ApeStatement_t ApeStatement_t;
typedef struct ApeParser_t ApeParser_t;
typedef struct ApeObject_t ApeObject_t;
typedef struct ApeFunction_t ApeFunction_t;
typedef struct ApeNativeFunction_t ApeNativeFunction_t;
typedef struct ApeExternalData_t ApeExternalData_t;
typedef struct ApeObjectError_t ApeObjectError_t;
typedef struct ApeObjectString_t ApeObjectString_t;
typedef struct ApeObjectData_t ApeObjectData_t;
typedef struct ApeSymbol_t ApeSymbol_t;
typedef struct ApeBlockScope_t ApeBlockScope_t;
typedef struct ApeSymbol_table_t ApeSymbol_table_t;
typedef struct ApeOpcodeDefinition_t ApeOpcodeDefinition_t;
typedef struct ApeCompilationResult_t ApeCompilationResult_t;
typedef struct ApeCompilationScope_t ApeCompilationScope_t;
typedef struct ApeObjectDataPool_t ApeObjectDataPool_t;
typedef struct ApeGCMemory_t ApeGCMemory_t;
typedef struct ApeTracebackItem_t ApeTracebackItem_t;
typedef struct ApeTraceback_t ApeTraceback_t;
typedef struct ApeFrame_t ApeFrame_t;
typedef struct ApeValDictionary_t ApeValDictionary_t;
typedef struct ApeDictionary_t ApeDictionary_t;
typedef struct ApeArray_t ApeArray_t;
typedef struct ApePtrDictionary_t ApePtrDictionary_t;
typedef struct ApePtrArray_t ApePtrArray_t;
typedef struct ApeStringBuffer_t ApeStringBuffer_t;
typedef struct ApeGlobalStore_t ApeGlobalStore_t;
typedef struct ApeModule_t ApeModule_t;
typedef struct ApeFileScope_t ApeFileScope_t;
typedef struct ApeCompiler_t ApeCompiler_t;
typedef struct ApeNativeFuncWrapper_t ApeNativeFuncWrapper_t;



//typedef ApeObject_t (*ape_native_fn)(ApeContext_t* ape, void* data, int argc, ApeObject_t* args);
typedef ApeObject_t (*ApeUserFNCallback_t)(ApeContext_t* ape, void* data, int argc, ApeObject_t* args);
typedef ApeObject_t     (*ApeNativeFNCallback_t)            (ApeVM_t* vm, void* data, int argc, ApeObject_t* args);

typedef void* (*ApeMallocFNCallback_t)(void* ctx, size_t size);
typedef void (*ApeFreeFNCallback_t)(void* ctx, void* ptr);
typedef void (*ApeDataDestroyFNCallback_t)(void* data);
typedef void* (*ApeDataCopyFNCallback_t)(void* data);
typedef size_t (*ApeStdoutWriteFNCallback_t)(void* context, const void* data, size_t data_size);
typedef char* (*ApeReadFileFNCallback_t)(void* context, const char* path);
typedef size_t (*ApeWriteFileFNCallback_t)(void* context, const char* path, const char* string, size_t string_size);
typedef unsigned long (*ApeCollectionsHashFNCallback_t)(const void* val);
typedef bool (*ApeCollectionsEqualsFNCallback_t)(const void* a, const void* b);
typedef void* (*ApeAllocatorMallocFNCallback_t)(void* ctx, size_t size);
typedef void (*ApeAllocatorFreeFNCallback_t)(void* ctx, void* ptr);
typedef void (*ApeDictItemDestroyFNCallback_t)(void* item);
typedef void* (*ApeDictItemCopyFNCallback_t)(void* item);
typedef void (*ApeArrayItemDeinitFNCallback_t)(void* item);
typedef void (*ApePtrArrayItemDestroyFNCallback_t)(void* item);
typedef void* (*ApePtrArrayItemCopyFNCallback_t)(void* item);
typedef ApeExpression_t* (*ApeRightAssocParseFNCallback_t)(ApeParser_t* p);
typedef ApeExpression_t* (*ApeLeftAssocParseFNCallback_t)(ApeParser_t* p, ApeExpression_t* expr);

typedef void (*ApeExternalDataDestroyFNCallback_t)(void* data);
typedef void* (*ApeExternalDataCopyFNCallback_t)(void* data);


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
            ApeStdoutWriteFNCallback_t write;
            void* context;
        } write;
    } stdio;

    struct
    {
        struct
        {
            ApeReadFileFNCallback_t read_file;
            void* context;
        } read_file;

        struct
        {
            ApeWriteFileFNCallback_t write_file;
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
    ApeAllocatorMallocFNCallback_t malloc;
    ApeAllocatorFreeFNCallback_t free;
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
    ptrarray(char*) * lines;
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
    ptrarray(ApeStatement_t) * statements;
};

struct ApeMapLiteral_t
{
    ptrarray(ApeExpression_t) * keys;
    ptrarray(ApeExpression_t) * values;
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
    ptrarray(ApeIdent_t) * params;
    ApeCodeblock_t* body;
};

struct ApeCallExpr_t
{
    ApeExpression_t* function;
    ptrarray(ApeExpression_t) * args;
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
    ApeExpr_type_t type;
    union
    {
        ApeIdent_t* ident;
        double number_literal;
        bool bool_literal;
        char* string_literal;
        ptrarray(ApeExpression_t) * array;
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
    ptrarray(ApeIfCase_t) * cases;
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
    uint64_t _internal;
    union
    {
        uint64_t handle;
        double number;
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
    int free_vals_count;
    bool owns_data;
};

struct ApeNativeFunction_t
{
    char* name;
    ApeNativeFNCallback_t fn;
    uint8_t data[NATIVE_FN_MAX_DATA_LEN];
    int data_len;
};

struct ApeExternalData_t
{
    void* data;
    ApeExternalDataDestroyFNCallback_t data_destroy_fn;
    ApeExternalDataCopyFNCallback_t data_copy_fn;
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

struct ApeObjectData_t
{
    ApeGCMemory_t* mem;
    union
    {
        ApeObjectString_t string;
        ApeObjectError_t error;
        array(ApeObject_t) * array;
        valdict(ApeObject_t, ApeObject_t) * map;
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
    dict(ApeSymbol_t) * store;
    int offset;
    int num_definitions;
};

struct ApeSymbol_table_t
{
    ApeAllocator_t* alloc;
    ApeSymbol_table_t* outer;
    ApeGlobalStore_t* global_store;
    ptrarray(ApeBlockScope_t) * block_scopes;
    ptrarray(ApeSymbol_t) * free_symbols;
    ptrarray(ApeSymbol_t) * module_global_symbols;
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
    uint8_t* bytecode;
    ApePosition_t* src_positions;
    int count;
};

struct ApeCompilationScope_t
{
    ApeAllocator_t* alloc;
    ApeCompilationScope_t* outer;
    array(uint8_t) * bytecode;
    array(ApePosition_t) * src_positions;
    array(int) * break_ip_stack;
    array(int) * continue_ip_stack;
    opcode_t last_opcode;
};

struct ApeObjectDataPool_t
{
    ApeObjectData_t* data[GCMEM_POOL_SIZE];
    int count;
};

struct ApeGCMemory_t
{
    ApeAllocator_t* alloc;
    int allocations_since_sweep;
    ptrarray(ApeObjectData_t) * objects;
    ptrarray(ApeObjectData_t) * objects_back;
    array(ApeObject_t) * objects_not_gced;
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
    array(ApeTracebackItem_t) * items;
};

struct ApeFrame_t
{
    ApeObject_t function;
    int ip;
    int base_pointer;
    const ApePosition_t* src_positions;
    uint8_t* bytecode;
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
    int globals_count;
    ApeObject_t stack[VM_STACK_SIZE];
    int sp;
    ApeObject_t this_stack[VM_THIS_STACK_SIZE];
    int this_sp;
    ApeFrame_t frames[VM_MAX_FRAMES];
    int frames_count;
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
    ApeCollectionsHashFNCallback_t _hash_key;
    ApeCollectionsEqualsFNCallback_t _keys_equals;
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
    ApeDictItemCopyFNCallback_t copy_fn;
    ApeDictItemDestroyFNCallback_t destroy_fn;
};

struct ApeArray_t
{
    ApeAllocator_t* alloc;
    unsigned char* data;
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
    char* data;
    size_t capacity;
    size_t len;
};

struct ApeGlobalStore_t
{
    ApeAllocator_t* alloc;
    dict(ApeSymbol_t) * symbols;
    array(ApeObject_t) * objects;
};

struct ApeModule_t
{
    ApeAllocator_t* alloc;
    char* name;
    ptrarray(ApeSymbol_t) * symbols;
};

struct ApeFileScope_t
{
    ApeAllocator_t* alloc;
    ApeParser_t* parser;
    ApeSymbol_table_t* symbol_table;
    ApeCompiledFile_t* file;
    ptrarray(char) * loaded_module_names;
};

struct ApeCompiler_t
{
    ApeAllocator_t* alloc;
    const ApeConfig_t* config;
    ApeGCMemory_t* mem;
    ApeErrorList_t* errors;
    ptrarray(ApeCompiledFile_t) * files;
    ApeGlobalStore_t* global_store;
    array(ApeObject_t) * constants;
    ApeCompilationScope_t* compilation_scope;
    ptrarray(ApeFileScope_t) * file_scopes;
    array(ApePosition_t) * src_positions_stack;
    dict(ApeModule_t) * modules;
    dict(int) * string_constants_positions;
};

struct ApeNativeFuncWrapper_t
{
    ApeUserFNCallback_t fn;
    ApeContext_t* ape;
    void* data;
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
    ptrarray(ApeCompiledFile_t) * files;
    ApeGlobalStore_t* global_store;
    ApeCompiler_t* compiler;
    ApeVM_t* vm;
    ApeErrorList_t errors;
    ApeConfig_t config;
    ApeAllocator_t custom_allocator;
};

static const ApePosition_t src_pos_invalid = { NULL, -1, -1 };
static const ApePosition_t src_pos_zero = { NULL, 0, 0 };

ApePosition_t src_pos_make(const ApeCompiledFile_t* file, int line, int column);
char* ape_stringf(ApeAllocator_t* alloc, const char* format, ...);
void ape_log(const char* file, int line, const char* format, ...);
char* ape_strndup(ApeAllocator_t* alloc, const char* string, size_t n);
char* ape_strdup(ApeAllocator_t* alloc, const char* string);

uint64_t ape_double_to_uint64(double val);
double ape_uint64_to_double(uint64_t val);

bool ape_timer_platform_supported(void);
ApeTimer_t ape_timer_start(void);
double ape_timer_get_elapsed_ms(const ApeTimer_t* timer);


//-----------------------------------------------------------------------------
// Collections
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Allocator
//-----------------------------------------------------------------------------


ApeAllocator_t allocator_make(ApeAllocatorMallocFNCallback_t malloc_fn, ApeAllocatorFreeFNCallback_t free_fn, void* ctx);
void* allocator_malloc(ApeAllocator_t* allocator, size_t size);
void allocator_free(ApeAllocator_t* allocator, void* ptr);

//-----------------------------------------------------------------------------
// Dictionary
//-----------------------------------------------------------------------------


ApeDictionary_t* dict_make_(ApeAllocator_t* alloc, ApeDictItemCopyFNCallback_t copy_fn, ApeDictItemDestroyFNCallback_t destroy_fn);
void dict_destroy(ApeDictionary_t* dict);
void dict_destroy_with_items(ApeDictionary_t* dict);
ApeDictionary_t* dict_copy_with_items(ApeDictionary_t* dict);
bool dict_set(ApeDictionary_t* dict, const char* key, void* value);
void* dict_get(const ApeDictionary_t* dict, const char* key);
void* dict_get_value_at(const ApeDictionary_t* dict, unsigned int ix);
const char* dict_get_key_at(const ApeDictionary_t* dict, unsigned int ix);
int dict_count(const ApeDictionary_t* dict);
bool dict_remove(ApeDictionary_t* dict, const char* key);

//-----------------------------------------------------------------------------
// Value dictionary
//-----------------------------------------------------------------------------


ApeValDictionary_t* valdict_make_(ApeAllocator_t* alloc, size_t key_size, size_t val_size);
ApeValDictionary_t* valdict_make_with_capacity(ApeAllocator_t* alloc, unsigned int min_capacity, size_t key_size, size_t val_size);
void valdict_destroy(ApeValDictionary_t* dict);
void valdict_set_hash_function(ApeValDictionary_t* dict, ApeCollectionsHashFNCallback_t hash_fn);
void valdict_set_equals_function(ApeValDictionary_t* dict, ApeCollectionsEqualsFNCallback_t equals_fn);
bool valdict_set(ApeValDictionary_t* dict, void* key, void* value);
void* valdict_get(const ApeValDictionary_t* dict, const void* key);
void* valdict_get_key_at(const ApeValDictionary_t* dict, unsigned int ix);
void* valdict_get_value_at(const ApeValDictionary_t* dict, unsigned int ix);
unsigned int valdict_get_capacity(const ApeValDictionary_t* dict);
bool valdict_set_value_at(const ApeValDictionary_t* dict, unsigned int ix, const void* value);
int valdict_count(const ApeValDictionary_t* dict);
bool valdict_remove(ApeValDictionary_t* dict, void* key);
void valdict_clear(ApeValDictionary_t* dict);

//-----------------------------------------------------------------------------
// Pointer dictionary
//-----------------------------------------------------------------------------


ApePtrDictionary_t* ptrdict_make(ApeAllocator_t* alloc);
void ptrdict_destroy(ApePtrDictionary_t* dict);
void ptrdict_set_hash_function(ApePtrDictionary_t* dict, ApeCollectionsHashFNCallback_t hash_fn);
void ptrdict_set_equals_function(ApePtrDictionary_t* dict, ApeCollectionsEqualsFNCallback_t equals_fn);
bool ptrdict_set(ApePtrDictionary_t* dict, void* key, void* value);
void* ptrdict_get(const ApePtrDictionary_t* dict, const void* key);
void* ptrdict_get_value_at(const ApePtrDictionary_t* dict, unsigned int ix);
void* ptrdict_get_key_at(const ApePtrDictionary_t* dict, unsigned int ix);
int ptrdict_count(const ApePtrDictionary_t* dict);
bool ptrdict_remove(ApePtrDictionary_t* dict, void* key);

//-----------------------------------------------------------------------------
// Array
//-----------------------------------------------------------------------------


ApeArray_t* array_make_(ApeAllocator_t* alloc, size_t element_size);
ApeArray_t* array_make_with_capacity(ApeAllocator_t* alloc, unsigned int capacity, size_t element_size);
void array_destroy(ApeArray_t* arr);
void array_destroy_with_items_(ApeArray_t* arr, ApeArrayItemDeinitFNCallback_t deinit_fn);
ApeArray_t* array_copy(const ApeArray_t* arr);
bool array_add(ApeArray_t* arr, const void* value);
bool array_addn(ApeArray_t* arr, const void* values, int n);
bool array_add_array(ApeArray_t* dest, ApeArray_t* source);
bool array_push(ApeArray_t* arr, const void* value);
bool array_pop(ApeArray_t* arr, void* out_value);
void* array_top(ApeArray_t* arr);
bool array_set(ApeArray_t* arr, unsigned int ix, void* value);
bool array_setn(ApeArray_t* arr, unsigned int ix, void* values, int n);
void* array_get(ApeArray_t* arr, unsigned int ix);
const void* array_get_const(const ApeArray_t* arr, unsigned int ix);
void* array_get_last(ApeArray_t* arr);
int array_count(const ApeArray_t* arr);
unsigned int array_get_capacity(const ApeArray_t* arr);
bool array_remove_at(ApeArray_t* arr, unsigned int ix);
bool array_remove_item(ApeArray_t* arr, void* ptr);
void array_clear(ApeArray_t* arr);
void array_clear_and_deinit_items_(ApeArray_t* arr, ApeArrayItemDeinitFNCallback_t deinit_fn);
void array_lock_capacity(ApeArray_t* arr);
int array_get_index(const ApeArray_t* arr, void* ptr);
bool array_contains(const ApeArray_t* arr, void* ptr);
void* array_data(ApeArray_t* arr);// might become invalidated by remove/add operations
const void* array_const_data(const ApeArray_t* arr);
void array_orphan_data(ApeArray_t* arr);
bool array_reverse(ApeArray_t* arr);

//-----------------------------------------------------------------------------
// Pointer Array
//-----------------------------------------------------------------------------


ApePtrArray_t* ptrarray_make(ApeAllocator_t* alloc);
ApePtrArray_t* ptrarray_make_with_capacity(ApeAllocator_t* alloc, unsigned int capacity);
void ptrarray_destroy(ApePtrArray_t* arr);
void ptrarray_destroy_with_items_(ApePtrArray_t* arr, ApePtrArrayItemDestroyFNCallback_t destroy_fn);
ApePtrArray_t* ptrarray_copy(ApePtrArray_t* arr);
ApePtrArray_t* ptrarray_copy_with_items_(ApePtrArray_t* arr, ApePtrArrayItemCopyFNCallback_t copy_fn, ApePtrArrayItemDestroyFNCallback_t destroy_fn);
bool ptrarray_add(ApePtrArray_t* arr, void* ptr);
bool ptrarray_set(ApePtrArray_t* arr, unsigned int ix, void* ptr);
bool ptrarray_add_array(ApePtrArray_t* dest, ApePtrArray_t* source);
void* ptrarray_get(ApePtrArray_t* arr, unsigned int ix);
const void* ptrarray_get_const(const ApePtrArray_t* arr, unsigned int ix);
bool ptrarray_push(ApePtrArray_t* arr, void* ptr);
void* ptrarray_pop(ApePtrArray_t* arr);
void* ptrarray_top(ApePtrArray_t* arr);
int ptrarray_count(const ApePtrArray_t* arr);
bool ptrarray_remove_at(ApePtrArray_t* arr, unsigned int ix);
bool ptrarray_remove_item(ApePtrArray_t* arr, void* item);
void ptrarray_clear(ApePtrArray_t* arr);
void ptrarray_clear_and_destroy_items_(ApePtrArray_t* arr, ApePtrArrayItemDestroyFNCallback_t destroy_fn);
void ptrarray_lock_capacity(ApePtrArray_t* arr);
int ptrarray_get_index(ApePtrArray_t* arr, void* ptr);
bool ptrarray_contains(ApePtrArray_t* arr, void* ptr);
void* ptrarray_get_addr(ApePtrArray_t* arr, unsigned int ix);
void* ptrarray_data(ApePtrArray_t* arr);// might become invalidated by remove/add operations
void ptrarray_reverse(ApePtrArray_t* arr);

//-----------------------------------------------------------------------------
// String buffer
//-----------------------------------------------------------------------------


ApeStringBuffer_t* strbuf_make(ApeAllocator_t* alloc);
ApeStringBuffer_t* strbuf_make_with_capacity(ApeAllocator_t* alloc, unsigned int capacity);
void strbuf_destroy(ApeStringBuffer_t* buf);
void strbuf_clear(ApeStringBuffer_t* buf);
bool strbuf_append(ApeStringBuffer_t* buf, const char* str);
bool strbuf_appendf(ApeStringBuffer_t* buf, const char* fmt, ...);
const char* strbuf_get_string(const ApeStringBuffer_t* buf);
size_t strbuf_get_length(const ApeStringBuffer_t* buf);
char* strbuf_get_string_and_destroy(ApeStringBuffer_t* buf);
bool strbuf_failed(ApeStringBuffer_t* buf);

//-----------------------------------------------------------------------------
// Utils
//-----------------------------------------------------------------------------

ptrarray(char) * kg_split_string(ApeAllocator_t* alloc, const char* str, const char* delimiter);
char* kg_join(ApeAllocator_t* alloc, ptrarray(char) * items, const char* with);
char* kg_canonicalise_path(ApeAllocator_t* alloc, const char* path);
bool kg_is_path_absolute(const char* path);
bool kg_streq(const char* a, const char* b);


void errors_init(ApeErrorList_t* errors);
void errors_deinit(ApeErrorList_t* errors);

void errors_add_error(ApeErrorList_t* errors, ApeErrorType_t type, ApePosition_t pos, const char* message);
void errors_add_errorf(ApeErrorList_t* errors, ApeErrorType_t type, ApePosition_t pos, const char* format, ...);
void errors_clear(ApeErrorList_t* errors);
int errors_get_count(const ApeErrorList_t* errors);
ApeError_t* errors_get(ApeErrorList_t* errors, int ix);
const ApeError_t* errors_getc(const ApeErrorList_t* errors, int ix);
const char* error_type_to_string(ApeErrorType_t type);
ApeError_t* errors_get_last_error(ApeErrorList_t* errors);
bool errors_has_errors(const ApeErrorList_t* errors);


void token_init(ApeToken_t* tok, ApeTokenType_t type, const char* literal, int len);// no need to destroy
char* token_duplicate_literal(ApeAllocator_t* alloc, const ApeToken_t* tok);
const char* token_type_to_string(ApeTokenType_t type);


ApeCompiledFile_t* compiled_file_make(ApeAllocator_t* alloc, const char* path);
void compiled_file_destroy(ApeCompiledFile_t* file);


bool lexer_init(ApeLexer_t* lex, ApeAllocator_t* alloc, ApeErrorList_t* errs, const char* input, ApeCompiledFile_t* file);// no need to deinit

bool lexer_failed(ApeLexer_t* lex);
void lexer_continue_template_string(ApeLexer_t* lex);
bool lexer_cur_token_is(ApeLexer_t* lex, ApeTokenType_t type);
bool lexer_peek_token_is(ApeLexer_t* lex, ApeTokenType_t type);
bool lexer_next_token(ApeLexer_t* lex);
bool lexer_previous_token(ApeLexer_t* lex);
ApeToken_t lexer_next_token_internal(ApeLexer_t* lex);// exposed here for tests
bool lexer_expect_current(ApeLexer_t* lex, ApeTokenType_t type);


char* statements_to_string(ApeAllocator_t* alloc, ptrarray(ApeStatement_t) * statements);

ApeStatement_t* statement_make_define(ApeAllocator_t* alloc, ApeIdent_t* name, ApeExpression_t* value, bool assignable);
ApeStatement_t* statement_make_if(ApeAllocator_t* alloc, ptrarray(ApeIfCase_t) * cases, ApeCodeblock_t* alternative);
ApeStatement_t* statement_make_return(ApeAllocator_t* alloc, ApeExpression_t* value);
ApeStatement_t* statement_make_expression(ApeAllocator_t* alloc, ApeExpression_t* value);
ApeStatement_t* statement_make_while_loop(ApeAllocator_t* alloc, ApeExpression_t* test, ApeCodeblock_t* body);
ApeStatement_t* statement_make_break(ApeAllocator_t* alloc);
ApeStatement_t* statement_make_foreach(ApeAllocator_t* alloc, ApeIdent_t* iterator, ApeExpression_t* source, ApeCodeblock_t* body);
ApeStatement_t*
statement_make_for_loop(ApeAllocator_t* alloc, ApeStatement_t* init, ApeExpression_t* test, ApeExpression_t* update, ApeCodeblock_t* body);
ApeStatement_t* statement_make_continue(ApeAllocator_t* alloc);
ApeStatement_t* statement_make_block(ApeAllocator_t* alloc, ApeCodeblock_t* block);
ApeStatement_t* statement_make_import(ApeAllocator_t* alloc, char* path);
ApeStatement_t* statement_make_recover(ApeAllocator_t* alloc, ApeIdent_t* error_ident, ApeCodeblock_t* body);

void statement_destroy(ApeStatement_t* stmt);

ApeStatement_t* statement_copy(const ApeStatement_t* stmt);

ApeCodeblock_t* code_block_make(ApeAllocator_t* alloc, ptrarray(ApeStatement_t) * statements);
void code_block_destroy(ApeCodeblock_t* stmt);
ApeCodeblock_t* code_block_copy(ApeCodeblock_t* block);

ApeExpression_t* expression_make_ident(ApeAllocator_t* alloc, ApeIdent_t* ident);
ApeExpression_t* expression_make_number_literal(ApeAllocator_t* alloc, double val);
ApeExpression_t* expression_make_bool_literal(ApeAllocator_t* alloc, bool val);
ApeExpression_t* expression_make_string_literal(ApeAllocator_t* alloc, char* value);
ApeExpression_t* expression_make_null_literal(ApeAllocator_t* alloc);
ApeExpression_t* expression_make_array_literal(ApeAllocator_t* alloc, ptrarray(ApeExpression_t) * values);
ApeExpression_t* expression_make_map_literal(ApeAllocator_t* alloc, ptrarray(ApeExpression_t) * keys, ptrarray(ApeExpression_t) * values);
ApeExpression_t* expression_make_prefix(ApeAllocator_t* alloc, ApeOperator_t op, ApeExpression_t* right);
ApeExpression_t* expression_make_infix(ApeAllocator_t* alloc, ApeOperator_t op, ApeExpression_t* left, ApeExpression_t* right);
ApeExpression_t* expression_make_fn_literal(ApeAllocator_t* alloc, ptrarray(ApeIdent_t) * params, ApeCodeblock_t* body);
ApeExpression_t* expression_make_call(ApeAllocator_t* alloc, ApeExpression_t* function, ptrarray(ApeExpression_t) * args);
ApeExpression_t* expression_make_index(ApeAllocator_t* alloc, ApeExpression_t* left, ApeExpression_t* index);
ApeExpression_t* expression_make_assign(ApeAllocator_t* alloc, ApeExpression_t* dest, ApeExpression_t* source, bool is_postfix);
ApeExpression_t* expression_make_logical(ApeAllocator_t* alloc, ApeOperator_t op, ApeExpression_t* left, ApeExpression_t* right);
ApeExpression_t* expression_make_ternary(ApeAllocator_t* alloc, ApeExpression_t* test, ApeExpression_t* if_true, ApeExpression_t* if_false);

void expression_destroy(ApeExpression_t* expr);

ApeExpression_t* expression_copy(ApeExpression_t* expr);

void statement_to_string(const ApeStatement_t* stmt, ApeStringBuffer_t* buf);
void expression_to_string(ApeExpression_t* expr, ApeStringBuffer_t* buf);

void code_block_to_string(const ApeCodeblock_t* stmt, ApeStringBuffer_t* buf);
const char* operator_to_string(ApeOperator_t op);

const char* expression_type_to_string(ApeExpr_type_t type);

ApeIdent_t* ident_make(ApeAllocator_t* alloc, ApeToken_t tok);
ApeIdent_t* ident_copy(ApeIdent_t* ident);
void ident_destroy(ApeIdent_t* ident);

ApeIfCase_t* if_case_make(ApeAllocator_t* alloc, ApeExpression_t* test, ApeCodeblock_t* consequence);
void if_case_destroy(ApeIfCase_t* cond);
ApeIfCase_t* if_case_copy(ApeIfCase_t* cond);


ApeParser_t* parser_make(ApeAllocator_t* alloc, const ApeConfig_t* config, ApeErrorList_t* errors);
void parser_destroy(ApeParser_t* parser);

ptrarray(ApeStatement_t) * parser_parse_all(ApeParser_t* parser, const char* input, ApeCompiledFile_t* file);


ApeObject_t object_make_from_data(ApeObjectType_t type, ApeObjectData_t* data);
ApeObject_t object_make_number(double val);
ApeObject_t object_make_bool(bool val);
ApeObject_t object_make_null(void);
ApeObject_t object_make_string(ApeGCMemory_t* mem, const char* string);
ApeObject_t object_make_string_with_capacity(ApeGCMemory_t* mem, int capacity);
ApeObject_t object_make_native_function(ApeGCMemory_t* mem, const char* name, ApeNativeFNCallback_t fn, void* data, int data_len);
ApeObject_t object_make_array(ApeGCMemory_t* mem);
ApeObject_t object_make_array_with_capacity(ApeGCMemory_t* mem, unsigned capacity);
ApeObject_t object_make_map(ApeGCMemory_t* mem);
ApeObject_t object_make_map_with_capacity(ApeGCMemory_t* mem, unsigned capacity);
ApeObject_t object_make_error(ApeGCMemory_t* mem, const char* message);
ApeObject_t object_make_error_no_copy(ApeGCMemory_t* mem, char* message);
ApeObject_t object_make_errorf(ApeGCMemory_t* mem, const char* fmt, ...);
ApeObject_t
object_make_function(ApeGCMemory_t* mem, const char* name, ApeCompilationResult_t* comp_res, bool owns_data, int num_locals, int num_args, int free_vals_count);
ApeObject_t object_make_external(ApeGCMemory_t* mem, void* data);

void object_deinit(ApeObject_t obj);
void object_data_deinit(ApeObjectData_t* obj);

bool object_is_allocated(ApeObject_t obj);
ApeGCMemory_t* object_get_mem(ApeObject_t obj);
bool object_is_hashable(ApeObject_t obj);
void object_to_string(ApeObject_t obj, ApeStringBuffer_t* buf, bool quote_str);
const char* object_get_type_name(const ApeObjectType_t type);
char* object_get_type_union_name(ApeAllocator_t* alloc, const ApeObjectType_t type);
char* object_serialize(ApeAllocator_t* alloc, ApeObject_t object, size_t* lendest);
ApeObject_t object_deep_copy(ApeGCMemory_t* mem, ApeObject_t object);
ApeObject_t object_copy(ApeGCMemory_t* mem, ApeObject_t obj);
double object_compare(ApeObject_t a, ApeObject_t b, bool* out_ok);
bool object_equals(ApeObject_t a, ApeObject_t b);

ApeObjectData_t* object_get_allocated_data(ApeObject_t object);

bool object_get_bool(ApeObject_t obj);
double object_get_number(ApeObject_t obj);
ApeFunction_t* object_get_function(ApeObject_t obj);
const char* object_get_string(ApeObject_t obj);
int object_get_string_length(ApeObject_t obj);
void object_set_string_length(ApeObject_t obj, int len);
int object_get_string_capacity(ApeObject_t obj);
char* object_get_mutable_string(ApeObject_t obj);
bool object_string_append(ApeObject_t obj, const char* src, int len);
unsigned long object_get_string_hash(ApeObject_t obj);
ApeNativeFunction_t* object_get_native_function(ApeObject_t obj);
ApeObjectType_t object_get_type(ApeObject_t obj);

bool object_is_numeric(ApeObject_t obj);
bool object_is_null(ApeObject_t obj);
bool object_is_callable(ApeObject_t obj);

const char* object_get_function_name(ApeObject_t obj);
ApeObject_t object_get_function_free_val(ApeObject_t obj, int ix);
void object_set_function_free_val(ApeObject_t obj, int ix, ApeObject_t val);
ApeObject_t* object_get_function_free_vals(ApeObject_t obj);

const char* object_get_error_message(ApeObject_t obj);
void object_set_error_traceback(ApeObject_t obj, ApeTraceback_t* traceback);
ApeTraceback_t* object_get_error_traceback(ApeObject_t obj);

ApeExternalData_t* object_get_external_data(ApeObject_t object);
bool object_set_external_destroy_function(ApeObject_t object, ApeExternalDataDestroyFNCallback_t destroy_fn);
bool object_set_external_data(ApeObject_t object, void* data);
bool object_set_external_copy_function(ApeObject_t object, ApeExternalDataCopyFNCallback_t copy_fn);

ApeObject_t object_get_array_value_at(ApeObject_t array, int ix);
bool object_set_array_value_at(ApeObject_t obj, int ix, ApeObject_t val);
bool object_add_array_value(ApeObject_t array, ApeObject_t val);
int object_get_array_length(ApeObject_t array);
bool object_remove_array_value_at(ApeObject_t array, int ix);

int object_get_map_length(ApeObject_t obj);
ApeObject_t object_get_map_key_at(ApeObject_t obj, int ix);
ApeObject_t object_get_map_value_at(ApeObject_t obj, int ix);
bool object_set_map_value_at(ApeObject_t obj, int ix, ApeObject_t val);
ApeObject_t object_get_kv_pair_at(ApeGCMemory_t* mem, ApeObject_t obj, int ix);
bool object_set_map_value(ApeObject_t obj, ApeObject_t key, ApeObject_t val);
ApeObject_t object_get_map_value(ApeObject_t obj, ApeObject_t key);
bool object_map_has_key(ApeObject_t obj, ApeObject_t key);


ApeGlobalStore_t* global_store_make(ApeAllocator_t* alloc, ApeGCMemory_t* mem);
void global_store_destroy(ApeGlobalStore_t* store);
const ApeSymbol_t* global_store_get_symbol(ApeGlobalStore_t* store, const char* name);
ApeObject_t global_store_get_object(ApeGlobalStore_t* store, const char* name);
bool global_store_set(ApeGlobalStore_t* store, const char* name, ApeObject_t object);
ApeObject_t global_store_get_object_at(ApeGlobalStore_t* store, int ix, bool* out_ok);
bool global_store_set_object_at(ApeGlobalStore_t* store, int ix, ApeObject_t object);
ApeObject_t* global_store_get_object_data(ApeGlobalStore_t* store);
int global_store_get_object_count(ApeGlobalStore_t* store);


ApeSymbol_t* symbol_make(ApeAllocator_t* alloc, const char* name, ApeSymbolType_t type, int index, bool assignable);
void symbol_destroy(ApeSymbol_t* symbol);
ApeSymbol_t* symbol_copy(ApeSymbol_t* symbol);

ApeSymbol_table_t* symbol_table_make(ApeAllocator_t* alloc, ApeSymbol_table_t* outer, ApeGlobalStore_t* global_store, int module_global_offset);
void symbol_table_destroy(ApeSymbol_table_t* st);
ApeSymbol_table_t* symbol_table_copy(ApeSymbol_table_t* st);
bool symbol_table_add_module_symbol(ApeSymbol_table_t* st, ApeSymbol_t* symbol);
const ApeSymbol_t* symbol_table_define(ApeSymbol_table_t* st, const char* name, bool assignable);
const ApeSymbol_t* symbol_table_define_free(ApeSymbol_table_t* st, const ApeSymbol_t* original);
const ApeSymbol_t* symbol_table_define_function_name(ApeSymbol_table_t* st, const char* name, bool assignable);
const ApeSymbol_t* symbol_table_define_this(ApeSymbol_table_t* st);

const ApeSymbol_t* symbol_table_resolve(ApeSymbol_table_t* st, const char* name);

bool symbol_table_symbol_is_defined(ApeSymbol_table_t* st, const char* name);
bool symbol_table_push_block_scope(ApeSymbol_table_t* table);
void symbol_table_pop_block_scope(ApeSymbol_table_t* table);
ApeBlockScope_t* symbol_table_get_block_scope(ApeSymbol_table_t* table);

bool symbol_table_is_module_global_scope(ApeSymbol_table_t* table);
bool symbol_table_is_top_block_scope(ApeSymbol_table_t* table);
bool symbol_table_is_top_global_scope(ApeSymbol_table_t* table);

int symbol_table_get_module_global_symbol_count(const ApeSymbol_table_t* table);
const ApeSymbol_t* symbol_table_get_module_global_symbol_at(const ApeSymbol_table_t* table, int ix);

ApeOpcodeDefinition_t* opcode_lookup(opcode_t op);
const char* opcode_get_name(opcode_t op);
int code_make(opcode_t op, int operands_count, uint64_t* operands, array(uint8_t) * res);
void code_to_string(uint8_t* code, ApePosition_t* source_positions, size_t code_size, ApeStringBuffer_t* res);
bool code_read_operands(ApeOpcodeDefinition_t* def, uint8_t* instr, uint64_t out_operands[2]);


ApeCompilationScope_t* compilation_scope_make(ApeAllocator_t* alloc, ApeCompilationScope_t* outer);
void compilation_scope_destroy(ApeCompilationScope_t* scope);
ApeCompilationResult_t* compilation_scope_orphan_result(ApeCompilationScope_t* scope);

ApeCompilationResult_t* compilation_result_make(ApeAllocator_t* alloc, uint8_t* bytecode, ApePosition_t* src_positions, int count);
void compilation_result_destroy(ApeCompilationResult_t* res);

ApeExpression_t* optimise_expression(ApeExpression_t* expr);


ApeCompiler_t* compiler_make(ApeAllocator_t* alloc,
                          const ApeConfig_t* config,
                          ApeGCMemory_t* mem,
                          ApeErrorList_t* errors,
                          ptrarray(ApeCompiledFile_t) * files,
                          ApeGlobalStore_t* global_store);
void compiler_destroy(ApeCompiler_t* comp);
ApeCompilationResult_t* compiler_compile(ApeCompiler_t* comp, const char* code);
ApeCompilationResult_t* compiler_compile_file(ApeCompiler_t* comp, const char* path);
ApeSymbol_table_t* compiler_get_symbol_table(ApeCompiler_t* comp);
void compiler_set_symbol_table(ApeCompiler_t* comp, ApeSymbol_table_t* table);
array(ApeObject_t) * compiler_get_constants(const ApeCompiler_t* comp);


ApeGCMemory_t* gcmem_make(ApeAllocator_t* alloc);
void gcmem_destroy(ApeGCMemory_t* mem);

ApeObjectData_t* gcmem_alloc_object_data(ApeGCMemory_t* mem, ApeObjectType_t type);
ApeObjectData_t* gcmem_get_object_data_from_pool(ApeGCMemory_t* mem, ApeObjectType_t type);

void gc_unmark_all(ApeGCMemory_t* mem);
void gc_mark_objects(ApeObject_t* objects, int count);
void gc_mark_object(ApeObject_t object);
void gc_sweep(ApeGCMemory_t* mem);

bool gc_disable_on_object(ApeObject_t obj);
void gc_enable_on_object(ApeObject_t obj);

int gc_should_sweep(ApeGCMemory_t* mem);


int builtins_count(void);
ApeNativeFNCallback_t builtins_get_fn(int ix);
const char* builtins_get_name(int ix);


ApeTraceback_t* traceback_make(ApeAllocator_t* alloc);
void traceback_destroy(ApeTraceback_t* traceback);
bool traceback_append(ApeTraceback_t* traceback, const char* function_name, ApePosition_t pos);
bool traceback_append_from_vm(ApeTraceback_t* traceback, ApeVM_t* vm);
bool traceback_to_string(const ApeTraceback_t* traceback, ApeStringBuffer_t* buf);
const char* traceback_item_get_line(ApeTracebackItem_t* item);
const char* traceback_item_get_filepath(ApeTracebackItem_t* item);


bool frame_init(ApeFrame_t* frame, ApeObject_t function, int base_pointer);

ApeOpcodeValue_t frame_read_opcode(ApeFrame_t* frame);
uint64_t frame_read_uint64(ApeFrame_t* frame);
uint16_t frame_read_uint16(ApeFrame_t* frame);
uint8_t frame_read_uint8(ApeFrame_t* frame);
ApePosition_t frame_src_position(const ApeFrame_t* frame);


ApeVM_t* vm_make(ApeAllocator_t* alloc, const ApeConfig_t* config, ApeGCMemory_t* mem, ApeErrorList_t* errors, ApeGlobalStore_t* global_store);// config can be null (for internal testing purposes)
void vm_destroy(ApeVM_t* vm);

void vm_reset(ApeVM_t* vm);

bool vm_run(ApeVM_t* vm, ApeCompilationResult_t* comp_res, array(ApeObject_t) * constants);
ApeObject_t vm_call(ApeVM_t* vm, array(ApeObject_t) * constants, ApeObject_t callee, int argc, ApeObject_t* args);
bool vm_execute_function(ApeVM_t* vm, ApeObject_t function, array(ApeObject_t) * constants);

ApeObject_t vm_get_last_popped(ApeVM_t* vm);
bool vm_has_errors(ApeVM_t* vm);

bool vm_set_global(ApeVM_t* vm, int ix, ApeObject_t val);
ApeObject_t vm_get_global(ApeVM_t* vm, int ix);


ApeContext_t* ape_make(void);
ApeContext_t* ape_make_ex(ApeMallocFNCallback_t malloc_fn, ApeFreeFNCallback_t free_fn, void* ctx);
void ape_destroy(ApeContext_t* ape);

void ape_free_allocated(ApeContext_t* ape, void* ptr);

void ape_set_repl_mode(ApeContext_t* ape, bool enabled);

// -1 to disable, returns false if it can't be set for current platform (otherwise true).
// If execution time exceeds given limit an APE_ERROR_TIMEOUT error is set.
// Precision is not guaranteed because time can't be checked every VM tick
// but expect it to be submilisecond.
bool ape_set_timeout(ApeContext_t* ape, double max_execution_time_ms);

void ape_set_stdout_write_function(ApeContext_t* ape, ApeStdoutWriteFNCallback_t stdout_write, void* context);
void ape_set_file_write_function(ApeContext_t* ape, ApeWriteFileFNCallback_t file_write, void* context);
void ape_set_file_read_function(ApeContext_t* ape, ApeReadFileFNCallback_t file_read, void* context);

ApeProgram_t* ape_compile(ApeContext_t* ape, const char* code);
ApeProgram_t* ape_compile_file(ApeContext_t* ape, const char* path);
ApeObject_t ape_execute_program(ApeContext_t* ape, const ApeProgram_t* program);
void ape_program_destroy(ApeProgram_t* program);

ApeObject_t ape_execute(ApeContext_t* ape, const char* code);
ApeObject_t ape_execute_file(ApeContext_t* ape, const char* path);

ApeObject_t ape_call(ApeContext_t* ape, const char* function_name, int argc, ApeObject_t* args);


void ape_set_runtime_error(ApeContext_t* ape, const char* message);
void ape_set_runtime_errorf(ApeContext_t* ape, const char* format, ...);
bool ape_has_errors(const ApeContext_t* ape);
int ape_errors_count(const ApeContext_t* ape);
void ape_clear_errors(ApeContext_t* ape);
const ApeError_t* ape_get_error(const ApeContext_t* ape, int index);

bool ape_set_native_function(ApeContext_t* ape, const char* name, ApeUserFNCallback_t fn, void* data);
bool ape_set_global_constant(ApeContext_t* ape, const char* name, ApeObject_t obj);
ApeObject_t ape_get_object(ApeContext_t* ape, const char* name);

bool ape_check_args(ApeContext_t* ape, bool generate_error, int argc, ApeObject_t* args, int expected_argc, int* expected_types);

// Ape object
ApeObject_t ape_object_make_number(double val);
ApeObject_t ape_object_make_bool(bool val);
ApeObject_t ape_object_make_null(void);
ApeObject_t ape_object_make_string(ApeContext_t* ape, const char* str);
ApeObject_t ape_object_make_stringf(ApeContext_t* ape, const char* format, ...);
ApeObject_t ape_object_make_array(ApeContext_t* ape);
ApeObject_t ape_object_make_map(ApeContext_t* ape);
ApeObject_t ape_object_make_native_function(ApeContext_t* ape, ApeUserFNCallback_t fn, void* data);
ApeObject_t ape_object_make_error(ApeContext_t* ape, const char* message);
ApeObject_t ape_object_make_errorf(ApeContext_t* ape, const char* format, ...);
ApeObject_t ape_object_make_external(ApeContext_t* ape, void* data);

char* ape_object_serialize(ApeContext_t* ape, ApeObject_t obj, size_t* lendest);

bool ape_object_disable_gc(ApeObject_t obj);
void ape_object_enable_gc(ApeObject_t obj);

bool ape_object_equals(ApeObject_t a, ApeObject_t b);

bool ape_object_is_null(ApeObject_t obj);

ApeObject_t ape_object_copy(ApeObject_t obj);
ApeObject_t ape_object_deep_copy(ApeObject_t obj);

ApeObjectType_t ape_object_get_type(ApeObject_t obj);
const char* ape_object_get_type_string(ApeObject_t obj);
const char* ape_object_get_type_name(ApeObjectType_t type);

double ape_object_get_number(ApeObject_t obj);
bool ape_object_get_bool(ApeObject_t obj);
const char* ape_object_get_string(ApeObject_t obj);

const char* ape_object_get_error_message(ApeObject_t obj);
const ApeTraceback_t* ape_object_get_error_traceback(ApeObject_t obj);

bool ape_object_set_external_destroy_function(ApeObject_t object, ApeDataDestroyFNCallback_t destroy_fn);
bool ape_object_set_external_copy_function(ApeObject_t object, ApeDataCopyFNCallback_t copy_fn);

// Ape object array
int ape_object_get_array_length(ApeObject_t obj);

ApeObject_t ape_object_get_array_value(ApeObject_t object, int ix);
const char* ape_object_get_array_string(ApeObject_t object, int ix);
double ape_object_get_array_number(ApeObject_t object, int ix);
bool ape_object_get_array_bool(ApeObject_t object, int ix);

bool ape_object_set_array_value(ApeObject_t object, int ix, ApeObject_t value);
bool ape_object_set_array_string(ApeObject_t object, int ix, const char* string);
bool ape_object_set_array_number(ApeObject_t object, int ix, double number);
bool ape_object_set_array_bool(ApeObject_t object, int ix, bool value);

bool ape_object_add_array_value(ApeObject_t object, ApeObject_t value);
bool ape_object_add_array_string(ApeObject_t object, const char* string);
bool ape_object_add_array_number(ApeObject_t object, double number);
bool ape_object_add_array_bool(ApeObject_t object, bool value);

// Ape object map
int ape_object_get_map_length(ApeObject_t obj);
ApeObject_t ape_object_get_map_key_at(ApeObject_t object, int ix);
ApeObject_t ape_object_get_map_value_at(ApeObject_t object, int ix);
bool ape_object_set_map_value_at(ApeObject_t object, int ix, ApeObject_t val);

bool ape_object_set_map_value_with_value_key(ApeObject_t object, ApeObject_t key, ApeObject_t value);
bool ape_object_set_map_value(ApeObject_t object, const char* key, ApeObject_t value);
bool ape_object_set_map_string(ApeObject_t object, const char* key, const char* string);
bool ape_object_set_map_number(ApeObject_t object, const char* key, double number);
bool ape_object_set_map_bool(ApeObject_t object, const char* key, bool value);

ApeObject_t ape_object_get_map_value_with_value_key(ApeObject_t object, ApeObject_t key);
ApeObject_t ape_object_get_map_value(ApeObject_t object, const char* key);
const char* ape_object_get_map_string(ApeObject_t object, const char* key);
double ape_object_get_map_number(ApeObject_t object, const char* key);
bool ape_object_get_map_bool(ApeObject_t object, const char* key);

bool ape_object_map_has_key(ApeObject_t object, const char* key);

// Ape error
const char* ape_error_get_message(const ApeError_t* error);
const char* ape_error_get_filepath(const ApeError_t* error);
const char* ape_error_get_line(const ApeError_t* error);
int ape_error_get_line_number(const ApeError_t* error);
int ape_error_get_column_number(const ApeError_t* error);
ApeErrorType_t ape_error_get_type(const ApeError_t* error);
const char* ape_error_get_type_string(const ApeError_t* error);
const char* ape_error_type_to_string(ApeErrorType_t type);
char* ape_error_serialize(ApeContext_t* ape, const ApeError_t* error);
const ApeTraceback_t* ape_error_get_traceback(const ApeError_t* error);

// Ape traceback
int ape_traceback_get_depth(const ApeTraceback_t* traceback);
const char* ape_traceback_get_filepath(const ApeTraceback_t* traceback, int depth);
const char* ape_traceback_get_line(const ApeTraceback_t* traceback, int depth);
int ape_traceback_get_line_number(const ApeTraceback_t* traceback, int depth);
int ape_traceback_get_column_number(const ApeTraceback_t* traceback, int depth);
const char* ape_traceback_get_function_name(const ApeTraceback_t* traceback, int depth);


