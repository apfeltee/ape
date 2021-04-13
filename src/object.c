#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <float.h>
#include <math.h>

#ifndef APE_AMALGAMATED
#include "object.h"
#include "code.h"
#include "compiler.h"
#include "traceback.h"
#include "gc.h"
#endif

#define OBJECT_PATTERN          0xfff8000000000000
#define OBJECT_HEADER_MASK      0xffff000000000000
#define OBJECT_ALLOCATED_HEADER 0xfffc000000000000
#define OBJECT_BOOL_HEADER      0xfff9000000000000
#define OBJECT_NULL_PATTERN     0xfffa000000000000

static object_t object_make(object_type_t type, object_data_t *data);
static object_t object_deep_copy_internal(gcmem_t *mem, object_t obj, valdict(object_t, object_t) *copies);
static bool object_equals_wrapped(const object_t *a, const object_t *b);
static unsigned long object_hash(object_t *obj_ptr);
static unsigned long object_hash_string(const char *str);
static unsigned long object_hash_double(double val);
static bool object_is_number(object_t obj);
static uint64_t get_type_tag(object_type_t type);
static bool freevals_are_allocated(function_t *fun);

object_t object_make_number(double val) {
    object_t o = { .number = val };
    if ((o.handle & OBJECT_PATTERN) == OBJECT_PATTERN) {
        o.handle = 0x7ff8000000000000;
    }
    return o;
}

object_t object_make_bool(bool val) {
    return (object_t) { .handle = OBJECT_BOOL_HEADER | val };
}

object_t object_make_null() {
    return (object_t) { .handle = OBJECT_NULL_PATTERN };
}

object_t object_make_string(gcmem_t *mem, const char *string) {
    object_data_t *obj = gcmem_alloc_object_data(mem, OBJECT_STRING);
    int len = (int)strlen(string);
    if ((len + 1) < OBJECT_STRING_BUF_SIZE) {
        memcpy(obj->string.value_buf, string, len + 1);
        obj->string.is_allocated = false;
    } else {
        obj->string.value_allocated = ape_strdup(string);
        obj->string.is_allocated = true;
    }
    obj->string.hash = object_hash_string(string);
    return object_make(OBJECT_STRING, obj);
}

object_t object_make_string_no_copy(gcmem_t *mem, char *string) {
    object_data_t *obj = gcmem_alloc_object_data(mem, OBJECT_STRING);
    int len = (int)strlen(string);
    if ((len + 1) < OBJECT_STRING_BUF_SIZE) {
        memcpy(obj->string.value_buf, string, len + 1);
        obj->string.is_allocated = false;
        obj->string.hash = object_hash_string(string);
        ape_free(string);
        string = NULL;
    } else {
        obj->string.value_allocated = string;
        obj->string.is_allocated = true;
        obj->string.hash = object_hash_string(string);
    }
    return object_make(OBJECT_STRING, obj);
}

object_t object_make_stringf(gcmem_t *mem, const char *fmt, ...) {
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
    return object_make_string_no_copy(mem, res);
}

object_t object_make_native_function(gcmem_t *mem, const char *name, native_fn fn, void *data) {
    object_data_t *obj = gcmem_alloc_object_data(mem, OBJECT_NATIVE_FUNCTION);
    obj->native_function.name = ape_strdup(name);
    obj->native_function.fn = fn;
    obj->native_function.data = data;
    return object_make(OBJECT_NATIVE_FUNCTION, obj);
}

object_t object_make_array(gcmem_t *mem) {
    return object_make_array_with_capacity(mem, 8);
}

object_t object_make_array_with_capacity(gcmem_t *mem, unsigned capacity) {
    array(object_t) *arr = array_make_with_capacity(capacity, sizeof(object_t));
    return object_make_array_with_array(mem, arr);
}

object_t object_make_array_with_array(gcmem_t *mem, array(object_t) *array) {
    object_data_t *data = gcmem_alloc_object_data(mem, OBJECT_ARRAY);
    data->array = array;
    return object_make(OBJECT_ARRAY, data);
}

object_t object_make_map(gcmem_t *mem) {
    return object_make_map_with_capacity(mem, 32);
}

object_t object_make_map_with_capacity(gcmem_t *mem, unsigned capacity) {
    object_data_t *data = gcmem_alloc_object_data(mem, OBJECT_MAP);
    data->map = valdict_make_with_capacity(capacity, sizeof(object_t), sizeof(object_t));
    valdict_set_hash_function(data->map, (collections_hash_fn)object_hash);
    valdict_set_equals_function(data->map, (collections_equals_fn)object_equals_wrapped);
    return object_make(OBJECT_MAP, data);
}

object_t object_make_error(gcmem_t *mem, const char *error) {
    return object_make_error_no_copy(mem, ape_strdup(error));
}

object_t object_make_error_no_copy(gcmem_t *mem, char *error) {
    object_data_t *data = gcmem_alloc_object_data(mem, OBJECT_ERROR);
    data->error.message = error;
    data->error.traceback = NULL;
    return object_make(OBJECT_ERROR, data);
}

object_t object_make_errorf(gcmem_t *mem, const char *fmt, ...) {
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
    return object_make_error_no_copy(mem, res);
}

object_t object_make_function(gcmem_t *mem, const char *name, compilation_result_t *comp_res, bool owns_data,
                              int num_locals, int num_args,
                              int free_vals_count) {
    object_data_t *obj = gcmem_alloc_object_data(mem, OBJECT_FUNCTION);
    if (owns_data) {
        obj->function.name = name ? ape_strdup(name) : ape_strdup("anonymous");
    } else {
        obj->function.const_name = name ? name : "anonymous";
    }
    obj->function.comp_result = comp_res;
    obj->function.owns_data = owns_data;
    obj->function.num_locals = num_locals;
    obj->function.num_args = num_args;
    obj->function.free_vals_count = free_vals_count;
    if (free_vals_count >= APE_ARRAY_LEN(obj->function.free_vals_buf)) {
        obj->function.free_vals_allocated = ape_malloc(sizeof(object_t) * free_vals_count);
    }
    return object_make(OBJECT_FUNCTION, obj);
}

object_t object_make_external(gcmem_t *mem, void *data) {
    object_data_t *obj = gcmem_alloc_object_data(mem, OBJECT_EXTERNAL);
    obj->external.data = data;
    obj->external.data_destroy_fn = NULL;
    obj->external.data_copy_fn = NULL;
    return object_make(OBJECT_EXTERNAL, obj);
}

void object_deinit(object_t obj) {
    if (object_is_allocated(obj)) {
        object_data_t *data = object_get_allocated_data(obj);
        object_data_deinit(data);
    }
}

void object_data_deinit(object_data_t *data) {
    switch (data->type) {
        case OBJECT_FREED: {
            APE_ASSERT(false);
            return;
        }
        case OBJECT_STRING: {
            if (data->string.is_allocated) {
                ape_free(data->string.value_allocated);
            }
            break;
        }
        case OBJECT_FUNCTION: {
            if (data->function.owns_data) {
                ape_free(data->function.name);
                compilation_result_destroy(data->function.comp_result);
            }
            if (freevals_are_allocated(&data->function)) {
                ape_free(data->function.free_vals_allocated);
            }
            break;
        }
        case OBJECT_ARRAY: {
            array_destroy(data->array);
            break;
        }
        case OBJECT_MAP: {
            valdict_destroy(data->map);
            break;
        }
        case OBJECT_NATIVE_FUNCTION: {
            ape_free(data->native_function.name);
            break;
        }
        case OBJECT_EXTERNAL: {
            if (data->external.data_destroy_fn) {
                data->external.data_destroy_fn(data->external.data);
            }
            break;
        }
        case OBJECT_ERROR: {
            ape_free(data->error.message);
            traceback_destroy(data->error.traceback);
            break;
        }
        default: {
            break;
        }
    }
    data->type = OBJECT_FREED;
}

bool object_is_allocated(object_t object) {
    return (object.handle & OBJECT_ALLOCATED_HEADER) == OBJECT_ALLOCATED_HEADER;
}

gcmem_t* object_get_mem(object_t obj) {
    object_data_t *data = object_get_allocated_data(obj);
    return data->mem;
}

bool object_is_hashable(object_t obj) {
    object_type_t type = object_get_type(obj);
    switch (type) {
        case OBJECT_STRING: return true;
        case OBJECT_NUMBER: return true;
        case OBJECT_BOOL: return true;
        default: return false;
    }
}

void object_to_string(object_t obj, strbuf_t *buf, bool quote_str) {
    object_type_t type = object_get_type(obj);
    switch (type) {
        case OBJECT_FREED: {
            strbuf_append(buf, "FREED");
            break;
        }
        case OBJECT_NONE: {
            strbuf_append(buf, "NONE");
            break;
        }
        case OBJECT_NUMBER: {
            double number = object_get_number(obj);
            strbuf_appendf(buf, "%1.10g", number);
            break;
        }
        case OBJECT_BOOL: {
            strbuf_append(buf, object_get_bool(obj) ? "true" : "false");
            break;
        }
        case OBJECT_STRING: {
            const char *string = object_get_string(obj);
            if (quote_str) {
                strbuf_appendf(buf, "\"%s\"", string);
            } else {
                strbuf_append(buf, string);
            }
            break;
        }
        case OBJECT_NULL: {
            strbuf_append(buf, "null");
            break;
        }
        case OBJECT_FUNCTION: {
            const function_t *function = object_get_function(obj);
            strbuf_appendf(buf, "CompiledFunction: %s\n", object_get_function_name(obj));
            code_to_string(function->comp_result->bytecode, function->comp_result->src_positions, function->comp_result->count, buf);
            break;
        }
        case OBJECT_ARRAY: {
            strbuf_append(buf, "[");
            for (int i = 0; i < object_get_array_length(obj); i++) {
                object_t iobj = object_get_array_value_at(obj, i);
                object_to_string(iobj, buf, true);
                if (i < (object_get_array_length(obj) - 1)) {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, "]");
            break;
        }
        case OBJECT_MAP: {
            strbuf_append(buf, "{");
            for (int i = 0; i < object_get_map_length(obj); i++) {
                object_t key = object_get_map_key_at(obj, i);
                object_t val = object_get_map_value_at(obj, i);
                object_to_string(key, buf, true);
                strbuf_append(buf, ": ");
                object_to_string(val, buf, true);
                if (i < (object_get_map_length(obj) - 1)) {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, "}");
            break;
        }
        case OBJECT_NATIVE_FUNCTION: {
            strbuf_append(buf, "NATIVE_FUNCTION");
            break;
        }
        case OBJECT_EXTERNAL: {
            strbuf_append(buf, "EXTERNAL");
            break;
        }
        case OBJECT_ERROR: {
            strbuf_appendf(buf, "ERROR: %s\n", object_get_error_message(obj));
            traceback_t *traceback = object_get_error_traceback(obj);
            APE_ASSERT(traceback);
            if (traceback) {
                strbuf_append(buf, "Traceback:\n");
                traceback_to_string(traceback, buf);
            }
            break;
        }
        case OBJECT_ANY: {
            APE_ASSERT(false);
        }
    }
}

const char *object_get_type_name(const object_type_t type) {
    switch (type) {
        case OBJECT_NONE:            return "NONE";
        case OBJECT_FREED:           return "NONE";
        case OBJECT_NUMBER:          return "NUMBER";
        case OBJECT_BOOL:            return "BOOL";
        case OBJECT_STRING:          return "STRING";
        case OBJECT_NULL:            return "NULL";
        case OBJECT_NATIVE_FUNCTION: return "NATIVE_FUNCTION";
        case OBJECT_ARRAY:           return "ARRAY";
        case OBJECT_MAP:             return "MAP";
        case OBJECT_FUNCTION:        return "FUNCTION";
        case OBJECT_EXTERNAL:        return "EXTERNAL";
        case OBJECT_ERROR:           return "ERROR";
        case OBJECT_ANY:             return "ANY";
    }
    return "NONE";
}

char* object_serialize(object_t object) {
    strbuf_t *buf = strbuf_make();
    object_to_string(object, buf, true);
    char *string = strbuf_get_string_and_destroy(buf);
    return string;
}

object_t object_deep_copy(gcmem_t *mem, object_t obj) {
    valdict(object_t, object_t) *copies = valdict_make(object_t, object_t);
    object_t res = object_deep_copy_internal(mem, obj, copies);
    valdict_destroy(copies);
    return res;
}

object_t object_copy(gcmem_t *mem, object_t obj) {
    object_t copy = object_make_null();
    object_type_t type = object_get_type(obj);
    switch (type) {
        case OBJECT_ANY:
        case OBJECT_FREED:
        case OBJECT_NONE: {
            APE_ASSERT(false);
            copy = object_make_null();
            break;
        }
        case OBJECT_NUMBER:
        case OBJECT_BOOL:
        case OBJECT_NULL:
        case OBJECT_FUNCTION:
        case OBJECT_NATIVE_FUNCTION:
        case OBJECT_ERROR: {
            copy = obj;
            break;
        }
        case OBJECT_STRING: {
            const char *str = object_get_string(obj);
            copy = object_make_string(mem, str);
            break;
        }
        case OBJECT_ARRAY: {
            array(object_t) *array = object_get_array(obj);
            array(object_t) *res_array = array_make(object_t);
            for (int i = 0; i < array_count(array); i++) {
                object_t *array_obj = array_get(array, i);
                array_add(res_array, array_obj);
            }
            copy = object_make_array_with_array(mem, res_array);
            break;
        }
        case OBJECT_MAP: {
            copy = object_make_map(mem);
            for (int i = 0; i < object_get_map_length(obj); i++) {
                object_t key = object_get_map_key_at(obj, i);
                object_t val = object_get_map_value_at(obj, i);
                object_set_map_value(copy, key, val);
            }
            break;
        }
        case OBJECT_EXTERNAL: {
            external_data_t *external = object_get_external_data(obj);
            void *data_copy = NULL;
            if (external->data_copy_fn) {
                data_copy = external->data_copy_fn(external->data);
            } else {
                data_copy = external->data;
            }
            copy = object_make_external(mem, data_copy);
            object_set_external_destroy_function(copy, external->data_destroy_fn);
            object_set_external_copy_function(copy, external->data_copy_fn);
            break;
        }
    }
    return copy;
}

double object_compare(object_t a, object_t b) {
    if (a.handle == b.handle) {
        return 0;
    }
    
    object_type_t a_type = object_get_type(a);
    object_type_t b_type = object_get_type(b);

    if ((a_type == OBJECT_NUMBER || a_type == OBJECT_BOOL || a_type == OBJECT_NULL)
        && (b_type == OBJECT_NUMBER || b_type == OBJECT_BOOL || b_type == OBJECT_NULL)) {
        double left_val = object_get_number(a);
        double right_val = object_get_number(b);
        return left_val - right_val;
    } else if (a_type == b_type && a_type == OBJECT_STRING) {
        const char *left_string = object_get_string(a);
        const char *right_string = object_get_string(b);
        return strcmp(left_string, right_string);
    } else {
        intptr_t a_data_val = (intptr_t)object_get_allocated_data(a);
        intptr_t b_data_val = (intptr_t)object_get_allocated_data(b);
        return (double)(a_data_val - b_data_val);
    }
}

bool object_equals(object_t a, object_t b) {
    object_type_t a_type = object_get_type(a);
    object_type_t b_type = object_get_type(b);

    if (a_type != b_type) {
        return false;
    }
    double res = object_compare(a, b);
    return APE_DBLEQ(res, 0);
}

external_data_t* object_get_external_data(object_t object) {
    APE_ASSERT(object_get_type(object) == OBJECT_EXTERNAL);
    object_data_t *data = object_get_allocated_data(object);
    return &data->external;
}

bool object_set_external_destroy_function(object_t object, external_data_destroy_fn destroy_fn) {
    APE_ASSERT(object_get_type(object) == OBJECT_EXTERNAL);
    external_data_t* data = object_get_external_data(object);
    if (!data) {
        return false;
    }
    data->data_destroy_fn = destroy_fn;
    return true;
}

object_data_t* object_get_allocated_data(object_t object) {
    APE_ASSERT(object_is_allocated(object) || object_get_type(object) == OBJECT_NULL);
    return (object_data_t*)(object.handle & ~OBJECT_HEADER_MASK);
}

bool object_get_bool(object_t obj) {
    if (object_is_number(obj)) {
        return obj.handle;
    }
    return obj.handle & (~OBJECT_HEADER_MASK);
}

double object_get_number(object_t obj) {
    if (object_is_number(obj)) { // todo: optimise? always return number?
        return obj.number;
    }
    return obj.handle & (~OBJECT_HEADER_MASK);
}

const char * object_get_string(object_t object) {
    APE_ASSERT(object_get_type(object) == OBJECT_STRING);
    object_data_t *data = object_get_allocated_data(object);
    if (data->string.is_allocated) {
        return data->string.value_allocated;
    } else {
        return data->string.value_buf;
    }
}

function_t* object_get_function(object_t object) {
    APE_ASSERT(object_get_type(object) == OBJECT_FUNCTION);
    object_data_t *data = object_get_allocated_data(object);
    return &data->function;
}

native_function_t* object_get_native_function(object_t obj) {
    object_data_t *data = object_get_allocated_data(obj);
    return &data->native_function;
}

object_type_t object_get_type(object_t obj) {
    if (object_is_number(obj)) {
        return OBJECT_NUMBER;
    }
    uint64_t tag = (obj.handle >> 48) & 0x7;
    switch (tag) {
        case 0: return OBJECT_NONE;
        case 1: return OBJECT_BOOL;
        case 2: return OBJECT_NULL;
        case 4: {
            object_data_t *data = object_get_allocated_data(obj);
            return data->type;
        }
        default: return OBJECT_NONE;
    }
}

bool object_is_numeric(object_t obj) {
    object_type_t type = object_get_type(obj);
    return type == OBJECT_NUMBER || type == OBJECT_BOOL;
}

bool object_is_null(object_t obj) {
    return object_get_type(obj) == OBJECT_NULL;
}

bool object_is_callable(object_t obj) {
    object_type_t type = object_get_type(obj);
    return type == OBJECT_NATIVE_FUNCTION || type == OBJECT_FUNCTION;
}

const char* object_get_function_name(object_t obj) {
    APE_ASSERT(object_get_type(obj) == OBJECT_FUNCTION);
    object_data_t *data = object_get_allocated_data(obj);
    APE_ASSERT(data);
    if (!data) {
        return NULL;
    }

    if (data->function.owns_data) {
        return data->function.name;
    } else {
        return data->function.const_name;
    }
}

object_t object_get_function_free_val(object_t obj, int ix) {
    APE_ASSERT(object_get_type(obj) == OBJECT_FUNCTION);
    object_data_t *data = object_get_allocated_data(obj);
    APE_ASSERT(data);
    if (!data) {
        return object_make_null();
    }
    function_t *fun = &data->function;
    APE_ASSERT(ix >= 0 && ix < fun->free_vals_count);
    if (ix < 0 || ix >= fun->free_vals_count) {
        return object_make_null();
    }
    if (freevals_are_allocated(fun)) {
        return fun->free_vals_allocated[ix];
    } else {
        return fun->free_vals_buf[ix];
    }
}

void object_set_function_free_val(object_t obj, int ix, object_t val) {
    APE_ASSERT(object_get_type(obj) == OBJECT_FUNCTION);
    object_data_t *data = object_get_allocated_data(obj);
    APE_ASSERT(data);
    if (!data) {
        return;
    }
    function_t *fun = &data->function;
    APE_ASSERT(ix >= 0 && ix < fun->free_vals_count);
    if (ix < 0 || ix >= fun->free_vals_count) {
        return;
    }
    if (freevals_are_allocated(fun)) {
        fun->free_vals_allocated[ix] = val;
    } else {
        fun->free_vals_buf[ix] = val;
    }
}

object_t* object_get_function_free_vals(object_t obj) {
    APE_ASSERT(object_get_type(obj) == OBJECT_FUNCTION);
    object_data_t *data = object_get_allocated_data(obj);
    APE_ASSERT(data);
    if (!data) {
        return NULL;
    }
    function_t *fun = &data->function;
    if (freevals_are_allocated(fun)) {
        return fun->free_vals_allocated;
    } else {
        return fun->free_vals_buf;
    }
}

const char* object_get_error_message(object_t object) {
    APE_ASSERT(object_get_type(object) == OBJECT_ERROR);
    object_data_t *data = object_get_allocated_data(object);
    return data->error.message;
}

void object_set_error_traceback(object_t object, traceback_t *traceback) {
    APE_ASSERT(object_get_type(object) == OBJECT_ERROR);
    object_data_t *data = object_get_allocated_data(object);
    APE_ASSERT(data->error.traceback == NULL);
    data->error.traceback = traceback;
}

traceback_t* object_get_error_traceback(object_t object) {
    APE_ASSERT(object_get_type(object) == OBJECT_ERROR);
    object_data_t *data = object_get_allocated_data(object);
    return data->error.traceback;
}

bool object_set_external_copy_function(object_t object, external_data_copy_fn copy_fn) {
    APE_ASSERT(object_get_type(object) == OBJECT_EXTERNAL);
    external_data_t* data = object_get_external_data(object);
    if (!data) {
        return false;
    }
    data->data_copy_fn = copy_fn;
    return true;
}

array(object_t)* object_get_array(object_t object) {
    APE_ASSERT(object_get_type(object) == OBJECT_ARRAY);
    object_data_t *data = object_get_allocated_data(object);
    return data->array;
}

object_t object_get_array_value_at(object_t object, int ix) {
    APE_ASSERT(object_get_type(object) == OBJECT_ARRAY);
    array(object_t)* array = object_get_array(object);
    if (ix < 0 || ix >= array_count(array)) {
        return object_make_null();
    }
    object_t *res = array_get(array, ix);
    if (!res) {
        return object_make_null();
    }
    return *res;
}

bool object_set_array_value_at(object_t object, int ix, object_t val) {
    APE_ASSERT(object_get_type(object) == OBJECT_ARRAY);
    array(object_t)* array = object_get_array(object);
    if (ix < 0 || ix >= array_count(array)) {
        return false;
    }
    return array_set(array, ix, &val);
}

bool object_add_array_value(object_t object, object_t val) {
    APE_ASSERT(object_get_type(object) == OBJECT_ARRAY);
    array(object_t)* array = object_get_array(object);
    return array_add(array, &val);
}

int object_get_array_length(object_t object) {
    APE_ASSERT(object_get_type(object) == OBJECT_ARRAY);
    array(object_t)* array = object_get_array(object);
    return array_count(array);
}

int object_get_map_length(object_t object) {
    APE_ASSERT(object_get_type(object) == OBJECT_MAP);
    object_data_t *data = object_get_allocated_data(object);
    return valdict_count(data->map);
}

object_t object_get_map_key_at(object_t object, int ix) {
    APE_ASSERT(object_get_type(object) == OBJECT_MAP);
    object_data_t *data = object_get_allocated_data(object);
    object_t *res = valdict_get_key_at(data->map, ix);
    if (!res) {
        return object_make_null();
    }
    return *res;
}

object_t object_get_map_value_at(object_t object, int ix) {
    APE_ASSERT(object_get_type(object) == OBJECT_MAP);
    object_data_t *data = object_get_allocated_data(object);
    object_t *res = valdict_get_value_at(data->map, ix);
    if (!res) {
        return object_make_null();
    }
    return *res;
}

bool object_set_map_value_at(object_t object, int ix, object_t val) {
    APE_ASSERT(object_get_type(object) == OBJECT_MAP);
    if (ix >= object_get_map_length(object)) {
        return false;
    }
    object_data_t *data = object_get_allocated_data(object);
    bool res = valdict_set_value_at(data->map, ix, &val);
    return res;
}

object_t object_get_kv_pair_at(gcmem_t *mem, object_t object, int ix) {
    APE_ASSERT(object_get_type(object) == OBJECT_MAP);
    object_data_t *data = object_get_allocated_data(object);
    if (ix >= valdict_count(data->map)) {
        return object_make_null();
    }
    object_t key = object_get_map_key_at(object, ix);
    object_t val = object_get_map_value_at(object, ix);
    object_t res = object_make_map(mem);
    object_set_map_value(res, object_make_string(mem, "key"), key);
    object_set_map_value(res, object_make_string(mem, "value"), val);
    return res;
}

bool object_set_map_value(object_t object, object_t key, object_t val) {
    APE_ASSERT(object_get_type(object) == OBJECT_MAP);
    object_data_t *data = object_get_allocated_data(object);
    return valdict_set(data->map, &key, &val);
}

object_t object_get_map_value(object_t object, object_t key) {
    APE_ASSERT(object_get_type(object) == OBJECT_MAP);
    object_data_t *data = object_get_allocated_data(object);
    object_t *res = valdict_get(data->map, &key);
    if (!res) {
        return object_make_null();
    }
    return *res;
}

bool object_map_has_key(object_t object, object_t key) {
    APE_ASSERT(object_get_type(object) == OBJECT_MAP);
    object_data_t *data = object_get_allocated_data(object);
    object_t *res = valdict_get(data->map, &key);
    return res != NULL;
}

// INTERNAL
static object_t object_make(object_type_t type, object_data_t *data) {
    object_t object;
    object.handle = OBJECT_PATTERN;
    uint64_t type_tag = get_type_tag(type) & 0x7;
    object.handle |= (type_tag << 48);
    object.handle |= (uintptr_t)data; // assumes no pointer exceeds 48 bits
    return object;
}

static object_t object_deep_copy_internal(gcmem_t *mem, object_t obj, valdict(object_t, object_t) *copies) {
    object_t *copy_ptr = valdict_get(copies, &obj);
    if (copy_ptr) {
        return *copy_ptr;
    }

    object_t copy = object_make_null()  ;

    object_type_t type = object_get_type(obj);
    switch (type) {
        case OBJECT_FREED:
        case OBJECT_ANY:
        case OBJECT_NONE: {
            APE_ASSERT(false);
            copy = object_make_null();
            break;
        }
        case OBJECT_NUMBER:
        case OBJECT_BOOL:
        case OBJECT_NULL:
        case OBJECT_NATIVE_FUNCTION: {
            copy = obj;
            break;
        }
        case OBJECT_STRING: {
            const char *str = object_get_string(obj);
            copy = object_make_string(mem, str);
            break;
        }
        case OBJECT_FUNCTION: {
            function_t *function = object_get_function(obj);
            uint8_t *bytecode_copy = ape_malloc(sizeof(uint8_t) * function->comp_result->count);
            memcpy(bytecode_copy, function->comp_result->bytecode, sizeof(uint8_t) * function->comp_result->count);
            src_pos_t *src_positions_copy = ape_malloc(sizeof(src_pos_t) * function->comp_result->count);
            memcpy(src_positions_copy, function->comp_result->src_positions, sizeof(src_pos_t) * function->comp_result->count);
            compilation_result_t *comp_res_copy = compilation_result_make(bytecode_copy, src_positions_copy, function->comp_result->count);
            copy = object_make_function(mem, object_get_function_name(obj), comp_res_copy, true,
                                        function->num_locals, function->num_args, 0);
            valdict_set(copies, &obj, &copy);
            function_t *function_copy = object_get_function(copy);
            if (freevals_are_allocated(function)) {
                function_copy->free_vals_allocated = ape_malloc(sizeof(object_t) * function->free_vals_count);
            }
            function_copy->free_vals_count = function->free_vals_count;
            for (int i = 0; i < function->free_vals_count; i++) {
                object_t free_val = object_get_function_free_val(obj, i);
                object_t free_val_copy = object_deep_copy_internal(mem, free_val, copies);
                object_set_function_free_val(copy, i, free_val_copy);
            }
            break;
        }
        case OBJECT_ARRAY: {
            array(object_t) *res_array = array_make(object_t);
            for (int i = 0; i < object_get_array_length(obj); i++) {
                object_t array_obj = object_get_array_value_at(obj, i);
                object_t copy = object_deep_copy_internal(mem, array_obj, copies);
                array_add(res_array, &copy);
            }
            copy = object_make_array_with_array(mem, res_array);
            valdict_set(copies, &obj, &copy);
            break;
        }
        case OBJECT_MAP: {
            copy = object_make_map(mem);
            valdict_set(copies, &obj, &copy);
            for (int i = 0; i < object_get_map_length(obj); i++) {
                object_t key = object_get_map_key_at(obj, i);
                object_t val = object_get_map_value_at(obj, i);
                object_t key_copy = object_deep_copy_internal(mem, key, copies);
                object_t val_copy = object_deep_copy_internal(mem, val, copies);
                object_set_map_value(copy, key_copy, val_copy);
            }
            break;
        }
        case OBJECT_EXTERNAL: {
            copy = object_copy(mem, obj);
            break;
        }
        case OBJECT_ERROR: {
            copy = obj;
            break;
        }
    }
    return copy;
}

static bool object_equals_wrapped(const object_t *a_ptr, const object_t *b_ptr) {
    object_t a = *a_ptr;
    object_t b = *b_ptr;
    return object_equals(a, b);
}

static unsigned long object_hash(object_t *obj_ptr) {
    object_t obj = *obj_ptr;
    object_type_t type = object_get_type(obj);

    switch (type) {
        case OBJECT_NUMBER: {
            double val = object_get_number(obj);
            return object_hash_double(val);
        }
        case OBJECT_BOOL: {
            bool val = object_get_bool(obj);
            return val;
        }
        case OBJECT_STRING: {
            object_data_t *data = object_get_allocated_data(obj);
            return data->string.hash;
        }
        default: {
            return 0;
        }
    }
}

static unsigned long object_hash_string(const char *str) { /* djb2 */
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

static unsigned long object_hash_double(double val) { /* djb2 */
    uint32_t *val_ptr = (uint32_t*)&val;
    unsigned long hash = 5381;
    hash = ((hash << 5) + hash) + val_ptr[0];
    hash = ((hash << 5) + hash) + val_ptr[1];
    return hash;
}

static bool object_is_number(object_t o) {
    return (o.handle & OBJECT_PATTERN) != OBJECT_PATTERN;
}

static uint64_t get_type_tag(object_type_t type) {
    switch (type) {
        case OBJECT_NONE: return 0;
        case OBJECT_BOOL: return 1;
        case OBJECT_NULL: return 2;
        default:          return 4;
    }
}

static bool freevals_are_allocated(function_t *fun) {
    return fun->free_vals_count >= APE_ARRAY_LEN(fun->free_vals_buf);
}
