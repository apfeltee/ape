
#include "inline.h"

#define APE_CONF_DICT_INITIAL_SIZE (2)
//#define APE_CONF_MAP_INITIAL_CAPACITY (64/4)
#define APE_CONF_MAP_INITIAL_CAPACITY 0

ApeValDict* ape_make_valdict(ApeContext* ctx, ApeSize ksz, ApeSize vsz)
{
    return ape_make_valdictcapacity(ctx, APE_CONF_DICT_INITIAL_SIZE, ksz, vsz);
}

ApeValDict* ape_make_valdictcapacity(ApeContext* ctx, ApeSize min_capacity, ApeSize ksz, ApeSize vsz)
{
    bool ok;
    ApeValDict* dict;
    ApeSize capacity;
    dict = (ApeValDict*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeValDict));
    capacity = ape_util_upperpoweroftwo(min_capacity * 2);
    if(capacity == 0)
    {
        capacity = 1;
    }
    if(!dict)
    {
        return NULL;
    }
    dict->context = ctx;
    ok = ape_valdict_init(dict->context, dict, ksz, vsz, capacity);
    if(!ok)
    {
        ape_allocator_free(&ctx->alloc, dict);
        return NULL;
    }
    return dict;
}

bool ape_valdict_init(ApeContext* ctx, ApeValDict* dict, ApeSize ksz, ApeSize vsz, ApeSize initial_capacity)
{
    ApeSize i;
    dict->keysize = ksz;
    dict->valsize = vsz;
    dict->cells = NULL;
    dict->keys = NULL;
    dict->values = NULL;
    dict->cellindices = NULL;
    dict->hashes = NULL;
    dict->count = 0;
    dict->cellcap = initial_capacity;
    //dict->itemcap = (ApeSize)(initial_capacity * 0.7f);
    dict->itemcap = (ApeSize)(initial_capacity);
    dict->fnequalkeys = NULL;
    dict->fnhashkey = NULL;
    //fprintf(stderr, "ape_valdict_init: dict->cellcap=%d dict->itemcap=%d initial_capacity=%d\n", dict->cellcap, dict->itemcap, initial_capacity);
    dict->cells = (unsigned int*)ape_allocator_alloc(&ctx->alloc, dict->cellcap * sizeof(*dict->cells));
    dict->keys = (void**)ape_allocator_alloc(&ctx->alloc, dict->itemcap * ksz);
    dict->values = (void**)ape_allocator_alloc(&ctx->alloc, dict->itemcap * vsz);
    dict->cellindices = (unsigned int*)ape_allocator_alloc(&ctx->alloc, dict->itemcap * sizeof(*dict->cellindices));
    dict->hashes = (long unsigned int*)ape_allocator_alloc(&ctx->alloc, dict->itemcap * sizeof(*dict->hashes));
    if(dict->cells == NULL || dict->keys == NULL || dict->values == NULL || dict->cellindices == NULL || dict->hashes == NULL)
    {
        goto error;
    }
    for(i = 0; i < dict->cellcap; i++)
    {
        dict->cells[i] = APE_CONF_INVALID_VALDICT_IX;
    }
    return true;
error:
    ape_allocator_free(&ctx->alloc, dict->cells);
    ape_allocator_free(&ctx->alloc, dict->keys);
    ape_allocator_free(&ctx->alloc, dict->values);
    ape_allocator_free(&ctx->alloc, dict->cellindices);
    ape_allocator_free(&ctx->alloc, dict->hashes);
    return false;
}

void ape_valdict_deinit(ApeValDict* dict)
{
    ApeContext* ctx;
    ctx = dict->context;
    dict->keysize = 0;
    dict->valsize = 0;
    dict->count = 0;
    dict->itemcap = 0;
    dict->cellcap = 0;
    ape_allocator_free(&ctx->alloc, dict->cells);
    ape_allocator_free(&ctx->alloc, dict->keys);
    ape_allocator_free(&ctx->alloc, dict->values);
    ape_allocator_free(&ctx->alloc, dict->cellindices);
    ape_allocator_free(&ctx->alloc, dict->hashes);
    dict->cells = NULL;
    dict->keys = NULL;
    dict->values = NULL;
    dict->cellindices = NULL;
    dict->hashes = NULL;
}

void ape_valdict_destroy(ApeValDict* dict)
{
    ApeContext* ctx;
    if(!dict)
    {
        return;
    }
    ctx = dict->context;
    ape_valdict_deinit(dict);
    ape_allocator_free(&ctx->alloc, dict);
}

void ape_valdict_destroywithitems(ApeContext* ctx, ApeValDict* dict)
{
    ApeSize i;
    void** vp;
    if(!dict)
    {
        return;
    }
    if(dict->fnvaldestroy)
    {
        vp = (void**)(dict->values);
        for(i = 0; i < dict->count; i++)
        {
            dict->fnvaldestroy(ctx, vp[i]);
        }
    }
    ape_valdict_destroy(dict);
}

void ape_valdict_sethashfunction(ApeValDict* dict, ApeDataHashFunc hash_fn)
{
    dict->fnhashkey = hash_fn;
}

void ape_valdict_setequalsfunction(ApeValDict* dict, ApeDataEqualsFunc equals_fn)
{
    dict->fnequalkeys = equals_fn;
}

bool ape_valdict_set(ApeValDict* dict, void* key, void* value)
{
    bool ok;
    bool found;
    ApeSize last_ix;
    ApeSize cell_ix;
    ApeSize item_ix;
    unsigned long hash;
    hash = ape_valdict_hashkey(dict, key);
    found = false;
    cell_ix = ape_valdict_getcellindex(dict, key, hash, &found);
    if(found)
    {
        item_ix = dict->cells[cell_ix];
        ape_valdict_setvalueat(dict, item_ix, value);
        return true;
    }
    if(dict->count >= dict->itemcap)
    {
        ok = ape_valdict_growandrehash(dict);
        if(!ok)
        {
            return false;
        }
        cell_ix = ape_valdict_getcellindex(dict, key, hash, &found);
    }
    last_ix = dict->count;
    dict->count++;
    dict->cells[cell_ix] = last_ix;
    ape_valdict_setkeyat(dict, last_ix, key);
    ape_valdict_setvalueat(dict, last_ix, value);
    dict->cellindices[last_ix] = cell_ix;
    dict->hashes[last_ix] = hash;
    return true;
}

void* ape_valdict_getbyhash(const ApeValDict* dict, const void* key, unsigned long hash)
{
    bool found;
    ApeSize item_ix;
    ApeSize cell_ix;
    found = false;
    cell_ix = ape_valdict_getcellindex(dict, key, hash, &found);
    if(!found)
    {
        return NULL;
    }
    item_ix = dict->cells[cell_ix];
    return ape_valdict_getvalueat(dict, item_ix);
}

void* ape_valdict_getbykey(const ApeValDict* dict, const void* key)
{
    unsigned long hash;
    hash = ape_valdict_hashkey(dict, key);
    return ape_valdict_getbyhash(dict, key, hash);
}


ApeSize ape_valdict_getcellindex(const ApeValDict* dict, const void* key, unsigned long hash, bool* out_found)
{
    bool are_equal;
    ApeSize ofs;
    ApeSize i;
    ApeSize ix;
    ApeSize cell;
    ApeSize cell_ix;
    unsigned long checkhash;
    void* keycheck;
    *out_found = false;
    #if 0
    fprintf(stderr, "ape_valdict_getcellindex: dict=%p, dict->cellcap=%d\n", dict, dict->cellcap);
    #endif
    ofs = 0;
    if(dict->cellcap > 1)
    {
        ofs = (dict->cellcap - 1);
    }
    cell_ix = hash & ofs;
    for(i = 0; i < dict->cellcap; i++)
    {
        cell = APE_CONF_INVALID_VALDICT_IX;
        ix = (cell_ix + i) & ofs;
        #if 0
        fprintf(stderr, "(cell_ix=%d + i=%d) & ofs=%d == %d\n", cell_ix, i, ofs, ix);
        #endif
        cell = dict->cells[ix];
        if(cell == APE_CONF_INVALID_VALDICT_IX)
        {
            return ix;
        }
        checkhash = dict->hashes[cell];
        if(hash != checkhash)
        {
            continue;
        }
        keycheck = ape_valdict_getkeyat(dict, cell);
        are_equal = ape_valdict_keysareequal(dict, key, keycheck);
        if(are_equal)
        {
            *out_found = true;
            return ix;
        }
    }
    return APE_CONF_INVALID_VALDICT_IX;
}

bool ape_valdict_growandrehash(ApeValDict* dict)
{
    bool ok;
    ApeSize new_capacity;
    ApeSize i;
    char* key;
    void* value;
    ApeValDict newdict;
    new_capacity = dict->cellcap == 0 ? APE_CONF_DICT_INITIAL_SIZE : dict->cellcap * 2;
    ok = ape_valdict_init(dict->context, &newdict, dict->keysize, dict->valsize, new_capacity);
    if(!ok)
    {
        return false;
    }
    newdict.context = dict->context;
    newdict.fnequalkeys = dict->fnequalkeys;
    newdict.fnhashkey = dict->fnhashkey;
    for(i = 0; i < dict->count; i++)
    {
        key = (char*)ape_valdict_getkeyat(dict, i);
        value = ape_valdict_getvalueat(dict, i);
        ok = ape_valdict_set(&newdict, key, value);
        if(!ok)
        {
            ape_valdict_deinit(&newdict);
            return false;
        }
    }
    ape_valdict_deinit(dict);
    *dict = newdict;
    return true;
}

bool ape_valdict_setkeyat(ApeValDict* dict, ApeSize ix, void* key)
{
    ApeSize offset;
    if(ix >= dict->count)
    {
        return false;
    }
    offset = ix * dict->keysize;
    memcpy((char*)dict->keys + offset, key, dict->keysize);
    return true;
}

bool ape_valdict_keysareequal(const ApeValDict* dict, const void* a, const void* b)
{
    if(dict->fnequalkeys)
    {
        return dict->fnequalkeys(a, b);
    }
    return memcmp(a, b, dict->keysize) == 0;
}

unsigned long ape_valdict_hashkey(const ApeValDict* dict, const void* key)
{
    if(dict->fnhashkey)
    {
        return dict->fnhashkey(key);
    }
    return ape_util_hashstring(key, dict->keysize);
}

void ape_valdict_setcopyfunc(ApeValDict* dict, ApeDataCallback fn)
{
    dict->fnvalcopy = fn;
}

void ape_valdict_setdeletefunc(ApeValDict* dict, ApeDataCallback fn)
{
    dict->fnvaldestroy = fn;
}

ApeValDict* ape_valdict_copywithitems(ApeContext* ctx, ApeValDict* dict)
{
    ApeSize i;
    bool ok;
    const char* key;
    void* item;
    void* item_copy;
    ApeValDict* vcopy;
    ok = false;
    if(!dict->fnvalcopy || !dict->fnvaldestroy)
    {
        return NULL;
    }
    vcopy = ape_make_valdict(ctx, dict->keysize, dict->valsize);
    vcopy->context = ctx;
    ape_valdict_setcopyfunc(vcopy, (ApeDataCallback)dict->fnvalcopy);
    ape_valdict_setdeletefunc(vcopy, (ApeDataCallback)dict->fnvaldestroy);
    if(!vcopy)
    {
        return NULL;
    }
    for(i = 0; i < ape_valdict_count(dict); i++)
    {
        key = (const char*)ape_valdict_getkeyat(dict, i);
        item = ape_valdict_getvalueat(dict, i);
        item_copy = vcopy->fnvalcopy(ctx, item);
        if(item && !item_copy)
        {
            ape_valdict_destroywithitems(ctx, vcopy);
            return NULL;
        }
        ok = ape_valdict_set(vcopy, (void*)key, item_copy);
        if(!ok)
        {
            vcopy->fnvaldestroy(ctx, item_copy);
            ape_valdict_destroywithitems(ctx, vcopy);
            return NULL;
        }
    }
    return vcopy;
}

ApeStrDict* ape_make_strdict(ApeContext* ctx, ApeDataCallback copy_fn, ApeDataCallback destroy_fn)
{
    bool ok;
    ApeStrDict* dict;
    dict = (ApeStrDict*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeStrDict));
    if(dict == NULL)
    {
        return NULL;
    }
    dict->context = ctx;
    ok = ape_strdict_init(dict, ctx, APE_CONF_DICT_INITIAL_SIZE, copy_fn, destroy_fn);
    if(!ok)
    {
        ape_allocator_free(&ctx->alloc, dict);
        return NULL;
    }
    return dict;
}

bool ape_strdict_init(ApeStrDict* dict, ApeContext* ctx, ApeSize initial_capacity, ApeDataCallback copy_fn, ApeDataCallback destroy_fn)
{
    ApeSize i;
    dict->context = ctx;
    dict->cells = NULL;
    dict->keys = NULL;
    dict->values = NULL;
    dict->cellindices = NULL;
    dict->hashes = NULL;
    dict->count = 0;
    dict->cellcap = initial_capacity;
    dict->itemcap = (ApeSize)(initial_capacity * 0.7f);
    dict->fnstrcopy = copy_fn;
    dict->fnstrdestroy = destroy_fn;
    dict->cells = (unsigned int*)ape_allocator_alloc(&ctx->alloc, dict->cellcap * sizeof(*dict->cells));
    dict->keys = (char**)ape_allocator_alloc(&ctx->alloc, dict->itemcap * sizeof(*dict->keys));
    dict->values = (void**)ape_allocator_alloc(&ctx->alloc, dict->itemcap * sizeof(*dict->values));
    dict->cellindices = (unsigned int*)ape_allocator_alloc(&ctx->alloc, dict->itemcap * sizeof(*dict->cellindices));
    dict->hashes = (long unsigned int*)ape_allocator_alloc(&ctx->alloc, dict->itemcap * sizeof(*dict->hashes));
    if(dict->cells == NULL || dict->keys == NULL || dict->values == NULL || dict->cellindices == NULL || dict->hashes == NULL)
    {
        goto error;
    }
    for(i = 0; i < dict->cellcap; i++)
    {
        dict->cells[i] = APE_CONF_INVALID_STRDICT_IX;
    }
    return true;
error:
    ape_allocator_free(&ctx->alloc, dict->cells);
    ape_allocator_free(&ctx->alloc, dict->keys);
    ape_allocator_free(&ctx->alloc, dict->values);
    ape_allocator_free(&ctx->alloc, dict->cellindices);
    ape_allocator_free(&ctx->alloc, dict->hashes);
    return false;
}

void ape_strdict_deinit(ApeStrDict* dict, bool free_keys)
{
    ApeSize i;
    ApeContext* ctx;
    ctx = dict->context;
    if(free_keys)
    {
        for(i = 0; i < dict->count; i++)
        {
            ape_allocator_free(&ctx->alloc, dict->keys[i]);
        }
    }
    dict->count = 0;
    dict->itemcap = 0;
    dict->cellcap = 0;
    ape_allocator_free(&ctx->alloc, dict->cells);
    ape_allocator_free(&ctx->alloc, dict->keys);
    ape_allocator_free(&ctx->alloc, dict->values);
    ape_allocator_free(&ctx->alloc, dict->cellindices);
    ape_allocator_free(&ctx->alloc, dict->hashes);
    dict->cells = NULL;
    dict->keys = NULL;
    dict->values = NULL;
    dict->cellindices = NULL;
    dict->hashes = NULL;
}

void ape_strdict_destroy(ApeStrDict* dict)
{
    ApeContext* ctx;
    if(!dict)
    {
        return;
    }
    ctx = dict->context;
    ape_strdict_deinit(dict, true);
    ape_allocator_free(&ctx->alloc, dict);
}

void ape_strdict_destroywithitems(ApeContext* ctx, ApeStrDict* dict)
{
    ApeSize i;
    dict->context = ctx;
    if(!dict)
    {
        return;
    }
    if(dict->fnstrdestroy)
    {
        for(i = 0; i < dict->count; i++)
        {
            dict->fnstrdestroy(ctx, dict->values[i]);
        }
    }
    ape_strdict_destroy(dict);
}

ApeStrDict* ape_strdict_copywithitems(ApeContext* ctx, ApeStrDict* dict)
{
    ApeSize i;
    bool ok;
    const char* key;
    void* item;
    void* item_copy;
    ApeStrDict* copy;
    ok = false;
    if(!dict->fnstrcopy || !dict->fnstrdestroy)
    {
        return NULL;
    }
    copy = ape_make_strdict(ctx, (ApeDataCallback)dict->fnstrcopy, (ApeDataCallback)dict->fnstrdestroy);
    if(!copy)
    {
        return NULL;
    }
    copy->context = ctx;
    for(i = 0; i < ape_strdict_count(dict); i++)
    {
        key = ape_strdict_getkeyat(dict, i);
        item = ape_strdict_getvalueat(dict, i);
        item_copy = copy->fnstrcopy(ctx, item);
        if(item && !item_copy)
        {
            ape_strdict_destroywithitems(ctx, copy);
            return NULL;
        }
        ok = ape_strdict_set(copy, key, item_copy);
        if(!ok)
        {
            copy->fnstrdestroy(ctx, item_copy);
            ape_strdict_destroywithitems(ctx, copy);
            return NULL;
        }
    }
    return copy;
}

bool ape_strdict_set(ApeStrDict* dict, const char* key, void* value)
{
    return ape_strdict_setinternal(dict, key, NULL, value);
}

ApeSize ape_strdict_getcellindex(const ApeStrDict* dict, const char* key, unsigned long keyhash, bool* out_found)
{
    ApeSize i;
    ApeSize ix;
    ApeSize cell;
    ApeSize cell_ix;
    unsigned long checkhash;
    const char* keycheck;
    *out_found = false;
    cell_ix = keyhash & (dict->cellcap - 1);
    for(i = 0; i < dict->cellcap; i++)
    {
        ix = (cell_ix + i) & (dict->cellcap - 1);
        cell = dict->cells[ix];
        if(cell == APE_CONF_INVALID_STRDICT_IX)
        {
            return ix;
        }
        checkhash = dict->hashes[cell];
        if(keyhash != checkhash)
        {
            continue;
        }
        keycheck = dict->keys[cell];
        if(strcmp(key, keycheck) == 0)
        {
            *out_found = true;
            return ix;
        }
    }
    return APE_CONF_INVALID_STRDICT_IX;
}

void* ape_strdict_getbyhash(const ApeStrDict* dict, const char* key, unsigned long hash)
{
    bool found;
    ApeSize item_ix;
    ApeSize cell_ix;
    found = false;
    cell_ix = ape_strdict_getcellindex(dict, key, hash, &found);
    if(found == false)
    {
        return NULL;
    }
    item_ix = dict->cells[cell_ix];
    return dict->values[item_ix];
}

void* ape_strdict_getbyname(const ApeStrDict* dict, const char* key)
{
    unsigned long hash;
    ApeSize klen;
    klen = strlen(key);
    hash = ape_util_hashstring(key, klen);
    return ape_strdict_getbyhash(dict, key, hash);
}

void* ape_strdict_getvalueat(const ApeStrDict* dict, ApeSize ix)
{
    if(ix >= dict->count)
    {
        return NULL;
    }
    return dict->values[ix];
}

const char* ape_strdict_getkeyat(const ApeStrDict* dict, ApeSize ix)
{
    if(ix >= dict->count)
    {
        return NULL;
    }
    return dict->keys[ix];
}

ApeSize ape_strdict_count(const ApeStrDict* dict)
{
    if(!dict)
    {
        return 0;
    }
    return dict->count;
}

bool ape_strdict_growandrehash(ApeStrDict* dict)
{
    bool ok;
    ApeSize i;
    char* key;
    void* value;
    ApeContext* ctx;
    ApeStrDict newdict;
    ctx = dict->context;
    ok = ape_strdict_init(&newdict, ctx, dict->cellcap * 2, dict->fnstrcopy, dict->fnstrdestroy);
    if(!ok)
    {
        return false;
    }
    for(i = 0; i < dict->count; i++)
    {
        key = dict->keys[i];
        value = dict->values[i];
        ok = ape_strdict_setinternal(&newdict, key, key, value);
        if(!ok)
        {
            ape_strdict_deinit(&newdict, false);
            return false;
        }
    }
    ape_strdict_deinit(dict, false);
    *dict = newdict;
    return true;
}

bool ape_strdict_setinternal(ApeStrDict* dict, const char* ckey, char* mkey, void* value)
{
    bool ok;
    bool found;
    ApeSize cklen;
    char* key_copy;
    unsigned long hash;
    ApeSize cell_ix;
    ApeSize item_ix;
    ApeContext* ctx;
    ctx = dict->context;
    cklen = strlen(ckey);
    hash = ape_util_hashstring(ckey, cklen);
    found = false;
    cell_ix = ape_strdict_getcellindex(dict, ckey, hash, &found);
    if(found)
    {
        item_ix = dict->cells[cell_ix];
        dict->values[item_ix] = value;
        return true;
    }
    if(dict->count >= dict->itemcap)
    {
        ok = ape_strdict_growandrehash(dict);
        if(!ok)
        {
            return false;
        }
        cell_ix = ape_strdict_getcellindex(dict, ckey, hash, &found);
    }
    if(mkey)
    {
        dict->keys[dict->count] = mkey;
    }
    else
    {
        key_copy = ape_util_strdup(ctx, ckey);
        if(!key_copy)
        {
            return false;
        }
        dict->keys[dict->count] = key_copy;
    }
    dict->cells[cell_ix] = dict->count;
    dict->values[dict->count] = value;
    dict->cellindices[dict->count] = cell_ix;
    dict->hashes[dict->count] = hash;
    dict->count++;
    return true;
}

ApeObject ape_object_make_map(ApeContext* ctx)
{
    return ape_object_make_mapcapacity(ctx, APE_CONF_MAP_INITIAL_CAPACITY);
}

ApeObject ape_object_make_mapcapacity(ApeContext* ctx, unsigned capacity)
{
    ApeGCObjData* data;
    #if 1
        #if 1
        data = ape_gcmem_getfrompool(ctx->vm->mem, APE_OBJECT_MAP);
        if(data)
        {
            ape_valdict_clear(data->valmap);
            return object_make_from_data(ctx, APE_OBJECT_MAP, data);
        }
        #endif
        data = ape_gcmem_allocobjdata(ctx->vm->mem, APE_OBJECT_MAP);
    #else
        data = ape_object_make_objdata(ctx, APE_OBJECT_MAP);
    #endif
    if(!data)
    {
        return ape_object_make_null(ctx);
    }
    data->valmap = ape_make_valdictcapacity(ctx, capacity, sizeof(ApeObject), sizeof(ApeObject));
    if(!data->valmap)
    {
        return ape_object_make_null(ctx);
    }
    ape_valdict_sethashfunction(data->valmap, (ApeDataHashFunc)ape_object_value_hash);
    ape_valdict_setequalsfunction(data->valmap, (ApeDataEqualsFunc)ape_object_value_wrapequals);
    return object_make_from_data(ctx, APE_OBJECT_MAP, data);
}

ApeSize ape_object_map_getlength(ApeObject object)
{
    ApeGCObjData* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_MAP);
    data = ape_object_value_allocated_data(object);
    return ape_valdict_count(data->valmap);
}

ApeObject ape_object_map_getkeyat(ApeObject object, ApeSize ix)
{
    ApeObject* res;
    ApeGCObjData* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_MAP);
    data = ape_object_value_allocated_data(object);
    res= (ApeObject*)ape_valdict_getkeyat(data->valmap, ix);
    if(!res)
    {
        return ape_object_make_null(data->context);
    }
    return *res;
}

ApeObject ape_object_map_getvalueat(ApeObject object, ApeSize ix)
{
    ApeObject* res;
    ApeGCObjData* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_MAP);
    data = ape_object_value_allocated_data(object);
    res = (ApeObject*)ape_valdict_getvalueat(data->valmap, ix);
    if(!res)
    {
        return ape_object_make_null(data->context);
    }
    return *res;
}

bool ape_object_map_setvalue(ApeObject object, ApeObject key, ApeObject val)
{
    ApeGCObjData* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_MAP);
    data = ape_object_value_allocated_data(object);
    return ape_valdict_set(data->valmap, &key, &val);
}

ApeObject ape_object_map_getvalueobject(ApeObject object, ApeObject key)
{
    ApeObject* res;
    ApeGCObjData* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_MAP);
    data = ape_object_value_allocated_data(object);
    res = (ApeObject*)ape_valdict_getbykey(data->valmap, &key);
    if(!res)
    {
        return ape_object_make_null(data->context);
    }
    return *res;
}

bool ape_object_map_setnamedvalue(ApeContext* ctx, ApeObject obj, const char* key, ApeObject value)
{
    ApeGCMemory* mem;
    mem = ape_object_value_getmem(obj);
    if(!mem)
    {
        return false;
    }
    ApeObject key_object = ape_object_make_string(ctx, key);
    if(ape_object_value_isnull(key_object))
    {
        return false;
    }
    return ape_object_map_setvalue(obj, key_object, value);
}

bool ape_object_map_setnamedstringlen(ApeContext* ctx, ApeObject obj, const char* key, const char* string, size_t len)
{
    ApeObject strobj;
    ApeGCMemory* mem;
    mem = ape_object_value_getmem(obj);
    if(!mem)
    {
        return false;
    }
    strobj = ape_object_make_stringlen(ctx, string, len);
    if(ape_object_value_isnull(strobj))
    {
        return false;
    }
    return ape_object_map_setnamedvalue(ctx, obj, key, strobj);
}


bool ape_object_map_setnamedstring(ApeContext* ctx, ApeObject obj, const char* key, const char* string)
{
    return ape_object_map_setnamedstringlen(ctx, obj, key, string, strlen(string));
}


bool ape_object_map_setnamednumber(ApeContext* ctx, ApeObject obj, const char* key, ApeFloat number)
{
    ApeObject numobj;
    numobj = ape_object_make_floatnumber(ctx, number);
    return ape_object_map_setnamedvalue(ctx, obj, key, numobj);
}

bool ape_object_map_setnamedbool(ApeContext* ctx, ApeObject obj, const char* key, bool value)
{
    ApeObject boolobj;
    boolobj = ape_object_make_bool(ctx, value);
    return ape_object_map_setnamedvalue(ctx, obj, key, boolobj);
}



