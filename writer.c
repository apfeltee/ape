
#include "inline.h"

ApeWriter* ape_make_writer(ApeContext* ctx)
{
    return ape_make_writercapacity(ctx, 1);
}

ApeWriter* ape_make_writerdefault(ApeContext* ctx)
{
    ApeWriter* buf;
    buf = (ApeWriter*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeWriter));
    if(buf == NULL)
    {
        return NULL;
    }
    memset(buf, 0, sizeof(ApeWriter));
    buf->context = ctx;
    buf->failed = false;
    buf->datastring = NULL;
    buf->datacapacity = 0;
    buf->datalength = 0;
    buf->iohandle = NULL;
    buf->iowriter = NULL;
    buf->iomustclose = false;
    return buf;
}

ApeWriter* ape_make_writercapacity(ApeContext* ctx, ApeSize capacity)
{
    ApeWriter* buf;
    buf = ape_make_writerdefault(ctx);
    buf->datastring = (char*)ape_allocator_alloc(&ctx->alloc, capacity);
    if(buf->datastring == NULL)
    {
        ape_allocator_free(&ctx->alloc, buf);
        return NULL;
    }
    buf->datacapacity = capacity;
    buf->datalength = 0;
    buf->datastring[0] = '\0';
    return buf;
}

static size_t wr_default_iowriter(ApeContext* ctx, void* hptr, const char* source, size_t len)
{
    (void)ctx;
    return fwrite(source, sizeof(char), len, (FILE*)hptr);
}

static void wr_default_ioflusher(ApeContext* ctx, void* hptr)
{
    (void)ctx;
    fflush((FILE*)hptr);
}

ApeWriter* ape_make_writerio(ApeContext* ctx, FILE* hnd, bool alsoclose, bool alsoflush)
{
    ApeWriter* buf;
    buf = ape_make_writerdefault(ctx);
    buf->iohandle = hnd;
    buf->iowriter = wr_default_iowriter;
    buf->ioflusher = wr_default_ioflusher;
    buf->iomustclose = alsoclose;
    buf->iomustflush = alsoflush;
    return buf;
}

void ape_writer_destroy(ApeWriter* buf)
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
        ape_allocator_free(&buf->context->alloc, buf->datastring);
    }
    ape_allocator_free(&buf->context->alloc, buf);

}

bool ape_writer_appendlen(ApeWriter* buf, const char* str, ApeSize str_len)
{
    bool ok;
    ApeSize required_capacity;
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
            /* todo: this should also be a function pointer. just in case. */
            fflush((FILE*)buf->iohandle);
        }
        return true;
    }
    required_capacity = buf->datalength + str_len + 1;
    if(required_capacity > buf->datacapacity)
    {
        ok = ape_writer_grow(buf, required_capacity * 2);
        if(!ok)
        {
            return false;
        }
    }
    memcpy(buf->datastring + buf->datalength, str, str_len);
    buf->datalength = buf->datalength + str_len;
    buf->datastring[buf->datalength] = '\0';
    return true;
}

bool ape_writer_append(ApeWriter* buf, const char* str)
{
    return ape_writer_appendlen(buf, str, strlen(str));
}

bool ape_writer_appendf(ApeWriter* buf, const char* fmt, ...)
{
    bool ok;
    ApeInt written;
    ApeInt to_write;
    ApeSize required_capacity;
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
    required_capacity = buf->datalength + to_write + 1;
    /*
    if(required_capacity > buf->datacapacity)
    {
        ok = ape_writer_grow(buf, required_capacity * 2);
        if(!ok)
        {
            return false;
        }
    }
    va_start(args, fmt);
    written = vsprintf(buf->datastring + buf->datalength, fmt, args);
    va_end(args);
    if(written != to_write)
    {
        return false;
    }
    buf->datalength = buf->datalength + to_write;
    buf->datastring[buf->datalength] = '\0';
    */

    tbuf = (char*)ape_allocator_alloc(&buf->context->alloc, required_capacity);
    va_start(args, fmt);
    written = vsnprintf(tbuf, required_capacity, fmt, args);
    va_end(args);
    ok = ape_writer_appendlen(buf, tbuf, written);
    ape_allocator_free(&buf->context->alloc, tbuf);
    return ok;
}

const char* ape_writer_getdata(const ApeWriter* buf)
{
    if(buf->failed)
    {
        return NULL;
    }
    return buf->datastring;
}

const char* ape_writer_getstring(const ApeWriter* buf)
{
    return ape_writer_getdata(buf);
}

ApeSize ape_writer_getlength(const ApeWriter* buf)
{
    if(buf->failed)
    {
        return 0;
    }
    return buf->datalength;
}

char* ape_writer_getstringanddestroy(ApeWriter* buf)
{
    char* res;
    if(buf->failed)
    {
        ape_writer_destroy(buf);
        return NULL;
    }
    res = buf->datastring;
    buf->datastring = NULL;
    ape_writer_destroy(buf);
    return res;
}

bool ape_writer_failed(ApeWriter* buf)
{
    return buf->failed;
}

bool ape_writer_grow(ApeWriter* buf, ApeSize new_capacity)
{
    char* new_data;
    new_data = (char*)ape_allocator_alloc(&buf->context->alloc, new_capacity);
    if(new_data == NULL)
    {
        buf->failed = true;
        return false;
    }
    memcpy(new_data, buf->datastring, buf->datalength);
    new_data[buf->datalength] = '\0';
    ape_allocator_free(&buf->context->alloc, buf->datastring);
    buf->datastring = new_data;
    buf->datacapacity = new_capacity;
    return true;
}


