
#include "ape.h"

ApeObject_t object_make_stringlen(ApeContext_t* ctx, const char* string, ApeSize_t len)
{
    bool ok;
    ApeObject_t res;
    res = object_make_stringcapacity(ctx, len);
    if(object_value_isnull(res))
    {
        return res;
    }
    ok = object_string_append(res, string, len);
    if(!ok)
    {
        return object_make_null(ctx);
    }
    return res;
}

ApeObject_t object_make_string(ApeContext_t* ctx, const char* string)
{
    return object_make_stringlen(ctx, string, strlen(string));
}

ApeObject_t object_make_stringcapacity(ApeContext_t* ctx, ApeSize_t capacity)
{
    bool ok;
    ApeObjectData_t* data;
    data = gcmem_get_object_data_from_pool(ctx->mem, APE_OBJECT_STRING);
    if(!data)
    {
        data = gcmem_alloc_object_data(ctx->mem, APE_OBJECT_STRING);
        if(!data)
        {
            return object_make_null(ctx);
        }
        data->string.capacity = APE_CONF_SIZE_STRING_BUFSIZE - 1;
        data->string.is_allocated = false;
    }
    data->string.length = 0;
    data->string.hash = 0;
    if(capacity > data->string.capacity)
    {
        ok = object_string_reservecapacity(data, capacity);
        if(!ok)
        {
            return object_make_null(ctx);
        }
    }
    return object_make_from_data(ctx, APE_OBJECT_STRING, data);
}

const char* object_string_getdata(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_STRING);
    data = object_value_allocated_data(object);
    return object_string_getinternalobjdata(data);
}

char* object_string_getinternalobjdata(ApeObjectData_t* data)
{
    APE_ASSERT(data->type == APE_OBJECT_STRING);
    if(data->string.is_allocated)
    {
        return data->string.value_allocated;
    }
    return data->string.value_buf;
}

bool object_string_reservecapacity(ApeObjectData_t* data, ApeSize_t capacity)
{
    char* new_value;
    ApeObjectString_t* string;
    APE_ASSERT(capacity >= 0);
    string = &data->string;
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
            allocator_free(data->mem->alloc, string->value_allocated);// just in case
        }
        string->capacity = APE_CONF_SIZE_STRING_BUFSIZE - 1;
        string->is_allocated = false;
        return true;
    }
    new_value = (char*)allocator_malloc(data->mem->alloc, capacity + 1);
    if(!new_value)
    {
        return false;
    }
    if(string->is_allocated)
    {
        allocator_free(data->mem->alloc, string->value_allocated);
    }
    string->value_allocated = new_value;
    string->is_allocated = true;
    string->capacity = capacity;
    return true;
}

ApeSize_t object_string_getlength(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_STRING);
    data = object_value_allocated_data(object);
    return data->string.length;
}

void object_string_setlength(ApeObject_t object, ApeSize_t len)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_STRING);
    data = object_value_allocated_data(object);
    data->string.length = len;
}

char* object_string_getmutable(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_STRING);
    data = object_value_allocated_data(object);
    return object_string_getinternalobjdata(data);
}

bool object_string_append(ApeObject_t obj, const char* src, ApeSize_t len)
{
    ApeSize_t capacity;
    ApeSize_t current_len;
    char* str_buf;
    ApeObjectData_t* data;
    ApeObjectString_t* string;
    APE_ASSERT(object_value_type(obj) == APE_OBJECT_STRING);
    data = object_value_allocated_data(obj);
    string = &data->string;
    str_buf = object_string_getmutable(obj);
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

unsigned long object_string_gethash(ApeObject_t obj)
{
    const char* rawstr;
    ApeSize_t rawlen;
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(obj) == APE_OBJECT_STRING);
    data = object_value_allocated_data(obj);
    if(data->string.hash == 0)
    {
        rawstr = object_string_getdata(obj);
        rawlen = object_string_getlength(obj);
        data->string.hash = util_hashstring(rawstr, rawlen);
        if(data->string.hash == 0)
        {
            data->string.hash = 1;
        }
    }
    return data->string.hash;
}


