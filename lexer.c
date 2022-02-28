
#include "ape.h"

static bool read_char(lexer_t* lex);
static char peek_char(lexer_t* lex);
static bool is_letter(char ch);
static bool is_digit(char ch);
static bool is_one_of(char ch, const char* allowed, int allowed_len);
static const char* read_identifier(lexer_t* lex, int* out_len);
static const char* read_number(lexer_t* lex, int* out_len);
static const char* read_string(lexer_t* lex, char delimiter, bool is_template, bool* out_template_found, int* out_len);
static token_type_t lookup_identifier(const char* ident, int len);
static void skip_whitespace(lexer_t* lex);
static bool add_line(lexer_t* lex, int offset);

bool lexer_init(lexer_t* lex, allocator_t* alloc, errors_t* errs, const char* input, compiled_file_t* file)
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
    bool ok = add_line(lex, 0);
    if(!ok)
    {
        return false;
    }
    ok = read_char(lex);
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

bool lexer_failed(lexer_t* lex)
{
    return lex->failed;
}

void lexer_continue_template_string(lexer_t* lex)
{
    lex->continue_template_string = true;
}

bool lexer_cur_token_is(lexer_t* lex, token_type_t type)
{
    return lex->cur_token.type == type;
}

bool lexer_peek_token_is(lexer_t* lex, token_type_t type)
{
    return lex->peek_token.type == type;
}

bool lexer_next_token(lexer_t* lex)
{
    lex->prev_token = lex->cur_token;
    lex->cur_token = lex->peek_token;
    lex->peek_token = lexer_next_token_internal(lex);
    return !lex->failed;
}

bool lexer_previous_token(lexer_t* lex)
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

token_t lexer_next_token_internal(lexer_t* lex)
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
            skip_whitespace(lex);
        }

        token_t out_tok;
        out_tok.type = TOKEN_INVALID;
        out_tok.literal = lex->input + lex->position;
        out_tok.len = 1;
        out_tok.pos = src_pos_make(lex->file, lex->line, lex->column);

        char c = lex->continue_template_string ? '`' : lex->ch;

        switch(c)
        {
            case '\0':
                token_init(&out_tok, TOKEN_EOF, "EOF", 3);
                break;
            case '=':
            {
                if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_EQ, "==", 2);
                    read_char(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_ASSIGN, "=", 1);
                }
                break;
            }
            case '&':
            {
                if(peek_char(lex) == '&')
                {
                    token_init(&out_tok, TOKEN_AND, "&&", 2);
                    read_char(lex);
                }
                else if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_BIT_AND_ASSIGN, "&=", 2);
                    read_char(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_BIT_AND, "&", 1);
                }
                break;
            }
            case '|':
            {
                if(peek_char(lex) == '|')
                {
                    token_init(&out_tok, TOKEN_OR, "||", 2);
                    read_char(lex);
                }
                else if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_BIT_OR_ASSIGN, "|=", 2);
                    read_char(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_BIT_OR, "|", 1);
                }
                break;
            }
            case '^':
            {
                if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_BIT_XOR_ASSIGN, "^=", 2);
                    read_char(lex);
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
                if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_PLUS_ASSIGN, "+=", 2);
                    read_char(lex);
                }
                else if(peek_char(lex) == '+')
                {
                    token_init(&out_tok, TOKEN_PLUS_PLUS, "++", 2);
                    read_char(lex);
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
                if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_MINUS_ASSIGN, "-=", 2);
                    read_char(lex);
                }
                else if(peek_char(lex) == '-')
                {
                    token_init(&out_tok, TOKEN_MINUS_MINUS, "--", 2);
                    read_char(lex);
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
                if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_NOT_EQ, "!=", 2);
                    read_char(lex);
                }
                else
                {
                    token_init(&out_tok, TOKEN_BANG, "!", 1);
                }
                break;
            }
            case '*':
            {
                if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_ASTERISK_ASSIGN, "*=", 2);
                    read_char(lex);
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
                if(peek_char(lex) == '/')
                {
                    read_char(lex);
                    while(lex->ch != '\n' && lex->ch != '\0')
                    {
                        read_char(lex);
                    }
                    continue;
                }
                else if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_SLASH_ASSIGN, "/=", 2);
                    read_char(lex);
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
                if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_LTE, "<=", 2);
                    read_char(lex);
                }
                else if(peek_char(lex) == '<')
                {
                    read_char(lex);
                    if(peek_char(lex) == '=')
                    {
                        token_init(&out_tok, TOKEN_LSHIFT_ASSIGN, "<<=", 3);
                        read_char(lex);
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
                if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_GTE, ">=", 2);
                    read_char(lex);
                }
                else if(peek_char(lex) == '>')
                {
                    read_char(lex);
                    if(peek_char(lex) == '=')
                    {
                        token_init(&out_tok, TOKEN_RSHIFT_ASSIGN, ">>=", 3);
                        read_char(lex);
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
                if(peek_char(lex) == '=')
                {
                    token_init(&out_tok, TOKEN_PERCENT_ASSIGN, "%=", 2);
                    read_char(lex);
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
                read_char(lex);
                int len;
                const char* str = read_string(lex, '"', false, NULL, &len);
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
                read_char(lex);
                int len;
                const char* str = read_string(lex, '\'', false, NULL, &len);
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
                    read_char(lex);
                }
                int len;
                bool template_found = false;
                const char* str = read_string(lex, '`', true, &template_found, &len);
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
                if(is_letter(lex->ch))
                {
                    int ident_len = 0;
                    const char* ident = read_identifier(lex, &ident_len);
                    token_type_t type = lookup_identifier(ident, ident_len);
                    token_init(&out_tok, type, ident, ident_len);
                    return out_tok;
                }
                else if(is_digit(lex->ch))
                {
                    int number_len = 0;
                    const char* number = read_number(lex, &number_len);
                    token_init(&out_tok, TOKEN_NUMBER, number, number_len);
                    return out_tok;
                }
                break;
            }
        }
        read_char(lex);
        if(lexer_failed(lex))
        {
            token_init(&out_tok, TOKEN_INVALID, NULL, 0);
        }
        lex->continue_template_string = false;
        return out_tok;
    }
}

bool lexer_expect_current(lexer_t* lex, token_type_t type)
{
    if(lexer_failed(lex))
    {
        return false;
    }

    if(!lexer_cur_token_is(lex, type))
    {
        const char* expected_type_str = token_type_to_string(type);
        const char* actual_type_str = token_type_to_string(lex->cur_token.type);
        errors_add_errorf(lex->errors, APE_ERROR_PARSING, lex->cur_token.pos,
                          "Expected current token to be \"%s\", got \"%s\" instead", expected_type_str, actual_type_str);
        return false;
    }
    return true;
}

// INTERNAL

static bool read_char(lexer_t* lex)
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
        bool ok = add_line(lex, lex->next_position);
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

static char peek_char(lexer_t* lex)
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

static bool is_letter(char ch)
{
    return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z') || ch == '_';
}

static bool is_digit(char ch)
{
    return ch >= '0' && ch <= '9';
}

static bool is_one_of(char ch, const char* allowed, int allowed_len)
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

static const char* read_identifier(lexer_t* lex, int* out_len)
{
    int position = lex->position;
    int len = 0;
    while(is_digit(lex->ch) || is_letter(lex->ch) || lex->ch == ':')
    {
        if(lex->ch == ':')
        {
            if(peek_char(lex) != ':')
            {
                goto end;
            }
            read_char(lex);
        }
        read_char(lex);
    }
end:
    len = lex->position - position;
    *out_len = len;
    return lex->input + position;
}

static const char* read_number(lexer_t* lex, int* out_len)
{
    char allowed[] = ".xXaAbBcCdDeEfF";
    int position = lex->position;
    while(is_digit(lex->ch) || is_one_of(lex->ch, allowed, APE_ARRAY_LEN(allowed) - 1))
    {
        read_char(lex);
    }
    int len = lex->position - position;
    *out_len = len;
    return lex->input + position;
}

static const char* read_string(lexer_t* lex, char delimiter, bool is_template, bool* out_template_found, int* out_len)
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
        if(is_template && !escaped && lex->ch == '$' && peek_char(lex) == '{')
        {
            *out_template_found = true;
            break;
        }
        escaped = false;
        if(lex->ch == '\\')
        {
            escaped = true;
        }
        read_char(lex);
    }
    int len = lex->position - position;
    *out_len = len;
    return lex->input + position;
}

static token_type_t lookup_identifier(const char* ident, int len)
{
    struct
    {
        const char* value;
        int len;
        token_type_t type;
    } keywords[] = {
        { "fn", 2, TOKEN_FUNCTION },
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

static void skip_whitespace(lexer_t* lex)
{
    char ch = lex->ch;
    while(ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
    {
        read_char(lex);
        ch = lex->ch;
    }
}

static bool add_line(lexer_t* lex, int offset)
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
        line = ape_strdup(lex->alloc, line_start);
    }
    else
    {
        size_t line_len = new_line_ptr - line_start;
        line = ape_strndup(lex->alloc, line_start, line_len);
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

