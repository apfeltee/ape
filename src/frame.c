#include <stdlib.h>

#ifndef APE_AMALGAMATED
#include "frame.h"
#include "compiler.h"
#endif

bool frame_init(frame_t* frame, object_t function_obj, int base_pointer) {
    if (object_get_type(function_obj) != OBJECT_FUNCTION) {
        return false;
    }
    function_t* function = object_get_function(function_obj);
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

opcode_val_t frame_read_opcode(frame_t* frame){
    frame->src_ip = frame->ip;
    return frame_read_uint8(frame);
}

uint64_t frame_read_uint64(frame_t* frame) {
    const uint8_t *data = frame->bytecode + frame->ip;
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

uint16_t frame_read_uint16(frame_t* frame) {
    const uint8_t *data = frame->bytecode + frame->ip;
    frame->ip += 2;
    return (data[0] << 8) | data[1];
}

uint8_t frame_read_uint8(frame_t* frame) {
    const uint8_t *data = frame->bytecode + frame->ip;
    frame->ip += 1;
    return data[0];
}

src_pos_t frame_src_position(const frame_t *frame) {
    if (frame->src_positions) {
        return frame->src_positions[frame->src_ip];
    }
    return src_pos_invalid;
}
