
#include "ape.h"

ApeExpression_t* ape_optimizer_optexpr(ApeExpression_t* expr)
{
    return NULL;
    switch(expr->type)
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

ApeExpression_t* ape_optimizer_optinfixexpr(ApeExpression_t* expr)
{
    bool leftisnum;
    bool rightisnum;
    bool leftisstr;
    bool rightisstr;
    ApeInt_t leftint;
    ApeInt_t rightint;
    char* res_str;
    const char* leftstr;
    const char* rightstr;
    ApeExpression_t* leftexpr;
    ApeExpression_t* leftopt;
    ApeExpression_t* rightexpr;
    ApeExpression_t* rightopt;
    ApeExpression_t* res;
    ApeAllocator_t* alloc;
    ApeFloat_t leftval;
    ApeFloat_t rightval;
    leftexpr = expr->infix.left;
    leftopt = ape_optimizer_optexpr(leftexpr);
    if(leftopt)
    {
        leftexpr = leftopt;
    }
    rightexpr = expr->infix.right;
    rightopt = ape_optimizer_optexpr(rightexpr);
    if(rightopt)
    {
        rightexpr = rightopt;
    }
    res = NULL;
    leftisnum = leftexpr->type == APE_EXPR_LITERALNUMBER || leftexpr->type == APE_EXPR_LITERALBOOL;
    rightisnum = rightexpr->type == APE_EXPR_LITERALNUMBER || rightexpr->type == APE_EXPR_LITERALBOOL;
    leftisstr = leftexpr->type == APE_EXPR_LITERALSTRING;
    rightisstr = rightexpr->type == APE_EXPR_LITERALSTRING;
    alloc = expr->alloc;
    if(leftisnum && rightisnum)
    {
        leftval = leftexpr->type == APE_EXPR_LITERALNUMBER ? leftexpr->numberliteral : leftexpr->boolliteral;
        rightval = rightexpr->type == APE_EXPR_LITERALNUMBER ? rightexpr->numberliteral : rightexpr->boolliteral;
        leftint = (ApeInt_t)leftval;
        rightint = (ApeInt_t)rightval;
        switch(expr->infix.op)
        {
            case APE_OPERATOR_PLUS:
                {
                    res = ape_ast_make_numberliteralexpr(expr->context, leftval + rightval);
                }
                break;
            case APE_OPERATOR_MINUS:
                {
                    res = ape_ast_make_numberliteralexpr(expr->context, leftval - rightval);
                }
                break;
            case APE_OPERATOR_STAR:
                {
                    res = ape_ast_make_numberliteralexpr(expr->context, leftval * rightval);
                }
                break;
            case APE_OPERATOR_SLASH:
                {
                    res = ape_ast_make_numberliteralexpr(expr->context, leftval / rightval);
                }
                break;
            case APE_OPERATOR_LESSTHAN:
                {
                    res = ape_ast_make_boolliteralexpr(expr->context, leftval < rightval);
                }
                break;
            case APE_OPERATOR_LESSEQUAL:
                {
                    res = ape_ast_make_boolliteralexpr(expr->context, leftval <= rightval);
                }
                break;
            case APE_OPERATOR_GREATERTHAN:
                {
                    res = ape_ast_make_boolliteralexpr(expr->context, leftval > rightval);
                }
                break;
            case APE_OPERATOR_GREATEREQUAL:
                {
                    res = ape_ast_make_boolliteralexpr(expr->context, leftval >= rightval);
                }
                break;
            case APE_OPERATOR_EQUAL:
                {
                    res = ape_ast_make_boolliteralexpr(expr->context, APE_DBLEQ(leftval, rightval));
                }
                break;

            case APE_OPERATOR_NOTEQUAL:
                {
                    res = ape_ast_make_boolliteralexpr(expr->context, !APE_DBLEQ(leftval, rightval));
                }
                break;
            case APE_OPERATOR_MODULUS:
                {
                    //res = ape_ast_make_numberliteralexpr(expr->context, fmod(leftval, rightval));
                    res = ape_ast_make_numberliteralexpr(expr->context, (leftint % rightint));
                }
                break;
            case APE_OPERATOR_BITAND:
                {
                    res = ape_ast_make_numberliteralexpr(expr->context, (ApeFloat_t)(leftint & rightint));
                }
                break;
            case APE_OPERATOR_BITOR:
                {
                    res = ape_ast_make_numberliteralexpr(expr->context, (ApeFloat_t)(leftint | rightint));
                }
                break;
            case APE_OPERATOR_BITXOR:
                {
                    res = ape_ast_make_numberliteralexpr(expr->context, (ApeFloat_t)(leftint ^ rightint));
                }
                break;
            case APE_OPERATOR_LEFTSHIFT:
                {
                    res = ape_ast_make_numberliteralexpr(expr->context, (ApeFloat_t)(leftint << rightint));
                }
                break;
            case APE_OPERATOR_RIGHTSHIFT:
                {
                    res = ape_ast_make_numberliteralexpr(expr->context, (ApeFloat_t)(leftint >> rightint));
                }
                break;
            default:
                {
                }
                break;
        }
    }
    else if(expr->infix.op == APE_OPERATOR_PLUS && leftisstr && rightisstr)
    {
        leftstr = leftexpr->stringliteral;
        rightstr = rightexpr->stringliteral;
        res_str = ape_util_stringfmt(expr->context, "%s%s", leftstr, rightstr);
        if(res_str)
        {
            res = ape_ast_make_stringliteralexpr(expr->context, res_str);
            if(!res)
            {
                ape_allocator_free(alloc, res_str);
            }
        }
    }
    ape_ast_destroy_expr(leftopt);
    ape_ast_destroy_expr(rightopt);
    if(res)
    {
        res->pos = expr->pos;
    }
    return res;
}

ApeExpression_t* ape_optimizer_optprefixexpr(ApeExpression_t* expr)
{
    ApeExpression_t* rightexpr;
    ApeExpression_t* rightopt;
    ApeExpression_t* res;
    rightexpr = expr->prefix.right;
    rightopt = ape_optimizer_optexpr(rightexpr);
    if(rightopt)
    {
        rightexpr = rightopt;
    }
    res = NULL;
    if(expr->prefix.op == APE_OPERATOR_MINUS && rightexpr->type == APE_EXPR_LITERALNUMBER)
    {
        res = ape_ast_make_numberliteralexpr(expr->context, -rightexpr->numberliteral);
    }
    else if(expr->prefix.op == APE_OPERATOR_NOT && rightexpr->type == APE_EXPR_LITERALBOOL)
    {
        res = ape_ast_make_boolliteralexpr(expr->context, !rightexpr->boolliteral);
    }
    ape_ast_destroy_expr(rightopt);
    if(res)
    {
        res->pos = expr->pos;
    }
    return res;
}



