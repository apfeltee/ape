
#pragma once

#ifndef APE_AMALGAMATED
#include "common.h"
#include "collections.h"
#endif

typedef struct vm vm_t;
typedef struct traceback traceback_t;
typedef struct traceback_item traceback_item_t;

struct traceback_item {
    char *function_name;
    src_pos_t pos;
};

struct traceback {
    array(traceback_item_t)* items;
};

APE_INTERNAL traceback_t* traceback_make(void);
APE_INTERNAL void traceback_destroy(traceback_t *traceback);
APE_INTERNAL void traceback_append(traceback_t *traceback, const char *function_name, src_pos_t pos);
APE_INTERNAL void traceback_append_from_vm(traceback_t *traceback, vm_t *vm);
APE_INTERNAL void traceback_to_string(const traceback_t *traceback, strbuf_t *buf);
APE_INTERNAL const char* traceback_item_get_line(traceback_item_t *item);
APE_INTERNAL const char* traceback_item_get_filepath(traceback_item_t *item);
