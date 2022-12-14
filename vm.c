/*
* this file contains the VM of ape.
* it also contains some functions that probably should be in their own files,
* but that's legacy from the original code (where everything was stuffed into one file).
*/

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

#include "inline.h"

static const ApePosition_t g_vmpriv_srcposinvalid = { NULL, -1, -1 };


bool ape_vm_setsymbol(ApeSymTable_t *table, ApeSymbol_t *symbol);
int ape_vm_nextsymbolindex(ApeSymTable_t *table);
int ape_vm_countnumdefs(ApeSymTable_t *table);
void ape_vm_setstackpointer(ApeVM_t *vm, int new_sp);
bool ape_vm_tryoverloadoperator(ApeVM_t *vm, ApeObject_t left, ApeObject_t right, ApeOpByte_t op, bool *out_overload_found);

/*
* todo: these MUST reflect the order of ApeOpcodeValue_t.
* meaning its prone to break terribly if and/or when it is changed.
*/
static ApeOpcodeDef_t g_definitions[APE_OPCODE_MAX + 1] =
{
    { "none", 0, { 0 } },
    { "constant", 1, { 2 } },
    { "op(+)", 0, { 0 } },
    { "pop", 0, { 0 } },
    { "sub", 0, { 0 } },
    { "op(*)", 0, { 0 } },
    { "op(/)", 0, { 0 } },
    { "op(%)", 0, { 0 } },
    { "true", 0, { 0 } },
    { "false", 0, { 0 } },
    { "compare", 0, { 0 } },
    { "compareeq", 0, { 0 } },
    { "equal", 0, { 0 } },
    { "notequal", 0, { 0 } },
    { "greaterthan", 0, { 0 } },
    { "greaterequal", 0, { 0 } },
    { "op(-)", 0, { 0 } },
    { "op(!)", 0, { 0 } },
    { "jump", 1, { 2 } },
    { "jumpiffalse", 1, { 2 } },
    { "jumpiftrue", 1, { 2 } },
    { "null", 0, { 0 } },
    { "getmoduleglobal", 1, { 2 } },
    { "setmoduleglobal", 1, { 2 } },
    { "definemoduleglobal", 1, { 2 } },
    { "array", 1, { 2 } },
    { "mapstart", 1, { 2 } },
    { "mapend", 1, { 2 } },
    { "getthis", 0, { 0 } },
    { "getindex", 0, { 0 } },
    { "setindex", 0, { 0 } },
    { "getvalue_at", 0, { 0 } },
    { "call", 1, { 1 } },
    { "returnvalue", 0, { 0 } },
    { "return", 0, { 0 } },
    { "getlocal", 1, { 1 } },
    { "definelocal", 1, { 1 } },
    { "setlocal", 1, { 1 } },
    { "getcontextglobal", 1, { 2 } },
    { "function", 2, { 2, 1 } },
    { "getfree", 1, { 1 } },
    { "setfree", 1, { 1 } },
    { "currentfunction", 0, { 0 } },
    { "dup", 0, { 0 } },
    { "number", 1, { 8 } },
    { "len", 0, { 0 } },
    { "setrecover", 1, { 2 } },
    { "op(|)", 0, { 0 } },
    { "op(^)", 0, { 0 } },
    { "op(&)", 0, { 0 } },
    { "op(<<)", 0, { 0 } },
    { "op(>>)", 0, { 0 } },
    { "import", 1, {1} },
    { "invalid_max", 0, { 0 } },
};

ApeOpcodeDef_t* ape_vm_opcodefind(ApeOpByte_t op)
{
    if(op <= APE_OPCODE_NONE || op >= APE_OPCODE_MAX)
    {
        return NULL;
    }
    return &g_definitions[op];
}

const char* ape_vm_opcodename(ApeOpByte_t op)
{
    if(op <= APE_OPCODE_NONE || op >= APE_OPCODE_MAX)
    {
        return NULL;
    }
    return g_definitions[op].name;
}

void ape_vm_adderrorv(ApeVM_t* vm, ApeErrorType_t etype, const char* fmt, va_list va)
{
    ape_errorlist_addformatv(vm->errors, etype, ape_frame_srcposition(vm->currentframe), fmt, va);
}

void ape_vm_adderror(ApeVM_t* vm, ApeErrorType_t etype, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    ape_vm_adderrorv(vm, etype, fmt, va);
    va_end(va);
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
    store->symbols = ape_make_strdict(ctx, (ApeDataCallback_t)ape_symbol_copy, (ApeDataCallback_t)ape_symbol_destroy);
    if(!store->symbols)
    {
        goto err;
    }
    store->objects = ape_make_valarray(ctx, ApeObject_t);
    if(!store->objects)
    {
        goto err;
    }
    if(mem)
    {
        for(i = 0; i < ape_builtins_count(); i++)
        {
            name = ape_builtins_getname(i);
            builtin = ape_object_make_nativefuncmemory(ctx, name, ape_builtins_getfunc(i), NULL, 0);
            if(ape_object_value_isnull(builtin))
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
    ape_allocator_free(&store->context->alloc, store);
}

ApeSymbol_t* ape_globalstore_getsymbol(ApeGlobalStore_t* store, const char* name)
{
    return (ApeSymbol_t*)ape_strdict_getbyname(store->symbols, name);
}

bool ape_globalstore_set(ApeGlobalStore_t* store, const char* name, ApeObject_t object)
{
    ApeInt_t ix;
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

ApeSymbol_t* ape_make_symbol(ApeContext_t* ctx, const char* name, ApeSymbolType_t type, ApeSize_t index, bool assignable)
{
    ApeSymbol_t* symbol;
    symbol = (ApeSymbol_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeSymbol_t));
    if(!symbol)
    {
        return NULL;
    }
    memset(symbol, 0, sizeof(ApeSymbol_t));
    symbol->context = ctx;
    symbol->name = ape_util_strdup(ctx, name);
    if(!symbol->name)
    {
        ape_allocator_free(&ctx->alloc, symbol);
        return NULL;
    }
    symbol->symtype = type;
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
    ape_allocator_free(&symbol->context->alloc, symbol->name);
    ape_allocator_free(&symbol->context->alloc, symbol);
    return NULL;
}

ApeSymbol_t* ape_symbol_copy(ApeSymbol_t* symbol)
{
    return ape_make_symbol(symbol->context, symbol->name, symbol->symtype, symbol->index, symbol->assignable);
}

ApeSymTable_t* ape_make_symtable(ApeContext_t* ctx, ApeSymTable_t* outer, ApeGlobalStore_t* global_store, int mgo)
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
    table->maxnumdefinitions = 0;
    table->outer = outer;
    table->globalstore = global_store;
    table->modglobaloffset = mgo;
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
    table->modglobalsymbols = ape_make_ptrarray(ctx);
    if(!table->modglobalsymbols)
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
    ApeContext_t* ctx;
    if(!table)
    {
        return;
    }
    while(ape_ptrarray_count(table->blockscopes) > 0)
    {
        ape_symtable_popblockscope(table);
    }
    ape_ptrarray_destroy(table->blockscopes);
    ape_ptrarray_destroywithitems(table->modglobalsymbols, (ApeDataCallback_t)ape_symbol_destroy);
    ape_ptrarray_destroywithitems(table->freesymbols, (ApeDataCallback_t)ape_symbol_destroy);
    ctx = table->context;
    memset(table, 0, sizeof(ApeSymTable_t));
    ape_allocator_free(&ctx->alloc, table);
}

ApeSymTable_t* ape_symtable_copy(ApeSymTable_t* table)
{
    ApeSymTable_t* copy;
    copy = (ApeSymTable_t*)ape_allocator_alloc(&table->context->alloc, sizeof(ApeSymTable_t));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(ApeSymTable_t));
    copy->context = table->context;
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
    copy->modglobalsymbols = ape_ptrarray_copywithitems(table->modglobalsymbols, (ApeDataCallback_t)ape_symbol_copy, (ApeDataCallback_t)ape_symbol_destroy);
    if(!copy->modglobalsymbols)
    {
        goto err;
    }
    copy->maxnumdefinitions = table->maxnumdefinitions;
    copy->modglobaloffset = table->modglobaloffset;
    return copy;
err:
    ape_symtable_destroy(copy);
    return NULL;
}

bool ape_symtable_addmodulesymbol(ApeSymTable_t* st, ApeSymbol_t* symbol)
{
    bool ok;
    if(symbol->symtype != APE_SYMBOL_MODULEGLOBAL)
    {
        APE_ASSERT(false);
        return false;
    }
    if(ape_symtable_symbol_is_defined(st, symbol->name))
    {
        /* todo: make sure it should be true in this case */
        return true;
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

ApeSymbol_t* ape_symtable_define(ApeSymTable_t* table, const char* name, bool assignable)
{    
    bool ok;
    bool global_symbol_added;
    int ix;
    ApeSize_t definitions_count;
    ApeAstBlockScope_t* top_scope;
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
        /* module symbol */
        return NULL;
    }
    if(APE_STREQ(name, "this"))
    {
        /* "this" is reserved */
        return NULL;
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
        ok = ape_ptrarray_add(table->modglobalsymbols, global_symbol_copy);
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
            global_symbol_copy = (ApeSymbol_t*)ape_ptrarray_pop(table->modglobalsymbols);
            ape_symbol_destroy(global_symbol_copy);
        }
        ape_symbol_destroy(symbol);
        return NULL;
    }
    top_scope = (ApeAstBlockScope_t*)ape_ptrarray_top(table->blockscopes);
    top_scope->numdefinitions++;
    definitions_count = ape_vm_countnumdefs(table);
    if(definitions_count > table->maxnumdefinitions)
    {
        table->maxnumdefinitions = definitions_count;
    }

    return symbol;
}

ApeSymbol_t* ape_symtable_deffree(ApeSymTable_t* st, ApeSymbol_t* original)
{
    bool ok;
    ApeSymbol_t* symbol;
    ApeSymbol_t* copy;
    copy = ape_make_symbol(st->context, original->name, original->symtype, original->index, original->assignable);
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

ApeSymbol_t* ape_symtable_definefuncname(ApeSymTable_t* st, const char* name, bool assignable)
{
    bool ok;
    ApeSymbol_t* symbol;
    if(strchr(name, ':'))
    {
        /* module symbol */
        return NULL;
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

ApeSymbol_t* ape_symtable_definethis(ApeSymTable_t* st)
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

ApeSymbol_t* ape_symtable_resolve(ApeSymTable_t* table, const char* name)
{
    ApeInt_t i;
    ApeSymbol_t* symbol;
    ApeAstBlockScope_t* scope;
    scope = NULL;
    symbol = ape_globalstore_getsymbol(table->globalstore, name);
    if(symbol)
    {
        return symbol;
    }
    for(i = (ApeInt_t)ape_ptrarray_count(table->blockscopes) - 1; i >= 0; i--)
    {
        scope = (ApeAstBlockScope_t*)ape_ptrarray_get(table->blockscopes, i);
        symbol = (ApeSymbol_t*)ape_strdict_getbyname(scope->store, name);
        if(symbol)
        {
            break;
        }
    }
    if(symbol && symbol->symtype == APE_SYMBOL_THIS)
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
        if(symbol->symtype == APE_SYMBOL_MODULEGLOBAL || symbol->symtype == APE_SYMBOL_CONTEXTGLOBAL)
        {
            return symbol;
        }
        symbol = ape_symtable_deffree(table, symbol);
    }
    return symbol;
}

bool ape_symtable_symbol_is_defined(ApeSymTable_t* table, const char* name)
{
    ApeAstBlockScope_t* top_scope;
    ApeSymbol_t* symbol;
    /* todo: rename to something more obvious */
    symbol = ape_globalstore_getsymbol(table->globalstore, name);
    if(symbol)
    {
        return true;
    }
    top_scope = (ApeAstBlockScope_t*)ape_ptrarray_top(table->blockscopes);
    symbol = (ApeSymbol_t*)ape_strdict_getbyname(top_scope->store, name);
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
    ApeAstBlockScope_t* prev_block_scope;
    ApeAstBlockScope_t* new_scope;
    block_scope_offset = 0;
    prev_block_scope = (ApeAstBlockScope_t*)ape_ptrarray_top(table->blockscopes);
    if(prev_block_scope)
    {
        block_scope_offset = table->modglobaloffset + prev_block_scope->offset + prev_block_scope->numdefinitions;
    }
    else
    {
        block_scope_offset = table->modglobaloffset;
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
    ApeAstBlockScope_t* top_scope;
    top_scope = (ApeAstBlockScope_t*)ape_ptrarray_top(table->blockscopes);
    ape_ptrarray_pop(table->blockscopes);
    ape_blockscope_destroy(top_scope);
}

ApeAstBlockScope_t* ape_symtable_getblockscope(ApeSymTable_t* table)
{
    ApeAstBlockScope_t* top_scope;
    top_scope = (ApeAstBlockScope_t*)ape_ptrarray_top(table->blockscopes);
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
    return ape_ptrarray_count(table->modglobalsymbols);
}

const ApeSymbol_t* ape_symtable_getmoduleglobalsymbolat(const ApeSymTable_t* table, int ix)
{
    return (ApeSymbol_t*)ape_ptrarray_get(table->modglobalsymbols, ix);
}

ApeAstBlockScope_t* ape_make_blockscope(ApeContext_t* ctx, int offset)
{
    ApeAstBlockScope_t* new_scope;
    new_scope = (ApeAstBlockScope_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstBlockScope_t));
    if(!new_scope)
    {
        return NULL;
    }
    memset(new_scope, 0, sizeof(ApeAstBlockScope_t));
    new_scope->context = ctx;
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

void* ape_blockscope_destroy(ApeAstBlockScope_t* scope)
{
    ape_strdict_destroywithitems(scope->store);
    ape_allocator_free(&scope->context->alloc, scope);
    return NULL;
}

ApeAstBlockScope_t* ape_blockscope_copy(ApeAstBlockScope_t* scope)
{
    ApeAstBlockScope_t* copy;
    copy = (ApeAstBlockScope_t*)ape_allocator_alloc(&scope->context->alloc, sizeof(ApeAstBlockScope_t));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(ApeAstBlockScope_t));
    copy->context = scope->context;
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

bool ape_vm_setsymbol(ApeSymTable_t* table, ApeSymbol_t* symbol)
{
    ApeAstBlockScope_t* top_scope;
    ApeSymbol_t* existing;
    top_scope = (ApeAstBlockScope_t*)ape_ptrarray_top(table->blockscopes);
    existing= (ApeSymbol_t*)ape_strdict_getbyname(top_scope->store, symbol->name);
    if(existing)
    {
        ape_symbol_destroy(existing);
    }
    return ape_strdict_set(top_scope->store, symbol->name, symbol);
}

int ape_vm_nextsymbolindex(ApeSymTable_t* table)
{
    int ix;
    ApeAstBlockScope_t* top_scope;
    top_scope = (ApeAstBlockScope_t*)ape_ptrarray_top(table->blockscopes);
    ix = top_scope->offset + top_scope->numdefinitions;
    return ix;
}

int ape_vm_countnumdefs(ApeSymTable_t* table)
{
    ApeInt_t i;
    int count;
    ApeAstBlockScope_t* scope;
    count = 0;
    for(i = (ApeInt_t)ape_ptrarray_count(table->blockscopes) - 1; i >= 0; i--)
    {
        scope = (ApeAstBlockScope_t*)ape_ptrarray_get(table->blockscopes, i);
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

int ape_make_code(ApeOpByte_t op, ApeSize_t operands_count, ApeOpByte_t* operands, ApeValArray_t* res)
{
    ApeSize_t i;
    int width;
    int instr_len;
    bool ok;
    ApeUShort_t val;
    ApeOpcodeDef_t* def;
    def = ape_vm_opcodefind(op);
    if(!def)
    {
        return 0;
    }
    instr_len = 1;
    for(i = 0; i < def->operandcount; i++)
    {
        instr_len += def->operandwidths[i];
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
        width = def->operandwidths[i];
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


ApeOpcodeValue_t ape_frame_readopcode(ApeFrame_t* frame)
{
    frame->srcip = frame->ip;
    return (ApeOpcodeValue_t)ape_frame_readuint8(frame);
}

ApeOpByte_t ape_frame_readuint64(ApeFrame_t* frame)
{
    ApeOpByte_t res;
    const ApeUShort_t* data;
    data = frame->bytecode + frame->ip;
    frame->ip += 8;
    res = 0;
    res |= (ApeOpByte_t)data[7];
    res |= (ApeOpByte_t)data[6] << 8;
    res |= (ApeOpByte_t)data[5] << 16;
    res |= (ApeOpByte_t)data[4] << 24;
    res |= (ApeOpByte_t)data[3] << 32;
    res |= (ApeOpByte_t)data[2] << 40;
    res |= (ApeOpByte_t)data[1] << 48;
    res |= (ApeOpByte_t)data[0] << 56;
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
        return frame->srcpositions[frame->srcip];
    }
    return g_vmpriv_srcposinvalid;
}

ApeObject_t ape_vm_getlastpopped(ApeVM_t* vm)
{
    return vm->lastpopped;
}

bool ape_vm_haserrors(ApeVM_t* vm)
{
    return ape_errorlist_count(vm->errors) > 0;
}

bool ape_vm_setglobal(ApeVM_t* vm, ApeSize_t ix, ApeObject_t val)
{
    ape_valdict_set(vm->globalobjects, &ix, &val);
    return true;
}

ApeObject_t ape_vm_getglobal(ApeVM_t* vm, ApeSize_t ix)
{
    ApeObject_t* rt;
    rt = (ApeObject_t*)ape_valdict_getbykey(vm->globalobjects, &ix);
    if(rt == NULL)
    {
        return ape_object_make_null(vm->context);
    }
    return *rt;
}

void ape_vm_setstackpointer(ApeVM_t* vm, int new_sp)
{
    int count;
    size_t bytescount;
    ApeSize_t i;
    ApeSize_t idx;
    idx = vm->stackptr;
    /* to avoid gcing freed objects */
    if((ApeInt_t)new_sp > (ApeInt_t)idx)
    {
        count = new_sp - idx;
        bytescount = count * sizeof(ApeObject_t);
        /*
        * TODO:FIXME: this is probably where the APE_OBJECT_NUMBER might come from.
        * but it's not really obvious, tbh.
        */
        /*
        //memset(vm->stackobjects + vm->stackptr, 0, bytescount);
        //ape_valdict_clear(vm->stackobjects);
        //memset(vm->stackobjects->values + vm->stackptr, 0, bytescount);
        */
        for(i=vm->stackptr; i<bytescount; i++)
        {
            ApeObject_t nullval = ape_object_make_null(vm->context);
            ape_valdict_set(vm->stackobjects, &i, &nullval);
        }
    }
    vm->stackptr = new_sp;
}

void ape_vm_pushstack(ApeVM_t* vm, ApeObject_t obj)
{
    ApeSize_t idx;
#if defined(APE_DEBUG) && (APE_DEBUG == 1)
    if(vm->currentframe)
    {
        ApeFrame_t* frame = vm->currentframe;
        ApeScriptFunction_t* current_function = ape_object_value_asfunction(frame->function);
        int nl = current_function->numlocals;
        APE_ASSERT(vm->stackptr >= (frame->basepointer + nl));
    }
#endif
    idx = vm->stackptr;
    ape_valdict_set(vm->stackobjects, &idx, &obj);
    vm->stackptr++;
}

ApeObject_t ape_vm_popstack(ApeVM_t* vm)
{
    ApeSize_t idx;
    ApeObject_t* ptr;
    ApeObject_t objres;
#if defined(APE_DEBUG) && (APE_DEBUG == 1)
    if(vm->stackptr == 0)
    {
        ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe), "stack underflow");
        APE_ASSERT(false);
        return ape_object_make_null(vm->context);
    }
    if(vm->currentframe)
    {
        ApeFrame_t* frame = vm->currentframe;
        ApeScriptFunction_t* current_function = ape_object_value_asfunction(frame->function);
        int nl = current_function->numlocals;
        APE_ASSERT((vm->stackptr - 1) >= (frame->basepointer + nl));
    }
#endif
    vm->stackptr--;
    idx = vm->stackptr-0;
    ptr = (ApeObject_t*)ape_valdict_getbykey(vm->stackobjects, &idx);
    if(ptr == NULL)
    {
        return ape_object_make_null(vm->context);
    }
    objres = *ptr;
    /*
    * TODO:FIXME: somewhere along the line, objres.handle->datatype is either not set, or set incorrectly.
    * this right here works, but it shouldn't be necessary.
    * specifically, objres.handle->datatype gets ***sometimes*** set to APE_OBJECT_NONE, and
    * so far i've only observed this when objres.type==APE_OBJECT_NUMBER.
    * very strange, very weird, very heisenbug-ish.
    */
    if(objres.handle != NULL)
    {
        objres.handle->datatype = objres.type;
    }
    vm->lastpopped = objres;
    return objres;
}

ApeObject_t ape_vm_getstack(ApeVM_t* vm, int nth_item)
{
    ApeSize_t ix;
    ApeObject_t* p;
    ix = vm->stackptr - 1 - nth_item;
    p = (ApeObject_t*)ape_valdict_getbykey(vm->stackobjects, &ix);
    if(p == NULL)
    {
        return ape_object_make_null(vm->context);
    }

    return *p;
}

void ape_vm_pushthisstack(ApeVM_t* vm, ApeObject_t obj)
{
#if defined(APE_DEBUG) && (APE_DEBUG == 1)
    if(vm->thisptr >= APE_CONF_SIZE_VM_THISSTACK)
    {
        APE_ASSERT(false);
        ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe), "this stack overflow");
        return;
    }
#endif
    vm->thisobjects[vm->thisptr] = obj;
    vm->thisptr++;
}

ApeObject_t ape_vm_popthisstack(ApeVM_t* vm)
{
#if defined(APE_DEBUG) && (APE_DEBUG == 1)
    if(vm->thisptr == 0)
    {
        ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe), "this stack underflow");
        return ape_object_make_null(vm->context);
    }
#endif
    vm->thisptr--;
    return vm->thisobjects[vm->thisptr];
}

ApeObject_t ape_vm_getthisstack(ApeVM_t* vm, int nth_item)
{
    int ix;
    ix = vm->thisptr - 1 - nth_item;
    if(ix < 0)
    {
        ix = nth_item;
    }
#if defined(APE_DEBUG) && (APE_DEBUG == 1)
    if(ix < 0 || ix >= APE_CONF_SIZE_VM_THISSTACK)
    {
        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe), "invalid this shared->stack index: %d", nth_item);
        APE_ASSERT(false);
        return ape_object_make_null(vm->context);
    }
#endif
    return vm->thisobjects[ix];
}


void ape_vm_dumpstack(ApeVM_t* vm)
{
    ApeInt_t i;
    ApeSize_t key;
    ApeWriter_t* wr;
    ApeSize_t* keys;
    ApeObject_t* vals;
    keys = ((ApeSize_t*)(vm->stackobjects->keys));
    vals = ((ApeObject_t*)(vm->stackobjects->values));
    wr = ape_make_writerio(vm->context, stderr, false, true);
    {
        for(i=0; i<vm->stackptr; i++)
        {
            key = keys[i];
            fprintf(stderr, "vm->stack[%d] = (%s) = [[", (int)key, ape_object_value_typename(ape_object_value_type(vals[i])));
            ape_tostring_object(wr, vals[i], true);
            fprintf(stderr, "]]\n");
        }
    }
    ape_writer_destroy(wr);
}


ApeObject_t ape_vm_callnativefunction(ApeVM_t* vm, ApeObject_t callee, ApePosition_t src_pos, int argc, ApeObject_t* args)
{
    ApeError_t* err;
    ApeObject_t objres;
    ApeObjType_t restype;
    ApeNativeFunction_t* nfunc;
    ApeTraceback_t* traceback;
    nfunc = ape_object_value_asnativefunction(callee);
    objres = nfunc->nativefnptr(vm, nfunc->dataptr, argc, args);
    if(ape_errorlist_haserrors(vm->errors) && !APE_STREQ(nfunc->name, "crash"))
    {
        err = ape_errorlist_lasterror(vm->errors);
        err->pos = src_pos;
        err->traceback = ape_make_traceback(vm->context);
        if(err->traceback)
        {
            ape_traceback_append(err->traceback, nfunc->name, g_vmpriv_srcposinvalid);
        }
        return ape_object_make_null(vm->context);
    }
    restype = ape_object_value_type(objres);
    if(restype == APE_OBJECT_ERROR)
    {
        traceback = ape_make_traceback(vm->context);
        if(traceback)
        {
            /* error builtin is treated in a special way */
            if(!APE_STREQ(nfunc->name, "error"))
            {
                ape_traceback_append(traceback, nfunc->name, g_vmpriv_srcposinvalid);
            }
            ape_traceback_appendfromvm(traceback, vm);
            ape_object_value_seterrortraceback(objres, traceback);
        }
    }
    return objres;
}


bool ape_vm_callobjectargs(ApeVM_t* vm, ApeObject_t callee, ApeInt_t nargs, ApeObject_t* args)
{
    bool ok;
    ApeSize_t idx;
    ApeInt_t ofs;
    ApeInt_t actualargs;
    ApeObjType_t calleetype;
    ApeScriptFunction_t* scriptcallee;
    ApeFrame_t framecallee;
    ApeObject_t* fwdargs;
    ApeObject_t* stackpos;
    ApeObject_t* stackvals;
    ApeObject_t objres;
    const char* calleetypename;
    calleetype = ape_object_value_type(callee);
    if(calleetype == APE_OBJECT_SCRIPTFUNCTION)
    {
        scriptcallee = ape_object_value_asfunction(callee);
        #if 0
        if(nargs != -1)
        {
            if(nargs != (ApeInt_t)scriptcallee->numargs)
            {
                ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe),
                                  "invalid number of arguments to \"%s\", expected %d, got %d",
                                  ape_object_function_getname(callee), scriptcallee->numargs, nargs);
                return false;
            }
        }
        #endif
        ofs = nargs;
        if(nargs == -1)
        {
            ofs = 0;
        }
        ok = ape_frame_init(&framecallee, callee, vm->stackptr - ofs);
        if(!ok)
        {
            ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, g_vmpriv_srcposinvalid, "frame init failed in ape_vm_callobjectargs");
            return false;
        }
        ok = ape_vm_pushframe(vm, framecallee);
        if(!ok)
        {
            ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, g_vmpriv_srcposinvalid, "pushing frame failed in ape_vm_callobjectargs");
            return false;
        }
    }
    else if(calleetype == APE_OBJECT_NATIVEFUNCTION)
    {
        ofs = nargs;
        if(nargs == -1)
        {
            ofs = 0;
        }
        idx = vm->stackptr;
        stackvals = ((ApeObject_t*)(vm->stackobjects->values));
        stackpos = stackvals + idx - ofs;
        if(args == NULL)
        {
            fwdargs = stackpos;
        }
        else
        {
            fwdargs = args;
        }
        if(vm->context->config.dumpstack)
        {
            ape_vm_dumpstack(vm);
        }
        actualargs = nargs;
        if(nargs == -1)
        {
            actualargs = vm->stackptr - ofs;
        }
        objres = ape_vm_callnativefunction(vm, callee, ape_frame_srcposition(vm->currentframe), actualargs, fwdargs);
        if(ape_vm_haserrors(vm))
        {
            return false;
        }
        ape_vm_setstackpointer(vm, vm->stackptr - ofs - 1);
        ape_vm_pushstack(vm, objres);
    }
    else
    {
        calleetypename = ape_object_value_typename(calleetype);
        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe), "%s object is not callable", calleetypename);
        return false;
    }
    return true;
}

bool ape_vm_callobjectstack(ApeVM_t* vm, ApeObject_t callee, ApeInt_t nargs)
{
    return ape_vm_callobjectargs(vm, callee, nargs, NULL);
}

bool ape_vm_checkassign(ApeVM_t* vm, ApeObject_t oldval, ApeObject_t newval)
{
    ApeObjType_t oldtype;
    ApeObjType_t newtype;
    (void)vm;
    oldtype = ape_object_value_type(oldval);
    newtype = ape_object_value_type(newval);
    if(oldtype == APE_OBJECT_NULL || newtype == APE_OBJECT_NULL)
    {
        return true;
    }
    if(oldtype != newtype)
    {
        /*
        * this is redundant, and frankly, unnecessary.
        */
        /*
        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe), "trying to assign variable of type %s to %s",
                          ape_object_value_typename(newtype), ape_object_value_typename(oldtype));
        return false;
        */
    }
    return true;
}

bool ape_vm_tryoverloadoperator(ApeVM_t* vm, ApeObject_t left, ApeObject_t right, ApeOpByte_t op, bool* out_overload_found)
{
    int numop;
    ApeObject_t key;
    ApeObject_t callee;
    ApeObjType_t lefttype;
    ApeObjType_t righttype;
    *out_overload_found = false;
    lefttype = ape_object_value_type(left);
    righttype = ape_object_value_type(right);
    if(lefttype != APE_OBJECT_MAP && righttype != APE_OBJECT_MAP)
    {
        *out_overload_found = false;
        return true;
    }
    numop = 2;
    if(op == APE_OPCODE_MINUS || op == APE_OPCODE_NOT)
    {
        numop = 1;
    }
    key = vm->overloadkeys[op];
    callee = ape_object_make_null(vm->context);
    if(lefttype == APE_OBJECT_MAP)
    {
        callee = ape_object_map_getvalueobject(left, key);
    }
    if(!ape_object_value_iscallable(callee))
    {
        if(righttype == APE_OBJECT_MAP)
        {
            callee = ape_object_map_getvalueobject(right, key);
        }
        if(!ape_object_value_iscallable(callee))
        {
            *out_overload_found = false;
            return true;
        }
    }
    *out_overload_found = true;
    ape_vm_pushstack(vm, callee);
    ape_vm_pushstack(vm, left);
    if(numop == 2)
    {
        ape_vm_pushstack(vm, right);
    }
    return ape_vm_callobjectstack(vm, callee, numop);
}


#define SET_OPERATOR_OVERLOAD_KEY(op, key)                   \
    do                                                       \
    {                                                        \
        key_obj = ape_object_make_string(ctx, key); \
        if(ape_object_value_isnull(key_obj))                          \
        {                                                    \
            goto err;                                        \
        }                                                    \
        vm->overloadkeys[op] = key_obj;             \
    } while(0)

ApeVM_t* ape_make_vm(ApeContext_t* ctx, const ApeConfig_t* config, ApeGCMemory_t* mem, ApeErrorList_t* errors, ApeGlobalStore_t* global_store)
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
    vm->config = config;
    vm->mem = mem;
    vm->errors = errors;
    vm->globalstore = global_store;
    vm->stackptr = 0;
    vm->thisptr = 0;
    vm->countframes = 0;
    vm->lastpopped = ape_object_make_null(ctx);
    vm->running = false;
    vm->globalobjects = ape_make_valdict(ctx, ApeSize_t, ApeObject_t);
    vm->stackobjects = ape_make_valdict(ctx, ApeSize_t, ApeObject_t);
    vm->lastframe = NULL;
    vm->frameobjects = deqlist_create_empty();
    for(i = 0; i < APE_OPCODE_MAX; i++)
    {
        vm->overloadkeys[i] = ape_object_make_null(ctx);
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
    #if 1
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_GETINDEX, "__getindex__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_SETINDEX, "__setindex__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_CALL, "__call__");
    #endif
    return vm;
err:
    ape_vm_destroy(vm);
    return NULL;
}
#undef SET_OPERATOR_OVERLOAD_KEY

void ape_vm_destroy(ApeVM_t* vm)
{
    ApeContext_t* ctx;
    ApeFrame_t* popped;
    if(!vm)
    {
        return;
    }
    ctx = vm->context;
    ape_valdict_destroy(vm->globalobjects);
    ape_valdict_destroy(vm->stackobjects);
    fprintf(stderr, "deqlist_count(vm->frameobjects)=%d\n", deqlist_count(vm->frameobjects));
    if(deqlist_count(vm->frameobjects) != 0)
    {
        while(deqlist_count(vm->frameobjects) != 0)
        {
            popped = (ApeFrame_t*)deqlist_get(vm->frameobjects, deqlist_count(vm->frameobjects)-1);
            if(popped != NULL)
            {
                //if(popped->allocated)
                {
                    ape_allocator_free(&vm->context->alloc, popped);
                }
            }
            deqlist_pop(&vm->frameobjects);
        }
    }
    deqlist_destroy(vm->frameobjects);
    ape_allocator_free(&ctx->alloc, vm);
}

void ape_vm_reset(ApeVM_t* vm)
{
    vm->stackptr = 0;
    vm->thisptr = 0;
    while(vm->countframes > 0)
    {
        ape_vm_popframe(vm);
    }
}


bool ape_frame_init(ApeFrame_t* frame, ApeObject_t function_obj, int bptr)
{
    ApeScriptFunction_t* function;
    if(ape_object_value_type(function_obj) != APE_OBJECT_SCRIPTFUNCTION)
    {
        return false;
    }
    function = ape_object_value_asfunction(function_obj);
    frame->function = function_obj;
    frame->ip = 0;
    frame->basepointer = bptr;
    frame->srcip = 0;
    frame->bytecode = function->compiledcode->bytecode;
    frame->srcpositions = function->compiledcode->srcpositions;
    frame->bcsize = function->compiledcode->count;
    frame->recoverip = -1;
    frame->isrecovering = false;
    return true;
}

/*
* this function updates an existing stack-alloc'd frame.
* since frames only hold primitive data (integrals and pointers), NO copying
* beyond primitives is done here.
* if a frame needs to copy something, do so in ape_frame_copyalloc.
*/
ApeFrame_t* ape_frame_update(ApeVM_t* vm, ApeFrame_t* rt, ApeFrame_t* from)
{
    (void)vm;
    rt->allocated = true;
    rt->function = from->function;
    rt->srcpositions = from->srcpositions;
    rt->bytecode = from->bytecode;
    rt->ip = from->ip;
    rt->basepointer = from->basepointer;
    rt->srcip = from->srcip;
    rt->bcsize = from->bcsize;
    rt->recoverip = from->recoverip;
    rt->isrecovering = from->isrecovering;
    return rt;
}

/*
* creates a new frame with data provided by an existing frame.
*/
ApeFrame_t* ape_frame_copyalloc(ApeVM_t* vm, ApeFrame_t* from)
{
    ApeFrame_t* rt;
    rt = (ApeFrame_t*)ape_allocator_alloc(&vm->context->alloc, sizeof(ApeFrame_t));
    ape_frame_update(vm, rt, from);
    return rt;
}

bool ape_vm_pushframe(ApeVM_t* vm, ApeFrame_t frame)
{
    ApeFrame_t* pf;
    ApeFrame_t* tmp;
    ApeScriptFunction_t* fn;
    /*
    * when a new frame exceeds existing the frame list, then
    * allocate a new one from $frame ...
    * ... otherwise, copy data from $frame, and stuff it back into the frame list.
    * this saves a considerable amount of memory by reusing existing memory.
    */
    if(vm->countframes >= deqlist_count(vm->frameobjects))
    {
        pf = ape_frame_copyalloc(vm, &frame);
        deqlist_push(&vm->frameobjects, pf);
    }
    else
    {
        tmp = (ApeFrame_t*)deqlist_get(vm->frameobjects, vm->countframes);
        pf = ape_frame_update(vm, tmp, &frame);
        deqlist_set(vm->frameobjects, vm->countframes, pf);
    }
    vm->lastframe = pf;
    vm->currentframe = (ApeFrame_t*)deqlist_get(vm->frameobjects, vm->countframes);
    vm->countframes++;
    fn = ape_object_value_asfunction(frame.function);
    ape_vm_setstackpointer(vm, frame.basepointer + fn->numlocals);
    return true;
}

bool ape_vm_popframe(ApeVM_t* vm)
{
    ApeFrame_t* popped;
    ape_vm_setstackpointer(vm, vm->currentframe->basepointer - 1);
    if(vm->countframes <= 0)
    {
        APE_ASSERT(false);
        vm->currentframe = NULL;
        return false;
    }
    vm->countframes--;
    if(vm->countframes == 0)
    {
        vm->currentframe = NULL;
        return false;
    }
    popped = (ApeFrame_t*)deqlist_get(vm->frameobjects, vm->countframes);
    /*
    * don't deallocate $popped here - they'll be deallocated in ape_vm_destroy.
    */
    vm->currentframe = (ApeFrame_t*)deqlist_get(vm->frameobjects, vm->countframes - 1);
    return true;
}

void ape_vm_collectgarbage(ApeVM_t* vm, ApeValArray_t* constants, bool alsostack)
{
    ApeSize_t i;
    ApeFrame_t* frame;
    ape_gcmem_unmarkall(vm->mem);
    ape_gcmem_markobjlist(ape_globalstore_getobjectdata(vm->globalstore), ape_globalstore_getobjectcount(vm->globalstore));
    if(constants != NULL)
    {
        ape_gcmem_markobjlist((ApeObject_t*)ape_valarray_data(constants), ape_valarray_count(constants));
    }
    ape_gcmem_markobjlist((ApeObject_t*)(vm->globalobjects->values), vm->globalobjects->count);
    for(i = 0; i < vm->countframes; i++)
    {
        frame = (ApeFrame_t*)deqlist_get(vm->frameobjects, i);
        ape_gcmem_markobject(frame->function);
    }
    if(alsostack)
    {
        ape_gcmem_markobjlist(((ApeObject_t*)(vm->stackobjects->values)), vm->stackptr);
        ape_gcmem_markobjlist(vm->thisobjects, vm->thisptr);
    }
    ape_gcmem_markobject(vm->lastpopped);
    ape_gcmem_markobjlist(vm->overloadkeys, APE_OPCODE_MAX);
    ape_gcmem_sweep(vm->mem);
}

bool ape_vm_run(ApeVM_t* vm, ApeAstCompResult_t* comp_res, ApeValArray_t * constants)
{
    bool res;
    int old_sp;
    int old_this_sp;
    ApeSize_t old_frames_count;
    ApeObject_t main_fn;
    (void)old_sp;
    old_sp = vm->stackptr;
    old_this_sp = vm->thisptr;
    old_frames_count = vm->countframes;
    main_fn = ape_object_make_function(vm->context, "__main__", comp_res, false, 0, 0, 0);
    if(ape_object_value_isnull(main_fn))
    {
        return false;
    }
    ape_vm_pushstack(vm, main_fn);
    res = ape_vm_executefunction(vm, main_fn, constants);
    while(vm->countframes > old_frames_count)
    {
        ape_vm_popframe(vm);
    }
    APE_ASSERT(vm->stackptr == old_sp);
    vm->thisptr = old_this_sp;
    return res;
}

ApeObject_t ape_object_string_copy(ApeContext_t* ctx, ApeObject_t obj)
{
    return ape_object_make_stringlen(ctx, ape_object_string_getdata(obj), ape_object_string_getlength(obj));
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
        /* avoid doing unnecessary copying by reusing the origin as-is */
        if(leftlen == 0)
        {
            //ape_vm_pushstack(vm, ape_object_string_copy(vm->context, right));
            ape_vm_pushstack(vm, right);
        }
        else if(rightlen == 0)
        {
            //ape_vm_pushstack(vm, ape_object_string_copy(vm->context, left));
            ape_vm_pushstack(vm, left);
        }
        else
        {
            leftstr = ape_object_string_getdata(left);
            rightstr = ape_object_string_getdata(right);
            #if 0
            ape_gcmem_markobject(left);
            ape_gcmem_markobject(right);
            #endif
            objres = ape_object_make_stringcapacity(vm->context, leftlen + rightlen);
            if(ape_object_value_isnull(objres))
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
        ape_writer_appendlen(allbuf, ape_object_string_getdata(left), ape_object_string_getlength(left));
        ape_tostring_object(tostrbuf, right, false);
        ape_writer_appendlen(allbuf, ape_writer_getdata(tostrbuf), ape_writer_getlength(tostrbuf));
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


bool ape_vm_getindex(ApeVM_t* vm, ApeObject_t left, ApeObject_t index, ApeObjType_t lefttype, ApeObjType_t indextype)
{
    bool canindex;
    unsigned long nhash;
    const char* str;
    const char* idxname;
    const char* indextypename;
    const char* lefttypename;
    ApeSize_t idxlen;
    ApeInt_t ix;
    ApeInt_t leftlen;
    ApeObject_t objres;
    (void)idxname;
    canindex = false;
    /*
    * todo: object method lookup could be implemented here
    */
    #if 1
    {
        ApeObject_t objfn;
        ApeObject_t objval;
        ApeObjMemberItem_t* afn;
        if(indextype == APE_OBJECT_STRING)
        {
            idxname = ape_object_string_getdata(index);
            idxlen = ape_object_string_getlength(index);
            nhash = ape_util_hashstring(idxname, idxlen);
            if((afn = builtin_get_object(vm->context, lefttype, idxname, nhash)) != NULL)
            {
                objval = ape_object_make_null(vm->context);
                if(afn->isfunction)
                {
                    /*
                    * "normal" functions are pushed as generic, run-of-the-mill functions, except
                    * that "this" is also pushed.
                    */
                    objfn = ape_object_make_nativefuncmemory(vm->context, idxname, afn->fn, NULL, 0);
                    ape_vm_pushthisstack(vm, left);
                    objval = objfn;
                }
                else
                {
                    /*
                    * "non" functions (pseudo functions) like "length" receive no arguments whatsover,
                    * only "this" is pushed.
                    */
                    ape_vm_pushthisstack(vm, left);
                    objval = afn->fn(vm, NULL, 0, NULL);
                }
                ape_vm_pushstack(vm, objval);
                return true;
            }
        }
    }
    #endif
    canindex = (
        (lefttype == APE_OBJECT_ARRAY) ||
        (lefttype == APE_OBJECT_MAP) ||
        (lefttype == APE_OBJECT_STRING)
    );
    if(!canindex)
    {
        lefttypename = ape_object_value_typename(lefttype);
        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe),
            "type %s is not indexable (in APE_OPCODE_GETINDEX)", lefttypename);
        return false;
    }
    objres = ape_object_make_null(vm->context);
    if(lefttype == APE_OBJECT_ARRAY)
    {
        if(indextype != APE_OBJECT_NUMBER)
        {
            lefttypename = ape_object_value_typename(lefttype);
            indextypename = ape_object_value_typename(indextype);
            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe),
                              "cannot index %s with %s", lefttypename, indextypename);
            return false;
        }
        ix = (int)ape_object_value_asnumber(index);
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
        ix = (int)ape_object_value_asnumber(index);
        if(ix >= 0 && ix < leftlen)
        {
            char res_str[2] = { str[ix], '\0' };
            objres = ape_object_make_string(vm->context, res_str);
        }
    }
    ape_vm_pushstack(vm, objres);
    return true;
}

bool ape_vm_math(ApeVM_t* vm, ApeObject_t left, ApeObject_t right, ApeOpcodeValue_t opcode)
{
    bool ok;
    bool overloadfound;
    const char* lefttypename;
    const char* righttypename;
    const char* opcode_name;
    ApeFloat_t bigres;
    ApeFloat_t leftval;
    ApeFloat_t rightval;
    ApeInt_t leftint;
    ApeInt_t rightint;
    ApeObjType_t lefttype;
    ApeObjType_t righttype;
    /* NULL to 0 coercion */
    if(ape_object_value_isnumeric(left) && ape_object_value_isnull(right))
    {
        right = ape_object_make_number(vm->context, 0);
    }
    if(ape_object_value_isnumeric(right) && ape_object_value_isnull(left))
    {
        left = ape_object_make_number(vm->context, 0);
    }
    lefttype = ape_object_value_type(left);
    righttype = ape_object_value_type(right);
    if(ape_object_value_isnumeric(left) && ape_object_value_isnumeric(right))
    {
        rightval = ape_object_value_asnumber(right);
        leftval = ape_object_value_asnumber(left);
        leftint = ape_util_numbertoint32(leftval);
        rightint = ape_util_numbertoint32(rightval);
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
                    #if 0
                    bigres = fmod(leftval, rightval);
                    #else
                    bigres = (leftint % rightint);
                    #endif
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
                    int uleft = ape_util_numbertoint32(leftval);
                    unsigned int uright = ape_util_numbertouint32(rightval);
                    bigres = (uleft << (uright & 0x1F));
                }
                break;
            case APE_OPCODE_RIGHTSHIFT:
                {
                    int uleft = ape_util_numbertoint32(leftval);
                    unsigned int uright = ape_util_numbertouint32(rightval);
                    bigres = (uleft >> (uright & 0x1F));
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
            return false;
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
            return false;
        }
        if(!overloadfound)
        {
            opcode_name = ape_vm_opcodename(opcode);
            lefttypename = ape_object_value_typename(lefttype);
            righttypename = ape_object_value_typename(righttype);
            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe),
                              "invalid operand types for %s, got %s and %s", opcode_name, lefttypename,
                              righttypename);
            return false;
        }
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
    const ApeScriptFunction_t* constfunction;
    const char* indextypename;
    const char* keytypename;
    const char* lefttypestring;
    const char* lefttypename;
    const char* operandtypestring;
    const char* righttypestring;
    const char* str;
    const char* type_name;
    int elapsed_ms;
    ApeSize_t i;
    int ix;
    int leftlen;
    int len;
    int recover_frame_ix;
    ApeInt_t ui;

    ApeUInt_t constant_ix;
    ApeUInt_t count;
    ApeUInt_t items_count;
    ApeUInt_t kvp_count;
    ApeUInt_t recip;
    ApeInt_t val;
    ApeInt_t pos;
    unsigned time_check_counter;
    unsigned time_check_interval;

    ApeUShort_t free_ix;
    ApeUShort_t nargs;
    ApeUShort_t num_free;
    ApeSize_t idx;
    ApeSize_t tmpi;
    ApeSize_t kvstart;
    ApeFloat_t comparison_res;
    ApeFloat_t maxexecms;
    ApeFloat_t valdouble;
    ApeObjType_t constant_type;
    ApeObjType_t indextype;
    ApeObjType_t keytype;
    ApeObjType_t lefttype;
    ApeObjType_t operand_type;
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
    ApeObject_t popped;
    ApeObject_t* stackvals;
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
        ape_errorlist_add(vm->errors, APE_ERROR_USER, g_vmpriv_srcposinvalid, "VM is already executing code");
        return false;
    }
    #endif
    /* naming is hard */
    function_function = ape_object_value_asfunction(function);
    ok = false;
    ok = ape_frame_init(&new_frame, function, vm->stackptr - function_function->numargs);
    if(!ok)
    {
        return false;
    }
    ok = ape_vm_pushframe(vm, new_frame);
    if(!ok)
    {
        ape_errorlist_add(vm->errors, APE_ERROR_USER, g_vmpriv_srcposinvalid, "pushing frame failed");
        return false;
    }
    vm->running = true;
    vm->lastpopped = ape_object_make_null(vm->context);
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
    while(vm->currentframe->ip < (ApeInt_t)vm->currentframe->bcsize)
    {
        opcode = ape_frame_readopcode(vm->currentframe);
        switch(opcode)
        {
            case APE_OPCODE_CONSTANT:
                {
                    constant_ix = ape_frame_readuint16(vm->currentframe);
                    constant = (ApeObject_t*)ape_valarray_get(constants, constant_ix);
                    if(!constant)
                    {
                        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe),
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
                    ok = ape_vm_math(vm, left, right, opcode);
                    if(!ok)
                    {
                        goto err;
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
                        #if 0
                        if(opcode == APE_OPCODE_COMPAREEQUAL)
                        #endif
                        {
                            objres = ape_object_make_number(vm->context, comparison_res);
                            ape_vm_pushstack(vm, objres);
                        }
                        else
                        {
                            righttypestring = ape_object_value_typename(ape_object_value_type(right));
                            lefttypestring = ape_object_value_typename(ape_object_value_type(left));
                            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe),
                                              "cannot compare %s and %s", lefttypestring, righttypestring);
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
                    comparison_res = ape_object_value_asnumber(value);
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
                    operand_type = ape_object_value_type(operand);
                    if(operand_type == APE_OBJECT_NUMBER)
                    {
                        val = ape_object_value_asnumber(operand);
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
                            operandtypestring = ape_object_value_typename(operand_type);
                            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe),
                                              "invalid operand type for MINUS, got %s", operandtypestring);
                            goto err;
                        }
                    }
                }
                break;
            case APE_OPCODE_NOT:
                {
                    operand = ape_vm_popstack(vm);
                    type = ape_object_value_type(operand);
                    if(type == APE_OBJECT_BOOL)
                    {
                        objres = ape_object_make_bool(vm->context, !ape_object_value_asbool(operand));
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
                    pos = ape_frame_readuint16(vm->currentframe);
                    vm->currentframe->ip = pos;
                }
                break;
            case APE_OPCODE_JUMPIFFALSE:
                {
                    pos = ape_frame_readuint16(vm->currentframe);
                    testobj = ape_vm_popstack(vm);
                    if(!ape_object_value_asbool(testobj))
                    {
                        vm->currentframe->ip = pos;
                    }
                }
                break;
            case APE_OPCODE_JUMPIFTRUE:
                {
                    pos = ape_frame_readuint16(vm->currentframe);
                    testobj = ape_vm_popstack(vm);
                    if(ape_object_value_asbool(testobj))
                    {
                        vm->currentframe->ip = pos;
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
                    ix = ape_frame_readuint16(vm->currentframe);
                    objval = ape_vm_popstack(vm);
                    ape_vm_setglobal(vm, ix, objval);
                }
                break;
            case APE_OPCODE_SETMODULEGLOBAL:
                {
                    ix = ape_frame_readuint16(vm->currentframe);
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
                    ix = ape_frame_readuint16(vm->currentframe);
                    global = ape_vm_getglobal(vm, ix);
                    ape_vm_pushstack(vm, global);
                }
                break;
            case APE_OPCODE_MKARRAY:
                {
                    count = ape_frame_readuint16(vm->currentframe);
                    array_obj = ape_object_make_arraycapacity(vm->context, count);
                    if(ape_object_value_isnull(array_obj))
                    {
                        goto err;
                    }
                    idx = vm->stackptr;
                    items = ((ApeObject_t*)(vm->stackobjects->values)) + idx - count;
                    for(i = 0; i < count; i++)
                    {
                        ApeObject_t item = items[i];
                        ok = ape_object_array_pushvalue(array_obj, item);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                    ape_vm_setstackpointer(vm, vm->stackptr - count);
                    ape_vm_pushstack(vm, array_obj);
                }
                break;
            case APE_OPCODE_MAPSTART:
                {
                    count = ape_frame_readuint16(vm->currentframe);
                    map_obj = ape_object_make_mapcapacity(vm->context, count);
                    if(ape_object_value_isnull(map_obj))
                    {
                        goto err;
                    }
                    ape_vm_pushthisstack(vm, map_obj);
                }
                break;
            case APE_OPCODE_MAPEND:
                {
                    kvp_count = ape_frame_readuint16(vm->currentframe);
                    items_count = kvp_count * 2;
                    map_obj = ape_vm_popthisstack(vm);
                    stackvals = ((ApeObject_t*)(vm->stackobjects->values));
                    idx = vm->stackptr;
                    /*
                    * previously, extracting key->value pairs was as straight-forward
                    * as stackobjects[N] for the key and stackobjects[N+1] for the value.
                    * with stackobjects being a dict, their locations (indices) are
                    * the keys in stackobjects, and kvstart is the offset in which
                    * the active opcode operates in.
                    * thus, simplified:
                    *   key = stackobjects[(((stackposition - length(items)) + index) + 0)]
                    *   val = stackobjects[(((stackposition - length(items)) + index) + 1)]
                    *
                    * this also means that lookups aren't *quite* linear anymore.
                    * in terms of performance, the difference is negligible.
                    */
                    kvstart = (idx - items_count);
                    kv_pairs = stackvals + kvstart;
                    for(i = 0; i < items_count; i += 2)
                    {
                        tmpi = kvstart+i;
                        key = *(ApeObject_t*)ape_valdict_getbykey(vm->stackobjects, &tmpi);
                        /*
                        * NB. this only goes for literals.
                        * maps can have anything as a key, i.e.:
                        *   foo = {}
                        *   foo[function(){}] = "bar"
                        * would be perfectly valid.
                        */
                        if(!ape_object_value_ishashable(key))
                        {
                            keytype = ape_object_value_type(key);
                            keytypename = ape_object_value_typename(keytype);
                            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe),
                                              "key of type %s is not hashable", keytypename);
                            goto err;
                        }
                        tmpi = kvstart+i+1;
                        objval = *(ApeObject_t*)ape_valdict_getbykey(vm->stackobjects, &tmpi);
                        ok = ape_object_map_setvalue(map_obj, key, objval);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                    ape_vm_setstackpointer(vm, vm->stackptr - items_count);
                    ape_vm_pushstack(vm, map_obj);
                }
                break;
            case APE_OPCODE_GETTHIS:
                {
                    objval = ape_vm_getthisstack(vm, 0);
                    #if 0
                    objval = ape_vm_popthisstack(vm);
                    #endif
                    ape_vm_pushstack(vm, objval);
                }
                break;
            case APE_OPCODE_GETINDEX:
                {
                    index = ape_vm_popstack(vm);
                    left = ape_vm_popstack(vm);
                    lefttype = ape_object_value_type(left);
                    indextype = ape_object_value_type(index);
                    ok = ape_vm_getindex(vm, left, index, lefttype, indextype);
                    if(!ok)
                    {
                        goto err;
                    }
                }
                break;
            case APE_OPCODE_GETVALUEAT:
                {
                    index = ape_vm_popstack(vm);
                    left = ape_vm_popstack(vm);
                    lefttype = ape_object_value_type(left);
                    indextype = ape_object_value_type(index);
                    if(lefttype != APE_OBJECT_ARRAY && lefttype != APE_OBJECT_MAP && lefttype != APE_OBJECT_STRING)
                    {
                        lefttypename = ape_object_value_typename(lefttype);
                        indextypename = ape_object_value_typename(indextype);
                        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe),
                                          "type %s is not indexable (in APE_OPCODE_GETVALUEAT)", lefttypename);
                        goto err;
                    }
                    objres = ape_object_make_null(vm->context);
                    if(indextype != APE_OBJECT_NUMBER)
                    {
                        lefttypename = ape_object_value_typename(lefttype);
                        indextypename = ape_object_value_typename(indextype);
                        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe),
                                          "cannot index %s with %s", lefttypename, indextypename);
                        goto err;
                    }
                    ix = (int)ape_object_value_asnumber(index);
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
                        ix = (int)ape_object_value_asnumber(index);
                        if(ix >= 0 && ix < leftlen)
                        {
                            char res_str[2] = { str[ix], '\0' };
                            objres = ape_object_make_string(vm->context, res_str);
                        }
                    }
                    ape_vm_pushstack(vm, objres);
                }
                break;
            case APE_OPCODE_CALL:
                {
                    nargs = ape_frame_readuint8(vm->currentframe);
                    callee = ape_vm_getstack(vm, nargs);
                    ok = ape_vm_callobjectstack(vm, callee, nargs);
                    if(!ok)
                    {
                        goto err;
                    }
                }
                break;
            case APE_OPCODE_RETURNVALUE:
                {
                    objres = ape_vm_popstack(vm);
                    ok = ape_vm_popframe(vm);
                    if(!ok)
                    {
                        goto end;
                    }
                    ape_vm_pushstack(vm, objres);
                }
                break;
            case APE_OPCODE_RETURNNOTHING:
                {
                    ok = ape_vm_popframe(vm);
                    ape_vm_pushstack(vm, ape_object_make_null(vm->context));
                    if(!ok)
                    {
                        ape_vm_popstack(vm);
                        goto end;
                    }
                }
                break;
            case APE_OPCODE_DEFLOCAL:
                {
                    pos = ape_frame_readuint8(vm->currentframe);
                    popped = ape_vm_popstack(vm);
                    idx = vm->currentframe->basepointer + pos; 
                    ape_valdict_set(vm->stackobjects, &idx, &popped);
                }
                break;
            case APE_OPCODE_SETLOCAL:
                {
                    pos = ape_frame_readuint8(vm->currentframe);
                    new_value = ape_vm_popstack(vm);
                    idx = vm->currentframe->basepointer + pos;
                    old_value = *(ApeObject_t*)ape_valdict_getbykey(vm->stackobjects, &idx);
                    if(!ape_vm_checkassign(vm, old_value, new_value))
                    {
                        goto err;
                    }
                    idx = vm->currentframe->basepointer + pos;
                    ape_valdict_set(vm->stackobjects, &idx, &new_value);
                }
                break;
            case APE_OPCODE_GETLOCAL:
                {
                    pos = ape_frame_readuint8(vm->currentframe);
                    idx = vm->currentframe->basepointer + pos;
                    objval = *(ApeObject_t*)ape_valdict_getbykey(vm->stackobjects, &idx);
                    ape_vm_pushstack(vm, objval);
                }
                break;
            case APE_OPCODE_GETCONTEXTGLOBAL:
                {
                    ix = ape_frame_readuint16(vm->currentframe);
                    ok = false;
                    objval = ape_globalstore_getat(vm->globalstore, ix, &ok);
                    if(!ok)
                    {
                        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe),
                                          "global value %d not found", ix);
                        goto err;
                    }
                    ape_vm_pushstack(vm, objval);
                }
                break;
            case APE_OPCODE_MKFUNCTION:
                {
                    constant_ix = ape_frame_readuint16(vm->currentframe);
                    num_free = ape_frame_readuint8(vm->currentframe);
                    constant = (ApeObject_t*)ape_valarray_get(constants, constant_ix);
                    if(!constant)
                    {
                        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe),
                                          "constant %d not found", constant_ix);
                        goto err;
                    }
                    constant_type = ape_object_value_type(*constant);
                    if(constant_type != APE_OBJECT_SCRIPTFUNCTION)
                    {
                        type_name = ape_object_value_typename(constant_type);
                        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe),
                                          "%s is not a function", type_name);
                        goto err;
                    }
                    constfunction = ape_object_value_asfunction(*constant);
                    function_obj = ape_object_make_function(vm->context, ape_object_function_getname(*constant), constfunction->compiledcode,
                                           false, constfunction->numlocals, constfunction->numargs, num_free);
                    if(ape_object_value_isnull(function_obj))
                    {
                        goto err;
                    }
                    for(i = 0; i < num_free; i++)
                    {
                        idx = vm->stackptr - num_free + i;
                        free_val = *(ApeObject_t*)ape_valdict_getbykey(vm->stackobjects, &idx);
                        ape_object_function_setfreeval(function_obj, i, free_val);
                    }
                    ape_vm_setstackpointer(vm, vm->stackptr - num_free);
                    ape_vm_pushstack(vm, function_obj);
                }
                break;
            case APE_OPCODE_GETFREE:
                {
                    free_ix = ape_frame_readuint8(vm->currentframe);
                    objval = ape_object_function_getfreeval(vm->currentframe->function, free_ix);
                    ape_vm_pushstack(vm, objval);
                }
                break;
            case APE_OPCODE_SETFREE:
                {
                    free_ix = ape_frame_readuint8(vm->currentframe);
                    objval = ape_vm_popstack(vm);
                    ape_object_function_setfreeval(vm->currentframe->function, free_ix, objval);
                }
                break;
            case APE_OPCODE_CURRENTFUNCTION:
                {
                    current_function = vm->currentframe->function;
                    ape_vm_pushstack(vm, current_function);
                }
                break;
            case APE_OPCODE_SETINDEX:
                {
                    index = ape_vm_popstack(vm);
                    left = ape_vm_popstack(vm);
                    new_value = ape_vm_popstack(vm);
                    lefttype = ape_object_value_type(left);
                    indextype = ape_object_value_type(index);
                    if(lefttype != APE_OBJECT_ARRAY && lefttype != APE_OBJECT_MAP)
                    {
                        lefttypename = ape_object_value_typename(lefttype);
                        indextypename = ape_object_value_typename(indextype);
                        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe),
                                          "type %s is not indexable (in APE_OPCODE_SETINDEX)", lefttypename);
                        goto err;
                    }
                    if(lefttype == APE_OBJECT_ARRAY)
                    {
                        if(indextype != APE_OBJECT_NUMBER)
                        {
                            lefttypename = ape_object_value_typename(lefttype);
                            indextypename = ape_object_value_typename(indextype);
                            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe),
                                              "cannot index %s with %s", lefttypename, indextypename);
                            goto err;
                        }
                        ix = (int)ape_object_value_asnumber(index);
                        ok = ape_object_array_setat(left, ix, new_value);
                        if(!ok)
                        {
                            lefttypename = ape_object_value_typename(lefttype);
                            indextypename = ape_object_value_typename(indextype);
                            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe),
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
                    type = ape_object_value_type(objval);
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
                        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe),
                                          "cannot get length of %s", type_name);
                        goto err;
                    }
                    ape_vm_pushstack(vm, ape_object_make_number(vm->context, len));
                }
                break;
            case APE_OPCODE_MKNUMBER:
                {
                    /* FIXME: why does ape_util_uinttofloat break things here? */
                    val = ape_frame_readuint64(vm->currentframe);
                    #if 0
                    valdouble = ape_util_uinttofloat(val);
                    #else
                    valdouble = val;
                    #endif
                    objval = ape_object_make_number(vm->context, valdouble);
                    //objval.handle->datatype = APE_OBJECT_NUMBER;
                    ape_vm_pushstack(vm, objval);
                }
                break;
            case APE_OPCODE_SETRECOVER:
                {
                    recip = ape_frame_readuint16(vm->currentframe);
                    vm->currentframe->recoverip = recip;
                }
                break;
            case APE_OPCODE_IMPORT:
                {
                    
                }
                break;
            default:
                {
                    APE_ASSERT(false);
                    ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe), "unknown opcode: 0x%x", opcode);
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
                    ape_errorlist_addformat(vm->errors, APE_ERROR_TIMEOUT, ape_frame_srcposition(vm->currentframe),
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
            if(err->errtype == APE_ERROR_RUNTIME && ape_errorlist_count(vm->errors) == 1)
            {
                recover_frame_ix = -1;
                for(ui = vm->countframes - 1; ui >= 0; ui--)
                {
                    frame = (ApeFrame_t*)deqlist_get(vm->frameobjects, ui);
                    if(frame->recoverip >= 0 && !frame->isrecovering)
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
                    while((ApeInt_t)vm->countframes > (ApeInt_t)(recover_frame_ix + 1))
                    {
                        ape_vm_popframe(vm);
                    }
                    err_obj = ape_object_make_error(vm->context, err->message);
                    if(!ape_object_value_isnull(err_obj))
                    {
                        ape_object_value_seterrortraceback(err_obj, err->traceback);
                        err->traceback = NULL;
                    }
                    ape_vm_pushstack(vm, err_obj);
                    vm->currentframe->ip = vm->currentframe->recoverip;
                    vm->currentframe->isrecovering = true;
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
            ape_vm_collectgarbage(vm, constants, true);
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
    ape_vm_collectgarbage(vm, constants, true);
    vm->running = false;
    return ape_errorlist_count(vm->errors) == 0;
}
