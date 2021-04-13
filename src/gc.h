#ifndef gc_h
#define gc_h

#ifndef APE_AMALGAMATED
#include "common.h"
#include "collections.h"
#include "object.h"
#endif

typedef struct object_data object_data_t;
typedef struct env env_t;

typedef struct gcmem gcmem_t;

APE_INTERNAL gcmem_t *gcmem_make(void);
APE_INTERNAL void gcmem_destroy(gcmem_t *mem);

APE_INTERNAL object_data_t* gcmem_alloc_object_data(gcmem_t *mem, object_type_t type);

APE_INTERNAL void gc_unmark_all(gcmem_t *mem);
APE_INTERNAL void gc_mark_objects(object_t *objects, int count);
APE_INTERNAL void gc_mark_object(object_t object);
APE_INTERNAL void gc_mark_object_data(object_data_t *data);
APE_INTERNAL void gc_sweep(gcmem_t *mem);

APE_INTERNAL void gc_disable_on_object(object_t obj);
APE_INTERNAL void gc_enable_on_object(object_t obj);

#endif /* gc_h */
