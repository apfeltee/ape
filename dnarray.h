
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>


// ifexpr ? thenexpr : elseexpr
#define _da_if(...) (__VA_ARGS__) ?
#define _da_then(...) (__VA_ARGS__) :
#define _da_else(...) (__VA_ARGS__)

#ifndef JK_DYNARRAY_MAX
    #define JK_DYNARRAY_MAX(a, b) \
        ( \
            _da_if((a) > (b)) \
            _da_then(a) \
            _da_else(b) \
        )
#endif

#ifndef JK_DYNARRAY_MIN
    #define JK_DYNARRAY_MIN(a, b) \
        ( \
            _da_if((a) < (b)) \
            _da_then(a) \
            _da_else(b) \
        )
#endif

static APE_INLINE unsigned char** da_grow_internal(unsigned char** arr, intptr_t count, intptr_t size);

#define da_count_internal(arr) \
    ((((intptr_t*)(arr)) - 2)[0])

#define da_capacity_internal(arr) \
    ((((intptr_t*)(arr)) - 1)[0])


#define da_need_to_grow_internal(arr, n) \
    (!(arr) || (da_count_internal(arr) + (n)) >= da_capacity_internal(arr))

#define da_maybe_grow_internal(arr, n) \
    ( \
        _da_if(da_need_to_grow_internal((unsigned char**)(arr), (n))) \
        _da_then( \
            (arr) = (__typeof__(arr)) da_grow_internal((unsigned char**)(arr), (n), sizeof(*(arr))) \
        ) \
        _da_else(0) \
    )

#define da_init(arr, n) \
    (__typeof__(arr))  da_grow_internal((unsigned char**)arr, n, sizeof(*arr))

#define da_count(arr) \
    ( \
        _da_if((arr) == NULL) \
        _da_then(0) \
        _da_else( \
            da_count_internal(arr) \
        ) \
    )

#define da_capacity(arr) \
    ( \
        _da_if(arr) \
        _da_then(da_capacity_internal(arr)) \
        _da_else(0) \
    )

#define da_last(arr) \
    ( \
        (arr)[da_count_internal(arr) - 1] \
    )

#if 0
    #define da_push(arr, ...) \
        ( \
            da_maybe_grow_internal((arr), APE_CONF_PLAINLIST_CAPACITY_ADD), \
            (arr)[da_count_internal(arr)++] = (__VA_ARGS__) \
        )
#else
    #define da_push(arr, ...) \
        ( \
            da_maybe_grow_internal((arr), 1), \
            (arr)[da_count_internal(arr)++] = (__VA_ARGS__) \
        )

#endif

#define da_pushn(arr, n) \
    ( \
        da_maybe_grow_internal((arr), n), \
        da_count_internal(arr) += n \
    )


#define da_pop(arr) \
    ( \
        _da_if(da_count(arr) > 0) \
        _da_then(memset((arr) + (--da_count_internal(arr)), 0, sizeof(*(arr))), 0) \
        _da_else(0) \
    )

#define da_popn(arr, n) \
    (\
        _da_if(da_count(arr) > 0) \
        _da_then( \
            memset( \
                (arr) + (da_count_internal(arr) - JK_DYNARRAY_MIN((n), da_count_internal(arr))), \
                0, \
                sizeof(*(arr)) * (JK_DYNARRAY_MIN((n), da_count_internal(arr)))\
            ), \
            da_count_internal(arr) -= JK_DYNARRAY_MIN((n), \
            da_count_internal(arr)), 0 \
        ) \
        _da_else(0) \
    )

#define da_free(arr) \
    ( \
        _da_if(arr) \
        _da_then( \
            free(((intptr_t*)(arr)) - 2), (arr) = NULL, 0 \
        ) \
        _da_else(0) \
    )

#define da_clear(arr) \
    (\
        _da_if(da_count(arr) > 0) \
        _da_then( \
            memset((arr), 0, sizeof(*(arr)) * da_count_internal(arr)), \
            da_count_internal(arr) = 0, \
            0 \
        ) \
        _da_else(0) \
    )

#define da_grow(arr, n) \
    ( \
        ((arr) = da_grow_internal((arr), (n), sizeof(*(arr)))), \
        da_count_internal(arr) += (n) \
    )

#define da_remove_swap_last(arr, index) \
    ( \
        _da_if(((index) >= 0) && (index) < da_count_internal(arr)) \
        _da_then( \
            memcpy((arr) + (index), &da_last(arr), sizeof(*(arr))), \
            --da_count_internal(arr) \
        ) \
        _da_else(0)\
    )

#define da_sizeof(arr) (sizeof(*(arr)) * da_count(arr))

static APE_INLINE void da_copy(unsigned char** from, unsigned char** to, unsigned int begin, unsigned int end)
{
    unsigned int i;
    for(i=begin; i<end; i++)
    {
        da_maybe_grow_internal(to, APE_CONF_PLAINLIST_CAPACITY_ADD);
        to[i] = from[i];
    }
}

static APE_INLINE void da_removeat(unsigned char** from, unsigned int ix)
{
    (void)from;
    (void)ix;
}

/*
#undef _da_if
#undef _da_then
#undef _da_else
*/

static APE_INLINE unsigned char** da_grow_internal(unsigned char** arr, intptr_t count, intptr_t size)
{
    intptr_t asize;
    intptr_t acount;
    intptr_t zerosize;
    unsigned char** res;
    intptr_t* ptr;
    res = NULL;
    acount = JK_DYNARRAY_MAX(2 * da_count(arr), da_count(arr) + count);
    asize = 2 * sizeof(intptr_t) + acount * size;
    if(arr)
    {
        #if 0
            ptr = (intptr_t*)realloc(((intptr_t*)arr)-2, asize);
        #else
            ptr = (intptr_t*)malloc(asize);
            if(ptr)
            {
                memcpy(ptr, ((intptr_t*)arr) - 2, da_count(arr) * size + 2 * sizeof(intptr_t));
                da_free(arr);
            }
        #endif
        if(ptr)
        {
            zerosize = asize - 2 * sizeof(intptr_t) - ptr[0] * size;
            memset(((char*)ptr) + (asize - zerosize), 0, zerosize);
            res = (unsigned char**)(ptr + 2);
            da_capacity_internal(res) = acount;
        }
        else
        {
            assert(0);
        }
    }
    else
    {
        #if 0
            ptr = (intptr_t*)realloc(0, asize);
        #else
            ptr = (intptr_t*)malloc(asize);
        #endif
        if(ptr)
        {
            res = (unsigned char**)(ptr + 2);
            memset(ptr, 0, asize);
            da_count_internal(res) = 0;
            da_capacity_internal(res) = acount;
        }
        else
        {
            assert(0);
        }
    }
    assert(res);
    return res;
}

#if 1
    #ifndef JK_DYNAMIC_ARRAY_NO_PRINTF
        #define da_printf(buf, ...) da_printf_internal(&(buf), __VA_ARGS__)

        //TODO This needs to checked for portability. I'm not sure what the status of va_copy is on msvc for example.
        static APE_INLINE int da_printf_internal(char** buf, const char* format, ...)
        {
            int len;
            int res;
            unsigned char** p;
            
            va_list va;
            va_list copied;
            va_start(va, format);
            va_copy(copied, va);
            /*
            * do you ever just stop and wonder, "what the hell am i even doing"?
            * case in point, taking pointer to reference of dereference of $buf, casting to a pointer to a pointer of unsigned char.
            * which sounds silly, but it gets us a (type-corrected) pointer to the thing
            * that $buf actually points to. and it's only needed because C++ won't allow it. normally.
            */
            p = (unsigned char**)&*buf;
            len = vsnprintf(NULL, 0, format, va);
            va_end(va);
            if(len > 0)
            {
                da_maybe_grow_internal(p, len + 1);
                res = vsnprintf((char*)(p + da_count(p)), len + 1, format, copied);
                assert(res == len);
                da_count_internal(p) += res;
                return res;
            }
            else if(len < 0)
            {
                assert(0);
            }
            va_end(copied);
            return len;
        }

    #endif
#endif
