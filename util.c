
/*
* fp-critical code looms below.
* no touchy, unless you know what you're getting yourself into.
*/

#define _XOPEN_SOURCE 500
#include <time.h>
#if defined(__linux__)
    #include <sys/time.h>
#endif
#include "inline.h"

/*
* https://beej.us/guide/bgnet/examples/ieee754.c
* https://beej.us/guide/bgnet/html/split/slightly-advanced-techniques.html#serialization
* https://en.wikipedia.org/wiki/IEEE_754
*/
#define pack754_32(f) (pack754((f), 32, 8))
#define pack754_64(f) (pack754((f), 64, 11))
#define unpack754_32(i) (unpack754((i), 32, 8))
#define unpack754_64(i) (unpack754((i), 64, 11))

uint64_t pack754(long double f, unsigned bits, unsigned expbits)
{
    long double fnorm;
    int shift;
    long long sign;
    long long exp;
    long long significand;
    unsigned significandbits;
    /* -1 for sign bit */
    significandbits = bits - expbits - 1;
    /* get this special case out of the way */
    if(f == 0.0)
    {
        return 0;
    }
    /* check sign and begin normalization */
    if(f < 0)
    {
        sign = 1;
        fnorm = -f;
    }
    else
    {
        sign = 0;
        fnorm = f;
    }
    /* get the normalized form of f and track the exponent */
    shift = 0;
    while(fnorm >= 2.0)
    {
        fnorm /= 2.0;
        shift++;
    }
    while(fnorm < 1.0)
    {
        fnorm *= 2.0;
        shift--;
    }
    fnorm = fnorm - 1.0;
    /* calculate the binary form (non-float) of the significand data */
    significand = fnorm * ((1LL << significandbits) + 0.5f);
    /* get the biased exponent */
    /* shift + bias */
    exp = shift + ((1<<(expbits-1)) - 1);
    /* return the final answer */
    return (
        (sign << (bits - 1)) | (exp << (bits - expbits - 1)) | significand
    );
}

long double unpack754(uint64_t i, unsigned bits, unsigned expbits)
{
    long double result;
    long long shift;
    unsigned bias;
    unsigned significandbits;
    /* -1 for sign bit */
    significandbits = bits - expbits - 1;
    if(i == 0)
    {
        return 0.0;
    }
    /* pull the significand */
    /* mask */
    result = (i&((1LL<<significandbits)-1));
    /* convert back to float */
    result /= (1LL<<significandbits);
    /* add the one back on */
    result += 1.0f;
    /* deal with the exponent */
    bias = ((1 << (expbits - 1)) - 1);
    shift = (((i >> significandbits) & ((1LL << expbits) - 1)) - bias);
    while(shift > 0)
    {
        result *= 2.0;
        shift--;
    }
    while(shift < 0)
    {
        result /= 2.0;
        shift++;
    }
    /* sign it */
    if(((i>>(bits-1)) & 1) == 1)
    {
        result = result * -1.0;
    }
    else
    {
        result = result * 1.0;
    }
    return result;
}


ApeFloat_t ape_util_uinttofloat(ApeUInt_t val)
{
    return unpack754_64(val);
}

ApeUInt_t ape_util_floattouint(ApeFloat_t val)
{
    return pack754_64(val);
}

char* ape_util_stringfmt(ApeContext_t* ctx, const char* format, ...)
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
    res = (char*)ape_allocator_alloc(&ctx->alloc, to_write + 1);
    if(!res)
    {
        return NULL;
    }
    written = vsprintf(res, format, args);
    APE_ASSERT(written == to_write);
    va_end(args);
    return res;
}

bool ape_util_timersupported()
{
#if defined(APE_POSIX) || defined(APE_EMSCRIPTEN) || defined(APE_WINDOWS)
    return true;
#else
    return false;
#endif
}

ApeTimer_t ape_util_timerstart()
{
    ApeTimer_t timer;
    memset(&timer, 0, sizeof(ApeTimer_t));
    #if 0
        #if defined(APE_POSIX)
            /* At some point it should be replaced with more accurate per-platform timers */
            struct timeval start_time;
            gettimeofday(&start_time, NULL);
            timer.start_offset = start_time.tv_sec;
            timer.start_time_ms = start_time.tv_usec / 1000.0;
        #elif defined(APE_WINDOWS)
            LARGE_INTEGER li;
            /* not sure what to do if it fails */
            QueryPerformanceFrequency(&li);
            timer.pc_frequency = (ApeFloat_t)(li.QuadPart) / 1000.0;
            QueryPerformanceCounter(&li);
            timer.start_time_ms = li.QuadPart / timer.pc_frequency;
        #elif defined(APE_EMSCRIPTEN)
            timer.start_time_ms = emscripten_get_now();
        #endif
    #endif
    return timer;
}

ApeFloat_t ape_util_timergetelapsed(const ApeTimer_t* timer)
{
    (void)timer;
    #if 0
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
    #endif
    return 0;
}

char* ape_util_strndup(ApeContext_t* ctx, const char* string, size_t n)
{
    char* output_string;
    output_string = (char*)ape_allocator_alloc(&ctx->alloc, n + 1);
    if(!output_string)
    {
        return NULL;
    }
    output_string[n] = '\0';
    memcpy(output_string, string, n);
    return output_string;
}

char* ape_util_strdup(ApeContext_t* ctx, const char* string)
{
    if(!string)
    {
        return NULL;
    }
    return ape_util_strndup(ctx, string, strlen(string));
}

/* djb2 */
unsigned long ape_util_hashstring(const void* ptr, size_t len)
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
unsigned long ape_util_hashfloat(ApeFloat_t val)
{
    uintptr_t* vptr;
    unsigned long hash;
    vptr = (uintptr_t*)&val;
    hash = 5381;
    hash = ((hash << 5) + hash) + vptr[0];
    hash = ((hash << 5) + hash) + vptr[1];
    return hash;
}

unsigned int ape_util_upperpoweroftwo(unsigned int v)
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

char* ape_util_default_readfile(void* ptr, const char* filename)
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
    file_contents = (char*)ape_allocator_alloc(&ctx->alloc, sizeof(char) * (size_to_read + 1));
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

size_t ape_util_default_writefile(void* ctx, const char* path, const char* string, size_t string_size)
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

size_t ape_util_default_stdoutwrite(void* ctx, const void* data, size_t size)
{
    (void)ctx;
    return fwrite(data, 1, size, stdout);
}

ApePtrArray_t * ape_util_splitstring(ApeContext_t* ctx, const char* str, const char* delimiter)
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
    res = ape_make_ptrarray(ctx);
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
        line = ape_util_strndup(ctx, line_start, len);
        if(!line)
        {
            goto err;
        }
        ok = ape_ptrarray_push(res, &line);
        if(!ok)
        {
            ape_allocator_free(&ctx->alloc, line);
            goto err;
        }
        line_start = line_end + 1;
        line_end = strstr(line_start, delimiter);
    }
    rest = ape_util_strdup(ctx, line_start);
    if(!rest)
    {
        goto err;
    }
    ok = ape_ptrarray_push(res, &rest);
    if(!ok)
    {
        goto err;
    }
    return res;
err:
    ape_allocator_free(&ctx->alloc, rest);
    if(res)
    {
        for(i = 0; i < ape_ptrarray_count(res); i++)
        {
            line = (char*)ape_ptrarray_get(res, i);
            ape_allocator_free(&ctx->alloc, line);
        }
    }
    ape_ptrarray_destroy(res);
    return NULL;
}

char* ape_util_joinarray(ApeContext_t* ctx, ApePtrArray_t * items, const char* with)
{
    ApeSize_t i;
    char* item;
    ApeWriter_t* res;
    res = ape_make_writer(ctx);
    if(!res)
    {
        return NULL;
    }
    for(i = 0; i < ape_ptrarray_count(items); i++)
    {
        item = (char*)ape_ptrarray_get(items, i);
        ape_writer_append(res, item);
        if(i < (ape_ptrarray_count(items) - 1))
        {
            ape_writer_append(res, with);
        }
    }
    return ape_writer_getstringanddestroy(res);
}

char* ape_util_canonicalisepath(ApeContext_t* ctx, const char* path)
{
    ApeSize_t i;
    char* joined;
    char* item;
    char* next_item;
    void* ptritem;
    ApePtrArray_t* split;
    if(!strchr(path, '/') || (!strstr(path, "/../") && !strstr(path, "./")))
    {
        return ape_util_strdup(ctx, path);
    }
    split = ape_util_splitstring(ctx, path, "/");
    if(!split)
    {
        return NULL;
    }
    for(i = 0; i < ape_ptrarray_count(split) - 1; i++)
    {
        item = (char*)ape_ptrarray_get(split, i);
        next_item = (char*)ape_ptrarray_get(split, i + 1);
        if(ape_util_strequal(item, "."))
        {
            ape_allocator_free(&ctx->alloc, item);
            ape_ptrarray_removeat(split, i);
            i = -1;
            continue;
        }
        if(ape_util_strequal(next_item, ".."))
        {
            ape_allocator_free(&ctx->alloc, item);
            ape_allocator_free(&ctx->alloc, next_item);
            ape_ptrarray_removeat(split, i);
            ape_ptrarray_removeat(split, i);
            i = -1;
            continue;
        }
    }
    joined = ape_util_joinarray(ctx, split, "/");

    for(i = 0; i < ape_ptrarray_count(split); i++)
    {
        ptritem = (void*)ape_ptrarray_get(split, i);
        ape_allocator_free(&ctx->alloc, ptritem);
    }
    ape_ptrarray_destroy(split);
    return joined;
}

bool ape_util_isabspath(const char* path)
{
    return path[0] == '/';
}

bool ape_util_strequal(const char* a, const char* b)
{
    return strcmp(a, b) == 0;
}



