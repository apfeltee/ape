
#include "ape.h"

static const ApePosition_t gccprivsrcposinvalid = { NULL, -1, -1 };

static const ApeSymbol_t *ape_compiler_definesym(ApeCompiler_t *comp, ApePosition_t pos, const char *name, bool assignable, bool canshadow);
static void ape_compiler_setsymtable(ApeCompiler_t *comp, ApeSymbolTable_t *table);
static ApeInt_t ape_compiler_emit(ApeCompiler_t *comp, ApeOpByte_t op, ApeSize_t operandscount, uint64_t *operands);
static ApeCompilationScope_t *ape_compiler_getcompscope(ApeCompiler_t *comp);
static bool ape_compiler_pushcompscope(ApeCompiler_t *comp);
static void ape_compiler_popcompscope(ApeCompiler_t *comp);
static bool ape_compiler_pushsymtable(ApeCompiler_t *comp, ApeInt_t globaloffset);
static void ape_compiler_popsymtable(ApeCompiler_t *comp);
static ApeOpByte_t ape_compiler_getlastopcode(ApeCompiler_t *comp);
static bool ape_compiler_compilecode(ApeCompiler_t *comp, const char *code);
static bool ape_compiler_compilestmtlist(ApeCompiler_t *comp, ApePtrArray_t *statements);
static bool ape_compiler_importmodule(ApeCompiler_t *comp, const ApeStatement_t *importstmt);
static bool ape_compiler_compilestatement(ApeCompiler_t *comp, const ApeStatement_t *stmt);
static bool ape_compiler_compileexpression(ApeCompiler_t *comp, ApeExpression_t *expr);
static bool ape_compiler_compilecodeblock(ApeCompiler_t *comp, const ApeCodeblock_t *block);
static ApeInt_t ape_compiler_addconstant(ApeCompiler_t *comp, ApeObject_t obj);
static void ape_compiler_moduint16operand(ApeCompiler_t *comp, ApeInt_t ip, uint64_t operand);
static bool ape_compiler_lastopcodeis(ApeCompiler_t *comp, ApeOpByte_t op);
static bool ape_compiler_readsym(ApeCompiler_t *comp, const ApeSymbol_t *symbol);
static bool ape_compiler_writesym(ApeCompiler_t *comp, const ApeSymbol_t *symbol, bool define);
static bool ape_compiler_pushbreakip(ApeCompiler_t *comp, ApeInt_t ip);
static void ape_compiler_popbreakip(ApeCompiler_t *comp);
static ApeInt_t ape_compiler_getbreakip(ApeCompiler_t *comp);
static bool ape_compiler_pushcontip(ApeCompiler_t *comp, ApeInt_t ip);
static void ape_compiler_popcontip(ApeCompiler_t *comp);
static ApeInt_t ape_compiler_getcontip(ApeCompiler_t *comp);
static ApeInt_t ape_compiler_getip(ApeCompiler_t *comp);
static ApeArray_t *ape_compiler_getsrcpositions(ApeCompiler_t *comp);
static ApeArray_t *ape_compiler_getbytecode(ApeCompiler_t *comp);
static ApeFileScope_t *ape_compiler_makefilescope(ApeCompiler_t *comp, ApeCompiledFile_t *file);
static void ape_compiler_destroyfilescope(ApeFileScope_t *scope);
static bool ape_compiler_pushfilescope(ApeCompiler_t *comp, const char *filepath);
static void ape_compiler_popfilescope(ApeCompiler_t *comp);
static void ape_compiler_setcompscope(ApeCompiler_t *comp, ApeCompilationScope_t *scope);


static const ApeSymbol_t* ape_compiler_definesym(ApeCompiler_t* comp, ApePosition_t pos, const char* name, bool assignable, bool canshadow)
{
    ApeSymbolTable_t* symbol_table;
    const ApeSymbol_t* currentsymbol;
    const ApeSymbol_t* symbol;
    symbol_table = ape_compiler_getsymboltable(comp);
    if(!canshadow && !symbol_table_is_top_global_scope(symbol_table))
    {
        currentsymbol = symbol_table_resolve(symbol_table, name);
        if(currentsymbol)
        {
            errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, pos, "symbol '%s' is already defined", name);
            return NULL;
        }
    }
    symbol = symbol_table_define(symbol_table, name, assignable);
    if(!symbol)
    {
        errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, pos, "cannot define symbol '%s'", name);
        return NULL;
    }
    return symbol;
}


ApeCompiler_t* ape_compiler_make(ApeContext_t* ctx, const ApeConfig_t* config, ApeGCMemory_t* mem, ApeErrorList_t* errors, ApePtrArray_t * files, ApeGlobalStore_t* globalstore)
{
    bool ok;
    ApeCompiler_t* comp;
    comp = (ApeCompiler_t*)allocator_malloc(&ctx->alloc, sizeof(ApeCompiler_t));
    if(!comp)
    {
        return NULL;
    }
    comp->context = ctx;
    ok = ape_compiler_init(comp, ctx, config, mem, errors, files, globalstore);
    if(!ok)
    {
        allocator_free(&ctx->alloc, comp);
        return NULL;
    }
    return comp;
}

void ape_compiler_destroy(ApeCompiler_t* comp)
{
    ApeAllocator_t* alloc;
    if(!comp)
    {
        return;
    }
    alloc = comp->alloc;
    ape_compiler_deinit(comp);
    allocator_free(alloc, comp);
}

ApeCompilationResult_t* ape_compiler_compilesource(ApeCompiler_t* comp, const char* code)
{
    bool ok;
    ApeCompiler_t compshallowcopy;
    ApeCompilationScope_t* compilation_scope;
    ApeCompilationResult_t* res;
    compilation_scope = ape_compiler_getcompscope(comp);
    APE_ASSERT(array_count(comp->srcpositionsstack) == 0);
    APE_ASSERT(array_count(compilation_scope->bytecode) == 0);
    APE_ASSERT(array_count(compilation_scope->breakipstack) == 0);
    APE_ASSERT(array_count(compilation_scope->continueipstack) == 0);
    array_clear(comp->srcpositionsstack);
    array_clear(compilation_scope->bytecode);
    array_clear(compilation_scope->srcpositions);
    array_clear(compilation_scope->breakipstack);
    array_clear(compilation_scope->continueipstack);
    ok = ape_compiler_initshallowcopy(&compshallowcopy, comp);
    if(!ok)
    {
        return NULL;
    }
    ok = ape_compiler_compilecode(comp, code);
    if(!ok)
    {
        goto err;
    }
    compilation_scope = ape_compiler_getcompscope(comp);// might've changed
    APE_ASSERT(compilation_scope->outer == NULL);
    compilation_scope = ape_compiler_getcompscope(comp);
    res = compilation_scope_orphan_result(compilation_scope);
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

ApeCompilationResult_t* ape_compiler_compilefile(ApeCompiler_t* comp, const char* path)
{
    bool ok;
    char* code;
    ApeCompiledFile_t* file;
    ApeCompilationResult_t* res;
    ApeFileScope_t* filescope;
    ApeCompiledFile_t* prevfile;
    code = NULL;
    file = NULL;
    res = NULL;
    if(!comp->config->fileio.read_file.read_file)
    {// todo: read code function
        errorlist_add(comp->errors, APE_ERROR_COMPILATION, gccprivsrcposinvalid, "file read function not configured");
        goto err;
    }
    code = comp->config->fileio.read_file.read_file(comp->config->fileio.read_file.context, path);
    if(!code)
    {
        errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, gccprivsrcposinvalid, "reading file '%s' failed", path);
        goto err;
    }
    file = compiled_file_make(comp->context, path);
    if(!file)
    {
        goto err;
    }
    ok = ptrarray_add(comp->files, file);
    if(!ok)
    {
        compiled_file_destroy(file);
        goto err;
    }
    APE_ASSERT(ptrarray_count(comp->filescopes) == 1);
    filescope = (ApeFileScope_t*)ptrarray_top(comp->filescopes);
    if(!filescope)
    {
        goto err;
    }
    prevfile = filescope->file;// todo: push file scope instead?
    filescope->file = file;
    res = ape_compiler_compilesource(comp, code);
    if(!res)
    {
        filescope->file = prevfile;
        goto err;
    }
    filescope->file = prevfile;
    allocator_free(comp->alloc, code);
    return res;
err:
    allocator_free(comp->alloc, code);
    return NULL;
}

ApeSymbolTable_t* ape_compiler_getsymboltable(ApeCompiler_t* comp)
{
    ApeFileScope_t* filescope;
    filescope = (ApeFileScope_t*)ptrarray_top(comp->filescopes);
    if(!filescope)
    {
        APE_ASSERT(false);
        return NULL;
    }
    return filescope->symbol_table;
}

static void ape_compiler_setsymtable(ApeCompiler_t* comp, ApeSymbolTable_t* table)
{
    ApeFileScope_t* filescope;
    filescope = (ApeFileScope_t*)ptrarray_top(comp->filescopes);
    if(!filescope)
    {
        APE_ASSERT(false);
        return;
    }
    filescope->symbol_table = table;
}

ApeArray_t* ape_compiler_getconstants(const ApeCompiler_t* comp)
{
    return comp->constants;
}

// INTERNAL
bool ape_compiler_init(ApeCompiler_t* comp,
                          ApeContext_t* ctx,
                          const ApeConfig_t* config,
                          ApeGCMemory_t* mem,
                          ApeErrorList_t* errors,
                          ApePtrArray_t * files,
                          ApeGlobalStore_t* globalstore)
{
    bool ok;
    memset(comp, 0, sizeof(ApeCompiler_t));
    comp->context = ctx;
    comp->alloc = &ctx->alloc;
    comp->config = config;
    comp->mem = mem;
    comp->errors = errors;
    comp->files = files;
    comp->globalstore = globalstore;
    comp->filescopes = ptrarray_make(ctx);
    if(!comp->filescopes)
    {
        goto err;
    }
    comp->constants = array_make(ctx, ApeObject_t);
    if(!comp->constants)
    {
        goto err;
    }
    comp->srcpositionsstack = array_make(ctx, ApePosition_t);
    if(!comp->srcpositionsstack)
    {
        goto err;
    }
    comp->modules = strdict_make(ctx, (ApeDataCallback_t)module_copy, (ApeDataCallback_t)module_destroy);
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
    comp->stringconstantspositions = strdict_make(ctx, NULL, NULL);
    if(!comp->stringconstantspositions)
    {
        goto err;
    }

    return true;
err:
    ape_compiler_deinit(comp);
    return false;
}

void ape_compiler_deinit(ApeCompiler_t* comp)
{
    ApeSize_t i;
    ApeInt_t* val;
    if(!comp)
    {
        return;
    }
    for(i = 0; i < strdict_count(comp->stringconstantspositions); i++)
    {
        val = (ApeInt_t*)strdict_getvalueat(comp->stringconstantspositions, i);
        allocator_free(comp->alloc, val);
    }
    strdict_destroy(comp->stringconstantspositions);
    while(ptrarray_count(comp->filescopes) > 0)
    {
        ape_compiler_popfilescope(comp);
    }
    while(ape_compiler_getcompscope(comp))
    {
        ape_compiler_popcompscope(comp);
    }
    strdict_destroywithitems(comp->modules);
    array_destroy(comp->srcpositionsstack);
    array_destroy(comp->constants);
    ptrarray_destroy(comp->filescopes);
    memset(comp, 0, sizeof(ApeCompiler_t));
}

bool ape_compiler_initshallowcopy(ApeCompiler_t* copy, ApeCompiler_t* src)
{
    ApeSize_t i;
    bool ok;
    ApeInt_t* val;
    ApeInt_t* valcopy;
    const char* key;
    const char* loadedname;
    char* loadednamecopy;
    ApeSymbolTable_t* srcst;
    ApeSymbolTable_t* srcstcopy;
    ApeSymbolTable_t* copyst;
    ApeStrDictionary_t* modulescopy;
    ApeArray_t* constantscopy;
    ApeFileScope_t* srcfilescope;
    ApeFileScope_t* copyfilescope;
    ApePtrArray_t* srcloadedmodulenames;
    ApePtrArray_t* copyloadedmodulenames;

    ok = ape_compiler_init(copy, src->context, src->config, src->mem, src->errors, src->files, src->globalstore);
    if(!ok)
    {
        return false;
    }
    copy->context = src->context;
    srcst = ape_compiler_getsymboltable(src);
    APE_ASSERT(ptrarray_count(src->filescopes) == 1);
    APE_ASSERT(srcst->outer == NULL);
    srcstcopy = symbol_table_copy(srcst);
    if(!srcstcopy)
    {
        goto err;
    }
    copyst = ape_compiler_getsymboltable(copy);
    symbol_table_destroy(copyst);
    copyst = NULL;
    ape_compiler_setsymtable(copy, srcstcopy);
    modulescopy = strdict_copywithitems(src->modules);
    if(!modulescopy)
    {
        goto err;
    }
    strdict_destroywithitems(copy->modules);
    copy->modules = modulescopy;
    constantscopy = array_copy(src->constants);
    if(!constantscopy)
    {
        goto err;
    }
    array_destroy(copy->constants);
    copy->constants = constantscopy;
    for(i = 0; i < strdict_count(src->stringconstantspositions); i++)
    {
        key = (const char*)strdict_getkeyat(src->stringconstantspositions, i);
        val = (ApeInt_t*)strdict_getvalueat(src->stringconstantspositions, i);
        valcopy = (ApeInt_t*)allocator_malloc(src->alloc, sizeof(ApeInt_t));
        if(!valcopy)
        {
            goto err;
        }
        *valcopy = *val;
        ok = strdict_set(copy->stringconstantspositions, key, valcopy);
        if(!ok)
        {
            allocator_free(src->alloc, valcopy);
            goto err;
        }
    }
    srcfilescope = (ApeFileScope_t*)ptrarray_top(src->filescopes);
    copyfilescope = (ApeFileScope_t*)ptrarray_top(copy->filescopes);
    srcloadedmodulenames = srcfilescope->loadedmodulenames;
    copyloadedmodulenames = copyfilescope->loadedmodulenames;
    for(i = 0; i < ptrarray_count(srcloadedmodulenames); i++)
    {

        loadedname = (const char*)ptrarray_get(srcloadedmodulenames, i);
        loadednamecopy = util_strdup(copy->context, loadedname);
        if(!loadednamecopy)
        {
            goto err;
        }
        ok = ptrarray_add(copyloadedmodulenames, loadednamecopy);
        if(!ok)
        {
            allocator_free(copy->alloc, loadednamecopy);
            goto err;
        }
    }

    return true;
err:
    ape_compiler_deinit(copy);
    return false;
}

static ApeInt_t ape_compiler_emit(ApeCompiler_t* comp, ApeOpByte_t op, ApeSize_t operandscount, uint64_t* operands)
{
    ApeInt_t i;
    ApeInt_t ip;
    ApeInt_t len;
    bool ok;
    ApePosition_t* srcpos;
    ApeCompilationScope_t* compilation_scope;
    ip = ape_compiler_getip(comp);
    len = code_make(op, operandscount, operands, ape_compiler_getbytecode(comp));
    if(len == 0)
    {
        return -1;
    }
    for(i = 0; i < len; i++)
    {
        srcpos = (ApePosition_t*)array_top(comp->srcpositionsstack);
        APE_ASSERT(srcpos->line >= 0);
        APE_ASSERT(srcpos->column >= 0);
        ok = array_add(ape_compiler_getsrcpositions(comp), srcpos);
        if(!ok)
        {
            return -1;
        }
    }
    compilation_scope = ape_compiler_getcompscope(comp);
    compilation_scope->lastopcode = op;
    return ip;
}

static ApeCompilationScope_t* ape_compiler_getcompscope(ApeCompiler_t* comp)
{
    return comp->compilation_scope;
}

static bool ape_compiler_pushcompscope(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* currentscope;
    ApeCompilationScope_t* newscope;
    currentscope = ape_compiler_getcompscope(comp);
    newscope = compilation_scope_make(comp->context, currentscope);
    if(!newscope)
    {
        return false;
    }
    ape_compiler_setcompscope(comp, newscope);
    return true;
}

static void ape_compiler_popcompscope(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* currentscope;
    currentscope = ape_compiler_getcompscope(comp);
    APE_ASSERT(currentscope);
    ape_compiler_setcompscope(comp, currentscope->outer);
    compilation_scope_destroy(currentscope);
}

static bool ape_compiler_pushsymtable(ApeCompiler_t* comp, ApeInt_t globaloffset)
{
    ApeFileScope_t* filescope;
    filescope = (ApeFileScope_t*)ptrarray_top(comp->filescopes);
    if(!filescope)
    {
        APE_ASSERT(false);
        return false;
    }
    ApeSymbolTable_t* currenttable = filescope->symbol_table;
    filescope->symbol_table = symbol_table_make(comp->context, currenttable, comp->globalstore, globaloffset);
    if(!filescope->symbol_table)
    {
        filescope->symbol_table = currenttable;
        return false;
    }
    return true;
}

static void ape_compiler_popsymtable(ApeCompiler_t* comp)
{
    ApeFileScope_t* filescope;
    ApeSymbolTable_t* currenttable;
    filescope = (ApeFileScope_t*)ptrarray_top(comp->filescopes);
    if(!filescope)
    {
        APE_ASSERT(false);
        return;
    }
    currenttable = filescope->symbol_table;
    if(!currenttable)
    {
        APE_ASSERT(false);
        return;
    }
    filescope->symbol_table = currenttable->outer;
    symbol_table_destroy(currenttable);
}

static ApeOpByte_t ape_compiler_getlastopcode(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* currentscope;
    currentscope = ape_compiler_getcompscope(comp);
    return currentscope->lastopcode;
}

static bool ape_compiler_compilecode(ApeCompiler_t* comp, const char* code)
{
    bool ok;
    ApeFileScope_t* filescope;
    ApePtrArray_t* statements;
    filescope = (ApeFileScope_t*)ptrarray_top(comp->filescopes);
    APE_ASSERT(filescope);
    statements = ape_parser_parseall(filescope->parser, code, filescope->file);
    if(!statements)
    {
        // errors are added by parser
        return false;
    }
    if(comp->context->config.dumpast)
    {
        context_dumpast(comp->context, statements);
    }
    ok = ape_compiler_compilestmtlist(comp, statements);
    ptrarray_destroywithitems(statements, (ApeDataCallback_t)ape_ast_destroy_stmt);

    // Left for debugging purposes
    //    if (ok) {
    //        ApeWriter_t *buf = writer_make(NULL);
    //        code_tostring(array_data(comp->compilation_scope->bytecode),
    //                       array_data(comp->compilation_scope->srcpositions),
    //                       array_count(comp->compilation_scope->bytecode), buf);
    //        puts(writer_getdata(buf));
    //        writer_destroy(buf);
    //    }

    return ok;
}

static bool ape_compiler_compilestmtlist(ApeCompiler_t* comp, ApePtrArray_t * statements)
{
    ApeSize_t i;
    bool ok;
    const ApeStatement_t* stmt;
    ok = true;
    for(i = 0; i < ptrarray_count(statements); i++)
    {
        stmt = (const ApeStatement_t*)ptrarray_get(statements, i);
        ok = ape_compiler_compilestatement(comp, stmt);
        if(!ok)
        {
            break;
        }
    }
    return ok;
}

static bool ape_compiler_importmodule(ApeCompiler_t* comp, const ApeStatement_t* importstmt)
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
    ApeSymbolTable_t* symbol_table;
    ApeFileScope_t* fs;
    ApeModule_t* module;
    ApeSymbolTable_t* st;
    ApeSymbol_t* symbol;
    result = false;
    filepath = NULL;
    code = NULL;
    filescope = (ApeFileScope_t*)ptrarray_top(comp->filescopes);
    modulepath = importstmt->import.path;
    module_name = get_module_name(modulepath);
    for(i = 0; i < ptrarray_count(filescope->loadedmodulenames); i++)
    {
        loadedname = (const char*)ptrarray_get(filescope->loadedmodulenames, i);
        if(util_strequal(loadedname, module_name))
        {
            errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, importstmt->pos, "module '%s' was already imported", module_name);
            result = false;
            goto end;
        }
    }
    filepathbuf = writer_make(comp->context);
    if(!filepathbuf)
    {
        result = false;
        goto end;
    }
    if(util_is_absolutepath(modulepath))
    {
        writer_appendf(filepathbuf, "%s.ape", modulepath);
    }
    else
    {
        writer_appendf(filepathbuf, "%s%s.ape", filescope->file->dirpath, modulepath);
    }
    if(writer_failed(filepathbuf))
    {
        writer_destroy(filepathbuf);
        result = false;
        goto end;
    }
    filepathnoncanonicalised = writer_getdata(filepathbuf);
    filepath = util_canonicalisepath(comp->context, filepathnoncanonicalised);
    writer_destroy(filepathbuf);
    if(!filepath)
    {
        result = false;
        goto end;
    }
    symbol_table = ape_compiler_getsymboltable(comp);
    if(symbol_table->outer != NULL || ptrarray_count(symbol_table->blockscopes) > 1)
    {
        errorlist_add(comp->errors, APE_ERROR_COMPILATION, importstmt->pos, "modules can only be imported in global scope");
        result = false;
        goto end;
    }
    for(i = 0; i < ptrarray_count(comp->filescopes); i++)
    {
        fs = (ApeFileScope_t*)ptrarray_get(comp->filescopes, i);
        if(APE_STREQ(fs->file->path, filepath))
        {
            errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, importstmt->pos, "cyclic reference of file '%s'", filepath);
            result = false;
            goto end;
        }
    }
    module = (ApeModule_t*)strdict_get(comp->modules, filepath);
    if(!module)
    {
        // todo: create new module function
        if(!comp->config->fileio.read_file.read_file)
        {
            errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, importstmt->pos,
                              "cannot import module '%s', file read function not configured", filepath);
            result = false;
            goto end;
        }
        code = comp->config->fileio.read_file.read_file(comp->config->fileio.read_file.context, filepath);
        if(!code)
        {
            errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, importstmt->pos, "reading module file '%s' failed", filepath);
            result = false;
            goto end;
        }
        module = module_make(comp->context, module_name);
        if(!module)
        {
            result = false;
            goto end;
        }
        ok = ape_compiler_pushfilescope(comp, filepath);
        if(!ok)
        {
            module_destroy(module);
            result = false;
            goto end;
        }
        ok = ape_compiler_compilecode(comp, code);
        if(!ok)
        {
            module_destroy(module);
            result = false;
            goto end;
        }
        st = ape_compiler_getsymboltable(comp);
        for(i = 0; i < symbol_table_get_module_global_symbol_count(st); i++)
        {
            symbol = (ApeSymbol_t*)symbol_table_get_module_global_symbol_at(st, i);
            module_add_symbol(module, symbol);
        }
        ape_compiler_popfilescope(comp);
        ok = strdict_set(comp->modules, filepath, module);
        if(!ok)
        {
            module_destroy(module);
            result = false;
            goto end;
        }
    }
    for(i = 0; i < ptrarray_count(module->symbols); i++)
    {
        symbol = (ApeSymbol_t*)ptrarray_get(module->symbols, i);
        ok = symbol_table_add_module_symbol(symbol_table, symbol);
        if(!ok)
        {
            result = false;
            goto end;
        }
    }
    namecopy = util_strdup(comp->context, module_name);
    if(!namecopy)
    {
        result = false;
        goto end;
    }
    ok = ptrarray_add(filescope->loadedmodulenames, namecopy);
    if(!ok)
    {
        allocator_free(comp->alloc, namecopy);
        result = false;
        goto end;
    }
    result = true;
end:
    allocator_free(comp->alloc, filepath);
    allocator_free(comp->alloc, code);
    return result;
}

static bool ape_compiler_compilestatement(ApeCompiler_t* comp, const ApeStatement_t* stmt)
{
    ApeSize_t i;
    ApeInt_t ip;
    ApeInt_t nextcasejumpip;
    ApeInt_t afteraltip;
    ApeInt_t afterelifip;
    ApeInt_t* pos;
    ApeInt_t jumptoendip;
    ApeInt_t beforetestip;
    ApeInt_t aftertestip;
    ApeInt_t jumptoafterbodyip;
    ApeInt_t afterbodyip;
    ApeInt_t breakip;
    bool ok;

    const ApeWhileLoopStmt_t* loop;
    ApeCompilationScope_t* compilation_scope;
    ApeSymbolTable_t* symbol_table;
    const ApeSymbol_t* symbol;
    const ApeIfStmt_t* ifstmt;
    ApeArray_t* jumptoendips;
    ApeIfCase_t* ifcase;

    breakip = 0;
    ok = false;
    ip = -1;
    ok = array_push(comp->srcpositionsstack, &stmt->pos);
    if(!ok)
    {
        return false;
    }
    compilation_scope = ape_compiler_getcompscope(comp);
    symbol_table = ape_compiler_getsymboltable(comp);
    switch(stmt->type)
    {
        case STATEMENT_EXPRESSION:
            {
                ok = ape_compiler_compileexpression(comp, stmt->expression);
                if(!ok)
                {
                    return false;
                }
                ip = ape_compiler_emit(comp, OPCODE_POP, 0, NULL);
                if(ip < 0)
                {
                    return false;
                }
            }
            break;

        case STATEMENT_DEFINE:
            {
                ok = ape_compiler_compileexpression(comp, stmt->define.value);
                if(!ok)
                {
                    return false;
                }
                symbol = ape_compiler_definesym(comp, stmt->define.name->pos, stmt->define.name->value, stmt->define.assignable, false);
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

        case STATEMENT_IF:
            {
                ifstmt = &stmt->ifstatement;
                jumptoendips = array_make(comp->context, ApeInt_t);
                if(!jumptoendips)
                {
                    goto statementiferror;
                }
                for(i = 0; i < ptrarray_count(ifstmt->cases); i++)
                {
                    ifcase = (ApeIfCase_t*)ptrarray_get(ifstmt->cases, i);
                    ok = ape_compiler_compileexpression(comp, ifcase->test);
                    if(!ok)
                    {
                        goto statementiferror;
                    }
                    nextcasejumpip = ape_compiler_emit(comp, OPCODE_JUMP_IF_FALSE, 1, (uint64_t[]){ (uint64_t)(0xbeef) });
                    ok = ape_compiler_compilecodeblock(comp, ifcase->consequence);
                    if(!ok)
                    {
                        goto statementiferror;
                    }
                    // don't ape_compiler_emit jump for the last statement
                    if(i < (ptrarray_count(ifstmt->cases) - 1) || ifstmt->alternative)
                    {

                        jumptoendip = ape_compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)(0xbeef) });
                        ok = array_add(jumptoendips, &jumptoendip);
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
                for(i = 0; i < array_count(jumptoendips); i++)
                {
                    pos = (ApeInt_t*)array_get(jumptoendips, i);
                    ape_compiler_moduint16operand(comp, *pos + 1, afteraltip);
                }
                array_destroy(jumptoendips);

                break;
            statementiferror:
                array_destroy(jumptoendips);
                return false;
            }
            break;
        case STATEMENT_RETURN_VALUE:
            {
                if(compilation_scope->outer == NULL)
                {
                    errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "nothing to return from");
                    return false;
                }
                ip = -1;
                if(stmt->returnvalue)
                {
                    ok = ape_compiler_compileexpression(comp, stmt->returnvalue);
                    if(!ok)
                    {
                        return false;
                    }
                    ip = ape_compiler_emit(comp, OPCODE_RETURN_VALUE, 0, NULL);
                }
                else
                {
                    ip = ape_compiler_emit(comp, OPCODE_RETURN, 0, NULL);
                }
                if(ip < 0)
                {
                    return false;
                }
            }
            break;
        case STATEMENT_WHILE_LOOP:
            {
                loop = &stmt->whileloop;
                beforetestip = ape_compiler_getip(comp);
                ok = ape_compiler_compileexpression(comp, loop->test);
                if(!ok)
                {
                    return false;
                }
                aftertestip = ape_compiler_getip(comp);
                ip = ape_compiler_emit(comp, OPCODE_JUMP_IF_TRUE, 1, (uint64_t[]){ (uint64_t)(aftertestip + 6) });
                if(ip < 0)
                {
                    return false;
                }
                jumptoafterbodyip = ape_compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)0xdead });
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
                ok = ape_compiler_compilecodeblock(comp, loop->body);
                if(!ok)
                {
                    return false;
                }
                ape_compiler_popbreakip(comp);
                ape_compiler_popcontip(comp);
                ip = ape_compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)beforetestip });
                if(ip < 0)
                {
                    return false;
                }
                afterbodyip = ape_compiler_getip(comp);
                ape_compiler_moduint16operand(comp, jumptoafterbodyip + 1, afterbodyip);
            }
            break;

        case STATEMENT_BREAK:
        {
            breakip = ape_compiler_getbreakip(comp);
            if(breakip < 0)
            {
                errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "nothing to break from");
                return false;
            }
            ip = ape_compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)breakip });
            if(ip < 0)
            {
                return false;
            }
            break;
        }
        case STATEMENT_CONTINUE:
        {
            ApeInt_t continueip = ape_compiler_getcontip(comp);
            if(continueip < 0)
            {
                errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "nothing to continue from");
                return false;
            }
            ip = ape_compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)continueip });
            if(ip < 0)
            {
                return false;
            }
            break;
        }
        case STATEMENT_FOREACH:
        {
            const ApeForeachStmt_t* foreach = &stmt->foreach;
            ok = symbol_table_push_block_scope(symbol_table);
            if(!ok)
            {
                return false;
            }

            // Init
            const ApeSymbol_t* indexsymbol = ape_compiler_definesym(comp, stmt->pos, "@i", false, true);
            if(!indexsymbol)
            {
                return false;
            }

            ip = ape_compiler_emit(comp, OPCODE_NUMBER, 1, (uint64_t[]){ (uint64_t)0 });
            if(ip < 0)
            {
                return false;
            }

            ok = ape_compiler_writesym(comp, indexsymbol, true);
            if(!ok)
            {
                return false;
            }

            const ApeSymbol_t* sourcesymbol = NULL;
            if(foreach->source->type == EXPRESSION_IDENT)
            {
                sourcesymbol = symbol_table_resolve(symbol_table, foreach->source->ident->value);
                if(!sourcesymbol)
                {
                    errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, foreach->source->pos,
                                      "symbol '%s' could not be resolved", foreach->source->ident->value);
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

            // Update
            ApeInt_t jumptoafterupdateip = ape_compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)0xbeef });
            if(jumptoafterupdateip < 0)
            {
                return false;
            }

            ApeInt_t updateip = ape_compiler_getip(comp);
            ok = ape_compiler_readsym(comp, indexsymbol);
            if(!ok)
            {
                return false;
            }

            ip = ape_compiler_emit(comp, OPCODE_NUMBER, 1, (uint64_t[]){ (uint64_t)util_double_to_uint64(1) });
            if(ip < 0)
            {
                return false;
            }

            ip = ape_compiler_emit(comp, OPCODE_ADD, 0, NULL);
            if(ip < 0)
            {
                return false;
            }

            ok = ape_compiler_writesym(comp, indexsymbol, false);
            if(!ok)
            {
                return false;
            }

            ApeInt_t afterupdateip = ape_compiler_getip(comp);
            ape_compiler_moduint16operand(comp, jumptoafterupdateip + 1, afterupdateip);

            // Test
            ok = array_push(comp->srcpositionsstack, &foreach->source->pos);
            if(!ok)
            {
                return false;
            }

            ok = ape_compiler_readsym(comp, sourcesymbol);
            if(!ok)
            {
                return false;
            }

            ip = ape_compiler_emit(comp, OPCODE_LEN, 0, NULL);
            if(ip < 0)
            {
                return false;
            }

            array_pop(comp->srcpositionsstack, NULL);
            ok = ape_compiler_readsym(comp, indexsymbol);
            if(!ok)
            {
                return false;
            }

            ip = ape_compiler_emit(comp, OPCODE_COMPARE, 0, NULL);
            if(ip < 0)
            {
                return false;
            }

            ip = ape_compiler_emit(comp, OPCODE_EQUAL, 0, NULL);
            if(ip < 0)
            {
                return false;
            }

            ApeInt_t aftertestip = ape_compiler_getip(comp);
            ip = ape_compiler_emit(comp, OPCODE_JUMP_IF_FALSE, 1, (uint64_t[]){ (uint64_t)(aftertestip + 6) });
            if(ip < 0)
            {
                return false;
            }

            ApeInt_t jumptoafterbodyip = ape_compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)0xdead });
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

            ip = ape_compiler_emit(comp, OPCODE_GET_VALUE_AT, 0, NULL);
            if(ip < 0)
            {
                return false;
            }

            const ApeSymbol_t* itersymbol = ape_compiler_definesym(comp, foreach->iterator->pos, foreach->iterator->value, false, false);
            if(!itersymbol)
            {
                return false;
            }

            ok = ape_compiler_writesym(comp, itersymbol, true);
            if(!ok)
            {
                return false;
            }

            // Body
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

            ip = ape_compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)updateip });
            if(ip < 0)
            {
                return false;
            }

            ApeInt_t afterbodyip = ape_compiler_getip(comp);
            ape_compiler_moduint16operand(comp, jumptoafterbodyip + 1, afterbodyip);

            symbol_table_pop_block_scope(symbol_table);
            break;
        }
        case STATEMENT_FOR_LOOP:
        {
            const ApeForLoopStmt_t* loop = &stmt->forloop;

            ok = symbol_table_push_block_scope(symbol_table);
            if(!ok)
            {
                return false;
            }

            // Init
            ApeInt_t jumptoafterupdateip = 0;
            bool ok = false;
            if(loop->init)
            {
                ok = ape_compiler_compilestatement(comp, loop->init);
                if(!ok)
                {
                    return false;
                }
                jumptoafterupdateip = ape_compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)0xbeef });
                if(jumptoafterupdateip < 0)
                {
                    return false;
                }
            }

            // Update
            ApeInt_t updateip = ape_compiler_getip(comp);
            if(loop->update)
            {
                ok = ape_compiler_compileexpression(comp, loop->update);
                if(!ok)
                {
                    return false;
                }
                ip = ape_compiler_emit(comp, OPCODE_POP, 0, NULL);
                if(ip < 0)
                {
                    return false;
                }
            }

            if(loop->init)
            {
                ApeInt_t afterupdateip = ape_compiler_getip(comp);
                ape_compiler_moduint16operand(comp, jumptoafterupdateip + 1, afterupdateip);
            }

            // Test
            if(loop->test)
            {
                ok = ape_compiler_compileexpression(comp, loop->test);
                if(!ok)
                {
                    return false;
                }
            }
            else
            {
                ip = ape_compiler_emit(comp, OPCODE_TRUE, 0, NULL);
                if(ip < 0)
                {
                    return false;
                }
            }
            ApeInt_t aftertestip = ape_compiler_getip(comp);

            ip = ape_compiler_emit(comp, OPCODE_JUMP_IF_TRUE, 1, (uint64_t[]){ (uint64_t)(aftertestip + 6) });
            if(ip < 0)
            {
                return false;
            }
            ApeInt_t jmptoafterbodyip = ape_compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)0xdead });
            if(jmptoafterbodyip < 0)
            {
                return false;
            }

            // Body
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

            ok = ape_compiler_compilecodeblock(comp, loop->body);
            if(!ok)
            {
                return false;
            }

            ape_compiler_popbreakip(comp);
            ape_compiler_popcontip(comp);

            ip = ape_compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)updateip });
            if(ip < 0)
            {
                return false;
            }

            ApeInt_t afterbodyip = ape_compiler_getip(comp);
            ape_compiler_moduint16operand(comp, jmptoafterbodyip + 1, afterbodyip);

            symbol_table_pop_block_scope(symbol_table);
            break;
        }
        case STATEMENT_BLOCK:
        {
            ok = ape_compiler_compilecodeblock(comp, stmt->block);
            if(!ok)
            {
                return false;
            }
            break;
        }
        case STATEMENT_IMPORT:
        {
            ok = ape_compiler_importmodule(comp, stmt);
            if(!ok)
            {
                return false;
            }
            break;
        }
        case STATEMENT_RECOVER:
        {
            const ApeRecoverStmt_t* recover = &stmt->recover;

            if(symbol_table_is_module_global_scope(symbol_table))
            {
                errorlist_add(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "recover statement cannot be defined in global scope");
                return false;
            }

            if(!symbol_table_is_top_block_scope(symbol_table))
            {
                errorlist_add(comp->errors, APE_ERROR_COMPILATION, stmt->pos,
                                 "recover statement cannot be defined within other statements");
                return false;
            }

            ApeInt_t recoverip = ape_compiler_emit(comp, OPCODE_SET_RECOVER, 1, (uint64_t[]){ (uint64_t)0xbeef });
            if(recoverip < 0)
            {
                return false;
            }

            ApeInt_t jumptoafterrecoverip = ape_compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)0xbeef });
            if(jumptoafterrecoverip < 0)
            {
                return false;
            }

            ApeInt_t afterjumptorecoverip = ape_compiler_getip(comp);
            ape_compiler_moduint16operand(comp, recoverip + 1, afterjumptorecoverip);

            ok = symbol_table_push_block_scope(symbol_table);
            if(!ok)
            {
                return false;
            }

            const ApeSymbol_t* errorsymbol
            = ape_compiler_definesym(comp, recover->errorident->pos, recover->errorident->value, false, false);
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

            if(!ape_compiler_lastopcodeis(comp, OPCODE_RETURN) && !ape_compiler_lastopcodeis(comp, OPCODE_RETURN_VALUE))
            {
                errorlist_add(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "recover body must end with a return statement");
                return false;
            }

            symbol_table_pop_block_scope(symbol_table);

            ApeInt_t afterrecoverip = ape_compiler_getip(comp);
            ape_compiler_moduint16operand(comp, jumptoafterrecoverip + 1, afterrecoverip);

            break;
        }
        default:
        {
            APE_ASSERT(false);
            return false;
        }
    }
    array_pop(comp->srcpositionsstack, NULL);
    return true;
}

static bool ape_compiler_compileexpression(ApeCompiler_t* comp, ApeExpression_t* expr)
{
    ApeSize_t i;
    bool ok = false;
    ApeInt_t ip = -1;

    ApeExpression_t* exproptimised = optimise_expression(expr);
    if(exproptimised)
    {
        expr = exproptimised;
    }

    ok = array_push(comp->srcpositionsstack, &expr->pos);
    if(!ok)
    {
        return false;
    }

    ApeCompilationScope_t* compilation_scope = ape_compiler_getcompscope(comp);
    ApeSymbolTable_t* symbol_table = ape_compiler_getsymboltable(comp);

    bool res = false;

    switch(expr->type)
    {
        case EXPRESSION_INFIX:
        {
            bool rearrange = false;

            ApeOpByte_t op = OPCODE_NONE;
            switch(expr->infix.op)
            {
                case OPERATOR_PLUS:
                    op = OPCODE_ADD;
                    break;
                case OPERATOR_MINUS:
                    op = OPCODE_SUB;
                    break;
                case OPERATOR_ASTERISK:
                    op = OPCODE_MUL;
                    break;
                case OPERATOR_SLASH:
                    op = OPCODE_DIV;
                    break;
                case OPERATOR_MODULUS:
                    op = OPCODE_MOD;
                    break;
                case OPERATOR_EQ:
                    op = OPCODE_EQUAL;
                    break;
                case OPERATOR_NOT_EQ:
                    op = OPCODE_NOT_EQUAL;
                    break;
                case OPERATOR_GT:
                    op = OPCODE_GREATER_THAN;
                    break;
                case OPERATOR_GTE:
                    op = OPCODE_GREATER_THAN_EQUAL;
                    break;
                case OPERATOR_LT:
                    op = OPCODE_GREATER_THAN;
                    rearrange = true;
                    break;
                case OPERATOR_LTE:
                    op = OPCODE_GREATER_THAN_EQUAL;
                    rearrange = true;
                    break;
                case OPERATOR_BIT_OR:
                    op = OPCODE_OR;
                    break;
                case OPERATOR_BIT_XOR:
                    op = OPCODE_XOR;
                    break;
                case OPERATOR_BIT_AND:
                    op = OPCODE_BIT_AND;
                    break;
                case OPERATOR_LSHIFT:
                    op = OPCODE_LSHIFT;
                    break;
                case OPERATOR_RSHIFT:
                    op = OPCODE_RSHIFT;
                    break;
                default:
                {
                    errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, expr->pos, "unknown infix operator");
                    goto error;
                }
            }

            ApeExpression_t* left = rearrange ? expr->infix.right : expr->infix.left;
            ApeExpression_t* right = rearrange ? expr->infix.left : expr->infix.right;

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

            switch(expr->infix.op)
            {
                case OPERATOR_EQ:
                case OPERATOR_NOT_EQ:
                {
                    ip = ape_compiler_emit(comp, OPCODE_COMPARE_EQ, 0, NULL);
                    if(ip < 0)
                    {
                        goto error;
                    }
                    break;
                }
                case OPERATOR_GT:
                case OPERATOR_GTE:
                case OPERATOR_LT:
                case OPERATOR_LTE:
                {
                    ip = ape_compiler_emit(comp, OPCODE_COMPARE, 0, NULL);
                    if(ip < 0)
                    {
                        goto error;
                    }
                    break;
                }
                default:
                    break;
            }

            ip = ape_compiler_emit(comp, op, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_NUMBER_LITERAL:
        {
            ApeFloat_t number = expr->numberliteral;
            ip = ape_compiler_emit(comp, OPCODE_NUMBER, 1, (uint64_t[]){ (uint64_t)util_double_to_uint64(number) });
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_STRING_LITERAL:
        {
            ApeInt_t pos = 0;
            ApeInt_t* currentpos = (ApeInt_t*)strdict_get(comp->stringconstantspositions, expr->stringliteral);
            if(currentpos)
            {
                pos = *currentpos;
            }
            else
            {
                ApeObject_t obj = object_make_string(comp->context, expr->stringliteral);
                if(object_value_isnull(obj))
                {
                    goto error;
                }

                pos = ape_compiler_addconstant(comp, obj);
                if(pos < 0)
                {
                    goto error;
                }

                ApeInt_t* posval = (ApeInt_t*)allocator_malloc(comp->alloc, sizeof(ApeInt_t));
                if(!posval)
                {
                    goto error;
                }

                *posval = pos;
                ok = strdict_set(comp->stringconstantspositions, expr->stringliteral, posval);
                if(!ok)
                {
                    allocator_free(comp->alloc, posval);
                    goto error;
                }
            }

            ip = ape_compiler_emit(comp, OPCODE_CONSTANT, 1, (uint64_t[]){ (uint64_t)pos });
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_NULL_LITERAL:
        {
            ip = ape_compiler_emit(comp, OPCODE_NULL, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }
            break;
        }
        case EXPRESSION_BOOL_LITERAL:
        {
            ip = ape_compiler_emit(comp, expr->boolliteral ? OPCODE_TRUE : OPCODE_FALSE, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }
            break;
        }
        case EXPRESSION_ARRAY_LITERAL:
        {
            for(i = 0; i < ptrarray_count(expr->array); i++)
            {
                ok = ape_compiler_compileexpression(comp, (ApeExpression_t*)ptrarray_get(expr->array, i));
                if(!ok)
                {
                    goto error;
                }
            }
            ip = ape_compiler_emit(comp, OPCODE_ARRAY, 1, (uint64_t[]){ (uint64_t)ptrarray_count(expr->array) });
            if(ip < 0)
            {
                goto error;
            }
            break;
        }
        case EXPRESSION_MAP_LITERAL:
        {
            const ApeMapLiteral_t* map = &expr->map;
            ApeSize_t len = ptrarray_count(map->keys);
            ip = ape_compiler_emit(comp, OPCODE_MAP_START, 1, (uint64_t[]){ (uint64_t)len });
            if(ip < 0)
            {
                goto error;
            }

            for(i = 0; i < len; i++)
            {
                ApeExpression_t* key = (ApeExpression_t*)ptrarray_get(map->keys, i);
                ApeExpression_t* val = (ApeExpression_t*)ptrarray_get(map->values, i);

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

            ip = ape_compiler_emit(comp, OPCODE_MAP_END, 1, (uint64_t[]){ (uint64_t)len });
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_PREFIX:
        {
            ok = ape_compiler_compileexpression(comp, expr->prefix.right);
            if(!ok)
            {
                goto error;
            }

            ApeOpByte_t op = OPCODE_NONE;
            switch(expr->prefix.op)
            {
                case OPERATOR_MINUS:
                    op = OPCODE_MINUS;
                    break;
                case OPERATOR_BANG:
                    op = OPCODE_BANG;
                    break;
                default:
                {
                    errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, expr->pos, "unknown prefix operator.");
                    goto error;
                }
            }
            ip = ape_compiler_emit(comp, op, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_IDENT:
        {
            const ApeIdent_t* ident = expr->ident;
            const ApeSymbol_t* symbol = symbol_table_resolve(symbol_table, ident->value);
            if(!symbol)
            {
                errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, ident->pos, "symbol '%s' could not be resolved",
                                  ident->value);
                goto error;
            }
            ok = ape_compiler_readsym(comp, symbol);
            if(!ok)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_INDEX:
        {
            const ApeIndexExpr_t* index = &expr->indexexpr;
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
            ip = ape_compiler_emit(comp, OPCODE_GET_INDEX, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_FUNCTION_LITERAL:
        {
            const ApeFnLiteral_t* fn = &expr->fnliteral;

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

            compilation_scope = ape_compiler_getcompscope(comp);
            symbol_table = ape_compiler_getsymboltable(comp);

            if(fn->name)
            {
                const ApeSymbol_t* fnsymbol = symbol_table_define_function_name(symbol_table, fn->name, false);
                if(!fnsymbol)
                {
                    errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, expr->pos, "cannot define symbol '%s'", fn->name);
                    goto error;
                }
            }

            const ApeSymbol_t* thissymbol = symbol_table_define_this(symbol_table);
            if(!thissymbol)
            {
                errorlist_add(comp->errors, APE_ERROR_COMPILATION, expr->pos, "cannot define symbol 'this'");
                goto error;
            }

            for(i = 0; i < ptrarray_count(expr->fnliteral.params); i++)
            {
                ApeIdent_t* param = (ApeIdent_t*)ptrarray_get(expr->fnliteral.params, i);
                const ApeSymbol_t* paramsymbol = ape_compiler_definesym(comp, param->pos, param->value, true, false);
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

            if(!ape_compiler_lastopcodeis(comp, OPCODE_RETURN_VALUE) && !ape_compiler_lastopcodeis(comp, OPCODE_RETURN))
            {
                ip = ape_compiler_emit(comp, OPCODE_RETURN, 0, NULL);
                if(ip < 0)
                {
                    goto error;
                }
            }

            ApePtrArray_t* freesymbols = symbol_table->freesymbols;
            symbol_table->freesymbols = NULL;// because it gets destroyed with compiler_pop_compilation_scope()

            ApeInt_t numlocals = symbol_table->maxnumdefinitions;

            ApeCompilationResult_t* compres = compilation_scope_orphan_result(compilation_scope);
            if(!compres)
            {
                ptrarray_destroywithitems(freesymbols, (ApeDataCallback_t)symbol_destroy);
                goto error;
            }
            ape_compiler_popsymtable(comp);
            ape_compiler_popcompscope(comp);
            compilation_scope = ape_compiler_getcompscope(comp);
            symbol_table = ape_compiler_getsymboltable(comp);

            ApeObject_t obj = object_make_function(comp->context, fn->name, compres, true, numlocals, ptrarray_count(fn->params), 0);

            if(object_value_isnull(obj))
            {
                ptrarray_destroywithitems(freesymbols, (ApeDataCallback_t)symbol_destroy);
                compilation_result_destroy(compres);
                goto error;
            }

            for(i = 0; i < ptrarray_count(freesymbols); i++)
            {
                ApeSymbol_t* symbol = (ApeSymbol_t*)ptrarray_get(freesymbols, i);
                ok = ape_compiler_readsym(comp, symbol);
                if(!ok)
                {
                    ptrarray_destroywithitems(freesymbols, (ApeDataCallback_t)symbol_destroy);
                    goto error;
                }
            }

            ApeInt_t pos = ape_compiler_addconstant(comp, obj);
            if(pos < 0)
            {
                ptrarray_destroywithitems(freesymbols, (ApeDataCallback_t)symbol_destroy);
                goto error;
            }

            ip = ape_compiler_emit(comp, OPCODE_FUNCTION, 2, (uint64_t[]){ (uint64_t)pos, (uint64_t)ptrarray_count(freesymbols) });
            if(ip < 0)
            {
                ptrarray_destroywithitems(freesymbols, (ApeDataCallback_t)symbol_destroy);
                goto error;
            }

            ptrarray_destroywithitems(freesymbols, (ApeDataCallback_t)symbol_destroy);

            break;
        }
        case EXPRESSION_CALL:
        {
            ok = ape_compiler_compileexpression(comp, expr->callexpr.function);
            if(!ok)
            {
                goto error;
            }

            for(i = 0; i < ptrarray_count(expr->callexpr.args); i++)
            {
                ApeExpression_t* argexpr = (ApeExpression_t*)ptrarray_get(expr->callexpr.args, i);
                ok = ape_compiler_compileexpression(comp, argexpr);
                if(!ok)
                {
                    goto error;
                }
            }

            ip = ape_compiler_emit(comp, OPCODE_CALL, 1, (uint64_t[]){ (uint64_t)ptrarray_count(expr->callexpr.args) });
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_ASSIGN:
        {
            const ApeAssignExpr_t* assign = &expr->assign;
            if(assign->dest->type != EXPRESSION_IDENT && assign->dest->type != EXPRESSION_INDEX)
            {
                errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, assign->dest->pos, "expression is not assignable");
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

            ip = ape_compiler_emit(comp, OPCODE_DUP, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }

            ok = array_push(comp->srcpositionsstack, &assign->dest->pos);
            if(!ok)
            {
                goto error;
            }

            if(assign->dest->type == EXPRESSION_IDENT)
            {
                const ApeIdent_t* ident = assign->dest->ident;
                const ApeSymbol_t* symbol = symbol_table_resolve(symbol_table, ident->value);
                if(!symbol)
                {
                    //errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, assign->dest->pos, "symbol '%s' could not be resolved", ident->value);
                    //goto error;
                    //symbol_table_define(ApeSymbolTable_t* table, const char* name, bool assignable)
                    symbol = symbol_table_define(symbol_table, ident->value, true);
                }
                if(!symbol->assignable)
                {
                    errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, assign->dest->pos,
                                      "symbol '%s' is not assignable", ident->value);
                    goto error;
                }
                ok = ape_compiler_writesym(comp, symbol, false);
                if(!ok)
                {
                    goto error;
                }
            }
            else if(assign->dest->type == EXPRESSION_INDEX)
            {
                const ApeIndexExpr_t* index = &assign->dest->indexexpr;
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
                ip = ape_compiler_emit(comp, OPCODE_SET_INDEX, 0, NULL);
                if(ip < 0)
                {
                    goto error;
                }
            }

            if(assign->ispostfix)
            {
                ip = ape_compiler_emit(comp, OPCODE_POP, 0, NULL);
                if(ip < 0)
                {
                    goto error;
                }
            }

            array_pop(comp->srcpositionsstack, NULL);
            break;
        }
        case EXPRESSION_LOGICAL:
        {
            const ApeLogicalExpr_t* logi = &expr->logical;

            ok = ape_compiler_compileexpression(comp, logi->left);
            if(!ok)
            {
                goto error;
            }

            ip = ape_compiler_emit(comp, OPCODE_DUP, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }

            ApeInt_t afterleftjumpip = 0;
            if(logi->op == OPERATOR_LOGICAL_AND)
            {
                afterleftjumpip = ape_compiler_emit(comp, OPCODE_JUMP_IF_FALSE, 1, (uint64_t[]){ (uint64_t)0xbeef });
            }
            else
            {
                afterleftjumpip = ape_compiler_emit(comp, OPCODE_JUMP_IF_TRUE, 1, (uint64_t[]){ (uint64_t)0xbeef });
            }

            if(afterleftjumpip < 0)
            {
                goto error;
            }

            ip = ape_compiler_emit(comp, OPCODE_POP, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }

            ok = ape_compiler_compileexpression(comp, logi->right);
            if(!ok)
            {
                goto error;
            }

            ApeInt_t afterrightip = ape_compiler_getip(comp);
            ape_compiler_moduint16operand(comp, afterleftjumpip + 1, afterrightip);

            break;
        }
        case EXPRESSION_TERNARY:
        {
            const ApeTernaryExpr_t* ternary = &expr->ternary;

            ok = ape_compiler_compileexpression(comp, ternary->test);
            if(!ok)
            {
                goto error;
            }

            ApeInt_t elsejumpip = ape_compiler_emit(comp, OPCODE_JUMP_IF_FALSE, 1, (uint64_t[]){ (uint64_t)0xbeef });

            ok = ape_compiler_compileexpression(comp, ternary->iftrue);
            if(!ok)
            {
                goto error;
            }

            ApeInt_t endjumpip = ape_compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)0xbeef });

            ApeInt_t elseip = ape_compiler_getip(comp);
            ape_compiler_moduint16operand(comp, elsejumpip + 1, elseip);

            ok = ape_compiler_compileexpression(comp, ternary->iffalse);
            if(!ok)
            {
                goto error;
            }

            ApeInt_t endip = ape_compiler_getip(comp);
            ape_compiler_moduint16operand(comp, endjumpip + 1, endip);

            break;
        }
        default:
        {
            APE_ASSERT(false);
            break;
        }
    }
    res = true;
    goto end;
error:
    res = false;
end:
    array_pop(comp->srcpositionsstack, NULL);
    ape_ast_destroy_expr(exproptimised);
    return res;
}

static bool ape_compiler_compilecodeblock(ApeCompiler_t* comp, const ApeCodeblock_t* block)
{
    ApeSize_t i;
    ApeSymbolTable_t* symbol_table = ape_compiler_getsymboltable(comp);
    if(!symbol_table)
    {
        return false;
    }

    bool ok = symbol_table_push_block_scope(symbol_table);
    if(!ok)
    {
        return false;
    }

    if(ptrarray_count(block->statements) == 0)
    {
        ApeInt_t ip = ape_compiler_emit(comp, OPCODE_NULL, 0, NULL);
        if(ip < 0)
        {
            return false;
        }
        ip = ape_compiler_emit(comp, OPCODE_POP, 0, NULL);
        if(ip < 0)
        {
            return false;
        }
    }

    for(i = 0; i < ptrarray_count(block->statements); i++)
    {
        const ApeStatement_t* stmt = (ApeStatement_t*)ptrarray_get(block->statements, i);
        bool ok = ape_compiler_compilestatement(comp, stmt);
        if(!ok)
        {
            return false;
        }
    }
    symbol_table_pop_block_scope(symbol_table);
    return true;
}

static ApeInt_t ape_compiler_addconstant(ApeCompiler_t* comp, ApeObject_t obj)
{
    bool ok = array_add(comp->constants, &obj);
    if(!ok)
    {
        return -1;
    }
    ApeInt_t pos = array_count(comp->constants) - 1;
    return pos;
}

static void ape_compiler_moduint16operand(ApeCompiler_t* comp, ApeInt_t ip, uint64_t operand)
{
    ApeArray_t* bytecode = ape_compiler_getbytecode(comp);
    if((ip + 1) >= (ApeInt_t)array_count(bytecode))
    {
        APE_ASSERT(false);
        return;
    }
    ApeUShort_t hi = (ApeUShort_t)(operand >> 8);
    array_set(bytecode, ip, &hi);
    ApeUShort_t lo = (ApeUShort_t)(operand);
    array_set(bytecode, ip + 1, &lo);
}

static bool ape_compiler_lastopcodeis(ApeCompiler_t* comp, ApeOpByte_t op)
{
    ApeOpByte_t lastopcode = ape_compiler_getlastopcode(comp);
    return lastopcode == op;
}

static bool ape_compiler_readsym(ApeCompiler_t* comp, const ApeSymbol_t* symbol)
{
    ApeInt_t ip = -1;
    if(symbol->type == SYMBOL_MODULE_GLOBAL)
    {
        ip = ape_compiler_emit(comp, OPCODE_GET_MODULE_GLOBAL, 1, (uint64_t[]){ (uint64_t)(symbol->index) });
    }
    else if(symbol->type == SYMBOL_CONTEXT_GLOBAL)
    {
        ip = ape_compiler_emit(comp, OPCODE_GET_CONTEXT_GLOBAL, 1, (uint64_t[]){ (uint64_t)(symbol->index) });
    }
    else if(symbol->type == SYMBOL_LOCAL)
    {
        ip = ape_compiler_emit(comp, OPCODE_GET_LOCAL, 1, (uint64_t[]){ (uint64_t)(symbol->index) });
    }
    else if(symbol->type == SYMBOL_FREE)
    {
        ip = ape_compiler_emit(comp, OPCODE_GET_FREE, 1, (uint64_t[]){ (uint64_t)(symbol->index) });
    }
    else if(symbol->type == SYMBOL_FUNCTION)
    {
        ip = ape_compiler_emit(comp, OPCODE_CURRENT_FUNCTION, 0, NULL);
    }
    else if(symbol->type == SYMBOL_THIS)
    {
        ip = ape_compiler_emit(comp, OPCODE_GET_THIS, 0, NULL);
    }
    return ip >= 0;
}

static bool ape_compiler_writesym(ApeCompiler_t* comp, const ApeSymbol_t* symbol, bool define)
{
    ApeInt_t ip = -1;
    if(symbol->type == SYMBOL_MODULE_GLOBAL)
    {
        if(define)
        {
            ip = ape_compiler_emit(comp, OPCODE_DEFINE_MODULE_GLOBAL, 1, (uint64_t[]){ (uint64_t)(symbol->index) });
        }
        else
        {
            ip = ape_compiler_emit(comp, OPCODE_SET_MODULE_GLOBAL, 1, (uint64_t[]){ (uint64_t)(symbol->index) });
        }
    }
    else if(symbol->type == SYMBOL_LOCAL)
    {
        if(define)
        {
            ip = ape_compiler_emit(comp, OPCODE_DEFINE_LOCAL, 1, (uint64_t[]){ (uint64_t)(symbol->index) });
        }
        else
        {
            ip = ape_compiler_emit(comp, OPCODE_SET_LOCAL, 1, (uint64_t[]){ (uint64_t)(symbol->index) });
        }
    }
    else if(symbol->type == SYMBOL_FREE)
    {
        ip = ape_compiler_emit(comp, OPCODE_SET_FREE, 1, (uint64_t[]){ (uint64_t)(symbol->index) });
    }
    return ip >= 0;
}

static bool ape_compiler_pushbreakip(ApeCompiler_t* comp, ApeInt_t ip)
{
    ApeCompilationScope_t* compscope = ape_compiler_getcompscope(comp);
    return array_push(compscope->breakipstack, &ip);
}

static void ape_compiler_popbreakip(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* compscope = ape_compiler_getcompscope(comp);
    if(array_count(compscope->breakipstack) == 0)
    {
        APE_ASSERT(false);
        return;
    }
    array_pop(compscope->breakipstack, NULL);
}

static ApeInt_t ape_compiler_getbreakip(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* compscope = ape_compiler_getcompscope(comp);
    if(array_count(compscope->breakipstack) == 0)
    {
        return -1;
    }
    ApeInt_t* res = (ApeInt_t*)array_top(compscope->breakipstack);
    return *res;
}

static bool ape_compiler_pushcontip(ApeCompiler_t* comp, ApeInt_t ip)
{
    ApeCompilationScope_t* compscope = ape_compiler_getcompscope(comp);
    return array_push(compscope->continueipstack, &ip);
}

static void ape_compiler_popcontip(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* compscope = ape_compiler_getcompscope(comp);
    if(array_count(compscope->continueipstack) == 0)
    {
        APE_ASSERT(false);
        return;
    }
    array_pop(compscope->continueipstack, NULL);
}

static ApeInt_t ape_compiler_getcontip(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* compscope = ape_compiler_getcompscope(comp);
    if(array_count(compscope->continueipstack) == 0)
    {
        APE_ASSERT(false);
        return -1;
    }
    ApeInt_t* res = (ApeInt_t*)array_top(compscope->continueipstack);
    return *res;
}

static ApeInt_t ape_compiler_getip(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* compilation_scope = ape_compiler_getcompscope(comp);
    return array_count(compilation_scope->bytecode);
}

static ApeArray_t * ape_compiler_getsrcpositions(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* compilation_scope = ape_compiler_getcompscope(comp);
    return compilation_scope->srcpositions;
}

static ApeArray_t * ape_compiler_getbytecode(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* compilation_scope = ape_compiler_getcompscope(comp);
    return compilation_scope->bytecode;
}

static ApeFileScope_t* ape_compiler_makefilescope(ApeCompiler_t* comp, ApeCompiledFile_t* file)
{
    ApeFileScope_t* filescope = (ApeFileScope_t*)allocator_malloc(comp->alloc, sizeof(ApeFileScope_t));
    if(!filescope)
    {
        return NULL;
    }
    memset(filescope, 0, sizeof(ApeFileScope_t));
    filescope->alloc = comp->alloc;
    filescope->parser = ape_ast_make_parser(comp->context, comp->config, comp->errors);
    if(!filescope->parser)
    {
        goto err;
    }
    filescope->symbol_table = NULL;
    filescope->file = file;
    filescope->loadedmodulenames = ptrarray_make(comp->context);
    if(!filescope->loadedmodulenames)
    {
        goto err;
    }
    return filescope;
err:
    ape_compiler_destroyfilescope(filescope);
    return NULL;
}

static void ape_compiler_destroyfilescope(ApeFileScope_t* scope)
{
    ApeSize_t i;
    for(i = 0; i < ptrarray_count(scope->loadedmodulenames); i++)
    {
        void* name = ptrarray_get(scope->loadedmodulenames, i);
        allocator_free(scope->alloc, name);
    }
    ptrarray_destroy(scope->loadedmodulenames);
    ape_parser_destroy(scope->parser);
    allocator_free(scope->alloc, scope);
}

static bool ape_compiler_pushfilescope(ApeCompiler_t* comp, const char* filepath)
{
    bool ok;
    ApeBlockScope_t* prevsttopscope;
    ApeSymbolTable_t* prevst;
    ApeFileScope_t* filescope;
    prevst = NULL;
    if(ptrarray_count(comp->filescopes) > 0)
    {
        prevst = ape_compiler_getsymboltable(comp);
    }
    ApeCompiledFile_t* file;
    file = compiled_file_make(comp->context, filepath);
    if(!file)
    {
        return false;
    }

    ok = ptrarray_add(comp->files, file);
    if(!ok)
    {
        compiled_file_destroy(file);
        return false;
    }

    filescope = ape_compiler_makefilescope(comp, file);
    if(!filescope)
    {
        return false;
    }

    ok = ptrarray_push(comp->filescopes, filescope);
    if(!ok)
    {
        ape_compiler_destroyfilescope(filescope);
        return false;
    }

    ApeInt_t globaloffset = 0;
    if(prevst)
    {
        prevsttopscope = symbol_table_get_block_scope(prevst);
        globaloffset = prevsttopscope->offset + prevsttopscope->numdefinitions;
    }
    ok = ape_compiler_pushsymtable(comp, globaloffset);
    if(!ok)
    {
        ptrarray_pop(comp->filescopes);
        ape_compiler_destroyfilescope(filescope);
        return false;
    }
    return true;
}

static void ape_compiler_popfilescope(ApeCompiler_t* comp)
{
    ApeSymbolTable_t* currentst;
    ApeBlockScope_t* currentsttopscope;
    ApeSymbolTable_t* poppedst;
    ApeBlockScope_t* poppedsttopscope;
    ApeFileScope_t* scope;
    ApeInt_t poppednumdefs;
    poppedst = ape_compiler_getsymboltable(comp);
    poppedsttopscope = symbol_table_get_block_scope(poppedst);
    poppednumdefs = poppedsttopscope->numdefinitions;
    while(ape_compiler_getsymboltable(comp))
    {
        ape_compiler_popsymtable(comp);
    }
    scope = (ApeFileScope_t*)ptrarray_top(comp->filescopes);
    if(!scope)
    {
        APE_ASSERT(false);
        return;
    }
    ape_compiler_destroyfilescope(scope);
    ptrarray_pop(comp->filescopes);
    if(ptrarray_count(comp->filescopes) > 0)
    {
        currentst = ape_compiler_getsymboltable(comp);
        currentsttopscope = symbol_table_get_block_scope(currentst);
        currentsttopscope->numdefinitions += poppednumdefs;
    }
}

static void ape_compiler_setcompscope(ApeCompiler_t* comp, ApeCompilationScope_t* scope)
{
    comp->compilation_scope = scope;
}
