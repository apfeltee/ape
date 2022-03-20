
#include "priv.h"

ApeParser_t* parser_make(ApeAllocator_t* alloc, const ApeConfig_t* config, ApeErrorList_t* errors)
{
    ApeParser_t* parser;
    parser = (ApeParser_t*)allocator_malloc(alloc, sizeof(ApeParser_t));
    if(!parser)
    {
        return NULL;
    }
    memset(parser, 0, sizeof(ApeParser_t));

    parser->alloc = alloc;
    parser->config = config;
    parser->errors = errors;

    parser->right_assoc_parse_fns[TOKEN_IDENT] = parse_identifier;
    parser->right_assoc_parse_fns[TOKEN_NUMBER] = parse_number_literal;
    parser->right_assoc_parse_fns[TOKEN_TRUE] = parse_bool_literal;
    parser->right_assoc_parse_fns[TOKEN_FALSE] = parse_bool_literal;
    parser->right_assoc_parse_fns[TOKEN_STRING] = parse_string_literal;
    parser->right_assoc_parse_fns[TOKEN_TEMPLATE_STRING] = parse_template_string_literal;
    parser->right_assoc_parse_fns[TOKEN_NULL] = parse_null_literal;
    parser->right_assoc_parse_fns[TOKEN_BANG] = parse_prefix_expression;
    parser->right_assoc_parse_fns[TOKEN_MINUS] = parse_prefix_expression;
    parser->right_assoc_parse_fns[TOKEN_LPAREN] = parse_grouped_expression;
    parser->right_assoc_parse_fns[TOKEN_FUNCTION] = parse_function_literal;
    parser->right_assoc_parse_fns[TOKEN_LBRACKET] = parse_array_literal;
    parser->right_assoc_parse_fns[TOKEN_LBRACE] = parse_map_literal;
    parser->right_assoc_parse_fns[TOKEN_PLUS_PLUS] = parse_incdec_prefix_expression;
    parser->right_assoc_parse_fns[TOKEN_MINUS_MINUS] = parse_incdec_prefix_expression;

    parser->left_assoc_parse_fns[TOKEN_PLUS] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_MINUS] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_SLASH] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_ASTERISK] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_PERCENT] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_EQ] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_NOT_EQ] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_LT] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_LTE] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_GT] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_GTE] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_LPAREN] = parse_call_expression;
    parser->left_assoc_parse_fns[TOKEN_LBRACKET] = parse_index_expression;
    parser->left_assoc_parse_fns[TOKEN_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_PLUS_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_MINUS_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_SLASH_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_ASTERISK_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_PERCENT_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_BIT_AND_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_BIT_OR_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_BIT_XOR_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_LSHIFT_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_RSHIFT_ASSIGN] = parse_assign_expression;
    parser->left_assoc_parse_fns[TOKEN_DOT] = parse_dot_expression;
    parser->left_assoc_parse_fns[TOKEN_AND] = parse_logical_expression;
    parser->left_assoc_parse_fns[TOKEN_OR] = parse_logical_expression;
    parser->left_assoc_parse_fns[TOKEN_BIT_AND] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_BIT_OR] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_BIT_XOR] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_LSHIFT] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_RSHIFT] = parse_infix_expression;
    parser->left_assoc_parse_fns[TOKEN_QUESTION] = parse_ternary_expression;
    parser->left_assoc_parse_fns[TOKEN_PLUS_PLUS] = parse_incdec_postfix_expression;
    parser->left_assoc_parse_fns[TOKEN_MINUS_MINUS] = parse_incdec_postfix_expression;

    parser->depth = 0;

    return parser;
}

void parser_destroy(ApeParser_t* parser)
{
    if(!parser)
    {
        return;
    }
    allocator_free(parser->alloc, parser);
}

ApePtrArray_t * parser_parse_all(ApeParser_t* parser, const char* input, ApeCompiledFile_t* file)
{
    bool ok;
    ApePtrArray_t* statements;
    ApeStatement_t* stmt;
    parser->depth = 0;
    ok = lexer_init(&parser->lexer, parser->alloc, parser->errors, input, file);
    if(!ok)
    {
        return NULL;
    }
    lexer_next_token(&parser->lexer);
    lexer_next_token(&parser->lexer);
    statements = ptrarray_make(parser->alloc);
    if(!statements)
    {
        return NULL;
    }
    while(!lexer_cur_token_is(&parser->lexer, TOKEN_EOF))
    {
        if(lexer_cur_token_is(&parser->lexer, TOKEN_SEMICOLON))
        {
            lexer_next_token(&parser->lexer);
            continue;
        }
        stmt = parse_statement(parser);
        if(!stmt)
        {
            goto err;
        }
        ok = ptrarray_add(statements, stmt);
        if(!ok)
        {
            statement_destroy(stmt);
            goto err;
        }
    }
    if(errors_get_count(parser->errors) > 0)
    {
        goto err;
    }

    return statements;
err:
    ptrarray_destroy_with_items(statements, statement_destroy);
    return NULL;
}

// INTERNAL
ApeStatement_t* parse_statement(ApeParser_t* p)
{
    ApePosition_t pos = p->lexer.cur_token.pos;

    ApeStatement_t* res = NULL;
    switch(p->lexer.cur_token.type)
    {
        case TOKEN_VAR:
        case TOKEN_CONST:
        {
            res = parse_define_statement(p);
            break;
        }
        case TOKEN_IF:
        {
            res = parse_if_statement(p);
            break;
        }
        case TOKEN_RETURN:
        {
            res = parse_return_statement(p);
            break;
        }
        case TOKEN_WHILE:
        {
            res = parse_while_loop_statement(p);
            break;
        }
        case TOKEN_BREAK:
        {
            res = parse_break_statement(p);
            break;
        }
        case TOKEN_FOR:
        {
            res = parse_for_loop_statement(p);
            break;
        }
        case TOKEN_FUNCTION:
        {
            if(lexer_peek_token_is(&p->lexer, TOKEN_IDENT))
            {
                res = parse_function_statement(p);
            }
            else
            {
                res = parse_expression_statement(p);
            }
            break;
        }
        case TOKEN_LBRACE:
        {
            if(p->config->repl_mode && p->depth == 0)
            {
                res = parse_expression_statement(p);
            }
            else
            {
                res = parse_block_statement(p);
            }
            break;
        }
        case TOKEN_CONTINUE:
        {
            res = parse_continue_statement(p);
            break;
        }
        case TOKEN_IMPORT:
        {
            res = parse_import_statement(p);
            break;
        }
        case TOKEN_RECOVER:
        {
            res = parse_recover_statement(p);
            break;
        }
        default:
        {
            res = parse_expression_statement(p);
            break;
        }
    }
    if(res)
    {
        res->pos = pos;
    }
    return res;
}

ApeStatement_t* parse_define_statement(ApeParser_t* p)
{
    bool assignable;
    ApeIdent_t* name_ident;
    ApeExpression_t* value;
    ApeStatement_t* res;
    name_ident = NULL;
    value = NULL;
    assignable = lexer_cur_token_is(&p->lexer, TOKEN_VAR);
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_IDENT))
    {
        goto err;
    }
    name_ident = ident_make(p->alloc, p->lexer.cur_token);
    if(!name_ident)
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_ASSIGN))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    value = parse_expression(p, PRECEDENCE_LOWEST);
    if(!value)
    {
        goto err;
    }
    if(value->type == EXPRESSION_FUNCTION_LITERAL)
    {
        value->fn_literal.name = ape_strdup(p->alloc, name_ident->value);
        if(!value->fn_literal.name)
        {
            goto err;
        }
    }
    res = statement_make_define(p->alloc, name_ident, value, assignable);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    expression_destroy(value);
    ident_destroy(name_ident);
    return NULL;
}

ApeStatement_t* parse_if_statement(ApeParser_t* p)
{
    bool ok;
    ApePtrArray_t* cases;
    ApeCodeblock_t* alternative;
    ApeIfCase_t* elif;
    ApeStatement_t* res;
    ApeIfCase_t* cond;
    cases = NULL;
    alternative = NULL;
    cases = ptrarray_make(p->alloc);
    if(!cases)
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_LPAREN))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    cond = if_case_make(p->alloc, NULL, NULL);
    if(!cond)
    {
        goto err;
    }
    ok = ptrarray_add(cases, cond);
    if(!ok)
    {
        if_case_destroy(cond);
        goto err;
    }
    cond->test = parse_expression(p, PRECEDENCE_LOWEST);
    if(!cond->test)
    {
        goto err;
    }
    if(!lexer_expect_current(&p->lexer, TOKEN_RPAREN))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    cond->consequence = parse_code_block(p);
    if(!cond->consequence)
    {
        goto err;
    }
    while(lexer_cur_token_is(&p->lexer, TOKEN_ELSE))
    {
        lexer_next_token(&p->lexer);
        if(lexer_cur_token_is(&p->lexer, TOKEN_IF))
        {
            lexer_next_token(&p->lexer);
            if(!lexer_expect_current(&p->lexer, TOKEN_LPAREN))
            {
                goto err;
            }
            lexer_next_token(&p->lexer);
            elif = if_case_make(p->alloc, NULL, NULL);
            if(!elif)
            {
                goto err;
            }
            ok = ptrarray_add(cases, elif);
            if(!ok)
            {
                if_case_destroy(elif);
                goto err;
            }
            elif->test = parse_expression(p, PRECEDENCE_LOWEST);
            if(!elif->test)
            {
                goto err;
            }
            if(!lexer_expect_current(&p->lexer, TOKEN_RPAREN))
            {
                goto err;
            }
            lexer_next_token(&p->lexer);
            elif->consequence = parse_code_block(p);
            if(!elif->consequence)
            {
                goto err;
            }
        }
        else
        {
            alternative = parse_code_block(p);
            if(!alternative)
            {
                goto err;
            }
        }
    }
    res = statement_make_if(p->alloc, cases, alternative);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    ptrarray_destroy_with_items(cases, if_case_destroy);
    code_block_destroy(alternative);
    return NULL;
}

ApeStatement_t* parse_return_statement(ApeParser_t* p)
{
    ApeExpression_t* expr;
    ApeStatement_t* res;
    expr = NULL;
    lexer_next_token(&p->lexer);
    if(!lexer_cur_token_is(&p->lexer, TOKEN_SEMICOLON) && !lexer_cur_token_is(&p->lexer, TOKEN_RBRACE) && !lexer_cur_token_is(&p->lexer, TOKEN_EOF))
    {
        expr = parse_expression(p, PRECEDENCE_LOWEST);
        if(!expr)
        {
            return NULL;
        }
    }
    res = statement_make_return(p->alloc, expr);
    if(!res)
    {
        expression_destroy(expr);
        return NULL;
    }
    return res;
}

ApeStatement_t* parse_expression_statement(ApeParser_t* p)
{
    ApeExpression_t* expr;
    ApeStatement_t* res;
    expr = parse_expression(p, PRECEDENCE_LOWEST);
    if(!expr)
    {
        return NULL;
    }
    if(expr && (!p->config->repl_mode || p->depth > 0))
    {
        if(expr->type != EXPRESSION_ASSIGN && expr->type != EXPRESSION_CALL)
        {
            errors_add_errorf(p->errors, APE_ERROR_PARSING, expr->pos, "Only assignments and function calls can be expression statements");
            expression_destroy(expr);
            return NULL;
        }
    }
    res = statement_make_expression(p->alloc, expr);
    if(!res)
    {
        expression_destroy(expr);
        return NULL;
    }
    return res;
}

ApeStatement_t* parse_while_loop_statement(ApeParser_t* p)
{
    ApeExpression_t* test;
    ApeCodeblock_t* body;
    ApeStatement_t* res;
    test = NULL;
    body = NULL;
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_LPAREN))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    test = parse_expression(p, PRECEDENCE_LOWEST);
    if(!test)
    {
        goto err;
    }
    if(!lexer_expect_current(&p->lexer, TOKEN_RPAREN))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    body = parse_code_block(p);
    if(!body)
    {
        goto err;
    }
    res = statement_make_while_loop(p->alloc, test, body);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    code_block_destroy(body);
    expression_destroy(test);
    return NULL;
}

ApeStatement_t* parse_break_statement(ApeParser_t* p)
{
    lexer_next_token(&p->lexer);
    return statement_make_break(p->alloc);
}

ApeStatement_t* parse_continue_statement(ApeParser_t* p)
{
    lexer_next_token(&p->lexer);
    return statement_make_continue(p->alloc);
}

ApeStatement_t* parse_block_statement(ApeParser_t* p)
{
    ApeCodeblock_t* block;
    ApeStatement_t* res;
    block = parse_code_block(p);
    if(!block)
    {
        return NULL;
    }
    res = statement_make_block(p->alloc, block);
    if(!res)
    {
        code_block_destroy(block);
        return NULL;
    }
    return res;
}

ApeStatement_t* parse_import_statement(ApeParser_t* p)
{
    char* processed_name;
    ApeStatement_t* res;
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_STRING))
    {
        return NULL;
    }
    processed_name = process_and_copy_string(p->alloc, p->lexer.cur_token.literal, p->lexer.cur_token.len);
    if(!processed_name)
    {
        errors_add_error(p->errors, APE_ERROR_PARSING, p->lexer.cur_token.pos, "Error when parsing module name");
        return NULL;
    }
    lexer_next_token(&p->lexer);
    res= statement_make_import(p->alloc, processed_name);
    if(!res)
    {
        allocator_free(p->alloc, processed_name);
        return NULL;
    }
    return res;
}

ApeStatement_t* parse_recover_statement(ApeParser_t* p)
{
    ApeIdent_t* error_ident;
    ApeCodeblock_t* body;
    ApeStatement_t* res;
    error_ident = NULL;
    body = NULL;
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_LPAREN))
    {
        return NULL;
    }
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_IDENT))
    {
        return NULL;
    }
    error_ident = ident_make(p->alloc, p->lexer.cur_token);
    if(!error_ident)
    {
        return NULL;
    }
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_RPAREN))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    body = parse_code_block(p);
    if(!body)
    {
        goto err;
    }
    res = statement_make_recover(p->alloc, error_ident, body);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    code_block_destroy(body);
    ident_destroy(error_ident);
    return NULL;
}

ApeStatement_t* parse_for_loop_statement(ApeParser_t* p)
{
    lexer_next_token(&p->lexer);

    if(!lexer_expect_current(&p->lexer, TOKEN_LPAREN))
    {
        return NULL;
    }

    lexer_next_token(&p->lexer);

    if(lexer_cur_token_is(&p->lexer, TOKEN_IDENT) && lexer_peek_token_is(&p->lexer, TOKEN_IN))
    {
        return parse_foreach(p);
    }
    return parse_classic_for_loop(p);
}

ApeStatement_t* parse_foreach(ApeParser_t* p)
{
    ApeExpression_t* source;
    ApeCodeblock_t* body;
    ApeIdent_t* iterator_ident;
    ApeStatement_t* res;
    source = NULL;
    body = NULL;
    iterator_ident = ident_make(p->alloc, p->lexer.cur_token);
    if(!iterator_ident)
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_IN))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    source = parse_expression(p, PRECEDENCE_LOWEST);
    if(!source)
    {
        goto err;
    }
    if(!lexer_expect_current(&p->lexer, TOKEN_RPAREN))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    body = parse_code_block(p);
    if(!body)
    {
        goto err;
    }
    res = statement_make_foreach(p->alloc, iterator_ident, source, body);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    code_block_destroy(body);
    ident_destroy(iterator_ident);
    expression_destroy(source);
    return NULL;
}

ApeStatement_t* parse_classic_for_loop(ApeParser_t* p)
{
    ApeStatement_t* init;
    ApeExpression_t* test;
    ApeExpression_t* update;
    ApeCodeblock_t* body;
    ApeStatement_t* res;

    init = NULL;
    test = NULL;
    update = NULL;
    body = NULL;
    if(!lexer_cur_token_is(&p->lexer, TOKEN_SEMICOLON))
    {
        init = parse_statement(p);
        if(!init)
        {
            goto err;
        }
        if(init->type != STATEMENT_DEFINE && init->type != STATEMENT_EXPRESSION)
        {
            errors_add_errorf(p->errors, APE_ERROR_PARSING, init->pos, "for loop's init clause should be a define statement or an expression");
            goto err;
        }
        if(!lexer_expect_current(&p->lexer, TOKEN_SEMICOLON))
        {
            goto err;
        }
    }
    lexer_next_token(&p->lexer);
    if(!lexer_cur_token_is(&p->lexer, TOKEN_SEMICOLON))
    {
        test = parse_expression(p, PRECEDENCE_LOWEST);
        if(!test)
        {
            goto err;
        }
        if(!lexer_expect_current(&p->lexer, TOKEN_SEMICOLON))
        {
            goto err;
        }
    }
    lexer_next_token(&p->lexer);
    if(!lexer_cur_token_is(&p->lexer, TOKEN_RPAREN))
    {
        update = parse_expression(p, PRECEDENCE_LOWEST);
        if(!update)
        {
            goto err;
        }
        if(!lexer_expect_current(&p->lexer, TOKEN_RPAREN))
        {
            goto err;
        }
    }
    lexer_next_token(&p->lexer);
    body = parse_code_block(p);
    if(!body)
    {
        goto err;
    }
    res = statement_make_for_loop(p->alloc, init, test, update, body);
    if(!res)
    {
        goto err;
    }

    return res;
err:
    statement_destroy(init);
    expression_destroy(test);
    expression_destroy(update);
    code_block_destroy(body);
    return NULL;
}

ApeStatement_t* parse_function_statement(ApeParser_t* p)
{
    ApeIdent_t* name_ident;
    ApeExpression_t* value;
    ApePosition_t pos;
    ApeStatement_t* res;
    value = NULL;
    name_ident = NULL;
    pos = p->lexer.cur_token.pos;
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_IDENT))
    {
        goto err;
    }
    name_ident = ident_make(p->alloc, p->lexer.cur_token);
    if(!name_ident)
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    value = parse_function_literal(p);
    if(!value)
    {
        goto err;
    }
    value->pos = pos;
    value->fn_literal.name = ape_strdup(p->alloc, name_ident->value);
    if(!value->fn_literal.name)
    {
        goto err;
    }
    res = statement_make_define(p->alloc, name_ident, value, false);
    if(!res)
    {
        goto err;
    }
    return res;

err:
    expression_destroy(value);
    ident_destroy(name_ident);
    return NULL;
}

ApeCodeblock_t* parse_code_block(ApeParser_t* p)
{
    bool ok;
    ApePtrArray_t* statements;
    ApeStatement_t* stmt;
    ApeCodeblock_t* res;
    if(!lexer_expect_current(&p->lexer, TOKEN_LBRACE))
    {
        return NULL;
    }
    lexer_next_token(&p->lexer);
    p->depth++;
    statements = ptrarray_make(p->alloc);
    if(!statements)
    {
        goto err;
    }
    while(!lexer_cur_token_is(&p->lexer, TOKEN_RBRACE))
    {
        if(lexer_cur_token_is(&p->lexer, TOKEN_EOF))
        {
            errors_add_error(p->errors, APE_ERROR_PARSING, p->lexer.cur_token.pos, "Unexpected EOF");
            goto err;
        }
        if(lexer_cur_token_is(&p->lexer, TOKEN_SEMICOLON))
        {
            lexer_next_token(&p->lexer);
            continue;
        }
        stmt = parse_statement(p);
        if(!stmt)
        {
            goto err;
        }
        ok = ptrarray_add(statements, stmt);
        if(!ok)
        {
            statement_destroy(stmt);
            goto err;
        }
    }
    lexer_next_token(&p->lexer);
    p->depth--;
    res = code_block_make(p->alloc, statements);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    p->depth--;
    ptrarray_destroy_with_items(statements, statement_destroy);
    return NULL;
}

ApeExpression_t* parse_expression(ApeParser_t* p, ApePrecedence_t prec)
{
    char* literal;
    ApePosition_t pos;
    ApeRightAssocParseFNCallback_t parse_right_assoc;
    ApeLeftAssocParseFNCallback_t parse_left_assoc;
    ApeExpression_t* left_expr;
    ApeExpression_t* new_left_expr;
    pos = p->lexer.cur_token.pos;
    if(p->lexer.cur_token.type == TOKEN_INVALID)
    {
        errors_add_error(p->errors, APE_ERROR_PARSING, p->lexer.cur_token.pos, "Illegal token");
        return NULL;
    }
    parse_right_assoc = p->right_assoc_parse_fns[p->lexer.cur_token.type];
    if(!parse_right_assoc)
    {
        literal = token_duplicate_literal(p->alloc, &p->lexer.cur_token);
        errors_add_errorf(p->errors, APE_ERROR_PARSING, p->lexer.cur_token.pos, "No prefix parse function for \"%s\" found", literal);
        allocator_free(p->alloc, literal);
        return NULL;
    }
    left_expr = parse_right_assoc(p);
    if(!left_expr)
    {
        return NULL;
    }
    left_expr->pos = pos;
    while(!lexer_cur_token_is(&p->lexer, TOKEN_SEMICOLON) && prec < get_precedence(p->lexer.cur_token.type))
    {
        parse_left_assoc = p->left_assoc_parse_fns[p->lexer.cur_token.type];
        if(!parse_left_assoc)
        {
            return left_expr;
        }
        pos = p->lexer.cur_token.pos;
        new_left_expr= parse_left_assoc(p, left_expr);
        if(!new_left_expr)
        {
            expression_destroy(left_expr);
            return NULL;
        }
        new_left_expr->pos = pos;
        left_expr = new_left_expr;
    }
    return left_expr;
}

ApeExpression_t* parse_identifier(ApeParser_t* p)
{
    ApeIdent_t* ident;
    ApeExpression_t* res;
    ident = ident_make(p->alloc, p->lexer.cur_token);
    if(!ident)
    {
        return NULL;
    }
    res = expression_make_ident(p->alloc, ident);
    if(!res)
    {
        ident_destroy(ident);
        return NULL;
    }
    lexer_next_token(&p->lexer);
    return res;
}

ApeExpression_t* parse_number_literal(ApeParser_t* p)
{
    char* end;
    char* literal;
    double number;
    long parsed_len;
    number = 0;
    errno = 0;
    number = strtod(p->lexer.cur_token.literal, &end);
    parsed_len = end - p->lexer.cur_token.literal;
    if(errno || parsed_len != p->lexer.cur_token.len)
    {
        literal = token_duplicate_literal(p->alloc, &p->lexer.cur_token);
        errors_add_errorf(p->errors, APE_ERROR_PARSING, p->lexer.cur_token.pos, "Parsing number literal \"%s\" failed", literal);
        allocator_free(p->alloc, literal);
        return NULL;
    }
    lexer_next_token(&p->lexer);
    return expression_make_number_literal(p->alloc, number);
}

ApeExpression_t* parse_bool_literal(ApeParser_t* p)
{
    ApeExpression_t* res;
    res = expression_make_bool_literal(p->alloc, p->lexer.cur_token.type == TOKEN_TRUE);
    lexer_next_token(&p->lexer);
    return res;
}

ApeExpression_t* parse_string_literal(ApeParser_t* p)
{
    char* processed_literal;
    processed_literal = process_and_copy_string(p->alloc, p->lexer.cur_token.literal, p->lexer.cur_token.len);
    if(!processed_literal)
    {
        errors_add_error(p->errors, APE_ERROR_PARSING, p->lexer.cur_token.pos, "Error when parsing string literal");
        return NULL;
    }
    lexer_next_token(&p->lexer);
    ApeExpression_t* res = expression_make_string_literal(p->alloc, processed_literal);
    if(!res)
    {
        allocator_free(p->alloc, processed_literal);
        return NULL;
    }
    return res;
}

ApeExpression_t* parse_template_string_literal(ApeParser_t* p)
{
    char* processed_literal;
    ApeExpression_t* left_string_expr;
    ApeExpression_t* template_expr;
    ApeExpression_t* to_str_call_expr;
    ApeExpression_t* left_add_expr;
    ApeExpression_t* right_expr;
    ApeExpression_t* right_add_expr;
    ApePosition_t pos;

    processed_literal = NULL;
    left_string_expr = NULL;
    template_expr = NULL;
    to_str_call_expr = NULL;
    left_add_expr = NULL;
    right_expr = NULL;
    right_add_expr = NULL;
    processed_literal = process_and_copy_string(p->alloc, p->lexer.cur_token.literal, p->lexer.cur_token.len);
    if(!processed_literal)
    {
        errors_add_error(p->errors, APE_ERROR_PARSING, p->lexer.cur_token.pos, "Error when parsing string literal");
        return NULL;
    }
    lexer_next_token(&p->lexer);

    if(!lexer_expect_current(&p->lexer, TOKEN_LBRACE))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);

    pos = p->lexer.cur_token.pos;

    left_string_expr = expression_make_string_literal(p->alloc, processed_literal);
    if(!left_string_expr)
    {
        goto err;
    }
    left_string_expr->pos = pos;
    processed_literal = NULL;
    pos = p->lexer.cur_token.pos;
    template_expr = parse_expression(p, PRECEDENCE_LOWEST);
    if(!template_expr)
    {
        goto err;
    }
    to_str_call_expr = wrap_expression_in_function_call(p->alloc, template_expr, "to_str");
    if(!to_str_call_expr)
    {
        goto err;
    }
    to_str_call_expr->pos = pos;
    template_expr = NULL;
    left_add_expr = expression_make_infix(p->alloc, OPERATOR_PLUS, left_string_expr, to_str_call_expr);
    if(!left_add_expr)
    {
        goto err;
    }
    left_add_expr->pos = pos;
    left_string_expr = NULL;
    to_str_call_expr = NULL;
    if(!lexer_expect_current(&p->lexer, TOKEN_RBRACE))
    {
        goto err;
    }
    lexer_previous_token(&p->lexer);
    lexer_continue_template_string(&p->lexer);
    lexer_next_token(&p->lexer);
    lexer_next_token(&p->lexer);
    pos = p->lexer.cur_token.pos;
    right_expr = parse_expression(p, PRECEDENCE_HIGHEST);
    if(!right_expr)
    {
        goto err;
    }
    right_add_expr = expression_make_infix(p->alloc, OPERATOR_PLUS, left_add_expr, right_expr);
    if(!right_add_expr)
    {
        goto err;
    }
    right_add_expr->pos = pos;
    left_add_expr = NULL;
    right_expr = NULL;

    return right_add_expr;
err:
    expression_destroy(right_add_expr);
    expression_destroy(right_expr);
    expression_destroy(left_add_expr);
    expression_destroy(to_str_call_expr);
    expression_destroy(template_expr);
    expression_destroy(left_string_expr);
    allocator_free(p->alloc, processed_literal);
    return NULL;
}

ApeExpression_t* parse_null_literal(ApeParser_t* p)
{
    lexer_next_token(&p->lexer);
    return expression_make_null_literal(p->alloc);
}

ApeExpression_t* parse_array_literal(ApeParser_t* p)
{
    ApePtrArray_t* array;
    ApeExpression_t* res;
    array = parse_expression_list(p, TOKEN_LBRACKET, TOKEN_RBRACKET, true);
    if(!array)
    {
        return NULL;
    }
    res = expression_make_array_literal(p->alloc, array);
    if(!res)
    {
        ptrarray_destroy_with_items(array, expression_destroy);
        return NULL;
    }
    return res;
}

ApeExpression_t* parse_map_literal(ApeParser_t* p)
{
    bool ok;
    char* str;
    ApePtrArray_t* keys;
    ApePtrArray_t* values;
    ApeExpression_t* key;
    ApeExpression_t* value;
    ApeExpression_t* res;
    keys = ptrarray_make(p->alloc);
    values = ptrarray_make(p->alloc);
    if(!keys || !values)
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    while(!lexer_cur_token_is(&p->lexer, TOKEN_RBRACE))
    {
        key = NULL;
        if(lexer_cur_token_is(&p->lexer, TOKEN_IDENT))
        {
            str = token_duplicate_literal(p->alloc, &p->lexer.cur_token);
            key = expression_make_string_literal(p->alloc, str);
            if(!key)
            {
                allocator_free(p->alloc, str);
                goto err;
            }
            key->pos = p->lexer.cur_token.pos;
            lexer_next_token(&p->lexer);
        }
        else
        {
            key = parse_expression(p, PRECEDENCE_LOWEST);
            if(!key)
            {
                goto err;
            }
            switch(key->type)
            {
                case EXPRESSION_STRING_LITERAL:
                case EXPRESSION_NUMBER_LITERAL:
                case EXPRESSION_BOOL_LITERAL:
                {
                    break;
                }
                default:
                {
                    errors_add_errorf(p->errors, APE_ERROR_PARSING, key->pos, "Invalid map literal key type");
                    expression_destroy(key);
                    goto err;
                }
            }
        }

        ok = ptrarray_add(keys, key);
        if(!ok)
        {
            expression_destroy(key);
            goto err;
        }

        if(!lexer_expect_current(&p->lexer, TOKEN_COLON))
        {
            goto err;
        }

        lexer_next_token(&p->lexer);

        value = parse_expression(p, PRECEDENCE_LOWEST);
        if(!value)
        {
            goto err;
        }
        ok = ptrarray_add(values, value);
        if(!ok)
        {
            expression_destroy(value);
            goto err;
        }

        if(lexer_cur_token_is(&p->lexer, TOKEN_RBRACE))
        {
            break;
        }

        if(!lexer_expect_current(&p->lexer, TOKEN_COMMA))
        {
            goto err;
        }

        lexer_next_token(&p->lexer);
    }

    lexer_next_token(&p->lexer);

    res = expression_make_map_literal(p->alloc, keys, values);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    ptrarray_destroy_with_items(keys, expression_destroy);
    ptrarray_destroy_with_items(values, expression_destroy);
    return NULL;
}

ApeExpression_t* parse_prefix_expression(ApeParser_t* p)
{
    ApeOperator_t op;
    ApeExpression_t* right;
    ApeExpression_t* res;
    op = token_to_operator(p->lexer.cur_token.type);
    lexer_next_token(&p->lexer);
    right = parse_expression(p, PRECEDENCE_PREFIX);
    if(!right)
    {
        return NULL;
    }
    res = expression_make_prefix(p->alloc, op, right);
    if(!res)
    {
        expression_destroy(right);
        return NULL;
    }
    return res;
}

ApeExpression_t* parse_infix_expression(ApeParser_t* p, ApeExpression_t* left)
{
    ApeOperator_t op;
    ApePrecedence_t prec;
    ApeExpression_t* right;
    ApeExpression_t* res;
    op = token_to_operator(p->lexer.cur_token.type);
    prec = get_precedence(p->lexer.cur_token.type);
    lexer_next_token(&p->lexer);
    right = parse_expression(p, prec);
    if(!right)
    {
        return NULL;
    }
    res = expression_make_infix(p->alloc, op, left, right);
    if(!res)
    {
        expression_destroy(right);
        return NULL;
    }
    return res;
}

ApeExpression_t* parse_grouped_expression(ApeParser_t* p)
{
    ApeExpression_t* expr;
    lexer_next_token(&p->lexer);
    expr = parse_expression(p, PRECEDENCE_LOWEST);
    if(!expr || !lexer_expect_current(&p->lexer, TOKEN_RPAREN))
    {
        expression_destroy(expr);
        return NULL;
    }
    lexer_next_token(&p->lexer);
    return expr;
}

ApeExpression_t* parse_function_literal(ApeParser_t* p)
{
    bool ok;
    ApePtrArray_t* params;
    ApeCodeblock_t* body;
    ApeExpression_t* res;
    p->depth++;
    params = NULL;
    body = NULL;
    if(lexer_cur_token_is(&p->lexer, TOKEN_FUNCTION))
    {
        lexer_next_token(&p->lexer);
    }
    params = ptrarray_make(p->alloc);
    ok = parse_function_parameters(p, params);
    if(!ok)
    {
        goto err;
    }
    body = parse_code_block(p);
    if(!body)
    {
        goto err;
    }
    res = expression_make_fn_literal(p->alloc, params, body);
    if(!res)
    {
        goto err;
    }
    p->depth -= 1;
    return res;
err:
    code_block_destroy(body);
    ptrarray_destroy_with_items(params, ident_destroy);
    p->depth -= 1;
    return NULL;
}

bool parse_function_parameters(ApeParser_t* p, ApePtrArray_t * out_params)
{
    bool ok;
    ApeIdent_t* ident;
    if(!lexer_expect_current(&p->lexer, TOKEN_LPAREN))
    {
        return false;
    }
    lexer_next_token(&p->lexer);
    if(lexer_cur_token_is(&p->lexer, TOKEN_RPAREN))
    {
        lexer_next_token(&p->lexer);
        return true;
    }
    if(!lexer_expect_current(&p->lexer, TOKEN_IDENT))
    {
        return false;
    }
    ident = ident_make(p->alloc, p->lexer.cur_token);
    if(!ident)
    {
        return false;
    }
    ok = ptrarray_add(out_params, ident);
    if(!ok)
    {
        ident_destroy(ident);
        return false;
    }
    lexer_next_token(&p->lexer);
    while(lexer_cur_token_is(&p->lexer, TOKEN_COMMA))
    {
        lexer_next_token(&p->lexer);
        if(!lexer_expect_current(&p->lexer, TOKEN_IDENT))
        {
            return false;
        }
        ident = ident_make(p->alloc, p->lexer.cur_token);
        if(!ident)
        {
            return false;
        }
        ok = ptrarray_add(out_params, ident);
        if(!ok)
        {
            ident_destroy(ident);
            return false;
        }
        lexer_next_token(&p->lexer);
    }
    if(!lexer_expect_current(&p->lexer, TOKEN_RPAREN))
    {
        return false;
    }

    lexer_next_token(&p->lexer);
    return true;
}

ApeExpression_t* parse_call_expression(ApeParser_t* p, ApeExpression_t* left)
{
    ApeExpression_t* function;
    ApePtrArray_t* args;
    ApeExpression_t* res;
    function = left;
    args = parse_expression_list(p, TOKEN_LPAREN, TOKEN_RPAREN, false);
    if(!args)
    {
        return NULL;
    }
    res = expression_make_call(p->alloc, function, args);
    if(!res)
    {
        ptrarray_destroy_with_items(args, expression_destroy);
        return NULL;
    }
    return res;
}

ApePtrArray_t* parse_expression_list(ApeParser_t* p, ApeTokenType_t start_token, ApeTokenType_t end_token, bool trailing_comma_allowed)
{
    bool ok;
    ApePtrArray_t* res;
    ApeExpression_t* arg_expr;
    if(!lexer_expect_current(&p->lexer, start_token))
    {
        return NULL;
    }
    lexer_next_token(&p->lexer);
    res = ptrarray_make(p->alloc);
    if(lexer_cur_token_is(&p->lexer, end_token))
    {
        lexer_next_token(&p->lexer);
        return res;
    }
    arg_expr = parse_expression(p, PRECEDENCE_LOWEST);
    if(!arg_expr)
    {
        goto err;
    }
    ok = ptrarray_add(res, arg_expr);
    if(!ok)
    {
        expression_destroy(arg_expr);
        goto err;
    }
    while(lexer_cur_token_is(&p->lexer, TOKEN_COMMA))
    {
        lexer_next_token(&p->lexer);
        if(trailing_comma_allowed && lexer_cur_token_is(&p->lexer, end_token))
        {
            break;
        }
        arg_expr = parse_expression(p, PRECEDENCE_LOWEST);
        if(!arg_expr)
        {
            goto err;
        }
        ok = ptrarray_add(res, arg_expr);
        if(!ok)
        {
            expression_destroy(arg_expr);
            goto err;
        }
    }
    if(!lexer_expect_current(&p->lexer, end_token))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    return res;
err:
    ptrarray_destroy_with_items(res, expression_destroy);
    return NULL;
}

ApeExpression_t* parse_index_expression(ApeParser_t* p, ApeExpression_t* left)
{
    ApeExpression_t* index;
    ApeExpression_t* res;
    lexer_next_token(&p->lexer);
    index = parse_expression(p, PRECEDENCE_LOWEST);
    if(!index)
    {
        return NULL;
    }
    if(!lexer_expect_current(&p->lexer, TOKEN_RBRACKET))
    {
        expression_destroy(index);
        return NULL;
    }
    lexer_next_token(&p->lexer);
    res = expression_make_index(p->alloc, left, index);
    if(!res)
    {
        expression_destroy(index);
        return NULL;
    }
    return res;
}

ApeExpression_t* parse_assign_expression(ApeParser_t* p, ApeExpression_t* left)
{
    ApeTokenType_t assign_type;
    ApePosition_t pos;
    ApeOperator_t op;
    ApeExpression_t* source;
    ApeExpression_t* left_copy;
    ApeExpression_t* new_source;
    ApeExpression_t* res;

    source = NULL;
    assign_type = p->lexer.cur_token.type;
    lexer_next_token(&p->lexer);
    source = parse_expression(p, PRECEDENCE_LOWEST);
    if(!source)
    {
        goto err;
    }
    switch(assign_type)
    {
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
                op = token_to_operator(assign_type);
                left_copy = expression_copy(left);
                if(!left_copy)
                {
                    goto err;
                }
                pos = source->pos;
                new_source = expression_make_infix(p->alloc, op, left_copy, source);
                if(!new_source)
                {
                    expression_destroy(left_copy);
                    goto err;
                }
                new_source->pos = pos;
                source = new_source;
            }
            break;
        case TOKEN_ASSIGN:
            {
            }
            break;
        default:
            {
                APE_ASSERT(false);
            }
            break;
    }
    res = expression_make_assign(p->alloc, left, source, false);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    expression_destroy(source);
    return NULL;
}

ApeExpression_t* parse_logical_expression(ApeParser_t* p, ApeExpression_t* left)
{
    ApeOperator_t op;
    ApePrecedence_t prec;
    ApeExpression_t* right;
    ApeExpression_t* res;
    op = token_to_operator(p->lexer.cur_token.type);
    prec = get_precedence(p->lexer.cur_token.type);
    lexer_next_token(&p->lexer);
    right = parse_expression(p, prec);
    if(!right)
    {
        return NULL;
    }
    res = expression_make_logical(p->alloc, op, left, right);
    if(!res)
    {
        expression_destroy(right);
        return NULL;
    }
    return res;
}

ApeExpression_t* parse_ternary_expression(ApeParser_t* p, ApeExpression_t* left)
{
    ApeExpression_t* if_true;
    ApeExpression_t* if_false;
    ApeExpression_t* res;
    lexer_next_token(&p->lexer);
    if_true = parse_expression(p, PRECEDENCE_LOWEST);
    if(!if_true)
    {
        return NULL;
    }
    if(!lexer_expect_current(&p->lexer, TOKEN_COLON))
    {
        expression_destroy(if_true);
        return NULL;
    }
    lexer_next_token(&p->lexer);
    if_false = parse_expression(p, PRECEDENCE_LOWEST);
    if(!if_false)
    {
        expression_destroy(if_true);
        return NULL;
    }
    res = expression_make_ternary(p->alloc, left, if_true, if_false);
    if(!res)
    {
        expression_destroy(if_true);
        expression_destroy(if_false);
        return NULL;
    }
    return res;
}

ApeExpression_t* parse_incdec_prefix_expression(ApeParser_t* p)
{
    ApeExpression_t* source;
    ApeTokenType_t operation_type;
    ApePosition_t pos;
    ApeOperator_t op;
    ApeExpression_t* dest;
    ApeExpression_t* one_literal;
    ApeExpression_t* dest_copy;
    ApeExpression_t* operation;
    ApeExpression_t* res;

    source = NULL;
    operation_type = p->lexer.cur_token.type;
    pos = p->lexer.cur_token.pos;
    lexer_next_token(&p->lexer);
    op = token_to_operator(operation_type);
    dest = parse_expression(p, PRECEDENCE_PREFIX);
    if(!dest)
    {
        goto err;
    }
    one_literal = expression_make_number_literal(p->alloc, 1);
    if(!one_literal)
    {
        expression_destroy(dest);
        goto err;
    }
    one_literal->pos = pos;
    dest_copy = expression_copy(dest);
    if(!dest_copy)
    {
        expression_destroy(one_literal);
        expression_destroy(dest);
        goto err;
    }
    operation = expression_make_infix(p->alloc, op, dest_copy, one_literal);
    if(!operation)
    {
        expression_destroy(dest_copy);
        expression_destroy(dest);
        expression_destroy(one_literal);
        goto err;
    }
    operation->pos = pos;

    res = expression_make_assign(p->alloc, dest, operation, false);
    if(!res)
    {
        expression_destroy(dest);
        expression_destroy(operation);
        goto err;
    }
    return res;
err:
    expression_destroy(source);
    return NULL;
}

ApeExpression_t* parse_incdec_postfix_expression(ApeParser_t* p, ApeExpression_t* left)
{
    ApeExpression_t* source;
    ApeTokenType_t operation_type;
    ApePosition_t pos;
    ApeOperator_t op;
    ApeExpression_t* left_copy;
    ApeExpression_t* one_literal;
    ApeExpression_t* operation;
    ApeExpression_t* res;
    source = NULL;
    operation_type = p->lexer.cur_token.type;
    pos = p->lexer.cur_token.pos;
    lexer_next_token(&p->lexer);
    op = token_to_operator(operation_type);
    left_copy = expression_copy(left);
    if(!left_copy)
    {
        goto err;
    }
    one_literal = expression_make_number_literal(p->alloc, 1);
    if(!one_literal)
    {
        expression_destroy(left_copy);
        goto err;
    }
    one_literal->pos = pos;
    operation = expression_make_infix(p->alloc, op, left_copy, one_literal);
    if(!operation)
    {
        expression_destroy(one_literal);
        expression_destroy(left_copy);
        goto err;
    }
    operation->pos = pos;
    res = expression_make_assign(p->alloc, left, operation, true);
    if(!res)
    {
        expression_destroy(operation);
        goto err;
    }

    return res;
err:
    expression_destroy(source);
    return NULL;
}


ApeExpression_t* parse_dot_expression(ApeParser_t* p, ApeExpression_t* left)
{
    lexer_next_token(&p->lexer);

    if(!lexer_expect_current(&p->lexer, TOKEN_IDENT))
    {
        return NULL;
    }

    char* str = token_duplicate_literal(p->alloc, &p->lexer.cur_token);
    ApeExpression_t* index = expression_make_string_literal(p->alloc, str);
    if(!index)
    {
        allocator_free(p->alloc, str);
        return NULL;
    }
    index->pos = p->lexer.cur_token.pos;

    lexer_next_token(&p->lexer);

    ApeExpression_t* res = expression_make_index(p->alloc, left, index);
    if(!res)
    {
        expression_destroy(index);
        return NULL;
    }
    return res;
}

ApePrecedence_t get_precedence(ApeTokenType_t tk)
{
    switch(tk)
    {
        case TOKEN_EQ:
            return PRECEDENCE_EQUALS;
        case TOKEN_NOT_EQ:
            return PRECEDENCE_EQUALS;
        case TOKEN_LT:
            return PRECEDENCE_LESSGREATER;
        case TOKEN_LTE:
            return PRECEDENCE_LESSGREATER;
        case TOKEN_GT:
            return PRECEDENCE_LESSGREATER;
        case TOKEN_GTE:
            return PRECEDENCE_LESSGREATER;
        case TOKEN_PLUS:
            return PRECEDENCE_SUM;
        case TOKEN_MINUS:
            return PRECEDENCE_SUM;
        case TOKEN_SLASH:
            return PRECEDENCE_PRODUCT;
        case TOKEN_ASTERISK:
            return PRECEDENCE_PRODUCT;
        case TOKEN_PERCENT:
            return PRECEDENCE_PRODUCT;
        case TOKEN_LPAREN:
            return PRECEDENCE_POSTFIX;
        case TOKEN_LBRACKET:
            return PRECEDENCE_POSTFIX;
        case TOKEN_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_PLUS_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_MINUS_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_ASTERISK_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_SLASH_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_PERCENT_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_BIT_AND_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_BIT_OR_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_BIT_XOR_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_LSHIFT_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_RSHIFT_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_DOT:
            return PRECEDENCE_POSTFIX;
        case TOKEN_AND:
            return PRECEDENCE_LOGICAL_AND;
        case TOKEN_OR:
            return PRECEDENCE_LOGICAL_OR;
        case TOKEN_BIT_OR:
            return PRECEDENCE_BIT_OR;
        case TOKEN_BIT_XOR:
            return PRECEDENCE_BIT_XOR;
        case TOKEN_BIT_AND:
            return PRECEDENCE_BIT_AND;
        case TOKEN_LSHIFT:
            return PRECEDENCE_SHIFT;
        case TOKEN_RSHIFT:
            return PRECEDENCE_SHIFT;
        case TOKEN_QUESTION:
            return PRECEDENCE_TERNARY;
        case TOKEN_PLUS_PLUS:
            return PRECEDENCE_INCDEC;
        case TOKEN_MINUS_MINUS:
            return PRECEDENCE_INCDEC;
        default:
            return PRECEDENCE_LOWEST;
    }
}

ApeOperator_t token_to_operator(ApeTokenType_t tk)
{
    switch(tk)
    {
        case TOKEN_ASSIGN:
            return OPERATOR_ASSIGN;
        case TOKEN_PLUS:
            return OPERATOR_PLUS;
        case TOKEN_MINUS:
            return OPERATOR_MINUS;
        case TOKEN_BANG:
            return OPERATOR_BANG;
        case TOKEN_ASTERISK:
            return OPERATOR_ASTERISK;
        case TOKEN_SLASH:
            return OPERATOR_SLASH;
        case TOKEN_LT:
            return OPERATOR_LT;
        case TOKEN_LTE:
            return OPERATOR_LTE;
        case TOKEN_GT:
            return OPERATOR_GT;
        case TOKEN_GTE:
            return OPERATOR_GTE;
        case TOKEN_EQ:
            return OPERATOR_EQ;
        case TOKEN_NOT_EQ:
            return OPERATOR_NOT_EQ;
        case TOKEN_PERCENT:
            return OPERATOR_MODULUS;
        case TOKEN_AND:
            return OPERATOR_LOGICAL_AND;
        case TOKEN_OR:
            return OPERATOR_LOGICAL_OR;
        case TOKEN_PLUS_ASSIGN:
            return OPERATOR_PLUS;
        case TOKEN_MINUS_ASSIGN:
            return OPERATOR_MINUS;
        case TOKEN_ASTERISK_ASSIGN:
            return OPERATOR_ASTERISK;
        case TOKEN_SLASH_ASSIGN:
            return OPERATOR_SLASH;
        case TOKEN_PERCENT_ASSIGN:
            return OPERATOR_MODULUS;
        case TOKEN_BIT_AND_ASSIGN:
            return OPERATOR_BIT_AND;
        case TOKEN_BIT_OR_ASSIGN:
            return OPERATOR_BIT_OR;
        case TOKEN_BIT_XOR_ASSIGN:
            return OPERATOR_BIT_XOR;
        case TOKEN_LSHIFT_ASSIGN:
            return OPERATOR_LSHIFT;
        case TOKEN_RSHIFT_ASSIGN:
            return OPERATOR_RSHIFT;
        case TOKEN_BIT_AND:
            return OPERATOR_BIT_AND;
        case TOKEN_BIT_OR:
            return OPERATOR_BIT_OR;
        case TOKEN_BIT_XOR:
            return OPERATOR_BIT_XOR;
        case TOKEN_LSHIFT:
            return OPERATOR_LSHIFT;
        case TOKEN_RSHIFT:
            return OPERATOR_RSHIFT;
        case TOKEN_PLUS_PLUS:
            return OPERATOR_PLUS;
        case TOKEN_MINUS_MINUS:
            return OPERATOR_MINUS;
        default:
        {
            APE_ASSERT(false);
            return OPERATOR_NONE;
        }
    }
}

char escape_char(const char c)
{
    switch(c)
    {
        case '\"':
            return '\"';
        case '\\':
            return '\\';
        case '/':
            return '/';
        case 'b':
            return '\b';
        case 'f':
            return '\f';
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        case '0':
            return '\0';
        default:
            return c;
    }
}

char* process_and_copy_string(ApeAllocator_t* alloc, const char* input, size_t len)
{
    size_t in_i;
    size_t out_i;
    char* output;
    output = (char*)allocator_malloc(alloc, len + 1);
    if(!output)
    {
        return NULL;
    }
    in_i = 0;
    out_i = 0;
    while(input[in_i] != '\0' && in_i < len)
    {
        if(input[in_i] == '\\')
        {
            in_i++;
            output[out_i] = escape_char(input[in_i]);
            if(output[out_i] < 0)
            {
                goto error;
            }
        }
        else
        {
            output[out_i] = input[in_i];
        }
        out_i++;
        in_i++;
    }
    output[out_i] = '\0';
    return output;
error:
    allocator_free(alloc, output);
    return NULL;
}

ApeExpression_t* wrap_expression_in_function_call(ApeAllocator_t* alloc, ApeExpression_t* expr, const char* function_name)
{
    ApeToken_t fn_token;
    token_init(&fn_token, TOKEN_IDENT, function_name, (int)strlen(function_name));
    fn_token.pos = expr->pos;

    ApeIdent_t* ident = ident_make(alloc, fn_token);
    if(!ident)
    {
        return NULL;
    }
    ident->pos = fn_token.pos;

    ApeExpression_t* function_ident_expr = expression_make_ident(alloc, ident);
    ;
    if(!function_ident_expr)
    {
        ident_destroy(ident);
        return NULL;
    }
    function_ident_expr->pos = expr->pos;
    ident = NULL;

    ApePtrArray_t* args = ptrarray_make(alloc);
    if(!args)
    {
        expression_destroy(function_ident_expr);
        return NULL;
    }

    bool ok = ptrarray_add(args, expr);
    if(!ok)
    {
        ptrarray_destroy(args);
        expression_destroy(function_ident_expr);
        return NULL;
    }

    ApeExpression_t* call_expr = expression_make_call(alloc, function_ident_expr, args);
    if(!call_expr)
    {
        ptrarray_destroy(args);
        expression_destroy(function_ident_expr);
        return NULL;
    }
    call_expr->pos = expr->pos;

    return call_expr;
}


