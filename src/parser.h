#ifndef parser_h
#define parser_h

#ifndef APE_AMALGAMATED
#include "common.h"
#include "lexer.h"
#include "token.h"
#include "ast.h"
#include "collections.h"
#endif

typedef struct parser parser_t;
typedef struct errortag_t errortag_t;

typedef expression_t* (*prefix_parse_fn)(parser_t *p);
typedef expression_t* (*infix_parse_fn)(parser_t *p, expression_t *expr);

typedef struct parser {
    const ape_config_t *config;
    lexer_t lexer;
    token_t cur_token;
    token_t peek_token;
    ptrarray(errortag_t) *errors;
    
    prefix_parse_fn prefix_parse_fns[TOKEN_TYPE_MAX];
    infix_parse_fn infix_parse_fns[TOKEN_TYPE_MAX];

    int depth;
} parser_t;

APE_INTERNAL parser_t* parser_make(const ape_config_t *config, ptrarray(errortag_t) *errors);
APE_INTERNAL void parser_destroy(parser_t *parser);

APE_INTERNAL ptrarray(statement_t)* parser_parse_all(parser_t *parser,  const char *input, compiled_file_t *file);

#endif /* parser_h */
