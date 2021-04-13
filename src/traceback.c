#ifndef APE_AMALGAMATED
#include "traceback.h"
#include "vm.h"
#include "compiler.h"
#endif

traceback_t* traceback_make(void) {
    traceback_t *traceback = ape_malloc(sizeof(traceback_t));
    traceback->items = array_make(traceback_item_t);
    return traceback;
}

void traceback_destroy(traceback_t *traceback) {
    if (!traceback) {
        return;
    }
    for (int i = 0; i < array_count(traceback->items); i++) {
        traceback_item_t *item = array_get(traceback->items, i);
        ape_free(item->function_name);
    }
    array_destroy(traceback->items);
    ape_free(traceback);
}

void traceback_append(traceback_t *traceback, const char *function_name, src_pos_t pos) {
    traceback_item_t item;
    item.function_name = ape_strdup(function_name);
    item.pos = pos;
    array_add(traceback->items, &item);
}

void traceback_append_from_vm(traceback_t *traceback, vm_t *vm) {
    for (int i = vm->frames_count - 1; i >= 0; i--) {
        frame_t *frame = &vm->frames[i];
        traceback_append(traceback, object_get_function_name(frame->function), frame_src_position(frame));
    }
}

void traceback_to_string(const traceback_t *traceback, strbuf_t *buf) {
    int depth  = array_count(traceback->items);
    for (int i = 0; i < depth; i++) {
        traceback_item_t *item = array_get(traceback->items, i);
        const char *filename = traceback_item_get_filepath(item);
        if (item->pos.line >= 0 && item->pos.column >= 0) {
            strbuf_appendf(buf, "%s in %s on %d:%d\n", item->function_name, filename, item->pos.line, item->pos.column);
        } else {
            strbuf_appendf(buf, "%s\n", item->function_name);
        }
    }
}

const char* traceback_item_get_line(traceback_item_t *item) {
    if (!item->pos.file) {
        return NULL;
    }
    ptrarray(char*) *lines = item->pos.file->lines;
    if (item->pos.line >= ptrarray_count(lines)) {
        return NULL;
    }
    const char *line = ptrarray_get(lines, item->pos.line);
    return line;
}

const char* traceback_item_get_filepath(traceback_item_t *item) {
    if (!item->pos.file) {
        return NULL;
    }
    return item->pos.file->path;
}
