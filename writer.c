
#include "ape.h"

ApeWriter_t* ape_make_writer(ApeContext_t* ctx)
{
    return ape_make_writercapacity(ctx, 1);
}

ApeWriter_t* ape_make_writerdefault(ApeContext_t* ctx)
{
    ApeWriter_t* buf;
    buf = (ApeWriter_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeWriter_t));
    if(buf == NULL)
    {
        return NULL;
    }
    memset(buf, 0, sizeof(ApeWriter_t));
    buf->context = ctx;
    buf->alloc = &ctx->alloc;
    buf->failed = false;
    buf->datastring = NULL;
    buf->datacapacity = 0;
    buf->datalength = 0;
    buf->iohandle = NULL;
    buf->iowriter = NULL;
    buf->iomustclose = false;
    return buf;
}

ApeWriter_t* ape_make_writercapacity(ApeContext_t* ctx, ApeSize_t capacity)
{
    ApeWriter_t* buf;
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

ApeWriter_t* ape_make_writerio(ApeContext_t* ctx, FILE* hnd, bool alsoclose, bool alsoflush)
{
    ApeWriter_t* buf;
    buf = ape_make_writerdefault(ctx);
    buf->iohandle = hnd;
    buf->iowriter = wr_default_iowriter;
    buf->ioflusher = wr_default_ioflusher;
    buf->iomustclose = alsoclose;
    buf->iomustflush = alsoflush;
    return buf;
}

void ape_writer_destroy(ApeWriter_t* buf)
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
        ape_allocator_free(buf->alloc, buf->datastring);
    }
    ape_allocator_free(buf->alloc, buf);

}

bool ape_writer_appendn(ApeWriter_t* buf, const char* str, ApeSize_t str_len)
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

bool ape_writer_append(ApeWriter_t* buf, const char* str)
{
    return ape_writer_appendn(buf, str, strlen(str));
}

bool ape_writer_appendf(ApeWriter_t* buf, const char* fmt, ...)
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

    tbuf = (char*)ape_allocator_alloc(buf->alloc, required_capacity);
    va_start(args, fmt);
    written = vsnprintf(tbuf, required_capacity, fmt, args);
    va_end(args);
    ok = ape_writer_appendn(buf, tbuf, written);
    ape_allocator_free(buf->alloc, tbuf);
    return ok;
}

const char* ape_writer_getdata(const ApeWriter_t* buf)
{
    if(buf->failed)
    {
        return NULL;
    }
    return buf->datastring;
}

ApeSize_t ape_writer_getlength(const ApeWriter_t* buf)
{
    if(buf->failed)
    {
        return 0;
    }
    return buf->datalength;
}

char* ape_writer_getstringanddestroy(ApeWriter_t* buf)
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

bool ape_writer_failed(ApeWriter_t* buf)
{
    return buf->failed;
}

bool ape_writer_grow(ApeWriter_t* buf, ApeSize_t new_capacity)
{
    char* new_data;
    new_data = (char*)ape_allocator_alloc(buf->alloc, new_capacity);
    if(new_data == NULL)
    {
        buf->failed = true;
        return false;
    }
    memcpy(new_data, buf->datastring, buf->datalength);
    new_data[buf->datalength] = '\0';
    ape_allocator_free(buf->alloc, buf->datastring);
    buf->datastring = new_data;
    buf->datacapacity = new_capacity;
    return true;
}


