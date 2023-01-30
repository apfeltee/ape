
#include "inline.h"

static const ApePosition g_prspriv_srcposinvalid = { NULL, -1, -1 };

static ApeRightAssocParseFNCallback rightassocparsefns[TOKEN_TYPE_MAX + 1];
static ApeLeftAssocParseFNCallback  leftassocparsefns[TOKEN_TYPE_MAX + 1];


ApeAstParser* ape_ast_make_parser(ApeContext* ctx, const ApeConfig* config, ApeErrorList* errors)
{
    ApeAstParser* parser;
    parser = (ApeAstParser*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstParser));
    if(!parser)
    {
        return NULL;
    }
    memset(parser, 0, sizeof(ApeAstParser));
    parser->context = ctx;
    parser->config = config;
    parser->errors = errors;
    {
        rightassocparsefns[TOKEN_VALIDENT] = ape_parser_parseident;
        rightassocparsefns[TOKEN_VALNUMBER] = ape_parser_parseliteralnumber;
        rightassocparsefns[TOKEN_KWTRUE] = ape_parser_parseliteralbool;
        rightassocparsefns[TOKEN_KWFALSE] = ape_parser_parseliteralbool;
        rightassocparsefns[TOKEN_VALSTRING] = ape_parser_parseliteralstring;
        rightassocparsefns[TOKEN_VALTPLSTRING] = ape_parser_parseliteraltplstring;
        rightassocparsefns[TOKEN_KWNULL] = ape_parser_parseliteralnull;
        rightassocparsefns[TOKEN_OPNOT] = ape_parser_parseprefixexpr;
        rightassocparsefns[TOKEN_OPMINUS] = ape_parser_parseprefixexpr;
        rightassocparsefns[TOKEN_OPBITNOT] = ape_parser_parseprefixexpr;

        rightassocparsefns[TOKEN_OPLEFTPAREN] = ape_parser_parsegroupedexpr;
        rightassocparsefns[TOKEN_KWFUNCTION] = ape_parser_parseliteralfunc;
        rightassocparsefns[TOKEN_OPLEFTBRACKET] = ape_parser_parseliteralarray;
        rightassocparsefns[TOKEN_OPLEFTBRACE] = ape_parser_parseliteralmap;
        rightassocparsefns[TOKEN_OPINCREASE] = ape_parser_parseincdecprefixexpr;
        rightassocparsefns[TOKEN_OPDECREASE] = ape_parser_parseincdecprefixexpr;
    }
    {
        leftassocparsefns[TOKEN_OPPLUS] = ape_parser_parseinfixexpr;
        leftassocparsefns[TOKEN_OPMINUS] = ape_parser_parseinfixexpr;
        leftassocparsefns[TOKEN_OPSLASH] = ape_parser_parseinfixexpr;
        leftassocparsefns[TOKEN_OPSTAR] = ape_parser_parseinfixexpr;
        leftassocparsefns[TOKEN_OPMODULO] = ape_parser_parseinfixexpr;
        leftassocparsefns[TOKEN_OPEQUAL] = ape_parser_parseinfixexpr;
        leftassocparsefns[TOKEN_OPNOTEQUAL] = ape_parser_parseinfixexpr;
        leftassocparsefns[TOKEN_OPLESSTHAN] = ape_parser_parseinfixexpr;
        leftassocparsefns[TOKEN_OPLESSEQUAL] = ape_parser_parseinfixexpr;
        leftassocparsefns[TOKEN_OPGREATERTHAN] = ape_parser_parseinfixexpr;
        leftassocparsefns[TOKEN_OPGREATERTEQUAL] = ape_parser_parseinfixexpr;
        leftassocparsefns[TOKEN_OPLEFTPAREN] = ape_parser_parsecallexpr;
        leftassocparsefns[TOKEN_OPLEFTBRACKET] = ape_parser_parseindexexpr;
        leftassocparsefns[TOKEN_ASSIGN] = ape_parser_parseassignexpr;
        leftassocparsefns[TOKEN_ASSIGNPLUS] = ape_parser_parseassignexpr;
        leftassocparsefns[TOKEN_ASSIGNMINUS] = ape_parser_parseassignexpr;
        leftassocparsefns[TOKEN_ASSIGNSLASH] = ape_parser_parseassignexpr;
        leftassocparsefns[TOKEN_ASSIGNSTAR] = ape_parser_parseassignexpr;
        leftassocparsefns[TOKEN_ASSIGNMODULO] = ape_parser_parseassignexpr;
        leftassocparsefns[TOKEN_ASSIGNBITAND] = ape_parser_parseassignexpr;
        leftassocparsefns[TOKEN_ASSIGNBITOR] = ape_parser_parseassignexpr;
        leftassocparsefns[TOKEN_ASSIGNBITXOR] = ape_parser_parseassignexpr;
        leftassocparsefns[TOKEN_ASSIGNLEFTSHIFT] = ape_parser_parseassignexpr;
        leftassocparsefns[TOKEN_ASSIGNRIGHTSHIFT] = ape_parser_parseassignexpr;
        leftassocparsefns[TOKEN_OPDOT] = ape_parser_parsedotexpr;
        leftassocparsefns[TOKEN_OPAND] = ape_parser_parselogicalexpr;
        leftassocparsefns[TOKEN_OPOR] = ape_parser_parselogicalexpr;
        leftassocparsefns[TOKEN_OPBITAND] = ape_parser_parseinfixexpr;
        leftassocparsefns[TOKEN_OPBITOR] = ape_parser_parseinfixexpr;
        leftassocparsefns[TOKEN_OPBITXOR] = ape_parser_parseinfixexpr;
        leftassocparsefns[TOKEN_OPLEFTSHIFT] = ape_parser_parseinfixexpr;
        leftassocparsefns[TOKEN_OPRIGHTSHIFT] = ape_parser_parseinfixexpr;
        leftassocparsefns[TOKEN_OPQUESTION] = ape_parser_parseternaryexpr;
        leftassocparsefns[TOKEN_OPINCREASE] = ape_parser_parseincdecpostfixexpr;
        leftassocparsefns[TOKEN_OPDECREASE] = ape_parser_parseincdecpostfixexpr;
    }
    parser->depth = 0;
    return parser;
}

void ape_parser_destroy(ApeAstParser* parser)
{
    if(!parser)
    {
        return;
    }
    ape_allocator_free(&parser->context->alloc, parser);
}

ApePtrArray * ape_parser_parseall(ApeAstParser* parser, const char* input, size_t inlen, ApeAstCompFile* file)
{
    bool ok;
    ApeContext* ctx;
    ApePtrArray* statements;
    ApeAstExpression* stmt;
    parser->depth = 0;
    ctx = parser->context;
    ok = ape_lexer_init(&parser->lexer, parser->context, parser->errors, input, inlen, file);
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
        ok = ape_ptrarray_push(statements, &stmt);
        if(!ok)
        {
            ape_ast_destroy_expr(ctx, stmt);
            goto err;
        }
    }
    if(ape_errorlist_count(parser->errors) > 0)
    {
        goto err;
    }
    return statements;
err:
    ape_ptrarray_destroywithitems(parser->context, statements, (ApeDataCallback)ape_ast_destroy_expr);
    return NULL;
}

ApeAstExpression* ape_ast_copy_expr(ApeContext* ctx, ApeAstExpression* expr)
{
    char* pathcopy;
    char* stringcopy;
    char* namecopy;
    ApeAstIdentExpr* ident;
    ApePtrArray* valuescopy;
    ApePtrArray* keyscopy;
    ApePtrArray* paramscopy;
    ApeAstBlockExpr* bodycopy;
    ApeAstExpression* functioncopy;
    ApePtrArray* argscopy;
    ApeAstExpression* rightcopy;
    ApeAstExpression* leftcopy;
    ApeAstExpression* indexcopy;
    ApeAstExpression* destcopy;
    ApeAstExpression* sourcecopy;
    ApeAstExpression* iftruecopy;
    ApeAstExpression* iffalsecopy;
    ApeAstExpression* initcopy;
    ApeAstExpression* testcopy;
    ApeAstExpression* updatecopy;
    ApeAstBlockExpr* blockcopy;
    ApeAstIdentExpr* erroridentcopy;
    ApeAstBlockExpr* alternativecopy;
    ApePtrArray* casescopy;
    ApeAstExpression* valuecopy;
    ApeAstExpression* res;
    ApeDataCallback copyfn;
    ApeDataCallback destroyfn;
    if(!expr)
    {
        return NULL;
    }
    //fprintf(stderr, "copying expr (%s)\n", ape_tostring_exprtype(expr->extype));
    res = NULL;
    switch(expr->extype)
    {
        case APE_EXPR_NONE:
            {
                APE_ASSERT(false);
            }
            break;
        case APE_EXPR_IDENT:
            {
                ident = ape_ast_copy_ident(ctx, expr->exident);
                if(!ident)
                {
                    return NULL;
                }
                res = ape_ast_make_identexpr(ctx, ident);
                if(!res)
                {
                    ape_ast_destroy_ident(ctx, ident);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_LITERALNUMBER:
            {
                res = ape_ast_make_literalnumberexpr(ctx, expr->exliteralnumber);
            }
            break;
        case APE_EXPR_LITERALBOOL:
            {
                res = ape_ast_make_literalboolexpr(ctx, expr->exliteralbool);
            }
            break;
        case APE_EXPR_LITERALSTRING:
            {
                stringcopy = ape_util_strndup(ctx, expr->exliteralstring, expr->stringlitlength);
                if(!stringcopy)
                {
                    return NULL;
                }
                res = ape_ast_make_literalstringexpr(ctx, stringcopy, expr->stringlitlength, true);
                if(!res)
                {
                    ape_allocator_free(&ctx->alloc, stringcopy);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_LITERALNULL:
            {
                res = ape_ast_make_literalnullexpr(ctx);
            }
            break;
        case APE_EXPR_LITERALARRAY:
            {
                copyfn = (ApeDataCallback)ape_ast_copy_expr;
                destroyfn = (ApeDataCallback)ape_ast_destroy_expr;
                valuescopy = ape_ptrarray_copywithitems(ctx, expr->exarray, copyfn, destroyfn);
                if(!valuescopy)
                {
                    return NULL;
                }
                res = ape_ast_make_literalarrayexpr(ctx, valuescopy);
                if(!res)
                {
                    ape_ptrarray_destroywithitems(ctx, valuescopy, destroyfn);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_LITERALMAP:
            {
                copyfn = (ApeDataCallback)ape_ast_copy_expr;
                destroyfn = (ApeDataCallback)ape_ast_destroy_expr;
                keyscopy = ape_ptrarray_copywithitems(ctx, expr->exmap.keys, copyfn, destroyfn);
                valuescopy = ape_ptrarray_copywithitems(ctx, expr->exmap.values, copyfn, destroyfn);
                if(!keyscopy || !valuescopy)
                {
                    ape_ptrarray_destroywithitems(ctx, keyscopy, destroyfn);
                    ape_ptrarray_destroywithitems(ctx, valuescopy, destroyfn);
                    return NULL;
                }
                res = ape_ast_make_literalmapexpr(ctx, keyscopy, valuescopy);
                if(!res)
                {
                    ape_ptrarray_destroywithitems(ctx, keyscopy, destroyfn);
                    ape_ptrarray_destroywithitems(ctx, valuescopy, destroyfn);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_PREFIX:
            {
                rightcopy = ape_ast_copy_expr(ctx, expr->exprefix.right);
                if(!rightcopy)
                {
                    return NULL;
                }
                res = ape_ast_make_prefixexpr(ctx, expr->exprefix.op, rightcopy);
                if(!res)
                {
                    ape_ast_destroy_expr(ctx, rightcopy);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_INFIX:
            {
                leftcopy = ape_ast_copy_expr(ctx, expr->exinfix.left);
                rightcopy = ape_ast_copy_expr(ctx, expr->exinfix.right);
                if(!leftcopy || !rightcopy)
                {
                    ape_ast_destroy_expr(ctx, leftcopy);
                    ape_ast_destroy_expr(ctx, rightcopy);
                    return NULL;
                }
                res = ape_ast_make_infixexpr(ctx, expr->exinfix.op, leftcopy, rightcopy);
                if(!res)
                {
                    ape_ast_destroy_expr(ctx, leftcopy);
                    ape_ast_destroy_expr(ctx, rightcopy);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_LITERALFUNCTION:
            {
                copyfn = (ApeDataCallback)ape_ast_copy_ident;
                destroyfn = (ApeDataCallback)ape_ast_destroy_ident;
                paramscopy = ape_ptrarray_copywithitems(ctx, expr->exliteralfunc.params, copyfn, destroyfn);
                bodycopy = ape_ast_copy_codeblock(ctx, expr->exliteralfunc.body);
                namecopy = ape_util_strdup(ctx, expr->exliteralfunc.name);
                if(!paramscopy || !bodycopy)
                {
                    ape_ptrarray_destroywithitems(ctx, paramscopy, destroyfn);
                    ape_ast_destroy_codeblock(bodycopy);
                    ape_allocator_free(&ctx->alloc, namecopy);
                    return NULL;
                }
                res = ape_ast_make_literalfuncexpr(ctx, paramscopy, bodycopy);
                if(!res)
                {
                    ape_ptrarray_destroywithitems(ctx, paramscopy, destroyfn);
                    ape_ast_destroy_codeblock(bodycopy);
                    ape_allocator_free(&ctx->alloc, namecopy);
                    return NULL;
                }
                res->exliteralfunc.name = namecopy;
            }
            break;
        case APE_EXPR_CALL:
            {
                functioncopy = ape_ast_copy_expr(ctx, expr->excall.function);
                copyfn = (ApeDataCallback)ape_ast_copy_expr;
                destroyfn = (ApeDataCallback)ape_ast_destroy_expr;
                argscopy = ape_ptrarray_copywithitems(ctx, expr->excall.args, copyfn, destroyfn);
                if(!functioncopy || !argscopy)
                {
                    ape_ast_destroy_expr(ctx, functioncopy);
                    ape_ptrarray_destroywithitems(ctx, expr->excall.args, destroyfn);
                    return NULL;
                }
                res = ape_ast_make_callexpr(ctx, functioncopy, argscopy);
                if(!res)
                {
                    ape_ast_destroy_expr(ctx, functioncopy);
                    ape_ptrarray_destroywithitems(ctx, expr->excall.args, destroyfn);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_INDEX:
            {
                leftcopy = ape_ast_copy_expr(ctx, expr->exindex.left);
                indexcopy = ape_ast_copy_expr(ctx, expr->exindex.index);
                if(!leftcopy || !indexcopy)
                {
                    ape_ast_destroy_expr(ctx, leftcopy);
                    ape_ast_destroy_expr(ctx, indexcopy);
                    return NULL;
                }
                res = ape_ast_make_indexexpr(ctx, leftcopy, indexcopy);
                if(!res)
                {
                    ape_ast_destroy_expr(ctx, leftcopy);
                    ape_ast_destroy_expr(ctx, indexcopy);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_ASSIGN:
            {
                destcopy = ape_ast_copy_expr(ctx, expr->exassign.dest);
                sourcecopy = ape_ast_copy_expr(ctx, expr->exassign.source);
                if(!destcopy || !sourcecopy)
                {
                    ape_ast_destroy_expr(ctx, destcopy);
                    ape_ast_destroy_expr(ctx, sourcecopy);
                    return NULL;
                }
                res = ape_ast_make_assignexpr(ctx, destcopy, sourcecopy, expr->exassign.ispostfix);
                if(!res)
                {
                    ape_ast_destroy_expr(ctx, destcopy);
                    ape_ast_destroy_expr(ctx, sourcecopy);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_LOGICAL:
            {
                leftcopy = ape_ast_copy_expr(ctx, expr->exlogical.left);
                rightcopy = ape_ast_copy_expr(ctx, expr->exlogical.right);
                if(!leftcopy || !rightcopy)
                {
                    ape_ast_destroy_expr(ctx, leftcopy);
                    ape_ast_destroy_expr(ctx, rightcopy);
                    return NULL;
                }
                res = ape_ast_make_logicalexpr(ctx, expr->exlogical.op, leftcopy, rightcopy);
                if(!res)
                {
                    ape_ast_destroy_expr(ctx, leftcopy);
                    ape_ast_destroy_expr(ctx, rightcopy);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_TERNARY:
            {
                testcopy = ape_ast_copy_expr(ctx, expr->externary.test);
                iftruecopy = ape_ast_copy_expr(ctx, expr->externary.iftrue);
                iffalsecopy = ape_ast_copy_expr(ctx, expr->externary.iffalse);
                if(!testcopy || !iftruecopy || !iffalsecopy)
                {
                    ape_ast_destroy_expr(ctx, testcopy);
                    ape_ast_destroy_expr(ctx, iftruecopy);
                    ape_ast_destroy_expr(ctx, iffalsecopy);
                    return NULL;
                }
                res = ape_ast_make_ternaryexpr(ctx, testcopy, iftruecopy, iffalsecopy);
                if(!res)
                {
                    ape_ast_destroy_expr(ctx, testcopy);
                    ape_ast_destroy_expr(ctx, iftruecopy);
                    ape_ast_destroy_expr(ctx, iffalsecopy);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_DEFINE:
            {
                valuecopy = ape_ast_copy_expr(ctx, expr->exdefine.value);
                if(!valuecopy)
                {
                    return NULL;
                }
                res = ape_ast_make_definestmt(ctx, ape_ast_copy_ident(ctx, expr->exdefine.name), valuecopy, expr->exdefine.assignable);
                if(!res)
                {
                    ape_ast_destroy_expr(ctx, valuecopy);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_IFELSE:
            {
                copyfn = (ApeDataCallback)ape_ast_copy_ifcase;
                destroyfn = (ApeDataCallback)ape_ast_destroy_ifcase;
                casescopy = ape_ptrarray_copywithitems(ctx, expr->exifstmt.cases, copyfn, destroyfn);
                alternativecopy = ape_ast_copy_codeblock(ctx, expr->exifstmt.alternative);
                if(!casescopy || !alternativecopy)
                {
                    ape_ptrarray_destroywithitems(ctx, casescopy, destroyfn);
                    ape_ast_destroy_codeblock(alternativecopy);
                    return NULL;
                }
                res = ape_ast_make_ifstmt(ctx, casescopy, alternativecopy);
                if(res)
                {
                    ape_ptrarray_destroywithitems(ctx, casescopy, destroyfn);
                    ape_ast_destroy_codeblock(alternativecopy);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_RETURNVALUE:
            {
                valuecopy = ape_ast_copy_expr(ctx, expr->exreturn);
                if(!valuecopy)
                {
                    return NULL;
                }
                res = ape_ast_make_returnstmt(ctx, valuecopy);
                if(!res)
                {
                    ape_ast_destroy_expr(ctx, valuecopy);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_EXPRESSION:
            {
                valuecopy = ape_ast_copy_expr(ctx, expr->exexpression);
                if(!valuecopy)
                {
                    return NULL;
                }
                res = ape_ast_make_expressionstmt(ctx, valuecopy);
                if(!res)
                {
                    ape_ast_destroy_expr(ctx, valuecopy);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_WHILELOOP:
            {
                testcopy = ape_ast_copy_expr(ctx, expr->exwhilestmt.test);
                bodycopy = ape_ast_copy_codeblock(ctx, expr->exwhilestmt.body);
                if(!testcopy || !bodycopy)
                {
                    ape_ast_destroy_expr(ctx, testcopy);
                    ape_ast_destroy_codeblock(bodycopy);
                    return NULL;
                }
                res = ape_ast_make_whilestmt(ctx, testcopy, bodycopy);
                if(!res)
                {
                    ape_ast_destroy_expr(ctx, testcopy);
                    ape_ast_destroy_codeblock(bodycopy);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_BREAK:
            {
                res = ape_ast_make_breakstmt(ctx);
            }
            break;
        case APE_EXPR_CONTINUE:
            {
                res = ape_ast_make_continuestmt(ctx);
            }
            break;
        case APE_EXPR_FOREACH:
            {
                sourcecopy = ape_ast_copy_expr(ctx, expr->exforeachstmt.source);
                bodycopy = ape_ast_copy_codeblock(ctx, expr->exforeachstmt.body);
                if(!sourcecopy || !bodycopy)
                {
                    ape_ast_destroy_expr(ctx, sourcecopy);
                    ape_ast_destroy_codeblock(bodycopy);
                    return NULL;
                }
                res = ape_ast_make_foreachstmt(ctx, ape_ast_copy_ident(ctx, expr->exforeachstmt.iterator), sourcecopy, bodycopy);
                if(!res)
                {
                    ape_ast_destroy_expr(ctx, sourcecopy);
                    ape_ast_destroy_codeblock(bodycopy);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_FORLOOP:
            {
                initcopy = ape_ast_copy_expr(ctx, expr->exforstmt.init);
                testcopy = ape_ast_copy_expr(ctx, expr->exforstmt.test);
                updatecopy = ape_ast_copy_expr(ctx, expr->exforstmt.update);
                bodycopy = ape_ast_copy_codeblock(ctx, expr->exforstmt.body);
                if(!initcopy || !testcopy || !updatecopy || !bodycopy)
                {
                    ape_ast_destroy_expr(ctx, initcopy);
                    ape_ast_destroy_expr(ctx, testcopy);
                    ape_ast_destroy_expr(ctx, updatecopy);
                    ape_ast_destroy_codeblock(bodycopy);
                    return NULL;
                }
                res = ape_ast_make_forstmt(ctx, initcopy, testcopy, updatecopy, bodycopy);
                if(!res)
                {
                    ape_ast_destroy_expr(ctx, initcopy);
                    ape_ast_destroy_expr(ctx, testcopy);
                    ape_ast_destroy_expr(ctx, updatecopy);
                    ape_ast_destroy_codeblock(bodycopy);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_BLOCK:
            {
                blockcopy = ape_ast_copy_codeblock(ctx, expr->exblock);
                if(!blockcopy)
                {
                    return NULL;
                }
                res = ape_ast_make_blockstmt(ctx, blockcopy);
                if(!res)
                {
                    ape_ast_destroy_codeblock(blockcopy);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_INCLUDE:
            {
                pathcopy = ape_util_strdup(ctx, expr->exincludestmt.path);
                if(!pathcopy)
                {
                    return NULL;
                }
                res = ape_ast_make_includestmt(ctx, pathcopy);
                if(!res)
                {
                    ape_allocator_free(&ctx->alloc, pathcopy);
                    return NULL;
                }
            }
            break;
        case APE_EXPR_RECOVER:
            {
                bodycopy = ape_ast_copy_codeblock(ctx, expr->exrecoverstmt.body);
                erroridentcopy = ape_ast_copy_ident(ctx, expr->exrecoverstmt.errorident);
                if(!bodycopy || !erroridentcopy)
                {
                    ape_ast_destroy_codeblock(bodycopy);
                    ape_ast_destroy_ident(ctx, erroridentcopy);
                    return NULL;
                }
                res = ape_ast_make_recoverstmt(ctx, erroridentcopy, bodycopy);
                if(!res)
                {
                    ape_ast_destroy_codeblock(bodycopy);
                    ape_ast_destroy_ident(ctx, erroridentcopy);
                    return NULL;
                }
            }
            break;
    }
    if(!res)
    {
        return NULL;
    }
    res->pos = expr->pos;
    return res;
}






ApeAstIfCaseExpr* ape_ast_make_ifcase(ApeContext* ctx, ApeAstExpression* test, ApeAstBlockExpr* consequence)
{
    ApeAstIfCaseExpr* res;
    res = (ApeAstIfCaseExpr*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeAstIfCaseExpr));
    if(!res)
    {
        return NULL;
    }
    res->context = ctx;
    res->test = test;
    res->consequence = consequence;
    return res;
}

void* ape_ast_destroy_ifcase(ApeContext* ctx, ApeAstIfCaseExpr* cond)
{
    if(!cond)
    {
        return NULL;
    }
    ape_ast_destroy_expr(ctx, cond->test);
    ape_ast_destroy_codeblock(cond->consequence);
    ape_allocator_free(&ctx->alloc, cond);
    return NULL;
}

void* ape_ast_destroy_expr(ApeContext* ctx, ApeAstExpression* expr)
{
    ApeAstLiteralFuncExpr* fn;
    ApeDataCallback destroyfn;
    if(!expr)
    {
        return NULL;
    }
    switch(expr->extype)
    {
        case APE_EXPR_NONE:
            {
                APE_ASSERT(false);
            }
            break;
        case APE_EXPR_IDENT:
            {
                ape_ast_destroy_ident(ctx, expr->exident);
            }
            break;
        case APE_EXPR_LITERALNUMBER:
        case APE_EXPR_LITERALBOOL:
            {
            }
            break;
        case APE_EXPR_LITERALSTRING:
            {
                if(expr->stringwasallocd)
                {
                    ape_allocator_free(&expr->context->alloc, expr->exliteralstring);
                }
            }
            break;
        case APE_EXPR_LITERALNULL:
            {
            }
            break;
        case APE_EXPR_LITERALARRAY:
            {
                ape_ptrarray_destroywithitems(ctx, expr->exarray, (ApeDataCallback)ape_ast_destroy_expr);
            }
            break;
        case APE_EXPR_LITERALMAP:
            {
                destroyfn = (ApeDataCallback)ape_ast_destroy_expr;
                ape_ptrarray_destroywithitems(ctx, expr->exmap.keys, destroyfn);
                ape_ptrarray_destroywithitems(ctx, expr->exmap.values, destroyfn);
            }
            break;
        case APE_EXPR_PREFIX:
            {
                ape_ast_destroy_expr(ctx, expr->exprefix.right);
            }
            break;
        case APE_EXPR_INFIX:
            {
                ape_ast_destroy_expr(ctx, expr->exinfix.left);
                ape_ast_destroy_expr(ctx, expr->exinfix.right);
            }
            break;
        case APE_EXPR_LITERALFUNCTION:
            {
                fn = &expr->exliteralfunc;
                ape_allocator_free(&expr->context->alloc, fn->name);
                ape_ptrarray_destroywithitems(ctx, fn->params, (ApeDataCallback)ape_ast_destroy_ident);
                ape_ast_destroy_codeblock(fn->body);
            }
            break;
        case APE_EXPR_CALL:
            {
                ape_ptrarray_destroywithitems(ctx, expr->excall.args, (ApeDataCallback)ape_ast_destroy_expr);
                ape_ast_destroy_expr(ctx, expr->excall.function);
            }
            break;
        case APE_EXPR_INDEX:
            {
                ape_ast_destroy_expr(ctx, expr->exindex.left);
                ape_ast_destroy_expr(ctx, expr->exindex.index);
            }
            break;
        case APE_EXPR_ASSIGN:
            {
                ape_ast_destroy_expr(ctx, expr->exassign.dest);
                ape_ast_destroy_expr(ctx, expr->exassign.source);
            }
            break;
        case APE_EXPR_LOGICAL:
            {
                ape_ast_destroy_expr(ctx, expr->exlogical.left);
                ape_ast_destroy_expr(ctx, expr->exlogical.right);
            }
            break;
        case APE_EXPR_TERNARY:
            {
                ape_ast_destroy_expr(ctx, expr->externary.test);
                ape_ast_destroy_expr(ctx, expr->externary.iftrue);
                ape_ast_destroy_expr(ctx, expr->externary.iffalse);
            }
            break;
        case APE_EXPR_DEFINE:
            {
                ape_ast_destroy_ident(ctx, expr->exdefine.name);
                ape_ast_destroy_expr(ctx, expr->exdefine.value);
            }
            break;
        case APE_EXPR_IFELSE:
            {
                ape_ptrarray_destroywithitems(ctx, expr->exifstmt.cases, (ApeDataCallback)ape_ast_destroy_ifcase);
                ape_ast_destroy_codeblock(expr->exifstmt.alternative);
            }
            break;
        case APE_EXPR_RETURNVALUE:
            {
                ape_ast_destroy_expr(ctx, expr->exreturn);
            }
            break;
        case APE_EXPR_EXPRESSION:
            {
                ape_ast_destroy_expr(ctx, expr->exexpression);
            }
            break;
        case APE_EXPR_WHILELOOP:
            {
                ape_ast_destroy_expr(ctx, expr->exwhilestmt.test);
                ape_ast_destroy_codeblock(expr->exwhilestmt.body);
            }
            break;
        case APE_EXPR_BREAK:
            {
            }
            break;
        case APE_EXPR_CONTINUE:
            {
            }
            break;
        case APE_EXPR_FOREACH:
            {
                ape_ast_destroy_ident(ctx, expr->exforeachstmt.iterator);
                ape_ast_destroy_expr(ctx, expr->exforeachstmt.source);
                ape_ast_destroy_codeblock(expr->exforeachstmt.body);
            }
            break;
        case APE_EXPR_FORLOOP:
            {
                ape_ast_destroy_expr(ctx, expr->exforstmt.init);
                ape_ast_destroy_expr(ctx, expr->exforstmt.test);
                ape_ast_destroy_expr(ctx, expr->exforstmt.update);
                ape_ast_destroy_codeblock(expr->exforstmt.body);
            }
            break;
        case APE_EXPR_BLOCK:
            {
                ape_ast_destroy_codeblock(expr->exblock);
            }
            break;
        case APE_EXPR_INCLUDE:
            {
                ape_allocator_free(&expr->context->alloc, expr->exincludestmt.path);
            }
            break;
        case APE_EXPR_RECOVER:
            {
                ape_ast_destroy_codeblock(expr->exrecoverstmt.body);
                ape_ast_destroy_ident(ctx, expr->exrecoverstmt.errorident);
            }
            break;
    }
    ape_allocator_free(&expr->context->alloc, expr);
    return NULL;
}


void* ape_ast_destroy_codeblock(ApeAstBlockExpr* block)
{
    ApeContext* ctx;
    if(!block)
    {
        return NULL;
    }
    ctx = block->context;
    ape_ptrarray_destroywithitems(ctx, block->statements, (ApeDataCallback)ape_ast_destroy_expr);
    ape_allocator_free(&ctx->alloc, block);
    return NULL;
}

void* ape_ast_destroy_ident(ApeContext* ctx, ApeAstIdentExpr* ident)
{
    if(!ident)
    {
        return NULL;
    }
    ape_allocator_free(&ctx->alloc, ident->value);
    ident->value = NULL;
    ident->pos = g_prspriv_srcposinvalid;
    ape_allocator_free(&ctx->alloc, ident);
    return NULL;
}

ApeAstExpression* ape_parser_parsestmt(ApeAstParser* p)
{
    ApePosition pos;
    ApeAstExpression* res;
    pos = p->lexer.curtoken.pos;
    res = NULL;
    switch(p->lexer.curtoken.toktype)
    {
        case TOKEN_KWVAR:
        case TOKEN_KWCONST:
            {
                res = ape_parser_parsevarstmt(p);
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

ApeAstExpression* ape_parser_parsevarstmt(ApeAstParser* p)
{
    bool assignable;
    ApeContext* ctx;
    ApeAstIdentExpr* nameident;
    ApeAstExpression* value;
    ApeAstExpression* res;
    ctx = p->context;
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
    if(ape_lexer_currenttokenis(&p->lexer, TOKEN_ASSIGN))
    {
        ape_lexer_nexttoken(&p->lexer);
        value = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
        if(!value)
        {
            goto err;
        }
        if(value->extype == APE_EXPR_LITERALFUNCTION)
        {
            value->exliteralfunc.name = ape_util_strdup(p->context, nameident->value);
            if(!value->exliteralfunc.name)
            {
                goto err;
            }
        }
    }
    else
    {
        value = ape_ast_make_literalnullexpr(p->context);
    }
    res = ape_ast_make_definestmt(p->context, nameident, value, assignable);
    if(!res)
    {
        goto err;
    }
    
    return res;
err:
    ape_ast_destroy_expr(ctx, value);
    ape_ast_destroy_ident(ctx, nameident);
    return NULL;
}

ApeAstExpression* ape_parser_parseifstmt(ApeAstParser* p)
{
    bool ok;
    ApeContext* ctx;
    ApePtrArray* cases;
    ApeAstBlockExpr* alternative;
    ApeAstIfCaseExpr* elif;
    ApeAstExpression* res;
    ApeAstIfCaseExpr* cond;
    ctx = p->context;
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
    cond = ape_ast_make_ifcase(p->context, NULL, NULL);
    if(!cond)
    {
        goto err;
    }
    ok = ape_ptrarray_push(cases, &cond);
    if(!ok)
    {
        ape_ast_destroy_ifcase(ctx, cond);
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
            elif = ape_ast_make_ifcase(p->context, NULL, NULL);
            if(!elif)
            {
                goto err;
            }
            ok = ape_ptrarray_push(cases, &elif);
            if(!ok)
            {
                ape_ast_destroy_ifcase(ctx, elif);
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
    ape_ptrarray_destroywithitems(ctx, cases, (ApeDataCallback)ape_ast_destroy_ifcase);
    ape_ast_destroy_codeblock(alternative);
    return NULL;
}

ApeAstExpression* ape_parser_parsereturnstmt(ApeAstParser* p)
{
    bool isexprblock;
    ApeContext* ctx;
    ApeAstExpression* expr;
    ApeAstExpression* res;
    ctx = p->context;
    expr = NULL;
    ape_lexer_nexttoken(&p->lexer);
    isexprblock = (
        (!ape_lexer_currenttokenis(&p->lexer, TOKEN_OPSEMICOLON)) &&
        (!ape_lexer_currenttokenis(&p->lexer, TOKEN_OPRIGHTBRACE)) &&
        (!ape_lexer_currenttokenis(&p->lexer, TOKEN_EOF))
    );
    if(isexprblock)
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
        ape_ast_destroy_expr(ctx, expr);
        return NULL;
    }
    return res;
}

ApeAstExpression* ape_parser_parseexprstmt(ApeAstParser* p)
{
    ApeContext* ctx;
    ApeAstExpression* expr;
    ApeAstExpression* res;
    ctx = p->context;
    expr = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
    if(!expr)
    {
        return NULL;
    }
    /* NB. this would disable using plain expressions. why was it ever a thing? puzzling. */
    #if 0
    if(expr && (!p->config->replmode || p->depth > 0))
    {
        if(expr->extype != APE_EXPR_ASSIGN && expr->extype != APE_EXPR_CALL)
        {
            ape_errorlist_addformat(p->errors, APE_ERROR_PARSING, expr->pos, "only assignments and function calls can be expression statements");
            ape_ast_destroy_expr(ctx, expr);
            return NULL;
        }
    }
    #endif
    res = ape_ast_make_expressionstmt(p->context, expr);
    if(!res)
    {
        ape_ast_destroy_expr(ctx, expr);
        return NULL;
    }
    return res;
}

ApeAstExpression* ape_parser_parsewhileloopstmt(ApeAstParser* p)
{
    ApeContext* ctx;
    ApeAstExpression* test;
    ApeAstBlockExpr* body;
    ApeAstExpression* res;
    ctx = p->context;
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
    res = ape_ast_make_whilestmt(p->context, test, body);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    ape_ast_destroy_codeblock(body);
    ape_ast_destroy_expr(ctx, test);
    return NULL;
}

ApeAstExpression* ape_parser_parsebreakstmt(ApeAstParser* p)
{
    ape_lexer_nexttoken(&p->lexer);
    return ape_ast_make_breakstmt(p->context);
}

ApeAstExpression* ape_parser_parsecontinuestmt(ApeAstParser* p)
{
    ape_lexer_nexttoken(&p->lexer);
    return ape_ast_make_continuestmt(p->context);
}

ApeAstExpression* ape_parser_parseblockstmt(ApeAstParser* p)
{
    ApeAstBlockExpr* block;
    ApeAstExpression* res;
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

ApeAstExpression* ape_parser_parseincludestmt(ApeAstParser* p)
{
    char* processedname;
    ApeSize len;
    ApeAstExpression* res;
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_VALSTRING))
    {
        return NULL;
    }
    processedname = ape_ast_processandcopystring(&p->context->alloc, p->lexer.curtoken.literal, p->lexer.curtoken.len, &len);
    if(!processedname)
    {
        ape_errorlist_add(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "error when parsing module name");
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    res= ape_ast_make_includestmt(p->context, processedname);
    if(!res)
    {
        ape_allocator_free(&p->context->alloc, processedname);
        return NULL;
    }
    return res;
}

ApeAstExpression* ape_parser_parserecoverstmt(ApeAstParser* p)
{
    ApeContext* ctx;
    ApeAstIdentExpr* errorident;
    ApeAstBlockExpr* body;
    ApeAstExpression* res;
    ctx = p->context;
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
    ape_ast_destroy_ident(ctx, errorident);
    return NULL;
}

ApeAstExpression* ape_parser_parseforloopstmt(ApeAstParser* p)
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

ApeAstExpression* ape_parser_parseforeachstmt(ApeAstParser* p)
{
    ApeContext* ctx;
    ApeAstExpression* source;
    ApeAstBlockExpr* body;
    ApeAstIdentExpr* iteratorident;
    ApeAstExpression* res;
    ctx = p->context;
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
    ape_ast_destroy_ident(ctx, iteratorident);
    ape_ast_destroy_expr(ctx, source);
    return NULL;
}

ApeAstExpression* ape_parser_parseclassicforstmt(ApeAstParser* p)
{
    ApeContext* ctx;
    ApeAstExpression* init;
    ApeAstExpression* test;
    ApeAstExpression* update;
    ApeAstBlockExpr* body;
    ApeAstExpression* res;
    ctx = p->context;
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
        if(init->extype != APE_EXPR_DEFINE && init->extype != APE_EXPR_EXPRESSION)
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
    res = ape_ast_make_forstmt(p->context, init, test, update, body);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    ape_ast_destroy_expr(ctx, init);
    ape_ast_destroy_expr(ctx, test);
    ape_ast_destroy_expr(ctx, update);
    ape_ast_destroy_codeblock(body);
    return NULL;
}

ApeAstExpression* ape_parser_parsefuncstmt(ApeAstParser* p)
{
    ApeContext* ctx;
    ApeAstIdentExpr* nameident;
    ApeAstExpression* value;
    ApePosition pos;
    ApeAstExpression* res;
    ctx = p->context;
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
    value->exliteralfunc.name = ape_util_strdup(p->context, nameident->value);
    if(!value->exliteralfunc.name)
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
    ape_ast_destroy_expr(ctx, value);
    ape_ast_destroy_ident(ctx, nameident);
    return NULL;
}

ApeAstBlockExpr* ape_parser_parsecodeblock(ApeAstParser* p)
{
    bool ok;
    bool withbraces;
    ApeContext* ctx;
    ApePtrArray* statements;
    ApeAstExpression* stmt;
    ApeAstBlockExpr* res;
    ctx = p->context;
    withbraces = false;
    if(ape_lexer_currenttokenis(&p->lexer, TOKEN_OPLEFTBRACE))
    {
        withbraces = true;
        ape_lexer_nexttoken(&p->lexer);
    }
    p->depth++;
    statements = ape_make_ptrarray(p->context);
    if(!statements)
    {
        goto err;
    }
    if(withbraces)
    {
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
            ok = ape_ptrarray_push(statements, &stmt);
            if(!ok)
            {
                ape_ast_destroy_expr(ctx, stmt);
                goto err;
            }
        }
        ape_lexer_nexttoken(&p->lexer);
    }
    else
    {
        if(ape_lexer_currenttokenis(&p->lexer, TOKEN_EOF))
        {
            ape_errorlist_add(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "unexpected EOF");
            goto err;
        }
        #if 1
            if((stmt = ape_parser_parsestmt(p)) == NULL)
            {
                stmt = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
            }
        #else
            stmt = ape_parser_parseexprstmt(p);
        #endif
        if(!stmt)
        {
            goto err;
        }
        ok = ape_ptrarray_push(statements, &stmt);
        if(!ok)
        {
            ape_ast_destroy_expr(ctx, stmt);
            goto err;
        }
        if(ape_lexer_currenttokenis(&p->lexer, TOKEN_OPSEMICOLON))
        {
            ape_lexer_nexttoken(&p->lexer);
        }
    }
    p->depth--;
    res = ape_ast_make_codeblock(p->context, statements);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    p->depth--;
    ape_ptrarray_destroywithitems(ctx, statements, (ApeDataCallback)ape_ast_destroy_expr);
    return NULL;
}

ApeAstExpression* ape_parser_parseexpr(ApeAstParser* p, ApeAstPrecedence prec)
{
    char* literal;
    ApeContext* ctx;
    ApePosition pos;
    ApeRightAssocParseFNCallback prighta;
    ApeLeftAssocParseFNCallback plefta;
    ApeAstExpression* leftexpr;
    ApeAstExpression* newleftexpr;
    ctx = p->context;
    pos = p->lexer.curtoken.pos;
    if(p->lexer.curtoken.toktype == TOKEN_INVALID)
    {
        ape_errorlist_add(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "illegal token");
        return NULL;
    }
    prighta = rightassocparsefns[p->lexer.curtoken.toktype];
    if(!prighta)
    {
        #if 1
        literal = ape_lexer_tokendupliteral(p->context, &p->lexer.curtoken);
        ape_errorlist_addformat(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "no prefix parse function for '%s' found", literal);
        ape_allocator_free(&p->context->alloc, literal);
        return NULL;
        #else
        return ape_parser_parsestmt(p);
        #endif
    }
    leftexpr = prighta(p);
    if(!leftexpr)
    {
        return NULL;
    }
    leftexpr->pos = pos;
    while(!ape_lexer_currenttokenis(&p->lexer, TOKEN_OPSEMICOLON) && prec < ape_parser_getprecedence(p->lexer.curtoken.toktype))
    {
        plefta = leftassocparsefns[p->lexer.curtoken.toktype];
        if(!plefta)
        {
            return leftexpr;
        }
        pos = p->lexer.curtoken.pos;
        newleftexpr= plefta(p, leftexpr);
        if(!newleftexpr)
        {
            ape_ast_destroy_expr(ctx, leftexpr);
            return NULL;
        }
        newleftexpr->pos = pos;
        leftexpr = newleftexpr;
    }
    return leftexpr;
}

ApeAstExpression* ape_parser_parseident(ApeAstParser* p)
{
    ApeContext* ctx;
    ApeAstIdentExpr* ident;
    ApeAstExpression* res;
    ctx = p->context;
    ident = ape_ast_make_ident(p->context, p->lexer.curtoken);
    if(!ident)
    {
        return NULL;
    }
    res = ape_ast_make_identexpr(p->context, ident);
    if(!res)
    {
        ape_ast_destroy_ident(ctx, ident);
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    return res;
}

ApeAstExpression* ape_parser_parseliteralnumber(ApeAstParser* p)
{
    char* end;
    char* literal;
    ApeFloat number;
    ApeInt parsedlen;
    number = 0;
    errno = 0;
    number = strtod(p->lexer.curtoken.literal, &end);
    parsedlen = end - p->lexer.curtoken.literal;
    if(errno || parsedlen != (ApeInt)p->lexer.curtoken.len)
    {
        literal = ape_lexer_tokendupliteral(p->context, &p->lexer.curtoken);
        ape_errorlist_addformat(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "parsing number literal '%s' failed", literal);
        ape_allocator_free(&p->context->alloc, literal);
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    return ape_ast_make_literalnumberexpr(p->context, number);
}

ApeAstExpression* ape_parser_parseliteralbool(ApeAstParser* p)
{
    ApeAstExpression* res;
    res = ape_ast_make_literalboolexpr(p->context, p->lexer.curtoken.toktype == TOKEN_KWTRUE);
    ape_lexer_nexttoken(&p->lexer);
    return res;
}

ApeAstExpression* ape_parser_parseliteralstring(ApeAstParser* p)
{
    char* processedliteral;
    ApeSize len;
    ApeAstExpression* res;
    processedliteral = ape_ast_processandcopystring(&p->context->alloc, p->lexer.curtoken.literal, p->lexer.curtoken.len, &len);
    if(!processedliteral)
    {
        ape_errorlist_add(p->errors, APE_ERROR_PARSING, p->lexer.curtoken.pos, "error while parsing string literal");
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    res = ape_ast_make_literalstringexpr(p->context, processedliteral, len, true);
    if(!res)
    {
        ape_allocator_free(&p->context->alloc, processedliteral);
        return NULL;
    }
    return res;
}

ApeAstExpression* ape_parser_parseliteraltplstring(ApeAstParser* p)
{
    char* processedliteral;
    ApeSize len;
    ApeContext* ctx;
    ApeAstExpression* leftstringexpr;
    ApeAstExpression* templateexpr;
    ApeAstExpression* tostrcallexpr;
    ApeAstExpression* leftaddexpr;
    ApeAstExpression* rightexpr;
    ApeAstExpression* rightaddexpr;
    ApePosition pos;
    ctx = p->context;
    processedliteral = NULL;
    leftstringexpr = NULL;
    templateexpr = NULL;
    tostrcallexpr = NULL;
    leftaddexpr = NULL;
    rightexpr = NULL;
    rightaddexpr = NULL;
    processedliteral = ape_ast_processandcopystring(&p->context->alloc, p->lexer.curtoken.literal, p->lexer.curtoken.len, &len);
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
    leftstringexpr = ape_ast_make_literalstringexpr(p->context, processedliteral, len, true);
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
    ape_ast_destroy_expr(ctx, rightaddexpr);
    ape_ast_destroy_expr(ctx, rightexpr);
    ape_ast_destroy_expr(ctx, leftaddexpr);
    ape_ast_destroy_expr(ctx, tostrcallexpr);
    ape_ast_destroy_expr(ctx, templateexpr);
    ape_ast_destroy_expr(ctx, leftstringexpr);
    ape_allocator_free(&p->context->alloc, processedliteral);
    return NULL;
}

ApeAstExpression* ape_parser_parseliteralnull(ApeAstParser* p)
{
    ape_lexer_nexttoken(&p->lexer);
    return ape_ast_make_literalnullexpr(p->context);
}

ApeAstExpression* ape_parser_parseliteralarray(ApeAstParser* p)
{
    ApeContext* ctx;
    ApePtrArray* array;
    ApeAstExpression* res;
    ctx = p->context;
    array = ape_parser_parseexprlist(p, TOKEN_OPLEFTBRACKET, TOKEN_OPRIGHTBRACKET, true);
    if(!array)
    {
        return NULL;
    }
    res = ape_ast_make_literalarrayexpr(p->context, array);
    if(!res)
    {
        ape_ptrarray_destroywithitems(ctx, array, (ApeDataCallback)ape_ast_destroy_expr);
        return NULL;
    }
    return res;
}

ApeAstExpression* ape_parser_parseliteralmap(ApeAstParser* p)
{
    bool ok;
    char* str;
    ApeContext* ctx;
    ApePtrArray* keys;
    ApePtrArray* values;
    ApeAstExpression* key;
    ApeAstExpression* value;
    ApeAstExpression* res;
    ctx = p->context;
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
            key = ape_ast_make_literalstringexpr(p->context, str, strlen(str), true);
            if(!key)
            {
                ape_allocator_free(&p->context->alloc, str);
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
            switch(key->extype)
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
                        ape_ast_destroy_expr(ctx, key);
                        goto err;
                    }
                    break;
            }
        }
        ok = ape_ptrarray_push(keys, &key);
        if(!ok)
        {
            ape_ast_destroy_expr(ctx, key);
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
        ok = ape_ptrarray_push(values, &value);
        if(!ok)
        {
            ape_ast_destroy_expr(ctx, value);
            goto err;
        }
        if(ape_lexer_currenttokenis(&p->lexer, TOKEN_OPRIGHTBRACE))
        {
            break;
        }
        /*
        allows to omit commas if next token is an identifier, like this:
            {
                foo: "bar"
                blah: "etc"
                ...
            }
            
        */
        #if 0
            if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPCOMMA))
            {
                goto err;
            }
            ape_lexer_nexttoken(&p->lexer);
        #else
            if(ape_lexer_currenttokenis(&p->lexer, TOKEN_OPCOMMA))
            {
                ape_lexer_nexttoken(&p->lexer);
            }
            else if(!ape_lexer_currenttokenis(&p->lexer, TOKEN_VALIDENT))
            {
                goto err;
            }
        #endif
    }
    ape_lexer_nexttoken(&p->lexer);
    res = ape_ast_make_literalmapexpr(p->context, keys, values);
    if(!res)
    {
        goto err;
    }
    return res;
err:
    ape_ptrarray_destroywithitems(ctx, keys, (ApeDataCallback)ape_ast_destroy_expr);
    ape_ptrarray_destroywithitems(ctx, values, (ApeDataCallback)ape_ast_destroy_expr);
    return NULL;
}

ApeAstExpression* ape_parser_parseprefixexpr(ApeAstParser* p)
{
    ApeOperator op;
    ApeContext* ctx;
    ApeAstExpression* right;
    ApeAstExpression* res;
    ctx = p->context;
    op = ape_parser_tokentooperator(p->lexer.curtoken.toktype);
    ape_lexer_nexttoken(&p->lexer);
    right = ape_parser_parseexpr(p, PRECEDENCE_PREFIX);
    if(!right)
    {
        return NULL;
    }
    res = ape_ast_make_prefixexpr(p->context, op, right);
    if(!res)
    {
        ape_ast_destroy_expr(ctx, right);
        return NULL;
    }
    return res;
}

ApeAstExpression* ape_parser_parseinfixexpr(ApeAstParser* p, ApeAstExpression* left)
{
    ApeOperator op;
    ApeAstPrecedence prec;
    ApeContext* ctx;
    ApeAstExpression* right;
    ApeAstExpression* res;
    ctx = p->context;
    op = ape_parser_tokentooperator(p->lexer.curtoken.toktype);
    prec = ape_parser_getprecedence(p->lexer.curtoken.toktype);
    ape_lexer_nexttoken(&p->lexer);
    right = ape_parser_parseexpr(p, prec);
    if(!right)
    {
        return NULL;
    }
    res = ape_ast_make_infixexpr(p->context, op, left, right);
    if(!res)
    {
        ape_ast_destroy_expr(ctx, right);
        return NULL;
    }
    return res;
}

ApeAstExpression* ape_parser_parsegroupedexpr(ApeAstParser* p)
{
    ApeContext* ctx;
    ApeAstExpression* expr;
    ctx = p->context;
    ape_lexer_nexttoken(&p->lexer);
    expr = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
    if(!expr || !ape_lexer_expectcurrent(&p->lexer, TOKEN_OPRIGHTPAREN))
    {
        ape_ast_destroy_expr(ctx, expr);
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    return expr;
}

ApeAstExpression* ape_parser_parseliteralfunc(ApeAstParser* p)
{
    bool ok;
    ApeContext* ctx;
    ApePtrArray* params;
    ApeAstBlockExpr* body;
    ApeAstExpression* res;
    ctx = p->context;
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
    res = ape_ast_make_literalfuncexpr(p->context, params, body);
    if(!res)
    {
        goto err;
    }
    p->depth -= 1;
    return res;
err:
    ape_ast_destroy_codeblock(body);
    ape_ptrarray_destroywithitems(ctx, params, (ApeDataCallback)ape_ast_destroy_ident);
    p->depth -= 1;
    return NULL;
}

bool ape_parser_parsefuncparams(ApeAstParser* p, ApePtrArray * outparams)
{
    bool ok;
    ApeContext* ctx;
    ApeAstIdentExpr* ident;
    ctx = p->context;
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
    ok = ape_ptrarray_push(outparams, &ident);
    if(!ok)
    {
        ape_ast_destroy_ident(ctx, ident);
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
        ok = ape_ptrarray_push(outparams, &ident);
        if(!ok)
        {
            ape_ast_destroy_ident(ctx, ident);
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

ApeAstExpression* ape_parser_parsecallexpr(ApeAstParser* p, ApeAstExpression* left)
{
    ApeContext* ctx;
    ApeAstExpression* function;
    ApePtrArray* args;
    ApeAstExpression* res;
    ctx = p->context; 
    function = left;
    args = ape_parser_parseexprlist(p, TOKEN_OPLEFTPAREN, TOKEN_OPRIGHTPAREN, false);
    if(!args)
    {
        return NULL;
    }
    res = ape_ast_make_callexpr(p->context, function, args);
    if(!res)
    {
        ape_ptrarray_destroywithitems(ctx, args, (ApeDataCallback)ape_ast_destroy_expr);
        return NULL;
    }
    return res;
}

ApePtrArray* ape_parser_parseexprlist(ApeAstParser* p, ApeAstTokType starttoken, ApeAstTokType endtoken, bool trailingcommaallowed)
{
    bool ok;
    ApeContext* ctx;
    ApePtrArray* res;
    ApeAstExpression* argexpr;
    ctx = p->context;
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
    ok = ape_ptrarray_push(res, &argexpr);
    if(!ok)
    {
        ape_ast_destroy_expr(ctx, argexpr);
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
        ok = ape_ptrarray_push(res, &argexpr);
        if(!ok)
        {
            ape_ast_destroy_expr(ctx, argexpr);
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
    ape_ptrarray_destroywithitems(p->context, res, (ApeDataCallback)ape_ast_destroy_expr);
    return NULL;
}

ApeAstExpression* ape_parser_parseindexexpr(ApeAstParser* p, ApeAstExpression* left)
{
    ApeContext* ctx;
    ApeAstExpression* index;
    ApeAstExpression* res;
    ctx = p->context;
    ape_lexer_nexttoken(&p->lexer);
    index = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
    if(!index)
    {
        return NULL;
    }
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPRIGHTBRACKET))
    {
        ape_ast_destroy_expr(ctx, index);
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    res = ape_ast_make_indexexpr(p->context, left, index);
    if(!res)
    {
        ape_ast_destroy_expr(ctx, index);
        return NULL;
    }
    return res;
}

ApeAstExpression* ape_parser_parseassignexpr(ApeAstParser* p, ApeAstExpression* left)
{
    ApeAstTokType assigntype;
    ApePosition pos;
    ApeOperator op;
    ApeContext* ctx;
    ApeAstExpression* source;
    ApeAstExpression* leftcopy;
    ApeAstExpression* newsource;
    ApeAstExpression* res;
    ctx = p->context;
    source = NULL;
    assigntype = p->lexer.curtoken.toktype;
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
                leftcopy = ape_ast_copy_expr(ctx, left);
                if(!leftcopy)
                {
                    goto err;
                }
                pos = source->pos;
                newsource = ape_ast_make_infixexpr(p->context, op, leftcopy, source);
                if(!newsource)
                {
                    ape_ast_destroy_expr(ctx, leftcopy);
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
    ape_ast_destroy_expr(ctx, source);
    return NULL;
}

ApeAstExpression* ape_parser_parselogicalexpr(ApeAstParser* p, ApeAstExpression* left)
{
    ApeOperator op;
    ApeAstPrecedence prec;
    ApeAstExpression* right;
    ApeAstExpression* res;
    op = ape_parser_tokentooperator(p->lexer.curtoken.toktype);
    prec = ape_parser_getprecedence(p->lexer.curtoken.toktype);
    ape_lexer_nexttoken(&p->lexer);
    right = ape_parser_parseexpr(p, prec);
    if(!right)
    {
        return NULL;
    }
    res = ape_ast_make_logicalexpr(p->context, op, left, right);
    if(!res)
    {
        ape_ast_destroy_expr(p->context, right);
        return NULL;
    }
    return res;
}

ApeAstExpression* ape_parser_parseternaryexpr(ApeAstParser* p, ApeAstExpression* left)
{
    ApeContext* ctx;
    ApeAstExpression* iftrue;
    ApeAstExpression* iffalse;
    ApeAstExpression* res;
    ctx = p->context;
    ape_lexer_nexttoken(&p->lexer);
    iftrue = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
    if(!iftrue)
    {
        return NULL;
    }
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_OPCOLON))
    {
        ape_ast_destroy_expr(ctx, iftrue);
        return NULL;
    }
    ape_lexer_nexttoken(&p->lexer);
    iffalse = ape_parser_parseexpr(p, PRECEDENCE_LOWEST);
    if(!iffalse)
    {
        ape_ast_destroy_expr(ctx, iftrue);
        return NULL;
    }
    res = ape_ast_make_ternaryexpr(p->context, left, iftrue, iffalse);
    if(!res)
    {
        ape_ast_destroy_expr(ctx, iftrue);
        ape_ast_destroy_expr(ctx, iffalse);
        return NULL;
    }
    return res;
}

ApeAstExpression* ape_parser_parseincdecprefixexpr(ApeAstParser* p)
{
    ApeContext* ctx;
    ApeAstExpression* source;
    ApeAstTokType operationtype;
    ApePosition pos;
    ApeOperator op;
    ApeAstExpression* dest;
    ApeAstExpression* oneliteral;
    ApeAstExpression* destcopy;
    ApeAstExpression* operation;
    ApeAstExpression* res;
    ctx = p->context;
    source = NULL;
    operationtype = p->lexer.curtoken.toktype;
    pos = p->lexer.curtoken.pos;
    ape_lexer_nexttoken(&p->lexer);
    op = ape_parser_tokentooperator(operationtype);
    dest = ape_parser_parseexpr(p, PRECEDENCE_PREFIX);
    if(!dest)
    {
        goto err;
    }
    oneliteral = ape_ast_make_literalnumberexpr(p->context, 1);
    if(!oneliteral)
    {
        ape_ast_destroy_expr(ctx, dest);
        goto err;
    }
    oneliteral->pos = pos;
    destcopy = ape_ast_copy_expr(p->context, dest);
    if(!destcopy)
    {
        ape_ast_destroy_expr(ctx, oneliteral);
        ape_ast_destroy_expr(ctx, dest);
        goto err;
    }
    operation = ape_ast_make_infixexpr(p->context, op, destcopy, oneliteral);
    if(!operation)
    {
        ape_ast_destroy_expr(ctx, destcopy);
        ape_ast_destroy_expr(ctx, dest);
        ape_ast_destroy_expr(ctx, oneliteral);
        goto err;
    }
    operation->pos = pos;
    res = ape_ast_make_assignexpr(p->context, dest, operation, false);
    if(!res)
    {
        ape_ast_destroy_expr(ctx, dest);
        ape_ast_destroy_expr(ctx, operation);
        goto err;
    }
    return res;
err:
    ape_ast_destroy_expr(ctx, source);
    return NULL;
}

ApeAstExpression* ape_parser_parseincdecpostfixexpr(ApeAstParser* p, ApeAstExpression* left)
{
    ApeContext* ctx;
    ApeAstExpression* source;
    ApeAstTokType operationtype;
    ApePosition pos;
    ApeOperator op;
    ApeAstExpression* leftcopy;
    ApeAstExpression* oneliteral;
    ApeAstExpression* operation;
    ApeAstExpression* res;
    ctx = p->context;
    source = NULL;
    operationtype = p->lexer.curtoken.toktype;
    pos = p->lexer.curtoken.pos;
    ape_lexer_nexttoken(&p->lexer);
    op = ape_parser_tokentooperator(operationtype);
    leftcopy = ape_ast_copy_expr(p->context, left);
    if(!leftcopy)
    {
        goto err;
    }
    oneliteral = ape_ast_make_literalnumberexpr(p->context, 1);
    if(!oneliteral)
    {
        ape_ast_destroy_expr(ctx, leftcopy);
        goto err;
    }
    oneliteral->pos = pos;
    operation = ape_ast_make_infixexpr(p->context, op, leftcopy, oneliteral);
    if(!operation)
    {
        ape_ast_destroy_expr(ctx, oneliteral);
        ape_ast_destroy_expr(ctx, leftcopy);
        goto err;
    }
    operation->pos = pos;
    res = ape_ast_make_assignexpr(p->context, left, operation, true);
    if(!res)
    {
        ape_ast_destroy_expr(ctx, operation);
        goto err;
    }
    return res;
err:
    ape_ast_destroy_expr(ctx, source);
    return NULL;
}

ApeAstExpression* ape_parser_parsedotexpr(ApeAstParser* p, ApeAstExpression* left)
{
    char* str;
    ApeContext* ctx;
    ApeAstExpression* index;
    ApeAstExpression* res;
    ctx = p->context;
    ape_lexer_nexttoken(&p->lexer);
    if(!ape_lexer_expectcurrent(&p->lexer, TOKEN_VALIDENT))
    {
        return NULL;
    }
    str = ape_lexer_tokendupliteral(p->context, &p->lexer.curtoken);
    index = ape_ast_make_literalstringexpr(p->context, str, strlen(str), true);
    if(!index)
    {
        ape_allocator_free(&p->context->alloc, str);
        return NULL;
    }
    index->pos = p->lexer.curtoken.pos;
    ape_lexer_nexttoken(&p->lexer);
    res = ape_ast_make_indexexpr(p->context, left, index);
    if(!res)
    {
        ape_ast_destroy_expr(ctx, index);
        return NULL;
    }
    return res;
}

ApeAstPrecedence ape_parser_getprecedence(ApeAstTokType tk)
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
        case TOKEN_OPBITNOT:
            return PRECEDENCE_SUM;
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

ApeOperator ape_parser_tokentooperator(ApeAstTokType tk)
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
        case TOKEN_OPBITNOT:
            return APE_OPERATOR_BITNOT;
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

char ape_parser_escapechar(const char c)
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

char* ape_ast_processandcopystring(ApeAllocator* alloc, const char* input, size_t len, ApeSize* destlen)
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

ApeAstExpression* ape_ast_wrapexprinfunccall(ApeContext* ctx, ApeAstExpression* expr, const char* functionname)
{
    bool ok;
    ApeAstExpression* callexpr;
    ApeAstIdentExpr* ident;
    ApeAstExpression* functionidentexpr;
    ApePtrArray* args;
    ApeAstToken fntoken;
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
        ape_ast_destroy_ident(ctx, ident);
        return NULL;
    }
    functionidentexpr->pos = expr->pos;
    ident = NULL;
    args = ape_make_ptrarray(ctx);
    if(!args)
    {
        ape_ast_destroy_expr(ctx, functionidentexpr);
        return NULL;
    }
    ok = ape_ptrarray_push(args, &expr);
    if(!ok)
    {
        ape_ptrarray_destroy(args);
        ape_ast_destroy_expr(ctx, functionidentexpr);
        return NULL;
    }
    callexpr = ape_ast_make_callexpr(ctx, functionidentexpr, args);
    if(!callexpr)
    {
        ape_ptrarray_destroy(args);
        ape_ast_destroy_expr(ctx, functionidentexpr);
        return NULL;
    }
    callexpr->pos = expr->pos;
    return callexpr;
}
