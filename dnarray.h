
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>


// ifexpr ? thenexpr : elseexpr
#define _if(...) (__VA_ARGS__) ?
#define _then(...) (__VA_ARGS__) :
#define _else(...) (__VA_ARGS__)

#ifndef JK_DYNARRAY_MAX
    #define JK_DYNARRAY_MAX(a, b) \
        ( \
            _if((a) > (b)) \
            _then(a) \
            _else(b) \
        )
#endif

#ifndef JK_DYNARRAY_MIN
    #define JK_DYNARRAY_MIN(a, b) \
        ( \
            _if((a) < (b)) \
            _then(a) \
            _else(b) \
        )
#endif


#define da_count_internal(arr) \
    ((((intptr_t*)(arr)) - 2)[0])

#define da_allocated_internal(arr) \
    ((((intptr_t*)(arr)) - 1)[0])

#define da_need_to_grow_internal(arr, n) \
    (!(arr) || (da_count_internal(arr) + (n)) >= da_allocated_internal(arr))

#define da_maybe_grow_internal(arr, n) \
    ( \
        _if(da_need_to_grow_internal((arr), (n))) \
        _then( \
            *((void**)&(arr)) = da_grow_internal((void*)(arr), (n), sizeof(*(arr))) \
        ) \
        _else(0) \
    )

#define da_init(arr, n) \
    ( \
        (arr) = da_grow_internal((arr), (n), sizeof(*(arr))) \
    )

#define da_count(arr) \
    ( \
        _if(arr) \
        _then( \
            da_count_internal(arr) \
        ) \
        _else(0) \
    )

#define da_allocated(arr) \
    ( \
        _if(arr) \
        _then(da_allocated_internal(arr)) \
        _else(0) \
    )

#define da_last(arr) \
    ( \
        (arr)[da_count_internal(arr) - 1] \
    )

#define da_push(arr, ...) \
    ( \
        da_maybe_grow_internal((arr), 1), \
        (arr)[da_count_internal(arr)++] = (__VA_ARGS__) \
    )

#define da_pushn(arr, n) \
    ( \
        da_maybe_grow_internal((arr), n), \
        da_count_internal(arr) += n \
    )

#define da_pop(arr) \
    ( \
        _if(da_count(arr)) \
        _then(memset((arr) + (--da_count_internal(arr)), 0, sizeof(*(arr))), 0) \
        _else(0) \
    )

#define da_popn(arr, n) \
    (\
        _if(da_count(arr)) \
        _then( \
            memset( \
                (arr) + (da_count_internal(arr) - JK_DYNARRAY_MIN((n), da_count_internal(arr))), \
                0, \
                sizeof(*(arr)) * (JK_DYNARRAY_MIN((n), da_count_internal(arr)))\
            ), \
            da_count_internal(arr) -= JK_DYNARRAY_MIN((n), \
            da_count_internal(arr)), 0 \
        ) \
        _else(0) \
    )

#define da_free(arr) \
    ( \
        _if(arr) \
        _then( \
            free(((intptr_t*)(arr)) - 2), (arr) = NULL, 0 \
        ) \
        _else(0) \
    )

#define da_clear(arr) \
    (\
        _if(da_count(arr) > 0) \
        _then( \
            memset((arr), 0, sizeof(*(arr)) * da_count_internal(arr)), \
            da_count_internal(arr) = 0, \
            0 \
        ) \
        _else(0) \
    )

#define da_grow(arr, n) \
    ( \
        ((arr) = da_grow_internal((arr), (n), sizeof(*(arr)))), \
        da_count_internal(arr) += (n) \
    )

#define da_remove_swap_last(arr, index) \
    ( \
        _if(((index) >= 0) && (index) < da_count_internal(arr)) \
        _then( \
            memcpy((arr) + (index), &da_last(arr), sizeof(*(arr))), \
            --da_count_internal(arr) \
        ) \
        _else(0)\
    )

#define da_sizeof(arr) (sizeof(*(arr)) * da_count(arr))

/*
#undef _if
#undef _then
#undef _else
*/

static APE_INLINE void* da_grow_internal(void* arr, intptr_t count, intptr_t size)
{
    intptr_t asize;
    intptr_t acount;
    intptr_t zerosize;
    void* res;
    intptr_t* ptr;
    res = 0;
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
                //free(((intptr_t*)arr) - 2);
                da_free(arr);
            }
        #endif
        if(ptr)
        {
            zerosize = asize - 2 * sizeof(intptr_t) - ptr[0] * size;
            memset(((char*)ptr) + (asize - zerosize), 0, zerosize);
            res = ptr + 2;
            da_allocated_internal(res) = acount;
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
            res = ptr + 2;
            memset(ptr, 0, asize);
            da_count_internal(res) = 0;
            da_allocated_internal(res) = acount;
        }
        else
        {
            assert(0);
        }
    }
    assert(res);
    return res;
}

#ifndef JK_DYNAMIC_ARRAY_NO_PRINTF
//TODO This needs to checked for portability. I'm not sure what the status of va_copy is on msvc for example.
static APE_INLINE int da_printf_internal(char** buf, const char* format, ...)
{
    int len;
    int res;
    va_list va;
    va_list copied;
    va_start(va, format);
    va_copy(copied, va);
    len = vsnprintf(NULL, 0, format, va);
    va_end(va);
    if(len > 0)
    {
        da_maybe_grow_internal(*buf, len + 1);
        res = vsnprintf(*buf + da_count(*buf), len + 1, format, copied);
        assert(res == len);
        da_count_internal(*buf) += res;
        return res;
    }
    else if(len < 0)
    {
        assert(0);
    }
    va_end(copied);
    return len;
}

    #define da_printf(buf, ...) da_printf_internal(&(buf), __VA_ARGS__)
#endif

