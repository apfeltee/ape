
#include "inline.h"

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
    module->name = ape_util_strdup(ctx, name);
    if(!module->name)
    {
        ape_module_destroy(module);
        return NULL;
    }
    module->modsymbols = ape_make_ptrarray(ctx);
    if(!module->modsymbols)
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
    ape_allocator_free(&module->context->alloc, module->name);
    ape_ptrarray_destroywithitems(module->modsymbols, (ApeDataCallback_t)ape_symbol_destroy);
    ape_allocator_free(&module->context->alloc, module);
    return NULL;
}

ApeModule_t* ape_module_copy(ApeModule_t* src)
{
    ApeModule_t* copy;
    copy = (ApeModule_t*)ape_allocator_alloc(&src->context->alloc, sizeof(ApeModule_t));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(ApeModule_t));
    copy->context = src->context;
    copy->name = ape_util_strdup(src->context, src->name);
    if(!copy->name)
    {
        ape_module_destroy(copy);
        return NULL;
    }
    copy->modsymbols = ape_ptrarray_copywithitems(src->modsymbols, (ApeDataCallback_t)ape_symbol_copy, (ApeDataCallback_t)ape_symbol_destroy);
    if(!copy->modsymbols)
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
    ok = ape_ptrarray_add(module->modsymbols, module_symbol);
    if(!ok)
    {
        ape_symbol_destroy(module_symbol);
        return false;
    }
    return true;
}


