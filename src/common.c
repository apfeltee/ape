#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#ifndef APE_AMALGAMATED
#include "common.h"
#endif

APE_INTERNAL const src_pos_t src_pos_invalid = { NULL, -1, -1 };
APE_INTERNAL const src_pos_t src_pos_zero = { NULL, 0, 0 };

APE_INTERNAL src_pos_t src_pos_make(const compiled_file_t *file, int line, int column) {
    return (src_pos_t) {
        .file = file,
        .line = line,
        .column = column,
    };
}

char *ape_stringf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int to_write = vsnprintf(NULL, 0, format, args);
    va_end(args);
    va_start(args, format);
    char *res = (char*)ape_malloc(to_write + 1);
    int written = vsprintf(res, format, args);
    (void)written;
    APE_ASSERT(written == to_write);
    va_end(args);
    return res;
}

void ape_log(const char *file, int line, const char *format, ...) {
    char msg[4096];
    int written = snprintf(msg, APE_ARRAY_LEN(msg), "%s:%d: ", file, line);
    (void)written;
    va_list args;
    va_start(args, format);
    int written_msg = vsnprintf(msg + written, APE_ARRAY_LEN(msg) - written, format, args);
    (void)written_msg;
    va_end(args);

    APE_ASSERT(written_msg <= (APE_ARRAY_LEN(msg) - written));

    printf("%s", msg);
}

char* ape_strndup(const char *string, size_t n) {
    char *output_string = (char*)ape_malloc(n + 1);
    if (!output_string) {
        return NULL;
    }
    output_string[n] = '\0';
    memcpy(output_string, string, n);
    return output_string;
}

char* ape_strdup(const char *string) {
    return ape_strndup(string, strlen(string));
}

uint64_t ape_double_to_uint64(double val) {
    union {
        uint64_t val_uint64;
        double val_double;
    } temp = {
        .val_double = val
    };
    return temp.val_uint64;
}

double ape_uint64_to_double(uint64_t val) {
    union {
        uint64_t val_uint64;
        double val_double;
    } temp = {
        .val_uint64 = val
    };
    return temp.val_double;
}
