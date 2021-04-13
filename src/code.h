#ifndef code_h
#define code_h

#include <stdint.h>

#ifndef APE_AMALGAMATED
#include "common.h"
#include "collections.h"
#endif

typedef uint8_t opcode_t;
typedef enum opcode_val opcode_val_t;
typedef struct opcode_definition opcode_definition_t;


enum opcode_val {
    OPCODE_NONE = 0,
    OPCODE_CONSTANT,
    OPCODE_ADD,
    OPCODE_POP,
    OPCODE_SUB,
    OPCODE_MUL,
    OPCODE_DIV,
    OPCODE_MOD,
    OPCODE_TRUE,
    OPCODE_FALSE,
    OPCODE_COMPARE,
    OPCODE_EQUAL,
    OPCODE_NOT_EQUAL,
    OPCODE_GREATER_THAN,
    OPCODE_GREATER_THAN_EQUAL,
    OPCODE_MINUS,
    OPCODE_BANG,
    OPCODE_JUMP,
    OPCODE_JUMP_IF_FALSE,
    OPCODE_JUMP_IF_TRUE,
    OPCODE_NULL,
    OPCODE_GET_GLOBAL,
    OPCODE_SET_GLOBAL,
    OPCODE_DEFINE_GLOBAL,
    OPCODE_ARRAY,
    OPCODE_MAP_START,
    OPCODE_MAP_END,
    OPCODE_GET_THIS,
    OPCODE_GET_INDEX,
    OPCODE_SET_INDEX,
    OPCODE_GET_VALUE_AT,
    OPCODE_CALL,
    OPCODE_RETURN_VALUE,
    OPCODE_RETURN,
    OPCODE_GET_LOCAL,
    OPCODE_DEFINE_LOCAL,
    OPCODE_SET_LOCAL,
    OPCODE_GET_NATIVE_FUNCTION,
    OPCODE_FUNCTION,
    OPCODE_GET_FREE,
    OPCODE_SET_FREE,
    OPCODE_CURRENT_FUNCTION,
    OPCODE_DUP,
    OPCODE_NUMBER,
    OPCODE_LEN,
    OPCODE_SET_RECOVER,
    OPCODE_OR,
    OPCODE_XOR,
    OPCODE_AND,
    OPCODE_LSHIFT,
    OPCODE_RSHIFT,
    OPCODE_MAX,
};

struct opcode_definition {
    const char *name;
    int num_operands;
    int operand_widths[2];
};

APE_INTERNAL opcode_definition_t* opcode_lookup(opcode_t op);
APE_INTERNAL const char *opcode_get_name(opcode_t op);
APE_INTERNAL int code_make(opcode_t op, int operands_count, uint64_t *operands, array(uint8_t) *res);
APE_INTERNAL void code_to_string(uint8_t *code, src_pos_t *source_positions, size_t code_size, strbuf_t *res);
APE_INTERNAL bool code_read_operands(opcode_definition_t *def, uint8_t *instr, uint64_t out_operands[2]);

#endif /* code_h */
