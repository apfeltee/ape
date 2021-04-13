#ifndef builtins_h
#define builtins_h

#include <stdint.h>

#ifndef APE_AMALGAMATED
#include "common.h"
#include "object.h"
#endif

typedef struct vm vm_t;

APE_INTERNAL int builtins_count(void);
APE_INTERNAL native_fn builtins_get_fn(int ix);
APE_INTERNAL const char* builtins_get_name(int ix);

#endif /* builtins_h */
