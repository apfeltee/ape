
#include "inline.h"

ApeModule* ape_make_module(ApeContext* ctx, const char* name)
{
    ApeModule* module;
    module = (ApeModule*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeModule));
    if(!module)
    {
        return NULL;
    }
    memset(module, 0, sizeof(ApeModule));
    module->context = ctx;
    module->name = ape_util_strdup(ctx, name);
    if(!module->name)
    {
        ape_module_destroy(ctx, module);
        return NULL;
    }
    module->modsymbols = ape_make_ptrarray(ctx);
    if(!module->modsymbols)
    {
        ape_module_destroy(ctx, module);
        return NULL;
    }
    return module;
}

void* ape_module_destroy(ApeContext* ctx, ApeModule* module)
{
    if(!module)
    {
        return NULL;
    }
    ape_allocator_free(&ctx->alloc, module->name);
    ape_ptrarray_destroywithitems(ctx, module->modsymbols, (ApeDataCallback)ape_symbol_destroy);
    ape_allocator_free(&ctx->alloc, module);
    return NULL;
}

ApeModule* ape_module_copy(ApeContext* ctx, ApeModule* src)
{
    ApeModule* copy;
    copy = (ApeModule*)ape_allocator_alloc(&src->context->alloc, sizeof(ApeModule));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(ApeModule));
    copy->context = src->context;
    copy->name = ape_util_strdup(src->context, src->name);
    if(!copy->name)
    {
        ape_module_destroy(ctx, copy);
        return NULL;
    }
    copy->modsymbols = ape_ptrarray_copywithitems(ctx, src->modsymbols, (ApeDataCallback)ape_symbol_copy, (ApeDataCallback)ape_symbol_destroy);
    if(!copy->modsymbols)
    {
        ape_module_destroy(ctx, copy);
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

bool ape_module_addsymbol(ApeModule* module, const ApeSymbol* symbol)
{
    bool ok;
    ApeWriter* name_buf;
    ApeSymbol* module_symbol;
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
    ok = ape_ptrarray_push(module->modsymbols, &module_symbol);
    if(!ok)
    {
        ape_symbol_destroy(module->context, module_symbol);
        return false;
    }
    return true;
}


