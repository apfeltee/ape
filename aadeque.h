
#pragma once
/*
 * aadeque.h - Another array deque
 *
 * The author disclaims copyright to this source code.
 */
#include <stdlib.h>
#include <string.h>

/* allocation macros, tweakable */
#ifndef DEQUELIST_ALLOC
    #define DEQUELIST_ALLOC(size) malloc(size)
#endif
#ifndef DEQUELIST_REALLOC
    #define DEQUELIST_REALLOC(ptr, size, oldsize) realloc(ptr, size)
#endif
#ifndef DEQUELIST_FREE
    #define DEQUELIST_FREE(ptr, size) free(ptr)
#endif
#ifndef DEQUELIST_OOM
    #define DEQUELIST_OOM() exit(-1)
#endif

/* minimum capacity */
#ifndef DEQUELIST_MIN_CAPACITY
    #define DEQUELIST_MIN_CAPACITY 4
#endif

/* value type, tweakable */
typedef void* DequeValue_t;

/* the type of the lengths and indices */
typedef unsigned int DequeSize_t;

typedef struct DequeList_t DequeList_t;

/* The deque type, optionally prefixed user-defined extra members */
struct DequeList_t
{
#ifdef DEQUELIST_HEADER
    DEQUELIST_HEADER
#endif
    DequeSize_t  capacity; /* capacity, actual length of the elemdata array */
    DequeSize_t  offsetfirst; /* offset to the first element in elemdata */
    DequeSize_t  length; /* length */
    DequeValue_t elemdata[1]; /* elements, allocated in-place */
};
/* aadeque.h */
static  size_t deqlist_sizeof(unsigned int cap);
static  unsigned int deqlist_idx(DequeList_t *a, unsigned int i);
static  DequeList_t *deqlist_create(unsigned int len);
static  DequeList_t *deqlist_create_empty(void);
static  void deqlist_destroy(DequeList_t *a);
static  unsigned int deqlist_len(DequeList_t *a);
static  void *deqlist_get(DequeList_t *a, unsigned int i);
static  void deqlist_set(DequeList_t *a, unsigned int i, void *value);
static  void deqlist_clear(DequeList_t *a, unsigned int i, unsigned int n);
static  DequeList_t *deqlist_clone(DequeList_t *a);
static  DequeList_t *deqlist_from_array(void **array, unsigned int n);
static  int deqlist_eq_array(DequeList_t *a, void **array, unsigned int n);
static  DequeList_t *deqlist_reserve(DequeList_t *a, unsigned int n);
static  DequeList_t *deqlist_compact_to(DequeList_t *a, unsigned int mincap);
static  DequeList_t *deqlist_compact_some(DequeList_t *a);
static  DequeList_t *deqlist_make_space_after(DequeList_t *a, unsigned int n);
static  DequeList_t *deqlist_make_space_before(DequeList_t *a, unsigned int n);
static  DequeList_t *deqlist_crop(DequeList_t *a, unsigned int offset, unsigned int length);
static  DequeList_t *deqlist_delete_last_n(DequeList_t *a, unsigned int n);
static  DequeList_t *deqlist_delete_first_n(DequeList_t *a, unsigned int n);
static  void deqlist_unshift(DequeList_t **aptr, void *value);
static  void *deqlist_shift(DequeList_t **aptr);
static  void deqlist_push(DequeList_t **aptr, void *value);
static  void *deqlist_pop(DequeList_t **aptr);
static  DequeList_t *deqlist_append(DequeList_t *a1, DequeList_t *a2);
static  DequeList_t *deqlist_prepend(DequeList_t *a1, DequeList_t *a2);
static  DequeList_t *deqlist_slice(DequeList_t *a, unsigned int offset, unsigned int length);
static  DequeList_t *deqlist_compact(DequeList_t *a);
static  void deqlist_make_contiguous_unordered(DequeList_t *a);


/* Size to allocate for a struct aadeque of capacity cap. Used internally. */
static inline size_t deqlist_sizeof(DequeSize_t cap)
{
    return sizeof(DequeList_t) + (cap - 1) * sizeof(DequeValue_t);
}

/*
 * Convert external index to internal one. Used internally.
 *
 * i must fulfil i >= 0 and i < length, otherwise the result is undefined.
 * i % cap == i & (cap - 1), since cap always is a power of 2.
 */
static inline DequeSize_t deqlist_idx(DequeList_t* a, DequeSize_t i)
{
    DequeSize_t idx = a->offsetfirst + i;
    if(idx >= a->capacity)
    {
        idx -= a->capacity;
    }
    return idx;
}

/*
 * Creates an array of length len with undefined values.
 */
static inline DequeList_t* deqlist_create(DequeSize_t len)
{
    DequeSize_t cap = len;//DEQUELIST_MIN_CAPACITY;
    DequeList_t*     a;
    //while (cap < len)
    //	cap = cap << 1;
    if(cap < DEQUELIST_MIN_CAPACITY)
    {
        cap = DEQUELIST_MIN_CAPACITY;
    }
    a = (DequeList_t*)DEQUELIST_ALLOC(deqlist_sizeof(cap));
    if(!a)
    {
        DEQUELIST_OOM();
    }
    a->length = len;
    a->offsetfirst = 0;
    a->capacity = cap;
    return a;
}

/*
 * Creates an empty array.
 */
static inline DequeList_t* deqlist_create_empty(void)
{
    return deqlist_create(0);
}

/*
* Frees the memory.
*/
static inline void deqlist_destroy(DequeList_t* a)
{
    DEQUELIST_FREE(a, deqlist_sizeof(a->capacity));
}

/*
* Returns the number of elements in the array.
*/
static inline DequeSize_t deqlist_size(DequeList_t* a)
{
    return a->length;
}

static inline DequeSize_t deqlist_count(DequeList_t* a)
{
    return a->length;
}

static inline DequeSize_t deqlist_len(DequeList_t* a)
{
    return a->length;
}

/*
* Fetch the element at the zero based index i. The index bounds are not
* checked.
*/
static inline DequeValue_t deqlist_get(DequeList_t* a, DequeSize_t i)
{
    DequeSize_t pos;
    pos = deqlist_idx(a, i);
    return a->elemdata[pos];
}

/*
* Set (replace) the element at the zero based index i. The index bounds are not
* checked.
*/
static inline void deqlist_set(DequeList_t* a, DequeSize_t i, DequeValue_t value)
{
    DequeSize_t pos;
    pos = deqlist_idx(a, i);
    a->elemdata[pos] = value;
}

/*
* Clear the memory (set to zery bytes) of n elements at indices between
* i and i+n-1.
*
* Used internally if DEQUELIST_CLEAR_UNUSED_MEM is defined.
*/
static inline void deqlist_clear(DequeList_t* a, DequeSize_t i, DequeSize_t n)
{
    if(deqlist_idx(a, i) > deqlist_idx(a, i + n - 1))
    {
        /*
		 * It warps. There are two parts to clear.
		 *
		 *     0  i+n-1   i   cap
		 *    /   /      /   /
		 *   |--->      o---|
		 */
        memset(&a->elemdata[0], 0, sizeof(DequeValue_t) * deqlist_idx(a, n + i));
        memset(&a->elemdata[deqlist_idx(a, i)], 0, sizeof(DequeValue_t) * (a->capacity - deqlist_idx(a, i)));
    }
    else
    {
        /*
		 * It's in consecutive memory.
		 *
		 *     0   i   i+n-1  cap
		 *    /   /     /    /
		 *   |   o----->    |
		 */
        memset(&a->elemdata[deqlist_idx(a, i)], 0, sizeof(DequeValue_t) * n);
    }
}

/*
 * Clones an array deque, preserving the internal memory layout.
 */
static inline DequeList_t* deqlist_clone(DequeList_t* a)
{
    size_t sz;
    void* clone;
    sz = deqlist_sizeof(a->capacity);
    clone = DEQUELIST_ALLOC(sz);
    if(!clone)
    {
        DEQUELIST_OOM();
    }
    return (DequeList_t*)memcpy(clone, a, sz);
}

/*
 * Create an struct aadeque from the n values pointed to by array.
 */
static inline DequeList_t* deqlist_from_array(DequeValue_t* array, DequeSize_t n)
{
    DequeList_t* a;
    a = deqlist_create(n);
    memcpy(a->elemdata, array, sizeof(DequeValue_t) * n);
    return a;
}

/*
 * Compare the contents of a against a static C array of n elements. Returns 1
 * if the number of elements is equal to n and all elements are equal to their
 * counterparts in array, 0 otherwise.
 */
static inline int deqlist_eq_array(DequeList_t* a, DequeValue_t* array, DequeSize_t n)
{
    DequeSize_t i;
    if(a->length != n)
    {
        return 0;
    }
    for(i = 0; i < n; i++)
    {
        if(deqlist_get(a, i) != array[i])
        {
            return 0;
        }
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * Helpers for growing and compacting the underlying buffer. Like realloc,
 * these functions try to resize the underlying buffer and return a. It there
 * is not enough space, they copy the data to a new memory location, free the
 * old memory and return a pointer to the new the new memory allocation.
 *----------------------------------------------------------------------------*/

/*
 * Reserve space for at least n more elements
 */
static inline DequeList_t* deqlist_reserve(DequeList_t* a, DequeSize_t n)
{
    if(a->capacity < a->length + n)
    {
        /* calulate and set new capacity */
        DequeSize_t oldcap = a->capacity;
        do
        {
            a->capacity = a->capacity << 1;
        } while(a->capacity < a->length + n);
        /* allocate more mem */
        a = (DequeList_t*)DEQUELIST_REALLOC(a, deqlist_sizeof(a->capacity), deqlist_sizeof(oldcap));
        if(!a)
        {
            DEQUELIST_OOM();
        }
        /* adjust content to the increased capacity */
        if(a->offsetfirst + a->length > oldcap)
        {
            /*
            * It warped around before. Make it warp around the new boundary.
            *
            * Symbols: o-- = first part of content
            *          --> = last part of content
            *
            *            0      oldcap     cap
            *           /        /         /
            * Before:  |-->  o--|
            * After:   |-->     |      o--|
            */
            memcpy(&(a->elemdata[a->offsetfirst + a->capacity - oldcap]), &(a->elemdata[a->offsetfirst]), sizeof(DequeValue_t) * (a->capacity - oldcap));
#ifdef DEQUELIST_CLEAR_UNUSED_MEM
            memset(&(a->elemdata[a->offsetfirst]), 0, sizeof(DequeValue_t) * (a->capacity - oldcap));
#endif
            a->offsetfirst += a->capacity - oldcap;
        }
    }
    return a;
}

/*
 * Reduces the capacity to some number larger than or equal to mincap. Mincap
 * must be at least as large as the number of elements in the deque.
 */
static inline DequeList_t* deqlist_compact_to(DequeList_t* a, DequeSize_t mincap)
{
    DequeSize_t oldcap;
    DequeSize_t doublemincap;
    doublemincap = mincap << 1;
    if(a->capacity >= doublemincap && a->capacity > DEQUELIST_MIN_CAPACITY)
    {
        /*
        * Halve the capacity as long as it is >= twice the minimum capacity.
        */
        oldcap = a->capacity;
        /* Calulate and set new capacity */
        do
        {
            a->capacity = a->capacity >> 1;
        } while(a->capacity >= doublemincap && a->capacity > DEQUELIST_MIN_CAPACITY);
        /* Adjust content to decreased capacity */
        if(a->offsetfirst + a->length > oldcap)
        {
            /*
            * It warpped around already. Move the first part to within the new
            * boundaries.
            *
            * Symbols: o-- = first part of content
            *          --> = last part of content
            *
            *            0       cap      oldcap
            *           /        /         /
            * Before:  |-->     |      o--|
            * After:   |-->  o--|
            */
            memcpy(&(a->elemdata[a->offsetfirst + a->capacity - oldcap]), &(a->elemdata[a->offsetfirst]), sizeof(DequeValue_t) * (oldcap - a->offsetfirst));
            a->offsetfirst += a->capacity - oldcap;
        }
        else if(a->offsetfirst >= a->capacity)
        {
            /*
            * The whole content will be outside, but it's in one piece. Move
            * it to offset 0.
            *
            *            0       cap      oldcap
            *           /        /         /
            * Before:  |        |  o----> |
            * After:   |o---->  |
            */
            memcpy(&(a->elemdata[0]), &(a->elemdata[a->offsetfirst]), sizeof(DequeValue_t) * a->length);
            a->offsetfirst = 0;
        }
        else if(a->offsetfirst + a->length > a->capacity)
        {
            /*
            * It will overflow the new cap. Make it warp.
            *
            *            0       cap      oldcap
            *           /        /         /
            * Before:  |     o--|-->      |
            * After:   |-->  o--|
            */
            memcpy(&(a->elemdata[0]), &(a->elemdata[a->capacity]), sizeof(DequeValue_t) * (a->offsetfirst + a->length - a->capacity));
        }
        else
        {
            /*
            * The whole content is in the first half. Nothing to move.
            *
            *            0       cap      oldcap
            *           /        /         /
            * Before:  | o----> |         |
            * After:   | o----> |
            */
        }
        /* Free the unused, second half, between cap and oldcap. */
        a = (DequeList_t*)DEQUELIST_REALLOC(a, deqlist_sizeof(a->capacity), deqlist_sizeof(oldcap));
        if(!a)
        {
            DEQUELIST_OOM();
        }
    }
    return a;
}

/*
* Reduces the capacity to half if only 25% of the capacity or less is used.
* Call this after removing elements. Automatically done by pop and shift.
*
* This strategy prevents the scenario that alternate insertions and deletions
* trigger buffer resizing on every operation, thus keeping the insertions and
* deletions at O(1) amortized.
*/
static inline DequeList_t* deqlist_compact_some(DequeList_t* a)
{
    return deqlist_compact_to(a, a->length << 1);
}

/*
*----------------------------------------------------------------------------
* Functions that extend the deque in either end. The added elements will not
* be initialized, thus their indices will contain undefined values.
*
* These functions resize the underlying buffer when needed. Like realloc,
* they try to resize the underlying buffer and return a. It there is not
* enough space, they copy the data to a new memory location, free the old
* memory and return a pointer to the new the new memory allocation.
*----------------------------------------------------------------------------
*/

/*
* Inserts n undefined values after the last element.
*/
static inline DequeList_t* deqlist_make_space_after(DequeList_t* a, DequeSize_t n)
{
    a = deqlist_reserve(a, n);
    a->length += n;
    return a;
}

/*
* Inserts n undefined values before the first element.
*/
static inline DequeList_t* deqlist_make_space_before(DequeList_t* a, DequeSize_t n)
{
    a = deqlist_reserve(a, n);
    a->offsetfirst = deqlist_idx(a, a->capacity - n);
    a->length += n;
    return a;
}

/*
*----------------------------------------------------------------------------
* Functions for deleting multiple elements
*----------------------------------------------------------------------------
*/

/*
* Deletes everything exept n elements starting at index i. Like slice, but
* destructive, i.e. it modifies *a*. Returns a pointer to the new or modified
* array deque.
*
* If length + offset is greater than the length of a, the behaviour is
* undefined. No check is performed on *length* and *offset*.
*/
static inline DequeList_t* deqlist_crop(DequeList_t* a, DequeSize_t offset, DequeSize_t length)
{
#ifdef DEQUELIST_CLEAR_UNUSED_MEM
    deqlist_clear(a, offset + length, a->length - length);
#endif
    a->offsetfirst = deqlist_idx(a, offset);
    a->length = length;
    return deqlist_compact_some(a);
}

/*
* Deletes the last n elements.
*/
static inline DequeList_t* deqlist_delete_last_n(DequeList_t* a, DequeSize_t n)
{
    return deqlist_crop(a, 0, a->length - n);
}

/*
* Deletes the first n elements.
*/
static inline DequeList_t* deqlist_delete_first_n(DequeList_t* a, DequeSize_t n)
{
    return deqlist_crop(a, n, a->length - n);
}

/*
*---------------------------------------------------------------------------
* The pure deque operations: Inserting and deleting values in both ends.
* Shift, unshift, push, pop.
*---------------------------------------------------------------------------
*/

/*
 * Insert an element at the beginning.
 * May change aptr if it needs to be reallocated.
 */
static inline void deqlist_unshift(DequeList_t** aptr, DequeValue_t value)
{
    *aptr = deqlist_make_space_before(*aptr, 1);
    (*aptr)->elemdata[(*aptr)->offsetfirst] = value;
}

/*
 * Remove an element at the beginning and return its value.
 * May change aptr if it needs to be reallocated.
*/
static inline DequeValue_t deqlist_shift(DequeList_t** aptr)
{
    DequeValue_t value;
    value = (*aptr)->elemdata[(*aptr)->offsetfirst];
    *aptr = deqlist_crop(*aptr, 1, (*aptr)->length - 1);
    return value;
}

/*
 * Insert an element at the end.
 * May change aptr if it needs to be reallocated.
 */
static inline void deqlist_push(DequeList_t** aptr, DequeValue_t value)
{
    *aptr = deqlist_make_space_after(*aptr, 1);
    (*aptr)->elemdata[deqlist_idx(*aptr, (*aptr)->length - 1)] = value;
}

/*
 * Remove an element at the end and return its value.
 * May change aptr if it needs to be reallocated.
 */
static inline DequeValue_t deqlist_pop(DequeList_t** aptr)
{
    DequeValue_t value;
    value = (*aptr)->elemdata[deqlist_idx(*aptr, (*aptr)->length - 1)];
    *aptr = deqlist_crop(*aptr, 0, (*aptr)->length - 1);
    return value;
}

/*---------------------------------------------------------------------------
 * Append or prepend all elements of one array deque to another
 *---------------------------------------------------------------------------*/

/*
 * Appends all elements of s2 to a1 and returns the (possibly reallocated) a1.
 */
static inline DequeList_t* deqlist_append(DequeList_t* a1, DequeList_t* a2)
{
    DequeSize_t i;
    DequeSize_t j;
    i = a1->length;
    a1 = deqlist_make_space_after(a1, a2->length);
    /* TODO: copy using memcpy instead of the loop */
    for(j = 0; j < a2->length; j++)
    {
        deqlist_set(a1, i++, deqlist_get(a2, j));
    }
    return a1;
}

/*
 * Prepends all elements of s2 to a1 and returns the (possibly reallocated) a1.
 */
static inline DequeList_t* deqlist_prepend(DequeList_t* a1, DequeList_t* a2)
{
    DequeSize_t i;
    DequeSize_t j;
    i = 0;
    a1 = deqlist_make_space_before(a1, a2->length);
    /* TODO: copy using memcpy instead of the loop */
    for(j = 0; j < a2->length; j++)
    {
        deqlist_set(a1, i++, deqlist_get(a2, j));
    }
    return a1;
}

/*---------------------------------------------------------------------------
 * Slice: copy a part of the contents to a new array deque.
 *---------------------------------------------------------------------------*/

/*
 * Creates a new array deque, by copying *length* elements starting at *offset*.
 *
 * If length + offset is greater than the length of a, the behaviour is
 * undefined. No check is performed on *length* and *offset*.
 *
 * See also *deqlist_crop()*.
 */
static inline DequeList_t* deqlist_slice(DequeList_t* a, DequeSize_t offset, DequeSize_t length)
{
    DequeSize_t i;
    DequeValue_t x;
    DequeList_t* b;
    b = deqlist_create(length);
    /* TODO: use memcpy instead of the loop */
    for(i = 0; i < length; i++)
    {
        x = deqlist_get(a, i + offset);
        deqlist_set(b, i, x);
    }
    return b;
}

/*----------------------------------------------------------------------------
 * Various, perhaps less useful functions
 *----------------------------------------------------------------------------*/

/*
* Reduces the capacity, freeing as much unused memory as possible. This is
* only useful after deleting elements, if you're not going to add elements the
* deque again (which may trigger expensive resizing again).
*
* Returns the unmodified pointer if no memory could be free'd. Otherwise, frees
* the old one and returns a pointer to the new, resized allocation.
*/
static inline DequeList_t* deqlist_compact(DequeList_t* a)
{
    return deqlist_compact_to(a, a->length);
}

/*
* Joins the parts together in memory, possibly in the wrong order. This is
* useful if you want to sort or shuffle the underlaying array using functions
* for raw arrays (such as qsort).
*/
static inline void deqlist_make_contiguous_unordered(DequeList_t* a)
{
    if(a->offsetfirst + a->length > a->capacity)
    {
        /*
        * It warps around. Just move the parts together in the wrong order.
        *
        *                  0  end  start cap
        *                 /  /    /     /
        * Before:        |-->    o-----|
        * Intermediate:  |-->o-----    |
        * After:         |o------->    |
        */
        memmove(&(a->elemdata[a->offsetfirst + a->length - a->capacity]), &(a->elemdata[a->offsetfirst]), sizeof(DequeValue_t) * (a->capacity - a->offsetfirst));
        a->offsetfirst = 0;
    }
}
