
#include "ape.h"

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

const char* ape_tostring_exprtype(ApeExprType_t type)
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
        default:
            break;
    }
    return "unknown";
}



static bool ape_tostring_opcodecoderead(ApeOpcodeDef_t* def, ApeUShort_t* instr, uint64_t outop[2])
{
    ApeSize_t i;
    int offset = 0;
    for(i = 0; i < def->num_operands; i++)
    {
        int opwidth = def->operand_widths[i];
        switch(opwidth)
        {
            case 1:
            {
                outop[i] = instr[offset];
                break;
            }
            case 2:
            {
                uint64_t operand = 0;
                operand = operand | (instr[offset] << 8);
                operand = operand | (instr[offset + 1]);
                outop[i] = operand;
                break;
            }
            case 4:
            {
                uint64_t operand = 0;
                operand = operand | (instr[offset + 0] << 24);
                operand = operand | (instr[offset + 1] << 16);
                operand = operand | (instr[offset + 2] << 8);
                operand = operand | (instr[offset + 3]);
                outop[i] = operand;
                break;
            }
            case 8:
            {
                uint64_t operand = 0;
                operand = operand | ((uint64_t)instr[offset + 0] << 56);
                operand = operand | ((uint64_t)instr[offset + 1] << 48);
                operand = operand | ((uint64_t)instr[offset + 2] << 40);
                operand = operand | ((uint64_t)instr[offset + 3] << 32);
                operand = operand | ((uint64_t)instr[offset + 4] << 24);
                operand = operand | ((uint64_t)instr[offset + 5] << 16);
                operand = operand | ((uint64_t)instr[offset + 6] << 8);
                operand = operand | ((uint64_t)instr[offset + 7]);
                outop[i] = operand;
                break;
            }
            default:
            {
                APE_ASSERT(false);
                return false;
            }
        }
        offset += opwidth;
    }
    return true;
}

bool ape_tostring_stmtlist(ApeWriter_t* buf, ApePtrArray_t* statements)
{
    ApeSize_t i;
    ApeSize_t count;
    ApeStatement_t* stmt;
    count = ape_ptrarray_count(statements);
    for(i = 0; i < count; i++)
    {
        stmt = (ApeStatement_t*)ape_ptrarray_get(statements, i);
        ape_tostring_statement(buf, stmt);
        if(i < (count - 1))
        {
            ape_writer_append(buf, "\n");
        }
    }
    return true;
}

bool ape_tostring_statement(ApeWriter_t* buf, ApeStatement_t* stmt)
{
    ApeSize_t i;
    ApeDefineStmt_t* defstmt;
    ApeIfCase_t* ifcase;
    ApeIfCase_t* elifcase;
    switch(stmt->type)
    {
        case APE_STMT_DEFINE:
            {
                defstmt = &stmt->define;
                if(stmt->define.assignable)
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
        case APE_STMT_IF:
            {
                ifcase = (ApeIfCase_t*)ape_ptrarray_get(stmt->ifstatement.cases, 0);
                ape_writer_append(buf, "if (");
                ape_tostring_expression(buf, ifcase->test);
                ape_writer_append(buf, ") ");
                ape_tostring_codeblock(buf, ifcase->consequence);
                for(i = 1; i < ape_ptrarray_count(stmt->ifstatement.cases); i++)
                {
                    elifcase = (ApeIfCase_t*)ape_ptrarray_get(stmt->ifstatement.cases, i);
                    ape_writer_append(buf, " elif (");
                    ape_tostring_expression(buf, elifcase->test);
                    ape_writer_append(buf, ") ");
                    ape_tostring_codeblock(buf, elifcase->consequence);
                }
                if(stmt->ifstatement.alternative)
                {
                    ape_writer_append(buf, " else ");
                    ape_tostring_codeblock(buf, stmt->ifstatement.alternative);
                }
            }
            break;
        case APE_STMT_RETURNVALUE:
            {
                ape_writer_append(buf, "return ");
                if(stmt->returnvalue)
                {
                    ape_tostring_expression(buf, stmt->returnvalue);
                }
            }
            break;
        case APE_STMT_EXPRESSION:
            {
                if(stmt->expression)
                {
                    ape_tostring_expression(buf, stmt->expression);
                }
            }
            break;
        case APE_STMT_WHILELOOP:
            {
                ape_writer_append(buf, "while (");
                ape_tostring_expression(buf, stmt->whileloop.test);
                ape_writer_append(buf, ")");
                ape_tostring_codeblock(buf, stmt->whileloop.body);
            }
            break;
        case APE_STMT_FORLOOP:
            {
                ape_writer_append(buf, "for (");
                if(stmt->forloop.init)
                {
                    ape_tostring_statement(buf, stmt->forloop.init);
                    ape_writer_append(buf, " ");
                }
                else
                {
                    ape_writer_append(buf, ";");
                }
                if(stmt->forloop.test)
                {
                    ape_tostring_expression(buf, stmt->forloop.test);
                    ape_writer_append(buf, "; ");
                }
                else
                {
                    ape_writer_append(buf, ";");
                }
                if(stmt->forloop.update)
                {
                    ape_tostring_expression(buf, stmt->forloop.test);
                }
                ape_writer_append(buf, ")");
                ape_tostring_codeblock(buf, stmt->forloop.body);
            }
            break;
        case APE_STMT_FOREACH:
            {
                ape_writer_append(buf, "for (");
                ape_writer_appendf(buf, "%s", stmt->foreach.iterator->value);
                ape_writer_append(buf, " in ");
                ape_tostring_expression(buf, stmt->foreach.source);
                ape_writer_append(buf, ")");
                ape_tostring_codeblock(buf, stmt->foreach.body);
            }
            break;
        case APE_STMT_BLOCK:
            {
                ape_tostring_codeblock(buf, stmt->block);
            }
            break;
        case APE_STMT_BREAK:
            {
                ape_writer_append(buf, "break");
            }
            break;
        case APE_STMT_CONTINUE:
            {
                ape_writer_append(buf, "continue");
            }
            break;
        case APE_STMT_INCLUDE:
            {
                ape_writer_appendf(buf, "include \"%s\"", stmt->stmtinclude.path);
            }
            break;
        case APE_STMT_NONE:
            {
                ape_writer_append(buf, "statement_none");
            }
            break;
        case APE_STMT_RECOVER:
            {
                ape_writer_appendf(buf, "recover (%s)", stmt->recover.errorident->value);
                ape_tostring_codeblock(buf, stmt->recover.body);
            }
            break;
    }
    return true;
}

bool ape_tostring_expression(ApeWriter_t* buf, ApeExpression_t* expr)
{
    ApeSize_t i;
    ApeExpression_t* arrexpr;
    ApeMapLiteral_t* mapexpr;
    ApeExpression_t* keyexpr;
    ApeExpression_t* valexpr;
    ApeFnLiteral_t* fnexpr;
    ApeIdent_t* paramexpr;
    ApeExpression_t* argexpr;
    ApeCallExpr_t* callexpr;
    switch(expr->type)
    {
        case APE_EXPR_IDENT:
            {
                ape_writer_append(buf, expr->ident->value);
            }
            break;
        case APE_EXPR_LITERALNUMBER:
            {
                ape_writer_appendf(buf, "%1.17g", expr->numberliteral);
            }
            break;
        case APE_EXPR_LITERALBOOL:
            {
                ape_writer_appendf(buf, "%s", expr->boolliteral ? "true" : "false");
            }
            break;
        case APE_EXPR_LITERALSTRING:
            {
                ape_writer_appendf(buf, "\"%s\"", expr->stringliteral);
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
                for(i = 0; i < ape_ptrarray_count(expr->array); i++)
                {
                    arrexpr = (ApeExpression_t*)ape_ptrarray_get(expr->array, i);
                    ape_tostring_expression(buf, arrexpr);
                    if(i < (ape_ptrarray_count(expr->array) - 1))
                    {
                        ape_writer_append(buf, ", ");
                    }
                }
                ape_writer_append(buf, "]");
            }
            break;
        case APE_EXPR_LITERALMAP:
            {
                mapexpr = &expr->map;
                ape_writer_append(buf, "{");
                for(i = 0; i < ape_ptrarray_count(mapexpr->keys); i++)
                {
                    keyexpr = (ApeExpression_t*)ape_ptrarray_get(mapexpr->keys, i);
                    valexpr = (ApeExpression_t*)ape_ptrarray_get(mapexpr->values, i);
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
                ape_writer_append(buf, ape_tostring_operator(expr->infix.op));
                ape_tostring_expression(buf, expr->prefix.right);
                ape_writer_append(buf, ")");
            }
            break;
        case APE_EXPR_INFIX:
            {
                ape_writer_append(buf, "(");
                ape_tostring_expression(buf, expr->infix.left);
                ape_writer_append(buf, " ");
                ape_writer_append(buf, ape_tostring_operator(expr->infix.op));
                ape_writer_append(buf, " ");
                ape_tostring_expression(buf, expr->infix.right);
                ape_writer_append(buf, ")");
            }
            break;
        case APE_EXPR_LITERALFUNCTION:
            {
                fnexpr = &expr->fnliteral;
                ape_writer_append(buf, "function");
                ape_writer_append(buf, "(");
                for(i = 0; i < ape_ptrarray_count(fnexpr->params); i++)
                {
                    paramexpr = (ApeIdent_t*)ape_ptrarray_get(fnexpr->params, i);
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
                callexpr = &expr->callexpr;
                ape_tostring_expression(buf, callexpr->function);
                ape_writer_append(buf, "(");
                for(i = 0; i < ape_ptrarray_count(callexpr->args); i++)
                {
                    argexpr = (ApeExpression_t*)ape_ptrarray_get(callexpr->args, i);
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
                ape_tostring_expression(buf, expr->indexexpr.left);
                ape_writer_append(buf, "[");
                ape_tostring_expression(buf, expr->indexexpr.index);
                ape_writer_append(buf, "])");
            }
            break;
        case APE_EXPR_ASSIGN:
            {
                ape_tostring_expression(buf, expr->assign.dest);
                ape_writer_append(buf, " = ");
                ape_tostring_expression(buf, expr->assign.source);
            }
            break;
        case APE_EXPR_LOGICAL:
            {
                ape_tostring_expression(buf, expr->logical.left);
                ape_writer_append(buf, " ");
                ape_writer_append(buf, ape_tostring_operator(expr->infix.op));
                ape_writer_append(buf, " ");
                ape_tostring_expression(buf, expr->logical.right);
            }
            break;
        case APE_EXPR_TERNARY:
            {
                ape_tostring_expression(buf, expr->ternary.test);
                ape_writer_append(buf, " ? ");
                ape_tostring_expression(buf, expr->ternary.iftrue);
                ape_writer_append(buf, " : ");
                ape_tostring_expression(buf, expr->ternary.iffalse);
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

bool ape_tostring_codeblock(ApeWriter_t* buf, ApeCodeblock_t* stmt)
{
    bool ok;
    ApeSize_t i;
    ApeStatement_t* istmt;
    ape_writer_append(buf, "{ ");
    for(i = 0; i < ape_ptrarray_count(stmt->statements); i++)
    {
        istmt = (ApeStatement_t*)ape_ptrarray_get(stmt->statements, i);
        ok = ape_tostring_statement(buf, istmt);
        ape_writer_append(buf, "\n");
        if(!ok)
        {
            return false;
        }
    }
    ape_writer_append(buf, " }");
    return true;
}

bool ape_tostring_compresult(ApeWriter_t* buf, ApeCompResult_t* res)
{
    return ape_tostring_bytecode(buf, res->bytecode, res->srcpositions, res->count);
}

bool ape_tostring_bytecode(ApeWriter_t* buf, ApeUShort_t* code, ApePosition_t* source_positions, size_t code_size)
{
    bool ok;
    unsigned int pos;
    uint64_t operands[2];
    ApeSize_t i;
    ApeUShort_t op;
    ApeFloat_t dv;
    ApeOpcodeDef_t* def;
    ApePosition_t srcpos;
    pos = 0;
    while(pos < code_size)
    {
        op = code[pos];
        def = ape_vm_opcodefind(op);
        APE_ASSERT(def);
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
        for(i = 0; i < def->num_operands; i++)
        {
            ape_writer_append(buf, " ");
            if(op == APE_OPCODE_MKNUMBER)
            {
                dv = ape_util_uinttofloat(operands[i]);
                ape_writer_appendf(buf, "%1.17g", dv);
            }
            else
            {
                ape_writer_appendf(buf, "@%llu", (long long unsigned int)operands[i]);
            }
            pos += def->operand_widths[i];
        }
        ape_writer_append(buf, "\n");
    }
    return true;
}


const char* ape_tostring_tokentype(ApeTokenType_t type)
{
    return g_type_names[type];
}
