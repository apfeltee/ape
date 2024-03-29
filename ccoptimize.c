
#include "inline.h"

ApeAstExpression* ape_optimizer_optexpr(ApeAstExpression* expr)
{
    return NULL;
    switch(expr->extype)
    {
        case APE_EXPR_INFIX:
            {
                return ape_optimizer_optinfixexpr(expr);
            }
            break;
        case APE_EXPR_PREFIX:
            {
                return ape_optimizer_optprefixexpr(expr);
            }
            break;
        default:
            {
            }
            break;
    }
    return NULL;
}

ApeAstExpression* ape_optimizer_optinfixexpr(ApeAstExpression* expr)
{
    bool leftisnum;
    bool rightisnum;
    bool leftisstr;
    bool rightisstr;
    ApeInt leftint;
    ApeInt rightint;
    ApeSize rtlen;
    char* rtstr;
    const char* leftstr;
    const char* rightstr;
    ApeAstExpression* leftexpr;
    ApeAstExpression* leftopt;
    ApeAstExpression* rightexpr;
    ApeAstExpression* rightopt;
    ApeAstExpression* res;
    ApeWriter* buf;
    ApeFloat leftval;
    ApeFloat rightval;
    leftexpr = expr->exinfix.left;
    leftopt = ape_optimizer_optexpr(leftexpr);
    if(leftopt)
    {
        leftexpr = leftopt;
    }
    rightexpr = expr->exinfix.right;
    rightopt = ape_optimizer_optexpr(rightexpr);
    if(rightopt)
    {
        rightexpr = rightopt;
    }
    res = NULL;
    leftisnum = leftexpr->extype == APE_EXPR_LITERALNUMBER || leftexpr->extype == APE_EXPR_LITERALBOOL;
    rightisnum = rightexpr->extype == APE_EXPR_LITERALNUMBER || rightexpr->extype == APE_EXPR_LITERALBOOL;
    leftisstr = leftexpr->extype == APE_EXPR_LITERALSTRING;
    rightisstr = rightexpr->extype == APE_EXPR_LITERALSTRING;
    if(leftisnum && rightisnum)
    {
        leftval = leftexpr->extype == APE_EXPR_LITERALNUMBER ? leftexpr->exliteralnumber : leftexpr->exliteralbool;
        rightval = rightexpr->extype == APE_EXPR_LITERALNUMBER ? rightexpr->exliteralnumber : rightexpr->exliteralbool;
        leftint = (ApeInt)leftval;
        rightint = (ApeInt)rightval;
        switch(expr->exinfix.op)
        {
            case APE_OPERATOR_PLUS:
                {
                    res = ape_ast_make_literalnumberexpr(expr->context, leftval + rightval);
                }
                break;
            case APE_OPERATOR_MINUS:
                {
                    res = ape_ast_make_literalnumberexpr(expr->context, leftval - rightval);
                }
                break;
            case APE_OPERATOR_STAR:
                {
                    res = ape_ast_make_literalnumberexpr(expr->context, leftval * rightval);
                }
                break;
            case APE_OPERATOR_SLASH:
                {
                    res = ape_ast_make_literalnumberexpr(expr->context, leftval / rightval);
                }
                break;
            case APE_OPERATOR_LESSTHAN:
                {
                    res = ape_ast_make_literalboolexpr(expr->context, leftval < rightval);
                }
                break;
            case APE_OPERATOR_LESSEQUAL:
                {
                    res = ape_ast_make_literalboolexpr(expr->context, leftval <= rightval);
                }
                break;
            case APE_OPERATOR_GREATERTHAN:
                {
                    res = ape_ast_make_literalboolexpr(expr->context, leftval > rightval);
                }
                break;
            case APE_OPERATOR_GREATEREQUAL:
                {
                    res = ape_ast_make_literalboolexpr(expr->context, leftval >= rightval);
                }
                break;
            case APE_OPERATOR_EQUAL:
                {
                    res = ape_ast_make_literalboolexpr(expr->context, APE_DBLEQ(leftval, rightval));
                }
                break;

            case APE_OPERATOR_NOTEQUAL:
                {
                    res = ape_ast_make_literalboolexpr(expr->context, !APE_DBLEQ(leftval, rightval));
                }
                break;
            case APE_OPERATOR_MODULUS:
                {
                    #if 0
                    res = ape_ast_make_literalnumberexpr(expr->context, fmod(leftval, rightval));
                    #else
                    res = ape_ast_make_literalnumberexpr(expr->context, (leftint % rightint));
                    #endif
                }
                break;
            case APE_OPERATOR_BITAND:
                {
                    res = ape_ast_make_literalnumberexpr(expr->context, (ApeFloat)(leftint & rightint));
                }
                break;
            case APE_OPERATOR_BITOR:
                {
                    res = ape_ast_make_literalnumberexpr(expr->context, (ApeFloat)(leftint | rightint));
                }
                break;
            case APE_OPERATOR_BITXOR:
                {
                    res = ape_ast_make_literalnumberexpr(expr->context, (ApeFloat)(leftint ^ rightint));
                }
                break;
            case APE_OPERATOR_LEFTSHIFT:
                {
                    res = ape_ast_make_literalnumberexpr(expr->context, (ApeFloat)(leftint << rightint));
                }
                break;
            case APE_OPERATOR_RIGHTSHIFT:
                {
                    res = ape_ast_make_literalnumberexpr(expr->context, (ApeFloat)(leftint >> rightint));
                }
                break;
            default:
                {
                }
                break;
        }
    }
    else if(expr->exinfix.op == APE_OPERATOR_PLUS && leftisstr && rightisstr)
    {
        leftstr = leftexpr->exliteralstring;
        rightstr = rightexpr->exliteralstring;
        buf = ape_make_writercapacity(expr->context, leftexpr->stringlitlength + rightexpr->stringlitlength + 1);
        ape_writer_appendlen(buf, leftstr, leftexpr->stringlitlength);
        ape_writer_appendlen(buf, rightstr, rightexpr->stringlitlength);
        rtlen = ape_writer_getlength(buf);
        rtstr = ape_util_strndup(expr->context, ape_writer_getdata(buf), rtlen);
        res = ape_ast_make_literalstringexpr(expr->context, rtstr, rtlen, true);
        ape_writer_destroy(buf);
    }
    ape_ast_destroy_expr(expr->context, leftopt);
    ape_ast_destroy_expr(expr->context, rightopt);
    if(res)
    {
        res->pos = expr->pos;
    }
    return res;
}

ApeAstExpression* ape_optimizer_optprefixexpr(ApeAstExpression* expr)
{
    ApeAstExpression* rightexpr;
    ApeAstExpression* rightopt;
    ApeAstExpression* res;
    rightexpr = expr->exprefix.right;
    rightopt = ape_optimizer_optexpr(rightexpr);
    if(rightopt)
    {
        rightexpr = rightopt;
    }
    res = NULL;
    if(expr->exprefix.op == APE_OPERATOR_MINUS && rightexpr->extype == APE_EXPR_LITERALNUMBER)
    {
        res = ape_ast_make_literalnumberexpr(expr->context, -rightexpr->exliteralnumber);
    }
    else if(expr->exprefix.op == APE_OPERATOR_NOT && rightexpr->extype == APE_EXPR_LITERALBOOL)
    {
        res = ape_ast_make_literalboolexpr(expr->context, !rightexpr->exliteralbool);
    }
    ape_ast_destroy_expr(expr->context, rightopt);
    if(res)
    {
        res->pos = expr->pos;
    }
    return res;
}



