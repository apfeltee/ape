#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef APE_AMALGAMATED
#include "gc.h"
#include "object.h"
#endif

#define GCMEM_POOL_SIZE 1024

typedef struct gcmem {
    ptrarray(object_data_t) *objects;
    ptrarray(object_data_t) *objects_back;

    array(object_t) *objects_not_gced;

    object_data_t *pool[GCMEM_POOL_SIZE];
    int pool_index;
} gcmem_t;

gcmem_t *gcmem_make() {
    gcmem_t *mem = ape_malloc(sizeof(gcmem_t));
    memset(mem, 0, sizeof(gcmem_t));
    mem->objects = ptrarray_make();
    mem->objects_back = ptrarray_make();
    mem->objects_not_gced = array_make(object_t);
    mem->pool_index = -1;
    return mem;
}

void gcmem_destroy(gcmem_t *mem) {
    if (!mem) {
        return;
    }
    for (int i = 0; i < ptrarray_count(mem->objects); i++) {
        object_data_t *obj = ptrarray_get(mem->objects, i);
        object_data_deinit(obj);
        ape_free(obj);
    }
    ptrarray_destroy(mem->objects);
    ptrarray_destroy(mem->objects_back);
    array_destroy(mem->objects_not_gced);
    for (int i = 0; i <= mem->pool_index; i++) {
        ape_free(mem->pool[i]);
    }
    memset(mem, 0, sizeof(gcmem_t));
    ape_free(mem);
}

object_data_t* gcmem_alloc_object_data(gcmem_t *mem, object_type_t type) {
    object_data_t *data = NULL;
    if (mem->pool_index >= 0) {
        data = mem->pool[mem->pool_index];
        mem->pool_index--;
    } else {
        data = ape_malloc(sizeof(object_data_t));
    }
    memset(data, 0, sizeof(object_data_t));
    ptrarray_add(mem->objects, data);
    data->mem = mem;
    data->type = type;
    return data;
}

void gc_unmark_all(gcmem_t *mem) {
    for (int i = 0; i < ptrarray_count(mem->objects); i++) {
        object_data_t *data = ptrarray_get(mem->objects, i);
        data->gcmark = false;
    }
}

void gc_mark_objects(object_t *objects, int count) {
    for (int i = 0; i < count; i++) {
        object_t obj = objects[i];
        gc_mark_object(obj);
    }
}

void gc_mark_object(object_t obj) {
    if (!object_is_allocated(obj)) {
        return;
    }
    object_data_t *data = object_get_allocated_data(obj);
    if (data->gcmark) {
        return;
    }

    data->gcmark = true;
    switch (data->type) {
        case OBJECT_MAP: {
            int len = object_get_map_length(obj);
            for (int i = 0; i < len; i++) {
                object_t key = object_get_map_key_at(obj, i);
                object_t val = object_get_map_value_at(obj, i);
                gc_mark_object(key);
                gc_mark_object(val);
            }
            break;
        }
        case OBJECT_ARRAY: {
            int len = object_get_array_length(obj);
            for (int i = 0; i < len; i++) {
                object_t val = object_get_array_value_at(obj, i);
                gc_mark_object(val);
            }
            break;
        }
        case OBJECT_FUNCTION: {
            function_t *function = object_get_function(obj);
            for (int i = 0; i < function->free_vals_count; i++) {
                object_t free_val = object_get_function_free_val(obj, i);
                gc_mark_object(free_val);
            }
            break;
        }
        default: {
            break;
        }
    }
}

void gc_sweep(gcmem_t *mem) {
    gc_mark_objects(array_data(mem->objects_not_gced), array_count(mem->objects_not_gced));

    ptrarray_clear(mem->objects_back);
    for (int i = 0; i < ptrarray_count(mem->objects); i++) {
        object_data_t *data = ptrarray_get(mem->objects, i);
        if (data->gcmark) {
            ptrarray_add(mem->objects_back, data);
        } else {
            object_data_deinit(data);
            if (mem->pool_index < (GCMEM_POOL_SIZE - 1)) {
                mem->pool_index++;
                mem->pool[mem->pool_index] = data;
            } else {
                ape_free(data);
            }
        }
    }
    ptrarray(object_t) *objs_temp = mem->objects;
    mem->objects = mem->objects_back;
    mem->objects_back = objs_temp;
}

void gc_disable_on_object(object_t obj) {
    if (!object_is_allocated(obj)) {
        return;
    }
    object_data_t *data = object_get_allocated_data(obj);
    if (array_contains(data->mem->objects_not_gced, &obj)) {
        return;
    }
    array_add(data->mem->objects_not_gced, &obj);
}

void gc_enable_on_object(object_t obj) {
    if (!object_is_allocated(obj)) {
        return;
    }
    object_data_t *data = object_get_allocated_data(obj);
    array_remove_item(data->mem->objects_not_gced, &obj);
}
