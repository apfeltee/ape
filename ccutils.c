
#include "inline.h"

static const ApePosition g_prspriv_srcposinvalid = { NULL, -1, -1 };

ApeAstIdentExpr* ape_ast_make_ident(ApeContext* ctx, ApeAstToken tok)
{
    ApeAstIdentExpr* res;
    res = (ApeAstIdentExpr*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstIdentExpr));
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

ApeAstExpression* ape_ast_make_expression(ApeContext* ctx, ApeAstExprType type)
{
    ApeAstExpression* res;
    res = (ApeAstExpression*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstExpression));
    if(!res)
    {
        return NULL;
    }
    res->context = ctx;
    res->extype = type;
    res->pos = g_prspriv_srcposinvalid;
    return res;
}

ApeAstExpression* ape_ast_make_identexpr(ApeContext* ctx, ApeAstIdentExpr* ident)
{
    ApeAstExpression* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_IDENT);
    if(!res)
    {
        return NULL;
    }
    res->exident = ident;
    return res;
}

ApeAstExpression* ape_ast_make_literalnumberexpr(ApeContext* ctx, ApeFloat val)
{
    ApeAstExpression* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_LITERALNUMBER);
    if(!res)
    {
        return NULL;
    }
    res->exliteralnumber = val;
    return res;
}

ApeAstExpression* ape_ast_make_literalboolexpr(ApeContext* ctx, bool val)
{
    ApeAstExpression* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_LITERALBOOL);
    if(!res)
    {
        return NULL;
    }
    res->exliteralbool = val;
    return res;
}

ApeAstExpression* ape_ast_make_literalstringexpr(ApeContext* ctx, char* value, ApeSize len, bool wasallocd)
{
    ApeAstExpression* res;
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

ApeAstExpression* ape_ast_make_literalnullexpr(ApeContext* ctx)
{
    ApeAstExpression* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_LITERALNULL);
    if(!res)
    {
        return NULL;
    }
    return res;
}

ApeAstExpression* ape_ast_make_literalarrayexpr(ApeContext* ctx, ApePtrArray * values)
{
    ApeAstExpression* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_LITERALARRAY);
    if(!res)
    {
        return NULL;
    }
    res->exarray = values;
    return res;
}

ApeAstExpression* ape_ast_make_literalmapexpr(ApeContext* ctx, ApePtrArray * keys, ApePtrArray * values)
{
    ApeAstExpression* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_LITERALMAP);
    if(!res)
    {
        return NULL;
    }
    res->exmap.keys = keys;
    res->exmap.values = values;
    return res;
}

ApeAstExpression* ape_ast_make_prefixexpr(ApeContext* ctx, ApeOperator op, ApeAstExpression* right)
{
    ApeAstExpression* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_PREFIX);
    if(!res)
    {
        return NULL;
    }
    res->exprefix.op = op;
    res->exprefix.right = right;
    return res;
}

ApeAstExpression* ape_ast_make_infixexpr(ApeContext* ctx, ApeOperator op, ApeAstExpression* left, ApeAstExpression* right)
{
    ApeAstExpression* res;
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

ApeAstExpression* ape_ast_make_literalfuncexpr(ApeContext* ctx, ApePtrArray * params, ApeAstBlockExpr* body)
{
    ApeAstExpression* res;
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

ApeAstExpression* ape_ast_make_callexpr(ApeContext* ctx, ApeAstExpression* function, ApePtrArray * args)
{
    ApeAstExpression* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_CALL);
    if(!res)
    {
        return NULL;
    }
    res->excall.function = function;
    res->excall.args = args;
    return res;
}

ApeAstExpression* ape_ast_make_indexexpr(ApeContext* ctx, ApeAstExpression* left, ApeAstExpression* index)
{
    ApeAstExpression* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_INDEX);
    if(!res)
    {
        return NULL;
    }
    res->exindex.left = left;
    res->exindex.index = index;
    return res;
}

ApeAstExpression* ape_ast_make_assignexpr(ApeContext* ctx, ApeAstExpression* dest, ApeAstExpression* source, bool ispostfix)
{
    ApeAstExpression* res;
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

ApeAstExpression* ape_ast_make_logicalexpr(ApeContext* ctx, ApeOperator op, ApeAstExpression* left, ApeAstExpression* right)
{
    ApeAstExpression* res;
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

ApeAstExpression* ape_ast_make_ternaryexpr(ApeContext* ctx, ApeAstExpression* test, ApeAstExpression* iftrue, ApeAstExpression* iffalse)
{
    ApeAstExpression* res;
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

ApeAstExpression* ape_ast_make_definestmt(ApeContext* ctx, ApeAstIdentExpr* name, ApeAstExpression* value, bool assignable)
{
    ApeAstExpression* res;
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

ApeAstExpression* ape_ast_make_ifstmt(ApeContext* ctx, ApePtrArray * cases, ApeAstBlockExpr* alternative)
{
    ApeAstExpression* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_IFELSE);
    if(!res)
    {
        return NULL;
    }
    res->exifstmt.cases = cases;
    res->exifstmt.alternative = alternative;
    return res;
}

ApeAstExpression* ape_ast_make_returnstmt(ApeContext* ctx, ApeAstExpression* value)
{
    ApeAstExpression* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_RETURNVALUE);
    if(!res)
    {
        return NULL;
    }
    res->exreturn = value;
    return res;
}

ApeAstExpression* ape_ast_make_expressionstmt(ApeContext* ctx, ApeAstExpression* value)
{
    ApeAstExpression* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_EXPRESSION);
    if(!res)
    {
        return NULL;
    }
    res->exexpression = value;
    return res;
}

ApeAstExpression* ape_ast_make_whilestmt(ApeContext* ctx, ApeAstExpression* test, ApeAstBlockExpr* body)
{
    ApeAstExpression* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_WHILELOOP);
    if(!res)
    {
        return NULL;
    }
    res->exwhilestmt.test = test;
    res->exwhilestmt.body = body;
    return res;
}

ApeAstExpression* ape_ast_make_breakstmt(ApeContext* ctx)
{
    ApeAstExpression* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_BREAK);
    if(!res)
    {
        return NULL;
    }
    return res;
}

ApeAstExpression* ape_ast_make_foreachstmt(ApeContext* ctx, ApeAstIdentExpr* iterator, ApeAstExpression* source, ApeAstBlockExpr* body)
{
    ApeAstExpression* res;
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

ApeAstExpression* ape_ast_make_forstmt(ApeContext* ctx, ApeAstExpression* init, ApeAstExpression* test, ApeAstExpression* update, ApeAstBlockExpr* body)
{
    ApeAstExpression* res;
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

ApeAstExpression* ape_ast_make_continuestmt(ApeContext* ctx)
{
    ApeAstExpression* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_CONTINUE);
    if(!res)
    {
        return NULL;
    }
    return res;
}

ApeAstExpression* ape_ast_make_blockstmt(ApeContext* ctx, ApeAstBlockExpr* block)
{
    ApeAstExpression* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_BLOCK);
    if(!res)
    {
        return NULL;
    }
    res->exblock = block;
    return res;
}

ApeAstExpression* ape_ast_make_includestmt(ApeContext* ctx, char* path)
{
    ApeAstExpression* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_INCLUDE);
    if(!res)
    {
        return NULL;
    }
    res->exincludestmt.path = path;
    return res;
}

ApeAstExpression* ape_ast_make_recoverstmt(ApeContext* ctx, ApeAstIdentExpr* errorident, ApeAstBlockExpr* body)
{
    ApeAstExpression* res;
    res = ape_ast_make_expression(ctx, APE_EXPR_RECOVER);
    if(!res)
    {
        return NULL;
    }
    res->exrecoverstmt.errorident = errorident;
    res->exrecoverstmt.body = body;
    return res;
}

ApeAstBlockExpr* ape_ast_make_codeblock(ApeContext* ctx, ApePtrArray * statements)
{
    ApeAstBlockExpr* block;
    block = (ApeAstBlockExpr*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstBlockExpr));
    if(!block)
    {
        return NULL;
    }
    block->context = ctx;
    block->statements = statements;
    return block;
}

ApeAstIdentExpr* ape_ast_copy_ident(ApeContext* ctx, ApeAstIdentExpr* ident)
{
    ApeAstIdentExpr* res;
    res = (ApeAstIdentExpr*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstIdentExpr));
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

ApeAstBlockExpr* ape_ast_copy_codeblock(ApeContext* ctx, ApeAstBlockExpr* block)
{
    ApeAstBlockExpr* res;
    ApePtrArray* statementscopy;
    ApeDataCallback copyfn;
    ApeDataCallback destroyfn;
    copyfn = (ApeDataCallback)ape_ast_copy_expr;
    destroyfn = (ApeDataCallback)ape_ast_destroy_expr;
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

ApeAstIfCaseExpr* ape_ast_copy_ifcase(ApeContext* ctx, ApeAstIfCaseExpr* ifcase)
{
    ApeAstExpression* testcopy;
    ApeAstBlockExpr* consequencecopy;
    ApeAstIfCaseExpr* ifcasecopy;
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


ApeAstCompFile* ape_make_compfile(ApeContext* ctx, const char* path)
{
    size_t len;
    const char* last_slash_pos;
    ApeAstCompFile* file;
    file = (ApeAstCompFile*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstCompFile));
    if(!file)
    {
        return NULL;
    }
    memset(file, 0, sizeof(ApeAstCompFile));
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

void* ape_compfile_destroy(ApeContext* ctx, ApeAstCompFile* file)
{
    ApeSize i;
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

ApeAstCompScope* ape_make_compscope(ApeContext* ctx, ApeAstCompScope* outer)
{
    ApeAstCompScope* scope;
    scope = (ApeAstCompScope*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstCompScope));
    if(!scope)
    {
        return NULL;
    }
    memset(scope, 0, sizeof(ApeAstCompScope));
    scope->context = ctx;
    scope->outer = outer;
    scope->bytecode = ape_make_valarray(ctx, sizeof(ApeUShort));
    if(!scope->bytecode)
    {
        goto err;
    }
    scope->srcpositions = ape_make_valarray(ctx, sizeof(ApePosition));
    if(!scope->srcpositions)
    {
        goto err;
    }
    scope->breakipstack = ape_make_valarray(ctx, sizeof(ApeInt));
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

void ape_compscope_destroy(ApeAstCompScope* scope)
{
    ApeContext* ctx;
    ctx = scope->context;
    ape_valarray_destroy(scope->continueipstack);
    ape_valarray_destroy(scope->breakipstack);
    ape_valarray_destroy(scope->bytecode);
    ape_valarray_destroy(scope->srcpositions);
    ape_allocator_free(&ctx->alloc, scope);
}

ApeAstCompResult* ape_compscope_orphanresult(ApeAstCompScope* scope)
{
    ApeAstCompResult* res;
    res = ape_make_compresult(scope->context,
        (ApeUShort*)ape_valarray_data(scope->bytecode),
        (ApePosition*)ape_valarray_data(scope->srcpositions),
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

ApeAstCompResult* ape_make_compresult(ApeContext* ctx, ApeUShort* bytecode, ApePosition* src_positions, int count)
{
    ApeAstCompResult* res;
    res = (ApeAstCompResult*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstCompResult));
    if(!res)
    {
        return NULL;
    }
    memset(res, 0, sizeof(ApeAstCompResult));
    res->context = ctx;
    res->bytecode = bytecode;
    res->srcpositions = src_positions;
    res->count = count;
    return res;
}

void ape_compresult_destroy(ApeAstCompResult* res)
{
    ApeContext* ctx;
    if(res == NULL)
    {
        return;
    }
    ctx = res->context;
    ape_allocator_free(&ctx->alloc, res->bytecode);
    ape_allocator_free(&ctx->alloc, res->srcpositions);
    ape_allocator_free(&ctx->alloc, res);
}


ApeAstBlockScope* ape_make_blockscope(ApeContext* ctx, int offset)
{
    ApeAstBlockScope* sc;
    sc = (ApeAstBlockScope*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstBlockScope));
    if(!sc)
    {
        return NULL;
    }
    memset(sc, 0, sizeof(ApeAstBlockScope));
    sc->context = ctx;
    sc->store = ape_make_strdict(ctx, (ApeDataCallback)ape_symbol_copy, (ApeDataCallback)ape_symbol_destroy);
    if(!sc->store)
    {
        ape_blockscope_destroy(ctx, sc);
        return NULL;
    }
    sc->numdefinitions = 0;
    sc->offset = offset;
    return sc;
}

void* ape_blockscope_destroy(ApeContext* ctx, ApeAstBlockScope* scope)
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

ApeAstBlockScope* ape_blockscope_copy(ApeContext* ctx, ApeAstBlockScope* scope)
{
    ApeAstBlockScope* copy;
    if(scope == NULL)
    {
        return NULL;
    }
    copy = (ApeAstBlockScope*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstBlockScope));
    if(!copy)
    {
        return NULL;
    }
    memset(copy, 0, sizeof(ApeAstBlockScope));
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


