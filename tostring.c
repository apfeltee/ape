
#include <inttypes.h>
#include "inline.h"

static const char* g_type_names[] = {
    "illegal",  "eof",   "=",        "+=",     "-=",
    "*=",       "/=",    "%=",       "&=",     "|=",
    "^=",       "<<=",   ">>=",      "?",      "+",
    "++",       "-",     "--",       "!",      "*",
    "/",        "<",     "<=",       ">",      ">=",
    "==",       "!=",    "&&",       "||",     "&",
    "|",        "^",     "<<",       ">>",     ",",
    ";",        ":",     "(",        ")",      "{",
    "}",        "[",     "]",        ".",      "%",
    "function", "const", "var",      "true",   "false",
    "if",       "else",  "return",   "while",  "break",
    "for",      "in",    "continue", "null",   "include",
    "import", "recover",  "ident", "number",   "string",
    "templatestring",
};


const char* ape_tostring_operator(ApeOperator_t op)
{
    switch(op)
    {
        case APE_OPERATOR_NONE:
            return "operator_none";
        case APE_OPERATOR_ASSIGN:
            return "=";
        case APE_OPERATOR_PLUS:
            return "+";
        case APE_OPERATOR_MINUS:
            return "-";
        case APE_OPERATOR_NOT:
            return "!";
        case APE_OPERATOR_STAR:
            return "*";
        case APE_OPERATOR_SLASH:
            return "/";
        case APE_OPERATOR_LESSTHAN:
            return "<";
        case APE_OPERATOR_GREATERTHAN:
            return ">";
        case APE_OPERATOR_EQUAL:
            return "==";
        case APE_OPERATOR_NOTEQUAL:
            return "!=";
        case APE_OPERATOR_MODULUS:
            return "%";
        case APE_OPERATOR_LOGICALAND:
            return "&&";
        case APE_OPERATOR_LOGICALOR:
            return "||";
        case APE_OPERATOR_BITAND:
            return "&";
        case APE_OPERATOR_BITOR:
            return "|";
        case APE_OPERATOR_BITXOR:
            return "^";
        case APE_OPERATOR_LEFTSHIFT:
            return "<<";
        case APE_OPERATOR_RIGHTSHIFT:
            return ">>";
        default:
            break;
    }
    return "operator_unknown";
}

const char* ape_tostring_exprtype(ApeAstExprType_t type)
{
    switch(type)
    {
        case APE_EXPR_NONE:
            return "none";
        case APE_EXPR_IDENT:
            return "identifier";
        case APE_EXPR_LITERALNUMBER:
            return "litint";
        case APE_EXPR_LITERALBOOL:
            return "litbool";
        case APE_EXPR_LITERALSTRING:
            return "litstring";
        case APE_EXPR_LITERALARRAY:
            return "litarray";
        case APE_EXPR_LITERALMAP:
            return "litmap";
        case APE_EXPR_PREFIX:
            return "prefix";
        case APE_EXPR_INFIX:
            return "infix";
        case APE_EXPR_LITERALFUNCTION:
            return "litfunc";
        case APE_EXPR_CALL:
            return "call";
        case APE_EXPR_INDEX:
            return "index";
        case APE_EXPR_ASSIGN:
            return "assign";
        case APE_EXPR_LOGICAL:
            return "logical";
        case APE_EXPR_TERNARY:
            return "ternary";
        case APE_EXPR_DEFINE:
            return "define";
        case APE_EXPR_IF:
            return "if";
        case APE_EXPR_RETURNVALUE:
            return "returnvalue";
        case APE_EXPR_EXPRESSION:
            return "expression";
        case APE_EXPR_WHILELOOP:
            return "whileloop";
        case APE_EXPR_BREAK:
            return "break";
        case APE_EXPR_CONTINUE:
            return "continue";
        case APE_EXPR_FOREACH:
            return "foreach";
        case APE_EXPR_FORLOOP:
            return "forloop";
        case APE_EXPR_BLOCK:
            return "block";
        case APE_EXPR_INCLUDE:
            return "include";
        case APE_EXPR_RECOVER:
            return "recover";

        default:
            break;
    }
    return "unknown";
}

static bool ape_tostring_opcodecoderead(ApeOpcodeDef_t* def, ApeUShort_t* instr, ApeOpByte_t outop[2])
{
    ApeSize_t i;
    ApeOpByte_t operand;
    int opwidth;
    int offset;
    offset = 0;
    if(def == NULL)
    {
        return false;
    }
    for(i = 0; i < def->operandcount; i++)
    {
        opwidth = def->operandwidths[i];
        switch(opwidth)
        {
            case 1:
                {
                    outop[i] = instr[offset];
                }
                break;
            case 2:
                {
                    operand = 0;
                    operand = operand | (instr[offset] << 8);
                    operand = operand | (instr[offset + 1]);
                    outop[i] = operand;
                }
                break;
            case 4:
                {
                    operand = 0;
                    operand = operand | (instr[offset + 0] << 24);
                    operand = operand | (instr[offset + 1] << 16);
                    operand = operand | (instr[offset + 2] << 8);
                    operand = operand | (instr[offset + 3]);
                    outop[i] = operand;
                }
                break;
            case 8:
                {
                    operand = 0;
                    operand = operand | ((ApeOpByte_t)instr[offset + 0] << 56);
                    operand = operand | ((ApeOpByte_t)instr[offset + 1] << 48);
                    operand = operand | ((ApeOpByte_t)instr[offset + 2] << 40);
                    operand = operand | ((ApeOpByte_t)instr[offset + 3] << 32);
                    operand = operand | ((ApeOpByte_t)instr[offset + 4] << 24);
                    operand = operand | ((ApeOpByte_t)instr[offset + 5] << 16);
                    operand = operand | ((ApeOpByte_t)instr[offset + 6] << 8);
                    operand = operand | ((ApeOpByte_t)instr[offset + 7]);
                    outop[i] = operand;
                }
                break;
            default:
                {
                    APE_ASSERT(false);
                    return false;
                }
                break;
        }
        offset += opwidth;
    }
    return true;
}

bool ape_tostring_exprlist(ApeWriter_t* buf, ApePtrArray_t* statements)
{
    ApeSize_t i;
    ApeSize_t count;
    ApeAstExpression_t* stmt;
    count = ape_ptrarray_count(statements);
    for(i = 0; i < count; i++)
    {
        stmt = (ApeAstExpression_t*)ape_ptrarray_get(statements, i);
        ape_tostring_expression(buf, stmt);
        if(i < (count - 1))
        {
            ape_writer_append(buf, "\n");
        }
    }
    return true;
}

bool ape_tostring_expression(ApeWriter_t* buf, ApeAstExpression_t* expr)
{
    ApeSize_t i;
    ApeFloat_t fltnum;
    ApeAstExpression_t* arrexpr;
    ApeAstLiteralMapExpr_t* mapexpr;
    ApeAstExpression_t* keyexpr;
    ApeAstExpression_t* valexpr;
    ApeAstLiteralFuncExpr_t* fnexpr;
    ApeAstIdentExpr_t* paramexpr;
    ApeAstExpression_t* argexpr;
    ApeAstCallExpr_t* callexpr;
    ApeAstDefineExpr_t* defstmt;
    ApeAstIfCaseExpr_t* ifcase;
    ApeAstIfCaseExpr_t* elifcase;
    switch(expr->extype)
    {
        case APE_EXPR_IDENT:
            {
                ape_writer_append(buf, expr->exident->value);
            }
            break;
        case APE_EXPR_LITERALNUMBER:
            {
                fltnum = expr->exliteralnumber;
                #if 0
                ape_writer_appendf(buf, "%1.17g", fltnum);
                #else
                if(fltnum == ((ApeInt_t)fltnum))
                {
                    #if 0
                    ape_writer_appendf(buf, "%" PRId64, (ApeInt_t)fltnum);
                    #else
                    ape_writer_appendf(buf, "%" PRIi64, (ApeInt_t)fltnum);
                    #endif
                }
                else
                {
                    ape_writer_appendf(buf, "%g", fltnum);
                }
                #endif
            }
            break;
        case APE_EXPR_LITERALBOOL:
            {
                ape_writer_appendf(buf, "%s", expr->exliteralbool ? "true" : "false");
            }
            break;
        case APE_EXPR_LITERALSTRING:
            {
                ape_writer_appendf(buf, "\"%.*s\"", (int)expr->stringlitlength, expr->exliteralstring);
            }
            break;
        case APE_EXPR_LITERALNULL:
            {
                ape_writer_append(buf, "null");
            }
            break;
        case APE_EXPR_LITERALARRAY:
            {
                ape_writer_append(buf, "[");
                for(i = 0; i < ape_ptrarray_count(expr->exarray); i++)
                {
                    arrexpr = (ApeAstExpression_t*)ape_ptrarray_get(expr->exarray, i);
                    ape_tostring_expression(buf, arrexpr);
                    if(i < (ape_ptrarray_count(expr->exarray) - 1))
                    {
                        ape_writer_append(buf, ", ");
                    }
                }
                ape_writer_append(buf, "]");
            }
            break;
        case APE_EXPR_LITERALMAP:
            {
                mapexpr = &expr->exmap;
                ape_writer_append(buf, "{");
                for(i = 0; i < ape_ptrarray_count(mapexpr->keys); i++)
                {
                    keyexpr = (ApeAstExpression_t*)ape_ptrarray_get(mapexpr->keys, i);
                    valexpr = (ApeAstExpression_t*)ape_ptrarray_get(mapexpr->values, i);
                    ape_tostring_expression(buf, keyexpr);
                    ape_writer_append(buf, " : ");
                    ape_tostring_expression(buf, valexpr);
                    if(i < (ape_ptrarray_count(mapexpr->keys) - 1))
                    {
                        ape_writer_append(buf, ", ");
                    }
                }
                ape_writer_append(buf, "}");
            }
            break;
        case APE_EXPR_PREFIX:
            {
                ape_writer_append(buf, "(");
                ape_writer_append(buf, ape_tostring_operator(expr->exinfix.op));
                ape_tostring_expression(buf, expr->exprefix.right);
                ape_writer_append(buf, ")");
            }
            break;
        case APE_EXPR_INFIX:
            {
                ape_writer_append(buf, "(");
                ape_tostring_expression(buf, expr->exinfix.left);
                ape_writer_append(buf, " ");
                ape_writer_append(buf, ape_tostring_operator(expr->exinfix.op));
                ape_writer_append(buf, " ");
                ape_tostring_expression(buf, expr->exinfix.right);
                ape_writer_append(buf, ")");
            }
            break;
        case APE_EXPR_LITERALFUNCTION:
            {
                fnexpr = &expr->exliteralfunc;
                ape_writer_append(buf, "function");
                ape_writer_append(buf, "(");
                for(i = 0; i < ape_ptrarray_count(fnexpr->params); i++)
                {
                    paramexpr = (ApeAstIdentExpr_t*)ape_ptrarray_get(fnexpr->params, i);
                    ape_writer_append(buf, paramexpr->value);
                    if(i < (ape_ptrarray_count(fnexpr->params) - 1))
                    {
                        ape_writer_append(buf, ", ");
                    }
                }
                ape_writer_append(buf, ") ");
                ape_tostring_codeblock(buf, fnexpr->body);
            }
            break;
        case APE_EXPR_CALL:
            {
                callexpr = &expr->excall;
                ape_tostring_expression(buf, callexpr->function);
                ape_writer_append(buf, "(");
                for(i = 0; i < ape_ptrarray_count(callexpr->args); i++)
                {
                    argexpr = (ApeAstExpression_t*)ape_ptrarray_get(callexpr->args, i);
                    ape_tostring_expression(buf, argexpr);
                    if(i < (ape_ptrarray_count(callexpr->args) - 1))
                    {
                        ape_writer_append(buf, ", ");
                    }
                }
                ape_writer_append(buf, ")");
            }
            break;
        case APE_EXPR_INDEX:
            {
                ape_writer_append(buf, "(");
                ape_tostring_expression(buf, expr->exindex.left);
                ape_writer_append(buf, "[");
                ape_tostring_expression(buf, expr->exindex.index);
                ape_writer_append(buf, "])");
            }
            break;
        case APE_EXPR_ASSIGN:
            {
                ape_tostring_expression(buf, expr->exassign.dest);
                ape_writer_append(buf, " = ");
                ape_tostring_expression(buf, expr->exassign.source);
            }
            break;
        case APE_EXPR_LOGICAL:
            {
                ape_tostring_expression(buf, expr->exlogical.left);
                ape_writer_append(buf, " ");
                ape_writer_append(buf, ape_tostring_operator(expr->exinfix.op));
                ape_writer_append(buf, " ");
                ape_tostring_expression(buf, expr->exlogical.right);
            }
            break;
        case APE_EXPR_TERNARY:
            {
                ape_tostring_expression(buf, expr->externary.test);
                ape_writer_append(buf, " ? ");
                ape_tostring_expression(buf, expr->externary.iftrue);
                ape_writer_append(buf, " : ");
                ape_tostring_expression(buf, expr->externary.iffalse);
            }
            break;
        case APE_EXPR_DEFINE:
            {
                defstmt = &expr->exdefine;
                if(expr->exdefine.assignable)
                {
                    ape_writer_append(buf, "var ");
                }
                else
                {
                    ape_writer_append(buf, "const ");
                }
                ape_writer_append(buf, defstmt->name->value);
                ape_writer_append(buf, " = ");
                if(defstmt->value)
                {
                    ape_tostring_expression(buf, defstmt->value);
                }
            }
            break;
        case APE_EXPR_IF:
            {
                ifcase = (ApeAstIfCaseExpr_t*)ape_ptrarray_get(expr->exifstmt.cases, 0);
                ape_writer_append(buf, "if (");
                ape_tostring_expression(buf, ifcase->test);
                ape_writer_append(buf, ") ");
                ape_tostring_codeblock(buf, ifcase->consequence);
                for(i = 1; i < ape_ptrarray_count(expr->exifstmt.cases); i++)
                {
                    elifcase = (ApeAstIfCaseExpr_t*)ape_ptrarray_get(expr->exifstmt.cases, i);
                    ape_writer_append(buf, " elif (");
                    ape_tostring_expression(buf, elifcase->test);
                    ape_writer_append(buf, ") ");
                    ape_tostring_codeblock(buf, elifcase->consequence);
                }
                if(expr->exifstmt.alternative)
                {
                    ape_writer_append(buf, " else ");
                    ape_tostring_codeblock(buf, expr->exifstmt.alternative);
                }
            }
            break;
        case APE_EXPR_RETURNVALUE:
            {
                ape_writer_append(buf, "return ");
                if(expr->exreturn)
                {
                    ape_tostring_expression(buf, expr->exreturn);
                }
            }
            break;
        case APE_EXPR_EXPRESSION:
            {
                if(expr->exexpression)
                {
                    ape_tostring_expression(buf, expr->exexpression);
                }
            }
            break;
        case APE_EXPR_WHILELOOP:
            {
                ape_writer_append(buf, "while (");
                ape_tostring_expression(buf, expr->exwhilestmt.test);
                ape_writer_append(buf, ")");
                ape_tostring_codeblock(buf, expr->exwhilestmt.body);
            }
            break;
        case APE_EXPR_FORLOOP:
            {
                ape_writer_append(buf, "for (");
                if(expr->exforstmt.init)
                {
                    ape_tostring_expression(buf, expr->exforstmt.init);
                    ape_writer_append(buf, " ");
                }
                else
                {
                    ape_writer_append(buf, ";");
                }
                if(expr->exforstmt.test)
                {
                    ape_tostring_expression(buf, expr->exforstmt.test);
                    ape_writer_append(buf, "; ");
                }
                else
                {
                    ape_writer_append(buf, ";");
                }
                if(expr->exforstmt.update)
                {
                    ape_tostring_expression(buf, expr->exforstmt.test);
                }
                ape_writer_append(buf, ")");
                ape_tostring_codeblock(buf, expr->exforstmt.body);
            }
            break;
        case APE_EXPR_FOREACH:
            {
                ape_writer_append(buf, "for (");
                ape_writer_appendf(buf, "%s", expr->exforeachstmt.iterator->value);
                ape_writer_append(buf, " in ");
                ape_tostring_expression(buf, expr->exforeachstmt.source);
                ape_writer_append(buf, ")");
                ape_tostring_codeblock(buf, expr->exforeachstmt.body);
            }
            break;
        case APE_EXPR_BLOCK:
            {
                ape_tostring_codeblock(buf, expr->exblock);
            }
            break;
        case APE_EXPR_BREAK:
            {
                ape_writer_append(buf, "break");
            }
            break;
        case APE_EXPR_CONTINUE:
            {
                ape_writer_append(buf, "continue");
            }
            break;
        case APE_EXPR_INCLUDE:
            {
                ape_writer_appendf(buf, "include \"%s\"", expr->exincludestmt.path);
            }
            break;
        case APE_EXPR_RECOVER:
            {
                ape_writer_appendf(buf, "recover (%s)", expr->exrecoverstmt.errorident->value);
                ape_tostring_codeblock(buf, expr->exrecoverstmt.body);
            }
            break;
        case APE_EXPR_NONE:
            {
                ape_writer_append(buf, "expression_none");
            }
            break;
    }
    return true;
}

bool ape_tostring_codeblock(ApeWriter_t* buf, ApeAstBlockExpr_t* stmt)
{
    bool ok;
    ApeSize_t i;
    ApeAstExpression_t* istmt;
    ape_writer_append(buf, "{ ");
    for(i = 0; i < ape_ptrarray_count(stmt->statements); i++)
    {
        istmt = (ApeAstExpression_t*)ape_ptrarray_get(stmt->statements, i);
        ok = ape_tostring_expression(buf, istmt);
        ape_writer_append(buf, "\n");
        if(!ok)
        {
            return false;
        }
    }
    ape_writer_append(buf, " }");
    return true;
}

bool ape_tostring_compresult(ApeWriter_t* buf, ApeAstCompResult_t* res, bool sparse)
{
    return ape_tostring_bytecode(buf, res->bytecode, res->srcpositions, res->count, sparse);
}

bool ape_tostring_bytecode(ApeWriter_t* buf, ApeUShort_t* code, ApePosition_t* source_positions, size_t code_size, bool sparse)
{
    enum { kMaxDepth = 128*2 };
    bool ok;
    unsigned int pos;
    ApeOpByte_t operands[2];
    ApeSize_t i;
    ApeSize_t cntdef;
    ApeSize_t cntdepth;
    ApeUShort_t op;
    double dv;
    ApeOpcodeDef_t* def;
    ApePosition_t srcpos;
    pos = 0;
    cntdepth = 0;
    fprintf(stderr, "ape_tostring_bytecode: code_size=%d\n", code_size);
    while((pos < code_size) && (cntdepth < kMaxDepth))
    {
        op = code[pos];
        def = ape_vm_opcodefind(op);
        //APE_ASSERT(def);
        if(sparse)
        {
            cntdef = 0;
            ok = ape_tostring_opcodecoderead(def, code + pos, operands);
            if(!ok)
            {
                fprintf(stderr, "internal error: cannot read opcode at %p\n", code+pos);
                return false;
            }
            for(i = 0; i < def->operandcount; i++)
            {
                cntdef += 1;
                if(def->operandwidths[i] <= 0)
                {
                    pos += 1;
                }
                else
                {
                    pos += def->operandwidths[i];
                }
            }
            ape_writer_appendf(buf, "<%d instructions>", cntdef);
        }
        else
        {
            ape_writer_append(buf, "  ");
            if(source_positions)
            {
                srcpos = source_positions[pos];
                ape_writer_appendf(buf, "[%s:%d:%d]%-2s\t", srcpos.file->path, srcpos.line, srcpos.column, " ");
            }
            ape_writer_appendf(buf, "%04d %s", pos, def->name);
            pos++;
            ok = ape_tostring_opcodecoderead(def, code + pos, operands);
            if(!ok)
            {
                fprintf(stderr, "internal error: cannot read opcode at %p\n", code+pos);
                return false;
            }
            for(i = 0; i < def->operandcount; i++)
            {
                ape_writer_append(buf, " ");
                if(op == APE_OPCODE_MKNUMBER)
                {
                    #if 0
                    dv = ape_util_uinttofloat(operands[i]);
                    #else
                    dv = (ApeFloat_t)operands[i];
                    #endif
                    #if 0
                    ape_writer_appendf(buf, "%1.17g", dv);
                    #else
                    if(dv == ((ApeInt_t)dv))
                    {
                        #if 0
                        ape_writer_appendf(buf, "%" PRId64, (ApeInt_t)fltnum);
                        #else
                        ape_writer_appendf(buf, "int<%" PRIi64 ">", (ApeInt_t)dv);
                        #endif
                    }
                    else
                    {
                        ape_writer_appendf(buf, "flt<%f>", dv);
                    }
                    #endif
                }
                else
                {
                    ape_writer_appendf(buf, "@%llu", (long long unsigned int)operands[i]);
                }
                pos += def->operandwidths[i];
            }
            ape_writer_append(buf, "\n");
        }
        cntdepth++;
    }
    return true;
}


const char* ape_tostring_tokentype(ApeAstTokType_t type)
{
    return g_type_names[type];
}
