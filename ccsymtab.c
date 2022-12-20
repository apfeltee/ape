
#include "inline.h"

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

void* ape_symbol_destroy(ApeContext_t* ctx, ApeSymbol_t* symbol)
{
    if(!symbol)
    {
        return NULL;
    }
    ape_allocator_free(&ctx->alloc, symbol->name);
    ape_allocator_free(&ctx->alloc, symbol);
    return NULL;
}

ApeSymbol_t* ape_symbol_copy(ApeContext_t* ctx, ApeSymbol_t* symbol)
{
    return ape_make_symbol(ctx, symbol->name, symbol->symtype, symbol->index, symbol->assignable);
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
    ctx = table->context;
    while(ape_ptrarray_count(table->blockscopes) > 0)
    {
        ape_symtable_popblockscope(table);
    }
    ape_ptrarray_destroy(table->blockscopes);
    ape_ptrarray_destroywithitems(ctx, table->modglobalsymbols, (ApeDataCallback_t)ape_symbol_destroy);
    ape_ptrarray_destroywithitems(ctx, table->freesymbols, (ApeDataCallback_t)ape_symbol_destroy);
    memset(table, 0, sizeof(ApeSymTable_t));
    ape_allocator_free(&ctx->alloc, table);
}

ApeSymTable_t* ape_symtable_copy(ApeContext_t* ctx, ApeSymTable_t* table)
{
    ApeSymTable_t* copy;
    copy = (ApeSymTable_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeSymTable_t));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(ApeSymTable_t));
    copy->context = table->context;
    copy->outer = table->outer;
    copy->globalstore = table->globalstore;
    copy->blockscopes = ape_ptrarray_copywithitems(ctx, table->blockscopes, (ApeDataCallback_t)ape_blockscope_copy, (ApeDataCallback_t)ape_blockscope_destroy);
    //copy->blockscopes = ape_ptrarray_copywithitems(ctx, table->blockscopes, NULL, NULL);
    if(!copy->blockscopes)
    {
        goto err;
    }
    copy->freesymbols = ape_ptrarray_copywithitems(ctx, table->freesymbols, (ApeDataCallback_t)ape_symbol_copy, (ApeDataCallback_t)ape_symbol_destroy);
    if(!copy->freesymbols)
    {
        goto err;
    }
    copy->modglobalsymbols = ape_ptrarray_copywithitems(ctx, table->modglobalsymbols, (ApeDataCallback_t)ape_symbol_copy, (ApeDataCallback_t)ape_symbol_destroy);
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


bool ape_symtable_setsymbol(ApeSymTable_t* table, ApeSymbol_t* symbol)
{
    ApeAstBlockScope_t* topscope;
    ApeSymbol_t* existing;
    topscope = (ApeAstBlockScope_t*)ape_ptrarray_top(table->blockscopes);
    if(topscope != NULL)
    {
        if(topscope->store)
        {
            existing= (ApeSymbol_t*)ape_strdict_getbyname(topscope->store, symbol->name);
            if(existing)
            {
                ape_symbol_destroy(table->context, existing);
            }
            return ape_strdict_set(topscope->store, symbol->name, symbol);
        }
    }
    return false;
}

int ape_symtable_nextsymbolindex(ApeSymTable_t* table)
{
    int ix;
    ApeAstBlockScope_t* topscope;
    topscope = (ApeAstBlockScope_t*)ape_ptrarray_top(table->blockscopes);
    ix = topscope->offset + topscope->numdefinitions;
    return ix;
}

int ape_symtable_count(ApeSymTable_t* table)
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
    ApeSymbol_t* copy = ape_symbol_copy(st->context, symbol);
    if(!copy)
    {
        return false;
    }
    ok = ape_symtable_setsymbol(st, copy);
    if(!ok)
    {
        ape_symbol_destroy(st->context, copy);
        return false;
    }
    return true;
}

ApeSymbol_t* ape_symtable_define(ApeSymTable_t* table, const char* name, bool assignable)
{    
    bool ok;
    bool gsadded;
    int ix;
    ApeSize_t defcnt;
    ApeAstBlockScope_t* topscope;
    ApeSymbolType_t symtype;
    ApeSymbol_t* symbol;
    ApeSymbol_t* gscopy;
    const ApeSymbol_t* glsym;
    glsym = ape_globalstore_getsymbol(table->globalstore, name);
    if(glsym)
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
    symtype = table->outer == NULL ? APE_SYMBOL_MODULEGLOBAL : APE_SYMBOL_LOCAL;
    ix = ape_symtable_nextsymbolindex(table);
    symbol = ape_make_symbol(table->context, name, symtype, ix, assignable);
    if(!symbol)
    {
        return NULL;
    }
    gsadded = false;
    ok = false;
    if(symtype == APE_SYMBOL_MODULEGLOBAL && ape_ptrarray_count(table->blockscopes) == 1)
    {
        gscopy = ape_symbol_copy(table->context, symbol);
        if(!gscopy)
        {
            ape_symbol_destroy(table->context, symbol);
            return NULL;
        }
        ok = ape_ptrarray_push(table->modglobalsymbols, &gscopy);
        if(!ok)
        {
            ape_symbol_destroy(table->context, gscopy);
            ape_symbol_destroy(table->context, symbol);
            return NULL;
        }
        gsadded = true;
    }
    ok = ape_symtable_setsymbol(table, symbol);
    if(!ok)
    {
        if(gsadded)
        {
            gscopy = (ApeSymbol_t*)ape_ptrarray_pop(table->modglobalsymbols);
            ape_symbol_destroy(table->context, gscopy);
        }
        ape_symbol_destroy(table->context, symbol);
        return NULL;
    }
    topscope = (ApeAstBlockScope_t*)ape_ptrarray_top(table->blockscopes);
    topscope->numdefinitions++;
    defcnt = ape_symtable_count(table);
    if(defcnt > table->maxnumdefinitions)
    {
        table->maxnumdefinitions = defcnt;
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
    ok = ape_ptrarray_push(st->freesymbols, &copy);
    if(!ok)
    {
        ape_symbol_destroy(st->context, copy);
        return NULL;
    }

    symbol = ape_make_symbol(st->context, original->name, APE_SYMBOL_FREE, ape_ptrarray_count(st->freesymbols) - 1, original->assignable);
    if(!symbol)
    {
        return NULL;
    }

    ok = ape_symtable_setsymbol(st, symbol);
    if(!ok)
    {
        ape_symbol_destroy(st->context, symbol);
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
    ok = ape_symtable_setsymbol(st, symbol);
    if(!ok)
    {
        ape_symbol_destroy(st->context, symbol);
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
    ok = ape_symtable_setsymbol(st, symbol);
    if(!ok)
    {
        ape_symbol_destroy(st->context, symbol);
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
    ApeAstBlockScope_t* topscope;
    ApeSymbol_t* symbol;
    /* todo: rename to something more obvious */
    symbol = ape_globalstore_getsymbol(table->globalstore, name);
    if(symbol)
    {
        return true;
    }
    topscope = (ApeAstBlockScope_t*)ape_ptrarray_top(table->blockscopes);
    symbol = (ApeSymbol_t*)ape_strdict_getbyname(topscope->store, name);
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
    ok = ape_ptrarray_push(table->blockscopes, &new_scope);
    if(!ok)
    {
        ape_blockscope_destroy(table->context, new_scope);
        return false;
    }
    return true;
}

void ape_symtable_popblockscope(ApeSymTable_t* table)
{
    ApeAstBlockScope_t* topscope;
    topscope = (ApeAstBlockScope_t*)ape_ptrarray_top(table->blockscopes);
    ape_ptrarray_pop(table->blockscopes);
    ape_blockscope_destroy(table->context, topscope);
}

ApeAstBlockScope_t* ape_symtable_getblockscope(ApeSymTable_t* table)
{
    ApeAstBlockScope_t* topscope;
    topscope = (ApeAstBlockScope_t*)ape_ptrarray_top(table->blockscopes);
    return topscope;
}

bool ape_symtable_ismoduleglobalscope(ApeSymTable_t* table)
{
    if(table == NULL)
    {
        return false;
    }
    return table->outer == NULL;
}

bool ape_symtable_istopblockscope(ApeSymTable_t* table)
{
    return ape_ptrarray_count(table->blockscopes) == 1;
}

bool ape_symtable_istopglobalscope(ApeSymTable_t* table)
{
    if(table != NULL)
    {
        if(ape_symtable_ismoduleglobalscope(table) && ape_symtable_istopblockscope(table))
        {
            return true;
        }
    }
    return false;
}

ApeSize_t ape_symtable_getmoduleglobalsymbolcount(const ApeSymTable_t* table)
{
    return ape_ptrarray_count(table->modglobalsymbols);
}

const ApeSymbol_t* ape_symtable_getmoduleglobalsymbolat(const ApeSymTable_t* table, int ix)
{
    return (ApeSymbol_t*)ape_ptrarray_get(table->modglobalsymbols, ix);
}
