
#include "ape.h"

static const ApePosition_t gprsprivsrcposinvalid = { NULL, -1, -1 };

static ApePrecedence_t ape_parser_getprecedence(ApeTokenType_t tk);
static ApeOperator_t ape_parser_tokentooperator(ApeTokenType_t tk);
static char ape_parser_escapechar(const char c);
static char *ape_ast_processandcopystring(ApeAllocator_t *alloc, const char *input, size_t len);
static ApeExpression_t *ape_ast_wrapexprinfunccall(ApeContext_t *ctx, ApeExpression_t *expr, const char *functionname);

static ApeIdent_t* ape_ast_make_ident(ApeContext_t* ctx, ApeToken_t tok)
{
    ApeIdent_t* res = (ApeIdent_t*)allocator_malloc(&ctx->alloc, sizeof(ApeIdent_t));
    if(!res)
    {
        return NULL;
    }
    res->context = ctx;
    res->alloc = &ctx->alloc;
    res->value = ape_lexer_tokendupliteral(ctx, &tok);
    if(!res->value)
    {
        allocator_free(&ctx->alloc, res);
        return NULL;
    }
    res->pos = tok.pos;
    return res;
}

static ApeIdent_t* ape_ast_copy_ident(ApeIdent_t* ident)
{
    ApeIdent_t* res = (ApeIdent_t*)allocator_malloc(ident->alloc, sizeof(ApeIdent_t));
    if(!res)
    {
        return NULL;
    }
    res->context = ident->context;
    res->alloc = ident->alloc;
    res->value = ape_util_strdup(ident->context, ident->value);
    if(!res->value)
    {
        allocator_free(ident->alloc, res);
        return NULL;
    }
    res->pos = ident->pos;
    return res;
}

ApeExpression_t* ape_ast_make_identexpr(ApeContext_t* ctx, ApeIdent_t* ident)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, EXPRESSION_IDENT);
    if(!res)
    {
        return NULL;
    }
    res->ident = ident;
    return res;
}

ApeExpression_t* ape_ast_make_numberliteralexpr(ApeContext_t* ctx, ApeFloat_t val)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, EXPRESSION_NUMBER_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->numberliteral = val;
    return res;
}

ApeExpression_t* ape_ast_make_boolliteralexpr(ApeContext_t* ctx, bool val)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, EXPRESSION_BOOL_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->boolliteral = val;
    return res;
}

ApeExpression_t* ape_ast_make_stringliteralexpr(ApeContext_t* ctx, char* value)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, EXPRESSION_STRING_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->stringliteral = value;
    return res;
}

ApeExpression_t* ape_ast_make_nullliteralexpr(ApeContext_t* ctx)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, EXPRESSION_NULL_LITERAL);
    if(!res)
    {
        return NULL;
    }
    return res;
}

ApeExpression_t* ape_ast_make_arrayliteralexpr(ApeContext_t* ctx, ApePtrArray_t * values)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, EXPRESSION_ARRAY_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->array = values;
    return res;
}

ApeExpression_t* ape_ast_make_mapliteralexpr(ApeContext_t* ctx, ApePtrArray_t * keys, ApePtrArray_t * values)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, EXPRESSION_MAP_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->map.keys = keys;
    res->map.values = values;
    return res;
}

ApeExpression_t* ape_ast_make_prefixexpr(ApeContext_t* ctx, ApeOperator_t op, ApeExpression_t* right)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, EXPRESSION_PREFIX);
    if(!res)
    {
        return NULL;
    }
    res->prefix.op = op;
    res->prefix.right = right;
    return res;
}

ApeExpression_t* ape_ast_make_infixexpr(ApeContext_t* ctx, ApeOperator_t op, ApeExpression_t* left, ApeExpression_t* right)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, EXPRESSION_INFIX);
    if(!res)
    {
        return NULL;
    }
    res->infix.op = op;
    res->infix.left = left;
    res->infix.right = right;
    return res;
}

ApeExpression_t* ape_ast_make_fnliteralexpr(ApeContext_t* ctx, ApePtrArray_t * params, ApeCodeblock_t* body)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, EXPRESSION_FUNCTION_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->fnliteral.name = NULL;
    res->fnliteral.params = params;
    res->fnliteral.body = body;
    return res;
}

ApeExpression_t* ape_ast_make_callexpr(ApeContext_t* ctx, ApeExpression_t* function, ApePtrArray_t * args)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, EXPRESSION_CALL);
    if(!res)
    {
        return NULL;
    }
    res->callexpr.function = function;
    res->callexpr.args = args;
    return res;
}

ApeExpression_t* ape_ast_make_indexexpr(ApeContext_t* ctx, ApeExpression_t* left, ApeExpression_t* index)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, EXPRESSION_INDEX);
    if(!res)
    {
        return NULL;
    }
    res->indexexpr.left = left;
    res->indexexpr.index = index;
    return res;
}

ApeExpression_t* ape_ast_make_assignexpr(ApeContext_t* ctx, ApeExpression_t* dest, ApeExpression_t* source, bool ispostfix)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, EXPRESSION_ASSIGN);
    if(!res)
    {
        return NULL;
    }
    res->assign.dest = dest;
    res->assign.source = source;
    res->assign.ispostfix = ispostfix;
    return res;
}

ApeExpression_t* ape_ast_make_logicalexpr(ApeContext_t* ctx, ApeOperator_t op, ApeExpression_t* left, ApeExpression_t* right)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, EXPRESSION_LOGICAL);
    if(!res)
    {
        return NULL;
    }
    res->logical.op = op;
    res->logical.left = left;
    res->logical.right = right;
    return res;
}

ApeExpression_t* ape_ast_make_ternaryexpr(ApeContext_t* ctx, ApeExpression_t* test, ApeExpression_t* iftrue, ApeExpression_t* iffalse)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, EXPRESSION_TERNARY);
    if(!res)
    {
        return NULL;
    }
    res->ternary.test = test;
    res->ternary.iftrue = iftrue;
    res->ternary.iffalse = iffalse;
    return res;
}

void* ape_ast_destroy_expr(ApeExpression_t* expr)
{
    if(!expr)
    {
        return NULL;
    }
    switch(expr->type)
    {
        case EXPRESSION_NONE:
        {
            APE_ASSERT(false);
            break;
        }
        case EXPRESSION_IDENT:
        {
            ape_ast_destroy_ident(expr->ident);
            break;
        }
        case EXPRESSION_NUMBER_LITERAL:
        case EXPRESSION_BOOL_LITERAL:
        {
            break;
        }
        case EXPRESSION_STRING_LITERAL:
        {
            allocator_free(expr->alloc, expr->stringliteral);
            break;
        }
        case EXPRESSION_NULL_LITERAL:
        {
            break;
        }
        case EXPRESSION_ARRAY_LITERAL:
        {
            ape_ptrarray_destroywithitems(expr->array, (ApeDataCallback_t)ape_ast_destroy_expr);
            break;
        }
        case EXPRESSION_MAP_LITERAL:
        {
            ape_ptrarray_destroywithitems(expr->map.keys, (ApeDataCallback_t)ape_ast_destroy_expr);
            ape_ptrarray_destroywithitems(expr->map.values, (ApeDataCallback_t)ape_ast_destroy_expr);
            break;
        }
        case EXPRESSION_PREFIX:
        {
            ape_ast_destroy_expr(expr->prefix.right);
            break;
        }
        case EXPRESSION_INFIX:
        {
            ape_ast_destroy_expr(expr->infix.left);
            ape_ast_destroy_expr(expr->infix.right);
            break;
        }
        case EXPRESSION_FUNCTION_LITERAL:
        {
            ApeFnLiteral_t* fn = &expr->fnliteral;
            allocator_free(expr->alloc, fn->name);
            ape_ptrarray_destroywithitems(fn->params, (ApeDataCallback_t)ape_ast_destroy_ident);
            ape_ast_destroy_codeblock(fn->body);
            break;
        }
        case EXPRESSION_CALL:
        {
            ape_ptrarray_destroywithitems(expr->callexpr.args, (ApeDataCallback_t)ape_ast_destroy_expr);
            ape_ast_destroy_expr(expr->callexpr.function);
            break;
        }
        case EXPRESSION_INDEX:
        {
            ape_ast_destroy_expr(expr->indexexpr.left);
            ape_ast_destroy_expr(expr->indexexpr.index);
            break;
        }
        case EXPRESSION_ASSIGN:
        {
            ape_ast_destroy_expr(expr->assign.dest);
            ape_ast_destroy_expr(expr->assign.source);
            break;
        }
        case EXPRESSION_LOGICAL:
        {
            ape_ast_destroy_expr(expr->logical.left);
            ape_ast_destroy_expr(expr->logical.right);
            break;
        }
        case EXPRESSION_TERNARY:
        {
            ape_ast_destroy_expr(expr->ternary.test);
            ape_ast_destroy_expr(expr->ternary.iftrue);
            ape_ast_destroy_expr(expr->ternary.iffalse);
            break;
        }
    }
    allocator_free(expr->alloc, expr);
    return NULL;
}

ApeExpression_t* ape_ast_copy_expr(ApeExpression_t* expr)
{
    if(!expr)
    {
        return NULL;
    }
    ApeExpression_t* res = NULL;
    switch(expr->type)
    {
        case EXPRESSION_NONE:
        {
            APE_ASSERT(false);
            break;
        }
        case EXPRESSION_IDENT:
        {
            ApeIdent_t* ident = ape_ast_copy_ident(expr->ident);
            if(!ident)
            {
                return NULL;
            }
            res = ape_ast_make_identexpr(expr->context, ident);
            if(!res)
            {
                ape_ast_destroy_ident(ident);
                return NULL;
            }
            break;
        }
        case EXPRESSION_NUMBER_LITERAL:
        {
            res = ape_ast_make_numberliteralexpr(expr->context, expr->numberliteral);
            break;
        }
        case EXPRESSION_BOOL_LITERAL:
        {
            res = ape_ast_make_boolliteralexpr(expr->context, expr->boolliteral);
            break;
        }
        case EXPRESSION_STRING_LITERAL:
        {
            char* stringcopy = ape_util_strdup(expr->context, expr->stringliteral);
            if(!stringcopy)
            {
                return NULL;
            }
            res = ape_ast_make_stringliteralexpr(expr->context, stringcopy);
            if(!res)
            {
                allocator_free(expr->alloc, stringcopy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_NULL_LITERAL:
        {
            res = ape_ast_make_nullliteralexpr(expr->context);
            break;
        }
        case EXPRESSION_ARRAY_LITERAL:
        {
            ApePtrArray_t* valuescopy = ape_ptrarray_copywithitems(expr->array, (ApeDataCallback_t)ape_ast_copy_expr, (ApeDataCallback_t)ape_ast_destroy_expr);
            if(!valuescopy)
            {
                return NULL;
            }
            res = ape_ast_make_arrayliteralexpr(expr->context, valuescopy);
            if(!res)
            {
                ape_ptrarray_destroywithitems(valuescopy, (ApeDataCallback_t)ape_ast_destroy_expr);
                return NULL;
            }
            break;
        }
        case EXPRESSION_MAP_LITERAL:
        {
            ApePtrArray_t* keyscopy = ape_ptrarray_copywithitems(expr->map.keys, (ApeDataCallback_t)ape_ast_copy_expr, (ApeDataCallback_t)ape_ast_destroy_expr);
            ApePtrArray_t* valuescopy = ape_ptrarray_copywithitems(expr->map.values, (ApeDataCallback_t)ape_ast_copy_expr, (ApeDataCallback_t)ape_ast_destroy_expr);
            if(!keyscopy || !valuescopy)
            {
                ape_ptrarray_destroywithitems(keyscopy, (ApeDataCallback_t)ape_ast_destroy_expr);
                ape_ptrarray_destroywithitems(valuescopy, (ApeDataCallback_t)ape_ast_destroy_expr);
                return NULL;
            }
            res = ape_ast_make_mapliteralexpr(expr->context, keyscopy, valuescopy);
            if(!res)
            {
                ape_ptrarray_destroywithitems(keyscopy, (ApeDataCallback_t)ape_ast_destroy_expr);
                ape_ptrarray_destroywithitems(valuescopy, (ApeDataCallback_t)ape_ast_destroy_expr);
                return NULL;
            }
            break;
        }
        case EXPRESSION_PREFIX:
        {
            ApeExpression_t* rightcopy = ape_ast_copy_expr(expr->prefix.right);
            if(!rightcopy)
            {
                return NULL;
            }
            res = ape_ast_make_prefixexpr(expr->context, expr->prefix.op, rightcopy);
            if(!res)
            {
                ape_ast_destroy_expr(rightcopy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_INFIX:
        {
            ApeExpression_t* leftcopy = ape_ast_copy_expr(expr->infix.left);
            ApeExpression_t* rightcopy = ape_ast_copy_expr(expr->infix.right);
            if(!leftcopy || !rightcopy)
            {
                ape_ast_destroy_expr(leftcopy);
                ape_ast_destroy_expr(rightcopy);
                return NULL;
            }
            res = ape_ast_make_infixexpr(expr->context, expr->infix.op, leftcopy, rightcopy);
            if(!res)
            {
                ape_ast_destroy_expr(leftcopy);
                ape_ast_destroy_expr(rightcopy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_FUNCTION_LITERAL:
        {
            ApePtrArray_t* paramscopy = ape_ptrarray_copywithitems(expr->fnliteral.params, (ApeDataCallback_t)ape_ast_copy_ident,(ApeDataCallback_t) ape_ast_destroy_ident);
            ApeCodeblock_t* bodycopy = ape_ast_copy_codeblock(expr->fnliteral.body);
            char* namecopy = ape_util_strdup(expr->context, expr->fnliteral.name);
            if(!paramscopy || !bodycopy)
            {
                ape_ptrarray_destroywithitems(paramscopy, (ApeDataCallback_t)ape_ast_destroy_ident);
                ape_ast_destroy_codeblock(bodycopy);
                allocator_free(expr->alloc, namecopy);
                return NULL;
            }
            res = ape_ast_make_fnliteralexpr(expr->context, paramscopy, bodycopy);
            if(!res)
            {
                ape_ptrarray_destroywithitems(paramscopy, (ApeDataCallback_t)ape_ast_destroy_ident);
                ape_ast_destroy_codeblock(bodycopy);
                allocator_free(expr->alloc, namecopy);
                return NULL;
            }
            res->fnliteral.name = namecopy;
            break;
        }
        case EXPRESSION_CALL:
        {
            ApeExpression_t* functioncopy = ape_ast_copy_expr(expr->callexpr.function);
            ApePtrArray_t* argscopy = ape_ptrarray_copywithitems(expr->callexpr.args, (ApeDataCallback_t)ape_ast_copy_expr, (ApeDataCallback_t)ape_ast_destroy_expr);
            if(!functioncopy || !argscopy)
            {
                ape_ast_destroy_expr(functioncopy);
                ape_ptrarray_destroywithitems(expr->callexpr.args, (ApeDataCallback_t)ape_ast_destroy_expr);
                return NULL;
            }
            res = ape_ast_make_callexpr(expr->context, functioncopy, argscopy);
            if(!res)
            {
                ape_ast_destroy_expr(functioncopy);
                ape_ptrarray_destroywithitems(expr->callexpr.args, (ApeDataCallback_t)ape_ast_destroy_expr);
                return NULL;
            }
            break;
        }
        case EXPRESSION_INDEX:
        {
            ApeExpression_t* leftcopy = ape_ast_copy_expr(expr->indexexpr.left);
            ApeExpression_t* indexcopy = ape_ast_copy_expr(expr->indexexpr.index);
            if(!leftcopy || !indexcopy)
            {
                ape_ast_destroy_expr(leftcopy);
                ape_ast_destroy_expr(indexcopy);
                return NULL;
            }
            res = ape_ast_make_indexexpr(expr->context, leftcopy, indexcopy);
            if(!res)
            {
                ape_ast_destroy_expr(leftcopy);
                ape_ast_destroy_expr(indexcopy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_ASSIGN:
        {
            ApeExpression_t* destcopy = ape_ast_copy_expr(expr->assign.dest);
            ApeExpression_t* sourcecopy = ape_ast_copy_expr(expr->assign.source);
            if(!destcopy || !sourcecopy)
            {
                ape_ast_destroy_expr(destcopy);
                ape_ast_destroy_expr(sourcecopy);
                return NULL;
            }
            res = ape_ast_make_assignexpr(expr->context, destcopy, sourcecopy, expr->assign.ispostfix);
            if(!res)
            {
                ape_ast_destroy_expr(destcopy);
                ape_ast_destroy_expr(sourcecopy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_LOGICAL:
        {
            ApeExpression_t* leftcopy = ape_ast_copy_expr(expr->logical.left);
            ApeExpression_t* rightcopy = ape_ast_copy_expr(expr->logical.right);
            if(!leftcopy || !rightcopy)
            {
                ape_ast_destroy_expr(leftcopy);
                ape_ast_destroy_expr(rightcopy);
                return NULL;
            }
            res = ape_ast_make_logicalexpr(expr->context, expr->logical.op, leftcopy, rightcopy);
            if(!res)
            {
                ape_ast_destroy_expr(leftcopy);
                ape_ast_destroy_expr(rightcopy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_TERNARY:
        {
            ApeExpression_t* testcopy = ape_ast_copy_expr(expr->ternary.test);
            ApeExpression_t* iftruecopy = ape_ast_copy_expr(expr->ternary.iftrue);
            ApeExpression_t* iffalsecopy = ape_ast_copy_expr(expr->ternary.iffalse);
            if(!testcopy || !iftruecopy || !iffalsecopy)
            {
                ape_ast_destroy_expr(testcopy);
                ape_ast_destroy_expr(iftruecopy);
                ape_ast_destroy_expr(iffalsecopy);
                return NULL;
            }
            res = ape_ast_make_ternaryexpr(expr->context, testcopy, iftruecopy, iffalsecopy);
            if(!res)
            {
                ape_ast_destroy_expr(testcopy);
                ape_ast_destroy_expr(iftruecopy);
                ape_ast_destroy_expr(iffalsecopy);
                return NULL;
            }
            break;
        }
    }
    if(!res)
    {
        return NULL;
    }
    res->pos = expr->pos;
    return res;
}

ApeStatement_t* ape_ast_make_definestmt(ApeContext_t* ctx, ApeIdent_t* name, ApeExpression_t* value, bool assignable)
{
    ApeStatement_t* res = ape_ast_make_statement(ctx, STATEMENT_DEFINE);
    if(!res)
    {
        return NULL;
    }
    res->define.name = name;
    res->define.value = value;
    res->define.assignable = assignable;
    return res;
}

ApeStatement_t* ape_ast_make_ifstmt(ApeContext_t* ctx, ApePtrArray_t * cases, ApeCodeblock_t* alternative)
{
    ApeStatement_t* res = ape_ast_make_statement(ctx, STATEMENT_IF);
    if(!res)
    {
        return NULL;
    }
    res->ifstatement.cases = cases;
    res->ifstatement.alternative = alternative;
    return res;
}

ApeStatement_t* ape_ast_make_returnstmt(ApeContext_t* ctx, ApeExpression_t* value)
{
    ApeStatement_t* res = ape_ast_make_statement(ctx, STATEMENT_RETURN_VALUE);
    if(!res)
    {
        return NULL;
    }
    res->returnvalue = value;
    return res;
}

ApeStatement_t* ape_ast_make_expressionstmt(ApeContext_t* ctx, ApeExpression_t* value)
{
    ApeStatement_t* res = ape_ast_make_statement(ctx, STATEMENT_EXPRESSION);
    if(!res)
    {
        return NULL;
    }
    res->expression = value;
    return res;
}

ApeStatement_t* ape_ast_make_whileloopstmt(ApeContext_t* ctx, ApeExpression_t* test, ApeCodeblock_t* body)
{
    ApeStatement_t* res = ape_ast_make_statement(ctx, STATEMENT_WHILE_LOOP);
    if(!res)
    {
        return NULL;
    }
    res->whileloop.test = test;
    res->whileloop.body = body;
    return res;
}

ApeStatement_t* ape_ast_make_breakstmt(ApeContext_t* ctx)
{
    ApeStatement_t* res = ape_ast_make_statement(ctx, STATEMENT_BREAK);
    if(!res)
    {
        return NULL;
    }
    return res;
}

ApeStatement_t* ape_ast_make_foreachstmt(ApeContext_t* ctx, ApeIdent_t* iterator, ApeExpression_t* source, ApeCodeblock_t* body)
{
    ApeStatement_t* res = ape_ast_make_statement(ctx, STATEMENT_FOREACH);
    if(!res)
    {
        return NULL;
    }
    res->foreach.iterator = iterator;
    res->foreach.source = source;
    res->foreach.body = body;
    return res;
}

ApeStatement_t* ape_ast_make_forloopstmt(ApeContext_t* ctx, ApeStatement_t* init, ApeExpression_t* test, ApeExpression_t* update, ApeCodeblock_t* body)
{
    ApeStatement_t* res = ape_ast_make_statement(ctx, STATEMENT_FOR_LOOP);
    if(!res)
    {
        return NULL;
    }
    res->forloop.init = init;
    res->forloop.test = test;
    res->forloop.update = update;
    res->forloop.body = body;
    return res;
}

ApeStatement_t* ape_ast_make_continuestmt(ApeContext_t* ctx)
{
    ApeStatement_t* res = ape_ast_make_statement(ctx, STATEMENT_CONTINUE);
    if(!res)
    {
        return NULL;
    }
    return res;
}

ApeStatement_t* ape_ast_make_blockstmt(ApeContext_t* ctx, ApeCodeblock_t* block)
{
    ApeStatement_t* res = ape_ast_make_statement(ctx, STATEMENT_BLOCK);
    if(!res)
    {
        return NULL;
    }
    res->block = block;
    return res;
}

ApeStatement_t* ape_ast_make_importstmt(ApeContext_t* ctx, char* path)
{
    ApeStatement_t* res = ape_ast_make_statement(ctx, STATEMENT_IMPORT);
    if(!res)
    {
        return NULL;
    }
    res->import.path = path;
    return res;
}

ApeStatement_t* ape_ast_make_recoverstmt(ApeContext_t* ctx, ApeIdent_t* errorident, ApeCodeblock_t* body)
{
    ApeStatement_t* res = ape_ast_make_statement(ctx, STATEMENT_RECOVER);
    if(!res)
    {
        return NULL;
    }
    res->recover.errorident = errorident;
    res->recover.body = body;
    return res;
}

void* ape_ast_destroy_stmt(ApeStatement_t* stmt)
{
    if(!stmt)
    {
        return NULL;
    }
    switch(stmt->type)
    {
        case STATEMENT_NONE:
            {
                APE_ASSERT(false);
            }
            break;
        case STATEMENT_DEFINE:
            {
                ape_ast_destroy_ident(stmt->define.name);
                ape_ast_destroy_expr(stmt->define.value);
            }
            break;
        case STATEMENT_IF:
            {
                ape_ptrarray_destroywithitems(stmt->ifstatement.cases, (ApeDataCallback_t)ape_ast_destroy_ifcase);
                ape_ast_destroy_codeblock(stmt->ifstatement.alternative);
            }
            break;
        case STATEMENT_RETURN_VALUE:
            {
                ape_ast_destroy_expr(stmt->returnvalue);
            }
            break;
        case STATEMENT_EXPRESSION:
            {
                ape_ast_destroy_expr(stmt->expression);
            }
            break;
        case STATEMENT_WHILE_LOOP:
            {
                ape_ast_destroy_expr(stmt->whileloop.test);
                ape_ast_destroy_codeblock(stmt->whileloop.body);
            }
            break;
        case STATEMENT_BREAK:
            {
            }
            break;
        case STATEMENT_CONTINUE:
            {
            }
            break;
        case STATEMENT_FOREACH:
            {
                ape_ast_destroy_ident(stmt->foreach.iterator);
                ape_ast_destroy_expr(stmt->foreach.source);
                ape_ast_destroy_codeblock(stmt->foreach.body);
            }
            break;
        case STATEMENT_FOR_LOOP:
            {
                ape_ast_destroy_stmt(stmt->forloop.init);
                ape_ast_destroy_expr(stmt->forloop.test);
                ape_ast_destroy_expr(stmt->forloop.update);
                ape_ast_destroy_codeblock(stmt->forloop.body);
            }
            break;
        case STATEMENT_BLOCK:
            {
                ape_ast_destroy_codeblock(stmt->block);
            }
            break;
        case STATEMENT_IMPORT:
            {
                allocator_free(stmt->alloc, stmt->import.path);
            }
            break;
        case STATEMENT_RECOVER:
            {
                ape_ast_destroy_codeblock(stmt->recover.body);
                ape_ast_destroy_ident(stmt->recover.errorident);
            }
            break;
    }
    allocator_free(stmt->alloc, stmt);
    return NULL;
}

ApeStatement_t* ape_ast_copy_stmt(const ApeStatement_t* stmt)
{
    if(!stmt)
    {
        return NULL;
    }
    ApeStatement_t* res = NULL;
    switch(stmt->type)
    {
        case STATEMENT_NONE:
            {
                APE_ASSERT(false);
            }
            break;
        case STATEMENT_DEFINE:
            {
                ApeExpression_t* valuecopy = ape_ast_copy_expr(stmt->define.value);
                if(!valuecopy)
                {
                    return NULL;
                }
                res = ape_ast_make_definestmt(stmt->context, ape_ast_copy_ident(stmt->define.name), valuecopy, stmt->define.assignable);
                if(!res)
                {
                    ape_ast_destroy_expr(valuecopy);
                    return NULL;
                }
            }
            break;
        case STATEMENT_IF:
            {
                ApePtrArray_t* casescopy = ape_ptrarray_copywithitems(stmt->ifstatement.cases, (ApeDataCallback_t)ape_ast_copy_ifcase, (ApeDataCallback_t)ape_ast_destroy_ifcase);
                ApeCodeblock_t* alternativecopy = ape_ast_copy_codeblock(stmt->ifstatement.alternative);
                if(!casescopy || !alternativecopy)
                {
                    ape_ptrarray_destroywithitems(casescopy, (ApeDataCallback_t)ape_ast_destroy_ifcase);
                    ape_ast_destroy_codeblock(alternativecopy);
                    return NULL;
                }
                res = ape_ast_make_ifstmt(stmt->context, casescopy, alternativecopy);
                if(res)
                {
                    ape_ptrarray_destroywithitems(casescopy, (ApeDataCallback_t)ape_ast_destroy_ifcase);
                    ape_ast_destroy_codeblock(alternativecopy);
                    return NULL;
                }
            }
            break;
        case STATEMENT_RETURN_VALUE:
            {
                ApeExpression_t* valuecopy = ape_ast_copy_expr(stmt->returnvalue);
                if(!valuecopy)
                {
                    return NULL;
                }
                res = ape_ast_make_returnstmt(stmt->context, valuecopy);
                if(!res)
                {
                    ape_ast_destroy_expr(valuecopy);
                    return NULL;
                }
            }
            break;
        case STATEMENT_EXPRESSION:
            {
                ApeExpression_t* valuecopy = ape_ast_copy_expr(stmt->expression);
                if(!valuecopy)
                {
                    return NULL;
                }
                res = ape_ast_make_expressionstmt(stmt->context, valuecopy);
                if(!res)
                {
                    ape_ast_destroy_expr(valuecopy);
                    return NULL;
                }
            }
            break;
        case STATEMENT_WHILE_LOOP:
            {
                ApeExpression_t* testcopy = ape_ast_copy_expr(stmt->whileloop.test);
                ApeCodeblock_t* bodycopy = ape_ast_copy_codeblock(stmt->whileloop.body);
                if(!testcopy || !bodycopy)
                {
                    ape_ast_destroy_expr(testcopy);
                    ape_ast_destroy_codeblock(bodycopy);
                    return NULL;
                }
                res = ape_ast_make_whileloopstmt(stmt->context, testcopy, bodycopy);
                if(!res)
                {
                    ape_ast_destroy_expr(testcopy);
                    ape_ast_destroy_codeblock(bodycopy);
                    return NULL;
                }
            }
            break;
        case STATEMENT_BREAK:
            {
                res = ape_ast_make_breakstmt(stmt->context);
            }
            break;
        case STATEMENT_CONTINUE:
            {
                res = ape_ast_make_continuestmt(stmt->context);
            }
            break;
        case STATEMENT_FOREACH:
            {
                ApeExpression_t* sourcecopy = ape_ast_copy_expr(stmt->foreach.source);
                ApeCodeblock_t* bodycopy = ape_ast_copy_codeblock(stmt->foreach.body);
                if(!sourcecopy || !bodycopy)
                {
                    ape_ast_destroy_expr(sourcecopy);
                    ape_ast_destroy_codeblock(bodycopy);
                    return NULL;
                }
                res = ape_ast_make_foreachstmt(stmt->context, ape_ast_copy_ident(stmt->foreach.iterator), sourcecopy, bodycopy);
                if(!res)
                {
                    ape_ast_destroy_expr(sourcecopy);
                    ape_ast_destroy_codeblock(bodycopy);
                    return NULL;
                }
            }
            break;
        case STATEMENT_FOR_LOOP:
            {
                ApeStatement_t* initcopy = ape_ast_copy_stmt(stmt->forloop.init);
                ApeExpression_t* testcopy = ape_ast_copy_expr(stmt->forloop.test);
                ApeExpression_t* updatecopy = ape_ast_copy_expr(stmt->forloop.update);
                ApeCodeblock_t* bodycopy = ape_ast_copy_codeblock(stmt->forloop.body);
                if(!initcopy || !testcopy || !updatecopy || !bodycopy)
                {
                    ape_ast_destroy_stmt(initcopy);
                    ape_ast_destroy_expr(testcopy);
                    ape_ast_destroy_expr(updatecopy);
                    ape_ast_destroy_codeblock(bodycopy);
                    return NULL;
                }
                res = ape_ast_make_forloopstmt(stmt->context, initcopy, testcopy, updatecopy, bodycopy);
                if(!res)
                {
                    ape_ast_destroy_stmt(initcopy);
                    ape_ast_destroy_expr(testcopy);
                    ape_ast_destroy_expr(updatecopy);
                    ape_ast_destroy_codeblock(bodycopy);
                    return NULL;
                }
            }
            break;
        case STATEMENT_BLOCK:
            {
                ApeCodeblock_t* block_copy = ape_ast_copy_codeblock(stmt->block);
                if(!block_copy)
                {
                    return NULL;
                }
                res = ape_ast_make_blockstmt(stmt->context, block_copy);
                if(!res)
                {
                    ape_ast_destroy_codeblock(block_copy);
                    return NULL;
                }
            }
            break;
        case STATEMENT_IMPORT:
            {
                char* pathcopy = ape_util_strdup(stmt->context, stmt->import.path);
                if(!pathcopy)
                {
                    return NULL;
                }
                res = ape_ast_make_importstmt(stmt->context, pathcopy);
                if(!res)
                {
                    allocator_free(stmt->alloc, pathcopy);
                    return NULL;
                }
            }
            break;
        case STATEMENT_RECOVER:
            {
                ApeCodeblock_t* bodycopy = ape_ast_copy_codeblock(stmt->recover.body);
                ApeIdent_t* erroridentcopy = ape_ast_copy_ident(stmt->recover.errorident);
                if(!bodycopy || !erroridentcopy)
                {
                    ape_ast_destroy_codeblock(bodycopy);
                    ape_ast_destroy_ident(erroridentcopy);
                    return NULL;
                }
                res = ape_ast_make_recoverstmt(stmt->context, erroridentcopy, bodycopy);
                if(!res)
                {
                    ape_ast_destroy_codeblock(bodycopy);
                    ape_ast_destroy_ident(erroridentcopy);
                    return NULL;
                }
            }
            break;

    }
    if(!res)
    {
        return NULL;
    }
    res->pos = stmt->pos;
    return res;
}

ApeCodeblock_t* ape_ast_make_code_block(ApeAllocator_t* alloc, ApePtrArray_t * statements)
{
    ApeCodeblock_t* block;
    block = (ApeCodeblock_t*)allocator_malloc(alloc, sizeof(ApeCodeblock_t));
    if(!block)
    {
        return NULL;
    }
    block->alloc = alloc;
    block->statements = statements;
    return block;
}

void* ape_ast_destroy_codeblock(ApeCodeblock_t* block)
{
    if(!block)
    {
        return NULL;
    }
    ape_ptrarray_destroywithitems(block->statements, (ApeDataCallback_t)ape_ast_destroy_stmt);
    allocator_free(block->alloc, block);
    return NULL;
}

ApeCodeblock_t* ape_ast_copy_codeblock(ApeCodeblock_t* block)
{
    ApeCodeblock_t* res;
    ApePtrArray_t* statementscopy;
    if(!block)
    {
        return NULL;
    }
    statementscopy = ape_ptrarray_copywithitems(block->statements, (ApeDataCallback_t)ape_ast_copy_stmt, (ApeDataCallback_t)ape_ast_destroy_stmt);
    if(!statementscopy)
    {
        return NULL;
    }
    res = ape_ast_make_code_block(block->alloc, statementscopy);
    if(!res)
    {
        ape_ptrarray_destroywithitems(statementscopy, (ApeDataCallback_t)ape_ast_destroy_stmt);
        return NULL;
    }
    return res;
}

void* ape_ast_destroy_ident(ApeIdent_t* ident)
{
    if(!ident)
    {
        return NULL;
    }
    allocator_free(ident->alloc, ident->value);
    ident->value = NULL;
    ident->pos = gprsprivsrcposinvalid;
    allocator_free(ident->alloc, ident);
    return NULL;
}

ApeIfCase_t* ape_ast_make_if_case(ApeAllocator_t* alloc, ApeExpression_t* test, ApeCodeblock_t* consequence)
{
    ApeIfCase_t* res;
    res = (ApeIfCase_t*)allocator_malloc(alloc, sizeof(ApeIfCase_t));
    if(!res)
    {
        return NULL;
    }
    res->alloc = alloc;
    res->test = test;
    res->consequence = consequence;
    return res;
}

void* ape_ast_destroy_ifcase(ApeIfCase_t* cond)
{
    if(!cond)
    {
        return NULL;
    }
    ape_ast_destroy_expr(cond->test);
    ape_ast_destroy_codeblock(cond->consequence);
    allocator_free(cond->alloc, cond);
    return NULL;
}

ApeIfCase_t* ape_ast_copy_ifcase(ApeIfCase_t* ifcase)
{
    ApeExpression_t* testcopy;
    ApeCodeblock_t* consequencecopy;
    ApeIfCase_t* ifcasecopy;
    if(!ifcase)
    {
        return NULL;
    }
    testcopy = NULL;
    consequencecopy = NULL;
    ifcasecopy = NULL;
    testcopy = ape_ast_copy_expr(ifcase->test);
    if(!testcopy)
    {
        goto err;
    }
    consequencecopy = ape_ast_copy_codeblock(ifcase->consequence);
    if(!testcopy || !consequencecopy)
    {
        goto err;
    }
    ifcasecopy = ape_ast_make_if_case(ifcase->alloc, testcopy, consequencecopy);
    if(!ifcasecopy)
    {
        goto err;
    }
    return ifcasecopy;
err:
    ape_ast_destroy_expr(testcopy);
    ape_ast_destroy_codeblock(consequencecopy);
    ape_ast_destroy_ifcase(ifcasecopy);
    return NULL;
}

// INTERNAL
ApeExpression_t* ape_ast_make_expression(ApeContext_t* ctx, ApeExprType_t type)
{
    ApeExpression_t* res;
    res = (ApeExpression_t*)allocator_malloc(&ctx->alloc, sizeof(ApeExpression_t));
    if(!res)
    {
        return NULL;
    }
    res->context = ctx;
    res->alloc = &ctx->alloc;
    res->type = type;
    res->pos = gprsprivsrcposinvalid;
    return res;
}

ApeStatement_t* ape_ast_make_statement(ApeContext_t* ctx, ApeStatementType_t type)
{
    ApeStatement_t* res = (ApeStatement_t*)allocator_malloc(&ctx->alloc, sizeof(ApeStatement_t));
    if(!res)
    {
        return NULL;
    }
    res->context = ctx;
    res->alloc = &ctx->alloc;
    res->type = type;
    res->pos = gprsprivsrcposinvalid;
    return res;
}

ApeParser_t* ape_ast_make_parser(ApeContext_t* ctx, const ApeConfig_t* config, ApeErrorList_t* errors)
{
    ApeParser_t* parser;
    parser = (ApeParser_t*)allocator_malloc(&ctx->alloc, sizeof(ApeParser_t));
    if(!parser)
    {
        return NULL;
    }
    memset(parser, 0, sizeof(ApeParser_t));
    parser->context = ctx;
    parser->alloc = &ctx->alloc;
    parser->config = config;
    parser->errors = errors;
    {
        parser->rightassocparsefns[TOKEN_IDENT] = ape_parser_parseident;
        parser->rightassocparsefns[TOKEN_NUMBER] = ape_parser_parseliteralnumber;
        parser->rightassocparsefns[TOKEN_TRUE] = ape_parser_parseliteralbool;
        parser->rightassocparsefns[TOKEN_FALSE] = ape_parser_parseliteralbool;
        parser->rightassocparsefns[TOKEN_STRING] = ape_parser_parseliteralstring;
        parser->rightassocparsefns[TOKEN_TEMPLATE_STRING] = ape_parser_parseliteraltplstring;
        parser->rightassocparsefns[TOKEN_NULL] = ape_parser_parseliteralnull;
        parser->rightassocparsefns[TOKEN_BANG] = ape_parser_parseprefixexpr;
        parser->rightassocparsefns[TOKEN_MINUS] = ape_parser_parseprefixexpr;
        parser->rightassocparsefns[TOKEN_LPAREN] = ape_parser_parsegroupedexpr;
        parser->rightassocparsefns[TOKEN_FUNCTION] = ape_parser_parseliteralfunc;
        parser->rightassocparsefns[TOKEN_LBRACKET] = ape_parser_parseliteralarray;
        parser->rightassocparsefns[TOKEN_LBRACE] = ape_parser_parseliteralmap;
        parser->rightassocparsefns[TOKEN_PLUS_PLUS] = ape_parser_parseincdecprefixexpr;
        parser->rightassocparsefns[TOKEN_MINUS_MINUS] = ape_parser_parseincdecprefixexpr;
    }
    {
        parser->leftassocparsefns[TOKEN_PLUS] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_MINUS] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_SLASH] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_ASTERISK] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_PERCENT] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_EQ] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_NOT_EQ] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_LT] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_LTE] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_GT] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_GTE] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_LPAREN] = ape_parser_parsecallexpr;
        parser->leftassocparsefns[TOKEN_LBRACKET] = ape_parser_parseindexexpr;
        parser->leftassocparsefns[TOKEN_ASSIGN] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_PLUS_ASSIGN] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_MINUS_ASSIGN] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_SLASH_ASSIGN] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_ASTERISK_ASSIGN] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_PERCENT_ASSIGN] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_BIT_AND_ASSIGN] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_BIT_OR_ASSIGN] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_BIT_XOR_ASSIGN] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_LSHIFT_ASSIGN] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_RSHIFT_ASSIGN] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_DOT] = ape_parser_parsedotexpr;
        parser->leftassocparsefns[TOKEN_AND] = ape_parser_parselogicalexpr;
        parser->leftassocparsefns[TOKEN_OR] = ape_parser_parselogicalexpr;
        parser->leftassocparsefns[TOKEN_BIT_AND] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_BIT_OR] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_BIT_XOR] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_LSHIFT] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_RSHIFT] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_QUESTION] = ape_parser_parseternaryexpr;
        parser->leftassocparsefns[TOKEN_PLUS_PLUS] = ape_parser_parseincdecpostfixexpr;
        parser->leftassocparsefns[TOKEN_MINUS_MINUS] = ape_parser_parseincdecpostfixexpr;
    }
    parser->depth = 0;
    return parser;
}

void ape_parser_destroy(ApeParser_t* parser)
{
    if(!parser)
    {
        return;
    }
    allocator_free(parser->alloc, parser);
}

ApePtrArray_t * ape_parser_parseall(ApeParser_t* parser, const char* input, ApeCompiledFile_t* file)
{
    bool ok;
    ApePtrArray_t* statements;
    ApeStatement_t* stmt;
    parser->depth = 0;
    ok = ape_lexer_init(&parser->lexer, parser->context, parser->errors, input, file);
    if(!ok)
    {
        return NULL;
    }
    ape_lexer_nexttoken(&parser->lexer);
    ape_lexer_nexttoken(&parser->lexer);
    statements = ape_make_ptrarray(parser->context);
    if(!statements)
    {
        return NULL;
    }
    while(!ape_lexer_currenttokenis(&parser->lexer, TOKEN_EOF))
    {
        if(ape_lexer_currenttokenis(&parser->lexer, TOKEN_SEMICOLON))
        {
            ape_lexer_nexttoken(&parser->lexer);
            continue;
        }
        stmt = ape_parser_parsestmt(parser);
        if(!stmt)
        {
            goto err;
        }
        ok = ape_ptrarray_add(statements, stmt);
        if(!ok)
        {
            ape_ast_destroy_stmt(stmt);
            goto err;
        }
    }
    if(errorlist_count(parser->errors) > 0)
    {
        goto err;
    }
    return statements;
err:
    ape_ptrarray_destroywithitems(statements, (ApeDataCallback_t)ape_ast_destroy_stmt);
    return NULL;
}

// INTERNAL
ApeStatement_t* ape_parser_parsestmt(ApeParser_t* p)
{
    ApePosition_t pos;
    ApeStatement_t* res;
    pos = p->lexer.curtoken.pos;
    res = NULL;
    switch(p->lexer.curtoken.type)
    {
        case TOKEN_VAR:
        case TOKEN_CONST:
            {
                res = ape_parser_parsedefinestmt(p);
            }
            break;
        case TOKEN_IF:
            {
                res = ape_parser_parseifstmt(p);
            }
            break;
        case TOKEN_RETURN:
            {
                res = ape_parser_parsereturnstmt(p);
            }
            break;
        case TOKEN_WHILE:
            {
                res = ape_parser_parsewhileloopstmt(p);
            }
            break;
        case TOKEN_BREAK:
            {
                res = ape_parser_parsebreakstmt(p);
            }
            break;
        case TOKEN_FOR:
            {
                res = ape_parser_parseforloopstmt(p);
            }
            break;
        case TOKEN_FUNCTION:
            {
                if(ape_lexer_peektokenis(&p->lexer, TOKEN_IDENT))
                {
                    res = ape_parser_parsefuncstmt(p);
                }
                else
                {
                    res = ape_parser_parseexprstmt(p);
                }
            }
            break;
        case TOKEN_LBRACE:
            {
                if(p->config->replmode && p->depth == 0)
                {
                    res = ape_parser_parseexprstmt(p);
                }
                else
                {
                    res = ape_parser_parseblockstmt(p);
                }
            }
            break;
        case TOKEN_CONTINUE:
            {
                res = ape_parser_parsecontinuestmt(p);
            }
            break;
        case TOKEN_IMPORT:
            {
                res = ape_parser_parseimportstmt(p);
            }
            break;
        case TOKEN_RECOVER:
            {
                res = ape_parser_parserecoverstmt(p);
            }
            break;
        default:
            {
                res = ape_parser_parseexprstmt(p);
            }
            break;
    }
    if(res)
    {
        res->pos = pos;
    }
    return res;
}

ApeStatement_t* ape_parser_parsedefinestmt(ApeParser_t* p)
{
    bool assignable;
    ApeIdent_t* nameident;
    ApeExpression_t* value;
    ApeStatement_t* res;
    nameident = NULL;
    value = NULL;
    assignable = ape_lexer_currenttokenis(&p->lexer, TOKEN_VAR);
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_IDENT))
    {
        goto err;
    }
    nameident = ape_ast_make_ident(p->context, p->lexer.curtoken);
    if(!nameident)
    {
        goto err;
    }
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_ASSIGN))
    {
        goto err;
    }
    ape_lexer_nexttoken(&p->lexer);
    value = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
    if(!value)
    {
        goto err;
    }
    if(value->type == EXPRESSION_FUNCTION_LITERAL)
    {
        value->fnliteral.name = ape_util_strdup(p->context, nameident->value);
        if(!value->fnliteral.name)
        {
            goto err;
        }
    }
    res = ape_ast_make_definestmt(p->context, nameident, value, assignable);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    ape_ast_destroy_expr(value);
    ape_ast_destroy_ident(nameident);
    return NULL;
}

ApeStatement_t* ape_parser_parseifstmt(ApeParser_t* p)
{
    bool ok;
    ApePtrArray_t* cases;
    ApeCodeblock_t* alternative;
    ApeIfCase_t* elif;
    ApeStatement_t* res;
    ApeIfCase_t* cond;
    cases = NULL;
    alternative = NULL;
    cases = ape_make_ptrarray(p->context);
    if(!cases)
    {
        goto err;
    }
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_LPAREN))
    {
        goto err;
    }
    ape_lexer_nexttoken(&p->lexer);
    cond = ape_ast_make_if_case(p->alloc, NULL, NULL);
    if(!cond)
    {
        goto err;
    }
    ok = ape_ptrarray_add(cases, cond);
    if(!ok)
    {
        ape_ast_destroy_ifcase(cond);
        goto err;
    }
    cond->test = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
    if(!cond->test)
    {
        goto err;
    }
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_RPAREN))
    {
        goto err;
    }
    ape_lexer_nexttoken(&p->lexer);
    cond->consequence = ape_parser_parsecodeblock(p);
    if(!cond->consequence)
    {
        goto err;
    }
    while(ape_lexer_currenttokenis(&p->lexer, TOKEN_ELSE))
    {
        ape_lexer_nexttoken(&p->lexer);
        if(ape_lexer_currenttokenis(&p->lexer, TOKEN_IF))
        {
            ape_lexer_nexttoken(&p->lexer);
            if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_LPAREN))
            {
                goto err;
            }
            ape_lexer_nexttoken(&p->lexer);
            elif = ape_ast_make_if_case(p->alloc, NULL, NULL);
            if(!elif)
            {
                goto err;
            }
            ok = ape_ptrarray_add(cases, elif);
            if(!ok)
            {
                ape_ast_destroy_ifcase(elif);
                goto err;
            }
            elif->test = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
            if(!elif->test)
            {
                goto err;
            }
            if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_RPAREN))
            {
                goto err;
            }
            ape_lexer_nexttoken(&p->lexer);
            elif->consequence = ape_parser_parsecodeblock(p);
            if(!elif->consequence)
            {
                goto err;
            }
        }
        else
        {
            alternative = ape_parser_parsecodeblock(p);
            if(!alternative)
            {
                goto err;
            }
        }
    }
    res = ape_ast_make_ifstmt(p->context, cases, alternative);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    ape_ptrarray_destroywithitems(cases, (ApeDataCallback_t)ape_ast_destroy_ifcase);
    ape_ast_destroy_codeblock(alternative);
    return NULL;
}

ApeStatement_t* ape_parser_parsereturnstmt(ApeParser_t* p)
{
    ApeExpression_t* expr;
    ApeStatement_t* res;
    expr = NULL;
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_currenttokenis(&p->lexer, TOKEN_SEMICOLON) && !ape_lexer_currenttokenis(&p->lexer, TOKEN_RBRACE) && !ape_lexer_currenttokenis(&p->lexer, TOKEN_EOF))
    {
        expr = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
        if(!expr)
        {
            return NULL;
        }
    }
    res = ape_ast_make_returnstmt(p->context, expr);
    if(!res)
    {
        ape_ast_destroy_expr(expr);
        return NULL;
    }
    return res;
}

ApeStatement_t* ape_parser_parseexprstmt(ApeParser_t* p)
{
    ApeExpression_t* expr;
    ApeStatement_t* res;
    expr = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
    if(!expr)
    {
        return NULL;
    }
    #if 0
    if(expr && (!p->config->replmode || p->depth > 0))
    {
        if(expr->type != EXPRESSION_ASSIGN && expr->type != EXPRESSION_CALL)
        {
            errorlist_addformat(p->errors, APE_ERROR_PARSING, expr->pos, "only assignments and function calls can be expression statements");
            ape_ast_destroy_expr(expr);
            return NULL;
        }
    }
    #endif
    res = ape_ast_make_expressionstmt(p->context, expr);
    if(!res)
    {
        ape_ast_destroy_expr(expr);
        return NULL;
    }
    return res;
}

ApeStatement_t* ape_parser_parsewhileloopstmt(ApeParser_t* p)
{
    ApeExpression_t* test;
    ApeCodeblock_t* body;
    ApeStatement_t* res;
    test = NULL;
    body = NULL;
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_LPAREN))
    {
        goto err;
    }
    ape_lexer_nexttoken(&p->lexer);
    test = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
    if(!test)
    {
        goto err;
    }
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_RPAREN))
    {
        goto err;
    }
    ape_lexer_nexttoken(&p->lexer);
    body = ape_parser_parsecodeblock(p);
    if(!body)
    {
        goto err;
    }
    res = ape_ast_make_whileloopstmt(p->context, test, body);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    ape_ast_destroy_codeblock(body);
    ape_ast_destroy_expr(test);
    return NULL;
}

ApeStatement_t* ape_parser_parsebreakstmt(ApeParser_t* p)
{
    ape_lexer_nexttoken(&p->lexer);
    return ape_ast_make_breakstmt(p->context);
}

ApeStatement_t* ape_parser_parsecontinuestmt(ApeParser_t* p)
{
    ape_lexer_nexttoken(&p->lexer);
    return ape_ast_make_continuestmt(p->context);
}

ApeStatement_t* ape_parser_parseblockstmt(ApeParser_t* p)
{
    ApeCodeblock_t* block;
    ApeStatement_t* res;
    block = ape_parser_parsecodeblock(p);
    if(!block)
    {
        return NULL;
    }
    res = ape_ast_make_blockstmt(p->context, block);
    if(!res)
    {
        ape_ast_destroy_codeblock(block);
        return NULL;
    }
    return res;
}

ApeStatement_t* ape_parser_parseimportstmt(ApeParser_t* p)
{
    char* processedname;
    ApeStatement_t* res;
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_STRING))
    {
        return NULL;
    }
    processedname = ape_ast_processandcopystring(p->alloc, p->lexer.curtoken.literal, p->lexer.curtoken.len);
    if(!processedname)
    {
        errorlist_add(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "error when parsing module name");
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    res= ape_ast_make_importstmt(p->context, processedname);
    if(!res)
    {
        allocator_free(p->alloc, processedname);
        return NULL;
    }
    return res;
}

ApeStatement_t* ape_parser_parserecoverstmt(ApeParser_t* p)
{
    ApeIdent_t* errorident;
    ApeCodeblock_t* body;
    ApeStatement_t* res;
    errorident = NULL;
    body = NULL;
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_LPAREN))
    {
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_IDENT))
    {
        return NULL;
    }
    errorident = ape_ast_make_ident(p->context, p->lexer.curtoken);
    if(!errorident)
    {
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_RPAREN))
    {
        goto err;
    }
    ape_lexer_nexttoken(&p->lexer);
    body = ape_parser_parsecodeblock(p);
    if(!body)
    {
        goto err;
    }
    res = ape_ast_make_recoverstmt(p->context, errorident, body);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    ape_ast_destroy_codeblock(body);
    ape_ast_destroy_ident(errorident);
    return NULL;
}

ApeStatement_t* ape_parser_parseforloopstmt(ApeParser_t* p)
{
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_LPAREN))
    {
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    if(ape_lexer_currenttokenis(&p->lexer, TOKEN_IDENT) && ape_lexer_peektokenis(&p->lexer, TOKEN_IN))
    {
        return ape_parser_parseforeachstmt(p);
    }
    return ape_parser_parseclassicforstmt(p);
}

ApeStatement_t* ape_parser_parseforeachstmt(ApeParser_t* p)
{
    ApeExpression_t* source;
    ApeCodeblock_t* body;
    ApeIdent_t* iteratorident;
    ApeStatement_t* res;
    source = NULL;
    body = NULL;
    iteratorident = ape_ast_make_ident(p->context, p->lexer.curtoken);
    if(!iteratorident)
    {
        goto err;
    }
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_IN))
    {
        goto err;
    }
    ape_lexer_nexttoken(&p->lexer);
    source = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
    if(!source)
    {
        goto err;
    }
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_RPAREN))
    {
        goto err;
    }
    ape_lexer_nexttoken(&p->lexer);
    body = ape_parser_parsecodeblock(p);
    if(!body)
    {
        goto err;
    }
    res = ape_ast_make_foreachstmt(p->context, iteratorident, source, body);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    ape_ast_destroy_codeblock(body);
    ape_ast_destroy_ident(iteratorident);
    ape_ast_destroy_expr(source);
    return NULL;
}

ApeStatement_t* ape_parser_parseclassicforstmt(ApeParser_t* p)
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
    if(!ape_lexer_currenttokenis(&p->lexer, TOKEN_SEMICOLON))
    {
        init = ape_parser_parsestmt(p);
        if(!init)
        {
            goto err;
        }
        if(init->type != STATEMENT_DEFINE && init->type != STATEMENT_EXPRESSION)
        {
            errorlist_addformat(p->errors, APE_ERROR_PARSING, init->pos, "for loop's init clause should be a define statement or an expression");
            goto err;
        }
        if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_SEMICOLON))
        {
            goto err;
        }
    }
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_currenttokenis(&p->lexer, TOKEN_SEMICOLON))
    {
        test = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
        if(!test)
        {
            goto err;
        }
        if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_SEMICOLON))
        {
            goto err;
        }
    }
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_currenttokenis(&p->lexer, TOKEN_RPAREN))
    {
        update = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
        if(!update)
        {
            goto err;
        }
        if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_RPAREN))
        {
            goto err;
        }
    }
    ape_lexer_nexttoken(&p->lexer);
    body = ape_parser_parsecodeblock(p);
    if(!body)
    {
        goto err;
    }
    res = ape_ast_make_forloopstmt(p->context, init, test, update, body);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    ape_ast_destroy_stmt(init);
    ape_ast_destroy_expr(test);
    ape_ast_destroy_expr(update);
    ape_ast_destroy_codeblock(body);
    return NULL;
}

ApeStatement_t* ape_parser_parsefuncstmt(ApeParser_t* p)
{
    ApeIdent_t* nameident;
    ApeExpression_t* value;
    ApePosition_t pos;
    ApeStatement_t* res;
    value = NULL;
    nameident = NULL;
    pos = p->lexer.curtoken.pos;
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_IDENT))
    {
        goto err;
    }
    nameident = ape_ast_make_ident(p->context, p->lexer.curtoken);
    if(!nameident)
    {
        goto err;
    }
    ape_lexer_nexttoken(&p->lexer);
    value = ape_parser_parseliteralfunc(p);
    if(!value)
    {
        goto err;
    }
    value->pos = pos;
    value->fnliteral.name = ape_util_strdup(p->context, nameident->value);
    if(!value->fnliteral.name)
    {
        goto err;
    }
    res = ape_ast_make_definestmt(p->context, nameident, value, false);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    ape_ast_destroy_expr(value);
    ape_ast_destroy_ident(nameident);
    return NULL;
}

ApeCodeblock_t* ape_parser_parsecodeblock(ApeParser_t* p)
{
    bool ok;
    ApePtrArray_t* statements;
    ApeStatement_t* stmt;
    ApeCodeblock_t* res;
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_LBRACE))
    {
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    p->depth++;
    statements = ape_make_ptrarray(p->context);
    if(!statements)
    {
        goto err;
    }
    while(!ape_lexer_currenttokenis(&p->lexer, TOKEN_RBRACE))
    {
        if(ape_lexer_currenttokenis(&p->lexer, TOKEN_EOF))
        {
            errorlist_add(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "unexpected EOF");
            goto err;
        }
        if(ape_lexer_currenttokenis(&p->lexer, TOKEN_SEMICOLON))
        {
            ape_lexer_nexttoken(&p->lexer);
            continue;
        }
        stmt = ape_parser_parsestmt(p);
        if(!stmt)
        {
            goto err;
        }
        ok = ape_ptrarray_add(statements, stmt);
        if(!ok)
        {
            ape_ast_destroy_stmt(stmt);
            goto err;
        }
    }
    ape_lexer_nexttoken(&p->lexer);
    p->depth--;
    res = ape_ast_make_code_block(p->alloc, statements);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    p->depth--;
    ape_ptrarray_destroywithitems(statements, (ApeDataCallback_t)ape_ast_destroy_stmt);
    return NULL;
}

ApeExpression_t* ape_parser_parseexpr(ApeParser_t* p, ApePrecedence_t prec)
{
    char* literal;
    ApePosition_t pos;
    ApeRightAssocParseFNCallback_t prighta;
    ApeLeftAssocParseFNCallback_t plefta;
    ApeExpression_t* leftexpr;
    ApeExpression_t* newleftexpr;
    pos = p->lexer.curtoken.pos;
    if(p->lexer.curtoken.type == TOKEN_INVALID)
    {
        errorlist_add(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "illegal token");
        return NULL;
    }
    prighta = p->rightassocparsefns[p->lexer.curtoken.type];
    if(!prighta)
    {
        literal = ape_lexer_tokendupliteral(p->context, &p->lexer.curtoken);
        errorlist_addformat(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "no prefix parse function for '%s' found", literal);
        allocator_free(p->alloc, literal);
        return NULL;
    }
    leftexpr = prighta(p);
    if(!leftexpr)
    {
        return NULL;
    }
    leftexpr->pos = pos;
    while(!ape_lexer_currenttokenis(&p->lexer, TOKEN_SEMICOLON) && prec < ape_parser_getprecedence(p->lexer.curtoken.type))
    {
        plefta = p->leftassocparsefns[p->lexer.curtoken.type];
        if(!plefta)
        {
            return leftexpr;
        }
        pos = p->lexer.curtoken.pos;
        newleftexpr= plefta(p, leftexpr);
        if(!newleftexpr)
        {
            ape_ast_destroy_expr(leftexpr);
            return NULL;
        }
        newleftexpr->pos = pos;
        leftexpr = newleftexpr;
    }
    return leftexpr;
}

ApeExpression_t* ape_parser_parseident(ApeParser_t* p)
{
    ApeIdent_t* ident;
    ApeExpression_t* res;
    ident = ape_ast_make_ident(p->context, p->lexer.curtoken);
    if(!ident)
    {
        return NULL;
    }
    res = ape_ast_make_identexpr(p->context, ident);
    if(!res)
    {
        ape_ast_destroy_ident(ident);
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    return res;
}

ApeExpression_t* ape_parser_parseliteralnumber(ApeParser_t* p)
{
    char* end;
    char* literal;
    ApeFloat_t number;
    ApeInt_t parsedlen;
    number = 0;
    errno = 0;
    number = strtod(p->lexer.curtoken.literal, &end);
    parsedlen = end - p->lexer.curtoken.literal;
    if(errno || parsedlen != (ApeInt_t)p->lexer.curtoken.len)
    {
        literal = ape_lexer_tokendupliteral(p->context, &p->lexer.curtoken);
        errorlist_addformat(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "parsing number literal '%s' failed", literal);
        allocator_free(p->alloc, literal);
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    return ape_ast_make_numberliteralexpr(p->context, number);
}

ApeExpression_t* ape_parser_parseliteralbool(ApeParser_t* p)
{
    ApeExpression_t* res;
    res = ape_ast_make_boolliteralexpr(p->context, p->lexer.curtoken.type == TOKEN_TRUE);
    ape_lexer_nexttoken(&p->lexer);
    return res;
}

ApeExpression_t* ape_parser_parseliteralstring(ApeParser_t* p)
{
    char* processedliteral;
    ApeExpression_t* res;
    processedliteral = ape_ast_processandcopystring(p->alloc, p->lexer.curtoken.literal, p->lexer.curtoken.len);
    if(!processedliteral)
    {
        errorlist_add(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "error while parsing string literal");
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    res = ape_ast_make_stringliteralexpr(p->context, processedliteral);
    if(!res)
    {
        allocator_free(p->alloc, processedliteral);
        return NULL;
    }
    return res;
}

ApeExpression_t* ape_parser_parseliteraltplstring(ApeParser_t* p)
{
    char* processedliteral;
    ApeExpression_t* leftstringexpr;
    ApeExpression_t* templateexpr;
    ApeExpression_t* tostrcallexpr;
    ApeExpression_t* leftaddexpr;
    ApeExpression_t* rightexpr;
    ApeExpression_t* rightaddexpr;
    ApePosition_t pos;
    processedliteral = NULL;
    leftstringexpr = NULL;
    templateexpr = NULL;
    tostrcallexpr = NULL;
    leftaddexpr = NULL;
    rightexpr = NULL;
    rightaddexpr = NULL;
    processedliteral = ape_ast_processandcopystring(p->alloc, p->lexer.curtoken.literal, p->lexer.curtoken.len);
    if(!processedliteral)
    {
        errorlist_add(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "error while parsing string literal");
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);

    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_LBRACE))
    {
        goto err;
    }
    ape_lexer_nexttoken(&p->lexer);

    pos = p->lexer.curtoken.pos;

    leftstringexpr = ape_ast_make_stringliteralexpr(p->context, processedliteral);
    if(!leftstringexpr)
    {
        goto err;
    }
    leftstringexpr->pos = pos;
    processedliteral = NULL;
    pos = p->lexer.curtoken.pos;
    templateexpr = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
    if(!templateexpr)
    {
        goto err;
    }
    tostrcallexpr = ape_ast_wrapexprinfunccall(p->context, templateexpr, "tostring");
    if(!tostrcallexpr)
    {
        goto err;
    }
    tostrcallexpr->pos = pos;
    templateexpr = NULL;
    leftaddexpr = ape_ast_make_infixexpr(p->context, OPERATOR_PLUS, leftstringexpr, tostrcallexpr);
    if(!leftaddexpr)
    {
        goto err;
    }
    leftaddexpr->pos = pos;
    leftstringexpr = NULL;
    tostrcallexpr = NULL;
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_RBRACE))
    {
        goto err;
    }
    ape_lexer_previous_token(&p->lexer);
    ape_lexer_continuetemplatestring(&p->lexer);
    ape_lexer_nexttoken(&p->lexer);
    ape_lexer_nexttoken(&p->lexer);
    pos = p->lexer.curtoken.pos;
    rightexpr = ape_parser_parseexpr(p, PRECEDENCE_HIGHEST);
    if(!rightexpr)
    {
        goto err;
    }
    rightaddexpr = ape_ast_make_infixexpr(p->context, OPERATOR_PLUS, leftaddexpr, rightexpr);
    if(!rightaddexpr)
    {
        goto err;
    }
    rightaddexpr->pos = pos;
    leftaddexpr = NULL;
    rightexpr = NULL;

    return rightaddexpr;
err:
    ape_ast_destroy_expr(rightaddexpr);
    ape_ast_destroy_expr(rightexpr);
    ape_ast_destroy_expr(leftaddexpr);
    ape_ast_destroy_expr(tostrcallexpr);
    ape_ast_destroy_expr(templateexpr);
    ape_ast_destroy_expr(leftstringexpr);
    allocator_free(p->alloc, processedliteral);
    return NULL;
}

ApeExpression_t* ape_parser_parseliteralnull(ApeParser_t* p)
{
    ape_lexer_nexttoken(&p->lexer);
    return ape_ast_make_nullliteralexpr(p->context);
}

ApeExpression_t* ape_parser_parseliteralarray(ApeParser_t* p)
{
    ApePtrArray_t* array;
    ApeExpression_t* res;
    array = ape_parser_parseexprlist(p, TOKEN_LBRACKET, TOKEN_RBRACKET, true);
    if(!array)
    {
        return NULL;
    }
    res = ape_ast_make_arrayliteralexpr(p->context, array);
    if(!res)
    {
        ape_ptrarray_destroywithitems(array, (ApeDataCallback_t)ape_ast_destroy_expr);
        return NULL;
    }
    return res;
}

ApeExpression_t* ape_parser_parseliteralmap(ApeParser_t* p)
{
    bool ok;
    char* str;
    ApePtrArray_t* keys;
    ApePtrArray_t* values;
    ApeExpression_t* key;
    ApeExpression_t* value;
    ApeExpression_t* res;
    keys = ape_make_ptrarray(p->context);
    values = ape_make_ptrarray(p->context);
    if(!keys || !values)
    {
        goto err;
    }
    ape_lexer_nexttoken(&p->lexer);
    while(!ape_lexer_currenttokenis(&p->lexer, TOKEN_RBRACE))
    {
        key = NULL;
        if(ape_lexer_currenttokenis(&p->lexer, TOKEN_IDENT))
        {
            str = ape_lexer_tokendupliteral(p->context, &p->lexer.curtoken);
            key = ape_ast_make_stringliteralexpr(p->context, str);
            if(!key)
            {
                allocator_free(p->alloc, str);
                goto err;
            }
            key->pos = p->lexer.curtoken.pos;
            ape_lexer_nexttoken(&p->lexer);
        }
        else
        {
            key = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
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
                    }
                    break;
                default:
                    {
                        errorlist_addformat(p->errors, APE_ERROR_PARSING, key->pos, "invalid map literal key type");
                        ape_ast_destroy_expr(key);
                        goto err;
                    }
                    break;
            }
        }
        ok = ape_ptrarray_add(keys, key);
        if(!ok)
        {
            ape_ast_destroy_expr(key);
            goto err;
        }

        if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_COLON))
        {
            goto err;
        }
        ape_lexer_nexttoken(&p->lexer);
        value = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
        if(!value)
        {
            goto err;
        }
        ok = ape_ptrarray_add(values, value);
        if(!ok)
        {
            ape_ast_destroy_expr(value);
            goto err;
        }
        if(ape_lexer_currenttokenis(&p->lexer, TOKEN_RBRACE))
        {
            break;
        }
        if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_COMMA))
        {
            goto err;
        }
        ape_lexer_nexttoken(&p->lexer);
    }
    ape_lexer_nexttoken(&p->lexer);
    res = ape_ast_make_mapliteralexpr(p->context, keys, values);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    ape_ptrarray_destroywithitems(keys, (ApeDataCallback_t)ape_ast_destroy_expr);
    ape_ptrarray_destroywithitems(values, (ApeDataCallback_t)ape_ast_destroy_expr);
    return NULL;
}

ApeExpression_t* ape_parser_parseprefixexpr(ApeParser_t* p)
{
    ApeOperator_t op;
    ApeExpression_t* right;
    ApeExpression_t* res;
    op = ape_parser_tokentooperator(p->lexer.curtoken.type);
    ape_lexer_nexttoken(&p->lexer);
    right = ape_parser_parseexpr(p, PRECEDENCE_PREFIX);
    if(!right)
    {
        return NULL;
    }
    res = ape_ast_make_prefixexpr(p->context, op, right);
    if(!res)
    {
        ape_ast_destroy_expr(right);
        return NULL;
    }
    return res;
}

ApeExpression_t* ape_parser_parseinfixexpr(ApeParser_t* p, ApeExpression_t* left)
{
    ApeOperator_t op;
    ApePrecedence_t prec;
    ApeExpression_t* right;
    ApeExpression_t* res;
    op = ape_parser_tokentooperator(p->lexer.curtoken.type);
    prec = ape_parser_getprecedence(p->lexer.curtoken.type);
    ape_lexer_nexttoken(&p->lexer);
    right = ape_parser_parseexpr(p, prec);
    if(!right)
    {
        return NULL;
    }
    res = ape_ast_make_infixexpr(p->context, op, left, right);
    if(!res)
    {
        ape_ast_destroy_expr(right);
        return NULL;
    }
    return res;
}

ApeExpression_t* ape_parser_parsegroupedexpr(ApeParser_t* p)
{
    ApeExpression_t* expr;
    ape_lexer_nexttoken(&p->lexer);
    expr = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
    if(!expr || !ape_lexer_expectcurrent(&p->lexer, TOKEN_RPAREN))
    {
        ape_ast_destroy_expr(expr);
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    return expr;
}

ApeExpression_t* ape_parser_parseliteralfunc(ApeParser_t* p)
{
    bool ok;
    ApePtrArray_t* params;
    ApeCodeblock_t* body;
    ApeExpression_t* res;
    p->depth++;
    params = NULL;
    body = NULL;
    if(ape_lexer_currenttokenis(&p->lexer, TOKEN_FUNCTION))
    {
        ape_lexer_nexttoken(&p->lexer);
    }
    params = ape_make_ptrarray(p->context);
    ok = ape_parser_parsefuncparams(p, params);
    if(!ok)
    {
        goto err;
    }
    body = ape_parser_parsecodeblock(p);
    if(!body)
    {
        goto err;
    }
    res = ape_ast_make_fnliteralexpr(p->context, params, body);
    if(!res)
    {
        goto err;
    }
    p->depth -= 1;
    return res;
err:
    ape_ast_destroy_codeblock(body);
    ape_ptrarray_destroywithitems(params, (ApeDataCallback_t)ape_ast_destroy_ident);
    p->depth -= 1;
    return NULL;
}

bool ape_parser_parsefuncparams(ApeParser_t* p, ApePtrArray_t * outparams)
{
    bool ok;
    ApeIdent_t* ident;
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_LPAREN))
    {
        return false;
    }
    ape_lexer_nexttoken(&p->lexer);
    if(ape_lexer_currenttokenis(&p->lexer, TOKEN_RPAREN))
    {
        ape_lexer_nexttoken(&p->lexer);
        return true;
    }
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_IDENT))
    {
        return false;
    }
    ident = ape_ast_make_ident(p->context, p->lexer.curtoken);
    if(!ident)
    {
        return false;
    }
    ok = ape_ptrarray_add(outparams, ident);
    if(!ok)
    {
        ape_ast_destroy_ident(ident);
        return false;
    }
    ape_lexer_nexttoken(&p->lexer);
    while(ape_lexer_currenttokenis(&p->lexer, TOKEN_COMMA))
    {
        ape_lexer_nexttoken(&p->lexer);
        if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_IDENT))
        {
            return false;
        }
        ident = ape_ast_make_ident(p->context, p->lexer.curtoken);
        if(!ident)
        {
            return false;
        }
        ok = ape_ptrarray_add(outparams, ident);
        if(!ok)
        {
            ape_ast_destroy_ident(ident);
            return false;
        }
        ape_lexer_nexttoken(&p->lexer);
    }
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_RPAREN))
    {
        return false;
    }

    ape_lexer_nexttoken(&p->lexer);
    return true;
}

ApeExpression_t* ape_parser_parsecallexpr(ApeParser_t* p, ApeExpression_t* left)
{
    ApeExpression_t* function;
    ApePtrArray_t* args;
    ApeExpression_t* res;
    function = left;
    args = ape_parser_parseexprlist(p, TOKEN_LPAREN, TOKEN_RPAREN, false);
    if(!args)
    {
        return NULL;
    }
    res = ape_ast_make_callexpr(p->context, function, args);
    if(!res)
    {
        ape_ptrarray_destroywithitems(args, (ApeDataCallback_t)ape_ast_destroy_expr);
        return NULL;
    }
    return res;
}

ApePtrArray_t* ape_parser_parseexprlist(ApeParser_t* p, ApeTokenType_t starttoken, ApeTokenType_t endtoken, bool trailingcommaallowed)
{
    bool ok;
    ApePtrArray_t* res;
    ApeExpression_t* argexpr;
    if(!ape_lexer_expectcurrent(&p->lexer, starttoken))
    {
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    res = ape_make_ptrarray(p->context);
    if(ape_lexer_currenttokenis(&p->lexer, endtoken))
    {
        ape_lexer_nexttoken(&p->lexer);
        return res;
    }
    argexpr = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
    if(!argexpr)
    {
        goto err;
    }
    ok = ape_ptrarray_add(res, argexpr);
    if(!ok)
    {
        ape_ast_destroy_expr(argexpr);
        goto err;
    }
    while(ape_lexer_currenttokenis(&p->lexer, TOKEN_COMMA))
    {
        ape_lexer_nexttoken(&p->lexer);
        if(trailingcommaallowed && ape_lexer_currenttokenis(&p->lexer, endtoken))
        {
            break;
        }
        argexpr = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
        if(!argexpr)
        {
            goto err;
        }
        ok = ape_ptrarray_add(res, argexpr);
        if(!ok)
        {
            ape_ast_destroy_expr(argexpr);
            goto err;
        }
    }
    if(!ape_lexer_expectcurrent(&p->lexer, endtoken))
    {
        goto err;
    }
    ape_lexer_nexttoken(&p->lexer);
    return res;
err:
    ape_ptrarray_destroywithitems(res, (ApeDataCallback_t)ape_ast_destroy_expr);
    return NULL;
}

ApeExpression_t* ape_parser_parseindexexpr(ApeParser_t* p, ApeExpression_t* left)
{
    ApeExpression_t* index;
    ApeExpression_t* res;
    ape_lexer_nexttoken(&p->lexer);
    index = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
    if(!index)
    {
        return NULL;
    }
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_RBRACKET))
    {
        ape_ast_destroy_expr(index);
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    res = ape_ast_make_indexexpr(p->context, left, index);
    if(!res)
    {
        ape_ast_destroy_expr(index);
        return NULL;
    }
    return res;
}

ApeExpression_t* ape_parser_parseassignexpr(ApeParser_t* p, ApeExpression_t* left)
{
    ApeTokenType_t assigntype;
    ApePosition_t pos;
    ApeOperator_t op;
    ApeExpression_t* source;
    ApeExpression_t* leftcopy;
    ApeExpression_t* newsource;
    ApeExpression_t* res;
    source = NULL;
    assigntype = p->lexer.curtoken.type;
    ape_lexer_nexttoken(&p->lexer);
    source = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
    if(!source)
    {
        goto err;
    }
    switch(assigntype)
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
                op = ape_parser_tokentooperator(assigntype);
                leftcopy = ape_ast_copy_expr(left);
                if(!leftcopy)
                {
                    goto err;
                }
                pos = source->pos;
                newsource = ape_ast_make_infixexpr(p->context, op, leftcopy, source);
                if(!newsource)
                {
                    ape_ast_destroy_expr(leftcopy);
                    goto err;
                }
                newsource->pos = pos;
                source = newsource;
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
    res = ape_ast_make_assignexpr(p->context, left, source, false);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    ape_ast_destroy_expr(source);
    return NULL;
}

ApeExpression_t* ape_parser_parselogicalexpr(ApeParser_t* p, ApeExpression_t* left)
{
    ApeOperator_t op;
    ApePrecedence_t prec;
    ApeExpression_t* right;
    ApeExpression_t* res;
    op = ape_parser_tokentooperator(p->lexer.curtoken.type);
    prec = ape_parser_getprecedence(p->lexer.curtoken.type);
    ape_lexer_nexttoken(&p->lexer);
    right = ape_parser_parseexpr(p, prec);
    if(!right)
    {
        return NULL;
    }
    res = ape_ast_make_logicalexpr(p->context, op, left, right);
    if(!res)
    {
        ape_ast_destroy_expr(right);
        return NULL;
    }
    return res;
}

ApeExpression_t* ape_parser_parseternaryexpr(ApeParser_t* p, ApeExpression_t* left)
{
    ApeExpression_t* iftrue;
    ApeExpression_t* iffalse;
    ApeExpression_t* res;
    ape_lexer_nexttoken(&p->lexer);
    iftrue = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
    if(!iftrue)
    {
        return NULL;
    }
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_COLON))
    {
        ape_ast_destroy_expr(iftrue);
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    iffalse = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
    if(!iffalse)
    {
        ape_ast_destroy_expr(iftrue);
        return NULL;
    }
    res = ape_ast_make_ternaryexpr(p->context, left, iftrue, iffalse);
    if(!res)
    {
        ape_ast_destroy_expr(iftrue);
        ape_ast_destroy_expr(iffalse);
        return NULL;
    }
    return res;
}

ApeExpression_t* ape_parser_parseincdecprefixexpr(ApeParser_t* p)
{
    ApeExpression_t* source;
    ApeTokenType_t operationtype;
    ApePosition_t pos;
    ApeOperator_t op;
    ApeExpression_t* dest;
    ApeExpression_t* oneliteral;
    ApeExpression_t* destcopy;
    ApeExpression_t* operation;
    ApeExpression_t* res;
    source = NULL;
    operationtype = p->lexer.curtoken.type;
    pos = p->lexer.curtoken.pos;
    ape_lexer_nexttoken(&p->lexer);
    op = ape_parser_tokentooperator(operationtype);
    dest = ape_parser_parseexpr(p, PRECEDENCE_PREFIX);
    if(!dest)
    {
        goto err;
    }
    oneliteral = ape_ast_make_numberliteralexpr(p->context, 1);
    if(!oneliteral)
    {
        ape_ast_destroy_expr(dest);
        goto err;
    }
    oneliteral->pos = pos;
    destcopy = ape_ast_copy_expr(dest);
    if(!destcopy)
    {
        ape_ast_destroy_expr(oneliteral);
        ape_ast_destroy_expr(dest);
        goto err;
    }
    operation = ape_ast_make_infixexpr(p->context, op, destcopy, oneliteral);
    if(!operation)
    {
        ape_ast_destroy_expr(destcopy);
        ape_ast_destroy_expr(dest);
        ape_ast_destroy_expr(oneliteral);
        goto err;
    }
    operation->pos = pos;

    res = ape_ast_make_assignexpr(p->context, dest, operation, false);
    if(!res)
    {
        ape_ast_destroy_expr(dest);
        ape_ast_destroy_expr(operation);
        goto err;
    }
    return res;
err:
    ape_ast_destroy_expr(source);
    return NULL;
}

ApeExpression_t* ape_parser_parseincdecpostfixexpr(ApeParser_t* p, ApeExpression_t* left)
{
    ApeExpression_t* source;
    ApeTokenType_t operationtype;
    ApePosition_t pos;
    ApeOperator_t op;
    ApeExpression_t* leftcopy;
    ApeExpression_t* oneliteral;
    ApeExpression_t* operation;
    ApeExpression_t* res;
    source = NULL;
    operationtype = p->lexer.curtoken.type;
    pos = p->lexer.curtoken.pos;
    ape_lexer_nexttoken(&p->lexer);
    op = ape_parser_tokentooperator(operationtype);
    leftcopy = ape_ast_copy_expr(left);
    if(!leftcopy)
    {
        goto err;
    }
    oneliteral = ape_ast_make_numberliteralexpr(p->context, 1);
    if(!oneliteral)
    {
        ape_ast_destroy_expr(leftcopy);
        goto err;
    }
    oneliteral->pos = pos;
    operation = ape_ast_make_infixexpr(p->context, op, leftcopy, oneliteral);
    if(!operation)
    {
        ape_ast_destroy_expr(oneliteral);
        ape_ast_destroy_expr(leftcopy);
        goto err;
    }
    operation->pos = pos;
    res = ape_ast_make_assignexpr(p->context, left, operation, true);
    if(!res)
    {
        ape_ast_destroy_expr(operation);
        goto err;
    }
    return res;
err:
    ape_ast_destroy_expr(source);
    return NULL;
}

ApeExpression_t* ape_parser_parsedotexpr(ApeParser_t* p, ApeExpression_t* left)
{
    char* str;
    ApeExpression_t* index;
    ApeExpression_t* res;
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_IDENT))
    {
        return NULL;
    }
    str = ape_lexer_tokendupliteral(p->context, &p->lexer.curtoken);
    index = ape_ast_make_stringliteralexpr(p->context, str);
    if(!index)
    {
        allocator_free(p->alloc, str);
        return NULL;
    }
    index->pos = p->lexer.curtoken.pos;
    ape_lexer_nexttoken(&p->lexer);
    res = ape_ast_make_indexexpr(p->context, left, index);
    if(!res)
    {
        ape_ast_destroy_expr(index);
        return NULL;
    }
    return res;
}

static ApePrecedence_t ape_parser_getprecedence(ApeTokenType_t tk)
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
            break;
    }
    return PRECEDENCE_LOWEST;
}

static ApeOperator_t ape_parser_tokentooperator(ApeTokenType_t tk)
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
            break;
    }
    APE_ASSERT(false);
    return OPERATOR_NONE;
}

static char ape_parser_escapechar(const char c)
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
            break;
    }
    return c;
}

static char* ape_ast_processandcopystring(ApeAllocator_t* alloc, const char* input, size_t len)
{
    size_t ini;
    size_t outi;
    char* output;
    output = (char*)allocator_malloc(alloc, len + 1);
    if(!output)
    {
        return NULL;
    }
    ini = 0;
    outi = 0;
    while(input[ini] != '\0' && ini < len)
    {
        if(input[ini] == '\\')
        {
            ini++;
            output[outi] = ape_parser_escapechar(input[ini]);
            if(output[outi] < 0)
            {
                goto error;
            }
        }
        else
        {
            output[outi] = input[ini];
        }
        outi++;
        ini++;
    }
    output[outi] = '\0';
    return output;
error:
    allocator_free(alloc, output);
    return NULL;
}

static ApeExpression_t* ape_ast_wrapexprinfunccall(ApeContext_t* ctx, ApeExpression_t* expr, const char* functionname)
{
    bool ok;
    ApeExpression_t* callexpr;
    ApeIdent_t* ident;
    ApeExpression_t* functionidentexpr;
    ApePtrArray_t* args;
    ApeToken_t fntoken;
    ape_lexer_token_init(&fntoken, TOKEN_IDENT, functionname, (int)strlen(functionname));
    fntoken.pos = expr->pos;
    ident = ape_ast_make_ident(ctx, fntoken);
    if(!ident)
    {
        return NULL;
    }
    ident->pos = fntoken.pos;
    functionidentexpr = ape_ast_make_identexpr(ctx, ident);
    if(!functionidentexpr)
    {
        ape_ast_destroy_ident(ident);
        return NULL;
    }
    functionidentexpr->pos = expr->pos;
    ident = NULL;
    args = ape_make_ptrarray(ctx);
    if(!args)
    {
        ape_ast_destroy_expr(functionidentexpr);
        return NULL;
    }
    ok = ape_ptrarray_add(args, expr);
    if(!ok)
    {
        ape_ptrarray_destroy(args);
        ape_ast_destroy_expr(functionidentexpr);
        return NULL;
    }
    callexpr = ape_ast_make_callexpr(ctx, functionidentexpr, args);
    if(!callexpr)
    {
        ape_ptrarray_destroy(args);
        ape_ast_destroy_expr(functionidentexpr);
        return NULL;
    }
    callexpr->pos = expr->pos;
    return callexpr;
}


