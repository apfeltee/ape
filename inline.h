
#pragma once
/*
* this file contains code that is intended to be inlined.
* it is *not* required in production code; only ape.h is.
*/

#if !defined(_ISOC99_SOURCE)
    #define _ISOC99_SOURCE
#endif

#if !defined(_POSIX_C_SOURCE)
    #define _POSIX_C_SOURCE 200112L
#endif

#include <signal.h>
#include <math.h>
#include "ape.h"

#if defined(__GNUC__)
    #if !defined(isfinite)
        #define isfinite(v) (__builtin_isfinite(v))
    #endif
#endif

#if !defined(va_copy)
    #if defined(__GNUC__)
        #define va_copy(d,s) __builtin_va_copy(d,s)
    #else
        /* https://stackoverflow.com/questions/558223/va-copy-porting-to-visual-c */
        #define va_copy(d,s) ((d) = (s))
    #endif
#endif

#if defined(__GNUC__)
    #define APE_LIKELY(x) \
        __builtin_expect(!!(x), 1)

    #define APE_UNLIKELY(x) \
        __builtin_expect(!!(x), 0)
#else
    #define APE_LIKELY(x) x
    #define APE_UNLIKELY(x) x
#endif

extern void* ds_extmalloc(size_t size, void* userptr);
extern void* ds_extrealloc(void* ptr, size_t oldsz, size_t newsz, void* userptr);
extern void ds_extfree(void* ptr, void* userptr);

/*
* must include *after* decl of ds_ext*, because they depend on it
*/
#include "dnarray.h"
#include "sds.h"


/*
* initialize a memory pool for allocations between 2^min2 and 2^max2, inclusive.
* (Larger allocations will be directly allocated and freed via mmap / munmap.)
*/
ApeMemPool* ape_mempool_init(ApeSize min2, ApeSize max2);


bool ape_mempool_setdebughandle(ApeMemPool* mp, FILE* handle, bool mustclose);

bool ape_mempool_setdebugfile(ApeMemPool* mp, const char* path);
void ape_mempool_debugprintv(ApeMemPool* mp, const char* fmt, va_list va);
void ape_mempool_debugprintf(ApeMemPool* mp, const char* fmt, ...);

/* Allocate SZ bytes. */
void* ape_mempool_alloc(ApeMemPool* mp, ApeSize sz);

/* would release a pointer when mmap (or alternatives) are not available */
void ape_mempool_free(ApeMemPool* mp, void* p);

/*
* mmap a new memory pool of TOTAL_SZ bytes, then build an internal
* freelist of SZ-byte cells, with the head at (result)[0].
*/
void** ape_mempool_newpool(ApeMemPool* mp, ApeSize sz, ApeSize total_sz);

/* return pointer P (SZ bytes in size) to the appropriate pool. */
void ape_mempool_repool(ApeMemPool* mp, void *p, ApeSize sz);

/* resize P from OLD_SZ to NEW_SZ, copying content. */
void *ape_mempool_realloc(ApeMemPool* mp, void *p, ApeSize old_sz, ApeSize new_sz);

/* releases and destroys the pool. */
void ape_mempool_destroy(ApeMemPool* mp);

static APE_INLINE int ape_util_doubletoint(double n)
{
    if(n == 0)
    {
        return 0;
    }
    if(isnan(n))
    {
        return 0;
    }
    if(n < 0)
    {
        n = -floor(-n);
    }
    else
    {
        n = floor(n);
    }
    if(n < INT_MIN)
    {
        return INT_MIN;
    }
    if(n > INT_MAX)
    {
        return INT_MAX;
    }
    return (int)n;
}

static APE_INLINE int ape_util_numbertoint32(double n)
{
    /* magic. no idea. */
    double two32 = 4294967296.0;
    double two31 = 2147483648.0;
    if(!isfinite(n) || n == 0)
    {
        return 0;
    }
    n = fmod(n, two32);
    if(n >= 0)
    {
        n = floor(n);
    }
    else
    {
        n = ceil(n) + two32;
    }
    if(n >= two31)
    {
        return n - two32;
    }
    return n;
}

static APE_INLINE unsigned int ape_util_numbertouint32(double n)
{
    return (unsigned int)ape_util_numbertoint32(n);
}

/* fixme */
ApeUInt ape_util_floattouint(ApeFloat val);
ApeFloat ape_util_uinttofloat(ApeUInt val);


static APE_INLINE void* ape_valdict_getkeyat(const ApeValDict* dict, ApeSize ix)
{
    if(ix >= dict->count)
    {
        return NULL;
    }
    return ((char*)dict->keys) + (dict->keysize * ix);
}

static APE_INLINE void* ape_valdict_getvalueat(const ApeValDict* dict, ApeSize ix)
{
    if(ix >= dict->count)
    {
        return NULL;
    }
    return (char*)dict->values + (dict->valsize * ix);
}

static APE_INLINE bool ape_valdict_setvalueat(const ApeValDict* dict, ApeSize ix, const void* value)
{
    ApeSize offset;
    if(ix >= dict->count)
    {
        return false;
    }
    offset = ix * dict->valsize;
    memcpy((char*)dict->values + offset, value, dict->valsize);
    return true;
}

static APE_INLINE ApeSize ape_valdict_count(const ApeValDict* dict)
{
    if(!dict)
    {
        return 0;
    }
    return dict->count;
}

static APE_INLINE void ape_valdict_clear(ApeValDict* dict)
{
    ApeSize i;
    dict->count = 0;
    for(i = 0; i < dict->cellcap; i++)
    {
        dict->cells[i] = APE_CONF_INVALID_VALDICT_IX;
    }
}
