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

#include "ape.h"

static const ApePosition_t g_vmpriv_src_pos_invalid = { NULL, -1, -1 };

static unsigned int vmpriv_upperpoweroftwo(unsigned int v);
static void vmpriv_initerrors(ApeErrorList_t *errors);
static void vmpriv_destroyerrors(ApeErrorList_t *errors);
static const ApeError_t *vmpriv_errorsgetc(const ApeErrorList_t *errors, int ix);
static bool vmpriv_setsymbol(ApeSymbolTable_t *table, ApeSymbol_t *symbol);
static int vmpriv_nextsymbolindex(ApeSymbolTable_t *table);
static int vmpriv_countnumdefs(ApeSymbolTable_t *table);
static uint64_t vmpriv_gettypetag(ApeObjectType_t type);
static bool vmpriv_freevalsallocated(ApeFunction_t *fun);
static void vmpriv_setstackpointer(ApeVM_t *vm, int new_sp);
static void vmpriv_pushthisstack(ApeVM_t *vm, ApeObject_t obj);
static ApeObject_t vmpriv_popthisstack(ApeVM_t *vm);
static ApeObject_t vmpriv_getthisstack(ApeVM_t *vm, int nth_item);
static bool vmpriv_tryoverloadoperator(ApeVM_t *vm, ApeObject_t left, ApeObject_t right, ApeOpByte_t op, bool *out_overload_found);
static void vmpriv_setdefaultconfig(ApeContext_t *ape);
static char *vmpriv_default_readfile(void *ctx, const char *filename);
static size_t vmpriv_default_writefile(void *ctx, const char *path, const char *string, size_t string_size);
static size_t vmpriv_default_stdout_write(void *ctx, const void *data, size_t size);


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

char* ape_stringf(ApeAllocator_t* alloc, const char* format, ...)
{
    int to_write;
    int written;
    char* res;
    va_list args;
    (void)written;
    va_start(args, format);
    to_write = vsnprintf(NULL, 0, format, args);
    va_end(args);
    va_start(args, format);
    res = (char*)allocator_malloc(alloc, to_write + 1);
    if(!res)
    {
        return NULL;
    }
    written = vsprintf(res, format, args);
    APE_ASSERT(written == to_write);
    va_end(args);
    return res;
}

char* ape_strndup(ApeAllocator_t* alloc, const char* string, size_t n)
{
    char* output_string;
    output_string = (char*)allocator_malloc(alloc, n + 1);
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

// fixme
uint64_t ape_double_to_uint64(ApeFloat_t val)
{
    union
    {
        uint64_t fltcast_uint64;
        ApeFloat_t fltcast_double;
    } temp = { .fltcast_double = val };
    return temp.fltcast_uint64;
}

ApeFloat_t ape_uint64_to_double(uint64_t val)
{
    union
    {
        uint64_t fltcast_uint64;
        ApeFloat_t fltcast_double;
    } temp = { .fltcast_uint64 = val };
    return temp.fltcast_double;
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
    timer.pc_frequency = (ApeFloat_t)(li.QuadPart) / 1000.0;
    QueryPerformanceCounter(&li);
    timer.start_time_ms = li.QuadPart / timer.pc_frequency;
#elif defined(APE_EMSCRIPTEN)
    timer.start_time_ms = emscripten_get_now();
#endif
    return timer;
}

ApeFloat_t ape_timer_get_elapsed_ms(const ApeTimer_t* timer)
{
#if defined(APE_POSIX)
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    int time_s = (int)((int64_t)current_time.tv_sec - timer->start_offset);
    ApeFloat_t current_time_ms = (time_s * 1000) + (current_time.tv_usec / 1000.0);
    return current_time_ms - timer->start_time_ms;
#elif defined(APE_WINDOWS)
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    ApeFloat_t current_time_ms = li.QuadPart / timer->pc_frequency;
    return current_time_ms - timer->start_time_ms;
#elif defined(APE_EMSCRIPTEN)
    ApeFloat_t current_time_ms = emscripten_get_now();
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
    char* output_string;
    output_string = (char*)allocator_malloc(alloc, n + 1);
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

/* djb2 */
unsigned long collections_hash(const void* ptr, size_t len)
{
    size_t i;
    unsigned long hash;
    ApeUShort_t val;
    const ApeUShort_t* ptr_u8;
    ptr_u8 = (const ApeUShort_t*)ptr;
    hash = 5381;
    for(i = 0; i < len; i++)
    {
        val = ptr_u8[i];
        hash = ((hash << 5) + hash) + val;
    }
    return hash;
}

static unsigned int vmpriv_upperpoweroftwo(unsigned int v)
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

ApeAllocator_t allocator_make(ApeMemAllocFunc_t malloc_fn, ApeMemFreeFunc_t free_fn, void* ctx)
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
ApeDictionary_t* dict_make(ApeAllocator_t* alloc, ApeDataCallback_t copy_fn, ApeDataCallback_t destroy_fn)
{
    bool ok;
    ApeDictionary_t* dict;
    dict = (ApeDictionary_t*)allocator_malloc(alloc, sizeof(ApeDictionary_t));
    if(dict == NULL)
    {
        return NULL;
    }
    ok = dict_init(dict, alloc, DICT_INITIAL_SIZE, copy_fn, destroy_fn);
    if(!ok)
    {
        allocator_free(alloc, dict);
        return NULL;
    }
    return dict;
}

void dict_destroy(ApeDictionary_t* dict)
{
    ApeAllocator_t* alloc;
    if(!dict)
    {
        return;
    }
    alloc = dict->alloc;
    dict_deinit(dict, true);
    allocator_free(alloc, dict);
}

void dict_destroy_with_items(ApeDictionary_t* dict)
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
    dict_destroy(dict);
}

ApeDictionary_t* dict_copy_with_items(ApeDictionary_t* dict)
{
    ApeSize_t i;
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
    dict_copy = dict_make(dict->alloc, (ApeDataCallback_t)dict->copy_fn, (ApeDataCallback_t)dict->destroy_fn);
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
    bool found;
    unsigned int item_ix;
    unsigned long cell_ix;
    unsigned long hash;
    hash = hash_string(key);
    found = false;
    cell_ix = dict_get_cell_ix(dict, key, hash, &found);
    if(found == false)
    {
        return NULL;
    }
    item_ix = dict->cells[cell_ix];
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

ApeSize_t dict_count(const ApeDictionary_t* dict)
{
    if(!dict)
    {
        return 0;
    }
    return dict->count;
}

// Private definitions
bool dict_init(ApeDictionary_t* dict, ApeAllocator_t* alloc, unsigned int initial_capacity, ApeDataCallback_t copy_fn, ApeDataCallback_t destroy_fn)
{
    ApeSize_t i;
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
    for(i = 0; i < dict->cell_capacity; i++)
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

unsigned int dict_get_cell_ix(const ApeDictionary_t* dict, const char* key, unsigned long hash, bool* out_found)
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
        if(cell == DICT_INVALID_IX)
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
    return DICT_INVALID_IX;
}

/* djb2 */
unsigned long hash_string(const char* str)
{
    unsigned long hash;
    ApeUShort_t c;
    hash = 5381;
    while((c = *str++))
    {
        /* hash * 33 + c */
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

bool dict_grow_and_rehash(ApeDictionary_t* dict)
{
    bool ok;
    ApeSize_t i;
    char* key;
    void* value;
    ApeDictionary_t new_dict;
    ok = dict_init(&new_dict, dict->alloc, dict->cell_capacity * 2, dict->copy_fn, dict->destroy_fn);
    if(!ok)
    {
        return false;
    }
    for(i = 0; i < dict->count; i++)
    {

        key = dict->keys[i];
        value = dict->values[i];
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
    bool ok;
    bool found;
    char* key_copy;
    unsigned long hash;
    unsigned int cell_ix;
    unsigned int item_ix;
    hash = hash_string(ckey);
    found = false;
    cell_ix = dict_get_cell_ix(dict, ckey, hash, &found);
    if(found)
    {
        item_ix = dict->cells[cell_ix];
        dict->values[item_ix] = value;
        return true;
    }
    if(dict->count >= dict->item_capacity)
    {
        ok = dict_grow_and_rehash(dict);
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
        key_copy = collections_strdup(dict->alloc, ckey);
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
    capacity = vmpriv_upperpoweroftwo(min_capacity * 2);
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
    ApeAllocator_t* alloc;
    if(!dict)
    {
        return;
    }
    alloc = dict->alloc;
    valdict_deinit(dict);
    allocator_free(alloc, dict);
}

void valdict_set_hash_function(ApeValDictionary_t* dict, ApeDataHashFunc_t hash_fn)
{
    dict->_hash_key = hash_fn;
}

void valdict_set_equals_function(ApeValDictionary_t* dict, ApeDataEqualsFunc_t equals_fn)
{
    dict->_keys_equals = equals_fn;
}

bool valdict_set(ApeValDictionary_t* dict, void* key, void* value)
{
    bool ok;
    bool found;
    unsigned int last_ix;
    unsigned int cell_ix;
    unsigned int item_ix;
    unsigned long hash;
    hash = valdict_hash_key(dict, key);
    found = false;
    cell_ix = valdict_get_cell_ix(dict, key, hash, &found);
    if(found)
    {
        item_ix = dict->cells[cell_ix];
        valdict_set_value_at(dict, item_ix, value);
        return true;
    }
    if(dict->count >= dict->item_capacity)
    {
        ok = valdict_grow_and_rehash(dict);
        if(!ok)
        {
            return false;
        }
        cell_ix = valdict_get_cell_ix(dict, key, hash, &found);
    }
    last_ix = dict->count;
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
    bool found;
    unsigned int item_ix;
    unsigned long hash;
    unsigned long cell_ix;
    hash = valdict_hash_key(dict, key);
    found = false;
    cell_ix = valdict_get_cell_ix(dict, key, hash, &found);
    if(!found)
    {
        return NULL;
    }
    item_ix = dict->cells[cell_ix];
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

bool valdict_set_value_at(const ApeValDictionary_t* dict, unsigned int ix, const void* value)
{
    size_t offset;
    if(ix >= dict->count)
    {
        return false;
    }
    offset = ix * dict->val_size;
    memcpy((char*)dict->values + offset, value, dict->val_size);
    return true;
}

ApeSize_t valdict_count(const ApeValDictionary_t* dict)
{
    if(!dict)
    {
        return 0;
    }
    return dict->count;
}

void valdict_clear(ApeValDictionary_t* dict)
{
    ApeSize_t i;
    dict->count = 0;
    for(i = 0; i < dict->cell_capacity; i++)
    {
        dict->cells[i] = VALDICT_INVALID_IX;
    }
}

// Private definitions
bool valdict_init(ApeValDictionary_t* dict, ApeAllocator_t* alloc, size_t key_size, size_t val_size, unsigned int initial_capacity)
{
    ApeSize_t i;
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
    for(i = 0; i < dict->cell_capacity; i++)
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
    bool are_equal;
    ApeSize_t ofs;
    ApeSize_t i;
    ApeSize_t ix;
    ApeSize_t cell;
    ApeSize_t cell_ix;
    unsigned long hash_to_check;
    void* key_to_check;
    *out_found = false;

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
    bool ok;
    ApeSize_t new_capacity;
    ApeSize_t i;
    char* key;
    void* value;
    ApeValDictionary_t new_dict;
    new_capacity = dict->cell_capacity == 0 ? DICT_INITIAL_SIZE : dict->cell_capacity * 2;
    ok = valdict_init(&new_dict, dict->alloc, dict->key_size, dict->val_size, new_capacity);
    if(!ok)
    {
        return false;
    }
    new_dict._keys_equals = dict->_keys_equals;
    new_dict._hash_key = dict->_hash_key;
    for(i = 0; i < dict->count; i++)
    {
        key = (char*)valdict_get_key_at(dict, i);
        value = valdict_get_value_at(dict, i);
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
    size_t offset;
    if(ix >= dict->count)
    {
        return false;
    }
    offset = ix * dict->key_size;
    memcpy((char*)dict->keys + offset, key, dict->key_size);
    return true;
}

bool valdict_keys_are_equal(const ApeValDictionary_t* dict, const void* a, const void* b)
{
    if(dict->_keys_equals)
    {
        return dict->_keys_equals(a, b);
    }
    return memcmp(a, b, dict->key_size) == 0;
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
// Array
//-----------------------------------------------------------------------------


ApeArray_t* array_make_(ApeAllocator_t* alloc, size_t element_size)
{
    return array_make_with_capacity(alloc, 32, element_size);
}

ApeArray_t* array_make_with_capacity(ApeAllocator_t* alloc, unsigned int capacity, size_t element_size)
{
    bool ok;
    ApeArray_t* arr;
    arr = (ApeArray_t*)allocator_malloc(alloc, sizeof(ApeArray_t));
    if(!arr)
    {
        return NULL;
    }

    ok = array_init_with_capacity(arr, alloc, capacity, element_size);
    if(!ok)
    {
        allocator_free(alloc, arr);
        return NULL;
    }
    return arr;
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
    unsigned int new_capacity;
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
    size_t offset;
    if(ix >= arr->count)
    {
        COLLECTIONS_ASSERT(false);
        return false;
    }
    offset = ix * arr->element_size;
    memmove(arr->arraydata + offset, value, arr->element_size);
    return true;
}


void* array_get(ApeArray_t* arr, unsigned int ix)
{
    size_t offset;
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

bool array_remove_at(ApeArray_t* arr, unsigned int ix)
{
    size_t to_move_bytes;
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

void array_orphan_data(ApeArray_t* arr)
{
    array_init_with_capacity(arr, arr->alloc, 0, arr->element_size);
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
    bool ok;
    ApePtrArray_t* ptrarr;
    ptrarr = (ApePtrArray_t*)allocator_malloc(alloc, sizeof(ApePtrArray_t));
    if(!ptrarr)
    {
        return NULL;
    }
    ptrarr->alloc = alloc;
    ok = array_init_with_capacity(&ptrarr->arr, alloc, capacity, sizeof(void*));
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

// todo: destroy and copy in make fn
void ptrarray_destroy_with_items(ApePtrArray_t* arr, ApeDataCallback_t destroy_fn)
{
    if(arr == NULL)
    {
        return;
    }
    if(destroy_fn)
    {
        ptrarray_clear_and_destroy_items(arr, destroy_fn);
    }
    ptrarray_destroy(arr);
}

ApePtrArray_t* ptrarray_copy_with_items(ApePtrArray_t* arr, ApeDataCallback_t copy_fn, ApeDataCallback_t destroy_fn)
{
    bool ok;
    ApeSize_t i;
    void* item;
    void* item_copy;
    ApePtrArray_t* arr_copy;
    arr_copy = ptrarray_make_with_capacity(arr->alloc, arr->arr.capacity);
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
    ptrarray_destroy_with_items(arr_copy, (ApeDataCallback_t)destroy_fn);
    return NULL;
}

bool ptrarray_add(ApePtrArray_t* arr, void* ptr)
{
    return array_add(&arr->arr, &ptr);
}

void* ptrarray_get(ApePtrArray_t* arr, unsigned int ix)
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
    ptrarray_remove_at(arr, ix);
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

bool ptrarray_remove_at(ApePtrArray_t* arr, unsigned int ix)
{
    return array_remove_at(&arr->arr, ix);
}

void ptrarray_clear(ApePtrArray_t* arr)
{
    array_clear(&arr->arr);
}

void ptrarray_clear_and_destroy_items(ApePtrArray_t* arr, ApeDataCallback_t destroy_fn)
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

//-----------------------------------------------------------------------------
// String buffer
//-----------------------------------------------------------------------------


ApeStringBuffer_t* strbuf_make(ApeAllocator_t* alloc)
{
    return strbuf_make_with_capacity(alloc, 1);
}

ApeStringBuffer_t* strbuf_make_with_capacity(ApeAllocator_t* alloc, unsigned int capacity)
{
    ApeStringBuffer_t* buf;
    buf = (ApeStringBuffer_t*)allocator_malloc(alloc, sizeof(ApeStringBuffer_t));
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

bool strbuf_appendn(ApeStringBuffer_t* buf, const char* str, size_t str_len)
{
    bool ok;
    size_t required_capacity;
    if(buf->failed)
    {
        return false;
    }
    if(str_len == 0)
    {
        return true;
    }
    required_capacity = buf->len + str_len + 1;
    if(required_capacity > buf->capacity)
    {
        ok = strbuf_grow(buf, required_capacity * 2);
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


bool strbuf_append(ApeStringBuffer_t* buf, const char* str)
{
    return strbuf_appendn(buf, str, strlen(str));
}

bool strbuf_appendf(ApeStringBuffer_t* buf, const char* fmt, ...)
{
    bool ok;
    int written;
    int to_write;
    size_t required_capacity;
    va_list args;
    (void)written;
    if(buf->failed)
    {
        return false;
    }
    va_start(args, fmt);
    to_write = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if(to_write == 0)
    {
        return true;
    }
    required_capacity = buf->len + to_write + 1;
    if(required_capacity > buf->capacity)
    {
        ok = strbuf_grow(buf, required_capacity * 2);
        if(!ok)
        {
            return false;
        }
    }
    va_start(args, fmt);
    written = vsprintf(buf->stringdata + buf->len, fmt, args);
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
    char* res;
    if(buf->failed)
    {
        strbuf_destroy(buf);
        return NULL;
    }
    res = buf->stringdata;
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
    char* new_data;
    new_data = (char*)allocator_malloc(buf->alloc, new_capacity);
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
    ApeSize_t i;
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
    ApeSize_t i;
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
    ApeSize_t i;
    char* joined;
    char* item;
    char* next_item;
    void* ptritem;
    ApePtrArray_t* split;
    if(!strchr(path, '/') || (!strstr(path, "/../") && !strstr(path, "./")))
    {
        return collections_strdup(alloc, path);
    }
    split = kg_split_string(alloc, path, "/");
    if(!split)
    {
        return NULL;
    }
    for(i = 0; i < ptrarray_count(split) - 1; i++)
    {
        item = (char*)ptrarray_get(split, i);
        next_item = (char*)ptrarray_get(split, i + 1);
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
    joined = kg_join(alloc, split, "/");

    for(i = 0; i < ptrarray_count(split); i++)
    {
        ptritem = (void*)ptrarray_get(split, i);
        allocator_free(alloc, ptritem);
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

static void vmpriv_initerrors(ApeErrorList_t* errors)
{
    memset(errors, 0, sizeof(ApeErrorList_t));
    errors->count = 0;
}

static void vmpriv_destroyerrors(ApeErrorList_t* errors)
{
    errors_clear(errors);
}

void errors_add_error(ApeErrorList_t* errors, ApeErrorType_t type, ApePosition_t pos, const char* message)
{
    int len;
    int to_copy;
    ApeError_t err;
    if(errors->count >= ERRORS_MAX_COUNT)
    {
        return;
    }
    memset(&err, 0, sizeof(ApeError_t));
    err.type = type;
    len = (int)strlen(message);
    to_copy = len;
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
    int to_write;
    int written;
    char res[APE_ERROR_MESSAGE_MAX_LENGTH];
    va_list args;
    (void)to_write;
    (void)written;
    va_start(args, format);
    to_write = vsnprintf(NULL, 0, format, args);
    va_end(args);
    va_start(args, format);
    written = vsnprintf(res, APE_ERROR_MESSAGE_MAX_LENGTH, format, args);
    APE_ASSERT(to_write == written);
    va_end(args);
    errors_add_error(errors, type, pos, res);
}

void errors_clear(ApeErrorList_t* errors)
{
    ApeSize_t i;
    ApeError_t* error;
    for(i = 0; i < errors_get_count(errors); i++)
    {
        error = errors_get(errors, i);
        if(error->traceback)
        {
            traceback_destroy(error->traceback);
        }
    }
    errors->count = 0;
}

ApeSize_t errors_get_count(const ApeErrorList_t* errors)
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

static const ApeError_t* vmpriv_errorsgetc(const ApeErrorList_t* errors, int ix)
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

ApeCompiledFile_t* compiled_file_make(ApeAllocator_t* alloc, const char* path)
{
    size_t len;
    const char* last_slash_pos;
    ApeCompiledFile_t* file;
    file = (ApeCompiledFile_t*)allocator_malloc(alloc, sizeof(ApeCompiledFile_t));
    if(!file)
    {
        return NULL;
    }
    memset(file, 0, sizeof(ApeCompiledFile_t));
    file->alloc = alloc;
    last_slash_pos = strrchr(path, '/');
    if(last_slash_pos)
    {
        len = last_slash_pos - path + 1;
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

void* compiled_file_destroy(ApeCompiledFile_t* file)
{
    ApeSize_t i;
    void* item;
    if(!file)
    {
        return NULL;
    }
    for(i = 0; i < ptrarray_count(file->lines); i++)
    {
        item = (void*)ptrarray_get(file->lines, i);
        allocator_free(file->alloc, item);
    }
    ptrarray_destroy(file->lines);
    allocator_free(file->alloc, file->dir_path);
    allocator_free(file->alloc, file->path);
    allocator_free(file->alloc, file);
    return NULL;
}

ApeGlobalStore_t* global_store_make(ApeAllocator_t* alloc, ApeGCMemory_t* mem)
{
    ApeSize_t i;
    bool ok;
    const char* name;
    ApeObject_t builtin;
    ApeGlobalStore_t* store;
    store = (ApeGlobalStore_t*)allocator_malloc(alloc, sizeof(ApeGlobalStore_t));
    if(!store)
    {
        return NULL;
    }
    memset(store, 0, sizeof(ApeGlobalStore_t));
    store->alloc = alloc;
    store->symbols = dict_make(alloc, (ApeDataCallback_t)symbol_copy, (ApeDataCallback_t)symbol_destroy);
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
        for(i = 0; i < builtins_count(); i++)
        {
            name = builtins_get_name(i);
            builtin = object_make_native_function_memory(mem, name, builtins_get_fn(i), NULL, 0);
            if(object_is_null(builtin))
            {
                goto err;
            }
            ok = global_store_set(store, name, builtin);
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

ApeObject_t* global_store_get_object_data(ApeGlobalStore_t* store)
{
    return (ApeObject_t*)array_data(store->objects);
}

ApeSize_t global_store_get_object_count(ApeGlobalStore_t* store)
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

void* symbol_destroy(ApeSymbol_t* symbol)
{
    if(!symbol)
    {
        return NULL;
    }
    allocator_free(symbol->alloc, symbol->name);
    allocator_free(symbol->alloc, symbol);
    return NULL;
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
    ptrarray_destroy_with_items(table->module_global_symbols, (ApeDataCallback_t)symbol_destroy);
    ptrarray_destroy_with_items(table->free_symbols, (ApeDataCallback_t)symbol_destroy);
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
    copy->block_scopes = ptrarray_copy_with_items(table->block_scopes, (ApeDataCallback_t)block_scope_copy, (ApeDataCallback_t)block_scope_destroy);
    if(!copy->block_scopes)
    {
        goto err;
    }
    copy->free_symbols = ptrarray_copy_with_items(table->free_symbols, (ApeDataCallback_t)symbol_copy, (ApeDataCallback_t)symbol_destroy);
    if(!copy->free_symbols)
    {
        goto err;
    }
    copy->module_global_symbols = ptrarray_copy_with_items(table->module_global_symbols, (ApeDataCallback_t)symbol_copy, (ApeDataCallback_t)symbol_destroy);
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
    ok = vmpriv_setsymbol(st, copy);
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
    ApeSize_t definitions_count;
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
    ix = vmpriv_nextsymbolindex(table);
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
    ok = vmpriv_setsymbol(table, symbol);
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
    definitions_count = vmpriv_countnumdefs(table);
    if(definitions_count > table->max_num_definitions)
    {
        table->max_num_definitions = definitions_count;
    }

    return symbol;
}

const ApeSymbol_t* symbol_table_define_free(ApeSymbolTable_t* st, const ApeSymbol_t* original)
{
    bool ok;
    ApeSymbol_t* symbol;
    ApeSymbol_t* copy;
    copy = symbol_make(st->alloc, original->name, original->type, original->index, original->assignable);
    if(!copy)
    {
        return NULL;
    }
    ok = ptrarray_add(st->free_symbols, copy);
    if(!ok)
    {
        symbol_destroy(copy);
        return NULL;
    }

    symbol = symbol_make(st->alloc, original->name, SYMBOL_FREE, ptrarray_count(st->free_symbols) - 1, original->assignable);
    if(!symbol)
    {
        return NULL;
    }

    ok = vmpriv_setsymbol(st, symbol);
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
    ok = vmpriv_setsymbol(st, symbol);
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
    ok = vmpriv_setsymbol(st, symbol);
    if(!ok)
    {
        symbol_destroy(symbol);
        return NULL;
    }
    return symbol;
}

const ApeSymbol_t* symbol_table_resolve(ApeSymbolTable_t* table, const char* name)
{
    int64_t i;
    const ApeSymbol_t* symbol;
    ApeBlockScope_t* scope;
    scope = NULL;
    symbol = global_store_get_symbol(table->global_store, name);
    if(symbol)
    {
        return symbol;
    }
    for(i = (int64_t)ptrarray_count(table->block_scopes) - 1; i >= 0; i--)
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

ApeSize_t symbol_table_get_module_global_symbol_count(const ApeSymbolTable_t* table)
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
    new_scope->store = dict_make(alloc, (ApeDataCallback_t)symbol_copy, (ApeDataCallback_t)symbol_destroy);
    if(!new_scope->store)
    {
        block_scope_destroy(new_scope);
        return NULL;
    }
    new_scope->num_definitions = 0;
    new_scope->offset = offset;
    return new_scope;
}

void* block_scope_destroy(ApeBlockScope_t* scope)
{
    dict_destroy_with_items(scope->store);
    allocator_free(scope->alloc, scope);
    return NULL;
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

static bool vmpriv_setsymbol(ApeSymbolTable_t* table, ApeSymbol_t* symbol)
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

static int vmpriv_nextsymbolindex(ApeSymbolTable_t* table)
{
    int ix;
    ApeBlockScope_t* top_scope;
    top_scope = (ApeBlockScope_t*)ptrarray_top(table->block_scopes);
    ix = top_scope->offset + top_scope->num_definitions;
    return ix;
}

static int vmpriv_countnumdefs(ApeSymbolTable_t* table)
{
    int64_t i;
    int count;
    ApeBlockScope_t* scope;
    count = 0;
    for(i = (int64_t)ptrarray_count(table->block_scopes) - 1; i >= 0; i--)
    {
        scope = (ApeBlockScope_t*)ptrarray_get(table->block_scopes, i);
        count += scope->num_definitions;
    }
    return count;
}


ApeOpcodeDefinition_t* opcode_lookup(ApeOpByte_t op)
{
    if(op <= OPCODE_NONE || op >= OPCODE_MAX)
    {
        return NULL;
    }
    return &g_definitions[op];
}

const char* opcode_get_name(ApeOpByte_t op)
{
    if(op <= OPCODE_NONE || op >= OPCODE_MAX)
    {
        return NULL;
    }
    return g_definitions[op].name;
}

#define APPEND_BYTE(n)                           \
    do                                           \
    {                                            \
        val = (ApeUShort_t)(operands[i] >> (n * 8)); \
        ok = array_add(res, &val);               \
        if(!ok)                                  \
        {                                        \
            return 0;                            \
        }                                        \
    } while(0)

int code_make(ApeOpByte_t op, ApeSize_t operands_count, uint64_t* operands, ApeArray_t* res)
{
    ApeSize_t i;
    int width;
    int instr_len;
    bool ok;
    ApeUShort_t val;
    ApeOpcodeDefinition_t* def;
    def = opcode_lookup(op);
    if(!def)
    {
        return 0;
    }
    instr_len = 1;
    for(i = 0; i < def->num_operands; i++)
    {
        instr_len += def->operand_widths[i];
    }
    val = op;
    ok = false;
    ok = array_add(res, &val);
    if(!ok)
    {
        return 0;
    }
    for(i = 0; i < operands_count; i++)
    {
        width = def->operand_widths[i];
        switch(width)
        {
            case 1:
                {
                    APPEND_BYTE(0);
                }
                break;
            case 2:
                {
                    APPEND_BYTE(1);
                    APPEND_BYTE(0);
                }
                break;
            case 4:
                {
                    APPEND_BYTE(3);
                    APPEND_BYTE(2);
                    APPEND_BYTE(1);
                    APPEND_BYTE(0);
                }
                break;
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
                }
                break;
            default:
                {
                    APE_ASSERT(false);
                }
                break;
        }
    }
    return instr_len;
}
#undef APPEND_BYTE

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
    scope->bytecode = array_make(alloc, ApeUShort_t);
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
        (ApeUShort_t*)array_data(scope->bytecode),
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

ApeCompilationResult_t* compilation_result_make(ApeAllocator_t* alloc, ApeUShort_t* bytecode, ApePosition_t* src_positions, int count)
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

ApeExpression_t* optimise_expression(ApeExpression_t* expr)
{

    switch(expr->type)
    {
        case EXPRESSION_INFIX:
            {
                return optimise_infix_expression(expr);
            }
            break;
        case EXPRESSION_PREFIX:
            {
                return optimise_prefix_expression(expr);
            }
            break;
        default:
            {
            }
            break;
    }
    return NULL;
}

// INTERNAL
ApeExpression_t* optimise_infix_expression(ApeExpression_t* expr)
{
    bool left_is_numeric;
    bool right_is_numeric;
    bool left_is_string;
    bool right_is_string;
    int64_t leftint;
    int64_t rightint;
    char* res_str;
    const char* leftstr;
    const char* rightstr;
    ApeExpression_t* left;
    ApeExpression_t* left_optimised;
    ApeExpression_t* right;
    ApeExpression_t* right_optimised;
    ApeExpression_t* res;
    ApeAllocator_t* alloc;
    ApeFloat_t leftval;
    ApeFloat_t rightval;
    left = expr->infix.left;
    left_optimised = optimise_expression(left);
    if(left_optimised)
    {
        left = left_optimised;
    }
    right = expr->infix.right;
    right_optimised = optimise_expression(right);
    if(right_optimised)
    {
        right = right_optimised;
    }
    res = NULL;
    left_is_numeric = left->type == EXPRESSION_NUMBER_LITERAL || left->type == EXPRESSION_BOOL_LITERAL;
    right_is_numeric = right->type == EXPRESSION_NUMBER_LITERAL || right->type == EXPRESSION_BOOL_LITERAL;
    left_is_string = left->type == EXPRESSION_STRING_LITERAL;
    right_is_string = right->type == EXPRESSION_STRING_LITERAL;
    alloc = expr->alloc;
    if(left_is_numeric && right_is_numeric)
    {
        leftval = left->type == EXPRESSION_NUMBER_LITERAL ? left->number_literal : left->bool_literal;
        rightval = right->type == EXPRESSION_NUMBER_LITERAL ? right->number_literal : right->bool_literal;
        leftint = (int64_t)leftval;
        rightint = (int64_t)rightval;
        switch(expr->infix.op)
        {
            case OPERATOR_PLUS:
                {
                    res = expression_make_number_literal(alloc, leftval + rightval);
                }
                break;
            case OPERATOR_MINUS:
                {
                    res = expression_make_number_literal(alloc, leftval - rightval);
                }
                break;
            case OPERATOR_ASTERISK:
                {
                    res = expression_make_number_literal(alloc, leftval * rightval);
                }
                break;
            case OPERATOR_SLASH:
                {
                    res = expression_make_number_literal(alloc, leftval / rightval);
                }
                break;
            case OPERATOR_LT:
                {
                    res = expression_make_bool_literal(alloc, leftval < rightval);
                }
                break;
            case OPERATOR_LTE:
                {
                    res = expression_make_bool_literal(alloc, leftval <= rightval);
                }
                break;
            case OPERATOR_GT:
                {
                    res = expression_make_bool_literal(alloc, leftval > rightval);
                }
                break;
            case OPERATOR_GTE:
                {
                    res = expression_make_bool_literal(alloc, leftval >= rightval);
                }
                break;
            case OPERATOR_EQ:
                {
                    res = expression_make_bool_literal(alloc, APE_DBLEQ(leftval, rightval));
                }
                break;

            case OPERATOR_NOT_EQ:
                {
                    res = expression_make_bool_literal(alloc, !APE_DBLEQ(leftval, rightval));
                }
                break;
            case OPERATOR_MODULUS:
                {
                    //res = expression_make_number_literal(alloc, fmod(leftval, rightval));
                    res = expression_make_number_literal(alloc, (leftint % rightint));
                }
                break;
            case OPERATOR_BIT_AND:
                {
                    res = expression_make_number_literal(alloc, (ApeFloat_t)(leftint & rightint));
                }
                break;
            case OPERATOR_BIT_OR:
                {
                    res = expression_make_number_literal(alloc, (ApeFloat_t)(leftint | rightint));
                }
                break;
            case OPERATOR_BIT_XOR:
                {
                    res = expression_make_number_literal(alloc, (ApeFloat_t)(leftint ^ rightint));
                }
                break;
            case OPERATOR_LSHIFT:
                {
                    res = expression_make_number_literal(alloc, (ApeFloat_t)(leftint << rightint));
                }
                break;
            case OPERATOR_RSHIFT:
                {
                    res = expression_make_number_literal(alloc, (ApeFloat_t)(leftint >> rightint));
                }
                break;
            default:
                {
                }
                break;
        }
    }
    else if(expr->infix.op == OPERATOR_PLUS && left_is_string && right_is_string)
    {
        leftstr = left->string_literal;
        rightstr = right->string_literal;
        res_str = ape_stringf(alloc, "%s%s", leftstr, rightstr);
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
    ApeExpression_t* right;
    ApeExpression_t* right_optimised;
    ApeExpression_t* res;
    right = expr->prefix.right;
    right_optimised = optimise_expression(right);
    if(right_optimised)
    {
        right = right_optimised;
    }
    res = NULL;
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
    ApeModule_t* module;
    module = (ApeModule_t*)allocator_malloc(alloc, sizeof(ApeModule_t));
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

void* module_destroy(ApeModule_t* module)
{
    if(!module)
    {
        return NULL;
    }
    allocator_free(module->alloc, module->name);
    ptrarray_destroy_with_items(module->symbols, (ApeDataCallback_t)symbol_destroy);
    allocator_free(module->alloc, module);
    return NULL;
}

ApeModule_t* module_copy(ApeModule_t* src)
{
    ApeModule_t* copy;
    copy = (ApeModule_t*)allocator_malloc(src->alloc, sizeof(ApeModule_t));
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
    copy->symbols = ptrarray_copy_with_items(src->symbols, (ApeDataCallback_t)symbol_copy, (ApeDataCallback_t)symbol_destroy);
    if(!copy->symbols)
    {
        module_destroy(copy);
        return NULL;
    }
    return copy;
}

const char* get_module_name(const char* path)
{
    const char* last_slash_pos;
    last_slash_pos = strrchr(path, '/');
    if(last_slash_pos)
    {
        return last_slash_pos + 1;
    }
    return path;
}

bool module_add_symbol(ApeModule_t* module, const ApeSymbol_t* symbol)
{
    bool ok;
    ApeStringBuffer_t* name_buf;
    ApeSymbol_t* module_symbol;
    name_buf = strbuf_make(module->alloc);
    if(!name_buf)
    {
        return false;
    }
    ok = strbuf_appendf(name_buf, "%s::%s", module->name, symbol->name);
    if(!ok)
    {
        strbuf_destroy(name_buf);
        return false;
    }
    module_symbol = symbol_make(module->alloc, strbuf_get_string(name_buf), SYMBOL_MODULE_GLOBAL, symbol->index, false);
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
    type_tag = vmpriv_gettypetag(type) & 0x7;
    // assumes no pointer exceeds 48 bits
    object.handle |= (type_tag << 48);
    object.handle |= (uintptr_t)data;
    return object;
}

ApeObject_t object_make_number(ApeFloat_t val)
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

ApeObject_t object_make_string_len(ApeGCMemory_t* mem, const char* string, size_t len)
{
    bool ok;
    ApeObject_t res;
    res = object_make_string_with_capacity(mem, len);
    if(object_is_null(res))
    {
        return res;
    }
    ok = object_string_append(res, string, len);
    if(!ok)
    {
        return object_make_null();
    }
    return res;
}

ApeObject_t object_make_string(ApeGCMemory_t* mem, const char* string)
{
    return object_make_string_len(mem, string, strlen(string));
}

ApeObject_t object_make_string_with_capacity(ApeGCMemory_t* mem, int capacity)
{
    bool ok;
    ApeObjectData_t* data;
    data = gcmem_get_object_data_from_pool(mem, APE_OBJECT_STRING);
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
        ok = object_data_string_reserve_capacity(data, capacity);
        if(!ok)
        {
            return object_make_null();
        }
    }
    return object_make_from_data(APE_OBJECT_STRING, data);
}


ApeObject_t object_make_native_function_memory(ApeGCMemory_t* mem, const char* name, ApeNativeFunc_t fn, void* data, int data_len)
{
    ApeObjectData_t* obj;
    if(data_len > NATIVE_FN_MAX_DATA_LEN)
    {
        return object_make_null();
    }
    obj = gcmem_alloc_object_data(mem, APE_OBJECT_NATIVE_FUNCTION);
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
    ApeObjectData_t* data;
    data = gcmem_get_object_data_from_pool(mem, APE_OBJECT_MAP);
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
    valdict_set_hash_function(data->map, (ApeDataHashFunc_t)object_hash);
    valdict_set_equals_function(data->map, (ApeDataEqualsFunc_t)object_equals_wrapped);
    return object_make_from_data(APE_OBJECT_MAP, data);
}

ApeObject_t object_make_error(ApeGCMemory_t* mem, const char* error)
{
    char* error_str;
    ApeObject_t res;
    error_str = ape_strdup(mem->alloc, error);
    if(!error_str)
    {
        return object_make_null();
    }
    res = object_make_error_no_copy(mem, error_str);
    if(object_is_null(res))
    {
        allocator_free(mem->alloc, error_str);
        return object_make_null();
    }
    return res;
}

ApeObject_t object_make_error_no_copy(ApeGCMemory_t* mem, char* error)
{
    ApeObjectData_t* data;
    data = gcmem_alloc_object_data(mem, APE_OBJECT_ERROR);
    if(!data)
    {
        return object_make_null();
    }
    data->error.message = error;
    data->error.traceback = NULL;
    return object_make_from_data(APE_OBJECT_ERROR, data);
}

ApeObject_t
object_make_function(ApeGCMemory_t* mem, const char* name, ApeCompilationResult_t* comp_res, bool owns_data, int num_locals, int num_args, ApeSize_t free_vals_count)
{
    ApeObjectData_t* data;
    data = gcmem_alloc_object_data(mem, APE_OBJECT_FUNCTION);
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
    ApeObjectData_t* obj;
    obj = gcmem_alloc_object_data(mem, APE_OBJECT_EXTERNAL);
    if(!obj)
    {
        return object_make_null();
    }
    obj->external.data = data;
    obj->external.data_destroy_fn = NULL;
    obj->external.data_copy_fn = NULL;
    return object_make_from_data(APE_OBJECT_EXTERNAL, obj);
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
            break;
        case APE_OBJECT_STRING:
            {
                if(data->string.is_allocated)
                {
                    allocator_free(data->mem->alloc, data->string.value_allocated);
                }
            }
            break;
        case APE_OBJECT_FUNCTION:
            {
                if(data->function.owns_data)
                {
                    allocator_free(data->mem->alloc, data->function.name);
                    compilation_result_destroy(data->function.comp_result);
                }
                if(vmpriv_freevalsallocated(&data->function))
                {
                    allocator_free(data->mem->alloc, data->function.free_vals_allocated);
                }
            }
            break;
        case APE_OBJECT_ARRAY:
            {
                array_destroy(data->array);
            }
            break;
        case APE_OBJECT_MAP:
            {
                valdict_destroy(data->map);
            }
            break;
        case APE_OBJECT_NATIVE_FUNCTION:
            {
                allocator_free(data->mem->alloc, data->native_function.name);
            }
            break;
        case APE_OBJECT_EXTERNAL:
            {
                if(data->external.data_destroy_fn)
                {
                    data->external.data_destroy_fn(data->external.data);
                }
            }
            break;
        case APE_OBJECT_ERROR:
            {
                allocator_free(data->mem->alloc, data->error.message);
                traceback_destroy(data->error.traceback);
            }
            break;
        default:
            {
            }
            break;
    }
    data->type = APE_OBJECT_FREED;
}

bool object_is_allocated(ApeObject_t object)
{
    return (object.handle & OBJECT_ALLOCATED_HEADER) == OBJECT_ALLOCATED_HEADER;
}

ApeGCMemory_t* object_get_mem(ApeObject_t obj)
{
    ApeObjectData_t* data;
    data = object_get_allocated_data(obj);
    return data->mem;
}

bool object_is_hashable(ApeObject_t obj)
{
    ApeObjectType_t type;
    type = object_get_type(obj);
    switch(type)
    {
        case APE_OBJECT_STRING:
            {
                return true;
            }
            break;
        case APE_OBJECT_NUMBER:
            {
                return true;
            }
            break;
        case APE_OBJECT_BOOL:
            {
                return true;
            }
            break;
        default:
            {
            }
            break;
    }
    return false;

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

char* object_get_type_union_name(ApeAllocator_t* alloc, const ApeObjectType_t type)
{
    bool in_between;
    ApeStringBuffer_t* res;
    if(type == APE_OBJECT_ANY || type == APE_OBJECT_NONE || type == APE_OBJECT_FREED)
    {
        return ape_strdup(alloc, object_get_type_name(type));
    }
    res = strbuf_make(alloc);
    if(!res)
    {
        return NULL;
    }
    in_between = false;
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
    ApeObject_t res;
    ApeValDictionary_t* copies;
    copies = valdict_make(mem->alloc, ApeObject_t, ApeObject_t);
    if(!copies)
    {
        return object_make_null();
    }
    res = object_deep_copy_internal(mem, obj, copies);
    valdict_destroy(copies);
    return res;
}

ApeObject_t object_copy(ApeGCMemory_t* mem, ApeObject_t obj)
{
    ApeSize_t i;
    int len;
    bool ok;
    const char* str;
    void* data_copy;
    ApeObject_t item;
    ApeObject_t copy;
    ApeObject_t key;
    ApeObject_t val;
    ApeObjectType_t type;
    ApeExternalData_t* external;
    copy = object_make_null();
    type = object_get_type(obj);
    switch(type)
    {
        case APE_OBJECT_ANY:
        case APE_OBJECT_FREED:
        case APE_OBJECT_NONE:
            {
                APE_ASSERT(false);
                copy = object_make_null();
            }
            break;
        case APE_OBJECT_NUMBER:
        case APE_OBJECT_BOOL:
        case APE_OBJECT_NULL:
        case APE_OBJECT_FUNCTION:
        case APE_OBJECT_NATIVE_FUNCTION:
        case APE_OBJECT_ERROR:
            {
                copy = obj;
            }
            break;
        case APE_OBJECT_STRING:
            {
                str = object_get_string(obj);
                copy = object_make_string(mem, str);
            }
            break;
        case APE_OBJECT_ARRAY:
            {
                len = object_get_array_length(obj);
                copy = object_make_array_with_capacity(mem, len);
                if(object_is_null(copy))
                {
                    return object_make_null();
                }
                for(i = 0; i < len; i++)
                {
                    item = object_get_array_value(obj, i);
                    ok = object_add_array_value(copy, item);
                    if(!ok)
                    {
                        return object_make_null();
                    }
                }
            }
            break;
        case APE_OBJECT_MAP:
            {
                copy = object_make_map(mem);
                for(i = 0; i < object_get_map_length(obj); i++)
                {
                    key = (ApeObject_t)object_get_map_key_at(obj, i);
                    val = (ApeObject_t)object_get_map_value_at(obj, i);
                    ok = object_set_map_value_object(copy, key, val);
                    if(!ok)
                    {
                        return object_make_null();
                    }
                }
            }
            break;
        case APE_OBJECT_EXTERNAL:
            {
                copy = object_make_external(mem, NULL);
                if(object_is_null(copy))
                {
                    return object_make_null();
                }
                external = object_get_external_data(obj);
                data_copy = NULL;
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
            }
            break;
    }
    return copy;
}

ApeFloat_t object_compare(ApeObject_t a, ApeObject_t b, bool* out_ok)
{
    bool isnumeric;
    int a_len;
    int b_len;
    unsigned long a_hash;
    unsigned long b_hash;
    intptr_t a_data_val;
    intptr_t b_data_val;
    const char* a_string;
    const char* b_string;
    ApeFloat_t leftval;
    ApeFloat_t rightval;
    ApeObjectType_t a_type;
    ApeObjectType_t b_type;
    if(a.handle == b.handle)
    {
        return 0;
    }
    *out_ok = true;
    a_type = object_get_type(a);
    b_type = object_get_type(b);
    isnumeric = (
        (a_type == APE_OBJECT_NUMBER || a_type == APE_OBJECT_BOOL || a_type == APE_OBJECT_NULL) &&
        (b_type == APE_OBJECT_NUMBER || b_type == APE_OBJECT_BOOL || b_type == APE_OBJECT_NULL)
    );
    if(isnumeric)
    {
        leftval = object_get_number(a);
        rightval = object_get_number(b);
        return leftval - rightval;
    }
    else if(a_type == b_type && a_type == APE_OBJECT_STRING)
    {
        a_len = object_get_string_length(a);
        b_len = object_get_string_length(b);
        if(a_len != b_len)
        {
            return a_len - b_len;
        }
        a_hash = object_get_string_hash(a);
        b_hash = object_get_string_hash(b);
        if(a_hash != b_hash)
        {
            return a_hash - b_hash;
        }
        a_string = object_get_string(a);
        b_string = object_get_string(b);
        return strcmp(a_string, b_string);
    }
    else if((object_is_allocated(a) || object_is_null(a)) && (object_is_allocated(b) || object_is_null(b)))
    {
        a_data_val = (intptr_t)object_get_allocated_data(a);
        b_data_val = (intptr_t)object_get_allocated_data(b);
        return (ApeFloat_t)(a_data_val - b_data_val);
    }
    else
    {
        *out_ok = false;
    }
    return 1;
}

bool object_equals(ApeObject_t a, ApeObject_t b)
{
    bool ok;
    ApeFloat_t res;
    ApeObjectType_t a_type;
    ApeObjectType_t b_type;
    a_type = object_get_type(a);
    b_type = object_get_type(b);
    if(a_type != b_type)
    {
        return false;
    }
    ok = false;
    res = object_compare(a, b, &ok);
    return APE_DBLEQ(res, 0);
}

ApeExternalData_t* object_get_external_data(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_EXTERNAL);
    data = object_get_allocated_data(object);
    return &data->external;
}

bool object_set_external_destroy_function(ApeObject_t object, ApeDataCallback_t destroy_fn)
{
    ApeExternalData_t* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_EXTERNAL);
    data = object_get_external_data(object);
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

ApeFloat_t object_get_number(ApeObject_t obj)
{
    if(object_is_number(obj))
    {// todo: optimise? always return number?
        return obj.number;
    }
    return (ApeFloat_t)(obj.handle & (~OBJECT_HEADER_MASK));
}

const char* object_get_string(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_STRING);
    data = object_get_allocated_data(object);
    return object_data_get_string(data);
}

int object_get_string_length(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_STRING);
    data = object_get_allocated_data(object);
    return data->string.length;
}

void object_set_string_length(ApeObject_t object, int len)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_STRING);
    data = object_get_allocated_data(object);
    data->string.length = len;
}

char* object_get_mutable_string(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_STRING);
    data = object_get_allocated_data(object);
    return object_data_get_string(data);
}

bool object_string_append(ApeObject_t obj, const char* src, int len)
{
    int capacity;
    int current_len;
    char* str_buf;
    ApeObjectData_t* data;
    ApeObjectString_t* string;
    APE_ASSERT(object_get_type(obj) == APE_OBJECT_STRING);
    data = object_get_allocated_data(obj);
    string = &data->string;
    str_buf = object_get_mutable_string(obj);
    current_len = string->length;
    capacity = string->capacity;
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
    ApeObjectData_t* data;
    APE_ASSERT(object_get_type(obj) == APE_OBJECT_STRING);
    data = object_get_allocated_data(obj);
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
    ApeObjectData_t* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_FUNCTION);
    data = object_get_allocated_data(object);
    return &data->function;
}

ApeNativeFunction_t* object_get_native_function(ApeObject_t obj)
{
    ApeObjectData_t* data;
    data = object_get_allocated_data(obj);
    return &data->native_function;
}

ApeObjectType_t object_get_type(ApeObject_t obj)
{
    uint64_t tag;
    if(object_is_number(obj))
    {
        return APE_OBJECT_NUMBER;
    }
    tag = (obj.handle >> 48) & 0x7;
    switch(tag)
    {
        case 0:
            {
                return APE_OBJECT_NONE;
            }
            break;
        case 1:
            {
                return APE_OBJECT_BOOL;
            }
            break;
        case 2:
            {
                return APE_OBJECT_NULL;
            }
            break;
        case 4:
            {
                ApeObjectData_t* data = object_get_allocated_data(obj);
                return data->type;
            }
            break;
        default:
            {
            }
            break;
    }
    return APE_OBJECT_NONE;

}

bool object_is_numeric(ApeObject_t obj)
{
    ApeObjectType_t type;
    type = object_get_type(obj);
    return type == APE_OBJECT_NUMBER || type == APE_OBJECT_BOOL;
}

bool object_is_null(ApeObject_t obj)
{
    return object_get_type(obj) == APE_OBJECT_NULL;
}

bool object_is_callable(ApeObject_t obj)
{
    ApeObjectType_t type;
    type = object_get_type(obj);
    return type == APE_OBJECT_NATIVE_FUNCTION || type == APE_OBJECT_FUNCTION;
}

const char* object_get_function_name(ApeObject_t obj)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_get_type(obj) == APE_OBJECT_FUNCTION);
    data = object_get_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return NULL;
    }
    if(data->function.owns_data)
    {
        return data->function.name;
    }
    return data->function.const_name;
}

ApeObject_t object_get_function_free_val(ApeObject_t obj, int ix)
{
    ApeObjectData_t* data;
    ApeFunction_t* fun;
    APE_ASSERT(object_get_type(obj) == APE_OBJECT_FUNCTION);
    data = object_get_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return object_make_null();
    }
    fun = &data->function;
    APE_ASSERT(ix >= 0 && ix < fun->free_vals_count);
    if(ix < 0 || ix >= fun->free_vals_count)
    {
        return object_make_null();
    }
    if(vmpriv_freevalsallocated(fun))
    {
        return fun->free_vals_allocated[ix];
    }
    return fun->free_vals_buf[ix];
}

void object_set_function_free_val(ApeObject_t obj, int ix, ApeObject_t val)
{
    ApeObjectData_t* data;
    ApeFunction_t* fun;
    APE_ASSERT(object_get_type(obj) == APE_OBJECT_FUNCTION);
    data = object_get_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return;
    }
    fun = &data->function;
    APE_ASSERT(ix >= 0 && ix < fun->free_vals_count);
    if(ix < 0 || ix >= fun->free_vals_count)
    {
        return;
    }
    if(vmpriv_freevalsallocated(fun))
    {
        fun->free_vals_allocated[ix] = val;
    }
    else
    {
        fun->free_vals_buf[ix] = val;
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
    ApeObjectData_t* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ERROR);
    if(object_get_type(object) != APE_OBJECT_ERROR)
    {
        return;
    }
    data = object_get_allocated_data(object);
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

bool object_set_external_copy_function(ApeObject_t object, ApeDataCallback_t copy_fn)
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

/*
* TODO: since this pushes NULLs before 'ix' if 'ix' is out of bounds, this
* may be possibly extremely inefficient.
*/
bool object_set_array_value_at(ApeObject_t object, ApeSize_t ix, ApeObject_t val)
{
    ApeArray_t* array;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ARRAY);
    array = object_get_allocated_array(object);
    if(ix < 0 || ix >= array_count(array))
    {
        while(ix >= array_count(array))
        {
            object_add_array_value(object, object_make_null());
        }
    }
    return array_set(array, ix, &val);
}

bool object_add_array_value(ApeObject_t object, ApeObject_t val)
{
    ApeArray_t* array;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ARRAY);
    array = object_get_allocated_array(object);
    return array_add(array, &val);
}

int object_get_array_length(ApeObject_t object)
{
    ApeArray_t* array;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ARRAY);
    array = object_get_allocated_array(object);
    return array_count(array);
}

bool object_remove_array_value_at(ApeObject_t object, int ix)
{
    ApeArray_t* array;
    array = object_get_allocated_array(object);
    return array_remove_at(array, ix);
}

int object_get_map_length(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    data = object_get_allocated_data(object);
    return valdict_count(data->map);
}

ApeObject_t object_get_map_key_at(ApeObject_t object, int ix)
{
    ApeObject_t* res;
    ApeObjectData_t* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    data = object_get_allocated_data(object);
    res= (ApeObject_t*)valdict_get_key_at(data->map, ix);
    if(!res)
    {
        return object_make_null();
    }
    return *res;
}

ApeObject_t object_get_map_value_at(ApeObject_t object, int ix)
{
    ApeObject_t* res;
    ApeObjectData_t* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    data = object_get_allocated_data(object);
    res = (ApeObject_t*)valdict_get_value_at(data->map, ix);
    if(!res)
    {
        return object_make_null();
    }
    return *res;
}

ApeObject_t object_get_kv_pair_at(ApeGCMemory_t* mem, ApeObject_t object, int ix)
{
    ApeObject_t key;
    ApeObject_t val;
    ApeObject_t res;
    ApeObject_t key_obj;
    ApeObject_t val_obj;
    ApeObjectData_t* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    data = object_get_allocated_data(object);
    if(ix >= valdict_count(data->map))
    {
        return object_make_null();
    }
    key = object_get_map_key_at(object, ix);
    val = object_get_map_value_at(object, ix);
    res = object_make_map(mem);
    if(object_is_null(res))
    {
        return object_make_null();
    }
    key_obj = object_make_string(mem, "key");
    if(object_is_null(key_obj))
    {
        return object_make_null();
    }
    object_set_map_value_object(res, key_obj, key);
    val_obj = object_make_string(mem, "value");
    if(object_is_null(val_obj))
    {
        return object_make_null();
    }
    object_set_map_value_object(res, val_obj, val);
    return res;
}

bool object_set_map_value_object(ApeObject_t object, ApeObject_t key, ApeObject_t val)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    data = object_get_allocated_data(object);
    return valdict_set(data->map, &key, &val);
}

ApeObject_t object_get_map_value_object(ApeObject_t object, ApeObject_t key)
{
    ApeObject_t* res;
    ApeObjectData_t* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_MAP);
    data = object_get_allocated_data(object);
    res = (ApeObject_t*)valdict_get(data->map, &key);
    if(!res)
    {
        return object_make_null();
    }
    return *res;
}

// INTERNAL
ApeObject_t object_deep_copy_internal(ApeGCMemory_t* mem, ApeObject_t obj, ApeValDictionary_t * copies)
{
    ApeSize_t i;
    int len;
    bool ok;
    const char* str;
    ApeUShort_t* bytecode_copy;
    ApeFunction_t* function_copy;
    ApeFunction_t* function;
    ApePosition_t* src_positions_copy;
    ApeCompilationResult_t* comp_res_copy;
    ApeObject_t free_val;
    ApeObject_t free_val_copy;
    ApeObject_t item;
    ApeObject_t item_copy;
    ApeObject_t key;
    ApeObject_t val;
    ApeObject_t key_copy;
    ApeObject_t val_copy;
    ApeObject_t copy;
    ApeObject_t* copy_ptr;
    ApeObjectType_t type;
    copy_ptr = (ApeObject_t*)valdict_get(copies, &obj);
    if(copy_ptr)
    {
        return *copy_ptr;
    }
    copy = object_make_null();
    type = object_get_type(obj);
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
                str = object_get_string(obj);
                copy = object_make_string(mem, str);
            }
            break;

        case APE_OBJECT_FUNCTION:
            {
                function = object_get_function(obj);
                bytecode_copy = NULL;
                src_positions_copy = NULL;
                comp_res_copy = NULL;
                bytecode_copy = (ApeUShort_t*)allocator_malloc(mem->alloc, sizeof(ApeUShort_t) * function->comp_result->count);
                if(!bytecode_copy)
                {
                    return object_make_null();
                }
                memcpy(bytecode_copy, function->comp_result->bytecode, sizeof(ApeUShort_t) * function->comp_result->count);
                src_positions_copy = (ApePosition_t*)allocator_malloc(mem->alloc, sizeof(ApePosition_t) * function->comp_result->count);
                if(!src_positions_copy)
                {
                    allocator_free(mem->alloc, bytecode_copy);
                    return object_make_null();
                }
                memcpy(src_positions_copy, function->comp_result->src_positions, sizeof(ApePosition_t) * function->comp_result->count);
                // todo: add compilation result copy function
                comp_res_copy = compilation_result_make(mem->alloc, bytecode_copy, src_positions_copy, function->comp_result->count);
                if(!comp_res_copy)
                {
                    allocator_free(mem->alloc, src_positions_copy);
                    allocator_free(mem->alloc, bytecode_copy);
                    return object_make_null();
                }
                copy = object_make_function(mem, object_get_function_name(obj), comp_res_copy, true, function->num_locals, function->num_args, 0);
                if(object_is_null(copy))
                {
                    compilation_result_destroy(comp_res_copy);
                    return object_make_null();
                }
                ok = valdict_set(copies, &obj, &copy);
                if(!ok)
                {
                    return object_make_null();
                }
                function_copy = object_get_function(copy);
                if(vmpriv_freevalsallocated(function))
                {
                    function_copy->free_vals_allocated = (ApeObject_t*)allocator_malloc(mem->alloc, sizeof(ApeObject_t) * function->free_vals_count);
                    if(!function_copy->free_vals_allocated)
                    {
                        return object_make_null();
                    }
                }
                function_copy->free_vals_count = function->free_vals_count;
                for(i = 0; i < function->free_vals_count; i++)
                {
                    free_val = object_get_function_free_val(obj, i);
                    free_val_copy = object_deep_copy_internal(mem, free_val, copies);
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
                len = object_get_array_length(obj);
                copy = object_make_array_with_capacity(mem, len);
                if(object_is_null(copy))
                {
                    return object_make_null();
                }
                ok = valdict_set(copies, &obj, &copy);
                if(!ok)
                {
                    return object_make_null();
                }
                for(i = 0; i < len; i++)
                {
                    item = object_get_array_value(obj, i);
                    item_copy = object_deep_copy_internal(mem, item, copies);
                    if(!object_is_null(item) && object_is_null(item_copy))
                    {
                        return object_make_null();
                    }
                    ok = object_add_array_value(copy, item_copy);
                    if(!ok)
                    {
                        return object_make_null();
                    }
                }
            }
            break;
        case APE_OBJECT_MAP:
            {
                copy = object_make_map(mem);
                if(object_is_null(copy))
                {
                    return object_make_null();
                }
                ok = valdict_set(copies, &obj, &copy);
                if(!ok)
                {
                    return object_make_null();
                }
                for(i = 0; i < object_get_map_length(obj); i++)
                {
                    key = object_get_map_key_at(obj, i);
                    val = object_get_map_value_at(obj, i);
                    key_copy = object_deep_copy_internal(mem, key, copies);
                    if(!object_is_null(key) && object_is_null(key_copy))
                    {
                        return object_make_null();
                    }
                    val_copy = object_deep_copy_internal(mem, val, copies);
                    if(!object_is_null(val) && object_is_null(val_copy))
                    {
                        return object_make_null();
                    }
                    ok = object_set_map_value_object(copy, key_copy, val_copy);
                    if(!ok)
                    {
                        return object_make_null();
                    }
                }
            }
            break;
        case APE_OBJECT_EXTERNAL:
            {
                copy = object_copy(mem, obj);
            }
            break;
        case APE_OBJECT_ERROR:
            {
                copy = obj;
            }
            break;
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
    bool bval;
    ApeFloat_t val;
    ApeObject_t obj;
    ApeObjectType_t type;
    obj = *obj_ptr;
    type = object_get_type(obj);
    switch(type)
    {
        case APE_OBJECT_NUMBER:
            {
                val = object_get_number(obj);
                return object_hash_double(val);
            }
            break;
        case APE_OBJECT_BOOL:
            {
                bval = object_get_bool(obj);
                return bval;
            }
            break;
        case APE_OBJECT_STRING:
            {
                return object_get_string_hash(obj);
            }
            break;
        default:
            {
            }
            break;
    }
    return 0;
}

/* djb2 */
unsigned long object_hash_string(const char* str)
{
    int c;
    unsigned long hash;
    hash = 5381;
    while((c = *str++))
    {
        /* hash * 33 + c */
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/* djb2 */
unsigned long object_hash_double(ApeFloat_t val)
{
    uint32_t* val_ptr;
    unsigned long hash;
    val_ptr = (uint32_t*)&val;
    hash = 5381;
    hash = ((hash << 5) + hash) + val_ptr[0];
    hash = ((hash << 5) + hash) + val_ptr[1];
    return hash;
}

ApeArray_t * object_get_allocated_array(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_get_type(object) == APE_OBJECT_ARRAY);
    data = object_get_allocated_data(object);
    return data->array;
}

bool object_is_number(ApeObject_t o)
{
    return (o.handle & OBJECT_PATTERN) != OBJECT_PATTERN;
}

static uint64_t vmpriv_gettypetag(ApeObjectType_t type)
{
    switch(type)
    {
        case APE_OBJECT_NONE:
            {
                return 0;
            }
            break;
        case APE_OBJECT_BOOL:
            {
                return 1;
            }
            break;
        case APE_OBJECT_NULL:
            {
                return 2;
            }
            break;
        default:
            {
            }
            break;
    }
    return 4;
}

static bool vmpriv_freevalsallocated(ApeFunction_t* fun)
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
    return data->string.value_buf;
}

bool object_data_string_reserve_capacity(ApeObjectData_t* data, int capacity)
{
    char* new_value;
    ApeObjectString_t* string;
    APE_ASSERT(capacity >= 0);
    string = &data->string;
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
    new_value = (char*)allocator_malloc(data->mem->alloc, capacity + 1);
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
    ApeSize_t i;
    ApeGCMemory_t* mem;
    ApeObjectDataPool_t* pool;
    mem = (ApeGCMemory_t*)allocator_malloc(alloc, sizeof(ApeGCMemory_t));
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
    if(!object_is_allocated(obj))
    {
        return;
    }
    data = object_get_allocated_data(obj);
    if(data->gcmark)
    {
        return;
    }
    data->gcmark = true;
    switch(data->type)
    {
        case APE_OBJECT_MAP:
        {
            len = object_get_map_length(obj);
            for(i = 0; i < len; i++)
            {
                key = object_get_map_key_at(obj, i);
                if(object_is_allocated(key))
                {

                    key_data = object_get_allocated_data(key);
                    if(!key_data->gcmark)
                    {
                        gc_mark_object(key);
                    }
                }
                val = object_get_map_value_at(obj, i);
                if(object_is_allocated(val))
                {

                    val_data = object_get_allocated_data(val);
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
                len = object_get_array_length(obj);
                for(i = 0; i < len; i++)
                {
                    val = object_get_array_value(obj, i);
                    if(object_is_allocated(val))
                    {
                        val_data = object_get_allocated_data(val);
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
                function = object_get_function(obj);
                for(i = 0; i < function->free_vals_count; i++)
                {
                    free_val = object_get_function_free_val(obj, i);
                    gc_mark_object(free_val);
                    if(object_is_allocated(free_val))
                    {
                        free_val_data = object_get_allocated_data(free_val);
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
                if(object_get_array_length(obj) > 1024)
                {
                    return false;
                }
            }
            break;
        case APE_OBJECT_MAP:
            {
                if(object_get_map_length(obj) > 1024)
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


ApeTraceback_t* traceback_make(ApeAllocator_t* alloc)
{
    ApeTraceback_t* traceback;
    traceback = (ApeTraceback_t*)allocator_malloc(alloc, sizeof(ApeTraceback_t));
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
    ApeSize_t i;
    ApeTracebackItem_t* item;
    if(!traceback)
    {
        return;
    }
    for(i = 0; i < array_count(traceback->items); i++)
    {
        item = (ApeTracebackItem_t*)array_get(traceback->items, i);
        allocator_free(traceback->alloc, item->function_name);
    }
    array_destroy(traceback->items);
    allocator_free(traceback->alloc, traceback);
}

bool traceback_append(ApeTraceback_t* traceback, const char* function_name, ApePosition_t pos)
{
    bool ok;
    ApeTracebackItem_t item;
    item.function_name = ape_strdup(traceback->alloc, function_name);
    if(!item.function_name)
    {
        return false;
    }
    item.pos = pos;
    ok = array_add(traceback->items, &item);
    if(!ok)
    {
        allocator_free(traceback->alloc, item.function_name);
        return false;
    }
    return true;
}

bool traceback_append_from_vm(ApeTraceback_t* traceback, ApeVM_t* vm)
{
    int64_t i;
    bool ok;
    ApeFrame_t* frame;
    for(i = vm->frames_count - 1; i >= 0; i--)
    {
        frame = &vm->frames[i];
        ok = traceback_append(traceback, object_get_function_name(frame->function), frame_src_position(frame));
        if(!ok)
        {
            return false;
        }
    }
    return true;
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
    ApeFunction_t* function;
    if(object_get_type(function_obj) != APE_OBJECT_FUNCTION)
    {
        return false;
    }
    function = object_get_function(function_obj);
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
    uint64_t res;
    const ApeUShort_t* data;
    data = frame->bytecode + frame->ip;
    frame->ip += 8;
    res = 0;
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
    const ApeUShort_t* data;
    data = frame->bytecode + frame->ip;
    frame->ip += 2;
    return (data[0] << 8) | data[1];
}

ApeUShort_t frame_read_uint8(ApeFrame_t* frame)
{
    const ApeUShort_t* data;
    data = frame->bytecode + frame->ip;
    frame->ip++;
    return data[0];
}

ApePosition_t frame_src_position(const ApeFrame_t* frame)
{
    if(frame->src_positions)
    {
        return frame->src_positions[frame->src_ip];
    }
    return g_vmpriv_src_pos_invalid;
}

#define SET_OPERATOR_OVERLOAD_KEY(op, key)                   \
    do                                                       \
    {                                                        \
        key_obj = object_make_string(vm->mem, key); \
        if(object_is_null(key_obj))                          \
        {                                                    \
            goto err;                                        \
        }                                                    \
        vm->operator_oveload_keys[op] = key_obj;             \
    } while(0)

ApeVM_t* vm_make(ApeAllocator_t* alloc, const ApeConfig_t* config, ApeGCMemory_t* mem, ApeErrorList_t* errors, ApeGlobalStore_t* global_store)
{
    ApeSize_t i;
    ApeVM_t* vm;
    ApeObject_t key_obj;
    vm = (ApeVM_t*)allocator_malloc(alloc, sizeof(ApeVM_t));
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
    for(i = 0; i < OPCODE_MAX; i++)
    {
        vm->operator_oveload_keys[i] = object_make_null();
    }
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
    return vm;
err:
    vm_destroy(vm);
    return NULL;
}
#undef SET_OPERATOR_OVERLOAD_KEY

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
        vmpriv_popframe(vm);
    }
}

bool vm_run(ApeVM_t* vm, ApeCompilationResult_t* comp_res, ApeArray_t * constants)
{
    bool res;
    int old_sp;
    int old_this_sp;
    ApeSize_t old_frames_count;
    ApeObject_t main_fn;
#ifdef APE_DEBUG
    old_sp = vm->sp;
#endif
    old_this_sp = vm->this_sp;
    old_frames_count = vm->frames_count;
    main_fn = object_make_function(vm->mem, "main", comp_res, false, 0, 0, 0);
    if(object_is_null(main_fn))
    {
        return false;
    }
    vmpriv_pushstack(vm, main_fn);
    res = vm_execute_function(vm, main_fn, constants);
    while(vm->frames_count > old_frames_count)
    {
        vmpriv_popframe(vm);
    }
    APE_ASSERT(vm->sp == old_sp);
    vm->this_sp = old_this_sp;
    return res;
}

bool vmpriv_append_string(ApeVM_t* vm, ApeObject_t left, ApeObject_t right, ApeObjectType_t lefttype, ApeObjectType_t righttype)
{
    bool ok;
    const char* leftstr;
    const char* rightstr;
    ApeObject_t objres;
    ApeInt_t buflen;
    ApeInt_t leftlen;
    ApeInt_t rightlen;
    ApeStringBuffer_t* allbuf;
    ApeStringBuffer_t* tostrbuf;
    (void)lefttype;
    if(righttype == APE_OBJECT_STRING)
    {
        leftlen = (int)object_get_string_length(left);
        rightlen = (int)object_get_string_length(right);
        // avoid doing unnecessary copying by reusing the origin as-is
        if(leftlen == 0)
        {
            vmpriv_pushstack(vm, right);
        }
        else if(rightlen == 0)
        {
            vmpriv_pushstack(vm, left);
        }
        else
        {
            leftstr = object_get_string(left);
            rightstr = object_get_string(right);
            objres = object_make_string_with_capacity(vm->mem, leftlen + rightlen);
            if(object_is_null(objres))
            {
                return false;
            }
            ok = object_string_append(objres, leftstr, leftlen);
            if(!ok)
            {
                return false;
            }
            ok = object_string_append(objres, rightstr, rightlen);
            if(!ok)
            {
                return false;
            }
            vmpriv_pushstack(vm, objres);
        }
    }
    else
    {
        /*
        * when 'right' is not a string, stringify it, and create a new string.
        * in short, this does 'left = left + tostring(right)'
        * TODO: probably really inefficient.
        */
        allbuf = strbuf_make(vm->alloc);
        tostrbuf = strbuf_make(vm->alloc);
        strbuf_appendn(allbuf, object_get_string(left), object_get_string_length(left));
        object_to_string(right, tostrbuf, false);
        strbuf_appendn(allbuf, strbuf_get_string(tostrbuf), strbuf_get_length(tostrbuf));
        buflen = strbuf_get_length(allbuf);
        objres = object_make_string_with_capacity(vm->mem, buflen);
        ok = object_string_append(objres, strbuf_get_string(allbuf), buflen);
        strbuf_destroy(tostrbuf);
        strbuf_destroy(allbuf);
        if(!ok)
        {
            return false;
        }
        vmpriv_pushstack(vm, objres);
    }
    return true;
}

/*
* TODO: there is A LOT of code in this function.
* some could be easily split into other functions.
*/
bool vm_execute_function(ApeVM_t* vm, ApeObject_t function, ApeArray_t * constants)
{
    bool checktime;
    bool isoverloaded;
    bool ok;
    bool overloadfound;
    bool resval;
    const ApeFunction_t* constfunction;
    const char* indextypename;
    const char* keytypename;
    const char* left_type_name;
    const char* left_type_string;

    const char* opcode_name;
    const char* operand_type_string;
    const char* right_type_name;
    const char* right_type_string;
    const char* str;
    const char* type_name;
    int elapsed_ms;
    ApeSize_t i;
    int ix;
    int leftlen;
    int len;
    int recover_frame_ix;
    int64_t ui;
    int64_t leftint;
    int64_t rightint;
    uint16_t constant_ix;
    uint16_t count;
    uint16_t items_count;
    uint16_t kvp_count;
    uint16_t recover_ip;
    int64_t val;
    int64_t pos;
    unsigned time_check_counter;
    unsigned time_check_interval;
    ApeUShort_t free_ix;
    ApeUShort_t num_args;
    ApeUShort_t num_free;
    ApeFloat_t comparison_res;
    ApeFloat_t leftval;
    ApeFloat_t maxexecms;
    ApeFloat_t res;
    ApeFloat_t rightval;
    ApeFloat_t valdouble;
    ApeObjectType_t constant_type;
    ApeObjectType_t indextype;
    ApeObjectType_t keytype;
    ApeObjectType_t lefttype;
    ApeObjectType_t operand_type;
    ApeObjectType_t righttype;
    ApeObjectType_t type;
    ApeObject_t array_obj;
    ApeObject_t callee;
    ApeObject_t current_function;
    ApeObject_t err_obj;
    ApeObject_t free_val;
    ApeObject_t function_obj;
    ApeObject_t global;
    ApeObject_t index;
    ApeObject_t key;
    ApeObject_t left;
    ApeObject_t map_obj;
    ApeObject_t new_value;
    ApeObject_t objres;
    ApeObject_t objval;
    ApeObject_t old_value;
    ApeObject_t operand;
    ApeObject_t right;
    ApeObject_t testobj;
    ApeObject_t value;
    ApeObject_t* constant;
    ApeObject_t* items;
    ApeObject_t* kv_pairs;
    ApeOpcodeValue_t opcode;
    ApeTimer_t timer;
    ApeFrame_t* frame;
    ApeError_t* err;
    ApeFrame_t new_frame;
    ApeFunction_t* function_function;
    if(vm->running)
    {
        errors_add_error(vm->errors, APE_ERROR_USER, g_vmpriv_src_pos_invalid, "VM is already executing code");
        return false;
    }
    function_function = object_get_function(function);// naming is hard
    ok = false;
    ok = frame_init(&new_frame, function, vm->sp - function_function->num_args);
    if(!ok)
    {
        return false;
    }
    ok = vmpriv_pushframe(vm, new_frame);
    if(!ok)
    {
        errors_add_error(vm->errors, APE_ERROR_USER, g_vmpriv_src_pos_invalid, "pushing frame failed");
        return false;
    }
    vm->running = true;
    vm->last_popped = object_make_null();
    checktime = false;
    maxexecms = 0;
    if(vm->config)
    {
        checktime = vm->config->max_execution_time_set;
        maxexecms = vm->config->max_execution_time_ms;
    }
    time_check_interval = 1000;
    time_check_counter = 0;
    memset(&timer, 0, sizeof(ApeTimer_t));
    if(checktime)
    {
        timer = ape_timer_start();
    }
    while(vm->current_frame->ip < vm->current_frame->bytecode_size)
    {
        opcode = frame_read_opcode(vm->current_frame);
        switch(opcode)
        {
            case OPCODE_CONSTANT:
                {
                    constant_ix = frame_read_uint16(vm->current_frame);
                    constant = (ApeObject_t*)array_get(constants, constant_ix);
                    if(!constant)
                    {
                        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "constant at %d not found", constant_ix);
                        goto err;
                    }
                    vmpriv_pushstack(vm, *constant);
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
                    right = vmpriv_popstack(vm);
                    left = vmpriv_popstack(vm);
                    // NULL to 0 coercion
                    if(object_is_numeric(left) && object_is_null(right))
                    {
                        right = object_make_number(0);
                    }
                    if(object_is_numeric(right) && object_is_null(left))
                    {
                        left = object_make_number(0);
                    }
                    lefttype = object_get_type(left);
                    righttype = object_get_type(right);
                    if(object_is_numeric(left) && object_is_numeric(right))
                    {
                        rightval = object_get_number(right);
                        leftval = object_get_number(left);
                        leftint = (int64_t)leftval;
                        rightint = (int64_t)rightval;
                        res = 0;
                        switch(opcode)
                        {
                            case OPCODE_ADD:
                                {
                                    res = leftval + rightval;
                                }
                                break;
                            case OPCODE_SUB:
                                {
                                    res = leftval - rightval;
                                }
                                break;
                            case OPCODE_MUL:
                                {
                                    res = leftval * rightval;
                                }
                                break;
                            case OPCODE_DIV:
                                {
                                    res = leftval / rightval;
                                }
                                break;
                            case OPCODE_MOD:
                                {
                                    //res = fmod(leftval, rightval);
                                    res = (leftint % rightint);
                                }
                                break;
                            case OPCODE_OR:
                                {
                                    res = (ApeFloat_t)(leftint | rightint);
                                }
                                break;
                            case OPCODE_XOR:
                                {
                                    res = (ApeFloat_t)(leftint ^ rightint);
                                }
                                break;
                            case OPCODE_AND:
                                {
                                    res = (ApeFloat_t)(leftint & rightint);
                                }
                                break;
                            case OPCODE_LSHIFT:
                                {
                                    res = (ApeFloat_t)(leftint << rightint);
                                }
                                break;
                            case OPCODE_RSHIFT:
                                {
                                    res = (ApeFloat_t)(leftint >> rightint);
                                }
                                break;
                            default:
                                {
                                    APE_ASSERT(false);
                                }
                                break;
                        }
                        vmpriv_pushstack(vm, object_make_number(res));
                    }
                    else if(lefttype == APE_OBJECT_STRING && ((righttype == APE_OBJECT_STRING) || (righttype == APE_OBJECT_NUMBER)) && opcode == OPCODE_ADD)
                    {
                        ok = vmpriv_append_string(vm, left, right, lefttype, righttype);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                    else if((lefttype == APE_OBJECT_ARRAY) && opcode == OPCODE_ADD)
                    {
                        object_add_array_value(left, right);
                        vmpriv_pushstack(vm, left);
                    }
                    else
                    {
                        overloadfound = false;
                        ok = vmpriv_tryoverloadoperator(vm, left, right, opcode, &overloadfound);
                        if(!ok)
                        {
                            goto err;
                        }
                        if(!overloadfound)
                        {
                            opcode_name = opcode_get_name(opcode);
                            left_type_name = object_get_type_name(lefttype);
                            right_type_name = object_get_type_name(righttype);
                            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "invalid operand types for %s, got %s and %s", opcode_name, left_type_name,
                                              right_type_name);
                            goto err;
                        }
                    }
                }
                break;
            case OPCODE_POP:
                {
                    vmpriv_popstack(vm);
                }
                break;
            case OPCODE_TRUE:
                {
                    vmpriv_pushstack(vm, object_make_bool(true));
                }
                break;
            case OPCODE_FALSE:
                {
                    vmpriv_pushstack(vm, object_make_bool(false));
                }
                break;
            case OPCODE_COMPARE:
            case OPCODE_COMPARE_EQ:
                {
                    right = vmpriv_popstack(vm);
                    left = vmpriv_popstack(vm);
                    isoverloaded = false;
                    ok = vmpriv_tryoverloadoperator(vm, left, right, OPCODE_COMPARE, &isoverloaded);
                    if(!ok)
                    {
                        goto err;
                    }
                    if(!isoverloaded)
                    {
                        comparison_res = object_compare(left, right, &ok);
                        if(ok || opcode == OPCODE_COMPARE_EQ)
                        {
                            objres = object_make_number(comparison_res);
                            vmpriv_pushstack(vm, objres);
                        }
                        else
                        {
                            right_type_string = object_get_type_name(object_get_type(right));
                            left_type_string = object_get_type_name(object_get_type(left));
                            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "cannot compare %s and %s", left_type_string, right_type_string);
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
                    value = vmpriv_popstack(vm);
                    comparison_res = object_get_number(value);
                    resval = false;
                    switch(opcode)
                    {
                        case OPCODE_EQUAL:
                            resval = APE_DBLEQ(comparison_res, 0);
                            break;
                        case OPCODE_NOT_EQUAL:
                            resval = !APE_DBLEQ(comparison_res, 0);
                            break;
                        case OPCODE_GREATER_THAN:
                            resval = comparison_res > 0;
                            break;
                        case OPCODE_GREATER_THAN_EQUAL:
                            {
                                resval = comparison_res > 0 || APE_DBLEQ(comparison_res, 0);
                            }
                            break;
                        default:
                            {
                                APE_ASSERT(false);
                            }
                            break;
                    }
                    objres = object_make_bool(resval);
                    vmpriv_pushstack(vm, objres);
                }
                break;

            case OPCODE_MINUS:
                {
                    operand = vmpriv_popstack(vm);
                    operand_type = object_get_type(operand);
                    if(operand_type == APE_OBJECT_NUMBER)
                    {
                        val = object_get_number(operand);
                        objres = object_make_number(-val);
                        vmpriv_pushstack(vm, objres);
                    }
                    else
                    {
                        overloadfound = false;
                        ok = vmpriv_tryoverloadoperator(vm, operand, object_make_null(), OPCODE_MINUS, &overloadfound);
                        if(!ok)
                        {
                            goto err;
                        }
                        if(!overloadfound)
                        {
                            operand_type_string = object_get_type_name(operand_type);
                            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "invalid operand type for MINUS, got %s", operand_type_string);
                            goto err;
                        }
                    }
                }
                break;

            case OPCODE_BANG:
                {
                    operand = vmpriv_popstack(vm);
                    type = object_get_type(operand);
                    if(type == APE_OBJECT_BOOL)
                    {
                        objres = object_make_bool(!object_get_bool(operand));
                        vmpriv_pushstack(vm, objres);
                    }
                    else if(type == APE_OBJECT_NULL)
                    {
                        objres = object_make_bool(true);
                        vmpriv_pushstack(vm, objres);
                    }
                    else
                    {
                        overloadfound = false;
                        ok = vmpriv_tryoverloadoperator(vm, operand, object_make_null(), OPCODE_BANG, &overloadfound);
                        if(!ok)
                        {
                            goto err;
                        }
                        if(!overloadfound)
                        {
                            ApeObject_t objres = object_make_bool(false);
                            vmpriv_pushstack(vm, objres);
                        }
                    }
                }
                break;

            case OPCODE_JUMP:
                {
                    pos = frame_read_uint16(vm->current_frame);
                    vm->current_frame->ip = pos;
                }
                break;

            case OPCODE_JUMP_IF_FALSE:
                {
                    pos = frame_read_uint16(vm->current_frame);
                    testobj = vmpriv_popstack(vm);
                    if(!object_get_bool(testobj))
                    {
                        vm->current_frame->ip = pos;
                    }
                }
                break;

            case OPCODE_JUMP_IF_TRUE:
                {
                    pos = frame_read_uint16(vm->current_frame);
                    testobj = vmpriv_popstack(vm);
                    if(object_get_bool(testobj))
                    {
                        vm->current_frame->ip = pos;
                    }
                }
                break;

            case OPCODE_NULL:
                {
                    vmpriv_pushstack(vm, object_make_null());
                }
                break;

            case OPCODE_DEFINE_MODULE_GLOBAL:
            {
                ix = frame_read_uint16(vm->current_frame);
                objval = vmpriv_popstack(vm);
                vm_set_global(vm, ix, objval);
                break;
            }
            case OPCODE_SET_MODULE_GLOBAL:
                {
                    ix = frame_read_uint16(vm->current_frame);
                    new_value = vmpriv_popstack(vm);
                    old_value = vm_get_global(vm, ix);
                    if(!vmpriv_checkassign(vm, old_value, new_value))
                    {
                        goto err;
                    }
                    vm_set_global(vm, ix, new_value);
                }
                break;

            case OPCODE_GET_MODULE_GLOBAL:
                {
                    ix = frame_read_uint16(vm->current_frame);
                    global = vm->globals[ix];
                    vmpriv_pushstack(vm, global);
                }
                break;

            case OPCODE_ARRAY:
                {
                    count = frame_read_uint16(vm->current_frame);
                    array_obj = object_make_array_with_capacity(vm->mem, count);
                    if(object_is_null(array_obj))
                    {
                        goto err;
                    }
                    items = vm->stack + vm->sp - count;
                    for(i = 0; i < count; i++)
                    {
                        ApeObject_t item = items[i];
                        ok = object_add_array_value(array_obj, item);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                    vmpriv_setstackpointer(vm, vm->sp - count);
                    vmpriv_pushstack(vm, array_obj);
                }
                break;

            case OPCODE_MAP_START:
                {
                    count = frame_read_uint16(vm->current_frame);
                    map_obj = object_make_map_with_capacity(vm->mem, count);
                    if(object_is_null(map_obj))
                    {
                        goto err;
                    }
                    vmpriv_pushthisstack(vm, map_obj);
                }
                break;
            case OPCODE_MAP_END:
                {
                    kvp_count = frame_read_uint16(vm->current_frame);
                    items_count = kvp_count * 2;
                    map_obj = vmpriv_popthisstack(vm);
                    kv_pairs = vm->stack + vm->sp - items_count;
                    for(i = 0; i < items_count; i += 2)
                    {
                        key = kv_pairs[i];
                        if(!object_is_hashable(key))
                        {
                            keytype = object_get_type(key);
                            keytypename = object_get_type_name(keytype);
                            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "key of type %s is not hashable", keytypename);
                            goto err;
                        }
                        objval = kv_pairs[i + 1];
                        ok = object_set_map_value_object(map_obj, key, objval);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                    vmpriv_setstackpointer(vm, vm->sp - items_count);
                    vmpriv_pushstack(vm, map_obj);
                }
                break;

            case OPCODE_GET_THIS:
                {
                    objval = vmpriv_getthisstack(vm, 0);
                    vmpriv_pushstack(vm, objval);
                }
                break;

            case OPCODE_GET_INDEX:
                {
                    #if 0
                    const char* idxname;
                    #endif
                    index = vmpriv_popstack(vm);
                    left = vmpriv_popstack(vm);
                    lefttype = object_get_type(left);
                    indextype = object_get_type(index);
                    left_type_name = object_get_type_name(lefttype);
                    indextypename = object_get_type_name(indextype);
                    /*
                    * todo: object method lookup could be implemented here
                    */
                    #if 0
                    {
                        int argc;
                        ApeObject_t args[10];
                        ApeNativeFunc_t afn;
                        argc = 0;
                        if(indextype == APE_OBJECT_STRING)
                        {
                            idxname = object_get_string(index);
                            fprintf(stderr, "index is a string: name=%s\n", idxname);
                            if((afn = builtin_get_object(lefttype, idxname)) != NULL)
                            {
                                fprintf(stderr, "got a callback: afn=%p\n", afn);
                                //typedef ApeObject_t (*ApeNativeFunc_t)(ApeVM_t*, void*, int, ApeObject_t*);
                                args[argc] = left;
                                argc++;
                                vmpriv_pushstack(vm, afn(vm, NULL, argc, args));
                                break;
                            }
                        }
                    }
                    #endif

                    if(lefttype != APE_OBJECT_ARRAY && lefttype != APE_OBJECT_MAP && lefttype != APE_OBJECT_STRING)
                    {
                        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                            "type %s is not indexable (in OPCODE_GET_INDEX)", left_type_name);
                        goto err;
                    }
                    objres = object_make_null();
                    if(lefttype == APE_OBJECT_ARRAY)
                    {
                        if(indextype != APE_OBJECT_NUMBER)
                        {
                            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "cannot index %s with %s", left_type_name, indextypename);
                            goto err;
                        }
                        ix = (int)object_get_number(index);
                        if(ix < 0)
                        {
                            ix = object_get_array_length(left) + ix;
                        }
                        if(ix >= 0 && ix < object_get_array_length(left))
                        {
                            objres = object_get_array_value(left, ix);
                        }
                    }
                    else if(lefttype == APE_OBJECT_MAP)
                    {
                        objres = object_get_map_value_object(left, index);
                    }
                    else if(lefttype == APE_OBJECT_STRING)
                    {
                        str = object_get_string(left);
                        leftlen = object_get_string_length(left);
                        ix = (int)object_get_number(index);
                        if(ix >= 0 && ix < leftlen)
                        {
                            char res_str[2] = { str[ix], '\0' };
                            objres = object_make_string(vm->mem, res_str);
                        }
                    }
                    vmpriv_pushstack(vm, objres);
                }
                break;

            case OPCODE_GET_VALUE_AT:
            {
                index = vmpriv_popstack(vm);
                left = vmpriv_popstack(vm);
                lefttype = object_get_type(left);
                indextype = object_get_type(index);
                left_type_name = object_get_type_name(lefttype);
                indextypename = object_get_type_name(indextype);
                if(lefttype != APE_OBJECT_ARRAY && lefttype != APE_OBJECT_MAP && lefttype != APE_OBJECT_STRING)
                {
                    errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                      "type %s is not indexable (in OPCODE_GET_VALUE_AT)", left_type_name);
                    goto err;
                }
                objres = object_make_null();
                if(indextype != APE_OBJECT_NUMBER)
                {
                    errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                      "cannot index %s with %s", left_type_name, indextypename);
                    goto err;
                }
                ix = (int)object_get_number(index);
                if(lefttype == APE_OBJECT_ARRAY)
                {
                    objres = object_get_array_value(left, ix);
                }
                else if(lefttype == APE_OBJECT_MAP)
                {
                    objres = object_get_kv_pair_at(vm->mem, left, ix);
                }
                else if(lefttype == APE_OBJECT_STRING)
                {
                    str = object_get_string(left);
                    leftlen = object_get_string_length(left);
                    ix = (int)object_get_number(index);
                    if(ix >= 0 && ix < leftlen)
                    {
                        char res_str[2] = { str[ix], '\0' };
                        objres = object_make_string(vm->mem, res_str);
                    }
                }
                vmpriv_pushstack(vm, objres);
                break;
            }
            case OPCODE_CALL:
            {
                num_args = frame_read_uint8(vm->current_frame);
                callee = vmpriv_getstack(vm, num_args);
                ok = vmpriv_callobject(vm, callee, num_args);
                if(!ok)
                {
                    goto err;
                }
                break;
            }
            case OPCODE_RETURN_VALUE:
            {
                objres = vmpriv_popstack(vm);
                ok = vmpriv_popframe(vm);
                if(!ok)
                {
                    goto end;
                }
                vmpriv_pushstack(vm, objres);
                break;
            }
            case OPCODE_RETURN:
            {
                ok = vmpriv_popframe(vm);
                vmpriv_pushstack(vm, object_make_null());
                if(!ok)
                {
                    vmpriv_popstack(vm);
                    goto end;
                }
                break;
            }
            case OPCODE_DEFINE_LOCAL:
            {
                pos = frame_read_uint8(vm->current_frame);
                vm->stack[vm->current_frame->base_pointer + pos] = vmpriv_popstack(vm);
                break;
            }
            case OPCODE_SET_LOCAL:
            {
                pos = frame_read_uint8(vm->current_frame);
                new_value = vmpriv_popstack(vm);
                old_value = vm->stack[vm->current_frame->base_pointer + pos];
                if(!vmpriv_checkassign(vm, old_value, new_value))
                {
                    goto err;
                }
                vm->stack[vm->current_frame->base_pointer + pos] = new_value;
                break;
            }
            case OPCODE_GET_LOCAL:
            {
                pos = frame_read_uint8(vm->current_frame);
                objval = vm->stack[vm->current_frame->base_pointer + pos];
                vmpriv_pushstack(vm, objval);
                break;
            }
            case OPCODE_GET_APE_GLOBAL:
            {
                ix = frame_read_uint16(vm->current_frame);
                ok = false;
                objval = global_store_get_object_at(vm->global_store, ix, &ok);
                if(!ok)
                {
                    errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                      "global value %d not found", ix);
                    goto err;
                }
                vmpriv_pushstack(vm, objval);
                break;
            }
            case OPCODE_FUNCTION:
                {
                    constant_ix = frame_read_uint16(vm->current_frame);
                    num_free = frame_read_uint8(vm->current_frame);
                    constant = (ApeObject_t*)array_get(constants, constant_ix);
                    if(!constant)
                    {
                        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "constant %d not found", constant_ix);
                        goto err;
                    }
                    constant_type = object_get_type(*constant);
                    if(constant_type != APE_OBJECT_FUNCTION)
                    {
                        type_name = object_get_type_name(constant_type);
                        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "%s is not a function", type_name);
                        goto err;
                    }
                    constfunction = object_get_function(*constant);
                    function_obj = object_make_function(vm->mem, object_get_function_name(*constant), constfunction->comp_result,
                                           false, constfunction->num_locals, constfunction->num_args, num_free);
                    if(object_is_null(function_obj))
                    {
                        goto err;
                    }
                    for(i = 0; i < num_free; i++)
                    {
                        free_val = vm->stack[vm->sp - num_free + i];
                        object_set_function_free_val(function_obj, i, free_val);
                    }
                    vmpriv_setstackpointer(vm, vm->sp - num_free);
                    vmpriv_pushstack(vm, function_obj);
                }
                break;
            case OPCODE_GET_FREE:
                {
                    free_ix = frame_read_uint8(vm->current_frame);
                    objval = object_get_function_free_val(vm->current_frame->function, free_ix);
                    vmpriv_pushstack(vm, objval);
                }
                break;
            case OPCODE_SET_FREE:
                {
                    free_ix = frame_read_uint8(vm->current_frame);
                    objval = vmpriv_popstack(vm);
                    object_set_function_free_val(vm->current_frame->function, free_ix, objval);
                }
                break;
            case OPCODE_CURRENT_FUNCTION:
                {
                    current_function = vm->current_frame->function;
                    vmpriv_pushstack(vm, current_function);
                }
                break;
            case OPCODE_SET_INDEX:
                {
                    index = vmpriv_popstack(vm);
                    left = vmpriv_popstack(vm);
                    new_value = vmpriv_popstack(vm);
                    lefttype = object_get_type(left);
                    indextype = object_get_type(index);
                    left_type_name = object_get_type_name(lefttype);
                    indextypename = object_get_type_name(indextype);
                    if(lefttype != APE_OBJECT_ARRAY && lefttype != APE_OBJECT_MAP)
                    {
                        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "type %s is not indexable (in OPCODE_SET_INDEX)", left_type_name);
                        goto err;
                    }

                    if(lefttype == APE_OBJECT_ARRAY)
                    {
                        if(indextype != APE_OBJECT_NUMBER)
                        {
                            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "cannot index %s with %s", left_type_name, indextypename);
                            goto err;
                        }
                        ix = (int)object_get_number(index);
                        ok = object_set_array_value_at(left, ix, new_value);
                        if(!ok)
                        {
                            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                             "setting array item failed (index %d out of bounds of %d)", ix, object_get_array_length(left));
                            goto err;
                        }
                    }
                    else if(lefttype == APE_OBJECT_MAP)
                    {
                        old_value = object_get_map_value_object(left, index);
                        if(!vmpriv_checkassign(vm, old_value, new_value))
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
                    objval = vmpriv_getstack(vm, 0);
                    vmpriv_pushstack(vm, objval);
                }
                break;
            case OPCODE_LEN:
                {
                    objval = vmpriv_popstack(vm);
                    len = 0;
                    type = object_get_type(objval);
                    if(type == APE_OBJECT_ARRAY)
                    {
                        len = object_get_array_length(objval);
                    }
                    else if(type == APE_OBJECT_MAP)
                    {
                        len = object_get_map_length(objval);
                    }
                    else if(type == APE_OBJECT_STRING)
                    {
                        len = object_get_string_length(objval);
                    }
                    else
                    {
                        type_name = object_get_type_name(type);
                        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                          "cannot get length of %s", type_name);
                        goto err;
                    }
                    vmpriv_pushstack(vm, object_make_number(len));
                }
                break;
            case OPCODE_NUMBER:
                {
                    val = frame_read_uint64(vm->current_frame);
                    valdouble = ape_uint64_to_double(val);
                    ApeObject_t obj = object_make_number(valdouble);
                    vmpriv_pushstack(vm, obj);
                }
                break;
            case OPCODE_SET_RECOVER:
                {
                    recover_ip = frame_read_uint16(vm->current_frame);
                    vm->current_frame->recover_ip = recover_ip;
                }
                break;
            default:
                {
                    APE_ASSERT(false);
                    errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "unknown opcode: 0x%x", opcode);
                    goto err;
                }
                break;
        }
        if(checktime)
        {
            time_check_counter++;
            if(time_check_counter > time_check_interval)
            {
                elapsed_ms = (int)ape_timer_get_elapsed_ms(&timer);
                if(elapsed_ms > maxexecms)
                {
                    errors_add_errorf(vm->errors, APE_ERROR_TIMEOUT, frame_src_position(vm->current_frame),
                                      "execution took more than %1.17g ms", maxexecms);
                    goto err;
                }
                time_check_counter = 0;
            }
        }
    err:
        if(errors_get_count(vm->errors) > 0)
        {
            err = errors_get_last_error(vm->errors);
            if(err->type == APE_ERROR_RUNTIME && errors_get_count(vm->errors) == 1)
            {
                recover_frame_ix = -1;
                for(ui = vm->frames_count - 1; ui >= 0; ui--)
                {
                    frame = &vm->frames[ui];
                    if(frame->recover_ip >= 0 && !frame->is_recovering)
                    {
                        recover_frame_ix = ui;
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
                        vmpriv_popframe(vm);
                    }
                    err_obj = object_make_error(vm->mem, err->message);
                    if(!object_is_null(err_obj))
                    {
                        object_set_error_traceback(err_obj, err->traceback);
                        err->traceback = NULL;
                    }
                    vmpriv_pushstack(vm, err_obj);
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
            vmpriv_collectgarbage(vm, constants);
        }
    }
end:
    if(errors_get_count(vm->errors) > 0)
    {
        err = errors_get_last_error(vm->errors);
        if(!err->traceback)
        {
            err->traceback = traceback_make(vm->alloc);
        }
        if(err->traceback)
        {
            traceback_append_from_vm(err->traceback, vm);
        }
    }

    vmpriv_collectgarbage(vm, constants);

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

bool vm_set_global(ApeVM_t* vm, ApeSize_t ix, ApeObject_t val)
{
    if(ix >= VM_MAX_GLOBALS)
    {
        APE_ASSERT(false);
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "global write out of range");
        return false;
    }
    vm->globals[ix] = val;
    if(ix >= vm->globals_count)
    {
        vm->globals_count = ix + 1;
    }
    return true;
}

ApeObject_t vm_get_global(ApeVM_t* vm, ApeSize_t ix)
{
    if(ix >= VM_MAX_GLOBALS)
    {
        APE_ASSERT(false);
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "global read out of range");
        return object_make_null();
    }
    return vm->globals[ix];
}

// INTERNAL
static void vmpriv_setstackpointer(ApeVM_t* vm, int new_sp)
{
    if(new_sp > vm->sp)
    {// to avoid gcing freed objects
        int count = new_sp - vm->sp;
        size_t bytes_count = count * sizeof(ApeObject_t);
        memset(vm->stack + vm->sp, 0, bytes_count);
    }
    vm->sp = new_sp;
}

void vmpriv_pushstack(ApeVM_t* vm, ApeObject_t obj)
{
#ifdef APE_DEBUG
    if(vm->sp >= VM_STACK_SIZE)
    {
        APE_ASSERT(false);
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "stack overflow");
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

ApeObject_t vmpriv_popstack(ApeVM_t* vm)
{
#ifdef APE_DEBUG
    if(vm->sp == 0)
    {
        errors_add_error(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "stack underflow");
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
    ApeObject_t objres = vm->stack[vm->sp];
    vm->last_popped = objres;
    return objres;
}

ApeObject_t vmpriv_getstack(ApeVM_t* vm, int nth_item)
{
    int ix = vm->sp - 1 - nth_item;
#ifdef APE_DEBUG
    if(ix < 0 || ix >= VM_STACK_SIZE)
    {
        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "invalid stack index: %d", nth_item);
        APE_ASSERT(false);
        return object_make_null();
    }
#endif
    return vm->stack[ix];
}

static void vmpriv_pushthisstack(ApeVM_t* vm, ApeObject_t obj)
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

static ApeObject_t vmpriv_popthisstack(ApeVM_t* vm)
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

static ApeObject_t vmpriv_getthisstack(ApeVM_t* vm, int nth_item)
{
    int ix = vm->this_sp - 1 - nth_item;
#ifdef APE_DEBUG
    if(ix < 0 || ix >= VM_THIS_STACK_SIZE)
    {
        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "invalid this stack index: %d", nth_item);
        APE_ASSERT(false);
        return object_make_null();
    }
#endif
    return vm->this_stack[ix];
}

bool vmpriv_pushframe(ApeVM_t* vm, ApeFrame_t frame)
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
    vmpriv_setstackpointer(vm, frame.base_pointer + frame_function->num_locals);
    return true;
}

bool vmpriv_popframe(ApeVM_t* vm)
{
    vmpriv_setstackpointer(vm, vm->current_frame->base_pointer - 1);
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

void vmpriv_collectgarbage(ApeVM_t* vm, ApeArray_t * constants)
{
    ApeSize_t i;
    ApeFrame_t* frame;
    gc_unmark_all(vm->mem);
    gc_mark_objects(global_store_get_object_data(vm->global_store), global_store_get_object_count(vm->global_store));
    gc_mark_objects((ApeObject_t*)array_data(constants), array_count(constants));
    gc_mark_objects(vm->globals, vm->globals_count);
    for(i = 0; i < vm->frames_count; i++)
    {
        frame = &vm->frames[i];
        gc_mark_object(frame->function);
    }
    gc_mark_objects(vm->stack, vm->sp);
    gc_mark_objects(vm->this_stack, vm->this_sp);
    gc_mark_object(vm->last_popped);
    gc_mark_objects(vm->operator_oveload_keys, OPCODE_MAX);
    gc_sweep(vm->mem);
}

bool vmpriv_callobject(ApeVM_t* vm, ApeObject_t callee, int num_args)
{
    bool ok;
    ApeObjectType_t callee_type;
    ApeFunction_t* callee_function;
    ApeFrame_t callee_frame;
    ApeObject_t* stack_pos;
    ApeObject_t objres;
    const char* callee_type_name;
    callee_type = object_get_type(callee);
    if(callee_type == APE_OBJECT_FUNCTION)
    {
        callee_function = object_get_function(callee);
        if(num_args != callee_function->num_args)
        {
            errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame),
                              "invalid number of arguments to \"%s\", expected %d, got %d",
                              object_get_function_name(callee), callee_function->num_args, num_args);
            return false;
        }
        ok = frame_init(&callee_frame, callee, vm->sp - num_args);
        if(!ok)
        {
            errors_add_error(vm->errors, APE_ERROR_RUNTIME, g_vmpriv_src_pos_invalid, "frame init failed in vmpriv_callobject");
            return false;
        }
        ok = vmpriv_pushframe(vm, callee_frame);
        if(!ok)
        {
            errors_add_error(vm->errors, APE_ERROR_RUNTIME, g_vmpriv_src_pos_invalid, "pushing frame failed in vmpriv_callobject");
            return false;
        }
    }
    else if(callee_type == APE_OBJECT_NATIVE_FUNCTION)
    {
        stack_pos = vm->stack + vm->sp - num_args;
        objres = vmpriv_callnativefunction(vm, callee, frame_src_position(vm->current_frame), num_args, stack_pos);
        if(vm_has_errors(vm))
        {
            return false;
        }
        vmpriv_setstackpointer(vm, vm->sp - num_args - 1);
        vmpriv_pushstack(vm, objres);
    }
    else
    {
        callee_type_name = object_get_type_name(callee_type);
        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "%s object is not callable", callee_type_name);
        return false;
    }
    return true;
}

ApeObject_t vmpriv_callnativefunction(ApeVM_t* vm, ApeObject_t callee, ApePosition_t src_pos, int argc, ApeObject_t* args)
{
    ApeNativeFunction_t* native_fun = object_get_native_function(callee);
    ApeObject_t objres = native_fun->native_funcptr(vm, native_fun->data, argc, args);
    if(errors_has_errors(vm->errors) && !APE_STREQ(native_fun->name, "crash"))
    {
        ApeError_t* err = errors_get_last_error(vm->errors);
        err->pos = src_pos;
        err->traceback = traceback_make(vm->alloc);
        if(err->traceback)
        {
            traceback_append(err->traceback, native_fun->name, g_vmpriv_src_pos_invalid);
        }
        return object_make_null();
    }
    ApeObjectType_t res_type = object_get_type(objres);
    if(res_type == APE_OBJECT_ERROR)
    {
        ApeTraceback_t* traceback = traceback_make(vm->alloc);
        if(traceback)
        {
            // error builtin is treated in a special way
            if(!APE_STREQ(native_fun->name, "error"))
            {
                traceback_append(traceback, native_fun->name, g_vmpriv_src_pos_invalid);
            }
            traceback_append_from_vm(traceback, vm);
            object_set_error_traceback(objres, traceback);
        }
    }
    return objres;
}

bool vmpriv_checkassign(ApeVM_t* vm, ApeObject_t old_value, ApeObject_t new_value)
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
        errors_add_errorf(vm->errors, APE_ERROR_RUNTIME, frame_src_position(vm->current_frame), "trying to assign variable of type %s to %s",
                          object_get_type_name(new_value_type), object_get_type_name(old_value_type));
        return false;
        */
    }
    return true;
}

static bool vmpriv_tryoverloadoperator(ApeVM_t* vm, ApeObject_t left, ApeObject_t right, ApeOpByte_t op, bool* out_overload_found)
{
    int num_operands;
    ApeObject_t key;
    ApeObject_t callee;
    ApeObjectType_t lefttype;
    ApeObjectType_t righttype;
    *out_overload_found = false;
    lefttype = object_get_type(left);
    righttype = object_get_type(right);
    if(lefttype != APE_OBJECT_MAP && righttype != APE_OBJECT_MAP)
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
    if(lefttype == APE_OBJECT_MAP)
    {
        callee = object_get_map_value_object(left, key);
    }
    if(!object_is_callable(callee))
    {
        if(righttype == APE_OBJECT_MAP)
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
    vmpriv_pushstack(vm, callee);
    vmpriv_pushstack(vm, left);
    if(num_operands == 2)
    {
        vmpriv_pushstack(vm, right);
    }
    return vmpriv_callobject(vm, callee, num_operands);
}

//-----------------------------------------------------------------------------
// Ape
//-----------------------------------------------------------------------------
ApeContext_t* ape_make(void)
{
    return ape_make_ex(NULL, NULL, NULL);
}

ApeContext_t* ape_make_ex(ApeMemAllocFunc_t malloc_fn, ApeMemFreeFunc_t free_fn, void* ctx)
{
    ApeAllocator_t custom_alloc = allocator_make((ApeMemAllocFunc_t)malloc_fn, (ApeMemFreeFunc_t)free_fn, ctx);

    ApeContext_t* ape = (ApeContext_t*)allocator_malloc(&custom_alloc, sizeof(ApeContext_t));
    if(!ape)
    {
        return NULL;
    }

    memset(ape, 0, sizeof(ApeContext_t));
    ape->alloc = allocator_make(ape_malloc, ape_free, ape);
    ape->custom_allocator = custom_alloc;

    vmpriv_setdefaultconfig(ape);

    vmpriv_initerrors(&ape->errors);

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

bool ape_set_timeout(ApeContext_t* ape, ApeFloat_t max_execution_time_ms)
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

void ape_set_stdout_write_function(ApeContext_t* ape, ApeIOStdoutWriteFunc_t stdout_write, void* context)
{
    ape->config.stdio.write.write = stdout_write;
    ape->config.stdio.write.context = context;
}

void ape_set_file_write_function(ApeContext_t* ape, ApeIOWriteFunc_t file_write, void* context)
{
    ape->config.fileio.write_file.write_file = file_write;
    ape->config.fileio.write_file.context = context;
}

void ape_set_file_read_function(ApeContext_t* ape, ApeIOReadFunc_t file_read, void* context)
{
    ape->config.fileio.read_file.read_file = file_read;
    ape->config.fileio.read_file.context = context;
}


ApeObject_t ape_execute(ApeContext_t* ape, const char* code)
{
    bool ok;
    ApeObject_t objres;
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
    objres = vm_get_last_popped(ape->vm);
    if(object_get_type(objres) == APE_OBJECT_NONE)
    {
        goto err;
    }
    compilation_result_destroy(comp_res);
    return objres;

err:
    compilation_result_destroy(comp_res);
    return object_make_null();
}

ApeObject_t ape_execute_file(ApeContext_t* ape, const char* path)
{
    bool ok;
    ApeObject_t objres;
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
    objres = vm_get_last_popped(ape->vm);
    if(object_get_type(objres) == APE_OBJECT_NONE)
    {
        goto err;
    }
    compilation_result_destroy(comp_res);

    return objres;

err:
    compilation_result_destroy(comp_res);
    return object_make_null();
}



bool ape_has_errors(const ApeContext_t* ape)
{
    return ape_errors_count(ape) > 0;
}

ApeSize_t ape_errors_count(const ApeContext_t* ape)
{
    return errors_get_count(&ape->errors);
}

void ape_clear_errors(ApeContext_t* ape)
{
    errors_clear(&ape->errors);
}

const ApeError_t* ape_get_error(const ApeContext_t* ape, int index)
{
    return (const ApeError_t*)vmpriv_errorsgetc(&ape->errors, index);
}

bool ape_set_native_function(ApeContext_t* ape, const char* name, ApeNativeWrapFunc_t fn, void* data)
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

//-----------------------------------------------------------------------------
// Ape object array
//-----------------------------------------------------------------------------


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

bool ape_object_set_map_number(ApeObject_t obj, const char* key, ApeFloat_t number)
{
    ApeObject_t number_object = object_make_number(number);
    return ape_object_set_map_value(obj, key, number_object);
}

bool ape_object_set_map_bool(ApeObject_t obj, const char* key, bool value)
{
    ApeObject_t bool_object = object_make_bool(value);
    return ape_object_set_map_value(obj, key, bool_object);
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
        strbuf_appendf(buf, "traceback:\n");
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
// Ape internal
//-----------------------------------------------------------------------------
void ape_deinit(ApeContext_t* ape)
{
    vm_destroy(ape->vm);
    compiler_destroy(ape->compiler);
    global_store_destroy(ape->global_store);
    gcmem_destroy(ape->mem);
    ptrarray_destroy_with_items(ape->files, (ApeDataCallback_t)compiled_file_destroy);
    vmpriv_destroyerrors(&ape->errors);
}

static ApeObject_t vmpriv_wrapnativefunc(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    ApeObject_t objres;
    ApeNativeFuncWrapper_t* wrapper;
    (void)vm;
    wrapper = (ApeNativeFuncWrapper_t*)data;
    APE_ASSERT(vm == wrapper->ape->vm);
    objres = wrapper->wrapped_funcptr(wrapper->ape, wrapper->data, argc, (ApeObject_t*)args);
    if(ape_has_errors(wrapper->ape))
    {
        return object_make_null();
    }
    return objres;
}

ApeObject_t ape_object_make_native_function_with_name(ApeContext_t* ape, const char* name, ApeNativeWrapFunc_t fn, void* data)
{
    ApeNativeFuncWrapper_t wrapper;
    memset(&wrapper, 0, sizeof(ApeNativeFuncWrapper_t));
    wrapper.wrapped_funcptr = fn;
    wrapper.ape = ape;
    wrapper.data = data;
    ApeObject_t wrapper_native_function = object_make_native_function_memory(ape->mem, name, vmpriv_wrapnativefunc, &wrapper, sizeof(wrapper));
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

static void vmpriv_setdefaultconfig(ApeContext_t* ape)
{
    memset(&ape->config, 0, sizeof(ApeConfig_t));
    ape_set_repl_mode(ape, false);
    ape_set_timeout(ape, -1);
    ape_set_file_read_function(ape, vmpriv_default_readfile, ape);
    ape_set_file_write_function(ape, vmpriv_default_writefile, ape);
    ape_set_stdout_write_function(ape, vmpriv_default_stdout_write, ape);
}

static char* vmpriv_default_readfile(void* ctx, const char* filename)
{
    long pos;
    size_t size_read;
    size_t size_to_read;
    char* file_contents;
    FILE* fp;
    ApeContext_t* ape;
    ape = (ApeContext_t*)ctx;
    fp = fopen(filename, "r");
    size_to_read = 0;
    size_read = 0;
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

static size_t vmpriv_default_writefile(void* ctx, const char* path, const char* string, size_t string_size)
{
    size_t written;
    FILE* fp;
    (void)ctx;
    fp = fopen(path, "w");
    if(!fp)
    {
        return 0;
    }
    written = fwrite(string, 1, string_size, fp);
    fclose(fp);
    return written;
}

static size_t vmpriv_default_stdout_write(void* ctx, const void* data, size_t size)
{
    (void)ctx;
    return fwrite(data, 1, size, stdout);
}

void* ape_malloc(void* ctx, size_t size)
{
    void* resptr;
    ApeContext_t* ape;
    ape = (ApeContext_t*)ctx;
    resptr = (void*)allocator_malloc(&ape->custom_allocator, size);
    if(!resptr)
    {
        errors_add_error(&ape->errors, APE_ERROR_ALLOCATION, g_vmpriv_src_pos_invalid, "allocation failed");
    }
    return resptr;
}

void ape_free(void* ctx, void* ptr)
{
    ApeContext_t* ape;
    ape = (ApeContext_t*)ctx;
    allocator_free(&ape->custom_allocator, ptr);
}
