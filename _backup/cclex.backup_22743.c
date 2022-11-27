
#include "ape.h"

static bool ape_lexer_readchar(ApeLexer_t *lex);
static char ape_lexer_peekchar(ApeLexer_t *lex);
static bool ape_lexer_isletter(char ch);
static bool ape_lexer_isdigit(char ch);
static bool ape_lexer_isoneof(char ch, const char *allowed, int allowedlen);
static const char *ape_lexer_readident(ApeLexer_t *lex, int *outlen);
static const char *ape_lexer_readnumber(ApeLexer_t *lex, int *outlen);
static const char *ape_lexer_readstring(ApeLexer_t *lex, char delimiter, bool istemplate, bool *outtemplatefound, int *outlen);
static ApeTokenType_t ape_lexer_lookupident(const char *ident, int len);
static void ape_lexer_skipspace(ApeLexer_t *lex);
static bool ape_lexer_addline(ApeLexer_t *lex, int offset);


static ApePosition_t ape_lexer_makesourcepos(const ApeCompiledFile_t* file, int line, int column)
{
    return (ApePosition_t){
        .file = file,
        .line = line,
        .column = column,
    };
}


void ape_lexer_token_init(ApeToken_t* tok, ApeTokenType_t type, const char* literal, int len)
{
    tok->type = type;
    tok->literal = literal;
    tok->len = len;
}

char* ape_lexer_tokendupliteral(ApeContext_t* ctx, const ApeToken_t* tok)
{
    return ape_util_strndup(ctx, tok->literal, tok->len);
}

bool ape_lexer_init(ApeLexer_t* lex, ApeContext_t* ctx, ApeErrorList_t* errs, const char* input, ApeCompiledFile_t* file)
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
        lex->line = ape_ptrarray_count(file->lines);
    }
    else
    {
        lex->line = 0;
    }
    lex->column = -1;
    lex->file = file;
    bool ok = ape_lexer_addline(lex, 0);
    if(!ok)
    {
        return false;
    }
    ok = ape_lexer_readchar(lex);
    if(!ok)
    {
        return false;
    }
    lex->failed = false;
    lex->continue_template_string = false;

    memset(&lex->prevtokenstate, 0, sizeof(lex->prevtokenstate));
    ape_lexer_token_init(&lex->prevtoken, TOKEN_INVALID, NULL, 0);
    ape_lexer_token_init(&lex->curtoken, TOKEN_INVALID, NULL, 0);
    ape_lexer_token_init(&lex->peektoken, TOKEN_INVALID, NULL, 0);

    return true;
}

bool ape_lexer_failed(ApeLexer_t* lex)
{
    return lex->failed;
}

void ape_lexer_continuetemplatestring(ApeLexer_t* lex)
{
    lex->continue_template_string = true;
}

bool ape_lexer_currenttokenis(ApeLexer_t* lex, ApeTokenType_t type)
{
    return lex->curtoken.type == type;
}

bool ape_lexer_peektokenis(ApeLexer_t* lex, ApeTokenType_t type)
{
    return lex->peektoken.type == type;
}

bool ape_lexer_nexttoken(ApeLexer_t* lex)
{
    lex->prevtoken = lex->curtoken;
    lex->curtoken = lex->peektoken;
    lex->peektoken = ape_lexer_internalnexttoken(lex);
    return !lex->failed;
}

bool ape_lexer_previous_token(ApeLexer_t* lex)
{
    if(lex->prevtoken.type == TOKEN_INVALID)
    {
        return false;
    }

    lex->peektoken = lex->curtoken;
    lex->curtoken = lex->prevtoken;
    ape_lexer_token_init(&lex->prevtoken, TOKEN_INVALID, NULL, 0);

    lex->ch = lex->prevtokenstate.ch;
    lex->column = lex->prevtokenstate.column;
    lex->line = lex->prevtokenstate.line;
    lex->position = lex->prevtokenstate.position;
    lex->nextposition = lex->prevtokenstate.nextposition;

    return true;
}

ApeToken_t ape_lexer_internalnexttoken(ApeLexer_t* lex)
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
            ape_lexer_skipspace(lex);
        }

        ApeToken_t outtok;
        outtok.type = TOKEN_INVALID;
        outtok.literal = lex->input + lex->position;
        outtok.len = 1;
        outtok.pos = ape_lexer_makesourcepos(lex->file, lex->line, lex->column);

        char c = lex->continue_template_string ? '`' : lex->ch;

        switch(c)
        {
            case '\0':
                ape_lexer_token_init(&outtok, TOKEN_EOF, "EOF", 3);
                break;
            case '=':
            {
                if(ape_lexer_peekchar(lex) == '=')
                {
                    ape_lexer_token_init(&outtok, TOKEN_EQ, "==", 2);
                    ape_lexer_readchar(lex);
                }
                else
                {
                    ape_lexer_token_init(&outtok, TOKEN_ASSIGN, "=", 1);
                }
                break;
            }
            case '&':
            {
                if(ape_lexer_peekchar(lex) == '&')
                {
                    ape_lexer_token_init(&outtok, TOKEN_AND, "&&", 2);
                    ape_lexer_readchar(lex);
                }
                else if(ape_lexer_peekchar(lex) == '=')
                {
                    ape_lexer_token_init(&outtok, TOKEN_BIT_AND_ASSIGN, "&=", 2);
                    ape_lexer_readchar(lex);
                }
                else
                {
                    ape_lexer_token_init(&outtok, TOKEN_BIT_AND, "&", 1);
                }
                break;
            }
            case '|':
            {
                if(ape_lexer_peekchar(lex) == '|')
                {
                    ape_lexer_token_init(&outtok, TOKEN_OR, "||", 2);
                    ape_lexer_readchar(lex);
                }
                else if(ape_lexer_peekchar(lex) == '=')
                {
                    ape_lexer_token_init(&outtok, TOKEN_BIT_OR_ASSIGN, "|=", 2);
                    ape_lexer_readchar(lex);
                }
                else
                {
                    ape_lexer_token_init(&outtok, TOKEN_BIT_OR, "|", 1);
                }
                break;
            }
            case '^':
            {
                if(ape_lexer_peekchar(lex) == '=')
                {
                    ape_lexer_token_init(&outtok, TOKEN_BIT_XOR_ASSIGN, "^=", 2);
                    ape_lexer_readchar(lex);
                }
                else
                {
                    ape_lexer_token_init(&outtok, TOKEN_BIT_XOR, "^", 1);
                    break;
                }
                break;
            }
            case '+':
            {
                if(ape_lexer_peekchar(lex) == '=')
                {
                    ape_lexer_token_init(&outtok, TOKEN_PLUS_ASSIGN, "+=", 2);
                    ape_lexer_readchar(lex);
                }
                else if(ape_lexer_peekchar(lex) == '+')
                {
                    ape_lexer_token_init(&outtok, TOKEN_PLUS_PLUS, "++", 2);
                    ape_lexer_readchar(lex);
                }
                else
                {
                    ape_lexer_token_init(&outtok, TOKEN_PLUS, "+", 1);
                    break;
                }
                break;
            }
            case '-':
            {
                if(ape_lexer_peekchar(lex) == '=')
                {
                    ape_lexer_token_init(&outtok, TOKEN_MINUS_ASSIGN, "-=", 2);
                    ape_lexer_readchar(lex);
                }
                else if(ape_lexer_peekchar(lex) == '-')
                {
                    ape_lexer_token_init(&outtok, TOKEN_MINUS_MINUS, "--", 2);
                    ape_lexer_readchar(lex);
                }
                else
                {
                    ape_lexer_token_init(&outtok, TOKEN_MINUS, "-", 1);
                    break;
                }
                break;
            }
            case '!':
            {
                if(ape_lexer_peekchar(lex) == '=')
                {
                    ape_lexer_token_init(&outtok, TOKEN_NOT_EQ, "!=", 2);
                    ape_lexer_readchar(lex);
                }
                else
                {
                    ape_lexer_token_init(&outtok, TOKEN_BANG, "!", 1);
                }
                break;
            }
            case '*':
            {
                if(ape_lexer_peekchar(lex) == '=')
                {
                    ape_lexer_token_init(&outtok, TOKEN_ASTERISK_ASSIGN, "*=", 2);
                    ape_lexer_readchar(lex);
                }
                else
                {
                    ape_lexer_token_init(&outtok, TOKEN_ASTERISK, "*", 1);
                    break;
                }
                break;
            }
            case '/':
            {
                if(ape_lexer_peekchar(lex) == '/')
                {
                    ape_lexer_readchar(lex);
                    while(lex->ch != '\n' && lex->ch != '\0')
                    {
                        ape_lexer_readchar(lex);
                    }
                    continue;
                }
                else if(ape_lexer_peekchar(lex) == '=')
                {
                    ape_lexer_token_init(&outtok, TOKEN_SLASH_ASSIGN, "/=", 2);
                    ape_lexer_readchar(lex);
                }
                else
                {
                    ape_lexer_token_init(&outtok, TOKEN_SLASH, "/", 1);
                    break;
                }
                break;
            }
            case '<':
            {
                if(ape_lexer_peekchar(lex) == '=')
                {
                    ape_lexer_token_init(&outtok, TOKEN_LTE, "<=", 2);
                    ape_lexer_readchar(lex);
                }
                else if(ape_lexer_peekchar(lex) == '<')
                {
                    ape_lexer_readchar(lex);
                    if(ape_lexer_peekchar(lex) == '=')
                    {
                        ape_lexer_token_init(&outtok, TOKEN_LSHIFT_ASSIGN, "<<=", 3);
                        ape_lexer_readchar(lex);
                    }
                    else
                    {
                        ape_lexer_token_init(&outtok, TOKEN_LSHIFT, "<<", 2);
                    }
                }
                else
                {
                    ape_lexer_token_init(&outtok, TOKEN_LT, "<", 1);
                    break;
                }
                break;
            }
            case '>':
            {
                if(ape_lexer_peekchar(lex) == '=')
                {
                    ape_lexer_token_init(&outtok, TOKEN_GTE, ">=", 2);
                    ape_lexer_readchar(lex);
                }
                else if(ape_lexer_peekchar(lex) == '>')
                {
                    ape_lexer_readchar(lex);
                    if(ape_lexer_peekchar(lex) == '=')
                    {
                        ape_lexer_token_init(&outtok, TOKEN_RSHIFT_ASSIGN, ">>=", 3);
                        ape_lexer_readchar(lex);
                    }
                    else
                    {
                        ape_lexer_token_init(&outtok, TOKEN_RSHIFT, ">>", 2);
                    }
                }
                else
                {
                    ape_lexer_token_init(&outtok, TOKEN_GT, ">", 1);
                }
                break;
            }
            case ',':
                ape_lexer_token_init(&outtok, TOKEN_COMMA, ",", 1);
                break;
            case ';':
                ape_lexer_token_init(&outtok, TOKEN_SEMICOLON, ";", 1);
                break;
            case ':':
                ape_lexer_token_init(&outtok, TOKEN_COLON, ":", 1);
                break;
            case '(':
                ape_lexer_token_init(&outtok, TOKEN_LPAREN, "(", 1);
                break;
            case ')':
                ape_lexer_token_init(&outtok, TOKEN_RPAREN, ")", 1);
                break;
            case '{':
                ape_lexer_token_init(&outtok, TOKEN_LBRACE, "{", 1);
                break;
            case '}':
                ape_lexer_token_init(&outtok, TOKEN_RBRACE, "}", 1);
                break;
            case '[':
                ape_lexer_token_init(&outtok, TOKEN_LBRACKET, "[", 1);
                break;
            case ']':
                ape_lexer_token_init(&outtok, TOKEN_RBRACKET, "]", 1);
                break;
            case '.':
                ape_lexer_token_init(&outtok, TOKEN_DOT, ".", 1);
                break;
            case '?':
                ape_lexer_token_init(&outtok, TOKEN_QUESTION, "?", 1);
                break;
            case '%':
            {
                if(ape_lexer_peekchar(lex) == '=')
                {
                    ape_lexer_token_init(&outtok, TOKEN_PERCENT_ASSIGN, "%=", 2);
                    ape_lexer_readchar(lex);
                }
                else
                {
                    ape_lexer_token_init(&outtok, TOKEN_PERCENT, "%", 1);
                    break;
                }
                break;
            }
            case '"':
            {
                ape_lexer_readchar(lex);
                int len;
                const char* str = ape_lexer_readstring(lex, '"', false, NULL, &len);
                if(str)
                {
                    ape_lexer_token_init(&outtok, TOKEN_STRING, str, len);
                }
                else
                {
                    ape_lexer_token_init(&outtok, TOKEN_INVALID, NULL, 0);
                }
                break;
            }
            case '\'':
            {
                ape_lexer_readchar(lex);
                int len;
                const char* str = ape_lexer_readstring(lex, '\'', false, NULL, &len);
                if(str)
                {
                    ape_lexer_token_init(&outtok, TOKEN_STRING, str, len);
                }
                else
                {
                    ape_lexer_token_init(&outtok, TOKEN_INVALID, NULL, 0);
                }
                break;
            }
            case '`':
            {
                if(!lex->continue_template_string)
                {
                    ape_lexer_readchar(lex);
                }
                int len;
                bool templatefound = false;
                const char* str = ape_lexer_readstring(lex, '`', true, &templatefound, &len);
                if(str)
                {
                    if(templatefound)
                    {
                        ape_lexer_token_init(&outtok, TOKEN_TEMPLATE_STRING, str, len);
                    }
                    else
                    {
                        ape_lexer_token_init(&outtok, TOKEN_STRING, str, len);
                    }
                }
                else
                {
                    ape_lexer_token_init(&outtok, TOKEN_INVALID, NULL, 0);
                }
                break;
            }
            default:
            {
                if(ape_lexer_isletter(lex->ch))
                {
                    int identlen = 0;
                    const char* ident = ape_lexer_readident(lex, &identlen);
                    ApeTokenType_t type = ape_lexer_lookupident(ident, identlen);
                    ape_lexer_token_init(&outtok, type, ident, identlen);
                    return outtok;
                }
                else if(ape_lexer_isdigit(lex->ch))
                {
                    int numberlen = 0;
                    const char* number = ape_lexer_readnumber(lex, &numberlen);
                    ape_lexer_token_init(&outtok, TOKEN_NUMBER, number, numberlen);
                    return outtok;
                }
                break;
            }
        }
        ape_lexer_readchar(lex);
        if(ape_lexer_failed(lex))
        {
            ape_lexer_token_init(&outtok, TOKEN_INVALID, NULL, 0);
        }
        lex->continue_template_string = false;
        return outtok;
    }
}

bool ape_lexer_expectcurrent(ApeLexer_t* lex, ApeTokenType_t type)
{
    if(ape_lexer_failed(lex))
    {
        return false;
    }

    if(!ape_lexer_currenttokenis(lex, type))
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

static bool ape_lexer_readchar(ApeLexer_t* lex)
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
        bool ok = ape_lexer_addline(lex, lex->nextposition);
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

static char ape_lexer_peekchar(ApeLexer_t* lex)
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

static bool ape_lexer_isletter(char ch)
{
    return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z') || ch == '_';
}

static bool ape_lexer_isdigit(char ch)
{
    return ch >= '0' && ch <= '9';
}

static bool ape_lexer_isoneof(char ch, const char* allowed, int allowedlen)
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

static const char* ape_lexer_readident(ApeLexer_t* lex, int* outlen)
{
    int position = lex->position;
    int len = 0;
    while(ape_lexer_isdigit(lex->ch) || ape_lexer_isletter(lex->ch) || lex->ch == ':')
    {
        if(lex->ch == ':')
        {
            if(ape_lexer_peekchar(lex) != ':')
            {
                goto end;
            }
            ape_lexer_readchar(lex);
        }
        ape_lexer_readchar(lex);
    }
end:
    len = lex->position - position;
    *outlen = len;
    return lex->input + position;
}

static const char* ape_lexer_readnumber(ApeLexer_t* lex, int* outlen)
{
    char allowed[] = ".xXaAbBcCdDeEfF";
    int position = lex->position;
    while(ape_lexer_isdigit(lex->ch) || ape_lexer_isoneof(lex->ch, allowed, APE_ARRAY_LEN(allowed) - 1))
    {
        ape_lexer_readchar(lex);
    }
    int len = lex->position - position;
    *outlen = len;
    return lex->input + position;
}

static const char* ape_lexer_readstring(ApeLexer_t* lex, char delimiter, bool istemplate, bool* outtemplatefound, int* outlen)
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
        if(istemplate && !escaped && lex->ch == '$' && ape_lexer_peekchar(lex) == '{')
        {
            *outtemplatefound = true;
            break;
        }
        escaped = false;
        if(lex->ch == '\\')
        {
            escaped = true;
        }
        ape_lexer_readchar(lex);
    }
    int len = lex->position - position;
    *outlen = len;
    return lex->input + position;
}

static ApeTokenType_t ape_lexer_lookupident(const char* ident, int len)
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

static void ape_lexer_skipspace(ApeLexer_t* lex)
{
    char ch = lex->ch;
    while(ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
    {
        ape_lexer_readchar(lex);
        ch = lex->ch;
    }
}

static bool ape_lexer_addline(ApeLexer_t* lex, int offset)
{
    if(!lex->file)
    {
        return true;
    }
    if(lex->line < (ApeInt_t)ape_ptrarray_count(lex->file->lines))
    {
        return true;
    }
    const char* linestart = lex->input + offset;
    const char* newlineptr = strchr(linestart, '\n');
    char* line = NULL;
    if(!newlineptr)
    {
        line = ape_util_strdup(lex->context, linestart);
    }
    else
    {
        size_t linelen = newlineptr - linestart;
        line = ape_util_strndup(lex->context, linestart, linelen);
    }
    if(!line)
    {
        lex->failed = true;
        return false;
    }
    bool ok = ape_ptrarray_add(lex->file->lines, line);
    if(!ok)
    {
        lex->failed = true;
        allocator_free(lex->alloc, line);
        return false;
    }
    return true;
}

