
#pragma once
/*
* this file contains code that is intended to be inlined.
* it is *not* required in production code; only ape.h is.
*/
#define _ISOC99_SOURCE
#define _POSIX_C_SOURCE 200112L

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

//#if defined(APE_DEBUG)
#if 0
    #define ape_allocator_alloc(alc, sz) \
        ape_allocator_alloc_debug(alc, #sz, __FUNCTION__, __FILE__, __LINE__, sz)
#else
    #define ape_allocator_alloc(alc, sz) \
        ape_allocator_alloc_real(alc, sz)
#endif

#include "dnarray.h"

#if defined(APE_USE_ALIST) && (APE_USE_ALIST == 1)
    /*
    #define memlist_make(ctx, self) \
        alist_new()

    #define memlist_destroy(list) \
        alist_destroy(list)

    #define memlist_at(list, ix) \
        alist_at(list, ix)

    #define memlist_count(list) \
        ((list)->size)

    #define memlist_append(list, item) \
        alist_append(list, item)

    #define memlist_clear(list) \
        alist_clear(list)
    */
    #define memlist_make(ctx, self) \
        da_init(self, 2)

    #define memlist_destroy(list) \
        da_free(list)

    #define memlist_at(list, ix) \
        ((list)[ix])

    #define memlist_count(list) \
        da_count(list)

    #define memlist_append(list, item) \
        da_push(list, item)

    #define memlist_clear(list) \
        da_clear(list)

#else
    #define memlist_make(ctx, self) \
        ape_make_ptrarray(ctx)

    #define memlist_destroy(list) \
        ape_ptrarray_destroy(list)

    #define memlist_at(list, ix) \
        ape_ptrarray_get(list, ix)

    #define memlist_count(list) \
        ape_ptrarray_count(list)

    #define memlist_append(list, item) \
        ape_ptrarray_push(list, item)

    #define memlist_clear(list) \
        ape_ptrarray_clear(list)
#endif

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
    double two32 = 4294967296.0;
    double two31 = 2147483648.0;

    if(!isfinite(n) || n == 0)
    {
        return 0;
    }
    n = fmod(n, two32);
    n = n >= 0 ? floor(n) : ceil(n) + two32;
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
static APE_INLINE ApeUInt_t ape_util_floattouint(ApeFloat_t val)
{
    return val;
    union
    {
        ApeUInt_t fltcast_uint64;
        ApeFloat_t fltcast_double;
    } temp = { .fltcast_double = val };
    return temp.fltcast_uint64;
}

static APE_INLINE ApeFloat_t ape_util_uinttofloat(ApeUInt_t val)
{
    return val;
    union
    {
        ApeUInt_t fltcast_uint64;
        ApeFloat_t fltcast_double;
    } temp = { .fltcast_uint64 = val };
    return temp.fltcast_double;
}

static APE_INLINE ApeSize_t ape_valarray_count(const ApeValArray_t* arr)
{
    if(!arr)
    {
        return 0;
    }
    return arr->count;
}

static APE_INLINE ApeSize_t ape_ptrarray_count(const ApePtrArray_t* arr)
{
    if(!arr)
    {
        return 0;
    }
    return ape_valarray_count(&arr->arr);
}

static APE_INLINE bool ape_valarray_set(ApeValArray_t* arr, ApeSize_t ix, void* value)
{
    ApeSize_t offset;
    if(ix >= arr->count)
    {
        APE_ASSERT(false);
        return false;
    }
    offset = ix * arr->elemsize;
    memmove(arr->arraydata + offset, value, arr->elemsize);
    return true;
}

static APE_INLINE void* ape_valarray_get(ApeValArray_t* arr, ApeSize_t ix)
{
    ApeSize_t offset;
    if(ix >= arr->count)
    {
        APE_ASSERT(false);
        return NULL;
    }
    offset = ix * arr->elemsize;
    return arr->arraydata + offset;
}

static APE_INLINE bool ape_valarray_pop(ApeValArray_t* arr, void* out_value)
{
    void* res;
    if(arr->count <= 0)
    {
        return false;
    }
    if(out_value)
    {
        res = (void*)ape_valarray_get(arr, arr->count - 1);
        memcpy(out_value, res, arr->elemsize);
    }
    ape_valarray_removeat(arr, arr->count - 1);
    return true;
}

static APE_INLINE void* ape_valarray_top(ApeValArray_t* arr)
{
    if(arr->count <= 0)
    {
        return NULL;
    }
    return (void*)ape_valarray_get(arr, arr->count - 1);
}

static APE_INLINE void* ape_ptrarray_pop(ApePtrArray_t* arr)
{
    ApeSize_t ix;
    void* res;
    ix = ape_ptrarray_count(arr) - 1;
    res = ape_ptrarray_get(arr, ix);
    ape_ptrarray_removeat(arr, ix);
    return res;
}

static APE_INLINE void* ape_ptrarray_top(ApePtrArray_t* arr)
{
    ApeSize_t count;
    count = ape_ptrarray_count(arr);
    if(count == 0)
    {
        return NULL;
    }
    return ape_ptrarray_get(arr, count - 1);
}
