
#include "ape.h"

ApeObject_t ape_object_make_stringlen(ApeContext_t* ctx, const char* string, ApeSize_t len)
{
    bool ok;
    ApeObject_t res;
    res = ape_object_make_stringcapacity(ctx, len);
    if(object_value_isnull(res))
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
    APE_ASSERT(object_value_type(object) == APE_OBJECT_STRING);
    data = object_value_allocated_data(object);
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
    APE_ASSERT(capacity >= 0);
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
    APE_ASSERT(object_value_type(object) == APE_OBJECT_STRING);
    data = object_value_allocated_data(object);
    return data->valstring.length;
}

void ape_object_string_setlength(ApeObject_t object, ApeSize_t len)
{
    ApeObjData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_STRING);
    data = object_value_allocated_data(object);
    data->valstring.length = len;
}

char* ape_object_string_getmutable(ApeObject_t object)
{
    ApeObjData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_STRING);
    data = object_value_allocated_data(object);
    return ape_object_string_getinternalobjdata(data);
}

bool ape_object_string_append(ApeObject_t obj, const char* src, ApeSize_t len)
{
    ApeSize_t capacity;
    ApeSize_t current_len;
    char* str_buf;
    ApeObjData_t* data;
    ApeObjString_t* string;
    APE_ASSERT(object_value_type(obj) == APE_OBJECT_STRING);
    data = object_value_allocated_data(obj);
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
    APE_ASSERT(object_value_type(obj) == APE_OBJECT_STRING);
    data = object_value_allocated_data(obj);
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


