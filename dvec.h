
#pragma once

/** 
 * Copyright (c) 2014 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#include <stdlib.h>
#include <string.h>

#define VEC_VERSION "0.2.1"

#define _dv_if(...) (__VA_ARGS__) ?
#define _dv_then(...) (__VA_ARGS__) :
#define _dv_else(...) (__VA_ARGS__)

#define vec_unpack_(v) \
    (char**)&(v)->data, &(v)->length, &(v)->capacity, sizeof(*(v)->data)


#define vec_t(T) \
    struct \
    { \
        T* data; \
        int length; \
        int capacity; \
    }

#define vec_init(v) \
    memset((v), 0, sizeof(*(v)))

#define vec_deinit(v) \
    ( \
        free((v)->data), \
        vec_init(v) \
    )

#define vec_count(v) \
    ((v)->length)

#define vec_get(v, i) \
    (v)->data[i]

#define vec_push(v, val) \
    ( \
        _dv_if(vec_expand_(vec_unpack_(v))) \
        _dv_then( -1 ) \
        _dv_else( ((v)->data[(v)->length++] = (val), 0)), \
        0 \
    )

#define vec_pop(v) \
    (v)->data[--(v)->length]

#define vec_splice(v, start, count) \
    ( \
        vec_splice_(vec_unpack_(v), start, count), \
        (v)->length -= (count) \
    )

#define vec_swapsplice(v, start, count) \
    ( \
        vec_swapsplice_(vec_unpack_(v), start, count), \
        (v)->length -= (count) \
    )

#define vec_insert(v, idx, val) \
    ( \
        _dv_if(vec_insert_(vec_unpack_(v), idx)) \
        _dv_then(-1) \
        _dv_else((v)->data[idx] = (val), 0), \
        (v)->length++, 0 \
    )

#define vec_sort(v, fn) \
    qsort((v)->data, (v)->length, sizeof(*(v)->data), fn)

#define vec_swap(v, idx1, idx2) \
    vec_swap_(vec_unpack_(v), idx1, idx2)

#define vec_truncate(v, len) \
    (\
        (v)->length = ( \
            _dv_if((len) < (v)->length) \
            _dv_then(len) \
            _dv_else((v)->length) \
        ) \
    )

#define vec_clear(v) \
    ((v)->length = 0)

#define vec_first(v) \
    (v)->data[0]

#define vec_last(v) \
    (v)->data[(v)->length - 1]

#define vec_reserve(v, n) \
    vec_reserve_(vec_unpack_(v), n)

#define vec_compact(v) \
    vec_compact_(vec_unpack_(v))

#define vec_pusharr(v, arr, count) \
    do \
    { \
        int i__, n__ = (count); \
        if(vec_reserve_po2_(vec_unpack_(v), (v)->length + n__) != 0) \
        { \
            break; \
        } \
        for(i__ = 0; i__ < n__; i__++) \
        { \
            (v)->data[(v)->length++] = (arr)[i__]; \
        } \
    } while(0)


#define vec_extend(v, v2) \
    vec_pusharr((v), (v2)->data, (v2)->length)

#define vec_find(v, val, idx) \
    do \
    { \
        for((idx) = 0; (idx) < (v)->length; (idx)++) \
        { \
            if((v)->data[(idx)] == (val)) \
            { \
                break; \
            } \
        } \
        if((idx) == (v)->length) \
        { \
            (idx) = -1; \
        } \
    } while(0)

#define vec_remove(v, val) \
    do \
    { \
        int idx__; \
        vec_find(v, val, idx__); \
        if(idx__ != -1) \
        { \
            vec_splice(v, idx__, 1); \
        } \
    } while(0)

#define vec_reverse(v) \
    do \
    { \
        int i__ = (v)->length / 2; \
        while(i__--) \
        { \
            vec_swap((v), i__, (v)->length - (i__ + 1)); \
        } \
    } while(0)


#define vec_foreach(v, var, iter) \
    if((v)->length > 0) \
        for((iter) = 0; (iter) < (v)->length && (((var) = (v)->data[(iter)]), 1); ++(iter))


#define vec_foreach_rev(v, var, iter) \
    if((v)->length > 0) \
        for((iter) = (v)->length - 1; (iter) >= 0 && (((var) = (v)->data[(iter)]), 1); --(iter))


#define vec_foreach_ptr(v, var, iter) \
    if((v)->length > 0) \
        for((iter) = 0; (iter) < (v)->length && (((var) = &(v)->data[(iter)]), 1); ++(iter))


#define vec_foreach_ptr_rev(v, var, iter) \
    if((v)->length > 0) \
        for((iter) = (v)->length - 1; (iter) >= 0 && (((var) = &(v)->data[(iter)]), 1); --(iter))


typedef vec_t(void*) vec_void_t;
typedef vec_t(char*) vec_str_t;
typedef vec_t(int) vec_int_t;
typedef vec_t(char) vec_char_t;
typedef vec_t(float) vec_float_t;
typedef vec_t(double) vec_double_t;


static int vec_expand_(char** data, int* length, int* capacity, int memsz);
static int vec_reserve_(char** data, int* length, int* capacity, int memsz, int n);
static int vec_reserve_po2_(char** data, int* length, int* capacity, int memsz, int n);
static int vec_compact_(char** data, int* length, int* capacity, int memsz);
static int vec_insert_(char** data, int* length, int* capacity, int memsz, int idx);
static void vec_splice_(char** data, int* length, int* capacity, int memsz, int start, int count);
static void vec_swapsplice_(char** data, int* length, int* capacity, int memsz, int start, int count);
static void vec_swap_(char** data, int* length, int* capacity, int memsz, int idx1, int idx2);

static inline int vec_expand_(char** data, int* length, int* capacity, int memsz)
{
    if(*length + 1 > *capacity)
    {
        void* ptr;
        int n = (*capacity == 0) ? 1 : *capacity << 1;
        ptr = realloc(*data, n * memsz);
        if(ptr == NULL)
            return -1;
        *data = ptr;
        *capacity = n;
    }
    return 0;
}

static inline int vec_reserve_(char** data, int* length, int* capacity, int memsz, int n)
{
    (void)length;
    if(n > *capacity)
    {
        void* ptr = realloc(*data, n * memsz);
        if(ptr == NULL)
            return -1;
        *data = ptr;
        *capacity = n;
    }
    return 0;
}

static inline int vec_reserve_po2_(char** data, int* length, int* capacity, int memsz, int n)
{
    int n2 = 1;
    if(n == 0)
        return 0;
    while(n2 < n)
        n2 <<= 1;
    return vec_reserve_(data, length, capacity, memsz, n2);
}

static inline int vec_compact_(char** data, int* length, int* capacity, int memsz)
{
    if(*length == 0)
    {
        free(*data);
        *data = NULL;
        *capacity = 0;
        return 0;
    }
    else
    {
        void* ptr;
        int n = *length;
        ptr = realloc(*data, n * memsz);
        if(ptr == NULL)
            return -1;
        *capacity = n;
        *data = ptr;
    }
    return 0;
}

static inline int vec_insert_(char** data, int* length, int* capacity, int memsz, int idx)
{
    int err = vec_expand_(data, length, capacity, memsz);
    if(err)
        return err;
    memmove(*data + (idx + 1) * memsz, *data + idx * memsz, (*length - idx) * memsz);
    return 0;
}

static inline void vec_splice_(char** data, int* length, int* capacity, int memsz, int start, int count)
{
    (void)capacity;
    memmove(*data + start * memsz, *data + (start + count) * memsz, (*length - start - count) * memsz);
}

static inline void vec_swapsplice_(char** data, int* length, int* capacity, int memsz, int start, int count)
{
    (void)capacity;
    memmove(*data + start * memsz, *data + (*length - count) * memsz, count * memsz);
}

static inline void vec_swap_(char** data, int* length, int* capacity, int memsz, int idx1, int idx2)
{
    unsigned char *a, *b, tmp;
    int count;
    (void)length;
    (void)capacity;
    if(idx1 == idx2)
        return;
    a = (unsigned char*)*data + idx1 * memsz;
    b = (unsigned char*)*data + idx2 * memsz;
    count = memsz;
    while(count--)
    {
        tmp = *a;
        *a = *b;
        *b = tmp;
        a++, b++;
    }
}
