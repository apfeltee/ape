
#include "ape.h"

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

bool ape_compiler_importmodule(ApeCompiler_t* comp, ApeStatement_t* importstmt)
{
    // todo: split into smaller functions
    ApeSize_t i;
    bool ok;
    bool result;
    char* filepath;
    char* code;
    char* namecopy;
    const char* loadedname;
    const char* modulepath;
    const char* module_name;
    const char* filepathnoncanonicalised;
    ApeFileScope_t* filescope;
    ApeWriter_t* filepathbuf;
    ApeSymTable_t* symtable;
    ApeFileScope_t* fs;
    ApeModule_t* module;
    ApeSymTable_t* st;
    ApeSymbol_t* symbol;
    result = false;
    filepath = NULL;
    code = NULL;
    filescope = (ApeFileScope_t*)ape_ptrarray_top(comp->filescopes);
    modulepath = importstmt->import.path;
    module_name = ape_module_getname(modulepath);
    for(i = 0; i < ape_ptrarray_count(filescope->loadedmodulenames); i++)
    {
        loadedname = (const char*)ape_ptrarray_get(filescope->loadedmodulenames, i);
        if(ape_util_strequal(loadedname, module_name))
        {
            ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, importstmt->pos, "module '%s' was already imported", module_name);
            result = false;
            goto end;
        }
    }
    filepathbuf = ape_make_writer(comp->context);
    if(!filepathbuf)
    {
        result = false;
        goto end;
    }
    if(ape_util_isabspath(modulepath))
    {
        ape_writer_appendf(filepathbuf, "%s.ape", modulepath);
    }
    else
    {
        ape_writer_appendf(filepathbuf, "%s%s.ape", filescope->file->dirpath, modulepath);
    }
    if(ape_writer_failed(filepathbuf))
    {
        ape_writer_destroy(filepathbuf);
        result = false;
        goto end;
    }
    filepathnoncanonicalised = ape_writer_getdata(filepathbuf);
    filepath = ape_util_canonicalisepath(comp->context, filepathnoncanonicalised);
    ape_writer_destroy(filepathbuf);
    if(!filepath)
    {
        result = false;
        goto end;
    }
    symtable = ape_compiler_getsymboltable(comp);
    if(symtable->outer != NULL || ape_ptrarray_count(symtable->blockscopes) > 1)
    {
        ape_errorlist_add(comp->errors, APE_ERROR_COMPILATION, importstmt->pos, "modules can only be imported in global scope");
        result = false;
        goto end;
    }
    for(i = 0; i < ape_ptrarray_count(comp->filescopes); i++)
    {
        fs = (ApeFileScope_t*)ape_ptrarray_get(comp->filescopes, i);
        if(APE_STREQ(fs->file->path, filepath))
        {
            ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, importstmt->pos, "cyclic reference of file '%s'", filepath);
            result = false;
            goto end;
        }
    }
    module = (ApeModule_t*)ape_strdict_get(comp->modules, filepath);
    if(!module)
    {
        // todo: create new module function
        if(!comp->config->fileio.ioreader.fnreadfile)
        {
            ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, importstmt->pos,
                              "cannot import module '%s', file read function not configured", filepath);
            result = false;
            goto end;
        }
        code = comp->config->fileio.ioreader.fnreadfile(comp->config->fileio.ioreader.context, filepath);
        if(!code)
        {
            ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, importstmt->pos, "reading module file '%s' failed", filepath);
            result = false;
            goto end;
        }
        module = ape_make_module(comp->context, module_name);
        if(!module)
        {
            result = false;
            goto end;
        }
        ok = ape_compiler_pushfilescope(comp, filepath);
        if(!ok)
        {
            ape_module_destroy(module);
            result = false;
            goto end;
        }
        ok = ape_compiler_compilecode(comp, code);
        if(!ok)
        {
            ape_module_destroy(module);
            result = false;
            goto end;
        }
        st = ape_compiler_getsymboltable(comp);
        for(i = 0; i < ape_symtable_getmoduleglobalsymbolcount(st); i++)
        {
            symbol = (ApeSymbol_t*)ape_symtable_getmoduleglobalsymbolat(st, i);
            ape_module_addsymbol(module, symbol);
        }
        ape_compiler_popfilescope(comp);
        ok = ape_strdict_set(comp->modules, filepath, module);
        if(!ok)
        {
            ape_module_destroy(module);
            result = false;
            goto end;
        }
    }
    for(i = 0; i < ape_ptrarray_count(module->symbols); i++)
    {
        symbol = (ApeSymbol_t*)ape_ptrarray_get(module->symbols, i);
        ok = ape_symtable_addmodulesymbol(symtable, symbol);
        if(!ok)
        {
            result = false;
            goto end;
        }
    }
    namecopy = ape_util_strdup(comp->context, module_name);
    if(!namecopy)
    {
        result = false;
        goto end;
    }
    ok = ape_ptrarray_add(filescope->loadedmodulenames, namecopy);
    if(!ok)
    {
        ape_allocator_free(comp->alloc, namecopy);
        result = false;
        goto end;
    }
    result = true;
end:
    ape_allocator_free(comp->alloc, filepath);
    ape_allocator_free(comp->alloc, code);
    return result;
}

