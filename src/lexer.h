#ifndef lexer_h
#define lexer_h

#include <stdbool.h>
#include <stddef.h>

#ifndef APE_AMALGAMATED
#include "common.h"
#include "token.h"
#endif

typedef struct lexer {
    const char *input;
    int input_len;
    int position;
    int next_position;
    char ch;
    int line;
    int column;
    compiled_file_t *file;
} lexer_t;

APE_INTERNAL bool lexer_init(lexer_t *lex, const char *input, compiled_file_t *file); // no need to deinit

APE_INTERNAL token_t lexer_next_token(lexer_t *lex);

#endif /* lexer_h */
