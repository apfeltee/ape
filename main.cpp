

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
    vm->call((function_name), sizeof((Object[]){ __VA_ARGS__ }) / sizeof(Object), (Object[]){ __VA_ARGS__ })


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
struct /**/Token;
struct /**/CompiledFile;
struct /**/Lexer;
struct /**/StmtBlock;
struct /**/MapLiteral;
struct /**/StmtPrefix;
struct /**/StmtInfix;
struct /**/StmtFuncDef;
struct /**/StmtCall;
struct /**/StmtIndex;
struct /**/StmtAssign;
struct /**/StmtLogical;
struct /**/StmtTernary;
struct /**/Ident;
struct /**/Expression;
struct /**/StmtDefine;
struct /**/StmtIfClause;
struct /**/StmtWhileLoop;
struct /**/StmtForeach;
struct /**/StmtForLoop;
struct /**/StmtImport;
struct /**/StmtRecover;
struct /**/Expression;
struct /**/Parser;
struct /**/Object;
struct /**/ScriptFunction;
struct /**/NativeFunction;
struct /**/ExternalData;
struct /**/ObjectError;
struct /**/String;
struct /**/ObjectData;
struct /**/Symbol;
struct /**/BlockScope;
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
typedef Object (*UserFNCallback)(Context*, void*, int, Object*);
typedef Object (*NativeFNCallback)(VM*, void*, int, Object*);
typedef void* (*MallocFNCallback)(void*, size_t size);
typedef void (*FreeFNCallback)(void*, void* ptr);
typedef void (*DataDestroyFNCallback)(void*);
typedef void* (*DataCopyFNCallback)(void*);
typedef size_t (*StdoutWriteFNCallback)(void*, const void*, size_t);
typedef char* (*ReadFileFNCallback)(void*, const char*);
typedef size_t (*WriteFileFNCallback)(void*, const char*, const char*, size_t);
typedef unsigned long (*CollectionsHashFNCallback)(const void*);
typedef bool (*CollectionsEqualsFNCallback)(const void*, const void*);
typedef void* (*AllocatorMallocFNCallback)(void*, size_t);
typedef void (*AllocatorFreeFNCallback)(void*, void*);
typedef void (*DictItemDestroyFNCallback)(void*);
typedef void* (*DictItemCopyFNCallback)(void*);
typedef void (*ArrayItemDeinitFNCallback)(void*);
typedef void (*PtrArrayItemDestroyFNCallback)(void*);
typedef void* (*PtrArrayItemCopyFNCallback)(void*);
typedef Expression* (*RightAssocParseFNCallback)(Parser*);
typedef Expression* (*LeftAssocParseFNCallback)(Parser*, Expression*);

typedef void (*ExternalDataDestroyFNCallback)(void* data);
typedef void* (*ExternalDataCopyFNCallback)(void* data);


/* builtins.c */
void builtins_install(VM *vm);
int builtins_count(void);
NativeFNCallback builtins_get_fn(int ix);
const char *builtins_get_name(int ix);
NativeFNCallback builtin_get_object(ObjectType objt, const char *idxname);
/* compiler.c */


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

PtrArray *kg_split_string(Allocator *alloc, const char *str, const char *delimiter);
char *kg_join(Allocator *alloc, PtrArray *items, const char *with);
char *kg_canonicalise_path(Allocator *alloc, const char *path);
bool kg_is_path_absolute(const char *path);
bool kg_streq(const char *a, const char *b);

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


void object_data_deinit(ObjectData *data);

const char *object_get_type_name(const ObjectType type);
char *object_get_type_union_name(Allocator *alloc, const ObjectType type);
char *object_serialize(Allocator *alloc, Object object, size_t *lendest);
Object object_deep_copy(GCMemory *mem, Object obj);
Object object_copy(GCMemory *mem, Object obj);
double object_compare(Object a, Object b, bool *out_ok);
bool object_equals(Object a, Object b);
ExternalData *object_get_external_data(Object object);
bool object_set_external_destroy_function(Object object, ExternalDataDestroyFNCallback destroy_fn);

bool object_get_bool(Object obj);
double object_get_number(Object obj);
const char *object_get_string(Object object);
int object_get_string_length(Object object);
void object_set_string_length(Object object, int len);
int object_get_string_capacity(Object object);
char *object_get_mutable_string(Object object);
bool object_string_append(Object obj, const char *src, int len);
unsigned long object_get_string_hash(Object obj);
ScriptFunction *object_get_function(Object object);
NativeFunction *object_get_native_function(Object obj);

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

char *object_data_get_string(ObjectData *data);
bool object_data_string_reserve_capacity(ObjectData *data, int capacity);


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



void ape_program_destroy(Program *program);
bool ape_object_disable_gc(Object ape_obj);
void ape_object_enable_gc(Object ape_obj);

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


int ape_traceback_get_depth(const Traceback *ape_traceback);
const char *ape_traceback_get_filepath(const Traceback *ape_traceback, int depth);
const char *ape_traceback_get_line(const Traceback *ape_traceback, int depth);
int ape_traceback_get_line_number(const Traceback *ape_traceback, int depth);
int ape_traceback_get_column_number(const Traceback *ape_traceback, int depth);
const char *ape_traceback_get_function_name(const Traceback *ape_traceback, int depth);
Object ape_native_fn_wrapper(VM *vm, void *data, int argc, Object *args);
void *ape_malloc(void*, size_t size);
void ape_free(void*, void *ptr);


/* main.c */
int main(int argc, char *argv[]);
/* parser.c */

StmtBlock *code_block_make(Allocator *alloc, PtrArray *statements);
void code_block_destroy(StmtBlock *block);
StmtBlock *code_block_copy(StmtBlock *block);
void ident_destroy(Ident *ident);



/* tostring.c */
void statements_to_string(StringBuffer* buf, PtrArray *statements);
void statement_to_string(const Expression *stmt, StringBuffer *buf);
void code_block_to_string(const StmtBlock *stmt, StringBuffer *buf);
const char *operator_to_string(Operator op);
const char *expression_type_to_string(Expr_type type);
void code_to_string(uint8_t *code, Position *source_positions, size_t code_size, StringBuffer *res);
void object_to_string(Object obj, StringBuffer *buf, bool quote_str);
bool traceback_to_string(const Traceback *traceback, StringBuffer *buf);

const char *token_type_to_string(TokenType type);




static unsigned int upper_power_of_two(unsigned int v);

static uint64_t get_type_tag(ObjectType type);
static bool freevals_are_allocated(ScriptFunction *fun);

static char *read_file_default(void*, const char*);
static size_t write_file_default(void*, const char*, const char*, size_t);
static size_t stdout_write_default(void*, const void*, size_t);

struct Allocator
{
    public:
        struct Allocated
        {
            public:
                Allocator* m_alloc;
        };

    public:
        static Allocator make(AllocatorMallocFNCallback malloc_fn, AllocatorFreeFNCallback free_fn, void* pctx)
        {
            Allocator alloc;
            alloc.m_allocfn = malloc_fn;
            alloc.m_freefn = free_fn;
            alloc.m_context = pctx;
            return alloc;
        }

    public:
        AllocatorMallocFNCallback m_allocfn;
        AllocatorFreeFNCallback m_freefn;
        void* m_context;

    public:
        template<typename Type>
        Type* allocate()
        {
            return (Type*)allocate(sizeof(Type));
        }

        void* allocate(size_t size)
        {
            if(!this || !m_allocfn)
            {
                return ::malloc(size);
            }
            return m_allocfn(m_context, size);
        }

        void release(void* ptr)
        {
            if(!this || !m_freefn)
            {
                ::free(ptr);
                return;
            }
            m_freefn(m_context, ptr);
        }
};



struct NatFunc_t
{
    const char* name;
    NativeFNCallback fn;
};

struct Position
{
    public:
        static Position make(const CompiledFile* file, int line, int column)
        {
            return (Position){
                .file = file,
                .line = line,
                .column = column,
            };
        }

        static inline constexpr Position invalid()
        {
            return Position{NULL, -1, -1};
        }

        static inline constexpr Position zero()
        {
            return Position{NULL, 0, 0};
        }

    public:
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

struct ValDictionary: public Allocator::Allocated
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
            dict = alloc->allocate<ValDictionary>();
            capacity = upper_power_of_two(min_capacity * 2);
            if(!dict)
            {
                return NULL;
            }
            ok = dict->init(alloc, key_size, val_size, capacity);
            if(!ok)
            {
                alloc->release(dict);
                return NULL;
            }
            return dict;
        }

    public:
        size_t m_keysize;
        size_t m_valsize;
        unsigned int* m_cells;
        unsigned long* m_hashes;
        void* m_keylist;
        void* m_valueslist;
        unsigned int* m_cellindices;
        unsigned int m_count;
        unsigned int m_itemcapacity;
        unsigned int m_cellcapacity;
        CollectionsHashFNCallback m_fnhash;
        CollectionsEqualsFNCallback m_fnequals;

    public:
        void destroy()
        {
            if(!this)
            {
                return;
            }
            Allocator* alloc = m_alloc;
            this->deinit();
            alloc->release(this);
        }

        void setHashFunction(CollectionsHashFNCallback hash_fn)
        {
            m_fnhash = hash_fn;
        }

        void setEqualsFunction(CollectionsEqualsFNCallback equals_fn)
        {
            m_fnequals = equals_fn;
        }

        bool set(void* key, void* value)
        {
            unsigned long hash = this->hashKey(key);
            bool found = false;
            unsigned int cell_ix = this->getCellIndex(key, hash, &found);
            if(found)
            {
                unsigned int item_ix = m_cells[cell_ix];
                this->setValueAt(item_ix, value);
                return true;
            }
            if(m_count >= m_itemcapacity)
            {
                bool ok = this->growAndRehash();
                if(!ok)
                {
                    return false;
                }
                cell_ix = this->getCellIndex(key, hash, &found);
            }
            unsigned int last_ix = m_count;
            m_count++;
            m_cells[cell_ix] = last_ix;
            this->setKeyAt(last_ix, key);
            this->setValueAt(last_ix, value);
            m_cellindices[last_ix] = cell_ix;
            m_hashes[last_ix] = hash;
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
            unsigned int item_ix = m_cells[cell_ix];
            return this->getValueAt(item_ix);
        }

        void* getKeyAt(unsigned int ix) const
        {
            if(ix >= m_count)
            {
                return NULL;
            }
            return (char*)m_keylist + (m_keysize * ix);
        }

        void* getValueAt(unsigned int ix) const
        {
            if(ix >= m_count)
            {
                return NULL;
            }
            return (char*)m_valueslist + (m_valsize * ix);
        }

        unsigned int getCapacity()
        {
            return m_itemcapacity;
        }

        bool setValueAt(unsigned int ix, const void* value)
        {
            if(ix >= m_count)
            {
                return false;
            }
            size_t offset = ix * m_valsize;
            memcpy((char*)m_valueslist + offset, value, m_valsize);
            return true;
        }

        int count() const
        {
            if(!this)
            {
                return 0;
            }
            return m_count;
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

            unsigned int item_ix = m_cells[cell];
            unsigned int last_item_ix = m_count - 1;
            if(item_ix < last_item_ix)
            {
                void* last_key = this->getKeyAt(last_item_ix);
                this->setKeyAt( item_ix, last_key);
                void* last_value = this->getKeyAt(last_item_ix);
                this->setValueAt(item_ix, last_value);
                m_cellindices[item_ix] = m_cellindices[last_item_ix];
                m_hashes[item_ix] = m_hashes[last_item_ix];
                m_cells[m_cellindices[item_ix]] = item_ix;
            }
            m_count--;

            unsigned int i = cell;
            unsigned int j = i;
            for(unsigned int x = 0; x < (m_cellcapacity - 1); x++)
            {
                j = (j + 1) & (m_cellcapacity - 1);
                if(m_cells[j] == VALDICT_INVALID_IX)
                {
                    break;
                }
                unsigned int k = m_hashes[m_cells[j]] & (m_cellcapacity - 1);
                if((j > i && (k <= i || k > j)) || (j < i && (k <= i && k > j)))
                {
                    m_cellindices[m_cells[j]] = i;
                    m_cells[i] = m_cells[j];
                    i = j;
                }
            }
            m_cells[i] = VALDICT_INVALID_IX;
            return true;
        }

        void clear()
        {
            m_count = 0;
            for(unsigned int i = 0; i < m_cellcapacity; i++)
            {
                m_cells[i] = VALDICT_INVALID_IX;
            }
        }

        // Private definitions
        bool init(Allocator* alloc, size_t key_size, size_t val_size, unsigned int initial_capacity)
        {
            m_alloc = alloc;
            m_keysize = key_size;
            m_valsize = val_size;
            m_cells = NULL;
            m_keylist = NULL;
            m_valueslist = NULL;
            m_cellindices = NULL;
            m_hashes = NULL;
            m_count = 0;
            m_cellcapacity = initial_capacity;
            m_itemcapacity = (unsigned int)(initial_capacity * 0.7f);
            m_fnequals = NULL;
            m_fnhash = NULL;
            m_cells = (unsigned int*)m_alloc->allocate(m_cellcapacity * sizeof(*m_cells));
            m_keylist = m_alloc->allocate(m_itemcapacity * key_size);
            m_valueslist = m_alloc->allocate(m_itemcapacity * val_size);
            m_cellindices = (unsigned int*)m_alloc->allocate(m_itemcapacity * sizeof(*m_cellindices));
            m_hashes = (long unsigned int*)m_alloc->allocate(m_itemcapacity * sizeof(*m_hashes));
            if(m_cells == NULL || m_keylist == NULL || m_valueslist == NULL || m_cellindices == NULL || m_hashes == NULL)
            {
                goto error;
            }
            for(unsigned int i = 0; i < m_cellcapacity; i++)
            {
                m_cells[i] = VALDICT_INVALID_IX;
            }
            return true;
        error:
            m_alloc->release(m_cells);
            m_alloc->release(m_keylist);
            m_alloc->release(m_valueslist);
            m_alloc->release(m_cellindices);
            m_alloc->release(m_hashes);
            return false;
        }

        void deinit()
        {
            m_keysize = 0;
            m_valsize = 0;
            m_count = 0;
            m_itemcapacity = 0;
            m_cellcapacity = 0;

            m_alloc->release(m_cells);
            m_alloc->release(m_keylist);
            m_alloc->release(m_valueslist);
            m_alloc->release(m_cellindices);
            m_alloc->release(m_hashes);

            m_cells = NULL;
            m_keylist = NULL;
            m_valueslist = NULL;
            m_cellindices = NULL;
            m_hashes = NULL;
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
            //fprintf(stderr, "valdict_get_cell_ix: this=%p, m_cellcapacity=%d\n", this, m_cellcapacity);
            ofs = 0;
            if(m_cellcapacity > 1)
            {
                ofs = (m_cellcapacity - 1);
            }
            cell_ix = hash & ofs;
            for(i = 0; i < m_cellcapacity; i++)
            {
                cell = VALDICT_INVALID_IX;
                ix = (cell_ix + i) & ofs;
                //fprintf(stderr, "(cell_ix=%d + i=%d) & ofs=%d == %d\n", cell_ix, i, ofs, ix);
                cell = m_cells[ix];
                if(cell == VALDICT_INVALID_IX)
                {
                    return ix;
                }
                hash_to_check = m_hashes[cell];
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
            unsigned new_capacity = m_cellcapacity == 0 ? DICT_INITIAL_SIZE : m_cellcapacity * 2;
            bool ok = new_dict.init(m_alloc, m_keysize, m_valsize, new_capacity);
            if(!ok)
            {
                return false;
            }
            new_dict.m_fnequals = m_fnequals;
            new_dict.m_fnhash = m_fnhash;
            for(unsigned int i = 0; i < m_count; i++)
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
            if(ix >= m_count)
            {
                return false;
            }
            size_t offset = ix * m_keysize;
            memcpy((char*)m_keylist + offset, key, m_keysize);
            return true;
        }

        bool keysAreEqual(const void* a, const void* b) const
        {
            if(m_fnequals)
            {
                return m_fnequals(a, b);
            }
            else
            {
                return memcmp(a, b, m_keysize) == 0;
            }
        }

        unsigned long hashKey(const void* key) const
        {
            if(m_fnhash)
            {
                return m_fnhash(key);
            }
            else
            {
                return collections_hash(key, m_keysize);
            }
        }

};

struct Dictionary: public Allocator::Allocated
{
    public:
        template<typename TypeFNCopy, typename TypeFNDestroy>
        static Dictionary* make(Allocator* alloc, TypeFNCopy copy_fn, TypeFNDestroy destroy_fn)
        {
            Dictionary* dict = alloc->allocate<Dictionary>();
            if(dict == NULL)
            {
                return NULL;
            }
            bool ok = dict->init(alloc, DICT_INITIAL_SIZE, (DictItemCopyFNCallback)copy_fn, (DictItemDestroyFNCallback)destroy_fn);
            if(!ok)
            {
                alloc->release(dict);
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
        unsigned int* m_cells;
        unsigned long* m_hashes;
        char** m_keylist;
        void** m_valueslist;
        unsigned int* m_cellindices;
        unsigned int m_count;
        unsigned int m_itemcapacity;
        unsigned int m_cellcapacity;
        DictItemCopyFNCallback m_fncopy;
        DictItemDestroyFNCallback m_fndestroy;

    public:
        void destroy()
        {
            if(!this)
            {
                return;
            }
            Allocator* alloc = m_alloc;
            this->deinit(true);
            m_alloc->release(this);
        }

        void destroyWithItems()
        {
            unsigned int i;
            if(!this)
            {
                return;
            }
            if(m_fndestroy)
            {
                for(i = 0; i < m_count; i++)
                {
                    m_fndestroy(m_valueslist[i]);
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
            if(!m_fncopy || !m_fndestroy)
            {
                return NULL;
            }

            dict_copy = Dictionary::make(m_alloc, m_fncopy, m_fndestroy);
            if(!dict_copy)
            {
                return NULL;
            }
            for(i = 0; i < this->count(); i++)
            {
                key = this->getKeyAt(i);
                item = this->getValueAt( i);
                item_copy = dict_copy->m_fncopy(item);
                if(item && !item_copy)
                {
                    dict_copy->destroyWithItems();
                    return NULL;
                }
                ok = dict_copy->set(key, item_copy);
                if(!ok)
                {
                    dict_copy->m_fndestroy(item_copy);
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
            unsigned int item_ix = m_cells[cell_ix];
            return m_valueslist[item_ix];
        }

        void* getValueAt(unsigned int ix)
        {
            if(ix >= m_count)
            {
                return NULL;
            }
            return m_valueslist[ix];
        }

        const char* getKeyAt(unsigned int ix)
        {
            if(ix >= m_count)
            {
                return NULL;
            }
            return m_keylist[ix];
        }

        int count()
        {
            if(!this)
            {
                return 0;
            }
            return m_count;
        }


        // Private definitions
        bool init(Allocator* alloc, unsigned int initial_capacity, DictItemCopyFNCallback copy_fn, DictItemDestroyFNCallback destroy_fn)
        {
            m_alloc = alloc;
            m_cells = NULL;
            m_keylist = NULL;
            m_valueslist = NULL;
            m_cellindices = NULL;
            m_hashes = NULL;
            m_count = 0;
            m_cellcapacity = initial_capacity;
            m_itemcapacity = (unsigned int)(initial_capacity * 0.7f);
            m_fncopy = copy_fn;
            m_fndestroy = destroy_fn;
            m_cells = (unsigned int*)alloc->allocate(m_cellcapacity * sizeof(*m_cells));
            m_keylist = (char**)alloc->allocate(m_itemcapacity * sizeof(*m_keylist));
            m_valueslist = (void**)alloc->allocate(m_itemcapacity * sizeof(*m_valueslist));
            m_cellindices = (unsigned int*)alloc->allocate(m_itemcapacity * sizeof(*m_cellindices));
            m_hashes = (long unsigned int*)alloc->allocate(m_itemcapacity * sizeof(*m_hashes));
            if(m_cells == NULL || m_keylist == NULL || m_valueslist == NULL || m_cellindices == NULL || m_hashes == NULL)
            {
                goto error;
            }
            for(unsigned int i = 0; i < m_cellcapacity; i++)
            {
                m_cells[i] = DICT_INVALID_IX;
            }
            return true;
        error:
            m_alloc->release(m_cells);
            m_alloc->release(m_keylist);
            m_alloc->release(m_valueslist);
            m_alloc->release(m_cellindices);
            m_alloc->release(m_hashes);
            return false;
        }

        void deinit(bool free_keys)
        {
            if(free_keys)
            {
                for(unsigned int i = 0; i < m_count; i++)
                {
                    m_alloc->release(m_keylist[i]);
                }
            }
            m_count = 0;
            m_itemcapacity = 0;
            m_cellcapacity = 0;
            m_alloc->release(m_cells);
            m_alloc->release(m_keylist);
            m_alloc->release(m_valueslist);
            m_alloc->release(m_cellindices);
            m_alloc->release(m_hashes);
            m_cells = NULL;
            m_keylist = NULL;
            m_valueslist = NULL;
            m_cellindices = NULL;
            m_hashes = NULL;
        }

        unsigned int getCellIndex(const char* key, unsigned long hash, bool* out_found)
        {
            *out_found = false;
            unsigned int cell_ix = hash & (m_cellcapacity - 1);
            for(unsigned int i = 0; i < m_cellcapacity; i++)
            {
                unsigned int ix = (cell_ix + i) & (m_cellcapacity - 1);
                unsigned int cell = m_cells[ix];
                if(cell == DICT_INVALID_IX)
                {
                    return ix;
                }
                unsigned long hash_to_check = m_hashes[cell];
                if(hash != hash_to_check)
                {
                    continue;
                }
                const char* key_to_check = m_keylist[cell];
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
            bool ok = new_dict.init(m_alloc, m_cellcapacity * 2, m_fncopy, m_fndestroy);
            if(!ok)
            {
                return false;
            }
            for(unsigned int i = 0; i < m_count; i++)
            {
                char* key = m_keylist[i];
                void* value = m_valueslist[i];
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
                unsigned int item_ix = m_cells[cell_ix];
                m_valueslist[item_ix] = value;
                return true;
            }
            if(m_count >= m_itemcapacity)
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
                m_keylist[m_count] = mkey;
            }
            else
            {
                char* key_copy = collections_strdup(m_alloc, ckey);
                if(!key_copy)
                {
                    return false;
                }
                m_keylist[m_count] = key_copy;
            }
            m_cells[cell_ix] = m_count;
            m_valueslist[m_count] = value;
            m_cellindices[m_count] = cell_ix;
            m_hashes[m_count] = hash;
            m_count++;
            return true;
        }

};

struct Array: public Allocator::Allocated
{
    public:
        template<typename Type>
        static Array* make(Allocator* alloc)
        {
            return Array::make(alloc, 32, sizeof(Type));
        }

        static Array* make(Allocator* alloc, unsigned int capacity, size_t element_size)
        {
            Array* arr = alloc->allocate<Array>();
            if(!arr)
            {
                return NULL;
            }

            bool ok = arr->init(alloc, capacity, element_size);
            if(!ok)
            {
                alloc->release(arr);
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
            Allocator* alloc = arr->m_alloc;
            arr->deinit();
            alloc->release(arr);
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
            Array* copy = arr->m_alloc->allocate<Array>();
            if(!copy)
            {
                return NULL;
            }
            copy->m_alloc = arr->m_alloc;
            copy->m_capacity = arr->m_capacity;
            copy->m_count = arr->m_count;
            copy->m_elemsize = arr->m_elemsize;
            copy->m_iscaplocked = arr->m_iscaplocked;
            if(arr->m_dallocated)
            {
                copy->m_dallocated = (unsigned char*)arr->m_alloc->allocate(arr->m_capacity * arr->m_elemsize);
                if(!copy->m_dallocated)
                {
                    arr->m_alloc->release(copy);
                    return NULL;
                }
                copy->m_arraydata = copy->m_dallocated;
                memcpy(copy->m_dallocated, arr->m_arraydata, arr->m_capacity * arr->m_elemsize);
            }
            else
            {
                copy->m_dallocated = NULL;
                copy->m_arraydata = NULL;
            }

            return copy;
        }


    public:
        unsigned char* m_arraydata;
        unsigned char* m_dallocated;
        unsigned int m_count;
        unsigned int m_capacity;
        size_t m_elemsize;
        bool m_iscaplocked;

    public:
        bool add(const void* value)
        {
            if(m_count >= m_capacity)
            {
                COLLECTIONS_ASSERT(!m_iscaplocked);
                if(m_iscaplocked)
                {
                    return false;
                }
                unsigned int new_capacity = m_capacity > 0 ? m_capacity * 2 : 1;
                unsigned char* new_data = (unsigned char*)m_alloc->allocate(new_capacity * m_elemsize);
                if(!new_data)
                {
                    return false;
                }
                memcpy(new_data, m_arraydata, m_count * m_elemsize);
                m_alloc->release(m_dallocated);
                m_dallocated = new_data;
                m_arraydata = m_dallocated;
                m_capacity = new_capacity;
            }
            if(value)
            {
                memcpy(m_arraydata + (m_count * m_elemsize), value, m_elemsize);
            }
            m_count++;
            return true;
        }

        bool addn(const void* values, int n)
        {
            for(int i = 0; i < n; i++)
            {
                const uint8_t* value = NULL;
                if(values)
                {
                    value = (const uint8_t*)values + (i * m_elemsize);
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
            COLLECTIONS_ASSERT(m_elemsize == source->m_elemsize);
            if(m_elemsize != source->m_elemsize)
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
                    m_count = dest_before_count;
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
            if(m_count <= 0)
            {
                return false;
            }
            if(out_value)
            {
                void* res = (void*)this->get(m_count - 1);
                memcpy(out_value, res, m_elemsize);
            }
            this->removeAt(m_count - 1);
            return true;
        }

        void* top()
        {
            if(m_count <= 0)
            {
                return NULL;
            }
            return (void*)this->get(m_count - 1);
        }

        bool set(unsigned int ix, void* value)
        {
            if(ix >= m_count)
            {
                COLLECTIONS_ASSERT(false);
                return false;
            }
            size_t offset = ix * m_elemsize;
            memmove(m_arraydata + offset, value, m_elemsize);
            return true;
        }

        bool setn(unsigned int ix, void* values, int n)
        {
            for(int i = 0; i < n; i++)
            {
                int dest_ix = ix + i;
                unsigned char* value = (unsigned char*)values + (i * m_elemsize);
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
            if(ix >= m_count)
            {
                COLLECTIONS_ASSERT(false);
                return NULL;
            }
            size_t offset = ix * m_elemsize;
            return m_arraydata + offset;
        }

        const void* getConst(unsigned int ix) const
        {
            if(ix >= m_count)
            {
                COLLECTIONS_ASSERT(false);
                return NULL;
            }
            size_t offset = ix * m_elemsize;
            return m_arraydata + offset;
        }

        void* getLast()
        {
            if(m_count <= 0)
            {
                return NULL;
            }
            return this->get(m_count - 1);
        }

        int count() const
        {
            if(!this)
            {
                return 0;
            }
            return m_count;
        }

        unsigned int getCapacity() const
        {
            return m_capacity;
        }

        bool removeAt(unsigned int ix)
        {
            if(ix >= m_count)
            {
                return false;
            }
            if(ix == 0)
            {
                m_arraydata += m_elemsize;
                m_capacity--;
                m_count--;
                return true;
            }
            if(ix == (m_count - 1))
            {
                m_count--;
                return true;
            }
            size_t to_move_bytes = (m_count - 1 - ix) * m_elemsize;
            void* dest = m_arraydata + (ix * m_elemsize);
            void* src = m_arraydata + ((ix + 1) * m_elemsize);
            memmove(dest, src, to_move_bytes);
            m_count--;
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
            m_count = 0;
        }

        void clearAndDeinit(ArrayItemDeinitFNCallback deinit_fn)
        {
            for(int i = 0; i < this->count(); i++)
            {
                void* item = this->get(i);
                deinit_fn(item);
            }
            m_count = 0;
        }

        void lockCapacity()
        {
            m_iscaplocked = true;
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
            return m_arraydata;
        }

        const void* constData() const
        {
            return m_arraydata;
        }

        void orphanData()
        {
            this->init(m_alloc, 0, m_elemsize);
        }

        bool reverse()
        {
            int cn = this->count();
            if(cn < 2)
            {
                return true;
            }
            void* temp = (void*)m_alloc->allocate(m_elemsize);
            if(!temp)
            {
                return false;
            }
            for(int a_ix = 0; a_ix < (cn / 2); a_ix++)
            {
                int b_ix = cn - a_ix - 1;
                void* a = this->get(a_ix);
                void* b = this->get(b_ix);
                memcpy(temp, a, m_elemsize);
                this->set(a_ix, b);// no need for check because it will be within range
                this->set(b_ix, temp);
            }
            m_alloc->release(temp);
            return true;
        }

        bool init(Allocator* alloc, unsigned int capacity, size_t element_size)
        {
            m_alloc = alloc;
            if(capacity > 0)
            {
                m_dallocated = (unsigned char*)m_alloc->allocate(capacity * element_size);
                m_arraydata = m_dallocated;
                if(!m_dallocated)
                {
                    return false;
                }
            }
            else
            {
                m_dallocated = NULL;
                m_arraydata = NULL;
            }
            m_capacity = capacity;
            m_count = 0;
            m_elemsize = element_size;
            m_iscaplocked = false;
            return true;
        }

        void deinit()
        {
            m_alloc->release(m_dallocated);
        }

};

struct PtrArray: public Allocator::Allocated
{
    public:
        static PtrArray* make(Allocator* alloc)
        {
            return PtrArray::make(alloc, 0);
        }

        static PtrArray* make(Allocator* alloc, unsigned int capacity)
        {
            PtrArray* ptrarr = alloc->allocate<PtrArray>();
            if(!ptrarr)
            {
                return NULL;
            }
            ptrarr->m_alloc = alloc;
            bool ok = ptrarr->m_actualarray.init(alloc, capacity, sizeof(void*));
            if(!ok)
            {
                alloc->release(ptrarr);
                return NULL;
            }
            return ptrarr;
        }


    public:
        Array m_actualarray;

    public:
        void destroy()
        {
            if(!this)
            {
                return;
            }
            m_actualarray.deinit();
            m_alloc->release(this);
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
            PtrArray* arr_copy = PtrArray::make(m_alloc, m_actualarray.m_capacity);
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
            PtrArray* arr_copy = PtrArray::make(m_alloc, m_actualarray.m_capacity);
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
            return m_actualarray.add(&ptr);
        }

        bool set(unsigned int ix, void* ptr)
        {
            return m_actualarray.set(ix, &ptr);
        }

        bool addArray(PtrArray* source)
        {
            return m_actualarray.addArray(&source->m_actualarray);
        }

        void* get(unsigned int ix) const
        {
            void* res = m_actualarray.get(ix);
            if(!res)
            {
                return NULL;
            }
            return *(void**)res;
        }

        const void* getConst(unsigned int ix) const
        {
            const void* res = m_actualarray.getConst(ix);
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
            return m_actualarray.count();
        }

        bool removeAt(unsigned int ix)
        {
            return m_actualarray.removeAt(ix);
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
            m_actualarray.clear();
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
            m_actualarray.lockCapacity();
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
            void* res = m_actualarray.get(ix);
            if(res == NULL)
            {
                return NULL;
            }
            return res;
        }

        void* data()
        {
            return m_actualarray.data();
        }

        void reverse()
        {
            m_actualarray.reverse();
        }

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


struct CompiledFile: public Allocator::Allocated
{
    char* dir_path;
    char* path;
    PtrArray * lines;
};

struct ErrorList
{
    public:
        struct Error
        {
            public:
                ErrorType type;
                char message[APE_ERROR_MESSAGE_MAX_LENGTH];
                Position pos;
                Traceback* traceback;

            public:
                const char* getFilePath() const
                {
                    if(!this->pos.file)
                    {
                        return NULL;
                    }
                    return this->pos.file->path;
                }

                const char* getSource() const
                {
                    if(!this->pos.file)
                    {
                        return NULL;
                    }
                    PtrArray* lines = this->pos.file->lines;
                    if(this->pos.line >= lines->count())
                    {
                        return NULL;
                    }
                    const char* line = (const char*)lines->get(this->pos.line);
                    return line;
                }

                int getLine() const
                {
                    if(this->pos.line < 0)
                    {
                        return -1;
                    }
                    return this->pos.line + 1;
                }

                int getColumn() const
                {
                    if(this->pos.column < 0)
                    {
                        return -1;
                    }
                    return this->pos.column + 1;
                }


                const Traceback* getTraceback() const
                {
                    return (const Traceback*)this->traceback;
                }

        };

    public:
        static const char* toString(ErrorType type)
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



    public:
        Error items[ERRORS_MAX_COUNT];
        int m_count;

    public:
        void init()
        {
            memset(this, 0, sizeof(ErrorList));
            this->m_count = 0;
        }

        void deinit()
        {
            this->clear();
        }

        void addError(ErrorType type, Position pos, const char* message)
        {
            if(this->m_count >= ERRORS_MAX_COUNT)
            {
                return;
            }
            ErrorList::Error err;
            memset(&err, 0, sizeof(ErrorList::Error));
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
            this->items[this->m_count] = err;
            this->m_count++;
        }

        void addFormat(ErrorType type, Position pos, const char* format, ...)
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
            this->addError(type, pos, res);
        }

        void clear()
        {
            int i;
            for(i = 0; i < this->count(); i++)
            {
                Error* error = this->get(i);
                if(error->traceback)
                {
                    traceback_destroy(error->traceback);
                }
            }
            this->m_count = 0;
        }

        int count() const
        {
            return this->m_count;
        }

        Error* get(int ix)
        {
            if(ix >= this->m_count)
            {
                return NULL;
            }
            return &this->items[ix];
        }

        const Error* get(int ix) const
        {
            if(ix >= this->m_count)
            {
                return NULL;
            }
            return &this->items[ix];
        }

        Error* getLast()
        {
            if(this->m_count <= 0)
            {
                return NULL;
            }
            return &this->items[this->m_count - 1];
        }

        bool hasErrors() const
        {
            return this->count() > 0;
        }


};





struct Token
{
    public:
        TokenType type;
        const char* literal;
        int len;
        Position pos;

    public:
        void init(TokenType type, const char* literal, int len)
        {
            this->type = type;
            this->literal = literal;
            this->len = len;
        }

        char* copyLiteral(Allocator* alloc) const
        {
            return ape_strndup(alloc, this->literal, this->len);
        }

};


struct Lexer: public Allocator::Allocated
{
    public:
        ErrorList* m_errors;
        const char* m_sourcedata;
        int m_sourcelen;
        int m_position;
        int m_nextpos;
        char m_currchar;
        int m_currline;
        int m_currcolumn;
        CompiledFile* m_compiledfile;
        bool m_failed;
        bool m_continuetplstring;
        struct
        {
            int position;
            int nextpos;
            char currchar;
            int currline;
            int currcolumn;
        } m_prevstate;
        Token m_prevtoken;
        Token m_currtoken;
        Token m_peektoken;

    public:
        bool init(Allocator* alloc, ErrorList* errs, const char* input, CompiledFile* file)
        {
            m_alloc = alloc;
            m_errors = errs;
            m_sourcedata = input;
            m_sourcelen = (int)strlen(m_sourcedata);
            m_position = 0;
            m_nextpos = 0;
            m_currchar = '\0';
            if(file)
            {
                m_currline = file->lines->count();
            }
            else
            {
                m_currline = 0;
            }
            m_currcolumn = -1;
            m_compiledfile = file;
            bool ok = this->addLine(0);
            if(!ok)
            {
                return false;
            }
            ok = this->readChar();
            if(!ok)
            {
                return false;
            }
            m_failed = false;
            m_continuetplstring = false;

            memset(&m_prevstate, 0, sizeof(m_prevstate));
            m_prevtoken.init(TOKEN_INVALID, NULL, 0);
            m_currtoken.init(TOKEN_INVALID, NULL, 0);
            m_peektoken.init(TOKEN_INVALID, NULL, 0);

            return true;
        }

        bool failed()
        {
            return m_failed;
        }

        void continueTemplateString()
        {
            m_continuetplstring = true;
        }

        bool currentTokenIs(TokenType type)
        {
            return m_currtoken.type == type;
        }

        bool peekTokenIs(TokenType type)
        {
            return m_peektoken.type == type;
        }

        bool nextToken()
        {
            m_prevtoken = m_currtoken;
            m_currtoken = m_peektoken;
            m_peektoken = this->nextTokenInternal();
            return !m_failed;
        }

        bool previousToken()
        {
            if(m_prevtoken.type == TOKEN_INVALID)
            {
                return false;
            }

            m_peektoken = m_currtoken;
            m_currtoken = m_prevtoken;
            m_prevtoken.init(TOKEN_INVALID, NULL, 0);

            m_currchar = m_prevstate.currchar;
            m_currcolumn = m_prevstate.currcolumn;
            m_currline = m_prevstate.currline;
            m_position = m_prevstate.position;
            m_nextpos = m_prevstate.nextpos;

            return true;
        }

        Token nextTokenInternal()
        {
            m_prevstate.currchar = m_currchar;
            m_prevstate.currcolumn = m_currcolumn;
            m_prevstate.currline = m_currline;
            m_prevstate.position = m_position;
            m_prevstate.nextpos = m_nextpos;

            while(true)
            {
                if(!m_continuetplstring)
                {
                    this->skipSpace();
                }

                Token out_tok;
                out_tok.type = TOKEN_INVALID;
                out_tok.literal = m_sourcedata + m_position;
                out_tok.len = 1;
                out_tok.pos = Position::make(m_compiledfile, m_currline, m_currcolumn);

                char c = m_continuetplstring ? '`' : m_currchar;

                switch(c)
                {
                    case '\0':
                        out_tok.init(TOKEN_EOF, "EOF", 3);
                        break;
                    case '=':
                    {
                        if(this->peekChar() == '=')
                        {
                            out_tok.init(TOKEN_EQ, "==", 2);
                            this->readChar();
                        }
                        else
                        {
                            out_tok.init(TOKEN_ASSIGN, "=", 1);
                        }
                        break;
                    }
                    case '&':
                    {
                        if(this->peekChar() == '&')
                        {
                            out_tok.init(TOKEN_AND, "&&", 2);
                            this->readChar();
                        }
                        else if(this->peekChar() == '=')
                        {
                            out_tok.init(TOKEN_BIT_AND_ASSIGN, "&=", 2);
                            this->readChar();
                        }
                        else
                        {
                            out_tok.init(TOKEN_BIT_AND, "&", 1);
                        }
                        break;
                    }
                    case '|':
                    {
                        if(this->peekChar() == '|')
                        {
                            out_tok.init(TOKEN_OR, "||", 2);
                            this->readChar();
                        }
                        else if(this->peekChar() == '=')
                        {
                            out_tok.init(TOKEN_BIT_OR_ASSIGN, "|=", 2);
                            this->readChar();
                        }
                        else
                        {
                            out_tok.init(TOKEN_BIT_OR, "|", 1);
                        }
                        break;
                    }
                    case '^':
                    {
                        if(this->peekChar() == '=')
                        {
                            out_tok.init(TOKEN_BIT_XOR_ASSIGN, "^=", 2);
                            this->readChar();
                        }
                        else
                        {
                            out_tok.init(TOKEN_BIT_XOR, "^", 1);
                            break;
                        }
                        break;
                    }
                    case '+':
                    {
                        if(this->peekChar() == '=')
                        {
                            out_tok.init(TOKEN_PLUS_ASSIGN, "+=", 2);
                            this->readChar();
                        }
                        else if(this->peekChar() == '+')
                        {
                            out_tok.init(TOKEN_PLUS_PLUS, "++", 2);
                            this->readChar();
                        }
                        else
                        {
                            out_tok.init(TOKEN_PLUS, "+", 1);
                            break;
                        }
                        break;
                    }
                    case '-':
                    {
                        if(this->peekChar() == '=')
                        {
                            out_tok.init(TOKEN_MINUS_ASSIGN, "-=", 2);
                            this->readChar();
                        }
                        else if(this->peekChar() == '-')
                        {
                            out_tok.init(TOKEN_MINUS_MINUS, "--", 2);
                            this->readChar();
                        }
                        else
                        {
                            out_tok.init(TOKEN_MINUS, "-", 1);
                            break;
                        }
                        break;
                    }
                    case '!':
                    {
                        if(this->peekChar() == '=')
                        {
                            out_tok.init(TOKEN_NOT_EQ, "!=", 2);
                            this->readChar();
                        }
                        else
                        {
                            out_tok.init(TOKEN_BANG, "!", 1);
                        }
                        break;
                    }
                    case '*':
                    {
                        if(this->peekChar() == '=')
                        {
                            out_tok.init(TOKEN_ASTERISK_ASSIGN, "*=", 2);
                            this->readChar();
                        }
                        else
                        {
                            out_tok.init(TOKEN_ASTERISK, "*", 1);
                            break;
                        }
                        break;
                    }
                    case '/':
                    {
                        if(this->peekChar() == '/')
                        {
                            this->readChar();
                            while(m_currchar != '\n' && m_currchar != '\0')
                            {
                                this->readChar();
                            }
                            continue;
                        }
                        else if(this->peekChar() == '=')
                        {
                            out_tok.init(TOKEN_SLASH_ASSIGN, "/=", 2);
                            this->readChar();
                        }
                        else
                        {
                            out_tok.init(TOKEN_SLASH, "/", 1);
                            break;
                        }
                        break;
                    }
                    case '<':
                    {
                        if(this->peekChar() == '=')
                        {
                            out_tok.init(TOKEN_LTE, "<=", 2);
                            this->readChar();
                        }
                        else if(this->peekChar() == '<')
                        {
                            this->readChar();
                            if(this->peekChar() == '=')
                            {
                                out_tok.init(TOKEN_LSHIFT_ASSIGN, "<<=", 3);
                                this->readChar();
                            }
                            else
                            {
                                out_tok.init(TOKEN_LSHIFT, "<<", 2);
                            }
                        }
                        else
                        {
                            out_tok.init(TOKEN_LT, "<", 1);
                            break;
                        }
                        break;
                    }
                    case '>':
                    {
                        if(this->peekChar() == '=')
                        {
                            out_tok.init(TOKEN_GTE, ">=", 2);
                            this->readChar();
                        }
                        else if(this->peekChar() == '>')
                        {
                            this->readChar();
                            if(this->peekChar() == '=')
                            {
                                out_tok.init(TOKEN_RSHIFT_ASSIGN, ">>=", 3);
                                this->readChar();
                            }
                            else
                            {
                                out_tok.init(TOKEN_RSHIFT, ">>", 2);
                            }
                        }
                        else
                        {
                            out_tok.init(TOKEN_GT, ">", 1);
                        }
                        break;
                    }
                    case ',':
                        out_tok.init(TOKEN_COMMA, ",", 1);
                        break;
                    case ';':
                        out_tok.init(TOKEN_SEMICOLON, ";", 1);
                        break;
                    case ':':
                        out_tok.init(TOKEN_COLON, ":", 1);
                        break;
                    case '(':
                        out_tok.init(TOKEN_LPAREN, "(", 1);
                        break;
                    case ')':
                        out_tok.init(TOKEN_RPAREN, ")", 1);
                        break;
                    case '{':
                        out_tok.init(TOKEN_LBRACE, "{", 1);
                        break;
                    case '}':
                        out_tok.init(TOKEN_RBRACE, "}", 1);
                        break;
                    case '[':
                        out_tok.init(TOKEN_LBRACKET, "[", 1);
                        break;
                    case ']':
                        out_tok.init(TOKEN_RBRACKET, "]", 1);
                        break;
                    case '.':
                        out_tok.init(TOKEN_DOT, ".", 1);
                        break;
                    case '?':
                        out_tok.init(TOKEN_QUESTION, "?", 1);
                        break;
                    case '%':
                    {
                        if(this->peekChar() == '=')
                        {
                            out_tok.init(TOKEN_PERCENT_ASSIGN, "%=", 2);
                            this->readChar();
                        }
                        else
                        {
                            out_tok.init(TOKEN_PERCENT, "%", 1);
                            break;
                        }
                        break;
                    }
                    case '"':
                    {
                        this->readChar();
                        int len;
                        const char* str = this->readString('"', false, NULL, &len);
                        if(str)
                        {
                            out_tok.init(TOKEN_STRING, str, len);
                        }
                        else
                        {
                            out_tok.init(TOKEN_INVALID, NULL, 0);
                        }
                        break;
                    }
                    case '\'':
                    {
                        this->readChar();
                        int len;
                        const char* str = this->readString('\'', false, NULL, &len);
                        if(str)
                        {
                            out_tok.init(TOKEN_STRING, str, len);
                        }
                        else
                        {
                            out_tok.init(TOKEN_INVALID, NULL, 0);
                        }
                        break;
                    }
                    case '`':
                    {
                        if(!m_continuetplstring)
                        {
                            this->readChar();
                        }
                        int len;
                        bool template_found = false;
                        const char* str = this->readString('`', true, &template_found, &len);
                        if(str)
                        {
                            if(template_found)
                            {
                                out_tok.init(TOKEN_TEMPLATE_STRING, str, len);
                            }
                            else
                            {
                                out_tok.init(TOKEN_STRING, str, len);
                            }
                        }
                        else
                        {
                            out_tok.init(TOKEN_INVALID, NULL, 0);
                        }
                        break;
                    }
                    default:
                    {
                        if(is_letter(m_currchar))
                        {
                            int ident_len = 0;
                            const char* ident = this->readIdentifier(&ident_len);
                            TokenType type = lookup_identifier(ident, ident_len);
                            out_tok.init(type, ident, ident_len);
                            return out_tok;
                        }
                        else if(is_digit(m_currchar))
                        {
                            int number_len = 0;
                            const char* number = this->readNumber(&number_len);
                            out_tok.init(TOKEN_NUMBER, number, number_len);
                            return out_tok;
                        }
                        break;
                    }
                }
                this->readChar();
                if(this->failed())
                {
                    out_tok.init(TOKEN_INVALID, NULL, 0);
                }
                m_continuetplstring = false;
                return out_tok;
            }
        }

        bool expectCurrent(TokenType type)
        {
            if(this->failed())
            {
                return false;
            }

            if(!this->currentTokenIs(type))
            {
                const char* expected_type_str = token_type_to_string(type);
                const char* actual_type_str = token_type_to_string(m_currtoken.type);
                m_errors->addFormat(APE_ERROR_PARSING, m_currtoken.pos,
                                  "Expected current token to be \"%s\", got \"%s\" instead", expected_type_str, actual_type_str);
                return false;
            }
            return true;
        }

        // INTERNAL

        bool readChar()
        {
            if(m_nextpos >= m_sourcelen)
            {
                m_currchar = '\0';
            }
            else
            {
                m_currchar = m_sourcedata[m_nextpos];
            }
            m_position = m_nextpos;
            m_nextpos++;

            if(m_currchar == '\n')
            {
                m_currline++;
                m_currcolumn = -1;
                bool ok = this->addLine(m_nextpos);
                if(!ok)
                {
                    m_failed = true;
                    return false;
                }
            }
            else
            {
                m_currcolumn++;
            }
            return true;
        }

        char peekChar()
        {
            if(m_nextpos >= m_sourcelen)
            {
                return '\0';
            }
            else
            {
                return m_sourcedata[m_nextpos];
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

        const char* readIdentifier(int* out_len)
        {
            int position = m_position;
            int len = 0;
            while(is_digit(m_currchar) || is_letter(m_currchar) || m_currchar == ':')
            {
                if(m_currchar == ':')
                {
                    if(this->peekChar() != ':')
                    {
                        goto end;
                    }
                    this->readChar();
                }
                this->readChar();
            }
        end:
            len = m_position - position;
            *out_len = len;
            return m_sourcedata + position;
        }

        const char* readNumber(int* out_len)
        {
            char allowed[] = ".xXaAbBcCdDeEfF";
            int position = m_position;
            while(is_digit(m_currchar) || is_one_of(m_currchar, allowed, APE_ARRAY_LEN(allowed) - 1))
            {
                this->readChar();
            }
            int len = m_position - position;
            *out_len = len;
            return m_sourcedata + position;
        }

        const char* readString(char delimiter, bool is_template, bool* out_template_found, int* out_len)
        {
            *out_len = 0;

            bool escaped = false;
            int position = m_position;

            while(true)
            {
                if(m_currchar == '\0')
                {
                    return NULL;
                }
                if(m_currchar == delimiter && !escaped)
                {
                    break;
                }
                if(is_template && !escaped && m_currchar == '$' && this->peekChar() == '{')
                {
                    *out_template_found = true;
                    break;
                }
                escaped = false;
                if(m_currchar == '\\')
                {
                    escaped = true;
                }
                this->readChar();
            }
            int len = m_position - position;
            *out_len = len;
            return m_sourcedata + position;
        }

        static TokenType lookup_identifier(const char* ident, int len)
        {
            static struct
            {
                const char* value;
                int len;
                TokenType type;
            } keywords[] = {
                { "function", 8, TOKEN_FUNCTION },
                { "const", 5, TOKEN_CONST },
                { "var", 3, TOKEN_VAR },
                { "true", 4, TOKEN_TRUE },
                { "false", 5, TOKEN_FALSE },
                { "if", 2, TOKEN_IF },
                { "else", 4, TOKEN_ELSE },
                { "return", 6, TOKEN_RETURN },
                { "while", 5, TOKEN_WHILE },
                { "break", 5, TOKEN_BREAK },
                { "for", 3, TOKEN_FOR },
                { "in", 2, TOKEN_IN },
                { "continue", 8, TOKEN_CONTINUE },
                { "null", 4, TOKEN_NULL },
                { "import", 6, TOKEN_IMPORT },
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

        void skipSpace()
        {
            char ch = m_currchar;
            while(ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
            {
                this->readChar();
                ch = m_currchar;
            }
        }

        bool addLine(int offset)
        {
            if(!m_compiledfile)
            {
                return true;
            }

            if(m_currline < m_compiledfile->lines->count())
            {
                return true;
            }

            const char* line_start = m_sourcedata + offset;
            const char* new_line_ptr = strchr(line_start, '\n');
            char* line = NULL;
            if(!new_line_ptr)
            {
                line = ape_strdup(m_alloc, line_start);
            }
            else
            {
                size_t line_len = new_line_ptr - line_start;
                line = ape_strndup(m_alloc, line_start, line_len);
            }
            if(!line)
            {
                m_failed = true;
                return false;
            }
            bool ok = m_compiledfile->lines->add(line);
            if(!ok)
            {
                m_failed = true;
                m_alloc->release(line);
                return false;
            }
            return true;
        }

};

struct StmtBlock: public Allocator::Allocated
{
    PtrArray * statements;
};

struct MapLiteral
{
    PtrArray * m_keylist;
    PtrArray * m_valueslist;
};

struct StmtPrefix
{
    Operator op;
    Expression* right;
};

struct StmtInfix
{
    Operator op;
    Expression* left;
    Expression* right;
};


struct StmtFuncDef
{
    char* name;
    PtrArray * params;
    StmtBlock* body;
};

struct StmtCall
{
    Expression* function;
    PtrArray * args;
};

struct StmtIndex
{
    Expression* left;
    Expression* index;
};

struct StmtAssign
{
    Expression* dest;
    Expression* source;
    bool is_postfix;
};

struct StmtLogical
{
    Operator op;
    Expression* left;
    Expression* right;
};

struct StmtTernary
{
    Expression* test;
    Expression* if_true;
    Expression* if_false;
};

struct Ident: public Allocator::Allocated
{
    public:
    public:
        static Ident* make(Allocator* alloc, Token tok)
        {
            Ident* res = alloc->allocate<Ident>();
            if(!res)
            {
                return NULL;
            }
            res->m_alloc = alloc;
            res->value = tok.copyLiteral(alloc);
            if(!res->value)
            {
                alloc->release(res);
                return NULL;
            }
            res->pos = tok.pos;
            return res;
        }

        static Ident* copy_callback(Ident* ident)
        {
            Ident* res = ident->m_alloc->allocate<Ident>();
            if(!res)
            {
                return NULL;
            }
            res->m_alloc = ident->m_alloc;
            res->value = ape_strdup(ident->m_alloc, ident->value);
            if(!res->value)
            {
                ident->m_alloc->release(res);
                return NULL;
            }
            res->pos = ident->pos;
            return res;
        }

    public:
        char* value;
        Position pos;

    public:

};

struct StmtDefine
{
    Ident* name;
    Expression* value;
    bool assignable;
};

struct StmtIfClause
{
    public:
        struct Case: public Allocator::Allocated
        {
            public:
                static Case* make(Allocator* alloc, Expression* test, StmtBlock* consequence)
                {
                    Case* res = alloc->allocate<Case>();
                    if(!res)
                    {
                        return NULL;
                    }
                    res->m_alloc = alloc;
                    res->test = test;
                    res->consequence = consequence;
                    return res;
                }

                static void destroy_callback(Case* cs)
                {
                    cs->destroy();
                }

                static Case* copy_callback(Case* cs)
                {
                    return cs->copy();
                }

            public:
                Expression* test;
                StmtBlock* consequence;

            public:
                void destroy();

                Case* copy();

        };

    public:
        PtrArray * cases;
        StmtBlock* alternative;
};


struct StmtWhileLoop
{
    Expression* test;
    StmtBlock* body;
};

struct StmtForeach
{
    Ident* iterator;
    Expression* source;
    StmtBlock* body;
};

struct StmtForLoop
{
    Expression* init;
    Expression* test;
    Expression* update;
    StmtBlock* body;
};

struct StmtImport
{
    char* path;
};

struct StmtRecover
{
    Ident* error_ident;
    StmtBlock* body;
};




struct Expression: public Allocator::Allocated
{
    public:
        static Expression* makeBasicExpression(Allocator* alloc, Expr_type type)
        {
            Expression* res = alloc->allocate<Expression>();
            if(!res)
            {
                return NULL;
            }
            res->m_alloc = alloc;
            res->type = type;
            res->pos = Position::invalid();
            return res;
        }

        static Expression* makeIdent(Allocator* alloc, Ident* ident)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_IDENT);
            if(!res)
            {
                return NULL;
            }
            res->ident = ident;
            return res;
        }

        static Expression* makeNumberLiteral(Allocator* alloc, double val)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_NUMBER_LITERAL);
            if(!res)
            {
                return NULL;
            }
            res->number_literal = val;
            return res;
        }

        static Expression* makeBoolLiteral(Allocator* alloc, bool val)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_BOOL_LITERAL);
            if(!res)
            {
                return NULL;
            }
            res->bool_literal = val;
            return res;
        }

        static Expression* makeStringLiteral(Allocator* alloc, char* value)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_STRING_LITERAL);
            if(!res)
            {
                return NULL;
            }
            res->string_literal = value;
            return res;
        }

        static Expression* makeNullLiteral(Allocator* alloc)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_NULL_LITERAL);
            if(!res)
            {
                return NULL;
            }
            return res;
        }

        static Expression* makeArrayLiteral(Allocator* alloc, PtrArray * values)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_ARRAY_LITERAL);
            if(!res)
            {
                return NULL;
            }
            res->array = values;
            return res;
        }

        static Expression* makeMapLiteral(Allocator* alloc, PtrArray * keys, PtrArray * values)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_MAP_LITERAL);
            if(!res)
            {
                return NULL;
            }
            res->map.m_keylist = keys;
            res->map.m_valueslist = values;
            return res;
        }

        static Expression* makePrefix(Allocator* alloc, Operator op, Expression* right)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_PREFIX);
            if(!res)
            {
                return NULL;
            }
            res->prefix.op = op;
            res->prefix.right = right;
            return res;
        }

        static Expression* makeInfix(Allocator* alloc, Operator op, Expression* left, Expression* right)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_INFIX);
            if(!res)
            {
                return NULL;
            }
            res->infix.op = op;
            res->infix.left = left;
            res->infix.right = right;
            return res;
        }

        static Expression* makeFunctionLiteral(Allocator* alloc, PtrArray * params, StmtBlock* body)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_FUNCTION_LITERAL);
            if(!res)
            {
                return NULL;
            }
            res->fn_literal.name = NULL;
            res->fn_literal.params = params;
            res->fn_literal.body = body;
            return res;
        }

        static Expression* makeCall(Allocator* alloc, Expression* function, PtrArray * args)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_CALL);
            if(!res)
            {
                return NULL;
            }
            res->call_expr.function = function;
            res->call_expr.args = args;
            return res;
        }

        static Expression* makeIndex(Allocator* alloc, Expression* left, Expression* index)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_INDEX);
            if(!res)
            {
                return NULL;
            }
            res->index_expr.left = left;
            res->index_expr.index = index;
            return res;
        }

        static Expression* makeAssign(Allocator* alloc, Expression* dest, Expression* source, bool is_postfix)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_ASSIGN);
            if(!res)
            {
                return NULL;
            }
            res->assign.dest = dest;
            res->assign.source = source;
            res->assign.is_postfix = is_postfix;
            return res;
        }

        static Expression* makeLogical(Allocator* alloc, Operator op, Expression* left, Expression* right)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_LOGICAL);
            if(!res)
            {
                return NULL;
            }
            res->logical.op = op;
            res->logical.left = left;
            res->logical.right = right;
            return res;
        }

        static Expression* makeTernary(Allocator* alloc, Expression* test, Expression* if_true, Expression* if_false)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_TERNARY);
            if(!res)
            {
                return NULL;
            }
            res->ternary.test = test;
            res->ternary.if_true = if_true;
            res->ternary.if_false = if_false;
            return res;
        }


        static Expression* makeDefine(Allocator* alloc, Ident* name, Expression* value, bool assignable)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_DEFINE);
            if(!res)
            {
                return NULL;
            }
            res->define.name = name;
            res->define.value = value;
            res->define.assignable = assignable;
            return res;
        }

        static Expression* makeIf(Allocator* alloc, PtrArray * cases, StmtBlock* alternative)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_IF);
            if(!res)
            {
                return NULL;
            }
            res->if_statement.cases = cases;
            res->if_statement.alternative = alternative;
            return res;
        }

        static Expression* makeReturn(Allocator* alloc, Expression* value)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_RETURN_VALUE);
            if(!res)
            {
                return NULL;
            }
            res->return_value = value;
            return res;
        }

        static Expression* makeExpression(Allocator* alloc, Expression* value)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_EXPRESSION);
            if(!res)
            {
                return NULL;
            }
            res->expression = value;
            return res;
        }

        static Expression* makeWhileLoop(Allocator* alloc, Expression* test, StmtBlock* body)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_WHILE_LOOP);
            if(!res)
            {
                return NULL;
            }
            res->while_loop.test = test;
            res->while_loop.body = body;
            return res;
        }

        static Expression* makeBreak(Allocator* alloc)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_BREAK);
            if(!res)
            {
                return NULL;
            }
            return res;
        }

        static Expression* makeForeach(Allocator* alloc, Ident* iterator, Expression* source, StmtBlock* body)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_FOREACH);
            if(!res)
            {
                return NULL;
            }
            res->foreach.iterator = iterator;
            res->foreach.source = source;
            res->foreach.body = body;
            return res;
        }

        static Expression* makeForLoop(Allocator* alloc, Expression* init, Expression* test, Expression* update, StmtBlock* body)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_FOR_LOOP);
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

        static Expression* makeContinue(Allocator* alloc)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_CONTINUE);
            if(!res)
            {
                return NULL;
            }
            return res;
        }

        static Expression* makeBlock(Allocator* alloc, StmtBlock* block)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_BLOCK);
            if(!res)
            {
                return NULL;
            }
            res->block = block;
            return res;
        }

        static Expression* makeImport(Allocator* alloc, char* path)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_IMPORT);
            if(!res)
            {
                return NULL;
            }
            res->import.path = path;
            return res;
        }

        static Expression* makeRecover(Allocator* alloc, Ident* error_ident, StmtBlock* body)
        {
            Expression* res = Expression::makeBasicExpression(alloc, EXPRESSION_RECOVER);
            if(!res)
            {
                return NULL;
            }
            res->recover.error_ident = error_ident;
            res->recover.body = body;
            return res;
        }

        static Expression* copyExpression(Expression* expr)
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
                    Ident* ident = Ident::copy_callback(expr->ident);
                    if(!ident)
                    {
                        return NULL;
                    }
                    res = Expression::makeIdent(expr->m_alloc, ident);
                    if(!res)
                    {
                        ident_destroy(ident);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_NUMBER_LITERAL:
                {
                    res = Expression::makeNumberLiteral(expr->m_alloc, expr->number_literal);
                    break;
                }
                case EXPRESSION_BOOL_LITERAL:
                {
                    res = Expression::makeBoolLiteral(expr->m_alloc, expr->bool_literal);
                    break;
                }
                case EXPRESSION_STRING_LITERAL:
                {
                    char* string_copy = ape_strdup(expr->m_alloc, expr->string_literal);
                    if(!string_copy)
                    {
                        return NULL;
                    }
                    res = Expression::makeStringLiteral(expr->m_alloc, string_copy);
                    if(!res)
                    {
                        expr->m_alloc->release(string_copy);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_NULL_LITERAL:
                {
                    res = Expression::makeNullLiteral(expr->m_alloc);
                    break;
                }
                case EXPRESSION_ARRAY_LITERAL:
                {
                    PtrArray* values_copy = expr->array->copyWithItems(Expression::copyExpression, Expression::destroyExpression);
                    if(!values_copy)
                    {
                        return NULL;
                    }
                    res = Expression::makeArrayLiteral(expr->m_alloc, values_copy);
                    if(!res)
                    {
                        values_copy->destroyWithItems(Expression::destroyExpression);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_MAP_LITERAL:
                {
                    PtrArray* keys_copy = expr->map.m_keylist->copyWithItems(Expression::copyExpression, Expression::destroyExpression);
                    PtrArray* values_copy = expr->map.m_valueslist->copyWithItems(Expression::copyExpression, Expression::destroyExpression);
                    if(!keys_copy || !values_copy)
                    {
                        keys_copy->destroyWithItems(Expression::destroyExpression);
                        values_copy->destroyWithItems(Expression::destroyExpression);
                        return NULL;
                    }
                    res = Expression::makeMapLiteral(expr->m_alloc, keys_copy, values_copy);
                    if(!res)
                    {
                        keys_copy->destroyWithItems(Expression::destroyExpression);
                        values_copy->destroyWithItems(Expression::destroyExpression);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_PREFIX:
                {
                    Expression* right_copy = Expression::copyExpression(expr->prefix.right);
                    if(!right_copy)
                    {
                        return NULL;
                    }
                    res = Expression::makePrefix(expr->m_alloc, expr->prefix.op, right_copy);
                    if(!res)
                    {
                        Expression::destroyExpression(right_copy);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_INFIX:
                {
                    Expression* left_copy = Expression::copyExpression(expr->infix.left);
                    Expression* right_copy = Expression::copyExpression(expr->infix.right);
                    if(!left_copy || !right_copy)
                    {
                        Expression::destroyExpression(left_copy);
                        Expression::destroyExpression(right_copy);
                        return NULL;
                    }
                    res = Expression::makeInfix(expr->m_alloc, expr->infix.op, left_copy, right_copy);
                    if(!res)
                    {
                        Expression::destroyExpression(left_copy);
                        Expression::destroyExpression(right_copy);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_FUNCTION_LITERAL:
                {
                    PtrArray* params_copy = expr->fn_literal.params->copyWithItems(Ident::copy_callback, ident_destroy);
                    StmtBlock* body_copy = code_block_copy(expr->fn_literal.body);
                    char* name_copy = ape_strdup(expr->m_alloc, expr->fn_literal.name);
                    if(!params_copy || !body_copy)
                    {
                        params_copy->destroyWithItems(ident_destroy);
                        code_block_destroy(body_copy);
                        expr->m_alloc->release(name_copy);
                        return NULL;
                    }
                    res = Expression::makeFunctionLiteral(expr->m_alloc, params_copy, body_copy);
                    if(!res)
                    {
                        params_copy->destroyWithItems(ident_destroy);
                        code_block_destroy(body_copy);
                        expr->m_alloc->release(name_copy);
                        return NULL;
                    }
                    res->fn_literal.name = name_copy;
                    break;
                }
                case EXPRESSION_CALL:
                {
                    Expression* function_copy = Expression::copyExpression(expr->call_expr.function);
                    PtrArray* args_copy = expr->call_expr.args->copyWithItems(Expression::copyExpression, Expression::destroyExpression);
                    if(!function_copy || !args_copy)
                    {
                        Expression::destroyExpression(function_copy);
                        expr->call_expr.args->destroyWithItems(Expression::destroyExpression);
                        return NULL;
                    }
                    res = Expression::makeCall(expr->m_alloc, function_copy, args_copy);
                    if(!res)
                    {
                        Expression::destroyExpression(function_copy);
                        expr->call_expr.args->destroyWithItems(Expression::destroyExpression);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_INDEX:
                {
                    Expression* left_copy = Expression::copyExpression(expr->index_expr.left);
                    Expression* index_copy = Expression::copyExpression(expr->index_expr.index);
                    if(!left_copy || !index_copy)
                    {
                        Expression::destroyExpression(left_copy);
                        Expression::destroyExpression(index_copy);
                        return NULL;
                    }
                    res = Expression::makeIndex(expr->m_alloc, left_copy, index_copy);
                    if(!res)
                    {
                        Expression::destroyExpression(left_copy);
                        Expression::destroyExpression(index_copy);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_ASSIGN:
                {
                    Expression* dest_copy = Expression::copyExpression(expr->assign.dest);
                    Expression* source_copy = Expression::copyExpression(expr->assign.source);
                    if(!dest_copy || !source_copy)
                    {
                        Expression::destroyExpression(dest_copy);
                        Expression::destroyExpression(source_copy);
                        return NULL;
                    }
                    res = Expression::makeAssign(expr->m_alloc, dest_copy, source_copy, expr->assign.is_postfix);
                    if(!res)
                    {
                        Expression::destroyExpression(dest_copy);
                        Expression::destroyExpression(source_copy);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_LOGICAL:
                {
                    Expression* left_copy = Expression::copyExpression(expr->logical.left);
                    Expression* right_copy = Expression::copyExpression(expr->logical.right);
                    if(!left_copy || !right_copy)
                    {
                        Expression::destroyExpression(left_copy);
                        Expression::destroyExpression(right_copy);
                        return NULL;
                    }
                    res = Expression::makeLogical(expr->m_alloc, expr->logical.op, left_copy, right_copy);
                    if(!res)
                    {
                        Expression::destroyExpression(left_copy);
                        Expression::destroyExpression(right_copy);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_TERNARY:
                {
                    Expression* test_copy = Expression::copyExpression(expr->ternary.test);
                    Expression* if_true_copy = Expression::copyExpression(expr->ternary.if_true);
                    Expression* if_false_copy = Expression::copyExpression(expr->ternary.if_false);
                    if(!test_copy || !if_true_copy || !if_false_copy)
                    {
                        Expression::destroyExpression(test_copy);
                        Expression::destroyExpression(if_true_copy);
                        Expression::destroyExpression(if_false_copy);
                        return NULL;
                    }
                    res = Expression::makeTernary(expr->m_alloc, test_copy, if_true_copy, if_false_copy);
                    if(!res)
                    {
                        Expression::destroyExpression(test_copy);
                        Expression::destroyExpression(if_true_copy);
                        Expression::destroyExpression(if_false_copy);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_DEFINE:
                {
                    Expression* value_copy = Expression::copyExpression(expr->define.value);
                    if(!value_copy)
                    {
                        return NULL;
                    }
                    res = Expression::makeDefine(expr->m_alloc, Ident::copy_callback(expr->define.name), value_copy, expr->define.assignable);
                    if(!res)
                    {
                        Expression::destroyExpression(value_copy);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_IF:
                {
                    PtrArray* cases_copy = expr->if_statement.cases->copyWithItems(StmtIfClause::Case::copy_callback, StmtIfClause::Case::destroy_callback);
                    StmtBlock* alternative_copy = code_block_copy(expr->if_statement.alternative);
                    if(!cases_copy || !alternative_copy)
                    {
                        cases_copy->destroyWithItems(StmtIfClause::Case::destroy_callback);
                        code_block_destroy(alternative_copy);
                        return NULL;
                    }
                    res = Expression::makeIf(expr->m_alloc, cases_copy, alternative_copy);
                    if(res)
                    {
                        cases_copy->destroyWithItems(StmtIfClause::Case::destroy_callback);
                        code_block_destroy(alternative_copy);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_RETURN_VALUE:
                {
                    Expression* value_copy = Expression::copyExpression(expr->return_value);
                    if(!value_copy)
                    {
                        return NULL;
                    }
                    res = Expression::makeReturn(expr->m_alloc, value_copy);
                    if(!res)
                    {
                        Expression::destroyExpression(value_copy);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_EXPRESSION:
                {
                    Expression* value_copy = Expression::copyExpression(expr->expression);
                    if(!value_copy)
                    {
                        return NULL;
                    }
                    res = Expression::makeExpression(expr->m_alloc, value_copy);
                    if(!res)
                    {
                        Expression::destroyExpression(value_copy);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_WHILE_LOOP:
                {
                    Expression* test_copy = Expression::copyExpression(expr->while_loop.test);
                    StmtBlock* body_copy = code_block_copy(expr->while_loop.body);
                    if(!test_copy || !body_copy)
                    {
                        Expression::destroyExpression(test_copy);
                        code_block_destroy(body_copy);
                        return NULL;
                    }
                    res = Expression::makeWhileLoop(expr->m_alloc, test_copy, body_copy);
                    if(!res)
                    {
                        Expression::destroyExpression(test_copy);
                        code_block_destroy(body_copy);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_BREAK:
                {
                    res = Expression::makeBreak(expr->m_alloc);
                    break;
                }
                case EXPRESSION_CONTINUE:
                {
                    res = Expression::makeContinue(expr->m_alloc);
                    break;
                }
                case EXPRESSION_FOREACH:
                {
                    Expression* source_copy = Expression::copyExpression(expr->foreach.source);
                    StmtBlock* body_copy = code_block_copy(expr->foreach.body);
                    if(!source_copy || !body_copy)
                    {
                        Expression::destroyExpression(source_copy);
                        code_block_destroy(body_copy);
                        return NULL;
                    }
                    res = Expression::makeForeach(expr->m_alloc, Ident::copy_callback(expr->foreach.iterator), source_copy, body_copy);
                    if(!res)
                    {
                        Expression::destroyExpression(source_copy);
                        code_block_destroy(body_copy);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_FOR_LOOP:
                {
                    Expression* init_copy = Expression::copyExpression(expr->for_loop.init);
                    Expression* test_copy = Expression::copyExpression(expr->for_loop.test);
                    Expression* update_copy = Expression::copyExpression(expr->for_loop.update);
                    StmtBlock* body_copy = code_block_copy(expr->for_loop.body);
                    if(!init_copy || !test_copy || !update_copy || !body_copy)
                    {
                        Expression::destroyExpression(init_copy);
                        Expression::destroyExpression(test_copy);
                        Expression::destroyExpression(update_copy);
                        code_block_destroy(body_copy);
                        return NULL;
                    }
                    res = Expression::makeForLoop(expr->m_alloc, init_copy, test_copy, update_copy, body_copy);
                    if(!res)
                    {
                        Expression::destroyExpression(init_copy);
                        Expression::destroyExpression(test_copy);
                        Expression::destroyExpression(update_copy);
                        code_block_destroy(body_copy);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_BLOCK:
                {
                    StmtBlock* block_copy = code_block_copy(expr->block);
                    if(!block_copy)
                    {
                        return NULL;
                    }
                    res = Expression::makeBlock(expr->m_alloc, block_copy);
                    if(!res)
                    {
                        code_block_destroy(block_copy);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_IMPORT:
                {
                    char* path_copy = ape_strdup(expr->m_alloc, expr->import.path);
                    if(!path_copy)
                    {
                        return NULL;
                    }
                    res = Expression::makeImport(expr->m_alloc, path_copy);
                    if(!res)
                    {
                        expr->m_alloc->release(path_copy);
                        return NULL;
                    }
                    break;
                }
                case EXPRESSION_RECOVER:
                {
                    StmtBlock* body_copy = code_block_copy(expr->recover.body);
                    Ident* error_ident_copy = Ident::copy_callback(expr->recover.error_ident);
                    if(!body_copy || !error_ident_copy)
                    {
                        code_block_destroy(body_copy);
                        ident_destroy(error_ident_copy);
                        return NULL;
                    }
                    res = Expression::makeRecover(expr->m_alloc, error_ident_copy, body_copy);
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
            res->pos = expr->pos;
            return res;
        }

        static void destroyExpression(Expression* expr)
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
                    expr->m_alloc->release(expr->string_literal);
                    break;
                }
                case EXPRESSION_NULL_LITERAL:
                {
                    break;
                }
                case EXPRESSION_ARRAY_LITERAL:
                {
                    expr->array->destroyWithItems(Expression::destroyExpression);
                    break;
                }
                case EXPRESSION_MAP_LITERAL:
                {
                    expr->map.m_keylist->destroyWithItems(Expression::destroyExpression);
                    expr->map.m_valueslist->destroyWithItems(Expression::destroyExpression);
                    break;
                }
                case EXPRESSION_PREFIX:
                {
                    Expression::destroyExpression(expr->prefix.right);
                    break;
                }
                case EXPRESSION_INFIX:
                {
                    Expression::destroyExpression(expr->infix.left);
                    Expression::destroyExpression(expr->infix.right);
                    break;
                }
                case EXPRESSION_FUNCTION_LITERAL:
                {
                    StmtFuncDef* fn = &expr->fn_literal;
                    expr->m_alloc->release(fn->name);
                    fn->params->destroyWithItems(ident_destroy);
                    code_block_destroy(fn->body);
                    break;
                }
                case EXPRESSION_CALL:
                {
                    expr->call_expr.args->destroyWithItems(Expression::destroyExpression);
                    Expression::destroyExpression(expr->call_expr.function);
                    break;
                }
                case EXPRESSION_INDEX:
                {
                    Expression::destroyExpression(expr->index_expr.left);
                    Expression::destroyExpression(expr->index_expr.index);
                    break;
                }
                case EXPRESSION_ASSIGN:
                {
                    Expression::destroyExpression(expr->assign.dest);
                    Expression::destroyExpression(expr->assign.source);
                    break;
                }
                case EXPRESSION_LOGICAL:
                {
                    Expression::destroyExpression(expr->logical.left);
                    Expression::destroyExpression(expr->logical.right);
                    break;
                }
                case EXPRESSION_TERNARY:
                {
                    Expression::destroyExpression(expr->ternary.test);
                    Expression::destroyExpression(expr->ternary.if_true);
                    Expression::destroyExpression(expr->ternary.if_false);
                    break;
                }
                case EXPRESSION_DEFINE:
                {
                    ident_destroy(expr->define.name);
                    Expression::destroyExpression(expr->define.value);
                    break;
                }
                case EXPRESSION_IF:
                {
                    expr->if_statement.cases->destroyWithItems(StmtIfClause::Case::destroy_callback);
                    code_block_destroy(expr->if_statement.alternative);
                    break;
                }
                case EXPRESSION_RETURN_VALUE:
                {
                    Expression::destroyExpression(expr->return_value);
                    break;
                }
                case EXPRESSION_EXPRESSION:
                {
                    Expression::destroyExpression(expr->expression);
                    break;
                }
                case EXPRESSION_WHILE_LOOP:
                {
                    Expression::destroyExpression(expr->while_loop.test);
                    code_block_destroy(expr->while_loop.body);
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
                    ident_destroy(expr->foreach.iterator);
                    Expression::destroyExpression(expr->foreach.source);
                    code_block_destroy(expr->foreach.body);
                    break;
                }
                case EXPRESSION_FOR_LOOP:
                {
                    Expression::destroyExpression(expr->for_loop.init);
                    Expression::destroyExpression(expr->for_loop.test);
                    Expression::destroyExpression(expr->for_loop.update);
                    code_block_destroy(expr->for_loop.body);
                    break;
                }
                case EXPRESSION_BLOCK:
                {
                    code_block_destroy(expr->block);
                    break;
                }
                case EXPRESSION_IMPORT:
                {
                    expr->m_alloc->release(expr->import.path);
                    break;
                }
                case EXPRESSION_RECOVER:
                {
                    code_block_destroy(expr->recover.body);
                    ident_destroy(expr->recover.error_ident);
                    break;
                }
            }
            expr->m_alloc->release(expr);
        }


    public:
        Expr_type type;
        union
        {
            Ident* ident;
            double number_literal;
            bool bool_literal;
            char* string_literal;
            PtrArray * array;
            MapLiteral map;
            StmtPrefix prefix;
            StmtInfix infix;
            StmtFuncDef fn_literal;
            StmtCall call_expr;
            StmtIndex index_expr;
            StmtAssign assign;
            StmtLogical logical;
            StmtTernary ternary;
            StmtDefine define;
            StmtIfClause if_statement;
            Expression* return_value;
            Expression* expression;
            StmtWhileLoop while_loop;
            StmtForeach foreach;
            StmtForLoop for_loop;
            StmtBlock* block;
            StmtImport import;
            StmtRecover recover;
        };
        Position pos;
};

struct Parser: public Allocator::Allocated
{
    public:
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
            output = (char*)alloc->allocate(len + 1);
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
            alloc->release(output);
            return NULL;
        }


        static Expression* wrap_expression_in_function_call(Allocator* alloc, Expression* expr, const char* function_name)
        {
            Token fn_token;
            fn_token.init(TOKEN_IDENT, function_name, (int)strlen(function_name));
            fn_token.pos = expr->pos;

            Ident* ident = Ident::make(alloc, fn_token);
            if(!ident)
            {
                return NULL;
            }
            ident->pos = fn_token.pos;

            Expression* function_ident_expr = Expression::makeIdent(alloc, ident);
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
                Expression::destroyExpression(function_ident_expr);
                return NULL;
            }

            bool ok = args->add(expr);
            if(!ok)
            {
                args->destroy();
                Expression::destroyExpression(function_ident_expr);
                return NULL;
            }

            Expression* call_expr = Expression::makeCall(alloc, function_ident_expr, args);
            if(!call_expr)
            {
                args->destroy();
                Expression::destroyExpression(function_ident_expr);
                return NULL;
            }
            call_expr->pos = expr->pos;

            return call_expr;
        }


        static Parser* make(Allocator* alloc, const Config* config, ErrorList* errors)
        {
            Parser* parser;
            parser = alloc->allocate<Parser>();
            if(!parser)
            {
                return NULL;
            }
            memset(parser, 0, sizeof(Parser));

            parser->m_alloc = alloc;
            parser->config = config;
            parser->m_errors = errors;

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

    public:
        const Config* config;
        Lexer lexer;
        ErrorList* m_errors;
        RightAssocParseFNCallback right_assoc_parse_fns[TOKEN_TYPE_MAX];
        LeftAssocParseFNCallback left_assoc_parse_fns[TOKEN_TYPE_MAX];
        int depth;

    public:
        void destroy()
        {
            if(!this)
            {
                return;
            }
            m_alloc->release(this);
        }

        PtrArray* parseAll(const char* input, CompiledFile* file)
        {
            bool ok;
            PtrArray* statements;
            Expression* stmt;
            this->depth = 0;
            ok = this->lexer.init(m_alloc, m_errors, input, file);
            if(!ok)
            {
                return NULL;
            }
            this->lexer.nextToken();
            this->lexer.nextToken();
            statements = PtrArray::make(m_alloc);
            if(!statements)
            {
                return NULL;
            }
            while(!this->lexer.currentTokenIs( TOKEN_EOF))
            {
                if(this->lexer.currentTokenIs(TOKEN_SEMICOLON))
                {
                    this->lexer.nextToken();
                    continue;
                }
                stmt = parse_statement(this);
                if(!stmt)
                {
                    goto err;
                }
                ok = statements->add(stmt);
                if(!ok)
                {
                    Expression::destroyExpression(stmt);
                    goto err;
                }
            }
            if(m_errors->count() > 0)
            {
                goto err;
            }

            return statements;
        err:
            statements->destroyWithItems(Expression::destroyExpression);
            return NULL;
        }

        // INTERNAL
        static Expression* parse_statement(Parser* p)
        {
            Position pos = p->lexer.m_currtoken.pos;

            Expression* res = NULL;
            switch(p->lexer.m_currtoken.type)
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
                    if(p->lexer.peekTokenIs(TOKEN_IDENT))
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

        static Expression* parse_define_statement(Parser* p)
        {
            bool assignable;
            Ident* name_ident;
            Expression* value;
            Expression* res;
            name_ident = NULL;
            value = NULL;
            assignable = p->lexer.currentTokenIs(TOKEN_VAR);
            p->lexer.nextToken();
            if(!p->lexer.expectCurrent(TOKEN_IDENT))
            {
                goto err;
            }
            name_ident = Ident::make(p->m_alloc, p->lexer.m_currtoken);
            if(!name_ident)
            {
                goto err;
            }
            p->lexer.nextToken();
            if(!p->lexer.expectCurrent(TOKEN_ASSIGN))
            {
                goto err;
            }
            p->lexer.nextToken();
            value = parse_expression(p, PRECEDENCE_LOWEST);
            if(!value)
            {
                goto err;
            }
            if(value->type == EXPRESSION_FUNCTION_LITERAL)
            {
                value->fn_literal.name = ape_strdup(p->m_alloc, name_ident->value);
                if(!value->fn_literal.name)
                {
                    goto err;
                }
            }
            res = Expression::makeDefine(p->m_alloc, name_ident, value, assignable);
            if(!res)
            {
                goto err;
            }
            return res;
        err:
            Expression::destroyExpression(value);
            ident_destroy(name_ident);
            return NULL;
        }

        static Expression* parse_if_statement(Parser* p)
        {
            bool ok;
            PtrArray* cases;
            StmtBlock* alternative;
            StmtIfClause::Case* elif;
            Expression* res;
            StmtIfClause::Case* cond;
            cases = NULL;
            alternative = NULL;
            cases = PtrArray::make(p->m_alloc);
            if(!cases)
            {
                goto err;
            }
            p->lexer.nextToken();
            if(!p->lexer.expectCurrent(TOKEN_LPAREN))
            {
                goto err;
            }
            p->lexer.nextToken();
            cond = StmtIfClause::Case::make(p->m_alloc, NULL, NULL);
            if(!cond)
            {
                goto err;
            }
            ok = cases->add(cond);
            if(!ok)
            {
                cond->destroy();
                goto err;
            }
            cond->test = parse_expression(p, PRECEDENCE_LOWEST);
            if(!cond->test)
            {
                goto err;
            }
            if(!p->lexer.expectCurrent(TOKEN_RPAREN))
            {
                goto err;
            }
            p->lexer.nextToken();
            cond->consequence = parse_code_block(p);
            if(!cond->consequence)
            {
                goto err;
            }
            while(p->lexer.currentTokenIs(TOKEN_ELSE))
            {
                p->lexer.nextToken();
                if(p->lexer.currentTokenIs(TOKEN_IF))
                {
                    p->lexer.nextToken();
                    if(!p->lexer.expectCurrent(TOKEN_LPAREN))
                    {
                        goto err;
                    }
                    p->lexer.nextToken();
                    elif = StmtIfClause::Case::make(p->m_alloc, NULL, NULL);
                    if(!elif)
                    {
                        goto err;
                    }
                    ok = cases->add(elif);
                    if(!ok)
                    {
                        elif->destroy();
                        goto err;
                    }
                    elif->test = parse_expression(p, PRECEDENCE_LOWEST);
                    if(!elif->test)
                    {
                        goto err;
                    }
                    if(!p->lexer.expectCurrent(TOKEN_RPAREN))
                    {
                        goto err;
                    }
                    p->lexer.nextToken();
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
            res = Expression::makeIf(p->m_alloc, cases, alternative);
            if(!res)
            {
                goto err;
            }
            return res;
        err:
            cases->destroyWithItems(StmtIfClause::Case::destroy_callback);
            code_block_destroy(alternative);
            return NULL;
        }

        static Expression* parse_return_statement(Parser* p)
        {
            Expression* expr;
            Expression* res;
            expr = NULL;
            p->lexer.nextToken();
            if(!p->lexer.currentTokenIs(TOKEN_SEMICOLON) && !p->lexer.currentTokenIs(TOKEN_RBRACE) && !p->lexer.currentTokenIs(TOKEN_EOF))
            {
                expr = parse_expression(p, PRECEDENCE_LOWEST);
                if(!expr)
                {
                    return NULL;
                }
            }
            res = Expression::makeReturn(p->m_alloc, expr);
            if(!res)
            {
                Expression::destroyExpression(expr);
                return NULL;
            }
            return res;
        }

        static Expression* parse_expression_statement(Parser* p)
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
                    p->m_errors->addFormat(APE_ERROR_PARSING, expr->pos, "Only assignments and function calls can be expression statements");
                    Expression::destroyExpression(expr);
                    return NULL;
                }
            }
            res = Expression::makeExpression(p->m_alloc, expr);
            if(!res)
            {
                Expression::destroyExpression(expr);
                return NULL;
            }
            return res;
        }

        static Expression* parse_while_loop_statement(Parser* p)
        {
            Expression* test;
            StmtBlock* body;
            Expression* res;
            test = NULL;
            body = NULL;
            p->lexer.nextToken();
            if(!p->lexer.expectCurrent(TOKEN_LPAREN))
            {
                goto err;
            }
            p->lexer.nextToken();
            test = parse_expression(p, PRECEDENCE_LOWEST);
            if(!test)
            {
                goto err;
            }
            if(!p->lexer.expectCurrent(TOKEN_RPAREN))
            {
                goto err;
            }
            p->lexer.nextToken();
            body = parse_code_block(p);
            if(!body)
            {
                goto err;
            }
            res = Expression::makeWhileLoop(p->m_alloc, test, body);
            if(!res)
            {
                goto err;
            }
            return res;
        err:
            code_block_destroy(body);
            Expression::destroyExpression(test);
            return NULL;
        }

        static Expression* parse_break_statement(Parser* p)
        {
            p->lexer.nextToken();
            return Expression::makeBreak(p->m_alloc);
        }

        static Expression* parse_continue_statement(Parser* p)
        {
            p->lexer.nextToken();
            return Expression::makeContinue(p->m_alloc);
        }

        static Expression* parse_block_statement(Parser* p)
        {
            StmtBlock* block;
            Expression* res;
            block = parse_code_block(p);
            if(!block)
            {
                return NULL;
            }
            res = Expression::makeBlock(p->m_alloc, block);
            if(!res)
            {
                code_block_destroy(block);
                return NULL;
            }
            return res;
        }

        static Expression* parse_import_statement(Parser* p)
        {
            char* processed_name;
            Expression* res;
            p->lexer.nextToken();
            if(!p->lexer.expectCurrent(TOKEN_STRING))
            {
                return NULL;
            }
            processed_name = process_and_copy_string(p->m_alloc, p->lexer.m_currtoken.literal, p->lexer.m_currtoken.len);
            if(!processed_name)
            {
                p->m_errors->addError(APE_ERROR_PARSING, p->lexer.m_currtoken.pos, "Error when parsing module name");
                return NULL;
            }
            p->lexer.nextToken();
            res= Expression::makeImport(p->m_alloc, processed_name);
            if(!res)
            {
                p->m_alloc->release(processed_name);
                return NULL;
            }
            return res;
        }

        static Expression* parse_recover_statement(Parser* p)
        {
            Ident* error_ident;
            StmtBlock* body;
            Expression* res;
            error_ident = NULL;
            body = NULL;
            p->lexer.nextToken();
            if(!p->lexer.expectCurrent(TOKEN_LPAREN))
            {
                return NULL;
            }
            p->lexer.nextToken();
            if(!p->lexer.expectCurrent(TOKEN_IDENT))
            {
                return NULL;
            }
            error_ident = Ident::make(p->m_alloc, p->lexer.m_currtoken);
            if(!error_ident)
            {
                return NULL;
            }
            p->lexer.nextToken();
            if(!p->lexer.expectCurrent(TOKEN_RPAREN))
            {
                goto err;
            }
            p->lexer.nextToken();
            body = parse_code_block(p);
            if(!body)
            {
                goto err;
            }
            res = Expression::makeRecover(p->m_alloc, error_ident, body);
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

        static Expression* parse_for_loop_statement(Parser* p)
        {
            p->lexer.nextToken();
            if(!p->lexer.expectCurrent(TOKEN_LPAREN))
            {
                return NULL;
            }
            p->lexer.nextToken();
            if(p->lexer.currentTokenIs( TOKEN_IDENT) && p->lexer.peekTokenIs(TOKEN_IN))
            {
                return parse_foreach(p);
            }
            return parse_classic_for_loop(p);
        }

        static Expression* parse_foreach(Parser* p)
        {
            Expression* source;
            StmtBlock* body;
            Ident* iterator_ident;
            Expression* res;
            source = NULL;
            body = NULL;
            iterator_ident = Ident::make(p->m_alloc, p->lexer.m_currtoken);
            if(!iterator_ident)
            {
                goto err;
            }
            p->lexer.nextToken();
            if(!p->lexer.expectCurrent(TOKEN_IN))
            {
                goto err;
            }
            p->lexer.nextToken();
            source = parse_expression(p, PRECEDENCE_LOWEST);
            if(!source)
            {
                goto err;
            }
            if(!p->lexer.expectCurrent(TOKEN_RPAREN))
            {
                goto err;
            }
            p->lexer.nextToken();
            body = parse_code_block(p);
            if(!body)
            {
                goto err;
            }
            res = Expression::makeForeach(p->m_alloc, iterator_ident, source, body);
            if(!res)
            {
                goto err;
            }
            return res;
        err:
            code_block_destroy(body);
            ident_destroy(iterator_ident);
            Expression::destroyExpression(source);
            return NULL;
        }

        static Expression* parse_classic_for_loop(Parser* p)
        {
            Expression* init;
            Expression* test;
            Expression* update;
            StmtBlock* body;
            Expression* res;

            init = NULL;
            test = NULL;
            update = NULL;
            body = NULL;
            if(!p->lexer.currentTokenIs(TOKEN_SEMICOLON))
            {
                init = parse_statement(p);
                if(!init)
                {
                    goto err;
                }
                if(init->type != EXPRESSION_DEFINE && init->type != EXPRESSION_EXPRESSION)
                {
                    p->m_errors->addFormat(APE_ERROR_PARSING, init->pos, "for loop's init clause should be a define statement or an expression");
                    goto err;
                }
                if(!p->lexer.expectCurrent(TOKEN_SEMICOLON))
                {
                    goto err;
                }
            }
            p->lexer.nextToken();
            if(!p->lexer.currentTokenIs(TOKEN_SEMICOLON))
            {
                test = parse_expression(p, PRECEDENCE_LOWEST);
                if(!test)
                {
                    goto err;
                }
                if(!p->lexer.expectCurrent(TOKEN_SEMICOLON))
                {
                    goto err;
                }
            }
            p->lexer.nextToken();
            if(!p->lexer.currentTokenIs(TOKEN_RPAREN))
            {
                update = parse_expression(p, PRECEDENCE_LOWEST);
                if(!update)
                {
                    goto err;
                }
                if(!p->lexer.expectCurrent(TOKEN_RPAREN))
                {
                    goto err;
                }
            }
            p->lexer.nextToken();
            body = parse_code_block(p);
            if(!body)
            {
                goto err;
            }
            res = Expression::makeForLoop(p->m_alloc, init, test, update, body);
            if(!res)
            {
                goto err;
            }

            return res;
        err:
            Expression::destroyExpression(init);
            Expression::destroyExpression(test);
            Expression::destroyExpression(update);
            code_block_destroy(body);
            return NULL;
        }

        static Expression* parse_function_statement(Parser* p)
        {
            Ident* name_ident;
            Expression* value;
            Position pos;
            Expression* res;
            value = NULL;
            name_ident = NULL;
            pos = p->lexer.m_currtoken.pos;
            p->lexer.nextToken();
            if(!p->lexer.expectCurrent(TOKEN_IDENT))
            {
                goto err;
            }
            name_ident = Ident::make(p->m_alloc, p->lexer.m_currtoken);
            if(!name_ident)
            {
                goto err;
            }
            p->lexer.nextToken();
            value = parse_function_literal(p);
            if(!value)
            {
                goto err;
            }
            value->pos = pos;
            value->fn_literal.name = ape_strdup(p->m_alloc, name_ident->value);
            if(!value->fn_literal.name)
            {
                goto err;
            }
            res = Expression::makeDefine(p->m_alloc, name_ident, value, false);
            if(!res)
            {
                goto err;
            }
            return res;

        err:
            Expression::destroyExpression(value);
            ident_destroy(name_ident);
            return NULL;
        }

        static StmtBlock* parse_code_block(Parser* p)
        {
            bool ok;
            PtrArray* statements;
            Expression* stmt;
            StmtBlock* res;
            if(!p->lexer.expectCurrent(TOKEN_LBRACE))
            {
                return NULL;
            }
            p->lexer.nextToken();
            p->depth++;
            statements = PtrArray::make(p->m_alloc);
            if(!statements)
            {
                goto err;
            }
            while(!p->lexer.currentTokenIs( TOKEN_RBRACE))
            {
                if(p->lexer.currentTokenIs(TOKEN_EOF))
                {
                    p->m_errors->addError(APE_ERROR_PARSING, p->lexer.m_currtoken.pos, "Unexpected EOF");
                    goto err;
                }
                if(p->lexer.currentTokenIs(TOKEN_SEMICOLON))
                {
                    p->lexer.nextToken();
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
                    Expression::destroyExpression(stmt);
                    goto err;
                }
            }
            p->lexer.nextToken();
            p->depth--;
            res = code_block_make(p->m_alloc, statements);
            if(!res)
            {
                goto err;
            }
            return res;
        err:
            p->depth--;
            statements->destroyWithItems(Expression::destroyExpression);
            return NULL;
        }

        static Expression* parse_expression(Parser* p, Precedence prec)
        {
            char* literal;
            Position pos;
            RightAssocParseFNCallback parse_right_assoc;
            LeftAssocParseFNCallback parse_left_assoc;
            Expression* left_expr;
            Expression* new_left_expr;
            pos = p->lexer.m_currtoken.pos;
            if(p->lexer.m_currtoken.type == TOKEN_INVALID)
            {
                p->m_errors->addError(APE_ERROR_PARSING, p->lexer.m_currtoken.pos, "Illegal token");
                return NULL;
            }
            parse_right_assoc = p->right_assoc_parse_fns[p->lexer.m_currtoken.type];
            if(!parse_right_assoc)
            {
                literal = p->lexer.m_currtoken.copyLiteral(p->m_alloc);
                p->m_errors->addFormat(APE_ERROR_PARSING, p->lexer.m_currtoken.pos, "No prefix parse function for \"%s\" found", literal);
                p->m_alloc->release(literal);
                return NULL;
            }
            left_expr = parse_right_assoc(p);
            if(!left_expr)
            {
                return NULL;
            }
            left_expr->pos = pos;
            while(!p->lexer.currentTokenIs(TOKEN_SEMICOLON) && prec < get_precedence(p->lexer.m_currtoken.type))
            {
                parse_left_assoc = p->left_assoc_parse_fns[p->lexer.m_currtoken.type];
                if(!parse_left_assoc)
                {
                    return left_expr;
                }
                pos = p->lexer.m_currtoken.pos;
                new_left_expr= parse_left_assoc(p, left_expr);
                if(!new_left_expr)
                {
                    Expression::destroyExpression(left_expr);
                    return NULL;
                }
                new_left_expr->pos = pos;
                left_expr = new_left_expr;
            }
            return left_expr;
        }

        static Expression* parse_identifier(Parser* p)
        {
            Ident* ident;
            Expression* res;
            ident = Ident::make(p->m_alloc, p->lexer.m_currtoken);
            if(!ident)
            {
                return NULL;
            }
            res = Expression::makeIdent(p->m_alloc, ident);
            if(!res)
            {
                ident_destroy(ident);
                return NULL;
            }
            p->lexer.nextToken();
            return res;
        }

        static Expression* parse_number_literal(Parser* p)
        {
            char* end;
            char* literal;
            double number;
            long parsed_len;
            number = 0;
            errno = 0;
            number = strtod(p->lexer.m_currtoken.literal, &end);
            parsed_len = end - p->lexer.m_currtoken.literal;
            if(errno || parsed_len != p->lexer.m_currtoken.len)
            {
                literal = p->lexer.m_currtoken.copyLiteral(p->m_alloc);
                p->m_errors->addFormat(APE_ERROR_PARSING, p->lexer.m_currtoken.pos, "Parsing number literal \"%s\" failed", literal);
                p->m_alloc->release(literal);
                return NULL;
            }
            p->lexer.nextToken();
            return Expression::makeNumberLiteral(p->m_alloc, number);
        }

        static Expression* parse_bool_literal(Parser* p)
        {
            Expression* res;
            res = Expression::makeBoolLiteral(p->m_alloc, p->lexer.m_currtoken.type == TOKEN_TRUE);
            p->lexer.nextToken();
            return res;
        }

        static Expression* parse_string_literal(Parser* p)
        {
            char* processed_literal;
            processed_literal = process_and_copy_string(p->m_alloc, p->lexer.m_currtoken.literal, p->lexer.m_currtoken.len);
            if(!processed_literal)
            {
                p->m_errors->addError(APE_ERROR_PARSING, p->lexer.m_currtoken.pos, "Error when parsing string literal");
                return NULL;
            }
            p->lexer.nextToken();
            Expression* res = Expression::makeStringLiteral(p->m_alloc, processed_literal);
            if(!res)
            {
                p->m_alloc->release(processed_literal);
                return NULL;
            }
            return res;
        }

        static Expression* parse_template_string_literal(Parser* p)
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
            processed_literal = process_and_copy_string(p->m_alloc, p->lexer.m_currtoken.literal, p->lexer.m_currtoken.len);
            if(!processed_literal)
            {
                p->m_errors->addError(APE_ERROR_PARSING, p->lexer.m_currtoken.pos, "Error when parsing string literal");
                return NULL;
            }
            p->lexer.nextToken();

            if(!p->lexer.expectCurrent(TOKEN_LBRACE))
            {
                goto err;
            }
            p->lexer.nextToken();

            pos = p->lexer.m_currtoken.pos;

            left_string_expr = Expression::makeStringLiteral(p->m_alloc, processed_literal);
            if(!left_string_expr)
            {
                goto err;
            }
            left_string_expr->pos = pos;
            processed_literal = NULL;
            pos = p->lexer.m_currtoken.pos;
            template_expr = parse_expression(p, PRECEDENCE_LOWEST);
            if(!template_expr)
            {
                goto err;
            }
            to_str_call_expr = wrap_expression_in_function_call(p->m_alloc, template_expr, "to_str");
            if(!to_str_call_expr)
            {
                goto err;
            }
            to_str_call_expr->pos = pos;
            template_expr = NULL;
            left_add_expr = Expression::makeInfix(p->m_alloc, OPERATOR_PLUS, left_string_expr, to_str_call_expr);
            if(!left_add_expr)
            {
                goto err;
            }
            left_add_expr->pos = pos;
            left_string_expr = NULL;
            to_str_call_expr = NULL;
            if(!p->lexer.expectCurrent(TOKEN_RBRACE))
            {
                goto err;
            }
            p->lexer.previousToken();
            p->lexer.continueTemplateString();
            p->lexer.nextToken();
            p->lexer.nextToken();
            pos = p->lexer.m_currtoken.pos;
            right_expr = parse_expression(p, PRECEDENCE_HIGHEST);
            if(!right_expr)
            {
                goto err;
            }
            right_add_expr = Expression::makeInfix(p->m_alloc, OPERATOR_PLUS, left_add_expr, right_expr);
            if(!right_add_expr)
            {
                goto err;
            }
            right_add_expr->pos = pos;
            left_add_expr = NULL;
            right_expr = NULL;

            return right_add_expr;
        err:
            Expression::destroyExpression(right_add_expr);
            Expression::destroyExpression(right_expr);
            Expression::destroyExpression(left_add_expr);
            Expression::destroyExpression(to_str_call_expr);
            Expression::destroyExpression(template_expr);
            Expression::destroyExpression(left_string_expr);
            p->m_alloc->release(processed_literal);
            return NULL;
        }

        static Expression* parse_null_literal(Parser* p)
        {
            p->lexer.nextToken();
            return Expression::makeNullLiteral(p->m_alloc);
        }

        static Expression* parse_array_literal(Parser* p)
        {
            PtrArray* array;
            Expression* res;
            array = parse_expression_list(p, TOKEN_LBRACKET, TOKEN_RBRACKET, true);
            if(!array)
            {
                return NULL;
            }
            res = Expression::makeArrayLiteral(p->m_alloc, array);
            if(!res)
            {
                array->destroyWithItems(Expression::destroyExpression);
                return NULL;
            }
            return res;
        }

        static Expression* parse_map_literal(Parser* p)
        {
            bool ok;
            char* str;
            PtrArray* keys;
            PtrArray* values;
            Expression* key;
            Expression* value;
            Expression* res;
            keys = PtrArray::make(p->m_alloc);
            values = PtrArray::make(p->m_alloc);
            if(!keys || !values)
            {
                goto err;
            }
            p->lexer.nextToken();
            while(!p->lexer.currentTokenIs(TOKEN_RBRACE))
            {
                key = NULL;
                if(p->lexer.currentTokenIs(TOKEN_IDENT))
                {
                    str = p->lexer.m_currtoken.copyLiteral(p->m_alloc);
                    key = Expression::makeStringLiteral(p->m_alloc, str);
                    if(!key)
                    {
                        p->m_alloc->release(str);
                        goto err;
                    }
                    key->pos = p->lexer.m_currtoken.pos;
                    p->lexer.nextToken();
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
                            p->m_errors->addFormat(APE_ERROR_PARSING, key->pos, "Invalid map literal key type");
                            Expression::destroyExpression(key);
                            goto err;
                        }
                    }
                }

                ok = keys->add(key);
                if(!ok)
                {
                    Expression::destroyExpression(key);
                    goto err;
                }

                if(!p->lexer.expectCurrent(TOKEN_COLON))
                {
                    goto err;
                }

                p->lexer.nextToken();

                value = parse_expression(p, PRECEDENCE_LOWEST);
                if(!value)
                {
                    goto err;
                }
                ok = values->add(value);
                if(!ok)
                {
                    Expression::destroyExpression(value);
                    goto err;
                }

                if(p->lexer.currentTokenIs(TOKEN_RBRACE))
                {
                    break;
                }

                if(!p->lexer.expectCurrent(TOKEN_COMMA))
                {
                    goto err;
                }

                p->lexer.nextToken();
            }

            p->lexer.nextToken();

            res = Expression::makeMapLiteral(p->m_alloc, keys, values);
            if(!res)
            {
                goto err;
            }
            return res;
        err:
            keys->destroyWithItems(Expression::destroyExpression);
            values->destroyWithItems(Expression::destroyExpression);
            return NULL;
        }

        static Expression* parse_prefix_expression(Parser* p)
        {
            Operator op;
            Expression* right;
            Expression* res;
            op = token_to_operator(p->lexer.m_currtoken.type);
            p->lexer.nextToken();
            right = parse_expression(p, PRECEDENCE_PREFIX);
            if(!right)
            {
                return NULL;
            }
            res = Expression::makePrefix(p->m_alloc, op, right);
            if(!res)
            {
                Expression::destroyExpression(right);
                return NULL;
            }
            return res;
        }

        static Expression* parse_infix_expression(Parser* p, Expression* left)
        {
            Operator op;
            Precedence prec;
            Expression* right;
            Expression* res;
            op = token_to_operator(p->lexer.m_currtoken.type);
            prec = get_precedence(p->lexer.m_currtoken.type);
            p->lexer.nextToken();
            right = parse_expression(p, prec);
            if(!right)
            {
                return NULL;
            }
            res = Expression::makeInfix(p->m_alloc, op, left, right);
            if(!res)
            {
                Expression::destroyExpression(right);
                return NULL;
            }
            return res;
        }

        static Expression* parse_grouped_expression(Parser* p)
        {
            Expression* expr;
            p->lexer.nextToken();
            expr = parse_expression(p, PRECEDENCE_LOWEST);
            if(!expr || !p->lexer.expectCurrent(TOKEN_RPAREN))
            {
                Expression::destroyExpression(expr);
                return NULL;
            }
            p->lexer.nextToken();
            return expr;
        }

        static Expression* parse_function_literal(Parser* p)
        {
            bool ok;
            PtrArray* params;
            StmtBlock* body;
            Expression* res;
            p->depth++;
            params = NULL;
            body = NULL;
            if(p->lexer.currentTokenIs(TOKEN_FUNCTION))
            {
                p->lexer.nextToken();
            }
            params = PtrArray::make(p->m_alloc);
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
            res = Expression::makeFunctionLiteral(p->m_alloc, params, body);
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

        static bool parse_function_parameters(Parser* p, PtrArray * out_params)
        {
            bool ok;
            Ident* ident;
            if(!p->lexer.expectCurrent(TOKEN_LPAREN))
            {
                return false;
            }
            p->lexer.nextToken();
            if(p->lexer.currentTokenIs(TOKEN_RPAREN))
            {
                p->lexer.nextToken();
                return true;
            }
            if(!p->lexer.expectCurrent(TOKEN_IDENT))
            {
                return false;
            }
            ident = Ident::make(p->m_alloc, p->lexer.m_currtoken);
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
            p->lexer.nextToken();
            while(p->lexer.currentTokenIs(TOKEN_COMMA))
            {
                p->lexer.nextToken();
                if(!p->lexer.expectCurrent(TOKEN_IDENT))
                {
                    return false;
                }
                ident = Ident::make(p->m_alloc, p->lexer.m_currtoken);
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
                p->lexer.nextToken();
            }
            if(!p->lexer.expectCurrent(TOKEN_RPAREN))
            {
                return false;
            }

            p->lexer.nextToken();
            return true;
        }

        static Expression* parse_call_expression(Parser* p, Expression* left)
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
            res = Expression::makeCall(p->m_alloc, function, args);
            if(!res)
            {
                args->destroyWithItems(Expression::destroyExpression);
                return NULL;
            }
            return res;
        }

        static PtrArray* parse_expression_list(Parser* p, TokenType start_token, TokenType end_token, bool trailing_comma_allowed)
        {
            bool ok;
            PtrArray* res;
            Expression* arg_expr;
            if(!p->lexer.expectCurrent(start_token))
            {
                return NULL;
            }
            p->lexer.nextToken();
            res = PtrArray::make(p->m_alloc);
            if(p->lexer.currentTokenIs(end_token))
            {
                p->lexer.nextToken();
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
                Expression::destroyExpression(arg_expr);
                goto err;
            }
            while(p->lexer.currentTokenIs(TOKEN_COMMA))
            {
                p->lexer.nextToken();
                if(trailing_comma_allowed && p->lexer.currentTokenIs(end_token))
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
                    Expression::destroyExpression(arg_expr);
                    goto err;
                }
            }
            if(!p->lexer.expectCurrent(end_token))
            {
                goto err;
            }
            p->lexer.nextToken();
            return res;
        err:
            res->destroyWithItems(Expression::destroyExpression);
            return NULL;
        }

        static Expression* parse_index_expression(Parser* p, Expression* left)
        {
            Expression* index;
            Expression* res;
            p->lexer.nextToken();
            index = parse_expression(p, PRECEDENCE_LOWEST);
            if(!index)
            {
                return NULL;
            }
            if(!p->lexer.expectCurrent(TOKEN_RBRACKET))
            {
                Expression::destroyExpression(index);
                return NULL;
            }
            p->lexer.nextToken();
            res = Expression::makeIndex(p->m_alloc, left, index);
            if(!res)
            {
                Expression::destroyExpression(index);
                return NULL;
            }
            return res;
        }

        static Expression* parse_assign_expression(Parser* p, Expression* left)
        {
            TokenType assign_type;
            Position pos;
            Operator op;
            Expression* source;
            Expression* left_copy;
            Expression* new_source;
            Expression* res;

            source = NULL;
            assign_type = p->lexer.m_currtoken.type;
            p->lexer.nextToken();
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
                        left_copy = Expression::copyExpression(left);
                        if(!left_copy)
                        {
                            goto err;
                        }
                        pos = source->pos;
                        new_source = Expression::makeInfix(p->m_alloc, op, left_copy, source);
                        if(!new_source)
                        {
                            Expression::destroyExpression(left_copy);
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
            res = Expression::makeAssign(p->m_alloc, left, source, false);
            if(!res)
            {
                goto err;
            }
            return res;
        err:
            Expression::destroyExpression(source);
            return NULL;
        }

        static Expression* parse_logical_expression(Parser* p, Expression* left)
        {
            Operator op;
            Precedence prec;
            Expression* right;
            Expression* res;
            op = token_to_operator(p->lexer.m_currtoken.type);
            prec = get_precedence(p->lexer.m_currtoken.type);
            p->lexer.nextToken();
            right = parse_expression(p, prec);
            if(!right)
            {
                return NULL;
            }
            res = Expression::makeLogical(p->m_alloc, op, left, right);
            if(!res)
            {
                Expression::destroyExpression(right);
                return NULL;
            }
            return res;
        }

        static Expression* parse_ternary_expression(Parser* p, Expression* left)
        {
            Expression* if_true;
            Expression* if_false;
            Expression* res;
            p->lexer.nextToken();
            if_true = parse_expression(p, PRECEDENCE_LOWEST);
            if(!if_true)
            {
                return NULL;
            }
            if(!p->lexer.expectCurrent(TOKEN_COLON))
            {
                Expression::destroyExpression(if_true);
                return NULL;
            }
            p->lexer.nextToken();
            if_false = parse_expression(p, PRECEDENCE_LOWEST);
            if(!if_false)
            {
                Expression::destroyExpression(if_true);
                return NULL;
            }
            res = Expression::makeTernary(p->m_alloc, left, if_true, if_false);
            if(!res)
            {
                Expression::destroyExpression(if_true);
                Expression::destroyExpression(if_false);
                return NULL;
            }
            return res;
        }

        static Expression* parse_incdec_prefix_expression(Parser* p)
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
            operation_type = p->lexer.m_currtoken.type;
            pos = p->lexer.m_currtoken.pos;
            p->lexer.nextToken();
            op = token_to_operator(operation_type);
            dest = parse_expression(p, PRECEDENCE_PREFIX);
            if(!dest)
            {
                goto err;
            }
            one_literal = Expression::makeNumberLiteral(p->m_alloc, 1);
            if(!one_literal)
            {
                Expression::destroyExpression(dest);
                goto err;
            }
            one_literal->pos = pos;
            dest_copy = Expression::copyExpression(dest);
            if(!dest_copy)
            {
                Expression::destroyExpression(one_literal);
                Expression::destroyExpression(dest);
                goto err;
            }
            operation = Expression::makeInfix(p->m_alloc, op, dest_copy, one_literal);
            if(!operation)
            {
                Expression::destroyExpression(dest_copy);
                Expression::destroyExpression(dest);
                Expression::destroyExpression(one_literal);
                goto err;
            }
            operation->pos = pos;

            res = Expression::makeAssign(p->m_alloc, dest, operation, false);
            if(!res)
            {
                Expression::destroyExpression(dest);
                Expression::destroyExpression(operation);
                goto err;
            }
            return res;
        err:
            Expression::destroyExpression(source);
            return NULL;
        }

        static Expression* parse_incdec_postfix_expression(Parser* p, Expression* left)
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
            operation_type = p->lexer.m_currtoken.type;
            pos = p->lexer.m_currtoken.pos;
            p->lexer.nextToken();
            op = token_to_operator(operation_type);
            left_copy = Expression::copyExpression(left);
            if(!left_copy)
            {
                goto err;
            }
            one_literal = Expression::makeNumberLiteral(p->m_alloc, 1);
            if(!one_literal)
            {
                Expression::destroyExpression(left_copy);
                goto err;
            }
            one_literal->pos = pos;
            operation = Expression::makeInfix(p->m_alloc, op, left_copy, one_literal);
            if(!operation)
            {
                Expression::destroyExpression(one_literal);
                Expression::destroyExpression(left_copy);
                goto err;
            }
            operation->pos = pos;
            res = Expression::makeAssign(p->m_alloc, left, operation, true);
            if(!res)
            {
                Expression::destroyExpression(operation);
                goto err;
            }

            return res;
        err:
            Expression::destroyExpression(source);
            return NULL;
        }


        static Expression* parse_dot_expression(Parser* p, Expression* left)
        {
            p->lexer.nextToken();

            if(!p->lexer.expectCurrent(TOKEN_IDENT))
            {
                return NULL;
            }

            char* str = p->lexer.m_currtoken.copyLiteral(p->m_alloc);
            Expression* index = Expression::makeStringLiteral(p->m_alloc, str);
            if(!index)
            {
                p->m_alloc->release(str);
                return NULL;
            }
            index->pos = p->lexer.m_currtoken.pos;

            p->lexer.nextToken();

            Expression* res = Expression::makeIndex(p->m_alloc, left, index);
            if(!res)
            {
                Expression::destroyExpression(index);
                return NULL;
            }
            return res;
        }

};

struct Object
{
    public:

        static ObjectData* allocObjectData(GCMemory* mem, ObjectType t);
        static ObjectData* objectDataFromPool(GCMemory* mem, ObjectType t);
        static Allocator* getAllocator(GCMemory* mem);

        static inline Object makeFromData(ObjectType type, ObjectData* data)
        {
            uint64_t type_tag;
            Object object;
            object.handle = OBJECT_PATTERN;
            type_tag = get_type_tag(type) & 0x7;
            object.handle |= (type_tag << 48);
            object.handle |= (uintptr_t)data;// assumes no pointer exceeds 48 bits
            return object;
        }

        static inline Object makeNumber(double val)
        {
            Object o = { .number = val };
            if((o.handle & OBJECT_PATTERN) == OBJECT_PATTERN)
            {
                o.handle = 0x7ff8000000000000;
            }
            return o;
        }

        static inline Object makeBool(bool val)
        {
            return (Object){ .handle = OBJECT_BOOL_HEADER | val };
        }

        static inline Object makeNull()
        {
            return (Object){ .handle = OBJECT_NULL_PATTERN };
        }

        static Object makeString(GCMemory* mem, const char* string)
        {
            return Object::makeString(mem, string, strlen(string));
        }

        static Object makeString(GCMemory* mem, const char* string, size_t len)
        {
            Object res = Object::makeStringCapacity(mem, len);
            if(res.isNull())
            {
                return res;
            }
            bool ok = object_string_append(res, string, len);
            if(!ok)
            {
                return Object::makeNull();
            }
            return res;
        }

        static Object makeStringCapacity(GCMemory* mem, int capacity);

        static Object makeStringFormat(GCMemory* mem, const char* fmt, ...)
        {
            va_list args;
            va_start(args, fmt);
            int to_write = vsnprintf(NULL, 0, fmt, args);
            va_end(args);
            va_start(args, fmt);
            Object res = Object::makeStringCapacity(mem, to_write);
            if(res.isNull())
            {
                return Object::makeNull();
            }
            char* res_buf = object_get_mutable_string(res);
            int written = vsprintf(res_buf, fmt, args);
            (void)written;
            APE_ASSERT(written == to_write);
            va_end(args);
            object_set_string_length(res, to_write);
            return res;
        }

        static Object makeNativeFunctionMemory(GCMemory* mem, const char* name, NativeFNCallback fn, void* data, int data_len);

        static Object makeArray(GCMemory* mem)
        {
            return Object::makeArray(mem, 8);
        }

        static Object makeArray(GCMemory* mem, unsigned capacity);

        static Object makeMap(GCMemory* mem)
        {
            return Object::makeMap(mem, 32);
        }

        static Object makeMap(GCMemory* mem, unsigned capacity);

        static Object makeError(GCMemory* mem, const char* error)
        {
            char* error_str = ape_strdup(Object::getAllocator(mem), error);
            if(!error_str)
            {
                return Object::makeNull();
            }
            Object res = Object::makeErrorNoCopy(mem, error_str);
            if(res.isNull())
            {
                Object::getAllocator(mem)->release(error_str);
                return Object::makeNull();
            }
            return res;
        }

        static Object makeErrorNoCopy(GCMemory* mem, char* error);

        static Object makeErrorFormat(GCMemory* mem, const char* fmt, ...)
        {
            va_list args;
            va_start(args, fmt);
            int to_write = vsnprintf(NULL, 0, fmt, args);
            va_end(args);
            va_start(args, fmt);
            char* res = (char*)Object::getAllocator(mem)->allocate(to_write + 1);
            if(!res)
            {
                return Object::makeNull();
            }
            int written = vsprintf(res, fmt, args);
            (void)written;
            APE_ASSERT(written == to_write);
            va_end(args);
            Object res_obj = Object::makeErrorNoCopy(mem, res);
            if(res_obj.isNull())
            {
                Object::getAllocator(mem)->release(res);
                return Object::makeNull();
            }
            return res_obj;
        }

        static Object makeFunction(GCMemory* mem, const char* name, CompilationResult* comp_res, bool owns_data, int num_locals, int num_args, int free_vals_count);

        static Object makeExternal(GCMemory* mem, void* data);

        static void deinit(Object obj);

    public:
        uint64_t _internal;
        union
        {
            uint64_t handle;
            double number;
        };

    public:
        static inline ObjectType typeFromData(ObjectData* data);

        ObjectType type()
        {
            if(this->isNumber())
            {
                return APE_OBJECT_NUMBER;
            }
            uint64_t tag = (this->handle >> 48) & 0x7;
            switch(tag)
            {
                case 0:
                    {
                        return APE_OBJECT_NONE;
                    }
                    break;
                case 1:
                    {
                        return APE_OBJECT_BOOL;
                    }
                    break;
                case 2:
                    {
                        return APE_OBJECT_NULL;
                    }
                    break;
                case 4:
                    {
                        return Object::typeFromData(this->getAllocatedData());
                    }
                    break;
                default:
                    break;
            }
            return APE_OBJECT_NONE;
        }

        ObjectData* getAllocatedData()
        {
            APE_ASSERT(GCMemory::objectIsAllocated(*this) || this->type() == APE_OBJECT_NULL);
            return (ObjectData*)(this->handle & ~OBJECT_HEADER_MASK);
        }

        bool isHashable()
        {
            ObjectType type = this->type();
            switch(type)
            {
                case APE_OBJECT_STRING:
                    return true;
                case APE_OBJECT_NUMBER:
                    return true;
                case APE_OBJECT_BOOL:
                    return true;
                default:
                    break;
            }
            return false;
        }

        bool isNumeric()
        {
            ObjectType type = this->type();
            return type == APE_OBJECT_NUMBER || type == APE_OBJECT_BOOL;
        }

        bool isNull()
        {
            return this->type() == APE_OBJECT_NULL;
        }

        bool isCallable()
        {
            ObjectType type = this->type();
            return type == APE_OBJECT_NATIVE_FUNCTION || type == APE_OBJECT_FUNCTION;
        }

        bool isNumber()
        {
            return (this->handle & OBJECT_PATTERN) != OBJECT_PATTERN;
        }

};


struct String
{
    union
    {
        char* value_allocated;
        char value_buf[OBJECT_STRING_BUF_SIZE];
    };
    unsigned long hash;
    bool is_allocated;
    int m_capacity;
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
    Context* context;
    void* data;
};       

struct ScriptFunction
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

struct ObjectData
{
    GCMemory* mem;
    union
    {
        String string;
        ObjectError error;
        Array* array;
        ValDictionary * map;
        ScriptFunction function;
        NativeFunction native_function;
        ExternalData external;
    };
    bool gcmark;
    ObjectType type;
};

struct ObjectDataPool
{
    ObjectData* datapool[GCMEM_POOL_SIZE];
    int m_count;
};

inline ObjectType Object::typeFromData(ObjectData* data)
{
    return data->type;
}

struct GCMemory: public Allocator::Allocated
{
    public:
        static void markObjects(Object* objects, int cn)
        {
            for(int i = 0; i < cn; i++)
            {
                Object obj = objects[i];
                markObject(obj);
            }
        }

        static void markObject(Object obj)
        {
            if(!objectIsAllocated(obj))
            {
                return;
            }

            ObjectData* data = obj.getAllocatedData();
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
                        if(objectIsAllocated(key))
                        {
                            ObjectData* key_data = key.getAllocatedData();
                            if(!key_data->gcmark)
                            {
                                GCMemory::markObject(key);
                            }
                        }
                        Object val = object_get_map_value_at(obj, i);
                        if(objectIsAllocated(val))
                        {
                            ObjectData* val_data = val.getAllocatedData();
                            if(!val_data->gcmark)
                            {
                                GCMemory::markObject(val);
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
                        if(objectIsAllocated(val))
                        {
                            ObjectData* val_data = val.getAllocatedData();
                            if(!val_data->gcmark)
                            {
                                GCMemory::markObject(val);
                            }
                        }
                    }
                    break;
                }
                case APE_OBJECT_FUNCTION:
                {
                    ScriptFunction* function = object_get_function(obj);
                    for(int i = 0; i < function->free_vals_count; i++)
                    {
                        Object free_val = object_get_function_free_val(obj, i);
                        GCMemory::markObject(free_val);
                        if(objectIsAllocated(free_val))
                        {
                            ObjectData* free_val_data = free_val.getAllocatedData();
                            if(!free_val_data->gcmark)
                            {
                                GCMemory::markObject(free_val);
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

        static bool objectIsAllocated(Object object)
        {
            return (object.handle & OBJECT_ALLOCATED_HEADER) == OBJECT_ALLOCATED_HEADER;
        }

        static GCMemory* objectGetMemory(Object obj)
        {
            ObjectData* data = obj.getAllocatedData();
            return data->mem;
        }

        static bool disableFor(Object obj)
        {
            if(!objectIsAllocated(obj))
            {
                return false;
            }
            ObjectData* data = obj.getAllocatedData();
            if(data->mem->objects_not_gced->contains(&obj))
            {
                return false;
            }
            bool ok = data->mem->objects_not_gced->add(&obj);
            return ok;
        }

        static void enableFor(Object obj)
        {
            if(!objectIsAllocated(obj))
            {
                return;
            }
            ObjectData* data = obj.getAllocatedData();
            data->mem->objects_not_gced->removeItem(&obj);
        }

        static GCMemory* make(Allocator* alloc)
        {
            int i;
            GCMemory* mem = alloc->allocate<GCMemory>();
            if(!mem)
            {
                return NULL;
            }
            memset(mem, 0, sizeof(GCMemory));
            mem->m_alloc = alloc;
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
            mem->destroy();
            return NULL;
        }


    public:
        int allocations_since_sweep;
        PtrArray * objects;
        PtrArray * objects_back;
        Array * objects_not_gced;
        ObjectDataPool data_only_pool;
        ObjectDataPool pools[GCMEM_POOLS_NUM];

    public:
        void destroy()
        {
            if(!this)
            {
                return;
            }

            Array::destroy(this->objects_not_gced);
            this->objects_back->destroy();

            for(int i = 0; i < this->objects->count(); i++)
            {
                ObjectData* obj = (ObjectData*)this->objects->get(i);
                object_data_deinit(obj);
                m_alloc->release(obj);
            }
            this->objects->destroy();

            for(int i = 0; i < GCMEM_POOLS_NUM; i++)
            {
                ObjectDataPool* pool = &this->pools[i];
                for(int j = 0; j < pool->m_count; j++)
                {
                    ObjectData* data = pool->datapool[j];
                    object_data_deinit(data);
                    m_alloc->release(data);
                }
                memset(pool, 0, sizeof(ObjectDataPool));
            }

            for(int i = 0; i < this->data_only_pool.m_count; i++)
            {
                m_alloc->release(this->data_only_pool.datapool[i]);
            }

            m_alloc->release(this);
        }

        ObjectData* allocObjectData(ObjectType type)
        {
            ObjectData* data = NULL;
            this->allocations_since_sweep++;
            if(this->data_only_pool.m_count > 0)
            {
                data = this->data_only_pool.datapool[this->data_only_pool.m_count - 1];
                this->data_only_pool.m_count--;
            }
            else
            {
                data = m_alloc->allocate<ObjectData>();
                if(!data)
                {
                    return NULL;
                }
            }

            memset(data, 0, sizeof(ObjectData));

            APE_ASSERT(this->objects_back->count() >= this->objects->count());
            // we want to make sure that appending to objects_back never fails in sweep
            // so this only reserves space there.
            bool ok = this->objects_back->add(data);
            if(!ok)
            {
                m_alloc->release(data);
                return NULL;
            }
            ok = this->objects->add(data);
            if(!ok)
            {
                m_alloc->release(data);
                return NULL;
            }
            data->mem = this;
            data->type = type;
            return data;
        }

        ObjectData* objectDataFromPool(ObjectType type)
        {
            ObjectDataPool* pool = this->getPoolByType(type);
            if(!pool || pool->m_count <= 0)
            {
                return NULL;
            }
            ObjectData* data = pool->datapool[pool->m_count - 1];
            APE_ASSERT(this->objects_back->count() >= this->objects->count());
            // we want to make sure that appending to objects_back never fails in sweep
            // so this only reserves space there.
            bool ok = this->objects_back->add(data);
            if(!ok)
            {
                return NULL;
            }
            ok = this->objects->add(data);
            if(!ok)
            {
                return NULL;
            }
            pool->m_count--;
            return data;
        }

        void unmarkAll()
        {
            for(int i = 0; i < this->objects->count(); i++)
            {
                ObjectData* data = (ObjectData*)this->objects->get(i);
                data->gcmark = false;
            }
        }

        void sweep()
        {
            int i;
            bool ok;
            GCMemory::markObjects((Object*)this->objects_not_gced->data(), this->objects_not_gced->count());

            APE_ASSERT(this->objects_back->count() >= this->objects->count());

            this->objects_back->clear();
            for(i = 0; i < this->objects->count(); i++)
            {
                ObjectData* data = (ObjectData*)this->objects->get(i);
                if(data->gcmark)
                {
                    // this should never fail because objects_back's size should be equal to objects
                    ok = this->objects_back->add(data);
                    (void)ok;
                    APE_ASSERT(ok);
                }
                else
                {
                    if(this->canStoreInPool(data))
                    {
                        ObjectDataPool* pool = this->getPoolByType(data->type);
                        pool->datapool[pool->m_count] = data;
                        pool->m_count++;
                    }
                    else
                    {
                        object_data_deinit(data);
                        if(this->data_only_pool.m_count < GCMEM_POOL_SIZE)
                        {
                            this->data_only_pool.datapool[this->data_only_pool.m_count] = data;
                            this->data_only_pool.m_count++;
                        }
                        else
                        {
                            m_alloc->release(data);
                        }
                    }
                }
            }
            PtrArray* objs_temp = this->objects;
            this->objects = this->objects_back;
            this->objects_back = objs_temp;
            this->allocations_since_sweep = 0;
        }

        int shouldSweep()
        {
            return this->allocations_since_sweep > GCMEM_SWEEP_INTERVAL;
        }

        // INTERNAL
        ObjectDataPool* getPoolByType(ObjectType type)
        {
            switch(type)
            {
                case APE_OBJECT_ARRAY:
                    return &this->pools[0];
                case APE_OBJECT_MAP:
                    return &this->pools[1];
                case APE_OBJECT_STRING:
                    return &this->pools[2];
                default:
                    return NULL;
            }
        }

        bool canStoreInPool(ObjectData* data)
        {
            Object obj = Object::makeFromData(data->type, data);

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
                    if(!data->string.is_allocated || data->string.m_capacity > 4096)
                    {
                        return false;
                    }
                    break;
                }
                default:
                    break;
            }

            ObjectDataPool* pool = this->getPoolByType(data->type);
            if(!pool || pool->m_count >= GCMEM_POOL_SIZE)
            {
                return false;
            }

            return true;
        }

};

struct BlockScope: public Allocator::Allocated
{
    Dictionary * store;
    int offset;
    int num_definitions;
};


struct OpcodeDefinition
{
    const char* name;
    int num_operands;
    int operand_widths[2];
};

struct CompilationResult: public Allocator::Allocated
{
    uint8_t* bytecode;
    Position* src_positions;
    int m_count;
};

struct CompilationScope: public Allocator::Allocated
{
    CompilationScope* outer;
    Array * bytecode;
    Array * src_positions;
    Array * break_ip_stack;
    Array * continue_ip_stack;
    opcode_t last_opcode;
};


struct TracebackItem
{
    char* function_name;
    Position pos;
};

struct Traceback: public Allocator::Allocated
{
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

struct VM: public Allocator::Allocated
{
    public:
        static VM* make(Allocator* alloc, const Config* config, GCMemory* mem, ErrorList* errors, GlobalStore* global_store)
        {
            VM* vm = alloc->allocate<VM>();
            if(!vm)
            {
                return NULL;
            }
            memset(vm, 0, sizeof(VM));
            vm->m_alloc = alloc;
            vm->config = config;
            vm->mem = mem;
            vm->m_errors = errors;
            vm->global_store = global_store;
            vm->globals_count = 0;
            vm->sp = 0;
            vm->this_sp = 0;
            vm->frames_count = 0;
            vm->last_popped = Object::makeNull();
            vm->running = false;

            for(int i = 0; i < OPCODE_MAX; i++)
            {
                vm->operator_oveload_keys[i] = Object::makeNull();
            }
        #define SET_OPERATOR_OVERLOAD_KEY(op, key)                   \
            do                                                       \
            {                                                        \
                Object key_obj = Object::makeString(vm->mem, key); \
                if(key_obj.isNull())                          \
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
            vm->destroy();
            return NULL;
        }

    public:
        const Config* config;
        GCMemory* mem;
        ErrorList* m_errors;
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

    public:
        void destroy()
        {
            if(!this)
            {
                return;
            }
            m_alloc->release(this);
        }

        void reset()
        {
            this->sp = 0;
            this->this_sp = 0;
            while(this->frames_count > 0)
            {
                this->popFrame();
            }
        }

        bool run(CompilationResult* comp_res, Array * constants)
        {
        #ifdef APE_DEBUG
            int old_sp = this->sp;
        #endif
            int old_this_sp = this->this_sp;
            int old_frames_count = this->frames_count;
            Object main_fn = Object::makeFunction(this->mem, "main", comp_res, false, 0, 0, 0);
            if(main_fn.isNull())
            {
                return false;
            }
            this->stackPush(main_fn);
            bool res = this->executeFunction(main_fn, constants);
            while(this->frames_count > old_frames_count)
            {
                this->popFrame();
            }
            APE_ASSERT(this->sp == old_sp);
            this->this_sp = old_this_sp;
            return res;
        }

        Object call(Array * constants, Object callee, int argc, Object* args)
        {
            ObjectType type = callee.type();
            if(type == APE_OBJECT_FUNCTION)
            {
        #ifdef APE_DEBUG
                int old_sp = this->sp;
        #endif
                int old_this_sp = this->this_sp;
                int old_frames_count = this->frames_count;
                this->stackPush(callee);
                for(int i = 0; i < argc; i++)
                {
                    this->stackPush(args[i]);
                }
                bool ok = this->executeFunction(callee, constants);
                if(!ok)
                {
                    return Object::makeNull();
                }
                while(this->frames_count > old_frames_count)
                {
                    this->popFrame();
                }
                APE_ASSERT(this->sp == old_sp);
                this->this_sp = old_this_sp;
                return this->getLastPopped();
            }
            else if(type == APE_OBJECT_NATIVE_FUNCTION)
            {
                return this->callNativeFunction(callee, Position::invalid(), argc, args);
            }
            else
            {
                m_errors->addError(APE_ERROR_USER, Position::invalid(), "Object is not callable");
                return Object::makeNull();
            }
        }

        Object getLastPopped()
        {
            return this->last_popped;
        }

        bool hasErrors()
        {
            return m_errors->count() > 0;
        }

        bool setGlobalByIndex(int ix, Object val)
        {
            if(ix >= VM_MAX_GLOBALS)
            {
                APE_ASSERT(false);
                m_errors->addError(APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "Global write out of range");
                return false;
            }
            this->globals[ix] = val;
            if(ix >= this->globals_count)
            {
                this->globals_count = ix + 1;
            }
            return true;
        }

        Object getGlobalByIndex(int ix)
        {
            if(ix >= VM_MAX_GLOBALS)
            {
                APE_ASSERT(false);
                m_errors->addError(APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "Global read out of range");
                return Object::makeNull();
            }
            return this->globals[ix];
        }

        void setStackPointer(int new_sp)
        {
            if(new_sp > this->sp)
            {// to avoid gcing freed objects
                int cn = new_sp - this->sp;
                size_t bytes_count = cn * sizeof(Object);
                memset(this->stack + this->sp, 0, bytes_count);
            }
            this->sp = new_sp;
        }

        void stackPush(Object obj)
        {
        #ifdef APE_DEBUG
            if(this->sp >= VM_STACK_SIZE)
            {
                APE_ASSERT(false);
                m_errors->addError(APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "Stack overflow");
                return;
            }
            if(this->current_frame)
            {
                Frame* frame = this->current_frame;
                ScriptFunction* current_function = object_get_function(frame->function);
                int num_locals = current_function->num_locals;
                APE_ASSERT(this->sp >= (frame->base_pointer + num_locals));
            }
        #endif
            this->stack[this->sp] = obj;
            this->sp++;
        }

        Object stackPop()
        {
        #ifdef APE_DEBUG
            if(this->sp == 0)
            {
                m_errors->addError(APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "Stack underflow");
                APE_ASSERT(false);
                return Object::makeNull();
            }
            if(this->current_frame)
            {
                Frame* frame = this->current_frame;
                ScriptFunction* current_function = object_get_function(frame->function);
                int num_locals = current_function->num_locals;
                APE_ASSERT((this->sp - 1) >= (frame->base_pointer + num_locals));
            }
        #endif
            this->sp--;
            Object res = this->stack[this->sp];
            this->last_popped = res;
            return res;
        }

        Object stackGet(int nth_item)
        {
            int ix = this->sp - 1 - nth_item;
        #ifdef APE_DEBUG
            if(ix < 0 || ix >= VM_STACK_SIZE)
            {
                m_errors->addFormat(APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "Invalid stack index: %d", nth_item);
                APE_ASSERT(false);
                return Object::makeNull();
            }
        #endif
            return this->stack[ix];
        }

        void pushThisStack(Object obj)
        {
        #ifdef APE_DEBUG
            if(this->this_sp >= VM_THIS_STACK_SIZE)
            {
                APE_ASSERT(false);
                m_errors->addError(APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "this stack overflow");
                return;
            }
        #endif
            this->this_stack[this->this_sp] = obj;
            this->this_sp++;
        }

        Object popThisStack()
        {
        #ifdef APE_DEBUG
            if(this->this_sp == 0)
            {
                m_errors->addError(APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "this stack underflow");
                APE_ASSERT(false);
                return Object::makeNull();
            }
        #endif
            this->this_sp--;
            return this->this_stack[this->this_sp];
        }

        Object getThisStack(int nth_item)
        {
            int ix = this->this_sp - 1 - nth_item;
        #ifdef APE_DEBUG
            if(ix < 0 || ix >= VM_THIS_STACK_SIZE)
            {
                m_errors->addFormat(APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "Invalid this stack index: %d", nth_item);
                APE_ASSERT(false);
                return Object::makeNull();
            }
        #endif
            return this->this_stack[ix];
        }

        bool pushFrame(Frame frame)
        {
            if(this->frames_count >= VM_MAX_FRAMES)
            {
                APE_ASSERT(false);
                return false;
            }
            this->frames[this->frames_count] = frame;
            this->current_frame = &this->frames[this->frames_count];
            this->frames_count++;
            ScriptFunction* frame_function = object_get_function(frame.function);
            this->setStackPointer(frame.base_pointer + frame_function->num_locals);
            return true;
        }

        bool popFrame()
        {
            this->setStackPointer(this->current_frame->base_pointer - 1);
            if(this->frames_count <= 0)
            {
                APE_ASSERT(false);
                this->current_frame = NULL;
                return false;
            }
            this->frames_count--;
            if(this->frames_count == 0)
            {
                this->current_frame = NULL;
                return false;
            }
            this->current_frame = &this->frames[this->frames_count - 1];
            return true;
        }

        void runGarbageCollector(Array * constants)
        {
            this->mem->unmarkAll();
            GCMemory::markObjects(global_store_get_object_data(this->global_store), global_store_get_object_count(this->global_store));
            GCMemory::markObjects((Object*)constants->data(), constants->count());
            GCMemory::markObjects(this->globals, this->globals_count);
            for(int i = 0; i < this->frames_count; i++)
            {
                Frame* frame = &this->frames[i];
                GCMemory::markObject(frame->function);
            }
            GCMemory::markObjects(this->stack, this->sp);
            GCMemory::markObjects(this->this_stack, this->this_sp);
            GCMemory::markObject(this->last_popped);
            GCMemory::markObjects(this->operator_oveload_keys, OPCODE_MAX);
            this->mem->sweep();
        }

        bool callObject(Object callee, int num_args)
        {
            ObjectType callee_type = callee.type();
            if(callee_type == APE_OBJECT_FUNCTION)
            {
                ScriptFunction* callee_function = object_get_function(callee);
                if(num_args != callee_function->num_args)
                {
                    m_errors->addFormat(APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                      "Invalid number of arguments to \"%s\", expected %d, got %d",
                                      object_get_function_name(callee), callee_function->num_args, num_args);
                    return false;
                }
                Frame callee_frame;
                bool ok = frame_init(&callee_frame, callee, this->sp - num_args);
                if(!ok)
                {
                    m_errors->addError(APE_ERROR_RUNTIME, Position::invalid(), "Frame init failed in call_object");
                    return false;
                }
                ok = this->pushFrame(callee_frame);
                if(!ok)
                {
                    m_errors->addError(APE_ERROR_RUNTIME, Position::invalid(), "Pushing frame failed in call_object");
                    return false;
                }
            }
            else if(callee_type == APE_OBJECT_NATIVE_FUNCTION)
            {
                Object* stack_pos = this->stack + this->sp - num_args;
                Object res = this->callNativeFunction(callee, frame_src_position(this->current_frame), num_args, stack_pos);
                if(this->hasErrors())
                {
                    return false;
                }
                this->setStackPointer(this->sp - num_args - 1);
                this->stackPush(res);
            }
            else
            {
                const char* callee_type_name = object_get_type_name(callee_type);
                m_errors->addFormat(APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "%s object is not callable", callee_type_name);
                return false;
            }
            return true;
        }

        Object callNativeFunction(Object callee, Position src_pos, int argc, Object* args)
        {
            NativeFunction* native_fun = object_get_native_function(callee);
            Object res = native_fun->native_funcptr(this, native_fun->data, argc, args);
            if(m_errors->hasErrors() && !APE_STREQ(native_fun->name, "crash"))
            {
                ErrorList::Error* err = m_errors->getLast();
                err->pos = src_pos;
                err->traceback = traceback_make(m_alloc);
                if(err->traceback)
                {
                    traceback_append(err->traceback, native_fun->name, Position::invalid());
                }
                return Object::makeNull();
            }
            ObjectType res_type = res.type();
            if(res_type == APE_OBJECT_ERROR)
            {
                Traceback* traceback = traceback_make(m_alloc);
                if(traceback)
                {
                    // error builtin is treated in a special way
                    if(!APE_STREQ(native_fun->name, "error"))
                    {
                        traceback_append(traceback, native_fun->name, Position::invalid());
                    }
                    traceback_append_from_vm(traceback, this);
                    object_set_error_traceback(res, traceback);
                }
            }
            return res;
        }

        bool checkAssign(Object old_value, Object new_value)
        {
            ObjectType old_value_type;
            ObjectType new_value_type;
            (void)this;
            old_value_type = old_value.type();
            new_value_type = new_value.type();
            if(old_value_type == APE_OBJECT_NULL || new_value_type == APE_OBJECT_NULL)
            {
                return true;
            }
            if(old_value_type != new_value_type)
            {
                /*
                m_errors->addFormat(APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "Trying to assign variable of type %s to %s",
                                  object_get_type_name(new_value_type), object_get_type_name(old_value_type));
                return false;
                */
            }
            return true;
        }

        bool findOverload(Object left, Object right, opcode_t op, bool* out_overload_found)
        {
            int num_operands;
            Object key;
            Object callee;
            ObjectType left_type;
            ObjectType right_type;
            *out_overload_found = false;
            left_type = left.type();
            right_type = right.type();
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
            key = this->operator_oveload_keys[op];
            callee = Object::makeNull();
            if(left_type == APE_OBJECT_MAP)
            {
                callee = object_get_map_value_object(left, key);
            }
            if(!callee.isCallable())
            {
                if(right_type == APE_OBJECT_MAP)
                {
                    callee = object_get_map_value_object(right, key);
                }
                if(!callee.isCallable())
                {
                    *out_overload_found = false;
                    return true;
                }
            }
            *out_overload_found = true;
            this->stackPush(callee);
            this->stackPush(left);
            if(num_operands == 2)
            {
                this->stackPush( right);
            }
            return this->callObject(callee, num_operands);
        }


        bool executeFunction(Object function, Array * constants)
        {
            if(this->running)
            {
                m_errors->addError(APE_ERROR_USER, Position::invalid(), "VM is already executing code");
                return false;
            }

            ScriptFunction* function_function = object_get_function(function);// naming is hard
            Frame new_frame;
            bool ok = false;
            ok = frame_init(&new_frame, function, this->sp - function_function->num_args);
            if(!ok)
            {
                return false;
            }
            ok = this->pushFrame(new_frame);
            if(!ok)
            {
                m_errors->addError(APE_ERROR_USER, Position::invalid(), "Pushing frame failed");
                return false;
            }

            this->running = true;
            this->last_popped = Object::makeNull();

            bool check_time = false;
            double max_exec_time_ms = 0;
            if(this->config)
            {
                check_time = this->config->max_execution_time_set;
                max_exec_time_ms = this->config->max_execution_time_ms;
            }
            unsigned time_check_interval = 1000;
            unsigned time_check_counter = 0;
            Timer timer;
            memset(&timer, 0, sizeof(Timer));
            if(check_time)
            {
                timer = ape_timer_start();
            }

            while(this->current_frame->ip < this->current_frame->bytecode_size)
            {
                OpcodeValue opcode = frame_read_opcode(this->current_frame);
                switch(opcode)
                {
                    case OPCODE_CONSTANT:
                        {
                            uint16_t constant_ix = frame_read_uint16(this->current_frame);
                            Object* constant = (Object*)constants->get(constant_ix);
                            if(!constant)
                            {
                                m_errors->addFormat(APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                  "Constant at %d not found", constant_ix);
                                goto err;
                            }
                            this->stackPush(*constant);
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
                            Object right = this->stackPop();
                            Object left = this->stackPop();
                            ObjectType left_type = left.type();
                            ObjectType right_type = right.type();
                            if(left.isNumeric() && right.isNumeric())
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
                                this->stackPush(Object::makeNumber(res));
                            }
                            else if(left_type == APE_OBJECT_STRING && right_type == APE_OBJECT_STRING && opcode == OPCODE_ADD)
                            {
                                int left_len = (int)object_get_string_length(left);
                                int right_len = (int)object_get_string_length(right);

                                if(left_len == 0)
                                {
                                    this->stackPush(right);
                                }
                                else if(right_len == 0)
                                {
                                    this->stackPush(left);
                                }
                                else
                                {
                                    const char* left_val = object_get_string(left);
                                    const char* right_val = object_get_string(right);

                                    Object res = Object::makeStringCapacity(this->mem, left_len + right_len);
                                    if(res.isNull())
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
                                    this->stackPush(res);
                                }
                            }
                            else if((left_type == APE_OBJECT_ARRAY) && opcode == OPCODE_ADD)
                            {
                                object_add_array_value(left, right);
                                this->stackPush(left);
                            }
                            else
                            {
                                bool overload_found = false;
                                bool ok = this->findOverload(left, right, opcode, &overload_found);
                                if(!ok)
                                {
                                    goto err;
                                }
                                if(!overload_found)
                                {
                                    const char* opcode_name = opcode_get_name(opcode);
                                    const char* left_type_name = object_get_type_name(left_type);
                                    const char* right_type_name = object_get_type_name(right_type);
                                    m_errors->addFormat(APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                      "Invalid operand types for %s, got %s and %s", opcode_name, left_type_name,
                                                      right_type_name);
                                    goto err;
                                }
                            }
                        }
                        break;

                    case OPCODE_POP:
                        {
                            this->stackPop();
                        }
                        break;

                    case OPCODE_TRUE:
                        {
                            this->stackPush(Object::makeBool(true));
                        }
                        break;

                    case OPCODE_FALSE:
                    {
                        this->stackPush(Object::makeBool(false));
                        break;
                    }
                    case OPCODE_COMPARE:
                    case OPCODE_COMPARE_EQ:
                        {
                            Object right = this->stackPop();
                            Object left = this->stackPop();
                            bool is_overloaded = false;
                            bool ok = this->findOverload(left, right, OPCODE_COMPARE, &is_overloaded);
                            if(!ok)
                            {
                                goto err;
                            }
                            if(!is_overloaded)
                            {
                                double comparison_res = object_compare(left, right, &ok);
                                if(ok || opcode == OPCODE_COMPARE_EQ)
                                {
                                    Object res = Object::makeNumber(comparison_res);
                                    this->stackPush(res);
                                }
                                else
                                {
                                    const char* right_type_string = object_get_type_name(right.type());
                                    const char* left_type_string = object_get_type_name(left.type());
                                    m_errors->addFormat(APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
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
                            Object value = this->stackPop();
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
                            Object res = Object::makeBool(res_val);
                            this->stackPush(res);
                        }
                        break;

                    case OPCODE_MINUS:
                        {
                            Object operand = this->stackPop();
                            ObjectType operand_type = operand.type();
                            if(operand_type == APE_OBJECT_NUMBER)
                            {
                                double val = object_get_number(operand);
                                Object res = Object::makeNumber(-val);
                                this->stackPush(res);
                            }
                            else
                            {
                                bool overload_found = false;
                                bool ok = this->findOverload(operand, Object::makeNull(), OPCODE_MINUS, &overload_found);
                                if(!ok)
                                {
                                    goto err;
                                }
                                if(!overload_found)
                                {
                                    const char* operand_type_string = object_get_type_name(operand_type);
                                    m_errors->addFormat(APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                      "Invalid operand type for MINUS, got %s", operand_type_string);
                                    goto err;
                                }
                            }
                        }
                        break;

                    case OPCODE_BANG:
                        {
                            Object operand = this->stackPop();
                            ObjectType type = operand.type();
                            if(type == APE_OBJECT_BOOL)
                            {
                                Object res = Object::makeBool(!object_get_bool(operand));
                                this->stackPush(res);
                            }
                            else if(type == APE_OBJECT_NULL)
                            {
                                Object res = Object::makeBool(true);
                                this->stackPush(res);
                            }
                            else
                            {
                                bool overload_found = false;
                                bool ok = this->findOverload(operand, Object::makeNull(), OPCODE_BANG, &overload_found);
                                if(!ok)
                                {
                                    goto err;
                                }
                                if(!overload_found)
                                {
                                    Object res = Object::makeBool(false);
                                    this->stackPush(res);
                                }
                            }
                        }
                        break;

                    case OPCODE_JUMP:
                        {
                            uint16_t pos = frame_read_uint16(this->current_frame);
                            this->current_frame->ip = pos;
                        }
                        break;

                    case OPCODE_JUMP_IF_FALSE:
                        {
                            uint16_t pos = frame_read_uint16(this->current_frame);
                            Object test = this->stackPop();
                            if(!object_get_bool(test))
                            {
                                this->current_frame->ip = pos;
                            }
                        }
                        break;

                    case OPCODE_JUMP_IF_TRUE:
                        {
                            uint16_t pos = frame_read_uint16(this->current_frame);
                            Object test = this->stackPop();
                            if(object_get_bool(test))
                            {
                                this->current_frame->ip = pos;
                            }
                        }
                        break;

                    case OPCODE_NULL:
                        {
                            this->stackPush(Object::makeNull());
                        }
                        break;

                    case OPCODE_DEFINE_MODULE_GLOBAL:
                    {
                        uint16_t ix = frame_read_uint16(this->current_frame);
                        Object value = this->stackPop();
                        this->setGlobalByIndex(ix, value);
                        break;
                    }
                    case OPCODE_SET_MODULE_GLOBAL:
                        {
                            uint16_t ix = frame_read_uint16(this->current_frame);
                            Object new_value = this->stackPop();
                            Object old_value = this->getGlobalByIndex(ix);
                            if(!this->checkAssign(old_value, new_value))
                            {
                                goto err;
                            }
                            this->setGlobalByIndex(ix, new_value);
                        }
                        break;

                    case OPCODE_GET_MODULE_GLOBAL:
                        {
                            uint16_t ix = frame_read_uint16(this->current_frame);
                            Object global = this->globals[ix];
                            this->stackPush(global);
                        }
                        break;

                    case OPCODE_ARRAY:
                        {
                            uint16_t cn = frame_read_uint16(this->current_frame);
                            Object array_obj = Object::makeArray(this->mem, cn);
                            if(array_obj.isNull())
                            {
                                goto err;
                            }
                            Object* items = this->stack + this->sp - cn;
                            for(int i = 0; i < cn; i++)
                            {
                                Object item = items[i];
                                ok = object_add_array_value(array_obj, item);
                                if(!ok)
                                {
                                    goto err;
                                }
                            }
                            this->setStackPointer(this->sp - cn);
                            this->stackPush(array_obj);
                        }
                        break;

                    case OPCODE_MAP_START:
                        {
                            uint16_t cn = frame_read_uint16(this->current_frame);
                            Object map_obj = Object::makeMap(this->mem, cn);
                            if(map_obj.isNull())
                            {
                                goto err;
                            }
                            this->pushThisStack(map_obj);
                        }
                        break;

                    case OPCODE_MAP_END:
                        {
                            uint16_t kvp_count = frame_read_uint16(this->current_frame);
                            uint16_t items_count = kvp_count * 2;
                            Object map_obj = this->popThisStack();
                            Object* kv_pairs = this->stack + this->sp - items_count;
                            for(int i = 0; i < items_count; i += 2)
                            {
                                Object key = kv_pairs[i];
                                if(!key.isHashable())
                                {
                                    ObjectType key_type = key.type();
                                    const char* key_type_name = object_get_type_name(key_type);
                                    m_errors->addFormat(APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
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
                            this->setStackPointer(this->sp - items_count);
                            this->stackPush(map_obj);
                        }
                        break;

                    case OPCODE_GET_THIS:
                        {
                            Object obj = this->getThisStack(0);
                            this->stackPush(obj);
                        }
                        break;

                    case OPCODE_GET_INDEX:
                        {
                            #if 0
                            const char* idxname;
                            #endif
                            Object index = this->stackPop();
                            Object left = this->stackPop();
                            ObjectType left_type = left.type();
                            ObjectType index_type = index.type();
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
                                        this->stackPush(afn(this, NULL, argc, args));
                                        break;
                                    }
                                }
                            }
                            #endif

                            if(left_type != APE_OBJECT_ARRAY && left_type != APE_OBJECT_MAP && left_type != APE_OBJECT_STRING)
                            {
                                m_errors->addFormat(APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                    "Type %s is not indexable (in OPCODE_GET_INDEX)", left_type_name);
                                goto err;
                            }
                            Object res = Object::makeNull();

                            if(left_type == APE_OBJECT_ARRAY)
                            {
                                if(index_type != APE_OBJECT_NUMBER)
                                {
                                    m_errors->addFormat(APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
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
                                    res = Object::makeString(this->mem, res_str);
                                }
                            }
                            this->stackPush(res);
                        }
                        break;

                    case OPCODE_GET_VALUE_AT:
                    {
                        Object index = this->stackPop();
                        Object left = this->stackPop();
                        ObjectType left_type = left.type();
                        ObjectType index_type = index.type();
                        const char* left_type_name = object_get_type_name(left_type);
                        const char* index_type_name = object_get_type_name(index_type);

                        if(left_type != APE_OBJECT_ARRAY && left_type != APE_OBJECT_MAP && left_type != APE_OBJECT_STRING)
                        {
                            m_errors->addFormat(APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                              "Type %s is not indexable (in OPCODE_GET_VALUE_AT)", left_type_name);
                            goto err;
                        }

                        Object res = Object::makeNull();
                        if(index_type != APE_OBJECT_NUMBER)
                        {
                            m_errors->addFormat(APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
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
                            res = object_get_kv_pair_at(this->mem, left, ix);
                        }
                        else if(left_type == APE_OBJECT_STRING)
                        {
                            const char* str = object_get_string(left);
                            int left_len = object_get_string_length(left);
                            int ix = (int)object_get_number(index);
                            if(ix >= 0 && ix < left_len)
                            {
                                char res_str[2] = { str[ix], '\0' };
                                res = Object::makeString(this->mem, res_str);
                            }
                        }
                        this->stackPush(res);
                        break;
                    }
                    case OPCODE_CALL:
                    {
                        uint8_t num_args = frame_read_uint8(this->current_frame);
                        Object callee = this->stackGet(num_args);
                        bool ok = this->callObject(callee, num_args);
                        if(!ok)
                        {
                            goto err;
                        }
                        break;
                    }
                    case OPCODE_RETURN_VALUE:
                    {
                        Object res = this->stackPop();
                        bool ok = this->popFrame();
                        if(!ok)
                        {
                            goto end;
                        }
                        this->stackPush(res);
                        break;
                    }
                    case OPCODE_RETURN:
                    {
                        bool ok = this->popFrame();
                        this->stackPush(Object::makeNull());
                        if(!ok)
                        {
                            this->stackPop();
                            goto end;
                        }
                        break;
                    }
                    case OPCODE_DEFINE_LOCAL:
                    {
                        uint8_t pos = frame_read_uint8(this->current_frame);
                        this->stack[this->current_frame->base_pointer + pos] = this->stackPop();
                        break;
                    }
                    case OPCODE_SET_LOCAL:
                    {
                        uint8_t pos = frame_read_uint8(this->current_frame);
                        Object new_value = this->stackPop();
                        Object old_value = this->stack[this->current_frame->base_pointer + pos];
                        if(!this->checkAssign(old_value, new_value))
                        {
                            goto err;
                        }
                        this->stack[this->current_frame->base_pointer + pos] = new_value;
                        break;
                    }
                    case OPCODE_GET_LOCAL:
                    {
                        uint8_t pos = frame_read_uint8(this->current_frame);
                        Object val = this->stack[this->current_frame->base_pointer + pos];
                        this->stackPush(val);
                        break;
                    }
                    case OPCODE_GET_APE_GLOBAL:
                    {
                        uint16_t ix = frame_read_uint16(this->current_frame);
                        bool ok = false;
                        Object val = global_store_get_object_at(this->global_store, ix, &ok);
                        if(!ok)
                        {
                            m_errors->addFormat( APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                              "Global value %d not found", ix);
                            goto err;
                        }
                        this->stackPush(val);
                        break;
                    }
                    case OPCODE_FUNCTION:
                        {
                            uint16_t constant_ix = frame_read_uint16(this->current_frame);
                            uint8_t num_free = frame_read_uint8(this->current_frame);
                            Object* constant = (Object*)constants->get(constant_ix);
                            if(!constant)
                            {
                                m_errors->addFormat( APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                  "Constant %d not found", constant_ix);
                                goto err;
                            }
                            ObjectType constant_type = (*constant).type();
                            if(constant_type != APE_OBJECT_FUNCTION)
                            {
                                const char* type_name = object_get_type_name(constant_type);
                                m_errors->addFormat( APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                  "%s is not a function", type_name);
                                goto err;
                            }

                            const ScriptFunction* constant_function = object_get_function(*constant);
                            Object function_obj
                            = Object::makeFunction(this->mem, object_get_function_name(*constant), constant_function->comp_result,
                                                   false, constant_function->num_locals, constant_function->num_args, num_free);
                            if(function_obj.isNull())
                            {
                                goto err;
                            }
                            for(int i = 0; i < num_free; i++)
                            {
                                Object free_val = this->stack[this->sp - num_free + i];
                                object_set_function_free_val(function_obj, i, free_val);
                            }
                            this->setStackPointer(this->sp - num_free);
                            this->stackPush(function_obj);
                        }
                        break;
                    case OPCODE_GET_FREE:
                        {
                            uint8_t free_ix = frame_read_uint8(this->current_frame);
                            Object val = object_get_function_free_val(this->current_frame->function, free_ix);
                            this->stackPush(val);
                        }
                        break;
                    case OPCODE_SET_FREE:
                        {
                            uint8_t free_ix = frame_read_uint8(this->current_frame);
                            Object val = this->stackPop();
                            object_set_function_free_val(this->current_frame->function, free_ix, val);
                        }
                        break;
                    case OPCODE_CURRENT_FUNCTION:
                        {
                            Object current_function = this->current_frame->function;
                            this->stackPush(current_function);
                        }
                        break;
                    case OPCODE_SET_INDEX:
                        {
                            Object index = this->stackPop();
                            Object left = this->stackPop();
                            Object new_value = this->stackPop();
                            ObjectType left_type = left.type();
                            ObjectType index_type = index.type();
                            const char* left_type_name = object_get_type_name(left_type);
                            const char* index_type_name = object_get_type_name(index_type);

                            if(left_type != APE_OBJECT_ARRAY && left_type != APE_OBJECT_MAP)
                            {
                                m_errors->addFormat( APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                  "Type %s is not indexable (in OPCODE_SET_INDEX)", left_type_name);
                                goto err;
                            }

                            if(left_type == APE_OBJECT_ARRAY)
                            {
                                if(index_type != APE_OBJECT_NUMBER)
                                {
                                    m_errors->addFormat( APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                      "Cannot index %s with %s", left_type_name, index_type_name);
                                    goto err;
                                }
                                int ix = (int)object_get_number(index);
                                ok = object_set_array_value_at(left, ix, new_value);
                                if(!ok)
                                {
                                    m_errors->addError(APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                     "Setting array item failed (out of bounds?)");
                                    goto err;
                                }
                            }
                            else if(left_type == APE_OBJECT_MAP)
                            {
                                Object old_value = object_get_map_value_object(left, index);
                                if(!this->checkAssign(old_value, new_value))
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
                            Object val = this->stackGet(0);
                            this->stackPush(val);
                        }
                        break;
                    case OPCODE_LEN:
                        {
                            Object val = this->stackPop();
                            int len = 0;
                            ObjectType type = val.type();
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
                                m_errors->addFormat( APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                  "Cannot get length of %s", type_name);
                                goto err;
                            }
                            this->stackPush(Object::makeNumber(len));
                        }
                        break;

                    case OPCODE_NUMBER:
                        {
                            uint64_t val = frame_read_uint64(this->current_frame);
                            double val_double = ape_uint64_to_double(val);
                            Object obj = Object::makeNumber(val_double);
                            this->stackPush(obj);
                        }
                        break;
                    case OPCODE_SET_RECOVER:
                        {
                            uint16_t recover_ip = frame_read_uint16(this->current_frame);
                            this->current_frame->recover_ip = recover_ip;
                        }
                        break;
                    default:
                        {
                            APE_ASSERT(false);
                            m_errors->addFormat( APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "Unknown opcode: 0x%x", opcode);
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
                            m_errors->addFormat(APE_ERROR_TIMEOUT, frame_src_position(this->current_frame),
                                              "Execution took more than %1.17g ms", max_exec_time_ms);
                            goto err;
                        }
                        time_check_counter = 0;
                    }
                }
            err:
                if(m_errors->count() > 0)
                {
                    ErrorList::Error* err = m_errors->getLast();
                    if(err->type == APE_ERROR_RUNTIME && m_errors->count() == 1)
                    {
                        int recover_frame_ix = -1;
                        for(int i = this->frames_count - 1; i >= 0; i--)
                        {
                            Frame* frame = &this->frames[i];
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
                                err->traceback = traceback_make(m_alloc);
                            }
                            if(err->traceback)
                            {
                                traceback_append_from_vm(err->traceback, this);
                            }
                            while(this->frames_count > (recover_frame_ix + 1))
                            {
                                this->popFrame();
                            }
                            Object err_obj = Object::makeError(this->mem, err->message);
                            if(!err_obj.isNull())
                            {
                                object_set_error_traceback(err_obj, err->traceback);
                                err->traceback = NULL;
                            }
                            this->stackPush(err_obj);
                            this->current_frame->ip = this->current_frame->recover_ip;
                            this->current_frame->is_recovering = true;
                            m_errors->clear();
                        }
                    }
                    else
                    {
                        goto end;
                    }
                }
                if(this->mem->shouldSweep())
                {
                    this->runGarbageCollector(constants);
                }
            }

        end:
            if(m_errors->count() > 0)
            {
                ErrorList::Error* err = m_errors->getLast();
                if(!err->traceback)
                {
                    err->traceback = traceback_make(m_alloc);
                }
                if(err->traceback)
                {
                    traceback_append_from_vm(err->traceback, this);
                }
            }

            this->runGarbageCollector(constants);

            this->running = false;
            return m_errors->count() == 0;
        }

};



struct StringBuffer: public Allocator::Allocated
{
    public:
        static StringBuffer* make(Allocator* alloc)
        {
            return make(alloc, 1);
        }

        static StringBuffer* make(Allocator* alloc, unsigned int capacity)
        {
            StringBuffer* buf = alloc->allocate<StringBuffer>();
            if(buf == NULL)
            {
                return NULL;
            }
            memset(buf, 0, sizeof(StringBuffer));
            buf->m_alloc = alloc;
            buf->m_failed = false;
            buf->stringdata = (char*)alloc->allocate(capacity);
            if(buf->stringdata == NULL)
            {
                alloc->release(buf);
                return NULL;
            }
            buf->m_capacity = capacity;
            buf->len = 0;
            buf->stringdata[0] = '\0';
            return buf;
        }

    public:
        bool m_failed;
        char* stringdata;
        size_t m_capacity;
        size_t len;

    public:
        void destroy()
        {
            if(this == NULL)
            {
                return;
            }
            m_alloc->release(this->stringdata);
            m_alloc->release(this);
        }

        void clear()
        {
            if(m_failed)
            {
                return;
            }
            this->len = 0;
            this->stringdata[0] = '\0';
        }

        bool append(const char* str)
        {
            if(m_failed)
            {
                return false;
            }
            size_t str_len = strlen(str);
            if(str_len == 0)
            {
                return true;
            }
            size_t required_capacity = this->len + str_len + 1;
            if(required_capacity > m_capacity)
            {
                bool ok = this->grow(required_capacity * 2);
                if(!ok)
                {
                    return false;
                }
            }
            memcpy(this->stringdata + this->len, str, str_len);
            this->len = this->len + str_len;
            this->stringdata[this->len] = '\0';
            return true;
        }

        bool appendFormat(const char* fmt, ...)
        {
            if(m_failed)
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
            size_t required_capacity = this->len + to_write + 1;
            if(required_capacity > m_capacity)
            {
                bool ok = this->grow(required_capacity * 2);
                if(!ok)
                {
                    return false;
                }
            }
            va_start(args, fmt);
            int written = vsprintf(this->stringdata + this->len, fmt, args);
            (void)written;
            va_end(args);
            if(written != to_write)
            {
                return false;
            }
            this->len = this->len + to_write;
            this->stringdata[this->len] = '\0';
            return true;
        }

        const char* string() const
        {
            if(m_failed)
            {
                return NULL;
            }
            return this->stringdata;
        }

        size_t length() const
        {
            if(m_failed)
            {
                return 0;
            }
            return this->len;
        }

        char* getStringAndDestroy()
        {
            if(m_failed)
            {
                this->destroy();
                return NULL;
            }
            char* res = this->stringdata;
            this->stringdata = NULL;
            this->destroy();
            return res;
        }

        bool failed()
        {
            return m_failed;
        }

        bool grow(size_t new_capacity)
        {
            char* new_data = (char*)m_alloc->allocate(new_capacity);
            if(new_data == NULL)
            {
                m_failed = true;
                return false;
            }
            memcpy(new_data, this->stringdata, this->len);
            new_data[this->len] = '\0';
            m_alloc->release(this->stringdata);
            this->stringdata = new_data;
            m_capacity = new_capacity;
            return true;
        }

};


struct Symbol: public Allocator::Allocated
{
    public:
        struct Table: public Allocator::Allocated
        {
            public:
                static Table* make(Allocator* alloc, Table* outer, GlobalStore* global_store, int module_global_offset)
                {
                    bool ok;
                    Table* table;
                    table = alloc->allocate<Table>();
                    if(!table)
                    {
                        return NULL;
                    }
                    memset(table, 0, sizeof(Table));
                    table->m_alloc = alloc;
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
                    ok = table->pushBlockScope();
                    if(!ok)
                    {
                        goto err;
                    }
                    return table;
                err:
                    table->destroy();
                    return NULL;
                }

            public:
                Table* outer;
                GlobalStore* global_store;
                PtrArray * block_scopes;
                PtrArray * free_symbols;
                PtrArray * module_global_symbols;
                int max_num_definitions;
                int module_global_offset;

            public:
                void destroy()
                {
                    Allocator* alloc;
                    while(this->block_scopes->count() > 0)
                    {
                        this->popBlockScope();
                    }
                    this->block_scopes->destroy();
                    this->module_global_symbols->destroyWithItems(Symbol::callback_destroy);
                    this->free_symbols->destroyWithItems(Symbol::callback_destroy);
                    alloc = m_alloc;
                    memset(this, 0, sizeof(Table));
                    alloc->release(this);
                }

                Table* copy()
                {
                    Table* copy;
                    copy = m_alloc->allocate<Table>();
                    if(!copy)
                    {
                        return NULL;
                    }
                    memset(copy, 0, sizeof(Table));
                    copy->m_alloc = m_alloc;
                    copy->outer = this->outer;
                    copy->global_store = this->global_store;
                    copy->block_scopes = this->block_scopes->copyWithItems(block_scope_copy, block_scope_destroy);
                    if(!copy->block_scopes)
                    {
                        goto err;
                    }
                    copy->free_symbols = this->free_symbols->copyWithItems(Symbol::callback_copy, Symbol::callback_destroy);
                    if(!copy->free_symbols)
                    {
                        goto err;
                    }
                    copy->module_global_symbols = this->module_global_symbols->copyWithItems(Symbol::callback_copy, Symbol::callback_destroy);
                    if(!copy->module_global_symbols)
                    {
                        goto err;
                    }
                    copy->max_num_definitions = this->max_num_definitions;
                    copy->module_global_offset = this->module_global_offset;
                    return copy;
                err:
                    copy->destroy();
                    return NULL;
                }

                bool set(Symbol* symbol)
                {
                    BlockScope* top_scope;
                    Symbol* existing;
                    top_scope = (BlockScope*)this->block_scopes->top();
                    existing= (Symbol*)top_scope->store->get(symbol->name);
                    if(existing)
                    {
                        existing->destroy();
                    }
                    return top_scope->store->set(symbol->name, symbol);
                }

                int nextIndex()
                {
                    int ix;
                    BlockScope* top_scope;
                    top_scope = (BlockScope*)this->block_scopes->top();
                    ix = top_scope->offset + top_scope->num_definitions;
                    return ix;
                }

                int defCount()
                {
                    int i;
                    int cn;
                    BlockScope* scope;
                    cn = 0;
                    for(i = this->block_scopes->count() - 1; i >= 0; i--)
                    {
                        scope = (BlockScope*)this->block_scopes->get(i);
                        cn += scope->num_definitions;
                    }
                    return cn;
                }

                bool addModuleSymbol(Symbol* symbol)
                {
                    bool ok;
                    if(symbol->type != SYMBOL_MODULE_GLOBAL)
                    {
                        APE_ASSERT(false);
                        return false;
                    }
                    if(this->isDefined(symbol->name))
                    {
                        return true;// todo: make sure it should be true in this case
                    }
                    Symbol* copy = symbol->copy();
                    if(!copy)
                    {
                        return false;
                    }
                    ok = this->set(copy);
                    if(!ok)
                    {
                        copy->destroy();
                        return false;
                    }
                    return true;
                }

                const Symbol* define(const char* name, bool assignable)
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
                    global_symbol = global_store_get_symbol(this->global_store, name);
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
                    symbol_type = this->outer == NULL ? SYMBOL_MODULE_GLOBAL : SYMBOL_LOCAL;
                    ix = this->nextIndex();
                    symbol = Symbol::make(m_alloc, name, symbol_type, ix, assignable);
                    if(!symbol)
                    {
                        return NULL;
                    }
                    global_symbol_added = false;
                    ok = false;
                    if(symbol_type == SYMBOL_MODULE_GLOBAL && this->block_scopes->count() == 1)
                    {
                        global_symbol_copy = symbol->copy();
                        if(!global_symbol_copy)
                        {
                            symbol->destroy();
                            return NULL;
                        }
                        ok = this->module_global_symbols->add(global_symbol_copy);
                        if(!ok)
                        {
                            global_symbol_copy->destroy();
                            symbol->destroy();
                            return NULL;
                        }
                        global_symbol_added = true;
                    }

                    ok = this->set(symbol);
                    if(!ok)
                    {
                        if(global_symbol_added)
                        {
                            global_symbol_copy = (Symbol*)this->module_global_symbols->pop();
                            global_symbol_copy->destroy();
                        }
                        symbol->destroy();
                        return NULL;
                    }
                    top_scope = (BlockScope*)this->block_scopes->top();
                    top_scope->num_definitions++;
                    definitions_count = this->defCount();
                    if(definitions_count > this->max_num_definitions)
                    {
                        this->max_num_definitions = definitions_count;
                    }

                    return symbol;
                }

                const Symbol* defineFree(const Symbol* original)
                {
                    bool ok;
                    Symbol* symbol;
                    Symbol* copy;
                    copy = Symbol::make(m_alloc, original->name, original->type, original->index, original->assignable);
                    if(!copy)
                    {
                        return NULL;
                    }
                    ok = this->free_symbols->add(copy);
                    if(!ok)
                    {
                        copy->destroy();
                        return NULL;
                    }

                    symbol = Symbol::make(m_alloc, original->name, SYMBOL_FREE, this->free_symbols->count() - 1, original->assignable);
                    if(!symbol)
                    {
                        return NULL;
                    }

                    ok = this->set(symbol);
                    if(!ok)
                    {
                        symbol->destroy();
                        return NULL;
                    }

                    return symbol;
                }

                const Symbol* defineFunctionName(const char* name, bool assignable)
                {
                    bool ok;
                    Symbol* symbol;
                    if(strchr(name, ':'))
                    {
                        return NULL;// module symbol
                    }
                    symbol = Symbol::make(m_alloc, name, SYMBOL_FUNCTION, 0, assignable);
                    if(!symbol)
                    {
                        return NULL;
                    }
                    ok = this->set(symbol);
                    if(!ok)
                    {
                        symbol->destroy();
                        return NULL;
                    }

                    return symbol;
                }

                const Symbol* defineThis()
                {
                    bool ok;
                    Symbol* symbol;
                    symbol = Symbol::make(m_alloc, "this", SYMBOL_THIS, 0, false);
                    if(!symbol)
                    {
                        return NULL;
                    }
                    ok = this->set(symbol);
                    if(!ok)
                    {
                        symbol->destroy();
                        return NULL;
                    }
                    return symbol;
                }

                const Symbol* resolve(const char* name)
                {
                    int i;
                    const Symbol* symbol;
                    BlockScope* scope;
                    scope = NULL;
                    symbol = global_store_get_symbol(this->global_store, name);
                    if(symbol)
                    {
                        return symbol;
                    }
                    for(i = this->block_scopes->count() - 1; i >= 0; i--)
                    {
                        scope = (BlockScope*)this->block_scopes->get(i);
                        symbol = (Symbol*)scope->store->get(name);
                        if(symbol)
                        {
                            break;
                        }
                    }
                    if(symbol && symbol->type == SYMBOL_THIS)
                    {
                        symbol = this->defineFree(symbol);
                    }

                    if(!symbol && this->outer)
                    {
                        symbol = this->outer->resolve(name);
                        if(!symbol)
                        {
                            return NULL;
                        }
                        if(symbol->type == SYMBOL_MODULE_GLOBAL || symbol->type == SYMBOL_APE_GLOBAL)
                        {
                            return symbol;
                        }
                        symbol = this->defineFree(symbol);
                    }
                    return symbol;
                }

                bool isDefined(const char* name)
                {
                    BlockScope* top_scope;
                    const Symbol* symbol;
                    // todo: rename to something more obvious
                    symbol = global_store_get_symbol(this->global_store, name);
                    if(symbol)
                    {
                        return true;
                    }
                    top_scope = (BlockScope*)this->block_scopes->top();
                    symbol = (Symbol*)top_scope->store->get(name);
                    if(symbol)
                    {
                        return true;
                    }
                    return false;
                }

                bool pushBlockScope()
                {
                    bool ok;
                    int block_scope_offset;
                    BlockScope* prev_block_scope;
                    BlockScope* new_scope;
                    block_scope_offset = 0;
                    prev_block_scope = (BlockScope*)this->block_scopes->top();
                    if(prev_block_scope)
                    {
                        block_scope_offset = this->module_global_offset + prev_block_scope->offset + prev_block_scope->num_definitions;
                    }
                    else
                    {
                        block_scope_offset = this->module_global_offset;
                    }

                    new_scope = block_scope_make(m_alloc, block_scope_offset);
                    if(!new_scope)
                    {
                        return false;
                    }
                    ok = this->block_scopes->push(new_scope);
                    if(!ok)
                    {
                        block_scope_destroy(new_scope);
                        return false;
                    }
                    return true;
                }

                void popBlockScope()
                {
                    BlockScope* top_scope;
                    top_scope = (BlockScope*)this->block_scopes->top();
                    this->block_scopes->pop();
                    block_scope_destroy(top_scope);
                }

                BlockScope* getBlockScope()
                {
                    BlockScope* top_scope;
                    top_scope = (BlockScope*)this->block_scopes->top();
                    return top_scope;
                }

                bool isModuleGlobalScope()
                {
                    return this->outer == NULL;
                }

                bool isTopBlockScope()
                {
                    return this->block_scopes->count() == 1;
                }

                bool isTopGlobalScope()
                {
                    return this->isModuleGlobalScope() && this->isTopBlockScope();
                }

                int getModuleGlobalSymbolCount()
                {
                    return this->module_global_symbols->count();
                }

                const Symbol* getModuleGlobalSymbolAt(int ix)
                {
                    return (Symbol*)this->module_global_symbols->get(ix);
                }


        };



    public:
        static Symbol* make(Allocator* alloc, const char* name, SymbolType type, int index, bool assignable)
        {
            Symbol* symbol;
            symbol = alloc->allocate<Symbol>();
            if(!symbol)
            {
                return NULL;
            }
            memset(symbol, 0, sizeof(Symbol));
            symbol->m_alloc = alloc;
            symbol->name = ape_strdup(alloc, name);
            if(!symbol->name)
            {
                alloc->release(symbol);
                return NULL;
            }
            symbol->type = type;
            symbol->index = index;
            symbol->assignable = assignable;
            return symbol;
        }

        static void callback_destroy(Symbol* sym)
        {
            sym->destroy();
        }

        static Symbol* callback_copy(Symbol* sym)
        {
            return sym->copy();
        }

    public:
        SymbolType type;
        char* name;
        int index;
        bool assignable;

    public:
        void destroy()
        {
            m_alloc->release(this->name);
            m_alloc->release(this);
        }

        Symbol* copy()
        {
            return Symbol::make(m_alloc, this->name, this->type, this->index, this->assignable);
        }
};

struct GlobalStore: public Allocator::Allocated
{
    Dictionary * symbols;
    Array * objects;
};

struct Module: public Allocator::Allocated
{
    char* name;
    PtrArray * symbols;
};

struct FileScope: public Allocator::Allocated
{
    public:
        Parser* parser;
        Symbol::Table* symbol_table;
        CompiledFile* file;
        PtrArray * loaded_module_names;

    public:
        void destroy()
        {
            for(int i = 0; i < this->loaded_module_names->count(); i++)
            {
                void* name = this->loaded_module_names->get(i);
                m_alloc->release(name);
            }
            this->loaded_module_names->destroy();
            this->parser->destroy();
            m_alloc->release(this);
        }

};


struct Compiler: public Allocator::Allocated
{
    public:
        static Compiler* make(Allocator* alloc, const Config* config, GCMemory* mem, ErrorList* errors, PtrArray * files, GlobalStore* global_store)
        {
            bool ok;
            Compiler* comp;
            comp = alloc->allocate<Compiler>();
            if(!comp)
            {
                return NULL;
            }
            ok = comp->init(alloc, config, mem, errors, files, global_store);
            if(!ok)
            {
                alloc->release(comp);
                return NULL;
            }
            return comp;
        }

        static bool makeShallowCopy(Compiler* copy, Compiler* src)
        {
            int i;
            bool ok;
            int* val;
            int* val_copy;
            const char* key;
            const char* loaded_name;
            char* loaded_name_copy;
            Symbol::Table* src_st;
            Symbol::Table* src_st_copy;
            Symbol::Table* copy_st;
            Dictionary* modules_copy;
            Array* constants_copy;
            FileScope* src_file_scope;
            FileScope* copy_file_scope;
            PtrArray* src_loaded_module_names;
            PtrArray* copy_loaded_module_names;

            ok = copy->init(src->m_alloc, src->config, src->mem, src->m_errors, src->files, src->global_store);
            if(!ok)
            {
                return false;
            }
            src_st = src->getSymbolTable();
            APE_ASSERT(src->file_scopes->count() == 1);
            APE_ASSERT(src_st->outer == NULL);
            src_st_copy = src_st->copy();
            if(!src_st_copy)
            {
                goto err;
            }
            copy_st = copy->getSymbolTable();
            copy_st->destroy();
            copy_st = NULL;
            copy->setSymbolTable(src_st_copy);
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
                val_copy = src->m_alloc->allocate<int>();
                if(!val_copy)
                {
                    goto err;
                }
                *val_copy = *val;
                ok = copy->string_constants_positions->set(key, val_copy);
                if(!ok)
                {
                    src->m_alloc->release(val_copy);
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
                loaded_name_copy = ape_strdup(copy->m_alloc, loaded_name);
                if(!loaded_name_copy)
                {
                    goto err;
                }
                ok = copy_loaded_module_names->add(loaded_name_copy);
                if(!ok)
                {
                    copy->m_alloc->release(loaded_name_copy);
                    goto err;
                }
            }

            return true;
        err:
            copy->deinit();
            return false;
        }

    public:
        const Config* config;
        GCMemory* mem;
        ErrorList* m_errors;
        PtrArray * files;
        GlobalStore* global_store;
        Array * constants;
        CompilationScope* compilation_scope;
        PtrArray * file_scopes;
        Array * src_positions_stack;
        Dictionary * modules;
        Dictionary * string_constants_positions;

    public:
        const Symbol* define(Position pos, const char* name, bool assignable, bool can_shadow)
        {
            Symbol::Table* symbol_table;
            const Symbol* current_symbol;
            const Symbol* symbol;
            symbol_table = this->getSymbolTable();
            if(!can_shadow && !symbol_table->isTopGlobalScope())
            {
                current_symbol = symbol_table->resolve(name);
                if(current_symbol)
                {
                    m_errors->addFormat(APE_ERROR_COMPILATION, pos, "Symbol \"%s\" is already defined", name);
                    return NULL;
                }
            }
            symbol = symbol_table->define(name, assignable);
            if(!symbol)
            {
                m_errors->addFormat(APE_ERROR_COMPILATION, pos, "Cannot define symbol \"%s\"", name);
                return NULL;
            }
            return symbol;
        }

        void destroy()
        {
            Allocator* alloc;
            if(!this)
            {
                return;
            }
            alloc = m_alloc;
            this->deinit();
            alloc->release(this);
        }

        template<typename... ArgsT>
        int emit(opcode_t op, int operands_count, ArgsT&&... operargs)
        {
            int i;
            int ip;
            int len;
            bool ok;
            Position* src_pos;
            CompilationScope* compilation_scope;
            uint64_t operands[] = { static_cast<uint64_t>(operargs)... };
            ip = this->getIP();
            len = code_make(op, operands_count, operands, this->getBytecode());
            if(len == 0)
            {
                return -1;
            }
            for(i = 0; i < len; i++)
            {
                src_pos = (Position*)this->src_positions_stack->top();
                APE_ASSERT(src_pos->line >= 0);
                APE_ASSERT(src_pos->column >= 0);
                ok = this->getSourcePositions()->add(src_pos);
                if(!ok)
                {
                    return -1;
                }
            }
            compilation_scope = this->getCompilationScope();
            compilation_scope->last_opcode = op;
            return ip;
        }

        CompilationResult* compileSource(const char* code)
        {
            bool ok;
            Compiler comp_shallow_copy;
            CompilationScope* compilation_scope;
            CompilationResult* res;
            compilation_scope = this->getCompilationScope();
            APE_ASSERT(this->src_positions_stack->count() == 0);
            APE_ASSERT(compilation_scope->bytecode->count() == 0);
            APE_ASSERT(compilation_scope->break_ip_stack->count() == 0);
            APE_ASSERT(compilation_scope->continue_ip_stack->count() == 0);
            this->src_positions_stack->clear();
            compilation_scope->bytecode->clear();
            compilation_scope->src_positions->clear();
            compilation_scope->break_ip_stack->clear();
            compilation_scope->continue_ip_stack->clear();
            ok = Compiler::makeShallowCopy(&comp_shallow_copy, this);
            if(!ok)
            {
                return NULL;
            }
            ok = this->compileCode(code);
            if(!ok)
            {
                goto err;
            }
            compilation_scope = this->getCompilationScope();// might've changed
            APE_ASSERT(compilation_scope->outer == NULL);
            compilation_scope = this->getCompilationScope();
            res = compilation_scope_orphan_result(compilation_scope);
            if(!res)
            {
                goto err;
            }
            comp_shallow_copy.deinit();
            return res;
        err:
            this->deinit();
            *this = comp_shallow_copy;
            return NULL;
        }

        CompilationResult* compileFile(const char* path)
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
            if(!this->config->fileio.read_file.read_file)
            {// todo: read code function
                m_errors->addError(APE_ERROR_COMPILATION, Position::invalid(), "File read function not configured");
                goto err;
            }
            code = this->config->fileio.read_file.read_file(this->config->fileio.read_file.context, path);
            if(!code)
            {
                m_errors->addFormat( APE_ERROR_COMPILATION, Position::invalid(), "Reading file \"%s\" failed", path);
                goto err;
            }
            file = compiled_file_make(m_alloc, path);
            if(!file)
            {
                goto err;
            }
            ok = this->files->add(file);
            if(!ok)
            {
                compiled_file_destroy(file);
                goto err;
            }
            APE_ASSERT(this->file_scopes->count() == 1);
            file_scope = (FileScope*)this->file_scopes->top();
            if(!file_scope)
            {
                goto err;
            }
            prev_file = file_scope->file;// todo: push file scope instead?
            file_scope->file = file;
            res = this->compileSource(code);
            if(!res)
            {
                file_scope->file = prev_file;
                goto err;
            }
            file_scope->file = prev_file;
            m_alloc->release(code);
            return res;
        err:
            m_alloc->release(code);
            return NULL;
        }

        Symbol::Table* getSymbolTable()
        {
            FileScope* file_scope;
            file_scope = (FileScope*)this->file_scopes->top();
            if(!file_scope)
            {
                APE_ASSERT(false);
                return NULL;
            }
            return file_scope->symbol_table;
        }

        void setSymbolTable(Symbol::Table* table)
        {
            FileScope* file_scope;
            file_scope = (FileScope*)this->file_scopes->top();
            if(!file_scope)
            {
                APE_ASSERT(false);
                return;
            }
            file_scope->symbol_table = table;
        }

        Array* getConstants()
        {
            return this->constants;
        }


        bool init(Allocator* alloc,
                                  const Config* config,
                                  GCMemory* mem,
                                  ErrorList* errors,
                                  PtrArray * files,
                                  GlobalStore* global_store)
        {
            bool ok;
            memset(this, 0, sizeof(Compiler));
            m_alloc = alloc;
            this->config = config;
            this->mem = mem;
            m_errors = errors;
            this->files = files;
            this->global_store = global_store;
            this->file_scopes = PtrArray::make(alloc);
            if(!this->file_scopes)
            {
                goto err;
            }
            this->constants = Array::make<Object>(alloc);
            if(!this->constants)
            {
                goto err;
            }
            this->src_positions_stack = Array::make<Position>(alloc);
            if(!this->src_positions_stack)
            {
                goto err;
            }
            this->modules = Dictionary::make(alloc, module_copy, module_destroy);
            if(!this->modules)
            {
                goto err;
            }
            ok = this->pushCompilationScope();
            if(!ok)
            {
                goto err;
            }
            ok = this->pushFileScope("none");
            if(!ok)
            {
                goto err;
            }
            this->string_constants_positions = Dictionary::make(m_alloc, NULL, NULL);
            if(!this->string_constants_positions)
            {
                goto err;
            }

            return true;
        err:
            this->deinit();
            return false;
        }

        void deinit()
        {
            int i;
            int* val;
            if(!this)
            {
                return;
            }
            for(i = 0; i < this->string_constants_positions->count(); i++)
            {
                val = (int*)this->string_constants_positions->getValueAt(i);
                m_alloc->release(val);
            }
            this->string_constants_positions->destroy();
            while(this->file_scopes->count() > 0)
            {
                this->popFileScope();
            }
            while(this->getCompilationScope())
            {
                this->popCompilationScope();
            }
            this->modules->destroyWithItems();
            Array::destroy(this->src_positions_stack);
            Array::destroy(this->constants);
            this->file_scopes->destroy();
            memset(this, 0, sizeof(Compiler));
        }

        CompilationScope* getCompilationScope()
        {
            return this->compilation_scope;
        }

        bool pushCompilationScope()
        {
            CompilationScope* current_scope;
            CompilationScope* new_scope;
            current_scope = this->getCompilationScope();
            new_scope = compilation_scope_make(m_alloc, current_scope);
            if(!new_scope)
            {
                return false;
            }
            this->setCompilationScope(new_scope);
            return true;
        }

        void popCompilationScope()
        {
            CompilationScope* current_scope;
            current_scope = this->getCompilationScope();
            APE_ASSERT(current_scope);
            this->setCompilationScope(current_scope->outer);
            compilation_scope_destroy(current_scope);
        }

        bool pushSymbolTable(int global_offset)
        {
            FileScope* file_scope;
            file_scope = (FileScope*)this->file_scopes->top();
            if(!file_scope)
            {
                APE_ASSERT(false);
                return false;
            }
            Symbol::Table* current_table = file_scope->symbol_table;
            file_scope->symbol_table = Symbol::Table::make(m_alloc, current_table, this->global_store, global_offset);
            if(!file_scope->symbol_table)
            {
                file_scope->symbol_table = current_table;
                return false;
            }
            return true;
        }

        void popSymbolTable()
        {
            FileScope* file_scope;
            Symbol::Table* current_table;
            file_scope = (FileScope*)this->file_scopes->top();
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
            current_table->destroy();
        }

        opcode_t getLastOpcode()
        {
            CompilationScope* current_scope;
            current_scope = this->getCompilationScope();
            return current_scope->last_opcode;
        }

        bool compileCode(const char* code)
        {
            bool ok;
            StringBuffer *buf;

            FileScope* file_scope;
            PtrArray* statements;
            buf = StringBuffer::make(NULL);
            file_scope = (FileScope*)this->file_scopes->top();
            APE_ASSERT(file_scope);
            statements = file_scope->parser->parseAll(code, file_scope->file);
            if(!statements)
            {
                // errors are added by parser
                return false;
            }
            ok = this->compileStatements(statements);
            statements_to_string(buf, statements);            
            puts(buf->string());
            statements->destroyWithItems(Expression::destroyExpression);

            // Left for debugging purposes
            #if 0
            if (ok)
            {

                buf->clear();
                code_to_string(
                    (uint8_t*)this->compilation_scope->bytecode->data(),
                    (Position*)this->compilation_scope->src_positions->data(),
                    this->compilation_scope->bytecode->count(), buf);
                puts(buf->string());
            }
            #endif
            buf->destroy();
            return ok;
        }

        bool compileStatements(PtrArray * statements)
        {
            int i;
            bool ok;
            const Expression* stmt;
            ok = true;
            for(i = 0; i < statements->count(); i++)
            {
                stmt = (const Expression*)statements->get(i);
                ok = this->compileStatement(stmt);
                if(!ok)
                {
                    break;
                }
            }
            return ok;
        }

        bool importModule(const Expression* import_stmt)
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
            Symbol::Table* symbol_table;
            FileScope* fs;
            Module* module;
            Symbol::Table* st;
            Symbol* symbol;
            result = false;
            filepath = NULL;
            code = NULL;
            file_scope = (FileScope*)this->file_scopes->top();
            module_path = import_stmt->import.path;
            module_name = get_module_name(module_path);
            for(i = 0; i < file_scope->loaded_module_names->count(); i++)
            {
                loaded_name = (const char*)file_scope->loaded_module_names->get(i);
                if(kg_streq(loaded_name, module_name))
                {
                    m_errors->addFormat(APE_ERROR_COMPILATION, import_stmt->pos, "Module \"%s\" was already imported", module_name);
                    result = false;
                    goto end;
                }
            }
            filepath_buf = StringBuffer::make(m_alloc);
            if(!filepath_buf)
            {
                result = false;
                goto end;
            }
            if(kg_is_path_absolute(module_path))
            {
                filepath_buf->appendFormat("%s.ape", module_path);
            }
            else
            {
                filepath_buf->appendFormat("%s%s.ape", file_scope->file->dir_path, module_path);
            }
            if(filepath_buf->failed())
            {
                filepath_buf->destroy();
                result = false;
                goto end;
            }
            filepath_non_canonicalised = filepath_buf->string();
            filepath = kg_canonicalise_path(m_alloc, filepath_non_canonicalised);
            filepath_buf->destroy();
            if(!filepath)
            {
                result = false;
                goto end;
            }
            symbol_table = this->getSymbolTable();
            if(symbol_table->outer != NULL || symbol_table->block_scopes->count() > 1)
            {
                m_errors->addError(APE_ERROR_COMPILATION, import_stmt->pos, "Modules can only be imported in global scope");
                result = false;
                goto end;
            }
            for(i = 0; i < this->file_scopes->count(); i++)
            {
                fs = (FileScope*)this->file_scopes->get(i);
                if(APE_STREQ(fs->file->path, filepath))
                {
                    m_errors->addFormat(APE_ERROR_COMPILATION, import_stmt->pos, "Cyclic reference of file \"%s\"", filepath);
                    result = false;
                    goto end;
                }
            }
            module = (Module*)this->modules->get(filepath);
            if(!module)
            {
                // todo: create new module function
                if(!this->config->fileio.read_file.read_file)
                {
                    m_errors->addFormat( APE_ERROR_COMPILATION, import_stmt->pos,
                                      "Cannot import module \"%s\", file read function not configured", filepath);
                    result = false;
                    goto end;
                }
                code = this->config->fileio.read_file.read_file(this->config->fileio.read_file.context, filepath);
                if(!code)
                {
                    m_errors->addFormat( APE_ERROR_COMPILATION, import_stmt->pos, "Reading module file \"%s\" failed", filepath);
                    result = false;
                    goto end;
                }
                module = module_make(m_alloc, module_name);
                if(!module)
                {
                    result = false;
                    goto end;
                }
                ok = this->pushFileScope(filepath);
                if(!ok)
                {
                    module_destroy(module);
                    result = false;
                    goto end;
                }
                ok = this->compileCode(code);
                if(!ok)
                {
                    module_destroy(module);
                    result = false;
                    goto end;
                }
                st = this->getSymbolTable();
                for(i = 0; i < st->getModuleGlobalSymbolCount(); i++)
                {
                    symbol = (Symbol*)st->getModuleGlobalSymbolAt(i);
                    module_add_symbol(module, symbol);
                }
                this->popFileScope();
                ok = this->modules->set( filepath, module);
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
                ok = symbol_table->addModuleSymbol(symbol);
                if(!ok)
                {
                    result = false;
                    goto end;
                }
            }
            name_copy = ape_strdup(m_alloc, module_name);
            if(!name_copy)
            {
                result = false;
                goto end;
            }
            ok = file_scope->loaded_module_names->add(name_copy);
            if(!ok)
            {
                m_alloc->release(name_copy);
                result = false;
                goto end;
            }
            result = true;
        end:
            m_alloc->release(filepath);
            m_alloc->release(code);
            return result;
        }

        bool compileStatement(const Expression* stmt)
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

            const StmtWhileLoop* loop;
            CompilationScope* compilation_scope;
            Symbol::Table* symbol_table;
            const Symbol* symbol;
            const StmtIfClause* if_stmt;
            Array* jump_to_end_ips;
            StmtIfClause::Case* if_case;
            ok = false;
            ip = -1;
            ok = this->src_positions_stack->push(&stmt->pos);
            if(!ok)
            {
                return false;
            }
            compilation_scope = this->getCompilationScope();
            symbol_table = this->getSymbolTable();
            switch(stmt->type)
            {
                case EXPRESSION_EXPRESSION:
                    {
                        ok = this->compileExpression(stmt->expression);
                        if(!ok)
                        {
                            return false;
                        }
                        ip = this->emit(OPCODE_POP, 0);
                        if(ip < 0)
                        {
                            return false;
                        }
                    }
                    break;

                case EXPRESSION_DEFINE:
                    {
                        ok = this->compileExpression(stmt->define.value);
                        if(!ok)
                        {
                            return false;
                        }
                        symbol = this->define(stmt->define.name->pos, stmt->define.name->value, stmt->define.assignable, false);
                        if(!symbol)
                        {
                            return false;
                        }
                        ok = this->writeSymbol(symbol, true);
                        if(!ok)
                        {
                            return false;
                        }
                    }
                    break;

                case EXPRESSION_IF:
                    {
                        if_stmt = &stmt->if_statement;
                        jump_to_end_ips = Array::make<int>(m_alloc);
                        if(!jump_to_end_ips)
                        {
                            goto statement_if_error;
                        }
                        for(i = 0; i < if_stmt->cases->count(); i++)
                        {
                            if_case = (StmtIfClause::Case*)if_stmt->cases->get(i);
                            ok = this->compileExpression(if_case->test);
                            if(!ok)
                            {
                                goto statement_if_error;
                            }
                            next_case_jump_ip = this->emit(OPCODE_JUMP_IF_FALSE, 1, (uint64_t)(0xbeef));
                            ok = this->compileCodeBlock(if_case->consequence);
                            if(!ok)
                            {
                                goto statement_if_error;
                            }
                            // don't emit jump for the last statement
                            if(i < (if_stmt->cases->count() - 1) || if_stmt->alternative)
                            {

                                jump_to_end_ip = this->emit(OPCODE_JUMP, 1, (uint64_t)(0xbeef));
                                ok = jump_to_end_ips->add(&jump_to_end_ip);
                                if(!ok)
                                {
                                    goto statement_if_error;
                                }
                            }
                            after_elif_ip = this->getIP();
                            this->changeUint16Operand(next_case_jump_ip + 1, after_elif_ip);
                        }
                        if(if_stmt->alternative)
                        {
                            ok = this->compileCodeBlock(if_stmt->alternative);
                            if(!ok)
                            {
                                goto statement_if_error;
                            }
                        }
                        after_alt_ip = this->getIP();
                        for(i = 0; i < jump_to_end_ips->count(); i++)
                        {
                            pos = (int*)jump_to_end_ips->get(i);
                            this->changeUint16Operand(*pos + 1, after_alt_ip);
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
                            m_errors->addFormat( APE_ERROR_COMPILATION, stmt->pos, "Nothing to return from");
                            return false;
                        }
                        ip = -1;
                        if(stmt->return_value)
                        {
                            ok = this->compileExpression(stmt->return_value);
                            if(!ok)
                            {
                                return false;
                            }
                            ip = this->emit(OPCODE_RETURN_VALUE, 0);
                        }
                        else
                        {
                            ip = this->emit(OPCODE_RETURN, 0);
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
                        before_test_ip = this->getIP();
                        ok = this->compileExpression(loop->test);
                        if(!ok)
                        {
                            return false;
                        }
                        after_test_ip = this->getIP();
                        ip = this->emit(OPCODE_JUMP_IF_TRUE, 1, (uint64_t)(after_test_ip + 6));
                        if(ip < 0)
                        {
                            return false;
                        }
                        jump_to_after_body_ip = this->emit(OPCODE_JUMP, 1, (uint64_t)0xdead);
                        if(jump_to_after_body_ip < 0)
                        {
                            return false;
                        }
                        ok = this->pushContinueIP(before_test_ip);
                        if(!ok)
                        {
                            return false;
                        }
                        ok = this->pushBreakIP(jump_to_after_body_ip);
                        if(!ok)
                        {
                            return false;
                        }
                        ok = this->compileCodeBlock(loop->body);
                        if(!ok)
                        {
                            return false;
                        }
                        this->popBreakIP();
                        this->popContinueIP();
                        ip = this->emit(OPCODE_JUMP, 1, (uint64_t)before_test_ip);
                        if(ip < 0)
                        {
                            return false;
                        }
                        after_body_ip = this->getIP();
                        this->changeUint16Operand(jump_to_after_body_ip + 1, after_body_ip);
                    }
                    break;

                case EXPRESSION_BREAK:
                {
                    int break_ip = this->getBreakIP();
                    if(break_ip < 0)
                    {
                        m_errors->addFormat(APE_ERROR_COMPILATION, stmt->pos, "Nothing to break from.");
                        return false;
                    }
                    ip = this->emit(OPCODE_JUMP, 1, (uint64_t)break_ip);
                    if(ip < 0)
                    {
                        return false;
                    }
                    break;
                }
                case EXPRESSION_CONTINUE:
                {
                    int continue_ip = this->getContinueIP();
                    if(continue_ip < 0)
                    {
                        m_errors->addFormat( APE_ERROR_COMPILATION, stmt->pos, "Nothing to continue from.");
                        return false;
                    }
                    ip = this->emit(OPCODE_JUMP, 1, (uint64_t)continue_ip);
                    if(ip < 0)
                    {
                        return false;
                    }
                    break;
                }
                case EXPRESSION_FOREACH:
                {
                    const StmtForeach* foreach = &stmt->foreach;
                    ok = symbol_table->pushBlockScope();
                    if(!ok)
                    {
                        return false;
                    }

                    // Init
                    const Symbol* index_symbol = this->define(stmt->pos, "@i", false, true);
                    if(!index_symbol)
                    {
                        return false;
                    }

                    ip = this->emit(OPCODE_NUMBER, 1, (uint64_t)0);
                    if(ip < 0)
                    {
                        return false;
                    }

                    ok = this->writeSymbol(index_symbol, true);
                    if(!ok)
                    {
                        return false;
                    }

                    const Symbol* source_symbol = NULL;
                    if(foreach->source->type == EXPRESSION_IDENT)
                    {
                        source_symbol = symbol_table->resolve(foreach->source->ident->value);
                        if(!source_symbol)
                        {
                            m_errors->addFormat( APE_ERROR_COMPILATION, foreach->source->pos,
                                              "Symbol \"%s\" could not be resolved", foreach->source->ident->value);
                            return false;
                        }
                    }
                    else
                    {
                        ok = this->compileExpression(foreach->source);
                        if(!ok)
                        {
                            return false;
                        }
                        source_symbol = this->define(foreach->source->pos, "@source", false, true);
                        if(!source_symbol)
                        {
                            return false;
                        }
                        ok = this->writeSymbol(source_symbol, true);
                        if(!ok)
                        {
                            return false;
                        }
                    }

                    // Update
                    int jump_to_after_update_ip = this->emit(OPCODE_JUMP, 1, (uint64_t)0xbeef);
                    if(jump_to_after_update_ip < 0)
                    {
                        return false;
                    }

                    int update_ip = this->getIP();
                    ok = this->readSymbol(index_symbol);
                    if(!ok)
                    {
                        return false;
                    }

                    ip = this->emit(OPCODE_NUMBER, 1, (uint64_t)ape_double_to_uint64(1));
                    if(ip < 0)
                    {
                        return false;
                    }

                    ip = this->emit(OPCODE_ADD, 0);
                    if(ip < 0)
                    {
                        return false;
                    }

                    ok = this->writeSymbol(index_symbol, false);
                    if(!ok)
                    {
                        return false;
                    }

                    int after_update_ip = this->getIP();
                    this->changeUint16Operand(jump_to_after_update_ip + 1, after_update_ip);

                    // Test
                    ok = this->src_positions_stack->push(&foreach->source->pos);
                    if(!ok)
                    {
                        return false;
                    }

                    ok = this->readSymbol(source_symbol);
                    if(!ok)
                    {
                        return false;
                    }

                    ip = this->emit(OPCODE_LEN, 0);
                    if(ip < 0)
                    {
                        return false;
                    }

                    this->src_positions_stack->pop(NULL);
                    ok = this->readSymbol(index_symbol);
                    if(!ok)
                    {
                        return false;
                    }

                    ip = this->emit(OPCODE_COMPARE, 0);
                    if(ip < 0)
                    {
                        return false;
                    }

                    ip = this->emit(OPCODE_EQUAL, 0);
                    if(ip < 0)
                    {
                        return false;
                    }

                    int after_test_ip = this->getIP();
                    ip = this->emit(OPCODE_JUMP_IF_FALSE, 1, (uint64_t)(after_test_ip + 6));
                    if(ip < 0)
                    {
                        return false;
                    }

                    int jump_to_after_body_ip = this->emit(OPCODE_JUMP, 1, (uint64_t)0xdead);
                    if(jump_to_after_body_ip < 0)
                    {
                        return false;
                    }

                    ok = this->readSymbol(source_symbol);
                    if(!ok)
                    {
                        return false;
                    }

                    ok = this->readSymbol(index_symbol);
                    if(!ok)
                    {
                        return false;
                    }

                    ip = this->emit(OPCODE_GET_VALUE_AT, 0);
                    if(ip < 0)
                    {
                        return false;
                    }

                    const Symbol* iter_symbol = this->define(foreach->iterator->pos, foreach->iterator->value, false, false);
                    if(!iter_symbol)
                    {
                        return false;
                    }

                    ok = this->writeSymbol(iter_symbol, true);
                    if(!ok)
                    {
                        return false;
                    }

                    // Body
                    ok = this->pushContinueIP(update_ip);
                    if(!ok)
                    {
                        return false;
                    }

                    ok = this->pushBreakIP(jump_to_after_body_ip);
                    if(!ok)
                    {
                        return false;
                    }

                    ok = this->compileCodeBlock(foreach->body);
                    if(!ok)
                    {
                        return false;
                    }

                    this->popBreakIP();
                    this->popContinueIP();

                    ip = this->emit(OPCODE_JUMP, 1, (uint64_t)update_ip);
                    if(ip < 0)
                    {
                        return false;
                    }

                    int after_body_ip = this->getIP();
                    this->changeUint16Operand(jump_to_after_body_ip + 1, after_body_ip);

                    symbol_table->popBlockScope();
                    break;
                }
                case EXPRESSION_FOR_LOOP:
                {
                    const StmtForLoop* loop = &stmt->for_loop;

                    ok = symbol_table->pushBlockScope();
                    if(!ok)
                    {
                        return false;
                    }

                    // Init
                    int jump_to_after_update_ip = 0;
                    bool ok = false;
                    if(loop->init)
                    {
                        ok = this->compileStatement(loop->init);
                        if(!ok)
                        {
                            return false;
                        }
                        jump_to_after_update_ip = this->emit(OPCODE_JUMP, 1, (uint64_t)0xbeef);
                        if(jump_to_after_update_ip < 0)
                        {
                            return false;
                        }
                    }

                    // Update
                    int update_ip = this->getIP();
                    if(loop->update)
                    {
                        ok = this->compileExpression(loop->update);
                        if(!ok)
                        {
                            return false;
                        }
                        ip = this->emit(OPCODE_POP, 0);
                        if(ip < 0)
                        {
                            return false;
                        }
                    }

                    if(loop->init)
                    {
                        int after_update_ip = this->getIP();
                        this->changeUint16Operand(jump_to_after_update_ip + 1, after_update_ip);
                    }

                    // Test
                    if(loop->test)
                    {
                        ok = this->compileExpression(loop->test);
                        if(!ok)
                        {
                            return false;
                        }
                    }
                    else
                    {
                        ip = this->emit(OPCODE_TRUE, 0);
                        if(ip < 0)
                        {
                            return false;
                        }
                    }
                    int after_test_ip = this->getIP();

                    ip = this->emit(OPCODE_JUMP_IF_TRUE, 1, (uint64_t)(after_test_ip + 6));
                    if(ip < 0)
                    {
                        return false;
                    }
                    int jmp_to_after_body_ip = this->emit(OPCODE_JUMP, 1, (uint64_t)0xdead);
                    if(jmp_to_after_body_ip < 0)
                    {
                        return false;
                    }

                    // Body
                    ok = this->pushContinueIP(update_ip);
                    if(!ok)
                    {
                        return false;
                    }

                    ok = this->pushBreakIP(jmp_to_after_body_ip);
                    if(!ok)
                    {
                        return false;
                    }

                    ok = this->compileCodeBlock(loop->body);
                    if(!ok)
                    {
                        return false;
                    }

                    this->popBreakIP();
                    this->popContinueIP();

                    ip = this->emit(OPCODE_JUMP, 1, (uint64_t)update_ip);
                    if(ip < 0)
                    {
                        return false;
                    }

                    int after_body_ip = this->getIP();
                    this->changeUint16Operand(jmp_to_after_body_ip + 1, after_body_ip);

                    symbol_table->popBlockScope();
                    break;
                }
                case EXPRESSION_BLOCK:
                {
                    ok = this->compileCodeBlock(stmt->block);
                    if(!ok)
                    {
                        return false;
                    }
                    break;
                }
                case EXPRESSION_IMPORT:
                {
                    ok = this->importModule(stmt);
                    if(!ok)
                    {
                        return false;
                    }
                    break;
                }
                case EXPRESSION_RECOVER:
                {
                    const StmtRecover* recover = &stmt->recover;

                    if(symbol_table->isModuleGlobalScope())
                    {
                        m_errors->addError(APE_ERROR_COMPILATION, stmt->pos, "Recover statement cannot be defined in global scope");
                        return false;
                    }

                    if(!symbol_table->isTopBlockScope())
                    {
                        m_errors->addError(APE_ERROR_COMPILATION, stmt->pos,
                                         "Recover statement cannot be defined within other statements");
                        return false;
                    }

                    int recover_ip = this->emit(OPCODE_SET_RECOVER, 1, (uint64_t)0xbeef);
                    if(recover_ip < 0)
                    {
                        return false;
                    }

                    int jump_to_after_recover_ip = this->emit(OPCODE_JUMP, 1, (uint64_t)0xbeef);
                    if(jump_to_after_recover_ip < 0)
                    {
                        return false;
                    }

                    int after_jump_to_recover_ip = this->getIP();
                    this->changeUint16Operand(recover_ip + 1, after_jump_to_recover_ip);

                    ok = symbol_table->pushBlockScope();
                    if(!ok)
                    {
                        return false;
                    }

                    const Symbol* error_symbol = this->define(recover->error_ident->pos, recover->error_ident->value, false, false);
                    if(!error_symbol)
                    {
                        return false;
                    }

                    ok = this->writeSymbol(error_symbol, true);
                    if(!ok)
                    {
                        return false;
                    }

                    ok = this->compileCodeBlock(recover->body);
                    if(!ok)
                    {
                        return false;
                    }

                    if(!this->lastOpcodeIs(OPCODE_RETURN) && !this->lastOpcodeIs(OPCODE_RETURN_VALUE))
                    {
                        m_errors->addError(APE_ERROR_COMPILATION, stmt->pos, "Recover body must end with a return statement");
                        return false;
                    }

                    symbol_table->popBlockScope();

                    int after_recover_ip = this->getIP();
                    this->changeUint16Operand(jump_to_after_recover_ip + 1, after_recover_ip);

                    break;
                }
                default:
                {
                    APE_ASSERT(false);
                    return false;
                }
            }
            this->src_positions_stack->pop(NULL);
            return true;
        }

        bool compileExpression(Expression* expr)
        {
            bool ok = false;
            int ip = -1;

            Expression* expr_optimised = optimise_expression(expr);
            if(expr_optimised)
            {
                expr = expr_optimised;
            }

            ok = this->src_positions_stack->push(&expr->pos);
            if(!ok)
            {
                return false;
            }

            CompilationScope* compilation_scope = this->getCompilationScope();
            Symbol::Table* symbol_table = this->getSymbolTable();

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
                            m_errors->addFormat( APE_ERROR_COMPILATION, expr->pos, "Unknown infix operator");
                            goto error;
                        }
                    }

                    Expression* left = rearrange ? expr->infix.right : expr->infix.left;
                    Expression* right = rearrange ? expr->infix.left : expr->infix.right;

                    ok = this->compileExpression(left);
                    if(!ok)
                    {
                        goto error;
                    }

                    ok = this->compileExpression(right);
                    if(!ok)
                    {
                        goto error;
                    }

                    switch(expr->infix.op)
                    {
                        case OPERATOR_EQ:
                        case OPERATOR_NOT_EQ:
                        {
                            ip = this->emit(OPCODE_COMPARE_EQ, 0);
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
                            ip = this->emit(OPCODE_COMPARE, 0);
                            if(ip < 0)
                            {
                                goto error;
                            }
                            break;
                        }
                        default:
                            break;
                    }

                    ip = this->emit(op, 0);
                    if(ip < 0)
                    {
                        goto error;
                    }

                    break;
                }
                case EXPRESSION_NUMBER_LITERAL:
                {
                    double number = expr->number_literal;
                    ip = this->emit(OPCODE_NUMBER, 1, (uint64_t)ape_double_to_uint64(number));
                    if(ip < 0)
                    {
                        goto error;
                    }

                    break;
                }
                case EXPRESSION_STRING_LITERAL:
                {
                    int pos = 0;
                    int* current_pos = (int*)this->string_constants_positions->get(expr->string_literal);
                    if(current_pos)
                    {
                        pos = *current_pos;
                    }
                    else
                    {
                        Object obj = Object::makeString(this->mem, expr->string_literal);
                        if(obj.isNull())
                        {
                            goto error;
                        }

                        pos = this->addConstant(obj);
                        if(pos < 0)
                        {
                            goto error;
                        }

                        int* pos_val = m_alloc->allocate<int>();
                        if(!pos_val)
                        {
                            goto error;
                        }

                        *pos_val = pos;
                        ok = this->string_constants_positions->set(expr->string_literal, pos_val);
                        if(!ok)
                        {
                            m_alloc->release(pos_val);
                            goto error;
                        }
                    }

                    ip = this->emit(OPCODE_CONSTANT, 1, (uint64_t)pos);
                    if(ip < 0)
                    {
                        goto error;
                    }

                    break;
                }
                case EXPRESSION_NULL_LITERAL:
                {
                    ip = this->emit(OPCODE_NULL, 0);
                    if(ip < 0)
                    {
                        goto error;
                    }
                    break;
                }
                case EXPRESSION_BOOL_LITERAL:
                {
                    ip = this->emit(expr->bool_literal ? OPCODE_TRUE : OPCODE_FALSE, 0);
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
                        ok = this->compileExpression((Expression*)expr->array->get(i));
                        if(!ok)
                        {
                            goto error;
                        }
                    }
                    ip = this->emit(OPCODE_ARRAY, 1, (uint64_t)expr->array->count());
                    if(ip < 0)
                    {
                        goto error;
                    }
                    break;
                }
                case EXPRESSION_MAP_LITERAL:
                {
                    const MapLiteral* map = &expr->map;
                    int len = map->m_keylist->count();
                    ip = this->emit(OPCODE_MAP_START, 1, (uint64_t)len);
                    if(ip < 0)
                    {
                        goto error;
                    }

                    for(int i = 0; i < len; i++)
                    {
                        Expression* key = (Expression*)map->m_keylist->get(i);
                        Expression* val = (Expression*)map->m_valueslist->get(i);

                        ok = this->compileExpression(key);
                        if(!ok)
                        {
                            goto error;
                        }

                        ok = this->compileExpression(val);
                        if(!ok)
                        {
                            goto error;
                        }
                    }

                    ip = this->emit(OPCODE_MAP_END, 1, (uint64_t)len);
                    if(ip < 0)
                    {
                        goto error;
                    }

                    break;
                }
                case EXPRESSION_PREFIX:
                {
                    ok = this->compileExpression(expr->prefix.right);
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
                            m_errors->addFormat( APE_ERROR_COMPILATION, expr->pos, "Unknown prefix operator.");
                            goto error;
                        }
                    }
                    ip = this->emit(op, 0);
                    if(ip < 0)
                    {
                        goto error;
                    }

                    break;
                }
                case EXPRESSION_IDENT:
                {
                    const Ident* ident = expr->ident;
                    const Symbol* symbol = symbol_table->resolve(ident->value);
                    if(!symbol)
                    {
                        m_errors->addFormat( APE_ERROR_COMPILATION, ident->pos, "Symbol \"%s\" could not be resolved",
                                          ident->value);
                        goto error;
                    }
                    ok = this->readSymbol(symbol);
                    if(!ok)
                    {
                        goto error;
                    }

                    break;
                }
                case EXPRESSION_INDEX:
                {
                    const StmtIndex* index = &expr->index_expr;
                    ok = this->compileExpression(index->left);
                    if(!ok)
                    {
                        goto error;
                    }
                    ok = this->compileExpression(index->index);
                    if(!ok)
                    {
                        goto error;
                    }
                    ip = this->emit(OPCODE_GET_INDEX, 0);
                    if(ip < 0)
                    {
                        goto error;
                    }

                    break;
                }
                case EXPRESSION_FUNCTION_LITERAL:
                {
                    const StmtFuncDef* fn = &expr->fn_literal;

                    ok = this->pushCompilationScope();
                    if(!ok)
                    {
                        goto error;
                    }

                    ok = this->pushSymbolTable(0);
                    if(!ok)
                    {
                        goto error;
                    }

                    compilation_scope = this->getCompilationScope();
                    symbol_table = this->getSymbolTable();

                    if(fn->name)
                    {
                        const Symbol* fn_symbol = symbol_table->defineFunctionName(fn->name, false);
                        if(!fn_symbol)
                        {
                            m_errors->addFormat( APE_ERROR_COMPILATION, expr->pos, "Cannot define symbol \"%s\"", fn->name);
                            goto error;
                        }
                    }

                    const Symbol* this_symbol = symbol_table->defineThis();
                    if(!this_symbol)
                    {
                        m_errors->addError(APE_ERROR_COMPILATION, expr->pos, "Cannot define \"this\" symbol");
                        goto error;
                    }

                    for(int i = 0; i < expr->fn_literal.params->count(); i++)
                    {
                        Ident* param = (Ident*)expr->fn_literal.params->get(i);
                        const Symbol* param_symbol = this->define(param->pos, param->value, true, false);
                        if(!param_symbol)
                        {
                            goto error;
                        }
                    }

                    ok = this->compileStatements(fn->body->statements);
                    if(!ok)
                    {
                        goto error;
                    }

                    if(!this->lastOpcodeIs(OPCODE_RETURN_VALUE) && !this->lastOpcodeIs(OPCODE_RETURN))
                    {
                        ip = this->emit(OPCODE_RETURN, 0);
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
                        free_symbols->destroyWithItems(Symbol::callback_destroy);
                        goto error;
                    }
                    this->popSymbolTable();
                    this->popCompilationScope();
                    compilation_scope = this->getCompilationScope();
                    symbol_table = this->getSymbolTable();

                    Object obj = Object::makeFunction(this->mem, fn->name, comp_res, true, num_locals, fn->params->count(), 0);

                    if(obj.isNull())
                    {
                        free_symbols->destroyWithItems(Symbol::callback_destroy);
                        compilation_result_destroy(comp_res);
                        goto error;
                    }

                    for(int i = 0; i < free_symbols->count(); i++)
                    {
                        Symbol* symbol = (Symbol*)free_symbols->get(i);
                        ok = this->readSymbol(symbol);
                        if(!ok)
                        {
                            free_symbols->destroyWithItems(Symbol::callback_destroy);
                            goto error;
                        }
                    }

                    int pos = this->addConstant(obj);
                    if(pos < 0)
                    {
                        free_symbols->destroyWithItems(Symbol::callback_destroy);
                        goto error;
                    }

                    ip = this->emit(OPCODE_FUNCTION, 2, (uint64_t)pos, (uint64_t)free_symbols->count());
                    if(ip < 0)
                    {
                        free_symbols->destroyWithItems(Symbol::callback_destroy);
                        goto error;
                    }

                    free_symbols->destroyWithItems(Symbol::callback_destroy);

                    break;
                }
                case EXPRESSION_CALL:
                {
                    ok = this->compileExpression(expr->call_expr.function);
                    if(!ok)
                    {
                        goto error;
                    }

                    for(int i = 0; i < expr->call_expr.args->count(); i++)
                    {
                        Expression* arg_expr = (Expression*)expr->call_expr.args->get(i);
                        ok = this->compileExpression(arg_expr);
                        if(!ok)
                        {
                            goto error;
                        }
                    }

                    ip = this->emit(OPCODE_CALL, 1, (uint64_t)expr->call_expr.args->count());
                    if(ip < 0)
                    {
                        goto error;
                    }

                    break;
                }
                case EXPRESSION_ASSIGN:
                {
                    const StmtAssign* assign = &expr->assign;
                    if(assign->dest->type != EXPRESSION_IDENT && assign->dest->type != EXPRESSION_INDEX)
                    {
                        m_errors->addFormat( APE_ERROR_COMPILATION, assign->dest->pos, "Expression is not assignable.");
                        goto error;
                    }

                    if(assign->is_postfix)
                    {
                        ok = this->compileExpression(assign->dest);
                        if(!ok)
                        {
                            goto error;
                        }
                    }

                    ok = this->compileExpression(assign->source);
                    if(!ok)
                    {
                        goto error;
                    }

                    ip = this->emit(OPCODE_DUP, 0);
                    if(ip < 0)
                    {
                        goto error;
                    }

                    ok = this->src_positions_stack->push(&assign->dest->pos);
                    if(!ok)
                    {
                        goto error;
                    }

                    if(assign->dest->type == EXPRESSION_IDENT)
                    {
                        const Ident* ident = assign->dest->ident;
                        const Symbol* symbol = symbol_table->resolve(ident->value);
                        if(!symbol)
                        {
                            //m_errors->addFormat( APE_ERROR_COMPILATION, assign->dest->pos, "Symbol \"%s\" could not be resolved", ident->value);
                            //goto error;
                            symbol = symbol_table->define(ident->value, true);
                        }
                        if(!symbol->assignable)
                        {
                            m_errors->addFormat( APE_ERROR_COMPILATION, assign->dest->pos,
                                              "Symbol \"%s\" is not assignable", ident->value);
                            goto error;
                        }
                        ok = this->writeSymbol(symbol, false);
                        if(!ok)
                        {
                            goto error;
                        }
                    }
                    else if(assign->dest->type == EXPRESSION_INDEX)
                    {
                        const StmtIndex* index = &assign->dest->index_expr;
                        ok = this->compileExpression(index->left);
                        if(!ok)
                        {
                            goto error;
                        }
                        ok = this->compileExpression(index->index);
                        if(!ok)
                        {
                            goto error;
                        }
                        ip = this->emit(OPCODE_SET_INDEX, 0);
                        if(ip < 0)
                        {
                            goto error;
                        }
                    }

                    if(assign->is_postfix)
                    {
                        ip = this->emit(OPCODE_POP, 0);
                        if(ip < 0)
                        {
                            goto error;
                        }
                    }

                    this->src_positions_stack->pop(NULL);
                    break;
                }
                case EXPRESSION_LOGICAL:
                {
                    const StmtLogical* logi = &expr->logical;

                    ok = this->compileExpression(logi->left);
                    if(!ok)
                    {
                        goto error;
                    }

                    ip = this->emit(OPCODE_DUP, 0);
                    if(ip < 0)
                    {
                        goto error;
                    }

                    int after_left_jump_ip = 0;
                    if(logi->op == OPERATOR_LOGICAL_AND)
                    {
                        after_left_jump_ip = this->emit(OPCODE_JUMP_IF_FALSE, 1, (uint64_t)0xbeef);
                    }
                    else
                    {
                        after_left_jump_ip = this->emit(OPCODE_JUMP_IF_TRUE, 1, (uint64_t)0xbeef);
                    }

                    if(after_left_jump_ip < 0)
                    {
                        goto error;
                    }

                    ip = this->emit(OPCODE_POP, 0);
                    if(ip < 0)
                    {
                        goto error;
                    }

                    ok = this->compileExpression(logi->right);
                    if(!ok)
                    {
                        goto error;
                    }

                    int after_right_ip = this->getIP();
                    this->changeUint16Operand(after_left_jump_ip + 1, after_right_ip);

                    break;
                }
                case EXPRESSION_TERNARY:
                {
                    const StmtTernary* ternary = &expr->ternary;

                    ok = this->compileExpression(ternary->test);
                    if(!ok)
                    {
                        goto error;
                    }

                    int else_jump_ip = this->emit(OPCODE_JUMP_IF_FALSE, 1, (uint64_t)0xbeef);

                    ok = this->compileExpression(ternary->if_true);
                    if(!ok)
                    {
                        goto error;
                    }

                    int end_jump_ip = this->emit(OPCODE_JUMP, 1, (uint64_t)0xbeef);

                    int else_ip = this->getIP();
                    this->changeUint16Operand(else_jump_ip + 1, else_ip);

                    ok = this->compileExpression(ternary->if_false);
                    if(!ok)
                    {
                        goto error;
                    }

                    int end_ip = this->getIP();
                    this->changeUint16Operand(end_jump_ip + 1, end_ip);

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
            this->src_positions_stack->pop(NULL);
            Expression::destroyExpression(expr_optimised);
            return res;
        }

        bool compileCodeBlock(const StmtBlock* block)
        {
            Symbol::Table* symbol_table = this->getSymbolTable();
            if(!symbol_table)
            {
                return false;
            }

            bool ok = symbol_table->pushBlockScope();
            if(!ok)
            {
                return false;
            }

            if(block->statements->count() == 0)
            {
                int ip = this->emit(OPCODE_NULL, 0);
                if(ip < 0)
                {
                    return false;
                }
                ip = this->emit(OPCODE_POP, 0);
                if(ip < 0)
                {
                    return false;
                }
            }

            for(int i = 0; i < block->statements->count(); i++)
            {
                const Expression* stmt = (Expression*)block->statements->get(i);
                bool ok = this->compileStatement(stmt);
                if(!ok)
                {
                    return false;
                }
            }
            symbol_table->popBlockScope();
            return true;
        }

        int addConstant(Object obj)
        {
            bool ok = this->constants->add(&obj);
            if(!ok)
            {
                return -1;
            }
            int pos = this->constants->count() - 1;
            return pos;
        }

        void changeUint16Operand(int ip, uint16_t operand)
        {
            Array* bytecode = this->getBytecode();
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

        bool lastOpcodeIs(opcode_t op)
        {
            opcode_t last_opcode = this->getLastOpcode();
            return last_opcode == op;
        }

        bool readSymbol(const Symbol* symbol)
        {
            int ip = -1;
            if(symbol->type == SYMBOL_MODULE_GLOBAL)
            {
                ip = this->emit(OPCODE_GET_MODULE_GLOBAL, 1, (uint64_t)(symbol->index));
            }
            else if(symbol->type == SYMBOL_APE_GLOBAL)
            {
                ip = this->emit(OPCODE_GET_APE_GLOBAL, 1, (uint64_t)(symbol->index));
            }
            else if(symbol->type == SYMBOL_LOCAL)
            {
                ip = this->emit(OPCODE_GET_LOCAL, 1, (uint64_t)(symbol->index));
            }
            else if(symbol->type == SYMBOL_FREE)
            {
                ip = this->emit(OPCODE_GET_FREE, 1, (uint64_t)(symbol->index));
            }
            else if(symbol->type == SYMBOL_FUNCTION)
            {
                ip = this->emit(OPCODE_CURRENT_FUNCTION, 0);
            }
            else if(symbol->type == SYMBOL_THIS)
            {
                ip = this->emit(OPCODE_GET_THIS, 0);
            }
            return ip >= 0;
        }

        bool writeSymbol(const Symbol* symbol, bool define)
        {
            int ip = -1;
            if(symbol->type == SYMBOL_MODULE_GLOBAL)
            {
                if(define)
                {
                    ip = this->emit(OPCODE_DEFINE_MODULE_GLOBAL, 1, (uint64_t)(symbol->index));
                }
                else
                {
                    ip = this->emit(OPCODE_SET_MODULE_GLOBAL, 1, (uint64_t)(symbol->index));
                }
            }
            else if(symbol->type == SYMBOL_LOCAL)
            {
                if(define)
                {
                    ip = this->emit(OPCODE_DEFINE_LOCAL, 1, (uint64_t)(symbol->index));
                }
                else
                {
                    ip = this->emit(OPCODE_SET_LOCAL, 1, (uint64_t)(symbol->index));
                }
            }
            else if(symbol->type == SYMBOL_FREE)
            {
                ip = this->emit(OPCODE_SET_FREE, 1, (uint64_t)(symbol->index));
            }
            return ip >= 0;
        }

        bool pushBreakIP(int ip)
        {
            CompilationScope* comp_scope = this->getCompilationScope();
            return comp_scope->break_ip_stack->push(&ip);
        }

        void popBreakIP()
        {
            CompilationScope* comp_scope = this->getCompilationScope();
            if(comp_scope->break_ip_stack->count() == 0)
            {
                APE_ASSERT(false);
                return;
            }
            comp_scope->break_ip_stack->pop(NULL);
        }

        int getBreakIP()
        {
            CompilationScope* comp_scope = this->getCompilationScope();
            if(comp_scope->break_ip_stack->count() == 0)
            {
                return -1;
            }
            int* res = (int*)comp_scope->break_ip_stack->top();
            return *res;
        }

        bool pushContinueIP(int ip)
        {
            CompilationScope* comp_scope = this->getCompilationScope();
            return comp_scope->continue_ip_stack->push(&ip);
        }

        void popContinueIP()
        {
            CompilationScope* comp_scope = this->getCompilationScope();
            if(comp_scope->continue_ip_stack->count() == 0)
            {
                APE_ASSERT(false);
                return;
            }
            comp_scope->continue_ip_stack->pop(NULL);
        }

        int getContinueIP()
        {
            CompilationScope* comp_scope = this->getCompilationScope();
            if(comp_scope->continue_ip_stack->count() == 0)
            {
                APE_ASSERT(false);
                return -1;
            }
            int* res = (int*)comp_scope->continue_ip_stack->top();
            return *res;
        }

        int getIP()
        {
            CompilationScope* compilation_scope = this->getCompilationScope();
            return compilation_scope->bytecode->count();
        }

        Array * getSourcePositions()
        {
            CompilationScope* compilation_scope = this->getCompilationScope();
            return compilation_scope->src_positions;
        }

        Array * getBytecode()
        {
            CompilationScope* compilation_scope = this->getCompilationScope();
            return compilation_scope->bytecode;
        }

        FileScope* makeFileScope(CompiledFile* file)
        {
            FileScope* file_scope = m_alloc->allocate<FileScope>();
            if(!file_scope)
            {
                return NULL;
            }
            memset(file_scope, 0, sizeof(FileScope));
            file_scope->m_alloc = m_alloc;
            file_scope->parser = Parser::make(m_alloc, this->config, m_errors);
            if(!file_scope->parser)
            {
                goto err;
            }
            file_scope->symbol_table = NULL;
            file_scope->file = file;
            file_scope->loaded_module_names = PtrArray::make(m_alloc);
            if(!file_scope->loaded_module_names)
            {
                goto err;
            }
            return file_scope;
        err:
            file_scope->destroy();
            return NULL;
        }

        bool pushFileScope(const char* filepath)
        {
            Symbol::Table* prev_st = NULL;
            if(this->file_scopes->count() > 0)
            {
                prev_st = this->getSymbolTable();
            }

            CompiledFile* file = compiled_file_make(m_alloc, filepath);
            if(!file)
            {
                return false;
            }

            bool ok = this->files->add(file);
            if(!ok)
            {
                compiled_file_destroy(file);
                return false;
            }

            FileScope* file_scope = this->makeFileScope(file);
            if(!file_scope)
            {
                return false;
            }

            ok = this->file_scopes->push(file_scope);
            if(!ok)
            {
                file_scope->destroy();
                return false;
            }

            int global_offset = 0;
            if(prev_st)
            {
                BlockScope* prev_st_top_scope = prev_st->getBlockScope();
                global_offset = prev_st_top_scope->offset + prev_st_top_scope->num_definitions;
            }

            ok = this->pushSymbolTable(global_offset);
            if(!ok)
            {
                this->file_scopes->pop();
                file_scope->destroy();
                return false;
            }

            return true;
        }

        void popFileScope()
        {
            Symbol::Table* popped_st = this->getSymbolTable();
            BlockScope* popped_st_top_scope = popped_st->getBlockScope();
            int popped_num_defs = popped_st_top_scope->num_definitions;

            while(this->getSymbolTable())
            {
                this->popSymbolTable();
            }
            FileScope* scope = (FileScope*)this->file_scopes->top();
            if(!scope)
            {
                APE_ASSERT(false);
                return;
            }
            scope->destroy();

            this->file_scopes->pop();

            if(this->file_scopes->count() > 0)
            {
                Symbol::Table* current_st = this->getSymbolTable();
                BlockScope* current_st_top_scope = current_st->getBlockScope();
                current_st_top_scope->num_definitions += popped_num_defs;
            }
        }

        void setCompilationScope(CompilationScope* scope)
        {
            this->compilation_scope = scope;
        }
};


struct Program
{
    Context* context;
    CompilationResult* comp_res;
};

struct Context
{
    public:
        static Context* make(void)
        {
            return Context::make(NULL, NULL, NULL);
        }

        static Context* make(MallocFNCallback malloc_fn, FreeFNCallback free_fn, void* pctx)
        {
            Allocator custom_alloc = Allocator::make((AllocatorMallocFNCallback)malloc_fn, (AllocatorFreeFNCallback)free_fn, pctx);
            Context* ctx = custom_alloc.allocate<Context>();
            if(!ctx)
            {
                return NULL;
            }
            memset(ctx, 0, sizeof(Context));
            ctx->m_alloc = Allocator::make(ape_malloc, ape_free, ctx);
            ctx->custom_allocator = custom_alloc;
            ctx->setDefaultConfig();
            ctx->m_errors.init();
            ctx->mem = GCMemory::make(&ctx->m_alloc);
            if(!ctx->mem)
            {
                goto err;
            }
            ctx->files = PtrArray::make(&ctx->m_alloc);
            if(!ctx->files)
            {
                goto err;
            }
            ctx->global_store = global_store_make(&ctx->m_alloc, ctx->mem);
            if(!ctx->global_store)
            {
                goto err;
            }
            ctx->compiler = Compiler::make(&ctx->m_alloc, &ctx->config, ctx->mem, &ctx->m_errors, ctx->files, ctx->global_store);
            if(!ctx->compiler)
            {
                goto err;
            }
            ctx->vm = VM::make(&ctx->m_alloc, &ctx->config, ctx->mem, &ctx->m_errors, ctx->global_store);
            if(!ctx->vm)
            {
                goto err;
            }
            builtins_install(ctx->vm);
            return ctx;
        err:
            ctx->deinit();
            custom_alloc.release(ctx);
            return NULL;
        }

    public:
        Allocator m_alloc;
        GCMemory* mem;
        PtrArray * files;
        GlobalStore* global_store;
        Compiler* compiler;
        VM* vm;
        ErrorList m_errors;
        Config config;
        Allocator custom_allocator;

    public:
        void destroy()
        {
            if(!this)
            {
                return;
            }
            this->deinit();
            Allocator alloc = m_alloc;
            alloc.release(this);
        }

        void deinit()
        {
            this->vm->destroy();
            this->compiler->destroy();
            global_store_destroy(this->global_store);
            this->mem->destroy();
            this->files->destroyWithItems(compiled_file_destroy);
            m_errors.deinit();
        }

        Object makeNamedNativeFunction(const char* name, UserFNCallback fn, void* data)
        {
            NativeFuncWrapper wrapper;
            memset(&wrapper, 0, sizeof(NativeFuncWrapper));
            wrapper.wrapped_funcptr = fn;
            wrapper.context = this;
            wrapper.data = data;
            Object wrapper_native_function = Object::makeNativeFunctionMemory(this->mem, name, ape_native_fn_wrapper, &wrapper, sizeof(wrapper));
            if(wrapper_native_function.isNull())
            {
                return Object::makeNull();
            }
            return wrapper_native_function;
        }

        void resetState()
        {
            this->clearErrors();
            this->vm->reset();
        }

        void setDefaultConfig()
        {
            memset(&this->config, 0, sizeof(Config));
            this->setREPLMode(false);
            this->setTimeout(-1);
            this->setFileRead(read_file_default, this);
            this->setFileWrite(write_file_default, this);
            this->setStdoutWrite(stdout_write_default, this);
        }

        void pushRuntimeError(const char* fmt, ...)
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
            m_errors.addError( APE_ERROR_RUNTIME, Position::invalid(), res);
        }

        char* serializeError(const ErrorList::Error* err)
        {
            const char* type_str = ErrorList::toString(err->type);
            const char* filename = err->getFilePath();
            const char* line = err->getSource();
            int line_num = err->getLine();
            int col_num = err->getColumn();
            StringBuffer* buf = StringBuffer::make(&m_alloc);
            if(!buf)
            {
                return NULL;
            }
            if(line)
            {
                buf->append(line);
                buf->append("\n");
                if(col_num >= 0)
                {
                    for(int j = 0; j < (col_num - 1); j++)
                    {
                        buf->append(" ");
                    }
                    buf->append("^\n");
                }
            }
            buf->appendFormat("%s ERROR in \"%s\" on %d:%d: %s\n", type_str, filename, line_num, col_num, err->message);
            const Traceback* traceback = err->getTraceback();
            if(traceback)
            {
                buf->appendFormat("Traceback:\n");
                traceback_to_string((const Traceback*)err->getTraceback(), buf);
            }
            if(buf->failed())
            {
                buf->destroy();
                return NULL;
            }
            return buf->getStringAndDestroy();
        }

        void releaseAllocated(void* ptr)
        {
            m_alloc.release(ptr);
        }

        void setREPLMode(bool enabled)
        {
            this->config.repl_mode = enabled;
        }

        bool setTimeout(double max_execution_time_ms)
        {
            if(!ape_timer_platform_supported())
            {
                this->config.max_execution_time_ms = 0;
                this->config.max_execution_time_set = false;
                return false;
            }

            if(max_execution_time_ms >= 0)
            {
                this->config.max_execution_time_ms = max_execution_time_ms;
                this->config.max_execution_time_set = true;
            }
            else
            {
                this->config.max_execution_time_ms = 0;
                this->config.max_execution_time_set = false;
            }
            return true;
        }

        void setStdoutWrite(StdoutWriteFNCallback stdout_write, void* context)
        {
            this->config.stdio.write.write = stdout_write;
            this->config.stdio.write.context = context;
        }

        void setFileWrite(WriteFileFNCallback file_write, void* context)
        {
            this->config.fileio.write_file.write_file = file_write;
            this->config.fileio.write_file.context = context;
        }

        void setFileRead(ReadFileFNCallback file_read, void* context)
        {
            this->config.fileio.read_file.read_file = file_read;
            this->config.fileio.read_file.context = context;
        }

        Object executeCode(const char* code)
        {
            bool ok;
            Object res;
            CompilationResult* comp_res;
            this->resetState();
            comp_res = this->compiler->compileSource(code);
            if(!comp_res || m_errors.count() > 0)
            {
                goto err;
            }
            ok = this->vm->run(comp_res, this->compiler->getConstants());
            if(!ok || m_errors.count() > 0)
            {
                goto err;
            }
            APE_ASSERT(this->vm->sp == 0);
            res = this->vm->getLastPopped();
            if(res.type() == APE_OBJECT_NONE)
            {
                goto err;
            }
            compilation_result_destroy(comp_res);
            return res;

        err:
            compilation_result_destroy(comp_res);
            return Object::makeNull();
        }

        Object executeFile(const char* path)
        {
            bool ok;
            Object res;
            CompilationResult* comp_res;
            this->resetState();
            comp_res = this->compiler->compileFile(path);
            if(!comp_res || m_errors.count() > 0)
            {
                goto err;
            }
            ok = this->vm->run(comp_res, this->compiler->getConstants());
            if(!ok || m_errors.count() > 0)
            {
                goto err;
            }
            APE_ASSERT(this->vm->sp == 0);
            res = this->vm->getLastPopped();
            if(res.type() == APE_OBJECT_NONE)
            {
                goto err;
            }
            compilation_result_destroy(comp_res);

            return res;

        err:
            compilation_result_destroy(comp_res);
            return Object::makeNull();
        }

        bool hasErrors()
        {
            return this->errorCount() > 0;
        }

        int errorCount()
        {
            return m_errors.count();
        }

        void clearErrors()
        {
            m_errors.clear();
        }

        const ErrorList::Error* getError(int index)
        {
            return (const ErrorList::Error*)m_errors.get(index);
        }

        bool setNativeFunction(const char* name, UserFNCallback fn, void* data)
        {
            Object obj = this->makeNamedNativeFunction(name, fn, data);
            if(obj.isNull())
            {
                return false;
            }
            return this->setGlobalConstant(name, obj);
        }

        bool setGlobalConstant(const char* name, Object obj)
        {
            return global_store_set(this->global_store, name, obj);
        }

        Object getObject(const char* name)
        {
            Symbol::Table* st = this->compiler->getSymbolTable();
            const Symbol* symbol = st->resolve(name);
            if(!symbol)
            {
                m_errors.addFormat( APE_ERROR_USER, Position::invalid(), "Symbol \"%s\" is not defined", name);
                return Object::makeNull();
            }
            Object res = Object::makeNull();
            if(symbol->type == SYMBOL_MODULE_GLOBAL)
            {
                res = this->vm->getGlobalByIndex(symbol->index);
            }
            else if(symbol->type == SYMBOL_APE_GLOBAL)
            {
                bool ok = false;
                res = global_store_get_object_at(this->global_store, symbol->index, &ok);
                if(!ok)
                {
                    m_errors.addFormat(APE_ERROR_USER, Position::invalid(), "Failed to get global object at %d", symbol->index);
                    return Object::makeNull();
                }
            }
            else
            {
                m_errors.addFormat( APE_ERROR_USER, Position::invalid(), "Value associated with symbol \"%s\" could not be loaded", name);
                return Object::makeNull();
            }
            return res;
        }

        Object makeNativeFunctionObject(UserFNCallback fn, void* data)
        {
            return this->makeNamedNativeFunction("", fn, data);
        }
};

//impl::object
ObjectData* Object::allocObjectData(GCMemory* mem, ObjectType t)
{
    return mem->allocObjectData(t);
}

ObjectData* Object::objectDataFromPool(GCMemory* mem, ObjectType t)
{
    return mem->objectDataFromPool(t);
}

Allocator* Object::getAllocator(GCMemory* mem)
{
    return mem->m_alloc;
}


Object Object::makeExternal(GCMemory* mem, void* data)
{
    ObjectData* obj = Object::allocObjectData(mem, APE_OBJECT_EXTERNAL);
    if(!obj)
    {
        return Object::makeNull();
    }
    obj->external.data = data;
    obj->external.data_destroy_fn = NULL;
    obj->external.data_copy_fn = NULL;
    return Object::makeFromData(APE_OBJECT_EXTERNAL, obj);
}

Object Object::makeFunction(GCMemory* mem, const char* name, CompilationResult* comp_res, bool owns_data, int num_locals, int num_args, int free_vals_count)
{
    ObjectData* data;
    Allocator* alloc;
    alloc = Object::getAllocator(mem);
    data = Object::allocObjectData(mem, APE_OBJECT_FUNCTION);
    if(!data)
    {
        return Object::makeNull();
    }
    if(owns_data)
    {
        data->function.name = name ? ape_strdup(alloc, name) : ape_strdup(alloc, "anonymous");
        if(!data->function.name)
        {
            return Object::makeNull();
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
        data->function.free_vals_allocated = (Object*)Object::getAllocator(mem)->allocate(sizeof(Object) * free_vals_count);
        if(!data->function.free_vals_allocated)
        {
            return Object::makeNull();
        }
    }
    data->function.free_vals_count = free_vals_count;
    return Object::makeFromData(APE_OBJECT_FUNCTION, data);
}

Object Object::makeErrorNoCopy(GCMemory* mem, char* error)
{
    ObjectData* data = Object::allocObjectData(mem, APE_OBJECT_ERROR);
    if(!data)
    {
        return Object::makeNull();
    }
    data->error.message = error;
    data->error.traceback = NULL;
    return Object::makeFromData(APE_OBJECT_ERROR, data);
}

Object Object::makeMap(GCMemory* mem, unsigned capacity)
{
    ObjectData* data = Object::objectDataFromPool(mem, APE_OBJECT_MAP);
    if(data)
    {
        data->map->clear();
        return Object::makeFromData(APE_OBJECT_MAP, data);
    }
    data = Object::allocObjectData(mem, APE_OBJECT_MAP);
    if(!data)
    {
        return Object::makeNull();
    }
    data->map = ValDictionary::makeWithCapacity(Object::getAllocator(mem), capacity, sizeof(Object), sizeof(Object));
    if(!data->map)
    {
        return Object::makeNull();
    }
    data->map->setHashFunction((CollectionsHashFNCallback)object_hash);
    data->map->setEqualsFunction((CollectionsEqualsFNCallback)object_equals_wrapped);
    return Object::makeFromData(APE_OBJECT_MAP, data);
}

Object Object::makeArray(GCMemory* mem, unsigned capacity)
{
    ObjectData* data = Object::objectDataFromPool(mem, APE_OBJECT_ARRAY);
    if(data)
    {
        data->array->clear();
        return Object::makeFromData(APE_OBJECT_ARRAY, data);
    }
    data = Object::allocObjectData(mem, APE_OBJECT_ARRAY);
    if(!data)
    {
        return Object::makeNull();
    }
    data->array = Array::make(Object::getAllocator(mem), capacity, sizeof(Object));
    if(!data->array)
    {
        return Object::makeNull();
    }
    return Object::makeFromData(APE_OBJECT_ARRAY, data);
}

Object Object::makeNativeFunctionMemory(GCMemory* mem, const char* name, NativeFNCallback fn, void* data, int data_len)
{
    if(data_len > NATIVE_FN_MAX_DATA_LEN)
    {
        return Object::makeNull();
    }
    ObjectData* obj = Object::allocObjectData(mem, APE_OBJECT_NATIVE_FUNCTION);
    if(!obj)
    {
        return Object::makeNull();
    }
    obj->native_function.name = ape_strdup(Object::getAllocator(mem), name);
    if(!obj->native_function.name)
    {
        return Object::makeNull();
    }
    obj->native_function.native_funcptr = fn;
    if(data)
    {
        //memcpy(obj->native_function.data, data, data_len);
        obj->native_function.data = data;
    }
    obj->native_function.data_len = data_len;
    return Object::makeFromData(APE_OBJECT_NATIVE_FUNCTION, obj);
}

Object Object::makeStringCapacity(GCMemory* mem, int capacity)
{
    ObjectData* data = Object::objectDataFromPool(mem, APE_OBJECT_STRING);
    if(!data)
    {
        data = Object::allocObjectData(mem, APE_OBJECT_STRING);
        if(!data)
        {
            return Object::makeNull();
        }
        data->string.m_capacity = OBJECT_STRING_BUF_SIZE - 1;
        data->string.is_allocated = false;
    }

    data->string.length = 0;
    data->string.hash = 0;

    if(capacity > data->string.m_capacity)
    {
        bool ok = object_data_string_reserve_capacity(data, capacity);
        if(!ok)
        {
            return Object::makeNull();
        }
    }

    return Object::makeFromData(APE_OBJECT_STRING, data);
}

void Object::deinit(Object obj)
{
    if(GCMemory::objectIsAllocated(obj))
    {
        ObjectData* data = obj.getAllocatedData();
        object_data_deinit(data);
    }
}

// impl:expression

void StmtIfClause::Case::destroy()
{
    if(!this)
    {
        return;
    }
    Expression::destroyExpression(this->test);
    code_block_destroy(this->consequence);
    m_alloc->release(this);
}

StmtIfClause::Case* StmtIfClause::Case::copy()
{
    if(!this)
    {
        return NULL;
    }
    Expression* test_copy = NULL;
    StmtBlock* consequence_copy = NULL;
    Case* if_case_copy = NULL;
    test_copy = Expression::copyExpression(this->test);
    if(!test_copy)
    {
        goto err;
    }
    consequence_copy = code_block_copy(this->consequence);
    if(!test_copy || !consequence_copy)
    {
        goto err;
    }
    if_case_copy = Case::make(m_alloc, test_copy, consequence_copy);
    if(!if_case_copy)
    {
        goto err;
    }
    return if_case_copy;
err:
    Expression::destroyExpression(test_copy);
    code_block_destroy(consequence_copy);
    if_case_copy->destroy();
    return NULL;
}

StmtBlock* code_block_make(Allocator* alloc, PtrArray * statements)
{
    StmtBlock* block = alloc->allocate<StmtBlock>();
    if(!block)
    {
        return NULL;
    }
    block->m_alloc = alloc;
    block->statements = statements;
    return block;
}

void code_block_destroy(StmtBlock* block)
{
    if(!block)
    {
        return;
    }
    block->statements->destroyWithItems(Expression::destroyExpression);
    block->m_alloc->release(block);
}

StmtBlock* code_block_copy(StmtBlock* block)
{
    if(!block)
    {
        return NULL;
    }
    PtrArray* statements_copy = block->statements->copyWithItems(Expression::copyExpression, Expression::destroyExpression);
    if(!statements_copy)
    {
        return NULL;
    }
    StmtBlock* res = code_block_make(block->m_alloc, statements_copy);
    if(!res)
    {
        statements_copy->destroyWithItems(Expression::destroyExpression);
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
    ident->m_alloc->release(ident->value);
    ident->value = NULL;
    ident->pos = Position::invalid();
    ident->m_alloc->release(ident);
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

void statements_to_string(StringBuffer* buf, PtrArray * statements)
{
    int i;
    int cn;
    const Expression* stmt;
    cn = statements->count();
    for(i = 0; i < cn; i++)
    {
        stmt = (Expression*)statements->get(i);
        statement_to_string(stmt, buf);
        if(i < (cn - 1))
        {
            buf->append("\n");
        }
    }
}

void statement_to_string(const Expression* stmt, StringBuffer* buf)
{
    int i;
    Expression* arr_expr;
    const MapLiteral* map;
    const StmtDefine* def_stmt;
    StmtIfClause::Case* if_case;
    StmtIfClause::Case* elif_case;
    switch(stmt->type)
    {
        case EXPRESSION_DEFINE:
        {
            def_stmt = &stmt->define;
            if(stmt->define.assignable)
            {
                buf->append("var ");
            }
            else
            {
                buf->append("const ");
            }
            buf->append(def_stmt->name->value);
            buf->append(" = ");

            if(def_stmt->value)
            {
                statement_to_string(def_stmt->value, buf);
            }

            break;
        }
        case EXPRESSION_IF:
        {
            if_case = (StmtIfClause::Case*)stmt->if_statement.cases->get(0);
            buf->append("if (");
            statement_to_string(if_case->test, buf);
            buf->append(") ");
            code_block_to_string(if_case->consequence, buf);
            for(i = 1; i < stmt->if_statement.cases->count(); i++)
            {
                elif_case = (StmtIfClause::Case*)stmt->if_statement.cases->get(i);
                buf->append(" elif (");
                statement_to_string(elif_case->test, buf);
                buf->append(") ");
                code_block_to_string(elif_case->consequence, buf);
            }
            if(stmt->if_statement.alternative)
            {
                buf->append(" else ");
                code_block_to_string(stmt->if_statement.alternative, buf);
            }
            break;
        }
        case EXPRESSION_RETURN_VALUE:
        {
            buf->append("return ");
            if(stmt->return_value)
            {
                statement_to_string(stmt->return_value, buf);
            }
            break;
        }
        case EXPRESSION_EXPRESSION:
        {
            if(stmt->expression)
            {
                statement_to_string(stmt->expression, buf);
            }
            break;
        }
        case EXPRESSION_WHILE_LOOP:
        {
            buf->append("while (");
            statement_to_string(stmt->while_loop.test, buf);
            buf->append(")");
            code_block_to_string(stmt->while_loop.body, buf);
            break;
        }
        case EXPRESSION_FOR_LOOP:
        {
            buf->append("for (");
            if(stmt->for_loop.init)
            {
                statement_to_string(stmt->for_loop.init, buf);
                buf->append(" ");
            }
            else
            {
                buf->append(";");
            }
            if(stmt->for_loop.test)
            {
                statement_to_string(stmt->for_loop.test, buf);
                buf->append("; ");
            }
            else
            {
                buf->append(";");
            }
            if(stmt->for_loop.update)
            {
                statement_to_string(stmt->for_loop.test, buf);
            }
            buf->append(")");
            code_block_to_string(stmt->for_loop.body, buf);
            break;
        }
        case EXPRESSION_FOREACH:
        {
            buf->append("for (");
            buf->appendFormat("%s", stmt->foreach.iterator->value);
            buf->append(" in ");
            statement_to_string(stmt->foreach.source, buf);
            buf->append(")");
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
            buf->append("break");
            break;
        }
        case EXPRESSION_CONTINUE:
        {
            buf->append("continue");
            break;
        }
        case EXPRESSION_IMPORT:
        {
            buf->appendFormat("import \"%s\"", stmt->import.path);
            break;
        }
        case EXPRESSION_RECOVER:
        {
            buf->appendFormat("recover (%s)", stmt->recover.error_ident->value);
            code_block_to_string(stmt->recover.body, buf);
            break;
        }
        case EXPRESSION_IDENT:
        {
            buf->append(stmt->ident->value);
            break;
        }
        case EXPRESSION_NUMBER_LITERAL:
        {
            buf->appendFormat("%1.17g", stmt->number_literal);
            break;
        }
        case EXPRESSION_BOOL_LITERAL:
        {
            buf->appendFormat("%s", stmt->bool_literal ? "true" : "false");
            break;
        }
        case EXPRESSION_STRING_LITERAL:
        {
            buf->appendFormat("\"%s\"", stmt->string_literal);
            break;
        }
        case EXPRESSION_NULL_LITERAL:
        {
            buf->append("null");
            break;
        }
        case EXPRESSION_ARRAY_LITERAL:
        {
            buf->append("[");
            for(i = 0; i < stmt->array->count(); i++)
            {
                arr_expr = (Expression*)stmt->array->get(i);
                statement_to_string(arr_expr, buf);
                if(i < (stmt->array->count() - 1))
                {
                    buf->append(", ");
                }
            }
            buf->append("]");
            break;
        }
        case EXPRESSION_MAP_LITERAL:
        {
            map = &stmt->map;
            buf->append("{");
            for(i = 0; i < map->m_keylist->count(); i++)
            {
                Expression* key_expr = (Expression*)map->m_keylist->get(i);
                Expression* val_expr = (Expression*)map->m_valueslist->get(i);
                statement_to_string(key_expr, buf);
                buf->append(" : ");
                statement_to_string(val_expr, buf);
                if(i < (map->m_keylist->count() - 1))
                {
                    buf->append(", ");
                }
            }
            buf->append("}");
            break;
        }
        case EXPRESSION_PREFIX:
        {
            buf->append("(");
            buf->append(operator_to_string(stmt->infix.op));
            statement_to_string(stmt->prefix.right, buf);
            buf->append(")");
            break;
        }
        case EXPRESSION_INFIX:
        {
            buf->append("(");
            statement_to_string(stmt->infix.left, buf);
            buf->append(" ");
            buf->append(operator_to_string(stmt->infix.op));
            buf->append(" ");
            statement_to_string(stmt->infix.right, buf);
            buf->append(")");
            break;
        }
        case EXPRESSION_FUNCTION_LITERAL:
        {
            auto fn = &stmt->fn_literal;
            buf->append("function");
            buf->append("(");
            for(i = 0; i < fn->params->count(); i++)
            {
                Ident* param = (Ident*)fn->params->get(i);
                buf->append(param->value);
                if(i < (fn->params->count() - 1))
                {
                    buf->append(", ");
                }
            }
            buf->append(") ");
            code_block_to_string(fn->body, buf);
            break;
        }
        case EXPRESSION_CALL:
        {
            auto call_expr = &stmt->call_expr;
            statement_to_string(call_expr->function, buf);
            buf->append("(");
            for(int i = 0; i < call_expr->args->count(); i++)
            {
                Expression* arg = (Expression*)call_expr->args->get(i);
                statement_to_string(arg, buf);
                if(i < (call_expr->args->count() - 1))
                {
                    buf->append(", ");
                }
            }
            buf->append(")");

            break;
        }
        case EXPRESSION_INDEX:
        {
            buf->append("(");
            statement_to_string(stmt->index_expr.left, buf);
            buf->append("[");
            statement_to_string(stmt->index_expr.index, buf);
            buf->append("])");
            break;
        }
        case EXPRESSION_ASSIGN:
        {
            statement_to_string(stmt->assign.dest, buf);
            buf->append(" = ");
            statement_to_string(stmt->assign.source, buf);
            break;
        }
        case EXPRESSION_LOGICAL:
        {
            statement_to_string(stmt->logical.left, buf);
            buf->append(" ");
            buf->append(operator_to_string(stmt->infix.op));
            buf->append(" ");
            statement_to_string(stmt->logical.right, buf);
            break;
        }
        case EXPRESSION_TERNARY:
        {
            statement_to_string(stmt->ternary.test, buf);
            buf->append(" ? ");
            statement_to_string(stmt->ternary.if_true, buf);
            buf->append(" : ");
            statement_to_string(stmt->ternary.if_false, buf);
            break;
        }
        case EXPRESSION_NONE:
        {
            buf->append("EXPRESSION_NONE");
            break;
        }
    }
}

void code_block_to_string(const StmtBlock* stmt, StringBuffer* buf)
{
    int i;
    Expression* istmt;
    buf->append("{ ");
    for(i = 0; i < stmt->statements->count(); i++)
    {
        istmt = (Expression*)stmt->statements->get(i);
        statement_to_string(istmt, buf);
        buf->append("\n");
    }
    buf->append(" }");
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
            res->appendFormat("%d:%-4d\t%04d\t%s", src_pos.line, src_pos.column, pos, def->name);
        }
        else
        {
            res->appendFormat("%04d %s", pos, def->name);
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
                res->appendFormat(" %1.17g", val_double);
            }
            else
            {
                res->appendFormat(" %llu", (long long unsigned int)operands[i]);
            }
            pos += def->operand_widths[i];
        }
        res->append("\n");
    }
    return;
}

void object_to_string(Object obj, StringBuffer* buf, bool quote_str)
{
    int i;
    ObjectType type;
    type = obj.type();
    switch(type)
    {
        case APE_OBJECT_FREED:
        {
            buf->append("FREED");
            break;
        }
        case APE_OBJECT_NONE:
        {
            buf->append("NONE");
            break;
        }
        case APE_OBJECT_NUMBER:
        {
            double number = object_get_number(obj);
            buf->appendFormat("%1.10g", number);
            break;
        }
        case APE_OBJECT_BOOL:
        {
            buf->append(object_get_bool(obj) ? "true" : "false");
            break;
        }
        case APE_OBJECT_STRING:
            {
                const char* string = object_get_string(obj);
                if(quote_str)
                {
                    buf->appendFormat("\"%s\"", string);
                }
                else
                {
                    buf->append(string);
                }
            }
            break;
        case APE_OBJECT_NULL:
            {
                buf->append("null");
            }
            break;

        case APE_OBJECT_FUNCTION:
            {
                const ScriptFunction* function = object_get_function(obj);
                buf->appendFormat("CompiledFunction: %s\n", object_get_function_name(obj));
                code_to_string(function->comp_result->bytecode, function->comp_result->src_positions, function->comp_result->m_count, buf);
            }
            break;

        case APE_OBJECT_ARRAY:
            {
                buf->append("[");
                for(i = 0; i < object_get_array_length(obj); i++)
                {
                    Object iobj = object_get_array_value(obj, i);
                    object_to_string(iobj, buf, true);
                    if(i < (object_get_array_length(obj) - 1))
                    {
                        buf->append(", ");
                    }
                }
                buf->append("]");
            }
            break;
        case APE_OBJECT_MAP:
            {
                buf->append("{");
                for(i = 0; i < object_get_map_length(obj); i++)
                {
                    Object key = object_get_map_key_at(obj, i);
                    Object val = object_get_map_value_at(obj, i);
                    object_to_string(key, buf, true);
                    buf->append(": ");
                    object_to_string(val, buf, true);
                    if(i < (object_get_map_length(obj) - 1))
                    {
                        buf->append(", ");
                    }
                }
                buf->append("}");
            }
            break;
        case APE_OBJECT_NATIVE_FUNCTION:
            {
                buf->append("NATIVE_FUNCTION");
            }
            break;
        case APE_OBJECT_EXTERNAL:
            {
                buf->append("EXTERNAL");
            }
            break;

        case APE_OBJECT_ERROR:
            {
                buf->appendFormat("ERROR: %s\n", object_get_error_message(obj));
                Traceback* traceback = object_get_error_traceback(obj);
                APE_ASSERT(traceback);
                if(traceback)
                {
                    buf->append("Traceback:\n");
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
            buf->appendFormat("%s in %s on %d:%d\n", item->function_name, filename, item->pos.line, item->pos.column);
        }
        else
        {
            buf->appendFormat("%s\n", item->function_name);
        }
    }
    return !buf->failed();
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
    char* res = (char*)alloc->allocate(to_write + 1);
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
    char* output_string = (char*)alloc->allocate(n + 1);
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
    char* output_string = (char*)alloc->allocate(n + 1);
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
            alloc->release(line);
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
    alloc->release(rest);
    if(res)
    {
        for(i = 0; i < res->count(); i++)
        {
            line = (char*)res->get(i);
            alloc->release(line);
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
    res = StringBuffer::make(alloc);
    if(!res)
    {
        return NULL;
    }
    for(i = 0; i < items->count(); i++)
    {
        item = (char*)items->get(i);
        res->append(item);
        if(i < (items->count() - 1))
        {
            res->append(with);
        }
    }
    return res->getStringAndDestroy();
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
            alloc->release(item);
            split->removeAt(i);
            i = -1;
            continue;
        }
        if(kg_streq(next_item, ".."))
        {
            alloc->release(item);
            alloc->release(next_item);
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
        alloc->release(item);
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




CompiledFile* compiled_file_make(Allocator* alloc, const char* path)
{
    CompiledFile* file = alloc->allocate<CompiledFile>();
    if(!file)
    {
        return NULL;
    }
    memset(file, 0, sizeof(CompiledFile));
    file->m_alloc = alloc;
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
        file->m_alloc->release(item);
    }
    file->lines->destroy();
    file->m_alloc->release(file->dir_path);
    file->m_alloc->release(file->path);
    file->m_alloc->release(file);
}

GlobalStore* global_store_make(Allocator* alloc, GCMemory* mem)
{
    int i;
    bool ok;
    const char* name;
    Object builtin;
    GlobalStore* store;
    store = alloc->allocate<GlobalStore>();
    if(!store)
    {
        return NULL;
    }
    memset(store, 0, sizeof(GlobalStore));
    store->m_alloc = alloc;
    store->symbols = Dictionary::make(alloc, Symbol::callback_copy, Symbol::callback_destroy);
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
            builtin = Object::makeNativeFunctionMemory(mem, name, builtins_get_fn(i), NULL, 0);
            if(builtin.isNull())
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
    store->m_alloc->release(store);
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
        return Object::makeNull();
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
    symbol = Symbol::make(store->m_alloc, name, SYMBOL_APE_GLOBAL, ix, false);
    if(!symbol)
    {
        goto err;
    }
    ok = store->symbols->set(name, symbol);
    if(!ok)
    {
        symbol->destroy();
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
        return Object::makeNull();
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



// INTERNAL
BlockScope* block_scope_make(Allocator* alloc, int offset)
{
    BlockScope* new_scope;
    new_scope = alloc->allocate<BlockScope>();
    if(!new_scope)
    {
        return NULL;
    }
    memset(new_scope, 0, sizeof(BlockScope));
    new_scope->m_alloc = alloc;
    new_scope->store = Dictionary::make(alloc, Symbol::callback_copy, Symbol::callback_destroy);
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
    scope->m_alloc->release(scope);
}

BlockScope* block_scope_copy(BlockScope* scope)
{
    BlockScope* copy;
    copy = scope->m_alloc->allocate<BlockScope>();
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(BlockScope));
    copy->m_alloc = scope->m_alloc;
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
    CompilationScope* scope = alloc->allocate<CompilationScope>();
    if(!scope)
    {
        return NULL;
    }
    memset(scope, 0, sizeof(CompilationScope));
    scope->m_alloc = alloc;
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
    scope->m_alloc->release(scope);
}

CompilationResult* compilation_scope_orphan_result(CompilationScope* scope)
{
    CompilationResult* res;
    res = compilation_result_make(scope->m_alloc,
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
    res = alloc->allocate<CompilationResult>();
    if(!res)
    {
        return NULL;
    }
    memset(res, 0, sizeof(CompilationResult));
    res->m_alloc = alloc;
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
    res->m_alloc->release(res->bytecode);
    res->m_alloc->release(res->src_positions);
    res->m_alloc->release(res);
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

    Allocator* alloc = expr->m_alloc;
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
                res = Expression::makeNumberLiteral(alloc, left_val + right_val);
                break;
            }
            case OPERATOR_MINUS:
            {
                res = Expression::makeNumberLiteral(alloc, left_val - right_val);
                break;
            }
            case OPERATOR_ASTERISK:
            {
                res = Expression::makeNumberLiteral(alloc, left_val * right_val);
                break;
            }
            case OPERATOR_SLASH:
            {
                res = Expression::makeNumberLiteral(alloc, left_val / right_val);
                break;
            }
            case OPERATOR_LT:
            {
                res = Expression::makeBoolLiteral(alloc, left_val < right_val);
                break;
            }
            case OPERATOR_LTE:
            {
                res = Expression::makeBoolLiteral(alloc, left_val <= right_val);
                break;
            }
            case OPERATOR_GT:
            {
                res = Expression::makeBoolLiteral(alloc, left_val > right_val);
                break;
            }
            case OPERATOR_GTE:
            {
                res = Expression::makeBoolLiteral(alloc, left_val >= right_val);
                break;
            }
            case OPERATOR_EQ:
            {
                res = Expression::makeBoolLiteral(alloc, APE_DBLEQ(left_val, right_val));
                break;
            }
            case OPERATOR_NOT_EQ:
            {
                res = Expression::makeBoolLiteral(alloc, !APE_DBLEQ(left_val, right_val));
                break;
            }
            case OPERATOR_MODULUS:
            {
                res = Expression::makeNumberLiteral(alloc, fmod(left_val, right_val));
                break;
            }
            case OPERATOR_BIT_AND:
            {
                res = Expression::makeNumberLiteral(alloc, (double)(left_val_int & right_val_int));
                break;
            }
            case OPERATOR_BIT_OR:
            {
                res = Expression::makeNumberLiteral(alloc, (double)(left_val_int | right_val_int));
                break;
            }
            case OPERATOR_BIT_XOR:
            {
                res = Expression::makeNumberLiteral(alloc, (double)(left_val_int ^ right_val_int));
                break;
            }
            case OPERATOR_LSHIFT:
            {
                res = Expression::makeNumberLiteral(alloc, (double)(left_val_int << right_val_int));
                break;
            }
            case OPERATOR_RSHIFT:
            {
                res = Expression::makeNumberLiteral(alloc, (double)(left_val_int >> right_val_int));
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
            res = Expression::makeStringLiteral(alloc, res_str);
            if(!res)
            {
                alloc->release(res_str);
            }
        }
    }

    Expression::destroyExpression(left_optimised);
    Expression::destroyExpression(right_optimised);

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
        res = Expression::makeNumberLiteral(expr->m_alloc, -right->number_literal);
    }
    else if(expr->prefix.op == OPERATOR_BANG && right->type == EXPRESSION_BOOL_LITERAL)
    {
        res = Expression::makeBoolLiteral(expr->m_alloc, !right->bool_literal);
    }
    Expression::destroyExpression(right_optimised);
    if(res)
    {
        res->pos = expr->pos;
    }
    return res;
}


Module* module_make(Allocator* alloc, const char* name)
{
    Module* module = alloc->allocate<Module>();
    if(!module)
    {
        return NULL;
    }
    memset(module, 0, sizeof(Module));
    module->m_alloc = alloc;
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
    module->m_alloc->release(module->name);
    module->symbols->destroyWithItems(Symbol::callback_destroy);
    module->m_alloc->release(module);
}

Module* module_copy(Module* src)
{
    Module* copy = src->m_alloc->allocate<Module>();
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(Module));
    copy->m_alloc = src->m_alloc;
    copy->name = ape_strdup(copy->m_alloc, src->name);
    if(!copy->name)
    {
        module_destroy(copy);
        return NULL;
    }
    copy->symbols = src->symbols->copyWithItems(Symbol::callback_copy, Symbol::callback_destroy);
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
    StringBuffer* name_buf = StringBuffer::make(module->m_alloc);
    if(!name_buf)
    {
        return false;
    }
    bool ok = name_buf->appendFormat("%s::%s", module->name, symbol->name);
    if(!ok)
    {
        name_buf->destroy();
        return false;
    }
    Symbol* module_symbol = Symbol::make(module->m_alloc, name_buf->string(), SYMBOL_MODULE_GLOBAL, symbol->index, false);
    name_buf->destroy();
    if(!module_symbol)
    {
        return false;
    }
    ok = module->symbols->add(module_symbol);
    if(!ok)
    {
        module_symbol->destroy();
        return false;
    }
    return true;
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
                data->mem->m_alloc->release(data->string.value_allocated);
            }
            break;
        }
        case APE_OBJECT_FUNCTION:
        {
            if(data->function.owns_data)
            {
                data->mem->m_alloc->release(data->function.name);
                compilation_result_destroy(data->function.comp_result);
            }
            if(freevals_are_allocated(&data->function))
            {
                data->mem->m_alloc->release(data->function.free_vals_allocated);
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
            data->mem->m_alloc->release(data->native_function.name);
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
            data->mem->m_alloc->release(data->error.message);
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
    StringBuffer* res = StringBuffer::make(alloc);
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
                res->append("|");                 \
            }                                            \
            res->append(object_get_type_name(t)); \
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

    return res->getStringAndDestroy();
}

char* object_serialize(Allocator* alloc, Object object, size_t* lendest)
{
    size_t l;
    char* string;
    StringBuffer* buf;
    buf = StringBuffer::make(alloc);
    if(!buf)
    {
        return NULL;
    }
    object_to_string(object, buf, true);
    l = buf->len;
    string = buf->getStringAndDestroy();
    if(lendest != NULL)
    {
        *lendest = l;
    }
    return string;
}

Object object_deep_copy(GCMemory* mem, Object obj)
{
    ValDictionary* copies = ValDictionary::make<Object, Object>(mem->m_alloc);
    if(!copies)
    {
        return Object::makeNull();
    }
    Object res = object_deep_copy_internal(mem, obj, copies);
    copies->destroy();
    return res;
}

Object object_copy(GCMemory* mem, Object obj)
{
    Object copy = Object::makeNull();
    ObjectType type = obj.type();
    switch(type)
    {
        case APE_OBJECT_ANY:
        case APE_OBJECT_FREED:
        case APE_OBJECT_NONE:
        {
            APE_ASSERT(false);
            copy = Object::makeNull();
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
            copy = Object::makeString(mem, str);
            break;
        }
        case APE_OBJECT_ARRAY:
        {
            int len = object_get_array_length(obj);
            copy = Object::makeArray(mem, len);
            if(copy.isNull())
            {
                return Object::makeNull();
            }
            for(int i = 0; i < len; i++)
            {
                Object item = object_get_array_value(obj, i);
                bool ok = object_add_array_value(copy, item);
                if(!ok)
                {
                    return Object::makeNull();
                }
            }
            break;
        }
        case APE_OBJECT_MAP:
        {
            copy = Object::makeMap(mem);
            for(int i = 0; i < object_get_map_length(obj); i++)
            {
                Object key = (Object)object_get_map_key_at(obj, i);
                Object val = (Object)object_get_map_value_at(obj, i);
                bool ok = object_set_map_value_object(copy, key, val);
                if(!ok)
                {
                    return Object::makeNull();
                }
            }
            break;
        }
        case APE_OBJECT_EXTERNAL:
        {
            copy = Object::makeExternal(mem, NULL);
            if(copy.isNull())
            {
                return Object::makeNull();
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

    ObjectType a_type = a.type();
    ObjectType b_type = b.type();

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
    else if((GCMemory::objectIsAllocated(a) || a.isNull()) && (GCMemory::objectIsAllocated(b) || b.isNull()))
    {
        intptr_t a_data_val = (intptr_t)a.getAllocatedData();
        intptr_t b_data_val = (intptr_t)b.getAllocatedData();
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
    ObjectType a_type = a.type();
    ObjectType b_type = b.type();

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
    APE_ASSERT(object.type() == APE_OBJECT_EXTERNAL);
    ObjectData* data = object.getAllocatedData();
    return &data->external;
}

bool object_set_external_destroy_function(Object object, ExternalDataDestroyFNCallback destroy_fn)
{
    APE_ASSERT(object.type() == APE_OBJECT_EXTERNAL);
    ExternalData* data = object_get_external_data(object);
    if(!data)
    {
        return false;
    }
    data->data_destroy_fn = destroy_fn;
    return true;
}



bool object_get_bool(Object obj)
{
    if(obj.isNumber())
    {
        return obj.handle;
    }
    return obj.handle & (~OBJECT_HEADER_MASK);
}

double object_get_number(Object obj)
{
    if(obj.isNumber())
    {// todo: optimise? always return number?
        return obj.number;
    }
    return (double)(obj.handle & (~OBJECT_HEADER_MASK));
}

const char* object_get_string(Object object)
{
    APE_ASSERT(object.type() == APE_OBJECT_STRING);
    ObjectData* data = object.getAllocatedData();
    return object_data_get_string(data);
}

int object_get_string_length(Object object)
{
    APE_ASSERT(object.type() == APE_OBJECT_STRING);
    ObjectData* data = object.getAllocatedData();
    return data->string.length;
}

void object_set_string_length(Object object, int len)
{
    APE_ASSERT(object.type() == APE_OBJECT_STRING);
    ObjectData* data = object.getAllocatedData();
    data->string.length = len;
}


int object_get_string_capacity(Object object)
{
    APE_ASSERT(object.type() == APE_OBJECT_STRING);
    ObjectData* data = object.getAllocatedData();
    return data->string.m_capacity;
}

char* object_get_mutable_string(Object object)
{
    APE_ASSERT(object.type() == APE_OBJECT_STRING);
    ObjectData* data = object.getAllocatedData();
    return object_data_get_string(data);
}

bool object_string_append(Object obj, const char* src, int len)
{
    APE_ASSERT(obj.type() == APE_OBJECT_STRING);
    ObjectData* data = obj.getAllocatedData();
    String* string = &data->string;
    char* str_buf = object_get_mutable_string(obj);
    int current_len = string->length;
    int capacity = string->m_capacity;
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
    APE_ASSERT(obj.type() == APE_OBJECT_STRING);
    ObjectData* data = obj.getAllocatedData();
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

ScriptFunction* object_get_function(Object object)
{
    APE_ASSERT(object.type() == APE_OBJECT_FUNCTION);
    ObjectData* data = object.getAllocatedData();
    return &data->function;
}

NativeFunction* object_get_native_function(Object obj)
{
    ObjectData* data = obj.getAllocatedData();
    return &data->native_function;
}




const char* object_get_function_name(Object obj)
{
    APE_ASSERT(obj.type() == APE_OBJECT_FUNCTION);
    ObjectData* data = obj.getAllocatedData();
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
    APE_ASSERT(obj.type() == APE_OBJECT_FUNCTION);
    ObjectData* data = obj.getAllocatedData();
    APE_ASSERT(data);
    if(!data)
    {
        return Object::makeNull();
    }
    ScriptFunction* fun = &data->function;
    APE_ASSERT(ix >= 0 && ix < fun->free_vals_count);
    if(ix < 0 || ix >= fun->free_vals_count)
    {
        return Object::makeNull();
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
    APE_ASSERT(obj.type() == APE_OBJECT_FUNCTION);
    ObjectData* data = obj.getAllocatedData();
    APE_ASSERT(data);
    if(!data)
    {
        return;
    }
    ScriptFunction* fun = &data->function;
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
    APE_ASSERT(obj.type() == APE_OBJECT_FUNCTION);
    data = obj.getAllocatedData();
    APE_ASSERT(data);
    if(!data)
    {
        return NULL;
    }
    ScriptFunction* fun = &data->function;
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
    APE_ASSERT(object.type() == APE_OBJECT_ERROR);
    data = object.getAllocatedData();
    return data->error.message;
}

void object_set_error_traceback(Object object, Traceback* traceback)
{
    APE_ASSERT(object.type() == APE_OBJECT_ERROR);
    if(object.type() != APE_OBJECT_ERROR)
    {
        return;
    }
    ObjectData* data = object.getAllocatedData();
    APE_ASSERT(data->error.traceback == NULL);
    data->error.traceback = traceback;
}

Traceback* object_get_error_traceback(Object object)
{
    ObjectData* data;
    APE_ASSERT(object.type() == APE_OBJECT_ERROR);
    data = object.getAllocatedData();
    return data->error.traceback;
}

bool object_set_external_data(Object object, void* ext_data)
{
    ExternalData* data;
    APE_ASSERT(object.type() == APE_OBJECT_EXTERNAL);
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
    APE_ASSERT(object.type() == APE_OBJECT_EXTERNAL);
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
    APE_ASSERT(object.type() == APE_OBJECT_ARRAY);
    array = object_get_allocated_array(object);
    if(ix < 0 || ix >= array->count())
    {
        return Object::makeNull();
    }
    res = (Object*)array->get(ix);
    if(!res)
    {
        return Object::makeNull();
    }
    return *res;
}

bool object_set_array_value_at(Object object, int ix, Object val)
{
    Array* array;
    APE_ASSERT(object.type() == APE_OBJECT_ARRAY);
    array = object_get_allocated_array(object);
    if(ix < 0 || ix >= array->count())
    {
        return false;
    }
    return array->set(ix, &val);
}

bool object_add_array_value(Object object, Object val)
{
    APE_ASSERT(object.type() == APE_OBJECT_ARRAY);
    Array* array = object_get_allocated_array(object);
    return array->add(&val);
}

int object_get_array_length(Object object)
{
    APE_ASSERT(object.type() == APE_OBJECT_ARRAY);
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
    APE_ASSERT(object.type() == APE_OBJECT_MAP);
    ObjectData* data = object.getAllocatedData();
    return data->map->count();
}

Object object_get_map_key_at(Object object, int ix)
{
    APE_ASSERT(object.type() == APE_OBJECT_MAP);
    ObjectData* data = object.getAllocatedData();
    Object* res = (Object*)data->map->getKeyAt(ix);
    if(!res)
    {
        return Object::makeNull();
    }
    return *res;
}

Object object_get_map_value_at(Object object, int ix)
{
    APE_ASSERT(object.type() == APE_OBJECT_MAP);
    ObjectData* data = object.getAllocatedData();
    Object* res = (Object*)data->map->getValueAt(ix);
    if(!res)
    {
        return Object::makeNull();
    }
    return *res;
}

bool object_set_map_value_at(Object object, int ix, Object val)
{
    APE_ASSERT(object.type() == APE_OBJECT_MAP);
    if(ix >= object_get_map_length(object))
    {
        return false;
    }
    ObjectData* data = object.getAllocatedData();
    return data->map->setValueAt(ix, &val);
}

Object object_get_kv_pair_at(GCMemory* mem, Object object, int ix)
{
    APE_ASSERT(object.type() == APE_OBJECT_MAP);
    ObjectData* data = object.getAllocatedData();
    if(ix >= data->map->count())
    {
        return Object::makeNull();
    }
    Object key = object_get_map_key_at(object, ix);
    Object val = object_get_map_value_at(object, ix);
    Object res = Object::makeMap(mem);
    if(res.isNull())
    {
        return Object::makeNull();
    }

    Object key_obj = Object::makeString(mem, "key");
    if(key_obj.isNull())
    {
        return Object::makeNull();
    }
    object_set_map_value_object(res, key_obj, key);

    Object val_obj = Object::makeString(mem, "value");
    if(val_obj.isNull())
    {
        return Object::makeNull();
    }
    object_set_map_value_object(res, val_obj, val);

    return res;
}

bool object_set_map_value_object(Object object, Object key, Object val)
{
    APE_ASSERT(object.type() == APE_OBJECT_MAP);
    ObjectData* data = object.getAllocatedData();
    return data->map->set(&key, &val);
}

Object object_get_map_value_object(Object object, Object key)
{
    APE_ASSERT(object.type() == APE_OBJECT_MAP);
    ObjectData* data = object.getAllocatedData();
    Object* res = (Object*)data->map->get(&key);
    if(!res)
    {
        return Object::makeNull();
    }
    return *res;
}

bool object_map_has_key_object(Object object, Object key)
{
    APE_ASSERT(object.type() == APE_OBJECT_MAP);
    ObjectData* data = object.getAllocatedData();
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

    Object copy = Object::makeNull();

    ObjectType type = obj.type();
    switch(type)
    {
        case APE_OBJECT_FREED:
        case APE_OBJECT_ANY:
        case APE_OBJECT_NONE:
            {
                APE_ASSERT(false);
                copy = Object::makeNull();
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
                copy = Object::makeString(mem, str);
            }
            break;

        case APE_OBJECT_FUNCTION:
            {
                ScriptFunction* function = object_get_function(obj);
                uint8_t* bytecode_copy = NULL;
                Position* src_positions_copy = NULL;
                CompilationResult* comp_res_copy = NULL;

                bytecode_copy = (uint8_t*)mem->m_alloc->allocate(sizeof(uint8_t) * function->comp_result->m_count);
                if(!bytecode_copy)
                {
                    return Object::makeNull();
                }
                memcpy(bytecode_copy, function->comp_result->bytecode, sizeof(uint8_t) * function->comp_result->m_count);

                src_positions_copy = (Position*)mem->m_alloc->allocate(sizeof(Position) * function->comp_result->m_count);
                if(!src_positions_copy)
                {
                    mem->m_alloc->release(bytecode_copy);
                    return Object::makeNull();
                }
                memcpy(src_positions_copy, function->comp_result->src_positions, sizeof(Position) * function->comp_result->m_count);

                comp_res_copy = compilation_result_make(mem->m_alloc, bytecode_copy, src_positions_copy,
                                                        function->comp_result->m_count);// todo: add compilation result copy function
                if(!comp_res_copy)
                {
                    mem->m_alloc->release(src_positions_copy);
                    mem->m_alloc->release(bytecode_copy);
                    return Object::makeNull();
                }

                copy = Object::makeFunction(mem, object_get_function_name(obj), comp_res_copy, true, function->num_locals,
                                            function->num_args, 0);
                if(copy.isNull())
                {
                    compilation_result_destroy(comp_res_copy);
                    return Object::makeNull();
                }

                bool ok = copies->set(&obj, &copy);
                if(!ok)
                {
                    return Object::makeNull();
                }

                ScriptFunction* function_copy = object_get_function(copy);
                if(freevals_are_allocated(function))
                {
                    function_copy->free_vals_allocated = (Object*)mem->m_alloc->allocate(sizeof(Object) * function->free_vals_count);
                    if(!function_copy->free_vals_allocated)
                    {
                        return Object::makeNull();
                    }
                }

                function_copy->free_vals_count = function->free_vals_count;
                for(int i = 0; i < function->free_vals_count; i++)
                {
                    Object free_val = object_get_function_free_val(obj, i);
                    Object free_val_copy = object_deep_copy_internal(mem, free_val, copies);
                    if(!free_val.isNull() && free_val_copy.isNull())
                    {
                        return Object::makeNull();
                    }
                    object_set_function_free_val(copy, i, free_val_copy);
                }
            }
            break;

        case APE_OBJECT_ARRAY:
            {
                int len = object_get_array_length(obj);
                copy = Object::makeArray(mem, len);
                if(copy.isNull())
                {
                    return Object::makeNull();
                }
                bool ok = copies->set(&obj, &copy);
                if(!ok)
                {
                    return Object::makeNull();
                }
                for(int i = 0; i < len; i++)
                {
                    Object item = object_get_array_value(obj, i);
                    Object item_copy = object_deep_copy_internal(mem, item, copies);
                    if(!item.isNull() && item_copy.isNull())
                    {
                        return Object::makeNull();
                    }
                    bool ok = object_add_array_value(copy, item_copy);
                    if(!ok)
                    {
                        return Object::makeNull();
                    }
                }
            }
            break;

        case APE_OBJECT_MAP:
            {
                copy = Object::makeMap(mem);
                if(copy.isNull())
                {
                    return Object::makeNull();
                }
                bool ok = copies->set( &obj, &copy);
                if(!ok)
                {
                    return Object::makeNull();
                }
                for(int i = 0; i < object_get_map_length(obj); i++)
                {
                    Object key = object_get_map_key_at(obj, i);
                    Object val = object_get_map_value_at(obj, i);

                    Object key_copy = object_deep_copy_internal(mem, key, copies);
                    if(!key.isNull() && key_copy.isNull())
                    {
                        return Object::makeNull();
                    }

                    Object val_copy = object_deep_copy_internal(mem, val, copies);
                    if(!val.isNull() && val_copy.isNull())
                    {
                        return Object::makeNull();
                    }

                    bool ok = object_set_map_value_object(copy, key_copy, val_copy);
                    if(!ok)
                    {
                        return Object::makeNull();
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
    ObjectType type = obj.type();

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
    APE_ASSERT(object.type() == APE_OBJECT_ARRAY);
    ObjectData* data = object.getAllocatedData();
    return data->array;
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

static bool freevals_are_allocated(ScriptFunction* fun)
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

    String* string = &data->string;

    string->length = 0;
    string->hash = 0;

    if(capacity <= string->m_capacity)
    {
        return true;
    }

    if(capacity <= (OBJECT_STRING_BUF_SIZE - 1))
    {
        if(string->is_allocated)
        {
            APE_ASSERT(false);// should never happen
            data->mem->m_alloc->release(string->value_allocated);// just in case
        }
        string->m_capacity = OBJECT_STRING_BUF_SIZE - 1;
        string->is_allocated = false;
        return true;
    }

    char* new_value = (char*)data->mem->m_alloc->allocate(capacity + 1);
    if(!new_value)
    {
        return false;
    }

    if(string->is_allocated)
    {
        data->mem->m_alloc->release(string->value_allocated);
    }

    string->value_allocated = new_value;
    string->is_allocated = true;
    string->m_capacity = capacity;
    return true;
}


Traceback* traceback_make(Allocator* alloc)
{
    Traceback* traceback = alloc->allocate<Traceback>();
    if(!traceback)
    {
        return NULL;
    }
    memset(traceback, 0, sizeof(Traceback));
    traceback->m_alloc = alloc;
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
        traceback->m_alloc->release(item->function_name);
    }
    Array::destroy(traceback->items);
    traceback->m_alloc->release(traceback);
}

bool traceback_append(Traceback* traceback, const char* function_name, Position pos)
{
    TracebackItem item;
    item.function_name = ape_strdup(traceback->m_alloc, function_name);
    if(!item.function_name)
    {
        return false;
    }
    item.pos = pos;
    bool ok = traceback->items->add(&item);
    if(!ok)
    {
        traceback->m_alloc->release(item.function_name);
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
    if(function_obj.type() != APE_OBJECT_FUNCTION)
    {
        return false;
    }
    ScriptFunction* function = object_get_function(function_obj);
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
    return Position::invalid();
}



//-----------------------------------------------------------------------------
// Ape
//-----------------------------------------------------------------------------



void ape_program_destroy(Program* program)
{
    if(!program)
    {
        return;
    }
    compilation_result_destroy(program->comp_res);
    program->context->m_alloc.release(program);
}




bool ape_object_disable_gc(Object ape_obj)
{
    Object obj = ape_obj;
    return GCMemory::disableFor(obj);
}

void ape_object_enable_gc(Object ape_obj)
{
    Object obj = ape_obj;
    GCMemory::enableFor(obj);
}


//-----------------------------------------------------------------------------
// Ape object array
//-----------------------------------------------------------------------------


const char* ape_object_get_array_string(Object obj, int ix)
{
    Object object;
    object = object_get_array_value(obj, ix);
    if(object.type() != APE_OBJECT_STRING)
    {
        return NULL;
    }
    return object_get_string(object);
}

double ape_object_get_array_number(Object obj, int ix)
{
    Object object;
    object = object_get_array_value(obj, ix);
    if(object.type() != APE_OBJECT_NUMBER)
    {
        return 0;
    }
    return object_get_number(object);
}

bool ape_object_get_array_bool(Object obj, int ix)
{
    Object object = object_get_array_value(obj, ix);
    if(object.type() != APE_OBJECT_BOOL)
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
    GCMemory* mem = GCMemory::objectGetMemory(obj);
    if(!mem)
    {
        return false;
    }
    Object new_value = Object::makeString(mem, string);
    return ape_object_set_array_value(obj, ix, new_value);
}

bool ape_object_set_array_number(Object obj, int ix, double number)
{
    Object new_value = Object::makeNumber(number);
    return ape_object_set_array_value(obj, ix, new_value);
}

bool ape_object_set_array_bool(Object obj, int ix, bool value)
{
    Object new_value = Object::makeBool(value);
    return ape_object_set_array_value(obj, ix, new_value);
}


bool ape_object_add_array_string(Object obj, const char* string)
{
    GCMemory* mem = GCMemory::objectGetMemory(obj);
    if(!mem)
    {
        return false;
    }
    Object new_value = Object::makeString(mem, string);
    return object_add_array_value(obj, new_value);
}

bool ape_object_add_array_number(Object obj, double number)
{
    Object new_value = Object::makeNumber(number);
    return object_add_array_value(obj, new_value);
}

bool ape_object_add_array_bool(Object obj, bool value)
{
    Object new_value = Object::makeBool(value);
    return object_add_array_value(obj, new_value);
}

//-----------------------------------------------------------------------------
// Ape object map
//-----------------------------------------------------------------------------

bool ape_object_set_map_value(Object obj, const char* key, Object value)
{
    GCMemory* mem = GCMemory::objectGetMemory(obj);
    if(!mem)
    {
        return false;
    }
    Object key_object = Object::makeString(mem, key);
    if(key_object.isNull())
    {
        return false;
    }
    return object_set_map_value_object(obj, key_object, value);
}

bool ape_object_set_map_string(Object obj, const char* key, const char* string)
{
    GCMemory* mem = GCMemory::objectGetMemory(obj);
    if(!mem)
    {
        return false;
    }
    Object string_object = Object::makeString(mem, string);
    if(string_object.isNull())
    {
        return false;
    }
    return ape_object_set_map_value(obj, key, string_object);
}

bool ape_object_set_map_number(Object obj, const char* key, double number)
{
    Object number_object = Object::makeNumber(number);
    return ape_object_set_map_value(obj, key, number_object);
}

bool ape_object_set_map_bool(Object obj, const char* key, bool value)
{
    Object bool_object = Object::makeBool(value);
    return ape_object_set_map_value(obj, key, bool_object);
}


Object ape_object_get_map_value(Object object, const char* key)
{
    GCMemory* mem = GCMemory::objectGetMemory(object);
    if(!mem)
    {
        return Object::makeNull();
    }
    Object key_object = Object::makeString(mem, key);
    if(key_object.isNull())
    {
        return Object::makeNull();
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
    GCMemory* mem = GCMemory::objectGetMemory(object);
    if(!mem)
    {
        return false;
    }
    Object key_object = Object::makeString(mem, key);
    if(key_object.isNull())
    {
        return false;
    }
    return object_map_has_key_object(object, key_object);
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


Object ape_native_fn_wrapper(VM* vm, void* data, int argc, Object* args)
{
    Object res;
    NativeFuncWrapper* wrapper;
    (void)vm;
    wrapper = (NativeFuncWrapper*)data;
    APE_ASSERT(vm == wrapper->context->vm);
    res = wrapper->wrapped_funcptr(wrapper->context, wrapper->data, argc, (Object*)args);
    if(wrapper->context->hasErrors())
    {
        return Object::makeNull();
    }
    return res;
}


static char* read_file_default(void* pctx, const char* filename)
{
    long pos;
    size_t size_read;
    size_t size_to_read;
    char* file_contents;
    FILE* fp;
    Context* ctx;
    ctx = (Context*)pctx;
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
    file_contents = (char*)ctx->m_alloc.allocate(sizeof(char) * (size_to_read + 1));
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

static size_t write_file_default(void* pctx, const char* path, const char* string, size_t string_size)
{
    size_t written;
    FILE* fp;
    (void)pctx;
    fp = fopen(path, "w");
    if(!fp)
    {
        return 0;
    }
    written = fwrite(string, 1, string_size, fp);
    fclose(fp);
    return written;
}

static size_t stdout_write_default(void* pctx, const void* data, size_t size)
{
    (void)pctx;
    return fwrite(data, 1, size, stdout);
}

void* ape_malloc(void* pctx, size_t size)
{
    void* res;
    Context* ctx;
    ctx = (Context*)pctx;
    res = (void*)ctx->custom_allocator.allocate(size);
    if(!res)
    {
        ctx->m_errors.addError(APE_ERROR_ALLOCATION, Position::invalid(), "Allocation failed");
    }
    return res;
}

void ape_free(void* pctx, void* ptr)
{
    Context* ctx;
    ctx = (Context*)pctx;
    ctx->custom_allocator.release( ptr);
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
    Object::makeNativeFunctionMemory(vm->mem, name, fnc, dataptr, datasize)

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
            vm->m_errors->addFormat(APE_ERROR_RUNTIME, Position::invalid(),
                              "Invalid number or arguments, got %d instead of %d", argc, expected_argc);
        }
        return false;
    }

    for(int i = 0; i < argc; i++)
    {
        Object arg = args[i];
        ObjectType type = arg.type();
        ObjectType expected_type = expected_types[i];
        if(!(type & expected_type))
        {
            if(generate_error)
            {
                const char* type_str = object_get_type_name(type);
                char* expected_type_str = object_get_type_union_name(vm->m_alloc, expected_type);
                if(!expected_type_str)
                {
                    return false;
                }
                vm->m_errors->addFormat(APE_ERROR_RUNTIME, Position::invalid(),
                                  "Invalid argument %d type, got %s, expected %s", i, type_str, expected_type_str);
                vm->m_alloc->release(expected_type_str);
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
        return Object::makeNull();
    }

    Object arg = args[0];
    ObjectType type = arg.type();
    if(type == APE_OBJECT_STRING)
    {
        int len = object_get_string_length(arg);
        return Object::makeNumber(len);
    }
    else if(type == APE_OBJECT_ARRAY)
    {
        int len = object_get_array_length(arg);
        return Object::makeNumber(len);
    }
    else if(type == APE_OBJECT_MAP)
    {
        int len = object_get_map_length(arg);
        return Object::makeNumber(len);
    }

    return Object::makeNull();
}

static Object cfn_first(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY))
    {
        return Object::makeNull();
    }
    Object arg = args[0];
    return object_get_array_value(arg, 0);
}

static Object cfn_last(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY))
    {
        return Object::makeNull();
    }
    Object arg = args[0];
    return object_get_array_value(arg, object_get_array_length(arg) - 1);
}

static Object cfn_rest(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY))
    {
        return Object::makeNull();
    }
    Object arg = args[0];
    int len = object_get_array_length(arg);
    if(len == 0)
    {
        return Object::makeNull();
    }

    Object res = Object::makeArray(vm->mem);
    if(res.isNull())
    {
        return Object::makeNull();
    }
    for(int i = 1; i < len; i++)
    {
        Object item = object_get_array_value(arg, i);
        bool ok = object_add_array_value(res, item);
        if(!ok)
        {
            return Object::makeNull();
        }
    }
    return res;
}

static Object cfn_reverse(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY | APE_OBJECT_STRING))
    {
        return Object::makeNull();
    }
    Object arg = args[0];
    ObjectType type = arg.type();
    if(type == APE_OBJECT_ARRAY)
    {
        int len = object_get_array_length(arg);
        Object res = Object::makeArray(vm->mem, len);
        if(res.isNull())
        {
            return Object::makeNull();
        }
        for(int i = 0; i < len; i++)
        {
            Object obj = object_get_array_value(arg, i);
            bool ok = object_set_array_value_at(res, len - i - 1, obj);
            if(!ok)
            {
                return Object::makeNull();
            }
        }
        return res;
    }
    else if(type == APE_OBJECT_STRING)
    {
        const char* str = object_get_string(arg);
        int len = object_get_string_length(arg);

        Object res = Object::makeStringCapacity(vm->mem, len);
        if(res.isNull())
        {
            return Object::makeNull();
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
    return Object::makeNull();
}

static Object cfn_array(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(argc == 1)
    {
        if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
        {
            return Object::makeNull();
        }
        int capacity = (int)object_get_number(args[0]);
        Object res = Object::makeArray(vm->mem, capacity);
        if(res.isNull())
        {
            return Object::makeNull();
        }
        Object obj_null = Object::makeNull();
        for(int i = 0; i < capacity; i++)
        {
            bool ok = object_add_array_value(res, obj_null);
            if(!ok)
            {
                return Object::makeNull();
            }
        }
        return res;
    }
    else if(argc == 2)
    {
        if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER, APE_OBJECT_ANY))
        {
            return Object::makeNull();
        }
        int capacity = (int)object_get_number(args[0]);
        Object res = Object::makeArray(vm->mem, capacity);
        if(res.isNull())
        {
            return Object::makeNull();
        }
        for(int i = 0; i < capacity; i++)
        {
            bool ok = object_add_array_value(res, args[1]);
            if(!ok)
            {
                return Object::makeNull();
            }
        }
        return res;
    }
    CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER);
    return Object::makeNull();
}

static Object cfn_append(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY, APE_OBJECT_ANY))
    {
        return Object::makeNull();
    }
    bool ok = object_add_array_value(args[0], args[1]);
    if(!ok)
    {
        return Object::makeNull();
    }
    int len = object_get_array_length(args[0]);
    return Object::makeNumber(len);
}

static Object cfn_println(VM* vm, void* data, int argc, Object* args)
{
    (void)data;

    const Config* config = vm->config;

    if(!config->stdio.write.write)
    {
        return Object::makeNull();// todo: runtime error?
    }

    StringBuffer* buf = StringBuffer::make(vm->m_alloc);
    if(!buf)
    {
        return Object::makeNull();
    }
    for(int i = 0; i < argc; i++)
    {
        Object arg = args[i];
        object_to_string(arg, buf, false);
    }
    buf->append("\n");
    if(buf->failed())
    {
        buf->destroy();
        return Object::makeNull();
    }
    config->stdio.write.write(config->stdio.write.context, buf->string(), buf->length());
    buf->destroy();
    return Object::makeNull();
}

static Object cfn_print(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    const Config* config = vm->config;

    if(!config->stdio.write.write)
    {
        return Object::makeNull();// todo: runtime error?
    }

    StringBuffer* buf = StringBuffer::make(vm->m_alloc);
    if(!buf)
    {
        return Object::makeNull();
    }
    for(int i = 0; i < argc; i++)
    {
        Object arg = args[i];
        object_to_string(arg, buf, false);
    }
    if(buf->failed())
    {
        buf->destroy();
        return Object::makeNull();
    }
    config->stdio.write.write(config->stdio.write.context, buf->string(), buf->length());
    buf->destroy();
    return Object::makeNull();
}

static Object cfn_to_str(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING | APE_OBJECT_NUMBER | APE_OBJECT_BOOL | APE_OBJECT_NULL | APE_OBJECT_MAP | APE_OBJECT_ARRAY))
    {
        return Object::makeNull();
    }
    Object arg = args[0];
    StringBuffer* buf = StringBuffer::make(vm->m_alloc);
    if(!buf)
    {
        return Object::makeNull();
    }
    object_to_string(arg, buf, false);
    if(buf->failed())
    {
        buf->destroy();
        return Object::makeNull();
    }
    Object res = Object::makeString(vm->mem, buf->string());
    buf->destroy();
    return res;
}

static Object cfn_to_num(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING | APE_OBJECT_NUMBER | APE_OBJECT_BOOL | APE_OBJECT_NULL))
    {
        return Object::makeNull();
    }
    double result = 0;
    const char* string = "";
    if(args[0].isNumeric())
    {
        result = object_get_number(args[0]);
    }
    else if(args[0].isNull())
    {
        result = 0;
    }
    else if(args[0].type() == APE_OBJECT_STRING)
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
    return Object::makeNumber(result);
err:
    vm->m_errors->addFormat(APE_ERROR_RUNTIME, Position::invalid(), "Cannot convert \"%s\" to number", string);
    return Object::makeNull();
}

static Object cfn_chr(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return Object::makeNull();
    }

    double val = object_get_number(args[0]);

    char c = (char)val;
    char str[2] = { c, '\0' };
    return Object::makeString(vm->mem, str);
}

static Object cfn_range(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    for(int i = 0; i < argc; i++)
    {
        ObjectType type = args[i].type();
        if(type != APE_OBJECT_NUMBER)
        {
            const char* type_str = object_get_type_name(type);
            const char* expected_str = object_get_type_name(APE_OBJECT_NUMBER);
            vm->m_errors->addFormat(APE_ERROR_RUNTIME, Position::invalid(),
                              "Invalid argument %d passed to range, got %s instead of %s", i, type_str, expected_str);
            return Object::makeNull();
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
        vm->m_errors->addFormat(APE_ERROR_RUNTIME, Position::invalid(), "Invalid number of arguments passed to range, got %d", argc);
        return Object::makeNull();
    }

    if(step == 0)
    {
        vm->m_errors->addError(APE_ERROR_RUNTIME, Position::invalid(), "range step cannot be 0");
        return Object::makeNull();
    }

    Object res = Object::makeArray(vm->mem);
    if(res.isNull())
    {
        return Object::makeNull();
    }
    for(int i = start; i < end; i += step)
    {
        Object item = Object::makeNumber(i);
        bool ok = object_add_array_value(res, item);
        if(!ok)
        {
            return Object::makeNull();
        }
    }
    return res;
}

static Object cfn_keys(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_MAP))
    {
        return Object::makeNull();
    }
    Object arg = args[0];
    Object res = Object::makeArray(vm->mem);
    if(res.isNull())
    {
        return Object::makeNull();
    }
    int len = object_get_map_length(arg);
    for(int i = 0; i < len; i++)
    {
        Object key = object_get_map_key_at(arg, i);
        bool ok = object_add_array_value(res, key);
        if(!ok)
        {
            return Object::makeNull();
        }
    }
    return res;
}

static Object cfn_values(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_MAP))
    {
        return Object::makeNull();
    }
    Object arg = args[0];
    Object res = Object::makeArray(vm->mem);
    if(res.isNull())
    {
        return Object::makeNull();
    }
    int len = object_get_map_length(arg);
    for(int i = 0; i < len; i++)
    {
        Object key = object_get_map_value_at(arg, i);
        bool ok = object_add_array_value(res, key);
        if(!ok)
        {
            return Object::makeNull();
        }
    }
    return res;
}

static Object cfn_copy(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return Object::makeNull();
    }
    return object_copy(vm->mem, args[0]);
}

static Object cfn_deep_copy(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return Object::makeNull();
    }
    return object_deep_copy(vm->mem, args[0]);
}

static Object cfn_concat(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY | APE_OBJECT_STRING, APE_OBJECT_ARRAY | APE_OBJECT_STRING))
    {
        return Object::makeNull();
    }
    ObjectType type = args[0].type();
    ObjectType item_type = args[1].type();
    if(type == APE_OBJECT_ARRAY)
    {
        if(item_type != APE_OBJECT_ARRAY)
        {
            const char* item_type_str = object_get_type_name(item_type);
            vm->m_errors->addFormat( APE_ERROR_RUNTIME, Position::invalid(), "Invalid argument 2 passed to concat, got %s", item_type_str);
            return Object::makeNull();
        }
        for(int i = 0; i < object_get_array_length(args[1]); i++)
        {
            Object item = object_get_array_value(args[1], i);
            bool ok = object_add_array_value(args[0], item);
            if(!ok)
            {
                return Object::makeNull();
            }
        }
        return Object::makeNumber(object_get_array_length(args[0]));
    }
    else if(type == APE_OBJECT_STRING)
    {
        if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING, APE_OBJECT_STRING))
        {
            return Object::makeNull();
        }
        const char* left_val = object_get_string(args[0]);
        int left_len = (int)object_get_string_length(args[0]);

        const char* right_val = object_get_string(args[1]);
        int right_len = (int)object_get_string_length(args[1]);

        Object res = Object::makeStringCapacity(vm->mem, left_len + right_len);
        if(res.isNull())
        {
            return Object::makeNull();
        }

        bool ok = object_string_append(res, left_val, left_len);
        if(!ok)
        {
            return Object::makeNull();
        }

        ok = object_string_append(res, right_val, right_len);
        if(!ok)
        {
            return Object::makeNull();
        }

        return res;
    }
    return Object::makeNull();
}

static Object cfn_remove(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY, APE_OBJECT_ANY))
    {
        return Object::makeNull();
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
        return Object::makeBool(false);
    }

    bool res = object_remove_array_value_at(args[0], ix);
    return Object::makeBool(res);
}

static Object cfn_remove_at(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY, APE_OBJECT_NUMBER))
    {
        return Object::makeNull();
    }

    ObjectType type = args[0].type();
    int ix = (int)object_get_number(args[1]);

    switch(type)
    {
        case APE_OBJECT_ARRAY:
        {
            bool res = object_remove_array_value_at(args[0], ix);
            return Object::makeBool(res);
        }
        default:
            break;
    }

    return Object::makeBool(true);
}


static Object cfn_error(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(argc == 1 && args[0].type() == APE_OBJECT_STRING)
    {
        return Object::makeError(vm->mem, object_get_string(args[0]));
    }
    else
    {
        return Object::makeError(vm->mem, "");
    }
}

static Object cfn_crash(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(argc == 1 && args[0].type() == APE_OBJECT_STRING)
    {
        vm->m_errors->addError(APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), object_get_string(args[0]));
    }
    else
    {
        vm->m_errors->addError(APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "");
    }
    return Object::makeNull();
}

static Object cfn_assert(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_BOOL))
    {
        return Object::makeNull();
    }

    if(!object_get_bool(args[0]))
    {
        vm->m_errors->addError(APE_ERROR_RUNTIME, Position::invalid(), "assertion failed");
        return Object::makeNull();
    }

    return Object::makeBool(true);
}

static Object cfn_random_seed(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return Object::makeNull();
    }
    int seed = (int)object_get_number(args[0]);
    srand(seed);
    return Object::makeBool(true);
}

static Object cfn_random(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    double res = (double)rand() / RAND_MAX;
    if(argc == 0)
    {
        return Object::makeNumber(res);
    }
    else if(argc == 2)
    {
        if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER, APE_OBJECT_NUMBER))
        {
            return Object::makeNull();
        }
        double min = object_get_number(args[0]);
        double max = object_get_number(args[1]);
        if(min >= max)
        {
            vm->m_errors->addError(APE_ERROR_RUNTIME, Position::invalid(), "max is bigger than min");
            return Object::makeNull();
        }
        double range = max - min;
        res = min + (res * range);
        return Object::makeNumber(res);
    }
    else
    {
        vm->m_errors->addError(APE_ERROR_RUNTIME, Position::invalid(), "Invalid number or arguments");
        return Object::makeNull();
    }
}

static Object cfn_slice(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING | APE_OBJECT_ARRAY, APE_OBJECT_NUMBER))
    {
        return Object::makeNull();
    }
    ObjectType arg_type = args[0].type();
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
        Object res = Object::makeArray(vm->mem, len - index);
        if(res.isNull())
        {
            return Object::makeNull();
        }
        for(int i = index; i < len; i++)
        {
            Object item = object_get_array_value(args[0], i);
            bool ok = object_add_array_value(res, item);
            if(!ok)
            {
                return Object::makeNull();
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
                return Object::makeString(vm->mem, "");
            }
        }
        if(index >= len)
        {
            return Object::makeString(vm->mem, "");
        }
        int res_len = len - index;
        Object res = Object::makeStringCapacity(vm->mem, res_len);
        if(res.isNull())
        {
            return Object::makeNull();
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
        vm->m_errors->addFormat( APE_ERROR_RUNTIME, Position::invalid(), "Invalid argument 0 passed to slice, got %s instead", type_str);
        return Object::makeNull();
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
        return Object::makeNull();
    }
    return Object::makeBool(args[0].type() == APE_OBJECT_STRING);
}

static Object cfn_is_array(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return Object::makeNull();
    }
    return Object::makeBool(args[0].type() == APE_OBJECT_ARRAY);
}

static Object cfn_is_map(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return Object::makeNull();
    }
    return Object::makeBool(args[0].type() == APE_OBJECT_MAP);
}

static Object cfn_is_number(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return Object::makeNull();
    }
    return Object::makeBool(args[0].type() == APE_OBJECT_NUMBER);
}

static Object cfn_is_bool(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return Object::makeNull();
    }
    return Object::makeBool(args[0].type() == APE_OBJECT_BOOL);
}

static Object cfn_is_null(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return Object::makeNull();
    }
    return Object::makeBool(args[0].type() == APE_OBJECT_NULL);
}

static Object cfn_is_function(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return Object::makeNull();
    }
    return Object::makeBool(args[0].type() == APE_OBJECT_FUNCTION);
}

static Object cfn_is_external(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return Object::makeNull();
    }
    return Object::makeBool(args[0].type() == APE_OBJECT_EXTERNAL);
}

static Object cfn_is_error(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return Object::makeNull();
    }
    return Object::makeBool(args[0].type() == APE_OBJECT_ERROR);
}

static Object cfn_is_native_function(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return Object::makeNull();
    }
    return Object::makeBool(args[0].type() == APE_OBJECT_NATIVE_FUNCTION);
}

//-----------------------------------------------------------------------------
// Math
//-----------------------------------------------------------------------------

static Object cfn_sqrt(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return Object::makeNull();
    }
    double arg = object_get_number(args[0]);
    double res = sqrt(arg);
    return Object::makeNumber(res);
}

static Object cfn_pow(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER, APE_OBJECT_NUMBER))
    {
        return Object::makeNull();
    }
    double arg1 = object_get_number(args[0]);
    double arg2 = object_get_number(args[1]);
    double res = pow(arg1, arg2);
    return Object::makeNumber(res);
}

static Object cfn_sin(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return Object::makeNull();
    }
    double arg = object_get_number(args[0]);
    double res = sin(arg);
    return Object::makeNumber(res);
}

static Object cfn_cos(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return Object::makeNull();
    }
    double arg = object_get_number(args[0]);
    double res = cos(arg);
    return Object::makeNumber(res);
}

static Object cfn_tan(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return Object::makeNull();
    }
    double arg = object_get_number(args[0]);
    double res = tan(arg);
    return Object::makeNumber(res);
}

static Object cfn_log(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return Object::makeNull();
    }
    double arg = object_get_number(args[0]);
    double res = log(arg);
    return Object::makeNumber(res);
}

static Object cfn_ceil(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return Object::makeNull();
    }
    double arg = object_get_number(args[0]);
    double res = ceil(arg);
    return Object::makeNumber(res);
}

static Object cfn_floor(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return Object::makeNull();
    }
    double arg = object_get_number(args[0]);
    double res = floor(arg);
    return Object::makeNumber(res);
}

static Object cfn_abs(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return Object::makeNull();
    }
    double arg = object_get_number(args[0]);
    double res = fabs(arg);
    return Object::makeNumber(res);
}


static Object cfn_file_write(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING, APE_OBJECT_STRING))
    {
        return Object::makeNull();
    }

    const Config* config = vm->config;

    if(!config->fileio.write_file.write_file)
    {
        return Object::makeNull();
    }

    const char* path = object_get_string(args[0]);
    const char* string = object_get_string(args[1]);
    int string_len = object_get_string_length(args[1]);

    int written = (int)config->fileio.write_file.write_file(config->fileio.write_file.context, path, string, string_len);

    return Object::makeNumber(written);
}

static Object cfn_file_read(VM* vm, void* data, int argc, Object* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
    {
        return Object::makeNull();
    }

    const Config* config = vm->config;

    if(!config->fileio.read_file.read_file)
    {
        return Object::makeNull();
    }

    const char* path = object_get_string(args[0]);

    char* contents = config->fileio.read_file.read_file(config->fileio.read_file.context, path);
    if(!contents)
    {
        return Object::makeNull();
    }
    Object res = Object::makeString(vm->mem, contents);
    vm->m_alloc->release(contents);
    return res;
}

static Object cfn_file_isfile(VM* vm, void* data, int argc, Object* args)
{
    const char* path;
    struct stat st;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
    {
        return Object::makeBool(false);
    }
    path = object_get_string(args[0]);
    if(stat(path, &st) != -1)
    {
        if((st.st_mode & S_IFMT) == S_IFREG)
        {
            return Object::makeBool(true);
        }
    }
    return Object::makeBool(false);
}

static Object cfn_file_isdirectory(VM* vm, void* data, int argc, Object* args)
{
    const char* path;
    struct stat st;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
    {
        return Object::makeBool(false);
    }
    path = object_get_string(args[0]);
    if(stat(path, &st) != -1)
    {
        if((st.st_mode & S_IFMT) == S_IFDIR)
        {
            return Object::makeBool(true);
        }
    }
    return Object::makeBool(false);
}

static Object timespec_to_map(VM* vm, struct timespec ts)
{
    Object map;
    map = Object::makeMap(vm->mem);
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
        return Object::makeBool(false);
    }
    path = object_get_string(args[0]);
    if(stat(path, &st) != -1)
    {
        map = Object::makeMap(vm->mem);
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
    return Object::makeNull();
}

static Object objfn_string_length(VM* vm, void* data, int argc, Object* args)
{
    Object self;
    (void)vm;
    (void)data;
    (void)argc;
    self = args[0];
    return Object::makeNumber(object_get_string_length(self));
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
        return Object::makeNull();
    }
    path = object_get_string(args[0]);
    hnd = opendir(path);
    if(hnd == NULL)
    {
        return Object::makeNull();
    }
    ary = Object::makeArray(vm->mem);
    while(true)
    {
        dent = readdir(hnd);
        if(dent == NULL)
        {
            break;
        }
        isfile = (dent->d_type == DT_REG);
        isdir = (dent->d_type == DT_DIR);
        subm = Object::makeMap(vm->mem);
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
        return Object::makeNull();
    }
    str = object_get_string(args[0]);
    delim = object_get_string(args[1]);
    arr = Object::makeArray(vm->mem);
    if(object_get_string_length(args[1]) == 0)
    {
        len = object_get_string_length(args[0]);
        for(i=0; i<len; i++)
        {
            c = str[i];
            object_add_array_value(arr, Object::makeString(vm->mem, &c, 1));
        }
    }
    else
    {
        parr = kg_split_string(vm->m_alloc, str, delim);
        for(i=0; i<parr->count(); i++)
        {
            itm = (char*)parr->get(i);
            object_add_array_value(arr, Object::makeString(vm->mem, itm));
            vm->m_alloc->release((void*)itm);
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
    map = Object::makeMap(vm->mem);
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



static void print_errors(Context* ctx)
{
    int i;
    int cn;
    char *err_str;
    const ErrorList::Error *err;
    cn = ctx->errorCount();
    for (i = 0; i < cn; i++)
    {
        err = ctx->getError(i);
        err_str = ctx->serializeError(err);
        fprintf(stderr, "%s", err_str);
        ctx->releaseAllocated(err_str);
    }
}

static Object exit_repl(Context* ctx, void *data, int argc, Object *args)
{
    bool *exit_repl;
    (void)ctx;
    (void)argc;
    (void)args;
    exit_repl = (bool*)data;
    *exit_repl = true;
    return Object::makeNull();
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

static void do_repl(Context* ctx)
{
    size_t len;
    char *line;
    char *object_str;
    Object res;
    ctx->setREPLMode(true);
    ctx->setTimeout(100.0);
    while(true)
    {
        line = readline(">> ");
        if(!line || !notjustspace(line))
        {
            continue;
        }
        add_history(line);
        res = ctx->executeCode(line);
        if (ctx->hasErrors())
        {
            print_errors(ctx);
            free(line);
        }
        else
        {
            object_str = object_serialize(&ctx->m_alloc, res, &len);
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
    Context* ctx;
    Object args_array;
    replexit = false;
    populate_flags(argc, 1, argv, "epI", &fx);
    ctx = Context::make();
    if(!parse_options(&opts, fx.flags, fx.fcnt))
    {
        fprintf(stderr, "failed to process command line flags.\n");
        return 1;
    }
    ctx->setNativeFunction("exit", exit_repl, &replexit);
    if((fx.poscnt > 0) || (opts.codeline != NULL))
    {
        args_array = Object::makeArray(ctx->mem);
        for(i=0; i<fx.poscnt; i++)
        {
            ape_object_add_array_string(args_array, fx.positional[i]);
        }
        ctx->setGlobalConstant("args", args_array);
        if(opts.codeline)
        {
            ctx->executeCode(opts.codeline);
        }
        else
        {
            filename = fx.positional[0];
            ctx->executeFile(filename);
        }
        if(ctx->hasErrors())
        {
            print_errors(ctx);
        }
    }
    else
    {
        #if !defined(NO_READLINE)
            do_repl(ctx);
        #else
            fprintf(stderr, "no repl support compiled in\n");
        #endif
    }
    ctx->destroy();
    return 0;
}

