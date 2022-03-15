
#include <sys/stat.h>
#if __has_include(<dirent.h>)
    #include <dirent.h>
#else
    #define CORE_NODIRENT
#endif

#include "priv.h"

#define CHECK_ARGS(vm, generate_error, argc, args, ...) \
    check_args((vm), (generate_error), (argc), (args),  \
               sizeof((ApeObjectType_t[]){ __VA_ARGS__ }) / sizeof(ApeObjectType_t), (ApeObjectType_t[]){ __VA_ARGS__ })

#define APE_CHECK_ARGS(ape, generate_error, argc, args, ...) \
    ape_check_args((ape), (generate_error), (argc), (args), sizeof((int[]){ __VA_ARGS__ }) / sizeof(int), (int[]){ __VA_ARGS__ })



//ApeObject_t object_make_native_function(ApeGCMemory_t* mem, const char* name, ApeNativeFNCallback_t fn, void* data, int data_len);
#define make_fn_data(vm, name, fnc, dataptr, datasize) \
    object_make_native_function(vm->mem, name, fnc, dataptr, datasize)

#define make_fn(vm, name, fnc) \
    make_fn_data(vm, name, fnc, NULL, 0)

#define make_fn_entry_data(vm, map, name, fnc, dataptr, datasize) \
    ape_object_set_map_value(map, name, make_fn(vm, name, fnc))

#define make_fn_entry(vm, map, name, fnc) \
    make_fn_entry_data(vm, map, name, fnc, NULL, 0)



typedef struct NatFunc_t NatFunc_t;
struct NatFunc_t
{
    const char* name;
    ApeNativeFNCallback_t fn;
};


static bool check_args(ApeVM_t* vm, bool generate_error, int argc, ApeObject_t* args, int expected_argc, ApeObjectType_t* expected_types);


// INTERNAL
static ApeObject_t cfn_len(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING | APE_OBJECT_ARRAY | APE_OBJECT_MAP))
    {
        return object_make_null();
    }

    ApeObject_t arg = args[0];
    ApeObjectType_t type = object_get_type(arg);
    if(type == APE_OBJECT_STRING)
    {
        int len = object_get_string_length(arg);
        return object_make_number(len);
    }
    else if(type == APE_OBJECT_ARRAY)
    {
        int len = object_get_array_length(arg);
        return object_make_number(len);
    }
    else if(type == APE_OBJECT_MAP)
    {
        int len = object_get_map_length(arg);
        return object_make_number(len);
    }

    return object_make_null();
}

static ApeObject_t cfn_first(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY))
    {
        return object_make_null();
    }
    ApeObject_t arg = args[0];
    return object_get_array_value(arg, 0);
}

static ApeObject_t cfn_last(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY))
    {
        return object_make_null();
    }
    ApeObject_t arg = args[0];
    return object_get_array_value(arg, object_get_array_length(arg) - 1);
}

static ApeObject_t cfn_rest(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY))
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
        ApeObject_t item = object_get_array_value(arg, i);
        bool ok = object_add_array_value(res, item);
        if(!ok)
        {
            return object_make_null();
        }
    }
    return res;
}

static ApeObject_t cfn_reverse(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY | APE_OBJECT_STRING))
    {
        return object_make_null();
    }
    ApeObject_t arg = args[0];
    ApeObjectType_t type = object_get_type(arg);
    if(type == APE_OBJECT_ARRAY)
    {
        int len = object_get_array_length(arg);
        ApeObject_t res = object_make_array_with_capacity(vm->mem, len);
        if(object_is_null(res))
        {
            return object_make_null();
        }
        for(int i = 0; i < len; i++)
        {
            ApeObject_t obj = object_get_array_value(arg, i);
            bool ok = object_set_array_value_at(res, len - i - 1, obj);
            if(!ok)
            {
                return object_make_null();
            }
        }
        return res;
    }
    else if(type == APE_OBJECT_STRING)
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

static ApeObject_t cfn_array(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(argc == 1)
    {
        if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
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
        if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER, APE_OBJECT_ANY))
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
    CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER);
    return object_make_null();
}

static ApeObject_t cfn_append(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY, APE_OBJECT_ANY))
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

static ApeObject_t cfn_println(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;

    const ApeConfig_t* config = vm->config;

    if(!config->stdio.write.write)
    {
        return object_make_null();// todo: runtime error?
    }

    ApeStringBuffer_t* buf = strbuf_make(vm->alloc);
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

static ApeObject_t cfn_print(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    const ApeConfig_t* config = vm->config;

    if(!config->stdio.write.write)
    {
        return object_make_null();// todo: runtime error?
    }

    ApeStringBuffer_t* buf = strbuf_make(vm->alloc);
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

static ApeObject_t cfn_to_str(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING | APE_OBJECT_NUMBER | APE_OBJECT_BOOL | APE_OBJECT_NULL | APE_OBJECT_MAP | APE_OBJECT_ARRAY))
    {
        return object_make_null();
    }
    ApeObject_t arg = args[0];
    ApeStringBuffer_t* buf = strbuf_make(vm->alloc);
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

static ApeObject_t cfn_to_num(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING | APE_OBJECT_NUMBER | APE_OBJECT_BOOL | APE_OBJECT_NULL))
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
    else if(object_get_type(args[0]) == APE_OBJECT_STRING)
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

static ApeObject_t cfn_chr(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }

    double val = object_get_number(args[0]);

    char c = (char)val;
    char str[2] = { c, '\0' };
    return object_make_string(vm->mem, str);
}

static ApeObject_t cfn_range(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    for(int i = 0; i < argc; i++)
    {
        ApeObjectType_t type = object_get_type(args[i]);
        if(type != APE_OBJECT_NUMBER)
        {
            const char* type_str = object_get_type_name(type);
            const char* expected_str = object_get_type_name(APE_OBJECT_NUMBER);
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

static ApeObject_t cfn_keys(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_MAP))
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

static ApeObject_t cfn_values(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_MAP))
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

static ApeObject_t cfn_copy(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_copy(vm->mem, args[0]);
}

static ApeObject_t cfn_deep_copy(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_deep_copy(vm->mem, args[0]);
}

static ApeObject_t cfn_concat(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY | APE_OBJECT_STRING, APE_OBJECT_ARRAY | APE_OBJECT_STRING))
    {
        return object_make_null();
    }
    ApeObjectType_t type = object_get_type(args[0]);
    ApeObjectType_t item_type = object_get_type(args[1]);
    if(type == APE_OBJECT_ARRAY)
    {
        if(item_type != APE_OBJECT_ARRAY)
        {
            const char* item_type_str = object_get_type_name(item_type);
            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "Invalid argument 2 passed to concat, got %s", item_type_str);
            return object_make_null();
        }
        for(int i = 0; i < object_get_array_length(args[1]); i++)
        {
            ApeObject_t item = object_get_array_value(args[1], i);
            bool ok = object_add_array_value(args[0], item);
            if(!ok)
            {
                return object_make_null();
            }
        }
        return object_make_number(object_get_array_length(args[0]));
    }
    else if(type == APE_OBJECT_STRING)
    {
        if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING, APE_OBJECT_STRING))
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

static ApeObject_t cfn_remove(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY, APE_OBJECT_ANY))
    {
        return object_make_null();
    }

    int ix = -1;
    for(int i = 0; i < object_get_array_length(args[0]); i++)
    {
        ApeObject_t obj = object_get_array_value(args[0], i);
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

static ApeObject_t cfn_remove_at(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }

    ApeObjectType_t type = object_get_type(args[0]);
    int ix = (int)object_get_number(args[1]);

    switch(type)
    {
        case APE_OBJECT_ARRAY:
        {
            bool res = object_remove_array_value_at(args[0], ix);
            return object_make_bool(res);
        }
        default:
            break;
    }

    return object_make_bool(true);
}


static ApeObject_t cfn_error(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(argc == 1 && object_get_type(args[0]) == APE_OBJECT_STRING)
    {
        return object_make_error(vm->mem, object_get_string(args[0]));
    }
    else
    {
        return object_make_error(vm->mem, "");
    }
}

static ApeObject_t cfn_crash(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(argc == 1 && object_get_type(args[0]) == APE_OBJECT_STRING)
    {
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), object_get_string(args[0]));
    }
    else
    {
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "");
    }
    return object_make_null();
}

static ApeObject_t cfn_assert(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_BOOL))
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

static ApeObject_t cfn_random_seed(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    int seed = (int)object_get_number(args[0]);
    srand(seed);
    return object_make_bool(true);
}

static ApeObject_t cfn_random(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    double res = (double)rand() / RAND_MAX;
    if(argc == 0)
    {
        return object_make_number(res);
    }
    else if(argc == 2)
    {
        if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER, APE_OBJECT_NUMBER))
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

static ApeObject_t cfn_slice(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING | APE_OBJECT_ARRAY, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    ApeObjectType_t arg_type = object_get_type(args[0]);
    int index = (int)object_get_number(args[1]);
    if(arg_type == APE_OBJECT_ARRAY)
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
            ApeObject_t item = object_get_array_value(args[0], i);
            bool ok = object_add_array_value(res, item);
            if(!ok)
            {
                return object_make_null();
            }
        }
        return res;
    }
    else if(arg_type == APE_OBJECT_STRING)
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

static ApeObject_t cfn_is_string(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_STRING);
}

static ApeObject_t cfn_is_array(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_ARRAY);
}

static ApeObject_t cfn_is_map(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_MAP);
}

static ApeObject_t cfn_is_number(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_NUMBER);
}

static ApeObject_t cfn_is_bool(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_BOOL);
}

static ApeObject_t cfn_is_null(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_NULL);
}

static ApeObject_t cfn_is_function(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_FUNCTION);
}

static ApeObject_t cfn_is_external(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_EXTERNAL);
}

static ApeObject_t cfn_is_error(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_ERROR);
}

static ApeObject_t cfn_is_native_function(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return object_make_null();
    }
    return object_make_bool(object_get_type(args[0]) == APE_OBJECT_NATIVE_FUNCTION);
}

//-----------------------------------------------------------------------------
// Math
//-----------------------------------------------------------------------------

static ApeObject_t cfn_sqrt(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = sqrt(arg);
    return object_make_number(res);
}

static ApeObject_t cfn_pow(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg1 = object_get_number(args[0]);
    double arg2 = object_get_number(args[1]);
    double res = pow(arg1, arg2);
    return object_make_number(res);
}

static ApeObject_t cfn_sin(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = sin(arg);
    return object_make_number(res);
}

static ApeObject_t cfn_cos(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = cos(arg);
    return object_make_number(res);
}

static ApeObject_t cfn_tan(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = tan(arg);
    return object_make_number(res);
}

static ApeObject_t cfn_log(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = log(arg);
    return object_make_number(res);
}

static ApeObject_t cfn_ceil(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = ceil(arg);
    return object_make_number(res);
}

static ApeObject_t cfn_floor(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = floor(arg);
    return object_make_number(res);
}

static ApeObject_t cfn_abs(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return object_make_null();
    }
    double arg = object_get_number(args[0]);
    double res = fabs(arg);
    return object_make_number(res);
}


static ApeObject_t cfn_file_write(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING, APE_OBJECT_STRING))
    {
        return object_make_null();
    }

    const ApeConfig_t* config = vm->config;

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

static ApeObject_t cfn_file_read(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
    {
        return object_make_null();
    }

    const ApeConfig_t* config = vm->config;

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

static ApeObject_t cfn_file_isfile(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    const char* path;
    struct stat st;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
    {
        return object_make_bool(false);
    }
    path = object_get_string(args[0]);
    if(stat(path, &st) != -1)
    {
        if((st.st_mode & S_IFMT) == S_IFREG)
        {
            return object_make_bool(true);
        }
    }
    return object_make_bool(false);
}

static ApeObject_t cfn_file_isdirectory(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    const char* path;
    struct stat st;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
    {
        return object_make_bool(false);
    }
    path = object_get_string(args[0]);
    if(stat(path, &st) != -1)
    {
        if((st.st_mode & S_IFMT) == S_IFDIR)
        {
            return object_make_bool(true);
        }
    }
    return object_make_bool(false);
}

static ApeObject_t timespec_to_map(ApeVM_t* vm, struct timespec ts)
{
    ApeObject_t map;
    map = object_make_map(vm->mem);
    ape_object_set_map_number(map, "sec", ts.tv_sec);
    ape_object_set_map_number(map, "nsec", ts.tv_nsec);
    return map;
}

static ApeObject_t cfn_file_stat(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    const char* path;
    struct stat st;
    ApeObject_t map;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
    {
        return object_make_bool(false);
    }
    path = object_get_string(args[0]);
    if(stat(path, &st) != -1)
    {
        map = object_make_map(vm->mem);
        //ape_object_add_array_string(ary, dent->d_name);
        ape_object_set_map_string(map, "name", path);
        ape_object_set_map_string(map, "path", path);
        ape_object_set_map_number(map, "dev", st.st_dev);
        ape_object_set_map_number(map, "inode", st.st_ino);
        ape_object_set_map_number(map, "mode", st.st_mode);
        ape_object_set_map_number(map, "nlink", st.st_nlink);
        ape_object_set_map_number(map, "uid", st.st_uid);
        ape_object_set_map_number(map, "gid", st.st_gid);
        ape_object_set_map_number(map, "rdev", st.st_rdev);
        ape_object_set_map_number(map, "size", st.st_size);
        ape_object_set_map_number(map, "blksize", st.st_blksize);
        ape_object_set_map_number(map, "blocks", st.st_blocks);
        ape_object_set_map_value(map, "atim", timespec_to_map(vm, st.st_atim));
        ape_object_set_map_value(map, "mtim", timespec_to_map(vm, st.st_mtim));
        ape_object_set_map_value(map, "ctim", timespec_to_map(vm, st.st_ctim));
        return map;
    }
    return object_make_null();
}

static ApeObject_t objfn_string_length(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    ApeObject_t self;
    self = args[0];
    return object_make_number(object_get_string_length(self));
}


#if !defined(CORE_NODIRENT)

static ApeObject_t cfn_dir_readdir(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    bool isdir;
    bool isfile;
    const char* path;
    DIR* hnd;
    struct dirent* dent;
    ApeObject_t ary;
    ApeObject_t sobj;
    ApeObject_t subm;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
    {
        return object_make_null();
    }
    path = object_get_string(args[0]);
    hnd = opendir(path);
    if(hnd == NULL)
    {
        return object_make_null();
    }
    ary = object_make_array(vm->mem);
    while(true)
    {
        dent = readdir(hnd);
        if(dent == NULL)
        {
            break;
        }
        isfile = (dent->d_type == DT_REG);
        isdir = (dent->d_type == DT_DIR);
        subm = object_make_map(vm->mem);
        //ape_object_add_array_string(ary, dent->d_name);
        ape_object_set_map_string(subm, "name", dent->d_name);
        ape_object_set_map_number(subm, "ino", dent->d_ino);
        ape_object_set_map_number(subm, "type", dent->d_type);
        ape_object_set_map_bool(subm, "isfile", isfile);
        ape_object_set_map_bool(subm, "isdir", isdir);
        object_add_array_value(ary, subm);
    }
    closedir(hnd);
    return ary;
}

#endif

static bool check_args(ApeVM_t* vm, bool generate_error, int argc, ApeObject_t* args, int expected_argc, ApeObjectType_t* expected_types)
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
        ApeObjectType_t type = object_get_type(arg);
        ApeObjectType_t expected_type = expected_types[i];
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

static NatFunc_t g_core_globalfuncs[] =
{
    { "len", cfn_len },
    { "println", cfn_println },
    { "print", cfn_print },
    { "first", cfn_first },
    { "last", cfn_last },
    { "rest", cfn_rest },
    { "append", cfn_append },
    { "remove", cfn_remove },
    { "remove_at", cfn_remove_at },
    { "to_str", cfn_to_str },
    { "to_num", cfn_to_num },
    { "range", cfn_range },
    { "keys", cfn_keys },
    { "values", cfn_values },
    { "copy", cfn_copy },
    { "deep_copy", cfn_deep_copy },
    { "concat", cfn_concat },
    { "chr", cfn_chr },
    { "reverse", cfn_reverse },
    { "array", cfn_array },
    { "error", cfn_error },
    { "crash", cfn_crash },
    { "assert", cfn_assert },
    { "random_seed", cfn_random_seed },
    { "random", cfn_random },
    { "slice", cfn_slice },

    // Type checks
    { "is_string", cfn_is_string },
    { "is_array", cfn_is_array },
    { "is_map", cfn_is_map },
    { "is_number", cfn_is_number },
    { "is_bool", cfn_is_bool },
    { "is_null", cfn_is_null },
    { "is_function", cfn_is_function },
    { "is_external", cfn_is_external },
    { "is_error", cfn_is_error },
    { "is_native_function", cfn_is_native_function },

    // Math
    { "sqrt", cfn_sqrt },
    { "pow", cfn_pow },
    { "sin", cfn_sin },
    { "cos", cfn_cos },
    { "tan", cfn_tan },
    { "log", cfn_log },
    { "ceil", cfn_ceil },
    { "floor", cfn_floor },
    { "abs", cfn_abs },

};

static NatFunc_t g_core_filefuncs[]=
{
    {"read", cfn_file_read},
    {"write", cfn_file_write},
    {"isfile", cfn_file_isfile},
    {"isdirectory", cfn_file_isdirectory},
    {"stat", cfn_file_stat},
    {NULL, NULL},
};

static NatFunc_t g_core_dirfuncs[]=
{
    // filesystem
    #if !defined(CORE_NODIRENT)
    {"read", cfn_dir_readdir},
    #endif
    {NULL, NULL},
};

void builtins_install(ApeVM_t* vm)
{
    int i;
    ApeObject_t map;
    {
        map = object_make_map(vm->mem);
        for(i=0; g_core_dirfuncs[i].name != NULL; i++)
        {
            make_fn_entry(vm, map, g_core_dirfuncs[i].name, g_core_dirfuncs[i].fn);
        }
        global_store_set(vm->global_store, "Dir", map);
    }
    {
        map = object_make_map(vm->mem);
        for(i=0; g_core_filefuncs[i].name != NULL; i++)
        {
            make_fn_entry(vm, map, g_core_filefuncs[i].name, g_core_filefuncs[i].fn);
        }
        global_store_set(vm->global_store, "File", map);
    }
}

int builtins_count()
{
    return APE_ARRAY_LEN(g_core_globalfuncs);
}

ApeNativeFNCallback_t builtins_get_fn(int ix)
{
    return g_core_globalfuncs[ix].fn;
}

const char* builtins_get_name(int ix)
{
    return g_core_globalfuncs[ix].name;
}

ApeNativeFNCallback_t builtin_get_object(ApeObjectType_t objt, const char* idxname)
{
    if(objt == APE_OBJECT_STRING)
    {
        if(APE_STREQ(idxname, "length"))
        {
            return objfn_string_length;
        }
    }
    return NULL;
}
