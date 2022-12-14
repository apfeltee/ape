
#pragma once

/*
* NOTE: this is a heavily modified version of sds!
*
* the `struct`ures have been renamed to unify namespacing, and while the
* API has not changed, it is likely not binary compatible.
* everything is inlined - because of this, this header does *not* export anything, and
* is likely not going to compatible with ancient compilers. the existence of the
* "inline" keyword is required.
* include this file whereever you intend to use sds, and you should be good to go.
*/

/* SDSLib 2.0 -- A C dynamic strings library
 *
 * Copyright (c) 2006-2015, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2015, Oran Agra
 * Copyright (c) 2015, Redis Labs, Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include <sys/types.h>

#define ds_wrapmalloc malloc
#define ds_wraprealloc realloc
#define ds_wrapfree free

#define SDS_MAX_PREALLOC (1024 * 1024)
#define SDS_TYPE_5 0
#define SDS_TYPE_8 1
#define SDS_TYPE_16 2
#define SDS_TYPE_32 3
#define SDS_TYPE_64 4
#define SDS_TYPE_MASK 7
#define SDS_TYPE_BITS 3
#define SDS_HDR_VAR(T, s) struct DynStrHead##T##_t* sh = (struct DynStrHead##T##_t*)((s) - (sizeof(struct DynStrHead##T##_t)));
#define SDS_HDR(T, s) ((struct DynStrHead##T##_t*)((s) - (sizeof(struct DynStrHead##T##_t))))
#define SDS_TYPE_5_LEN(f) ((f) >> SDS_TYPE_BITS)

#define SDS_LLSTR_SIZE 21

#if defined(_MSC_VER)
    #define SDS_ATTRIBUTE(...)
    #define ssize_t signed long
#else
    #define SDS_ATTRIBUTE(...) __attribute__(__VA_ARGS__)
#endif

typedef char DynString_t;
typedef struct DynStrHead5_t DynStrHead5_t;
typedef struct DynStrHead8_t DynStrHead8_t;
typedef struct DynStrHead16_t DynStrHead16_t;
typedef struct DynStrHead32_t DynStrHead32_t;
typedef struct DynStrHead64_t DynStrHead64_t;

static const char* SDS_NOINIT = "SDS_NOINIT";

/* Note: DynStrHead5_t is never used, we just access the flags byte directly.
 * However is here to document the layout of type 5 SDS strings. */
struct SDS_ATTRIBUTE((__packed__)) DynStrHead5_t
{
    unsigned char flags; /* 3 lsb of type, and 5 msb of string length */
    char buf[];
};

struct SDS_ATTRIBUTE((__packed__)) DynStrHead8_t
{
    uint8_t len; /* used */
    uint8_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct SDS_ATTRIBUTE((__packed__)) DynStrHead16_t
{
    uint16_t len; /* used */
    uint16_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct SDS_ATTRIBUTE((__packed__)) DynStrHead32_t
{
    uint32_t len; /* used */
    uint32_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};

struct SDS_ATTRIBUTE((__packed__)) DynStrHead64_t
{
    uint64_t len; /* used */
    uint64_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};

/* ds_getallocated() = ds_getavailable() + ds_getlength() */
static inline size_t ds_getallocated(const DynString_t* s);
static inline void ds_setallocated(DynString_t* s, size_t newlen);


/* Create a new sds string with the content specified by the 'init' pointer
 * and 'initlen'.
 * If NULL is used for 'init' the string is initialized with zero bytes.
 * If SDS_NOINIT is used, the buffer is left uninitialized;
 *
 * The string is always null-termined (all the sds strings are, always) so
 * even if you create an sds string with:
 *
 * mystring = ds_newlen("abc",3);
 *
 * You can print the string with printf() as there is an implicit \0 at the
 * end of the string. However the string is binary safe and can contain
 * \0 characters in the middle, as the length is stored in the sds header. */
static inline DynString_t* ds_newlen(const void* init, size_t initlen);

/* Create an empty (zero length) sds string. Even in this case the string
 * always has an implicit null term. */
static inline DynString_t* ds_newempty(void);

/* Create a new sds string starting from a null terminated C string. */
static inline DynString_t* ds_new(const char* init);

/* Duplicate an sds string. */
static inline DynString_t* ds_duplicate(const DynString_t* s);

/* Free an sds string. No operation is performed if 's' is NULL. */
static inline void ds_destroy(DynString_t* s);


/* Set the sds string length to the length as obtained with strlen(), so
 * considering as content only up to the first null term character.
 *
 * This function is useful when the sds string is hacked manually in some
 * way, like in the following example:
 *
 * s = ds_new("foobar");
 * s[2] = '\0';
 * ds_updatelength(s);
 * printf("%d\n", ds_getlength(s));
 *
 * The output will be "2", but if we comment out the call to ds_updatelength()
 * the output will be "6" as the string was modified but the logical length
 * remains 6 bytes. */
static inline void ds_updatelength(DynString_t* s);


/* Modify an sds string in-place to make it empty (zero length).
 * However all the existing buffer is not discarded but set as free space
 * so that next append operations will not require allocations up to the
 * number of bytes previously available. */
static inline void ds_clear(DynString_t* s);

/* Enlarge the free space at the end of the sds string so that the caller
 * is sure that after calling this function can overwrite up to addlen
 * bytes after the end of the string, plus one more byte for nul term.
 *
 * Note: this does not change the *length* of the sds string as returned
 * by ds_getlength(), but only the free buffer space we have. */
static inline DynString_t* ds_makeroomfor(DynString_t* s, size_t addlen);


/* Reallocate the sds string so that it has no free space at the end. The
 * contained string remains not altered, but next concatenation operations
 * will require a reallocation.
 *
 * After the call, the passed sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
static inline DynString_t* ds_removefreespace(DynString_t* s);

/* Return the total size of the allocation of the specified sds string,
 * including:
 * 1) The sds header before the pointer.
 * 2) The string.
 * 3) The free buffer at the end if any.
 * 4) The implicit null term.
 */
static inline size_t ds_interngetallocsize(DynString_t* s);


/* Return the pointer of the actual SDS allocation (normally SDS strings
 * are referenced by the start of the string buffer). */
static inline void* ds_interngetallocptr(DynString_t* s);

/* Increment the sds length and decrements the left free space at the
 * end of the string according to 'incr'. Also set the null term
 * in the new end of the string.
 *
 * This function is used in order to fix the string length after the
 * user calls ds_makeroomfor(), writes something after the end of
 * the current string, and finally needs to set the new length.
 *
 * Note: it is possible to use a negative increment in order to
 * right-trim the string.
 *
 * Usage example:
 *
 * Using ds_internincrlength() and ds_makeroomfor() it is possible to mount the
 * following schema, to cat bytes coming from the kernel to the end of an
 * sds string without copying into an intermediate buffer:
 *
 * oldlen = ds_getlength(s);
 * s = ds_makeroomfor(s, BUFFER_SIZE);
 * nread = read(fd, s+oldlen, BUFFER_SIZE);
 * ... check for nread <= 0 and handle it ...
 * ds_internincrlength(s, nread);
 */
static inline void ds_internincrlength(DynString_t* s, ssize_t incr);

/* Grow the sds to have the specified length. Bytes that were not part of
 * the original length of the sds will be set to zero.
 *
 * if the specified length is smaller than the current length, no operation
 * is performed. */
static inline DynString_t* ds_growzero(DynString_t* s, size_t len);

/* Append the specified binary-safe string pointed by 't' of 'len' bytes to the
 * end of the specified sds string 's'.
 *
 * After the call, the passed sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
static inline DynString_t* ds_appendlen(DynString_t* s, const void* t, size_t len);

/* Append the specified null termianted C string to the sds string 's'.
 *
 * After the call, the passed sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
static inline DynString_t* ds_append(DynString_t* s, const char* t);

static inline DynString_t* ds_appendchar(DynString_t* s, char ch);

/* Append the specified sds 't' to the existing sds 's'.
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
static inline DynString_t* ds_appendsds(DynString_t* s, const DynString_t* t);


/* Destructively modify the sds string 's' to hold the specified binary
 * safe string pointed by 't' of length 'len' bytes. */
static inline DynString_t* ds_copylength(DynString_t* s, const char* t, size_t len);

/* Like ds_copylength() but 't' must be a null-termined string so that the length
 * of the string is obtained with strlen(). */
static inline DynString_t* ds_copy(DynString_t* s, const char* t);

/*
* Helper for sdscatlonglong() doing the actual number -> string
* conversion. 's' must point to a string with room for at least
* SDS_LLSTR_SIZE bytes.
*
* The function returns the length of the null-terminated string
* representation stored at 's'.
*/
static inline int ds_ll2str(char* s, long long value);

/* Identical ds_ll2str(), but for unsigned long long type. */
static inline int ds_ull2str(char* s, unsigned long long v);

/*
* Create an sds string from a long long value. It is much faster than:
*
* ds_appendprintf(ds_newempty(),"%lld\n", value);
*/
static inline DynString_t* ds_fromlonglong(long long value);

 
/*
* Append to the sds string 's' a string obtained using printf-alike format
* specifier.
*
* After the call, the modified sds string is no longer valid and all the
* references must be substituted with the new pointer returned by the call.
*
* Example:
*
* s = ds_new("Sum is: ");
* s = ds_appendprintf(s,"%d+%d = %d",a,b,a+b).
*
* Often you need to create a string from scratch with the printf-alike
* format. When this is the need, just use ds_newempty() as the target string:
*
* s = ds_appendprintf(ds_newempty(), "... your format ...", args);
*/
#ifdef __GNUC__
static inline DynString_t* ds_appendprintf(DynString_t* s, const char* fmt, ...) SDS_ATTRIBUTE((format(printf, 2, 3)));
#else
static inline DynString_t* ds_appendprintf(DynString_t* s, const char* fmt, ...);
#endif

/* Like ds_appendprintf() but gets va_list instead of being variadic. */
static inline DynString_t* ds_appendvprintf(DynString_t* s, const char* fmt, va_list ap);

/* This function is similar to ds_appendprintf, but much faster as it does
 * not rely on sprintf() family functions implemented by the libc that
 * are often very slow. Moreover directly handling the sds string as
 * new data is concatenated provides a performance improvement.
 *
 * However this function only handles an incompatible subset of printf-alike
 * format specifiers:
 *
 * %s - C String
 * %S - SDS string
 * %i - signed int
 * %I - 64 bit signed integer (long long, int64_t)
 * %u - unsigned int
 * %U - 64 bit unsigned integer (unsigned long long, uint64_t)
 * %c - character
 * %% - Verbatim "%" character.
 */
static inline DynString_t* ds_appendfmt(DynString_t* s, char const* fmt, ...);

/* Remove the part of the string from left and from right composed just of
 * contiguous characters found in 'cset', that is a null terminted C string.
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 *
 * Example:
 *
 * s = ds_new("AA...AA.a.aa.aHelloWorld     :::");
 * s = ds_trim(s,"Aa. :");
 * printf("%s\n", s);
 *
 * Output will be just "HelloWorld".
 */
static inline DynString_t* ds_trim(DynString_t* s, const char* cset);

/* Turn the string into a smaller (or equal) string containing only the
 * substring specified by the 'start' and 'end' indexes.
 *
 * start and end can be negative, where -1 means the last character of the
 * string, -2 the penultimate character, and so forth.
 *
 * The interval is inclusive, so the start and end characters will be part
 * of the resulting string.
 *
 * The string is modified in-place.
 *
 * Example:
 *
 * s = ds_new("Hello World");
 * ds_range(s,1,-1); => "ello World"
 */
static inline void ds_range(DynString_t* s, ssize_t start, ssize_t end);

/* Apply toupper() to every character of the sds string 's'. */
static inline void ds_tolower(DynString_t* s);

/* Apply tolower() to every character of the sds string 's'. */
static inline void ds_toupper(DynString_t* s);

/* Compare two sds strings s1 and s2 with memcmp().
 *
 * Return value:
 *
 *     positive if s1 > s2.
 *     negative if s1 < s2.
 *     0 if s1 and s2 are exactly the same binary string.
 *
 * If two strings share exactly the same prefix, but one of the two has
 * additional characters, the longer string is considered to be greater than
 * the smaller one. */
static inline int ds_compare(const DynString_t* s1, const DynString_t* s2);

/* Split 's' with separator in 'sep'. An array
 * of sds strings is returned. *count will be set
 * by reference to the number of tokens returned.
 *
 * On out of memory, zero length string, zero length
 * separator, NULL is returned.
 *
 * Note that 'sep' is able to split a string using
 * a multi-character separator. For example
 * sdssplit("foo_-_bar","_-_"); will return two
 * elements "foo" and "bar".
 *
 * This version of the function is binary-safe but
 * requires length arguments. sdssplit() is just the
 * same function but for zero-terminated strings.
 */
static inline DynString_t* *ds_splitlen(const char* s, ssize_t len, const char* sep, int seplen, size_t* count);

/* Free the result returned by ds_splitlen(), or do nothing if 'tokens' is NULL. */
static inline void ds_freesplitres(DynString_t* *tokens, size_t count);


/* Append to the sds string "s" an escaped string representation where
 * all the non-printable characters (tested with isprint()) are turned into
 * escapes in the form "\n\r\a...." or "\x<hex-number>".
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
static inline DynString_t* ds_appendrepr(DynString_t* s, const char* p, size_t len, bool withquotes);

/* Helper function for ds_splitargs() that returns non zero if 'c'
 * is a valid hex digit. */
static inline int sdsutil_ishex(char c);

/* Helper function for ds_splitargs() that converts a hex digit into an
 * integer from 0 to 15 */
static inline int sdsutil_hextoint(char c);

/* Split a line into arguments, where every argument can be in the
 * following programming-language REPL-alike form:
 *
 * foo bar "newline are supported\n" and "\xff\x00otherstuff"
 *
 * The number of arguments is stored into *argc, and an array
 * of sds is returned.
 *
 * The caller should free the resulting array of sds strings with
 * ds_freesplitres().
 *
 * Note that ds_appendrepr() is able to convert back a string into
 * a quoted string in the same format ds_splitargs() is able to parse.
 *
 * The function returns the allocated tokens on success, even when the
 * input string is empty, or NULL if the input contains unbalanced
 * quotes or closed quotes followed by non space characters
 * as in: "foo"bar or "foo'
 */
static inline DynString_t* *ds_splitargs(const char* line, int* argc);

/* Modify the string substituting all the occurrences of the set of
 * characters specified in the 'from' string to the corresponding character
 * in the 'to' array.
 *
 * For instance: ds_mapchars(mystring, "ho", "01", 2)
 * will have the effect of turning the string "hello" into "0ell1".
 *
 * The function returns the sds string pointer, that is always the same
 * as the input pointer since no resize is needed. */
static inline DynString_t* ds_mapchars(DynString_t* s, const char* from, const char* to, size_t setlen);


/* Join an array of C strings using the specified separator (also a C string).
 * Returns the result as an sds string. */
static inline DynString_t* ds_join(char* *argv, int argc, char* sep);
static inline DynString_t* ds_joinsds(DynString_t* *argv, int argc, const char* sep, size_t seplen);

/* Wrappers to the allocators used by SDS. Note that SDS will actually
 * just use the macros defined into ds_getallocated.h in order to avoid to pay
 * the overhead of function calls. Here we define these wrappers only for
 * the programs SDS is linked to, if they want to touch the SDS internals
 * even if they use a different allocator. */
static inline void* ds_malloc(size_t size);
static inline void* ds_realloc(void* ptr, size_t size);
static inline void ds_free(void* ptr);

/* TODO: explain these! */
static inline int ds_getheadersize(char type);
static inline char ds_getreqtype(size_t string_size);
static inline size_t ds_getlength(const DynString_t* s);
static inline size_t ds_getavailable(const DynString_t* s);
static inline void ds_setlength(DynString_t* s, size_t newlen);
static inline void ds_increaselength(DynString_t* s, size_t inc);


static inline int ds_getheadersize(char type)
{
    switch(type & SDS_TYPE_MASK)
    {
        case SDS_TYPE_5:
            return sizeof(struct DynStrHead5_t);
        case SDS_TYPE_8:
            return sizeof(struct DynStrHead8_t);
        case SDS_TYPE_16:
            return sizeof(struct DynStrHead16_t);
        case SDS_TYPE_32:
            return sizeof(struct DynStrHead32_t);
        case SDS_TYPE_64:
            return sizeof(struct DynStrHead64_t);
    }
    return 0;
}

static inline char ds_getreqtype(size_t string_size)
{
    if(string_size < 1 << 5)
    {
        return SDS_TYPE_5;
    }
    if(string_size < 1 << 8)
    {
        return SDS_TYPE_8;
    }
    if(string_size < 1 << 16)
    {
        return SDS_TYPE_16;
    }
#if(LONG_MAX == LLONG_MAX)
    if(string_size < 1ll << 32)
    {
        return SDS_TYPE_32;
    }
    return SDS_TYPE_64;
#else
    return SDS_TYPE_32;
#endif
}

static inline size_t ds_getlength(const DynString_t* s)
{
    unsigned char flags = s[-1];
    switch(flags & SDS_TYPE_MASK)
    {
        case SDS_TYPE_5:
            {
                return SDS_TYPE_5_LEN(flags);
            }
            break;
        case SDS_TYPE_8:
            {
                return SDS_HDR(8, s)->len;
            }
            break;
        case SDS_TYPE_16:
            {
                return SDS_HDR(16, s)->len;
            }
            break;
        case SDS_TYPE_32:
            {
                return SDS_HDR(32, s)->len;
            }
            break;
        case SDS_TYPE_64:
            {
                return SDS_HDR(64, s)->len;
            }
            break;
    }
    return 0;
}

static inline size_t ds_getavailable(const DynString_t* s)
{
    unsigned char flags;
    flags = s[-1];
    switch(flags & SDS_TYPE_MASK)
    {
        case SDS_TYPE_5:
            {
                return 0;
            }
            break;
        case SDS_TYPE_8:
            {
                SDS_HDR_VAR(8, s);
                return sh->alloc - sh->len;
            }
            break;
        case SDS_TYPE_16:
            {
                SDS_HDR_VAR(16, s);
                return sh->alloc - sh->len;
            }
            break;
        case SDS_TYPE_32:
            {
                SDS_HDR_VAR(32, s);
                return sh->alloc - sh->len;
            }
            break;
        case SDS_TYPE_64:
            {
                SDS_HDR_VAR(64, s);
                return sh->alloc - sh->len;
            }
            break;
    }
    return 0;
}

static inline void ds_setlength(DynString_t* s, size_t newlen)
{
    unsigned char flags;
    unsigned char* fp;
    flags = s[-1];
    switch(flags & SDS_TYPE_MASK)
    {
        case SDS_TYPE_5:
            {
                fp = ((unsigned char*)s) - 1;
                *fp = SDS_TYPE_5 | (newlen << SDS_TYPE_BITS);
            }
            break;
        case SDS_TYPE_8:
            {
                SDS_HDR(8, s)->len = newlen;
            }
            break;
        case SDS_TYPE_16:
            {
                SDS_HDR(16, s)->len = newlen;
            }
            break;
        case SDS_TYPE_32:
            {
                SDS_HDR(32, s)->len = newlen;
            }
            break;
        case SDS_TYPE_64:
            {
                SDS_HDR(64, s)->len = newlen;
            }
            break;
    }
}

static inline void ds_increaselength(DynString_t* s, size_t inc)
{
    unsigned char newlen;
    unsigned char flags;
    unsigned char* fp;
    flags = s[-1];
    switch(flags & SDS_TYPE_MASK)
    {
        case SDS_TYPE_5:
            {
                fp = ((unsigned char*)s) - 1;
                newlen = SDS_TYPE_5_LEN(flags) + inc;
                *fp = SDS_TYPE_5 | (newlen << SDS_TYPE_BITS);
            }
            break;
        case SDS_TYPE_8:
            {
                SDS_HDR(8, s)->len += inc;
            }
            break;
        case SDS_TYPE_16:
            {
                SDS_HDR(16, s)->len += inc;
            }
            break;
        case SDS_TYPE_32:
            {
                SDS_HDR(32, s)->len += inc;
            }
            break;
        case SDS_TYPE_64:
            {
                SDS_HDR(64, s)->len += inc;
            }
            break;
    }
}

static inline size_t ds_getallocated(const DynString_t* s)
{
    unsigned char flags;
    flags = s[-1];
    switch(flags & SDS_TYPE_MASK)
    {
        case SDS_TYPE_5:
            {
                return SDS_TYPE_5_LEN(flags);
            }
            break;
        case SDS_TYPE_8:
            {
                return SDS_HDR(8, s)->alloc;
            }
            break;
        case SDS_TYPE_16:
            {
                return SDS_HDR(16, s)->alloc;
            }
            break;
        case SDS_TYPE_32:
            {
                return SDS_HDR(32, s)->alloc;
            }
            break;
        case SDS_TYPE_64:
            {
                return SDS_HDR(64, s)->alloc;
            }
            break;
    }
    return 0;
}

static inline void ds_setallocated(DynString_t* s, size_t newlen)
{
    unsigned char flags;
    flags = s[-1];
    switch(flags & SDS_TYPE_MASK)
    {
        case SDS_TYPE_5:
            {
                /* Nothing to do, this type has no total allocation info. */
            }
            break;
        case SDS_TYPE_8:
            {
                SDS_HDR(8, s)->alloc = newlen;
            }
            break;
        case SDS_TYPE_16:
            {
                SDS_HDR(16, s)->alloc = newlen;
            }
            break;
        case SDS_TYPE_32:
            {
                SDS_HDR(32, s)->alloc = newlen;
            }
            break;
        case SDS_TYPE_64:
            {
                SDS_HDR(64, s)->alloc = newlen;
            }
            break;
    }
}


static inline DynString_t* ds_newlen(const void* init, size_t initlen)
{
    int hdrlen;
    char type;
    void* sh;
    unsigned char* fp;
    DynString_t* s;
    type = ds_getreqtype(initlen);
    /* Empty strings are usually created in order to append. Use type 8
     * since type 5 is not good at this. */
    if(type == SDS_TYPE_5 && initlen == 0)
    {
        type = SDS_TYPE_8;
    }
    hdrlen = ds_getheadersize(type);
    sh = ds_wrapmalloc(hdrlen + initlen + 1);
    if(sh == NULL)
    {
        return NULL;
    }
    if(init == SDS_NOINIT)
    {
        init = NULL;
    }
    else if(!init)
    {
        memset(sh, 0, hdrlen + initlen + 1);
    }
    s = (char*)sh + hdrlen;
    fp = ((unsigned char*)s) - 1;
    switch(type)
    {
        case SDS_TYPE_5:
            {
                *fp = type | (initlen << SDS_TYPE_BITS);
            }
            break;
        case SDS_TYPE_8:
            {
                SDS_HDR_VAR(8, s);
                sh->len = initlen;
                sh->alloc = initlen;
                *fp = type;
            }
            break;
        case SDS_TYPE_16:
            {
                SDS_HDR_VAR(16, s);
                sh->len = initlen;
                sh->alloc = initlen;
                *fp = type;
            }
            break;
        case SDS_TYPE_32:
            {
                SDS_HDR_VAR(32, s);
                sh->len = initlen;
                sh->alloc = initlen;
                *fp = type;
            }
            break;
        case SDS_TYPE_64:
            {
                SDS_HDR_VAR(64, s);
                sh->len = initlen;
                sh->alloc = initlen;
                *fp = type;
            }
            break;
    }
    if(initlen && init)
    {
        memcpy(s, init, initlen);
    }
    s[initlen] = '\0';
    return s;
}

static inline DynString_t* ds_newempty(void)
{
    return ds_newlen("", 0);
}

static inline DynString_t* ds_new(const char* init)
{
    size_t initlen;
    initlen = (init == NULL) ? 0 : strlen(init);
    return ds_newlen(init, initlen);
}

static inline DynString_t* ds_duplicate(const DynString_t* s)
{
    return ds_newlen(s, ds_getlength(s));
}

static inline void ds_destroy(DynString_t* s)
{
    if(s == NULL)
    {
        return;
    }
    ds_wrapfree((char*)s - ds_getheadersize(s[-1]));
}

static inline void ds_updatelength(DynString_t* s)
{
    size_t reallen;
    reallen = strlen(s);
    ds_setlength(s, reallen);
}

static inline void ds_clear(DynString_t* s)
{
    ds_setlength(s, 0);
    s[0] = '\0';
}

static inline DynString_t* ds_makeroomfor(DynString_t* s, size_t addlen)
{
    int hdrlen;
    char type;
    char oldtype;
    size_t avail;
    size_t len;
    size_t newlen;
    void* sh;
    void* newsh;
    avail = ds_getavailable(s);
    oldtype = s[-1] & SDS_TYPE_MASK;
    /* Return ASAP if there is enough space left. */
    if(avail >= addlen)
    {
        return s;
    }
    len = ds_getlength(s);
    sh = (char*)s - ds_getheadersize(oldtype);
    newlen = (len + addlen);
    if(newlen < SDS_MAX_PREALLOC)
    {
        newlen *= 2;
    }
    else
    {
        newlen += SDS_MAX_PREALLOC;
    }
    type = ds_getreqtype(newlen);

    /* Don't use type 5: the user is appending to the string and type 5 is
     * not able to remember empty space, so ds_makeroomfor() must be called
     * at every appending operation. */
    if(type == SDS_TYPE_5)
    {
        type = SDS_TYPE_8;
    }
    hdrlen = ds_getheadersize(type);
    if(oldtype == type)
    {
        newsh = ds_wraprealloc(sh, hdrlen + newlen + 1);
        if(newsh == NULL)
        {
            return NULL;
        }
        s = (char*)newsh + hdrlen;
    }
    else
    {
        /* Since the header size changes, need to move the string forward,
         * and can't use realloc */
        newsh = ds_wrapmalloc(hdrlen + newlen + 1);
        if(newsh == NULL)
        {
            return NULL;
        }
        memcpy((char*)newsh + hdrlen, s, len + 1);
        ds_wrapfree(sh);
        s = (char*)newsh + hdrlen;
        s[-1] = type;
        ds_setlength(s, len);
    }
    ds_setallocated(s, newlen);
    return s;
}

static inline DynString_t* ds_removefreespace(DynString_t* s)
{
    int hdrlen;
    int oldhdrlen;
    size_t len;
    size_t avail;
    void* sh;
    void* newsh;
    char type;
    char oldtype;
    oldtype = s[-1] & SDS_TYPE_MASK;
    oldhdrlen = ds_getheadersize(oldtype);
    len = ds_getlength(s);
    avail = ds_getavailable(s);
    sh = (char*)s - oldhdrlen;
    /* Return ASAP if there is no space left. */
    if(avail == 0)
    {
        return s;
    }
    /* Check what would be the minimum SDS header that is just good enough to
     * fit this string. */
    type = ds_getreqtype(len);
    hdrlen = ds_getheadersize(type);

    /* If the type is the same, or at least a large enough type is still
     * required, we just realloc(), letting the allocator to do the copy
     * only if really needed. Otherwise if the change is huge, we manually
     * reallocate the string to use the different header type. */
    if(oldtype == type || type > SDS_TYPE_8)
    {
        newsh = ds_wraprealloc(sh, oldhdrlen + len + 1);
        if(newsh == NULL)
        {
            return NULL;
        }
        s = (char*)newsh + oldhdrlen;
    }
    else
    {
        newsh = ds_wrapmalloc(hdrlen + len + 1);
        if(newsh == NULL)
        {
            return NULL;
        }
        memcpy((char*)newsh + hdrlen, s, len + 1);
        ds_wrapfree(sh);
        s = (char*)newsh + hdrlen;
        s[-1] = type;
        ds_setlength(s, len);
    }
    ds_setallocated(s, len);
    return s;
}

static inline size_t ds_interngetallocsize(DynString_t* s)
{
    size_t alloc;
    alloc = ds_getallocated(s);
    return ds_getheadersize(s[-1]) + alloc + 1;
}

static inline void* ds_interngetallocptr(DynString_t* s)
{
    return (void*)(s - ds_getheadersize(s[-1]));
}

static inline void ds_internincrlength(DynString_t* s, ssize_t incr)
{
    size_t len;
    unsigned char oldlen;
    unsigned char flags;
    unsigned char* fp;
    flags = s[-1];
    switch(flags & SDS_TYPE_MASK)
    {
        case SDS_TYPE_5:
            {
                fp = ((unsigned char*)s) - 1;
                oldlen = SDS_TYPE_5_LEN(flags);
                assert((incr > 0 && oldlen + incr < 32) || (incr < 0 && oldlen >= (unsigned int)(-incr)));
                *fp = SDS_TYPE_5 | ((oldlen + incr) << SDS_TYPE_BITS);
                len = oldlen + incr;
            }
            break;
        case SDS_TYPE_8:
            {
                SDS_HDR_VAR(8, s);
                assert((incr >= 0 && sh->alloc - sh->len >= incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
                len = (sh->len += incr);
            }
            break;
        case SDS_TYPE_16:
            {
                SDS_HDR_VAR(16, s);
                assert((incr >= 0 && sh->alloc - sh->len >= incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
                len = (sh->len += incr);
            }
            break;
        case SDS_TYPE_32:
            {
                SDS_HDR_VAR(32, s);
                assert((incr >= 0 && sh->alloc - sh->len >= (unsigned int)incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
                len = (sh->len += incr);
            }
            break;
        case SDS_TYPE_64:
            {
                SDS_HDR_VAR(64, s);
                assert((incr >= 0 && sh->alloc - sh->len >= (uint64_t)incr) || (incr < 0 && sh->len >= (uint64_t)(-incr)));
                len = (sh->len += incr);
            }
            break;
        default:
            {
                len = 0; /* Just to avoid compilation warnings. */
            }
            break;
    }
    s[len] = '\0';
}

static inline DynString_t* ds_growzero(DynString_t* s, size_t len)
{
    size_t curlen;
    curlen = ds_getlength(s);
    if(len <= curlen)
    {
        return s;
    }
    s = ds_makeroomfor(s, len - curlen);
    if(s == NULL)
    {
        return NULL;
    }
    /* Make sure added region doesn't contain garbage */
    memset(s + curlen, 0, (len - curlen + 1)); /* also set trailing \0 byte */
    ds_setlength(s, len);
    return s;
}

static inline DynString_t* ds_appendlen(DynString_t* s, const void* t, size_t len)
{
    size_t curlen;
    curlen = ds_getlength(s);
    s = ds_makeroomfor(s, len);
    if(s == NULL)
    {
        return NULL;
    }
    memcpy(s + curlen, t, len);
    ds_setlength(s, curlen + len);
    s[curlen + len] = '\0';
    return s;
}

static inline DynString_t* ds_append(DynString_t* s, const char* t)
{
    return ds_appendlen(s, t, strlen(t));
}

static inline DynString_t* ds_appendchar(DynString_t* s, char ch)
{
    return ds_appendlen(s, &ch, 1);
}

static inline DynString_t* ds_appendsds(DynString_t* s, const DynString_t* t)
{
    return ds_appendlen(s, t, ds_getlength(t));
}

static inline DynString_t* ds_copylength(DynString_t* s, const char* t, size_t len)
{
    if(ds_getallocated(s) < len)
    {
        s = ds_makeroomfor(s, len - ds_getlength(s));
        if(s == NULL)
        {
            return NULL;
        }
    }
    memcpy(s, t, len);
    s[len] = '\0';
    ds_setlength(s, len);
    return s;
}

/* Like ds_copylength() but 't' must be a null-termined string so that the length
 * of the string is obtained with strlen(). */
static inline DynString_t* ds_copy(DynString_t* s, const char* t)
{
    return ds_copylength(s, t, strlen(t));
}

/*
* Helper for sdscatlonglong() doing the actual number -> string
* conversion. 's' must point to a string with room for at least
* SDS_LLSTR_SIZE bytes.
*
* The function returns the length of the null-terminated string
* representation stored at 's'.
*/
static inline int ds_ll2str(char* s, long long value)
{
    size_t l;
    unsigned long long v;
    char aux;
    char *p;
    /*
    * Generate the string representation.
    * this method produces a reversed string.
    */
    v = (value < 0) ? -value : value;
    p = s;
    do
    {
        *p++ = '0' + (v % 10);
        v /= 10;
    } while(v);
    if(value < 0)
    {
        *p++ = '-';
    }
    /* Compute length and add null term. */
    l = p - s;
    *p = '\0';
    /* Reverse the string. */
    p--;
    while(s < p)
    {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }
    return l;
}

/* Identical ds_ll2str(), but for unsigned long long type. */
static inline int ds_ull2str(char* s, unsigned long long v)
{
    size_t l;
    char aux;
    char *p;
    /* Generate the string representation, this method produces
     * an reversed string. */
    p = s;
    do
    {
        *p++ = '0' + (v % 10);
        v /= 10;
    } while(v);

    /* Compute length and add null term. */
    l = p - s;
    *p = '\0';

    /* Reverse the string. */
    p--;
    while(s < p)
    {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }
    return l;
}

static inline DynString_t* ds_fromlonglong(long long value)
{
    int len;
    char buf[SDS_LLSTR_SIZE];
    len = ds_ll2str(buf, value);
    return ds_newlen(buf, len);
}

static inline DynString_t* ds_appendvprintf(DynString_t* s, const char* fmt, va_list ap)
{
    size_t buflen;
    va_list cpy;
    char *t;
    char* buf;
    char staticbuf[1024];
    buf = staticbuf;
    buflen = strlen(fmt) * 2;
    /* We try to start using a static buffer for speed.
     * If not possible we revert to heap allocation. */
    if(buflen > sizeof(staticbuf))
    {
        buf = (char*)ds_wrapmalloc(buflen);
        if(buf == NULL)
        {
            return NULL;
        }
    }
    else
    {
        buflen = sizeof(staticbuf);
    }
    /* Try with buffers two times bigger every time we fail to
     * fit the string in the current buffer size. */
    while(true)
    {
        buf[buflen - 2] = '\0';
        va_copy(cpy, ap);
        vsnprintf(buf, buflen, fmt, cpy);
        va_end(cpy);
        if(buf[buflen - 2] != '\0')
        {
            if(buf != staticbuf)
            {
                ds_wrapfree(buf);
            }
            buflen *= 2;
            buf = (char*)ds_wrapmalloc(buflen);
            if(buf == NULL)
            {
                return NULL;
            }
            continue;
        }
        break;
    }

    /* Finally concat the obtained string to the SDS string and return it. */
    t = ds_append(s, buf);
    if(buf != staticbuf)
    {
        ds_wrapfree(buf);
    }
    return t;
}

static inline DynString_t* ds_appendprintf(DynString_t* s, const char* fmt, ...)
{
    va_list ap;
    char* t;
    va_start(ap, fmt);
    t = ds_appendvprintf(s, fmt, ap);
    va_end(ap);
    return t;
}

static inline DynString_t* ds_appendfmt(DynString_t* s, char const* fmt, ...)
{
    char next;
    size_t l;
    size_t initlen;
    long i;
    long long num;
    unsigned long long unum;
    char* str;
    const char* f;
    va_list ap;
    char buf[SDS_LLSTR_SIZE + 1];
    char singlecharbuf[5];

    initlen = ds_getlength(s);
    f = fmt;
    /* To avoid continuous reallocations, let's start with a buffer that
     * can hold at least two times the format string itself. It's not the
     * best heuristic but seems to work in practice. */
    s = ds_makeroomfor(s, initlen + strlen(fmt) * 2);
    va_start(ap, fmt);
    f = fmt; /* Next format specifier byte to process. */
    i = initlen; /* Position of the next byte to write to dest str. */
    while(*f)
    {
        /* Make sure there is always space for at least 1 char. */
        if(ds_getavailable(s) == 0)
        {
            s = ds_makeroomfor(s, 1);
        }
        switch(*f)
        {
            case '%':
                next = *(f + 1);
                f++;
                switch(next)
                {
                    case 's':
                    case 'S':
                        {
                            str = va_arg(ap, char*);
                            l = (next == 's') ? strlen(str) : ds_getlength(str);
                            if(ds_getavailable(s) < l)
                            {
                                s = ds_makeroomfor(s, l);
                            }
                            memcpy(s + i, str, l);
                            ds_increaselength(s, l);
                            i += l;
                        }
                        break;
                    case 'i':
                    case 'I':
                    case 'd':
                        {
                            if(next == 'i')
                            {
                                num = va_arg(ap, int);
                            }
                            else
                            {
                                num = va_arg(ap, long long);
                            }
                            memset(buf, 0, SDS_LLSTR_SIZE);
                            l = ds_ll2str(buf, num);
                            if(ds_getavailable(s) < l)
                            {
                                s = ds_makeroomfor(s, l);
                            }
                            memcpy(s + i, buf, l);
                            ds_increaselength(s, l);
                            i += l;
                        }
                        break;
                    case 'u':
                    case 'U':
                        {
                            if(next == 'u')
                            {
                                unum = va_arg(ap, unsigned int);
                            }
                            else
                            {
                                unum = va_arg(ap, unsigned long long);
                            }                    
                            memset(buf, 0, SDS_LLSTR_SIZE);
                            l = ds_ull2str(buf, unum);
                            if(ds_getavailable(s) < l)
                            {
                                s = ds_makeroomfor(s, l);
                            }
                            memcpy(s + i, buf, l);
                            ds_increaselength(s, l);
                            i += l;
                        }
                        break;
                    case 'c':
                        {
                            num = va_arg(ap, int);
                            singlecharbuf[0] = (char)num;
                            singlecharbuf[1] = 0;
                            if(ds_getavailable(s) < 1)
                            {
                                s = ds_makeroomfor(s, 1);
                            }
                            memcpy(s+i, singlecharbuf, 1);
                            ds_increaselength(s, 1);
                            i += 1;
                        }
                        break;
                    default: /* Handle %% and generally %<unknown>. */
                        {
                            s[i++] = next;
                            ds_increaselength(s, 1);
                        }
                        break;
                }
                break;
            default:
                {
                    s[i++] = *f;
                    ds_increaselength(s, 1);
                }
                break;
        }
        f++;
    }
    va_end(ap);
    /* Add null-term */
    s[i] = '\0';
    return s;
}

static inline DynString_t* ds_trim(DynString_t* s, const char* cset)
{
    size_t len;
    char* sp;
    char* ep;
    char* end;
    char* start;
    sp = start = s;
    ep = end = s + ds_getlength(s) - 1;
    while(sp <= end && strchr(cset, *sp))
    {
        sp++;
    }
    while(ep > sp && strchr(cset, *ep))
    {
        ep--;
    }
    len = (sp > ep) ? 0 : ((ep - sp) + 1);
    if(s != sp)
    {
        memmove(s, sp, len);
    }
    s[len] = '\0';
    ds_setlength(s, len);
    return s;
}

static inline void ds_range(DynString_t* s, ssize_t start, ssize_t end)
{
    size_t len;
    size_t newlen;
    len = ds_getlength(s);
    if(len == 0)
    {
        return;
    }
    if(start < 0)
    {
        start = len + start;
        if(start < 0)
        {
            start = 0;
        }
    }
    if(end < 0)
    {
        end = len + end;
        if(end < 0)
        {
            end = 0;
        }
    }
    newlen = (start > end) ? 0 : (end - start) + 1;
    if(newlen != 0)
    {
        if(start >= (ssize_t)len)
        {
            newlen = 0;
        }
        else if(end >= (ssize_t)len)
        {
            end = len - 1;
            newlen = (start > end) ? 0 : (end - start) + 1;
        }
    }
    else
    {
        start = 0;
    }
    if(start && newlen)
    {
        memmove(s, s + start, newlen);
    }
    s[newlen] = 0;
    ds_setlength(s, newlen);
}

static inline void ds_tolower(DynString_t* s)
{
    size_t j;
    size_t len;
    len = ds_getlength(s);
    for(j = 0; j < len; j++)
    {
        s[j] = tolower(s[j]);
    }
}

static inline void ds_toupper(DynString_t* s)
{
    size_t j;
    size_t len;
    len = ds_getlength(s);
    for(j = 0; j < len; j++)
    {
        s[j] = toupper(s[j]);
    }
}

static inline int ds_compare(const DynString_t* s1, const DynString_t* s2)
{
    int cmp;
    size_t l1;
    size_t l2;
    size_t minlen;
    l1 = ds_getlength(s1);
    l2 = ds_getlength(s2);
    minlen = (l1 < l2) ? l1 : l2;
    cmp = memcmp(s1, s2, minlen);
    if(cmp == 0)
    {
        if(l1 > l2)
        {
            return 1;
        }
        else if(l1 < l2)
        {
            return -1;
        }
        return 0;
    }
    return cmp;
}

static inline DynString_t** ds_splitlen(const char* s, ssize_t len, const char* sep, int seplen, size_t* count)
{
    size_t i;
    size_t elements;
    size_t slots;
    long j;
    long start;
    DynString_t** tokens;
    DynString_t** newtokens;
    elements = 0;
    slots = 5;
    start = 0;
    if(seplen < 1 || len < 0)
    {
        return NULL;
    }
    tokens = (DynString_t**)ds_wrapmalloc(sizeof(DynString_t*) * slots);
    if(tokens == NULL)
    {
        return NULL;
    }
    if(len == 0)
    {
        *count = 0;
        return tokens;
    }
    for(j = 0; j < (len - (seplen - 1)); j++)
    {
        /* make sure there is room for the next element and the final one */
        if(slots < elements + 2)
        {
            slots *= 2;
            newtokens = (DynString_t**)ds_wraprealloc(tokens, sizeof(DynString_t*) * slots);
            if(newtokens == NULL)
            {
                goto cleanup;
            }
            tokens = newtokens;
        }
        /* search the separator */
        if((seplen == 1 && *(s + j) == sep[0]) || (memcmp(s + j, sep, seplen) == 0))
        {
            tokens[elements] = ds_newlen(s + start, j - start);
            if(tokens[elements] == NULL)
            {
                goto cleanup;
            }
            elements++;
            start = j + seplen;
            j = j + seplen - 1; /* skip the separator */
        }
    }
    /* Add the final element. We are sure there is room in the tokens array. */
    tokens[elements] = ds_newlen(s + start, len - start);
    if(tokens[elements] == NULL)
    {
        goto cleanup;
    }
    elements++;
    *count = elements;
    return tokens;

    cleanup:
    {
        for(i = 0; i < elements; i++)
        {
            ds_destroy(tokens[i]);
        }
        ds_wrapfree(tokens);
        *count = 0;
        return NULL;
    }
}

static inline void ds_freesplitres(DynString_t** tokens, size_t count)
{
    if(!tokens)
    {
        return;
    }
    while(count--)
    {
        ds_destroy(tokens[count]);
    }
    ds_wrapfree(tokens);
}

static inline DynString_t* ds_appendrepr(DynString_t* s, const char* p, size_t len, bool withquotes)
{
    if(withquotes)
    {
        s = ds_appendlen(s, "\"", 1);
    }
    while(len--)
    {
        switch(*p)
        {
            case '\\':
            case '"':
                {
                    s = ds_appendprintf(s, "\\%c", *p);
                }
                break;
            case '\n':
                {
                    s = ds_appendlen(s, "\\n", 2);
                }
                break;
            case '\r':
                {
                    s = ds_appendlen(s, "\\r", 2);
                }
                break;
            case '\t':
                {
                    s = ds_appendlen(s, "\\t", 2);
                }
                break;
            case '\a':
                {
                    s = ds_appendlen(s, "\\a", 2);
                }
                break;
            case '\b':
                {
                    s = ds_appendlen(s, "\\b", 2);
                }
                break;
            default:
                {
                    if(isprint(*p))
                    {
                        s = ds_appendprintf(s, "%c", *p);
                    }
                    else
                    {
                        s = ds_appendprintf(s, "\\x%02x", (unsigned char)*p);
                    }
                }
                break;
        }
        p++;
    }
    if(withquotes)
    {
        return ds_appendlen(s, "\"", 1);
    }
    return s;
}


static inline int sdsutil_ishex(char c)
{
    return (
        ((c >= '0') && (c <= '9')) ||
        ((c >= 'a') && (c <= 'f')) ||
        ((c >= 'A') && (c <= 'F'))
    );
}

static inline int sdsutil_hextoint(char c)
{
    switch(c)
    {
        case '0':
            return 0;
        case '1':
            return 1;
        case '2':
            return 2;
        case '3':
            return 3;
        case '4':
            return 4;
        case '5':
            return 5;
        case '6':
            return 6;
        case '7':
            return 7;
        case '8':
            return 8;
        case '9':
            return 9;
        case 'a':
        case 'A':
            return 10;
        case 'b':
        case 'B':
            return 11;
        case 'c':
        case 'C':
            return 12;
        case 'd':
        case 'D':
            return 13;
        case 'e':
        case 'E':
            return 14;
        case 'f':
        case 'F':
            return 15;
        default:
            {
            }
            break;
    }
    return 0;

}


static inline DynString_t** ds_splitargs(const char* line, int* argc)
{
    int inq;
    int insq;
    int done;
    char c;
    unsigned char byte;
    char* current;
    char** vector;
    const char* p;
    p = line;
    current = NULL;
    vector = NULL;
    *argc = 0;
    while(1)
    {
        /* skip blanks */
        while(*p && isspace(*p))
        {
            p++;
        }
        if(*p)
        {
            /* get a token */
            inq = 0; /* set to 1 if we are in "quotes" */
            insq = 0; /* set to 1 if we are in 'single quotes' */
            done = 0;
            if(current == NULL)
            {
                current = ds_newempty();
            }
            while(!done)
            {
                if(inq)
                {
                    if((*p == '\\') && (*(p + 1) == 'x') && sdsutil_ishex(*(p + 2)) && sdsutil_ishex(*(p + 3)))
                    {
                        byte = (sdsutil_hextoint(*(p + 2)) * 16) + sdsutil_hextoint(*(p + 3));
                        current = ds_appendlen(current, (char*)&byte, 1);
                        p += 3;
                    }
                    else if((*p == '\\') && *(p + 1))
                    {
                        p++;
                        switch(*p)
                        {
                            case 'n':
                                {
                                    c = '\n';
                                }
                                break;
                            case 'r':
                                {
                                    c = '\r';
                                }
                                break;
                            case 't':
                                {
                                    c = '\t';
                                }
                                break;
                            case 'b':
                                {
                                    c = '\b';
                                }
                                break;
                            case 'a':
                                {
                                    c = '\a';
                                }
                                break;
                            default:
                                {
                                    c = *p;
                                }
                                break;
                        }
                        current = ds_appendlen(current, &c, 1);
                    }
                    else if(*p == '"')
                    {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if(*(p + 1) && !isspace(*(p + 1)))
                        {
                            goto err;
                        }
                        done = 1;
                    }
                    else if(!*p)
                    {
                        /* unterminated quotes */
                        goto err;
                    }
                    else
                    {
                        current = ds_appendlen(current, p, 1);
                    }
                }
                else if(insq)
                {
                    if(*p == '\\' && *(p + 1) == '\'')
                    {
                        p++;
                        current = ds_appendlen(current, "'", 1);
                    }
                    else if(*p == '\'')
                    {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if(*(p + 1) && !isspace(*(p + 1)))
                        {
                            goto err;
                        }
                        done = 1;
                    }
                    else if(!*p)
                    {
                        /* unterminated quotes */
                        goto err;
                    }
                    else
                    {
                        current = ds_appendlen(current, p, 1);
                    }
                }
                else
                {
                    switch(*p)
                    {
                        case ' ':
                        case '\n':
                        case '\r':
                        case '\t':
                        case '\0':
                            {
                                done = 1;
                            }
                            break;
                        case '"':
                            {
                                inq = 1;
                            }
                            break;
                        case '\'':
                            {
                                insq = 1;
                            }
                            break;
                        default:
                            {
                                current = ds_appendlen(current, p, 1);
                            }
                            break;
                    }
                }
                if(*p)
                {
                    p++;
                }
            }
            /* add the token to the vector */
            vector = (char**)ds_wraprealloc(vector, ((*argc) + 1) * sizeof(char*));
            vector[*argc] = current;
            (*argc)++;
            current = NULL;
        }
        else
        {
            /* Even on empty input string return something not NULL. */
            if(vector == NULL)
            {
                vector = (char**)ds_wrapmalloc(sizeof(void*));
            }
            return vector;
        }
    }

err:
    while((*argc)--)
    {
        ds_destroy(vector[*argc]);
    }
    ds_wrapfree(vector);
    if(current)
    {
        ds_destroy(current);
    }
    *argc = 0;
    return NULL;
}


static inline DynString_t* ds_mapchars(DynString_t* s, const char* from, const char* to, size_t setlen)
{
    size_t j;
    size_t i;
    size_t l;
    l = ds_getlength(s);
    for(j = 0; j < l; j++)
    {
        for(i = 0; i < setlen; i++)
        {
            if(s[j] == from[i])
            {
                s[j] = to[i];
                break;
            }
        }
    }
    return s;
}

static inline DynString_t* ds_join(char** argv, int argc, char* sep)
{
    int j;
    DynString_t* join;
    join = ds_newempty();
    for(j = 0; j < argc; j++)
    {
        join = ds_append(join, argv[j]);
        if(j != argc - 1)
        {
            join = ds_append(join, sep);
        }
    }
    return join;
}

/* Like ds_join, but joins an array of SDS strings. */
static inline DynString_t* ds_joinsds(DynString_t** argv, int argc, const char* sep, size_t seplen)
{
    DynString_t* join = ds_newempty();
    int j;

    for(j = 0; j < argc; j++)
    {
        join = ds_appendsds(join, argv[j]);
        if(j != argc - 1)
        {
            join = ds_appendlen(join, sep, seplen);
        }
    }
    return join;
}


static inline void* ds_malloc(size_t size)
{
    return ds_wrapmalloc(size);
}

static inline void* ds_realloc(void* ptr, size_t size)
{
    return ds_wraprealloc(ptr, size);
}

static inline void ds_free(void* ptr)
{
    ds_wrapfree(ptr);
}

