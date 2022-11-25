
#include "ape.h"


ApeWriter_t* writer_make(ApeContext_t* ctx)
{
    return writer_makecapacity(ctx, 1);
}

ApeWriter_t* writer_makedefault(ApeContext_t* ctx)
{
    ApeWriter_t* buf;
    buf = (ApeWriter_t*)allocator_malloc(&ctx->alloc, sizeof(ApeWriter_t));
    if(buf == NULL)
    {
        return NULL;
    }
    memset(buf, 0, sizeof(ApeWriter_t));
    buf->context = ctx;
    buf->alloc = &ctx->alloc;
    buf->failed = false;
    buf->stringdata = NULL;
    buf->capacity = 0;
    buf->len = 0;
    buf->iohandle = NULL;
    buf->iowriter = NULL;
    buf->iomustclose = false;
    return buf;
}

ApeWriter_t* writer_makecapacity(ApeContext_t* ctx, ApeSize_t capacity)
{
    ApeWriter_t* buf;
    buf = writer_makedefault(ctx);
    buf->stringdata = (char*)allocator_malloc(&ctx->alloc, capacity);
    if(buf->stringdata == NULL)
    {
        allocator_free(&ctx->alloc, buf);
        return NULL;
    }
    buf->capacity = capacity;
    buf->len = 0;
    buf->stringdata[0] = '\0';
    return buf;
}

static size_t wr_default_iowriter(ApeContext_t* ctx, void* hptr, const char* source, size_t len)
{
    (void)ctx;
    return fwrite(source, sizeof(char), len, (FILE*)hptr);
}

static void wr_default_ioflusher(ApeContext_t* ctx, void* hptr)
{
    (void)ctx;
    fflush((FILE*)hptr);
}

ApeWriter_t* writer_makeio(ApeContext_t* ctx, FILE* hnd, bool alsoclose, bool alsoflush)
{
    ApeWriter_t* buf;
    buf = writer_makedefault(ctx);
    buf->iohandle = hnd;
    buf->iowriter = wr_default_iowriter;
    buf->ioflusher = wr_default_ioflusher;
    buf->iomustclose = alsoclose;
    buf->iomustflush = alsoflush;
    return buf;
}

void writer_destroy(ApeWriter_t* buf)
{
    if(buf == NULL)
    {
        return;
    }
    if(buf->iohandle != NULL)
    {
        if(buf->iomustflush)
        {
            if(buf->ioflusher != NULL)
            {
                (buf->ioflusher)(buf->context, buf->iohandle);
            }
        }
        if(buf->iomustclose)
        {
            fclose((FILE*)(buf->iohandle));
        }
    }
    else
    {
        allocator_free(buf->alloc, buf->stringdata);
    }
    allocator_free(buf->alloc, buf);

}

bool writer_appendn(ApeWriter_t* buf, const char* str, ApeSize_t str_len)
{
    bool ok;
    ApeSize_t required_capacity;
    if(buf->failed)
    {
        return false;
    }
    if(str_len == 0)
    {
        return true;
    }
    if((buf->iohandle != NULL) && (buf->iowriter != NULL))
    {
        (buf->iowriter)(buf->context, buf->iohandle, str, str_len);
        if(buf->iomustflush)
        {
            // todo: this should also be a function pointer. just in case.
            fflush((FILE*)buf->iohandle);
        }
        return true;
    }
    required_capacity = buf->len + str_len + 1;
    if(required_capacity > buf->capacity)
    {
        ok = writer_grow(buf, required_capacity * 2);
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

bool writer_append(ApeWriter_t* buf, const char* str)
{
    return writer_appendn(buf, str, strlen(str));
}

bool writer_appendf(ApeWriter_t* buf, const char* fmt, ...)
{
    bool ok;
    ApeInt_t written;
    ApeInt_t to_write;
    ApeSize_t required_capacity;
    char* tbuf;
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
    /*
    if(required_capacity > buf->capacity)
    {
        ok = writer_grow(buf, required_capacity * 2);
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
    */

    tbuf = (char*)allocator_malloc(buf->alloc, required_capacity);
    va_start(args, fmt);
    written = vsnprintf(tbuf, required_capacity, fmt, args);
    va_end(args);
    ok = writer_appendn(buf, tbuf, written);
    allocator_free(buf->alloc, tbuf);
    return ok;
}

const char* writer_getdata(const ApeWriter_t* buf)
{
    if(buf->failed)
    {
        return NULL;
    }
    return buf->stringdata;
}

ApeSize_t writer_getlength(const ApeWriter_t* buf)
{
    if(buf->failed)
    {
        return 0;
    }
    return buf->len;
}

char* writer_getstringanddestroy(ApeWriter_t* buf)
{
    char* res;
    if(buf->failed)
    {
        writer_destroy(buf);
        return NULL;
    }
    res = buf->stringdata;
    buf->stringdata = NULL;
    writer_destroy(buf);
    return res;
}

bool writer_failed(ApeWriter_t* buf)
{
    return buf->failed;
}

bool writer_grow(ApeWriter_t* buf, ApeSize_t new_capacity)
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


