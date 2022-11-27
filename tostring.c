
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
    "for",      "in",    "continue", "null",   "import",
    "recover",  "ident", "number",   "string", "templatestring",
};

/*
* todo: these MUST reflect the order of ApeOpcodeValue_t.
* meaning its prone to break terribly if and/or when it is changed.
*/
static ApeOpcodeDefinition_t g_definitions[OPCODE_MAX + 1] =
{
    { "none", 0, { 0 } },
    { "constant", 1, { 2 } },
    { "op(+)", 0, { 0 } },
    { "pop", 0, { 0 } },
    { "sub", 0, { 0 } },
    { "op(*)", 0, { 0 } },
    { "op(/)", 0, { 0 } },
    { "op(%)", 0, { 0 } },
    { "true", 0, { 0 } },
    { "false", 0, { 0 } },
    { "compare", 0, { 0 } },
    { "compareeq", 0, { 0 } },
    { "equal", 0, { 0 } },
    { "notequal", 0, { 0 } },
    { "greaterthan", 0, { 0 } },
    { "greaterequal", 0, { 0 } },
    { "op(-)", 0, { 0 } },
    { "op(!)", 0, { 0 } },
    { "jump", 1, { 2 } },
    { "jumpiffalse", 1, { 2 } },
    { "jumpiftrue", 1, { 2 } },
    { "null", 0, { 0 } },
    { "getmoduleglobal", 1, { 2 } },
    { "setmoduleglobal", 1, { 2 } },
    { "definemoduleglobal", 1, { 2 } },
    { "array", 1, { 2 } },
    { "mapstart", 1, { 2 } },
    { "mapend", 1, { 2 } },
    { "getthis", 0, { 0 } },
    { "getindex", 0, { 0 } },
    { "setindex", 0, { 0 } },
    { "getvalue_at", 0, { 0 } },
    { "call", 1, { 1 } },
    { "returnvalue", 0, { 0 } },
    { "return", 0, { 0 } },
    { "getlocal", 1, { 1 } },
    { "definelocal", 1, { 1 } },
    { "setlocal", 1, { 1 } },
    { "getcontextglobal", 1, { 2 } },
    { "function", 2, { 2, 1 } },
    { "getfree", 1, { 1 } },
    { "setfree", 1, { 1 } },
    { "currentfunction", 0, { 0 } },
    { "dup", 0, { 0 } },
    { "number", 1, { 8 } },
    { "len", 0, { 0 } },
    { "setrecover", 1, { 2 } },
    { "op(|)", 0, { 0 } },
    { "op(^)", 0, { 0 } },
    { "op(&)", 0, { 0 } },
    { "op(<<)", 0, { 0 } },
    { "op(>>)", 0, { 0 } },
    { "invalid_max", 0, { 0 } },
};

ApeOpcodeDefinition_t* ape_tostring_opcodefind(ApeOpByte_t op)
{
    if(op <= OPCODE_NONE || op >= OPCODE_MAX)
    {
        return NULL;
    }
    return &g_definitions[op];
}

const char* ape_tostring_opcodename(ApeOpByte_t op)
{
    if(op <= OPCODE_NONE || op >= OPCODE_MAX)
    {
        return NULL;
    }
    return g_definitions[op].name;
}

static bool ape_tostrign_opcodecoderead(ApeOpcodeDefinition_t* def, ApeUShort_t* instr, uint64_t outop[2])
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

bool ape_tostring_stmtlist(ApePtrArray_t* statements, ApeWriter_t* buf)
{
    ApeSize_t i;
    ApeSize_t count;
    const ApeStatement_t* stmt;
    count = ape_ptrarray_count(statements);
    for(i = 0; i < count; i++)
    {
        stmt = (ApeStatement_t*)ape_ptrarray_get(statements, i);
        ape_tostring_statement(stmt, buf);
        if(i < (count - 1))
        {
            ape_writer_append(buf, "\n");
        }
    }
    return true;
}

bool ape_tostring_statement(const ApeStatement_t* stmt, ApeWriter_t* buf)
{
    ApeSize_t i;
    ApeDefineStmt_t* defstmt;
    ApeIfCase_t* ifcase;
    ApeIfCase_t* elifcase;
    switch(stmt->type)
    {
        case STATEMENT_DEFINE:
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
                    ape_tostring_expression(defstmt->value, buf);
                }
            }
            break;
        case STATEMENT_IF:
            {
                ifcase = (ApeIfCase_t*)ape_ptrarray_get(stmt->ifstatement.cases, 0);
                ape_writer_append(buf, "if (");
                ape_tostring_expression(ifcase->test, buf);
                ape_writer_append(buf, ") ");
                ape_tostring_codeblock(ifcase->consequence, buf);
                for(i = 1; i < ape_ptrarray_count(stmt->ifstatement.cases); i++)
                {
                    elifcase = (ApeIfCase_t*)ape_ptrarray_get(stmt->ifstatement.cases, i);
                    ape_writer_append(buf, " elif (");
                    ape_tostring_expression(elifcase->test, buf);
                    ape_writer_append(buf, ") ");
                    ape_tostring_codeblock(elifcase->consequence, buf);
                }
                if(stmt->ifstatement.alternative)
                {
                    ape_writer_append(buf, " else ");
                    ape_tostring_codeblock(stmt->ifstatement.alternative, buf);
                }
            }
            break;
        case STATEMENT_RETURN_VALUE:
            {
                ape_writer_append(buf, "return ");
                if(stmt->returnvalue)
                {
                    ape_tostring_expression(stmt->returnvalue, buf);
                }
            }
            break;
        case STATEMENT_EXPRESSION:
            {
                if(stmt->expression)
                {
                    ape_tostring_expression(stmt->expression, buf);
                }
            }
            break;
        case STATEMENT_WHILE_LOOP:
            {
                ape_writer_append(buf, "while (");
                ape_tostring_expression(stmt->whileloop.test, buf);
                ape_writer_append(buf, ")");
                ape_tostring_codeblock(stmt->whileloop.body, buf);
            }
            break;
        case STATEMENT_FOR_LOOP:
            {
                ape_writer_append(buf, "for (");
                if(stmt->forloop.init)
                {
                    ape_tostring_statement(stmt->forloop.init, buf);
                    ape_writer_append(buf, " ");
                }
                else
                {
                    ape_writer_append(buf, ";");
                }
                if(stmt->forloop.test)
                {
                    ape_tostring_expression(stmt->forloop.test, buf);
                    ape_writer_append(buf, "; ");
                }
                else
                {
                    ape_writer_append(buf, ";");
                }
                if(stmt->forloop.update)
                {
                    ape_tostring_expression(stmt->forloop.test, buf);
                }
                ape_writer_append(buf, ")");
                ape_tostring_codeblock(stmt->forloop.body, buf);
            }
            break;
        case STATEMENT_FOREACH:
            {
                ape_writer_append(buf, "for (");
                ape_writer_appendf(buf, "%s", stmt->foreach.iterator->value);
                ape_writer_append(buf, " in ");
                ape_tostring_expression(stmt->foreach.source, buf);
                ape_writer_append(buf, ")");
                ape_tostring_codeblock(stmt->foreach.body, buf);
            }
            break;
        case STATEMENT_BLOCK:
            {
                ape_tostring_codeblock(stmt->block, buf);
            }
            break;
        case STATEMENT_BREAK:
            {
                ape_writer_append(buf, "break");
            }
            break;
        case STATEMENT_CONTINUE:
            {
                ape_writer_append(buf, "continue");
            }
            break;
        case STATEMENT_IMPORT:
            {
                ape_writer_appendf(buf, "import \"%s\"", stmt->import.path);
            }
            break;
        case STATEMENT_NONE:
            {
                ape_writer_append(buf, "statement_none");
            }
            break;
        case STATEMENT_RECOVER:
            {
                ape_writer_appendf(buf, "recover (%s)", stmt->recover.errorident->value);
                ape_tostring_codeblock(stmt->recover.body, buf);
            }
            break;
    }
    return true;
}

bool ape_tostring_expression(ApeExpression_t* expr, ApeWriter_t* buf)
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
        case EXPRESSION_IDENT:
            {
                ape_writer_append(buf, expr->ident->value);
            }
            break;
        case EXPRESSION_NUMBER_LITERAL:
            {
                ape_writer_appendf(buf, "%1.17g", expr->numberliteral);
            }
            break;
        case EXPRESSION_BOOL_LITERAL:
            {
                ape_writer_appendf(buf, "%s", expr->boolliteral ? "true" : "false");
            }
            break;
        case EXPRESSION_STRING_LITERAL:
            {
                ape_writer_appendf(buf, "\"%s\"", expr->stringliteral);
            }
            break;
        case EXPRESSION_NULL_LITERAL:
            {
                ape_writer_append(buf, "null");
            }
            break;
        case EXPRESSION_ARRAY_LITERAL:
            {
                ape_writer_append(buf, "[");
                for(i = 0; i < ape_ptrarray_count(expr->array); i++)
                {
                    arrexpr = (ApeExpression_t*)ape_ptrarray_get(expr->array, i);
                    ape_tostring_expression(arrexpr, buf);
                    if(i < (ape_ptrarray_count(expr->array) - 1))
                    {
                        ape_writer_append(buf, ", ");
                    }
                }
                ape_writer_append(buf, "]");
            }
            break;
        case EXPRESSION_MAP_LITERAL:
            {
                mapexpr = &expr->map;
                ape_writer_append(buf, "{");
                for(i = 0; i < ape_ptrarray_count(mapexpr->keys); i++)
                {
                    keyexpr = (ApeExpression_t*)ape_ptrarray_get(mapexpr->keys, i);
                    valexpr = (ApeExpression_t*)ape_ptrarray_get(mapexpr->values, i);
                    ape_tostring_expression(keyexpr, buf);
                    ape_writer_append(buf, " : ");
                    ape_tostring_expression(valexpr, buf);
                    if(i < (ape_ptrarray_count(mapexpr->keys) - 1))
                    {
                        ape_writer_append(buf, ", ");
                    }
                }
                ape_writer_append(buf, "}");
            }
            break;
        case EXPRESSION_PREFIX:
            {
                ape_writer_append(buf, "(");
                ape_writer_append(buf, ape_tostring_operator(expr->infix.op));
                ape_tostring_expression(expr->prefix.right, buf);
                ape_writer_append(buf, ")");
            }
            break;
        case EXPRESSION_INFIX:
            {
                ape_writer_append(buf, "(");
                ape_tostring_expression(expr->infix.left, buf);
                ape_writer_append(buf, " ");
                ape_writer_append(buf, ape_tostring_operator(expr->infix.op));
                ape_writer_append(buf, " ");
                ape_tostring_expression(expr->infix.right, buf);
                ape_writer_append(buf, ")");
            }
            break;
        case EXPRESSION_FUNCTION_LITERAL:
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
                ape_tostring_codeblock(fnexpr->body, buf);
            }
            break;
        case EXPRESSION_CALL:
            {
                callexpr = &expr->callexpr;
                ape_tostring_expression(callexpr->function, buf);
                ape_writer_append(buf, "(");
                for(i = 0; i < ape_ptrarray_count(callexpr->args); i++)
                {
                    argexpr = (ApeExpression_t*)ape_ptrarray_get(callexpr->args, i);
                    ape_tostring_expression(argexpr, buf);
                    if(i < (ape_ptrarray_count(callexpr->args) - 1))
                    {
                        ape_writer_append(buf, ", ");
                    }
                }
                ape_writer_append(buf, ")");
            }
            break;
        case EXPRESSION_INDEX:
            {
                ape_writer_append(buf, "(");
                ape_tostring_expression(expr->indexexpr.left, buf);
                ape_writer_append(buf, "[");
                ape_tostring_expression(expr->indexexpr.index, buf);
                ape_writer_append(buf, "])");
            }
            break;
        case EXPRESSION_ASSIGN:
            {
                ape_tostring_expression(expr->assign.dest, buf);
                ape_writer_append(buf, " = ");
                ape_tostring_expression(expr->assign.source, buf);
            }
            break;
        case EXPRESSION_LOGICAL:
            {
                ape_tostring_expression(expr->logical.left, buf);
                ape_writer_append(buf, " ");
                ape_writer_append(buf, ape_tostring_operator(expr->infix.op));
                ape_writer_append(buf, " ");
                ape_tostring_expression(expr->logical.right, buf);
            }
            break;
        case EXPRESSION_TERNARY:
            {
                ape_tostring_expression(expr->ternary.test, buf);
                ape_writer_append(buf, " ? ");
                ape_tostring_expression(expr->ternary.iftrue, buf);
                ape_writer_append(buf, " : ");
                ape_tostring_expression(expr->ternary.iffalse, buf);
            }
            break;
        case EXPRESSION_NONE:
            {
                ape_writer_append(buf, "expression_none");
            }
            break;
    }
    return true;
}

bool ape_tostring_codeblock(const ApeCodeblock_t* stmt, ApeWriter_t* buf)
{
    bool ok;
    ApeSize_t i;
    ApeStatement_t* istmt;
    ape_writer_append(buf, "{ ");
    for(i = 0; i < ape_ptrarray_count(stmt->statements); i++)
    {
        istmt = (ApeStatement_t*)ape_ptrarray_get(stmt->statements, i);
        ok = ape_tostring_statement(istmt, buf);
        ape_writer_append(buf, "\n");
        if(!ok)
        {
            return false;
        }
    }
    ape_writer_append(buf, " }");
    return true;
}

const char* ape_tostring_operator(ApeOperator_t op)
{
    switch(op)
    {
        case OPERATOR_NONE:
            return "operator_none";
        case OPERATOR_ASSIGN:
            return "=";
        case OPERATOR_PLUS:
            return "+";
        case OPERATOR_MINUS:
            return "-";
        case OPERATOR_BANG:
            return "!";
        case OPERATOR_ASTERISK:
            return "*";
        case OPERATOR_SLASH:
            return "/";
        case OPERATOR_LT:
            return "<";
        case OPERATOR_GT:
            return ">";
        case OPERATOR_EQ:
            return "==";
        case OPERATOR_NOT_EQ:
            return "!=";
        case OPERATOR_MODULUS:
            return "%";
        case OPERATOR_LOGICAL_AND:
            return "&&";
        case OPERATOR_LOGICAL_OR:
            return "||";
        case OPERATOR_BIT_AND:
            return "&";
        case OPERATOR_BIT_OR:
            return "|";
        case OPERATOR_BIT_XOR:
            return "^";
        case OPERATOR_LSHIFT:
            return "<<";
        case OPERATOR_RSHIFT:
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
        case EXPRESSION_NONE:
            return "none";
        case EXPRESSION_IDENT:
            return "identifier";
        case EXPRESSION_NUMBER_LITERAL:
            return "litint";
        case EXPRESSION_BOOL_LITERAL:
            return "litbool";
        case EXPRESSION_STRING_LITERAL:
            return "litstring";
        case EXPRESSION_ARRAY_LITERAL:
            return "litarray";
        case EXPRESSION_MAP_LITERAL:
            return "litmap";
        case EXPRESSION_PREFIX:
            return "prefix";
        case EXPRESSION_INFIX:
            return "infix";
        case EXPRESSION_FUNCTION_LITERAL:
            return "litfunc";
        case EXPRESSION_CALL:
            return "call";
        case EXPRESSION_INDEX:
            return "index";
        case EXPRESSION_ASSIGN:
            return "assign";
        case EXPRESSION_LOGICAL:
            return "logical";
        case EXPRESSION_TERNARY:
            return "ternary";
        default:
            break;
    }
    return "unknown";
}

bool ape_tostring_compresult(ApeCompilationResult_t* res, ApeWriter_t* buf)
{
    return ape_tostring_code(res->bytecode, res->srcpositions, res->count, buf);
}

bool ape_tostring_code(ApeUShort_t* code, ApePosition_t* source_positions, size_t code_size, ApeWriter_t* res)
{
    ApeSize_t i;
    unsigned pos = 0;
    while(pos < code_size)
    {
        ApeUShort_t op = code[pos];
        ApeOpcodeDefinition_t* def = ape_tostring_opcodefind(op);
        APE_ASSERT(def);
        if(source_positions)
        {
            ApePosition_t src_pos = source_positions[pos];
            ape_writer_appendf(res, "%d:%-4d\t%04d\t%s", src_pos.line, src_pos.column, pos, def->name);
        }
        else
        {
            ape_writer_appendf(res, "%04d %s", pos, def->name);
        }
        pos++;

        uint64_t operands[2];
        bool ok = ape_tostrign_opcodecoderead(def, code + pos, operands);
        if(!ok)
        {
            return false;
        }
        for(i = 0; i < def->num_operands; i++)
        {
            if(op == OPCODE_NUMBER)
            {
                ApeFloat_t val_double = ape_util_uinttofloat(operands[i]);
                ape_writer_appendf(res, " %1.17g", val_double);
            }
            else
            {
                ape_writer_appendf(res, " %llu", (long long unsigned int)operands[i]);
            }
            pos += def->operand_widths[i];
        }
        ape_writer_append(res, "\n");
    }
    return true;
}


const char* ape_tostring_tokentype(ApeTokenType_t type)
{
    return g_type_names[type];
}
