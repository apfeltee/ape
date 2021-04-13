#include <string.h>
#include <stdlib.h>

#ifndef APE_AMALGAMATED
#include "token.h"
#endif

static const char *g_type_names[] = {
    "ILLEGAL",
    "EOF",
    "=",
    "+=",
    "-=",
    "*=",
    "/=",
    "%=",
    "&=",
    "|=",
    "^=",
    "<<=",
    ">>=",
    "+",
    "-",
    "!",
    "*",
    "/",
    "<",
    "<=",
    ">",
    ">=",
    "==",
    "!=",
    "&&",
    "||",
    "&",
    "|",
    "^",
    "<<",
    ">>",
    ",",
    ";",
    ":",
    "(",
    ")",
    "{",
    "}",
    "[",
    "]",
    ".",
    "%",
    "FUNCTION",
    "CONST",
    "VAR",
    "TRUE",
    "FALSE",
    "IF",
    "ELSE",
    "RETURN",
    "WHILE",
    "BREAK",
    "FOR",
    "IN",
    "CONTINUE",
    "NULL",
    "IMPORT",
    "RECOVER",
    "IDENT",
    "NUMBER",
    "STRING",
};

void token_make(token_t *tok, token_type_t type, const char *literal, int len) {
    tok->type = type;
    tok->literal = literal;
    tok->len = len;
}

char* token_duplicate_literal(const token_t *tok) {
    return ape_strndup(tok->literal, tok->len);
}

const char* token_type_to_string(token_type_t type) {
    return g_type_names[type];
}
