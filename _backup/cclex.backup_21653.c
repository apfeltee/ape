
#include "ape.h"

static bool lexpriv_readchar(ApeLexer_t *lex);
static char lexpriv_peekchar(ApeLexer_t *lex);
static bool lexpriv_isletter(char ch);
static bool lexpriv_isdigit(char ch);
static bool lexpriv_isoneof(char ch, const char *allowed, int allowedlen);
static const char *lexpriv_readident(ApeLexer_t *lex, int *outlen);
static const char *lexpriv_readnumber(ApeLexer_t *lex, int *outlen);
static const char *lexpriv_readstring(ApeLexer_t *lex, char delimiter, bool istemplate, bool *outtemplatefound, int *outlen);
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

char* token_duplicate_literal(ApeContext_t* ctx, const ApeToken_t* tok)
{
    return util_strndup(ctx, tok->literal, tok->len);
}

bool lexer_init(ApeLexer_t* lex, ApeContext_t* ctx, ApeErrorList_t* errs, const char* input, ApeCompiledFile_t* file)
{
    lex->context = ctx;
    lex->alloc = &ctx->alloc;
    lex->errors = errs;
    lex->input = input;
    lex->inputlen = (int)strlen(input);
    lex->position = 0;
    lex->nextposition = 0;
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

    memset(&lex->prevtokenstate, 0, sizeof(lex->prevtokenstate));
    token_init(&lex->prevtoken, TOKEN_INVALID, NULL, 0);
    token_init(&lex->curtoken, TOKEN_INVALID, NULL, 0);
    token_init(&lex->peektoken, TOKEN_INVALID, NULL, 0);

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
    return lex->curtoken.type == type;
}

bool lexer_peek_token_is(ApeLexer_t* lex, ApeTokenType_t type)
{
    return lex->peektoken.type == type;
}

bool lexer_next_token(ApeLexer_t* lex)
{
    lex->prevtoken = lex->curtoken;
    lex->curtoken = lex->peektoken;
    lex->peektoken = lexer_next_token_internal(lex);
    return !lex->failed;
}

bool lexer_previous_token(ApeLexer_t* lex)
{
    if(lex->prevtoken.type == TOKEN_INVALID)
    {
        return false;
    }

    lex->peektoken = lex->curtoken;
    lex->curtoken = lex->prevtoken;
    token_init(&lex->prevtoken, TOKEN_INVALID, NULL, 0);

    lex->ch = lex->prevtokenstate.ch;
    lex->column = lex->prevtokenstate.column;
    lex->line = lex->prevtokenstate.line;
    lex->position = lex->prevtokenstate.position;
    lex->nextposition = lex->prevtokenstate.nextposition;

    return true;
}

ApeToken_t lexer_next_token_internal(ApeLexer_t* lex)
{
    lex->prevtokenstate.ch = lex->ch;
    lex->prevtokenstate.column = lex->column;
    lex->prevtokenstate.line = lex->line;
    lex->prevtokenstate.position = lex->position;
    lex->prevtokenstate.nextposition = lex->nextposition;

    while(true)
    {
        if(!lex->continue_template_string)
        {
            lexpriv_skipspace(lex);
        }

        ApeToken_t outtok;
        outtok.type = TOKEN_INVALID;
        outtok.literal = lex->input + lex->position;
        outtok.len = 1;
        outtok.pos = lexpriv_makesourcepos(lex->file, lex->line, lex->column);

        char c = lex->continue_template_string ? '`' : lex->ch;

        switch(c)
        {
            case '\0':
                token_init(&outtok, TOKEN_EOF, "EOF", 3);
                break;
            case '=':
            {
                if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&outtok, TOKEN_EQ, "==", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&outtok, TOKEN_ASSIGN, "=", 1);
                }
                break;
            }
            case '&':
            {
                if(lexpriv_peekchar(lex) == '&')
                {
                    token_init(&outtok, TOKEN_AND, "&&", 2);
                    lexpriv_readchar(lex);
                }
                else if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&outtok, TOKEN_BIT_AND_ASSIGN, "&=", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&outtok, TOKEN_BIT_AND, "&", 1);
                }
                break;
            }
            case '|':
            {
                if(lexpriv_peekchar(lex) == '|')
                {
                    token_init(&outtok, TOKEN_OR, "||", 2);
                    lexpriv_readchar(lex);
                }
                else if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&outtok, TOKEN_BIT_OR_ASSIGN, "|=", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&outtok, TOKEN_BIT_OR, "|", 1);
                }
                break;
            }
            case '^':
            {
                if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&outtok, TOKEN_BIT_XOR_ASSIGN, "^=", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&outtok, TOKEN_BIT_XOR, "^", 1);
                    break;
                }
                break;
            }
            case '+':
            {
                if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&outtok, TOKEN_PLUS_ASSIGN, "+=", 2);
                    lexpriv_readchar(lex);
                }
                else if(lexpriv_peekchar(lex) == '+')
                {
                    token_init(&outtok, TOKEN_PLUS_PLUS, "++", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&outtok, TOKEN_PLUS, "+", 1);
                    break;
                }
                break;
            }
            case '-':
            {
                if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&outtok, TOKEN_MINUS_ASSIGN, "-=", 2);
                    lexpriv_readchar(lex);
                }
                else if(lexpriv_peekchar(lex) == '-')
                {
                    token_init(&outtok, TOKEN_MINUS_MINUS, "--", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&outtok, TOKEN_MINUS, "-", 1);
                    break;
                }
                break;
            }
            case '!':
            {
                if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&outtok, TOKEN_NOT_EQ, "!=", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&outtok, TOKEN_BANG, "!", 1);
                }
                break;
            }
            case '*':
            {
                if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&outtok, TOKEN_ASTERISK_ASSIGN, "*=", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&outtok, TOKEN_ASTERISK, "*", 1);
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
                    token_init(&outtok, TOKEN_SLASH_ASSIGN, "/=", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&outtok, TOKEN_SLASH, "/", 1);
                    break;
                }
                break;
            }
            case '<':
            {
                if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&outtok, TOKEN_LTE, "<=", 2);
                    lexpriv_readchar(lex);
                }
                else if(lexpriv_peekchar(lex) == '<')
                {
                    lexpriv_readchar(lex);
                    if(lexpriv_peekchar(lex) == '=')
                    {
                        token_init(&outtok, TOKEN_LSHIFT_ASSIGN, "<<=", 3);
                        lexpriv_readchar(lex);
                    }
                    else
                    {
                        token_init(&outtok, TOKEN_LSHIFT, "<<", 2);
                    }
                }
                else
                {
                    token_init(&outtok, TOKEN_LT, "<", 1);
                    break;
                }
                break;
            }
            case '>':
            {
                if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&outtok, TOKEN_GTE, ">=", 2);
                    lexpriv_readchar(lex);
                }
                else if(lexpriv_peekchar(lex) == '>')
                {
                    lexpriv_readchar(lex);
                    if(lexpriv_peekchar(lex) == '=')
                    {
                        token_init(&outtok, TOKEN_RSHIFT_ASSIGN, ">>=", 3);
                        lexpriv_readchar(lex);
                    }
                    else
                    {
                        token_init(&outtok, TOKEN_RSHIFT, ">>", 2);
                    }
                }
                else
                {
                    token_init(&outtok, TOKEN_GT, ">", 1);
                }
                break;
            }
            case ',':
                token_init(&outtok, TOKEN_COMMA, ",", 1);
                break;
            case ';':
                token_init(&outtok, TOKEN_SEMICOLON, ";", 1);
                break;
            case ':':
                token_init(&outtok, TOKEN_COLON, ":", 1);
                break;
            case '(':
                token_init(&outtok, TOKEN_LPAREN, "(", 1);
                break;
            case ')':
                token_init(&outtok, TOKEN_RPAREN, ")", 1);
                break;
            case '{':
                token_init(&outtok, TOKEN_LBRACE, "{", 1);
                break;
            case '}':
                token_init(&outtok, TOKEN_RBRACE, "}", 1);
                break;
            case '[':
                token_init(&outtok, TOKEN_LBRACKET, "[", 1);
                break;
            case ']':
                token_init(&outtok, TOKEN_RBRACKET, "]", 1);
                break;
            case '.':
                token_init(&outtok, TOKEN_DOT, ".", 1);
                break;
            case '?':
                token_init(&outtok, TOKEN_QUESTION, "?", 1);
                break;
            case '%':
            {
                if(lexpriv_peekchar(lex) == '=')
                {
                    token_init(&outtok, TOKEN_PERCENT_ASSIGN, "%=", 2);
                    lexpriv_readchar(lex);
                }
                else
                {
                    token_init(&outtok, TOKEN_PERCENT, "%", 1);
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
                    token_init(&outtok, TOKEN_STRING, str, len);
                }
                else
                {
                    token_init(&outtok, TOKEN_INVALID, NULL, 0);
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
                    token_init(&outtok, TOKEN_STRING, str, len);
                }
                else
                {
                    token_init(&outtok, TOKEN_INVALID, NULL, 0);
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
                bool templatefound = false;
                const char* str = lexpriv_readstring(lex, '`', true, &templatefound, &len);
                if(str)
                {
                    if(templatefound)
                    {
                        token_init(&outtok, TOKEN_TEMPLATE_STRING, str, len);
                    }
                    else
                    {
                        token_init(&outtok, TOKEN_STRING, str, len);
                    }
                }
                else
                {
                    token_init(&outtok, TOKEN_INVALID, NULL, 0);
                }
                break;
            }
            default:
            {
                if(lexpriv_isletter(lex->ch))
                {
                    int identlen = 0;
                    const char* ident = lexpriv_readident(lex, &identlen);
                    ApeTokenType_t type = lexpriv_lookupident(ident, identlen);
                    token_init(&outtok, type, ident, identlen);
                    return outtok;
                }
                else if(lexpriv_isdigit(lex->ch))
                {
                    int numberlen = 0;
                    const char* number = lexpriv_readnumber(lex, &numberlen);
                    token_init(&outtok, TOKEN_NUMBER, number, numberlen);
                    return outtok;
                }
                break;
            }
        }
        lexpriv_readchar(lex);
        if(lexer_failed(lex))
        {
            token_init(&outtok, TOKEN_INVALID, NULL, 0);
        }
        lex->continue_template_string = false;
        return outtok;
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
        const char* expectedtypestr = tokentype_tostring(type);
        const char* actualtypestr = tokentype_tostring(lex->curtoken.type);
        errorlist_addformat(lex->errors, APE_ERROR_PARSING, lex->curtoken.pos,
                          "expected current token to be '%s', got '%s' instead", expectedtypestr, actualtypestr);
        return false;
    }
    return true;
}

// INTERNAL

static bool lexpriv_readchar(ApeLexer_t* lex)
{
    if(lex->nextposition >= lex->inputlen)
    {
        lex->ch = '\0';
    }
    else
    {
        lex->ch = lex->input[lex->nextposition];
    }
    lex->position = lex->nextposition;
    lex->nextposition++;

    if(lex->ch == '\n')
    {
        lex->line++;
        lex->column = -1;
        bool ok = lexpriv_addline(lex, lex->nextposition);
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
    if(lex->nextposition >= lex->inputlen)
    {
        return '\0';
    }
    else
    {
        return lex->input[lex->nextposition];
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

static bool lexpriv_isoneof(char ch, const char* allowed, int allowedlen)
{
    for(int i = 0; i < allowedlen; i++)
    {
        if(ch == allowed[i])
        {
            return true;
        }
    }
    return false;
}

static const char* lexpriv_readident(ApeLexer_t* lex, int* outlen)
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
    *outlen = len;
    return lex->input + position;
}

static const char* lexpriv_readnumber(ApeLexer_t* lex, int* outlen)
{
    char allowed[] = ".xXaAbBcCdDeEfF";
    int position = lex->position;
    while(lexpriv_isdigit(lex->ch) || lexpriv_isoneof(lex->ch, allowed, APE_ARRAY_LEN(allowed) - 1))
    {
        lexpriv_readchar(lex);
    }
    int len = lex->position - position;
    *outlen = len;
    return lex->input + position;
}

static const char* lexpriv_readstring(ApeLexer_t* lex, char delimiter, bool istemplate, bool* outtemplatefound, int* outlen)
{
    *outlen = 0;

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
        if(istemplate && !escaped && lex->ch == '$' && lexpriv_peekchar(lex) == '{')
        {
            *outtemplatefound = true;
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
    *outlen = len;
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
        { "break", 5, TOKEN_BREAK },
        { "for", 3, TOKEN_FOR },
        { "in", 2, TOKEN_IN },
        { "continue", 8, TOKEN_CONTINUE },
        { "null", 4, TOKEN_NULL },
        { "import", 6, TOKEN_IMPORT },
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
    if(lex->line < (ApeInt_t)ptrarray_count(lex->file->lines))
    {
        return true;
    }
    const char* linestart = lex->input + offset;
    const char* newlineptr = strchr(linestart, '\n');
    char* line = NULL;
    if(!newlineptr)
    {
        line = util_strdup(lex->context, linestart);
    }
    else
    {
        size_t linelen = newlineptr - linestart;
        line = util_strndup(lex->context, linestart, linelen);
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

