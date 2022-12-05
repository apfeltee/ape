
#include "ape.h"

static const ApePosition_t g_prspriv_srcposinvalid = { NULL, -1, -1 };

static ApePrecedence_t ape_parser_getprecedence(ApeTokenType_t tk);
static ApeOperator_t ape_parser_tokentooperator(ApeTokenType_t tk);
static char ape_parser_escapechar(const char c);
static char *ape_ast_processandcopystring(ApeAllocator_t *alloc, const char *input, size_t len, ApeSize_t* destlen);
static ApeExpression_t *ape_ast_wrapexprinfunccall(ApeContext_t *ctx, ApeExpression_t *expr, const char *functionname);

static ApeIdent_t* ape_ast_make_ident(ApeContext_t* ctx, ApeToken_t tok)
{
    ApeIdent_t* res = (ApeIdent_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeIdent_t));
    if(!res)
    {
        return NULL;
    }
    res->context = ctx;
    res->alloc = &ctx->alloc;
    res->value = ape_lexer_tokendupliteral(ctx, &tok);
    if(!res->value)
    {
        ape_allocator_free(&ctx->alloc, res);
        return NULL;
    }
    res->pos = tok.pos;
    return res;
}

static ApeIdent_t* ape_ast_copy_ident(ApeIdent_t* ident)
{
    ApeIdent_t* res = (ApeIdent_t*)ape_allocator_alloc(ident->alloc, sizeof(ApeIdent_t));
    if(!res)
    {
        return NULL;
    }
    res->context = ident->context;
    res->alloc = ident->alloc;
    res->value = ape_util_strdup(ident->context, ident->value);
    if(!res->value)
    {
        ape_allocator_free(ident->alloc, res);
        return NULL;
    }
    res->pos = ident->pos;
    return res;
}

ApeExpression_t* ape_ast_make_identexpr(ApeContext_t* ctx, ApeIdent_t* ident)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, APE_EXPR_IDENT);
    if(!res)
    {
        return NULL;
    }
    res->ident = ident;
    return res;
}

ApeExpression_t* ape_ast_make_numberliteralexpr(ApeContext_t* ctx, ApeFloat_t val)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, APE_EXPR_LITERALNUMBER);
    if(!res)
    {
        return NULL;
    }
    res->numberliteral = val;
    return res;
}

ApeExpression_t* ape_ast_make_boolliteralexpr(ApeContext_t* ctx, bool val)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, APE_EXPR_LITERALBOOL);
    if(!res)
    {
        return NULL;
    }
    res->boolliteral = val;
    return res;
}

ApeExpression_t* ape_ast_make_stringliteralexpr(ApeContext_t* ctx, const char* value, ApeSize_t len)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, APE_EXPR_LITERALSTRING);
    if(!res)
    {
        return NULL;
    }
    res->stringliteral = value;
    res->stringlitlength = len;
    return res;
}

ApeExpression_t* ape_ast_make_nullliteralexpr(ApeContext_t* ctx)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, APE_EXPR_LITERALNULL);
    if(!res)
    {
        return NULL;
    }
    return res;
}

ApeExpression_t* ape_ast_make_arrayliteralexpr(ApeContext_t* ctx, ApePtrArray_t * values)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, APE_EXPR_LITERALARRAY);
    if(!res)
    {
        return NULL;
    }
    res->array = values;
    return res;
}

ApeExpression_t* ape_ast_make_mapliteralexpr(ApeContext_t* ctx, ApePtrArray_t * keys, ApePtrArray_t * values)
{
    ApeExpression_t* res = ape_ast_make_expression(ctx, APE_EXPR_LITERALMAP);
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
    ApeExpression_t* res = ape_ast_make_expression(ctx, APE_EXPR_PREFIX);
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
    ApeExpression_t* res = ape_ast_make_expression(ctx, APE_EXPR_INFIX);
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
    ApeExpression_t* res = ape_ast_make_expression(ctx, APE_EXPR_LITERALFUNCTION);
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
    ApeExpression_t* res = ape_ast_make_expression(ctx, APE_EXPR_CALL);
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
    ApeExpression_t* res = ape_ast_make_expression(ctx, APE_EXPR_INDEX);
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
    ApeExpression_t* res = ape_ast_make_expression(ctx, APE_EXPR_ASSIGN);
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
    ApeExpression_t* res = ape_ast_make_expression(ctx, APE_EXPR_LOGICAL);
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
    ApeExpression_t* res = ape_ast_make_expression(ctx, APE_EXPR_TERNARY);
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
        case APE_EXPR_NONE:
        {
            APE_ASSERT(false);
            break;
        }
        case APE_EXPR_IDENT:
        {
            ape_ast_destroy_ident(expr->ident);
            break;
        }
        case APE_EXPR_LITERALNUMBER:
        case APE_EXPR_LITERALBOOL:
        {
            break;
        }
        case APE_EXPR_LITERALSTRING:
        {
            ape_allocator_free(expr->alloc, expr->stringliteral);
            break;
        }
        case APE_EXPR_LITERALNULL:
        {
            break;
        }
        case APE_EXPR_LITERALARRAY:
        {
            ape_ptrarray_destroywithitems(expr->array, (ApeDataCallback_t)ape_ast_destroy_expr);
            break;
        }
        case APE_EXPR_LITERALMAP:
        {
            ape_ptrarray_destroywithitems(expr->map.keys, (ApeDataCallback_t)ape_ast_destroy_expr);
            ape_ptrarray_destroywithitems(expr->map.values, (ApeDataCallback_t)ape_ast_destroy_expr);
            break;
        }
        case APE_EXPR_PREFIX:
        {
            ape_ast_destroy_expr(expr->prefix.right);
            break;
        }
        case APE_EXPR_INFIX:
        {
            ape_ast_destroy_expr(expr->infix.left);
            ape_ast_destroy_expr(expr->infix.right);
            break;
        }
        case APE_EXPR_LITERALFUNCTION:
        {
            ApeFnLiteral_t* fn = &expr->fnliteral;
            ape_allocator_free(expr->alloc, fn->name);
            ape_ptrarray_destroywithitems(fn->params, (ApeDataCallback_t)ape_ast_destroy_ident);
            ape_ast_destroy_codeblock(fn->body);
            break;
        }
        case APE_EXPR_CALL:
        {
            ape_ptrarray_destroywithitems(expr->callexpr.args, (ApeDataCallback_t)ape_ast_destroy_expr);
            ape_ast_destroy_expr(expr->callexpr.function);
            break;
        }
        case APE_EXPR_INDEX:
        {
            ape_ast_destroy_expr(expr->indexexpr.left);
            ape_ast_destroy_expr(expr->indexexpr.index);
            break;
        }
        case APE_EXPR_ASSIGN:
        {
            ape_ast_destroy_expr(expr->assign.dest);
            ape_ast_destroy_expr(expr->assign.source);
            break;
        }
        case APE_EXPR_LOGICAL:
        {
            ape_ast_destroy_expr(expr->logical.left);
            ape_ast_destroy_expr(expr->logical.right);
            break;
        }
        case APE_EXPR_TERNARY:
        {
            ape_ast_destroy_expr(expr->ternary.test);
            ape_ast_destroy_expr(expr->ternary.iftrue);
            ape_ast_destroy_expr(expr->ternary.iffalse);
            break;
        }
    }
    ape_allocator_free(expr->alloc, expr);
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
        case APE_EXPR_NONE:
        {
            APE_ASSERT(false);
            break;
        }
        case APE_EXPR_IDENT:
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
        case APE_EXPR_LITERALNUMBER:
        {
            res = ape_ast_make_numberliteralexpr(expr->context, expr->numberliteral);
            break;
        }
        case APE_EXPR_LITERALBOOL:
        {
            res = ape_ast_make_boolliteralexpr(expr->context, expr->boolliteral);
            break;
        }
        case APE_EXPR_LITERALSTRING:
        {
            char* stringcopy = ape_util_strndup(expr->context, expr->stringliteral, expr->stringlitlength);
            if(!stringcopy)
            {
                return NULL;
            }
            res = ape_ast_make_stringliteralexpr(expr->context, stringcopy, expr->stringlitlength);
            if(!res)
            {
                ape_allocator_free(expr->alloc, stringcopy);
                return NULL;
            }
            break;
        }
        case APE_EXPR_LITERALNULL:
        {
            res = ape_ast_make_nullliteralexpr(expr->context);
            break;
        }
        case APE_EXPR_LITERALARRAY:
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
        case APE_EXPR_LITERALMAP:
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
        case APE_EXPR_PREFIX:
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
        case APE_EXPR_INFIX:
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
        case APE_EXPR_LITERALFUNCTION:
        {
            ApePtrArray_t* paramscopy = ape_ptrarray_copywithitems(expr->fnliteral.params, (ApeDataCallback_t)ape_ast_copy_ident,(ApeDataCallback_t) ape_ast_destroy_ident);
            ApeCodeblock_t* bodycopy = ape_ast_copy_codeblock(expr->fnliteral.body);
            char* namecopy = ape_util_strdup(expr->context, expr->fnliteral.name);
            if(!paramscopy || !bodycopy)
            {
                ape_ptrarray_destroywithitems(paramscopy, (ApeDataCallback_t)ape_ast_destroy_ident);
                ape_ast_destroy_codeblock(bodycopy);
                ape_allocator_free(expr->alloc, namecopy);
                return NULL;
            }
            res = ape_ast_make_fnliteralexpr(expr->context, paramscopy, bodycopy);
            if(!res)
            {
                ape_ptrarray_destroywithitems(paramscopy, (ApeDataCallback_t)ape_ast_destroy_ident);
                ape_ast_destroy_codeblock(bodycopy);
                ape_allocator_free(expr->alloc, namecopy);
                return NULL;
            }
            res->fnliteral.name = namecopy;
            break;
        }
        case APE_EXPR_CALL:
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
        case APE_EXPR_INDEX:
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
        case APE_EXPR_ASSIGN:
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
        case APE_EXPR_LOGICAL:
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
        case APE_EXPR_TERNARY:
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
    ApeStatement_t* res = ape_ast_make_statement(ctx, APE_STMT_DEFINE);
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
    ApeStatement_t* res = ape_ast_make_statement(ctx, APE_STMT_IF);
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
    ApeStatement_t* res = ape_ast_make_statement(ctx, APE_STMT_RETURNVALUE);
    if(!res)
    {
        return NULL;
    }
    res->returnvalue = value;
    return res;
}

ApeStatement_t* ape_ast_make_expressionstmt(ApeContext_t* ctx, ApeExpression_t* value)
{
    ApeStatement_t* res = ape_ast_make_statement(ctx, APE_STMT_EXPRESSION);
    if(!res)
    {
        return NULL;
    }
    res->expression = value;
    return res;
}

ApeStatement_t* ape_ast_make_whileloopstmt(ApeContext_t* ctx, ApeExpression_t* test, ApeCodeblock_t* body)
{
    ApeStatement_t* res = ape_ast_make_statement(ctx, APE_STMT_WHILELOOP);
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
    ApeStatement_t* res = ape_ast_make_statement(ctx, APE_STMT_BREAK);
    if(!res)
    {
        return NULL;
    }
    return res;
}

ApeStatement_t* ape_ast_make_foreachstmt(ApeContext_t* ctx, ApeIdent_t* iterator, ApeExpression_t* source, ApeCodeblock_t* body)
{
    ApeStatement_t* res = ape_ast_make_statement(ctx, APE_STMT_FOREACH);
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
    ApeStatement_t* res = ape_ast_make_statement(ctx, APE_STMT_FORLOOP);
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
    ApeStatement_t* res = ape_ast_make_statement(ctx, APE_STMT_CONTINUE);
    if(!res)
    {
        return NULL;
    }
    return res;
}

ApeStatement_t* ape_ast_make_blockstmt(ApeContext_t* ctx, ApeCodeblock_t* block)
{
    ApeStatement_t* res = ape_ast_make_statement(ctx, APE_STMT_BLOCK);
    if(!res)
    {
        return NULL;
    }
    res->block = block;
    return res;
}

ApeStatement_t* ape_ast_make_includestmt(ApeContext_t* ctx, char* path)
{
    ApeStatement_t* res = ape_ast_make_statement(ctx, APE_STMT_INCLUDE);
    if(!res)
    {
        return NULL;
    }
    res->stmtinclude.path = path;
    return res;
}

ApeStatement_t* ape_ast_make_recoverstmt(ApeContext_t* ctx, ApeIdent_t* errorident, ApeCodeblock_t* body)
{
    ApeStatement_t* res = ape_ast_make_statement(ctx, APE_STMT_RECOVER);
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
        case APE_STMT_NONE:
            {
                APE_ASSERT(false);
            }
            break;
        case APE_STMT_DEFINE:
            {
                ape_ast_destroy_ident(stmt->define.name);
                ape_ast_destroy_expr(stmt->define.value);
            }
            break;
        case APE_STMT_IF:
            {
                ape_ptrarray_destroywithitems(stmt->ifstatement.cases, (ApeDataCallback_t)ape_ast_destroy_ifcase);
                ape_ast_destroy_codeblock(stmt->ifstatement.alternative);
            }
            break;
        case APE_STMT_RETURNVALUE:
            {
                ape_ast_destroy_expr(stmt->returnvalue);
            }
            break;
        case APE_STMT_EXPRESSION:
            {
                ape_ast_destroy_expr(stmt->expression);
            }
            break;
        case APE_STMT_WHILELOOP:
            {
                ape_ast_destroy_expr(stmt->whileloop.test);
                ape_ast_destroy_codeblock(stmt->whileloop.body);
            }
            break;
        case APE_STMT_BREAK:
            {
            }
            break;
        case APE_STMT_CONTINUE:
            {
            }
            break;
        case APE_STMT_FOREACH:
            {
                ape_ast_destroy_ident(stmt->foreach.iterator);
                ape_ast_destroy_expr(stmt->foreach.source);
                ape_ast_destroy_codeblock(stmt->foreach.body);
            }
            break;
        case APE_STMT_FORLOOP:
            {
                ape_ast_destroy_stmt(stmt->forloop.init);
                ape_ast_destroy_expr(stmt->forloop.test);
                ape_ast_destroy_expr(stmt->forloop.update);
                ape_ast_destroy_codeblock(stmt->forloop.body);
            }
            break;
        case APE_STMT_BLOCK:
            {
                ape_ast_destroy_codeblock(stmt->block);
            }
            break;
        case APE_STMT_INCLUDE:
            {
                ape_allocator_free(stmt->alloc, stmt->stmtinclude.path);
            }
            break;
        case APE_STMT_RECOVER:
            {
                ape_ast_destroy_codeblock(stmt->recover.body);
                ape_ast_destroy_ident(stmt->recover.errorident);
            }
            break;
    }
    ape_allocator_free(stmt->alloc, stmt);
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
        case APE_STMT_NONE:
            {
                APE_ASSERT(false);
            }
            break;
        case APE_STMT_DEFINE:
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
        case APE_STMT_IF:
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
        case APE_STMT_RETURNVALUE:
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
        case APE_STMT_EXPRESSION:
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
        case APE_STMT_WHILELOOP:
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
        case APE_STMT_BREAK:
            {
                res = ape_ast_make_breakstmt(stmt->context);
            }
            break;
        case APE_STMT_CONTINUE:
            {
                res = ape_ast_make_continuestmt(stmt->context);
            }
            break;
        case APE_STMT_FOREACH:
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
        case APE_STMT_FORLOOP:
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
        case APE_STMT_BLOCK:
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
        case APE_STMT_INCLUDE:
            {
                char* pathcopy = ape_util_strdup(stmt->context, stmt->stmtinclude.path);
                if(!pathcopy)
                {
                    return NULL;
                }
                res = ape_ast_make_includestmt(stmt->context, pathcopy);
                if(!res)
                {
                    ape_allocator_free(stmt->alloc, pathcopy);
                    return NULL;
                }
            }
            break;
        case APE_STMT_RECOVER:
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
    block = (ApeCodeblock_t*)ape_allocator_alloc(alloc, sizeof(ApeCodeblock_t));
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
    ape_allocator_free(block->alloc, block);
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
    ape_allocator_free(ident->alloc, ident->value);
    ident->value = NULL;
    ident->pos = g_prspriv_srcposinvalid;
    ape_allocator_free(ident->alloc, ident);
    return NULL;
}

ApeIfCase_t* ape_ast_make_if_case(ApeAllocator_t* alloc, ApeExpression_t* test, ApeCodeblock_t* consequence)
{
    ApeIfCase_t* res;
    res = (ApeIfCase_t*)ape_allocator_alloc(alloc, sizeof(ApeIfCase_t));
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
    ape_allocator_free(cond->alloc, cond);
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
    res = (ApeExpression_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeExpression_t));
    if(!res)
    {
        return NULL;
    }
    res->context = ctx;
    res->alloc = &ctx->alloc;
    res->type = type;
    res->pos = g_prspriv_srcposinvalid;
    return res;
}

ApeStatement_t* ape_ast_make_statement(ApeContext_t* ctx, ApeStmtType_t type)
{
    ApeStatement_t* res = (ApeStatement_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeStatement_t));
    if(!res)
    {
        return NULL;
    }
    res->context = ctx;
    res->alloc = &ctx->alloc;
    res->type = type;
    res->pos = g_prspriv_srcposinvalid;
    return res;
}

ApeParser_t* ape_ast_make_parser(ApeContext_t* ctx, const ApeConfig_t* config, ApeErrorList_t* errors)
{
    ApeParser_t* parser;
    parser = (ApeParser_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeParser_t));
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
        parser->rightassocparsefns[TOKEN_VALIDENT] = ape_parser_parseident;
        parser->rightassocparsefns[TOKEN_VALNUMBER] = ape_parser_parseliteralnumber;
        parser->rightassocparsefns[TOKEN_KWTRUE] = ape_parser_parseliteralbool;
        parser->rightassocparsefns[TOKEN_KWFALSE] = ape_parser_parseliteralbool;
        parser->rightassocparsefns[TOKEN_VALSTRING] = ape_parser_parseliteralstring;
        parser->rightassocparsefns[TOKEN_VALTPLSTRING] = ape_parser_parseliteraltplstring;
        parser->rightassocparsefns[TOKEN_KWNULL] = ape_parser_parseliteralnull;
        parser->rightassocparsefns[TOKEN_OPNOT] = ape_parser_parseprefixexpr;
        parser->rightassocparsefns[TOKEN_OPMINUS] = ape_parser_parseprefixexpr;
        parser->rightassocparsefns[TOKEN_OPLEFTPAREN] = ape_parser_parsegroupedexpr;
        parser->rightassocparsefns[TOKEN_KWFUNCTION] = ape_parser_parseliteralfunc;
        parser->rightassocparsefns[TOKEN_OPLEFTBRACKET] = ape_parser_parseliteralarray;
        parser->rightassocparsefns[TOKEN_OPLEFTBRACE] = ape_parser_parseliteralmap;
        parser->rightassocparsefns[TOKEN_OPINCREASE] = ape_parser_parseincdecprefixexpr;
        parser->rightassocparsefns[TOKEN_OPDECREASE] = ape_parser_parseincdecprefixexpr;
    }
    {
        parser->leftassocparsefns[TOKEN_OPPLUS] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_OPMINUS] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_OPSLASH] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_OPSTAR] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_OPMODULO] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_OPEQUAL] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_OPNOTEQUAL] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_OPLESSTHAN] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_OPLESSEQUAL] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_OPGREATERTHAN] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_OPGREATERTEQUAL] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_OPLEFTPAREN] = ape_parser_parsecallexpr;
        parser->leftassocparsefns[TOKEN_OPLEFTBRACKET] = ape_parser_parseindexexpr;
        parser->leftassocparsefns[TOKEN_ASSIGN] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_ASSIGNPLUS] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_ASSIGNMINUS] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_ASSIGNSLASH] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_ASSIGNSTAR] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_ASSIGNMODULO] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_ASSIGNBITAND] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_ASSIGNBITOR] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_ASSIGNBITXOR] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_ASSIGNLEFTSHIFT] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_ASSIGNRIGHTSHIFT] = ape_parser_parseassignexpr;
        parser->leftassocparsefns[TOKEN_OPDOT] = ape_parser_parsedotexpr;
        parser->leftassocparsefns[TOKEN_OPAND] = ape_parser_parselogicalexpr;
        parser->leftassocparsefns[TOKEN_OPOR] = ape_parser_parselogicalexpr;
        parser->leftassocparsefns[TOKEN_OPBITAND] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_OPBITOR] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_OPBITXOR] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_OPLEFTSHIFT] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_OPRIGHTSHIFT] = ape_parser_parseinfixexpr;
        parser->leftassocparsefns[TOKEN_OPQUESTION] = ape_parser_parseternaryexpr;
        parser->leftassocparsefns[TOKEN_OPINCREASE] = ape_parser_parseincdecpostfixexpr;
        parser->leftassocparsefns[TOKEN_OPDECREASE] = ape_parser_parseincdecpostfixexpr;
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
    ape_allocator_free(parser->alloc, parser);
}

ApePtrArray_t * ape_parser_parseall(ApeParser_t* parser, const char* input, ApeCompFile_t* file)
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
        if(ape_lexer_currenttokenis(&parser->lexer, TOKEN_OPSEMICOLON))
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
    if(ape_errorlist_count(parser->errors) > 0)
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
        case TOKEN_KWVAR:
        case TOKEN_KWCONST:
            {
                res = ape_parser_parsedefinestmt(p);
            }
            break;
        case TOKEN_KWIF:
            {
                res = ape_parser_parseifstmt(p);
            }
            break;
        case TOKEN_KWRETURN:
            {
                res = ape_parser_parsereturnstmt(p);
            }
            break;
        case TOKEN_KWWHILE:
            {
                res = ape_parser_parsewhileloopstmt(p);
            }
            break;
        case TOKEN_KWBREAK:
            {
                res = ape_parser_parsebreakstmt(p);
            }
            break;
        case TOKEN_KWFOR:
            {
                res = ape_parser_parseforloopstmt(p);
            }
            break;
        case TOKEN_KWFUNCTION:
            {
                if(ape_lexer_peektokenis(&p->lexer, TOKEN_VALIDENT))
                {
                    res = ape_parser_parsefuncstmt(p);
                }
                else
                {
                    res = ape_parser_parseexprstmt(p);
                }
            }
            break;
        case TOKEN_OPLEFTBRACE:
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
        case TOKEN_KWCONTINUE:
            {
                res = ape_parser_parsecontinuestmt(p);
            }
            break;
        case TOKEN_KWINCLUDE:
            {
                res = ape_parser_parseincludestmt(p);
            }
            break;
        case TOKEN_KWRECOVER:
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
    assignable = ape_lexer_currenttokenis(&p->lexer, TOKEN_KWVAR);
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_VALIDENT))
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
    if(value->type == APE_EXPR_LITERALFUNCTION)
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
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPLEFTPAREN))
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
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPRIGHTPAREN))
    {
        goto err;
    }
    ape_lexer_nexttoken(&p->lexer);
    cond->consequence = ape_parser_parsecodeblock(p);
    if(!cond->consequence)
    {
        goto err;
    }
    while(ape_lexer_currenttokenis(&p->lexer, TOKEN_KWELSE))
    {
        ape_lexer_nexttoken(&p->lexer);
        if(ape_lexer_currenttokenis(&p->lexer, TOKEN_KWIF))
        {
            ape_lexer_nexttoken(&p->lexer);
            if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPLEFTPAREN))
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
            if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPRIGHTPAREN))
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
    if(!ape_lexer_currenttokenis(&p->lexer, TOKEN_OPSEMICOLON) && !ape_lexer_currenttokenis(&p->lexer, TOKEN_OPRIGHTBRACE) && !ape_lexer_currenttokenis(&p->lexer, TOKEN_EOF))
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
        if(expr->type != APE_EXPR_ASSIGN && expr->type != APE_EXPR_CALL)
        {
            ape_errorlist_addformat(p->errors, APE_ERROR_PARSING, expr->pos, "only assignments and function calls can be expression statements");
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
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPLEFTPAREN))
    {
        goto err;
    }
    ape_lexer_nexttoken(&p->lexer);
    test = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
    if(!test)
    {
        goto err;
    }
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPRIGHTPAREN))
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

ApeStatement_t* ape_parser_parseincludestmt(ApeParser_t* p)
{
    char* processedname;
    ApeSize_t len;
    ApeStatement_t* res;
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_VALSTRING))
    {
        return NULL;
    }
    processedname = ape_ast_processandcopystring(p->alloc, p->lexer.curtoken.literal, p->lexer.curtoken.len, &len);
    if(!processedname)
    {
        ape_errorlist_add(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "error when parsing module name");
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    res= ape_ast_make_includestmt(p->context, processedname);
    if(!res)
    {
        ape_allocator_free(p->alloc, processedname);
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
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPLEFTPAREN))
    {
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_VALIDENT))
    {
        return NULL;
    }
    errorident = ape_ast_make_ident(p->context, p->lexer.curtoken);
    if(!errorident)
    {
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPRIGHTPAREN))
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
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPLEFTPAREN))
    {
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    if(ape_lexer_currenttokenis(&p->lexer, TOKEN_VALIDENT) && ape_lexer_peektokenis(&p->lexer, TOKEN_KWIN))
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
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_KWIN))
    {
        goto err;
    }
    ape_lexer_nexttoken(&p->lexer);
    source = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
    if(!source)
    {
        goto err;
    }
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPRIGHTPAREN))
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
    if(!ape_lexer_currenttokenis(&p->lexer, TOKEN_OPSEMICOLON))
    {
        init = ape_parser_parsestmt(p);
        if(!init)
        {
            goto err;
        }
        if(init->type != APE_STMT_DEFINE && init->type != APE_STMT_EXPRESSION)
        {
            ape_errorlist_addformat(p->errors, APE_ERROR_PARSING, init->pos, "for loop's init clause should be a define statement or an expression");
            goto err;
        }
        if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPSEMICOLON))
        {
            goto err;
        }
    }
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_currenttokenis(&p->lexer, TOKEN_OPSEMICOLON))
    {
        test = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
        if(!test)
        {
            goto err;
        }
        if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPSEMICOLON))
        {
            goto err;
        }
    }
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_currenttokenis(&p->lexer, TOKEN_OPRIGHTPAREN))
    {
        update = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
        if(!update)
        {
            goto err;
        }
        if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPRIGHTPAREN))
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
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_VALIDENT))
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
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPLEFTBRACE))
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
    while(!ape_lexer_currenttokenis(&p->lexer, TOKEN_OPRIGHTBRACE))
    {
        if(ape_lexer_currenttokenis(&p->lexer, TOKEN_EOF))
        {
            ape_errorlist_add(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "unexpected EOF");
            goto err;
        }
        if(ape_lexer_currenttokenis(&p->lexer, TOKEN_OPSEMICOLON))
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
        ape_errorlist_add(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "illegal token");
        return NULL;
    }
    prighta = p->rightassocparsefns[p->lexer.curtoken.type];
    if(!prighta)
    {
        literal = ape_lexer_tokendupliteral(p->context, &p->lexer.curtoken);
        ape_errorlist_addformat(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "no prefix parse function for '%s' found", literal);
        ape_allocator_free(p->alloc, literal);
        return NULL;
    }
    leftexpr = prighta(p);
    if(!leftexpr)
    {
        return NULL;
    }
    leftexpr->pos = pos;
    while(!ape_lexer_currenttokenis(&p->lexer, TOKEN_OPSEMICOLON) && prec < ape_parser_getprecedence(p->lexer.curtoken.type))
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
        ape_errorlist_addformat(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "parsing number literal '%s' failed", literal);
        ape_allocator_free(p->alloc, literal);
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    return ape_ast_make_numberliteralexpr(p->context, number);
}

ApeExpression_t* ape_parser_parseliteralbool(ApeParser_t* p)
{
    ApeExpression_t* res;
    res = ape_ast_make_boolliteralexpr(p->context, p->lexer.curtoken.type == TOKEN_KWTRUE);
    ape_lexer_nexttoken(&p->lexer);
    return res;
}

ApeExpression_t* ape_parser_parseliteralstring(ApeParser_t* p)
{
    char* processedliteral;
    ApeSize_t len;
    ApeExpression_t* res;
    processedliteral = ape_ast_processandcopystring(p->alloc, p->lexer.curtoken.literal, p->lexer.curtoken.len, &len);
    if(!processedliteral)
    {
        ape_errorlist_add(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "error while parsing string literal");
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    res = ape_ast_make_stringliteralexpr(p->context, processedliteral, len);
    if(!res)
    {
        ape_allocator_free(p->alloc, processedliteral);
        return NULL;
    }
    return res;
}

ApeExpression_t* ape_parser_parseliteraltplstring(ApeParser_t* p)
{
    char* processedliteral;
    ApeSize_t len;
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
    processedliteral = ape_ast_processandcopystring(p->alloc, p->lexer.curtoken.literal, p->lexer.curtoken.len, &len);
    if(!processedliteral)
    {
        ape_errorlist_add(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "error while parsing string literal");
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);

    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPLEFTBRACE))
    {
        goto err;
    }
    ape_lexer_nexttoken(&p->lexer);

    pos = p->lexer.curtoken.pos;

    leftstringexpr = ape_ast_make_stringliteralexpr(p->context, processedliteral, len);
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
    leftaddexpr = ape_ast_make_infixexpr(p->context, APE_OPERATOR_PLUS, leftstringexpr, tostrcallexpr);
    if(!leftaddexpr)
    {
        goto err;
    }
    leftaddexpr->pos = pos;
    leftstringexpr = NULL;
    tostrcallexpr = NULL;
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPRIGHTBRACE))
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
    rightaddexpr = ape_ast_make_infixexpr(p->context, APE_OPERATOR_PLUS, leftaddexpr, rightexpr);
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
    ape_allocator_free(p->alloc, processedliteral);
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
    array = ape_parser_parseexprlist(p, TOKEN_OPLEFTBRACKET, TOKEN_OPRIGHTBRACKET, true);
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
    while(!ape_lexer_currenttokenis(&p->lexer, TOKEN_OPRIGHTBRACE))
    {
        key = NULL;
        if(ape_lexer_currenttokenis(&p->lexer, TOKEN_VALIDENT))
        {
            str = ape_lexer_tokendupliteral(p->context, &p->lexer.curtoken);
            key = ape_ast_make_stringliteralexpr(p->context, str, strlen(str));
            if(!key)
            {
                ape_allocator_free(p->alloc, str);
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
                case APE_EXPR_LITERALSTRING:
                case APE_EXPR_LITERALNUMBER:
                case APE_EXPR_LITERALBOOL:
                    {
                    }
                    break;
                default:
                    {
                        ape_errorlist_addformat(p->errors, APE_ERROR_PARSING, key->pos, "invalid map literal key type");
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

        if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPCOLON))
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
        if(ape_lexer_currenttokenis(&p->lexer, TOKEN_OPRIGHTBRACE))
        {
            break;
        }
        if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPCOMMA))
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
    if(!expr || !ape_lexer_expectcurrent(&p->lexer, TOKEN_OPRIGHTPAREN))
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
    if(ape_lexer_currenttokenis(&p->lexer, TOKEN_KWFUNCTION))
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
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPLEFTPAREN))
    {
        return false;
    }
    ape_lexer_nexttoken(&p->lexer);
    if(ape_lexer_currenttokenis(&p->lexer, TOKEN_OPRIGHTPAREN))
    {
        ape_lexer_nexttoken(&p->lexer);
        return true;
    }
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_VALIDENT))
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
    while(ape_lexer_currenttokenis(&p->lexer, TOKEN_OPCOMMA))
    {
        ape_lexer_nexttoken(&p->lexer);
        if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_VALIDENT))
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
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPRIGHTPAREN))
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
    args = ape_parser_parseexprlist(p, TOKEN_OPLEFTPAREN, TOKEN_OPRIGHTPAREN, false);
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
    while(ape_lexer_currenttokenis(&p->lexer, TOKEN_OPCOMMA))
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
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPRIGHTBRACKET))
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
        case TOKEN_ASSIGNPLUS:
        case TOKEN_ASSIGNMINUS:
        case TOKEN_ASSIGNSLASH:
        case TOKEN_ASSIGNSTAR:
        case TOKEN_ASSIGNMODULO:
        case TOKEN_ASSIGNBITAND:
        case TOKEN_ASSIGNBITOR:
        case TOKEN_ASSIGNBITXOR:
        case TOKEN_ASSIGNLEFTSHIFT:
        case TOKEN_ASSIGNRIGHTSHIFT:
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
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPCOLON))
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
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_VALIDENT))
    {
        return NULL;
    }
    str = ape_lexer_tokendupliteral(p->context, &p->lexer.curtoken);
    index = ape_ast_make_stringliteralexpr(p->context, str, strlen(str));
    if(!index)
    {
        ape_allocator_free(p->alloc, str);
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
        case TOKEN_OPEQUAL:
            return PRECEDENCE_EQUALS;
        case TOKEN_OPNOTEQUAL:
            return PRECEDENCE_EQUALS;
        case TOKEN_OPLESSTHAN:
            return PRECEDENCE_LESSGREATER;
        case TOKEN_OPLESSEQUAL:
            return PRECEDENCE_LESSGREATER;
        case TOKEN_OPGREATERTHAN:
            return PRECEDENCE_LESSGREATER;
        case TOKEN_OPGREATERTEQUAL:
            return PRECEDENCE_LESSGREATER;
        case TOKEN_OPPLUS:
            return PRECEDENCE_SUM;
        case TOKEN_OPMINUS:
            return PRECEDENCE_SUM;
        case TOKEN_OPSLASH:
            return PRECEDENCE_PRODUCT;
        case TOKEN_OPSTAR:
            return PRECEDENCE_PRODUCT;
        case TOKEN_OPMODULO:
            return PRECEDENCE_PRODUCT;
        case TOKEN_OPLEFTPAREN:
            return PRECEDENCE_POSTFIX;
        case TOKEN_OPLEFTBRACKET:
            return PRECEDENCE_POSTFIX;
        case TOKEN_ASSIGN:
            return PRECEDENCE_ASSIGN;
        case TOKEN_ASSIGNPLUS:
            return PRECEDENCE_ASSIGN;
        case TOKEN_ASSIGNMINUS:
            return PRECEDENCE_ASSIGN;
        case TOKEN_ASSIGNSTAR:
            return PRECEDENCE_ASSIGN;
        case TOKEN_ASSIGNSLASH:
            return PRECEDENCE_ASSIGN;
        case TOKEN_ASSIGNMODULO:
            return PRECEDENCE_ASSIGN;
        case TOKEN_ASSIGNBITAND:
            return PRECEDENCE_ASSIGN;
        case TOKEN_ASSIGNBITOR:
            return PRECEDENCE_ASSIGN;
        case TOKEN_ASSIGNBITXOR:
            return PRECEDENCE_ASSIGN;
        case TOKEN_ASSIGNLEFTSHIFT:
            return PRECEDENCE_ASSIGN;
        case TOKEN_ASSIGNRIGHTSHIFT:
            return PRECEDENCE_ASSIGN;
        case TOKEN_OPDOT:
            return PRECEDENCE_POSTFIX;
        case TOKEN_OPAND:
            return PRECEDENCE_LOGICAL_AND;
        case TOKEN_OPOR:
            return PRECEDENCE_LOGICAL_OR;
        case TOKEN_OPBITOR:
            return PRECEDENCE_BIT_OR;
        case TOKEN_OPBITXOR:
            return PRECEDENCE_BIT_XOR;
        case TOKEN_OPBITAND:
            return PRECEDENCE_BIT_AND;
        case TOKEN_OPLEFTSHIFT:
            return PRECEDENCE_SHIFT;
        case TOKEN_OPRIGHTSHIFT:
            return PRECEDENCE_SHIFT;
        case TOKEN_OPQUESTION:
            return PRECEDENCE_TERNARY;
        case TOKEN_OPINCREASE:
            return PRECEDENCE_INCDEC;
        case TOKEN_OPDECREASE:
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
            return APE_OPERATOR_ASSIGN;
        case TOKEN_OPPLUS:
            return APE_OPERATOR_PLUS;
        case TOKEN_OPMINUS:
            return APE_OPERATOR_MINUS;
        case TOKEN_OPNOT:
            return APE_OPERATOR_NOT;
        case TOKEN_OPSTAR:
            return APE_OPERATOR_STAR;
        case TOKEN_OPSLASH:
            return APE_OPERATOR_SLASH;
        case TOKEN_OPLESSTHAN:
            return APE_OPERATOR_LESSTHAN;
        case TOKEN_OPLESSEQUAL:
            return APE_OPERATOR_LESSEQUAL;
        case TOKEN_OPGREATERTHAN:
            return APE_OPERATOR_GREATERTHAN;
        case TOKEN_OPGREATERTEQUAL:
            return APE_OPERATOR_GREATEREQUAL;
        case TOKEN_OPEQUAL:
            return APE_OPERATOR_EQUAL;
        case TOKEN_OPNOTEQUAL:
            return APE_OPERATOR_NOTEQUAL;
        case TOKEN_OPMODULO:
            return APE_OPERATOR_MODULUS;
        case TOKEN_OPAND:
            return APE_OPERATOR_LOGICALAND;
        case TOKEN_OPOR:
            return APE_OPERATOR_LOGICALOR;
        case TOKEN_ASSIGNPLUS:
            return APE_OPERATOR_PLUS;
        case TOKEN_ASSIGNMINUS:
            return APE_OPERATOR_MINUS;
        case TOKEN_ASSIGNSTAR:
            return APE_OPERATOR_STAR;
        case TOKEN_ASSIGNSLASH:
            return APE_OPERATOR_SLASH;
        case TOKEN_ASSIGNMODULO:
            return APE_OPERATOR_MODULUS;
        case TOKEN_ASSIGNBITAND:
            return APE_OPERATOR_BITAND;
        case TOKEN_ASSIGNBITOR:
            return APE_OPERATOR_BITOR;
        case TOKEN_ASSIGNBITXOR:
            return APE_OPERATOR_BITXOR;
        case TOKEN_ASSIGNLEFTSHIFT:
            return APE_OPERATOR_LEFTSHIFT;
        case TOKEN_ASSIGNRIGHTSHIFT:
            return APE_OPERATOR_RIGHTSHIFT;
        case TOKEN_OPBITAND:
            return APE_OPERATOR_BITAND;
        case TOKEN_OPBITOR:
            return APE_OPERATOR_BITOR;
        case TOKEN_OPBITXOR:
            return APE_OPERATOR_BITXOR;
        case TOKEN_OPLEFTSHIFT:
            return APE_OPERATOR_LEFTSHIFT;
        case TOKEN_OPRIGHTSHIFT:
            return APE_OPERATOR_RIGHTSHIFT;
        case TOKEN_OPINCREASE:
            return APE_OPERATOR_PLUS;
        case TOKEN_OPDECREASE:
            return APE_OPERATOR_MINUS;
        default:
            break;
    }
    APE_ASSERT(false);
    return APE_OPERATOR_NONE;
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

static char* ape_ast_processandcopystring(ApeAllocator_t* alloc, const char* input, size_t len, ApeSize_t* destlen)
{
    size_t ini;
    size_t outi;
    char* output;
    output = (char*)ape_allocator_alloc(alloc, len + 1);
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
    *destlen = outi;
    return output;
error:
    ape_allocator_free(alloc, output);
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
    ape_lexer_token_init(&fntoken, TOKEN_VALIDENT, functionname, (int)strlen(functionname));
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


