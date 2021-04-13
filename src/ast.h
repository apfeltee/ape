#pragma once

#ifndef APE_AMALGAMATED
#include "common.h"
#include "collections.h"
#include "token.h"
#endif


typedef struct expression expression_t;
typedef struct statement statement_t;
typedef struct code_block code_block_t;

struct code_block
{
    ptrarray(statement_t) *statements;
};

typedef struct map_literal {
    ptrarray(expression_t) *keys;
    ptrarray(expression_t) *values;
} map_literal_t;

typedef enum {
    OPERATOR_NONE,
    OPERATOR_ASSIGN,
    OPERATOR_PLUS,
    OPERATOR_MINUS,
    OPERATOR_BANG,
    OPERATOR_ASTERISK,
    OPERATOR_SLASH,
    OPERATOR_LT,
    OPERATOR_LTE,
    OPERATOR_GT,
    OPERATOR_GTE,
    OPERATOR_EQ,
    OPERATOR_NOT_EQ,
    OPERATOR_MODULUS,
    OPERATOR_LOGICAL_AND,
    OPERATOR_LOGICAL_OR,
    OPERATOR_BIT_AND,
    OPERATOR_BIT_OR,
    OPERATOR_BIT_XOR,
    OPERATOR_LSHIFT,
    OPERATOR_RSHIFT,
} operator_t;

typedef struct prefix {
    operator_t op;
    expression_t *right;
} prefix_expression_t;

typedef struct infix {
    operator_t op;
    expression_t *left;
    expression_t *right;
} infix_expression_t;

typedef struct if_case {
    expression_t *test;
    code_block_t *consequence;
} if_case_t;

typedef struct fn_literal {
    char *name;
    array(ident_t) *params;
    code_block_t *body;
} fn_literal_t;

typedef struct call_expression {
    expression_t *function;
    ptrarray(expression_t) *args;
} call_expression_t;

typedef struct index_expression {
    expression_t *left;
    expression_t *index;
} index_expression_t;

typedef struct assign_expression {
    expression_t *dest;
    expression_t *source;
} assign_expression_t;

typedef struct logical_expression {
    operator_t op;
    expression_t *left;
    expression_t *right;
} logical_expression_t;

typedef enum expression_type {
    EXPRESSION_NONE,
    EXPRESSION_IDENT,
    EXPRESSION_NUMBER_LITERAL,
    EXPRESSION_BOOL_LITERAL,
    EXPRESSION_STRING_LITERAL,
    EXPRESSION_NULL_LITERAL,
    EXPRESSION_ARRAY_LITERAL,
    EXPRESSION_MAP_LITERAL,
    EXPRESSION_PREFIX,
    EXPRESSION_INFIX,
    EXPRESSION_FUNCTION_LITERAL,
    EXPRESSION_CALL,
    EXPRESSION_INDEX,
    EXPRESSION_ASSIGN,
    EXPRESSION_LOGICAL,
} expression_type_t;

typedef struct ident {
    char *value;
    src_pos_t pos;
} ident_t;

typedef struct expression {
    expression_type_t type;
    union {
        ident_t ident;
        double number_literal;
        bool bool_literal;
        char *string_literal;
        ptrarray(expression_t) *array;
        map_literal_t map;
        prefix_expression_t prefix;
        infix_expression_t infix;
        fn_literal_t fn_literal;
        call_expression_t call_expr;
        index_expression_t index_expr;
        assign_expression_t assign;
        logical_expression_t logical;
    };
    src_pos_t pos;
} expression_t;

typedef enum statement_type {
    STATEMENT_NONE,
    STATEMENT_DEFINE,
    STATEMENT_IF,
    STATEMENT_RETURN_VALUE,
    STATEMENT_EXPRESSION,
    STATEMENT_WHILE_LOOP,
    STATEMENT_BREAK,
    STATEMENT_CONTINUE,
    STATEMENT_FOREACH,
    STATEMENT_FOR_LOOP,
    STATEMENT_BLOCK,
    STATEMENT_IMPORT,
    STATEMENT_RECOVER,
} statement_type_t;

typedef struct define_statement {
    ident_t name;
    expression_t *value;
    bool assignable;
} define_statement_t;

typedef struct if_statement {
    ptrarray(if_case_t) *cases;
    code_block_t *alternative;
} if_statement_t;

typedef struct while_loop_statement {
    expression_t *test;
    code_block_t *body;
} while_loop_statement_t;

typedef struct foreach_statement {
    ident_t iterator;
    expression_t *source;
    code_block_t *body;
} foreach_statement_t;

typedef struct for_loop_statement {
    statement_t *init;
    expression_t *test;
    expression_t *update;
    code_block_t *body;
} for_loop_statement_t;

typedef struct import_statement {
    char *path;
} import_statement_t;

typedef struct recover_statement {
    ident_t error_ident;
    code_block_t *body;
} recover_statement_t;

typedef struct statement {
    statement_type_t type;
    union {
        define_statement_t define;
        if_statement_t if_statement;
        expression_t *return_value;
        expression_t *expression;
        while_loop_statement_t while_loop;
        foreach_statement_t foreach;
        for_loop_statement_t for_loop;
        code_block_t *block;
        import_statement_t import;
        recover_statement_t recover;
    };
    src_pos_t pos;
} statement_t;

APE_INTERNAL char* statements_to_string(ptrarray(statement_t) *statements);

APE_INTERNAL statement_t* statement_make_define(ident_t name, expression_t *value, bool assignable);
APE_INTERNAL statement_t* statement_make_if(ptrarray(if_case_t) *cases, code_block_t *alternative);
APE_INTERNAL statement_t* statement_make_return(expression_t *value);
APE_INTERNAL statement_t* statement_make_expression(expression_t *value);
APE_INTERNAL statement_t* statement_make_while_loop(expression_t *test, code_block_t *body);
APE_INTERNAL statement_t* statement_make_break(void);
APE_INTERNAL statement_t* statement_make_foreach(ident_t iterator, expression_t *source, code_block_t *body);
APE_INTERNAL statement_t* statement_make_for_loop(statement_t *init, expression_t *test, expression_t *update, code_block_t *body);
APE_INTERNAL statement_t* statement_make_continue(void);
APE_INTERNAL statement_t* statement_make_block(code_block_t *block);
APE_INTERNAL statement_t* statement_make_import(char *path);
APE_INTERNAL statement_t* statement_make_recover(ident_t error_ident, code_block_t *body);

APE_INTERNAL void statement_destroy(statement_t *stmt);

APE_INTERNAL statement_t* statement_copy(statement_t *stmt);

APE_INTERNAL code_block_t* code_block_make(ptrarray(statement_t) *statements);
APE_INTERNAL void code_block_destroy(code_block_t *stmt);
APE_INTERNAL code_block_t* code_block_copy(code_block_t *block);

APE_INTERNAL expression_t* expression_make_ident(ident_t ident);
APE_INTERNAL expression_t* expression_make_number_literal(double val);
APE_INTERNAL expression_t* expression_make_bool_literal(bool val);
APE_INTERNAL expression_t* expression_make_string_literal(char *value);
APE_INTERNAL expression_t* expression_make_null_literal(void);
APE_INTERNAL expression_t* expression_make_array_literal(ptrarray(expression_t) *values);
APE_INTERNAL expression_t* expression_make_map_literal(ptrarray(expression_t) *keys, ptrarray(expression_t) *values);
APE_INTERNAL expression_t* expression_make_prefix(operator_t op, expression_t *right);
APE_INTERNAL expression_t* expression_make_infix(operator_t op, expression_t *left, expression_t *right);
APE_INTERNAL expression_t* expression_make_fn_literal(array(ident_t) *params, code_block_t *body);
APE_INTERNAL expression_t* expression_make_call(expression_t *function, ptrarray(expression_t) *args);
APE_INTERNAL expression_t* expression_make_index(expression_t *left, expression_t *index);
APE_INTERNAL expression_t* expression_make_assign(expression_t *dest, expression_t *source);
APE_INTERNAL expression_t* expression_make_logical(operator_t op, expression_t *left, expression_t *right);

APE_INTERNAL void expression_destroy(expression_t *expr);

APE_INTERNAL expression_t* expression_copy(expression_t *expr);

APE_INTERNAL void statement_to_string(const statement_t *stmt, strbuf_t *buf);
APE_INTERNAL void expression_to_string(expression_t *expr, strbuf_t *buf);

APE_INTERNAL void code_block_to_string(const code_block_t *stmt, strbuf_t *buf);
APE_INTERNAL const char* operator_to_string(operator_t op);

APE_INTERNAL const char *expression_type_to_string(expression_type_t type);

APE_INTERNAL void fn_literal_deinit(fn_literal_t *fn);

APE_INTERNAL ident_t ident_make(token_t tok);
APE_INTERNAL ident_t ident_copy(ident_t ident);
APE_INTERNAL void ident_deinit(ident_t *ident);

APE_INTERNAL if_case_t *if_case_make(expression_t *test, code_block_t *consequence);
APE_INTERNAL void if_case_destroy(if_case_t *cond);

#endif /* ast_h */
