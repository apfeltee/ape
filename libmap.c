
#include "inline.h"

#define APE_CONF_DICT_INITIAL_SIZE (2)

ApeValDict_t* ape_make_valdict(ApeContext_t* ctx, ApeSize_t ksz, ApeSize_t vsz)
{
    return ape_make_valdictcapacity(ctx, APE_CONF_DICT_INITIAL_SIZE, ksz, vsz);
}

ApeValDict_t* ape_make_valdictcapacity(ApeContext_t* ctx, ApeSize_t min_capacity, ApeSize_t ksz, ApeSize_t vsz)
{
    bool ok;
    ApeValDict_t* dict;
    ApeSize_t capacity;
    dict = (ApeValDict_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeValDict_t));
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

bool ape_valdict_init(ApeContext_t* ctx, ApeValDict_t* dict, ApeSize_t ksz, ApeSize_t vsz, ApeSize_t initial_capacity)
{
    ApeSize_t i;
    dict->keysize = ksz;
    dict->valsize = vsz;
    dict->cells = NULL;
    dict->keys = NULL;
    dict->values = NULL;
    dict->cellindices = NULL;
    dict->hashes = NULL;
    dict->count = 0;
    dict->cellcap = initial_capacity;
    //dict->itemcap = (ApeSize_t)(initial_capacity * 0.7f);
    dict->itemcap = (ApeSize_t)(initial_capacity);
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

void ape_valdict_deinit(ApeValDict_t* dict)
{
    ApeContext_t* ctx;
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

void ape_valdict_destroy(ApeValDict_t* dict)
{
    ApeContext_t* ctx;
    if(!dict)
    {
        return;
    }
    ctx = dict->context;
    ape_valdict_deinit(dict);
    ape_allocator_free(&ctx->alloc, dict);
}

void ape_valdict_destroywithitems(ApeValDict_t* dict)
{
    ApeSize_t i;
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
            dict->fnvaldestroy(vp[i]);
        }
    }
    ape_valdict_destroy(dict);
}

void ape_valdict_sethashfunction(ApeValDict_t* dict, ApeDataHashFunc_t hash_fn)
{
    dict->fnhashkey = hash_fn;
}

void ape_valdict_setequalsfunction(ApeValDict_t* dict, ApeDataEqualsFunc_t equals_fn)
{
    dict->fnequalkeys = equals_fn;
}

bool ape_valdict_set(ApeValDict_t* dict, void* key, void* value)
{
    bool ok;
    bool found;
    ApeSize_t last_ix;
    ApeSize_t cell_ix;
    ApeSize_t item_ix;
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

void* ape_valdict_getbyhash(const ApeValDict_t* dict, const void* key, unsigned long hash)
{
    bool found;
    ApeSize_t item_ix;
    ApeSize_t cell_ix;
    found = false;
    cell_ix = ape_valdict_getcellindex(dict, key, hash, &found);
    if(!found)
    {
        return NULL;
    }
    item_ix = dict->cells[cell_ix];
    return ape_valdict_getvalueat(dict, item_ix);
}

void* ape_valdict_getbykey(const ApeValDict_t* dict, const void* key)
{
    unsigned long hash;
    hash = ape_valdict_hashkey(dict, key);
    return ape_valdict_getbyhash(dict, key, hash);
}


ApeSize_t ape_valdict_getcellindex(const ApeValDict_t* dict, const void* key, unsigned long hash, bool* out_found)
{
    bool are_equal;
    ApeSize_t ofs;
    ApeSize_t i;
    ApeSize_t ix;
    ApeSize_t cell;
    ApeSize_t cell_ix;
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

bool ape_valdict_growandrehash(ApeValDict_t* dict)
{
    bool ok;
    ApeSize_t new_capacity;
    ApeSize_t i;
    char* key;
    void* value;
    ApeValDict_t newdict;
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

bool ape_valdict_setkeyat(ApeValDict_t* dict, ApeSize_t ix, void* key)
{
    ApeSize_t offset;
    if(ix >= dict->count)
    {
        return false;
    }
    offset = ix * dict->keysize;
    memcpy((char*)dict->keys + offset, key, dict->keysize);
    return true;
}

bool ape_valdict_keysareequal(const ApeValDict_t* dict, const void* a, const void* b)
{
    if(dict->fnequalkeys)
    {
        return dict->fnequalkeys(a, b);
    }
    return memcmp(a, b, dict->keysize) == 0;
}

unsigned long ape_valdict_hashkey(const ApeValDict_t* dict, const void* key)
{
    if(dict->fnhashkey)
    {
        return dict->fnhashkey(key);
    }
    return ape_util_hashstring(key, dict->keysize);
}

void ape_valdict_setcopyfunc(ApeValDict_t* dict, ApeDataCallback_t fn)
{
    dict->fnvalcopy = fn;
}

void ape_valdict_setdeletefunc(ApeValDict_t* dict, ApeDataCallback_t fn)
{
    dict->fnvaldestroy = fn;
}

ApeValDict_t* ape_valdict_copywithitems(ApeValDict_t* dict)
{
    ApeSize_t i;
    bool ok;
    const char* key;
    void* item;
    void* item_copy;
    ApeContext_t* ctx;
    ApeValDict_t* vcopy;
    ok = false;
    if(!dict->fnvalcopy || !dict->fnvaldestroy)
    {
        return NULL;
    }
    ctx = dict->context;
    vcopy = ape_make_valdict(ctx, dict->keysize, dict->valsize);
    vcopy->context = ctx;
    ape_valdict_setcopyfunc(vcopy, (ApeDataCallback_t)dict->fnvalcopy);
    ape_valdict_setdeletefunc(vcopy, (ApeDataCallback_t)dict->fnvaldestroy);
    if(!vcopy)
    {
        return NULL;
    }
    for(i = 0; i < ape_valdict_count(dict); i++)
    {
        key = (const char*)ape_valdict_getkeyat(dict, i);
        item = ape_valdict_getvalueat(dict, i);
        item_copy = vcopy->fnvalcopy(item);
        if(item && !item_copy)
        {
            ape_valdict_destroywithitems(vcopy);
            return NULL;
        }
        ok = ape_valdict_set(vcopy, (void*)key, item_copy);
        if(!ok)
        {
            vcopy->fnvaldestroy(item_copy);
            ape_valdict_destroywithitems(vcopy);
            return NULL;
        }
    }
    return vcopy;
}

ApeStrDict_t* ape_make_strdict(ApeContext_t* ctx, ApeDataCallback_t copy_fn, ApeDataCallback_t destroy_fn)
{
    bool ok;
    ApeStrDict_t* dict;
    dict = (ApeStrDict_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeStrDict_t));
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

bool ape_strdict_init(ApeStrDict_t* dict, ApeContext_t* ctx, ApeSize_t initial_capacity, ApeDataCallback_t copy_fn, ApeDataCallback_t destroy_fn)
{
    ApeSize_t i;
    dict->context = ctx;
    dict->cells = NULL;
    dict->keys = NULL;
    dict->values = NULL;
    dict->cellindices = NULL;
    dict->hashes = NULL;
    dict->count = 0;
    dict->cellcap = initial_capacity;
    dict->itemcap = (ApeSize_t)(initial_capacity * 0.7f);
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

void ape_strdict_deinit(ApeStrDict_t* dict, bool free_keys)
{
    ApeSize_t i;
    ApeContext_t* ctx;
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

void ape_strdict_destroy(ApeStrDict_t* dict)
{
    ApeContext_t* ctx;
    if(!dict)
    {
        return;
    }
    ctx = dict->context;
    ape_strdict_deinit(dict, true);
    ape_allocator_free(&ctx->alloc, dict);
}

void ape_strdict_destroywithitems(ApeStrDict_t* dict)
{
    ApeSize_t i;
    if(!dict)
    {
        return;
    }
    if(dict->fnstrdestroy)
    {
        for(i = 0; i < dict->count; i++)
        {
            dict->fnstrdestroy(dict->values[i]);
        }
    }
    ape_strdict_destroy(dict);
}

ApeStrDict_t* ape_strdict_copywithitems(ApeStrDict_t* dict)
{
    ApeSize_t i;
    bool ok;
    const char* key;
    void* item;
    void* item_copy;
    ApeStrDict_t* copy;
    ApeContext_t* ctx;
    ok = false;
    if(!dict->fnstrcopy || !dict->fnstrdestroy)
    {
        return NULL;
    }
    ctx = dict->context;
    copy = ape_make_strdict(ctx, (ApeDataCallback_t)dict->fnstrcopy, (ApeDataCallback_t)dict->fnstrdestroy);
    if(!copy)
    {
        return NULL;
    }
    copy->context = ctx;
    for(i = 0; i < ape_strdict_count(dict); i++)
    {
        key = ape_strdict_getkeyat(dict, i);
        item = ape_strdict_getvalueat(dict, i);
        item_copy = copy->fnstrcopy(item);
        if(item && !item_copy)
        {
            ape_strdict_destroywithitems(copy);
            return NULL;
        }
        ok = ape_strdict_set(copy, key, item_copy);
        if(!ok)
        {
            copy->fnstrdestroy(item_copy);
            ape_strdict_destroywithitems(copy);
            return NULL;
        }
    }
    return copy;
}

bool ape_strdict_set(ApeStrDict_t* dict, const char* key, void* value)
{
    return ape_strdict_setinternal(dict, key, NULL, value);
}

ApeSize_t ape_strdict_getcellindex(const ApeStrDict_t* dict, const char* key, unsigned long keyhash, bool* out_found)
{
    ApeSize_t i;
    ApeSize_t ix;
    ApeSize_t cell;
    ApeSize_t cell_ix;
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

void* ape_strdict_getbyhash(const ApeStrDict_t* dict, const char* key, unsigned long hash)
{
    bool found;
    ApeSize_t item_ix;
    ApeSize_t cell_ix;
    found = false;
    cell_ix = ape_strdict_getcellindex(dict, key, hash, &found);
    if(found == false)
    {
        return NULL;
    }
    item_ix = dict->cells[cell_ix];
    return dict->values[item_ix];
}

void* ape_strdict_getbyname(const ApeStrDict_t* dict, const char* key)
{
    unsigned long hash;
    ApeSize_t klen;
    klen = strlen(key);
    hash = ape_util_hashstring(key, klen);
    return ape_strdict_getbyhash(dict, key, hash);
}

void* ape_strdict_getvalueat(const ApeStrDict_t* dict, ApeSize_t ix)
{
    if(ix >= dict->count)
    {
        return NULL;
    }
    return dict->values[ix];
}

const char* ape_strdict_getkeyat(const ApeStrDict_t* dict, ApeSize_t ix)
{
    if(ix >= dict->count)
    {
        return NULL;
    }
    return dict->keys[ix];
}

ApeSize_t ape_strdict_count(const ApeStrDict_t* dict)
{
    if(!dict)
    {
        return 0;
    }
    return dict->count;
}

bool ape_strdict_growandrehash(ApeStrDict_t* dict)
{
    bool ok;
    ApeSize_t i;
    char* key;
    void* value;
    ApeContext_t* ctx;
    ApeStrDict_t newdict;
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

bool ape_strdict_setinternal(ApeStrDict_t* dict, const char* ckey, char* mkey, void* value)
{
    bool ok;
    bool found;
    ApeSize_t cklen;
    char* key_copy;
    unsigned long hash;
    ApeSize_t cell_ix;
    ApeSize_t item_ix;
    ApeContext_t* ctx;
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

ApeSize_t ape_object_map_getlength(ApeObject_t object)
{
    ApeGCObjData_t* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_MAP);
    data = ape_object_value_allocated_data(object);
    return ape_valdict_count(data->valmap);
}

ApeObject_t ape_object_map_getkeyat(ApeObject_t object, ApeSize_t ix)
{
    ApeObject_t* res;
    ApeGCObjData_t* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_MAP);
    data = ape_object_value_allocated_data(object);
    res= (ApeObject_t*)ape_valdict_getkeyat(data->valmap, ix);
    if(!res)
    {
        return ape_object_make_null(data->context);
    }
    return *res;
}

ApeObject_t ape_object_map_getvalueat(ApeObject_t object, ApeSize_t ix)
{
    ApeObject_t* res;
    ApeGCObjData_t* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_MAP);
    data = ape_object_value_allocated_data(object);
    res = (ApeObject_t*)ape_valdict_getvalueat(data->valmap, ix);
    if(!res)
    {
        return ape_object_make_null(data->context);
    }
    return *res;
}

bool ape_object_map_setvalue(ApeObject_t object, ApeObject_t key, ApeObject_t val)
{
    ApeGCObjData_t* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_MAP);
    data = ape_object_value_allocated_data(object);
    return ape_valdict_set(data->valmap, &key, &val);
}

ApeObject_t ape_object_map_getvalueobject(ApeObject_t object, ApeObject_t key)
{
    ApeObject_t* res;
    ApeGCObjData_t* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_MAP);
    data = ape_object_value_allocated_data(object);
    res = (ApeObject_t*)ape_valdict_getbykey(data->valmap, &key);
    if(!res)
    {
        return ape_object_make_null(data->context);
    }
    return *res;
}

bool ape_object_map_setnamedvalue(ApeContext_t* ctx, ApeObject_t obj, const char* key, ApeObject_t value)
{
    ApeGCMemory_t* mem;
    mem = ape_object_value_getmem(obj);
    if(!mem)
    {
        return false;
    }
    ApeObject_t key_object = ape_object_make_string(ctx, key);
    if(ape_object_value_isnull(key_object))
    {
        return false;
    }
    return ape_object_map_setvalue(obj, key_object, value);
}

bool ape_object_map_setnamedstring(ApeContext_t* ctx, ApeObject_t obj, const char* key, const char* string)
{
    ApeGCMemory_t* mem = ape_object_value_getmem(obj);
    if(!mem)
    {
        return false;
    }
    ApeObject_t string_object = ape_object_make_string(ctx, string);
    if(ape_object_value_isnull(string_object))
    {
        return false;
    }
    return ape_object_map_setnamedvalue(ctx, obj, key, string_object);
}

bool ape_object_map_setnamednumber(ApeContext_t* ctx, ApeObject_t obj, const char* key, ApeFloat_t number)
{
    ApeGCObjData_t* data;
    data = ape_object_value_allocated_data(obj);
    ApeObject_t number_object = ape_object_make_number(ctx, number);
    return ape_object_map_setnamedvalue(ctx, obj, key, number_object);
}

bool ape_object_map_setnamedbool(ApeContext_t* ctx, ApeObject_t obj, const char* key, bool value)
{
    ApeGCObjData_t* data;
    data = ape_object_value_allocated_data(obj);
    ApeObject_t bool_object = ape_object_make_bool(ctx, value);
    return ape_object_map_setnamedvalue(ctx, obj, key, bool_object);
}



