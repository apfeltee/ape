
#include <stdarg.h>
#include "inline.h"

#define make_fn_data(ctx, name, fnc, dataptr, datasize) \
    ape_object_make_nativefuncmemory(ctx, name, fnc, dataptr, datasize)

#define make_fn(ctx, name, fnc) \
    make_fn_data(ctx, name, fnc, NULL, 0)

#define make_fn_entry_data(ctx, map, name, fnc, dataptr, datasize) \
    ape_object_map_setnamedvalue(ctx, map, name, make_fn(ctx, name, fnc))

#define make_fn_entry(ctx, map, name, fnc) \
    make_fn_entry_data(ctx, map, name, fnc, NULL, 0)

static ApeObject_t cfn_object_length(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t len;
    ApeObject_t arg;
    ApeObjType_t type;
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "length", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_STRING | APE_OBJECT_ARRAY | APE_OBJECT_MAP))
    {
        return ape_object_make_null(vm->context);
    }
    arg = args[0];
    type = ape_object_value_type(arg);
    if(type == APE_OBJECT_STRING)
    {
        len = ape_object_string_getlength(arg);
        return ape_object_make_floatnumber(vm->context, len);
    }
    else if(type == APE_OBJECT_ARRAY)
    {
        len = ape_object_array_getlength(arg);
        return ape_object_make_floatnumber(vm->context, len);
    }
    else if(type == APE_OBJECT_MAP)
    {
        len = ape_object_map_getlength(arg);
        return ape_object_make_floatnumber(vm->context, len);
    }
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_object_first(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeObject_t arg;
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "first", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_ARRAY))
    {
        return ape_object_make_null(vm->context);
    }
    arg = args[0];
    return ape_object_array_getvalue(arg, 0);
}

static ApeObject_t cfn_object_last(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeObject_t arg;
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "last", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_ARRAY))
    {
        return ape_object_make_null(vm->context);
    }
    arg = args[0];
    return ape_object_array_getvalue(arg, ape_object_array_getlength(arg) - 1);
}

static ApeObject_t cfn_object_rest(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    bool ok;
    ApeSize_t i;
    ApeSize_t len;
    ApeObject_t arg;
    ApeObject_t res;
    ApeObject_t item;
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "rest", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_ARRAY))
    {
        return ape_object_make_null(vm->context);
    }
    arg = args[0];
    len = ape_object_array_getlength(arg);
    if(len == 0)
    {
        return ape_object_make_null(vm->context);
    }
    res = ape_object_make_array(vm->context);
    if(ape_object_value_isnull(res))
    {
        return ape_object_make_null(vm->context);
    }
    for(i = 1; i < len; i++)
    {
        item = ape_object_array_getvalue(arg, i);
        ok = ape_object_array_pushvalue(res, item);
        if(!ok)
        {
            return ape_object_make_null(vm->context);
        }
    }
    return res;
}

static ApeObject_t cfn_array(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    bool ok;
    ApeSize_t i;
    ApeSize_t capacity;
    ApeObject_t res;
    ApeObject_t obj_null;
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "array", argc, args);
    if(argc == 1)
    {
        if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC))
        {
            return ape_object_make_null(vm->context);
        }
        capacity = (ApeSize_t)ape_object_value_asnumber(args[0]);
        res = ape_object_make_arraycapacity(vm->context, capacity);
        if(ape_object_value_isnull(res))
        {
            return ape_object_make_null(vm->context);
        }
        obj_null = ape_object_make_null(vm->context);
        for(i = 0; i < capacity; i++)
        {
            ok = ape_object_array_pushvalue(res, obj_null);
            if(!ok)
            {
                return ape_object_make_null(vm->context);
            }
        }
        return res;
    }
    else if(argc == 2)
    {
        if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC) && !ape_args_check(&check, 1, APE_OBJECT_ANY))
        {
            return ape_object_make_null(vm->context);
        }
        capacity = (ApeSize_t)ape_object_value_asnumber(args[0]);
        res = ape_object_make_arraycapacity(vm->context, capacity);
        if(ape_object_value_isnull(res))
        {
            return ape_object_make_null(vm->context);
        }
        for(i = 0; i < capacity; i++)
        {
            ok = ape_object_array_pushvalue(res, args[1]);
            if(!ok)
            {
                return ape_object_make_null(vm->context);
            }
        }
        return res;
    }
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_println(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t i;
    ApeObject_t arg;
    ApeWriter_t* buf;
    (void)data;
    buf = ape_make_writerio(vm->context, stdout, false, true);
    for(i = 0; i < argc; i++)
    {
        arg = args[i];
        ape_tostring_object(buf, arg, false);
    }
    ape_writer_append(buf, "\n");
    ape_writer_destroy(buf);
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_print(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t i;
    ApeObject_t arg;
    ApeWriter_t* buf;
    (void)data;
    buf = ape_make_writerio(vm->context, stdout, false, true);
    for(i = 0; i < argc; i++)
    {
        arg = args[i];
        ape_tostring_object(buf, arg, false);
    }
    ape_writer_destroy(buf);
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_tostring(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    const char* bstr;
    ApeSize_t blen;
    ApeObject_t res;
    ApeObject_t arg;
    ApeWriter_t* buf;
    (void)data;
    if(argc == 0)
    {
        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "tostring() expects at least one argument");
        return ape_object_make_null(vm->context);
    }
    arg = args[0];
    if(ape_object_value_type(arg) == APE_OBJECT_STRING)
    {
        return arg;
    }
    buf = ape_make_writer(vm->context);
    if(!buf)
    {
        return ape_object_make_null(vm->context);
    }
    ape_tostring_object(buf, arg, false);
    if(ape_writer_failed(buf))
    {
        ape_writer_destroy(buf);
        return ape_object_make_null(vm->context);
    }
    bstr = ape_writer_getdata(buf);
    blen = ape_writer_getlength(buf);
    res = ape_object_make_stringlen(vm->context, bstr, blen);
    ape_writer_destroy(buf);
    return res;
}

static ApeObject_t cfn_toint(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    char* end;
    const char* strv;
    ApeFloat_t result;
    ApeSize_t lenstr;
    ApeSize_t lenparsed;
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "toint", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_STRING | APE_OBJECT_NUMERIC | APE_OBJECT_BOOL | APE_OBJECT_NULL))
    {
        return ape_object_make_null(vm->context);
    }
    result = 0;
    strv = "";
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
        strv = ape_object_string_getdata(args[0]);
        errno = 0;
        result = strtod(strv, &end);
        if(errno == ERANGE && (result <= -HUGE_VAL || result >= HUGE_VAL))
        {
            goto err;
        }
        if(errno && errno != ERANGE)
        {
            goto err;
        }
        lenstr = ape_object_string_getlength(args[0]);
        lenparsed = (ApeSize_t)(end - strv);
        if(lenstr != lenparsed)
        {
            goto err;
        }
    }
    else
    {
        goto err;
    }
    return ape_object_make_floatnumber(vm->context, result);
err:
    ape_vm_adderror(vm, APE_ERROR_RUNTIME, "cannot convert '%s' to number", strv);
    return ape_object_make_null(vm->context);
}



static ApeObject_t cfn_range(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    bool ok;
    const char* typestr;
    const char* expected_str;
    ApeInt_t i;
    ApeInt_t start;
    ApeInt_t end;
    ApeInt_t step;
    ApeObject_t res;
    ApeObject_t item;
    ApeObjType_t type;
    (void)data;
    for(i = 0; i < (ApeInt_t)argc; i++)
    {
        type = ape_object_value_type(args[i]);
        if(!ape_object_type_isnumber(type))
        {
            typestr = ape_object_value_typename(type);
            expected_str = ape_object_value_typename(APE_OBJECT_NUMERIC);
            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "invalid argument %d passed to range, got %s instead of %s", i, typestr, expected_str);
            return ape_object_make_null(vm->context);
        }
    }
    start = 0;
    end = 0;
    step = 1;
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
        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "invalid number of arguments passed to range, got %d", argc);
        return ape_object_make_null(vm->context);
    }
    if(step == 0)
    {
        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "range step cannot be 0");
        return ape_object_make_null(vm->context);
    }
    res = ape_object_make_array(vm->context);
    if(ape_object_value_isnull(res))
    {
        return ape_object_make_null(vm->context);
    }
    for(i = start; i < end; i += step)
    {
        item = ape_object_make_floatnumber(vm->context, i);
        ok = ape_object_array_pushvalue(res, item);
        if(!ok)
        {
            return ape_object_make_null(vm->context);
        }
    }
    return res;
}

static ApeObject_t cfn_object_objkeys(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    bool ok;
    ApeSize_t i;
    ApeSize_t len;
    ApeObject_t arg;
    ApeObject_t res;
    ApeObject_t key;
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "keys", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_MAP))
    {
        return ape_object_make_null(vm->context);
    }
    arg = args[0];
    res = ape_object_make_array(vm->context);
    if(ape_object_value_isnull(res))
    {
        return ape_object_make_null(vm->context);
    }
    len = ape_object_map_getlength(arg);
    for(i = 0; i < len; i++)
    {
        key = ape_object_map_getkeyat(arg, i);
        ok = ape_object_array_pushvalue(res, key);
        if(!ok)
        {
            return ape_object_make_null(vm->context);
        }
    }
    return res;
}

static ApeObject_t cfn_object_objvalues(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    bool ok;
    ApeSize_t i;
    ApeSize_t len;
    ApeObject_t key;
    ApeObject_t res;
    ApeObject_t arg;
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "value", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_MAP))
    {
        return ape_object_make_null(vm->context);
    }
    arg = args[0];
    res = ape_object_make_array(vm->context);
    if(ape_object_value_isnull(res))
    {
        return ape_object_make_null(vm->context);
    }
    len = ape_object_map_getlength(arg);
    for(i = 0; i < len; i++)
    {
        key = ape_object_map_getvalueat(arg, i);
        ok = ape_object_array_pushvalue(res, key);
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
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "copy", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }
    return ape_object_value_copyflat(vm->context, args[0]);
}

static ApeObject_t cfn_object_deepcopy(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "reverse", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }
    return ape_object_value_copydeep(vm->context, args[0]);
}

static ApeObject_t cfn_concat(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    bool ok;
    const char* itemtypstr;
    const char* leftstr;
    const char* rightstr;
    ApeSize_t i;
    ApeSize_t leftlen;
    ApeSize_t rightlen;
    ApeObject_t res;
    ApeObject_t item;
    ApeObjType_t type;
    ApeObjType_t itemtype;
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "concat", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_ARRAY | APE_OBJECT_STRING) && !ape_args_check(&check, 1, APE_OBJECT_ARRAY | APE_OBJECT_STRING))
    {
        return ape_object_make_null(vm->context);
    }
    type = ape_object_value_type(args[0]);
    itemtype = ape_object_value_type(args[1]);
    if(type == APE_OBJECT_ARRAY)
    {
        if(itemtype != APE_OBJECT_ARRAY)
        {
            itemtypstr = ape_object_value_typename(itemtype);
            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "invalid argument 2 passed to concat, got %s", itemtypstr);
            return ape_object_make_null(vm->context);
        }
        for(i = 0; i < ape_object_array_getlength(args[1]); i++)
        {
            item = ape_object_array_getvalue(args[1], i);
            ok = ape_object_array_pushvalue(args[0], item);
            if(!ok)
            {
                return ape_object_make_null(vm->context);
            }
        }
        return ape_object_make_floatnumber(vm->context, ape_object_array_getlength(args[0]));
    }
    else if(type == APE_OBJECT_STRING)
    {
        if(!ape_args_check(&check, 0, APE_OBJECT_STRING) && !ape_args_check(&check, 0, APE_OBJECT_STRING))
        {
            return ape_object_make_null(vm->context);
        }
        leftstr = ape_object_string_getdata(args[0]);
        leftlen = (ApeSize_t)ape_object_string_getlength(args[0]);
        rightstr = ape_object_string_getdata(args[1]);
        rightlen = (ApeSize_t)ape_object_string_getlength(args[1]);
        res = ape_object_make_stringcapacity(vm->context, leftlen + rightlen);
        if(ape_object_value_isnull(res))
        {
            return ape_object_make_null(vm->context);
        }
        ok = ape_object_string_append(vm->context, res, leftstr, leftlen);
        if(!ok)
        {
            return ape_object_make_null(vm->context);
        }
        ok = ape_object_string_append(vm->context, res, rightstr, rightlen);
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
    bool res;
    ApeSize_t i;
    ApeInt_t ix;
    ApeObject_t obj;
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "removekey", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_ARRAY) && !ape_args_check(&check, 1, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }
    ix = -1;
    for(i = 0; i < ape_object_array_getlength(args[0]); i++)
    {
        obj = ape_object_array_getvalue(args[0], i);
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
    res = ape_object_array_removeat(args[0], ix);
    return ape_object_make_bool(vm->context, res);
}

static ApeObject_t cfn_object_removeat(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    bool res;
    ApeSize_t ix;
    ApeObjType_t type;
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "removeat", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_ARRAY) && !ape_args_check(&check, 1, APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);
    }
    type = ape_object_value_type(args[0]);
    ix = (ApeSize_t)ape_object_value_asnumber(args[1]);
    switch(type)
    {
        case APE_OBJECT_ARRAY:
            {
                res = ape_object_array_removeat(args[0], ix);
                return ape_object_make_bool(vm->context, res);
            }
            break;
        default:
            break;
    }
    return ape_object_make_bool(vm->context, true);
}


static ApeObject_t cfn_error(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    const char* msg;
    msg = "unspecified error";
    if(argc == 1 && ape_object_value_type(args[0]) == APE_OBJECT_STRING)
    {
        msg = ape_object_string_getdata(args[0]);
    }
    /*return ape_object_make_error(vm->context, msg);*/
    ape_vm_adderror(vm, APE_ERROR_RUNTIME, "%s", msg);
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_crash(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(argc == 1 && ape_object_value_type(args[0]) == APE_OBJECT_STRING)
    {
        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "%s", ape_object_string_getdata(args[0]));
    }
    else
    {
        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "");
    }
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_assert(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "assert", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_BOOL))
    {
        return ape_object_make_null(vm->context);
    }
    if(!ape_object_value_asbool(args[0]))
    {
        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "assertion failed");
        return ape_object_make_null(vm->context);
    }
    return ape_object_make_bool(vm->context, true);
}

static ApeObject_t cfn_random_seed(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t seed;
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "seed", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);
    }
    seed = (ApeSize_t)ape_object_value_asnumber(args[0]);
    srand(seed);
    return ape_object_make_bool(vm->context, true);
}

static ApeObject_t cfn_random(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeFloat_t minv;
    ApeFloat_t maxv;
    ApeFloat_t res;
    ApeFloat_t range;
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "random", argc, args);
    res = (ApeFloat_t)rand() / RAND_MAX;
    if(argc == 0)
    {
        return ape_object_make_floatnumber(vm->context, res);
    }
    else if(argc == 2)
    {
        if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC) && !ape_args_check(&check, 1, APE_OBJECT_NUMERIC))
        {
            return ape_object_make_null(vm->context);
        }
        minv = ape_object_value_asnumber(args[0]);
        maxv = ape_object_value_asnumber(args[1]);
        if(minv >= maxv)
        {
            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "max is bigger than min");
            return ape_object_make_null(vm->context);
        }
        range = maxv - minv;
        res = minv + (res * range);
        return ape_object_make_floatnumber(vm->context, res);
    }
    ape_vm_adderror(vm, APE_ERROR_RUNTIME, "invalid number or arguments");
    return ape_object_make_null(vm->context);
}


static ApeObject_t cfn_object_isstring(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "isstring", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_STRING);
}

static ApeObject_t cfn_object_isarray(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "isarray", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_ARRAY);
}

static ApeObject_t cfn_object_ismap(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "ismap", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_MAP);
}

static ApeObject_t cfn_object_isnumber(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "isnumber", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_type_isnumber(ape_object_value_type(args[0])));
}

static ApeObject_t cfn_object_isbool(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "isbool", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_BOOL);
}

static ApeObject_t cfn_object_isnull(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "isnull", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_NULL);
}

static ApeObject_t cfn_object_isfunction(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "isfunction", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_SCRIPTFUNCTION);
}

static ApeObject_t cfn_object_isexternal(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "isexternal", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_EXTERNAL);
}

static ApeObject_t cfn_object_iserror(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "iserror", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_ERROR);
}

static ApeObject_t cfn_object_isnativefunction(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_init(vm, &check, "isnativefunction", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_NATIVEFUNCTION);
}

static ApeObject_t cfn_bitnot(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeInt_t val;
    ApeArgCheck_t check;
    (void)data;
    ape_args_init(vm, &check, "bitnot", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);        
    }
    val = ape_object_value_asfixednumber(args[0]);
    return ape_object_make_fixednumber(vm->context, ~val);
}


static ApeObject_t cfn_object_eval(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t slen;
    const char* str;
    ApeObject_t rt;
    ApeArgCheck_t check;
    (void)data;
    (void)data;
    ape_args_init(vm, &check, "eval", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_STRING))
    {
        return ape_object_make_null(vm->context);        
    }
    str = ape_object_string_getdata(args[0]);
    slen = ape_object_string_getlength(args[0]);
    rt = ape_context_executesource(vm->context, str, slen, false);
    return rt;
}

static ApeObject_t cfn_vm_gcsweep(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    (void)argc;
    (void)args;
    ape_gcmem_sweep(vm->mem);
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_vm_gccollect(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    (void)argc;
    (void)args;
    ape_vm_collectgarbage(vm, NULL, false);
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_vm_stack(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t i;
    ApeInt_t idx;
    ApeObject_t arr;
    (void)data;
    (void)argc;
    (void)args;
    if(argc > 0)
    {
        if(!ape_object_type_isnumber(ape_object_value_type(args[0])))
        {
            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "stack() optional argument must be a number");
            return ape_object_make_null(vm->context);
        }
        idx = ape_object_value_asnumber(args[0]);
        if(idx > (ApeInt_t)(vm->stackptr + 1))
        {
            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "stack() optional argument out of bounds (idx=%d stackptr=%d)", idx, vm->stackptr);
            return ape_object_make_null(vm->context);
        }
        return *(ApeObject_t*)ape_valdict_getbykey(vm->stackobjects, &idx);
    }
    arr = ape_object_make_array(vm->context);
    for(i=0; i<(ApeSize_t)(vm->stackptr + 1); i++)
    {
        ape_object_array_pushvalue(arr, *(ApeObject_t*)ape_valdict_getbykey(vm->stackobjects, &i));
    }
    return arr;
}

static ApeNativeItem_t g_core_globalfuncs[] =
{
    { "println", cfn_println },
    { "print", cfn_print },
    /*
    * tostring and to(_num|int) *should* be in Object, but because
    * the compiler wraps template strings in tostring(), and because
    * getting Object.tostring would result in slightly more computation,
    * they remain as global functions.
    * at least for now, anyway.
    */
    { "tostring", cfn_tostring },
    { "to_num", cfn_toint },
    { "toint", cfn_toint },
    { "range", cfn_range },
    { "concat", cfn_concat },
    { "array", cfn_array },
    { "error", cfn_error },
    { "crash", cfn_crash },
    { "assert", cfn_assert },
    { "random_seed", cfn_random_seed },
    { "random", cfn_random },

    { "eval", cfn_object_eval },
    /* Math */
    { "bitnot", cfn_bitnot },
    {NULL, NULL},
};

void ape_builtins_setup_namespace(ApeVM_t* vm, const char* nsname, ApeNativeItem_t* fnarray)
{
    ApeSize_t i;
    ApeObject_t map;
    ApeSymbol_t* sym;
    static ApePosition_t posinvalid = { NULL, -1, -1 };
    /* this ensures that the namespace does not get assigned to */
    sym = ape_compiler_definesym(vm->context->compiler, posinvalid, nsname, false, false);
    if(!sym)
    {
        //ape_vm_adderror(vm, APE_ERROR_RUNTIME, "failed to declare namespace '%s' in compiler");
    }
    map = ape_object_make_map(vm->context);
    for(i=0; fnarray[i].name != NULL; i++)
    {
        make_fn_entry(vm->context, map, fnarray[i].name, fnarray[i].fn);
    }
    ape_globalstore_set(vm->globalstore, nsname, map);
}

void ape_builtins_install_vm(ApeVM_t* vm)
{
    static ApeNativeItem_t staticfuncs[]=
    {
        {"sweep", cfn_vm_gcsweep},
        {"collect", cfn_vm_gccollect},
        {"stack", cfn_vm_stack},
        #if 0
        {"delete", cfn_vm_delete},
        #endif
        {NULL, NULL},
    };
    ape_builtins_setup_namespace(vm, "VM", staticfuncs);    
}

void ape_builtins_install_object(ApeVM_t* vm)
{
    /*
    Object.copy instead of clone?
    */
    static ApeNativeItem_t staticfuncs[]=
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
        { "length", cfn_object_length },
        { "first", cfn_object_first },
        { "last", cfn_object_last },
        { "rest", cfn_object_rest },

        { "remove", cfn_object_removekey },
        { "removeat", cfn_object_removeat },
        { "keys", cfn_object_objkeys },
        { "values", cfn_object_objvalues },
        { "copy", cfn_object_copy },
        { "deepcopy", cfn_object_deepcopy },
        {NULL, NULL},
    };
    ape_builtins_setup_namespace(vm, "Object", staticfuncs);
}

/* TODO: this is an attempt to get rid of (most) global functions. */
void ape_builtins_install(ApeVM_t* vm)
{
    ape_builtins_install_object(vm);
    ape_builtins_install_io(vm);
    ape_builtins_install_string(vm);
    ape_builtins_install_array(vm);
    ape_builtins_install_math(vm);
    ape_builtins_install_vm(vm);
}

ApeSize_t ape_builtins_count()
{
    return (APE_ARRAY_LEN(g_core_globalfuncs) - 1);
}

ApeNativeFuncPtr_t ape_builtins_getfunc(ApeSize_t ix)
{
    return g_core_globalfuncs[ix].fn;
}

const char* ape_builtins_getname(ApeSize_t ix)
{
    return g_core_globalfuncs[ix].name;
}

