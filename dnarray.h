
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

static APE_INLINE intptr_t* da_grow_internal(intptr_t* arr, intptr_t count, intptr_t tsize);

#define da_count_internal(arr) \
    ((((intptr_t*)(arr)) - 2)[0])

#define da_capacity_internal(arr) \
    ((((intptr_t*)(arr)) - 1)[0])


#define da_need_to_grow_internal(arr, n) \
    (!(arr) || (da_count_internal(arr) + (n)) >= da_capacity_internal(arr))

#define da_maybe_grow_internal(arr, n) \
    ( \
        _da_if(da_need_to_grow_internal((intptr_t*)(arr), (n))) \
        _da_then( \
            (arr) = (__typeof__(arr)) da_grow_internal((intptr_t*)(arr), (n), sizeof(*(arr))) \
        ) \
        _da_else(0) \
    )

#define da_make(arr, n, sztyp) \
    (__typeof__(arr))  da_grow_internal((intptr_t*)arr, n, sztyp)

#define da_init(arr, n, sztyp) \
    da_init(arr, n, sztyp)

#define da_destroy(arr) \
    ( \
        _da_if(arr) \
        _da_then( \
            free(((intptr_t*)(arr)) - 2), (arr) = NULL, 0 \
        ) \
        _da_else(0) \
    )

#define da_free(arr) \
    da_destroy(arr)

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

#define da_count(arr) \
    ((unsigned int)( \
        _da_if((arr) == NULL) \
        _da_then(0) \
        _da_else( \
            da_count_internal(arr) \
        ) \
    ))

#define da_capacity(arr) \
    ((unsigned int)( \
        _da_if(arr) \
        _da_then(da_capacity_internal(arr)) \
        _da_else(0) \
    ))

#define da_get(arr, idx) \
    (arr)[(idx)]

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
        _da_then( \
            memset((arr) + (--da_count_internal(arr)), 0, sizeof(*(arr))), \
            0 \
        ) \
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

static APE_INLINE void da_copy(intptr_t* from, intptr_t* to, unsigned int begin, unsigned int end)
{
    unsigned int i;
    for(i=begin; i<end; i++)
    {
        da_maybe_grow_internal(to, APE_CONF_PLAINLIST_CAPACITY_ADD);
        to[i] = from[i];
    }
}

static APE_INLINE void da_removeat(intptr_t* from, unsigned int ix)
{
    (void)from;
    (void)ix;
}

/*
#undef _da_if
#undef _da_then
#undef _da_else
*/

static APE_INLINE intptr_t* da_grow_internal(intptr_t* arr, intptr_t count, intptr_t tsize)
{
    intptr_t asize;
    intptr_t acount;
    intptr_t zerosize;
    intptr_t* res;
    intptr_t* ptr;
    res = NULL;
    acount = JK_DYNARRAY_MAX(2 * da_count(arr), da_count(arr) + count);
    asize = 2 * sizeof(intptr_t) + acount * tsize;
    if(arr)
    {
        #if 0
            ptr = (intptr_t*)realloc(((intptr_t*)arr)-2, asize*tsize);
        #else
            //ptr = (intptr_t*)malloc(asize);
            ptr = (intptr_t*)calloc(asize, tsize);
            if(ptr)
            {
                memcpy(ptr, ((intptr_t*)arr) - 2, da_count(arr) * tsize + 2 * sizeof(intptr_t));
                da_free(arr);
            }
        #endif
        if(ptr)
        {
            zerosize = asize - 2 * sizeof(intptr_t) - ptr[0] * tsize;
            memset(((intptr_t*)ptr) + (asize - zerosize), 0, zerosize);
            res = (intptr_t*)(ptr + 2);
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
            ptr = (intptr_t*)realloc(0, asize*tsize);
        #else
            //ptr = (intptr_t*)malloc(asize);
            ptr = (intptr_t*)calloc(asize, tsize);
        #endif
        if(ptr)
        {
            res = (intptr_t*)(ptr + 2);
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

