#ifndef symbol_table_h
#define symbol_table_h

#ifndef APE_AMALGAMATED
#include "common.h"
#include "token.h"
#include "collections.h"
#endif

typedef enum symbol_type {
    SYMBOL_NONE = 0,
    SYMBOL_GLOBAL,
    SYMBOL_LOCAL,
    SYMBOL_NATIVE_FUNCTION,
    SYMBOL_FREE,
    SYMBOL_FUNCTION,
    SYMBOL_THIS,
} symbol_type_t;

typedef struct symbol {
    symbol_type_t type;
    char *name;
    int index;
    bool assignable;
} symbol_t;

typedef struct block_scope {
    dict(symbol_t) *store;
    int offset;
    int num_definitions;
} block_scope_t;

typedef struct symbol_table {
    struct symbol_table *outer;
    ptrarray(block_scope_t) *block_scopes;
    ptrarray(symbol_t) *free_symbols;
    int max_num_definitions;
} symbol_table_t;

APE_INTERNAL symbol_t *symbol_make(const char *name, symbol_type_t type, int index, bool assignable);
APE_INTERNAL void symbol_destroy(symbol_t *symbol);
APE_INTERNAL symbol_t* symbol_copy(const symbol_t *symbol);

APE_INTERNAL symbol_table_t *symbol_table_make(symbol_table_t *outer);
APE_INTERNAL void symbol_table_destroy(symbol_table_t *st);
APE_INTERNAL symbol_table_t* symbol_table_copy(symbol_table_t *st);
APE_INTERNAL void symbol_table_add_module_symbol(symbol_table_t *st, const symbol_t *symbol);
APE_INTERNAL symbol_t *symbol_table_define(symbol_table_t *st, const char *name, bool assignable);
APE_INTERNAL symbol_t *symbol_table_define_native_function(symbol_table_t *st, const char *name, int ix);
APE_INTERNAL symbol_t *symbol_table_define_free(symbol_table_t *st, symbol_t *original);
APE_INTERNAL symbol_t *symbol_table_define_function_name(symbol_table_t *st, const char *name, bool assignable);
APE_INTERNAL symbol_t *symbol_table_define_this(symbol_table_t *st);

APE_INTERNAL symbol_t *symbol_table_resolve(symbol_table_t *st, const char *name);

APE_INTERNAL bool symbol_table_symbol_is_defined(symbol_table_t *st, const char *name);
APE_INTERNAL void symbol_table_push_block_scope(symbol_table_t *table);
APE_INTERNAL void symbol_table_pop_block_scope(symbol_table_t *table);
APE_INTERNAL block_scope_t* symbol_table_get_block_scope(symbol_table_t *table);

APE_INTERNAL bool symbol_table_is_global_scope(symbol_table_t *table);
APE_INTERNAL bool symbol_table_is_top_block_scope(symbol_table_t *table);
APE_INTERNAL bool symbol_table_is_top_global_scope(symbol_table_t *table);

#endif /* symbol_table_h */
