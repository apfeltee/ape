
#include "ape.h"

static const ApePosition_t g_ccpriv_src_pos_invalid = { NULL, -1, -1 };

static const ApeSymbol_t *ccpriv_definesym(ApeCompiler_t *comp, ApePosition_t pos, const char *name, bool assignable, bool can_shadow);
static void ccpriv_setsymtable(ApeCompiler_t *comp, ApeSymbolTable_t *table);
static int ccpriv_emit(ApeCompiler_t *comp, ApeOpByte_t op, ApeSize_t operands_count, uint64_t *operands);
static ApeCompilationScope_t *ccpriv_getcompscope(ApeCompiler_t *comp);
static bool ccpriv_pushcompscope(ApeCompiler_t *comp);
static void ccpriv_popcompscope(ApeCompiler_t *comp);
static bool ccpriv_pushsymtable(ApeCompiler_t *comp, int global_offset);
static void ccpriv_popsymtable(ApeCompiler_t *comp);
static ApeOpByte_t ccpriv_getlastopcode(ApeCompiler_t *comp);
static bool ccpriv_compilecode(ApeCompiler_t *comp, const char *code);
static bool ccpriv_compilestmtlist(ApeCompiler_t *comp, ApePtrArray_t *statements);
static bool ccpriv_importmodule(ApeCompiler_t *comp, const ApeStatement_t *import_stmt);
static bool ccpriv_compilestatement(ApeCompiler_t *comp, const ApeStatement_t *stmt);
static bool ccpriv_compileexpression(ApeCompiler_t *comp, ApeExpression_t *expr);
static bool ccpriv_compilecodeblock(ApeCompiler_t *comp, const ApeCodeblock_t *block);
static int ccpriv_addconstant(ApeCompiler_t *comp, ApeObject_t obj);
static void ccpriv_moduint16operand(ApeCompiler_t *comp, int ip, uint16_t operand);
static bool ccpriv_lastopcodeis(ApeCompiler_t *comp, ApeOpByte_t op);
static bool ccpriv_readsym(ApeCompiler_t *comp, const ApeSymbol_t *symbol);
static bool ccpriv_writesym(ApeCompiler_t *comp, const ApeSymbol_t *symbol, bool define);
static bool ccpriv_pushbreakip(ApeCompiler_t *comp, int ip);
static void ccpriv_popbreakip(ApeCompiler_t *comp);
static int ccpriv_getbreakip(ApeCompiler_t *comp);
static bool ccpriv_pushcontip(ApeCompiler_t *comp, int ip);
static void ccpriv_popcontip(ApeCompiler_t *comp);
static int ccpriv_getcontip(ApeCompiler_t *comp);
static int ccpriv_getip(ApeCompiler_t *comp);
static ApeArray_t *ccpriv_getsrcpositions(ApeCompiler_t *comp);
static ApeArray_t *ccpriv_getbytecode(ApeCompiler_t *comp);
static ApeFileScope_t *ccpriv_makefilescope(ApeCompiler_t *comp, ApeCompiledFile_t *file);
static void ccpriv_destroyfilescope(ApeFileScope_t *scope);
static bool ccpriv_pushfilescope(ApeCompiler_t *comp, const char *filepath);
static void ccpriv_popfilescope(ApeCompiler_t *comp);
static void ccpriv_setcompscope(ApeCompiler_t *comp, ApeCompilationScope_t *scope);


static const ApeSymbol_t* ccpriv_definesym(ApeCompiler_t* comp, ApePosition_t pos, const char* name, bool assignable, bool can_shadow)
{
    ApeSymbolTable_t* symbol_table;
    const ApeSymbol_t* current_symbol;
    const ApeSymbol_t* symbol;
    symbol_table = compiler_get_symbol_table(comp);
    if(!can_shadow && !symbol_table_is_top_global_scope(symbol_table))
    {
        current_symbol = symbol_table_resolve(symbol_table, name);
        if(current_symbol)
        {
            errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, pos, "Symbol \"%s\" is already defined", name);
            return NULL;
        }
    }
    symbol = symbol_table_define(symbol_table, name, assignable);
    if(!symbol)
    {
        errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, pos, "Cannot define symbol \"%s\"", name);
        return NULL;
    }
    return symbol;
}


ApeCompiler_t*
compiler_make(ApeAllocator_t* alloc, const ApeConfig_t* config, ApeGCMemory_t* mem, ApeErrorList_t* errors, ApePtrArray_t * files, ApeGlobalStore_t* global_store)
{
    bool ok;
    ApeCompiler_t* comp;
    comp = (ApeCompiler_t*)allocator_malloc(alloc, sizeof(ApeCompiler_t));
    if(!comp)
    {
        return NULL;
    }
    ok = compiler_init(comp, alloc, config, mem, errors, files, global_store);
    if(!ok)
    {
        allocator_free(alloc, comp);
        return NULL;
    }
    return comp;
}

void compiler_destroy(ApeCompiler_t* comp)
{
    ApeAllocator_t* alloc;
    if(!comp)
    {
        return;
    }
    alloc = comp->alloc;
    compiler_deinit(comp);
    allocator_free(alloc, comp);
}

ApeCompilationResult_t* compiler_compile(ApeCompiler_t* comp, const char* code)
{
    bool ok;
    ApeCompiler_t comp_shallow_copy;
    ApeCompilationScope_t* compilation_scope;
    ApeCompilationResult_t* res;
    compilation_scope = ccpriv_getcompscope(comp);
    APE_ASSERT(array_count(comp->src_positions_stack) == 0);
    APE_ASSERT(array_count(compilation_scope->bytecode) == 0);
    APE_ASSERT(array_count(compilation_scope->break_ip_stack) == 0);
    APE_ASSERT(array_count(compilation_scope->continue_ip_stack) == 0);
    array_clear(comp->src_positions_stack);
    array_clear(compilation_scope->bytecode);
    array_clear(compilation_scope->src_positions);
    array_clear(compilation_scope->break_ip_stack);
    array_clear(compilation_scope->continue_ip_stack);
    ok = compiler_init_shallow_copy(&comp_shallow_copy, comp);
    if(!ok)
    {
        return NULL;
    }
    ok = ccpriv_compilecode(comp, code);
    if(!ok)
    {
        goto err;
    }
    compilation_scope = ccpriv_getcompscope(comp);// might've changed
    APE_ASSERT(compilation_scope->outer == NULL);
    compilation_scope = ccpriv_getcompscope(comp);
    res = compilation_scope_orphan_result(compilation_scope);
    if(!res)
    {
        goto err;
    }
    compiler_deinit(&comp_shallow_copy);
    return res;
err:
    compiler_deinit(comp);
    *comp = comp_shallow_copy;
    return NULL;
}

ApeCompilationResult_t* compiler_compile_file(ApeCompiler_t* comp, const char* path)
{
    bool ok;
    char* code;
    ApeCompiledFile_t* file;
    ApeCompilationResult_t* res;
    ApeFileScope_t* file_scope;
    ApeCompiledFile_t* prev_file;
    code = NULL;
    file = NULL;
    res = NULL;
    if(!comp->config->fileio.read_file.read_file)
    {// todo: read code function
        errors_add_error(comp->errors, APE_ERROR_COMPILATION, g_ccpriv_src_pos_invalid, "File read function not configured");
        goto err;
    }
    code = comp->config->fileio.read_file.read_file(comp->config->fileio.read_file.context, path);
    if(!code)
    {
        errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, g_ccpriv_src_pos_invalid, "Reading file \"%s\" failed", path);
        goto err;
    }
    file = compiled_file_make(comp->alloc, path);
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
    APE_ASSERT(ptrarray_count(comp->file_scopes) == 1);
    file_scope = (ApeFileScope_t*)ptrarray_top(comp->file_scopes);
    if(!file_scope)
    {
        goto err;
    }
    prev_file = file_scope->file;// todo: push file scope instead?
    file_scope->file = file;
    res = compiler_compile(comp, code);
    if(!res)
    {
        file_scope->file = prev_file;
        goto err;
    }
    file_scope->file = prev_file;
    allocator_free(comp->alloc, code);
    return res;
err:
    allocator_free(comp->alloc, code);
    return NULL;
}

ApeSymbolTable_t* compiler_get_symbol_table(ApeCompiler_t* comp)
{
    ApeFileScope_t* file_scope;
    file_scope = (ApeFileScope_t*)ptrarray_top(comp->file_scopes);
    if(!file_scope)
    {
        APE_ASSERT(false);
        return NULL;
    }
    return file_scope->symbol_table;
}

static void ccpriv_setsymtable(ApeCompiler_t* comp, ApeSymbolTable_t* table)
{
    ApeFileScope_t* file_scope;
    file_scope = (ApeFileScope_t*)ptrarray_top(comp->file_scopes);
    if(!file_scope)
    {
        APE_ASSERT(false);
        return;
    }
    file_scope->symbol_table = table;
}

ApeArray_t* compiler_get_constants(const ApeCompiler_t* comp)
{
    return comp->constants;
}

// INTERNAL
bool compiler_init(ApeCompiler_t* comp,
                          ApeAllocator_t* alloc,
                          const ApeConfig_t* config,
                          ApeGCMemory_t* mem,
                          ApeErrorList_t* errors,
                          ApePtrArray_t * files,
                          ApeGlobalStore_t* global_store)
{
    bool ok;
    memset(comp, 0, sizeof(ApeCompiler_t));
    comp->alloc = alloc;
    comp->config = config;
    comp->mem = mem;
    comp->errors = errors;
    comp->files = files;
    comp->global_store = global_store;
    comp->file_scopes = ptrarray_make(alloc);
    if(!comp->file_scopes)
    {
        goto err;
    }
    comp->constants = array_make(alloc, ApeObject_t);
    if(!comp->constants)
    {
        goto err;
    }
    comp->src_positions_stack = array_make(alloc, ApePosition_t);
    if(!comp->src_positions_stack)
    {
        goto err;
    }
    comp->modules = dict_make(alloc, (ApeDataCallback_t)module_copy, (ApeDataCallback_t)module_destroy);
    if(!comp->modules)
    {
        goto err;
    }
    ok = ccpriv_pushcompscope(comp);
    if(!ok)
    {
        goto err;
    }
    ok = ccpriv_pushfilescope(comp, "none");
    if(!ok)
    {
        goto err;
    }
    comp->string_constants_positions = dict_make(comp->alloc, NULL, NULL);
    if(!comp->string_constants_positions)
    {
        goto err;
    }

    return true;
err:
    compiler_deinit(comp);
    return false;
}

void compiler_deinit(ApeCompiler_t* comp)
{
    int i;
    int* val;
    if(!comp)
    {
        return;
    }
    for(i = 0; i < dict_count(comp->string_constants_positions); i++)
    {
        val = (int*)dict_get_value_at(comp->string_constants_positions, i);
        allocator_free(comp->alloc, val);
    }
    dict_destroy(comp->string_constants_positions);
    while(ptrarray_count(comp->file_scopes) > 0)
    {
        ccpriv_popfilescope(comp);
    }
    while(ccpriv_getcompscope(comp))
    {
        ccpriv_popcompscope(comp);
    }
    dict_destroy_with_items(comp->modules);
    array_destroy(comp->src_positions_stack);
    array_destroy(comp->constants);
    ptrarray_destroy(comp->file_scopes);
    memset(comp, 0, sizeof(ApeCompiler_t));
}

bool compiler_init_shallow_copy(ApeCompiler_t* copy, ApeCompiler_t* src)
{
    int i;
    bool ok;
    int* val;
    int* val_copy;
    const char* key;
    const char* loaded_name;
    char* loaded_name_copy;
    ApeSymbolTable_t* src_st;
    ApeSymbolTable_t* src_st_copy;
    ApeSymbolTable_t* copy_st;
    ApeDictionary_t* modules_copy;
    ApeArray_t* constants_copy;
    ApeFileScope_t* src_file_scope;
    ApeFileScope_t* copy_file_scope;
    ApePtrArray_t* src_loaded_module_names;
    ApePtrArray_t* copy_loaded_module_names;

    ok = compiler_init(copy, src->alloc, src->config, src->mem, src->errors, src->files, src->global_store);
    if(!ok)
    {
        return false;
    }
    src_st = compiler_get_symbol_table(src);
    APE_ASSERT(ptrarray_count(src->file_scopes) == 1);
    APE_ASSERT(src_st->outer == NULL);
    src_st_copy = symbol_table_copy(src_st);
    if(!src_st_copy)
    {
        goto err;
    }
    copy_st = compiler_get_symbol_table(copy);
    symbol_table_destroy(copy_st);
    copy_st = NULL;
    ccpriv_setsymtable(copy, src_st_copy);
    modules_copy = dict_copy_with_items(src->modules);
    if(!modules_copy)
    {
        goto err;
    }
    dict_destroy_with_items(copy->modules);
    copy->modules = modules_copy;
    constants_copy = array_copy(src->constants);
    if(!constants_copy)
    {
        goto err;
    }
    array_destroy(copy->constants);
    copy->constants = constants_copy;
    for(i = 0; i < dict_count(src->string_constants_positions); i++)
    {
        key = (const char*)dict_get_key_at(src->string_constants_positions, i);
        val = (int*)dict_get_value_at(src->string_constants_positions, i);
        val_copy = (int*)allocator_malloc(src->alloc, sizeof(int));
        if(!val_copy)
        {
            goto err;
        }
        *val_copy = *val;
        ok = dict_set(copy->string_constants_positions, key, val_copy);
        if(!ok)
        {
            allocator_free(src->alloc, val_copy);
            goto err;
        }
    }
    src_file_scope = (ApeFileScope_t*)ptrarray_top(src->file_scopes);
    copy_file_scope = (ApeFileScope_t*)ptrarray_top(copy->file_scopes);
    src_loaded_module_names = src_file_scope->loaded_module_names;
    copy_loaded_module_names = copy_file_scope->loaded_module_names;
    for(i = 0; i < ptrarray_count(src_loaded_module_names); i++)
    {

        loaded_name = (const char*)ptrarray_get(src_loaded_module_names, i);
        loaded_name_copy = util_strdup(copy->alloc, loaded_name);
        if(!loaded_name_copy)
        {
            goto err;
        }
        ok = ptrarray_add(copy_loaded_module_names, loaded_name_copy);
        if(!ok)
        {
            allocator_free(copy->alloc, loaded_name_copy);
            goto err;
        }
    }

    return true;
err:
    compiler_deinit(copy);
    return false;
}

static int ccpriv_emit(ApeCompiler_t* comp, ApeOpByte_t op, ApeSize_t operands_count, uint64_t* operands)
{
    int i;
    int ip;
    int len;
    bool ok;
    ApePosition_t* src_pos;
    ApeCompilationScope_t* compilation_scope;
    ip = ccpriv_getip(comp);
    len = code_make(op, operands_count, operands, ccpriv_getbytecode(comp));
    if(len == 0)
    {
        return -1;
    }
    for(i = 0; i < len; i++)
    {
        src_pos = (ApePosition_t*)array_top(comp->src_positions_stack);
        APE_ASSERT(src_pos->line >= 0);
        APE_ASSERT(src_pos->column >= 0);
        ok = array_add(ccpriv_getsrcpositions(comp), src_pos);
        if(!ok)
        {
            return -1;
        }
    }
    compilation_scope = ccpriv_getcompscope(comp);
    compilation_scope->last_opcode = op;
    return ip;
}

static ApeCompilationScope_t* ccpriv_getcompscope(ApeCompiler_t* comp)
{
    return comp->compilation_scope;
}

static bool ccpriv_pushcompscope(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* current_scope;
    ApeCompilationScope_t* new_scope;
    current_scope = ccpriv_getcompscope(comp);
    new_scope = compilation_scope_make(comp->alloc, current_scope);
    if(!new_scope)
    {
        return false;
    }
    ccpriv_setcompscope(comp, new_scope);
    return true;
}

static void ccpriv_popcompscope(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* current_scope;
    current_scope = ccpriv_getcompscope(comp);
    APE_ASSERT(current_scope);
    ccpriv_setcompscope(comp, current_scope->outer);
    compilation_scope_destroy(current_scope);
}

static bool ccpriv_pushsymtable(ApeCompiler_t* comp, int global_offset)
{
    ApeFileScope_t* file_scope;
    file_scope = (ApeFileScope_t*)ptrarray_top(comp->file_scopes);
    if(!file_scope)
    {
        APE_ASSERT(false);
        return false;
    }
    ApeSymbolTable_t* current_table = file_scope->symbol_table;
    file_scope->symbol_table = symbol_table_make(comp->alloc, current_table, comp->global_store, global_offset);
    if(!file_scope->symbol_table)
    {
        file_scope->symbol_table = current_table;
        return false;
    }
    return true;
}

static void ccpriv_popsymtable(ApeCompiler_t* comp)
{
    ApeFileScope_t* file_scope;
    ApeSymbolTable_t* current_table;
    file_scope = (ApeFileScope_t*)ptrarray_top(comp->file_scopes);
    if(!file_scope)
    {
        APE_ASSERT(false);
        return;
    }
    current_table = file_scope->symbol_table;
    if(!current_table)
    {
        APE_ASSERT(false);
        return;
    }
    file_scope->symbol_table = current_table->outer;
    symbol_table_destroy(current_table);
}

static ApeOpByte_t ccpriv_getlastopcode(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* current_scope;
    current_scope = ccpriv_getcompscope(comp);
    return current_scope->last_opcode;
}

static bool ccpriv_compilecode(ApeCompiler_t* comp, const char* code)
{
    bool ok;
    ApeFileScope_t* file_scope;
    ApePtrArray_t* statements;
    file_scope = (ApeFileScope_t*)ptrarray_top(comp->file_scopes);
    APE_ASSERT(file_scope);
    statements = parser_parse_all(file_scope->parser, code, file_scope->file);
    if(!statements)
    {
        // errors are added by parser
        return false;
    }
    ok = ccpriv_compilestmtlist(comp, statements);
    ptrarray_destroy_with_items(statements, (ApeDataCallback_t)statement_destroy);

    // Left for debugging purposes
    //    if (ok) {
    //        ApeStringBuffer_t *buf = strbuf_make(NULL);
    //        code_tostring(array_data(comp->compilation_scope->bytecode),
    //                       array_data(comp->compilation_scope->src_positions),
    //                       array_count(comp->compilation_scope->bytecode), buf);
    //        puts(strbuf_get_string(buf));
    //        strbuf_destroy(buf);
    //    }

    return ok;
}

static bool ccpriv_compilestmtlist(ApeCompiler_t* comp, ApePtrArray_t * statements)
{
    ApeSize_t i;
    bool ok;
    const ApeStatement_t* stmt;
    ok = true;
    for(i = 0; i < ptrarray_count(statements); i++)
    {
        stmt = (const ApeStatement_t*)ptrarray_get(statements, i);
        ok = ccpriv_compilestatement(comp, stmt);
        if(!ok)
        {
            break;
        }
    }
    return ok;
}

static bool ccpriv_importmodule(ApeCompiler_t* comp, const ApeStatement_t* import_stmt)
{
    // todo: split into smaller functions
    ApeSize_t i;
    bool ok;
    bool result;
    char* filepath;
    char* code;
    char* name_copy;
    const char* loaded_name;
    const char* module_path;
    const char* module_name;
    const char* filepath_non_canonicalised;
    ApeFileScope_t* file_scope;
    ApeStringBuffer_t* filepath_buf;
    ApeSymbolTable_t* symbol_table;
    ApeFileScope_t* fs;
    ApeModule_t* module;
    ApeSymbolTable_t* st;
    ApeSymbol_t* symbol;
    result = false;
    filepath = NULL;
    code = NULL;
    file_scope = (ApeFileScope_t*)ptrarray_top(comp->file_scopes);
    module_path = import_stmt->import.path;
    module_name = get_module_name(module_path);
    for(i = 0; i < ptrarray_count(file_scope->loaded_module_names); i++)
    {
        loaded_name = (const char*)ptrarray_get(file_scope->loaded_module_names, i);
        if(kg_streq(loaded_name, module_name))
        {
            errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, import_stmt->pos, "Module \"%s\" was already imported", module_name);
            result = false;
            goto end;
        }
    }
    filepath_buf = strbuf_make(comp->alloc);
    if(!filepath_buf)
    {
        result = false;
        goto end;
    }
    if(kg_is_path_absolute(module_path))
    {
        strbuf_appendf(filepath_buf, "%s.ape", module_path);
    }
    else
    {
        strbuf_appendf(filepath_buf, "%s%s.ape", file_scope->file->dir_path, module_path);
    }
    if(strbuf_failed(filepath_buf))
    {
        strbuf_destroy(filepath_buf);
        result = false;
        goto end;
    }
    filepath_non_canonicalised = strbuf_get_string(filepath_buf);
    filepath = kg_canonicalise_path(comp->alloc, filepath_non_canonicalised);
    strbuf_destroy(filepath_buf);
    if(!filepath)
    {
        result = false;
        goto end;
    }
    symbol_table = compiler_get_symbol_table(comp);
    if(symbol_table->outer != NULL || ptrarray_count(symbol_table->block_scopes) > 1)
    {
        errors_add_error(comp->errors, APE_ERROR_COMPILATION, import_stmt->pos, "Modules can only be imported in global scope");
        result = false;
        goto end;
    }
    for(i = 0; i < ptrarray_count(comp->file_scopes); i++)
    {
        fs = (ApeFileScope_t*)ptrarray_get(comp->file_scopes, i);
        if(APE_STREQ(fs->file->path, filepath))
        {
            errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, import_stmt->pos, "Cyclic reference of file \"%s\"", filepath);
            result = false;
            goto end;
        }
    }
    module = (ApeModule_t*)dict_get(comp->modules, filepath);
    if(!module)
    {
        // todo: create new module function
        if(!comp->config->fileio.read_file.read_file)
        {
            errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, import_stmt->pos,
                              "Cannot import module \"%s\", file read function not configured", filepath);
            result = false;
            goto end;
        }
        code = comp->config->fileio.read_file.read_file(comp->config->fileio.read_file.context, filepath);
        if(!code)
        {
            errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, import_stmt->pos, "Reading module file \"%s\" failed", filepath);
            result = false;
            goto end;
        }
        module = module_make(comp->alloc, module_name);
        if(!module)
        {
            result = false;
            goto end;
        }
        ok = ccpriv_pushfilescope(comp, filepath);
        if(!ok)
        {
            module_destroy(module);
            result = false;
            goto end;
        }
        ok = ccpriv_compilecode(comp, code);
        if(!ok)
        {
            module_destroy(module);
            result = false;
            goto end;
        }
        st = compiler_get_symbol_table(comp);
        for(i = 0; i < symbol_table_get_module_global_symbol_count(st); i++)
        {
            symbol = (ApeSymbol_t*)symbol_table_get_module_global_symbol_at(st, i);
            module_add_symbol(module, symbol);
        }
        ccpriv_popfilescope(comp);
        ok = dict_set(comp->modules, filepath, module);
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
    name_copy = util_strdup(comp->alloc, module_name);
    if(!name_copy)
    {
        result = false;
        goto end;
    }
    ok = ptrarray_add(file_scope->loaded_module_names, name_copy);
    if(!ok)
    {
        allocator_free(comp->alloc, name_copy);
        result = false;
        goto end;
    }
    result = true;
end:
    allocator_free(comp->alloc, filepath);
    allocator_free(comp->alloc, code);
    return result;
}

static bool ccpriv_compilestatement(ApeCompiler_t* comp, const ApeStatement_t* stmt)
{
    ApeSize_t i;
    int ip;
    int next_case_jump_ip;
    int after_alt_ip;
    int after_elif_ip;
    int* pos;
    int jump_to_end_ip;
    int before_test_ip;
    int after_test_ip;
    int jump_to_after_body_ip;
    int after_body_ip;
    bool ok;

    const ApeWhileLoopStmt_t* loop;
    ApeCompilationScope_t* compilation_scope;
    ApeSymbolTable_t* symbol_table;
    const ApeSymbol_t* symbol;
    const ApeIfStmt_t* if_stmt;
    ApeArray_t* jump_to_end_ips;
    ApeIfCase_t* if_case;
    ok = false;
    ip = -1;
    ok = array_push(comp->src_positions_stack, &stmt->pos);
    if(!ok)
    {
        return false;
    }
    compilation_scope = ccpriv_getcompscope(comp);
    symbol_table = compiler_get_symbol_table(comp);
    switch(stmt->type)
    {
        case STATEMENT_EXPRESSION:
            {
                ok = ccpriv_compileexpression(comp, stmt->expression);
                if(!ok)
                {
                    return false;
                }
                ip = ccpriv_emit(comp, OPCODE_POP, 0, NULL);
                if(ip < 0)
                {
                    return false;
                }
            }
            break;

        case STATEMENT_DEFINE:
            {
                ok = ccpriv_compileexpression(comp, stmt->define.value);
                if(!ok)
                {
                    return false;
                }
                symbol = ccpriv_definesym(comp, stmt->define.name->pos, stmt->define.name->value, stmt->define.assignable, false);
                if(!symbol)
                {
                    return false;
                }
                ok = ccpriv_writesym(comp, symbol, true);
                if(!ok)
                {
                    return false;
                }
            }
            break;

        case STATEMENT_IF:
            {
                if_stmt = &stmt->if_statement;
                jump_to_end_ips = array_make(comp->alloc, int);
                if(!jump_to_end_ips)
                {
                    goto statement_if_error;
                }
                for(i = 0; i < ptrarray_count(if_stmt->cases); i++)
                {
                    if_case = (ApeIfCase_t*)ptrarray_get(if_stmt->cases, i);
                    ok = ccpriv_compileexpression(comp, if_case->test);
                    if(!ok)
                    {
                        goto statement_if_error;
                    }
                    next_case_jump_ip = ccpriv_emit(comp, OPCODE_JUMP_IF_FALSE, 1, (uint64_t[]){ (uint64_t)(0xbeef) });
                    ok = ccpriv_compilecodeblock(comp, if_case->consequence);
                    if(!ok)
                    {
                        goto statement_if_error;
                    }
                    // don't ccpriv_emit jump for the last statement
                    if(i < (ptrarray_count(if_stmt->cases) - 1) || if_stmt->alternative)
                    {

                        jump_to_end_ip = ccpriv_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)(0xbeef) });
                        ok = array_add(jump_to_end_ips, &jump_to_end_ip);
                        if(!ok)
                        {
                            goto statement_if_error;
                        }
                    }
                    after_elif_ip = ccpriv_getip(comp);
                    ccpriv_moduint16operand(comp, next_case_jump_ip + 1, after_elif_ip);
                }
                if(if_stmt->alternative)
                {
                    ok = ccpriv_compilecodeblock(comp, if_stmt->alternative);
                    if(!ok)
                    {
                        goto statement_if_error;
                    }
                }
                after_alt_ip = ccpriv_getip(comp);
                for(i = 0; i < array_count(jump_to_end_ips); i++)
                {
                    pos = (int*)array_get(jump_to_end_ips, i);
                    ccpriv_moduint16operand(comp, *pos + 1, after_alt_ip);
                }
                array_destroy(jump_to_end_ips);

                break;
            statement_if_error:
                array_destroy(jump_to_end_ips);
                return false;
            }
            break;
        case STATEMENT_RETURN_VALUE:
            {
                if(compilation_scope->outer == NULL)
                {
                    errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "Nothing to return from");
                    return false;
                }
                ip = -1;
                if(stmt->return_value)
                {
                    ok = ccpriv_compileexpression(comp, stmt->return_value);
                    if(!ok)
                    {
                        return false;
                    }
                    ip = ccpriv_emit(comp, OPCODE_RETURN_VALUE, 0, NULL);
                }
                else
                {
                    ip = ccpriv_emit(comp, OPCODE_RETURN, 0, NULL);
                }
                if(ip < 0)
                {
                    return false;
                }
            }
            break;
        case STATEMENT_WHILE_LOOP:
            {
                loop = &stmt->while_loop;
                before_test_ip = ccpriv_getip(comp);
                ok = ccpriv_compileexpression(comp, loop->test);
                if(!ok)
                {
                    return false;
                }
                after_test_ip = ccpriv_getip(comp);
                ip = ccpriv_emit(comp, OPCODE_JUMP_IF_TRUE, 1, (uint64_t[]){ (uint64_t)(after_test_ip + 6) });
                if(ip < 0)
                {
                    return false;
                }
                jump_to_after_body_ip = ccpriv_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)0xdead });
                if(jump_to_after_body_ip < 0)
                {
                    return false;
                }
                ok = ccpriv_pushcontip(comp, before_test_ip);
                if(!ok)
                {
                    return false;
                }
                ok = ccpriv_pushbreakip(comp, jump_to_after_body_ip);
                if(!ok)
                {
                    return false;
                }
                ok = ccpriv_compilecodeblock(comp, loop->body);
                if(!ok)
                {
                    return false;
                }
                ccpriv_popbreakip(comp);
                ccpriv_popcontip(comp);
                ip = ccpriv_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)before_test_ip });
                if(ip < 0)
                {
                    return false;
                }
                after_body_ip = ccpriv_getip(comp);
                ccpriv_moduint16operand(comp, jump_to_after_body_ip + 1, after_body_ip);
            }
            break;

        case STATEMENT_BREAK:
        {
            int break_ip = ccpriv_getbreakip(comp);
            if(break_ip < 0)
            {
                errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "Nothing to break from.");
                return false;
            }
            ip = ccpriv_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)break_ip });
            if(ip < 0)
            {
                return false;
            }
            break;
        }
        case STATEMENT_CONTINUE:
        {
            int continue_ip = ccpriv_getcontip(comp);
            if(continue_ip < 0)
            {
                errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "Nothing to continue from.");
                return false;
            }
            ip = ccpriv_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)continue_ip });
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
            const ApeSymbol_t* index_symbol = ccpriv_definesym(comp, stmt->pos, "@i", false, true);
            if(!index_symbol)
            {
                return false;
            }

            ip = ccpriv_emit(comp, OPCODE_NUMBER, 1, (uint64_t[]){ (uint64_t)0 });
            if(ip < 0)
            {
                return false;
            }

            ok = ccpriv_writesym(comp, index_symbol, true);
            if(!ok)
            {
                return false;
            }

            const ApeSymbol_t* source_symbol = NULL;
            if(foreach->source->type == EXPRESSION_IDENT)
            {
                source_symbol = symbol_table_resolve(symbol_table, foreach->source->ident->value);
                if(!source_symbol)
                {
                    errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, foreach->source->pos,
                                      "Symbol \"%s\" could not be resolved", foreach->source->ident->value);
                    return false;
                }
            }
            else
            {
                ok = ccpriv_compileexpression(comp, foreach->source);
                if(!ok)
                {
                    return false;
                }
                source_symbol = ccpriv_definesym(comp, foreach->source->pos, "@source", false, true);
                if(!source_symbol)
                {
                    return false;
                }
                ok = ccpriv_writesym(comp, source_symbol, true);
                if(!ok)
                {
                    return false;
                }
            }

            // Update
            int jump_to_after_update_ip = ccpriv_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)0xbeef });
            if(jump_to_after_update_ip < 0)
            {
                return false;
            }

            int update_ip = ccpriv_getip(comp);
            ok = ccpriv_readsym(comp, index_symbol);
            if(!ok)
            {
                return false;
            }

            ip = ccpriv_emit(comp, OPCODE_NUMBER, 1, (uint64_t[]){ (uint64_t)util_double_to_uint64(1) });
            if(ip < 0)
            {
                return false;
            }

            ip = ccpriv_emit(comp, OPCODE_ADD, 0, NULL);
            if(ip < 0)
            {
                return false;
            }

            ok = ccpriv_writesym(comp, index_symbol, false);
            if(!ok)
            {
                return false;
            }

            int after_update_ip = ccpriv_getip(comp);
            ccpriv_moduint16operand(comp, jump_to_after_update_ip + 1, after_update_ip);

            // Test
            ok = array_push(comp->src_positions_stack, &foreach->source->pos);
            if(!ok)
            {
                return false;
            }

            ok = ccpriv_readsym(comp, source_symbol);
            if(!ok)
            {
                return false;
            }

            ip = ccpriv_emit(comp, OPCODE_LEN, 0, NULL);
            if(ip < 0)
            {
                return false;
            }

            array_pop(comp->src_positions_stack, NULL);
            ok = ccpriv_readsym(comp, index_symbol);
            if(!ok)
            {
                return false;
            }

            ip = ccpriv_emit(comp, OPCODE_COMPARE, 0, NULL);
            if(ip < 0)
            {
                return false;
            }

            ip = ccpriv_emit(comp, OPCODE_EQUAL, 0, NULL);
            if(ip < 0)
            {
                return false;
            }

            int after_test_ip = ccpriv_getip(comp);
            ip = ccpriv_emit(comp, OPCODE_JUMP_IF_FALSE, 1, (uint64_t[]){ (uint64_t)(after_test_ip + 6) });
            if(ip < 0)
            {
                return false;
            }

            int jump_to_after_body_ip = ccpriv_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)0xdead });
            if(jump_to_after_body_ip < 0)
            {
                return false;
            }

            ok = ccpriv_readsym(comp, source_symbol);
            if(!ok)
            {
                return false;
            }

            ok = ccpriv_readsym(comp, index_symbol);
            if(!ok)
            {
                return false;
            }

            ip = ccpriv_emit(comp, OPCODE_GET_VALUE_AT, 0, NULL);
            if(ip < 0)
            {
                return false;
            }

            const ApeSymbol_t* iter_symbol = ccpriv_definesym(comp, foreach->iterator->pos, foreach->iterator->value, false, false);
            if(!iter_symbol)
            {
                return false;
            }

            ok = ccpriv_writesym(comp, iter_symbol, true);
            if(!ok)
            {
                return false;
            }

            // Body
            ok = ccpriv_pushcontip(comp, update_ip);
            if(!ok)
            {
                return false;
            }

            ok = ccpriv_pushbreakip(comp, jump_to_after_body_ip);
            if(!ok)
            {
                return false;
            }

            ok = ccpriv_compilecodeblock(comp, foreach->body);
            if(!ok)
            {
                return false;
            }

            ccpriv_popbreakip(comp);
            ccpriv_popcontip(comp);

            ip = ccpriv_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)update_ip });
            if(ip < 0)
            {
                return false;
            }

            int after_body_ip = ccpriv_getip(comp);
            ccpriv_moduint16operand(comp, jump_to_after_body_ip + 1, after_body_ip);

            symbol_table_pop_block_scope(symbol_table);
            break;
        }
        case STATEMENT_FOR_LOOP:
        {
            const ApeForLoopStmt_t* loop = &stmt->for_loop;

            ok = symbol_table_push_block_scope(symbol_table);
            if(!ok)
            {
                return false;
            }

            // Init
            int jump_to_after_update_ip = 0;
            bool ok = false;
            if(loop->init)
            {
                ok = ccpriv_compilestatement(comp, loop->init);
                if(!ok)
                {
                    return false;
                }
                jump_to_after_update_ip = ccpriv_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)0xbeef });
                if(jump_to_after_update_ip < 0)
                {
                    return false;
                }
            }

            // Update
            int update_ip = ccpriv_getip(comp);
            if(loop->update)
            {
                ok = ccpriv_compileexpression(comp, loop->update);
                if(!ok)
                {
                    return false;
                }
                ip = ccpriv_emit(comp, OPCODE_POP, 0, NULL);
                if(ip < 0)
                {
                    return false;
                }
            }

            if(loop->init)
            {
                int after_update_ip = ccpriv_getip(comp);
                ccpriv_moduint16operand(comp, jump_to_after_update_ip + 1, after_update_ip);
            }

            // Test
            if(loop->test)
            {
                ok = ccpriv_compileexpression(comp, loop->test);
                if(!ok)
                {
                    return false;
                }
            }
            else
            {
                ip = ccpriv_emit(comp, OPCODE_TRUE, 0, NULL);
                if(ip < 0)
                {
                    return false;
                }
            }
            int after_test_ip = ccpriv_getip(comp);

            ip = ccpriv_emit(comp, OPCODE_JUMP_IF_TRUE, 1, (uint64_t[]){ (uint64_t)(after_test_ip + 6) });
            if(ip < 0)
            {
                return false;
            }
            int jmp_to_after_body_ip = ccpriv_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)0xdead });
            if(jmp_to_after_body_ip < 0)
            {
                return false;
            }

            // Body
            ok = ccpriv_pushcontip(comp, update_ip);
            if(!ok)
            {
                return false;
            }

            ok = ccpriv_pushbreakip(comp, jmp_to_after_body_ip);
            if(!ok)
            {
                return false;
            }

            ok = ccpriv_compilecodeblock(comp, loop->body);
            if(!ok)
            {
                return false;
            }

            ccpriv_popbreakip(comp);
            ccpriv_popcontip(comp);

            ip = ccpriv_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)update_ip });
            if(ip < 0)
            {
                return false;
            }

            int after_body_ip = ccpriv_getip(comp);
            ccpriv_moduint16operand(comp, jmp_to_after_body_ip + 1, after_body_ip);

            symbol_table_pop_block_scope(symbol_table);
            break;
        }
        case STATEMENT_BLOCK:
        {
            ok = ccpriv_compilecodeblock(comp, stmt->block);
            if(!ok)
            {
                return false;
            }
            break;
        }
        case STATEMENT_IMPORT:
        {
            ok = ccpriv_importmodule(comp, stmt);
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
                errors_add_error(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "Recover statement cannot be defined in global scope");
                return false;
            }

            if(!symbol_table_is_top_block_scope(symbol_table))
            {
                errors_add_error(comp->errors, APE_ERROR_COMPILATION, stmt->pos,
                                 "Recover statement cannot be defined within other statements");
                return false;
            }

            int recover_ip = ccpriv_emit(comp, OPCODE_SET_RECOVER, 1, (uint64_t[]){ (uint64_t)0xbeef });
            if(recover_ip < 0)
            {
                return false;
            }

            int jump_to_after_recover_ip = ccpriv_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)0xbeef });
            if(jump_to_after_recover_ip < 0)
            {
                return false;
            }

            int after_jump_to_recover_ip = ccpriv_getip(comp);
            ccpriv_moduint16operand(comp, recover_ip + 1, after_jump_to_recover_ip);

            ok = symbol_table_push_block_scope(symbol_table);
            if(!ok)
            {
                return false;
            }

            const ApeSymbol_t* error_symbol
            = ccpriv_definesym(comp, recover->error_ident->pos, recover->error_ident->value, false, false);
            if(!error_symbol)
            {
                return false;
            }

            ok = ccpriv_writesym(comp, error_symbol, true);
            if(!ok)
            {
                return false;
            }

            ok = ccpriv_compilecodeblock(comp, recover->body);
            if(!ok)
            {
                return false;
            }

            if(!ccpriv_lastopcodeis(comp, OPCODE_RETURN) && !ccpriv_lastopcodeis(comp, OPCODE_RETURN_VALUE))
            {
                errors_add_error(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "Recover body must end with a return statement");
                return false;
            }

            symbol_table_pop_block_scope(symbol_table);

            int after_recover_ip = ccpriv_getip(comp);
            ccpriv_moduint16operand(comp, jump_to_after_recover_ip + 1, after_recover_ip);

            break;
        }
        default:
        {
            APE_ASSERT(false);
            return false;
        }
    }
    array_pop(comp->src_positions_stack, NULL);
    return true;
}

static bool ccpriv_compileexpression(ApeCompiler_t* comp, ApeExpression_t* expr)
{
    ApeSize_t i;
    bool ok = false;
    int ip = -1;

    ApeExpression_t* expr_optimised = optimise_expression(expr);
    if(expr_optimised)
    {
        expr = expr_optimised;
    }

    ok = array_push(comp->src_positions_stack, &expr->pos);
    if(!ok)
    {
        return false;
    }

    ApeCompilationScope_t* compilation_scope = ccpriv_getcompscope(comp);
    ApeSymbolTable_t* symbol_table = compiler_get_symbol_table(comp);

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
                    op = OPCODE_AND;
                    break;
                case OPERATOR_LSHIFT:
                    op = OPCODE_LSHIFT;
                    break;
                case OPERATOR_RSHIFT:
                    op = OPCODE_RSHIFT;
                    break;
                default:
                {
                    errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, expr->pos, "Unknown infix operator");
                    goto error;
                }
            }

            ApeExpression_t* left = rearrange ? expr->infix.right : expr->infix.left;
            ApeExpression_t* right = rearrange ? expr->infix.left : expr->infix.right;

            ok = ccpriv_compileexpression(comp, left);
            if(!ok)
            {
                goto error;
            }

            ok = ccpriv_compileexpression(comp, right);
            if(!ok)
            {
                goto error;
            }

            switch(expr->infix.op)
            {
                case OPERATOR_EQ:
                case OPERATOR_NOT_EQ:
                {
                    ip = ccpriv_emit(comp, OPCODE_COMPARE_EQ, 0, NULL);
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
                    ip = ccpriv_emit(comp, OPCODE_COMPARE, 0, NULL);
                    if(ip < 0)
                    {
                        goto error;
                    }
                    break;
                }
                default:
                    break;
            }

            ip = ccpriv_emit(comp, op, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_NUMBER_LITERAL:
        {
            ApeFloat_t number = expr->number_literal;
            ip = ccpriv_emit(comp, OPCODE_NUMBER, 1, (uint64_t[]){ (uint64_t)util_double_to_uint64(number) });
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_STRING_LITERAL:
        {
            int pos = 0;
            int* current_pos = (int*)dict_get(comp->string_constants_positions, expr->string_literal);
            if(current_pos)
            {
                pos = *current_pos;
            }
            else
            {
                ApeObject_t obj = object_make_string(comp->mem, expr->string_literal);
                if(object_is_null(obj))
                {
                    goto error;
                }

                pos = ccpriv_addconstant(comp, obj);
                if(pos < 0)
                {
                    goto error;
                }

                int* pos_val = (int*)allocator_malloc(comp->alloc, sizeof(int));
                if(!pos_val)
                {
                    goto error;
                }

                *pos_val = pos;
                ok = dict_set(comp->string_constants_positions, expr->string_literal, pos_val);
                if(!ok)
                {
                    allocator_free(comp->alloc, pos_val);
                    goto error;
                }
            }

            ip = ccpriv_emit(comp, OPCODE_CONSTANT, 1, (uint64_t[]){ (uint64_t)pos });
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_NULL_LITERAL:
        {
            ip = ccpriv_emit(comp, OPCODE_NULL, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }
            break;
        }
        case EXPRESSION_BOOL_LITERAL:
        {
            ip = ccpriv_emit(comp, expr->bool_literal ? OPCODE_TRUE : OPCODE_FALSE, 0, NULL);
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
                ok = ccpriv_compileexpression(comp, (ApeExpression_t*)ptrarray_get(expr->array, i));
                if(!ok)
                {
                    goto error;
                }
            }
            ip = ccpriv_emit(comp, OPCODE_ARRAY, 1, (uint64_t[]){ (uint64_t)ptrarray_count(expr->array) });
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
            ip = ccpriv_emit(comp, OPCODE_MAP_START, 1, (uint64_t[]){ (uint64_t)len });
            if(ip < 0)
            {
                goto error;
            }

            for(i = 0; i < len; i++)
            {
                ApeExpression_t* key = (ApeExpression_t*)ptrarray_get(map->keys, i);
                ApeExpression_t* val = (ApeExpression_t*)ptrarray_get(map->values, i);

                ok = ccpriv_compileexpression(comp, key);
                if(!ok)
                {
                    goto error;
                }

                ok = ccpriv_compileexpression(comp, val);
                if(!ok)
                {
                    goto error;
                }
            }

            ip = ccpriv_emit(comp, OPCODE_MAP_END, 1, (uint64_t[]){ (uint64_t)len });
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_PREFIX:
        {
            ok = ccpriv_compileexpression(comp, expr->prefix.right);
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
                    errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, expr->pos, "Unknown prefix operator.");
                    goto error;
                }
            }
            ip = ccpriv_emit(comp, op, 0, NULL);
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
                errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, ident->pos, "Symbol \"%s\" could not be resolved",
                                  ident->value);
                goto error;
            }
            ok = ccpriv_readsym(comp, symbol);
            if(!ok)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_INDEX:
        {
            const ApeIndexExpr_t* index = &expr->index_expr;
            ok = ccpriv_compileexpression(comp, index->left);
            if(!ok)
            {
                goto error;
            }
            ok = ccpriv_compileexpression(comp, index->index);
            if(!ok)
            {
                goto error;
            }
            ip = ccpriv_emit(comp, OPCODE_GET_INDEX, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_FUNCTION_LITERAL:
        {
            const ApeFnLiteral_t* fn = &expr->fn_literal;

            ok = ccpriv_pushcompscope(comp);
            if(!ok)
            {
                goto error;
            }

            ok = ccpriv_pushsymtable(comp, 0);
            if(!ok)
            {
                goto error;
            }

            compilation_scope = ccpriv_getcompscope(comp);
            symbol_table = compiler_get_symbol_table(comp);

            if(fn->name)
            {
                const ApeSymbol_t* fn_symbol = symbol_table_define_function_name(symbol_table, fn->name, false);
                if(!fn_symbol)
                {
                    errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, expr->pos, "Cannot define symbol \"%s\"", fn->name);
                    goto error;
                }
            }

            const ApeSymbol_t* this_symbol = symbol_table_define_this(symbol_table);
            if(!this_symbol)
            {
                errors_add_error(comp->errors, APE_ERROR_COMPILATION, expr->pos, "Cannot define \"this\" symbol");
                goto error;
            }

            for(i = 0; i < ptrarray_count(expr->fn_literal.params); i++)
            {
                ApeIdent_t* param = (ApeIdent_t*)ptrarray_get(expr->fn_literal.params, i);
                const ApeSymbol_t* param_symbol = ccpriv_definesym(comp, param->pos, param->value, true, false);
                if(!param_symbol)
                {
                    goto error;
                }
            }

            ok = ccpriv_compilestmtlist(comp, fn->body->statements);
            if(!ok)
            {
                goto error;
            }

            if(!ccpriv_lastopcodeis(comp, OPCODE_RETURN_VALUE) && !ccpriv_lastopcodeis(comp, OPCODE_RETURN))
            {
                ip = ccpriv_emit(comp, OPCODE_RETURN, 0, NULL);
                if(ip < 0)
                {
                    goto error;
                }
            }

            ApePtrArray_t* free_symbols = symbol_table->free_symbols;
            symbol_table->free_symbols = NULL;// because it gets destroyed with compiler_pop_compilation_scope()

            int num_locals = symbol_table->max_num_definitions;

            ApeCompilationResult_t* comp_res = compilation_scope_orphan_result(compilation_scope);
            if(!comp_res)
            {
                ptrarray_destroy_with_items(free_symbols, (ApeDataCallback_t)symbol_destroy);
                goto error;
            }
            ccpriv_popsymtable(comp);
            ccpriv_popcompscope(comp);
            compilation_scope = ccpriv_getcompscope(comp);
            symbol_table = compiler_get_symbol_table(comp);

            ApeObject_t obj = object_make_function(comp->mem, fn->name, comp_res, true, num_locals, ptrarray_count(fn->params), 0);

            if(object_is_null(obj))
            {
                ptrarray_destroy_with_items(free_symbols, (ApeDataCallback_t)symbol_destroy);
                compilation_result_destroy(comp_res);
                goto error;
            }

            for(i = 0; i < ptrarray_count(free_symbols); i++)
            {
                ApeSymbol_t* symbol = (ApeSymbol_t*)ptrarray_get(free_symbols, i);
                ok = ccpriv_readsym(comp, symbol);
                if(!ok)
                {
                    ptrarray_destroy_with_items(free_symbols, (ApeDataCallback_t)symbol_destroy);
                    goto error;
                }
            }

            int pos = ccpriv_addconstant(comp, obj);
            if(pos < 0)
            {
                ptrarray_destroy_with_items(free_symbols, (ApeDataCallback_t)symbol_destroy);
                goto error;
            }

            ip = ccpriv_emit(comp, OPCODE_FUNCTION, 2, (uint64_t[]){ (uint64_t)pos, (uint64_t)ptrarray_count(free_symbols) });
            if(ip < 0)
            {
                ptrarray_destroy_with_items(free_symbols, (ApeDataCallback_t)symbol_destroy);
                goto error;
            }

            ptrarray_destroy_with_items(free_symbols, (ApeDataCallback_t)symbol_destroy);

            break;
        }
        case EXPRESSION_CALL:
        {
            ok = ccpriv_compileexpression(comp, expr->call_expr.function);
            if(!ok)
            {
                goto error;
            }

            for(i = 0; i < ptrarray_count(expr->call_expr.args); i++)
            {
                ApeExpression_t* arg_expr = (ApeExpression_t*)ptrarray_get(expr->call_expr.args, i);
                ok = ccpriv_compileexpression(comp, arg_expr);
                if(!ok)
                {
                    goto error;
                }
            }

            ip = ccpriv_emit(comp, OPCODE_CALL, 1, (uint64_t[]){ (uint64_t)ptrarray_count(expr->call_expr.args) });
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
                errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, assign->dest->pos, "Expression is not assignable.");
                goto error;
            }

            if(assign->is_postfix)
            {
                ok = ccpriv_compileexpression(comp, assign->dest);
                if(!ok)
                {
                    goto error;
                }
            }

            ok = ccpriv_compileexpression(comp, assign->source);
            if(!ok)
            {
                goto error;
            }

            ip = ccpriv_emit(comp, OPCODE_DUP, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }

            ok = array_push(comp->src_positions_stack, &assign->dest->pos);
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
                    //errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, assign->dest->pos, "Symbol \"%s\" could not be resolved", ident->value);
                    //goto error;
                    //symbol_table_define(ApeSymbolTable_t* table, const char* name, bool assignable)
                    symbol = symbol_table_define(symbol_table, ident->value, true);
                }
                if(!symbol->assignable)
                {
                    errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, assign->dest->pos,
                                      "Symbol \"%s\" is not assignable", ident->value);
                    goto error;
                }
                ok = ccpriv_writesym(comp, symbol, false);
                if(!ok)
                {
                    goto error;
                }
            }
            else if(assign->dest->type == EXPRESSION_INDEX)
            {
                const ApeIndexExpr_t* index = &assign->dest->index_expr;
                ok = ccpriv_compileexpression(comp, index->left);
                if(!ok)
                {
                    goto error;
                }
                ok = ccpriv_compileexpression(comp, index->index);
                if(!ok)
                {
                    goto error;
                }
                ip = ccpriv_emit(comp, OPCODE_SET_INDEX, 0, NULL);
                if(ip < 0)
                {
                    goto error;
                }
            }

            if(assign->is_postfix)
            {
                ip = ccpriv_emit(comp, OPCODE_POP, 0, NULL);
                if(ip < 0)
                {
                    goto error;
                }
            }

            array_pop(comp->src_positions_stack, NULL);
            break;
        }
        case EXPRESSION_LOGICAL:
        {
            const ApeLogicalExpr_t* logi = &expr->logical;

            ok = ccpriv_compileexpression(comp, logi->left);
            if(!ok)
            {
                goto error;
            }

            ip = ccpriv_emit(comp, OPCODE_DUP, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }

            int after_left_jump_ip = 0;
            if(logi->op == OPERATOR_LOGICAL_AND)
            {
                after_left_jump_ip = ccpriv_emit(comp, OPCODE_JUMP_IF_FALSE, 1, (uint64_t[]){ (uint64_t)0xbeef });
            }
            else
            {
                after_left_jump_ip = ccpriv_emit(comp, OPCODE_JUMP_IF_TRUE, 1, (uint64_t[]){ (uint64_t)0xbeef });
            }

            if(after_left_jump_ip < 0)
            {
                goto error;
            }

            ip = ccpriv_emit(comp, OPCODE_POP, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }

            ok = ccpriv_compileexpression(comp, logi->right);
            if(!ok)
            {
                goto error;
            }

            int after_right_ip = ccpriv_getip(comp);
            ccpriv_moduint16operand(comp, after_left_jump_ip + 1, after_right_ip);

            break;
        }
        case EXPRESSION_TERNARY:
        {
            const ApeTernaryExpr_t* ternary = &expr->ternary;

            ok = ccpriv_compileexpression(comp, ternary->test);
            if(!ok)
            {
                goto error;
            }

            int else_jump_ip = ccpriv_emit(comp, OPCODE_JUMP_IF_FALSE, 1, (uint64_t[]){ (uint64_t)0xbeef });

            ok = ccpriv_compileexpression(comp, ternary->if_true);
            if(!ok)
            {
                goto error;
            }

            int end_jump_ip = ccpriv_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ (uint64_t)0xbeef });

            int else_ip = ccpriv_getip(comp);
            ccpriv_moduint16operand(comp, else_jump_ip + 1, else_ip);

            ok = ccpriv_compileexpression(comp, ternary->if_false);
            if(!ok)
            {
                goto error;
            }

            int end_ip = ccpriv_getip(comp);
            ccpriv_moduint16operand(comp, end_jump_ip + 1, end_ip);

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
    array_pop(comp->src_positions_stack, NULL);
    expression_destroy(expr_optimised);
    return res;
}

static bool ccpriv_compilecodeblock(ApeCompiler_t* comp, const ApeCodeblock_t* block)
{
    ApeSize_t i;
    ApeSymbolTable_t* symbol_table = compiler_get_symbol_table(comp);
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
        int ip = ccpriv_emit(comp, OPCODE_NULL, 0, NULL);
        if(ip < 0)
        {
            return false;
        }
        ip = ccpriv_emit(comp, OPCODE_POP, 0, NULL);
        if(ip < 0)
        {
            return false;
        }
    }

    for(i = 0; i < ptrarray_count(block->statements); i++)
    {
        const ApeStatement_t* stmt = (ApeStatement_t*)ptrarray_get(block->statements, i);
        bool ok = ccpriv_compilestatement(comp, stmt);
        if(!ok)
        {
            return false;
        }
    }
    symbol_table_pop_block_scope(symbol_table);
    return true;
}

static int ccpriv_addconstant(ApeCompiler_t* comp, ApeObject_t obj)
{
    bool ok = array_add(comp->constants, &obj);
    if(!ok)
    {
        return -1;
    }
    int pos = array_count(comp->constants) - 1;
    return pos;
}

static void ccpriv_moduint16operand(ApeCompiler_t* comp, int ip, uint16_t operand)
{
    ApeArray_t* bytecode = ccpriv_getbytecode(comp);
    if((ip + 1) >= array_count(bytecode))
    {
        APE_ASSERT(false);
        return;
    }
    ApeUShort_t hi = (ApeUShort_t)(operand >> 8);
    array_set(bytecode, ip, &hi);
    ApeUShort_t lo = (ApeUShort_t)(operand);
    array_set(bytecode, ip + 1, &lo);
}

static bool ccpriv_lastopcodeis(ApeCompiler_t* comp, ApeOpByte_t op)
{
    ApeOpByte_t last_opcode = ccpriv_getlastopcode(comp);
    return last_opcode == op;
}

static bool ccpriv_readsym(ApeCompiler_t* comp, const ApeSymbol_t* symbol)
{
    int ip = -1;
    if(symbol->type == SYMBOL_MODULE_GLOBAL)
    {
        ip = ccpriv_emit(comp, OPCODE_GET_MODULE_GLOBAL, 1, (uint64_t[]){ (uint64_t)(symbol->index) });
    }
    else if(symbol->type == SYMBOL_APE_GLOBAL)
    {
        ip = ccpriv_emit(comp, OPCODE_GET_APE_GLOBAL, 1, (uint64_t[]){ (uint64_t)(symbol->index) });
    }
    else if(symbol->type == SYMBOL_LOCAL)
    {
        ip = ccpriv_emit(comp, OPCODE_GET_LOCAL, 1, (uint64_t[]){ (uint64_t)(symbol->index) });
    }
    else if(symbol->type == SYMBOL_FREE)
    {
        ip = ccpriv_emit(comp, OPCODE_GET_FREE, 1, (uint64_t[]){ (uint64_t)(symbol->index) });
    }
    else if(symbol->type == SYMBOL_FUNCTION)
    {
        ip = ccpriv_emit(comp, OPCODE_CURRENT_FUNCTION, 0, NULL);
    }
    else if(symbol->type == SYMBOL_THIS)
    {
        ip = ccpriv_emit(comp, OPCODE_GET_THIS, 0, NULL);
    }
    return ip >= 0;
}

static bool ccpriv_writesym(ApeCompiler_t* comp, const ApeSymbol_t* symbol, bool define)
{
    int ip = -1;
    if(symbol->type == SYMBOL_MODULE_GLOBAL)
    {
        if(define)
        {
            ip = ccpriv_emit(comp, OPCODE_DEFINE_MODULE_GLOBAL, 1, (uint64_t[]){ (uint64_t)(symbol->index) });
        }
        else
        {
            ip = ccpriv_emit(comp, OPCODE_SET_MODULE_GLOBAL, 1, (uint64_t[]){ (uint64_t)(symbol->index) });
        }
    }
    else if(symbol->type == SYMBOL_LOCAL)
    {
        if(define)
        {
            ip = ccpriv_emit(comp, OPCODE_DEFINE_LOCAL, 1, (uint64_t[]){ (uint64_t)(symbol->index) });
        }
        else
        {
            ip = ccpriv_emit(comp, OPCODE_SET_LOCAL, 1, (uint64_t[]){ (uint64_t)(symbol->index) });
        }
    }
    else if(symbol->type == SYMBOL_FREE)
    {
        ip = ccpriv_emit(comp, OPCODE_SET_FREE, 1, (uint64_t[]){ (uint64_t)(symbol->index) });
    }
    return ip >= 0;
}

static bool ccpriv_pushbreakip(ApeCompiler_t* comp, int ip)
{
    ApeCompilationScope_t* comp_scope = ccpriv_getcompscope(comp);
    return array_push(comp_scope->break_ip_stack, &ip);
}

static void ccpriv_popbreakip(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* comp_scope = ccpriv_getcompscope(comp);
    if(array_count(comp_scope->break_ip_stack) == 0)
    {
        APE_ASSERT(false);
        return;
    }
    array_pop(comp_scope->break_ip_stack, NULL);
}

static int ccpriv_getbreakip(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* comp_scope = ccpriv_getcompscope(comp);
    if(array_count(comp_scope->break_ip_stack) == 0)
    {
        return -1;
    }
    int* res = (int*)array_top(comp_scope->break_ip_stack);
    return *res;
}

static bool ccpriv_pushcontip(ApeCompiler_t* comp, int ip)
{
    ApeCompilationScope_t* comp_scope = ccpriv_getcompscope(comp);
    return array_push(comp_scope->continue_ip_stack, &ip);
}

static void ccpriv_popcontip(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* comp_scope = ccpriv_getcompscope(comp);
    if(array_count(comp_scope->continue_ip_stack) == 0)
    {
        APE_ASSERT(false);
        return;
    }
    array_pop(comp_scope->continue_ip_stack, NULL);
}

static int ccpriv_getcontip(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* comp_scope = ccpriv_getcompscope(comp);
    if(array_count(comp_scope->continue_ip_stack) == 0)
    {
        APE_ASSERT(false);
        return -1;
    }
    int* res = (int*)array_top(comp_scope->continue_ip_stack);
    return *res;
}

static int ccpriv_getip(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* compilation_scope = ccpriv_getcompscope(comp);
    return array_count(compilation_scope->bytecode);
}

static ApeArray_t * ccpriv_getsrcpositions(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* compilation_scope = ccpriv_getcompscope(comp);
    return compilation_scope->src_positions;
}

static ApeArray_t * ccpriv_getbytecode(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* compilation_scope = ccpriv_getcompscope(comp);
    return compilation_scope->bytecode;
}

static ApeFileScope_t* ccpriv_makefilescope(ApeCompiler_t* comp, ApeCompiledFile_t* file)
{
    ApeFileScope_t* file_scope = (ApeFileScope_t*)allocator_malloc(comp->alloc, sizeof(ApeFileScope_t));
    if(!file_scope)
    {
        return NULL;
    }
    memset(file_scope, 0, sizeof(ApeFileScope_t));
    file_scope->alloc = comp->alloc;
    file_scope->parser = parser_make(comp->alloc, comp->config, comp->errors);
    if(!file_scope->parser)
    {
        goto err;
    }
    file_scope->symbol_table = NULL;
    file_scope->file = file;
    file_scope->loaded_module_names = ptrarray_make(comp->alloc);
    if(!file_scope->loaded_module_names)
    {
        goto err;
    }
    return file_scope;
err:
    ccpriv_destroyfilescope(file_scope);
    return NULL;
}

static void ccpriv_destroyfilescope(ApeFileScope_t* scope)
{
    ApeSize_t i;
    for(i = 0; i < ptrarray_count(scope->loaded_module_names); i++)
    {
        void* name = ptrarray_get(scope->loaded_module_names, i);
        allocator_free(scope->alloc, name);
    }
    ptrarray_destroy(scope->loaded_module_names);
    parser_destroy(scope->parser);
    allocator_free(scope->alloc, scope);
}

static bool ccpriv_pushfilescope(ApeCompiler_t* comp, const char* filepath)
{
    ApeSymbolTable_t* prev_st = NULL;
    if(ptrarray_count(comp->file_scopes) > 0)
    {
        prev_st = compiler_get_symbol_table(comp);
    }

    ApeCompiledFile_t* file = compiled_file_make(comp->alloc, filepath);
    if(!file)
    {
        return false;
    }

    bool ok = ptrarray_add(comp->files, file);
    if(!ok)
    {
        compiled_file_destroy(file);
        return false;
    }

    ApeFileScope_t* file_scope = ccpriv_makefilescope(comp, file);
    if(!file_scope)
    {
        return false;
    }

    ok = ptrarray_push(comp->file_scopes, file_scope);
    if(!ok)
    {
        ccpriv_destroyfilescope(file_scope);
        return false;
    }

    int global_offset = 0;
    if(prev_st)
    {
        ApeBlockScope_t* prev_st_top_scope = symbol_table_get_block_scope(prev_st);
        global_offset = prev_st_top_scope->offset + prev_st_top_scope->num_definitions;
    }

    ok = ccpriv_pushsymtable(comp, global_offset);
    if(!ok)
    {
        ptrarray_pop(comp->file_scopes);
        ccpriv_destroyfilescope(file_scope);
        return false;
    }

    return true;
}

static void ccpriv_popfilescope(ApeCompiler_t* comp)
{
    ApeSymbolTable_t* popped_st = compiler_get_symbol_table(comp);
    ApeBlockScope_t* popped_st_top_scope = symbol_table_get_block_scope(popped_st);
    int popped_num_defs = popped_st_top_scope->num_definitions;

    while(compiler_get_symbol_table(comp))
    {
        ccpriv_popsymtable(comp);
    }
    ApeFileScope_t* scope = (ApeFileScope_t*)ptrarray_top(comp->file_scopes);
    if(!scope)
    {
        APE_ASSERT(false);
        return;
    }
    ccpriv_destroyfilescope(scope);

    ptrarray_pop(comp->file_scopes);

    if(ptrarray_count(comp->file_scopes) > 0)
    {
        ApeSymbolTable_t* current_st = compiler_get_symbol_table(comp);
        ApeBlockScope_t* current_st_top_scope = symbol_table_get_block_scope(current_st);
        current_st_top_scope->num_definitions += popped_num_defs;
    }
}

static void ccpriv_setcompscope(ApeCompiler_t* comp, ApeCompilationScope_t* scope)
{
    comp->compilation_scope = scope;
}
