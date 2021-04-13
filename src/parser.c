#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#ifndef APE_AMALGAMATED
#include "parser.h"
#include "error.h"
#endif

typedef enum precedence {
    PRECEDENCE_LOWEST = 0,
    PRECEDENCE_ASSIGN,      // a = b
    PRECEDENCE_LOGICAL_OR,  // ||
    PRECEDENCE_LOGICAL_AND, // &&
    PRECEDENCE_BIT_OR,      // |
    PRECEDENCE_BIT_XOR,     // ^
    PRECEDENCE_BIT_AND,     // &
    PRECEDENCE_EQUALS,      // == !=
    PRECEDENCE_LESSGREATER, // >, >=, <, <=
    PRECEDENCE_SHIFT,       // << >>
    PRECEDENCE_SUM,         // + -
    PRECEDENCE_PRODUCT,     // * / %
    PRECEDENCE_PREFIX,      // -X or !X
    PRECEDENCE_CALL,        // myFunction(X)
    PRECEDENCE_INDEX,       // arr[x]
    PRECEDENCE_DOT,         // obj.foo
} precedence_t;

static void next_token(parser_t *parser);
static statement_t* parse_statement(parser_t *p);
static statement_t* parse_define_statement(parser_t *p);
static statement_t* parse_if_statement(parser_t *p);
static statement_t* parse_return_statement(parser_t *p);
static statement_t* parse_expression_statement(parser_t *p);
static statement_t* parse_while_loop_statement(parser_t *p);
static statement_t* parse_break_statement(parser_t *p);
static statement_t* parse_continue_statement(parser_t *p);
static statement_t* parse_for_loop_statement(parser_t *p);
static statement_t* parse_foreach(parser_t *p);
static statement_t* parse_classic_for_loop(parser_t *p);
static statement_t* parse_function_statement(parser_t *p);
static statement_t* parse_block_statement(parser_t *p);
static statement_t* parse_import_statement(parser_t *p);
static statement_t* parse_recover_statement(parser_t *p);

static code_block_t* parse_code_block(parser_t *p);

static expression_t* parse_expression(parser_t *p, precedence_t prec);
static expression_t* parse_identifier(parser_t *p);
static expression_t* parse_number_literal(parser_t *p);
static expression_t* parse_bool_literal(parser_t *p);
static expression_t* parse_string_literal(parser_t *p);
static expression_t* parse_null_literal(parser_t *p);
static expression_t* parse_array_literal(parser_t *p);
static expression_t* parse_map_literal(parser_t *p);
static expression_t* parse_prefix_expression(parser_t *p);
static expression_t* parse_infix_expression(parser_t *p, expression_t *left);
static expression_t* parse_grouped_expression(parser_t *p);
static expression_t* parse_function_literal(parser_t *p);
static bool parse_function_parameters(parser_t *p, array(ident_t) *out_params);
static expression_t* parse_call_expression(parser_t *p, expression_t *left);
static ptrarray(expression_t)* parse_expression_list(parser_t *p, token_type_t start_token, token_type_t end_token, bool trailing_comma_allowed);
static expression_t* parse_index_expression(parser_t *p, expression_t *left);
static expression_t* parse_dot_expression(parser_t *p, expression_t *left);
static expression_t* parse_assign_expression(parser_t *p, expression_t *left);
static expression_t* parse_logical_expression(parser_t *p, expression_t *left);

static precedence_t get_precedence(token_type_t tk);
static operator_t token_to_operator(token_type_t tk);

static bool cur_token_is(parser_t *p, token_type_t type);
static bool peek_token_is(parser_t *p, token_type_t type);
static bool expect_current(parser_t *p, token_type_t type);

static char escape_char(const char c);
static char* process_and_copy_string(const char *input, size_t len);

parser_t* parser_make(const ape_config_t *config, ptrarray(errortag_t) *errors) {
    parser_t *parser = ape_malloc(sizeof(parser_t));
    memset(parser, 0, sizeof(parser_t));
    APE_ASSERT(config);

    parser->config = config;
    parser->errors = errors;

    parser->prefix_parse_fns[TOKEN_IDENT] = parse_identifier;
    parser->prefix_parse_fns[TOKEN_NUMBER] = parse_number_literal;
    parser->prefix_parse_fns[TOKEN_TRUE] = parse_bool_literal;
    parser->prefix_parse_fns[TOKEN_FALSE] = parse_bool_literal;
    parser->prefix_parse_fns[TOKEN_STRING] = parse_string_literal;
    parser->prefix_parse_fns[TOKEN_NULL] = parse_null_literal;
    parser->prefix_parse_fns[TOKEN_BANG] = parse_prefix_expression;
    parser->prefix_parse_fns[TOKEN_MINUS] = parse_prefix_expression;
    parser->prefix_parse_fns[TOKEN_LPAREN] = parse_grouped_expression;
    parser->prefix_parse_fns[TOKEN_FUNCTION] = parse_function_literal;
    parser->prefix_parse_fns[TOKEN_LBRACKET] = parse_array_literal;
    parser->prefix_parse_fns[TOKEN_LBRACE] = parse_map_literal;

    parser->infix_parse_fns[TOKEN_PLUS] = parse_infix_expression;
    parser->infix_parse_fns[TOKEN_MINUS] = parse_infix_expression;
    parser->infix_parse_fns[TOKEN_SLASH] = parse_infix_expression;
    parser->infix_parse_fns[TOKEN_ASTERISK] = parse_infix_expression;
    parser->infix_parse_fns[TOKEN_PERCENT] = parse_infix_expression;
    parser->infix_parse_fns[TOKEN_EQ] = parse_infix_expression;
    parser->infix_parse_fns[TOKEN_NOT_EQ] = parse_infix_expression;
    parser->infix_parse_fns[TOKEN_LT] = parse_infix_expression;
    parser->infix_parse_fns[TOKEN_LTE] = parse_infix_expression;
    parser->infix_parse_fns[TOKEN_GT] = parse_infix_expression;
    parser->infix_parse_fns[TOKEN_GTE] = parse_infix_expression;
    parser->infix_parse_fns[TOKEN_LPAREN] = parse_call_expression;
    parser->infix_parse_fns[TOKEN_LBRACKET] = parse_index_expression;
    parser->infix_parse_fns[TOKEN_ASSIGN] = parse_assign_expression;
    parser->infix_parse_fns[TOKEN_PLUS_ASSIGN] = parse_assign_expression;
    parser->infix_parse_fns[TOKEN_MINUS_ASSIGN] = parse_assign_expression;
    parser->infix_parse_fns[TOKEN_SLASH_ASSIGN] = parse_assign_expression;
    parser->infix_parse_fns[TOKEN_ASTERISK_ASSIGN] = parse_assign_expression;
    parser->infix_parse_fns[TOKEN_PERCENT_ASSIGN] = parse_assign_expression;
    parser->infix_parse_fns[TOKEN_BIT_AND_ASSIGN] = parse_assign_expression;
    parser->infix_parse_fns[TOKEN_BIT_OR_ASSIGN] = parse_assign_expression;
    parser->infix_parse_fns[TOKEN_BIT_XOR_ASSIGN] = parse_assign_expression;
    parser->infix_parse_fns[TOKEN_LSHIFT_ASSIGN] = parse_assign_expression;
    parser->infix_parse_fns[TOKEN_RSHIFT_ASSIGN] = parse_assign_expression;
    parser->infix_parse_fns[TOKEN_DOT] = parse_dot_expression;
    parser->infix_parse_fns[TOKEN_AND] = parse_logical_expression;
    parser->infix_parse_fns[TOKEN_OR] = parse_logical_expression;
    parser->infix_parse_fns[TOKEN_BIT_AND] = parse_infix_expression;
    parser->infix_parse_fns[TOKEN_BIT_OR] = parse_infix_expression;
    parser->infix_parse_fns[TOKEN_BIT_XOR] = parse_infix_expression;
    parser->infix_parse_fns[TOKEN_LSHIFT] = parse_infix_expression;
    parser->infix_parse_fns[TOKEN_RSHIFT] = parse_infix_expression;

    parser->depth = 0;

    return parser;
}

void parser_destroy(parser_t *parser) {
    if (!parser) {
        return;
    }
    memset(parser, 0, sizeof(parser_t));
    ape_free(parser);
}

ptrarray(statement_t)* parser_parse_all(parser_t *parser,  const char *input, compiled_file_t *file) {
    parser->depth = 0;

    lexer_init(&parser->lexer, input, file);

    next_token(parser);
    next_token(parser);

    ptrarray(statement_t)* statements = ptrarray_make();

    while (!cur_token_is(parser, TOKEN_EOF)) {
        if (cur_token_is(parser, TOKEN_SEMICOLON)) {
            next_token(parser);
            continue;
        }
        statement_t *stmt = parse_statement(parser);
        if (stmt) {
            ptrarray_add(statements, stmt);
        } else {
            goto err;
        }
    }

    if (ptrarray_count(parser->errors) > 0) {
        goto err;
    }

    return statements;
err:
    ptrarray_destroy_with_items(statements, statement_destroy);
    return NULL;
}

// INTERNAL
static void next_token(parser_t *p) {
    p->cur_token = p->peek_token;
    p->peek_token = lexer_next_token(&p->lexer);
}

static statement_t* parse_statement(parser_t *p) {
    src_pos_t pos = p->cur_token.pos;

    statement_t *res = NULL;
    switch (p->cur_token.type) {
        case TOKEN_VAR:
        case TOKEN_CONST: {
            res = parse_define_statement(p);
            break;
        }
        case TOKEN_IF: {
            res = parse_if_statement(p);
            break;
        }
        case TOKEN_RETURN: {
            res = parse_return_statement(p);
            break;
        }
        case TOKEN_WHILE: {
            res = parse_while_loop_statement(p);
            break;
        }
        case TOKEN_BREAK: {
            res = parse_break_statement(p);
            break;
        }
        case TOKEN_FOR: {
            res = parse_for_loop_statement(p);
            break;
        }
        case TOKEN_FUNCTION: {
            if (peek_token_is(p, TOKEN_IDENT)) {
                res = parse_function_statement(p);
            } else {
                res = parse_expression_statement(p);
            }
            break;
        }
        case TOKEN_LBRACE: {
            if (p->config->repl_mode && p->depth == 0) {
                res = parse_expression_statement(p);
            } else {
                res = parse_block_statement(p);
            }
            break;
        }
        case TOKEN_CONTINUE: {
            res = parse_continue_statement(p);
            break;
        }
        case TOKEN_IMPORT: {
            res = parse_import_statement(p);
            break;
        }
        case TOKEN_RECOVER: {
            res = parse_recover_statement(p);
            break;
        }
        default: {
            res = parse_expression_statement(p);
            break;
        }
    }
    if (res) {
        res->pos = pos;
    }
    return res;
}

static statement_t* parse_define_statement(parser_t *p) {
    ident_t name_ident;
    expression_t *value = NULL;

    bool assignable = cur_token_is(p, TOKEN_VAR);

    next_token(p);

    if (!expect_current(p, TOKEN_IDENT)) {
        goto err;
    }

    name_ident = ident_make(p->cur_token);

    next_token(p);

    if (!expect_current(p, TOKEN_ASSIGN)) {
        goto err;
    }

    next_token(p);

    value = parse_expression(p, PRECEDENCE_LOWEST);
    if (!value) {
        goto err;
    }

    if (value->type == EXPRESSION_FUNCTION_LITERAL) {
        value->fn_literal.name = ape_strdup(name_ident.value);
    }

    return statement_make_define(name_ident, value, assignable);
err:
    expression_destroy(value);
    ident_deinit(&name_ident);
    return NULL;
}

static statement_t* parse_if_statement(parser_t *p) {
    ptrarray(if_case_t) *cases = ptrarray_make();
    code_block_t *alternative = NULL;

    next_token(p);

    if (!expect_current(p, TOKEN_LPAREN)) {
        goto err;
    }

    next_token(p);

    if_case_t *cond = if_case_make(NULL, NULL);
    ptrarray_add(cases, cond);

    cond->test = parse_expression(p, PRECEDENCE_LOWEST);
    if (!cond->test) {
        goto err;
    }

    if (!expect_current(p, TOKEN_RPAREN)) {
        goto err;
    }

    next_token(p);

    cond->consequence = parse_code_block(p);
    if (!cond->consequence) {
        goto err;
    }

    while (cur_token_is(p, TOKEN_ELSE)) {
        next_token(p);

        if (cur_token_is(p, TOKEN_IF)) {
            next_token(p);

            if (!expect_current(p, TOKEN_LPAREN)) {
                goto err;
            }

            next_token(p);

            if_case_t *elif = if_case_make(NULL, NULL);
            ptrarray_add(cases, elif);

            elif->test = parse_expression(p, PRECEDENCE_LOWEST);
            if (!cond->test) {
                goto err;
            }

            if (!expect_current(p, TOKEN_RPAREN)) {
                goto err;
            }

            next_token(p);

            elif->consequence = parse_code_block(p);
            if (!cond->consequence) {
                goto err;
            }
        } else {
            alternative = parse_code_block(p);
            if (!alternative) {
                goto err;
            }
        }
    }

    return statement_make_if(cases, alternative);
err:
    ptrarray_destroy_with_items(cases, if_case_destroy);
    code_block_destroy(alternative);
    return NULL;
}

static statement_t* parse_return_statement(parser_t *p) {
    expression_t *expr = NULL;

    next_token(p);

    if (!cur_token_is(p, TOKEN_SEMICOLON) && !cur_token_is(p, TOKEN_RBRACE) && !cur_token_is(p, TOKEN_EOF)) {
        expr = parse_expression(p, PRECEDENCE_LOWEST);
        if (!expr) {
            return NULL;
        }
    }

    return statement_make_return(expr);
}

static statement_t* parse_expression_statement(parser_t *p) {
    expression_t *expr = parse_expression(p, PRECEDENCE_LOWEST);
    if (!expr) {
        return NULL;
    }

    if (expr && (!p->config->repl_mode || p->depth > 0)) {
        if (expr->type != EXPRESSION_ASSIGN && expr->type != EXPRESSION_CALL) {
            errortag_t *err = error_makef(ERROR_PARSING, expr->pos,
                                       "Only assignments and function calls can be expression statements");
            ptrarray_add(p->errors, err);
            expression_destroy(expr);
            return NULL;
        }
    }

    return statement_make_expression(expr);
}

static statement_t* parse_while_loop_statement(parser_t *p) {
    expression_t *test = NULL;

    next_token(p);

    if (!expect_current(p, TOKEN_LPAREN)) {
        goto err;
    }

    next_token(p);

    test = parse_expression(p, PRECEDENCE_LOWEST);
    if (!test) {
        goto err;
    }

    if (!expect_current(p, TOKEN_RPAREN)) {
        goto err;
    }

    next_token(p);

    code_block_t *body = parse_code_block(p);
    if (!body) {
        goto err;
    }

    return statement_make_while_loop(test, body);
err:
    expression_destroy(test);
    return NULL;
}

static statement_t* parse_break_statement(parser_t *p) {
    next_token(p);
    return statement_make_break();
}

static statement_t* parse_continue_statement(parser_t *p) {
    next_token(p);
    return statement_make_continue();
}

static statement_t* parse_block_statement(parser_t *p) {
    code_block_t *block = parse_code_block(p);
    if (!block) {
        return NULL;
    }
    return statement_make_block(block);
}

static statement_t* parse_import_statement(parser_t *p) {
    next_token(p);

    if (!expect_current(p, TOKEN_STRING)) {
        return NULL;
    }

    char *processed_name = process_and_copy_string(p->cur_token.literal, p->cur_token.len);
    if (!processed_name) {
        errortag_t *err = error_make(ERROR_PARSING, p->cur_token.pos, "Error when parsing module name");
        ptrarray_add(p->errors, err);
        return NULL;
    }
    next_token(p);
    statement_t *result = statement_make_import(processed_name);
    return result;
}

static statement_t* parse_recover_statement(parser_t *p) {
    next_token(p);

    if (!expect_current(p, TOKEN_LPAREN)) {
        return NULL;
    }
    next_token(p);


    if (!expect_current(p, TOKEN_IDENT)) {
        return NULL;
    }

    ident_t error_ident = ident_make(p->cur_token);
    next_token(p);

    if (!expect_current(p, TOKEN_RPAREN)) {
        goto err;
    }
    next_token(p);

    code_block_t *body = parse_code_block(p);
    if (!body) {
        goto err;
    }

    return statement_make_recover(error_ident, body);
err:
    ident_deinit(&error_ident);
    return NULL;

}

static statement_t* parse_for_loop_statement(parser_t *p) {
    next_token(p);

    if (!expect_current(p, TOKEN_LPAREN)) {
        return NULL;
    }

    next_token(p);

    if (cur_token_is(p, TOKEN_IDENT) && peek_token_is(p, TOKEN_IN)) {
        return parse_foreach(p);
    } else {
        return parse_classic_for_loop(p);
    }
}

static statement_t* parse_foreach(parser_t *p) {
    expression_t *source = NULL;
    ident_t iterator_ident = ident_make(p->cur_token);

    next_token(p);

    if (!expect_current(p, TOKEN_IN)) {
        goto err;
    }

    next_token(p);

    source = parse_expression(p, PRECEDENCE_LOWEST);
    if (!source) {
        goto err;
    }

    if (!expect_current(p, TOKEN_RPAREN)) {
        goto err;
    }

    next_token(p);

    code_block_t *body = parse_code_block(p);
    if (!body) {
        goto err;
    }

    return statement_make_foreach(iterator_ident, source, body);
err:
    ident_deinit(&iterator_ident);
    expression_destroy(source);
    return NULL;
}

static statement_t* parse_classic_for_loop(parser_t *p) {
    statement_t *init = NULL;
    expression_t *test = NULL;
    expression_t *update = NULL;
    code_block_t *body = NULL;

    if (!cur_token_is(p, TOKEN_SEMICOLON)) {
        init = parse_statement(p);
        if (!init) {
            goto err;
        }
        if (init->type != STATEMENT_DEFINE && init->type != STATEMENT_EXPRESSION) {
            errortag_t *err = error_makef(ERROR_PARSING, init->pos,
                                       "for loop's init clause should be a define statement or an expression");
            ptrarray_add(p->errors, err);
            goto err;
        }
        if (!expect_current(p, TOKEN_SEMICOLON)) {
            goto err;
        }
    }

    next_token(p);

    if (!cur_token_is(p, TOKEN_SEMICOLON)) {
        test = parse_expression(p, PRECEDENCE_LOWEST);
        if (!test) {
            goto err;
        }
        if (!expect_current(p, TOKEN_SEMICOLON)) {
            goto err;
        }
    }

    next_token(p);

    if (!cur_token_is(p, TOKEN_RPAREN)) {
        update = parse_expression(p, PRECEDENCE_LOWEST);
        if (!update) {
            goto err;
        }
        if (!expect_current(p, TOKEN_RPAREN)) {
            goto err;
        }
    }

    next_token(p);

    body = parse_code_block(p);
    if (!body) {
        goto err;
    }

    return statement_make_for_loop(init, test, update, body);
err:
    statement_destroy(init);
    expression_destroy(test);
    expression_destroy(update);
    code_block_destroy(body);
    return NULL;
}

static statement_t* parse_function_statement(parser_t *p) {
    ident_t name_ident;

    expression_t* value = NULL;

    src_pos_t pos = p->cur_token.pos;

    next_token(p);

    if (!expect_current(p, TOKEN_IDENT)) {
        goto err;
    }

    name_ident = ident_make(p->cur_token);

    next_token(p);

    value = parse_function_literal(p);
    if (!value) {
        goto err;
    }

    value->pos = pos;
    value->fn_literal.name = ape_strdup(name_ident.value);

    return statement_make_define(name_ident, value, false);
err:
    expression_destroy(value);
    ident_deinit(&name_ident);
    return NULL;
}

static code_block_t* parse_code_block(parser_t *p) {
    if (!expect_current(p, TOKEN_LBRACE)) {
        return NULL;
    }

    next_token(p);
    p->depth++;

    ptrarray(statement_t)* statements = ptrarray_make();

    while (!cur_token_is(p, TOKEN_RBRACE)) {
        if (cur_token_is(p, TOKEN_EOF)) {
            errortag_t *err = error_make(ERROR_PARSING, p->cur_token.pos, "Unexpected EOF");
            ptrarray_add(p->errors, err);
            goto err;
        }
        if (cur_token_is(p, TOKEN_SEMICOLON)) {
            next_token(p);
            continue;
        }
        statement_t *stmt = parse_statement(p);
        if (stmt) {
            ptrarray_add(statements, stmt);
        } else {
            goto err;
        }
    }

    next_token(p);

    p->depth--;
    
    return code_block_make(statements);

err:
    p->depth--;
    ptrarray_destroy_with_items(statements, statement_destroy);
    return NULL;
}

static expression_t* parse_expression(parser_t *p, precedence_t prec) {
    src_pos_t pos = p->cur_token.pos;

    if (p->cur_token.type == TOKEN_ILLEGAL) {
        errortag_t *err = error_make(ERROR_PARSING, p->cur_token.pos, "Illegal token");
        ptrarray_add(p->errors, err);
        return NULL;
    }

    prefix_parse_fn prefix = p->prefix_parse_fns[p->cur_token.type];
    if (!prefix) {
        char *literal = token_duplicate_literal(&p->cur_token);
        errortag_t *err = error_makef(ERROR_PARSING, p->cur_token.pos,
                                  "No prefix parse function for \"%s\" found", literal);
        ptrarray_add(p->errors, err);
        ape_free(literal);
        return NULL;
    }

    expression_t *left_expr = prefix(p);
    if (!left_expr) {
        return NULL;
    }
    left_expr->pos = pos;

    while (!cur_token_is(p, TOKEN_SEMICOLON) && prec < get_precedence(p->cur_token.type)) {
        infix_parse_fn infix = p->infix_parse_fns[p->cur_token.type];
        if (!infix) {
            return left_expr;
        }
        pos = p->cur_token.pos;
        expression_t *new_left_expr = infix(p, left_expr);
        if (!new_left_expr) {
            expression_destroy(left_expr);
            return NULL;
        }
        new_left_expr->pos = pos;
        left_expr = new_left_expr;
    }

    return left_expr;
}

static expression_t* parse_identifier(parser_t *p) {
    ident_t ident = ident_make(p->cur_token);
    expression_t *res = expression_make_ident(ident);
    next_token(p);
    return res;
}

static expression_t* parse_number_literal(parser_t *p) {
    char *end;
    double number = 0;
    errno = 0;
    number = strtod(p->cur_token.literal, &end);
    long parsed_len = end - p->cur_token.literal;
    if (errno || parsed_len != p->cur_token.len) {
        char *literal = token_duplicate_literal(&p->cur_token);
        errortag_t *err = error_makef(ERROR_PARSING, p->cur_token.pos,
                                  "Parsing number literal \"%s\" failed", literal);
        ptrarray_add(p->errors, err);
        ape_free(literal);
        return NULL;
    }
    next_token(p);
    return expression_make_number_literal(number);
}

static expression_t* parse_bool_literal(parser_t *p) {
    expression_t *res = expression_make_bool_literal(p->cur_token.type == TOKEN_TRUE);
    next_token(p);
    return res;
}

static expression_t* parse_string_literal(parser_t *p) {
    char *processed_literal = process_and_copy_string(p->cur_token.literal, p->cur_token.len);
    if (!processed_literal) {
        errortag_t *err = error_make(ERROR_PARSING, p->cur_token.pos, "Error when parsing string literal");
        ptrarray_add(p->errors, err);
        return NULL;
    }
    next_token(p);
    return expression_make_string_literal(processed_literal);
}

static expression_t* parse_null_literal(parser_t *p) {
    next_token(p);
    return expression_make_null_literal();
}

static expression_t* parse_array_literal(parser_t *p) {
    ptrarray(expression_t) *array = parse_expression_list(p, TOKEN_LBRACKET, TOKEN_RBRACKET, true);
    if (!array) {
        return NULL;
    }
    return expression_make_array_literal(array);
}

static expression_t* parse_map_literal(parser_t *p) {
    ptrarray(expression_t) *keys = ptrarray_make();
    ptrarray(expression_t) *values = ptrarray_make();

    next_token(p);

    while (!cur_token_is(p, TOKEN_RBRACE)) {
        expression_t *key = NULL;
        if (cur_token_is(p, TOKEN_IDENT)) {
            key = expression_make_string_literal(token_duplicate_literal(&p->cur_token));
            key->pos = p->cur_token.pos;
            next_token(p);
        } else {
            key = parse_expression(p, PRECEDENCE_LOWEST);
            if (!key) {
                goto err;
            }
            switch (key->type) {
                case EXPRESSION_STRING_LITERAL:
                case EXPRESSION_NUMBER_LITERAL:
                case EXPRESSION_BOOL_LITERAL: {
                    break;
                }
                default: {
                    errortag_t *err = error_makef(ERROR_PARSING, key->pos, "Invalid map literal key type");
                    ptrarray_add(p->errors, err);
                    expression_destroy(key);
                    goto err;
                }
            }
        }

        if (!key) {
            goto err;
        }

        ptrarray_add(keys, key);

        if (!expect_current(p, TOKEN_COLON)) {
            goto err;
        }

        next_token(p);

        expression_t *value = parse_expression(p, PRECEDENCE_LOWEST);
        if (!value) {
            goto err;
        }
        ptrarray_add(values, value);

        if (cur_token_is(p, TOKEN_RBRACE)) {
            break;
        }

        if (!expect_current(p, TOKEN_COMMA)) {
            goto err;
        }

        next_token(p);
    }

    next_token(p);

    return expression_make_map_literal(keys, values);
err:
    ptrarray_destroy_with_items(keys, expression_destroy);
    ptrarray_destroy_with_items(values, expression_destroy);
    return NULL;
}

static expression_t* parse_prefix_expression(parser_t *p) {
    operator_t op = token_to_operator(p->cur_token.type);
    next_token(p);
    expression_t *right = parse_expression(p, PRECEDENCE_PREFIX);
    if (!right) {
        return NULL;
    }
    return expression_make_prefix(op, right);
}

static expression_t* parse_infix_expression(parser_t *p, expression_t *left) {
    operator_t op = token_to_operator(p->cur_token.type);
    precedence_t prec = get_precedence(p->cur_token.type);
    next_token(p);
    expression_t *right = parse_expression(p, prec);
    if (!right) {
        return NULL;
    }
    return expression_make_infix(op, left, right);
}

static expression_t* parse_grouped_expression(parser_t *p) {
    next_token(p);
    expression_t *expr = parse_expression(p, PRECEDENCE_LOWEST);
    if (!expr || !expect_current(p, TOKEN_RPAREN)) {
        expression_destroy(expr);
        return NULL;
    }
    next_token(p);
    return expr;
}

static expression_t* parse_function_literal(parser_t *p) {
    p->depth += 1;
    array(ident) *params = NULL;
    code_block_t *body = NULL;

    if (cur_token_is(p, TOKEN_FUNCTION)) {
        next_token(p);
    }

    params = array_make(ident_t);

    bool ok = parse_function_parameters(p, params);

    if (!ok) {
        goto err;
    }

    body = parse_code_block(p);
    if (!body) {
        goto err;
    }

    p->depth -= 1;

    return expression_make_fn_literal(params, body);
err:
    array_destroy_with_items(params, ident_deinit);
    p->depth -= 1;
    return NULL;
}

static bool parse_function_parameters(parser_t *p, array(ident_t) *out_params) {
    if (!expect_current(p, TOKEN_LPAREN)) {
        return false;
    }

    next_token(p);

    if (cur_token_is(p, TOKEN_RPAREN)) {
        next_token(p);
        return true;
    }

    if (!expect_current(p, TOKEN_IDENT)) {
        return false;
    }

    ident_t ident = ident_make(p->cur_token);
    array_add(out_params, &ident);

    next_token(p);

    while (cur_token_is(p, TOKEN_COMMA)) {
        next_token(p);

        if (!expect_current(p, TOKEN_IDENT)) {
            return false;
        }

        ident_t ident = ident_make(p->cur_token);
        array_add(out_params, &ident);

        next_token(p);
    }

    if (!expect_current(p, TOKEN_RPAREN)) {
        return false;
    }

    next_token(p);

    return true;
}

static expression_t* parse_call_expression(parser_t *p, expression_t *left) {
    expression_t *function = left;
    ptrarray(expression_t) *args = parse_expression_list(p, TOKEN_LPAREN, TOKEN_RPAREN, false);
    if (!args) {
        return NULL;
    }
    return expression_make_call(function, args);
}

static ptrarray(expression_t)* parse_expression_list(parser_t *p, token_type_t start_token, token_type_t end_token, bool trailing_comma_allowed) {
    if (!expect_current(p, start_token)) {
        return NULL;
    }

    next_token(p);

    ptrarray(expression_t)* res = ptrarray_make();

    if (cur_token_is(p, end_token)) {
        next_token(p);
        return res;
    }

    expression_t *arg_expr = parse_expression(p, PRECEDENCE_LOWEST);
    if (!arg_expr) {
        goto err;
    }
    ptrarray_add(res, arg_expr);

    while (cur_token_is(p, TOKEN_COMMA)) {
        next_token(p);

        if (cur_token_is(p, end_token)) {
            break;
        }
        arg_expr = parse_expression(p, PRECEDENCE_LOWEST);
        if (!arg_expr) {
            goto err;
        }
        ptrarray_add(res, arg_expr);
    }

    if (!expect_current(p, end_token)) {
        goto err;
    }

    next_token(p);

    return res;
err:
    ptrarray_destroy_with_items(res, expression_destroy);
    return NULL;
}

static expression_t* parse_index_expression(parser_t *p, expression_t *left) {
    next_token(p);

    expression_t *index = parse_expression(p, PRECEDENCE_LOWEST);
    if (!index) {
        return NULL;
    }

    if (!expect_current(p, TOKEN_RBRACKET)) {
        expression_destroy(index);
        return NULL;
    }

    next_token(p);

    return expression_make_index(left, index);
}

static expression_t* parse_assign_expression(parser_t *p, expression_t *left) {
    expression_t *source = NULL;
    expression_t *left_copy = NULL;
    token_type_t assign_type = p->cur_token.type;

    next_token(p);

    source = parse_expression(p, PRECEDENCE_LOWEST);
    if (!source) {
        goto err;
    }

    switch (assign_type) {
        case TOKEN_PLUS_ASSIGN:
        case TOKEN_MINUS_ASSIGN:
        case TOKEN_SLASH_ASSIGN:
        case TOKEN_ASTERISK_ASSIGN:
        case TOKEN_PERCENT_ASSIGN:
        case TOKEN_BIT_AND_ASSIGN:
        case TOKEN_BIT_OR_ASSIGN:
        case TOKEN_BIT_XOR_ASSIGN:
        case TOKEN_LSHIFT_ASSIGN:
        case TOKEN_RSHIFT_ASSIGN:
        {
            operator_t op = token_to_operator(assign_type);
            expression_t *left_copy = expression_copy(left);
            if (!left_copy) {
                goto err;
            }
            src_pos_t pos = source->pos;
            expression_t *new_source = expression_make_infix(op, left_copy, source);
            if (!new_source) {
                goto err;
            }
            new_source->pos = pos;
            source = new_source;
            break;
        }
        case TOKEN_ASSIGN: break;
        default: APE_ASSERT(false); break;
    }

    return expression_make_assign(left, source);
err:
    expression_destroy(left_copy);
    expression_destroy(source);
    return NULL;
}

static expression_t* parse_logical_expression(parser_t *p, expression_t *left) {
    operator_t op = token_to_operator(p->cur_token.type);
    precedence_t prec = get_precedence(p->cur_token.type);
    next_token(p);
    expression_t *right = parse_expression(p, prec);
    if (!right) {
        return NULL;
    }
    return expression_make_logical(op, left, right);
}

static expression_t* parse_dot_expression(parser_t *p, expression_t *left) {
    next_token(p);
    
    if (!expect_current(p, TOKEN_IDENT)) {
        return NULL;
    }

    expression_t *index = expression_make_string_literal(token_duplicate_literal(&p->cur_token));
    if (!index) {
        return NULL;
    }
    index->pos = p->cur_token.pos;

    next_token(p);

    return expression_make_index(left, index);
}

static precedence_t get_precedence(token_type_t tk) {
    switch (tk) {
        case TOKEN_EQ:              return PRECEDENCE_EQUALS;
        case TOKEN_NOT_EQ:          return PRECEDENCE_EQUALS;
        case TOKEN_LT:              return PRECEDENCE_LESSGREATER;
        case TOKEN_LTE:             return PRECEDENCE_LESSGREATER;
        case TOKEN_GT:              return PRECEDENCE_LESSGREATER;
        case TOKEN_GTE:             return PRECEDENCE_LESSGREATER;
        case TOKEN_PLUS:            return PRECEDENCE_SUM;
        case TOKEN_MINUS:           return PRECEDENCE_SUM;
        case TOKEN_SLASH:           return PRECEDENCE_PRODUCT;
        case TOKEN_ASTERISK:        return PRECEDENCE_PRODUCT;
        case TOKEN_PERCENT:         return PRECEDENCE_PRODUCT;
        case TOKEN_LPAREN:          return PRECEDENCE_CALL;
        case TOKEN_LBRACKET:        return PRECEDENCE_INDEX;
        case TOKEN_ASSIGN:          return PRECEDENCE_ASSIGN;
        case TOKEN_PLUS_ASSIGN:     return PRECEDENCE_ASSIGN;
        case TOKEN_MINUS_ASSIGN:    return PRECEDENCE_ASSIGN;
        case TOKEN_ASTERISK_ASSIGN: return PRECEDENCE_ASSIGN;
        case TOKEN_SLASH_ASSIGN:    return PRECEDENCE_ASSIGN;
        case TOKEN_PERCENT_ASSIGN:  return PRECEDENCE_ASSIGN;
        case TOKEN_BIT_AND_ASSIGN:  return PRECEDENCE_ASSIGN;
        case TOKEN_BIT_OR_ASSIGN:   return PRECEDENCE_ASSIGN;
        case TOKEN_BIT_XOR_ASSIGN:  return PRECEDENCE_ASSIGN;
        case TOKEN_LSHIFT_ASSIGN:   return PRECEDENCE_ASSIGN;
        case TOKEN_RSHIFT_ASSIGN:   return PRECEDENCE_ASSIGN;
        case TOKEN_DOT:             return PRECEDENCE_DOT;
        case TOKEN_AND:             return PRECEDENCE_LOGICAL_AND;
        case TOKEN_OR:              return PRECEDENCE_LOGICAL_OR;
        case TOKEN_BIT_OR:          return PRECEDENCE_BIT_OR;
        case TOKEN_BIT_XOR:         return PRECEDENCE_BIT_XOR;
        case TOKEN_BIT_AND:         return PRECEDENCE_BIT_AND;
        case TOKEN_LSHIFT:          return PRECEDENCE_SHIFT;
        case TOKEN_RSHIFT:          return PRECEDENCE_SHIFT;
        default:                    return PRECEDENCE_LOWEST;
    }
}

static operator_t token_to_operator(token_type_t tk) {
    switch (tk) {
        case TOKEN_ASSIGN:          return OPERATOR_ASSIGN;
        case TOKEN_PLUS:            return OPERATOR_PLUS;
        case TOKEN_MINUS:           return OPERATOR_MINUS;
        case TOKEN_BANG:            return OPERATOR_BANG;
        case TOKEN_ASTERISK:        return OPERATOR_ASTERISK;
        case TOKEN_SLASH:           return OPERATOR_SLASH;
        case TOKEN_LT:              return OPERATOR_LT;
        case TOKEN_LTE:             return OPERATOR_LTE;
        case TOKEN_GT:              return OPERATOR_GT;
        case TOKEN_GTE:             return OPERATOR_GTE;
        case TOKEN_EQ:              return OPERATOR_EQ;
        case TOKEN_NOT_EQ:          return OPERATOR_NOT_EQ;
        case TOKEN_PERCENT:         return OPERATOR_MODULUS;
        case TOKEN_AND:             return OPERATOR_LOGICAL_AND;
        case TOKEN_OR:              return OPERATOR_LOGICAL_OR;
        case TOKEN_PLUS_ASSIGN:     return OPERATOR_PLUS;
        case TOKEN_MINUS_ASSIGN:    return OPERATOR_MINUS;
        case TOKEN_ASTERISK_ASSIGN: return OPERATOR_ASTERISK;
        case TOKEN_SLASH_ASSIGN:    return OPERATOR_SLASH;
        case TOKEN_PERCENT_ASSIGN:  return OPERATOR_MODULUS;
        case TOKEN_BIT_AND_ASSIGN:  return OPERATOR_BIT_AND;
        case TOKEN_BIT_OR_ASSIGN:   return OPERATOR_BIT_OR;
        case TOKEN_BIT_XOR_ASSIGN:  return OPERATOR_BIT_XOR;
        case TOKEN_LSHIFT_ASSIGN:   return OPERATOR_LSHIFT;
        case TOKEN_RSHIFT_ASSIGN:   return OPERATOR_RSHIFT;
        case TOKEN_BIT_AND:         return OPERATOR_BIT_AND;
        case TOKEN_BIT_OR:          return OPERATOR_BIT_OR;
        case TOKEN_BIT_XOR:         return OPERATOR_BIT_XOR;
        case TOKEN_LSHIFT:          return OPERATOR_LSHIFT;
        case TOKEN_RSHIFT:          return OPERATOR_RSHIFT;
        default: {
            APE_ASSERT(false);
            return OPERATOR_NONE;
        }
    }
}

static bool cur_token_is(parser_t *p, token_type_t type) {
    return p->cur_token.type == type;
}

static bool peek_token_is(parser_t *p, token_type_t type) {
    return p->peek_token.type == type;
}

static bool expect_current(parser_t *p, token_type_t type) {
    if (!cur_token_is(p, type)) {
        const char *expected_type_str = token_type_to_string(type);
        const char *actual_type_str = token_type_to_string(p->cur_token.type);
        errortag_t *err = error_makef(ERROR_PARSING, p->cur_token.pos,
                                   "Expected current token to be \"%s\", got \"%s\" instead",
                                   expected_type_str, actual_type_str);
        ptrarray_add(p->errors, err);
        return false;
    }
    return true;
}

static char escape_char(const char c) {
    switch (c) {
        case '\"': return '\"';
        case '\\': return '\\';
        case '/':  return '/';
        case 'b':  return '\b';
        case 'f':  return '\f';
        case 'n':  return '\n';
        case 'r':  return '\r';
        case 't':  return '\t';
        case '0':  return '\0';
        default: {
            APE_ASSERT(false);
            return -1;
        }
    }
}

static char* process_and_copy_string(const char *input, size_t len) {
    char *output = ape_malloc(len + 1);

    size_t in_i = 0;
    size_t out_i = 0;

    while (input[in_i] != '\0' && in_i < len) {
        if (input[in_i] == '\\') {
            in_i++;
            output[out_i] = escape_char(input[in_i]);
            if (output[out_i] < 0) {
                goto error;
            }
        } else {
            output[out_i] = input[in_i];
        }
        out_i++;
        in_i++;
    }
    output[out_i] = '\0';
    return output;
error:
    ape_free(output);
    return NULL;
}
