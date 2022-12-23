
#include "inline.h"

ApeAstCompFile_t* ape_make_compfile(ApeContext_t* ctx, const char* path)
{
    size_t len;
    const char* last_slash_pos;
    ApeAstCompFile_t* file;
    file = (ApeAstCompFile_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstCompFile_t));
    if(!file)
    {
        return NULL;
    }
    memset(file, 0, sizeof(ApeAstCompFile_t));
    file->context = ctx;
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
    ape_compfile_destroy(ctx, file);
    return NULL;
}

void* ape_compfile_destroy(ApeContext_t* ctx, ApeAstCompFile_t* file)
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
        ape_allocator_free(&ctx->alloc, item);
    }
    ape_ptrarray_destroy(file->lines);
    ape_allocator_free(&ctx->alloc, file->dirpath);
    ape_allocator_free(&ctx->alloc, file->path);
    ape_allocator_free(&ctx->alloc, file);
    return NULL;
}

ApeAstCompScope_t* ape_make_compscope(ApeContext_t* ctx, ApeAstCompScope_t* outer)
{
    ApeAstCompScope_t* scope;
    scope = (ApeAstCompScope_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstCompScope_t));
    if(!scope)
    {
        return NULL;
    }
    memset(scope, 0, sizeof(ApeAstCompScope_t));
    scope->context = ctx;
    scope->outer = outer;
    scope->bytecode = ape_make_valarray(ctx, sizeof(ApeUShort_t));
    if(!scope->bytecode)
    {
        goto err;
    }
    scope->srcpositions = ape_make_valarray(ctx, sizeof(ApePosition_t));
    if(!scope->srcpositions)
    {
        goto err;
    }
    scope->breakipstack = ape_make_valarray(ctx, sizeof(ApeInt_t));
    if(!scope->breakipstack)
    {
        goto err;
    }
    scope->continueipstack = ape_make_valarray(ctx, sizeof(int));
    if(!scope->continueipstack)
    {
        goto err;
    }
    return scope;
err:
    ape_compscope_destroy(scope);
    return NULL;
}

void ape_compscope_destroy(ApeAstCompScope_t* scope)
{
    ApeContext_t* ctx;
    ctx = scope->context;
    ape_valarray_destroy(scope->continueipstack);
    ape_valarray_destroy(scope->breakipstack);
    ape_valarray_destroy(scope->bytecode);
    ape_valarray_destroy(scope->srcpositions);
    ape_allocator_free(&ctx->alloc, scope);
}

ApeAstCompResult_t* ape_compscope_orphanresult(ApeAstCompScope_t* scope)
{
    ApeAstCompResult_t* res;
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

ApeAstCompResult_t* ape_make_compresult(ApeContext_t* ctx, ApeUShort_t* bytecode, ApePosition_t* src_positions, int count)
{
    ApeAstCompResult_t* res;
    res = (ApeAstCompResult_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstCompResult_t));
    if(!res)
    {
        return NULL;
    }
    memset(res, 0, sizeof(ApeAstCompResult_t));
    res->context = ctx;
    res->bytecode = bytecode;
    res->srcpositions = src_positions;
    res->count = count;
    return res;
}

void ape_compresult_destroy(ApeAstCompResult_t* res)
{
    ApeContext_t* ctx;
    if(res == NULL)
    {
        return;
    }
    ctx = res->context;
    ape_allocator_free(&ctx->alloc, res->bytecode);
    ape_allocator_free(&ctx->alloc, res->srcpositions);
    ape_allocator_free(&ctx->alloc, res);
}


ApeAstBlockScope_t* ape_make_blockscope(ApeContext_t* ctx, int offset)
{
    ApeAstBlockScope_t* sc;
    sc = (ApeAstBlockScope_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstBlockScope_t));
    if(!sc)
    {
        return NULL;
    }
    memset(sc, 0, sizeof(ApeAstBlockScope_t));
    sc->context = ctx;
    sc->store = ape_make_strdict(ctx, (ApeDataCallback_t)ape_symbol_copy, (ApeDataCallback_t)ape_symbol_destroy);
    if(!sc->store)
    {
        ape_blockscope_destroy(ctx, sc);
        return NULL;
    }
    sc->numdefinitions = 0;
    sc->offset = offset;
    return sc;
}

void* ape_blockscope_destroy(ApeContext_t* ctx, ApeAstBlockScope_t* scope)
{
    if(scope != NULL)
    {
        if(scope->store != NULL)
        {
            ape_strdict_destroywithitems(ctx, scope->store);
        }
        ape_allocator_free(&ctx->alloc, scope);
    }
    return NULL;
}

ApeAstBlockScope_t* ape_blockscope_copy(ApeContext_t* ctx, ApeAstBlockScope_t* scope)
{
    ApeAstBlockScope_t* copy;
    if(scope == NULL)
    {
        return NULL;
    }
    copy = (ApeAstBlockScope_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstBlockScope_t));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(ApeAstBlockScope_t));
    copy->context = ctx;
    copy->numdefinitions = scope->numdefinitions;
    copy->offset = scope->offset;
    if(copy->store != NULL)
    {
        copy->store = ape_strdict_copywithitems(ctx, scope->store);
        if(!copy->store)
        {
            ape_blockscope_destroy(ctx, copy);
            return NULL;
        }
    }
    return copy;
}


