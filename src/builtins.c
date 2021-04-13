#include <stdlib.h>
#include <stdio.h>

#ifndef APE_AMALGAMATED
#include "builtins.h"

#include "common.h"
#include "object.h"
#include "vm.h"
#endif

static object_t len_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t first_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t last_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t rest_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t reverse_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t array_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t append_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t remove_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t remove_at_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t println_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t print_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t read_file_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t write_file_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t to_str_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t char_to_str_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t range_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t keys_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t values_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t copy_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t deep_copy_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t concat_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t error_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t crash_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t assert_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t random_fn(vm_t *vm, void *data, int argc, object_t *args);

// Type checks
static object_t is_string_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t is_array_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t is_map_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t is_number_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t is_bool_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t is_null_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t is_function_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t is_external_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t is_error_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t is_native_function_fn(vm_t *vm, void *data, int argc, object_t *args);

// Math
static object_t sqrt_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t pow_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t sin_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t cos_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t tan_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t log_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t ceil_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t floor_fn(vm_t *vm, void *data, int argc, object_t *args);
static object_t abs_fn(vm_t *vm, void *data, int argc, object_t *args);

static bool check_args(vm_t *vm, bool generate_error, int argc, object_t *args, int expected_argc, object_type_t *expected_types);
#define CHECK_ARGS(vm, generate_error, argc, args, ...) \
    check_args(\
        (vm),\
        (generate_error),\
        (argc),\
        (args),\
        sizeof((object_type_t[]){__VA_ARGS__}) / sizeof(object_type_t),\
        (object_type_t[]){__VA_ARGS__})

static struct {
    const char *name;
    native_fn fn;
} g_native_functions[] = {
    {"len",         len_fn},
    {"println",     println_fn},
    {"print",       print_fn},
    {"read_file",   read_file_fn},
    {"write_file",  write_file_fn},
    {"first",       first_fn},
    {"last",        last_fn},
    {"rest",        rest_fn},
    {"append",      append_fn},
    {"remove",      remove_fn},
    {"remove_at",   remove_at_fn},
    {"to_str",      to_str_fn},
    {"range",       range_fn},
    {"keys",        keys_fn},
    {"values",      values_fn},
    {"copy",        copy_fn},
    {"deep_copy",   deep_copy_fn},
    {"concat",      concat_fn},
    {"char_to_str", char_to_str_fn},
    {"reverse",     reverse_fn},
    {"array",       array_fn},
    {"error",       error_fn},
    {"crash",       crash_fn},
    {"assert",      assert_fn},
    {"random",      random_fn},

    // Type checks
    {"is_string",   is_string_fn},
    {"is_array",    is_array_fn},
    {"is_map",      is_map_fn},
    {"is_number",   is_number_fn},
    {"is_bool",     is_bool_fn},
    {"is_null",     is_null_fn},
    {"is_function", is_function_fn},
    {"is_external", is_external_fn},
    {"is_error",    is_error_fn},
    {"is_native_function", is_native_function_fn},

    // Math
    {"sqrt",  sqrt_fn},
    {"pow",   pow_fn},
    {"sin",   sin_fn},
    {"cos",   cos_fn},
    {"tan",   tan_fn},
    {"log",   log_fn},
    {"ceil",  ceil_fn},
    {"floor", floor_fn},
    {"abs",   abs_fn},
};

int builtins_count() {
    return APE_ARRAY_LEN(g_native_functions);
}

native_fn builtins_get_fn(int ix) {
    return g_native_functions[ix].fn;
}

const char* builtins_get_name(int ix) {
    return g_native_functions[ix].name;
}

// INTERNAL
static object_t len_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_STRING | OBJECT_ARRAY | OBJECT_MAP)) {
        return object_make_null();
    }

    object_t arg = args[0];
    object_type_t type = object_get_type(arg);
    if (type == OBJECT_STRING) {
        const char *str = object_get_string(arg);
        int len = (int)strlen(str);
        return object_make_number(len);
    } else if (type == OBJECT_ARRAY) {
        int len = object_get_array_length(arg);
        return object_make_number(len);
    } else if (type == OBJECT_MAP) {
        int len = object_get_map_length(arg);
        return object_make_number(len);
    }

    return object_make_null();
}

static object_t first_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ARRAY)) {
        return object_make_null();
    }
     object_t arg = args[0];
    return object_get_array_value_at(arg, 0);
}

static object_t last_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ARRAY)) {
        return object_make_null();
    }
    object_t arg = args[0];
    return object_get_array_value_at(arg, object_get_array_length(arg) - 1);
}

static object_t rest_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ARRAY)) {
        return object_make_null();
    }
    object_t arg = args[0];
    int len = object_get_array_length(arg);
    if (len == 0) {
        return object_make_null();
    }

    object_t res = object_make_array(vm->mem);
    for (int i = 1; i < len; i++) {
        object_t item = object_get_array_value_at(arg, i);
        object_add_array_value(res, item);
    }

    return res;
}

static object_t reverse_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ARRAY | OBJECT_STRING)) {
        return object_make_null();
    }
    object_t arg = args[0];
    object_type_t type = object_get_type(arg);
    if (type == OBJECT_ARRAY) {
        array(object_t) *array = object_get_array(arg);
        array_reverse(array);
        return object_make_array_with_array(vm->mem, array);
    } else if (type == OBJECT_STRING) {
        const char *str = object_get_string(arg);
        int len = (int)strlen(str);
        char *res_buf = ape_malloc(len + 1);
        for (int i = 0; i < len; i++) {
            res_buf[len - i - 1] = str[i];
        }
        res_buf[len] = '\0';
        return object_make_string_no_copy(vm->mem, res_buf);
    }
    return object_make_null();
}

static object_t array_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (argc == 1) {
        if (!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER)) {
            return object_make_null();
        }
        int capacity = (int)object_get_number(args[0]);
        array(object_t) *res_arr = array_make_with_capacity(capacity, sizeof(object_t));
        object_t obj_null = object_make_null();
        for (int i = 0; i < capacity; i++) {
            array_add(res_arr, &obj_null);
        }
        return object_make_array_with_array(vm->mem, res_arr);
    } else if (argc == 2) {
        if (!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER, OBJECT_ANY)) {
            return object_make_null();
        }
        int capacity = (int)object_get_number(args[0]);
        array(object_t) *res_arr = array_make_with_capacity(capacity, sizeof(object_t));
        for (int i = 0; i < capacity; i++) {
            array_add(res_arr, &args[1]);
        }
        return object_make_array_with_array(vm->mem, res_arr);
    }
    CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER);
    return object_make_null();
}

static object_t append_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ARRAY, OBJECT_ANY)) {
        return object_make_null();
    }
    object_add_array_value(args[0], args[1]);
    int len = object_get_array_length(args[0]);
    return object_make_number(len);
}

static object_t println_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;

    const ape_config_t *config = vm->config;
    
    if (!config->stdio.write.write) {
        return object_make_null(); // todo: runtime error?
    }

    strbuf_t *buf = strbuf_make();
    for (int i = 0; i < argc; i++) {
        object_t arg = args[i];
        object_to_string(arg, buf, false);
    }
    strbuf_append(buf, "\n");
    config->stdio.write.write(config->stdio.write.context, strbuf_get_string(buf), strbuf_get_length(buf));
    strbuf_destroy(buf);
    return object_make_null();
}

static object_t print_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    const ape_config_t *config = vm->config;

    if (!config->stdio.write.write) {
        return object_make_null(); // todo: runtime error?
    }

    strbuf_t *buf = strbuf_make();
    for (int i = 0; i < argc; i++) {
        object_t arg = args[i];
        object_to_string(arg, buf, false);
    }
    config->stdio.write.write(config->stdio.write.context, strbuf_get_string(buf), strbuf_get_length(buf));
    strbuf_destroy(buf);
    return object_make_null();
}

static object_t write_file_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_STRING, OBJECT_STRING)) {
        return object_make_null();
    }

    const ape_config_t *config = vm->config;

    if (!config->fileio.write_file.write_file) {
        return object_make_null();
    }

    const char *path = object_get_string(args[0]);
    const char *string = object_get_string(args[1]);
    int string_size = (int)strlen(string) + 1;

    int written = (int)config->fileio.write_file.write_file(config->fileio.write_file.context, path, string, string_size);
    
    return object_make_number(written);
}

static object_t read_file_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_STRING)) {
        return object_make_null();
    }

    const ape_config_t *config = vm->config;

    if (!config->fileio.read_file.read_file) {
        return object_make_null();
    }

    const char *path = object_get_string(args[0]);

    const char *contents = config->fileio.read_file.read_file(config->fileio.read_file.context, path);
    if (!contents) {
        return object_make_null();
    }

    return object_make_string(vm->mem, contents);
}

static object_t to_str_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_STRING | OBJECT_NUMBER | OBJECT_BOOL | OBJECT_NULL | OBJECT_MAP | OBJECT_ARRAY)) {
        return object_make_null();
    }
    object_t arg = args[0];
    strbuf_t *buf = strbuf_make();
    object_to_string(arg, buf, false);
    object_t res = object_make_string(vm->mem, strbuf_get_string(buf));
    strbuf_destroy(buf);
    return res;
}

static object_t char_to_str_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER)) {
        return object_make_null();
    }

    double val = object_get_number(args[0]);

    char c = (char)val;
    char str[2] = {c, '\0'};
    return object_make_string(vm->mem, str);
}

static object_t range_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    for (int i = 0; i < argc; i++) {
        object_type_t type = object_get_type(args[i]);
        if (type != OBJECT_NUMBER) {
            const char *type_str = object_get_type_name(type);
            const char *expected_str = object_get_type_name(OBJECT_NUMBER);
            errortag_t *err = error_makef(ERROR_RUNTIME, src_pos_invalid,
                                       "Invalid argument %d passed to range, got %s instead of %s",
                                       i, type_str, expected_str);
            vm_set_runtime_error(vm, err);
            return object_make_null();
        }
    }

    int start = 0;
    int end = 0;
    int step = 1;

    if (argc == 1) {
        end = object_get_number(args[0]);
    } else if (argc == 2) {
        start = object_get_number(args[0]);
        end = object_get_number(args[1]);
    } else if (argc == 3) {
        start = object_get_number(args[0]);
        end = object_get_number(args[1]);
        step = object_get_number(args[2]);
    } else {
        errortag_t *err = error_makef(ERROR_RUNTIME, src_pos_invalid, "Invalid number of arguments passed to range, got %d", argc);
        vm_set_runtime_error(vm, err);
        return object_make_null();
    }

    if (step == 0) {
        errortag_t *err = error_make(ERROR_RUNTIME, src_pos_invalid, "range step cannot be 0");
        vm_set_runtime_error(vm, err);
        return object_make_null();
    }

    object_t res = object_make_array(vm->mem);
    for (int i = start; i < end; i += step) {
        object_t item = object_make_number(i);
        object_add_array_value(res, item);
    }
    return res;
}

static object_t keys_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_MAP)) {
        return object_make_null();
    }
    object_t arg = args[0];
    object_t res = object_make_array(vm->mem);
    int len = object_get_map_length(arg);
    for (int i = 0; i < len; i++) {
        object_t key = object_get_map_key_at(arg, i);
        object_add_array_value(res, key);
    }
    return res;
}

static object_t values_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_MAP)) {
        return object_make_null();
    }
    object_t arg = args[0];
    object_t res = object_make_array(vm->mem);
    int len = object_get_map_length(arg);
    for (int i = 0; i < len; i++) {
        object_t key = object_get_map_value_at(arg, i);
        object_add_array_value(res, key);
    }
    return res;
}

static object_t copy_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY)) {
        return object_make_null();
    }
    return object_copy(vm->mem, args[0]);
}

static object_t deep_copy_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY)) {
        return object_make_null();
    }
    return object_deep_copy(vm->mem, args[0]);
}

static object_t concat_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ARRAY | OBJECT_STRING, OBJECT_ANY)) {
        return object_make_null();
    }
    object_type_t type = object_get_type(args[0]);
    object_type_t item_type = object_get_type(args[1]);
    if (type == OBJECT_ARRAY) {
        if (item_type != OBJECT_ARRAY) {
            const char *item_type_str = object_get_type_name(item_type);
            errortag_t *err = error_makef(ERROR_RUNTIME, src_pos_invalid,
                                       "Invalid argument 2 passed to concat, got %s",
                                       item_type_str);
            vm_set_runtime_error(vm, err);
            return object_make_null();
        }
        array(object_t) *arr = object_get_array(args[0]);
        array(object_t) *item_arr = object_get_array(args[1]);
        array_add_array(arr, item_arr);
        return object_make_number(array_count(arr));
    } else if (type == OBJECT_STRING) {
        if (!CHECK_ARGS(vm, true, argc, args, OBJECT_STRING, OBJECT_STRING)) {
            return object_make_null();
        }
        const char *str = object_get_string(args[0]);
        int len = (int)strlen(str);
        const char *arg_str = object_get_string(args[1]);
        int arg_str_len = (int)strlen(arg_str);
        char *res_buf = ape_malloc(len + arg_str_len + 1);
        for (int i = 0; i < len; i++) {
            res_buf[i] = str[i];
        }
        for (int i = 0; i < arg_str_len; i++) {
            res_buf[len + i] = arg_str[i];
        }
        res_buf[len + arg_str_len] = '\0';
        return object_make_string_no_copy(vm->mem, res_buf);
    }
    return object_make_null();
}

static object_t assert_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_BOOL)) {
        return object_make_null();
    }

    if (!object_get_bool(args[0])) {
        errortag_t *err = error_make(ERROR_RUNTIME, src_pos_invalid, "assertion failed");
        vm_set_runtime_error(vm, err);
        return object_make_null();
    }

    return object_make_bool(true);
}

static object_t remove_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ARRAY, OBJECT_ANY)) {
        return object_make_null();
    }

    int ix = -1;
    for (int i = 0; i < object_get_array_length(args[0]); i++) {
        object_t obj = object_get_array_value_at(args[0], i);
        if (object_equals(obj, args[1])) {
            ix = i;
            break;
        }
    }

    if (ix == -1) {
        return object_make_bool(false);
    }

    array(object_t) *arr = object_get_array(args[0]);
    bool res = array_remove_at(arr, ix);
    return object_make_bool(res);
}

static object_t remove_at_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ARRAY, OBJECT_NUMBER)) {
        return object_make_null();
    }

    object_type_t type = object_get_type(args[0]);
    int ix = object_get_number(args[1]);

    switch (type) {
        case OBJECT_ARRAY: {
            array(object_t) *arr = object_get_array(args[0]);
            bool res = array_remove_at(arr, ix);
            return object_make_bool(res);
        }
        default:
            break;
    }

    return object_make_bool(true);
}


static object_t error_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (argc == 1 && object_get_type(args[0]) == OBJECT_STRING) {
        return object_make_error(vm->mem, object_get_string(args[0]));
    } else {
        return object_make_error(vm->mem, "");
    }
}

static object_t crash_fn(vm_t *vm, void *data, int argc, object_t *args) {
    errortag_t *err = NULL;
    if (argc == 1 && object_get_type(args[0]) == OBJECT_STRING) {
        err = error_make(ERROR_RUNTIME, frame_src_position(vm->current_frame), object_get_string(args[0]));
    } else {
        err = error_make(ERROR_RUNTIME, frame_src_position(vm->current_frame), "");
    }
    vm_set_runtime_error(vm, err);
    return object_make_null();
}

static object_t random_fn(vm_t *vm, void *data, int argc, object_t *args) {
    double res = (double)rand() / RAND_MAX;
    if (argc == 0) {
        return object_make_number(res);
    } else if (argc == 2) {
        if (!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER, OBJECT_NUMBER)) {
            return object_make_null();
        }
        double min = object_get_number(args[0]);
        double max = object_get_number(args[1]);
        if (min >= max) {
            errortag_t *err = error_make(ERROR_RUNTIME, src_pos_invalid, "max is bigger than min");
            vm_set_runtime_error(vm, err);
            return object_make_null();
        }
        double range = max - min;
        res = min + (res * range);
        return object_make_number(res);
    } else {
        errortag_t *err = error_make(ERROR_RUNTIME, src_pos_invalid, "Invalid number or arguments");
        vm_set_runtime_error(vm, err);
        return object_make_null();
    }
}

//-----------------------------------------------------------------------------
// Type checks
//-----------------------------------------------------------------------------

static object_t is_string_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY)) {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_STRING);
}

static object_t is_array_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY)) {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_ARRAY);
}

static object_t is_map_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY)) {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_MAP);
}

static object_t is_number_fn(vm_t *vm, void *data, int argc, object_t *args){
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY)) {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_NUMBER);
}

static object_t is_bool_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY)) {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_BOOL);
}

static object_t is_null_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY)) {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_NULL);
}

static object_t is_function_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY)) {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_FUNCTION);
}

static object_t is_external_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY)) {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_EXTERNAL);
}

static object_t is_error_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY)) {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_ERROR);
}

static object_t is_native_function_fn(vm_t *vm, void *data, int argc, object_t *args) {
    (void)data;
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY)) {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_NATIVE_FUNCTION);
}

//-----------------------------------------------------------------------------
// Math
//-----------------------------------------------------------------------------

static object_t sqrt_fn(vm_t *vm, void *data, int argc, object_t *args) {
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER)) {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = sqrt(arg);
    return object_make_number(res);
}

static object_t pow_fn(vm_t *vm, void *data, int argc, object_t *args) {
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER, OBJECT_NUMBER)) {
        return object_make_null();
    }
    double arg1 = object_get_number(args[0]);
    double arg2 = object_get_number(args[1]);
    double res = pow(arg1, arg2);
    return object_make_number(res);
}

static object_t sin_fn(vm_t *vm, void *data, int argc, object_t *args) {
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER)) {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = sin(arg);
    return object_make_number(res);
}

static object_t cos_fn(vm_t *vm, void *data, int argc, object_t *args) {
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER)) {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = cos(arg);
    return object_make_number(res);
}

static object_t tan_fn(vm_t *vm, void *data, int argc, object_t *args) {
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER)) {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = tan(arg);
    return object_make_number(res);
}

static object_t log_fn(vm_t *vm, void *data, int argc, object_t *args) {
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER)) {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = log(arg);
    return object_make_number(res);
}

static object_t ceil_fn(vm_t *vm, void *data, int argc, object_t *args) {
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER)) {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = ceil(arg);
    return object_make_number(res);
}

static object_t floor_fn(vm_t *vm, void *data, int argc, object_t *args) {
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER)) {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = floor(arg);
    return object_make_number(res);
}

static object_t abs_fn(vm_t *vm, void *data, int argc, object_t *args) {
    if (!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER)) {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = fabs(arg);
    return object_make_number(res);
}


static bool check_args(vm_t *vm, bool generate_error, int argc, object_t *args, int expected_argc, object_type_t *expected_types) {
    if (argc != expected_argc) {
        if (generate_error) {
            errortag_t *err = error_makef(ERROR_RUNTIME, src_pos_invalid,
                                       "Invalid number or arguments, got %d instead of %d",
                                       argc, expected_argc);
            vm_set_runtime_error(vm, err);
        }
        return false;
    }

    for (int i = 0; i < argc; i++) {
       object_t arg = args[i];
        object_type_t type = object_get_type(arg);
        object_type_t expected_type = expected_types[i];
        if (!(type & expected_type)) {
            if (generate_error) {
                const char *type_str = object_get_type_name(type);
                const char *expected_type_str = object_get_type_name(expected_type);
                errortag_t *err = error_makef(ERROR_RUNTIME, src_pos_invalid,
                                           "Invalid argument %d type, got %s, expected %s",
                                           i, type_str, expected_type_str);
                vm_set_runtime_error(vm, err);
            }
            return false;
        }
    }
    return true;
}
