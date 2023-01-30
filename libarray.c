
#include "inline.h"

//#define APE_CONF_ARRAY_INITIAL_CAPACITY (64/4)
#define APE_CONF_ARRAY_INITIAL_CAPACITY 2

#define DEBUG 0

static ApeSize g_callcount = 0;
static ApeSize g_arrayident = 0;

struct ApeValArray
{
    ApeSize ident;
    ApeContext* context;
    ApeSize elemsize;
    unsigned char* arraydata;
    unsigned char* allocdata;
    ApeSize count;
    ApeSize capacity;
    bool lock_capacity;
};

struct ApePtrArray
{
    ApeContext* context;
    ApeValArray* arr;
};

#if defined(__GNUC__)
static inline void debugmsg(void* self, const char* name, const char* fmt, ...)
 __attribute__((format(printf, 3, 4)));
#endif

static inline void debugmsgv(ApeValArray* self, const char* name, const char* fmt, va_list va)
{
    ApeSize cap;
    ApeSize cnt;
    ApeSize elemsz;
    elemsz = self->elemsize;
    cap = self->capacity;
    cnt = self->count;
    fprintf(stderr, "--%04zd: %-17s in <%zd:elemsize=%zd count=%zd capacity=%zd>:", g_callcount, name, self->ident, elemsz, cnt, cap);
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

ApeValArray* ape_make_valarray(ApeContext* ctx, ApeSize elsz)
{
    return ape_make_valarraycapacity(ctx, APE_CONF_ARRAY_INITIAL_CAPACITY, elsz);
}

ApeValArray* ape_make_valarraycapacity(ApeContext* ctx, ApeSize capacity, ApeSize elsz)
{
    bool ok;
    ApeValArray* arr;
    arr = (ApeValArray*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeValArray));
    if(!arr)
    {
        return NULL;
    }
    arr->ident = g_arrayident;
    arr->context = ctx;
    arr->elemsize = elsz;
    arr->arraydata = NULL;
    g_arrayident++;
    ok = ape_valarray_init(ctx, arr, capacity);
    if(!ok)
    {
        ape_allocator_free(&ctx->alloc, arr);
        return NULL;
    }
    return arr;
}

bool ape_valarray_init(ApeContext* ctx, ApeValArray* arr, ApeSize capacity)
{
    arr->context = ctx;
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

    #if defined(DEBUG) && (DEBUG == 1)
        debugmsg(arr, "ape_valarray_init", "capacity=%zd elsz=%zd", capacity, arr->elemsize);
    #endif

    return true;
}

void ape_valarray_deinit(ApeValArray* arr)
{
    ApeContext* ctx;
    (void)ctx;
    if(arr)
    {
        ctx = arr->context;
        if(arr->allocdata != NULL)
        {
            ape_allocator_free(&ctx->alloc, arr->allocdata);
            arr->allocdata = NULL;
        }
    }
}

void ape_valarray_destroy(ApeValArray* arr)
{
    ApeContext* ctx;
    if(!arr)
    {
        return;
    }
    ctx = arr->context;
    ape_valarray_deinit(arr);
    ape_allocator_free(&ctx->alloc, arr);
}

ApeSize ape_valarray_count(ApeValArray* arr)
{
    if(arr == NULL)
    {
        return 0;
    }
    return arr->count;
}

ApeSize ape_valarray_capacity(ApeValArray* arr)
{
    if(!arr)
    {
        return 0;
    }
    return arr->capacity;
}

ApeSize ape_valarray_size(ApeValArray* arr)
{
    return ape_valarray_count(arr);
}

bool ape_valarray_canappend(ApeValArray* arr)
{
    ApeSize cap;
    ApeSize acnt;
    ApeSize elmsz;
    ApeSize hereidx;
    ApeSize needed;
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
    return false;
}

bool ape_valarray_push(ApeValArray* arr, void* value)
{
    bool ok;
    ApeContext* ctx;
    ApeSize cnt;
    ctx = arr->context;
    cnt = ape_valarray_count(arr);
    if(cnt > 0)
    {
        cnt = cnt - 1;
    }
    #if defined(DEBUG) && (DEBUG == 1)
        debugmsg(arr, "ape_valarray_push", "ix=%zd p=%p", cnt, value);
    #endif
    ApeSize idx;
    ApeSize initcap;
    ApeSize tmpcap;
    ApeSize oldcap;
    ApeSize newcap;
    ApeSize prevalloc;
    ApeSize toalloc;
    ApeSize elmsz;
    ApeSize acnt;
    (void)initcap;
    elmsz = arr->elemsize;
    oldcap = arr->capacity;
    acnt = arr->count;
    newcap = 0;
    initcap = oldcap;
    #if 1
        idx = acnt + 0;
        ok = ape_valarray_setcheck(arr, idx, value, false);
        if(ok)
        {
            arr->count++;
            return ok;
        }
    #endif
    /*
    reserve:
        if(*length + 1 > *capacity)
        {
            void* ptr;
            int n = (*capacity == 0) ? 1 : *capacity << 1;
            ptr = ds_extrealloc(*data, *capacity, n * memsz, uptr);
            if(ptr == NULL)
                return -1;
            *data = ptr;
            *capacity = n;
        }

    expand:
        if(*length + 1 > *capacity)
        {
            void* ptr;
            int n = (*capacity == 0) ? 1 : *capacity << 1;
            ptr = ds_extrealloc(*data, *capacity, n * memsz, uptr);
            if(ptr == NULL)
                return -1;
            *data = ptr;
            *capacity = n;
        }

    */
    if((arr->count + 1) > arr->capacity)
    {
        prevalloc = oldcap * elmsz;
        tmpcap = arr->capacity;
        if(tmpcap == 0)
        {
            newcap = 1;
        }
        else
        {
            newcap = (tmpcap << 1);
            //newcap = (tmpcap + 8);
        }
        toalloc = (newcap + 0) * elmsz;

        //fprintf(stderr, "tmpcap=%zu newcap=%zd toalloc=%zd\n", tmpcap, newcap, toalloc);
        arr->allocdata = ape_allocator_realloc(&ctx->alloc, arr->allocdata, prevalloc, toalloc);
        arr->arraydata = arr->allocdata;
        arr->capacity = newcap;
    }
    arr->count++;
    if(value)
    {
        ape_valarray_set(arr, arr->count-1, value);
    }

    return true;
}

void* ape_valarray_get(ApeValArray* arr, ApeSize ix)
{
    void* raw;
    void* tmp;
    void* rtp;
    (void)raw;
    (void)tmp;
    if(ix >= ape_valarray_count(arr))
    {
        APE_ASSERT(false);
        return NULL;
    }

    ApeSize offset;
    offset = (ix * arr->elemsize);
    if(ix == 0)
    {
        offset = 0;
    }
    rtp = &arr->arraydata[offset];

    #if defined(DEBUG) && (DEBUG == 1)
        debugmsg(arr, "ape_valarray_get", "ix=%zd rtp=%p", ix, rtp);
    #endif
    return rtp;
}

bool ape_valarray_set(ApeValArray* arr, ApeSize ix, void* value)
{
    return ape_valarray_setcheck(arr, ix, value, true);
}

bool ape_valarray_setcheck(ApeValArray* arr, ApeSize ix, void* value, bool check)
{
    bool ok;
    #if defined(DEBUG) && (DEBUG == 1)
        debugmsg(arr, "ape_valarray_set", "ix=%zd p=%p", ix, value);
    #endif

    ApeSize cnt;
    ApeSize elmsz;
    ApeSize offset;
    ok = true;
    if(arr == NULL)
    {
        return false;
    }
    if(arr->arraydata == NULL)
    {
        return false;
    }
    if(ix >= arr->capacity)
    {
        return false;
    }
    elmsz = arr->elemsize;
    cnt = ape_valarray_count(arr);
    if(((ix > 0) && (cnt > 0)) && (ix >= cnt))
    {
        if(check)
        {
            return false;
        }
        else
        {
            if(ix >= arr->capacity)
            {
                return false;
            }
        }
    }
    offset = ix * elmsz;
    if(ix == 0)
    {
        offset = 0;
    }
    memmove(arr->arraydata + offset, value, elmsz);

    return ok;
}

void* ape_valarray_pop(ApeValArray* arr)
{
    void* res;
    ApeSize cnt;

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
    ape_valarray_removeat(arr, cnt - 1);
    #if defined(DEBUG) && (DEBUG == 1)
        debugmsg(arr, "ape_valarray_pop", "ix=%zd p=%p", cnt-1, res);
    #endif
    return res;
}

bool ape_valarray_popinto(ApeValArray* arr, void* out_value)
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

void* ape_valarray_top(ApeValArray* arr)
{
    ApeSize cnt;
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


ApeValArray* ape_valarray_copy(ApeContext* ctx, ApeValArray* arr)
{
    ApeSize i;
    ApeSize elemsz;
    ApeValArray* copy;
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
        }
    }
    return copy;
}

bool ape_valarray_removeat(ApeValArray* arr, ApeSize ix)
{
    ApeSize cnt;
    (void)cnt;
    cnt = ape_valarray_count(arr);

    #if defined(DEBUG) && (DEBUG == 1)
        debugmsg(arr, "ape_valarray_removeat", "ix=%zd", ix);
    #endif

    ApeSize to_move_bytes;
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

void ape_valarray_clear(ApeValArray* arr)
{
    ApeSize sz;
    sz = ape_valarray_size(arr);
    if(sz > 0)
    {
        arr->count = 0;
    }
}

void* ape_valarray_data(ApeValArray* arr)
{
    #if defined(DEBUG) && (DEBUG == 1)
        debugmsg(arr, "ape_valarray_data", "%s", "");
    #endif
    return arr->arraydata;
}

void ape_valarray_reset(ApeValArray* arr)
{
    //fprintf(stderr, "!!!!in reset!!!!\n");
    ape_valarray_init(arr->context, arr, 0);
}

ApePtrArray* ape_make_ptrarray(ApeContext* ctx)
{
    return ape_make_ptrarraycapacity(ctx, 0);
}

ApePtrArray* ape_make_ptrarraycapacity(ApeContext* ctx, ApeSize capacity)
{
    bool ok;
    ApeSize psz;
    ApePtrArray* ptrarr;
    ok = true;
    psz = sizeof(void*);
    ptrarr = (ApePtrArray*)ape_allocator_alloc(&ctx->alloc, sizeof(ApePtrArray));
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

void ape_ptrarray_destroy(ApePtrArray* arr)
{
    ApeContext* ctx;
    if(!arr)
    {
        return;
    }
    ctx = arr->context;
    ape_valarray_destroy(arr->arr);
    ape_allocator_free(&ctx->alloc, arr);
}

/* todo: destroy and copy in make fn */
void ape_ptrarray_destroywithitems(ApeContext* ctx, ApePtrArray* arr, ApeDataCallback destroyfn)
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


void ape_ptrarray_clearanddestroyitems(ApeContext* ctx, ApePtrArray* arr, ApeDataCallback destroyfn)
{
    ApeSize i;
    void* curritem;
    for(i = 0; i < ape_ptrarray_count(arr); i++)
    {
        curritem = ape_ptrarray_get(arr, i);
        destroyfn(ctx, curritem);
    }
    ape_ptrarray_clear(arr);
}


ApePtrArray* ape_ptrarray_copywithitems(ApeContext* ctx, ApePtrArray* arr, ApeDataCallback copyfn, ApeDataCallback destroyfn)
{
    bool ok;
    ApeSize i;
    void* curritem;
    void* copieditem;
    ApePtrArray* copy;
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
            copieditem = copyfn(ctx, curritem);
        }
        if(curritem && !copieditem)
        {
            goto err;
        }
        ok = ape_ptrarray_push(copy, &copieditem);
        if(!ok)
        {
            goto err;
        }
    }
    return copy;
err:
    ape_ptrarray_destroywithitems(ctx, copy, (ApeDataCallback)destroyfn);
    return NULL;
}

ApeSize ape_ptrarray_count(ApePtrArray* arr)
{
    if(arr == NULL)
    {
        return 0;
    }
    return ape_valarray_count(arr->arr);
}

void* ape_ptrarray_pop(ApePtrArray* arr)
{
    return ape_valarray_pop(arr->arr);
}

void* ape_ptrarray_top(ApePtrArray* arr)
{
    ApeSize count;
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

bool ape_ptrarray_push(ApePtrArray* arr, void* ptr)
{
    return ape_valarray_push(arr->arr, ptr);
}

void* ape_ptrarray_get(ApePtrArray* arr, ApeSize ix)
{
    ApeSize cnt;
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
    return *(void**)res;
}

bool ape_ptrarray_removeat(ApePtrArray* arr, ApeSize ix)
{
    return ape_valarray_removeat(arr->arr, ix);
}

void ape_ptrarray_clear(ApePtrArray* arr)
{
    ape_valarray_clear(arr->arr);
}

ApeObject ape_object_make_array(ApeContext* ctx)
{
    return ape_object_make_arraycapacity(ctx, 8);
}

ApeObject ape_object_make_arraycapacity(ApeContext* ctx, unsigned capacity)
{
    ApeGCObjData* data;
    if(capacity == 0)
    {
        capacity = 1;
    }
    #if 1
        #if 1
        data = ape_gcmem_getfrompool(ctx->vm->mem, APE_OBJECT_ARRAY);
        if(data)
        {
            ape_valarray_clear(data->valarray);
            return object_make_from_data(ctx, APE_OBJECT_ARRAY, data);
        }
        #endif
        data = ape_gcmem_allocobjdata(ctx->vm->mem, APE_OBJECT_ARRAY);
    #else
        data = ape_object_make_objdata(ctx, APE_OBJECT_ARRAY);
    #endif
    if(!data)
    {
        return ape_object_make_null(ctx);
    }
    data->valarray = ape_make_valarraycapacity(ctx, capacity, sizeof(ApeObject));
    if(!data->valarray)
    {
        return ape_object_make_null(ctx);
    }
    return object_make_from_data(ctx, APE_OBJECT_ARRAY, data);
}

ApeObject ape_object_array_getvalue(ApeObject object, ApeSize ix)
{
    ApeObject* res;
    ApeValArray* array;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_ARRAY);
    array = ape_object_array_getarray(object);
    if(ix >= ape_valarray_count(array))
    {
        return ape_object_make_null(array->context);
    }
    res = (ApeObject*)ape_valarray_get(array, ix);
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
bool ape_object_array_setat(ApeObject object, ApeInt ix, ApeObject val)
{
    ApeValArray* array;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_ARRAY);
    array = ape_object_array_getarray(object);
    if(ix < 0 || ix >= (ApeInt)ape_valarray_count(array))
    {
        if(ix < 0)
        {
            return false;
        }
        while(ix >= (ApeInt)ape_valarray_count(array))
        {
            ape_object_array_pushvalue(object, ape_object_make_null(array->context));
        }
    }
    return ape_valarray_set(array, ix, &val);
}

bool ape_object_array_pushvalue(ApeObject object, ApeObject val)
{
    ApeValArray* array;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_ARRAY);
    array = ape_object_array_getarray(object);
    return ape_valarray_push(array, &val);
}

bool ape_object_array_popvalue(ApeObject object, ApeObject* dest)
{

    ApeValArray* array;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_ARRAY);
    array = ape_object_array_getarray(object);
    return ape_valarray_popinto(array, dest);
}

ApeSize ape_object_array_getlength(ApeObject object)
{
    ApeValArray* array;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_ARRAY);
    array = ape_object_array_getarray(object);
    return ape_valarray_count(array);
}

bool ape_object_array_removeat(ApeObject object, ApeInt ix)
{
    ApeValArray* array;
    array = ape_object_array_getarray(object);
    return ape_valarray_removeat(array, ix);
}

bool ape_object_array_pushstring(ApeContext* ctx, ApeObject obj, const char* string)
{
    ApeObject new_value;
    ApeGCMemory* mem;
    mem = ape_object_value_getmem(obj);
    if(!mem)
    {
        return false;
    }
    new_value = ape_object_make_string(ctx, string);
    return ape_object_array_pushvalue(obj, new_value);
}


ApeValArray * ape_object_array_getarray(ApeObject object)
{
    ApeGCObjData* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_ARRAY);
    data = ape_object_value_allocated_data(object);
    return data->valarray;
}

static ApeObject objfn_array_length(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    ApeObject self;
    (void)vm;
    (void)data;
    (void)argc;
    (void)args;
    self = ape_vm_thispop(vm);
    return ape_object_make_floatnumber(vm->context, ape_object_array_getlength(self));
}

static ApeObject objfn_array_push(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    ApeSize i;
    ApeObject self;
    (void)vm;
    (void)data;
    (void)argc;
    (void)args;
    self = ape_vm_thispop(vm);
    for(i=0; i<argc; i++)
    {
        ape_object_array_pushvalue(self, args[i]);
    }
    return ape_object_make_null(vm->context);
}

static ApeObject objfn_array_pop(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    ApeObject self;
    ApeObject rt;
    (void)vm;
    (void)data;
    (void)argc;
    (void)args;
    self = ape_vm_thispop(vm);
    if(ape_object_array_popvalue(self, &rt))
    {
        return rt;
    }
    return ape_object_make_null(vm->context);
}

static ApeObject objfn_array_first(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    ApeSize len;
    ApeObject self;
    (void)vm;
    (void)data;
    (void)argc;
    (void)args;
    self = ape_vm_thispop(vm);
    len = ape_object_array_getlength(self);
    if(len > 0)
    {
        return ape_object_array_getvalue(self, 0);
    }
    return ape_object_make_null(vm->context);
}

static ApeObject objfn_array_last(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    ApeSize len;
    ApeObject self;
    (void)vm;
    (void)data;
    (void)argc;
    (void)args;
    self = ape_vm_thispop(vm);
    len = ape_object_array_getlength(self);
    if(len > 0)
    {
        return ape_object_array_getvalue(self, len - 1);
    }
    return ape_object_make_null(vm->context);
}

static ApeObject objfn_array_fill(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    ApeSize i;
    ApeSize len;
    ApeSize howmuch;
    ApeObject val;
    ApeObject self;
    ApeArgCheck check;
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
    self = ape_vm_thispop(vm);
    len = ape_object_array_getlength(self);
    for(i=0; i<howmuch; i++)
    {
        ape_object_array_pushvalue(self, val);
    }
    return ape_object_make_null(vm->context);
}

static ApeObject objfn_array_map(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    ApeSize i;
    ApeSize len;
    ApeObject fn;
    ApeObject rt;
    ApeObject val;
    ApeObject self;
    ApeObject newarr;
    ApeObject fwdargs[5];
    ApeArgCheck check;
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
    self = ape_vm_thispop(vm);
    len = ape_object_array_getlength(self);
    newarr = ape_object_make_arraycapacity(vm->context, len);
    
    for(i=0; i<len; i++)
    {
        val = ape_object_array_getvalue(self, i);
        //ape_context_debugvalue(vm->context, "map:self->val", val);
        #if 1
        /*
        ape_vm_pushstack(vm, ape_object_make_null(vm->context));
        ape_vm_pushstack(vm, ape_object_make_null(vm->context));
        ape_vm_pushstack(vm, fn);
        */
        ape_vm_pushstack(vm, ape_object_make_null(vm->context));
        ape_vm_pushstack(vm, val);
        if(!ape_vm_callobjectstack(vm, fn, 1))
        #else
        ape_vm_pushstack(vm, fn);
        ape_vm_pushstack(vm, val);
        //fwdargs[0] = fn;
        fwdargs[0] = val;
        if(!ape_vm_callobjectargs(vm, fn, 1, fwdargs))
        #endif
        {
            fprintf(stderr, "failed to call function\n");
            return ape_object_make_null(vm->context);
        }

        if(vm->stackptr > 0)
        {
            rt = ape_vm_popstack(vm);
            //rt = ape_vm_getlastpopped(vm);
            ape_context_debugvalue(vm->context, "map:popped->val", rt);
            ape_object_array_pushvalue(newarr, rt);
        }
    }
    ape_context_debugvalue(vm->context, "map:newarr", newarr);
    ape_context_debugvalue(vm->context, "map:self", self);
    return newarr;
}

static ApeObject objfn_array_join(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    const char* sstr;
    const char* bstr;
    ApeSize i;
    ApeSize slen;
    ApeSize alen;
    ApeSize blen;
    ApeObject self;
    ApeObject rt;
    ApeObject arritem;
    ApeObject sjoin;
    ApeWriter* wr;
    ApeArgCheck check;
    (void)data;
    sstr = "";
    slen = 0;
    self = ape_vm_thispop(vm);
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


static ApeObject objfn_array_slice(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    bool ok;
    ApeInt i;
    ApeInt len;
    ApeInt ibegin;
    ApeInt iend;
    ApeObject self;
    ApeObject item;
    ApeObject res;
    ApeArgCheck check;
    (void)data;
    self = ape_vm_thispop(vm);
    ape_args_init(vm, &check, "slice", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);        
    }
    len = ape_object_array_getlength(self);
    ibegin = (ApeInt)ape_object_value_asnumber(args[0]);
    iend = len;
    if(ape_args_checkoptional(&check, 1, APE_OBJECT_NUMERIC, true))
    {
        iend = (ApeInt)ape_object_value_asnumber(args[1]);
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

void ape_builtins_install_array(ApeVM* vm)
{
    ApeSize i;
    ApePseudoClass* psc;
    static const char classname[] = "Array";
    static ApeObjMemberItem memberfuncs[]=
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

