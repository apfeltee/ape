
#include "ape.h"


ApeValArray_t* ape_make_valarray_actual(ApeContext_t* ctx, ApeSize_t elsz)
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
    ok = ape_valarray_initcapacity(arr, ctx, capacity, elsz);
    if(!ok)
    {
        ape_allocator_free(&ctx->alloc, arr);
        return NULL;
    }
    return arr;
}

bool ape_valarray_initcapacity(ApeValArray_t* arr, ApeContext_t* ctx, ApeSize_t capacity, ApeSize_t elsz)
{
    arr->context = ctx;
    arr->alloc = &ctx->alloc;
    if(capacity > 0)
    {
        arr->allocdata = (unsigned char*)ape_allocator_alloc(arr->alloc, capacity * elsz);
        memset(arr->allocdata, 0, capacity * elsz);
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
    arr->elemsize = elsz;
    arr->lock_capacity = false;
    return true;
}

void ape_valarray_deinit(ApeValArray_t* arr)
{
    ape_allocator_free(arr->alloc, arr->allocdata);
}

void ape_valarray_destroy(ApeValArray_t* arr)
{
    ApeAllocator_t* alloc;
    if(!arr)
    {
        return;
    }
    alloc = arr->alloc;
    ape_valarray_deinit(arr);
    ape_allocator_free(alloc, arr);
}

ApeValArray_t* ape_valarray_copy(const ApeValArray_t* arr)
{
    ApeValArray_t* copy;
    copy = (ApeValArray_t*)ape_allocator_alloc(arr->alloc, sizeof(ApeValArray_t));
    if(!copy)
    {
        return NULL;
    }
    copy->alloc = arr->alloc;
    copy->capacity = arr->capacity;
    copy->count = arr->count;
    copy->elemsize = arr->elemsize;
    copy->lock_capacity = arr->lock_capacity;
    if(arr->allocdata)
    {
        copy->allocdata = (unsigned char*)ape_allocator_alloc(arr->alloc, arr->capacity * arr->elemsize);
        if(!copy->allocdata)
        {
            ape_allocator_free(arr->alloc, copy);
            return NULL;
        }
        copy->arraydata = copy->allocdata;
        memcpy(copy->allocdata, arr->arraydata, arr->capacity * arr->elemsize);
    }
    else
    {
        copy->allocdata = NULL;
        copy->arraydata = NULL;
    }

    return copy;
}

bool ape_valarray_add(ApeValArray_t* arr, const void* value)
{
    ApeSize_t newcap;
    unsigned char* newdata;
    if(arr->count >= arr->capacity)
    {
        APE_ASSERT(!arr->lock_capacity);
        if(arr->lock_capacity)
        {
            return false;
        }
        newcap = arr->capacity > 0 ? arr->capacity * 2 : 1;
        newdata = (unsigned char*)ape_allocator_alloc(arr->alloc, newcap * arr->elemsize);
        if(!newdata)
        {
            return false;
        }
        memcpy(newdata, arr->arraydata, arr->count * arr->elemsize);
        ape_allocator_free(arr->alloc, arr->allocdata);
        arr->allocdata = newdata;
        arr->arraydata = arr->allocdata;
        arr->capacity = newcap;
    }
    if(value)
    {
        memcpy(arr->arraydata + (arr->count * arr->elemsize), value, arr->elemsize);
    }
    arr->count++;
    return true;
}

bool ape_valarray_push(ApeValArray_t* arr, const void* value)
{
    return ape_valarray_add(arr, value);
}

bool ape_valarray_pop(ApeValArray_t* arr, void* out_value)
{
    void* res;
    if(arr->count <= 0)
    {
        return false;
    }
    if(out_value)
    {
        res = (void*)ape_valarray_get(arr, arr->count - 1);
        memcpy(out_value, res, arr->elemsize);
    }
    ape_valarray_removeat(arr, arr->count - 1);
    return true;
}

void* ape_valarray_top(ApeValArray_t* arr)
{
    if(arr->count <= 0)
    {
        return NULL;
    }
    return (void*)ape_valarray_get(arr, arr->count - 1);
}

bool ape_valarray_set(ApeValArray_t* arr, ApeSize_t ix, void* value)
{
    ApeSize_t offset;
    if(ix >= arr->count)
    {
        APE_ASSERT(false);
        return false;
    }
    offset = ix * arr->elemsize;
    memmove(arr->arraydata + offset, value, arr->elemsize);
    return true;
}


void* ape_valarray_get(ApeValArray_t* arr, ApeSize_t ix)
{
    ApeSize_t offset;
    if(ix >= arr->count)
    {
        APE_ASSERT(false);
        return NULL;
    }
    offset = ix * arr->elemsize;
    return arr->arraydata + offset;
}

ApeSize_t ape_valarray_count(const ApeValArray_t* arr)
{
    if(!arr)
    {
        return 0;
    }
    return arr->count;
}

bool ape_valarray_removeat(ApeValArray_t* arr, ApeSize_t ix)
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
    return true;
}

void ape_valarray_clear(ApeValArray_t* arr)
{
    arr->count = 0;
}

void* ape_valarray_data(ApeValArray_t* arr)
{
    return arr->arraydata;
}

void ape_valarray_orphandata(ApeValArray_t* arr)
{
    ape_valarray_initcapacity(arr, arr->context, 0, arr->elemsize);
}


//-----------------------------------------------------------------------------
// Pointer Array
//-----------------------------------------------------------------------------

ApePtrArray_t* ape_make_ptrarray(ApeContext_t* ctx)
{
    return ape_make_ptrarraycapacity(ctx, 0);
}

ApePtrArray_t* ape_make_ptrarraycapacity(ApeContext_t* ctx, ApeSize_t capacity)
{
    bool ok;
    ApePtrArray_t* ptrarr;
    ptrarr = (ApePtrArray_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApePtrArray_t));
    if(!ptrarr)
    {
        return NULL;
    }
    ptrarr->alloc = &ctx->alloc;
    ok = ape_valarray_initcapacity(&ptrarr->arr, ctx, capacity, sizeof(void*));
    if(!ok)
    {
        ape_allocator_free(&ctx->alloc, ptrarr);
        return NULL;
    }
    return ptrarr;
}

void ape_ptrarray_destroy(ApePtrArray_t* arr)
{
    if(!arr)
    {
        return;
    }
    ape_valarray_deinit(&arr->arr);
    ape_allocator_free(arr->alloc, arr);
}

// todo: destroy and copy in make fn
void ape_ptrarray_destroywithitems(ApePtrArray_t* arr, ApeDataCallback_t destroy_fn)
{
    if(arr == NULL)
    {
        return;
    }
    if(destroy_fn)
    {
        ape_ptrarray_clearanddestroyitems(arr, destroy_fn);
    }
    ape_ptrarray_destroy(arr);
}

ApePtrArray_t* ape_ptrarray_copywithitems(ApePtrArray_t* arr, ApeDataCallback_t copy_fn, ApeDataCallback_t destroy_fn)
{
    bool ok;
    ApeSize_t i;
    void* item;
    void* item_copy;
    ApePtrArray_t* arr_copy;
    arr_copy = ape_make_ptrarraycapacity(arr->context, arr->arr.capacity);
    if(!arr_copy)
    {
        return NULL;
    }
    for(i = 0; i < ape_ptrarray_count(arr); i++)
    {
        item = ape_ptrarray_get(arr, i);
        item_copy = copy_fn(item);
        if(item && !item_copy)
        {
            goto err;
        }
        ok = ape_ptrarray_add(arr_copy, item_copy);
        if(!ok)
        {
            goto err;
        }
    }
    return arr_copy;
err:
    ape_ptrarray_destroywithitems(arr_copy, (ApeDataCallback_t)destroy_fn);
    return NULL;
}

bool ape_ptrarray_add(ApePtrArray_t* arr, void* ptr)
{
    return ape_valarray_add(&arr->arr, &ptr);
}

void* ape_ptrarray_get(ApePtrArray_t* arr, ApeSize_t ix)
{
    void* res;
    res = ape_valarray_get(&arr->arr, ix);
    if(!res)
    {
        return NULL;
    }
    return *(void**)res;
}

bool ape_ptrarray_push(ApePtrArray_t* arr, void* ptr)
{
    return ape_ptrarray_add(arr, ptr);
}

void* ape_ptrarray_pop(ApePtrArray_t* arr)
{
    ApeSize_t ix;
    void* res;
    ix = ape_ptrarray_count(arr) - 1;
    res = ape_ptrarray_get(arr, ix);
    ape_ptrarray_removeat(arr, ix);
    return res;
}

void* ape_ptrarray_top(ApePtrArray_t* arr)
{
    ApeSize_t count;
    count = ape_ptrarray_count(arr);
    if(count == 0)
    {
        return NULL;
    }
    return ape_ptrarray_get(arr, count - 1);
}

ApeSize_t ape_ptrarray_count(const ApePtrArray_t* arr)
{
    if(!arr)
    {
        return 0;
    }
    return ape_valarray_count(&arr->arr);
}

bool ape_ptrarray_removeat(ApePtrArray_t* arr, ApeSize_t ix)
{
    return ape_valarray_removeat(&arr->arr, ix);
}

void ape_ptrarray_clear(ApePtrArray_t* arr)
{
    ape_valarray_clear(&arr->arr);
}

void ape_ptrarray_clearanddestroyitems(ApePtrArray_t* arr, ApeDataCallback_t destroy_fn)
{
    ApeSize_t i;
    void* item;
    for(i = 0; i < ape_ptrarray_count(arr); i++)
    {
        item = ape_ptrarray_get(arr, i);
        destroy_fn(item);
    }
    ape_ptrarray_clear(arr);
}

ApeObject_t ape_object_make_array(ApeContext_t* ctx)
{
    return ape_object_make_arraycapacity(ctx, 8);
}

ApeObject_t ape_object_make_arraycapacity(ApeContext_t* ctx, unsigned capacity)
{
    ApeObjData_t* data;
    data = ape_gcmem_getfrompool(ctx->vm->mem, APE_OBJECT_ARRAY);
    if(data)
    {
        ape_valarray_clear(data->valarray);
        return object_make_from_data(ctx, APE_OBJECT_ARRAY, data);
    }
    data = ape_gcmem_allocobjdata(ctx->vm->mem, APE_OBJECT_ARRAY);
    if(!data)
    {
        return ape_object_make_null(ctx);
    }
    data->valarray = ape_make_valarraycapacity(ctx, capacity, sizeof(ApeObject_t));
    if(!data->valarray)
    {
        return ape_object_make_null(ctx);
    }
    return object_make_from_data(ctx, APE_OBJECT_ARRAY, data);
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
    return ape_valarray_add(array, &val);
}

//bool ape_valarray_pop(ApeValArray_t *arr, void *out_value);

bool ape_object_array_popvalue(ApeObject_t object, ApeObject_t* dest)
{

    ApeValArray_t* array;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_ARRAY);
    array = ape_object_array_getarray(object);
    return ape_valarray_pop(array, dest);
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

bool ape_object_array_pushstring(ApeObject_t obj, const char* string)
{
    ApeObject_t new_value;
    ApeGCMemory_t* mem;
    mem = ape_object_value_getmem(obj);
    if(!mem)
    {
        return false;
    }
    new_value = ape_object_make_string(mem->context, string);
    return ape_object_array_pushvalue(obj, new_value);
}


ApeValArray_t * ape_object_array_getarray(ApeObject_t object)
{
    ApeObjData_t* data;
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
    return ape_object_make_number(vm->context, ape_object_array_getlength(self));
}

static ApeObject_t objfn_array_push(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    size_t i;
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
        /* TODO: implement me! */
        #if 0
        //{"", true, objfn_array_},
        {"map", true, objfn_array_map},
        {"sort", true, objfn_array_sort},
        {"grep", true, objfn_array_grep},

        // pseudo funcs
        {"first", false, objfn_array_first},
        {"last", false, objfn_array_first},
        #endif
        {NULL, false, NULL},
    };
    psc = ape_context_make_pseudoclass(vm->context, vm->context->objarrayfuncs, APE_OBJECT_ARRAY, classname);
    for(i=0; memberfuncs[i].name != NULL; i++)
    {
        //ape_strdict_set(vm->context->objarrayfuncs, classname, memberfuncs[i].name, &memberfuncs[i]);
        ape_pseudoclass_setmethod(psc, memberfuncs[i].name, &memberfuncs[i]);
    }
}

