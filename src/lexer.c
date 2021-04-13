#include <stdlib.h>
#include <string.h>

#ifndef APE_AMALGAMATED
#include "lexer.h"
#include "collections.h"
#include "compiler.h"
#endif

static void read_char(lexer_t *lex);
static char peek_char(lexer_t *lex);
static bool is_letter(char ch);
static bool is_digit(char ch);
static bool is_one_of(char ch, const char* allowed, int allowed_len);
static const char* read_identifier(lexer_t *lex, int *out_len);
static const char* read_number(lexer_t *lex, int *out_len);
static const char* read_string(lexer_t *lex, char delimiter, int *out_len);
static token_type_t lookup_identifier(const char *ident, int len);
static void skip_whitespace(lexer_t *lex);
static void add_line(lexer_t *lex, int offset);

bool lexer_init(lexer_t *lex, const char *input, compiled_file_t *file) {
    lex->input = input;
    lex->input_len = (int)strlen(input);
    lex->position = 0;
    lex->next_position = 0;
    lex->ch = '\0';
    if (file) {
        lex->line = ptrarray_count(file->lines);
    } else {
        lex->line = 0;
    }
    lex->column = -1;
    lex->file = file;
    add_line(lex, 0);
    read_char(lex);
    return true;
}

token_t lexer_next_token(lexer_t *lex) { 
    while (true) {
        skip_whitespace(lex);
        
        token_t out_tok;
        out_tok.type = TOKEN_ILLEGAL;
        out_tok.literal = lex->input + lex->position;
        out_tok.len = 1;
        out_tok.pos = src_pos_make(lex->file, lex->line, lex->column);

        switch (lex->ch) {
            case '\0': token_make(&out_tok, TOKEN_EOF, "EOF", 3); break;
            case '=': {
                if (peek_char(lex) == '=') {
                    token_make(&out_tok, TOKEN_EQ, "==", 2);
                    read_char(lex);
                } else {
                    token_make(&out_tok, TOKEN_ASSIGN, "=", 1);
                }
                break;
            }
            case '&': {
                if (peek_char(lex) == '&') {
                    token_make(&out_tok, TOKEN_AND, "&&", 2);
                    read_char(lex);
                } else if (peek_char(lex) == '=') {
                    token_make(&out_tok, TOKEN_BIT_AND_ASSIGN, "&=", 2);
                    read_char(lex);
                } else {
                    token_make(&out_tok, TOKEN_BIT_AND, "&", 1);
                }
                break;
            }
            case '|': {
                if (peek_char(lex) == '|') {
                    token_make(&out_tok, TOKEN_OR, "||", 2);
                    read_char(lex);
                } else if (peek_char(lex) == '=') {
                    token_make(&out_tok, TOKEN_BIT_OR_ASSIGN, "|=", 2);
                    read_char(lex);
                } else {
                    token_make(&out_tok, TOKEN_BIT_OR, "|", 1);
                }
                break;
            }
            case '^': {
                if (peek_char(lex) == '=') {
                    token_make(&out_tok, TOKEN_BIT_XOR_ASSIGN, "^=", 2);
                    read_char(lex);
                } else {
                    token_make(&out_tok, TOKEN_BIT_XOR, "^", 1); break;
                }
                break;
            }
            case '+': {
                if (peek_char(lex) == '=') {
                    token_make(&out_tok, TOKEN_PLUS_ASSIGN, "+=", 2);
                    read_char(lex);
                } else {
                    token_make(&out_tok, TOKEN_PLUS, "+", 1); break;
                }
                break;
            }
            case '-': {
                if (peek_char(lex) == '=') {
                    token_make(&out_tok, TOKEN_MINUS_ASSIGN, "-=", 2);
                    read_char(lex);
                } else {
                    token_make(&out_tok, TOKEN_MINUS, "-", 1); break;
                }
                break;
            }
            case '!': {
                if (peek_char(lex) == '=') {
                    token_make(&out_tok, TOKEN_NOT_EQ, "!=", 2);
                    read_char(lex);
                } else {
                    token_make(&out_tok, TOKEN_BANG, "!", 1);
                }
                break;
            }
            case '*': {
                if (peek_char(lex) == '=') {
                    token_make(&out_tok, TOKEN_ASTERISK_ASSIGN, "*=", 2);
                    read_char(lex);
                } else {
                    token_make(&out_tok, TOKEN_ASTERISK, "*", 1); break;
                }
                break;
            }
            case '/': {
                if (peek_char(lex) == '/') {
                    read_char(lex);
                    while (lex->ch != '\n' && lex->ch != '\0') {
                        read_char(lex);
                    }
                    continue;
                } else if (peek_char(lex) == '=') {
                    token_make(&out_tok, TOKEN_SLASH_ASSIGN, "/=", 2);
                    read_char(lex);
                } else {
                    token_make(&out_tok, TOKEN_SLASH, "/", 1); break;
                }
                break;
            }
            case '<': {
                if (peek_char(lex) == '=') {
                    token_make(&out_tok, TOKEN_LTE, "<=", 2);
                    read_char(lex);
                } else if (peek_char(lex) == '<') {
                    read_char(lex);
                    if (peek_char(lex) == '=') {
                        token_make(&out_tok, TOKEN_LSHIFT_ASSIGN, "<<=", 3);
                        read_char(lex);
                    } else {
                        token_make(&out_tok, TOKEN_LSHIFT, "<<", 2);
                    }
                } else {
                    token_make(&out_tok, TOKEN_LT, "<", 1); break;
                }
                break;
            }
            case '>': {
                if (peek_char(lex) == '=') {
                    token_make(&out_tok, TOKEN_GTE, ">=", 2);
                    read_char(lex);
                } else if (peek_char(lex) == '>') {
                    read_char(lex);
                    if (peek_char(lex) == '=') {
                        token_make(&out_tok, TOKEN_RSHIFT_ASSIGN, ">>=", 3);
                        read_char(lex);
                    } else {
                        token_make(&out_tok, TOKEN_RSHIFT, ">>", 2);
                    }
                } else {
                    token_make(&out_tok, TOKEN_GT, ">", 1);
                }
                break;
            }
            case ',': token_make(&out_tok, TOKEN_COMMA, ",", 1); break;
            case ';': token_make(&out_tok, TOKEN_SEMICOLON, ";", 1); break;
            case ':': token_make(&out_tok, TOKEN_COLON, ":", 1); break;
            case '(': token_make(&out_tok, TOKEN_LPAREN, "(", 1); break;
            case ')': token_make(&out_tok, TOKEN_RPAREN, ")", 1); break;
            case '{': token_make(&out_tok, TOKEN_LBRACE, "{", 1); break;
            case '}': token_make(&out_tok, TOKEN_RBRACE, "}", 1); break;
            case '[': token_make(&out_tok, TOKEN_LBRACKET, "[", 1); break;
            case ']': token_make(&out_tok, TOKEN_RBRACKET, "]", 1); break;
            case '.': token_make(&out_tok, TOKEN_DOT, ".", 1); break;
            case '%': {
                if (peek_char(lex) == '=') {
                    token_make(&out_tok, TOKEN_PERCENT_ASSIGN, "%=", 2);
                    read_char(lex);
                } else {
                    token_make(&out_tok, TOKEN_PERCENT, "%", 1); break;
                }
                break;
            }
            case '"': {
                int len;
                const char *str = read_string(lex, '"', &len);
                if (str) {
                    token_make(&out_tok, TOKEN_STRING, str, len);
                } else {
                    token_make(&out_tok, TOKEN_ILLEGAL, NULL, 0);
                }
                break;
            }
            case '\'': {
                int len;
                const char *str = read_string(lex, '\'', &len);
                if (str) {
                    token_make(&out_tok, TOKEN_STRING, str, len);
                } else {
                    token_make(&out_tok, TOKEN_ILLEGAL, NULL, 0);
                }
                break;
            }
            default: {
                if (is_letter(lex->ch)) {
                    int ident_len = 0;
                    const char *ident = read_identifier(lex, &ident_len);
                    token_type_t type = lookup_identifier(ident, ident_len);
                    token_make(&out_tok, type, ident, ident_len);
                    return out_tok;
                } else if (is_digit(lex->ch)) {
                    int number_len = 0;
                    const char *number = read_number(lex, &number_len);
                    token_make(&out_tok, TOKEN_NUMBER, number, number_len);
                    return out_tok;
                }
                break;
            }
        }
        read_char(lex);
        return out_tok;
    }
}

// INTERNAL
static void read_char(lexer_t *lex) {
    if (lex->next_position >= lex->input_len) {
        lex->ch = '\0';
    } else {
        lex->ch = lex->input[lex->next_position];
    }
    lex->position = lex->next_position;
    lex->next_position++;
    
    if (lex->ch == '\n') {
        lex->line++;
        lex->column = -1;
        add_line(lex, lex->next_position);
    } else {
        lex->column++;
    }
}

static char peek_char(lexer_t *lex) {
    if (lex->next_position >= lex->input_len) {
        return '\0';
    } else {
        return lex->input[lex->next_position];
    }
}

static bool is_letter(char ch) {
    return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z') || ch == '_';
}

static bool is_digit(char ch) {
    return ch >= '0' && ch <= '9';
}

static bool is_one_of(char ch, const char* allowed, int allowed_len) {
    for (int i = 0; i < allowed_len; i++) {
        if (ch == allowed[i]) {
            return true;
        }
    }
    return false;
}

static const char* read_identifier(lexer_t *lex, int *out_len) {
    int position = lex->position;
    int len = 0;
    while (is_digit(lex->ch) || is_letter(lex->ch) || lex->ch == ':') {
        if (lex->ch == ':') {
            if (peek_char(lex) != ':') {
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

static const char* read_number(lexer_t *lex, int *out_len) {
    char allowed[] = ".xXaAbBcCdDeEfF";
    int position = lex->position;
    while (is_digit(lex->ch) || is_one_of(lex->ch, allowed, APE_ARRAY_LEN(allowed) - 1)) {
        read_char(lex);
    }
    int len = lex->position - position;
    *out_len = len;
    return lex->input + position;
}

static const char* read_string(lexer_t *lex, char delimiter, int *out_len) {
    *out_len = 0;

    bool escaped = false;
    int position = lex->position + 1;
    while (true) {
        read_char(lex);
        if (lex->ch == '\0') {
            return NULL;
        }
        if (lex->ch == delimiter && !escaped) {
            break;
        }
        escaped = false;
        if (lex->ch == '\\') {
            escaped = true;
        }
    }
    int len = lex->position - position;
    *out_len = len;
    return lex->input + position;
}

static token_type_t lookup_identifier(const char *ident, int len) {
    struct {
        const char *value;
        int len;
        token_type_t type;
    } keywords[] = {
        {"fn", 2, TOKEN_FUNCTION},
        {"const", 5, TOKEN_CONST},
        {"var", 3, TOKEN_VAR},
        {"true", 4, TOKEN_TRUE},
        {"false", 5, TOKEN_FALSE},
        {"if", 2, TOKEN_IF},
        {"else", 4, TOKEN_ELSE},
        {"return", 6, TOKEN_RETURN},
        {"while", 5, TOKEN_WHILE},
        {"break", 5, TOKEN_BREAK},
        {"for", 3, TOKEN_FOR},
        {"in", 2, TOKEN_IN},
        {"continue", 8, TOKEN_CONTINUE},
        {"null", 4, TOKEN_NULL},
        {"import", 6, TOKEN_IMPORT},
        {"recover", 7, TOKEN_RECOVER},
    };

    for (int i = 0; i < APE_ARRAY_LEN(keywords); i++) {
        if (keywords[i].len == len && APE_STRNEQ(ident, keywords[i].value, len)) {
            return keywords[i].type;
        }
    }
    
    return TOKEN_IDENT;
}

static void skip_whitespace(lexer_t *lex) {
    char ch = lex->ch;
    while (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
        read_char(lex);
        ch = lex->ch;
    }
}

static void add_line(lexer_t *lex, int offset) {
    if (!lex->file) {
        return;
    }
    const char *line_start = lex->input + offset;
    const char *new_line_ptr = strchr(line_start, '\n');
    char *line = NULL;
    if (!new_line_ptr) {
        line = ape_strdup(line_start);
    } else {
        size_t line_len = new_line_ptr - line_start;
        line = ape_strndup(line_start, line_len);
    }
    ptrarray_add(lex->file->lines, line);
}
