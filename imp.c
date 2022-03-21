/*
SPDX-License-Identifier: MIT

ape
https://github.com/kgabis/ape
Copyright (c) 2021 Krzysztof Gabis

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "priv.h"

/**/
static ApeOpcodeDefinition_t g_definitions[OPCODE_MAX + 1] =
{
    { "NONE", 0, { 0 } },
    { "CONSTANT", 1, { 2 } },
    { "ADD", 0, { 0 } },
    { "POP", 0, { 0 } },
    { "SUB", 0, { 0 } },
    { "MUL", 0, { 0 } },
    { "DIV", 0, { 0 } },
    { "MOD", 0, { 0 } },
    { "TRUE", 0, { 0 } },
    { "FALSE", 0, { 0 } },
    { "COMPARE", 0, { 0 } },
    { "COMPARE_EQ", 0, { 0 } },
    { "EQUAL", 0, { 0 } },
    { "NOT_EQUAL", 0, { 0 } },
    { "GREATER_THAN", 0, { 0 } },
    { "GREATER_THAN_EQUAL", 0, { 0 } },
    { "MINUS", 0, { 0 } },
    { "BANG", 0, { 0 } },
    { "JUMP", 1, { 2 } },
    { "JUMP_IF_FALSE", 1, { 2 } },
    { "JUMP_IF_TRUE", 1, { 2 } },
    { "NULL", 0, { 0 } },
    { "GET_MODULE_GLOBAL", 1, { 2 } },
    { "SET_MODULE_GLOBAL", 1, { 2 } },
    { "DEFINE_MODULE_GLOBAL", 1, { 2 } },
    { "ARRAY", 1, { 2 } },
    { "MAP_START", 1, { 2 } },
    { "MAP_END", 1, { 2 } },
    { "GET_THIS", 0, { 0 } },
    { "GET_INDEX", 0, { 0 } },
    { "SET_INDEX", 0, { 0 } },
    { "GET_VALUE_AT", 0, { 0 } },
    { "CALL", 1, { 1 } },
    { "RETURN_VALUE", 0, { 0 } },
    { "RETURN", 0, { 0 } },
    { "GET_LOCAL", 1, { 1 } },
    { "DEFINE_LOCAL", 1, { 1 } },
    { "SET_LOCAL", 1, { 1 } },
    { "GET_APE_GLOBAL", 1, { 2 } },
    { "FUNCTION", 2, { 2, 1 } },
    { "GET_FREE", 1, { 1 } },
    { "SET_FREE", 1, { 1 } },
    { "CURRENT_FUNCTION", 0, { 0 } },
    { "DUP", 0, { 0 } },
    { "NUMBER", 1, { 8 } },
    { "LEN", 0, { 0 } },
    { "SET_RECOVER", 1, { 2 } },
    { "OR", 0, { 0 } },
    { "XOR", 0, { 0 } },
    { "AND", 0, { 0 } },
    { "LSHIFT", 0, { 0 } },
    { "RSHIFT", 0, { 0 } },
    { "INVALID_MAX", 0, { 0 } },
};

#if 0
ApeObject_t ape_object_to_object(ApeObject_t obj)
{
    return (ApeObject_t){ .handle = obj._internal };
}

ApeObject_t object_to_ape_object(ApeObject_t obj)
{
    return (ApeObject_t){ ._internal = obj.handle };
}

#else
    #define ape_object_to_object(o) o
    #define object_to_ape_object(o) o
#endif

ApePosition_t src_pos_make(const ApeCompiledFile_t* file, int line, int column)
{
    return (ApePosition_t){
        .file = file,
        .line = line,
        .column = column,
    };
}

char* ape_stringf(ApeAllocator_t* alloc, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int to_write = vsnprintf(NULL, 0, format, args);
    va_end(args);
    va_start(args, format);
    char* res = (char*)allocator_malloc(alloc, to_write + 1);
    if(!res)
    {
        return NULL;
    }
    int written = vsprintf(res, format, args);
    (void)written;
    APE_ASSERT(written == to_write);
    va_end(args);
    return res;
}

void ape_log(const char* file, int line, const char* format, ...)
{
    char msg[4096];
    int written = snprintf(msg, APE_ARRAY_LEN(msg), "%s:%d: ", file, line);
    (void)written;
    va_list args;
    va_start(args, format);
    int written_msg = vsnprintf(msg + written, APE_ARRAY_LEN(msg) - written, format, args);
    (void)written_msg;
    va_end(args);

    APE_ASSERT(written_msg <= (APE_ARRAY_LEN(msg) - written));

    printf("%s", msg);
}

char* ape_strndup(ApeAllocator_t* alloc, const char* string, size_t n)
{
    char* output_string = (char*)allocator_malloc(alloc, n + 1);
    if(!output_string)
    {
        return NULL;
    }
    output_string[n] = '\0';
    memcpy(output_string, string, n);
    return output_string;
}

char* ape_strdup(ApeAllocator_t* alloc, const char* string)
{
    if(!string)
    {
        return NULL;
    }
    return ape_strndup(alloc, string, strlen(string));
}

uint64_t ape_double_to_uint64(double val)
{
    union
    {
        uint64_t val_uint64;
        double val_double;
    } temp = { .val_double = val };
    return temp.val_uint64;
}

double ape_uint64_to_double(uint64_t val)
{
    union
    {
        uint64_t val_uint64;
        double val_double;
    } temp = { .val_uint64 = val };
    return temp.val_double;
}

bool ape_timer_platform_supported()
{
#if defined(APE_POSIX) || defined(APE_EMSCRIPTEN) || defined(APE_WINDOWS)
    return true;
#else
    return false;
#endif
}

ApeTimer_t ape_timer_start()
{
    ApeTimer_t timer;
    memset(&timer, 0, sizeof(ApeTimer_t));
#if defined(APE_POSIX)
    // At some point it should be replaced with more accurate per-platform timers
    struct timeval start_time;
    gettimeofday(&start_time, NULL);
    timer.start_offset = start_time.tv_sec;
    timer.start_time_ms = start_time.tv_usec / 1000.0;
#elif defined(APE_WINDOWS)
    LARGE_INTEGER li;
    QueryPerformanceFrequency(&li);// not sure what to do if it fails
    timer.pc_frequency = (double)(li.QuadPart) / 1000.0;
    QueryPerformanceCounter(&li);
    timer.start_time_ms = li.QuadPart / timer.pc_frequency;
#elif defined(APE_EMSCRIPTEN)
    timer.start_time_ms = emscripten_get_now();
#endif
    return timer;
}

double ape_timer_get_elapsed_ms(const ApeTimer_t* timer)
{
#if defined(APE_POSIX)
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    int time_s = (int)((int64_t)current_time.tv_sec - timer->start_offset);
    double current_time_ms = (time_s * 1000) + (current_time.tv_usec / 1000.0);
    return current_time_ms - timer->start_time_ms;
#elif defined(APE_WINDOWS)
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    double current_time_ms = li.QuadPart / timer->pc_frequency;
    return current_time_ms - timer->start_time_ms;
#elif defined(APE_EMSCRIPTEN)
    double current_time_ms = emscripten_get_now();
    return current_time_ms - timer->start_time_ms;
#else
    return 0;
#endif
}

//-----------------------------------------------------------------------------
// Collections
//-----------------------------------------------------------------------------

char* collections_strndup(ApeAllocator_t* alloc, const char* string, size_t n)
{
    char* output_string = (char*)allocator_malloc(alloc, n + 1);
    if(!output_string)
    {
        return NULL;
    }
    output_string[n] = '\0';
    memcpy(output_string, string, n);
    return output_string;
}

char* collections_strdup(ApeAllocator_t* alloc, const char* string)
{
    if(!string)
    {
        return NULL;
    }
    return collections_strndup(alloc, string, strlen(string));
}

unsigned long collections_hash(const void* ptr, size_t len)
{ /* djb2 */
    const uint8_t* ptr_u8 = (const uint8_t*)ptr;
    unsigned long hash = 5381;
    for(size_t i = 0; i < len; i++)
    {
        uint8_t val = ptr_u8[i];
        hash = ((hash << 5) + hash) + val;
    }
    return hash;
}

unsigned int upper_power_of_two(unsigned int v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

//-----------------------------------------------------------------------------
// Allocator
//-----------------------------------------------------------------------------

ApeAllocator_t allocator_make(ApeAllocatorMallocFNCallback_t malloc_fn, ApeAllocatorFreeFNCallback_t free_fn, void* ctx)
{
    ApeAllocator_t alloc;
    alloc.malloc = malloc_fn;
    alloc.free = free_fn;
    alloc.ctx = ctx;
    return alloc;
}

void* allocator_malloc(ApeAllocator_t* allocator, size_t size)
{
    if(!allocator || !allocator->malloc)
    {
        return malloc(size);
    }
    return allocator->malloc(allocator->ctx, size);
}

void allocator_free(ApeAllocator_t* allocator, void* ptr)
{
    if(!allocator || !allocator->free)
    {
        free(ptr);
        return;
    }
    allocator->free(allocator->ctx, ptr);
}


// Public
ApeDictionary_t* dict_make_(ApeAllocator_t* alloc, ApeDictItemCopyFNCallback_t copy_fn, ApeDictItemDestroyFNCallback_t destroy_fn)
{
    ApeDictionary_t* dict = (ApeDictionary_t*)allocator_malloc(alloc, sizeof(ApeDictionary_t));
    if(dict == NULL)
    {
        return NULL;
    }
    bool ok = dict_init(dict, alloc, DICT_INITIAL_SIZE, copy_fn, destroy_fn);
    if(!ok)
    {
        allocator_free(alloc, dict);
        return NULL;
    }
    return dict;
}

void dict_destroy(ApeDictionary_t* dict)
{
    if(!dict)
    {
        return;
    }
    ApeAllocator_t* alloc = dict->alloc;
    dict_deinit(dict, true);
    allocator_free(alloc, dict);
}

void dict_destroy_with_items(ApeDictionary_t* dict)
{
    unsigned int i;
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

    dict_destroy(dict);
}

ApeDictionary_t* dict_copy_with_items(ApeDictionary_t* dict)
{
    int i;
    bool ok;
    const char* key;
    void* item;
    void* item_copy;
    ApeDictionary_t* dict_copy;
    ok = false;
    if(!dict->copy_fn || !dict->destroy_fn)
    {
        return NULL;
    }

    dict_copy = dict_make_(dict->alloc, dict->copy_fn, dict->destroy_fn);
    if(!dict_copy)
    {
        return NULL;
    }
    for(i = 0; i < dict_count(dict); i++)
    {
        key = dict_get_key_at(dict, i);
        item = dict_get_value_at(dict, i);
        item_copy = dict_copy->copy_fn(item);
        if(item && !item_copy)
        {
            dict_destroy_with_items(dict_copy);
            return NULL;
        }
        ok = dict_set(dict_copy, key, item_copy);
        if(!ok)
        {
            dict_copy->destroy_fn(item_copy);
            dict_destroy_with_items(dict_copy);
            return NULL;
        }
    }
    return dict_copy;
}

bool dict_set(ApeDictionary_t* dict, const char* key, void* value)
{
    return dict_set_internal(dict, key, NULL, value);
}

void* dict_get(const ApeDictionary_t* dict, const char* key)
{
    unsigned long hash = hash_string(key);
    bool found = false;
    unsigned long cell_ix = dict_get_cell_ix(dict, key, hash, &found);
    if(found == false)
    {
        return NULL;
    }
    unsigned int item_ix = dict->cells[cell_ix];
    return dict->values[item_ix];
}

void* dict_get_value_at(const ApeDictionary_t* dict, unsigned int ix)
{
    if(ix >= dict->count)
    {
        return NULL;
    }
    return dict->values[ix];
}

const char* dict_get_key_at(const ApeDictionary_t* dict, unsigned int ix)
{
    if(ix >= dict->count)
    {
        return NULL;
    }
    return dict->keys[ix];
}

int dict_count(const ApeDictionary_t* dict)
{
    if(!dict)
    {
        return 0;
    }
    return dict->count;
}


// Private definitions
bool dict_init(ApeDictionary_t* dict, ApeAllocator_t* alloc, unsigned int initial_capacity, ApeDictItemCopyFNCallback_t copy_fn, ApeDictItemDestroyFNCallback_t destroy_fn)
{
    dict->alloc = alloc;
    dict->cells = NULL;
    dict->keys = NULL;
    dict->values = NULL;
    dict->cell_ixs = NULL;
    dict->hashes = NULL;

    dict->count = 0;
    dict->cell_capacity = initial_capacity;
    dict->item_capacity = (unsigned int)(initial_capacity * 0.7f);
    dict->copy_fn = copy_fn;
    dict->destroy_fn = destroy_fn;

    dict->cells = (unsigned int*)allocator_malloc(alloc, dict->cell_capacity * sizeof(*dict->cells));
    dict->keys = (char**)allocator_malloc(alloc, dict->item_capacity * sizeof(*dict->keys));
    dict->values = (void**)allocator_malloc(alloc, dict->item_capacity * sizeof(*dict->values));
    dict->cell_ixs = (unsigned int*)allocator_malloc(alloc, dict->item_capacity * sizeof(*dict->cell_ixs));
    dict->hashes = (long unsigned int*)allocator_malloc(alloc, dict->item_capacity * sizeof(*dict->hashes));
    if(dict->cells == NULL || dict->keys == NULL || dict->values == NULL || dict->cell_ixs == NULL || dict->hashes == NULL)
    {
        goto error;
    }
    for(unsigned int i = 0; i < dict->cell_capacity; i++)
    {
        dict->cells[i] = DICT_INVALID_IX;
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

void dict_deinit(ApeDictionary_t* dict, bool free_keys)
{
    if(free_keys)
    {
        for(unsigned int i = 0; i < dict->count; i++)
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

unsigned int dict_get_cell_ix(const ApeDictionary_t* dict, const char* key, unsigned long hash, bool* out_found)
{
    *out_found = false;
    unsigned int cell_ix = hash & (dict->cell_capacity - 1);
    for(unsigned int i = 0; i < dict->cell_capacity; i++)
    {
        unsigned int ix = (cell_ix + i) & (dict->cell_capacity - 1);
        unsigned int cell = dict->cells[ix];
        if(cell == DICT_INVALID_IX)
        {
            return ix;
        }
        unsigned long hash_to_check = dict->hashes[cell];
        if(hash != hash_to_check)
        {
            continue;
        }
        const char* key_to_check = dict->keys[cell];
        if(strcmp(key, key_to_check) == 0)
        {
            *out_found = true;
            return ix;
        }
    }
    return DICT_INVALID_IX;
}

unsigned long hash_string(const char* str)
{ /* djb2 */
    unsigned long hash = 5381;
    uint8_t c;
    while((c = *str++))
    {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

bool dict_grow_and_rehash(ApeDictionary_t* dict)
{
    ApeDictionary_t new_dict;
    bool ok = dict_init(&new_dict, dict->alloc, dict->cell_capacity * 2, dict->copy_fn, dict->destroy_fn);
    if(!ok)
    {
        return false;
    }
    for(unsigned int i = 0; i < dict->count; i++)
    {
        char* key = dict->keys[i];
        void* value = dict->values[i];
        ok = dict_set_internal(&new_dict, key, key, value);
        if(!ok)
        {
            dict_deinit(&new_dict, false);
            return false;
        }
    }
    dict_deinit(dict, false);
    *dict = new_dict;
    return true;
}

bool dict_set_internal(ApeDictionary_t* dict, const char* ckey, char* mkey, void* value)
{
    unsigned long hash = hash_string(ckey);
    bool found = false;
    unsigned int cell_ix = dict_get_cell_ix(dict, ckey, hash, &found);
    if(found)
    {
        unsigned int item_ix = dict->cells[cell_ix];
        dict->values[item_ix] = value;
        return true;
    }
    if(dict->count >= dict->item_capacity)
    {
        bool ok = dict_grow_and_rehash(dict);
        if(!ok)
        {
            return false;
        }
        cell_ix = dict_get_cell_ix(dict, ckey, hash, &found);
    }

    if(mkey)
    {
        dict->keys[dict->count] = mkey;
    }
    else
    {
        char* key_copy = collections_strdup(dict->alloc, ckey);
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

//-----------------------------------------------------------------------------
// Value dictionary
//-----------------------------------------------------------------------------


// Public
ApeValDictionary_t* valdict_make_(ApeAllocator_t* alloc, size_t key_size, size_t val_size)
{
    return valdict_make_with_capacity(alloc, DICT_INITIAL_SIZE, key_size, val_size);
}

ApeValDictionary_t* valdict_make_with_capacity(ApeAllocator_t* alloc, unsigned int min_capacity, size_t key_size, size_t val_size)
{
    bool ok;
    ApeValDictionary_t* dict;
    unsigned int capacity;
    dict = (ApeValDictionary_t*)allocator_malloc(alloc, sizeof(ApeValDictionary_t));
    capacity = upper_power_of_two(min_capacity * 2);
    if(!dict)
    {
        return NULL;
    }
    ok = valdict_init(dict, alloc, key_size, val_size, capacity);
    if(!ok)
    {
        allocator_free(alloc, dict);
        return NULL;
    }
    return dict;
}

void valdict_destroy(ApeValDictionary_t* dict)
{
    if(!dict)
    {
        return;
    }
    ApeAllocator_t* alloc = dict->alloc;
    valdict_deinit(dict);
    allocator_free(alloc, dict);
}

void valdict_set_hash_function(ApeValDictionary_t* dict, ApeCollectionsHashFNCallback_t hash_fn)
{
    dict->_hash_key = hash_fn;
}

void valdict_set_equals_function(ApeValDictionary_t* dict, ApeCollectionsEqualsFNCallback_t equals_fn)
{
    dict->_keys_equals = equals_fn;
}

bool valdict_set(ApeValDictionary_t* dict, void* key, void* value)
{
    unsigned long hash = valdict_hash_key(dict, key);
    bool found = false;
    unsigned int cell_ix = valdict_get_cell_ix(dict, key, hash, &found);
    if(found)
    {
        unsigned int item_ix = dict->cells[cell_ix];
        valdict_set_value_at(dict, item_ix, value);
        return true;
    }
    if(dict->count >= dict->item_capacity)
    {
        bool ok = valdict_grow_and_rehash(dict);
        if(!ok)
        {
            return false;
        }
        cell_ix = valdict_get_cell_ix(dict, key, hash, &found);
    }
    unsigned int last_ix = dict->count;
    dict->count++;
    dict->cells[cell_ix] = last_ix;
    valdict_set_key_at(dict, last_ix, key);
    valdict_set_value_at(dict, last_ix, value);
    dict->cell_ixs[last_ix] = cell_ix;
    dict->hashes[last_ix] = hash;
    return true;
}

void* valdict_get(const ApeValDictionary_t* dict, const void* key)
{
    unsigned long hash = valdict_hash_key(dict, key);
    bool found = false;
    unsigned long cell_ix = valdict_get_cell_ix(dict, key, hash, &found);
    if(!found)
    {
        return NULL;
    }
    unsigned int item_ix = dict->cells[cell_ix];
    return valdict_get_value_at(dict, item_ix);
}

void* valdict_get_key_at(const ApeValDictionary_t* dict, unsigned int ix)
{
    if(ix >= dict->count)
    {
        return NULL;
    }
    return (char*)dict->keys + (dict->key_size * ix);
}

void* valdict_get_value_at(const ApeValDictionary_t* dict, unsigned int ix)
{
    if(ix >= dict->count)
    {
        return NULL;
    }
    return (char*)dict->values + (dict->val_size * ix);
}

unsigned int valdict_get_capacity(const ApeValDictionary_t* dict)
{
    return dict->item_capacity;
}

bool valdict_set_value_at(const ApeValDictionary_t* dict, unsigned int ix, const void* value)
{
    if(ix >= dict->count)
    {
        return false;
    }
    size_t offset = ix * dict->val_size;
    memcpy((char*)dict->values + offset, value, dict->val_size);
    return true;
}

int valdict_count(const ApeValDictionary_t* dict)
{
    if(!dict)
    {
        return 0;
    }
    return dict->count;
}

bool valdict_remove(ApeValDictionary_t* dict, void* key)
{
    unsigned long hash = valdict_hash_key(dict, key);
    bool found = false;
    unsigned int cell = valdict_get_cell_ix(dict, key, hash, &found);
    if(!found)
    {
        return false;
    }

    unsigned int item_ix = dict->cells[cell];
    unsigned int last_item_ix = dict->count - 1;
    if(item_ix < last_item_ix)
    {
        void* last_key = valdict_get_key_at(dict, last_item_ix);
        valdict_set_key_at(dict, item_ix, last_key);
        void* last_value = valdict_get_key_at(dict, last_item_ix);
        valdict_set_value_at(dict, item_ix, last_value);
        dict->cell_ixs[item_ix] = dict->cell_ixs[last_item_ix];
        dict->hashes[item_ix] = dict->hashes[last_item_ix];
        dict->cells[dict->cell_ixs[item_ix]] = item_ix;
    }
    dict->count--;

    unsigned int i = cell;
    unsigned int j = i;
    for(unsigned int x = 0; x < (dict->cell_capacity - 1); x++)
    {
        j = (j + 1) & (dict->cell_capacity - 1);
        if(dict->cells[j] == VALDICT_INVALID_IX)
        {
            break;
        }
        unsigned int k = dict->hashes[dict->cells[j]] & (dict->cell_capacity - 1);
        if((j > i && (k <= i || k > j)) || (j < i && (k <= i && k > j)))
        {
            dict->cell_ixs[dict->cells[j]] = i;
            dict->cells[i] = dict->cells[j];
            i = j;
        }
    }
    dict->cells[i] = VALDICT_INVALID_IX;
    return true;
}

void valdict_clear(ApeValDictionary_t* dict)
{
    dict->count = 0;
    for(unsigned int i = 0; i < dict->cell_capacity; i++)
    {
        dict->cells[i] = VALDICT_INVALID_IX;
    }
}

// Private definitions
bool valdict_init(ApeValDictionary_t* dict, ApeAllocator_t* alloc, size_t key_size, size_t val_size, unsigned int initial_capacity)
{
    dict->alloc = alloc;
    dict->key_size = key_size;
    dict->val_size = val_size;
    dict->cells = NULL;
    dict->keys = NULL;
    dict->values = NULL;
    dict->cell_ixs = NULL;
    dict->hashes = NULL;
    dict->count = 0;
    dict->cell_capacity = initial_capacity;
    dict->item_capacity = (unsigned int)(initial_capacity * 0.7f);
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
    for(unsigned int i = 0; i < dict->cell_capacity; i++)
    {
        dict->cells[i] = VALDICT_INVALID_IX;
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

void valdict_deinit(ApeValDictionary_t* dict)
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

unsigned int valdict_get_cell_ix(const ApeValDictionary_t* dict, const void* key, unsigned long hash, bool* out_found)
{
    *out_found = false;
    bool are_equal;
    unsigned int ofs;
    unsigned int i;
    unsigned int ix;
    unsigned int cell;
    unsigned int cell_ix;
    unsigned long hash_to_check;
    void* key_to_check;
    //fprintf(stderr, "valdict_get_cell_ix: dict=%p, dict->cell_capacity=%d\n", dict, dict->cell_capacity);
    ofs = 0;
    if(dict->cell_capacity > 1)
    {
        ofs = (dict->cell_capacity - 1);
    }
    cell_ix = hash & ofs;
    for(i = 0; i < dict->cell_capacity; i++)
    {
        cell = VALDICT_INVALID_IX;
        ix = (cell_ix + i) & ofs;
        //fprintf(stderr, "(cell_ix=%d + i=%d) & ofs=%d == %d\n", cell_ix, i, ofs, ix);
        cell = dict->cells[ix];
        if(cell == VALDICT_INVALID_IX)
        {
            return ix;
        }
        hash_to_check = dict->hashes[cell];
        if(hash != hash_to_check)
        {
            continue;
        }
        key_to_check = valdict_get_key_at(dict, cell);
        are_equal = valdict_keys_are_equal(dict, key, key_to_check);
        if(are_equal)
        {
            *out_found = true;
            return ix;
        }
    }
    return VALDICT_INVALID_IX;
}

bool valdict_grow_and_rehash(ApeValDictionary_t* dict)
{
    ApeValDictionary_t new_dict;
    unsigned new_capacity = dict->cell_capacity == 0 ? DICT_INITIAL_SIZE : dict->cell_capacity * 2;
    bool ok = valdict_init(&new_dict, dict->alloc, dict->key_size, dict->val_size, new_capacity);
    if(!ok)
    {
        return false;
    }
    new_dict._keys_equals = dict->_keys_equals;
    new_dict._hash_key = dict->_hash_key;
    for(unsigned int i = 0; i < dict->count; i++)
    {
        char* key = (char*)valdict_get_key_at(dict, i);
        void* value = valdict_get_value_at(dict, i);
        ok = valdict_set(&new_dict, key, value);
        if(!ok)
        {
            valdict_deinit(&new_dict);
            return false;
        }
    }
    valdict_deinit(dict);
    *dict = new_dict;
    return true;
}

bool valdict_set_key_at(ApeValDictionary_t* dict, unsigned int ix, void* key)
{
    if(ix >= dict->count)
    {
        return false;
    }
    size_t offset = ix * dict->key_size;
    memcpy((char*)dict->keys + offset, key, dict->key_size);
    return true;
}

bool valdict_keys_are_equal(const ApeValDictionary_t* dict, const void* a, const void* b)
{
    if(dict->_keys_equals)
    {
        return dict->_keys_equals(a, b);
    }
    else
    {
        return memcmp(a, b, dict->key_size) == 0;
    }
}

unsigned long valdict_hash_key(const ApeValDictionary_t* dict, const void* key)
{
    if(dict->_hash_key)
    {
        return dict->_hash_key(key);
    }
    else
    {
        return collections_hash(key, dict->key_size);
    }
}

//-----------------------------------------------------------------------------
// Pointer dictionary
//-----------------------------------------------------------------------------

// Public
ApePtrDictionary_t* ptrdict_make(ApeAllocator_t* alloc)
{
    ApePtrDictionary_t* dict = (ApePtrDictionary_t*)allocator_malloc(alloc, sizeof(ApePtrDictionary_t));
    if(!dict)
    {
        return NULL;
    }
    dict->alloc = alloc;
    bool ok = valdict_init(&dict->vd, alloc, sizeof(void*), sizeof(void*), DICT_INITIAL_SIZE);
    if(!ok)
    {
        allocator_free(alloc, dict);
        return NULL;
    }
    return dict;
}

void ptrdict_destroy(ApePtrDictionary_t* dict)
{
    if(dict == NULL)
    {
        return;
    }
    valdict_deinit(&dict->vd);
    allocator_free(dict->alloc, dict);
}

void ptrdict_set_hash_function(ApePtrDictionary_t* dict, ApeCollectionsHashFNCallback_t hash_fn)
{
    valdict_set_hash_function(&dict->vd, hash_fn);
}

void ptrdict_set_equals_function(ApePtrDictionary_t* dict, ApeCollectionsEqualsFNCallback_t equals_fn)
{
    valdict_set_equals_function(&dict->vd, equals_fn);
}

bool ptrdict_set(ApePtrDictionary_t* dict, void* key, void* value)
{
    return valdict_set(&dict->vd, &key, &value);
}

void* ptrdict_get(const ApePtrDictionary_t* dict, const void* key)
{
    void* res = (void*)valdict_get(&dict->vd, &key);
    if(!res)
    {
        return NULL;
    }
    return *(void**)res;
}

void* ptrdict_get_value_at(const ApePtrDictionary_t* dict, unsigned int ix)
{
    void* res = valdict_get_value_at(&dict->vd, ix);
    if(!res)
    {
        return NULL;
    }
    return *(void**)res;
}

void* ptrdict_get_key_at(const ApePtrDictionary_t* dict, unsigned int ix)
{
    void* res = valdict_get_key_at(&dict->vd, ix);
    if(!res)
    {
        return NULL;
    }
    return *(void**)res;
}

int ptrdict_count(const ApePtrDictionary_t* dict)
{
    return valdict_count(&dict->vd);
}

bool ptrdict_remove(ApePtrDictionary_t* dict, void* key)
{
    return valdict_remove(&dict->vd, &key);
}

//-----------------------------------------------------------------------------
// Array
//-----------------------------------------------------------------------------


ApeArray_t* array_make_(ApeAllocator_t* alloc, size_t element_size)
{
    return array_make_with_capacity(alloc, 32, element_size);
}

ApeArray_t* array_make_with_capacity(ApeAllocator_t* alloc, unsigned int capacity, size_t element_size)
{
    ApeArray_t* arr = (ApeArray_t*)allocator_malloc(alloc, sizeof(ApeArray_t));
    if(!arr)
    {
        return NULL;
    }

    bool ok = array_init_with_capacity(arr, alloc, capacity, element_size);
    if(!ok)
    {
        allocator_free(alloc, arr);
        return NULL;
    }
    return arr;
}

void array_destroy(ApeArray_t* arr)
{
    if(!arr)
    {
        return;
    }
    ApeAllocator_t* alloc = arr->alloc;
    array_deinit(arr);
    allocator_free(alloc, arr);
}

void array_destroy_with_items_(ApeArray_t* arr, ApeArrayItemDeinitFNCallback_t deinit_fn)
{
    for(int i = 0; i < array_count(arr); i++)
    {
        void* item = (void*)array_get(arr, i);
        deinit_fn(item);
    }
    array_destroy(arr);
}

ApeArray_t* array_copy(const ApeArray_t* arr)
{
    ApeArray_t* copy = (ApeArray_t*)allocator_malloc(arr->alloc, sizeof(ApeArray_t));
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
    if(arr->count >= arr->capacity)
    {
        COLLECTIONS_ASSERT(!arr->lock_capacity);
        if(arr->lock_capacity)
        {
            return false;
        }
        unsigned int new_capacity = arr->capacity > 0 ? arr->capacity * 2 : 1;
        unsigned char* new_data = (unsigned char*)allocator_malloc(arr->alloc, new_capacity * arr->element_size);
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

bool array_addn(ApeArray_t* arr, const void* values, int n)
{
    for(int i = 0; i < n; i++)
    {
        const uint8_t* value = NULL;
        if(values)
        {
            value = (const uint8_t*)values + (i * arr->element_size);
        }
        bool ok = array_add(arr, value);
        if(!ok)
        {
            return false;
        }
    }
    return true;
}

bool array_add_array(ApeArray_t* dest, ApeArray_t* source)
{
    COLLECTIONS_ASSERT(dest->element_size == source->element_size);
    if(dest->element_size != source->element_size)
    {
        return false;
    }
    int dest_before_count = array_count(dest);
    for(int i = 0; i < array_count(source); i++)
    {
        void* item = (void*)array_get(source, i);
        bool ok = array_add(dest, item);
        if(!ok)
        {
            dest->count = dest_before_count;
            return false;
        }
    }
    return true;
}

bool array_push(ApeArray_t* arr, const void* value)
{
    return array_add(arr, value);
}

bool array_pop(ApeArray_t* arr, void* out_value)
{
    if(arr->count <= 0)
    {
        return false;
    }
    if(out_value)
    {
        void* res = (void*)array_get(arr, arr->count - 1);
        memcpy(out_value, res, arr->element_size);
    }
    array_remove_at(arr, arr->count - 1);
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

bool array_set(ApeArray_t* arr, unsigned int ix, void* value)
{
    if(ix >= arr->count)
    {
        COLLECTIONS_ASSERT(false);
        return false;
    }
    size_t offset = ix * arr->element_size;
    memmove(arr->arraydata + offset, value, arr->element_size);
    return true;
}

bool array_setn(ApeArray_t* arr, unsigned int ix, void* values, int n)
{
    for(int i = 0; i < n; i++)
    {
        int dest_ix = ix + i;
        unsigned char* value = (unsigned char*)values + (i * arr->element_size);
        if(dest_ix < array_count(arr))
        {
            bool ok = array_set(arr, dest_ix, value);
            if(!ok)
            {
                return false;
            }
        }
        else
        {
            bool ok = array_add(arr, value);
            if(!ok)
            {
                return false;
            }
        }
    }
    return true;
}

void* array_get(ApeArray_t* arr, unsigned int ix)
{
    if(ix >= arr->count)
    {
        COLLECTIONS_ASSERT(false);
        return NULL;
    }
    size_t offset = ix * arr->element_size;
    return arr->arraydata + offset;
}

const void* array_get_const(const ApeArray_t* arr, unsigned int ix)
{
    if(ix >= arr->count)
    {
        COLLECTIONS_ASSERT(false);
        return NULL;
    }
    size_t offset = ix * arr->element_size;
    return arr->arraydata + offset;
}

void* array_get_last(ApeArray_t* arr)
{
    if(arr->count <= 0)
    {
        return NULL;
    }
    return array_get(arr, arr->count - 1);
}

int array_count(const ApeArray_t* arr)
{
    if(!arr)
    {
        return 0;
    }
    return arr->count;
}

unsigned int array_get_capacity(const ApeArray_t* arr)
{
    return arr->capacity;
}

bool array_remove_at(ApeArray_t* arr, unsigned int ix)
{
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
    size_t to_move_bytes = (arr->count - 1 - ix) * arr->element_size;
    void* dest = arr->arraydata + (ix * arr->element_size);
    void* src = arr->arraydata + ((ix + 1) * arr->element_size);
    memmove(dest, src, to_move_bytes);
    arr->count--;
    return true;
}

bool array_remove_item(ApeArray_t* arr, void* ptr)
{
    int ix = array_get_index(arr, ptr);
    if(ix < 0)
    {
        return false;
    }
    return array_remove_at(arr, ix);
}

void array_clear(ApeArray_t* arr)
{
    arr->count = 0;
}

void array_clear_and_deinit_items_(ApeArray_t* arr, ApeArrayItemDeinitFNCallback_t deinit_fn)
{
    for(int i = 0; i < array_count(arr); i++)
    {
        void* item = array_get(arr, i);
        deinit_fn(item);
    }
    arr->count = 0;
}

void array_lock_capacity(ApeArray_t* arr)
{
    arr->lock_capacity = true;
}

int array_get_index(const ApeArray_t* arr, void* ptr)
{
    for(int i = 0; i < array_count(arr); i++)
    {
        if(array_get_const(arr, i) == ptr)
        {
            return i;
        }
    }
    return -1;
}

bool array_contains(const ApeArray_t* arr, void* ptr)
{
    return array_get_index(arr, ptr) >= 0;
}

void* array_data(ApeArray_t* arr)
{
    return arr->arraydata;
}

const void* array_const_data(const ApeArray_t* arr)
{
    return arr->arraydata;
}

void array_orphan_data(ApeArray_t* arr)
{
    array_init_with_capacity(arr, arr->alloc, 0, arr->element_size);
}

bool array_reverse(ApeArray_t* arr)
{
    int count = array_count(arr);
    if(count < 2)
    {
        return true;
    }
    void* temp = (void*)allocator_malloc(arr->alloc, arr->element_size);
    if(!temp)
    {
        return false;
    }
    for(int a_ix = 0; a_ix < (count / 2); a_ix++)
    {
        int b_ix = count - a_ix - 1;
        void* a = array_get(arr, a_ix);
        void* b = array_get(arr, b_ix);
        memcpy(temp, a, arr->element_size);
        array_set(arr, a_ix, b);// no need for check because it will be within range
        array_set(arr, b_ix, temp);
    }
    allocator_free(arr->alloc, temp);
    return true;
}

bool array_init_with_capacity(ApeArray_t* arr, ApeAllocator_t* alloc, unsigned int capacity, size_t element_size)
{
    arr->alloc = alloc;
    if(capacity > 0)
    {
        arr->data_allocated = (unsigned char*)allocator_malloc(arr->alloc, capacity * element_size);
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

//-----------------------------------------------------------------------------
// Pointer Array
//-----------------------------------------------------------------------------


ApePtrArray_t* ptrarray_make(ApeAllocator_t* alloc)
{
    return ptrarray_make_with_capacity(alloc, 0);
}

ApePtrArray_t* ptrarray_make_with_capacity(ApeAllocator_t* alloc, unsigned int capacity)
{
    ApePtrArray_t* ptrarr = (ApePtrArray_t*)allocator_malloc(alloc, sizeof(ApePtrArray_t));
    if(!ptrarr)
    {
        return NULL;
    }
    ptrarr->alloc = alloc;
    bool ok = array_init_with_capacity(&ptrarr->arr, alloc, capacity, sizeof(void*));
    if(!ok)
    {
        allocator_free(alloc, ptrarr);
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

void ptrarray_destroy_with_items_(ApePtrArray_t* arr, ApePtrArrayItemDestroyFNCallback_t destroy_fn)
{// todo: destroy and copy in make fn
    if(arr == NULL)
    {
        return;
    }

    if(destroy_fn)
    {
        ptrarray_clear_and_destroy_items_(arr, destroy_fn);
    }

    ptrarray_destroy(arr);
}

ApePtrArray_t* ptrarray_copy(ApePtrArray_t* arr)
{
    ApePtrArray_t* arr_copy = ptrarray_make_with_capacity(arr->alloc, arr->arr.capacity);
    if(!arr_copy)
    {
        return NULL;
    }
    for(int i = 0; i < ptrarray_count(arr); i++)
    {
        void* item = ptrarray_get(arr, i);
        bool ok = ptrarray_add(arr_copy, item);
        if(!ok)
        {
            ptrarray_destroy(arr_copy);
            return NULL;
        }
    }
    return arr_copy;
}

ApePtrArray_t* ptrarray_copy_with_items_(ApePtrArray_t* arr, ApePtrArrayItemCopyFNCallback_t copy_fn, ApePtrArrayItemDestroyFNCallback_t destroy_fn)
{
    ApePtrArray_t* arr_copy = ptrarray_make_with_capacity(arr->alloc, arr->arr.capacity);
    if(!arr_copy)
    {
        return NULL;
    }
    for(int i = 0; i < ptrarray_count(arr); i++)
    {
        void* item = ptrarray_get(arr, i);
        void* item_copy = copy_fn(item);
        if(item && !item_copy)
        {
            goto err;
        }
        bool ok = ptrarray_add(arr_copy, item_copy);
        if(!ok)
        {
            goto err;
        }
    }
    return arr_copy;
err:
    ptrarray_destroy_with_items_(arr_copy, destroy_fn);
    return NULL;
}

bool ptrarray_add(ApePtrArray_t* arr, void* ptr)
{
    return array_add(&arr->arr, &ptr);
}

bool ptrarray_set(ApePtrArray_t* arr, unsigned int ix, void* ptr)
{
    return array_set(&arr->arr, ix, &ptr);
}

bool ptrarray_add_array(ApePtrArray_t* dest, ApePtrArray_t* source)
{
    return array_add_array(&dest->arr, &source->arr);
}

void* ptrarray_get(ApePtrArray_t* arr, unsigned int ix)
{
    void* res = array_get(&arr->arr, ix);
    if(!res)
    {
        return NULL;
    }
    return *(void**)res;
}

const void* ptrarray_get_const(const ApePtrArray_t* arr, unsigned int ix)
{
    const void* res = array_get_const(&arr->arr, ix);
    if(!res)
    {
        return NULL;
    }
    return *(void* const*)res;
}

bool ptrarray_push(ApePtrArray_t* arr, void* ptr)
{
    return ptrarray_add(arr, ptr);
}


void* ptrarray_pop(ApePtrArray_t* arr)
{
    int ix = ptrarray_count(arr) - 1;
    void* res = ptrarray_get(arr, ix);
    ptrarray_remove_at(arr, ix);
    return res;
}

void* ptrarray_top(ApePtrArray_t* arr)
{
    int count = ptrarray_count(arr);
    if(count == 0)
    {
        return NULL;
    }
    return ptrarray_get(arr, count - 1);
}

int ptrarray_count(const ApePtrArray_t* arr)
{
    if(!arr)
    {
        return 0;
    }
    return array_count(&arr->arr);
}

bool ptrarray_remove_at(ApePtrArray_t* arr, unsigned int ix)
{
    return array_remove_at(&arr->arr, ix);
}

bool ptrarray_remove_item(ApePtrArray_t* arr, void* item)
{
    for(int i = 0; i < ptrarray_count(arr); i++)
    {
        if(item == ptrarray_get(arr, i))
        {
            ptrarray_remove_at(arr, i);
            return true;
        }
    }
    COLLECTIONS_ASSERT(false);
    return false;
}

void ptrarray_clear(ApePtrArray_t* arr)
{
    array_clear(&arr->arr);
}

void ptrarray_clear_and_destroy_items_(ApePtrArray_t* arr, ApePtrArrayItemDestroyFNCallback_t destroy_fn)
{
    for(int i = 0; i < ptrarray_count(arr); i++)
    {
        void* item = ptrarray_get(arr, i);
        destroy_fn(item);
    }
    ptrarray_clear(arr);
}

void ptrarray_lock_capacity(ApePtrArray_t* arr)
{
    array_lock_capacity(&arr->arr);
}

int ptrarray_get_index(ApePtrArray_t* arr, void* ptr)
{
    for(int i = 0; i < ptrarray_count(arr); i++)
    {
        if(ptrarray_get(arr, i) == ptr)
        {
            return i;
        }
    }
    return -1;
}

bool ptrarray_contains(ApePtrArray_t* arr, void* item)
{
    return ptrarray_get_index(arr, item) >= 0;
}

void* ptrarray_get_addr(ApePtrArray_t* arr, unsigned int ix)
{
    void* res = array_get(&arr->arr, ix);
    if(res == NULL)
    {
        return NULL;
    }
    return res;
}

void* ptrarray_data(ApePtrArray_t* arr)
{
    return array_data(&arr->arr);
}

void ptrarray_reverse(ApePtrArray_t* arr)
{
    array_reverse(&arr->arr);
}

//-----------------------------------------------------------------------------
// String buffer
//-----------------------------------------------------------------------------


ApeStringBuffer_t* strbuf_make(ApeAllocator_t* alloc)
{
    return strbuf_make_with_capacity(alloc, 1);
}

ApeStringBuffer_t* strbuf_make_with_capacity(ApeAllocator_t* alloc, unsigned int capacity)
{
    ApeStringBuffer_t* buf = (ApeStringBuffer_t*)allocator_malloc(alloc, sizeof(ApeStringBuffer_t));
    if(buf == NULL)
    {
        return NULL;
    }
    memset(buf, 0, sizeof(ApeStringBuffer_t));
    buf->alloc = alloc;
    buf->failed = false;
    buf->stringdata = (char*)allocator_malloc(alloc, capacity);
    if(buf->stringdata == NULL)
    {
        allocator_free(alloc, buf);
        return NULL;
    }
    buf->capacity = capacity;
    buf->len = 0;
    buf->stringdata[0] = '\0';
    return buf;
}

void strbuf_destroy(ApeStringBuffer_t* buf)
{
    if(buf == NULL)
    {
        return;
    }
    allocator_free(buf->alloc, buf->stringdata);
    allocator_free(buf->alloc, buf);
}

void strbuf_clear(ApeStringBuffer_t* buf)
{
    if(buf->failed)
    {
        return;
    }
    buf->len = 0;
    buf->stringdata[0] = '\0';
}

bool strbuf_append(ApeStringBuffer_t* buf, const char* str)
{
    if(buf->failed)
    {
        return false;
    }
    size_t str_len = strlen(str);
    if(str_len == 0)
    {
        return true;
    }
    size_t required_capacity = buf->len + str_len + 1;
    if(required_capacity > buf->capacity)
    {
        bool ok = strbuf_grow(buf, required_capacity * 2);
        if(!ok)
        {
            return false;
        }
    }
    memcpy(buf->stringdata + buf->len, str, str_len);
    buf->len = buf->len + str_len;
    buf->stringdata[buf->len] = '\0';
    return true;
}

bool strbuf_appendf(ApeStringBuffer_t* buf, const char* fmt, ...)
{
    if(buf->failed)
    {
        return false;
    }
    va_list args;
    va_start(args, fmt);
    int to_write = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if(to_write == 0)
    {
        return true;
    }
    size_t required_capacity = buf->len + to_write + 1;
    if(required_capacity > buf->capacity)
    {
        bool ok = strbuf_grow(buf, required_capacity * 2);
        if(!ok)
        {
            return false;
        }
    }
    va_start(args, fmt);
    int written = vsprintf(buf->stringdata + buf->len, fmt, args);
    (void)written;
    va_end(args);
    if(written != to_write)
    {
        return false;
    }
    buf->len = buf->len + to_write;
    buf->stringdata[buf->len] = '\0';
    return true;
}

const char* strbuf_get_string(const ApeStringBuffer_t* buf)
{
    if(buf->failed)
    {
        return NULL;
    }
    return buf->stringdata;
}

size_t strbuf_get_length(const ApeStringBuffer_t* buf)
{
    if(buf->failed)
    {
        return 0;
    }
    return buf->len;
}

char* strbuf_get_string_and_destroy(ApeStringBuffer_t* buf)
{
    if(buf->failed)
    {
        strbuf_destroy(buf);
        return NULL;
    }
    char* res = buf->stringdata;
    buf->stringdata = NULL;
    strbuf_destroy(buf);
    return res;
}

bool strbuf_failed(ApeStringBuffer_t* buf)
{
    return buf->failed;
}

bool strbuf_grow(ApeStringBuffer_t* buf, size_t new_capacity)
{
    char* new_data = (char*)allocator_malloc(buf->alloc, new_capacity);
    if(new_data == NULL)
    {
        buf->failed = true;
        return false;
    }
    memcpy(new_data, buf->stringdata, buf->len);
    new_data[buf->len] = '\0';
    allocator_free(buf->alloc, buf->stringdata);
    buf->stringdata = new_data;
    buf->capacity = new_capacity;
    return true;
}

//-----------------------------------------------------------------------------
// Utils
//-----------------------------------------------------------------------------

ApePtrArray_t * kg_split_string(ApeAllocator_t* alloc, const char* str, const char* delimiter)
{
    int i;
    long len;
    bool ok;
    ApePtrArray_t* res;
    char* line;
    char* rest;
    const char* line_start;
    const char* line_end;
    ok = false;
    res = ptrarray_make(alloc);
    rest = NULL;
    if(!str)
    {
        return res;
    }

    line_start = str;
    line_end = strstr(line_start, delimiter);
    while(line_end != NULL)
    {
        len = line_end - line_start;
        line = collections_strndup(alloc, line_start, len);
        if(!line)
        {
            goto err;
        }
        ok = ptrarray_add(res, line);
        if(!ok)
        {
            allocator_free(alloc, line);
            goto err;
        }
        line_start = line_end + 1;
        line_end = strstr(line_start, delimiter);
    }
    rest = collections_strdup(alloc, line_start);
    if(!rest)
    {
        goto err;
    }
    ok = ptrarray_add(res, rest);
    if(!ok)
    {
        goto err;
    }
    return res;
err:
    allocator_free(alloc, rest);
    if(res)
    {
        for(i = 0; i < ptrarray_count(res); i++)
        {
            line = (char*)ptrarray_get(res, i);
            allocator_free(alloc, line);
        }
    }
    ptrarray_destroy(res);
    return NULL;
}

char* kg_join(ApeAllocator_t* alloc, ApePtrArray_t * items, const char* with)
{
    int i;
    char* item;
    ApeStringBuffer_t* res;
    res = strbuf_make(alloc);
    if(!res)
    {
        return NULL;
    }
    for(i = 0; i < ptrarray_count(items); i++)
    {
        item = (char*)ptrarray_get(items, i);
        strbuf_append(res, item);
        if(i < (ptrarray_count(items) - 1))
        {
            strbuf_append(res, with);
        }
    }
    return strbuf_get_string_and_destroy(res);
}

char* kg_canonicalise_path(ApeAllocator_t* alloc, const char* path)
{
    if(!strchr(path, '/') || (!strstr(path, "/../") && !strstr(path, "./")))
    {
        return collections_strdup(alloc, path);
    }

    ApePtrArray_t* split = kg_split_string(alloc, path, "/");
    if(!split)
    {
        return NULL;
    }

    for(int i = 0; i < ptrarray_count(split) - 1; i++)
    {
        char* item = (char*)ptrarray_get(split, i);
        char* next_item = (char*)ptrarray_get(split, i + 1);
        if(kg_streq(item, "."))
        {
            allocator_free(alloc, item);
            ptrarray_remove_at(split, i);
            i = -1;
            continue;
        }
        if(kg_streq(next_item, ".."))
        {
            allocator_free(alloc, item);
            allocator_free(alloc, next_item);
            ptrarray_remove_at(split, i);
            ptrarray_remove_at(split, i);
            i = -1;
            continue;
        }
    }

    char* joined = kg_join(alloc, split, "/");
    for(int i = 0; i < ptrarray_count(split); i++)
    {
        void* item = (void*)ptrarray_get(split, i);
        allocator_free(alloc, item);
    }
    ptrarray_destroy(split);
    return joined;
}

bool kg_is_path_absolute(const char* path)
{
    return path[0] == '/';
}

bool kg_streq(const char* a, const char* b)
{
    return strcmp(a, b) == 0;
}

void errors_init(ApeErrorList_t* errors)
{
    memset(errors, 0, sizeof(ApeErrorList_t));
    errors->count = 0;
}

void errors_deinit(ApeErrorList_t* errors)
{
    errors_clear(errors);
}

void errors_add_error(ApeErrorList_t* errors, ApeErrorType_t type, ApePosition_t pos, const char* message)
{
    if(errors->count >= ERRORS_MAX_COUNT)
    {
        return;
    }
    ApeError_t err;
    memset(&err, 0, sizeof(ApeError_t));
    err.type = type;
    int len = (int)strlen(message);
    int to_copy = len;
    if(to_copy >= (APE_ERROR_MESSAGE_MAX_LENGTH - 1))
    {
        to_copy = APE_ERROR_MESSAGE_MAX_LENGTH - 1;
    }
    memcpy(err.message, message, to_copy);
    err.message[to_copy] = '\0';
    err.pos = pos;
    err.traceback = NULL;
    errors->errors[errors->count] = err;
    errors->count++;
}

void errors_add_errorf(ApeErrorList_t* errors, ApeErrorType_t type, ApePosition_t pos, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int to_write = vsnprintf(NULL, 0, format, args);
    (void)to_write;
    va_end(args);
    va_start(args, format);
    char res[APE_ERROR_MESSAGE_MAX_LENGTH];
    int written = vsnprintf(res, APE_ERROR_MESSAGE_MAX_LENGTH, format, args);
    (void)written;
    APE_ASSERT(to_write == written);
    va_end(args);
    errors_add_error(errors, type, pos, res);
}

void errors_clear(ApeErrorList_t* errors)
{
    for(int i = 0; i < errors_get_count(errors); i++)
    {
        ApeError_t* error = errors_get(errors, i);
        if(error->traceback)
        {
            traceback_destroy(error->traceback);
        }
    }
    errors->count = 0;
}

int errors_get_count(const ApeErrorList_t* errors)
{
    return errors->count;
}

ApeError_t* errors_get(ApeErrorList_t* errors, int ix)
{
    if(ix >= errors->count)
    {
        return NULL;
    }
    return &errors->errors[ix];
}

const ApeError_t* errors_getc(const ApeErrorList_t* errors, int ix)
{
    if(ix >= errors->count)
    {
        return NULL;
    }
    return &errors->errors[ix];
}


ApeError_t* errors_get_last_error(ApeErrorList_t* errors)
{
    if(errors->count <= 0)
    {
        return NULL;
    }
    return &errors->errors[errors->count - 1];
}

bool errors_has_errors(const ApeErrorList_t* errors)
{
    return errors_get_count(errors) > 0;
}


void token_init(ApeToken_t* tok, ApeTokenType_t type, const char* literal, int len)
{
    tok->type = type;
    tok->literal = literal;
    tok->len = len;
}

char* token_duplicate_literal(ApeAllocator_t* alloc, const ApeToken_t* tok)
{
    return ape_strndup(alloc, tok->literal, tok->len);
}



ApeCompiledFile_t* compiled_file_make(ApeAllocator_t* alloc, const char* path)
{
    ApeCompiledFile_t* file = (ApeCompiledFile_t*)allocator_malloc(alloc, sizeof(ApeCompiledFile_t));
    if(!file)
    {
        return NULL;
    }
    memset(file, 0, sizeof(ApeCompiledFile_t));
    file->alloc = alloc;
    const char* last_slash_pos = strrchr(path, '/');
    if(last_slash_pos)
    {
        size_t len = last_slash_pos - path + 1;
        file->dir_path = ape_strndup(alloc, path, len);
    }
    else
    {
        file->dir_path = ape_strdup(alloc, "");
    }
    if(!file->dir_path)
    {
        goto error;
    }
    file->path = ape_strdup(alloc, path);
    if(!file->path)
    {
        goto error;
    }
    file->lines = ptrarray_make(alloc);
    if(!file->lines)
    {
        goto error;
    }
    return file;
error:
    compiled_file_destroy(file);
    return NULL;
}

void compiled_file_destroy(ApeCompiledFile_t* file)
{
    if(!file)
    {
        return;
    }
    for(int i = 0; i < ptrarray_count(file->lines); i++)
    {
        void* item = (void*)ptrarray_get(file->lines, i);
        allocator_free(file->alloc, item);
    }
    ptrarray_destroy(file->lines);
    allocator_free(file->alloc, file->dir_path);
    allocator_free(file->alloc, file->path);
    allocator_free(file->alloc, file);
}




ApeExpression_t* expression_make_ident(ApeAllocator_t* alloc, ApeIdent_t* ident)
{
    ApeExpression_t* res = expression_make(alloc, EXPRESSION_IDENT);
    if(!res)
    {
        return NULL;
    }
    res->ident = ident;
    return res;
}

ApeExpression_t* expression_make_number_literal(ApeAllocator_t* alloc, double val)
{
    ApeExpression_t* res = expression_make(alloc, EXPRESSION_NUMBER_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->number_literal = val;
    return res;
}

ApeExpression_t* expression_make_bool_literal(ApeAllocator_t* alloc, bool val)
{
    ApeExpression_t* res = expression_make(alloc, EXPRESSION_BOOL_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->bool_literal = val;
    return res;
}

ApeExpression_t* expression_make_string_literal(ApeAllocator_t* alloc, char* value)
{
    ApeExpression_t* res = expression_make(alloc, EXPRESSION_STRING_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->string_literal = value;
    return res;
}

ApeExpression_t* expression_make_null_literal(ApeAllocator_t* alloc)
{
    ApeExpression_t* res = expression_make(alloc, EXPRESSION_NULL_LITERAL);
    if(!res)
    {
        return NULL;
    }
    return res;
}

ApeExpression_t* expression_make_array_literal(ApeAllocator_t* alloc, ApePtrArray_t * values)
{
    ApeExpression_t* res = expression_make(alloc, EXPRESSION_ARRAY_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->array = values;
    return res;
}

ApeExpression_t* expression_make_map_literal(ApeAllocator_t* alloc, ApePtrArray_t * keys, ApePtrArray_t * values)
{
    ApeExpression_t* res = expression_make(alloc, EXPRESSION_MAP_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->map.keys = keys;
    res->map.values = values;
    return res;
}

ApeExpression_t* expression_make_prefix(ApeAllocator_t* alloc, ApeOperator_t op, ApeExpression_t* right)
{
    ApeExpression_t* res = expression_make(alloc, EXPRESSION_PREFIX);
    if(!res)
    {
        return NULL;
    }
    res->prefix.op = op;
    res->prefix.right = right;
    return res;
}

ApeExpression_t* expression_make_infix(ApeAllocator_t* alloc, ApeOperator_t op, ApeExpression_t* left, ApeExpression_t* right)
{
    ApeExpression_t* res = expression_make(alloc, EXPRESSION_INFIX);
    if(!res)
    {
        return NULL;
    }
    res->infix.op = op;
    res->infix.left = left;
    res->infix.right = right;
    return res;
}

ApeExpression_t* expression_make_fn_literal(ApeAllocator_t* alloc, ApePtrArray_t * params, ApeCodeblock_t* body)
{
    ApeExpression_t* res = expression_make(alloc, EXPRESSION_FUNCTION_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->fn_literal.name = NULL;
    res->fn_literal.params = params;
    res->fn_literal.body = body;
    return res;
}

ApeExpression_t* expression_make_call(ApeAllocator_t* alloc, ApeExpression_t* function, ApePtrArray_t * args)
{
    ApeExpression_t* res = expression_make(alloc, EXPRESSION_CALL);
    if(!res)
    {
        return NULL;
    }
    res->call_expr.function = function;
    res->call_expr.args = args;
    return res;
}

ApeExpression_t* expression_make_index(ApeAllocator_t* alloc, ApeExpression_t* left, ApeExpression_t* index)
{
    ApeExpression_t* res = expression_make(alloc, EXPRESSION_INDEX);
    if(!res)
    {
        return NULL;
    }
    res->index_expr.left = left;
    res->index_expr.index = index;
    return res;
}

ApeExpression_t* expression_make_assign(ApeAllocator_t* alloc, ApeExpression_t* dest, ApeExpression_t* source, bool is_postfix)
{
    ApeExpression_t* res = expression_make(alloc, EXPRESSION_ASSIGN);
    if(!res)
    {
        return NULL;
    }
    res->assign.dest = dest;
    res->assign.source = source;
    res->assign.is_postfix = is_postfix;
    return res;
}

ApeExpression_t* expression_make_logical(ApeAllocator_t* alloc, ApeOperator_t op, ApeExpression_t* left, ApeExpression_t* right)
{
    ApeExpression_t* res = expression_make(alloc, EXPRESSION_LOGICAL);
    if(!res)
    {
        return NULL;
    }
    res->logical.op = op;
    res->logical.left = left;
    res->logical.right = right;
    return res;
}

ApeExpression_t* expression_make_ternary(ApeAllocator_t* alloc, ApeExpression_t* test, ApeExpression_t* if_true, ApeExpression_t* if_false)
{
    ApeExpression_t* res = expression_make(alloc, EXPRESSION_TERNARY);
    if(!res)
    {
        return NULL;
    }
    res->ternary.test = test;
    res->ternary.if_true = if_true;
    res->ternary.if_false = if_false;
    return res;
}

void expression_destroy(ApeExpression_t* expr)
{
    if(!expr)
    {
        return;
    }

    switch(expr->type)
    {
        case EXPRESSION_NONE:
        {
            APE_ASSERT(false);
            break;
        }
        case EXPRESSION_IDENT:
        {
            ident_destroy(expr->ident);
            break;
        }
        case EXPRESSION_NUMBER_LITERAL:
        case EXPRESSION_BOOL_LITERAL:
        {
            break;
        }
        case EXPRESSION_STRING_LITERAL:
        {
            allocator_free(expr->alloc, expr->string_literal);
            break;
        }
        case EXPRESSION_NULL_LITERAL:
        {
            break;
        }
        case EXPRESSION_ARRAY_LITERAL:
        {
            ptrarray_destroy_with_items(expr->array, expression_destroy);
            break;
        }
        case EXPRESSION_MAP_LITERAL:
        {
            ptrarray_destroy_with_items(expr->map.keys, expression_destroy);
            ptrarray_destroy_with_items(expr->map.values, expression_destroy);
            break;
        }
        case EXPRESSION_PREFIX:
        {
            expression_destroy(expr->prefix.right);
            break;
        }
        case EXPRESSION_INFIX:
        {
            expression_destroy(expr->infix.left);
            expression_destroy(expr->infix.right);
            break;
        }
        case EXPRESSION_FUNCTION_LITERAL:
        {
            ApeFnLiteral_t* fn = &expr->fn_literal;
            allocator_free(expr->alloc, fn->name);
            ptrarray_destroy_with_items(fn->params, ident_destroy);
            code_block_destroy(fn->body);
            break;
        }
        case EXPRESSION_CALL:
        {
            ptrarray_destroy_with_items(expr->call_expr.args, expression_destroy);
            expression_destroy(expr->call_expr.function);
            break;
        }
        case EXPRESSION_INDEX:
        {
            expression_destroy(expr->index_expr.left);
            expression_destroy(expr->index_expr.index);
            break;
        }
        case EXPRESSION_ASSIGN:
        {
            expression_destroy(expr->assign.dest);
            expression_destroy(expr->assign.source);
            break;
        }
        case EXPRESSION_LOGICAL:
        {
            expression_destroy(expr->logical.left);
            expression_destroy(expr->logical.right);
            break;
        }
        case EXPRESSION_TERNARY:
        {
            expression_destroy(expr->ternary.test);
            expression_destroy(expr->ternary.if_true);
            expression_destroy(expr->ternary.if_false);
            break;
        }
    }
    allocator_free(expr->alloc, expr);
}

ApeExpression_t* expression_copy(ApeExpression_t* expr)
{
    if(!expr)
    {
        return NULL;
    }
    ApeExpression_t* res = NULL;
    switch(expr->type)
    {
        case EXPRESSION_NONE:
        {
            APE_ASSERT(false);
            break;
        }
        case EXPRESSION_IDENT:
        {
            ApeIdent_t* ident = ident_copy(expr->ident);
            if(!ident)
            {
                return NULL;
            }
            res = expression_make_ident(expr->alloc, ident);
            if(!res)
            {
                ident_destroy(ident);
                return NULL;
            }
            break;
        }
        case EXPRESSION_NUMBER_LITERAL:
        {
            res = expression_make_number_literal(expr->alloc, expr->number_literal);
            break;
        }
        case EXPRESSION_BOOL_LITERAL:
        {
            res = expression_make_bool_literal(expr->alloc, expr->bool_literal);
            break;
        }
        case EXPRESSION_STRING_LITERAL:
        {
            char* string_copy = ape_strdup(expr->alloc, expr->string_literal);
            if(!string_copy)
            {
                return NULL;
            }
            res = expression_make_string_literal(expr->alloc, string_copy);
            if(!res)
            {
                allocator_free(expr->alloc, string_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_NULL_LITERAL:
        {
            res = expression_make_null_literal(expr->alloc);
            break;
        }
        case EXPRESSION_ARRAY_LITERAL:
        {
            ApePtrArray_t* values_copy = ptrarray_copy_with_items(expr->array, expression_copy, expression_destroy);
            if(!values_copy)
            {
                return NULL;
            }
            res = expression_make_array_literal(expr->alloc, values_copy);
            if(!res)
            {
                ptrarray_destroy_with_items(values_copy, expression_destroy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_MAP_LITERAL:
        {
            ApePtrArray_t* keys_copy = ptrarray_copy_with_items(expr->map.keys, expression_copy, expression_destroy);
            ApePtrArray_t* values_copy = ptrarray_copy_with_items(expr->map.values, expression_copy, expression_destroy);
            if(!keys_copy || !values_copy)
            {
                ptrarray_destroy_with_items(keys_copy, expression_destroy);
                ptrarray_destroy_with_items(values_copy, expression_destroy);
                return NULL;
            }
            res = expression_make_map_literal(expr->alloc, keys_copy, values_copy);
            if(!res)
            {
                ptrarray_destroy_with_items(keys_copy, expression_destroy);
                ptrarray_destroy_with_items(values_copy, expression_destroy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_PREFIX:
        {
            ApeExpression_t* right_copy = expression_copy(expr->prefix.right);
            if(!right_copy)
            {
                return NULL;
            }
            res = expression_make_prefix(expr->alloc, expr->prefix.op, right_copy);
            if(!res)
            {
                expression_destroy(right_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_INFIX:
        {
            ApeExpression_t* left_copy = expression_copy(expr->infix.left);
            ApeExpression_t* right_copy = expression_copy(expr->infix.right);
            if(!left_copy || !right_copy)
            {
                expression_destroy(left_copy);
                expression_destroy(right_copy);
                return NULL;
            }
            res = expression_make_infix(expr->alloc, expr->infix.op, left_copy, right_copy);
            if(!res)
            {
                expression_destroy(left_copy);
                expression_destroy(right_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_FUNCTION_LITERAL:
        {
            ApePtrArray_t* params_copy = ptrarray_copy_with_items(expr->fn_literal.params, ident_copy, ident_destroy);
            ApeCodeblock_t* body_copy = code_block_copy(expr->fn_literal.body);
            char* name_copy = ape_strdup(expr->alloc, expr->fn_literal.name);
            if(!params_copy || !body_copy)
            {
                ptrarray_destroy_with_items(params_copy, ident_destroy);
                code_block_destroy(body_copy);
                allocator_free(expr->alloc, name_copy);
                return NULL;
            }
            res = expression_make_fn_literal(expr->alloc, params_copy, body_copy);
            if(!res)
            {
                ptrarray_destroy_with_items(params_copy, ident_destroy);
                code_block_destroy(body_copy);
                allocator_free(expr->alloc, name_copy);
                return NULL;
            }
            res->fn_literal.name = name_copy;
            break;
        }
        case EXPRESSION_CALL:
        {
            ApeExpression_t* function_copy = expression_copy(expr->call_expr.function);
            ApePtrArray_t* args_copy = ptrarray_copy_with_items(expr->call_expr.args, expression_copy, expression_destroy);
            if(!function_copy || !args_copy)
            {
                expression_destroy(function_copy);
                ptrarray_destroy_with_items(expr->call_expr.args, expression_destroy);
                return NULL;
            }
            res = expression_make_call(expr->alloc, function_copy, args_copy);
            if(!res)
            {
                expression_destroy(function_copy);
                ptrarray_destroy_with_items(expr->call_expr.args, expression_destroy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_INDEX:
        {
            ApeExpression_t* left_copy = expression_copy(expr->index_expr.left);
            ApeExpression_t* index_copy = expression_copy(expr->index_expr.index);
            if(!left_copy || !index_copy)
            {
                expression_destroy(left_copy);
                expression_destroy(index_copy);
                return NULL;
            }
            res = expression_make_index(expr->alloc, left_copy, index_copy);
            if(!res)
            {
                expression_destroy(left_copy);
                expression_destroy(index_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_ASSIGN:
        {
            ApeExpression_t* dest_copy = expression_copy(expr->assign.dest);
            ApeExpression_t* source_copy = expression_copy(expr->assign.source);
            if(!dest_copy || !source_copy)
            {
                expression_destroy(dest_copy);
                expression_destroy(source_copy);
                return NULL;
            }
            res = expression_make_assign(expr->alloc, dest_copy, source_copy, expr->assign.is_postfix);
            if(!res)
            {
                expression_destroy(dest_copy);
                expression_destroy(source_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_LOGICAL:
        {
            ApeExpression_t* left_copy = expression_copy(expr->logical.left);
            ApeExpression_t* right_copy = expression_copy(expr->logical.right);
            if(!left_copy || !right_copy)
            {
                expression_destroy(left_copy);
                expression_destroy(right_copy);
                return NULL;
            }
            res = expression_make_logical(expr->alloc, expr->logical.op, left_copy, right_copy);
            if(!res)
            {
                expression_destroy(left_copy);
                expression_destroy(right_copy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_TERNARY:
        {
            ApeExpression_t* test_copy = expression_copy(expr->ternary.test);
            ApeExpression_t* if_true_copy = expression_copy(expr->ternary.if_true);
            ApeExpression_t* if_false_copy = expression_copy(expr->ternary.if_false);
            if(!test_copy || !if_true_copy || !if_false_copy)
            {
                expression_destroy(test_copy);
                expression_destroy(if_true_copy);
                expression_destroy(if_false_copy);
                return NULL;
            }
            res = expression_make_ternary(expr->alloc, test_copy, if_true_copy, if_false_copy);
            if(!res)
            {
                expression_destroy(test_copy);
                expression_destroy(if_true_copy);
                expression_destroy(if_false_copy);
                return NULL;
            }
            break;
        }
    }
    if(!res)
    {
        return NULL;
    }
    res->pos = expr->pos;
    return res;
}

ApeStatement_t* statement_make_define(ApeAllocator_t* alloc, ApeIdent_t* name, ApeExpression_t* value, bool assignable)
{
    ApeStatement_t* res = statement_make(alloc, STATEMENT_DEFINE);
    if(!res)
    {
        return NULL;
    }
    res->define.name = name;
    res->define.value = value;
    res->define.assignable = assignable;
    return res;
}

ApeStatement_t* statement_make_if(ApeAllocator_t* alloc, ApePtrArray_t * cases, ApeCodeblock_t* alternative)
{
    ApeStatement_t* res = statement_make(alloc, STATEMENT_IF);
    if(!res)
    {
        return NULL;
    }
    res->if_statement.cases = cases;
    res->if_statement.alternative = alternative;
    return res;
}

ApeStatement_t* statement_make_return(ApeAllocator_t* alloc, ApeExpression_t* value)
{
    ApeStatement_t* res = statement_make(alloc, STATEMENT_RETURN_VALUE);
    if(!res)
    {
        return NULL;
    }
    res->return_value = value;
    return res;
}

ApeStatement_t* statement_make_expression(ApeAllocator_t* alloc, ApeExpression_t* value)
{
    ApeStatement_t* res = statement_make(alloc, STATEMENT_EXPRESSION);
    if(!res)
    {
        return NULL;
    }
    res->expression = value;
    return res;
}

ApeStatement_t* statement_make_while_loop(ApeAllocator_t* alloc, ApeExpression_t* test, ApeCodeblock_t* body)
{
    ApeStatement_t* res = statement_make(alloc, STATEMENT_WHILE_LOOP);
    if(!res)
    {
        return NULL;
    }
    res->while_loop.test = test;
    res->while_loop.body = body;
    return res;
}

ApeStatement_t* statement_make_break(ApeAllocator_t* alloc)
{
    ApeStatement_t* res = statement_make(alloc, STATEMENT_BREAK);
    if(!res)
    {
        return NULL;
    }
    return res;
}

ApeStatement_t* statement_make_foreach(ApeAllocator_t* alloc, ApeIdent_t* iterator, ApeExpression_t* source, ApeCodeblock_t* body)
{
    ApeStatement_t* res = statement_make(alloc, STATEMENT_FOREACH);
    if(!res)
    {
        return NULL;
    }
    res->foreach.iterator = iterator;
    res->foreach.source = source;
    res->foreach.body = body;
    return res;
}

ApeStatement_t* statement_make_for_loop(ApeAllocator_t* alloc, ApeStatement_t* init, ApeExpression_t* test, ApeExpression_t* update, ApeCodeblock_t* body)
{
    ApeStatement_t* res = statement_make(alloc, STATEMENT_FOR_LOOP);
    if(!res)
    {
        return NULL;
    }
    res->for_loop.init = init;
    res->for_loop.test = test;
    res->for_loop.update = update;
    res->for_loop.body = body;
    return res;
}

ApeStatement_t* statement_make_continue(ApeAllocator_t* alloc)
{
    ApeStatement_t* res = statement_make(alloc, STATEMENT_CONTINUE);
    if(!res)
    {
        return NULL;
    }
    return res;
}

ApeStatement_t* statement_make_block(ApeAllocator_t* alloc, ApeCodeblock_t* block)
{
    ApeStatement_t* res = statement_make(alloc, STATEMENT_BLOCK);
    if(!res)
    {
        return NULL;
    }
    res->block = block;
    return res;
}

ApeStatement_t* statement_make_import(ApeAllocator_t* alloc, char* path)
{
    ApeStatement_t* res = statement_make(alloc, STATEMENT_IMPORT);
    if(!res)
    {
        return NULL;
    }
    res->import.path = path;
    return res;
}

ApeStatement_t* statement_make_recover(ApeAllocator_t* alloc, ApeIdent_t* error_ident, ApeCodeblock_t* body)
{
    ApeStatement_t* res = statement_make(alloc, STATEMENT_RECOVER);
    if(!res)
    {
        return NULL;
    }
    res->recover.error_ident = error_ident;
    res->recover.body = body;
    return res;
}

void statement_destroy(ApeStatement_t* stmt)
{
    if(!stmt)
    {
        return;
    }
    switch(stmt->type)
    {
        case STATEMENT_NONE:
        {
            APE_ASSERT(false);
            break;
        }
        case STATEMENT_DEFINE:
        {
            ident_destroy(stmt->define.name);
            expression_destroy(stmt->define.value);
            break;
        }
        case STATEMENT_IF:
        {
            ptrarray_destroy_with_items(stmt->if_statement.cases, if_case_destroy);
            code_block_destroy(stmt->if_statement.alternative);
            break;
        }
        case STATEMENT_RETURN_VALUE:
        {
            expression_destroy(stmt->return_value);
            break;
        }
        case STATEMENT_EXPRESSION:
        {
            expression_destroy(stmt->expression);
            break;
        }
        case STATEMENT_WHILE_LOOP:
        {
            expression_destroy(stmt->while_loop.test);
            code_block_destroy(stmt->while_loop.body);
            break;
        }
        case STATEMENT_BREAK:
        {
            break;
        }
        case STATEMENT_CONTINUE:
        {
            break;
        }
        case STATEMENT_FOREACH:
        {
            ident_destroy(stmt->foreach.iterator);
            expression_destroy(stmt->foreach.source);
            code_block_destroy(stmt->foreach.body);
            break;
        }
        case STATEMENT_FOR_LOOP:
        {
            statement_destroy(stmt->for_loop.init);
            expression_destroy(stmt->for_loop.test);
            expression_destroy(stmt->for_loop.update);
            code_block_destroy(stmt->for_loop.body);
            break;
        }
        case STATEMENT_BLOCK:
        {
            code_block_destroy(stmt->block);
            break;
        }
        case STATEMENT_IMPORT:
        {
            allocator_free(stmt->alloc, stmt->import.path);
            break;
        }
        case STATEMENT_RECOVER:
        {
            code_block_destroy(stmt->recover.body);
            ident_destroy(stmt->recover.error_ident);
            break;
        }
    }
    allocator_free(stmt->alloc, stmt);
}

ApeStatement_t* statement_copy(const ApeStatement_t* stmt)
{
    if(!stmt)
    {
        return NULL;
    }
    ApeStatement_t* res = NULL;
    switch(stmt->type)
    {
        case STATEMENT_NONE:
        {
            APE_ASSERT(false);
            break;
        }
        case STATEMENT_DEFINE:
        {
            ApeExpression_t* value_copy = expression_copy(stmt->define.value);
            if(!value_copy)
            {
                return NULL;
            }
            res = statement_make_define(stmt->alloc, ident_copy(stmt->define.name), value_copy, stmt->define.assignable);
            if(!res)
            {
                expression_destroy(value_copy);
                return NULL;
            }
            break;
        }
        case STATEMENT_IF:
        {
            ApePtrArray_t* cases_copy = ptrarray_copy_with_items(stmt->if_statement.cases, if_case_copy, if_case_destroy);
            ApeCodeblock_t* alternative_copy = code_block_copy(stmt->if_statement.alternative);
            if(!cases_copy || !alternative_copy)
            {
                ptrarray_destroy_with_items(cases_copy, if_case_destroy);
                code_block_destroy(alternative_copy);
                return NULL;
            }
            res = statement_make_if(stmt->alloc, cases_copy, alternative_copy);
            if(res)
            {
                ptrarray_destroy_with_items(cases_copy, if_case_destroy);
                code_block_destroy(alternative_copy);
                return NULL;
            }
            break;
        }
        case STATEMENT_RETURN_VALUE:
        {
            ApeExpression_t* value_copy = expression_copy(stmt->return_value);
            if(!value_copy)
            {
                return NULL;
            }
            res = statement_make_return(stmt->alloc, value_copy);
            if(!res)
            {
                expression_destroy(value_copy);
                return NULL;
            }
            break;
        }
        case STATEMENT_EXPRESSION:
        {
            ApeExpression_t* value_copy = expression_copy(stmt->expression);
            if(!value_copy)
            {
                return NULL;
            }
            res = statement_make_expression(stmt->alloc, value_copy);
            if(!res)
            {
                expression_destroy(value_copy);
                return NULL;
            }
            break;
        }
        case STATEMENT_WHILE_LOOP:
        {
            ApeExpression_t* test_copy = expression_copy(stmt->while_loop.test);
            ApeCodeblock_t* body_copy = code_block_copy(stmt->while_loop.body);
            if(!test_copy || !body_copy)
            {
                expression_destroy(test_copy);
                code_block_destroy(body_copy);
                return NULL;
            }
            res = statement_make_while_loop(stmt->alloc, test_copy, body_copy);
            if(!res)
            {
                expression_destroy(test_copy);
                code_block_destroy(body_copy);
                return NULL;
            }
            break;
        }
        case STATEMENT_BREAK:
        {
            res = statement_make_break(stmt->alloc);
            break;
        }
        case STATEMENT_CONTINUE:
        {
            res = statement_make_continue(stmt->alloc);
            break;
        }
        case STATEMENT_FOREACH:
        {
            ApeExpression_t* source_copy = expression_copy(stmt->foreach.source);
            ApeCodeblock_t* body_copy = code_block_copy(stmt->foreach.body);
            if(!source_copy || !body_copy)
            {
                expression_destroy(source_copy);
                code_block_destroy(body_copy);
                return NULL;
            }
            res = statement_make_foreach(stmt->alloc, ident_copy(stmt->foreach.iterator), source_copy, body_copy);
            if(!res)
            {
                expression_destroy(source_copy);
                code_block_destroy(body_copy);
                return NULL;
            }
            break;
        }
        case STATEMENT_FOR_LOOP:
        {
            ApeStatement_t* init_copy = statement_copy(stmt->for_loop.init);
            ApeExpression_t* test_copy = expression_copy(stmt->for_loop.test);
            ApeExpression_t* update_copy = expression_copy(stmt->for_loop.update);
            ApeCodeblock_t* body_copy = code_block_copy(stmt->for_loop.body);
            if(!init_copy || !test_copy || !update_copy || !body_copy)
            {
                statement_destroy(init_copy);
                expression_destroy(test_copy);
                expression_destroy(update_copy);
                code_block_destroy(body_copy);
                return NULL;
            }
            res = statement_make_for_loop(stmt->alloc, init_copy, test_copy, update_copy, body_copy);
            if(!res)
            {
                statement_destroy(init_copy);
                expression_destroy(test_copy);
                expression_destroy(update_copy);
                code_block_destroy(body_copy);
                return NULL;
            }
            break;
        }
        case STATEMENT_BLOCK:
        {
            ApeCodeblock_t* block_copy = code_block_copy(stmt->block);
            if(!block_copy)
            {
                return NULL;
            }
            res = statement_make_block(stmt->alloc, block_copy);
            if(!res)
            {
                code_block_destroy(block_copy);
                return NULL;
            }
            break;
        }
        case STATEMENT_IMPORT:
        {
            char* path_copy = ape_strdup(stmt->alloc, stmt->import.path);
            if(!path_copy)
            {
                return NULL;
            }
            res = statement_make_import(stmt->alloc, path_copy);
            if(!res)
            {
                allocator_free(stmt->alloc, path_copy);
                return NULL;
            }
            break;
        }
        case STATEMENT_RECOVER:
        {
            ApeCodeblock_t* body_copy = code_block_copy(stmt->recover.body);
            ApeIdent_t* error_ident_copy = ident_copy(stmt->recover.error_ident);
            if(!body_copy || !error_ident_copy)
            {
                code_block_destroy(body_copy);
                ident_destroy(error_ident_copy);
                return NULL;
            }
            res = statement_make_recover(stmt->alloc, error_ident_copy, body_copy);
            if(!res)
            {
                code_block_destroy(body_copy);
                ident_destroy(error_ident_copy);
                return NULL;
            }
            break;
        }
    }
    if(!res)
    {
        return NULL;
    }
    res->pos = stmt->pos;
    return res;
}

ApeCodeblock_t* code_block_make(ApeAllocator_t* alloc, ApePtrArray_t * statements)
{
    ApeCodeblock_t* block = (ApeCodeblock_t*)allocator_malloc(alloc, sizeof(ApeCodeblock_t));
    if(!block)
    {
        return NULL;
    }
    block->alloc = alloc;
    block->statements = statements;
    return block;
}

void code_block_destroy(ApeCodeblock_t* block)
{
    if(!block)
    {
        return;
    }
    ptrarray_destroy_with_items(block->statements, statement_destroy);
    allocator_free(block->alloc, block);
}

ApeCodeblock_t* code_block_copy(ApeCodeblock_t* block)
{
    if(!block)
    {
        return NULL;
    }
    ApePtrArray_t* statements_copy = ptrarray_copy_with_items(block->statements, statement_copy, statement_destroy);
    if(!statements_copy)
    {
        return NULL;
    }
    ApeCodeblock_t* res = code_block_make(block->alloc, statements_copy);
    if(!res)
    {
        ptrarray_destroy_with_items(statements_copy, statement_destroy);
        return NULL;
    }
    return res;
}


ApeIdent_t* ident_make(ApeAllocator_t* alloc, ApeToken_t tok)
{
    ApeIdent_t* res = (ApeIdent_t*)allocator_malloc(alloc, sizeof(ApeIdent_t));
    if(!res)
    {
        return NULL;
    }
    res->alloc = alloc;
    res->value = token_duplicate_literal(alloc, &tok);
    if(!res->value)
    {
        allocator_free(alloc, res);
        return NULL;
    }
    res->pos = tok.pos;
    return res;
}

ApeIdent_t* ident_copy(ApeIdent_t* ident)
{
    ApeIdent_t* res = (ApeIdent_t*)allocator_malloc(ident->alloc, sizeof(ApeIdent_t));
    if(!res)
    {
        return NULL;
    }
    res->alloc = ident->alloc;
    res->value = ape_strdup(ident->alloc, ident->value);
    if(!res->value)
    {
        allocator_free(ident->alloc, res);
        return NULL;
    }
    res->pos = ident->pos;
    return res;
}

void ident_destroy(ApeIdent_t* ident)
{
    if(!ident)
    {
        return;
    }
    allocator_free(ident->alloc, ident->value);
    ident->value = NULL;
    ident->pos = src_pos_invalid;
    allocator_free(ident->alloc, ident);
}

ApeIfCase_t* if_case_make(ApeAllocator_t* alloc, ApeExpression_t* test, ApeCodeblock_t* consequence)
{
    ApeIfCase_t* res = (ApeIfCase_t*)allocator_malloc(alloc, sizeof(ApeIfCase_t));
    if(!res)
    {
        return NULL;
    }
    res->alloc = alloc;
    res->test = test;
    res->consequence = consequence;
    return res;
}

void if_case_destroy(ApeIfCase_t* cond)
{
    if(!cond)
    {
        return;
    }
    expression_destroy(cond->test);
    code_block_destroy(cond->consequence);
    allocator_free(cond->alloc, cond);
}

ApeIfCase_t* if_case_copy(ApeIfCase_t* if_case)
{
    if(!if_case)
    {
        return NULL;
    }
    ApeExpression_t* test_copy = NULL;
    ApeCodeblock_t* consequence_copy = NULL;
    ApeIfCase_t* if_case_copy = NULL;

    test_copy = expression_copy(if_case->test);
    if(!test_copy)
    {
        goto err;
    }
    consequence_copy = code_block_copy(if_case->consequence);
    if(!test_copy || !consequence_copy)
    {
        goto err;
    }
    if_case_copy = if_case_make(if_case->alloc, test_copy, consequence_copy);
    if(!if_case_copy)
    {
        goto err;
    }
    return if_case_copy;
err:
    expression_destroy(test_copy);
    code_block_destroy(consequence_copy);
    if_case_destroy(if_case_copy);
    return NULL;
}

// INTERNAL
ApeExpression_t* expression_make(ApeAllocator_t* alloc, ApeExpr_type_t type)
{
    ApeExpression_t* res = (ApeExpression_t*)allocator_malloc(alloc, sizeof(ApeExpression_t));
    if(!res)
    {
        return NULL;
    }
    res->alloc = alloc;
    res->type = type;
    res->pos = src_pos_invalid;
    return res;
}

ApeStatement_t* statement_make(ApeAllocator_t* alloc, ApeStatementType_t type)
{
    ApeStatement_t* res = (ApeStatement_t*)allocator_malloc(alloc, sizeof(ApeStatement_t));
    if(!res)
    {
        return NULL;
    }
    res->alloc = alloc;
    res->type = type;
    res->pos = src_pos_invalid;
    return res;
}




ApeGlobalStore_t* global_store_make(ApeAllocator_t* alloc, ApeGCMemory_t* mem)
{
    ApeGlobalStore_t* store = (ApeGlobalStore_t*)allocator_malloc(alloc, sizeof(ApeGlobalStore_t));
    if(!store)
    {
        return NULL;
    }
    memset(store, 0, sizeof(ApeGlobalStore_t));
    store->alloc = alloc;
    store->symbols = dict_make(alloc, symbol_copy, symbol_destroy);
    if(!store->symbols)
    {
        goto err;
    }
    store->objects = array_make(alloc, ApeObject_t);
    if(!store->objects)
    {
        goto err;
    }

    if(mem)
    {
        for(int i = 0; i < builtins_count(); i++)
        {
            const char* name = builtins_get_name(i);
            ApeObject_t builtin = object_make_native_function_memory(mem, name, builtins_get_fn(i), NULL, 0);
            if(object_is_null(builtin))
            {
                goto err;
            }
            bool ok = global_store_set(store, name, builtin);
            if(!ok)
            {
                goto err;
            }
        }
    }

    return store;
err:
    global_store_destroy(store);
    return NULL;
}

void global_store_destroy(ApeGlobalStore_t* store)
{
    if(!store)
    {
        return;
    }
    dict_destroy_with_items(store->symbols);
    array_destroy(store->objects);
    allocator_free(store->alloc, store);
}

const ApeSymbol_t* global_store_get_symbol(ApeGlobalStore_t* store, const char* name)
{
    return (ApeSymbol_t*)dict_get(store->symbols, name);
}

ApeObject_t global_store_get_object(ApeGlobalStore_t* store, const char* name)
{
    const ApeSymbol_t* symbol = global_store_get_symbol(store, name);
    if(!symbol)
    {
        return object_make_null();
    }
    APE_ASSERT(symbol->type == SYMBOL_APE_GLOBAL);
    ApeObject_t* res = (ApeObject_t*)array_get(store->objects, symbol->index);
    return *res;
}

bool global_store_set(ApeGlobalStore_t* store, const char* name, ApeObject_t object)
{
    int ix;
    bool ok;
    ApeSymbol_t* symbol;
    const ApeSymbol_t* existing_symbol;
    existing_symbol = global_store_get_symbol(store, name);
    if(existing_symbol)
    {
        ok = array_set(store->objects, existing_symbol->index, &object);
        return ok;
    }
    ix = array_count(store->objects);
    ok = array_add(store->objects, &object);
    if(!ok)
    {
        return false;
    }
    symbol = symbol_make(store->alloc, name, SYMBOL_APE_GLOBAL, ix, false);
    if(!symbol)
    {
        goto err;
    }
    ok = dict_set(store->symbols, name, symbol);
    if(!ok)
    {
        symbol_destroy(symbol);
        goto err;
    }
    return true;
err:
    array_pop(store->objects, NULL);
    return false;
}

ApeObject_t global_store_get_object_at(ApeGlobalStore_t* store, int ix, bool* out_ok)
{
    ApeObject_t* res = (ApeObject_t*)array_get(store->objects, ix);
    if(!res)
    {
        *out_ok = false;
        return object_make_null();
    }
    *out_ok = true;
    return *res;
}

bool global_store_set_object_at(ApeGlobalStore_t* store, int ix, ApeObject_t object)
{
    bool ok = array_set(store->objects, ix, &object);
    return ok;
}

ApeObject_t* global_store_get_object_data(ApeGlobalStore_t* store)
{
    return (ApeObject_t*)array_data(store->objects);
}

int global_store_get_object_count(ApeGlobalStore_t* store)
{
    return array_count(store->objects);
}


ApeSymbol_t* symbol_make(ApeAllocator_t* alloc, const char* name, ApeSymbolType_t type, int index, bool assignable)
{
    ApeSymbol_t* symbol;
    symbol = (ApeSymbol_t*)allocator_malloc(alloc, sizeof(ApeSymbol_t));
    if(!symbol)
    {
        return NULL;
    }
    memset(symbol, 0, sizeof(ApeSymbol_t));
    symbol->alloc = alloc;
    symbol->name = ape_strdup(alloc, name);
    if(!symbol->name)
    {
        allocator_free(alloc, symbol);
        return NULL;
    }
    symbol->type = type;
    symbol->index = index;
    symbol->assignable = assignable;
    return symbol;
}

void symbol_destroy(ApeSymbol_t* symbol)
{
    if(!symbol)
    {
        return;
    }
    allocator_free(symbol->alloc, symbol->name);
    allocator_free(symbol->alloc, symbol);
}

ApeSymbol_t* symbol_copy(ApeSymbol_t* symbol)
{
    return symbol_make(symbol->alloc, symbol->name, symbol->type, symbol->index, symbol->assignable);
}

ApeSymbolTable_t* symbol_table_make(ApeAllocator_t* alloc, ApeSymbolTable_t* outer, ApeGlobalStore_t* global_store, int module_global_offset)
{
    bool ok;
    ApeSymbolTable_t* table;
    table = (ApeSymbolTable_t*)allocator_malloc(alloc, sizeof(ApeSymbolTable_t));
    if(!table)
    {
        return NULL;
    }
    memset(table, 0, sizeof(ApeSymbolTable_t));
    table->alloc = alloc;
    table->max_num_definitions = 0;
    table->outer = outer;
    table->global_store = global_store;
    table->module_global_offset = module_global_offset;
    table->block_scopes = ptrarray_make(alloc);
    if(!table->block_scopes)
    {
        goto err;
    }
    table->free_symbols = ptrarray_make(alloc);
    if(!table->free_symbols)
    {
        goto err;
    }
    table->module_global_symbols = ptrarray_make(alloc);
    if(!table->module_global_symbols)
    {
        goto err;
    }
    ok = symbol_table_push_block_scope(table);
    if(!ok)
    {
        goto err;
    }
    return table;
err:
    symbol_table_destroy(table);
    return NULL;
}

void symbol_table_destroy(ApeSymbolTable_t* table)
{
    ApeAllocator_t* alloc;
    if(!table)
    {
        return;
    }

    while(ptrarray_count(table->block_scopes) > 0)
    {
        symbol_table_pop_block_scope(table);
    }
    ptrarray_destroy(table->block_scopes);
    ptrarray_destroy_with_items(table->module_global_symbols, symbol_destroy);
    ptrarray_destroy_with_items(table->free_symbols, symbol_destroy);
    alloc = table->alloc;
    memset(table, 0, sizeof(ApeSymbolTable_t));
    allocator_free(alloc, table);
}

ApeSymbolTable_t* symbol_table_copy(ApeSymbolTable_t* table)
{
    ApeSymbolTable_t* copy;
    copy = (ApeSymbolTable_t*)allocator_malloc(table->alloc, sizeof(ApeSymbolTable_t));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(ApeSymbolTable_t));
    copy->alloc = table->alloc;
    copy->outer = table->outer;
    copy->global_store = table->global_store;
    copy->block_scopes = ptrarray_copy_with_items(table->block_scopes, block_scope_copy, block_scope_destroy);
    if(!copy->block_scopes)
    {
        goto err;
    }
    copy->free_symbols = ptrarray_copy_with_items(table->free_symbols, symbol_copy, symbol_destroy);
    if(!copy->free_symbols)
    {
        goto err;
    }
    copy->module_global_symbols = ptrarray_copy_with_items(table->module_global_symbols, symbol_copy, symbol_destroy);
    if(!copy->module_global_symbols)
    {
        goto err;
    }
    copy->max_num_definitions = table->max_num_definitions;
    copy->module_global_offset = table->module_global_offset;
    return copy;
err:
    symbol_table_destroy(copy);
    return NULL;
}

bool symbol_table_add_module_symbol(ApeSymbolTable_t* st, ApeSymbol_t* symbol)
{
    bool ok;
    if(symbol->type != SYMBOL_MODULE_GLOBAL)
    {
        APE_ASSERT(false);
        return false;
    }
    if(symbol_table_symbol_is_defined(st, symbol->name))
    {
        return true;// todo: make sure it should be true in this case
    }
    ApeSymbol_t* copy = symbol_copy(symbol);
    if(!copy)
    {
        return false;
    }
    ok = set_symbol(st, copy);
    if(!ok)
    {
        symbol_destroy(copy);
        return false;
    }
    return true;
}

const ApeSymbol_t* symbol_table_define(ApeSymbolTable_t* table, const char* name, bool assignable)
{
    
    bool ok;
    bool global_symbol_added;
    int ix;
    int definitions_count;
    ApeBlockScope_t* top_scope;
    ApeSymbolType_t symbol_type;
    ApeSymbol_t* symbol;
    ApeSymbol_t* global_symbol_copy;
    const ApeSymbol_t* global_symbol;
    global_symbol = global_store_get_symbol(table->global_store, name);
    if(global_symbol)
    {
        return NULL;
    }
    if(strchr(name, ':'))
    {
        return NULL;// module symbol
    }
    if(APE_STREQ(name, "this"))
    {
        return NULL;// "this" is reserved
    }
    symbol_type = table->outer == NULL ? SYMBOL_MODULE_GLOBAL : SYMBOL_LOCAL;
    ix = next_symbol_index(table);
    symbol = symbol_make(table->alloc, name, symbol_type, ix, assignable);
    if(!symbol)
    {
        return NULL;
    }
    global_symbol_added = false;
    ok = false;
    if(symbol_type == SYMBOL_MODULE_GLOBAL && ptrarray_count(table->block_scopes) == 1)
    {
        global_symbol_copy = symbol_copy(symbol);
        if(!global_symbol_copy)
        {
            symbol_destroy(symbol);
            return NULL;
        }
        ok = ptrarray_add(table->module_global_symbols, global_symbol_copy);
        if(!ok)
        {
            symbol_destroy(global_symbol_copy);
            symbol_destroy(symbol);
            return NULL;
        }
        global_symbol_added = true;
    }

    ok = set_symbol(table, symbol);
    if(!ok)
    {
        if(global_symbol_added)
        {
            global_symbol_copy = (ApeSymbol_t*)ptrarray_pop(table->module_global_symbols);
            symbol_destroy(global_symbol_copy);
        }
        symbol_destroy(symbol);
        return NULL;
    }
    top_scope = (ApeBlockScope_t*)ptrarray_top(table->block_scopes);
    top_scope->num_definitions++;
    definitions_count = count_num_definitions(table);
    if(definitions_count > table->max_num_definitions)
    {
        table->max_num_definitions = definitions_count;
    }

    return symbol;
}

const ApeSymbol_t* symbol_table_define_free(ApeSymbolTable_t* st, const ApeSymbol_t* original)
{
    ApeSymbol_t* copy = symbol_make(st->alloc, original->name, original->type, original->index, original->assignable);
    if(!copy)
    {
        return NULL;
    }
    bool ok = ptrarray_add(st->free_symbols, copy);
    if(!ok)
    {
        symbol_destroy(copy);
        return NULL;
    }

    ApeSymbol_t* symbol
    = symbol_make(st->alloc, original->name, SYMBOL_FREE, ptrarray_count(st->free_symbols) - 1, original->assignable);
    if(!symbol)
    {
        return NULL;
    }

    ok = set_symbol(st, symbol);
    if(!ok)
    {
        symbol_destroy(symbol);
        return NULL;
    }

    return symbol;
}

const ApeSymbol_t* symbol_table_define_function_name(ApeSymbolTable_t* st, const char* name, bool assignable)
{
    bool ok;
    ApeSymbol_t* symbol;
    if(strchr(name, ':'))
    {
        return NULL;// module symbol
    }
    symbol = symbol_make(st->alloc, name, SYMBOL_FUNCTION, 0, assignable);
    if(!symbol)
    {
        return NULL;
    }
    ok = set_symbol(st, symbol);
    if(!ok)
    {
        symbol_destroy(symbol);
        return NULL;
    }

    return symbol;
}

const ApeSymbol_t* symbol_table_define_this(ApeSymbolTable_t* st)
{
    bool ok;
    ApeSymbol_t* symbol;
    symbol = symbol_make(st->alloc, "this", SYMBOL_THIS, 0, false);
    if(!symbol)
    {
        return NULL;
    }
    ok = set_symbol(st, symbol);
    if(!ok)
    {
        symbol_destroy(symbol);
        return NULL;
    }
    return symbol;
}

const ApeSymbol_t* symbol_table_resolve(ApeSymbolTable_t* table, const char* name)
{
    int i;
    const ApeSymbol_t* symbol;
    ApeBlockScope_t* scope;
    scope = NULL;
    symbol = global_store_get_symbol(table->global_store, name);
    if(symbol)
    {
        return symbol;
    }
    for(i = ptrarray_count(table->block_scopes) - 1; i >= 0; i--)
    {
        scope = (ApeBlockScope_t*)ptrarray_get(table->block_scopes, i);
        symbol = (ApeSymbol_t*)dict_get(scope->store, name);
        if(symbol)
        {
            break;
        }
    }
    if(symbol && symbol->type == SYMBOL_THIS)
    {
        symbol = symbol_table_define_free(table, symbol);
    }

    if(!symbol && table->outer)
    {
        symbol = symbol_table_resolve(table->outer, name);
        if(!symbol)
        {
            return NULL;
        }
        if(symbol->type == SYMBOL_MODULE_GLOBAL || symbol->type == SYMBOL_APE_GLOBAL)
        {
            return symbol;
        }
        symbol = symbol_table_define_free(table, symbol);
    }
    return symbol;
}

bool symbol_table_symbol_is_defined(ApeSymbolTable_t* table, const char* name)
{
    ApeBlockScope_t* top_scope;
    const ApeSymbol_t* symbol;
    // todo: rename to something more obvious
    symbol = global_store_get_symbol(table->global_store, name);
    if(symbol)
    {
        return true;
    }
    top_scope = (ApeBlockScope_t*)ptrarray_top(table->block_scopes);
    symbol = (ApeSymbol_t*)dict_get(top_scope->store, name);
    if(symbol)
    {
        return true;
    }
    return false;
}

bool symbol_table_push_block_scope(ApeSymbolTable_t* table)
{
    bool ok;
    int block_scope_offset;
    ApeBlockScope_t* prev_block_scope;
    ApeBlockScope_t* new_scope;
    block_scope_offset = 0;
    prev_block_scope = (ApeBlockScope_t*)ptrarray_top(table->block_scopes);
    if(prev_block_scope)
    {
        block_scope_offset = table->module_global_offset + prev_block_scope->offset + prev_block_scope->num_definitions;
    }
    else
    {
        block_scope_offset = table->module_global_offset;
    }

    new_scope = block_scope_make(table->alloc, block_scope_offset);
    if(!new_scope)
    {
        return false;
    }
    ok = ptrarray_push(table->block_scopes, new_scope);
    if(!ok)
    {
        block_scope_destroy(new_scope);
        return false;
    }
    return true;
}

void symbol_table_pop_block_scope(ApeSymbolTable_t* table)
{
    ApeBlockScope_t* top_scope;
    top_scope = (ApeBlockScope_t*)ptrarray_top(table->block_scopes);
    ptrarray_pop(table->block_scopes);
    block_scope_destroy(top_scope);
}

ApeBlockScope_t* symbol_table_get_block_scope(ApeSymbolTable_t* table)
{
    ApeBlockScope_t* top_scope;
    top_scope = (ApeBlockScope_t*)ptrarray_top(table->block_scopes);
    return top_scope;
}

bool symbol_table_is_module_global_scope(ApeSymbolTable_t* table)
{
    return table->outer == NULL;
}

bool symbol_table_is_top_block_scope(ApeSymbolTable_t* table)
{
    return ptrarray_count(table->block_scopes) == 1;
}

bool symbol_table_is_top_global_scope(ApeSymbolTable_t* table)
{
    return symbol_table_is_module_global_scope(table) && symbol_table_is_top_block_scope(table);
}

int symbol_table_get_module_global_symbol_count(const ApeSymbolTable_t* table)
{
    return ptrarray_count(table->module_global_symbols);
}

const ApeSymbol_t* symbol_table_get_module_global_symbol_at(const ApeSymbolTable_t* table, int ix)
{
    return (ApeSymbol_t*)ptrarray_get(table->module_global_symbols, ix);
}

// INTERNAL
ApeBlockScope_t* block_scope_make(ApeAllocator_t* alloc, int offset)
{
    ApeBlockScope_t* new_scope;
    new_scope = (ApeBlockScope_t*)allocator_malloc(alloc, sizeof(ApeBlockScope_t));
    if(!new_scope)
    {
        return NULL;
    }
    memset(new_scope, 0, sizeof(ApeBlockScope_t));
    new_scope->alloc = alloc;
    new_scope->store = dict_make(alloc, symbol_copy, symbol_destroy);
    if(!new_scope->store)
    {
        block_scope_destroy(new_scope);
        return NULL;
    }
    new_scope->num_definitions = 0;
    new_scope->offset = offset;
    return new_scope;
}

void block_scope_destroy(ApeBlockScope_t* scope)
{
    dict_destroy_with_items(scope->store);
    allocator_free(scope->alloc, scope);
}

ApeBlockScope_t* block_scope_copy(ApeBlockScope_t* scope)
{
    ApeBlockScope_t* copy;
    copy = (ApeBlockScope_t*)allocator_malloc(scope->alloc, sizeof(ApeBlockScope_t));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(ApeBlockScope_t));
    copy->alloc = scope->alloc;
    copy->num_definitions = scope->num_definitions;
    copy->offset = scope->offset;
    copy->store = dict_copy_with_items(scope->store);
    if(!copy->store)
    {
        block_scope_destroy(copy);
        return NULL;
    }
    return copy;
}

bool set_symbol(ApeSymbolTable_t* table, ApeSymbol_t* symbol)
{
    ApeBlockScope_t* top_scope;
    ApeSymbol_t* existing;
    top_scope = (ApeBlockScope_t*)ptrarray_top(table->block_scopes);
    existing= (ApeSymbol_t*)dict_get(top_scope->store, symbol->name);
    if(existing)
    {
        symbol_destroy(existing);
    }
    return dict_set(top_scope->store, symbol->name, symbol);
}

int next_symbol_index(ApeSymbolTable_t* table)
{
    int ix;
    ApeBlockScope_t* top_scope;
    top_scope = (ApeBlockScope_t*)ptrarray_top(table->block_scopes);
    ix = top_scope->offset + top_scope->num_definitions;
    return ix;
}

int count_num_definitions(ApeSymbolTable_t* table)
{
    int i;
    int count;
    ApeBlockScope_t* scope;
    count = 0;
    for(i = ptrarray_count(table->block_scopes) - 1; i >= 0; i--)
    {
        scope = (ApeBlockScope_t*)ptrarray_get(table->block_scopes, i);
        count += scope->num_definitions;
    }
    return count;
}


ApeOpcodeDefinition_t* opcode_lookup(opcode_t op)
{
    if(op <= OPCODE_NONE || op >= OPCODE_MAX)
    {
        return NULL;
    }
    return &g_definitions[op];
}

const char* opcode_get_name(opcode_t op)
{
    if(op <= OPCODE_NONE || op >= OPCODE_MAX)
    {
        return NULL;
    }
    return g_definitions[op].name;
}

int code_make(opcode_t op, int operands_count, uint64_t* operands, ApeArray_t* res)
{
    ApeOpcodeDefinition_t* def = opcode_lookup(op);
    if(!def)
    {
        return 0;
    }

    int instr_len = 1;
    for(int i = 0; i < def->num_operands; i++)
    {
        instr_len += def->operand_widths[i];
    }

    uint8_t val = op;
    bool ok = false;

    ok = array_add(res, &val);
    if(!ok)
    {
        return 0;
    }

#define APPEND_BYTE(n)                           \
    do                                           \
    {                                            \
        val = (uint8_t)(operands[i] >> (n * 8)); \
        ok = array_add(res, &val);               \
        if(!ok)                                  \
        {                                        \
            return 0;                            \
        }                                        \
    } while(0)

    for(int i = 0; i < operands_count; i++)
    {
        int width = def->operand_widths[i];
        switch(width)
        {
            case 1:
            {
                APPEND_BYTE(0);
                break;
            }
            case 2:
            {
                APPEND_BYTE(1);
                APPEND_BYTE(0);
                break;
            }
            case 4:
            {
                APPEND_BYTE(3);
                APPEND_BYTE(2);
                APPEND_BYTE(1);
                APPEND_BYTE(0);
                break;
            }
            case 8:
            {
                APPEND_BYTE(7);
                APPEND_BYTE(6);
                APPEND_BYTE(5);
                APPEND_BYTE(4);
                APPEND_BYTE(3);
                APPEND_BYTE(2);
                APPEND_BYTE(1);
                APPEND_BYTE(0);
                break;
            }
            default:
            {
                APE_ASSERT(false);
                break;
            }
        }
    }
#undef APPEND_BYTE
    return instr_len;
}



bool code_read_operands(ApeOpcodeDefinition_t* def, uint8_t* instr, uint64_t out_operands[2])
{
    int offset = 0;
    for(int i = 0; i < def->num_operands; i++)
    {
        int operand_width = def->operand_widths[i];
        switch(operand_width)
        {
            case 1:
            {
                out_operands[i] = instr[offset];
                break;
            }
            case 2:
            {
                uint64_t operand = 0;
                operand = operand | (instr[offset] << 8);
                operand = operand | (instr[offset + 1]);
                out_operands[i] = operand;
                break;
            }
            case 4:
            {
                uint64_t operand = 0;
                operand = operand | (instr[offset + 0] << 24);
                operand = operand | (instr[offset + 1] << 16);
                operand = operand | (instr[offset + 2] << 8);
                operand = operand | (instr[offset + 3]);
                out_operands[i] = operand;
                break;
            }
            case 8:
            {
                uint64_t operand = 0;
                operand = operand | ((uint64_t)instr[offset + 0] << 56);
                operand = operand | ((uint64_t)instr[offset + 1] << 48);
                operand = operand | ((uint64_t)instr[offset + 2] << 40);
                operand = operand | ((uint64_t)instr[offset + 3] << 32);
                operand = operand | ((uint64_t)instr[offset + 4] << 24);
                operand = operand | ((uint64_t)instr[offset + 5] << 16);
                operand = operand | ((uint64_t)instr[offset + 6] << 8);
                operand = operand | ((uint64_t)instr[offset + 7]);
                out_operands[i] = operand;
                break;
            }
            default:
            {
                APE_ASSERT(false);
                return false;
            }
        }
        offset += operand_width;
    }
    return true;
    ;
}


ApeCompilationScope_t* compilation_scope_make(ApeAllocator_t* alloc, ApeCompilationScope_t* outer)
{
    ApeCompilationScope_t* scope = (ApeCompilationScope_t*)allocator_malloc(alloc, sizeof(ApeCompilationScope_t));
    if(!scope)
    {
        return NULL;
    }
    memset(scope, 0, sizeof(ApeCompilationScope_t));
    scope->alloc = alloc;
    scope->outer = outer;
    scope->bytecode = array_make(alloc, uint8_t);
    if(!scope->bytecode)
    {
        goto err;
    }
    scope->src_positions = array_make(alloc, ApePosition_t);
    if(!scope->src_positions)
    {
        goto err;
    }
    scope->break_ip_stack = array_make(alloc, int);
    if(!scope->break_ip_stack)
    {
        goto err;
    }
    scope->continue_ip_stack = array_make(alloc, int);
    if(!scope->continue_ip_stack)
    {
        goto err;
    }
    return scope;
err:
    compilation_scope_destroy(scope);
    return NULL;
}

void compilation_scope_destroy(ApeCompilationScope_t* scope)
{
    array_destroy(scope->continue_ip_stack);
    array_destroy(scope->break_ip_stack);
    array_destroy(scope->bytecode);
    array_destroy(scope->src_positions);
    allocator_free(scope->alloc, scope);
}

ApeCompilationResult_t* compilation_scope_orphan_result(ApeCompilationScope_t* scope)
{
    ApeCompilationResult_t* res;
    res = compilation_result_make(scope->alloc,
        (uint8_t*)array_data(scope->bytecode),
        (ApePosition_t*)array_data(scope->src_positions),
        array_count(scope->bytecode)
    );
    if(!res)
    {
        return NULL;
    }
    array_orphan_data(scope->bytecode);
    array_orphan_data(scope->src_positions);
    return res;
}

ApeCompilationResult_t* compilation_result_make(ApeAllocator_t* alloc, uint8_t* bytecode, ApePosition_t* src_positions, int count)
{
    ApeCompilationResult_t* res;
    res = (ApeCompilationResult_t*)allocator_malloc(alloc, sizeof(ApeCompilationResult_t));
    if(!res)
    {
        return NULL;
    }
    memset(res, 0, sizeof(ApeCompilationResult_t));
    res->alloc = alloc;
    res->bytecode = bytecode;
    res->src_positions = src_positions;
    res->count = count;
    return res;
}

void compilation_result_destroy(ApeCompilationResult_t* res)
{
    if(!res)
    {
        return;
    }
    allocator_free(res->alloc, res->bytecode);
    allocator_free(res->alloc, res->src_positions);
    allocator_free(res->alloc, res);
}


ApeExpression_t* optimise_infix_expression(ApeExpression_t* expr);
ApeExpression_t* optimise_prefix_expression(ApeExpression_t* expr);

ApeExpression_t* optimise_expression(ApeExpression_t* expr)
{
    switch(expr->type)
    {
        case EXPRESSION_INFIX:
            return optimise_infix_expression(expr);
        case EXPRESSION_PREFIX:
            return optimise_prefix_expression(expr);
        default:
            return NULL;
    }
}

// INTERNAL
ApeExpression_t* optimise_infix_expression(ApeExpression_t* expr)
{
    ApeExpression_t* left = expr->infix.left;
    ApeExpression_t* left_optimised = optimise_expression(left);
    if(left_optimised)
    {
        left = left_optimised;
    }

    ApeExpression_t* right = expr->infix.right;
    ApeExpression_t* right_optimised = optimise_expression(right);
    if(right_optimised)
    {
        right = right_optimised;
    }

    ApeExpression_t* res = NULL;

    bool left_is_numeric = left->type == EXPRESSION_NUMBER_LITERAL || left->type == EXPRESSION_BOOL_LITERAL;
    bool right_is_numeric = right->type == EXPRESSION_NUMBER_LITERAL || right->type == EXPRESSION_BOOL_LITERAL;

    bool left_is_string = left->type == EXPRESSION_STRING_LITERAL;
    bool right_is_string = right->type == EXPRESSION_STRING_LITERAL;

    ApeAllocator_t* alloc = expr->alloc;
    if(left_is_numeric && right_is_numeric)
    {
        double left_val = left->type == EXPRESSION_NUMBER_LITERAL ? left->number_literal : left->bool_literal;
        double right_val = right->type == EXPRESSION_NUMBER_LITERAL ? right->number_literal : right->bool_literal;
        int64_t left_val_int = (int64_t)left_val;
        int64_t right_val_int = (int64_t)right_val;
        switch(expr->infix.op)
        {
            case OPERATOR_PLUS:
            {
                res = expression_make_number_literal(alloc, left_val + right_val);
                break;
            }
            case OPERATOR_MINUS:
            {
                res = expression_make_number_literal(alloc, left_val - right_val);
                break;
            }
            case OPERATOR_ASTERISK:
            {
                res = expression_make_number_literal(alloc, left_val * right_val);
                break;
            }
            case OPERATOR_SLASH:
            {
                res = expression_make_number_literal(alloc, left_val / right_val);
                break;
            }
            case OPERATOR_LT:
            {
                res = expression_make_bool_literal(alloc, left_val < right_val);
                break;
            }
            case OPERATOR_LTE:
            {
                res = expression_make_bool_literal(alloc, left_val <= right_val);
                break;
            }
            case OPERATOR_GT:
            {
                res = expression_make_bool_literal(alloc, left_val > right_val);
                break;
            }
            case OPERATOR_GTE:
            {
                res = expression_make_bool_literal(alloc, left_val >= right_val);
                break;
            }
            case OPERATOR_EQ:
            {
                res = expression_make_bool_literal(alloc, APE_DBLEQ(left_val, right_val));
                break;
            }
            case OPERATOR_NOT_EQ:
            {
                res = expression_make_bool_literal(alloc, !APE_DBLEQ(left_val, right_val));
                break;
            }
            case OPERATOR_MODULUS:
            {
                res = expression_make_number_literal(alloc, fmod(left_val, right_val));
                break;
            }
            case OPERATOR_BIT_AND:
            {
                res = expression_make_number_literal(alloc, (double)(left_val_int & right_val_int));
                break;
            }
            case OPERATOR_BIT_OR:
            {
                res = expression_make_number_literal(alloc, (double)(left_val_int | right_val_int));
                break;
            }
            case OPERATOR_BIT_XOR:
            {
                res = expression_make_number_literal(alloc, (double)(left_val_int ^ right_val_int));
                break;
            }
            case OPERATOR_LSHIFT:
            {
                res = expression_make_number_literal(alloc, (double)(left_val_int << right_val_int));
                break;
            }
            case OPERATOR_RSHIFT:
            {
                res = expression_make_number_literal(alloc, (double)(left_val_int >> right_val_int));
                break;
            }
            default:
            {
                break;
            }
        }
    }
    else if(expr->infix.op == OPERATOR_PLUS && left_is_string && right_is_string)
    {
        const char* left_val = left->string_literal;
        const char* right_val = right->string_literal;
        char* res_str = ape_stringf(alloc, "%s%s", left_val, right_val);
        if(res_str)
        {
            res = expression_make_string_literal(alloc, res_str);
            if(!res)
            {
                allocator_free(alloc, res_str);
            }
        }
    }

    expression_destroy(left_optimised);
    expression_destroy(right_optimised);

    if(res)
    {
        res->pos = expr->pos;
    }

    return res;
}

ApeExpression_t* optimise_prefix_expression(ApeExpression_t* expr)
{
    ApeExpression_t* right = expr->prefix.right;
    ApeExpression_t* right_optimised = optimise_expression(right);
    if(right_optimised)
    {
        right = right_optimised;
    }
    ApeExpression_t* res = NULL;
    if(expr->prefix.op == OPERATOR_MINUS && right->type == EXPRESSION_NUMBER_LITERAL)
    {
        res = expression_make_number_literal(expr->alloc, -right->number_literal);
    }
    else if(expr->prefix.op == OPERATOR_BANG && right->type == EXPRESSION_BOOL_LITERAL)
    {
        res = expression_make_bool_literal(expr->alloc, !right->bool_literal);
    }
    expression_destroy(right_optimised);
    if(res)
    {
        res->pos = expr->pos;
    }
    return res;
}


ApeModule_t* module_make(ApeAllocator_t* alloc, const char* name)
{
    ApeModule_t* module = (ApeModule_t*)allocator_malloc(alloc, sizeof(ApeModule_t));
    if(!module)
    {
        return NULL;
    }
    memset(module, 0, sizeof(ApeModule_t));
    module->alloc = alloc;
    module->name = ape_strdup(alloc, name);
    if(!module->name)
    {
        module_destroy(module);
        return NULL;
    }
    module->symbols = ptrarray_make(alloc);
    if(!module->symbols)
    {
        module_destroy(module);
        return NULL;
    }
    return module;
}

void module_destroy(ApeModule_t* module)
{
    if(!module)
    {
        return;
    }
    allocator_free(module->alloc, module->name);
    ptrarray_destroy_with_items(module->symbols, symbol_destroy);
    allocator_free(module->alloc, module);
}

ApeModule_t* module_copy(ApeModule_t* src)
{
    ApeModule_t* copy = (ApeModule_t*)allocator_malloc(src->alloc, sizeof(ApeModule_t));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(ApeModule_t));
    copy->alloc = src->alloc;
    copy->name = ape_strdup(copy->alloc, src->name);
    if(!copy->name)
    {
        module_destroy(copy);
        return NULL;
    }
    copy->symbols = ptrarray_copy_with_items(src->symbols, symbol_copy, symbol_destroy);
    if(!copy->symbols)
    {
        module_destroy(copy);
        return NULL;
    }
    return copy;
}

const char* get_module_name(const char* path)
{
    const char* last_slash_pos = strrchr(path, '/');
    if(last_slash_pos)
    {
        return last_slash_pos + 1;
    }
    return path;
}

bool module_add_symbol(ApeModule_t* module, const ApeSymbol_t* symbol)
{
    ApeStringBuffer_t* name_buf = strbuf_make(module->alloc);
    if(!name_buf)
    {
        return false;
    }
    bool ok = strbuf_appendf(name_buf, "%s::%s", module->name, symbol->name);
    if(!ok)
    {
        strbuf_destroy(name_buf);
        return false;
    }
    ApeSymbol_t* module_symbol = symbol_make(module->alloc, strbuf_get_string(name_buf), SYMBOL_MODULE_GLOBAL, symbol->index, false);
    strbuf_destroy(name_buf);
    if(!module_symbol)
    {
        return false;
    }
    ok = ptrarray_add(module->symbols, module_symbol);
    if(!ok)
    {
        symbol_destroy(module_symbol);
        return false;
    }
    return true;
}



ApeObject_t object_make_from_data(ApeObjectType_t type, ApeObjectData_t* data)
{
    uint64_t type_tag;
    ApeObject_t object;
    object.handle = OBJECT_PATTERN;
    type_tag = get_type_tag(type) & 0x7;
    object.handle |= (type_tag << 48);
    object.handle |= (uintptr_t)data;// assumes no pointer exceeds 48 bits
    return object;
}

ApeObject_t object_make_number(double val)
{
    ApeObject_t o = { .number = val };
    if((o.handle & OBJECT_PATTERN) == OBJECT_PATTERN)
    {
        o.handle = 0x7ff8000000000000;
    }
    return o;
}

ApeObject_t object_make_bool(bool val)
{
    return (ApeObject_t){ .handle = OBJECT_BOOL_HEADER | val };
}

ApeObject_t object_make_null()
{
    return (ApeObject_t){ .handle = OBJECT_NULL_PATTERN };
}

ApeObject_t object_make_string(ApeGCMemory_t* mem, const char* string)
{
    int len = (int)strlen(string);
    ApeObject_t res = object_make_string_with_capacity(mem, len);
    if(object_is_null(res))
    {
        return res;
    }
    bool ok = object_string_append(res, string, len);
    if(!ok)
    {
        return object_make_null();
    }
    return res;
}

ApeObject_t object_make_string_with_capacity(ApeGCMemory_t* mem, int capacity)
{
    ApeObjectData_t* data = gcmem_get_object_data_from_pool(mem, APE_OBJECT_STRING);
    if(!data)
    {
        data = gcmem_alloc_object_data(mem, APE_OBJECT_STRING);
        if(!data)
        {
            return object_make_null();
        }
        data->string.capacity = OBJECT_STRING_BUF_SIZE - 1;
        data->string.is_allocated = false;
    }

    data->string.length = 0;
    data->string.hash = 0;

    if(capacity > data->string.capacity)
    {
        bool ok = object_data_string_reserve_capacity(data, capacity);
        if(!ok)
        {
            return object_make_null();
        }
    }

    return object_make_from_data(APE_OBJECT_STRING, data);
}

ApeObject_t object_make_stringf(ApeGCMemory_t* mem, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int to_write = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    va_start(args, fmt);
    ApeObject_t res = object_make_string_with_capacity(mem, to_write);
    if(object_is_null(res))
    {
        return object_make_null();
    }
    char* res_buf = object_get_mutable_string(res);
    int written = vsprintf(res_buf, fmt, args);
    (void)written;
    APE_ASSERT(written == to_write);
    va_end(args);
    object_set_string_length(res, to_write);
    return res;
}

ApeObject_t object_make_native_function_memory(ApeGCMemory_t* mem, const char* name, ApeNativeFNCallback_t fn, void* data, int data_len)
{
    if(data_len > NATIVE_FN_MAX_DATA_LEN)
    {
        return object_make_null();
    }
    ApeObjectData_t* obj = gcmem_alloc_object_data(mem, APE_OBJECT_NATIVE_FUNCTION);
    if(!obj)
    {
        return object_make_null();
    }
    obj->native_function.name = ape_strdup(mem->alloc, name);
    if(!obj->native_function.name)
    {
        return object_make_null();
    }
    obj->native_function.native_funcptr = fn;
    if(data)
    {
        //memcpy(obj->native_function.data, data, data_len);
        obj->native_function.data = data;
    }
    obj->native_function.data_len = data_len;
    return object_make_from_data(APE_OBJECT_NATIVE_FUNCTION, obj);
}

ApeObject_t object_make_array(ApeGCMemory_t* mem)
{
    return object_make_array_with_capacity(mem, 8);
}

ApeObject_t object_make_array_with_capacity(ApeGCMemory_t* mem, unsigned capacity)
{
    ApeObjectData_t* data = gcmem_get_object_data_from_pool(mem, APE_OBJECT_ARRAY);
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
    data->array = array_make_with_capacity(mem->alloc, capacity, sizeof(ApeObject_t));
    if(!data->array)
    {
        return object_make_null();
    }
    return object_make_from_data(APE_OBJECT_ARRAY, data);
}

ApeObject_t object_make_map(ApeGCMemory_t* mem)
{
    return object_make_map_with_capacity(mem, 32);
}

ApeObject_t object_make_map_with_capacity(ApeGCMemory_t* mem, unsigned capacity)
{
    ApeObjectData_t* data = gcmem_get_object_data_from_pool(mem, APE_OBJECT_MAP);
    if(data)
    {
        valdict_clear(data->map);
        return object_make_from_data(APE_OBJECT_MAP, data);
    }
    data = gcmem_alloc_object_data(mem, APE_OBJECT_MAP);
    if(!data)
    {
        return object_make_null();
    }
    data->map = valdict_make_with_capacity(mem->alloc, capacity, sizeof(ApeObject_t), sizeof(ApeObject_t));
    if(!data->map)
    {
        return object_make_null();
    }
    valdict_set_hash_function(data->map, (ApeCollectionsHashFNCallback_t)object_hash);
    valdict_set_equals_function(data->map, (ApeCollectionsEqualsFNCallback_t)object_equals_wrapped);
    return object_make_from_data(APE_OBJECT_MAP, data);
}

ApeObject_t object_make_error(ApeGCMemory_t* mem, const char* error)
{
    char* error_str = ape_strdup(mem->alloc, error);
    if(!error_str)
    {
        return object_make_null();
    }
    ApeObject_t res = object_make_error_no_copy(mem, error_str);
    if(object_is_null(res))
    {
        allocator_free(mem->alloc, error_str);
        return object_make_null();
    }
    return res;
}

ApeObject_t object_make_error_no_copy(ApeGCMemory_t* mem, char* error)
{
    ApeObjectData_t* data = gcmem_alloc_object_data(mem, APE_OBJECT_ERROR);
    if(!data)
    {
        return object_make_null();
    }
    data->error.message = error;
    data->error.traceback = NULL;
    return object_make_from_data(APE_OBJECT_ERROR, data);
}

ApeObject_t object_make_errorf(ApeGCMemory_t* mem, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int to_write = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    va_start(args, fmt);
    char* res = (char*)allocator_malloc(mem->alloc, to_write + 1);
    if(!res)
    {
        return object_make_null();
    }
    int written = vsprintf(res, fmt, args);
    (void)written;
    APE_ASSERT(written == to_write);
    va_end(args);
    ApeObject_t res_obj = object_make_error_no_copy(mem, res);
    if(object_is_null(res_obj))
    {
        allocator_free(mem->alloc, res);
        return object_make_null();
    }
    return res_obj;
}

ApeObject_t
object_make_function(ApeGCMemory_t* mem, const char* name, ApeCompilationResult_t* comp_res, bool owns_data, int num_locals, int num_args, int free_vals_count)
{
    ApeObjectData_t* data = gcmem_alloc_object_data(mem, APE_OBJECT_FUNCTION);
    if(!data)
    {
        return object_make_null();
    }
    if(owns_data)
    {
        data->function.name = name ? ape_strdup(mem->alloc, name) : ape_strdup(mem->alloc, "anonymous");
        if(!data->function.name)
        {
            return object_make_null();
        }
    }
    else
    {
        data->function.const_name = name ? name : "anonymous";
    }
    data->function.comp_result = comp_res;
    data->function.owns_data = owns_data;
    data->function.num_locals = num_locals;
    data->function.num_args = num_args;
    if(free_vals_count >= APE_ARRAY_LEN(data->function.free_vals_buf))
    {
        data->function.free_vals_allocated = (ApeObject_t*)allocator_malloc(mem->alloc, sizeof(ApeObject_t) * free_vals_count);
        if(!data->function.free_vals_allocated)
        {
            return object_make_null();
        }
    }
    data->function.free_vals_count = free_vals_count;
    return object_make_from_data(APE_OBJECT_FUNCTION, data);
}

ApeObject_t object_make_external(ApeGCMemory_t* mem, void* data)
{
    ApeObjectData_t* obj = gcmem_alloc_object_data(mem, APE_OBJECT_EXTERNAL);
    if(!obj)
    {
        return object_make_null();
    }
    obj->external.data = data;
    obj->external.data_destroy_fn = NULL;
    obj->external.data_copy_fn = NULL;
    return object_make_from_data(APE_OBJECT_EXTERNAL, obj);
}

void object_deinit(ApeObject_t obj)
{
    if(object_is_allocated(obj))
    {
        ApeObjectData_t* data = object_get_allocated_data(obj);
        object_data_deinit(data);
    }
}

void object_data_deinit(ApeObjectData_t* data)
{
    switch(data->type)
    {
        case APE_OBJECT_FREED:
        {
            APE_ASSERT(false);
            return;
        }
        case APE_OBJECT_STRING:
        {
            if(data->string.is_allocated)
            {
                allocator_free(data->mem->alloc, data->string.value_allocated);
            }
            break;
        }
        case APE_OBJECT_FUNCTION:
        {
            if(data->function.owns_data)
            {
                allocator_free(data->mem->alloc, data->function.name);
                compilation_result_destroy(data->function.comp_result);
            }
            if(freevals_are_allocated(&data->function))
            {
                allocator_free(data->mem->alloc, data->function.free_vals_allocated);
            }
            break;
        }
        case APE_OBJECT_ARRAY:
        {
            array_destroy(data->array);
            break;
        }
        case APE_OBJECT_MAP:
        {
            valdict_destroy(data->map);
            break;
        }
        case APE_OBJECT_NATIVE_FUNCTION:
        {
            allocator_free(data->mem->alloc, data->native_function.name);
            break;
        }
        case APE_OBJECT_EXTERNAL:
        {
            if(data->external.data_destroy_fn)
            {
                data->external.data_destroy_fn(data->external.data);
            }
            break;
        }
        case APE_OBJECT_ERROR:
        {
            allocator_free(data->mem->alloc, data->error.message);
            traceback_destroy(data->error.traceback);
            break;
        }
        default:
        {
            break;
        }
    }
    data->type = APE_OBJECT_FREED;
}

bool object_is_allocated(ApeObject_t object)
{
    return (object.handle & OBJECT_ALLOCATED_HEADER) == OBJECT_ALLOCATED_HEADER;
}

ApeGCMemory_t* object_get_mem(ApeObject_t obj)
{
    ApeObjectData_t* data = object_get_allocated_data(obj);
    return data->mem;
}

bool object_is_hashable(ApeObject_t obj)
{
    ApeObjectType_t type = object_get_type(obj);
    switch(type)
    {
        case APE_OBJECT_STRING:
            return true;
        case APE_OBJECT_NUMBER:
            return true;
        case APE_OBJECT_BOOL:
            return true;
        default:
            return false;
    }
}



const char* object_get_type_name(const ApeObjectType_t type)
{
    switch(type)
    {
        case APE_OBJECT_NONE:
            return "NONE";
        case APE_OBJECT_FREED:
            return "NONE";
        case APE_OBJECT_NUMBER:
            return "NUMBER";
        case APE_OBJECT_BOOL:
            return "BOOL";
        case APE_OBJECT_STRING:
            return "STRING";
        case APE_OBJECT_NULL:
            return "NULL";
        case APE_OBJECT_NATIVE_FUNCTION:
            return "NATIVE_FUNCTION";
        case APE_OBJECT_ARRAY:
            return "ARRAY";
        case APE_OBJECT_MAP:
            return "MAP";
        case APE_OBJECT_FUNCTION:
            return "FUNCTION";
        case APE_OBJECT_EXTERNAL:
            return "EXTERNAL";
        case APE_OBJECT_ERROR:
            return "ERROR";
        case APE_OBJECT_ANY:
            return "ANY";
    }
    return "NONE";
}

char* object_get_type_union_name(ApeAllocator_t* alloc, const ApeObjectType_t type)
{
    if(type == APE_OBJECT_ANY || type == APE_OBJECT_NONE || type == APE_OBJECT_FREED)
    {
        return ape_strdup(alloc, object_get_type_name(type));
    }
    ApeStringBuffer_t* res = strbuf_make(alloc);
    if(!res)
    {
        return NULL;
    }
    bool in_between = false;
#define CHECK_TYPE(t)                                    \
    do                                                   \
    {                                                    \
        if((type & t) == t)                              \
        {                                                \
            if(in_between)                               \
            {                                            \
                strbuf_append(res, "|");                 \
            }                                            \
            strbuf_append(res, object_get_type_name(t)); \
            in_between = true;                           \
        }                                                \
    } while(0)

    CHECK_TYPE(APE_OBJECT_NUMBER);
    CHECK_TYPE(APE_OBJECT_BOOL);
    CHECK_TYPE(APE_OBJECT_STRING);
    CHECK_TYPE(APE_OBJECT_NULL);
    CHECK_TYPE(APE_OBJECT_NATIVE_FUNCTION);
    CHECK_TYPE(APE_OBJECT_ARRAY);
    CHECK_TYPE(APE_OBJECT_MAP);
    CHECK_TYPE(APE_OBJECT_FUNCTION);
    CHECK_TYPE(APE_OBJECT_EXTERNAL);
    CHECK_TYPE(APE_OBJECT_ERROR);

    return strbuf_get_string_and_destroy(res);
}

char* object_serialize(ApeAllocator_t* alloc, ApeObject_t object, size_t* lendest)
{
    size_t l;
    char* string;
    ApeStringBuffer_t* buf;
    buf = strbuf_make(alloc);
    if(!buf)
    {
        return NULL;
    }
    object_to_string(object, buf, true);
    l = buf->len;
    string = strbuf_get_string_and_destroy(buf);
    if(lendest != NULL)
    {
        *lendest = l;
    }
    return string;
}

ApeObject_t object_deep_copy(ApeGCMemory_t* mem, ApeObject_t obj)
{
    ApeValDictionary_t* copies = valdict_make(mem->alloc, ApeObject_t, ApeObject_t);
    if(!copies)
    {
        return object_make_null();
    }
    ApeObject_t res = object_deep_copy_internal(mem, obj, copies);
    valdict_destroy(copies);
    return res;
}

ApeObject_t object_copy(ApeGCMemory_t* mem, ApeObject_t obj)
{
    ApeObject_t copy = object_make_null();
    ApeObjectType_t type = object_get_type(obj);
    switch(type)
    {
        case APE_OBJECT_ANY:
        case APE_OBJECT_FREED:
        case APE_OBJECT_NONE:
        {
            APE_ASSERT(false);
            copy = object_make_null();
            break;
        }
        case APE_OBJECT_NUMBER:
        case APE_OBJECT_BOOL:
        case APE_OBJECT_NULL:
        case APE_OBJECT_FUNCTION:
        case APE_OBJECT_NATIVE_FUNCTION:
        case APE_OBJECT_ERROR:
        {
            copy = obj;
            break;
        }
        case APE_OBJECT_STRING:
        {
            const char* str = object_get_string(obj);
            copy = object_make_string(mem, str);
            break;
        }
        case APE_OBJECT_ARRAY:
        {
            int len = object_get_array_length(obj);
            copy = object_make_array_with_capacity(mem, len);
            if(object_is_null(copy))
            {
                return object_make_null();
            }
            for(int i = 0; i < len; i++)
            {
                ApeObject_t item = object_get_array_value(obj, i);
                bool ok = object_add_array_value(copy, item);
                if(!ok)
                {
                    return object_make_null();
                }
            }
            break;
        }
        case APE_OBJECT_MAP:
        {
            copy = object_make_map(mem);
            for(int i = 0; i < object_get_map_length(obj); i++)
            {
                ApeObject_t key = (ApeObject_t)object_get_map_key_at(obj, i);
                ApeObject_t val = (ApeObject_t)object_get_map_value_at(obj, i);
                bool ok = object_set_map_value_object(copy, key, val);
                if(!ok)
                {
                    return object_make_null();
                }
            }
            break;
        }
        case APE_OBJECT_EXTERNAL:
        {
            copy = object_make_external(mem, NULL);
            if(object_is_null(copy))
            {
                return object_make_null();
            }
            ApeExternalData_t* external = object_get_external_data(obj);
            void* data_copy = NULL;
            if(external->data_copy_fn)
            {
                data_copy = external->data_copy_fn(external->data);
            }
            else
            {
                data_copy = external->data;
            }
            object_set_external_data(copy, data_copy);
            object_set_external_destroy_function(copy, external->data_destroy_fn);
            object_set_external_copy_function(copy, external->data_copy_fn);
            break;
        }
    }
    return copy;
}

double object_compare(ApeObject_t a, ApeObject_t b, bool* out_ok)
{
    if(a.handle == b.handle)
    {
        return 0;
    }

    *out_ok = true;

    ApeObjectType_t a_type = object_get_type(a);
    ApeObjectType_t b_type = object_get_type(b);

    if((a_type == APE_OBJECT_NUMBER || a_type == APE_OBJECT_BOOL || a_type == APE_OBJECT_NULL)
       && (b_type == APE_OBJECT_NUMBER || b_type == APE_OBJECT_BOOL || b_type == APE_OBJECT_NULL))
    {
        double left_val = object_get_number(a);
        double right_val = object_get_number(b);
        return left_val - right_val;
    }
    else if(a_type == b_type && a_type == APE_OBJECT_STRING)
    {
        int a_len = object_get_string_length(a);
        int b_len = object_get_string_length(b);
        if(a_len != b_len)
        {
            return a_len - b_len;
        }
        unsigned long a_hash = object_get_string_hash(a);
        unsigned long b_hash = object_get_string_hash(b);
        if(a_hash != b_hash)
        {
            return a_hash - b_hash;
        }
        const char* a_string = object_get_string(a);
        const char* b_string = object_get_string(b);
        return strcmp(a_string, b_string);
    }
    else if((object_is_allocated(a) || object_is_null(a)) && (object_is_allocated(b) || object_is_null(b)))
    {
        intptr_t a_data_val = (intptr_t)object_get_allocated_data(a);
        intptr_t b_data_val = (intptr_t)object_get_allocated_data(b);
        return (double)(a_data_val - b_data_val);
    }
    else
    {
        *out_ok = false;
    }
    return 1;
}

bool object_equals(ApeObject_t a, ApeObject_t b)
{
    ApeObjectType_t a_type = object_get_type(a);
    ApeObjectType_t b_type = object_get_type(b);

    if(a_type != b_type)
    {
        return false;
    }
    bool ok = false;
    double res = object_compare(a, b, &ok);
    return APE_DBLEQ(res, 0);
}

ApeExternalData_t* object_get_external_data(ApeObject_t object)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_EXTERNAL);
    ApeObjectData_t* data = object_get_allocated_data(object);
    return &data->external;
}

bool object_set_external_destroy_function(ApeObject_t object, ApeExternalDataDestroyFNCallback_t destroy_fn)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_EXTERNAL);
    ApeExternalData_t* data = object_get_external_data(object);
    if(!data)
    {
        return false;
    }
    data->data_destroy_fn = destroy_fn;
    return true;
}

ApeObjectData_t* object_get_allocated_data(ApeObject_t object)
{
    APE_ASSERT(object_is_allocated(object) || object_get_type(object) == APE_OBJECT_NULL);
    return (ApeObjectData_t*)(object.handle & ~OBJECT_HEADER_MASK);
}

bool object_get_bool(ApeObject_t obj)
{
    if(object_is_number(obj))
    {
        return obj.handle;
    }
    return obj.handle & (~OBJECT_HEADER_MASK);
}

double object_get_number(ApeObject_t obj)
{
    if(object_is_number(obj))
    {// todo: optimise? always return number?
        return obj.number;
    }
    return (double)(obj.handle & (~OBJECT_HEADER_MASK));
}

const char* object_get_string(ApeObject_t object)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_STRING);
    ApeObjectData_t* data = object_get_allocated_data(object);
    return object_data_get_string(data);
}

int object_get_string_length(ApeObject_t object)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_STRING);
    ApeObjectData_t* data = object_get_allocated_data(object);
    return data->string.length;
}

void object_set_string_length(ApeObject_t object, int len)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_STRING);
    ApeObjectData_t* data = object_get_allocated_data(object);
    data->string.length = len;
}


int object_get_string_capacity(ApeObject_t object)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_STRING);
    ApeObjectData_t* data = object_get_allocated_data(object);
    return data->string.capacity;
}

char* object_get_mutable_string(ApeObject_t object)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_STRING);
    ApeObjectData_t* data = object_get_allocated_data(object);
    return object_data_get_string(data);
}

bool object_string_append(ApeObject_t obj, const char* src, int len)
{
    APE_ASSERT(object_get_type(obj) == APE_OBJECT_STRING);
    ApeObjectData_t* data = object_get_allocated_data(obj);
    ApeObjectString_t* string = &data->string;
    char* str_buf = object_get_mutable_string(obj);
    int current_len = string->length;
    int capacity = string->capacity;
    if((len + current_len) > capacity)
    {
        APE_ASSERT(false);
        return false;
    }
    memcpy(str_buf + current_len, src, len);
    string->length += len;
    str_buf[string->length] = '\0';
    return true;
}

unsigned long object_get_string_hash(ApeObject_t obj)
{
    APE_ASSERT(object_get_type(obj) == APE_OBJECT_STRING);
    ApeObjectData_t* data = object_get_allocated_data(obj);
    if(data->string.hash == 0)
    {
        data->string.hash = object_hash_string(object_get_string(obj));
        if(data->string.hash == 0)
        {
            data->string.hash = 1;
        }
    }
    return data->string.hash;
}

ApeFunction_t* object_get_function(ApeObject_t object)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_FUNCTION);
    ApeObjectData_t* data = object_get_allocated_data(object);
    return &data->function;
}

ApeNativeFunction_t* object_get_native_function(ApeObject_t obj)
{
    ApeObjectData_t* data = object_get_allocated_data(obj);
    return &data->native_function;
}

ApeObjectType_t object_get_type(ApeObject_t obj)
{
    if(object_is_number(obj))
    {
        return APE_OBJECT_NUMBER;
    }
    uint64_t tag = (obj.handle >> 48) & 0x7;
    switch(tag)
    {
        case 0:
            return APE_OBJECT_NONE;
        case 1:
            return APE_OBJECT_BOOL;
        case 2:
            return APE_OBJECT_NULL;
        case 4:
        {
            ApeObjectData_t* data = object_get_allocated_data(obj);
            return data->type;
        }
        default:
            return APE_OBJECT_NONE;
    }
}

bool object_is_numeric(ApeObject_t obj)
{
    ApeObjectType_t type = object_get_type(obj);
    return type == APE_OBJECT_NUMBER || type == APE_OBJECT_BOOL;
}

bool object_is_null(ApeObject_t obj)
{
    return object_get_type(obj) == APE_OBJECT_NULL;
}

bool object_is_callable(ApeObject_t obj)
{
    ApeObjectType_t type = object_get_type(obj);
    return type == APE_OBJECT_NATIVE_FUNCTION || type == APE_OBJECT_FUNCTION;
}

const char* object_get_function_name(ApeObject_t obj)
{
    APE_ASSERT(object_get_type(obj) == APE_OBJECT_FUNCTION);
    ApeObjectData_t* data = object_get_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return NULL;
    }

    if(data->function.owns_data)
    {
        return data->function.name;
    }
    else
    {
        return data->function.const_name;
    }
}

ApeObject_t object_get_function_free_val(ApeObject_t obj, int ix)
{
    APE_ASSERT(object_get_type(obj) == APE_OBJECT_FUNCTION);
    ApeObjectData_t* data = object_get_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return object_make_null();
    }
    ApeFunction_t* fun = &data->function;
    APE_ASSERT(ix >= 0 && ix < fun->free_vals_count);
    if(ix < 0 || ix >= fun->free_vals_count)
    {
        return object_make_null();
    }
    if(freevals_are_allocated(fun))
    {
        return fun->free_vals_allocated[ix];
    }
    else
    {
        return fun->free_vals_buf[ix];
    }
}

void object_set_function_free_val(ApeObject_t obj, int ix, ApeObject_t val)
{
    APE_ASSERT(object_get_type(obj) == APE_OBJECT_FUNCTION);
    ApeObjectData_t* data = object_get_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return;
    }
    ApeFunction_t* fun = &data->function;
    APE_ASSERT(ix >= 0 && ix < fun->free_vals_count);
    if(ix < 0 || ix >= fun->free_vals_count)
    {
        return;
    }
    if(freevals_are_allocated(fun))
    {
        fun->free_vals_allocated[ix] = val;
    }
    else
    {
        fun->free_vals_buf[ix] = val;
    }
}

ApeObject_t* object_get_function_free_vals(ApeObject_t obj)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_get_type(obj) == APE_OBJECT_FUNCTION);
    data = object_get_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return NULL;
    }
    ApeFunction_t* fun = &data->function;
    if(freevals_are_allocated(fun))
    {
        return fun->free_vals_allocated;
    }
    else
    {
        return fun->free_vals_buf;
    }
}

const char* object_get_error_message(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ERROR);
    data = object_get_allocated_data(object);
    return data->error.message;
}

void object_set_error_traceback(ApeObject_t object, ApeTraceback_t* traceback)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ERROR);
    if(object_get_type(object) != APE_OBJECT_ERROR)
    {
        return;
    }
    ApeObjectData_t* data = object_get_allocated_data(object);
    APE_ASSERT(data->error.traceback == NULL);
    data->error.traceback = traceback;
}

ApeTraceback_t* object_get_error_traceback(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ERROR);
    data = object_get_allocated_data(object);
    return data->error.traceback;
}

bool object_set_external_data(ApeObject_t object, void* ext_data)
{
    ApeExternalData_t* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_EXTERNAL);
    data = object_get_external_data(object);
    if(!data)
    {
        return false;
    }
    data->data = ext_data;
    return true;
}

bool object_set_external_copy_function(ApeObject_t object, ApeExternalDataCopyFNCallback_t copy_fn)
{
    ApeExternalData_t* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_EXTERNAL);
    data = object_get_external_data(object);
    if(!data)
    {
        return false;
    }
    data->data_copy_fn = copy_fn;
    return true;
}

ApeObject_t object_get_array_value(ApeObject_t object, int ix)
{
    ApeObject_t* res;
    ApeArray_t* array;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ARRAY);
    array = object_get_allocated_array(object);
    if(ix < 0 || ix >= array_count(array))
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

bool object_set_array_value_at(ApeObject_t object, int ix, ApeObject_t val)
{
    ApeArray_t* array;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ARRAY);
    array = object_get_allocated_array(object);
    if(ix < 0 || ix >= array_count(array))
    {
        return false;
    }
    return array_set(array, ix, &val);
}

bool object_add_array_value(ApeObject_t object, ApeObject_t val)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ARRAY);
    ApeArray_t* array = object_get_allocated_array(object);
    return array_add(array, &val);
}

int object_get_array_length(ApeObject_t object)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ARRAY);
    ApeArray_t* array = object_get_allocated_array(object);
    return array_count(array);
}

bool object_remove_array_value_at(ApeObject_t object, int ix)
{
    ApeArray_t* array = object_get_allocated_array(object);
    return array_remove_at(array, ix);
}

int object_get_map_length(ApeObject_t object)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    ApeObjectData_t* data = object_get_allocated_data(object);
    return valdict_count(data->map);
}

ApeObject_t object_get_map_key_at(ApeObject_t object, int ix)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    ApeObjectData_t* data = object_get_allocated_data(object);
    ApeObject_t* res = (ApeObject_t*)valdict_get_key_at(data->map, ix);
    if(!res)
    {
        return object_make_null();
    }
    return *res;
}

ApeObject_t object_get_map_value_at(ApeObject_t object, int ix)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    ApeObjectData_t* data = object_get_allocated_data(object);
    ApeObject_t* res = (ApeObject_t*)valdict_get_value_at(data->map, ix);
    if(!res)
    {
        return object_make_null();
    }
    return *res;
}

bool object_set_map_value_at(ApeObject_t object, int ix, ApeObject_t val)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    if(ix >= object_get_map_length(object))
    {
        return false;
    }
    ApeObjectData_t* data = object_get_allocated_data(object);
    return valdict_set_value_at(data->map, ix, &val);
}

ApeObject_t object_get_kv_pair_at(ApeGCMemory_t* mem, ApeObject_t object, int ix)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    ApeObjectData_t* data = object_get_allocated_data(object);
    if(ix >= valdict_count(data->map))
    {
        return object_make_null();
    }
    ApeObject_t key = object_get_map_key_at(object, ix);
    ApeObject_t val = object_get_map_value_at(object, ix);
    ApeObject_t res = object_make_map(mem);
    if(object_is_null(res))
    {
        return object_make_null();
    }

    ApeObject_t key_obj = object_make_string(mem, "key");
    if(object_is_null(key_obj))
    {
        return object_make_null();
    }
    object_set_map_value_object(res, key_obj, key);

    ApeObject_t val_obj = object_make_string(mem, "value");
    if(object_is_null(val_obj))
    {
        return object_make_null();
    }
    object_set_map_value_object(res, val_obj, val);

    return res;
}

bool object_set_map_value_object(ApeObject_t object, ApeObject_t key, ApeObject_t val)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    ApeObjectData_t* data = object_get_allocated_data(object);
    return valdict_set(data->map, &key, &val);
}

ApeObject_t object_get_map_value_object(ApeObject_t object, ApeObject_t key)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    ApeObjectData_t* data = object_get_allocated_data(object);
    ApeObject_t* res = (ApeObject_t*)valdict_get(data->map, &key);
    if(!res)
    {
        return object_make_null();
    }
    return *res;
}

bool object_map_has_key_object(ApeObject_t object, ApeObject_t key)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    ApeObjectData_t* data = object_get_allocated_data(object);
    ApeObject_t* res = (ApeObject_t*)valdict_get(data->map, &key);
    return res != NULL;
}

// INTERNAL
ApeObject_t object_deep_copy_internal(ApeGCMemory_t* mem, ApeObject_t obj, ApeValDictionary_t * copies)
{
    ApeObject_t* copy_ptr = (ApeObject_t*)valdict_get(copies, &obj);
    if(copy_ptr)
    {
        return *copy_ptr;
    }

    ApeObject_t copy = object_make_null();

    ApeObjectType_t type = object_get_type(obj);
    switch(type)
    {
        case APE_OBJECT_FREED:
        case APE_OBJECT_ANY:
        case APE_OBJECT_NONE:
            {
                APE_ASSERT(false);
                copy = object_make_null();
            }
            break;

        case APE_OBJECT_NUMBER:
        case APE_OBJECT_BOOL:
        case APE_OBJECT_NULL:
        case APE_OBJECT_NATIVE_FUNCTION:
            {
                copy = obj;
            }
            break;

        case APE_OBJECT_STRING:
            {
                const char* str = object_get_string(obj);
                copy = object_make_string(mem, str);
            }
            break;

        case APE_OBJECT_FUNCTION:
            {
                ApeFunction_t* function = object_get_function(obj);
                uint8_t* bytecode_copy = NULL;
                ApePosition_t* src_positions_copy = NULL;
                ApeCompilationResult_t* comp_res_copy = NULL;

                bytecode_copy = (uint8_t*)allocator_malloc(mem->alloc, sizeof(uint8_t) * function->comp_result->count);
                if(!bytecode_copy)
                {
                    return object_make_null();
                }
                memcpy(bytecode_copy, function->comp_result->bytecode, sizeof(uint8_t) * function->comp_result->count);

                src_positions_copy = (ApePosition_t*)allocator_malloc(mem->alloc, sizeof(ApePosition_t) * function->comp_result->count);
                if(!src_positions_copy)
                {
                    allocator_free(mem->alloc, bytecode_copy);
                    return object_make_null();
                }
                memcpy(src_positions_copy, function->comp_result->src_positions, sizeof(ApePosition_t) * function->comp_result->count);

                comp_res_copy = compilation_result_make(mem->alloc, bytecode_copy, src_positions_copy,
                                                        function->comp_result->count);// todo: add compilation result copy function
                if(!comp_res_copy)
                {
                    allocator_free(mem->alloc, src_positions_copy);
                    allocator_free(mem->alloc, bytecode_copy);
                    return object_make_null();
                }

                copy = object_make_function(mem, object_get_function_name(obj), comp_res_copy, true, function->num_locals,
                                            function->num_args, 0);
                if(object_is_null(copy))
                {
                    compilation_result_destroy(comp_res_copy);
                    return object_make_null();
                }

                bool ok = valdict_set(copies, &obj, &copy);
                if(!ok)
                {
                    return object_make_null();
                }

                ApeFunction_t* function_copy = object_get_function(copy);
                if(freevals_are_allocated(function))
                {
                    function_copy->free_vals_allocated = (ApeObject_t*)allocator_malloc(mem->alloc, sizeof(ApeObject_t) * function->free_vals_count);
                    if(!function_copy->free_vals_allocated)
                    {
                        return object_make_null();
                    }
                }

                function_copy->free_vals_count = function->free_vals_count;
                for(int i = 0; i < function->free_vals_count; i++)
                {
                    ApeObject_t free_val = object_get_function_free_val(obj, i);
                    ApeObject_t free_val_copy = object_deep_copy_internal(mem, free_val, copies);
                    if(!object_is_null(free_val) && object_is_null(free_val_copy))
                    {
                        return object_make_null();
                    }
                    object_set_function_free_val(copy, i, free_val_copy);
                }
            }
            break;

        case APE_OBJECT_ARRAY:
        {
            int len = object_get_array_length(obj);
            copy = object_make_array_with_capacity(mem, len);
            if(object_is_null(copy))
            {
                return object_make_null();
            }
            bool ok = valdict_set(copies, &obj, &copy);
            if(!ok)
            {
                return object_make_null();
            }
            for(int i = 0; i < len; i++)
            {
                ApeObject_t item = object_get_array_value(obj, i);
                ApeObject_t item_copy = object_deep_copy_internal(mem, item, copies);
                if(!object_is_null(item) && object_is_null(item_copy))
                {
                    return object_make_null();
                }
                bool ok = object_add_array_value(copy, item_copy);
                if(!ok)
                {
                    return object_make_null();
                }
            }
            break;
        }
        case APE_OBJECT_MAP:
        {
            copy = object_make_map(mem);
            if(object_is_null(copy))
            {
                return object_make_null();
            }
            bool ok = valdict_set(copies, &obj, &copy);
            if(!ok)
            {
                return object_make_null();
            }
            for(int i = 0; i < object_get_map_length(obj); i++)
            {
                ApeObject_t key = object_get_map_key_at(obj, i);
                ApeObject_t val = object_get_map_value_at(obj, i);

                ApeObject_t key_copy = object_deep_copy_internal(mem, key, copies);
                if(!object_is_null(key) && object_is_null(key_copy))
                {
                    return object_make_null();
                }

                ApeObject_t val_copy = object_deep_copy_internal(mem, val, copies);
                if(!object_is_null(val) && object_is_null(val_copy))
                {
                    return object_make_null();
                }

                bool ok = object_set_map_value_object(copy, key_copy, val_copy);
                if(!ok)
                {
                    return object_make_null();
                }
            }
            break;
        }
        case APE_OBJECT_EXTERNAL:
        {
            copy = object_copy(mem, obj);
            break;
        }
        case APE_OBJECT_ERROR:
        {
            copy = obj;
            break;
        }
    }
    return copy;
}


bool object_equals_wrapped(const ApeObject_t* a_ptr, const ApeObject_t* b_ptr)
{
    ApeObject_t a = *a_ptr;
    ApeObject_t b = *b_ptr;
    return object_equals(a, b);
}

unsigned long object_hash(ApeObject_t* obj_ptr)
{
    ApeObject_t obj = *obj_ptr;
    ApeObjectType_t type = object_get_type(obj);

    switch(type)
    {
        case APE_OBJECT_NUMBER:
        {
            double val = object_get_number(obj);
            return object_hash_double(val);
        }
        case APE_OBJECT_BOOL:
        {
            bool val = object_get_bool(obj);
            return val;
        }
        case APE_OBJECT_STRING:
        {
            return object_get_string_hash(obj);
        }
        default:
        {
            return 0;
        }
    }
}

unsigned long object_hash_string(const char* str)
{ /* djb2 */
    unsigned long hash = 5381;
    int c;
    while((c = *str++))
    {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

unsigned long object_hash_double(double val)
{ /* djb2 */
    uint32_t* val_ptr = (uint32_t*)&val;
    unsigned long hash = 5381;
    hash = ((hash << 5) + hash) + val_ptr[0];
    hash = ((hash << 5) + hash) + val_ptr[1];
    return hash;
}

ApeArray_t * object_get_allocated_array(ApeObject_t object)
{
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ARRAY);
    ApeObjectData_t* data = object_get_allocated_data(object);
    return data->array;
}

bool object_is_number(ApeObject_t o)
{
    return (o.handle & OBJECT_PATTERN) != OBJECT_PATTERN;
}

uint64_t get_type_tag(ApeObjectType_t type)
{
    switch(type)
    {
        case APE_OBJECT_NONE:
            return 0;
        case APE_OBJECT_BOOL:
            return 1;
        case APE_OBJECT_NULL:
            return 2;
        default:
            return 4;
    }
}

bool freevals_are_allocated(ApeFunction_t* fun)
{
    return fun->free_vals_count >= APE_ARRAY_LEN(fun->free_vals_buf);
}

char* object_data_get_string(ApeObjectData_t* data)
{
    APE_ASSERT(data->type == APE_OBJECT_STRING);
    if(data->string.is_allocated)
    {
        return data->string.value_allocated;
    }
    else
    {
        return data->string.value_buf;
    }
}

bool object_data_string_reserve_capacity(ApeObjectData_t* data, int capacity)
{
    APE_ASSERT(capacity >= 0);

    ApeObjectString_t* string = &data->string;

    string->length = 0;
    string->hash = 0;

    if(capacity <= string->capacity)
    {
        return true;
    }

    if(capacity <= (OBJECT_STRING_BUF_SIZE - 1))
    {
        if(string->is_allocated)
        {
            APE_ASSERT(false);// should never happen
            allocator_free(data->mem->alloc, string->value_allocated);// just in case
        }
        string->capacity = OBJECT_STRING_BUF_SIZE - 1;
        string->is_allocated = false;
        return true;
    }

    char* new_value = (char*)allocator_malloc(data->mem->alloc, capacity + 1);
    if(!new_value)
    {
        return false;
    }

    if(string->is_allocated)
    {
        allocator_free(data->mem->alloc, string->value_allocated);
    }

    string->value_allocated = new_value;
    string->is_allocated = true;
    string->capacity = capacity;
    return true;
}

ApeObjectDataPool_t* get_pool_for_type(ApeGCMemory_t* mem, ApeObjectType_t type);
bool can_data_be_put_in_pool(ApeGCMemory_t* mem, ApeObjectData_t* data);

ApeGCMemory_t* gcmem_make(ApeAllocator_t* alloc)
{
    int i;
    ApeGCMemory_t* mem = (ApeGCMemory_t*)allocator_malloc(alloc, sizeof(ApeGCMemory_t));
    if(!mem)
    {
        return NULL;
    }
    memset(mem, 0, sizeof(ApeGCMemory_t));
    mem->alloc = alloc;
    mem->objects = ptrarray_make(alloc);
    if(!mem->objects)
    {
        goto error;
    }
    mem->objects_back = ptrarray_make(alloc);
    if(!mem->objects_back)
    {
        goto error;
    }
    mem->objects_not_gced = array_make(alloc, ApeObject_t);
    if(!mem->objects_not_gced)
    {
        goto error;
    }
    mem->allocations_since_sweep = 0;
    mem->data_only_pool.count = 0;
    for(i = 0; i < GCMEM_POOLS_NUM; i++)
    {
        ApeObjectDataPool_t* pool = &mem->pools[i];
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
    if(!mem)
    {
        return;
    }

    array_destroy(mem->objects_not_gced);
    ptrarray_destroy(mem->objects_back);

    for(int i = 0; i < ptrarray_count(mem->objects); i++)
    {
        ApeObjectData_t* obj = (ApeObjectData_t*)ptrarray_get(mem->objects, i);
        object_data_deinit(obj);
        allocator_free(mem->alloc, obj);
    }
    ptrarray_destroy(mem->objects);

    for(int i = 0; i < GCMEM_POOLS_NUM; i++)
    {
        ApeObjectDataPool_t* pool = &mem->pools[i];
        for(int j = 0; j < pool->count; j++)
        {
            ApeObjectData_t* data = pool->datapool[j];
            object_data_deinit(data);
            allocator_free(mem->alloc, data);
        }
        memset(pool, 0, sizeof(ApeObjectDataPool_t));
    }

    for(int i = 0; i < mem->data_only_pool.count; i++)
    {
        allocator_free(mem->alloc, mem->data_only_pool.datapool[i]);
    }

    allocator_free(mem->alloc, mem);
}

ApeObjectData_t* gcmem_alloc_object_data(ApeGCMemory_t* mem, ApeObjectType_t type)
{
    ApeObjectData_t* data = NULL;
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
    bool ok = ptrarray_add(mem->objects_back, data);
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
    ApeObjectDataPool_t* pool = get_pool_for_type(mem, type);
    if(!pool || pool->count <= 0)
    {
        return NULL;
    }
    ApeObjectData_t* data = pool->datapool[pool->count - 1];

    APE_ASSERT(ptrarray_count(mem->objects_back) >= ptrarray_count(mem->objects));

    // we want to make sure that appending to objects_back never fails in sweep
    // so this only reserves space there.
    bool ok = ptrarray_add(mem->objects_back, data);
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
    for(int i = 0; i < ptrarray_count(mem->objects); i++)
    {
        ApeObjectData_t* data = (ApeObjectData_t*)ptrarray_get(mem->objects, i);
        data->gcmark = false;
    }
}

void gc_mark_objects(ApeObject_t* objects, int count)
{
    for(int i = 0; i < count; i++)
    {
        ApeObject_t obj = objects[i];
        gc_mark_object(obj);
    }
}

void gc_mark_object(ApeObject_t obj)
{
    if(!object_is_allocated(obj))
    {
        return;
    }

    ApeObjectData_t* data = object_get_allocated_data(obj);
    if(data->gcmark)
    {
        return;
    }

    data->gcmark = true;
    switch(data->type)
    {
        case APE_OBJECT_MAP:
        {
            int len = object_get_map_length(obj);
            for(int i = 0; i < len; i++)
            {
                ApeObject_t key = object_get_map_key_at(obj, i);
                if(object_is_allocated(key))
                {
                    ApeObjectData_t* key_data = object_get_allocated_data(key);
                    if(!key_data->gcmark)
                    {
                        gc_mark_object(key);
                    }
                }
                ApeObject_t val = object_get_map_value_at(obj, i);
                if(object_is_allocated(val))
                {
                    ApeObjectData_t* val_data = object_get_allocated_data(val);
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
            int len = object_get_array_length(obj);
            for(int i = 0; i < len; i++)
            {
                ApeObject_t val = object_get_array_value(obj, i);
                if(object_is_allocated(val))
                {
                    ApeObjectData_t* val_data = object_get_allocated_data(val);
                    if(!val_data->gcmark)
                    {
                        gc_mark_object(val);
                    }
                }
            }
            break;
        }
        case APE_OBJECT_FUNCTION:
        {
            ApeFunction_t* function = object_get_function(obj);
            for(int i = 0; i < function->free_vals_count; i++)
            {
                ApeObject_t free_val = object_get_function_free_val(obj, i);
                gc_mark_object(free_val);
                if(object_is_allocated(free_val))
                {
                    ApeObjectData_t* free_val_data = object_get_allocated_data(free_val);
                    if(!free_val_data->gcmark)
                    {
                        gc_mark_object(free_val);
                    }
                }
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

void gc_sweep(ApeGCMemory_t* mem)
{
    int i;
    bool ok;
    gc_mark_objects((ApeObject_t*)array_data(mem->objects_not_gced), array_count(mem->objects_not_gced));

    APE_ASSERT(ptrarray_count(mem->objects_back) >= ptrarray_count(mem->objects));

    ptrarray_clear(mem->objects_back);
    for(i = 0; i < ptrarray_count(mem->objects); i++)
    {
        ApeObjectData_t* data = (ApeObjectData_t*)ptrarray_get(mem->objects, i);
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
                ApeObjectDataPool_t* pool = get_pool_for_type(mem, data->type);
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
    ApePtrArray_t* objs_temp = mem->objects;
    mem->objects = mem->objects_back;
    mem->objects_back = objs_temp;
    mem->allocations_since_sweep = 0;
}

bool gc_disable_on_object(ApeObject_t obj)
{
    if(!object_is_allocated(obj))
    {
        return false;
    }
    ApeObjectData_t* data = object_get_allocated_data(obj);
    if(array_contains(data->mem->objects_not_gced, &obj))
    {
        return false;
    }
    bool ok = array_add(data->mem->objects_not_gced, &obj);
    return ok;
}

void gc_enable_on_object(ApeObject_t obj)
{
    if(!object_is_allocated(obj))
    {
        return;
    }
    ApeObjectData_t* data = object_get_allocated_data(obj);
    array_remove_item(data->mem->objects_not_gced, &obj);
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
            return &mem->pools[0];
        case APE_OBJECT_MAP:
            return &mem->pools[1];
        case APE_OBJECT_STRING:
            return &mem->pools[2];
        default:
            return NULL;
    }
}

bool can_data_be_put_in_pool(ApeGCMemory_t* mem, ApeObjectData_t* data)
{
    ApeObject_t obj = object_make_from_data(data->type, data);

    // this is to ensure that large objects won't be kept in pool indefinitely
    switch(data->type)
    {
        case APE_OBJECT_ARRAY:
        {
            if(object_get_array_length(obj) > 1024)
            {
                return false;
            }
            break;
        }
        case APE_OBJECT_MAP:
        {
            if(object_get_map_length(obj) > 1024)
            {
                return false;
            }
            break;
        }
        case APE_OBJECT_STRING:
        {
            if(!data->string.is_allocated || data->string.capacity > 4096)
            {
                return false;
            }
            break;
        }
        default:
            break;
    }

    ApeObjectDataPool_t* pool = get_pool_for_type(mem, data->type);
    if(!pool || pool->count >= GCMEM_POOL_SIZE)
    {
        return false;
    }

    return true;
}


ApeTraceback_t* traceback_make(ApeAllocator_t* alloc)
{
    ApeTraceback_t* traceback = (ApeTraceback_t*)allocator_malloc(alloc, sizeof(ApeTraceback_t));
    if(!traceback)
    {
        return NULL;
    }
    memset(traceback, 0, sizeof(ApeTraceback_t));
    traceback->alloc = alloc;
    traceback->items = array_make(alloc, ApeTracebackItem_t);
    if(!traceback->items)
    {
        traceback_destroy(traceback);
        return NULL;
    }
    return traceback;
}

void traceback_destroy(ApeTraceback_t* traceback)
{
    if(!traceback)
    {
        return;
    }
    for(int i = 0; i < array_count(traceback->items); i++)
    {
        ApeTracebackItem_t* item = (ApeTracebackItem_t*)array_get(traceback->items, i);
        allocator_free(traceback->alloc, item->function_name);
    }
    array_destroy(traceback->items);
    allocator_free(traceback->alloc, traceback);
}

bool traceback_append(ApeTraceback_t* traceback, const char* function_name, ApePosition_t pos)
{
    ApeTracebackItem_t item;
    item.function_name = ape_strdup(traceback->alloc, function_name);
    if(!item.function_name)
    {
        return false;
    }
    item.pos = pos;
    bool ok = array_add(traceback->items, &item);
    if(!ok)
    {
        allocator_free(traceback->alloc, item.function_name);
        return false;
    }
    return true;
}

bool traceback_append_from_vm(ApeTraceback_t* traceback, ApeVM_t* vm)
{
    for(int i = vm->frames_count - 1; i >= 0; i--)
    {
        ApeFrame_t* frame = &vm->frames[i];
        bool ok = traceback_append(traceback, object_get_function_name(frame->function), frame_src_position(frame));
        if(!ok)
        {
            return false;
        }
    }
    return true;
}



const char* traceback_item_get_line(ApeTracebackItem_t* item)
{
    if(!item->pos.file)
    {
        return NULL;
    }
    ApePtrArray_t* lines = item->pos.file->lines;
    if(item->pos.line >= ptrarray_count(lines))
    {
        return NULL;
    }
    const char* line = (const char*)ptrarray_get(lines, item->pos.line);
    return line;
}

const char* traceback_item_get_filepath(ApeTracebackItem_t* item)
{
    if(!item->pos.file)
    {
        return NULL;
    }
    return item->pos.file->path;
}

bool frame_init(ApeFrame_t* frame, ApeObject_t function_obj, int base_pointer)
{
    if(object_get_type(function_obj) != APE_OBJECT_FUNCTION)
    {
        return false;
    }
    ApeFunction_t* function = object_get_function(function_obj);
    frame->function = function_obj;
    frame->ip = 0;
    frame->base_pointer = base_pointer;
    frame->src_ip = 0;
    frame->bytecode = function->comp_result->bytecode;
    frame->src_positions = function->comp_result->src_positions;
    frame->bytecode_size = function->comp_result->count;
    frame->recover_ip = -1;
    frame->is_recovering = false;
    return true;
}

ApeOpcodeValue_t frame_read_opcode(ApeFrame_t* frame)
{
    frame->src_ip = frame->ip;
    return (ApeOpcodeValue_t)frame_read_uint8(frame);
}

uint64_t frame_read_uint64(ApeFrame_t* frame)
{
    const uint8_t* data = frame->bytecode + frame->ip;
    frame->ip += 8;
    uint64_t res = 0;
    res |= (uint64_t)data[7];
    res |= (uint64_t)data[6] << 8;
    res |= (uint64_t)data[5] << 16;
    res |= (uint64_t)data[4] << 24;
    res |= (uint64_t)data[3] << 32;
    res |= (uint64_t)data[2] << 40;
    res |= (uint64_t)data[1] << 48;
    res |= (uint64_t)data[0] << 56;
    return res;
}

uint16_t frame_read_uint16(ApeFrame_t* frame)
{
    const uint8_t* data = frame->bytecode + frame->ip;
    frame->ip += 2;
    return (data[0] << 8) | data[1];
}

uint8_t frame_read_uint8(ApeFrame_t* frame)
{
    const uint8_t* data = frame->bytecode + frame->ip;
    frame->ip++;
    return data[0];
}

ApePosition_t frame_src_position(const ApeFrame_t* frame)
{
    if(frame->src_positions)
    {
        return frame->src_positions[frame->src_ip];
    }
    return src_pos_invalid;
}


ApeVM_t* vm_make(ApeAllocator_t* alloc, const ApeConfig_t* config, ApeGCMemory_t* mem, ApeErrorList_t* errors, ApeGlobalStore_t* global_store)
{
    ApeVM_t* vm = (ApeVM_t*)allocator_malloc(alloc, sizeof(ApeVM_t));
    if(!vm)
    {
        return NULL;
    }
    memset(vm, 0, sizeof(ApeVM_t));
    vm->alloc = alloc;
    vm->config = config;
    vm->mem = mem;
    vm->errors = errors;
    vm->global_store = global_store;
    vm->globals_count = 0;
    vm->sp = 0;
    vm->this_sp = 0;
    vm->frames_count = 0;
    vm->last_popped = object_make_null();
    vm->running = false;

    for(int i = 0; i < OPCODE_MAX; i++)
    {
        vm->operator_oveload_keys[i] = object_make_null();
    }
#define SET_OPERATOR_OVERLOAD_KEY(op, key)                   \
    do                                                       \
    {                                                        \
        ApeObject_t key_obj = object_make_string(vm->mem, key); \
        if(object_is_null(key_obj))                          \
        {                                                    \
            goto err;                                        \
        }                                                    \
        vm->operator_oveload_keys[op] = key_obj;             \
    } while(0)
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_ADD, "__operator_add__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_SUB, "__operator_sub__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_MUL, "__operator_mul__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_DIV, "__operator_div__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_MOD, "__operator_mod__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_OR, "__operator_or__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_XOR, "__operator_xor__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_AND, "__operator_and__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_LSHIFT, "__operator_lshift__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_RSHIFT, "__operator_rshift__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_MINUS, "__operator_minus__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_BANG, "__operator_bang__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_COMPARE, "__cmp__");
#undef SET_OPERATOR_OVERLOAD_KEY

    return vm;
err:
    vm_destroy(vm);
    return NULL;
}

void vm_destroy(ApeVM_t* vm)
{
    if(!vm)
    {
        return;
    }
    allocator_free(vm->alloc, vm);
}

void vm_reset(ApeVM_t* vm)
{
    vm->sp = 0;
    vm->this_sp = 0;
    while(vm->frames_count > 0)
    {
        pop_frame(vm);
    }
}

bool vm_run(ApeVM_t* vm, ApeCompilationResult_t* comp_res, ApeArray_t * constants)
{
#ifdef APE_DEBUG
    int old_sp = vm->sp;
#endif
    int old_this_sp = vm->this_sp;
    int old_frames_count = vm->frames_count;
    ApeObject_t main_fn = object_make_function(vm->mem, "main", comp_res, false, 0, 0, 0);
    if(object_is_null(main_fn))
    {
        return false;
    }
    stack_push(vm, main_fn);
    bool res = vm_execute_function(vm, main_fn, constants);
    while(vm->frames_count > old_frames_count)
    {
        pop_frame(vm);
    }
    APE_ASSERT(vm->sp == old_sp);
    vm->this_sp = old_this_sp;
    return res;
}

ApeObject_t vm_call(ApeVM_t* vm, ApeArray_t * constants, ApeObject_t callee, int argc, ApeObject_t* args)
{
    ApeObjectType_t type = object_get_type(callee);
    if(type == APE_OBJECT_FUNCTION)
    {
#ifdef APE_DEBUG
        int old_sp = vm->sp;
#endif
        int old_this_sp = vm->this_sp;
        int old_frames_count = vm->frames_count;
        stack_push(vm, callee);
        for(int i = 0; i < argc; i++)
        {
            stack_push(vm, args[i]);
        }
        bool ok = vm_execute_function(vm, callee, constants);
        if(!ok)
        {
            return object_make_null();
        }
        while(vm->frames_count > old_frames_count)
        {
            pop_frame(vm);
        }
        APE_ASSERT(vm->sp == old_sp);
        vm->this_sp = old_this_sp;
        return vm_get_last_popped(vm);
    }
    else if(type == APE_OBJECT_NATIVE_FUNCTION)
    {
        return call_native_function(vm, callee, src_pos_invalid, argc, args);
    }
    else
    {
        errors_add_error(vm->errors, APE_ERROR_USER, src_pos_invalid, "Object is not callable");
        return object_make_null();
    }
}

bool vm_execute_function(ApeVM_t* vm, ApeObject_t function, ApeArray_t * constants)
{
    if(vm->running)
    {
        errors_add_error(vm->errors, APE_ERROR_USER, src_pos_invalid, "VM is already executing code");
        return false;
    }

    ApeFunction_t* function_function = object_get_function(function);// naming is hard
    ApeFrame_t new_frame;
    bool ok = false;
    ok = frame_init(&new_frame, function, vm->sp - function_function->num_args);
    if(!ok)
    {
        return false;
    }
    ok = push_frame(vm, new_frame);
    if(!ok)
    {
        errors_add_error(vm->errors, APE_ERROR_USER, src_pos_invalid, "Pushing frame failed");
        return false;
    }

    vm->running = true;
    vm->last_popped = object_make_null();

    bool check_time = false;
    double max_exec_time_ms = 0;
    if(vm->config)
    {
        check_time = vm->config->max_execution_time_set;
        max_exec_time_ms = vm->config->max_execution_time_ms;
    }
    unsigned time_check_interval = 1000;
    unsigned time_check_counter = 0;
    ApeTimer_t timer;
    memset(&timer, 0, sizeof(ApeTimer_t));
    if(check_time)
    {
        timer = ape_timer_start();
    }

    while(vm->current_frame->ip < vm->current_frame->bytecode_size)
    {
        ApeOpcodeValue_t opcode = frame_read_opcode(vm->current_frame);
        switch(opcode)
        {
            case OPCODE_CONSTANT:
                {
                    uint16_t constant_ix = frame_read_uint16(vm->current_frame);
                    ApeObject_t* constant = (ApeObject_t*)array_get(constants, constant_ix);
                    if(!constant)
                    {
                        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "Constant at %d not found", constant_ix);
                        goto err;
                    }
                    stack_push(vm, *constant);
                }
                break;

            case OPCODE_ADD:
            case OPCODE_SUB:
            case OPCODE_MUL:
            case OPCODE_DIV:
            case OPCODE_MOD:
            case OPCODE_OR:
            case OPCODE_XOR:
            case OPCODE_AND:
            case OPCODE_LSHIFT:
            case OPCODE_RSHIFT:
                {
                    ApeObject_t right = stack_pop(vm);
                    ApeObject_t left = stack_pop(vm);
                    ApeObjectType_t left_type = object_get_type(left);
                    ApeObjectType_t right_type = object_get_type(right);
                    if(object_is_numeric(left) && object_is_numeric(right))
                    {
                        double right_val = object_get_number(right);
                        double left_val = object_get_number(left);

                        int64_t left_val_int = (int64_t)left_val;
                        int64_t right_val_int = (int64_t)right_val;

                        double res = 0;
                        switch(opcode)
                        {
                            case OPCODE_ADD:
                                res = left_val + right_val;
                                break;
                            case OPCODE_SUB:
                                res = left_val - right_val;
                                break;
                            case OPCODE_MUL:
                                res = left_val * right_val;
                                break;
                            case OPCODE_DIV:
                                res = left_val / right_val;
                                break;
                            case OPCODE_MOD:
                                res = fmod(left_val, right_val);
                                break;
                            case OPCODE_OR:
                                res = (double)(left_val_int | right_val_int);
                                break;
                            case OPCODE_XOR:
                                res = (double)(left_val_int ^ right_val_int);
                                break;
                            case OPCODE_AND:
                                res = (double)(left_val_int & right_val_int);
                                break;
                            case OPCODE_LSHIFT:
                                res = (double)(left_val_int << right_val_int);
                                break;
                            case OPCODE_RSHIFT:
                                res = (double)(left_val_int >> right_val_int);
                                break;
                            default:
                                APE_ASSERT(false);
                                break;
                        }
                        stack_push(vm, object_make_number(res));
                    }
                    else if(left_type == APE_OBJECT_STRING && right_type == APE_OBJECT_STRING && opcode == OPCODE_ADD)
                    {
                        int left_len = (int)object_get_string_length(left);
                        int right_len = (int)object_get_string_length(right);

                        if(left_len == 0)
                        {
                            stack_push(vm, right);
                        }
                        else if(right_len == 0)
                        {
                            stack_push(vm, left);
                        }
                        else
                        {
                            const char* left_val = object_get_string(left);
                            const char* right_val = object_get_string(right);

                            ApeObject_t res = object_make_string_with_capacity(vm->mem, left_len + right_len);
                            if(object_is_null(res))
                            {
                                goto err;
                            }

                            ok = object_string_append(res, left_val, left_len);
                            if(!ok)
                            {
                                goto err;
                            }

                            ok = object_string_append(res, right_val, right_len);
                            if(!ok)
                            {
                                goto err;
                            }
                            stack_push(vm, res);
                        }
                    }
                    else if((left_type == APE_OBJECT_ARRAY) && opcode == OPCODE_ADD)
                    {
                        object_add_array_value(left, right);
                        stack_push(vm, left);
                    }
                    else
                    {
                        bool overload_found = false;
                        bool ok = try_overload_operator(vm, left, right, opcode, &overload_found);
                        if(!ok)
                        {
                            goto err;
                        }
                        if(!overload_found)
                        {
                            const char* opcode_name = opcode_get_name(opcode);
                            const char* left_type_name = object_get_type_name(left_type);
                            const char* right_type_name = object_get_type_name(right_type);
                            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "Invalid operand types for %s, got %s and %s", opcode_name, left_type_name,
                                              right_type_name);
                            goto err;
                        }
                    }
                }
                break;

            case OPCODE_POP:
                {
                    stack_pop(vm);
                }
                break;

            case OPCODE_TRUE:
                {
                    stack_push(vm, object_make_bool(true));
                }
                break;

            case OPCODE_FALSE:
            {
                stack_push(vm, object_make_bool(false));
                break;
            }
            case OPCODE_COMPARE:
            case OPCODE_COMPARE_EQ:
                {
                    ApeObject_t right = stack_pop(vm);
                    ApeObject_t left = stack_pop(vm);
                    bool is_overloaded = false;
                    bool ok = try_overload_operator(vm, left, right, OPCODE_COMPARE, &is_overloaded);
                    if(!ok)
                    {
                        goto err;
                    }
                    if(!is_overloaded)
                    {
                        double comparison_res = object_compare(left, right, &ok);
                        if(ok || opcode == OPCODE_COMPARE_EQ)
                        {
                            ApeObject_t res = object_make_number(comparison_res);
                            stack_push(vm, res);
                        }
                        else
                        {
                            const char* right_type_string = object_get_type_name(object_get_type(right));
                            const char* left_type_string = object_get_type_name(object_get_type(left));
                            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "Cannot compare %s and %s", left_type_string, right_type_string);
                            goto err;
                        }
                    }
                }
                break;

            case OPCODE_EQUAL:
            case OPCODE_NOT_EQUAL:
            case OPCODE_GREATER_THAN:
            case OPCODE_GREATER_THAN_EQUAL:
                {
                    ApeObject_t value = stack_pop(vm);
                    double comparison_res = object_get_number(value);
                    bool res_val = false;
                    switch(opcode)
                    {
                        case OPCODE_EQUAL:
                            res_val = APE_DBLEQ(comparison_res, 0);
                            break;
                        case OPCODE_NOT_EQUAL:
                            res_val = !APE_DBLEQ(comparison_res, 0);
                            break;
                        case OPCODE_GREATER_THAN:
                            res_val = comparison_res > 0;
                            break;
                        case OPCODE_GREATER_THAN_EQUAL:
                        {
                            res_val = comparison_res > 0 || APE_DBLEQ(comparison_res, 0);
                            break;
                        }
                        default:
                            APE_ASSERT(false);
                            break;
                    }
                    ApeObject_t res = object_make_bool(res_val);
                    stack_push(vm, res);
                }
                break;

            case OPCODE_MINUS:
                {
                    ApeObject_t operand = stack_pop(vm);
                    ApeObjectType_t operand_type = object_get_type(operand);
                    if(operand_type == APE_OBJECT_NUMBER)
                    {
                        double val = object_get_number(operand);
                        ApeObject_t res = object_make_number(-val);
                        stack_push(vm, res);
                    }
                    else
                    {
                        bool overload_found = false;
                        bool ok = try_overload_operator(vm, operand, object_make_null(), OPCODE_MINUS, &overload_found);
                        if(!ok)
                        {
                            goto err;
                        }
                        if(!overload_found)
                        {
                            const char* operand_type_string = object_get_type_name(operand_type);
                            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "Invalid operand type for MINUS, got %s", operand_type_string);
                            goto err;
                        }
                    }
                }
                break;

            case OPCODE_BANG:
                {
                    ApeObject_t operand = stack_pop(vm);
                    ApeObjectType_t type = object_get_type(operand);
                    if(type == APE_OBJECT_BOOL)
                    {
                        ApeObject_t res = object_make_bool(!object_get_bool(operand));
                        stack_push(vm, res);
                    }
                    else if(type == APE_OBJECT_NULL)
                    {
                        ApeObject_t res = object_make_bool(true);
                        stack_push(vm, res);
                    }
                    else
                    {
                        bool overload_found = false;
                        bool ok = try_overload_operator(vm, operand, object_make_null(), OPCODE_BANG, &overload_found);
                        if(!ok)
                        {
                            goto err;
                        }
                        if(!overload_found)
                        {
                            ApeObject_t res = object_make_bool(false);
                            stack_push(vm, res);
                        }
                    }
                }
                break;

            case OPCODE_JUMP:
                {
                    uint16_t pos = frame_read_uint16(vm->current_frame);
                    vm->current_frame->ip = pos;
                }
                break;

            case OPCODE_JUMP_IF_FALSE:
                {
                    uint16_t pos = frame_read_uint16(vm->current_frame);
                    ApeObject_t test = stack_pop(vm);
                    if(!object_get_bool(test))
                    {
                        vm->current_frame->ip = pos;
                    }
                }
                break;

            case OPCODE_JUMP_IF_TRUE:
                {
                    uint16_t pos = frame_read_uint16(vm->current_frame);
                    ApeObject_t test = stack_pop(vm);
                    if(object_get_bool(test))
                    {
                        vm->current_frame->ip = pos;
                    }
                }
                break;

            case OPCODE_NULL:
                {
                    stack_push(vm, object_make_null());
                }
                break;

            case OPCODE_DEFINE_MODULE_GLOBAL:
            {
                uint16_t ix = frame_read_uint16(vm->current_frame);
                ApeObject_t value = stack_pop(vm);
                vm_set_global(vm, ix, value);
                break;
            }
            case OPCODE_SET_MODULE_GLOBAL:
                {
                    uint16_t ix = frame_read_uint16(vm->current_frame);
                    ApeObject_t new_value = stack_pop(vm);
                    ApeObject_t old_value = vm_get_global(vm, ix);
                    if(!check_assign(vm, old_value, new_value))
                    {
                        goto err;
                    }
                    vm_set_global(vm, ix, new_value);
                }
                break;

            case OPCODE_GET_MODULE_GLOBAL:
                {
                    uint16_t ix = frame_read_uint16(vm->current_frame);
                    ApeObject_t global = vm->globals[ix];
                    stack_push(vm, global);
                }
                break;

            case OPCODE_ARRAY:
                {
                    uint16_t count = frame_read_uint16(vm->current_frame);
                    ApeObject_t array_obj = object_make_array_with_capacity(vm->mem, count);
                    if(object_is_null(array_obj))
                    {
                        goto err;
                    }
                    ApeObject_t* items = vm->stack + vm->sp - count;
                    for(int i = 0; i < count; i++)
                    {
                        ApeObject_t item = items[i];
                        ok = object_add_array_value(array_obj, item);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                    set_sp(vm, vm->sp - count);
                    stack_push(vm, array_obj);
                }
                break;

            case OPCODE_MAP_START:
                {
                    uint16_t count = frame_read_uint16(vm->current_frame);
                    ApeObject_t map_obj = object_make_map_with_capacity(vm->mem, count);
                    if(object_is_null(map_obj))
                    {
                        goto err;
                    }
                    this_stack_push(vm, map_obj);
                }
                break;

            case OPCODE_MAP_END:
                {
                    uint16_t kvp_count = frame_read_uint16(vm->current_frame);
                    uint16_t items_count = kvp_count * 2;
                    ApeObject_t map_obj = this_stack_pop(vm);
                    ApeObject_t* kv_pairs = vm->stack + vm->sp - items_count;
                    for(int i = 0; i < items_count; i += 2)
                    {
                        ApeObject_t key = kv_pairs[i];
                        if(!object_is_hashable(key))
                        {
                            ApeObjectType_t key_type = object_get_type(key);
                            const char* key_type_name = object_get_type_name(key_type);
                            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "Key of type %s is not hashable", key_type_name);
                            goto err;
                        }
                        ApeObject_t val = kv_pairs[i + 1];
                        ok = object_set_map_value_object(map_obj, key, val);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                    set_sp(vm, vm->sp - items_count);
                    stack_push(vm, map_obj);
                }
                break;

            case OPCODE_GET_THIS:
                {
                    ApeObject_t obj = this_stack_get(vm, 0);
                    stack_push(vm, obj);
                }
                break;

            case OPCODE_GET_INDEX:
                {
                    #if 0
                    const char* idxname;
                    #endif
                    ApeObject_t index = stack_pop(vm);
                    ApeObject_t left = stack_pop(vm);
                    ApeObjectType_t left_type = object_get_type(left);
                    ApeObjectType_t index_type = object_get_type(index);
                    const char* left_type_name = object_get_type_name(left_type);
                    const char* index_type_name = object_get_type_name(index_type);
                    /*
                    * todo: object method lookup could be implemented here
                    */
                    #if 0
                    {
                        int argc;
                        ApeObject_t args[10];
                        ApeNativeFNCallback_t afn;
                        argc = 0;
                        if(index_type == APE_OBJECT_STRING)
                        {
                            idxname = object_get_string(index);
                            fprintf(stderr, "index is a string: name=%s\n", idxname);
                            if((afn = builtin_get_object(left_type, idxname)) != NULL)
                            {
                                fprintf(stderr, "got a callback: afn=%p\n", afn);
                                //typedef ApeObject_t (*ApeNativeFNCallback_t)(ApeVM_t*, void*, int, ApeObject_t*);
                                args[argc] = left;
                                argc++;
                                stack_push(vm, afn(vm, NULL, argc, args));
                                break;
                            }
                        }
                    }
                    #endif

                    if(left_type != APE_OBJECT_ARRAY && left_type != APE_OBJECT_MAP && left_type != APE_OBJECT_STRING)
                    {
                        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                            "Type %s is not indexable (in OPCODE_GET_INDEX)", left_type_name);
                        goto err;
                    }
                    ApeObject_t res = object_make_null();

                    if(left_type == APE_OBJECT_ARRAY)
                    {
                        if(index_type != APE_OBJECT_NUMBER)
                        {
                            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "Cannot index %s with %s", left_type_name, index_type_name);
                            goto err;
                        }
                        int ix = (int)object_get_number(index);
                        if(ix < 0)
                        {
                            ix = object_get_array_length(left) + ix;
                        }
                        if(ix >= 0 && ix < object_get_array_length(left))
                        {
                            res = object_get_array_value(left, ix);
                        }
                    }
                    else if(left_type == APE_OBJECT_MAP)
                    {
                        res = object_get_map_value_object(left, index);
                    }
                    else if(left_type == APE_OBJECT_STRING)
                    {
                        const char* str = object_get_string(left);
                        int left_len = object_get_string_length(left);
                        int ix = (int)object_get_number(index);
                        if(ix >= 0 && ix < left_len)
                        {
                            char res_str[2] = { str[ix], '\0' };
                            res = object_make_string(vm->mem, res_str);
                        }
                    }
                    stack_push(vm, res);
                }
                break;

            case OPCODE_GET_VALUE_AT:
            {
                ApeObject_t index = stack_pop(vm);
                ApeObject_t left = stack_pop(vm);
                ApeObjectType_t left_type = object_get_type(left);
                ApeObjectType_t index_type = object_get_type(index);
                const char* left_type_name = object_get_type_name(left_type);
                const char* index_type_name = object_get_type_name(index_type);

                if(left_type != APE_OBJECT_ARRAY && left_type != APE_OBJECT_MAP && left_type != APE_OBJECT_STRING)
                {
                    errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                      "Type %s is not indexable (in OPCODE_GET_VALUE_AT)", left_type_name);
                    goto err;
                }

                ApeObject_t res = object_make_null();
                if(index_type != APE_OBJECT_NUMBER)
                {
                    errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                      "Cannot index %s with %s", left_type_name, index_type_name);
                    goto err;
                }
                int ix = (int)object_get_number(index);

                if(left_type == APE_OBJECT_ARRAY)
                {
                    res = object_get_array_value(left, ix);
                }
                else if(left_type == APE_OBJECT_MAP)
                {
                    res = object_get_kv_pair_at(vm->mem, left, ix);
                }
                else if(left_type == APE_OBJECT_STRING)
                {
                    const char* str = object_get_string(left);
                    int left_len = object_get_string_length(left);
                    int ix = (int)object_get_number(index);
                    if(ix >= 0 && ix < left_len)
                    {
                        char res_str[2] = { str[ix], '\0' };
                        res = object_make_string(vm->mem, res_str);
                    }
                }
                stack_push(vm, res);
                break;
            }
            case OPCODE_CALL:
            {
                uint8_t num_args = frame_read_uint8(vm->current_frame);
                ApeObject_t callee = stack_get(vm, num_args);
                bool ok = call_object(vm, callee, num_args);
                if(!ok)
                {
                    goto err;
                }
                break;
            }
            case OPCODE_RETURN_VALUE:
            {
                ApeObject_t res = stack_pop(vm);
                bool ok = pop_frame(vm);
                if(!ok)
                {
                    goto end;
                }
                stack_push(vm, res);
                break;
            }
            case OPCODE_RETURN:
            {
                bool ok = pop_frame(vm);
                stack_push(vm, object_make_null());
                if(!ok)
                {
                    stack_pop(vm);
                    goto end;
                }
                break;
            }
            case OPCODE_DEFINE_LOCAL:
            {
                uint8_t pos = frame_read_uint8(vm->current_frame);
                vm->stack[vm->current_frame->base_pointer + pos] = stack_pop(vm);
                break;
            }
            case OPCODE_SET_LOCAL:
            {
                uint8_t pos = frame_read_uint8(vm->current_frame);
                ApeObject_t new_value = stack_pop(vm);
                ApeObject_t old_value = vm->stack[vm->current_frame->base_pointer + pos];
                if(!check_assign(vm, old_value, new_value))
                {
                    goto err;
                }
                vm->stack[vm->current_frame->base_pointer + pos] = new_value;
                break;
            }
            case OPCODE_GET_LOCAL:
            {
                uint8_t pos = frame_read_uint8(vm->current_frame);
                ApeObject_t val = vm->stack[vm->current_frame->base_pointer + pos];
                stack_push(vm, val);
                break;
            }
            case OPCODE_GET_APE_GLOBAL:
            {
                uint16_t ix = frame_read_uint16(vm->current_frame);
                bool ok = false;
                ApeObject_t val = global_store_get_object_at(vm->global_store, ix, &ok);
                if(!ok)
                {
                    errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                      "Global value %d not found", ix);
                    goto err;
                }
                stack_push(vm, val);
                break;
            }
            case OPCODE_FUNCTION:
                {
                    uint16_t constant_ix = frame_read_uint16(vm->current_frame);
                    uint8_t num_free = frame_read_uint8(vm->current_frame);
                    ApeObject_t* constant = (ApeObject_t*)array_get(constants, constant_ix);
                    if(!constant)
                    {
                        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "Constant %d not found", constant_ix);
                        goto err;
                    }
                    ApeObjectType_t constant_type = object_get_type(*constant);
                    if(constant_type != APE_OBJECT_FUNCTION)
                    {
                        const char* type_name = object_get_type_name(constant_type);
                        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "%s is not a function", type_name);
                        goto err;
                    }

                    const ApeFunction_t* constant_function = object_get_function(*constant);
                    ApeObject_t function_obj
                    = object_make_function(vm->mem, object_get_function_name(*constant), constant_function->comp_result,
                                           false, constant_function->num_locals, constant_function->num_args, num_free);
                    if(object_is_null(function_obj))
                    {
                        goto err;
                    }
                    for(int i = 0; i < num_free; i++)
                    {
                        ApeObject_t free_val = vm->stack[vm->sp - num_free + i];
                        object_set_function_free_val(function_obj, i, free_val);
                    }
                    set_sp(vm, vm->sp - num_free);
                    stack_push(vm, function_obj);
                }
                break;
            case OPCODE_GET_FREE:
                {
                    uint8_t free_ix = frame_read_uint8(vm->current_frame);
                    ApeObject_t val = object_get_function_free_val(vm->current_frame->function, free_ix);
                    stack_push(vm, val);
                }
                break;
            case OPCODE_SET_FREE:
                {
                    uint8_t free_ix = frame_read_uint8(vm->current_frame);
                    ApeObject_t val = stack_pop(vm);
                    object_set_function_free_val(vm->current_frame->function, free_ix, val);
                }
                break;
            case OPCODE_CURRENT_FUNCTION:
                {
                    ApeObject_t current_function = vm->current_frame->function;
                    stack_push(vm, current_function);
                }
                break;
            case OPCODE_SET_INDEX:
                {
                    ApeObject_t index = stack_pop(vm);
                    ApeObject_t left = stack_pop(vm);
                    ApeObject_t new_value = stack_pop(vm);
                    ApeObjectType_t left_type = object_get_type(left);
                    ApeObjectType_t index_type = object_get_type(index);
                    const char* left_type_name = object_get_type_name(left_type);
                    const char* index_type_name = object_get_type_name(index_type);

                    if(left_type != APE_OBJECT_ARRAY && left_type != APE_OBJECT_MAP)
                    {
                        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "Type %s is not indexable (in OPCODE_SET_INDEX)", left_type_name);
                        goto err;
                    }

                    if(left_type == APE_OBJECT_ARRAY)
                    {
                        if(index_type != APE_OBJECT_NUMBER)
                        {
                            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "Cannot index %s with %s", left_type_name, index_type_name);
                            goto err;
                        }
                        int ix = (int)object_get_number(index);
                        ok = object_set_array_value_at(left, ix, new_value);
                        if(!ok)
                        {
                            errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                             "Setting array item failed (out of bounds?)");
                            goto err;
                        }
                    }
                    else if(left_type == APE_OBJECT_MAP)
                    {
                        ApeObject_t old_value = object_get_map_value_object(left, index);
                        if(!check_assign(vm, old_value, new_value))
                        {
                            goto err;
                        }
                        ok = object_set_map_value_object(left, index, new_value);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                }
                break;
            case OPCODE_DUP:
                {
                    ApeObject_t val = stack_get(vm, 0);
                    stack_push(vm, val);
                }
                break;
            case OPCODE_LEN:
                {
                    ApeObject_t val = stack_pop(vm);
                    int len = 0;
                    ApeObjectType_t type = object_get_type(val);
                    if(type == APE_OBJECT_ARRAY)
                    {
                        len = object_get_array_length(val);
                    }
                    else if(type == APE_OBJECT_MAP)
                    {
                        len = object_get_map_length(val);
                    }
                    else if(type == APE_OBJECT_STRING)
                    {
                        len = object_get_string_length(val);
                    }
                    else
                    {
                        const char* type_name = object_get_type_name(type);
                        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "Cannot get length of %s", type_name);
                        goto err;
                    }
                    stack_push(vm, object_make_number(len));
                }
                break;

            case OPCODE_NUMBER:
                {
                    uint64_t val = frame_read_uint64(vm->current_frame);
                    double val_double = ape_uint64_to_double(val);
                    ApeObject_t obj = object_make_number(val_double);
                    stack_push(vm, obj);
                }
                break;
            case OPCODE_SET_RECOVER:
                {
                    uint16_t recover_ip = frame_read_uint16(vm->current_frame);
                    vm->current_frame->recover_ip = recover_ip;
                }
                break;
            default:
                {
                    APE_ASSERT(false);
                    errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "Unknown opcode: 0x%x", opcode);
                    goto err;
                }
                break;
        }
        if(check_time)
        {
            time_check_counter++;
            if(time_check_counter > time_check_interval)
            {
                int elapsed_ms = (int)ape_timer_get_elapsed_ms(&timer);
                if(elapsed_ms > max_exec_time_ms)
                {
                    errors_add_errorf(vm->errors, APE_ERROR_TIMEOUT, frame_src_position(vm->current_frame),
                                      "Execution took more than %1.17g ms", max_exec_time_ms);
                    goto err;
                }
                time_check_counter = 0;
            }
        }
    err:
        if(errors_get_count(vm->errors) > 0)
        {
            ApeError_t* err = errors_get_last_error(vm->errors);
            if(err->type == APE_ERROR_RUNTIME && errors_get_count(vm->errors) == 1)
            {
                int recover_frame_ix = -1;
                for(int i = vm->frames_count - 1; i >= 0; i--)
                {
                    ApeFrame_t* frame = &vm->frames[i];
                    if(frame->recover_ip >= 0 && !frame->is_recovering)
                    {
                        recover_frame_ix = i;
                        break;
                    }
                }
                if(recover_frame_ix < 0)
                {
                    goto end;
                }
                else
                {
                    if(!err->traceback)
                    {
                        err->traceback = traceback_make(vm->alloc);
                    }
                    if(err->traceback)
                    {
                        traceback_append_from_vm(err->traceback, vm);
                    }
                    while(vm->frames_count > (recover_frame_ix + 1))
                    {
                        pop_frame(vm);
                    }
                    ApeObject_t err_obj = object_make_error(vm->mem, err->message);
                    if(!object_is_null(err_obj))
                    {
                        object_set_error_traceback(err_obj, err->traceback);
                        err->traceback = NULL;
                    }
                    stack_push(vm, err_obj);
                    vm->current_frame->ip = vm->current_frame->recover_ip;
                    vm->current_frame->is_recovering = true;
                    errors_clear(vm->errors);
                }
            }
            else
            {
                goto end;
            }
        }
        if(gc_should_sweep(vm->mem))
        {
            run_gc(vm, constants);
        }
    }

end:
    if(errors_get_count(vm->errors) > 0)
    {
        ApeError_t* err = errors_get_last_error(vm->errors);
        if(!err->traceback)
        {
            err->traceback = traceback_make(vm->alloc);
        }
        if(err->traceback)
        {
            traceback_append_from_vm(err->traceback, vm);
        }
    }

    run_gc(vm, constants);

    vm->running = false;
    return errors_get_count(vm->errors) == 0;
}

ApeObject_t vm_get_last_popped(ApeVM_t* vm)
{
    return vm->last_popped;
}

bool vm_has_errors(ApeVM_t* vm)
{
    return errors_get_count(vm->errors) > 0;
}

bool vm_set_global(ApeVM_t* vm, int ix, ApeObject_t val)
{
    if(ix >= VM_MAX_GLOBALS)
    {
        APE_ASSERT(false);
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "Global write out of range");
        return false;
    }
    vm->globals[ix] = val;
    if(ix >= vm->globals_count)
    {
        vm->globals_count = ix + 1;
    }
    return true;
}

ApeObject_t vm_get_global(ApeVM_t* vm, int ix)
{
    if(ix >= VM_MAX_GLOBALS)
    {
        APE_ASSERT(false);
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "Global read out of range");
        return object_make_null();
    }
    return vm->globals[ix];
}

// INTERNAL
void set_sp(ApeVM_t* vm, int new_sp)
{
    if(new_sp > vm->sp)
    {// to avoid gcing freed objects
        int count = new_sp - vm->sp;
        size_t bytes_count = count * sizeof(ApeObject_t);
        memset(vm->stack + vm->sp, 0, bytes_count);
    }
    vm->sp = new_sp;
}

void stack_push(ApeVM_t* vm, ApeObject_t obj)
{
#ifdef APE_DEBUG
    if(vm->sp >= VM_STACK_SIZE)
    {
        APE_ASSERT(false);
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "Stack overflow");
        return;
    }
    if(vm->current_frame)
    {
        ApeFrame_t* frame = vm->current_frame;
        ApeFunction_t* current_function = object_get_function(frame->function);
        int num_locals = current_function->num_locals;
        APE_ASSERT(vm->sp >= (frame->base_pointer + num_locals));
    }
#endif
    vm->stack[vm->sp] = obj;
    vm->sp++;
}

ApeObject_t stack_pop(ApeVM_t* vm)
{
#ifdef APE_DEBUG
    if(vm->sp == 0)
    {
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "Stack underflow");
        APE_ASSERT(false);
        return object_make_null();
    }
    if(vm->current_frame)
    {
        ApeFrame_t* frame = vm->current_frame;
        ApeFunction_t* current_function = object_get_function(frame->function);
        int num_locals = current_function->num_locals;
        APE_ASSERT((vm->sp - 1) >= (frame->base_pointer + num_locals));
    }
#endif
    vm->sp--;
    ApeObject_t res = vm->stack[vm->sp];
    vm->last_popped = res;
    return res;
}

ApeObject_t stack_get(ApeVM_t* vm, int nth_item)
{
    int ix = vm->sp - 1 - nth_item;
#ifdef APE_DEBUG
    if(ix < 0 || ix >= VM_STACK_SIZE)
    {
        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "Invalid stack index: %d", nth_item);
        APE_ASSERT(false);
        return object_make_null();
    }
#endif
    return vm->stack[ix];
}

void this_stack_push(ApeVM_t* vm, ApeObject_t obj)
{
#ifdef APE_DEBUG
    if(vm->this_sp >= VM_THIS_STACK_SIZE)
    {
        APE_ASSERT(false);
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "this stack overflow");
        return;
    }
#endif
    vm->this_stack[vm->this_sp] = obj;
    vm->this_sp++;
}

ApeObject_t this_stack_pop(ApeVM_t* vm)
{
#ifdef APE_DEBUG
    if(vm->this_sp == 0)
    {
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "this stack underflow");
        APE_ASSERT(false);
        return object_make_null();
    }
#endif
    vm->this_sp--;
    return vm->this_stack[vm->this_sp];
}

ApeObject_t this_stack_get(ApeVM_t* vm, int nth_item)
{
    int ix = vm->this_sp - 1 - nth_item;
#ifdef APE_DEBUG
    if(ix < 0 || ix >= VM_THIS_STACK_SIZE)
    {
        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "Invalid this stack index: %d", nth_item);
        APE_ASSERT(false);
        return object_make_null();
    }
#endif
    return vm->this_stack[ix];
}

bool push_frame(ApeVM_t* vm, ApeFrame_t frame)
{
    if(vm->frames_count >= VM_MAX_FRAMES)
    {
        APE_ASSERT(false);
        return false;
    }
    vm->frames[vm->frames_count] = frame;
    vm->current_frame = &vm->frames[vm->frames_count];
    vm->frames_count++;
    ApeFunction_t* frame_function = object_get_function(frame.function);
    set_sp(vm, frame.base_pointer + frame_function->num_locals);
    return true;
}

bool pop_frame(ApeVM_t* vm)
{
    set_sp(vm, vm->current_frame->base_pointer - 1);
    if(vm->frames_count <= 0)
    {
        APE_ASSERT(false);
        vm->current_frame = NULL;
        return false;
    }
    vm->frames_count--;
    if(vm->frames_count == 0)
    {
        vm->current_frame = NULL;
        return false;
    }
    vm->current_frame = &vm->frames[vm->frames_count - 1];
    return true;
}

void run_gc(ApeVM_t* vm, ApeArray_t * constants)
{
    gc_unmark_all(vm->mem);
    gc_mark_objects(global_store_get_object_data(vm->global_store), global_store_get_object_count(vm->global_store));
    gc_mark_objects((ApeObject_t*)array_data(constants), array_count(constants));
    gc_mark_objects(vm->globals, vm->globals_count);
    for(int i = 0; i < vm->frames_count; i++)
    {
        ApeFrame_t* frame = &vm->frames[i];
        gc_mark_object(frame->function);
    }
    gc_mark_objects(vm->stack, vm->sp);
    gc_mark_objects(vm->this_stack, vm->this_sp);
    gc_mark_object(vm->last_popped);
    gc_mark_objects(vm->operator_oveload_keys, OPCODE_MAX);
    gc_sweep(vm->mem);
}

bool call_object(ApeVM_t* vm, ApeObject_t callee, int num_args)
{
    ApeObjectType_t callee_type = object_get_type(callee);
    if(callee_type == APE_OBJECT_FUNCTION)
    {
        ApeFunction_t* callee_function = object_get_function(callee);
        if(num_args != callee_function->num_args)
        {
            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                              "Invalid number of arguments to \"%s\", expected %d, got %d",
                              object_get_function_name(callee), callee_function->num_args, num_args);
            return false;
        }
        ApeFrame_t callee_frame;
        bool ok = frame_init(&callee_frame, callee, vm->sp - num_args);
        if(!ok)
        {
            errors_add_error(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "Frame init failed in call_object");
            return false;
        }
        ok = push_frame(vm, callee_frame);
        if(!ok)
        {
            errors_add_error(vm->errors, APE_ERROR_RUNTIME, src_pos_invalid, "Pushing frame failed in call_object");
            return false;
        }
    }
    else if(callee_type == APE_OBJECT_NATIVE_FUNCTION)
    {
        ApeObject_t* stack_pos = vm->stack + vm->sp - num_args;
        ApeObject_t res = call_native_function(vm, callee, frame_src_position(vm->current_frame), num_args, stack_pos);
        if(vm_has_errors(vm))
        {
            return false;
        }
        set_sp(vm, vm->sp - num_args - 1);
        stack_push(vm, res);
    }
    else
    {
        const char* callee_type_name = object_get_type_name(callee_type);
        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "%s object is not callable", callee_type_name);
        return false;
    }
    return true;
}

ApeObject_t call_native_function(ApeVM_t* vm, ApeObject_t callee, ApePosition_t src_pos, int argc, ApeObject_t* args)
{
    ApeNativeFunction_t* native_fun = object_get_native_function(callee);
    ApeObject_t res = native_fun->native_funcptr(vm, native_fun->data, argc, args);
    if(errors_has_errors(vm->errors) && !APE_STREQ(native_fun->name, "crash"))
    {
        ApeError_t* err = errors_get_last_error(vm->errors);
        err->pos = src_pos;
        err->traceback = traceback_make(vm->alloc);
        if(err->traceback)
        {
            traceback_append(err->traceback, native_fun->name, src_pos_invalid);
        }
        return object_make_null();
    }
    ApeObjectType_t res_type = object_get_type(res);
    if(res_type == APE_OBJECT_ERROR)
    {
        ApeTraceback_t* traceback = traceback_make(vm->alloc);
        if(traceback)
        {
            // error builtin is treated in a special way
            if(!APE_STREQ(native_fun->name, "error"))
            {
                traceback_append(traceback, native_fun->name, src_pos_invalid);
            }
            traceback_append_from_vm(traceback, vm);
            object_set_error_traceback(res, traceback);
        }
    }
    return res;
}

bool check_assign(ApeVM_t* vm, ApeObject_t old_value, ApeObject_t new_value)
{
    ApeObjectType_t old_value_type;
    ApeObjectType_t new_value_type;
    (void)vm;
    old_value_type = object_get_type(old_value);
    new_value_type = object_get_type(new_value);
    if(old_value_type == APE_OBJECT_NULL || new_value_type == APE_OBJECT_NULL)
    {
        return true;
    }
    if(old_value_type != new_value_type)
    {
        /*
        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "Trying to assign variable of type %s to %s",
                          object_get_type_name(new_value_type), object_get_type_name(old_value_type));
        return false;
        */
    }
    return true;
}

bool try_overload_operator(ApeVM_t* vm, ApeObject_t left, ApeObject_t right, opcode_t op, bool* out_overload_found)
{
    int num_operands;
    ApeObject_t key;
    ApeObject_t callee;
    ApeObjectType_t left_type;
    ApeObjectType_t right_type;
    *out_overload_found = false;
    left_type = object_get_type(left);
    right_type = object_get_type(right);
    if(left_type != APE_OBJECT_MAP && right_type != APE_OBJECT_MAP)
    {
        *out_overload_found = false;
        return true;
    }
    num_operands = 2;
    if(op == OPCODE_MINUS || op == OPCODE_BANG)
    {
        num_operands = 1;
    }
    key = vm->operator_oveload_keys[op];
    callee = object_make_null();
    if(left_type == APE_OBJECT_MAP)
    {
        callee = object_get_map_value_object(left, key);
    }
    if(!object_is_callable(callee))
    {
        if(right_type == APE_OBJECT_MAP)
        {
            callee = object_get_map_value_object(right, key);
        }
        if(!object_is_callable(callee))
        {
            *out_overload_found = false;
            return true;
        }
    }
    *out_overload_found = true;
    stack_push(vm, callee);
    stack_push(vm, left);
    if(num_operands == 2)
    {
        stack_push(vm, right);
    }
    return call_object(vm, callee, num_operands);
}

//-----------------------------------------------------------------------------
// Ape
//-----------------------------------------------------------------------------
ApeContext_t* ape_make(void)
{
    return ape_make_ex(NULL, NULL, NULL);
}

ApeContext_t* ape_make_ex(ApeMallocFNCallback_t malloc_fn, ApeFreeFNCallback_t free_fn, void* ctx)
{
    ApeAllocator_t custom_alloc = allocator_make((ApeAllocatorMallocFNCallback_t)malloc_fn, (ApeAllocatorFreeFNCallback_t)free_fn, ctx);

    ApeContext_t* ape = (ApeContext_t*)allocator_malloc(&custom_alloc, sizeof(ApeContext_t));
    if(!ape)
    {
        return NULL;
    }

    memset(ape, 0, sizeof(ApeContext_t));
    ape->alloc = allocator_make(ape_malloc, ape_free, ape);
    ape->custom_allocator = custom_alloc;

    set_default_config(ape);

    errors_init(&ape->errors);

    ape->mem = gcmem_make(&ape->alloc);
    if(!ape->mem)
    {
        goto err;
    }

    ape->files = ptrarray_make(&ape->alloc);
    if(!ape->files)
    {
        goto err;
    }

    ape->global_store = global_store_make(&ape->alloc, ape->mem);
    if(!ape->global_store)
    {
        goto err;
    }

    ape->compiler = compiler_make(&ape->alloc, &ape->config, ape->mem, &ape->errors, ape->files, ape->global_store);
    if(!ape->compiler)
    {
        goto err;
    }

    ape->vm = vm_make(&ape->alloc, &ape->config, ape->mem, &ape->errors, ape->global_store);
    if(!ape->vm)
    {
        goto err;
    }
    builtins_install(ape->vm);
    return ape;
err:
    ape_deinit(ape);
    allocator_free(&custom_alloc, ape);
    return NULL;
}

void ape_destroy(ApeContext_t* ape)
{
    if(!ape)
    {
        return;
    }
    ape_deinit(ape);
    ApeAllocator_t alloc = ape->alloc;
    allocator_free(&alloc, ape);
}

void ape_free_allocated(ApeContext_t* ape, void* ptr)
{
    allocator_free(&ape->alloc, ptr);
}

void ape_set_repl_mode(ApeContext_t* ape, bool enabled)
{
    ape->config.repl_mode = enabled;
}

bool ape_set_timeout(ApeContext_t* ape, double max_execution_time_ms)
{
    if(!ape_timer_platform_supported())
    {
        ape->config.max_execution_time_ms = 0;
        ape->config.max_execution_time_set = false;
        return false;
    }

    if(max_execution_time_ms >= 0)
    {
        ape->config.max_execution_time_ms = max_execution_time_ms;
        ape->config.max_execution_time_set = true;
    }
    else
    {
        ape->config.max_execution_time_ms = 0;
        ape->config.max_execution_time_set = false;
    }
    return true;
}

void ape_set_stdout_write_function(ApeContext_t* ape, ApeStdoutWriteFNCallback_t stdout_write, void* context)
{
    ape->config.stdio.write.write = stdout_write;
    ape->config.stdio.write.context = context;
}

void ape_set_file_write_function(ApeContext_t* ape, ApeWriteFileFNCallback_t file_write, void* context)
{
    ape->config.fileio.write_file.write_file = file_write;
    ape->config.fileio.write_file.context = context;
}

void ape_set_file_read_function(ApeContext_t* ape, ApeReadFileFNCallback_t file_read, void* context)
{
    ape->config.fileio.read_file.read_file = file_read;
    ape->config.fileio.read_file.context = context;
}




void ape_program_destroy(ApeProgram_t* program)
{
    if(!program)
    {
        return;
    }
    compilation_result_destroy(program->comp_res);
    allocator_free(&program->ape->alloc, program);
}

ApeObject_t ape_execute(ApeContext_t* ape, const char* code)
{
    bool ok;
    ApeObject_t res;
    ApeCompilationResult_t* comp_res;
    reset_state(ape);
    comp_res = compiler_compile(ape->compiler, code);
    if(!comp_res || errors_get_count(&ape->errors) > 0)
    {
        goto err;
    }
    ok = vm_run(ape->vm, comp_res, compiler_get_constants(ape->compiler));
    if(!ok || errors_get_count(&ape->errors) > 0)
    {
        goto err;
    }
    APE_ASSERT(ape->vm->sp == 0);
    res = vm_get_last_popped(ape->vm);
    if(object_get_type(res) == APE_OBJECT_NONE)
    {
        goto err;
    }
    compilation_result_destroy(comp_res);
    return res;

err:
    compilation_result_destroy(comp_res);
    return object_make_null();
}

ApeObject_t ape_execute_file(ApeContext_t* ape, const char* path)
{
    bool ok;
    ApeObject_t res;
    ApeCompilationResult_t* comp_res;
    reset_state(ape);
    comp_res = compiler_compile_file(ape->compiler, path);
    if(!comp_res || errors_get_count(&ape->errors) > 0)
    {
        goto err;
    }
    ok = vm_run(ape->vm, comp_res, compiler_get_constants(ape->compiler));
    if(!ok || errors_get_count(&ape->errors) > 0)
    {
        goto err;
    }
    APE_ASSERT(ape->vm->sp == 0);
    res = vm_get_last_popped(ape->vm);
    if(object_get_type(res) == APE_OBJECT_NONE)
    {
        goto err;
    }
    compilation_result_destroy(comp_res);

    return res;

err:
    compilation_result_destroy(comp_res);
    return object_make_null();
}



bool ape_has_errors(const ApeContext_t* ape)
{
    return ape_errors_count(ape) > 0;
}

int ape_errors_count(const ApeContext_t* ape)
{
    return errors_get_count(&ape->errors);
}

void ape_clear_errors(ApeContext_t* ape)
{
    errors_clear(&ape->errors);
}

const ApeError_t* ape_get_error(const ApeContext_t* ape, int index)
{
    return (const ApeError_t*)errors_getc(&ape->errors, index);
}

bool ape_set_native_function(ApeContext_t* ape, const char* name, ApeUserFNCallback_t fn, void* data)
{
    ApeObject_t obj = ape_object_make_native_function_with_name(ape, name, fn, data);
    if(object_is_null(obj))
    {
        return false;
    }
    return ape_set_global_constant(ape, name, obj);
}

bool ape_set_global_constant(ApeContext_t* ape, const char* name, ApeObject_t obj)
{
    return global_store_set(ape->global_store, name, obj);
}

ApeObject_t ape_get_object(ApeContext_t* ape, const char* name)
{
    ApeSymbolTable_t* st = compiler_get_symbol_table(ape->compiler);
    const ApeSymbol_t* symbol = symbol_table_resolve(st, name);
    if(!symbol)
    {
        errors_add_errorf(&ape->errors, APE_ERROR_USER, src_pos_invalid, "Symbol \"%s\" is not defined", name);
        return object_make_null();
    }
    ApeObject_t res = object_make_null();
    if(symbol->type == SYMBOL_MODULE_GLOBAL)
    {
        res = vm_get_global(ape->vm, symbol->index);
    }
    else if(symbol->type == SYMBOL_APE_GLOBAL)
    {
        bool ok = false;
        res = global_store_get_object_at(ape->global_store, symbol->index, &ok);
        if(!ok)
        {
            errors_add_errorf(&ape->errors, APE_ERROR_USER, src_pos_invalid, "Failed to get global object at %d", symbol->index);
            return object_make_null();
        }
    }
    else
    {
        errors_add_errorf(&ape->errors, APE_ERROR_USER, src_pos_invalid, "Value associated with symbol \"%s\" could not be loaded", name);
        return object_make_null();
    }
    return res;
}

ApeObject_t ape_object_make_native_function(ApeContext_t* ape, ApeUserFNCallback_t fn, void* data)
{
    return ape_object_make_native_function_with_name(ape, "", fn, data);
}



bool ape_object_disable_gc(ApeObject_t ape_obj)
{
    ApeObject_t obj = ape_obj;
    return gc_disable_on_object(obj);
}

void ape_object_enable_gc(ApeObject_t ape_obj)
{
    ApeObject_t obj = ape_obj;
    gc_enable_on_object(obj);
}

void ape_set_runtime_error(ApeContext_t* ape, const char* message)
{
    errors_add_error(&ape->errors, APE_ERROR_RUNTIME, src_pos_invalid, message);
}

void ape_set_runtime_errorf(ApeContext_t* ape, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int to_write = vsnprintf(NULL, 0, fmt, args);
    (void)to_write;
    va_end(args);
    va_start(args, fmt);
    char res[APE_ERROR_MESSAGE_MAX_LENGTH];
    int written = vsnprintf(res, APE_ERROR_MESSAGE_MAX_LENGTH, fmt, args);
    (void)written;
    APE_ASSERT(to_write == written);
    va_end(args);
    errors_add_error(&ape->errors, APE_ERROR_RUNTIME, src_pos_invalid, res);
}

//-----------------------------------------------------------------------------
// Ape object array
//-----------------------------------------------------------------------------


const char* ape_object_get_array_string(ApeObject_t obj, int ix)
{
    ApeObject_t object = object_get_array_value(obj, ix);
    if(object_get_type(object) != APE_OBJECT_STRING)
    {
        return NULL;
    }
    return object_get_string(object);
}

double ape_object_get_array_number(ApeObject_t obj, int ix)
{
    ApeObject_t object = object_get_array_value(obj, ix);
    if(object_get_type(object) != APE_OBJECT_NUMBER)
    {
        return 0;
    }
    return object_get_number(object);
}

bool ape_object_get_array_bool(ApeObject_t obj, int ix)
{
    ApeObject_t object = object_get_array_value(obj, ix);
    if(object_get_type(object) != APE_OBJECT_BOOL)
    {
        return 0;
    }
    return object_get_bool(object);
}

bool ape_object_set_array_value(ApeObject_t ape_obj, int ix, ApeObject_t ape_value)
{
    ApeObject_t obj = ape_obj;
    ApeObject_t value = ape_value;
    return object_set_array_value_at(obj, ix, value);
}

bool ape_object_set_array_string(ApeObject_t obj, int ix, const char* string)
{
    ApeGCMemory_t* mem = object_get_mem(obj);
    if(!mem)
    {
        return false;
    }
    ApeObject_t new_value = object_make_string(mem, string);
    return ape_object_set_array_value(obj, ix, new_value);
}

bool ape_object_set_array_number(ApeObject_t obj, int ix, double number)
{
    ApeObject_t new_value = object_make_number(number);
    return ape_object_set_array_value(obj, ix, new_value);
}

bool ape_object_set_array_bool(ApeObject_t obj, int ix, bool value)
{
    ApeObject_t new_value = object_make_bool(value);
    return ape_object_set_array_value(obj, ix, new_value);
}


bool ape_object_add_array_string(ApeObject_t obj, const char* string)
{
    ApeGCMemory_t* mem = object_get_mem(obj);
    if(!mem)
    {
        return false;
    }
    ApeObject_t new_value = object_make_string(mem, string);
    return object_add_array_value(obj, new_value);
}

bool ape_object_add_array_number(ApeObject_t obj, double number)
{
    ApeObject_t new_value = object_make_number(number);
    return object_add_array_value(obj, new_value);
}

bool ape_object_add_array_bool(ApeObject_t obj, bool value)
{
    ApeObject_t new_value = object_make_bool(value);
    return object_add_array_value(obj, new_value);
}

//-----------------------------------------------------------------------------
// Ape object map
//-----------------------------------------------------------------------------


bool ape_object_set_map_value(ApeObject_t obj, const char* key, ApeObject_t value)
{
    ApeGCMemory_t* mem = object_get_mem(obj);
    if(!mem)
    {
        return false;
    }
    ApeObject_t key_object = object_make_string(mem, key);
    if(object_is_null(key_object))
    {
        return false;
    }
    return object_set_map_value_object(obj, key_object, value);
}

bool ape_object_set_map_string(ApeObject_t obj, const char* key, const char* string)
{
    ApeGCMemory_t* mem = object_get_mem(obj);
    if(!mem)
    {
        return false;
    }
    ApeObject_t string_object = object_make_string(mem, string);
    if(object_is_null(string_object))
    {
        return false;
    }
    return ape_object_set_map_value(obj, key, string_object);
}

bool ape_object_set_map_number(ApeObject_t obj, const char* key, double number)
{
    ApeObject_t number_object = object_make_number(number);
    return ape_object_set_map_value(obj, key, number_object);
}

bool ape_object_set_map_bool(ApeObject_t obj, const char* key, bool value)
{
    ApeObject_t bool_object = object_make_bool(value);
    return ape_object_set_map_value(obj, key, bool_object);
}


ApeObject_t ape_object_get_map_value(ApeObject_t object, const char* key)
{
    ApeGCMemory_t* mem = object_get_mem(object);
    if(!mem)
    {
        return object_make_null();
    }
    ApeObject_t key_object = object_make_string(mem, key);
    if(object_is_null(key_object))
    {
        return object_make_null();
    }
    ApeObject_t res = object_get_map_value_object(object, key_object);
    return res;
}

const char* ape_object_get_map_string(ApeObject_t object, const char* key)
{
    ApeObject_t res = ape_object_get_map_value(object, key);
    return object_get_string(res);
}

double ape_object_get_map_number(ApeObject_t object, const char* key)
{
    ApeObject_t res = ape_object_get_map_value(object, key);
    return object_get_number(res);
}

bool ape_object_get_map_bool(ApeObject_t object, const char* key)
{
    ApeObject_t res = ape_object_get_map_value(object, key);
    return object_get_bool(res);
}

bool ape_object_map_has_key(ApeObject_t ape_object, const char* key)
{
    ApeObject_t object = ape_object;
    ApeGCMemory_t* mem = object_get_mem(object);
    if(!mem)
    {
        return false;
    }
    ApeObject_t key_object = object_make_string(mem, key);
    if(object_is_null(key_object))
    {
        return false;
    }
    return object_map_has_key_object(object, key_object);
}

//-----------------------------------------------------------------------------
// Ape error
//-----------------------------------------------------------------------------

const char* ape_error_get_message(const ApeError_t* ae)
{
    const ApeError_t* error = (const ApeError_t*)ae;
    return error->message;
}

const char* ape_error_get_filepath(const ApeError_t* ae)
{
    const ApeError_t* error = (const ApeError_t*)ae;
    if(!error->pos.file)
    {
        return NULL;
    }
    return error->pos.file->path;
}

const char* ape_error_get_line(const ApeError_t* ae)
{
    const ApeError_t* error = (const ApeError_t*)ae;
    if(!error->pos.file)
    {
        return NULL;
    }
    ApePtrArray_t* lines = error->pos.file->lines;
    if(error->pos.line >= ptrarray_count(lines))
    {
        return NULL;
    }
    const char* line = (const char*)ptrarray_get(lines, error->pos.line);
    return line;
}

int ape_error_get_line_number(const ApeError_t* ae)
{
    const ApeError_t* error = (const ApeError_t*)ae;
    if(error->pos.line < 0)
    {
        return -1;
    }
    return error->pos.line + 1;
}

int ape_error_get_column_number(const ApeError_t* ae)
{
    const ApeError_t* error = (const ApeError_t*)ae;
    if(error->pos.column < 0)
    {
        return -1;
    }
    return error->pos.column + 1;
}

ApeErrorType_t ape_error_get_type(const ApeError_t* ae)
{
    const ApeError_t* error = (const ApeError_t*)ae;
    switch(error->type)
    {
        case APE_ERROR_NONE:
            return APE_ERROR_NONE;
        case APE_ERROR_PARSING:
            return APE_ERROR_PARSING;
        case APE_ERROR_COMPILATION:
            return APE_ERROR_COMPILATION;
        case APE_ERROR_RUNTIME:
            return APE_ERROR_RUNTIME;
        case APE_ERROR_TIMEOUT:
            return APE_ERROR_TIMEOUT;
        case APE_ERROR_ALLOCATION:
            return APE_ERROR_ALLOCATION;
        case APE_ERROR_USER:
            return APE_ERROR_USER;
        default:
            return APE_ERROR_NONE;
    }
}

const char* ape_error_get_type_string(const ApeError_t* error)
{
    return ape_error_type_to_string(ape_error_get_type(error));
}


char* ape_error_serialize(ApeContext_t* ape, const ApeError_t* err)
{
    const char* type_str = ape_error_get_type_string(err);
    const char* filename = ape_error_get_filepath(err);
    const char* line = ape_error_get_line(err);
    int line_num = ape_error_get_line_number(err);
    int col_num = ape_error_get_column_number(err);
    ApeStringBuffer_t* buf = strbuf_make(&ape->alloc);
    if(!buf)
    {
        return NULL;
    }
    if(line)
    {
        strbuf_append(buf, line);
        strbuf_append(buf, "\n");
        if(col_num >= 0)
        {
            for(int j = 0; j < (col_num - 1); j++)
            {
                strbuf_append(buf, " ");
            }
            strbuf_append(buf, "^\n");
        }
    }
    strbuf_appendf(buf, "%s ERROR in \"%s\" on %d:%d: %s\n", type_str, filename, line_num, col_num, ape_error_get_message(err));
    const ApeTraceback_t* traceback = ape_error_get_traceback(err);
    if(traceback)
    {
        strbuf_appendf(buf, "Traceback:\n");
        traceback_to_string((const ApeTraceback_t*)ape_error_get_traceback(err), buf);
    }
    if(strbuf_failed(buf))
    {
        strbuf_destroy(buf);
        return NULL;
    }
    return strbuf_get_string_and_destroy(buf);
}

const ApeTraceback_t* ape_error_get_traceback(const ApeError_t* ae)
{
    const ApeError_t* error = (const ApeError_t*)ae;
    return (const ApeTraceback_t*)error->traceback;
}

//-----------------------------------------------------------------------------
// Ape traceback
//-----------------------------------------------------------------------------

int ape_traceback_get_depth(const ApeTraceback_t* ape_traceback)
{
    const ApeTraceback_t* traceback = (const ApeTraceback_t*)ape_traceback;
    return array_count(traceback->items);
}

const char* ape_traceback_get_filepath(const ApeTraceback_t* ape_traceback, int depth)
{
    const ApeTraceback_t* traceback = (const ApeTraceback_t*)ape_traceback;
    ApeTracebackItem_t* item = (ApeTracebackItem_t*)array_get(traceback->items, depth);
    if(!item)
    {
        return NULL;
    }
    return traceback_item_get_filepath(item);
}

const char* ape_traceback_get_line(const ApeTraceback_t* ape_traceback, int depth)
{
    const ApeTraceback_t* traceback = (const ApeTraceback_t*)ape_traceback;
    ApeTracebackItem_t* item = (ApeTracebackItem_t*)array_get(traceback->items, depth);
    if(!item)
    {
        return NULL;
    }
    return traceback_item_get_line(item);
}

int ape_traceback_get_line_number(const ApeTraceback_t* ape_traceback, int depth)
{
    const ApeTraceback_t* traceback = (const ApeTraceback_t*)ape_traceback;
    ApeTracebackItem_t* item = (ApeTracebackItem_t*)array_get(traceback->items, depth);
    if(!item)
    {
        return -1;
    }
    return item->pos.line;
}

int ape_traceback_get_column_number(const ApeTraceback_t* ape_traceback, int depth)
{
    const ApeTraceback_t* traceback = (const ApeTraceback_t*)ape_traceback;
    ApeTracebackItem_t* item = (ApeTracebackItem_t*)array_get(traceback->items, depth);
    if(!item)
    {
        return -1;
    }
    return item->pos.column;
}

const char* ape_traceback_get_function_name(const ApeTraceback_t* ape_traceback, int depth)
{
    const ApeTraceback_t* traceback = (const ApeTraceback_t*)ape_traceback;
    ApeTracebackItem_t* item = (ApeTracebackItem_t*)array_get(traceback->items, depth);
    if(!item)
    {
        return "";
    }
    return item->function_name;
}

//-----------------------------------------------------------------------------
// Ape internal
//-----------------------------------------------------------------------------
void ape_deinit(ApeContext_t* ape)
{
    vm_destroy(ape->vm);
    compiler_destroy(ape->compiler);
    global_store_destroy(ape->global_store);
    gcmem_destroy(ape->mem);
    ptrarray_destroy_with_items(ape->files, compiled_file_destroy);
    errors_deinit(&ape->errors);
}

ApeObject_t ape_native_fn_wrapper(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    (void)vm;
    ApeNativeFuncWrapper_t* wrapper = (ApeNativeFuncWrapper_t*)data;
    APE_ASSERT(vm == wrapper->ape->vm);
    ApeObject_t res = wrapper->wrapped_funcptr(wrapper->ape, wrapper->data, argc, (ApeObject_t*)args);
    if(ape_has_errors(wrapper->ape))
    {
        return object_make_null();
    }
    return res;
}

ApeObject_t ape_object_make_native_function_with_name(ApeContext_t* ape, const char* name, ApeUserFNCallback_t fn, void* data)
{
    ApeNativeFuncWrapper_t wrapper;
    memset(&wrapper, 0, sizeof(ApeNativeFuncWrapper_t));
    wrapper.wrapped_funcptr = fn;
    wrapper.ape = ape;
    wrapper.data = data;
    ApeObject_t wrapper_native_function = object_make_native_function_memory(ape->mem, name, ape_native_fn_wrapper, &wrapper, sizeof(wrapper));
    if(object_is_null(wrapper_native_function))
    {
        return object_make_null();
    }
    return wrapper_native_function;
}

void reset_state(ApeContext_t* ape)
{
    ape_clear_errors(ape);
    vm_reset(ape->vm);
}

void set_default_config(ApeContext_t* ape)
{
    memset(&ape->config, 0, sizeof(ApeConfig_t));
    ape_set_repl_mode(ape, false);
    ape_set_timeout(ape, -1);
    ape_set_file_read_function(ape, read_file_default, ape);
    ape_set_file_write_function(ape, write_file_default, ape);
    ape_set_stdout_write_function(ape, stdout_write_default, ape);
}

char* read_file_default(void* ctx, const char* filename)
{
    ApeContext_t* ape = (ApeContext_t*)ctx;
    FILE* fp = fopen(filename, "r");
    size_t size_to_read = 0;
    size_t size_read = 0;
    long pos;
    char* file_contents;
    if(!fp)
    {
        return NULL;
    }
    fseek(fp, 0L, SEEK_END);
    pos = ftell(fp);
    if(pos < 0)
    {
        fclose(fp);
        return NULL;
    }
    size_to_read = pos;
    rewind(fp);
    file_contents = (char*)allocator_malloc(&ape->alloc, sizeof(char) * (size_to_read + 1));
    if(!file_contents)
    {
        fclose(fp);
        return NULL;
    }
    size_read = fread(file_contents, 1, size_to_read, fp);
    if(ferror(fp))
    {
        fclose(fp);
        free(file_contents);
        return NULL;
    }
    fclose(fp);
    file_contents[size_read] = '\0';
    return file_contents;
}

size_t write_file_default(void* ctx, const char* path, const char* string, size_t string_size)
{
    (void)ctx;
    FILE* fp = fopen(path, "w");
    if(!fp)
    {
        return 0;
    }
    size_t written = fwrite(string, 1, string_size, fp);
    fclose(fp);
    return written;
}

size_t stdout_write_default(void* ctx, const void* data, size_t size)
{
    (void)ctx;
    return fwrite(data, 1, size, stdout);
}

void* ape_malloc(void* ctx, size_t size)
{
    ApeContext_t* ape = (ApeContext_t*)ctx;
    void* res = (void*)allocator_malloc(&ape->custom_allocator, size);
    if(!res)
    {
        errors_add_error(&ape->errors, APE_ERROR_ALLOCATION, src_pos_invalid, "Allocation failed");
    }
    return res;
}

void ape_free(void* ctx, void* ptr)
{
    ApeContext_t* ape = (ApeContext_t*)ctx;
    allocator_free(&ape->custom_allocator, ptr);
}
