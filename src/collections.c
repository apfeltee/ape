/*
Copyright (c) 2020 Krzysztof Gabis
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

#ifndef COLLECTIONS_AMALGAMATED
#include "collections.h"
#endif

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

#ifdef COLLECTIONS_DEBUG
#include <assert.h>
#define COLLECTIONS_ASSERT(x) assert(x)
#else
#define COLLECTIONS_ASSERT(x)
#endif

//-----------------------------------------------------------------------------
// Collections
//-----------------------------------------------------------------------------

#undef malloc
#undef free

static char* collections_strndup(const char *string, size_t n);
static char* collections_strdup(const char *string);
static unsigned long collections_hash(const void *ptr, size_t len); /* djb2 */
static unsigned int upper_power_of_two(unsigned int v);

static collections_malloc_fn collections_malloc = malloc;
static collections_free_fn collections_free = free;

void collections_set_memory_functions(collections_malloc_fn malloc_fn, collections_free_fn free_fn) {
    collections_malloc = malloc_fn;
    collections_free = free_fn;
}

static char* collections_strndup(const char *string, size_t n) {
    char *output_string = (char*)collections_malloc(n + 1);
    if (!output_string) {
        return NULL;
    }
    output_string[n] = '\0';
    memcpy(output_string, string, n);
    return output_string;
}

static char* collections_strdup(const char *string) {
    return collections_strndup(string, strlen(string));
}

static unsigned long collections_hash(const void *ptr, size_t len) { /* djb2 */
    const uint8_t *ptr_u8 = (const uint8_t*)ptr;
    unsigned long hash = 5381;
    for (size_t i = 0; i < len; i++) {
        uint8_t val = ptr_u8[i];
        hash = ((hash << 5) + hash) + val;
    }
    return hash;
}

static unsigned int upper_power_of_two(unsigned int v) {
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
// Dictionary
//-----------------------------------------------------------------------------

#define DICT_INVALID_IX UINT_MAX
#define DICT_INITIAL_SIZE 32

typedef struct dict_ {
    unsigned int *cells;
    unsigned long *hashes;
    char **keys;
    void **values;
    unsigned int *cell_ixs;
    unsigned int count;
    unsigned int item_capacity;
    unsigned int cell_capacity;
} dict_t_;

// Private declarations
static bool dict_init(dict_t_ *dict, unsigned int initial_capacity);
static void dict_deinit(dict_t_ *dict, bool free_keys);
static unsigned int dict_get_cell_ix(const dict_t_ *dict,
                                     const char *key,
                                     unsigned long hash,
                                     bool *out_found);
static unsigned long hash_string(const char *str);
static bool dict_grow_and_rehash(dict_t_ *dict);
static bool dict_set_internal(dict_t_ *dict, const char *ckey, char *mkey, void *value);

// Public
dict_t_* dict_make(void) {
    dict_t_ *dict = collections_malloc(sizeof(dict_t_));
    if (dict == NULL) {
        return NULL;
    }
    bool succeeded = dict_init(dict, DICT_INITIAL_SIZE);
    if (succeeded == false) {
        collections_free(dict);
        return NULL;
    }
    return dict;
}

void dict_destroy(dict_t_ *dict) {
    if (dict == NULL) {
        return;
    }
    dict_deinit(dict, true);
    collections_free(dict);
}

void dict_destroy_with_items_(dict_t_ *dict, dict_item_destroy_fn destroy_fn) {
    if (dict == NULL) {
        return;
    }

    if (destroy_fn) {
        for (unsigned int i = 0; i < dict->count; i++) {
            destroy_fn(dict->values[i]);
        }
    }

    dict_destroy(dict);
}

dict_t_* dict_copy_with_items_(dict_t_ *dict, dict_item_copy_fn copy_fn) {
    dict_t_ *dict_copy = dict_make();
    for (int i = 0; i < dict_count(dict); i++) {
        const char *key = dict_get_key_at(dict, i);
        void *item = dict_get_value_at(dict, i);
        void *item_copy = copy_fn(item);
        dict_set(dict_copy, key, item_copy);
    }
    return dict_copy;
}

bool dict_set(dict_t_ *dict, const char *key, void *value) {
    return dict_set_internal(dict, key, NULL, value);
}

void* dict_get(const dict_t_ *dict, const char *key) {
    unsigned long hash = hash_string(key);
    bool found = false;
    unsigned long cell_ix = dict_get_cell_ix(dict, key, hash, &found);
    if (found == false) {
        return NULL;
    }
    unsigned int item_ix = dict->cells[cell_ix];
    return dict->values[item_ix];
}

void *dict_get_value_at(const dict_t_ *dict, unsigned int ix) {
    if (ix >= dict->count) {
        return NULL;
    }
    return dict->values[ix];
}

const char *dict_get_key_at(const dict_t_ *dict, unsigned int ix) {
    if (ix >= dict->count) {
        return NULL;
    }
    return dict->keys[ix];
}

int dict_count(const dict_t_ *dict) {
    if (!dict) {
        return 0;
    }
    return dict->count;
}

bool dict_remove(dict_t_ *dict, const char *key) {
    unsigned long hash = hash_string(key);
    bool found = false;
    unsigned int cell = dict_get_cell_ix(dict, key, hash, &found);
    if (!found) {
        return false;
    }

    unsigned int item_ix = dict->cells[cell];
    collections_free(dict->keys[item_ix]);
    unsigned int last_item_ix = dict->count - 1;
    if (item_ix < last_item_ix) {
        dict->keys[item_ix] = dict->keys[last_item_ix];
        dict->values[item_ix] = dict->values[last_item_ix];
        dict->cell_ixs[item_ix] = dict->cell_ixs[last_item_ix];
        dict->hashes[item_ix] = dict->hashes[last_item_ix];
        dict->cells[dict->cell_ixs[item_ix]] = item_ix;
    }
    dict->count--;

    unsigned int i = cell;
    unsigned int j = i;
    for (unsigned int x = 0; x < (dict->cell_capacity - 1); x++) {
        j = (j + 1) & (dict->cell_capacity - 1);
        if (dict->cells[j] == DICT_INVALID_IX) {
            break;
        }
        unsigned int k = dict->hashes[dict->cells[j]] & (dict->cell_capacity - 1);
        if ((j > i && (k <= i || k > j))
            || (j < i && (k <= i && k > j))) {
            dict->cell_ixs[dict->cells[j]] = i;
            dict->cells[i] = dict->cells[j];
            i = j;
        }
    }
    dict->cells[i] = DICT_INVALID_IX;
    return true;
}

void dict_clear(dict_t_ *dict) {
    for (unsigned int i = 0; i < dict->count; i++) {
        collections_free(dict->keys[i]);
    }
    dict->count = 0;
    for (unsigned int i = 0; i < dict->cell_capacity; i++) {
        dict->cells[i] = DICT_INVALID_IX;
    }
}

// Private definitions
static bool dict_init(dict_t_ *dict, unsigned int initial_capacity) {
    dict->cells = NULL;
    dict->keys = NULL;
    dict->values = NULL;
    dict->cell_ixs = NULL;
    dict->hashes = NULL;

    dict->count = 0;
    dict->cell_capacity = initial_capacity;
    dict->item_capacity = (unsigned int)(initial_capacity * 0.7f);

    dict->cells = collections_malloc(dict->cell_capacity * sizeof(*dict->cells));
    dict->keys = collections_malloc(dict->item_capacity * sizeof(*dict->keys));
    dict->values = collections_malloc(dict->item_capacity * sizeof(*dict->values));
    dict->cell_ixs = collections_malloc(dict->item_capacity * sizeof(*dict->cell_ixs));
    dict->hashes = collections_malloc(dict->item_capacity * sizeof(*dict->hashes));
    if (dict->cells == NULL
        || dict->keys == NULL
        || dict->values == NULL
        || dict->cell_ixs == NULL
        || dict->hashes == NULL) {
        goto error;
    }
    for (unsigned int i = 0; i < dict->cell_capacity; i++) {
        dict->cells[i] = DICT_INVALID_IX;
    }
    return true;
error:
    collections_free(dict->cells);
    collections_free(dict->keys);
    collections_free(dict->values);
    collections_free(dict->cell_ixs);
    collections_free(dict->hashes);
    return false;
}

static void dict_deinit(dict_t_ *dict, bool free_keys) {
    if (free_keys) {
        for (unsigned int i = 0; i < dict->count; i++) {
            collections_free(dict->keys[i]);
        }
    }
    dict->count = 0;
    dict->item_capacity = 0;
    dict->cell_capacity = 0;

    collections_free(dict->cells);
    collections_free(dict->keys);
    collections_free(dict->values);
    collections_free(dict->cell_ixs);
    collections_free(dict->hashes);

    dict->cells = NULL;
    dict->keys = NULL;
    dict->values = NULL;
    dict->cell_ixs = NULL;
    dict->hashes = NULL;
}

static unsigned int dict_get_cell_ix(const dict_t_ *dict,
                                     const char *key,
                                     unsigned long hash,
                                     bool *out_found)
{
    *out_found = false;
    unsigned int cell_ix = hash & (dict->cell_capacity - 1);
    for (unsigned int i = 0; i < dict->cell_capacity; i++) {
        unsigned int ix = (cell_ix + i) & (dict->cell_capacity - 1);
        unsigned int cell = dict->cells[ix];
        if (cell == DICT_INVALID_IX) {
            return ix;
        }
        unsigned long hash_to_check = dict->hashes[cell];
        if (hash != hash_to_check) {
            continue;
        }
        const char *key_to_check = dict->keys[cell];
        if (strcmp(key, key_to_check) == 0) {
            *out_found = true;
            return ix;
        }
    }
    return DICT_INVALID_IX;
}

static unsigned long hash_string(const char *str) { /* djb2 */
    unsigned long hash = 5381;
    uint8_t c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

static bool dict_grow_and_rehash(dict_t_ *dict) {
    dict_t_ new_dict;
    bool succeeded = dict_init(&new_dict, dict->cell_capacity * 2);
    if (succeeded == false) {
        return false;
    }
    for (unsigned int i = 0; i < dict->count; i++) {
        char *key = dict->keys[i];
        void *value = dict->values[i];
        succeeded = dict_set_internal(&new_dict, key, key, value);
        if (succeeded == false) {
            dict_deinit(&new_dict, false);
            return false;
        }
    }
    dict_deinit(dict, false);
    *dict = new_dict;
    return true;
}

static bool dict_set_internal(dict_t_ *dict, const char *ckey, char *mkey, void *value) {
    unsigned long hash = hash_string(ckey);
    bool found = false;
    unsigned int cell_ix = dict_get_cell_ix(dict, ckey, hash, &found);
    if (found) {
        unsigned int item_ix = dict->cells[cell_ix];
        dict->values[item_ix] = value;
        return true;
    }
    if (dict->count >= dict->item_capacity) {
        bool succeeded = dict_grow_and_rehash(dict);
        if (succeeded == false) {
            return false;
        }
        cell_ix = dict_get_cell_ix(dict, ckey, hash, &found);
    }
    dict->cells[cell_ix] = dict->count;
    dict->keys[dict->count] = mkey != NULL ? mkey : collections_strdup(ckey);
    dict->values[dict->count] = value;
    dict->cell_ixs[dict->count] = cell_ix;
    dict->hashes[dict->count] = hash;
    dict->count++;
    return true;
}
//-----------------------------------------------------------------------------
// Value dictionary
//-----------------------------------------------------------------------------
#define VALDICT_INVALID_IX UINT_MAX

typedef struct valdict_ {
    size_t key_size;
    size_t val_size;
    unsigned int *cells;
    unsigned long *hashes;
    void *keys;
    void *values;
    unsigned int *cell_ixs;
    unsigned int count;
    unsigned int item_capacity;
    unsigned int cell_capacity;
    collections_hash_fn _hash_key;
    collections_equals_fn _keys_equals;
} valdict_t_;

// Private declarations
static bool valdict_init(valdict_t_ *dict, size_t key_size, size_t val_size, unsigned int initial_capacity);
static void valdict_deinit(valdict_t_ *dict);
static unsigned int valdict_get_cell_ix(const valdict_t_ *dict,
                                        const void *key,
                                        unsigned long hash,
                                        bool *out_found);
static bool valdict_grow_and_rehash(valdict_t_ *dict);
static bool valdict_set_key_at(valdict_t_ *dict, unsigned int ix, void *key);
static bool valdict_keys_are_equal(const valdict_t_ *dict, const void *a, const void *b);
static unsigned long valdict_hash_key(const valdict_t_ *dict, const void *key);

// Public
valdict_t_* valdict_make_(size_t key_size, size_t val_size) {
    return valdict_make_with_capacity(DICT_INITIAL_SIZE, key_size, val_size);
}

valdict_t_* valdict_make_with_capacity(unsigned int min_capacity, size_t key_size, size_t val_size) {
    unsigned int capacity = upper_power_of_two(min_capacity * 2);

    valdict_t_ *dict = collections_malloc(sizeof(valdict_t_));
    if (dict == NULL) {
        return NULL;
    }
    bool succeeded = valdict_init(dict, key_size, val_size, capacity);
    if (succeeded == false) {
        collections_free(dict);
        return NULL;
    }
    return dict;
}

void valdict_destroy(valdict_t_ *dict) {
    if (dict == NULL) {
        return;
    }
    valdict_deinit(dict);
    collections_free(dict);
}

void  valdict_set_hash_function(valdict_t_ *dict, collections_hash_fn hash_fn) {
    dict->_hash_key = hash_fn;
}

void valdict_set_equals_function(valdict_t_ *dict, collections_equals_fn equals_fn) {
    dict->_keys_equals = equals_fn;
}

bool valdict_set(valdict_t_ *dict, void *key, void *value) {
    unsigned long hash = valdict_hash_key(dict, key);
    bool found = false;
    unsigned int cell_ix = valdict_get_cell_ix(dict, key, hash, &found);
    if (found) {
        unsigned int item_ix = dict->cells[cell_ix];
        valdict_set_value_at(dict, item_ix, value);
        return true;
    }
    if (dict->count >= dict->item_capacity) {
        bool succeeded = valdict_grow_and_rehash(dict);
        if (succeeded == false) {
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

void* valdict_get(const valdict_t_ *dict, const void *key) {
    unsigned long hash = valdict_hash_key(dict, key);
    bool found = false;
    unsigned long cell_ix = valdict_get_cell_ix(dict, key, hash, &found);
    if (found == false) {
        return NULL;
    }
    unsigned int item_ix = dict->cells[cell_ix];
    return valdict_get_value_at(dict, item_ix);
}

void* valdict_get_key_at(const valdict_t_ *dict, unsigned int ix) {
    if (ix >= dict->count) {
        return NULL;
    }
    return (char*)dict->keys + (dict->key_size * ix);
}

void *valdict_get_value_at(const valdict_t_ *dict, unsigned int ix) {
    if (ix >= dict->count) {
        return NULL;
    }
    return (char*)dict->values + (dict->val_size * ix);
}

bool valdict_set_value_at(const valdict_t_ *dict, unsigned int ix, const void *value) {
    if (ix >= dict->count) {
        return false;
    }
    size_t offset = ix * dict->val_size;
    memcpy((char*)dict->values + offset, value, dict->val_size);
    return true;
}

int valdict_count(const valdict_t_ *dict) {
    if (!dict) {
        return 0;
    }
    return dict->count;
}

bool valdict_remove(valdict_t_ *dict, void *key) {
    unsigned long hash = valdict_hash_key(dict, key);
    bool found = false;
    unsigned int cell = valdict_get_cell_ix(dict, key, hash, &found);
    if (!found) {
        return false;
    }

    unsigned int item_ix = dict->cells[cell];
    unsigned int last_item_ix = dict->count - 1;
    if (item_ix < last_item_ix) {
        void *last_key = valdict_get_key_at(dict, last_item_ix);
        valdict_set_key_at(dict, item_ix, last_key);
        void *last_value = valdict_get_key_at(dict, last_item_ix);
        valdict_set_value_at(dict, item_ix, last_value);
        dict->cell_ixs[item_ix] = dict->cell_ixs[last_item_ix];
        dict->hashes[item_ix] = dict->hashes[last_item_ix];
        dict->cells[dict->cell_ixs[item_ix]] = item_ix;
    }
    dict->count--;

    unsigned int i = cell;
    unsigned int j = i;
    for (unsigned int x = 0; x < (dict->cell_capacity - 1); x++) {
        j = (j + 1) & (dict->cell_capacity - 1);
        if (dict->cells[j] == VALDICT_INVALID_IX) {
            break;
        }
        unsigned int k = dict->hashes[dict->cells[j]] & (dict->cell_capacity - 1);
        if ((j > i && (k <= i || k > j))
            || (j < i && (k <= i && k > j))) {
            dict->cell_ixs[dict->cells[j]] = i;
            dict->cells[i] = dict->cells[j];
            i = j;
        }
    }
    dict->cells[i] = VALDICT_INVALID_IX;
    return true;
}

void valdict_clear(valdict_t_ *dict) {
    dict->count = 0;
    for (unsigned int i = 0; i < dict->cell_capacity; i++) {
        dict->cells[i] = VALDICT_INVALID_IX;
    }
}

// Private definitions
static bool valdict_init(valdict_t_ *dict, size_t key_size, size_t val_size, unsigned int initial_capacity) {
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

    dict->cells = collections_malloc(dict->cell_capacity * sizeof(*dict->cells));
    dict->keys = collections_malloc(dict->item_capacity * key_size);
    dict->values = collections_malloc(dict->item_capacity * val_size);
    dict->cell_ixs = collections_malloc(dict->item_capacity * sizeof(*dict->cell_ixs));
    dict->hashes = collections_malloc(dict->item_capacity * sizeof(*dict->hashes));
    if (dict->cells == NULL
        || dict->keys == NULL
        || dict->values == NULL
        || dict->cell_ixs == NULL
        || dict->hashes == NULL) {
        goto error;
    }
    for (unsigned int i = 0; i < dict->cell_capacity; i++) {
        dict->cells[i] = VALDICT_INVALID_IX;
    }
    return true;
error:
    collections_free(dict->cells);
    collections_free(dict->keys);
    collections_free(dict->values);
    collections_free(dict->cell_ixs);
    collections_free(dict->hashes);
    return false;
}

static void valdict_deinit(valdict_t_ *dict) {
    dict->key_size = 0;
    dict->val_size = 0;
    dict->count = 0;
    dict->item_capacity = 0;
    dict->cell_capacity = 0;

    collections_free(dict->cells);
    collections_free(dict->keys);
    collections_free(dict->values);
    collections_free(dict->cell_ixs);
    collections_free(dict->hashes);

    dict->cells = NULL;
    dict->keys = NULL;
    dict->values = NULL;
    dict->cell_ixs = NULL;
    dict->hashes = NULL;
}

static unsigned int valdict_get_cell_ix(const valdict_t_ *dict,
                                        const void *key,
                                        unsigned long hash,
                                        bool *out_found)
{
    *out_found = false;
    unsigned int cell_ix = hash & (dict->cell_capacity - 1);
    for (unsigned int i = 0; i < dict->cell_capacity; i++) {
        unsigned int ix = (cell_ix + i) & (dict->cell_capacity - 1);
        unsigned int cell = dict->cells[ix];
        if (cell == VALDICT_INVALID_IX) {
            return ix;
        }
        unsigned long hash_to_check = dict->hashes[cell];
        if (hash != hash_to_check) {
            continue;
        }
        void *key_to_check = valdict_get_key_at(dict, cell);
        bool are_equal = valdict_keys_are_equal(dict, key, key_to_check);
        if (are_equal) {
            *out_found = true;
            return ix;
        }
    }
    return VALDICT_INVALID_IX;
}

static bool valdict_grow_and_rehash(valdict_t_ *dict) {
    valdict_t_ new_dict;
    unsigned new_capacity = dict->cell_capacity == 0 ? DICT_INITIAL_SIZE : dict->cell_capacity * 2;
    bool succeeded = valdict_init(&new_dict, dict->key_size, dict->val_size, new_capacity);
    if (succeeded == false) {
        return false;
    }
    new_dict._keys_equals = dict->_keys_equals;
    new_dict._hash_key = dict->_hash_key;
    for (unsigned int i = 0; i < dict->count; i++) {
        char *key = valdict_get_key_at(dict, i);
        void *value = valdict_get_value_at(dict, i);
        succeeded = valdict_set(&new_dict, key, value);
        if (succeeded == false) {
            valdict_deinit(&new_dict);
            return false;
        }
    }
    valdict_deinit(dict);
    *dict = new_dict;
    return true;
}

static bool valdict_set_key_at(valdict_t_ *dict, unsigned int ix, void *key) {
    if (ix >= dict->count) {
        return false;
    }
    size_t offset = ix * dict->key_size;
    memcpy((char*)dict->keys + offset, key, dict->key_size);
    return true;
}

static bool valdict_keys_are_equal(const valdict_t_ *dict, const void *a, const void *b) {
    if (dict->_keys_equals) {
        return dict->_keys_equals(a, b);
    } else {
        return memcmp(a, b, dict->key_size) == 0;
    }
}

static unsigned long valdict_hash_key(const valdict_t_ *dict, const void *key) {
    if (dict->_hash_key) {
        return dict->_hash_key(key);
    } else {
        return collections_hash(key, dict->key_size);
    }
}

//-----------------------------------------------------------------------------
// Pointer dictionary
//-----------------------------------------------------------------------------
typedef struct ptrdict_ {
    valdict_t_ vd;
} ptrdict_t_;

// Public
ptrdict_t_* ptrdict_make(void) {
    ptrdict_t_ *dict = collections_malloc(sizeof(ptrdict_t_));
    if (dict == NULL) {
        return NULL;
    }
    bool succeeded = valdict_init(&dict->vd, sizeof(void*), sizeof(void*), DICT_INITIAL_SIZE);
    if (succeeded == false) {
        collections_free(dict);
        return NULL;
    }
    return dict;
}

void ptrdict_destroy(ptrdict_t_ *dict) {
    if (dict == NULL) {
        return;
    }
    valdict_deinit(&dict->vd);
    collections_free(dict);
}

void  ptrdict_set_hash_function(ptrdict_t_ *dict, collections_hash_fn hash_fn) {
    valdict_set_hash_function(&dict->vd, hash_fn);
}

void ptrdict_set_equals_function(ptrdict_t_ *dict, collections_equals_fn equals_fn) {
    valdict_set_equals_function(&dict->vd, equals_fn);
}

bool ptrdict_set(ptrdict_t_ *dict, void *key, void *value) {
    return valdict_set(&dict->vd, &key, &value);
}

void* ptrdict_get(const ptrdict_t_ *dict, const void *key) {
    void* res = valdict_get(&dict->vd, &key);
    if (!res) {
        return NULL;
    }
    return *(void**)res;
}

void* ptrdict_get_value_at(const ptrdict_t_ *dict, unsigned int ix) {
    void* res = valdict_get_value_at(&dict->vd, ix);
    if (!res) {
        return NULL;
    }
    return *(void**)res;
}

void* ptrdict_get_key_at(const ptrdict_t_ *dict, unsigned int ix) {
    void* res = valdict_get_key_at(&dict->vd, ix);
    if (!res) {
        return NULL;
    }
    return *(void**)res;
}

int ptrdict_count(const ptrdict_t_ *dict) {
    return valdict_count(&dict->vd);
}

bool ptrdict_remove(ptrdict_t_ *dict, void *key) {
    return valdict_remove(&dict->vd, &key);
}

void ptrdict_clear(ptrdict_t_ *dict) {
    valdict_clear(&dict->vd);
}

//-----------------------------------------------------------------------------
// Array
//-----------------------------------------------------------------------------

typedef struct array_ {
    unsigned char *data;
    unsigned int count;
    unsigned int capacity;
    size_t element_size;
    bool lock_capacity;
} array_t_;

static bool array_init_with_capacity(array_t_ *arr, unsigned int capacity, size_t element_size);
static void array_deinit(array_t_ *arr);

array_t_* array_make_(size_t element_size) {
    return array_make_with_capacity(32, element_size);
}

array_t_* array_make_with_capacity(unsigned int capacity, size_t element_size) {
    array_t_ *arr = collections_malloc(sizeof(array_t_));
    if (arr == NULL) {
        return NULL;
    }
    bool succeeded = array_init_with_capacity(arr, capacity, element_size);
    if (succeeded == false) {
        collections_free(arr);
        return NULL;
    }
    return arr;
}

void array_destroy(array_t_ *arr) {
    if (arr == NULL) {
        return;
    }
    array_deinit(arr);
    collections_free(arr);
}

void array_destroy_with_items_(array_t_ *arr, array_item_deinit_fn deinit_fn) {
    for (int i = 0; i < array_count(arr); i++) {
        void *item = array_get(arr, i);
        deinit_fn(item);
    }
    array_destroy(arr);
}

array_t_* array_copy(const array_t_ *arr) {
    array_t_ *copy = collections_malloc(sizeof(array_t_));
    if (!copy) {
        return NULL;
    }
    copy->capacity = arr->capacity;
    copy->count = arr->count;
    copy->element_size = arr->element_size;
    copy->lock_capacity = arr->lock_capacity;
    copy->data = collections_malloc(arr->capacity * arr->element_size);
    memcpy(copy->data, arr->data, arr->capacity * arr->element_size);
    if (!copy->data) {
        collections_free(copy);
        return NULL;
    }
    return copy;
}


bool array_add(array_t_ *arr, const void *value) {
    if (arr->count >= arr->capacity) {
        COLLECTIONS_ASSERT(!arr->lock_capacity);
        if (arr->lock_capacity) {
            return false;
        }
        unsigned int new_capacity = arr->capacity > 0 ? arr->capacity * 2 : 1;
        unsigned char *new_data = collections_malloc(new_capacity * arr->element_size);
        if (new_data == NULL) {
            return false;
        }
        memcpy(new_data, arr->data, arr->count * arr->element_size);
        collections_free(arr->data);
        arr->data = new_data;
        arr->capacity = new_capacity;
    }
    if (value) {
        memcpy(arr->data + (arr->count * arr->element_size), value, arr->element_size);
    }
    arr->count++;
    return true;
}

bool array_addn(array_t_ *arr, const void *values, int n) {
    for (int i = 0; i < n; i++) {
        const uint8_t *value = NULL;
        if (values) {
            value = (const uint8_t*)values + (i * arr->element_size);
        }
        bool ok = array_add(arr, value);
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool array_add_array(array_t_ *dest, const array_t_ *source) {
    COLLECTIONS_ASSERT(dest->element_size == source->element_size);
    if (dest->element_size != source->element_size) {
        return false;
    }
    for (int i = 0; i < array_count(source); i++) {
        void *item = array_get(source, i);
        bool ok = array_add(dest, item);
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool array_push(array_t_ *arr, const void *value) {
    return array_add(arr, value);
}

bool array_pop(array_t_ *arr, void *out_value) {
    if (arr->count <= 0) {
        return false;
    }
    if (out_value) {
        void *res = array_get(arr, arr->count - 1);
        memcpy(out_value, res, arr->element_size);
    }
    array_remove_at(arr, arr->count - 1);
    return true;
}

void* array_top(array_t_ *arr) {
    if (arr->count <= 0) {
        return NULL;
    }
    return array_get(arr, arr->count - 1);
}

bool array_set(array_t_ *arr, unsigned int ix, void *value) {
    if (ix >= arr->count) {
        COLLECTIONS_ASSERT(false);
        return false;
    }
    size_t offset = ix * arr->element_size;
    memmove(arr->data + offset, value, arr->element_size);
    return true;
}

bool array_setn(array_t_ *arr, unsigned int ix, void *values, int n) {
    for (int i = 0; i < n; i++) {
        int dest_ix = ix + i;
        unsigned char *value = (unsigned char*)values + (i * arr->element_size);
        if (dest_ix < array_count(arr)) {
            bool ok = array_set(arr, dest_ix, value);
            if (!ok) {
                return false;
            }
        } else {
            bool ok = array_add(arr, value);
            if (!ok) {
                return false;
            }
        }
    }
    return true;
}

void * array_get(const array_t_ *arr, unsigned int ix) {
    if (ix >= arr->count) {
        COLLECTIONS_ASSERT(false);
        return NULL;
    }
    size_t offset = ix * arr->element_size;
    return arr->data + offset;
}

void * array_get_last(const array_t_ *arr) {
    if (arr->count <= 0) {
        return NULL;
    }
    return array_get(arr, arr->count - 1);
}

int array_count(const array_t_ *arr) {
    if (!arr) {
        return 0;
    }
    return arr->count;
}

bool array_remove_at(array_t_ *arr, unsigned int ix) {
    if (ix >= arr->count) {
        return false;
    }
    if (ix == (arr->count - 1)) {
        arr->count--;
        return true;
    }
    size_t to_move_bytes = (arr->count - 1 - ix) * arr->element_size;
    void *dest = arr->data + (ix * arr->element_size);
    void *src = arr->data + ((ix + 1) * arr->element_size);
    memmove(dest, src, to_move_bytes);
    arr->count--;
    return true;
}

bool array_remove_item(array_t_ *arr, void *ptr) {
    int ix = array_get_index(arr, ptr);
    if (ix < 0) {
        return false;
    }
    return array_remove_at(arr, ix);
}

void array_clear(array_t_ *arr) {
    arr->count = 0;
}

void array_clear_and_deinit_items_(array_t_ *arr, array_item_deinit_fn deinit_fn) {
    for (int i = 0; i < array_count(arr); i++) {
        void *item = array_get(arr, i);
        deinit_fn(item);
    }
    arr->count = 0;
}

void array_lock_capacity(array_t_ *arr) {
    arr->lock_capacity = true;
}

int array_get_index(const array_t_ *arr, void *ptr) {
    for (int i = 0; i < array_count(arr); i++) {
        if (array_get(arr, i) == ptr) {
            return i;
        }
    }
    return -1;
}

bool array_contains(const array_t_ *arr, void *ptr) {
    return array_get_index(arr, ptr) >= 0;
}

void* array_data(array_t_ *arr) {
    return arr->data;
}

const void*  array_const_data(const array_t_ *arr) {
    return arr->data;
}

bool array_orphan_data(array_t_ *arr) {
    return array_init_with_capacity(arr, 0, arr->element_size);
}

void array_reverse(array_t_ *arr) {
    int count = array_count(arr);
    if (count < 2) {
        return;
    }
    void *temp = collections_malloc(arr->element_size);
    for (int a_ix = 0; a_ix < (count / 2); a_ix++) {
        int b_ix = count - a_ix - 1;
        void *a = array_get(arr, a_ix);
        void *b = array_get(arr, b_ix);
        memcpy(temp, a, arr->element_size);
        array_set(arr, a_ix, b);
        array_set(arr, b_ix, temp);
    }
    collections_free(temp);
}

static bool array_init_with_capacity(array_t_ *arr, unsigned int capacity, size_t element_size) {
    if (capacity > 0) {
        arr->data = collections_malloc(capacity * element_size);
        if (arr->data == NULL) {
            return false;
        }
    } else {
        arr->data = NULL;
    }
    arr->capacity = capacity;
    arr->count = 0;
    arr->element_size = element_size;
    arr->lock_capacity = false;
    return true;
}

static void array_deinit(array_t_ *arr) {
    collections_free(arr->data);
}

//-----------------------------------------------------------------------------
// Pointer Array
//-----------------------------------------------------------------------------

typedef struct ptrarray_ {
    array_t_ arr;
} ptrarray_t_;

ptrarray_t_* ptrarray_make(void) {
    return ptrarray_make_with_capacity(0);
}

ptrarray_t_* ptrarray_make_with_capacity(unsigned int capacity) {
    ptrarray_t_ *ptrarr = collections_malloc(sizeof(ptrarray_t_));
    if (ptrarr == NULL) {
        return NULL;
    }
    bool succeeded = array_init_with_capacity(&ptrarr->arr, capacity, sizeof(void*));
    if (succeeded == false) {
        collections_free(ptrarr);
        return NULL;
    }
    return ptrarr;
}

void ptrarray_destroy(ptrarray_t_ *arr) {
    if (arr == NULL) {
        return;
    }
    array_deinit(&arr->arr);
    collections_free(arr);
}

void ptrarray_destroy_with_items_(ptrarray_t_ *arr, ptrarray_item_destroy_fn destroy_fn){
    if (arr == NULL) {
        return;
    }

    if (destroy_fn) {
        ptrarray_clear_and_destroy_items_(arr, destroy_fn);
    }

    ptrarray_destroy(arr);
}

ptrarray_t_* ptrarray_copy(ptrarray_t_ *arr) {
    ptrarray_t_ *arr_copy = ptrarray_make_with_capacity(arr->arr.capacity);
    for (int i = 0; i < ptrarray_count(arr); i++) {
        void *item = ptrarray_get(arr, i);
        ptrarray_add(arr_copy, item);
    }
    return arr_copy;
}

ptrarray_t_* ptrarray_copy_with_items_(ptrarray_t_ *arr, ptrarray_item_copy_fn copy_fn) {
    ptrarray_t_ *arr_copy = ptrarray_make_with_capacity(arr->arr.capacity);
    for (int i = 0; i < ptrarray_count(arr); i++) {
        void *item = ptrarray_get(arr, i);
        void *item_copy = copy_fn(item);
        ptrarray_add(arr_copy, item_copy);
    }
    return arr_copy;
}

bool ptrarray_add(ptrarray_t_ *arr, void *ptr) {
    return array_add(&arr->arr, &ptr);
}

bool ptrarray_set(ptrarray_t_ *arr, unsigned int ix, void *ptr) {
    return array_set(&arr->arr, ix, &ptr);
}

bool ptrarray_add_array(ptrarray_t_ *dest, const ptrarray_t_ *source) {
    return array_add_array(&dest->arr, &source->arr);
}

void * ptrarray_get(ptrarray_t_ *arr, unsigned int ix) {
    void* res = array_get(&arr->arr, ix);
    if (!res) {
        return NULL;
    }
    return *(void**)res;
}

bool ptrarray_push(ptrarray_t_ *arr, void *ptr) {
    return ptrarray_add(arr, ptr);
}

void *ptrarray_pop(ptrarray_t_ *arr) {
    int ix = ptrarray_count(arr) - 1;
    void *res = ptrarray_get(arr, ix);
    ptrarray_remove_at(arr, ix);
    return res;
}

void *ptrarray_top(ptrarray_t_ *arr) {
    int count = ptrarray_count(arr);
    return ptrarray_get(arr, count - 1);
}

int ptrarray_count(const ptrarray_t_ *arr) {
    if (!arr) {
        return 0;
    }
    return array_count(&arr->arr);
}

bool ptrarray_remove_at(ptrarray_t_ *arr, unsigned int ix) {
    return array_remove_at(&arr->arr, ix);
}

bool ptrarray_remove_item(ptrarray_t_ *arr, void *item) {
    for (int i = 0; i < ptrarray_count(arr); i++) {
        if (item == ptrarray_get(arr, i)) {
            ptrarray_remove_at(arr, i);
            return true;
        }
    }
    COLLECTIONS_ASSERT(false);
    return false;
}

void ptrarray_clear(ptrarray_t_ *arr) {
    array_clear(&arr->arr);
}

void ptrarray_clear_and_destroy_items_(ptrarray_t_ *arr, ptrarray_item_destroy_fn destroy_fn) {
    for (int i = 0; i < ptrarray_count(arr); i++) {
        void *item = ptrarray_get(arr, i);
        destroy_fn(item);
    }
    ptrarray_clear(arr);
}

void ptrarray_lock_capacity(ptrarray_t_ *arr) {
    array_lock_capacity(&arr->arr);
}

int ptrarray_get_index(ptrarray_t_ *arr, void *ptr) {
    for (int i = 0; i < ptrarray_count(arr); i++) {
        if (ptrarray_get(arr, i) == ptr) {
            return i;
        }
    }
    return -1;
}

bool ptrarray_contains(ptrarray_t_ *arr, void *item) {
    return ptrarray_get_index(arr, item) >= 0;
}

void * ptrarray_get_addr(ptrarray_t_ *arr, unsigned int ix) {
    void* res = array_get(&arr->arr, ix);
    if (res == NULL) {
        return NULL;
    }
    return res;
}

void* ptrarray_data(ptrarray_t_ *arr) {
    return array_data(&arr->arr);
}

void ptrarray_reverse(ptrarray_t_ *arr) {
    array_reverse(&arr->arr);
}

//-----------------------------------------------------------------------------
// String buffer
//-----------------------------------------------------------------------------

typedef struct strbuf {
    char *data;
    size_t capacity;
    size_t len;
} strbuf_t;

static bool strbuf_grow(strbuf_t *buf, size_t new_capacity);

strbuf_t* strbuf_make(void) {
    strbuf_t *res = strbuf_make_with_capacity(1);
    return res;
}

strbuf_t* strbuf_make_with_capacity(unsigned int capacity) {
    strbuf_t *buf = collections_malloc(sizeof(strbuf_t));
    if (buf == NULL) {
        return NULL;
    }
    memset(buf, 0, sizeof(strbuf_t));
    buf->data = collections_malloc(capacity);
    if (buf->data == NULL) {
        collections_free(buf);
        return NULL;
    }
    buf->capacity = capacity;
    buf->len = 0;
    buf->data[0] = '\0';
    return buf;
}

void strbuf_destroy(strbuf_t *buf) {
    if (buf == NULL) {
        return;
    }
    collections_free(buf->data);
    collections_free(buf);
}

void strbuf_clear(strbuf_t *buf) {
    buf->len = 0;
    buf->data[0] = '\0';
}

bool strbuf_append(strbuf_t *buf, const char *str) {
    size_t str_len = strlen(str);
    if (str_len == 0) {
        return true;
    }
    size_t required_capacity = buf->len + str_len + 1;
    if (required_capacity > buf->capacity) {
        bool ok = strbuf_grow(buf, required_capacity * 2);
        if (!ok) {
            return false;
        }
    }
    memcpy(buf->data + buf->len, str, str_len);
    buf->len = buf->len + str_len;
    buf->data[buf->len] = '\0';
    return true;
}

bool strbuf_appendf(strbuf_t *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int to_write = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (to_write == 0) {
        return true;
    }
    size_t required_capacity = buf->len + to_write + 1;
    if (required_capacity > buf->capacity) {
        bool ok = strbuf_grow(buf, required_capacity * 2);
        if (!ok) {
            return false;
        }
    }
    va_start(args, fmt);
    int written = vsprintf(buf->data + buf->len, fmt, args);
    (void)written;
    va_end(args);
    if (written != to_write) {
        return false;
    }
    buf->len = buf->len + to_write;
    buf->data[buf->len] = '\0';
    return true;
}

const char * strbuf_get_string(const strbuf_t *buf) {
    return buf->data;
}

size_t strbuf_get_length(const strbuf_t *buf) {
    return buf->len;
}

char * strbuf_get_string_and_destroy(strbuf_t *buf) {
    char *res = buf->data;
    buf->data = NULL;
    strbuf_destroy(buf);
    return res;
}

static bool strbuf_grow(strbuf_t *buf, size_t new_capacity) {
    char *new_data = collections_malloc(new_capacity);
    if (new_data == NULL) {
        return false;
    }
    memcpy(new_data, buf->data, buf->len);
    new_data[buf->len] = '\0';
    collections_free(buf->data);
    buf->data = new_data;
    buf->capacity = new_capacity;
    return true;
}

//-----------------------------------------------------------------------------
// Utils
//-----------------------------------------------------------------------------

ptrarray(char)* kg_split_string(const char *str, const char *delimiter) {
    ptrarray(char)* res = ptrarray_make();
    if (!str) {
        return res;
    }
    const char *line_start = str;
    const char *line_end = strstr(line_start, delimiter);
    while (line_end != NULL) {
        long len = line_end - line_start;
        char *line = collections_strndup(line_start, len);
        ptrarray_add(res, line);
        line_start = line_end + 1;
        line_end = strstr(line_start, delimiter);
    }
    char *rest = collections_strdup(line_start);
    ptrarray_add(res, rest);
    return res;
}

char* kg_join(ptrarray(char) *items, const char *with) {
    strbuf_t *res = strbuf_make();
    for (int i = 0; i < ptrarray_count(items); i++) {
        char *item = ptrarray_get(items, i);
        strbuf_append(res, item);
        if (i < (ptrarray_count(items) - 1)) {
            strbuf_append(res, with);
        }
    }
    return strbuf_get_string_and_destroy(res);
}

char* kg_canonicalise_path(const char *path) {
    if (!strchr(path, '/') || (!strstr(path, "/../") && !strstr(path, "./"))) {
        return collections_strdup(path);
    }

    ptrarray(char) *split = kg_split_string(path, "/");

    for (int i = 0; i < ptrarray_count(split) - 1; i++) {
        char *item = ptrarray_get(split, i);
        char *next_item = ptrarray_get(split, i + 1);
        if (kg_streq(item, ".")) {
            collections_free(item);
            ptrarray_remove_at(split, i);
            i = -1;
            continue;
        }
        if (kg_streq(next_item, "..")) {
            collections_free(item);
            collections_free(next_item);
            ptrarray_remove_at(split, i);
            ptrarray_remove_at(split, i);
            i = -1;
            continue;
        }
    }

    char *joined = kg_join(split, "/");
    ptrarray_destroy_with_items(split, collections_free);
    return joined;
}

bool kg_is_path_absolute(const char *path) {
    return path[0] == '/';
}

bool kg_streq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}
