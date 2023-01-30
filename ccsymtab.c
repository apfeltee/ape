
#include "inline.h"

ApeSymbol* ape_make_symbol(ApeContext* ctx, const char* name, ApeSymbolType type, ApeSize index, bool assignable)
{
    ApeSymbol* symbol;
    symbol = (ApeSymbol*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeSymbol));
    if(!symbol)
    {
        return NULL;
    }
    memset(symbol, 0, sizeof(ApeSymbol));
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

void* ape_symbol_destroy(ApeContext* ctx, ApeSymbol* symbol)
{
    if(!symbol)
    {
        return NULL;
    }
    ape_allocator_free(&ctx->alloc, symbol->name);
    ape_allocator_free(&ctx->alloc, symbol);
    return NULL;
}

ApeSymbol* ape_symbol_copy(ApeContext* ctx, ApeSymbol* symbol)
{
    return ape_make_symbol(ctx, symbol->name, symbol->symtype, symbol->index, symbol->assignable);
}

ApeSymTable* ape_make_symtable(ApeContext* ctx, ApeSymTable* outer, ApeGlobalStore* global_store, int mgo)
{
    bool ok;
    ApeSymTable* table;
    table = (ApeSymTable*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeSymTable));
    if(!table)
    {
        return NULL;
    }
    memset(table, 0, sizeof(ApeSymTable));
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

void ape_symtable_destroy(ApeSymTable* table)
{
    ApeContext* ctx;
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
    ape_ptrarray_destroywithitems(ctx, table->modglobalsymbols, (ApeDataCallback)ape_symbol_destroy);
    ape_ptrarray_destroywithitems(ctx, table->freesymbols, (ApeDataCallback)ape_symbol_destroy);
    memset(table, 0, sizeof(ApeSymTable));
    ape_allocator_free(&ctx->alloc, table);
}

ApeSymTable* ape_symtable_copy(ApeContext* ctx, ApeSymTable* table)
{
    ApeSymTable* copy;

    copy = (ApeSymTable*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeSymTable));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(ApeSymTable));
    copy->context = ctx;
    copy->outer = NULL;
    copy->globalstore = NULL;
    copy->blockscopes = NULL;
    copy->freesymbols = NULL;
    if(table != NULL)
    {
        copy->outer = table->outer;
        copy->globalstore = table->globalstore;
        copy->blockscopes = ape_ptrarray_copywithitems(ctx, table->blockscopes, (ApeDataCallback)ape_blockscope_copy, (ApeDataCallback)ape_blockscope_destroy);
        //copy->blockscopes = ape_ptrarray_copywithitems(ctx, table->blockscopes, NULL, NULL);
        if(!copy->blockscopes)
        {
            goto err;
        }
        copy->freesymbols = ape_ptrarray_copywithitems(ctx, table->freesymbols, (ApeDataCallback)ape_symbol_copy, (ApeDataCallback)ape_symbol_destroy);
        if(!copy->freesymbols)
        {
            goto err;
        }
        copy->modglobalsymbols = ape_ptrarray_copywithitems(ctx, table->modglobalsymbols, (ApeDataCallback)ape_symbol_copy, (ApeDataCallback)ape_symbol_destroy);
        if(!copy->modglobalsymbols)
        {
            goto err;
        }
        copy->maxnumdefinitions = table->maxnumdefinitions;
        copy->modglobaloffset = table->modglobaloffset;
    }
    return copy;
err:
    ape_symtable_destroy(copy);
    return NULL;
}


bool ape_symtable_setsymbol(ApeSymTable* table, ApeSymbol* symbol)
{
    ApeAstBlockScope* topscope;
    ApeSymbol* existing;
    topscope = (ApeAstBlockScope*)ape_ptrarray_top(table->blockscopes);
    if(topscope != NULL)
    {
        if(topscope->store)
        {
            existing= (ApeSymbol*)ape_strdict_getbyname(topscope->store, symbol->name);
            if(existing)
            {
                ape_symbol_destroy(table->context, existing);
            }
            return ape_strdict_set(topscope->store, symbol->name, symbol);
        }
    }
    return false;
}

int ape_symtable_nextsymbolindex(ApeSymTable* table)
{
    int ix;
    ApeAstBlockScope* topscope;
    topscope = (ApeAstBlockScope*)ape_ptrarray_top(table->blockscopes);
    ix = topscope->offset + topscope->numdefinitions;
    return ix;
}

int ape_symtable_count(ApeSymTable* table)
{
    ApeInt i;
    int count;
    ApeAstBlockScope* scope;
    count = 0;
    for(i = (ApeInt)ape_ptrarray_count(table->blockscopes) - 1; i >= 0; i--)
    {
        scope = (ApeAstBlockScope*)ape_ptrarray_get(table->blockscopes, i);
        count += scope->numdefinitions;
    }
    return count;
}


bool ape_symtable_addmodulesymbol(ApeSymTable* st, ApeSymbol* symbol)
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
    ApeSymbol* copy = ape_symbol_copy(st->context, symbol);
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

ApeSymbol* ape_symtable_define(ApeSymTable* table, const char* name, bool assignable)
{    
    bool ok;
    bool gsadded;
    int ix;
    ApeSize defcnt;
    ApeContext* ctx;
    ApeAstBlockScope* topscope;
    ApeSymbolType symtype;
    ApeSymbol* symbol;
    ApeSymbol* gscopy;
    const ApeSymbol* glsym;
    ctx = table->context;
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
    symbol = ape_make_symbol(ctx, name, symtype, ix, assignable);
    if(!symbol)
    {
        return NULL;
    }
    gsadded = false;
    ok = false;
    if(symtype == APE_SYMBOL_MODULEGLOBAL && ape_ptrarray_count(table->blockscopes) == 1)
    {
        gscopy = ape_symbol_copy(ctx, symbol);
        if(!gscopy)
        {
            ape_symbol_destroy(ctx, symbol);
            return NULL;
        }
        ok = ape_ptrarray_push(table->modglobalsymbols, &gscopy);
        if(!ok)
        {
            ape_symbol_destroy(ctx, gscopy);
            ape_symbol_destroy(ctx, symbol);
            return NULL;
        }
        gsadded = true;
    }
    ok = ape_symtable_setsymbol(table, symbol);
    if(!ok)
    {
        if(gsadded)
        {
            gscopy = (ApeSymbol*)ape_ptrarray_pop(table->modglobalsymbols);
            ape_symbol_destroy(ctx, gscopy);
        }
        ape_symbol_destroy(ctx, symbol);
        return NULL;
    }
    topscope = (ApeAstBlockScope*)ape_ptrarray_top(table->blockscopes);
    topscope->numdefinitions++;
    defcnt = ape_symtable_count(table);
    if(defcnt > table->maxnumdefinitions)
    {
        table->maxnumdefinitions = defcnt;
    }

    return symbol;
}

ApeSymbol* ape_symtable_deffree(ApeSymTable* st, ApeSymbol* original)
{
    bool ok;
    ApeSymbol* symbol;
    ApeSymbol* copy;
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

ApeSymbol* ape_symtable_definefuncname(ApeSymTable* st, const char* name, bool assignable)
{
    bool ok;
    ApeSymbol* symbol;
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

ApeSymbol* ape_symtable_definethis(ApeSymTable* st)
{
    bool ok;
    ApeSymbol* symbol;
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

ApeSymbol* ape_symtable_resolve(ApeSymTable* table, const char* name)
{
    ApeInt i;
    ApeSymbol* symbol;
    ApeAstBlockScope* scope;
    scope = NULL;
    symbol = ape_globalstore_getsymbol(table->globalstore, name);
    if(symbol)
    {
        return symbol;
    }
    for(i = (ApeInt)ape_ptrarray_count(table->blockscopes) - 1; i >= 0; i--)
    {
        scope = (ApeAstBlockScope*)ape_ptrarray_get(table->blockscopes, i);
        symbol = (ApeSymbol*)ape_strdict_getbyname(scope->store, name);
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

bool ape_symtable_symbol_is_defined(ApeSymTable* table, const char* name)
{
    ApeAstBlockScope* topscope;
    ApeSymbol* symbol;
    /* todo: rename to something more obvious */
    symbol = ape_globalstore_getsymbol(table->globalstore, name);
    if(symbol)
    {
        return true;
    }
    topscope = (ApeAstBlockScope*)ape_ptrarray_top(table->blockscopes);
    symbol = (ApeSymbol*)ape_strdict_getbyname(topscope->store, name);
    if(symbol)
    {
        return true;
    }
    return false;
}

bool ape_symtable_pushblockscope(ApeSymTable* table)
{
    bool ok;
    int block_scope_offset;
    ApeAstBlockScope* prev_block_scope;
    ApeAstBlockScope* new_scope;
    block_scope_offset = 0;
    prev_block_scope = (ApeAstBlockScope*)ape_ptrarray_top(table->blockscopes);
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

void ape_symtable_popblockscope(ApeSymTable* table)
{
    ApeAstBlockScope* topscope;
    topscope = (ApeAstBlockScope*)ape_ptrarray_top(table->blockscopes);
    ape_ptrarray_pop(table->blockscopes);
    ape_blockscope_destroy(table->context, topscope);
}

ApeAstBlockScope* ape_symtable_getblockscope(ApeSymTable* table)
{
    ApeAstBlockScope* topscope;
    if(table == NULL)
    {
        return NULL;
    }
    topscope = (ApeAstBlockScope*)ape_ptrarray_top(table->blockscopes);
    return topscope;
}

bool ape_symtable_ismoduleglobalscope(ApeSymTable* table)
{
    if(table == NULL)
    {
        return false;
    }
    return table->outer == NULL;
}

bool ape_symtable_istopblockscope(ApeSymTable* table)
{
    return ape_ptrarray_count(table->blockscopes) == 1;
}

bool ape_symtable_istopglobalscope(ApeSymTable* table)
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

ApeSize ape_symtable_getmoduleglobalsymbolcount(const ApeSymTable* table)
{
    return ape_ptrarray_count(table->modglobalsymbols);
}

const ApeSymbol* ape_symtable_getmoduleglobalsymbolat(const ApeSymTable* table, int ix)
{
    return (ApeSymbol*)ape_ptrarray_get(table->modglobalsymbols, ix);
}
