
#include <stdarg.h>
#include "ape.h"

ApeObjType_t* ape_args_make_typarray(int first, ...)
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


void ape_args_checkinit(ApeVM_t* vm, ApeArgCheck_t* check, const char* name, ApeSize_t argc, ApeObject_t* args)
{
    check->vm = vm;
    check->name = name;
    check->haveminargs = false;
    check->counterror = false;
    check->minargs = 0;
    check->argc = argc;
    check->args = args;
}

void ape_args_raisetyperror(ApeArgCheck_t* check, ApeSize_t ix, ApeObjType_t typ)
{
    char* extypestr;
    const char* typestr;
    typestr = ape_object_value_typename(ape_object_value_type(check->args[ix]));
    extypestr = ape_object_value_typeunionname(check->vm->context, typ);
    ape_vm_adderror(check->vm, APE_ERROR_RUNTIME,
        "invalid arguments: function '%s' expects argument #%d to be a %s, but got %s instead", check->name, ix, extypestr, typestr);
    ape_allocator_free(check->vm->alloc, extypestr);
}

bool ape_args_checkoptional(ApeArgCheck_t* check, ApeSize_t ix, ApeObjType_t typ, bool raisetyperror)
{
    check->counterror = false;
    if((check->argc == 0) || (check->argc <= ix))
    {
        check->counterror = true;
        return false;
    }
    if(!(ape_object_value_type(check->args[ix]) & typ))
    {
        if(raisetyperror)
        {
            ape_args_raisetyperror(check, ix, typ);
        }
        return false;
    }
    return true;
}

bool ape_args_checktype(ApeArgCheck_t* check, ApeSize_t ix, ApeObjType_t typ)
{
    ApeSize_t ixval;
    if(!ape_args_checkoptional(check, ix, typ, false))
    {
        if(check->counterror)
        {
            ixval = ix;
            if(check->haveminargs)
            {
                ixval = check->minargs;               
            }
            else
            {
            }
            ape_vm_adderror(check->vm, APE_ERROR_RUNTIME, "invalid arguments: function '%s' expects at least %d arguments", check->name, ixval);
        }
        else
        {
            ape_args_raisetyperror(check, ix, typ);
        }
    }
    return true;
}

bool ape_args_check(ApeVM_t* vm, bool generate_error, ApeSize_t argc, ApeObject_t* args, ApeSize_t expected_argc, const ApeObjType_t* expected_types)
{
    ApeSize_t i;
    ApeObject_t arg;
    ApeObjType_t type;
    ApeObjType_t extype;
    char* extypestr;
    const char* typestr;
    if(argc != expected_argc)
    {
        if(generate_error)
        {
            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "invalid number of arguments, got %d instead of %d", argc, expected_argc);
        }
        return false;
    }
    for(i = 0; i < argc; i++)
    {
        arg = args[i];
        type = ape_object_value_type(arg);
        extype = expected_types[i];
        if(!(type & extype))
        {
            if(generate_error)
            {
                typestr = ape_object_value_typename(type);
                extypestr = ape_object_value_typeunionname(vm->context, extype);
                if(!extypestr)
                {
                    return false;
                }
                ape_vm_adderror(vm, APE_ERROR_RUNTIME, "invalid argument %d type, got %s, expected %s", i, typestr, extypestr);
                ape_allocator_free(vm->alloc, extypestr);
            }
            return false;
        }
    }
    return true;
}

static ApeObject_t cfn_object_length(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t len;
    ApeObject_t arg;
    ApeObjType_t type;
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING | APE_OBJECT_ARRAY | APE_OBJECT_MAP))
    {
        return ape_object_make_null(vm->context);
    }
    arg = args[0];
    type = ape_object_value_type(arg);
    if(type == APE_OBJECT_STRING)
    {
        len = ape_object_string_getlength(arg);
        return ape_object_make_number(vm->context, len);
    }
    else if(type == APE_OBJECT_ARRAY)
    {
        len = ape_object_array_getlength(arg);
        return ape_object_make_number(vm->context, len);
    }
    else if(type == APE_OBJECT_MAP)
    {
        len = ape_object_map_getlength(arg);
        return ape_object_make_number(vm->context, len);
    }
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_object_first(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeObject_t arg;
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY))
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
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY))
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
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY))
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

static ApeObject_t cfn_reverse(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    bool ok;
    char* resbuf;
    const char* str;
    ApeObject_t obj;
    ApeObject_t arg;
    ApeObject_t res;
    ApeObjType_t type;
    ApeSize_t i;
    ApeSize_t len;
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY | APE_OBJECT_STRING))
    {
        return ape_object_make_null(vm->context);
    }
    arg = args[0];
    type = ape_object_value_type(arg);
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
            obj = ape_object_array_getvalue(arg, i);
            ok = ape_object_array_setat(res, len - i - 1, obj);
            if(!ok)
            {
                return ape_object_make_null(vm->context);
            }
        }
        return res;
    }
    else if(type == APE_OBJECT_STRING)
    {
        str = ape_object_string_getdata(arg);
        len = ape_object_string_getlength(arg);
        res = ape_object_make_stringcapacity(vm->context, len);
        if(ape_object_value_isnull(res))
        {
            return ape_object_make_null(vm->context);
        }
        resbuf = ape_object_string_getmutable(res);
        for(i = 0; i < len; i++)
        {
            resbuf[len - i - 1] = str[i];
        }
        resbuf[len] = '\0';
        ape_object_string_setlength(res, len);
        return res;
    }
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_array(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    bool ok;
    ApeSize_t i;
    ApeSize_t capacity;
    ApeObject_t res;
    ApeObject_t obj_null;
    (void)data;
    if(argc == 1)
    {
        if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
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
        if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER, APE_OBJECT_ANY))
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
    APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER);
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_object_append(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    bool ok;
    ApeSize_t len;
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }
    ok = ape_object_array_pushvalue(args[0], args[1]);
    if(!ok)
    {
        return ape_object_make_null(vm->context);
    }
    len = ape_object_array_getlength(args[0]);
    return ape_object_make_number(vm->context, len);
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
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING | APE_OBJECT_NUMBER | APE_OBJECT_BOOL | APE_OBJECT_NULL))
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
    return ape_object_make_number(vm->context, result);
err:
    ape_vm_adderror(vm, APE_ERROR_RUNTIME, "cannot convert '%s' to number", strv);
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_chr(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    char c;
    char buf[2];
    ApeFloat_t val;
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    val = ape_object_value_asnumber(args[0]);
    c = (char)val;
    buf[0] = c;
    buf[1] = '\0';
    return ape_object_make_stringlen(vm->context, buf, 1);
}


static ApeObject_t cfn_ord(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    const char* str;
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING | APE_OBJECT_NULL))
    {
        return ape_object_make_null(vm->context);
    }
    if(ape_object_value_isnull(args[0]))
    {
        return ape_object_make_number(vm->context, 0);
    }
    str = ape_object_string_getdata(args[0]);
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
    /* fixme: this is actually incorrect */
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
        if(type != APE_OBJECT_NUMBER)
        {
            typestr = ape_object_value_typename(type);
            expected_str = ape_object_value_typename(APE_OBJECT_NUMBER);
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
        item = ape_object_make_number(vm->context, i);
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
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_MAP))
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
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_MAP))
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
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }
    return ape_object_value_copyflat(vm->context, args[0]);
}

static ApeObject_t cfn_object_deepcopy(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ANY))
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
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY | APE_OBJECT_STRING, APE_OBJECT_ARRAY | APE_OBJECT_STRING))
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
        return ape_object_make_number(vm->context, ape_object_array_getlength(args[0]));
    }
    else if(type == APE_OBJECT_STRING)
    {
        if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING, APE_OBJECT_STRING))
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
        ok = ape_object_string_append(res, leftstr, leftlen);
        if(!ok)
        {
            return ape_object_make_null(vm->context);
        }
        ok = ape_object_string_append(res, rightstr, rightlen);
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
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY, APE_OBJECT_ANY))
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
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_ARRAY, APE_OBJECT_NUMBER))
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
    if(argc == 1 && ape_object_value_type(args[0]) == APE_OBJECT_STRING)
    {
        return ape_object_make_error(vm->context, ape_object_string_getdata(args[0]));
    }
    return ape_object_make_error(vm->context, "");
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
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_BOOL))
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
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
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
    res = (ApeFloat_t)rand() / RAND_MAX;
    if(argc == 0)
    {
        return ape_object_make_number(vm->context, res);
    }
    else if(argc == 2)
    {
        if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER, APE_OBJECT_NUMBER))
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
        return ape_object_make_number(vm->context, res);
    }
    ape_vm_adderror(vm, APE_ERROR_RUNTIME, "invalid number or arguments");
    return ape_object_make_null(vm->context);
}

static ApeObject_t cfn_object_slice(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    bool ok;
    char c;
    char* resbuf;
    const char* str;
    const char* typestr;
    ApeInt_t i;
    ApeInt_t len;
    ApeInt_t index;
    ApeInt_t reslen;
    ApeObject_t res;
    ApeObject_t item;
    ApeObjType_t argtype;
    (void)data;
    ApeArgCheck_t check;
    ape_args_checkinit(vm, &check, "Object.slice", argc, args);
    if(!ape_args_checktype(&check, 0, APE_OBJECT_STRING | APE_OBJECT_ARRAY | APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);        
    }
    argtype = ape_object_value_type(args[0]);
    index = (ApeInt_t)ape_object_value_asnumber(args[1]);
    if(argtype == APE_OBJECT_ARRAY)
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
        res = ape_object_make_arraycapacity(vm->context, len - index);
        if(ape_object_value_isnull(res))
        {
            return ape_object_make_null(vm->context);
        }
        for(i = index; i < len; i++)
        {
            item = ape_object_array_getvalue(args[0], i);
            ok = ape_object_array_pushvalue(res, item);
            if(!ok)
            {
                return ape_object_make_null(vm->context);
            }
        }
        return res;
    }
    else if(argtype == APE_OBJECT_STRING)
    {
        str = ape_object_string_getdata(args[0]);
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
        reslen = len - index;
        res = ape_object_make_stringcapacity(vm->context, reslen);
        if(ape_object_value_isnull(res))
        {
            return ape_object_make_null(vm->context);
        }
        resbuf = ape_object_string_getmutable(res);
        memset(resbuf, 0, reslen + 1);
        for(i = index; i < len; i++)
        {
            c = str[i];
            resbuf[i - index] = c;
        }
        ape_object_string_setlength(res, reslen);
        return res;
    }
    typestr = ape_object_value_typename(argtype);
    ape_vm_adderror(vm, APE_ERROR_RUNTIME, "invalid argument 0 passed to slice, got %s instead", typestr);
    return ape_object_make_null(vm->context);
    
}

static ApeObject_t cfn_object_isstring(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_checkinit(vm, &check, "isstring", argc, args);
    if(!ape_args_checktype(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_STRING);
}

static ApeObject_t cfn_object_isarray(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_checkinit(vm, &check, "isarray", argc, args);
    if(!ape_args_checktype(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_ARRAY);
}

static ApeObject_t cfn_object_ismap(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_checkinit(vm, &check, "ismap", argc, args);
    if(!ape_args_checktype(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_MAP);
}

static ApeObject_t cfn_object_isnumber(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_checkinit(vm, &check, "isnumber", argc, args);
    if(!ape_args_checktype(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_NUMBER);
}

static ApeObject_t cfn_object_isbool(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_checkinit(vm, &check, "isbool", argc, args);
    if(!ape_args_checktype(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_BOOL);
}

static ApeObject_t cfn_object_isnull(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_checkinit(vm, &check, "isnull", argc, args);
    if(!ape_args_checktype(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_NULL);
}

static ApeObject_t cfn_object_isfunction(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_checkinit(vm, &check, "isfunction", argc, args);
    if(!ape_args_checktype(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_SCRIPTFUNCTION);
}

static ApeObject_t cfn_object_isexternal(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_checkinit(vm, &check, "isexternal", argc, args);
    if(!ape_args_checktype(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_EXTERNAL);
}

static ApeObject_t cfn_object_iserror(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_checkinit(vm, &check, "iserror", argc, args);
    if(!ape_args_checktype(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_ERROR);
}

static ApeObject_t cfn_object_isnativefunction(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    ApeArgCheck_t check;
    ape_args_checkinit(vm, &check, "isnativefunction", argc, args);
    if(!ape_args_checktype(&check, 0, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);        
    }
    return ape_object_make_bool(vm->context, ape_object_value_type(args[0]) == APE_OBJECT_NATIVEFUNCTION);
}

static ApeObject_t objfn_string_length(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    const char* str;
    ApeInt_t len;
    ApeObject_t self;
    (void)vm;
    (void)data;
    (void)argc;
    (void)args;
    /* fixme: value is passed incorrectly? */
    #if 0
    self = args[0];
    #else
    self = vm->stack[0];
    #endif
    str = ape_object_string_getdata(self);
    len = ape_object_string_getlength(self);
    fprintf(stderr, "objfn_string_length:argc=%d - self=<%.*s>\n", (int)argc, (int)len, str);
    return ape_object_make_number(vm->context, len);
}


static ApeObject_t cfn_string_split(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    char c;
    const char* inpstr;
    const char* delimstr;
    char* itm;
    ApeSize_t i;
    ApeSize_t inplen;
    ApeSize_t delimlen;
    ApeObject_t arr;
    ApePtrArray_t* parr;
    ApeArgCheck_t check;
    (void)data;
    delimstr = "";
    delimlen = 0;
    ape_args_checkinit(vm, &check, "String.split", argc, args);
    if(!ape_args_checktype(&check, 0, APE_OBJECT_STRING))
    {
        return ape_object_make_null(vm->context);        
    }
    inpstr = ape_object_string_getdata(args[0]);
    if(ape_args_checkoptional(&check, 1, APE_OBJECT_STRING, true))
    {
        delimstr = ape_object_string_getdata(args[1]);
        delimlen = ape_object_string_getlength(args[1]);
    }
    arr = ape_object_make_array(vm->context);
    if(delimlen == 0)
    {
        inplen = ape_object_string_getlength(args[0]);
        for(i=0; i<inplen; i++)
        {
            c = inpstr[i];
            ape_object_array_pushvalue(arr, ape_object_make_stringlen(vm->context, &c, 1));
        }
    }
    else
    {
        parr = ape_util_splitstring(vm->context, inpstr, delimstr);
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

static ApeObject_t cfn_string_join(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    const char* sstr;
    const char* bstr;
    ApeSize_t i;
    ApeSize_t slen;
    ApeSize_t alen;
    ApeSize_t blen;
    ApeObject_t rt;
    ApeObject_t arritem;
    ApeObject_t arrobj;
    ApeObject_t sjoin;
    ApeWriter_t* wr;
    ApeArgCheck_t check;
    (void)data;
    sstr = "";
    slen = 0;
    ape_args_checkinit(vm, &check, "String.join", argc, args);
    if(!ape_args_checktype(&check, 0, APE_OBJECT_ARRAY))
    {
        return ape_object_make_null(vm->context);        
    }
    arrobj = args[0];
    if(ape_args_checkoptional(&check, 1, APE_OBJECT_STRING, true))
    {
        sjoin = args[1];
        if(ape_object_value_type(sjoin) != APE_OBJECT_STRING)
        {
            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "String.join expects second argument to be a string");
            return ape_object_make_null(vm->context);
        }
        sstr = ape_object_string_getdata(sjoin);
        slen = ape_object_string_getlength(sjoin);
    }
    alen = ape_object_array_getlength(arrobj);
    wr = ape_make_writer(vm->context);
    for(i=0; i<alen; i++)
    {
        arritem = ape_object_array_getvalue(arrobj, i);
        ape_tostring_object(wr, arritem, false);
        if((i + 1) < alen)
        {
            ape_writer_appendn(wr, sstr, slen);
        }
    }
    bstr = ape_writer_getdata(wr);
    blen = ape_writer_getlength(wr);
    rt = ape_object_make_stringlen(vm->context, bstr, blen);
    ape_writer_destroy(wr);
    return rt;
}

static ApeObject_t cfn_bitnot(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeInt_t val;
    ApeArgCheck_t check;
    (void)data;
    ape_args_checkinit(vm, &check, "bitnot", argc, args);
    if(!ape_args_checktype(&check, 0, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);        
    }
    val = ape_object_value_asnumber(args[0]);
    return ape_object_make_number(vm->context, ~val);
}

static ApeObject_t cfn_shiftleft(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeInt_t v1;
    ApeInt_t v2;
    ApeArgCheck_t check;
    (void)data;
    ape_args_checkinit(vm, &check, "shiftleft", argc, args);
    if(!ape_args_checktype(&check, 0, APE_OBJECT_NUMBER) || !ape_args_checktype(&check, 1, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);        
    }
    v1 = ape_object_value_asnumber(args[0]);
    v2 = ape_object_value_asnumber(args[1]);
    return ape_object_make_number(vm->context, (v1 << v2));
}

static ApeObject_t cfn_shiftright(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeInt_t v1;
    ApeInt_t v2;
    ApeArgCheck_t check;
    (void)data;
    ape_args_checkinit(vm, &check, "shiftright", argc, args);
    if(!ape_args_checktype(&check, 0, APE_OBJECT_NUMBER) || !ape_args_checktype(&check, 1, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);        
    }
    v1 = ape_object_value_asnumber(args[0]);
    v2 = ape_object_value_asnumber(args[1]);
    return ape_object_make_number(vm->context, (v1 >> v2));
}

static ApeObject_t cfn_object_eval(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t slen;
    const char* str;
    ApeObject_t rt;
    ApeArgCheck_t check;
    (void)data;
    (void)slen;
    (void)data;
    ape_args_checkinit(vm, &check, "eval", argc, args);
    if(!ape_args_checktype(&check, 0, APE_OBJECT_STRING))
    {
        return ape_object_make_null(vm->context);        
    }
    str = ape_object_string_getdata(args[0]);
    slen = ape_object_string_getlength(args[0]);
    rt = ape_context_executesource(vm->context, str, false);
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

static ApeNativeItem_t g_core_globalfuncs[] =
{
    { "println", cfn_println },
    { "print", cfn_print },
    { "tostring", cfn_tostring },
    { "to_num", cfn_toint },
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
    /* Math */
    { "bitnot", cfn_bitnot },
    { "shiftleft", cfn_shiftleft },
    { "shiftright", cfn_shiftright },
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
        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "failed to declare namespace '%s' in compiler");
    }
    map = ape_object_make_map(vm->context);
    for(i=0; fnarray[i].name != NULL; i++)
    {
        make_fn_entry(vm->context, map, fnarray[i].name, fnarray[i].fn);
    }
    ape_globalstore_set(vm->globalstore, nsname, map);
}

void ape_builtins_install_string(ApeVM_t* vm)
{
    static ApeNativeItem_t stringfuncs[]=
    {
        {"split", cfn_string_split},
        {"join", cfn_string_join},
        #if 0
        {"trim", cfn_string_trim},
        #endif
        {NULL, NULL},
    };
    ape_builtins_setup_namespace(vm, "String", stringfuncs);    
}

void ape_builtins_install_vm(ApeVM_t* vm)
{
    static ApeNativeItem_t vmfuncs[]=
    {
        {"sweep", cfn_vm_gcsweep},
        {"collect", cfn_vm_gccollect},
        {NULL, NULL},
    };
    ape_builtins_setup_namespace(vm, "VM", vmfuncs);    
}

void ape_builtins_install_object(ApeVM_t* vm)
{
    /*
    Object.copy instead of clone?
    */
    static ApeNativeItem_t objectfuncs[]=
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
    ape_builtins_setup_namespace(vm, "Object", objectfuncs);
}

/* TODO: this is an attempt to get rid of (most) global functions. */
void ape_builtins_install(ApeVM_t* vm)
{
    ape_builtins_install_object(vm);
    ape_builtins_install_io(vm);
    ape_builtins_install_string(vm);
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

ApeNativeFuncPtr_t ape_builtin_find_objectfunc(ApeContext_t* ctx, ApeNativeItem_t* nf, const char* idxname)
{
    ApeInt_t i;
    (void)ctx;
    for(i=0; nf[i].name != NULL; i++)
    {
        if(APE_STREQ(nf[i].name, idxname))
        {
            return nf[i].fn;
        }
    }
    return NULL;
}

ApeNativeFuncPtr_t ape_builtin_objectfunc_find_string(ApeContext_t* ctx, const char* idxname)
{
    static ApeNativeItem_t g_object_stringfuncs[]=
    {
        {"length", objfn_string_length},
        {NULL, NULL},
    };
    return ape_builtin_find_objectfunc(ctx, g_object_stringfuncs, idxname);    
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

ApeNativeFuncPtr_t builtin_get_object(ApeContext_t* ctx, ApeObjType_t objt, const char* idxname)
{
    if(objt == APE_OBJECT_STRING)
    {
        return ape_builtin_objectfunc_find_string(ctx, idxname);
    }
    return NULL;
}
