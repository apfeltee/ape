#include <stdlib.h>
#include <math.h>

#ifndef APE_AMALGAMATED
#include "compiler.h"

#include "ape.h"
#include "ast.h"
#include "object.h"
#include "gc.h"
#include "code.h"
#include "symbol_table.h"
#include "error.h"
#endif

static bool compile_code(compiler_t *comp, const char *code);
static bool compile_statements(compiler_t *comp, ptrarray(statement_t) *statements);
static bool import_module(compiler_t *comp, const statement_t *import_stmt);
static bool compile_statement(compiler_t *comp, const statement_t *stmt);
static bool compile_expression(compiler_t *comp, const expression_t *expr);
static bool compile_code_block(compiler_t *comp, const code_block_t *block);
static int  add_constant(compiler_t *comp, object_t obj);
static void change_uint16_operand(compiler_t *comp, int ip, uint16_t operand);
static bool last_opcode_is(compiler_t *comp, opcode_t op);
static void read_symbol(compiler_t *comp, symbol_t *symbol);
static void write_symbol(compiler_t *comp, symbol_t *symbol, bool define);

static void push_break_ip(compiler_t *comp, int ip);
static void pop_break_ip(compiler_t *comp);
static int  get_break_ip(compiler_t *comp);

static void push_continue_ip(compiler_t *comp, int ip);
static void pop_continue_ip(compiler_t *comp);
static int  get_continue_ip(compiler_t *comp);

static int  get_ip(compiler_t *comp);

static array(src_pos_t)* get_src_positions(compiler_t *comp);
static array(uint8_t)*   get_bytecode(compiler_t *comp);

static void push_file_scope(compiler_t *comp, const char *filepath, module_t *module);
static void pop_file_scope(compiler_t *comp);

static void set_compilation_scope(compiler_t *comp, compilation_scope_t *scope);

static module_t* get_current_module(compiler_t *comp);
static module_t* module_make(const char *name);
static void module_destroy(module_t *module);
static void module_add_symbol(module_t *module, const symbol_t *symbol);

static compiled_file_t* compiled_file_make(const char *path);
static void compiled_file_destroy(compiled_file_t *file);

static const char* get_module_name(const char *path);
static symbol_t* define_symbol(compiler_t *comp, src_pos_t pos, const char *name, bool assignable, bool can_shadow);

static bool is_comparison(operator_t op);

compiler_t *compiler_make(const ape_config_t *config, gcmem_t *mem, ptrarray(errortag_t) *errors) {
    compiler_t *comp = ape_malloc(sizeof(compiler_t));
    memset(comp, 0, sizeof(compiler_t));
    APE_ASSERT(config);
    comp->config = config;
    comp->mem = mem;
    comp->file_scopes = ptrarray_make();
    comp->constants = array_make(object_t);
    comp->errors = errors;
    comp->break_ip_stack = array_make(int);
    comp->continue_ip_stack = array_make(int);
    comp->src_positions_stack = array_make(src_pos_t);
    comp->modules = dict_make();
    comp->files = ptrarray_make();
    compiler_push_compilation_scope(comp);
    push_file_scope(comp, "none", NULL);
    return comp;
}

void compiler_destroy(compiler_t *comp) {
    if (!comp) {
        return;
    }
    array_destroy(comp->constants);
    array_destroy(comp->continue_ip_stack);
    array_destroy(comp->break_ip_stack);
    array_destroy(comp->src_positions_stack);
    while (compiler_get_compilation_scope(comp)) {
        compiler_pop_compilation_scope(comp);
    }
    while (ptrarray_count(comp->file_scopes) > 0) {
        pop_file_scope(comp);
    }
    ptrarray_destroy(comp->file_scopes);
    ptrarray_destroy_with_items(comp->files, compiled_file_destroy);
    dict_destroy_with_items(comp->modules, module_destroy);
    ape_free(comp);
}

compilation_result_t* compiler_compile(compiler_t *comp, const char *code) {
    // todo: make compiler_reset function
    array_clear(comp->src_positions_stack);
    array_clear(comp->break_ip_stack);
    array_clear(comp->continue_ip_stack);

    compilation_scope_t *compilation_scope = compiler_get_compilation_scope(comp);
    array_clear(compilation_scope->bytecode);
    array_clear(compilation_scope->src_positions);

    symbol_table_t *global_table_copy = symbol_table_copy(compiler_get_symbol_table(comp));

    bool ok = compile_code(comp, code);

    compilation_scope = compiler_get_compilation_scope(comp);

    while (compilation_scope->outer != NULL) {
        compiler_pop_compilation_scope(comp);
        compilation_scope = compiler_get_compilation_scope(comp);
    }

    if (!ok) {
        while (compiler_get_symbol_table(comp) != NULL) {
            compiler_pop_symbol_table(comp);
        }
        compiler_set_symbol_table(comp, global_table_copy);
        return NULL;
    }

    symbol_table_destroy(global_table_copy);

    compilation_scope = compiler_get_compilation_scope(comp);
    return compilation_scope_orphan_result(compilation_scope);
}

compilation_result_t* compiler_compile_file(compiler_t *comp, const char *path) {
    if (!comp->config->fileio.read_file.read_file) {
        errortag_t *err = error_make(ERROR_COMPILATION, src_pos_invalid, "File read function not configured");
        ptrarray_add(comp->errors, err);
        return NULL;
    }

    char *code = comp->config->fileio.read_file.read_file(comp->config->fileio.read_file.context, path);
    if (!code) {
        errortag_t *err = error_makef(ERROR_COMPILATION, src_pos_invalid, "Reading file \"%s\" failed", path);
        ptrarray_add(comp->errors, err);
        return NULL;
    }

    compiled_file_t *file = compiled_file_make(path);
    ptrarray_add(comp->files, file);

    APE_ASSERT(ptrarray_count(comp->file_scopes) == 1);
    file_scope_t *file_scope = ptrarray_top(comp->file_scopes);
    if (!file_scope) {
        APE_ASSERT(false);
        ape_free(code);
        return NULL;
    }
    compiled_file_t *prev_file = file_scope->file;
    file_scope->file = file;

    compilation_result_t *res = compiler_compile(comp, code);
    
    file_scope->file = prev_file;
    ape_free(code);
    return res;
}

compilation_scope_t* compiler_get_compilation_scope(compiler_t *comp) {
    return comp->compilation_scope;
}

void compiler_push_compilation_scope(compiler_t *comp) {
    compilation_scope_t *current_scope = compiler_get_compilation_scope(comp);
    compilation_scope_t *new_scope = compilation_scope_make(current_scope);
    set_compilation_scope(comp, new_scope);
}

void compiler_pop_compilation_scope(compiler_t *comp) {
    compilation_scope_t *current_scope = compiler_get_compilation_scope(comp);
    APE_ASSERT(current_scope);
    set_compilation_scope(comp, current_scope->outer);
    compilation_scope_destroy(current_scope);
}

void compiler_push_symbol_table(compiler_t *comp) {
    file_scope_t *file_scope = ptrarray_top(comp->file_scopes);
    if (!file_scope) {
        APE_ASSERT(false);
        return;
    }
    symbol_table_t *current_table = file_scope->symbol_table;
    file_scope->symbol_table = symbol_table_make(current_table);
}

void compiler_pop_symbol_table(compiler_t *comp) {
    file_scope_t *file_scope = ptrarray_top(comp->file_scopes);
    if (!file_scope) {
        APE_ASSERT(false);
        return;
    }
    symbol_table_t *current_table = file_scope->symbol_table;
    if (!current_table) {
        APE_ASSERT(false);
        return;
    }
    file_scope->symbol_table = current_table->outer;
    symbol_table_destroy(current_table);
}

symbol_table_t* compiler_get_symbol_table(compiler_t *comp) {
    file_scope_t *file_scope = ptrarray_top(comp->file_scopes);
    if (!file_scope) {
        APE_ASSERT(false);
        return NULL;
    }
    symbol_table_t *current_table = file_scope->symbol_table;
    if (!current_table) {
        return NULL;
    }
    return current_table;
}

void compiler_set_symbol_table(compiler_t *comp, symbol_table_t *table) {
    file_scope_t *file_scope = ptrarray_top(comp->file_scopes);
    if (!file_scope) {
        APE_ASSERT(false);
        return;
    }
    file_scope->symbol_table = table;
}

opcode_t compiler_last_opcode(compiler_t *comp) {
    compilation_scope_t *current_scope = compiler_get_compilation_scope(comp);
    return current_scope->last_opcode;
}

// INTERNAL
static bool compile_code(compiler_t *comp, const char *code) {
    file_scope_t *file_scope = ptrarray_top(comp->file_scopes);
    APE_ASSERT(file_scope);

    ptrarray(statement_t) *statements = parser_parse_all(file_scope->parser, code, file_scope->file);
    if (!statements) {
        // errors are added by parser
        return false;
    }

    bool ok = compile_statements(comp, statements);

    ptrarray_destroy_with_items(statements, statement_destroy);

    return ok;
}

static bool compile_statements(compiler_t *comp, ptrarray(statement_t) *statements) {
    bool ok = true;
    for (int i = 0; i < ptrarray_count(statements); i++) {
        const statement_t *stmt = ptrarray_get(statements, i);
        ok = compile_statement(comp, stmt);
        if (!ok) {
            break;
        }
    }
    return ok;
}

static bool import_module(compiler_t *comp, const statement_t *import_stmt) {
    bool result = false;
    char *filepath = NULL;

    file_scope_t *file_scope = ptrarray_top(comp->file_scopes);

    const char *module_path = import_stmt->import.path;
    const char *module_name = get_module_name(module_path);

    for (int i = 0; i < ptrarray_count(file_scope->loaded_module_names); i++) {
        const char *loaded_name = ptrarray_get(file_scope->loaded_module_names, i);
        if (kg_streq(loaded_name, module_name)) {
            errortag_t *err = error_makef(ERROR_COMPILATION, import_stmt->pos, "Module \"%s\" was already imported", module_name);
            ptrarray_add(comp->errors, err);
            result = false;
            goto end;
        }
    }

    strbuf_t *filepath_buf = strbuf_make();
    if (kg_is_path_absolute(module_path)) {
        strbuf_appendf(filepath_buf, "%s.bn", module_path);
    } else {
        strbuf_appendf(filepath_buf, "%s%s.bn", file_scope->file->dir_path, module_path);
    }
    const char *filepath_non_canonicalised = strbuf_get_string(filepath_buf);
    filepath = kg_canonicalise_path(filepath_non_canonicalised);
    strbuf_destroy(filepath_buf);

    symbol_table_t *symbol_table = compiler_get_symbol_table(comp);
    if (symbol_table->outer != NULL || ptrarray_count(symbol_table->block_scopes) > 1) {
        errortag_t *err = error_make(ERROR_COMPILATION, import_stmt->pos, "Modules can only be imported in global scope");
        ptrarray_add(comp->errors, err);
        result = false;
        goto end;
    }

    for (int i = 0; i < ptrarray_count(comp->file_scopes); i++) {
        file_scope_t *fs = ptrarray_get(comp->file_scopes, i);
        if (APE_STREQ(fs->file->path, filepath)) {
            errortag_t *err = error_makef(ERROR_COMPILATION, import_stmt->pos, "Cyclic reference of file \"%s\"", filepath);
            ptrarray_add(comp->errors, err);
            result = false;
            goto end;
        }
    }

    module_t *module = dict_get(comp->modules, filepath);
    if (!module) {
        if (!comp->config->fileio.read_file.read_file) {
            errortag_t *err = error_makef(ERROR_COMPILATION, import_stmt->pos, "Cannot import module \"%s\", file read function not configured", filepath);
            ptrarray_add(comp->errors, err);
            result = false;
            goto end;
        }

        char *code = comp->config->fileio.read_file.read_file(comp->config->fileio.read_file.context, filepath);
        if (!code) {
            errortag_t *err = error_makef(ERROR_COMPILATION, import_stmt->pos, "Reading module file \"%s\" failed", filepath);
            ptrarray_add(comp->errors, err);
            result = false;
            goto end;
        }

        module = module_make(module_name);
        push_file_scope(comp, filepath, module);
        bool ok = compile_code(comp, code);
        pop_file_scope(comp);
        ape_free(code);

        if (!ok) {
            module_destroy(module);
            result = false;
            goto end;
        }

        dict_set(comp->modules, filepath, module);
    }

    for (int i = 0; i < ptrarray_count(module->symbols); i++) {
        symbol_t *symbol = ptrarray_get(module->symbols, i);
        symbol_table_add_module_symbol(symbol_table, symbol);
    }

    ptrarray_add(file_scope->loaded_module_names, ape_strdup(module_name));
    
    result = true;

end:
    ape_free(filepath);
    return result;
}

static bool compile_statement(compiler_t *comp, const statement_t *stmt) {
    bool ok = false;
    array_push(comp->src_positions_stack, &stmt->pos);
    compilation_scope_t *compilation_scope = compiler_get_compilation_scope(comp);
    symbol_table_t *symbol_table = compiler_get_symbol_table(comp);
    switch (stmt->type) {
        case STATEMENT_EXPRESSION: {
            ok = compile_expression(comp, stmt->expression);
            if (!ok) {
                return false;
            }
            compiler_emit(comp, OPCODE_POP, 0, NULL);
            break;
        }
        case STATEMENT_DEFINE: {
            ok = compile_expression(comp, stmt->define.value);
            if (!ok) {
                return false;
            }
            
            symbol_t *symbol = define_symbol(comp, stmt->define.name.pos, stmt->define.name.value, stmt->define.assignable, false);
            if (!symbol) {
                return false;
            }

            if (symbol->type == SYMBOL_GLOBAL) {
                module_t *module = get_current_module(comp);
                if (module) {
                    module_add_symbol(module, symbol);
                }
            }

            write_symbol(comp, symbol, true);

            break;
        }
        case STATEMENT_IF: {
            const if_statement_t *if_stmt = &stmt->if_statement;

            array(int) *jump_to_end_ips = array_make(int);
            for (int i = 0; i < ptrarray_count(if_stmt->cases); i++) {
                if_case_t *if_case = ptrarray_get(if_stmt->cases, i);

                ok = compile_expression(comp, if_case->test);
                if (!ok) {
                    array_destroy(jump_to_end_ips);
                    return false;
                }

                int next_case_jump_ip = compiler_emit(comp, OPCODE_JUMP_IF_FALSE, 1, (uint64_t[]){0xbeef});

                ok = compile_code_block(comp, if_case->consequence);
                if (!ok) {
                    array_destroy(jump_to_end_ips);
                    return false;
                }

                int jump_to_end_ip = compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){0xbeef});
                array_add(jump_to_end_ips, &jump_to_end_ip);

                int after_elif_ip = get_ip(comp);
                change_uint16_operand(comp, next_case_jump_ip + 1, after_elif_ip);
            }

            if (if_stmt->alternative) {
                ok = compile_code_block(comp, if_stmt->alternative);
                if (!ok) {
                    array_destroy(jump_to_end_ips);
                    return false;
                }
            }

            int after_alt_ip = get_ip(comp);

            for (int i = 0; i < array_count(jump_to_end_ips); i++) {
                int *pos = array_get(jump_to_end_ips, i);
                change_uint16_operand(comp, *pos + 1, after_alt_ip);
            }

            array_destroy(jump_to_end_ips);
            
            break;
        }
        case STATEMENT_RETURN_VALUE: {
            if (compilation_scope->outer == NULL) {
                errortag_t *err = error_makef(ERROR_COMPILATION, stmt->pos, "Nothing to return from");
                ptrarray_add(comp->errors, err);
                return false;
            }
            if (stmt->return_value) {
                ok = compile_expression(comp, stmt->return_value);
                if (!ok) {
                    return false;
                }
                compiler_emit(comp, OPCODE_RETURN_VALUE, 0, NULL);
            } else {
                compiler_emit(comp, OPCODE_RETURN, 0, NULL);
            }
            break;
        }
        case STATEMENT_WHILE_LOOP: {
            const while_loop_statement_t *loop = &stmt->while_loop;

            int before_test_ip = get_ip(comp);

            ok = compile_expression(comp, loop->test);
            if (!ok) {
                return false;
            }

            int after_test_ip = get_ip(comp);
            compiler_emit(comp, OPCODE_JUMP_IF_TRUE, 1, (uint64_t[]){after_test_ip + 6});
            int jump_to_after_body_ip = compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){0xdead});

            push_continue_ip(comp, before_test_ip);
            push_break_ip(comp, jump_to_after_body_ip);
            ok = compile_code_block(comp, loop->body);
            if (!ok) {
                return false;
            }
            pop_break_ip(comp);
            pop_continue_ip(comp);

            compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){before_test_ip});

            int after_body_ip = get_ip(comp);
            change_uint16_operand(comp, jump_to_after_body_ip + 1, after_body_ip);

            break;
        }
        case STATEMENT_BREAK: {
            int break_ip = get_break_ip(comp);
            if (break_ip < 0) {
                errortag_t *err = error_makef(ERROR_COMPILATION, stmt->pos, "Nothing to break from.");
                ptrarray_add(comp->errors, err);
                return false;
            }
            compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){break_ip});
            break;
        }
        case STATEMENT_CONTINUE: {
            int continue_ip = get_continue_ip(comp);
            if (continue_ip < 0) {
                errortag_t *err = error_makef(ERROR_COMPILATION, stmt->pos, "Nothing to continue from.");
                ptrarray_add(comp->errors, err);
                return false;
            }
            compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){continue_ip});
            break;
        }
        case STATEMENT_FOREACH: {
            const foreach_statement_t *foreach = &stmt->foreach;
            symbol_table_push_block_scope(symbol_table);

            // Init
            symbol_t *index_symbol = define_symbol(comp, stmt->pos, "@i", false, true);
            if (!index_symbol) {
                APE_ASSERT(false);
                return false;
            }

            compiler_emit(comp, OPCODE_NUMBER, 1, (uint64_t[]){0});
            write_symbol(comp, index_symbol, true);
            symbol_t *source_symbol = NULL;
            if (foreach->source->type == EXPRESSION_IDENT) {
                source_symbol = symbol_table_resolve(symbol_table, foreach->source->ident.value);
                if (!source_symbol) {
                    errortag_t *err = error_makef(ERROR_COMPILATION, foreach->source->pos,
                                              "Symbol \"%s\" could not be resolved", foreach->source->ident.value);
                    ptrarray_add(comp->errors, err);
                    return false;
                }
            } else {
                ok = compile_expression(comp, foreach->source);
                if (!ok) {
                    return false;
                }
                source_symbol = define_symbol(comp, foreach->source->pos, "@source", false, true);
                if (!source_symbol) {
                    APE_ASSERT(false);
                    return false;
                }
                write_symbol(comp, source_symbol, true);
            }

            // Update
            int jump_to_after_update_ip = compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){0xbeef});
            int update_ip = get_ip(comp);
            read_symbol(comp, index_symbol);
            compiler_emit(comp, OPCODE_NUMBER, 1, (uint64_t[]){ape_double_to_uint64(1)});
            compiler_emit(comp, OPCODE_ADD, 0, NULL);
            write_symbol(comp, index_symbol, false);
            int after_update_ip = get_ip(comp);
            change_uint16_operand(comp, jump_to_after_update_ip + 1, after_update_ip);

            // Test
            array_push(comp->src_positions_stack, &foreach->source->pos);
            read_symbol(comp, source_symbol);
            compiler_emit(comp, OPCODE_LEN, 0, NULL);
            array_pop(comp->src_positions_stack, NULL);
            read_symbol(comp, index_symbol);
            compiler_emit(comp, OPCODE_COMPARE, 0, NULL);
            compiler_emit(comp, OPCODE_EQUAL, 0, NULL);

            int after_test_ip = get_ip(comp);
            compiler_emit(comp, OPCODE_JUMP_IF_FALSE, 1, (uint64_t[]){after_test_ip + 6});
            int jump_to_after_body_ip = compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){0xdead});

            read_symbol(comp, source_symbol);
            read_symbol(comp, index_symbol);
            compiler_emit(comp, OPCODE_GET_VALUE_AT, 0, NULL);

            symbol_t *iter_symbol  = define_symbol(comp, foreach->iterator.pos, foreach->iterator.value, false, false);
            if (!iter_symbol) {
                return false;
            }
            
            write_symbol(comp, iter_symbol, true);

            // Body
            push_continue_ip(comp, update_ip);
            push_break_ip(comp, jump_to_after_body_ip);
            ok = compile_code_block(comp, foreach->body);
            if (!ok) {
                return false;
            }
            pop_break_ip(comp);
            pop_continue_ip(comp);
            compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){update_ip});

            int after_body_ip = get_ip(comp);
            change_uint16_operand(comp, jump_to_after_body_ip + 1, after_body_ip);

            symbol_table_pop_block_scope(symbol_table);
            break;
        }
        case STATEMENT_FOR_LOOP: {
            const for_loop_statement_t *loop = &stmt->for_loop;

            symbol_table_push_block_scope(symbol_table);

            // Init
            bool ok = false;
            if (loop->init) {
                ok = compile_statement(comp, loop->init);
                if (!ok) {
                    return false;
                }
            }
            int jump_to_after_update_ip = compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){0xbeef});

            // Update
            int update_ip = get_ip(comp);
            if (loop->update) {
                ok = compile_expression(comp, loop->update);
                if (!ok) {
                    return false;
                }
                compiler_emit(comp, OPCODE_POP, 0, NULL);
            }
            int after_update_ip = get_ip(comp);
            change_uint16_operand(comp, jump_to_after_update_ip + 1, after_update_ip);

            // Test
            if (loop->test) {
                ok = compile_expression(comp, loop->test);
                if (!ok) {
                    return false;
                }
            } else {
                compiler_emit(comp, OPCODE_TRUE, 0, NULL);
            }
            int after_test_ip = get_ip(comp);

            compiler_emit(comp, OPCODE_JUMP_IF_TRUE, 1, (uint64_t[]){after_test_ip + 6});
            int jmp_to_after_body_ip = compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){0xdead});

            // Body
            push_continue_ip(comp, update_ip);
            push_break_ip(comp, jmp_to_after_body_ip);
            ok = compile_code_block(comp, loop->body);
            if (!ok) {
                return false;
            }
            pop_break_ip(comp);
            pop_continue_ip(comp);
            compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){update_ip});

            int after_body_ip = get_ip(comp);
            change_uint16_operand(comp, jmp_to_after_body_ip + 1, after_body_ip);

            symbol_table_pop_block_scope(symbol_table);
            break;
        }
        case STATEMENT_BLOCK: {
            ok = compile_code_block(comp, stmt->block);
            if (!ok) {
                return false;
            }
            break;
        }
        case STATEMENT_IMPORT: {
            ok = import_module(comp, stmt);
            if (!ok) {
                return false;
            }
            break;
        }
        case STATEMENT_RECOVER: {
            const recover_statement_t *recover = &stmt->recover;

            if (symbol_table_is_global_scope(symbol_table)) {
                errortag_t *err = error_make(ERROR_COMPILATION, stmt->pos,
                                          "Recover statement cannot be defined in global scope");
                ptrarray_add(comp->errors, err);
                return false;
            }

            if (!symbol_table_is_top_block_scope(symbol_table)) {
                errortag_t *err = error_make(ERROR_COMPILATION, stmt->pos,
                                          "Recover statement cannot be defined within other statements");
                ptrarray_add(comp->errors, err);
                return false;
            }

            int recover_ip = compiler_emit(comp, OPCODE_SET_RECOVER, 1, (uint64_t[]){0xbeef});
            int jump_to_after_recover_ip = compiler_emit(comp, OPCODE_JUMP, 1, (uint64_t[]){0xbeef});
            int after_jump_to_recover_ip = get_ip(comp);
            change_uint16_operand(comp, recover_ip + 1, after_jump_to_recover_ip);

            symbol_table_push_block_scope(symbol_table);

            symbol_t *error_symbol = define_symbol(comp, recover->error_ident.pos, recover->error_ident.value, false, false);
            if (!error_symbol) {
                return false;
            }

            write_symbol(comp, error_symbol, true);

            ok = compile_code_block(comp, recover->body);
            if (!ok) {
                return false;
            }

            if (!last_opcode_is(comp, OPCODE_RETURN) && !last_opcode_is(comp, OPCODE_RETURN_VALUE)) {
                errortag_t *err = error_make(ERROR_COMPILATION, stmt->pos,
                                          "Recover body must end with a return statement");
                ptrarray_add(comp->errors, err);
                return false;
            }

            symbol_table_pop_block_scope(symbol_table);

            int after_recover_ip = get_ip(comp);
            change_uint16_operand(comp, jump_to_after_recover_ip + 1, after_recover_ip);

            break;
        }
        default: {
            APE_ASSERT(false);
            return false;
        }
    }
    array_pop(comp->src_positions_stack, NULL);
    return true;
}

static bool compile_expression(compiler_t *comp, const expression_t *expr) {
    bool ok = false;
    array_push(comp->src_positions_stack, &expr->pos);
    compilation_scope_t *compilation_scope = compiler_get_compilation_scope(comp);
    symbol_table_t *symbol_table = compiler_get_symbol_table(comp);

    switch (expr->type) {
        case EXPRESSION_INFIX: {
            bool rearrange = false;

            opcode_t op = OPCODE_NONE;
            switch (expr->infix.op) {
                case OPERATOR_PLUS:        op = OPCODE_ADD; break;
                case OPERATOR_MINUS:       op = OPCODE_SUB; break;
                case OPERATOR_ASTERISK:    op = OPCODE_MUL; break;
                case OPERATOR_SLASH:       op = OPCODE_DIV; break;
                case OPERATOR_MODULUS:     op = OPCODE_MOD; break;
                case OPERATOR_EQ:          op = OPCODE_EQUAL; break;
                case OPERATOR_NOT_EQ:      op = OPCODE_NOT_EQUAL; break;
                case OPERATOR_GT:          op = OPCODE_GREATER_THAN; break;
                case OPERATOR_GTE:         op = OPCODE_GREATER_THAN_EQUAL; break;
                case OPERATOR_LT:          op = OPCODE_GREATER_THAN; rearrange = true; break;
                case OPERATOR_LTE:         op = OPCODE_GREATER_THAN_EQUAL; rearrange = true; break;
                case OPERATOR_BIT_OR:      op = OPCODE_OR; break;
                case OPERATOR_BIT_XOR:     op = OPCODE_XOR; break;
                case OPERATOR_BIT_AND:     op = OPCODE_AND; break;
                case OPERATOR_LSHIFT:      op = OPCODE_LSHIFT; break;
                case OPERATOR_RSHIFT:      op = OPCODE_RSHIFT; break;
                default: {
                    errortag_t *err = error_makef(ERROR_COMPILATION, expr->pos, "Unknown infix operator");
                    ptrarray_add(comp->errors, err);
                    return false;
                }
            }

            const expression_t *left = rearrange ? expr->infix.right : expr->infix.left;
            const expression_t *right = rearrange ? expr->infix.left : expr->infix.right;

            ok = compile_expression(comp, left);
            if (!ok) {
                return false;
            }

            ok = compile_expression(comp, right);
            if (!ok) {
                return false;
            }

            if (is_comparison(expr->infix.op)) {
                compiler_emit(comp, OPCODE_COMPARE, 0, NULL);
            }

            compiler_emit(comp, op, 0, NULL);

            break;
        }
        case EXPRESSION_NUMBER_LITERAL: {
            double number = expr->number_literal;
            compiler_emit(comp, OPCODE_NUMBER, 1, (uint64_t[]){ape_double_to_uint64(number)});
            break;
        }
        case EXPRESSION_STRING_LITERAL: {
            object_t obj = object_make_string(comp->mem, expr->string_literal);
            int pos = add_constant(comp, obj);
            compiler_emit(comp, OPCODE_CONSTANT, 1, (uint64_t[]){pos});
            break;
        }
        case EXPRESSION_NULL_LITERAL: {
            compiler_emit(comp, OPCODE_NULL, 0, NULL);
            break;
        }
        case EXPRESSION_BOOL_LITERAL: {
            compiler_emit(comp, expr->bool_literal ? OPCODE_TRUE : OPCODE_FALSE, 0, NULL);
            break;
        }
        case EXPRESSION_ARRAY_LITERAL: {
            for (int i = 0; i < ptrarray_count(expr->array); i++) {
                ok = compile_expression(comp, ptrarray_get(expr->array, i));
                if (!ok) {
                    return false;
                }
            }
            compiler_emit(comp, OPCODE_ARRAY, 1, (uint64_t[]){ptrarray_count(expr->array)});
            break;
        }
        case EXPRESSION_MAP_LITERAL: {
            const map_literal_t *map = &expr->map;
            int len = ptrarray_count(map->keys);
            compiler_emit(comp, OPCODE_MAP_START, 1, (uint64_t[]){len * 2});
            for (int i = 0; i < len; i++) {
                const expression_t *key = ptrarray_get(map->keys, i);
                const expression_t *val = ptrarray_get(map->values, i);

                ok = compile_expression(comp, key);
                if (!ok) {
                    return false;
                }

                ok = compile_expression(comp, val);
                if (!ok) {
                    return false;
                }
            }
            compiler_emit(comp, OPCODE_MAP_END, 1, (uint64_t[]){len * 2});
            break;
        }
        case EXPRESSION_PREFIX: {
            ok = compile_expression(comp, expr->prefix.right);
            if (!ok) {
                return false;
            }
            opcode_t op = OPCODE_NONE;
            switch (expr->prefix.op) {
                case OPERATOR_MINUS: op = OPCODE_MINUS; break;
                case OPERATOR_BANG: op = OPCODE_BANG; break;
                default: {
                    errortag_t *err = error_makef(ERROR_COMPILATION, expr->pos, "Unknown prefix operator.");
                    ptrarray_add(comp->errors, err);
                    return false;
                }
            }
            compiler_emit(comp, op, 0, NULL);
            break;
        }
        case EXPRESSION_IDENT: {
            const ident_t *ident = &expr->ident;
            symbol_t *symbol = symbol_table_resolve(symbol_table, ident->value);
            if (!symbol) {
                errortag_t *err = error_makef(ERROR_COMPILATION, ident->pos,
                                           "Symbol \"%s\" could not be resolved", ident->value);
                ptrarray_add(comp->errors, err);
                return false;
            }
            read_symbol(comp, symbol);
            break;
        }
        case EXPRESSION_INDEX: {
            const index_expression_t *index = &expr->index_expr;
            ok = compile_expression(comp, index->left);
            if (!ok) {
                return false;
            }
            ok = compile_expression(comp, index->index);
            if (!ok) {
                return false;
            }
            compiler_emit(comp, OPCODE_GET_INDEX, 0, NULL);
            break;
        }
        case EXPRESSION_FUNCTION_LITERAL: {
            const fn_literal_t *fn = &expr->fn_literal;

            compiler_push_compilation_scope(comp);
            compiler_push_symbol_table(comp);
            compilation_scope = compiler_get_compilation_scope(comp);
            symbol_table = compiler_get_symbol_table(comp);

            if (fn->name) {
                symbol_t *fn_symbol = symbol_table_define_function_name(symbol_table, fn->name, false);
                if (!fn_symbol) {
                    errortag_t *err = error_makef(ERROR_COMPILATION, expr->pos,
                                               "Cannot define symbol \"%s\"", fn->name);
                    ptrarray_add(comp->errors, err);
                    return false;
                }
            }

            symbol_t *this_symbol = symbol_table_define_this(symbol_table);
            if (!this_symbol) {
                errortag_t *err = error_make(ERROR_COMPILATION, expr->pos, "Cannot define \"this\" symbol");
                ptrarray_add(comp->errors, err);
                return false;
            }

            for (int i = 0; i < array_count(expr->fn_literal.params); i++) {
                ident_t *param = array_get(expr->fn_literal.params, i);
                symbol_t *param_symbol = define_symbol(comp, param->pos, param->value, true, false);
                if (!param_symbol) {
                    return false;
                }
            }

            ok = compile_statements(comp, fn->body->statements);
            if (!ok) {
                return false;
            }
        
            if (!last_opcode_is(comp, OPCODE_RETURN_VALUE) && !last_opcode_is(comp, OPCODE_RETURN)) {
                compiler_emit(comp, OPCODE_RETURN, 0, NULL);
            }

            ptrarray(symbol_t) *free_symbols = symbol_table->free_symbols;
            symbol_table->free_symbols = NULL; // because it gets destroyed with compiler_pop_compilation_scope()

            int num_locals = symbol_table->max_num_definitions;
            
            compilation_result_t *comp_res = compilation_scope_orphan_result(compilation_scope);
            compiler_pop_symbol_table(comp);
            compiler_pop_compilation_scope(comp);
            compilation_scope = compiler_get_compilation_scope(comp);
            symbol_table = compiler_get_symbol_table(comp);
            
            object_t obj = object_make_function(comp->mem, fn->name, comp_res, true,
                                                num_locals, array_count(fn->params), 0);

            for (int i = 0; i < ptrarray_count(free_symbols); i++) {
                symbol_t *symbol = ptrarray_get(free_symbols, i);
                read_symbol(comp, symbol);
            }

            int pos = add_constant(comp, obj);
            compiler_emit(comp, OPCODE_FUNCTION, 2, (uint64_t[]){pos, ptrarray_count(free_symbols)});

            ptrarray_destroy_with_items(free_symbols, symbol_destroy);

            break;
        }
        case EXPRESSION_CALL: {
            ok = compile_expression(comp, expr->call_expr.function);
            if (!ok) {
                return false;
            }

            for (int i = 0; i < ptrarray_count(expr->call_expr.args); i++) {
                const expression_t *arg_expr = ptrarray_get(expr->call_expr.args, i);
                ok = compile_expression(comp, arg_expr);
                if (!ok) {
                    return false;
                }
            }

            compiler_emit(comp, OPCODE_CALL, 1, (uint64_t[]){ptrarray_count(expr->call_expr.args)});
            break;
        }
        case EXPRESSION_ASSIGN: {
            const assign_expression_t *assign = &expr->assign;
            if (assign->dest->type != EXPRESSION_IDENT && assign->dest->type != EXPRESSION_INDEX) {
                errortag_t *err = error_makef(ERROR_COMPILATION, assign->dest->pos,
                                          "Expression is not assignable.");
                ptrarray_add(comp->errors, err);
                return false;
            }

            ok = compile_expression(comp, assign->source);
            if (!ok) {
                return false;
            }

            compiler_emit(comp, OPCODE_DUP, 0, NULL);

            array_push(comp->src_positions_stack, &assign->dest->pos);
            if (assign->dest->type == EXPRESSION_IDENT) {
                const ident_t *ident = &assign->dest->ident;
                symbol_t *symbol = symbol_table_resolve(symbol_table, ident->value);
                if (!symbol) {
                    errortag_t *err = error_makef(ERROR_COMPILATION, assign->dest->pos,
                                              "Symbol \"%s\" could not be resolved", ident->value);
                    ptrarray_add(comp->errors, err);
                    return false;
                }
                if (!symbol->assignable) {
                    errortag_t *err = error_makef(ERROR_COMPILATION, assign->dest->pos,
                                              "Symbol \"%s\" is not assignable", ident->value);
                    ptrarray_add(comp->errors, err);
                    return false;
                }
                write_symbol(comp, symbol, false);
            } else if (assign->dest->type == EXPRESSION_INDEX) {
                const index_expression_t *index = &assign->dest->index_expr;
                ok = compile_expression(comp, index->left);
                if (!ok) {
                    return false;
                }
                ok = compile_expression(comp, index->index);
                if (!ok) {
                    return false;
                }
                compiler_emit(comp, OPCODE_SET_INDEX, 0, NULL);
            }
            array_pop(comp->src_positions_stack, NULL);
            break;
        }
        case EXPRESSION_LOGICAL: {
            const logical_expression_t* logi = &expr->logical;

            ok = compile_expression(comp, logi->left);
            if (!ok) {
                return false;
            }

            compiler_emit(comp, OPCODE_DUP, 0, NULL);

            int after_left_jump_ip = 0;
            if (logi->op == OPERATOR_LOGICAL_AND) {
                after_left_jump_ip = compiler_emit(comp, OPCODE_JUMP_IF_FALSE, 1, (uint64_t[]){0xbeef});
            } else {
                after_left_jump_ip = compiler_emit(comp, OPCODE_JUMP_IF_TRUE, 1, (uint64_t[]){0xbeef});
            }

            compiler_emit(comp, OPCODE_POP, 0, NULL);

            ok = compile_expression(comp, logi->right);
            if (!ok) {
                return false;
            }

            int after_right_ip = get_ip(comp);
            change_uint16_operand(comp, after_left_jump_ip + 1, after_right_ip);

            break;
        }
        default: {
            APE_ASSERT(false);
            break;
        }
    }
    array_pop(comp->src_positions_stack, NULL);
    return true;
}

static bool compile_code_block(compiler_t *comp, const code_block_t *block) {
    symbol_table_t *symbol_table = compiler_get_symbol_table(comp);
    symbol_table_push_block_scope(symbol_table);
    if (ptrarray_count(block->statements) == 0) {
        compiler_emit(comp, OPCODE_NULL, 0, NULL);
        compiler_emit(comp, OPCODE_POP, 0, NULL);
    }
    for (int i = 0; i < ptrarray_count(block->statements); i++) {
        const statement_t *stmt = ptrarray_get(block->statements, i);
        bool ok = compile_statement(comp, stmt);
        if (!ok) {
            return false;
        }
    }
    symbol_table_pop_block_scope(symbol_table);
    return true;
}

static int add_constant(compiler_t *comp, object_t obj) {
    array_add(comp->constants, &obj);
    int pos = array_count(comp->constants) - 1;
    return pos;
}

int compiler_emit(compiler_t *comp, opcode_t op, int operands_count, uint64_t *operands) {
    int ip = get_ip(comp);
    int len = code_make(op, operands_count, operands, get_bytecode(comp));
    for (int i = 0; i < len; i++) {
        src_pos_t *src_pos = array_top(comp->src_positions_stack);
        APE_ASSERT(src_pos->line >= 0);
        APE_ASSERT(src_pos->column >= 0);
        array_add(get_src_positions(comp), src_pos);
    }
    compilation_scope_t *compilation_scope = compiler_get_compilation_scope(comp);
    compilation_scope->last_opcode = op;
    return ip;
}

static void change_uint16_operand(compiler_t *comp, int ip, uint16_t operand) {
    array(uint8_t) *bytecode = get_bytecode(comp);
    if ((ip + 1) >= array_count(bytecode)) {
        APE_ASSERT(false);
        return;
    }
    uint8_t hi = operand >> 8;
    array_set(bytecode, ip, &hi);
    uint8_t lo = operand;
    array_set(bytecode, ip + 1, &lo);
}

static bool last_opcode_is(compiler_t *comp, opcode_t op) {
    opcode_t last_opcode = compiler_last_opcode(comp);
    return last_opcode == op;
}

static void read_symbol(compiler_t *comp, symbol_t *symbol) {
    if (symbol->type == SYMBOL_GLOBAL) {
        compiler_emit(comp, OPCODE_GET_GLOBAL, 1, (uint64_t[]){symbol->index});
    } else if (symbol->type == SYMBOL_NATIVE_FUNCTION) {
        compiler_emit(comp, OPCODE_GET_NATIVE_FUNCTION, 1, (uint64_t[]){symbol->index});
    } else if (symbol->type == SYMBOL_LOCAL) {
        compiler_emit(comp, OPCODE_GET_LOCAL, 1, (uint64_t[]){symbol->index});
    } else if (symbol->type == SYMBOL_FREE) {
        compiler_emit(comp, OPCODE_GET_FREE, 1, (uint64_t[]){symbol->index});
    } else if (symbol->type == SYMBOL_FUNCTION) {
        compiler_emit(comp, OPCODE_CURRENT_FUNCTION, 0, NULL);
    } else if (symbol->type == SYMBOL_THIS) {
        compiler_emit(comp, OPCODE_GET_THIS, 0, NULL);
    }
}

static void write_symbol(compiler_t *comp, symbol_t *symbol, bool define) {
    if (symbol->type == SYMBOL_GLOBAL) {
        if (define) {
            compiler_emit(comp, OPCODE_DEFINE_GLOBAL, 1, (uint64_t[]){symbol->index});
        } else {
            compiler_emit(comp, OPCODE_SET_GLOBAL, 1, (uint64_t[]){symbol->index});
        }
    } else if (symbol->type == SYMBOL_LOCAL) {
        if (define) {
            compiler_emit(comp, OPCODE_DEFINE_LOCAL, 1, (uint64_t[]){symbol->index});
        } else {
            compiler_emit(comp, OPCODE_SET_LOCAL, 1, (uint64_t[]){symbol->index});
        }
    } else if (symbol->type == SYMBOL_FREE) {
        compiler_emit(comp, OPCODE_SET_FREE, 1, (uint64_t[]){symbol->index});
    }
}

static void push_break_ip(compiler_t *comp, int ip) {
    array_push(comp->break_ip_stack, &ip);
}

static void pop_break_ip(compiler_t *comp) {
    if (array_count(comp->break_ip_stack) == 0) {
        APE_ASSERT(false);
        return;
    }
    array_pop(comp->break_ip_stack, NULL);
}

static int get_break_ip(compiler_t *comp) {
    if (array_count(comp->break_ip_stack) == 0) {
        APE_ASSERT(false);
        return -1;
    }
    int *res = array_top(comp->break_ip_stack);
    return *res;
}

static void push_continue_ip(compiler_t *comp, int ip) {
    array_push(comp->continue_ip_stack, &ip);
}

static void pop_continue_ip(compiler_t *comp) {
    if (array_count(comp->continue_ip_stack) == 0) {
        APE_ASSERT(false);
        return;
    }
    array_pop(comp->continue_ip_stack, NULL);
}

static int get_continue_ip(compiler_t *comp) {
    if (array_count(comp->continue_ip_stack) == 0) {
        return -1;
    }
    int *res = array_top(comp->continue_ip_stack);
    return *res;
}

static int get_ip(compiler_t *comp) {
    compilation_scope_t *compilation_scope = compiler_get_compilation_scope(comp);
    return array_count(compilation_scope->bytecode);
}

static array(src_pos_t)* get_src_positions(compiler_t *comp) {
    compilation_scope_t *compilation_scope = compiler_get_compilation_scope(comp);
    return compilation_scope->src_positions;
}

static array(uint8_t)* get_bytecode(compiler_t *comp) {
    compilation_scope_t *compilation_scope = compiler_get_compilation_scope(comp);
    return compilation_scope->bytecode;
}

static void push_file_scope(compiler_t *comp, const char *filepath, module_t *module) {
    symbol_table_t *prev_st = NULL;
    if (ptrarray_count(comp->file_scopes) > 0) {
        prev_st = compiler_get_symbol_table(comp);
    }
    file_scope_t *file_scope = ape_malloc(sizeof(file_scope_t));
    memset(file_scope, 0, sizeof(file_scope_t));
    file_scope->parser = parser_make(comp->config, comp->errors);
    file_scope->symbol_table = NULL;
    file_scope->module = module;
    file_scope->file = compiled_file_make(filepath);
    ptrarray_add(comp->files, file_scope->file);
    file_scope->loaded_module_names = ptrarray_make();

    ptrarray_push(comp->file_scopes, file_scope);
    compiler_push_symbol_table(comp);

    if (prev_st) {
        block_scope_t *prev_st_top_scope = symbol_table_get_block_scope(prev_st);
        symbol_table_t *new_st = compiler_get_symbol_table(comp);
        block_scope_t *new_st_top_scope = symbol_table_get_block_scope(new_st);
        new_st_top_scope->offset = prev_st_top_scope->offset + prev_st_top_scope->num_definitions;
    }
}

static void pop_file_scope(compiler_t *comp) {
    symbol_table_t *popped_st = compiler_get_symbol_table(comp);
    block_scope_t *popped_st_top_scope = symbol_table_get_block_scope(popped_st);
    int popped_num_defs = popped_st_top_scope->num_definitions;

    file_scope_t *scope = ptrarray_top(comp->file_scopes);
    if (!scope) {
        APE_ASSERT(false);
        return;
    }
    while (compiler_get_symbol_table(comp)) {
        compiler_pop_symbol_table(comp);
    }
    ptrarray_destroy_with_items(scope->loaded_module_names, ape_free);
    scope->loaded_module_names = NULL;
    
    parser_destroy(scope->parser);

    ape_free(scope);
    ptrarray_pop(comp->file_scopes);

    if (ptrarray_count(comp->file_scopes) > 0) {
        symbol_table_t *current_st = compiler_get_symbol_table(comp);
        block_scope_t *current_st_top_scope = symbol_table_get_block_scope(current_st);
        current_st_top_scope->num_definitions += popped_num_defs;
    }
}

static void set_compilation_scope(compiler_t *comp, compilation_scope_t *scope) {
    comp->compilation_scope = scope;
}

static module_t* get_current_module(compiler_t *comp) {
    file_scope_t *scope = ptrarray_top(comp->file_scopes);
    return scope->module;
}

static module_t* module_make(const char *name) {
    module_t *module = ape_malloc(sizeof(module_t));
    module->name = ape_strdup(name);
    module->symbols = ptrarray_make();
    return module;
}

static void module_destroy(module_t *module) {
    ape_free(module->name);
    ptrarray_destroy_with_items(module->symbols, symbol_destroy);
    ape_free(module);
}

static void module_add_symbol(module_t *module, const symbol_t *symbol) {
    strbuf_t *name_buf = strbuf_make();
    strbuf_appendf(name_buf, "%s::%s", module->name, symbol->name);
    symbol_t *module_symbol = symbol_make(strbuf_get_string(name_buf), SYMBOL_GLOBAL, symbol->index, false);
    strbuf_destroy(name_buf);
    ptrarray_add(module->symbols, module_symbol);
}

static compiled_file_t* compiled_file_make(const char *path) {
    compiled_file_t *file = ape_malloc(sizeof(compiled_file_t));
    const char *last_slash_pos = strrchr(path, '/');
    if (last_slash_pos) {
        size_t len = last_slash_pos - path + 1;
        file->dir_path = ape_strndup(path, len);
    } else {
        file->dir_path = ape_strdup("");
    }
    file->path = ape_strdup(path);
    file->lines = ptrarray_make();
    return file;
}

static void compiled_file_destroy(compiled_file_t *file) {
    if (!file) {
        return;
    }
    ptrarray_destroy_with_items(file->lines, ape_free);
    ape_free(file->dir_path);
    ape_free(file->path);
    ape_free(file);
}

static const char* get_module_name(const char *path) {
    const char *last_slash_pos = strrchr(path, '/');
    if (last_slash_pos) {
        return last_slash_pos + 1;
    }
    return path;
}

static symbol_t* define_symbol(compiler_t *comp, src_pos_t pos, const char *name, bool assignable, bool can_shadow) {
    symbol_table_t *symbol_table = compiler_get_symbol_table(comp);
    if (!can_shadow && !symbol_table_is_top_global_scope(symbol_table)) {
        symbol_t *current_symbol = symbol_table_resolve(symbol_table, name);
        if (current_symbol) {
            errortag_t *err = error_makef(ERROR_COMPILATION, pos, "Symbol \"%s\" is already defined", name);
            ptrarray_add(comp->errors, err);
            return NULL;
        }
    }

    symbol_t *symbol = symbol_table_define(symbol_table, name, assignable);
    if (!symbol) {
        errortag_t *err = error_makef(ERROR_COMPILATION, pos, "Cannot define symbol \"%s\"", name);
        ptrarray_add(comp->errors, err);
        return false;
    }

    return symbol;
}

static bool is_comparison(operator_t op) {
    switch (op) {
        case OPERATOR_EQ:
        case OPERATOR_NOT_EQ:
        case OPERATOR_GT:
        case OPERATOR_GTE:
        case OPERATOR_LT:
        case OPERATOR_LTE:
            return true;
        default:
            return false;
    }
    return false;
}
