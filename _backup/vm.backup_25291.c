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


static bool ape_vm_setsymbol(ApeSymTable_t *table, ApeSymbol_t *symbol);
static int ape_vm_nextsymbolindex(ApeSymTable_t *table);
static int ape_vm_countnumdefs(ApeSymTable_t *table);
static void ape_vm_setstackpointer(ApeVM_t *vm, int new_sp);
static void ape_vm_pushthisstack(ApeVM_t *vm, ApeObject_t obj);
static ApeObject_t ape_vm_popthisstack(ApeVM_t *vm);
static ApeObject_t ape_vm_getthisstack(ApeVM_t *vm, int nth_item);
static bool ape_vm_tryoverloadoperator(ApeVM_t *vm, ApeObject_t left, ApeObject_t right, ApeOpByte_t op, bool *out_overload_found);


ApeCompiledFile_t* ape_make_compiledfile(ApeContext_t* ctx, const char* path)
{
    size_t len;
    const char* last_slash_pos;
    ApeCompiledFile_t* file;
    file = (ApeCompiledFile_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeCompiledFile_t));
    if(!file)
    {
        return NULL;
    }
    memset(file, 0, sizeof(ApeCompiledFile_t));
    file->context = ctx;
    file->alloc = &ctx->alloc;
    last_slash_pos = strrchr(path, '/');
    if(last_slash_pos)
    {
        len = last_slash_pos - path + 1;
        file->dirpath = ape_util_strndup(ctx, path, len);
    }
    else
    {
        file->dirpath = ape_util_strdup(ctx, "");
    }
    if(!file->dirpath)
    {
        goto error;
    }
    file->path = ape_util_strdup(ctx, path);
    if(!file->path)
    {
        goto error;
    }
    file->lines = ape_make_ptrarray(ctx);
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
    for(i = 0; i < ape_ptrarray_count(file->lines); i++)
    {
        item = (void*)ape_ptrarray_get(file->lines, i);
        ape_allocator_free(file->alloc, item);
    }
    ape_ptrarray_destroy(file->lines);
    ape_allocator_free(file->alloc, file->dirpath);
    ape_allocator_free(file->alloc, file->path);
    ape_allocator_free(file->alloc, file);
    return NULL;
}

ApeGlobalStore_t* ape_make_globalstore(ApeContext_t* ctx, ApeGCMemory_t* mem)
{
    ApeSize_t i;
    bool ok;
    const char* name;
    ApeObject_t builtin;
    ApeGlobalStore_t* store;
    store = (ApeGlobalStore_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeGlobalStore_t));
    if(!store)
    {
        return NULL;
    }
    memset(store, 0, sizeof(ApeGlobalStore_t));
    store->context = ctx;
    store->alloc = &ctx->alloc;
    store->symbols = ape_make_strdict(ctx, (ApeDataCallback_t)ape_symbol_copy, (ApeDataCallback_t)ape_symbol_destroy);
    if(!store->symbols)
    {
        goto err;
    }
    store->objects = array_make(ctx, ApeObject_t);
    if(!store->objects)
    {
        goto err;
    }
    if(mem)
    {
        for(i = 0; i < builtins_count(); i++)
        {
            name = builtins_get_name(i);
            builtin = ape_object_make_nativefuncmemory(ctx, name, builtins_get_fn(i), NULL, 0);
            if(object_value_isnull(builtin))
            {
                goto err;
            }
            ok = ape_globalstore_set(store, name, builtin);
            if(!ok)
            {
                goto err;
            }
        }
    }
    return store;
err:
    ape_globalstore_destroy(store);
    return NULL;
}

void ape_globalstore_destroy(ApeGlobalStore_t* store)
{
    if(!store)
    {
        return;
    }
    ape_strdict_destroywithitems(store->symbols);
    ape_valarray_destroy(store->objects);
    ape_allocator_free(store->alloc, store);
}

const ApeSymbol_t* ape_globalstore_getsymbol(ApeGlobalStore_t* store, const char* name)
{
    return (ApeSymbol_t*)ape_strdict_get(store->symbols, name);
}

bool ape_globalstore_set(ApeGlobalStore_t* store, const char* name, ApeObject_t object)
{
    int ix;
    bool ok;
    ApeSymbol_t* symbol;
    const ApeSymbol_t* existing_symbol;
    existing_symbol = ape_globalstore_getsymbol(store, name);
    if(existing_symbol)
    {
        ok = ape_valarray_set(store->objects, existing_symbol->index, &object);
        return ok;
    }
    ix = ape_valarray_count(store->objects);
    ok = ape_valarray_add(store->objects, &object);
    if(!ok)
    {
        return false;
    }
    symbol = ape_make_symbol(store->context, name, APE_SYMBOL_CONTEXTGLOBAL, ix, false);
    if(!symbol)
    {
        goto err;
    }
    ok = ape_strdict_set(store->symbols, name, symbol);
    if(!ok)
    {
        ape_symbol_destroy(symbol);
        goto err;
    }
    return true;
err:
    ape_valarray_pop(store->objects, NULL);
    return false;
}

ApeObject_t ape_globalstore_getat(ApeGlobalStore_t* store, int ix, bool* out_ok)
{
    ApeObject_t* res = (ApeObject_t*)ape_valarray_get(store->objects, ix);
    if(!res)
    {
        *out_ok = false;
        return ape_object_make_null(store->context);
    }
    *out_ok = true;
    return *res;
}

ApeObject_t* ape_globalstore_getobjectdata(ApeGlobalStore_t* store)
{
    return (ApeObject_t*)ape_valarray_data(store->objects);
}

ApeSize_t ape_globalstore_getobjectcount(ApeGlobalStore_t* store)
{
    return ape_valarray_count(store->objects);
}

ApeSymbol_t* ape_make_symbol(ApeContext_t* ctx, const char* name, ApeSymbolType_t type, int index, bool assignable)
{
    ApeSymbol_t* symbol;
    symbol = (ApeSymbol_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeSymbol_t));
    if(!symbol)
    {
        return NULL;
    }
    memset(symbol, 0, sizeof(ApeSymbol_t));
    symbol->context = ctx;
    symbol->alloc = &ctx->alloc;
    symbol->name = ape_util_strdup(ctx, name);
    if(!symbol->name)
    {
        ape_allocator_free(&ctx->alloc, symbol);
        return NULL;
    }
    symbol->type = type;
    symbol->index = index;
    symbol->assignable = assignable;
    return symbol;
}

void* ape_symbol_destroy(ApeSymbol_t* symbol)
{
    if(!symbol)
    {
        return NULL;
    }
    ape_allocator_free(symbol->alloc, symbol->name);
    ape_allocator_free(symbol->alloc, symbol);
    return NULL;
}

ApeSymbol_t* ape_symbol_copy(ApeSymbol_t* symbol)
{
    return ape_make_symbol(symbol->context, symbol->name, symbol->type, symbol->index, symbol->assignable);
}

ApeSymTable_t* ape_make_symtable(ApeContext_t* ctx, ApeSymTable_t* outer, ApeGlobalStore_t* global_store, int module_global_offset)
{
    bool ok;
    ApeSymTable_t* table;
    table = (ApeSymTable_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeSymTable_t));
    if(!table)
    {
        return NULL;
    }
    memset(table, 0, sizeof(ApeSymTable_t));
    table->context = ctx;
    table->alloc = &ctx->alloc;
    table->maxnumdefinitions = 0;
    table->outer = outer;
    table->globalstore = global_store;
    table->module_global_offset = module_global_offset;
    table->blockscopes = ape_make_ptrarray(ctx);
    if(!table->blockscopes)
    {
        goto err;
    }
    table->freesymbols = ape_make_ptrarray(ctx);
    if(!table->freesymbols)
    {
        goto err;
    }
    table->module_global_symbols = ape_make_ptrarray(ctx);
    if(!table->module_global_symbols)
    {
        goto err;
    }
    ok = ape_symtable_pushblockscope(table);
    if(!ok)
    {
        goto err;
    }
    return table;
err:
    ape_symtable_destroy(table);
    return NULL;
}

void ape_symtable_destroy(ApeSymTable_t* table)
{
    ApeAllocator_t* alloc;
    if(!table)
    {
        return;
    }
    while(ape_ptrarray_count(table->blockscopes) > 0)
    {
        ape_symtable_popblockscope(table);
    }
    ape_ptrarray_destroy(table->blockscopes);
    ape_ptrarray_destroywithitems(table->module_global_symbols, (ApeDataCallback_t)ape_symbol_destroy);
    ape_ptrarray_destroywithitems(table->freesymbols, (ApeDataCallback_t)ape_symbol_destroy);
    alloc = table->alloc;
    memset(table, 0, sizeof(ApeSymTable_t));
    ape_allocator_free(alloc, table);
}

ApeSymTable_t* ape_symtable_copy(ApeSymTable_t* table)
{
    ApeSymTable_t* copy;
    copy = (ApeSymTable_t*)ape_allocator_alloc(table->alloc, sizeof(ApeSymTable_t));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(ApeSymTable_t));
    copy->alloc = table->alloc;
    copy->outer = table->outer;
    copy->globalstore = table->globalstore;
    copy->blockscopes = ape_ptrarray_copywithitems(table->blockscopes, (ApeDataCallback_t)ape_blockscope_copy, (ApeDataCallback_t)ape_blockscope_destroy);
    if(!copy->blockscopes)
    {
        goto err;
    }
    copy->freesymbols = ape_ptrarray_copywithitems(table->freesymbols, (ApeDataCallback_t)ape_symbol_copy, (ApeDataCallback_t)ape_symbol_destroy);
    if(!copy->freesymbols)
    {
        goto err;
    }
    copy->module_global_symbols = ape_ptrarray_copywithitems(table->module_global_symbols, (ApeDataCallback_t)ape_symbol_copy, (ApeDataCallback_t)ape_symbol_destroy);
    if(!copy->module_global_symbols)
    {
        goto err;
    }
    copy->maxnumdefinitions = table->maxnumdefinitions;
    copy->module_global_offset = table->module_global_offset;
    return copy;
err:
    ape_symtable_destroy(copy);
    return NULL;
}

bool ape_symtable_addmodulesymbol(ApeSymTable_t* st, ApeSymbol_t* symbol)
{
    bool ok;
    if(symbol->type != APE_SYMBOL_MODULEGLOBAL)
    {
        APE_ASSERT(false);
        return false;
    }
    if(ape_symtable_symbol_is_defined(st, symbol->name))
    {
        return true;// todo: make sure it should be true in this case
    }
    ApeSymbol_t* copy = ape_symbol_copy(symbol);
    if(!copy)
    {
        return false;
    }
    ok = ape_vm_setsymbol(st, copy);
    if(!ok)
    {
        ape_symbol_destroy(copy);
        return false;
    }
    return true;
}

const ApeSymbol_t* ape_symtable_define(ApeSymTable_t* table, const char* name, bool assignable)
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
    global_symbol = ape_globalstore_getsymbol(table->globalstore, name);
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
    symbol_type = table->outer == NULL ? APE_SYMBOL_MODULEGLOBAL : APE_SYMBOL_LOCAL;
    ix = ape_vm_nextsymbolindex(table);
    symbol = ape_make_symbol(table->context, name, symbol_type, ix, assignable);
    if(!symbol)
    {
        return NULL;
    }
    global_symbol_added = false;
    ok = false;
    if(symbol_type == APE_SYMBOL_MODULEGLOBAL && ape_ptrarray_count(table->blockscopes) == 1)
    {
        global_symbol_copy = ape_symbol_copy(symbol);
        if(!global_symbol_copy)
        {
            ape_symbol_destroy(symbol);
            return NULL;
        }
        ok = ape_ptrarray_add(table->module_global_symbols, global_symbol_copy);
        if(!ok)
        {
            ape_symbol_destroy(global_symbol_copy);
            ape_symbol_destroy(symbol);
            return NULL;
        }
        global_symbol_added = true;
    }
    ok = ape_vm_setsymbol(table, symbol);
    if(!ok)
    {
        if(global_symbol_added)
        {
            global_symbol_copy = (ApeSymbol_t*)ape_ptrarray_pop(table->module_global_symbols);
            ape_symbol_destroy(global_symbol_copy);
        }
        ape_symbol_destroy(symbol);
        return NULL;
    }
    top_scope = (ApeBlockScope_t*)ape_ptrarray_top(table->blockscopes);
    top_scope->numdefinitions++;
    definitions_count = ape_vm_countnumdefs(table);
    if(definitions_count > table->maxnumdefinitions)
    {
        table->maxnumdefinitions = definitions_count;
    }

    return symbol;
}

const ApeSymbol_t* ape_symtable_deffree(ApeSymTable_t* st, const ApeSymbol_t* original)
{
    bool ok;
    ApeSymbol_t* symbol;
    ApeSymbol_t* copy;
    copy = ape_make_symbol(st->context, original->name, original->type, original->index, original->assignable);
    if(!copy)
    {
        return NULL;
    }
    ok = ape_ptrarray_add(st->freesymbols, copy);
    if(!ok)
    {
        ape_symbol_destroy(copy);
        return NULL;
    }

    symbol = ape_make_symbol(st->context, original->name, APE_SYMBOL_FREE, ape_ptrarray_count(st->freesymbols) - 1, original->assignable);
    if(!symbol)
    {
        return NULL;
    }

    ok = ape_vm_setsymbol(st, symbol);
    if(!ok)
    {
        ape_symbol_destroy(symbol);
        return NULL;
    }

    return symbol;
}

const ApeSymbol_t* ape_symtable_definefuncname(ApeSymTable_t* st, const char* name, bool assignable)
{
    bool ok;
    ApeSymbol_t* symbol;
    if(strchr(name, ':'))
    {
        return NULL;// module symbol
    }
    symbol = ape_make_symbol(st->context, name, APE_SYMBOL_FUNCTION, 0, assignable);
    if(!symbol)
    {
        return NULL;
    }
    ok = ape_vm_setsymbol(st, symbol);
    if(!ok)
    {
        ape_symbol_destroy(symbol);
        return NULL;
    }

    return symbol;
}

const ApeSymbol_t* ape_symtable_definethis(ApeSymTable_t* st)
{
    bool ok;
    ApeSymbol_t* symbol;
    symbol = ape_make_symbol(st->context, "this", APE_SYMBOL_THIS, 0, false);
    if(!symbol)
    {
        return NULL;
    }
    ok = ape_vm_setsymbol(st, symbol);
    if(!ok)
    {
        ape_symbol_destroy(symbol);
        return NULL;
    }
    return symbol;
}

const ApeSymbol_t* ape_symtable_resolve(ApeSymTable_t* table, const char* name)
{
    ApeInt_t i;
    const ApeSymbol_t* symbol;
    ApeBlockScope_t* scope;
    scope = NULL;
    symbol = ape_globalstore_getsymbol(table->globalstore, name);
    if(symbol)
    {
        return symbol;
    }
    for(i = (ApeInt_t)ape_ptrarray_count(table->blockscopes) - 1; i >= 0; i--)
    {
        scope = (ApeBlockScope_t*)ape_ptrarray_get(table->blockscopes, i);
        symbol = (ApeSymbol_t*)ape_strdict_get(scope->store, name);
        if(symbol)
        {
            break;
        }
    }
    if(symbol && symbol->type == APE_SYMBOL_THIS)
    {
        symbol = ape_symtable_deffree(table, symbol);
    }

    if(!symbol && table->outer)
    {
        symbol = ape_symtable_resolve(table->outer, name);
        if(!symbol)
        {
            return NULL;
        }
        if(symbol->type == APE_SYMBOL_MODULEGLOBAL || symbol->type == APE_SYMBOL_CONTEXTGLOBAL)
        {
            return symbol;
        }
        symbol = ape_symtable_deffree(table, symbol);
    }
    return symbol;
}

bool ape_symtable_symbol_is_defined(ApeSymTable_t* table, const char* name)
{
    ApeBlockScope_t* top_scope;
    const ApeSymbol_t* symbol;
    // todo: rename to something more obvious
    symbol = ape_globalstore_getsymbol(table->globalstore, name);
    if(symbol)
    {
        return true;
    }
    top_scope = (ApeBlockScope_t*)ape_ptrarray_top(table->blockscopes);
    symbol = (ApeSymbol_t*)ape_strdict_get(top_scope->store, name);
    if(symbol)
    {
        return true;
    }
    return false;
}

bool ape_symtable_pushblockscope(ApeSymTable_t* table)
{
    bool ok;
    int block_scope_offset;
    ApeBlockScope_t* prev_block_scope;
    ApeBlockScope_t* new_scope;
    block_scope_offset = 0;
    prev_block_scope = (ApeBlockScope_t*)ape_ptrarray_top(table->blockscopes);
    if(prev_block_scope)
    {
        block_scope_offset = table->module_global_offset + prev_block_scope->offset + prev_block_scope->numdefinitions;
    }
    else
    {
        block_scope_offset = table->module_global_offset;
    }

    new_scope = ape_make_blockscope(table->context, block_scope_offset);
    if(!new_scope)
    {
        return false;
    }
    ok = ape_ptrarray_push(table->blockscopes, new_scope);
    if(!ok)
    {
        ape_blockscope_destroy(new_scope);
        return false;
    }
    return true;
}

void ape_symtable_popblockscope(ApeSymTable_t* table)
{
    ApeBlockScope_t* top_scope;
    top_scope = (ApeBlockScope_t*)ape_ptrarray_top(table->blockscopes);
    ape_ptrarray_pop(table->blockscopes);
    ape_blockscope_destroy(top_scope);
}

ApeBlockScope_t* ape_symtable_getblockscope(ApeSymTable_t* table)
{
    ApeBlockScope_t* top_scope;
    top_scope = (ApeBlockScope_t*)ape_ptrarray_top(table->blockscopes);
    return top_scope;
}

bool ape_symtable_ismoduleglobalscope(ApeSymTable_t* table)
{
    return table->outer == NULL;
}

bool ape_symtable_istopblockscope(ApeSymTable_t* table)
{
    return ape_ptrarray_count(table->blockscopes) == 1;
}

bool ape_symtable_istopglobalscope(ApeSymTable_t* table)
{
    return ape_symtable_ismoduleglobalscope(table) && ape_symtable_istopblockscope(table);
}

ApeSize_t ape_symtable_getmoduleglobalsymbolcount(const ApeSymTable_t* table)
{
    return ape_ptrarray_count(table->module_global_symbols);
}

const ApeSymbol_t* ape_symtable_getmoduleglobalsymbolat(const ApeSymTable_t* table, int ix)
{
    return (ApeSymbol_t*)ape_ptrarray_get(table->module_global_symbols, ix);
}

// INTERNAL
ApeBlockScope_t* ape_make_blockscope(ApeContext_t* ctx, int offset)
{
    ApeBlockScope_t* new_scope;
    new_scope = (ApeBlockScope_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeBlockScope_t));
    if(!new_scope)
    {
        return NULL;
    }
    memset(new_scope, 0, sizeof(ApeBlockScope_t));
    new_scope->context = ctx;
    new_scope->alloc = &ctx->alloc;
    new_scope->store = ape_make_strdict(ctx, (ApeDataCallback_t)ape_symbol_copy, (ApeDataCallback_t)ape_symbol_destroy);
    if(!new_scope->store)
    {
        ape_blockscope_destroy(new_scope);
        return NULL;
    }
    new_scope->numdefinitions = 0;
    new_scope->offset = offset;
    return new_scope;
}

void* ape_blockscope_destroy(ApeBlockScope_t* scope)
{
    ape_strdict_destroywithitems(scope->store);
    ape_allocator_free(scope->alloc, scope);
    return NULL;
}

ApeBlockScope_t* ape_blockscope_copy(ApeBlockScope_t* scope)
{
    ApeBlockScope_t* copy;
    copy = (ApeBlockScope_t*)ape_allocator_alloc(scope->alloc, sizeof(ApeBlockScope_t));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(ApeBlockScope_t));
    copy->alloc = scope->alloc;
    copy->numdefinitions = scope->numdefinitions;
    copy->offset = scope->offset;
    copy->store = ape_strdict_copywithitems(scope->store);
    if(!copy->store)
    {
        ape_blockscope_destroy(copy);
        return NULL;
    }
    return copy;
}

static bool ape_vm_setsymbol(ApeSymTable_t* table, ApeSymbol_t* symbol)
{
    ApeBlockScope_t* top_scope;
    ApeSymbol_t* existing;
    top_scope = (ApeBlockScope_t*)ape_ptrarray_top(table->blockscopes);
    existing= (ApeSymbol_t*)ape_strdict_get(top_scope->store, symbol->name);
    if(existing)
    {
        ape_symbol_destroy(existing);
    }
    return ape_strdict_set(top_scope->store, symbol->name, symbol);
}

static int ape_vm_nextsymbolindex(ApeSymTable_t* table)
{
    int ix;
    ApeBlockScope_t* top_scope;
    top_scope = (ApeBlockScope_t*)ape_ptrarray_top(table->blockscopes);
    ix = top_scope->offset + top_scope->numdefinitions;
    return ix;
}

static int ape_vm_countnumdefs(ApeSymTable_t* table)
{
    ApeInt_t i;
    int count;
    ApeBlockScope_t* scope;
    count = 0;
    for(i = (ApeInt_t)ape_ptrarray_count(table->blockscopes) - 1; i >= 0; i--)
    {
        scope = (ApeBlockScope_t*)ape_ptrarray_get(table->blockscopes, i);
        count += scope->numdefinitions;
    }
    return count;
}



#define APPEND_BYTE(n)                           \
    do                                           \
    {                                            \
        val = (ApeUShort_t)(operands[i] >> (n * 8)); \
        ok = ape_valarray_add(res, &val);               \
        if(!ok)                                  \
        {                                        \
            return 0;                            \
        }                                        \
    } while(0)

int ape_make_code(ApeOpByte_t op, ApeSize_t operands_count, uint64_t* operands, ApeValArray_t* res)
{
    ApeSize_t i;
    int width;
    int instr_len;
    bool ok;
    ApeUShort_t val;
    ApeOpcodeDef_t* def;
    def = ape_tostring_opcodefind(op);
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
    ok = ape_valarray_add(res, &val);
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

ApeCompScope_t* ape_make_compscope(ApeContext_t* ctx, ApeCompScope_t* outer)
{
    ApeCompScope_t* scope;
    scope = (ApeCompScope_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeCompScope_t));
    if(!scope)
    {
        return NULL;
    }
    memset(scope, 0, sizeof(ApeCompScope_t));
    scope->context = ctx;
    scope->alloc = &ctx->alloc;
    scope->outer = outer;
    scope->bytecode = array_make(ctx, ApeUShort_t);
    if(!scope->bytecode)
    {
        goto err;
    }
    scope->srcpositions = array_make(ctx, ApePosition_t);
    if(!scope->srcpositions)
    {
        goto err;
    }
    scope->breakipstack = array_make(ctx, int);
    if(!scope->breakipstack)
    {
        goto err;
    }
    scope->continueipstack = array_make(ctx, int);
    if(!scope->continueipstack)
    {
        goto err;
    }
    return scope;
err:
    ape_compscope_destroy(scope);
    return NULL;
}

void ape_compscope_destroy(ApeCompScope_t* scope)
{
    ape_valarray_destroy(scope->continueipstack);
    ape_valarray_destroy(scope->breakipstack);
    ape_valarray_destroy(scope->bytecode);
    ape_valarray_destroy(scope->srcpositions);
    ape_allocator_free(scope->alloc, scope);
}

ApeCompResult_t* ape_compscope_orphanresult(ApeCompScope_t* scope)
{
    ApeCompResult_t* res;
    res = ape_make_compresult(scope->context,
        (ApeUShort_t*)ape_valarray_data(scope->bytecode),
        (ApePosition_t*)ape_valarray_data(scope->srcpositions),
        ape_valarray_count(scope->bytecode)
    );
    if(!res)
    {
        return NULL;
    }
    ape_valarray_orphandata(scope->bytecode);
    ape_valarray_orphandata(scope->srcpositions);
    return res;
}

ApeCompResult_t* ape_make_compresult(ApeContext_t* ctx, ApeUShort_t* bytecode, ApePosition_t* src_positions, int count)
{
    ApeCompResult_t* res;
    res = (ApeCompResult_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeCompResult_t));
    if(!res)
    {
        return NULL;
    }
    memset(res, 0, sizeof(ApeCompResult_t));
    res->context = ctx;
    res->alloc = &ctx->alloc;
    res->bytecode = bytecode;
    res->srcpositions = src_positions;
    res->count = count;
    return res;
}

void ape_compresult_destroy(ApeCompResult_t* res)
{
    if(!res)
    {
        return;
    }
    ape_allocator_free(res->alloc, res->bytecode);
    ape_allocator_free(res->alloc, res->srcpositions);
    ape_allocator_free(res->alloc, res);
}

ApeExpression_t* ape_optimizer_optexpr(ApeExpression_t* expr)
{

    switch(expr->type)
    {
        case APE_EXPR_INFIX:
            {
                return ape_optimizer_optinfixexpr(expr);
            }
            break;
        case APE_EXPR_PREFIX:
            {
                return ape_optimizer_optprefixexpr(expr);
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
ApeExpression_t* ape_optimizer_optinfixexpr(ApeExpression_t* expr)
{
    return NULL;
    bool left_is_numeric;
    bool right_is_numeric;
    bool left_is_string;
    bool right_is_string;
    ApeInt_t leftint;
    ApeInt_t rightint;
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
    left_optimised = ape_optimizer_optexpr(left);
    if(left_optimised)
    {
        left = left_optimised;
    }
    right = expr->infix.right;
    right_optimised = ape_optimizer_optexpr(right);
    if(right_optimised)
    {
        right = right_optimised;
    }
    res = NULL;
    left_is_numeric = left->type == APE_EXPR_LITERALNUMBER || left->type == APE_EXPR_LITERALBOOL;
    right_is_numeric = right->type == APE_EXPR_LITERALNUMBER || right->type == APE_EXPR_LITERALBOOL;
    left_is_string = left->type == APE_EXPR_LITERALSTRING;
    right_is_string = right->type == APE_EXPR_LITERALSTRING;
    alloc = expr->alloc;
    if(left_is_numeric && right_is_numeric)
    {
        leftval = left->type == APE_EXPR_LITERALNUMBER ? left->numberliteral : left->boolliteral;
        rightval = right->type == APE_EXPR_LITERALNUMBER ? right->numberliteral : right->boolliteral;
        leftint = (ApeInt_t)leftval;
        rightint = (ApeInt_t)rightval;
        switch(expr->infix.op)
        {
            case APE_OPERATOR_PLUS:
                {
                    res = ape_ast_make_numberliteralexpr(expr->context, leftval + rightval);
                }
                break;
            case APE_OPERATOR_MINUS:
                {
                    res = ape_ast_make_numberliteralexpr(expr->context, leftval - rightval);
                }
                break;
            case APE_OPERATOR_STAR:
                {
                    res = ape_ast_make_numberliteralexpr(expr->context, leftval * rightval);
                }
                break;
            case APE_OPERATOR_SLASH:
                {
                    res = ape_ast_make_numberliteralexpr(expr->context, leftval / rightval);
                }
                break;
            case APE_OPERATOR_LESSTHAN:
                {
                    res = ape_ast_make_boolliteralexpr(expr->context, leftval < rightval);
                }
                break;
            case APE_OPERATOR_LESSEQUAL:
                {
                    res = ape_ast_make_boolliteralexpr(expr->context, leftval <= rightval);
                }
                break;
            case APE_OPERATOR_GREATERTHAN:
                {
                    res = ape_ast_make_boolliteralexpr(expr->context, leftval > rightval);
                }
                break;
            case APE_OPERATOR_GREATEREQUAL:
                {
                    res = ape_ast_make_boolliteralexpr(expr->context, leftval >= rightval);
                }
                break;
            case APE_OPERATOR_EQUAL:
                {
                    res = ape_ast_make_boolliteralexpr(expr->context, APE_DBLEQ(leftval, rightval));
                }
                break;

            case APE_OPERATOR_NOTEQUAL:
                {
                    res = ape_ast_make_boolliteralexpr(expr->context, !APE_DBLEQ(leftval, rightval));
                }
                break;
            case APE_OPERATOR_MODULUS:
                {
                    //res = ape_ast_make_numberliteralexpr(expr->context, fmod(leftval, rightval));
                    res = ape_ast_make_numberliteralexpr(expr->context, (leftint % rightint));
                }
                break;
            case APE_OPERATOR_BITAND:
                {
                    res = ape_ast_make_numberliteralexpr(expr->context, (ApeFloat_t)(leftint & rightint));
                }
                break;
            case APE_OPERATOR_BITOR:
                {
                    res = ape_ast_make_numberliteralexpr(expr->context, (ApeFloat_t)(leftint | rightint));
                }
                break;
            case APE_OPERATOR_BITXOR:
                {
                    res = ape_ast_make_numberliteralexpr(expr->context, (ApeFloat_t)(leftint ^ rightint));
                }
                break;
            case APE_OPERATOR_LEFTSHIFT:
                {
                    res = ape_ast_make_numberliteralexpr(expr->context, (ApeFloat_t)(leftint << rightint));
                }
                break;
            case APE_OPERATOR_RIGHTSHIFT:
                {
                    res = ape_ast_make_numberliteralexpr(expr->context, (ApeFloat_t)(leftint >> rightint));
                }
                break;
            default:
                {
                }
                break;
        }
    }
    else if(expr->infix.op == APE_OPERATOR_PLUS && left_is_string && right_is_string)
    {
        leftstr = left->stringliteral;
        rightstr = right->stringliteral;
        res_str = ape_util_stringfmt(expr->context, "%s%s", leftstr, rightstr);
        if(res_str)
        {
            res = ape_ast_make_stringliteralexpr(expr->context, res_str);
            if(!res)
            {
                ape_allocator_free(alloc, res_str);
            }
        }
    }
    ape_ast_destroy_expr(left_optimised);
    ape_ast_destroy_expr(right_optimised);
    if(res)
    {
        res->pos = expr->pos;
    }

    return res;
}

ApeExpression_t* ape_optimizer_optprefixexpr(ApeExpression_t* expr)
{
    ApeExpression_t* right;
    ApeExpression_t* right_optimised;
    ApeExpression_t* res;
    right = expr->prefix.right;
    right_optimised = ape_optimizer_optexpr(right);
    if(right_optimised)
    {
        right = right_optimised;
    }
    res = NULL;
    if(expr->prefix.op == APE_OPERATOR_MINUS && right->type == APE_EXPR_LITERALNUMBER)
    {
        res = ape_ast_make_numberliteralexpr(expr->context, -right->numberliteral);
    }
    else if(expr->prefix.op == APE_OPERATOR_NOT && right->type == APE_EXPR_LITERALBOOL)
    {
        res = ape_ast_make_boolliteralexpr(expr->context, !right->boolliteral);
    }
    ape_ast_destroy_expr(right_optimised);
    if(res)
    {
        res->pos = expr->pos;
    }
    return res;
}

ApeModule_t* ape_make_module(ApeContext_t* ctx, const char* name)
{
    ApeModule_t* module;
    module = (ApeModule_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeModule_t));
    if(!module)
    {
        return NULL;
    }
    memset(module, 0, sizeof(ApeModule_t));
    module->context = ctx;
    module->alloc = &ctx->alloc;
    module->name = ape_util_strdup(ctx, name);
    if(!module->name)
    {
        ape_module_destroy(module);
        return NULL;
    }
    module->symbols = ape_make_ptrarray(ctx);
    if(!module->symbols)
    {
        ape_module_destroy(module);
        return NULL;
    }
    return module;
}

void* ape_module_destroy(ApeModule_t* module)
{
    if(!module)
    {
        return NULL;
    }
    ape_allocator_free(module->alloc, module->name);
    ape_ptrarray_destroywithitems(module->symbols, (ApeDataCallback_t)ape_symbol_destroy);
    ape_allocator_free(module->alloc, module);
    return NULL;
}

ApeModule_t* ape_module_copy(ApeModule_t* src)
{
    ApeModule_t* copy;
    copy = (ApeModule_t*)ape_allocator_alloc(src->alloc, sizeof(ApeModule_t));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(ApeModule_t));
    copy->alloc = src->alloc;
    copy->name = ape_util_strdup(src->context, src->name);
    if(!copy->name)
    {
        ape_module_destroy(copy);
        return NULL;
    }
    copy->symbols = ape_ptrarray_copywithitems(src->symbols, (ApeDataCallback_t)ape_symbol_copy, (ApeDataCallback_t)ape_symbol_destroy);
    if(!copy->symbols)
    {
        ape_module_destroy(copy);
        return NULL;
    }
    return copy;
}

const char* ape_module_getname(const char* path)
{
    const char* last_slash_pos;
    last_slash_pos = strrchr(path, '/');
    if(last_slash_pos)
    {
        return last_slash_pos + 1;
    }
    return path;
}

bool ape_module_addsymbol(ApeModule_t* module, const ApeSymbol_t* symbol)
{
    bool ok;
    ApeWriter_t* name_buf;
    ApeSymbol_t* module_symbol;
    name_buf = ape_make_writer(module->context);
    if(!name_buf)
    {
        return false;
    }
    ok = ape_writer_appendf(name_buf, "%s::%s", module->name, symbol->name);
    if(!ok)
    {
        ape_writer_destroy(name_buf);
        return false;
    }
    module_symbol = ape_make_symbol(module->context, ape_writer_getdata(name_buf), APE_SYMBOL_MODULEGLOBAL, symbol->index, false);
    ape_writer_destroy(name_buf);
    if(!module_symbol)
    {
        return false;
    }
    ok = ape_ptrarray_add(module->symbols, module_symbol);
    if(!ok)
    {
        ape_symbol_destroy(module_symbol);
        return false;
    }
    return true;
}

bool ape_frame_init(ApeFrame_t* frame, ApeObject_t function_obj, int base_pointer)
{
    ApeScriptFunction_t* function;
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
    frame->srcpositions = function->comp_result->srcpositions;
    frame->bytecode_size = function->comp_result->count;
    frame->recover_ip = -1;
    frame->is_recovering = false;
    return true;
}

ApeOpcodeValue_t ape_frame_readopcode(ApeFrame_t* frame)
{
    frame->src_ip = frame->ip;
    return (ApeOpcodeValue_t)ape_frame_readuint8(frame);
}

uint64_t ape_frame_readuint64(ApeFrame_t* frame)
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

uint16_t ape_frame_readuint16(ApeFrame_t* frame)
{
    const ApeUShort_t* data;
    data = frame->bytecode + frame->ip;
    frame->ip += 2;
    return (data[0] << 8) | data[1];
}

ApeUShort_t ape_frame_readuint8(ApeFrame_t* frame)
{
    const ApeUShort_t* data;
    data = frame->bytecode + frame->ip;
    frame->ip++;
    return data[0];
}

ApePosition_t ape_frame_srcposition(const ApeFrame_t* frame)
{
    if(frame->srcpositions)
    {
        return frame->srcpositions[frame->src_ip];
    }
    return g_vmpriv_src_pos_invalid;
}

ApeObject_t ape_vm_getlastpopped(ApeVM_t* vm)
{
    return vm->last_popped;
}

bool ape_vm_haserrors(ApeVM_t* vm)
{
    return ape_errorlist_count(vm->errors) > 0;
}

bool ape_vm_setglobal(ApeVM_t* vm, ApeSize_t ix, ApeObject_t val)
{
    if(ix >= APE_CONF_SIZE_VM_MAXGLOBALS)
    {
        APE_ASSERT(false);
        ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame), "global write out of range");
        return false;
    }
    vm->globals[ix] = val;
    if(ix >= vm->globals_count)
    {
        vm->globals_count = ix + 1;
    }
    return true;
}

ApeObject_t ape_vm_getglobal(ApeVM_t* vm, ApeSize_t ix)
{
    if(ix >= APE_CONF_SIZE_VM_MAXGLOBALS)
    {
        APE_ASSERT(false);
        ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame), "global read out of range");
        return ape_object_make_null(vm->context);
    }
    return vm->globals[ix];
}

// INTERNAL
static void ape_vm_setstackpointer(ApeVM_t* vm, int new_sp)
{
    if(new_sp > vm->sp)
    {// to avoid gcing freed objects
        int count = new_sp - vm->sp;
        size_t bytes_count = count * sizeof(ApeObject_t);
        memset(vm->stack + vm->sp, 0, bytes_count);
    }
    vm->sp = new_sp;
}

void ape_vm_pushstack(ApeVM_t* vm, ApeObject_t obj)
{
#ifdef APE_DEBUG
    if(vm->sp >= APE_CONF_SIZE_VM_STACK)
    {
        APE_ASSERT(false);
        ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame), "stack overflow");
        return;
    }
    if(vm->current_frame)
    {
        ApeFrame_t* frame = vm->current_frame;
        ApeScriptFunction_t* current_function = object_value_asfunction(frame->function);
        int num_locals = current_function->num_locals;
        APE_ASSERT(vm->sp >= (frame->base_pointer + num_locals));
    }
#endif
    vm->stack[vm->sp] = obj;
    vm->sp++;
}

ApeObject_t ape_vm_popstack(ApeVM_t* vm)
{
#ifdef APE_DEBUG
    if(vm->sp == 0)
    {
        ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame), "stack underflow");
        APE_ASSERT(false);
        return ape_object_make_null(vm->context);
    }
    if(vm->current_frame)
    {
        ApeFrame_t* frame = vm->current_frame;
        ApeScriptFunction_t* current_function = object_value_asfunction(frame->function);
        int num_locals = current_function->num_locals;
        APE_ASSERT((vm->sp - 1) >= (frame->base_pointer + num_locals));
    }
#endif
    vm->sp--;
    ApeObject_t objres = vm->stack[vm->sp];
    vm->last_popped = objres;
    return objres;
}

ApeObject_t ape_vm_getstack(ApeVM_t* vm, int nth_item)
{
    int ix = vm->sp - 1 - nth_item;
#ifdef APE_DEBUG
    if(ix < 0 || ix >= APE_CONF_SIZE_VM_STACK)
    {
        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame), "invalid stack index: %d", nth_item);
        APE_ASSERT(false);
        return ape_object_make_null(vm->context);
    }
#endif
    return vm->stack[ix];
}

static void ape_vm_pushthisstack(ApeVM_t* vm, ApeObject_t obj)
{
#ifdef APE_DEBUG
    if(vm->this_sp >= APE_CONF_SIZE_VM_THISSTACK)
    {
        APE_ASSERT(false);
        ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame), "this stack overflow");
        return;
    }
#endif
    vm->this_stack[vm->this_sp] = obj;
    vm->this_sp++;
}

static ApeObject_t ape_vm_popthisstack(ApeVM_t* vm)
{
#ifdef APE_DEBUG
    if(vm->this_sp == 0)
    {
        ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame), "this stack underflow");
        APE_ASSERT(false);
        return ape_object_make_null(vm->context);
    }
#endif
    vm->this_sp--;
    return vm->this_stack[vm->this_sp];
}

static ApeObject_t ape_vm_getthisstack(ApeVM_t* vm, int nth_item)
{
    int ix = vm->this_sp - 1 - nth_item;
#ifdef APE_DEBUG
    if(ix < 0 || ix >= APE_CONF_SIZE_VM_THISSTACK)
    {
        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame), "invalid this stack index: %d", nth_item);
        APE_ASSERT(false);
        return ape_object_make_null(vm->context);
    }
#endif
    return vm->this_stack[ix];
}

bool ape_vm_pushframe(ApeVM_t* vm, ApeFrame_t frame)
{
    if(vm->frames_count >= APE_CONF_SIZE_MAXFRAMES)
    {
        APE_ASSERT(false);
        return false;
    }
    vm->frames[vm->frames_count] = frame;
    vm->current_frame = &vm->frames[vm->frames_count];
    vm->frames_count++;
    ApeScriptFunction_t* frame_function = object_value_asfunction(frame.function);
    ape_vm_setstackpointer(vm, frame.base_pointer + frame_function->num_locals);
    return true;
}

bool ape_vm_popframe(ApeVM_t* vm)
{
    ape_vm_setstackpointer(vm, vm->current_frame->base_pointer - 1);
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

void ape_vm_collectgarbage(ApeVM_t* vm, ApeValArray_t * constants)
{
    ApeSize_t i;
    ApeFrame_t* frame;
    ape_gcmem_unmarkall(vm->mem);
    ape_gcmem_markobjlist(ape_globalstore_getobjectdata(vm->globalstore), ape_globalstore_getobjectcount(vm->globalstore));
    ape_gcmem_markobjlist((ApeObject_t*)ape_valarray_data(constants), ape_valarray_count(constants));
    ape_gcmem_markobjlist(vm->globals, vm->globals_count);
    for(i = 0; i < vm->frames_count; i++)
    {
        frame = &vm->frames[i];
        ape_gcmem_markobject(frame->function);
    }
    ape_gcmem_markobjlist(vm->stack, vm->sp);
    ape_gcmem_markobjlist(vm->this_stack, vm->this_sp);
    ape_gcmem_markobject(vm->last_popped);
    ape_gcmem_markobjlist(vm->operator_oveload_keys, APE_OPCODE_MAX);
    ape_gcmem_sweep(vm->mem);
}

bool ape_vm_callobject(ApeVM_t* vm, ApeObject_t callee, ApeInt_t num_args)
{
    bool ok;
    ApeObjType_t callee_type;
    ApeScriptFunction_t* callee_function;
    ApeFrame_t callee_frame;
    ApeObject_t* stack_pos;
    ApeObject_t objres;
    const char* callee_type_name;
    callee_type = object_value_type(callee);
    if(callee_type == APE_OBJECT_FUNCTION)
    {
        callee_function = object_value_asfunction(callee);
        if(num_args != (ApeInt_t)callee_function->num_args)
        {
            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame),
                              "invalid number of arguments to \"%s\", expected %d, got %d",
                              ape_object_function_getname(callee), callee_function->num_args, num_args);
            return false;
        }
        ok = ape_frame_init(&callee_frame, callee, vm->sp - num_args);
        if(!ok)
        {
            ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, g_vmpriv_src_pos_invalid, "frame init failed in ape_vm_callobject");
            return false;
        }
        ok = ape_vm_pushframe(vm, callee_frame);
        if(!ok)
        {
            ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, g_vmpriv_src_pos_invalid, "pushing frame failed in ape_vm_callobject");
            return false;
        }
    }
    else if(callee_type == APE_OBJECT_NATIVE_FUNCTION)
    {
        stack_pos = vm->stack + vm->sp - num_args;
        objres = ape_vm_callnativefunction(vm, callee, ape_frame_srcposition(vm->current_frame), num_args, stack_pos);
        if(ape_vm_haserrors(vm))
        {
            return false;
        }
        ape_vm_setstackpointer(vm, vm->sp - num_args - 1);
        ape_vm_pushstack(vm, objres);
    }
    else
    {
        callee_type_name = ape_object_value_typename(callee_type);
        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame), "%s object is not callable", callee_type_name);
        return false;
    }
    return true;
}

ApeObject_t ape_vm_callnativefunction(ApeVM_t* vm, ApeObject_t callee, ApePosition_t src_pos, int argc, ApeObject_t* args)
{
    ApeNativeFunction_t* native_fun = object_value_asnativefunction(callee);
    ApeObject_t objres = native_fun->native_funcptr(vm, native_fun->data, argc, args);
    if(ape_errorlist_haserrors(vm->errors) && !APE_STREQ(native_fun->name, "crash"))
    {
        ApeError_t* err = ape_errorlist_lasterror(vm->errors);
        err->pos = src_pos;
        err->traceback = ape_make_traceback(vm->context);
        if(err->traceback)
        {
            ape_traceback_append(err->traceback, native_fun->name, g_vmpriv_src_pos_invalid);
        }
        return ape_object_make_null(vm->context);
    }
    ApeObjType_t res_type = object_value_type(objres);
    if(res_type == APE_OBJECT_ERROR)
    {
        ApeTraceback_t* traceback = ape_make_traceback(vm->context);
        if(traceback)
        {
            // error builtin is treated in a special way
            if(!APE_STREQ(native_fun->name, "error"))
            {
                ape_traceback_append(traceback, native_fun->name, g_vmpriv_src_pos_invalid);
            }
            ape_traceback_appendfromvm(traceback, vm);
            ape_object_value_seterrortraceback(objres, traceback);
        }
    }
    return objres;
}

bool ape_vm_checkassign(ApeVM_t* vm, ApeObject_t oldval, ApeObject_t newval)
{
    ApeObjType_t oldtype;
    ApeObjType_t newtype;
    (void)vm;
    oldtype = object_value_type(oldval);
    newtype = object_value_type(newval);
    if(oldtype == APE_OBJECT_NULL || newtype == APE_OBJECT_NULL)
    {
        return true;
    }
    if(oldtype != newtype)
    {
        /*
        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame), "trying to assign variable of type %s to %s",
                          ape_object_value_typename(newtype), ape_object_value_typename(oldtype));
        return false;
        */
    }
    return true;
}

static bool ape_vm_tryoverloadoperator(ApeVM_t* vm, ApeObject_t left, ApeObject_t right, ApeOpByte_t op, bool* out_overload_found)
{
    int num_operands;
    ApeObject_t key;
    ApeObject_t callee;
    ApeObjType_t lefttype;
    ApeObjType_t righttype;
    *out_overload_found = false;
    lefttype = object_value_type(left);
    righttype = object_value_type(right);
    if(lefttype != APE_OBJECT_MAP && righttype != APE_OBJECT_MAP)
    {
        *out_overload_found = false;
        return true;
    }
    num_operands = 2;
    if(op == APE_OPCODE_MINUS || op == APE_OPCODE_NOT)
    {
        num_operands = 1;
    }
    key = vm->operator_oveload_keys[op];
    callee = ape_object_make_null(vm->context);
    if(lefttype == APE_OBJECT_MAP)
    {
        callee = ape_object_map_getvalueobject(left, key);
    }
    if(!object_value_iscallable(callee))
    {
        if(righttype == APE_OBJECT_MAP)
        {
            callee = ape_object_map_getvalueobject(right, key);
        }
        if(!object_value_iscallable(callee))
        {
            *out_overload_found = false;
            return true;
        }
    }
    *out_overload_found = true;
    ape_vm_pushstack(vm, callee);
    ape_vm_pushstack(vm, left);
    if(num_operands == 2)
    {
        ape_vm_pushstack(vm, right);
    }
    return ape_vm_callobject(vm, callee, num_operands);
}


#define SET_OPERATOR_OVERLOAD_KEY(op, key)                   \
    do                                                       \
    {                                                        \
        key_obj = ape_object_make_string(ctx, key); \
        if(object_value_isnull(key_obj))                          \
        {                                                    \
            goto err;                                        \
        }                                                    \
        vm->operator_oveload_keys[op] = key_obj;             \
    } while(0)

ApeVM_t* ape_vm_make(ApeContext_t* ctx, const ApeConfig_t* config, ApeGCMemory_t* mem, ApeErrorList_t* errors, ApeGlobalStore_t* global_store)
{
    ApeSize_t i;
    ApeVM_t* vm;
    ApeObject_t key_obj;
    vm = (ApeVM_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeVM_t));
    if(!vm)
    {
        return NULL;
    }
    memset(vm, 0, sizeof(ApeVM_t));
    vm->context = ctx;
    vm->alloc = &ctx->alloc;
    vm->config = config;
    vm->mem = mem;
    vm->errors = errors;
    vm->globalstore = global_store;
    vm->globals_count = 0;
    vm->sp = 0;
    vm->this_sp = 0;
    vm->frames_count = 0;
    vm->last_popped = ape_object_make_null(ctx);
    vm->running = false;
    for(i = 0; i < APE_OPCODE_MAX; i++)
    {
        vm->operator_oveload_keys[i] = ape_object_make_null(ctx);
    }
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_ADD, "__operator_add__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_SUB, "__operator_sub__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_MUL, "__operator_mul__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_DIV, "__operator_div__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_MOD, "__operator_mod__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_BITOR, "__operator_or__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_BITXOR, "__operator_xor__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_BITAND, "__operator_and__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_LEFTSHIFT, "__operator_lshift__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_RIGHTSHIFT, "__operator_rshift__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_MINUS, "__operator_minus__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_NOT, "__operator_bang__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_COMPAREPLAIN, "__cmp__");
    return vm;
err:
    ape_vm_destroy(vm);
    return NULL;
}
#undef SET_OPERATOR_OVERLOAD_KEY

void ape_vm_destroy(ApeVM_t* vm)
{
    if(!vm)
    {
        return;
    }
    ape_allocator_free(vm->alloc, vm);
}

void ape_vm_reset(ApeVM_t* vm)
{
    vm->sp = 0;
    vm->this_sp = 0;
    while(vm->frames_count > 0)
    {
        ape_vm_popframe(vm);
    }
}

bool ape_vm_run(ApeVM_t* vm, ApeCompResult_t* comp_res, ApeValArray_t * constants)
{
    bool res;
    int old_sp;
    int old_this_sp;
    ApeSize_t old_frames_count;
    ApeObject_t main_fn;
    (void)old_sp;
    old_sp = vm->sp;
    old_this_sp = vm->this_sp;
    old_frames_count = vm->frames_count;
    main_fn = ape_object_make_function(vm->context, "main", comp_res, false, 0, 0, 0);
    if(object_value_isnull(main_fn))
    {
        return false;
    }
    ape_vm_pushstack(vm, main_fn);
    res = ape_vm_executefunction(vm, main_fn, constants);
    while(vm->frames_count > old_frames_count)
    {
        ape_vm_popframe(vm);
    }
    APE_ASSERT(vm->sp == old_sp);
    vm->this_sp = old_this_sp;
    return res;
}


bool ape_vm_appendstring(ApeVM_t* vm, ApeObject_t left, ApeObject_t right, ApeObjType_t lefttype, ApeObjType_t righttype)
{
    bool ok;
    const char* leftstr;
    const char* rightstr;
    ApeObject_t objres;
    ApeInt_t buflen;
    ApeInt_t leftlen;
    ApeInt_t rightlen;
    ApeWriter_t* allbuf;
    ApeWriter_t* tostrbuf;
    (void)lefttype;
    if(righttype == APE_OBJECT_STRING)
    {
        leftlen = (int)ape_object_string_getlength(left);
        rightlen = (int)ape_object_string_getlength(right);
        // avoid doing unnecessary copying by reusing the origin as-is
        if(leftlen == 0)
        {
            ape_vm_pushstack(vm, right);
        }
        else if(rightlen == 0)
        {
            ape_vm_pushstack(vm, left);
        }
        else
        {
            leftstr = ape_object_string_getdata(left);
            rightstr = ape_object_string_getdata(right);
            objres = ape_object_make_stringcapacity(vm->context, leftlen + rightlen);
            if(object_value_isnull(objres))
            {
                return false;
            }
            ok = ape_object_string_append(objres, leftstr, leftlen);
            if(!ok)
            {
                return false;
            }
            ok = ape_object_string_append(objres, rightstr, rightlen);
            if(!ok)
            {
                return false;
            }
            ape_vm_pushstack(vm, objres);
        }
    }
    else
    {
        /*
        * when 'right' is not a string, stringify it, and create a new string.
        * in short, this does 'left = left + tostring(right)'
        * TODO: probably really inefficient.
        */
        allbuf = ape_make_writer(vm->context);
        tostrbuf = ape_make_writer(vm->context);
        ape_writer_appendn(allbuf, ape_object_string_getdata(left), ape_object_string_getlength(left));
        ape_tostring_object(right, tostrbuf, false);
        ape_writer_appendn(allbuf, ape_writer_getdata(tostrbuf), ape_writer_getlength(tostrbuf));
        buflen = ape_writer_getlength(allbuf);
        objres = ape_object_make_stringcapacity(vm->context, buflen);
        ok = ape_object_string_append(objres, ape_writer_getdata(allbuf), buflen);
        ape_writer_destroy(tostrbuf);
        ape_writer_destroy(allbuf);
        if(!ok)
        {
            return false;
        }
        ape_vm_pushstack(vm, objres);
    }
    return true;
}


/*
* TODO: there is A LOT of code in this function.
* some could be easily split into other functions.
*/
bool ape_vm_executefunction(ApeVM_t* vm, ApeObject_t function, ApeValArray_t * constants)
{
    bool checktime;
    bool isoverloaded;
    bool ok;
    bool overloadfound;
    bool resval;
    bool iseq;
    const ApeScriptFunction_t* constfunction;
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
    ApeInt_t ui;
    ApeInt_t leftint;
    ApeInt_t rightint;
    ApeUInt_t constant_ix;
    ApeUInt_t count;
    ApeUInt_t items_count;
    ApeUInt_t kvp_count;
    ApeUInt_t recover_ip;
    ApeInt_t val;
    ApeInt_t pos;
    unsigned time_check_counter;
    unsigned time_check_interval;
    ApeInt_t bigres;
    ApeUShort_t free_ix;
    ApeUShort_t num_args;
    ApeUShort_t num_free;
    ApeFloat_t comparison_res;
    ApeFloat_t leftval;
    ApeFloat_t maxexecms;
    ApeFloat_t rightval;
    ApeFloat_t valdouble;
    ApeObjType_t constant_type;
    ApeObjType_t indextype;
    ApeObjType_t keytype;
    ApeObjType_t lefttype;
    ApeObjType_t operand_type;
    ApeObjType_t righttype;
    ApeObjType_t type;
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
    ApeScriptFunction_t* function_function;
    #if 0
    if(vm->running)
    {
        ape_errorlist_add(vm->errors, APE_ERROR_USER, g_vmpriv_src_pos_invalid, "VM is already executing code");
        return false;
    }
    #endif
    function_function = object_value_asfunction(function);// naming is hard
    ok = false;
    ok = ape_frame_init(&new_frame, function, vm->sp - function_function->num_args);
    if(!ok)
    {
        return false;
    }
    ok = ape_vm_pushframe(vm, new_frame);
    if(!ok)
    {
        ape_errorlist_add(vm->errors, APE_ERROR_USER, g_vmpriv_src_pos_invalid, "pushing frame failed");
        return false;
    }
    vm->running = true;
    vm->last_popped = ape_object_make_null(vm->context);
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
        timer = ape_util_timerstart();
    }
    while(vm->current_frame->ip < (ApeInt_t)vm->current_frame->bytecode_size)
    {
        opcode = ape_frame_readopcode(vm->current_frame);
        switch(opcode)
        {
            case APE_OPCODE_CONSTANT:
                {
                    constant_ix = ape_frame_readuint16(vm->current_frame);
                    constant = (ApeObject_t*)ape_valarray_get(constants, constant_ix);
                    if(!constant)
                    {
                        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame),
                                          "constant at %d not found", constant_ix);
                        goto err;
                    }
                    ape_vm_pushstack(vm, *constant);
                }
                break;
            case APE_OPCODE_ADD:
            case APE_OPCODE_SUB:
            case APE_OPCODE_MUL:
            case APE_OPCODE_DIV:
            case APE_OPCODE_MOD:
            case APE_OPCODE_BITOR:
            case APE_OPCODE_BITXOR:
            case APE_OPCODE_BITAND:
            case APE_OPCODE_LEFTSHIFT:
            case APE_OPCODE_RIGHTSHIFT:
                {
                    right = ape_vm_popstack(vm);
                    left = ape_vm_popstack(vm);
                    // NULL to 0 coercion
                    if(object_value_isnumeric(left) && object_value_isnull(right))
                    {
                        right = ape_object_make_number(vm->context, 0);
                    }
                    if(object_value_isnumeric(right) && object_value_isnull(left))
                    {
                        left = ape_object_make_number(vm->context, 0);
                    }
                    lefttype = object_value_type(left);
                    righttype = object_value_type(right);
                    if(object_value_isnumeric(left) && object_value_isnumeric(right))
                    {
                        rightval = object_value_asnumber(right);
                        leftval = object_value_asnumber(left);
                        leftint = (ApeInt_t)leftval;
                        rightint = (ApeInt_t)rightval;
                        bigres = 0;
                        switch(opcode)
                        {
                            case APE_OPCODE_ADD:
                                {
                                    bigres = leftval + rightval;
                                }
                                break;
                            case APE_OPCODE_SUB:
                                {
                                    bigres = leftval - rightval;
                                }
                                break;
                            case APE_OPCODE_MUL:
                                {
                                    bigres = leftval * rightval;
                                }
                                break;
                            case APE_OPCODE_DIV:
                                {
                                    bigres = leftval / rightval;
                                }
                                break;
                            case APE_OPCODE_MOD:
                                {
                                    //bigres = fmod(leftval, rightval);
                                    bigres = (leftint % rightint);
                                }
                                break;
                            case APE_OPCODE_BITOR:
                                {
                                    bigres = (ApeFloat_t)(leftint | rightint);
                                }
                                break;
                            case APE_OPCODE_BITXOR:
                                {
                                    bigres = (ApeFloat_t)(leftint ^ rightint);
                                }
                                break;
                            case APE_OPCODE_BITAND:
                                {
                                    bigres = (ApeFloat_t)(leftint & rightint);
                                }
                                break;
                            case APE_OPCODE_LEFTSHIFT:
                                {
                                    bigres = (ApeInt_t)(leftint << rightint);
                                }
                                break;
                            case APE_OPCODE_RIGHTSHIFT:
                                {
                                    bigres = (ApeInt_t)(leftint >> rightint);
                                }
                                break;
                            default:
                                {
                                    APE_ASSERT(false);
                                }
                                break;
                        }
                        ape_vm_pushstack(vm, ape_object_make_number(vm->context, bigres));
                    }
                    else if(lefttype == APE_OBJECT_STRING /*&& ((righttype == APE_OBJECT_STRING) || (righttype == APE_OBJECT_NUMBER))*/ && opcode == APE_OPCODE_ADD)
                    {
                        ok = ape_vm_appendstring(vm, left, right, lefttype, righttype);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                    else if((lefttype == APE_OBJECT_ARRAY) && opcode == APE_OPCODE_ADD)
                    {
                        ape_object_array_pushvalue(left, right);
                        ape_vm_pushstack(vm, left);
                    }
                    else
                    {
                        overloadfound = false;
                        ok = ape_vm_tryoverloadoperator(vm, left, right, opcode, &overloadfound);
                        if(!ok)
                        {
                            goto err;
                        }
                        if(!overloadfound)
                        {
                            opcode_name = ape_tostring_opcodename(opcode);
                            left_type_name = ape_object_value_typename(lefttype);
                            right_type_name = ape_object_value_typename(righttype);
                            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame),
                                              "invalid operand types for %s, got %s and %s", opcode_name, left_type_name,
                                              right_type_name);
                            goto err;
                        }
                    }
                }
                break;
            case APE_OPCODE_POP:
                {
                    ape_vm_popstack(vm);
                }
                break;
            case APE_OPCODE_TRUE:
                {
                    ape_vm_pushstack(vm, ape_object_make_bool(vm->context, true));
                }
                break;
            case APE_OPCODE_FALSE:
                {
                    ape_vm_pushstack(vm, ape_object_make_bool(vm->context, false));
                }
                break;
            case APE_OPCODE_COMPAREPLAIN:
            case APE_OPCODE_COMPAREEQUAL:
                {
                    right = ape_vm_popstack(vm);
                    left = ape_vm_popstack(vm);
                    isoverloaded = false;
                    ok = ape_vm_tryoverloadoperator(vm, left, right, APE_OPCODE_COMPAREPLAIN, &isoverloaded);
                    if(!ok)
                    {
                        goto err;
                    }
                    if(!isoverloaded)
                    {
                        comparison_res = ape_object_value_compare(left, right, &ok);
                        if(ok || opcode == APE_OPCODE_COMPAREEQUAL)
                        //if(opcode == APE_OPCODE_COMPAREEQUAL)
                        {
                            objres = ape_object_make_number(vm->context, comparison_res);
                            ape_vm_pushstack(vm, objres);
                        }
                        else
                        {
                            right_type_string = ape_object_value_typename(object_value_type(right));
                            left_type_string = ape_object_value_typename(object_value_type(left));
                            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame),
                                              "cannot compare %s and %s", left_type_string, right_type_string);
                            goto err;
                        }
                    }
                }
                break;

            case APE_OPCODE_ISEQUAL:
            case APE_OPCODE_NOTEQUAL:
            case APE_OPCODE_GREATERTHAN:
            case APE_OPCODE_GREATEREQUAL:
                {
                    value = ape_vm_popstack(vm);
                    comparison_res = object_value_asnumber(value);
                    resval = false;
                    switch(opcode)
                    {
                        case APE_OPCODE_ISEQUAL:
                            resval = APE_DBLEQ(comparison_res, 0);
                            break;
                        case APE_OPCODE_NOTEQUAL:
                            resval = !APE_DBLEQ(comparison_res, 0);
                            break;
                        case APE_OPCODE_GREATERTHAN:
                            resval = comparison_res > 0;
                            break;
                        case APE_OPCODE_GREATEREQUAL:
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
                    objres = ape_object_make_bool(vm->context, resval);
                    ape_vm_pushstack(vm, objres);
                }
                break;

            case APE_OPCODE_MINUS:
                {
                    operand = ape_vm_popstack(vm);
                    operand_type = object_value_type(operand);
                    if(operand_type == APE_OBJECT_NUMBER)
                    {
                        val = object_value_asnumber(operand);
                        objres = ape_object_make_number(vm->context, -val);
                        ape_vm_pushstack(vm, objres);
                    }
                    else
                    {
                        overloadfound = false;
                        ok = ape_vm_tryoverloadoperator(vm, operand, ape_object_make_null(vm->context), APE_OPCODE_MINUS, &overloadfound);
                        if(!ok)
                        {
                            goto err;
                        }
                        if(!overloadfound)
                        {
                            operand_type_string = ape_object_value_typename(operand_type);
                            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame),
                                              "invalid operand type for MINUS, got %s", operand_type_string);
                            goto err;
                        }
                    }
                }
                break;

            case APE_OPCODE_NOT:
                {
                    operand = ape_vm_popstack(vm);
                    type = object_value_type(operand);
                    if(type == APE_OBJECT_BOOL)
                    {
                        objres = ape_object_make_bool(vm->context, !object_value_asbool(operand));
                        ape_vm_pushstack(vm, objres);
                    }
                    else if(type == APE_OBJECT_NULL)
                    {
                        objres = ape_object_make_bool(vm->context, true);
                        ape_vm_pushstack(vm, objres);
                    }
                    else
                    {
                        overloadfound = false;
                        ok = ape_vm_tryoverloadoperator(vm, operand, ape_object_make_null(vm->context), APE_OPCODE_NOT, &overloadfound);
                        if(!ok)
                        {
                            goto err;
                        }
                        if(!overloadfound)
                        {
                            ApeObject_t objres = ape_object_make_bool(vm->context, false);
                            ape_vm_pushstack(vm, objres);
                        }
                    }
                }
                break;

            case APE_OPCODE_JUMP:
                {
                    pos = ape_frame_readuint16(vm->current_frame);
                    vm->current_frame->ip = pos;
                }
                break;

            case APE_OPCODE_JUMPIFFALSE:
                {
                    pos = ape_frame_readuint16(vm->current_frame);
                    testobj = ape_vm_popstack(vm);
                    if(!object_value_asbool(testobj))
                    {
                        vm->current_frame->ip = pos;
                    }
                }
                break;

            case APE_OPCODE_JUMPIFTRUE:
                {
                    pos = ape_frame_readuint16(vm->current_frame);
                    testobj = ape_vm_popstack(vm);
                    if(object_value_asbool(testobj))
                    {
                        vm->current_frame->ip = pos;
                    }
                }
                break;

            case APE_OPCODE_NULL:
                {
                    ape_vm_pushstack(vm, ape_object_make_null(vm->context));
                }
                break;

            case APE_OPCODE_DEFMODULEGLOBAL:
            {
                ix = ape_frame_readuint16(vm->current_frame);
                objval = ape_vm_popstack(vm);
                ape_vm_setglobal(vm, ix, objval);
                break;
            }
            case APE_OPCODE_SETMODULEGLOBAL:
                {
                    ix = ape_frame_readuint16(vm->current_frame);
                    new_value = ape_vm_popstack(vm);
                    old_value = ape_vm_getglobal(vm, ix);
                    if(!ape_vm_checkassign(vm, old_value, new_value))
                    {
                        goto err;
                    }
                    ape_vm_setglobal(vm, ix, new_value);
                }
                break;

            case APE_OPCODE_GETMODULEGLOBAL:
                {
                    ix = ape_frame_readuint16(vm->current_frame);
                    global = vm->globals[ix];
                    ape_vm_pushstack(vm, global);
                }
                break;

            case APE_OPCODE_MKARRAY:
                {
                    count = ape_frame_readuint16(vm->current_frame);
                    array_obj = ape_object_make_arraycapacity(vm->context, count);
                    if(object_value_isnull(array_obj))
                    {
                        goto err;
                    }
                    items = vm->stack + vm->sp - count;
                    for(i = 0; i < count; i++)
                    {
                        ApeObject_t item = items[i];
                        ok = ape_object_array_pushvalue(array_obj, item);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                    ape_vm_setstackpointer(vm, vm->sp - count);
                    ape_vm_pushstack(vm, array_obj);
                }
                break;

            case APE_OPCODE_MAPSTART:
                {
                    count = ape_frame_readuint16(vm->current_frame);
                    map_obj = ape_object_make_mapcapacity(vm->context, count);
                    if(object_value_isnull(map_obj))
                    {
                        goto err;
                    }
                    ape_vm_pushthisstack(vm, map_obj);
                }
                break;
            case APE_OPCODE_MAPEND:
                {
                    kvp_count = ape_frame_readuint16(vm->current_frame);
                    items_count = kvp_count * 2;
                    map_obj = ape_vm_popthisstack(vm);
                    kv_pairs = vm->stack + vm->sp - items_count;
                    for(i = 0; i < items_count; i += 2)
                    {
                        key = kv_pairs[i];
                        if(!ape_object_value_ishashable(key))
                        {
                            keytype = object_value_type(key);
                            keytypename = ape_object_value_typename(keytype);
                            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame),
                                              "key of type %s is not hashable", keytypename);
                            goto err;
                        }
                        objval = kv_pairs[i + 1];
                        ok = ape_object_map_setvalue(map_obj, key, objval);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                    ape_vm_setstackpointer(vm, vm->sp - items_count);
                    ape_vm_pushstack(vm, map_obj);
                }
                break;

            case APE_OPCODE_GETTHIS:
                {
                    objval = ape_vm_getthisstack(vm, 0);
                    ape_vm_pushstack(vm, objval);
                }
                break;

            case APE_OPCODE_GETINDEX:
                {
                    #if 0
                    const char* idxname;
                    #endif
                    index = ape_vm_popstack(vm);
                    left = ape_vm_popstack(vm);
                    lefttype = object_value_type(left);
                    indextype = object_value_type(index);
                    left_type_name = ape_object_value_typename(lefttype);
                    indextypename = ape_object_value_typename(indextype);
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
                            idxname = ape_object_string_getdata(index);
                            fprintf(stderr, "index is a string: name=%s\n", idxname);
                            if((afn = builtin_get_object(lefttype, idxname)) != NULL)
                            {
                                fprintf(stderr, "got a callback: afn=%p\n", afn);
                                //typedef ApeObject_t (*ApeNativeFunc_t)(ApeVM_t*, void*, int, ApeObject_t*);
                                args[argc] = left;
                                argc++;
                                ape_vm_pushstack(vm, afn(vm, NULL, argc, args));
                                break;
                            }
                        }
                    }
                    #endif

                    if(lefttype != APE_OBJECT_ARRAY && lefttype != APE_OBJECT_MAP && lefttype != APE_OBJECT_STRING)
                    {
                        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame),
                            "type %s is not indexable (in APE_OPCODE_GETINDEX)", left_type_name);
                        goto err;
                    }
                    objres = ape_object_make_null(vm->context);
                    if(lefttype == APE_OBJECT_ARRAY)
                    {
                        if(indextype != APE_OBJECT_NUMBER)
                        {
                            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame),
                                              "cannot index %s with %s", left_type_name, indextypename);
                            goto err;
                        }
                        ix = (int)object_value_asnumber(index);
                        if(ix < 0)
                        {
                            ix = ape_object_array_getlength(left) + ix;
                        }
                        if(ix >= 0 && ix < (ApeInt_t)ape_object_array_getlength(left))
                        {
                            objres = ape_object_array_getvalue(left, ix);
                        }
                    }
                    else if(lefttype == APE_OBJECT_MAP)
                    {
                        objres = ape_object_map_getvalueobject(left, index);
                    }
                    else if(lefttype == APE_OBJECT_STRING)
                    {
                        str = ape_object_string_getdata(left);
                        leftlen = ape_object_string_getlength(left);
                        ix = (int)object_value_asnumber(index);
                        if(ix >= 0 && ix < leftlen)
                        {
                            char res_str[2] = { str[ix], '\0' };
                            objres = ape_object_make_string(vm->context, res_str);
                        }
                    }
                    ape_vm_pushstack(vm, objres);
                }
                break;

            case APE_OPCODE_GETVALUEAT:
            {
                index = ape_vm_popstack(vm);
                left = ape_vm_popstack(vm);
                lefttype = object_value_type(left);
                indextype = object_value_type(index);
                left_type_name = ape_object_value_typename(lefttype);
                indextypename = ape_object_value_typename(indextype);
                if(lefttype != APE_OBJECT_ARRAY && lefttype != APE_OBJECT_MAP && lefttype != APE_OBJECT_STRING)
                {
                    ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame),
                                      "type %s is not indexable (in APE_OPCODE_GETVALUEAT)", left_type_name);
                    goto err;
                }
                objres = ape_object_make_null(vm->context);
                if(indextype != APE_OBJECT_NUMBER)
                {
                    ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame),
                                      "cannot index %s with %s", left_type_name, indextypename);
                    goto err;
                }
                ix = (int)object_value_asnumber(index);
                if(lefttype == APE_OBJECT_ARRAY)
                {
                    objres = ape_object_array_getvalue(left, ix);
                }
                else if(lefttype == APE_OBJECT_MAP)
                {
                    objres = ape_object_getkvpairat(vm->context, left, ix);
                }
                else if(lefttype == APE_OBJECT_STRING)
                {
                    str = ape_object_string_getdata(left);
                    leftlen = ape_object_string_getlength(left);
                    ix = (int)object_value_asnumber(index);
                    if(ix >= 0 && ix < leftlen)
                    {
                        char res_str[2] = { str[ix], '\0' };
                        objres = ape_object_make_string(vm->context, res_str);
                    }
                }
                ape_vm_pushstack(vm, objres);
                break;
            }
            case APE_OPCODE_CALL:
            {
                num_args = ape_frame_readuint8(vm->current_frame);
                callee = ape_vm_getstack(vm, num_args);
                ok = ape_vm_callobject(vm, callee, num_args);
                if(!ok)
                {
                    goto err;
                }
                break;
            }
            case APE_OPCODE_RETURNVALUE:
            {
                objres = ape_vm_popstack(vm);
                ok = ape_vm_popframe(vm);
                if(!ok)
                {
                    goto end;
                }
                ape_vm_pushstack(vm, objres);
                break;
            }
            case APE_OPCODE_RETURNNOTHING:
            {
                ok = ape_vm_popframe(vm);
                ape_vm_pushstack(vm, ape_object_make_null(vm->context));
                if(!ok)
                {
                    ape_vm_popstack(vm);
                    goto end;
                }
                break;
            }
            case APE_OPCODE_DEFLOCAL:
            {
                pos = ape_frame_readuint8(vm->current_frame);
                vm->stack[vm->current_frame->base_pointer + pos] = ape_vm_popstack(vm);
                break;
            }
            case APE_OPCODE_SETLOCAL:
            {
                pos = ape_frame_readuint8(vm->current_frame);
                new_value = ape_vm_popstack(vm);
                old_value = vm->stack[vm->current_frame->base_pointer + pos];
                if(!ape_vm_checkassign(vm, old_value, new_value))
                {
                    goto err;
                }
                vm->stack[vm->current_frame->base_pointer + pos] = new_value;
                break;
            }
            case APE_OPCODE_GETLOCAL:
            {
                pos = ape_frame_readuint8(vm->current_frame);
                objval = vm->stack[vm->current_frame->base_pointer + pos];
                ape_vm_pushstack(vm, objval);
                break;
            }
            case APE_OPCODE_GETCONTEXTGLOBAL:
            {
                ix = ape_frame_readuint16(vm->current_frame);
                ok = false;
                objval = ape_globalstore_getat(vm->globalstore, ix, &ok);
                if(!ok)
                {
                    ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame),
                                      "global value %d not found", ix);
                    goto err;
                }
                ape_vm_pushstack(vm, objval);
                break;
            }
            case APE_OPCODE_MKFUNCTION:
                {
                    constant_ix = ape_frame_readuint16(vm->current_frame);
                    num_free = ape_frame_readuint8(vm->current_frame);
                    constant = (ApeObject_t*)ape_valarray_get(constants, constant_ix);
                    if(!constant)
                    {
                        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame),
                                          "constant %d not found", constant_ix);
                        goto err;
                    }
                    constant_type = object_value_type(*constant);
                    if(constant_type != APE_OBJECT_FUNCTION)
                    {
                        type_name = ape_object_value_typename(constant_type);
                        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame),
                                          "%s is not a function", type_name);
                        goto err;
                    }
                    constfunction = object_value_asfunction(*constant);
                    function_obj = ape_object_make_function(vm->context, ape_object_function_getname(*constant), constfunction->comp_result,
                                           false, constfunction->num_locals, constfunction->num_args, num_free);
                    if(object_value_isnull(function_obj))
                    {
                        goto err;
                    }
                    for(i = 0; i < num_free; i++)
                    {
                        free_val = vm->stack[vm->sp - num_free + i];
                        ape_object_function_setfreeval(function_obj, i, free_val);
                    }
                    ape_vm_setstackpointer(vm, vm->sp - num_free);
                    ape_vm_pushstack(vm, function_obj);
                }
                break;
            case APE_OPCODE_GETFREE:
                {
                    free_ix = ape_frame_readuint8(vm->current_frame);
                    objval = ape_object_function_getfreeval(vm->current_frame->function, free_ix);
                    ape_vm_pushstack(vm, objval);
                }
                break;
            case APE_OPCODE_SETFREE:
                {
                    free_ix = ape_frame_readuint8(vm->current_frame);
                    objval = ape_vm_popstack(vm);
                    ape_object_function_setfreeval(vm->current_frame->function, free_ix, objval);
                }
                break;
            case APE_OPCODE_CURRENTFUNCTION:
                {
                    current_function = vm->current_frame->function;
                    ape_vm_pushstack(vm, current_function);
                }
                break;
            case APE_OPCODE_SETINDEX:
                {
                    index = ape_vm_popstack(vm);
                    left = ape_vm_popstack(vm);
                    new_value = ape_vm_popstack(vm);
                    lefttype = object_value_type(left);
                    indextype = object_value_type(index);
                    left_type_name = ape_object_value_typename(lefttype);
                    indextypename = ape_object_value_typename(indextype);
                    if(lefttype != APE_OBJECT_ARRAY && lefttype != APE_OBJECT_MAP)
                    {
                        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame),
                                          "type %s is not indexable (in APE_OPCODE_SETINDEX)", left_type_name);
                        goto err;
                    }

                    if(lefttype == APE_OBJECT_ARRAY)
                    {
                        if(indextype != APE_OBJECT_NUMBER)
                        {
                            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame),
                                              "cannot index %s with %s", left_type_name, indextypename);
                            goto err;
                        }
                        ix = (int)object_value_asnumber(index);
                        ok = ape_object_array_setat(left, ix, new_value);
                        if(!ok)
                        {
                            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame),
                                             "setting array item failed (index %d out of bounds of %d)", ix, ape_object_array_getlength(left));
                            goto err;
                        }
                    }
                    else if(lefttype == APE_OBJECT_MAP)
                    {
                        old_value = ape_object_map_getvalueobject(left, index);
                        if(!ape_vm_checkassign(vm, old_value, new_value))
                        {
                            goto err;
                        }
                        ok = ape_object_map_setvalue(left, index, new_value);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                }
                break;
            case APE_OPCODE_DUP:
                {
                    objval = ape_vm_getstack(vm, 0);
                    ape_vm_pushstack(vm, ape_object_value_copyflat(vm->context, objval));
                }
                break;
            case APE_OPCODE_LEN:
                {
                    objval = ape_vm_popstack(vm);
                    len = 0;
                    type = object_value_type(objval);
                    if(type == APE_OBJECT_ARRAY)
                    {
                        len = ape_object_array_getlength(objval);
                    }
                    else if(type == APE_OBJECT_MAP)
                    {
                        len = ape_object_map_getlength(objval);
                    }
                    else if(type == APE_OBJECT_STRING)
                    {
                        len = ape_object_string_getlength(objval);
                    }
                    else
                    {
                        type_name = ape_object_value_typename(type);
                        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame),
                                          "cannot get length of %s", type_name);
                        goto err;
                    }
                    ape_vm_pushstack(vm, ape_object_make_number(vm->context, len));
                }
                break;
            case APE_OPCODE_MKNUMBER:
                {
                    // FIXME: why does ape_util_uinttofloat break things here?
                    val = ape_frame_readuint64(vm->current_frame);
                    //valdouble = ape_util_uinttofloat(val);
                    valdouble = val;
                    objval = ape_object_make_number(vm->context, valdouble);
                    ape_vm_pushstack(vm, objval);
                }
                break;
            case APE_OPCODE_SETRECOVER:
                {
                    recover_ip = ape_frame_readuint16(vm->current_frame);
                    vm->current_frame->recover_ip = recover_ip;
                }
                break;
            default:
                {
                    APE_ASSERT(false);
                    ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame), "unknown opcode: 0x%x", opcode);
                    goto err;
                }
                break;
        }
        if(checktime)
        {
            time_check_counter++;
            if(time_check_counter > time_check_interval)
            {
                elapsed_ms = (int)ape_util_timergetelapsed(&timer);
                if(elapsed_ms > maxexecms)
                {
                    ape_errorlist_addformat(vm->errors, APE_ERROR_TIMEOUT, ape_frame_srcposition(vm->current_frame),
                                      "execution took more than %1.17g ms", maxexecms);
                    goto err;
                }
                time_check_counter = 0;
            }
        }
    err:
        if(ape_errorlist_count(vm->errors) > 0)
        {
            err = ape_errorlist_lasterror(vm->errors);
            if(err->type == APE_ERROR_RUNTIME && ape_errorlist_count(vm->errors) == 1)
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
                        err->traceback = ape_make_traceback(vm->context);
                    }
                    if(err->traceback)
                    {
                        ape_traceback_appendfromvm(err->traceback, vm);
                    }
                    while((ApeInt_t)vm->frames_count > (ApeInt_t)(recover_frame_ix + 1))
                    {
                        ape_vm_popframe(vm);
                    }
                    err_obj = ape_object_make_error(vm->context, err->message);
                    if(!object_value_isnull(err_obj))
                    {
                        ape_object_value_seterrortraceback(err_obj, err->traceback);
                        err->traceback = NULL;
                    }
                    ape_vm_pushstack(vm, err_obj);
                    vm->current_frame->ip = vm->current_frame->recover_ip;
                    vm->current_frame->is_recovering = true;
                    ape_errorlist_clear(vm->errors);
                }
            }
            else
            {
                goto end;
            }
        }
        if(ape_gcmem_shouldsweep(vm->mem))
        {
            ape_vm_collectgarbage(vm, constants);
        }
    }
end:
    if(ape_errorlist_count(vm->errors) > 0)
    {
        err = ape_errorlist_lasterror(vm->errors);
        if(!err->traceback)
        {
            err->traceback = ape_make_traceback(vm->context);
        }
        if(err->traceback)
        {
            ape_traceback_appendfromvm(err->traceback, vm);
        }
    }

    ape_vm_collectgarbage(vm, constants);

    vm->running = false;
    return ape_errorlist_count(vm->errors) == 0;
}
