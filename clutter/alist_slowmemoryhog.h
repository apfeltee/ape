
#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/*
 * Copyright (c) 2001 Sandro Sigala.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
NAME
     ApePlainList_t — Doubly-linked lists

SYNOPSIS
     #include <ApePlainList_t.h>

     typedef (* ... *) ApePlainList_t;

DESCRIPTION
     The ApePlainList_t library provides a high-level interface to doubly-linked lists.

     The library provides one type: ApePlainList_t.

     The alist_new() function allocates a new empty list.

     The alist_copy() function allocates a new list and fills it with the
     items of the argument list al.

     The alist_destroy() function deallocates the argument list al.

     The alist_clear() function removes all the items from the argument list
     al.

     The alist_count() function returns the number of items contained in the
     argument list al.

     The alist_isempty() function returns a zero value if the argument list al
     is empty, non-zero otherwise.

     The alist_first() function returns the first item of the argument list
     al.

     The alist_last() function returns the last item of the argument list al.

     The alist_prev() function returns the previous item of the argument list
     al.

     The alist_next() function returns the next item of the argument list al.

     The alist_current() function returns the current item of the argument
     list al.

     The alist_current_idx() function returns the current item index of the
     argument list al.  The first item has a zero index, while the latest item
     has alist_count(al)-1 index.

     The alist_insert() function inserts the p item at position i.  Previously
     existing items at position i will be moved after the inserted item.

     The alist_append() function appends the p item to the argument list al.

     The alist_prepend() function prepends the p item to the argument list al.

     The alist_remove() function removes the current item from the argument
     list al.  The function will return a zero value if the operation was suc‐
     cessful, -1 otherwise.

     The alist_remove_ptr() function removes the argument item p from the ar‐
     gument list al.  The function will return a zero value if the operation
     was successful, -1 otherwise.

     The alist_remove_idx() function removes the item at index i from the ar‐
     gument list al.  The function will return a zero value if the operation
     was successful, -1 otherwise.

     The alist_take() function removes the current item from the argument list
     al.  The function will return the removed item.

     The alist_take_idx() function removes the item at index i from the argu‐
     ment list al.  The function will return the removed item.

     The alist_sort() functions sorts the argument list al using the compari‐
     son function pointed by the argument cmp.

     The contents of the list are sorted in ascending order according to a
     comparison function pointed to by cmp, which is called with two arguments
     that point to the objects being compared.  The comparison function must
     return an integer less than, equal to, or greater than zero if the first
     argument is considered to be respectively less than, equal to, or greater
     than the second. If two members compare as equal, their order in the
     sorted list is undefined.

     The alist_at() function returns the item at index i contained in the ar‐
     gument list al.

     The alist_find() function finds the first occurrence of the argument item
     p into the argument list al.

DEBUGGING
     If you would like to debug your program, you should define the macro
     ALIST_NO_MACRO_DEFS before including the header of this library, i.e.

           #define ALIST_NO_MACRO_DEFS
           #include <ApePlainList_t.h>

     This prevents defining at least the following function macros that makes
     code faster but debugging harder: alist_isempty(), alist_count(),
     alist_first(), alist_last(), alist_prev(), alist_next(), alist_current(),
     alist_current_idx().  Side effects (like incrementing the argument) in
     parameters of these macros should be avoided.

EXAMPLES
     Sorting and printing a list of words:

           int sorter(const void *p1, const void *p2)
           {
                   return strcmp(*(char **)p1, *(char **)p2);
           }

           int main(void)
           {
                   ApePlainList_t* al;
                   char *s;
                   al = alist_new();
                   alist_append(al, "long");
                   alist_append(al, "int");
                   alist_append(al, "double");
                   alist_append(al, "char");
                   alist_append(al, "float");
                   alist_sort(al, sorter);
                   for (s = alist_first(al); s != NULL; s = alist_next(al))
                           printf("Item #%d: %s\n",
                                  alist_current_idx(al) + 1, s);
                   alist_destroy(al);
                   return 0;
           }

     An address book:

           typedef struct person {
                   char *first_name;
                   char *last_name;
                   char *phone;
           } person;

           int sorter(const void *p1, const void *p2)
           {
                   person *pe1 = *(person **)p1, *pe2 = *(person **)p2;
                   int v;
                   if ((v = strcmp(pe1->last_name, pe2->last_name)) == 0)
                           v = strcmp(pe1->first_name, pe2->first_name);
                   return v;
           }

           int main(void)
           {
                   ApePlainList_t* al;
                   person *p, p1, p2, p3, p4;
                   al = alist_new();
                   p1.first_name = "Joe";
                   p1.last_name = "Green";
                   p1.phone = "555-123456";
                   p2.first_name = "Joe";
                   p2.last_name = "Doe";
                   p2.phone = "555-654321";
                   p3.first_name = "Dennis";
                   p3.last_name = "Ritchie";
                   p3.phone = "555-314159";
                   p4.first_name = "Brian";
                   p4.last_name = "Kernighan";
                   p4.phone = "555-271828";

                   alist_append(al, &p1);
                   alist_append(al, &p2);
                   alist_append(al, &p3);
                   alist_append(al, &p4);
                   alist_sort(al, sorter);
                   for (p = alist_first(al); p != NULL; p = alist_next(al))
                           printf("Person #%d: %10s, %-10s Tel.: %s\n",
                                  alist_current_idx(al) + 1,
                                  p->first_name, p->last_name, p->phone);
                   alist_destroy(al);
                   return 0;
           }

     A list of lists:

           ApePlainList_t* al1;
           ApePlainList_t* al2;
           ApePlainList_t* al3;
           ApePlainList_t* al;
           char *s;
           al1 = alist_new();
           al2 = alist_new();
           al3 = alist_new();
           alist_append(al2, "obj1");
           alist_append(al2, "obj2");
           alist_append(al2, "obj3");
           alist_append(al3, "obja");
           alist_append(al3, "objb");
           alist_append(al3, "objc");
           alist_append(al1, al2);
           alist_append(al1, al3);
           for (al = alist_first(al1); al != NULL; al = alist_next(al1))
                   for (s = alist_first(al); s != NULL; s = alist_next(al))
                           printf("String: %s\n");
           alist_destroy(al1);
           alist_destroy(al2);
           alist_destroy(al3);

AUTHORS
     Sandro Sigala <sandro@sigala.it>
*/

/*
 * Fast macro definitions.
 */

#define alist_isempty(al)	(al->size == 0)
#define alist_count(al)		(al->size)
#define alist_first(al)		(al->idx = 0, (al->current = al->head) != NULL ? al->current->p : NULL)
#define alist_last(al)		((al->current = al->tail) != NULL ? (al->idx = al->size - 1, a->current->p) : NULL)
#define alist_prev(al)		((al->current != NULL) ? (al->current = al->current->prev, (al->current != NULL) ? --al->idx, al->current->p : NULL) : NULL)
#define alist_next(al)		((al->current != NULL) ? (al->current = al->current->next, (al->current != NULL) ? ++al->idx, al->current->p : NULL) : NULL)
#define alist_current(al)	((al->current != NULL) ? al->current->p : NULL)
#define alist_current_idx(al)	((al->current != NULL) ? al->idx : -1)

#if !defined(APE_INLINE)
    #define APE_INLINE inline
#endif


static ApePlainNode_t *aentry_new(void *p);
static void aentry_destroy(ApePlainNode_t *ae);
static ApePlainNode_t *find_aentry(ApePlainList_t *al, unsigned int i);
static ApePlainNode_t *find_aentry_ptr(ApePlainList_t *al, void *p);
static ApePlainList_t *alist_new(void);
static void alist_destroy(ApePlainList_t *al);
static void alist_sort(ApePlainList_t *al, int (*cmp)(const void *p1, const void *p2));
static ApePlainList_t *alist_copy(ApePlainList_t *src);
static void alist_clear(ApePlainList_t *al);
static int (alist_count)(ApePlainList_t *al);
static int (alist_isempty)(ApePlainList_t *al);
static void *(alist_first)(ApePlainList_t *al);
static void *(alist_last)(ApePlainList_t *al);
static void *(alist_prev)(ApePlainList_t *al);
static void *(alist_next)(ApePlainList_t *al);
static void *(alist_current)(ApePlainList_t *al);
static int (alist_current_idx)(ApePlainList_t *al);
static void alist_insert(ApePlainList_t *al, unsigned int i, void *p);
static void alist_prepend(ApePlainList_t *al, void *p);
static void alist_append(ApePlainList_t *al, void *p);
static void take_off(ApePlainList_t *al, ApePlainNode_t *ae);
static int alist_remove(ApePlainList_t *al);
static int alist_remove_ptr(ApePlainList_t *al, void *p);
static int alist_remove_idx(ApePlainList_t *al, unsigned int i);
static void *alist_take(ApePlainList_t *al);
static void *alist_take_idx(ApePlainList_t *al, unsigned int i);
static void alist_sort(ApePlainList_t *al, int (*cmp)(const void *p1, const void *p2));
static void *alist_at(ApePlainList_t *al, unsigned int i);
static int alist_find(ApePlainList_t *al, void *p);


static APE_INLINE ApePlainNode_t* aentry_new(void *p)
{
    ApePlainNode_t* ae;
    ae = (ApePlainNode_t*)malloc(sizeof *ae);
    ae->p = p;
    ae->prev = ae->next = NULL;
    return ae;
}

static APE_INLINE void aentry_destroy(ApePlainNode_t* ae)
{
    ae->p = NULL;			/* Useful for debugging. */
    ae->prev = ae->next = NULL;
    free(ae);
}

static APE_INLINE ApePlainNode_t* find_aentry(ApePlainList_t* al, unsigned int i)
{
    ApePlainNode_t* ae;
    unsigned int count;
    for (ae = al->head, count = 0; ae != NULL; ae = ae->next, ++count)
        if (count == i)
            return ae;
    return NULL;
}

static APE_INLINE ApePlainNode_t* find_aentry_ptr(ApePlainList_t* al, void *p)
{
    ApePlainNode_t* ae;
    for (ae = al->head; ae != NULL; ae = ae->next)
        if (ae->p == p)
            return ae;
    return NULL;
}

static APE_INLINE ApePlainList_t* alist_new(void)
{
    ApePlainList_t* al;
    al = (ApePlainList_t*)malloc(sizeof *al);
    al->head = al->tail = al->current = NULL;
    al->idx = 0;
    al->size = 0;
    return al;
}

static APE_INLINE void alist_destroy(ApePlainList_t* al)
{
	assert(al != NULL);
	alist_clear(al);
	free(al);
}

static APE_INLINE void alist_sort(ApePlainList_t* al, int (*cmp)(const void *p1, const void *p2))
{
	void **vec;
	ApePlainNode_t* ae;
	int i;
	assert(al != NULL && cmp != NULL);
	if (al->size == 0)
		return;
	vec = (void **)malloc(sizeof(void *) * al->size);
	for (ae = al->head, i = 0; ae != NULL; ae = ae->next, ++i)
		vec[i] = ae->p;
	qsort(vec, al->size, sizeof(void *), cmp);
	for (ae = al->head, i = 0; ae != NULL; ae = ae->next, ++i)
		ae->p = vec[i];
	free(vec);
}


static APE_INLINE ApePlainList_t* alist_copy(ApePlainList_t* src)
{
    ApePlainList_t* al;
    ApePlainNode_t* ae;
    assert(src != NULL);
    al = alist_new();
    for (ae = src->head; ae != NULL; ae = ae->next)
        alist_append(al, ae->p);
    return al;
}

static APE_INLINE void alist_clear(ApePlainList_t* al)
{
    ApePlainNode_t* ae;
    ApePlainNode_t* next;
    assert(al != NULL);
    for (ae = al->head; ae != NULL; ae = next) {
        next = ae->next;
        aentry_destroy(ae);
    }
    al->head = al->tail = al->current = NULL;
    al->idx = 0;
    al->size = 0;
}

static APE_INLINE int (alist_count)(ApePlainList_t* al)
{
    return al->size;
}

static APE_INLINE int (alist_isempty)(ApePlainList_t* al)
{
    return al->size == 0;
}

static APE_INLINE void* (alist_first)(ApePlainList_t* al)
{
    assert(al != NULL);
    al->current = al->head;
    al->idx = 0;
    if (al->current != NULL)
        return al->current->p;
    return NULL;
}

static APE_INLINE void* (alist_last)(ApePlainList_t* al)
{
    assert(al != NULL);
    al->current = al->tail;
    if (al->current != NULL) {
        al->idx = al->size - 1;
        return al->current->p;
    }
    return NULL;
}

static APE_INLINE void* (alist_prev)(ApePlainList_t* al)
{
    assert(al != NULL);
    if (al->current != NULL) {
        al->current = al->current->prev;
        if (al->current != NULL) {
            --al->idx;
            return al->current->p;
        } else
            return NULL;
    }
    return NULL;
}

static APE_INLINE void* (alist_next)(ApePlainList_t* al)
{
    assert(al != NULL);
    if (al->current != NULL) {
        al->current = al->current->next;
        if (al->current != NULL) {
            ++al->idx;
            return al->current->p;
        } else
            return NULL;
    }
    return NULL;
}

static APE_INLINE void* (alist_current)(ApePlainList_t* al)
{
    assert(al != NULL);
    if (al->current != NULL)
        return al->current->p;
    return NULL;
}

static APE_INLINE int (alist_current_idx)(ApePlainList_t* al)
{
    if (al->current != NULL)
        return al->idx;
    return -1;
}

static APE_INLINE void alist_insert(ApePlainList_t* al, unsigned int i, void *p)
{
    if (i >= al->size) {
        i = al->size;
        alist_append(al, p);
    } else if (i == 0)
        alist_prepend(al, p);
    else {
        ApePlainNode_t* oldae = find_aentry(al, i);
        ApePlainNode_t* ae = aentry_new(p);
        ae->next = oldae;
        if (oldae->prev != NULL) {
            ae->prev = oldae->prev;
            ae->prev->next = ae;
        }
        oldae->prev = ae;
        al->current = ae;
        al->idx = i;
        ++al->size;
    }
}

static APE_INLINE void alist_prepend(ApePlainList_t* al, void *p)
{
    ApePlainNode_t* ae;
    assert(al != NULL);
    ae = aentry_new(p);
    if (al->head != NULL) {
        al->head->prev = ae;
        ae->next = al->head;
        al->head = ae;
    } else
        al->head = al->tail = ae;
    al->current = ae;
    al->idx = 0;
    ++al->size;
}

static APE_INLINE void alist_append(ApePlainList_t* al, void *p)
{
    ApePlainNode_t* ae;
    assert(al != NULL);
    ae = aentry_new(p);
    if (al->tail != NULL) {
        al->tail->next = ae;
        ae->prev = al->tail;
        al->tail = ae;
    } else
        al->head = al->tail = ae;
    al->current = ae;
    ++al->size;
    al->idx = al->size - 1;
}

static APE_INLINE void take_off(ApePlainList_t* al, ApePlainNode_t* ae)
{
    if (ae->prev != NULL)
        ae->prev->next = ae->next;
    if (ae->next != NULL)
        ae->next->prev = ae->prev;
    if (al->head == ae)
        al->head = ae->next;
    if (al->tail == ae)
        al->tail = ae->prev;
    al->current = NULL;
    al->idx = 0;
    --al->size;
}

static APE_INLINE int alist_remove(ApePlainList_t* al)
{
    ApePlainNode_t* ae;
    assert(al != NULL);
    if ((ae = al->current) == NULL)
        return -1;
    take_off(al, ae);
    aentry_destroy(ae);
    return 0;
}

static APE_INLINE int alist_remove_ptr(ApePlainList_t* al, void *p)
{
    ApePlainNode_t* ae;
    assert(al != NULL);
    if ((ae = find_aentry_ptr(al, p)) != NULL) {
        take_off(al, ae);
        aentry_destroy(ae);
        return 0;
    }
    return -1;
}

static APE_INLINE int alist_remove_idx(ApePlainList_t* al, unsigned int i)
{
    ApePlainNode_t* ae;
    assert(al != NULL);
    if (i >= al->size)
        return -1;
    ae = find_aentry(al, i);
    take_off(al, ae);
    aentry_destroy(ae);
    return 0;
}

static APE_INLINE void *alist_take(ApePlainList_t* al)
{
    ApePlainNode_t* ae;
    void *p;
    assert(al != NULL);
    if ((ae = al->current) == NULL)
        return NULL;
    take_off(al, ae);
    p = ae->p;
    aentry_destroy(ae);
    return p;
}

static APE_INLINE void *alist_take_idx(ApePlainList_t* al, unsigned int i)
{
    ApePlainNode_t* ae;
    void *p;
    assert(al != NULL);
    if (i >= al->size)
        return NULL;
    ae = find_aentry(al, i);
    take_off(al, ae);
    p = ae->p;
    aentry_destroy(ae);
    return p;
}


static APE_INLINE void *alist_at(ApePlainList_t* al, unsigned int i)
{
    ApePlainNode_t* ae;
    assert(al != NULL);
    if (i >= al->size)
        return NULL;
    ae = find_aentry(al, i);
    return ae->p;
}

static APE_INLINE int alist_find(ApePlainList_t* al, void *p)
{
    ApePlainNode_t* ae;
    unsigned int count;
    assert(al != NULL);
    for (ae = al->head, count = 0; ae != NULL; ae = ae->next, ++count)
        if (ae->p == p)
            return count;
    return -1;
}

#if defined(ALIST_TEST)
typedef struct person person;

struct person
{
    char *first_name;
    char *last_name;
    char *phone;
};

int sorter(const void *p1, const void *p2)
{
    person *pe1 = *(person **)p1, *pe2 = *(person **)p2;
    int v;
    if ((v = strcmp(pe1->last_name, pe2->last_name)) == 0)
        v = strcmp(pe1->first_name, pe2->first_name);
    return v;
}


int main()
{
    ApePlainList_t* al;
    person *p, p1, p2, p3, p4;
    al = alist_new();
    p1.first_name = "Joe";
    p1.last_name = "Green";
    p1.phone = "555-123456";
    p2.first_name = "Joe";
    p2.last_name = "Doe";
    p2.phone = "555-654321";
    p3.first_name = "Dennis";
    p3.last_name = "Ritchie";
    p3.phone = "555-314159";
    p4.first_name = "Brian";
    p4.last_name = "Kernighan";
    p4.phone = "555-271828";
    alist_append(al, &p1);
    alist_append(al, &p2);
    alist_append(al, &p3);
    alist_append(al, &p4);
    alist_sort(al, sorter);
    for (p = alist_first(al); p != NULL; p = alist_next(al))
    {
        printf("Person #%d: %10s, %-10s Tel.: %s\n", alist_current_idx(al) + 1, p->first_name, p->last_name, p->phone);
    }
    alist_destroy(al);
    return 0;    
}
#endif
