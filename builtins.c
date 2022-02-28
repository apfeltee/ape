
#include "ape.h"


static bool check_args(ApeVM_t* vm, bool generate_error, int argc, ApeObject_t* args, int expected_argc, object_type_t* expected_types);


// INTERNAL
static ApeObject_t len_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_STRING | OBJECT_ARRAY | OBJECT_MAP))
    {
        return object_make_null();
    }

    ApeObject_t arg = args[0];
    object_type_t type = object_get_type(arg);
    if(type == OBJECT_STRING)
    {
        int len = object_get_string_length(arg);
        return object_make_number(len);
    }
    else if(type == OBJECT_ARRAY)
    {
        int len = object_get_array_length(arg);
        return object_make_number(len);
    }
    else if(type == OBJECT_MAP)
    {
        int len = object_get_map_length(arg);
        return object_make_number(len);
    }

    return object_make_null();
}

static ApeObject_t first_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ARRAY))
    {
        return object_make_null();
    }
    ApeObject_t arg = args[0];
    return object_get_array_value_at(arg, 0);
}

static ApeObject_t last_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ARRAY))
    {
        return object_make_null();
    }
    ApeObject_t arg = args[0];
    return object_get_array_value_at(arg, object_get_array_length(arg) - 1);
}

static ApeObject_t rest_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ARRAY))
    {
        return object_make_null();
    }
    ApeObject_t arg = args[0];
    int len = object_get_array_length(arg);
    if(len == 0)
    {
        return object_make_null();
    }

    ApeObject_t res = object_make_array(vm->mem);
    if(object_is_null(res))
    {
        return object_make_null();
    }
    for(int i = 1; i < len; i++)
    {
        ApeObject_t item = object_get_array_value_at(arg, i);
        bool ok = object_add_array_value(res, item);
        if(!ok)
        {
            return object_make_null();
        }
    }
    return res;
}

static ApeObject_t reverse_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ARRAY | OBJECT_STRING))
    {
        return object_make_null();
    }
    ApeObject_t arg = args[0];
    object_type_t type = object_get_type(arg);
    if(type == OBJECT_ARRAY)
    {
        int len = object_get_array_length(arg);
        ApeObject_t res = object_make_array_with_capacity(vm->mem, len);
        if(object_is_null(res))
        {
            return object_make_null();
        }
        for(int i = 0; i < len; i++)
        {
            ApeObject_t obj = object_get_array_value_at(arg, i);
            bool ok = object_set_array_value_at(res, len - i - 1, obj);
            if(!ok)
            {
                return object_make_null();
            }
        }
        return res;
    }
    else if(type == OBJECT_STRING)
    {
        const char* str = object_get_string(arg);
        int len = object_get_string_length(arg);

        ApeObject_t res = object_make_string_with_capacity(vm->mem, len);
        if(object_is_null(res))
        {
            return object_make_null();
        }
        char* res_buf = object_get_mutable_string(res);
        for(int i = 0; i < len; i++)
        {
            res_buf[len - i - 1] = str[i];
        }
        res_buf[len] = '\0';
        object_set_string_length(res, len);
        return res;
    }
    return object_make_null();
}

static ApeObject_t array_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(argc == 1)
    {
        if(!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER))
        {
            return object_make_null();
        }
        int capacity = (int)object_get_number(args[0]);
        ApeObject_t res = object_make_array_with_capacity(vm->mem, capacity);
        if(object_is_null(res))
        {
            return object_make_null();
        }
        ApeObject_t obj_null = object_make_null();
        for(int i = 0; i < capacity; i++)
        {
            bool ok = object_add_array_value(res, obj_null);
            if(!ok)
            {
                return object_make_null();
            }
        }
        return res;
    }
    else if(argc == 2)
    {
        if(!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER, OBJECT_ANY))
        {
            return object_make_null();
        }
        int capacity = (int)object_get_number(args[0]);
        ApeObject_t res = object_make_array_with_capacity(vm->mem, capacity);
        if(object_is_null(res))
        {
            return object_make_null();
        }
        for(int i = 0; i < capacity; i++)
        {
            bool ok = object_add_array_value(res, args[1]);
            if(!ok)
            {
                return object_make_null();
            }
        }
        return res;
    }
    CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER);
    return object_make_null();
}

static ApeObject_t append_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ARRAY, OBJECT_ANY))
    {
        return object_make_null();
    }
    bool ok = object_add_array_value(args[0], args[1]);
    if(!ok)
    {
        return object_make_null();
    }
    int len = object_get_array_length(args[0]);
    return object_make_number(len);
}

static ApeObject_t println_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;

    const ape_config_t* config = vm->config;

    if(!config->stdio.write.write)
    {
        return object_make_null();// todo: runtime error?
    }

    strbuf_t* buf = strbuf_make(vm->alloc);
    if(!buf)
    {
        return object_make_null();
    }
    for(int i = 0; i < argc; i++)
    {
        ApeObject_t arg = args[i];
        object_to_string(arg, buf, false);
    }
    strbuf_append(buf, "\n");
    if(strbuf_failed(buf))
    {
        strbuf_destroy(buf);
        return object_make_null();
    }
    config->stdio.write.write(config->stdio.write.context, strbuf_get_string(buf), strbuf_get_length(buf));
    strbuf_destroy(buf);
    return object_make_null();
}

static ApeObject_t print_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    const ape_config_t* config = vm->config;

    if(!config->stdio.write.write)
    {
        return object_make_null();// todo: runtime error?
    }

    strbuf_t* buf = strbuf_make(vm->alloc);
    if(!buf)
    {
        return object_make_null();
    }
    for(int i = 0; i < argc; i++)
    {
        ApeObject_t arg = args[i];
        object_to_string(arg, buf, false);
    }
    if(strbuf_failed(buf))
    {
        strbuf_destroy(buf);
        return object_make_null();
    }
    config->stdio.write.write(config->stdio.write.context, strbuf_get_string(buf), strbuf_get_length(buf));
    strbuf_destroy(buf);
    return object_make_null();
}

static ApeObject_t write_file_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_STRING, OBJECT_STRING))
    {
        return object_make_null();
    }

    const ape_config_t* config = vm->config;

    if(!config->fileio.write_file.write_file)
    {
        return object_make_null();
    }

    const char* path = object_get_string(args[0]);
    const char* string = object_get_string(args[1]);
    int string_len = object_get_string_length(args[1]);

    int written = (int)config->fileio.write_file.write_file(config->fileio.write_file.context, path, string, string_len);

    return object_make_number(written);
}

static ApeObject_t read_file_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_STRING))
    {
        return object_make_null();
    }

    const ape_config_t* config = vm->config;

    if(!config->fileio.read_file.read_file)
    {
        return object_make_null();
    }

    const char* path = object_get_string(args[0]);

    char* contents = config->fileio.read_file.read_file(config->fileio.read_file.context, path);
    if(!contents)
    {
        return object_make_null();
    }
    ApeObject_t res = object_make_string(vm->mem, contents);
    allocator_free(vm->alloc, contents);
    return res;
}

static ApeObject_t to_str_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_STRING | OBJECT_NUMBER | OBJECT_BOOL | OBJECT_NULL | OBJECT_MAP | OBJECT_ARRAY))
    {
        return object_make_null();
    }
    ApeObject_t arg = args[0];
    strbuf_t* buf = strbuf_make(vm->alloc);
    if(!buf)
    {
        return object_make_null();
    }
    object_to_string(arg, buf, false);
    if(strbuf_failed(buf))
    {
        strbuf_destroy(buf);
        return object_make_null();
    }
    ApeObject_t res = object_make_string(vm->mem, strbuf_get_string(buf));
    strbuf_destroy(buf);
    return res;
}

static ApeObject_t to_num_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_STRING | OBJECT_NUMBER | OBJECT_BOOL | OBJECT_NULL))
    {
        return object_make_null();
    }
    double result = 0;
    const char* string = "";
    if(object_is_numeric(args[0]))
    {
        result = object_get_number(args[0]);
    }
    else if(object_is_null(args[0]))
    {
        result = 0;
    }
    else if(object_get_type(args[0]) == OBJECT_STRING)
    {
        string = object_get_string(args[0]);
        char* end;
        errno = 0;
        result = strtod(string, &end);
        if(errno == ERANGE && (result <= -HUGE_VAL || result >= HUGE_VAL))
        {
            goto err;
            ;
        }
        if(errno && errno != ERANGE)
        {
            goto err;
        }
        int string_len = object_get_string_length(args[0]);
        int parsed_len = (int)(end - string);
        if(string_len != parsed_len)
        {
            goto err;
        }
    }
    else
    {
        goto err;
    }
    return object_make_number(result);
err:
    errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "Cannot convert \"%s\" to number", string);
    return object_make_null();
}

static ApeObject_t char_to_str_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER))
    {
        return object_make_null();
    }

    double val = object_get_number(args[0]);

    char c = (char)val;
    char str[2] = { c, '\0' };
    return object_make_string(vm->mem, str);
}

static ApeObject_t range_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    for(int i = 0; i < argc; i++)
    {
        object_type_t type = object_get_type(args[i]);
        if(type != OBJECT_NUMBER)
        {
            const char* type_str = object_get_type_name(type);
            const char* expected_str = object_get_type_name(OBJECT_NUMBER);
            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid,
                              "Invalid argument %d passed to range, got %s instead of %s", i, type_str, expected_str);
            return object_make_null();
        }
    }

    int start = 0;
    int end = 0;
    int step = 1;

    if(argc == 1)
    {
        end = (int)object_get_number(args[0]);
    }
    else if(argc == 2)
    {
        start = (int)object_get_number(args[0]);
        end = (int)object_get_number(args[1]);
    }
    else if(argc == 3)
    {
        start = (int)object_get_number(args[0]);
        end = (int)object_get_number(args[1]);
        step = (int)object_get_number(args[2]);
    }
    else
    {
        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "Invalid number of arguments passed to range, got %d", argc);
        return object_make_null();
    }

    if(step == 0)
    {
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "range step cannot be 0");
        return object_make_null();
    }

    ApeObject_t res = object_make_array(vm->mem);
    if(object_is_null(res))
    {
        return object_make_null();
    }
    for(int i = start; i < end; i += step)
    {
        ApeObject_t item = object_make_number(i);
        bool ok = object_add_array_value(res, item);
        if(!ok)
        {
            return object_make_null();
        }
    }
    return res;
}

static ApeObject_t keys_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_MAP))
    {
        return object_make_null();
    }
    ApeObject_t arg = args[0];
    ApeObject_t res = object_make_array(vm->mem);
    if(object_is_null(res))
    {
        return object_make_null();
    }
    int len = object_get_map_length(arg);
    for(int i = 0; i < len; i++)
    {
        ApeObject_t key = object_get_map_key_at(arg, i);
        bool ok = object_add_array_value(res, key);
        if(!ok)
        {
            return object_make_null();
        }
    }
    return res;
}

static ApeObject_t values_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_MAP))
    {
        return object_make_null();
    }
    ApeObject_t arg = args[0];
    ApeObject_t res = object_make_array(vm->mem);
    if(object_is_null(res))
    {
        return object_make_null();
    }
    int len = object_get_map_length(arg);
    for(int i = 0; i < len; i++)
    {
        ApeObject_t key = object_get_map_value_at(arg, i);
        bool ok = object_add_array_value(res, key);
        if(!ok)
        {
            return object_make_null();
        }
    }
    return res;
}

static ApeObject_t copy_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_copy(vm->mem, args[0]);
}

static ApeObject_t deep_copy_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_deep_copy(vm->mem, args[0]);
}

static ApeObject_t concat_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ARRAY | OBJECT_STRING, OBJECT_ARRAY | OBJECT_STRING))
    {
        return object_make_null();
    }
    object_type_t type = object_get_type(args[0]);
    object_type_t item_type = object_get_type(args[1]);
    if(type == OBJECT_ARRAY)
    {
        if(item_type != OBJECT_ARRAY)
        {
            const char* item_type_str = object_get_type_name(item_type);
            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "Invalid argument 2 passed to concat, got %s", item_type_str);
            return object_make_null();
        }
        for(int i = 0; i < object_get_array_length(args[1]); i++)
        {
            ApeObject_t item = object_get_array_value_at(args[1], i);
            bool ok = object_add_array_value(args[0], item);
            if(!ok)
            {
                return object_make_null();
            }
        }
        return object_make_number(object_get_array_length(args[0]));
    }
    else if(type == OBJECT_STRING)
    {
        if(!CHECK_ARGS(vm, true, argc, args, OBJECT_STRING, OBJECT_STRING))
        {
            return object_make_null();
        }
        const char* left_val = object_get_string(args[0]);
        int left_len = (int)object_get_string_length(args[0]);

        const char* right_val = object_get_string(args[1]);
        int right_len = (int)object_get_string_length(args[1]);

        ApeObject_t res = object_make_string_with_capacity(vm->mem, left_len + right_len);
        if(object_is_null(res))
        {
            return object_make_null();
        }

        bool ok = object_string_append(res, left_val, left_len);
        if(!ok)
        {
            return object_make_null();
        }

        ok = object_string_append(res, right_val, right_len);
        if(!ok)
        {
            return object_make_null();
        }

        return res;
    }
    return object_make_null();
}

static ApeObject_t remove_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ARRAY, OBJECT_ANY))
    {
        return object_make_null();
    }

    int ix = -1;
    for(int i = 0; i < object_get_array_length(args[0]); i++)
    {
        ApeObject_t obj = object_get_array_value_at(args[0], i);
        if(object_equals(obj, args[1]))
        {
            ix = i;
            break;
        }
    }

    if(ix == -1)
    {
        return object_make_bool(false);
    }

    bool res = object_remove_array_value_at(args[0], ix);
    return object_make_bool(res);
}

static ApeObject_t remove_at_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ARRAY, OBJECT_NUMBER))
    {
        return object_make_null();
    }

    object_type_t type = object_get_type(args[0]);
    int ix = (int)object_get_number(args[1]);

    switch(type)
    {
        case OBJECT_ARRAY:
        {
            bool res = object_remove_array_value_at(args[0], ix);
            return object_make_bool(res);
        }
        default:
            break;
    }

    return object_make_bool(true);
}


static ApeObject_t error_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(argc == 1 && object_get_type(args[0]) == OBJECT_STRING)
    {
        return object_make_error(vm->mem, object_get_string(args[0]));
    }
    else
    {
        return object_make_error(vm->mem, "");
    }
}

static ApeObject_t crash_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(argc == 1 && object_get_type(args[0]) == OBJECT_STRING)
    {
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), object_get_string(args[0]));
    }
    else
    {
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "");
    }
    return object_make_null();
}

static ApeObject_t assert_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_BOOL))
    {
        return object_make_null();
    }

    if(!object_get_bool(args[0]))
    {
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "assertion failed");
        return object_make_null();
    }

    return object_make_bool(true);
}

static ApeObject_t random_seed_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER))
    {
        return object_make_null();
    }
    int seed = (int)object_get_number(args[0]);
    srand(seed);
    return object_make_bool(true);
}

static ApeObject_t random_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    double res = (double)rand() / RAND_MAX;
    if(argc == 0)
    {
        return object_make_number(res);
    }
    else if(argc == 2)
    {
        if(!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER, OBJECT_NUMBER))
        {
            return object_make_null();
        }
        double min = object_get_number(args[0]);
        double max = object_get_number(args[1]);
        if(min >= max)
        {
            errors_add_error(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "max is bigger than min");
            return object_make_null();
        }
        double range = max - min;
        res = min + (res * range);
        return object_make_number(res);
    }
    else
    {
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "Invalid number or arguments");
        return object_make_null();
    }
}

static ApeObject_t slice_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_STRING | OBJECT_ARRAY, OBJECT_NUMBER))
    {
        return object_make_null();
    }
    object_type_t arg_type = object_get_type(args[0]);
    int index = (int)object_get_number(args[1]);
    if(arg_type == OBJECT_ARRAY)
    {
        int len = object_get_array_length(args[0]);
        if(index < 0)
        {
            index = len + index;
            if(index < 0)
            {
                index = 0;
            }
        }
        ApeObject_t res = object_make_array_with_capacity(vm->mem, len - index);
        if(object_is_null(res))
        {
            return object_make_null();
        }
        for(int i = index; i < len; i++)
        {
            ApeObject_t item = object_get_array_value_at(args[0], i);
            bool ok = object_add_array_value(res, item);
            if(!ok)
            {
                return object_make_null();
            }
        }
        return res;
    }
    else if(arg_type == OBJECT_STRING)
    {
        const char* str = object_get_string(args[0]);
        int len = (int)object_get_string_length(args[0]);
        if(index < 0)
        {
            index = len + index;
            if(index < 0)
            {
                return object_make_string(vm->mem, "");
            }
        }
        if(index >= len)
        {
            return object_make_string(vm->mem, "");
        }
        int res_len = len - index;
        ApeObject_t res = object_make_string_with_capacity(vm->mem, res_len);
        if(object_is_null(res))
        {
            return object_make_null();
        }

        char* res_buf = object_get_mutable_string(res);
        memset(res_buf, 0, res_len + 1);
        for(int i = index; i < len; i++)
        {
            char c = str[i];
            res_buf[i - index] = c;
        }
        object_set_string_length(res, res_len);
        return res;
    }
    else
    {
        const char* type_str = object_get_type_name(arg_type);
        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "Invalid argument 0 passed to slice, got %s instead", type_str);
        return object_make_null();
    }
}

//-----------------------------------------------------------------------------
// Type checks
//-----------------------------------------------------------------------------

static ApeObject_t is_string_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_STRING);
}

static ApeObject_t is_array_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_ARRAY);
}

static ApeObject_t is_map_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_MAP);
}

static ApeObject_t is_number_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_NUMBER);
}

static ApeObject_t is_bool_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_BOOL);
}

static ApeObject_t is_null_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_NULL);
}

static ApeObject_t is_function_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_FUNCTION);
}

static ApeObject_t is_external_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_EXTERNAL);
}

static ApeObject_t is_error_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_ERROR);
}

static ApeObject_t is_native_function_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == OBJECT_NATIVE_FUNCTION);
}

//-----------------------------------------------------------------------------
// Math
//-----------------------------------------------------------------------------

static ApeObject_t sqrt_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = sqrt(arg);
    return object_make_number(res);
}

static ApeObject_t pow_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER, OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg1 = object_get_number(args[0]);
    double arg2 = object_get_number(args[1]);
    double res = pow(arg1, arg2);
    return object_make_number(res);
}

static ApeObject_t sin_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = sin(arg);
    return object_make_number(res);
}

static ApeObject_t cos_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = cos(arg);
    return object_make_number(res);
}

static ApeObject_t tan_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = tan(arg);
    return object_make_number(res);
}

static ApeObject_t log_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = log(arg);
    return object_make_number(res);
}

static ApeObject_t ceil_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = ceil(arg);
    return object_make_number(res);
}

static ApeObject_t floor_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = floor(arg);
    return object_make_number(res);
}

static ApeObject_t abs_fn(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = fabs(arg);
    return object_make_number(res);
}

static bool check_args(ApeVM_t* vm, bool generate_error, int argc, ApeObject_t* args, int expected_argc, object_type_t* expected_types)
{
    if(argc != expected_argc)
    {
        if(generate_error)
        {
            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid,
                              "Invalid number or arguments, got %d instead of %d", argc, expected_argc);
        }
        return false;
    }

    for(int i = 0; i < argc; i++)
    {
        ApeObject_t arg = args[i];
        object_type_t type = object_get_type(arg);
        object_type_t expected_type = expected_types[i];
        if(!(type & expected_type))
        {
            if(generate_error)
            {
                const char* type_str = object_get_type_name(type);
                char* expected_type_str = object_get_type_union_name(vm->alloc, expected_type);
                if(!expected_type_str)
                {
                    return false;
                }
                errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid,
                                  "Invalid argument %d type, got %s, expected %s", i, type_str, expected_type_str);
                allocator_free(vm->alloc, expected_type_str);
            }
            return false;
        }
    }
    return true;
}

static struct
{
    const char* name;
    ApeNativeFNCallback_t fn;
} g_native_functions[] = {
    { "len", len_fn },
    { "println", println_fn },
    { "print", print_fn },
    { "read_file", read_file_fn },
    { "write_file", write_file_fn },
    { "first", first_fn },
    { "last", last_fn },
    { "rest", rest_fn },
    { "append", append_fn },
    { "remove", remove_fn },
    { "remove_at", remove_at_fn },
    { "to_str", to_str_fn },
    { "to_num", to_num_fn },
    { "range", range_fn },
    { "keys", keys_fn },
    { "values", values_fn },
    { "copy", copy_fn },
    { "deep_copy", deep_copy_fn },
    { "concat", concat_fn },
    { "char_to_str", char_to_str_fn },
    { "reverse", reverse_fn },
    { "array", array_fn },
    { "error", error_fn },
    { "crash", crash_fn },
    { "assert", assert_fn },
    { "random_seed", random_seed_fn },
    { "random", random_fn },
    { "slice", slice_fn },

    // Type checks
    { "is_string", is_string_fn },
    { "is_array", is_array_fn },
    { "is_map", is_map_fn },
    { "is_number", is_number_fn },
    { "is_bool", is_bool_fn },
    { "is_null", is_null_fn },
    { "is_function", is_function_fn },
    { "is_external", is_external_fn },
    { "is_error", is_error_fn },
    { "is_native_function", is_native_function_fn },

    // Math
    { "sqrt", sqrt_fn },
    { "pow", pow_fn },
    { "sin", sin_fn },
    { "cos", cos_fn },
    { "tan", tan_fn },
    { "log", log_fn },
    { "ceil", ceil_fn },
    { "floor", floor_fn },
    { "abs", abs_fn },
};


int builtins_count()
{
    return APE_ARRAY_LEN(g_native_functions);
}

ApeNativeFNCallback_t builtins_get_fn(int ix)
{
    return g_native_functions[ix].fn;
}

const char* builtins_get_name(int ix)
{
    return g_native_functions[ix].name;
}
