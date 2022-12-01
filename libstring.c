
#include "ape.h"

ApeObject_t ape_object_make_stringlen(ApeContext_t* ctx, const char* string, ApeSize_t len)
{
    bool ok;
    ApeObject_t res;
    res = ape_object_make_stringcapacity(ctx, len);
    if(ape_object_value_isnull(res))
    {
        return res;
    }
    ok = ape_object_string_append(res, string, len);
    if(!ok)
    {
        return ape_object_make_null(ctx);
    }
    return res;
}

ApeObject_t ape_object_make_string(ApeContext_t* ctx, const char* string)
{
    return ape_object_make_stringlen(ctx, string, strlen(string));
}

ApeObject_t ape_object_make_stringcapacity(ApeContext_t* ctx, ApeSize_t capacity)
{
    bool ok;
    ApeObjData_t* data;
    data = ape_gcmem_getfrompool(ctx->mem, APE_OBJECT_STRING);
    if(!data)
    {
        data = ape_gcmem_allocobjdata(ctx->mem, APE_OBJECT_STRING);
        if(!data)
        {
            return ape_object_make_null(ctx);
        }
        data->valstring.capacity = APE_CONF_SIZE_STRING_BUFSIZE - 1;
        data->valstring.is_allocated = false;
    }
    data->valstring.length = 0;
    data->valstring.hash = 0;
    if(capacity > data->valstring.capacity)
    {
        ok = ape_object_string_reservecapacity(data, capacity);
        if(!ok)
        {
            return ape_object_make_null(ctx);
        }
    }
    return object_make_from_data(ctx, APE_OBJECT_STRING, data);
}

const char* ape_object_string_getdata(ApeObject_t object)
{
    ApeObjData_t* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_STRING);
    data = ape_object_value_allocated_data(object);
    return ape_object_string_getinternalobjdata(data);
}

char* ape_object_string_getinternalobjdata(ApeObjData_t* data)
{
    APE_ASSERT(data->type == APE_OBJECT_STRING);
    if(data->valstring.is_allocated)
    {
        return data->valstring.value_allocated;
    }
    return data->valstring.value_buf;
}

bool ape_object_string_reservecapacity(ApeObjData_t* data, ApeSize_t capacity)
{
    char* new_value;
    ApeObjString_t* string;
    string = &data->valstring;
    string->length = 0;
    string->hash = 0;
    if(capacity <= string->capacity)
    {
        return true;
    }
    if(capacity <= (APE_CONF_SIZE_STRING_BUFSIZE - 1))
    {
        if(string->is_allocated)
        {
            APE_ASSERT(false);// should never happen
            ape_allocator_free(data->mem->alloc, string->value_allocated);// just in case
        }
        string->capacity = APE_CONF_SIZE_STRING_BUFSIZE - 1;
        string->is_allocated = false;
        return true;
    }
    new_value = (char*)ape_allocator_alloc(data->mem->alloc, capacity + 1);
    if(!new_value)
    {
        return false;
    }
    if(string->is_allocated)
    {
        ape_allocator_free(data->mem->alloc, string->value_allocated);
    }
    string->value_allocated = new_value;
    string->is_allocated = true;
    string->capacity = capacity;
    return true;
}

ApeSize_t ape_object_string_getlength(ApeObject_t object)
{
    ApeObjData_t* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_STRING);
    data = ape_object_value_allocated_data(object);
    return data->valstring.length;
}

void ape_object_string_setlength(ApeObject_t object, ApeSize_t len)
{
    ApeObjData_t* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_STRING);
    data = ape_object_value_allocated_data(object);
    data->valstring.length = len;
}

char* ape_object_string_getmutable(ApeObject_t object)
{
    ApeObjData_t* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_STRING);
    data = ape_object_value_allocated_data(object);
    return ape_object_string_getinternalobjdata(data);
}

bool ape_object_string_append(ApeObject_t obj, const char* src, ApeSize_t len)
{
    ApeSize_t capacity;
    ApeSize_t current_len;
    char* str_buf;
    ApeObjData_t* data;
    ApeObjString_t* string;
    APE_ASSERT(ape_object_value_type(obj) == APE_OBJECT_STRING);
    data = ape_object_value_allocated_data(obj);
    string = &data->valstring;
    str_buf = ape_object_string_getmutable(obj);
    current_len = string->length;
    capacity = string->capacity;
    if((len + current_len) > capacity)
    {
        APE_ASSERT(false);
        return false;
    }
    memcpy(str_buf + current_len, src, len);
    string->length += len;
    str_buf[string->length] = '\0';
    return true;
}

unsigned long ape_object_string_gethash(ApeObject_t obj)
{
    const char* rawstr;
    ApeSize_t rawlen;
    ApeObjData_t* data;
    APE_ASSERT(ape_object_value_type(obj) == APE_OBJECT_STRING);
    data = ape_object_value_allocated_data(obj);
    if(data->valstring.hash == 0)
    {
        rawstr = ape_object_string_getdata(obj);
        rawlen = ape_object_string_getlength(obj);
        data->valstring.hash = ape_util_hashstring(rawstr, rawlen);
        if(data->valstring.hash == 0)
        {
            data->valstring.hash = 1;
        }
    }
    return data->valstring.hash;
}


static ApeObject_t objfn_string_length(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeInt_t len;
    ApeObject_t self;
    (void)vm;
    (void)data;
    (void)argc;
    (void)args;
    /* fixme: value is passed incorrectly? */
    self = ape_vm_popthisstack(vm);
    len = ape_object_string_getlength(self);
    return ape_object_make_number(vm->context, len);
}

static ApeObject_t objfn_string_substr(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeInt_t begin;
    ApeInt_t end;
    ApeInt_t len;
    ApeInt_t nlen;
    ApeObject_t self;
    ApeArgCheck_t check;
    const char* str;
    (void)data;
    self = ape_vm_popthisstack(vm);
    ape_args_checkinit(vm, &check, "substr", argc, args);
    if(!ape_args_checktype(&check, 0, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);        
    }
    str = ape_object_string_getdata(self);
    len = ape_object_string_getlength(self);
    begin = ape_object_value_asnumber(args[0]);
    end = len;
    if(ape_args_checkoptional(&check, 1, APE_OBJECT_NUMBER, true))
    {
        end = ape_object_value_asnumber(args[1]);
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

static ApeObject_t objfn_string_split(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    char c;
    const char* inpstr;
    const char* delimstr;
    char* itm;
    ApeSize_t i;
    ApeSize_t inplen;
    ApeSize_t delimlen;
    ApeObject_t arr;
    ApeObject_t self;
    ApePtrArray_t* parr;
    ApeArgCheck_t check;
    (void)data;
    delimstr = "";
    delimlen = 0;
    ape_args_checkinit(vm, &check, "split", argc, args);
    self = ape_vm_popthisstack(vm);
    inpstr = ape_object_string_getdata(self);
    if(ape_args_checkoptional(&check, 0, APE_OBJECT_STRING, true))
    {
        delimstr = ape_object_string_getdata(args[0]);
        delimlen = ape_object_string_getlength(args[0]);
    }
    arr = ape_object_make_array(vm->context);
    if(delimlen == 0)
    {
        inplen = ape_object_string_getlength(self);
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

static ApeObject_t objfn_string_indexof(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    char findme;
    const char* findstr;
    const char* inpstr;
    ApeSize_t i;
    ApeSize_t inplen;
    ApeSize_t findlen;
    ApeObject_t self;
    ApeObjType_t styp;
    ApeArgCheck_t check;
    (void)data;
    self = ape_vm_popthisstack(vm);
    ape_args_checkinit(vm, &check, "index", argc, args);
    if(!ape_args_checktype(&check, 0, APE_OBJECT_STRING | APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    findme = -1;
    inpstr = ape_object_string_getdata(self);
    inplen = ape_object_string_getlength(self);
    styp = ape_object_value_type(args[0]);
    if(styp == APE_OBJECT_STRING)
    {
        findstr = ape_object_string_getdata(args[0]);
        findlen = ape_object_string_getlength(args[0]);
        if(findlen == 0)
        {
            return ape_object_make_number(vm->context, -1);
        }
        findme = findstr[0];
    }
    else if(styp == APE_OBJECT_NUMBER)
    {
        findme = ape_object_value_asnumber(args[0]);
        if(findme == -1)
        {
            return ape_object_make_number(vm->context, -1);
        }
    }
    for(i=0; i<inplen; i++)
    {
        if(inpstr[i] == findme)
        {
            return ape_object_make_number(vm->context, i);
        }
    }
    return ape_object_make_number(vm->context, -1);
}

static ApeObject_t cfn_string_chr(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
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

static ApeObject_t cfn_string_ord(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
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

void ape_builtins_install_string(ApeVM_t* vm)
{
    static ApeNativeItem_t stringfuncs[]=
    {
        {"join", cfn_string_join},
        { "chr", cfn_string_chr },
        { "ord", cfn_string_ord },
        #if 0
        {"trim", cfn_string_trim},
        #endif
        {NULL, NULL},
    };
    ape_builtins_setup_namespace(vm, "String", stringfuncs);    
}

bool ape_builtin_objectfunc_find_string(ApeContext_t* ctx, const char* idxname, ApeObjMemberItem_t* dest)
{
    static ApeObjMemberItem_t funcs[]=
    {
        {"length", false, objfn_string_length},
        {"split", true, objfn_string_split},
        {"index", true, objfn_string_indexof},
        {"substr", true, objfn_string_substr},
        {NULL, false, NULL},
    };
    return ape_builtin_find_objectfunc(ctx, funcs, idxname, dest);
}
