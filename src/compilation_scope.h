#ifndef compilation_scope_h
#define compilation_scope_h

#ifndef APE_AMALGAMATED
#include "symbol_table.h"
#include "code.h"
#include "gc.h"
#endif

typedef struct compilation_result {
    uint8_t *bytecode;
    src_pos_t *src_positions;
    int count;
} compilation_result_t;

typedef struct compilation_scope {
    struct compilation_scope *outer;
    array(uint8_t) *bytecode;
    array(src_pos_t) *src_positions;
    opcode_t last_opcode;
} compilation_scope_t;

APE_INTERNAL compilation_scope_t* compilation_scope_make(compilation_scope_t *outer);
APE_INTERNAL void compilation_scope_destroy(compilation_scope_t *scope);
APE_INTERNAL compilation_result_t *compilation_scope_orphan_result(compilation_scope_t *scope);

APE_INTERNAL compilation_result_t* compilation_result_make(uint8_t *bytecode, src_pos_t *src_positions, int count);
APE_INTERNAL void compilation_result_destroy(compilation_result_t* res);

#endif /* compilation_scope_h */
