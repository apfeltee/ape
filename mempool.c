
/*
* this file contains the memory pool logic.
* like memgc.c, do not touch it unless you know what you're doing.
*/

/* 
* Copyright (c) 2011 Scott Vokes <vokes.s@gmail.com>
*  
* Permission to use, copy, modify, and/or distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*  
* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
* WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
* ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
* ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
* OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/*
* A memory pool allocator, designed for systems that need to
* allocate/free pointers in amortized O(1) time. Memory is allocated a
* page at a time, then added to a set of pools of equally sized
* regions. A free list for each size is maintained in the unused
* regions. When a pointer is repooled, it is linked back into the
* pool with the given size's free list.
*
* Note that repooling with the wrong size leads to subtle/ugly memory
* clobbering bugs. Turning on memory use logging via MPOOL_DEBUG
* can help pin down the location of most such errors.
* 
* Allocations larger than the page size are allocated whole via
* mmap, and those larger than mp->maxpoolsize (configurable) are
* freed immediately via munmap; no free list is used.
*/


#if defined(__linux__) || defined(__unix__) || defined(__CYGWIN__)
    #define APE_MEMPOOL_ISLINUX
    #define APE_MEMPOOL_HAVEMMAP
#endif

#if defined(APE_MEMPOOL_HAVEMMAP) || defined(APE_MEMPOOL_HAVEVIRTALLOC)
    //#define APE_MEMPOOL_ISAVAILABLE
#endif

#if defined(APE_MEMPOOL_HAVEMMAP)
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/mman.h>
#endif

#include "inline.h"

/* these should point to plain stdlib mem functions */
#define MPOOL_MALLOC(sz) malloc(sz)
#define MPOOL_REALLOC(p, sz) realloc(p, sz)
#define MPOOL_FREE(p, sz) free(p)

static void* wrap_mmap(long sz)
{
    (void)sz;
    #if defined(APE_MEMPOOL_HAVEMMAP)
        void* p;
        p = mmap(0, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if(p == MAP_FAILED)
        {
            return NULL;
        }
        return p;
    #else
        return NULL;
    #endif
}

int wrap_munmap(void* addr, size_t length)
{
    (void)addr;
    (void)length;
    #if defined(APE_MEMPOOL_HAVEMMAP)
        return munmap(addr, length);
    #endif
    return -1;
}

/* Optimized base-2 integer ceiling, from _Hacker's Delight_
 * by Henry S. Warren, pg. 48. Called 'clp2' there. */
static unsigned int iceil2(unsigned int x)
{
    x = x - 1;
    x = x | (x >> 1);
    x = x | (x >> 2);
    x = x | (x >> 4);
    x = x | (x >> 8);
    x = x | (x >> 16);
    return x + 1;
}

void ape_mempool_debugprintv(ApeMemPool_t* mp, const char* fmt, va_list va)
{
    if(APE_UNLIKELY(mp->enabledebug))
    {
        vfprintf(mp->debughandle, fmt, va);
        fflush(mp->debughandle);
    }
}

void ape_mempool_debugprintf(ApeMemPool_t* mp, const char* fmt, ...)
{
    va_list va;
    if(APE_UNLIKELY(mp->enabledebug))
    {
        va_start(va, fmt);
        ape_mempool_debugprintv(mp, fmt, va);
        va_end(va);
    }
}

/* mmap a new memory pool of TOTAL_SZ bytes, then build an internal
 * freelist of SZ-byte cells, with the head at (result)[0].
 * Returns NULL on error. */
void** ape_mempool_newpool(ApeMemPool_t* mp, ApeSize_t sz, ApeSize_t total_sz)
{
    void* p;
    int i;
    /* o=offset */
    int o;
    int lim;
    int** pool;
    void* last;
    last = NULL;
    p = wrap_mmap(sz > total_sz ? sz : total_sz);
    o = 0;
    if(p == NULL)
    {
        return NULL;
    }
    pool = (int**)p;
    assert(pool);
    //assert(sz > sizeof(void *));
    lim = (total_sz / sz);
    if(APE_UNLIKELY(mp->enabledebug))
    {
        ape_mempool_debugprintf(mp, "mempool_newpool sz: %zu lim: %d => %lu %p\n", sz, lim, lim * sz, p);
    }
    for(i = 0; i < lim; i++)
    {
        if(last)
        {
            assert(last == pool[o]);
        }
        o = (i * sz) / sizeof(void*);
        pool[o] = (int*)&pool[o + (sz / sizeof(void*))];
        last = pool[o];
        if(APE_UNLIKELY(mp->enabledebug))
        {
            ape_mempool_debugprintf(mp, "  %d (%d / 0x%04x) -> %p = %p\n", i, o, o, &pool[o], pool[o]);
        }
    }
    pool[o] = NULL;
    return (void**)p;
}

/* Add a new pool, resizing the pool array if necessary. */
static int add_pool(ApeMemPool_t* mp, void* p, int sz)
{
    /* new pools*/
    void** nps;
    /* new psizes */
    void* nsizes;
    assert(p);
    assert(sz > 0);
    if(APE_UNLIKELY(mp->enabledebug))
    {
        ape_mempool_debugprintf(mp, "mpool_add_pool (%zu / %zu) @ %p, sz %d\n", mp->poolcount, mp->poolarrlength, p, sz);
    }
    if(mp->poolcount == mp->poolarrlength)
    {
        mp->poolarrlength *= 2; /* ram will exhaust before overflow... */
        nps = (void**)MPOOL_REALLOC(mp->pools, mp->poolarrlength * sizeof(void**));
        nsizes = MPOOL_REALLOC(mp->psizes, mp->poolarrlength * sizeof(int*));
        if(nps == NULL || nsizes == NULL)
        {
            return -1;
        }
        mp->psizes = (int*)nsizes;
        mp->pools = nps;
    }
    mp->pools[mp->poolcount] = p;
    mp->psizes[mp->poolcount] = sz;
    mp->poolcount++;
    return 0;
}

/* Initialize a memory pool set, with pools in sizes
 * 2^min2 to 2^max2. Returns NULL on error. */
ApeMemPool_t* ape_mempool_initdebughandle(int min2, int max2, FILE* hnd, bool mustclose)
{
    /* length of pool array */
    int palen;
    /* pool array count */
    int ct;
    long pgsz;
    ApeMemPool_t* mp;
    void* pools;
    int* sizes;
    ct = max2 - min2 + 1;
    pgsz = 1024;
    #if defined(APE_MEMPOOL_ISLINUX)
        pgsz = sysconf(_SC_PAGESIZE);
    #endif

    if(ct < 1)
    {
        ct = min2;
    }
    palen = iceil2(ct);
    //assert(ct > 0);
    mp = (ApeMemPool_t*)MPOOL_MALLOC(sizeof(ApeMemPool_t) + (ct - 1) * sizeof(void*));
    pools = MPOOL_MALLOC(palen * sizeof(void**));
    sizes = (int*)MPOOL_MALLOC(palen * sizeof(int));
    if(mp == NULL || pools == NULL || sizes == NULL)
    {
        return NULL;
    }
    #if defined(APE_MEMPOOL_ISAVAILABLE)
        mp->available = true;
    #else
        mp->available = false;
        fprintf(stderr, "mmap not available, doing malloc/free directly\n");
    #endif
    mp->enabledebug = (hnd != NULL);
    mp->debughandle = hnd;
    mp->debughndmustclose = mustclose;
    mp->totalalloccount = 0;
    mp->totalmapped = 0;
    mp->totalbytes = 0;
    mp->poolcount = ct;
    mp->pools = (void**)pools;
    mp->poolarrlength = palen;
    mp->pgsize = pgsz;
    mp->psizes = sizes;
    if(APE_UNLIKELY(mp->enabledebug))
    {
        ape_mempool_debugprintf(mp, "mempool_init for cells %d - %d bytes\n", 1 << min2, 1 << max2);
    }
    mp->minpoolsize = 1 << min2;
    mp->maxpoolsize = 1 << max2;
    memset(sizes, 0, palen * sizeof(int));
    memset(pools, 0, palen * sizeof(void*));
    memset(mp->heads, 0, ct * sizeof(void*));
    return mp;
}

ApeMemPool_t* ape_mempool_init(ApeSize_t min2, ApeSize_t max2)
{
    return ape_mempool_initdebughandle(min2, max2, NULL, false);
}

/* Free a memory pool set. */
void ape_mempool_destroy(ApeMemPool_t* mp)
{
    ApeSize_t i;
    ApeSize_t sz;
    ApeSize_t pgsz;
    void* p;
    pgsz = mp->pgsize;
    assert(mp);
    if(APE_UNLIKELY(mp->enabledebug))
    {
        ape_mempool_debugprintf(mp, "%zu/%zu pools, freeing...\n", mp->poolcount, mp->poolarrlength);
        ape_mempool_debugprintf(mp, "statistics: %zu allocations (%zu of which mapped), %zu bytes total\n", 
            mp->totalalloccount,
            mp->totalmapped,
            mp->totalbytes
        );
    }
    for(i = 0; i < mp->poolcount; i++)
    {
        p = mp->pools[i];
        if(p)
        {
            sz = mp->psizes[i];
            assert(sz > 0);
            sz = sz >= pgsz ? sz : pgsz;
            if(APE_UNLIKELY(mp->enabledebug))
            {
                ape_mempool_debugprintf(mp, "mempool_destroy %ld, sz %ld (%p)\n", i, sz, mp->pools[i]);
            }
            if(mp->available)
            {
                if(wrap_munmap(mp->pools[i], sz) == -1)
                {
                    ape_mempool_debugprintf(mp, "munmap error while unmapping %lu bytes at %p\n", sz, mp->pools[i]);
                }
            }
        }
    }
    MPOOL_FREE(mp->pools, mp->poolcount * sizeof(*ps));
    MPOOL_FREE(mp->psizes, sizeof(int*));
    if(mp->debughandle != NULL)
    {
        if(mp->debughndmustclose)
        {
            fclose(mp->debughandle);
        }
    }
    MPOOL_FREE(mp, sizeof(*mp));
}

bool ape_mempool_setdebughandle(ApeMemPool_t* mp, FILE* handle, bool mustclose)
{
    if(handle == NULL)
    {
        mp->enabledebug = false;
        return false;
    }
    mp->enabledebug = true;
    mp->debughandle = handle;
    mp->debughndmustclose = mustclose;
    return true;
}

bool ape_mempool_setdebugfile(ApeMemPool_t* mp, const char* path)
{
    FILE* hnd;
    hnd = fopen(path, "wb");
    if(!hnd)
    {
        return false;
    }
    return ape_mempool_setdebughandle(mp, hnd, true);
}

/* Allocate memory out of the relevant memory pool.
 * If larger than maxpoolsize, just mmap it. If pool is full, mmap a new one and
 * link it to the end of the current one. Returns NULL on error. */
void* ape_mempool_alloc(ApeMemPool_t* mp, ApeSize_t sz)
{
    ApeInt_t i;
    ApeSize_t p;
    ApeInt_t szceil;
    /* new pool */
    void **cur;
    void **np;
    void** pool;
    mp->totalalloccount++;
    mp->totalbytes += sz;
    if(!mp->available)
    {
        if(APE_UNLIKELY(mp->enabledebug))
        {
            ape_mempool_debugprintf(mp, "fallback: alloc %zu bytes\n", sz);
        }
        return MPOOL_MALLOC(sz);
    }
    szceil = 0;
    assert(mp);
    if(sz >= mp->maxpoolsize)
    {
        cur = (void**)wrap_mmap(sz); /* just mmap it */
        if(cur == NULL)
        {
            return NULL;
        }
        if(APE_UNLIKELY(mp->enabledebug))
        {
            ape_mempool_debugprintf(mp, "mempool_alloc mmap %zu bytes @ %p\n", sz, cur);
        }
        mp->totalmapped++;
        return cur;
    }
    for(i = 0, p = mp->minpoolsize;; i++, p *= 2)
    {
        if(p > sz)
        {
            szceil = p;
            break;
        }
    }
    assert(szceil > 0);
    cur = (void**)mp->heads[i]; /* get current head */
    if(cur == NULL)
    {
        /* lazily allocate & init pool */
        pool = ape_mempool_newpool(mp, szceil, mp->pgsize);
        if(pool == NULL)
        {
            return NULL;
        }
        fprintf(stderr, "mp->pools=%p i=%ld pool=%p\n", mp->pools[i], i, pool[0]);
        mp->pools[i] = pool;
        mp->heads[i] = &pool[0];
        mp->psizes[i] = szceil;
        cur = (void**)mp->heads[i];
    }
    assert(cur);
    if(*cur == NULL)
    {
        /* if at end, attach to a new page */
        if(APE_UNLIKELY(mp->enabledebug))
        {
            ape_mempool_debugprintf(mp, "mempool_alloc adding pool w/ cell size %ld\n", szceil);
        }
        np = ape_mempool_newpool(mp, szceil, mp->pgsize);
        if(np == NULL)
        {
            return NULL;
        }
        *cur = &np[0];
        assert(*cur);
        if(add_pool(mp, np, szceil) < 0)
        {
            return NULL;
        }
    }
    assert(*cur > (void*)4096);
    if(APE_UNLIKELY(mp->enabledebug))
    {
        ape_mempool_debugprintf(mp, "mempool_alloc pool %zu bytes @ %p (list %ld, szceil %ld )\n", sz, (void*)cur, i, szceil);
    }
    mp->heads[i] = *cur; /* set head to next head */
    return cur;
}

void ape_mempool_free(ApeMemPool_t* mp, void* p)
{
    if(!mp->available)
    {
        if(APE_UNLIKELY(mp->enabledebug))
        {
            ape_mempool_debugprintf(mp, "fallback: freeing %p\n", p);
        }
        MPOOL_FREE(p, 0);
    }
}

/* Push an individual pointer P back on the freelist for
 * the pool with size SZ cells.
 * if SZ is > the max pool size, just munmap it. */
void ape_mempool_repool(ApeMemPool_t* mp, void* p, ApeSize_t sz)
{
    ApeInt_t i;
    ApeSize_t szceil;
    ApeSize_t max_pool;
    void** ip;
    if(!mp->available)
    {
        return;
    }
    i = 0;
    max_pool = mp->maxpoolsize;
    if(sz > max_pool)
    {
        if(APE_UNLIKELY(mp->enabledebug))
        {
            ape_mempool_debugprintf(mp, "mempool_repool munmap sz %zu @ %p\n", sz, p);
        }
        if(mp->available)
        {
            if(wrap_munmap(p, sz) == -1)
            {
                fprintf(stderr, "munmap error while unmapping %zu bytes at %p\n", sz, p);
            }
        }
        return;
    }
    szceil = iceil2(sz);
    szceil = szceil > mp->minpoolsize ? szceil : mp->minpoolsize;
    ip = (void**)p;
    *ip = mp->heads[i];
    assert(ip);
    mp->heads[i] = ip;
    if(APE_UNLIKELY(mp->enabledebug))
    {
        ape_mempool_debugprintf(mp, "mempool_repool list %ld, %zu bytes (ceil %ld): %p\n", i, sz, szceil, ip);
    }
}

/* Reallocate data, growing or shrinking and copying the contents.
 * Returns NULL on reallocation error. */
void* ape_mempool_realloc(ApeMemPool_t* mp, void* p, ApeSize_t old_sz, ApeSize_t new_sz)
{
    void* r;
    mp->totalalloccount++;
    mp->totalbytes += new_sz;
    if(!mp->available)
    {
        if(APE_UNLIKELY(mp->enabledebug))
        {
            ape_mempool_debugprintf(mp, "fallback: realloc %p from %zu to %zu bytes\n", p, old_sz, new_sz);
        }
        return MPOOL_REALLOC(p, new_sz);
    }
    r = ape_mempool_alloc(mp, new_sz);
    if(r == NULL)
    {
        return NULL;
    }
    #if 0
        memcpy(r, p, old_sz);
    #else
        memcpy(r, p, new_sz);
    #endif
    ape_mempool_repool(mp, p, old_sz);
    return r;
}
