#ifndef common_h
#define common_h

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <float.h>

#ifndef APE_AMALGAMATED
#include "ape.h"
#endif

#define APE_STREQ(a, b) (strcmp((a), (b)) == 0)
#define APE_STRNEQ(a, b, n) (strncmp((a), (b), (n)) == 0)
#define APE_ARRAY_LEN(array) ((int)(sizeof(array) / sizeof(array[0])))
#define APE_DBLEQ(a, b) (fabs((a) - (b)) < DBL_EPSILON)

#ifdef APE_DEBUG
    #define APE_ASSERT(x) assert((x))
    #define APE_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
    #define APE_LOG(...) ape_log(APE_FILENAME, __LINE__, __VA_ARGS__)
#else
    #define APE_ASSERT(x) ((void)0)
    #define APE_LOG(...) ((void)0)
#endif

#ifdef APE_AMALGAMATED
#define COLLECTIONS_AMALGAMATED
#define APE_INTERNAL static
#else
#define APE_INTERNAL
#endif

typedef struct compiled_file compiled_file_t;

typedef struct src_pos {
    const compiled_file_t *file;
    int line;
    int column;
} src_pos_t;

typedef void * (*ape_malloc_fn)(size_t size);
typedef void (*ape_free_fn)(void *ptr);

typedef struct ape_config {
    struct {
        struct {
            ape_stdout_write_fn write;
            void *context;
        } write;
    } stdio;

    struct {
        struct {
            ape_read_file_fn read_file;
            void *context;
        } read_file;

        struct {
            ape_write_file_fn write_file;
            void *context;
        } write_file;
    } fileio;

    int gc_interval;

    bool repl_mode; // allows redefinition of symbols
} ape_config_t;

#ifndef APE_AMALGAMATED
extern const src_pos_t src_pos_invalid;
extern const src_pos_t src_pos_zero;
#endif

APE_INTERNAL src_pos_t src_pos_make(const compiled_file_t *file, int line, int column);
APE_INTERNAL char *ape_stringf(const char *format, ...);
APE_INTERNAL void ape_log(const char *file, int line, const char *format, ...);
APE_INTERNAL char* ape_strndup(const char *string, size_t n);
APE_INTERNAL char* ape_strdup(const char *string);

APE_INTERNAL uint64_t ape_double_to_uint64(double val);
APE_INTERNAL double ape_uint64_to_double(uint64_t val);

extern ape_malloc_fn ape_malloc;
extern ape_free_fn ape_free;

#endif /* common_h */
