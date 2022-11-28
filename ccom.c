
#if defined(__cplusplus)
    #include <vector>
#endif
#include "ape.h"

typedef struct ApeTempU64Array_t ApeTempU64Array_t;

struct ApeTempU64Array_t
{
    uint64_t data[10];
};

#define NARGS_SEQ(_1,_2,_3,_4,_5,_6,_7,_8,_9,N,...) N
#define NARGS(...) NARGS_SEQ(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1)

#if defined(__cplusplus)
    #define make_u64_array(...) (std::vector<uint64_t>{__VA_ARGS__}).data()
#else
    #define make_u64_array(...) ((uint64_t[]){__VA_ARGS__ })
#endif

static const ApePosition_t gccprivsrcposinvalid = { NULL, -1, -1 };


static const ApeSymbol_t *ape_compiler_definesym(ApeCompiler_t *comp, ApePosition_t pos, const char *name, bool assignable, bool canshadow);
static void ape_compiler_setsymtable(ApeCompiler_t *comp, ApeSymTable_t *table);
static ApeInt_t ape_compiler_emit(ApeCompiler_t *comp, ApeOpByte_t op, ApeSize_t operandscount, uint64_t *operands);
static ApeCompScope_t *ape_compiler_getcompscope(ApeCompiler_t *comp);
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
static ApeValArray_t *ape_compiler_getsrcpositions(ApeCompiler_t *comp);
static ApeValArray_t *ape_compiler_getbytecode(ApeCompiler_t *comp);
static ApeFileScope_t *ape_compiler_makefilescope(ApeCompiler_t *comp, ApeCompiledFile_t *file);
static void ape_compiler_destroyfilescope(ApeFileScope_t *scope);
static bool ape_compiler_pushfilescope(ApeCompiler_t *comp, const char *filepath);
static void ape_compiler_popfilescope(ApeCompiler_t *comp);
static void ape_compiler_setcompscope(ApeCompiler_t *comp, ApeCompScope_t *scope);


static const ApeSymbol_t* ape_compiler_definesym(ApeCompiler_t* comp, ApePosition_t pos, const char* name, bool assignable, bool canshadow)
{
    ApeSymTable_t* symbol_table;
    const ApeSymbol_t* currentsymbol;
    const ApeSymbol_t* symbol;
    symbol_table = ape_compiler_getsymboltable(comp);
    if(!canshadow && !ape_symtable_istopglobalscope(symbol_table))
    {
        currentsymbol = ape_symtable_resolve(symbol_table, name);
        if(currentsymbol)
        {
            ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, pos, "symbol '%s' is already defined", name);
            return NULL;
        }
    }
    symbol = ape_symtable_define(symbol_table, name, assignable);
    if(!symbol)
    {
        ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, pos, "cannot define symbol '%s'", name);
        return NULL;
    }
    return symbol;
}


ApeCompiler_t* ape_compiler_make(ApeContext_t* ctx, const ApeConfig_t* config, ApeGCMemory_t* mem, ApeErrorList_t* errors, ApePtrArray_t * files, ApeGlobalStore_t* globalstore)
{
    bool ok;
    ApeCompiler_t* comp;
    comp = (ApeCompiler_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeCompiler_t));
    if(!comp)
    {
        return NULL;
    }
    comp->context = ctx;
    ok = ape_compiler_init(comp, ctx, config, mem, errors, files, globalstore);
    if(!ok)
    {
        ape_allocator_free(&ctx->alloc, comp);
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
    ape_allocator_free(alloc, comp);
}

ApeCompResult_t* ape_compiler_compilesource(ApeCompiler_t* comp, const char* code)
{
    bool ok;
    ApeCompiler_t compshallowcopy;
    ApeCompScope_t* compilation_scope;
    ApeCompResult_t* res;
    compilation_scope = ape_compiler_getcompscope(comp);
    APE_ASSERT(ape_valarray_count(comp->srcpositionsstack) == 0);
    APE_ASSERT(ape_valarray_count(compilation_scope->bytecode) == 0);
    APE_ASSERT(ape_valarray_count(compilation_scope->breakipstack) == 0);
    APE_ASSERT(ape_valarray_count(compilation_scope->continueipstack) == 0);
    ape_valarray_clear(comp->srcpositionsstack);
    ape_valarray_clear(compilation_scope->bytecode);
    ape_valarray_clear(compilation_scope->srcpositions);
    ape_valarray_clear(compilation_scope->breakipstack);
    ape_valarray_clear(compilation_scope->continueipstack);
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
    res = ape_compscope_orphanresult(compilation_scope);
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

ApeCompResult_t* ape_compiler_compilefile(ApeCompiler_t* comp, const char* path)
{
    bool ok;
    char* code;
    ApeCompiledFile_t* file;
    ApeCompResult_t* res;
    ApeFileScope_t* filescope;
    ApeCompiledFile_t* prevfile;
    code = NULL;
    file = NULL;
    res = NULL;
    if(!comp->config->fileio.read_file.read_file)
    {// todo: read code function
        ape_errorlist_add(comp->errors, APE_ERROR_COMPILATION, gccprivsrcposinvalid, "file read function not configured");
        goto err;
    }
    code = comp->config->fileio.read_file.read_file(comp->config->fileio.read_file.context, path);
    if(!code)
    {
        ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, gccprivsrcposinvalid, "reading file '%s' failed", path);
        goto err;
    }
    file = ape_make_compiledfile(comp->context, path);
    if(!file)
    {
        goto err;
    }
    ok = ape_ptrarray_add(comp->files, file);
    if(!ok)
    {
        compiled_file_destroy(file);
        goto err;
    }
    APE_ASSERT(ape_ptrarray_count(comp->filescopes) == 1);
    filescope = (ApeFileScope_t*)ape_ptrarray_top(comp->filescopes);
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
    ape_allocator_free(comp->alloc, code);
    return res;
err:
    ape_allocator_free(comp->alloc, code);
    return NULL;
}

ApeSymTable_t* ape_compiler_getsymboltable(ApeCompiler_t* comp)
{
    ApeFileScope_t* filescope;
    filescope = (ApeFileScope_t*)ape_ptrarray_top(comp->filescopes);
    if(!filescope)
    {
        APE_ASSERT(false);
        return NULL;
    }
    return filescope->symbol_table;
}

static void ape_compiler_setsymtable(ApeCompiler_t* comp, ApeSymTable_t* table)
{
    ApeFileScope_t* filescope;
    filescope = (ApeFileScope_t*)ape_ptrarray_top(comp->filescopes);
    if(!filescope)
    {
        APE_ASSERT(false);
        return;
    }
    filescope->symbol_table = table;
}

ApeValArray_t* ape_compiler_getconstants(const ApeCompiler_t* comp)
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
    comp->filescopes = ape_make_ptrarray(ctx);
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

void ape_compiler_deinit(ApeCompiler_t* comp)
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
        ape_allocator_free(comp->alloc, val);
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
    ape_strdict_destroywithitems(comp->modules);
    ape_valarray_destroy(comp->srcpositionsstack);
    ape_valarray_destroy(comp->constants);
    ape_ptrarray_destroy(comp->filescopes);
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
    ApeSymTable_t* srcst;
    ApeSymTable_t* srcstcopy;
    ApeSymTable_t* copyst;
    ApeStrDict_t* modulescopy;
    ApeValArray_t* constantscopy;
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
    APE_ASSERT(ape_ptrarray_count(src->filescopes) == 1);
    APE_ASSERT(srcst->outer == NULL);
    srcstcopy = ape_symtable_copy(srcst);
    if(!srcstcopy)
    {
        goto err;
    }
    copyst = ape_compiler_getsymboltable(copy);
    ape_symtable_destroy(copyst);
    copyst = NULL;
    ape_compiler_setsymtable(copy, srcstcopy);
    modulescopy = ape_strdict_copywithitems(src->modules);
    if(!modulescopy)
    {
        goto err;
    }
    ape_strdict_destroywithitems(copy->modules);
    copy->modules = modulescopy;
    constantscopy = ape_valarray_copy(src->constants);
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
        valcopy = (ApeInt_t*)ape_allocator_alloc(src->alloc, sizeof(ApeInt_t));
        if(!valcopy)
        {
            goto err;
        }
        *valcopy = *val;
        ok = ape_strdict_set(copy->stringconstantspositions, key, valcopy);
        if(!ok)
        {
            ape_allocator_free(src->alloc, valcopy);
            goto err;
        }
    }
    srcfilescope = (ApeFileScope_t*)ape_ptrarray_top(src->filescopes);
    copyfilescope = (ApeFileScope_t*)ape_ptrarray_top(copy->filescopes);
    srcloadedmodulenames = srcfilescope->loadedmodulenames;
    copyloadedmodulenames = copyfilescope->loadedmodulenames;
    for(i = 0; i < ape_ptrarray_count(srcloadedmodulenames); i++)
    {

        loadedname = (const char*)ape_ptrarray_get(srcloadedmodulenames, i);
        loadednamecopy = ape_util_strdup(copy->context, loadedname);
        if(!loadednamecopy)
        {
            goto err;
        }
        ok = ape_ptrarray_add(copyloadedmodulenames, loadednamecopy);
        if(!ok)
        {
            ape_allocator_free(copy->alloc, loadednamecopy);
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
    ApeCompScope_t* compilation_scope;
    ip = ape_compiler_getip(comp);
    len = ape_make_code(op, operandscount, operands, ape_compiler_getbytecode(comp));
    if(len == 0)
    {
        return -1;
    }
    for(i = 0; i < len; i++)
    {
        srcpos = (ApePosition_t*)ape_valarray_top(comp->srcpositionsstack);
        APE_ASSERT(srcpos->line >= 0);
        APE_ASSERT(srcpos->column >= 0);
        ok = ape_valarray_add(ape_compiler_getsrcpositions(comp), srcpos);
        if(!ok)
        {
            return -1;
        }
    }
    compilation_scope = ape_compiler_getcompscope(comp);
    compilation_scope->lastopcode = op;
    return ip;
}

static ApeCompScope_t* ape_compiler_getcompscope(ApeCompiler_t* comp)
{
    return comp->compilation_scope;
}

static bool ape_compiler_pushcompscope(ApeCompiler_t* comp)
{
    ApeCompScope_t* currentscope;
    ApeCompScope_t* newscope;
    currentscope = ape_compiler_getcompscope(comp);
    newscope = ape_make_compscope(comp->context, currentscope);
    if(!newscope)
    {
        return false;
    }
    ape_compiler_setcompscope(comp, newscope);
    return true;
}

static void ape_compiler_popcompscope(ApeCompiler_t* comp)
{
    ApeCompScope_t* currentscope;
    currentscope = ape_compiler_getcompscope(comp);
    APE_ASSERT(currentscope);
    ape_compiler_setcompscope(comp, currentscope->outer);
    ape_compscope_destroy(currentscope);
}

static bool ape_compiler_pushsymtable(ApeCompiler_t* comp, ApeInt_t globaloffset)
{
    ApeFileScope_t* filescope;
    filescope = (ApeFileScope_t*)ape_ptrarray_top(comp->filescopes);
    if(!filescope)
    {
        APE_ASSERT(false);
        return false;
    }
    ApeSymTable_t* currenttable = filescope->symbol_table;
    filescope->symbol_table = ape_make_symtable(comp->context, currenttable, comp->globalstore, globaloffset);
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
    ApeSymTable_t* currenttable;
    filescope = (ApeFileScope_t*)ape_ptrarray_top(comp->filescopes);
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
    ape_symtable_destroy(currenttable);
}

static ApeOpByte_t ape_compiler_getlastopcode(ApeCompiler_t* comp)
{
    ApeCompScope_t* currentscope;
    currentscope = ape_compiler_getcompscope(comp);
    return currentscope->lastopcode;
}

static bool ape_compiler_compilecode(ApeCompiler_t* comp, const char* code)
{
    bool ok;
    ApeFileScope_t* filescope;
    ApePtrArray_t* statements;
    filescope = (ApeFileScope_t*)ape_ptrarray_top(comp->filescopes);
    APE_ASSERT(filescope);
    statements = ape_parser_parseall(filescope->parser, code, filescope->file);
    if(!statements)
    {
        // errors are added by parser
        return false;
    }
    if(comp->context->config.dumpast)
    {
        ape_context_dumpast(comp->context, statements);
    }
    ok = ape_compiler_compilestmtlist(comp, statements);
    ape_ptrarray_destroywithitems(statements, (ApeDataCallback_t)ape_ast_destroy_stmt);

    // Left for debugging purposes
    //    if (ok) {
    //        ApeWriter_t *buf = ape_make_writer(NULL);
    //        ape_tostring_code(ape_valarray_data(comp->compilation_scope->bytecode),
    //                       ape_valarray_data(comp->compilation_scope->srcpositions),
    //                       ape_valarray_count(comp->compilation_scope->bytecode), buf);
    //        puts(ape_writer_getdata(buf));
    //        ape_writer_destroy(buf);
    //    }

    return ok;
}

static bool ape_compiler_compilestmtlist(ApeCompiler_t* comp, ApePtrArray_t * statements)
{
    ApeSize_t i;
    bool ok;
    const ApeStatement_t* stmt;
    ok = true;
    for(i = 0; i < ape_ptrarray_count(statements); i++)
    {
        stmt = (const ApeStatement_t*)ape_ptrarray_get(statements, i);
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
    ApeSymTable_t* symbol_table;
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
    symbol_table = ape_compiler_getsymboltable(comp);
    if(symbol_table->outer != NULL || ape_ptrarray_count(symbol_table->blockscopes) > 1)
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
        if(!comp->config->fileio.read_file.read_file)
        {
            ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, importstmt->pos,
                              "cannot import module '%s', file read function not configured", filepath);
            result = false;
            goto end;
        }
        code = comp->config->fileio.read_file.read_file(comp->config->fileio.read_file.context, filepath);
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
        ok = ape_symtable_addmodulesymbol(symbol_table, symbol);
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
    ApeCompScope_t* compilation_scope;
    ApeSymTable_t* symbol_table;
    const ApeSymbol_t* symbol;
    const ApeIfStmt_t* ifstmt;
    ApeValArray_t* jumptoendips;
    ApeIfCase_t* ifcase;

    breakip = 0;
    ok = false;
    ip = -1;
    ok = ape_valarray_push(comp->srcpositionsstack, &stmt->pos);
    if(!ok)
    {
        return false;
    }
    compilation_scope = ape_compiler_getcompscope(comp);
    symbol_table = ape_compiler_getsymboltable(comp);
    switch(stmt->type)
    {
        case APE_STMT_EXPRESSION:
            {
                ok = ape_compiler_compileexpression(comp, stmt->expression);
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

        case APE_STMT_DEFINE:
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

        case APE_STMT_IF:
            {
                ifstmt = &stmt->ifstatement;
                jumptoendips = array_make(comp->context, ApeInt_t);
                if(!jumptoendips)
                {
                    goto statementiferror;
                }
                for(i = 0; i < ape_ptrarray_count(ifstmt->cases); i++)
                {
                    ifcase = (ApeIfCase_t*)ape_ptrarray_get(ifstmt->cases, i);
                    ok = ape_compiler_compileexpression(comp, ifcase->test);
                    if(!ok)
                    {
                        goto statementiferror;
                    }
                    nextcasejumpip = ape_compiler_emit(comp, APE_OPCODE_JUMPIFFALSE, 1, make_u64_array((uint64_t)(0xbeef)));
                    ok = ape_compiler_compilecodeblock(comp, ifcase->consequence);
                    if(!ok)
                    {
                        goto statementiferror;
                    }
                    // don't ape_compiler_emit jump for the last statement
                    if(i < (ape_ptrarray_count(ifstmt->cases) - 1) || ifstmt->alternative)
                    {
                        jumptoendip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((uint64_t)(0xbeef)));
                        ok = ape_valarray_add(jumptoendips, &jumptoendip);
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
        case APE_STMT_RETURNVALUE:
            {
                if(compilation_scope->outer == NULL)
                {
                    ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "nothing to return from");
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
        case APE_STMT_WHILELOOP:
            {
                loop = &stmt->whileloop;
                beforetestip = ape_compiler_getip(comp);
                ok = ape_compiler_compileexpression(comp, loop->test);
                if(!ok)
                {
                    return false;
                }
                aftertestip = ape_compiler_getip(comp);
                ip = ape_compiler_emit(comp, APE_OPCODE_JUMPIFTRUE, 1, make_u64_array((uint64_t)(aftertestip + 6)));
                if(ip < 0)
                {
                    return false;
                }
                jumptoafterbodyip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((uint64_t)0xdead));
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
                ip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((uint64_t)beforetestip));
                if(ip < 0)
                {
                    return false;
                }
                afterbodyip = ape_compiler_getip(comp);
                ape_compiler_moduint16operand(comp, jumptoafterbodyip + 1, afterbodyip);
            }
            break;
        case APE_STMT_BREAK:
            {
                breakip = ape_compiler_getbreakip(comp);
                if(breakip < 0)
                {
                    ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "nothing to break from");
                    return false;
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((uint64_t)breakip));
                if(ip < 0)
                {
                    return false;
                }
            }
            break;
        case APE_STMT_CONTINUE:
            {
                ApeInt_t continueip = ape_compiler_getcontip(comp);
                if(continueip < 0)
                {
                    ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "nothing to continue from");
                    return false;
                }
                ip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((uint64_t)continueip));
                if(ip < 0)
                {
                    return false;
                }
            }
            break;
        case APE_STMT_FOREACH:
            {
                const ApeForeachStmt_t* foreach = &stmt->foreach;
                ok = ape_symtable_pushblockscope(symbol_table);
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
                ip = ape_compiler_emit(comp, APE_OPCODE_MKNUMBER, 1, make_u64_array((uint64_t)0));
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
                if(foreach->source->type == APE_EXPR_IDENT)
                {
                    sourcesymbol = ape_symtable_resolve(symbol_table, foreach->source->ident->value);
                    if(!sourcesymbol)
                    {
                        ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, foreach->source->pos,
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
                ApeInt_t jumptoafterupdateip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((uint64_t)0xbeef));
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
                ip = ape_compiler_emit(comp, APE_OPCODE_MKNUMBER, 1, make_u64_array((uint64_t)ape_util_floattouint(1)));
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
                ApeInt_t afterupdateip = ape_compiler_getip(comp);
                ape_compiler_moduint16operand(comp, jumptoafterupdateip + 1, afterupdateip);
                // Test
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
                ape_valarray_pop(comp->srcpositionsstack, NULL);
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
                ApeInt_t aftertestip = ape_compiler_getip(comp);
                ip = ape_compiler_emit(comp, APE_OPCODE_JUMPIFFALSE, 1, make_u64_array((uint64_t)(aftertestip + 6)));
                if(ip < 0)
                {
                    return false;
                }
                ApeInt_t jumptoafterbodyip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((uint64_t)0xdead));
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
                ip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((uint64_t)updateip));
                if(ip < 0)
                {
                    return false;
                }
                ApeInt_t afterbodyip = ape_compiler_getip(comp);
                ape_compiler_moduint16operand(comp, jumptoafterbodyip + 1, afterbodyip);
                ape_symtable_popblockscope(symbol_table);
            }
            break;

        case APE_STMT_FORLOOP:
            {
                const ApeForLoopStmt_t* loop = &stmt->forloop;
                ok = ape_symtable_pushblockscope(symbol_table);
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
                    jumptoafterupdateip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((uint64_t)0xbeef));
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
                    ip = ape_compiler_emit(comp, APE_OPCODE_POP, 0, NULL);
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
                    ip = ape_compiler_emit(comp, APE_OPCODE_TRUE, 0, NULL);
                    if(ip < 0)
                    {
                        return false;
                    }
                }
                ApeInt_t aftertestip = ape_compiler_getip(comp);
                ip = ape_compiler_emit(comp, APE_OPCODE_JUMPIFTRUE, 1, make_u64_array((uint64_t)(aftertestip + 6)));
                if(ip < 0)
                {
                    return false;
                }
                ApeInt_t jmptoafterbodyip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((uint64_t)0xdead));
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

                ip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((uint64_t)updateip));
                if(ip < 0)
                {
                    return false;
                }
                ApeInt_t afterbodyip = ape_compiler_getip(comp);
                ape_compiler_moduint16operand(comp, jmptoafterbodyip + 1, afterbodyip);
                ape_symtable_popblockscope(symbol_table);
            }
            break;

        case APE_STMT_BLOCK:
        {
            ok = ape_compiler_compilecodeblock(comp, stmt->block);
            if(!ok)
            {
                return false;
            }
            break;
        }
        case APE_STMT_IMPORT:
        {
            ok = ape_compiler_importmodule(comp, stmt);
            if(!ok)
            {
                return false;
            }
            break;
        }
        case APE_STMT_RECOVER:
        {
            const ApeRecoverStmt_t* recover = &stmt->recover;

            if(ape_symtable_ismoduleglobalscope(symbol_table))
            {
                ape_errorlist_add(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "recover statement cannot be defined in global scope");
                return false;
            }

            if(!ape_symtable_istopblockscope(symbol_table))
            {
                ape_errorlist_add(comp->errors, APE_ERROR_COMPILATION, stmt->pos,
                                 "recover statement cannot be defined within other statements");
                return false;
            }

            ApeInt_t recoverip = ape_compiler_emit(comp, APE_OPCODE_SETRECOVER, 1, make_u64_array((uint64_t)0xbeef));
            if(recoverip < 0)
            {
                return false;
            }

            ApeInt_t jumptoafterrecoverip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((uint64_t)0xbeef));
            if(jumptoafterrecoverip < 0)
            {
                return false;
            }

            ApeInt_t afterjumptorecoverip = ape_compiler_getip(comp);
            ape_compiler_moduint16operand(comp, recoverip + 1, afterjumptorecoverip);

            ok = ape_symtable_pushblockscope(symbol_table);
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

            if(!ape_compiler_lastopcodeis(comp, APE_OPCODE_RETURNNOTHING) && !ape_compiler_lastopcodeis(comp, APE_OPCODE_RETURNVALUE))
            {
                ape_errorlist_add(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "recover body must end with a return statement");
                return false;
            }

            ape_symtable_popblockscope(symbol_table);

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
    ape_valarray_pop(comp->srcpositionsstack, NULL);
    return true;
}

static bool ape_compiler_compileexpression(ApeCompiler_t* comp, ApeExpression_t* expr)
{
    ApeSize_t i;
    bool ok = false;
    ApeInt_t ip = -1;

    ApeExpression_t* exproptimised = ape_optimizer_optexpr(expr);
    if(exproptimised)
    {
        expr = exproptimised;
    }

    ok = ape_valarray_push(comp->srcpositionsstack, &expr->pos);
    if(!ok)
    {
        return false;
    }

    ApeCompScope_t* compilation_scope = ape_compiler_getcompscope(comp);
    ApeSymTable_t* symbol_table = ape_compiler_getsymboltable(comp);

    bool res = false;

    switch(expr->type)
    {
        case APE_EXPR_INFIX:
        {
            bool rearrange = false;

            ApeOpByte_t op = APE_OPCODE_NONE;
            switch(expr->infix.op)
            {
                case APE_OPERATOR_PLUS:
                    op = APE_OPCODE_ADD;
                    break;
                case APE_OPERATOR_MINUS:
                    op = APE_OPCODE_SUB;
                    break;
                case APE_OPERATOR_STAR:
                    op = APE_OPCODE_MUL;
                    break;
                case APE_OPERATOR_SLASH:
                    op = APE_OPCODE_DIV;
                    break;
                case APE_OPERATOR_MODULUS:
                    op = APE_OPCODE_MOD;
                    break;
                case APE_OPERATOR_EQUAL:
                    op = APE_OPCODE_ISEQUAL;
                    break;
                case APE_OPERATOR_NOTEQUAL:
                    op = APE_OPCODE_NOTEQUAL;
                    break;
                case APE_OPERATOR_GREATERTHAN:
                    op = APE_OPCODE_GREATERTHAN;
                    break;
                case APE_OPERATOR_GREATEREQUAL:
                    op = APE_OPCODE_GREATEREQUAL;
                    break;
                case APE_OPERATOR_LESSTHAN:
                    op = APE_OPCODE_GREATERTHAN;
                    rearrange = true;
                    break;
                case APE_OPERATOR_LESSEQUAL:
                    op = APE_OPCODE_GREATEREQUAL;
                    rearrange = true;
                    break;
                case APE_OPERATOR_BITOR:
                    op = APE_OPCODE_BITOR;
                    break;
                case APE_OPERATOR_BITXOR:
                    op = APE_OPCODE_BITXOR;
                    break;
                case APE_OPERATOR_BITAND:
                    op = APE_OPCODE_BITAND;
                    break;
                case APE_OPERATOR_LEFTSHIFT:
                    op = APE_OPCODE_LEFTSHIFT;
                    break;
                case APE_OPERATOR_RIGHTSHIFT:
                    op = APE_OPCODE_RIGHTSHIFT;
                    break;
                default:
                {
                    ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, expr->pos, "unknown infix operator");
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
                case APE_OPERATOR_EQUAL:
                case APE_OPERATOR_NOTEQUAL:
                {
                    ip = ape_compiler_emit(comp, APE_OPCODE_COMPAREEQUAL, 0, NULL);
                    if(ip < 0)
                    {
                        goto error;
                    }
                    break;
                }
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
        case APE_EXPR_LITERALNUMBER:
        {
            ApeFloat_t number = expr->numberliteral;
            ip = ape_compiler_emit(comp, APE_OPCODE_MKNUMBER, 1, make_u64_array((uint64_t)ape_util_floattouint(number)));
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case APE_EXPR_LITERALSTRING:
        {
            ApeInt_t pos = 0;
            ApeInt_t* currentpos = (ApeInt_t*)ape_strdict_get(comp->stringconstantspositions, expr->stringliteral);
            if(currentpos)
            {
                pos = *currentpos;
            }
            else
            {
                ApeObject_t obj = ape_object_make_string(comp->context, expr->stringliteral);
                if(ape_object_value_isnull(obj))
                {
                    goto error;
                }

                pos = ape_compiler_addconstant(comp, obj);
                if(pos < 0)
                {
                    goto error;
                }

                ApeInt_t* posval = (ApeInt_t*)ape_allocator_alloc(comp->alloc, sizeof(ApeInt_t));
                if(!posval)
                {
                    goto error;
                }

                *posval = pos;
                ok = ape_strdict_set(comp->stringconstantspositions, expr->stringliteral, posval);
                if(!ok)
                {
                    ape_allocator_free(comp->alloc, posval);
                    goto error;
                }
            }

            ip = ape_compiler_emit(comp, APE_OPCODE_CONSTANT, 1, make_u64_array((uint64_t)pos));
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case APE_EXPR_LITERALNULL:
        {
            ip = ape_compiler_emit(comp, APE_OPCODE_NULL, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }
            break;
        }
        case APE_EXPR_LITERALBOOL:
        {
            ip = ape_compiler_emit(comp, expr->boolliteral ? APE_OPCODE_TRUE : APE_OPCODE_FALSE, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }
            break;
        }
        case APE_EXPR_LITERALARRAY:
        {
            for(i = 0; i < ape_ptrarray_count(expr->array); i++)
            {
                ok = ape_compiler_compileexpression(comp, (ApeExpression_t*)ape_ptrarray_get(expr->array, i));
                if(!ok)
                {
                    goto error;
                }
            }
            ip = ape_compiler_emit(comp, APE_OPCODE_MKARRAY, 1, make_u64_array((uint64_t)ape_ptrarray_count(expr->array)));
            if(ip < 0)
            {
                goto error;
            }
            break;
        }
        case APE_EXPR_LITERALMAP:
        {
            const ApeMapLiteral_t* map = &expr->map;
            ApeSize_t len = ape_ptrarray_count(map->keys);
            ip = ape_compiler_emit(comp, APE_OPCODE_MAPSTART, 1, make_u64_array((uint64_t)len));
            if(ip < 0)
            {
                goto error;
            }

            for(i = 0; i < len; i++)
            {
                ApeExpression_t* key = (ApeExpression_t*)ape_ptrarray_get(map->keys, i);
                ApeExpression_t* val = (ApeExpression_t*)ape_ptrarray_get(map->values, i);

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

            ip = ape_compiler_emit(comp, APE_OPCODE_MAPEND, 1, make_u64_array((uint64_t)len));
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case APE_EXPR_PREFIX:
        {
            ok = ape_compiler_compileexpression(comp, expr->prefix.right);
            if(!ok)
            {
                goto error;
            }

            ApeOpByte_t op = APE_OPCODE_NONE;
            switch(expr->prefix.op)
            {
                case APE_OPERATOR_MINUS:
                    op = APE_OPCODE_MINUS;
                    break;
                case APE_OPERATOR_NOT:
                    op = APE_OPCODE_NOT;
                    break;
                default:
                {
                    ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, expr->pos, "unknown prefix operator.");
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
        case APE_EXPR_IDENT:
        {
            const ApeIdent_t* ident = expr->ident;
            const ApeSymbol_t* symbol = ape_symtable_resolve(symbol_table, ident->value);
            if(!symbol)
            {
                ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, ident->pos, "symbol '%s' could not be resolved",
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
        case APE_EXPR_INDEX:
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
            ip = ape_compiler_emit(comp, APE_OPCODE_GETINDEX, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case APE_EXPR_LITERALFUNCTION:
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
                const ApeSymbol_t* fnsymbol = ape_symtable_definefuncname(symbol_table, fn->name, false);
                if(!fnsymbol)
                {
                    ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, expr->pos, "cannot define symbol '%s'", fn->name);
                    goto error;
                }
            }

            const ApeSymbol_t* thissymbol = ape_symtable_definethis(symbol_table);
            if(!thissymbol)
            {
                ape_errorlist_add(comp->errors, APE_ERROR_COMPILATION, expr->pos, "cannot define symbol 'this'");
                goto error;
            }

            for(i = 0; i < ape_ptrarray_count(expr->fnliteral.params); i++)
            {
                ApeIdent_t* param = (ApeIdent_t*)ape_ptrarray_get(expr->fnliteral.params, i);
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

            if(!ape_compiler_lastopcodeis(comp, APE_OPCODE_RETURNVALUE) && !ape_compiler_lastopcodeis(comp, APE_OPCODE_RETURNNOTHING))
            {
                ip = ape_compiler_emit(comp, APE_OPCODE_RETURNNOTHING, 0, NULL);
                if(ip < 0)
                {
                    goto error;
                }
            }

            ApePtrArray_t* freesymbols = symbol_table->freesymbols;
            symbol_table->freesymbols = NULL;// because it gets destroyed with compiler_pop_compilation_scope()

            ApeInt_t numlocals = symbol_table->maxnumdefinitions;

            ApeCompResult_t* compres = ape_compscope_orphanresult(compilation_scope);
            if(!compres)
            {
                ape_ptrarray_destroywithitems(freesymbols, (ApeDataCallback_t)ape_symbol_destroy);
                goto error;
            }
            ape_compiler_popsymtable(comp);
            ape_compiler_popcompscope(comp);
            compilation_scope = ape_compiler_getcompscope(comp);
            symbol_table = ape_compiler_getsymboltable(comp);

            ApeObject_t obj = ape_object_make_function(comp->context, fn->name, compres, true, numlocals, ape_ptrarray_count(fn->params), 0);

            if(ape_object_value_isnull(obj))
            {
                ape_ptrarray_destroywithitems(freesymbols, (ApeDataCallback_t)ape_symbol_destroy);
                ape_compresult_destroy(compres);
                goto error;
            }

            for(i = 0; i < ape_ptrarray_count(freesymbols); i++)
            {
                ApeSymbol_t* symbol = (ApeSymbol_t*)ape_ptrarray_get(freesymbols, i);
                ok = ape_compiler_readsym(comp, symbol);
                if(!ok)
                {
                    ape_ptrarray_destroywithitems(freesymbols, (ApeDataCallback_t)ape_symbol_destroy);
                    goto error;
                }
            }

            ApeInt_t pos = ape_compiler_addconstant(comp, obj);
            if(pos < 0)
            {
                ape_ptrarray_destroywithitems(freesymbols, (ApeDataCallback_t)ape_symbol_destroy);
                goto error;
            }

            ip = ape_compiler_emit(comp, APE_OPCODE_MKFUNCTION, 2, make_u64_array((uint64_t)pos, (uint64_t)ape_ptrarray_count(freesymbols)));
            if(ip < 0)
            {
                ape_ptrarray_destroywithitems(freesymbols, (ApeDataCallback_t)ape_symbol_destroy);
                goto error;
            }

            ape_ptrarray_destroywithitems(freesymbols, (ApeDataCallback_t)ape_symbol_destroy);

            break;
        }
        case APE_EXPR_CALL:
        {
            ok = ape_compiler_compileexpression(comp, expr->callexpr.function);
            if(!ok)
            {
                goto error;
            }

            for(i = 0; i < ape_ptrarray_count(expr->callexpr.args); i++)
            {
                ApeExpression_t* argexpr = (ApeExpression_t*)ape_ptrarray_get(expr->callexpr.args, i);
                ok = ape_compiler_compileexpression(comp, argexpr);
                if(!ok)
                {
                    goto error;
                }
            }

            ip = ape_compiler_emit(comp, APE_OPCODE_CALL, 1, make_u64_array((uint64_t)ape_ptrarray_count(expr->callexpr.args)));
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case APE_EXPR_ASSIGN:
        {
            const ApeAssignExpr_t* assign = &expr->assign;
            if(assign->dest->type != APE_EXPR_IDENT && assign->dest->type != APE_EXPR_INDEX)
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

            if(assign->dest->type == APE_EXPR_IDENT)
            {
                const ApeIdent_t* ident = assign->dest->ident;
                const ApeSymbol_t* symbol = ape_symtable_resolve(symbol_table, ident->value);
                if(!symbol)
                {
                    //ape_errorlist_addformat(comp->errors, APE_ERROR_COMPILATION, assign->dest->pos, "symbol '%s' could not be resolved", ident->value);
                    //goto error;
                    //ape_symtable_define(ApeSymTable_t* table, const char* name, bool assignable)
                    symbol = ape_symtable_define(symbol_table, ident->value, true);
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
            else if(assign->dest->type == APE_EXPR_INDEX)
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

            ape_valarray_pop(comp->srcpositionsstack, NULL);
            break;
        }
        case APE_EXPR_LOGICAL:
        {
            const ApeLogicalExpr_t* logi = &expr->logical;

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

            ApeInt_t afterleftjumpip = 0;
            if(logi->op == APE_OPERATOR_LOGICALAND)
            {
                afterleftjumpip = ape_compiler_emit(comp, APE_OPCODE_JUMPIFFALSE, 1, make_u64_array((uint64_t)0xbeef));
            }
            else
            {
                afterleftjumpip = ape_compiler_emit(comp, APE_OPCODE_JUMPIFTRUE, 1, make_u64_array((uint64_t)0xbeef));
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

            ApeInt_t afterrightip = ape_compiler_getip(comp);
            ape_compiler_moduint16operand(comp, afterleftjumpip + 1, afterrightip);

            break;
        }
        case APE_EXPR_TERNARY:
        {
            const ApeTernaryExpr_t* ternary = &expr->ternary;

            ok = ape_compiler_compileexpression(comp, ternary->test);
            if(!ok)
            {
                goto error;
            }

            ApeInt_t elsejumpip = ape_compiler_emit(comp, APE_OPCODE_JUMPIFFALSE, 1, make_u64_array((uint64_t)0xbeef));

            ok = ape_compiler_compileexpression(comp, ternary->iftrue);
            if(!ok)
            {
                goto error;
            }

            ApeInt_t endjumpip = ape_compiler_emit(comp, APE_OPCODE_JUMP, 1, make_u64_array((uint64_t)0xbeef));

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
    ape_valarray_pop(comp->srcpositionsstack, NULL);
    ape_ast_destroy_expr(exproptimised);
    return res;
}

static bool ape_compiler_compilecodeblock(ApeCompiler_t* comp, const ApeCodeblock_t* block)
{
    ApeSize_t i;
    ApeSymTable_t* symbol_table = ape_compiler_getsymboltable(comp);
    if(!symbol_table)
    {
        return false;
    }

    bool ok = ape_symtable_pushblockscope(symbol_table);
    if(!ok)
    {
        return false;
    }

    if(ape_ptrarray_count(block->statements) == 0)
    {
        ApeInt_t ip = ape_compiler_emit(comp, APE_OPCODE_NULL, 0, NULL);
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
        const ApeStatement_t* stmt = (ApeStatement_t*)ape_ptrarray_get(block->statements, i);
        bool ok = ape_compiler_compilestatement(comp, stmt);
        if(!ok)
        {
            return false;
        }
    }
    ape_symtable_popblockscope(symbol_table);
    return true;
}

static ApeInt_t ape_compiler_addconstant(ApeCompiler_t* comp, ApeObject_t obj)
{
    bool ok = ape_valarray_add(comp->constants, &obj);
    if(!ok)
    {
        return -1;
    }
    ApeInt_t pos = ape_valarray_count(comp->constants) - 1;
    return pos;
}

static void ape_compiler_moduint16operand(ApeCompiler_t* comp, ApeInt_t ip, uint64_t operand)
{
    ApeValArray_t* bytecode = ape_compiler_getbytecode(comp);
    if((ip + 1) >= (ApeInt_t)ape_valarray_count(bytecode))
    {
        APE_ASSERT(false);
        return;
    }
    ApeUShort_t hi = (ApeUShort_t)(operand >> 8);
    ape_valarray_set(bytecode, ip, &hi);
    ApeUShort_t lo = (ApeUShort_t)(operand);
    ape_valarray_set(bytecode, ip + 1, &lo);
}

static bool ape_compiler_lastopcodeis(ApeCompiler_t* comp, ApeOpByte_t op)
{
    ApeOpByte_t lastopcode = ape_compiler_getlastopcode(comp);
    return lastopcode == op;
}

static bool ape_compiler_readsym(ApeCompiler_t* comp, const ApeSymbol_t* symbol)
{
    ApeInt_t ip = -1;
    if(symbol->type == APE_SYMBOL_MODULEGLOBAL)
    {
        ip = ape_compiler_emit(comp, APE_OPCODE_GETMODULEGLOBAL, 1, make_u64_array((uint64_t)(symbol->index)));
    }
    else if(symbol->type == APE_SYMBOL_CONTEXTGLOBAL)
    {
        ip = ape_compiler_emit(comp, APE_OPCODE_GETCONTEXTGLOBAL, 1, make_u64_array((uint64_t)(symbol->index)));
    }
    else if(symbol->type == APE_SYMBOL_LOCAL)
    {
        ip = ape_compiler_emit(comp, APE_OPCODE_GETLOCAL, 1, make_u64_array((uint64_t)(symbol->index)));
    }
    else if(symbol->type == APE_SYMBOL_FREE)
    {
        ip = ape_compiler_emit(comp, APE_OPCODE_GETFREE, 1, make_u64_array((uint64_t)(symbol->index)));
    }
    else if(symbol->type == APE_SYMBOL_FUNCTION)
    {
        ip = ape_compiler_emit(comp, APE_OPCODE_CURRENTFUNCTION, 0, NULL);
    }
    else if(symbol->type == APE_SYMBOL_THIS)
    {
        ip = ape_compiler_emit(comp, APE_OPCODE_GETTHIS, 0, NULL);
    }
    return ip >= 0;
}

static bool ape_compiler_writesym(ApeCompiler_t* comp, const ApeSymbol_t* symbol, bool define)
{
    ApeInt_t ip = -1;
    if(symbol->type == APE_SYMBOL_MODULEGLOBAL)
    {
        if(define)
        {
            ip = ape_compiler_emit(comp, APE_OPCODE_DEFMODULEGLOBAL, 1, make_u64_array((uint64_t)(symbol->index)));
        }
        else
        {
            ip = ape_compiler_emit(comp, APE_OPCODE_SETMODULEGLOBAL, 1, make_u64_array((uint64_t)(symbol->index)));
        }
    }
    else if(symbol->type == APE_SYMBOL_LOCAL)
    {
        if(define)
        {
            ip = ape_compiler_emit(comp, APE_OPCODE_DEFLOCAL, 1, make_u64_array((uint64_t)(symbol->index)));
        }
        else
        {
            ip = ape_compiler_emit(comp, APE_OPCODE_SETLOCAL, 1, make_u64_array((uint64_t)(symbol->index)));
        }
    }
    else if(symbol->type == APE_SYMBOL_FREE)
    {
        ip = ape_compiler_emit(comp, APE_OPCODE_SETFREE, 1, make_u64_array((uint64_t)(symbol->index)));
    }
    return ip >= 0;
}

static bool ape_compiler_pushbreakip(ApeCompiler_t* comp, ApeInt_t ip)
{
    ApeCompScope_t* compscope = ape_compiler_getcompscope(comp);
    return ape_valarray_push(compscope->breakipstack, &ip);
}

static void ape_compiler_popbreakip(ApeCompiler_t* comp)
{
    ApeCompScope_t* compscope = ape_compiler_getcompscope(comp);
    if(ape_valarray_count(compscope->breakipstack) == 0)
    {
        APE_ASSERT(false);
        return;
    }
    ape_valarray_pop(compscope->breakipstack, NULL);
}

static ApeInt_t ape_compiler_getbreakip(ApeCompiler_t* comp)
{
    ApeCompScope_t* compscope = ape_compiler_getcompscope(comp);
    if(ape_valarray_count(compscope->breakipstack) == 0)
    {
        return -1;
    }
    ApeInt_t* res = (ApeInt_t*)ape_valarray_top(compscope->breakipstack);
    return *res;
}

static bool ape_compiler_pushcontip(ApeCompiler_t* comp, ApeInt_t ip)
{
    ApeCompScope_t* compscope = ape_compiler_getcompscope(comp);
    return ape_valarray_push(compscope->continueipstack, &ip);
}

static void ape_compiler_popcontip(ApeCompiler_t* comp)
{
    ApeCompScope_t* compscope = ape_compiler_getcompscope(comp);
    if(ape_valarray_count(compscope->continueipstack) == 0)
    {
        APE_ASSERT(false);
        return;
    }
    ape_valarray_pop(compscope->continueipstack, NULL);
}

static ApeInt_t ape_compiler_getcontip(ApeCompiler_t* comp)
{
    ApeCompScope_t* compscope = ape_compiler_getcompscope(comp);
    if(ape_valarray_count(compscope->continueipstack) == 0)
    {
        APE_ASSERT(false);
        return -1;
    }
    ApeInt_t* res = (ApeInt_t*)ape_valarray_top(compscope->continueipstack);
    return *res;
}

static ApeInt_t ape_compiler_getip(ApeCompiler_t* comp)
{
    ApeCompScope_t* compilation_scope = ape_compiler_getcompscope(comp);
    return ape_valarray_count(compilation_scope->bytecode);
}

static ApeValArray_t * ape_compiler_getsrcpositions(ApeCompiler_t* comp)
{
    ApeCompScope_t* compilation_scope = ape_compiler_getcompscope(comp);
    return compilation_scope->srcpositions;
}

static ApeValArray_t * ape_compiler_getbytecode(ApeCompiler_t* comp)
{
    ApeCompScope_t* compilation_scope = ape_compiler_getcompscope(comp);
    return compilation_scope->bytecode;
}

static ApeFileScope_t* ape_compiler_makefilescope(ApeCompiler_t* comp, ApeCompiledFile_t* file)
{
    ApeFileScope_t* filescope = (ApeFileScope_t*)ape_allocator_alloc(comp->alloc, sizeof(ApeFileScope_t));
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

static void ape_compiler_destroyfilescope(ApeFileScope_t* scope)
{
    ApeSize_t i;
    for(i = 0; i < ape_ptrarray_count(scope->loadedmodulenames); i++)
    {
        void* name = ape_ptrarray_get(scope->loadedmodulenames, i);
        ape_allocator_free(scope->alloc, name);
    }
    ape_ptrarray_destroy(scope->loadedmodulenames);
    ape_parser_destroy(scope->parser);
    ape_allocator_free(scope->alloc, scope);
}

static bool ape_compiler_pushfilescope(ApeCompiler_t* comp, const char* filepath)
{
    bool ok;
    ApeBlockScope_t* prevsttopscope;
    ApeSymTable_t* prevst;
    ApeFileScope_t* filescope;
    prevst = NULL;
    if(ape_ptrarray_count(comp->filescopes) > 0)
    {
        prevst = ape_compiler_getsymboltable(comp);
    }
    ApeCompiledFile_t* file;
    file = ape_make_compiledfile(comp->context, filepath);
    if(!file)
    {
        return false;
    }

    ok = ape_ptrarray_add(comp->files, file);
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

    ok = ape_ptrarray_push(comp->filescopes, filescope);
    if(!ok)
    {
        ape_compiler_destroyfilescope(filescope);
        return false;
    }

    ApeInt_t globaloffset = 0;
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

static void ape_compiler_popfilescope(ApeCompiler_t* comp)
{
    ApeSymTable_t* currentst;
    ApeBlockScope_t* currentsttopscope;
    ApeSymTable_t* poppedst;
    ApeBlockScope_t* poppedsttopscope;
    ApeFileScope_t* scope;
    ApeInt_t poppednumdefs;
    poppedst = ape_compiler_getsymboltable(comp);
    poppedsttopscope = ape_symtable_getblockscope(poppedst);
    poppednumdefs = poppedsttopscope->numdefinitions;
    while(ape_compiler_getsymboltable(comp))
    {
        ape_compiler_popsymtable(comp);
    }
    scope = (ApeFileScope_t*)ape_ptrarray_top(comp->filescopes);
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

static void ape_compiler_setcompscope(ApeCompiler_t* comp, ApeCompScope_t* scope)
{
    comp->compilation_scope = scope;
}
