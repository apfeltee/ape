
#include "inline.h"

#define APE_CONF_ARRAY_INITIAL_CAPACITY (64/4)

#define VALARRAY_USE_DNARRAY 0

#define NEW_ALLOC_SCHEME 0
 
#define DEBUG 0



static ApeSize_t g_callcount = 0;
static ApeSize_t g_arrayident = 0;

struct ApeValArray_t
{
    ApeSize_t ident;
    ApeContext_t* context;
    ApeSize_t elemsize;

    #if defined(VALARRAY_USE_DNARRAY) && (VALARRAY_USE_DNARRAY == 1)
        DequeList_t* arraydata;
    #else
        unsigned char* start;
        unsigned char* arraydata;
        unsigned char* allocdata;
        ApeSize_t count;
        ApeSize_t capacity;
        bool lock_capacity;
    #endif
};

struct ApePtrArray_t
{
    ApeContext_t* context;
    ApeValArray_t* arr;
};

#if defined(__GNUC__)
static inline void debugmsg(void* self, const char* name, const char* fmt, ...)
 __attribute__((format(printf, 3, 4)));
#endif

//debugmsg(arr, "ape_valarray_push", "ix=%d p=%p", ix, value);
static inline void debugmsgv(ApeValArray_t* self, const char* name, const char* fmt, va_list va)
{
    ApeSize_t cap;
    ApeSize_t cnt;
    ApeSize_t elemsz;
    #if defined(VALARRAY_USE_DNARRAY) && (VALARRAY_USE_DNARRAY == 1)
        elemsz = self->arraydata->elemsize;
        cap = self->arraydata->capacity;
        cnt = self->arraydata->length;
    #else
        elemsz = self->elemsize;
        cap = self->capacity;
        cnt = self->count;
    #endif
    fprintf(stderr, "--%04ld: in <%ld:elemsize=%ld count=%ld capacity=%ld> %s:", g_callcount, self->ident, elemsz, cnt, cap, name);
    vfprintf(stderr, fmt, va);
    fprintf(stderr, "\n");
    g_callcount++;
}

static inline void debugmsg(void* self, const char* name, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    debugmsgv(self, name, fmt, va);
    va_end(va);
}

ApeValArray_t* ape_make_valarray(ApeContext_t* ctx, ApeSize_t elsz)
{
    return ape_make_valarraycapacity(ctx, APE_CONF_ARRAY_INITIAL_CAPACITY, elsz);
}

ApeValArray_t* ape_make_valarraycapacity(ApeContext_t* ctx, ApeSize_t capacity, ApeSize_t elsz)
{
    bool ok;
    ApeValArray_t* arr;
    arr = (ApeValArray_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeValArray_t));
    if(!arr)
    {
        return NULL;
    }
    arr->ident = g_arrayident;
    arr->context = ctx;
    arr->elemsize = elsz;
    arr->start = 0;
    g_arrayident++;
    ok = ape_valarray_initcapacity(arr, ctx, capacity);
    if(!ok)
    {
        ape_allocator_free(&ctx->alloc, arr);
        return NULL;
    }
    return arr;
}

bool ape_valarray_initcapacity(ApeValArray_t* arr, ApeContext_t* ctx, ApeSize_t capacity)
{
    arr->context = ctx;
    if(capacity == 0)
    {
        capacity = arr->elemsize * 1;
    }
    #if defined(VALARRAY_USE_DNARRAY) && (VALARRAY_USE_DNARRAY == 1)
        arr->arraydata = deqlist_create(arr->elemsize, capacity);
    #else
        if(capacity > 0)
        {
            arr->allocdata = (unsigned char*)ape_allocator_alloc(&ctx->alloc, capacity * arr->elemsize);
            memset(arr->allocdata, 0, capacity * arr->elemsize);
            arr->arraydata = arr->allocdata;
            if(!arr->allocdata)
            {
                return false;
            }
        }
        else
        {
            arr->allocdata = NULL;
            arr->arraydata = NULL;
        }
        arr->capacity = capacity;
        arr->count = 0;
        arr->lock_capacity = false;
    #endif
    #if defined(DEBUG) && (DEBUG == 1)
        debugmsg(arr, "ape_valarray_initcapacity", "capacity=%ld elsz=%ld", capacity, arr->elemsize);
    #endif

    return true;
}

void ape_valarray_deinit(ApeValArray_t* arr)
{
    ApeContext_t* ctx;
    (void)ctx;
    if(arr)
    {
        ctx = arr->context;
        #if defined(VALARRAY_USE_DNARRAY) && (VALARRAY_USE_DNARRAY == 1)
            deqlist_destroy(arr->arraydata);
        #else
            if(arr->allocdata != NULL)
            {
                ape_allocator_free(&ctx->alloc, arr->allocdata);
                arr->allocdata = NULL;
            }
        #endif
    }
}

void ape_valarray_destroy(ApeValArray_t* arr)
{
    ApeContext_t* ctx;
    if(!arr)
    {
        return;
    }
    ctx = arr->context;
    ape_valarray_deinit(arr);
    ape_allocator_free(&ctx->alloc, arr);
}

ApeSize_t ape_valarray_count(ApeValArray_t* arr)
{
    if(arr == NULL)
    {
        return 0;
    }
    #if defined(VALARRAY_USE_DNARRAY) && (VALARRAY_USE_DNARRAY == 1)
        if(arr->arraydata == NULL)
        {
            return 0;
        }
        return deqlist_count(arr->arraydata);
    #else
        return arr->count;
    #endif
}

ApeSize_t ape_valarray_capacity(ApeValArray_t* arr)
{
    if(!arr)
    {
        return 0;
    }
    #if defined(VALARRAY_USE_DNARRAY) && (VALARRAY_USE_DNARRAY == 1)
        return arr->arraydata->capacity;
    #else
        return arr->capacity;
    #endif
}

ApeSize_t ape_valarray_size(ApeValArray_t* arr)
{
    return ape_valarray_count(arr);
}

bool ape_valarray_canappend(ApeValArray_t* arr)
{
    #if defined(VALARRAY_USE_DNARRAY) && (VALARRAY_USE_DNARRAY == 1)
        (void)arr;
    #else
        return false;
        ApeSize_t cap;
        ApeSize_t acnt;
        ApeSize_t elmsz;
        ApeSize_t hereidx;
        ApeSize_t needed;
        elmsz = arr->elemsize;
        cap = arr->capacity;
        acnt = arr->count;
        hereidx = (acnt == 0) ? 0 : (acnt - 1);
        hereidx = (hereidx == 0) ? 1 : hereidx;
        if(cap > 0)
        {
            needed = ((hereidx) * (elmsz * 2));
            if(needed <= cap)
            {
                return true;
            }
        }
    #endif
    return false;
}

bool ape_valarray_push(ApeValArray_t* arr, void* value)
{
    ApeContext_t* ctx;
    ApeSize_t cnt;
    ctx = arr->context;
    cnt = ape_valarray_count(arr);
    if(cnt > 0)
    {
        cnt = cnt - 1;
    }
    #if defined(DEBUG) && (DEBUG == 1)
        debugmsg(arr, "ape_valarray_push", "ix=%ld p=%p", cnt, value);
    #endif
    #if defined(VALARRAY_USE_DNARRAY) && (VALARRAY_USE_DNARRAY == 1)
        #if 1
            deqlist_push(&arr->arraydata, *(void**)value);
            deqlist_set(arr->arraydata, cnt, *(void**)value);
        #else
            deqlist_push(&arr->arraydata, value);
            deqlist_set(arr->arraydata, cnt, value);
        #endif
    #else
        bool ok;
        ApeSize_t initcap;
        ApeSize_t oldcap;
        ApeSize_t newcap;
        ApeSize_t toalloc;
        ApeSize_t elmsz;
        ApeSize_t acnt;
        ApeSize_t offset;
        (void)initcap;
        unsigned char* newdata;
        elmsz = arr->elemsize;
        oldcap = arr->capacity;
        acnt = arr->count;
        newcap = 0;
        initcap = oldcap;
        #if 1
            if(ape_valarray_canappend(arr))
            {
                ok = true;
                ok = ape_valarray_set(arr, acnt-1, value);
                arr->count++;
                return ok;
            }
        #endif

        #if defined(NEW_ALLOC_SCHEME) && (NEW_ALLOC_SCHEME == 1)
            ApeSize_t items = arr->count;
            newcap = arr->capacity;
            if(newcap < 16)
            {
                /* Allocate new arr twice as much as current. */
                newcap = newcap * 2;
            }
            else
            {
                /* Allocate new arr half as much as current. */
                newcap += newcap / 2;
            }
            if(newcap < items)
            {
                newcap = items;
            }
            toalloc = newcap * arr->elemsize;
        #else
            if(oldcap > 0)
            {
                newcap = (oldcap * 2);            
            }
            else
            {
                newcap = elmsz+1;
            }
            toalloc = (newcap * elmsz);
        #endif




        bool isabove;

            isabove = (acnt >= oldcap);
            //isabove = (toalloc > (oldcap * elmsz));
            //isabove = ((newcap * elmsz) > (oldcap * elmsz));
            //isabove = (((acnt + 1) * newcap) > (oldcap * elmsz));
            //isabove = ((newcap) >= ((oldcap * elmsz) + elmsz));
    

        //fprintf(stderr, "valarray_push:isabove: newcap=%ld oldcap=%ld elmsz=%ld isabove=%d\n", newcap, oldcap, elmsz, isabove);



        if(isabove)
        {
            APE_ASSERT(!arr->lock_capacity);
            if(arr->lock_capacity)
            {
                return false;
            }
            newdata = (unsigned char*)ape_allocator_alloc(&ctx->alloc, toalloc+1);
            memset(newdata, 0, toalloc+1);
            if(!newdata)
            {
                return false;
            }
            if(arr->arraydata == NULL)
            {
                arr->arraydata = newdata;
            }
            else
            {
                memmove(newdata, arr->arraydata, acnt*elmsz);
            }
            ape_allocator_free(&ctx->alloc, arr->allocdata);
            arr->allocdata = newdata;
            arr->arraydata = arr->allocdata;
            arr->capacity = newcap;
        }
        arr->count++;
        if(value)
        {
            ape_valarray_set(arr, arr->count-1, value);
        }
    #endif


    return true;
}

void* ape_valarray_get(ApeValArray_t* arr, ApeSize_t ix)
{
    void* raw;
    void* tmp;
    void* rtp;
    if(ix >= ape_valarray_count(arr))
    {
        APE_ASSERT(false);
        return NULL;
    }
    #if defined(VALARRAY_USE_DNARRAY) && (VALARRAY_USE_DNARRAY == 1)
        raw = deqlist_get(arr->arraydata, ix);
        if(raw != NULL)
        {
            #if 0
                tmp = *(void**)raw;
            #else
                tmp = raw;
            #endif
        }
        else
        {
            tmp = raw;
        }
        rtp = tmp;
    #else
        ApeSize_t offset;
        offset = ix * arr->elemsize;
        rtp =  arr->arraydata + offset;
    #endif
    #if defined(DEBUG) && (DEBUG == 1)
        debugmsg(arr, "ape_valarray_get", "ix=%ld rtp=%p", ix, rtp);
    #endif
    return rtp;
}

bool ape_valarray_set(ApeValArray_t* arr, ApeSize_t ix, void* value)
{
    #if defined(DEBUG) && (DEBUG == 1)
        debugmsg(arr, "ape_valarray_set", "ix=%ld p=%p", ix, value);
    #endif
    #if defined(VALARRAY_USE_DNARRAY) && (VALARRAY_USE_DNARRAY == 1)
        #if 0
            deqlist_set(arr->arraydata, ix, *(void**)value);
        #else
            deqlist_set(arr->arraydata, ix, value);            
        #endif
    #else
        ApeSize_t cap;
        ApeSize_t cnt;
        ApeSize_t elmsz;
        ApeSize_t offset;
        cap = arr->capacity;
        elmsz = arr->elemsize;
        cnt = ape_valarray_count(arr);
        if(((ix > 0) && (cnt > 0)) && (ix >= cnt))
        {
            APE_ASSERT(false);
            return false;
        }
        #if 1
            offset = ix * cnt;
            offset = ix * arr->elemsize;    
            memmove(arr->arraydata + offset, value, arr->elemsize);
        #else
            memcpy(&arr->arraydata[(cnt + ix) + elmsz], value, arr->elemsize);
        #endif
    #endif
    return true;
}

void* ape_valarray_pop(ApeValArray_t* arr)
{
    void* res;
    ApeSize_t cnt;

    if(arr == NULL)
    {
        return NULL;
    }
    cnt = ape_valarray_count(arr);
    if(cnt == 0)
    {
        return NULL;
    }

    res = (void*)ape_valarray_get(arr, cnt - 1);

    #if defined(VALARRAY_USE_DNARRAY) && (VALARRAY_USE_DNARRAY == 1)
        //res =
        deqlist_pop(&arr->arraydata);
        #if 0
            res = *(void**)res;
        #endif
    #else
        ape_valarray_removeat(arr, cnt - 1);
    #endif

    #if defined(DEBUG) && (DEBUG == 1)
        debugmsg(arr, "ape_valarray_pop", "ix=%ld p=%p", cnt-1, res);
    #endif
    return res;
}

bool ape_valarray_popinto(ApeValArray_t* arr, void* out_value)
{
    void* res;
    if(arr == NULL)
    {
        return false;
    }
    res = ape_valarray_pop(arr);
    if(res != NULL)
    {
        if(out_value)
        {
            memcpy(out_value, res, arr->elemsize);
        }
        return true;
    }
    return false;
}

void* ape_valarray_top(ApeValArray_t* arr)
{
    ApeSize_t cnt;
    if(arr == NULL)
    {
        return NULL;
    }
    cnt = ape_valarray_count(arr);
    if(cnt == 0)
    {
        return NULL;
    }
    return ape_valarray_get(arr, cnt - 1);
}


ApeValArray_t* ape_valarray_copy(ApeContext_t* ctx, ApeValArray_t* arr)
{
    ApeSize_t i;
    ApeSize_t elemsz;
    ApeValArray_t* copy;
    void* p;
    elemsz = arr->elemsize;
    copy = ape_make_valarraycapacity(ctx, 0, elemsz);
    if(!copy)
    {
        return NULL;
    }
    copy->context = arr->context;
    copy->elemsize = elemsz;
    #if defined(DEBUG) && (DEBUG == 1)
        debugmsg(arr, "ape_valarray_copy", "%s", "");
    #endif
    for(i=0; i<ape_valarray_count(arr); i++)
    {
        p = ape_valarray_get(arr, i);
        if(p != NULL)
        {
            ape_valarray_push(copy, p);
            #if defined(VALARRAY_USE_DNARRAY) && (VALARRAY_USE_DNARRAY == 1)
                #if 0
                    deqlist_set(copy->arraydata, i, *(void**)p);
                #else
                    deqlist_set(copy->arraydata, i, p);                
                #endif
            #endif
        }
    }
    return copy;
}

bool ape_valarray_removeat(ApeValArray_t* arr, ApeSize_t ix)
{
    ApeSize_t cnt;
    (void)cnt;
    cnt = ape_valarray_count(arr);

    #if defined(DEBUG) && (DEBUG == 1)
        debugmsg(arr, "ape_valarray_removeat", "ix=%ld", ix);
    #endif
    #if defined(VALARRAY_USE_DNARRAY) && (VALARRAY_USE_DNARRAY == 1)

    #else
        ApeSize_t to_move_bytes;
        void* dest;
        void* src;
        if(ix >= arr->count)
        {
            return false;
        }
        if(ix == 0)
        {
            arr->arraydata += arr->elemsize;
            arr->capacity--;
            arr->count--;
            return true;
        }
        if(ix == (arr->count - 1))
        {
            arr->count--;
            return true;
        }
        to_move_bytes = (arr->count - 1 - ix) * arr->elemsize;
        dest = arr->arraydata + (ix * arr->elemsize);
        src = arr->arraydata + ((ix + 1) * arr->elemsize);
        memmove(dest, src, to_move_bytes);
        arr->count--;
    #endif
    return true;
}

void ape_valarray_clear(ApeValArray_t* arr)
{
    ApeSize_t sz;
    sz = ape_valarray_size(arr);
    if(sz > 0)
    {
        #if defined(VALARRAY_USE_DNARRAY) && (VALARRAY_USE_DNARRAY == 1)
            deqlist_clear(arr->arraydata, 0, sz-1);
        #else
            arr->count = 0;
        #endif
    }
}

void* ape_valarray_data(ApeValArray_t* arr)
{
    #if defined(DEBUG) && (DEBUG == 1)
        debugmsg(arr, "ape_valarray_data", "%s", "");
    #endif
    #if defined(VALARRAY_USE_DNARRAY) && (VALARRAY_USE_DNARRAY == 1)
        unsigned char** raw;
        raw = (unsigned char**)arr->arraydata->elemdata;
        return (void*)(&raw[0]);
    #else
        return arr->arraydata;
    #endif
}

void ape_valarray_orphandata(ApeValArray_t* arr)
{
    ape_valarray_initcapacity(arr, arr->context, 0);
}


ApePtrArray_t* ape_make_ptrarray(ApeContext_t* ctx)
{
    return ape_make_ptrarraycapacity(ctx, 0);
}

ApePtrArray_t* ape_make_ptrarraycapacity(ApeContext_t* ctx, ApeSize_t capacity)
{
    bool ok;
    ApeSize_t psz;
    ApePtrArray_t* ptrarr;
    ok = true;
    psz = sizeof(void*);
    #if defined(VALARRAY_USE_DNARRAY) && (VALARRAY_USE_DNARRAY == 1)
        //psz *= 2;
    #endif
    ptrarr = (ApePtrArray_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApePtrArray_t));
    if(!ptrarr)
    {
        return NULL;
    }
    ptrarr->context = ctx;
    ptrarr->arr = ape_make_valarraycapacity(ctx, capacity, psz);
    if(!ok)
    {
        ape_allocator_free(&ctx->alloc, ptrarr);
        return NULL;
    }
    return ptrarr;
}

void ape_ptrarray_destroy(ApePtrArray_t* arr)
{
    ApeContext_t* ctx;
    if(!arr)
    {
        return;
    }
    ctx = arr->context;
    ape_valarray_destroy(arr->arr);
    ape_allocator_free(&ctx->alloc, arr);
}

/* todo: destroy and copy in make fn */
void ape_ptrarray_destroywithitems(ApeContext_t* ctx, ApePtrArray_t* arr, ApeDataCallback_t destroyfn)
{
    if(arr == NULL)
    {
        return;
    }
    if(destroyfn)
    {
        ape_ptrarray_clearanddestroyitems(ctx, arr, destroyfn);
    }
    ape_ptrarray_destroy(arr);
}


void ape_ptrarray_clearanddestroyitems(ApeContext_t* ctx, ApePtrArray_t* arr, ApeDataCallback_t destroyfn)
{
    ApeSize_t i;
    void* curritem;
    for(i = 0; i < ape_ptrarray_count(arr); i++)
    {
        curritem = ape_ptrarray_get(arr, i);
        destroyfn(ctx, curritem);
    }
    ape_ptrarray_clear(arr);
}


ApePtrArray_t* ape_ptrarray_copywithitems(ApeContext_t* ctx, ApePtrArray_t* arr, ApeDataCallback_t copyfn, ApeDataCallback_t destroyfn)
{
    bool ok;
    ApeSize_t i;
    void* curritem;
    void* copieditem;
    ApePtrArray_t* copy;
    copy = ape_make_ptrarraycapacity(ctx, ape_valarray_capacity(arr->arr));
    if(!copy)
    {
        return NULL;
    }
    #if defined(DEBUG) && (DEBUG == 1)
        debugmsg(arr->arr, "ape_ptrarray_copy", "%s", "");
    #endif
    copy->context = arr->context;
    for(i = 0; i < ape_ptrarray_count(arr); i++)
    {
        curritem = ape_ptrarray_get(arr, i);
        if(copyfn == NULL)
        {
            copieditem = curritem;
        }
        else
        {
        #if defined(VALARRAY_USE_DNARRAY) && (VALARRAY_USE_DNARRAY == 1)
            //curritem = deqlist_get(arr->arr->arraydata, i);
            copieditem = copyfn(ctx, curritem);
        #else
            copieditem = copyfn(ctx, curritem);
        #endif
        }
    
        if(curritem && !copieditem)
        {
            goto err;
        }
        ok = ape_ptrarray_push(copy, &copieditem);
        //ok = ape_valarray_push(copy->arr, copieditem);
        if(!ok)
        {
            goto err;
        }
    }
    return copy;
err:
    ape_ptrarray_destroywithitems(ctx, copy, (ApeDataCallback_t)destroyfn);
    return NULL;
}

ApeSize_t ape_ptrarray_count(ApePtrArray_t* arr)
{
    if(arr == NULL)
    {
        return 0;
    }
    return ape_valarray_count(arr->arr);
}

void* ape_ptrarray_pop(ApePtrArray_t* arr)
{
    return ape_valarray_pop(arr->arr);
}

void* ape_ptrarray_top(ApePtrArray_t* arr)
{
    ApeSize_t count;
    if(arr == NULL)
    {
        return NULL;
    }
    count = ape_ptrarray_count(arr);
    if(count == 0)
    {
        return NULL;
    }
    if(count == 1)
    {
        return ape_ptrarray_get(arr, 0);
    }
    return ape_ptrarray_get(arr, count - 1);
}

bool ape_ptrarray_push(ApePtrArray_t* arr, void* ptr)
{
    return ape_valarray_push(arr->arr, ptr);
}

void* ape_ptrarray_get(ApePtrArray_t* arr, ApeSize_t ix)
{
    ApeSize_t cnt;
    void* res;
    cnt = ape_valarray_count(arr->arr);
    if(cnt == 0)
    {
        return NULL;
    }
    res = ape_valarray_get(arr->arr, ix);
    if(!res)
    {
        return NULL;
    }
    //fprintf(stderr, "ape_ptrarray_get: ix=%ld p=%p\n", ix, res);
    #if defined(VALARRAY_USE_DNARRAY) && (VALARRAY_USE_DNARRAY == 1)
        return res;
    #else
        return *(void**)res;
    #endif
}

bool ape_ptrarray_removeat(ApePtrArray_t* arr, ApeSize_t ix)
{
    return ape_valarray_removeat(arr->arr, ix);
}

void ape_ptrarray_clear(ApePtrArray_t* arr)
{
    ape_valarray_clear(arr->arr);
}

ApeObject_t ape_object_array_getvalue(ApeObject_t object, ApeSize_t ix)
{
    ApeObject_t* res;
    ApeValArray_t* array;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_ARRAY);
    array = ape_object_array_getarray(object);
    if(ix >= ape_valarray_count(array))
    {
        return ape_object_make_null(array->context);
    }
    res = (ApeObject_t*)ape_valarray_get(array, ix);
    if(!res)
    {
        return ape_object_make_null(array->context);
    }
    return *res;
}

/*
* TODO: since this pushes NULLs before 'ix' if 'ix' is out of bounds, this
* may be possibly extremely inefficient.
*/
bool ape_object_array_setat(ApeObject_t object, ApeInt_t ix, ApeObject_t val)
{
    ApeValArray_t* array;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_ARRAY);
    array = ape_object_array_getarray(object);
    if(ix < 0 || ix >= (ApeInt_t)ape_valarray_count(array))
    {
        if(ix < 0)
        {
            return false;
        }
        while(ix >= (ApeInt_t)ape_valarray_count(array))
        {
            ape_object_array_pushvalue(object, ape_object_make_null(array->context));
        }
    }
    return ape_valarray_set(array, ix, &val);
}

bool ape_object_array_pushvalue(ApeObject_t object, ApeObject_t val)
{
    ApeValArray_t* array;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_ARRAY);
    array = ape_object_array_getarray(object);
    return ape_valarray_push(array, &val);
}

bool ape_object_array_popvalue(ApeObject_t object, ApeObject_t* dest)
{

    ApeValArray_t* array;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_ARRAY);
    array = ape_object_array_getarray(object);
    return ape_valarray_popinto(array, dest);
}

ApeSize_t ape_object_array_getlength(ApeObject_t object)
{
    ApeValArray_t* array;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_ARRAY);
    array = ape_object_array_getarray(object);
    return ape_valarray_count(array);
}

bool ape_object_array_removeat(ApeObject_t object, ApeInt_t ix)
{
    ApeValArray_t* array;
    array = ape_object_array_getarray(object);
    return ape_valarray_removeat(array, ix);
}

bool ape_object_array_pushstring(ApeContext_t* ctx, ApeObject_t obj, const char* string)
{
    ApeObject_t new_value;
    ApeGCMemory_t* mem;
    mem = ape_object_value_getmem(obj);
    if(!mem)
    {
        return false;
    }
    new_value = ape_object_make_string(ctx, string);
    return ape_object_array_pushvalue(obj, new_value);
}


ApeValArray_t * ape_object_array_getarray(ApeObject_t object)
{
    ApeGCObjData_t* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_ARRAY);
    data = ape_object_value_allocated_data(object);
    return data->valarray;
}

static ApeObject_t objfn_array_length(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeObject_t self;
    (void)vm;
    (void)data;
    (void)argc;
    (void)args;
    self = ape_vm_popthisstack(vm);
    return ape_object_make_floatnumber(vm->context, ape_object_array_getlength(self));
}

static ApeObject_t objfn_array_push(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t i;
    ApeObject_t self;
    (void)vm;
    (void)data;
    (void)argc;
    (void)args;
    self = ape_vm_popthisstack(vm);
    for(i=0; i<argc; i++)
    {
        ape_object_array_pushvalue(self, args[i]);
    }
    return ape_object_make_null(vm->context);
}

static ApeObject_t objfn_array_pop(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeObject_t self;
    ApeObject_t rt;
    (void)vm;
    (void)data;
    (void)argc;
    (void)args;
    self = ape_vm_popthisstack(vm);
    if(ape_object_array_popvalue(self, &rt))
    {
        return rt;
    }
    return ape_object_make_null(vm->context);
}

static ApeObject_t objfn_array_first(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t len;
    ApeObject_t self;
    (void)vm;
    (void)data;
    (void)argc;
    (void)args;
    self = ape_vm_popthisstack(vm);
    len = ape_object_array_getlength(self);
    if(len > 0)
    {
        return ape_object_array_getvalue(self, 0);
    }
    return ape_object_make_null(vm->context);
}

static ApeObject_t objfn_array_last(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t len;
    ApeObject_t self;
    (void)vm;
    (void)data;
    (void)argc;
    (void)args;
    self = ape_vm_popthisstack(vm);
    len = ape_object_array_getlength(self);
    if(len > 0)
    {
        return ape_object_array_getvalue(self, len - 1);
    }
    return ape_object_make_null(vm->context);
}

static ApeObject_t objfn_array_fill(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t i;
    ApeSize_t len;
    ApeSize_t howmuch;
    ApeObject_t val;
    ApeObject_t self;
    ApeArgCheck_t check;
    (void)vm;
    (void)data;
    (void)argc;
    (void)args;
    (void)len;
    ape_args_init(vm, &check, "fill", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);  
    }
    if(!ape_args_check(&check, 1, APE_OBJECT_ANY))
    {
        return ape_object_make_null(vm->context);
    }
    howmuch = ape_object_value_asnumber(args[0]);
    val = args[1];
    self = ape_vm_popthisstack(vm);
    len = ape_object_array_getlength(self);
    for(i=0; i<howmuch; i++)
    {
        ape_object_array_pushvalue(self, val);
    }
    return ape_object_make_null(vm->context);
}

static ApeObject_t objfn_array_map(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeSize_t i;
    ApeSize_t len;
    ApeObject_t fn;
    ApeObject_t rt;
    ApeObject_t val;
    ApeObject_t self;
    ApeObject_t fwdargs[2];
    ApeArgCheck_t check;
    (void)vm;
    (void)data;
    (void)argc;
    (void)args;
    ape_args_init(vm, &check, "map", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_SCRIPTFUNCTION | APE_OBJECT_NATIVEFUNCTION))
    {
        return ape_object_make_null(vm->context);  
    }
    fn = args[0];
    self = ape_vm_popthisstack(vm);
    len = ape_object_array_getlength(self);
    for(i=0; i<len; i++)
    {
        val = ape_object_array_getvalue(self, i);
        ape_context_debugvalue(vm->context, "array->val", val);
        #if 0
        ape_vm_pushstack(vm, fn);
        ape_vm_pushstack(vm, val);
        if(!ape_vm_callobjectstack(vm, fn, 1))
        #else
        fwdargs[0] = ape_object_value_copyflat(vm->context, val);
        if(!ape_vm_callobjectargs(vm, fn, 1, fwdargs))
        #endif
        {
            fprintf(stderr, "failed to call function\n");
            return ape_object_make_null(vm->context);
        }
        if(vm->stackptr > 0)
        {
            rt = ape_vm_popstack(vm);
            if(i >= ape_object_array_getlength(self))
            {
                ape_object_array_pushvalue(self, rt);
            }
            else
            {
                ape_object_array_setat(self, i, rt);
            }
        }
    }
    return self;
}

static ApeObject_t objfn_array_join(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    const char* sstr;
    const char* bstr;
    ApeSize_t i;
    ApeSize_t slen;
    ApeSize_t alen;
    ApeSize_t blen;
    ApeObject_t self;
    ApeObject_t rt;
    ApeObject_t arritem;
    ApeObject_t sjoin;
    ApeWriter_t* wr;
    ApeArgCheck_t check;
    (void)data;
    sstr = "";
    slen = 0;
    self = ape_vm_popthisstack(vm);
    ape_args_init(vm, &check, "join", argc, args);
    if(ape_args_checkoptional(&check, 0, APE_OBJECT_STRING, true))
    {
        sjoin = args[0];
        if(ape_object_value_type(sjoin) != APE_OBJECT_STRING)
        {
            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "join expects optional argument to be a string");
            return ape_object_make_null(vm->context);
        }
        sstr = ape_object_string_getdata(sjoin);
        slen = ape_object_string_getlength(sjoin);
    }
    alen = ape_object_array_getlength(self);
    wr = ape_make_writer(vm->context);
    for(i=0; i<alen; i++)
    {
        arritem = ape_object_array_getvalue(self, i);
        ape_tostring_object(wr, arritem, false);
        if((i + 1) < alen)
        {
            ape_writer_appendlen(wr, sstr, slen);
        }
    }
    bstr = ape_writer_getdata(wr);
    blen = ape_writer_getlength(wr);
    rt = ape_object_make_stringlen(vm->context, bstr, blen);
    ape_writer_destroy(wr);
    return rt;
}


static ApeObject_t objfn_array_slice(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    bool ok;
    ApeInt_t i;
    ApeInt_t len;
    ApeInt_t ibegin;
    ApeInt_t iend;
    ApeObject_t self;
    ApeObject_t item;
    ApeObject_t res;
    ApeArgCheck_t check;
    (void)data;
    self = ape_vm_popthisstack(vm);
    ape_args_init(vm, &check, "slice", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);        
    }
    len = ape_object_array_getlength(self);
    ibegin = (ApeInt_t)ape_object_value_asnumber(args[0]);
    iend = len;
    if(ape_args_checkoptional(&check, 1, APE_OBJECT_NUMERIC, true))
    {
        iend = (ApeInt_t)ape_object_value_asnumber(args[1]);
        if(iend > len)
        {
            iend = len;
        }
    }
    if(ibegin < 0)
    {
        ibegin = len + ibegin;
        if(ibegin < 0)
        {
            ibegin = 0;
        }
    }
    res = ape_object_make_arraycapacity(vm->context, len - ibegin);
    if(ape_object_value_isnull(res))
    {
        return ape_object_make_null(vm->context);
    }
    for(i = ibegin; i < iend; i++)
    {
        item = ape_object_array_getvalue(self, i);
        ok = ape_object_array_pushvalue(res, item);
        if(!ok)
        {
            return ape_object_make_null(vm->context);
        }
    }
    return res;
}

void ape_builtins_install_array(ApeVM_t* vm)
{
    ApeSize_t i;
    ApePseudoClass_t* psc;
    static const char classname[] = "Array";
    static ApeObjMemberItem_t memberfuncs[]=
    {
        {"length", false, objfn_array_length},
        {"push", true, objfn_array_push},
        {"append", true, objfn_array_push},
        {"pop", true, objfn_array_pop},
        {"fill", true, objfn_array_fill},
        {"map", true, objfn_array_map},
        {"join", true, objfn_array_join},
        {"slice", true, objfn_array_slice},

        /* pseudo funcs */
        {"first", false, objfn_array_first},
        {"last", false, objfn_array_last},

        /* TODO: implement me! */
        #if 0
        /* {"", true, objfn_array_}, */
        {"map", true, objfn_array_map},
        {"sort", true, objfn_array_sort},
        {"grep", true, objfn_array_grep},

        #endif
        {NULL, false, NULL},
    };
    psc = ape_context_make_pseudoclass(vm->context, vm->context->objarrayfuncs, APE_OBJECT_ARRAY, classname);
    for(i=0; memberfuncs[i].name != NULL; i++)
    {
        ape_pseudoclass_setmethod(psc, memberfuncs[i].name, &memberfuncs[i]);
    }
}

