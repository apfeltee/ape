
#include "priv.h"

const ApeSymbol_t* define_symbol(ApeCompiler_t* comp, ApePosition_t pos, const char* name, bool assignable, bool can_shadow)
{
    ApeSymbol_table_t* symbol_table = compiler_get_symbol_table(comp);
    if(!can_shadow && !symbol_table_is_top_global_scope(symbol_table))
    {
        const ApeSymbol_t* current_symbol = symbol_table_resolve(symbol_table, name);
        if(current_symbol)
        {
            errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, pos, "Symbol \"%s\" is already defined", name);
            return NULL;
        }
    }

    const ApeSymbol_t* symbol = symbol_table_define(symbol_table, name, assignable);
    if(!symbol)
    {
        errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, pos, "Cannot define symbol \"%s\"", name);
        return false;
    }

    return symbol;
}


ApeCompiler_t*
compiler_make(ApeAllocator_t* alloc, const ApeConfig_t* config, ApeGCMemory_t* mem, ApeErrorList_t* errors, ptrarray(ApeCompiledFile_t) * files, ApeGlobalStore_t* global_store)
{
    ApeCompiler_t* comp = allocator_malloc(alloc, sizeof(ApeCompiler_t));
    if(!comp)
    {
        return NULL;
    }
    bool ok = compiler_init(comp, alloc, config, mem, errors, files, global_store);
    if(!ok)
    {
        allocator_free(alloc, comp);
        return NULL;
    }
    return comp;
}


void compiler_destroy(ApeCompiler_t* comp)
{
    if(!comp)
    {
        return;
    }
    ApeAllocator_t* alloc = comp->alloc;
    compiler_deinit(comp);
    allocator_free(alloc, comp);
}

ApeCompilationResult_t* compiler_compile(ApeCompiler_t* comp, const char* code)
{
    ApeCompilationScope_t* compilation_scope = get_compilation_scope(comp);

    APE_ASSERT(array_count(comp->src_positions_stack) == 0);
    APE_ASSERT(array_count(compilation_scope->bytecode) == 0);
    APE_ASSERT(array_count(compilation_scope->break_ip_stack) == 0);
    APE_ASSERT(array_count(compilation_scope->continue_ip_stack) == 0);

    array_clear(comp->src_positions_stack);
    array_clear(compilation_scope->bytecode);
    array_clear(compilation_scope->src_positions);
    array_clear(compilation_scope->break_ip_stack);
    array_clear(compilation_scope->continue_ip_stack);

    ApeCompiler_t comp_shallow_copy;
    bool ok = compiler_init_shallow_copy(&comp_shallow_copy, comp);
    if(!ok)
    {
        return NULL;
    }

    ok = compile_code(comp, code);
    if(!ok)
    {
        goto err;
    }

    compilation_scope = get_compilation_scope(comp);// might've changed
    APE_ASSERT(compilation_scope->outer == NULL);

    compilation_scope = get_compilation_scope(comp);
    ApeCompilationResult_t* res = compilation_scope_orphan_result(compilation_scope);
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
    char* code = NULL;
    ApeCompiledFile_t* file = NULL;
    ApeCompilationResult_t* res = NULL;

    if(!comp->config->fileio.read_file.read_file)
    {// todo: read code function
        errors_add_error(comp->errors, APE_ERROR_COMPILATION, src_pos_invalid, "File read function not configured");
        goto err;
    }

    code = comp->config->fileio.read_file.read_file(comp->config->fileio.read_file.context, path);
    if(!code)
    {
        errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, src_pos_invalid, "Reading file \"%s\" failed", path);
        goto err;
    }

    file = compiled_file_make(comp->alloc, path);
    if(!file)
    {
        goto err;
    }

    bool ok = ptrarray_add(comp->files, file);
    if(!ok)
    {
        compiled_file_destroy(file);
        goto err;
    }

    APE_ASSERT(ptrarray_count(comp->file_scopes) == 1);
    ApeFileScope_t* file_scope = ptrarray_top(comp->file_scopes);
    if(!file_scope)
    {
        goto err;
    }

    ApeCompiledFile_t* prev_file = file_scope->file;// todo: push file scope instead?
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

ApeSymbol_table_t* compiler_get_symbol_table(ApeCompiler_t* comp)
{
    ApeFileScope_t* file_scope = ptrarray_top(comp->file_scopes);
    if(!file_scope)
    {
        APE_ASSERT(false);
        return NULL;
    }
    return file_scope->symbol_table;
}

void compiler_set_symbol_table(ApeCompiler_t* comp, ApeSymbol_table_t* table)
{
    ApeFileScope_t* file_scope = ptrarray_top(comp->file_scopes);
    if(!file_scope)
    {
        APE_ASSERT(false);
        return;
    }
    file_scope->symbol_table = table;
}

array(ApeObject_t) * compiler_get_constants(const ApeCompiler_t* comp)
{
    return comp->constants;
}

// INTERNAL
bool compiler_init(ApeCompiler_t* comp,
                          ApeAllocator_t* alloc,
                          const ApeConfig_t* config,
                          ApeGCMemory_t* mem,
                          ApeErrorList_t* errors,
                          ptrarray(ApeCompiledFile_t) * files,
                          ApeGlobalStore_t* global_store)
{
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
    comp->modules = dict_make(alloc, module_copy, module_destroy);
    if(!comp->modules)
    {
        goto err;
    }
    bool ok = push_compilation_scope(comp);
    if(!ok)
    {
        goto err;
    }
    ok = push_file_scope(comp, "none");
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
    if(!comp)
    {
        return;
    }
    for(int i = 0; i < dict_count(comp->string_constants_positions); i++)
    {
        int* val = dict_get_value_at(comp->string_constants_positions, i);
        allocator_free(comp->alloc, val);
    }
    dict_destroy(comp->string_constants_positions);

    while(ptrarray_count(comp->file_scopes) > 0)
    {
        pop_file_scope(comp);
    }
    while(get_compilation_scope(comp))
    {
        pop_compilation_scope(comp);
    }
    dict_destroy_with_items(comp->modules);
    array_destroy(comp->src_positions_stack);

    array_destroy(comp->constants);
    ptrarray_destroy(comp->file_scopes);
    memset(comp, 0, sizeof(ApeCompiler_t));
}

bool compiler_init_shallow_copy(ApeCompiler_t* copy, ApeCompiler_t* src)
{
    bool ok = compiler_init(copy, src->alloc, src->config, src->mem, src->errors, src->files, src->global_store);
    if(!ok)
    {
        return false;
    }

    ApeSymbol_table_t* src_st = compiler_get_symbol_table(src);
    APE_ASSERT(ptrarray_count(src->file_scopes) == 1);
    APE_ASSERT(src_st->outer == NULL);
    ApeSymbol_table_t* src_st_copy = symbol_table_copy(src_st);
    if(!src_st_copy)
    {
        goto err;
    }
    ApeSymbol_table_t* copy_st = compiler_get_symbol_table(copy);
    symbol_table_destroy(copy_st);
    copy_st = NULL;
    compiler_set_symbol_table(copy, src_st_copy);

    dict(ApeModule_t)* modules_copy = dict_copy_with_items(src->modules);
    if(!modules_copy)
    {
        goto err;
    }
    dict_destroy_with_items(copy->modules);
    copy->modules = modules_copy;

    array(ApeObject_t)* constants_copy = array_copy(src->constants);
    if(!constants_copy)
    {
        goto err;
    }
    array_destroy(copy->constants);
    copy->constants = constants_copy;

    for(int i = 0; i < dict_count(src->string_constants_positions); i++)
    {
        const char* key = dict_get_key_at(src->string_constants_positions, i);
        int* val = dict_get_value_at(src->string_constants_positions, i);
        int* val_copy = allocator_malloc(src->alloc, sizeof(int));
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

    ApeFileScope_t* src_file_scope = ptrarray_top(src->file_scopes);
    ApeFileScope_t* copy_file_scope = ptrarray_top(copy->file_scopes);

    ptrarray(char)* src_loaded_module_names = src_file_scope->loaded_module_names;
    ptrarray(char)* copy_loaded_module_names = copy_file_scope->loaded_module_names;

    for(int i = 0; i < ptrarray_count(src_loaded_module_names); i++)
    {
        const char* loaded_name = ptrarray_get(src_loaded_module_names, i);
        char* loaded_name_copy = ape_strdup(copy->alloc, loaded_name);
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

int emit(ApeCompiler_t* comp, opcode_t op, int operands_count, uint64_t* operands)
{
    int ip = get_ip(comp);
    int len = code_make(op, operands_count, operands, get_bytecode(comp));
    if(len == 0)
    {
        return -1;
    }
    for(int i = 0; i < len; i++)
    {
        ApePosition_t* src_pos = array_top(comp->src_positions_stack);
        APE_ASSERT(src_pos->line >= 0);
        APE_ASSERT(src_pos->column >= 0);
        bool ok = array_add(get_src_positions(comp), src_pos);
        if(!ok)
        {
            return -1;
        }
    }
    ApeCompilationScope_t* compilation_scope = get_compilation_scope(comp);
    compilation_scope->last_opcode = op;
    return ip;
}

ApeCompilationScope_t* get_compilation_scope(ApeCompiler_t* comp)
{
    return comp->compilation_scope;
}

bool push_compilation_scope(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* current_scope = get_compilation_scope(comp);
    ApeCompilationScope_t* new_scope = compilation_scope_make(comp->alloc, current_scope);
    if(!new_scope)
    {
        return false;
    }
    set_compilation_scope(comp, new_scope);
    return true;
}

void pop_compilation_scope(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* current_scope = get_compilation_scope(comp);
    APE_ASSERT(current_scope);
    set_compilation_scope(comp, current_scope->outer);
    compilation_scope_destroy(current_scope);
}

bool push_symbol_table(ApeCompiler_t* comp, int global_offset)
{
    ApeFileScope_t* file_scope = ptrarray_top(comp->file_scopes);
    if(!file_scope)
    {
        APE_ASSERT(false);
        return false;
    }
    ApeSymbol_table_t* current_table = file_scope->symbol_table;
    file_scope->symbol_table = symbol_table_make(comp->alloc, current_table, comp->global_store, global_offset);
    if(!file_scope->symbol_table)
    {
        file_scope->symbol_table = current_table;
        return false;
    }
    return true;
}

void pop_symbol_table(ApeCompiler_t* comp)
{
    ApeFileScope_t* file_scope = ptrarray_top(comp->file_scopes);
    if(!file_scope)
    {
        APE_ASSERT(false);
        return;
    }
    ApeSymbol_table_t* current_table = file_scope->symbol_table;
    if(!current_table)
    {
        APE_ASSERT(false);
        return;
    }
    file_scope->symbol_table = current_table->outer;
    symbol_table_destroy(current_table);
}

opcode_t get_last_opcode(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* current_scope = get_compilation_scope(comp);
    return current_scope->last_opcode;
}

bool compile_code(ApeCompiler_t* comp, const char* code)
{
    ApeFileScope_t* file_scope = ptrarray_top(comp->file_scopes);
    APE_ASSERT(file_scope);

    ptrarray(ApeStatement_t)* statements = parser_parse_all(file_scope->parser, code, file_scope->file);
    if(!statements)
    {
        // errors are added by parser
        return false;
    }

    bool ok = compile_statements(comp, statements);

    ptrarray_destroy_with_items(statements, statement_destroy);

    // Left for debugging purposes
    //    if (ok) {
    //        ApeStringBuffer_t *buf = strbuf_make(NULL);
    //        code_to_string(array_data(comp->compilation_scope->bytecode),
    //                       array_data(comp->compilation_scope->src_positions),
    //                       array_count(comp->compilation_scope->bytecode), buf);
    //        puts(strbuf_get_string(buf));
    //        strbuf_destroy(buf);
    //    }

    return ok;
}

bool compile_statements(ApeCompiler_t* comp, ptrarray(ApeStatement_t) * statements)
{
    bool ok = true;
    for(int i = 0; i < ptrarray_count(statements); i++)
    {
        const ApeStatement_t* stmt = ptrarray_get(statements, i);
        ok = compile_statement(comp, stmt);
        if(!ok)
        {
            break;
        }
    }
    return ok;
}

bool import_module(ApeCompiler_t* comp, const ApeStatement_t* import_stmt)
{// todo: split into smaller functions
    bool result = false;
    char* filepath = NULL;
    char* code = NULL;

    ApeFileScope_t* file_scope = ptrarray_top(comp->file_scopes);

    const char* module_path = import_stmt->import.path;
    const char* module_name = get_module_name(module_path);

    for(int i = 0; i < ptrarray_count(file_scope->loaded_module_names); i++)
    {
        const char* loaded_name = ptrarray_get(file_scope->loaded_module_names, i);
        if(kg_streq(loaded_name, module_name))
        {
            errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, import_stmt->pos, "Module \"%s\" was already imported", module_name);
            result = false;
            goto end;
        }
    }

    ApeStringBuffer_t* filepath_buf = strbuf_make(comp->alloc);
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

    const char* filepath_non_canonicalised = strbuf_get_string(filepath_buf);
    filepath = kg_canonicalise_path(comp->alloc, filepath_non_canonicalised);
    strbuf_destroy(filepath_buf);
    if(!filepath)
    {
        result = false;
        goto end;
    }

    ApeSymbol_table_t* symbol_table = compiler_get_symbol_table(comp);
    if(symbol_table->outer != NULL || ptrarray_count(symbol_table->block_scopes) > 1)
    {
        errors_add_error(comp->errors, APE_ERROR_COMPILATION, import_stmt->pos, "Modules can only be imported in global scope");
        result = false;
        goto end;
    }

    for(int i = 0; i < ptrarray_count(comp->file_scopes); i++)
    {
        ApeFileScope_t* fs = ptrarray_get(comp->file_scopes, i);
        if(APE_STREQ(fs->file->path, filepath))
        {
            errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, import_stmt->pos, "Cyclic reference of file \"%s\"", filepath);
            result = false;
            goto end;
        }
    }

    ApeModule_t* module = dict_get(comp->modules, filepath);
    if(!module)
    {// todo: create new module function
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

        bool ok = push_file_scope(comp, filepath);
        if(!ok)
        {
            module_destroy(module);
            result = false;
            goto end;
        }

        ok = compile_code(comp, code);
        if(!ok)
        {
            module_destroy(module);
            result = false;
            goto end;
        }

        ApeSymbol_table_t* st = compiler_get_symbol_table(comp);
        for(int i = 0; i < symbol_table_get_module_global_symbol_count(st); i++)
        {
            const ApeSymbol_t* symbol = symbol_table_get_module_global_symbol_at(st, i);
            module_add_symbol(module, symbol);
        }

        pop_file_scope(comp);

        ok = dict_set(comp->modules, filepath, module);
        if(!ok)
        {
            module_destroy(module);
            result = false;
            goto end;
        }
    }

    for(int i = 0; i < ptrarray_count(module->symbols); i++)
    {
        ApeSymbol_t* symbol = ptrarray_get(module->symbols, i);
        bool ok = symbol_table_add_module_symbol(symbol_table, symbol);
        if(!ok)
        {
            result = false;
            goto end;
        }
    }

    char* name_copy = ape_strdup(comp->alloc, module_name);
    if(!name_copy)
    {
        result = false;
        goto end;
    }

    bool ok = ptrarray_add(file_scope->loaded_module_names, name_copy);
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

bool compile_statement(ApeCompiler_t* comp, const ApeStatement_t* stmt)
{
    bool ok = false;
    int ip = -1;

    ok = array_push(comp->src_positions_stack, &stmt->pos);
    if(!ok)
    {
        return false;
    }

    ApeCompilationScope_t* compilation_scope = get_compilation_scope(comp);
    ApeSymbol_table_t* symbol_table = compiler_get_symbol_table(comp);
    switch(stmt->type)
    {
        case STATEMENT_EXPRESSION:
        {
            ok = compile_expression(comp, stmt->expression);
            if(!ok)
            {
                return false;
            }
            ip = emit(comp, OPCODE_POP, 0, NULL);
            if(ip < 0)
            {
                return false;
            }
            break;
        }
        case STATEMENT_DEFINE:
        {
            ok = compile_expression(comp, stmt->define.value);
            if(!ok)
            {
                return false;
            }

            const ApeSymbol_t* symbol
            = define_symbol(comp, stmt->define.name->pos, stmt->define.name->value, stmt->define.assignable, false);
            if(!symbol)
            {
                return false;
            }

            ok = write_symbol(comp, symbol, true);
            if(!ok)
            {
                return false;
            }

            break;
        }
        case STATEMENT_IF:
        {
            const ApeIfStmt_t* if_stmt = &stmt->if_statement;

            array(int)* jump_to_end_ips = array_make(comp->alloc, int);
            if(!jump_to_end_ips)
            {
                goto statement_if_error;
            }

            for(int i = 0; i < ptrarray_count(if_stmt->cases); i++)
            {
                ApeIfCase_t* if_case = ptrarray_get(if_stmt->cases, i);

                ok = compile_expression(comp, if_case->test);
                if(!ok)
                {
                    goto statement_if_error;
                }

                int next_case_jump_ip = emit(comp, OPCODE_JUMP_IF_FALSE, 1, (uint64_t[]){ 0xbeef });

                ok = compile_code_block(comp, if_case->consequence);
                if(!ok)
                {
                    goto statement_if_error;
                }

                // don't emit jump for the last statement
                if(i < (ptrarray_count(if_stmt->cases) - 1) || if_stmt->alternative)
                {
                    int jump_to_end_ip = emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ 0xbeef });
                    bool ok = array_add(jump_to_end_ips, &jump_to_end_ip);
                    if(!ok)
                    {
                        goto statement_if_error;
                    }
                }

                int after_elif_ip = get_ip(comp);
                change_uint16_operand(comp, next_case_jump_ip + 1, after_elif_ip);
            }

            if(if_stmt->alternative)
            {
                ok = compile_code_block(comp, if_stmt->alternative);
                if(!ok)
                {
                    goto statement_if_error;
                }
            }

            int after_alt_ip = get_ip(comp);

            for(int i = 0; i < array_count(jump_to_end_ips); i++)
            {
                int* pos = array_get(jump_to_end_ips, i);
                change_uint16_operand(comp, *pos + 1, after_alt_ip);
            }

            array_destroy(jump_to_end_ips);

            break;
        statement_if_error:
            array_destroy(jump_to_end_ips);
            return false;
        }
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
                ok = compile_expression(comp, stmt->return_value);
                if(!ok)
                {
                    return false;
                }
                ip = emit(comp, OPCODE_RETURN_VALUE, 0, NULL);
            }
            else
            {
                ip = emit(comp, OPCODE_RETURN, 0, NULL);
            }
            if(ip < 0)
            {
                return false;
            }
            break;
        }
        case STATEMENT_WHILE_LOOP:
        {
            const ApeWhileLoopStmt_t* loop = &stmt->while_loop;

            int before_test_ip = get_ip(comp);

            ok = compile_expression(comp, loop->test);
            if(!ok)
            {
                return false;
            }

            int after_test_ip = get_ip(comp);
            ip = emit(comp, OPCODE_JUMP_IF_TRUE, 1, (uint64_t[]){ after_test_ip + 6 });
            if(ip < 0)
            {
                return false;
            }

            int jump_to_after_body_ip = emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ 0xdead });
            if(jump_to_after_body_ip < 0)
            {
                return false;
            }

            ok = push_continue_ip(comp, before_test_ip);
            if(!ok)
            {
                return false;
            }

            ok = push_break_ip(comp, jump_to_after_body_ip);
            if(!ok)
            {
                return false;
            }

            ok = compile_code_block(comp, loop->body);
            if(!ok)
            {
                return false;
            }

            pop_break_ip(comp);
            pop_continue_ip(comp);

            ip = emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ before_test_ip });
            if(ip < 0)
            {
                return false;
            }

            int after_body_ip = get_ip(comp);
            change_uint16_operand(comp, jump_to_after_body_ip + 1, after_body_ip);

            break;
        }
        case STATEMENT_BREAK:
        {
            int break_ip = get_break_ip(comp);
            if(break_ip < 0)
            {
                errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "Nothing to break from.");
                return false;
            }
            ip = emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ break_ip });
            if(ip < 0)
            {
                return false;
            }
            break;
        }
        case STATEMENT_CONTINUE:
        {
            int continue_ip = get_continue_ip(comp);
            if(continue_ip < 0)
            {
                errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "Nothing to continue from.");
                return false;
            }
            ip = emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ continue_ip });
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
            const ApeSymbol_t* index_symbol = define_symbol(comp, stmt->pos, "@i", false, true);
            if(!index_symbol)
            {
                return false;
            }

            ip = emit(comp, OPCODE_NUMBER, 1, (uint64_t[]){ 0 });
            if(ip < 0)
            {
                return false;
            }

            ok = write_symbol(comp, index_symbol, true);
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
                ok = compile_expression(comp, foreach->source);
                if(!ok)
                {
                    return false;
                }
                source_symbol = define_symbol(comp, foreach->source->pos, "@source", false, true);
                if(!source_symbol)
                {
                    return false;
                }
                ok = write_symbol(comp, source_symbol, true);
                if(!ok)
                {
                    return false;
                }
            }

            // Update
            int jump_to_after_update_ip = emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ 0xbeef });
            if(jump_to_after_update_ip < 0)
            {
                return false;
            }

            int update_ip = get_ip(comp);
            ok = read_symbol(comp, index_symbol);
            if(!ok)
            {
                return false;
            }

            ip = emit(comp, OPCODE_NUMBER, 1, (uint64_t[]){ ape_double_to_uint64(1) });
            if(ip < 0)
            {
                return false;
            }

            ip = emit(comp, OPCODE_ADD, 0, NULL);
            if(ip < 0)
            {
                return false;
            }

            ok = write_symbol(comp, index_symbol, false);
            if(!ok)
            {
                return false;
            }

            int after_update_ip = get_ip(comp);
            change_uint16_operand(comp, jump_to_after_update_ip + 1, after_update_ip);

            // Test
            ok = array_push(comp->src_positions_stack, &foreach->source->pos);
            if(!ok)
            {
                return false;
            }

            ok = read_symbol(comp, source_symbol);
            if(!ok)
            {
                return false;
            }

            ip = emit(comp, OPCODE_LEN, 0, NULL);
            if(ip < 0)
            {
                return false;
            }

            array_pop(comp->src_positions_stack, NULL);
            ok = read_symbol(comp, index_symbol);
            if(!ok)
            {
                return false;
            }

            ip = emit(comp, OPCODE_COMPARE, 0, NULL);
            if(ip < 0)
            {
                return false;
            }

            ip = emit(comp, OPCODE_EQUAL, 0, NULL);
            if(ip < 0)
            {
                return false;
            }

            int after_test_ip = get_ip(comp);
            ip = emit(comp, OPCODE_JUMP_IF_FALSE, 1, (uint64_t[]){ after_test_ip + 6 });
            if(ip < 0)
            {
                return false;
            }

            int jump_to_after_body_ip = emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ 0xdead });
            if(jump_to_after_body_ip < 0)
            {
                return false;
            }

            ok = read_symbol(comp, source_symbol);
            if(!ok)
            {
                return false;
            }

            ok = read_symbol(comp, index_symbol);
            if(!ok)
            {
                return false;
            }

            ip = emit(comp, OPCODE_GET_VALUE_AT, 0, NULL);
            if(ip < 0)
            {
                return false;
            }

            const ApeSymbol_t* iter_symbol = define_symbol(comp, foreach->iterator->pos, foreach->iterator->value, false, false);
            if(!iter_symbol)
            {
                return false;
            }

            ok = write_symbol(comp, iter_symbol, true);
            if(!ok)
            {
                return false;
            }

            // Body
            ok = push_continue_ip(comp, update_ip);
            if(!ok)
            {
                return false;
            }

            ok = push_break_ip(comp, jump_to_after_body_ip);
            if(!ok)
            {
                return false;
            }

            ok = compile_code_block(comp, foreach->body);
            if(!ok)
            {
                return false;
            }

            pop_break_ip(comp);
            pop_continue_ip(comp);

            ip = emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ update_ip });
            if(ip < 0)
            {
                return false;
            }

            int after_body_ip = get_ip(comp);
            change_uint16_operand(comp, jump_to_after_body_ip + 1, after_body_ip);

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
                ok = compile_statement(comp, loop->init);
                if(!ok)
                {
                    return false;
                }
                jump_to_after_update_ip = emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ 0xbeef });
                if(jump_to_after_update_ip < 0)
                {
                    return false;
                }
            }

            // Update
            int update_ip = get_ip(comp);
            if(loop->update)
            {
                ok = compile_expression(comp, loop->update);
                if(!ok)
                {
                    return false;
                }
                ip = emit(comp, OPCODE_POP, 0, NULL);
                if(ip < 0)
                {
                    return false;
                }
            }

            if(loop->init)
            {
                int after_update_ip = get_ip(comp);
                change_uint16_operand(comp, jump_to_after_update_ip + 1, after_update_ip);
            }

            // Test
            if(loop->test)
            {
                ok = compile_expression(comp, loop->test);
                if(!ok)
                {
                    return false;
                }
            }
            else
            {
                ip = emit(comp, OPCODE_TRUE, 0, NULL);
                if(ip < 0)
                {
                    return false;
                }
            }
            int after_test_ip = get_ip(comp);

            ip = emit(comp, OPCODE_JUMP_IF_TRUE, 1, (uint64_t[]){ after_test_ip + 6 });
            if(ip < 0)
            {
                return false;
            }
            int jmp_to_after_body_ip = emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ 0xdead });
            if(jmp_to_after_body_ip < 0)
            {
                return false;
            }

            // Body
            ok = push_continue_ip(comp, update_ip);
            if(!ok)
            {
                return false;
            }

            ok = push_break_ip(comp, jmp_to_after_body_ip);
            if(!ok)
            {
                return false;
            }

            ok = compile_code_block(comp, loop->body);
            if(!ok)
            {
                return false;
            }

            pop_break_ip(comp);
            pop_continue_ip(comp);

            ip = emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ update_ip });
            if(ip < 0)
            {
                return false;
            }

            int after_body_ip = get_ip(comp);
            change_uint16_operand(comp, jmp_to_after_body_ip + 1, after_body_ip);

            symbol_table_pop_block_scope(symbol_table);
            break;
        }
        case STATEMENT_BLOCK:
        {
            ok = compile_code_block(comp, stmt->block);
            if(!ok)
            {
                return false;
            }
            break;
        }
        case STATEMENT_IMPORT:
        {
            ok = import_module(comp, stmt);
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

            int recover_ip = emit(comp, OPCODE_SET_RECOVER, 1, (uint64_t[]){ 0xbeef });
            if(recover_ip < 0)
            {
                return false;
            }

            int jump_to_after_recover_ip = emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ 0xbeef });
            if(jump_to_after_recover_ip < 0)
            {
                return false;
            }

            int after_jump_to_recover_ip = get_ip(comp);
            change_uint16_operand(comp, recover_ip + 1, after_jump_to_recover_ip);

            ok = symbol_table_push_block_scope(symbol_table);
            if(!ok)
            {
                return false;
            }

            const ApeSymbol_t* error_symbol
            = define_symbol(comp, recover->error_ident->pos, recover->error_ident->value, false, false);
            if(!error_symbol)
            {
                return false;
            }

            ok = write_symbol(comp, error_symbol, true);
            if(!ok)
            {
                return false;
            }

            ok = compile_code_block(comp, recover->body);
            if(!ok)
            {
                return false;
            }

            if(!last_opcode_is(comp, OPCODE_RETURN) && !last_opcode_is(comp, OPCODE_RETURN_VALUE))
            {
                errors_add_error(comp->errors, APE_ERROR_COMPILATION, stmt->pos, "Recover body must end with a return statement");
                return false;
            }

            symbol_table_pop_block_scope(symbol_table);

            int after_recover_ip = get_ip(comp);
            change_uint16_operand(comp, jump_to_after_recover_ip + 1, after_recover_ip);

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

bool compile_expression(ApeCompiler_t* comp, ApeExpression_t* expr)
{
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

    ApeCompilationScope_t* compilation_scope = get_compilation_scope(comp);
    ApeSymbol_table_t* symbol_table = compiler_get_symbol_table(comp);

    bool res = false;

    switch(expr->type)
    {
        case EXPRESSION_INFIX:
        {
            bool rearrange = false;

            opcode_t op = OPCODE_NONE;
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

            ok = compile_expression(comp, left);
            if(!ok)
            {
                goto error;
            }

            ok = compile_expression(comp, right);
            if(!ok)
            {
                goto error;
            }

            switch(expr->infix.op)
            {
                case OPERATOR_EQ:
                case OPERATOR_NOT_EQ:
                {
                    ip = emit(comp, OPCODE_COMPARE_EQ, 0, NULL);
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
                    ip = emit(comp, OPCODE_COMPARE, 0, NULL);
                    if(ip < 0)
                    {
                        goto error;
                    }
                    break;
                }
                default:
                    break;
            }

            ip = emit(comp, op, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_NUMBER_LITERAL:
        {
            double number = expr->number_literal;
            ip = emit(comp, OPCODE_NUMBER, 1, (uint64_t[]){ ape_double_to_uint64(number) });
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_STRING_LITERAL:
        {
            int pos = 0;
            int* current_pos = dict_get(comp->string_constants_positions, expr->string_literal);
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

                pos = add_constant(comp, obj);
                if(pos < 0)
                {
                    goto error;
                }

                int* pos_val = allocator_malloc(comp->alloc, sizeof(int));
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

            ip = emit(comp, OPCODE_CONSTANT, 1, (uint64_t[]){ pos });
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_NULL_LITERAL:
        {
            ip = emit(comp, OPCODE_NULL, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }
            break;
        }
        case EXPRESSION_BOOL_LITERAL:
        {
            ip = emit(comp, expr->bool_literal ? OPCODE_TRUE : OPCODE_FALSE, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }
            break;
        }
        case EXPRESSION_ARRAY_LITERAL:
        {
            for(int i = 0; i < ptrarray_count(expr->array); i++)
            {
                ok = compile_expression(comp, ptrarray_get(expr->array, i));
                if(!ok)
                {
                    goto error;
                }
            }
            ip = emit(comp, OPCODE_ARRAY, 1, (uint64_t[]){ ptrarray_count(expr->array) });
            if(ip < 0)
            {
                goto error;
            }
            break;
        }
        case EXPRESSION_MAP_LITERAL:
        {
            const ApeMapLiteral_t* map = &expr->map;
            int len = ptrarray_count(map->keys);
            ip = emit(comp, OPCODE_MAP_START, 1, (uint64_t[]){ len });
            if(ip < 0)
            {
                goto error;
            }

            for(int i = 0; i < len; i++)
            {
                ApeExpression_t* key = ptrarray_get(map->keys, i);
                ApeExpression_t* val = ptrarray_get(map->values, i);

                ok = compile_expression(comp, key);
                if(!ok)
                {
                    goto error;
                }

                ok = compile_expression(comp, val);
                if(!ok)
                {
                    goto error;
                }
            }

            ip = emit(comp, OPCODE_MAP_END, 1, (uint64_t[]){ len });
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_PREFIX:
        {
            ok = compile_expression(comp, expr->prefix.right);
            if(!ok)
            {
                goto error;
            }

            opcode_t op = OPCODE_NONE;
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
            ip = emit(comp, op, 0, NULL);
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
            ok = read_symbol(comp, symbol);
            if(!ok)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_INDEX:
        {
            const ApeIndexExpr_t* index = &expr->index_expr;
            ok = compile_expression(comp, index->left);
            if(!ok)
            {
                goto error;
            }
            ok = compile_expression(comp, index->index);
            if(!ok)
            {
                goto error;
            }
            ip = emit(comp, OPCODE_GET_INDEX, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }

            break;
        }
        case EXPRESSION_FUNCTION_LITERAL:
        {
            const ApeFnLiteral_t* fn = &expr->fn_literal;

            ok = push_compilation_scope(comp);
            if(!ok)
            {
                goto error;
            }

            ok = push_symbol_table(comp, 0);
            if(!ok)
            {
                goto error;
            }

            compilation_scope = get_compilation_scope(comp);
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

            for(int i = 0; i < ptrarray_count(expr->fn_literal.params); i++)
            {
                ApeIdent_t* param = ptrarray_get(expr->fn_literal.params, i);
                const ApeSymbol_t* param_symbol = define_symbol(comp, param->pos, param->value, true, false);
                if(!param_symbol)
                {
                    goto error;
                }
            }

            ok = compile_statements(comp, fn->body->statements);
            if(!ok)
            {
                goto error;
            }

            if(!last_opcode_is(comp, OPCODE_RETURN_VALUE) && !last_opcode_is(comp, OPCODE_RETURN))
            {
                ip = emit(comp, OPCODE_RETURN, 0, NULL);
                if(ip < 0)
                {
                    goto error;
                }
            }

            ptrarray(ApeSymbol_t)* free_symbols = symbol_table->free_symbols;
            symbol_table->free_symbols = NULL;// because it gets destroyed with compiler_pop_compilation_scope()

            int num_locals = symbol_table->max_num_definitions;

            ApeCompilationResult_t* comp_res = compilation_scope_orphan_result(compilation_scope);
            if(!comp_res)
            {
                ptrarray_destroy_with_items(free_symbols, symbol_destroy);
                goto error;
            }
            pop_symbol_table(comp);
            pop_compilation_scope(comp);
            compilation_scope = get_compilation_scope(comp);
            symbol_table = compiler_get_symbol_table(comp);

            ApeObject_t obj = object_make_function(comp->mem, fn->name, comp_res, true, num_locals, ptrarray_count(fn->params), 0);

            if(object_is_null(obj))
            {
                ptrarray_destroy_with_items(free_symbols, symbol_destroy);
                compilation_result_destroy(comp_res);
                goto error;
            }

            for(int i = 0; i < ptrarray_count(free_symbols); i++)
            {
                ApeSymbol_t* symbol = ptrarray_get(free_symbols, i);
                ok = read_symbol(comp, symbol);
                if(!ok)
                {
                    ptrarray_destroy_with_items(free_symbols, symbol_destroy);
                    goto error;
                }
            }

            int pos = add_constant(comp, obj);
            if(pos < 0)
            {
                ptrarray_destroy_with_items(free_symbols, symbol_destroy);
                goto error;
            }

            ip = emit(comp, OPCODE_FUNCTION, 2, (uint64_t[]){ pos, ptrarray_count(free_symbols) });
            if(ip < 0)
            {
                ptrarray_destroy_with_items(free_symbols, symbol_destroy);
                goto error;
            }

            ptrarray_destroy_with_items(free_symbols, symbol_destroy);

            break;
        }
        case EXPRESSION_CALL:
        {
            ok = compile_expression(comp, expr->call_expr.function);
            if(!ok)
            {
                goto error;
            }

            for(int i = 0; i < ptrarray_count(expr->call_expr.args); i++)
            {
                ApeExpression_t* arg_expr = ptrarray_get(expr->call_expr.args, i);
                ok = compile_expression(comp, arg_expr);
                if(!ok)
                {
                    goto error;
                }
            }

            ip = emit(comp, OPCODE_CALL, 1, (uint64_t[]){ ptrarray_count(expr->call_expr.args) });
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
                ok = compile_expression(comp, assign->dest);
                if(!ok)
                {
                    goto error;
                }
            }

            ok = compile_expression(comp, assign->source);
            if(!ok)
            {
                goto error;
            }

            ip = emit(comp, OPCODE_DUP, 0, NULL);
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
                    //symbol_table_define(ApeSymbol_table_t* table, const char* name, bool assignable)
                    symbol = symbol_table_define(symbol_table, ident->value, true);
                }
                if(!symbol->assignable)
                {
                    errors_add_errorf(comp->errors, APE_ERROR_COMPILATION, assign->dest->pos,
                                      "Symbol \"%s\" is not assignable", ident->value);
                    goto error;
                }
                ok = write_symbol(comp, symbol, false);
                if(!ok)
                {
                    goto error;
                }
            }
            else if(assign->dest->type == EXPRESSION_INDEX)
            {
                const ApeIndexExpr_t* index = &assign->dest->index_expr;
                ok = compile_expression(comp, index->left);
                if(!ok)
                {
                    goto error;
                }
                ok = compile_expression(comp, index->index);
                if(!ok)
                {
                    goto error;
                }
                ip = emit(comp, OPCODE_SET_INDEX, 0, NULL);
                if(ip < 0)
                {
                    goto error;
                }
            }

            if(assign->is_postfix)
            {
                ip = emit(comp, OPCODE_POP, 0, NULL);
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

            ok = compile_expression(comp, logi->left);
            if(!ok)
            {
                goto error;
            }

            ip = emit(comp, OPCODE_DUP, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }

            int after_left_jump_ip = 0;
            if(logi->op == OPERATOR_LOGICAL_AND)
            {
                after_left_jump_ip = emit(comp, OPCODE_JUMP_IF_FALSE, 1, (uint64_t[]){ 0xbeef });
            }
            else
            {
                after_left_jump_ip = emit(comp, OPCODE_JUMP_IF_TRUE, 1, (uint64_t[]){ 0xbeef });
            }

            if(after_left_jump_ip < 0)
            {
                goto error;
            }

            ip = emit(comp, OPCODE_POP, 0, NULL);
            if(ip < 0)
            {
                goto error;
            }

            ok = compile_expression(comp, logi->right);
            if(!ok)
            {
                goto error;
            }

            int after_right_ip = get_ip(comp);
            change_uint16_operand(comp, after_left_jump_ip + 1, after_right_ip);

            break;
        }
        case EXPRESSION_TERNARY:
        {
            const ApeTernaryExpr_t* ternary = &expr->ternary;

            ok = compile_expression(comp, ternary->test);
            if(!ok)
            {
                goto error;
            }

            int else_jump_ip = emit(comp, OPCODE_JUMP_IF_FALSE, 1, (uint64_t[]){ 0xbeef });

            ok = compile_expression(comp, ternary->if_true);
            if(!ok)
            {
                goto error;
            }

            int end_jump_ip = emit(comp, OPCODE_JUMP, 1, (uint64_t[]){ 0xbeef });

            int else_ip = get_ip(comp);
            change_uint16_operand(comp, else_jump_ip + 1, else_ip);

            ok = compile_expression(comp, ternary->if_false);
            if(!ok)
            {
                goto error;
            }

            int end_ip = get_ip(comp);
            change_uint16_operand(comp, end_jump_ip + 1, end_ip);

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

bool compile_code_block(ApeCompiler_t* comp, const ApeCodeblock_t* block)
{
    ApeSymbol_table_t* symbol_table = compiler_get_symbol_table(comp);
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
        int ip = emit(comp, OPCODE_NULL, 0, NULL);
        if(ip < 0)
        {
            return false;
        }
        ip = emit(comp, OPCODE_POP, 0, NULL);
        if(ip < 0)
        {
            return false;
        }
    }

    for(int i = 0; i < ptrarray_count(block->statements); i++)
    {
        const ApeStatement_t* stmt = ptrarray_get(block->statements, i);
        bool ok = compile_statement(comp, stmt);
        if(!ok)
        {
            return false;
        }
    }
    symbol_table_pop_block_scope(symbol_table);
    return true;
}

int add_constant(ApeCompiler_t* comp, ApeObject_t obj)
{
    bool ok = array_add(comp->constants, &obj);
    if(!ok)
    {
        return -1;
    }
    int pos = array_count(comp->constants) - 1;
    return pos;
}

void change_uint16_operand(ApeCompiler_t* comp, int ip, uint16_t operand)
{
    array(uint8_t)* bytecode = get_bytecode(comp);
    if((ip + 1) >= array_count(bytecode))
    {
        APE_ASSERT(false);
        return;
    }
    uint8_t hi = (uint8_t)(operand >> 8);
    array_set(bytecode, ip, &hi);
    uint8_t lo = (uint8_t)(operand);
    array_set(bytecode, ip + 1, &lo);
}

bool last_opcode_is(ApeCompiler_t* comp, opcode_t op)
{
    opcode_t last_opcode = get_last_opcode(comp);
    return last_opcode == op;
}

bool read_symbol(ApeCompiler_t* comp, const ApeSymbol_t* symbol)
{
    int ip = -1;
    if(symbol->type == SYMBOL_MODULE_GLOBAL)
    {
        ip = emit(comp, OPCODE_GET_MODULE_GLOBAL, 1, (uint64_t[]){ symbol->index });
    }
    else if(symbol->type == SYMBOL_APE_GLOBAL)
    {
        ip = emit(comp, OPCODE_GET_APE_GLOBAL, 1, (uint64_t[]){ symbol->index });
    }
    else if(symbol->type == SYMBOL_LOCAL)
    {
        ip = emit(comp, OPCODE_GET_LOCAL, 1, (uint64_t[]){ symbol->index });
    }
    else if(symbol->type == SYMBOL_FREE)
    {
        ip = emit(comp, OPCODE_GET_FREE, 1, (uint64_t[]){ symbol->index });
    }
    else if(symbol->type == SYMBOL_FUNCTION)
    {
        ip = emit(comp, OPCODE_CURRENT_FUNCTION, 0, NULL);
    }
    else if(symbol->type == SYMBOL_THIS)
    {
        ip = emit(comp, OPCODE_GET_THIS, 0, NULL);
    }
    return ip >= 0;
}

bool write_symbol(ApeCompiler_t* comp, const ApeSymbol_t* symbol, bool define)
{
    int ip = -1;
    if(symbol->type == SYMBOL_MODULE_GLOBAL)
    {
        if(define)
        {
            ip = emit(comp, OPCODE_DEFINE_MODULE_GLOBAL, 1, (uint64_t[]){ symbol->index });
        }
        else
        {
            ip = emit(comp, OPCODE_SET_MODULE_GLOBAL, 1, (uint64_t[]){ symbol->index });
        }
    }
    else if(symbol->type == SYMBOL_LOCAL)
    {
        if(define)
        {
            ip = emit(comp, OPCODE_DEFINE_LOCAL, 1, (uint64_t[]){ symbol->index });
        }
        else
        {
            ip = emit(comp, OPCODE_SET_LOCAL, 1, (uint64_t[]){ symbol->index });
        }
    }
    else if(symbol->type == SYMBOL_FREE)
    {
        ip = emit(comp, OPCODE_SET_FREE, 1, (uint64_t[]){ symbol->index });
    }
    return ip >= 0;
}

bool push_break_ip(ApeCompiler_t* comp, int ip)
{
    ApeCompilationScope_t* comp_scope = get_compilation_scope(comp);
    return array_push(comp_scope->break_ip_stack, &ip);
}

void pop_break_ip(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* comp_scope = get_compilation_scope(comp);
    if(array_count(comp_scope->break_ip_stack) == 0)
    {
        APE_ASSERT(false);
        return;
    }
    array_pop(comp_scope->break_ip_stack, NULL);
}

int get_break_ip(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* comp_scope = get_compilation_scope(comp);
    if(array_count(comp_scope->break_ip_stack) == 0)
    {
        return -1;
    }
    int* res = array_top(comp_scope->break_ip_stack);
    return *res;
}

bool push_continue_ip(ApeCompiler_t* comp, int ip)
{
    ApeCompilationScope_t* comp_scope = get_compilation_scope(comp);
    return array_push(comp_scope->continue_ip_stack, &ip);
}

void pop_continue_ip(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* comp_scope = get_compilation_scope(comp);
    if(array_count(comp_scope->continue_ip_stack) == 0)
    {
        APE_ASSERT(false);
        return;
    }
    array_pop(comp_scope->continue_ip_stack, NULL);
}

int get_continue_ip(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* comp_scope = get_compilation_scope(comp);
    if(array_count(comp_scope->continue_ip_stack) == 0)
    {
        APE_ASSERT(false);
        return -1;
    }
    int* res = array_top(comp_scope->continue_ip_stack);
    return *res;
}

int get_ip(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* compilation_scope = get_compilation_scope(comp);
    return array_count(compilation_scope->bytecode);
}

array(ApePosition_t) * get_src_positions(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* compilation_scope = get_compilation_scope(comp);
    return compilation_scope->src_positions;
}

array(uint8_t) * get_bytecode(ApeCompiler_t* comp)
{
    ApeCompilationScope_t* compilation_scope = get_compilation_scope(comp);
    return compilation_scope->bytecode;
}

ApeFileScope_t* file_scope_make(ApeCompiler_t* comp, ApeCompiledFile_t* file)
{
    ApeFileScope_t* file_scope = allocator_malloc(comp->alloc, sizeof(ApeFileScope_t));
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
    file_scope_destroy(file_scope);
    return NULL;
}

void file_scope_destroy(ApeFileScope_t* scope)
{
    for(int i = 0; i < ptrarray_count(scope->loaded_module_names); i++)
    {
        void* name = ptrarray_get(scope->loaded_module_names, i);
        allocator_free(scope->alloc, name);
    }
    ptrarray_destroy(scope->loaded_module_names);
    parser_destroy(scope->parser);
    allocator_free(scope->alloc, scope);
}

bool push_file_scope(ApeCompiler_t* comp, const char* filepath)
{
    ApeSymbol_table_t* prev_st = NULL;
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

    ApeFileScope_t* file_scope = file_scope_make(comp, file);
    if(!file_scope)
    {
        return false;
    }

    ok = ptrarray_push(comp->file_scopes, file_scope);
    if(!ok)
    {
        file_scope_destroy(file_scope);
        return false;
    }

    int global_offset = 0;
    if(prev_st)
    {
        ApeBlockScope_t* prev_st_top_scope = symbol_table_get_block_scope(prev_st);
        global_offset = prev_st_top_scope->offset + prev_st_top_scope->num_definitions;
    }

    ok = push_symbol_table(comp, global_offset);
    if(!ok)
    {
        ptrarray_pop(comp->file_scopes);
        file_scope_destroy(file_scope);
        return false;
    }

    return true;
}

void pop_file_scope(ApeCompiler_t* comp)
{
    ApeSymbol_table_t* popped_st = compiler_get_symbol_table(comp);
    ApeBlockScope_t* popped_st_top_scope = symbol_table_get_block_scope(popped_st);
    int popped_num_defs = popped_st_top_scope->num_definitions;

    while(compiler_get_symbol_table(comp))
    {
        pop_symbol_table(comp);
    }
    ApeFileScope_t* scope = ptrarray_top(comp->file_scopes);
    if(!scope)
    {
        APE_ASSERT(false);
        return;
    }
    file_scope_destroy(scope);

    ptrarray_pop(comp->file_scopes);

    if(ptrarray_count(comp->file_scopes) > 0)
    {
        ApeSymbol_table_t* current_st = compiler_get_symbol_table(comp);
        ApeBlockScope_t* current_st_top_scope = symbol_table_get_block_scope(current_st);
        current_st_top_scope->num_definitions += popped_num_defs;
    }
}

void set_compilation_scope(ApeCompiler_t* comp, ApeCompilationScope_t* scope)
{
    comp->compilation_scope = scope;
}