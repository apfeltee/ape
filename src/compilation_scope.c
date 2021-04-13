#ifndef APE_AMALGAMATED
#include "compilation_scope.h"
#endif

compilation_scope_t *compilation_scope_make(compilation_scope_t *outer) {
    compilation_scope_t *scope = ape_malloc(sizeof(compilation_scope_t));
    memset(scope, 0, sizeof(compilation_scope_t));
    scope->outer = outer;
    scope->bytecode = array_make(uint8_t);
    scope->src_positions = array_make(src_pos_t);
    return scope;
}

void compilation_scope_destroy(compilation_scope_t *scope) {
    array_destroy(scope->bytecode);
    array_destroy(scope->src_positions);
    ape_free(scope);
}

compilation_result_t* compilation_scope_orphan_result(compilation_scope_t *scope) {
    compilation_result_t *res = compilation_result_make(array_data(scope->bytecode),
                                                        array_data(scope->src_positions),
                                                        array_count(scope->bytecode));
    array_orphan_data(scope->bytecode);
    array_orphan_data(scope->src_positions);
    return res;
}

compilation_result_t* compilation_result_make(uint8_t *bytecode, src_pos_t *src_positions, int count) {
    compilation_result_t *res = ape_malloc(sizeof(compilation_result_t));
    memset(res, 0, sizeof(compilation_result_t));
    res->bytecode = bytecode;
    res->src_positions = src_positions;
    res->count = count;
    return res;
}

void compilation_result_destroy(compilation_result_t *res) {
    if (!res) {
        return;
    }
    ape_free(res->bytecode);
    ape_free(res->src_positions);
    ape_free(res);
}
