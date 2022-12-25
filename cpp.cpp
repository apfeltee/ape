
/*
* this file is meant to cross-reference against C++ compilers that introduce subtle, but
* annoying issues.
* specifically, Visual Studio.
* also, to check that it compiles as C++, for standards-conforming compilers, such
* as GCC, Clang, etc.
*/

#include "ape.h"
#include "inline.h"
#include "builtins.c"
#include "cclex.c"
#include "ccom.c"
#include "ccopt.c"
#include "ccparse.c"
#include "context.c"
#include "error.c"
#include "libarray.c"
#include "libfunction.c"
#include "libio.c"
#include "libmap.c"
#include "libmath.c"
#include "libmod.c"
#include "libobject.c"
#include "libpseudo.c"
#include "libstring.c"
#include "main.c"
#include "memgc.c"
#include "mempool.c"
#include "tostring.c"
#include "util.c"
#include "vm.c"
#include "writer.c"
