
#include <stdarg.h>
#include <sys/stat.h>
#if __has_include(<dirent.h>)
    #include <dirent.h>
#else
    #define CORE_NODIRENT
#endif

#include "ape.h"

#if 0
    #define CHECK_ARGS(vm, generate_error, argc, args, ...) \
        check_args( \
            (vm), \
            (generate_error), \
            (argc), \
            (args),  \
            (sizeof((ApeObjType_t[]){ __VA_ARGS__ }) / sizeof(ApeObjType_t)), \
            ((ApeObjType_t*)((ApeObjType_t[]){ __VA_ARGS__ })) \
        )
#else
    #define CHECK_ARGS(vm, generate_error, argc, args, ...) \
        check_args( \
            (vm), \
            (generate_error), \
            (argc), \
            (args),  \
            (sizeof((int[]){ __VA_ARGS__ }) / sizeof(int)), \
            typargs_to_array(__VA_ARGS__, -1) \
        )
#endif

//ApeObject_t object_make_native_function(ApeGCMemory_t* mem, const char* name, ApeNativeFunc_t fn, void* data, int data_len);
#define make_fn_data(ctx, name, fnc, dataptr, datasize) \
    ape_object_make_nativefuncmemory(ctx, name, fnc, dataptr, datasize)

#define make_fn(ctx, name, fnc) \
    make_fn_data(ctx, name, fnc, NULL, 0)

#define make_fn_entry_data(ctx, map, name, fnc, dataptr, datasize) \
    ape_object_map_setnamedvalue(map, name, make_fn(ctx, name, fnc))

#define make_fn_entry(ctx, map, name, fnc) \
    make_fn_entry_data(ctx, map, name, fnc, NULL, 0)

typedef struct NatFunc_t NatFunc_t;
struct NatFunc_t
{
    const char* name;
    ApeNativeFunc_t fn;
};

static const ApePosition_t g_bltpriv_src_pos_invalid = { NULL, -1, -1 };

static ApeObjType_t* typargs_to_array(int first, ...)
{
    int idx;
    int thing;
    va_list va;
    static ApeObjType_t rt[20];
    idx = 0;
    rt[idx] = (ApeObjType_t)first;
    va_start(va, first);
    idx++;
    while(true)
    {
        thing = va_arg(va, int);
        if(thing == -1)
        {
            break;
        }
        rt[idx] = (ApeObjType_t)thing;
        idx++;
    }
    va_end(va);
    return rt;
}

static bool check_args(ApeVM_t* vm, bool generate_error, ApeSize_t argc, ApeObject_t* args, ApeSize_t expected_argc, const ApeObjType_t* expected_types)
{
    ApeSize_t i;
    if(argc != expected_argc)
    {
        if(generate_error)
        {
            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, g_bltpriv_src_pos_invalid,
                              "invalid number of arguments, got %d instead of %d", argc, expected_argc);
        }
        return false;
    }

    for(i = 0; i < argc; i++)
    {
        ApeObject_t arg = args[i];
        ApeObjType_t type = ape_object_value_type(arg);
        ApeObjType_t expected_type = expected_types[i];
        if(!(type & expected_type))
        {
            if(generate_error)
            {
                const char* type_str = ape_object_value_typename(type);
                char* expected_type_str = ape_object_value_typeunionname(vm->context, expected_type);
                if(!expected_type_str)
                {
                    return false;
                }
                ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, g_bltpriv_src_pos_invalid,
                                  "invalid argument %d type, got %s, expected %s", i, type_str, expected_type_str);
                ape_allocator_free(vm->alloc, expected_type_str);
            }
            return false;
        }
    }
    return true;
}

// INTERNAL
static ApeObject_t cfn_object_length(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING | APE_OBJECT_ARRAY | APE_OBJECT_MAP))
    {
        return ape_object_make_null(vm->context);
    }

    ApeObject_t arg = args[0];
    ApeObjType_t type = ape_object_value_type(arg);
    if(type == APE_OBJECT_STRING)
    {
        ApeSize_t len = ape_object_string_getlength(arg);
        return ape_object_make_number(vm->context, len);
    }
    else if(type == APE_OBJECT_ARRAY)
    {
        ApeSize_t len = ape_object_array_getlength(arg);
        return ape_object_make_number(vm->context, len);
    }
    else if(type == APE_OBJECT_MAP)
    {
        ApeSize_t len = ape_object_map_getlength(arg);
        return ape_object_make_number(vm->context, len);
    }

    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_object_first(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY))
    {
        return ape_object_make_null(vm->context);
    }
    ApeObject_t arg = args[0];
    return ape_object_array_getvalue(arg, 0);
}

static ApeObject_t cfn_object_last(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY))
    {
        return ape_object_make_null(vm->context);
    }
    ApeObject_t arg = args[0];
    return ape_object_array_getvalue(arg, ape_object_array_getlength(arg) - 1);
}

static ApeObject_t cfn_object_rest(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t i;
    ApeSize_t len;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY))
    {
        return ape_object_make_null(vm->context);
    }
    ApeObject_t arg = args[0];
    len = ape_object_array_getlength(arg);
    if(len == 0)
    {
        return ape_object_make_null(vm->context);
    }
    ApeObject_t res = ape_object_make_array(vm->context);
    if(ape_object_value_isnull(res))
    {
        return ape_object_make_null(vm->context);
    }
    for(i = 1; i < len; i++)
    {
        ApeObject_t item = ape_object_array_getvalue(arg, i);
        bool ok = ape_object_array_pushvalue(res, item);
        if(!ok)
        {
            return ape_object_make_null(vm->context);
        }
    }
    return res;
}

static ApeObject_t cfn_reverse(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t i;
    ApeSize_t len;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY | APE_OBJECT_STRING))
    {
        return ape_object_make_null(vm->context);
    }
    ApeObject_t arg = args[0];
    ApeObjType_t type = ape_object_value_type(arg);
    if(type == APE_OBJECT_ARRAY)
    {
        len = ape_object_array_getlength(arg);
        ApeObject_t res = ape_object_make_arraycapacity(vm->context, len);
        if(ape_object_value_isnull(res))
        {
            return ape_object_make_null(vm->context);
        }
        for(i = 0; i < len; i++)
        {
            ApeObject_t obj = ape_object_array_getvalue(arg, i);
            bool ok = ape_object_array_setat(res, len - i - 1, obj);
            if(!ok)
            {
                return ape_object_make_null(vm->context);
            }
        }
        return res;
    }
    else if(type == APE_OBJECT_STRING)
    {
        const char* str = ape_object_string_getdata(arg);
        len = ape_object_string_getlength(arg);
        ApeObject_t res = ape_object_make_stringcapacity(vm->context, len);
        if(ape_object_value_isnull(res))
        {
            return ape_object_make_null(vm->context);
        }
        char* res_buf = ape_object_string_getmutable(res);
        for(i = 0; i < len; i++)
        {
            res_buf[len - i - 1] = str[i];
        }
        res_buf[len] = '\0';
        ape_object_string_setlength(res, len);
        return res;
    }
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_array(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t i;
    ApeSize_t capacity;
    (void)data;
    if(argc == 1)
    {
        if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
        {
            return ape_object_make_null(vm->context);
        }
        capacity = (ApeSize_t)ape_object_value_asnumber(args[0]);
        ApeObject_t res = ape_object_make_arraycapacity(vm->context, capacity);
        if(ape_object_value_isnull(res))
        {
            return ape_object_make_null(vm->context);
        }
        ApeObject_t obj_null = ape_object_make_null(vm->context);
        for(i = 0; i < capacity; i++)
        {
            bool ok = ape_object_array_pushvalue(res, obj_null);
            if(!ok)
            {
                return ape_object_make_null(vm->context);
            }
        }
        return res;
    }
    else if(argc == 2)
    {
        if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER, APE_OBJECT_ANY))
        {
            return ape_object_make_null(vm->context);
        }
        capacity = (ApeSize_t)ape_object_value_asnumber(args[0]);
        ApeObject_t res = ape_object_make_arraycapacity(vm->context, capacity);
        if(ape_object_value_isnull(res))
        {
            return ape_object_make_null(vm->context);
        }
        for(i = 0; i < capacity; i++)
        {
            bool ok = ape_object_array_pushvalue(res, args[1]);
            if(!ok)
            {
                return ape_object_make_null(vm->context);
            }
        }
        return res;
    }
    CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER);
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_object_append(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }
    bool ok = ape_object_array_pushvalue(args[0], args[1]);
    if(!ok)
    {
        return ape_object_make_null(vm->context);
    }
    ApeSize_t len = ape_object_array_getlength(args[0]);
    return ape_object_make_number(vm->context, len);
}

static ApeObject_t cfn_println(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t i;
    (void)data;
    const ApeConfig_t* config = vm->config;
    if(!config->stdio.write.write)
    {
        return ape_object_make_null(vm->context);// todo: runtime error?
    }
    ApeWriter_t* buf = ape_make_writer(vm->context);
    if(!buf)
    {
        return ape_object_make_null(vm->context);
    }
    for(i = 0; i < argc; i++)
    {
        ApeObject_t arg = args[i];
        ape_tostring_object(arg, buf, false);
    }
    ape_writer_append(buf, "\n");
    if(ape_writer_failed(buf))
    {
        ape_writer_destroy(buf);
        return ape_object_make_null(vm->context);
    }
    config->stdio.write.write(config->stdio.write.context, ape_writer_getdata(buf), ape_writer_getlength(buf));
    ape_writer_destroy(buf);
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_print(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t i;
    (void)data;
    const ApeConfig_t* config = vm->config;

    if(!config->stdio.write.write)
    {
        return ape_object_make_null(vm->context);// todo: runtime error?
    }
    ApeWriter_t* buf = ape_make_writer(vm->context);
    if(!buf)
    {
        return ape_object_make_null(vm->context);
    }
    for(i = 0; i < argc; i++)
    {
        ApeObject_t arg = args[i];
        ape_tostring_object(arg, buf, false);
    }
    if(ape_writer_failed(buf))
    {
        ape_writer_destroy(buf);
        return ape_object_make_null(vm->context);
    }
    config->stdio.write.write(config->stdio.write.context, ape_writer_getdata(buf), ape_writer_getlength(buf));
    ape_writer_destroy(buf);
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_to_str(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING | APE_OBJECT_NUMBER | APE_OBJECT_BOOL | APE_OBJECT_NULL | APE_OBJECT_MAP | APE_OBJECT_ARRAY))
    {
        return ape_object_make_null(vm->context);
    }
    ApeObject_t arg = args[0];
    ApeWriter_t* buf = ape_make_writer(vm->context);
    if(!buf)
    {
        return ape_object_make_null(vm->context);
    }
    ape_tostring_object(arg, buf, false);
    if(ape_writer_failed(buf))
    {
        ape_writer_destroy(buf);
        return ape_object_make_null(vm->context);
    }
    ApeObject_t res = ape_object_make_string(vm->context, ape_writer_getdata(buf));
    ape_writer_destroy(buf);
    return res;
}

static ApeObject_t cfn_to_num(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING | APE_OBJECT_NUMBER | APE_OBJECT_BOOL | APE_OBJECT_NULL))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat_t result = 0;
    const char* string = "";
    if(ape_object_value_isnumeric(args[0]))
    {
        result = ape_object_value_asnumber(args[0]);
    }
    else if(ape_object_value_isnull(args[0]))
    {
        result = 0;
    }
    else if(ape_object_value_type(args[0]) == APE_OBJECT_STRING)
    {
        string = ape_object_string_getdata(args[0]);
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
        ApeSize_t string_len = ape_object_string_getlength(args[0]);
        ApeSize_t parsed_len = (ApeSize_t)(end - string);
        if(string_len != parsed_len)
        {
            goto err;
        }
    }
    else
    {
        goto err;
    }
    return ape_object_make_number(vm->context, result);
err:
    ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, g_bltpriv_src_pos_invalid, "cannot convert '%s' to number", string);
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_chr(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }

    ApeFloat_t val = ape_object_value_asnumber(args[0]);

    char c = (char)val;
    char str[2] = { c, '\0' };
    return ape_object_make_string(vm->context, str);
}


static ApeObject_t cfn_ord(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING | APE_OBJECT_NULL))
    {
        return ape_object_make_null(vm->context);
    }
    if(ape_object_value_isnull(args[0]))
    {
        return ape_object_make_number(vm->context, 0);
    }
    const char* str = ape_object_string_getdata(args[0]);
    return ape_object_make_number(vm->context, str[0]);
}


static ApeObject_t cfn_substr(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeInt_t begin;
    ApeInt_t end;
    ApeInt_t len;
    ApeInt_t nlen;
    const char* str;
    (void)data;
    if(argc < 1)
    {
        return ape_object_make_error(vm->context, "too few args to substr()");
    }
    if((ape_object_value_type(args[0]) != APE_OBJECT_STRING) || (ape_object_value_type(args[1]) != APE_OBJECT_NUMBER))
    {
        return ape_object_make_error(vm->context, "substr() expects string, number [, number]");
    }
    str = ape_object_string_getdata(args[0]);
    len = ape_object_string_getlength(args[0]);
    begin = ape_object_value_asnumber(args[1]);
    end = len;
    if(argc > 2)
    {
        if(ape_object_value_type(args[2]) != APE_OBJECT_NUMBER)
        {
            return ape_object_make_error(vm->context, "last argument must be number");
        }
        end = ape_object_value_asnumber(args[2]);
    }
    // fixme: this is actually incorrect
    if((begin >= len))
    {
        return ape_object_make_null(vm->context);
    }
    if(end >= len)
    {
        end = len;
    }
    nlen = end - begin;
    fprintf(stderr, "substr: len=%d, begin=%d, end=%d, nlen=%d\n", (int)len, (int)begin, (int)end, (int)nlen);
    return ape_object_make_stringlen(vm->context, str+begin, nlen);
}

static ApeObject_t cfn_range(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeInt_t i;
    (void)data;
    for(i = 0; i < (ApeInt_t)argc; i++)
    {
        ApeObjType_t type = ape_object_value_type(args[i]);
        if(type != APE_OBJECT_NUMBER)
        {
            const char* type_str = ape_object_value_typename(type);
            const char* expected_str = ape_object_value_typename(APE_OBJECT_NUMBER);
            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, g_bltpriv_src_pos_invalid,
                              "invalid argument %d passed to range, got %s instead of %s", i, type_str, expected_str);
            return ape_object_make_null(vm->context);
        }
    }
    ApeInt_t start = 0;
    ApeInt_t end = 0;
    ApeInt_t step = 1;

    if(argc == 1)
    {
        end = (ApeInt_t)ape_object_value_asnumber(args[0]);
    }
    else if(argc == 2)
    {
        start = (ApeInt_t)ape_object_value_asnumber(args[0]);
        end = (ApeInt_t)ape_object_value_asnumber(args[1]);
    }
    else if(argc == 3)
    {
        start = (ApeInt_t)ape_object_value_asnumber(args[0]);
        end = (ApeInt_t)ape_object_value_asnumber(args[1]);
        step = (ApeInt_t)ape_object_value_asnumber(args[2]);
    }
    else
    {
        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, g_bltpriv_src_pos_invalid, "invalid number of arguments passed to range, got %d", argc);
        return ape_object_make_null(vm->context);
    }

    if(step == 0)
    {
        ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, g_bltpriv_src_pos_invalid, "range step cannot be 0");
        return ape_object_make_null(vm->context);
    }

    ApeObject_t res = ape_object_make_array(vm->context);
    if(ape_object_value_isnull(res))
    {
        return ape_object_make_null(vm->context);
    }
    for(i = start; i < end; i += step)
    {
        ApeObject_t item = ape_object_make_number(vm->context, i);
        bool ok = ape_object_array_pushvalue(res, item);
        if(!ok)
        {
            return ape_object_make_null(vm->context);
        }
    }
    return res;
}

static ApeObject_t cfn_object_objkeys(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t i;
    ApeSize_t len;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_MAP))
    {
        return ape_object_make_null(vm->context);
    }
    ApeObject_t arg = args[0];
    ApeObject_t res = ape_object_make_array(vm->context);
    if(ape_object_value_isnull(res))
    {
        return ape_object_make_null(vm->context);
    }
    len = ape_object_map_getlength(arg);
    for(i = 0; i < len; i++)
    {
        ApeObject_t key = ape_object_map_getkeyat(arg, i);
        bool ok = ape_object_array_pushvalue(res, key);
        if(!ok)
        {
            return ape_object_make_null(vm->context);
        }
    }
    return res;
}

static ApeObject_t cfn_object_objvalues(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t i;
    ApeSize_t len;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_MAP))
    {
        return ape_object_make_null(vm->context);
    }
    ApeObject_t arg = args[0];
    ApeObject_t res = ape_object_make_array(vm->context);
    if(ape_object_value_isnull(res))
    {
        return ape_object_make_null(vm->context);
    }
    len = ape_object_map_getlength(arg);
    for(i = 0; i < len; i++)
    {
        ApeObject_t key = ape_object_map_getvalueat(arg, i);
        bool ok = ape_object_array_pushvalue(res, key);
        if(!ok)
        {
            return ape_object_make_null(vm->context);
        }
    }
    return res;
}

static ApeObject_t cfn_object_copy(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }
    return ape_object_value_copyflat(vm->context, args[0]);
}

static ApeObject_t cfn_object_deepcopy(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }
    return ape_object_value_copydeep(vm->context, args[0]);
}

static ApeObject_t cfn_concat(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t i;
    ApeSize_t left_len;
    ApeSize_t right_len;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY | APE_OBJECT_STRING, APE_OBJECT_ARRAY | APE_OBJECT_STRING))
    {
        return ape_object_make_null(vm->context);
    }
    ApeObjType_t type = ape_object_value_type(args[0]);
    ApeObjType_t item_type = ape_object_value_type(args[1]);
    if(type == APE_OBJECT_ARRAY)
    {
        if(item_type != APE_OBJECT_ARRAY)
        {
            const char* item_type_str = ape_object_value_typename(item_type);
            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, g_bltpriv_src_pos_invalid, "invalid argument 2 passed to concat, got %s", item_type_str);
            return ape_object_make_null(vm->context);
        }
        for(i = 0; i < ape_object_array_getlength(args[1]); i++)
        {
            ApeObject_t item = ape_object_array_getvalue(args[1], i);
            bool ok = ape_object_array_pushvalue(args[0], item);
            if(!ok)
            {
                return ape_object_make_null(vm->context);
            }
        }
        return ape_object_make_number(vm->context, ape_object_array_getlength(args[0]));
    }
    else if(type == APE_OBJECT_STRING)
    {
        if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING, APE_OBJECT_STRING))
        {
            return ape_object_make_null(vm->context);
        }
        const char* left_val = ape_object_string_getdata(args[0]);
        left_len = (ApeSize_t)ape_object_string_getlength(args[0]);
        const char* right_val = ape_object_string_getdata(args[1]);
        right_len = (ApeSize_t)ape_object_string_getlength(args[1]);
        ApeObject_t res = ape_object_make_stringcapacity(vm->context, left_len + right_len);
        if(ape_object_value_isnull(res))
        {
            return ape_object_make_null(vm->context);
        }

        bool ok = ape_object_string_append(res, left_val, left_len);
        if(!ok)
        {
            return ape_object_make_null(vm->context);
        }

        ok = ape_object_string_append(res, right_val, right_len);
        if(!ok)
        {
            return ape_object_make_null(vm->context);
        }

        return res;
    }
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_object_removekey(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t i;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }

    ApeInt_t ix = -1;
    for(i = 0; i < ape_object_array_getlength(args[0]); i++)
    {
        ApeObject_t obj = ape_object_array_getvalue(args[0], i);
        if(ape_object_value_equals(obj, args[1]))
        {
            ix = i;
            break;
        }
    }
    if(ix == -1)
    {
        return ape_object_make_bool(vm->context, false);
    }
    bool res = ape_object_array_removeat(args[0], ix);
    return ape_object_make_bool(vm->context, res);
}

static ApeObject_t cfn_object_removeat(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }

    ApeObjType_t type = ape_object_value_type(args[0]);
    ApeSize_t ix = (ApeSize_t)ape_object_value_asnumber(args[1]);

    switch(type)
    {
        case APE_OBJECT_ARRAY:
        {
            bool res = ape_object_array_removeat(args[0], ix);
            return ape_object_make_bool(vm->context, res);
        }
        default:
            break;
    }

    return ape_object_make_bool(vm->context, true);
}


static ApeObject_t cfn_error(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(argc == 1 && ape_object_value_type(args[0]) == APE_OBJECT_STRING)
    {
        return ape_object_make_error(vm->context, ape_object_string_getdata(args[0]));
    }
    else
    {
        return ape_object_make_error(vm->context, "");
    }
}

static ApeObject_t cfn_crash(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(argc == 1 && ape_object_value_type(args[0]) == APE_OBJECT_STRING)
    {
        ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame), ape_object_string_getdata(args[0]));
    }
    else
    {
        ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->current_frame), "");
    }
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_assert(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_BOOL))
    {
        return ape_object_make_null(vm->context);
    }

    if(!ape_object_value_asbool(args[0]))
    {
        ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, g_bltpriv_src_pos_invalid, "assertion failed");
        return ape_object_make_null(vm->context);
    }

    return ape_object_make_bool(vm->context, true);
}

static ApeObject_t cfn_random_seed(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeSize_t seed = (ApeSize_t)ape_object_value_asnumber(args[0]);
    srand(seed);
    return ape_object_make_bool(vm->context, true);
}

static ApeObject_t cfn_random(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeFloat_t res = (ApeFloat_t)rand() / RAND_MAX;
    if(argc == 0)
    {
        return ape_object_make_number(vm->context, res);
    }
    else if(argc == 2)
    {
        if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER, APE_OBJECT_NUMBER))
        {
            return ape_object_make_null(vm->context);
        }
        ApeFloat_t min = ape_object_value_asnumber(args[0]);
        ApeFloat_t max = ape_object_value_asnumber(args[1]);
        if(min >= max)
        {
            ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, g_bltpriv_src_pos_invalid, "max is bigger than min");
            return ape_object_make_null(vm->context);
        }
        ApeFloat_t range = max - min;
        res = min + (res * range);
        return ape_object_make_number(vm->context, res);
    }
    else
    {
        ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, g_bltpriv_src_pos_invalid, "invalid number or arguments");
        return ape_object_make_null(vm->context);
    }
}

static ApeObject_t cfn_object_slice(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeInt_t i;
    ApeInt_t len;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING | APE_OBJECT_ARRAY, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeObjType_t arg_type = ape_object_value_type(args[0]);
    ApeInt_t index = (ApeInt_t)ape_object_value_asnumber(args[1]);
    if(arg_type == APE_OBJECT_ARRAY)
    {
        len = ape_object_array_getlength(args[0]);
        if(index < 0)
        {
            index = len + index;
            if(index < 0)
            {
                index = 0;
            }
        }
        ApeObject_t res = ape_object_make_arraycapacity(vm->context, len - index);
        if(ape_object_value_isnull(res))
        {
            return ape_object_make_null(vm->context);
        }
        for(i = index; i < len; i++)
        {
            ApeObject_t item = ape_object_array_getvalue(args[0], i);
            bool ok = ape_object_array_pushvalue(res, item);
            if(!ok)
            {
                return ape_object_make_null(vm->context);
            }
        }
        return res;
    }
    else if(arg_type == APE_OBJECT_STRING)
    {
        const char* str = ape_object_string_getdata(args[0]);
        len = (ApeSize_t)ape_object_string_getlength(args[0]);
        if(index < 0)
        {
            index = len + index;
            if(index < 0)
            {
                return ape_object_make_string(vm->context, "");
            }
        }
        if(index >= len)
        {
            return ape_object_make_string(vm->context, "");
        }
        ApeInt_t res_len = len - index;
        ApeObject_t res = ape_object_make_stringcapacity(vm->context, res_len);
        if(ape_object_value_isnull(res))
        {
            return ape_object_make_null(vm->context);
        }

        char* res_buf = ape_object_string_getmutable(res);
        memset(res_buf, 0, res_len + 1);
        for(i = index; i < len; i++)
        {
            char c = str[i];
            res_buf[i - index] = c;
        }
        ape_object_string_setlength(res, res_len);
        return res;
    }
    else
    {
        const char* type_str = ape_object_value_typename(arg_type);
        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, g_bltpriv_src_pos_invalid, "invalid argument 0 passed to slice, got %s instead", type_str);
        return ape_object_make_null(vm->context);
    }
}

//-----------------------------------------------------------------------------
// Type checks
//-----------------------------------------------------------------------------

static ApeObject_t cfn_object_isstring(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_STRING);
}

static ApeObject_t cfn_object_isarray(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_ARRAY);
}

static ApeObject_t cfn_object_ismap(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_MAP);
}

static ApeObject_t cfn_object_isnumber(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_NUMBER);
}

static ApeObject_t cfn_object_isbool(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_BOOL);
}

static ApeObject_t cfn_object_isnull(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_NULL);
}

static ApeObject_t cfn_object_isfunction(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_FUNCTION);
}

static ApeObject_t cfn_object_isexternal(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_EXTERNAL);
}

static ApeObject_t cfn_object_iserror(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_ERROR);
}

static ApeObject_t cfn_object_isnativefunction(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_NATIVE_FUNCTION);
}

//-----------------------------------------------------------------------------
// Math
//-----------------------------------------------------------------------------

static ApeObject_t cfn_sqrt(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat_t arg = ape_object_value_asnumber(args[0]);
    ApeFloat_t res = sqrt(arg);
    return ape_object_make_number(vm->context, res);
}

static ApeObject_t cfn_pow(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat_t arg1 = ape_object_value_asnumber(args[0]);
    ApeFloat_t arg2 = ape_object_value_asnumber(args[1]);
    ApeFloat_t res = pow(arg1, arg2);
    return ape_object_make_number(vm->context, res);
}

static ApeObject_t cfn_sin(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat_t arg = ape_object_value_asnumber(args[0]);
    ApeFloat_t res = sin(arg);
    return ape_object_make_number(vm->context, res);
}

static ApeObject_t cfn_cos(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat_t arg = ape_object_value_asnumber(args[0]);
    ApeFloat_t res = cos(arg);
    return ape_object_make_number(vm->context, res);
}

static ApeObject_t cfn_tan(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat_t arg = ape_object_value_asnumber(args[0]);
    ApeFloat_t res = tan(arg);
    return ape_object_make_number(vm->context, res);
}

static ApeObject_t cfn_log(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat_t arg = ape_object_value_asnumber(args[0]);
    ApeFloat_t res = log(arg);
    return ape_object_make_number(vm->context, res);
}

static ApeObject_t cfn_ceil(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat_t arg = ape_object_value_asnumber(args[0]);
    ApeFloat_t res = ceil(arg);
    return ape_object_make_number(vm->context, res);
}

static ApeObject_t cfn_floor(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat_t arg = ape_object_value_asnumber(args[0]);
    ApeFloat_t res = floor(arg);
    return ape_object_make_number(vm->context, res);
}

static ApeObject_t cfn_abs(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat_t arg = ape_object_value_asnumber(args[0]);
    ApeFloat_t res = fabs(arg);
    return ape_object_make_number(vm->context, res);
}


static ApeObject_t cfn_file_write(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING, APE_OBJECT_STRING))
    {
        return ape_object_make_null(vm->context);
    }

    const ApeConfig_t* config = vm->config;

    if(!config->fileio.write_file.write_file)
    {
        return ape_object_make_null(vm->context);
    }

    const char* path = ape_object_string_getdata(args[0]);
    const char* string = ape_object_string_getdata(args[1]);
    ApeSize_t string_len = ape_object_string_getlength(args[1]);

    ApeSize_t written = (ApeSize_t)config->fileio.write_file.write_file(config->fileio.write_file.context, path, string, string_len);

    return ape_object_make_number(vm->context, written);
}

static ApeObject_t cfn_file_read(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
    {
        return ape_object_make_null(vm->context);
    }

    const ApeConfig_t* config = vm->config;

    if(!config->fileio.read_file.read_file)
    {
        return ape_object_make_null(vm->context);
    }

    const char* path = ape_object_string_getdata(args[0]);

    char* contents = config->fileio.read_file.read_file(config->fileio.read_file.context, path);
    if(!contents)
    {
        return ape_object_make_null(vm->context);
    }
    ApeObject_t res = ape_object_make_string(vm->context, contents);
    ape_allocator_free(vm->alloc, contents);
    return res;
}

static ApeObject_t cfn_file_isfile(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    const char* path;
    struct stat st;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
    {
        return ape_object_make_bool(vm->context, false);
    }
    path = ape_object_string_getdata(args[0]);
    if(stat(path, &st) != -1)
    {
        if((st.st_mode & S_IFMT) == S_IFREG)
        {
            return ape_object_make_bool(vm->context, true);
        }
    }
    return ape_object_make_bool(vm->context, false);
}

static ApeObject_t cfn_file_isdirectory(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    const char* path;
    struct stat st;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
    {
        return ape_object_make_bool(vm->context, false);
    }
    path = ape_object_string_getdata(args[0]);
    if(stat(path, &st) != -1)
    {
        if((st.st_mode & S_IFMT) == S_IFDIR)
        {
            return ape_object_make_bool(vm->context, true);
        }
    }
    return ape_object_make_bool(vm->context, false);
}

#if defined(__linux__)
static ApeObject_t timespec_to_map(ApeVM_t* vm, struct timespec ts)
{
    ApeObject_t map;
    map = ape_object_make_map(vm->context);
    ape_object_map_setnamednumber(map, "sec", ts.tv_sec);
    ape_object_map_setnamednumber(map, "nsec", ts.tv_nsec);
    return map;
}
#endif

static ApeObject_t cfn_file_stat(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    bool specificfield;
    const char* path;
    const char* field;
    struct stat st;
    ApeObject_t objsecond;
    ApeObject_t objpath;
    ApeObject_t map;
    (void)data;
    field = NULL;
    specificfield = false;
    map.type = APE_OBJECT_NULL;
    map.handle = NULL;
    if((argc == 0) || !ape_object_value_isstring(args[0]))
    {
        ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, g_bltpriv_src_pos_invalid, "File.stat expects at least one string argument");
        return ape_object_make_null(vm->context);
    }
    if(argc > 1)
    {
        objsecond = args[1];
        if(!ape_object_value_isstring(objsecond))
        {
            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, g_bltpriv_src_pos_invalid, "File.stat expects second argumt to be a string");
            return ape_object_make_null(vm->context);
        }
        specificfield = true;
    }
    objpath = args[0];
    path = ape_object_string_getdata(objpath);
    if(stat(path, &st) != -1)
    {
        #define for_field(f, val, mapfn, retfn) \
            { \
                if(specificfield) \
                { \
                    if((field != NULL) && (strcmp(field, f) == 0)) \
                    { \
                        return retfn(vm->context, (val)); \
                    } \
                } \
                else \
                { \
                    mapfn(map, f, val); \
                } \
            }

        #define for_field_string(f, val) \
            for_field(f, val, ape_object_map_setnamedstring, ape_object_make_string)

        #define for_field_number(f, val) \
            for_field(f, val, ape_object_map_setnamednumber, ape_object_make_number)

        if(specificfield)
        {
            field = ape_object_string_getdata(objsecond);
        }
        else
        {
            map = ape_object_make_map(vm->context);
        }
        for_field_string("name", path);
        //ape_object_array_pushstring(ary, dent->d_name);
        for_field_string("name", path);
        for_field_string("path", path);
        for_field_number("dev", st.st_dev);
        for_field_number("inode", st.st_ino);
        for_field_number("mode", st.st_mode);
        for_field_number("nlink", st.st_nlink);
        for_field_number("uid", st.st_uid);
        for_field_number("gid", st.st_gid);
        for_field_number("rdev", st.st_rdev);
        for_field_number("size", st.st_size);
        #if defined(__linux__)
        for_field_number("blksize", st.st_blksize);
        for_field_number("blocks", st.st_blocks);
        if(map.handle != NULL)
        {
            ape_object_map_setnamedvalue(map, "atim", timespec_to_map(vm, st.st_atim));
            ape_object_map_setnamedvalue(map, "mtim", timespec_to_map(vm, st.st_mtim));
            ape_object_map_setnamedvalue(map, "ctim", timespec_to_map(vm, st.st_ctim));
        }
        #endif
        if(map.handle != NULL)
        {
            return map;
        }
        else
        {
            ape_errorlist_addformat(vm->errors, APE_ERROR_RUNTIME, g_bltpriv_src_pos_invalid, "File.stat secondary field '%s' does not exist", field);
        }
        return ape_object_make_null(vm->context);
    }
    return ape_object_make_null(vm->context);
}

static ApeObject_t objfn_string_length(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeObject_t self;
    (void)vm;
    (void)data;
    (void)argc;
    self = args[0];
    return ape_object_make_number(vm->context, ape_object_string_getlength(self));
}


#if !defined(CORE_NODIRENT)

static ApeObject_t cfn_dir_readdir(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    bool isdir;
    bool isfile;
    const char* path;
    DIR* hnd;
    struct dirent* dent;
    ApeObject_t ary;
    ApeObject_t subm;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
    {
        return ape_object_make_null(vm->context);
    }
    path = ape_object_string_getdata(args[0]);
    hnd = opendir(path);
    if(hnd == NULL)
    {
        return ape_object_make_null(vm->context);
    }
    ary = ape_object_make_array(vm->context);
    while(true)
    {
        dent = readdir(hnd);
        if(dent == NULL)
        {
            break;
        }
        isfile = (dent->d_type == DT_REG);
        isdir = (dent->d_type == DT_DIR);
        subm = ape_object_make_map(vm->context);
        //ape_object_array_pushstring(ary, dent->d_name);
        ape_object_map_setnamedstring(subm, "name", dent->d_name);
        ape_object_map_setnamednumber(subm, "ino", dent->d_ino);
        ape_object_map_setnamednumber(subm, "type", dent->d_type);
        ape_object_map_setnamedbool(subm, "isfile", isfile);
        ape_object_map_setnamedbool(subm, "isdir", isdir);
        ape_object_array_pushvalue(ary, subm);
    }
    closedir(hnd);
    return ary;
}

#endif

static ApeObject_t cfn_string_split(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    size_t i;
    size_t len;
    char c;
    const char* str;
    const char* delim;
    char* itm;
    ApeObject_t arr;
    ApePtrArray_t* parr;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING, APE_OBJECT_STRING))
    {
        return ape_object_make_null(vm->context);
    }
    str = ape_object_string_getdata(args[0]);
    delim = ape_object_string_getdata(args[1]);
    arr = ape_object_make_array(vm->context);
    if(ape_object_string_getlength(args[1]) == 0)
    {
        len = ape_object_string_getlength(args[0]);
        for(i=0; i<len; i++)
        {
            c = str[i];
            ape_object_array_pushvalue(arr, ape_object_make_stringlen(vm->context, &c, 1));
        }
    }
    else
    {
        parr = ape_util_splitstring(vm->context, str, delim);
        for(i=0; i<ape_ptrarray_count(parr); i++)
        {
            itm = (char*)ape_ptrarray_get(parr, i);
            ape_object_array_pushvalue(arr, ape_object_make_string(vm->context, itm));
            ape_allocator_free(vm->alloc, (void*)itm);
        }
        ape_ptrarray_destroy(parr);
    }
    return arr;
}

static ApeObject_t cfn_bitnot(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeInt_t val = ape_object_value_asnumber(args[0]);
    return ape_object_make_number(vm->context, ~val);
}

static ApeObject_t cfn_shiftleft(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeInt_t v1 = ape_object_value_asnumber(args[0]);
    ApeInt_t v2 = ape_object_value_asnumber(args[1]);
    return ape_object_make_number(vm->context, (v1 << v2));
}

static ApeObject_t cfn_shiftright(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeInt_t v1 = ape_object_value_asnumber(args[0]);
    ApeInt_t v2 = ape_object_value_asnumber(args[1]);
    return ape_object_make_number(vm->context, (v1 >> v2));
}

static ApeObject_t cfn_object_eval(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t slen;
    const char* str;
    ApeObject_t rt;
    (void)data;
    if(!CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
    {
        return ape_object_make_null(vm->context);
    }
    str = ape_object_string_getdata(args[0]);
    slen = ape_object_string_getlength(args[0]);
    rt = ape_context_executesource(vm->context, str, false);
    return rt;
}


static NatFunc_t g_core_globalfuncs[] =
{
    { "println", cfn_println },
    { "print", cfn_print },

    { "tostring", cfn_to_str },
    { "to_num", cfn_to_num },
    { "range", cfn_range },


    { "concat", cfn_concat },
    { "chr", cfn_chr },
    { "ord", cfn_ord },
    { "substr", cfn_substr, },
    { "reverse", cfn_reverse },
    { "array", cfn_array },
    { "error", cfn_error },
    { "crash", cfn_crash },
    { "assert", cfn_assert },
    { "random_seed", cfn_random_seed },
    { "random", cfn_random },
    { "slice", cfn_object_slice },
    { "len", cfn_object_length },
    { "length", cfn_object_length },

    { "eval", cfn_object_eval },

    // Math
    { "bitnot", cfn_bitnot },
    { "shiftleft", cfn_shiftleft },
    { "shiftright", cfn_shiftright },

};

/*
Object.copy instead of clone?
*/

static NatFunc_t g_core_mathfuncs[]=
{
    { "sqrt", cfn_sqrt },
    { "pow", cfn_pow },
    { "sin", cfn_sin },
    { "cos", cfn_cos },
    { "tan", cfn_tan },
    { "log", cfn_log },
    { "ceil", cfn_ceil },
    { "floor", cfn_floor },
    { "abs", cfn_abs },
    {NULL, NULL},
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


static NatFunc_t g_core_stringfuncs[]=
{
    {"split", cfn_string_split},
    #if 0
    {"trim", cfn_string_trim},
    #endif
    {NULL, NULL},
};

static NatFunc_t g_core_objectfuncs[]=
{
    {"isstring", cfn_object_isstring},
    {"isarray", cfn_object_isarray },
    {"ismap", cfn_object_ismap },
    {"isnumber", cfn_object_isnumber },
    {"isbool", cfn_object_isbool },
    {"isnull", cfn_object_isnull },
    {"isfunction", cfn_object_isfunction },
    {"isexternal", cfn_object_isexternal },
    {"iserror", cfn_object_iserror },
    {"isnativefunction", cfn_object_isnativefunction },


    { "len", cfn_object_length },
    { "first", cfn_object_first },
    { "last", cfn_object_last },
    { "rest", cfn_object_rest },
    { "append", cfn_object_append },
    { "remove", cfn_object_removekey },
    { "removeat", cfn_object_removeat },

    { "keys", cfn_object_objkeys },
    { "values", cfn_object_objvalues },

    { "copy", cfn_object_copy },
    { "deepcopy", cfn_object_deepcopy },


    {NULL, NULL},
};


static void setup_namespace(ApeVM_t* vm, const char* nsname, NatFunc_t* fnarray)
{
    ApeSize_t i;
    ApeObject_t map;
    map = ape_object_make_map(vm->context);
    for(i=0; fnarray[i].name != NULL; i++)
    {
        make_fn_entry(vm->context, map, fnarray[i].name, fnarray[i].fn);
    }
    ape_globalstore_set(vm->globalstore, nsname, map);
}

// TODO: this is an attempt to get rid of (most) global functions.
void builtins_install(ApeVM_t* vm)
{
    setup_namespace(vm, "Math", g_core_mathfuncs);
    setup_namespace(vm, "Dir", g_core_dirfuncs);
    setup_namespace(vm, "File", g_core_filefuncs);
    setup_namespace(vm, "String", g_core_stringfuncs);
    setup_namespace(vm, "Object", g_core_objectfuncs);

    // TODO: implement me!
    #if 0
    setup_namespace(vm, "IO", g_core_iofuncs);
    setup_namespace(vm, "VM", g_core_vmfuncs);
    #endif
}

ApeSize_t builtins_count()
{
    return APE_ARRAY_LEN(g_core_globalfuncs);
}

ApeNativeFunc_t builtins_get_fn(ApeSize_t ix)
{
    return g_core_globalfuncs[ix].fn;
}

const char* builtins_get_name(ApeSize_t ix)
{
    return g_core_globalfuncs[ix].name;
}

/*
* this function is meant as a callback for builtin object methods.
*
*  ** IT IS CURRENTLY NOT USED **
*
* since the compiler compiles dot notation (foo.bar) and bracket-notation (foo["bar"]) into
* the same instructions, there is currently no clear way of how and where to best insert
* lookups to these functions.
*/
ApeNativeFunc_t builtin_get_object(ApeObjType_t objt, const char* idxname)
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
