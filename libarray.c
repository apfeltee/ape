
#include "ape.h"


ApeArray_t* array_make_(ApeContext_t* ctx, ApeSize_t element_size)
{
    return array_makecapacity(ctx, 32, element_size);
}

ApeArray_t* array_makecapacity(ApeContext_t* ctx, ApeSize_t capacity, ApeSize_t element_size)
{
    bool ok;
    ApeArray_t* arr;
    arr = (ApeArray_t*)allocator_malloc(&ctx->alloc, sizeof(ApeArray_t));
    if(!arr)
    {
        return NULL;
    }
    ok = array_initcapacity(arr, ctx, capacity, element_size);
    if(!ok)
    {
        allocator_free(&ctx->alloc, arr);
        return NULL;
    }
    return arr;
}


bool array_initcapacity(ApeArray_t* arr, ApeContext_t* ctx, ApeSize_t capacity, ApeSize_t element_size)
{
    arr->context = ctx;
    arr->alloc = &ctx->alloc;
    if(capacity > 0)
    {
        arr->data_allocated = (unsigned char*)allocator_malloc(arr->alloc, capacity * element_size);
        memset(arr->data_allocated, 0, capacity * element_size);
        arr->arraydata = arr->data_allocated;
        if(!arr->data_allocated)
        {
            return false;
        }
    }
    else
    {
        arr->data_allocated = NULL;
        arr->arraydata = NULL;
    }
    arr->capacity = capacity;
    arr->count = 0;
    arr->element_size = element_size;
    arr->lock_capacity = false;
    return true;
}

void array_deinit(ApeArray_t* arr)
{
    allocator_free(arr->alloc, arr->data_allocated);
}

void array_destroy(ApeArray_t* arr)
{
    ApeAllocator_t* alloc;
    if(!arr)
    {
        return;
    }
    alloc = arr->alloc;
    array_deinit(arr);
    allocator_free(alloc, arr);
}

ApeArray_t* array_copy(const ApeArray_t* arr)
{
    ApeArray_t* copy;
    copy = (ApeArray_t*)allocator_malloc(arr->alloc, sizeof(ApeArray_t));
    if(!copy)
    {
        return NULL;
    }
    copy->alloc = arr->alloc;
    copy->capacity = arr->capacity;
    copy->count = arr->count;
    copy->element_size = arr->element_size;
    copy->lock_capacity = arr->lock_capacity;
    if(arr->data_allocated)
    {
        copy->data_allocated = (unsigned char*)allocator_malloc(arr->alloc, arr->capacity * arr->element_size);
        if(!copy->data_allocated)
        {
            allocator_free(arr->alloc, copy);
            return NULL;
        }
        copy->arraydata = copy->data_allocated;
        memcpy(copy->data_allocated, arr->arraydata, arr->capacity * arr->element_size);
    }
    else
    {
        copy->data_allocated = NULL;
        copy->arraydata = NULL;
    }

    return copy;
}

bool array_add(ApeArray_t* arr, const void* value)
{
    ApeSize_t new_capacity;
    unsigned char* new_data;
    if(arr->count >= arr->capacity)
    {
        COLLECTIONS_ASSERT(!arr->lock_capacity);
        if(arr->lock_capacity)
        {
            return false;
        }

        new_capacity = arr->capacity > 0 ? arr->capacity * 2 : 1;
        new_data = (unsigned char*)allocator_malloc(arr->alloc, new_capacity * arr->element_size);
        if(!new_data)
        {
            return false;
        }
        memcpy(new_data, arr->arraydata, arr->count * arr->element_size);
        allocator_free(arr->alloc, arr->data_allocated);
        arr->data_allocated = new_data;
        arr->arraydata = arr->data_allocated;
        arr->capacity = new_capacity;
    }
    if(value)
    {
        memcpy(arr->arraydata + (arr->count * arr->element_size), value, arr->element_size);
    }
    arr->count++;
    return true;
}

bool array_push(ApeArray_t* arr, const void* value)
{
    return array_add(arr, value);
}

bool array_pop(ApeArray_t* arr, void* out_value)
{
    void* res;
    if(arr->count <= 0)
    {
        return false;
    }
    if(out_value)
    {
        res = (void*)array_get(arr, arr->count - 1);
        memcpy(out_value, res, arr->element_size);
    }
    array_removeat(arr, arr->count - 1);
    return true;
}

void* array_top(ApeArray_t* arr)
{
    if(arr->count <= 0)
    {
        return NULL;
    }
    return (void*)array_get(arr, arr->count - 1);
}

bool array_set(ApeArray_t* arr, ApeSize_t ix, void* value)
{
    ApeSize_t offset;
    if(ix >= arr->count)
    {
        COLLECTIONS_ASSERT(false);
        return false;
    }
    offset = ix * arr->element_size;
    memmove(arr->arraydata + offset, value, arr->element_size);
    return true;
}


void* array_get(ApeArray_t* arr, ApeSize_t ix)
{
    ApeSize_t offset;
    if(ix >= arr->count)
    {
        COLLECTIONS_ASSERT(false);
        return NULL;
    }
    offset = ix * arr->element_size;
    return arr->arraydata + offset;
}

ApeSize_t array_count(const ApeArray_t* arr)
{
    if(!arr)
    {
        return 0;
    }
    return arr->count;
}

bool array_removeat(ApeArray_t* arr, ApeSize_t ix)
{
    ApeSize_t to_move_bytes;
    void* dest;
    void* src;
    if(ix >= arr->count)
    {
        return false;
    }
    if(ix == 0)
    {
        arr->arraydata += arr->element_size;
        arr->capacity--;
        arr->count--;
        return true;
    }
    if(ix == (arr->count - 1))
    {
        arr->count--;
        return true;
    }
    to_move_bytes = (arr->count - 1 - ix) * arr->element_size;
    dest = arr->arraydata + (ix * arr->element_size);
    src = arr->arraydata + ((ix + 1) * arr->element_size);
    memmove(dest, src, to_move_bytes);
    arr->count--;
    return true;
}

void array_clear(ApeArray_t* arr)
{
    arr->count = 0;
}

void* array_data(ApeArray_t* arr)
{
    return arr->arraydata;
}

void array_orphandata(ApeArray_t* arr)
{
    array_initcapacity(arr, arr->context, 0, arr->element_size);
}


//-----------------------------------------------------------------------------
// Pointer Array
//-----------------------------------------------------------------------------

ApePtrArray_t* ptrarray_make(ApeContext_t* ctx)
{
    return ptrarray_makecapacity(ctx, 0);
}

ApePtrArray_t* ptrarray_makecapacity(ApeContext_t* ctx, ApeSize_t capacity)
{
    bool ok;
    ApePtrArray_t* ptrarr;
    ptrarr = (ApePtrArray_t*)allocator_malloc(&ctx->alloc, sizeof(ApePtrArray_t));
    if(!ptrarr)
    {
        return NULL;
    }
    ptrarr->alloc = &ctx->alloc;
    ok = array_initcapacity(&ptrarr->arr, ctx, capacity, sizeof(void*));
    if(!ok)
    {
        allocator_free(&ctx->alloc, ptrarr);
        return NULL;
    }
    return ptrarr;
}

void ptrarray_destroy(ApePtrArray_t* arr)
{
    if(!arr)
    {
        return;
    }
    array_deinit(&arr->arr);
    allocator_free(arr->alloc, arr);
}

// todo: destroy and copy in make fn
void ptrarray_destroywithitems(ApePtrArray_t* arr, ApeDataCallback_t destroy_fn)
{
    if(arr == NULL)
    {
        return;
    }
    if(destroy_fn)
    {
        ptrarray_clearanddestroyitems(arr, destroy_fn);
    }
    ptrarray_destroy(arr);
}

ApePtrArray_t* ptrarray_copywithitems(ApePtrArray_t* arr, ApeDataCallback_t copy_fn, ApeDataCallback_t destroy_fn)
{
    bool ok;
    ApeSize_t i;
    void* item;
    void* item_copy;
    ApePtrArray_t* arr_copy;
    arr_copy = ptrarray_makecapacity(arr->context, arr->arr.capacity);
    if(!arr_copy)
    {
        return NULL;
    }
    for(i = 0; i < ptrarray_count(arr); i++)
    {
        item = ptrarray_get(arr, i);
        item_copy = copy_fn(item);
        if(item && !item_copy)
        {
            goto err;
        }
        ok = ptrarray_add(arr_copy, item_copy);
        if(!ok)
        {
            goto err;
        }
    }
    return arr_copy;
err:
    ptrarray_destroywithitems(arr_copy, (ApeDataCallback_t)destroy_fn);
    return NULL;
}

bool ptrarray_add(ApePtrArray_t* arr, void* ptr)
{
    return array_add(&arr->arr, &ptr);
}

void* ptrarray_get(ApePtrArray_t* arr, ApeSize_t ix)
{
    void* res;
    res = array_get(&arr->arr, ix);
    if(!res)
    {
        return NULL;
    }
    return *(void**)res;
}

bool ptrarray_push(ApePtrArray_t* arr, void* ptr)
{
    return ptrarray_add(arr, ptr);
}

void* ptrarray_pop(ApePtrArray_t* arr)
{
    ApeSize_t ix;
    void* res;
    ix = ptrarray_count(arr) - 1;
    res = ptrarray_get(arr, ix);
    ptrarray_removeat(arr, ix);
    return res;
}

void* ptrarray_top(ApePtrArray_t* arr)
{
    ApeSize_t count;
    count = ptrarray_count(arr);
    if(count == 0)
    {
        return NULL;
    }
    return ptrarray_get(arr, count - 1);
}

ApeSize_t ptrarray_count(const ApePtrArray_t* arr)
{
    if(!arr)
    {
        return 0;
    }
    return array_count(&arr->arr);
}

bool ptrarray_removeat(ApePtrArray_t* arr, ApeSize_t ix)
{
    return array_removeat(&arr->arr, ix);
}

void ptrarray_clear(ApePtrArray_t* arr)
{
    array_clear(&arr->arr);
}

void ptrarray_clearanddestroyitems(ApePtrArray_t* arr, ApeDataCallback_t destroy_fn)
{
    ApeSize_t i;
    void* item;
    for(i = 0; i < ptrarray_count(arr); i++)
    {
        item = ptrarray_get(arr, i);
        destroy_fn(item);
    }
    ptrarray_clear(arr);
}

ApeObject_t object_make_array(ApeGCMemory_t* mem)
{
    return object_make_arraycapacity(mem, 8);
}

ApeObject_t object_make_arraycapacity(ApeGCMemory_t* mem, unsigned capacity)
{
    ApeObjectData_t* data;
    data = gcmem_get_object_data_from_pool(mem, APE_OBJECT_ARRAY);
    if(data)
    {
        array_clear(data->array);
        return object_make_from_data(APE_OBJECT_ARRAY, data);
    }
    data = gcmem_alloc_object_data(mem, APE_OBJECT_ARRAY);
    if(!data)
    {
        return object_make_null();
    }
    data->array = array_makecapacity(mem->context, capacity, sizeof(ApeObject_t));
    if(!data->array)
    {
        return object_make_null();
    }
    return object_make_from_data(APE_OBJECT_ARRAY, data);
}


ApeObject_t object_array_getvalue(ApeObject_t object, ApeSize_t ix)
{
    ApeObject_t* res;
    ApeArray_t* array;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_ARRAY);
    array = object_array_getarray(object);
    if(ix >= array_count(array))
    {
        return object_make_null();
    }
    res = (ApeObject_t*)array_get(array, ix);
    if(!res)
    {
        return object_make_null();
    }
    return *res;
}

/*
* TODO: since this pushes NULLs before 'ix' if 'ix' is out of bounds, this
* may be possibly extremely inefficient.
*/
bool object_array_setat(ApeObject_t object, ApeInt_t ix, ApeObject_t val)
{
    ApeArray_t* array;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_ARRAY);
    array = object_array_getarray(object);
    if(ix < 0 || ix >= (ApeInt_t)array_count(array))
    {
        if(ix < 0)
        {
            return false;
        }
        while(ix >= (ApeInt_t)array_count(array))
        {
            object_array_pushvalue(object, object_make_null());
        }
    }
    return array_set(array, ix, &val);
}

bool object_array_pushvalue(ApeObject_t object, ApeObject_t val)
{
    ApeArray_t* array;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_ARRAY);
    array = object_array_getarray(object);
    return array_add(array, &val);
}

ApeSize_t object_array_getlength(ApeObject_t object)
{
    ApeArray_t* array;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_ARRAY);
    array = object_array_getarray(object);
    return array_count(array);
}

bool object_array_removeat(ApeObject_t object, ApeInt_t ix)
{
    ApeArray_t* array;
    array = object_array_getarray(object);
    return array_removeat(array, ix);
}

bool object_array_pushstring(ApeObject_t obj, const char* string)
{
    ApeObject_t new_value;
    ApeGCMemory_t* mem;
    mem = object_value_getmem(obj);
    if(!mem)
    {
        return false;
    }
    new_value = object_make_string(mem, string);
    return object_array_pushvalue(obj, new_value);
}


ApeArray_t * object_array_getarray(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_ARRAY);
    data = object_value_allocated_data(object);
    return data->array;
}



