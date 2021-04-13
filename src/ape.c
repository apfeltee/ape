#include "ape.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#define APE_IMPL_VERSION_MAJOR 0
#define APE_IMPL_VERSION_MINOR 5
#define APE_IMPL_VERSION_PATCH 1

#if (APE_VERSION_MAJOR != APE_IMPL_VERSION_MAJOR)\
 || (APE_VERSION_MINOR != APE_IMPL_VERSION_MINOR)\
 || (APE_VERSION_PATCH != APE_IMPL_VERSION_PATCH)
    #error "Version mismatch"
#endif

#ifndef APE_AMALGAMATED
#include "ape.h"
#include "gc.h"
#include "compiler.h"
#include "lexer.h"
#include "parser.h"
#include "vm.h"
#include "error.h"
#include "symbol_table.h"
#include "traceback.h"
#endif

typedef struct native_fn_wrapper {
    ape_native_fn fn;
    ape_t *ape;
    void *data;
} native_fn_wrapper_t;

typedef struct ape_program {
    ape_t *ape;
    compilation_result_t *comp_res;
} ape_program_t;

typedef struct ape {
    gcmem_t *mem;
    compiler_t *compiler;
    vm_t *vm;
    ptrarray(ape_error_t) *errors;
    ptrarray(native_fn_wrapper_t) *native_fn_wrappers;
    ape_config_t config;
} ape_t;

static object_t ape_native_fn_wrapper(vm_t *vm, void *data, int argc, object_t *args);
static object_t ape_object_to_object(ape_object_t obj);
static ape_object_t object_to_ape_object(object_t obj);

static void reset_state(ape_t *ape);
static void set_default_config(ape_t *ape);
static char* read_file_default(void *ctx, const char *filename);
static size_t write_file_default(void* context, const char *path, const char *string, size_t string_size);
static size_t stdout_write_default(void* context, const void *data, size_t size);

#undef malloc
#undef free

ape_malloc_fn ape_malloc = malloc;
ape_free_fn ape_free = free;

//-----------------------------------------------------------------------------
// Ape
//-----------------------------------------------------------------------------

void ape_set_memory_functions(ape_malloc_fn malloc_fn, ape_free_fn free_fn) {
    ape_malloc = malloc_fn;
    ape_free = free_fn;
    collections_set_memory_functions(malloc_fn, free_fn);
}

ape_t *ape_make(void) {
    ape_t *ape = ape_malloc(sizeof(ape_t));
    memset(ape, 0, sizeof(ape_t));

    set_default_config(ape);
    
    ape->mem = gcmem_make();
    ape->errors = ptrarray_make();
    if (!ape->errors) {
        goto err;
    }
    if (!ape->mem) {
        goto err;
    }
    ape->compiler = compiler_make(&ape->config, ape->mem, ape->errors);
    if (!ape->compiler) {
        goto err;
    }
    ape->vm = vm_make(&ape->config, ape->mem, ape->errors);
    if (!ape->vm) {
        goto err;
    }
    ape->native_fn_wrappers = ptrarray_make();
    if (!ape->native_fn_wrappers) {
        goto err;
    }
    return ape;
err:
    gcmem_destroy(ape->mem);
    compiler_destroy(ape->compiler);
    vm_destroy(ape->vm);
    ptrarray_destroy(ape->errors);
    return NULL;
}

void ape_destroy(ape_t *ape) {
    if (!ape) {
        return;
    }
    ptrarray_destroy_with_items(ape->native_fn_wrappers, ape_free);
    ptrarray_destroy_with_items(ape->errors, error_destroy);
    vm_destroy(ape->vm);
    compiler_destroy(ape->compiler);
    gcmem_destroy(ape->mem);
    ape_free(ape);
}

void ape_set_repl_mode(ape_t *ape, bool enabled) {
    ape->config.repl_mode = enabled;
}

void ape_set_gc_interval(ape_t *ape, int interval) {
    ape->config.gc_interval = interval;
}

void ape_set_stdout_write_function(ape_t *ape, ape_stdout_write_fn stdout_write, void *context) {
    ape->config.stdio.write.write = stdout_write;
    ape->config.stdio.write.context = context;
}

void ape_set_file_write_function(ape_t *ape, ape_write_file_fn file_write, void *context) {
    ape->config.fileio.write_file.write_file = file_write;
    ape->config.fileio.write_file.context = context;
}

void ape_set_file_read_function(ape_t *ape, ape_read_file_fn file_read, void *context) {
    ape->config.fileio.read_file.read_file = file_read;
    ape->config.fileio.read_file.context = context;
}

ape_program_t* ape_compile(ape_t *ape, const char *code) {
    ptrarray_clear_and_destroy_items(ape->errors, error_destroy);

    compilation_result_t *comp_res = NULL;

    comp_res = compiler_compile(ape->compiler, code);
    if (!comp_res || ptrarray_count(ape->errors) > 0) {
        goto err;
    }

    ape_program_t *program = ape_malloc(sizeof(ape_program_t));
    program->ape = ape;
    program->comp_res = comp_res;
    return program;

err:
    compilation_result_destroy(comp_res);
    return NULL;
}

ape_program_t* ape_compile_file(ape_t *ape, const char *path) {
    ptrarray_clear_and_destroy_items(ape->errors, error_destroy);

    compilation_result_t *comp_res = NULL;

    comp_res = compiler_compile_file(ape->compiler, path);
    if (!comp_res || ptrarray_count(ape->errors) > 0) {
        goto err;
    }

    ape_program_t *program = ape_malloc(sizeof(ape_program_t));
    program->ape = ape;
    program->comp_res = comp_res;
    return program;

err:
    compilation_result_destroy(comp_res);
    return NULL;
}

ape_object_t ape_execute_program(ape_t *ape, const ape_program_t *program) {
    reset_state(ape);
 
    if (ape != program->ape) {
        errortag_t *err = error_make(ERROR_USER, src_pos_invalid, "ape program was compiled with a different ape instance");
        ptrarray_add(ape->errors, err);
        return ape_object_make_null();
    }

    bool ok = vm_run(ape->vm, program->comp_res, ape->compiler->constants);
    if (!ok || ptrarray_count(ape->errors)) {
        return ape_object_make_null();
    }

    APE_ASSERT(ape->vm->sp == 0);

    object_t res = vm_get_last_popped(ape->vm);
    if (object_get_type(res) == OBJECT_NONE) {
        return ape_object_make_null();
    }

    return object_to_ape_object(res);
}

void ape_program_destroy(ape_program_t *program) {
    if (!program) {
        return;
    }
    compilation_result_destroy(program->comp_res);
    ape_free(program);
}

ape_object_t ape_execute(ape_t *ape, const char *code) {
    reset_state(ape);

    compilation_result_t *comp_res = NULL;

    comp_res = compiler_compile(ape->compiler, code);
    if (!comp_res || ptrarray_count(ape->errors) > 0) {
        goto err;
    }

    bool ok = vm_run(ape->vm, comp_res, ape->compiler->constants);
    if (!ok || ptrarray_count(ape->errors)) {
        goto err;
    }

    APE_ASSERT(ape->vm->sp == 0);

    object_t res = vm_get_last_popped(ape->vm);
    if (object_get_type(res) == OBJECT_NONE) {
        goto err;
    }

    compilation_result_destroy(comp_res);

    return object_to_ape_object(res);

err:
    compilation_result_destroy(comp_res);
    return ape_object_make_null();
}

ape_object_t ape_execute_file(ape_t *ape, const char *path) {
    reset_state(ape);

    compilation_result_t *comp_res = NULL;

    comp_res = compiler_compile_file(ape->compiler, path);
    if (!comp_res || ptrarray_count(ape->errors) > 0) {
        goto err;
    }

    bool ok = vm_run(ape->vm, comp_res, ape->compiler->constants);
    if (!ok || ptrarray_count(ape->errors)) {
        goto err;
    }

    APE_ASSERT(ape->vm->sp == 0);

    object_t res = vm_get_last_popped(ape->vm);
    if (object_get_type(res) == OBJECT_NONE) {
        goto err;
    }

    compilation_result_destroy(comp_res);

    return object_to_ape_object(res);

err:
    compilation_result_destroy(comp_res);
    return ape_object_make_null();
}

ape_object_t ape_call(ape_t *ape, const char *function_name, int argc, ape_object_t *args) {
    reset_state(ape);

    object_t callee = ape_object_to_object(ape_get_object(ape, function_name));
    if (object_get_type(callee) == OBJECT_NULL) {
        return ape_object_make_null();
    }
    object_t res = vm_call(ape->vm, ape->compiler->constants, callee, argc, (object_t*)args);
    if (ptrarray_count(ape->errors) > 0) {
        return ape_object_make_null();
    }
    return object_to_ape_object(res);
}

bool ape_has_errors(const ape_t *ape) {
    return ptrarray_count(ape->errors) > 0;
}

int ape_errors_count(const ape_t *ape) {
    return ptrarray_count(ape->errors);
}

const ape_error_t* ape_get_error(const ape_t *ape, int index) {
    return ptrarray_get(ape->errors, index);
}

bool ape_set_native_function(ape_t *ape, const char *name, ape_native_fn fn, void *data) {
    native_fn_wrapper_t *wrapper = ape_malloc(sizeof(native_fn_wrapper_t));
    memset(wrapper, 0, sizeof(native_fn_wrapper_t));
    wrapper->fn = fn;
    wrapper->ape = ape;
    wrapper->data = data;
    object_t wrapper_native_function = object_make_native_function(ape->mem, name, ape_native_fn_wrapper, wrapper);
    int ix = array_count(ape->vm->native_functions);
    array_add(ape->vm->native_functions, &wrapper_native_function);
    symbol_table_t *symbol_table = compiler_get_symbol_table(ape->compiler);
    symbol_table_define_native_function(symbol_table, name, ix);
    ptrarray_add(ape->native_fn_wrappers, wrapper);
    return true;
}

bool ape_set_global_constant(ape_t *ape, const char *name, ape_object_t obj) {
    symbol_table_t *symbol_table = compiler_get_symbol_table(ape->compiler);
    symbol_t *symbol = NULL;
    if (symbol_table_symbol_is_defined(symbol_table, name)) {
        symbol = symbol_table_resolve(symbol_table, name);
        if (symbol->type != SYMBOL_GLOBAL) {
            errortag_t *err = error_makef(ERROR_USER, src_pos_invalid,
                                       "Symbol \"%s\" already defined outside global scope", name);
            ptrarray_add(ape->errors, err);
            return false;
        }
    } else {
        symbol = symbol_table_define(symbol_table, name, false);
    }
    vm_set_global(ape->vm, symbol->index, ape_object_to_object(obj));
    return true;
}

ape_object_t ape_get_object(ape_t *ape, const char *name) {
    symbol_table_t *st = compiler_get_symbol_table(ape->compiler);
    symbol_t *symbol = symbol_table_resolve(st, name);
    if (!symbol) {
        errortag_t *err = error_makef(ERROR_USER, src_pos_invalid,
                                   "Symbol \"%s\" is not defined", name);
        ptrarray_add(ape->errors, err);
        return ape_object_make_null();
    }
    object_t res = object_make_null();
    if (symbol->type == SYMBOL_GLOBAL) {
        res = vm_get_global(ape->vm, symbol->index);
    } else if (symbol->type == SYMBOL_NATIVE_FUNCTION) {
        object_t *res_ptr = array_get(ape->vm->native_functions, symbol->index);
        res = *res_ptr;
    } else {
        errortag_t *err = error_makef(ERROR_USER, src_pos_invalid,
                                   "Value associated with symbol \"%s\" could not be loaded", name);
        ptrarray_add(ape->errors, err);
        return ape_object_make_null();
    }
    return object_to_ape_object(res);
}

bool ape_check_args(ape_t *ape, bool generate_error, int argc, ape_object_t *args, int expected_argc, int *expected_types) {
    if (argc != expected_argc) {
        if (generate_error) {
            ape_set_runtime_errorf(ape, "Invalid number or arguments, got %d instead of %d", argc, expected_argc);
        }
        return false;
    }
    
    for(int i = 0; i < argc; i++) {
        ape_object_t arg = args[i];
        ape_object_type_t type = ape_object_get_type(arg);
        ape_object_type_t expected_type = expected_types[i];
        if (!(type & expected_type)) {
            if (generate_error) {
                const char *type_str = ape_object_get_type_name(type);
                const char *expected_type_str = ape_object_get_type_name(expected_type);
                ape_set_runtime_errorf(ape, "Invalid argument type, got %s, expected %s", type_str, expected_type_str);
            }
            return false;
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
// Ape object
//-----------------------------------------------------------------------------

ape_object_t ape_object_make_number(double val) {
    return object_to_ape_object(object_make_number(val));
}

ape_object_t ape_object_make_bool(bool val) {
    return object_to_ape_object(object_make_bool(val));
}

ape_object_t ape_object_make_string(ape_t *ape, const char *str) {
    return object_to_ape_object(object_make_string(ape->mem, str));
}

ape_object_t ape_object_make_stringf(ape_t *ape, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int to_write = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    va_start(args, fmt);
    char *res = (char*)ape_malloc(to_write + 1);
    int written = vsprintf(res, fmt, args);
    (void)written;
    APE_ASSERT(written == to_write);
    va_end(args);
    return object_to_ape_object(object_make_string_no_copy(ape->mem, res));
}

ape_object_t ape_object_make_null() {
    return object_to_ape_object(object_make_null());
}

ape_object_t ape_object_make_array(ape_t *ape) {
    return object_to_ape_object(object_make_array(ape->mem));
}

ape_object_t ape_object_make_map(ape_t *ape) {
    return object_to_ape_object(object_make_map(ape->mem));
}

ape_object_t ape_object_make_native_function(ape_t *ape, ape_native_fn fn, void *data) {
    native_fn_wrapper_t *wrapper = ape_malloc(sizeof(native_fn_wrapper_t));
    memset(wrapper, 0, sizeof(native_fn_wrapper_t));
    wrapper->fn = fn;
    wrapper->ape = ape;
    wrapper->data = data;
    object_t wrapper_native_function = object_make_native_function(ape->mem, "", ape_native_fn_wrapper, wrapper);
    ptrarray_add(ape->native_fn_wrappers, wrapper);
    return object_to_ape_object(wrapper_native_function);
}

ape_object_t ape_object_make_error(ape_t *ape, const char *msg) {
    return object_to_ape_object(object_make_error(ape->mem, msg));
}

ape_object_t ape_object_make_errorf(ape_t *ape, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int to_write = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    va_start(args, fmt);
    char *res = (char*)ape_malloc(to_write + 1);
    int written = vsprintf(res, fmt, args);
    (void)written;
    APE_ASSERT(written == to_write);
    va_end(args);
    return object_to_ape_object(object_make_error_no_copy(ape->mem, res));
}

ape_object_t ape_object_make_external(ape_t *ape, void *data) {
    object_t res = object_make_external(ape->mem, data);
    return object_to_ape_object(res);
}

char *ape_object_serialize(ape_object_t obj) {
    return object_serialize(ape_object_to_object(obj));
}

void ape_object_disable_gc(ape_object_t ape_obj) {
    object_t obj = ape_object_to_object(ape_obj);
    gc_disable_on_object(obj);
}

void ape_object_enable_gc(ape_object_t ape_obj) {
    object_t obj = ape_object_to_object(ape_obj);
    gc_enable_on_object(obj);
}

bool ape_object_equals(ape_object_t ape_a, ape_object_t ape_b){
    object_t a = ape_object_to_object(ape_a);
    object_t b = ape_object_to_object(ape_b);
    return object_equals(a, b);
}

ape_object_t ape_object_copy(ape_object_t ape_obj) {
    object_t obj = ape_object_to_object(ape_obj);
    gcmem_t *mem = object_get_mem(obj);
    object_t res = object_copy(mem, obj);
    return object_to_ape_object(res);
}

ape_object_t ape_object_deep_copy(ape_object_t ape_obj) {
    object_t obj = ape_object_to_object(ape_obj);
    gcmem_t *mem = object_get_mem(obj);
    object_t res = object_deep_copy(mem, obj);
    return object_to_ape_object(res);
}

void ape_set_runtime_error(ape_t *ape, const char *message) {
    errortag_t *err = error_make(ERROR_RUNTIME, src_pos_invalid, message);
    ptrarray_add(ape->errors, err);
}

void ape_set_runtime_errorf(ape_t *ape, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int to_write = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    va_start(args, fmt);
    char *message = (char*)ape_malloc(to_write + 1);
    vsprintf(message, fmt, args);
    va_end(args);

    errortag_t *err = error_make_no_copy(ERROR_RUNTIME, src_pos_invalid, message);

    ptrarray_add(ape->errors, err);
}

ape_object_type_t ape_object_get_type(ape_object_t ape_obj) {
    object_t obj = ape_object_to_object(ape_obj);
    switch (object_get_type(obj)) {
        case OBJECT_NONE:            return APE_OBJECT_NONE;
        case OBJECT_ERROR:           return APE_OBJECT_ERROR;
        case OBJECT_NUMBER:          return APE_OBJECT_NUMBER;
        case OBJECT_BOOL:            return APE_OBJECT_BOOL;
        case OBJECT_STRING:          return APE_OBJECT_STRING;
        case OBJECT_NULL:            return APE_OBJECT_NULL;
        case OBJECT_NATIVE_FUNCTION: return APE_OBJECT_NATIVE_FUNCTION;
        case OBJECT_ARRAY:           return APE_OBJECT_ARRAY;
        case OBJECT_MAP:             return APE_OBJECT_MAP;
        case OBJECT_FUNCTION:        return APE_OBJECT_FUNCTION;
        case OBJECT_EXTERNAL:        return APE_OBJECT_EXTERNAL;
        case OBJECT_FREED:           return APE_OBJECT_FREED;
        case OBJECT_ANY:             return APE_OBJECT_ANY;
        default:                     return APE_OBJECT_NONE;
    }
}

const char* ape_object_get_type_string(ape_object_t obj) {
    return ape_object_get_type_name(ape_object_get_type(obj));
}

const char* ape_object_get_type_name(ape_object_type_t type) {
    switch (type) {
        case APE_OBJECT_NONE:            return "NONE";
        case APE_OBJECT_ERROR:           return "ERROR";
        case APE_OBJECT_NUMBER:          return "NUMBER";
        case APE_OBJECT_BOOL:            return "BOOL";
        case APE_OBJECT_STRING:          return "STRING";
        case APE_OBJECT_NULL:            return "NULL";
        case APE_OBJECT_NATIVE_FUNCTION: return "NATIVE_FUNCTION";
        case APE_OBJECT_ARRAY:           return "ARRAY";
        case APE_OBJECT_MAP:             return "MAP";
        case APE_OBJECT_FUNCTION:        return "FUNCTION";
        case APE_OBJECT_EXTERNAL:        return "EXTERNAL";
        case APE_OBJECT_FREED:           return "FREED";
        case APE_OBJECT_ANY:             return "ANY";
        default:                         return "NONE";
    }
}

double ape_object_get_number(ape_object_t obj) {
    return object_get_number(ape_object_to_object(obj));
}

bool ape_object_get_bool(ape_object_t obj) {
    return object_get_bool(ape_object_to_object(obj));
}

const char *ape_object_get_string(ape_object_t obj) {
    return object_get_string(ape_object_to_object(obj));
}

const char *ape_object_get_error_message(ape_object_t obj) {
    return object_get_error_message(ape_object_to_object(obj));
}

const ape_traceback_t* ape_object_get_error_traceback(ape_object_t ape_obj) {
    object_t obj = ape_object_to_object(ape_obj);
    return (const ape_traceback_t*)object_get_error_traceback(obj);
}

bool ape_object_set_external_destroy_function(ape_object_t object, ape_data_destroy_fn destroy_fn) {
    return object_set_external_destroy_function(ape_object_to_object(object), (external_data_destroy_fn)destroy_fn);
}

bool ape_object_set_external_copy_function(ape_object_t object, ape_data_copy_fn copy_fn) {
    return object_set_external_copy_function(ape_object_to_object(object), (external_data_copy_fn)copy_fn);
}

//-----------------------------------------------------------------------------
// Ape object array
//-----------------------------------------------------------------------------

int ape_object_get_array_length(ape_object_t obj) {
    return object_get_array_length(ape_object_to_object(obj));
}

ape_object_t ape_object_get_array_value(ape_object_t obj, int ix) {
    object_t res = object_get_array_value_at(ape_object_to_object(obj), ix);
    return object_to_ape_object(res);
}

const char* ape_object_get_array_string(ape_object_t obj, int ix) {
    ape_object_t object = ape_object_get_array_value(obj, ix);
    if (ape_object_get_type(object) != APE_OBJECT_STRING) {
        return NULL;
    }
    return ape_object_get_string(object);
}

double ape_object_get_array_number(ape_object_t obj, int ix) {
    ape_object_t object = ape_object_get_array_value(obj, ix);
    if (ape_object_get_type(object) != APE_OBJECT_NUMBER) {
        return 0;
    }
    return ape_object_get_number(object);
}

bool ape_object_get_array_bool(ape_object_t obj, int ix) {
    ape_object_t object = ape_object_get_array_value(obj, ix);
    if (ape_object_get_type(object) != APE_OBJECT_BOOL) {
        return 0;
    }
    return ape_object_get_bool(object);
}

bool ape_object_set_array_value(ape_object_t ape_obj, int ix, ape_object_t ape_value) {
    object_t obj = ape_object_to_object(ape_obj);
    object_t value = ape_object_to_object(ape_value);
    return object_set_array_value_at(obj, ix, value);
}

bool ape_object_set_array_string(ape_object_t obj, int ix, const char *string) {
    gcmem_t *mem = object_get_mem(ape_object_to_object(obj));
    if (!mem) {
        return false;
    }
    object_t new_value = object_make_string(mem, string);
    return ape_object_set_array_value(obj, ix, object_to_ape_object(new_value));
}

bool ape_object_set_array_number(ape_object_t obj, int ix, double number) {
    object_t new_value = object_make_number(number);
    return ape_object_set_array_value(obj, ix, object_to_ape_object(new_value));
}

bool ape_object_set_array_bool(ape_object_t obj, int ix, bool value) {
    object_t new_value = object_make_bool(value);
    return ape_object_set_array_value(obj, ix, object_to_ape_object(new_value));
}

bool ape_object_add_array_value(ape_object_t ape_obj, ape_object_t ape_value) {
    object_t obj = ape_object_to_object(ape_obj);
    object_t value = ape_object_to_object(ape_value);
    return object_add_array_value(obj, value);
}

bool ape_object_add_array_string(ape_object_t obj, const char *string) {
    gcmem_t *mem = object_get_mem(ape_object_to_object(obj));
    if (!mem) {
        return false;
    }
    object_t new_value = object_make_string(mem, string);
    return ape_object_add_array_value(obj, object_to_ape_object(new_value));
}

bool ape_object_add_array_number(ape_object_t obj, double number) {
    object_t new_value = object_make_number(number);
    return ape_object_add_array_value(obj, object_to_ape_object(new_value));
}

bool ape_object_add_array_bool(ape_object_t obj, bool value) {
    object_t new_value = object_make_bool(value);
    return ape_object_add_array_value(obj, object_to_ape_object(new_value));
}

//-----------------------------------------------------------------------------
// Ape object map
//-----------------------------------------------------------------------------

int ape_object_get_map_length(ape_object_t obj) {
    return object_get_map_length(ape_object_to_object(obj));
}

ape_object_t ape_object_get_map_key_at(ape_object_t ape_obj, int ix) {
    object_t obj = ape_object_to_object(ape_obj);
    return object_to_ape_object(object_get_map_key_at(obj, ix));
}

ape_object_t ape_object_get_map_value_at(ape_object_t ape_obj, int ix) {
    object_t obj = ape_object_to_object(ape_obj);
    object_t res = object_get_map_value_at(obj, ix);
    return object_to_ape_object(res);
}

bool ape_object_set_map_value_at(ape_object_t ape_obj, int ix, ape_object_t ape_val) {
    object_t obj = ape_object_to_object(ape_obj);
    object_t val = ape_object_to_object(ape_val);
    return object_set_map_value_at(obj, ix, val);
}

bool ape_object_set_map_value_with_value_key(ape_object_t obj, ape_object_t key, ape_object_t val) {
    return object_set_map_value(ape_object_to_object(obj), ape_object_to_object(key), ape_object_to_object(val));
}

bool ape_object_set_map_value(ape_object_t obj, const char *key, ape_object_t value) {
    gcmem_t *mem = object_get_mem(ape_object_to_object(obj));
    object_t key_object = object_make_string(mem, key);
    return ape_object_set_map_value_with_value_key(obj, object_to_ape_object(key_object), value);
}

bool ape_object_set_map_string(ape_object_t obj, const char *key, const char *string) {
    gcmem_t *mem = object_get_mem(ape_object_to_object(obj));
    object_t string_object = object_make_string(mem, string);
    return ape_object_set_map_value(obj, key, object_to_ape_object(string_object));
}

bool ape_object_set_map_number(ape_object_t obj, const char *key, double number) {
    object_t number_object = object_make_number(number);
    return ape_object_set_map_value(obj, key, object_to_ape_object(number_object));
}

bool ape_object_set_map_bool(ape_object_t obj, const char *key, bool value) {
    object_t bool_object = object_make_bool(value);
    return ape_object_set_map_value(obj, key, object_to_ape_object(bool_object));
}

ape_object_t ape_object_get_map_value_with_value_key(ape_object_t obj, ape_object_t key) {
    return object_to_ape_object(object_get_map_value(ape_object_to_object(obj), ape_object_to_object(key)));
}

ape_object_t ape_object_get_map_value(ape_object_t object, const char *key) {
    gcmem_t *mem = object_get_mem(ape_object_to_object(object));
    if (!mem) {
        return ape_object_make_null();
    }
    object_t key_object = object_make_string(mem, key);
    ape_object_t res = ape_object_get_map_value_with_value_key(object, object_to_ape_object(key_object));
    return res;
}

const char* ape_object_get_map_string(ape_object_t object, const char *key) {
    ape_object_t res = ape_object_get_map_value(object, key);
    return ape_object_get_string(res);
}

double ape_object_get_map_number(ape_object_t object, const char *key) {
    ape_object_t res = ape_object_get_map_value(object, key);
    return ape_object_get_number(res);
}

bool ape_object_get_map_bool(ape_object_t object, const char *key) {
    ape_object_t res = ape_object_get_map_value(object, key);
    return ape_object_get_bool(res);
}

bool ape_object_map_has_key(ape_object_t ape_object, const char *key) {
    object_t object = ape_object_to_object(ape_object);
    gcmem_t *mem = object_get_mem(object);
    if (!mem) {
        return false;
    }
    object_t key_object = object_make_string(mem, key);
    return object_map_has_key(object, key_object);
}

//-----------------------------------------------------------------------------
// Ape error
//-----------------------------------------------------------------------------

const char* ape_error_get_message(const ape_error_t *ape_error) {
    const errortag_t *error = (const errortag_t*)ape_error;
    return error->message;
}

const char* ape_error_get_filepath(const ape_error_t *ape_error) {
    const errortag_t *error = (const errortag_t*)ape_error;
    if (!error->pos.file) {
        return NULL;
    }
    return error->pos.file->path;
}

const char* ape_error_get_line(const ape_error_t *ape_error) {
    const errortag_t *error = (const errortag_t*)ape_error;
    if (!error->pos.file) {
        return NULL;
    }
    ptrarray(char*) *lines = error->pos.file->lines;
    if (error->pos.line >= ptrarray_count(lines)) {
        return NULL;
    }
    const char *line = ptrarray_get(lines, error->pos.line);
    return line;
}

int ape_error_get_line_number(const ape_error_t *ape_error) {
    const errortag_t *error = (const errortag_t*)ape_error;
    return error->pos.line + 1;
}

int ape_error_get_column_number(const ape_error_t *ape_error) {
    const errortag_t *error = (const errortag_t*)ape_error;
    return error->pos.column + 1;
}

ape_error_type_t ape_error_get_type(const ape_error_t *ape_error) {
    const errortag_t *error = (const errortag_t*)ape_error;
    switch (error->type) {
        case ERROR_NONE: return APE_ERROR_NONE;
        case ERROR_PARSING: return APE_ERROR_PARSING;
        case ERROR_COMPILATION: return APE_ERROR_COMPILATION;
        case ERROR_RUNTIME: return APE_ERROR_RUNTIME;
        case ERROR_USER: return APE_ERROR_USER;
        default: return APE_ERROR_NONE;
    }
}

const char* ape_error_get_type_string(const ape_error_t *error) {
    return ape_error_type_to_string(ape_error_get_type(error));
}

const char* ape_error_type_to_string(ape_error_type_t type) {
    switch (type) {
        case APE_ERROR_PARSING: return "PARSING";
        case APE_ERROR_COMPILATION: return "COMPILATION";
        case APE_ERROR_RUNTIME: return "RUNTIME";
        case APE_ERROR_USER: return "USER";
        default: return "NONE";
    }
}

char* ape_error_serialize(const ape_error_t *err) {
    const char *type_str = ape_error_get_type_string(err);
    const char *filename = ape_error_get_filepath(err);
    const char *line = ape_error_get_line(err);
    int line_num = ape_error_get_line_number(err);
    int col_num = ape_error_get_column_number(err);
    strbuf_t *buf = strbuf_make();
    if (line) {
        strbuf_append(buf, line);
        strbuf_append(buf, "\n");
        if (col_num >= 0) {
            for (int j = 0; j < (col_num - 1); j++) {
                strbuf_append(buf, " ");
            }
            strbuf_append(buf, "^\n");
        }
    }
    strbuf_appendf(buf, "%s ERROR in \"%s\" on %d:%d: %s\n", type_str,
           filename, line_num, col_num, ape_error_get_message(err));
    const ape_traceback_t *traceback = ape_error_get_traceback(err);
    if (traceback) {
        strbuf_appendf(buf, "Traceback:\n");
        traceback_to_string((const traceback_t*)ape_error_get_traceback(err), buf);
    }
    return strbuf_get_string_and_destroy(buf);
}

const ape_traceback_t* ape_error_get_traceback(const ape_error_t *ape_error) {
    const errortag_t *error = (const errortag_t*)ape_error;
    return (const ape_traceback_t*)error->traceback;
}

//-----------------------------------------------------------------------------
// Ape traceback
//-----------------------------------------------------------------------------

int ape_traceback_get_depth(const ape_traceback_t *ape_traceback) {
    const traceback_t *traceback = (const traceback_t*)ape_traceback;
    return array_count(traceback->items);
}

const char* ape_traceback_get_filepath(const ape_traceback_t *ape_traceback, int depth) {
    const traceback_t *traceback = (const traceback_t*)ape_traceback;
    traceback_item_t *item = array_get(traceback->items, depth);
    if (!item) {
        return NULL;
    }
    return traceback_item_get_filepath(item);
}

const char* ape_traceback_get_line(const ape_traceback_t *ape_traceback, int depth) {
    const traceback_t *traceback = (const traceback_t*)ape_traceback;
    traceback_item_t *item = array_get(traceback->items, depth);
    if (!item) {
        return NULL;
    }
    return traceback_item_get_line(item);
}

int ape_traceback_get_line_number(const ape_traceback_t *ape_traceback, int depth) {
    const traceback_t *traceback = (const traceback_t*)ape_traceback;
    traceback_item_t *item = array_get(traceback->items, depth);
    if (!item) {
        return -1;
    }
    return item->pos.line;
}

int ape_traceback_get_column_number(const ape_traceback_t *ape_traceback, int depth) {
    const traceback_t *traceback = (const traceback_t*)ape_traceback;
    traceback_item_t *item = array_get(traceback->items, depth);
    if (!item) {
        return -1;
    }
    return item->pos.column;
}

const char* ape_traceback_get_function_name(const ape_traceback_t *ape_traceback, int depth) {
    const traceback_t *traceback = (const traceback_t*)ape_traceback;
    traceback_item_t *item = array_get(traceback->items, depth);
    if (!item) {
        return "";
    }
    return item->function_name;
}

//-----------------------------------------------------------------------------
// Ape internal
//-----------------------------------------------------------------------------

static object_t ape_native_fn_wrapper(vm_t *vm, void *data, int argc, object_t *args) {
    native_fn_wrapper_t *wrapper = (native_fn_wrapper_t*)data;
    APE_ASSERT(vm == wrapper->ape->vm);
    ape_object_t res = wrapper->fn(wrapper->ape, wrapper->data, argc, (ape_object_t*)args);
    if (ape_has_errors(wrapper->ape)) {
        return object_make_null();
    }
    return ape_object_to_object(res);
}

static object_t ape_object_to_object(ape_object_t obj) {
    return (object_t){ .handle = obj._internal };
}

static ape_object_t object_to_ape_object(object_t obj) {
    return (ape_object_t){ ._internal = obj.handle };
}

static void reset_state(ape_t *ape) {
    ptrarray_clear_and_destroy_items(ape->errors, error_destroy);
    vm_reset(ape->vm);
}

static void set_default_config(ape_t *ape) {
    memset(&ape->config, 0, sizeof(ape_config_t));
    ape_set_repl_mode(ape, false);
    ape_set_file_read_function(ape, read_file_default, NULL);
    ape_set_file_write_function(ape, write_file_default, NULL);
    ape_set_stdout_write_function(ape, stdout_write_default, NULL);
    ape_set_gc_interval(ape, 10000);
}

static char* read_file_default(void *ctx, const char *filename){
    FILE *fp = fopen(filename, "r");
    size_t size_to_read = 0;
    size_t size_read = 0;
    long pos;
    char *file_contents;
    if (!fp) {
        return NULL;
    }
    fseek(fp, 0L, SEEK_END);
    pos = ftell(fp);
    if (pos < 0) {
        fclose(fp);
        return NULL;
    }
    size_to_read = pos;
    rewind(fp);
    file_contents = (char*)ape_malloc(sizeof(char) * (size_to_read + 1));
    if (!file_contents) {
        fclose(fp);
        return NULL;
    }
    size_read = fread(file_contents, 1, size_to_read, fp);
    if (size_read == 0 || ferror(fp)) {
        fclose(fp);
        free(file_contents);
        return NULL;
    }
    fclose(fp);
    file_contents[size_read] = '\0';
    return file_contents;
}

static size_t write_file_default(void* context, const char *path, const char *string, size_t string_size) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        return 0;
    }
    size_t written = fwrite(string, 1, string_size, fp);
    fclose(fp);
    return written;
}

static size_t stdout_write_default(void* context, const void *data, size_t size) {
    return fwrite(data, 1, size, stdout);
}
