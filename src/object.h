#ifndef object_h
#define object_h

#include <stdint.h>

#ifndef APE_AMALGAMATED
#include "common.h"
#include "collections.h"
#include "ast.h"
#endif

typedef struct compilation_result compilation_result_t;
typedef struct traceback traceback_t;
typedef struct vm vm_t;
typedef struct gcmem gcmem_t;

#define OBJECT_STRING_BUF_SIZE 32

typedef enum {
    OBJECT_NONE      = 0,
    OBJECT_ERROR     = 1 << 0,
    OBJECT_NUMBER    = 1 << 1,
    OBJECT_BOOL      = 1 << 2,
    OBJECT_STRING    = 1 << 3,
    OBJECT_NULL      = 1 << 4,
    OBJECT_NATIVE_FUNCTION   = 1 << 5,
    OBJECT_ARRAY     = 1 << 6,
    OBJECT_MAP       = 1 << 7,
    OBJECT_FUNCTION  = 1 << 8,
    OBJECT_EXTERNAL  = 1 << 9,
    OBJECT_FREED     = 1 << 10,
    OBJECT_ANY       = 0xffff,
} object_type_t;

typedef struct object {
    union {
        uint64_t handle;
        double number;
    };
} object_t;

typedef struct function {
    union {
        object_t *free_vals_allocated;
        object_t free_vals_buf[2];
    };
    union {
        char *name;
        const char *const_name;
    };
    compilation_result_t *comp_result;
    int num_locals;
    int num_args;
    int free_vals_count;
    bool owns_data;
} function_t;

typedef object_t (*native_fn)(vm_t *vm, void *data, int argc, object_t *args);

typedef struct native_function {
    char *name;
    native_fn fn;
    void *data;
} native_function_t;

typedef void  (*external_data_destroy_fn)(void* data);
typedef void* (*external_data_copy_fn)(void* data);

typedef struct external_data {
    void *data;
    external_data_destroy_fn data_destroy_fn;
    external_data_copy_fn    data_copy_fn;
} external_data_t;

typedef struct object_error {
    char *message;
    traceback_t *traceback;
} object_error_t;

typedef struct object_string {
    union {
        char *value_allocated;
        char value_buf[OBJECT_STRING_BUF_SIZE];
    };
    unsigned long hash;
    bool is_allocated;
} object_string_t;

typedef struct object_data {
    gcmem_t *mem;
    union {
        object_string_t string;
        object_error_t error;
        array(object_t) *array;
        valdict(object_t, object_t) *map;
        function_t function;
        native_function_t native_function;
        external_data_t external;
    };
    bool gcmark;
    object_type_t type;
} object_data_t;

APE_INTERNAL object_t object_make_number(double val);
APE_INTERNAL object_t object_make_bool(bool val);
APE_INTERNAL object_t object_make_null(void);
APE_INTERNAL object_t object_make_string(gcmem_t *mem, const char *string);
APE_INTERNAL object_t object_make_string_no_copy(gcmem_t *mem, char *string);
APE_INTERNAL object_t object_make_stringf(gcmem_t *mem, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
APE_INTERNAL object_t object_make_native_function(gcmem_t *mem, const char *name, native_fn fn, void *data);
APE_INTERNAL object_t object_make_array(gcmem_t *mem);
APE_INTERNAL object_t object_make_array_with_capacity(gcmem_t *mem, unsigned capacity);
APE_INTERNAL object_t object_make_array_with_array(gcmem_t *mem, array(object_t) *array);
APE_INTERNAL object_t object_make_map(gcmem_t *mem);
APE_INTERNAL object_t object_make_map_with_capacity(gcmem_t *mem, unsigned capacity);
APE_INTERNAL object_t object_make_error(gcmem_t *mem, const char *message);
APE_INTERNAL object_t object_make_error_no_copy(gcmem_t *mem, char *message);
APE_INTERNAL object_t object_make_errorf(gcmem_t *mem, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
APE_INTERNAL object_t object_make_function(gcmem_t *mem, const char *name, compilation_result_t *comp_res,
                                           bool owns_data, int num_locals, int num_args,
                                           int free_vals_count);
APE_INTERNAL object_t object_make_external(gcmem_t *mem, void *data);

APE_INTERNAL void object_deinit(object_t obj);
APE_INTERNAL void object_data_deinit(object_data_t *obj);

APE_INTERNAL bool        object_is_allocated(object_t obj);
APE_INTERNAL gcmem_t*    object_get_mem(object_t obj);
APE_INTERNAL bool        object_is_hashable(object_t obj);
APE_INTERNAL void        object_to_string(object_t obj, strbuf_t *buf, bool quote_str);
APE_INTERNAL const char* object_get_type_name(const object_type_t type);
APE_INTERNAL char*       object_serialize(object_t object);
APE_INTERNAL object_t    object_deep_copy(gcmem_t *mem, object_t object);
APE_INTERNAL object_t    object_copy(gcmem_t *mem, object_t obj);
APE_INTERNAL double      object_compare(object_t a, object_t b);
APE_INTERNAL bool        object_equals(object_t a, object_t b);

APE_INTERNAL object_data_t* object_get_allocated_data(object_t object);

APE_INTERNAL bool           object_get_bool(object_t obj);
APE_INTERNAL double         object_get_number(object_t obj);
APE_INTERNAL function_t*    object_get_function(object_t obj);
APE_INTERNAL const char*    object_get_string(object_t obj);
APE_INTERNAL native_function_t*     object_get_native_function(object_t obj);
APE_INTERNAL object_type_t  object_get_type(object_t obj);

APE_INTERNAL bool object_is_numeric(object_t obj);
APE_INTERNAL bool object_is_null(object_t obj);
APE_INTERNAL bool object_is_callable(object_t obj);

APE_INTERNAL const char* object_get_function_name(object_t obj);
APE_INTERNAL object_t    object_get_function_free_val(object_t obj, int ix);
APE_INTERNAL void        object_set_function_free_val(object_t obj, int ix, object_t val);
APE_INTERNAL object_t*   object_get_function_free_vals(object_t obj);

APE_INTERNAL const char*  object_get_error_message(object_t obj);
APE_INTERNAL void         object_set_error_traceback(object_t obj, traceback_t *traceback);
APE_INTERNAL traceback_t* object_get_error_traceback(object_t obj);

APE_INTERNAL external_data_t* object_get_external_data(object_t object);
APE_INTERNAL bool object_set_external_destroy_function(object_t object, external_data_destroy_fn destroy_fn);
APE_INTERNAL bool object_set_external_copy_function(object_t object, external_data_copy_fn copy_fn);

APE_INTERNAL array(object_t)* object_get_array(object_t array);
APE_INTERNAL object_t         object_get_array_value_at(object_t array, int ix);
APE_INTERNAL bool             object_set_array_value_at(object_t obj, int ix, object_t val);
APE_INTERNAL bool             object_add_array_value(object_t array, object_t val);
APE_INTERNAL int              object_get_array_length(object_t array);

APE_INTERNAL int      object_get_map_length(object_t obj);
APE_INTERNAL object_t object_get_map_key_at(object_t obj, int ix);
APE_INTERNAL object_t object_get_map_value_at(object_t obj, int ix);
APE_INTERNAL bool     object_set_map_value_at(object_t obj, int ix, object_t val);
APE_INTERNAL object_t object_get_kv_pair_at(gcmem_t *mem, object_t obj, int ix);
APE_INTERNAL bool     object_set_map_value(object_t obj, object_t key, object_t val);
APE_INTERNAL object_t object_get_map_value(object_t obj, object_t key);
APE_INTERNAL bool     object_map_has_key(object_t obj, object_t key);

#endif /* object_h */
