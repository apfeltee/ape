
#include "inline.h"

static const ApePosition_t g_mempriv_srcposinvalid = { NULL, -1, -1 };

void* ape_mem_defaultmalloc(void* opaqptr, size_t size)
{
    void* resptr;
    ApeContext_t* ctx;
    ctx = (ApeContext_t*)opaqptr;
    resptr = (void*)ape_allocator_alloc(&ctx->custom_allocator, size);
    if(!resptr)
    {
        ape_errorlist_add(&ctx->errors, APE_ERROR_ALLOCATION, g_mempriv_srcposinvalid, "allocation failed");
    }
    return resptr;
}

void ape_mem_defaultfree(void* opaqptr, void* objptr)
{
    ApeContext_t* ctx;
    ctx = (ApeContext_t*)opaqptr;
    ape_allocator_free(&ctx->custom_allocator, objptr);
}

//        ape_allocator_alloc_debug(alc, #sz, __FUNCTION__, __FILE__, __LINE__, sz)

void* ape_allocator_alloc_debug(ApeAllocator_t* alloc, const char* str, const char* func, const char* file, int line, size_t size)
{
    fprintf(stderr, "%d bytes allocd in %s:%d:%s: %s\n", size, file, line, func, str);
    return ape_allocator_alloc_real(alloc, size);
}

void* ape_allocator_alloc_real(ApeAllocator_t* alloc, size_t size)
{
    (void)alloc;
    return malloc(size);
    /*
    if(!alloc || !alloc->fnmalloc)
    {
        return fnmalloc(size);
    }
    return alloc->fnmalloc(alloc->ctx, size);
    */
}

void ape_allocator_free(ApeAllocator_t* alloc, void* ptr)
{
    (void)alloc;
    free(ptr);
    /*
    if(!alloc || !alloc->fnfree)
    {
        fnfree(ptr);
        return;
    }
    alloc->fnfree(alloc->ctx, ptr);
    */
}

ApeAllocator_t ape_make_allocator(ApeMemAllocFunc_t malloc_fn, ApeMemFreeFunc_t free_fn, void* ctx)
{
    ApeAllocator_t alloc;
    alloc.fnmalloc = malloc_fn;
    alloc.fnfree = free_fn;
    alloc.ctx = ctx;
    return alloc;
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
    //mem->frontobjects = ape_make_ptrarray(ctx);
    mem->frontobjects = da_make(mem->frontobjects, APE_CONF_PLAINLIST_CAPACITY_ADD, sizeof(ApeGCObjData_t));
    if(!mem->frontobjects)
    {
        goto error;
    }
    //mem->backobjects = ape_make_ptrarray(ctx);
    mem->backobjects = da_make(mem->backobjects, APE_CONF_PLAINLIST_CAPACITY_ADD, sizeof(ApeGCObjData_t));
    if(!mem->backobjects)
    {
        goto error;
    }
    mem->objects_not_gced = ape_make_valarray(ctx, ApeObject_t);
    if(!mem->objects_not_gced)
    {
        goto error;
    }
    mem->allocations_since_sweep = 0;
    mem->data_only_pool.count = 0;
    for(i = 0; i < APE_CONF_SIZE_GCMEM_POOLCOUNT; i++)
    {
        pool = &mem->pools[i];
        mem->pools[i].count = 0;
        memset(pool, 0, sizeof(ApeGCObjPool_t));
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
    ApeGCObjData_t* obj;
    ApeGCObjData_t* data;
    ApeGCObjPool_t* pool;
    if(!mem)
    {
        return;
    }
    ape_valarray_destroy(mem->objects_not_gced);
    da_destroy(mem->backobjects);
    for(i = 0; i < da_count(mem->frontobjects); i++)
    {
        obj = (ApeGCObjData_t*)da_get(mem->frontobjects, i);
        ape_object_data_deinit(obj);
        ape_allocator_free(mem->alloc, obj);
    }
    da_destroy(mem->frontobjects);
    for(i = 0; i < APE_CONF_SIZE_GCMEM_POOLCOUNT; i++)
    {
        pool = &mem->pools[i];
        for(j = 0; j < pool->count; j++)
        {
            data = pool->datapool[j];
            ape_object_data_deinit(data);
            ape_allocator_free(mem->alloc, data);
        }
        memset(pool, 0, sizeof(ApeGCObjPool_t));
    }
    for(i = 0; i < mem->data_only_pool.count; i++)
    {
        ape_allocator_free(mem->alloc, mem->data_only_pool.datapool[i]);
    }
    ape_allocator_free(mem->alloc, mem);
}

ApeGCObjData_t* ape_gcmem_allocobjdata(ApeGCMemory_t* mem, ApeObjType_t type)
{
    bool ok;
    ApeGCObjData_t* data;
    data = NULL;
    mem->allocations_since_sweep++;
    if(mem->data_only_pool.count > 0)
    {
        data = mem->data_only_pool.datapool[mem->data_only_pool.count - 1];
        mem->data_only_pool.count--;
    }
    else
    {
        data = (ApeGCObjData_t*)ape_allocator_alloc(mem->alloc, sizeof(ApeGCObjData_t));
        if(!data)
        {
            return NULL;
        }
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
    //data->datatype = type;
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
                if(!data->valstring.is_allocated || data->valstring.capacity > 4096)
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
    pool = ape_gcmem_getpoolfor(mem, (ApeObjType_t)data->datatype);
    if(!pool || pool->count >= APE_CONF_SIZE_GCMEM_POOLSIZE)
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
    bool ok;
    ApeGCObjData_t* data;
    ApeGCObjPool_t* pool;
    pool = ape_gcmem_getpoolfor(mem, type);
    if(!pool || (pool->count <= 0))
    {
        return NULL;
    }
    data = pool->datapool[pool->count - 1];
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
        data->gcmark = false;
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
                    if(!val_data->gcmark)
                    {
                        ape_gcmem_markobject(val);
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
                        if(val_data)
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
                function = ape_object_value_asfunction(obj);
                for(i = 0; i < function->numfreevals; i++)
                {
                    free_val = ape_object_function_getfreeval(obj, i);
                    ape_gcmem_markobject(free_val);
                    if(ape_object_value_isallocated(free_val))
                    {
                        free_val_data = ape_object_value_allocated_data(free_val);
                        if(!free_val_data->gcmark)
                        {
                            ape_gcmem_markobject(free_val);
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
    bool ok;
    ApeGCObjData_t* data;
    ApeGCObjPool_t* pool;
    ApeGCObjData_t** objs_temp;
    ape_gcmem_markobjlist((ApeObject_t*)ape_valarray_data(mem->objects_not_gced), ape_valarray_count(mem->objects_not_gced));
    APE_ASSERT(da_count(mem->backobjects) >= da_count(mem->frontobjects));
    da_clear(mem->backobjects);
    for(i = 0; i < da_count(mem->frontobjects); i++)
    {
        data = (ApeGCObjData_t*)da_get(mem->frontobjects, i);
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
                pool->datapool[pool->count] = data;
                pool->count++;
            }
            else
            {
                ape_object_data_deinit(data);
                if(mem->data_only_pool.count < APE_CONF_SIZE_GCMEM_POOLSIZE)
                {
                    mem->data_only_pool.datapool[mem->data_only_pool.count] = data;
                    mem->data_only_pool.count++;
                }
                else
                {
                    ape_allocator_free(mem->alloc, data);
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


