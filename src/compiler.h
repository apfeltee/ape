#ifndef compiler_h
#define compiler_h

#ifndef APE_AMALGAMATED
#include "collections.h"
#include "common.h"
#include "parser.h"
#include "code.h"
#include "token.h"
#include "compilation_scope.h"
#endif

typedef struct ape_config ape_config_t;
typedef struct gcmem gcmem_t;
typedef struct symbol_table symbol_table_t;
typedef struct module module_t;
typedef struct compiled_file compiled_file_t;
typedef struct file_scope file_scope_t;
typedef struct compiler compiler_t;

struct module
{
    char *name;
    ptrarray(symbol_t) *symbols;
};

struct compiled_file
{
    char *dir_path;
    char *path;
    ptrarray(char*) *lines;
};

struct file_scope {
    parser_t *parser;
    symbol_table_t *symbol_table;
    module_t *module;
    compiled_file_t *file;
    ptrarray(char) *loaded_module_names;
};

struct compiler {
    const ape_config_t *config;
    gcmem_t *mem;
    compilation_scope_t *compilation_scope;
    ptrarray(file_scope_t) *file_scopes;
    array(object_t) *constants;
    ptrarray(errortag_t) *errors;
    array(src_pos_t) *src_positions_stack;
    array(int) *break_ip_stack;
    array(int) *continue_ip_stack;
    dict(module_t) *modules;
    ptrarray(compiled_file_t) *files;
};

APE_INTERNAL compiler_t *compiler_make(const ape_config_t *config, gcmem_t *mem, ptrarray(errortag_t) *errors);
APE_INTERNAL void compiler_destroy(compiler_t *comp);
APE_INTERNAL compilation_result_t* compiler_compile(compiler_t *comp, const char *code);
APE_INTERNAL compilation_result_t* compiler_compile_file(compiler_t *comp, const char *path);
APE_INTERNAL int compiler_emit(compiler_t *comp, opcode_t op, int operands_count, uint64_t *operands);
APE_INTERNAL compilation_scope_t* compiler_get_compilation_scope(compiler_t *comp);
APE_INTERNAL void compiler_push_compilation_scope(compiler_t *comp);
APE_INTERNAL void compiler_pop_compilation_scope(compiler_t *comp);
APE_INTERNAL void compiler_push_symbol_table(compiler_t *comp);
APE_INTERNAL void compiler_pop_symbol_table(compiler_t *comp);
APE_INTERNAL symbol_table_t* compiler_get_symbol_table(compiler_t *comp);
APE_INTERNAL void compiler_set_symbol_table(compiler_t *comp, symbol_table_t *table);
APE_INTERNAL opcode_t compiler_last_opcode(compiler_t *comp);

#endif /* compiler_h */
