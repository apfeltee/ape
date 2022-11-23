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

#include "ape.h"

static const ApePosition_t g_vmpriv_src_pos_invalid = { NULL, -1, -1 };


static bool vmpriv_setsymbol(ApeSymbolTable_t *table, ApeSymbol_t *symbol);
static int vmpriv_nextsymbolindex(ApeSymbolTable_t *table);
static int vmpriv_countnumdefs(ApeSymbolTable_t *table);
static void vmpriv_setstackpointer(ApeVM_t *vm, int new_sp);
static void vmpriv_pushthisstack(ApeVM_t *vm, ApeObject_t obj);
static ApeObject_t vmpriv_popthisstack(ApeVM_t *vm);
static ApeObject_t vmpriv_getthisstack(ApeVM_t *vm, int nth_item);
static bool vmpriv_tryoverloadoperator(ApeVM_t *vm, ApeObject_t left, ApeObject_t right, ApeOpByte_t op, bool *out_overload_found);


/**/
static ApeOpcodeDefinition_t g_definitions[OPCODE_MAX + 1] =
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



//-----------------------------------------------------------------------------
// Allocator
//-----------------------------------------------------------------------------

ApeAllocator_t allocator_make(ApeMemAllocFunc_t malloc_fn, ApeMemFreeFunc_t free_fn, void* ctx)
{
    ApeAllocator_t alloc;
    alloc.malloc = malloc_fn;
    alloc.free = free_fn;
    alloc.ctx = ctx;
    return alloc;
}

void* allocator_malloc(ApeAllocator_t* allocator, size_t size)
{
    if(!allocator || !allocator->malloc)
    {
        return malloc(size);
    }
    return allocator->malloc(allocator->ctx, size);
}

void allocator_free(ApeAllocator_t* allocator, void* ptr)
{
    if(!allocator || !allocator->free)
    {
        free(ptr);
        return;
    }
    allocator->free(allocator->ctx, ptr);
}

ApeCompiledFile_t* compiled_file_make(ApeAllocator_t* alloc, const char* path)
{
    size_t len;
    const char* last_slash_pos;
    ApeCompiledFile_t* file;
    file = (ApeCompiledFile_t*)allocator_malloc(alloc, sizeof(ApeCompiledFile_t));
    if(!file)
    {
        return NULL;
    }
    memset(file, 0, sizeof(ApeCompiledFile_t));
    file->alloc = alloc;
    last_slash_pos = strrchr(path, '/');
    if(last_slash_pos)
    {
        len = last_slash_pos - path + 1;
        file->dir_path = util_strndup(alloc, path, len);
    }
    else
    {
        file->dir_path = util_strdup(alloc, "");
    }
    if(!file->dir_path)
    {
        goto error;
    }
    file->path = util_strdup(alloc, path);
    if(!file->path)
    {
        goto error;
    }
    file->lines = ptrarray_make(alloc);
    if(!file->lines)
    {
        goto error;
    }
    return file;
error:
    compiled_file_destroy(file);
    return NULL;
}

void* compiled_file_destroy(ApeCompiledFile_t* file)
{
    ApeSize_t i;
    void* item;
    if(!file)
    {
        return NULL;
    }
    for(i = 0; i < ptrarray_count(file->lines); i++)
    {
        item = (void*)ptrarray_get(file->lines, i);
        allocator_free(file->alloc, item);
    }
    ptrarray_destroy(file->lines);
    allocator_free(file->alloc, file->dir_path);
    allocator_free(file->alloc, file->path);
    allocator_free(file->alloc, file);
    return NULL;
}

ApeGlobalStore_t* global_store_make(ApeAllocator_t* alloc, ApeGCMemory_t* mem)
{
    ApeSize_t i;
    bool ok;
    const char* name;
    ApeObject_t builtin;
    ApeGlobalStore_t* store;
    store = (ApeGlobalStore_t*)allocator_malloc(alloc, sizeof(ApeGlobalStore_t));
    if(!store)
    {
        return NULL;
    }
    memset(store, 0, sizeof(ApeGlobalStore_t));
    store->alloc = alloc;
    store->symbols = strdict_make(alloc, (ApeDataCallback_t)symbol_copy, (ApeDataCallback_t)symbol_destroy);
    if(!store->symbols)
    {
        goto err;
    }
    store->objects = array_make(alloc, ApeObject_t);
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
            if(object_value_isnull(builtin))
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

void global_store_destroy(ApeGlobalStore_t* store)
{
    if(!store)
    {
        return;
    }
    strdict_destroy_with_items(store->symbols);
    array_destroy(store->objects);
    allocator_free(store->alloc, store);
}

const ApeSymbol_t* global_store_get_symbol(ApeGlobalStore_t* store, const char* name)
{
    return (ApeSymbol_t*)strdict_get(store->symbols, name);
}

bool global_store_set(ApeGlobalStore_t* store, const char* name, ApeObject_t object)
{
    int ix;
    bool ok;
    ApeSymbol_t* symbol;
    const ApeSymbol_t* existing_symbol;
    existing_symbol = global_store_get_symbol(store, name);
    if(existing_symbol)
    {
        ok = array_set(store->objects, existing_symbol->index, &object);
        return ok;
    }
    ix = array_count(store->objects);
    ok = array_add(store->objects, &object);
    if(!ok)
    {
        return false;
    }
    symbol = symbol_make(store->alloc, name, SYMBOL_APE_GLOBAL, ix, false);
    if(!symbol)
    {
        goto err;
    }
    ok = strdict_set(store->symbols, name, symbol);
    if(!ok)
    {
        symbol_destroy(symbol);
        goto err;
    }
    return true;
err:
    array_pop(store->objects, NULL);
    return false;
}

ApeObject_t global_store_get_object_at(ApeGlobalStore_t* store, int ix, bool* out_ok)
{
    ApeObject_t* res = (ApeObject_t*)array_get(store->objects, ix);
    if(!res)
    {
        *out_ok = false;
        return object_make_null();
    }
    *out_ok = true;
    return *res;
}

ApeObject_t* global_store_get_object_data(ApeGlobalStore_t* store)
{
    return (ApeObject_t*)array_data(store->objects);
}

ApeSize_t global_store_get_object_count(ApeGlobalStore_t* store)
{
    return array_count(store->objects);
}

ApeSymbol_t* symbol_make(ApeAllocator_t* alloc, const char* name, ApeSymbolType_t type, int index, bool assignable)
{
    ApeSymbol_t* symbol;
    symbol = (ApeSymbol_t*)allocator_malloc(alloc, sizeof(ApeSymbol_t));
    if(!symbol)
    {
        return NULL;
    }
    memset(symbol, 0, sizeof(ApeSymbol_t));
    symbol->alloc = alloc;
    symbol->name = util_strdup(alloc, name);
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

void* symbol_destroy(ApeSymbol_t* symbol)
{
    if(!symbol)
    {
        return NULL;
    }
    allocator_free(symbol->alloc, symbol->name);
    allocator_free(symbol->alloc, symbol);
    return NULL;
}

ApeSymbol_t* symbol_copy(ApeSymbol_t* symbol)
{
    return symbol_make(symbol->alloc, symbol->name, symbol->type, symbol->index, symbol->assignable);
}

ApeSymbolTable_t* symbol_table_make(ApeAllocator_t* alloc, ApeSymbolTable_t* outer, ApeGlobalStore_t* global_store, int module_global_offset)
{
    bool ok;
    ApeSymbolTable_t* table;
    table = (ApeSymbolTable_t*)allocator_malloc(alloc, sizeof(ApeSymbolTable_t));
    if(!table)
    {
        return NULL;
    }
    memset(table, 0, sizeof(ApeSymbolTable_t));
    table->alloc = alloc;
    table->max_num_definitions = 0;
    table->outer = outer;
    table->global_store = global_store;
    table->module_global_offset = module_global_offset;
    table->block_scopes = ptrarray_make(alloc);
    if(!table->block_scopes)
    {
        goto err;
    }
    table->free_symbols = ptrarray_make(alloc);
    if(!table->free_symbols)
    {
        goto err;
    }
    table->module_global_symbols = ptrarray_make(alloc);
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

void symbol_table_destroy(ApeSymbolTable_t* table)
{
    ApeAllocator_t* alloc;
    if(!table)
    {
        return;
    }
    while(ptrarray_count(table->block_scopes) > 0)
    {
        symbol_table_pop_block_scope(table);
    }
    ptrarray_destroy(table->block_scopes);
    ptrarray_destroy_with_items(table->module_global_symbols, (ApeDataCallback_t)symbol_destroy);
    ptrarray_destroy_with_items(table->free_symbols, (ApeDataCallback_t)symbol_destroy);
    alloc = table->alloc;
    memset(table, 0, sizeof(ApeSymbolTable_t));
    allocator_free(alloc, table);
}

ApeSymbolTable_t* symbol_table_copy(ApeSymbolTable_t* table)
{
    ApeSymbolTable_t* copy;
    copy = (ApeSymbolTable_t*)allocator_malloc(table->alloc, sizeof(ApeSymbolTable_t));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(ApeSymbolTable_t));
    copy->alloc = table->alloc;
    copy->outer = table->outer;
    copy->global_store = table->global_store;
    copy->block_scopes = ptrarray_copy_with_items(table->block_scopes, (ApeDataCallback_t)block_scope_copy, (ApeDataCallback_t)block_scope_destroy);
    if(!copy->block_scopes)
    {
        goto err;
    }
    copy->free_symbols = ptrarray_copy_with_items(table->free_symbols, (ApeDataCallback_t)symbol_copy, (ApeDataCallback_t)symbol_destroy);
    if(!copy->free_symbols)
    {
        goto err;
    }
    copy->module_global_symbols = ptrarray_copy_with_items(table->module_global_symbols, (ApeDataCallback_t)symbol_copy, (ApeDataCallback_t)symbol_destroy);
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

bool symbol_table_add_module_symbol(ApeSymbolTable_t* st, ApeSymbol_t* symbol)
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
    ApeSymbol_t* copy = symbol_copy(symbol);
    if(!copy)
    {
        return false;
    }
    ok = vmpriv_setsymbol(st, copy);
    if(!ok)
    {
        symbol_destroy(copy);
        return false;
    }
    return true;
}

const ApeSymbol_t* symbol_table_define(ApeSymbolTable_t* table, const char* name, bool assignable)
{
    
    bool ok;
    bool global_symbol_added;
    int ix;
    ApeSize_t definitions_count;
    ApeBlockScope_t* top_scope;
    ApeSymbolType_t symbol_type;
    ApeSymbol_t* symbol;
    ApeSymbol_t* global_symbol_copy;
    const ApeSymbol_t* global_symbol;
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
    ix = vmpriv_nextsymbolindex(table);
    symbol = symbol_make(table->alloc, name, symbol_type, ix, assignable);
    if(!symbol)
    {
        return NULL;
    }
    global_symbol_added = false;
    ok = false;
    if(symbol_type == SYMBOL_MODULE_GLOBAL && ptrarray_count(table->block_scopes) == 1)
    {
        global_symbol_copy = symbol_copy(symbol);
        if(!global_symbol_copy)
        {
            symbol_destroy(symbol);
            return NULL;
        }
        ok = ptrarray_add(table->module_global_symbols, global_symbol_copy);
        if(!ok)
        {
            symbol_destroy(global_symbol_copy);
            symbol_destroy(symbol);
            return NULL;
        }
        global_symbol_added = true;
    }
    ok = vmpriv_setsymbol(table, symbol);
    if(!ok)
    {
        if(global_symbol_added)
        {
            global_symbol_copy = (ApeSymbol_t*)ptrarray_pop(table->module_global_symbols);
            symbol_destroy(global_symbol_copy);
        }
        symbol_destroy(symbol);
        return NULL;
    }
    top_scope = (ApeBlockScope_t*)ptrarray_top(table->block_scopes);
    top_scope->num_definitions++;
    definitions_count = vmpriv_countnumdefs(table);
    if(definitions_count > table->max_num_definitions)
    {
        table->max_num_definitions = definitions_count;
    }

    return symbol;
}

const ApeSymbol_t* symbol_table_define_free(ApeSymbolTable_t* st, const ApeSymbol_t* original)
{
    bool ok;
    ApeSymbol_t* symbol;
    ApeSymbol_t* copy;
    copy = symbol_make(st->alloc, original->name, original->type, original->index, original->assignable);
    if(!copy)
    {
        return NULL;
    }
    ok = ptrarray_add(st->free_symbols, copy);
    if(!ok)
    {
        symbol_destroy(copy);
        return NULL;
    }

    symbol = symbol_make(st->alloc, original->name, SYMBOL_FREE, ptrarray_count(st->free_symbols) - 1, original->assignable);
    if(!symbol)
    {
        return NULL;
    }

    ok = vmpriv_setsymbol(st, symbol);
    if(!ok)
    {
        symbol_destroy(symbol);
        return NULL;
    }

    return symbol;
}

const ApeSymbol_t* symbol_table_define_function_name(ApeSymbolTable_t* st, const char* name, bool assignable)
{
    bool ok;
    ApeSymbol_t* symbol;
    if(strchr(name, ':'))
    {
        return NULL;// module symbol
    }
    symbol = symbol_make(st->alloc, name, SYMBOL_FUNCTION, 0, assignable);
    if(!symbol)
    {
        return NULL;
    }
    ok = vmpriv_setsymbol(st, symbol);
    if(!ok)
    {
        symbol_destroy(symbol);
        return NULL;
    }

    return symbol;
}

const ApeSymbol_t* symbol_table_define_this(ApeSymbolTable_t* st)
{
    bool ok;
    ApeSymbol_t* symbol;
    symbol = symbol_make(st->alloc, "this", SYMBOL_THIS, 0, false);
    if(!symbol)
    {
        return NULL;
    }
    ok = vmpriv_setsymbol(st, symbol);
    if(!ok)
    {
        symbol_destroy(symbol);
        return NULL;
    }
    return symbol;
}

const ApeSymbol_t* symbol_table_resolve(ApeSymbolTable_t* table, const char* name)
{
    int64_t i;
    const ApeSymbol_t* symbol;
    ApeBlockScope_t* scope;
    scope = NULL;
    symbol = global_store_get_symbol(table->global_store, name);
    if(symbol)
    {
        return symbol;
    }
    for(i = (int64_t)ptrarray_count(table->block_scopes) - 1; i >= 0; i--)
    {
        scope = (ApeBlockScope_t*)ptrarray_get(table->block_scopes, i);
        symbol = (ApeSymbol_t*)strdict_get(scope->store, name);
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

bool symbol_table_symbol_is_defined(ApeSymbolTable_t* table, const char* name)
{
    ApeBlockScope_t* top_scope;
    const ApeSymbol_t* symbol;
    // todo: rename to something more obvious
    symbol = global_store_get_symbol(table->global_store, name);
    if(symbol)
    {
        return true;
    }
    top_scope = (ApeBlockScope_t*)ptrarray_top(table->block_scopes);
    symbol = (ApeSymbol_t*)strdict_get(top_scope->store, name);
    if(symbol)
    {
        return true;
    }
    return false;
}

bool symbol_table_push_block_scope(ApeSymbolTable_t* table)
{
    bool ok;
    int block_scope_offset;
    ApeBlockScope_t* prev_block_scope;
    ApeBlockScope_t* new_scope;
    block_scope_offset = 0;
    prev_block_scope = (ApeBlockScope_t*)ptrarray_top(table->block_scopes);
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
    ok = ptrarray_push(table->block_scopes, new_scope);
    if(!ok)
    {
        block_scope_destroy(new_scope);
        return false;
    }
    return true;
}

void symbol_table_pop_block_scope(ApeSymbolTable_t* table)
{
    ApeBlockScope_t* top_scope;
    top_scope = (ApeBlockScope_t*)ptrarray_top(table->block_scopes);
    ptrarray_pop(table->block_scopes);
    block_scope_destroy(top_scope);
}

ApeBlockScope_t* symbol_table_get_block_scope(ApeSymbolTable_t* table)
{
    ApeBlockScope_t* top_scope;
    top_scope = (ApeBlockScope_t*)ptrarray_top(table->block_scopes);
    return top_scope;
}

bool symbol_table_is_module_global_scope(ApeSymbolTable_t* table)
{
    return table->outer == NULL;
}

bool symbol_table_is_top_block_scope(ApeSymbolTable_t* table)
{
    return ptrarray_count(table->block_scopes) == 1;
}

bool symbol_table_is_top_global_scope(ApeSymbolTable_t* table)
{
    return symbol_table_is_module_global_scope(table) && symbol_table_is_top_block_scope(table);
}

ApeSize_t symbol_table_get_module_global_symbol_count(const ApeSymbolTable_t* table)
{
    return ptrarray_count(table->module_global_symbols);
}

const ApeSymbol_t* symbol_table_get_module_global_symbol_at(const ApeSymbolTable_t* table, int ix)
{
    return (ApeSymbol_t*)ptrarray_get(table->module_global_symbols, ix);
}

// INTERNAL
ApeBlockScope_t* block_scope_make(ApeAllocator_t* alloc, int offset)
{
    ApeBlockScope_t* new_scope;
    new_scope = (ApeBlockScope_t*)allocator_malloc(alloc, sizeof(ApeBlockScope_t));
    if(!new_scope)
    {
        return NULL;
    }
    memset(new_scope, 0, sizeof(ApeBlockScope_t));
    new_scope->alloc = alloc;
    new_scope->store = strdict_make(alloc, (ApeDataCallback_t)symbol_copy, (ApeDataCallback_t)symbol_destroy);
    if(!new_scope->store)
    {
        block_scope_destroy(new_scope);
        return NULL;
    }
    new_scope->num_definitions = 0;
    new_scope->offset = offset;
    return new_scope;
}

void* block_scope_destroy(ApeBlockScope_t* scope)
{
    strdict_destroy_with_items(scope->store);
    allocator_free(scope->alloc, scope);
    return NULL;
}

ApeBlockScope_t* block_scope_copy(ApeBlockScope_t* scope)
{
    ApeBlockScope_t* copy;
    copy = (ApeBlockScope_t*)allocator_malloc(scope->alloc, sizeof(ApeBlockScope_t));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(ApeBlockScope_t));
    copy->alloc = scope->alloc;
    copy->num_definitions = scope->num_definitions;
    copy->offset = scope->offset;
    copy->store = strdict_copy_with_items(scope->store);
    if(!copy->store)
    {
        block_scope_destroy(copy);
        return NULL;
    }
    return copy;
}

static bool vmpriv_setsymbol(ApeSymbolTable_t* table, ApeSymbol_t* symbol)
{
    ApeBlockScope_t* top_scope;
    ApeSymbol_t* existing;
    top_scope = (ApeBlockScope_t*)ptrarray_top(table->block_scopes);
    existing= (ApeSymbol_t*)strdict_get(top_scope->store, symbol->name);
    if(existing)
    {
        symbol_destroy(existing);
    }
    return strdict_set(top_scope->store, symbol->name, symbol);
}

static int vmpriv_nextsymbolindex(ApeSymbolTable_t* table)
{
    int ix;
    ApeBlockScope_t* top_scope;
    top_scope = (ApeBlockScope_t*)ptrarray_top(table->block_scopes);
    ix = top_scope->offset + top_scope->num_definitions;
    return ix;
}

static int vmpriv_countnumdefs(ApeSymbolTable_t* table)
{
    int64_t i;
    int count;
    ApeBlockScope_t* scope;
    count = 0;
    for(i = (int64_t)ptrarray_count(table->block_scopes) - 1; i >= 0; i--)
    {
        scope = (ApeBlockScope_t*)ptrarray_get(table->block_scopes, i);
        count += scope->num_definitions;
    }
    return count;
}


ApeOpcodeDefinition_t* opcode_lookup(ApeOpByte_t op)
{
    if(op <= OPCODE_NONE || op >= OPCODE_MAX)
    {
        return NULL;
    }
    return &g_definitions[op];
}

const char* opcode_get_name(ApeOpByte_t op)
{
    if(op <= OPCODE_NONE || op >= OPCODE_MAX)
    {
        return NULL;
    }
    return g_definitions[op].name;
}

#define APPEND_BYTE(n)                           \
    do                                           \
    {                                            \
        val = (ApeUShort_t)(operands[i] >> (n * 8)); \
        ok = array_add(res, &val);               \
        if(!ok)                                  \
        {                                        \
            return 0;                            \
        }                                        \
    } while(0)

int code_make(ApeOpByte_t op, ApeSize_t operands_count, uint64_t* operands, ApeArray_t* res)
{
    ApeSize_t i;
    int width;
    int instr_len;
    bool ok;
    ApeUShort_t val;
    ApeOpcodeDefinition_t* def;
    def = opcode_lookup(op);
    if(!def)
    {
        return 0;
    }
    instr_len = 1;
    for(i = 0; i < def->num_operands; i++)
    {
        instr_len += def->operand_widths[i];
    }
    val = op;
    ok = false;
    ok = array_add(res, &val);
    if(!ok)
    {
        return 0;
    }
    for(i = 0; i < operands_count; i++)
    {
        width = def->operand_widths[i];
        switch(width)
        {
            case 1:
                {
                    APPEND_BYTE(0);
                }
                break;
            case 2:
                {
                    APPEND_BYTE(1);
                    APPEND_BYTE(0);
                }
                break;
            case 4:
                {
                    APPEND_BYTE(3);
                    APPEND_BYTE(2);
                    APPEND_BYTE(1);
                    APPEND_BYTE(0);
                }
                break;
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
                }
                break;
            default:
                {
                    APE_ASSERT(false);
                }
                break;
        }
    }
    return instr_len;
}
#undef APPEND_BYTE

ApeCompilationScope_t* compilation_scope_make(ApeAllocator_t* alloc, ApeCompilationScope_t* outer)
{
    ApeCompilationScope_t* scope = (ApeCompilationScope_t*)allocator_malloc(alloc, sizeof(ApeCompilationScope_t));
    if(!scope)
    {
        return NULL;
    }
    memset(scope, 0, sizeof(ApeCompilationScope_t));
    scope->alloc = alloc;
    scope->outer = outer;
    scope->bytecode = array_make(alloc, ApeUShort_t);
    if(!scope->bytecode)
    {
        goto err;
    }
    scope->src_positions = array_make(alloc, ApePosition_t);
    if(!scope->src_positions)
    {
        goto err;
    }
    scope->break_ip_stack = array_make(alloc, int);
    if(!scope->break_ip_stack)
    {
        goto err;
    }
    scope->continue_ip_stack = array_make(alloc, int);
    if(!scope->continue_ip_stack)
    {
        goto err;
    }
    return scope;
err:
    compilation_scope_destroy(scope);
    return NULL;
}

void compilation_scope_destroy(ApeCompilationScope_t* scope)
{
    array_destroy(scope->continue_ip_stack);
    array_destroy(scope->break_ip_stack);
    array_destroy(scope->bytecode);
    array_destroy(scope->src_positions);
    allocator_free(scope->alloc, scope);
}

ApeCompilationResult_t* compilation_scope_orphan_result(ApeCompilationScope_t* scope)
{
    ApeCompilationResult_t* res;
    res = compilation_result_make(scope->alloc,
        (ApeUShort_t*)array_data(scope->bytecode),
        (ApePosition_t*)array_data(scope->src_positions),
        array_count(scope->bytecode)
    );
    if(!res)
    {
        return NULL;
    }
    array_orphan_data(scope->bytecode);
    array_orphan_data(scope->src_positions);
    return res;
}

ApeCompilationResult_t* compilation_result_make(ApeAllocator_t* alloc, ApeUShort_t* bytecode, ApePosition_t* src_positions, int count)
{
    ApeCompilationResult_t* res;
    res = (ApeCompilationResult_t*)allocator_malloc(alloc, sizeof(ApeCompilationResult_t));
    if(!res)
    {
        return NULL;
    }
    memset(res, 0, sizeof(ApeCompilationResult_t));
    res->alloc = alloc;
    res->bytecode = bytecode;
    res->src_positions = src_positions;
    res->count = count;
    return res;
}

void compilation_result_destroy(ApeCompilationResult_t* res)
{
    if(!res)
    {
        return;
    }
    allocator_free(res->alloc, res->bytecode);
    allocator_free(res->alloc, res->src_positions);
    allocator_free(res->alloc, res);
}

ApeExpression_t* optimise_expression(ApeExpression_t* expr)
{

    switch(expr->type)
    {
        case EXPRESSION_INFIX:
            {
                return optimise_infix_expression(expr);
            }
            break;
        case EXPRESSION_PREFIX:
            {
                return optimise_prefix_expression(expr);
            }
            break;
        default:
            {
            }
            break;
    }
    return NULL;
}

// INTERNAL
ApeExpression_t* optimise_infix_expression(ApeExpression_t* expr)
{
    bool left_is_numeric;
    bool right_is_numeric;
    bool left_is_string;
    bool right_is_string;
    int64_t leftint;
    int64_t rightint;
    char* res_str;
    const char* leftstr;
    const char* rightstr;
    ApeExpression_t* left;
    ApeExpression_t* left_optimised;
    ApeExpression_t* right;
    ApeExpression_t* right_optimised;
    ApeExpression_t* res;
    ApeAllocator_t* alloc;
    ApeFloat_t leftval;
    ApeFloat_t rightval;
    left = expr->infix.left;
    left_optimised = optimise_expression(left);
    if(left_optimised)
    {
        left = left_optimised;
    }
    right = expr->infix.right;
    right_optimised = optimise_expression(right);
    if(right_optimised)
    {
        right = right_optimised;
    }
    res = NULL;
    left_is_numeric = left->type == EXPRESSION_NUMBER_LITERAL || left->type == EXPRESSION_BOOL_LITERAL;
    right_is_numeric = right->type == EXPRESSION_NUMBER_LITERAL || right->type == EXPRESSION_BOOL_LITERAL;
    left_is_string = left->type == EXPRESSION_STRING_LITERAL;
    right_is_string = right->type == EXPRESSION_STRING_LITERAL;
    alloc = expr->alloc;
    if(left_is_numeric && right_is_numeric)
    {
        leftval = left->type == EXPRESSION_NUMBER_LITERAL ? left->number_literal : left->bool_literal;
        rightval = right->type == EXPRESSION_NUMBER_LITERAL ? right->number_literal : right->bool_literal;
        leftint = (int64_t)leftval;
        rightint = (int64_t)rightval;
        switch(expr->infix.op)
        {
            case OPERATOR_PLUS:
                {
                    res = expression_make_number_literal(alloc, leftval + rightval);
                }
                break;
            case OPERATOR_MINUS:
                {
                    res = expression_make_number_literal(alloc, leftval - rightval);
                }
                break;
            case OPERATOR_ASTERISK:
                {
                    res = expression_make_number_literal(alloc, leftval * rightval);
                }
                break;
            case OPERATOR_SLASH:
                {
                    res = expression_make_number_literal(alloc, leftval / rightval);
                }
                break;
            case OPERATOR_LT:
                {
                    res = expression_make_bool_literal(alloc, leftval < rightval);
                }
                break;
            case OPERATOR_LTE:
                {
                    res = expression_make_bool_literal(alloc, leftval <= rightval);
                }
                break;
            case OPERATOR_GT:
                {
                    res = expression_make_bool_literal(alloc, leftval > rightval);
                }
                break;
            case OPERATOR_GTE:
                {
                    res = expression_make_bool_literal(alloc, leftval >= rightval);
                }
                break;
            case OPERATOR_EQ:
                {
                    res = expression_make_bool_literal(alloc, APE_DBLEQ(leftval, rightval));
                }
                break;

            case OPERATOR_NOT_EQ:
                {
                    res = expression_make_bool_literal(alloc, !APE_DBLEQ(leftval, rightval));
                }
                break;
            case OPERATOR_MODULUS:
                {
                    //res = expression_make_number_literal(alloc, fmod(leftval, rightval));
                    res = expression_make_number_literal(alloc, (leftint % rightint));
                }
                break;
            case OPERATOR_BIT_AND:
                {
                    res = expression_make_number_literal(alloc, (ApeFloat_t)(leftint & rightint));
                }
                break;
            case OPERATOR_BIT_OR:
                {
                    res = expression_make_number_literal(alloc, (ApeFloat_t)(leftint | rightint));
                }
                break;
            case OPERATOR_BIT_XOR:
                {
                    res = expression_make_number_literal(alloc, (ApeFloat_t)(leftint ^ rightint));
                }
                break;
            case OPERATOR_LSHIFT:
                {
                    res = expression_make_number_literal(alloc, (ApeFloat_t)(leftint << rightint));
                }
                break;
            case OPERATOR_RSHIFT:
                {
                    res = expression_make_number_literal(alloc, (ApeFloat_t)(leftint >> rightint));
                }
                break;
            default:
                {
                }
                break;
        }
    }
    else if(expr->infix.op == OPERATOR_PLUS && left_is_string && right_is_string)
    {
        leftstr = left->string_literal;
        rightstr = right->string_literal;
        res_str = util_stringfmt(alloc, "%s%s", leftstr, rightstr);
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

ApeExpression_t* optimise_prefix_expression(ApeExpression_t* expr)
{
    ApeExpression_t* right;
    ApeExpression_t* right_optimised;
    ApeExpression_t* res;
    right = expr->prefix.right;
    right_optimised = optimise_expression(right);
    if(right_optimised)
    {
        right = right_optimised;
    }
    res = NULL;
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

ApeModule_t* module_make(ApeAllocator_t* alloc, const char* name)
{
    ApeModule_t* module;
    module = (ApeModule_t*)allocator_malloc(alloc, sizeof(ApeModule_t));
    if(!module)
    {
        return NULL;
    }
    memset(module, 0, sizeof(ApeModule_t));
    module->alloc = alloc;
    module->name = util_strdup(alloc, name);
    if(!module->name)
    {
        module_destroy(module);
        return NULL;
    }
    module->symbols = ptrarray_make(alloc);
    if(!module->symbols)
    {
        module_destroy(module);
        return NULL;
    }
    return module;
}

void* module_destroy(ApeModule_t* module)
{
    if(!module)
    {
        return NULL;
    }
    allocator_free(module->alloc, module->name);
    ptrarray_destroy_with_items(module->symbols, (ApeDataCallback_t)symbol_destroy);
    allocator_free(module->alloc, module);
    return NULL;
}

ApeModule_t* module_copy(ApeModule_t* src)
{
    ApeModule_t* copy;
    copy = (ApeModule_t*)allocator_malloc(src->alloc, sizeof(ApeModule_t));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(ApeModule_t));
    copy->alloc = src->alloc;
    copy->name = util_strdup(copy->alloc, src->name);
    if(!copy->name)
    {
        module_destroy(copy);
        return NULL;
    }
    copy->symbols = ptrarray_copy_with_items(src->symbols, (ApeDataCallback_t)symbol_copy, (ApeDataCallback_t)symbol_destroy);
    if(!copy->symbols)
    {
        module_destroy(copy);
        return NULL;
    }
    return copy;
}

const char* get_module_name(const char* path)
{
    const char* last_slash_pos;
    last_slash_pos = strrchr(path, '/');
    if(last_slash_pos)
    {
        return last_slash_pos + 1;
    }
    return path;
}

bool module_add_symbol(ApeModule_t* module, const ApeSymbol_t* symbol)
{
    bool ok;
    ApeStringBuffer_t* name_buf;
    ApeSymbol_t* module_symbol;
    name_buf = strbuf_make(module->alloc);
    if(!name_buf)
    {
        return false;
    }
    ok = strbuf_appendf(name_buf, "%s::%s", module->name, symbol->name);
    if(!ok)
    {
        strbuf_destroy(name_buf);
        return false;
    }
    module_symbol = symbol_make(module->alloc, strbuf_get_string(name_buf), SYMBOL_MODULE_GLOBAL, symbol->index, false);
    strbuf_destroy(name_buf);
    if(!module_symbol)
    {
        return false;
    }
    ok = ptrarray_add(module->symbols, module_symbol);
    if(!ok)
    {
        symbol_destroy(module_symbol);
        return false;
    }
    return true;
}

ApeObjectDataPool_t* get_pool_for_type(ApeGCMemory_t* mem, ApeObjectType_t type);
bool can_data_be_put_in_pool(ApeGCMemory_t* mem, ApeObjectData_t* data);

ApeGCMemory_t* gcmem_make(ApeAllocator_t* alloc)
{
    ApeSize_t i;
    ApeGCMemory_t* mem;
    ApeObjectDataPool_t* pool;
    mem = (ApeGCMemory_t*)allocator_malloc(alloc, sizeof(ApeGCMemory_t));
    if(!mem)
    {
        return NULL;
    }
    memset(mem, 0, sizeof(ApeGCMemory_t));
    mem->alloc = alloc;
    mem->objects = ptrarray_make(alloc);
    if(!mem->objects)
    {
        goto error;
    }
    mem->objects_back = ptrarray_make(alloc);
    if(!mem->objects_back)
    {
        goto error;
    }
    mem->objects_not_gced = array_make(alloc, ApeObject_t);
    if(!mem->objects_not_gced)
    {
        goto error;
    }
    mem->allocations_since_sweep = 0;
    mem->data_only_pool.count = 0;
    for(i = 0; i < GCMEM_POOLS_NUM; i++)
    {
        pool = &mem->pools[i];
        mem->pools[i].count = 0;
        memset(pool, 0, sizeof(ApeObjectDataPool_t));
    }
    return mem;
error:
    gcmem_destroy(mem);
    return NULL;
}

void gcmem_destroy(ApeGCMemory_t* mem)
{
    ApeSize_t i;
    ApeSize_t j;
    ApeObjectData_t* obj;
    ApeObjectData_t* data;
    ApeObjectDataPool_t* pool;
    if(!mem)
    {
        return;
    }
    array_destroy(mem->objects_not_gced);
    ptrarray_destroy(mem->objects_back);
    for(i = 0; i < ptrarray_count(mem->objects); i++)
    {
        obj = (ApeObjectData_t*)ptrarray_get(mem->objects, i);
        object_data_deinit(obj);
        allocator_free(mem->alloc, obj);
    }
    ptrarray_destroy(mem->objects);
    for(i = 0; i < GCMEM_POOLS_NUM; i++)
    {
        pool = &mem->pools[i];
        for(j = 0; j < pool->count; j++)
        {
            data = pool->datapool[j];
            object_data_deinit(data);
            allocator_free(mem->alloc, data);
        }
        memset(pool, 0, sizeof(ApeObjectDataPool_t));
    }
    for(i = 0; i < mem->data_only_pool.count; i++)
    {
        allocator_free(mem->alloc, mem->data_only_pool.datapool[i]);
    }
    allocator_free(mem->alloc, mem);
}

ApeObjectData_t* gcmem_alloc_object_data(ApeGCMemory_t* mem, ApeObjectType_t type)
{
    bool ok;
    ApeObjectData_t* data;
    data = NULL;
    mem->allocations_since_sweep++;
    if(mem->data_only_pool.count > 0)
    {
        data = mem->data_only_pool.datapool[mem->data_only_pool.count - 1];
        mem->data_only_pool.count--;
    }
    else
    {
        data = (ApeObjectData_t*)allocator_malloc(mem->alloc, sizeof(ApeObjectData_t));
        if(!data)
        {
            return NULL;
        }
    }
    memset(data, 0, sizeof(ApeObjectData_t));
    APE_ASSERT(ptrarray_count(mem->objects_back) >= ptrarray_count(mem->objects));
    // we want to make sure that appending to objects_back never fails in sweep
    // so this only reserves space there.
    ok = ptrarray_add(mem->objects_back, data);
    if(!ok)
    {
        allocator_free(mem->alloc, data);
        return NULL;
    }
    ok = ptrarray_add(mem->objects, data);
    if(!ok)
    {
        allocator_free(mem->alloc, data);
        return NULL;
    }
    data->mem = mem;
    data->type = type;
    return data;
}

ApeObjectData_t* gcmem_get_object_data_from_pool(ApeGCMemory_t* mem, ApeObjectType_t type)
{
    bool ok;
    ApeObjectData_t* data;
    ApeObjectDataPool_t* pool;
    pool = get_pool_for_type(mem, type);
    if(!pool || pool->count <= 0)
    {
        return NULL;
    }
    data = pool->datapool[pool->count - 1];
    APE_ASSERT(ptrarray_count(mem->objects_back) >= ptrarray_count(mem->objects));
    // we want to make sure that appending to objects_back never fails in sweep
    // so this only reserves space there.
    ok = ptrarray_add(mem->objects_back, data);
    if(!ok)
    {
        return NULL;
    }
    ok = ptrarray_add(mem->objects, data);
    if(!ok)
    {
        return NULL;
    }
    pool->count--;
    return data;
}

void gc_unmark_all(ApeGCMemory_t* mem)
{
    ApeSize_t i;
    ApeObjectData_t* data;
    for(i = 0; i < ptrarray_count(mem->objects); i++)
    {
        data = (ApeObjectData_t*)ptrarray_get(mem->objects, i);
        data->gcmark = false;
    }
}

void gc_mark_objects(ApeObject_t* objects, ApeSize_t count)
{
    ApeSize_t i;
    ApeObject_t obj;
    for(i = 0; i < count; i++)
    {
        obj = objects[i];
        gc_mark_object(obj);
    }
}

void gc_mark_object(ApeObject_t obj)
{
    ApeSize_t i;
    ApeSize_t len;
    ApeObject_t key;
    ApeObject_t val;
    ApeObjectData_t* key_data;
    ApeObjectData_t* val_data;
    ApeFunction_t* function;
    ApeObject_t free_val;
    ApeObjectData_t* free_val_data;
    ApeObjectData_t* data;
    if(!object_value_isallocated(obj))
    {
        return;
    }
    data = object_value_allocated_data(obj);
    if(data->gcmark)
    {
        return;
    }
    data->gcmark = true;
    switch(data->type)
    {
        case APE_OBJECT_MAP:
        {
            len = object_map_getlength(obj);
            for(i = 0; i < len; i++)
            {
                key = object_map_getkeyat(obj, i);
                if(object_value_isallocated(key))
                {

                    key_data = object_value_allocated_data(key);
                    if(!key_data->gcmark)
                    {
                        gc_mark_object(key);
                    }
                }
                val = object_map_getvalueat(obj, i);
                if(object_value_isallocated(val))
                {

                    val_data = object_value_allocated_data(val);
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
                len = object_array_getlength(obj);
                for(i = 0; i < len; i++)
                {
                    val = object_array_getvalue(obj, i);
                    if(object_value_isallocated(val))
                    {
                        val_data = object_value_allocated_data(val);
                        if(!val_data->gcmark)
                        {
                            gc_mark_object(val);
                        }
                    }
                }
            }
            break;
        case APE_OBJECT_FUNCTION:
            {
                function = object_value_asfunction(obj);
                for(i = 0; i < function->free_vals_count; i++)
                {
                    free_val = object_function_getfreeval(obj, i);
                    gc_mark_object(free_val);
                    if(object_value_isallocated(free_val))
                    {
                        free_val_data = object_value_allocated_data(free_val);
                        if(!free_val_data->gcmark)
                        {
                            gc_mark_object(free_val);
                        }
                    }
                }
            }
            break;
        default:
            {
            }
            break;
    }
}

void gc_sweep(ApeGCMemory_t* mem)
{
    ApeSize_t i;
    bool ok;
    ApeObjectData_t* data;
    ApeObjectDataPool_t* pool;
    ApePtrArray_t* objs_temp;
    gc_mark_objects((ApeObject_t*)array_data(mem->objects_not_gced), array_count(mem->objects_not_gced));
    APE_ASSERT(ptrarray_count(mem->objects_back) >= ptrarray_count(mem->objects));
    ptrarray_clear(mem->objects_back);
    for(i = 0; i < ptrarray_count(mem->objects); i++)
    {
        data = (ApeObjectData_t*)ptrarray_get(mem->objects, i);
        if(data->gcmark)
        {
            // this should never fail because objects_back's size should be equal to objects
            ok = ptrarray_add(mem->objects_back, data);
            (void)ok;
            APE_ASSERT(ok);
        }
        else
        {
            if(can_data_be_put_in_pool(mem, data))
            {
                pool = get_pool_for_type(mem, data->type);
                pool->datapool[pool->count] = data;
                pool->count++;
            }
            else
            {
                object_data_deinit(data);
                if(mem->data_only_pool.count < GCMEM_POOL_SIZE)
                {
                    mem->data_only_pool.datapool[mem->data_only_pool.count] = data;
                    mem->data_only_pool.count++;
                }
                else
                {
                    allocator_free(mem->alloc, data);
                }
            }
        }
    }
    objs_temp = mem->objects;
    mem->objects = mem->objects_back;
    mem->objects_back = objs_temp;
    mem->allocations_since_sweep = 0;
}

int gc_should_sweep(ApeGCMemory_t* mem)
{
    return mem->allocations_since_sweep > GCMEM_SWEEP_INTERVAL;
}

// INTERNAL
ApeObjectDataPool_t* get_pool_for_type(ApeGCMemory_t* mem, ApeObjectType_t type)
{
    switch(type)
    {
        case APE_OBJECT_ARRAY:
            {
                return &mem->pools[0];
            }
            break;
        case APE_OBJECT_MAP:
            {
                return &mem->pools[1];
            }
            break;
        case APE_OBJECT_STRING:
            {
                return &mem->pools[2];
            }
            break;
        default:
            {
            }
            break;
    }
    return NULL;

}

bool can_data_be_put_in_pool(ApeGCMemory_t* mem, ApeObjectData_t* data)
{
    ApeObject_t obj;
    ApeObjectDataPool_t* pool;
    obj = object_make_from_data(data->type, data);
    // this is to ensure that large objects won't be kept in pool indefinitely
    switch(data->type)
    {
        case APE_OBJECT_ARRAY:
            {
                if(object_array_getlength(obj) > 1024)
                {
                    return false;
                }
            }
            break;
        case APE_OBJECT_MAP:
            {
                if(object_map_getlength(obj) > 1024)
                {
                    return false;
                }
            }
            break;
        case APE_OBJECT_STRING:
            {
                if(!data->string.is_allocated || data->string.capacity > 4096)
                {
                    return false;
                }
            }
            break;
        default:
            {
            }
            break;
    }
    pool = get_pool_for_type(mem, data->type);
    if(!pool || pool->count >= GCMEM_POOL_SIZE)
    {
        return false;
    }
    return true;
}


ApeTraceback_t* traceback_make(ApeAllocator_t* alloc)
{
    ApeTraceback_t* traceback;
    traceback = (ApeTraceback_t*)allocator_malloc(alloc, sizeof(ApeTraceback_t));
    if(!traceback)
    {
        return NULL;
    }
    memset(traceback, 0, sizeof(ApeTraceback_t));
    traceback->alloc = alloc;
    traceback->items = array_make(alloc, ApeTracebackItem_t);
    if(!traceback->items)
    {
        traceback_destroy(traceback);
        return NULL;
    }
    return traceback;
}

void traceback_destroy(ApeTraceback_t* traceback)
{
    ApeSize_t i;
    ApeTracebackItem_t* item;
    if(!traceback)
    {
        return;
    }
    for(i = 0; i < array_count(traceback->items); i++)
    {
        item = (ApeTracebackItem_t*)array_get(traceback->items, i);
        allocator_free(traceback->alloc, item->function_name);
    }
    array_destroy(traceback->items);
    allocator_free(traceback->alloc, traceback);
}

bool traceback_append(ApeTraceback_t* traceback, const char* function_name, ApePosition_t pos)
{
    bool ok;
    ApeTracebackItem_t item;
    item.function_name = util_strdup(traceback->alloc, function_name);
    if(!item.function_name)
    {
        return false;
    }
    item.pos = pos;
    ok = array_add(traceback->items, &item);
    if(!ok)
    {
        allocator_free(traceback->alloc, item.function_name);
        return false;
    }
    return true;
}

bool traceback_append_from_vm(ApeTraceback_t* traceback, ApeVM_t* vm)
{
    int64_t i;
    bool ok;
    ApeFrame_t* frame;
    for(i = vm->frames_count - 1; i >= 0; i--)
    {
        frame = &vm->frames[i];
        ok = traceback_append(traceback, object_function_getname(frame->function), frame_src_position(frame));
        if(!ok)
        {
            return false;
        }
    }
    return true;
}

const char* traceback_item_get_filepath(ApeTracebackItem_t* item)
{
    if(!item->pos.file)
    {
        return NULL;
    }
    return item->pos.file->path;
}

bool frame_init(ApeFrame_t* frame, ApeObject_t function_obj, int base_pointer)
{
    ApeFunction_t* function;
    if(object_value_type(function_obj) != APE_OBJECT_FUNCTION)
    {
        return false;
    }
    function = object_value_asfunction(function_obj);
    frame->function = function_obj;
    frame->ip = 0;
    frame->base_pointer = base_pointer;
    frame->src_ip = 0;
    frame->bytecode = function->comp_result->bytecode;
    frame->src_positions = function->comp_result->src_positions;
    frame->bytecode_size = function->comp_result->count;
    frame->recover_ip = -1;
    frame->is_recovering = false;
    return true;
}

ApeOpcodeValue_t frame_read_opcode(ApeFrame_t* frame)
{
    frame->src_ip = frame->ip;
    return (ApeOpcodeValue_t)frame_read_uint8(frame);
}

uint64_t frame_read_uint64(ApeFrame_t* frame)
{
    uint64_t res;
    const ApeUShort_t* data;
    data = frame->bytecode + frame->ip;
    frame->ip += 8;
    res = 0;
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

uint16_t frame_read_uint16(ApeFrame_t* frame)
{
    const ApeUShort_t* data;
    data = frame->bytecode + frame->ip;
    frame->ip += 2;
    return (data[0] << 8) | data[1];
}

ApeUShort_t frame_read_uint8(ApeFrame_t* frame)
{
    const ApeUShort_t* data;
    data = frame->bytecode + frame->ip;
    frame->ip++;
    return data[0];
}

ApePosition_t frame_src_position(const ApeFrame_t* frame)
{
    if(frame->src_positions)
    {
        return frame->src_positions[frame->src_ip];
    }
    return g_vmpriv_src_pos_invalid;
}

ApeObject_t vm_get_last_popped(ApeVM_t* vm)
{
    return vm->last_popped;
}

bool vm_has_errors(ApeVM_t* vm)
{
    return errorlist_count(vm->errors) > 0;
}

bool vm_set_global(ApeVM_t* vm, ApeSize_t ix, ApeObject_t val)
{
    if(ix >= VM_MAX_GLOBALS)
    {
        APE_ASSERT(false);
        errorlist_add(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "global write out of range");
        return false;
    }
    vm->globals[ix] = val;
    if(ix >= vm->globals_count)
    {
        vm->globals_count = ix + 1;
    }
    return true;
}

ApeObject_t vm_get_global(ApeVM_t* vm, ApeSize_t ix)
{
    if(ix >= VM_MAX_GLOBALS)
    {
        APE_ASSERT(false);
        errorlist_add(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "global read out of range");
        return object_make_null();
    }
    return vm->globals[ix];
}

// INTERNAL
static void vmpriv_setstackpointer(ApeVM_t* vm, int new_sp)
{
    if(new_sp > vm->sp)
    {// to avoid gcing freed objects
        int count = new_sp - vm->sp;
        size_t bytes_count = count * sizeof(ApeObject_t);
        memset(vm->stack + vm->sp, 0, bytes_count);
    }
    vm->sp = new_sp;
}

void vmpriv_pushstack(ApeVM_t* vm, ApeObject_t obj)
{
#ifdef APE_DEBUG
    if(vm->sp >= VM_STACK_SIZE)
    {
        APE_ASSERT(false);
        errorlist_add(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "stack overflow");
        return;
    }
    if(vm->current_frame)
    {
        ApeFrame_t* frame = vm->current_frame;
        ApeFunction_t* current_function = object_value_asfunction(frame->function);
        int num_locals = current_function->num_locals;
        APE_ASSERT(vm->sp >= (frame->base_pointer + num_locals));
    }
#endif
    vm->stack[vm->sp] = obj;
    vm->sp++;
}

ApeObject_t vmpriv_popstack(ApeVM_t* vm)
{
#ifdef APE_DEBUG
    if(vm->sp == 0)
    {
        errorlist_add(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "stack underflow");
        APE_ASSERT(false);
        return object_make_null();
    }
    if(vm->current_frame)
    {
        ApeFrame_t* frame = vm->current_frame;
        ApeFunction_t* current_function = object_value_asfunction(frame->function);
        int num_locals = current_function->num_locals;
        APE_ASSERT((vm->sp - 1) >= (frame->base_pointer + num_locals));
    }
#endif
    vm->sp--;
    ApeObject_t objres = vm->stack[vm->sp];
    vm->last_popped = objres;
    return objres;
}

ApeObject_t vmpriv_getstack(ApeVM_t* vm, int nth_item)
{
    int ix = vm->sp - 1 - nth_item;
#ifdef APE_DEBUG
    if(ix < 0 || ix >= VM_STACK_SIZE)
    {
        errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "invalid stack index: %d", nth_item);
        APE_ASSERT(false);
        return object_make_null();
    }
#endif
    return vm->stack[ix];
}

static void vmpriv_pushthisstack(ApeVM_t* vm, ApeObject_t obj)
{
#ifdef APE_DEBUG
    if(vm->this_sp >= VM_THIS_STACK_SIZE)
    {
        APE_ASSERT(false);
        errorlist_add(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "this stack overflow");
        return;
    }
#endif
    vm->this_stack[vm->this_sp] = obj;
    vm->this_sp++;
}

static ApeObject_t vmpriv_popthisstack(ApeVM_t* vm)
{
#ifdef APE_DEBUG
    if(vm->this_sp == 0)
    {
        errorlist_add(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "this stack underflow");
        APE_ASSERT(false);
        return object_make_null();
    }
#endif
    vm->this_sp--;
    return vm->this_stack[vm->this_sp];
}

static ApeObject_t vmpriv_getthisstack(ApeVM_t* vm, int nth_item)
{
    int ix = vm->this_sp - 1 - nth_item;
#ifdef APE_DEBUG
    if(ix < 0 || ix >= VM_THIS_STACK_SIZE)
    {
        errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "invalid this stack index: %d", nth_item);
        APE_ASSERT(false);
        return object_make_null();
    }
#endif
    return vm->this_stack[ix];
}

bool vmpriv_pushframe(ApeVM_t* vm, ApeFrame_t frame)
{
    if(vm->frames_count >= VM_MAX_FRAMES)
    {
        APE_ASSERT(false);
        return false;
    }
    vm->frames[vm->frames_count] = frame;
    vm->current_frame = &vm->frames[vm->frames_count];
    vm->frames_count++;
    ApeFunction_t* frame_function = object_value_asfunction(frame.function);
    vmpriv_setstackpointer(vm, frame.base_pointer + frame_function->num_locals);
    return true;
}

bool vmpriv_popframe(ApeVM_t* vm)
{
    vmpriv_setstackpointer(vm, vm->current_frame->base_pointer - 1);
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

void vmpriv_collectgarbage(ApeVM_t* vm, ApeArray_t * constants)
{
    ApeSize_t i;
    ApeFrame_t* frame;
    gc_unmark_all(vm->mem);
    gc_mark_objects(global_store_get_object_data(vm->global_store), global_store_get_object_count(vm->global_store));
    gc_mark_objects((ApeObject_t*)array_data(constants), array_count(constants));
    gc_mark_objects(vm->globals, vm->globals_count);
    for(i = 0; i < vm->frames_count; i++)
    {
        frame = &vm->frames[i];
        gc_mark_object(frame->function);
    }
    gc_mark_objects(vm->stack, vm->sp);
    gc_mark_objects(vm->this_stack, vm->this_sp);
    gc_mark_object(vm->last_popped);
    gc_mark_objects(vm->operator_oveload_keys, OPCODE_MAX);
    gc_sweep(vm->mem);
}

bool vmpriv_callobject(ApeVM_t* vm, ApeObject_t callee, int num_args)
{
    bool ok;
    ApeObjectType_t callee_type;
    ApeFunction_t* callee_function;
    ApeFrame_t callee_frame;
    ApeObject_t* stack_pos;
    ApeObject_t objres;
    const char* callee_type_name;
    callee_type = object_value_type(callee);
    if(callee_type == APE_OBJECT_FUNCTION)
    {
        callee_function = object_value_asfunction(callee);
        if(num_args != callee_function->num_args)
        {
            errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                              "invalid number of arguments to \"%s\", expected %d, got %d",
                              object_function_getname(callee), callee_function->num_args, num_args);
            return false;
        }
        ok = frame_init(&callee_frame, callee, vm->sp - num_args);
        if(!ok)
        {
            errorlist_add(vm->errors, APE_ERROR_RUNTIME, g_vmpriv_src_pos_invalid, "frame init failed in vmpriv_callobject");
            return false;
        }
        ok = vmpriv_pushframe(vm, callee_frame);
        if(!ok)
        {
            errorlist_add(vm->errors, APE_ERROR_RUNTIME, g_vmpriv_src_pos_invalid, "pushing frame failed in vmpriv_callobject");
            return false;
        }
    }
    else if(callee_type == APE_OBJECT_NATIVE_FUNCTION)
    {
        stack_pos = vm->stack + vm->sp - num_args;
        objres = vmpriv_callnativefunction(vm, callee, frame_src_position(vm->current_frame), num_args, stack_pos);
        if(vm_has_errors(vm))
        {
            return false;
        }
        vmpriv_setstackpointer(vm, vm->sp - num_args - 1);
        vmpriv_pushstack(vm, objres);
    }
    else
    {
        callee_type_name = object_value_typename(callee_type);
        errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "%s object is not callable", callee_type_name);
        return false;
    }
    return true;
}

ApeObject_t vmpriv_callnativefunction(ApeVM_t* vm, ApeObject_t callee, ApePosition_t src_pos, int argc, ApeObject_t* args)
{
    ApeNativeFunction_t* native_fun = object_value_asnativefunction(callee);
    ApeObject_t objres = native_fun->native_funcptr(vm, native_fun->data, argc, args);
    if(errorlist_haserrors(vm->errors) && !APE_STREQ(native_fun->name, "crash"))
    {
        ApeError_t* err = errorlist_lasterror(vm->errors);
        err->pos = src_pos;
        err->traceback = traceback_make(vm->alloc);
        if(err->traceback)
        {
            traceback_append(err->traceback, native_fun->name, g_vmpriv_src_pos_invalid);
        }
        return object_make_null();
    }
    ApeObjectType_t res_type = object_value_type(objres);
    if(res_type == APE_OBJECT_ERROR)
    {
        ApeTraceback_t* traceback = traceback_make(vm->alloc);
        if(traceback)
        {
            // error builtin is treated in a special way
            if(!APE_STREQ(native_fun->name, "error"))
            {
                traceback_append(traceback, native_fun->name, g_vmpriv_src_pos_invalid);
            }
            traceback_append_from_vm(traceback, vm);
            object_value_seterrortraceback(objres, traceback);
        }
    }
    return objres;
}

bool vmpriv_checkassign(ApeVM_t* vm, ApeObject_t old_value, ApeObject_t new_value)
{
    ApeObjectType_t old_value_type;
    ApeObjectType_t new_value_type;
    (void)vm;
    old_value_type = object_value_type(old_value);
    new_value_type = object_value_type(new_value);
    if(old_value_type == APE_OBJECT_NULL || new_value_type == APE_OBJECT_NULL)
    {
        return true;
    }
    if(old_value_type != new_value_type)
    {
        /*
        errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "trying to assign variable of type %s to %s",
                          object_value_typename(new_value_type), object_value_typename(old_value_type));
        return false;
        */
    }
    return true;
}

static bool vmpriv_tryoverloadoperator(ApeVM_t* vm, ApeObject_t left, ApeObject_t right, ApeOpByte_t op, bool* out_overload_found)
{
    int num_operands;
    ApeObject_t key;
    ApeObject_t callee;
    ApeObjectType_t lefttype;
    ApeObjectType_t righttype;
    *out_overload_found = false;
    lefttype = object_value_type(left);
    righttype = object_value_type(right);
    if(lefttype != APE_OBJECT_MAP && righttype != APE_OBJECT_MAP)
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
    if(lefttype == APE_OBJECT_MAP)
    {
        callee = object_map_getvalueobject(left, key);
    }
    if(!object_value_iscallable(callee))
    {
        if(righttype == APE_OBJECT_MAP)
        {
            callee = object_map_getvalueobject(right, key);
        }
        if(!object_value_iscallable(callee))
        {
            *out_overload_found = false;
            return true;
        }
    }
    *out_overload_found = true;
    vmpriv_pushstack(vm, callee);
    vmpriv_pushstack(vm, left);
    if(num_operands == 2)
    {
        vmpriv_pushstack(vm, right);
    }
    return vmpriv_callobject(vm, callee, num_operands);
}


#define SET_OPERATOR_OVERLOAD_KEY(op, key)                   \
    do                                                       \
    {                                                        \
        key_obj = object_make_string(vm->mem, key); \
        if(object_value_isnull(key_obj))                          \
        {                                                    \
            goto err;                                        \
        }                                                    \
        vm->operator_oveload_keys[op] = key_obj;             \
    } while(0)

ApeVM_t* vm_make(ApeAllocator_t* alloc, const ApeConfig_t* config, ApeGCMemory_t* mem, ApeErrorList_t* errors, ApeGlobalStore_t* global_store)
{
    ApeSize_t i;
    ApeVM_t* vm;
    ApeObject_t key_obj;
    vm = (ApeVM_t*)allocator_malloc(alloc, sizeof(ApeVM_t));
    if(!vm)
    {
        return NULL;
    }
    memset(vm, 0, sizeof(ApeVM_t));
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
    for(i = 0; i < OPCODE_MAX; i++)
    {
        vm->operator_oveload_keys[i] = object_make_null();
    }
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
    return vm;
err:
    vm_destroy(vm);
    return NULL;
}
#undef SET_OPERATOR_OVERLOAD_KEY

void vm_destroy(ApeVM_t* vm)
{
    if(!vm)
    {
        return;
    }
    allocator_free(vm->alloc, vm);
}

void vm_reset(ApeVM_t* vm)
{
    vm->sp = 0;
    vm->this_sp = 0;
    while(vm->frames_count > 0)
    {
        vmpriv_popframe(vm);
    }
}

bool vm_run(ApeVM_t* vm, ApeCompilationResult_t* comp_res, ApeArray_t * constants)
{
    bool res;
    int old_sp;
    int old_this_sp;
    ApeSize_t old_frames_count;
    ApeObject_t main_fn;
#ifdef APE_DEBUG
    old_sp = vm->sp;
#endif
    old_this_sp = vm->this_sp;
    old_frames_count = vm->frames_count;
    main_fn = object_make_function(vm->mem, "main", comp_res, false, 0, 0, 0);
    if(object_value_isnull(main_fn))
    {
        return false;
    }
    vmpriv_pushstack(vm, main_fn);
    res = vm_execute_function(vm, main_fn, constants);
    while(vm->frames_count > old_frames_count)
    {
        vmpriv_popframe(vm);
    }
    APE_ASSERT(vm->sp == old_sp);
    vm->this_sp = old_this_sp;
    return res;
}


bool vmpriv_append_string(ApeVM_t* vm, ApeObject_t left, ApeObject_t right, ApeObjectType_t lefttype, ApeObjectType_t righttype)
{
    bool ok;
    const char* leftstr;
    const char* rightstr;
    ApeObject_t objres;
    ApeInt_t buflen;
    ApeInt_t leftlen;
    ApeInt_t rightlen;
    ApeStringBuffer_t* allbuf;
    ApeStringBuffer_t* tostrbuf;
    (void)lefttype;
    if(righttype == APE_OBJECT_STRING)
    {
        leftlen = (int)object_string_getlength(left);
        rightlen = (int)object_string_getlength(right);
        // avoid doing unnecessary copying by reusing the origin as-is
        if(leftlen == 0)
        {
            vmpriv_pushstack(vm, right);
        }
        else if(rightlen == 0)
        {
            vmpriv_pushstack(vm, left);
        }
        else
        {
            leftstr = object_string_getdata(left);
            rightstr = object_string_getdata(right);
            objres = object_make_string_with_capacity(vm->mem, leftlen + rightlen);
            if(object_value_isnull(objres))
            {
                return false;
            }
            ok = object_string_append(objres, leftstr, leftlen);
            if(!ok)
            {
                return false;
            }
            ok = object_string_append(objres, rightstr, rightlen);
            if(!ok)
            {
                return false;
            }
            vmpriv_pushstack(vm, objres);
        }
    }
    else
    {
        /*
        * when 'right' is not a string, stringify it, and create a new string.
        * in short, this does 'left = left + tostring(right)'
        * TODO: probably really inefficient.
        */
        allbuf = strbuf_make(vm->alloc);
        tostrbuf = strbuf_make(vm->alloc);
        strbuf_appendn(allbuf, object_string_getdata(left), object_string_getlength(left));
        object_tostring(right, tostrbuf, false);
        strbuf_appendn(allbuf, strbuf_get_string(tostrbuf), strbuf_get_length(tostrbuf));
        buflen = strbuf_get_length(allbuf);
        objres = object_make_string_with_capacity(vm->mem, buflen);
        ok = object_string_append(objres, strbuf_get_string(allbuf), buflen);
        strbuf_destroy(tostrbuf);
        strbuf_destroy(allbuf);
        if(!ok)
        {
            return false;
        }
        vmpriv_pushstack(vm, objres);
    }
    return true;
}


/*
* TODO: there is A LOT of code in this function.
* some could be easily split into other functions.
*/
bool vm_execute_function(ApeVM_t* vm, ApeObject_t function, ApeArray_t * constants)
{
    bool checktime;
    bool isoverloaded;
    bool ok;
    bool overloadfound;
    bool resval;
    const ApeFunction_t* constfunction;
    const char* indextypename;
    const char* keytypename;
    const char* left_type_name;
    const char* left_type_string;

    const char* opcode_name;
    const char* operand_type_string;
    const char* right_type_name;
    const char* right_type_string;
    const char* str;
    const char* type_name;
    int elapsed_ms;
    ApeSize_t i;
    int ix;
    int leftlen;
    int len;
    int recover_frame_ix;
    int64_t ui;
    int64_t leftint;
    int64_t rightint;
    uint16_t constant_ix;
    uint16_t count;
    uint16_t items_count;
    uint16_t kvp_count;
    uint16_t recover_ip;
    int64_t val;
    int64_t pos;
    unsigned time_check_counter;
    unsigned time_check_interval;
    ApeUShort_t free_ix;
    ApeUShort_t num_args;
    ApeUShort_t num_free;
    ApeFloat_t comparison_res;
    ApeFloat_t leftval;
    ApeFloat_t maxexecms;
    ApeFloat_t res;
    ApeFloat_t rightval;
    ApeFloat_t valdouble;
    ApeObjectType_t constant_type;
    ApeObjectType_t indextype;
    ApeObjectType_t keytype;
    ApeObjectType_t lefttype;
    ApeObjectType_t operand_type;
    ApeObjectType_t righttype;
    ApeObjectType_t type;
    ApeObject_t array_obj;
    ApeObject_t callee;
    ApeObject_t current_function;
    ApeObject_t err_obj;
    ApeObject_t free_val;
    ApeObject_t function_obj;
    ApeObject_t global;
    ApeObject_t index;
    ApeObject_t key;
    ApeObject_t left;
    ApeObject_t map_obj;
    ApeObject_t new_value;
    ApeObject_t objres;
    ApeObject_t objval;
    ApeObject_t old_value;
    ApeObject_t operand;
    ApeObject_t right;
    ApeObject_t testobj;
    ApeObject_t value;
    ApeObject_t* constant;
    ApeObject_t* items;
    ApeObject_t* kv_pairs;
    ApeOpcodeValue_t opcode;
    ApeTimer_t timer;
    ApeFrame_t* frame;
    ApeError_t* err;
    ApeFrame_t new_frame;
    ApeFunction_t* function_function;
    if(vm->running)
    {
        errorlist_add(vm->errors, APE_ERROR_USER, g_vmpriv_src_pos_invalid, "VM is already executing code");
        return false;
    }
    function_function = object_value_asfunction(function);// naming is hard
    ok = false;
    ok = frame_init(&new_frame, function, vm->sp - function_function->num_args);
    if(!ok)
    {
        return false;
    }
    ok = vmpriv_pushframe(vm, new_frame);
    if(!ok)
    {
        errorlist_add(vm->errors, APE_ERROR_USER, g_vmpriv_src_pos_invalid, "pushing frame failed");
        return false;
    }
    vm->running = true;
    vm->last_popped = object_make_null();
    checktime = false;
    maxexecms = 0;
    if(vm->config)
    {
        checktime = vm->config->max_execution_time_set;
        maxexecms = vm->config->max_execution_time_ms;
    }
    time_check_interval = 1000;
    time_check_counter = 0;
    memset(&timer, 0, sizeof(ApeTimer_t));
    if(checktime)
    {
        timer = util_timer_start();
    }
    while(vm->current_frame->ip < vm->current_frame->bytecode_size)
    {
        opcode = frame_read_opcode(vm->current_frame);
        switch(opcode)
        {
            case OPCODE_CONSTANT:
                {
                    constant_ix = frame_read_uint16(vm->current_frame);
                    constant = (ApeObject_t*)array_get(constants, constant_ix);
                    if(!constant)
                    {
                        errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "constant at %d not found", constant_ix);
                        goto err;
                    }
                    vmpriv_pushstack(vm, *constant);
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
                    right = vmpriv_popstack(vm);
                    left = vmpriv_popstack(vm);
                    // NULL to 0 coercion
                    if(object_value_isnumeric(left) && object_value_isnull(right))
                    {
                        right = object_make_number(0);
                    }
                    if(object_value_isnumeric(right) && object_value_isnull(left))
                    {
                        left = object_make_number(0);
                    }
                    lefttype = object_value_type(left);
                    righttype = object_value_type(right);
                    if(object_value_isnumeric(left) && object_value_isnumeric(right))
                    {
                        rightval = object_value_asnumber(right);
                        leftval = object_value_asnumber(left);
                        leftint = (int64_t)leftval;
                        rightint = (int64_t)rightval;
                        res = 0;
                        switch(opcode)
                        {
                            case OPCODE_ADD:
                                {
                                    res = leftval + rightval;
                                }
                                break;
                            case OPCODE_SUB:
                                {
                                    res = leftval - rightval;
                                }
                                break;
                            case OPCODE_MUL:
                                {
                                    res = leftval * rightval;
                                }
                                break;
                            case OPCODE_DIV:
                                {
                                    res = leftval / rightval;
                                }
                                break;
                            case OPCODE_MOD:
                                {
                                    //res = fmod(leftval, rightval);
                                    res = (leftint % rightint);
                                }
                                break;
                            case OPCODE_OR:
                                {
                                    res = (ApeFloat_t)(leftint | rightint);
                                }
                                break;
                            case OPCODE_XOR:
                                {
                                    res = (ApeFloat_t)(leftint ^ rightint);
                                }
                                break;
                            case OPCODE_AND:
                                {
                                    res = (ApeFloat_t)(leftint & rightint);
                                }
                                break;
                            case OPCODE_LSHIFT:
                                {
                                    res = (ApeFloat_t)(leftint << rightint);
                                }
                                break;
                            case OPCODE_RSHIFT:
                                {
                                    res = (ApeFloat_t)(leftint >> rightint);
                                }
                                break;
                            default:
                                {
                                    APE_ASSERT(false);
                                }
                                break;
                        }
                        vmpriv_pushstack(vm, object_make_number(res));
                    }
                    else if(lefttype == APE_OBJECT_STRING && ((righttype == APE_OBJECT_STRING) || (righttype == APE_OBJECT_NUMBER)) && opcode == OPCODE_ADD)
                    {
                        ok = vmpriv_append_string(vm, left, right, lefttype, righttype);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                    else if((lefttype == APE_OBJECT_ARRAY) && opcode == OPCODE_ADD)
                    {
                        object_array_pushvalue(left, right);
                        vmpriv_pushstack(vm, left);
                    }
                    else
                    {
                        overloadfound = false;
                        ok = vmpriv_tryoverloadoperator(vm, left, right, opcode, &overloadfound);
                        if(!ok)
                        {
                            goto err;
                        }
                        if(!overloadfound)
                        {
                            opcode_name = opcode_get_name(opcode);
                            left_type_name = object_value_typename(lefttype);
                            right_type_name = object_value_typename(righttype);
                            errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "invalid operand types for %s, got %s and %s", opcode_name, left_type_name,
                                              right_type_name);
                            goto err;
                        }
                    }
                }
                break;
            case OPCODE_POP:
                {
                    vmpriv_popstack(vm);
                }
                break;
            case OPCODE_TRUE:
                {
                    vmpriv_pushstack(vm, object_make_bool(true));
                }
                break;
            case OPCODE_FALSE:
                {
                    vmpriv_pushstack(vm, object_make_bool(false));
                }
                break;
            case OPCODE_COMPARE:
            case OPCODE_COMPARE_EQ:
                {
                    right = vmpriv_popstack(vm);
                    left = vmpriv_popstack(vm);
                    isoverloaded = false;
                    ok = vmpriv_tryoverloadoperator(vm, left, right, OPCODE_COMPARE, &isoverloaded);
                    if(!ok)
                    {
                        goto err;
                    }
                    if(!isoverloaded)
                    {
                        comparison_res = object_value_compare(left, right, &ok);
                        if(ok || opcode == OPCODE_COMPARE_EQ)
                        {
                            objres = object_make_number(comparison_res);
                            vmpriv_pushstack(vm, objres);
                        }
                        else
                        {
                            right_type_string = object_value_typename(object_value_type(right));
                            left_type_string = object_value_typename(object_value_type(left));
                            errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "cannot compare %s and %s", left_type_string, right_type_string);
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
                    value = vmpriv_popstack(vm);
                    comparison_res = object_value_asnumber(value);
                    resval = false;
                    switch(opcode)
                    {
                        case OPCODE_EQUAL:
                            resval = APE_DBLEQ(comparison_res, 0);
                            break;
                        case OPCODE_NOT_EQUAL:
                            resval = !APE_DBLEQ(comparison_res, 0);
                            break;
                        case OPCODE_GREATER_THAN:
                            resval = comparison_res > 0;
                            break;
                        case OPCODE_GREATER_THAN_EQUAL:
                            {
                                resval = comparison_res > 0 || APE_DBLEQ(comparison_res, 0);
                            }
                            break;
                        default:
                            {
                                APE_ASSERT(false);
                            }
                            break;
                    }
                    objres = object_make_bool(resval);
                    vmpriv_pushstack(vm, objres);
                }
                break;

            case OPCODE_MINUS:
                {
                    operand = vmpriv_popstack(vm);
                    operand_type = object_value_type(operand);
                    if(operand_type == APE_OBJECT_NUMBER)
                    {
                        val = object_value_asnumber(operand);
                        objres = object_make_number(-val);
                        vmpriv_pushstack(vm, objres);
                    }
                    else
                    {
                        overloadfound = false;
                        ok = vmpriv_tryoverloadoperator(vm, operand, object_make_null(), OPCODE_MINUS, &overloadfound);
                        if(!ok)
                        {
                            goto err;
                        }
                        if(!overloadfound)
                        {
                            operand_type_string = object_value_typename(operand_type);
                            errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "invalid operand type for MINUS, got %s", operand_type_string);
                            goto err;
                        }
                    }
                }
                break;

            case OPCODE_BANG:
                {
                    operand = vmpriv_popstack(vm);
                    type = object_value_type(operand);
                    if(type == APE_OBJECT_BOOL)
                    {
                        objres = object_make_bool(!object_value_asbool(operand));
                        vmpriv_pushstack(vm, objres);
                    }
                    else if(type == APE_OBJECT_NULL)
                    {
                        objres = object_make_bool(true);
                        vmpriv_pushstack(vm, objres);
                    }
                    else
                    {
                        overloadfound = false;
                        ok = vmpriv_tryoverloadoperator(vm, operand, object_make_null(), OPCODE_BANG, &overloadfound);
                        if(!ok)
                        {
                            goto err;
                        }
                        if(!overloadfound)
                        {
                            ApeObject_t objres = object_make_bool(false);
                            vmpriv_pushstack(vm, objres);
                        }
                    }
                }
                break;

            case OPCODE_JUMP:
                {
                    pos = frame_read_uint16(vm->current_frame);
                    vm->current_frame->ip = pos;
                }
                break;

            case OPCODE_JUMP_IF_FALSE:
                {
                    pos = frame_read_uint16(vm->current_frame);
                    testobj = vmpriv_popstack(vm);
                    if(!object_value_asbool(testobj))
                    {
                        vm->current_frame->ip = pos;
                    }
                }
                break;

            case OPCODE_JUMP_IF_TRUE:
                {
                    pos = frame_read_uint16(vm->current_frame);
                    testobj = vmpriv_popstack(vm);
                    if(object_value_asbool(testobj))
                    {
                        vm->current_frame->ip = pos;
                    }
                }
                break;

            case OPCODE_NULL:
                {
                    vmpriv_pushstack(vm, object_make_null());
                }
                break;

            case OPCODE_DEFINE_MODULE_GLOBAL:
            {
                ix = frame_read_uint16(vm->current_frame);
                objval = vmpriv_popstack(vm);
                vm_set_global(vm, ix, objval);
                break;
            }
            case OPCODE_SET_MODULE_GLOBAL:
                {
                    ix = frame_read_uint16(vm->current_frame);
                    new_value = vmpriv_popstack(vm);
                    old_value = vm_get_global(vm, ix);
                    if(!vmpriv_checkassign(vm, old_value, new_value))
                    {
                        goto err;
                    }
                    vm_set_global(vm, ix, new_value);
                }
                break;

            case OPCODE_GET_MODULE_GLOBAL:
                {
                    ix = frame_read_uint16(vm->current_frame);
                    global = vm->globals[ix];
                    vmpriv_pushstack(vm, global);
                }
                break;

            case OPCODE_ARRAY:
                {
                    count = frame_read_uint16(vm->current_frame);
                    array_obj = object_make_array_with_capacity(vm->mem, count);
                    if(object_value_isnull(array_obj))
                    {
                        goto err;
                    }
                    items = vm->stack + vm->sp - count;
                    for(i = 0; i < count; i++)
                    {
                        ApeObject_t item = items[i];
                        ok = object_array_pushvalue(array_obj, item);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                    vmpriv_setstackpointer(vm, vm->sp - count);
                    vmpriv_pushstack(vm, array_obj);
                }
                break;

            case OPCODE_MAP_START:
                {
                    count = frame_read_uint16(vm->current_frame);
                    map_obj = object_make_map_with_capacity(vm->mem, count);
                    if(object_value_isnull(map_obj))
                    {
                        goto err;
                    }
                    vmpriv_pushthisstack(vm, map_obj);
                }
                break;
            case OPCODE_MAP_END:
                {
                    kvp_count = frame_read_uint16(vm->current_frame);
                    items_count = kvp_count * 2;
                    map_obj = vmpriv_popthisstack(vm);
                    kv_pairs = vm->stack + vm->sp - items_count;
                    for(i = 0; i < items_count; i += 2)
                    {
                        key = kv_pairs[i];
                        if(!object_value_ishashable(key))
                        {
                            keytype = object_value_type(key);
                            keytypename = object_value_typename(keytype);
                            errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "key of type %s is not hashable", keytypename);
                            goto err;
                        }
                        objval = kv_pairs[i + 1];
                        ok = object_map_setvalue(map_obj, key, objval);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                    vmpriv_setstackpointer(vm, vm->sp - items_count);
                    vmpriv_pushstack(vm, map_obj);
                }
                break;

            case OPCODE_GET_THIS:
                {
                    objval = vmpriv_getthisstack(vm, 0);
                    vmpriv_pushstack(vm, objval);
                }
                break;

            case OPCODE_GET_INDEX:
                {
                    #if 0
                    const char* idxname;
                    #endif
                    index = vmpriv_popstack(vm);
                    left = vmpriv_popstack(vm);
                    lefttype = object_value_type(left);
                    indextype = object_value_type(index);
                    left_type_name = object_value_typename(lefttype);
                    indextypename = object_value_typename(indextype);
                    /*
                    * todo: object method lookup could be implemented here
                    */
                    #if 0
                    {
                        int argc;
                        ApeObject_t args[10];
                        ApeNativeFunc_t afn;
                        argc = 0;
                        if(indextype == APE_OBJECT_STRING)
                        {
                            idxname = object_string_getdata(index);
                            fprintf(stderr, "index is a string: name=%s\n", idxname);
                            if((afn = builtin_get_object(lefttype, idxname)) != NULL)
                            {
                                fprintf(stderr, "got a callback: afn=%p\n", afn);
                                //typedef ApeObject_t (*ApeNativeFunc_t)(ApeVM_t*, void*, int, ApeObject_t*);
                                args[argc] = left;
                                argc++;
                                vmpriv_pushstack(vm, afn(vm, NULL, argc, args));
                                break;
                            }
                        }
                    }
                    #endif

                    if(lefttype != APE_OBJECT_ARRAY && lefttype != APE_OBJECT_MAP && lefttype != APE_OBJECT_STRING)
                    {
                        errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                            "type %s is not indexable (in OPCODE_GET_INDEX)", left_type_name);
                        goto err;
                    }
                    objres = object_make_null();
                    if(lefttype == APE_OBJECT_ARRAY)
                    {
                        if(indextype != APE_OBJECT_NUMBER)
                        {
                            errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "cannot index %s with %s", left_type_name, indextypename);
                            goto err;
                        }
                        ix = (int)object_value_asnumber(index);
                        if(ix < 0)
                        {
                            ix = object_array_getlength(left) + ix;
                        }
                        if(ix >= 0 && ix < object_array_getlength(left))
                        {
                            objres = object_array_getvalue(left, ix);
                        }
                    }
                    else if(lefttype == APE_OBJECT_MAP)
                    {
                        objres = object_map_getvalueobject(left, index);
                    }
                    else if(lefttype == APE_OBJECT_STRING)
                    {
                        str = object_string_getdata(left);
                        leftlen = object_string_getlength(left);
                        ix = (int)object_value_asnumber(index);
                        if(ix >= 0 && ix < leftlen)
                        {
                            char res_str[2] = { str[ix], '\0' };
                            objres = object_make_string(vm->mem, res_str);
                        }
                    }
                    vmpriv_pushstack(vm, objres);
                }
                break;

            case OPCODE_GET_VALUE_AT:
            {
                index = vmpriv_popstack(vm);
                left = vmpriv_popstack(vm);
                lefttype = object_value_type(left);
                indextype = object_value_type(index);
                left_type_name = object_value_typename(lefttype);
                indextypename = object_value_typename(indextype);
                if(lefttype != APE_OBJECT_ARRAY && lefttype != APE_OBJECT_MAP && lefttype != APE_OBJECT_STRING)
                {
                    errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                      "type %s is not indexable (in OPCODE_GET_VALUE_AT)", left_type_name);
                    goto err;
                }
                objres = object_make_null();
                if(indextype != APE_OBJECT_NUMBER)
                {
                    errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                      "cannot index %s with %s", left_type_name, indextypename);
                    goto err;
                }
                ix = (int)object_value_asnumber(index);
                if(lefttype == APE_OBJECT_ARRAY)
                {
                    objres = object_array_getvalue(left, ix);
                }
                else if(lefttype == APE_OBJECT_MAP)
                {
                    objres = object_get_kv_pair_at(vm->mem, left, ix);
                }
                else if(lefttype == APE_OBJECT_STRING)
                {
                    str = object_string_getdata(left);
                    leftlen = object_string_getlength(left);
                    ix = (int)object_value_asnumber(index);
                    if(ix >= 0 && ix < leftlen)
                    {
                        char res_str[2] = { str[ix], '\0' };
                        objres = object_make_string(vm->mem, res_str);
                    }
                }
                vmpriv_pushstack(vm, objres);
                break;
            }
            case OPCODE_CALL:
            {
                num_args = frame_read_uint8(vm->current_frame);
                callee = vmpriv_getstack(vm, num_args);
                ok = vmpriv_callobject(vm, callee, num_args);
                if(!ok)
                {
                    goto err;
                }
                break;
            }
            case OPCODE_RETURN_VALUE:
            {
                objres = vmpriv_popstack(vm);
                ok = vmpriv_popframe(vm);
                if(!ok)
                {
                    goto end;
                }
                vmpriv_pushstack(vm, objres);
                break;
            }
            case OPCODE_RETURN:
            {
                ok = vmpriv_popframe(vm);
                vmpriv_pushstack(vm, object_make_null());
                if(!ok)
                {
                    vmpriv_popstack(vm);
                    goto end;
                }
                break;
            }
            case OPCODE_DEFINE_LOCAL:
            {
                pos = frame_read_uint8(vm->current_frame);
                vm->stack[vm->current_frame->base_pointer + pos] = vmpriv_popstack(vm);
                break;
            }
            case OPCODE_SET_LOCAL:
            {
                pos = frame_read_uint8(vm->current_frame);
                new_value = vmpriv_popstack(vm);
                old_value = vm->stack[vm->current_frame->base_pointer + pos];
                if(!vmpriv_checkassign(vm, old_value, new_value))
                {
                    goto err;
                }
                vm->stack[vm->current_frame->base_pointer + pos] = new_value;
                break;
            }
            case OPCODE_GET_LOCAL:
            {
                pos = frame_read_uint8(vm->current_frame);
                objval = vm->stack[vm->current_frame->base_pointer + pos];
                vmpriv_pushstack(vm, objval);
                break;
            }
            case OPCODE_GET_APE_GLOBAL:
            {
                ix = frame_read_uint16(vm->current_frame);
                ok = false;
                objval = global_store_get_object_at(vm->global_store, ix, &ok);
                if(!ok)
                {
                    errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                      "global value %d not found", ix);
                    goto err;
                }
                vmpriv_pushstack(vm, objval);
                break;
            }
            case OPCODE_FUNCTION:
                {
                    constant_ix = frame_read_uint16(vm->current_frame);
                    num_free = frame_read_uint8(vm->current_frame);
                    constant = (ApeObject_t*)array_get(constants, constant_ix);
                    if(!constant)
                    {
                        errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "constant %d not found", constant_ix);
                        goto err;
                    }
                    constant_type = object_value_type(*constant);
                    if(constant_type != APE_OBJECT_FUNCTION)
                    {
                        type_name = object_value_typename(constant_type);
                        errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "%s is not a function", type_name);
                        goto err;
                    }
                    constfunction = object_value_asfunction(*constant);
                    function_obj = object_make_function(vm->mem, object_function_getname(*constant), constfunction->comp_result,
                                           false, constfunction->num_locals, constfunction->num_args, num_free);
                    if(object_value_isnull(function_obj))
                    {
                        goto err;
                    }
                    for(i = 0; i < num_free; i++)
                    {
                        free_val = vm->stack[vm->sp - num_free + i];
                        object_set_function_free_val(function_obj, i, free_val);
                    }
                    vmpriv_setstackpointer(vm, vm->sp - num_free);
                    vmpriv_pushstack(vm, function_obj);
                }
                break;
            case OPCODE_GET_FREE:
                {
                    free_ix = frame_read_uint8(vm->current_frame);
                    objval = object_function_getfreeval(vm->current_frame->function, free_ix);
                    vmpriv_pushstack(vm, objval);
                }
                break;
            case OPCODE_SET_FREE:
                {
                    free_ix = frame_read_uint8(vm->current_frame);
                    objval = vmpriv_popstack(vm);
                    object_set_function_free_val(vm->current_frame->function, free_ix, objval);
                }
                break;
            case OPCODE_CURRENT_FUNCTION:
                {
                    current_function = vm->current_frame->function;
                    vmpriv_pushstack(vm, current_function);
                }
                break;
            case OPCODE_SET_INDEX:
                {
                    index = vmpriv_popstack(vm);
                    left = vmpriv_popstack(vm);
                    new_value = vmpriv_popstack(vm);
                    lefttype = object_value_type(left);
                    indextype = object_value_type(index);
                    left_type_name = object_value_typename(lefttype);
                    indextypename = object_value_typename(indextype);
                    if(lefttype != APE_OBJECT_ARRAY && lefttype != APE_OBJECT_MAP)
                    {
                        errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "type %s is not indexable (in OPCODE_SET_INDEX)", left_type_name);
                        goto err;
                    }

                    if(lefttype == APE_OBJECT_ARRAY)
                    {
                        if(indextype != APE_OBJECT_NUMBER)
                        {
                            errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "cannot index %s with %s", left_type_name, indextypename);
                            goto err;
                        }
                        ix = (int)object_value_asnumber(index);
                        ok = object_array_setat(left, ix, new_value);
                        if(!ok)
                        {
                            errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                             "setting array item failed (index %d out of bounds of %d)", ix, object_array_getlength(left));
                            goto err;
                        }
                    }
                    else if(lefttype == APE_OBJECT_MAP)
                    {
                        old_value = object_map_getvalueobject(left, index);
                        if(!vmpriv_checkassign(vm, old_value, new_value))
                        {
                            goto err;
                        }
                        ok = object_map_setvalue(left, index, new_value);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                }
                break;
            case OPCODE_DUP:
                {
                    objval = vmpriv_getstack(vm, 0);
                    vmpriv_pushstack(vm, objval);
                }
                break;
            case OPCODE_LEN:
                {
                    objval = vmpriv_popstack(vm);
                    len = 0;
                    type = object_value_type(objval);
                    if(type == APE_OBJECT_ARRAY)
                    {
                        len = object_array_getlength(objval);
                    }
                    else if(type == APE_OBJECT_MAP)
                    {
                        len = object_map_getlength(objval);
                    }
                    else if(type == APE_OBJECT_STRING)
                    {
                        len = object_string_getlength(objval);
                    }
                    else
                    {
                        type_name = object_value_typename(type);
                        errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "cannot get length of %s", type_name);
                        goto err;
                    }
                    vmpriv_pushstack(vm, object_make_number(len));
                }
                break;
            case OPCODE_NUMBER:
                {
                    // FIXME: why does util_uint64_to_double break things here?
                    val = frame_read_uint64(vm->current_frame);
                    //valdouble = util_uint64_to_double(val);
                    valdouble = val;
                    objval = object_make_number(valdouble);
                    vmpriv_pushstack(vm, objval);
                }
                break;
            case OPCODE_SET_RECOVER:
                {
                    recover_ip = frame_read_uint16(vm->current_frame);
                    vm->current_frame->recover_ip = recover_ip;
                }
                break;
            default:
                {
                    APE_ASSERT(false);
                    errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "unknown opcode: 0x%x", opcode);
                    goto err;
                }
                break;
        }
        if(checktime)
        {
            time_check_counter++;
            if(time_check_counter > time_check_interval)
            {
                elapsed_ms = (int)util_timer_getelapsed(&timer);
                if(elapsed_ms > maxexecms)
                {
                    errorlist_addformat(vm->errors, APE_ERROR_TIMEOUT, frame_src_position(vm->current_frame),
                                      "execution took more than %1.17g ms", maxexecms);
                    goto err;
                }
                time_check_counter = 0;
            }
        }
    err:
        if(errorlist_count(vm->errors) > 0)
        {
            err = errorlist_lasterror(vm->errors);
            if(err->type == APE_ERROR_RUNTIME && errorlist_count(vm->errors) == 1)
            {
                recover_frame_ix = -1;
                for(ui = vm->frames_count - 1; ui >= 0; ui--)
                {
                    frame = &vm->frames[ui];
                    if(frame->recover_ip >= 0 && !frame->is_recovering)
                    {
                        recover_frame_ix = ui;
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
                        vmpriv_popframe(vm);
                    }
                    err_obj = object_make_error(vm->mem, err->message);
                    if(!object_value_isnull(err_obj))
                    {
                        object_value_seterrortraceback(err_obj, err->traceback);
                        err->traceback = NULL;
                    }
                    vmpriv_pushstack(vm, err_obj);
                    vm->current_frame->ip = vm->current_frame->recover_ip;
                    vm->current_frame->is_recovering = true;
                    errorlist_clear(vm->errors);
                }
            }
            else
            {
                goto end;
            }
        }
        if(gc_should_sweep(vm->mem))
        {
            vmpriv_collectgarbage(vm, constants);
        }
    }
end:
    if(errorlist_count(vm->errors) > 0)
    {
        err = errorlist_lasterror(vm->errors);
        if(!err->traceback)
        {
            err->traceback = traceback_make(vm->alloc);
        }
        if(err->traceback)
        {
            traceback_append_from_vm(err->traceback, vm);
        }
    }

    vmpriv_collectgarbage(vm, constants);

    vm->running = false;
    return errorlist_count(vm->errors) == 0;
}
