
#include "ape.h"

static const ApePosition_t g_mempriv_src_pos_invalid = { NULL, -1, -1 };

void* util_default_malloc(void* opaqptr, size_t size)
{
    void* resptr;
    ApeContext_t* ctx;
    ctx = (ApeContext_t*)opaqptr;
    resptr = (void*)allocator_malloc(&ctx->custom_allocator, size);
    if(!resptr)
    {
        errorlist_add(&ctx->errors, APE_ERROR_ALLOCATION, g_mempriv_src_pos_invalid, "allocation failed");
    }
    return resptr;
}

void util_default_free(void* opaqptr, void* objptr)
{
    ApeContext_t* ctx;
    ctx = (ApeContext_t*)opaqptr;
    allocator_free(&ctx->custom_allocator, objptr);
}

void* allocator_malloc(ApeAllocator_t* alloc, size_t size)
{
    (void)alloc;
    return malloc(size);
    /*
    if(!alloc || !alloc->malloc)
    {
        return malloc(size);
    }
    return alloc->malloc(alloc->ctx, size);
    */
}

void allocator_free(ApeAllocator_t* alloc, void* ptr)
{
    (void)alloc;
    free(ptr);
    /*
    if(!alloc || !alloc->free)
    {
        free(ptr);
        return;
    }
    alloc->free(alloc->ctx, ptr);
    */
}

ApeAllocator_t allocator_make(ApeMemAllocFunc_t malloc_fn, ApeMemFreeFunc_t free_fn, void* ctx)
{
    ApeAllocator_t alloc;
    alloc.malloc = malloc_fn;
    alloc.free = free_fn;
    alloc.ctx = ctx;
    return alloc;
}

ApeGCMemory_t* gcmem_make(ApeContext_t* ctx)
{
    ApeSize_t i;
    ApeGCMemory_t* mem;
    ApeObjectDataPool_t* pool;
    mem = (ApeGCMemory_t*)allocator_malloc(&ctx->alloc, sizeof(ApeGCMemory_t));
    if(!mem)
    {
        return NULL;
    }
    memset(mem, 0, sizeof(ApeGCMemory_t));
    mem->context = ctx;
    mem->alloc = &ctx->alloc;
    mem->objects = ptrarray_make(ctx);
    if(!mem->objects)
    {
        goto error;
    }
    mem->objects_back = ptrarray_make(ctx);
    if(!mem->objects_back)
    {
        goto error;
    }
    mem->objects_not_gced = array_make(ctx, ApeObject_t);
    if(!mem->objects_not_gced)
    {
        goto error;
    }
    mem->allocations_since_sweep = 0;
    mem->data_only_pool.count = 0;
    for(i = 0; i < GCMEM_POOLS_NUM; i++)
    {
        pool = &mem->pools[i];
        mem->pools[i].count = 0;
        memset(pool, 0, sizeof(ApeObjectDataPool_t));
    }
    return mem;
error:
    gcmem_destroy(mem);
    return NULL;
}

void gcmem_destroy(ApeGCMemory_t* mem)
{
    ApeSize_t i;
    ApeSize_t j;
    ApeObjectData_t* obj;
    ApeObjectData_t* data;
    ApeObjectDataPool_t* pool;
    if(!mem)
    {
        return;
    }
    array_destroy(mem->objects_not_gced);
    ptrarray_destroy(mem->objects_back);
    for(i = 0; i < ptrarray_count(mem->objects); i++)
    {
        obj = (ApeObjectData_t*)ptrarray_get(mem->objects, i);
        object_data_deinit(obj);
        allocator_free(mem->alloc, obj);
    }
    ptrarray_destroy(mem->objects);
    for(i = 0; i < GCMEM_POOLS_NUM; i++)
    {
        pool = &mem->pools[i];
        for(j = 0; j < pool->count; j++)
        {
            data = pool->datapool[j];
            object_data_deinit(data);
            allocator_free(mem->alloc, data);
        }
        memset(pool, 0, sizeof(ApeObjectDataPool_t));
    }
    for(i = 0; i < mem->data_only_pool.count; i++)
    {
        allocator_free(mem->alloc, mem->data_only_pool.datapool[i]);
    }
    allocator_free(mem->alloc, mem);
}

ApeObjectData_t* gcmem_alloc_object_data(ApeGCMemory_t* mem, ApeObjectType_t type)
{
    bool ok;
    ApeObjectData_t* data;
    data = NULL;
    mem->allocations_since_sweep++;
    if(mem->data_only_pool.count > 0)
    {
        data = mem->data_only_pool.datapool[mem->data_only_pool.count - 1];
        mem->data_only_pool.count--;
    }
    else
    {
        data = (ApeObjectData_t*)allocator_malloc(mem->alloc, sizeof(ApeObjectData_t));
        if(!data)
        {
            return NULL;
        }
    }
    memset(data, 0, sizeof(ApeObjectData_t));
    APE_ASSERT(ptrarray_count(mem->objects_back) >= ptrarray_count(mem->objects));
    // we want to make sure that appending to objects_back never fails in sweep
    // so this only reserves space there.
    ok = ptrarray_add(mem->objects_back, data);
    if(!ok)
    {
        allocator_free(mem->alloc, data);
        return NULL;
    }
    ok = ptrarray_add(mem->objects, data);
    if(!ok)
    {
        allocator_free(mem->alloc, data);
        return NULL;
    }
    data->mem = mem;
    data->type = type;
    return data;
}

ApeObjectData_t* gcmem_get_object_data_from_pool(ApeGCMemory_t* mem, ApeObjectType_t type)
{
    bool ok;
    ApeObjectData_t* data;
    ApeObjectDataPool_t* pool;
    pool = get_pool_for_type(mem, type);
    if(!pool || pool->count <= 0)
    {
        return NULL;
    }
    data = pool->datapool[pool->count - 1];
    APE_ASSERT(ptrarray_count(mem->objects_back) >= ptrarray_count(mem->objects));
    // we want to make sure that appending to objects_back never fails in sweep
    // so this only reserves space there.
    ok = ptrarray_add(mem->objects_back, data);
    if(!ok)
    {
        return NULL;
    }
    ok = ptrarray_add(mem->objects, data);
    if(!ok)
    {
        return NULL;
    }
    pool->count--;
    return data;
}

void gc_unmark_all(ApeGCMemory_t* mem)
{
    ApeSize_t i;
    ApeObjectData_t* data;
    for(i = 0; i < ptrarray_count(mem->objects); i++)
    {
        data = (ApeObjectData_t*)ptrarray_get(mem->objects, i);
        data->gcmark = false;
    }
}

void gc_mark_objects(ApeObject_t* objects, ApeSize_t count)
{
    ApeSize_t i;
    ApeObject_t obj;
    for(i = 0; i < count; i++)
    {
        obj = objects[i];
        gc_mark_object(obj);
    }
}

void gc_mark_object(ApeObject_t obj)
{
    ApeSize_t i;
    ApeSize_t len;
    ApeObject_t key;
    ApeObject_t val;
    ApeObjectData_t* key_data;
    ApeObjectData_t* val_data;
    ApeFunction_t* function;
    ApeObject_t free_val;
    ApeObjectData_t* free_val_data;
    ApeObjectData_t* data;
    if(!object_value_isallocated(obj))
    {
        return;
    }
    data = object_value_allocated_data(obj);
    if(data->gcmark)
    {
        return;
    }
    data->gcmark = true;
    switch(data->type)
    {
        case APE_OBJECT_MAP:
        {
            len = object_map_getlength(obj);
            for(i = 0; i < len; i++)
            {
                key = object_map_getkeyat(obj, i);
                if(object_value_isallocated(key))
                {

                    key_data = object_value_allocated_data(key);
                    if(!key_data->gcmark)
                    {
                        gc_mark_object(key);
                    }
                }
                val = object_map_getvalueat(obj, i);
                if(object_value_isallocated(val))
                {

                    val_data = object_value_allocated_data(val);
                    if(!val_data->gcmark)
                    {
                        gc_mark_object(val);
                    }
                }
            }
            break;
        }
        case APE_OBJECT_ARRAY:
            {
                len = object_array_getlength(obj);
                for(i = 0; i < len; i++)
                {
                    val = object_array_getvalue(obj, i);
                    if(object_value_isallocated(val))
                    {
                        val_data = object_value_allocated_data(val);
                        if(!val_data->gcmark)
                        {
                            gc_mark_object(val);
                        }
                    }
                }
            }
            break;
        case APE_OBJECT_FUNCTION:
            {
                function = object_value_asfunction(obj);
                for(i = 0; i < function->free_vals_count; i++)
                {
                    free_val = object_function_getfreeval(obj, i);
                    gc_mark_object(free_val);
                    if(object_value_isallocated(free_val))
                    {
                        free_val_data = object_value_allocated_data(free_val);
                        if(!free_val_data->gcmark)
                        {
                            gc_mark_object(free_val);
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

void gc_sweep(ApeGCMemory_t* mem)
{
    ApeSize_t i;
    bool ok;
    ApeObjectData_t* data;
    ApeObjectDataPool_t* pool;
    ApePtrArray_t* objs_temp;
    gc_mark_objects((ApeObject_t*)array_data(mem->objects_not_gced), array_count(mem->objects_not_gced));
    APE_ASSERT(ptrarray_count(mem->objects_back) >= ptrarray_count(mem->objects));
    ptrarray_clear(mem->objects_back);
    for(i = 0; i < ptrarray_count(mem->objects); i++)
    {
        data = (ApeObjectData_t*)ptrarray_get(mem->objects, i);
        if(data->gcmark)
        {
            // this should never fail because objects_back's size should be equal to objects
            ok = ptrarray_add(mem->objects_back, data);
            (void)ok;
            APE_ASSERT(ok);
        }
        else
        {
            if(can_data_be_put_in_pool(mem, data))
            {
                pool = get_pool_for_type(mem, data->type);
                pool->datapool[pool->count] = data;
                pool->count++;
            }
            else
            {
                object_data_deinit(data);
                if(mem->data_only_pool.count < GCMEM_POOL_SIZE)
                {
                    mem->data_only_pool.datapool[mem->data_only_pool.count] = data;
                    mem->data_only_pool.count++;
                }
                else
                {
                    allocator_free(mem->alloc, data);
                }
            }
        }
    }
    objs_temp = mem->objects;
    mem->objects = mem->objects_back;
    mem->objects_back = objs_temp;
    mem->allocations_since_sweep = 0;
}

int gc_should_sweep(ApeGCMemory_t* mem)
{
    return mem->allocations_since_sweep > GCMEM_SWEEP_INTERVAL;
}

// INTERNAL
ApeObjectDataPool_t* get_pool_for_type(ApeGCMemory_t* mem, ApeObjectType_t type)
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

bool can_data_be_put_in_pool(ApeGCMemory_t* mem, ApeObjectData_t* data)
{
    ApeObject_t obj;
    ApeObjectDataPool_t* pool;
    obj = object_make_from_data(data->type, data);
    // this is to ensure that large objects won't be kept in pool indefinitely
    switch(data->type)
    {
        case APE_OBJECT_ARRAY:
            {
                if(object_array_getlength(obj) > 1024)
                {
                    return false;
                }
            }
            break;
        case APE_OBJECT_MAP:
            {
                if(object_map_getlength(obj) > 1024)
                {
                    return false;
                }
            }
            break;
        case APE_OBJECT_STRING:
            {
                if(!data->string.is_allocated || data->string.capacity > 4096)
                {
                    return false;
                }
            }
            break;
        default:
            {
            }
            break;
    }
    pool = get_pool_for_type(mem, data->type);
    if(!pool || pool->count >= GCMEM_POOL_SIZE)
    {
        return false;
    }
    return true;
}


