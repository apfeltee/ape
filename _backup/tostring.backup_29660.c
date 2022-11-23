
#include "ape.h"

static const char* g_type_names[] = {
    "ILLEGAL",  "EOF",   "=",        "+=",     "-=",
    "*=",       "/=",    "%=",       "&=",     "|=",
    "^=",       "<<=",   ">>=",      "?",      "+",
    "++",       "-",     "--",       "!",      "*",
    "/",        "<",     "<=",       ">",      ">=",
    "==",       "!=",    "&&",       "||",     "&",
    "|",        "^",     "<<",       ">>",     ",",
    ";",        ":",     "(",        ")",      "{",
    "}",        "[",     "]",        ".",      "%",
    "FUNCTION", "CONST", "VAR",      "TRUE",   "FALSE",
    "IF",       "ELSE",  "RETURN",   "WHILE",  "BREAK",
    "FOR",      "IN",    "CONTINUE", "NULL",   "IMPORT",
    "RECOVER",  "IDENT", "NUMBER",   "STRING", "TEMPLATE_STRING",
};

static bool code_read_operands(ApeOpcodeDefinition_t* def, ApeUShort_t* instr, uint64_t out_operands[2])
{
    ApeSize_t i;
    int offset = 0;
    for(i = 0; i < def->num_operands; i++)
    {
        int operand_width = def->operand_widths[i];
        switch(operand_width)
        {
            case 1:
            {
                out_operands[i] = instr[offset];
                break;
            }
            case 2:
            {
                uint64_t operand = 0;
                operand = operand | (instr[offset] << 8);
                operand = operand | (instr[offset + 1]);
                out_operands[i] = operand;
                break;
            }
            case 4:
            {
                uint64_t operand = 0;
                operand = operand | (instr[offset + 0] << 24);
                operand = operand | (instr[offset + 1] << 16);
                operand = operand | (instr[offset + 2] << 8);
                operand = operand | (instr[offset + 3]);
                out_operands[i] = operand;
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
                out_operands[i] = operand;
                break;
            }
            default:
            {
                APE_ASSERT(false);
                return false;
            }
        }
        offset += operand_width;
    }
    return true;
    ;
}

char* statements_tostring(ApeAllocator_t* alloc, ApePtrArray_t * statements)
{
    ApeSize_t i;
    int count;
    const ApeStatement_t* stmt;
    ApeStringBuffer_t* buf;
    buf = strbuf_make(alloc);
    if(!buf)
    {
        return NULL;
    }
    count = ptrarray_count(statements);
    for(i = 0; i < count; i++)
    {
        stmt = (ApeStatement_t*)ptrarray_get(statements, i);
        statement_tostring(stmt, buf);
        if(i < (count - 1))
        {
            strbuf_append(buf, "\n");
        }
    }
    return strbuf_get_string_and_destroy(buf);
}

void statement_tostring(const ApeStatement_t* stmt, ApeStringBuffer_t* buf)
{
    ApeSize_t i;
    const ApeDefineStmt_t* def_stmt;
    ApeIfCase_t* if_case;
    ApeIfCase_t* elif_case;
    switch(stmt->type)
    {
        case STATEMENT_DEFINE:
        {
            def_stmt = &stmt->define;
            if(stmt->define.assignable)
            {
                strbuf_append(buf, "var ");
            }
            else
            {
                strbuf_append(buf, "const ");
            }
            strbuf_append(buf, def_stmt->name->value);
            strbuf_append(buf, " = ");

            if(def_stmt->value)
            {
                expression_tostring(def_stmt->value, buf);
            }

            break;
        }
        case STATEMENT_IF:
        {
            if_case = (ApeIfCase_t*)ptrarray_get(stmt->if_statement.cases, 0);
            strbuf_append(buf, "if (");
            expression_tostring(if_case->test, buf);
            strbuf_append(buf, ") ");
            codeblock_tostring(if_case->consequence, buf);
            for(i = 1; i < ptrarray_count(stmt->if_statement.cases); i++)
            {
                elif_case = (ApeIfCase_t*)ptrarray_get(stmt->if_statement.cases, i);
                strbuf_append(buf, " elif (");
                expression_tostring(elif_case->test, buf);
                strbuf_append(buf, ") ");
                codeblock_tostring(elif_case->consequence, buf);
            }
            if(stmt->if_statement.alternative)
            {
                strbuf_append(buf, " else ");
                codeblock_tostring(stmt->if_statement.alternative, buf);
            }
            break;
        }
        case STATEMENT_RETURN_VALUE:
        {
            strbuf_append(buf, "return ");
            if(stmt->return_value)
            {
                expression_tostring(stmt->return_value, buf);
            }
            break;
        }
        case STATEMENT_EXPRESSION:
        {
            if(stmt->expression)
            {
                expression_tostring(stmt->expression, buf);
            }
            break;
        }
        case STATEMENT_WHILE_LOOP:
        {
            strbuf_append(buf, "while (");
            expression_tostring(stmt->while_loop.test, buf);
            strbuf_append(buf, ")");
            codeblock_tostring(stmt->while_loop.body, buf);
            break;
        }
        case STATEMENT_FOR_LOOP:
        {
            strbuf_append(buf, "for (");
            if(stmt->for_loop.init)
            {
                statement_tostring(stmt->for_loop.init, buf);
                strbuf_append(buf, " ");
            }
            else
            {
                strbuf_append(buf, ";");
            }
            if(stmt->for_loop.test)
            {
                expression_tostring(stmt->for_loop.test, buf);
                strbuf_append(buf, "; ");
            }
            else
            {
                strbuf_append(buf, ";");
            }
            if(stmt->for_loop.update)
            {
                expression_tostring(stmt->for_loop.test, buf);
            }
            strbuf_append(buf, ")");
            codeblock_tostring(stmt->for_loop.body, buf);
            break;
        }
        case STATEMENT_FOREACH:
        {
            strbuf_append(buf, "for (");
            strbuf_appendf(buf, "%s", stmt->foreach.iterator->value);
            strbuf_append(buf, " in ");
            expression_tostring(stmt->foreach.source, buf);
            strbuf_append(buf, ")");
            codeblock_tostring(stmt->foreach.body, buf);
            break;
        }
        case STATEMENT_BLOCK:
        {
            codeblock_tostring(stmt->block, buf);
            break;
        }
        case STATEMENT_BREAK:
        {
            strbuf_append(buf, "break");
            break;
        }
        case STATEMENT_CONTINUE:
        {
            strbuf_append(buf, "continue");
            break;
        }
        case STATEMENT_IMPORT:
        {
            strbuf_appendf(buf, "import \"%s\"", stmt->import.path);
            break;
        }
        case STATEMENT_NONE:
        {
            strbuf_append(buf, "STATEMENT_NONE");
            break;
        }
        case STATEMENT_RECOVER:
        {
            strbuf_appendf(buf, "recover (%s)", stmt->recover.error_ident->value);
            codeblock_tostring(stmt->recover.body, buf);
            break;
        }
    }
}

void expression_tostring(ApeExpression_t* expr, ApeStringBuffer_t* buf)
{
    ApeSize_t i;
    ApeExpression_t* arr_expr;
    ApeMapLiteral_t* map;
    switch(expr->type)
    {
        case EXPRESSION_IDENT:
        {
            strbuf_append(buf, expr->ident->value);
            break;
        }
        case EXPRESSION_NUMBER_LITERAL:
        {
            strbuf_appendf(buf, "%1.17g", expr->number_literal);
            break;
        }
        case EXPRESSION_BOOL_LITERAL:
        {
            strbuf_appendf(buf, "%s", expr->bool_literal ? "true" : "false");
            break;
        }
        case EXPRESSION_STRING_LITERAL:
        {
            strbuf_appendf(buf, "\"%s\"", expr->string_literal);
            break;
        }
        case EXPRESSION_NULL_LITERAL:
        {
            strbuf_append(buf, "null");
            break;
        }
        case EXPRESSION_ARRAY_LITERAL:
        {
            strbuf_append(buf, "[");
            for(i = 0; i < ptrarray_count(expr->array); i++)
            {
                arr_expr = (ApeExpression_t*)ptrarray_get(expr->array, i);
                expression_tostring(arr_expr, buf);
                if(i < (ptrarray_count(expr->array) - 1))
                {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, "]");
            break;
        }
        case EXPRESSION_MAP_LITERAL:
        {
            map = &expr->map;
            strbuf_append(buf, "{");
            for(i = 0; i < ptrarray_count(map->keys); i++)
            {
                ApeExpression_t* key_expr = (ApeExpression_t*)ptrarray_get(map->keys, i);
                ApeExpression_t* val_expr = (ApeExpression_t*)ptrarray_get(map->values, i);
                expression_tostring(key_expr, buf);
                strbuf_append(buf, " : ");
                expression_tostring(val_expr, buf);
                if(i < (ptrarray_count(map->keys) - 1))
                {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, "}");
            break;
        }
        case EXPRESSION_PREFIX:
        {
            strbuf_append(buf, "(");
            strbuf_append(buf, operator_tostring(expr->infix.op));
            expression_tostring(expr->prefix.right, buf);
            strbuf_append(buf, ")");
            break;
        }
        case EXPRESSION_INFIX:
        {
            strbuf_append(buf, "(");
            expression_tostring(expr->infix.left, buf);
            strbuf_append(buf, " ");
            strbuf_append(buf, operator_tostring(expr->infix.op));
            strbuf_append(buf, " ");
            expression_tostring(expr->infix.right, buf);
            strbuf_append(buf, ")");
            break;
        }
        case EXPRESSION_FUNCTION_LITERAL:
        {
            ApeFnLiteral_t* fn = &expr->fn_literal;

            strbuf_append(buf, "fn");

            strbuf_append(buf, "(");
            for(i = 0; i < ptrarray_count(fn->params); i++)
            {
                ApeIdent_t* param = (ApeIdent_t*)ptrarray_get(fn->params, i);
                strbuf_append(buf, param->value);
                if(i < (ptrarray_count(fn->params) - 1))
                {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, ") ");

            codeblock_tostring(fn->body, buf);

            break;
        }
        case EXPRESSION_CALL:
        {
            ApeCallExpr_t* call_expr = &expr->call_expr;

            expression_tostring(call_expr->function, buf);

            strbuf_append(buf, "(");
            for(i = 0; i < ptrarray_count(call_expr->args); i++)
            {
                ApeExpression_t* arg = (ApeExpression_t*)ptrarray_get(call_expr->args, i);
                expression_tostring(arg, buf);
                if(i < (ptrarray_count(call_expr->args) - 1))
                {
                    strbuf_append(buf, ", ");
                }
            }
            strbuf_append(buf, ")");

            break;
        }
        case EXPRESSION_INDEX:
        {
            strbuf_append(buf, "(");
            expression_tostring(expr->index_expr.left, buf);
            strbuf_append(buf, "[");
            expression_tostring(expr->index_expr.index, buf);
            strbuf_append(buf, "])");
            break;
        }
        case EXPRESSION_ASSIGN:
        {
            expression_tostring(expr->assign.dest, buf);
            strbuf_append(buf, " = ");
            expression_tostring(expr->assign.source, buf);
            break;
        }
        case EXPRESSION_LOGICAL:
        {
            expression_tostring(expr->logical.left, buf);
            strbuf_append(buf, " ");
            strbuf_append(buf, operator_tostring(expr->infix.op));
            strbuf_append(buf, " ");
            expression_tostring(expr->logical.right, buf);
            break;
        }
        case EXPRESSION_TERNARY:
        {
            expression_tostring(expr->ternary.test, buf);
            strbuf_append(buf, " ? ");
            expression_tostring(expr->ternary.if_true, buf);
            strbuf_append(buf, " : ");
            expression_tostring(expr->ternary.if_false, buf);
            break;
        }
        case EXPRESSION_NONE:
        {
            strbuf_append(buf, "EXPRESSION_NONE");
            break;
        }
    }
}

void codeblock_tostring(const ApeCodeblock_t* stmt, ApeStringBuffer_t* buf)
{
    ApeSize_t i;
    ApeStatement_t* istmt;
    strbuf_append(buf, "{ ");
    for(i = 0; i < ptrarray_count(stmt->statements); i++)
    {
        istmt = (ApeStatement_t*)ptrarray_get(stmt->statements, i);
        statement_tostring(istmt, buf);
        strbuf_append(buf, "\n");
    }
    strbuf_append(buf, " }");
}

const char* operator_tostring(ApeOperator_t op)
{
    switch(op)
    {
        case OPERATOR_NONE:
            return "OPERATOR_NONE";
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
            return "OPERATOR_UNKNOWN";
    }
}

const char* expressiontype_tostring(ApeExprType_t type)
{
    switch(type)
    {
        case EXPRESSION_NONE:
            return "NONE";
        case EXPRESSION_IDENT:
            return "IDENT";
        case EXPRESSION_NUMBER_LITERAL:
            return "INT_LITERAL";
        case EXPRESSION_BOOL_LITERAL:
            return "BOOL_LITERAL";
        case EXPRESSION_STRING_LITERAL:
            return "STRING_LITERAL";
        case EXPRESSION_ARRAY_LITERAL:
            return "ARRAY_LITERAL";
        case EXPRESSION_MAP_LITERAL:
            return "MAP_LITERAL";
        case EXPRESSION_PREFIX:
            return "PREFIX";
        case EXPRESSION_INFIX:
            return "INFIX";
        case EXPRESSION_FUNCTION_LITERAL:
            return "FN_LITERAL";
        case EXPRESSION_CALL:
            return "CALL";
        case EXPRESSION_INDEX:
            return "INDEX";
        case EXPRESSION_ASSIGN:
            return "ASSIGN";
        case EXPRESSION_LOGICAL:
            return "LOGICAL";
        case EXPRESSION_TERNARY:
            return "TERNARY";
        default:
            return "UNKNOWN";
    }
}

void code_tostring(ApeUShort_t* code, ApePosition_t* source_positions, size_t code_size, ApeStringBuffer_t* res)
{
    ApeSize_t i;
    unsigned pos = 0;
    while(pos < code_size)
    {
        ApeUShort_t op = code[pos];
        ApeOpcodeDefinition_t* def = opcode_lookup(op);
        APE_ASSERT(def);
        if(source_positions)
        {
            ApePosition_t src_pos = source_positions[pos];
            strbuf_appendf(res, "%d:%-4d\t%04d\t%s", src_pos.line, src_pos.column, pos, def->name);
        }
        else
        {
            strbuf_appendf(res, "%04d %s", pos, def->name);
        }
        pos++;

        uint64_t operands[2];
        bool ok = code_read_operands(def, code + pos, operands);
        if(!ok)
        {
            return;
        }
        for(i = 0; i < def->num_operands; i++)
        {
            if(op == OPCODE_NUMBER)
            {
                ApeFloat_t val_double = util_uint64_to_double(operands[i]);
                strbuf_appendf(res, " %1.17g", val_double);
            }
            else
            {
                strbuf_appendf(res, " %llu", (long long unsigned int)operands[i]);
            }
            pos += def->operand_widths[i];
        }
        strbuf_append(res, "\n");
    }
    return;
}

void object_tostring(ApeObject_t obj, ApeStringBuffer_t* buf, bool quote_str)
{
    ApeSize_t i;
    ApeObjectType_t type;
    type = object_value_type(obj);
    switch(type)
    {
        case APE_OBJECT_FREED:
        {
            strbuf_append(buf, "FREED");
            break;
        }
        case APE_OBJECT_NONE:
        {
            strbuf_append(buf, "NONE");
            break;
        }
        case APE_OBJECT_NUMBER:
        {
            ApeFloat_t number = object_value_asnumber(obj);
            strbuf_appendf(buf, "%1.10g", number);
            break;
        }
        case APE_OBJECT_BOOL:
        {
            strbuf_append(buf, object_value_asbool(obj) ? "true" : "false");
            break;
        }
        case APE_OBJECT_STRING:
            {
                const char* string = object_string_getdata(obj);
                if(quote_str)
                {
                    strbuf_appendf(buf, "\"%s\"", string);
                }
                else
                {
                    strbuf_append(buf, string);
                }
            }
            break;
        case APE_OBJECT_NULL:
            {
                strbuf_append(buf, "null");
            }
            break;

        case APE_OBJECT_FUNCTION:
            {
                const ApeFunction_t* function = object_value_asfunction(obj);
                strbuf_appendf(buf, "CompiledFunction: %s\n", object_function_getname(obj));
                code_tostring(function->comp_result->bytecode, function->comp_result->src_positions, function->comp_result->count, buf);
            }
            break;

        case APE_OBJECT_ARRAY:
            {
                strbuf_append(buf, "[");
                for(i = 0; i < object_array_getlength(obj); i++)
                {
                    ApeObject_t iobj = object_array_getvalue(obj, i);
                    object_tostring(iobj, buf, true);
                    if(i < (object_array_getlength(obj) - 1))
                    {
                        strbuf_append(buf, ", ");
                    }
                }
                strbuf_append(buf, "]");
            }
            break;
        case APE_OBJECT_MAP:
            {
                strbuf_append(buf, "{");
                for(i = 0; i < object_map_getlength(obj); i++)
                {
                    ApeObject_t key = object_map_getkeyat(obj, i);
                    ApeObject_t val = object_map_getvalueat(obj, i);
                    object_tostring(key, buf, true);
                    strbuf_append(buf, ": ");
                    object_tostring(val, buf, true);
                    if(i < (object_map_getlength(obj) - 1))
                    {
                        strbuf_append(buf, ", ");
                    }
                }
                strbuf_append(buf, "}");
            }
            break;
        case APE_OBJECT_NATIVE_FUNCTION:
            {
                strbuf_append(buf, "NATIVE_FUNCTION");
            }
            break;
        case APE_OBJECT_EXTERNAL:
            {
                strbuf_append(buf, "EXTERNAL");
            }
            break;

        case APE_OBJECT_ERROR:
            {
                strbuf_appendf(buf, "ERROR: %s\n", object_value_geterrormessage(obj));
                ApeTraceback_t* traceback = object_value_geterrortraceback(obj);
                APE_ASSERT(traceback);
                if(traceback)
                {
                    strbuf_append(buf, "Traceback:\n");
                    traceback_tostring(traceback, buf);
                }
            }
            break;
        case APE_OBJECT_ANY:
            {
                APE_ASSERT(false);
            }
            break;
    }
}

bool traceback_tostring(const ApeTraceback_t* traceback, ApeStringBuffer_t* buf)
{
    ApeSize_t i;
    int depth;
    depth = array_count(traceback->items);
    for(i = 0; i < depth; i++)
    {
        ApeTracebackItem_t* item = (ApeTracebackItem_t*)array_get(traceback->items, i);
        const char* filename = traceback_item_get_filepath(item);
        if(item->pos.line >= 0 && item->pos.column >= 0)
        {
            strbuf_appendf(buf, "%s in %s on %d:%d\n", item->function_name, filename, item->pos.line, item->pos.column);
        }
        else
        {
            strbuf_appendf(buf, "%s\n", item->function_name);
        }
    }
    return !strbuf_failed(buf);
}

const char* ape_errortype_tostring(ApeErrorType_t type)
{
    switch(type)
    {
        case APE_ERROR_PARSING:
            return "PARSING";
        case APE_ERROR_COMPILATION:
            return "COMPILATION";
        case APE_ERROR_RUNTIME:
            return "RUNTIME";
        case APE_ERROR_TIMEOUT:
            return "TIMEOUT";
        case APE_ERROR_ALLOCATION:
            return "ALLOCATION";
        case APE_ERROR_USER:
            return "USER";
        default:
            return "NONE";
    }
}

const char* tokentype_tostring(ApeTokenType_t type)
{
    return g_type_names[type];
}
