
#pragma once

#if defined(__linux__)
    #define APE_LINUX
    #define APE_POSIX
#elif defined(_WIN32)
    #define APE_WINDOWS
#elif(defined(__APPLE__) && defined(__MACH__))
    #define APE_APPLE
    #define APE_POSIX
#elif defined(__EMSCRIPTEN__)
    #define APE_EMSCRIPTEN
#endif

#if defined(__unix__)
    #include <unistd.h>
    #if defined(_POSIX_VERSION)
        #define APE_POSIX
    #endif
#endif

#if defined(APE_POSIX)
    #include <sys/time.h>
#elif defined(APE_WINDOWS)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#elif defined(APE_EMSCRIPTEN)
    #include <emscripten/emscripten.h>
#endif

#include "ape.h"


#define APE_IMPL_VERSION_MAJOR 0
#define APE_IMPL_VERSION_MINOR 14
#define APE_IMPL_VERSION_PATCH 0

#if(APE_VERSION_MAJOR != APE_IMPL_VERSION_MAJOR) || (APE_VERSION_MINOR != APE_IMPL_VERSION_MINOR) \
|| (APE_VERSION_PATCH != APE_IMPL_VERSION_PATCH)
    #error "Version mismatch"
#endif


#ifdef COLLECTIONS_DEBUG
    #include <assert.h>
    #define COLLECTIONS_ASSERT(x) assert(x)
#else
    #define COLLECTIONS_ASSERT(x)
#endif




