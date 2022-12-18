
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

#define APE_LIKELY(x) \
    __builtin_expect(!!(x), 1)

#define APE_UNLIKELY(x) \
    __builtin_expect(!!(x), 0)


extern void* ds_extmalloc(size_t size, void* userptr);
extern void* ds_extrealloc(void* ptr, size_t oldsz, size_t newsz, void* userptr);
extern void ds_extfree(void* ptr, void* userptr);

/*
* must include *after* decl of ds_ext*, because they depend on it
*/
#include "dnarray.h"
#include "aadeque.h"
#include "sds.h"


/*
* initialize a memory pool for allocations between 2^min2 and 2^max2, inclusive.
* (Larger allocations will be directly allocated and freed via mmap / munmap.)
*/
ApeMemPool_t* ape_mempool_init(ApeSize_t min2, ApeSize_t max2);


bool ape_mempool_setdebughandle(ApeMemPool_t* mp, FILE* handle, bool mustclose);

bool ape_mempool_setdebugfile(ApeMemPool_t* mp, const char* path);
void ape_mempool_debugprintv(ApeMemPool_t* mp, const char* fmt, va_list va);
void ape_mempool_debugprintf(ApeMemPool_t* mp, const char* fmt, ...);

/* Allocate SZ bytes. */
void* ape_mempool_alloc(ApeMemPool_t* mp, ApeSize_t sz);

/* would release a pointer when mmap (or alternatives) are not available */
void ape_mempool_free(ApeMemPool_t* mp, void* p);

/*
* mmap a new memory pool of TOTAL_SZ bytes, then build an internal
* freelist of SZ-byte cells, with the head at (result)[0].
*/
void** ape_mempool_newpool(ApeMemPool_t* mp, ApeSize_t sz, ApeSize_t total_sz);

/* return pointer P (SZ bytes in size) to the appropriate pool. */
void ape_mempool_repool(ApeMemPool_t* mp, void *p, ApeSize_t sz);

/* resize P from OLD_SZ to NEW_SZ, copying content. */
void *ape_mempool_realloc(ApeMemPool_t* mp, void *p, ApeSize_t old_sz, ApeSize_t new_sz);

/* releases and destroys the pool. */
void ape_mempool_destroy(ApeMemPool_t* mp);

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
ApeUInt_t ape_util_floattouint(ApeFloat_t val);
ApeFloat_t ape_util_uinttofloat(ApeUInt_t val);


static APE_INLINE void* ape_valdict_getkeyat(const ApeValDict_t* dict, ApeSize_t ix)
{
    if(ix >= dict->count)
    {
        return NULL;
    }
    return ((char*)dict->keys) + (dict->keysize * ix);
}

static APE_INLINE void* ape_valdict_getvalueat(const ApeValDict_t* dict, ApeSize_t ix)
{
    if(ix >= dict->count)
    {
        return NULL;
    }
    return (char*)dict->values + (dict->valsize * ix);
}

static APE_INLINE bool ape_valdict_setvalueat(const ApeValDict_t* dict, ApeSize_t ix, const void* value)
{
    ApeSize_t offset;
    if(ix >= dict->count)
    {
        return false;
    }
    offset = ix * dict->valsize;
    memcpy((char*)dict->values + offset, value, dict->valsize);
    return true;
}

static APE_INLINE ApeSize_t ape_valdict_count(const ApeValDict_t* dict)
{
    if(!dict)
    {
        return 0;
    }
    return dict->count;
}

static APE_INLINE void ape_valdict_clear(ApeValDict_t* dict)
{
    ApeSize_t i;
    dict->count = 0;
    for(i = 0; i < dict->cellcap; i++)
    {
        dict->cells[i] = APE_CONF_INVALID_VALDICT_IX;
    }
}
