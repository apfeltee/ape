
#if defined(__cplusplus)
    #include <vector>
#endif
#include "inline.h"

typedef struct ApeTempU64Array_t ApeTempU64Array_t;

struct ApeTempU64Array_t
{
    ApeOpByte_t data[2];
};

#if 0
    #define NARGS_SEQ(_1,_2,_3,_4,_5,_6,_7,_8,_9,N,...) N
    #define NARGS(...) NARGS_SEQ(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1)
#endif

/* have not yet found a better way to do this in c++ ... */
#if defined(__cplusplus)
    #define make_u64_array(...) (std::vector<ApeOpByte_t>{__VA_ARGS__}).data()
#else
    #define make_u64_array(...) ((ApeOpByte_t[]){__VA_ARGS__ })
#endif

static const ApePosition_t g_ccpriv_srcposinvalid = { NULL, -1, -1 };

void ape_compiler_setsymtable(ApeAstCompiler_t* comp, ApeSymTable_t* table)
{
    ApeAstFileScope_t* filescope;
    filescope = (ApeAstFileScope_t*)ape_ptrarray_top(comp->filescopes);
    if(!filescope)
    {
        APE_ASSERT(false);
        return;
    }
    filescope->symtable = table;
}

ApeValArray_t* ape_compiler_getconstants(ApeAstCompiler_t* comp)
{
    return comp->constants;
}

ApeSymTable_t* ape_compiler_getsymboltable(ApeAstCompiler_t* comp)
{
    ApeSize_t l;
    ApeAstFileScope_t* filescope;
    filescope = NULL;
    if((l = ape_ptrarray_count(comp->filescopes)) > 0)
    {
        //filescope = *(ApeAstFileScope_t**)ape_ptrarray_top(comp->filescopes);
        filescope = (ApeAstFileScope_t*)ape_ptrarray_top(comp->filescopes);
        //filescope = (ApeAstFileScope_t*)ape_ptrarray_get(comp->filescopes, l - 1);
    }
    //fprintf(stderr, "filescope=%p comp->filescopes=%d\n", filescope, ape_ptrarray_count(comp->filescopes));
    if(!filescope)
    {
        APE_ASSERT(false);
        return NULL;
    }
    return filescope->symtable;
}

ApeSymbol_t* ape_compiler_definesym(ApeAstCompiler_t* comp, ApePosition_t pos, const char* name, bool assignable, bool canshadow)
{
    ApeSymTable_t* symtable =NULL;
    ApeSymbol_t* currentsymbol;
    ApeSymbol_t* symbol;
    symtable = ape_compiler_getsymboltable(comp);
    if(!canshadow && !ape_symtable_istopglobalscope(symtable))
    {
        currentsymbol = (ApeSymbol_t*)ape_symtable_resolve(symtable, name);
        if(currentsymbol)
        {
            ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, pos, "symbol '%s' is already defined", name);
            return NULL;
        }
    }
    symbol = (ApeSymbol_t*)ape_symtable_define(symtable, name, assignable);
    if(!symbol)
    {
        ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, pos, "cannot define symbol '%s'", name);
        return NULL;
    }
    return symbol;
}

ApeAstCompiler_t* ape_compiler_make(ApeContext_t* ctx, const ApeConfig_t* cfg, ApeGCMemory_t* mem, ApeErrorList_t* el, ApePtrArray_t* files, ApeGlobalStore_t* gs)
{
    bool ok;
    ApeAstCompiler_t* comp;
    comp = (ApeAstCompiler_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstCompiler_t));
    if(!comp)
    {
        return NULL;
    }
    comp->context = ctx;
    ok = ape_compiler_init(comp, ctx, cfg, mem, el, files, gs);
    if(!ok)
    {
        ape_allocator_free(&ctx->alloc, comp);
        return NULL;
    }
    return comp;
}

void ape_compiler_destroy(ApeAstCompiler_t* comp)
{
    ApeContext_t* ctx;
    if(!comp)
    {
        return;
    }
    ctx = comp->context;
    ape_compiler_deinit(comp);
    ape_allocator_free(&ctx->alloc, comp);
}

bool ape_compiler_compilecode(ApeAstCompiler_t* comp, const char* code, size_t csize)
{
    bool ok;
    ApeAstFileScope_t* filescope;
    ApePtrArray_t* statements;
    filescope = (ApeAstFileScope_t*)ape_ptrarray_top(comp->filescopes);
    APE_ASSERT(filescope);
    statements = ape_parser_parseall(filescope->parser, code, csize, filescope->file);
    if(!statements)
    {
        /* errors are added by parser */
        return false;
    }
    if(comp->context->config.dumpast)
    {
        ape_context_dumpast(comp->context, statements);
        if(!comp->context->config.runafterdump)
        {
            filescope = NULL;
            ape_ptrarray_destroywithitems(comp->context, statements, (ApeDataCallback_t)ape_ast_destroy_expr);
            return false;
        }
    }
    ok = ape_compiler_compilestmtlist(comp, statements);
    ape_ptrarray_destroywithitems(comp->context, statements, (ApeDataCallback_t)ape_ast_destroy_expr);
    return ok;
}

ApeAstCompResult_t* ape_compiler_compilesource(ApeAstCompiler_t* comp, const char* code, size_t csize)
{
    bool ok;
    ApeAstCompiler_t compshallowcopy;
    ApeAstCompScope_t* compscope;
    ApeAstCompResult_t* res;
    compscope = ape_compiler_getcompscope(comp);
    //fprintf(stderr, "ape_valarray_count(comp->srcpositionsstack)=%d\n", ape_valarray_count(comp->srcpositionsstack));
    /*
    APE_ASSERT(ape_valarray_count(comp->srcpositionsstack) == 0);
    APE_ASSERT(ape_valarray_count(compscope->bytecode) == 0);
    APE_ASSERT(ape_valarray_count(compscope->breakipstack) == 0);
    APE_ASSERT(ape_valarray_count(compscope->continueipstack) == 0);
    */
    ape_valarray_clear(comp->srcpositionsstack);
    ape_valarray_clear(compscope->bytecode);
    ape_valarray_clear(compscope->srcpositions);
    ape_valarray_clear(compscope->breakipstack);
    ape_valarray_clear(compscope->continueipstack);
    ok = ape_compiler_initshallowcopy(&compshallowcopy, comp);
    if(!ok)
    {
        return NULL;
    }
    ok = ape_compiler_compilecode(comp, code, csize);
    if(!ok)
    {
        goto err;
    }
    /* might've changed */
    compscope = ape_compiler_getcompscope(comp);
    APE_ASSERT(compscope->outer == NULL);
    compscope = ape_compiler_getcompscope(comp);
    res = ape_compscope_orphanresult(compscope);
    if(!res)
    {
        goto err;
    }
    ape_compiler_deinit(&compshallowcopy);
    return res;
err:
    ape_compiler_deinit(comp);
    *comp = compshallowcopy;
    return NULL;
}

ApeAstCompResult_t* ape_compiler_compilefile(ApeAstCompiler_t* comp, const char* path)
{
    bool ok;
    size_t clen;
    char* code;
    ApeAstCompFile_t* file;
    ApeAstCompResult_t* res;
    ApeAstFileScope_t* filescope;
    ApeAstCompFile_t* prevfile;
    (void)clen;
    code = NULL;
    file = NULL;
    res = NULL;
    /* todo: read code function */
    if(!comp->config->fileio.fnreadfile)
    {
        ape_errorlist_add(comp->errors, APE_ERROR_COMPILATION, g_ccpriv_srcposinvalid, "file read function not configured");
        goto err;
    }
    code = comp->config->fileio.fnreadfile(comp->context, path, -1, &clen);
    if(!code)
    {
        ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, g_ccpriv_srcposinvalid, "reading file '%s' failed", path);
        goto err;
    }
    file = ape_make_compfile(comp->context, path);
    if(!file)
    {
        goto err;
    }
    ok = ape_ptrarray_push(comp->files, &file);
    if(!ok)
    {
        ape_compfile_destroy(comp->context, file);
        goto err;
    }
    APE_ASSERT(ape_ptrarray_count(comp->filescopes) == 1);
    filescope = (ApeAstFileScope_t*)ape_ptrarray_top(comp->filescopes);
    if(!filescope)
    {
        goto err;
    }
    /* todo: push file scope instead? */
    prevfile = filescope->file;
    filescope->file = file;
    res = ape_compiler_compilesource(comp, code, clen);
    if(!res)
    {
        if(comp->context->config.runafterdump)
        {
            filescope->file = prevfile;
        }
        goto err;
    }
    if(filescope)
    {
        filescope->file = prevfile;
    }
    ape_allocator_free(&comp->context->alloc, code);
    return res;
err:
    ape_allocator_free(&comp->context->alloc, code);
    return NULL;
}

bool ape_compiler_init(ApeAstCompiler_t* comp, ApeContext_t* ctx, const ApeConfig_t* cfg, ApeGCMemory_t* mem, ApeErrorList_t* el, ApePtrArray_t* fl, ApeGlobalStore_t* gs)
{
    bool ok;
    memset(comp, 0, sizeof(ApeAstCompiler_t));
    comp->context = ctx;
    comp->config = cfg;
    comp->mem = mem;
    comp->errors = el;
    comp->files = fl;
    comp->globalstore = gs;
    comp->filescopes = ape_make_ptrarray(ctx);
    if(!comp->filescopes)
    {
        goto err;
    }
    comp->constants = ape_make_valarray(ctx, sizeof(ApeObject_t));
    if(!comp->constants)
    {
        goto err;
    }
    comp->srcpositionsstack = ape_make_valarray(ctx, sizeof(ApePosition_t));
    if(!comp->srcpositionsstack)
    {
        goto err;
    }
    comp->modules = ape_make_strdict(ctx, (ApeDataCallback_t)ape_module_copy, (ApeDataCallback_t)ape_module_destroy);
    if(!comp->modules)
    {
        goto err;
    }
    ok = ape_compiler_pushcompscope(comp);
    if(!ok)
    {
        goto err;
    }
    ok = ape_compiler_pushfilescope(comp, "none");
    if(!ok)
    {
        goto err;
    }
    comp->stringconstantspositions = ape_make_strdict(ctx, NULL, NULL);
    if(!comp->stringconstantspositions)
    {
        goto err;
    }

    return true;
err:
    ape_compiler_deinit(comp);
    return false;
}

void ape_compiler_deinit(ApeAstCompiler_t* comp)
{
    ApeSize_t i;
    ApeInt_t* val;
    if(!comp)
    {
        return;
    }
    for(i = 0; i < ape_strdict_count(comp->stringconstantspositions); i++)
    {
        val = (ApeInt_t*)ape_strdict_getvalueat(comp->stringconstantspositions, i);
        ape_allocator_free(&comp->context->alloc, val);
    }
    ape_strdict_destroy(comp->stringconstantspositions);
    while(ape_ptrarray_count(comp->filescopes) > 0)
    {
        ape_compiler_popfilescope(comp);
    }
    while(ape_compiler_getcompscope(comp))
    {
        ape_compiler_popcompscope(comp);
    }
    ape_strdict_destroywithitems(comp->context, comp->modules);
    ape_valarray_destroy(comp->srcpositionsstack);
    ape_valarray_destroy(comp->constants);
    ape_ptrarray_destroy(comp->filescopes);
    memset(comp, 0, sizeof(ApeAstCompiler_t));
}

bool ape_compiler_initshallowcopy(ApeAstCompiler_t* copy, ApeAstCompiler_t* src)
{
    ApeSize_t i;
    bool ok;
    ApeInt_t* val;
    ApeInt_t* valcopy;
    const char* key;
    const char* loadedname;
    char* loadednamecopy;
    ApeSymTable_t* srcst;
    ApeSymTable_t* srcstcopy;
    ApeSymTable_t* copyst;
    ApeStrDict_t* modulescopy;
    ApeValArray_t* constantscopy;
    ApeAstFileScope_t* srcfilescope;
    ApeAstFileScope_t* copyfilescope;
    ApePtrArray_t* srcloadedmodulenames;
    ApePtrArray_t* copyloadedmodulenames;
    ok = ape_compiler_init(copy, src->context, src->config, src->mem, src->errors, src->files, src->globalstore);
    if(!ok)
    {
        return false;
    }
    copy->context = src->context;
    srcst = ape_compiler_getsymboltable(src);
    APE_ASSERT(ape_ptrarray_count(src->filescopes) == 1);
    //APE_ASSERT(srcst->outer == NULL);
    srcstcopy = ape_symtable_copy(src->context, srcst);
    if(!srcstcopy)
    {
        goto err;
    }
    copyst = ape_compiler_getsymboltable(copy);
    ape_symtable_destroy(copyst);
    copyst = NULL;
    ape_compiler_setsymtable(copy, srcstcopy);
    modulescopy = ape_strdict_copywithitems(src->context, src->modules);
    if(!modulescopy)
    {
        goto err;
    }
    ape_strdict_destroywithitems(src->context, copy->modules);
    copy->modules = modulescopy;
    constantscopy = ape_valarray_copy(src->context, src->constants);
    if(!constantscopy)
    {
        goto err;
    }
    ape_valarray_destroy(copy->constants);
    copy->constants = constantscopy;
    for(i = 0; i < ape_strdict_count(src->stringconstantspositions); i++)
    {
        key = (const char*)ape_strdict_getkeyat(src->stringconstantspositions, i);
        val = (ApeInt_t*)ape_strdict_getvalueat(src->stringconstantspositions, i);
        valcopy = (ApeInt_t*)ape_allocator_alloc(&src->context->alloc, sizeof(ApeInt_t));
        if(!valcopy)
        {
            goto err;
        }
        *valcopy = *val;
        ok = ape_strdict_set(copy->stringconstantspositions, key, valcopy);
        if(!ok)
        {
            ape_allocator_free(&src->context->alloc, valcopy);
            goto err;
        }
    }
    srcfilescope = (ApeAstFileScope_t*)ape_ptrarray_top(src->filescopes);
    copyfilescope = (ApeAstFileScope_t*)ape_ptrarray_top(copy->filescopes);
    srcloadedmodulenames = srcfilescope->loadedmodulenames;
    copyloadedmodulenames = copyfilescope->loadedmodulenames;
    for(i = 0; i < ape_ptrarray_count(srcloadedmodulenames); i++)
    {
        loadedname = (const char*)ape_ptrarray_get(srcloadedmodulenames, i);
        loadednamecopy = ape_util_strdup(src->context, loadedname);
        if(!loadednamecopy)
        {
            goto err;
        }
        ok = ape_ptrarray_push(copyloadedmodulenames, &loadednamecopy);
        if(!ok)
        {
            ape_allocator_free(&src->context->alloc, loadednamecopy);
            goto err;
        }
    }
    return true;
err:
    ape_compiler_deinit(copy);
    return false;
}

ApeInt_t ape_compiler_emit(ApeAstCompiler_t* comp, ApeOpByte_t op, ApeSize_t operandscount, ApeOpByte_t* operands)
{
    ApeInt_t i;
    ApeInt_t ip;
    ApeInt_t len;
    bool ok;
    ApePosition_t* srcpos;
    ApeAstCompScope_t* compscope;
    ip = ape_compiler_getip(comp);
    len = ape_make_code(op, operandscount, operands, ape_compiler_getbytecode(comp));
    if(len == 0)
    {
        return -1;
    }
    for(i = 0; i < len; i++)
    {
        srcpos = (ApePosition_t*)ape_valarray_top(comp->srcpositionsstack);
        #if 0
            APE_ASSERT(srcpos->line >= 0);
            APE_ASSERT(srcpos->column >= 0);
        #endif
        ok = ape_valarray_push(ape_compiler_getsrcpositions(comp), srcpos);
        if(!ok)
        {
            return -1;
        }
    }
    compscope = ape_compiler_getcompscope(comp);
    compscope->lastopcode = op;
    return ip;
}

ApeAstCompScope_t* ape_compiler_getcompscope(ApeAstCompiler_t* comp)
{
    return comp->compilation_scope;
}

bool ape_compiler_pushcompscope(ApeAstCompiler_t* comp)
{
    ApeAstCompScope_t* currentscope;
    ApeAstCompScope_t* newscope;
    currentscope = ape_compiler_getcompscope(comp);
    newscope = ape_make_compscope(comp->context, currentscope);
    if(!newscope)
    {
        return false;
    }
    ape_compiler_setcompscope(comp, newscope);
    return true;
}

void ape_compiler_popcompscope(ApeAstCompiler_t* comp)
{
    ApeAstCompScope_t* currentscope;
    currentscope = ape_compiler_getcompscope(comp);
    APE_ASSERT(currentscope);
    ape_compiler_setcompscope(comp, currentscope->outer);
    ape_compscope_destroy(currentscope);
}

bool ape_compiler_pushsymtable(ApeAstCompiler_t* comp, ApeInt_t globaloffset)
{
    ApeAstFileScope_t* filescope;
    ApeSymTable_t* currenttable;
    filescope = (ApeAstFileScope_t*)ape_ptrarray_top(comp->filescopes);
    //fprintf(stderr, "pushsymtable: filescopes=%ld\n", ape_ptrarray_count(comp->filescopes));
    if(!filescope)
    {
        #if 1
            APE_ASSERT(false);
            return false;
        #else
            ApeAstCompFile_t* file = ape_make_compfile(comp->context, "none");
            filescope = ape_compiler_makefilescope(comp, file);
            ape_ptrarray_push(comp->filescopes, &filescope);
        #endif
    }
    currenttable = filescope->symtable;
    filescope->symtable = ape_make_symtable(comp->context, currenttable, comp->globalstore, globaloffset);
    if(!filescope->symtable)
    {
        filescope->symtable = currenttable;
        return false;
    }
    return true;
}

void ape_compiler_popsymtable(ApeAstCompiler_t* comp)
{
    ApeAstFileScope_t* filescope;
    ApeSymTable_t* currenttable;
    filescope = (ApeAstFileScope_t*)ape_ptrarray_top(comp->filescopes);
    if(!filescope)
    {
        APE_ASSERT(false);
        return;
    }
    currenttable = filescope->symtable;
    if(!currenttable)
    {
        APE_ASSERT(false);
        return;
    }
    filescope->symtable = currenttable->outer;
    ape_symtable_destroy(currenttable);
}

ApeOpByte_t ape_compiler_getlastopcode(ApeAstCompiler_t* comp)
{
    ApeAstCompScope_t* currentscope;
    currentscope = ape_compiler_getcompscope(comp);
    return currentscope->lastopcode;
}

bool ape_compiler_compilestmtlist(ApeAstCompiler_t* comp, ApePtrArray_t* statements)
{
    ApeSize_t i;
    bool ok;
    ApeAstExpression_t* stmt;
    ok = true;
    for(i = 0; i < ape_ptrarray_count(statements); i++)
    {
        stmt = (ApeAstExpression_t*)ape_ptrarray_get(statements, i);
        ok = ape_compiler_compilestatement(comp, stmt);
        if(!ok)
        {
            break;
        }
    }
    return ok;
}


bool ape_compiler_includemodule(ApeAstCompiler_t* comp, ApeAstExpression_t* includestmt)
{
    /* todo: split into smaller functions */
    ApeSize_t i;
    bool ok;
    bool result;
    size_t clen;
    char* filepath;
    char* code;
    char* namecopy;
    const char* loadedname;
    const char* modulepath;
    const char* module_name;
    const char* filepathnoncanonicalised;
    ApeAstFileScope_t* filescope;
    ApeWriter_t* filepathbuf;
    ApeSymTable_t* symtable;
    ApeAstFileScope_t* fs;
    ApeModule_t* module;
    ApeSymTable_t* st;
    ApeSymbol_t* symbol;
    (void)clen;
    result = false;
    filepath = NULL;
    code = NULL;
    filescope = (ApeAstFileScope_t*)ape_ptrarray_top(comp->filescopes);
    modulepath = includestmt->exincludestmt.path;
    module_name = ape_module_getname(modulepath);
    for(i = 0; i < ape_ptrarray_count(filescope->loadedmodulenames); i++)
    {
        loadedname = (const char*)ape_ptrarray_get(filescope->loadedmodulenames, i);
        if(ape_util_strequal(loadedname, module_name))
        {
            ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, includestmt->pos, "module '%s' was already included", module_name);
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
        ape_errorlist_add(comp->errors, APE_ERROR_COMPILATION, includestmt->pos, "modules can only be included in global scope");
        result = false;
        goto end;
    }
    for(i = 0; i < ape_ptrarray_count(comp->filescopes); i++)
    {
        fs = (ApeAstFileScope_t*)ape_ptrarray_get(comp->filescopes, i);
        if(APE_STREQ(fs->file->path, filepath))
        {
            ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, includestmt->pos, "cyclic reference of file '%s'", filepath);
            result = false;
            goto end;
        }
    }
    module = (ApeModule_t*)ape_strdict_getbyname(comp->modules, filepath);
    if(!module)
    {
        /* todo: create new module function */
        if(!comp->config->fileio.fnreadfile)
        {
            ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, includestmt->pos,
                              "cannot include file '%s', file read function not configured", filepath);
            result = false;
            goto end;
        }
        code = comp->config->fileio.fnreadfile(comp->context, filepath, -1, &clen);
        if(!code)
        {
            ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, includestmt->pos, "reading file '%s' failed", filepath);
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
            ape_module_destroy(comp->context, module);
            result = false;
            goto end;
        }
        ok = ape_compiler_compilecode(comp, code, clen);
        if(!ok)
        {
            ape_module_destroy(comp->context, module);
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
            ape_module_destroy(comp->context, module);
            result = false;
            goto end;
        }
    }
    for(i = 0; i < ape_ptrarray_count(module->modsymbols); i++)
    {
        symbol = (ApeSymbol_t*)ape_ptrarray_get(module->modsymbols, i);
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
    ok = ape_ptrarray_push(filescope->loadedmodulenames, &namecopy);
    if(!ok)
    {
        ape_allocator_free(&comp->context->alloc, namecopy);
        result = false;
        goto end;
    }
    result = true;
end:
    ape_allocator_free(&comp->context->alloc, filepath);
    ape_allocator_free(&comp->context->alloc, code);
    return result;
}

bool ape_compiler_compilestatement(ApeAstCompiler_t* comp, ApeAstExpression_t* stmt)
{
    bool ok;
    ApeInt_t afteraltip;
    ApeInt_t afterbodyip;
    ApeInt_t afterelifip;
    ApeInt_t afterjumptorecoverip;
    ApeInt_t afterrecoverip;
    ApeInt_t aftertestip;
    ApeInt_t afterupdateip;
    ApeInt_t beforetestip;
    ApeInt_t breakip;
    ApeInt_t continueip;
    ApeInt_t ip;
    ApeInt_t jmptoafterbodyip;
    ApeInt_t jumptoafterbodyip;
    ApeInt_t jumptoafterrecoverip;
    ApeInt_t jumptoafterupdateip;
    ApeInt_t jumptoendip;
    ApeInt_t nextcasejumpip;
    ApeInt_t recoverip;
    ApeInt_t updateip;
    ApeInt_t* pos;
    ApeSize_t i;
    ApeAstCompScope_t* compscope;
    ApeAstForExpr_t* forloop;
    ApeAstForeachExpr_t* foreach;
    ApeAstIfCaseExpr_t* ifcase;
    ApeAstIfExpr_t* ifstmt;
    ApeAstRecoverExpr_t* recover;
    ApeSymTable_t* symtable;
    ApeSymbol_t* errorsymbol;
    ApeSymbol_t* indexsymbol;
    ApeSymbol_t* itersymbol;
    ApeSymbol_t* sourcesymbol;
    ApeSymbol_t* symbol;
    ApeValArray_t* jumptoendips;
    ApeAstWhileLoopExpr_t* whileloop;
    breakip = 0;
    ok = false;
    ip = -1;
    ok = ape_valarray_push(comp->srcpositionsstack, &stmt->pos);
    if(!ok)
    {
        return false;
    }
    compscope = ape_compiler_getcompscope(comp);
    symtable = ape_compiler_getsymboltable(comp);
    switch(stmt->extype)
    {
        case APE_EXPR_EXPRESSION:
            {
                ok = ape_compiler_compileexpression(comp, stmt->exexpression);
                if(!ok)
                {
                    return false;
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_POP, 0, NULL);
                if(ip < 0)
                {
                    return false;
                }
            }
            break;
        case APE_EXPR_DEFINE:
            {
                ok = ape_compiler_compileexpression(comp, stmt->exdefine.value);
                if(!ok)
                {
                    return false;
                }
                symbol = ape_compiler_definesym(comp, stmt->exdefine.name->pos, stmt->exdefine.name->value, stmt->exdefine.assignable, false);
                if(!symbol)
                {
                    return false;
                }
                ok = ape_compiler_writesym(comp, symbol, true);
                if(!ok)
                {
                    return false;
                }
            }
            break;
        case APE_EXPR_IFELSE:
            {
                ifstmt = &stmt->exifstmt;
                jumptoendips = ape_make_valarray(comp->context, sizeof(ApeInt_t));
                if(!jumptoendips)
                {
                    goto statementiferror;
                }
                for(i = 0; i < ape_ptrarray_count(ifstmt->cases); i++)
                {
                    ifcase = (ApeAstIfCaseExpr_t*)ape_ptrarray_get(ifstmt->cases, i);
                    ok = ape_compiler_compileexpression(comp, ifcase->test);
                    if(!ok)
                    {
                        goto statementiferror;
                    }
                    nextcasejumpip = ape_compiler_emit(comp, APE_OPCODE_JUMPIFFALSE, 1, make_u64_array((ApeOpByte_t)(0xbeef)));
                    ok = ape_compiler_compilecodeblock(comp, ifcase->consequence);
                    if(!ok)
                    {
                        goto statementiferror;
                    }
                    /* don't emit jump for the last statement */
                    if(i < (ape_ptrarray_count(ifstmt->cases) - 1) || ifstmt->alternative)
                    {
                        jumptoendip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((ApeOpByte_t)(0xbeef)));
                        ok = ape_valarray_push(jumptoendips, &jumptoendip);
                        if(!ok)
                        {
                            goto statementiferror;
                        }
                    }
                    afterelifip = ape_compiler_getip(comp);
                    ape_compiler_moduint16operand(comp, nextcasejumpip + 1, afterelifip);
                }
                if(ifstmt->alternative)
                {
                    ok = ape_compiler_compilecodeblock(comp, ifstmt->alternative);
                    if(!ok)
                    {
                        goto statementiferror;
                    }
                }
                afteraltip = ape_compiler_getip(comp);
                for(i = 0; i < ape_valarray_count(jumptoendips); i++)
                {
                    pos = (ApeInt_t*)ape_valarray_get(jumptoendips, i);
                    ape_compiler_moduint16operand(comp, *pos + 1, afteraltip);
                }
                ape_valarray_destroy(jumptoendips);
                break;
            statementiferror:
                ape_valarray_destroy(jumptoendips);
                return false;
            }
            break;
        case APE_EXPR_RETURNVALUE:
            {
                if(compscope->outer == NULL)
                {
                    ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "nothing to return from");
                    return false;
                }
                ip = -1;
                if(stmt->exreturn)
                {
                    ok = ape_compiler_compileexpression(comp, stmt->exreturn);
                    if(!ok)
                    {
                        return false;
                    }
                    ip = ape_compiler_emit(comp, APE_OPCODE_RETURNVALUE, 0, NULL);
                }
                else
                {
                    ip = ape_compiler_emit(comp, APE_OPCODE_RETURNNOTHING, 0, NULL);
                }
                if(ip < 0)
                {
                    return false;
                }
            }
            break;
        case APE_EXPR_WHILELOOP:
            {
                whileloop = &stmt->exwhilestmt;
                beforetestip = ape_compiler_getip(comp);
                ok = ape_compiler_compileexpression(comp, whileloop->test);
                if(!ok)
                {
                    return false;
                }
                aftertestip = ape_compiler_getip(comp);
                ip = ape_compiler_emit(comp, APE_OPCODE_JUMPIFTRUE, 1, make_u64_array((ApeOpByte_t)(aftertestip + 6)));
                if(ip < 0)
                {
                    return false;
                }
                jumptoafterbodyip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((ApeOpByte_t)0xdead));
                if(jumptoafterbodyip < 0)
                {
                    return false;
                }
                ok = ape_compiler_pushcontip(comp, beforetestip);
                if(!ok)
                {
                    return false;
                }
                ok = ape_compiler_pushbreakip(comp, jumptoafterbodyip);
                if(!ok)
                {
                    return false;
                }
                ok = ape_compiler_compilecodeblock(comp, whileloop->body);
                if(!ok)
                {
                    return false;
                }
                ape_compiler_popbreakip(comp);
                ape_compiler_popcontip(comp);
                ip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((ApeOpByte_t)beforetestip));
                if(ip < 0)
                {
                    return false;
                }
                afterbodyip = ape_compiler_getip(comp);
                ape_compiler_moduint16operand(comp, jumptoafterbodyip + 1, afterbodyip);
            }
            break;
        case APE_EXPR_BREAK:
            {
                breakip = ape_compiler_getbreakip(comp);
                if(breakip < 0)
                {
                    ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "nothing to break from");
                    return false;
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((ApeOpByte_t)breakip));
                if(ip < 0)
                {
                    return false;
                }
            }
            break;
        case APE_EXPR_CONTINUE:
            {
                continueip = ape_compiler_getcontip(comp);
                if(continueip < 0)
                {
                    ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "nothing to continue from");
                    return false;
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((ApeOpByte_t)continueip));
                if(ip < 0)
                {
                    return false;
                }
            }
            break;
        case APE_EXPR_FOREACH:
            {
                foreach = &stmt->exforeachstmt;
                ok = ape_symtable_pushblockscope(symtable);
                if(!ok)
                {
                    return false;
                }
                /* init */
                indexsymbol = ape_compiler_definesym(comp, stmt->pos, "@i", false, true);
                if(!indexsymbol)
                {
                    return false;
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_MKNUMBER, 1, make_u64_array((ApeOpByte_t)0));
                if(ip < 0)
                {
                    return false;
                }
                ok = ape_compiler_writesym(comp, indexsymbol, true);
                if(!ok)
                {
                    return false;
                }
                sourcesymbol = NULL;
                if(foreach->source->extype == APE_EXPR_IDENT)
                {
                    sourcesymbol = ape_symtable_resolve(symtable, foreach->source->exident->value);
                    if(!sourcesymbol)
                    {
                        ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, foreach->source->pos,
                                          "symbol '%s' could not be resolved", foreach->source->exident->value);
                        return false;
                    }
                }
                else
                {
                    ok = ape_compiler_compileexpression(comp, foreach->source);
                    if(!ok)
                    {
                        return false;
                    }
                    sourcesymbol = ape_compiler_definesym(comp, foreach->source->pos, "@source", false, true);
                    if(!sourcesymbol)
                    {
                        return false;
                    }
                    ok = ape_compiler_writesym(comp, sourcesymbol, true);
                    if(!ok)
                    {
                        return false;
                    }
                }
                /* update */
                jumptoafterupdateip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((ApeOpByte_t)0xbeef));
                if(jumptoafterupdateip < 0)
                {
                    return false;
                }
                updateip = ape_compiler_getip(comp);
                ok = ape_compiler_readsym(comp, indexsymbol);
                if(!ok)
                {
                    return false;
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_MKNUMBER, 1, make_u64_array((ApeOpByte_t)ape_util_floattouint(1)));
                if(ip < 0)
                {
                    return false;
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_ADD, 0, NULL);
                if(ip < 0)
                {
                    return false;
                }
                ok = ape_compiler_writesym(comp, indexsymbol, false);
                if(!ok)
                {
                    return false;
                }
                afterupdateip = ape_compiler_getip(comp);
                ape_compiler_moduint16operand(comp, jumptoafterupdateip + 1, afterupdateip);
                /* test */
                ok = ape_valarray_push(comp->srcpositionsstack, &foreach->source->pos);
                if(!ok)
                {
                    return false;
                }
                ok = ape_compiler_readsym(comp, sourcesymbol);
                if(!ok)
                {
                    return false;
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_LEN, 0, NULL);
                if(ip < 0)
                {
                    return false;
                }
                ape_valarray_pop(comp->srcpositionsstack);
                ok = ape_compiler_readsym(comp, indexsymbol);
                if(!ok)
                {
                    return false;
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_COMPAREPLAIN, 0, NULL);
                if(ip < 0)
                {
                    return false;
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_ISEQUAL, 0, NULL);
                if(ip < 0)
                {
                    return false;
                }
                aftertestip = ape_compiler_getip(comp);
                ip = ape_compiler_emit(comp, APE_OPCODE_JUMPIFFALSE, 1, make_u64_array((ApeOpByte_t)(aftertestip + 6)));
                if(ip < 0)
                {
                    return false;
                }
                ApeInt_t jumptoafterbodyip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((ApeOpByte_t)0xdead));
                if(jumptoafterbodyip < 0)
                {
                    return false;
                }
                ok = ape_compiler_readsym(comp, sourcesymbol);
                if(!ok)
                {
                    return false;
                }
                ok = ape_compiler_readsym(comp, indexsymbol);
                if(!ok)
                {
                    return false;
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_GETVALUEAT, 0, NULL);
                if(ip < 0)
                {
                    return false;
                }
                itersymbol = ape_compiler_definesym(comp, foreach->iterator->pos, foreach->iterator->value, false, false);
                if(!itersymbol)
                {
                    return false;
                }
                ok = ape_compiler_writesym(comp, itersymbol, true);
                if(!ok)
                {
                    return false;
                }
                /* body */
                ok = ape_compiler_pushcontip(comp, updateip);
                if(!ok)
                {
                    return false;
                }
                ok = ape_compiler_pushbreakip(comp, jumptoafterbodyip);
                if(!ok)
                {
                    return false;
                }
                ok = ape_compiler_compilecodeblock(comp, foreach->body);
                if(!ok)
                {
                    return false;
                }
                ape_compiler_popbreakip(comp);
                ape_compiler_popcontip(comp);
                ip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((ApeOpByte_t)updateip));
                if(ip < 0)
                {
                    return false;
                }
                afterbodyip = ape_compiler_getip(comp);
                ape_compiler_moduint16operand(comp, jumptoafterbodyip + 1, afterbodyip);
                ape_symtable_popblockscope(symtable);
            }
            break;
        case APE_EXPR_FORLOOP:
            {
                forloop = &stmt->exforstmt;
                ok = ape_symtable_pushblockscope(symtable);
                if(!ok)
                {
                    return false;
                }
                /* init */
                jumptoafterupdateip = 0;
                ok = false;
                if(forloop->init)
                {
                    ok = ape_compiler_compilestatement(comp, forloop->init);
                    if(!ok)
                    {
                        return false;
                    }
                    jumptoafterupdateip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((ApeOpByte_t)0xbeef));
                    if(jumptoafterupdateip < 0)
                    {
                        return false;
                    }
                }
                /* update */
                updateip = ape_compiler_getip(comp);
                if(forloop->update)
                {
                    ok = ape_compiler_compileexpression(comp, forloop->update);
                    if(!ok)
                    {
                        return false;
                    }
                    ip = ape_compiler_emit(comp, APE_OPCODE_POP, 0, NULL);
                    if(ip < 0)
                    {
                        return false;
                    }
                }
                if(forloop->init)
                {
                    afterupdateip = ape_compiler_getip(comp);
                    ape_compiler_moduint16operand(comp, jumptoafterupdateip + 1, afterupdateip);
                }
                /* test */
                if(forloop->test)
                {
                    ok = ape_compiler_compileexpression(comp, forloop->test);
                    if(!ok)
                    {
                        return false;
                    }
                }
                else
                {
                    ip = ape_compiler_emit(comp, APE_OPCODE_TRUE, 0, NULL);
                    if(ip < 0)
                    {
                        return false;
                    }
                }
                aftertestip = ape_compiler_getip(comp);
                ip = ape_compiler_emit(comp, APE_OPCODE_JUMPIFTRUE, 1, make_u64_array((ApeOpByte_t)(aftertestip + 6)));
                if(ip < 0)
                {
                    return false;
                }
                jmptoafterbodyip= ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((ApeOpByte_t)0xdead));
                if(jmptoafterbodyip < 0)
                {
                    return false;
                }
                /* body */
                ok = ape_compiler_pushcontip(comp, updateip);
                if(!ok)
                {
                    return false;
                }
                ok = ape_compiler_pushbreakip(comp, jmptoafterbodyip);
                if(!ok)
                {
                    return false;
                }
                ok = ape_compiler_compilecodeblock(comp, forloop->body);
                if(!ok)
                {
                    return false;
                }
                ape_compiler_popbreakip(comp);
                ape_compiler_popcontip(comp);
                ip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((ApeOpByte_t)updateip));
                if(ip < 0)
                {
                    return false;
                }
                afterbodyip = ape_compiler_getip(comp);
                ape_compiler_moduint16operand(comp, jmptoafterbodyip + 1, afterbodyip);
                ape_symtable_popblockscope(symtable);
            }
            break;
        case APE_EXPR_BLOCK:
            {
                ok = ape_compiler_compilecodeblock(comp, stmt->exblock);
                if(!ok)
                {
                    return false;
                }
            }
            break;
        case APE_EXPR_INCLUDE:
            {
                ok = ape_compiler_includemodule(comp, stmt);
                if(!ok)
                {
                    return false;
                }
            }
            break;
        case APE_EXPR_RECOVER:
            {
                recover = &stmt->exrecoverstmt;
                if(ape_symtable_ismoduleglobalscope(symtable))
                {
                    ape_errorlist_add(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "recover statement cannot be defined in global scope");
                    return false;
                }
                if(!ape_symtable_istopblockscope(symtable))
                {
                    ape_errorlist_add(comp->errors, APE_ERROR_COMPILATION, stmt->pos,
                                     "recover statement cannot be defined within other statements");
                    return false;
                }
                recoverip = ape_compiler_emit(comp, APE_OPCODE_SETRECOVER, 1, make_u64_array((ApeOpByte_t)0xbeef));
                if(recoverip < 0)
                {
                    return false;
                }
                jumptoafterrecoverip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((ApeOpByte_t)0xbeef));
                if(jumptoafterrecoverip < 0)
                {
                    return false;
                }
                afterjumptorecoverip = ape_compiler_getip(comp);
                ape_compiler_moduint16operand(comp, recoverip + 1, afterjumptorecoverip);
                ok = ape_symtable_pushblockscope(symtable);
                if(!ok)
                {
                    return false;
                }
                errorsymbol = ape_compiler_definesym(comp, recover->errorident->pos, recover->errorident->value, false, false);
                if(!errorsymbol)
                {
                    return false;
                }
                ok = ape_compiler_writesym(comp, errorsymbol, true);
                if(!ok)
                {
                    return false;
                }
                ok = ape_compiler_compilecodeblock(comp, recover->body);
                if(!ok)
                {
                    return false;
                }
                if(!ape_compiler_lastopcodeis(comp, APE_OPCODE_RETURNNOTHING) && !ape_compiler_lastopcodeis(comp, APE_OPCODE_RETURNVALUE))
                {
                    ape_errorlist_add(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "recover body must end with a return statement");
                    return false;
                }
                ape_symtable_popblockscope(symtable);
                afterrecoverip = ape_compiler_getip(comp);
                ape_compiler_moduint16operand(comp, jumptoafterrecoverip + 1, afterrecoverip);
            }
            break;
        default:
            {
                /*
                APE_ASSERT(false);
                return false;
                */
                return ape_compiler_compileexpression(comp, stmt);
            }
            break;
    }
    ape_valarray_pop(comp->srcpositionsstack);
    return true;
}

bool ape_compiler_compileexpression(ApeAstCompiler_t* comp, ApeAstExpression_t* expr)
{
    bool ok;
    bool res;
    bool rearrange;
    ApeAstAssignExpr_t* assign;
    ApeAstCompResult_t* compres;
    ApeAstCompScope_t* compscope;
    ApeAstExpression_t* argexpr;
    ApeAstExpression_t* exproptimized;
    ApeAstExpression_t* key;
    ApeAstExpression_t* left;
    ApeAstExpression_t* right;
    ApeAstExpression_t* val;
    ApeFloat_t number;
    ApeAstLiteralFuncExpr_t* fn;
    ApeAstIdentExpr_t* ident;
    ApeAstIdentExpr_t* param;
    ApeAstIndexExpr_t* index;
    ApeInt_t afterleftjumpip;
    ApeInt_t afterrightip;
    ApeInt_t elseip;
    ApeInt_t elsejumpip;
    ApeInt_t endip;
    ApeInt_t endjumpip;
    ApeInt_t ip;
    ApeInt_t numlocals;
    ApeInt_t pos;
    ApeInt_t* currentpos;
    ApeInt_t* posval;
    ApeAstLogicalExpr_t* logi;
    ApeAstLiteralMapExpr_t* map;
    ApeObject_t obj;
    ApeOpByte_t op;
    ApePtrArray_t* freesymbols;
    ApeSize_t i;
    ApeSize_t len;
    ApeSymTable_t* symtable;
    ApeSymbol_t* fnsymbol;
    ApeSymbol_t* paramsymbol;
    ApeSymbol_t* symbol;
    ApeSymbol_t* thissymbol;
    ApeAstTernaryExpr_t* ternary;
    ok = false;
    ip = -1;
    exproptimized = ape_optimizer_optexpr(expr);
    if(exproptimized)
    {
        expr = exproptimized;
    }
    ok = ape_valarray_push(comp->srcpositionsstack, &expr->pos);
    if(!ok)
    {
        return false;
    }
    compscope = ape_compiler_getcompscope(comp);
    symtable = ape_compiler_getsymboltable(comp);
    res = false;
    switch(expr->extype)
    {
        case APE_EXPR_INFIX:
            {
                rearrange = false;
                op = APE_OPCODE_NONE;
                switch(expr->exinfix.op)
                {
                    case APE_OPERATOR_PLUS:
                        {
                            op = APE_OPCODE_ADD;
                        }
                        break;
                    case APE_OPERATOR_MINUS:
                        {
                            op = APE_OPCODE_SUB;
                        }
                        break;
                    case APE_OPERATOR_STAR:
                        {
                            op = APE_OPCODE_MUL;
                        }
                        break;
                    case APE_OPERATOR_SLASH:
                        {
                            op = APE_OPCODE_DIV;
                        }
                        break;
                    case APE_OPERATOR_MODULUS:
                        {
                            op = APE_OPCODE_MOD;
                        }
                        break;
                    case APE_OPERATOR_EQUAL:
                        {
                            op = APE_OPCODE_ISEQUAL;
                        }
                        break;
                    case APE_OPERATOR_NOTEQUAL:
                        {
                            op = APE_OPCODE_NOTEQUAL;
                        }
                        break;
                    case APE_OPERATOR_GREATERTHAN:
                        {
                            op = APE_OPCODE_GREATERTHAN;
                        }
                        break;
                    case APE_OPERATOR_GREATEREQUAL:
                        {
                            op = APE_OPCODE_GREATEREQUAL;
                        }
                        break;
                    case APE_OPERATOR_LESSTHAN:
                        {
                            op = APE_OPCODE_GREATERTHAN;
                            rearrange = true;
                        }
                        break;
                    case APE_OPERATOR_LESSEQUAL:
                        {
                            op = APE_OPCODE_GREATEREQUAL;
                            rearrange = true;
                        }
                        break;
                    case APE_OPERATOR_BITOR:
                        {
                            op = APE_OPCODE_BITOR;
                        }
                        break;
                    case APE_OPERATOR_BITXOR:
                        {
                            op = APE_OPCODE_BITXOR;
                        }
                        break;
                    case APE_OPERATOR_BITAND:
                        {
                            op = APE_OPCODE_BITAND;
                        }
                        break;
                    case APE_OPERATOR_LEFTSHIFT:
                        {
                            op = APE_OPCODE_LEFTSHIFT;
                        }
                        break;
                    case APE_OPERATOR_RIGHTSHIFT:
                        {
                            op = APE_OPCODE_RIGHTSHIFT;
                        }
                        break;
                    default:
                        {
                            ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, expr->pos, "unknown infix operator");
                            goto error;
                        }
                        break;
                }
                left = rearrange ? expr->exinfix.right : expr->exinfix.left;
                right = rearrange ? expr->exinfix.left : expr->exinfix.right;
                ok = ape_compiler_compileexpression(comp, left);
                if(!ok)
                {
                    goto error;
                }
                ok = ape_compiler_compileexpression(comp, right);
                if(!ok)
                {
                    goto error;
                }
                switch(expr->exinfix.op)
                {
                    case APE_OPERATOR_EQUAL:
                    case APE_OPERATOR_NOTEQUAL:
                        {
                            ip = ape_compiler_emit(comp, APE_OPCODE_COMPAREEQUAL, 0, NULL);
                            if(ip < 0)
                            {
                                goto error;
                            }
                        }
                        break;
                    case APE_OPERATOR_GREATERTHAN:
                    case APE_OPERATOR_GREATEREQUAL:
                    case APE_OPERATOR_LESSTHAN:
                    case APE_OPERATOR_LESSEQUAL:
                        {
                            ip = ape_compiler_emit(comp, APE_OPCODE_COMPAREPLAIN, 0, NULL);
                            if(ip < 0)
                            {
                                goto error;
                            }
                        }
                        break;
                    default:
                        break;
                }
                ip = ape_compiler_emit(comp, op, 0, NULL);
                if(ip < 0)
                {
                    goto error;
                }
            }
            break;
        case APE_EXPR_LITERALNUMBER:
            {
                number = expr->exliteralnumber;
                ip = ape_compiler_emit(comp, APE_OPCODE_MKNUMBER, 1, make_u64_array((ApeOpByte_t)ape_util_floattouint(number)));
                if(ip < 0)
                {
                    goto error;
                }
            }
            break;
        case APE_EXPR_LITERALSTRING:
            {
                pos = 0;
                currentpos = (ApeInt_t*)ape_strdict_getbyname(comp->stringconstantspositions, expr->exliteralstring);
                if(currentpos)
                {
                    pos = *currentpos;
                }
                else
                {
                    obj = ape_object_make_stringlen(comp->context, expr->exliteralstring, expr->stringlitlength);
                    if(ape_object_value_isnull(obj))
                    {
                        goto error;
                    }
                    pos = ape_compiler_addconstant(comp, obj);
                    if(pos < 0)
                    {
                        goto error;
                    }
                    posval = (ApeInt_t*)ape_allocator_alloc(&comp->context->alloc, sizeof(ApeInt_t));
                    if(!posval)
                    {
                        goto error;
                    }
                    *posval = pos;
                    ok = ape_strdict_set(comp->stringconstantspositions, expr->exliteralstring, posval);
                    if(!ok)
                    {
                        ape_allocator_free(&comp->context->alloc, posval);
                        goto error;
                    }
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_CONSTANT, 1, make_u64_array((ApeOpByte_t)pos));
                if(ip < 0)
                {
                    goto error;
                }
            }
            break;
        case APE_EXPR_LITERALNULL:
            {
                ip = ape_compiler_emit(comp, APE_OPCODE_NULL, 0, NULL);
                if(ip < 0)
                {
                    goto error;
                }
            }
            break;
        case APE_EXPR_LITERALBOOL:
            {
                ip = ape_compiler_emit(comp, expr->exliteralbool ? APE_OPCODE_TRUE : APE_OPCODE_FALSE, 0, NULL);
                if(ip < 0)
                {
                    goto error;
                }
            }
            break;
        case APE_EXPR_LITERALARRAY:
            {
                for(i = 0; i < ape_ptrarray_count(expr->exarray); i++)
                {
                    ok = ape_compiler_compileexpression(comp, (ApeAstExpression_t*)ape_ptrarray_get(expr->exarray, i));
                    if(!ok)
                    {
                        goto error;
                    }
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_MKARRAY, 1, make_u64_array((ApeOpByte_t)ape_ptrarray_count(expr->exarray)));
                if(ip < 0)
                {
                    goto error;
                }
            }
            break;
        case APE_EXPR_LITERALMAP:
            {
                map = &expr->exmap;
                len = ape_ptrarray_count(map->keys);
                ip = ape_compiler_emit(comp, APE_OPCODE_MAPSTART, 1, make_u64_array((ApeOpByte_t)len));
                if(ip < 0)
                {
                    goto error;
                }
                for(i = 0; i < len; i++)
                {
                    key = (ApeAstExpression_t*)ape_ptrarray_get(map->keys, i);
                    val = (ApeAstExpression_t*)ape_ptrarray_get(map->values, i);
                    ok = ape_compiler_compileexpression(comp, key);
                    if(!ok)
                    {
                        goto error;
                    }
                    ok = ape_compiler_compileexpression(comp, val);
                    if(!ok)
                    {
                        goto error;
                    }
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_MAPEND, 1, make_u64_array((ApeOpByte_t)len));
                if(ip < 0)
                {
                    goto error;
                }
            }
            break;
        case APE_EXPR_PREFIX:
            {
                ok = ape_compiler_compileexpression(comp, expr->exprefix.right);
                if(!ok)
                {
                    goto error;
                }
                op = APE_OPCODE_NONE;
                switch(expr->exprefix.op)
                {
                    case APE_OPERATOR_MINUS:
                        {
                            op = APE_OPCODE_MINUS;
                        }
                        break;
                    case APE_OPERATOR_NOT:
                        {
                            op = APE_OPCODE_NOT;
                        }
                        break;
                    default:
                        {
                            ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, expr->pos, "unknown prefix operator.");
                            goto error;
                        }
                        break;
                }
                ip = ape_compiler_emit(comp, op, 0, NULL);
                if(ip < 0)
                {
                    goto error;
                }
            }
            break;
        case APE_EXPR_IDENT:
            {
                ident = expr->exident;
                symbol = ape_symtable_resolve(symtable, ident->value);
                if(!symbol)
                {
                    ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, ident->pos, "symbol '%s' could not be resolved", ident->value);
                    goto error;
                }
                ok = ape_compiler_readsym(comp, symbol);
                if(!ok)
                {
                    goto error;
                }
            }
            break;
        case APE_EXPR_INDEX:
            {
                index = &expr->exindex;
                ok = ape_compiler_compileexpression(comp, index->left);
                if(!ok)
                {
                    goto error;
                }
                ok = ape_compiler_compileexpression(comp, index->index);
                if(!ok)
                {
                    goto error;
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_GETINDEX, 0, NULL);
                if(ip < 0)
                {
                    goto error;
                }
            }
            break;
        case APE_EXPR_LITERALFUNCTION:
            {
                fn = &expr->exliteralfunc;
                ok = ape_compiler_pushcompscope(comp);
                if(!ok)
                {
                    goto error;
                }
                ok = ape_compiler_pushsymtable(comp, 0);
                if(!ok)
                {
                    goto error;
                }
                compscope = ape_compiler_getcompscope(comp);
                symtable = ape_compiler_getsymboltable(comp);
                if(fn->name)
                {
                    fnsymbol = ape_symtable_definefuncname(symtable, fn->name, false);
                    if(!fnsymbol)
                    {
                        ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, expr->pos, "cannot define symbol '%s'", fn->name);
                        goto error;
                    }
                }
                thissymbol = ape_symtable_definethis(symtable);
                if(!thissymbol)
                {
                    ape_errorlist_add(comp->errors, APE_ERROR_COMPILATION, expr->pos, "cannot define symbol 'this'");
                    goto error;
                }
                for(i = 0; i < ape_ptrarray_count(expr->exliteralfunc.params); i++)
                {
                    param = (ApeAstIdentExpr_t*)ape_ptrarray_get(expr->exliteralfunc.params, i);
                    paramsymbol = ape_compiler_definesym(comp, param->pos, param->value, true, false);
                    if(!paramsymbol)
                    {
                        goto error;
                    }
                }
                ok = ape_compiler_compilestmtlist(comp, fn->body->statements);
                if(!ok)
                {
                    goto error;
                }
                if(!ape_compiler_lastopcodeis(comp, APE_OPCODE_RETURNVALUE) && !ape_compiler_lastopcodeis(comp, APE_OPCODE_RETURNNOTHING))
                {
                    ip = ape_compiler_emit(comp, APE_OPCODE_RETURNNOTHING, 0, NULL);
                    if(ip < 0)
                    {
                        goto error;
                    }
                }
                freesymbols = symtable->freesymbols;
                /* because it gets destroyed with compiler_pop_compilation_scope() */
                symtable->freesymbols = NULL;
                numlocals = symtable->maxnumdefinitions;
                compres = ape_compscope_orphanresult(compscope);
                if(!compres)
                {
                    ape_ptrarray_destroywithitems(comp->context, freesymbols, (ApeDataCallback_t)ape_symbol_destroy);
                    goto error;
                }
                ape_compiler_popsymtable(comp);
                ape_compiler_popcompscope(comp);
                compscope = ape_compiler_getcompscope(comp);
                symtable = ape_compiler_getsymboltable(comp);
                obj = ape_object_make_function(comp->context, fn->name, compres, true, numlocals, ape_ptrarray_count(fn->params), 0);
                if(ape_object_value_isnull(obj))
                {
                    ape_ptrarray_destroywithitems(comp->context, freesymbols, (ApeDataCallback_t)ape_symbol_destroy);
                    ape_compresult_destroy(compres);
                    goto error;
                }
                for(i = 0; i < ape_ptrarray_count(freesymbols); i++)
                {
                    symbol = (ApeSymbol_t*)ape_ptrarray_get(freesymbols, i);
                    ok = ape_compiler_readsym(comp, symbol);
                    if(!ok)
                    {
                        ape_ptrarray_destroywithitems(comp->context, freesymbols, (ApeDataCallback_t)ape_symbol_destroy);
                        goto error;
                    }
                }
                pos = ape_compiler_addconstant(comp, obj);
                if(pos < 0)
                {
                    ape_ptrarray_destroywithitems(comp->context, freesymbols, (ApeDataCallback_t)ape_symbol_destroy);
                    goto error;
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_MKFUNCTION, 2, make_u64_array((ApeOpByte_t)pos, (ApeOpByte_t)ape_ptrarray_count(freesymbols)));
                if(ip < 0)
                {
                    ape_ptrarray_destroywithitems(comp->context, freesymbols, (ApeDataCallback_t)ape_symbol_destroy);
                    goto error;
                }
                ape_ptrarray_destroywithitems(comp->context, freesymbols, (ApeDataCallback_t)ape_symbol_destroy);
            }
            break;
        case APE_EXPR_CALL:
            {
                ok = ape_compiler_compileexpression(comp, expr->excall.function);
                if(!ok)
                {
                    goto error;
                }
                for(i = 0; i < ape_ptrarray_count(expr->excall.args); i++)
                {
                    argexpr = (ApeAstExpression_t*)ape_ptrarray_get(expr->excall.args, i);
                    ok = ape_compiler_compileexpression(comp, argexpr);
                    if(!ok)
                    {
                        goto error;
                    }
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_CALL, 1, make_u64_array((ApeOpByte_t)ape_ptrarray_count(expr->excall.args)));
                if(ip < 0)
                {
                    goto error;
                }
            }
            break;
        case APE_EXPR_ASSIGN:
            {
                assign = &expr->exassign;
                if(assign->dest->extype != APE_EXPR_IDENT && assign->dest->extype != APE_EXPR_INDEX)
                {
                    ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, assign->dest->pos, "expression is not assignable");
                    goto error;
                }
                if(assign->ispostfix)
                {
                    ok = ape_compiler_compileexpression(comp, assign->dest);
                    if(!ok)
                    {
                        goto error;
                    }
                }
                ok = ape_compiler_compileexpression(comp, assign->source);
                if(!ok)
                {
                    goto error;
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_DUP, 0, NULL);
                if(ip < 0)
                {
                    goto error;
                }
                ok = ape_valarray_push(comp->srcpositionsstack, &assign->dest->pos);
                if(!ok)
                {
                    goto error;
                }
                if(assign->dest->extype == APE_EXPR_IDENT)
                {
                    ident = assign->dest->exident;
                    symbol = ape_symtable_resolve(symtable, ident->value);
                    if(!symbol)
                    {
                        /*
                        ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, assign->dest->pos, "symbol '%s' could not be resolved", ident->value);
                        goto error;
                        ape_symtable_define(ApeSymTable_t* table, const char* name, bool assignable)
                        */
                        symbol = ape_symtable_define(symtable, ident->value, true);
                    }
                    if(!symbol->assignable)
                    {
                        ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, assign->dest->pos,
                                          "symbol '%s' is not assignable", ident->value);
                        goto error;
                    }
                    ok = ape_compiler_writesym(comp, symbol, false);
                    if(!ok)
                    {
                        goto error;
                    }
                }
                else if(assign->dest->extype == APE_EXPR_INDEX)
                {
                    index = &assign->dest->exindex;
                    ok = ape_compiler_compileexpression(comp, index->left);
                    if(!ok)
                    {
                        goto error;
                    }
                    ok = ape_compiler_compileexpression(comp, index->index);
                    if(!ok)
                    {
                        goto error;
                    }
                    ip = ape_compiler_emit(comp, APE_OPCODE_SETINDEX, 0, NULL);
                    if(ip < 0)
                    {
                        goto error;
                    }
                }
                if(assign->ispostfix)
                {
                    ip = ape_compiler_emit(comp, APE_OPCODE_POP, 0, NULL);
                    if(ip < 0)
                    {
                        goto error;
                    }
                }
                ape_valarray_pop(comp->srcpositionsstack);
            }
            break;
        case APE_EXPR_LOGICAL:
            {
                logi = &expr->exlogical;
                ok = ape_compiler_compileexpression(comp, logi->left);
                if(!ok)
                {
                    goto error;
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_DUP, 0, NULL);
                if(ip < 0)
                {
                    goto error;
                }
                afterleftjumpip = 0;
                if(logi->op == APE_OPERATOR_LOGICALAND)
                {
                    afterleftjumpip = ape_compiler_emit(comp, APE_OPCODE_JUMPIFFALSE, 1, make_u64_array((ApeOpByte_t)0xbeef));
                }
                else
                {
                    afterleftjumpip = ape_compiler_emit(comp, APE_OPCODE_JUMPIFTRUE, 1, make_u64_array((ApeOpByte_t)0xbeef));
                }
                if(afterleftjumpip < 0)
                {
                    goto error;
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_POP, 0, NULL);
                if(ip < 0)
                {
                    goto error;
                }
                ok = ape_compiler_compileexpression(comp, logi->right);
                if(!ok)
                {
                    goto error;
                }
                afterrightip = ape_compiler_getip(comp);
                ape_compiler_moduint16operand(comp, afterleftjumpip + 1, afterrightip);
            }
            break;
        case APE_EXPR_TERNARY:
            {
                ternary = &expr->externary;
                ok = ape_compiler_compileexpression(comp, ternary->test);
                if(!ok)
                {
                    goto error;
                }
                elsejumpip = ape_compiler_emit(comp, APE_OPCODE_JUMPIFFALSE, 1, make_u64_array((ApeOpByte_t)0xbeef));
                ok = ape_compiler_compileexpression(comp, ternary->iftrue);
                if(!ok)
                {
                    goto error;
                }
                endjumpip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((ApeOpByte_t)0xbeef));
                elseip = ape_compiler_getip(comp);
                ape_compiler_moduint16operand(comp, elsejumpip + 1, elseip);
                ok = ape_compiler_compileexpression(comp, ternary->iffalse);
                if(!ok)
                {
                    goto error;
                }
                endip = ape_compiler_getip(comp);
                ape_compiler_moduint16operand(comp, endjumpip + 1, endip);
            }
            break;
        default:
            {
                //APE_ASSERT(false);
                //static bool ape_compiler_compilestatement(ApeAstCompiler_t* comp, ApeAstExpression_t* stmt);
                return ape_compiler_compilestatement(comp, expr);
            }
            break;
    }
    res = true;
    goto end;
error:
    res = false;
end:
    ape_valarray_pop(comp->srcpositionsstack);
    ape_ast_destroy_expr(comp->context, exproptimized);
    return res;
}

bool ape_compiler_compilecodeblock(ApeAstCompiler_t* comp, ApeAstBlockExpr_t* block)
{
    bool ok;
    ApeInt_t ip;
    ApeSize_t i;
    ApeSymTable_t* symtable;
    ApeAstExpression_t* stmt;
    symtable = ape_compiler_getsymboltable(comp);
    if(!symtable)
    {
        return false;
    }
    ok = ape_symtable_pushblockscope(symtable);
    if(!ok)
    {
        return false;
    }
    if(ape_ptrarray_count(block->statements) == 0)
    {
        ip = ape_compiler_emit(comp, APE_OPCODE_NULL, 0, NULL);
        if(ip < 0)
        {
            return false;
        }
        ip = ape_compiler_emit(comp, APE_OPCODE_POP, 0, NULL);
        if(ip < 0)
        {
            return false;
        }
    }
    for(i = 0; i < ape_ptrarray_count(block->statements); i++)
    {
        stmt = (ApeAstExpression_t*)ape_ptrarray_get(block->statements, i);
        ok = ape_compiler_compilestatement(comp, stmt);
        if(!ok)
        {
            return false;
        }
    }
    ape_symtable_popblockscope(symtable);
    return true;
}

ApeInt_t ape_compiler_addconstant(ApeAstCompiler_t* comp, ApeObject_t obj)
{
    bool ok;
    ApeInt_t pos;
    ok = ape_valarray_push(comp->constants, &obj);
    if(!ok)
    {
        return -1;
    }
    pos = ape_valarray_count(comp->constants) - 1;
    return pos;
}

void ape_compiler_moduint16operand(ApeAstCompiler_t* comp, ApeInt_t ip, ApeOpByte_t operand)
{
    ApeUShort_t hi;
    ApeUShort_t lo;
    ApeValArray_t* bytecode;
    bytecode = ape_compiler_getbytecode(comp);
    if((ip + 1) >= (ApeInt_t)ape_valarray_count(bytecode))
    {
        APE_ASSERT(false);
        return;
    }
    hi = (ApeUShort_t)(operand >> 8);
    ape_valarray_set(bytecode, ip, &hi);
    lo = (ApeUShort_t)(operand);
    ape_valarray_set(bytecode, ip + 1, &lo);
}

bool ape_compiler_lastopcodeis(ApeAstCompiler_t* comp, ApeOpByte_t op)
{
    ApeOpByte_t lastopcode;
    lastopcode = ape_compiler_getlastopcode(comp);
    return lastopcode == op;
}

bool ape_compiler_readsym(ApeAstCompiler_t* comp, ApeSymbol_t* symbol)
{
    ApeInt_t ip;
    ip = -1;
    if(symbol->symtype == APE_SYMBOL_MODULEGLOBAL)
    {
        ip = ape_compiler_emit(comp, APE_OPCODE_GETMODULEGLOBAL, 1, make_u64_array((ApeOpByte_t)(symbol->index)));
    }
    else if(symbol->symtype == APE_SYMBOL_CONTEXTGLOBAL)
    {
        ip = ape_compiler_emit(comp, APE_OPCODE_GETCONTEXTGLOBAL, 1, make_u64_array((ApeOpByte_t)(symbol->index)));
    }
    else if(symbol->symtype == APE_SYMBOL_LOCAL)
    {
        ip = ape_compiler_emit(comp, APE_OPCODE_GETLOCAL, 1, make_u64_array((ApeOpByte_t)(symbol->index)));
    }
    else if(symbol->symtype == APE_SYMBOL_FREE)
    {
        ip = ape_compiler_emit(comp, APE_OPCODE_GETFREE, 1, make_u64_array((ApeOpByte_t)(symbol->index)));
    }
    else if(symbol->symtype == APE_SYMBOL_FUNCTION)
    {
        ip = ape_compiler_emit(comp, APE_OPCODE_CURRENTFUNCTION, 0, NULL);
    }
    else if(symbol->symtype == APE_SYMBOL_THIS)
    {
        ip = ape_compiler_emit(comp, APE_OPCODE_GETTHIS, 0, NULL);
    }
    return ip >= 0;
}

bool ape_compiler_writesym(ApeAstCompiler_t* comp, ApeSymbol_t* symbol, bool define)
{
    ApeInt_t ip;
    ip = -1;
    if(symbol->symtype == APE_SYMBOL_MODULEGLOBAL)
    {
        if(define)
        {
            ip = ape_compiler_emit(comp, APE_OPCODE_DEFMODULEGLOBAL, 1, make_u64_array((ApeOpByte_t)(symbol->index)));
        }
        else
        {
            ip = ape_compiler_emit(comp, APE_OPCODE_SETMODULEGLOBAL, 1, make_u64_array((ApeOpByte_t)(symbol->index)));
        }
    }
    else if(symbol->symtype == APE_SYMBOL_LOCAL)
    {
        if(define)
        {
            ip = ape_compiler_emit(comp, APE_OPCODE_DEFLOCAL, 1, make_u64_array((ApeOpByte_t)(symbol->index)));
        }
        else
        {
            ip = ape_compiler_emit(comp, APE_OPCODE_SETLOCAL, 1, make_u64_array((ApeOpByte_t)(symbol->index)));
        }
    }
    else if(symbol->symtype == APE_SYMBOL_FREE)
    {
        ip = ape_compiler_emit(comp, APE_OPCODE_SETFREE, 1, make_u64_array((ApeOpByte_t)(symbol->index)));
    }
    return ip >= 0;
}

bool ape_compiler_pushbreakip(ApeAstCompiler_t* comp, ApeInt_t ip)
{
    ApeAstCompScope_t* compscope;
    compscope = ape_compiler_getcompscope(comp);
    return ape_valarray_push(compscope->breakipstack, &ip);
}

void ape_compiler_popbreakip(ApeAstCompiler_t* comp)
{
    ApeAstCompScope_t* compscope;
    compscope = ape_compiler_getcompscope(comp);
    if(ape_valarray_count(compscope->breakipstack) == 0)
    {
        APE_ASSERT(false);
        return;
    }
    ape_valarray_pop(compscope->breakipstack);
}

ApeInt_t ape_compiler_getbreakip(ApeAstCompiler_t* comp)
{
    ApeInt_t* res;
    ApeAstCompScope_t* compscope;
    compscope = ape_compiler_getcompscope(comp);
    if(compscope == NULL)
    {
        return -1;
    }
    if(ape_valarray_count(compscope->breakipstack) == 0)
    {
        return -1;
    }
    res = (ApeInt_t*)ape_valarray_top(compscope->breakipstack);
    if(res != NULL)
    {
        return *res;
    }
    return -1;
}

bool ape_compiler_pushcontip(ApeAstCompiler_t* comp, ApeInt_t ip)
{
    ApeAstCompScope_t* compscope;
    compscope = ape_compiler_getcompscope(comp);
    return ape_valarray_push(compscope->continueipstack, &ip);
}

void ape_compiler_popcontip(ApeAstCompiler_t* comp)
{
    ApeAstCompScope_t* compscope;
    compscope = ape_compiler_getcompscope(comp);
    if(ape_valarray_count(compscope->continueipstack) == 0)
    {
        APE_ASSERT(false);
        return;
    }
    ape_valarray_pop(compscope->continueipstack);
}

ApeInt_t ape_compiler_getcontip(ApeAstCompiler_t* comp)
{
    ApeInt_t* res;
    ApeAstCompScope_t* compscope;
    compscope = ape_compiler_getcompscope(comp);
    if(ape_valarray_count(compscope->continueipstack) == 0)
    {
        APE_ASSERT(false);
        return -1;
    }
    res = (ApeInt_t*)ape_valarray_top(compscope->continueipstack);
    return *res;
}

ApeInt_t ape_compiler_getip(ApeAstCompiler_t* comp)
{
    ApeAstCompScope_t* compscope;
    compscope = ape_compiler_getcompscope(comp);
    return ape_valarray_count(compscope->bytecode);
}

ApeValArray_t* ape_compiler_getsrcpositions(ApeAstCompiler_t* comp)
{
    ApeAstCompScope_t* compscope = ape_compiler_getcompscope(comp);
    return compscope->srcpositions;
}

ApeValArray_t* ape_compiler_getbytecode(ApeAstCompiler_t* comp)
{
    ApeAstCompScope_t* compscope = ape_compiler_getcompscope(comp);
    return compscope->bytecode;
}

ApeAstFileScope_t* ape_compiler_makefilescope(ApeAstCompiler_t* comp, ApeAstCompFile_t* file)
{
    ApeAstFileScope_t* filescope;
    filescope = (ApeAstFileScope_t*)ape_allocator_alloc(&comp->context->alloc, sizeof(ApeAstFileScope_t));
    if(!filescope)
    {
        return NULL;
    }
    memset(filescope, 0, sizeof(ApeAstFileScope_t));
    filescope->context = comp->context;
    filescope->parser = ape_ast_make_parser(comp->context, comp->config, comp->errors);
    if(!filescope->parser)
    {
        goto err;
    }
    filescope->symtable = NULL;
    filescope->file = file;
    filescope->loadedmodulenames = ape_make_ptrarray(comp->context);
    if(!filescope->loadedmodulenames)
    {
        goto err;
    }
    return filescope;
err:
    ape_compiler_destroyfilescope(filescope);
    return NULL;
}

void ape_compiler_destroyfilescope(ApeAstFileScope_t* scope)
{
    ApeSize_t i;
    void* name;
    for(i = 0; i < ape_ptrarray_count(scope->loadedmodulenames); i++)
    {
        name = ape_ptrarray_get(scope->loadedmodulenames, i);
        ape_allocator_free(&scope->context->alloc, name);
    }
    ape_ptrarray_destroy(scope->loadedmodulenames);
    ape_parser_destroy(scope->parser);
    ape_allocator_free(&scope->context->alloc, scope);
}

bool ape_compiler_pushfilescope(ApeAstCompiler_t* comp, const char* filepath)
{
    bool ok;
    ApeAstBlockScope_t* prevsttopscope;
    ApeSymTable_t* prevst;
    ApeAstFileScope_t* filescope;
    ApeAstCompFile_t* file;
    ApeInt_t globaloffset;
    prevst = NULL;
    if(ape_ptrarray_count(comp->filescopes) > 0)
    {
        prevst = ape_compiler_getsymboltable(comp);
    }
    file = ape_make_compfile(comp->context, filepath);
    if(!file)
    {
        return false;
    }
    ok = ape_ptrarray_push(comp->files, &file);
    if(!ok)
    {
        ape_compfile_destroy(comp->context, file);
        return false;
    }
    filescope = ape_compiler_makefilescope(comp, file);
    if(!filescope)
    {
        return false;
    }
    ok = ape_ptrarray_push(comp->filescopes, &filescope);
    if(!ok)
    {
        ape_compiler_destroyfilescope(filescope);
        return false;
    }
    globaloffset = 0;
    if(prevst)
    {
        prevsttopscope = ape_symtable_getblockscope(prevst);
        globaloffset = prevsttopscope->offset + prevsttopscope->numdefinitions;
    }
    ok = ape_compiler_pushsymtable(comp, globaloffset);
    if(!ok)
    {
        ape_ptrarray_pop(comp->filescopes);
        ape_compiler_destroyfilescope(filescope);
        return false;
    }
    return true;
}

void ape_compiler_popfilescope(ApeAstCompiler_t* comp)
{
    ApeSymTable_t* currentst;
    ApeAstBlockScope_t* currentsttopscope;
    ApeSymTable_t* poppedst;
    ApeAstBlockScope_t* poppedsttopscope;
    ApeAstFileScope_t* scope;
    ApeInt_t poppednumdefs;
    poppedst = ape_compiler_getsymboltable(comp);
    poppedsttopscope = ape_symtable_getblockscope(poppedst);
    poppednumdefs = poppedsttopscope->numdefinitions;
    while(ape_compiler_getsymboltable(comp))
    {
        ape_compiler_popsymtable(comp);
    }
    scope = (ApeAstFileScope_t*)ape_ptrarray_top(comp->filescopes);
    if(!scope)
    {
        APE_ASSERT(false);
        return;
    }
    ape_compiler_destroyfilescope(scope);
    ape_ptrarray_pop(comp->filescopes);
    if(ape_ptrarray_count(comp->filescopes) > 0)
    {
        currentst = ape_compiler_getsymboltable(comp);
        currentsttopscope = ape_symtable_getblockscope(currentst);
        currentsttopscope->numdefinitions += poppednumdefs;
    }
}

void ape_compiler_setcompscope(ApeAstCompiler_t* comp, ApeAstCompScope_t* scope)
{
    comp->compilation_scope = scope;
}
