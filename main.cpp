

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
#include <sys/time.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <readline/readline.h>
#include <readline/history.h>


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

#ifdef __cplusplus
    #define AS_LIST
#else
    #define AS_LIST (uint64_t[])
#endif


#define APE_IMPL_VERSION_MAJOR 0
#define APE_IMPL_VERSION_MINOR 14
#define APE_IMPL_VERSION_PATCH 0
#define APE_VERSION_STRING "0.14.0"

#ifdef COLLECTIONS_DEBUG
    #define COLLECTIONS_ASSERT(x) assert(x)
#else
    #define COLLECTIONS_ASSERT(x)
#endif

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





#define APE_STREQ(a, b) (strcmp((a), (b)) == 0)
#define APE_STRNEQ(a, b, n) (strncmp((a), (b), (n)) == 0)
#define APE_ARRAY_LEN(array) ((int)(sizeof(array) / sizeof(array[0])))
#define APE_DBLEQ(a, b) (fabs((a) - (b)) < DBL_EPSILON)

#define APE_CALL(vm, function_name, ...) \
    vm_call(vm, (function_name), sizeof((Object[]){ __VA_ARGS__ }) / sizeof(Object), (Object[]){ __VA_ARGS__ })


#ifdef APE_DEBUG
    #define APE_ASSERT(x) assert((x))
    #define APE_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
    #define APE_LOG(...) ape_log(APE_FILENAME, __LINE__, __VA_ARGS__)
#else
    #define APE_ASSERT(x) ((void)0)
    #define APE_LOG(...) ((void)0)
#endif

enum ErrorType
{
    APE_ERROR_NONE = 0,
    APE_ERROR_PARSING,
    APE_ERROR_COMPILATION,
    APE_ERROR_RUNTIME,
    APE_ERROR_TIMEOUT,
    APE_ERROR_ALLOCATION,
    APE_ERROR_USER,// from ape_add_error() or ape_add_errorf()
};

enum ObjectType
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


enum TokenType
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


enum Operator
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

enum Expr_type
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
    EXPRESSION_DEFINE,
    EXPRESSION_IF,
    EXPRESSION_RETURN_VALUE,
    EXPRESSION_EXPRESSION,
    EXPRESSION_WHILE_LOOP,
    EXPRESSION_BREAK,
    EXPRESSION_CONTINUE,
    EXPRESSION_FOREACH,
    EXPRESSION_FOR_LOOP,
    EXPRESSION_BLOCK,
    EXPRESSION_IMPORT,
    EXPRESSION_RECOVER,
};

enum SymbolType
{
    SYMBOL_NONE = 0,
    SYMBOL_MODULE_GLOBAL,
    SYMBOL_LOCAL,
    SYMBOL_APE_GLOBAL,
    SYMBOL_FREE,
    SYMBOL_FUNCTION,
    SYMBOL_THIS,
};


enum OpcodeValue
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

enum Precedence
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


enum /**/TokenType;
enum /**/Operator;
enum /**/SymbolType;
enum /**/ObjectType;
enum /**/Expr_type;
enum /**/OpcodeValue;
enum /**/Precedence;
enum /**/ObjectType;
enum /**/ErrorType;
struct /**/Context;
struct /**/Program;
struct /**/VM;
struct /**/Config;
struct /**/Position;
struct /**/Timer;
struct /**/Allocator;
struct /**/Error;
struct /**/ErrorList;
struct /**/Token;
struct /**/CompiledFile;
struct /**/Lexer;
struct /**/Codeblock;
struct /**/MapLiteral;
struct /**/PrefixExpr;
struct /**/InfixExpr;
struct /**/IfCase;
struct /**/FnLiteral;
struct /**/CallExpr;
struct /**/IndexExpr;
struct /**/AssignExpr;
struct /**/LogicalExpr;
struct /**/TernaryExpr;
struct /**/Ident;
struct /**/Expression;
struct /**/DefineStmt;
struct /**/IfStmt;
struct /**/WhileLoopStmt;
struct /**/ForeachStmt;
struct /**/ForLoopStmt;
struct /**/ImportStmt;
struct /**/RecoverStmt;
struct /**/Expression;
struct /**/Parser;
struct /**/Object;
struct /**/FunctionObject;
struct /**/NativeFunction;
struct /**/ExternalData;
struct /**/ObjectError;
struct /**/ObjectString;
struct /**/ObjectData;
struct /**/Symbol;
struct /**/BlockScope;
struct /**/SymbolTable;
struct /**/OpcodeDefinition;
struct /**/CompilationResult;
struct /**/CompilationScope;
struct /**/ObjectDataPool;
struct /**/GCMemory;
struct /**/TracebackItem;
struct /**/Traceback;
struct /**/Frame;
struct /**/ValDictionary;
struct /**/Dictionary;
struct /**/Array;
struct /**/PtrDictionary;
struct /**/PtrArray;
struct /**/StringBuffer;
struct /**/GlobalStore;
struct /**/Module;
struct /**/FileScope;
struct /**/Compiler;
struct /**/NativeFuncWrapper;
struct /**/NatFunc_t;

//typedef Object (*ape_native_fn)(Context* ape, void* data, int argc, Object* args);
typedef Object (*UserFNCallback)(Context* ape, void* data, int argc, Object* args);
typedef Object (*NativeFNCallback)(VM*, void*, int, Object*);

typedef void* (*MallocFNCallback)(void* ctx, size_t size);
typedef void (*FreeFNCallback)(void* ctx, void* ptr);
typedef void (*DataDestroyFNCallback)(void* data);
typedef void* (*DataCopyFNCallback)(void* data);
typedef size_t (*StdoutWriteFNCallback)(void* context, const void* data, size_t data_size);
typedef char* (*ReadFileFNCallback)(void* context, const char* path);
typedef size_t (*WriteFileFNCallback)(void* context, const char* path, const char* string, size_t string_size);
typedef unsigned long (*CollectionsHashFNCallback)(const void* val);
typedef bool (*CollectionsEqualsFNCallback)(const void* a, const void* b);
typedef void* (*AllocatorMallocFNCallback)(void* ctx, size_t size);
typedef void (*AllocatorFreeFNCallback)(void* ctx, void* ptr);
typedef void (*DictItemDestroyFNCallback)(void* item);
typedef void* (*DictItemCopyFNCallback)(void* item);
typedef void (*ArrayItemDeinitFNCallback)(void* item);
typedef void (*PtrArrayItemDestroyFNCallback)(void* item);
typedef void* (*PtrArrayItemCopyFNCallback)(void* item);
typedef Expression* (*RightAssocParseFNCallback)(Parser* p);
typedef Expression* (*LeftAssocParseFNCallback)(Parser* p, Expression* expr);

typedef void (*ExternalDataDestroyFNCallback)(void* data);
typedef void* (*ExternalDataCopyFNCallback)(void* data);


/* builtins.c */
void builtins_install(VM *vm);
int builtins_count(void);
NativeFNCallback builtins_get_fn(int ix);
const char *builtins_get_name(int ix);
NativeFNCallback builtin_get_object(ObjectType objt, const char *idxname);
/* compiler.c */
Compiler *compiler_make(Allocator *alloc, const Config *config, GCMemory *mem, ErrorList *errors, PtrArray *files, GlobalStore *global_store);
void compiler_destroy(Compiler *comp);
CompilationResult *compiler_compile(Compiler *comp, const char *code);
CompilationResult *compiler_compile_file(Compiler *comp, const char *path);
SymbolTable *compiler_get_symbol_table(Compiler *comp);
Array *compiler_get_constants(const Compiler *comp);
bool compiler_init(Compiler *comp, Allocator *alloc, const Config *config, GCMemory *mem, ErrorList *errors, PtrArray *files, GlobalStore *global_store);
void compiler_deinit(Compiler *comp);
bool compiler_init_shallow_copy(Compiler *copy, Compiler *src);
/* imp.c */
char *ape_stringf(Allocator *alloc, const char *format, ...);
void ape_log(const char *file, int line, const char *format, ...);
char *ape_strndup(Allocator *alloc, const char *string, size_t n);
char *ape_strdup(Allocator *alloc, const char *string);
uint64_t ape_double_to_uint64(double val);
double ape_uint64_to_double(uint64_t val);
bool ape_timer_platform_supported(void);
Timer ape_timer_start(void);
double ape_timer_get_elapsed_ms(const Timer *timer);
char *collections_strndup(Allocator *alloc, const char *string, size_t n);
char *collections_strdup(Allocator *alloc, const char *string);
unsigned long collections_hash(const void *ptr, size_t len);
Allocator allocator_make(AllocatorMallocFNCallback malloc_fn, AllocatorFreeFNCallback free_fn, void *ctx);
void *allocator_malloc(Allocator *allocator, size_t size);
void allocator_free(Allocator *allocator, void *ptr);

StringBuffer *strbuf_make(Allocator *alloc);
StringBuffer *strbuf_make_with_capacity(Allocator *alloc, unsigned int capacity);
void strbuf_destroy(StringBuffer *buf);
void strbuf_clear(StringBuffer *buf);
bool strbuf_append(StringBuffer *buf, const char *str);
bool strbuf_appendf(StringBuffer *buf, const char *fmt, ...);
const char *strbuf_get_string(const StringBuffer *buf);
size_t strbuf_get_length(const StringBuffer *buf);
char *strbuf_get_string_and_destroy(StringBuffer *buf);
bool strbuf_failed(StringBuffer *buf);
bool strbuf_grow(StringBuffer *buf, size_t new_capacity);

PtrArray *kg_split_string(Allocator *alloc, const char *str, const char *delimiter);
char *kg_join(Allocator *alloc, PtrArray *items, const char *with);
char *kg_canonicalise_path(Allocator *alloc, const char *path);
bool kg_is_path_absolute(const char *path);
bool kg_streq(const char *a, const char *b);
void errors_add_error(ErrorList *errors, ErrorType type, Position pos, const char *message);
void errors_add_errorf(ErrorList *errors, ErrorType type, Position pos, const char *format, ...);
void errors_clear(ErrorList *errors);
int errors_get_count(const ErrorList *errors);
Error *errors_get(ErrorList *errors, int ix);
Error *errors_get_last_error(ErrorList *errors);
bool errors_has_errors(const ErrorList *errors);
CompiledFile *compiled_file_make(Allocator *alloc, const char *path);
void compiled_file_destroy(CompiledFile *file);
GlobalStore *global_store_make(Allocator *alloc, GCMemory *mem);
void global_store_destroy(GlobalStore *store);
const Symbol *global_store_get_symbol(GlobalStore *store, const char *name);
Object global_store_get_object(GlobalStore *store, const char *name);
bool global_store_set(GlobalStore *store, const char *name, Object object);
Object global_store_get_object_at(GlobalStore *store, int ix, bool *out_ok);
bool global_store_set_object_at(GlobalStore *store, int ix, Object object);
Object *global_store_get_object_data(GlobalStore *store);
int global_store_get_object_count(GlobalStore *store);

Symbol *symbol_make(Allocator *alloc, const char *name, SymbolType type, int index, bool assignable);
void symbol_destroy(Symbol *symbol);
Symbol *symbol_copy(Symbol *symbol);
SymbolTable *symbol_table_make(Allocator *alloc, SymbolTable *outer, GlobalStore *global_store, int module_global_offset);
void symbol_table_destroy(SymbolTable *table);
SymbolTable *symbol_table_copy(SymbolTable *table);
bool symbol_table_add_module_symbol(SymbolTable *st, Symbol *symbol);
const Symbol *symbol_table_define(SymbolTable *table, const char *name, bool assignable);
const Symbol *symbol_table_define_free(SymbolTable *st, const Symbol *original);
const Symbol *symbol_table_define_function_name(SymbolTable *st, const char *name, bool assignable);
const Symbol *symbol_table_define_this(SymbolTable *st);
const Symbol *symbol_table_resolve(SymbolTable *table, const char *name);
bool symbol_table_symbol_is_defined(SymbolTable *table, const char *name);
bool symbol_table_push_block_scope(SymbolTable *table);
void symbol_table_pop_block_scope(SymbolTable *table);
BlockScope *symbol_table_get_block_scope(SymbolTable *table);
bool symbol_table_is_module_global_scope(SymbolTable *table);
bool symbol_table_is_top_block_scope(SymbolTable *table);
bool symbol_table_is_top_global_scope(SymbolTable *table);
int symbol_table_get_module_global_symbol_count(const SymbolTable *table);

const Symbol *symbol_table_get_module_global_symbol_at(const SymbolTable *table, int ix);
BlockScope *block_scope_make(Allocator *alloc, int offset);
void block_scope_destroy(BlockScope *scope);
BlockScope *block_scope_copy(BlockScope *scope);
OpcodeDefinition *opcode_lookup(opcode_t op);
const char *opcode_get_name(opcode_t op);
int code_make(opcode_t op, int operands_count, uint64_t* operands, Array *res);
CompilationScope *compilation_scope_make(Allocator *alloc, CompilationScope *outer);
void compilation_scope_destroy(CompilationScope *scope);
CompilationResult *compilation_scope_orphan_result(CompilationScope *scope);
CompilationResult *compilation_result_make(Allocator *alloc, uint8_t *bytecode, Position *src_positions, int cn);
void compilation_result_destroy(CompilationResult *res);
Expression *optimise_expression(Expression *expr);
Expression *optimise_infix_expression(Expression *expr);
Expression *optimise_prefix_expression(Expression *expr);
Module *module_make(Allocator *alloc, const char *name);
void module_destroy(Module *module);
Module *module_copy(Module *src);
const char *get_module_name(const char *path);
bool module_add_symbol(Module *module, const Symbol *symbol);

Object object_make_from_data(ObjectType type, ObjectData *data);
Object object_make_number(double val);
Object object_make_bool(bool val);
Object object_make_null(void);
Object object_make_string_len(GCMemory *mem, const char *string, size_t len);
Object object_make_string(GCMemory *mem, const char *string);
Object object_make_string_with_capacity(GCMemory *mem, int capacity);
Object object_make_stringf(GCMemory *mem, const char *fmt, ...);
Object object_make_native_function_memory(GCMemory *mem, const char *name, NativeFNCallback fn, void *data, int data_len);
Object object_make_array(GCMemory *mem);
Object object_make_array_with_capacity(GCMemory *mem, unsigned capacity);
Object object_make_map(GCMemory *mem);
Object object_make_map_with_capacity(GCMemory *mem, unsigned capacity);
Object object_make_error(GCMemory *mem, const char *error);
Object object_make_error_no_copy(GCMemory *mem, char *error);
Object object_make_errorf(GCMemory *mem, const char *fmt, ...);
Object object_make_function(GCMemory *mem, const char *name, CompilationResult *comp_res, bool owns_data, int num_locals, int num_args, int free_vals_count);
Object object_make_external(GCMemory *mem, void *data);
void object_deinit(Object obj);
void object_data_deinit(ObjectData *data);
bool object_is_allocated(Object object);
GCMemory *object_get_mem(Object obj);
bool object_is_hashable(Object obj);
const char *object_get_type_name(const ObjectType type);
char *object_get_type_union_name(Allocator *alloc, const ObjectType type);
char *object_serialize(Allocator *alloc, Object object, size_t *lendest);
Object object_deep_copy(GCMemory *mem, Object obj);
Object object_copy(GCMemory *mem, Object obj);
double object_compare(Object a, Object b, bool *out_ok);
bool object_equals(Object a, Object b);
ExternalData *object_get_external_data(Object object);
bool object_set_external_destroy_function(Object object, ExternalDataDestroyFNCallback destroy_fn);
ObjectData *object_get_allocated_data(Object object);
bool object_get_bool(Object obj);
double object_get_number(Object obj);
const char *object_get_string(Object object);
int object_get_string_length(Object object);
void object_set_string_length(Object object, int len);
int object_get_string_capacity(Object object);
char *object_get_mutable_string(Object object);
bool object_string_append(Object obj, const char *src, int len);
unsigned long object_get_string_hash(Object obj);
FunctionObject *object_get_function(Object object);
NativeFunction *object_get_native_function(Object obj);
ObjectType object_get_type(Object obj);
bool object_is_numeric(Object obj);
bool object_is_null(Object obj);
bool object_is_callable(Object obj);
const char *object_get_function_name(Object obj);
Object object_get_function_free_val(Object obj, int ix);
void object_set_function_free_val(Object obj, int ix, Object val);
Object *object_get_function_free_vals(Object obj);
const char *object_get_error_message(Object object);
void object_set_error_traceback(Object object, Traceback *traceback);
Traceback *object_get_error_traceback(Object object);
bool object_set_external_data(Object object, void *ext_data);
bool object_set_external_copy_function(Object object, ExternalDataCopyFNCallback copy_fn);
Object object_get_array_value(Object object, int ix);
bool object_set_array_value_at(Object object, int ix, Object val);
bool object_add_array_value(Object object, Object val);
int object_get_array_length(Object object);
bool object_remove_array_value_at(Object object, int ix);
int object_get_map_length(Object object);
Object object_get_map_key_at(Object object, int ix);
Object object_get_map_value_at(Object object, int ix);
bool object_set_map_value_at(Object object, int ix, Object val);
Object object_get_kv_pair_at(GCMemory *mem, Object object, int ix);
bool object_set_map_value_object(Object object, Object key, Object val);
Object object_get_map_value_object(Object object, Object key);
bool object_map_has_key_object(Object object, Object key);
Object object_deep_copy_internal(GCMemory *mem, Object obj, ValDictionary *copies);
bool object_equals_wrapped(const Object *a_ptr, const Object *b_ptr);
unsigned long object_hash(Object *obj_ptr);
unsigned long object_hash_string(const char *str);
unsigned long object_hash_double(double val);
Array *object_get_allocated_array(Object object);
bool object_is_number(Object o);
char *object_data_get_string(ObjectData *data);
bool object_data_string_reserve_capacity(ObjectData *data, int capacity);
GCMemory *gcmem_make(Allocator *alloc);
void gcmem_destroy(GCMemory *mem);
ObjectData *gcmem_alloc_object_data(GCMemory *mem, ObjectType type);
ObjectData *gcmem_get_object_data_from_pool(GCMemory *mem, ObjectType type);
void gc_unmark_all(GCMemory *mem);
void gc_mark_objects(Object *objects, int cn);
void gc_mark_object(Object obj);
void gc_sweep(GCMemory *mem);
bool gc_disable_on_object(Object obj);
void gc_enable_on_object(Object obj);
int gc_should_sweep(GCMemory *mem);
ObjectDataPool *get_pool_for_type(GCMemory *mem, ObjectType type);
bool can_data_be_put_in_pool(GCMemory *mem, ObjectData *data);
Traceback *traceback_make(Allocator *alloc);
void traceback_destroy(Traceback *traceback);
bool traceback_append(Traceback *traceback, const char *function_name, Position pos);
bool traceback_append_from_vm(Traceback *traceback, VM *vm);
const char *traceback_item_get_line(TracebackItem *item);
const char *traceback_item_get_filepath(TracebackItem *item);
bool frame_init(Frame *frame, Object function_obj, int base_pointer);
OpcodeValue frame_read_opcode(Frame *frame);
uint64_t frame_read_uint64(Frame *frame);
uint16_t frame_read_uint16(Frame *frame);
uint8_t frame_read_uint8(Frame *frame);
Position frame_src_position(const Frame *frame);
VM *vm_make(Allocator *alloc, const Config *config, GCMemory *mem, ErrorList *errors, GlobalStore *global_store);
void vm_destroy(VM *vm);
void vm_reset(VM *vm);
bool vm_run(VM *vm, CompilationResult *comp_res, Array *constants);
Object vm_call(VM *vm, Array *constants, Object callee, int argc, Object *args);
bool vm_execute_function(VM *vm, Object function, Array *constants);
Object vm_get_last_popped(VM *vm);
bool vm_has_errors(VM *vm);
bool vm_set_global(VM *vm, int ix, Object val);
Object vm_get_global(VM *vm, int ix);
void stack_push(VM *vm, Object obj);
Object stack_pop(VM *vm);
Object stack_get(VM *vm, int nth_item);
bool push_frame(VM *vm, Frame frame);
bool pop_frame(VM *vm);
void run_gc(VM *vm, Array *constants);
bool call_object(VM *vm, Object callee, int num_args);
Object call_native_function(VM *vm, Object callee, Position src_pos, int argc, Object *args);
bool check_assign(VM *vm, Object old_value, Object new_value);
Context *ape_make(void);
Context *ape_make_ex(MallocFNCallback malloc_fn, FreeFNCallback free_fn, void *ctx);
void ape_destroy(Context *ape);
void ape_free_allocated(Context *ape, void *ptr);
void ape_set_repl_mode(Context *ape, bool enabled);
bool ape_set_timeout(Context *ape, double max_execution_time_ms);
void ape_set_stdout_write_function(Context *ape, StdoutWriteFNCallback stdout_write, void *context);
void ape_set_file_write_function(Context *ape, WriteFileFNCallback file_write, void *context);
void ape_set_file_read_function(Context *ape, ReadFileFNCallback file_read, void *context);
void ape_program_destroy(Program *program);
Object ape_execute(Context *ape, const char *code);
Object ape_execute_file(Context *ape, const char *path);
bool ape_has_errors(const Context *ape);
int ape_errors_count(const Context *ape);
void ape_clear_errors(Context *ape);
const Error *ape_get_error(const Context *ape, int index);
bool ape_set_native_function(Context *ape, const char *name, UserFNCallback fn, void *data);
bool ape_set_global_constant(Context *ape, const char *name, Object obj);
Object ape_get_object(Context *ape, const char *name);
Object ape_object_make_native_function(Context *ape, UserFNCallback fn, void *data);
bool ape_object_disable_gc(Object ape_obj);
void ape_object_enable_gc(Object ape_obj);
void ape_set_runtime_error(Context *ape, const char *message);
void ape_set_runtime_errorf(Context *ape, const char *fmt, ...);
const char *ape_object_get_array_string(Object obj, int ix);
double ape_object_get_array_number(Object obj, int ix);
bool ape_object_get_array_bool(Object obj, int ix);
bool ape_object_set_array_value(Object ape_obj, int ix, Object ape_value);
bool ape_object_set_array_string(Object obj, int ix, const char *string);
bool ape_object_set_array_number(Object obj, int ix, double number);
bool ape_object_set_array_bool(Object obj, int ix, bool value);
bool ape_object_add_array_string(Object obj, const char *string);
bool ape_object_add_array_number(Object obj, double number);
bool ape_object_add_array_bool(Object obj, bool value);
bool ape_object_set_map_value(Object obj, const char *key, Object value);
bool ape_object_set_map_string(Object obj, const char *key, const char *string);
bool ape_object_set_map_number(Object obj, const char *key, double number);
bool ape_object_set_map_bool(Object obj, const char *key, bool value);
Object ape_object_get_map_value(Object object, const char *key);
const char *ape_object_get_map_string(Object object, const char *key);
double ape_object_get_map_number(Object object, const char *key);
bool ape_object_get_map_bool(Object object, const char *key);
bool ape_object_map_has_key(Object ape_object, const char *key);
const char *ape_error_get_message(const Error *ae);
const char *ape_error_get_filepath(const Error *ae);
const char *ape_error_get_line(const Error *ae);
int ape_error_get_line_number(const Error *ae);
int ape_error_get_column_number(const Error *ae);
ErrorType ape_error_get_type(const Error *ae);
const char *ape_error_get_type_string(const Error *error);
char *ape_error_serialize(Context *ape, const Error *err);
const Traceback *ape_error_get_traceback(const Error *ae);
int ape_traceback_get_depth(const Traceback *ape_traceback);
const char *ape_traceback_get_filepath(const Traceback *ape_traceback, int depth);
const char *ape_traceback_get_line(const Traceback *ape_traceback, int depth);
int ape_traceback_get_line_number(const Traceback *ape_traceback, int depth);
int ape_traceback_get_column_number(const Traceback *ape_traceback, int depth);
const char *ape_traceback_get_function_name(const Traceback *ape_traceback, int depth);
void ape_deinit(Context *ape);
Object ape_native_fn_wrapper(VM *vm, void *data, int argc, Object *args);
Object ape_object_make_native_function_with_name(Context *ape, const char *name, UserFNCallback fn, void *data);
void reset_state(Context *ape);
void *ape_malloc(void *ctx, size_t size);
void ape_free(void *ctx, void *ptr);
/* lexer.c */
void token_init(Token *tok, TokenType type, const char *literal, int len);
char *token_duplicate_literal(Allocator *alloc, const Token *tok);
bool lexer_init(Lexer *lex, Allocator *alloc, ErrorList *errs, const char *input, CompiledFile *file);
bool lexer_failed(Lexer *lex);
void lexer_continue_template_string(Lexer *lex);
bool lexer_cur_token_is(Lexer *lex, TokenType type);
bool lexer_peek_token_is(Lexer *lex, TokenType type);
bool lexer_next_token(Lexer *lex);
bool lexer_previous_token(Lexer *lex);
Token lexer_next_token_internal(Lexer *lex);
bool lexer_expect_current(Lexer *lex, TokenType type);
/* main.c */
int main(int argc, char *argv[]);
/* parser.c */
Expression *expression_make_ident(Allocator *alloc, Ident *ident);
Expression *expression_make_number_literal(Allocator *alloc, double val);
Expression *expression_make_bool_literal(Allocator *alloc, bool val);
Expression *expression_make_string_literal(Allocator *alloc, char *value);
Expression *expression_make_null_literal(Allocator *alloc);
Expression *expression_make_array_literal(Allocator *alloc, PtrArray *values);
Expression *expression_make_map_literal(Allocator *alloc, PtrArray *keys, PtrArray *values);
Expression *expression_make_prefix(Allocator *alloc, Operator op, Expression *right);
Expression *expression_make_infix(Allocator *alloc, Operator op, Expression *left, Expression *right);
Expression *expression_make_fn_literal(Allocator *alloc, PtrArray *params, Codeblock *body);
Expression *expression_make_call(Allocator *alloc, Expression *function, PtrArray *args);
Expression *expression_make_index(Allocator *alloc, Expression *left, Expression *index);
Expression *expression_make_assign(Allocator *alloc, Expression *dest, Expression *source, bool is_postfix);
Expression *expression_make_logical(Allocator *alloc, Operator op, Expression *left, Expression *right);
Expression *expression_make_ternary(Allocator *alloc, Expression *test, Expression *if_true, Expression *if_false);
void expression_destroy(Expression *expr);
Expression *expression_copy(Expression *expr);
Expression *statement_make_define(Allocator *alloc, Ident *name, Expression *value, bool assignable);
Expression *statement_make_if(Allocator *alloc, PtrArray *cases, Codeblock *alternative);
Expression *statement_make_return(Allocator *alloc, Expression *value);
Expression *statement_make_expression(Allocator *alloc, Expression *value);
Expression *statement_make_while_loop(Allocator *alloc, Expression *test, Codeblock *body);
Expression *statement_make_break(Allocator *alloc);
Expression *statement_make_foreach(Allocator *alloc, Ident *iterator, Expression *source, Codeblock *body);
Expression *statement_make_for_loop(Allocator *alloc, Expression *init, Expression *test, Expression *update, Codeblock *body);
Expression *statement_make_continue(Allocator *alloc);
Expression *statement_make_block(Allocator *alloc, Codeblock *block);
Expression *statement_make_import(Allocator *alloc, char *path);
Expression *statement_make_recover(Allocator *alloc, Ident *error_ident, Codeblock *body);
void statement_destroy(Expression *stmt);
Expression *statement_copy(const Expression *stmt);
Codeblock *code_block_make(Allocator *alloc, PtrArray *statements);
void code_block_destroy(Codeblock *block);
Codeblock *code_block_copy(Codeblock *block);
void ident_destroy(Ident *ident);
IfCase *if_case_make(Allocator *alloc, Expression *test, Codeblock *consequence);
void if_case_destroy(IfCase *cond);
IfCase *if_case_copy(IfCase *if_case);
Expression *expression_make(Allocator *alloc, Expr_type type);
Expression *statement_make(Allocator *alloc, Expr_type type);
Parser *parser_make(Allocator *alloc, const Config *config, ErrorList *errors);
void parser_destroy(Parser *parser);
PtrArray *parser_parse_all(Parser *parser, const char *input, CompiledFile *file);
Expression *parse_statement(Parser *p);
Expression *parse_define_statement(Parser *p);
Expression *parse_if_statement(Parser *p);
Expression *parse_return_statement(Parser *p);
Expression *parse_expression_statement(Parser *p);
Expression *parse_while_loop_statement(Parser *p);
Expression *parse_break_statement(Parser *p);
Expression *parse_continue_statement(Parser *p);
Expression *parse_block_statement(Parser *p);
Expression *parse_import_statement(Parser *p);
Expression *parse_recover_statement(Parser *p);
Expression *parse_for_loop_statement(Parser *p);
Expression *parse_foreach(Parser *p);
Expression *parse_classic_for_loop(Parser *p);
Expression *parse_function_statement(Parser *p);
Codeblock *parse_code_block(Parser *p);
Expression *parse_expression(Parser *p, Precedence prec);
Expression *parse_identifier(Parser *p);
Expression *parse_number_literal(Parser *p);
Expression *parse_bool_literal(Parser *p);
Expression *parse_string_literal(Parser *p);
Expression *parse_template_string_literal(Parser *p);
Expression *parse_null_literal(Parser *p);
Expression *parse_array_literal(Parser *p);
Expression *parse_map_literal(Parser *p);
Expression *parse_prefix_expression(Parser *p);
Expression *parse_infix_expression(Parser *p, Expression *left);
Expression *parse_grouped_expression(Parser *p);
Expression *parse_function_literal(Parser *p);
bool parse_function_parameters(Parser *p, PtrArray *out_params);
Expression *parse_call_expression(Parser *p, Expression *left);
PtrArray *parse_expression_list(Parser *p, TokenType start_token, TokenType end_token, bool trailing_comma_allowed);
Expression *parse_index_expression(Parser *p, Expression *left);
Expression *parse_assign_expression(Parser *p, Expression *left);
Expression *parse_logical_expression(Parser *p, Expression *left);
Expression *parse_ternary_expression(Parser *p, Expression *left);
Expression *parse_incdec_prefix_expression(Parser *p);
Expression *parse_incdec_postfix_expression(Parser *p, Expression *left);
Expression *parse_dot_expression(Parser *p, Expression *left);
/* tostring.c */
char *statements_to_string(Allocator *alloc, PtrArray *statements);
void statement_to_string(const Expression *stmt, StringBuffer *buf);
void expression_to_string(Expression *expr, StringBuffer *buf);
void code_block_to_string(const Codeblock *stmt, StringBuffer *buf);
const char *operator_to_string(Operator op);
const char *expression_type_to_string(Expr_type type);
void code_to_string(uint8_t *code, Position *source_positions, size_t code_size, StringBuffer *res);
void object_to_string(Object obj, StringBuffer *buf, bool quote_str);
bool traceback_to_string(const Traceback *traceback, StringBuffer *buf);
const char *ape_error_type_to_string(ErrorType type);
const char *token_type_to_string(TokenType type);

static bool read_char(Lexer *lex);
static char peek_char(Lexer *lex);
static bool is_letter(char ch);
static bool is_digit(char ch);
static bool is_one_of(char ch, const char *allowed, int allowed_len);
static const char *read_identifier(Lexer *lex, int *out_len);
static const char *read_number(Lexer *lex, int *out_len);
static const char *read_string(Lexer *lex, char delimiter, bool is_template, bool *out_template_found, int *out_len);
static TokenType lookup_identifier(const char *ident, int len);
static void skip_whitespace(Lexer *lex);
static bool add_line(Lexer *lex, int offset);



static unsigned int upper_power_of_two(unsigned int v);
static void errors_init(ErrorList *errors);
static void errors_deinit(ErrorList *errors);
static const Error *errors_getc(const ErrorList *errors, int ix);
static bool set_symbol(SymbolTable *table, Symbol *symbol);
static int next_symbol_index(SymbolTable *table);
static int count_num_definitions(SymbolTable *table);
static uint64_t get_type_tag(ObjectType type);
static bool freevals_are_allocated(FunctionObject *fun);
static void set_sp(VM *vm, int new_sp);
static void this_stack_push(VM *vm, Object obj);
static Object this_stack_pop(VM *vm);
static Object this_stack_get(VM *vm, int nth_item);
static bool try_overload_operator(VM *vm, Object left, Object right, opcode_t op, bool *out_overload_found);
static void set_default_config(Context *ape);
static char *read_file_default(void *ctx, const char *filename);
static size_t write_file_default(void *ctx, const char *path, const char *string, size_t string_size);
static size_t stdout_write_default(void *ctx, const void *data, size_t size);


struct NatFunc_t
{
    const char* name;
    NativeFNCallback fn;
};

struct Position
{
    const CompiledFile* file;
    int line;
    int column;
};

struct Config
{
    struct
    {
        struct
        {
            StdoutWriteFNCallback write;
            void* context;
        } write;
    } stdio;

    struct
    {
        struct
        {
            ReadFileFNCallback read_file;
            void* context;
        } read_file;

        struct
        {
            WriteFileFNCallback write_file;
            void* context;
        } write_file;
    } fileio;

    bool repl_mode;// allows redefinition of symbols

    double max_execution_time_ms;
    bool max_execution_time_set;
};

struct Timer
{
#if defined(APE_POSIX)
    int64_t start_offset;
#elif defined(APE_WINDOWS)
    double pc_frequency;
#endif
    double start_time_ms;
};

struct Allocator
{
    AllocatorMallocFNCallback malloc;
    AllocatorFreeFNCallback free;
    void* ctx;
};

struct Error
{
    ErrorType type;
    char message[APE_ERROR_MESSAGE_MAX_LENGTH];
    Position pos;
    Traceback* traceback;
};

struct ErrorList
{
    Error errors[ERRORS_MAX_COUNT];
    int m_count;
};

struct Token
{
    TokenType type;
    const char* literal;
    int len;
    Position pos;
};

struct CompiledFile
{
    Allocator* alloc;
    char* dir_path;
    char* path;
    PtrArray * lines;
};

struct Lexer
{
    Allocator* alloc;
    ErrorList* errors;
    const char* input;
    int input_len;
    int position;
    int next_position;
    char ch;
    int line;
    int column;
    CompiledFile* file;
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
    Token prev_token;
    Token cur_token;
    Token peek_token;
};

struct Codeblock
{
    Allocator* alloc;
    PtrArray * statements;
};

struct MapLiteral
{
    PtrArray * keys;
    PtrArray * values;
};

struct PrefixExpr
{
    Operator op;
    Expression* right;
};

struct InfixExpr
{
    Operator op;
    Expression* left;
    Expression* right;
};

struct IfCase
{
    Allocator* alloc;
    Expression* test;
    Codeblock* consequence;
};

struct FnLiteral
{
    char* name;
    PtrArray * params;
    Codeblock* body;
};

struct CallExpr
{
    Expression* function;
    PtrArray * args;
};

struct IndexExpr
{
    Expression* left;
    Expression* index;
};

struct AssignExpr
{
    Expression* dest;
    Expression* source;
    bool is_postfix;
};

struct LogicalExpr
{
    Operator op;
    Expression* left;
    Expression* right;
};

struct TernaryExpr
{
    Expression* test;
    Expression* if_true;
    Expression* if_false;
};

struct Ident
{
    Allocator* alloc;
    char* value;
    Position pos;
};

struct DefineStmt
{
    Ident* name;
    Expression* value;
    bool assignable;
};

struct IfStmt
{
    PtrArray * cases;
    Codeblock* alternative;
};

struct WhileLoopStmt
{
    Expression* test;
    Codeblock* body;
};

struct ForeachStmt
{
    Ident* iterator;
    Expression* source;
    Codeblock* body;
};

struct ForLoopStmt
{
    Expression* init;
    Expression* test;
    Expression* update;
    Codeblock* body;
};

struct ImportStmt
{
    char* path;
};

struct RecoverStmt
{
    Ident* error_ident;
    Codeblock* body;
};

struct Expression
{
    Allocator* alloc;
    Expr_type type;
    union
    {
        Ident* ident;
        double number_literal;
        bool bool_literal;
        char* string_literal;
        PtrArray * array;
        MapLiteral map;
        PrefixExpr prefix;
        InfixExpr infix;
        FnLiteral fn_literal;
        CallExpr call_expr;
        IndexExpr index_expr;
        AssignExpr assign;
        LogicalExpr logical;
        TernaryExpr ternary;
        DefineStmt define;
        IfStmt if_statement;
        Expression* return_value;
        Expression* expression;
        WhileLoopStmt while_loop;
        ForeachStmt foreach;
        ForLoopStmt for_loop;
        Codeblock* block;
        ImportStmt import;
        RecoverStmt recover;
    };
    Position pos;
};

struct Parser
{
    Allocator* alloc;
    const Config* config;
    Lexer lexer;
    ErrorList* errors;

    RightAssocParseFNCallback right_assoc_parse_fns[TOKEN_TYPE_MAX];
    LeftAssocParseFNCallback left_assoc_parse_fns[TOKEN_TYPE_MAX];

    int depth;
};

struct Object
{
    uint64_t _internal;
    union
    {
        uint64_t handle;
        double number;
    };
};

struct FunctionObject
{
    union
    {
        Object* free_vals_allocated;
        Object free_vals_buf[2];
    };
    union
    {
        char* name;
        const char* const_name;
    };
    CompilationResult* comp_result;
    int num_locals;
    int num_args;
    int free_vals_count;
    bool owns_data;
};


struct ExternalData
{
    void* data;
    ExternalDataDestroyFNCallback data_destroy_fn;
    ExternalDataCopyFNCallback data_copy_fn;
};

struct ObjectError
{
    char* message;
    Traceback* traceback;
};

struct ObjectString
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

struct NativeFunction
{
    char* name;
    NativeFNCallback native_funcptr;
    //uint8_t data[NATIVE_FN_MAX_DATA_LEN];
    void* data;
    int data_len;
};

struct NativeFuncWrapper
{
    UserFNCallback wrapped_funcptr;
    Context* ape;
    void* data;
};       

struct ObjectData
{
    GCMemory* mem;
    union
    {
        ObjectString string;
        ObjectError error;
        Array* array;
        ValDictionary * map;
        FunctionObject function;
        NativeFunction native_function;
        ExternalData external;
    };
    bool gcmark;
    ObjectType type;
};

struct Symbol
{
    Allocator* alloc;
    SymbolType type;
    char* name;
    int index;
    bool assignable;
};

struct BlockScope
{
    Allocator* alloc;
    Dictionary * store;
    int offset;
    int num_definitions;
};

struct SymbolTable
{
    Allocator* alloc;
    SymbolTable* outer;
    GlobalStore* global_store;
    PtrArray * block_scopes;
    PtrArray * free_symbols;
    PtrArray * module_global_symbols;
    int max_num_definitions;
    int module_global_offset;
};

struct OpcodeDefinition
{
    const char* name;
    int num_operands;
    int operand_widths[2];
};

struct CompilationResult
{
    Allocator* alloc;
    uint8_t* bytecode;
    Position* src_positions;
    int m_count;
};

struct CompilationScope
{
    Allocator* alloc;
    CompilationScope* outer;
    Array * bytecode;
    Array * src_positions;
    Array * break_ip_stack;
    Array * continue_ip_stack;
    opcode_t last_opcode;
};

struct ObjectDataPool
{
    ObjectData* datapool[GCMEM_POOL_SIZE];
    int m_count;
};

struct GCMemory
{
    Allocator* alloc;
    int allocations_since_sweep;
    PtrArray * objects;
    PtrArray * objects_back;
    Array * objects_not_gced;
    ObjectDataPool data_only_pool;
    ObjectDataPool pools[GCMEM_POOLS_NUM];
};

struct TracebackItem
{
    char* function_name;
    Position pos;
};

struct Traceback
{
    Allocator* alloc;
    Array * items;
};

struct Frame
{
    Object function;
    int ip;
    int base_pointer;
    const Position* src_positions;
    uint8_t* bytecode;
    int src_ip;
    int bytecode_size;
    int recover_ip;
    bool is_recovering;
};

struct VM
{
    Allocator* alloc;
    const Config* config;
    GCMemory* mem;
    ErrorList* errors;
    GlobalStore* global_store;
    Object globals[VM_MAX_GLOBALS];
    int globals_count;
    Object stack[VM_STACK_SIZE];
    int sp;
    Object this_stack[VM_THIS_STACK_SIZE];
    int this_sp;
    Frame frames[VM_MAX_FRAMES];
    int frames_count;
    Object last_popped;
    Frame* current_frame;
    bool running;
    Object operator_oveload_keys[OPCODE_MAX];
};

struct ValDictionary
{
    public:
        template<typename KeyType, typename ValueType>
        static ValDictionary* make(Allocator* alloc)
        {
            return ValDictionary::makeWithCapacity(alloc, DICT_INITIAL_SIZE, sizeof(KeyType), sizeof(ValueType));
        }

        static ValDictionary* makeWithCapacity(Allocator* alloc, unsigned int min_capacity, size_t key_size, size_t val_size)
        {
            bool ok;
            ValDictionary* dict;
            unsigned int capacity;
            dict = (ValDictionary*)allocator_malloc(alloc, sizeof(ValDictionary));
            capacity = upper_power_of_two(min_capacity * 2);
            if(!dict)
            {
                return NULL;
            }
            ok = dict->init(alloc, key_size, val_size, capacity);
            if(!ok)
            {
                allocator_free(alloc, dict);
                return NULL;
            }
            return dict;
        }

    public:
        Allocator* alloc;
        size_t key_size;
        size_t val_size;
        unsigned int* cells;
        unsigned long* hashes;
        void* keys;
        void* values;
        unsigned int* cell_ixs;
        unsigned int m_count;
        unsigned int item_capacity;
        unsigned int cell_capacity;
        CollectionsHashFNCallback _hash_key;
        CollectionsEqualsFNCallback _keys_equals;

    public:
        void destroy()
        {
            if(!this)
            {
                return;
            }
            Allocator* alloc = this->alloc;
            this->deinit();
            allocator_free(alloc, this);
        }

        void setHashFunction(CollectionsHashFNCallback hash_fn)
        {
            this->_hash_key = hash_fn;
        }

        void setEqualsFunction(CollectionsEqualsFNCallback equals_fn)
        {
            this->_keys_equals = equals_fn;
        }

        bool set(void* key, void* value)
        {
            unsigned long hash = this->hashKey(key);
            bool found = false;
            unsigned int cell_ix = this->getCellIndex(key, hash, &found);
            if(found)
            {
                unsigned int item_ix = this->cells[cell_ix];
                this->setValueAt(item_ix, value);
                return true;
            }
            if(this->m_count >= this->item_capacity)
            {
                bool ok = this->growAndRehash();
                if(!ok)
                {
                    return false;
                }
                cell_ix = this->getCellIndex(key, hash, &found);
            }
            unsigned int last_ix = this->m_count;
            this->m_count++;
            this->cells[cell_ix] = last_ix;
            this->setKeyAt(last_ix, key);
            this->setValueAt(last_ix, value);
            this->cell_ixs[last_ix] = cell_ix;
            this->hashes[last_ix] = hash;
            return true;
        }

        void* get(const void* key) const
        {
            unsigned long hash = this->hashKey(key);
            bool found = false;
            unsigned long cell_ix = this->getCellIndex(key, hash, &found);
            if(!found)
            {
                return NULL;
            }
            unsigned int item_ix = this->cells[cell_ix];
            return this->getValueAt(item_ix);
        }

        void* getKeyAt(unsigned int ix) const
        {
            if(ix >= this->m_count)
            {
                return NULL;
            }
            return (char*)this->keys + (this->key_size * ix);
        }

        void* getValueAt(unsigned int ix) const
        {
            if(ix >= this->m_count)
            {
                return NULL;
            }
            return (char*)this->values + (this->val_size * ix);
        }

        unsigned int getCapacity()
        {
            return this->item_capacity;
        }

        bool setValueAt(unsigned int ix, const void* value)
        {
            if(ix >= this->m_count)
            {
                return false;
            }
            size_t offset = ix * this->val_size;
            memcpy((char*)this->values + offset, value, this->val_size);
            return true;
        }

        int count() const
        {
            if(!this)
            {
                return 0;
            }
            return this->m_count;
        }

        bool remove(void* key)
        {
            unsigned long hash = this->hashKey(key);
            bool found = false;
            unsigned int cell = this->getCellIndex( key, hash, &found);
            if(!found)
            {
                return false;
            }

            unsigned int item_ix = this->cells[cell];
            unsigned int last_item_ix = this->m_count - 1;
            if(item_ix < last_item_ix)
            {
                void* last_key = this->getKeyAt(last_item_ix);
                this->setKeyAt( item_ix, last_key);
                void* last_value = this->getKeyAt(last_item_ix);
                this->setValueAt(item_ix, last_value);
                this->cell_ixs[item_ix] = this->cell_ixs[last_item_ix];
                this->hashes[item_ix] = this->hashes[last_item_ix];
                this->cells[this->cell_ixs[item_ix]] = item_ix;
            }
            this->m_count--;

            unsigned int i = cell;
            unsigned int j = i;
            for(unsigned int x = 0; x < (this->cell_capacity - 1); x++)
            {
                j = (j + 1) & (this->cell_capacity - 1);
                if(this->cells[j] == VALDICT_INVALID_IX)
                {
                    break;
                }
                unsigned int k = this->hashes[this->cells[j]] & (this->cell_capacity - 1);
                if((j > i && (k <= i || k > j)) || (j < i && (k <= i && k > j)))
                {
                    this->cell_ixs[this->cells[j]] = i;
                    this->cells[i] = this->cells[j];
                    i = j;
                }
            }
            this->cells[i] = VALDICT_INVALID_IX;
            return true;
        }

        void clear()
        {
            this->m_count = 0;
            for(unsigned int i = 0; i < this->cell_capacity; i++)
            {
                this->cells[i] = VALDICT_INVALID_IX;
            }
        }

        // Private definitions
        bool init(Allocator* alloc, size_t key_size, size_t val_size, unsigned int initial_capacity)
        {
            this->alloc = alloc;
            this->key_size = key_size;
            this->val_size = val_size;
            this->cells = NULL;
            this->keys = NULL;
            this->values = NULL;
            this->cell_ixs = NULL;
            this->hashes = NULL;
            this->m_count = 0;
            this->cell_capacity = initial_capacity;
            this->item_capacity = (unsigned int)(initial_capacity * 0.7f);
            this->_keys_equals = NULL;
            this->_hash_key = NULL;
            this->cells = (unsigned int*)allocator_malloc(this->alloc, this->cell_capacity * sizeof(*this->cells));
            this->keys = allocator_malloc(this->alloc, this->item_capacity * key_size);
            this->values = allocator_malloc(this->alloc, this->item_capacity * val_size);
            this->cell_ixs = (unsigned int*)allocator_malloc(this->alloc, this->item_capacity * sizeof(*this->cell_ixs));
            this->hashes = (long unsigned int*)allocator_malloc(this->alloc, this->item_capacity * sizeof(*this->hashes));
            if(this->cells == NULL || this->keys == NULL || this->values == NULL || this->cell_ixs == NULL || this->hashes == NULL)
            {
                goto error;
            }
            for(unsigned int i = 0; i < this->cell_capacity; i++)
            {
                this->cells[i] = VALDICT_INVALID_IX;
            }
            return true;
        error:
            allocator_free(this->alloc, this->cells);
            allocator_free(this->alloc, this->keys);
            allocator_free(this->alloc, this->values);
            allocator_free(this->alloc, this->cell_ixs);
            allocator_free(this->alloc, this->hashes);
            return false;
        }

        void deinit()
        {
            this->key_size = 0;
            this->val_size = 0;
            this->m_count = 0;
            this->item_capacity = 0;
            this->cell_capacity = 0;

            allocator_free(this->alloc, this->cells);
            allocator_free(this->alloc, this->keys);
            allocator_free(this->alloc, this->values);
            allocator_free(this->alloc, this->cell_ixs);
            allocator_free(this->alloc, this->hashes);

            this->cells = NULL;
            this->keys = NULL;
            this->values = NULL;
            this->cell_ixs = NULL;
            this->hashes = NULL;
        }

        unsigned int getCellIndex(const void* key, unsigned long hash, bool* out_found) const
        {
            *out_found = false;
            bool are_equal;
            unsigned int ofs;
            unsigned int i;
            unsigned int ix;
            unsigned int cell;
            unsigned int cell_ix;
            unsigned long hash_to_check;
            void* key_to_check;
            //fprintf(stderr, "valdict_get_cell_ix: this=%p, this->cell_capacity=%d\n", this, this->cell_capacity);
            ofs = 0;
            if(this->cell_capacity > 1)
            {
                ofs = (this->cell_capacity - 1);
            }
            cell_ix = hash & ofs;
            for(i = 0; i < this->cell_capacity; i++)
            {
                cell = VALDICT_INVALID_IX;
                ix = (cell_ix + i) & ofs;
                //fprintf(stderr, "(cell_ix=%d + i=%d) & ofs=%d == %d\n", cell_ix, i, ofs, ix);
                cell = this->cells[ix];
                if(cell == VALDICT_INVALID_IX)
                {
                    return ix;
                }
                hash_to_check = this->hashes[cell];
                if(hash != hash_to_check)
                {
                    continue;
                }
                key_to_check = this->getKeyAt(cell);
                are_equal = this->keysAreEqual(key, key_to_check);
                if(are_equal)
                {
                    *out_found = true;
                    return ix;
                }
            }
            return VALDICT_INVALID_IX;
        }

        bool growAndRehash()
        {
            ValDictionary new_dict;
            unsigned new_capacity = this->cell_capacity == 0 ? DICT_INITIAL_SIZE : this->cell_capacity * 2;
            bool ok = new_dict.init(this->alloc, this->key_size, this->val_size, new_capacity);
            if(!ok)
            {
                return false;
            }
            new_dict._keys_equals = this->_keys_equals;
            new_dict._hash_key = this->_hash_key;
            for(unsigned int i = 0; i < this->m_count; i++)
            {
                char* key = (char*)this->getKeyAt( i);
                void* value = this->getValueAt(i);
                ok = new_dict.set(key, value);
                if(!ok)
                {
                    new_dict.deinit();
                    return false;
                }
            }
            this->deinit();
            *this = new_dict;
            return true;
        }

        bool setKeyAt(unsigned int ix, void* key)
        {
            if(ix >= this->m_count)
            {
                return false;
            }
            size_t offset = ix * this->key_size;
            memcpy((char*)this->keys + offset, key, this->key_size);
            return true;
        }

        bool keysAreEqual(const void* a, const void* b) const
        {
            if(this->_keys_equals)
            {
                return this->_keys_equals(a, b);
            }
            else
            {
                return memcmp(a, b, this->key_size) == 0;
            }
        }

        unsigned long hashKey(const void* key) const
        {
            if(this->_hash_key)
            {
                return this->_hash_key(key);
            }
            else
            {
                return collections_hash(key, this->key_size);
            }
        }

};

struct Dictionary
{
    public:
        template<typename TypeFNCopy, typename TypeFNDestroy>
        static Dictionary* make(Allocator* alloc, TypeFNCopy copy_fn, TypeFNDestroy destroy_fn)
        {
            Dictionary* dict = (Dictionary*)allocator_malloc(alloc, sizeof(Dictionary));
            if(dict == NULL)
            {
                return NULL;
            }
            bool ok = dict->init(alloc, DICT_INITIAL_SIZE, (DictItemCopyFNCallback)copy_fn, (DictItemDestroyFNCallback)destroy_fn);
            if(!ok)
            {
                allocator_free(alloc, dict);
                return NULL;
            }
            return dict;
        }

        static unsigned long hash_string(const char* str)
        { /* djb2 */
            unsigned long hash = 5381;
            uint8_t c;
            while((c = *str++))
            {
                hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
            }
            return hash;
        }

    public:
        Allocator* alloc;
        unsigned int* cells;
        unsigned long* hashes;
        char** keys;
        void** values;
        unsigned int* cell_ixs;
        unsigned int m_count;
        unsigned int item_capacity;
        unsigned int cell_capacity;
        DictItemCopyFNCallback copy_fn;
        DictItemDestroyFNCallback destroy_fn;

    public:
        void destroy()
        {
            if(!this)
            {
                return;
            }
            Allocator* alloc = this->alloc;
            this->deinit(true);
            allocator_free(alloc, this);
        }

        void destroyWithItems()
        {
            unsigned int i;
            if(!this)
            {
                return;
            }
            if(this->destroy_fn)
            {
                for(i = 0; i < this->m_count; i++)
                {
                    this->destroy_fn(this->values[i]);
                }
            }
            this->destroy();
        }

        Dictionary* copyWithItems()
        {
            int i;
            bool ok;
            const char* key;
            void* item;
            void* item_copy;
            Dictionary* dict_copy;
            ok = false;
            if(!this->copy_fn || !this->destroy_fn)
            {
                return NULL;
            }

            dict_copy = Dictionary::make(this->alloc, this->copy_fn, this->destroy_fn);
            if(!dict_copy)
            {
                return NULL;
            }
            for(i = 0; i < this->count(); i++)
            {
                key = this->getKeyAt(i);
                item = this->getValueAt( i);
                item_copy = dict_copy->copy_fn(item);
                if(item && !item_copy)
                {
                    dict_copy->destroyWithItems();
                    return NULL;
                }
                ok = dict_copy->set(key, item_copy);
                if(!ok)
                {
                    dict_copy->destroy_fn(item_copy);
                    dict_copy->destroyWithItems();
                    return NULL;
                }
            }
            return dict_copy;
        }

        bool set(const char* key, void* value)
        {
            return this->setInternal(key, NULL, value);
        }

        void* get(const char* key)
        {
            unsigned long hash = hash_string(key);
            bool found = false;
            unsigned long cell_ix = this->getCellIndex(key, hash, &found);
            if(found == false)
            {
                return NULL;
            }
            unsigned int item_ix = this->cells[cell_ix];
            return this->values[item_ix];
        }

        void* getValueAt(unsigned int ix)
        {
            if(ix >= this->m_count)
            {
                return NULL;
            }
            return this->values[ix];
        }

        const char* getKeyAt(unsigned int ix)
        {
            if(ix >= this->m_count)
            {
                return NULL;
            }
            return this->keys[ix];
        }

        int count()
        {
            if(!this)
            {
                return 0;
            }
            return this->m_count;
        }


        // Private definitions
        bool init(Allocator* alloc, unsigned int initial_capacity, DictItemCopyFNCallback copy_fn, DictItemDestroyFNCallback destroy_fn)
        {
            this->alloc = alloc;
            this->cells = NULL;
            this->keys = NULL;
            this->values = NULL;
            this->cell_ixs = NULL;
            this->hashes = NULL;
            this->m_count = 0;
            this->cell_capacity = initial_capacity;
            this->item_capacity = (unsigned int)(initial_capacity * 0.7f);
            this->copy_fn = copy_fn;
            this->destroy_fn = destroy_fn;
            this->cells = (unsigned int*)allocator_malloc(alloc, this->cell_capacity * sizeof(*this->cells));
            this->keys = (char**)allocator_malloc(alloc, this->item_capacity * sizeof(*this->keys));
            this->values = (void**)allocator_malloc(alloc, this->item_capacity * sizeof(*this->values));
            this->cell_ixs = (unsigned int*)allocator_malloc(alloc, this->item_capacity * sizeof(*this->cell_ixs));
            this->hashes = (long unsigned int*)allocator_malloc(alloc, this->item_capacity * sizeof(*this->hashes));
            if(this->cells == NULL || this->keys == NULL || this->values == NULL || this->cell_ixs == NULL || this->hashes == NULL)
            {
                goto error;
            }
            for(unsigned int i = 0; i < this->cell_capacity; i++)
            {
                this->cells[i] = DICT_INVALID_IX;
            }
            return true;
        error:
            allocator_free(this->alloc, this->cells);
            allocator_free(this->alloc, this->keys);
            allocator_free(this->alloc, this->values);
            allocator_free(this->alloc, this->cell_ixs);
            allocator_free(this->alloc, this->hashes);
            return false;
        }

        void deinit(bool free_keys)
        {
            if(free_keys)
            {
                for(unsigned int i = 0; i < this->m_count; i++)
                {
                    allocator_free(this->alloc, this->keys[i]);
                }
            }
            this->m_count = 0;
            this->item_capacity = 0;
            this->cell_capacity = 0;
            allocator_free(this->alloc, this->cells);
            allocator_free(this->alloc, this->keys);
            allocator_free(this->alloc, this->values);
            allocator_free(this->alloc, this->cell_ixs);
            allocator_free(this->alloc, this->hashes);
            this->cells = NULL;
            this->keys = NULL;
            this->values = NULL;
            this->cell_ixs = NULL;
            this->hashes = NULL;
        }

        unsigned int getCellIndex(const char* key, unsigned long hash, bool* out_found)
        {
            *out_found = false;
            unsigned int cell_ix = hash & (this->cell_capacity - 1);
            for(unsigned int i = 0; i < this->cell_capacity; i++)
            {
                unsigned int ix = (cell_ix + i) & (this->cell_capacity - 1);
                unsigned int cell = this->cells[ix];
                if(cell == DICT_INVALID_IX)
                {
                    return ix;
                }
                unsigned long hash_to_check = this->hashes[cell];
                if(hash != hash_to_check)
                {
                    continue;
                }
                const char* key_to_check = this->keys[cell];
                if(strcmp(key, key_to_check) == 0)
                {
                    *out_found = true;
                    return ix;
                }
            }
            return DICT_INVALID_IX;
        }

        bool growAndRehash()
        {
            Dictionary new_dict;
            bool ok = new_dict.init(this->alloc, this->cell_capacity * 2, this->copy_fn, this->destroy_fn);
            if(!ok)
            {
                return false;
            }
            for(unsigned int i = 0; i < this->m_count; i++)
            {
                char* key = this->keys[i];
                void* value = this->values[i];
                ok = new_dict.setInternal(key, key, value);
                if(!ok)
                {
                    new_dict.deinit(false);
                    return false;
                }
            }
            this->deinit(false);
            *this = new_dict;
            return true;
        }

        bool setInternal(const char* ckey, char* mkey, void* value)
        {
            unsigned long hash = hash_string(ckey);
            bool found = false;
            unsigned int cell_ix = this->getCellIndex(ckey, hash, &found);
            if(found)
            {
                unsigned int item_ix = this->cells[cell_ix];
                this->values[item_ix] = value;
                return true;
            }
            if(this->m_count >= this->item_capacity)
            {
                bool ok = this->growAndRehash();
                if(!ok)
                {
                    return false;
                }
                cell_ix = this->getCellIndex(ckey, hash, &found);
            }

            if(mkey)
            {
                this->keys[this->m_count] = mkey;
            }
            else
            {
                char* key_copy = collections_strdup(this->alloc, ckey);
                if(!key_copy)
                {
                    return false;
                }
                this->keys[this->m_count] = key_copy;
            }
            this->cells[cell_ix] = this->m_count;
            this->values[this->m_count] = value;
            this->cell_ixs[this->m_count] = cell_ix;
            this->hashes[this->m_count] = hash;
            this->m_count++;
            return true;
        }

};

struct Array
{
    public:
        template<typename Type>
        static Array* make(Allocator* alloc)
        {
            return Array::make(alloc, 32, sizeof(Type));
        }

        static Array* make(Allocator* alloc, unsigned int capacity, size_t element_size)
        {
            Array* arr = (Array*)allocator_malloc(alloc, sizeof(Array));
            if(!arr)
            {
                return NULL;
            }

            bool ok = arr->init(alloc, capacity, element_size);
            if(!ok)
            {
                allocator_free(alloc, arr);
                return NULL;
            }
            return arr;
        }

        static void destroy(Array* arr)
        {
            if(!arr)
            {
                return;
            }
            Allocator* alloc = arr->alloc;
            arr->deinit();
            allocator_free(alloc, arr);
        }

        static void destroy(Array* arr, ArrayItemDeinitFNCallback deinit_fn)
        {
            int i;
            void* item;
            for(i = 0; i < arr->count(); i++)
            {
                item = (void*)arr->get(i);
                deinit_fn(item);
            }
            Array::destroy(arr);
        }

        static Array* copy(const Array* arr)
        {
            Array* copy = (Array*)allocator_malloc(arr->alloc, sizeof(Array));
            if(!copy)
            {
                return NULL;
            }
            copy->alloc = arr->alloc;
            copy->capacity = arr->capacity;
            copy->m_count = arr->m_count;
            copy->element_size = arr->element_size;
            copy->lock_capacity = arr->lock_capacity;
            if(arr->data_allocated)
            {
                copy->data_allocated = (unsigned char*)allocator_malloc(arr->alloc, arr->capacity * arr->element_size);
                if(!copy->data_allocated)
                {
                    allocator_free(arr->alloc, copy);
                    return NULL;
                }
                copy->arraydata = copy->data_allocated;
                memcpy(copy->data_allocated, arr->arraydata, arr->capacity * arr->element_size);
            }
            else
            {
                copy->data_allocated = NULL;
                copy->arraydata = NULL;
            }

            return copy;
        }


    public:
        Allocator* alloc;
        unsigned char* arraydata;
        unsigned char* data_allocated;
        unsigned int m_count;
        unsigned int capacity;
        size_t element_size;
        bool lock_capacity;

    public:
        bool add(const void* value)
        {
            if(this->m_count >= this->capacity)
            {
                COLLECTIONS_ASSERT(!this->lock_capacity);
                if(this->lock_capacity)
                {
                    return false;
                }
                unsigned int new_capacity = this->capacity > 0 ? this->capacity * 2 : 1;
                unsigned char* new_data = (unsigned char*)allocator_malloc(this->alloc, new_capacity * this->element_size);
                if(!new_data)
                {
                    return false;
                }
                memcpy(new_data, this->arraydata, this->m_count * this->element_size);
                allocator_free(this->alloc, this->data_allocated);
                this->data_allocated = new_data;
                this->arraydata = this->data_allocated;
                this->capacity = new_capacity;
            }
            if(value)
            {
                memcpy(this->arraydata + (this->m_count * this->element_size), value, this->element_size);
            }
            this->m_count++;
            return true;
        }

        bool addn(const void* values, int n)
        {
            for(int i = 0; i < n; i++)
            {
                const uint8_t* value = NULL;
                if(values)
                {
                    value = (const uint8_t*)values + (i * this->element_size);
                }
                bool ok = this->add(value);
                if(!ok)
                {
                    return false;
                }
            }
            return true;
        }

        bool addArray(Array* source)
        {
            int i;
            int dest_before_count;
            bool ok;
            void* item;
            COLLECTIONS_ASSERT(this->element_size == source->element_size);
            if(this->element_size != source->element_size)
            {
                return false;
            }
            dest_before_count = this->count();
            for(i = 0; i < source->count(); i++)
            {
                item = (void*)source->get(i);
                ok = this->add(item);
                if(!ok)
                {
                    this->m_count = dest_before_count;
                    return false;
                }
            }
            return true;
        }

        bool push(const void* value)
        {
            return this->add(value);
        }

        bool pop(void* out_value)
        {
            if(this->m_count <= 0)
            {
                return false;
            }
            if(out_value)
            {
                void* res = (void*)this->get(this->m_count - 1);
                memcpy(out_value, res, this->element_size);
            }
            this->removeAt(this->m_count - 1);
            return true;
        }

        void* top()
        {
            if(this->m_count <= 0)
            {
                return NULL;
            }
            return (void*)this->get(this->m_count - 1);
        }

        bool set(unsigned int ix, void* value)
        {
            if(ix >= this->m_count)
            {
                COLLECTIONS_ASSERT(false);
                return false;
            }
            size_t offset = ix * this->element_size;
            memmove(this->arraydata + offset, value, this->element_size);
            return true;
        }

        bool setn(unsigned int ix, void* values, int n)
        {
            for(int i = 0; i < n; i++)
            {
                int dest_ix = ix + i;
                unsigned char* value = (unsigned char*)values + (i * this->element_size);
                if(dest_ix < this->count())
                {
                    bool ok = this->set(dest_ix, value);
                    if(!ok)
                    {
                        return false;
                    }
                }
                else
                {
                    bool ok = this->add(value);
                    if(!ok)
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        void* get(unsigned int ix) const
        {
            if(ix >= this->m_count)
            {
                COLLECTIONS_ASSERT(false);
                return NULL;
            }
            size_t offset = ix * this->element_size;
            return this->arraydata + offset;
        }

        const void* getConst(unsigned int ix) const
        {
            if(ix >= this->m_count)
            {
                COLLECTIONS_ASSERT(false);
                return NULL;
            }
            size_t offset = ix * this->element_size;
            return this->arraydata + offset;
        }

        void* getLast()
        {
            if(this->m_count <= 0)
            {
                return NULL;
            }
            return this->get(this->m_count - 1);
        }

        int count() const
        {
            if(!this)
            {
                return 0;
            }
            return this->m_count;
        }

        unsigned int getCapacity() const
        {
            return this->capacity;
        }

        bool removeAt(unsigned int ix)
        {
            if(ix >= this->m_count)
            {
                return false;
            }
            if(ix == 0)
            {
                this->arraydata += this->element_size;
                this->capacity--;
                this->m_count--;
                return true;
            }
            if(ix == (this->m_count - 1))
            {
                this->m_count--;
                return true;
            }
            size_t to_move_bytes = (this->m_count - 1 - ix) * this->element_size;
            void* dest = this->arraydata + (ix * this->element_size);
            void* src = this->arraydata + ((ix + 1) * this->element_size);
            memmove(dest, src, to_move_bytes);
            this->m_count--;
            return true;
        }

        bool removeItem(void* ptr)
        {
            int ix = this->getIndex(ptr);
            if(ix < 0)
            {
                return false;
            }
            return this->removeAt(ix);
        }

        void clear()
        {
            this->m_count = 0;
        }

        void clearAndDeinit(ArrayItemDeinitFNCallback deinit_fn)
        {
            for(int i = 0; i < this->count(); i++)
            {
                void* item = this->get(i);
                deinit_fn(item);
            }
            this->m_count = 0;
        }

        void lockCapacity()
        {
            this->lock_capacity = true;
        }

        int getIndex(void* ptr) const
        {
            for(int i = 0; i < this->count(); i++)
            {
                if(this->getConst(i) == ptr)
                {
                    return i;
                }
            }
            return -1;
        }

        bool contains(void* ptr) const
        {
            return this->getIndex(ptr) >= 0;
        }

        void* data()
        {
            return this->arraydata;
        }

        const void* constData() const
        {
            return this->arraydata;
        }

        void orphanData()
        {
            this->init(this->alloc, 0, this->element_size);
        }

        bool reverse()
        {
            int cn = this->count();
            if(cn < 2)
            {
                return true;
            }
            void* temp = (void*)allocator_malloc(this->alloc, this->element_size);
            if(!temp)
            {
                return false;
            }
            for(int a_ix = 0; a_ix < (cn / 2); a_ix++)
            {
                int b_ix = cn - a_ix - 1;
                void* a = this->get(a_ix);
                void* b = this->get(b_ix);
                memcpy(temp, a, this->element_size);
                this->set(a_ix, b);// no need for check because it will be within range
                this->set(b_ix, temp);
            }
            allocator_free(this->alloc, temp);
            return true;
        }

        bool init(Allocator* alloc, unsigned int capacity, size_t element_size)
        {
            this->alloc = alloc;
            if(capacity > 0)
            {
                this->data_allocated = (unsigned char*)allocator_malloc(this->alloc, capacity * element_size);
                this->arraydata = this->data_allocated;
                if(!this->data_allocated)
                {
                    return false;
                }
            }
            else
            {
                this->data_allocated = NULL;
                this->arraydata = NULL;
            }
            this->capacity = capacity;
            this->m_count = 0;
            this->element_size = element_size;
            this->lock_capacity = false;
            return true;
        }

        void deinit()
        {
            allocator_free(this->alloc, this->data_allocated);
        }

};

struct PtrArray
{
    public:
        static PtrArray* make(Allocator* alloc)
        {
            return PtrArray::make(alloc, 0);
        }

        static PtrArray* make(Allocator* alloc, unsigned int capacity)
        {
            PtrArray* ptrarr = (PtrArray*)allocator_malloc(alloc, sizeof(PtrArray));
            if(!ptrarr)
            {
                return NULL;
            }
            ptrarr->alloc = alloc;
            bool ok = ptrarr->arr.init(alloc, capacity, sizeof(void*));
            if(!ok)
            {
                allocator_free(alloc, ptrarr);
                return NULL;
            }
            return ptrarr;
        }


    public:
        Allocator* alloc;
        Array arr;

    public:
        void destroy()
        {
            if(!this)
            {
                return;
            }
            this->arr.deinit();
            allocator_free(this->alloc, this);
        }

        template<typename FnType>
        void destroyWithItems(FnType destroy_fn)
        {// todo: destroy and copy in make fn
            if(this == NULL)
            {
                return;
            }
            if(destroy_fn)
            {
                this->clearAndDestroyItems(destroy_fn);
            }
            this->destroy();
        }

        PtrArray* copy()
        {
            PtrArray* arr_copy = PtrArray::make(this->alloc, this->arr.capacity);
            if(!arr_copy)
            {
                return NULL;
            }
            for(int i = 0; i < this->count(); i++)
            {
                void* item = this->get(i);
                bool ok = arr_copy->add(item);
                if(!ok)
                {
                    arr_copy->destroy();
                    return NULL;
                }
            }
            return arr_copy;
        }

        template<typename FnCopyType, typename FnDestroyType>
        PtrArray* copyWithItems(FnCopyType copy_fn, FnDestroyType destroy_fn)
        {
            PtrArray* arr_copy = PtrArray::make(this->alloc, this->arr.capacity);
            if(!arr_copy)
            {
                return NULL;
            }
            for(int i = 0; i < this->count(); i++)
            {
                void* item = this->get(i);
                void* item_copy = ((PtrArrayItemCopyFNCallback)copy_fn)(item);
                if(item && !item_copy)
                {
                    goto err;
                }
                bool ok = arr_copy->add(item_copy);
                if(!ok)
                {
                    goto err;
                }
            }
            return arr_copy;
        err:
            arr_copy->destroyWithItems(destroy_fn);
            return NULL;
        }

        bool add(void* ptr)
        {
            return this->arr.add(&ptr);
        }

        bool set(unsigned int ix, void* ptr)
        {
            return this->arr.set(ix, &ptr);
        }

        bool addArray(PtrArray* source)
        {
            return this->arr.addArray(&source->arr);
        }

        void* get(unsigned int ix) const
        {
            void* res = this->arr.get(ix);
            if(!res)
            {
                return NULL;
            }
            return *(void**)res;
        }

        const void* getConst(unsigned int ix) const
        {
            const void* res = this->arr.getConst(ix);
            if(!res)
            {
                return NULL;
            }
            return *(void* const*)res;
        }

        bool push(void* ptr)
        {
            return this->add( ptr);
        }

        void* pop()
        {
            int ix = this->count() - 1;
            void* res = this->get(ix);
            this->removeAt(ix);
            return res;
        }

        void* top() const
        {
            int cn = this->count();
            if(cn == 0)
            {
                return NULL;
            }
            return this->get(cn - 1);
        }

        int count() const
        {
            if(!this)
            {
                return 0;
            }
            return this->arr.count();
        }

        bool removeAt(unsigned int ix)
        {
            return this->arr.removeAt(ix);
        }

        bool removeItem(void* item)
        {
            for(int i = 0; i < this->count(); i++)
            {
                if(item == this->get(i))
                {
                    this->removeAt(i);
                    return true;
                }
            }
            COLLECTIONS_ASSERT(false);
            return false;
        }

        void clear()
        {
            this->arr.clear();
        }

        template<typename FnType>
        void clearAndDestroyItems(FnType destroy_fn)
        {
            for(int i = 0; i < this->count(); i++)
            {
                void* item = this->get(i);
                ((PtrArrayItemDestroyFNCallback)destroy_fn)(item);
            }
            this->clear();
        }

        void lockCapacity()
        {
            this->arr.lockCapacity();
        }

        int getIndex(void* ptr) const
        {
            for(int i = 0; i < this->count(); i++)
            {
                if(this->get(i) == ptr)
                {
                    return i;
                }
            }
            return -1;
        }

        bool contains(void* item) const
        {
            return this->getIndex(item) >= 0;
        }

        void* getAddr(unsigned int ix) const
        {
            void* res = this->arr.get(ix);
            if(res == NULL)
            {
                return NULL;
            }
            return res;
        }

        void* data()
        {
            return this->arr.data();
        }

        void reverse()
        {
            this->arr.reverse();
        }

};

struct StringBuffer
{
    Allocator* alloc;
    bool failed;
    char* stringdata;
    size_t capacity;
    size_t len;
};

struct GlobalStore
{
    Allocator* alloc;
    Dictionary * symbols;
    Array * objects;
};

struct Module
{
    Allocator* alloc;
    char* name;
    PtrArray * symbols;
};

struct FileScope
{
    Allocator* alloc;
    Parser* parser;
    SymbolTable* symbol_table;
    CompiledFile* file;
    PtrArray * loaded_module_names;
};

struct Compiler
{
    Allocator* alloc;
    const Config* config;
    GCMemory* mem;
    ErrorList* errors;
    PtrArray * files;
    GlobalStore* global_store;
    Array * constants;
    CompilationScope* compilation_scope;
    PtrArray * file_scopes;
    Array * src_positions_stack;
    Dictionary * modules;
    Dictionary * string_constants_positions;
};


struct Program
{
    Context* ape;
    CompilationResult* comp_res;
};

struct Context
{
    Allocator alloc;
    GCMemory* mem;
    PtrArray * files;
    GlobalStore* global_store;
    Compiler* compiler;
    VM* vm;
    ErrorList errors;
    Config config;
    Allocator custom_allocator;
};

static const Position src_pos_invalid = { NULL, -1, -1 };
static const Position src_pos_zero = { NULL, 0, 0 };


static Position src_pos_make(const CompiledFile* file, int line, int column)
{
    return (Position){
        .file = file,
        .line = line,
        .column = column,
    };
}


void token_init(Token* tok, TokenType type, const char* literal, int len)
{
    tok->type = type;
    tok->literal = literal;
    tok->len = len;
}

char* token_duplicate_literal(Allocator* alloc, const Token* tok)
{
    return ape_strndup(alloc, tok->literal, tok->len);
}

bool lexer_init(Lexer* lex, Allocator* alloc, ErrorList* errs, const char* input, CompiledFile* file)
{
    lex->alloc = alloc;
    lex->errors = errs;
    lex->input = input;
    lex->input_len = (int)strlen(input);
    lex->position = 0;
    lex->next_position = 0;
    lex->ch = '\0';
    if(file)
    {
        lex->line = file->lines->count();
    }
    else
    {
        lex->line = 0;
    }
    lex->column = -1;
    lex->file = file;
    bool ok = add_line(lex, 0);
    if(!ok)
    {
        return false;
    }
    ok = read_char(lex);
    if(!ok)
    {
        return false;
    }
    lex->failed = false;
    lex->continue_template_string = false;

    memset(&lex->prev_token_state, 0, sizeof(lex->prev_token_state));
    token_init(&lex->prev_token, TOKEN_INVALID, NULL, 0);
    token_init(&lex->cur_token, TOKEN_INVALID, NULL, 0);
    token_init(&lex->peek_token, TOKEN_INVALID, NULL, 0);

    return true;
}

bool lexer_failed(Lexer* lex)
{
    return lex->failed;
}

void lexer_continue_template_string(Lexer* lex)
{
    lex->continue_template_string = true;
}

bool lexer_cur_token_is(Lexer* lex, TokenType type)
{
    return lex->cur_token.type == type;
}

bool lexer_peek_token_is(Lexer* lex, TokenType type)
{
    return lex->peek_token.type == type;
}

bool lexer_next_token(Lexer* lex)
{
    lex->prev_token = lex->cur_token;
    lex->cur_token = lex->peek_token;
    lex->peek_token = lexer_next_token_internal(lex);
    return !lex->failed;
}

bool lexer_previous_token(Lexer* lex)
{
    if(lex->prev_token.type == TOKEN_INVALID)
    {
        return false;
    }

    lex->peek_token = lex->cur_token;
    lex->cur_token = lex->prev_token;
    token_init(&lex->prev_token, TOKEN_INVALID, NULL, 0);

    lex->ch = lex->prev_token_state.ch;
    lex->column = lex->prev_token_state.column;
    lex->line = lex->prev_token_state.line;
    lex->position = lex->prev_token_state.position;
    lex->next_position = lex->prev_token_state.next_position;

    return true;
}

Token lexer_next_token_internal(Lexer* lex)
{
    lex->prev_token_state.ch = lex->ch;
    lex->prev_token_state.column = lex->column;
    lex->prev_token_state.line = lex->line;
    lex->prev_token_state.position = lex->position;
    lex->prev_token_state.next_position = lex->next_position;

    while(true)
    {
        if(!lex->continue_template_string)
        {
            skip_whitespace(lex);
        }

        Token out_tok;
        out_tok.type = TOKEN_INVALID;
        out_tok.literal = lex->input + lex->position;
        out_tok.len = 1;
        out_tok.pos = src_pos_make(lex->file, lex->line, lex->column);

        char c = lex->continue_template_string ? '`' : lex->ch;

        switch(c)
        {
            case '\0':
                token_init(&out_tok, TOKEN_EOF, "EOF", 3);
                break;
            case '=':
            {
                if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_EQ, "==", 2);
                    read_char(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_ASSIGN, "=", 1);
                }
                break;
            }
            case '&':
            {
                if(peek_char(lex) == '&')
                {
                    token_init(&out_tok, TOKEN_AND, "&&", 2);
                    read_char(lex);
                }
                else if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_BIT_AND_ASSIGN, "&=", 2);
                    read_char(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_BIT_AND, "&", 1);
                }
                break;
            }
            case '|':
            {
                if(peek_char(lex) == '|')
                {
                    token_init(&out_tok, TOKEN_OR, "||", 2);
                    read_char(lex);
                }
                else if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_BIT_OR_ASSIGN, "|=", 2);
                    read_char(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_BIT_OR, "|", 1);
                }
                break;
            }
            case '^':
            {
                if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_BIT_XOR_ASSIGN, "^=", 2);
                    read_char(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_BIT_XOR, "^", 1);
                    break;
                }
                break;
            }
            case '+':
            {
                if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_PLUS_ASSIGN, "+=", 2);
                    read_char(lex);
                }
                else if(peek_char(lex) == '+')
                {
                    token_init(&out_tok, TOKEN_PLUS_PLUS, "++", 2);
                    read_char(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_PLUS, "+", 1);
                    break;
                }
                break;
            }
            case '-':
            {
                if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_MINUS_ASSIGN, "-=", 2);
                    read_char(lex);
                }
                else if(peek_char(lex) == '-')
                {
                    token_init(&out_tok, TOKEN_MINUS_MINUS, "--", 2);
                    read_char(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_MINUS, "-", 1);
                    break;
                }
                break;
            }
            case '!':
            {
                if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_NOT_EQ, "!=", 2);
                    read_char(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_BANG, "!", 1);
                }
                break;
            }
            case '*':
            {
                if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_ASTERISK_ASSIGN, "*=", 2);
                    read_char(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_ASTERISK, "*", 1);
                    break;
                }
                break;
            }
            case '/':
            {
                if(peek_char(lex) == '/')
                {
                    read_char(lex);
                    while(lex->ch != '\n' && lex->ch != '\0')
                    {
                        read_char(lex);
                    }
                    continue;
                }
                else if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_SLASH_ASSIGN, "/=", 2);
                    read_char(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_SLASH, "/", 1);
                    break;
                }
                break;
            }
            case '<':
            {
                if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_LTE, "<=", 2);
                    read_char(lex);
                }
                else if(peek_char(lex) == '<')
                {
                    read_char(lex);
                    if(peek_char(lex) == '=')
                    {
                        token_init(&out_tok, TOKEN_LSHIFT_ASSIGN, "<<=", 3);
                        read_char(lex);
                    }
                    else
                    {
                        token_init(&out_tok, TOKEN_LSHIFT, "<<", 2);
                    }
                }
                else
                {
                    token_init(&out_tok, TOKEN_LT, "<", 1);
                    break;
                }
                break;
            }
            case '>':
            {
                if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_GTE, ">=", 2);
                    read_char(lex);
                }
                else if(peek_char(lex) == '>')
                {
                    read_char(lex);
                    if(peek_char(lex) == '=')
                    {
                        token_init(&out_tok, TOKEN_RSHIFT_ASSIGN, ">>=", 3);
                        read_char(lex);
                    }
                    else
                    {
                        token_init(&out_tok, TOKEN_RSHIFT, ">>", 2);
                    }
                }
                else
                {
                    token_init(&out_tok, TOKEN_GT, ">", 1);
                }
                break;
            }
            case ',':
                token_init(&out_tok, TOKEN_COMMA, ",", 1);
                break;
            case ';':
                token_init(&out_tok, TOKEN_SEMICOLON, ";", 1);
                break;
            case ':':
                token_init(&out_tok, TOKEN_COLON, ":", 1);
                break;
            case '(':
                token_init(&out_tok, TOKEN_LPAREN, "(", 1);
                break;
            case ')':
                token_init(&out_tok, TOKEN_RPAREN, ")", 1);
                break;
            case '{':
                token_init(&out_tok, TOKEN_LBRACE, "{", 1);
                break;
            case '}':
                token_init(&out_tok, TOKEN_RBRACE, "}", 1);
                break;
            case '[':
                token_init(&out_tok, TOKEN_LBRACKET, "[", 1);
                break;
            case ']':
                token_init(&out_tok, TOKEN_RBRACKET, "]", 1);
                break;
            case '.':
                token_init(&out_tok, TOKEN_DOT, ".", 1);
                break;
            case '?':
                token_init(&out_tok, TOKEN_QUESTION, "?", 1);
                break;
            case '%':
            {
                if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_PERCENT_ASSIGN, "%=", 2);
                    read_char(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_PERCENT, "%", 1);
                    break;
                }
                break;
            }
            case '"':
            {
                read_char(lex);
                int len;
                const char* str = read_string(lex, '"', false, NULL, &len);
                if(str)
                {
                    token_init(&out_tok, TOKEN_STRING, str, len);
                }
                else
                {
                    token_init(&out_tok, TOKEN_INVALID, NULL, 0);
                }
                break;
            }
            case '\'':
            {
                read_char(lex);
                int len;
                const char* str = read_string(lex, '\'', false, NULL, &len);
                if(str)
                {
                    token_init(&out_tok, TOKEN_STRING, str, len);
                }
                else
                {
                    token_init(&out_tok, TOKEN_INVALID, NULL, 0);
                }
                break;
            }
            case '`':
            {
                if(!lex->continue_template_string)
                {
                    read_char(lex);
                }
                int len;
                bool template_found = false;
                const char* str = read_string(lex, '`', true, &template_found, &len);
                if(str)
                {
                    if(template_found)
                    {
                        token_init(&out_tok, TOKEN_TEMPLATE_STRING, str, len);
                    }
                    else
                    {
                        token_init(&out_tok, TOKEN_STRING, str, len);
                    }
                }
                else
                {
                    token_init(&out_tok, TOKEN_INVALID, NULL, 0);
                }
                break;
            }
            default:
            {
                if(is_letter(lex->ch))
                {
                    int ident_len = 0;
                    const char* ident = read_identifier(lex, &ident_len);
                    TokenType type = lookup_identifier(ident, ident_len);
                    token_init(&out_tok, type, ident, ident_len);
                    return out_tok;
                }
                else if(is_digit(lex->ch))
                {
                    int number_len = 0;
                    const char* number = read_number(lex, &number_len);
                    token_init(&out_tok, TOKEN_NUMBER, number, number_len);
                    return out_tok;
                }
                break;
            }
        }
        read_char(lex);
        if(lexer_failed(lex))
        {
            token_init(&out_tok, TOKEN_INVALID, NULL, 0);
        }
        lex->continue_template_string = false;
        return out_tok;
    }
}

bool lexer_expect_current(Lexer* lex, TokenType type)
{
    if(lexer_failed(lex))
    {
        return false;
    }

    if(!lexer_cur_token_is(lex, type))
    {
        const char* expected_type_str = token_type_to_string(type);
        const char* actual_type_str = token_type_to_string(lex->cur_token.type);
        errors_add_errorf(lex->errors, APE_ERROR_PARSING, lex->cur_token.pos,
                          "Expected current token to be \"%s\", got \"%s\" instead", expected_type_str, actual_type_str);
        return false;
    }
    return true;
}

// INTERNAL

static bool read_char(Lexer* lex)
{
    if(lex->next_position >= lex->input_len)
    {
        lex->ch = '\0';
    }
    else
    {
        lex->ch = lex->input[lex->next_position];
    }
    lex->position = lex->next_position;
    lex->next_position++;

    if(lex->ch == '\n')
    {
        lex->line++;
        lex->column = -1;
        bool ok = add_line(lex, lex->next_position);
        if(!ok)
        {
            lex->failed = true;
            return false;
        }
    }
    else
    {
        lex->column++;
    }
    return true;
}

static char peek_char(Lexer* lex)
{
    if(lex->next_position >= lex->input_len)
    {
        return '\0';
    }
    else
    {
        return lex->input[lex->next_position];
    }
}

static bool is_letter(char ch)
{
    return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z') || ch == '_';
}

static bool is_digit(char ch)
{
    return ch >= '0' && ch <= '9';
}

static bool is_one_of(char ch, const char* allowed, int allowed_len)
{
    for(int i = 0; i < allowed_len; i++)
    {
        if(ch == allowed[i])
        {
            return true;
        }
    }
    return false;
}

static const char* read_identifier(Lexer* lex, int* out_len)
{
    int position = lex->position;
    int len = 0;
    while(is_digit(lex->ch) || is_letter(lex->ch) || lex->ch == ':')
    {
        if(lex->ch == ':')
        {
            if(peek_char(lex) != ':')
            {
                goto end;
            }
            read_char(lex);
        }
        read_char(lex);
    }
end:
    len = lex->position - position;
    *out_len = len;
    return lex->input + position;
}

static const char* read_number(Lexer* lex, int* out_len)
{
    char allowed[] = ".xXaAbBcCdDeEfF";
    int position = lex->position;
    while(is_digit(lex->ch) || is_one_of(lex->ch, allowed, APE_ARRAY_LEN(allowed) - 1))
    {
        read_char(lex);
    }
    int len = lex->position - position;
    *out_len = len;
    return lex->input + position;
}

static const char* read_string(Lexer* lex, char delimiter, bool is_template, bool* out_template_found, int* out_len)
{
    *out_len = 0;

    bool escaped = false;
    int position = lex->position;

    while(true)
    {
        if(lex->ch == '\0')
        {
            return NULL;
        }
        if(lex->ch == delimiter && !escaped)
        {
            break;
        }
        if(is_template && !escaped && lex->ch == '$' && peek_char(lex) == '{')
        {
            *out_template_found = true;
            break;
        }
        escaped = false;
        if(lex->ch == '\\')
        {
            escaped = true;
        }
        read_char(lex);
    }
    int len = lex->position - position;
    *out_len = len;
    return lex->input + position;
}

static TokenType lookup_identifier(const char* ident, int len)
{
    static struct
    {
        const char* value;
        int len;
        TokenType type;
    } keywords[] = {
        { "fn", 2, TOKEN_FUNCTION },
        { "const", 5, TOKEN_CONST },
        { "var", 3, TOKEN_VAR },
        { "true", 4, TOKEN_TRUE },
        { "false", 5, TOKEN_FALSE },
        { "if", 2, TOKEN_IF },
        { "else", 4, TOKEN_ELSE },
        { "return", 6, TOKEN_RETURN },
        { "while", 5, TOKEN_WHILE },
        { "break", 5, TOKEN_BREAK },       { "for", 3, TOKEN_FOR },       { "in", 2, TOKEN_IN },
        { "continue", 8, TOKEN_CONTINUE }, { "null", 4, TOKEN_NULL },     { "import", 6, TOKEN_IMPORT },
        { "recover", 7, TOKEN_RECOVER },
    };

    for(int i = 0; i < APE_ARRAY_LEN(keywords); i++)
    {
        if(keywords[i].len == len && APE_STRNEQ(ident, keywords[i].value, len))
        {
            return keywords[i].type;
        }
    }

    return TOKEN_IDENT;
}

static void skip_whitespace(Lexer* lex)
{
    char ch = lex->ch;
    while(ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
    {
        read_char(lex);
        ch = lex->ch;
    }
}

static bool add_line(Lexer* lex, int offset)
{
    if(!lex->file)
    {
        return true;
    }

    if(lex->line < lex->file->lines->count())
    {
        return true;
    }

    const char* line_start = lex->input + offset;
    const char* new_line_ptr = strchr(line_start, '\n');
    char* line = NULL;
    if(!new_line_ptr)
    {
        line = ape_strdup(lex->alloc, line_start);
    }
    else
    {
        size_t line_len = new_line_ptr - line_start;
        line = ape_strndup(lex->alloc, line_start, line_len);
    }
    if(!line)
    {
        lex->failed = true;
        return false;
    }
    bool ok = lex->file->lines->add(line);
    if(!ok)
    {
        lex->failed = true;
        allocator_free(lex->alloc, line);
        return false;
    }
    return true;
}



static Precedence get_precedence(TokenType tk);
static Operator token_to_operator(TokenType tk);
static char escape_char(const char c);
static char *process_and_copy_string(Allocator *alloc, const char *input, size_t len);
static Expression *wrap_expression_in_function_call(Allocator *alloc, Expression *expr, const char *function_name);

static Ident* ident_make(Allocator* alloc, Token tok)
{
    Ident* res = (Ident*)allocator_malloc(alloc, sizeof(Ident));
    if(!res)
    {
        return NULL;
    }
    res->alloc = alloc;
    res->value = token_duplicate_literal(alloc, &tok);
    if(!res->value)
    {
        allocator_free(alloc, res);
        return NULL;
    }
    res->pos = tok.pos;
    return res;
}

static Ident* ident_copy(Ident* ident)
{
    Ident* res = (Ident*)allocator_malloc(ident->alloc, sizeof(Ident));
    if(!res)
    {
        return NULL;
    }
    res->alloc = ident->alloc;
    res->value = ape_strdup(ident->alloc, ident->value);
    if(!res->value)
    {
        allocator_free(ident->alloc, res);
        return NULL;
    }
    res->pos = ident->pos;
    return res;
}

Expression* expression_make_ident(Allocator* alloc, Ident* ident)
{
    Expression* res = expression_make(alloc, EXPRESSION_IDENT);
    if(!res)
    {
        return NULL;
    }
    res->ident = ident;
    return res;
}

Expression* expression_make_number_literal(Allocator* alloc, double val)
{
    Expression* res = expression_make(alloc, EXPRESSION_NUMBER_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->number_literal = val;
    return res;
}

Expression* expression_make_bool_literal(Allocator* alloc, bool val)
{
    Expression* res = expression_make(alloc, EXPRESSION_BOOL_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->bool_literal = val;
    return res;
}

Expression* expression_make_string_literal(Allocator* alloc, char* value)
{
    Expression* res = expression_make(alloc, EXPRESSION_STRING_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->string_literal = value;
    return res;
}

Expression* expression_make_null_literal(Allocator* alloc)
{
    Expression* res = expression_make(alloc, EXPRESSION_NULL_LITERAL);
    if(!res)
    {
        return NULL;
    }
    return res;
}

Expression* expression_make_array_literal(Allocator* alloc, PtrArray * values)
{
    Expression* res = expression_make(alloc, EXPRESSION_ARRAY_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->array = values;
    return res;
}

Expression* expression_make_map_literal(Allocator* alloc, PtrArray * keys, PtrArray * values)
{
    Expression* res = expression_make(alloc, EXPRESSION_MAP_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->map.keys = keys;
    res->map.values = values;
    return res;
}

Expression* expression_make_prefix(Allocator* alloc, Operator op, Expression* right)
{
    Expression* res = expression_make(alloc, EXPRESSION_PREFIX);
    if(!res)
    {
        return NULL;
    }
    res->prefix.op = op;
    res->prefix.right = right;
    return res;
}

Expression* expression_make_infix(Allocator* alloc, Operator op, Expression* left, Expression* right)
{
    Expression* res = expression_make(alloc, EXPRESSION_INFIX);
    if(!res)
    {
        return NULL;
    }
    res->infix.op = op;
    res->infix.left = left;
    res->infix.right = right;
    return res;
}

Expression* expression_make_fn_literal(Allocator* alloc, PtrArray * params, Codeblock* body)
{
    Expression* res = expression_make(alloc, EXPRESSION_FUNCTION_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->fn_literal.name = NULL;
    res->fn_literal.params = params;
    res->fn_literal.body = body;
    return res;
}

Expression* expression_make_call(Allocator* alloc, Expression* function, PtrArray * args)
{
    Expression* res = expression_make(alloc, EXPRESSION_CALL);
    if(!res)
    {
        return NULL;
    }
    res->call_expr.function = function;
    res->call_expr.args = args;
    return res;
}

Expression* expression_make_index(Allocator* alloc, Expression* left, Expression* index)
{
    Expression* res = expression_make(alloc, EXPRESSION_INDEX);
    if(!res)
    {
        return NULL;
    }
    res->index_expr.left = left;
    res->index_expr.index = index;
    return res;
}

Expression* expression_make_assign(Allocator* alloc, Expression* dest, Expression* source, bool is_postfix)
{
    Expression* res = expression_make(alloc, EXPRESSION_ASSIGN);
    if(!res)
    {
        return NULL;
    }
    res->assign.dest = dest;
    res->assign.source = source;
    res->assign.is_postfix = is_postfix;
    return res;
}

Expression* expression_make_logical(Allocator* alloc, Operator op, Expression* left, Expression* right)
{
    Expression* res = expression_make(alloc, EXPRESSION_LOGICAL);
    if(!res)
    {
        return NULL;
    }
    res->logical.op = op;
    res->logical.left = left;
    res->logical.right = right;
    return res;
}

Expression* expression_make_ternary(Allocator* alloc, Expression* test, Expression* if_true, Expression* if_false)
{
    Expression* res = expression_make(alloc, EXPRESSION_TERNARY);
    if(!res)
    {
        return NULL;
    }
    res->ternary.test = test;
    res->ternary.if_true = if_true;
    res->ternary.if_false = if_false;
    return res;
}

void expression_destroy(Expression* expr)
{
    if(!expr)
    {
        return;
    }

    switch(expr->type)
    {
        case EXPRESSION_NONE:
        {
            APE_ASSERT(false);
            break;
        }
        case EXPRESSION_IDENT:
        {
            ident_destroy(expr->ident);
            break;
        }
        case EXPRESSION_NUMBER_LITERAL:
        case EXPRESSION_BOOL_LITERAL:
        {
            break;
        }
        case EXPRESSION_STRING_LITERAL:
        {
            allocator_free(expr->alloc, expr->string_literal);
            break;
        }
        case EXPRESSION_NULL_LITERAL:
        {
            break;
        }
        case EXPRESSION_ARRAY_LITERAL:
        {
            expr->array->destroyWithItems(expression_destroy);
            break;
        }
        case EXPRESSION_MAP_LITERAL:
        {
            expr->map.keys->destroyWithItems(expression_destroy);
            expr->map.values->destroyWithItems(expression_destroy);
            break;
        }
        case EXPRESSION_PREFIX:
        {
            expression_destroy(expr->prefix.right);
            break;
        }
        case EXPRESSION_INFIX:
        {
            expression_destroy(expr->infix.left);
            expression_destroy(expr->infix.right);
            break;
        }
        case EXPRESSION_FUNCTION_LITERAL:
        {
            FnLiteral* fn = &expr->fn_literal;
            allocator_free(expr->alloc, fn->name);
            fn->params->destroyWithItems(ident_destroy);
            code_block_destroy(fn->body);
            break;
        }
        case EXPRESSION_CALL:
        {
            expr->call_expr.args->destroyWithItems(expression_destroy);
            expression_destroy(expr->call_expr.function);
            break;
        }
        case EXPRESSION_INDEX:
        {
            expression_destroy(expr->index_expr.left);
            expression_destroy(expr->index_expr.index);
            break;
        }
        case EXPRESSION_ASSIGN:
        {
            expression_destroy(expr->assign.dest);
            expression_destroy(expr->assign.source);
            break;
        }
        case EXPRESSION_LOGICAL:
        {
            expression_destroy(expr->logical.left);
            expression_destroy(expr->logical.right);
            break;
        }
        case EXPRESSION_TERNARY:
        {
            expression_destroy(expr->ternary.test);
            expression_destroy(expr->ternary.if_true);
            expression_destroy(expr->ternary.if_false);
            break;
        }
    }
    allocator_free(expr->alloc, expr);
}

Expression* expression_copy(Expression* expr)
{
    if(!expr)
    {
        return NULL;
    }
    Expression* res = NULL;
    switch(expr->type)
    {
        case EXPRESSION_NONE:
        {
            APE_ASSERT(false);
            break;
        }
        case EXPRESSION_IDENT:
        {
            Ident* ident = ident_copy(expr->ident);
            if(!ident)
            {
                return NULL;
            }
            res = expression_make_ident(expr->alloc, ident);
            if(!res)
            {
                ident_destroy(ident);
                return NULL;
            }
            break;
        }
        case EXPRESSION_NUMBER_LITERAL:
        {
            res = expression_make_number_literal(expr->alloc, expr->number_literal);
            break;
        }
        case EXPRESSION_BOOL_LITERAL:
        {
            res = expression_make_bool_literal(expr->alloc, expr->bool_literal);
            break;
        }
        case EXPRESSION_STRING_LITERAL:
        {
            char* string_copy = ape_strdup(expr->alloc, expr->string_literal);
            if(!string_copy)
            {
                return NULL;
            }
            res = expression_make_string_literal(expr->alloc, string_copy);
            if(!res)
            {
                allocator_free(expr->alloc, string_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_NULL_LITERAL:
        {
            res = expression_make_null_literal(expr->alloc);
            break;
        }
        case EXPRESSION_ARRAY_LITERAL:
        {
            PtrArray* values_copy = expr->array->copyWithItems(expression_copy, expression_destroy);
            if(!values_copy)
            {
                return NULL;
            }
            res = expression_make_array_literal(expr->alloc, values_copy);
            if(!res)
            {
                values_copy->destroyWithItems(expression_destroy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_MAP_LITERAL:
        {
            PtrArray* keys_copy = expr->map.keys->copyWithItems(expression_copy, expression_destroy);
            PtrArray* values_copy = expr->map.values->copyWithItems(expression_copy, expression_destroy);
            if(!keys_copy || !values_copy)
            {
                keys_copy->destroyWithItems(expression_destroy);
                values_copy->destroyWithItems(expression_destroy);
                return NULL;
            }
            res = expression_make_map_literal(expr->alloc, keys_copy, values_copy);
            if(!res)
            {
                keys_copy->destroyWithItems(expression_destroy);
                values_copy->destroyWithItems(expression_destroy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_PREFIX:
        {
            Expression* right_copy = expression_copy(expr->prefix.right);
            if(!right_copy)
            {
                return NULL;
            }
            res = expression_make_prefix(expr->alloc, expr->prefix.op, right_copy);
            if(!res)
            {
                expression_destroy(right_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_INFIX:
        {
            Expression* left_copy = expression_copy(expr->infix.left);
            Expression* right_copy = expression_copy(expr->infix.right);
            if(!left_copy || !right_copy)
            {
                expression_destroy(left_copy);
                expression_destroy(right_copy);
                return NULL;
            }
            res = expression_make_infix(expr->alloc, expr->infix.op, left_copy, right_copy);
            if(!res)
            {
                expression_destroy(left_copy);
                expression_destroy(right_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_FUNCTION_LITERAL:
        {
            PtrArray* params_copy = expr->fn_literal.params->copyWithItems(ident_copy, ident_destroy);
            Codeblock* body_copy = code_block_copy(expr->fn_literal.body);
            char* name_copy = ape_strdup(expr->alloc, expr->fn_literal.name);
            if(!params_copy || !body_copy)
            {
                params_copy->destroyWithItems(ident_destroy);
                code_block_destroy(body_copy);
                allocator_free(expr->alloc, name_copy);
                return NULL;
            }
            res = expression_make_fn_literal(expr->alloc, params_copy, body_copy);
            if(!res)
            {
                params_copy->destroyWithItems(ident_destroy);
                code_block_destroy(body_copy);
                allocator_free(expr->alloc, name_copy);
                return NULL;
            }
            res->fn_literal.name = name_copy;
            break;
        }
        case EXPRESSION_CALL:
        {
            Expression* function_copy = expression_copy(expr->call_expr.function);
            PtrArray* args_copy = expr->call_expr.args->copyWithItems(expression_copy, expression_destroy);
            if(!function_copy || !args_copy)
            {
                expression_destroy(function_copy);
                expr->call_expr.args->destroyWithItems(expression_destroy);
                return NULL;
            }
            res = expression_make_call(expr->alloc, function_copy, args_copy);
            if(!res)
            {
                expression_destroy(function_copy);
                expr->call_expr.args->destroyWithItems(expression_destroy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_INDEX:
        {
            Expression* left_copy = expression_copy(expr->index_expr.left);
            Expression* index_copy = expression_copy(expr->index_expr.index);
            if(!left_copy || !index_copy)
            {
                expression_destroy(left_copy);
                expression_destroy(index_copy);
                return NULL;
            }
            res = expression_make_index(expr->alloc, left_copy, index_copy);
            if(!res)
            {
                expression_destroy(left_copy);
                expression_destroy(index_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_ASSIGN:
        {
            Expression* dest_copy = expression_copy(expr->assign.dest);
            Expression* source_copy = expression_copy(expr->assign.source);
            if(!dest_copy || !source_copy)
            {
                expression_destroy(dest_copy);
                expression_destroy(source_copy);
                return NULL;
            }
            res = expression_make_assign(expr->alloc, dest_copy, source_copy, expr->assign.is_postfix);
            if(!res)
            {
                expression_destroy(dest_copy);
                expression_destroy(source_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_LOGICAL:
        {
            Expression* left_copy = expression_copy(expr->logical.left);
            Expression* right_copy = expression_copy(expr->logical.right);
            if(!left_copy || !right_copy)
            {
                expression_destroy(left_copy);
                expression_destroy(right_copy);
                return NULL;
            }
            res = expression_make_logical(expr->alloc, expr->logical.op, left_copy, right_copy);
            if(!res)
            {
                expression_destroy(left_copy);
                expression_destroy(right_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_TERNARY:
        {
            Expression* test_copy = expression_copy(expr->ternary.test);
            Expression* if_true_copy = expression_copy(expr->ternary.if_true);
            Expression* if_false_copy = expression_copy(expr->ternary.if_false);
            if(!test_copy || !if_true_copy || !if_false_copy)
            {
                expression_destroy(test_copy);
                expression_destroy(if_true_copy);
                expression_destroy(if_false_copy);
                return NULL;
            }
            res = expression_make_ternary(expr->alloc, test_copy, if_true_copy, if_false_copy);
            if(!res)
            {
                expression_destroy(test_copy);
                expression_destroy(if_true_copy);
                expression_destroy(if_false_copy);
                return NULL;
            }
            break;
        }
    }
    if(!res)
    {
        return NULL;
    }
    res->pos = expr->pos;
    return res;
}

Expression* statement_make_define(Allocator* alloc, Ident* name, Expression* value, bool assignable)
{
    Expression* res = statement_make(alloc, EXPRESSION_DEFINE);
    if(!res)
    {
        return NULL;
    }
    res->define.name = name;
    res->define.value = value;
    res->define.assignable = assignable;
    return res;
}

Expression* statement_make_if(Allocator* alloc, PtrArray * cases, Codeblock* alternative)
{
    Expression* res = statement_make(alloc, EXPRESSION_IF);
    if(!res)
    {
        return NULL;
    }
    res->if_statement.cases = cases;
    res->if_statement.alternative = alternative;
    return res;
}

Expression* statement_make_return(Allocator* alloc, Expression* value)
{
    Expression* res = statement_make(alloc, EXPRESSION_RETURN_VALUE);
    if(!res)
    {
        return NULL;
    }
    res->return_value = value;
    return res;
}

Expression* statement_make_expression(Allocator* alloc, Expression* value)
{
    Expression* res = statement_make(alloc, EXPRESSION_EXPRESSION);
    if(!res)
    {
        return NULL;
    }
    res->expression = value;
    return res;
}

Expression* statement_make_while_loop(Allocator* alloc, Expression* test, Codeblock* body)
{
    Expression* res = statement_make(alloc, EXPRESSION_WHILE_LOOP);
    if(!res)
    {
        return NULL;
    }
    res->while_loop.test = test;
    res->while_loop.body = body;
    return res;
}

Expression* statement_make_break(Allocator* alloc)
{
    Expression* res = statement_make(alloc, EXPRESSION_BREAK);
    if(!res)
    {
        return NULL;
    }
    return res;
}

Expression* statement_make_foreach(Allocator* alloc, Ident* iterator, Expression* source, Codeblock* body)
{
    Expression* res = statement_make(alloc, EXPRESSION_FOREACH);
    if(!res)
    {
        return NULL;
    }
    res->foreach.iterator = iterator;
    res->foreach.source = source;
    res->foreach.body = body;
    return res;
}

Expression* statement_make_for_loop(Allocator* alloc, Expression* init, Expression* test, Expression* update, Codeblock* body)
{
    Expression* res = statement_make(alloc, EXPRESSION_FOR_LOOP);
    if(!res)
    {
        return NULL;
    }
    res->for_loop.init = init;
    res->for_loop.test = test;
    res->for_loop.update = update;
    res->for_loop.body = body;
    return res;
}

Expression* statement_make_continue(Allocator* alloc)
{
    Expression* res = statement_make(alloc, EXPRESSION_CONTINUE);
    if(!res)
    {
        return NULL;
    }
    return res;
}

Expression* statement_make_block(Allocator* alloc, Codeblock* block)
{
    Expression* res = statement_make(alloc, EXPRESSION_BLOCK);
    if(!res)
    {
        return NULL;
    }
    res->block = block;
    return res;
}

Expression* statement_make_import(Allocator* alloc, char* path)
{
    Expression* res = statement_make(alloc, EXPRESSION_IMPORT);
    if(!res)
    {
        return NULL;
    }
    res->import.path = path;
    return res;
}

Expression* statement_make_recover(Allocator* alloc, Ident* error_ident, Codeblock* body)
{
    Expression* res = statement_make(alloc, EXPRESSION_RECOVER);
    if(!res)
    {
        return NULL;
    }
    res->recover.error_ident = error_ident;
    res->recover.body = body;
    return res;
}

void statement_destroy(Expression* stmt)
{
    if(!stmt)
    {
        return;
    }
    switch(stmt->type)
    {
        case EXPRESSION_NONE:
        {
            APE_ASSERT(false);
            break;
        }
        case EXPRESSION_DEFINE:
        {
            ident_destroy(stmt->define.name);
            expression_destroy(stmt->define.value);
            break;
        }
        case EXPRESSION_IF:
        {
            stmt->if_statement.cases->destroyWithItems(if_case_destroy);
            code_block_destroy(stmt->if_statement.alternative);
            break;
        }
        case EXPRESSION_RETURN_VALUE:
        {
            expression_destroy(stmt->return_value);
            break;
        }
        case EXPRESSION_EXPRESSION:
        {
            expression_destroy(stmt->expression);
            break;
        }
        case EXPRESSION_WHILE_LOOP:
        {
            expression_destroy(stmt->while_loop.test);
            code_block_destroy(stmt->while_loop.body);
            break;
        }
        case EXPRESSION_BREAK:
        {
            break;
        }
        case EXPRESSION_CONTINUE:
        {
            break;
        }
        case EXPRESSION_FOREACH:
        {
            ident_destroy(stmt->foreach.iterator);
            expression_destroy(stmt->foreach.source);
            code_block_destroy(stmt->foreach.body);
            break;
        }
        case EXPRESSION_FOR_LOOP:
        {
            statement_destroy(stmt->for_loop.init);
            expression_destroy(stmt->for_loop.test);
            expression_destroy(stmt->for_loop.update);
            code_block_destroy(stmt->for_loop.body);
            break;
        }
        case EXPRESSION_BLOCK:
        {
            code_block_destroy(stmt->block);
            break;
        }
        case EXPRESSION_IMPORT:
        {
            allocator_free(stmt->alloc, stmt->import.path);
            break;
        }
        case EXPRESSION_RECOVER:
        {
            code_block_destroy(stmt->recover.body);
            ident_destroy(stmt->recover.error_ident);
            break;
        }
    }
    allocator_free(stmt->alloc, stmt);
}

Expression* statement_copy(const Expression* stmt)
{
    if(!stmt)
    {
        return NULL;
    }
    Expression* res = NULL;
    switch(stmt->type)
    {
        case EXPRESSION_NONE:
        {
            APE_ASSERT(false);
            break;
        }
        case EXPRESSION_DEFINE:
        {
            Expression* value_copy = expression_copy(stmt->define.value);
            if(!value_copy)
            {
                return NULL;
            }
            res = statement_make_define(stmt->alloc, ident_copy(stmt->define.name), value_copy, stmt->define.assignable);
            if(!res)
            {
                expression_destroy(value_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_IF:
        {
            PtrArray* cases_copy = stmt->if_statement.cases->copyWithItems(if_case_copy, if_case_destroy);
            Codeblock* alternative_copy = code_block_copy(stmt->if_statement.alternative);
            if(!cases_copy || !alternative_copy)
            {
                cases_copy->destroyWithItems(if_case_destroy);
                code_block_destroy(alternative_copy);
                return NULL;
            }
            res = statement_make_if(stmt->alloc, cases_copy, alternative_copy);
            if(res)
            {
                cases_copy->destroyWithItems(if_case_destroy);
                code_block_destroy(alternative_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_RETURN_VALUE:
        {
            Expression* value_copy = expression_copy(stmt->return_value);
            if(!value_copy)
            {
                return NULL;
            }
            res = statement_make_return(stmt->alloc, value_copy);
            if(!res)
            {
                expression_destroy(value_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_EXPRESSION:
        {
            Expression* value_copy = expression_copy(stmt->expression);
            if(!value_copy)
            {
                return NULL;
            }
            res = statement_make_expression(stmt->alloc, value_copy);
            if(!res)
            {
                expression_destroy(value_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_WHILE_LOOP:
        {
            Expression* test_copy = expression_copy(stmt->while_loop.test);
            Codeblock* body_copy = code_block_copy(stmt->while_loop.body);
            if(!test_copy || !body_copy)
            {
                expression_destroy(test_copy);
                code_block_destroy(body_copy);
                return NULL;
            }
            res = statement_make_while_loop(stmt->alloc, test_copy, body_copy);
            if(!res)
            {
                expression_destroy(test_copy);
                code_block_destroy(body_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_BREAK:
        {
            res = statement_make_break(stmt->alloc);
            break;
        }
        case EXPRESSION_CONTINUE:
        {
            res = statement_make_continue(stmt->alloc);
            break;
        }
        case EXPRESSION_FOREACH:
        {
            Expression* source_copy = expression_copy(stmt->foreach.source);
            Codeblock* body_copy = code_block_copy(stmt->foreach.body);
            if(!source_copy || !body_copy)
            {
                expression_destroy(source_copy);
                code_block_destroy(body_copy);
                return NULL;
            }
            res = statement_make_foreach(stmt->alloc, ident_copy(stmt->foreach.iterator), source_copy, body_copy);
            if(!res)
            {
                expression_destroy(source_copy);
                code_block_destroy(body_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_FOR_LOOP:
        {
            Expression* init_copy = statement_copy(stmt->for_loop.init);
            Expression* test_copy = expression_copy(stmt->for_loop.test);
            Expression* update_copy = expression_copy(stmt->for_loop.update);
            Codeblock* body_copy = code_block_copy(stmt->for_loop.body);
            if(!init_copy || !test_copy || !update_copy || !body_copy)
            {
                statement_destroy(init_copy);
                expression_destroy(test_copy);
                expression_destroy(update_copy);
                code_block_destroy(body_copy);
                return NULL;
            }
            res = statement_make_for_loop(stmt->alloc, init_copy, test_copy, update_copy, body_copy);
            if(!res)
            {
                statement_destroy(init_copy);
                expression_destroy(test_copy);
                expression_destroy(update_copy);
                code_block_destroy(body_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_BLOCK:
        {
            Codeblock* block_copy = code_block_copy(stmt->block);
            if(!block_copy)
            {
                return NULL;
            }
            res = statement_make_block(stmt->alloc, block_copy);
            if(!res)
            {
                code_block_destroy(block_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_IMPORT:
        {
            char* path_copy = ape_strdup(stmt->alloc, stmt->import.path);
            if(!path_copy)
            {
                return NULL;
            }
            res = statement_make_import(stmt->alloc, path_copy);
            if(!res)
            {
                allocator_free(stmt->alloc, path_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_RECOVER:
        {
            Codeblock* body_copy = code_block_copy(stmt->recover.body);
            Ident* error_ident_copy = ident_copy(stmt->recover.error_ident);
            if(!body_copy || !error_ident_copy)
            {
                code_block_destroy(body_copy);
                ident_destroy(error_ident_copy);
                return NULL;
            }
            res = statement_make_recover(stmt->alloc, error_ident_copy, body_copy);
            if(!res)
            {
                code_block_destroy(body_copy);
                ident_destroy(error_ident_copy);
                return NULL;
            }
            break;
        }
    }
    if(!res)
    {
        return NULL;
    }
    res->pos = stmt->pos;
    return res;
}

Codeblock* code_block_make(Allocator* alloc, PtrArray * statements)
{
    Codeblock* block = (Codeblock*)allocator_malloc(alloc, sizeof(Codeblock));
    if(!block)
    {
        return NULL;
    }
    block->alloc = alloc;
    block->statements = statements;
    return block;
}

void code_block_destroy(Codeblock* block)
{
    if(!block)
    {
        return;
    }
    block->statements->destroyWithItems(statement_destroy);
    allocator_free(block->alloc, block);
}

Codeblock* code_block_copy(Codeblock* block)
{
    if(!block)
    {
        return NULL;
    }
    PtrArray* statements_copy = block->statements->copyWithItems(statement_copy, statement_destroy);
    if(!statements_copy)
    {
        return NULL;
    }
    Codeblock* res = code_block_make(block->alloc, statements_copy);
    if(!res)
    {
        statements_copy->destroyWithItems(statement_destroy);
        return NULL;
    }
    return res;
}






void ident_destroy(Ident* ident)
{
    if(!ident)
    {
        return;
    }
    allocator_free(ident->alloc, ident->value);
    ident->value = NULL;
    ident->pos = src_pos_invalid;
    allocator_free(ident->alloc, ident);
}

IfCase* if_case_make(Allocator* alloc, Expression* test, Codeblock* consequence)
{
    IfCase* res = (IfCase*)allocator_malloc(alloc, sizeof(IfCase));
    if(!res)
    {
        return NULL;
    }
    res->alloc = alloc;
    res->test = test;
    res->consequence = consequence;
    return res;
}

void if_case_destroy(IfCase* cond)
{
    if(!cond)
    {
        return;
    }
    expression_destroy(cond->test);
    code_block_destroy(cond->consequence);
    allocator_free(cond->alloc, cond);
}

IfCase* if_case_copy(IfCase* if_case)
{
    if(!if_case)
    {
        return NULL;
    }
    Expression* test_copy = NULL;
    Codeblock* consequence_copy = NULL;
    IfCase* if_case_copy = NULL;

    test_copy = expression_copy(if_case->test);
    if(!test_copy)
    {
        goto err;
    }
    consequence_copy = code_block_copy(if_case->consequence);
    if(!test_copy || !consequence_copy)
    {
        goto err;
    }
    if_case_copy = if_case_make(if_case->alloc, test_copy, consequence_copy);
    if(!if_case_copy)
    {
        goto err;
    }
    return if_case_copy;
err:
    expression_destroy(test_copy);
    code_block_destroy(consequence_copy);
    if_case_destroy(if_case_copy);
    return NULL;
}

// INTERNAL
Expression* expression_make(Allocator* alloc, Expr_type type)
{
    Expression* res = (Expression*)allocator_malloc(alloc, sizeof(Expression));
    if(!res)
    {
        return NULL;
    }
    res->alloc = alloc;
    res->type = type;
    res->pos = src_pos_invalid;
    return res;
}

Expression* statement_make(Allocator* alloc, Expr_type type)
{
    Expression* res = (Expression*)allocator_malloc(alloc, sizeof(Expression));
    if(!res)
    {
        return NULL;
    }
    res->alloc = alloc;
    res->type = type;
    res->pos = src_pos_invalid;
    return res;
}


Parser* parser_make(Allocator* alloc, const Config* config, ErrorList* errors)
{
    Parser* parser;
    parser = (Parser*)allocator_malloc(alloc, sizeof(Parser));
    if(!parser)
    {
        return NULL;
    }
    memset(parser, 0, sizeof(Parser));

    parser->alloc = alloc;
    parser->config = config;
    parser->errors = errors;

    parser->right_assoc_parse_fns[TOKEN_IDENT] = parse_identifier;
    parser->right_assoc_parse_fns[TOKEN_NUMBER] = parse_number_literal;
    parser->right_assoc_parse_fns[TOKEN_TRUE] = parse_bool_literal;
    parser->right_assoc_parse_fns[TOKEN_FALSE] = parse_bool_literal;
    parser->right_assoc_parse_fns[TOKEN_STRING] = parse_string_literal;
    parser->right_assoc_parse_fns[TOKEN_TEMPLATE_STRING] = parse_template_string_literal;
    parser->right_assoc_parse_fns[TOKEN_NULL] = parse_null_literal;
    parser->right_assoc_parse_fns[TOKEN_BANG] = parse_prefix_expression;
    parser->right_assoc_parse_fns[TOKEN_MINUS] = parse_prefix_expression;
    parser->right_assoc_parse_fns[TOKEN_LPAREN] = parse_grouped_expression;
    parser->right_assoc_parse_fns[TOKEN_FUNCTION] = parse_function_literal;
    parser->right_assoc_parse_fns[TOKEN_LBRACKET] = parse_array_literal;
    parser->right_assoc_parse_fns[TOKEN_LBRACE] = parse_map_literal;
    parser->right_assoc_parse_fns[TOKEN_PLUS_PLUS] = parse_incdec_prefix_expression;
    parser->right_assoc_parse_fns[TOKEN_MINUS_MINUS] = parse_incdec_prefix_expression;

    parser->left_assoc_parse_fns[TOKEN_PLUS] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_MINUS] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_SLASH] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_ASTERISK] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_PERCENT] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_EQ] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_NOT_EQ] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_LT] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_LTE] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_GT] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_GTE] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_LPAREN] = parse_call_expression;
    parser->left_assoc_parse_fns[TOKEN_LBRACKET] = parse_index_expression;
    parser->left_assoc_parse_fns[TOKEN_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_PLUS_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_MINUS_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_SLASH_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_ASTERISK_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_PERCENT_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_BIT_AND_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_BIT_OR_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_BIT_XOR_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_LSHIFT_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_RSHIFT_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_DOT] = parse_dot_expression;
    parser->left_assoc_parse_fns[TOKEN_AND] = parse_logical_expression;
    parser->left_assoc_parse_fns[TOKEN_OR] = parse_logical_expression;
    parser->left_assoc_parse_fns[TOKEN_BIT_AND] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_BIT_OR] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_BIT_XOR] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_LSHIFT] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_RSHIFT] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_QUESTION] = parse_ternary_expression;
    parser->left_assoc_parse_fns[TOKEN_PLUS_PLUS] = parse_incdec_postfix_expression;
    parser->left_assoc_parse_fns[TOKEN_MINUS_MINUS] = parse_incdec_postfix_expression;

    parser->depth = 0;

    return parser;
}

void parser_destroy(Parser* parser)
{
    if(!parser)
    {
        return;
    }
    allocator_free(parser->alloc, parser);
}

PtrArray * parser_parse_all(Parser* parser, const char* input, CompiledFile* file)
{
    bool ok;
    PtrArray* statements;
    Expression* stmt;
    parser->depth = 0;
    ok = lexer_init(&parser->lexer, parser->alloc, parser->errors, input, file);
    if(!ok)
    {
        return NULL;
    }
    lexer_next_token(&parser->lexer);
    lexer_next_token(&parser->lexer);
    statements = PtrArray::make(parser->alloc);
    if(!statements)
    {
        return NULL;
    }
    while(!lexer_cur_token_is(&parser->lexer, TOKEN_EOF))
    {
        if(lexer_cur_token_is(&parser->lexer, TOKEN_SEMICOLON))
        {
            lexer_next_token(&parser->lexer);
            continue;
        }
        stmt = parse_statement(parser);
        if(!stmt)
        {
            goto err;
        }
        ok = statements->add(stmt);
        if(!ok)
        {
            statement_destroy(stmt);
            goto err;
        }
    }
    if(errors_get_count(parser->errors) > 0)
    {
        goto err;
    }

    return statements;
err:
    statements->destroyWithItems(statement_destroy);
    return NULL;
}

// INTERNAL
Expression* parse_statement(Parser* p)
{
    Position pos = p->lexer.cur_token.pos;

    Expression* res = NULL;
    switch(p->lexer.cur_token.type)
    {
        case TOKEN_VAR:
        case TOKEN_CONST:
        {
            res = parse_define_statement(p);
            break;
        }
        case TOKEN_IF:
        {
            res = parse_if_statement(p);
            break;
        }
        case TOKEN_RETURN:
        {
            res = parse_return_statement(p);
            break;
        }
        case TOKEN_WHILE:
        {
            res = parse_while_loop_statement(p);
            break;
        }
        case TOKEN_BREAK:
        {
            res = parse_break_statement(p);
            break;
        }
        case TOKEN_FOR:
        {
            res = parse_for_loop_statement(p);
            break;
        }
        case TOKEN_FUNCTION:
        {
            if(lexer_peek_token_is(&p->lexer, TOKEN_IDENT))
            {
                res = parse_function_statement(p);
            }
            else
            {
                res = parse_expression_statement(p);
            }
            break;
        }
        case TOKEN_LBRACE:
        {
            if(p->config->repl_mode && p->depth == 0)
            {
                res = parse_expression_statement(p);
            }
            else
            {
                res = parse_block_statement(p);
            }
            break;
        }
        case TOKEN_CONTINUE:
        {
            res = parse_continue_statement(p);
            break;
        }
        case TOKEN_IMPORT:
        {
            res = parse_import_statement(p);
            break;
        }
        case TOKEN_RECOVER:
        {
            res = parse_recover_statement(p);
            break;
        }
        default:
        {
            res = parse_expression_statement(p);
            break;
        }
    }
    if(res)
    {
        res->pos = pos;
    }
    return res;
}

Expression* parse_define_statement(Parser* p)
{
    bool assignable;
    Ident* name_ident;
    Expression* value;
    Expression* res;
    name_ident = NULL;
    value = NULL;
    assignable = lexer_cur_token_is(&p->lexer, TOKEN_VAR);
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_IDENT))
    {
        goto err;
    }
    name_ident = ident_make(p->alloc, p->lexer.cur_token);
    if(!name_ident)
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_ASSIGN))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    value = parse_expression(p, PRECEDENCE_LOWEST);
    if(!value)
    {
        goto err;
    }
    if(value->type == EXPRESSION_FUNCTION_LITERAL)
    {
        value->fn_literal.name = ape_strdup(p->alloc, name_ident->value);
        if(!value->fn_literal.name)
        {
            goto err;
        }
    }
    res = statement_make_define(p->alloc, name_ident, value, assignable);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    expression_destroy(value);
    ident_destroy(name_ident);
    return NULL;
}

Expression* parse_if_statement(Parser* p)
{
    bool ok;
    PtrArray* cases;
    Codeblock* alternative;
    IfCase* elif;
    Expression* res;
    IfCase* cond;
    cases = NULL;
    alternative = NULL;
    cases = PtrArray::make(p->alloc);
    if(!cases)
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_LPAREN))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    cond = if_case_make(p->alloc, NULL, NULL);
    if(!cond)
    {
        goto err;
    }
    ok = cases->add(cond);
    if(!ok)
    {
        if_case_destroy(cond);
        goto err;
    }
    cond->test = parse_expression(p, PRECEDENCE_LOWEST);
    if(!cond->test)
    {
        goto err;
    }
    if(!lexer_expect_current(&p->lexer, TOKEN_RPAREN))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    cond->consequence = parse_code_block(p);
    if(!cond->consequence)
    {
        goto err;
    }
    while(lexer_cur_token_is(&p->lexer, TOKEN_ELSE))
    {
        lexer_next_token(&p->lexer);
        if(lexer_cur_token_is(&p->lexer, TOKEN_IF))
        {
            lexer_next_token(&p->lexer);
            if(!lexer_expect_current(&p->lexer, TOKEN_LPAREN))
            {
                goto err;
            }
            lexer_next_token(&p->lexer);
            elif = if_case_make(p->alloc, NULL, NULL);
            if(!elif)
            {
                goto err;
            }
            ok = cases->add(elif);
            if(!ok)
            {
                if_case_destroy(elif);
                goto err;
            }
            elif->test = parse_expression(p, PRECEDENCE_LOWEST);
            if(!elif->test)
            {
                goto err;
            }
            if(!lexer_expect_current(&p->lexer, TOKEN_RPAREN))
            {
                goto err;
            }
            lexer_next_token(&p->lexer);
            elif->consequence = parse_code_block(p);
            if(!elif->consequence)
            {
                goto err;
            }
        }
        else
        {
            alternative = parse_code_block(p);
            if(!alternative)
            {
                goto err;
            }
        }
    }
    res = statement_make_if(p->alloc, cases, alternative);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    cases->destroyWithItems(if_case_destroy);
    code_block_destroy(alternative);
    return NULL;
}

Expression* parse_return_statement(Parser* p)
{
    Expression* expr;
    Expression* res;
    expr = NULL;
    lexer_next_token(&p->lexer);
    if(!lexer_cur_token_is(&p->lexer, TOKEN_SEMICOLON) && !lexer_cur_token_is(&p->lexer, TOKEN_RBRACE) && !lexer_cur_token_is(&p->lexer, TOKEN_EOF))
    {
        expr = parse_expression(p, PRECEDENCE_LOWEST);
        if(!expr)
        {
            return NULL;
        }
    }
    res = statement_make_return(p->alloc, expr);
    if(!res)
    {
        expression_destroy(expr);
        return NULL;
    }
    return res;
}

Expression* parse_expression_statement(Parser* p)
{
    Expression* expr;
    Expression* res;
    expr = parse_expression(p, PRECEDENCE_LOWEST);
    if(!expr)
    {
        return NULL;
    }
    if(expr && (!p->config->repl_mode || p->depth > 0))
    {
        if(expr->type != EXPRESSION_ASSIGN && expr->type != EXPRESSION_CALL)
        {
            errors_add_errorf(p->errors, APE_ERROR_PARSING, expr->pos, "Only assignments and function calls can be expression statements");
            expression_destroy(expr);
            return NULL;
        }
    }
    res = statement_make_expression(p->alloc, expr);
    if(!res)
    {
        expression_destroy(expr);
        return NULL;
    }
    return res;
}

Expression* parse_while_loop_statement(Parser* p)
{
    Expression* test;
    Codeblock* body;
    Expression* res;
    test = NULL;
    body = NULL;
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_LPAREN))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    test = parse_expression(p, PRECEDENCE_LOWEST);
    if(!test)
    {
        goto err;
    }
    if(!lexer_expect_current(&p->lexer, TOKEN_RPAREN))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    body = parse_code_block(p);
    if(!body)
    {
        goto err;
    }
    res = statement_make_while_loop(p->alloc, test, body);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    code_block_destroy(body);
    expression_destroy(test);
    return NULL;
}

Expression* parse_break_statement(Parser* p)
{
    lexer_next_token(&p->lexer);
    return statement_make_break(p->alloc);
}

Expression* parse_continue_statement(Parser* p)
{
    lexer_next_token(&p->lexer);
    return statement_make_continue(p->alloc);
}

Expression* parse_block_statement(Parser* p)
{
    Codeblock* block;
    Expression* res;
    block = parse_code_block(p);
    if(!block)
    {
        return NULL;
    }
    res = statement_make_block(p->alloc, block);
    if(!res)
    {
        code_block_destroy(block);
        return NULL;
    }
    return res;
}

Expression* parse_import_statement(Parser* p)
{
    char* processed_name;
    Expression* res;
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_STRING))
    {
        return NULL;
    }
    processed_name = process_and_copy_string(p->alloc, p->lexer.cur_token.literal, p->lexer.cur_token.len);
    if(!processed_name)
    {
        errors_add_error(p->errors, APE_ERROR_PARSING, p->lexer.cur_token.pos, "Error when parsing module name");
        return NULL;
    }
    lexer_next_token(&p->lexer);
    res= statement_make_import(p->alloc, processed_name);
    if(!res)
    {
        allocator_free(p->alloc, processed_name);
        return NULL;
    }
    return res;
}

Expression* parse_recover_statement(Parser* p)
{
    Ident* error_ident;
    Codeblock* body;
    Expression* res;
    error_ident = NULL;
    body = NULL;
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_LPAREN))
    {
        return NULL;
    }
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_IDENT))
    {
        return NULL;
    }
    error_ident = ident_make(p->alloc, p->lexer.cur_token);
    if(!error_ident)
    {
        return NULL;
    }
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_RPAREN))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    body = parse_code_block(p);
    if(!body)
    {
        goto err;
    }
    res = statement_make_recover(p->alloc, error_ident, body);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    code_block_destroy(body);
    ident_destroy(error_ident);
    return NULL;
}

Expression* parse_for_loop_statement(Parser* p)
{
    lexer_next_token(&p->lexer);

    if(!lexer_expect_current(&p->lexer, TOKEN_LPAREN))
    {
        return NULL;
    }

    lexer_next_token(&p->lexer);

    if(lexer_cur_token_is(&p->lexer, TOKEN_IDENT) && lexer_peek_token_is(&p->lexer, TOKEN_IN))
    {
        return parse_foreach(p);
    }
    return parse_classic_for_loop(p);
}

Expression* parse_foreach(Parser* p)
{
    Expression* source;
    Codeblock* body;
    Ident* iterator_ident;
    Expression* res;
    source = NULL;
    body = NULL;
    iterator_ident = ident_make(p->alloc, p->lexer.cur_token);
    if(!iterator_ident)
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_IN))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    source = parse_expression(p, PRECEDENCE_LOWEST);
    if(!source)
    {
        goto err;
    }
    if(!lexer_expect_current(&p->lexer, TOKEN_RPAREN))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    body = parse_code_block(p);
    if(!body)
    {
        goto err;
    }
    res = statement_make_foreach(p->alloc, iterator_ident, source, body);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    code_block_destroy(body);
    ident_destroy(iterator_ident);
    expression_destroy(source);
    return NULL;
}

Expression* parse_classic_for_loop(Parser* p)
{
    Expression* init;
    Expression* test;
    Expression* update;
    Codeblock* body;
    Expression* res;

    init = NULL;
    test = NULL;
    update = NULL;
    body = NULL;
    if(!lexer_cur_token_is(&p->lexer, TOKEN_SEMICOLON))
    {
        init = parse_statement(p);
        if(!init)
        {
            goto err;
        }
        if(init->type != EXPRESSION_DEFINE && init->type != EXPRESSION_EXPRESSION)
        {
            errors_add_errorf(p->errors, APE_ERROR_PARSING, init->pos, "for loop's init clause should be a define statement or an expression");
            goto err;
        }
        if(!lexer_expect_current(&p->lexer, TOKEN_SEMICOLON))
        {
            goto err;
        }
    }
    lexer_next_token(&p->lexer);
    if(!lexer_cur_token_is(&p->lexer, TOKEN_SEMICOLON))
    {
        test = parse_expression(p, PRECEDENCE_LOWEST);
        if(!test)
        {
            goto err;
        }
        if(!lexer_expect_current(&p->lexer, TOKEN_SEMICOLON))
        {
            goto err;
        }
    }
    lexer_next_token(&p->lexer);
    if(!lexer_cur_token_is(&p->lexer, TOKEN_RPAREN))
    {
        update = parse_expression(p, PRECEDENCE_LOWEST);
        if(!update)
        {
            goto err;
        }
        if(!lexer_expect_current(&p->lexer, TOKEN_RPAREN))
        {
            goto err;
        }
    }
    lexer_next_token(&p->lexer);
    body = parse_code_block(p);
    if(!body)
    {
        goto err;
    }
    res = statement_make_for_loop(p->alloc, init, test, update, body);
    if(!res)
    {
        goto err;
    }

    return res;
err:
    statement_destroy(init);
    expression_destroy(test);
    expression_destroy(update);
    code_block_destroy(body);
    return NULL;
}

Expression* parse_function_statement(Parser* p)
{
    Ident* name_ident;
    Expression* value;
    Position pos;
    Expression* res;
    value = NULL;
    name_ident = NULL;
    pos = p->lexer.cur_token.pos;
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_IDENT))
    {
        goto err;
    }
    name_ident = ident_make(p->alloc, p->lexer.cur_token);
    if(!name_ident)
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    value = parse_function_literal(p);
    if(!value)
    {
        goto err;
    }
    value->pos = pos;
    value->fn_literal.name = ape_strdup(p->alloc, name_ident->value);
    if(!value->fn_literal.name)
    {
        goto err;
    }
    res = statement_make_define(p->alloc, name_ident, value, false);
    if(!res)
    {
        goto err;
    }
    return res;

err:
    expression_destroy(value);
    ident_destroy(name_ident);
    return NULL;
}

Codeblock* parse_code_block(Parser* p)
{
    bool ok;
    PtrArray* statements;
    Expression* stmt;
    Codeblock* res;
    if(!lexer_expect_current(&p->lexer, TOKEN_LBRACE))
    {
        return NULL;
    }
    lexer_next_token(&p->lexer);
    p->depth++;
    statements = PtrArray::make(p->alloc);
    if(!statements)
    {
        goto err;
    }
    while(!lexer_cur_token_is(&p->lexer, TOKEN_RBRACE))
    {
        if(lexer_cur_token_is(&p->lexer, TOKEN_EOF))
        {
            errors_add_error(p->errors, APE_ERROR_PARSING, p->lexer.cur_token.pos, "Unexpected EOF");
            goto err;
        }
        if(lexer_cur_token_is(&p->lexer, TOKEN_SEMICOLON))
        {
            lexer_next_token(&p->lexer);
            continue;
        }
        stmt = parse_statement(p);
        if(!stmt)
        {
            goto err;
        }
        ok = statements->add(stmt);
        if(!ok)
        {
            statement_destroy(stmt);
            goto err;
        }
    }
    lexer_next_token(&p->lexer);
    p->depth--;
    res = code_block_make(p->alloc, statements);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    p->depth--;
    statements->destroyWithItems(statement_destroy);
    return NULL;
}

Expression* parse_expression(Parser* p, Precedence prec)
{
    char* literal;
    Position pos;
    RightAssocParseFNCallback parse_right_assoc;
    LeftAssocParseFNCallback parse_left_assoc;
    Expression* left_expr;
    Expression* new_left_expr;
    pos = p->lexer.cur_token.pos;
    if(p->lexer.cur_token.type == TOKEN_INVALID)
    {
        errors_add_error(p->errors, APE_ERROR_PARSING, p->lexer.cur_token.pos, "Illegal token");
        return NULL;
    }
    parse_right_assoc = p->right_assoc_parse_fns[p->lexer.cur_token.type];
    if(!parse_right_assoc)
    {
        literal = token_duplicate_literal(p->alloc, &p->lexer.cur_token);
        errors_add_errorf(p->errors, APE_ERROR_PARSING, p->lexer.cur_token.pos, "No prefix parse function for \"%s\" found", literal);
        allocator_free(p->alloc, literal);
        return NULL;
    }
    left_expr = parse_right_assoc(p);
    if(!left_expr)
    {
        return NULL;
    }
    left_expr->pos = pos;
    while(!lexer_cur_token_is(&p->lexer, TOKEN_SEMICOLON) && prec < get_precedence(p->lexer.cur_token.type))
    {
        parse_left_assoc = p->left_assoc_parse_fns[p->lexer.cur_token.type];
        if(!parse_left_assoc)
        {
            return left_expr;
        }
        pos = p->lexer.cur_token.pos;
        new_left_expr= parse_left_assoc(p, left_expr);
        if(!new_left_expr)
        {
            expression_destroy(left_expr);
            return NULL;
        }
        new_left_expr->pos = pos;
        left_expr = new_left_expr;
    }
    return left_expr;
}

Expression* parse_identifier(Parser* p)
{
    Ident* ident;
    Expression* res;
    ident = ident_make(p->alloc, p->lexer.cur_token);
    if(!ident)
    {
        return NULL;
    }
    res = expression_make_ident(p->alloc, ident);
    if(!res)
    {
        ident_destroy(ident);
        return NULL;
    }
    lexer_next_token(&p->lexer);
    return res;
}

Expression* parse_number_literal(Parser* p)
{
    char* end;
    char* literal;
    double number;
    long parsed_len;
    number = 0;
    errno = 0;
    number = strtod(p->lexer.cur_token.literal, &end);
    parsed_len = end - p->lexer.cur_token.literal;
    if(errno || parsed_len != p->lexer.cur_token.len)
    {
        literal = token_duplicate_literal(p->alloc, &p->lexer.cur_token);
        errors_add_errorf(p->errors, APE_ERROR_PARSING, p->lexer.cur_token.pos, "Parsing number literal \"%s\" failed", literal);
        allocator_free(p->alloc, literal);
        return NULL;
    }
    lexer_next_token(&p->lexer);
    return expression_make_number_literal(p->alloc, number);
}

Expression* parse_bool_literal(Parser* p)
{
    Expression* res;
    res = expression_make_bool_literal(p->alloc, p->lexer.cur_token.type == TOKEN_TRUE);
    lexer_next_token(&p->lexer);
    return res;
}

Expression* parse_string_literal(Parser* p)
{
    char* processed_literal;
    processed_literal = process_and_copy_string(p->alloc, p->lexer.cur_token.literal, p->lexer.cur_token.len);
    if(!processed_literal)
    {
        errors_add_error(p->errors, APE_ERROR_PARSING, p->lexer.cur_token.pos, "Error when parsing string literal");
        return NULL;
    }
    lexer_next_token(&p->lexer);
    Expression* res = expression_make_string_literal(p->alloc, processed_literal);
    if(!res)
    {
        allocator_free(p->alloc, processed_literal);
        return NULL;
    }
    return res;
}

Expression* parse_template_string_literal(Parser* p)
{
    char* processed_literal;
    Expression* left_string_expr;
    Expression* template_expr;
    Expression* to_str_call_expr;
    Expression* left_add_expr;
    Expression* right_expr;
    Expression* right_add_expr;
    Position pos;

    processed_literal = NULL;
    left_string_expr = NULL;
    template_expr = NULL;
    to_str_call_expr = NULL;
    left_add_expr = NULL;
    right_expr = NULL;
    right_add_expr = NULL;
    processed_literal = process_and_copy_string(p->alloc, p->lexer.cur_token.literal, p->lexer.cur_token.len);
    if(!processed_literal)
    {
        errors_add_error(p->errors, APE_ERROR_PARSING, p->lexer.cur_token.pos, "Error when parsing string literal");
        return NULL;
    }
    lexer_next_token(&p->lexer);

    if(!lexer_expect_current(&p->lexer, TOKEN_LBRACE))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);

    pos = p->lexer.cur_token.pos;

    left_string_expr = expression_make_string_literal(p->alloc, processed_literal);
    if(!left_string_expr)
    {
        goto err;
    }
    left_string_expr->pos = pos;
    processed_literal = NULL;
    pos = p->lexer.cur_token.pos;
    template_expr = parse_expression(p, PRECEDENCE_LOWEST);
    if(!template_expr)
    {
        goto err;
    }
    to_str_call_expr = wrap_expression_in_function_call(p->alloc, template_expr, "to_str");
    if(!to_str_call_expr)
    {
        goto err;
    }
    to_str_call_expr->pos = pos;
    template_expr = NULL;
    left_add_expr = expression_make_infix(p->alloc, OPERATOR_PLUS, left_string_expr, to_str_call_expr);
    if(!left_add_expr)
    {
        goto err;
    }
    left_add_expr->pos = pos;
    left_string_expr = NULL;
    to_str_call_expr = NULL;
    if(!lexer_expect_current(&p->lexer, TOKEN_RBRACE))
    {
        goto err;
    }
    lexer_previous_token(&p->lexer);
    lexer_continue_template_string(&p->lexer);
    lexer_next_token(&p->lexer);
    lexer_next_token(&p->lexer);
    pos = p->lexer.cur_token.pos;
    right_expr = parse_expression(p, PRECEDENCE_HIGHEST);
    if(!right_expr)
    {
        goto err;
    }
    right_add_expr = expression_make_infix(p->alloc, OPERATOR_PLUS, left_add_expr, right_expr);
    if(!right_add_expr)
    {
        goto err;
    }
    right_add_expr->pos = pos;
    left_add_expr = NULL;
    right_expr = NULL;

    return right_add_expr;
err:
    expression_destroy(right_add_expr);
    expression_destroy(right_expr);
    expression_destroy(left_add_expr);
    expression_destroy(to_str_call_expr);
    expression_destroy(template_expr);
    expression_destroy(left_string_expr);
    allocator_free(p->alloc, processed_literal);
    return NULL;
}

Expression* parse_null_literal(Parser* p)
{
    lexer_next_token(&p->lexer);
    return expression_make_null_literal(p->alloc);
}

Expression* parse_array_literal(Parser* p)
{
    PtrArray* array;
    Expression* res;
    array = parse_expression_list(p, TOKEN_LBRACKET, TOKEN_RBRACKET, true);
    if(!array)
    {
        return NULL;
    }
    res = expression_make_array_literal(p->alloc, array);
    if(!res)
    {
        array->destroyWithItems(expression_destroy);
        return NULL;
    }
    return res;
}

Expression* parse_map_literal(Parser* p)
{
    bool ok;
    char* str;
    PtrArray* keys;
    PtrArray* values;
    Expression* key;
    Expression* value;
    Expression* res;
    keys = PtrArray::make(p->alloc);
    values = PtrArray::make(p->alloc);
    if(!keys || !values)
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    while(!lexer_cur_token_is(&p->lexer, TOKEN_RBRACE))
    {
        key = NULL;
        if(lexer_cur_token_is(&p->lexer, TOKEN_IDENT))
        {
            str = token_duplicate_literal(p->alloc, &p->lexer.cur_token);
            key = expression_make_string_literal(p->alloc, str);
            if(!key)
            {
                allocator_free(p->alloc, str);
                goto err;
            }
            key->pos = p->lexer.cur_token.pos;
            lexer_next_token(&p->lexer);
        }
        else
        {
            key = parse_expression(p, PRECEDENCE_LOWEST);
            if(!key)
            {
                goto err;
            }
            switch(key->type)
            {
                case EXPRESSION_STRING_LITERAL:
                case EXPRESSION_NUMBER_LITERAL:
                case EXPRESSION_BOOL_LITERAL:
                {
                    break;
                }
                default:
                {
                    errors_add_errorf(p->errors, APE_ERROR_PARSING, key->pos, "Invalid map literal key type");
                    expression_destroy(key);
                    goto err;
                }
            }
        }

        ok = keys->add(key);
        if(!ok)
        {
            expression_destroy(key);
            goto err;
        }

        if(!lexer_expect_current(&p->lexer, TOKEN_COLON))
        {
            goto err;
        }

        lexer_next_token(&p->lexer);

        value = parse_expression(p, PRECEDENCE_LOWEST);
        if(!value)
        {
            goto err;
        }
        ok = values->add(value);
        if(!ok)
        {
            expression_destroy(value);
            goto err;
        }

        if(lexer_cur_token_is(&p->lexer, TOKEN_RBRACE))
        {
            break;
        }

        if(!lexer_expect_current(&p->lexer, TOKEN_COMMA))
        {
            goto err;
        }

        lexer_next_token(&p->lexer);
    }

    lexer_next_token(&p->lexer);

    res = expression_make_map_literal(p->alloc, keys, values);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    keys->destroyWithItems(expression_destroy);
    values->destroyWithItems(expression_destroy);
    return NULL;
}

Expression* parse_prefix_expression(Parser* p)
{
    Operator op;
    Expression* right;
    Expression* res;
    op = token_to_operator(p->lexer.cur_token.type);
    lexer_next_token(&p->lexer);
    right = parse_expression(p, PRECEDENCE_PREFIX);
    if(!right)
    {
        return NULL;
    }
    res = expression_make_prefix(p->alloc, op, right);
    if(!res)
    {
        expression_destroy(right);
        return NULL;
    }
    return res;
}

Expression* parse_infix_expression(Parser* p, Expression* left)
{
    Operator op;
    Precedence prec;
    Expression* right;
    Expression* res;
    op = token_to_operator(p->lexer.cur_token.type);
    prec = get_precedence(p->lexer.cur_token.type);
    lexer_next_token(&p->lexer);
    right = parse_expression(p, prec);
    if(!right)
    {
        return NULL;
    }
    res = expression_make_infix(p->alloc, op, left, right);
    if(!res)
    {
        expression_destroy(right);
        return NULL;
    }
    return res;
}

Expression* parse_grouped_expression(Parser* p)
{
    Expression* expr;
    lexer_next_token(&p->lexer);
    expr = parse_expression(p, PRECEDENCE_LOWEST);
    if(!expr || !lexer_expect_current(&p->lexer, TOKEN_RPAREN))
    {
        expression_destroy(expr);
        return NULL;
    }
    lexer_next_token(&p->lexer);
    return expr;
}

Expression* parse_function_literal(Parser* p)
{
    bool ok;
    PtrArray* params;
    Codeblock* body;
    Expression* res;
    p->depth++;
    params = NULL;
    body = NULL;
    if(lexer_cur_token_is(&p->lexer, TOKEN_FUNCTION))
    {
        lexer_next_token(&p->lexer);
    }
    params = PtrArray::make(p->alloc);
    ok = parse_function_parameters(p, params);
    if(!ok)
    {
        goto err;
    }
    body = parse_code_block(p);
    if(!body)
    {
        goto err;
    }
    res = expression_make_fn_literal(p->alloc, params, body);
    if(!res)
    {
        goto err;
    }
    p->depth -= 1;
    return res;
err:
    code_block_destroy(body);
    params->destroyWithItems(ident_destroy);
    p->depth -= 1;
    return NULL;
}

bool parse_function_parameters(Parser* p, PtrArray * out_params)
{
    bool ok;
    Ident* ident;
    if(!lexer_expect_current(&p->lexer, TOKEN_LPAREN))
    {
        return false;
    }
    lexer_next_token(&p->lexer);
    if(lexer_cur_token_is(&p->lexer, TOKEN_RPAREN))
    {
        lexer_next_token(&p->lexer);
        return true;
    }
    if(!lexer_expect_current(&p->lexer, TOKEN_IDENT))
    {
        return false;
    }
    ident = ident_make(p->alloc, p->lexer.cur_token);
    if(!ident)
    {
        return false;
    }
    ok = out_params->add(ident);
    if(!ok)
    {
        ident_destroy(ident);
        return false;
    }
    lexer_next_token(&p->lexer);
    while(lexer_cur_token_is(&p->lexer, TOKEN_COMMA))
    {
        lexer_next_token(&p->lexer);
        if(!lexer_expect_current(&p->lexer, TOKEN_IDENT))
        {
            return false;
        }
        ident = ident_make(p->alloc, p->lexer.cur_token);
        if(!ident)
        {
            return false;
        }
        ok = out_params->add(ident);
        if(!ok)
        {
            ident_destroy(ident);
            return false;
        }
        lexer_next_token(&p->lexer);
    }
    if(!lexer_expect_current(&p->lexer, TOKEN_RPAREN))
    {
        return false;
    }

    lexer_next_token(&p->lexer);
    return true;
}

Expression* parse_call_expression(Parser* p, Expression* left)
{
    Expression* function;
    PtrArray* args;
    Expression* res;
    function = left;
    args = parse_expression_list(p, TOKEN_LPAREN, TOKEN_RPAREN, false);
    if(!args)
    {
        return NULL;
    }
    res = expression_make_call(p->alloc, function, args);
    if(!res)
    {
        args->destroyWithItems(expression_destroy);
        return NULL;
    }
    return res;
}

PtrArray* parse_expression_list(Parser* p, TokenType start_token, TokenType end_token, bool trailing_comma_allowed)
{
    bool ok;
    PtrArray* res;
    Expression* arg_expr;
    if(!lexer_expect_current(&p->lexer, start_token))
    {
        return NULL;
    }
    lexer_next_token(&p->lexer);
    res = PtrArray::make(p->alloc);
    if(lexer_cur_token_is(&p->lexer, end_token))
    {
        lexer_next_token(&p->lexer);
        return res;
    }
    arg_expr = parse_expression(p, PRECEDENCE_LOWEST);
    if(!arg_expr)
    {
        goto err;
    }
    ok = res->add(arg_expr);
    if(!ok)
    {
        expression_destroy(arg_expr);
        goto err;
    }
    while(lexer_cur_token_is(&p->lexer, TOKEN_COMMA))
    {
        lexer_next_token(&p->lexer);
        if(trailing_comma_allowed && lexer_cur_token_is(&p->lexer, end_token))
        {
            break;
        }
        arg_expr = parse_expression(p, PRECEDENCE_LOWEST);
        if(!arg_expr)
        {
            goto err;
        }
        ok = res->add(arg_expr);
        if(!ok)
        {
            expression_destroy(arg_expr);
            goto err;
        }
    }
    if(!lexer_expect_current(&p->lexer, end_token))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    return res;
err:
    res->destroyWithItems(expression_destroy);
    return NULL;
}

Expression* parse_index_expression(Parser* p, Expression* left)
{
    Expression* index;
    Expression* res;
    lexer_next_token(&p->lexer);
    index = parse_expression(p, PRECEDENCE_LOWEST);
    if(!index)
    {
        return NULL;
    }
    if(!lexer_expect_current(&p->lexer, TOKEN_RBRACKET))
    {
        expression_destroy(index);
        return NULL;
    }
    lexer_next_token(&p->lexer);
    res = expression_make_index(p->alloc, left, index);
    if(!res)
    {
        expression_destroy(index);
        return NULL;
    }
    return res;
}

Expression* parse_assign_expression(Parser* p, Expression* left)
{
    TokenType assign_type;
    Position pos;
    Operator op;
    Expression* source;
    Expression* left_copy;
    Expression* new_source;
    Expression* res;

    source = NULL;
    assign_type = p->lexer.cur_token.type;
    lexer_next_token(&p->lexer);
    source = parse_expression(p, PRECEDENCE_LOWEST);
    if(!source)
    {
        goto err;
    }
    switch(assign_type)
    {
        case TOKEN_PLUS_ASSIGN:
        case TOKEN_MINUS_ASSIGN:
        case TOKEN_SLASH_ASSIGN:
        case TOKEN_ASTERISK_ASSIGN:
        case TOKEN_PERCENT_ASSIGN:
        case TOKEN_BIT_AND_ASSIGN:
        case TOKEN_BIT_OR_ASSIGN:
        case TOKEN_BIT_XOR_ASSIGN:
        case TOKEN_LSHIFT_ASSIGN:
        case TOKEN_RSHIFT_ASSIGN:
            {
                op = token_to_operator(assign_type);
                left_copy = expression_copy(left);
                if(!left_copy)
                {
                    goto err;
                }
                pos = source->pos;
                new_source = expression_make_infix(p->alloc, op, left_copy, source);
                if(!new_source)
                {
                    expression_destroy(left_copy);
                    goto err;
                }
                new_source->pos = pos;
                source = new_source;
            }
            break;
        case TOKEN_ASSIGN:
            {
            }
            break;
        default:
            {
                APE_ASSERT(false);
            }
            break;
    }
    res = expression_make_assign(p->alloc, left, source, false);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    expression_destroy(source);
    return NULL;
}

Expression* parse_logical_expression(Parser* p, Expression* left)
{
    Operator op;
    Precedence prec;
    Expression* right;
    Expression* res;
    op = token_to_operator(p->lexer.cur_token.type);
    prec = get_precedence(p->lexer.cur_token.type);
    lexer_next_token(&p->lexer);
    right = parse_expression(p, prec);
    if(!right)
    {
        return NULL;
    }
    res = expression_make_logical(p->alloc, op, left, right);
    if(!res)
    {
        expression_destroy(right);
        return NULL;
    }
    return res;
}

Expression* parse_ternary_expression(Parser* p, Expression* left)
{
    Expression* if_true;
    Expression* if_false;
    Expression* res;
    lexer_next_token(&p->lexer);
    if_true = parse_expression(p, PRECEDENCE_LOWEST);
    if(!if_true)
    {
        return NULL;
    }
    if(!lexer_expect_current(&p->lexer, TOKEN_COLON))
    {
        expression_destroy(if_true);
        return NULL;
    }
    lexer_next_token(&p->lexer);
    if_false = parse_expression(p, PRECEDENCE_LOWEST);
    if(!if_false)
    {
        expression_destroy(if_true);
        return NULL;
    }
    res = expression_make_ternary(p->alloc, left, if_true, if_false);
    if(!res)
    {
        expression_destroy(if_true);
        expression_destroy(if_false);
        return NULL;
    }
    return res;
}

Expression* parse_incdec_prefix_expression(Parser* p)
{
    Expression* source;
    TokenType operation_type;
    Position pos;
    Operator op;
    Expression* dest;
    Expression* one_literal;
    Expression* dest_copy;
    Expression* operation;
    Expression* res;

    source = NULL;
    operation_type = p->lexer.cur_token.type;
    pos = p->lexer.cur_token.pos;
    lexer_next_token(&p->lexer);
    op = token_to_operator(operation_type);
    dest = parse_expression(p, PRECEDENCE_PREFIX);
    if(!dest)
    {
        goto err;
    }
    one_literal = expression_make_number_literal(p->alloc, 1);
    if(!one_literal)
    {
        expression_destroy(dest);
        goto err;
    }
    one_literal->pos = pos;
    dest_copy = expression_copy(dest);
    if(!dest_copy)
    {
        expression_destroy(one_literal);
        expression_destroy(dest);
        goto err;
    }
    operation = expression_make_infix(p->alloc, op, dest_copy, one_literal);
    if(!operation)
    {
        expression_destroy(dest_copy);
        expression_destroy(dest);
        expression_destroy(one_literal);
        goto err;
    }
    operation->pos = pos;

    res = expression_make_assign(p->alloc, dest, operation, false);
    if(!res)
    {
        expression_destroy(dest);
        expression_destroy(operation);
        goto err;
    }
    return res;
err:
    expression_destroy(source);
    return NULL;
}

Expression* parse_incdec_postfix_expression(Parser* p, Expression* left)
{
    Expression* source;
    TokenType operation_type;
    Position pos;
    Operator op;
    Expression* left_copy;
    Expression* one_literal;
    Expression* operation;
    Expression* res;
    source = NULL;
    operation_type = p->lexer.cur_token.type;
    pos = p->lexer.cur_token.pos;
    lexer_next_token(&p->lexer);
    op = token_to_operator(operation_type);
    left_copy = expression_copy(left);
    if(!left_copy)
    {
        goto err;
    }
    one_literal = expression_make_number_literal(p->alloc, 1);
    if(!one_literal)
    {
        expression_destroy(left_copy);
        goto err;
    }
    one_literal->pos = pos;
    operation = expression_make_infix(p->alloc, op, left_copy, one_literal);
    if(!operation)
    {
        expression_destroy(one_literal);
        expression_destroy(left_copy);
        goto err;
    }
    operation->pos = pos;
    res = expression_make_assign(p->alloc, left, operation, true);
    if(!res)
    {
        expression_destroy(operation);
        goto err;
    }

    return res;
err:
    expression_destroy(source);
    return NULL;
}


Expression* parse_dot_expression(Parser* p, Expression* left)
{
    lexer_next_token(&p->lexer);

    if(!lexer_expect_current(&p->lexer, TOKEN_IDENT))
    {
        return NULL;
    }

    char* str = token_duplicate_literal(p->alloc, &p->lexer.cur_token);
    Expression* index = expression_make_string_literal(p->alloc, str);
    if(!index)
    {
        allocator_free(p->alloc, str);
        return NULL;
    }
    index->pos = p->lexer.cur_token.pos;

    lexer_next_token(&p->lexer);

    Expression* res = expression_make_index(p->alloc, left, index);
    if(!res)
    {
        expression_destroy(index);
        return NULL;
    }
    return res;
}

static Precedence get_precedence(TokenType tk)
{
    switch(tk)
    {
        case TOKEN_EQ:
            return PRECEDENCE_EQUALS;
        case TOKEN_NOT_EQ:
            return PRECEDENCE_EQUALS;
        case TOKEN_LT:
            return PRECEDENCE_LESSGREATER;
        case TOKEN_LTE:
            return PRECEDENCE_LESSGREATER;
        case TOKEN_GT:
            return PRECEDENCE_LESSGREATER;
        case TOKEN_GTE:
            return PRECEDENCE_LESSGREATER;
        case TOKEN_PLUS:
            return PRECEDENCE_SUM;
        case TOKEN_MINUS:
            return PRECEDENCE_SUM;
        case TOKEN_SLASH:
            return PRECEDENCE_PRODUCT;
        case TOKEN_ASTERISK:
            return PRECEDENCE_PRODUCT;
        case TOKEN_PERCENT:
            return PRECEDENCE_PRODUCT;
        case TOKEN_LPAREN:
            return PRECEDENCE_POSTFIX;
        case TOKEN_LBRACKET:
            return PRECEDENCE_POSTFIX;
        case TOKEN_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_PLUS_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_MINUS_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_ASTERISK_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_SLASH_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_PERCENT_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_BIT_AND_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_BIT_OR_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_BIT_XOR_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_LSHIFT_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_RSHIFT_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_DOT:
            return PRECEDENCE_POSTFIX;
        case TOKEN_AND:
            return PRECEDENCE_LOGICAL_AND;
        case TOKEN_OR:
            return PRECEDENCE_LOGICAL_OR;
        case TOKEN_BIT_OR:
            return PRECEDENCE_BIT_OR;
        case TOKEN_BIT_XOR:
            return PRECEDENCE_BIT_XOR;
        case TOKEN_BIT_AND:
            return PRECEDENCE_BIT_AND;
        case TOKEN_LSHIFT:
            return PRECEDENCE_SHIFT;
        case TOKEN_RSHIFT:
            return PRECEDENCE_SHIFT;
        case TOKEN_QUESTION:
            return PRECEDENCE_TERNARY;
        case TOKEN_PLUS_PLUS:
            return PRECEDENCE_INCDEC;
        case TOKEN_MINUS_MINUS:
            return PRECEDENCE_INCDEC;
        default:
            return PRECEDENCE_LOWEST;
    }
}

static Operator token_to_operator(TokenType tk)
{
    switch(tk)
    {
        case TOKEN_ASSIGN:
            return OPERATOR_ASSIGN;
        case TOKEN_PLUS:
            return OPERATOR_PLUS;
        case TOKEN_MINUS:
            return OPERATOR_MINUS;
        case TOKEN_BANG:
            return OPERATOR_BANG;
        case TOKEN_ASTERISK:
            return OPERATOR_ASTERISK;
        case TOKEN_SLASH:
            return OPERATOR_SLASH;
        case TOKEN_LT:
            return OPERATOR_LT;
        case TOKEN_LTE:
            return OPERATOR_LTE;
        case TOKEN_GT:
            return OPERATOR_GT;
        case TOKEN_GTE:
            return OPERATOR_GTE;
        case TOKEN_EQ:
            return OPERATOR_EQ;
        case TOKEN_NOT_EQ:
            return OPERATOR_NOT_EQ;
        case TOKEN_PERCENT:
            return OPERATOR_MODULUS;
        case TOKEN_AND:
            return OPERATOR_LOGICAL_AND;
        case TOKEN_OR:
            return OPERATOR_LOGICAL_OR;
        case TOKEN_PLUS_ASSIGN:
            return OPERATOR_PLUS;
        case TOKEN_MINUS_ASSIGN:
            return OPERATOR_MINUS;
        case TOKEN_ASTERISK_ASSIGN:
            return OPERATOR_ASTERISK;
        case TOKEN_SLASH_ASSIGN:
            return OPERATOR_SLASH;
        case TOKEN_PERCENT_ASSIGN:
            return OPERATOR_MODULUS;
        case TOKEN_BIT_AND_ASSIGN:
            return OPERATOR_BIT_AND;
        case TOKEN_BIT_OR_ASSIGN:
            return OPERATOR_BIT_OR;
        case TOKEN_BIT_XOR_ASSIGN:
            return OPERATOR_BIT_XOR;
        case TOKEN_LSHIFT_ASSIGN:
            return OPERATOR_LSHIFT;
        case TOKEN_RSHIFT_ASSIGN:
            return OPERATOR_RSHIFT;
        case TOKEN_BIT_AND:
            return OPERATOR_BIT_AND;
        case TOKEN_BIT_OR:
            return OPERATOR_BIT_OR;
        case TOKEN_BIT_XOR:
            return OPERATOR_BIT_XOR;
        case TOKEN_LSHIFT:
            return OPERATOR_LSHIFT;
        case TOKEN_RSHIFT:
            return OPERATOR_RSHIFT;
        case TOKEN_PLUS_PLUS:
            return OPERATOR_PLUS;
        case TOKEN_MINUS_MINUS:
            return OPERATOR_MINUS;
        default:
        {
            APE_ASSERT(false);
            return OPERATOR_NONE;
        }
    }
}

static char escape_char(const char c)
{
    switch(c)
    {
        case '\"':
            return '\"';
        case '\\':
            return '\\';
        case '/':
            return '/';
        case 'b':
            return '\b';
        case 'f':
            return '\f';
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        case '0':
            return '\0';
        default:
            return c;
    }
}

static char* process_and_copy_string(Allocator* alloc, const char* input, size_t len)
{
    size_t in_i;
    size_t out_i;
    char* output;
    output = (char*)allocator_malloc(alloc, len + 1);
    if(!output)
    {
        return NULL;
    }
    in_i = 0;
    out_i = 0;
    while(input[in_i] != '\0' && in_i < len)
    {
        if(input[in_i] == '\\')
        {
            in_i++;
            output[out_i] = escape_char(input[in_i]);
            if(output[out_i] < 0)
            {
                goto error;
            }
        }
        else
        {
            output[out_i] = input[in_i];
        }
        out_i++;
        in_i++;
    }
    output[out_i] = '\0';
    return output;
error:
    allocator_free(alloc, output);
    return NULL;
}

static Expression* wrap_expression_in_function_call(Allocator* alloc, Expression* expr, const char* function_name)
{
    Token fn_token;
    token_init(&fn_token, TOKEN_IDENT, function_name, (int)strlen(function_name));
    fn_token.pos = expr->pos;

    Ident* ident = ident_make(alloc, fn_token);
    if(!ident)
    {
        return NULL;
    }
    ident->pos = fn_token.pos;

    Expression* function_ident_expr = expression_make_ident(alloc, ident);
    ;
    if(!function_ident_expr)
    {
        ident_destroy(ident);
        return NULL;
    }
    function_ident_expr->pos = expr->pos;
    ident = NULL;

    PtrArray* args = PtrArray::make(alloc);
    if(!args)
    {
        expression_destroy(function_ident_expr);
        return NULL;
    }

    bool ok = args->add(expr);
    if(!ok)
    {
        args->destroy();
        expression_destroy(function_ident_expr);
        return NULL;
    }

    Expression* call_expr = expression_make_call(alloc, function_ident_expr, args);
    if(!call_expr)
    {
        args->destroy();
        expression_destroy(function_ident_expr);
        return NULL;
    }
    call_expr->pos = expr->pos;

    return call_expr;
}

template<typename... ArgsT>
int emit(Compiler *comp, opcode_t op, int operands_count, ArgsT&&... operands);
static const Symbol *define_symbol(Compiler *comp, Position pos, const char *name, bool assignable, bool can_shadow);
static void compiler_set_symbol_table(Compiler *comp, SymbolTable *table);
static CompilationScope *get_compilation_scope(Compiler *comp);
static bool push_compilation_scope(Compiler *comp);
static void pop_compilation_scope(Compiler *comp);
static bool push_symbol_table(Compiler *comp, int global_offset);
static void pop_symbol_table(Compiler *comp);
static opcode_t get_last_opcode(Compiler *comp);
static bool compile_code(Compiler *comp, const char *code);
static bool compile_statements(Compiler *comp, PtrArray *statements);
static bool import_module(Compiler *comp, const Expression *import_stmt);
static bool compile_statement(Compiler *comp, const Expression *stmt);
static bool compile_expression(Compiler *comp, Expression *expr);
static bool compile_code_block(Compiler *comp, const Codeblock *block);
static int add_constant(Compiler *comp, Object obj);
static void change_uint16_operand(Compiler *comp, int ip, uint16_t operand);
static bool last_opcode_is(Compiler *comp, opcode_t op);
static bool read_symbol(Compiler *comp, const Symbol *symbol);
static bool write_symbol(Compiler *comp, const Symbol *symbol, bool define);
static bool push_break_ip(Compiler *comp, int ip);
static void pop_break_ip(Compiler *comp);
static int get_break_ip(Compiler *comp);
static bool push_continue_ip(Compiler *comp, int ip);
static void pop_continue_ip(Compiler *comp);
static int get_continue_ip(Compiler *comp);
static int get_ip(Compiler *comp);
static Array *get_src_positions(Compiler *comp);
static Array *get_bytecode(Compiler *comp);
static FileScope *file_scope_make(Compiler *comp, CompiledFile *file);
static void file_scope_destroy(FileScope *scope);
static bool push_file_scope(Compiler *comp, const char *filepath);
static void pop_file_scope(Compiler *comp);
static void set_compilation_scope(Compiler *comp, CompilationScope *scope);


static const Symbol* define_symbol(Compiler* comp, Position pos, const char* name, bool assignable, bool can_shadow)
{
    SymbolTable* symbol_table;
    const Symbol* current_symbol;
    const Symbol* symbol;
    symbol_table = compiler_get_symbol_table(comp);
    if(!can_shadow && !symbol_table_is_top_global_scope(symbol_table))
    {
        current_symbol = symbol_table_resolve(symbol_table, name);
        if(current_symbol)
        {
            errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, pos, "Symbol \"%s\" is already defined", name);
            return NULL;
        }
    }
    symbol = symbol_table_define(symbol_table, name, assignable);
    if(!symbol)
    {
        errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, pos, "Cannot define symbol \"%s\"", name);
        return NULL;
    }
    return symbol;
}


Compiler*
compiler_make(Allocator* alloc, const Config* config, GCMemory* mem, ErrorList* errors, PtrArray * files, GlobalStore* global_store)
{
    bool ok;
    Compiler* comp;
    comp = (Compiler*)allocator_malloc(alloc, sizeof(Compiler));
    if(!comp)
    {
        return NULL;
    }
    ok = compiler_init(comp, alloc, config, mem, errors, files, global_store);
    if(!ok)
    {
        allocator_free(alloc, comp);
        return NULL;
    }
    return comp;
}


void compiler_destroy(Compiler* comp)
{
    Allocator* alloc;
    if(!comp)
    {
        return;
    }
    alloc = comp->alloc;
    compiler_deinit(comp);
    allocator_free(alloc, comp);
}

CompilationResult* compiler_compile(Compiler* comp, const char* code)
{
    bool ok;
    Compiler comp_shallow_copy;
    CompilationScope* compilation_scope;
    CompilationResult* res;
    compilation_scope = get_compilation_scope(comp);
    APE_ASSERT(comp->src_positions_stack->count() == 0);
    APE_ASSERT(compilation_scope->bytecode->count() == 0);
    APE_ASSERT(compilation_scope->break_ip_stack->count() == 0);
    APE_ASSERT(compilation_scope->continue_ip_stack->count() == 0);
    comp->src_positions_stack->clear();
    compilation_scope->bytecode->clear();
    compilation_scope->src_positions->clear();
    compilation_scope->break_ip_stack->clear();
    compilation_scope->continue_ip_stack->clear();
    ok = compiler_init_shallow_copy(&comp_shallow_copy, comp);
    if(!ok)
    {
        return NULL;
    }
    ok = compile_code(comp, code);
    if(!ok)
    {
        goto err;
    }
    compilation_scope = get_compilation_scope(comp);// might've changed
    APE_ASSERT(compilation_scope->outer == NULL);
    compilation_scope = get_compilation_scope(comp);
    res = compilation_scope_orphan_result(compilation_scope);
    if(!res)
    {
        goto err;
    }
    compiler_deinit(&comp_shallow_copy);
    return res;
err:
    compiler_deinit(comp);
    *comp = comp_shallow_copy;
    return NULL;
}

CompilationResult* compiler_compile_file(Compiler* comp, const char* path)
{
    bool ok;
    char* code;
    CompiledFile* file;
    CompilationResult* res;
    FileScope* file_scope;
    CompiledFile* prev_file;
    code = NULL;
    file = NULL;
    res = NULL;
    if(!comp->config->fileio.read_file.read_file)
    {// todo: read code function
        errors_add_error(comp->errors, APE_ERROR_COMPILATION, src_pos_invalid, "File read function not configured");
        goto err;
    }
    code = comp->config->fileio.read_file.read_file(comp->config->fileio.read_file.context, path);
    if(!code)
    {
        errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, src_pos_invalid, "Reading file \"%s\" failed", path);
        goto err;
    }
    file = compiled_file_make(comp->alloc, path);
    if(!file)
    {
        goto err;
    }
    ok = comp->files->add(file);
    if(!ok)
    {
        compiled_file_destroy(file);
        goto err;
    }
    APE_ASSERT(comp->file_scopes->count() == 1);
    file_scope = (FileScope*)comp->file_scopes->top();
    if(!file_scope)
    {
        goto err;
    }
    prev_file = file_scope->file;// todo: push file scope instead?
    file_scope->file = file;
    res = compiler_compile(comp, code);
    if(!res)
    {
        file_scope->file = prev_file;
        goto err;
    }
    file_scope->file = prev_file;
    allocator_free(comp->alloc, code);
    return res;
err:
    allocator_free(comp->alloc, code);
    return NULL;
}

SymbolTable* compiler_get_symbol_table(Compiler* comp)
{
    FileScope* file_scope;
    file_scope = (FileScope*)comp->file_scopes->top();
    if(!file_scope)
    {
        APE_ASSERT(false);
        return NULL;
    }
    return file_scope->symbol_table;
}

static void compiler_set_symbol_table(Compiler* comp, SymbolTable* table)
{
    FileScope* file_scope;
    file_scope = (FileScope*)comp->file_scopes->top();
    if(!file_scope)
    {
        APE_ASSERT(false);
        return;
    }
    file_scope->symbol_table = table;
}

Array* compiler_get_constants(const Compiler* comp)
{
    return comp->constants;
}

// INTERNAL
bool compiler_init(Compiler* comp,
                          Allocator* alloc,
                          const Config* config,
                          GCMemory* mem,
                          ErrorList* errors,
                          PtrArray * files,
                          GlobalStore* global_store)
{
    bool ok;
    memset(comp, 0, sizeof(Compiler));
    comp->alloc = alloc;
    comp->config = config;
    comp->mem = mem;
    comp->errors = errors;
    comp->files = files;
    comp->global_store = global_store;
    comp->file_scopes = PtrArray::make(alloc);
    if(!comp->file_scopes)
    {
        goto err;
    }
    comp->constants = Array::make<Object>(alloc);
    if(!comp->constants)
    {
        goto err;
    }
    comp->src_positions_stack = Array::make<Position>(alloc);
    if(!comp->src_positions_stack)
    {
        goto err;
    }
    comp->modules = Dictionary::make(alloc, module_copy, module_destroy);
    if(!comp->modules)
    {
        goto err;
    }
    ok = push_compilation_scope(comp);
    if(!ok)
    {
        goto err;
    }
    ok = push_file_scope(comp, "none");
    if(!ok)
    {
        goto err;
    }
    comp->string_constants_positions = Dictionary::make(comp->alloc, NULL, NULL);
    if(!comp->string_constants_positions)
    {
        goto err;
    }

    return true;
err:
    compiler_deinit(comp);
    return false;
}

void compiler_deinit(Compiler* comp)
{
    int i;
    int* val;
    if(!comp)
    {
        return;
    }
    for(i = 0; i < comp->string_constants_positions->count(); i++)
    {
        val = (int*)comp->string_constants_positions->getValueAt(i);
        allocator_free(comp->alloc, val);
    }
    comp->string_constants_positions->destroy();
    while(comp->file_scopes->count() > 0)
    {
        pop_file_scope(comp);
    }
    while(get_compilation_scope(comp))
    {
        pop_compilation_scope(comp);
    }
    comp->modules->destroyWithItems();
    Array::destroy(comp->src_positions_stack);
    Array::destroy(comp->constants);
    comp->file_scopes->destroy();
    memset(comp, 0, sizeof(Compiler));
}

bool compiler_init_shallow_copy(Compiler* copy, Compiler* src)
{
    int i;
    bool ok;
    int* val;
    int* val_copy;
    const char* key;
    const char* loaded_name;
    char* loaded_name_copy;
    SymbolTable* src_st;
    SymbolTable* src_st_copy;
    SymbolTable* copy_st;
    Dictionary* modules_copy;
    Array* constants_copy;
    FileScope* src_file_scope;
    FileScope* copy_file_scope;
    PtrArray* src_loaded_module_names;
    PtrArray* copy_loaded_module_names;

    ok = compiler_init(copy, src->alloc, src->config, src->mem, src->errors, src->files, src->global_store);
    if(!ok)
    {
        return false;
    }
    src_st = compiler_get_symbol_table(src);
    APE_ASSERT(src->file_scopes->count() == 1);
    APE_ASSERT(src_st->outer == NULL);
    src_st_copy = symbol_table_copy(src_st);
    if(!src_st_copy)
    {
        goto err;
    }
    copy_st = compiler_get_symbol_table(copy);
    symbol_table_destroy(copy_st);
    copy_st = NULL;
    compiler_set_symbol_table(copy, src_st_copy);
    modules_copy = src->modules->copyWithItems();
    if(!modules_copy)
    {
        goto err;
    }
    copy->modules->destroyWithItems();
    copy->modules = modules_copy;
    constants_copy = Array::copy(src->constants);
    if(!constants_copy)
    {
        goto err;
    }
    Array::destroy(copy->constants);
    copy->constants = constants_copy;
    for(i = 0; i < src->string_constants_positions->count(); i++)
    {
        key = (const char*)src->string_constants_positions->getKeyAt(i);
        val = (int*)src->string_constants_positions->getValueAt( i);
        val_copy = (int*)allocator_malloc(src->alloc, sizeof(int));
        if(!val_copy)
        {
            goto err;
        }
        *val_copy = *val;
        ok = copy->string_constants_positions->set(key, val_copy);
        if(!ok)
        {
            allocator_free(src->alloc, val_copy);
            goto err;
        }
    }
    src_file_scope = (FileScope*)src->file_scopes->top();
    copy_file_scope = (FileScope*)copy->file_scopes->top();
    src_loaded_module_names = src_file_scope->loaded_module_names;
    copy_loaded_module_names = copy_file_scope->loaded_module_names;
    for(i = 0; i < src_loaded_module_names->count(); i++)
    {

        loaded_name = (const char*)src_loaded_module_names->get(i);
        loaded_name_copy = ape_strdup(copy->alloc, loaded_name);
        if(!loaded_name_copy)
        {
            goto err;
        }
        ok = copy_loaded_module_names->add(loaded_name_copy);
        if(!ok)
        {
            allocator_free(copy->alloc, loaded_name_copy);
            goto err;
        }
    }

    return true;
err:
    compiler_deinit(copy);
    return false;
}

template<typename... ArgsT>
int emit(Compiler *comp, opcode_t op, int operands_count, ArgsT&&... operargs)
{
    int i;
    int ip;
    int len;
    bool ok;
    Position* src_pos;
    CompilationScope* compilation_scope;
    uint64_t operands[] = { static_cast<uint64_t>(operargs)... };
    ip = get_ip(comp);
    len = code_make(op, operands_count, operands, get_bytecode(comp));
    if(len == 0)
    {
        return -1;
    }
    for(i = 0; i < len; i++)
    {
        src_pos = (Position*)comp->src_positions_stack->top();
        APE_ASSERT(src_pos->line >= 0);
        APE_ASSERT(src_pos->column >= 0);
        ok = get_src_positions(comp)->add(src_pos);
        if(!ok)
        {
            return -1;
        }
    }
    compilation_scope = get_compilation_scope(comp);
    compilation_scope->last_opcode = op;
    return ip;
}

static CompilationScope* get_compilation_scope(Compiler* comp)
{
    return comp->compilation_scope;
}

static bool push_compilation_scope(Compiler* comp)
{
    CompilationScope* current_scope;
    CompilationScope* new_scope;
    current_scope = get_compilation_scope(comp);
    new_scope = compilation_scope_make(comp->alloc, current_scope);
    if(!new_scope)
    {
        return false;
    }
    set_compilation_scope(comp, new_scope);
    return true;
}

static void pop_compilation_scope(Compiler* comp)
{
    CompilationScope* current_scope;
    current_scope = get_compilation_scope(comp);
    APE_ASSERT(current_scope);
    set_compilation_scope(comp, current_scope->outer);
    compilation_scope_destroy(current_scope);
}

static bool push_symbol_table(Compiler* comp, int global_offset)
{
    FileScope* file_scope;
    file_scope = (FileScope*)comp->file_scopes->top();
    if(!file_scope)
    {
        APE_ASSERT(false);
        return false;
    }
    SymbolTable* current_table = file_scope->symbol_table;
    file_scope->symbol_table = symbol_table_make(comp->alloc, current_table, comp->global_store, global_offset);
    if(!file_scope->symbol_table)
    {
        file_scope->symbol_table = current_table;
        return false;
    }
    return true;
}

static void pop_symbol_table(Compiler* comp)
{
    FileScope* file_scope;
    SymbolTable* current_table;
    file_scope = (FileScope*)comp->file_scopes->top();
    if(!file_scope)
    {
        APE_ASSERT(false);
        return;
    }
    current_table = file_scope->symbol_table;
    if(!current_table)
    {
        APE_ASSERT(false);
        return;
    }
    file_scope->symbol_table = current_table->outer;
    symbol_table_destroy(current_table);
}

static opcode_t get_last_opcode(Compiler* comp)
{
    CompilationScope* current_scope;
    current_scope = get_compilation_scope(comp);
    return current_scope->last_opcode;
}

static bool compile_code(Compiler* comp, const char* code)
{
    bool ok;
    FileScope* file_scope;
    PtrArray* statements;
    file_scope = (FileScope*)comp->file_scopes->top();
    APE_ASSERT(file_scope);
    statements = parser_parse_all(file_scope->parser, code, file_scope->file);
    if(!statements)
    {
        // errors are added by parser
        return false;
    }
    ok = compile_statements(comp, statements);
    statements->destroyWithItems(statement_destroy);

    // Left for debugging purposes
    //    if (ok) {
    //        StringBuffer *buf = strbuf_make(NULL);
    //        code_to_string(comp->compilation_scope->bytecode->data(),
    //                       comp->compilation_scope->src_positions->data(),
    //                       comp->compilation_scope->bytecode->count(), buf);
    //        puts(strbuf_get_string(buf));
    //        strbuf_destroy(buf);
    //    }

    return ok;
}

static bool compile_statements(Compiler* comp, PtrArray * statements)
{
    int i;
    bool ok;
    const Expression* stmt;
    ok = true;
    for(i = 0; i < statements->count(); i++)
    {
        stmt = (const Expression*)statements->get(i);
        ok = compile_statement(comp, stmt);
        if(!ok)
        {
            break;
        }
    }
    return ok;
}

static bool import_module(Compiler* comp, const Expression* import_stmt)
{
    // todo: split into smaller functions
    int i;
    bool ok;
    bool result;
    char* filepath;
    char* code;
    char* name_copy;
    const char* loaded_name;
    const char* module_path;
    const char* module_name;
    const char* filepath_non_canonicalised;
    FileScope* file_scope;
    StringBuffer* filepath_buf;
    SymbolTable* symbol_table;
    FileScope* fs;
    Module* module;
    SymbolTable* st;
    Symbol* symbol;
    result = false;
    filepath = NULL;
    code = NULL;
    file_scope = (FileScope*)comp->file_scopes->top();
    module_path = import_stmt->import.path;
    module_name = get_module_name(module_path);
    for(i = 0; i < file_scope->loaded_module_names->count(); i++)
    {
        loaded_name = (const char*)file_scope->loaded_module_names->get(i);
        if(kg_streq(loaded_name, module_name))
        {
            errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, import_stmt->pos, "Module \"%s\" was already imported", module_name);
            result = false;
            goto end;
        }
    }
    filepath_buf = strbuf_make(comp->alloc);
    if(!filepath_buf)
    {
        result = false;
        goto end;
    }
    if(kg_is_path_absolute(module_path))
    {
        strbuf_appendf(filepath_buf, "%s.ape", module_path);
    }
    else
    {
        strbuf_appendf(filepath_buf, "%s%s.ape", file_scope->file->dir_path, module_path);
    }
    if(strbuf_failed(filepath_buf))
    {
        strbuf_destroy(filepath_buf);
        result = false;
        goto end;
    }
    filepath_non_canonicalised = strbuf_get_string(filepath_buf);
    filepath = kg_canonicalise_path(comp->alloc, filepath_non_canonicalised);
    strbuf_destroy(filepath_buf);
    if(!filepath)
    {
        result = false;
        goto end;
    }
    symbol_table = compiler_get_symbol_table(comp);
    if(symbol_table->outer != NULL || symbol_table->block_scopes->count() > 1)
    {
        errors_add_error(comp->errors, APE_ERROR_COMPILATION, import_stmt->pos, "Modules can only be imported in global scope");
        result = false;
        goto end;
    }
    for(i = 0; i < comp->file_scopes->count(); i++)
    {
        fs = (FileScope*)comp->file_scopes->get(i);
        if(APE_STREQ(fs->file->path, filepath))
        {
            errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, import_stmt->pos, "Cyclic reference of file \"%s\"", filepath);
            result = false;
            goto end;
        }
    }
    module = (Module*)comp->modules->get(filepath);
    if(!module)
    {
        // todo: create new module function
        if(!comp->config->fileio.read_file.read_file)
        {
            errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, import_stmt->pos,
                              "Cannot import module \"%s\", file read function not configured", filepath);
            result = false;
            goto end;
        }
        code = comp->config->fileio.read_file.read_file(comp->config->fileio.read_file.context, filepath);
        if(!code)
        {
            errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, import_stmt->pos, "Reading module file \"%s\" failed", filepath);
            result = false;
            goto end;
        }
        module = module_make(comp->alloc, module_name);
        if(!module)
        {
            result = false;
            goto end;
        }
        ok = push_file_scope(comp, filepath);
        if(!ok)
        {
            module_destroy(module);
            result = false;
            goto end;
        }
        ok = compile_code(comp, code);
        if(!ok)
        {
            module_destroy(module);
            result = false;
            goto end;
        }
        st = compiler_get_symbol_table(comp);
        for(i = 0; i < symbol_table_get_module_global_symbol_count(st); i++)
        {
            symbol = (Symbol*)symbol_table_get_module_global_symbol_at(st, i);
            module_add_symbol(module, symbol);
        }
        pop_file_scope(comp);
        ok = comp->modules->set( filepath, module);
        if(!ok)
        {
            module_destroy(module);
            result = false;
            goto end;
        }
    }
    for(i = 0; i < module->symbols->count(); i++)
    {
        symbol = (Symbol*)module->symbols->get(i);
        ok = symbol_table_add_module_symbol(symbol_table, symbol);
        if(!ok)
        {
            result = false;
            goto end;
        }
    }
    name_copy = ape_strdup(comp->alloc, module_name);
    if(!name_copy)
    {
        result = false;
        goto end;
    }
    ok = file_scope->loaded_module_names->add(name_copy);
    if(!ok)
    {
        allocator_free(comp->alloc, name_copy);
        result = false;
        goto end;
    }
    result = true;
end:
    allocator_free(comp->alloc, filepath);
    allocator_free(comp->alloc, code);
    return result;
}

static bool compile_statement(Compiler* comp, const Expression* stmt)
{
    int i;
    int ip;
    int next_case_jump_ip;
    int after_alt_ip;
    int after_elif_ip;
    int* pos;
    int jump_to_end_ip;
    int before_test_ip;
    int after_test_ip;
    int jump_to_after_body_ip;
    int after_body_ip;
    bool ok;

    const WhileLoopStmt* loop;
    CompilationScope* compilation_scope;
    SymbolTable* symbol_table;
    const Symbol* symbol;
    const IfStmt* if_stmt;
    Array* jump_to_end_ips;
    IfCase* if_case;
    ok = false;
    ip = -1;
    ok = comp->src_positions_stack->push(&stmt->pos);
    if(!ok)
    {
        return false;
    }
    compilation_scope = get_compilation_scope(comp);
    symbol_table = compiler_get_symbol_table(comp);
    switch(stmt->type)
    {
        case EXPRESSION_EXPRESSION:
            {
                ok = compile_expression(comp, stmt->expression);
                if(!ok)
                {
                    return false;
                }
                ip = emit(comp, OPCODE_POP, 0);
                if(ip < 0)
                {
                    return false;
                }
            }
            break;

        case EXPRESSION_DEFINE:
            {
                ok = compile_expression(comp, stmt->define.value);
                if(!ok)
                {
                    return false;
                }
                symbol = define_symbol(comp, stmt->define.name->pos, stmt->define.name->value, stmt->define.assignable, false);
                if(!symbol)
                {
                    return false;
                }
                ok = write_symbol(comp, symbol, true);
                if(!ok)
                {
                    return false;
                }
            }
            break;

        case EXPRESSION_IF:
            {
                if_stmt = &stmt->if_statement;
                jump_to_end_ips = Array::make<int>(comp->alloc);
                if(!jump_to_end_ips)
                {
                    goto statement_if_error;
                }
                for(i = 0; i < if_stmt->cases->count(); i++)
                {
                    if_case = (IfCase*)if_stmt->cases->get(i);
                    ok = compile_expression(comp, if_case->test);
                    if(!ok)
                    {
                        goto statement_if_error;
                    }
                    next_case_jump_ip = emit(comp, OPCODE_JUMP_IF_FALSE, 1, (uint64_t)(0xbeef));
                    ok = compile_code_block(comp, if_case->consequence);
                    if(!ok)
                    {
                        goto statement_if_error;
                    }
                    // don't emit jump for the last statement
                    if(i < (if_stmt->cases->count() - 1) || if_stmt->alternative)
                    {

                        jump_to_end_ip = emit(comp, OPCODE_JUMP, 1, (uint64_t)(0xbeef));
                        ok = jump_to_end_ips->add(&jump_to_end_ip);
                        if(!ok)
                        {
                            goto statement_if_error;
                        }
                    }
                    after_elif_ip = get_ip(comp);
                    change_uint16_operand(comp, next_case_jump_ip + 1, after_elif_ip);
                }
                if(if_stmt->alternative)
                {
                    ok = compile_code_block(comp, if_stmt->alternative);
                    if(!ok)
                    {
                        goto statement_if_error;
                    }
                }
                after_alt_ip = get_ip(comp);
                for(i = 0; i < jump_to_end_ips->count(); i++)
                {
                    pos = (int*)jump_to_end_ips->get(i);
                    change_uint16_operand(comp, *pos + 1, after_alt_ip);
                }
                Array::destroy(jump_to_end_ips);

                break;
            statement_if_error:
                Array::destroy(jump_to_end_ips);
                return false;
            }
            break;
        case EXPRESSION_RETURN_VALUE:
            {
                if(compilation_scope->outer == NULL)
                {
                    errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "Nothing to return from");
                    return false;
                }
                ip = -1;
                if(stmt->return_value)
                {
                    ok = compile_expression(comp, stmt->return_value);
                    if(!ok)
                    {
                        return false;
                    }
                    ip = emit(comp, OPCODE_RETURN_VALUE, 0);
                }
                else
                {
                    ip = emit(comp, OPCODE_RETURN, 0);
                }
                if(ip < 0)
                {
                    return false;
                }
            }
            break;
        case EXPRESSION_WHILE_LOOP:
            {
                loop = &stmt->while_loop;
                before_test_ip = get_ip(comp);
                ok = compile_expression(comp, loop->test);
                if(!ok)
                {
                    return false;
                }
                after_test_ip = get_ip(comp);
                ip = emit(comp, OPCODE_JUMP_IF_TRUE, 1, (uint64_t)(after_test_ip + 6));
                if(ip < 0)
                {
                    return false;
                }
                jump_to_after_body_ip = emit(comp, OPCODE_JUMP, 1, (uint64_t)0xdead);
                if(jump_to_after_body_ip < 0)
                {
                    return false;
                }
                ok = push_continue_ip(comp, before_test_ip);
                if(!ok)
                {
                    return false;
                }
                ok = push_break_ip(comp, jump_to_after_body_ip);
                if(!ok)
                {
                    return false;
                }
                ok = compile_code_block(comp, loop->body);
                if(!ok)
                {
                    return false;
                }
                pop_break_ip(comp);
                pop_continue_ip(comp);
                ip = emit(comp, OPCODE_JUMP, 1, (uint64_t)before_test_ip);
                if(ip < 0)
                {
                    return false;
                }
                after_body_ip = get_ip(comp);
                change_uint16_operand(comp, jump_to_after_body_ip + 1, after_body_ip);
            }
            break;

        case EXPRESSION_BREAK:
        {
            int break_ip = get_break_ip(comp);
            if(break_ip < 0)
            {
                errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "Nothing to break from.");
                return false;
            }
            ip = emit(comp, OPCODE_JUMP, 1, (uint64_t)break_ip);
            if(ip < 0)
            {
                return false;
            }
            break;
        }
        case EXPRESSION_CONTINUE:
        {
            int continue_ip = get_continue_ip(comp);
            if(continue_ip < 0)
            {
                errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "Nothing to continue from.");
                return false;
            }
            ip = emit(comp, OPCODE_JUMP, 1, (uint64_t)continue_ip);
            if(ip < 0)
            {
                return false;
            }
            break;
        }
        case EXPRESSION_FOREACH:
        {
            const ForeachStmt* foreach = &stmt->foreach;
            ok = symbol_table_push_block_scope(symbol_table);
            if(!ok)
            {
                return false;
            }

            // Init
            const Symbol* index_symbol = define_symbol(comp, stmt->pos, "@i", false, true);
            if(!index_symbol)
            {
                return false;
            }

            ip = emit(comp, OPCODE_NUMBER, 1, (uint64_t)0);
            if(ip < 0)
            {
                return false;
            }

            ok = write_symbol(comp, index_symbol, true);
            if(!ok)
            {
                return false;
            }

            const Symbol* source_symbol = NULL;
            if(foreach->source->type == EXPRESSION_IDENT)
            {
                source_symbol = symbol_table_resolve(symbol_table, foreach->source->ident->value);
                if(!source_symbol)
                {
                    errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, foreach->source->pos,
                                      "Symbol \"%s\" could not be resolved", foreach->source->ident->value);
                    return false;
                }
            }
            else
            {
                ok = compile_expression(comp, foreach->source);
                if(!ok)
                {
                    return false;
                }
                source_symbol = define_symbol(comp, foreach->source->pos, "@source", false, true);
                if(!source_symbol)
                {
                    return false;
                }
                ok = write_symbol(comp, source_symbol, true);
                if(!ok)
                {
                    return false;
                }
            }

            // Update
            int jump_to_after_update_ip = emit(comp, OPCODE_JUMP, 1, (uint64_t)0xbeef);
            if(jump_to_after_update_ip < 0)
            {
                return false;
            }

            int update_ip = get_ip(comp);
            ok = read_symbol(comp, index_symbol);
            if(!ok)
            {
                return false;
            }

            ip = emit(comp, OPCODE_NUMBER, 1, (uint64_t)ape_double_to_uint64(1));
            if(ip < 0)
            {
                return false;
            }

            ip = emit(comp, OPCODE_ADD, 0);
            if(ip < 0)
            {
                return false;
            }

            ok = write_symbol(comp, index_symbol, false);
            if(!ok)
            {
                return false;
            }

            int after_update_ip = get_ip(comp);
            change_uint16_operand(comp, jump_to_after_update_ip + 1, after_update_ip);

            // Test
            ok = comp->src_positions_stack->push(&foreach->source->pos);
            if(!ok)
            {
                return false;
            }

            ok = read_symbol(comp, source_symbol);
            if(!ok)
            {
                return false;
            }

            ip = emit(comp, OPCODE_LEN, 0);
            if(ip < 0)
            {
                return false;
            }

            comp->src_positions_stack->pop(NULL);
            ok = read_symbol(comp, index_symbol);
            if(!ok)
            {
                return false;
            }

            ip = emit(comp, OPCODE_COMPARE, 0);
            if(ip < 0)
            {
                return false;
            }

            ip = emit(comp, OPCODE_EQUAL, 0);
            if(ip < 0)
            {
                return false;
            }

            int after_test_ip = get_ip(comp);
            ip = emit(comp, OPCODE_JUMP_IF_FALSE, 1, (uint64_t)(after_test_ip + 6));
            if(ip < 0)
            {
                return false;
            }

            int jump_to_after_body_ip = emit(comp, OPCODE_JUMP, 1, (uint64_t)0xdead);
            if(jump_to_after_body_ip < 0)
            {
                return false;
            }

            ok = read_symbol(comp, source_symbol);
            if(!ok)
            {
                return false;
            }

            ok = read_symbol(comp, index_symbol);
            if(!ok)
            {
                return false;
            }

            ip = emit(comp, OPCODE_GET_VALUE_AT, 0);
            if(ip < 0)
            {
                return false;
            }

            const Symbol* iter_symbol = define_symbol(comp, foreach->iterator->pos, foreach->iterator->value, false, false);
            if(!iter_symbol)
            {
                return false;
            }

            ok = write_symbol(comp, iter_symbol, true);
            if(!ok)
            {
                return false;
            }

            // Body
            ok = push_continue_ip(comp, update_ip);
            if(!ok)
            {
                return false;
            }

            ok = push_break_ip(comp, jump_to_after_body_ip);
            if(!ok)
            {
                return false;
            }

            ok = compile_code_block(comp, foreach->body);
            if(!ok)
            {
                return false;
            }

            pop_break_ip(comp);
            pop_continue_ip(comp);

            ip = emit(comp, OPCODE_JUMP, 1, (uint64_t)update_ip);
            if(ip < 0)
            {
                return false;
            }

            int after_body_ip = get_ip(comp);
            change_uint16_operand(comp, jump_to_after_body_ip + 1, after_body_ip);

            symbol_table_pop_block_scope(symbol_table);
            break;
        }
        case EXPRESSION_FOR_LOOP:
        {
            const ForLoopStmt* loop = &stmt->for_loop;

            ok = symbol_table_push_block_scope(symbol_table);
            if(!ok)
            {
                return false;
            }

            // Init
            int jump_to_after_update_ip = 0;
            bool ok = false;
            if(loop->init)
            {
                ok = compile_statement(comp, loop->init);
                if(!ok)
                {
                    return false;
                }
                jump_to_after_update_ip = emit(comp, OPCODE_JUMP, 1, (uint64_t)0xbeef);
                if(jump_to_after_update_ip < 0)
                {
                    return false;
                }
            }

            // Update
            int update_ip = get_ip(comp);
            if(loop->update)
            {
                ok = compile_expression(comp, loop->update);
                if(!ok)
                {
                    return false;
                }
                ip = emit(comp, OPCODE_POP, 0);
                if(ip < 0)
                {
                    return false;
                }
            }

            if(loop->init)
            {
                int after_update_ip = get_ip(comp);
                change_uint16_operand(comp, jump_to_after_update_ip + 1, after_update_ip);
            }

            // Test
            if(loop->test)
            {
                ok = compile_expression(comp, loop->test);
                if(!ok)
                {
                    return false;
                }
            }
            else
            {
                ip = emit(comp, OPCODE_TRUE, 0);
                if(ip < 0)
                {
                    return false;
                }
            }
            int after_test_ip = get_ip(comp);

            ip = emit(comp, OPCODE_JUMP_IF_TRUE, 1, (uint64_t)(after_test_ip + 6));
            if(ip < 0)
            {
                return false;
            }
            int jmp_to_after_body_ip = emit(comp, OPCODE_JUMP, 1, (uint64_t)0xdead);
            if(jmp_to_after_body_ip < 0)
            {
                return false;
            }

            // Body
            ok = push_continue_ip(comp, update_ip);
            if(!ok)
            {
                return false;
            }

            ok = push_break_ip(comp, jmp_to_after_body_ip);
            if(!ok)
            {
                return false;
            }

            ok = compile_code_block(comp, loop->body);
            if(!ok)
            {
                return false;
            }

            pop_break_ip(comp);
            pop_continue_ip(comp);

            ip = emit(comp, OPCODE_JUMP, 1, (uint64_t)update_ip);
            if(ip < 0)
            {
                return false;
            }

            int after_body_ip = get_ip(comp);
            change_uint16_operand(comp, jmp_to_after_body_ip + 1, after_body_ip);

            symbol_table_pop_block_scope(symbol_table);
            break;
        }
        case EXPRESSION_BLOCK:
        {
            ok = compile_code_block(comp, stmt->block);
            if(!ok)
            {
                return false;
            }
            break;
        }
        case EXPRESSION_IMPORT:
        {
            ok = import_module(comp, stmt);
            if(!ok)
            {
                return false;
            }
            break;
        }
        case EXPRESSION_RECOVER:
        {
            const RecoverStmt* recover = &stmt->recover;

            if(symbol_table_is_module_global_scope(symbol_table))
            {
                errors_add_error(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "Recover statement cannot be defined in global scope");
                return false;
            }

            if(!symbol_table_is_top_block_scope(symbol_table))
            {
                errors_add_error(comp->errors, APE_ERROR_COMPILATION, stmt->pos,
                                 "Recover statement cannot be defined within other statements");
                return false;
            }

            int recover_ip = emit(comp, OPCODE_SET_RECOVER, 1, (uint64_t)0xbeef);
            if(recover_ip < 0)
            {
                return false;
            }

            int jump_to_after_recover_ip = emit(comp, OPCODE_JUMP, 1, (uint64_t)0xbeef);
            if(jump_to_after_recover_ip < 0)
            {
                return false;
            }

            int after_jump_to_recover_ip = get_ip(comp);
            change_uint16_operand(comp, recover_ip + 1, after_jump_to_recover_ip);

            ok = symbol_table_push_block_scope(symbol_table);
            if(!ok)
            {
                return false;
            }

            const Symbol* error_symbol
            = define_symbol(comp, recover->error_ident->pos, recover->error_ident->value, false, false);
            if(!error_symbol)
            {
                return false;
            }

            ok = write_symbol(comp, error_symbol, true);
            if(!ok)
            {
                return false;
            }

            ok = compile_code_block(comp, recover->body);
            if(!ok)
            {
                return false;
            }

            if(!last_opcode_is(comp, OPCODE_RETURN) && !last_opcode_is(comp, OPCODE_RETURN_VALUE))
            {
                errors_add_error(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "Recover body must end with a return statement");
                return false;
            }

            symbol_table_pop_block_scope(symbol_table);

            int after_recover_ip = get_ip(comp);
            change_uint16_operand(comp, jump_to_after_recover_ip + 1, after_recover_ip);

            break;
        }
        default:
        {
            APE_ASSERT(false);
            return false;
        }
    }
    comp->src_positions_stack->pop(NULL);
    return true;
}

static bool compile_expression(Compiler* comp, Expression* expr)
{
    bool ok = false;
    int ip = -1;

    Expression* expr_optimised = optimise_expression(expr);
    if(expr_optimised)
    {
        expr = expr_optimised;
    }

    ok = comp->src_positions_stack->push(&expr->pos);
    if(!ok)
    {
        return false;
    }

    CompilationScope* compilation_scope = get_compilation_scope(comp);
    SymbolTable* symbol_table = compiler_get_symbol_table(comp);

    bool res = false;

    switch(expr->type)
    {
        case EXPRESSION_INFIX:
        {
            bool rearrange = false;

            opcode_t op = OPCODE_NONE;
            switch(expr->infix.op)
            {
                case OPERATOR_PLUS:
                    op = OPCODE_ADD;
                    break;
                case OPERATOR_MINUS:
                    op = OPCODE_SUB;
                    break;
                case OPERATOR_ASTERISK:
                    op = OPCODE_MUL;
                    break;
                case OPERATOR_SLASH:
                    op = OPCODE_DIV;
                    break;
                case OPERATOR_MODULUS:
                    op = OPCODE_MOD;
                    break;
                case OPERATOR_EQ:
                    op = OPCODE_EQUAL;
                    break;
                case OPERATOR_NOT_EQ:
                    op = OPCODE_NOT_EQUAL;
                    break;
                case OPERATOR_GT:
                    op = OPCODE_GREATER_THAN;
                    break;
                case OPERATOR_GTE:
                    op = OPCODE_GREATER_THAN_EQUAL;
                    break;
                case OPERATOR_LT:
                    op = OPCODE_GREATER_THAN;
                    rearrange = true;
                    break;
                case OPERATOR_LTE:
                    op = OPCODE_GREATER_THAN_EQUAL;
                    rearrange = true;
                    break;
                case OPERATOR_BIT_OR:
                    op = OPCODE_OR;
                    break;
                case OPERATOR_BIT_XOR:
                    op = OPCODE_XOR;
                    break;
                case OPERATOR_BIT_AND:
                    op = OPCODE_AND;
                    break;
                case OPERATOR_LSHIFT:
                    op = OPCODE_LSHIFT;
                    break;
                case OPERATOR_RSHIFT:
                    op = OPCODE_RSHIFT;
                    break;
                default:
                {
                    errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, expr->pos, "Unknown infix operator");
                    goto error;
                }
            }

            Expression* left = rearrange ? expr->infix.right : expr->infix.left;
            Expression* right = rearrange ? expr->infix.left : expr->infix.right;

            ok = compile_expression(comp, left);
            if(!ok)
            {
                goto error;
            }

            ok = compile_expression(comp, right);
            if(!ok)
            {
                goto error;
            }

            switch(expr->infix.op)
            {
                case OPERATOR_EQ:
                case OPERATOR_NOT_EQ:
                {
                    ip = emit(comp, OPCODE_COMPARE_EQ, 0);
                    if(ip < 0)
                    {
                        goto error;
                    }
                    break;
                }
                case OPERATOR_GT:
                case OPERATOR_GTE:
                case OPERATOR_LT:
                case OPERATOR_LTE:
                {
                    ip = emit(comp, OPCODE_COMPARE, 0);
                    if(ip < 0)
                    {
                        goto error;
                    }
                    break;
                }
                default:
                    break;
            }

            ip = emit(comp, op, 0);
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_NUMBER_LITERAL:
        {
            double number = expr->number_literal;
            ip = emit(comp, OPCODE_NUMBER, 1, (uint64_t)ape_double_to_uint64(number));
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_STRING_LITERAL:
        {
            int pos = 0;
            int* current_pos = (int*)comp->string_constants_positions->get(expr->string_literal);
            if(current_pos)
            {
                pos = *current_pos;
            }
            else
            {
                Object obj = object_make_string(comp->mem, expr->string_literal);
                if(object_is_null(obj))
                {
                    goto error;
                }

                pos = add_constant(comp, obj);
                if(pos < 0)
                {
                    goto error;
                }

                int* pos_val = (int*)allocator_malloc(comp->alloc, sizeof(int));
                if(!pos_val)
                {
                    goto error;
                }

                *pos_val = pos;
                ok = comp->string_constants_positions->set(expr->string_literal, pos_val);
                if(!ok)
                {
                    allocator_free(comp->alloc, pos_val);
                    goto error;
                }
            }

            ip = emit(comp, OPCODE_CONSTANT, 1, (uint64_t)pos);
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_NULL_LITERAL:
        {
            ip = emit(comp, OPCODE_NULL, 0);
            if(ip < 0)
            {
                goto error;
            }
            break;
        }
        case EXPRESSION_BOOL_LITERAL:
        {
            ip = emit(comp, expr->bool_literal ? OPCODE_TRUE : OPCODE_FALSE, 0);
            if(ip < 0)
            {
                goto error;
            }
            break;
        }
        case EXPRESSION_ARRAY_LITERAL:
        {
            for(int i = 0; i < expr->array->count(); i++)
            {
                ok = compile_expression(comp, (Expression*)expr->array->get(i));
                if(!ok)
                {
                    goto error;
                }
            }
            ip = emit(comp, OPCODE_ARRAY, 1, (uint64_t)expr->array->count());
            if(ip < 0)
            {
                goto error;
            }
            break;
        }
        case EXPRESSION_MAP_LITERAL:
        {
            const MapLiteral* map = &expr->map;
            int len = map->keys->count();
            ip = emit(comp, OPCODE_MAP_START, 1, (uint64_t)len);
            if(ip < 0)
            {
                goto error;
            }

            for(int i = 0; i < len; i++)
            {
                Expression* key = (Expression*)map->keys->get(i);
                Expression* val = (Expression*)map->values->get(i);

                ok = compile_expression(comp, key);
                if(!ok)
                {
                    goto error;
                }

                ok = compile_expression(comp, val);
                if(!ok)
                {
                    goto error;
                }
            }

            ip = emit(comp, OPCODE_MAP_END, 1, (uint64_t)len);
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_PREFIX:
        {
            ok = compile_expression(comp, expr->prefix.right);
            if(!ok)
            {
                goto error;
            }

            opcode_t op = OPCODE_NONE;
            switch(expr->prefix.op)
            {
                case OPERATOR_MINUS:
                    op = OPCODE_MINUS;
                    break;
                case OPERATOR_BANG:
                    op = OPCODE_BANG;
                    break;
                default:
                {
                    errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, expr->pos, "Unknown prefix operator.");
                    goto error;
                }
            }
            ip = emit(comp, op, 0);
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_IDENT:
        {
            const Ident* ident = expr->ident;
            const Symbol* symbol = symbol_table_resolve(symbol_table, ident->value);
            if(!symbol)
            {
                errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, ident->pos, "Symbol \"%s\" could not be resolved",
                                  ident->value);
                goto error;
            }
            ok = read_symbol(comp, symbol);
            if(!ok)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_INDEX:
        {
            const IndexExpr* index = &expr->index_expr;
            ok = compile_expression(comp, index->left);
            if(!ok)
            {
                goto error;
            }
            ok = compile_expression(comp, index->index);
            if(!ok)
            {
                goto error;
            }
            ip = emit(comp, OPCODE_GET_INDEX, 0);
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_FUNCTION_LITERAL:
        {
            const FnLiteral* fn = &expr->fn_literal;

            ok = push_compilation_scope(comp);
            if(!ok)
            {
                goto error;
            }

            ok = push_symbol_table(comp, 0);
            if(!ok)
            {
                goto error;
            }

            compilation_scope = get_compilation_scope(comp);
            symbol_table = compiler_get_symbol_table(comp);

            if(fn->name)
            {
                const Symbol* fn_symbol = symbol_table_define_function_name(symbol_table, fn->name, false);
                if(!fn_symbol)
                {
                    errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, expr->pos, "Cannot define symbol \"%s\"", fn->name);
                    goto error;
                }
            }

            const Symbol* this_symbol = symbol_table_define_this(symbol_table);
            if(!this_symbol)
            {
                errors_add_error(comp->errors, APE_ERROR_COMPILATION, expr->pos, "Cannot define \"this\" symbol");
                goto error;
            }

            for(int i = 0; i < expr->fn_literal.params->count(); i++)
            {
                Ident* param = (Ident*)expr->fn_literal.params->get(i);
                const Symbol* param_symbol = define_symbol(comp, param->pos, param->value, true, false);
                if(!param_symbol)
                {
                    goto error;
                }
            }

            ok = compile_statements(comp, fn->body->statements);
            if(!ok)
            {
                goto error;
            }

            if(!last_opcode_is(comp, OPCODE_RETURN_VALUE) && !last_opcode_is(comp, OPCODE_RETURN))
            {
                ip = emit(comp, OPCODE_RETURN, 0);
                if(ip < 0)
                {
                    goto error;
                }
            }

            PtrArray* free_symbols = symbol_table->free_symbols;
            symbol_table->free_symbols = NULL;// because it gets destroyed with compiler_pop_compilation_scope()

            int num_locals = symbol_table->max_num_definitions;

            CompilationResult* comp_res = compilation_scope_orphan_result(compilation_scope);
            if(!comp_res)
            {
                free_symbols->destroyWithItems(symbol_destroy);
                goto error;
            }
            pop_symbol_table(comp);
            pop_compilation_scope(comp);
            compilation_scope = get_compilation_scope(comp);
            symbol_table = compiler_get_symbol_table(comp);

            Object obj = object_make_function(comp->mem, fn->name, comp_res, true, num_locals, fn->params->count(), 0);

            if(object_is_null(obj))
            {
                free_symbols->destroyWithItems(symbol_destroy);
                compilation_result_destroy(comp_res);
                goto error;
            }

            for(int i = 0; i < free_symbols->count(); i++)
            {
                Symbol* symbol = (Symbol*)free_symbols->get(i);
                ok = read_symbol(comp, symbol);
                if(!ok)
                {
                    free_symbols->destroyWithItems(symbol_destroy);
                    goto error;
                }
            }

            int pos = add_constant(comp, obj);
            if(pos < 0)
            {
                free_symbols->destroyWithItems(symbol_destroy);
                goto error;
            }

            ip = emit(comp, OPCODE_FUNCTION, 2, (uint64_t)pos, (uint64_t)free_symbols->count());
            if(ip < 0)
            {
                free_symbols->destroyWithItems(symbol_destroy);
                goto error;
            }

            free_symbols->destroyWithItems(symbol_destroy);

            break;
        }
        case EXPRESSION_CALL:
        {
            ok = compile_expression(comp, expr->call_expr.function);
            if(!ok)
            {
                goto error;
            }

            for(int i = 0; i < expr->call_expr.args->count(); i++)
            {
                Expression* arg_expr = (Expression*)expr->call_expr.args->get(i);
                ok = compile_expression(comp, arg_expr);
                if(!ok)
                {
                    goto error;
                }
            }

            ip = emit(comp, OPCODE_CALL, 1, (uint64_t)expr->call_expr.args->count());
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_ASSIGN:
        {
            const AssignExpr* assign = &expr->assign;
            if(assign->dest->type != EXPRESSION_IDENT && assign->dest->type != EXPRESSION_INDEX)
            {
                errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, assign->dest->pos, "Expression is not assignable.");
                goto error;
            }

            if(assign->is_postfix)
            {
                ok = compile_expression(comp, assign->dest);
                if(!ok)
                {
                    goto error;
                }
            }

            ok = compile_expression(comp, assign->source);
            if(!ok)
            {
                goto error;
            }

            ip = emit(comp, OPCODE_DUP, 0);
            if(ip < 0)
            {
                goto error;
            }

            ok = comp->src_positions_stack->push(&assign->dest->pos);
            if(!ok)
            {
                goto error;
            }

            if(assign->dest->type == EXPRESSION_IDENT)
            {
                const Ident* ident = assign->dest->ident;
                const Symbol* symbol = symbol_table_resolve(symbol_table, ident->value);
                if(!symbol)
                {
                    //errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, assign->dest->pos, "Symbol \"%s\" could not be resolved", ident->value);
                    //goto error;
                    //symbol_table_define(SymbolTable* table, const char* name, bool assignable)
                    symbol = symbol_table_define(symbol_table, ident->value, true);
                }
                if(!symbol->assignable)
                {
                    errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, assign->dest->pos,
                                      "Symbol \"%s\" is not assignable", ident->value);
                    goto error;
                }
                ok = write_symbol(comp, symbol, false);
                if(!ok)
                {
                    goto error;
                }
            }
            else if(assign->dest->type == EXPRESSION_INDEX)
            {
                const IndexExpr* index = &assign->dest->index_expr;
                ok = compile_expression(comp, index->left);
                if(!ok)
                {
                    goto error;
                }
                ok = compile_expression(comp, index->index);
                if(!ok)
                {
                    goto error;
                }
                ip = emit(comp, OPCODE_SET_INDEX, 0);
                if(ip < 0)
                {
                    goto error;
                }
            }

            if(assign->is_postfix)
            {
                ip = emit(comp, OPCODE_POP, 0);
                if(ip < 0)
                {
                    goto error;
                }
            }

            comp->src_positions_stack->pop(NULL);
            break;
        }
        case EXPRESSION_LOGICAL:
        {
            const LogicalExpr* logi = &expr->logical;

            ok = compile_expression(comp, logi->left);
            if(!ok)
            {
                goto error;
            }

            ip = emit(comp, OPCODE_DUP, 0);
            if(ip < 0)
            {
                goto error;
            }

            int after_left_jump_ip = 0;
            if(logi->op == OPERATOR_LOGICAL_AND)
            {
                after_left_jump_ip = emit(comp, OPCODE_JUMP_IF_FALSE, 1, (uint64_t)0xbeef);
            }
            else
            {
                after_left_jump_ip = emit(comp, OPCODE_JUMP_IF_TRUE, 1, (uint64_t)0xbeef);
            }

            if(after_left_jump_ip < 0)
            {
                goto error;
            }

            ip = emit(comp, OPCODE_POP, 0);
            if(ip < 0)
            {
                goto error;
            }

            ok = compile_expression(comp, logi->right);
            if(!ok)
            {
                goto error;
            }

            int after_right_ip = get_ip(comp);
            change_uint16_operand(comp, after_left_jump_ip + 1, after_right_ip);

            break;
        }
        case EXPRESSION_TERNARY:
        {
            const TernaryExpr* ternary = &expr->ternary;

            ok = compile_expression(comp, ternary->test);
            if(!ok)
            {
                goto error;
            }

            int else_jump_ip = emit(comp, OPCODE_JUMP_IF_FALSE, 1, (uint64_t)0xbeef);

            ok = compile_expression(comp, ternary->if_true);
            if(!ok)
            {
                goto error;
            }

            int end_jump_ip = emit(comp, OPCODE_JUMP, 1, (uint64_t)0xbeef);

            int else_ip = get_ip(comp);
            change_uint16_operand(comp, else_jump_ip + 1, else_ip);

            ok = compile_expression(comp, ternary->if_false);
            if(!ok)
            {
                goto error;
            }

            int end_ip = get_ip(comp);
            change_uint16_operand(comp, end_jump_ip + 1, end_ip);

            break;
        }
        default:
        {
            APE_ASSERT(false);
            break;
        }
    }
    res = true;
    goto end;
error:
    res = false;
end:
    comp->src_positions_stack->pop(NULL);
    expression_destroy(expr_optimised);
    return res;
}

static bool compile_code_block(Compiler* comp, const Codeblock* block)
{
    SymbolTable* symbol_table = compiler_get_symbol_table(comp);
    if(!symbol_table)
    {
        return false;
    }

    bool ok = symbol_table_push_block_scope(symbol_table);
    if(!ok)
    {
        return false;
    }

    if(block->statements->count() == 0)
    {
        int ip = emit(comp, OPCODE_NULL, 0);
        if(ip < 0)
        {
            return false;
        }
        ip = emit(comp, OPCODE_POP, 0);
        if(ip < 0)
        {
            return false;
        }
    }

    for(int i = 0; i < block->statements->count(); i++)
    {
        const Expression* stmt = (Expression*)block->statements->get(i);
        bool ok = compile_statement(comp, stmt);
        if(!ok)
        {
            return false;
        }
    }
    symbol_table_pop_block_scope(symbol_table);
    return true;
}

static int add_constant(Compiler* comp, Object obj)
{
    bool ok = comp->constants->add(&obj);
    if(!ok)
    {
        return -1;
    }
    int pos = comp->constants->count() - 1;
    return pos;
}

static void change_uint16_operand(Compiler* comp, int ip, uint16_t operand)
{
    Array* bytecode = get_bytecode(comp);
    if((ip + 1) >= bytecode->count())
    {
        APE_ASSERT(false);
        return;
    }
    uint8_t hi = (uint8_t)(operand >> 8);
    bytecode->set(ip, &hi);
    uint8_t lo = (uint8_t)(operand);
    bytecode->set(ip + 1, &lo);
}

static bool last_opcode_is(Compiler* comp, opcode_t op)
{
    opcode_t last_opcode = get_last_opcode(comp);
    return last_opcode == op;
}

static bool read_symbol(Compiler* comp, const Symbol* symbol)
{
    int ip = -1;
    if(symbol->type == SYMBOL_MODULE_GLOBAL)
    {
        ip = emit(comp, OPCODE_GET_MODULE_GLOBAL, 1, (uint64_t)(symbol->index));
    }
    else if(symbol->type == SYMBOL_APE_GLOBAL)
    {
        ip = emit(comp, OPCODE_GET_APE_GLOBAL, 1, (uint64_t)(symbol->index));
    }
    else if(symbol->type == SYMBOL_LOCAL)
    {
        ip = emit(comp, OPCODE_GET_LOCAL, 1, (uint64_t)(symbol->index));
    }
    else if(symbol->type == SYMBOL_FREE)
    {
        ip = emit(comp, OPCODE_GET_FREE, 1, (uint64_t)(symbol->index));
    }
    else if(symbol->type == SYMBOL_FUNCTION)
    {
        ip = emit(comp, OPCODE_CURRENT_FUNCTION, 0);
    }
    else if(symbol->type == SYMBOL_THIS)
    {
        ip = emit(comp, OPCODE_GET_THIS, 0);
    }
    return ip >= 0;
}

static bool write_symbol(Compiler* comp, const Symbol* symbol, bool define)
{
    int ip = -1;
    if(symbol->type == SYMBOL_MODULE_GLOBAL)
    {
        if(define)
        {
            ip = emit(comp, OPCODE_DEFINE_MODULE_GLOBAL, 1, (uint64_t)(symbol->index));
        }
        else
        {
            ip = emit(comp, OPCODE_SET_MODULE_GLOBAL, 1, (uint64_t)(symbol->index));
        }
    }
    else if(symbol->type == SYMBOL_LOCAL)
    {
        if(define)
        {
            ip = emit(comp, OPCODE_DEFINE_LOCAL, 1, (uint64_t)(symbol->index));
        }
        else
        {
            ip = emit(comp, OPCODE_SET_LOCAL, 1, (uint64_t)(symbol->index));
        }
    }
    else if(symbol->type == SYMBOL_FREE)
    {
        ip = emit(comp, OPCODE_SET_FREE, 1, (uint64_t)(symbol->index));
    }
    return ip >= 0;
}

static bool push_break_ip(Compiler* comp, int ip)
{
    CompilationScope* comp_scope = get_compilation_scope(comp);
    return comp_scope->break_ip_stack->push(&ip);
}

static void pop_break_ip(Compiler* comp)
{
    CompilationScope* comp_scope = get_compilation_scope(comp);
    if(comp_scope->break_ip_stack->count() == 0)
    {
        APE_ASSERT(false);
        return;
    }
    comp_scope->break_ip_stack->pop(NULL);
}

static int get_break_ip(Compiler* comp)
{
    CompilationScope* comp_scope = get_compilation_scope(comp);
    if(comp_scope->break_ip_stack->count() == 0)
    {
        return -1;
    }
    int* res = (int*)comp_scope->break_ip_stack->top();
    return *res;
}

static bool push_continue_ip(Compiler* comp, int ip)
{
    CompilationScope* comp_scope = get_compilation_scope(comp);
    return comp_scope->continue_ip_stack->push(&ip);
}

static void pop_continue_ip(Compiler* comp)
{
    CompilationScope* comp_scope = get_compilation_scope(comp);
    if(comp_scope->continue_ip_stack->count() == 0)
    {
        APE_ASSERT(false);
        return;
    }
    comp_scope->continue_ip_stack->pop(NULL);
}

static int get_continue_ip(Compiler* comp)
{
    CompilationScope* comp_scope = get_compilation_scope(comp);
    if(comp_scope->continue_ip_stack->count() == 0)
    {
        APE_ASSERT(false);
        return -1;
    }
    int* res = (int*)comp_scope->continue_ip_stack->top();
    return *res;
}

static int get_ip(Compiler* comp)
{
    CompilationScope* compilation_scope = get_compilation_scope(comp);
    return compilation_scope->bytecode->count();
}

static Array * get_src_positions(Compiler* comp)
{
    CompilationScope* compilation_scope = get_compilation_scope(comp);
    return compilation_scope->src_positions;
}

static Array * get_bytecode(Compiler* comp)
{
    CompilationScope* compilation_scope = get_compilation_scope(comp);
    return compilation_scope->bytecode;
}

static FileScope* file_scope_make(Compiler* comp, CompiledFile* file)
{
    FileScope* file_scope = (FileScope*)allocator_malloc(comp->alloc, sizeof(FileScope));
    if(!file_scope)
    {
        return NULL;
    }
    memset(file_scope, 0, sizeof(FileScope));
    file_scope->alloc = comp->alloc;
    file_scope->parser = parser_make(comp->alloc, comp->config, comp->errors);
    if(!file_scope->parser)
    {
        goto err;
    }
    file_scope->symbol_table = NULL;
    file_scope->file = file;
    file_scope->loaded_module_names = PtrArray::make(comp->alloc);
    if(!file_scope->loaded_module_names)
    {
        goto err;
    }
    return file_scope;
err:
    file_scope_destroy(file_scope);
    return NULL;
}

static void file_scope_destroy(FileScope* scope)
{
    for(int i = 0; i < scope->loaded_module_names->count(); i++)
    {
        void* name = scope->loaded_module_names->get(i);
        allocator_free(scope->alloc, name);
    }
    scope->loaded_module_names->destroy();
    parser_destroy(scope->parser);
    allocator_free(scope->alloc, scope);
}

static bool push_file_scope(Compiler* comp, const char* filepath)
{
    SymbolTable* prev_st = NULL;
    if(comp->file_scopes->count() > 0)
    {
        prev_st = compiler_get_symbol_table(comp);
    }

    CompiledFile* file = compiled_file_make(comp->alloc, filepath);
    if(!file)
    {
        return false;
    }

    bool ok = comp->files->add(file);
    if(!ok)
    {
        compiled_file_destroy(file);
        return false;
    }

    FileScope* file_scope = file_scope_make(comp, file);
    if(!file_scope)
    {
        return false;
    }

    ok = comp->file_scopes->push(file_scope);
    if(!ok)
    {
        file_scope_destroy(file_scope);
        return false;
    }

    int global_offset = 0;
    if(prev_st)
    {
        BlockScope* prev_st_top_scope = symbol_table_get_block_scope(prev_st);
        global_offset = prev_st_top_scope->offset + prev_st_top_scope->num_definitions;
    }

    ok = push_symbol_table(comp, global_offset);
    if(!ok)
    {
        comp->file_scopes->pop();
        file_scope_destroy(file_scope);
        return false;
    }

    return true;
}

static void pop_file_scope(Compiler* comp)
{
    SymbolTable* popped_st = compiler_get_symbol_table(comp);
    BlockScope* popped_st_top_scope = symbol_table_get_block_scope(popped_st);
    int popped_num_defs = popped_st_top_scope->num_definitions;

    while(compiler_get_symbol_table(comp))
    {
        pop_symbol_table(comp);
    }
    FileScope* scope = (FileScope*)comp->file_scopes->top();
    if(!scope)
    {
        APE_ASSERT(false);
        return;
    }
    file_scope_destroy(scope);

    comp->file_scopes->pop();

    if(comp->file_scopes->count() > 0)
    {
        SymbolTable* current_st = compiler_get_symbol_table(comp);
        BlockScope* current_st_top_scope = symbol_table_get_block_scope(current_st);
        current_st_top_scope->num_definitions += popped_num_defs;
    }
}

static void set_compilation_scope(Compiler* comp, CompilationScope* scope)
{
    comp->compilation_scope = scope;
}


static const char* g_type_names[] = {
    "ILLEGAL",  "EOF",   "=",        "+=",     "-=",
    "*=",       "/=",    "%=",       "&=",     "|=",
    "^=",       "<<=",   ">>=",      "?",      "+",
    "++",       "-",     "--",       "!",      "*",
    "/",        "<",     "<=",       ">",      ">=",
    "==",       "!=",    "&&",       "||",     "&",
    "|",        "^",     "<<",       ">>",     ",",
    ";",        ":",     "(",        ")",      "{",
    "}",        "[",     "]",        ".",      "%",
    "FUNCTION", "CONST", "VAR",      "TRUE",   "FALSE",
    "IF",       "ELSE",  "RETURN",   "WHILE",  "BREAK",
    "FOR",      "IN",    "CONTINUE", "NULL",   "IMPORT",
    "RECOVER",  "IDENT", "NUMBER",   "STRING", "TEMPLATE_STRING",
};

static bool code_read_operands(OpcodeDefinition* def, uint8_t* instr, uint64_t out_operands[2])
{
    int offset = 0;
    for(int i = 0; i < def->num_operands; i++)
    {
        int operand_width = def->operand_widths[i];
        switch(operand_width)
        {
            case 1:
            {
                out_operands[i] = instr[offset];
                break;
            }
            case 2:
            {
                uint64_t operand = 0;
                operand = operand | (instr[offset] << 8);
                operand = operand | (instr[offset + 1]);
                out_operands[i] = operand;
                break;
            }
            case 4:
            {
                uint64_t operand = 0;
                operand = operand | (instr[offset + 0] << 24);
                operand = operand | (instr[offset + 1] << 16);
                operand = operand | (instr[offset + 2] << 8);
                operand = operand | (instr[offset + 3]);
                out_operands[i] = operand;
                break;
            }
            case 8:
            {
                uint64_t operand = 0;
                operand = operand | ((uint64_t)instr[offset + 0] << 56);
                operand = operand | ((uint64_t)instr[offset + 1] << 48);
                operand = operand | ((uint64_t)instr[offset + 2] << 40);
                operand = operand | ((uint64_t)instr[offset + 3] << 32);
                operand = operand | ((uint64_t)instr[offset + 4] << 24);
                operand = operand | ((uint64_t)instr[offset + 5] << 16);
                operand = operand | ((uint64_t)instr[offset + 6] << 8);
                operand = operand | ((uint64_t)instr[offset + 7]);
                out_operands[i] = operand;
                break;
            }
            default:
            {
                APE_ASSERT(false);
                return false;
            }
        }
        offset += operand_width;
    }
    return true;
    ;
}

char* statements_to_string(Allocator* alloc, PtrArray * statements)
{
    int i;
    int cn;
    const Expression* stmt;
    StringBuffer* buf;
    buf = strbuf_make(alloc);
    if(!buf)
    {
        return NULL;
    }
    cn = statements->count();
    for(i = 0; i < cn; i++)
    {
        stmt = (Expression*)statements->get(i);
        statement_to_string(stmt, buf);
        if(i < (cn - 1))
        {
            strbuf_append(buf, "\n");
        }
    }
    return strbuf_get_string_and_destroy(buf);
}

void statement_to_string(const Expression* stmt, StringBuffer* buf)
{
    int i;
    const DefineStmt* def_stmt;
    IfCase* if_case;
    IfCase* elif_case;
    switch(stmt->type)
    {
        case EXPRESSION_DEFINE:
        {
            def_stmt = &stmt->define;
            if(stmt->define.assignable)
            {
                strbuf_append(buf, "var ");
            }
            else
            {
                strbuf_append(buf, "const ");
            }
            strbuf_append(buf, def_stmt->name->value);
            strbuf_append(buf, " = ");

            if(def_stmt->value)
            {
                expression_to_string(def_stmt->value, buf);
            }

            break;
        }
        case EXPRESSION_IF:
        {
            if_case = (IfCase*)stmt->if_statement.cases->get(0);
            strbuf_append(buf, "if (");
            expression_to_string(if_case->test, buf);
            strbuf_append(buf, ") ");
            code_block_to_string(if_case->consequence, buf);
            for(i = 1; i < stmt->if_statement.cases->count(); i++)
            {
                elif_case = (IfCase*)stmt->if_statement.cases->get(i);
                strbuf_append(buf, " elif (");
                expression_to_string(elif_case->test, buf);
                strbuf_append(buf, ") ");
                code_block_to_string(elif_case->consequence, buf);
            }
            if(stmt->if_statement.alternative)
            {
                strbuf_append(buf, " else ");
                code_block_to_string(stmt->if_statement.alternative, buf);
            }
            break;
        }
        case EXPRESSION_RETURN_VALUE:
        {
            strbuf_append(buf, "return ");
            if(stmt->return_value)
            {
                expression_to_string(stmt->return_value, buf);
            }
            break;
        }
        case EXPRESSION_EXPRESSION:
        {
            if(stmt->expression)
            {
                expression_to_string(stmt->expression, buf);
            }
            break;
        }
        case EXPRESSION_WHILE_LOOP:
        {
            strbuf_append(buf, "while (");
            expression_to_string(stmt->while_loop.test, buf);
            strbuf_append(buf, ")");
            code_block_to_string(stmt->while_loop.body, buf);
            break;
        }
        case EXPRESSION_FOR_LOOP:
        {
            strbuf_append(buf, "for (");
            if(stmt->for_loop.init)
            {
                statement_to_string(stmt->for_loop.init, buf);
                strbuf_append(buf, " ");
            }
            else
            {
                strbuf_append(buf, ";");
            }
            if(stmt->for_loop.test)
            {
                expression_to_string(stmt->for_loop.test, buf);
                strbuf_append(buf, "; ");
            }
            else
            {
                strbuf_append(buf, ";");
            }
            if(stmt->for_loop.update)
            {
                expression_to_string(stmt->for_loop.test, buf);
            }
            strbuf_append(buf, ")");
            code_block_to_string(stmt->for_loop.body, buf);
            break;
        }
        case EXPRESSION_FOREACH:
        {
            strbuf_append(buf, "for (");
            strbuf_appendf(buf, "%s", stmt->foreach.iterator->value);
            strbuf_append(buf, " in ");
            expression_to_string(stmt->foreach.source, buf);
            strbuf_append(buf, ")");
            code_block_to_string(stmt->foreach.body, buf);
            break;
        }
        case EXPRESSION_BLOCK:
        {
            code_block_to_string(stmt->block, buf);
            break;
        }
        case EXPRESSION_BREAK:
        {
            strbuf_append(buf, "break");
            break;
        }
        case EXPRESSION_CONTINUE:
        {
            strbuf_append(buf, "continue");
            break;
        }
        case EXPRESSION_IMPORT:
        {
            strbuf_appendf(buf, "import \"%s\"", stmt->import.path);
            break;
        }
        case EXPRESSION_NONE:
        {
            strbuf_append(buf, "EXPRESSION_NONE");
            break;
        }
        case EXPRESSION_RECOVER:
        {
            strbuf_appendf(buf, "recover (%s)", stmt->recover.error_ident->value);
            code_block_to_string(stmt->recover.body, buf);
            break;
        }
    }
}

void expression_to_string(Expression* expr, StringBuffer* buf)
{
    int i;
    Expression* arr_expr;
    MapLiteral* map;
    switch(expr->type)
    {
        case EXPRESSION_IDENT:
        {
            strbuf_append(buf, expr->ident->value);
            break;
        }
        case EXPRESSION_NUMBER_LITERAL:
        {
            strbuf_appendf(buf, "%1.17g", expr->number_literal);
            break;
        }
        case EXPRESSION_BOOL_LITERAL:
        {
            strbuf_appendf(buf, "%s", expr->bool_literal ? "true" : "false");
            break;
        }
        case EXPRESSION_STRING_LITERAL:
        {
            strbuf_appendf(buf, "\"%s\"", expr->string_literal);
            break;
        }
        case EXPRESSION_NULL_LITERAL:
        {
            strbuf_append(buf, "null");
            break;
        }
        case EXPRESSION_ARRAY_LITERAL:
        {
            strbuf_append(buf, "[");
            for(i = 0; i < expr->array->count(); i++)
            {
                arr_expr = (Expression*)expr->array->get(i);
                expression_to_string(arr_expr, buf);
                if(i < (expr->array->count() - 1))
                {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, "]");
            break;
        }
        case EXPRESSION_MAP_LITERAL:
        {
            map = &expr->map;
            strbuf_append(buf, "{");
            for(i = 0; i < map->keys->count(); i++)
            {
                Expression* key_expr = (Expression*)map->keys->get(i);
                Expression* val_expr = (Expression*)map->values->get(i);
                expression_to_string(key_expr, buf);
                strbuf_append(buf, " : ");
                expression_to_string(val_expr, buf);
                if(i < (map->keys->count() - 1))
                {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, "}");
            break;
        }
        case EXPRESSION_PREFIX:
        {
            strbuf_append(buf, "(");
            strbuf_append(buf, operator_to_string(expr->infix.op));
            expression_to_string(expr->prefix.right, buf);
            strbuf_append(buf, ")");
            break;
        }
        case EXPRESSION_INFIX:
        {
            strbuf_append(buf, "(");
            expression_to_string(expr->infix.left, buf);
            strbuf_append(buf, " ");
            strbuf_append(buf, operator_to_string(expr->infix.op));
            strbuf_append(buf, " ");
            expression_to_string(expr->infix.right, buf);
            strbuf_append(buf, ")");
            break;
        }
        case EXPRESSION_FUNCTION_LITERAL:
        {
            FnLiteral* fn = &expr->fn_literal;

            strbuf_append(buf, "fn");

            strbuf_append(buf, "(");
            for(i = 0; i < fn->params->count(); i++)
            {
                Ident* param = (Ident*)fn->params->get(i);
                strbuf_append(buf, param->value);
                if(i < (fn->params->count() - 1))
                {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, ") ");

            code_block_to_string(fn->body, buf);

            break;
        }
        case EXPRESSION_CALL:
        {
            CallExpr* call_expr = &expr->call_expr;

            expression_to_string(call_expr->function, buf);

            strbuf_append(buf, "(");
            for(int i = 0; i < call_expr->args->count(); i++)
            {
                Expression* arg = (Expression*)call_expr->args->get(i);
                expression_to_string(arg, buf);
                if(i < (call_expr->args->count() - 1))
                {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, ")");

            break;
        }
        case EXPRESSION_INDEX:
        {
            strbuf_append(buf, "(");
            expression_to_string(expr->index_expr.left, buf);
            strbuf_append(buf, "[");
            expression_to_string(expr->index_expr.index, buf);
            strbuf_append(buf, "])");
            break;
        }
        case EXPRESSION_ASSIGN:
        {
            expression_to_string(expr->assign.dest, buf);
            strbuf_append(buf, " = ");
            expression_to_string(expr->assign.source, buf);
            break;
        }
        case EXPRESSION_LOGICAL:
        {
            expression_to_string(expr->logical.left, buf);
            strbuf_append(buf, " ");
            strbuf_append(buf, operator_to_string(expr->infix.op));
            strbuf_append(buf, " ");
            expression_to_string(expr->logical.right, buf);
            break;
        }
        case EXPRESSION_TERNARY:
        {
            expression_to_string(expr->ternary.test, buf);
            strbuf_append(buf, " ? ");
            expression_to_string(expr->ternary.if_true, buf);
            strbuf_append(buf, " : ");
            expression_to_string(expr->ternary.if_false, buf);
            break;
        }
        case EXPRESSION_NONE:
        {
            strbuf_append(buf, "EXPRESSION_NONE");
            break;
        }
    }
}

void code_block_to_string(const Codeblock* stmt, StringBuffer* buf)
{
    int i;
    Expression* istmt;
    strbuf_append(buf, "{ ");
    for(i = 0; i < stmt->statements->count(); i++)
    {
        istmt = (Expression*)stmt->statements->get(i);
        statement_to_string(istmt, buf);
        strbuf_append(buf, "\n");
    }
    strbuf_append(buf, " }");
}

const char* operator_to_string(Operator op)
{
    switch(op)
    {
        case OPERATOR_NONE:
            return "OPERATOR_NONE";
        case OPERATOR_ASSIGN:
            return "=";
        case OPERATOR_PLUS:
            return "+";
        case OPERATOR_MINUS:
            return "-";
        case OPERATOR_BANG:
            return "!";
        case OPERATOR_ASTERISK:
            return "*";
        case OPERATOR_SLASH:
            return "/";
        case OPERATOR_LT:
            return "<";
        case OPERATOR_GT:
            return ">";
        case OPERATOR_EQ:
            return "==";
        case OPERATOR_NOT_EQ:
            return "!=";
        case OPERATOR_MODULUS:
            return "%";
        case OPERATOR_LOGICAL_AND:
            return "&&";
        case OPERATOR_LOGICAL_OR:
            return "||";
        case OPERATOR_BIT_AND:
            return "&";
        case OPERATOR_BIT_OR:
            return "|";
        case OPERATOR_BIT_XOR:
            return "^";
        case OPERATOR_LSHIFT:
            return "<<";
        case OPERATOR_RSHIFT:
            return ">>";
        default:
            return "OPERATOR_UNKNOWN";
    }
}

const char* expression_type_to_string(Expr_type type)
{
    switch(type)
    {
        case EXPRESSION_NONE:
            return "NONE";
        case EXPRESSION_IDENT:
            return "IDENT";
        case EXPRESSION_NUMBER_LITERAL:
            return "INT_LITERAL";
        case EXPRESSION_BOOL_LITERAL:
            return "BOOL_LITERAL";
        case EXPRESSION_STRING_LITERAL:
            return "STRING_LITERAL";
        case EXPRESSION_ARRAY_LITERAL:
            return "ARRAY_LITERAL";
        case EXPRESSION_MAP_LITERAL:
            return "MAP_LITERAL";
        case EXPRESSION_PREFIX:
            return "PREFIX";
        case EXPRESSION_INFIX:
            return "INFIX";
        case EXPRESSION_FUNCTION_LITERAL:
            return "FN_LITERAL";
        case EXPRESSION_CALL:
            return "CALL";
        case EXPRESSION_INDEX:
            return "INDEX";
        case EXPRESSION_ASSIGN:
            return "ASSIGN";
        case EXPRESSION_LOGICAL:
            return "LOGICAL";
        case EXPRESSION_TERNARY:
            return "TERNARY";
        default:
            return "UNKNOWN";
    }
}

void code_to_string(uint8_t* code, Position* source_positions, size_t code_size, StringBuffer* res)
{
    unsigned pos = 0;
    while(pos < code_size)
    {
        uint8_t op = code[pos];
        OpcodeDefinition* def = opcode_lookup(op);
        APE_ASSERT(def);
        if(source_positions)
        {
            Position src_pos = source_positions[pos];
            strbuf_appendf(res, "%d:%-4d\t%04d\t%s", src_pos.line, src_pos.column, pos, def->name);
        }
        else
        {
            strbuf_appendf(res, "%04d %s", pos, def->name);
        }
        pos++;

        uint64_t operands[2];
        bool ok = code_read_operands(def, code + pos, operands);
        if(!ok)
        {
            return;
        }
        for(int i = 0; i < def->num_operands; i++)
        {
            if(op == OPCODE_NUMBER)
            {
                double val_double = ape_uint64_to_double(operands[i]);
                strbuf_appendf(res, " %1.17g", val_double);
            }
            else
            {
                strbuf_appendf(res, " %llu", (long long unsigned int)operands[i]);
            }
            pos += def->operand_widths[i];
        }
        strbuf_append(res, "\n");
    }
    return;
}

void object_to_string(Object obj, StringBuffer* buf, bool quote_str)
{
    int i;
    ObjectType type;
    type = object_get_type(obj);
    switch(type)
    {
        case APE_OBJECT_FREED:
        {
            strbuf_append(buf, "FREED");
            break;
        }
        case APE_OBJECT_NONE:
        {
            strbuf_append(buf, "NONE");
            break;
        }
        case APE_OBJECT_NUMBER:
        {
            double number = object_get_number(obj);
            strbuf_appendf(buf, "%1.10g", number);
            break;
        }
        case APE_OBJECT_BOOL:
        {
            strbuf_append(buf, object_get_bool(obj) ? "true" : "false");
            break;
        }
        case APE_OBJECT_STRING:
            {
                const char* string = object_get_string(obj);
                if(quote_str)
                {
                    strbuf_appendf(buf, "\"%s\"", string);
                }
                else
                {
                    strbuf_append(buf, string);
                }
            }
            break;
        case APE_OBJECT_NULL:
            {
                strbuf_append(buf, "null");
            }
            break;

        case APE_OBJECT_FUNCTION:
            {
                const FunctionObject* function = object_get_function(obj);
                strbuf_appendf(buf, "CompiledFunction: %s\n", object_get_function_name(obj));
                code_to_string(function->comp_result->bytecode, function->comp_result->src_positions, function->comp_result->m_count, buf);
            }
            break;

        case APE_OBJECT_ARRAY:
            {
                strbuf_append(buf, "[");
                for(i = 0; i < object_get_array_length(obj); i++)
                {
                    Object iobj = object_get_array_value(obj, i);
                    object_to_string(iobj, buf, true);
                    if(i < (object_get_array_length(obj) - 1))
                    {
                        strbuf_append(buf, ", ");
                    }
                }
                strbuf_append(buf, "]");
            }
            break;
        case APE_OBJECT_MAP:
            {
                strbuf_append(buf, "{");
                for(i = 0; i < object_get_map_length(obj); i++)
                {
                    Object key = object_get_map_key_at(obj, i);
                    Object val = object_get_map_value_at(obj, i);
                    object_to_string(key, buf, true);
                    strbuf_append(buf, ": ");
                    object_to_string(val, buf, true);
                    if(i < (object_get_map_length(obj) - 1))
                    {
                        strbuf_append(buf, ", ");
                    }
                }
                strbuf_append(buf, "}");
            }
            break;
        case APE_OBJECT_NATIVE_FUNCTION:
            {
                strbuf_append(buf, "NATIVE_FUNCTION");
            }
            break;
        case APE_OBJECT_EXTERNAL:
            {
                strbuf_append(buf, "EXTERNAL");
            }
            break;

        case APE_OBJECT_ERROR:
            {
                strbuf_appendf(buf, "ERROR: %s\n", object_get_error_message(obj));
                Traceback* traceback = object_get_error_traceback(obj);
                APE_ASSERT(traceback);
                if(traceback)
                {
                    strbuf_append(buf, "Traceback:\n");
                    traceback_to_string(traceback, buf);
                }
            }
            break;
        case APE_OBJECT_ANY:
            {
                APE_ASSERT(false);
            }
            break;
    }
}

bool traceback_to_string(const Traceback* traceback, StringBuffer* buf)
{
    int i;
    int depth;
    depth = traceback->items->count();
    for(i = 0; i < depth; i++)
    {
        TracebackItem* item = (TracebackItem*)traceback->items->get(i);
        const char* filename = traceback_item_get_filepath(item);
        if(item->pos.line >= 0 && item->pos.column >= 0)
        {
            strbuf_appendf(buf, "%s in %s on %d:%d\n", item->function_name, filename, item->pos.line, item->pos.column);
        }
        else
        {
            strbuf_appendf(buf, "%s\n", item->function_name);
        }
    }
    return !strbuf_failed(buf);
}

const char* ape_error_type_to_string(ErrorType type)
{
    switch(type)
    {
        case APE_ERROR_PARSING:
            return "PARSING";
        case APE_ERROR_COMPILATION:
            return "COMPILATION";
        case APE_ERROR_RUNTIME:
            return "RUNTIME";
        case APE_ERROR_TIMEOUT:
            return "TIMEOUT";
        case APE_ERROR_ALLOCATION:
            return "ALLOCATION";
        case APE_ERROR_USER:
            return "USER";
        default:
            return "NONE";
    }
}

const char* token_type_to_string(TokenType type)
{
    return g_type_names[type];
}
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




/**/
static OpcodeDefinition g_definitions[OPCODE_MAX + 1] =
{
    { "NONE", 0, { 0 } },
    { "CONSTANT", 1, { 2 } },
    { "ADD", 0, { 0 } },
    { "POP", 0, { 0 } },
    { "SUB", 0, { 0 } },
    { "MUL", 0, { 0 } },
    { "DIV", 0, { 0 } },
    { "MOD", 0, { 0 } },
    { "TRUE", 0, { 0 } },
    { "FALSE", 0, { 0 } },
    { "COMPARE", 0, { 0 } },
    { "COMPARE_EQ", 0, { 0 } },
    { "EQUAL", 0, { 0 } },
    { "NOT_EQUAL", 0, { 0 } },
    { "GREATER_THAN", 0, { 0 } },
    { "GREATER_THAN_EQUAL", 0, { 0 } },
    { "MINUS", 0, { 0 } },
    { "BANG", 0, { 0 } },
    { "JUMP", 1, { 2 } },
    { "JUMP_IF_FALSE", 1, { 2 } },
    { "JUMP_IF_TRUE", 1, { 2 } },
    { "NULL", 0, { 0 } },
    { "GET_MODULE_GLOBAL", 1, { 2 } },
    { "SET_MODULE_GLOBAL", 1, { 2 } },
    { "DEFINE_MODULE_GLOBAL", 1, { 2 } },
    { "ARRAY", 1, { 2 } },
    { "MAP_START", 1, { 2 } },
    { "MAP_END", 1, { 2 } },
    { "GET_THIS", 0, { 0 } },
    { "GET_INDEX", 0, { 0 } },
    { "SET_INDEX", 0, { 0 } },
    { "GET_VALUE_AT", 0, { 0 } },
    { "CALL", 1, { 1 } },
    { "RETURN_VALUE", 0, { 0 } },
    { "RETURN", 0, { 0 } },
    { "GET_LOCAL", 1, { 1 } },
    { "DEFINE_LOCAL", 1, { 1 } },
    { "SET_LOCAL", 1, { 1 } },
    { "GET_APE_GLOBAL", 1, { 2 } },
    { "FUNCTION", 2, { 2, 1 } },
    { "GET_FREE", 1, { 1 } },
    { "SET_FREE", 1, { 1 } },
    { "CURRENT_FUNCTION", 0, { 0 } },
    { "DUP", 0, { 0 } },
    { "NUMBER", 1, { 8 } },
    { "LEN", 0, { 0 } },
    { "SET_RECOVER", 1, { 2 } },
    { "OR", 0, { 0 } },
    { "XOR", 0, { 0 } },
    { "AND", 0, { 0 } },
    { "LSHIFT", 0, { 0 } },
    { "RSHIFT", 0, { 0 } },
    { "INVALID_MAX", 0, { 0 } },
};


#if 0
Object ape_object_to_object(Object obj)
{
    return (Object){ .handle = obj._internal };
}

Object object_to_ape_object(Object obj)
{
    return (Object){ ._internal = obj.handle };
}

#else
    #define ape_object_to_object(o) o
    #define object_to_ape_object(o) o
#endif

char* ape_stringf(Allocator* alloc, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int to_write = vsnprintf(NULL, 0, format, args);
    va_end(args);
    va_start(args, format);
    char* res = (char*)allocator_malloc(alloc, to_write + 1);
    if(!res)
    {
        return NULL;
    }
    int written = vsprintf(res, format, args);
    (void)written;
    APE_ASSERT(written == to_write);
    va_end(args);
    return res;
}

void ape_log(const char* file, int line, const char* format, ...)
{
    char msg[4096];
    int written = snprintf(msg, APE_ARRAY_LEN(msg), "%s:%d: ", file, line);
    (void)written;
    va_list args;
    va_start(args, format);
    int written_msg = vsnprintf(msg + written, APE_ARRAY_LEN(msg) - written, format, args);
    (void)written_msg;
    va_end(args);

    APE_ASSERT(written_msg <= (APE_ARRAY_LEN(msg) - written));

    printf("%s", msg);
}

char* ape_strndup(Allocator* alloc, const char* string, size_t n)
{
    char* output_string = (char*)allocator_malloc(alloc, n + 1);
    if(!output_string)
    {
        return NULL;
    }
    output_string[n] = '\0';
    memcpy(output_string, string, n);
    return output_string;
}

char* ape_strdup(Allocator* alloc, const char* string)
{
    if(!string)
    {
        return NULL;
    }
    return ape_strndup(alloc, string, strlen(string));
}

uint64_t ape_double_to_uint64(double val)
{
    union
    {
        uint64_t val_uint64;
        double val_double;
    } temp = { .val_double = val };
    return temp.val_uint64;
}

double ape_uint64_to_double(uint64_t val)
{
    union
    {
        uint64_t val_uint64;
        double val_double;
    } temp = { .val_uint64 = val };
    return temp.val_double;
}

bool ape_timer_platform_supported()
{
#if defined(APE_POSIX) || defined(APE_EMSCRIPTEN) || defined(APE_WINDOWS)
    return true;
#else
    return false;
#endif
}

Timer ape_timer_start()
{
    Timer timer;
    memset(&timer, 0, sizeof(Timer));
#if defined(APE_POSIX)
    // At some point it should be replaced with more accurate per-platform timers
    struct timeval start_time;
    gettimeofday(&start_time, NULL);
    timer.start_offset = start_time.tv_sec;
    timer.start_time_ms = start_time.tv_usec / 1000.0;
#elif defined(APE_WINDOWS)
    LARGE_INTEGER li;
    QueryPerformanceFrequency(&li);// not sure what to do if it fails
    timer.pc_frequency = (double)(li.QuadPart) / 1000.0;
    QueryPerformanceCounter(&li);
    timer.start_time_ms = li.QuadPart / timer.pc_frequency;
#elif defined(APE_EMSCRIPTEN)
    timer.start_time_ms = emscripten_get_now();
#endif
    return timer;
}

double ape_timer_get_elapsed_ms(const Timer* timer)
{
#if defined(APE_POSIX)
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    int time_s = (int)((int64_t)current_time.tv_sec - timer->start_offset);
    double current_time_ms = (time_s * 1000) + (current_time.tv_usec / 1000.0);
    return current_time_ms - timer->start_time_ms;
#elif defined(APE_WINDOWS)
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    double current_time_ms = li.QuadPart / timer->pc_frequency;
    return current_time_ms - timer->start_time_ms;
#elif defined(APE_EMSCRIPTEN)
    double current_time_ms = emscripten_get_now();
    return current_time_ms - timer->start_time_ms;
#else
    return 0;
#endif
}

//-----------------------------------------------------------------------------
// Collections
//-----------------------------------------------------------------------------

char* collections_strndup(Allocator* alloc, const char* string, size_t n)
{
    char* output_string = (char*)allocator_malloc(alloc, n + 1);
    if(!output_string)
    {
        return NULL;
    }
    output_string[n] = '\0';
    memcpy(output_string, string, n);
    return output_string;
}

char* collections_strdup(Allocator* alloc, const char* string)
{
    if(!string)
    {
        return NULL;
    }
    return collections_strndup(alloc, string, strlen(string));
}

unsigned long collections_hash(const void* ptr, size_t len)
{ /* djb2 */
    const uint8_t* ptr_u8 = (const uint8_t*)ptr;
    unsigned long hash = 5381;
    for(size_t i = 0; i < len; i++)
    {
        uint8_t val = ptr_u8[i];
        hash = ((hash << 5) + hash) + val;
    }
    return hash;
}

static unsigned int upper_power_of_two(unsigned int v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

//-----------------------------------------------------------------------------
// Allocator
//-----------------------------------------------------------------------------

Allocator allocator_make(AllocatorMallocFNCallback malloc_fn, AllocatorFreeFNCallback free_fn, void* ctx)
{
    Allocator alloc;
    alloc.malloc = malloc_fn;
    alloc.free = free_fn;
    alloc.ctx = ctx;
    return alloc;
}

void* allocator_malloc(Allocator* allocator, size_t size)
{
    if(!allocator || !allocator->malloc)
    {
        return malloc(size);
    }
    return allocator->malloc(allocator->ctx, size);
}

void allocator_free(Allocator* allocator, void* ptr)
{
    if(!allocator || !allocator->free)
    {
        free(ptr);
        return;
    }
    allocator->free(allocator->ctx, ptr);
}



//-----------------------------------------------------------------------------
// String buffer
//-----------------------------------------------------------------------------


StringBuffer* strbuf_make(Allocator* alloc)
{
    return strbuf_make_with_capacity(alloc, 1);
}

StringBuffer* strbuf_make_with_capacity(Allocator* alloc, unsigned int capacity)
{
    StringBuffer* buf = (StringBuffer*)allocator_malloc(alloc, sizeof(StringBuffer));
    if(buf == NULL)
    {
        return NULL;
    }
    memset(buf, 0, sizeof(StringBuffer));
    buf->alloc = alloc;
    buf->failed = false;
    buf->stringdata = (char*)allocator_malloc(alloc, capacity);
    if(buf->stringdata == NULL)
    {
        allocator_free(alloc, buf);
        return NULL;
    }
    buf->capacity = capacity;
    buf->len = 0;
    buf->stringdata[0] = '\0';
    return buf;
}

void strbuf_destroy(StringBuffer* buf)
{
    if(buf == NULL)
    {
        return;
    }
    allocator_free(buf->alloc, buf->stringdata);
    allocator_free(buf->alloc, buf);
}

void strbuf_clear(StringBuffer* buf)
{
    if(buf->failed)
    {
        return;
    }
    buf->len = 0;
    buf->stringdata[0] = '\0';
}

bool strbuf_append(StringBuffer* buf, const char* str)
{
    if(buf->failed)
    {
        return false;
    }
    size_t str_len = strlen(str);
    if(str_len == 0)
    {
        return true;
    }
    size_t required_capacity = buf->len + str_len + 1;
    if(required_capacity > buf->capacity)
    {
        bool ok = strbuf_grow(buf, required_capacity * 2);
        if(!ok)
        {
            return false;
        }
    }
    memcpy(buf->stringdata + buf->len, str, str_len);
    buf->len = buf->len + str_len;
    buf->stringdata[buf->len] = '\0';
    return true;
}

bool strbuf_appendf(StringBuffer* buf, const char* fmt, ...)
{
    if(buf->failed)
    {
        return false;
    }
    va_list args;
    va_start(args, fmt);
    int to_write = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if(to_write == 0)
    {
        return true;
    }
    size_t required_capacity = buf->len + to_write + 1;
    if(required_capacity > buf->capacity)
    {
        bool ok = strbuf_grow(buf, required_capacity * 2);
        if(!ok)
        {
            return false;
        }
    }
    va_start(args, fmt);
    int written = vsprintf(buf->stringdata + buf->len, fmt, args);
    (void)written;
    va_end(args);
    if(written != to_write)
    {
        return false;
    }
    buf->len = buf->len + to_write;
    buf->stringdata[buf->len] = '\0';
    return true;
}

const char* strbuf_get_string(const StringBuffer* buf)
{
    if(buf->failed)
    {
        return NULL;
    }
    return buf->stringdata;
}

size_t strbuf_get_length(const StringBuffer* buf)
{
    if(buf->failed)
    {
        return 0;
    }
    return buf->len;
}

char* strbuf_get_string_and_destroy(StringBuffer* buf)
{
    if(buf->failed)
    {
        strbuf_destroy(buf);
        return NULL;
    }
    char* res = buf->stringdata;
    buf->stringdata = NULL;
    strbuf_destroy(buf);
    return res;
}

bool strbuf_failed(StringBuffer* buf)
{
    return buf->failed;
}

bool strbuf_grow(StringBuffer* buf, size_t new_capacity)
{
    char* new_data = (char*)allocator_malloc(buf->alloc, new_capacity);
    if(new_data == NULL)
    {
        buf->failed = true;
        return false;
    }
    memcpy(new_data, buf->stringdata, buf->len);
    new_data[buf->len] = '\0';
    allocator_free(buf->alloc, buf->stringdata);
    buf->stringdata = new_data;
    buf->capacity = new_capacity;
    return true;
}

//-----------------------------------------------------------------------------
// Utils
//-----------------------------------------------------------------------------

PtrArray * kg_split_string(Allocator* alloc, const char* str, const char* delimiter)
{
    int i;
    long len;
    bool ok;
    PtrArray* res;
    char* line;
    char* rest;
    const char* line_start;
    const char* line_end;
    ok = false;
    res = PtrArray::make(alloc);
    rest = NULL;
    if(!str)
    {
        return res;
    }

    line_start = str;
    line_end = strstr(line_start, delimiter);
    while(line_end != NULL)
    {
        len = line_end - line_start;
        line = collections_strndup(alloc, line_start, len);
        if(!line)
        {
            goto err;
        }
        ok = res->add(line);
        if(!ok)
        {
            allocator_free(alloc, line);
            goto err;
        }
        line_start = line_end + 1;
        line_end = strstr(line_start, delimiter);
    }
    rest = collections_strdup(alloc, line_start);
    if(!rest)
    {
        goto err;
    }
    ok = res->add(rest);
    if(!ok)
    {
        goto err;
    }
    return res;
err:
    allocator_free(alloc, rest);
    if(res)
    {
        for(i = 0; i < res->count(); i++)
        {
            line = (char*)res->get(i);
            allocator_free(alloc, line);
        }
    }
    res->destroy();
    return NULL;
}

char* kg_join(Allocator* alloc, PtrArray * items, const char* with)
{
    int i;
    char* item;
    StringBuffer* res;
    res = strbuf_make(alloc);
    if(!res)
    {
        return NULL;
    }
    for(i = 0; i < items->count(); i++)
    {
        item = (char*)items->get(i);
        strbuf_append(res, item);
        if(i < (items->count() - 1))
        {
            strbuf_append(res, with);
        }
    }
    return strbuf_get_string_and_destroy(res);
}

char* kg_canonicalise_path(Allocator* alloc, const char* path)
{
    int i;
    if(!strchr(path, '/') || (!strstr(path, "/../") && !strstr(path, "./")))
    {
        return collections_strdup(alloc, path);
    }

    PtrArray* split = kg_split_string(alloc, path, "/");
    if(!split)
    {
        return NULL;
    }

    for(i = 0; i < split->count() - 1; i++)
    {
        char* item = (char*)split->get(i);
        char* next_item = (char*)split->get(i + 1);
        if(kg_streq(item, "."))
        {
            allocator_free(alloc, item);
            split->removeAt(i);
            i = -1;
            continue;
        }
        if(kg_streq(next_item, ".."))
        {
            allocator_free(alloc, item);
            allocator_free(alloc, next_item);
            split->removeAt(i);
            split->removeAt(i);
            i = -1;
            continue;
        }
    }

    char* joined = kg_join(alloc, split, "/");
    for(i = 0; i < split->count(); i++)
    {
        void* item = (void*)split->get(i);
        allocator_free(alloc, item);
    }
    split->destroy();
    return joined;
}

bool kg_is_path_absolute(const char* path)
{
    return path[0] == '/';
}

bool kg_streq(const char* a, const char* b)
{
    return strcmp(a, b) == 0;
}

static void errors_init(ErrorList* errors)
{
    memset(errors, 0, sizeof(ErrorList));
    errors->m_count = 0;
}

static void errors_deinit(ErrorList* errors)
{
    errors_clear(errors);
}

void errors_add_error(ErrorList* errors, ErrorType type, Position pos, const char* message)
{
    if(errors->m_count >= ERRORS_MAX_COUNT)
    {
        return;
    }
    Error err;
    memset(&err, 0, sizeof(Error));
    err.type = type;
    int len = (int)strlen(message);
    int to_copy = len;
    if(to_copy >= (APE_ERROR_MESSAGE_MAX_LENGTH - 1))
    {
        to_copy = APE_ERROR_MESSAGE_MAX_LENGTH - 1;
    }
    memcpy(err.message, message, to_copy);
    err.message[to_copy] = '\0';
    err.pos = pos;
    err.traceback = NULL;
    errors->errors[errors->m_count] = err;
    errors->m_count++;
}

void errors_add_errorf(ErrorList* errors, ErrorType type, Position pos, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int to_write = vsnprintf(NULL, 0, format, args);
    (void)to_write;
    va_end(args);
    va_start(args, format);
    char res[APE_ERROR_MESSAGE_MAX_LENGTH];
    int written = vsnprintf(res, APE_ERROR_MESSAGE_MAX_LENGTH, format, args);
    (void)written;
    APE_ASSERT(to_write == written);
    va_end(args);
    errors_add_error(errors, type, pos, res);
}

void errors_clear(ErrorList* errors)
{
    int i;
    for(i = 0; i < errors_get_count(errors); i++)
    {
        Error* error = errors_get(errors, i);
        if(error->traceback)
        {
            traceback_destroy(error->traceback);
        }
    }
    errors->m_count = 0;
}

int errors_get_count(const ErrorList* errors)
{
    return errors->m_count;
}

Error* errors_get(ErrorList* errors, int ix)
{
    if(ix >= errors->m_count)
    {
        return NULL;
    }
    return &errors->errors[ix];
}

static const Error* errors_getc(const ErrorList* errors, int ix)
{
    if(ix >= errors->m_count)
    {
        return NULL;
    }
    return &errors->errors[ix];
}


Error* errors_get_last_error(ErrorList* errors)
{
    if(errors->m_count <= 0)
    {
        return NULL;
    }
    return &errors->errors[errors->m_count - 1];
}

bool errors_has_errors(const ErrorList* errors)
{
    return errors_get_count(errors) > 0;
}




CompiledFile* compiled_file_make(Allocator* alloc, const char* path)
{
    CompiledFile* file = (CompiledFile*)allocator_malloc(alloc, sizeof(CompiledFile));
    if(!file)
    {
        return NULL;
    }
    memset(file, 0, sizeof(CompiledFile));
    file->alloc = alloc;
    const char* last_slash_pos = strrchr(path, '/');
    if(last_slash_pos)
    {
        size_t len = last_slash_pos - path + 1;
        file->dir_path = ape_strndup(alloc, path, len);
    }
    else
    {
        file->dir_path = ape_strdup(alloc, "");
    }
    if(!file->dir_path)
    {
        goto error;
    }
    file->path = ape_strdup(alloc, path);
    if(!file->path)
    {
        goto error;
    }
    file->lines = PtrArray::make(alloc);
    if(!file->lines)
    {
        goto error;
    }
    return file;
error:
    compiled_file_destroy(file);
    return NULL;
}

void compiled_file_destroy(CompiledFile* file)
{
    int i;
    if(!file)
    {
        return;
    }
    for(i = 0; i < file->lines->count(); i++)
    {
        void* item = (void*)file->lines->get(i);
        allocator_free(file->alloc, item);
    }
    file->lines->destroy();
    allocator_free(file->alloc, file->dir_path);
    allocator_free(file->alloc, file->path);
    allocator_free(file->alloc, file);
}

GlobalStore* global_store_make(Allocator* alloc, GCMemory* mem)
{
    int i;
    bool ok;
    const char* name;
    Object builtin;
    GlobalStore* store;
    store = (GlobalStore*)allocator_malloc(alloc, sizeof(GlobalStore));
    if(!store)
    {
        return NULL;
    }
    memset(store, 0, sizeof(GlobalStore));
    store->alloc = alloc;
    store->symbols = Dictionary::make(alloc, symbol_copy, symbol_destroy);
    if(!store->symbols)
    {
        goto err;
    }
    store->objects = Array::make<Object>(alloc);
    if(!store->objects)
    {
        goto err;
    }

    if(mem)
    {
        for(i = 0; i < builtins_count(); i++)
        {
            name = builtins_get_name(i);
            builtin = object_make_native_function_memory(mem, name, builtins_get_fn(i), NULL, 0);
            if(object_is_null(builtin))
            {
                goto err;
            }
            ok = global_store_set(store, name, builtin);
            if(!ok)
            {
                goto err;
            }
        }
    }

    return store;
err:
    global_store_destroy(store);
    return NULL;
}

void global_store_destroy(GlobalStore* store)
{
    if(!store)
    {
        return;
    }
    store->symbols->destroyWithItems();
    Array::destroy(store->objects);
    allocator_free(store->alloc, store);
}

const Symbol* global_store_get_symbol(GlobalStore* store, const char* name)
{
    return (Symbol*)store->symbols->get(name);
}

Object global_store_get_object(GlobalStore* store, const char* name)
{
    const Symbol* symbol = global_store_get_symbol(store, name);
    if(!symbol)
    {
        return object_make_null();
    }
    APE_ASSERT(symbol->type == SYMBOL_APE_GLOBAL);
    Object* res = (Object*)store->objects->get(symbol->index);
    return *res;
}

bool global_store_set(GlobalStore* store, const char* name, Object object)
{
    int ix;
    bool ok;
    Symbol* symbol;
    const Symbol* existing_symbol;
    existing_symbol = global_store_get_symbol(store, name);
    if(existing_symbol)
    {
        ok = store->objects->set(existing_symbol->index, &object);
        return ok;
    }
    ix = store->objects->count();
    ok = store->objects->add(&object);
    if(!ok)
    {
        return false;
    }
    symbol = symbol_make(store->alloc, name, SYMBOL_APE_GLOBAL, ix, false);
    if(!symbol)
    {
        goto err;
    }
    ok = store->symbols->set(name, symbol);
    if(!ok)
    {
        symbol_destroy(symbol);
        goto err;
    }
    return true;
err:
    store->objects->pop(NULL);
    return false;
}

Object global_store_get_object_at(GlobalStore* store, int ix, bool* out_ok)
{
    Object* res = (Object*)store->objects->get(ix);
    if(!res)
    {
        *out_ok = false;
        return object_make_null();
    }
    *out_ok = true;
    return *res;
}

bool global_store_set_object_at(GlobalStore* store, int ix, Object object)
{
    bool ok = store->objects->set(ix, &object);
    return ok;
}

Object* global_store_get_object_data(GlobalStore* store)
{
    return (Object*)store->objects->data();
}

int global_store_get_object_count(GlobalStore* store)
{
    return store->objects->count();
}


Symbol* symbol_make(Allocator* alloc, const char* name, SymbolType type, int index, bool assignable)
{
    Symbol* symbol;
    symbol = (Symbol*)allocator_malloc(alloc, sizeof(Symbol));
    if(!symbol)
    {
        return NULL;
    }
    memset(symbol, 0, sizeof(Symbol));
    symbol->alloc = alloc;
    symbol->name = ape_strdup(alloc, name);
    if(!symbol->name)
    {
        allocator_free(alloc, symbol);
        return NULL;
    }
    symbol->type = type;
    symbol->index = index;
    symbol->assignable = assignable;
    return symbol;
}

void symbol_destroy(Symbol* symbol)
{
    if(!symbol)
    {
        return;
    }
    allocator_free(symbol->alloc, symbol->name);
    allocator_free(symbol->alloc, symbol);
}

Symbol* symbol_copy(Symbol* symbol)
{
    return symbol_make(symbol->alloc, symbol->name, symbol->type, symbol->index, symbol->assignable);
}

SymbolTable* symbol_table_make(Allocator* alloc, SymbolTable* outer, GlobalStore* global_store, int module_global_offset)
{
    bool ok;
    SymbolTable* table;
    table = (SymbolTable*)allocator_malloc(alloc, sizeof(SymbolTable));
    if(!table)
    {
        return NULL;
    }
    memset(table, 0, sizeof(SymbolTable));
    table->alloc = alloc;
    table->max_num_definitions = 0;
    table->outer = outer;
    table->global_store = global_store;
    table->module_global_offset = module_global_offset;
    table->block_scopes = PtrArray::make(alloc);
    if(!table->block_scopes)
    {
        goto err;
    }
    table->free_symbols = PtrArray::make(alloc);
    if(!table->free_symbols)
    {
        goto err;
    }
    table->module_global_symbols = PtrArray::make(alloc);
    if(!table->module_global_symbols)
    {
        goto err;
    }
    ok = symbol_table_push_block_scope(table);
    if(!ok)
    {
        goto err;
    }
    return table;
err:
    symbol_table_destroy(table);
    return NULL;
}

void symbol_table_destroy(SymbolTable* table)
{
    Allocator* alloc;
    if(!table)
    {
        return;
    }

    while(table->block_scopes->count() > 0)
    {
        symbol_table_pop_block_scope(table);
    }
    table->block_scopes->destroy();
    table->module_global_symbols->destroyWithItems(symbol_destroy);
    table->free_symbols->destroyWithItems(symbol_destroy);
    alloc = table->alloc;
    memset(table, 0, sizeof(SymbolTable));
    allocator_free(alloc, table);
}

SymbolTable* symbol_table_copy(SymbolTable* table)
{
    SymbolTable* copy;
    copy = (SymbolTable*)allocator_malloc(table->alloc, sizeof(SymbolTable));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(SymbolTable));
    copy->alloc = table->alloc;
    copy->outer = table->outer;
    copy->global_store = table->global_store;
    copy->block_scopes = table->block_scopes->copyWithItems(block_scope_copy, block_scope_destroy);
    if(!copy->block_scopes)
    {
        goto err;
    }
    copy->free_symbols = table->free_symbols->copyWithItems(symbol_copy, symbol_destroy);
    if(!copy->free_symbols)
    {
        goto err;
    }
    copy->module_global_symbols = table->module_global_symbols->copyWithItems(symbol_copy, symbol_destroy);
    if(!copy->module_global_symbols)
    {
        goto err;
    }
    copy->max_num_definitions = table->max_num_definitions;
    copy->module_global_offset = table->module_global_offset;
    return copy;
err:
    symbol_table_destroy(copy);
    return NULL;
}

bool symbol_table_add_module_symbol(SymbolTable* st, Symbol* symbol)
{
    bool ok;
    if(symbol->type != SYMBOL_MODULE_GLOBAL)
    {
        APE_ASSERT(false);
        return false;
    }
    if(symbol_table_symbol_is_defined(st, symbol->name))
    {
        return true;// todo: make sure it should be true in this case
    }
    Symbol* copy = symbol_copy(symbol);
    if(!copy)
    {
        return false;
    }
    ok = set_symbol(st, copy);
    if(!ok)
    {
        symbol_destroy(copy);
        return false;
    }
    return true;
}

const Symbol* symbol_table_define(SymbolTable* table, const char* name, bool assignable)
{
    
    bool ok;
    bool global_symbol_added;
    int ix;
    int definitions_count;
    BlockScope* top_scope;
    SymbolType symbol_type;
    Symbol* symbol;
    Symbol* global_symbol_copy;
    const Symbol* global_symbol;
    global_symbol = global_store_get_symbol(table->global_store, name);
    if(global_symbol)
    {
        return NULL;
    }
    if(strchr(name, ':'))
    {
        return NULL;// module symbol
    }
    if(APE_STREQ(name, "this"))
    {
        return NULL;// "this" is reserved
    }
    symbol_type = table->outer == NULL ? SYMBOL_MODULE_GLOBAL : SYMBOL_LOCAL;
    ix = next_symbol_index(table);
    symbol = symbol_make(table->alloc, name, symbol_type, ix, assignable);
    if(!symbol)
    {
        return NULL;
    }
    global_symbol_added = false;
    ok = false;
    if(symbol_type == SYMBOL_MODULE_GLOBAL && table->block_scopes->count() == 1)
    {
        global_symbol_copy = symbol_copy(symbol);
        if(!global_symbol_copy)
        {
            symbol_destroy(symbol);
            return NULL;
        }
        ok = table->module_global_symbols->add(global_symbol_copy);
        if(!ok)
        {
            symbol_destroy(global_symbol_copy);
            symbol_destroy(symbol);
            return NULL;
        }
        global_symbol_added = true;
    }

    ok = set_symbol(table, symbol);
    if(!ok)
    {
        if(global_symbol_added)
        {
            global_symbol_copy = (Symbol*)table->module_global_symbols->pop();
            symbol_destroy(global_symbol_copy);
        }
        symbol_destroy(symbol);
        return NULL;
    }
    top_scope = (BlockScope*)table->block_scopes->top();
    top_scope->num_definitions++;
    definitions_count = count_num_definitions(table);
    if(definitions_count > table->max_num_definitions)
    {
        table->max_num_definitions = definitions_count;
    }

    return symbol;
}

const Symbol* symbol_table_define_free(SymbolTable* st, const Symbol* original)
{
    bool ok;
    Symbol* symbol;
    Symbol* copy;
    copy = symbol_make(st->alloc, original->name, original->type, original->index, original->assignable);
    if(!copy)
    {
        return NULL;
    }
    ok = st->free_symbols->add(copy);
    if(!ok)
    {
        symbol_destroy(copy);
        return NULL;
    }

    symbol = symbol_make(st->alloc, original->name, SYMBOL_FREE, st->free_symbols->count() - 1, original->assignable);
    if(!symbol)
    {
        return NULL;
    }

    ok = set_symbol(st, symbol);
    if(!ok)
    {
        symbol_destroy(symbol);
        return NULL;
    }

    return symbol;
}

const Symbol* symbol_table_define_function_name(SymbolTable* st, const char* name, bool assignable)
{
    bool ok;
    Symbol* symbol;
    if(strchr(name, ':'))
    {
        return NULL;// module symbol
    }
    symbol = symbol_make(st->alloc, name, SYMBOL_FUNCTION, 0, assignable);
    if(!symbol)
    {
        return NULL;
    }
    ok = set_symbol(st, symbol);
    if(!ok)
    {
        symbol_destroy(symbol);
        return NULL;
    }

    return symbol;
}

const Symbol* symbol_table_define_this(SymbolTable* st)
{
    bool ok;
    Symbol* symbol;
    symbol = symbol_make(st->alloc, "this", SYMBOL_THIS, 0, false);
    if(!symbol)
    {
        return NULL;
    }
    ok = set_symbol(st, symbol);
    if(!ok)
    {
        symbol_destroy(symbol);
        return NULL;
    }
    return symbol;
}

const Symbol* symbol_table_resolve(SymbolTable* table, const char* name)
{
    int i;
    const Symbol* symbol;
    BlockScope* scope;
    scope = NULL;
    symbol = global_store_get_symbol(table->global_store, name);
    if(symbol)
    {
        return symbol;
    }
    for(i = table->block_scopes->count() - 1; i >= 0; i--)
    {
        scope = (BlockScope*)table->block_scopes->get(i);
        symbol = (Symbol*)scope->store->get(name);
        if(symbol)
        {
            break;
        }
    }
    if(symbol && symbol->type == SYMBOL_THIS)
    {
        symbol = symbol_table_define_free(table, symbol);
    }

    if(!symbol && table->outer)
    {
        symbol = symbol_table_resolve(table->outer, name);
        if(!symbol)
        {
            return NULL;
        }
        if(symbol->type == SYMBOL_MODULE_GLOBAL || symbol->type == SYMBOL_APE_GLOBAL)
        {
            return symbol;
        }
        symbol = symbol_table_define_free(table, symbol);
    }
    return symbol;
}

bool symbol_table_symbol_is_defined(SymbolTable* table, const char* name)
{
    BlockScope* top_scope;
    const Symbol* symbol;
    // todo: rename to something more obvious
    symbol = global_store_get_symbol(table->global_store, name);
    if(symbol)
    {
        return true;
    }
    top_scope = (BlockScope*)table->block_scopes->top();
    symbol = (Symbol*)top_scope->store->get(name);
    if(symbol)
    {
        return true;
    }
    return false;
}

bool symbol_table_push_block_scope(SymbolTable* table)
{
    bool ok;
    int block_scope_offset;
    BlockScope* prev_block_scope;
    BlockScope* new_scope;
    block_scope_offset = 0;
    prev_block_scope = (BlockScope*)table->block_scopes->top();
    if(prev_block_scope)
    {
        block_scope_offset = table->module_global_offset + prev_block_scope->offset + prev_block_scope->num_definitions;
    }
    else
    {
        block_scope_offset = table->module_global_offset;
    }

    new_scope = block_scope_make(table->alloc, block_scope_offset);
    if(!new_scope)
    {
        return false;
    }
    ok = table->block_scopes->push(new_scope);
    if(!ok)
    {
        block_scope_destroy(new_scope);
        return false;
    }
    return true;
}

void symbol_table_pop_block_scope(SymbolTable* table)
{
    BlockScope* top_scope;
    top_scope = (BlockScope*)table->block_scopes->top();
    table->block_scopes->pop();
    block_scope_destroy(top_scope);
}

BlockScope* symbol_table_get_block_scope(SymbolTable* table)
{
    BlockScope* top_scope;
    top_scope = (BlockScope*)table->block_scopes->top();
    return top_scope;
}

bool symbol_table_is_module_global_scope(SymbolTable* table)
{
    return table->outer == NULL;
}

bool symbol_table_is_top_block_scope(SymbolTable* table)
{
    return table->block_scopes->count() == 1;
}

bool symbol_table_is_top_global_scope(SymbolTable* table)
{
    return symbol_table_is_module_global_scope(table) && symbol_table_is_top_block_scope(table);
}

int symbol_table_get_module_global_symbol_count(const SymbolTable* table)
{
    return table->module_global_symbols->count();
}

const Symbol* symbol_table_get_module_global_symbol_at(const SymbolTable* table, int ix)
{
    return (Symbol*)table->module_global_symbols->get(ix);
}

// INTERNAL
BlockScope* block_scope_make(Allocator* alloc, int offset)
{
    BlockScope* new_scope;
    new_scope = (BlockScope*)allocator_malloc(alloc, sizeof(BlockScope));
    if(!new_scope)
    {
        return NULL;
    }
    memset(new_scope, 0, sizeof(BlockScope));
    new_scope->alloc = alloc;
    new_scope->store = Dictionary::make(alloc, symbol_copy, symbol_destroy);
    if(!new_scope->store)
    {
        block_scope_destroy(new_scope);
        return NULL;
    }
    new_scope->num_definitions = 0;
    new_scope->offset = offset;
    return new_scope;
}

void block_scope_destroy(BlockScope* scope)
{
    scope->store->destroyWithItems();
    allocator_free(scope->alloc, scope);
}

BlockScope* block_scope_copy(BlockScope* scope)
{
    BlockScope* copy;
    copy = (BlockScope*)allocator_malloc(scope->alloc, sizeof(BlockScope));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(BlockScope));
    copy->alloc = scope->alloc;
    copy->num_definitions = scope->num_definitions;
    copy->offset = scope->offset;
    copy->store = scope->store->copyWithItems();
    if(!copy->store)
    {
        block_scope_destroy(copy);
        return NULL;
    }
    return copy;
}

static bool set_symbol(SymbolTable* table, Symbol* symbol)
{
    BlockScope* top_scope;
    Symbol* existing;
    top_scope = (BlockScope*)table->block_scopes->top();
    existing= (Symbol*)top_scope->store->get(symbol->name);
    if(existing)
    {
        symbol_destroy(existing);
    }
    return top_scope->store->set(symbol->name, symbol);
}

static int next_symbol_index(SymbolTable* table)
{
    int ix;
    BlockScope* top_scope;
    top_scope = (BlockScope*)table->block_scopes->top();
    ix = top_scope->offset + top_scope->num_definitions;
    return ix;
}

static int count_num_definitions(SymbolTable* table)
{
    int i;
    int cn;
    BlockScope* scope;
    cn = 0;
    for(i = table->block_scopes->count() - 1; i >= 0; i--)
    {
        scope = (BlockScope*)table->block_scopes->get(i);
        cn += scope->num_definitions;
    }
    return cn;
}


OpcodeDefinition* opcode_lookup(opcode_t op)
{
    if(op <= OPCODE_NONE || op >= OPCODE_MAX)
    {
        return NULL;
    }
    return &g_definitions[op];
}

const char* opcode_get_name(opcode_t op)
{
    if(op <= OPCODE_NONE || op >= OPCODE_MAX)
    {
        return NULL;
    }
    return g_definitions[op].name;
}

int code_make(opcode_t op, int operands_count, uint64_t* operands, Array* res)
{
    OpcodeDefinition* def = opcode_lookup(op);
    if(!def)
    {
        return 0;
    }

    int instr_len = 1;
    for(int i = 0; i < def->num_operands; i++)
    {
        instr_len += def->operand_widths[i];
    }

    uint8_t val = op;
    bool ok = false;

    ok = res->add(&val);
    if(!ok)
    {
        return 0;
    }

#define APPEND_BYTE(n)                           \
    do                                           \
    {                                            \
        val = (uint8_t)(operands[i] >> (n * 8)); \
        ok = res->add(&val);               \
        if(!ok)                                  \
        {                                        \
            return 0;                            \
        }                                        \
    } while(0)

    for(int i = 0; i < operands_count; i++)
    {
        int width = def->operand_widths[i];
        switch(width)
        {
            case 1:
            {
                APPEND_BYTE(0);
                break;
            }
            case 2:
            {
                APPEND_BYTE(1);
                APPEND_BYTE(0);
                break;
            }
            case 4:
            {
                APPEND_BYTE(3);
                APPEND_BYTE(2);
                APPEND_BYTE(1);
                APPEND_BYTE(0);
                break;
            }
            case 8:
            {
                APPEND_BYTE(7);
                APPEND_BYTE(6);
                APPEND_BYTE(5);
                APPEND_BYTE(4);
                APPEND_BYTE(3);
                APPEND_BYTE(2);
                APPEND_BYTE(1);
                APPEND_BYTE(0);
                break;
            }
            default:
            {
                APE_ASSERT(false);
                break;
            }
        }
    }
#undef APPEND_BYTE
    return instr_len;
}




CompilationScope* compilation_scope_make(Allocator* alloc, CompilationScope* outer)
{
    CompilationScope* scope = (CompilationScope*)allocator_malloc(alloc, sizeof(CompilationScope));
    if(!scope)
    {
        return NULL;
    }
    memset(scope, 0, sizeof(CompilationScope));
    scope->alloc = alloc;
    scope->outer = outer;
    scope->bytecode = Array::make<uint8_t>(alloc);
    if(!scope->bytecode)
    {
        goto err;
    }
    scope->src_positions = Array::make<Position>(alloc);
    if(!scope->src_positions)
    {
        goto err;
    }
    scope->break_ip_stack = Array::make<int>(alloc);
    if(!scope->break_ip_stack)
    {
        goto err;
    }
    scope->continue_ip_stack = Array::make<int>(alloc);
    if(!scope->continue_ip_stack)
    {
        goto err;
    }
    return scope;
err:
    compilation_scope_destroy(scope);
    return NULL;
}

void compilation_scope_destroy(CompilationScope* scope)
{
    Array::destroy(scope->continue_ip_stack);
    Array::destroy(scope->break_ip_stack);
    Array::destroy(scope->bytecode);
    Array::destroy(scope->src_positions);
    allocator_free(scope->alloc, scope);
}

CompilationResult* compilation_scope_orphan_result(CompilationScope* scope)
{
    CompilationResult* res;
    res = compilation_result_make(scope->alloc,
        (uint8_t*)scope->bytecode->data(),
        (Position*)scope->src_positions->data(),
        scope->bytecode->count()
    );
    if(!res)
    {
        return NULL;
    }
    scope->bytecode->orphanData();
    scope->src_positions->orphanData();
    return res;
}

CompilationResult* compilation_result_make(Allocator* alloc, uint8_t* bytecode, Position* src_positions, int cn)
{
    CompilationResult* res;
    res = (CompilationResult*)allocator_malloc(alloc, sizeof(CompilationResult));
    if(!res)
    {
        return NULL;
    }
    memset(res, 0, sizeof(CompilationResult));
    res->alloc = alloc;
    res->bytecode = bytecode;
    res->src_positions = src_positions;
    res->m_count = cn;
    return res;
}

void compilation_result_destroy(CompilationResult* res)
{
    if(!res)
    {
        return;
    }
    allocator_free(res->alloc, res->bytecode);
    allocator_free(res->alloc, res->src_positions);
    allocator_free(res->alloc, res);
}


Expression* optimise_infix_expression(Expression* expr);
Expression* optimise_prefix_expression(Expression* expr);

Expression* optimise_expression(Expression* expr)
{
    switch(expr->type)
    {
        case EXPRESSION_INFIX:
            return optimise_infix_expression(expr);
        case EXPRESSION_PREFIX:
            return optimise_prefix_expression(expr);
        default:
            return NULL;
    }
}

// INTERNAL
Expression* optimise_infix_expression(Expression* expr)
{
    Expression* left = expr->infix.left;
    Expression* left_optimised = optimise_expression(left);
    if(left_optimised)
    {
        left = left_optimised;
    }

    Expression* right = expr->infix.right;
    Expression* right_optimised = optimise_expression(right);
    if(right_optimised)
    {
        right = right_optimised;
    }

    Expression* res = NULL;

    bool left_is_numeric = left->type == EXPRESSION_NUMBER_LITERAL || left->type == EXPRESSION_BOOL_LITERAL;
    bool right_is_numeric = right->type == EXPRESSION_NUMBER_LITERAL || right->type == EXPRESSION_BOOL_LITERAL;

    bool left_is_string = left->type == EXPRESSION_STRING_LITERAL;
    bool right_is_string = right->type == EXPRESSION_STRING_LITERAL;

    Allocator* alloc = expr->alloc;
    if(left_is_numeric && right_is_numeric)
    {
        double left_val = left->type == EXPRESSION_NUMBER_LITERAL ? left->number_literal : left->bool_literal;
        double right_val = right->type == EXPRESSION_NUMBER_LITERAL ? right->number_literal : right->bool_literal;
        int64_t left_val_int = (int64_t)left_val;
        int64_t right_val_int = (int64_t)right_val;
        switch(expr->infix.op)
        {
            case OPERATOR_PLUS:
            {
                res = expression_make_number_literal(alloc, left_val + right_val);
                break;
            }
            case OPERATOR_MINUS:
            {
                res = expression_make_number_literal(alloc, left_val - right_val);
                break;
            }
            case OPERATOR_ASTERISK:
            {
                res = expression_make_number_literal(alloc, left_val * right_val);
                break;
            }
            case OPERATOR_SLASH:
            {
                res = expression_make_number_literal(alloc, left_val / right_val);
                break;
            }
            case OPERATOR_LT:
            {
                res = expression_make_bool_literal(alloc, left_val < right_val);
                break;
            }
            case OPERATOR_LTE:
            {
                res = expression_make_bool_literal(alloc, left_val <= right_val);
                break;
            }
            case OPERATOR_GT:
            {
                res = expression_make_bool_literal(alloc, left_val > right_val);
                break;
            }
            case OPERATOR_GTE:
            {
                res = expression_make_bool_literal(alloc, left_val >= right_val);
                break;
            }
            case OPERATOR_EQ:
            {
                res = expression_make_bool_literal(alloc, APE_DBLEQ(left_val, right_val));
                break;
            }
            case OPERATOR_NOT_EQ:
            {
                res = expression_make_bool_literal(alloc, !APE_DBLEQ(left_val, right_val));
                break;
            }
            case OPERATOR_MODULUS:
            {
                res = expression_make_number_literal(alloc, fmod(left_val, right_val));
                break;
            }
            case OPERATOR_BIT_AND:
            {
                res = expression_make_number_literal(alloc, (double)(left_val_int & right_val_int));
                break;
            }
            case OPERATOR_BIT_OR:
            {
                res = expression_make_number_literal(alloc, (double)(left_val_int | right_val_int));
                break;
            }
            case OPERATOR_BIT_XOR:
            {
                res = expression_make_number_literal(alloc, (double)(left_val_int ^ right_val_int));
                break;
            }
            case OPERATOR_LSHIFT:
            {
                res = expression_make_number_literal(alloc, (double)(left_val_int << right_val_int));
                break;
            }
            case OPERATOR_RSHIFT:
            {
                res = expression_make_number_literal(alloc, (double)(left_val_int >> right_val_int));
                break;
            }
            default:
            {
                break;
            }
        }
    }
    else if(expr->infix.op == OPERATOR_PLUS && left_is_string && right_is_string)
    {
        const char* left_val = left->string_literal;
        const char* right_val = right->string_literal;
        char* res_str = ape_stringf(alloc, "%s%s", left_val, right_val);
        if(res_str)
        {
            res = expression_make_string_literal(alloc, res_str);
            if(!res)
            {
                allocator_free(alloc, res_str);
            }
        }
    }

    expression_destroy(left_optimised);
    expression_destroy(right_optimised);

    if(res)
    {
        res->pos = expr->pos;
    }

    return res;
}

Expression* optimise_prefix_expression(Expression* expr)
{
    Expression* right = expr->prefix.right;
    Expression* right_optimised = optimise_expression(right);
    if(right_optimised)
    {
        right = right_optimised;
    }
    Expression* res = NULL;
    if(expr->prefix.op == OPERATOR_MINUS && right->type == EXPRESSION_NUMBER_LITERAL)
    {
        res = expression_make_number_literal(expr->alloc, -right->number_literal);
    }
    else if(expr->prefix.op == OPERATOR_BANG && right->type == EXPRESSION_BOOL_LITERAL)
    {
        res = expression_make_bool_literal(expr->alloc, !right->bool_literal);
    }
    expression_destroy(right_optimised);
    if(res)
    {
        res->pos = expr->pos;
    }
    return res;
}


Module* module_make(Allocator* alloc, const char* name)
{
    Module* module = (Module*)allocator_malloc(alloc, sizeof(Module));
    if(!module)
    {
        return NULL;
    }
    memset(module, 0, sizeof(Module));
    module->alloc = alloc;
    module->name = ape_strdup(alloc, name);
    if(!module->name)
    {
        module_destroy(module);
        return NULL;
    }
    module->symbols = PtrArray::make(alloc);
    if(!module->symbols)
    {
        module_destroy(module);
        return NULL;
    }
    return module;
}

void module_destroy(Module* module)
{
    if(!module)
    {
        return;
    }
    allocator_free(module->alloc, module->name);
    module->symbols->destroyWithItems(symbol_destroy);
    allocator_free(module->alloc, module);
}

Module* module_copy(Module* src)
{
    Module* copy = (Module*)allocator_malloc(src->alloc, sizeof(Module));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(Module));
    copy->alloc = src->alloc;
    copy->name = ape_strdup(copy->alloc, src->name);
    if(!copy->name)
    {
        module_destroy(copy);
        return NULL;
    }
    copy->symbols = src->symbols->copyWithItems(symbol_copy, symbol_destroy);
    if(!copy->symbols)
    {
        module_destroy(copy);
        return NULL;
    }
    return copy;
}

const char* get_module_name(const char* path)
{
    const char* last_slash_pos = strrchr(path, '/');
    if(last_slash_pos)
    {
        return last_slash_pos + 1;
    }
    return path;
}

bool module_add_symbol(Module* module, const Symbol* symbol)
{
    StringBuffer* name_buf = strbuf_make(module->alloc);
    if(!name_buf)
    {
        return false;
    }
    bool ok = strbuf_appendf(name_buf, "%s::%s", module->name, symbol->name);
    if(!ok)
    {
        strbuf_destroy(name_buf);
        return false;
    }
    Symbol* module_symbol = symbol_make(module->alloc, strbuf_get_string(name_buf), SYMBOL_MODULE_GLOBAL, symbol->index, false);
    strbuf_destroy(name_buf);
    if(!module_symbol)
    {
        return false;
    }
    ok = module->symbols->add(module_symbol);
    if(!ok)
    {
        symbol_destroy(module_symbol);
        return false;
    }
    return true;
}



Object object_make_from_data(ObjectType type, ObjectData* data)
{
    uint64_t type_tag;
    Object object;
    object.handle = OBJECT_PATTERN;
    type_tag = get_type_tag(type) & 0x7;
    object.handle |= (type_tag << 48);
    object.handle |= (uintptr_t)data;// assumes no pointer exceeds 48 bits
    return object;
}

Object object_make_number(double val)
{
    Object o = { .number = val };
    if((o.handle & OBJECT_PATTERN) == OBJECT_PATTERN)
    {
        o.handle = 0x7ff8000000000000;
    }
    return o;
}

Object object_make_bool(bool val)
{
    return (Object){ .handle = OBJECT_BOOL_HEADER | val };
}

Object object_make_null()
{
    return (Object){ .handle = OBJECT_NULL_PATTERN };
}


Object object_make_string_len(GCMemory* mem, const char* string, size_t len)
{
    Object res = object_make_string_with_capacity(mem, len);
    if(object_is_null(res))
    {
        return res;
    }
    bool ok = object_string_append(res, string, len);
    if(!ok)
    {
        return object_make_null();
    }
    return res;
}

Object object_make_string(GCMemory* mem, const char* string)
{
    return object_make_string_len(mem, string, strlen(string));
}


Object object_make_string_with_capacity(GCMemory* mem, int capacity)
{
    ObjectData* data = gcmem_get_object_data_from_pool(mem, APE_OBJECT_STRING);
    if(!data)
    {
        data = gcmem_alloc_object_data(mem, APE_OBJECT_STRING);
        if(!data)
        {
            return object_make_null();
        }
        data->string.capacity = OBJECT_STRING_BUF_SIZE - 1;
        data->string.is_allocated = false;
    }

    data->string.length = 0;
    data->string.hash = 0;

    if(capacity > data->string.capacity)
    {
        bool ok = object_data_string_reserve_capacity(data, capacity);
        if(!ok)
        {
            return object_make_null();
        }
    }

    return object_make_from_data(APE_OBJECT_STRING, data);
}

Object object_make_stringf(GCMemory* mem, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int to_write = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    va_start(args, fmt);
    Object res = object_make_string_with_capacity(mem, to_write);
    if(object_is_null(res))
    {
        return object_make_null();
    }
    char* res_buf = object_get_mutable_string(res);
    int written = vsprintf(res_buf, fmt, args);
    (void)written;
    APE_ASSERT(written == to_write);
    va_end(args);
    object_set_string_length(res, to_write);
    return res;
}

Object object_make_native_function_memory(GCMemory* mem, const char* name, NativeFNCallback fn, void* data, int data_len)
{
    if(data_len > NATIVE_FN_MAX_DATA_LEN)
    {
        return object_make_null();
    }
    ObjectData* obj = gcmem_alloc_object_data(mem, APE_OBJECT_NATIVE_FUNCTION);
    if(!obj)
    {
        return object_make_null();
    }
    obj->native_function.name = ape_strdup(mem->alloc, name);
    if(!obj->native_function.name)
    {
        return object_make_null();
    }
    obj->native_function.native_funcptr = fn;
    if(data)
    {
        //memcpy(obj->native_function.data, data, data_len);
        obj->native_function.data = data;
    }
    obj->native_function.data_len = data_len;
    return object_make_from_data(APE_OBJECT_NATIVE_FUNCTION, obj);
}

Object object_make_array(GCMemory* mem)
{
    return object_make_array_with_capacity(mem, 8);
}

Object object_make_array_with_capacity(GCMemory* mem, unsigned capacity)
{
    ObjectData* data = gcmem_get_object_data_from_pool(mem, APE_OBJECT_ARRAY);
    if(data)
    {
        data->array->clear();
        return object_make_from_data(APE_OBJECT_ARRAY, data);
    }
    data = gcmem_alloc_object_data(mem, APE_OBJECT_ARRAY);
    if(!data)
    {
        return object_make_null();
    }
    data->array = Array::make(mem->alloc, capacity, sizeof(Object));
    if(!data->array)
    {
        return object_make_null();
    }
    return object_make_from_data(APE_OBJECT_ARRAY, data);
}

Object object_make_map(GCMemory* mem)
{
    return object_make_map_with_capacity(mem, 32);
}

Object object_make_map_with_capacity(GCMemory* mem, unsigned capacity)
{
    ObjectData* data = gcmem_get_object_data_from_pool(mem, APE_OBJECT_MAP);
    if(data)
    {
        data->map->clear();
        return object_make_from_data(APE_OBJECT_MAP, data);
    }
    data = gcmem_alloc_object_data(mem, APE_OBJECT_MAP);
    if(!data)
    {
        return object_make_null();
    }
    data->map = ValDictionary::makeWithCapacity(mem->alloc, capacity, sizeof(Object), sizeof(Object));
    if(!data->map)
    {
        return object_make_null();
    }
    data->map->setHashFunction((CollectionsHashFNCallback)object_hash);
    data->map->setEqualsFunction((CollectionsEqualsFNCallback)object_equals_wrapped);
    return object_make_from_data(APE_OBJECT_MAP, data);
}

Object object_make_error(GCMemory* mem, const char* error)
{
    char* error_str = ape_strdup(mem->alloc, error);
    if(!error_str)
    {
        return object_make_null();
    }
    Object res = object_make_error_no_copy(mem, error_str);
    if(object_is_null(res))
    {
        allocator_free(mem->alloc, error_str);
        return object_make_null();
    }
    return res;
}

Object object_make_error_no_copy(GCMemory* mem, char* error)
{
    ObjectData* data = gcmem_alloc_object_data(mem, APE_OBJECT_ERROR);
    if(!data)
    {
        return object_make_null();
    }
    data->error.message = error;
    data->error.traceback = NULL;
    return object_make_from_data(APE_OBJECT_ERROR, data);
}

Object object_make_errorf(GCMemory* mem, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int to_write = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    va_start(args, fmt);
    char* res = (char*)allocator_malloc(mem->alloc, to_write + 1);
    if(!res)
    {
        return object_make_null();
    }
    int written = vsprintf(res, fmt, args);
    (void)written;
    APE_ASSERT(written == to_write);
    va_end(args);
    Object res_obj = object_make_error_no_copy(mem, res);
    if(object_is_null(res_obj))
    {
        allocator_free(mem->alloc, res);
        return object_make_null();
    }
    return res_obj;
}

Object
object_make_function(GCMemory* mem, const char* name, CompilationResult* comp_res, bool owns_data, int num_locals, int num_args, int free_vals_count)
{
    ObjectData* data = gcmem_alloc_object_data(mem, APE_OBJECT_FUNCTION);
    if(!data)
    {
        return object_make_null();
    }
    if(owns_data)
    {
        data->function.name = name ? ape_strdup(mem->alloc, name) : ape_strdup(mem->alloc, "anonymous");
        if(!data->function.name)
        {
            return object_make_null();
        }
    }
    else
    {
        data->function.const_name = name ? name : "anonymous";
    }
    data->function.comp_result = comp_res;
    data->function.owns_data = owns_data;
    data->function.num_locals = num_locals;
    data->function.num_args = num_args;
    if(free_vals_count >= APE_ARRAY_LEN(data->function.free_vals_buf))
    {
        data->function.free_vals_allocated = (Object*)allocator_malloc(mem->alloc, sizeof(Object) * free_vals_count);
        if(!data->function.free_vals_allocated)
        {
            return object_make_null();
        }
    }
    data->function.free_vals_count = free_vals_count;
    return object_make_from_data(APE_OBJECT_FUNCTION, data);
}

Object object_make_external(GCMemory* mem, void* data)
{
    ObjectData* obj = gcmem_alloc_object_data(mem, APE_OBJECT_EXTERNAL);
    if(!obj)
    {
        return object_make_null();
    }
    obj->external.data = data;
    obj->external.data_destroy_fn = NULL;
    obj->external.data_copy_fn = NULL;
    return object_make_from_data(APE_OBJECT_EXTERNAL, obj);
}

void object_deinit(Object obj)
{
    if(object_is_allocated(obj))
    {
        ObjectData* data = object_get_allocated_data(obj);
        object_data_deinit(data);
    }
}

void object_data_deinit(ObjectData* data)
{
    switch(data->type)
    {
        case APE_OBJECT_FREED:
        {
            APE_ASSERT(false);
            return;
        }
        case APE_OBJECT_STRING:
        {
            if(data->string.is_allocated)
            {
                allocator_free(data->mem->alloc, data->string.value_allocated);
            }
            break;
        }
        case APE_OBJECT_FUNCTION:
        {
            if(data->function.owns_data)
            {
                allocator_free(data->mem->alloc, data->function.name);
                compilation_result_destroy(data->function.comp_result);
            }
            if(freevals_are_allocated(&data->function))
            {
                allocator_free(data->mem->alloc, data->function.free_vals_allocated);
            }
            break;
        }
        case APE_OBJECT_ARRAY:
        {
            Array::destroy(data->array);
            break;
        }
        case APE_OBJECT_MAP:
        {
            data->map->destroy();
            break;
        }
        case APE_OBJECT_NATIVE_FUNCTION:
        {
            allocator_free(data->mem->alloc, data->native_function.name);
            break;
        }
        case APE_OBJECT_EXTERNAL:
        {
            if(data->external.data_destroy_fn)
            {
                data->external.data_destroy_fn(data->external.data);
            }
            break;
        }
        case APE_OBJECT_ERROR:
        {
            allocator_free(data->mem->alloc, data->error.message);
            traceback_destroy(data->error.traceback);
            break;
        }
        default:
        {
            break;
        }
    }
    data->type = APE_OBJECT_FREED;
}

bool object_is_allocated(Object object)
{
    return (object.handle & OBJECT_ALLOCATED_HEADER) == OBJECT_ALLOCATED_HEADER;
}

GCMemory* object_get_mem(Object obj)
{
    ObjectData* data = object_get_allocated_data(obj);
    return data->mem;
}

bool object_is_hashable(Object obj)
{
    ObjectType type = object_get_type(obj);
    switch(type)
    {
        case APE_OBJECT_STRING:
            return true;
        case APE_OBJECT_NUMBER:
            return true;
        case APE_OBJECT_BOOL:
            return true;
        default:
            return false;
    }
}



const char* object_get_type_name(const ObjectType type)
{
    switch(type)
    {
        case APE_OBJECT_NONE:
            return "NONE";
        case APE_OBJECT_FREED:
            return "NONE";
        case APE_OBJECT_NUMBER:
            return "NUMBER";
        case APE_OBJECT_BOOL:
            return "BOOL";
        case APE_OBJECT_STRING:
            return "STRING";
        case APE_OBJECT_NULL:
            return "NULL";
        case APE_OBJECT_NATIVE_FUNCTION:
            return "NATIVE_FUNCTION";
        case APE_OBJECT_ARRAY:
            return "ARRAY";
        case APE_OBJECT_MAP:
            return "MAP";
        case APE_OBJECT_FUNCTION:
            return "FUNCTION";
        case APE_OBJECT_EXTERNAL:
            return "EXTERNAL";
        case APE_OBJECT_ERROR:
            return "ERROR";
        case APE_OBJECT_ANY:
            return "ANY";
    }
    return "NONE";
}

char* object_get_type_union_name(Allocator* alloc, const ObjectType type)
{
    if(type == APE_OBJECT_ANY || type == APE_OBJECT_NONE || type == APE_OBJECT_FREED)
    {
        return ape_strdup(alloc, object_get_type_name(type));
    }
    StringBuffer* res = strbuf_make(alloc);
    if(!res)
    {
        return NULL;
    }
    bool in_between = false;
#define CHECK_TYPE(t)                                    \
    do                                                   \
    {                                                    \
        if((type & t) == t)                              \
        {                                                \
            if(in_between)                               \
            {                                            \
                strbuf_append(res, "|");                 \
            }                                            \
            strbuf_append(res, object_get_type_name(t)); \
            in_between = true;                           \
        }                                                \
    } while(0)

    CHECK_TYPE(APE_OBJECT_NUMBER);
    CHECK_TYPE(APE_OBJECT_BOOL);
    CHECK_TYPE(APE_OBJECT_STRING);
    CHECK_TYPE(APE_OBJECT_NULL);
    CHECK_TYPE(APE_OBJECT_NATIVE_FUNCTION);
    CHECK_TYPE(APE_OBJECT_ARRAY);
    CHECK_TYPE(APE_OBJECT_MAP);
    CHECK_TYPE(APE_OBJECT_FUNCTION);
    CHECK_TYPE(APE_OBJECT_EXTERNAL);
    CHECK_TYPE(APE_OBJECT_ERROR);

    return strbuf_get_string_and_destroy(res);
}

char* object_serialize(Allocator* alloc, Object object, size_t* lendest)
{
    size_t l;
    char* string;
    StringBuffer* buf;
    buf = strbuf_make(alloc);
    if(!buf)
    {
        return NULL;
    }
    object_to_string(object, buf, true);
    l = buf->len;
    string = strbuf_get_string_and_destroy(buf);
    if(lendest != NULL)
    {
        *lendest = l;
    }
    return string;
}

Object object_deep_copy(GCMemory* mem, Object obj)
{
    ValDictionary* copies = ValDictionary::make<Object, Object>(mem->alloc);
    if(!copies)
    {
        return object_make_null();
    }
    Object res = object_deep_copy_internal(mem, obj, copies);
    copies->destroy();
    return res;
}

Object object_copy(GCMemory* mem, Object obj)
{
    Object copy = object_make_null();
    ObjectType type = object_get_type(obj);
    switch(type)
    {
        case APE_OBJECT_ANY:
        case APE_OBJECT_FREED:
        case APE_OBJECT_NONE:
        {
            APE_ASSERT(false);
            copy = object_make_null();
            break;
        }
        case APE_OBJECT_NUMBER:
        case APE_OBJECT_BOOL:
        case APE_OBJECT_NULL:
        case APE_OBJECT_FUNCTION:
        case APE_OBJECT_NATIVE_FUNCTION:
        case APE_OBJECT_ERROR:
        {
            copy = obj;
            break;
        }
        case APE_OBJECT_STRING:
        {
            const char* str = object_get_string(obj);
            copy = object_make_string(mem, str);
            break;
        }
        case APE_OBJECT_ARRAY:
        {
            int len = object_get_array_length(obj);
            copy = object_make_array_with_capacity(mem, len);
            if(object_is_null(copy))
            {
                return object_make_null();
            }
            for(int i = 0; i < len; i++)
            {
                Object item = object_get_array_value(obj, i);
                bool ok = object_add_array_value(copy, item);
                if(!ok)
                {
                    return object_make_null();
                }
            }
            break;
        }
        case APE_OBJECT_MAP:
        {
            copy = object_make_map(mem);
            for(int i = 0; i < object_get_map_length(obj); i++)
            {
                Object key = (Object)object_get_map_key_at(obj, i);
                Object val = (Object)object_get_map_value_at(obj, i);
                bool ok = object_set_map_value_object(copy, key, val);
                if(!ok)
                {
                    return object_make_null();
                }
            }
            break;
        }
        case APE_OBJECT_EXTERNAL:
        {
            copy = object_make_external(mem, NULL);
            if(object_is_null(copy))
            {
                return object_make_null();
            }
            ExternalData* external = object_get_external_data(obj);
            void* data_copy = NULL;
            if(external->data_copy_fn)
            {
                data_copy = external->data_copy_fn(external->data);
            }
            else
            {
                data_copy = external->data;
            }
            object_set_external_data(copy, data_copy);
            object_set_external_destroy_function(copy, external->data_destroy_fn);
            object_set_external_copy_function(copy, external->data_copy_fn);
            break;
        }
    }
    return copy;
}

double object_compare(Object a, Object b, bool* out_ok)
{
    if(a.handle == b.handle)
    {
        return 0;
    }

    *out_ok = true;

    ObjectType a_type = object_get_type(a);
    ObjectType b_type = object_get_type(b);

    if((a_type == APE_OBJECT_NUMBER || a_type == APE_OBJECT_BOOL || a_type == APE_OBJECT_NULL)
       && (b_type == APE_OBJECT_NUMBER || b_type == APE_OBJECT_BOOL || b_type == APE_OBJECT_NULL))
    {
        double left_val = object_get_number(a);
        double right_val = object_get_number(b);
        return left_val - right_val;
    }
    else if(a_type == b_type && a_type == APE_OBJECT_STRING)
    {
        int a_len = object_get_string_length(a);
        int b_len = object_get_string_length(b);
        if(a_len != b_len)
        {
            return a_len - b_len;
        }
        unsigned long a_hash = object_get_string_hash(a);
        unsigned long b_hash = object_get_string_hash(b);
        if(a_hash != b_hash)
        {
            return a_hash - b_hash;
        }
        const char* a_string = object_get_string(a);
        const char* b_string = object_get_string(b);
        return strcmp(a_string, b_string);
    }
    else if((object_is_allocated(a) || object_is_null(a)) && (object_is_allocated(b) || object_is_null(b)))
    {
        intptr_t a_data_val = (intptr_t)object_get_allocated_data(a);
        intptr_t b_data_val = (intptr_t)object_get_allocated_data(b);
        return (double)(a_data_val - b_data_val);
    }
    else
    {
        *out_ok = false;
    }
    return 1;
}

bool object_equals(Object a, Object b)
{
    ObjectType a_type = object_get_type(a);
    ObjectType b_type = object_get_type(b);

    if(a_type != b_type)
    {
        return false;
    }
    bool ok = false;
    double res = object_compare(a, b, &ok);
    return APE_DBLEQ(res, 0);
}

ExternalData* object_get_external_data(Object object)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_EXTERNAL);
    ObjectData* data = object_get_allocated_data(object);
    return &data->external;
}

bool object_set_external_destroy_function(Object object, ExternalDataDestroyFNCallback destroy_fn)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_EXTERNAL);
    ExternalData* data = object_get_external_data(object);
    if(!data)
    {
        return false;
    }
    data->data_destroy_fn = destroy_fn;
    return true;
}

ObjectData* object_get_allocated_data(Object object)
{
    APE_ASSERT(object_is_allocated(object) || object_get_type(object) == APE_OBJECT_NULL);
    return (ObjectData*)(object.handle & ~OBJECT_HEADER_MASK);
}

bool object_get_bool(Object obj)
{
    if(object_is_number(obj))
    {
        return obj.handle;
    }
    return obj.handle & (~OBJECT_HEADER_MASK);
}

double object_get_number(Object obj)
{
    if(object_is_number(obj))
    {// todo: optimise? always return number?
        return obj.number;
    }
    return (double)(obj.handle & (~OBJECT_HEADER_MASK));
}

const char* object_get_string(Object object)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_STRING);
    ObjectData* data = object_get_allocated_data(object);
    return object_data_get_string(data);
}

int object_get_string_length(Object object)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_STRING);
    ObjectData* data = object_get_allocated_data(object);
    return data->string.length;
}

void object_set_string_length(Object object, int len)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_STRING);
    ObjectData* data = object_get_allocated_data(object);
    data->string.length = len;
}


int object_get_string_capacity(Object object)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_STRING);
    ObjectData* data = object_get_allocated_data(object);
    return data->string.capacity;
}

char* object_get_mutable_string(Object object)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_STRING);
    ObjectData* data = object_get_allocated_data(object);
    return object_data_get_string(data);
}

bool object_string_append(Object obj, const char* src, int len)
{
    APE_ASSERT(object_get_type(obj) == APE_OBJECT_STRING);
    ObjectData* data = object_get_allocated_data(obj);
    ObjectString* string = &data->string;
    char* str_buf = object_get_mutable_string(obj);
    int current_len = string->length;
    int capacity = string->capacity;
    if((len + current_len) > capacity)
    {
        APE_ASSERT(false);
        return false;
    }
    memcpy(str_buf + current_len, src, len);
    string->length += len;
    str_buf[string->length] = '\0';
    return true;
}

unsigned long object_get_string_hash(Object obj)
{
    APE_ASSERT(object_get_type(obj) == APE_OBJECT_STRING);
    ObjectData* data = object_get_allocated_data(obj);
    if(data->string.hash == 0)
    {
        data->string.hash = object_hash_string(object_get_string(obj));
        if(data->string.hash == 0)
        {
            data->string.hash = 1;
        }
    }
    return data->string.hash;
}

FunctionObject* object_get_function(Object object)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_FUNCTION);
    ObjectData* data = object_get_allocated_data(object);
    return &data->function;
}

NativeFunction* object_get_native_function(Object obj)
{
    ObjectData* data = object_get_allocated_data(obj);
    return &data->native_function;
}

ObjectType object_get_type(Object obj)
{
    if(object_is_number(obj))
    {
        return APE_OBJECT_NUMBER;
    }
    uint64_t tag = (obj.handle >> 48) & 0x7;
    switch(tag)
    {
        case 0:
            return APE_OBJECT_NONE;
        case 1:
            return APE_OBJECT_BOOL;
        case 2:
            return APE_OBJECT_NULL;
        case 4:
        {
            ObjectData* data = object_get_allocated_data(obj);
            return data->type;
        }
        default:
            return APE_OBJECT_NONE;
    }
}

bool object_is_numeric(Object obj)
{
    ObjectType type = object_get_type(obj);
    return type == APE_OBJECT_NUMBER || type == APE_OBJECT_BOOL;
}

bool object_is_null(Object obj)
{
    return object_get_type(obj) == APE_OBJECT_NULL;
}

bool object_is_callable(Object obj)
{
    ObjectType type = object_get_type(obj);
    return type == APE_OBJECT_NATIVE_FUNCTION || type == APE_OBJECT_FUNCTION;
}

const char* object_get_function_name(Object obj)
{
    APE_ASSERT(object_get_type(obj) == APE_OBJECT_FUNCTION);
    ObjectData* data = object_get_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return NULL;
    }

    if(data->function.owns_data)
    {
        return data->function.name;
    }
    else
    {
        return data->function.const_name;
    }
}

Object object_get_function_free_val(Object obj, int ix)
{
    APE_ASSERT(object_get_type(obj) == APE_OBJECT_FUNCTION);
    ObjectData* data = object_get_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return object_make_null();
    }
    FunctionObject* fun = &data->function;
    APE_ASSERT(ix >= 0 && ix < fun->free_vals_count);
    if(ix < 0 || ix >= fun->free_vals_count)
    {
        return object_make_null();
    }
    if(freevals_are_allocated(fun))
    {
        return fun->free_vals_allocated[ix];
    }
    else
    {
        return fun->free_vals_buf[ix];
    }
}

void object_set_function_free_val(Object obj, int ix, Object val)
{
    APE_ASSERT(object_get_type(obj) == APE_OBJECT_FUNCTION);
    ObjectData* data = object_get_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return;
    }
    FunctionObject* fun = &data->function;
    APE_ASSERT(ix >= 0 && ix < fun->free_vals_count);
    if(ix < 0 || ix >= fun->free_vals_count)
    {
        return;
    }
    if(freevals_are_allocated(fun))
    {
        fun->free_vals_allocated[ix] = val;
    }
    else
    {
        fun->free_vals_buf[ix] = val;
    }
}

Object* object_get_function_free_vals(Object obj)
{
    ObjectData* data;
    APE_ASSERT(object_get_type(obj) == APE_OBJECT_FUNCTION);
    data = object_get_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return NULL;
    }
    FunctionObject* fun = &data->function;
    if(freevals_are_allocated(fun))
    {
        return fun->free_vals_allocated;
    }
    else
    {
        return fun->free_vals_buf;
    }
}

const char* object_get_error_message(Object object)
{
    ObjectData* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ERROR);
    data = object_get_allocated_data(object);
    return data->error.message;
}

void object_set_error_traceback(Object object, Traceback* traceback)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ERROR);
    if(object_get_type(object) != APE_OBJECT_ERROR)
    {
        return;
    }
    ObjectData* data = object_get_allocated_data(object);
    APE_ASSERT(data->error.traceback == NULL);
    data->error.traceback = traceback;
}

Traceback* object_get_error_traceback(Object object)
{
    ObjectData* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ERROR);
    data = object_get_allocated_data(object);
    return data->error.traceback;
}

bool object_set_external_data(Object object, void* ext_data)
{
    ExternalData* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_EXTERNAL);
    data = object_get_external_data(object);
    if(!data)
    {
        return false;
    }
    data->data = ext_data;
    return true;
}

bool object_set_external_copy_function(Object object, ExternalDataCopyFNCallback copy_fn)
{
    ExternalData* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_EXTERNAL);
    data = object_get_external_data(object);
    if(!data)
    {
        return false;
    }
    data->data_copy_fn = copy_fn;
    return true;
}

Object object_get_array_value(Object object, int ix)
{
    Object* res;
    Array* array;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ARRAY);
    array = object_get_allocated_array(object);
    if(ix < 0 || ix >= array->count())
    {
        return object_make_null();
    }
    res = (Object*)array->get(ix);
    if(!res)
    {
        return object_make_null();
    }
    return *res;
}

bool object_set_array_value_at(Object object, int ix, Object val)
{
    Array* array;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ARRAY);
    array = object_get_allocated_array(object);
    if(ix < 0 || ix >= array->count())
    {
        return false;
    }
    return array->set(ix, &val);
}

bool object_add_array_value(Object object, Object val)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ARRAY);
    Array* array = object_get_allocated_array(object);
    return array->add(&val);
}

int object_get_array_length(Object object)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ARRAY);
    Array* array = object_get_allocated_array(object);
    return array->count();
}

bool object_remove_array_value_at(Object object, int ix)
{
    Array* array = object_get_allocated_array(object);
    return array->removeAt(ix);
}

int object_get_map_length(Object object)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    ObjectData* data = object_get_allocated_data(object);
    return data->map->count();
}

Object object_get_map_key_at(Object object, int ix)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    ObjectData* data = object_get_allocated_data(object);
    Object* res = (Object*)data->map->getKeyAt(ix);
    if(!res)
    {
        return object_make_null();
    }
    return *res;
}

Object object_get_map_value_at(Object object, int ix)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    ObjectData* data = object_get_allocated_data(object);
    Object* res = (Object*)data->map->getValueAt(ix);
    if(!res)
    {
        return object_make_null();
    }
    return *res;
}

bool object_set_map_value_at(Object object, int ix, Object val)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    if(ix >= object_get_map_length(object))
    {
        return false;
    }
    ObjectData* data = object_get_allocated_data(object);
    return data->map->setValueAt(ix, &val);
}

Object object_get_kv_pair_at(GCMemory* mem, Object object, int ix)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    ObjectData* data = object_get_allocated_data(object);
    if(ix >= data->map->count())
    {
        return object_make_null();
    }
    Object key = object_get_map_key_at(object, ix);
    Object val = object_get_map_value_at(object, ix);
    Object res = object_make_map(mem);
    if(object_is_null(res))
    {
        return object_make_null();
    }

    Object key_obj = object_make_string(mem, "key");
    if(object_is_null(key_obj))
    {
        return object_make_null();
    }
    object_set_map_value_object(res, key_obj, key);

    Object val_obj = object_make_string(mem, "value");
    if(object_is_null(val_obj))
    {
        return object_make_null();
    }
    object_set_map_value_object(res, val_obj, val);

    return res;
}

bool object_set_map_value_object(Object object, Object key, Object val)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    ObjectData* data = object_get_allocated_data(object);
    return data->map->set(&key, &val);
}

Object object_get_map_value_object(Object object, Object key)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    ObjectData* data = object_get_allocated_data(object);
    Object* res = (Object*)data->map->get(&key);
    if(!res)
    {
        return object_make_null();
    }
    return *res;
}

bool object_map_has_key_object(Object object, Object key)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    ObjectData* data = object_get_allocated_data(object);
    Object* res = (Object*)data->map->get( &key);
    return res != NULL;
}

// INTERNAL
Object object_deep_copy_internal(GCMemory* mem, Object obj, ValDictionary * copies)
{
    Object* copy_ptr = (Object*)copies->get(&obj);
    if(copy_ptr)
    {
        return *copy_ptr;
    }

    Object copy = object_make_null();

    ObjectType type = object_get_type(obj);
    switch(type)
    {
        case APE_OBJECT_FREED:
        case APE_OBJECT_ANY:
        case APE_OBJECT_NONE:
            {
                APE_ASSERT(false);
                copy = object_make_null();
            }
            break;

        case APE_OBJECT_NUMBER:
        case APE_OBJECT_BOOL:
        case APE_OBJECT_NULL:
        case APE_OBJECT_NATIVE_FUNCTION:
            {
                copy = obj;
            }
            break;

        case APE_OBJECT_STRING:
            {
                const char* str = object_get_string(obj);
                copy = object_make_string(mem, str);
            }
            break;

        case APE_OBJECT_FUNCTION:
            {
                FunctionObject* function = object_get_function(obj);
                uint8_t* bytecode_copy = NULL;
                Position* src_positions_copy = NULL;
                CompilationResult* comp_res_copy = NULL;

                bytecode_copy = (uint8_t*)allocator_malloc(mem->alloc, sizeof(uint8_t) * function->comp_result->m_count);
                if(!bytecode_copy)
                {
                    return object_make_null();
                }
                memcpy(bytecode_copy, function->comp_result->bytecode, sizeof(uint8_t) * function->comp_result->m_count);

                src_positions_copy = (Position*)allocator_malloc(mem->alloc, sizeof(Position) * function->comp_result->m_count);
                if(!src_positions_copy)
                {
                    allocator_free(mem->alloc, bytecode_copy);
                    return object_make_null();
                }
                memcpy(src_positions_copy, function->comp_result->src_positions, sizeof(Position) * function->comp_result->m_count);

                comp_res_copy = compilation_result_make(mem->alloc, bytecode_copy, src_positions_copy,
                                                        function->comp_result->m_count);// todo: add compilation result copy function
                if(!comp_res_copy)
                {
                    allocator_free(mem->alloc, src_positions_copy);
                    allocator_free(mem->alloc, bytecode_copy);
                    return object_make_null();
                }

                copy = object_make_function(mem, object_get_function_name(obj), comp_res_copy, true, function->num_locals,
                                            function->num_args, 0);
                if(object_is_null(copy))
                {
                    compilation_result_destroy(comp_res_copy);
                    return object_make_null();
                }

                bool ok = copies->set(&obj, &copy);
                if(!ok)
                {
                    return object_make_null();
                }

                FunctionObject* function_copy = object_get_function(copy);
                if(freevals_are_allocated(function))
                {
                    function_copy->free_vals_allocated = (Object*)allocator_malloc(mem->alloc, sizeof(Object) * function->free_vals_count);
                    if(!function_copy->free_vals_allocated)
                    {
                        return object_make_null();
                    }
                }

                function_copy->free_vals_count = function->free_vals_count;
                for(int i = 0; i < function->free_vals_count; i++)
                {
                    Object free_val = object_get_function_free_val(obj, i);
                    Object free_val_copy = object_deep_copy_internal(mem, free_val, copies);
                    if(!object_is_null(free_val) && object_is_null(free_val_copy))
                    {
                        return object_make_null();
                    }
                    object_set_function_free_val(copy, i, free_val_copy);
                }
            }
            break;

        case APE_OBJECT_ARRAY:
            {
                int len = object_get_array_length(obj);
                copy = object_make_array_with_capacity(mem, len);
                if(object_is_null(copy))
                {
                    return object_make_null();
                }
                bool ok = copies->set(&obj, &copy);
                if(!ok)
                {
                    return object_make_null();
                }
                for(int i = 0; i < len; i++)
                {
                    Object item = object_get_array_value(obj, i);
                    Object item_copy = object_deep_copy_internal(mem, item, copies);
                    if(!object_is_null(item) && object_is_null(item_copy))
                    {
                        return object_make_null();
                    }
                    bool ok = object_add_array_value(copy, item_copy);
                    if(!ok)
                    {
                        return object_make_null();
                    }
                }
            }
            break;

        case APE_OBJECT_MAP:
            {
                copy = object_make_map(mem);
                if(object_is_null(copy))
                {
                    return object_make_null();
                }
                bool ok = copies->set( &obj, &copy);
                if(!ok)
                {
                    return object_make_null();
                }
                for(int i = 0; i < object_get_map_length(obj); i++)
                {
                    Object key = object_get_map_key_at(obj, i);
                    Object val = object_get_map_value_at(obj, i);

                    Object key_copy = object_deep_copy_internal(mem, key, copies);
                    if(!object_is_null(key) && object_is_null(key_copy))
                    {
                        return object_make_null();
                    }

                    Object val_copy = object_deep_copy_internal(mem, val, copies);
                    if(!object_is_null(val) && object_is_null(val_copy))
                    {
                        return object_make_null();
                    }

                    bool ok = object_set_map_value_object(copy, key_copy, val_copy);
                    if(!ok)
                    {
                        return object_make_null();
                    }
                }
            }
            break;

        case APE_OBJECT_EXTERNAL:
            {
                copy = object_copy(mem, obj);
            }
            break;

        case APE_OBJECT_ERROR:
            {
                copy = obj;
            }
            break;

    }
    return copy;
}


bool object_equals_wrapped(const Object* a_ptr, const Object* b_ptr)
{
    Object a = *a_ptr;
    Object b = *b_ptr;
    return object_equals(a, b);
}

unsigned long object_hash(Object* obj_ptr)
{
    Object obj = *obj_ptr;
    ObjectType type = object_get_type(obj);

    switch(type)
    {
        case APE_OBJECT_NUMBER:
        {
            double val = object_get_number(obj);
            return object_hash_double(val);
        }
        case APE_OBJECT_BOOL:
        {
            bool val = object_get_bool(obj);
            return val;
        }
        case APE_OBJECT_STRING:
        {
            return object_get_string_hash(obj);
        }
        default:
        {
            return 0;
        }
    }
}

unsigned long object_hash_string(const char* str)
{ /* djb2 */
    unsigned long hash = 5381;
    int c;
    while((c = *str++))
    {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

unsigned long object_hash_double(double val)
{ /* djb2 */
    uint32_t* val_ptr = (uint32_t*)&val;
    unsigned long hash = 5381;
    hash = ((hash << 5) + hash) + val_ptr[0];
    hash = ((hash << 5) + hash) + val_ptr[1];
    return hash;
}

Array * object_get_allocated_array(Object object)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ARRAY);
    ObjectData* data = object_get_allocated_data(object);
    return data->array;
}

bool object_is_number(Object o)
{
    return (o.handle & OBJECT_PATTERN) != OBJECT_PATTERN;
}

static uint64_t get_type_tag(ObjectType type)
{
    switch(type)
    {
        case APE_OBJECT_NONE:
            return 0;
        case APE_OBJECT_BOOL:
            return 1;
        case APE_OBJECT_NULL:
            return 2;
        default:
            return 4;
    }
}

static bool freevals_are_allocated(FunctionObject* fun)
{
    return fun->free_vals_count >= APE_ARRAY_LEN(fun->free_vals_buf);
}

char* object_data_get_string(ObjectData* data)
{
    APE_ASSERT(data->type == APE_OBJECT_STRING);
    if(data->string.is_allocated)
    {
        return data->string.value_allocated;
    }
    else
    {
        return data->string.value_buf;
    }
}

bool object_data_string_reserve_capacity(ObjectData* data, int capacity)
{
    APE_ASSERT(capacity >= 0);

    ObjectString* string = &data->string;

    string->length = 0;
    string->hash = 0;

    if(capacity <= string->capacity)
    {
        return true;
    }

    if(capacity <= (OBJECT_STRING_BUF_SIZE - 1))
    {
        if(string->is_allocated)
        {
            APE_ASSERT(false);// should never happen
            allocator_free(data->mem->alloc, string->value_allocated);// just in case
        }
        string->capacity = OBJECT_STRING_BUF_SIZE - 1;
        string->is_allocated = false;
        return true;
    }

    char* new_value = (char*)allocator_malloc(data->mem->alloc, capacity + 1);
    if(!new_value)
    {
        return false;
    }

    if(string->is_allocated)
    {
        allocator_free(data->mem->alloc, string->value_allocated);
    }

    string->value_allocated = new_value;
    string->is_allocated = true;
    string->capacity = capacity;
    return true;
}

ObjectDataPool* get_pool_for_type(GCMemory* mem, ObjectType type);
bool can_data_be_put_in_pool(GCMemory* mem, ObjectData* data);

GCMemory* gcmem_make(Allocator* alloc)
{
    int i;
    GCMemory* mem = (GCMemory*)allocator_malloc(alloc, sizeof(GCMemory));
    if(!mem)
    {
        return NULL;
    }
    memset(mem, 0, sizeof(GCMemory));
    mem->alloc = alloc;
    mem->objects = PtrArray::make(alloc);
    if(!mem->objects)
    {
        goto error;
    }
    mem->objects_back = PtrArray::make(alloc);
    if(!mem->objects_back)
    {
        goto error;
    }
    mem->objects_not_gced = Array::make<Object>(alloc);
    if(!mem->objects_not_gced)
    {
        goto error;
    }
    mem->allocations_since_sweep = 0;
    mem->data_only_pool.m_count = 0;
    for(i = 0; i < GCMEM_POOLS_NUM; i++)
    {
        ObjectDataPool* pool = &mem->pools[i];
        mem->pools[i].m_count = 0;
        memset(pool, 0, sizeof(ObjectDataPool));
    }

    return mem;
error:
    gcmem_destroy(mem);
    return NULL;
}

void gcmem_destroy(GCMemory* mem)
{
    if(!mem)
    {
        return;
    }

    Array::destroy(mem->objects_not_gced);
    mem->objects_back->destroy();

    for(int i = 0; i < mem->objects->count(); i++)
    {
        ObjectData* obj = (ObjectData*)mem->objects->get(i);
        object_data_deinit(obj);
        allocator_free(mem->alloc, obj);
    }
    mem->objects->destroy();

    for(int i = 0; i < GCMEM_POOLS_NUM; i++)
    {
        ObjectDataPool* pool = &mem->pools[i];
        for(int j = 0; j < pool->m_count; j++)
        {
            ObjectData* data = pool->datapool[j];
            object_data_deinit(data);
            allocator_free(mem->alloc, data);
        }
        memset(pool, 0, sizeof(ObjectDataPool));
    }

    for(int i = 0; i < mem->data_only_pool.m_count; i++)
    {
        allocator_free(mem->alloc, mem->data_only_pool.datapool[i]);
    }

    allocator_free(mem->alloc, mem);
}

ObjectData* gcmem_alloc_object_data(GCMemory* mem, ObjectType type)
{
    ObjectData* data = NULL;
    mem->allocations_since_sweep++;
    if(mem->data_only_pool.m_count > 0)
    {
        data = mem->data_only_pool.datapool[mem->data_only_pool.m_count - 1];
        mem->data_only_pool.m_count--;
    }
    else
    {
        data = (ObjectData*)allocator_malloc(mem->alloc, sizeof(ObjectData));
        if(!data)
        {
            return NULL;
        }
    }

    memset(data, 0, sizeof(ObjectData));

    APE_ASSERT(mem->objects_back->count() >= mem->objects->count());
    // we want to make sure that appending to objects_back never fails in sweep
    // so this only reserves space there.
    bool ok = mem->objects_back->add(data);
    if(!ok)
    {
        allocator_free(mem->alloc, data);
        return NULL;
    }
    ok = mem->objects->add(data);
    if(!ok)
    {
        allocator_free(mem->alloc, data);
        return NULL;
    }
    data->mem = mem;
    data->type = type;
    return data;
}

ObjectData* gcmem_get_object_data_from_pool(GCMemory* mem, ObjectType type)
{
    ObjectDataPool* pool = get_pool_for_type(mem, type);
    if(!pool || pool->m_count <= 0)
    {
        return NULL;
    }
    ObjectData* data = pool->datapool[pool->m_count - 1];

    APE_ASSERT(mem->objects_back->count() >= mem->objects->count());

    // we want to make sure that appending to objects_back never fails in sweep
    // so this only reserves space there.
    bool ok = mem->objects_back->add(data);
    if(!ok)
    {
        return NULL;
    }
    ok = mem->objects->add(data);
    if(!ok)
    {
        return NULL;
    }

    pool->m_count--;

    return data;
}

void gc_unmark_all(GCMemory* mem)
{
    for(int i = 0; i < mem->objects->count(); i++)
    {
        ObjectData* data = (ObjectData*)mem->objects->get(i);
        data->gcmark = false;
    }
}

void gc_mark_objects(Object* objects, int cn)
{
    for(int i = 0; i < cn; i++)
    {
        Object obj = objects[i];
        gc_mark_object(obj);
    }
}

void gc_mark_object(Object obj)
{
    if(!object_is_allocated(obj))
    {
        return;
    }

    ObjectData* data = object_get_allocated_data(obj);
    if(data->gcmark)
    {
        return;
    }

    data->gcmark = true;
    switch(data->type)
    {
        case APE_OBJECT_MAP:
        {
            int len = object_get_map_length(obj);
            for(int i = 0; i < len; i++)
            {
                Object key = object_get_map_key_at(obj, i);
                if(object_is_allocated(key))
                {
                    ObjectData* key_data = object_get_allocated_data(key);
                    if(!key_data->gcmark)
                    {
                        gc_mark_object(key);
                    }
                }
                Object val = object_get_map_value_at(obj, i);
                if(object_is_allocated(val))
                {
                    ObjectData* val_data = object_get_allocated_data(val);
                    if(!val_data->gcmark)
                    {
                        gc_mark_object(val);
                    }
                }
            }
            break;
        }
        case APE_OBJECT_ARRAY:
        {
            int len = object_get_array_length(obj);
            for(int i = 0; i < len; i++)
            {
                Object val = object_get_array_value(obj, i);
                if(object_is_allocated(val))
                {
                    ObjectData* val_data = object_get_allocated_data(val);
                    if(!val_data->gcmark)
                    {
                        gc_mark_object(val);
                    }
                }
            }
            break;
        }
        case APE_OBJECT_FUNCTION:
        {
            FunctionObject* function = object_get_function(obj);
            for(int i = 0; i < function->free_vals_count; i++)
            {
                Object free_val = object_get_function_free_val(obj, i);
                gc_mark_object(free_val);
                if(object_is_allocated(free_val))
                {
                    ObjectData* free_val_data = object_get_allocated_data(free_val);
                    if(!free_val_data->gcmark)
                    {
                        gc_mark_object(free_val);
                    }
                }
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

void gc_sweep(GCMemory* mem)
{
    int i;
    bool ok;
    gc_mark_objects((Object*)mem->objects_not_gced->data(), mem->objects_not_gced->count());

    APE_ASSERT(mem->objects_back->count() >= mem->objects->count());

    mem->objects_back->clear();
    for(i = 0; i < mem->objects->count(); i++)
    {
        ObjectData* data = (ObjectData*)mem->objects->get(i);
        if(data->gcmark)
        {
            // this should never fail because objects_back's size should be equal to objects
            ok = mem->objects_back->add(data);
            (void)ok;
            APE_ASSERT(ok);
        }
        else
        {
            if(can_data_be_put_in_pool(mem, data))
            {
                ObjectDataPool* pool = get_pool_for_type(mem, data->type);
                pool->datapool[pool->m_count] = data;
                pool->m_count++;
            }
            else
            {
                object_data_deinit(data);
                if(mem->data_only_pool.m_count < GCMEM_POOL_SIZE)
                {
                    mem->data_only_pool.datapool[mem->data_only_pool.m_count] = data;
                    mem->data_only_pool.m_count++;
                }
                else
                {
                    allocator_free(mem->alloc, data);
                }
            }
        }
    }
    PtrArray* objs_temp = mem->objects;
    mem->objects = mem->objects_back;
    mem->objects_back = objs_temp;
    mem->allocations_since_sweep = 0;
}

bool gc_disable_on_object(Object obj)
{
    if(!object_is_allocated(obj))
    {
        return false;
    }
    ObjectData* data = object_get_allocated_data(obj);
    if(data->mem->objects_not_gced->contains(&obj))
    {
        return false;
    }
    bool ok = data->mem->objects_not_gced->add(&obj);
    return ok;
}

void gc_enable_on_object(Object obj)
{
    if(!object_is_allocated(obj))
    {
        return;
    }
    ObjectData* data = object_get_allocated_data(obj);
    data->mem->objects_not_gced->removeItem(&obj);
}

int gc_should_sweep(GCMemory* mem)
{
    return mem->allocations_since_sweep > GCMEM_SWEEP_INTERVAL;
}

// INTERNAL
ObjectDataPool* get_pool_for_type(GCMemory* mem, ObjectType type)
{
    switch(type)
    {
        case APE_OBJECT_ARRAY:
            return &mem->pools[0];
        case APE_OBJECT_MAP:
            return &mem->pools[1];
        case APE_OBJECT_STRING:
            return &mem->pools[2];
        default:
            return NULL;
    }
}

bool can_data_be_put_in_pool(GCMemory* mem, ObjectData* data)
{
    Object obj = object_make_from_data(data->type, data);

    // this is to ensure that large objects won't be kept in pool indefinitely
    switch(data->type)
    {
        case APE_OBJECT_ARRAY:
        {
            if(object_get_array_length(obj) > 1024)
            {
                return false;
            }
            break;
        }
        case APE_OBJECT_MAP:
        {
            if(object_get_map_length(obj) > 1024)
            {
                return false;
            }
            break;
        }
        case APE_OBJECT_STRING:
        {
            if(!data->string.is_allocated || data->string.capacity > 4096)
            {
                return false;
            }
            break;
        }
        default:
            break;
    }

    ObjectDataPool* pool = get_pool_for_type(mem, data->type);
    if(!pool || pool->m_count >= GCMEM_POOL_SIZE)
    {
        return false;
    }

    return true;
}


Traceback* traceback_make(Allocator* alloc)
{
    Traceback* traceback = (Traceback*)allocator_malloc(alloc, sizeof(Traceback));
    if(!traceback)
    {
        return NULL;
    }
    memset(traceback, 0, sizeof(Traceback));
    traceback->alloc = alloc;
    traceback->items = Array::make<TracebackItem>(alloc);
    if(!traceback->items)
    {
        traceback_destroy(traceback);
        return NULL;
    }
    return traceback;
}

void traceback_destroy(Traceback* traceback)
{
    if(!traceback)
    {
        return;
    }
    for(int i = 0; i < traceback->items->count(); i++)
    {
        TracebackItem* item = (TracebackItem*)traceback->items->get(i);
        allocator_free(traceback->alloc, item->function_name);
    }
    Array::destroy(traceback->items);
    allocator_free(traceback->alloc, traceback);
}

bool traceback_append(Traceback* traceback, const char* function_name, Position pos)
{
    TracebackItem item;
    item.function_name = ape_strdup(traceback->alloc, function_name);
    if(!item.function_name)
    {
        return false;
    }
    item.pos = pos;
    bool ok = traceback->items->add(&item);
    if(!ok)
    {
        allocator_free(traceback->alloc, item.function_name);
        return false;
    }
    return true;
}

bool traceback_append_from_vm(Traceback* traceback, VM* vm)
{
    for(int i = vm->frames_count - 1; i >= 0; i--)
    {
        Frame* frame = &vm->frames[i];
        bool ok = traceback_append(traceback, object_get_function_name(frame->function), frame_src_position(frame));
        if(!ok)
        {
            return false;
        }
    }
    return true;
}



const char* traceback_item_get_line(TracebackItem* item)
{
    if(!item->pos.file)
    {
        return NULL;
    }
    PtrArray* lines = item->pos.file->lines;
    if(item->pos.line >= lines->count())
    {
        return NULL;
    }
    const char* line = (const char*)lines->get(item->pos.line);
    return line;
}

const char* traceback_item_get_filepath(TracebackItem* item)
{
    if(!item->pos.file)
    {
        return NULL;
    }
    return item->pos.file->path;
}

bool frame_init(Frame* frame, Object function_obj, int base_pointer)
{
    if(object_get_type(function_obj) != APE_OBJECT_FUNCTION)
    {
        return false;
    }
    FunctionObject* function = object_get_function(function_obj);
    frame->function = function_obj;
    frame->ip = 0;
    frame->base_pointer = base_pointer;
    frame->src_ip = 0;
    frame->bytecode = function->comp_result->bytecode;
    frame->src_positions = function->comp_result->src_positions;
    frame->bytecode_size = function->comp_result->m_count;
    frame->recover_ip = -1;
    frame->is_recovering = false;
    return true;
}

OpcodeValue frame_read_opcode(Frame* frame)
{
    frame->src_ip = frame->ip;
    return (OpcodeValue)frame_read_uint8(frame);
}

uint64_t frame_read_uint64(Frame* frame)
{
    const uint8_t* data = frame->bytecode + frame->ip;
    frame->ip += 8;
    uint64_t res = 0;
    res |= (uint64_t)data[7];
    res |= (uint64_t)data[6] << 8;
    res |= (uint64_t)data[5] << 16;
    res |= (uint64_t)data[4] << 24;
    res |= (uint64_t)data[3] << 32;
    res |= (uint64_t)data[2] << 40;
    res |= (uint64_t)data[1] << 48;
    res |= (uint64_t)data[0] << 56;
    return res;
}

uint16_t frame_read_uint16(Frame* frame)
{
    const uint8_t* data = frame->bytecode + frame->ip;
    frame->ip += 2;
    return (data[0] << 8) | data[1];
}

uint8_t frame_read_uint8(Frame* frame)
{
    const uint8_t* data = frame->bytecode + frame->ip;
    frame->ip++;
    return data[0];
}

Position frame_src_position(const Frame* frame)
{
    if(frame->src_positions)
    {
        return frame->src_positions[frame->src_ip];
    }
    return src_pos_invalid;
}


VM* vm_make(Allocator* alloc, const Config* config, GCMemory* mem, ErrorList* errors, GlobalStore* global_store)
{
    VM* vm = (VM*)allocator_malloc(alloc, sizeof(VM));
    if(!vm)
    {
        return NULL;
    }
    memset(vm, 0, sizeof(VM));
    vm->alloc = alloc;
    vm->config = config;
    vm->mem = mem;
    vm->errors = errors;
    vm->global_store = global_store;
    vm->globals_count = 0;
    vm->sp = 0;
    vm->this_sp = 0;
    vm->frames_count = 0;
    vm->last_popped = object_make_null();
    vm->running = false;

    for(int i = 0; i < OPCODE_MAX; i++)
    {
        vm->operator_oveload_keys[i] = object_make_null();
    }
#define SET_OPERATOR_OVERLOAD_KEY(op, key)                   \
    do                                                       \
    {                                                        \
        Object key_obj = object_make_string(vm->mem, key); \
        if(object_is_null(key_obj))                          \
        {                                                    \
            goto err;                                        \
        }                                                    \
        vm->operator_oveload_keys[op] = key_obj;             \
    } while(0)
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_ADD, "__operator_add__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_SUB, "__operator_sub__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_MUL, "__operator_mul__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_DIV, "__operator_div__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_MOD, "__operator_mod__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_OR, "__operator_or__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_XOR, "__operator_xor__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_AND, "__operator_and__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_LSHIFT, "__operator_lshift__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_RSHIFT, "__operator_rshift__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_MINUS, "__operator_minus__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_BANG, "__operator_bang__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_COMPARE, "__cmp__");
#undef SET_OPERATOR_OVERLOAD_KEY

    return vm;
err:
    vm_destroy(vm);
    return NULL;
}

void vm_destroy(VM* vm)
{
    if(!vm)
    {
        return;
    }
    allocator_free(vm->alloc, vm);
}

void vm_reset(VM* vm)
{
    vm->sp = 0;
    vm->this_sp = 0;
    while(vm->frames_count > 0)
    {
        pop_frame(vm);
    }
}

bool vm_run(VM* vm, CompilationResult* comp_res, Array * constants)
{
#ifdef APE_DEBUG
    int old_sp = vm->sp;
#endif
    int old_this_sp = vm->this_sp;
    int old_frames_count = vm->frames_count;
    Object main_fn = object_make_function(vm->mem, "main", comp_res, false, 0, 0, 0);
    if(object_is_null(main_fn))
    {
        return false;
    }
    stack_push(vm, main_fn);
    bool res = vm_execute_function(vm, main_fn, constants);
    while(vm->frames_count > old_frames_count)
    {
        pop_frame(vm);
    }
    APE_ASSERT(vm->sp == old_sp);
    vm->this_sp = old_this_sp;
    return res;
}

Object vm_call(VM* vm, Array * constants, Object callee, int argc, Object* args)
{
    ObjectType type = object_get_type(callee);
    if(type == APE_OBJECT_FUNCTION)
    {
#ifdef APE_DEBUG
        int old_sp = vm->sp;
#endif
        int old_this_sp = vm->this_sp;
        int old_frames_count = vm->frames_count;
        stack_push(vm, callee);
        for(int i = 0; i < argc; i++)
        {
            stack_push(vm, args[i]);
        }
        bool ok = vm_execute_function(vm, callee, constants);
        if(!ok)
        {
            return object_make_null();
        }
        while(vm->frames_count > old_frames_count)
        {
            pop_frame(vm);
        }
        APE_ASSERT(vm->sp == old_sp);
        vm->this_sp = old_this_sp;
        return vm_get_last_popped(vm);
    }
    else if(type == APE_OBJECT_NATIVE_FUNCTION)
    {
        return call_native_function(vm, callee, src_pos_invalid, argc, args);
    }
    else
    {
        errors_add_error(vm->errors, APE_ERROR_USER, src_pos_invalid, "Object is not callable");
        return object_make_null();
    }
}

bool vm_execute_function(VM* vm, Object function, Array * constants)
{
    if(vm->running)
    {
        errors_add_error(vm->errors, APE_ERROR_USER, src_pos_invalid, "VM is already executing code");
        return false;
    }

    FunctionObject* function_function = object_get_function(function);// naming is hard
    Frame new_frame;
    bool ok = false;
    ok = frame_init(&new_frame, function, vm->sp - function_function->num_args);
    if(!ok)
    {
        return false;
    }
    ok = push_frame(vm, new_frame);
    if(!ok)
    {
        errors_add_error(vm->errors, APE_ERROR_USER, src_pos_invalid, "Pushing frame failed");
        return false;
    }

    vm->running = true;
    vm->last_popped = object_make_null();

    bool check_time = false;
    double max_exec_time_ms = 0;
    if(vm->config)
    {
        check_time = vm->config->max_execution_time_set;
        max_exec_time_ms = vm->config->max_execution_time_ms;
    }
    unsigned time_check_interval = 1000;
    unsigned time_check_counter = 0;
    Timer timer;
    memset(&timer, 0, sizeof(Timer));
    if(check_time)
    {
        timer = ape_timer_start();
    }

    while(vm->current_frame->ip < vm->current_frame->bytecode_size)
    {
        OpcodeValue opcode = frame_read_opcode(vm->current_frame);
        switch(opcode)
        {
            case OPCODE_CONSTANT:
                {
                    uint16_t constant_ix = frame_read_uint16(vm->current_frame);
                    Object* constant = (Object*)constants->get(constant_ix);
                    if(!constant)
                    {
                        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "Constant at %d not found", constant_ix);
                        goto err;
                    }
                    stack_push(vm, *constant);
                }
                break;

            case OPCODE_ADD:
            case OPCODE_SUB:
            case OPCODE_MUL:
            case OPCODE_DIV:
            case OPCODE_MOD:
            case OPCODE_OR:
            case OPCODE_XOR:
            case OPCODE_AND:
            case OPCODE_LSHIFT:
            case OPCODE_RSHIFT:
                {
                    Object right = stack_pop(vm);
                    Object left = stack_pop(vm);
                    ObjectType left_type = object_get_type(left);
                    ObjectType right_type = object_get_type(right);
                    if(object_is_numeric(left) && object_is_numeric(right))
                    {
                        double right_val = object_get_number(right);
                        double left_val = object_get_number(left);

                        int64_t left_val_int = (int64_t)left_val;
                        int64_t right_val_int = (int64_t)right_val;

                        double res = 0;
                        switch(opcode)
                        {
                            case OPCODE_ADD:
                                res = left_val + right_val;
                                break;
                            case OPCODE_SUB:
                                res = left_val - right_val;
                                break;
                            case OPCODE_MUL:
                                res = left_val * right_val;
                                break;
                            case OPCODE_DIV:
                                res = left_val / right_val;
                                break;
                            case OPCODE_MOD:
                                res = fmod(left_val, right_val);
                                break;
                            case OPCODE_OR:
                                res = (double)(left_val_int | right_val_int);
                                break;
                            case OPCODE_XOR:
                                res = (double)(left_val_int ^ right_val_int);
                                break;
                            case OPCODE_AND:
                                res = (double)(left_val_int & right_val_int);
                                break;
                            case OPCODE_LSHIFT:
                                res = (double)(left_val_int << right_val_int);
                                break;
                            case OPCODE_RSHIFT:
                                res = (double)(left_val_int >> right_val_int);
                                break;
                            default:
                                APE_ASSERT(false);
                                break;
                        }
                        stack_push(vm, object_make_number(res));
                    }
                    else if(left_type == APE_OBJECT_STRING && right_type == APE_OBJECT_STRING && opcode == OPCODE_ADD)
                    {
                        int left_len = (int)object_get_string_length(left);
                        int right_len = (int)object_get_string_length(right);

                        if(left_len == 0)
                        {
                            stack_push(vm, right);
                        }
                        else if(right_len == 0)
                        {
                            stack_push(vm, left);
                        }
                        else
                        {
                            const char* left_val = object_get_string(left);
                            const char* right_val = object_get_string(right);

                            Object res = object_make_string_with_capacity(vm->mem, left_len + right_len);
                            if(object_is_null(res))
                            {
                                goto err;
                            }

                            ok = object_string_append(res, left_val, left_len);
                            if(!ok)
                            {
                                goto err;
                            }

                            ok = object_string_append(res, right_val, right_len);
                            if(!ok)
                            {
                                goto err;
                            }
                            stack_push(vm, res);
                        }
                    }
                    else if((left_type == APE_OBJECT_ARRAY) && opcode == OPCODE_ADD)
                    {
                        object_add_array_value(left, right);
                        stack_push(vm, left);
                    }
                    else
                    {
                        bool overload_found = false;
                        bool ok = try_overload_operator(vm, left, right, opcode, &overload_found);
                        if(!ok)
                        {
                            goto err;
                        }
                        if(!overload_found)
                        {
                            const char* opcode_name = opcode_get_name(opcode);
                            const char* left_type_name = object_get_type_name(left_type);
                            const char* right_type_name = object_get_type_name(right_type);
                            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "Invalid operand types for %s, got %s and %s", opcode_name, left_type_name,
                                              right_type_name);
                            goto err;
                        }
                    }
                }
                break;

            case OPCODE_POP:
                {
                    stack_pop(vm);
                }
                break;

            case OPCODE_TRUE:
                {
                    stack_push(vm, object_make_bool(true));
                }
                break;

            case OPCODE_FALSE:
            {
                stack_push(vm, object_make_bool(false));
                break;
            }
            case OPCODE_COMPARE:
            case OPCODE_COMPARE_EQ:
                {
                    Object right = stack_pop(vm);
                    Object left = stack_pop(vm);
                    bool is_overloaded = false;
                    bool ok = try_overload_operator(vm, left, right, OPCODE_COMPARE, &is_overloaded);
                    if(!ok)
                    {
                        goto err;
                    }
                    if(!is_overloaded)
                    {
                        double comparison_res = object_compare(left, right, &ok);
                        if(ok || opcode == OPCODE_COMPARE_EQ)
                        {
                            Object res = object_make_number(comparison_res);
                            stack_push(vm, res);
                        }
                        else
                        {
                            const char* right_type_string = object_get_type_name(object_get_type(right));
                            const char* left_type_string = object_get_type_name(object_get_type(left));
                            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "Cannot compare %s and %s", left_type_string, right_type_string);
                            goto err;
                        }
                    }
                }
                break;

            case OPCODE_EQUAL:
            case OPCODE_NOT_EQUAL:
            case OPCODE_GREATER_THAN:
            case OPCODE_GREATER_THAN_EQUAL:
                {
                    Object value = stack_pop(vm);
                    double comparison_res = object_get_number(value);
                    bool res_val = false;
                    switch(opcode)
                    {
                        case OPCODE_EQUAL:
                            res_val = APE_DBLEQ(comparison_res, 0);
                            break;
                        case OPCODE_NOT_EQUAL:
                            res_val = !APE_DBLEQ(comparison_res, 0);
                            break;
                        case OPCODE_GREATER_THAN:
                            res_val = comparison_res > 0;
                            break;
                        case OPCODE_GREATER_THAN_EQUAL:
                        {
                            res_val = comparison_res > 0 || APE_DBLEQ(comparison_res, 0);
                            break;
                        }
                        default:
                            APE_ASSERT(false);
                            break;
                    }
                    Object res = object_make_bool(res_val);
                    stack_push(vm, res);
                }
                break;

            case OPCODE_MINUS:
                {
                    Object operand = stack_pop(vm);
                    ObjectType operand_type = object_get_type(operand);
                    if(operand_type == APE_OBJECT_NUMBER)
                    {
                        double val = object_get_number(operand);
                        Object res = object_make_number(-val);
                        stack_push(vm, res);
                    }
                    else
                    {
                        bool overload_found = false;
                        bool ok = try_overload_operator(vm, operand, object_make_null(), OPCODE_MINUS, &overload_found);
                        if(!ok)
                        {
                            goto err;
                        }
                        if(!overload_found)
                        {
                            const char* operand_type_string = object_get_type_name(operand_type);
                            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "Invalid operand type for MINUS, got %s", operand_type_string);
                            goto err;
                        }
                    }
                }
                break;

            case OPCODE_BANG:
                {
                    Object operand = stack_pop(vm);
                    ObjectType type = object_get_type(operand);
                    if(type == APE_OBJECT_BOOL)
                    {
                        Object res = object_make_bool(!object_get_bool(operand));
                        stack_push(vm, res);
                    }
                    else if(type == APE_OBJECT_NULL)
                    {
                        Object res = object_make_bool(true);
                        stack_push(vm, res);
                    }
                    else
                    {
                        bool overload_found = false;
                        bool ok = try_overload_operator(vm, operand, object_make_null(), OPCODE_BANG, &overload_found);
                        if(!ok)
                        {
                            goto err;
                        }
                        if(!overload_found)
                        {
                            Object res = object_make_bool(false);
                            stack_push(vm, res);
                        }
                    }
                }
                break;

            case OPCODE_JUMP:
                {
                    uint16_t pos = frame_read_uint16(vm->current_frame);
                    vm->current_frame->ip = pos;
                }
                break;

            case OPCODE_JUMP_IF_FALSE:
                {
                    uint16_t pos = frame_read_uint16(vm->current_frame);
                    Object test = stack_pop(vm);
                    if(!object_get_bool(test))
                    {
                        vm->current_frame->ip = pos;
                    }
                }
                break;

            case OPCODE_JUMP_IF_TRUE:
                {
                    uint16_t pos = frame_read_uint16(vm->current_frame);
                    Object test = stack_pop(vm);
                    if(object_get_bool(test))
                    {
                        vm->current_frame->ip = pos;
                    }
                }
                break;

            case OPCODE_NULL:
                {
                    stack_push(vm, object_make_null());
                }
                break;

            case OPCODE_DEFINE_MODULE_GLOBAL:
            {
                uint16_t ix = frame_read_uint16(vm->current_frame);
                Object value = stack_pop(vm);
                vm_set_global(vm, ix, value);
                break;
            }
            case OPCODE_SET_MODULE_GLOBAL:
                {
                    uint16_t ix = frame_read_uint16(vm->current_frame);
                    Object new_value = stack_pop(vm);
                    Object old_value = vm_get_global(vm, ix);
                    if(!check_assign(vm, old_value, new_value))
                    {
                        goto err;
                    }
                    vm_set_global(vm, ix, new_value);
                }
                break;

            case OPCODE_GET_MODULE_GLOBAL:
                {
                    uint16_t ix = frame_read_uint16(vm->current_frame);
                    Object global = vm->globals[ix];
                    stack_push(vm, global);
                }
                break;

            case OPCODE_ARRAY:
                {
                    uint16_t cn = frame_read_uint16(vm->current_frame);
                    Object array_obj = object_make_array_with_capacity(vm->mem, cn);
                    if(object_is_null(array_obj))
                    {
                        goto err;
                    }
                    Object* items = vm->stack + vm->sp - cn;
                    for(int i = 0; i < cn; i++)
                    {
                        Object item = items[i];
                        ok = object_add_array_value(array_obj, item);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                    set_sp(vm, vm->sp - cn);
                    stack_push(vm, array_obj);
                }
                break;

            case OPCODE_MAP_START:
                {
                    uint16_t cn = frame_read_uint16(vm->current_frame);
                    Object map_obj = object_make_map_with_capacity(vm->mem, cn);
                    if(object_is_null(map_obj))
                    {
                        goto err;
                    }
                    this_stack_push(vm, map_obj);
                }
                break;

            case OPCODE_MAP_END:
                {
                    uint16_t kvp_count = frame_read_uint16(vm->current_frame);
                    uint16_t items_count = kvp_count * 2;
                    Object map_obj = this_stack_pop(vm);
                    Object* kv_pairs = vm->stack + vm->sp - items_count;
                    for(int i = 0; i < items_count; i += 2)
                    {
                        Object key = kv_pairs[i];
                        if(!object_is_hashable(key))
                        {
                            ObjectType key_type = object_get_type(key);
                            const char* key_type_name = object_get_type_name(key_type);
                            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "Key of type %s is not hashable", key_type_name);
                            goto err;
                        }
                        Object val = kv_pairs[i + 1];
                        ok = object_set_map_value_object(map_obj, key, val);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                    set_sp(vm, vm->sp - items_count);
                    stack_push(vm, map_obj);
                }
                break;

            case OPCODE_GET_THIS:
                {
                    Object obj = this_stack_get(vm, 0);
                    stack_push(vm, obj);
                }
                break;

            case OPCODE_GET_INDEX:
                {
                    #if 0
                    const char* idxname;
                    #endif
                    Object index = stack_pop(vm);
                    Object left = stack_pop(vm);
                    ObjectType left_type = object_get_type(left);
                    ObjectType index_type = object_get_type(index);
                    const char* left_type_name = object_get_type_name(left_type);
                    const char* index_type_name = object_get_type_name(index_type);
                    /*
                    * todo: object method lookup could be implemented here
                    */
                    #if 0
                    {
                        int argc;
                        Object args[10];
                        NativeFNCallback afn;
                        argc = 0;
                        if(index_type == APE_OBJECT_STRING)
                        {
                            idxname = object_get_string(index);
                            fprintf(stderr, "index is a string: name=%s\n", idxname);
                            if((afn = builtin_get_object(left_type, idxname)) != NULL)
                            {
                                fprintf(stderr, "got a callback: afn=%p\n", afn);
                                //typedef Object (*NativeFNCallback)(VM*, void*, int, Object*);
                                args[argc] = left;
                                argc++;
                                stack_push(vm, afn(vm, NULL, argc, args));
                                break;
                            }
                        }
                    }
                    #endif

                    if(left_type != APE_OBJECT_ARRAY && left_type != APE_OBJECT_MAP && left_type != APE_OBJECT_STRING)
                    {
                        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                            "Type %s is not indexable (in OPCODE_GET_INDEX)", left_type_name);
                        goto err;
                    }
                    Object res = object_make_null();

                    if(left_type == APE_OBJECT_ARRAY)
                    {
                        if(index_type != APE_OBJECT_NUMBER)
                        {
                            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "Cannot index %s with %s", left_type_name, index_type_name);
                            goto err;
                        }
                        int ix = (int)object_get_number(index);
                        if(ix < 0)
                        {
                            ix = object_get_array_length(left) + ix;
                        }
                        if(ix >= 0 && ix < object_get_array_length(left))
                        {
                            res = object_get_array_value(left, ix);
                        }
                    }
                    else if(left_type == APE_OBJECT_MAP)
                    {
                        res = object_get_map_value_object(left, index);
                    }
                    else if(left_type == APE_OBJECT_STRING)
                    {
                        const char* str = object_get_string(left);
                        int left_len = object_get_string_length(left);
                        int ix = (int)object_get_number(index);
                        if(ix >= 0 && ix < left_len)
                        {
                            char res_str[2] = { str[ix], '\0' };
                            res = object_make_string(vm->mem, res_str);
                        }
                    }
                    stack_push(vm, res);
                }
                break;

            case OPCODE_GET_VALUE_AT:
            {
                Object index = stack_pop(vm);
                Object left = stack_pop(vm);
                ObjectType left_type = object_get_type(left);
                ObjectType index_type = object_get_type(index);
                const char* left_type_name = object_get_type_name(left_type);
                const char* index_type_name = object_get_type_name(index_type);

                if(left_type != APE_OBJECT_ARRAY && left_type != APE_OBJECT_MAP && left_type != APE_OBJECT_STRING)
                {
                    errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                      "Type %s is not indexable (in OPCODE_GET_VALUE_AT)", left_type_name);
                    goto err;
                }

                Object res = object_make_null();
                if(index_type != APE_OBJECT_NUMBER)
                {
                    errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                      "Cannot index %s with %s", left_type_name, index_type_name);
                    goto err;
                }
                int ix = (int)object_get_number(index);

                if(left_type == APE_OBJECT_ARRAY)
                {
                    res = object_get_array_value(left, ix);
                }
                else if(left_type == APE_OBJECT_MAP)
                {
                    res = object_get_kv_pair_at(vm->mem, left, ix);
                }
                else if(left_type == APE_OBJECT_STRING)
                {
                    const char* str = object_get_string(left);
                    int left_len = object_get_string_length(left);
                    int ix = (int)object_get_number(index);
                    if(ix >= 0 && ix < left_len)
                    {
                        char res_str[2] = { str[ix], '\0' };
                        res = object_make_string(vm->mem, res_str);
                    }
                }
                stack_push(vm, res);
                break;
            }
            case OPCODE_CALL:
            {
                uint8_t num_args = frame_read_uint8(vm->current_frame);
                Object callee = stack_get(vm, num_args);
                bool ok = call_object(vm, callee, num_args);
                if(!ok)
                {
                    goto err;
                }
                break;
            }
            case OPCODE_RETURN_VALUE:
            {
                Object res = stack_pop(vm);
                bool ok = pop_frame(vm);
                if(!ok)
                {
                    goto end;
                }
                stack_push(vm, res);
                break;
            }
            case OPCODE_RETURN:
            {
                bool ok = pop_frame(vm);
                stack_push(vm, object_make_null());
                if(!ok)
                {
                    stack_pop(vm);
                    goto end;
                }
                break;
            }
            case OPCODE_DEFINE_LOCAL:
            {
                uint8_t pos = frame_read_uint8(vm->current_frame);
                vm->stack[vm->current_frame->base_pointer + pos] = stack_pop(vm);
                break;
            }
            case OPCODE_SET_LOCAL:
            {
                uint8_t pos = frame_read_uint8(vm->current_frame);
                Object new_value = stack_pop(vm);
                Object old_value = vm->stack[vm->current_frame->base_pointer + pos];
                if(!check_assign(vm, old_value, new_value))
                {
                    goto err;
                }
                vm->stack[vm->current_frame->base_pointer + pos] = new_value;
                break;
            }
            case OPCODE_GET_LOCAL:
            {
                uint8_t pos = frame_read_uint8(vm->current_frame);
                Object val = vm->stack[vm->current_frame->base_pointer + pos];
                stack_push(vm, val);
                break;
            }
            case OPCODE_GET_APE_GLOBAL:
            {
                uint16_t ix = frame_read_uint16(vm->current_frame);
                bool ok = false;
                Object val = global_store_get_object_at(vm->global_store, ix, &ok);
                if(!ok)
                {
                    errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                      "Global value %d not found", ix);
                    goto err;
                }
                stack_push(vm, val);
                break;
            }
            case OPCODE_FUNCTION:
                {
                    uint16_t constant_ix = frame_read_uint16(vm->current_frame);
                    uint8_t num_free = frame_read_uint8(vm->current_frame);
                    Object* constant = (Object*)constants->get(constant_ix);
                    if(!constant)
                    {
                        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "Constant %d not found", constant_ix);
                        goto err;
                    }
                    ObjectType constant_type = object_get_type(*constant);
                    if(constant_type != APE_OBJECT_FUNCTION)
                    {
                        const char* type_name = object_get_type_name(constant_type);
                        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "%s is not a function", type_name);
                        goto err;
                    }

                    const FunctionObject* constant_function = object_get_function(*constant);
                    Object function_obj
                    = object_make_function(vm->mem, object_get_function_name(*constant), constant_function->comp_result,
                                           false, constant_function->num_locals, constant_function->num_args, num_free);
                    if(object_is_null(function_obj))
                    {
                        goto err;
                    }
                    for(int i = 0; i < num_free; i++)
                    {
                        Object free_val = vm->stack[vm->sp - num_free + i];
                        object_set_function_free_val(function_obj, i, free_val);
                    }
                    set_sp(vm, vm->sp - num_free);
                    stack_push(vm, function_obj);
                }
                break;
            case OPCODE_GET_FREE:
                {
                    uint8_t free_ix = frame_read_uint8(vm->current_frame);
                    Object val = object_get_function_free_val(vm->current_frame->function, free_ix);
                    stack_push(vm, val);
                }
                break;
            case OPCODE_SET_FREE:
                {
                    uint8_t free_ix = frame_read_uint8(vm->current_frame);
                    Object val = stack_pop(vm);
                    object_set_function_free_val(vm->current_frame->function, free_ix, val);
                }
                break;
            case OPCODE_CURRENT_FUNCTION:
                {
                    Object current_function = vm->current_frame->function;
                    stack_push(vm, current_function);
                }
                break;
            case OPCODE_SET_INDEX:
                {
                    Object index = stack_pop(vm);
                    Object left = stack_pop(vm);
                    Object new_value = stack_pop(vm);
                    ObjectType left_type = object_get_type(left);
                    ObjectType index_type = object_get_type(index);
                    const char* left_type_name = object_get_type_name(left_type);
                    const char* index_type_name = object_get_type_name(index_type);

                    if(left_type != APE_OBJECT_ARRAY && left_type != APE_OBJECT_MAP)
                    {
                        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "Type %s is not indexable (in OPCODE_SET_INDEX)", left_type_name);
                        goto err;
                    }

                    if(left_type == APE_OBJECT_ARRAY)
                    {
                        if(index_type != APE_OBJECT_NUMBER)
                        {
                            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "Cannot index %s with %s", left_type_name, index_type_name);
                            goto err;
                        }
                        int ix = (int)object_get_number(index);
                        ok = object_set_array_value_at(left, ix, new_value);
                        if(!ok)
                        {
                            errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                             "Setting array item failed (out of bounds?)");
                            goto err;
                        }
                    }
                    else if(left_type == APE_OBJECT_MAP)
                    {
                        Object old_value = object_get_map_value_object(left, index);
                        if(!check_assign(vm, old_value, new_value))
                        {
                            goto err;
                        }
                        ok = object_set_map_value_object(left, index, new_value);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                }
                break;
            case OPCODE_DUP:
                {
                    Object val = stack_get(vm, 0);
                    stack_push(vm, val);
                }
                break;
            case OPCODE_LEN:
                {
                    Object val = stack_pop(vm);
                    int len = 0;
                    ObjectType type = object_get_type(val);
                    if(type == APE_OBJECT_ARRAY)
                    {
                        len = object_get_array_length(val);
                    }
                    else if(type == APE_OBJECT_MAP)
                    {
                        len = object_get_map_length(val);
                    }
                    else if(type == APE_OBJECT_STRING)
                    {
                        len = object_get_string_length(val);
                    }
                    else
                    {
                        const char* type_name = object_get_type_name(type);
                        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "Cannot get length of %s", type_name);
                        goto err;
                    }
                    stack_push(vm, object_make_number(len));
                }
                break;

            case OPCODE_NUMBER:
                {
                    uint64_t val = frame_read_uint64(vm->current_frame);
                    double val_double = ape_uint64_to_double(val);
                    Object obj = object_make_number(val_double);
                    stack_push(vm, obj);
                }
                break;
            case OPCODE_SET_RECOVER:
                {
                    uint16_t recover_ip = frame_read_uint16(vm->current_frame);
                    vm->current_frame->recover_ip = recover_ip;
                }
                break;
            default:
                {
                    APE_ASSERT(false);
                    errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "Unknown opcode: 0x%x", opcode);
                    goto err;
                }
                break;
        }
        if(check_time)
        {
            time_check_counter++;
            if(time_check_counter > time_check_interval)
            {
                int elapsed_ms = (int)ape_timer_get_elapsed_ms(&timer);
                if(elapsed_ms > max_exec_time_ms)
                {
                    errors_add_errorf(vm->errors, APE_ERROR_TIMEOUT, frame_src_position(vm->current_frame),
                                      "Execution took more than %1.17g ms", max_exec_time_ms);
                    goto err;
                }
                time_check_counter = 0;
            }
        }
    err:
        if(errors_get_count(vm->errors) > 0)
        {
            Error* err = errors_get_last_error(vm->errors);
            if(err->type == APE_ERROR_RUNTIME && errors_get_count(vm->errors) == 1)
            {
                int recover_frame_ix = -1;
                for(int i = vm->frames_count - 1; i >= 0; i--)
                {
                    Frame* frame = &vm->frames[i];
                    if(frame->recover_ip >= 0 && !frame->is_recovering)
                    {
                        recover_frame_ix = i;
                        break;
                    }
                }
                if(recover_frame_ix < 0)
                {
                    goto end;
                }
                else
                {
                    if(!err->traceback)
                    {
                        err->traceback = traceback_make(vm->alloc);
                    }
                    if(err->traceback)
                    {
                        traceback_append_from_vm(err->traceback, vm);
                    }
                    while(vm->frames_count > (recover_frame_ix + 1))
                    {
                        pop_frame(vm);
                    }
                    Object err_obj = object_make_error(vm->mem, err->message);
                    if(!object_is_null(err_obj))
                    {
                        object_set_error_traceback(err_obj, err->traceback);
                        err->traceback = NULL;
                    }
                    stack_push(vm, err_obj);
                    vm->current_frame->ip = vm->current_frame->recover_ip;
                    vm->current_frame->is_recovering = true;
                    errors_clear(vm->errors);
                }
            }
            else
            {
                goto end;
            }
        }
        if(gc_should_sweep(vm->mem))
        {
            run_gc(vm, constants);
        }
    }

end:
    if(errors_get_count(vm->errors) > 0)
    {
        Error* err = errors_get_last_error(vm->errors);
        if(!err->traceback)
        {
            err->traceback = traceback_make(vm->alloc);
        }
        if(err->traceback)
        {
            traceback_append_from_vm(err->traceback, vm);
        }
    }

    run_gc(vm, constants);

    vm->running = false;
    return errors_get_count(vm->errors) == 0;
}

Object vm_get_last_popped(VM* vm)
{
    return vm->last_popped;
}

bool vm_has_errors(VM* vm)
{
    return errors_get_count(vm->errors) > 0;
}

bool vm_set_global(VM* vm, int ix, Object val)
{
    if(ix >= VM_MAX_GLOBALS)
    {
        APE_ASSERT(false);
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "Global write out of range");
        return false;
    }
    vm->globals[ix] = val;
    if(ix >= vm->globals_count)
    {
        vm->globals_count = ix + 1;
    }
    return true;
}

Object vm_get_global(VM* vm, int ix)
{
    if(ix >= VM_MAX_GLOBALS)
    {
        APE_ASSERT(false);
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "Global read out of range");
        return object_make_null();
    }
    return vm->globals[ix];
}

// INTERNAL
static void set_sp(VM* vm, int new_sp)
{
    if(new_sp > vm->sp)
    {// to avoid gcing freed objects
        int cn = new_sp - vm->sp;
        size_t bytes_count = cn * sizeof(Object);
        memset(vm->stack + vm->sp, 0, bytes_count);
    }
    vm->sp = new_sp;
}

void stack_push(VM* vm, Object obj)
{
#ifdef APE_DEBUG
    if(vm->sp >= VM_STACK_SIZE)
    {
        APE_ASSERT(false);
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "Stack overflow");
        return;
    }
    if(vm->current_frame)
    {
        Frame* frame = vm->current_frame;
        FunctionObject* current_function = object_get_function(frame->function);
        int num_locals = current_function->num_locals;
        APE_ASSERT(vm->sp >= (frame->base_pointer + num_locals));
    }
#endif
    vm->stack[vm->sp] = obj;
    vm->sp++;
}

Object stack_pop(VM* vm)
{
#ifdef APE_DEBUG
    if(vm->sp == 0)
    {
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "Stack underflow");
        APE_ASSERT(false);
        return object_make_null();
    }
    if(vm->current_frame)
    {
        Frame* frame = vm->current_frame;
        FunctionObject* current_function = object_get_function(frame->function);
        int num_locals = current_function->num_locals;
        APE_ASSERT((vm->sp - 1) >= (frame->base_pointer + num_locals));
    }
#endif
    vm->sp--;
    Object res = vm->stack[vm->sp];
    vm->last_popped = res;
    return res;
}

Object stack_get(VM* vm, int nth_item)
{
    int ix = vm->sp - 1 - nth_item;
#ifdef APE_DEBUG
    if(ix < 0 || ix >= VM_STACK_SIZE)
    {
        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "Invalid stack index: %d", nth_item);
        APE_ASSERT(false);
        return object_make_null();
    }
#endif
    return vm->stack[ix];
}

static void this_stack_push(VM* vm, Object obj)
{
#ifdef APE_DEBUG
    if(vm->this_sp >= VM_THIS_STACK_SIZE)
    {
        APE_ASSERT(false);
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "this stack overflow");
        return;
    }
#endif
    vm->this_stack[vm->this_sp] = obj;
    vm->this_sp++;
}

static Object this_stack_pop(VM* vm)
{
#ifdef APE_DEBUG
    if(vm->this_sp == 0)
    {
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "this stack underflow");
        APE_ASSERT(false);
        return object_make_null();
    }
#endif
    vm->this_sp--;
    return vm->this_stack[vm->this_sp];
}

static Object this_stack_get(VM* vm, int nth_item)
{
    int ix = vm->this_sp - 1 - nth_item;
#ifdef APE_DEBUG
    if(ix < 0 || ix >= VM_THIS_STACK_SIZE)
    {
        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "Invalid this stack index: %d", nth_item);
        APE_ASSERT(false);
        return object_make_null();
    }
#endif
    return vm->this_stack[ix];
}

bool push_frame(VM* vm, Frame frame)
{
    if(vm->frames_count >= VM_MAX_FRAMES)
    {
        APE_ASSERT(false);
        return false;
    }
    vm->frames[vm->frames_count] = frame;
    vm->current_frame = &vm->frames[vm->frames_count];
    vm->frames_count++;
    FunctionObject* frame_function = object_get_function(frame.function);
    set_sp(vm, frame.base_pointer + frame_function->num_locals);
    return true;
}

bool pop_frame(VM* vm)
{
    set_sp(vm, vm->current_frame->base_pointer - 1);
    if(vm->frames_count <= 0)
    {
        APE_ASSERT(false);
        vm->current_frame = NULL;
        return false;
    }
    vm->frames_count--;
    if(vm->frames_count == 0)
    {
        vm->current_frame = NULL;
        return false;
    }
    vm->current_frame = &vm->frames[vm->frames_count - 1];
    return true;
}

void run_gc(VM* vm, Array * constants)
{
    gc_unmark_all(vm->mem);
    gc_mark_objects(global_store_get_object_data(vm->global_store), global_store_get_object_count(vm->global_store));
    gc_mark_objects((Object*)constants->data(), constants->count());
    gc_mark_objects(vm->globals, vm->globals_count);
    for(int i = 0; i < vm->frames_count; i++)
    {
        Frame* frame = &vm->frames[i];
        gc_mark_object(frame->function);
    }
    gc_mark_objects(vm->stack, vm->sp);
    gc_mark_objects(vm->this_stack, vm->this_sp);
    gc_mark_object(vm->last_popped);
    gc_mark_objects(vm->operator_oveload_keys, OPCODE_MAX);
    gc_sweep(vm->mem);
}

bool call_object(VM* vm, Object callee, int num_args)
{
    ObjectType callee_type = object_get_type(callee);
    if(callee_type == APE_OBJECT_FUNCTION)
    {
        FunctionObject* callee_function = object_get_function(callee);
        if(num_args != callee_function->num_args)
        {
            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                              "Invalid number of arguments to \"%s\", expected %d, got %d",
                              object_get_function_name(callee), callee_function->num_args, num_args);
            return false;
        }
        Frame callee_frame;
        bool ok = frame_init(&callee_frame, callee, vm->sp - num_args);
        if(!ok)
        {
            errors_add_error(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "Frame init failed in call_object");
            return false;
        }
        ok = push_frame(vm, callee_frame);
        if(!ok)
        {
            errors_add_error(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "Pushing frame failed in call_object");
            return false;
        }
    }
    else if(callee_type == APE_OBJECT_NATIVE_FUNCTION)
    {
        Object* stack_pos = vm->stack + vm->sp - num_args;
        Object res = call_native_function(vm, callee, frame_src_position(vm->current_frame), num_args, stack_pos);
        if(vm_has_errors(vm))
        {
            return false;
        }
        set_sp(vm, vm->sp - num_args - 1);
        stack_push(vm, res);
    }
    else
    {
        const char* callee_type_name = object_get_type_name(callee_type);
        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "%s object is not callable", callee_type_name);
        return false;
    }
    return true;
}

Object call_native_function(VM* vm, Object callee, Position src_pos, int argc, Object* args)
{
    NativeFunction* native_fun = object_get_native_function(callee);
    Object res = native_fun->native_funcptr(vm, native_fun->data, argc, args);
    if(errors_has_errors(vm->errors) && !APE_STREQ(native_fun->name, "crash"))
    {
        Error* err = errors_get_last_error(vm->errors);
        err->pos = src_pos;
        err->traceback = traceback_make(vm->alloc);
        if(err->traceback)
        {
            traceback_append(err->traceback, native_fun->name, src_pos_invalid);
        }
        return object_make_null();
    }
    ObjectType res_type = object_get_type(res);
    if(res_type == APE_OBJECT_ERROR)
    {
        Traceback* traceback = traceback_make(vm->alloc);
        if(traceback)
        {
            // error builtin is treated in a special way
            if(!APE_STREQ(native_fun->name, "error"))
            {
                traceback_append(traceback, native_fun->name, src_pos_invalid);
            }
            traceback_append_from_vm(traceback, vm);
            object_set_error_traceback(res, traceback);
        }
    }
    return res;
}

bool check_assign(VM* vm, Object old_value, Object new_value)
{
    ObjectType old_value_type;
    ObjectType new_value_type;
    (void)vm;
    old_value_type = object_get_type(old_value);
    new_value_type = object_get_type(new_value);
    if(old_value_type == APE_OBJECT_NULL || new_value_type == APE_OBJECT_NULL)
    {
        return true;
    }
    if(old_value_type != new_value_type)
    {
        /*
        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "Trying to assign variable of type %s to %s",
                          object_get_type_name(new_value_type), object_get_type_name(old_value_type));
        return false;
        */
    }
    return true;
}

static bool try_overload_operator(VM* vm, Object left, Object right, opcode_t op, bool* out_overload_found)
{
    int num_operands;
    Object key;
    Object callee;
    ObjectType left_type;
    ObjectType right_type;
    *out_overload_found = false;
    left_type = object_get_type(left);
    right_type = object_get_type(right);
    if(left_type != APE_OBJECT_MAP && right_type != APE_OBJECT_MAP)
    {
        *out_overload_found = false;
        return true;
    }
    num_operands = 2;
    if(op == OPCODE_MINUS || op == OPCODE_BANG)
    {
        num_operands = 1;
    }
    key = vm->operator_oveload_keys[op];
    callee = object_make_null();
    if(left_type == APE_OBJECT_MAP)
    {
        callee = object_get_map_value_object(left, key);
    }
    if(!object_is_callable(callee))
    {
        if(right_type == APE_OBJECT_MAP)
        {
            callee = object_get_map_value_object(right, key);
        }
        if(!object_is_callable(callee))
        {
            *out_overload_found = false;
            return true;
        }
    }
    *out_overload_found = true;
    stack_push(vm, callee);
    stack_push(vm, left);
    if(num_operands == 2)
    {
        stack_push(vm, right);
    }
    return call_object(vm, callee, num_operands);
}

//-----------------------------------------------------------------------------
// Ape
//-----------------------------------------------------------------------------
Context* ape_make(void)
{
    return ape_make_ex(NULL, NULL, NULL);
}

Context* ape_make_ex(MallocFNCallback malloc_fn, FreeFNCallback free_fn, void* ctx)
{
    Allocator custom_alloc = allocator_make((AllocatorMallocFNCallback)malloc_fn, (AllocatorFreeFNCallback)free_fn, ctx);

    Context* ape = (Context*)allocator_malloc(&custom_alloc, sizeof(Context));
    if(!ape)
    {
        return NULL;
    }

    memset(ape, 0, sizeof(Context));
    ape->alloc = allocator_make(ape_malloc, ape_free, ape);
    ape->custom_allocator = custom_alloc;

    set_default_config(ape);

    errors_init(&ape->errors);

    ape->mem = gcmem_make(&ape->alloc);
    if(!ape->mem)
    {
        goto err;
    }

    ape->files = PtrArray::make(&ape->alloc);
    if(!ape->files)
    {
        goto err;
    }

    ape->global_store = global_store_make(&ape->alloc, ape->mem);
    if(!ape->global_store)
    {
        goto err;
    }

    ape->compiler = compiler_make(&ape->alloc, &ape->config, ape->mem, &ape->errors, ape->files, ape->global_store);
    if(!ape->compiler)
    {
        goto err;
    }

    ape->vm = vm_make(&ape->alloc, &ape->config, ape->mem, &ape->errors, ape->global_store);
    if(!ape->vm)
    {
        goto err;
    }
    builtins_install(ape->vm);
    return ape;
err:
    ape_deinit(ape);
    allocator_free(&custom_alloc, ape);
    return NULL;
}

void ape_destroy(Context* ape)
{
    if(!ape)
    {
        return;
    }
    ape_deinit(ape);
    Allocator alloc = ape->alloc;
    allocator_free(&alloc, ape);
}

void ape_free_allocated(Context* ape, void* ptr)
{
    allocator_free(&ape->alloc, ptr);
}

void ape_set_repl_mode(Context* ape, bool enabled)
{
    ape->config.repl_mode = enabled;
}

bool ape_set_timeout(Context* ape, double max_execution_time_ms)
{
    if(!ape_timer_platform_supported())
    {
        ape->config.max_execution_time_ms = 0;
        ape->config.max_execution_time_set = false;
        return false;
    }

    if(max_execution_time_ms >= 0)
    {
        ape->config.max_execution_time_ms = max_execution_time_ms;
        ape->config.max_execution_time_set = true;
    }
    else
    {
        ape->config.max_execution_time_ms = 0;
        ape->config.max_execution_time_set = false;
    }
    return true;
}

void ape_set_stdout_write_function(Context* ape, StdoutWriteFNCallback stdout_write, void* context)
{
    ape->config.stdio.write.write = stdout_write;
    ape->config.stdio.write.context = context;
}

void ape_set_file_write_function(Context* ape, WriteFileFNCallback file_write, void* context)
{
    ape->config.fileio.write_file.write_file = file_write;
    ape->config.fileio.write_file.context = context;
}

void ape_set_file_read_function(Context* ape, ReadFileFNCallback file_read, void* context)
{
    ape->config.fileio.read_file.read_file = file_read;
    ape->config.fileio.read_file.context = context;
}




void ape_program_destroy(Program* program)
{
    if(!program)
    {
        return;
    }
    compilation_result_destroy(program->comp_res);
    allocator_free(&program->ape->alloc, program);
}

Object ape_execute(Context* ape, const char* code)
{
    bool ok;
    Object res;
    CompilationResult* comp_res;
    reset_state(ape);
    comp_res = compiler_compile(ape->compiler, code);
    if(!comp_res || errors_get_count(&ape->errors) > 0)
    {
        goto err;
    }
    ok = vm_run(ape->vm, comp_res, compiler_get_constants(ape->compiler));
    if(!ok || errors_get_count(&ape->errors) > 0)
    {
        goto err;
    }
    APE_ASSERT(ape->vm->sp == 0);
    res = vm_get_last_popped(ape->vm);
    if(object_get_type(res) == APE_OBJECT_NONE)
    {
        goto err;
    }
    compilation_result_destroy(comp_res);
    return res;

err:
    compilation_result_destroy(comp_res);
    return object_make_null();
}

Object ape_execute_file(Context* ape, const char* path)
{
    bool ok;
    Object res;
    CompilationResult* comp_res;
    reset_state(ape);
    comp_res = compiler_compile_file(ape->compiler, path);
    if(!comp_res || errors_get_count(&ape->errors) > 0)
    {
        goto err;
    }
    ok = vm_run(ape->vm, comp_res, compiler_get_constants(ape->compiler));
    if(!ok || errors_get_count(&ape->errors) > 0)
    {
        goto err;
    }
    APE_ASSERT(ape->vm->sp == 0);
    res = vm_get_last_popped(ape->vm);
    if(object_get_type(res) == APE_OBJECT_NONE)
    {
        goto err;
    }
    compilation_result_destroy(comp_res);

    return res;

err:
    compilation_result_destroy(comp_res);
    return object_make_null();
}



bool ape_has_errors(const Context* ape)
{
    return ape_errors_count(ape) > 0;
}

int ape_errors_count(const Context* ape)
{
    return errors_get_count(&ape->errors);
}

void ape_clear_errors(Context* ape)
{
    errors_clear(&ape->errors);
}

const Error* ape_get_error(const Context* ape, int index)
{
    return (const Error*)errors_getc(&ape->errors, index);
}

bool ape_set_native_function(Context* ape, const char* name, UserFNCallback fn, void* data)
{
    Object obj = ape_object_make_native_function_with_name(ape, name, fn, data);
    if(object_is_null(obj))
    {
        return false;
    }
    return ape_set_global_constant(ape, name, obj);
}

bool ape_set_global_constant(Context* ape, const char* name, Object obj)
{
    return global_store_set(ape->global_store, name, obj);
}

Object ape_get_object(Context* ape, const char* name)
{
    SymbolTable* st = compiler_get_symbol_table(ape->compiler);
    const Symbol* symbol = symbol_table_resolve(st, name);
    if(!symbol)
    {
        errors_add_errorf(&ape->errors, APE_ERROR_USER, src_pos_invalid, "Symbol \"%s\" is not defined", name);
        return object_make_null();
    }
    Object res = object_make_null();
    if(symbol->type == SYMBOL_MODULE_GLOBAL)
    {
        res = vm_get_global(ape->vm, symbol->index);
    }
    else if(symbol->type == SYMBOL_APE_GLOBAL)
    {
        bool ok = false;
        res = global_store_get_object_at(ape->global_store, symbol->index, &ok);
        if(!ok)
        {
            errors_add_errorf(&ape->errors, APE_ERROR_USER, src_pos_invalid, "Failed to get global object at %d", symbol->index);
            return object_make_null();
        }
    }
    else
    {
        errors_add_errorf(&ape->errors, APE_ERROR_USER, src_pos_invalid, "Value associated with symbol \"%s\" could not be loaded", name);
        return object_make_null();
    }
    return res;
}

Object ape_object_make_native_function(Context* ape, UserFNCallback fn, void* data)
{
    return ape_object_make_native_function_with_name(ape, "", fn, data);
}



bool ape_object_disable_gc(Object ape_obj)
{
    Object obj = ape_obj;
    return gc_disable_on_object(obj);
}

void ape_object_enable_gc(Object ape_obj)
{
    Object obj = ape_obj;
    gc_enable_on_object(obj);
}

void ape_set_runtime_error(Context* ape, const char* message)
{
    errors_add_error(&ape->errors, APE_ERROR_RUNTIME, src_pos_invalid, message);
}

void ape_set_runtime_errorf(Context* ape, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int to_write = vsnprintf(NULL, 0, fmt, args);
    (void)to_write;
    va_end(args);
    va_start(args, fmt);
    char res[APE_ERROR_MESSAGE_MAX_LENGTH];
    int written = vsnprintf(res, APE_ERROR_MESSAGE_MAX_LENGTH, fmt, args);
    (void)written;
    APE_ASSERT(to_write == written);
    va_end(args);
    errors_add_error(&ape->errors, APE_ERROR_RUNTIME, src_pos_invalid, res);
}

//-----------------------------------------------------------------------------
// Ape object array
//-----------------------------------------------------------------------------


const char* ape_object_get_array_string(Object obj, int ix)
{
    Object object;
    object = object_get_array_value(obj, ix);
    if(object_get_type(object) != APE_OBJECT_STRING)
    {
        return NULL;
    }
    return object_get_string(object);
}

double ape_object_get_array_number(Object obj, int ix)
{
    Object object;
    object = object_get_array_value(obj, ix);
    if(object_get_type(object) != APE_OBJECT_NUMBER)
    {
        return 0;
    }
    return object_get_number(object);
}

bool ape_object_get_array_bool(Object obj, int ix)
{
    Object object = object_get_array_value(obj, ix);
    if(object_get_type(object) != APE_OBJECT_BOOL)
    {
        return 0;
    }
    return object_get_bool(object);
}

bool ape_object_set_array_value(Object ape_obj, int ix, Object ape_value)
{
    Object obj = ape_obj;
    Object value = ape_value;
    return object_set_array_value_at(obj, ix, value);
}

bool ape_object_set_array_string(Object obj, int ix, const char* string)
{
    GCMemory* mem = object_get_mem(obj);
    if(!mem)
    {
        return false;
    }
    Object new_value = object_make_string(mem, string);
    return ape_object_set_array_value(obj, ix, new_value);
}

bool ape_object_set_array_number(Object obj, int ix, double number)
{
    Object new_value = object_make_number(number);
    return ape_object_set_array_value(obj, ix, new_value);
}

bool ape_object_set_array_bool(Object obj, int ix, bool value)
{
    Object new_value = object_make_bool(value);
    return ape_object_set_array_value(obj, ix, new_value);
}


bool ape_object_add_array_string(Object obj, const char* string)
{
    GCMemory* mem = object_get_mem(obj);
    if(!mem)
    {
        return false;
    }
    Object new_value = object_make_string(mem, string);
    return object_add_array_value(obj, new_value);
}

bool ape_object_add_array_number(Object obj, double number)
{
    Object new_value = object_make_number(number);
    return object_add_array_value(obj, new_value);
}

bool ape_object_add_array_bool(Object obj, bool value)
{
    Object new_value = object_make_bool(value);
    return object_add_array_value(obj, new_value);
}

//-----------------------------------------------------------------------------
// Ape object map
//-----------------------------------------------------------------------------

bool ape_object_set_map_value(Object obj, const char* key, Object value)
{
    GCMemory* mem = object_get_mem(obj);
    if(!mem)
    {
        return false;
    }
    Object key_object = object_make_string(mem, key);
    if(object_is_null(key_object))
    {
        return false;
    }
    return object_set_map_value_object(obj, key_object, value);
}

bool ape_object_set_map_string(Object obj, const char* key, const char* string)
{
    GCMemory* mem = object_get_mem(obj);
    if(!mem)
    {
        return false;
    }
    Object string_object = object_make_string(mem, string);
    if(object_is_null(string_object))
    {
        return false;
    }
    return ape_object_set_map_value(obj, key, string_object);
}

bool ape_object_set_map_number(Object obj, const char* key, double number)
{
    Object number_object = object_make_number(number);
    return ape_object_set_map_value(obj, key, number_object);
}

bool ape_object_set_map_bool(Object obj, const char* key, bool value)
{
    Object bool_object = object_make_bool(value);
    return ape_object_set_map_value(obj, key, bool_object);
}


Object ape_object_get_map_value(Object object, const char* key)
{
    GCMemory* mem = object_get_mem(object);
    if(!mem)
    {
        return object_make_null();
    }
    Object key_object = object_make_string(mem, key);
    if(object_is_null(key_object))
    {
        return object_make_null();
    }
    Object res = object_get_map_value_object(object, key_object);
    return res;
}

const char* ape_object_get_map_string(Object object, const char* key)
{
    Object res = ape_object_get_map_value(object, key);
    return object_get_string(res);
}

double ape_object_get_map_number(Object object, const char* key)
{
    Object res = ape_object_get_map_value(object, key);
    return object_get_number(res);
}

bool ape_object_get_map_bool(Object object, const char* key)
{
    Object res = ape_object_get_map_value(object, key);
    return object_get_bool(res);
}

bool ape_object_map_has_key(Object ape_object, const char* key)
{
    Object object = ape_object;
    GCMemory* mem = object_get_mem(object);
    if(!mem)
    {
        return false;
    }
    Object key_object = object_make_string(mem, key);
    if(object_is_null(key_object))
    {
        return false;
    }
    return object_map_has_key_object(object, key_object);
}

//-----------------------------------------------------------------------------
// Ape error
//-----------------------------------------------------------------------------

const char* ape_error_get_message(const Error* ae)
{
    const Error* error = (const Error*)ae;
    return error->message;
}

const char* ape_error_get_filepath(const Error* ae)
{
    const Error* error = (const Error*)ae;
    if(!error->pos.file)
    {
        return NULL;
    }
    return error->pos.file->path;
}

const char* ape_error_get_line(const Error* ae)
{
    const Error* error = (const Error*)ae;
    if(!error->pos.file)
    {
        return NULL;
    }
    PtrArray* lines = error->pos.file->lines;
    if(error->pos.line >= lines->count())
    {
        return NULL;
    }
    const char* line = (const char*)lines->get(error->pos.line);
    return line;
}

int ape_error_get_line_number(const Error* ae)
{
    const Error* error = (const Error*)ae;
    if(error->pos.line < 0)
    {
        return -1;
    }
    return error->pos.line + 1;
}

int ape_error_get_column_number(const Error* ae)
{
    const Error* error = (const Error*)ae;
    if(error->pos.column < 0)
    {
        return -1;
    }
    return error->pos.column + 1;
}

ErrorType ape_error_get_type(const Error* ae)
{
    const Error* error = (const Error*)ae;
    switch(error->type)
    {
        case APE_ERROR_NONE:
            return APE_ERROR_NONE;
        case APE_ERROR_PARSING:
            return APE_ERROR_PARSING;
        case APE_ERROR_COMPILATION:
            return APE_ERROR_COMPILATION;
        case APE_ERROR_RUNTIME:
            return APE_ERROR_RUNTIME;
        case APE_ERROR_TIMEOUT:
            return APE_ERROR_TIMEOUT;
        case APE_ERROR_ALLOCATION:
            return APE_ERROR_ALLOCATION;
        case APE_ERROR_USER:
            return APE_ERROR_USER;
        default:
            return APE_ERROR_NONE;
    }
}

const char* ape_error_get_type_string(const Error* error)
{
    return ape_error_type_to_string(ape_error_get_type(error));
}


char* ape_error_serialize(Context* ape, const Error* err)
{
    const char* type_str = ape_error_get_type_string(err);
    const char* filename = ape_error_get_filepath(err);
    const char* line = ape_error_get_line(err);
    int line_num = ape_error_get_line_number(err);
    int col_num = ape_error_get_column_number(err);
    StringBuffer* buf = strbuf_make(&ape->alloc);
    if(!buf)
    {
        return NULL;
    }
    if(line)
    {
        strbuf_append(buf, line);
        strbuf_append(buf, "\n");
        if(col_num >= 0)
        {
            for(int j = 0; j < (col_num - 1); j++)
            {
                strbuf_append(buf, " ");
            }
            strbuf_append(buf, "^\n");
        }
    }
    strbuf_appendf(buf, "%s ERROR in \"%s\" on %d:%d: %s\n", type_str, filename, line_num, col_num, ape_error_get_message(err));
    const Traceback* traceback = ape_error_get_traceback(err);
    if(traceback)
    {
        strbuf_appendf(buf, "Traceback:\n");
        traceback_to_string((const Traceback*)ape_error_get_traceback(err), buf);
    }
    if(strbuf_failed(buf))
    {
        strbuf_destroy(buf);
        return NULL;
    }
    return strbuf_get_string_and_destroy(buf);
}

const Traceback* ape_error_get_traceback(const Error* ae)
{
    const Error* error = (const Error*)ae;
    return (const Traceback*)error->traceback;
}

//-----------------------------------------------------------------------------
// Ape traceback
//-----------------------------------------------------------------------------

int ape_traceback_get_depth(const Traceback* ape_traceback)
{
    const Traceback* traceback = (const Traceback*)ape_traceback;
    return traceback->items->count();
}

const char* ape_traceback_get_filepath(const Traceback* ape_traceback, int depth)
{
    const Traceback* traceback = (const Traceback*)ape_traceback;
    TracebackItem* item = (TracebackItem*)traceback->items->get(depth);
    if(!item)
    {
        return NULL;
    }
    return traceback_item_get_filepath(item);
}

const char* ape_traceback_get_line(const Traceback* ape_traceback, int depth)
{
    const Traceback* traceback = (const Traceback*)ape_traceback;
    TracebackItem* item = (TracebackItem*)traceback->items->get(depth);
    if(!item)
    {
        return NULL;
    }
    return traceback_item_get_line(item);
}

int ape_traceback_get_line_number(const Traceback* ape_traceback, int depth)
{
    const Traceback* traceback = (const Traceback*)ape_traceback;
    TracebackItem* item = (TracebackItem*)traceback->items->get(depth);
    if(!item)
    {
        return -1;
    }
    return item->pos.line;
}

int ape_traceback_get_column_number(const Traceback* ape_traceback, int depth)
{
    const Traceback* traceback = (const Traceback*)ape_traceback;
    TracebackItem* item = (TracebackItem*)traceback->items->get(depth);
    if(!item)
    {
        return -1;
    }
    return item->pos.column;
}

const char* ape_traceback_get_function_name(const Traceback* ape_traceback, int depth)
{
    const Traceback* traceback;
    TracebackItem* item;
    traceback = (const Traceback*)ape_traceback;
    item = (TracebackItem*)traceback->items->get(depth);
    if(!item)
    {
        return "";
    }
    return item->function_name;
}

//-----------------------------------------------------------------------------
// Ape internal
//-----------------------------------------------------------------------------
void ape_deinit(Context* ape)
{
    vm_destroy(ape->vm);
    compiler_destroy(ape->compiler);
    global_store_destroy(ape->global_store);
    gcmem_destroy(ape->mem);
    ape->files->destroyWithItems(compiled_file_destroy);
    errors_deinit(&ape->errors);
}

Object ape_native_fn_wrapper(VM* vm, void* data, int argc, Object* args)
{
    Object res;
    NativeFuncWrapper* wrapper;
    (void)vm;
    wrapper = (NativeFuncWrapper*)data;
    APE_ASSERT(vm == wrapper->ape->vm);
    res = wrapper->wrapped_funcptr(wrapper->ape, wrapper->data, argc, (Object*)args);
    if(ape_has_errors(wrapper->ape))
    {
        return object_make_null();
    }
    return res;
}

Object ape_object_make_native_function_with_name(Context* ape, const char* name, UserFNCallback fn, void* data)
{
    NativeFuncWrapper wrapper;
    memset(&wrapper, 0, sizeof(NativeFuncWrapper));
    wrapper.wrapped_funcptr = fn;
    wrapper.ape = ape;
    wrapper.data = data;
    Object wrapper_native_function = object_make_native_function_memory(ape->mem, name, ape_native_fn_wrapper, &wrapper, sizeof(wrapper));
    if(object_is_null(wrapper_native_function))
    {
        return object_make_null();
    }
    return wrapper_native_function;
}

void reset_state(Context* ape)
{
    ape_clear_errors(ape);
    vm_reset(ape->vm);
}

static void set_default_config(Context* ape)
{
    memset(&ape->config, 0, sizeof(Config));
    ape_set_repl_mode(ape, false);
    ape_set_timeout(ape, -1);
    ape_set_file_read_function(ape, read_file_default, ape);
    ape_set_file_write_function(ape, write_file_default, ape);
    ape_set_stdout_write_function(ape, stdout_write_default, ape);
}

static char* read_file_default(void* ctx, const char* filename)
{
    long pos;
    size_t size_read;
    size_t size_to_read;
    char* file_contents;
    FILE* fp;
    Context* ape;
    ape = (Context*)ctx;
    fp = fopen(filename, "r");
    size_to_read = 0;
    size_read = 0;
    if(!fp)
    {
        return NULL;
    }
    fseek(fp, 0L, SEEK_END);
    pos = ftell(fp);
    if(pos < 0)
    {
        fclose(fp);
        return NULL;
    }
    size_to_read = pos;
    rewind(fp);
    file_contents = (char*)allocator_malloc(&ape->alloc, sizeof(char) * (size_to_read + 1));
    if(!file_contents)
    {
        fclose(fp);
        return NULL;
    }
    size_read = fread(file_contents, 1, size_to_read, fp);
    if(ferror(fp))
    {
        fclose(fp);
        free(file_contents);
        return NULL;
    }
    fclose(fp);
    file_contents[size_read] = '\0';
    return file_contents;
}

static size_t write_file_default(void* ctx, const char* path, const char* string, size_t string_size)
{
    size_t written;
    FILE* fp;
    (void)ctx;
    fp = fopen(path, "w");
    if(!fp)
    {
        return 0;
    }
    written = fwrite(string, 1, string_size, fp);
    fclose(fp);
    return written;
}

static size_t stdout_write_default(void* ctx, const void* data, size_t size)
{
    (void)ctx;
    return fwrite(data, 1, size, stdout);
}

void* ape_malloc(void* ctx, size_t size)
{
    void* res;
    Context* ape;
    ape = (Context*)ctx;
    res = (void*)allocator_malloc(&ape->custom_allocator, size);
    if(!res)
    {
        errors_add_error(&ape->errors, APE_ERROR_ALLOCATION, src_pos_invalid, "Allocation failed");
    }
    return res;
}

void ape_free(void* ctx, void* ptr)
{
    Context* ape;
    ape = (Context*)ctx;
    allocator_free(&ape->custom_allocator, ptr);
}

#if __has_include(<dirent.h>)
#else
    #define CORE_NODIRENT
#endif


#if 0
    #define CHECK_ARGS(vm, generate_error, argc, args, ...) \
        check_args( \
            (vm), \
            (generate_error), \
            (argc), \
            (args),  \
            (sizeof((ObjectType[]){ __VA_ARGS__ }) / sizeof(ObjectType)), \
            ((ObjectType*)((ObjectType[]){ __VA_ARGS__ })) \
        )
#else
    #define CHECK_ARGS(vm, generate_error, argc, args, ...) \
        check_args( \
            (vm), \
            (generate_error), \
            (argc), \
            (args),  \
            (sizeof((int[]){ __VA_ARGS__ }) / sizeof(int)), \
            typargs_to_array(__VA_ARGS__, -1) \
        )
#endif

//Object object_make_native_function(GCMemory* mem, const char* name, NativeFNCallback fn, void* data, int data_len);
#define make_fn_data(vm, name, fnc, dataptr, datasize) \
    object_make_native_function_memory(vm->mem, name, fnc, dataptr, datasize)

#define make_fn(vm, name, fnc) \
    make_fn_data(vm, name, fnc, NULL, 0)

#define make_fn_entry_data(vm, map, name, fnc, dataptr, datasize) \
    ape_object_set_map_value(map, name, make_fn(vm, name, fnc))

#define make_fn_entry(vm, map, name, fnc) \
    make_fn_entry_data(vm, map, name, fnc, NULL, 0)


static ObjectType* typargs_to_array(int first, ...)
{
    int idx;
    int thing;
    va_list va;
    static ObjectType rt[20];
    idx = 0;
    rt[idx] = (ObjectType)first;
    va_start(va, first);
    idx++;
    while(true)
    {
        thing = va_arg(va, int);
        if(thing == -1)
        {
            break;
        }
        rt[idx] = (ObjectType)thing;
        idx++;
    }
    va_end(va);
    return rt;
}

static bool check_args(VM* vm, bool generate_error, int argc, Object* args, int expected_argc, const ObjectType* expected_types)
{
    if(argc != expected_argc)
    {
        if(generate_error)
        {
            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid,
                              "Invalid number or arguments, got %d instead of %d", argc, expected_argc);
        }
        return false;
    }

    for(int i = 0; i < argc; i++)
    {
        Object arg = args[i];
        ObjectType type = object_get_type(arg);
        ObjectType expected_type = expected_types[i];
        if(!(type & expected_type))
        {
            if(generate_error)
            {
                const char* type_str = object_get_type_name(type);
                char* expected_type_str = object_get_type_union_name(vm->alloc, expected_type);
                if(!expected_type_str)
                {
                    return false;
                }
                errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid,
                                  "Invalid argument %d type, got %s, expected %s", i, type_str, expected_type_str);
                allocator_free(vm->alloc, expected_type_str);
            }
            return false;
        }
    }
    return true;
}

// INTERNAL
static Object cfn_len(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING | APE_OBJECT_ARRAY | APE_OBJECT_MAP))
    {
        return object_make_null();
    }

    Object arg = args[0];
    ObjectType type = object_get_type(arg);
    if(type == APE_OBJECT_STRING)
    {
        int len = object_get_string_length(arg);
        return object_make_number(len);
    }
    else if(type == APE_OBJECT_ARRAY)
    {
        int len = object_get_array_length(arg);
        return object_make_number(len);
    }
    else if(type == APE_OBJECT_MAP)
    {
        int len = object_get_map_length(arg);
        return object_make_number(len);
    }

    return object_make_null();
}

static Object cfn_first(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY))
    {
        return object_make_null();
    }
    Object arg = args[0];
    return object_get_array_value(arg, 0);
}

static Object cfn_last(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY))
    {
        return object_make_null();
    }
    Object arg = args[0];
    return object_get_array_value(arg, object_get_array_length(arg) - 1);
}

static Object cfn_rest(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY))
    {
        return object_make_null();
    }
    Object arg = args[0];
    int len = object_get_array_length(arg);
    if(len == 0)
    {
        return object_make_null();
    }

    Object res = object_make_array(vm->mem);
    if(object_is_null(res))
    {
        return object_make_null();
    }
    for(int i = 1; i < len; i++)
    {
        Object item = object_get_array_value(arg, i);
        bool ok = object_add_array_value(res, item);
        if(!ok)
        {
            return object_make_null();
        }
    }
    return res;
}

static Object cfn_reverse(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY | APE_OBJECT_STRING))
    {
        return object_make_null();
    }
    Object arg = args[0];
    ObjectType type = object_get_type(arg);
    if(type == APE_OBJECT_ARRAY)
    {
        int len = object_get_array_length(arg);
        Object res = object_make_array_with_capacity(vm->mem, len);
        if(object_is_null(res))
        {
            return object_make_null();
        }
        for(int i = 0; i < len; i++)
        {
            Object obj = object_get_array_value(arg, i);
            bool ok = object_set_array_value_at(res, len - i - 1, obj);
            if(!ok)
            {
                return object_make_null();
            }
        }
        return res;
    }
    else if(type == APE_OBJECT_STRING)
    {
        const char* str = object_get_string(arg);
        int len = object_get_string_length(arg);

        Object res = object_make_string_with_capacity(vm->mem, len);
        if(object_is_null(res))
        {
            return object_make_null();
        }
        char* res_buf = object_get_mutable_string(res);
        for(int i = 0; i < len; i++)
        {
            res_buf[len - i - 1] = str[i];
        }
        res_buf[len] = '\0';
        object_set_string_length(res, len);
        return res;
    }
    return object_make_null();
}

static Object cfn_array(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(argc == 1)
    {
        if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
        {
            return object_make_null();
        }
        int capacity = (int)object_get_number(args[0]);
        Object res = object_make_array_with_capacity(vm->mem, capacity);
        if(object_is_null(res))
        {
            return object_make_null();
        }
        Object obj_null = object_make_null();
        for(int i = 0; i < capacity; i++)
        {
            bool ok = object_add_array_value(res, obj_null);
            if(!ok)
            {
                return object_make_null();
            }
        }
        return res;
    }
    else if(argc == 2)
    {
        if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER, APE_OBJECT_ANY))
        {
            return object_make_null();
        }
        int capacity = (int)object_get_number(args[0]);
        Object res = object_make_array_with_capacity(vm->mem, capacity);
        if(object_is_null(res))
        {
            return object_make_null();
        }
        for(int i = 0; i < capacity; i++)
        {
            bool ok = object_add_array_value(res, args[1]);
            if(!ok)
            {
                return object_make_null();
            }
        }
        return res;
    }
    CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER);
    return object_make_null();
}

static Object cfn_append(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    bool ok = object_add_array_value(args[0], args[1]);
    if(!ok)
    {
        return object_make_null();
    }
    int len = object_get_array_length(args[0]);
    return object_make_number(len);
}

static Object cfn_println(VM* vm, void* data, int argc, Object* args)
{
    (void)data;

    const Config* config = vm->config;

    if(!config->stdio.write.write)
    {
        return object_make_null();// todo: runtime error?
    }

    StringBuffer* buf = strbuf_make(vm->alloc);
    if(!buf)
    {
        return object_make_null();
    }
    for(int i = 0; i < argc; i++)
    {
        Object arg = args[i];
        object_to_string(arg, buf, false);
    }
    strbuf_append(buf, "\n");
    if(strbuf_failed(buf))
    {
        strbuf_destroy(buf);
        return object_make_null();
    }
    config->stdio.write.write(config->stdio.write.context, strbuf_get_string(buf), strbuf_get_length(buf));
    strbuf_destroy(buf);
    return object_make_null();
}

static Object cfn_print(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    const Config* config = vm->config;

    if(!config->stdio.write.write)
    {
        return object_make_null();// todo: runtime error?
    }

    StringBuffer* buf = strbuf_make(vm->alloc);
    if(!buf)
    {
        return object_make_null();
    }
    for(int i = 0; i < argc; i++)
    {
        Object arg = args[i];
        object_to_string(arg, buf, false);
    }
    if(strbuf_failed(buf))
    {
        strbuf_destroy(buf);
        return object_make_null();
    }
    config->stdio.write.write(config->stdio.write.context, strbuf_get_string(buf), strbuf_get_length(buf));
    strbuf_destroy(buf);
    return object_make_null();
}

static Object cfn_to_str(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING | APE_OBJECT_NUMBER | APE_OBJECT_BOOL | APE_OBJECT_NULL | APE_OBJECT_MAP | APE_OBJECT_ARRAY))
    {
        return object_make_null();
    }
    Object arg = args[0];
    StringBuffer* buf = strbuf_make(vm->alloc);
    if(!buf)
    {
        return object_make_null();
    }
    object_to_string(arg, buf, false);
    if(strbuf_failed(buf))
    {
        strbuf_destroy(buf);
        return object_make_null();
    }
    Object res = object_make_string(vm->mem, strbuf_get_string(buf));
    strbuf_destroy(buf);
    return res;
}

static Object cfn_to_num(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING | APE_OBJECT_NUMBER | APE_OBJECT_BOOL | APE_OBJECT_NULL))
    {
        return object_make_null();
    }
    double result = 0;
    const char* string = "";
    if(object_is_numeric(args[0]))
    {
        result = object_get_number(args[0]);
    }
    else if(object_is_null(args[0]))
    {
        result = 0;
    }
    else if(object_get_type(args[0]) == APE_OBJECT_STRING)
    {
        string = object_get_string(args[0]);
        char* end;
        errno = 0;
        result = strtod(string, &end);
        if(errno == ERANGE && (result <= -HUGE_VAL || result >= HUGE_VAL))
        {
            goto err;
            ;
        }
        if(errno && errno != ERANGE)
        {
            goto err;
        }
        int string_len = object_get_string_length(args[0]);
        int parsed_len = (int)(end - string);
        if(string_len != parsed_len)
        {
            goto err;
        }
    }
    else
    {
        goto err;
    }
    return object_make_number(result);
err:
    errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "Cannot convert \"%s\" to number", string);
    return object_make_null();
}

static Object cfn_chr(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }

    double val = object_get_number(args[0]);

    char c = (char)val;
    char str[2] = { c, '\0' };
    return object_make_string(vm->mem, str);
}

static Object cfn_range(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    for(int i = 0; i < argc; i++)
    {
        ObjectType type = object_get_type(args[i]);
        if(type != APE_OBJECT_NUMBER)
        {
            const char* type_str = object_get_type_name(type);
            const char* expected_str = object_get_type_name(APE_OBJECT_NUMBER);
            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid,
                              "Invalid argument %d passed to range, got %s instead of %s", i, type_str, expected_str);
            return object_make_null();
        }
    }

    int start = 0;
    int end = 0;
    int step = 1;

    if(argc == 1)
    {
        end = (int)object_get_number(args[0]);
    }
    else if(argc == 2)
    {
        start = (int)object_get_number(args[0]);
        end = (int)object_get_number(args[1]);
    }
    else if(argc == 3)
    {
        start = (int)object_get_number(args[0]);
        end = (int)object_get_number(args[1]);
        step = (int)object_get_number(args[2]);
    }
    else
    {
        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "Invalid number of arguments passed to range, got %d", argc);
        return object_make_null();
    }

    if(step == 0)
    {
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "range step cannot be 0");
        return object_make_null();
    }

    Object res = object_make_array(vm->mem);
    if(object_is_null(res))
    {
        return object_make_null();
    }
    for(int i = start; i < end; i += step)
    {
        Object item = object_make_number(i);
        bool ok = object_add_array_value(res, item);
        if(!ok)
        {
            return object_make_null();
        }
    }
    return res;
}

static Object cfn_keys(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_MAP))
    {
        return object_make_null();
    }
    Object arg = args[0];
    Object res = object_make_array(vm->mem);
    if(object_is_null(res))
    {
        return object_make_null();
    }
    int len = object_get_map_length(arg);
    for(int i = 0; i < len; i++)
    {
        Object key = object_get_map_key_at(arg, i);
        bool ok = object_add_array_value(res, key);
        if(!ok)
        {
            return object_make_null();
        }
    }
    return res;
}

static Object cfn_values(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_MAP))
    {
        return object_make_null();
    }
    Object arg = args[0];
    Object res = object_make_array(vm->mem);
    if(object_is_null(res))
    {
        return object_make_null();
    }
    int len = object_get_map_length(arg);
    for(int i = 0; i < len; i++)
    {
        Object key = object_get_map_value_at(arg, i);
        bool ok = object_add_array_value(res, key);
        if(!ok)
        {
            return object_make_null();
        }
    }
    return res;
}

static Object cfn_copy(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_copy(vm->mem, args[0]);
}

static Object cfn_deep_copy(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_deep_copy(vm->mem, args[0]);
}

static Object cfn_concat(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY | APE_OBJECT_STRING, APE_OBJECT_ARRAY | APE_OBJECT_STRING))
    {
        return object_make_null();
    }
    ObjectType type = object_get_type(args[0]);
    ObjectType item_type = object_get_type(args[1]);
    if(type == APE_OBJECT_ARRAY)
    {
        if(item_type != APE_OBJECT_ARRAY)
        {
            const char* item_type_str = object_get_type_name(item_type);
            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "Invalid argument 2 passed to concat, got %s", item_type_str);
            return object_make_null();
        }
        for(int i = 0; i < object_get_array_length(args[1]); i++)
        {
            Object item = object_get_array_value(args[1], i);
            bool ok = object_add_array_value(args[0], item);
            if(!ok)
            {
                return object_make_null();
            }
        }
        return object_make_number(object_get_array_length(args[0]));
    }
    else if(type == APE_OBJECT_STRING)
    {
        if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING, APE_OBJECT_STRING))
        {
            return object_make_null();
        }
        const char* left_val = object_get_string(args[0]);
        int left_len = (int)object_get_string_length(args[0]);

        const char* right_val = object_get_string(args[1]);
        int right_len = (int)object_get_string_length(args[1]);

        Object res = object_make_string_with_capacity(vm->mem, left_len + right_len);
        if(object_is_null(res))
        {
            return object_make_null();
        }

        bool ok = object_string_append(res, left_val, left_len);
        if(!ok)
        {
            return object_make_null();
        }

        ok = object_string_append(res, right_val, right_len);
        if(!ok)
        {
            return object_make_null();
        }

        return res;
    }
    return object_make_null();
}

static Object cfn_remove(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY, APE_OBJECT_ANY))
    {
        return object_make_null();
    }

    int ix = -1;
    for(int i = 0; i < object_get_array_length(args[0]); i++)
    {
        Object obj = object_get_array_value(args[0], i);
        if(object_equals(obj, args[1]))
        {
            ix = i;
            break;
        }
    }

    if(ix == -1)
    {
        return object_make_bool(false);
    }

    bool res = object_remove_array_value_at(args[0], ix);
    return object_make_bool(res);
}

static Object cfn_remove_at(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }

    ObjectType type = object_get_type(args[0]);
    int ix = (int)object_get_number(args[1]);

    switch(type)
    {
        case APE_OBJECT_ARRAY:
        {
            bool res = object_remove_array_value_at(args[0], ix);
            return object_make_bool(res);
        }
        default:
            break;
    }

    return object_make_bool(true);
}


static Object cfn_error(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(argc == 1 && object_get_type(args[0]) == APE_OBJECT_STRING)
    {
        return object_make_error(vm->mem, object_get_string(args[0]));
    }
    else
    {
        return object_make_error(vm->mem, "");
    }
}

static Object cfn_crash(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(argc == 1 && object_get_type(args[0]) == APE_OBJECT_STRING)
    {
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), object_get_string(args[0]));
    }
    else
    {
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "");
    }
    return object_make_null();
}

static Object cfn_assert(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_BOOL))
    {
        return object_make_null();
    }

    if(!object_get_bool(args[0]))
    {
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "assertion failed");
        return object_make_null();
    }

    return object_make_bool(true);
}

static Object cfn_random_seed(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    int seed = (int)object_get_number(args[0]);
    srand(seed);
    return object_make_bool(true);
}

static Object cfn_random(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    double res = (double)rand() / RAND_MAX;
    if(argc == 0)
    {
        return object_make_number(res);
    }
    else if(argc == 2)
    {
        if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER, APE_OBJECT_NUMBER))
        {
            return object_make_null();
        }
        double min = object_get_number(args[0]);
        double max = object_get_number(args[1]);
        if(min >= max)
        {
            errors_add_error(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "max is bigger than min");
            return object_make_null();
        }
        double range = max - min;
        res = min + (res * range);
        return object_make_number(res);
    }
    else
    {
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "Invalid number or arguments");
        return object_make_null();
    }
}

static Object cfn_slice(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING | APE_OBJECT_ARRAY, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    ObjectType arg_type = object_get_type(args[0]);
    int index = (int)object_get_number(args[1]);
    if(arg_type == APE_OBJECT_ARRAY)
    {
        int len = object_get_array_length(args[0]);
        if(index < 0)
        {
            index = len + index;
            if(index < 0)
            {
                index = 0;
            }
        }
        Object res = object_make_array_with_capacity(vm->mem, len - index);
        if(object_is_null(res))
        {
            return object_make_null();
        }
        for(int i = index; i < len; i++)
        {
            Object item = object_get_array_value(args[0], i);
            bool ok = object_add_array_value(res, item);
            if(!ok)
            {
                return object_make_null();
            }
        }
        return res;
    }
    else if(arg_type == APE_OBJECT_STRING)
    {
        const char* str = object_get_string(args[0]);
        int len = (int)object_get_string_length(args[0]);
        if(index < 0)
        {
            index = len + index;
            if(index < 0)
            {
                return object_make_string(vm->mem, "");
            }
        }
        if(index >= len)
        {
            return object_make_string(vm->mem, "");
        }
        int res_len = len - index;
        Object res = object_make_string_with_capacity(vm->mem, res_len);
        if(object_is_null(res))
        {
            return object_make_null();
        }

        char* res_buf = object_get_mutable_string(res);
        memset(res_buf, 0, res_len + 1);
        for(int i = index; i < len; i++)
        {
            char c = str[i];
            res_buf[i - index] = c;
        }
        object_set_string_length(res, res_len);
        return res;
    }
    else
    {
        const char* type_str = object_get_type_name(arg_type);
        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "Invalid argument 0 passed to slice, got %s instead", type_str);
        return object_make_null();
    }
}

//-----------------------------------------------------------------------------
// Type checks
//-----------------------------------------------------------------------------

static Object cfn_is_string(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_STRING);
}

static Object cfn_is_array(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_ARRAY);
}

static Object cfn_is_map(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_MAP);
}

static Object cfn_is_number(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_NUMBER);
}

static Object cfn_is_bool(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_BOOL);
}

static Object cfn_is_null(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_NULL);
}

static Object cfn_is_function(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_FUNCTION);
}

static Object cfn_is_external(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_EXTERNAL);
}

static Object cfn_is_error(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_ERROR);
}

static Object cfn_is_native_function(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_NATIVE_FUNCTION);
}

//-----------------------------------------------------------------------------
// Math
//-----------------------------------------------------------------------------

static Object cfn_sqrt(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = sqrt(arg);
    return object_make_number(res);
}

static Object cfn_pow(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg1 = object_get_number(args[0]);
    double arg2 = object_get_number(args[1]);
    double res = pow(arg1, arg2);
    return object_make_number(res);
}

static Object cfn_sin(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = sin(arg);
    return object_make_number(res);
}

static Object cfn_cos(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = cos(arg);
    return object_make_number(res);
}

static Object cfn_tan(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = tan(arg);
    return object_make_number(res);
}

static Object cfn_log(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = log(arg);
    return object_make_number(res);
}

static Object cfn_ceil(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = ceil(arg);
    return object_make_number(res);
}

static Object cfn_floor(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = floor(arg);
    return object_make_number(res);
}

static Object cfn_abs(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = fabs(arg);
    return object_make_number(res);
}


static Object cfn_file_write(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING, APE_OBJECT_STRING))
    {
        return object_make_null();
    }

    const Config* config = vm->config;

    if(!config->fileio.write_file.write_file)
    {
        return object_make_null();
    }

    const char* path = object_get_string(args[0]);
    const char* string = object_get_string(args[1]);
    int string_len = object_get_string_length(args[1]);

    int written = (int)config->fileio.write_file.write_file(config->fileio.write_file.context, path, string, string_len);

    return object_make_number(written);
}

static Object cfn_file_read(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
    {
        return object_make_null();
    }

    const Config* config = vm->config;

    if(!config->fileio.read_file.read_file)
    {
        return object_make_null();
    }

    const char* path = object_get_string(args[0]);

    char* contents = config->fileio.read_file.read_file(config->fileio.read_file.context, path);
    if(!contents)
    {
        return object_make_null();
    }
    Object res = object_make_string(vm->mem, contents);
    allocator_free(vm->alloc, contents);
    return res;
}

static Object cfn_file_isfile(VM* vm, void* data, int argc, Object* args)
{
    const char* path;
    struct stat st;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
    {
        return object_make_bool(false);
    }
    path = object_get_string(args[0]);
    if(stat(path, &st) != -1)
    {
        if((st.st_mode & S_IFMT) == S_IFREG)
        {
            return object_make_bool(true);
        }
    }
    return object_make_bool(false);
}

static Object cfn_file_isdirectory(VM* vm, void* data, int argc, Object* args)
{
    const char* path;
    struct stat st;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
    {
        return object_make_bool(false);
    }
    path = object_get_string(args[0]);
    if(stat(path, &st) != -1)
    {
        if((st.st_mode & S_IFMT) == S_IFDIR)
        {
            return object_make_bool(true);
        }
    }
    return object_make_bool(false);
}

static Object timespec_to_map(VM* vm, struct timespec ts)
{
    Object map;
    map = object_make_map(vm->mem);
    ape_object_set_map_number(map, "sec", ts.tv_sec);
    ape_object_set_map_number(map, "nsec", ts.tv_nsec);
    return map;
}

static Object cfn_file_stat(VM* vm, void* data, int argc, Object* args)
{
    const char* path;
    struct stat st;
    Object map;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
    {
        return object_make_bool(false);
    }
    path = object_get_string(args[0]);
    if(stat(path, &st) != -1)
    {
        map = object_make_map(vm->mem);
        //ape_object_add_array_string(ary, dent->d_name);
        ape_object_set_map_string(map, "name", path);
        ape_object_set_map_string(map, "path", path);
        ape_object_set_map_number(map, "dev", st.st_dev);
        ape_object_set_map_number(map, "inode", st.st_ino);
        ape_object_set_map_number(map, "mode", st.st_mode);
        ape_object_set_map_number(map, "nlink", st.st_nlink);
        ape_object_set_map_number(map, "uid", st.st_uid);
        ape_object_set_map_number(map, "gid", st.st_gid);
        ape_object_set_map_number(map, "rdev", st.st_rdev);
        ape_object_set_map_number(map, "size", st.st_size);
        ape_object_set_map_number(map, "blksize", st.st_blksize);
        ape_object_set_map_number(map, "blocks", st.st_blocks);
        ape_object_set_map_value(map, "atim", timespec_to_map(vm, st.st_atim));
        ape_object_set_map_value(map, "mtim", timespec_to_map(vm, st.st_mtim));
        ape_object_set_map_value(map, "ctim", timespec_to_map(vm, st.st_ctim));
        return map;
    }
    return object_make_null();
}

static Object objfn_string_length(VM* vm, void* data, int argc, Object* args)
{
    Object self;
    (void)vm;
    (void)data;
    (void)argc;
    self = args[0];
    return object_make_number(object_get_string_length(self));
}


#if !defined(CORE_NODIRENT)

static Object cfn_dir_readdir(VM* vm, void* data, int argc, Object* args)
{
    bool isdir;
    bool isfile;
    const char* path;
    DIR* hnd;
    struct dirent* dent;
    Object ary;
    Object subm;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
    {
        return object_make_null();
    }
    path = object_get_string(args[0]);
    hnd = opendir(path);
    if(hnd == NULL)
    {
        return object_make_null();
    }
    ary = object_make_array(vm->mem);
    while(true)
    {
        dent = readdir(hnd);
        if(dent == NULL)
        {
            break;
        }
        isfile = (dent->d_type == DT_REG);
        isdir = (dent->d_type == DT_DIR);
        subm = object_make_map(vm->mem);
        //ape_object_add_array_string(ary, dent->d_name);
        ape_object_set_map_string(subm, "name", dent->d_name);
        ape_object_set_map_number(subm, "ino", dent->d_ino);
        ape_object_set_map_number(subm, "type", dent->d_type);
        ape_object_set_map_bool(subm, "isfile", isfile);
        ape_object_set_map_bool(subm, "isdir", isdir);
        object_add_array_value(ary, subm);
    }
    closedir(hnd);
    return ary;
}

#endif

//PtrArray * kg_split_string(Allocator* alloc, const char* str, const char* delimiter)
static Object cfn_string_split(VM* vm, void* data, int argc, Object* args)
{
    size_t i;
    size_t len;
    char c;
    const char* str;
    const char* delim;
    char* itm;
    Object arr;
    PtrArray* parr;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING, APE_OBJECT_STRING))
    {
        return object_make_null();
    }
    str = object_get_string(args[0]);
    delim = object_get_string(args[1]);
    arr = object_make_array(vm->mem);
    if(object_get_string_length(args[1]) == 0)
    {
        len = object_get_string_length(args[0]);
        for(i=0; i<len; i++)
        {
            c = str[i];
            object_add_array_value(arr, object_make_string_len(vm->mem, &c, 1));
        }
    }
    else
    {
        parr = kg_split_string(vm->alloc, str, delim);
        for(i=0; i<parr->count(); i++)
        {
            itm = (char*)parr->get(i);
            object_add_array_value(arr, object_make_string(vm->mem, itm));
            allocator_free(vm->alloc, (void*)itm);
        }
        parr->destroy();
    }
    return arr;
}

static NatFunc_t g_core_globalfuncs[] =
{
    { "len", cfn_len },
    { "println", cfn_println },
    { "print", cfn_print },
    { "first", cfn_first },
    { "last", cfn_last },
    { "rest", cfn_rest },
    { "append", cfn_append },
    { "remove", cfn_remove },
    { "remove_at", cfn_remove_at },
    { "to_str", cfn_to_str },
    { "to_num", cfn_to_num },
    { "range", cfn_range },
    { "keys", cfn_keys },
    { "values", cfn_values },
    { "copy", cfn_copy },
    { "deep_copy", cfn_deep_copy },
    { "concat", cfn_concat },
    { "chr", cfn_chr },
    { "reverse", cfn_reverse },
    { "array", cfn_array },
    { "error", cfn_error },
    { "crash", cfn_crash },
    { "assert", cfn_assert },
    { "random_seed", cfn_random_seed },
    { "random", cfn_random },
    { "slice", cfn_slice },

    // Type checks
    { "is_string", cfn_is_string },
    { "is_array", cfn_is_array },
    { "is_map", cfn_is_map },
    { "is_number", cfn_is_number },
    { "is_bool", cfn_is_bool },
    { "is_null", cfn_is_null },
    { "is_function", cfn_is_function },
    { "is_external", cfn_is_external },
    { "is_error", cfn_is_error },
    { "is_native_function", cfn_is_native_function },

    // Math


};

/*
Object.copy instead of clone?
*/

static NatFunc_t g_core_mathfuncs[]=
{
    { "sqrt", cfn_sqrt },
    { "pow", cfn_pow },
    { "sin", cfn_sin },
    { "cos", cfn_cos },
    { "tan", cfn_tan },
    { "log", cfn_log },
    { "ceil", cfn_ceil },
    { "floor", cfn_floor },
    { "abs", cfn_abs },
    {NULL, NULL},
};

static NatFunc_t g_core_filefuncs[]=
{
    {"read", cfn_file_read},
    {"write", cfn_file_write},
    {"isfile", cfn_file_isfile},
    {"isdirectory", cfn_file_isdirectory},
    {"stat", cfn_file_stat},
    {NULL, NULL},
};

static NatFunc_t g_core_dirfuncs[]=
{
    // filesystem
    #if !defined(CORE_NODIRENT)
    {"read", cfn_dir_readdir},
    #endif
    {NULL, NULL},
};


static NatFunc_t g_core_stringfuncs[]=
{
    {"split", cfn_string_split},

    #if 0
    {"trim", cfn_string_trim},

    #endif
    {NULL, NULL},
};


static void setup_namespace(VM* vm, const char* nsname, NatFunc_t* fnarray)
{
    int i;
    Object map;
    map = object_make_map(vm->mem);
    for(i=0; fnarray[i].name != NULL; i++)
    {
        make_fn_entry(vm, map, fnarray[i].name, fnarray[i].fn);
    }
    global_store_set(vm->global_store, nsname, map);
}

void builtins_install(VM* vm)
{
    setup_namespace(vm, "Math", g_core_mathfuncs);
    setup_namespace(vm, "Dir", g_core_dirfuncs);
    setup_namespace(vm, "File", g_core_filefuncs);
    setup_namespace(vm, "String", g_core_stringfuncs);
}

int builtins_count()
{
    return APE_ARRAY_LEN(g_core_globalfuncs);
}

NativeFNCallback builtins_get_fn(int ix)
{
    return g_core_globalfuncs[ix].fn;
}

const char* builtins_get_name(int ix)
{
    return g_core_globalfuncs[ix].name;
}

/*
* this function is meant as a callback for builtin object methods.
*
*  ** IT IS CURRENTLY NOT USED **
*
* since the compiler compiles dot notation (foo.bar) and bracket-notation (foo["bar"]) into
* the same instructions, there is currently no clear way of how and where to best insert
* lookups to these functions.
*/
NativeFNCallback builtin_get_object(ObjectType objt, const char* idxname)
{
    if(objt == APE_OBJECT_STRING)
    {
        if(APE_STREQ(idxname, "length"))
        {
            return objfn_string_length;
        }
    }
    return NULL;
}

#if __has_include(<readline/readline.h>)
#else
    #define NO_READLINE
#endif

enum
{
    MAX_RESTARGS = 1024,
    MAX_OPTS = 1024,
};

typedef struct Flag_t Flag_t;
typedef struct FlagContext_t FlagContext_t;

struct Flag_t
{
    char flag;
    char* value;
};

struct FlagContext_t
{
    int nargc;
    int fcnt;
    int poscnt;
    char* positional[MAX_RESTARGS + 1];
    Flag_t flags[MAX_OPTS + 1];
};

typedef struct Options_t Options_t;

struct Options_t
{
    const char* package;
    const char* filename;
    bool debug;
    int n_paths;
    const char** paths;
    const char* codeline;
};


static bool populate_flags(int argc, int begin, char** argv, const char* expectvalue, FlagContext_t* fx)
{
    int i;
    int nextch;
    int psidx;
    int flidx;
    char* arg;
    char* nextarg;
    psidx = 0;
    flidx = 0;
    fx->fcnt = 0;
    fx->poscnt = 0;
    for(i=begin; i<argc; i++)
    {
        arg = argv[i];
        nextarg = NULL;
        if((i+1) < argc)
        {
            nextarg = argv[i+1];
        }
        if(arg[0] == '-')
        {
            fx->flags[flidx].flag = arg[1];
            fx->flags[flidx].value = NULL;
            if(strchr(expectvalue, arg[1]) != NULL)
            {
                nextch = arg[2];
                /* -e "somecode(...)" */
                /* -e is followed by text: -e"somecode(...)" */
                if(nextch != 0)
                {
                    fx->flags[flidx].value = arg + 2;
                }
                else if(nextarg != NULL)
                {
                    if(nextarg[0] != '-')
                    {
                        fx->flags[flidx].value = nextarg;
                        i++;
                    }
                }
                else
                {
                    fx->flags[flidx].value = NULL;
                }
            }
            flidx++;
        }
        else
        {
            fx->positional[psidx] = arg;
            psidx++;
        }
    }
    fx->fcnt = flidx;
    fx->poscnt = psidx;
    fx->nargc = i;
    return true;
}

static void print_errors(Context *ape)
{
    int i;
    int cn;
    char *err_str;
    const Error *err;
    cn = ape_errors_count(ape);
    for (i = 0; i < cn; i++)
    {
        err = ape_get_error(ape, i);
        err_str = ape_error_serialize(ape, err);
        fprintf(stderr, "%s", err_str);
        ape_free_allocated(ape, err_str);
    }
}

static Object exit_repl(Context *ape, void *data, int argc, Object *args)
{
    bool *exit_repl;
    (void)ape;
    (void)argc;
    (void)args;
    exit_repl = (bool*)data;
    *exit_repl = true;
    return object_make_null();
}

#if !defined(NO_READLINE)
static bool notjustspace(const char* line)
{
    int c;
    size_t i;
    for(i=0; (c = line[i]) != 0; i++)
    {
        if(!isspace(c))
        {
            return true;
        }
    }
    return false;
}

static void do_repl(Context* ape)
{
    size_t len;
    char *line;
    char *object_str;
    Object res;
    ape_set_repl_mode(ape, true);
    ape_set_timeout(ape, 100.0);
    while(true)
    {
        line = readline(">> ");
        if(!line || !notjustspace(line))
        {
            continue;
        }
        add_history(line);
        res = ape_execute(ape, line);
        if (ape_has_errors(ape))
        {
            print_errors(ape);
            free(line);
        }
        else
        {
            object_str = object_serialize(&ape->alloc, res, &len);
            printf("%.*s\n", (int)len, object_str);
            free(object_str);
        }
    }
}
#endif

static void show_usage()
{
    printf("known options:\n");
    printf(
        "  -h          this help.\n"
        "  -e <code>   evaluate <code> and exit.\n"
        "  -p <name>   load package <name>\n"
        "\n"
    );
}

static bool parse_options(Options_t* opts, Flag_t* flags, int fcnt)
{
    int i;
    opts->codeline = NULL;
    opts->package = NULL;
    opts->filename = NULL;
    opts->debug = false;
    for(i=0; i<fcnt; i++)
    {
        switch(flags[i].flag)
        {
            case 'h':
                {
                    show_usage();
                }
                break;
            case 'e':
                {
                    if(flags[i].value == NULL)
                    {
                        fprintf(stderr, "flag '-e' expects a value.\n");
                        return false;
                    }
                    opts->codeline = flags[i].value;
                }
                break;
            case 'd':
                {
                    opts->debug = true;
                }
                break;
            case 'p':
                {
                    if(flags[i].value == NULL)
                    {
                        fprintf(stderr, "flag '-p' expects a value.\n");
                        return false;
                    }
                    opts->package = flags[i].value;
                }
                break;
            default:
                break;
        }
    }
    return true;
}

int main(int argc, char *argv[])
{
    int i;
    bool replexit;
    const char* filename;
    FlagContext_t fx;
    Options_t opts;
    Context *ape;
    Object args_array;
    replexit = false;
    populate_flags(argc, 1, argv, "epI", &fx);
    ape = ape_make();
    if(!parse_options(&opts, fx.flags, fx.fcnt))
    {
        fprintf(stderr, "failed to process command line flags.\n");
        return 1;
    }
    ape_set_native_function(ape, "exit", exit_repl, &replexit);
    if((fx.poscnt > 0) || (opts.codeline != NULL))
    {
        args_array = object_make_array(ape->mem);
        for(i=0; i<fx.poscnt; i++)
        {
            ape_object_add_array_string(args_array, fx.positional[i]);
        }
        ape_set_global_constant(ape, "args", args_array);
        if(opts.codeline)
        {
            ape_execute(ape, opts.codeline);
        }
        else
        {
            filename = fx.positional[0];
            ape_execute_file(ape, filename);
        }
        if(ape_has_errors(ape))
        {
            print_errors(ape);
        }
    }
    else
    {
        #if !defined(NO_READLINE)
            do_repl(ape);
        #else
            fprintf(stderr, "no repl support compiled in\n");
        #endif
    }
    ape_destroy(ape);
    return 0;
}

