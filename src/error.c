#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef APE_AMALGAMATED
#include "error.h"
#include "traceback.h"
#endif

errortag_t* error_make_no_copy(error_type_t type, src_pos_t pos, char *message) {
    errortag_t *err = ape_malloc(sizeof(errortag_t));
    memset(err, 0, sizeof(errortag_t));
    err->type = type;
    err->message = message;
    err->pos = pos;
    err->traceback = NULL;
    return err;
}

errortag_t* error_make(error_type_t type, src_pos_t pos, const char *message) {
    return error_make_no_copy(type, pos, ape_strdup(message));
}

errortag_t* error_makef(error_type_t type, src_pos_t pos, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int to_write = vsnprintf(NULL, 0, format, args);
    va_end(args);
    va_start(args, format);
    char *res = (char*)ape_malloc(to_write + 1);
    vsprintf(res, format, args);
    va_end(args);
    return error_make_no_copy(type, pos, res);
}

void error_destroy(errortag_t *error) {
    if (!error) {
        return;
    }
    traceback_destroy(error->traceback);
    ape_free(error->message);
    ape_free(error);
}

const char *error_type_to_string(error_type_t type) {
    switch (type) {
        case ERROR_PARSING: return "PARSING";
        case ERROR_COMPILATION: return "COMPILATION";
        case ERROR_RUNTIME: return "RUNTIME";
        case ERROR_USER: return "USER";
        default: return "INVALID";
    }
}
