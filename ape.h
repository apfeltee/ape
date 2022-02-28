
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
               sizeof((object_type_t[]){ __VA_ARGS__ }) / sizeof(object_type_t), (object_type_t[]){ __VA_ARGS__ })


#define APE_CALL(ape, function_name, ...) \
    ape_call((ape), (function_name), sizeof((ApeObject_t[]){ __VA_ARGS__ }) / sizeof(ApeObject_t), (ApeObject_t[]){ __VA_ARGS__ })

#define APE_CHECK_ARGS(ape, generate_error, argc, args, ...) \
    ape_check_args((ape), (generate_error), (argc), (args), sizeof((int[]){ __VA_ARGS__ }) / sizeof(int), (int[]){ __VA_ARGS__ })


#define dict(TYPE) dictionary_t

#define dict_make(alloc, copy_fn, destroy_fn) \
    dict_make_(alloc, (ApeDictItemCopyFNCallback_t)(copy_fn), (ApeDictItemDestroyFNCallback_t)(destroy_fn))

#define valdict(KEY_TYPE, VALUE_TYPE) valdict_t

#define valdict_make(allocator, key_type, val_type) valdict_make_(allocator, sizeof(key_type), sizeof(val_type))

#define ptrdict(KEY_TYPE, VALUE_TYPE) ptrdictionary_t

#define array(TYPE) array_t

#define array_make(allocator, type) array_make_(allocator, sizeof(type))
#define array_destroy_with_items(arr, fn) array_destroy_with_items_(arr, (ApeArrayItemDeinitFNCallback_t)(fn))
#define array_clear_and_deinit_items(arr, fn) array_clear_and_deinit_items_(arr, (ApeArrayItemDeinitFNCallback_t)(fn))

#define ptrarray(TYPE) ptrarray_t

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

enum token_type_t
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


enum operator_t
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

enum expression_type_t
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


enum statement_type_t
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

enum object_type_t
{
    OBJECT_NONE = 0,
    OBJECT_ERROR = 1 << 0,
    OBJECT_NUMBER = 1 << 1,
    OBJECT_BOOL = 1 << 2,
    OBJECT_STRING = 1 << 3,
    OBJECT_NULL = 1 << 4,
    OBJECT_NATIVE_FUNCTION = 1 << 5,
    OBJECT_ARRAY = 1 << 6,
    OBJECT_MAP = 1 << 7,
    OBJECT_FUNCTION = 1 << 8,
    OBJECT_EXTERNAL = 1 << 9,
    OBJECT_FREED = 1 << 10,
    OBJECT_ANY = 0xffff,
};

enum symbol_type_t
{
    SYMBOL_NONE = 0,
    SYMBOL_MODULE_GLOBAL,
    SYMBOL_LOCAL,
    SYMBOL_APE_GLOBAL,
    SYMBOL_FREE,
    SYMBOL_FUNCTION,
    SYMBOL_THIS,
};


enum opcode_val_t
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

enum precedence_t
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


typedef enum token_type_t token_type_t;
typedef enum statement_type_t statement_type_t;
typedef enum operator_t operator_t;
typedef enum symbol_type_t symbol_type_t;
typedef enum object_type_t object_type_t;
typedef enum expression_type_t expression_type_t;
typedef enum opcode_val_t opcode_val_t;
typedef enum precedence_t precedence_t;
typedef enum ApeObjectType_t ApeObjectType_t;
typedef enum ApeErrorType_t ApeErrorType_t;
typedef struct ApeContext_t ApeContext_t;
typedef struct ApeProgram_t ApeProgram_t;
typedef struct ApeVM_t ApeVM_t;
typedef struct ape_config_t ape_config_t;
typedef struct src_pos_t src_pos_t;
typedef struct ape_config_t ape_config_t;
typedef struct ape_timer_t ape_timer_t;
typedef struct allocator_t allocator_t;
typedef struct error_t error_t;
typedef struct errors_t errors_t;
typedef struct token_t token_t;
typedef struct compiled_file_t compiled_file_t;
typedef struct lexer_t lexer_t;
typedef struct code_block_t code_block_t;
typedef struct map_literal_t map_literal_t;
typedef struct prefix_expression_t prefix_expression_t;
typedef struct infix_expression_t infix_expression_t;
typedef struct if_case_t if_case_t;
typedef struct fn_literal_t fn_literal_t;
typedef struct call_expression_t call_expression_t;
typedef struct index_expression_t index_expression_t;
typedef struct assign_expression_t assign_expression_t;
typedef struct logical_expression_t logical_expression_t;
typedef struct ternary_expression_t ternary_expression_t;
typedef struct ident_t ident_t;
typedef struct expression_t expression_t;
typedef struct define_statement_t define_statement_t;
typedef struct if_statement_t if_statement_t;
typedef struct while_loop_statement_t while_loop_statement_t;
typedef struct foreach_statement_t foreach_statement_t;
typedef struct for_loop_statement_t for_loop_statement_t;
typedef struct import_statement_t import_statement_t;
typedef struct recover_statement_t recover_statement_t;
typedef struct statement_t statement_t;
typedef struct parser_t parser_t;
typedef struct ApeObject_t ApeObject_t;
typedef struct function_t function_t;
typedef struct native_function_t native_function_t;
typedef struct external_data_t external_data_t;
typedef struct object_error_t object_error_t;
typedef struct object_string_t object_string_t;
typedef struct object_data_t object_data_t;
typedef struct symbol_t symbol_t;
typedef struct block_scope_t block_scope_t;
typedef struct symbol_table_t symbol_table_t;
typedef struct opcode_definition_t opcode_definition_t;
typedef struct compilation_result_t compilation_result_t;
typedef struct compilation_scope_t compilation_scope_t;
typedef struct object_data_pool_t object_data_pool_t;
typedef struct gcmem_t gcmem_t;
typedef struct traceback_item_t traceback_item_t;
typedef struct traceback_t traceback_t;
typedef struct frame_t frame_t;
typedef struct valdict_t valdict_t;
typedef struct dictionary_t dictionary_t;
typedef struct array_t array_t;
typedef struct ptrdictionary_t ptrdictionary_t;
typedef struct ptrarray_t ptrarray_t;
typedef struct strbuf_t strbuf_t;
typedef struct global_store_t global_store_t;
typedef struct module_t module_t;
typedef struct file_scope_t file_scope_t;
typedef struct compiler_t compiler_t;
typedef struct native_fn_wrapper_t native_fn_wrapper_t;
typedef struct ApeObject_t ApeObject_t;

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
typedef expression_t* (*ApeRightAssocParseFNCallback_t)(parser_t* p);
typedef expression_t* (*ApeLeftAssocParseFNCallback_t)(parser_t* p, expression_t* expr);

typedef void (*ApeExternalDataDestroyFNCallback_t)(void* data);
typedef void* (*ApeExternalDataCopyFNCallback_t)(void* data);


struct src_pos_t
{
    const compiled_file_t* file;
    int line;
    int column;
};

struct ape_config_t
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

struct ape_timer_t
{
#if defined(APE_POSIX)
    int64_t start_offset;
#elif defined(APE_WINDOWS)
    double pc_frequency;
#endif
    double start_time_ms;
};

struct allocator_t
{
    ApeAllocatorMallocFNCallback_t malloc;
    ApeAllocatorFreeFNCallback_t free;
    void* ctx;
};

struct error_t
{
    ApeErrorType_t type;
    char message[APE_ERROR_MESSAGE_MAX_LENGTH];
    src_pos_t pos;
    traceback_t* traceback;
};

struct errors_t
{
    error_t errors[ERRORS_MAX_COUNT];
    int count;
};

struct token_t
{
    token_type_t type;
    const char* literal;
    int len;
    src_pos_t pos;
};

struct compiled_file_t
{
    allocator_t* alloc;
    char* dir_path;
    char* path;
    ptrarray(char*) * lines;
};

struct lexer_t
{
    allocator_t* alloc;
    errors_t* errors;
    const char* input;
    int input_len;
    int position;
    int next_position;
    char ch;
    int line;
    int column;
    compiled_file_t* file;
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
    token_t prev_token;
    token_t cur_token;
    token_t peek_token;
};

struct code_block_t
{
    allocator_t* alloc;
    ptrarray(statement_t) * statements;
};

struct map_literal_t
{
    ptrarray(expression_t) * keys;
    ptrarray(expression_t) * values;
};


struct prefix_expression_t
{
    operator_t op;
    expression_t* right;
};

struct infix_expression_t
{
    operator_t op;
    expression_t* left;
    expression_t* right;
};

struct if_case_t
{
    allocator_t* alloc;
    expression_t* test;
    code_block_t* consequence;
};

struct fn_literal_t
{
    char* name;
    ptrarray(ident_t) * params;
    code_block_t* body;
};

struct call_expression_t
{
    expression_t* function;
    ptrarray(expression_t) * args;
};

struct index_expression_t
{
    expression_t* left;
    expression_t* index;
};

struct assign_expression_t
{
    expression_t* dest;
    expression_t* source;
    bool is_postfix;
};

struct logical_expression_t
{
    operator_t op;
    expression_t* left;
    expression_t* right;
};

struct ternary_expression_t
{
    expression_t* test;
    expression_t* if_true;
    expression_t* if_false;
};

struct ident_t
{
    allocator_t* alloc;
    char* value;
    src_pos_t pos;
};

struct expression_t
{
    allocator_t* alloc;
    expression_type_t type;
    union
    {
        ident_t* ident;
        double number_literal;
        bool bool_literal;
        char* string_literal;
        ptrarray(expression_t) * array;
        map_literal_t map;
        prefix_expression_t prefix;
        infix_expression_t infix;
        fn_literal_t fn_literal;
        call_expression_t call_expr;
        index_expression_t index_expr;
        assign_expression_t assign;
        logical_expression_t logical;
        ternary_expression_t ternary;
    };
    src_pos_t pos;
};

struct define_statement_t
{
    ident_t* name;
    expression_t* value;
    bool assignable;
};

struct if_statement_t
{
    ptrarray(if_case_t) * cases;
    code_block_t* alternative;
};

struct while_loop_statement_t
{
    expression_t* test;
    code_block_t* body;
};

struct foreach_statement_t
{
    ident_t* iterator;
    expression_t* source;
    code_block_t* body;
};

struct for_loop_statement_t
{
    statement_t* init;
    expression_t* test;
    expression_t* update;
    code_block_t* body;
};

struct import_statement_t
{
    char* path;
};

struct recover_statement_t
{
    ident_t* error_ident;
    code_block_t* body;
};

struct statement_t
{
    allocator_t* alloc;
    statement_type_t type;
    union
    {
        define_statement_t define;
        if_statement_t if_statement;
        expression_t* return_value;
        expression_t* expression;
        while_loop_statement_t while_loop;
        foreach_statement_t foreach;
        for_loop_statement_t for_loop;
        code_block_t* block;
        import_statement_t import;
        recover_statement_t recover;
    };
    src_pos_t pos;
};

struct parser_t
{
    allocator_t* alloc;
    const ape_config_t* config;
    lexer_t lexer;
    errors_t* errors;

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


struct function_t
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
    compilation_result_t* comp_result;
    int num_locals;
    int num_args;
    int free_vals_count;
    bool owns_data;
};

struct native_function_t
{
    char* name;
    ApeNativeFNCallback_t fn;
    uint8_t data[NATIVE_FN_MAX_DATA_LEN];
    int data_len;
};

struct external_data_t
{
    void* data;
    ApeExternalDataDestroyFNCallback_t data_destroy_fn;
    ApeExternalDataCopyFNCallback_t data_copy_fn;
};

struct object_error_t
{
    char* message;
    traceback_t* traceback;
};

struct object_string_t
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

struct object_data_t
{
    gcmem_t* mem;
    union
    {
        object_string_t string;
        object_error_t error;
        array(ApeObject_t) * array;
        valdict(ApeObject_t, ApeObject_t) * map;
        function_t function;
        native_function_t native_function;
        external_data_t external;
    };
    bool gcmark;
    object_type_t type;
};

struct symbol_t
{
    allocator_t* alloc;
    symbol_type_t type;
    char* name;
    int index;
    bool assignable;
};

struct block_scope_t
{
    allocator_t* alloc;
    dict(symbol_t) * store;
    int offset;
    int num_definitions;
};

struct symbol_table_t
{
    allocator_t* alloc;
    symbol_table_t* outer;
    global_store_t* global_store;
    ptrarray(block_scope_t) * block_scopes;
    ptrarray(symbol_t) * free_symbols;
    ptrarray(symbol_t) * module_global_symbols;
    int max_num_definitions;
    int module_global_offset;
};

struct opcode_definition_t
{
    const char* name;
    int num_operands;
    int operand_widths[2];
};

struct compilation_result_t
{
    allocator_t* alloc;
    uint8_t* bytecode;
    src_pos_t* src_positions;
    int count;
};

struct compilation_scope_t
{
    allocator_t* alloc;
    compilation_scope_t* outer;
    array(uint8_t) * bytecode;
    array(src_pos_t) * src_positions;
    array(int) * break_ip_stack;
    array(int) * continue_ip_stack;
    opcode_t last_opcode;
};

struct object_data_pool_t
{
    object_data_t* data[GCMEM_POOL_SIZE];
    int count;
};

struct gcmem_t
{
    allocator_t* alloc;
    int allocations_since_sweep;
    ptrarray(object_data_t) * objects;
    ptrarray(object_data_t) * objects_back;
    array(ApeObject_t) * objects_not_gced;
    object_data_pool_t data_only_pool;
    object_data_pool_t pools[GCMEM_POOLS_NUM];
};

struct traceback_item_t
{
    char* function_name;
    src_pos_t pos;
};

struct traceback_t
{
    allocator_t* alloc;
    array(traceback_item_t) * items;
};

struct frame_t
{
    ApeObject_t function;
    int ip;
    int base_pointer;
    const src_pos_t* src_positions;
    uint8_t* bytecode;
    int src_ip;
    int bytecode_size;
    int recover_ip;
    bool is_recovering;
};

struct ApeVM_t
{
    allocator_t* alloc;
    const ape_config_t* config;
    gcmem_t* mem;
    errors_t* errors;
    global_store_t* global_store;
    ApeObject_t globals[VM_MAX_GLOBALS];
    int globals_count;
    ApeObject_t stack[VM_STACK_SIZE];
    int sp;
    ApeObject_t this_stack[VM_THIS_STACK_SIZE];
    int this_sp;
    frame_t frames[VM_MAX_FRAMES];
    int frames_count;
    ApeObject_t last_popped;
    frame_t* current_frame;
    bool running;
    ApeObject_t operator_oveload_keys[OPCODE_MAX];
};

struct valdict_t
{
    allocator_t* alloc;
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

struct dictionary_t
{
    allocator_t* alloc;
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

struct array_t
{
    allocator_t* alloc;
    unsigned char* data;
    unsigned char* data_allocated;
    unsigned int count;
    unsigned int capacity;
    size_t element_size;
    bool lock_capacity;
};

struct ptrdictionary_t
{
    allocator_t* alloc;
    valdict_t vd;
};

struct ptrarray_t
{
    allocator_t* alloc;
    array_t arr;
};

struct strbuf_t
{
    allocator_t* alloc;
    bool failed;
    char* data;
    size_t capacity;
    size_t len;
};

struct global_store_t
{
    allocator_t* alloc;
    dict(symbol_t) * symbols;
    array(ApeObject_t) * objects;
};

struct module_t
{
    allocator_t* alloc;
    char* name;
    ptrarray(symbol_t) * symbols;
};

struct file_scope_t
{
    allocator_t* alloc;
    parser_t* parser;
    symbol_table_t* symbol_table;
    compiled_file_t* file;
    ptrarray(char) * loaded_module_names;
};

struct compiler_t
{
    allocator_t* alloc;
    const ape_config_t* config;
    gcmem_t* mem;
    errors_t* errors;
    ptrarray(compiled_file_t) * files;
    global_store_t* global_store;
    array(ApeObject_t) * constants;
    compilation_scope_t* compilation_scope;
    ptrarray(file_scope_t) * file_scopes;
    array(src_pos_t) * src_positions_stack;
    dict(module_t) * modules;
    dict(int) * string_constants_positions;
};

struct native_fn_wrapper_t
{
    ApeUserFNCallback_t fn;
    ApeContext_t* ape;
    void* data;
};

struct ApeProgram_t
{
    ApeContext_t* ape;
    compilation_result_t* comp_res;
};

struct ApeContext_t
{
    allocator_t alloc;
    gcmem_t* mem;
    ptrarray(compiled_file_t) * files;
    global_store_t* global_store;
    compiler_t* compiler;
    ApeVM_t* vm;
    errors_t errors;
    ape_config_t config;
    allocator_t custom_allocator;
};

static const src_pos_t src_pos_invalid = { NULL, -1, -1 };
static const src_pos_t src_pos_zero = { NULL, 0, 0 };

src_pos_t src_pos_make(const compiled_file_t* file, int line, int column);
char* ape_stringf(allocator_t* alloc, const char* format, ...);
void ape_log(const char* file, int line, const char* format, ...);
char* ape_strndup(allocator_t* alloc, const char* string, size_t n);
char* ape_strdup(allocator_t* alloc, const char* string);

uint64_t ape_double_to_uint64(double val);
double ape_uint64_to_double(uint64_t val);

bool ape_timer_platform_supported(void);
ape_timer_t ape_timer_start(void);
double ape_timer_get_elapsed_ms(const ape_timer_t* timer);


//-----------------------------------------------------------------------------
// Collections
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Allocator
//-----------------------------------------------------------------------------


allocator_t allocator_make(ApeAllocatorMallocFNCallback_t malloc_fn, ApeAllocatorFreeFNCallback_t free_fn, void* ctx);
void* allocator_malloc(allocator_t* allocator, size_t size);
void allocator_free(allocator_t* allocator, void* ptr);

//-----------------------------------------------------------------------------
// Dictionary
//-----------------------------------------------------------------------------


dictionary_t* dict_make_(allocator_t* alloc, ApeDictItemCopyFNCallback_t copy_fn, ApeDictItemDestroyFNCallback_t destroy_fn);
void dict_destroy(dictionary_t* dict);
void dict_destroy_with_items(dictionary_t* dict);
dictionary_t* dict_copy_with_items(dictionary_t* dict);
bool dict_set(dictionary_t* dict, const char* key, void* value);
void* dict_get(const dictionary_t* dict, const char* key);
void* dict_get_value_at(const dictionary_t* dict, unsigned int ix);
const char* dict_get_key_at(const dictionary_t* dict, unsigned int ix);
int dict_count(const dictionary_t* dict);
bool dict_remove(dictionary_t* dict, const char* key);

//-----------------------------------------------------------------------------
// Value dictionary
//-----------------------------------------------------------------------------


valdict_t* valdict_make_(allocator_t* alloc, size_t key_size, size_t val_size);
valdict_t* valdict_make_with_capacity(allocator_t* alloc, unsigned int min_capacity, size_t key_size, size_t val_size);
void valdict_destroy(valdict_t* dict);
void valdict_set_hash_function(valdict_t* dict, ApeCollectionsHashFNCallback_t hash_fn);
void valdict_set_equals_function(valdict_t* dict, ApeCollectionsEqualsFNCallback_t equals_fn);
bool valdict_set(valdict_t* dict, void* key, void* value);
void* valdict_get(const valdict_t* dict, const void* key);
void* valdict_get_key_at(const valdict_t* dict, unsigned int ix);
void* valdict_get_value_at(const valdict_t* dict, unsigned int ix);
unsigned int valdict_get_capacity(const valdict_t* dict);
bool valdict_set_value_at(const valdict_t* dict, unsigned int ix, const void* value);
int valdict_count(const valdict_t* dict);
bool valdict_remove(valdict_t* dict, void* key);
void valdict_clear(valdict_t* dict);

//-----------------------------------------------------------------------------
// Pointer dictionary
//-----------------------------------------------------------------------------


ptrdictionary_t* ptrdict_make(allocator_t* alloc);
void ptrdict_destroy(ptrdictionary_t* dict);
void ptrdict_set_hash_function(ptrdictionary_t* dict, ApeCollectionsHashFNCallback_t hash_fn);
void ptrdict_set_equals_function(ptrdictionary_t* dict, ApeCollectionsEqualsFNCallback_t equals_fn);
bool ptrdict_set(ptrdictionary_t* dict, void* key, void* value);
void* ptrdict_get(const ptrdictionary_t* dict, const void* key);
void* ptrdict_get_value_at(const ptrdictionary_t* dict, unsigned int ix);
void* ptrdict_get_key_at(const ptrdictionary_t* dict, unsigned int ix);
int ptrdict_count(const ptrdictionary_t* dict);
bool ptrdict_remove(ptrdictionary_t* dict, void* key);

//-----------------------------------------------------------------------------
// Array
//-----------------------------------------------------------------------------


array_t* array_make_(allocator_t* alloc, size_t element_size);
array_t* array_make_with_capacity(allocator_t* alloc, unsigned int capacity, size_t element_size);
void array_destroy(array_t* arr);
void array_destroy_with_items_(array_t* arr, ApeArrayItemDeinitFNCallback_t deinit_fn);
array_t* array_copy(const array_t* arr);
bool array_add(array_t* arr, const void* value);
bool array_addn(array_t* arr, const void* values, int n);
bool array_add_array(array_t* dest, array_t* source);
bool array_push(array_t* arr, const void* value);
bool array_pop(array_t* arr, void* out_value);
void* array_top(array_t* arr);
bool array_set(array_t* arr, unsigned int ix, void* value);
bool array_setn(array_t* arr, unsigned int ix, void* values, int n);
void* array_get(array_t* arr, unsigned int ix);
const void* array_get_const(const array_t* arr, unsigned int ix);
void* array_get_last(array_t* arr);
int array_count(const array_t* arr);
unsigned int array_get_capacity(const array_t* arr);
bool array_remove_at(array_t* arr, unsigned int ix);
bool array_remove_item(array_t* arr, void* ptr);
void array_clear(array_t* arr);
void array_clear_and_deinit_items_(array_t* arr, ApeArrayItemDeinitFNCallback_t deinit_fn);
void array_lock_capacity(array_t* arr);
int array_get_index(const array_t* arr, void* ptr);
bool array_contains(const array_t* arr, void* ptr);
void* array_data(array_t* arr);// might become invalidated by remove/add operations
const void* array_const_data(const array_t* arr);
void array_orphan_data(array_t* arr);
bool array_reverse(array_t* arr);

//-----------------------------------------------------------------------------
// Pointer Array
//-----------------------------------------------------------------------------


ptrarray_t* ptrarray_make(allocator_t* alloc);
ptrarray_t* ptrarray_make_with_capacity(allocator_t* alloc, unsigned int capacity);
void ptrarray_destroy(ptrarray_t* arr);
void ptrarray_destroy_with_items_(ptrarray_t* arr, ApePtrArrayItemDestroyFNCallback_t destroy_fn);
ptrarray_t* ptrarray_copy(ptrarray_t* arr);
ptrarray_t* ptrarray_copy_with_items_(ptrarray_t* arr, ApePtrArrayItemCopyFNCallback_t copy_fn, ApePtrArrayItemDestroyFNCallback_t destroy_fn);
bool ptrarray_add(ptrarray_t* arr, void* ptr);
bool ptrarray_set(ptrarray_t* arr, unsigned int ix, void* ptr);
bool ptrarray_add_array(ptrarray_t* dest, ptrarray_t* source);
void* ptrarray_get(ptrarray_t* arr, unsigned int ix);
const void* ptrarray_get_const(const ptrarray_t* arr, unsigned int ix);
bool ptrarray_push(ptrarray_t* arr, void* ptr);
void* ptrarray_pop(ptrarray_t* arr);
void* ptrarray_top(ptrarray_t* arr);
int ptrarray_count(const ptrarray_t* arr);
bool ptrarray_remove_at(ptrarray_t* arr, unsigned int ix);
bool ptrarray_remove_item(ptrarray_t* arr, void* item);
void ptrarray_clear(ptrarray_t* arr);
void ptrarray_clear_and_destroy_items_(ptrarray_t* arr, ApePtrArrayItemDestroyFNCallback_t destroy_fn);
void ptrarray_lock_capacity(ptrarray_t* arr);
int ptrarray_get_index(ptrarray_t* arr, void* ptr);
bool ptrarray_contains(ptrarray_t* arr, void* ptr);
void* ptrarray_get_addr(ptrarray_t* arr, unsigned int ix);
void* ptrarray_data(ptrarray_t* arr);// might become invalidated by remove/add operations
void ptrarray_reverse(ptrarray_t* arr);

//-----------------------------------------------------------------------------
// String buffer
//-----------------------------------------------------------------------------


strbuf_t* strbuf_make(allocator_t* alloc);
strbuf_t* strbuf_make_with_capacity(allocator_t* alloc, unsigned int capacity);
void strbuf_destroy(strbuf_t* buf);
void strbuf_clear(strbuf_t* buf);
bool strbuf_append(strbuf_t* buf, const char* str);
bool strbuf_appendf(strbuf_t* buf, const char* fmt, ...);
const char* strbuf_get_string(const strbuf_t* buf);
size_t strbuf_get_length(const strbuf_t* buf);
char* strbuf_get_string_and_destroy(strbuf_t* buf);
bool strbuf_failed(strbuf_t* buf);

//-----------------------------------------------------------------------------
// Utils
//-----------------------------------------------------------------------------

ptrarray(char) * kg_split_string(allocator_t* alloc, const char* str, const char* delimiter);
char* kg_join(allocator_t* alloc, ptrarray(char) * items, const char* with);
char* kg_canonicalise_path(allocator_t* alloc, const char* path);
bool kg_is_path_absolute(const char* path);
bool kg_streq(const char* a, const char* b);


void errors_init(errors_t* errors);
void errors_deinit(errors_t* errors);

void errors_add_error(errors_t* errors, ApeErrorType_t type, src_pos_t pos, const char* message);
void errors_add_errorf(errors_t* errors, ApeErrorType_t type, src_pos_t pos, const char* format, ...);
void errors_clear(errors_t* errors);
int errors_get_count(const errors_t* errors);
error_t* errors_get(errors_t* errors, int ix);
const error_t* errors_getc(const errors_t* errors, int ix);
const char* error_type_to_string(ApeErrorType_t type);
error_t* errors_get_last_error(errors_t* errors);
bool errors_has_errors(const errors_t* errors);


void token_init(token_t* tok, token_type_t type, const char* literal, int len);// no need to destroy
char* token_duplicate_literal(allocator_t* alloc, const token_t* tok);
const char* token_type_to_string(token_type_t type);


compiled_file_t* compiled_file_make(allocator_t* alloc, const char* path);
void compiled_file_destroy(compiled_file_t* file);


bool lexer_init(lexer_t* lex, allocator_t* alloc, errors_t* errs, const char* input, compiled_file_t* file);// no need to deinit

bool lexer_failed(lexer_t* lex);
void lexer_continue_template_string(lexer_t* lex);
bool lexer_cur_token_is(lexer_t* lex, token_type_t type);
bool lexer_peek_token_is(lexer_t* lex, token_type_t type);
bool lexer_next_token(lexer_t* lex);
bool lexer_previous_token(lexer_t* lex);
token_t lexer_next_token_internal(lexer_t* lex);// exposed here for tests
bool lexer_expect_current(lexer_t* lex, token_type_t type);


char* statements_to_string(allocator_t* alloc, ptrarray(statement_t) * statements);

statement_t* statement_make_define(allocator_t* alloc, ident_t* name, expression_t* value, bool assignable);
statement_t* statement_make_if(allocator_t* alloc, ptrarray(if_case_t) * cases, code_block_t* alternative);
statement_t* statement_make_return(allocator_t* alloc, expression_t* value);
statement_t* statement_make_expression(allocator_t* alloc, expression_t* value);
statement_t* statement_make_while_loop(allocator_t* alloc, expression_t* test, code_block_t* body);
statement_t* statement_make_break(allocator_t* alloc);
statement_t* statement_make_foreach(allocator_t* alloc, ident_t* iterator, expression_t* source, code_block_t* body);
statement_t*
statement_make_for_loop(allocator_t* alloc, statement_t* init, expression_t* test, expression_t* update, code_block_t* body);
statement_t* statement_make_continue(allocator_t* alloc);
statement_t* statement_make_block(allocator_t* alloc, code_block_t* block);
statement_t* statement_make_import(allocator_t* alloc, char* path);
statement_t* statement_make_recover(allocator_t* alloc, ident_t* error_ident, code_block_t* body);

void statement_destroy(statement_t* stmt);

statement_t* statement_copy(const statement_t* stmt);

code_block_t* code_block_make(allocator_t* alloc, ptrarray(statement_t) * statements);
void code_block_destroy(code_block_t* stmt);
code_block_t* code_block_copy(code_block_t* block);

expression_t* expression_make_ident(allocator_t* alloc, ident_t* ident);
expression_t* expression_make_number_literal(allocator_t* alloc, double val);
expression_t* expression_make_bool_literal(allocator_t* alloc, bool val);
expression_t* expression_make_string_literal(allocator_t* alloc, char* value);
expression_t* expression_make_null_literal(allocator_t* alloc);
expression_t* expression_make_array_literal(allocator_t* alloc, ptrarray(expression_t) * values);
expression_t* expression_make_map_literal(allocator_t* alloc, ptrarray(expression_t) * keys, ptrarray(expression_t) * values);
expression_t* expression_make_prefix(allocator_t* alloc, operator_t op, expression_t* right);
expression_t* expression_make_infix(allocator_t* alloc, operator_t op, expression_t* left, expression_t* right);
expression_t* expression_make_fn_literal(allocator_t* alloc, ptrarray(ident_t) * params, code_block_t* body);
expression_t* expression_make_call(allocator_t* alloc, expression_t* function, ptrarray(expression_t) * args);
expression_t* expression_make_index(allocator_t* alloc, expression_t* left, expression_t* index);
expression_t* expression_make_assign(allocator_t* alloc, expression_t* dest, expression_t* source, bool is_postfix);
expression_t* expression_make_logical(allocator_t* alloc, operator_t op, expression_t* left, expression_t* right);
expression_t* expression_make_ternary(allocator_t* alloc, expression_t* test, expression_t* if_true, expression_t* if_false);

void expression_destroy(expression_t* expr);

expression_t* expression_copy(expression_t* expr);

void statement_to_string(const statement_t* stmt, strbuf_t* buf);
void expression_to_string(expression_t* expr, strbuf_t* buf);

void code_block_to_string(const code_block_t* stmt, strbuf_t* buf);
const char* operator_to_string(operator_t op);

const char* expression_type_to_string(expression_type_t type);

ident_t* ident_make(allocator_t* alloc, token_t tok);
ident_t* ident_copy(ident_t* ident);
void ident_destroy(ident_t* ident);

if_case_t* if_case_make(allocator_t* alloc, expression_t* test, code_block_t* consequence);
void if_case_destroy(if_case_t* cond);
if_case_t* if_case_copy(if_case_t* cond);


parser_t* parser_make(allocator_t* alloc, const ape_config_t* config, errors_t* errors);
void parser_destroy(parser_t* parser);

ptrarray(statement_t) * parser_parse_all(parser_t* parser, const char* input, compiled_file_t* file);


ApeObject_t object_make_from_data(object_type_t type, object_data_t* data);
ApeObject_t object_make_number(double val);
ApeObject_t object_make_bool(bool val);
ApeObject_t object_make_null(void);
ApeObject_t object_make_string(gcmem_t* mem, const char* string);
ApeObject_t object_make_string_with_capacity(gcmem_t* mem, int capacity);
ApeObject_t object_make_native_function(gcmem_t* mem, const char* name, ApeNativeFNCallback_t fn, void* data, int data_len);
ApeObject_t object_make_array(gcmem_t* mem);
ApeObject_t object_make_array_with_capacity(gcmem_t* mem, unsigned capacity);
ApeObject_t object_make_map(gcmem_t* mem);
ApeObject_t object_make_map_with_capacity(gcmem_t* mem, unsigned capacity);
ApeObject_t object_make_error(gcmem_t* mem, const char* message);
ApeObject_t object_make_error_no_copy(gcmem_t* mem, char* message);
ApeObject_t object_make_errorf(gcmem_t* mem, const char* fmt, ...);
ApeObject_t
object_make_function(gcmem_t* mem, const char* name, compilation_result_t* comp_res, bool owns_data, int num_locals, int num_args, int free_vals_count);
ApeObject_t object_make_external(gcmem_t* mem, void* data);

void object_deinit(ApeObject_t obj);
void object_data_deinit(object_data_t* obj);

bool object_is_allocated(ApeObject_t obj);
gcmem_t* object_get_mem(ApeObject_t obj);
bool object_is_hashable(ApeObject_t obj);
void object_to_string(ApeObject_t obj, strbuf_t* buf, bool quote_str);
const char* object_get_type_name(const object_type_t type);
char* object_get_type_union_name(allocator_t* alloc, const object_type_t type);
char* object_serialize(allocator_t* alloc, ApeObject_t object, size_t* lendest);
ApeObject_t object_deep_copy(gcmem_t* mem, ApeObject_t object);
ApeObject_t object_copy(gcmem_t* mem, ApeObject_t obj);
double object_compare(ApeObject_t a, ApeObject_t b, bool* out_ok);
bool object_equals(ApeObject_t a, ApeObject_t b);

object_data_t* object_get_allocated_data(ApeObject_t object);

bool object_get_bool(ApeObject_t obj);
double object_get_number(ApeObject_t obj);
function_t* object_get_function(ApeObject_t obj);
const char* object_get_string(ApeObject_t obj);
int object_get_string_length(ApeObject_t obj);
void object_set_string_length(ApeObject_t obj, int len);
int object_get_string_capacity(ApeObject_t obj);
char* object_get_mutable_string(ApeObject_t obj);
bool object_string_append(ApeObject_t obj, const char* src, int len);
unsigned long object_get_string_hash(ApeObject_t obj);
native_function_t* object_get_native_function(ApeObject_t obj);
object_type_t object_get_type(ApeObject_t obj);

bool object_is_numeric(ApeObject_t obj);
bool object_is_null(ApeObject_t obj);
bool object_is_callable(ApeObject_t obj);

const char* object_get_function_name(ApeObject_t obj);
ApeObject_t object_get_function_free_val(ApeObject_t obj, int ix);
void object_set_function_free_val(ApeObject_t obj, int ix, ApeObject_t val);
ApeObject_t* object_get_function_free_vals(ApeObject_t obj);

const char* object_get_error_message(ApeObject_t obj);
void object_set_error_traceback(ApeObject_t obj, traceback_t* traceback);
traceback_t* object_get_error_traceback(ApeObject_t obj);

external_data_t* object_get_external_data(ApeObject_t object);
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
ApeObject_t object_get_kv_pair_at(gcmem_t* mem, ApeObject_t obj, int ix);
bool object_set_map_value(ApeObject_t obj, ApeObject_t key, ApeObject_t val);
ApeObject_t object_get_map_value(ApeObject_t obj, ApeObject_t key);
bool object_map_has_key(ApeObject_t obj, ApeObject_t key);


global_store_t* global_store_make(allocator_t* alloc, gcmem_t* mem);
void global_store_destroy(global_store_t* store);
const symbol_t* global_store_get_symbol(global_store_t* store, const char* name);
ApeObject_t global_store_get_object(global_store_t* store, const char* name);
bool global_store_set(global_store_t* store, const char* name, ApeObject_t object);
ApeObject_t global_store_get_object_at(global_store_t* store, int ix, bool* out_ok);
bool global_store_set_object_at(global_store_t* store, int ix, ApeObject_t object);
ApeObject_t* global_store_get_object_data(global_store_t* store);
int global_store_get_object_count(global_store_t* store);


symbol_t* symbol_make(allocator_t* alloc, const char* name, symbol_type_t type, int index, bool assignable);
void symbol_destroy(symbol_t* symbol);
symbol_t* symbol_copy(symbol_t* symbol);

symbol_table_t* symbol_table_make(allocator_t* alloc, symbol_table_t* outer, global_store_t* global_store, int module_global_offset);
void symbol_table_destroy(symbol_table_t* st);
symbol_table_t* symbol_table_copy(symbol_table_t* st);
bool symbol_table_add_module_symbol(symbol_table_t* st, symbol_t* symbol);
const symbol_t* symbol_table_define(symbol_table_t* st, const char* name, bool assignable);
const symbol_t* symbol_table_define_free(symbol_table_t* st, const symbol_t* original);
const symbol_t* symbol_table_define_function_name(symbol_table_t* st, const char* name, bool assignable);
const symbol_t* symbol_table_define_this(symbol_table_t* st);

const symbol_t* symbol_table_resolve(symbol_table_t* st, const char* name);

bool symbol_table_symbol_is_defined(symbol_table_t* st, const char* name);
bool symbol_table_push_block_scope(symbol_table_t* table);
void symbol_table_pop_block_scope(symbol_table_t* table);
block_scope_t* symbol_table_get_block_scope(symbol_table_t* table);

bool symbol_table_is_module_global_scope(symbol_table_t* table);
bool symbol_table_is_top_block_scope(symbol_table_t* table);
bool symbol_table_is_top_global_scope(symbol_table_t* table);

int symbol_table_get_module_global_symbol_count(const symbol_table_t* table);
const symbol_t* symbol_table_get_module_global_symbol_at(const symbol_table_t* table, int ix);

opcode_definition_t* opcode_lookup(opcode_t op);
const char* opcode_get_name(opcode_t op);
int code_make(opcode_t op, int operands_count, uint64_t* operands, array(uint8_t) * res);
void code_to_string(uint8_t* code, src_pos_t* source_positions, size_t code_size, strbuf_t* res);
bool code_read_operands(opcode_definition_t* def, uint8_t* instr, uint64_t out_operands[2]);


compilation_scope_t* compilation_scope_make(allocator_t* alloc, compilation_scope_t* outer);
void compilation_scope_destroy(compilation_scope_t* scope);
compilation_result_t* compilation_scope_orphan_result(compilation_scope_t* scope);

compilation_result_t* compilation_result_make(allocator_t* alloc, uint8_t* bytecode, src_pos_t* src_positions, int count);
void compilation_result_destroy(compilation_result_t* res);

expression_t* optimise_expression(expression_t* expr);


compiler_t* compiler_make(allocator_t* alloc,
                          const ape_config_t* config,
                          gcmem_t* mem,
                          errors_t* errors,
                          ptrarray(compiled_file_t) * files,
                          global_store_t* global_store);
void compiler_destroy(compiler_t* comp);
compilation_result_t* compiler_compile(compiler_t* comp, const char* code);
compilation_result_t* compiler_compile_file(compiler_t* comp, const char* path);
symbol_table_t* compiler_get_symbol_table(compiler_t* comp);
void compiler_set_symbol_table(compiler_t* comp, symbol_table_t* table);
array(ApeObject_t) * compiler_get_constants(const compiler_t* comp);


gcmem_t* gcmem_make(allocator_t* alloc);
void gcmem_destroy(gcmem_t* mem);

object_data_t* gcmem_alloc_object_data(gcmem_t* mem, object_type_t type);
object_data_t* gcmem_get_object_data_from_pool(gcmem_t* mem, object_type_t type);

void gc_unmark_all(gcmem_t* mem);
void gc_mark_objects(ApeObject_t* objects, int count);
void gc_mark_object(ApeObject_t object);
void gc_sweep(gcmem_t* mem);

bool gc_disable_on_object(ApeObject_t obj);
void gc_enable_on_object(ApeObject_t obj);

int gc_should_sweep(gcmem_t* mem);


int builtins_count(void);
ApeNativeFNCallback_t builtins_get_fn(int ix);
const char* builtins_get_name(int ix);


traceback_t* traceback_make(allocator_t* alloc);
void traceback_destroy(traceback_t* traceback);
bool traceback_append(traceback_t* traceback, const char* function_name, src_pos_t pos);
bool traceback_append_from_vm(traceback_t* traceback, ApeVM_t* vm);
bool traceback_to_string(const traceback_t* traceback, strbuf_t* buf);
const char* traceback_item_get_line(traceback_item_t* item);
const char* traceback_item_get_filepath(traceback_item_t* item);


bool frame_init(frame_t* frame, ApeObject_t function, int base_pointer);

opcode_val_t frame_read_opcode(frame_t* frame);
uint64_t frame_read_uint64(frame_t* frame);
uint16_t frame_read_uint16(frame_t* frame);
uint8_t frame_read_uint8(frame_t* frame);
src_pos_t frame_src_position(const frame_t* frame);


ApeVM_t* vm_make(allocator_t* alloc, const ape_config_t* config, gcmem_t* mem, errors_t* errors, global_store_t* global_store);// config can be null (for internal testing purposes)
void vm_destroy(ApeVM_t* vm);

void vm_reset(ApeVM_t* vm);

bool vm_run(ApeVM_t* vm, compilation_result_t* comp_res, array(ApeObject_t) * constants);
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
const error_t* ape_get_error(const ApeContext_t* ape, int index);

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
const traceback_t* ape_object_get_error_traceback(ApeObject_t obj);

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
const char* ape_error_get_message(const error_t* error);
const char* ape_error_get_filepath(const error_t* error);
const char* ape_error_get_line(const error_t* error);
int ape_error_get_line_number(const error_t* error);
int ape_error_get_column_number(const error_t* error);
ApeErrorType_t ape_error_get_type(const error_t* error);
const char* ape_error_get_type_string(const error_t* error);
const char* ape_error_type_to_string(ApeErrorType_t type);
char* ape_error_serialize(ApeContext_t* ape, const error_t* error);
const traceback_t* ape_error_get_traceback(const error_t* error);

// Ape traceback
int ape_traceback_get_depth(const traceback_t* traceback);
const char* ape_traceback_get_filepath(const traceback_t* traceback, int depth);
const char* ape_traceback_get_line(const traceback_t* traceback, int depth);
int ape_traceback_get_line_number(const traceback_t* traceback, int depth);
int ape_traceback_get_column_number(const traceback_t* traceback, int depth);
const char* ape_traceback_get_function_name(const traceback_t* traceback, int depth);


