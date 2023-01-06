
#include "inline.h"

static const ApePosition_t g_prspriv_srcposinvalid = { NULL, -1, -1 };

ApeAstIdentExpr_t* ape_ast_make_ident(ApeContext_t* ctx, ApeAstToken_t tok)
{
    ApeAstIdentExpr_t* res;
    res = (ApeAstIdentExpr_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstIdentExpr_t));
    if(!res)
    {
        return NULL;
    }
    res->context = ctx;
    res->value = ape_lexer_tokendupliteral(ctx, &tok);
    if(!res->value)
    {
        ape_allocator_free(&ctx->alloc, res);
        return NULL;
    }
    res->pos = tok.pos;
    return res;
}

ApeAstExpression_t* ape_ast_make_expression(ApeContext_t* ctx, ApeAstExprType_t type)
{
    ApeAstExpression_t* res;
    res = (ApeAstExpression_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstExpression_t));
    if(!res)
    {
        return NULL;
    }
    res->context = ctx;
    res->extype = type;
    res->pos = g_prspriv_srcposinvalid;
    return res;
}

ApeAstExpression_t* ape_ast_make_identexpr(ApeContext_t* ctx, ApeAstIdentExpr_t* ident)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_IDENT);
    if(!res)
    {
        return NULL;
    }
    res->exident = ident;
    return res;
}

ApeAstExpression_t* ape_ast_make_literalnumberexpr(ApeContext_t* ctx, ApeFloat_t val)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_LITERALNUMBER);
    if(!res)
    {
        return NULL;
    }
    res->exliteralnumber = val;
    return res;
}

ApeAstExpression_t* ape_ast_make_literalboolexpr(ApeContext_t* ctx, bool val)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_LITERALBOOL);
    if(!res)
    {
        return NULL;
    }
    res->exliteralbool = val;
    return res;
}

ApeAstExpression_t* ape_ast_make_literalstringexpr(ApeContext_t* ctx, char* value, ApeSize_t len, bool wasallocd)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_LITERALSTRING);
    if(!res)
    {
        return NULL;
    }
    res->stringwasallocd = wasallocd;
    res->exliteralstring = value;
    res->stringlitlength = len;
    return res;
}

ApeAstExpression_t* ape_ast_make_literalnullexpr(ApeContext_t* ctx)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_LITERALNULL);
    if(!res)
    {
        return NULL;
    }
    return res;
}

ApeAstExpression_t* ape_ast_make_literalarrayexpr(ApeContext_t* ctx, ApePtrArray_t * values)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_LITERALARRAY);
    if(!res)
    {
        return NULL;
    }
    res->exarray = values;
    return res;
}

ApeAstExpression_t* ape_ast_make_literalmapexpr(ApeContext_t* ctx, ApePtrArray_t * keys, ApePtrArray_t * values)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_LITERALMAP);
    if(!res)
    {
        return NULL;
    }
    res->exmap.keys = keys;
    res->exmap.values = values;
    return res;
}

ApeAstExpression_t* ape_ast_make_prefixexpr(ApeContext_t* ctx, ApeOperator_t op, ApeAstExpression_t* right)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_PREFIX);
    if(!res)
    {
        return NULL;
    }
    res->exprefix.op = op;
    res->exprefix.right = right;
    return res;
}

ApeAstExpression_t* ape_ast_make_infixexpr(ApeContext_t* ctx, ApeOperator_t op, ApeAstExpression_t* left, ApeAstExpression_t* right)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_INFIX);
    if(!res)
    {
        return NULL;
    }
    res->exinfix.op = op;
    res->exinfix.left = left;
    res->exinfix.right = right;
    return res;
}

ApeAstExpression_t* ape_ast_make_literalfuncexpr(ApeContext_t* ctx, ApePtrArray_t * params, ApeAstBlockExpr_t* body)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_LITERALFUNCTION);
    if(!res)
    {
        return NULL;
    }
    res->exliteralfunc.name = NULL;
    res->exliteralfunc.params = params;
    res->exliteralfunc.body = body;
    return res;
}

ApeAstExpression_t* ape_ast_make_callexpr(ApeContext_t* ctx, ApeAstExpression_t* function, ApePtrArray_t * args)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_CALL);
    if(!res)
    {
        return NULL;
    }
    res->excall.function = function;
    res->excall.args = args;
    return res;
}

ApeAstExpression_t* ape_ast_make_indexexpr(ApeContext_t* ctx, ApeAstExpression_t* left, ApeAstExpression_t* index)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_INDEX);
    if(!res)
    {
        return NULL;
    }
    res->exindex.left = left;
    res->exindex.index = index;
    return res;
}

ApeAstExpression_t* ape_ast_make_assignexpr(ApeContext_t* ctx, ApeAstExpression_t* dest, ApeAstExpression_t* source, bool ispostfix)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_ASSIGN);
    if(!res)
    {
        return NULL;
    }
    res->exassign.dest = dest;
    res->exassign.source = source;
    res->exassign.ispostfix = ispostfix;
    return res;
}

ApeAstExpression_t* ape_ast_make_logicalexpr(ApeContext_t* ctx, ApeOperator_t op, ApeAstExpression_t* left, ApeAstExpression_t* right)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_LOGICAL);
    if(!res)
    {
        return NULL;
    }
    res->exlogical.op = op;
    res->exlogical.left = left;
    res->exlogical.right = right;
    return res;
}

ApeAstExpression_t* ape_ast_make_ternaryexpr(ApeContext_t* ctx, ApeAstExpression_t* test, ApeAstExpression_t* iftrue, ApeAstExpression_t* iffalse)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_TERNARY);
    if(!res)
    {
        return NULL;
    }
    res->externary.test = test;
    res->externary.iftrue = iftrue;
    res->externary.iffalse = iffalse;
    return res;
}

ApeAstExpression_t* ape_ast_make_definestmt(ApeContext_t* ctx, ApeAstIdentExpr_t* name, ApeAstExpression_t* value, bool assignable)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_DEFINE);
    if(!res)
    {
        return NULL;
    }
    res->exdefine.name = name;
    res->exdefine.value = value;
    res->exdefine.assignable = assignable;
    return res;
}

ApeAstExpression_t* ape_ast_make_ifstmt(ApeContext_t* ctx, ApePtrArray_t * cases, ApeAstBlockExpr_t* alternative)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_IFELSE);
    if(!res)
    {
        return NULL;
    }
    res->exifstmt.cases = cases;
    res->exifstmt.alternative = alternative;
    return res;
}

ApeAstExpression_t* ape_ast_make_returnstmt(ApeContext_t* ctx, ApeAstExpression_t* value)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_RETURNVALUE);
    if(!res)
    {
        return NULL;
    }
    res->exreturn = value;
    return res;
}

ApeAstExpression_t* ape_ast_make_expressionstmt(ApeContext_t* ctx, ApeAstExpression_t* value)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_EXPRESSION);
    if(!res)
    {
        return NULL;
    }
    res->exexpression = value;
    return res;
}

ApeAstExpression_t* ape_ast_make_whilestmt(ApeContext_t* ctx, ApeAstExpression_t* test, ApeAstBlockExpr_t* body)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_WHILELOOP);
    if(!res)
    {
        return NULL;
    }
    res->exwhilestmt.test = test;
    res->exwhilestmt.body = body;
    return res;
}

ApeAstExpression_t* ape_ast_make_breakstmt(ApeContext_t* ctx)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_BREAK);
    if(!res)
    {
        return NULL;
    }
    return res;
}

ApeAstExpression_t* ape_ast_make_foreachstmt(ApeContext_t* ctx, ApeAstIdentExpr_t* iterator, ApeAstExpression_t* source, ApeAstBlockExpr_t* body)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_FOREACH);
    if(!res)
    {
        return NULL;
    }
    res->exforeachstmt.iterator = iterator;
    res->exforeachstmt.source = source;
    res->exforeachstmt.body = body;
    return res;
}

ApeAstExpression_t* ape_ast_make_forstmt(ApeContext_t* ctx, ApeAstExpression_t* init, ApeAstExpression_t* test, ApeAstExpression_t* update, ApeAstBlockExpr_t* body)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_FORLOOP);
    if(!res)
    {
        return NULL;
    }
    res->exforstmt.init = init;
    res->exforstmt.test = test;
    res->exforstmt.update = update;
    res->exforstmt.body = body;
    return res;
}

ApeAstExpression_t* ape_ast_make_continuestmt(ApeContext_t* ctx)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_CONTINUE);
    if(!res)
    {
        return NULL;
    }
    return res;
}

ApeAstExpression_t* ape_ast_make_blockstmt(ApeContext_t* ctx, ApeAstBlockExpr_t* block)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_BLOCK);
    if(!res)
    {
        return NULL;
    }
    res->exblock = block;
    return res;
}

ApeAstExpression_t* ape_ast_make_includestmt(ApeContext_t* ctx, char* path)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_INCLUDE);
    if(!res)
    {
        return NULL;
    }
    res->exincludestmt.path = path;
    return res;
}

ApeAstExpression_t* ape_ast_make_recoverstmt(ApeContext_t* ctx, ApeAstIdentExpr_t* errorident, ApeAstBlockExpr_t* body)
{
    ApeAstExpression_t* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_RECOVER);
    if(!res)
    {
        return NULL;
    }
    res->exrecoverstmt.errorident = errorident;
    res->exrecoverstmt.body = body;
    return res;
}

ApeAstBlockExpr_t* ape_ast_make_codeblock(ApeContext_t* ctx, ApePtrArray_t * statements)
{
    ApeAstBlockExpr_t* block;
    block = (ApeAstBlockExpr_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstBlockExpr_t));
    if(!block)
    {
        return NULL;
    }
    block->context = ctx;
    block->statements = statements;
    return block;
}

ApeAstIdentExpr_t* ape_ast_copy_ident(ApeContext_t* ctx, ApeAstIdentExpr_t* ident)
{
    ApeAstIdentExpr_t* res;
    res = (ApeAstIdentExpr_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstIdentExpr_t));
    if(!res)
    {
        return NULL;
    }
    res->context = ctx;
    res->value = ape_util_strdup(ctx, ident->value);
    if(!res->value)
    {
        ape_allocator_free(&ctx->alloc, res);
        return NULL;
    }
    res->pos = ident->pos;
    return res;
}

ApeAstBlockExpr_t* ape_ast_copy_codeblock(ApeContext_t* ctx, ApeAstBlockExpr_t* block)
{
    ApeAstBlockExpr_t* res;
    ApePtrArray_t* statementscopy;
    ApeDataCallback_t copyfn;
    ApeDataCallback_t destroyfn;
    copyfn = (ApeDataCallback_t)ape_ast_copy_expr;
    destroyfn = (ApeDataCallback_t)ape_ast_destroy_expr;
    if(!block)
    {
        return NULL;
    }
    statementscopy = ape_ptrarray_copywithitems(ctx, block->statements, copyfn, destroyfn);
    if(!statementscopy)
    {
        return NULL;
    }
    res = ape_ast_make_codeblock(ctx, statementscopy);
    if(!res)
    {
        ape_ptrarray_destroywithitems(ctx, statementscopy, destroyfn);
        return NULL;
    }
    res->context = ctx;
    return res;
}

ApeAstIfCaseExpr_t* ape_ast_copy_ifcase(ApeContext_t* ctx, ApeAstIfCaseExpr_t* ifcase)
{
    ApeAstExpression_t* testcopy;
    ApeAstBlockExpr_t* consequencecopy;
    ApeAstIfCaseExpr_t* ifcasecopy;
    if(!ifcase)
    {
        return NULL;
    }
    testcopy = NULL;
    consequencecopy = NULL;
    ifcasecopy = NULL;
    testcopy = ape_ast_copy_expr(ctx, ifcase->test);
    if(!testcopy)
    {
        goto err;
    }
    consequencecopy = ape_ast_copy_codeblock(ctx, ifcase->consequence);
    if(!testcopy || !consequencecopy)
    {
        goto err;
    }
    ifcasecopy = ape_ast_make_ifcase(ctx, testcopy, consequencecopy);
    if(!ifcasecopy)
    {
        goto err;
    }
    ifcasecopy->context = ctx;
    return ifcasecopy;
err:
    ape_ast_destroy_expr(ctx, testcopy);
    ape_ast_destroy_codeblock(consequencecopy);
    ape_ast_destroy_ifcase(ctx, ifcasecopy);
    return NULL;
}


ApeAstCompFile_t* ape_make_compfile(ApeContext_t* ctx, const char* path)
{
    size_t len;
    const char* last_slash_pos;
    ApeAstCompFile_t* file;
    file = (ApeAstCompFile_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstCompFile_t));
    if(!file)
    {
        return NULL;
    }
    memset(file, 0, sizeof(ApeAstCompFile_t));
    file->context = ctx;
    last_slash_pos = strrchr(path, '/');
    if(last_slash_pos)
    {
        len = last_slash_pos - path + 1;
        file->dirpath = ape_util_strndup(ctx, path, len);
    }
    else
    {
        file->dirpath = ape_util_strdup(ctx, "");
    }
    if(!file->dirpath)
    {
        goto error;
    }
    file->path = ape_util_strdup(ctx, path);
    if(!file->path)
    {
        goto error;
    }
    file->lines = ape_make_ptrarray(ctx);
    if(!file->lines)
    {
        goto error;
    }
    return file;
error:
    ape_compfile_destroy(ctx, file);
    return NULL;
}

void* ape_compfile_destroy(ApeContext_t* ctx, ApeAstCompFile_t* file)
{
    ApeSize_t i;
    void* item;
    if(!file)
    {
        return NULL;
    }
    for(i = 0; i < ape_ptrarray_count(file->lines); i++)
    {
        item = (void*)ape_ptrarray_get(file->lines, i);
        ape_allocator_free(&ctx->alloc, item);
    }
    ape_ptrarray_destroy(file->lines);
    ape_allocator_free(&ctx->alloc, file->dirpath);
    ape_allocator_free(&ctx->alloc, file->path);
    ape_allocator_free(&ctx->alloc, file);
    return NULL;
}

ApeAstCompScope_t* ape_make_compscope(ApeContext_t* ctx, ApeAstCompScope_t* outer)
{
    ApeAstCompScope_t* scope;
    scope = (ApeAstCompScope_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstCompScope_t));
    if(!scope)
    {
        return NULL;
    }
    memset(scope, 0, sizeof(ApeAstCompScope_t));
    scope->context = ctx;
    scope->outer = outer;
    scope->bytecode = ape_make_valarray(ctx, sizeof(ApeUShort_t));
    if(!scope->bytecode)
    {
        goto err;
    }
    scope->srcpositions = ape_make_valarray(ctx, sizeof(ApePosition_t));
    if(!scope->srcpositions)
    {
        goto err;
    }
    scope->breakipstack = ape_make_valarray(ctx, sizeof(ApeInt_t));
    if(!scope->breakipstack)
    {
        goto err;
    }
    scope->continueipstack = ape_make_valarray(ctx, sizeof(int));
    if(!scope->continueipstack)
    {
        goto err;
    }
    return scope;
err:
    ape_compscope_destroy(scope);
    return NULL;
}

void ape_compscope_destroy(ApeAstCompScope_t* scope)
{
    ApeContext_t* ctx;
    ctx = scope->context;
    ape_valarray_destroy(scope->continueipstack);
    ape_valarray_destroy(scope->breakipstack);
    ape_valarray_destroy(scope->bytecode);
    ape_valarray_destroy(scope->srcpositions);
    ape_allocator_free(&ctx->alloc, scope);
}

ApeAstCompResult_t* ape_compscope_orphanresult(ApeAstCompScope_t* scope)
{
    ApeAstCompResult_t* res;
    res = ape_make_compresult(scope->context,
        (ApeUShort_t*)ape_valarray_data(scope->bytecode),
        (ApePosition_t*)ape_valarray_data(scope->srcpositions),
        ape_valarray_count(scope->bytecode)
    );
    if(!res)
    {
        return NULL;
    }
    ape_valarray_reset(scope->bytecode);
    ape_valarray_reset(scope->srcpositions);
    return res;
}

ApeAstCompResult_t* ape_make_compresult(ApeContext_t* ctx, ApeUShort_t* bytecode, ApePosition_t* src_positions, int count)
{
    ApeAstCompResult_t* res;
    res = (ApeAstCompResult_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstCompResult_t));
    if(!res)
    {
        return NULL;
    }
    memset(res, 0, sizeof(ApeAstCompResult_t));
    res->context = ctx;
    res->bytecode = bytecode;
    res->srcpositions = src_positions;
    res->count = count;
    return res;
}

void ape_compresult_destroy(ApeAstCompResult_t* res)
{
    ApeContext_t* ctx;
    if(res == NULL)
    {
        return;
    }
    ctx = res->context;
    ape_allocator_free(&ctx->alloc, res->bytecode);
    ape_allocator_free(&ctx->alloc, res->srcpositions);
    ape_allocator_free(&ctx->alloc, res);
}


ApeAstBlockScope_t* ape_make_blockscope(ApeContext_t* ctx, int offset)
{
    ApeAstBlockScope_t* sc;
    sc = (ApeAstBlockScope_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstBlockScope_t));
    if(!sc)
    {
        return NULL;
    }
    memset(sc, 0, sizeof(ApeAstBlockScope_t));
    sc->context = ctx;
    sc->store = ape_make_strdict(ctx, (ApeDataCallback_t)ape_symbol_copy, (ApeDataCallback_t)ape_symbol_destroy);
    if(!sc->store)
    {
        ape_blockscope_destroy(ctx, sc);
        return NULL;
    }
    sc->numdefinitions = 0;
    sc->offset = offset;
    return sc;
}

void* ape_blockscope_destroy(ApeContext_t* ctx, ApeAstBlockScope_t* scope)
{
    if(scope != NULL)
    {
        if(scope->store != NULL)
        {
            ape_strdict_destroywithitems(ctx, scope->store);
        }
        ape_allocator_free(&ctx->alloc, scope);
    }
    return NULL;
}

ApeAstBlockScope_t* ape_blockscope_copy(ApeContext_t* ctx, ApeAstBlockScope_t* scope)
{
    ApeAstBlockScope_t* copy;
    if(scope == NULL)
    {
        return NULL;
    }
    copy = (ApeAstBlockScope_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstBlockScope_t));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(ApeAstBlockScope_t));
    copy->context = ctx;
    copy->numdefinitions = scope->numdefinitions;
    copy->offset = scope->offset;
    if(copy->store != NULL)
    {
        copy->store = ape_strdict_copywithitems(ctx, scope->store);
        if(!copy->store)
        {
            ape_blockscope_destroy(ctx, copy);
            return NULL;
        }
    }
    return copy;
}


