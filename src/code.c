#include <stdlib.h>

#ifndef APE_AMALGAMATED
#include "code.h"

#include "common.h"
#include "collections.h"
#endif

static opcode_definition_t g_definitions[OPCODE_MAX + 1] = {
    {"NONE", 0, {0}},
    {"CONSTANT", 1, {2}},
    {"ADD", 0, {0}},
    {"POP", 0, {0}},
    {"SUB", 0, {0}},
    {"MUL", 0, {0}},
    {"DIV", 0, {0}},
    {"MOD", 0, {0}},
    {"TRUE", 0, {0}},
    {"FALSE", 0, {0}},
    {"COMPARE", 0, {0}},
    {"EQUAL", 0, {0}},
    {"NOT_EQUAL", 0, {0}},
    {"GREATER_THAN", 0, {0}},
    {"GREATER_THAN_EQUAL", 0, {0}},
    {"MINUS", 0, {0}},
    {"BANG", 0, {0}},
    {"JUMP", 1, {2}},
    {"JUMP_IF_FALSE", 1, {2}},
    {"JUMP_IF_TRUE", 1, {2}},
    {"NULL", 0, {0}},
    {"GET_GLOBAL", 1, {2}},
    {"SET_GLOBAL", 1, {2}},
    {"DEFINE_GLOBAL", 1, {2}},
    {"ARRAY", 1, {2}},
    {"MAP_START", 1, {2}},
    {"MAP_END", 1, {2}},
    {"GET_THIS", 0, {0}},
    {"GET_INDEX", 0, {0}},
    {"SET_INDEX", 0, {0}},
    {"GET_VALUE_AT", 0, {0}},
    {"CALL", 1, {1}},
    {"RETURN_VALUE", 0, {0}},
    {"RETURN", 0, {0}},
    {"GET_LOCAL", 1, {1}},
    {"DEFINE_LOCAL", 1, {1}},
    {"SET_LOCAL", 1, {1}},
    {"GET_NATIVE_FUNCTION", 1, {2}},
    {"FUNCTION", 2, {2, 1}},
    {"GET_FREE", 1, {1}},
    {"SET_FREE", 1, {1}},
    {"CURRENT_FUNCTION", 0, {0}},
    {"DUP", 0, {0}},
    {"NUMBER", 1, {8}},
    {"LEN", 0, {0}},
    {"SET_RECOVER", 1, {2}},
    {"OR", 0, {0}},
    {"XOR", 0, {0}},
    {"AND", 0, {0}},
    {"LSHIFT", 0, {0}},
    {"RSHIFT", 0, {0}},
    {"INVALID_MAX", 0, {0}},
};

opcode_definition_t* opcode_lookup(opcode_t op) {
    if (op <= OPCODE_NONE || op >= OPCODE_MAX) {
        return NULL;
    }
    return &g_definitions[op];
}

const char *opcode_get_name(opcode_t op) {
    if (op <= OPCODE_NONE || op >= OPCODE_MAX) {
        return NULL;
    }
    return g_definitions[op].name;
}

int code_make(opcode_t op, int operands_count, uint64_t *operands, array(uint8_t) *res) {
    opcode_definition_t *def = opcode_lookup(op);
    if (!def) {
        return 0;
    }

    int instr_len = 1;
    for (int i = 0; i < def->num_operands; i++) {
        instr_len += def->operand_widths[i];
    }

    uint8_t val = op;
    array_add(res, &val);

    for (int i = 0; i < operands_count; i++) {
        int width = def->operand_widths[i];
        switch (width) {
            case 1: {
                val = operands[i];
                array_add(res, &val);
                break;
            }
            case 2: {
                val = operands[i] >> 8;
                array_add(res, &val);
                val = operands[i] >> 0;
                array_add(res, &val);
                break;
            }
            case 4: {
                val = operands[i] >> 24;
                array_add(res, &val);
                val = operands[i] >> 16;
                array_add(res, &val);
                val = operands[i] >> 8;
                array_add(res, &val);
                val = operands[i] >> 0;
                array_add(res, &val);
                break;
            }
            case 8: {
                val = operands[i] >> 56;
                array_add(res, &val);
                val = operands[i] >> 48;
                array_add(res, &val);
                val = operands[i] >> 40;
                array_add(res, &val);
                val = operands[i] >> 32;
                array_add(res, &val);
                val = operands[i] >> 24;
                array_add(res, &val);
                val = operands[i] >> 16;
                array_add(res, &val);
                val = operands[i] >> 8;
                array_add(res, &val);
                val = operands[i] >> 0;
                array_add(res, &val);
                break;
            }
            default: {
                APE_ASSERT(false);
                break;
            }
        }
    }

    return instr_len;
}

void code_to_string(uint8_t *code, src_pos_t *source_positions, size_t code_size, strbuf_t *res) {
    unsigned pos = 0;
    while (pos < code_size) {
        uint8_t op = code[pos];
        opcode_definition_t *def = opcode_lookup(op);
        APE_ASSERT(def);
        if (source_positions) {
            src_pos_t src_pos = source_positions[pos];
            strbuf_appendf(res, "%d:%-4d\t%04d\t%s", src_pos.line, src_pos.column, pos, def->name);
        } else {
            strbuf_appendf(res, "%04d %s", pos, def->name);
        }
        pos += 1;
        
        uint64_t operands[2];
        code_read_operands(def, code + pos, operands);
        for (int i = 0; i < def->num_operands; i++) {
            if (op == OPCODE_NUMBER) {
                double val_double = ape_uint64_to_double(operands[i]);
                strbuf_appendf(res, " %1.17g", val_double);
            } else {
                strbuf_appendf(res, " %llu", operands[i]);
            }
            pos += def->operand_widths[i];
        }
        strbuf_append(res, "\n");

    }
    return;
}

bool code_read_operands(opcode_definition_t *def, uint8_t *instr, uint64_t out_operands[2]) {
    int offset = 0;
    for (int i = 0; i < def->num_operands; i++) {
        int operand_width = def->operand_widths[i];
        switch (operand_width) {
            case 1: {
                out_operands[i] = instr[offset];
                break;
            }
            case 2: {
                uint64_t operand = 0;
                operand = operand | (instr[offset] << 8);
                operand = operand | (instr[offset + 1]);
                out_operands[i] = operand;
                break;
            }
            case 4: {
                uint64_t operand = 0;
                operand = operand | (instr[offset + 0] << 24);
                operand = operand | (instr[offset + 1] << 16);
                operand = operand | (instr[offset + 2] << 8);
                operand = operand | (instr[offset + 3]);
                out_operands[i] = operand;
                break;
            }
            case 8: {
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
            default: {
                APE_ASSERT(false);
                return false;
            }
        }
        offset += operand_width;
    }
    return true;;
}
