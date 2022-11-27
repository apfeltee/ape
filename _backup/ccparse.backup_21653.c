
#include "ape.h"

static const ApePosition_t gprsprivsrcposinvalid = { NULL, -1, -1 };

static ApePrecedence_t get_precedence(ApeTokenType_t tk);
static ApeOperator_t token_to_operator(ApeTokenType_t tk);
static char escape_char(const char c);
static char *process_and_copy_string(ApeAllocator_t *alloc, const char *input, size_t len);
static ApeExpression_t *wrap_expression_in_function_call(ApeContext_t *ctx, ApeExpression_t *expr, const char *functionname);

static ApeIdent_t* ident_make(ApeContext_t* ctx, ApeToken_t tok)
{
    ApeIdent_t* res = (ApeIdent_t*)allocator_malloc(&ctx->alloc, sizeof(ApeIdent_t));
    if(!res)
    {
        return NULL;
    }
    res->context = ctx;
    res->alloc = &ctx->alloc;
    res->value = token_duplicate_literal(ctx, &tok);
    if(!res->value)
    {
        allocator_free(&ctx->alloc, res);
        return NULL;
    }
    res->pos = tok.pos;
    return res;
}

static ApeIdent_t* ident_copy(ApeIdent_t* ident)
{
    ApeIdent_t* res = (ApeIdent_t*)allocator_malloc(ident->alloc, sizeof(ApeIdent_t));
    if(!res)
    {
        return NULL;
    }
    res->context = ident->context;
    res->alloc = ident->alloc;
    res->value = util_strdup(ident->context, ident->value);
    if(!res->value)
    {
        allocator_free(ident->alloc, res);
        return NULL;
    }
    res->pos = ident->pos;
    return res;
}

ApeExpression_t* expression_make_ident(ApeContext_t* ctx, ApeIdent_t* ident)
{
    ApeExpression_t* res = expression_make(ctx, EXPRESSION_IDENT);
    if(!res)
    {
        return NULL;
    }
    res->ident = ident;
    return res;
}

ApeExpression_t* expression_make_number_literal(ApeContext_t* ctx, ApeFloat_t val)
{
    ApeExpression_t* res = expression_make(ctx, EXPRESSION_NUMBER_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->numberliteral = val;
    return res;
}

ApeExpression_t* expression_make_bool_literal(ApeContext_t* ctx, bool val)
{
    ApeExpression_t* res = expression_make(ctx, EXPRESSION_BOOL_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->boolliteral = val;
    return res;
}

ApeExpression_t* expression_make_string_literal(ApeContext_t* ctx, char* value)
{
    ApeExpression_t* res = expression_make(ctx, EXPRESSION_STRING_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->stringliteral = value;
    return res;
}

ApeExpression_t* expression_make_null_literal(ApeContext_t* ctx)
{
    ApeExpression_t* res = expression_make(ctx, EXPRESSION_NULL_LITERAL);
    if(!res)
    {
        return NULL;
    }
    return res;
}

ApeExpression_t* expression_make_array_literal(ApeContext_t* ctx, ApePtrArray_t * values)
{
    ApeExpression_t* res = expression_make(ctx, EXPRESSION_ARRAY_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->array = values;
    return res;
}

ApeExpression_t* expression_make_map_literal(ApeContext_t* ctx, ApePtrArray_t * keys, ApePtrArray_t * values)
{
    ApeExpression_t* res = expression_make(ctx, EXPRESSION_MAP_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->map.keys = keys;
    res->map.values = values;
    return res;
}

ApeExpression_t* expression_make_prefix(ApeContext_t* ctx, ApeOperator_t op, ApeExpression_t* right)
{
    ApeExpression_t* res = expression_make(ctx, EXPRESSION_PREFIX);
    if(!res)
    {
        return NULL;
    }
    res->prefix.op = op;
    res->prefix.right = right;
    return res;
}

ApeExpression_t* expression_make_infix(ApeContext_t* ctx, ApeOperator_t op, ApeExpression_t* left, ApeExpression_t* right)
{
    ApeExpression_t* res = expression_make(ctx, EXPRESSION_INFIX);
    if(!res)
    {
        return NULL;
    }
    res->infix.op = op;
    res->infix.left = left;
    res->infix.right = right;
    return res;
}

ApeExpression_t* expression_make_fn_literal(ApeContext_t* ctx, ApePtrArray_t * params, ApeCodeblock_t* body)
{
    ApeExpression_t* res = expression_make(ctx, EXPRESSION_FUNCTION_LITERAL);
    if(!res)
    {
        return NULL;
    }
    res->fnliteral.name = NULL;
    res->fnliteral.params = params;
    res->fnliteral.body = body;
    return res;
}

ApeExpression_t* expression_make_call(ApeContext_t* ctx, ApeExpression_t* function, ApePtrArray_t * args)
{
    ApeExpression_t* res = expression_make(ctx, EXPRESSION_CALL);
    if(!res)
    {
        return NULL;
    }
    res->callexpr.function = function;
    res->callexpr.args = args;
    return res;
}

ApeExpression_t* expression_make_index(ApeContext_t* ctx, ApeExpression_t* left, ApeExpression_t* index)
{
    ApeExpression_t* res = expression_make(ctx, EXPRESSION_INDEX);
    if(!res)
    {
        return NULL;
    }
    res->indexexpr.left = left;
    res->indexexpr.index = index;
    return res;
}

ApeExpression_t* expression_make_assign(ApeContext_t* ctx, ApeExpression_t* dest, ApeExpression_t* source, bool ispostfix)
{
    ApeExpression_t* res = expression_make(ctx, EXPRESSION_ASSIGN);
    if(!res)
    {
        return NULL;
    }
    res->assign.dest = dest;
    res->assign.source = source;
    res->assign.ispostfix = ispostfix;
    return res;
}

ApeExpression_t* expression_make_logical(ApeContext_t* ctx, ApeOperator_t op, ApeExpression_t* left, ApeExpression_t* right)
{
    ApeExpression_t* res = expression_make(ctx, EXPRESSION_LOGICAL);
    if(!res)
    {
        return NULL;
    }
    res->logical.op = op;
    res->logical.left = left;
    res->logical.right = right;
    return res;
}

ApeExpression_t* expression_make_ternary(ApeContext_t* ctx, ApeExpression_t* test, ApeExpression_t* iftrue, ApeExpression_t* iffalse)
{
    ApeExpression_t* res = expression_make(ctx, EXPRESSION_TERNARY);
    if(!res)
    {
        return NULL;
    }
    res->ternary.test = test;
    res->ternary.iftrue = iftrue;
    res->ternary.iffalse = iffalse;
    return res;
}

void* expression_destroy(ApeExpression_t* expr)
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
            ident_destroy(expr->ident);
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
            ptrarray_destroywithitems(expr->array, (ApeDataCallback_t)expression_destroy);
            break;
        }
        case EXPRESSION_MAP_LITERAL:
        {
            ptrarray_destroywithitems(expr->map.keys, (ApeDataCallback_t)expression_destroy);
            ptrarray_destroywithitems(expr->map.values, (ApeDataCallback_t)expression_destroy);
            break;
        }
        case EXPRESSION_PREFIX:
        {
            expression_destroy(expr->prefix.right);
            break;
        }
        case EXPRESSION_INFIX:
        {
            expression_destroy(expr->infix.left);
            expression_destroy(expr->infix.right);
            break;
        }
        case EXPRESSION_FUNCTION_LITERAL:
        {
            ApeFnLiteral_t* fn = &expr->fnliteral;
            allocator_free(expr->alloc, fn->name);
            ptrarray_destroywithitems(fn->params, (ApeDataCallback_t)ident_destroy);
            code_block_destroy(fn->body);
            break;
        }
        case EXPRESSION_CALL:
        {
            ptrarray_destroywithitems(expr->callexpr.args, (ApeDataCallback_t)expression_destroy);
            expression_destroy(expr->callexpr.function);
            break;
        }
        case EXPRESSION_INDEX:
        {
            expression_destroy(expr->indexexpr.left);
            expression_destroy(expr->indexexpr.index);
            break;
        }
        case EXPRESSION_ASSIGN:
        {
            expression_destroy(expr->assign.dest);
            expression_destroy(expr->assign.source);
            break;
        }
        case EXPRESSION_LOGICAL:
        {
            expression_destroy(expr->logical.left);
            expression_destroy(expr->logical.right);
            break;
        }
        case EXPRESSION_TERNARY:
        {
            expression_destroy(expr->ternary.test);
            expression_destroy(expr->ternary.iftrue);
            expression_destroy(expr->ternary.iffalse);
            break;
        }
    }
    allocator_free(expr->alloc, expr);
    return NULL;
}

ApeExpression_t* expression_copy(ApeExpression_t* expr)
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
            ApeIdent_t* ident = ident_copy(expr->ident);
            if(!ident)
            {
                return NULL;
            }
            res = expression_make_ident(expr->context, ident);
            if(!res)
            {
                ident_destroy(ident);
                return NULL;
            }
            break;
        }
        case EXPRESSION_NUMBER_LITERAL:
        {
            res = expression_make_number_literal(expr->context, expr->numberliteral);
            break;
        }
        case EXPRESSION_BOOL_LITERAL:
        {
            res = expression_make_bool_literal(expr->context, expr->boolliteral);
            break;
        }
        case EXPRESSION_STRING_LITERAL:
        {
            char* stringcopy = util_strdup(expr->context, expr->stringliteral);
            if(!stringcopy)
            {
                return NULL;
            }
            res = expression_make_string_literal(expr->context, stringcopy);
            if(!res)
            {
                allocator_free(expr->alloc, stringcopy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_NULL_LITERAL:
        {
            res = expression_make_null_literal(expr->context);
            break;
        }
        case EXPRESSION_ARRAY_LITERAL:
        {
            ApePtrArray_t* valuescopy = ptrarray_copywithitems(expr->array, (ApeDataCallback_t)expression_copy, (ApeDataCallback_t)expression_destroy);
            if(!valuescopy)
            {
                return NULL;
            }
            res = expression_make_array_literal(expr->context, valuescopy);
            if(!res)
            {
                ptrarray_destroywithitems(valuescopy, (ApeDataCallback_t)expression_destroy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_MAP_LITERAL:
        {
            ApePtrArray_t* keyscopy = ptrarray_copywithitems(expr->map.keys, (ApeDataCallback_t)expression_copy, (ApeDataCallback_t)expression_destroy);
            ApePtrArray_t* valuescopy = ptrarray_copywithitems(expr->map.values, (ApeDataCallback_t)expression_copy, (ApeDataCallback_t)expression_destroy);
            if(!keyscopy || !valuescopy)
            {
                ptrarray_destroywithitems(keyscopy, (ApeDataCallback_t)expression_destroy);
                ptrarray_destroywithitems(valuescopy, (ApeDataCallback_t)expression_destroy);
                return NULL;
            }
            res = expression_make_map_literal(expr->context, keyscopy, valuescopy);
            if(!res)
            {
                ptrarray_destroywithitems(keyscopy, (ApeDataCallback_t)expression_destroy);
                ptrarray_destroywithitems(valuescopy, (ApeDataCallback_t)expression_destroy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_PREFIX:
        {
            ApeExpression_t* rightcopy = expression_copy(expr->prefix.right);
            if(!rightcopy)
            {
                return NULL;
            }
            res = expression_make_prefix(expr->context, expr->prefix.op, rightcopy);
            if(!res)
            {
                expression_destroy(rightcopy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_INFIX:
        {
            ApeExpression_t* leftcopy = expression_copy(expr->infix.left);
            ApeExpression_t* rightcopy = expression_copy(expr->infix.right);
            if(!leftcopy || !rightcopy)
            {
                expression_destroy(leftcopy);
                expression_destroy(rightcopy);
                return NULL;
            }
            res = expression_make_infix(expr->context, expr->infix.op, leftcopy, rightcopy);
            if(!res)
            {
                expression_destroy(leftcopy);
                expression_destroy(rightcopy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_FUNCTION_LITERAL:
        {
            ApePtrArray_t* paramscopy = ptrarray_copywithitems(expr->fnliteral.params, (ApeDataCallback_t)ident_copy,(ApeDataCallback_t) ident_destroy);
            ApeCodeblock_t* bodycopy = code_block_copy(expr->fnliteral.body);
            char* namecopy = util_strdup(expr->context, expr->fnliteral.name);
            if(!paramscopy || !bodycopy)
            {
                ptrarray_destroywithitems(paramscopy, (ApeDataCallback_t)ident_destroy);
                code_block_destroy(bodycopy);
                allocator_free(expr->alloc, namecopy);
                return NULL;
            }
            res = expression_make_fn_literal(expr->context, paramscopy, bodycopy);
            if(!res)
            {
                ptrarray_destroywithitems(paramscopy, (ApeDataCallback_t)ident_destroy);
                code_block_destroy(bodycopy);
                allocator_free(expr->alloc, namecopy);
                return NULL;
            }
            res->fnliteral.name = namecopy;
            break;
        }
        case EXPRESSION_CALL:
        {
            ApeExpression_t* functioncopy = expression_copy(expr->callexpr.function);
            ApePtrArray_t* argscopy = ptrarray_copywithitems(expr->callexpr.args, (ApeDataCallback_t)expression_copy, (ApeDataCallback_t)expression_destroy);
            if(!functioncopy || !argscopy)
            {
                expression_destroy(functioncopy);
                ptrarray_destroywithitems(expr->callexpr.args, (ApeDataCallback_t)expression_destroy);
                return NULL;
            }
            res = expression_make_call(expr->context, functioncopy, argscopy);
            if(!res)
            {
                expression_destroy(functioncopy);
                ptrarray_destroywithitems(expr->callexpr.args, (ApeDataCallback_t)expression_destroy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_INDEX:
        {
            ApeExpression_t* leftcopy = expression_copy(expr->indexexpr.left);
            ApeExpression_t* indexcopy = expression_copy(expr->indexexpr.index);
            if(!leftcopy || !indexcopy)
            {
                expression_destroy(leftcopy);
                expression_destroy(indexcopy);
                return NULL;
            }
            res = expression_make_index(expr->context, leftcopy, indexcopy);
            if(!res)
            {
                expression_destroy(leftcopy);
                expression_destroy(indexcopy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_ASSIGN:
        {
            ApeExpression_t* destcopy = expression_copy(expr->assign.dest);
            ApeExpression_t* sourcecopy = expression_copy(expr->assign.source);
            if(!destcopy || !sourcecopy)
            {
                expression_destroy(destcopy);
                expression_destroy(sourcecopy);
                return NULL;
            }
            res = expression_make_assign(expr->context, destcopy, sourcecopy, expr->assign.ispostfix);
            if(!res)
            {
                expression_destroy(destcopy);
                expression_destroy(sourcecopy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_LOGICAL:
        {
            ApeExpression_t* leftcopy = expression_copy(expr->logical.left);
            ApeExpression_t* rightcopy = expression_copy(expr->logical.right);
            if(!leftcopy || !rightcopy)
            {
                expression_destroy(leftcopy);
                expression_destroy(rightcopy);
                return NULL;
            }
            res = expression_make_logical(expr->context, expr->logical.op, leftcopy, rightcopy);
            if(!res)
            {
                expression_destroy(leftcopy);
                expression_destroy(rightcopy);
                return NULL;
            }
            break;
        }
        case EXPRESSION_TERNARY:
        {
            ApeExpression_t* testcopy = expression_copy(expr->ternary.test);
            ApeExpression_t* iftruecopy = expression_copy(expr->ternary.iftrue);
            ApeExpression_t* iffalsecopy = expression_copy(expr->ternary.iffalse);
            if(!testcopy || !iftruecopy || !iffalsecopy)
            {
                expression_destroy(testcopy);
                expression_destroy(iftruecopy);
                expression_destroy(iffalsecopy);
                return NULL;
            }
            res = expression_make_ternary(expr->context, testcopy, iftruecopy, iffalsecopy);
            if(!res)
            {
                expression_destroy(testcopy);
                expression_destroy(iftruecopy);
                expression_destroy(iffalsecopy);
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

ApeStatement_t* statement_make_define(ApeContext_t* ctx, ApeIdent_t* name, ApeExpression_t* value, bool assignable)
{
    ApeStatement_t* res = statement_make(ctx, STATEMENT_DEFINE);
    if(!res)
    {
        return NULL;
    }
    res->define.name = name;
    res->define.value = value;
    res->define.assignable = assignable;
    return res;
}

ApeStatement_t* statement_make_if(ApeContext_t* ctx, ApePtrArray_t * cases, ApeCodeblock_t* alternative)
{
    ApeStatement_t* res = statement_make(ctx, STATEMENT_IF);
    if(!res)
    {
        return NULL;
    }
    res->ifstatement.cases = cases;
    res->ifstatement.alternative = alternative;
    return res;
}

ApeStatement_t* statement_make_return(ApeContext_t* ctx, ApeExpression_t* value)
{
    ApeStatement_t* res = statement_make(ctx, STATEMENT_RETURN_VALUE);
    if(!res)
    {
        return NULL;
    }
    res->returnvalue = value;
    return res;
}

ApeStatement_t* statement_make_expression(ApeContext_t* ctx, ApeExpression_t* value)
{
    ApeStatement_t* res = statement_make(ctx, STATEMENT_EXPRESSION);
    if(!res)
    {
        return NULL;
    }
    res->expression = value;
    return res;
}

ApeStatement_t* statement_make_while_loop(ApeContext_t* ctx, ApeExpression_t* test, ApeCodeblock_t* body)
{
    ApeStatement_t* res = statement_make(ctx, STATEMENT_WHILE_LOOP);
    if(!res)
    {
        return NULL;
    }
    res->whileloop.test = test;
    res->whileloop.body = body;
    return res;
}

ApeStatement_t* statement_make_break(ApeContext_t* ctx)
{
    ApeStatement_t* res = statement_make(ctx, STATEMENT_BREAK);
    if(!res)
    {
        return NULL;
    }
    return res;
}

ApeStatement_t* statement_make_foreach(ApeContext_t* ctx, ApeIdent_t* iterator, ApeExpression_t* source, ApeCodeblock_t* body)
{
    ApeStatement_t* res = statement_make(ctx, STATEMENT_FOREACH);
    if(!res)
    {
        return NULL;
    }
    res->foreach.iterator = iterator;
    res->foreach.source = source;
    res->foreach.body = body;
    return res;
}

ApeStatement_t* statement_make_for_loop(ApeContext_t* ctx, ApeStatement_t* init, ApeExpression_t* test, ApeExpression_t* update, ApeCodeblock_t* body)
{
    ApeStatement_t* res = statement_make(ctx, STATEMENT_FOR_LOOP);
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

ApeStatement_t* statement_make_continue(ApeContext_t* ctx)
{
    ApeStatement_t* res = statement_make(ctx, STATEMENT_CONTINUE);
    if(!res)
    {
        return NULL;
    }
    return res;
}

ApeStatement_t* statement_make_block(ApeContext_t* ctx, ApeCodeblock_t* block)
{
    ApeStatement_t* res = statement_make(ctx, STATEMENT_BLOCK);
    if(!res)
    {
        return NULL;
    }
    res->block = block;
    return res;
}

ApeStatement_t* statement_make_import(ApeContext_t* ctx, char* path)
{
    ApeStatement_t* res = statement_make(ctx, STATEMENT_IMPORT);
    if(!res)
    {
        return NULL;
    }
    res->import.path = path;
    return res;
}

ApeStatement_t* statement_make_recover(ApeContext_t* ctx, ApeIdent_t* errorident, ApeCodeblock_t* body)
{
    ApeStatement_t* res = statement_make(ctx, STATEMENT_RECOVER);
    if(!res)
    {
        return NULL;
    }
    res->recover.errorident = errorident;
    res->recover.body = body;
    return res;
}

void* statement_destroy(ApeStatement_t* stmt)
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
                ident_destroy(stmt->define.name);
                expression_destroy(stmt->define.value);
            }
            break;
        case STATEMENT_IF:
            {
                ptrarray_destroywithitems(stmt->ifstatement.cases, (ApeDataCallback_t)if_case_destroy);
                code_block_destroy(stmt->ifstatement.alternative);
            }
            break;
        case STATEMENT_RETURN_VALUE:
            {
                expression_destroy(stmt->returnvalue);
            }
            break;
        case STATEMENT_EXPRESSION:
            {
                expression_destroy(stmt->expression);
            }
            break;
        case STATEMENT_WHILE_LOOP:
            {
                expression_destroy(stmt->whileloop.test);
                code_block_destroy(stmt->whileloop.body);
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
                ident_destroy(stmt->foreach.iterator);
                expression_destroy(stmt->foreach.source);
                code_block_destroy(stmt->foreach.body);
            }
            break;
        case STATEMENT_FOR_LOOP:
            {
                statement_destroy(stmt->forloop.init);
                expression_destroy(stmt->forloop.test);
                expression_destroy(stmt->forloop.update);
                code_block_destroy(stmt->forloop.body);
            }
            break;
        case STATEMENT_BLOCK:
            {
                code_block_destroy(stmt->block);
            }
            break;
        case STATEMENT_IMPORT:
            {
                allocator_free(stmt->alloc, stmt->import.path);
            }
            break;
        case STATEMENT_RECOVER:
            {
                code_block_destroy(stmt->recover.body);
                ident_destroy(stmt->recover.errorident);
            }
            break;
    }
    allocator_free(stmt->alloc, stmt);
    return NULL;
}

ApeStatement_t* statement_copy(const ApeStatement_t* stmt)
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
                ApeExpression_t* valuecopy = expression_copy(stmt->define.value);
                if(!valuecopy)
                {
                    return NULL;
                }
                res = statement_make_define(stmt->context, ident_copy(stmt->define.name), valuecopy, stmt->define.assignable);
                if(!res)
                {
                    expression_destroy(valuecopy);
                    return NULL;
                }
            }
            break;
        case STATEMENT_IF:
            {
                ApePtrArray_t* casescopy = ptrarray_copywithitems(stmt->ifstatement.cases, (ApeDataCallback_t)if_case_copy, (ApeDataCallback_t)if_case_destroy);
                ApeCodeblock_t* alternativecopy = code_block_copy(stmt->ifstatement.alternative);
                if(!casescopy || !alternativecopy)
                {
                    ptrarray_destroywithitems(casescopy, (ApeDataCallback_t)if_case_destroy);
                    code_block_destroy(alternativecopy);
                    return NULL;
                }
                res = statement_make_if(stmt->context, casescopy, alternativecopy);
                if(res)
                {
                    ptrarray_destroywithitems(casescopy, (ApeDataCallback_t)if_case_destroy);
                    code_block_destroy(alternativecopy);
                    return NULL;
                }
            }
            break;
        case STATEMENT_RETURN_VALUE:
            {
                ApeExpression_t* valuecopy = expression_copy(stmt->returnvalue);
                if(!valuecopy)
                {
                    return NULL;
                }
                res = statement_make_return(stmt->context, valuecopy);
                if(!res)
                {
                    expression_destroy(valuecopy);
                    return NULL;
                }
            }
            break;
        case STATEMENT_EXPRESSION:
            {
                ApeExpression_t* valuecopy = expression_copy(stmt->expression);
                if(!valuecopy)
                {
                    return NULL;
                }
                res = statement_make_expression(stmt->context, valuecopy);
                if(!res)
                {
                    expression_destroy(valuecopy);
                    return NULL;
                }
            }
            break;
        case STATEMENT_WHILE_LOOP:
            {
                ApeExpression_t* testcopy = expression_copy(stmt->whileloop.test);
                ApeCodeblock_t* bodycopy = code_block_copy(stmt->whileloop.body);
                if(!testcopy || !bodycopy)
                {
                    expression_destroy(testcopy);
                    code_block_destroy(bodycopy);
                    return NULL;
                }
                res = statement_make_while_loop(stmt->context, testcopy, bodycopy);
                if(!res)
                {
                    expression_destroy(testcopy);
                    code_block_destroy(bodycopy);
                    return NULL;
                }
            }
            break;
        case STATEMENT_BREAK:
            {
                res = statement_make_break(stmt->context);
            }
            break;
        case STATEMENT_CONTINUE:
            {
                res = statement_make_continue(stmt->context);
            }
            break;
        case STATEMENT_FOREACH:
            {
                ApeExpression_t* sourcecopy = expression_copy(stmt->foreach.source);
                ApeCodeblock_t* bodycopy = code_block_copy(stmt->foreach.body);
                if(!sourcecopy || !bodycopy)
                {
                    expression_destroy(sourcecopy);
                    code_block_destroy(bodycopy);
                    return NULL;
                }
                res = statement_make_foreach(stmt->context, ident_copy(stmt->foreach.iterator), sourcecopy, bodycopy);
                if(!res)
                {
                    expression_destroy(sourcecopy);
                    code_block_destroy(bodycopy);
                    return NULL;
                }
            }
            break;
        case STATEMENT_FOR_LOOP:
            {
                ApeStatement_t* initcopy = statement_copy(stmt->forloop.init);
                ApeExpression_t* testcopy = expression_copy(stmt->forloop.test);
                ApeExpression_t* updatecopy = expression_copy(stmt->forloop.update);
                ApeCodeblock_t* bodycopy = code_block_copy(stmt->forloop.body);
                if(!initcopy || !testcopy || !updatecopy || !bodycopy)
                {
                    statement_destroy(initcopy);
                    expression_destroy(testcopy);
                    expression_destroy(updatecopy);
                    code_block_destroy(bodycopy);
                    return NULL;
                }
                res = statement_make_for_loop(stmt->context, initcopy, testcopy, updatecopy, bodycopy);
                if(!res)
                {
                    statement_destroy(initcopy);
                    expression_destroy(testcopy);
                    expression_destroy(updatecopy);
                    code_block_destroy(bodycopy);
                    return NULL;
                }
            }
            break;
        case STATEMENT_BLOCK:
            {
                ApeCodeblock_t* block_copy = code_block_copy(stmt->block);
                if(!block_copy)
                {
                    return NULL;
                }
                res = statement_make_block(stmt->context, block_copy);
                if(!res)
                {
                    code_block_destroy(block_copy);
                    return NULL;
                }
            }
            break;
        case STATEMENT_IMPORT:
            {
                char* pathcopy = util_strdup(stmt->context, stmt->import.path);
                if(!pathcopy)
                {
                    return NULL;
                }
                res = statement_make_import(stmt->context, pathcopy);
                if(!res)
                {
                    allocator_free(stmt->alloc, pathcopy);
                    return NULL;
                }
            }
            break;
        case STATEMENT_RECOVER:
            {
                ApeCodeblock_t* bodycopy = code_block_copy(stmt->recover.body);
                ApeIdent_t* erroridentcopy = ident_copy(stmt->recover.errorident);
                if(!bodycopy || !erroridentcopy)
                {
                    code_block_destroy(bodycopy);
                    ident_destroy(erroridentcopy);
                    return NULL;
                }
                res = statement_make_recover(stmt->context, erroridentcopy, bodycopy);
                if(!res)
                {
                    code_block_destroy(bodycopy);
                    ident_destroy(erroridentcopy);
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

ApeCodeblock_t* code_block_make(ApeAllocator_t* alloc, ApePtrArray_t * statements)
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

void* code_block_destroy(ApeCodeblock_t* block)
{
    if(!block)
    {
        return NULL;
    }
    ptrarray_destroywithitems(block->statements, (ApeDataCallback_t)statement_destroy);
    allocator_free(block->alloc, block);
    return NULL;
}

ApeCodeblock_t* code_block_copy(ApeCodeblock_t* block)
{
    ApeCodeblock_t* res;
    ApePtrArray_t* statementscopy;
    if(!block)
    {
        return NULL;
    }
    statementscopy = ptrarray_copywithitems(block->statements, (ApeDataCallback_t)statement_copy, (ApeDataCallback_t)statement_destroy);
    if(!statementscopy)
    {
        return NULL;
    }
    res = code_block_make(block->alloc, statementscopy);
    if(!res)
    {
        ptrarray_destroywithitems(statementscopy, (ApeDataCallback_t)statement_destroy);
        return NULL;
    }
    return res;
}

void* ident_destroy(ApeIdent_t* ident)
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

ApeIfCase_t* if_case_make(ApeAllocator_t* alloc, ApeExpression_t* test, ApeCodeblock_t* consequence)
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

void* if_case_destroy(ApeIfCase_t* cond)
{
    if(!cond)
    {
        return NULL;
    }
    expression_destroy(cond->test);
    code_block_destroy(cond->consequence);
    allocator_free(cond->alloc, cond);
    return NULL;
}

ApeIfCase_t* if_case_copy(ApeIfCase_t* ifcase)
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
    testcopy = expression_copy(ifcase->test);
    if(!testcopy)
    {
        goto err;
    }
    consequencecopy = code_block_copy(ifcase->consequence);
    if(!testcopy || !consequencecopy)
    {
        goto err;
    }
    ifcasecopy = if_case_make(ifcase->alloc, testcopy, consequencecopy);
    if(!ifcasecopy)
    {
        goto err;
    }
    return ifcasecopy;
err:
    expression_destroy(testcopy);
    code_block_destroy(consequencecopy);
    if_case_destroy(ifcasecopy);
    return NULL;
}

// INTERNAL
ApeExpression_t* expression_make(ApeContext_t* ctx, ApeExprType_t type)
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

ApeStatement_t* statement_make(ApeContext_t* ctx, ApeStatementType_t type)
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

ApeParser_t* parser_make(ApeContext_t* ctx, const ApeConfig_t* config, ApeErrorList_t* errors)
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
        parser->rightassocparsefns[TOKEN_IDENT] = parse_identifier;
        parser->rightassocparsefns[TOKEN_NUMBER] = parse_number_literal;
        parser->rightassocparsefns[TOKEN_TRUE] = parse_bool_literal;
        parser->rightassocparsefns[TOKEN_FALSE] = parse_bool_literal;
        parser->rightassocparsefns[TOKEN_STRING] = parse_string_literal;
        parser->rightassocparsefns[TOKEN_TEMPLATE_STRING] = parse_template_string_literal;
        parser->rightassocparsefns[TOKEN_NULL] = parse_null_literal;
        parser->rightassocparsefns[TOKEN_BANG] = parse_prefix_expression;
        parser->rightassocparsefns[TOKEN_MINUS] = parse_prefix_expression;
        parser->rightassocparsefns[TOKEN_LPAREN] = parse_grouped_expression;
        parser->rightassocparsefns[TOKEN_FUNCTION] = parse_function_literal;
        parser->rightassocparsefns[TOKEN_LBRACKET] = parse_array_literal;
        parser->rightassocparsefns[TOKEN_LBRACE] = parse_map_literal;
        parser->rightassocparsefns[TOKEN_PLUS_PLUS] = parse_incdec_prefix_expression;
        parser->rightassocparsefns[TOKEN_MINUS_MINUS] = parse_incdec_prefix_expression;
    }
    {
        parser->leftassocparsefns[TOKEN_PLUS] = parse_infix_expression;
        parser->leftassocparsefns[TOKEN_MINUS] = parse_infix_expression;
        parser->leftassocparsefns[TOKEN_SLASH] = parse_infix_expression;
        parser->leftassocparsefns[TOKEN_ASTERISK] = parse_infix_expression;
        parser->leftassocparsefns[TOKEN_PERCENT] = parse_infix_expression;
        parser->leftassocparsefns[TOKEN_EQ] = parse_infix_expression;
        parser->leftassocparsefns[TOKEN_NOT_EQ] = parse_infix_expression;
        parser->leftassocparsefns[TOKEN_LT] = parse_infix_expression;
        parser->leftassocparsefns[TOKEN_LTE] = parse_infix_expression;
        parser->leftassocparsefns[TOKEN_GT] = parse_infix_expression;
        parser->leftassocparsefns[TOKEN_GTE] = parse_infix_expression;
        parser->leftassocparsefns[TOKEN_LPAREN] = parse_call_expression;
        parser->leftassocparsefns[TOKEN_LBRACKET] = parse_index_expression;
        parser->leftassocparsefns[TOKEN_ASSIGN] = parse_assign_expression;
        parser->leftassocparsefns[TOKEN_PLUS_ASSIGN] = parse_assign_expression;
        parser->leftassocparsefns[TOKEN_MINUS_ASSIGN] = parse_assign_expression;
        parser->leftassocparsefns[TOKEN_SLASH_ASSIGN] = parse_assign_expression;
        parser->leftassocparsefns[TOKEN_ASTERISK_ASSIGN] = parse_assign_expression;
        parser->leftassocparsefns[TOKEN_PERCENT_ASSIGN] = parse_assign_expression;
        parser->leftassocparsefns[TOKEN_BIT_AND_ASSIGN] = parse_assign_expression;
        parser->leftassocparsefns[TOKEN_BIT_OR_ASSIGN] = parse_assign_expression;
        parser->leftassocparsefns[TOKEN_BIT_XOR_ASSIGN] = parse_assign_expression;
        parser->leftassocparsefns[TOKEN_LSHIFT_ASSIGN] = parse_assign_expression;
        parser->leftassocparsefns[TOKEN_RSHIFT_ASSIGN] = parse_assign_expression;
        parser->leftassocparsefns[TOKEN_DOT] = parse_dot_expression;
        parser->leftassocparsefns[TOKEN_AND] = parse_logical_expression;
        parser->leftassocparsefns[TOKEN_OR] = parse_logical_expression;
        parser->leftassocparsefns[TOKEN_BIT_AND] = parse_infix_expression;
        parser->leftassocparsefns[TOKEN_BIT_OR] = parse_infix_expression;
        parser->leftassocparsefns[TOKEN_BIT_XOR] = parse_infix_expression;
        parser->leftassocparsefns[TOKEN_LSHIFT] = parse_infix_expression;
        parser->leftassocparsefns[TOKEN_RSHIFT] = parse_infix_expression;
        parser->leftassocparsefns[TOKEN_QUESTION] = parse_ternary_expression;
        parser->leftassocparsefns[TOKEN_PLUS_PLUS] = parse_incdec_postfix_expression;
        parser->leftassocparsefns[TOKEN_MINUS_MINUS] = parse_incdec_postfix_expression;
    }
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
    ok = lexer_init(&parser->lexer, parser->context, parser->errors, input, file);
    if(!ok)
    {
        return NULL;
    }
    lexer_next_token(&parser->lexer);
    lexer_next_token(&parser->lexer);
    statements = ptrarray_make(parser->context);
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
    if(errorlist_count(parser->errors) > 0)
    {
        goto err;
    }
    return statements;
err:
    ptrarray_destroywithitems(statements, (ApeDataCallback_t)statement_destroy);
    return NULL;
}

// INTERNAL
ApeStatement_t* parse_statement(ApeParser_t* p)
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
                res = parse_define_statement(p);
            }
            break;
        case TOKEN_IF:
            {
                res = parse_if_statement(p);
            }
            break;
        case TOKEN_RETURN:
            {
                res = parse_return_statement(p);
            }
            break;
        case TOKEN_WHILE:
            {
                res = parse_while_loop_statement(p);
            }
            break;
        case TOKEN_BREAK:
            {
                res = parse_break_statement(p);
            }
            break;
        case TOKEN_FOR:
            {
                res = parse_for_loop_statement(p);
            }
            break;
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
            }
            break;
        case TOKEN_LBRACE:
            {
                if(p->config->replmode && p->depth == 0)
                {
                    res = parse_expression_statement(p);
                }
                else
                {
                    res = parse_block_statement(p);
                }
            }
            break;
        case TOKEN_CONTINUE:
            {
                res = parse_continue_statement(p);
            }
            break;
        case TOKEN_IMPORT:
            {
                res = parse_import_statement(p);
            }
            break;
        case TOKEN_RECOVER:
            {
                res = parse_recover_statement(p);
            }
            break;
        default:
            {
                res = parse_expression_statement(p);
            }
            break;
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
    ApeIdent_t* nameident;
    ApeExpression_t* value;
    ApeStatement_t* res;
    nameident = NULL;
    value = NULL;
    assignable = lexer_cur_token_is(&p->lexer, TOKEN_VAR);
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_IDENT))
    {
        goto err;
    }
    nameident = ident_make(p->context, p->lexer.curtoken);
    if(!nameident)
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
        value->fnliteral.name = util_strdup(p->context, nameident->value);
        if(!value->fnliteral.name)
        {
            goto err;
        }
    }
    res = statement_make_define(p->context, nameident, value, assignable);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    expression_destroy(value);
    ident_destroy(nameident);
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
    cases = ptrarray_make(p->context);
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
    res = statement_make_if(p->context, cases, alternative);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    ptrarray_destroywithitems(cases, (ApeDataCallback_t)if_case_destroy);
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
    res = statement_make_return(p->context, expr);
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
    #if 0
    if(expr && (!p->config->replmode || p->depth > 0))
    {
        if(expr->type != EXPRESSION_ASSIGN && expr->type != EXPRESSION_CALL)
        {
            errorlist_addformat(p->errors, APE_ERROR_PARSING, expr->pos, "only assignments and function calls can be expression statements");
            expression_destroy(expr);
            return NULL;
        }
    }
    #endif
    res = statement_make_expression(p->context, expr);
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
    res = statement_make_while_loop(p->context, test, body);
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
    return statement_make_break(p->context);
}

ApeStatement_t* parse_continue_statement(ApeParser_t* p)
{
    lexer_next_token(&p->lexer);
    return statement_make_continue(p->context);
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
    res = statement_make_block(p->context, block);
    if(!res)
    {
        code_block_destroy(block);
        return NULL;
    }
    return res;
}

ApeStatement_t* parse_import_statement(ApeParser_t* p)
{
    char* processedname;
    ApeStatement_t* res;
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_STRING))
    {
        return NULL;
    }
    processedname = process_and_copy_string(p->alloc, p->lexer.curtoken.literal, p->lexer.curtoken.len);
    if(!processedname)
    {
        errorlist_add(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "error when parsing module name");
        return NULL;
    }
    lexer_next_token(&p->lexer);
    res= statement_make_import(p->context, processedname);
    if(!res)
    {
        allocator_free(p->alloc, processedname);
        return NULL;
    }
    return res;
}

ApeStatement_t* parse_recover_statement(ApeParser_t* p)
{
    ApeIdent_t* errorident;
    ApeCodeblock_t* body;
    ApeStatement_t* res;
    errorident = NULL;
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
    errorident = ident_make(p->context, p->lexer.curtoken);
    if(!errorident)
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
    res = statement_make_recover(p->context, errorident, body);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    code_block_destroy(body);
    ident_destroy(errorident);
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
    ApeIdent_t* iteratorident;
    ApeStatement_t* res;
    source = NULL;
    body = NULL;
    iteratorident = ident_make(p->context, p->lexer.curtoken);
    if(!iteratorident)
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
    res = statement_make_foreach(p->context, iteratorident, source, body);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    code_block_destroy(body);
    ident_destroy(iteratorident);
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
            errorlist_addformat(p->errors, APE_ERROR_PARSING, init->pos, "for loop's init clause should be a define statement or an expression");
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
    res = statement_make_for_loop(p->context, init, test, update, body);
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
    ApeIdent_t* nameident;
    ApeExpression_t* value;
    ApePosition_t pos;
    ApeStatement_t* res;
    value = NULL;
    nameident = NULL;
    pos = p->lexer.curtoken.pos;
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_IDENT))
    {
        goto err;
    }
    nameident = ident_make(p->context, p->lexer.curtoken);
    if(!nameident)
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
    value->fnliteral.name = util_strdup(p->context, nameident->value);
    if(!value->fnliteral.name)
    {
        goto err;
    }
    res = statement_make_define(p->context, nameident, value, false);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    expression_destroy(value);
    ident_destroy(nameident);
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
    statements = ptrarray_make(p->context);
    if(!statements)
    {
        goto err;
    }
    while(!lexer_cur_token_is(&p->lexer, TOKEN_RBRACE))
    {
        if(lexer_cur_token_is(&p->lexer, TOKEN_EOF))
        {
            errorlist_add(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "unexpected EOF");
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
    ptrarray_destroywithitems(statements, (ApeDataCallback_t)statement_destroy);
    return NULL;
}

ApeExpression_t* parse_expression(ApeParser_t* p, ApePrecedence_t prec)
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
        literal = token_duplicate_literal(p->context, &p->lexer.curtoken);
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
    while(!lexer_cur_token_is(&p->lexer, TOKEN_SEMICOLON) && prec < get_precedence(p->lexer.curtoken.type))
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
            expression_destroy(leftexpr);
            return NULL;
        }
        newleftexpr->pos = pos;
        leftexpr = newleftexpr;
    }
    return leftexpr;
}

ApeExpression_t* parse_identifier(ApeParser_t* p)
{
    ApeIdent_t* ident;
    ApeExpression_t* res;
    ident = ident_make(p->context, p->lexer.curtoken);
    if(!ident)
    {
        return NULL;
    }
    res = expression_make_ident(p->context, ident);
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
    ApeFloat_t number;
    ApeInt_t parsedlen;
    number = 0;
    errno = 0;
    number = strtod(p->lexer.curtoken.literal, &end);
    parsedlen = end - p->lexer.curtoken.literal;
    if(errno || parsedlen != (ApeInt_t)p->lexer.curtoken.len)
    {
        literal = token_duplicate_literal(p->context, &p->lexer.curtoken);
        errorlist_addformat(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "parsing number literal '%s' failed", literal);
        allocator_free(p->alloc, literal);
        return NULL;
    }
    lexer_next_token(&p->lexer);
    return expression_make_number_literal(p->context, number);
}

ApeExpression_t* parse_bool_literal(ApeParser_t* p)
{
    ApeExpression_t* res;
    res = expression_make_bool_literal(p->context, p->lexer.curtoken.type == TOKEN_TRUE);
    lexer_next_token(&p->lexer);
    return res;
}

ApeExpression_t* parse_string_literal(ApeParser_t* p)
{
    char* processedliteral;
    ApeExpression_t* res;
    processedliteral = process_and_copy_string(p->alloc, p->lexer.curtoken.literal, p->lexer.curtoken.len);
    if(!processedliteral)
    {
        errorlist_add(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "error while parsing string literal");
        return NULL;
    }
    lexer_next_token(&p->lexer);
    res = expression_make_string_literal(p->context, processedliteral);
    if(!res)
    {
        allocator_free(p->alloc, processedliteral);
        return NULL;
    }
    return res;
}

ApeExpression_t* parse_template_string_literal(ApeParser_t* p)
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
    processedliteral = process_and_copy_string(p->alloc, p->lexer.curtoken.literal, p->lexer.curtoken.len);
    if(!processedliteral)
    {
        errorlist_add(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "error while parsing string literal");
        return NULL;
    }
    lexer_next_token(&p->lexer);

    if(!lexer_expect_current(&p->lexer, TOKEN_LBRACE))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);

    pos = p->lexer.curtoken.pos;

    leftstringexpr = expression_make_string_literal(p->context, processedliteral);
    if(!leftstringexpr)
    {
        goto err;
    }
    leftstringexpr->pos = pos;
    processedliteral = NULL;
    pos = p->lexer.curtoken.pos;
    templateexpr = parse_expression(p, PRECEDENCE_LOWEST);
    if(!templateexpr)
    {
        goto err;
    }
    tostrcallexpr = wrap_expression_in_function_call(p->context, templateexpr, "tostring");
    if(!tostrcallexpr)
    {
        goto err;
    }
    tostrcallexpr->pos = pos;
    templateexpr = NULL;
    leftaddexpr = expression_make_infix(p->context, OPERATOR_PLUS, leftstringexpr, tostrcallexpr);
    if(!leftaddexpr)
    {
        goto err;
    }
    leftaddexpr->pos = pos;
    leftstringexpr = NULL;
    tostrcallexpr = NULL;
    if(!lexer_expect_current(&p->lexer, TOKEN_RBRACE))
    {
        goto err;
    }
    lexer_previous_token(&p->lexer);
    lexer_continue_template_string(&p->lexer);
    lexer_next_token(&p->lexer);
    lexer_next_token(&p->lexer);
    pos = p->lexer.curtoken.pos;
    rightexpr = parse_expression(p, PRECEDENCE_HIGHEST);
    if(!rightexpr)
    {
        goto err;
    }
    rightaddexpr = expression_make_infix(p->context, OPERATOR_PLUS, leftaddexpr, rightexpr);
    if(!rightaddexpr)
    {
        goto err;
    }
    rightaddexpr->pos = pos;
    leftaddexpr = NULL;
    rightexpr = NULL;

    return rightaddexpr;
err:
    expression_destroy(rightaddexpr);
    expression_destroy(rightexpr);
    expression_destroy(leftaddexpr);
    expression_destroy(tostrcallexpr);
    expression_destroy(templateexpr);
    expression_destroy(leftstringexpr);
    allocator_free(p->alloc, processedliteral);
    return NULL;
}

ApeExpression_t* parse_null_literal(ApeParser_t* p)
{
    lexer_next_token(&p->lexer);
    return expression_make_null_literal(p->context);
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
    res = expression_make_array_literal(p->context, array);
    if(!res)
    {
        ptrarray_destroywithitems(array, (ApeDataCallback_t)expression_destroy);
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
    keys = ptrarray_make(p->context);
    values = ptrarray_make(p->context);
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
            str = token_duplicate_literal(p->context, &p->lexer.curtoken);
            key = expression_make_string_literal(p->context, str);
            if(!key)
            {
                allocator_free(p->alloc, str);
                goto err;
            }
            key->pos = p->lexer.curtoken.pos;
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
                    }
                    break;
                default:
                    {
                        errorlist_addformat(p->errors, APE_ERROR_PARSING, key->pos, "invalid map literal key type");
                        expression_destroy(key);
                        goto err;
                    }
                    break;
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
    res = expression_make_map_literal(p->context, keys, values);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    ptrarray_destroywithitems(keys, (ApeDataCallback_t)expression_destroy);
    ptrarray_destroywithitems(values, (ApeDataCallback_t)expression_destroy);
    return NULL;
}

ApeExpression_t* parse_prefix_expression(ApeParser_t* p)
{
    ApeOperator_t op;
    ApeExpression_t* right;
    ApeExpression_t* res;
    op = token_to_operator(p->lexer.curtoken.type);
    lexer_next_token(&p->lexer);
    right = parse_expression(p, PRECEDENCE_PREFIX);
    if(!right)
    {
        return NULL;
    }
    res = expression_make_prefix(p->context, op, right);
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
    op = token_to_operator(p->lexer.curtoken.type);
    prec = get_precedence(p->lexer.curtoken.type);
    lexer_next_token(&p->lexer);
    right = parse_expression(p, prec);
    if(!right)
    {
        return NULL;
    }
    res = expression_make_infix(p->context, op, left, right);
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
    params = ptrarray_make(p->context);
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
    res = expression_make_fn_literal(p->context, params, body);
    if(!res)
    {
        goto err;
    }
    p->depth -= 1;
    return res;
err:
    code_block_destroy(body);
    ptrarray_destroywithitems(params, (ApeDataCallback_t)ident_destroy);
    p->depth -= 1;
    return NULL;
}

bool parse_function_parameters(ApeParser_t* p, ApePtrArray_t * outparams)
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
    ident = ident_make(p->context, p->lexer.curtoken);
    if(!ident)
    {
        return false;
    }
    ok = ptrarray_add(outparams, ident);
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
        ident = ident_make(p->context, p->lexer.curtoken);
        if(!ident)
        {
            return false;
        }
        ok = ptrarray_add(outparams, ident);
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
    res = expression_make_call(p->context, function, args);
    if(!res)
    {
        ptrarray_destroywithitems(args, (ApeDataCallback_t)expression_destroy);
        return NULL;
    }
    return res;
}

ApePtrArray_t* parse_expression_list(ApeParser_t* p, ApeTokenType_t starttoken, ApeTokenType_t endtoken, bool trailingcommaallowed)
{
    bool ok;
    ApePtrArray_t* res;
    ApeExpression_t* argexpr;
    if(!lexer_expect_current(&p->lexer, starttoken))
    {
        return NULL;
    }
    lexer_next_token(&p->lexer);
    res = ptrarray_make(p->context);
    if(lexer_cur_token_is(&p->lexer, endtoken))
    {
        lexer_next_token(&p->lexer);
        return res;
    }
    argexpr = parse_expression(p, PRECEDENCE_LOWEST);
    if(!argexpr)
    {
        goto err;
    }
    ok = ptrarray_add(res, argexpr);
    if(!ok)
    {
        expression_destroy(argexpr);
        goto err;
    }
    while(lexer_cur_token_is(&p->lexer, TOKEN_COMMA))
    {
        lexer_next_token(&p->lexer);
        if(trailingcommaallowed && lexer_cur_token_is(&p->lexer, endtoken))
        {
            break;
        }
        argexpr = parse_expression(p, PRECEDENCE_LOWEST);
        if(!argexpr)
        {
            goto err;
        }
        ok = ptrarray_add(res, argexpr);
        if(!ok)
        {
            expression_destroy(argexpr);
            goto err;
        }
    }
    if(!lexer_expect_current(&p->lexer, endtoken))
    {
        goto err;
    }
    lexer_next_token(&p->lexer);
    return res;
err:
    ptrarray_destroywithitems(res, (ApeDataCallback_t)expression_destroy);
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
    res = expression_make_index(p->context, left, index);
    if(!res)
    {
        expression_destroy(index);
        return NULL;
    }
    return res;
}

ApeExpression_t* parse_assign_expression(ApeParser_t* p, ApeExpression_t* left)
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
    lexer_next_token(&p->lexer);
    source = parse_expression(p, PRECEDENCE_LOWEST);
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
                op = token_to_operator(assigntype);
                leftcopy = expression_copy(left);
                if(!leftcopy)
                {
                    goto err;
                }
                pos = source->pos;
                newsource = expression_make_infix(p->context, op, leftcopy, source);
                if(!newsource)
                {
                    expression_destroy(leftcopy);
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
    res = expression_make_assign(p->context, left, source, false);
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
    op = token_to_operator(p->lexer.curtoken.type);
    prec = get_precedence(p->lexer.curtoken.type);
    lexer_next_token(&p->lexer);
    right = parse_expression(p, prec);
    if(!right)
    {
        return NULL;
    }
    res = expression_make_logical(p->context, op, left, right);
    if(!res)
    {
        expression_destroy(right);
        return NULL;
    }
    return res;
}

ApeExpression_t* parse_ternary_expression(ApeParser_t* p, ApeExpression_t* left)
{
    ApeExpression_t* iftrue;
    ApeExpression_t* iffalse;
    ApeExpression_t* res;
    lexer_next_token(&p->lexer);
    iftrue = parse_expression(p, PRECEDENCE_LOWEST);
    if(!iftrue)
    {
        return NULL;
    }
    if(!lexer_expect_current(&p->lexer, TOKEN_COLON))
    {
        expression_destroy(iftrue);
        return NULL;
    }
    lexer_next_token(&p->lexer);
    iffalse = parse_expression(p, PRECEDENCE_LOWEST);
    if(!iffalse)
    {
        expression_destroy(iftrue);
        return NULL;
    }
    res = expression_make_ternary(p->context, left, iftrue, iffalse);
    if(!res)
    {
        expression_destroy(iftrue);
        expression_destroy(iffalse);
        return NULL;
    }
    return res;
}

ApeExpression_t* parse_incdec_prefix_expression(ApeParser_t* p)
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
    lexer_next_token(&p->lexer);
    op = token_to_operator(operationtype);
    dest = parse_expression(p, PRECEDENCE_PREFIX);
    if(!dest)
    {
        goto err;
    }
    oneliteral = expression_make_number_literal(p->context, 1);
    if(!oneliteral)
    {
        expression_destroy(dest);
        goto err;
    }
    oneliteral->pos = pos;
    destcopy = expression_copy(dest);
    if(!destcopy)
    {
        expression_destroy(oneliteral);
        expression_destroy(dest);
        goto err;
    }
    operation = expression_make_infix(p->context, op, destcopy, oneliteral);
    if(!operation)
    {
        expression_destroy(destcopy);
        expression_destroy(dest);
        expression_destroy(oneliteral);
        goto err;
    }
    operation->pos = pos;

    res = expression_make_assign(p->context, dest, operation, false);
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
    lexer_next_token(&p->lexer);
    op = token_to_operator(operationtype);
    leftcopy = expression_copy(left);
    if(!leftcopy)
    {
        goto err;
    }
    oneliteral = expression_make_number_literal(p->context, 1);
    if(!oneliteral)
    {
        expression_destroy(leftcopy);
        goto err;
    }
    oneliteral->pos = pos;
    operation = expression_make_infix(p->context, op, leftcopy, oneliteral);
    if(!operation)
    {
        expression_destroy(oneliteral);
        expression_destroy(leftcopy);
        goto err;
    }
    operation->pos = pos;
    res = expression_make_assign(p->context, left, operation, true);
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
    char* str;
    ApeExpression_t* index;
    ApeExpression_t* res;
    lexer_next_token(&p->lexer);
    if(!lexer_expect_current(&p->lexer, TOKEN_IDENT))
    {
        return NULL;
    }
    str = token_duplicate_literal(p->context, &p->lexer.curtoken);
    index = expression_make_string_literal(p->context, str);
    if(!index)
    {
        allocator_free(p->alloc, str);
        return NULL;
    }
    index->pos = p->lexer.curtoken.pos;
    lexer_next_token(&p->lexer);
    res = expression_make_index(p->context, left, index);
    if(!res)
    {
        expression_destroy(index);
        return NULL;
    }
    return res;
}

static ApePrecedence_t get_precedence(ApeTokenType_t tk)
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

static ApeOperator_t token_to_operator(ApeTokenType_t tk)
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

static char escape_char(const char c)
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

static char* process_and_copy_string(ApeAllocator_t* alloc, const char* input, size_t len)
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
            output[outi] = escape_char(input[ini]);
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

static ApeExpression_t* wrap_expression_in_function_call(ApeContext_t* ctx, ApeExpression_t* expr, const char* functionname)
{
    bool ok;
    ApeExpression_t* callexpr;
    ApeIdent_t* ident;
    ApeExpression_t* functionidentexpr;
    ApePtrArray_t* args;
    ApeToken_t fntoken;
    token_init(&fntoken, TOKEN_IDENT, functionname, (int)strlen(functionname));
    fntoken.pos = expr->pos;
    ident = ident_make(ctx, fntoken);
    if(!ident)
    {
        return NULL;
    }
    ident->pos = fntoken.pos;
    functionidentexpr = expression_make_ident(ctx, ident);
    if(!functionidentexpr)
    {
        ident_destroy(ident);
        return NULL;
    }
    functionidentexpr->pos = expr->pos;
    ident = NULL;
    args = ptrarray_make(ctx);
    if(!args)
    {
        expression_destroy(functionidentexpr);
        return NULL;
    }
    ok = ptrarray_add(args, expr);
    if(!ok)
    {
        ptrarray_destroy(args);
        expression_destroy(functionidentexpr);
        return NULL;
    }
    callexpr = expression_make_call(ctx, functionidentexpr, args);
    if(!callexpr)
    {
        ptrarray_destroy(args);
        expression_destroy(functionidentexpr);
        return NULL;
    }
    callexpr->pos = expr->pos;
    return callexpr;
}


