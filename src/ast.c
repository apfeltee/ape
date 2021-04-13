#include <stdlib.h>
#include <string.h>

#ifndef APE_AMALGAMATED
#include "ast.h"
#include "common.h"
#endif

static expression_t* expression_make(expression_type_t type);
static statement_t* statement_make(statement_type_t type);

expression_t* expression_make_ident(ident_t ident) {
    expression_t *res = expression_make(EXPRESSION_IDENT);
    res->ident = ident;
    return res;
}

expression_t* expression_make_number_literal(double val) {
    expression_t *res = expression_make(EXPRESSION_NUMBER_LITERAL);
    res->number_literal = val;
    return res;
}

expression_t* expression_make_bool_literal(bool val) {
    expression_t *res = expression_make(EXPRESSION_BOOL_LITERAL);
    res->bool_literal = val;
    return res;
}

expression_t* expression_make_string_literal(char *value) {
    expression_t *res = expression_make(EXPRESSION_STRING_LITERAL);
    res->string_literal = value;
    return res;
}

expression_t* expression_make_null_literal() {
    expression_t *res = expression_make(EXPRESSION_NULL_LITERAL);
    return res;
}

expression_t* expression_make_array_literal(ptrarray(expression_t) *values) {
    expression_t *res = expression_make(EXPRESSION_ARRAY_LITERAL);
    res->array = values;
    return res;
}

expression_t* expression_make_map_literal(ptrarray(expression_t) *keys, ptrarray(expression_t) *values) {
    expression_t *res = expression_make(EXPRESSION_MAP_LITERAL);
    res->map.keys = keys;
    res->map.values = values;
    return res;
}

expression_t* expression_make_prefix(operator_t op, expression_t *right) {
    expression_t *res = expression_make(EXPRESSION_PREFIX);
    res->prefix.op = op;
    res->prefix.right = right;
    return res;
}

expression_t* expression_make_infix(operator_t op, expression_t *left, expression_t *right) {
    expression_t *res = expression_make(EXPRESSION_INFIX);
    res->infix.op = op;
    res->infix.left = left;
    res->infix.right = right;
    return res;
}

expression_t* expression_make_fn_literal(array(ident_t) *params, code_block_t *body) {
    expression_t *res = expression_make(EXPRESSION_FUNCTION_LITERAL);
    res->fn_literal.name = NULL;
    res->fn_literal.params = params;
    res->fn_literal.body = body;
    return res;
}

expression_t* expression_make_call(expression_t *function, ptrarray(expression_t) *args) {
    expression_t *res = expression_make(EXPRESSION_CALL);
    res->call_expr.function = function;
    res->call_expr.args = args;
    return res;
}

expression_t* expression_make_index(expression_t *left, expression_t *index) {
    expression_t *res = expression_make(EXPRESSION_INDEX);
    res->index_expr.left = left;
    res->index_expr.index = index;
    return res;
}

expression_t* expression_make_assign(expression_t *dest, expression_t *source) {
    expression_t *res = expression_make(EXPRESSION_ASSIGN);
    res->assign.dest = dest;
    res->assign.source = source;
    return res;
}

expression_t* expression_make_logical(operator_t op, expression_t *left, expression_t *right) {
    expression_t *res = expression_make(EXPRESSION_LOGICAL);
    res->logical.op = op;
    res->logical.left = left;
    res->logical.right = right;
    return res;
}

void expression_destroy(expression_t *expr) {
    if (!expr) {
        return;
    }

    switch (expr->type) {
        case EXPRESSION_NONE: {
            APE_ASSERT(false);
            break;
        }
        case EXPRESSION_IDENT: {
            ident_deinit(&expr->ident);
            break;
        }
        case EXPRESSION_NUMBER_LITERAL:
        case EXPRESSION_BOOL_LITERAL: {
            break;
        }
        case EXPRESSION_STRING_LITERAL: {
            ape_free(expr->string_literal);
            break;
        }
        case EXPRESSION_NULL_LITERAL: {
            break;
        }
        case EXPRESSION_ARRAY_LITERAL: {
            ptrarray_destroy_with_items(expr->array, expression_destroy);
            break;
        }
        case EXPRESSION_MAP_LITERAL: {
            ptrarray_destroy_with_items(expr->map.keys, expression_destroy);
            ptrarray_destroy_with_items(expr->map.values, expression_destroy);
            break;
        }
        case EXPRESSION_PREFIX: {
            expression_destroy(expr->prefix.right);
            break;
        }
        case EXPRESSION_INFIX: {
            expression_destroy(expr->infix.left);
            expression_destroy(expr->infix.right);
            break;
        }
        case EXPRESSION_FUNCTION_LITERAL: {
            fn_literal_deinit(&expr->fn_literal);
            break;
        }
        case EXPRESSION_CALL: {
            ptrarray_destroy_with_items(expr->call_expr.args, expression_destroy);
            expression_destroy(expr->call_expr.function);
            break;
        }
        case EXPRESSION_INDEX: {
            expression_destroy(expr->index_expr.left);
            expression_destroy(expr->index_expr.index);
            break;
        }
        case EXPRESSION_ASSIGN: {
            expression_destroy(expr->assign.dest);
            expression_destroy(expr->assign.source);
            break;
        }
        case EXPRESSION_LOGICAL: {
            expression_destroy(expr->logical.left);
            expression_destroy(expr->logical.right);
            break;
        }
    }
    ape_free(expr);
}

expression_t* expression_copy(expression_t *expr) {
    APE_ASSERT(expr);
    if (!expr) {
        return NULL;
    }
    expression_t *res = NULL;
    switch (expr->type) {
        case EXPRESSION_NONE: {
            APE_ASSERT(false);
            break;
        }
        case EXPRESSION_IDENT: {
            ident_t ident = ident_copy(expr->ident);
            res = expression_make_ident(ident);
            break;
        }
        case EXPRESSION_NUMBER_LITERAL: {
            res = expression_make_number_literal(expr->number_literal);
            break;
        }
        case EXPRESSION_BOOL_LITERAL: {
            res = expression_make_bool_literal(expr->bool_literal);
            break;
        }
        case EXPRESSION_STRING_LITERAL: {
            res = expression_make_string_literal(ape_strdup(expr->string_literal));
            break;
        }
        case EXPRESSION_NULL_LITERAL: {
            res = expression_make_null_literal();
            break;
        }
        case EXPRESSION_ARRAY_LITERAL: {
            ptrarray(expression_t) *values_copy = ptrarray_copy_with_items(expr->array, expression_copy);
            res = expression_make_array_literal(values_copy);
            break;
        }
        case EXPRESSION_MAP_LITERAL: {
            ptrarray(expression_t) *keys_copy = ptrarray_copy_with_items(expr->map.keys, expression_copy);
            ptrarray(expression_t) *values_copy = ptrarray_copy_with_items(expr->map.values, expression_copy);
            res = expression_make_map_literal(keys_copy, values_copy);
            break;
        }
        case EXPRESSION_PREFIX: {
            expression_t *right_copy = expression_copy(expr->prefix.right);
            res = expression_make_prefix(expr->prefix.op, right_copy);
            break;
        }
        case EXPRESSION_INFIX: {
            expression_t *left_copy = expression_copy(expr->infix.left);
            expression_t *right_copy = expression_copy(expr->infix.right);
            res = expression_make_infix(expr->infix.op, left_copy, right_copy);
            break;
        }
        case EXPRESSION_FUNCTION_LITERAL: {
            array(ident_t) *params_copy = array_make(ident_t);
            for (int i = 0; i < array_count(expr->fn_literal.params); i++) {
                ident_t *param = array_get(expr->fn_literal.params, i);
                ident_t copy = ident_copy(*param);
                array_add(params_copy, &copy);
            }
            code_block_t *body_copy = code_block_copy(expr->fn_literal.body);
            res = expression_make_fn_literal(params_copy, body_copy);
            res->fn_literal.name = ape_strdup(expr->fn_literal.name);
            break;
        }
        case EXPRESSION_CALL: {
            expression_t *function_copy = expression_copy(expr->call_expr.function);
            ptrarray(expression_t) *args_copy = ptrarray_copy_with_items(expr->call_expr.args, expression_copy);
            res = expression_make_call(function_copy, args_copy);
            break;
        }
        case EXPRESSION_INDEX: {
            expression_t *left_copy = expression_copy(expr->index_expr.left);
            expression_t *index_copy = expression_copy(expr->index_expr.index);
            res = expression_make_index(left_copy, index_copy);
            break;
        }
        case EXPRESSION_ASSIGN: {
            expression_t *dest_copy = expression_copy(expr->assign.dest);
            expression_t *source_copy = expression_copy(expr->assign.source);
            res = expression_make_assign(dest_copy, source_copy);
            break;
        }
        case EXPRESSION_LOGICAL: {
            expression_t *left_copy = expression_copy(expr->logical.left);
            expression_t *right_copy = expression_copy(expr->logical.right);
            res = expression_make_logical(expr->logical.op, left_copy, right_copy);
            break;
        }
    }
    res->pos = expr->pos;
    return res;
}

statement_t* statement_make_define(ident_t name, expression_t *value, bool assignable) {
    statement_t *res = statement_make(STATEMENT_DEFINE);
    res->define.name = name;
    res->define.value = value;
    res->define.assignable = assignable;
    return res;
}

statement_t* statement_make_if(ptrarray(if_case_t) *cases, code_block_t *alternative) {
    statement_t *res = statement_make(STATEMENT_IF);
    res->if_statement.cases = cases;
    res->if_statement.alternative = alternative;
    return res;
}

statement_t* statement_make_return(expression_t *value) {
    statement_t *res = statement_make(STATEMENT_RETURN_VALUE);
    res->return_value = value;
    return res;
}

statement_t* statement_make_expression(expression_t *value) {
    statement_t *res = statement_make(STATEMENT_EXPRESSION);
    res->expression = value;
    return res;
}

statement_t* statement_make_while_loop(expression_t *test, code_block_t *body) {
    statement_t *res = statement_make(STATEMENT_WHILE_LOOP);
    res->while_loop.test = test;
    res->while_loop.body = body;
    return res;
}

statement_t* statement_make_break() {
    statement_t *res = statement_make(STATEMENT_BREAK);
    return res;
}

statement_t* statement_make_foreach(ident_t iterator, expression_t *source, code_block_t *body) {
    statement_t *res = statement_make(STATEMENT_FOREACH);
    res->foreach.iterator = iterator;
    res->foreach.source = source;
    res->foreach.body = body;
    return res;
}

statement_t* statement_make_for_loop(statement_t *init, expression_t *test, expression_t *update, code_block_t *body) {
    statement_t *res = statement_make(STATEMENT_FOR_LOOP);
    res->for_loop.init = init;
    res->for_loop.test = test;
    res->for_loop.update = update;
    res->for_loop.body = body;
    return res;
}

statement_t* statement_make_continue() {
    statement_t *res = statement_make(STATEMENT_CONTINUE);
    return res;
}

statement_t* statement_make_block(code_block_t *block) {
    statement_t *res = statement_make(STATEMENT_BLOCK);
    res->block = block;
    return res;
}

statement_t* statement_make_import(char *path) {
    statement_t *res = statement_make(STATEMENT_IMPORT);
    res->import.path = path;
    return res;
}

statement_t* statement_make_recover(ident_t error_ident, code_block_t *body) {
    statement_t *res = statement_make(STATEMENT_RECOVER);
    res->recover.error_ident = error_ident;
    res->recover.body = body;
    return res;
}

void statement_destroy(statement_t *stmt) {
    if (!stmt) {
        return;
    }
    switch (stmt->type) {
        case STATEMENT_NONE: {
            APE_ASSERT(false);
            break;
        }
        case STATEMENT_DEFINE: {
            ident_deinit(&stmt->define.name);
            expression_destroy(stmt->define.value);
            break;
        }
        case STATEMENT_IF: {
            ptrarray_destroy_with_items(stmt->if_statement.cases, if_case_destroy);
            code_block_destroy(stmt->if_statement.alternative);
            break;
        }
        case STATEMENT_RETURN_VALUE: {
            expression_destroy(stmt->return_value);
            break;
        }
        case STATEMENT_EXPRESSION: {
            expression_destroy(stmt->expression);
            break;
        }
        case STATEMENT_WHILE_LOOP: {
            expression_destroy(stmt->while_loop.test);
            code_block_destroy(stmt->while_loop.body);
            break;
        }
        case STATEMENT_BREAK: {
            break;
        }
        case STATEMENT_CONTINUE: {
            break;
        }
        case STATEMENT_FOREACH: {
            ident_deinit(&stmt->foreach.iterator);
            expression_destroy(stmt->foreach.source);
            code_block_destroy(stmt->foreach.body);
            break;
        }
        case STATEMENT_FOR_LOOP: {
            statement_destroy(stmt->for_loop.init);
            expression_destroy(stmt->for_loop.test);
            expression_destroy(stmt->for_loop.update);
            code_block_destroy(stmt->for_loop.body);
            break;
        }
        case STATEMENT_BLOCK: {
            code_block_destroy(stmt->block);
            break;
        }
        case STATEMENT_IMPORT: {
            ape_free(stmt->import.path);
            break;
        }
        case STATEMENT_RECOVER: {
            code_block_destroy(stmt->recover.body);
            ident_deinit(&stmt->recover.error_ident);
            break;
        }
    }
    ape_free(stmt);
}

statement_t* statement_copy(statement_t *stmt) {
    APE_ASSERT(stmt);
    if (!stmt) {
        return NULL;
    }
    statement_t *res = NULL;
    switch (stmt->type) {
        case STATEMENT_NONE: {
            APE_ASSERT(false);
            break;
        }
        case STATEMENT_DEFINE: {
            expression_t *value_copy = expression_copy(stmt->define.value);
            res = statement_make_define(ident_copy(stmt->define.name), value_copy, stmt->define.assignable);
            break;
        }
        case STATEMENT_IF: {
            ptrarray(expression_t) *cases_copy = ptrarray_copy_with_items(stmt->if_statement.cases, expression_copy);
            code_block_t *alternative_copy = code_block_copy(stmt->if_statement.alternative);
            res = statement_make_if(cases_copy, alternative_copy);
            break;
        }
        case STATEMENT_RETURN_VALUE: {
            expression_t *value_copy = expression_copy(stmt->return_value);
            res = statement_make_return(value_copy);
            break;
        }
        case STATEMENT_EXPRESSION: {
            expression_t *value_copy = expression_copy(stmt->expression);
            res = statement_make_expression(value_copy);
            break;
        }
        case STATEMENT_WHILE_LOOP: {
            expression_t *test_copy = expression_copy(stmt->while_loop.test);
            code_block_t *body_copy = code_block_copy(stmt->while_loop.body);
            res = statement_make_while_loop(test_copy, body_copy);
            break;
        }
        case STATEMENT_BREAK: {
            res = statement_make_break();
            break;
        }
        case STATEMENT_CONTINUE: {
            res = statement_make_continue();
            break;
        }
        case STATEMENT_FOREACH: {
            expression_t *source_copy = expression_copy(stmt->foreach.source);
            code_block_t *body_copy = code_block_copy(stmt->foreach.body);
            res = statement_make_foreach(ident_copy(stmt->foreach.iterator), source_copy, body_copy);
            break;
        }
        case STATEMENT_FOR_LOOP: {
            statement_t *init_copy = statement_copy(stmt->for_loop.init);
            expression_t *test_copy = expression_copy(stmt->for_loop.test);
            expression_t *update_copy = expression_copy(stmt->for_loop.update);
            code_block_t *body_copy = code_block_copy(stmt->for_loop.body);
            res = statement_make_for_loop(init_copy, test_copy, update_copy, body_copy);
            break;
        }
        case STATEMENT_BLOCK: {
            code_block_t *block_copy = code_block_copy(stmt->block);
            res = statement_make_block(block_copy);
            break;
        }
        case STATEMENT_IMPORT: {
            res = statement_make_import(ape_strdup(stmt->import.path));
            break;
        }
        case STATEMENT_RECOVER: {
            code_block_t *body_copy = code_block_copy(stmt->recover.body);
            res = statement_make_recover(ident_copy(stmt->recover.error_ident), body_copy);
            break;
        }
    }
    res->pos = stmt->pos;
    return NULL;
}

code_block_t* code_block_make(ptrarray(statement_t) *statements) {
    code_block_t *stmt = ape_malloc(sizeof(code_block_t));
    stmt->statements = statements;
    return stmt;
}

void code_block_destroy(code_block_t *block) {
    if (!block) {
        return;
    }
    ptrarray_destroy_with_items(block->statements, statement_destroy);
    ape_free(block);
}

code_block_t* code_block_copy(code_block_t *block) {
    ptrarray(statement_t) *statements_copy = ptrarray_make();
    for (int i = 0; i < ptrarray_count(block->statements); i++) {
        statement_t *statement = ptrarray_get(block->statements, i);
        statement_t *copy = statement_copy(statement);
        ptrarray_add(statements_copy, copy);
    }
    return code_block_make(statements_copy);
}

char* statements_to_string(ptrarray(statement_t) *statements) {
    strbuf_t *buf = strbuf_make();
    int count =  ptrarray_count(statements);
    for (int i = 0; i < count; i++) {
        const statement_t *stmt = ptrarray_get(statements, i);
        statement_to_string(stmt, buf);
        if (i < (count - 1)) {
            strbuf_append(buf, "\n");
        }
    }
    return strbuf_get_string_and_destroy(buf);
}

void statement_to_string(const statement_t *stmt, strbuf_t *buf) {
    switch (stmt->type) {
        case STATEMENT_DEFINE: {
            const define_statement_t *def_stmt = &stmt->define;
            if (stmt->define.assignable) {
                strbuf_append(buf, "var ");
            } else {
                strbuf_append(buf, "const ");
            }
            strbuf_append(buf, def_stmt->name.value);
            strbuf_append(buf, " = ");

            if (def_stmt->value) {
                expression_to_string(def_stmt->value, buf);
            }

            break;
        }
        case STATEMENT_IF: {
            if_case_t *if_case = ptrarray_get(stmt->if_statement.cases, 0);
            strbuf_append(buf, "if (");
            expression_to_string(if_case->test, buf);
            strbuf_append(buf, ") ");
            code_block_to_string(if_case->consequence, buf);
            for (int i = 1; i < ptrarray_count(stmt->if_statement.cases); i++) {
                if_case_t *elif_case = ptrarray_get(stmt->if_statement.cases, i);
                strbuf_append(buf, " elif (");
                expression_to_string(elif_case->test, buf);
                strbuf_append(buf, ") ");
                code_block_to_string(elif_case->consequence, buf);
            }
            if (stmt->if_statement.alternative) {
                strbuf_append(buf, " else ");
                code_block_to_string(stmt->if_statement.alternative, buf);
            }
            break;
        }
        case STATEMENT_RETURN_VALUE: {
            strbuf_append(buf, "return ");
            if (stmt->return_value) {
                expression_to_string(stmt->return_value, buf);
            }
            break;
        }
        case STATEMENT_EXPRESSION: {
            if (stmt->expression) {
                expression_to_string(stmt->expression, buf);
            }
            break;
        }
        case STATEMENT_WHILE_LOOP: {
            strbuf_append(buf, "while (");
            expression_to_string(stmt->while_loop.test, buf);
            strbuf_append(buf, ")");
            code_block_to_string(stmt->while_loop.body, buf);
            break;
        }
        case STATEMENT_FOR_LOOP: {
            strbuf_append(buf, "for (");
            if (stmt->for_loop.init) {
                statement_to_string(stmt->for_loop.init, buf);
                strbuf_append(buf, " ");
            } else {
                strbuf_append(buf, ";");
            }
            if (stmt->for_loop.test) {
                expression_to_string(stmt->for_loop.test, buf);
                strbuf_append(buf, "; ");
            } else {
                strbuf_append(buf, ";");
            }
            if (stmt->for_loop.update) {
                expression_to_string(stmt->for_loop.test, buf);
            }
            strbuf_append(buf, ")");
            code_block_to_string(stmt->for_loop.body, buf);
            break;
        }
        case STATEMENT_FOREACH: {
            strbuf_append(buf, "for (");
            strbuf_appendf(buf, "%s", stmt->foreach.iterator.value);
            strbuf_append(buf, " in ");
            expression_to_string(stmt->foreach.source, buf);
            strbuf_append(buf, ")");
            code_block_to_string(stmt->foreach.body, buf);
            break;
        }
        case STATEMENT_BLOCK: {
            code_block_to_string(stmt->block, buf);
            break;
        }
        case STATEMENT_BREAK: {
            strbuf_append(buf, "break");
            break;
        }
        case STATEMENT_CONTINUE: {
            strbuf_append(buf, "continue");
            break;
        }
        case STATEMENT_IMPORT: {
            strbuf_appendf(buf, "import \"%s\"", stmt->import.path);
            break;
        }
        case STATEMENT_NONE: {
            strbuf_append(buf, "STATEMENT_NONE");
            break;
        }
        case STATEMENT_RECOVER: {
            strbuf_appendf(buf, "recover (%s)", stmt->recover.error_ident.value);
            code_block_to_string(stmt->recover.body, buf);
            break;
        }
    }
}

void expression_to_string(expression_t *expr, strbuf_t *buf) {
    switch (expr->type) {
        case EXPRESSION_IDENT: {
            strbuf_append(buf, expr->ident.value);
            break;
        }
        case EXPRESSION_NUMBER_LITERAL: {
            strbuf_appendf(buf, "%1.17g", expr->number_literal);
            break;
        }
        case EXPRESSION_BOOL_LITERAL: {
            strbuf_appendf(buf, "%s", expr->bool_literal ? "true" : "false");
            break;
        }
        case EXPRESSION_STRING_LITERAL: {
            strbuf_appendf(buf, "\"%s\"", expr->string_literal);
            break;
        }
        case EXPRESSION_NULL_LITERAL: {
            strbuf_append(buf, "null");
            break;
        }
        case EXPRESSION_ARRAY_LITERAL: {
            strbuf_append(buf, "[");
            for (int i = 0; i < ptrarray_count(expr->array); i++) {
                expression_t *arr_expr = ptrarray_get(expr->array, i);
                expression_to_string(arr_expr, buf);
                if (i < (ptrarray_count(expr->array) - 1)) {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, "]");
            break;
        }
        case EXPRESSION_MAP_LITERAL: {
            map_literal_t *map = &expr->map;

            strbuf_append(buf, "{");
            for (int i = 0; i < ptrarray_count(map->keys); i++) {
                expression_t *key_expr = ptrarray_get(map->keys, i);
                expression_t *val_expr = ptrarray_get(map->values, i);

                expression_to_string(key_expr, buf);
                strbuf_append(buf, " : ");
                expression_to_string(val_expr, buf);

                if (i < (ptrarray_count(map->keys) - 1)) {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, "}");
            break;
        }
        case EXPRESSION_PREFIX: {
            strbuf_append(buf, "(");
            strbuf_append(buf, operator_to_string(expr->infix.op));
            expression_to_string(expr->prefix.right, buf);
            strbuf_append(buf, ")");
            break;
        }
        case EXPRESSION_INFIX: {
            strbuf_append(buf, "(");
            expression_to_string(expr->infix.left, buf);
            strbuf_append(buf, " ");
            strbuf_append(buf, operator_to_string(expr->infix.op));
            strbuf_append(buf, " ");
            expression_to_string(expr->infix.right, buf);
            strbuf_append(buf, ")");
            break;
        }
        case EXPRESSION_FUNCTION_LITERAL: {
            fn_literal_t *fn = &expr->fn_literal;
            
            strbuf_append(buf, "fn");

            strbuf_append(buf, "(");
            for (int i = 0; i < array_count(fn->params); i++) {
                ident_t *param = array_get(fn->params, i);
                strbuf_append(buf, param->value);
                if (i < (array_count(fn->params) - 1)) {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, ") ");

            code_block_to_string(fn->body, buf);

            break;
        }
        case EXPRESSION_CALL: {
            call_expression_t *call_expr = &expr->call_expr;

            expression_to_string(call_expr->function, buf);

            strbuf_append(buf, "(");
            for (int i = 0; i < ptrarray_count(call_expr->args); i++) {
                expression_t *arg = ptrarray_get(call_expr->args, i);
                expression_to_string(arg, buf);
                if (i < (ptrarray_count(call_expr->args) - 1)) {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, ")");

            break;
        }
        case EXPRESSION_INDEX: {
            strbuf_append(buf, "(");
            expression_to_string(expr->index_expr.left, buf);
            strbuf_append(buf, "[");
            expression_to_string(expr->index_expr.index, buf);
            strbuf_append(buf, "])");
            break;
        }
        case EXPRESSION_ASSIGN: {
            expression_to_string(expr->assign.dest, buf);
            strbuf_append(buf, " = ");
            expression_to_string(expr->assign.source, buf);
            break;
        }
        case EXPRESSION_LOGICAL: {
            expression_to_string(expr->logical.left, buf);
            strbuf_append(buf, " ");
            strbuf_append(buf, operator_to_string(expr->infix.op));
            strbuf_append(buf, " ");
            expression_to_string(expr->logical.right, buf);
            break;
        }
        case EXPRESSION_NONE: {
            strbuf_append(buf, "EXPRESSION_NONE");
            break;
        }
    }
}

void code_block_to_string(const code_block_t *stmt, strbuf_t *buf) {
    strbuf_append(buf, "{ ");
    for (int i = 0; i < ptrarray_count(stmt->statements); i++) {
        statement_t *istmt = ptrarray_get(stmt->statements, i);
        statement_to_string(istmt, buf);
        strbuf_append(buf, "\n");
    }
    strbuf_append(buf, " }");
}

const char* operator_to_string(operator_t op) {
    switch (op) {
        case OPERATOR_NONE:        return "OPERATOR_NONE";
        case OPERATOR_ASSIGN:      return "=";
        case OPERATOR_PLUS:        return "+";
        case OPERATOR_MINUS:       return "-";
        case OPERATOR_BANG:        return "!";
        case OPERATOR_ASTERISK:    return "*";
        case OPERATOR_SLASH:       return "/";
        case OPERATOR_LT:          return "<";
        case OPERATOR_GT:          return ">";
        case OPERATOR_EQ:          return "==";
        case OPERATOR_NOT_EQ:      return "!=";
        case OPERATOR_MODULUS:     return "%";
        case OPERATOR_LOGICAL_AND: return "&&";
        case OPERATOR_LOGICAL_OR:  return "||";
        case OPERATOR_BIT_AND:     return "&";
        case OPERATOR_BIT_OR:      return "|";
        case OPERATOR_BIT_XOR:     return "^";
        case OPERATOR_LSHIFT:      return "<<";
        case OPERATOR_RSHIFT:      return ">>";
        default:                   return "OPERATOR_UNKNOWN";
    }
}

const char *expression_type_to_string(expression_type_t type) {
    switch (type) {
        case EXPRESSION_NONE:             return "NONE";
        case EXPRESSION_IDENT:            return "IDENT";
        case EXPRESSION_NUMBER_LITERAL:   return "INT_LITERAL";
        case EXPRESSION_BOOL_LITERAL:     return "BOOL_LITERAL";
        case EXPRESSION_STRING_LITERAL:   return "STRING_LITERAL";
        case EXPRESSION_ARRAY_LITERAL:    return "ARRAY_LITERAL";
        case EXPRESSION_MAP_LITERAL:      return "MAP_LITERAL";
        case EXPRESSION_PREFIX:           return "PREFIX";
        case EXPRESSION_INFIX:            return "INFIX";
        case EXPRESSION_FUNCTION_LITERAL: return "FN_LITERAL";
        case EXPRESSION_CALL:             return "CALL";
        case EXPRESSION_INDEX:            return "INDEX";
        case EXPRESSION_ASSIGN:           return "ASSIGN";
        case EXPRESSION_LOGICAL:          return "LOGICAL";
        default:                          return "UNKNOWN";
    }
}

void fn_literal_deinit(fn_literal_t *fn) {
    ape_free(fn->name);
    array_destroy_with_items(fn->params, ident_deinit);
    code_block_destroy(fn->body);
}

ident_t ident_make(token_t tok) {
    ident_t res;
    res.value = token_duplicate_literal(&tok);
    res.pos = tok.pos;
    return res;
}

ident_t ident_copy(ident_t ident) {
    ident_t res;
    res.value = ape_strdup(ident.value);
    res.pos = ident.pos;
    return res;
}

void ident_deinit(ident_t *ident) {
    ape_free(ident->value);
    ident->value = NULL;
    ident->pos = src_pos_invalid;
}

if_case_t *if_case_make(expression_t *test, code_block_t *consequence) {
    if_case_t *res = ape_malloc(sizeof(if_case_t));
    res->test = test;
    res->consequence = consequence;
    return res;
}

void if_case_destroy(if_case_t *cond) {
    if (!cond) {
        return;
    }

    expression_destroy(cond->test);
    code_block_destroy(cond->consequence);
    ape_free(cond);
}

// INTERNAL
static expression_t *expression_make(expression_type_t type) {
    expression_t *res = ape_malloc(sizeof(expression_t));
    res->type = type;
    res->pos = src_pos_invalid;
    return res;
}

static statement_t* statement_make(statement_type_t type) {
    statement_t *res = ape_malloc(sizeof(statement_t));
    res->type = type;
    res->pos = src_pos_invalid;
    return res;
}
