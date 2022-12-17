
#include "inline.h"

#define APE_CONF_ARRAY_INITIAL_CAPACITY (64/4)

#define USE_DNARRAY 0

struct ApeValArray_t
{
    ApeContext_t* context;
    ApeSize_t elemsize;

    #if defined(USE_DNARRAY) && (USE_DNARRAY == 1)
        //intptr_t* arraydata;
        DequeList_t* arraydata;
    #else
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
    arr->context = ctx;
    arr->elemsize = elsz;
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
    arr->elemsize = elsz;
    #if defined(USE_DNARRAY) && (USE_DNARRAY == 1)
        arr->arraydata = deqlist_create(elsz, capacity);
    #else
        if(capacity > 0)
        {
            arr->allocdata = (unsigned char*)ape_allocator_alloc(&ctx->alloc, capacity * elsz);
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

        arr->lock_capacity = false;
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
        #if defined(USE_DNARRAY) && (USE_DNARRAY == 1)
            deqlist_destroy(arr->arraydata);
        #else
            ape_allocator_free(&ctx->alloc, arr->allocdata);
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
    #if defined(USE_DNARRAY) && (USE_DNARRAY == 1)
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
    #if defined(USE_DNARRAY) && (USE_DNARRAY == 1)
        return arr->arraydata->capacity;
    #else
        return arr->capacity;
    #endif
}


ApeSize_t ape_valarray_size(ApeValArray_t* arr)
{
    return ape_valarray_count(arr);
}

ApeValArray_t* ape_valarray_copy(ApeValArray_t* arr)
{
    ApeValArray_t* copy;
    copy = (ApeValArray_t*)ape_allocator_alloc(&arr->context->alloc, sizeof(ApeValArray_t));
    if(!copy)
    {
        return NULL;
    }
    copy->context = arr->context;
    copy->elemsize = arr->elemsize;
    #if defined(USE_DNARRAY) && (USE_DNARRAY == 1)
        copy->arraydata = deqlist_clone(arr->arraydata);
    #else
        copy->capacity = arr->capacity;
        copy->count = arr->count;
        copy->lock_capacity = arr->lock_capacity;
        if(arr->allocdata)
        {
            copy->allocdata = (unsigned char*)ape_allocator_alloc(&arr->context->alloc, arr->capacity * arr->elemsize);
            if(!copy->allocdata)
            {
                ape_allocator_free(&arr->context->alloc, copy);
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
    #endif
    return copy;
}


bool ape_valarray_set(ApeValArray_t* arr, ApeSize_t ix, void* value)
{
    #if defined(USE_DNARRAY) && (USE_DNARRAY == 1)
        deqlist_set(arr->arraydata, ix, value);
    #else
        ApeSize_t cnt;
        ApeSize_t offset;
        cnt = ape_valarray_count(arr);
        if(ix >= cnt)
        {
            APE_ASSERT(false);
            return false;
        }
        #if 1
            offset = ix * arr->elemsize;    
            memmove(arr->arraydata + offset, value, arr->elemsize);
        #else
            memmove(&arr->arraydata[arr->count + arr->elemsize + ix], value, arr->elemsize);
        #endif
    #endif
    return true;
}

void* ape_valarray_get(ApeValArray_t* arr, ApeSize_t ix)
{
    #if defined(USE_DNARRAY) && (USE_DNARRAY == 1)
        void* p = deqlist_get(arr->arraydata, ix);
        //fprintf(stderr, "ape_valarray_get(ix=%d) = %p\n", ix, p);
        return p;
    #else
        ApeSize_t offset;
        if(ix >= arr->count)
        {
            APE_ASSERT(false);
            return NULL;
        }
        offset = ix * arr->elemsize;
        return arr->arraydata + offset;
    #endif
}

bool ape_valarray_push(ApeValArray_t* arr, void* value)
{
    #if defined(USE_DNARRAY) && (USE_DNARRAY == 1)
        deqlist_push(&arr->arraydata, value);
    #else
        ApeSize_t initcap;
        ApeSize_t oldcap;
        ApeSize_t newcap;
        ApeSize_t toalloc;

        ApeSize_t elmsz;
        ApeSize_t acnt;
        (void)initcap;
        unsigned char* newdata;
        elmsz = arr->elemsize;
        oldcap = arr->capacity;
        acnt = arr->count;
        newcap = 0;
        initcap = oldcap;
        if(oldcap > 0)
        {

            newcap = (oldcap * 2);
            //newcap = (oldcap + ((elmsz * 8) + 1));
            
        }
        else
        {
            newcap = 1;
        }
        toalloc = (newcap * elmsz);


        //fprintf(stderr, "valarray_add: toalloc=%d\n", toalloc);

        if(acnt >= oldcap)
        {
            APE_ASSERT(!arr->lock_capacity);
            if(arr->lock_capacity)
            {
                return false;
            }
            newdata = (unsigned char*)ape_allocator_alloc(&arr->context->alloc, toalloc+1);
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
                /*
                //void *memmove(void *dest, const void *src, size_t n);
                //void *memcpy(void *dest, const void *src, size_t n);
                */
                memmove(newdata, arr->arraydata, acnt*elmsz);
            }

            ape_allocator_free(&arr->context->alloc, arr->allocdata);
            arr->allocdata = newdata;
            arr->arraydata = arr->allocdata;
            arr->capacity = newcap;
        }
        arr->count++;
        if(value)
        {
            //memcpy(arr->arraydata + ((acnt-0) * elmsz), value, elmsz);
            //arr->arraydata[arr->count] = value;
            ape_valarray_set(arr, arr->count-1, value);
        }
    #endif
    return true;
}

bool ape_valarray_removeat(ApeValArray_t* arr, ApeSize_t ix)
{
    ApeSize_t cnt;
    (void)cnt;
    cnt = ape_valarray_count(arr);

    //fprintf(stderr, "ape_valarray_removeat: cnt=%ld ix=%ld\n", cnt, ix);


    #if defined(USE_DNARRAY) && (USE_DNARRAY == 1)

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
    #if defined(USE_DNARRAY) && (USE_DNARRAY == 1)
        deqlist_clear(arr->arraydata, 0, ape_valarray_size(arr));
    #else
        arr->count = 0;
    #endif
}

void* ape_valarray_data(ApeValArray_t* arr)
{
    //assert(0);
    #if defined(USE_DNARRAY) && (USE_DNARRAY == 1)
        return arr->arraydata->elemdata;
    #else
        return arr->arraydata;
    #endif
}

void ape_valarray_orphandata(ApeValArray_t* arr)
{
    ape_valarray_initcapacity(arr, arr->context, 0, arr->elemsize);
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

    #if defined(USE_DNARRAY) && (USE_DNARRAY == 1)
        deqlist_pop(&arr->arraydata);
    #else
        ape_valarray_removeat(arr, cnt - 1);
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

ApePtrArray_t* ape_make_ptrarray(ApeContext_t* ctx)
{
    return ape_make_ptrarraycapacity(ctx, 0);
}

ApePtrArray_t* ape_make_ptrarraycapacity(ApeContext_t* ctx, ApeSize_t capacity)
{
    bool ok;
    ApePtrArray_t* ptrarr;
    ok = true;
    ptrarr = (ApePtrArray_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApePtrArray_t));
    if(!ptrarr)
    {
        return NULL;
    }
    ptrarr->context = ctx;
    //ok = ape_valarray_initcapacity(ptrarr->arr, ctx, capacity, sizeof(void*));
    ptrarr->arr = ape_make_valarraycapacity(ctx, capacity, sizeof(void*));
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
    arr_copy = ape_make_ptrarraycapacity(arr->context, ape_valarray_capacity(arr->arr));
    if(!arr_copy)
    {
        return NULL;
    }
    arr_copy->context = arr->context;
    for(i = 0; i < ape_ptrarray_count(arr); i++)
    {
        item = ape_ptrarray_get(arr, i);
        item_copy = copy_fn(item);
        if(item && !item_copy)
        {
            goto err;
        }
        ok = ape_ptrarray_push(arr_copy, &item_copy);
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
    return ape_ptrarray_get(arr, count - 1);

}

bool ape_ptrarray_push(ApePtrArray_t* arr, void* ptr)
{
    return ape_valarray_push(arr->arr, ptr);
}

void* ape_ptrarray_get(ApePtrArray_t* arr, ApeSize_t ix)
{
    void* res;
    res = ape_valarray_get(arr->arr, ix);
    if(!res)
    {
        return NULL;
    }
    #if defined(USE_DNARRAY) && (USE_DNARRAY == 1)
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
    return ape_object_make_number(vm->context, ape_object_array_getlength(self));
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
    if(!ape_args_check(&check, 0, APE_OBJECT_NUMBER))
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

