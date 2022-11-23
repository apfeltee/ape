
#include "ape.h"

static bool lexpriv_readchar(ApeLexer_t *lex);
static char lexpriv_peekchar(ApeLexer_t *lex);
static bool lexpriv_isletter(char ch);
static bool lexpriv_isdigit(char ch);
static bool lexpriv_isoneof(char ch, const char *allowed, int allowed_len);
static const char *lexpriv_readident(ApeLexer_t *lex, int *out_len);
static const char *lexpriv_readnumber(ApeLexer_t *lex, int *out_len);
static const char *lexpriv_readstring(ApeLexer_t *lex, char delimiter, bool is_template, bool *out_template_found, int *out_len);
static ApeTokenType_t lexpriv_lookupident(const char *ident, int len);
static void lexpriv_skipspace(ApeLexer_t *lex);
static bool lexpriv_addline(ApeLexer_t *lex, int offset);


static ApePosition_t lexpriv_makesourcepos(const ApeCompiledFile_t* file, int line, int column)
{
    return (ApePosition_t){
        .file = file,
        .line = line,
        .column = column,
    };
}


void token_init(ApeToken_t* tok, ApeTokenType_t type, const char* literal, int len)
{
    tok->type = type;
    tok->literal = literal;
    tok->len = len;
}

char* token_duplicate_literal(ApeAllocator_t* alloc, const ApeToken_t* tok)
{
    return util_strndup(alloc, tok->literal, tok->len);
}

bool lexer_init(ApeLexer_t* lex, ApeAllocator_t* alloc, ApeErrorList_t* errs, const char* input, ApeCompiledFile_t* file)
{
    lex->alloc = alloc;
    lex->errors = errs;
    lex->input = input;
    lex->input_len = (int)strlen(input);
    lex->position = 0;
    lex->next_position = 0;
    lex->ch = '\0';
    if(file)
    {
        lex->line = ptrarray_count(file->lines);
    }
    else
    {
        lex->line = 0;
    }
    lex->column = -1;
    lex->file = file;
    bool ok = lexpriv_addline(lex, 0);
    if(!ok)
    {
        return false;
    }
    ok = lexpriv_readchar(lex);
    if(!ok)
    {
        return false;
    }
    lex->failed = false;
    lex->continue_template_string = false;

    memset(&lex->prev_token_state, 0, sizeof(lex->prev_token_state));
    token_init(&lex->prev_token, TOKEN_INVALID, NULL, 0);
    token_init(&lex->cur_token, TOKEN_INVALID, NULL, 0);
    token_init(&lex->peek_token, TOKEN_INVALID, NULL, 0);

    return true;
}

bool lexer_failed(ApeLexer_t* lex)
{
    return lex->failed;
}

void lexer_continue_template_string(ApeLexer_t* lex)
{
    lex->continue_template_string = true;
}

bool lexer_cur_token_is(ApeLexer_t* lex, ApeTokenType_t type)
{
    return lex->cur_token.type == type;
}

bool lexer_peek_token_is(ApeLexer_t* lex, ApeTokenType_t type)
{
    return lex->peek_token.type == type;
}

bool lexer_next_token(ApeLexer_t* lex)
{
    lex->prev_token = lex->cur_token;
    lex->cur_token = lex->peek_token;
    lex->peek_token = lexer_next_token_internal(lex);
    return !lex->failed;
}

bool lexer_previous_token(ApeLexer_t* lex)
{
    if(lex->prev_token.type == TOKEN_INVALID)
    {
        return false;
    }

    lex->peek_token = lex->cur_token;
    lex->cur_token = lex->prev_token;
    token_init(&lex->prev_token, TOKEN_INVALID, NULL, 0);

    lex->ch = lex->prev_token_state.ch;
    lex->column = lex->prev_token_state.column;
    lex->line = lex->prev_token_state.line;
    lex->position = lex->prev_token_state.position;
    lex->next_position = lex->prev_token_state.next_position;

    return true;
}

ApeToken_t lexer_next_token_internal(ApeLexer_t* lex)
{
    lex->prev_token_state.ch = lex->ch;
    lex->prev_token_state.column = lex->column;
    lex->prev_token_state.line = lex->line;
    lex->prev_token_state.position = lex->position;
    lex->prev_token_state.next_position = lex->next_position;

    while(true)
    {
        if(!lex->continue_template_string)
        {
            lexpriv_skipspace(lex);
        }

        ApeToken_t out_tok;
        out_tok.type = TOKEN_INVALID;
        out_tok.literal = lex->input + lex->position;
        out_tok.len = 1;
        out_tok.pos = lexpriv_makesourcepos(lex->file, lex->line, lex->column);

        char c = lex->continue_template_string ? '`' : lex->ch;

        switch(c)
        {
            case '\0':
                token_init(&out_tok, TOKEN_EOF, "EOF", 3);
                break;
            case '=':
            {
                if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_EQ, "==", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_ASSIGN, "=", 1);
                }
                break;
            }
            case '&':
            {
                if(lexpriv_peekchar(lex) == '&')
                {
                    token_init(&out_tok, TOKEN_AND, "&&", 2);
                    lexpriv_readchar(lex);
                }
                else if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_BIT_AND_ASSIGN, "&=", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_BIT_AND, "&", 1);
                }
                break;
            }
            case '|':
            {
                if(lexpriv_peekchar(lex) == '|')
                {
                    token_init(&out_tok, TOKEN_OR, "||", 2);
                    lexpriv_readchar(lex);
                }
                else if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_BIT_OR_ASSIGN, "|=", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_BIT_OR, "|", 1);
                }
                break;
            }
            case '^':
            {
                if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_BIT_XOR_ASSIGN, "^=", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_BIT_XOR, "^", 1);
                    break;
                }
                break;
            }
            case '+':
            {
                if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_PLUS_ASSIGN, "+=", 2);
                    lexpriv_readchar(lex);
                }
                else if(lexpriv_peekchar(lex) == '+')
                {
                    token_init(&out_tok, TOKEN_PLUS_PLUS, "++", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_PLUS, "+", 1);
                    break;
                }
                break;
            }
            case '-':
            {
                if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_MINUS_ASSIGN, "-=", 2);
                    lexpriv_readchar(lex);
                }
                else if(lexpriv_peekchar(lex) == '-')
                {
                    token_init(&out_tok, TOKEN_MINUS_MINUS, "--", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_MINUS, "-", 1);
                    break;
                }
                break;
            }
            case '!':
            {
                if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_NOT_EQ, "!=", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_BANG, "!", 1);
                }
                break;
            }
            case '*':
            {
                if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_ASTERISK_ASSIGN, "*=", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_ASTERISK, "*", 1);
                    break;
                }
                break;
            }
            case '/':
            {
                if(lexpriv_peekchar(lex) == '/')
                {
                    lexpriv_readchar(lex);
                    while(lex->ch != '\n' && lex->ch != '\0')
                    {
                        lexpriv_readchar(lex);
                    }
                    continue;
                }
                else if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_SLASH_ASSIGN, "/=", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_SLASH, "/", 1);
                    break;
                }
                break;
            }
            case '<':
            {
                if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_LTE, "<=", 2);
                    lexpriv_readchar(lex);
                }
                else if(lexpriv_peekchar(lex) == '<')
                {
                    lexpriv_readchar(lex);
                    if(lexpriv_peekchar(lex) == '=')
                    {
                        token_init(&out_tok, TOKEN_LSHIFT_ASSIGN, "<<=", 3);
                        lexpriv_readchar(lex);
                    }
                    else
                    {
                        token_init(&out_tok, TOKEN_LSHIFT, "<<", 2);
                    }
                }
                else
                {
                    token_init(&out_tok, TOKEN_LT, "<", 1);
                    break;
                }
                break;
            }
            case '>':
            {
                if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_GTE, ">=", 2);
                    lexpriv_readchar(lex);
                }
                else if(lexpriv_peekchar(lex) == '>')
                {
                    lexpriv_readchar(lex);
                    if(lexpriv_peekchar(lex) == '=')
                    {
                        token_init(&out_tok, TOKEN_RSHIFT_ASSIGN, ">>=", 3);
                        lexpriv_readchar(lex);
                    }
                    else
                    {
                        token_init(&out_tok, TOKEN_RSHIFT, ">>", 2);
                    }
                }
                else
                {
                    token_init(&out_tok, TOKEN_GT, ">", 1);
                }
                break;
            }
            case ',':
                token_init(&out_tok, TOKEN_COMMA, ",", 1);
                break;
            case ';':
                token_init(&out_tok, TOKEN_SEMICOLON, ";", 1);
                break;
            case ':':
                token_init(&out_tok, TOKEN_COLON, ":", 1);
                break;
            case '(':
                token_init(&out_tok, TOKEN_LPAREN, "(", 1);
                break;
            case ')':
                token_init(&out_tok, TOKEN_RPAREN, ")", 1);
                break;
            case '{':
                token_init(&out_tok, TOKEN_LBRACE, "{", 1);
                break;
            case '}':
                token_init(&out_tok, TOKEN_RBRACE, "}", 1);
                break;
            case '[':
                token_init(&out_tok, TOKEN_LBRACKET, "[", 1);
                break;
            case ']':
                token_init(&out_tok, TOKEN_RBRACKET, "]", 1);
                break;
            case '.':
                token_init(&out_tok, TOKEN_DOT, ".", 1);
                break;
            case '?':
                token_init(&out_tok, TOKEN_QUESTION, "?", 1);
                break;
            case '%':
            {
                if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_PERCENT_ASSIGN, "%=", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_PERCENT, "%", 1);
                    break;
                }
                break;
            }
            case '"':
            {
                lexpriv_readchar(lex);
                int len;
                const char* str = lexpriv_readstring(lex, '"', false, NULL, &len);
                if(str)
                {
                    token_init(&out_tok, TOKEN_STRING, str, len);
                }
                else
                {
                    token_init(&out_tok, TOKEN_INVALID, NULL, 0);
                }
                break;
            }
            case '\'':
            {
                lexpriv_readchar(lex);
                int len;
                const char* str = lexpriv_readstring(lex, '\'', false, NULL, &len);
                if(str)
                {
                    token_init(&out_tok, TOKEN_STRING, str, len);
                }
                else
                {
                    token_init(&out_tok, TOKEN_INVALID, NULL, 0);
                }
                break;
            }
            case '`':
            {
                if(!lex->continue_template_string)
                {
                    lexpriv_readchar(lex);
                }
                int len;
                bool template_found = false;
                const char* str = lexpriv_readstring(lex, '`', true, &template_found, &len);
                if(str)
                {
                    if(template_found)
                    {
                        token_init(&out_tok, TOKEN_TEMPLATE_STRING, str, len);
                    }
                    else
                    {
                        token_init(&out_tok, TOKEN_STRING, str, len);
                    }
                }
                else
                {
                    token_init(&out_tok, TOKEN_INVALID, NULL, 0);
                }
                break;
            }
            default:
            {
                if(lexpriv_isletter(lex->ch))
                {
                    int ident_len = 0;
                    const char* ident = lexpriv_readident(lex, &ident_len);
                    ApeTokenType_t type = lexpriv_lookupident(ident, ident_len);
                    token_init(&out_tok, type, ident, ident_len);
                    return out_tok;
                }
                else if(lexpriv_isdigit(lex->ch))
                {
                    int number_len = 0;
                    const char* number = lexpriv_readnumber(lex, &number_len);
                    token_init(&out_tok, TOKEN_NUMBER, number, number_len);
                    return out_tok;
                }
                break;
            }
        }
        lexpriv_readchar(lex);
        if(lexer_failed(lex))
        {
            token_init(&out_tok, TOKEN_INVALID, NULL, 0);
        }
        lex->continue_template_string = false;
        return out_tok;
    }
}

bool lexer_expect_current(ApeLexer_t* lex, ApeTokenType_t type)
{
    if(lexer_failed(lex))
    {
        return false;
    }

    if(!lexer_cur_token_is(lex, type))
    {
        const char* expected_type_str = tokentype_tostring(type);
        const char* actual_type_str = tokentype_tostring(lex->cur_token.type);
        errorlist_addformat(lex->errors, APE_ERROR_PARSING, lex->cur_token.pos,
                          "Expected current token to be \"%s\", got \"%s\" instead", expected_type_str, actual_type_str);
        return false;
    }
    return true;
}

// INTERNAL

static bool lexpriv_readchar(ApeLexer_t* lex)
{
    if(lex->next_position >= lex->input_len)
    {
        lex->ch = '\0';
    }
    else
    {
        lex->ch = lex->input[lex->next_position];
    }
    lex->position = lex->next_position;
    lex->next_position++;

    if(lex->ch == '\n')
    {
        lex->line++;
        lex->column = -1;
        bool ok = lexpriv_addline(lex, lex->next_position);
        if(!ok)
        {
            lex->failed = true;
            return false;
        }
    }
    else
    {
        lex->column++;
    }
    return true;
}

static char lexpriv_peekchar(ApeLexer_t* lex)
{
    if(lex->next_position >= lex->input_len)
    {
        return '\0';
    }
    else
    {
        return lex->input[lex->next_position];
    }
}

static bool lexpriv_isletter(char ch)
{
    return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z') || ch == '_';
}

static bool lexpriv_isdigit(char ch)
{
    return ch >= '0' && ch <= '9';
}

static bool lexpriv_isoneof(char ch, const char* allowed, int allowed_len)
{
    for(int i = 0; i < allowed_len; i++)
    {
        if(ch == allowed[i])
        {
            return true;
        }
    }
    return false;
}

static const char* lexpriv_readident(ApeLexer_t* lex, int* out_len)
{
    int position = lex->position;
    int len = 0;
    while(lexpriv_isdigit(lex->ch) || lexpriv_isletter(lex->ch) || lex->ch == ':')
    {
        if(lex->ch == ':')
        {
            if(lexpriv_peekchar(lex) != ':')
            {
                goto end;
            }
            lexpriv_readchar(lex);
        }
        lexpriv_readchar(lex);
    }
end:
    len = lex->position - position;
    *out_len = len;
    return lex->input + position;
}

static const char* lexpriv_readnumber(ApeLexer_t* lex, int* out_len)
{
    char allowed[] = ".xXaAbBcCdDeEfF";
    int position = lex->position;
    while(lexpriv_isdigit(lex->ch) || lexpriv_isoneof(lex->ch, allowed, APE_ARRAY_LEN(allowed) - 1))
    {
        lexpriv_readchar(lex);
    }
    int len = lex->position - position;
    *out_len = len;
    return lex->input + position;
}

static const char* lexpriv_readstring(ApeLexer_t* lex, char delimiter, bool is_template, bool* out_template_found, int* out_len)
{
    *out_len = 0;

    bool escaped = false;
    int position = lex->position;

    while(true)
    {
        if(lex->ch == '\0')
        {
            return NULL;
        }
        if(lex->ch == delimiter && !escaped)
        {
            break;
        }
        if(is_template && !escaped && lex->ch == '$' && lexpriv_peekchar(lex) == '{')
        {
            *out_template_found = true;
            break;
        }
        escaped = false;
        if(lex->ch == '\\')
        {
            escaped = true;
        }
        lexpriv_readchar(lex);
    }
    int len = lex->position - position;
    *out_len = len;
    return lex->input + position;
}

static ApeTokenType_t lexpriv_lookupident(const char* ident, int len)
{
    static struct
    {
        const char* value;
        int len;
        ApeTokenType_t type;
    } keywords[] = {
        { "function", 8, TOKEN_FUNCTION },
        { "const", 5, TOKEN_CONST },
        { "var", 3, TOKEN_VAR },
        { "true", 4, TOKEN_TRUE },
        { "false", 5, TOKEN_FALSE },
        { "if", 2, TOKEN_IF },
        { "else", 4, TOKEN_ELSE },
        { "return", 6, TOKEN_RETURN },
        { "while", 5, TOKEN_WHILE },
        { "break", 5, TOKEN_BREAK },       { "for", 3, TOKEN_FOR },       { "in", 2, TOKEN_IN },
        { "continue", 8, TOKEN_CONTINUE }, { "null", 4, TOKEN_NULL },     { "import", 6, TOKEN_IMPORT },
        { "recover", 7, TOKEN_RECOVER },
    };

    for(int i = 0; i < APE_ARRAY_LEN(keywords); i++)
    {
        if(keywords[i].len == len && APE_STRNEQ(ident, keywords[i].value, len))
        {
            return keywords[i].type;
        }
    }

    return TOKEN_IDENT;
}

static void lexpriv_skipspace(ApeLexer_t* lex)
{
    char ch = lex->ch;
    while(ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
    {
        lexpriv_readchar(lex);
        ch = lex->ch;
    }
}

static bool lexpriv_addline(ApeLexer_t* lex, int offset)
{
    if(!lex->file)
    {
        return true;
    }

    if(lex->line < ptrarray_count(lex->file->lines))
    {
        return true;
    }

    const char* line_start = lex->input + offset;
    const char* new_line_ptr = strchr(line_start, '\n');
    char* line = NULL;
    if(!new_line_ptr)
    {
        line = util_strdup(lex->alloc, line_start);
    }
    else
    {
        size_t line_len = new_line_ptr - line_start;
        line = util_strndup(lex->alloc, line_start, line_len);
    }
    if(!line)
    {
        lex->failed = true;
        return false;
    }
    bool ok = ptrarray_add(lex->file->lines, line);
    if(!ok)
    {
        lex->failed = true;
        allocator_free(lex->alloc, line);
        return false;
    }
    return true;
}

