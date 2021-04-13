#ifndef error_h
#define error_h

#ifndef APE_AMALGAMATED
#include "common.h"
#include "token.h"
#endif

typedef struct traceback traceback_t;

typedef enum error_type {
    ERROR_NONE = 0,
    ERROR_PARSING,
    ERROR_COMPILATION,
    ERROR_RUNTIME,
    ERROR_USER,
} error_type_t;

typedef struct errortag_t {
    error_type_t type;
    char *message;
    src_pos_t pos;
    traceback_t *traceback;
} errortag_t;

APE_INTERNAL errortag_t* error_make_no_copy(error_type_t type, src_pos_t pos, char *message);
APE_INTERNAL errortag_t* error_make(error_type_t type, src_pos_t pos, const char *message);
APE_INTERNAL errortag_t* error_makef(error_type_t type, src_pos_t pos, const char *format, ...) __attribute__ ((format (printf, 3, 4)));
APE_INTERNAL void error_destroy(errortag_t *error);
APE_INTERNAL const char *error_type_to_string(error_type_t type);

#endif /* error_h */
