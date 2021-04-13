#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef APE_AMALGAMATED
#include "symbol_table.h"
#include "builtins.h"
#endif

static block_scope_t* block_scope_copy(block_scope_t *scope);
static void set_symbol(symbol_table_t *table, symbol_t *symbol);
static int next_symbol_index(symbol_table_t *table);
static int count_num_definitions(symbol_table_t *table);

symbol_t *symbol_make(const char *name, symbol_type_t type, int index, bool assignable) {
    symbol_t *symbol = ape_malloc(sizeof(symbol_t));
    symbol->name = ape_strdup(name);
    symbol->type = type;
    symbol->index = index;
    symbol->assignable = assignable;
    return symbol;
}

void symbol_destroy(symbol_t *symbol) {
    if (!symbol) {
        return;
    }
    ape_free(symbol->name);
    ape_free(symbol);
}

symbol_t* symbol_copy(const symbol_t *symbol) {
    return symbol_make(symbol->name, symbol->type, symbol->index, symbol->assignable);
}

symbol_table_t *symbol_table_make(symbol_table_t *outer) {
    symbol_table_t *table = ape_malloc(sizeof(symbol_table_t));
    memset(table, 0, sizeof(symbol_table_t));
    table->max_num_definitions = 0;
    table->outer = outer;
    table->block_scopes = ptrarray_make();
    symbol_table_push_block_scope(table);
    table->free_symbols = ptrarray_make();
    if (!outer) {
        for (int i = 0; i < builtins_count(); i++) {
            const char *name = builtins_get_name(i);
            symbol_table_define_native_function(table, name, i);
        }
    }
    return table;
}

void symbol_table_destroy(symbol_table_t *table) {
    if (!table) {
        return;
    }

    while (ptrarray_count(table->block_scopes) > 0) {
        symbol_table_pop_block_scope(table);
    }
    ptrarray_destroy(table->block_scopes);
    ptrarray_destroy_with_items(table->free_symbols, symbol_destroy);
    ape_free(table);
}

symbol_table_t* symbol_table_copy(symbol_table_t *table) {
    symbol_table_t *copy = ape_malloc(sizeof(symbol_table_t));
    memset(copy, 0, sizeof(symbol_table_t));
    copy->outer = table->outer;
    copy->block_scopes = ptrarray_copy_with_items(table->block_scopes, block_scope_copy);
    copy->free_symbols = ptrarray_copy_with_items(table->free_symbols, symbol_copy);
    copy->max_num_definitions = table->max_num_definitions;
    return copy;
}

void symbol_table_add_module_symbol(symbol_table_t *st, const symbol_t *symbol) {
    if (symbol->type != SYMBOL_GLOBAL) {
        APE_ASSERT(false);
        return;
    }
    if (symbol_table_symbol_is_defined(st, symbol->name)) {
        return;
    }
    symbol_t *copy = symbol_copy(symbol);
    set_symbol(st, copy);
}

symbol_t *symbol_table_define(symbol_table_t *table, const char *name, bool assignable) {
    if (strchr(name, ':')) {
        return NULL; // module symbol
    }
    if (APE_STREQ(name, "this")) {
        return NULL; // this is reserved
    }
    symbol_type_t symbol_type = table->outer == NULL ? SYMBOL_GLOBAL : SYMBOL_LOCAL;
    int ix = next_symbol_index(table);
    symbol_t *symbol = symbol_make(name, symbol_type, ix, assignable);
    set_symbol(table, symbol);
    block_scope_t *top_scope = ptrarray_top(table->block_scopes);
    top_scope->num_definitions++;
    int definitions_count = count_num_definitions(table);
    if (definitions_count > table->max_num_definitions) {
        table->max_num_definitions = definitions_count;
    }
    return symbol;
}

symbol_t *symbol_table_define_native_function(symbol_table_t *st, const char *name, int ix) {
    symbol_t *symbol = symbol_make(name, SYMBOL_NATIVE_FUNCTION, ix, false);
    set_symbol(st, symbol);
    return symbol;
}

symbol_t *symbol_table_define_free(symbol_table_t *st, symbol_t *original) {
    symbol_t *copy = symbol_make(original->name, original->type, original->index, original->assignable);
    ptrarray_add(st->free_symbols, copy);

    symbol_t *symbol = symbol_make(original->name, SYMBOL_FREE, ptrarray_count(st->free_symbols) - 1, original->assignable);
    set_symbol(st, symbol);

    return symbol;
}

symbol_t * symbol_table_define_function_name(symbol_table_t *st, const char *name, bool assignable) {
    if (strchr(name, ':')) {
        return NULL; // module symbol
    }
    symbol_t *symbol = symbol_make(name, SYMBOL_FUNCTION, 0, assignable);
    set_symbol(st, symbol);
    return symbol;
}

symbol_t *symbol_table_define_this(symbol_table_t *st) {
    symbol_t *symbol = symbol_make("this", SYMBOL_THIS, 0, false);
    set_symbol(st, symbol);
    return symbol;
}

symbol_t *symbol_table_resolve(symbol_table_t *table, const char *name) {
    symbol_t *symbol = NULL;
    block_scope_t *scope = NULL;
    for (int i = ptrarray_count(table->block_scopes) - 1; i >= 0; i--) {
        scope = ptrarray_get(table->block_scopes, i);
        symbol = dict_get(scope->store, name);
        if (symbol) {
            break;
        }
    }

    if (symbol && symbol->type == SYMBOL_THIS) {
        symbol = symbol_table_define_free(table, symbol);
    }

    if (!symbol && table->outer) {
        symbol = symbol_table_resolve(table->outer, name);
        if (!symbol || symbol->type == SYMBOL_GLOBAL || symbol->type == SYMBOL_NATIVE_FUNCTION) {
            return symbol;
        }
        symbol = symbol_table_define_free(table, symbol);
    }
    return symbol;
}

bool symbol_table_symbol_is_defined(symbol_table_t *table, const char *name) { // todo: rename to something more obvious
    block_scope_t *top_scope = ptrarray_top(table->block_scopes);
    symbol_t *existing = dict_get(top_scope->store, name);
    if (existing) {
        return true;
    }
    return false;
}

void symbol_table_push_block_scope(symbol_table_t *table) {
    block_scope_t *new_scope = ape_malloc(sizeof(block_scope_t));
    new_scope->store = dict_make();
    new_scope->num_definitions = 0;
    new_scope->offset = count_num_definitions(table);
    ptrarray_push(table->block_scopes, new_scope);
}

void symbol_table_pop_block_scope(symbol_table_t *table) {
    block_scope_t *top_scope = ptrarray_top(table->block_scopes);
    ptrarray_pop(table->block_scopes);
    dict_destroy_with_items(top_scope->store, symbol_destroy);
    ape_free(top_scope);
}

block_scope_t* symbol_table_get_block_scope(symbol_table_t *table) {
    block_scope_t *top_scope = ptrarray_top(table->block_scopes);
    return top_scope;
}


bool symbol_table_is_global_scope(symbol_table_t *table) {
    return table->outer == NULL;
}

bool symbol_table_is_top_block_scope(symbol_table_t *table) {
    return ptrarray_count(table->block_scopes) == 1;
}

bool symbol_table_is_top_global_scope(symbol_table_t *table) {
    return symbol_table_is_global_scope(table) && symbol_table_is_top_block_scope(table);
}

// INTERNAL
static block_scope_t* block_scope_copy(block_scope_t *scope) {
    block_scope_t *copy = ape_malloc(sizeof(block_scope_t));
    copy->num_definitions = scope->num_definitions;
    copy->offset = scope->offset;
    copy->store = dict_copy_with_items(scope->store, symbol_copy);
    return copy;
}

static void set_symbol(symbol_table_t *table, symbol_t *symbol) {
    block_scope_t *top_scope = ptrarray_top(table->block_scopes);
    symbol_t *existing = dict_get(top_scope->store, symbol->name);
    if (existing) {
        symbol_destroy(existing);
    }
    dict_set(top_scope->store, symbol->name, symbol);
}

static int next_symbol_index(symbol_table_t *table) {
    block_scope_t *top_scope = ptrarray_top(table->block_scopes);
    int ix = top_scope->offset + top_scope->num_definitions;
    return ix;
}

static int count_num_definitions(symbol_table_t *table) {
    int count = 0;
    for (int i = ptrarray_count(table->block_scopes) - 1; i >= 0; i--) {
        block_scope_t *scope = ptrarray_get(table->block_scopes, i);
        count += scope->num_definitions;
    }
    return count;
}
