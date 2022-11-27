
#include "ape.h"

// Public
ApeValDictionary_t* ape_make_valdict_actual(ApeContext_t* ctx, ApeSize_t key_size, ApeSize_t val_size)
{
    return ape_make_valdictcapacity(ctx, APE_CONF_DICT_INITIAL_SIZE, key_size, val_size);
}

ApeValDictionary_t* ape_make_valdictcapacity(ApeContext_t* ctx, ApeSize_t min_capacity, ApeSize_t key_size, ApeSize_t val_size)
{
    bool ok;
    ApeValDictionary_t* dict;
    ApeSize_t capacity;
    dict = (ApeValDictionary_t*)allocator_malloc(&ctx->alloc, sizeof(ApeValDictionary_t));
    capacity = ape_util_upperpoweroftwo(min_capacity * 2);
    if(!dict)
    {
        return NULL;
    }
    ok = ape_valdict_init(dict, key_size, val_size, capacity);
    if(!ok)
    {
        allocator_free(&ctx->alloc, dict);
        return NULL;
    }
    return dict;
}

void ape_valdict_destroy(ApeValDictionary_t* dict)
{
    ApeAllocator_t* alloc;
    if(!dict)
    {
        return;
    }
    alloc = dict->alloc;
    ape_valdict_deinit(dict);
    allocator_free(alloc, dict);
}

void ape_valdict_sethashfunction(ApeValDictionary_t* dict, ApeDataHashFunc_t hash_fn)
{
    dict->_hash_key = hash_fn;
}

void ape_valdict_setequalsfunction(ApeValDictionary_t* dict, ApeDataEqualsFunc_t equals_fn)
{
    dict->_keys_equals = equals_fn;
}

bool ape_valdict_set(ApeValDictionary_t* dict, void* key, void* value)
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
    if(dict->count >= dict->item_capacity)
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
    dict->cell_ixs[last_ix] = cell_ix;
    dict->hashes[last_ix] = hash;
    return true;
}

void* ape_valdict_get(const ApeValDictionary_t* dict, const void* key)
{
    bool found;
    ApeSize_t item_ix;
    unsigned long hash;
    ApeSize_t cell_ix;
    hash = ape_valdict_hashkey(dict, key);
    found = false;
    cell_ix = ape_valdict_getcellindex(dict, key, hash, &found);
    if(!found)
    {
        return NULL;
    }
    item_ix = dict->cells[cell_ix];
    return ape_valdict_getvalueat(dict, item_ix);
}

void* ape_valdict_getkeyat(const ApeValDictionary_t* dict, ApeSize_t ix)
{
    if(ix >= dict->count)
    {
        return NULL;
    }
    return (char*)dict->keys + (dict->key_size * ix);
}

void* ape_valdict_getvalueat(const ApeValDictionary_t* dict, ApeSize_t ix)
{
    if(ix >= dict->count)
    {
        return NULL;
    }
    return (char*)dict->values + (dict->val_size * ix);
}

bool ape_valdict_setvalueat(const ApeValDictionary_t* dict, ApeSize_t ix, const void* value)
{
    ApeSize_t offset;
    if(ix >= dict->count)
    {
        return false;
    }
    offset = ix * dict->val_size;
    memcpy((char*)dict->values + offset, value, dict->val_size);
    return true;
}

ApeSize_t ape_valdict_count(const ApeValDictionary_t* dict)
{
    if(!dict)
    {
        return 0;
    }
    return dict->count;
}

void ape_valdict_clear(ApeValDictionary_t* dict)
{
    ApeSize_t i;
    dict->count = 0;
    for(i = 0; i < dict->cell_capacity; i++)
    {
        dict->cells[i] = APE_CONF_INVALID_VALDICT_IX;
    }
}

// Private definitions
bool ape_valdict_init(ApeValDictionary_t* dict, ApeSize_t key_size, ApeSize_t val_size, ApeSize_t initial_capacity)
{
    ApeSize_t i;
    dict->alloc = &dict->context->alloc;
    dict->key_size = key_size;
    dict->val_size = val_size;
    dict->cells = NULL;
    dict->keys = NULL;
    dict->values = NULL;
    dict->cell_ixs = NULL;
    dict->hashes = NULL;
    dict->count = 0;
    dict->cell_capacity = initial_capacity;
    dict->item_capacity = (ApeSize_t)(initial_capacity * 0.7f);
    dict->_keys_equals = NULL;
    dict->_hash_key = NULL;
    dict->cells = (unsigned int*)allocator_malloc(dict->alloc, dict->cell_capacity * sizeof(*dict->cells));
    dict->keys = allocator_malloc(dict->alloc, dict->item_capacity * key_size);
    dict->values = allocator_malloc(dict->alloc, dict->item_capacity * val_size);
    dict->cell_ixs = (unsigned int*)allocator_malloc(dict->alloc, dict->item_capacity * sizeof(*dict->cell_ixs));
    dict->hashes = (long unsigned int*)allocator_malloc(dict->alloc, dict->item_capacity * sizeof(*dict->hashes));
    if(dict->cells == NULL || dict->keys == NULL || dict->values == NULL || dict->cell_ixs == NULL || dict->hashes == NULL)
    {
        goto error;
    }
    for(i = 0; i < dict->cell_capacity; i++)
    {
        dict->cells[i] = APE_CONF_INVALID_VALDICT_IX;
    }
    return true;
error:
    allocator_free(dict->alloc, dict->cells);
    allocator_free(dict->alloc, dict->keys);
    allocator_free(dict->alloc, dict->values);
    allocator_free(dict->alloc, dict->cell_ixs);
    allocator_free(dict->alloc, dict->hashes);
    return false;
}

void ape_valdict_deinit(ApeValDictionary_t* dict)
{
    dict->key_size = 0;
    dict->val_size = 0;
    dict->count = 0;
    dict->item_capacity = 0;
    dict->cell_capacity = 0;
    allocator_free(dict->alloc, dict->cells);
    allocator_free(dict->alloc, dict->keys);
    allocator_free(dict->alloc, dict->values);
    allocator_free(dict->alloc, dict->cell_ixs);
    allocator_free(dict->alloc, dict->hashes);
    dict->cells = NULL;
    dict->keys = NULL;
    dict->values = NULL;
    dict->cell_ixs = NULL;
    dict->hashes = NULL;
}

ApeSize_t ape_valdict_getcellindex(const ApeValDictionary_t* dict, const void* key, unsigned long hash, bool* out_found)
{
    bool are_equal;
    ApeSize_t ofs;
    ApeSize_t i;
    ApeSize_t ix;
    ApeSize_t cell;
    ApeSize_t cell_ix;
    unsigned long hash_to_check;
    void* key_to_check;
    *out_found = false;

    //fprintf(stderr, "ape_valdict_getcellindex: dict=%p, dict->cell_capacity=%d\n", dict, dict->cell_capacity);
    ofs = 0;
    if(dict->cell_capacity > 1)
    {
        ofs = (dict->cell_capacity - 1);
    }
    cell_ix = hash & ofs;
    for(i = 0; i < dict->cell_capacity; i++)
    {
        cell = APE_CONF_INVALID_VALDICT_IX;
        ix = (cell_ix + i) & ofs;
        //fprintf(stderr, "(cell_ix=%d + i=%d) & ofs=%d == %d\n", cell_ix, i, ofs, ix);
        cell = dict->cells[ix];
        if(cell == APE_CONF_INVALID_VALDICT_IX)
        {
            return ix;
        }
        hash_to_check = dict->hashes[cell];
        if(hash != hash_to_check)
        {
            continue;
        }
        key_to_check = ape_valdict_getkeyat(dict, cell);
        are_equal = ape_valdict_keysareequal(dict, key, key_to_check);
        if(are_equal)
        {
            *out_found = true;
            return ix;
        }
    }
    return APE_CONF_INVALID_VALDICT_IX;
}

bool ape_valdict_growandrehash(ApeValDictionary_t* dict)
{
    bool ok;
    ApeSize_t new_capacity;
    ApeSize_t i;
    char* key;
    void* value;
    ApeValDictionary_t new_dict;
    new_capacity = dict->cell_capacity == 0 ? APE_CONF_DICT_INITIAL_SIZE : dict->cell_capacity * 2;
    ok = ape_valdict_init(&new_dict, dict->key_size, dict->val_size, new_capacity);
    if(!ok)
    {
        return false;
    }
    new_dict._keys_equals = dict->_keys_equals;
    new_dict._hash_key = dict->_hash_key;
    for(i = 0; i < dict->count; i++)
    {
        key = (char*)ape_valdict_getkeyat(dict, i);
        value = ape_valdict_getvalueat(dict, i);
        ok = ape_valdict_set(&new_dict, key, value);
        if(!ok)
        {
            ape_valdict_deinit(&new_dict);
            return false;
        }
    }
    ape_valdict_deinit(dict);
    *dict = new_dict;
    return true;
}

bool ape_valdict_setkeyat(ApeValDictionary_t* dict, ApeSize_t ix, void* key)
{
    ApeSize_t offset;
    if(ix >= dict->count)
    {
        return false;
    }
    offset = ix * dict->key_size;
    memcpy((char*)dict->keys + offset, key, dict->key_size);
    return true;
}

bool ape_valdict_keysareequal(const ApeValDictionary_t* dict, const void* a, const void* b)
{
    if(dict->_keys_equals)
    {
        return dict->_keys_equals(a, b);
    }
    return memcmp(a, b, dict->key_size) == 0;
}

unsigned long ape_valdict_hashkey(const ApeValDictionary_t* dict, const void* key)
{
    if(dict->_hash_key)
    {
        return dict->_hash_key(key);
    }
    return ape_util_hashstring(key, dict->key_size);
}

void ape_valdict_destroywithitems(ApeValDictionary_t* dict)
{
    ApeSize_t i;
    void** vp;
    if(!dict)
    {
        return;
    }
    if(dict->destroy_fn)
    {
        vp = (void**)(dict->values);
        for(i = 0; i < dict->count; i++)
        {
            
            dict->destroy_fn(vp[i]);
        }
    }
    ape_valdict_destroy(dict);
}

void ape_valdict_setcopyfunc(ApeValDictionary_t* dict, ApeDataCallback_t fn)
{
    dict->copy_fn = fn;
}

void ape_valdict_setdeletefunc(ApeValDictionary_t* dict, ApeDataCallback_t fn)
{
    dict->destroy_fn = fn;
}

ApeValDictionary_t* ape_valdict_copywithitems(ApeValDictionary_t* dict)
{
    ApeSize_t i;
    bool ok;
    const char* key;
    void* item;
    void* item_copy;
    ApeValDictionary_t* dict_copy;
    ok = false;
    if(!dict->copy_fn || !dict->destroy_fn)
    {
        return NULL;
    }
    dict_copy = valdict_make(dict->context, dict->key_size, dict->val_size);
    ape_valdict_setcopyfunc(dict_copy, (ApeDataCallback_t)dict->copy_fn);
    ape_valdict_setdeletefunc(dict_copy, (ApeDataCallback_t)dict->destroy_fn);

    if(!dict_copy)
    {
        return NULL;
    }
    for(i = 0; i < ape_valdict_count(dict); i++)
    {
        key = ape_valdict_getkeyat(dict, i);
        item = ape_valdict_getvalueat(dict, i);
        item_copy = dict_copy->copy_fn(item);
        if(item && !item_copy)
        {
            ape_valdict_destroywithitems(dict_copy);
            return NULL;
        }
        ok = ape_valdict_set(dict_copy, (void*)key, item_copy);
        if(!ok)
        {
            dict_copy->destroy_fn(item_copy);
            ape_valdict_destroywithitems(dict_copy);
            return NULL;
        }
    }
    return dict_copy;
}

// Public
ApeStrDictionary_t* ape_make_strdict(ApeContext_t* ctx, ApeDataCallback_t copy_fn, ApeDataCallback_t destroy_fn)
{
    bool ok;
    ApeStrDictionary_t* dict;
    dict = (ApeStrDictionary_t*)allocator_malloc(&ctx->alloc, sizeof(ApeStrDictionary_t));
    if(dict == NULL)
    {
        return NULL;
    }
    dict->context = ctx;
    ok = ape_strdict_init(dict, ctx, APE_CONF_DICT_INITIAL_SIZE, copy_fn, destroy_fn);
    if(!ok)
    {
        allocator_free(&ctx->alloc, dict);
        return NULL;
    }
    return dict;
}

void ape_strdict_destroy(ApeStrDictionary_t* dict)
{
    ApeAllocator_t* alloc;
    if(!dict)
    {
        return;
    }
    alloc = dict->alloc;
    ape_strdict_deinit(dict, true);
    allocator_free(alloc, dict);
}

void ape_strdict_destroywithitems(ApeStrDictionary_t* dict)
{
    ApeSize_t i;
    if(!dict)
    {
        return;
    }
    if(dict->destroy_fn)
    {
        for(i = 0; i < dict->count; i++)
        {
            dict->destroy_fn(dict->values[i]);
        }
    }
    ape_strdict_destroy(dict);
}

ApeStrDictionary_t* ape_strdict_copywithitems(ApeStrDictionary_t* dict)
{
    ApeSize_t i;
    bool ok;
    const char* key;
    void* item;
    void* item_copy;
    ApeStrDictionary_t* dict_copy;
    ok = false;
    if(!dict->copy_fn || !dict->destroy_fn)
    {
        return NULL;
    }
    dict_copy = ape_make_strdict(dict->context, (ApeDataCallback_t)dict->copy_fn, (ApeDataCallback_t)dict->destroy_fn);
    if(!dict_copy)
    {
        return NULL;
    }
    for(i = 0; i < ape_strdict_count(dict); i++)
    {
        key = ape_strdict_getkeyat(dict, i);
        item = ape_strdict_getvalueat(dict, i);
        item_copy = dict_copy->copy_fn(item);
        if(item && !item_copy)
        {
            ape_strdict_destroywithitems(dict_copy);
            return NULL;
        }
        ok = ape_strdict_set(dict_copy, key, item_copy);
        if(!ok)
        {
            dict_copy->destroy_fn(item_copy);
            ape_strdict_destroywithitems(dict_copy);
            return NULL;
        }
    }
    return dict_copy;
}

bool ape_strdict_set(ApeStrDictionary_t* dict, const char* key, void* value)
{
    return ape_strdict_setinternal(dict, key, NULL, value);
}

void* ape_strdict_get(const ApeStrDictionary_t* dict, const char* key)
{
    bool found;
    ApeSize_t klen;
    ApeSize_t item_ix;
    ApeSize_t cell_ix;
    unsigned long hash;
    klen = strlen(key);
    hash = ape_util_hashstring(key, klen);
    found = false;
    cell_ix = ape_strdict_getcellindex(dict, key, hash, &found);
    if(found == false)
    {
        return NULL;
    }
    item_ix = dict->cells[cell_ix];
    return dict->values[item_ix];
}

void* ape_strdict_getvalueat(const ApeStrDictionary_t* dict, ApeSize_t ix)
{
    if(ix >= dict->count)
    {
        return NULL;
    }
    return dict->values[ix];
}

const char* ape_strdict_getkeyat(const ApeStrDictionary_t* dict, ApeSize_t ix)
{
    if(ix >= dict->count)
    {
        return NULL;
    }
    return dict->keys[ix];
}

ApeSize_t ape_strdict_count(const ApeStrDictionary_t* dict)
{
    if(!dict)
    {
        return 0;
    }
    return dict->count;
}

// Private definitions
bool ape_strdict_init(ApeStrDictionary_t* dict, ApeContext_t* ctx, ApeSize_t initial_capacity, ApeDataCallback_t copy_fn, ApeDataCallback_t destroy_fn)
{
    ApeSize_t i;
    dict->context = ctx;
    dict->alloc = &ctx->alloc;
    dict->cells = NULL;
    dict->keys = NULL;
    dict->values = NULL;
    dict->cell_ixs = NULL;
    dict->hashes = NULL;
    dict->count = 0;
    dict->cell_capacity = initial_capacity;
    dict->item_capacity = (ApeSize_t)(initial_capacity * 0.7f);
    dict->copy_fn = copy_fn;
    dict->destroy_fn = destroy_fn;
    dict->cells = (unsigned int*)allocator_malloc(&ctx->alloc, dict->cell_capacity * sizeof(*dict->cells));
    dict->keys = (char**)allocator_malloc(&ctx->alloc, dict->item_capacity * sizeof(*dict->keys));
    dict->values = (void**)allocator_malloc(&ctx->alloc, dict->item_capacity * sizeof(*dict->values));
    dict->cell_ixs = (unsigned int*)allocator_malloc(&ctx->alloc, dict->item_capacity * sizeof(*dict->cell_ixs));
    dict->hashes = (long unsigned int*)allocator_malloc(&ctx->alloc, dict->item_capacity * sizeof(*dict->hashes));
    if(dict->cells == NULL || dict->keys == NULL || dict->values == NULL || dict->cell_ixs == NULL || dict->hashes == NULL)
    {
        goto error;
    }
    for(i = 0; i < dict->cell_capacity; i++)
    {
        dict->cells[i] = APE_CONF_INVALID_STRDICT_IX;
    }
    return true;
error:
    allocator_free(dict->alloc, dict->cells);
    allocator_free(dict->alloc, dict->keys);
    allocator_free(dict->alloc, dict->values);
    allocator_free(dict->alloc, dict->cell_ixs);
    allocator_free(dict->alloc, dict->hashes);
    return false;
}

void ape_strdict_deinit(ApeStrDictionary_t* dict, bool free_keys)
{
    ApeSize_t i;
    if(free_keys)
    {
        for(i = 0; i < dict->count; i++)
        {
            allocator_free(dict->alloc, dict->keys[i]);
        }
    }
    dict->count = 0;
    dict->item_capacity = 0;
    dict->cell_capacity = 0;
    allocator_free(dict->alloc, dict->cells);
    allocator_free(dict->alloc, dict->keys);
    allocator_free(dict->alloc, dict->values);
    allocator_free(dict->alloc, dict->cell_ixs);
    allocator_free(dict->alloc, dict->hashes);
    dict->cells = NULL;
    dict->keys = NULL;
    dict->values = NULL;
    dict->cell_ixs = NULL;
    dict->hashes = NULL;
}

ApeSize_t ape_strdict_getcellindex(const ApeStrDictionary_t* dict, const char* key, unsigned long hash, bool* out_found)
{
    ApeSize_t i;
    ApeSize_t ix;
    ApeSize_t cell;
    ApeSize_t cell_ix;
    unsigned long hash_to_check;
    const char* key_to_check;
    *out_found = false;
    cell_ix = hash & (dict->cell_capacity - 1);
    for(i = 0; i < dict->cell_capacity; i++)
    {
        ix = (cell_ix + i) & (dict->cell_capacity - 1);
        cell = dict->cells[ix];
        if(cell == APE_CONF_INVALID_STRDICT_IX)
        {
            return ix;
        }
        hash_to_check = dict->hashes[cell];
        if(hash != hash_to_check)
        {
            continue;
        }
        key_to_check = dict->keys[cell];
        if(strcmp(key, key_to_check) == 0)
        {
            *out_found = true;
            return ix;
        }
    }
    return APE_CONF_INVALID_STRDICT_IX;
}


bool ape_strdict_growandrehash(ApeStrDictionary_t* dict)
{
    bool ok;
    ApeSize_t i;
    char* key;
    void* value;
    ApeStrDictionary_t new_dict;
    ok = ape_strdict_init(&new_dict, dict->context, dict->cell_capacity * 2, dict->copy_fn, dict->destroy_fn);
    if(!ok)
    {
        return false;
    }
    for(i = 0; i < dict->count; i++)
    {

        key = dict->keys[i];
        value = dict->values[i];
        ok = ape_strdict_setinternal(&new_dict, key, key, value);
        if(!ok)
        {
            ape_strdict_deinit(&new_dict, false);
            return false;
        }
    }
    ape_strdict_deinit(dict, false);
    *dict = new_dict;
    return true;
}

bool ape_strdict_setinternal(ApeStrDictionary_t* dict, const char* ckey, char* mkey, void* value)
{
    bool ok;
    bool found;
    ApeSize_t cklen;
    char* key_copy;
    unsigned long hash;
    ApeSize_t cell_ix;
    ApeSize_t item_ix;
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
    if(dict->count >= dict->item_capacity)
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
        key_copy = ape_util_strdup(dict->context, ckey);
        if(!key_copy)
        {
            return false;
        }
        dict->keys[dict->count] = key_copy;
    }
    dict->cells[cell_ix] = dict->count;
    dict->values[dict->count] = value;
    dict->cell_ixs[dict->count] = cell_ix;
    dict->hashes[dict->count] = hash;
    dict->count++;
    return true;
}

ApeObject_t ape_object_make_map(ApeContext_t* ctx)
{
    return ape_object_make_mapcapacity(ctx, 32);
}

ApeObject_t ape_object_make_mapcapacity(ApeContext_t* ctx, unsigned capacity)
{
    ApeObjectData_t* data;
    data = gcmem_get_object_data_from_pool(ctx->vm->mem, APE_OBJECT_MAP);
    if(data)
    {
        ape_valdict_clear(data->map);
        return object_make_from_data(ctx, APE_OBJECT_MAP, data);
    }
    data = gcmem_alloc_object_data(ctx->vm->mem, APE_OBJECT_MAP);
    if(!data)
    {
        return object_make_null(ctx);
    }
    data->map = ape_make_valdictcapacity(ctx, capacity, sizeof(ApeObject_t), sizeof(ApeObject_t));
    if(!data->map)
    {
        return object_make_null(ctx);
    }
    ape_valdict_sethashfunction(data->map, (ApeDataHashFunc_t)object_value_hash);
    ape_valdict_setequalsfunction(data->map, (ApeDataEqualsFunc_t)object_value_wrapequals);
    return object_make_from_data(ctx, APE_OBJECT_MAP, data);
}


ApeSize_t ape_object_map_getlength(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_MAP);
    data = object_value_allocated_data(object);
    return ape_valdict_count(data->map);
}

ApeObject_t ape_object_map_getkeyat(ApeObject_t object, ApeSize_t ix)
{
    ApeObject_t* res;
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_MAP);
    data = object_value_allocated_data(object);
    res= (ApeObject_t*)ape_valdict_getkeyat(data->map, ix);
    if(!res)
    {
        return object_make_null(data->context);
    }
    return *res;
}

ApeObject_t ape_object_map_getvalueat(ApeObject_t object, ApeSize_t ix)
{
    ApeObject_t* res;
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_MAP);
    data = object_value_allocated_data(object);
    res = (ApeObject_t*)ape_valdict_getvalueat(data->map, ix);
    if(!res)
    {
        return object_make_null(data->context);
    }
    return *res;
}

bool ape_object_map_setvalue(ApeObject_t object, ApeObject_t key, ApeObject_t val)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_MAP);
    data = object_value_allocated_data(object);
    return ape_valdict_set(data->map, &key, &val);
}

ApeObject_t ape_object_map_getvalueobject(ApeObject_t object, ApeObject_t key)
{
    ApeObject_t* res;
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_MAP);
    data = object_value_allocated_data(object);
    res = (ApeObject_t*)ape_valdict_get(data->map, &key);
    if(!res)
    {
        return object_make_null(data->context);
    }
    return *res;
}

bool ape_object_map_setnamedvalue(ApeObject_t obj, const char* key, ApeObject_t value)
{
    ApeGCMemory_t* mem = object_value_getmem(obj);
    if(!mem)
    {
        return false;
    }
    ApeObject_t key_object = ape_object_make_string(mem->context, key);
    if(object_value_isnull(key_object))
    {
        return false;
    }
    return ape_object_map_setvalue(obj, key_object, value);
}

bool ape_object_map_setnamedstring(ApeObject_t obj, const char* key, const char* string)
{
    ApeGCMemory_t* mem = object_value_getmem(obj);
    if(!mem)
    {
        return false;
    }
    ApeObject_t string_object = ape_object_make_string(mem->context, string);
    if(object_value_isnull(string_object))
    {
        return false;
    }
    return ape_object_map_setnamedvalue(obj, key, string_object);
}

bool ape_object_map_setnamednumber(ApeObject_t obj, const char* key, ApeFloat_t number)
{
    ApeObjectData_t* data;
    data = object_value_allocated_data(obj);
    ApeObject_t number_object = object_make_number(data->context, number);
    return ape_object_map_setnamedvalue(obj, key, number_object);
}

bool ape_object_map_setnamedbool(ApeObject_t obj, const char* key, bool value)
{
    ApeObjectData_t* data;
    data = object_value_allocated_data(obj);
    ApeObject_t bool_object = object_make_bool(data->context, value);
    return ape_object_map_setnamedvalue(obj, key, bool_object);
}


