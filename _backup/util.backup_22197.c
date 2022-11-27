
#include <time.h>
#if defined(__linux__)
    #include <sys/time.h>
#endif
#include "ape.h"

char* util_stringfmt(ApeContext_t* ctx, const char* format, ...)
{
    int to_write;
    int written;
    char* res;
    va_list args;
    (void)written;
    va_start(args, format);
    to_write = vsnprintf(NULL, 0, format, args);
    va_end(args);
    va_start(args, format);
    res = (char*)allocator_malloc(&ctx->alloc, to_write + 1);
    if(!res)
    {
        return NULL;
    }
    written = vsprintf(res, format, args);
    APE_ASSERT(written == to_write);
    va_end(args);
    return res;
}

// fixme
ApeUInt_t util_double_to_uint64(ApeFloat_t val)
{
    return val;
    union
    {
        ApeUInt_t fltcast_uint64;
        ApeFloat_t fltcast_double;
    } temp = { .fltcast_double = val };
    return temp.fltcast_uint64;
}

ApeFloat_t util_uint64_to_double(ApeUInt_t val)
{
    return val;
    union
    {
        ApeUInt_t fltcast_uint64;
        ApeFloat_t fltcast_double;
    } temp = { .fltcast_uint64 = val };
    return temp.fltcast_double;
}


bool util_timer_supported()
{
#if defined(APE_POSIX) || defined(APE_EMSCRIPTEN) || defined(APE_WINDOWS)
    return true;
#else
    return false;
#endif
}

ApeTimer_t util_timer_start()
{
    ApeTimer_t timer;
    memset(&timer, 0, sizeof(ApeTimer_t));
/*
#if defined(APE_POSIX)
    // At some point it should be replaced with more accurate per-platform timers
    struct timeval start_time;
    gettimeofday(&start_time, NULL);
    timer.start_offset = start_time.tv_sec;
    timer.start_time_ms = start_time.tv_usec / 1000.0;
#elif defined(APE_WINDOWS)
    LARGE_INTEGER li;
    QueryPerformanceFrequency(&li);// not sure what to do if it fails
    timer.pc_frequency = (ApeFloat_t)(li.QuadPart) / 1000.0;
    QueryPerformanceCounter(&li);
    timer.start_time_ms = li.QuadPart / timer.pc_frequency;
#elif defined(APE_EMSCRIPTEN)
    timer.start_time_ms = emscripten_get_now();
#endif
*/
    return timer;
}

ApeFloat_t util_timer_getelapsed(const ApeTimer_t* timer)
{
/*
#if defined(APE_POSIX)
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    int time_s = (int)((ApeInt_t)current_time.tv_sec - timer->start_offset);
    ApeFloat_t current_time_ms = (time_s * 1000) + (current_time.tv_usec / 1000.0);
    return current_time_ms - timer->start_time_ms;
#elif defined(APE_WINDOWS)
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    ApeFloat_t current_time_ms = li.QuadPart / timer->pc_frequency;
    return current_time_ms - timer->start_time_ms;
#elif defined(APE_EMSCRIPTEN)
    ApeFloat_t current_time_ms = emscripten_get_now();
    return current_time_ms - timer->start_time_ms;
#endif
*/
    return 0;
}

char* util_strndup(ApeContext_t* ctx, const char* string, size_t n)
{
    char* output_string;
    output_string = (char*)allocator_malloc(&ctx->alloc, n + 1);
    if(!output_string)
    {
        return NULL;
    }
    output_string[n] = '\0';
    memcpy(output_string, string, n);
    return output_string;
}

char* util_strdup(ApeContext_t* ctx, const char* string)
{
    if(!string)
    {
        return NULL;
    }
    return util_strndup(ctx, string, strlen(string));
}

/* djb2 */
unsigned long util_hashstring(const void* ptr, size_t len)
{
    size_t i;
    unsigned long hash;
    ApeUShort_t val;
    const ApeUShort_t* ptr_u8;
    ptr_u8 = (const ApeUShort_t*)ptr;
    hash = 5381;
    for(i = 0; i < len; i++)
    {
        val = ptr_u8[i];
        hash = ((hash << 5) + hash) + val;
    }
    return hash;
}

/* djb2 */
unsigned long util_hashdouble(ApeFloat_t val)
{
    uintptr_t* vptr;
    unsigned long hash;
    vptr = (uintptr_t*)&val;
    hash = 5381;
    hash = ((hash << 5) + hash) + vptr[0];
    hash = ((hash << 5) + hash) + vptr[1];
    return hash;
}

unsigned int util_upperpoweroftwo(unsigned int v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

char* util_default_readfile(void* ptr, const char* filename)
{
    long pos;
    size_t size_read;
    size_t size_to_read;
    char* file_contents;
    FILE* fp;
    ApeContext_t* ctx;
    ctx = (ApeContext_t*)ptr;
    fp = fopen(filename, "r");
    size_to_read = 0;
    size_read = 0;
    if(!fp)
    {
        return NULL;
    }
    fseek(fp, 0L, SEEK_END);
    pos = ftell(fp);
    if(pos < 0)
    {
        fclose(fp);
        return NULL;
    }
    size_to_read = pos;
    rewind(fp);
    file_contents = (char*)allocator_malloc(&ctx->alloc, sizeof(char) * (size_to_read + 1));
    if(!file_contents)
    {
        fclose(fp);
        return NULL;
    }
    size_read = fread(file_contents, 1, size_to_read, fp);
    if(ferror(fp))
    {
        fclose(fp);
        free(file_contents);
        return NULL;
    }
    fclose(fp);
    file_contents[size_read] = '\0';
    return file_contents;
}

size_t util_default_writefile(void* ctx, const char* path, const char* string, size_t string_size)
{
    size_t written;
    FILE* fp;
    (void)ctx;
    fp = fopen(path, "w");
    if(!fp)
    {
        return 0;
    }
    written = fwrite(string, 1, string_size, fp);
    fclose(fp);
    return written;
}

size_t util_default_stdoutwrite(void* ctx, const void* data, size_t size)
{
    (void)ctx;
    return fwrite(data, 1, size, stdout);
}

ApePtrArray_t * util_split_string(ApeContext_t* ctx, const char* str, const char* delimiter)
{
    ApeSize_t i;
    long len;
    bool ok;
    ApePtrArray_t* res;
    char* line;
    char* rest;
    const char* line_start;
    const char* line_end;
    ok = false;
    res = ptrarray_make(ctx);
    rest = NULL;
    if(!str)
    {
        return res;
    }
    line_start = str;
    line_end = strstr(line_start, delimiter);
    while(line_end != NULL)
    {
        len = line_end - line_start;
        line = util_strndup(ctx, line_start, len);
        if(!line)
        {
            goto err;
        }
        ok = ptrarray_add(res, line);
        if(!ok)
        {
            allocator_free(&ctx->alloc, line);
            goto err;
        }
        line_start = line_end + 1;
        line_end = strstr(line_start, delimiter);
    }
    rest = util_strdup(ctx, line_start);
    if(!rest)
    {
        goto err;
    }
    ok = ptrarray_add(res, rest);
    if(!ok)
    {
        goto err;
    }
    return res;
err:
    allocator_free(&ctx->alloc, rest);
    if(res)
    {
        for(i = 0; i < ptrarray_count(res); i++)
        {
            line = (char*)ptrarray_get(res, i);
            allocator_free(&ctx->alloc, line);
        }
    }
    ptrarray_destroy(res);
    return NULL;
}

char* util_join(ApeContext_t* ctx, ApePtrArray_t * items, const char* with)
{
    ApeSize_t i;
    char* item;
    ApeWriter_t* res;
    res = ape_make_writer(ctx);
    if(!res)
    {
        return NULL;
    }
    for(i = 0; i < ptrarray_count(items); i++)
    {
        item = (char*)ptrarray_get(items, i);
        ape_writer_append(res, item);
        if(i < (ptrarray_count(items) - 1))
        {
            ape_writer_append(res, with);
        }
    }
    return ape_writer_getstringanddestroy(res);
}

char* util_canonicalisepath(ApeContext_t* ctx, const char* path)
{
    ApeSize_t i;
    char* joined;
    char* item;
    char* next_item;
    void* ptritem;
    ApePtrArray_t* split;
    if(!strchr(path, '/') || (!strstr(path, "/../") && !strstr(path, "./")))
    {
        return util_strdup(ctx, path);
    }
    split = util_split_string(ctx, path, "/");
    if(!split)
    {
        return NULL;
    }
    for(i = 0; i < ptrarray_count(split) - 1; i++)
    {
        item = (char*)ptrarray_get(split, i);
        next_item = (char*)ptrarray_get(split, i + 1);
        if(util_strequal(item, "."))
        {
            allocator_free(&ctx->alloc, item);
            ptrarray_removeat(split, i);
            i = -1;
            continue;
        }
        if(util_strequal(next_item, ".."))
        {
            allocator_free(&ctx->alloc, item);
            allocator_free(&ctx->alloc, next_item);
            ptrarray_removeat(split, i);
            ptrarray_removeat(split, i);
            i = -1;
            continue;
        }
    }
    joined = util_join(ctx, split, "/");

    for(i = 0; i < ptrarray_count(split); i++)
    {
        ptritem = (void*)ptrarray_get(split, i);
        allocator_free(&ctx->alloc, ptritem);
    }
    ptrarray_destroy(split);
    return joined;
}

bool util_is_absolutepath(const char* path)
{
    return path[0] == '/';
}

bool util_strequal(const char* a, const char* b)
{
    return strcmp(a, b) == 0;
}



