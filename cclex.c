
#include "inline.h"

static bool ape_lexer_readchar(ApeAstLexer_t* lex);
static char ape_lexer_peekchar(ApeAstLexer_t* lex);
static bool ape_lexer_isletter(char ch);
static bool ape_lexer_isdigit(char ch);
static bool ape_lexer_isoneof(char ch, const char* allowed, int allowedlen);
static const char* ape_lexer_readident(ApeAstLexer_t* lex, int* outlen);
static const char* ape_lexer_readnumber(ApeAstLexer_t* lex, int* outlen);
static const char* ape_lexer_readstring(ApeAstLexer_t* lex, char delimiter, bool istemplate, bool* outtemplatefound, int* outlen);
static void ape_lexer_skipspace(ApeAstLexer_t* lex);
static bool ape_lexer_addline(ApeAstLexer_t* lex, int offset);

static ApeAstTokType_t ape_lexer_lookupident(const char* ident, ApeSize_t len)
{
    int i;
    ApeSize_t kwlen;
    static struct
    {
        const char* value;
        ApeAstTokType_t type;
    } keywords[] = {
        { "function", TOKEN_KWFUNCTION },
        { "const", TOKEN_KWCONST },
        { "var", TOKEN_KWVAR },
        { "true", TOKEN_KWTRUE },
        { "false", TOKEN_KWFALSE },
        { "if", TOKEN_KWIF },
        { "else", TOKEN_KWELSE },
        { "return", TOKEN_KWRETURN },
        { "while", TOKEN_KWWHILE },
        { "break", TOKEN_KWBREAK },
        { "for", TOKEN_KWFOR },
        { "in", TOKEN_KWIN },
        { "continue", TOKEN_KWCONTINUE },
        { "null", TOKEN_KWNULL },
        { "import", TOKEN_KWIMPORT },
        { "include", TOKEN_KWINCLUDE },
        { "recover", TOKEN_KWRECOVER },
        #if 0
        { "global", TOKEN_KWGLOBAL },
        #endif
    };

    for(i = 0; i < APE_ARRAY_LEN(keywords); i++)
    {
        kwlen = strlen(keywords[i].value);
        if(kwlen == len && APE_STRNEQ(ident, keywords[i].value, len))
        {
            return keywords[i].type;
        }
    }
    return TOKEN_VALIDENT;
}

static ApePosition_t ape_lexer_makesourcepos(const ApeAstCompFile_t* file, int line, int column)
{
    return (ApePosition_t){
        .file = file,
        .line = line,
        .column = column,
    };
}

void ape_lexer_token_init(ApeAstToken_t* tok, ApeAstTokType_t type, const char* literal, int len)
{
    tok->toktype = type;
    tok->literal = literal;
    tok->len = len;
}

char* ape_lexer_tokendupliteral(ApeContext_t* ctx, const ApeAstToken_t* tok)
{
    return ape_util_strndup(ctx, tok->literal, tok->len);
}

bool ape_lexer_init(ApeAstLexer_t* lex, ApeContext_t* ctx, ApeErrorList_t* errs, const char* input, ApeAstCompFile_t* file)
{
    lex->context = ctx;
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

bool ape_lexer_failed(ApeAstLexer_t* lex)
{
    return lex->failed;
}

void ape_lexer_continuetemplatestring(ApeAstLexer_t* lex)
{
    lex->continue_template_string = true;
}

bool ape_lexer_currenttokenis(ApeAstLexer_t* lex, ApeAstTokType_t type)
{
    return lex->curtoken.toktype == type;
}

bool ape_lexer_peektokenis(ApeAstLexer_t* lex, ApeAstTokType_t type)
{
    return lex->peektoken.toktype == type;
}

bool ape_lexer_nexttoken(ApeAstLexer_t* lex)
{
    lex->prevtoken = lex->curtoken;
    lex->curtoken = lex->peektoken;
    lex->peektoken = ape_lexer_internalnexttoken(lex);
    return !lex->failed;
}

bool ape_lexer_previous_token(ApeAstLexer_t* lex)
{
    if(lex->prevtoken.toktype == TOKEN_INVALID)
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

ApeAstToken_t ape_lexer_internalnexttoken(ApeAstLexer_t* lex)
{
    bool templatefound;
    char c;    
    int len;
    int identlen;
    int numberlen;
    const char* str;
    const char* identstr;
    const char* numberstr;
    ApeAstTokType_t type;
    ApeAstToken_t outtok;
    outtok.toktype = TOKEN_INVALID;
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
        outtok.toktype = TOKEN_INVALID;
        outtok.literal = lex->input + lex->position;
        outtok.len = 1;
        outtok.pos = ape_lexer_makesourcepos(lex->file, lex->line, lex->column);
        c = lex->continue_template_string ? '`' : lex->ch;
        switch(c)
        {
            case '\0':
                {
                    ape_lexer_token_init(&outtok, TOKEN_EOF, "EOF", 3);
                }
                break;
            case '=':
                {
                    if(ape_lexer_peekchar(lex) == '=')
                    {
                        ape_lexer_token_init(&outtok, TOKEN_OPEQUAL, "==", 2);
                        ape_lexer_readchar(lex);
                    }
                    else
                    {
                        ape_lexer_token_init(&outtok, TOKEN_ASSIGN, "=", 1);
                    }
                }
                break;
            case '&':
                {
                    if(ape_lexer_peekchar(lex) == '&')
                    {
                        ape_lexer_token_init(&outtok, TOKEN_OPAND, "&&", 2);
                        ape_lexer_readchar(lex);
                    }
                    else if(ape_lexer_peekchar(lex) == '=')
                    {
                        ape_lexer_token_init(&outtok, TOKEN_ASSIGNBITAND, "&=", 2);
                        ape_lexer_readchar(lex);
                    }
                    else
                    {
                        ape_lexer_token_init(&outtok, TOKEN_OPBITAND, "&", 1);
                    }
                }
                break;
            case '|':
                {
                    if(ape_lexer_peekchar(lex) == '|')
                    {
                        ape_lexer_token_init(&outtok, TOKEN_OPOR, "||", 2);
                        ape_lexer_readchar(lex);
                    }
                    else if(ape_lexer_peekchar(lex) == '=')
                    {
                        ape_lexer_token_init(&outtok, TOKEN_ASSIGNBITOR, "|=", 2);
                        ape_lexer_readchar(lex);
                    }
                    else
                    {
                        ape_lexer_token_init(&outtok, TOKEN_OPBITOR, "|", 1);
                    }
                }
                break;
            case '^':
                {
                    if(ape_lexer_peekchar(lex) == '=')
                    {
                        ape_lexer_token_init(&outtok, TOKEN_ASSIGNBITXOR, "^=", 2);
                        ape_lexer_readchar(lex);
                    }
                    else
                    {
                        ape_lexer_token_init(&outtok, TOKEN_OPBITXOR, "^", 1);
                        break;
                    }
                }
                break;
            case '+':
                {
                    if(ape_lexer_peekchar(lex) == '=')
                    {
                        ape_lexer_token_init(&outtok, TOKEN_ASSIGNPLUS, "+=", 2);
                        ape_lexer_readchar(lex);
                    }
                    else if(ape_lexer_peekchar(lex) == '+')
                    {
                        ape_lexer_token_init(&outtok, TOKEN_OPINCREASE, "++", 2);
                        ape_lexer_readchar(lex);
                    }
                    else
                    {
                        ape_lexer_token_init(&outtok, TOKEN_OPPLUS, "+", 1);
                        break;
                    }
                }
                break;
            case '-':
                {
                    if(ape_lexer_peekchar(lex) == '=')
                    {
                        ape_lexer_token_init(&outtok, TOKEN_ASSIGNMINUS, "-=", 2);
                        ape_lexer_readchar(lex);
                    }
                    else if(ape_lexer_peekchar(lex) == '-')
                    {
                        ape_lexer_token_init(&outtok, TOKEN_OPDECREASE, "--", 2);
                        ape_lexer_readchar(lex);
                    }
                    else
                    {
                        ape_lexer_token_init(&outtok, TOKEN_OPMINUS, "-", 1);
                        break;
                    }
                }
                break;
            case '!':
                {
                    if(ape_lexer_peekchar(lex) == '=')
                    {
                        ape_lexer_token_init(&outtok, TOKEN_OPNOTEQUAL, "!=", 2);
                        ape_lexer_readchar(lex);
                    }
                    else
                    {
                        ape_lexer_token_init(&outtok, TOKEN_OPNOT, "!", 1);
                    }
                }
                break;
            case '*':
                {
                    if(ape_lexer_peekchar(lex) == '=')
                    {
                        ape_lexer_token_init(&outtok, TOKEN_ASSIGNSTAR, "*=", 2);
                        ape_lexer_readchar(lex);
                    }
                    else
                    {
                        ape_lexer_token_init(&outtok, TOKEN_OPSTAR, "*", 1);
                        break;
                    }
                }
                break;
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
                        ape_lexer_token_init(&outtok, TOKEN_ASSIGNSLASH, "/=", 2);
                        ape_lexer_readchar(lex);
                    }
                    else
                    {
                        ape_lexer_token_init(&outtok, TOKEN_OPSLASH, "/", 1);
                        break;
                    }
                }
                break;
            case '<':
                {
                    if(ape_lexer_peekchar(lex) == '=')
                    {
                        ape_lexer_token_init(&outtok, TOKEN_OPLESSEQUAL, "<=", 2);
                        ape_lexer_readchar(lex);
                    }
                    else if(ape_lexer_peekchar(lex) == '<')
                    {
                        ape_lexer_readchar(lex);
                        if(ape_lexer_peekchar(lex) == '=')
                        {
                            ape_lexer_token_init(&outtok, TOKEN_ASSIGNLEFTSHIFT, "<<=", 3);
                            ape_lexer_readchar(lex);
                        }
                        else
                        {
                            ape_lexer_token_init(&outtok, TOKEN_OPLEFTSHIFT, "<<", 2);
                        }
                    }
                    else
                    {
                        ape_lexer_token_init(&outtok, TOKEN_OPLESSTHAN, "<", 1);
                        break;
                    }
                }
                break;
            case '>':
                {
                    if(ape_lexer_peekchar(lex) == '=')
                    {
                        ape_lexer_token_init(&outtok, TOKEN_OPGREATERTEQUAL, ">=", 2);
                        ape_lexer_readchar(lex);
                    }
                    else if(ape_lexer_peekchar(lex) == '>')
                    {
                        ape_lexer_readchar(lex);
                        if(ape_lexer_peekchar(lex) == '=')
                        {
                            ape_lexer_token_init(&outtok, TOKEN_ASSIGNRIGHTSHIFT, ">>=", 3);
                            ape_lexer_readchar(lex);
                        }
                        else
                        {
                            ape_lexer_token_init(&outtok, TOKEN_OPRIGHTSHIFT, ">>", 2);
                        }
                    }
                    else
                    {
                        ape_lexer_token_init(&outtok, TOKEN_OPGREATERTHAN, ">", 1);
                    }
                }
                break;
            case ',':
                {
                    ape_lexer_token_init(&outtok, TOKEN_OPCOMMA, ",", 1);
                }
                break;
            case ';':
                {
                    ape_lexer_token_init(&outtok, TOKEN_OPSEMICOLON, ";", 1);
                }
                break;
            case ':':
                {
                    ape_lexer_token_init(&outtok, TOKEN_OPCOLON, ":", 1);
                }
                break;
            case '(':
                {
                    ape_lexer_token_init(&outtok, TOKEN_OPLEFTPAREN, "(", 1);
                }
                break;
            case ')':
                {
                    ape_lexer_token_init(&outtok, TOKEN_OPRIGHTPAREN, ")", 1);
                }
                break;
            case '{':
                {
                    ape_lexer_token_init(&outtok, TOKEN_OPLEFTBRACE, "{", 1);
                }
                break;
            case '}':
                {
                    ape_lexer_token_init(&outtok, TOKEN_OPRIGHTBRACE, "}", 1);
                }
                break;
            case '[':
                {
                    ape_lexer_token_init(&outtok, TOKEN_OPLEFTBRACKET, "[", 1);
                }
                break;
            case ']':
                {
                    ape_lexer_token_init(&outtok, TOKEN_OPRIGHTBRACKET, "]", 1);
                }
                break;
            case '.':
                {
                    ape_lexer_token_init(&outtok, TOKEN_OPDOT, ".", 1);
                }
                break;
            case '?':
                {
                    ape_lexer_token_init(&outtok, TOKEN_OPQUESTION, "?", 1);
                }
                break;
            case '%':
                {
                    if(ape_lexer_peekchar(lex) == '=')
                    {
                        ape_lexer_token_init(&outtok, TOKEN_ASSIGNMODULO, "%=", 2);
                        ape_lexer_readchar(lex);
                    }
                    else
                    {
                        ape_lexer_token_init(&outtok, TOKEN_OPMODULO, "%", 1);
                        break;
                    }
                }
                break;
            case '"':
                {
                    ape_lexer_readchar(lex);
                    str = ape_lexer_readstring(lex, '"', false, NULL, &len);
                    if(str)
                    {
                        ape_lexer_token_init(&outtok, TOKEN_VALSTRING, str, len);
                    }
                    else
                    {
                        ape_lexer_token_init(&outtok, TOKEN_INVALID, NULL, 0);
                    }
                }
                break;
            case '\'':
                {
                    ape_lexer_readchar(lex);
                    str = ape_lexer_readstring(lex, '\'', false, NULL, &len);
                    if(str)
                    {
                        ape_lexer_token_init(&outtok, TOKEN_VALSTRING, str, len);
                    }
                    else
                    {
                        ape_lexer_token_init(&outtok, TOKEN_INVALID, NULL, 0);
                    }
                }
                break;
            case '`':
                {
                    if(!lex->continue_template_string)
                    {
                        ape_lexer_readchar(lex);
                    }
                    templatefound = false;
                    str = ape_lexer_readstring(lex, '`', true, &templatefound, &len);
                    if(str)
                    {
                        if(templatefound)
                        {
                            ape_lexer_token_init(&outtok, TOKEN_VALTPLSTRING, str, len);
                        }
                        else
                        {
                            ape_lexer_token_init(&outtok, TOKEN_VALSTRING, str, len);
                        }
                    }
                    else
                    {
                        ape_lexer_token_init(&outtok, TOKEN_INVALID, NULL, 0);
                    }
                }
                break;
            default:
                {
                    if(ape_lexer_isletter(lex->ch))
                    {
                        identlen = 0;
                        identstr = ape_lexer_readident(lex, &identlen);
                        type = ape_lexer_lookupident(identstr, identlen);
                        ape_lexer_token_init(&outtok, type, identstr, identlen);
                        return outtok;
                    }
                    else if(ape_lexer_isdigit(lex->ch))
                    {
                        numberlen = 0;
                        numberstr = ape_lexer_readnumber(lex, &numberlen);
                        ape_lexer_token_init(&outtok, TOKEN_VALNUMBER, numberstr, numberlen);
                        return outtok;
                    }
                }
                break;
        }
        ape_lexer_readchar(lex);
        if(ape_lexer_failed(lex))
        {
            ape_lexer_token_init(&outtok, TOKEN_INVALID, NULL, 0);
        }
        lex->continue_template_string = false;
        return outtok;
    }
    outtok.toktype = TOKEN_INVALID;
    return outtok;
}

bool ape_lexer_expectcurrent(ApeAstLexer_t* lex, ApeAstTokType_t type)
{
    const char* expectedtypestr;
    const char* actualtypestr;
    if(ape_lexer_failed(lex))
    {
        return false;
    }
    if(!ape_lexer_currenttokenis(lex, type))
    {
        expectedtypestr = ape_tostring_tokentype(type);
        actualtypestr = ape_tostring_tokentype(lex->curtoken.toktype);
        ape_errorlist_addformat(lex->errors, APE_ERROR_PARSING, lex->curtoken.pos,
            "expected current token to be '%s', got '%s' instead", expectedtypestr, actualtypestr);
        return false;
    }
    return true;
}

static bool ape_lexer_readchar(ApeAstLexer_t* lex)
{
    bool ok;
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
        ok = ape_lexer_addline(lex, lex->nextposition);
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

static char ape_lexer_peekchar(ApeAstLexer_t* lex)
{
    if(lex->nextposition >= lex->inputlen)
    {
        return '\0';
    }
    return lex->input[lex->nextposition];
}

static bool ape_lexer_isletter(char ch)
{
    return (
        (('a' <= ch) && (ch <= 'z')) || (('A' <= ch) && (ch <= 'Z')) || (ch == '_')
    );
}

static bool ape_lexer_isdigit(char ch)
{
    return ((ch >= '0') && (ch <= '9'));
}

static bool ape_lexer_isoneof(char ch, const char* allowed, int allowedlen)
{
    int i;
    for(i = 0; i < allowedlen; i++)
    {
        if(ch == allowed[i])
        {
            return true;
        }
    }
    return false;
}

static const char* ape_lexer_readident(ApeAstLexer_t* lex, int* outlen)
{
    int len;
    int position;
    position = lex->position;
    len = 0;
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

static const char* ape_lexer_readnumber(ApeAstLexer_t* lex, int* outlen)
{
    int len;
    int position;
    static const char allowed[] = ".xXaAbBcCdDeEfF";
    position = lex->position;
    while(ape_lexer_isdigit(lex->ch) || ape_lexer_isoneof(lex->ch, allowed, APE_ARRAY_LEN(allowed) - 1))
    {
        ape_lexer_readchar(lex);
    }
    len = lex->position - position;
    *outlen = len;
    return lex->input + position;
}

static const char* ape_lexer_readstring(ApeAstLexer_t* lex, char delimiter, bool istemplate, bool* outtemplatefound, int* outlen)
{
    bool escaped;
    int len;
    int position;
    *outlen = 0;
    escaped = false;
    position = lex->position;
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
    len = lex->position - position;
    *outlen = len;
    return lex->input + position;
}


static void ape_lexer_skipspace(ApeAstLexer_t* lex)
{
    char ch;
    ch = lex->ch;
    while(ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
    {
        ape_lexer_readchar(lex);
        ch = lex->ch;
    }
}

static bool ape_lexer_addline(ApeAstLexer_t* lex, int offset)
{
    bool ok;
    size_t linelen;
    char* line;
    const char* linestart;
    const char* newlineptr;
    if(!lex->file)
    {
        return true;
    }
    if(lex->line < (ApeInt_t)ape_ptrarray_count(lex->file->lines))
    {
        return true;
    }
    linestart = lex->input + offset;
    newlineptr = strchr(linestart, '\n');
    line= NULL;
    if(!newlineptr)
    {
        line = ape_util_strdup(lex->context, linestart);
    }
    else
    {
        linelen = newlineptr - linestart;
        line = ape_util_strndup(lex->context, linestart, linelen);
    }
    if(!line)
    {
        lex->failed = true;
        return false;
    }
    ok = ape_ptrarray_add(lex->file->lines, line);
    if(!ok)
    {
        lex->failed = true;
        ape_allocator_free(&lex->context->alloc, line);
        return false;
    }
    return true;
}

