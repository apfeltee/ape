
/*
* this file contains the brunt of the garbage collector logic.
* be careful when modifying, as this is programmatically
* akin to the "jesus nut" (https://en.wikipedia.org/wiki/Jesus_nut),
* and things WILL break terribly.
*/

#include "inline.h"

/* decreasing these incurs higher memory use */
#define APE_CONF_SIZE_GCMEM_POOLSIZE (512 * 4)
#define APE_CONF_SIZE_GCMEM_POOLCOUNT ((4) + 1)
#define APE_CONF_CONST_GCMEM_SWEEPINTERVAL (128 / 64)

#define APE_ACTUAL_POOLSIZE (APE_CONF_SIZE_GCMEM_POOLSIZE)

#define APE_CONF_SIZE_MEMPOOL_INITIAL (1024/4)
#define APE_CONF_SIZE_MEMPOOL_MAX 0

struct ApeGCObjPool_t
{
    DequeList_t* datapool;
    ApeSize_t count;
};

struct ApeGCMemory_t
{
    ApeContext_t* context;
    ApeAllocator_t* alloc;
    ApeSize_t allocations_since_sweep;

    //ApeGCObjData_t** frontobjects;
    intptr_t* frontobjects;

    //ApeGCObjData_t** backobjects;
    intptr_t* backobjects;

    ApeValArray_t* objects_not_gced;
    ApeGCObjPool_t data_only_pool;
    ApeGCObjPool_t pools[APE_CONF_SIZE_GCMEM_POOLCOUNT];
};

static const ApePosition_t g_posinvalid = { NULL, -1, -1 };

void* ds_extmalloc(size_t size, void* userptr)
{
    ApeContext_t* ctx;
    ctx = (ApeContext_t*)userptr;
    //fprintf(stderr, "ds_extmalloc: userptr=%p\n", userptr);
    return ape_allocator_alloc(&ctx->alloc, size);
}

void* ds_extrealloc(void* ptr, size_t oldsz, size_t newsz, void* userptr)
{
    ApeContext_t* ctx;
    ctx = (ApeContext_t*)userptr;
    //fprintf(stderr, "ds_extrealloc: userptr=%p\n", userptr);    
    return ape_allocator_realloc(&ctx->alloc, ptr, oldsz, newsz);
}

void ds_extfree(void* ptr, void* userptr)
{
    ApeContext_t* ctx;
    ctx = (ApeContext_t*)userptr;
    //fprintf(stderr, "ds_extfree: userptr=%p\n", userptr);
    return ape_allocator_free(&ctx->alloc, ptr);
}

void poolinit(ApeContext_t* ctx, ApeGCObjPool_t* pool)
{
    (void)ctx;
    pool->count = 0;
    pool->datapool = deqlist_create_empty(sizeof(ApeGCObjData_t*));
}

void pooldestroy(ApeContext_t* ctx, ApeGCObjPool_t* pool)
{
    (void)ctx;
    deqlist_destroy(pool->datapool);
}

void poolput(ApeGCObjPool_t* pool, ApeInt_t idx, ApeGCObjData_t* data)
{
    if(idx >= deqlist_len(pool->datapool))
    {
        deqlist_push(&pool->datapool, data);
    }
    else
    {
        deqlist_set(pool->datapool, idx, data);
    }
}

ApeGCObjData_t* poolget(ApeGCObjPool_t* pool, ApeInt_t idx)
{
    return (ApeGCObjData_t*)deqlist_get(pool->datapool, idx);
}

void* ape_mem_defaultmalloc(ApeContext_t* ctx, void* userptr, size_t size)
{
    void* resptr;
    (void)userptr;
    resptr = (void*)ape_allocator_alloc(&ctx->custom_allocator, size);
    if(!resptr)
    {
        ape_errorlist_add(&ctx->errors, APE_ERROR_ALLOCATION, g_posinvalid, "allocation failed");
    }
    return resptr;
}

void ape_mem_defaultfree(ApeContext_t* ctx, void* userptr, void* objptr)
{
    (void)userptr;
    ape_allocator_free(&ctx->custom_allocator, objptr);
}

void* wrap_alloc(ApeAllocator_t* alloc, size_t size)
{
    if(alloc != NULL)
    {
        if(alloc->fnmalloc != NULL)
        {
            return alloc->fnmalloc(alloc->context, alloc->optr, size);
        }
    }
    return (malloc)(size);
}

void wrap_free(ApeAllocator_t* alloc, void* ptr)
{
    if(alloc != NULL)
    {
        if(alloc->fnfree != NULL)
        {
            alloc->fnfree(alloc->context, alloc->optr, ptr);
            return;
        }
    }
    free(ptr);    
}

void* ape_allocator_alloc_real(ApeAllocator_t* alloc, const char* str, const char* func, const char* file, int line, ApeInt_t size)
{
    if(!alloc->ready)
    {
        fprintf(stderr, "not ready, must initialize\n");
        alloc->pool = ape_mempool_init(APE_CONF_SIZE_MEMPOOL_INITIAL, APE_CONF_SIZE_MEMPOOL_MAX);
        alloc->ready = true;
    }
    ape_mempool_debugprintf(alloc->pool, "ape_allocator_alloc: %zu [%s:%d:%s] %s\n", size, file, line, func, str);
    if(size == 0)
    {
        //return NULL;
    }
    return ape_mempool_alloc(alloc->pool, size);
}

void ape_allocator_free(ApeAllocator_t* alloc, void* ptr)
{
    /* nothing to do */
    ape_mempool_free(alloc->pool, ptr);
}

void* ape_allocator_realloc(ApeAllocator_t* alloc, void* ptr, size_t oldsz, size_t newsz)
{
    return ape_mempool_realloc(alloc->pool, ptr, oldsz, newsz);
}


ApeAllocator_t* ape_make_allocator(ApeContext_t* ctx, ApeAllocator_t* dest, ApeMemAllocFunc_t malloc_fn, ApeMemFreeFunc_t free_fn, void* optr)
{
    memset(dest, 0, sizeof(ApeAllocator_t));
    dest->context = ctx;
    dest->fnmalloc = malloc_fn;
    dest->fnfree = free_fn;
    dest->optr = optr;
    dest->ready = false;
    return dest;
}

bool ape_allocator_setdebughandle(ApeAllocator_t* alloc, FILE* hnd, bool mustclose)
{
    return ape_mempool_setdebughandle(alloc->pool, hnd, mustclose);
}

bool ape_allocator_setdebugfile(ApeAllocator_t* alloc, const char* path)
{
    return ape_mempool_setdebugfile(alloc->pool, path);
}


void ape_allocator_destroy(ApeAllocator_t* alloc)
{
    (void)alloc;
    fprintf(stderr, "destroying allocator: %ld allocations (%ld of which mapped), %ld bytes total\n", 
        alloc->pool->totalalloccount,
        alloc->pool->totalmapped,
        alloc->pool->totalbytes
    );
    ape_mempool_destroy(alloc->pool);
}

ApeGCMemory_t* ape_make_gcmem(ApeContext_t* ctx)
{
    ApeSize_t i;
    ApeGCMemory_t* mem;
    ApeGCObjPool_t* pool;
    mem = (ApeGCMemory_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeGCMemory_t));
    if(!mem)
    {
        return NULL;
    }
    memset(mem, 0, sizeof(ApeGCMemory_t));
    mem->context = ctx;
    mem->alloc = &ctx->alloc;
    mem->frontobjects = da_make(mem->frontobjects, APE_CONF_PLAINLIST_CAPACITY_ADD, sizeof(ApeGCObjData_t*));
    if(!mem->frontobjects)
    {
        goto error;
    }
    mem->backobjects = da_make(mem->backobjects, APE_CONF_PLAINLIST_CAPACITY_ADD, sizeof(ApeGCObjData_t*));
    if(!mem->backobjects)
    {
        goto error;
    }
    mem->objects_not_gced = ape_make_valarray(ctx, sizeof(ApeObject_t));
    if(!mem->objects_not_gced)
    {
        goto error;
    }
    mem->allocations_since_sweep = 0;
    poolinit(ctx, &mem->data_only_pool);
    for(i = 0; i < APE_CONF_SIZE_GCMEM_POOLCOUNT; i++)
    {
        pool = &mem->pools[i];
        poolinit(ctx, pool);
    }
    return mem;
error:
    ape_gcmem_destroy(mem);
    return NULL;
}

void ape_gcmem_destroy(ApeGCMemory_t* mem)
{
    ApeSize_t i;
    ApeSize_t j;
    ApeSize_t notgclen;
    ApeGCObjData_t* obj;
    ApeGCObjData_t* data;
    ApeGCObjPool_t* pool;
    if(!mem)
    {
        return;
    }
    notgclen = ape_valarray_count(mem->objects_not_gced);
    if(notgclen != 0)
    {
        fprintf(stderr, "releasing %d objects not caught by GC\n", (int)notgclen);
        for(i=0; i<notgclen; i++)
        {
        }
    }
    ape_valarray_destroy(mem->objects_not_gced);
    da_destroy(mem->backobjects);
    for(i = 0; i < da_count(mem->frontobjects); i++)
    {
        obj = (ApeGCObjData_t*)da_get(mem->frontobjects, i);
        ape_object_data_deinit(mem->context, obj);
        ape_allocator_free(mem->alloc, obj);
    }
    da_destroy(mem->frontobjects);
    for(i = 0; i < APE_CONF_SIZE_GCMEM_POOLCOUNT; i++)
    {
        pool = &mem->pools[i];
        for(j = 0; j < (pool->count + 0); j++)
        //for(j=0; j<deqlist_size(pool->datapool); j++)
        {
            data = poolget(pool, j);
            fprintf(stderr, "deinit: type=%s\n", ape_object_value_typename(data->datatype));
            ape_object_data_deinit(mem->context, data);
            ape_allocator_free(mem->alloc, data);
        }
        pooldestroy(mem->context, pool);
        memset(pool, 0, sizeof(ApeGCObjPool_t));
    }
    for(i = 0; i < mem->data_only_pool.count; i++)
    //for(i=0; i<deqlist_size(mem->data_only_pool.datapool); i++)
    {
        ape_allocator_free(mem->alloc, poolget(&mem->data_only_pool, i));
    }
    pooldestroy(mem->context, &mem->data_only_pool);
    ape_allocator_free(mem->alloc, mem);
}

ApeGCObjData_t* ape_gcmem_allocobjdata(ApeGCMemory_t* mem, ApeObjType_t type)
{
    ApeGCObjData_t* data;
    (void)type;
    data = NULL;
    mem->allocations_since_sweep++;
    if(mem->data_only_pool.count > 0)
    {
        data = poolget(&mem->data_only_pool, mem->data_only_pool.count - 1);
        mem->data_only_pool.count--;
    }
    else
    {
        data = (ApeGCObjData_t*)ape_allocator_alloc(mem->alloc, sizeof(ApeGCObjData_t));
        if(data == NULL)
        {
            return NULL;
        }
    }
    if(data == NULL)
    {
        return NULL;
    }
    memset(data, 0, sizeof(ApeGCObjData_t));
    APE_ASSERT(da_count(mem->backobjects) >= da_count(mem->frontobjects));
    /*
    // we want to make sure that appending to backobjects never fails in sweep
    // so this only reserves space there.
    */
    da_push(mem->backobjects, data);
    da_push(mem->frontobjects, data);
    data->mem = mem;
    data->datatype = type;
    return data;
}


bool ape_gcmem_canputinpool(ApeGCMemory_t* mem, ApeGCObjData_t* data)
{
    ApeObject_t obj;
    ApeGCObjPool_t* pool;
    obj = object_make_from_data(mem->context, (ApeObjType_t)data->datatype, data);
    /* this is to ensure that large objects won't be kept in pool indefinitely */
    switch(data->datatype)
    {
        case APE_OBJECT_ARRAY:
            {
                if(ape_object_array_getlength(obj) > 1024)
                {
                    return false;
                }
            }
            break;
        case APE_OBJECT_MAP:
            {
                if(ape_object_map_getlength(obj) > 1024)
                {
                    return false;
                }
            }
            break;
        case APE_OBJECT_STRING:
            {
                return false;
                if(data->valstring.valalloc != NULL)
                {
                    if(ds_getavailable(data->valstring.valalloc) > 4096)
                    {
                        return false;
                    }
                }
            }
            break;
        default:
            {
            }
            break;
    }
    pool = ape_gcmem_getpoolfor(mem, (ApeObjType_t)data->datatype);
    if(!pool || pool->count >= APE_ACTUAL_POOLSIZE)
    {
        return false;
    }
    return true;
}

ApeGCObjPool_t* ape_gcmem_getpoolfor(ApeGCMemory_t* mem, ApeObjType_t type)
{
    switch(type)
    {
        case APE_OBJECT_ARRAY:
            {
                return &mem->pools[0];
            }
            break;
        case APE_OBJECT_MAP:
            {
                return &mem->pools[1];
            }
            break;
        case APE_OBJECT_STRING:
            {
                return &mem->pools[2];
            }
            break;
        default:
            {
            }
            break;
    }
    return NULL;

}

ApeGCObjData_t* ape_gcmem_getfrompool(ApeGCMemory_t* mem, ApeObjType_t type)
{
    ApeGCObjData_t* data;
    ApeGCObjPool_t* pool;
    pool = ape_gcmem_getpoolfor(mem, type);
    if(!pool || (pool->count <= 0))
    {
        return NULL;
    }
    data = poolget(pool, pool->count - 1);
    APE_ASSERT(da_count(mem->backobjects) >= da_count(mem->frontobjects));
    /*
    // we want to make sure that appending to backobjects never fails in sweep
    // so this only reserves space there.
    */
    da_push(mem->backobjects, data);
    da_push(mem->frontobjects, data);
    pool->count--;
    return data;
}

void ape_gcmem_unmarkall(ApeGCMemory_t* mem)
{
    ApeSize_t i;
    ApeGCObjData_t* data;
    for(i = 0; i < da_count(mem->frontobjects); i++)
    {
        data = (ApeGCObjData_t*)da_get(mem->frontobjects, i);
        if(data != NULL)
        {
            data->gcmark = false;
        }
    }
}

void ape_gcmem_markobjlist(ApeObject_t* objects, ApeSize_t count)
{
    ApeSize_t i;
    ApeObject_t obj;
    for(i = 0; i < count; i++)
    {
        obj = objects[i];
        if(obj.type != APE_OBJECT_NONE)
        {
            if(obj.handle)
            {
                ape_gcmem_markobject(obj);
            }
        }
    }
}

void ape_gcmem_markobject(ApeObject_t obj)
{
    ApeSize_t i;
    ApeSize_t len;
    ApeObject_t key;
    ApeObject_t val;
    ApeGCObjData_t* key_data;
    ApeGCObjData_t* val_data;
    ApeScriptFunction_t* function;
    ApeObject_t free_val;
    ApeGCObjData_t* free_val_data;
    ApeGCObjData_t* data;
    if(!ape_object_value_isallocated(obj))
    {
        return;
    }
    data = ape_object_value_allocated_data(obj);
    if(data == NULL)
    {
        return;
    }
    if(data->gcmark)
    {
        return;
    }
    data->gcmark = true;
    switch(data->datatype)
    {
        case APE_OBJECT_MAP:
        {
            len = ape_object_map_getlength(obj);
            for(i = 0; i < len; i++)
            {
                key = ape_object_map_getkeyat(obj, i);
                if(ape_object_value_isallocated(key))
                {

                    key_data = ape_object_value_allocated_data(key);
                    if(!key_data->gcmark)
                    {
                        ape_gcmem_markobject(key);
                    }
                }
                val = ape_object_map_getvalueat(obj, i);
                if(ape_object_value_isallocated(val))
                {
                    val_data = ape_object_value_allocated_data(val);
                    if(val_data != NULL)
                    {
                        if(!val_data->gcmark)
                        {
                            ape_gcmem_markobject(val);
                        }
                    }
                }
            }
            break;
        }
        case APE_OBJECT_ARRAY:
            {
                len = ape_object_array_getlength(obj);
                for(i = 0; i < len; i++)
                {
                    val = ape_object_array_getvalue(obj, i);
                    if(ape_object_value_isallocated(val))
                    {
                        val_data = ape_object_value_allocated_data(val);
                        if(val_data != NULL)
                        {
                            if(!val_data->gcmark)
                            {
                                ape_gcmem_markobject(val);
                            }
                        }
                    }
                }
            }
            break;
        case APE_OBJECT_SCRIPTFUNCTION:
            {
                function = ape_object_value_asscriptfunction(obj);
                for(i = 0; i < function->numfreevals; i++)
                {
                    free_val = ape_object_function_getfreeval(obj, i);
                    ape_gcmem_markobject(free_val);
                    if(ape_object_value_isallocated(free_val))
                    {
                        free_val_data = ape_object_value_allocated_data(free_val);
                        if(free_val_data)
                        {
                            if(!free_val_data->gcmark)
                            {
                                ape_gcmem_markobject(free_val);
                            }
                        }
                    }
                }
            }
            break;
        default:
            {
            }
            break;
    }
}

void ape_gcmem_sweep(ApeGCMemory_t* mem)
{
    ApeSize_t i;
    ApeGCObjData_t* data;
    ApeGCObjPool_t* pool;
    intptr_t* objs_temp;
    ape_gcmem_markobjlist((ApeObject_t*)ape_valarray_data(mem->objects_not_gced), ape_valarray_count(mem->objects_not_gced));
    APE_ASSERT(da_count(mem->backobjects) >= da_count(mem->frontobjects));
    da_clear(mem->backobjects);
    for(i = 0; i < da_count(mem->frontobjects); i++)
    {
        data = (ApeGCObjData_t*)da_get(mem->frontobjects, i);
        if(data != NULL)
        {
            if(data->gcmark)
            {
                /* this should never fail because backobjects's size should be equal to objects */
                da_push(mem->backobjects, data);
            }
            else
            {
                if(ape_gcmem_canputinpool(mem, data))
                {
                    pool = ape_gcmem_getpoolfor(mem, (ApeObjType_t)data->datatype);
                    poolput(pool, pool->count, data);
                    pool->count++;
                }
                else
                {
                    ape_object_data_deinit(mem->context, data);
                    if(mem->data_only_pool.count < APE_ACTUAL_POOLSIZE)
                    {
                        poolput(&mem->data_only_pool, mem->data_only_pool.count, data);
                        mem->data_only_pool.count++;
                    }
                    else
                    {
                        ape_allocator_free(mem->alloc, data);
                    }
                }
            }
        }
    }
    objs_temp = mem->frontobjects;
    mem->frontobjects = mem->backobjects;
    mem->backobjects = objs_temp;
    mem->allocations_since_sweep = 0;
}

int ape_gcmem_shouldsweep(ApeGCMemory_t* mem)
{
    return mem->allocations_since_sweep > APE_CONF_CONST_GCMEM_SWEEPINTERVAL;
}


