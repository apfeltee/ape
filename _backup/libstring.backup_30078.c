
#include "ape.h"


ApeObject_t object_make_stringlen(ApeGCMemory_t* mem, const char* string, size_t len)
{
    bool ok;
    ApeObject_t res;
    res = object_make_stringcapacity(mem, len);
    if(object_value_isnull(res))
    {
        return res;
    }
    ok = object_string_append(res, string, len);
    if(!ok)
    {
        return object_make_null();
    }
    return res;
}

ApeObject_t object_make_string(ApeGCMemory_t* mem, const char* string)
{
    return object_make_stringlen(mem, string, strlen(string));
}

ApeObject_t object_make_stringcapacity(ApeGCMemory_t* mem, int capacity)
{
    bool ok;
    ApeObjectData_t* data;
    data = gcmem_get_object_data_from_pool(mem, APE_OBJECT_STRING);
    if(!data)
    {
        data = gcmem_alloc_object_data(mem, APE_OBJECT_STRING);
        if(!data)
        {
            return object_make_null();
        }
        data->string.capacity = OBJECT_STRING_BUF_SIZE - 1;
        data->string.is_allocated = false;
    }
    data->string.length = 0;
    data->string.hash = 0;
    if(capacity > data->string.capacity)
    {
        ok = object_string_reservecapacity(data, capacity);
        if(!ok)
        {
            return object_make_null();
        }
    }
    return object_make_from_data(APE_OBJECT_STRING, data);
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

bool object_string_reservecapacity(ApeObjectData_t* data, int capacity)
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
    if(capacity <= (OBJECT_STRING_BUF_SIZE - 1))
    {
        if(string->is_allocated)
        {
            APE_ASSERT(false);// should never happen
            allocator_free(data->mem->alloc, string->value_allocated);// just in case
        }
        string->capacity = OBJECT_STRING_BUF_SIZE - 1;
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

int object_string_getlength(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_STRING);
    data = object_value_allocated_data(object);
    return data->string.length;
}

void object_string_setlength(ApeObject_t object, int len)
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

bool object_string_append(ApeObject_t obj, const char* src, int len)
{
    int capacity;
    int current_len;
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


ApeStringBuffer_t* strbuf_make(ApeAllocator_t* alloc)
{
    return strbuf_makecapacity(alloc, 1);
}

ApeStringBuffer_t* strbuf_makecapacity(ApeAllocator_t* alloc, unsigned int capacity)
{
    ApeStringBuffer_t* buf;
    buf = (ApeStringBuffer_t*)allocator_malloc(alloc, sizeof(ApeStringBuffer_t));
    if(buf == NULL)
    {
        return NULL;
    }
    memset(buf, 0, sizeof(ApeStringBuffer_t));
    buf->alloc = alloc;
    buf->failed = false;
    buf->stringdata = (char*)allocator_malloc(alloc, capacity);
    if(buf->stringdata == NULL)
    {
        allocator_free(alloc, buf);
        return NULL;
    }
    buf->capacity = capacity;
    buf->len = 0;
    buf->stringdata[0] = '\0';
    return buf;
}

void strbuf_destroy(ApeStringBuffer_t* buf)
{
    if(buf == NULL)
    {
        return;
    }
    allocator_free(buf->alloc, buf->stringdata);
    allocator_free(buf->alloc, buf);
}

bool strbuf_appendn(ApeStringBuffer_t* buf, const char* str, size_t str_len)
{
    bool ok;
    size_t required_capacity;
    if(buf->failed)
    {
        return false;
    }
    if(str_len == 0)
    {
        return true;
    }
    required_capacity = buf->len + str_len + 1;
    if(required_capacity > buf->capacity)
    {
        ok = strbuf_grow(buf, required_capacity * 2);
        if(!ok)
        {
            return false;
        }
    }
    memcpy(buf->stringdata + buf->len, str, str_len);
    buf->len = buf->len + str_len;
    buf->stringdata[buf->len] = '\0';
    return true;
}


bool strbuf_append(ApeStringBuffer_t* buf, const char* str)
{
    return strbuf_appendn(buf, str, strlen(str));
}

bool strbuf_appendf(ApeStringBuffer_t* buf, const char* fmt, ...)
{
    bool ok;
    int written;
    int to_write;
    size_t required_capacity;
    va_list args;
    (void)written;
    if(buf->failed)
    {
        return false;
    }
    va_start(args, fmt);
    to_write = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if(to_write == 0)
    {
        return true;
    }
    required_capacity = buf->len + to_write + 1;
    if(required_capacity > buf->capacity)
    {
        ok = strbuf_grow(buf, required_capacity * 2);
        if(!ok)
        {
            return false;
        }
    }
    va_start(args, fmt);
    written = vsprintf(buf->stringdata + buf->len, fmt, args);
    va_end(args);
    if(written != to_write)
    {
        return false;
    }
    buf->len = buf->len + to_write;
    buf->stringdata[buf->len] = '\0';
    return true;
}

const char* strbuf_getdata(const ApeStringBuffer_t* buf)
{
    if(buf->failed)
    {
        return NULL;
    }
    return buf->stringdata;
}

size_t strbuf_getlength(const ApeStringBuffer_t* buf)
{
    if(buf->failed)
    {
        return 0;
    }
    return buf->len;
}

char* strbuf_getstringanddestroy(ApeStringBuffer_t* buf)
{
    char* res;
    if(buf->failed)
    {
        strbuf_destroy(buf);
        return NULL;
    }
    res = buf->stringdata;
    buf->stringdata = NULL;
    strbuf_destroy(buf);
    return res;
}

bool strbuf_failed(ApeStringBuffer_t* buf)
{
    return buf->failed;
}

bool strbuf_grow(ApeStringBuffer_t* buf, size_t new_capacity)
{
    char* new_data;
    new_data = (char*)allocator_malloc(buf->alloc, new_capacity);
    if(new_data == NULL)
    {
        buf->failed = true;
        return false;
    }
    memcpy(new_data, buf->stringdata, buf->len);
    new_data[buf->len] = '\0';
    allocator_free(buf->alloc, buf->stringdata);
    buf->stringdata = new_data;
    buf->capacity = new_capacity;
    return true;
}


