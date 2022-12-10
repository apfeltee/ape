
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

/* The deque type, optionally prefixed user-defined extra members */
struct DequeList_t
{
#ifdef DEQUELIST_HEADER
    DEQUELIST_HEADER
#endif
    DequeSize_t  cap; /* capacity, actual length of the els array */
    DequeSize_t  off; /* offset to the first element in els */
    DequeSize_t  len; /* length */
    DequeValue_t els[1]; /* elements, allocated in-place */
};

typedef struct DequeList_t DequeList_t;

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
    DequeSize_t idx = a->off + i;
    if(idx >= a->cap)
        idx -= a->cap;
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
        cap = DEQUELIST_MIN_CAPACITY;
    a = (DequeList_t*)DEQUELIST_ALLOC(deqlist_sizeof(cap));
    if(!a)
        DEQUELIST_OOM();
    a->len = len;
    a->off = 0;
    a->cap = cap;
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
    DEQUELIST_FREE(a, deqlist_sizeof(a->cap));
}

/*
 * Returns the number of elements in the array.
 */
static inline DequeSize_t deqlist_len(DequeList_t* a)
{
    return a->len;
}

/*
 * Fetch the element at the zero based index i. The index bounds are not
 * checked.
 */
static inline DequeValue_t deqlist_get(DequeList_t* a, DequeSize_t i)
{
    DequeSize_t pos = deqlist_idx(a, i);
    return a->els[pos];
}

/*
 * Set (replace) the element at the zero based index i. The index bounds are not
 * checked.
 */
static inline void deqlist_set(DequeList_t* a, DequeSize_t i, DequeValue_t value)
{
    DequeSize_t pos = deqlist_idx(a, i);
    a->els[pos] = value;
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
        memset(&a->els[0], 0, sizeof(DequeValue_t) * deqlist_idx(a, n + i));
        memset(&a->els[deqlist_idx(a, i)], 0, sizeof(DequeValue_t) * (a->cap - deqlist_idx(a, i)));
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
        memset(&a->els[deqlist_idx(a, i)], 0, sizeof(DequeValue_t) * n);
    }
}

/*
 * Clones an array deque, preserving the internal memory layout.
 */
static inline DequeList_t* deqlist_clone(DequeList_t* a)
{
    size_t sz = deqlist_sizeof(a->cap);
    void*  clone = DEQUELIST_ALLOC(sz);
    if(!clone)
        DEQUELIST_OOM();
    return (DequeList_t*)memcpy(clone, a, sz);
}

/*
 * Create an struct aadeque from the n values pointed to by array.
 */
static inline DequeList_t* deqlist_from_array(DequeValue_t* array, DequeSize_t n)
{
    DequeList_t* a = deqlist_create(n);
    memcpy(a->els, array, sizeof(DequeValue_t) * n);
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
    if(a->len != n)
        return 0;
    for(i = 0; i < n; i++)
        if(deqlist_get(a, i) != array[i])
            return 0;
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
    if(a->cap < a->len + n)
    {
        /* calulate and set new capacity */
        DequeSize_t oldcap = a->cap;
        do
        {
            a->cap = a->cap << 1;
        } while(a->cap < a->len + n);
        /* allocate more mem */
        a = (DequeList_t*)DEQUELIST_REALLOC(a, deqlist_sizeof(a->cap), deqlist_sizeof(oldcap));
        if(!a)
            DEQUELIST_OOM();
        /* adjust content to the increased capacity */
        if(a->off + a->len > oldcap)
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
            memcpy(&(a->els[a->off + a->cap - oldcap]), &(a->els[a->off]), sizeof(DequeValue_t) * (a->cap - oldcap));
#ifdef DEQUELIST_CLEAR_UNUSED_MEM
            memset(&(a->els[a->off]), 0, sizeof(DequeValue_t) * (a->cap - oldcap));
#endif
            a->off += a->cap - oldcap;
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
    DequeSize_t doublemincap = mincap << 1;
    if(a->cap >= doublemincap && a->cap > DEQUELIST_MIN_CAPACITY)
    {
        /*
		 * Halve the capacity as long as it is >= twice the minimum capacity.
		 */
        DequeSize_t oldcap = a->cap;
        /* Calulate and set new capacity */
        do
        {
            a->cap = a->cap >> 1;
        } while(a->cap >= doublemincap && a->cap > DEQUELIST_MIN_CAPACITY);
        /* Adjust content to decreased capacity */
        if(a->off + a->len > oldcap)
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
            memcpy(&(a->els[a->off + a->cap - oldcap]), &(a->els[a->off]), sizeof(DequeValue_t) * (oldcap - a->off));
            a->off += a->cap - oldcap;
        }
        else if(a->off >= a->cap)
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
            memcpy(&(a->els[0]), &(a->els[a->off]), sizeof(DequeValue_t) * a->len);
            a->off = 0;
        }
        else if(a->off + a->len > a->cap)
        {
            /*
			 * It will overflow the new cap. Make it warp.
			 *
			 *            0       cap      oldcap
			 *           /        /         /
			 * Before:  |     o--|-->      |
			 * After:   |-->  o--|
			 */
            memcpy(&(a->els[0]), &(a->els[a->cap]), sizeof(DequeValue_t) * (a->off + a->len - a->cap));
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
        a = (DequeList_t*)DEQUELIST_REALLOC(a, deqlist_sizeof(a->cap), deqlist_sizeof(oldcap));
        if(!a)
            DEQUELIST_OOM();
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
    return deqlist_compact_to(a, a->len << 1);
}

/*----------------------------------------------------------------------------
 * Functions that extend the deque in either end. The added elements will not
 * be initialized, thus their indices will contain undefined values.
 *
 * These functions resize the underlying buffer when needed. Like realloc,
 * they try to resize the underlying buffer and return a. It there is not
 * enough space, they copy the data to a new memory location, free the old
 * memory and return a pointer to the new the new memory allocation.
 *----------------------------------------------------------------------------*/

/*
 * Inserts n undefined values after the last element.
 */
static inline DequeList_t* deqlist_make_space_after(DequeList_t* a, DequeSize_t n)
{
    a = deqlist_reserve(a, n);
    a->len += n;
    return a;
}

/*
 * Inserts n undefined values before the first element.
 */
static inline DequeList_t* deqlist_make_space_before(DequeList_t* a, DequeSize_t n)
{
    a = deqlist_reserve(a, n);
    a->off = deqlist_idx(a, a->cap - n);
    a->len += n;
    return a;
}

/*----------------------------------------------------------------------------
 * Functions for deleting multiple elements
 *----------------------------------------------------------------------------*/

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
    deqlist_clear(a, offset + length, a->len - length);
#endif
    a->off = deqlist_idx(a, offset);
    a->len = length;
    return deqlist_compact_some(a);
}

/*
 * Deletes the last n elements.
 */
static inline DequeList_t* deqlist_delete_last_n(DequeList_t* a, DequeSize_t n)
{
    return deqlist_crop(a, 0, a->len - n);
}

/*
 * Deletes the first n elements.
 */
static inline DequeList_t* deqlist_delete_first_n(DequeList_t* a, DequeSize_t n)
{
    return deqlist_crop(a, n, a->len - n);
}

/*---------------------------------------------------------------------------
 * The pure deque operations: Inserting and deleting values in both ends.
 * Shift, unshift, push, pop.
 *---------------------------------------------------------------------------*/

/*
 * Insert an element at the beginning.
 * May change aptr if it needs to be reallocated.
 */
static inline void deqlist_unshift(DequeList_t** aptr, DequeValue_t value)
{
    *aptr = deqlist_make_space_before(*aptr, 1);
    (*aptr)->els[(*aptr)->off] = value;
}

/*
 * Remove an element at the beginning and return its value.
 * May change aptr if it needs to be reallocated.
*/
static inline DequeValue_t deqlist_shift(DequeList_t** aptr)
{
    DequeValue_t value = (*aptr)->els[(*aptr)->off];
    *aptr = deqlist_crop(*aptr, 1, (*aptr)->len - 1);
    return value;
}

/*
 * Insert an element at the end.
 * May change aptr if it needs to be reallocated.
 */
static inline void deqlist_push(DequeList_t** aptr, DequeValue_t value)
{
    *aptr = deqlist_make_space_after(*aptr, 1);
    (*aptr)->els[deqlist_idx(*aptr, (*aptr)->len - 1)] = value;
}

/*
 * Remove an element at the end and return its value.
 * May change aptr if it needs to be reallocated.
 */
static inline DequeValue_t deqlist_pop(DequeList_t** aptr)
{
    DequeValue_t value = (*aptr)->els[deqlist_idx(*aptr, (*aptr)->len - 1)];
    *aptr = deqlist_crop(*aptr, 0, (*aptr)->len - 1);
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
    DequeSize_t i = a1->len, j;
    a1 = deqlist_make_space_after(a1, a2->len);
    /* TODO: copy using memcpy instead of the loop */
    for(j = 0; j < a2->len; j++)
        deqlist_set(a1, i++, deqlist_get(a2, j));
    return a1;
}

/*
 * Prepends all elements of s2 to a1 and returns the (possibly reallocated) a1.
 */
static inline DequeList_t* deqlist_prepend(DequeList_t* a1, DequeList_t* a2)
{
    DequeSize_t i = 0, j;
    a1 = deqlist_make_space_before(a1, a2->len);
    /* TODO: copy using memcpy instead of the loop */
    for(j = 0; j < a2->len; j++)
        deqlist_set(a1, i++, deqlist_get(a2, j));
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
    DequeList_t* b = deqlist_create(length);
    /* TODO: use memcpy instead of the loop */
    DequeSize_t i;
    for(i = 0; i < length; i++)
    {
        DequeValue_t x = deqlist_get(a, i + offset);
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
    return deqlist_compact_to(a, a->len);
}

/*
 * Joins the parts together in memory, possibly in the wrong order. This is
 * useful if you want to sort or shuffle the underlaying array using functions
 * for raw arrays (such as qsort).
 */
static inline void deqlist_make_contiguous_unordered(DequeList_t* a)
{
    if(a->off + a->len > a->cap)
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
        memmove(&(a->els[a->off + a->len - a->cap]), &(a->els[a->off]), sizeof(DequeValue_t) * (a->cap - a->off));
        a->off = 0;
    }
}
