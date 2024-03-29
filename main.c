
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#if defined(__has_include)
    #if __has_include(<readline/readline.h>)
        #define HAVE_READLINE
    #endif
#endif

#if defined(HAVE_READLINE)
    #include <readline/readline.h>
    #include <readline/history.h>
#endif
#include "ape.h"

enum
{
    MAX_RESTARGS = 1024,
    MAX_OPTS = 1024,
};

typedef struct Flag_t Flag_t;
typedef struct FlagContext_t FlagContext_t;

struct Flag_t
{
    char flag;
    char* value;
};

struct FlagContext_t
{
    int nargc;
    int fcnt;
    int poscnt;
    char* positional[MAX_RESTARGS + 1];
    Flag_t flags[MAX_OPTS + 1];
};

typedef struct Options_t Options_t;

struct Options_t
{
    const char* package;
    const char* filename;
    const char* debugmode;
    bool printast;
    bool printbytecode;
    bool alsorun;
    int n_paths;
    const char** paths;
    const char* codeline;
    const char* memdbglogfile;
};


static bool populate_flags(int argc, int begin, char** argv, const char* expectvalue, FlagContext_t* fx)
{
    int i;
    int nextch;
    int psidx;
    int flidx;
    char* arg;
    char* nextarg;
    psidx = 0;
    flidx = 0;
    fx->fcnt = 0;
    fx->poscnt = 0;
    for(i=begin; i<argc; i++)
    {
        arg = argv[i];
        nextarg = NULL;
        if((i+1) < argc)
        {
            nextarg = argv[i+1];
        }
        if(arg[0] == '-')
        {
            fx->flags[flidx].flag = arg[1];
            fx->flags[flidx].value = NULL;
            if(strchr(expectvalue, arg[1]) != NULL)
            {
                nextch = arg[2];
                /* -e "somecode(...)" */
                /* -e is followed by text: -e"somecode(...)" */
                if(nextch != 0)
                {
                    fx->flags[flidx].value = arg + 2;
                }
                else if(nextarg != NULL)
                {
                    if(nextarg[0] != '-')
                    {
                        fx->flags[flidx].value = nextarg;
                        i++;
                    }
                }
                else
                {
                    fx->flags[flidx].value = NULL;
                }
            }
            flidx++;
        }
        else
        {
            fx->positional[psidx] = arg;
            psidx++;
        }
    }
    fx->fcnt = flidx;
    fx->poscnt = psidx;
    fx->nargc = i;
    return true;
}

static void print_errors(ApeContext* ctx)
{
    int i;
    int count;
    char* err_str;
    ApeError* err;
    count = ape_context_errorcount(ctx);
    for (i = 0; i < count; i++)
    {
        err = ape_context_geterror(ctx, i);
        err_str = ape_context_errortostring(ctx, err);
        fprintf(stderr, "%s", err_str);
        ape_context_freeallocated(ctx, err_str);
    }
}

static ApeObject exit_repl(ApeContext* ctx, void* data, ApeSize argc, ApeObject* args)
{
    bool* exit_repl;
    (void)ctx;
    (void)argc;
    (void)args;
    exit_repl = (bool*)data;
    *exit_repl = true;
    return ape_object_make_null(ctx);
}

#if defined(HAVE_READLINE)
static bool notjustspace(const char* line)
{
    int c;
    size_t i;
    for(i=0; (c = line[i]) != 0; i++)
    {
        if(!isspace(c))
        {
            return true;
        }
    }
    return false;
}

static void do_repl(ApeContext* ctx)
{
    size_t len;
    char* line;
    char* object_str;
    ApeObject res;
    ctx->config.replmode = true;
    ape_context_settimeout(ctx, 100.0);
    while(true)
    {
        line = readline(">> ");
        if(!line || !notjustspace(line))
        {
            continue;
        }
        len = strlen(line);
        add_history(line);
        res = ape_context_executesource(ctx, line, len, true);
        if (ape_context_haserrors(ctx))
        {
            print_errors(ctx);
            free(line);
        }
        else
        {
            object_str = ape_object_value_serialize(ctx, res, &len);
            printf("%.*s\n", (int)len, object_str);
            free(object_str);
        }
    }
}
#endif

static void show_usage()
{
    printf("known options:\n");
    printf(
        "  -h          this help.\n"
        "  -e <code>   evaluate <code> and exit.\n"
        "  -p <name>   load package <name>\n"
        "  -d<what>    dump <what>, where <what>:\n"
        "              'all', '*': everything\n"
        "              'ast': print ast\n"
        "              'bc': print bytecode\n"
        "  -t          print type sizes (for debugging)\n"
        "\n"
    );
}

void print_types()
{
    #define typsz(t) \
        { \
            printf("  %-10d %d %s\n", (int)(sizeof(t)), (int)(sizeof(t*)), #t); \
        }
    #include "ptypes.inc"
    #undef typsz
}

static bool parse_options(Options_t* opts, Flag_t* flags, int fcnt)
{
    int i;
    opts->codeline = NULL;
    opts->package = NULL;
    opts->filename = NULL;
    opts->debugmode = NULL;
    opts->alsorun = false;
    opts->printast = false;
    opts->printbytecode = false;
    opts->memdbglogfile = NULL;
    for(i=0; i<fcnt; i++)
    {
        switch(flags[i].flag)
        {
            case 'h':
                {
                    show_usage();
                    return false;
                }
                break;
            case 't':
                {
                    print_types();
                    return false;
                }
                break;
            case 'x':
                {
                    opts->alsorun = true;
                }
                break;
            case 'e':
                {
                    if(flags[i].value == NULL)
                    {
                        fprintf(stderr, "flag '-e' expects a string\n");
                        return false;
                    }
                    opts->codeline = flags[i].value;
                }
                break;
            case 'd':
                {
                    if(flags[i].value == NULL)
                    {
                        fprintf(stderr, "flag '-d' expects a value. run '-h' for possible values\n");
                        return false;
                    }
                    opts->debugmode = flags[i].value;
                }
                break;
            case 'a':
                {
                    opts->printast = true;
                }
                break;
            case 'b':
                {
                    opts->printbytecode = true;
                }
                break;
            case 'm':
                {
                    if(flags[i].value == NULL)
                    {
                        fprintf(stderr, "flag '-m' expects a value.\n");
                        return false;
                    }
                    opts->memdbglogfile = flags[i].value;
                }
                break;
            case 'p':
                {
                    if(flags[i].value == NULL)
                    {
                        fprintf(stderr, "flag '-p' expects a value.\n");
                        return false;
                    }
                    opts->package = flags[i].value;
                }
                break;
            default:
                break;
        }
    }
    return true;
}

static const char* strings_printast[] =
{
    "ast", "printast", "dumpast",
    NULL
};

static const char* strings_printbytecode[] =
{
    "bc", "bytecode", "printbc", "printbytecode", "dumpbc", "dumpbytecode",
    NULL
};


bool casecmplen(const char* s1, size_t l1, const char* s2, size_t l2)
{
    int i;
    int c1;
    int c2;
    if(l1 != l2)
    {
        return false;
    }
    for(i=0; (s1[i] != 0) && (s2[i] != 0); i++)
    {
        if(s1[i] )
        c1 = tolower(s1[i]);
        c2 = tolower(s2[i]);
        if(c1 != c2)
        {
            return false;
        }
    }
    return true;
}

bool casecmp(const char* s1, const char* s2)
{
    return casecmplen(s1, strlen(s1), s2, strlen(s2));
}

bool icontains(const char** strlist, const char* findme)
{
    size_t i;
    for(i=0; strlist[i] != NULL; i++)
    {
        if(casecmp(strlist[i], findme))
        {
            return true;
        }
    }
    return false;
}

int main(int argc, char* argv[])
{
    int i;
    bool cmdfailed;
    bool replexit;
    const char* dm;
    const char* filename;
    FlagContext_t fx;
    Options_t opts;
    ApeContext* ctx;
    ApeObject args_array;
    replexit = false;
    cmdfailed = false;
    populate_flags(argc, 1, argv, "epIdm", &fx);
    ctx = ape_make_context();
    if(!parse_options(&opts, fx.flags, fx.fcnt))
    {
        cmdfailed = true;
    }
    else
    {
        if(opts.memdbglogfile != NULL)
        {
            ape_allocator_setdebugfile(&ctx->alloc, opts.memdbglogfile);
        }
        ctx->config.runafterdump = opts.alsorun;
        ape_context_setnativefunction(ctx, "exit", exit_repl, &replexit);
        if(opts.printast)
        {
            ctx->config.dumpast = true;
        }
        if(opts.printbytecode)
        {
            ctx->config.dumpbytecode = true;
        }
        if(opts.debugmode != NULL)
        {
            dm = opts.debugmode;
            if(casecmp(dm, "all") || casecmp(dm, "*"))
            {
                ctx->config.dumpast = true;
                ctx->config.dumpbytecode = true;
            }
            else if(icontains(strings_printast, dm))
            {
                ctx->config.dumpast = true;
            }
            else if(icontains(strings_printbytecode, dm))
            {
                ctx->config.dumpbytecode = true;            
            }
            else
            {
                fprintf(stderr, "unrecognized dump mode '%s'\n", dm);
                cmdfailed = true;
            }
        }
    }
    if(!cmdfailed)
    {
        if((fx.poscnt > 0) || (opts.codeline != NULL))
        {
            args_array = ape_object_make_array(ctx);
            for(i=0; i<fx.poscnt; i++)
            {
                ape_object_array_pushstring(ctx, args_array, fx.positional[i]);
            }
            ape_context_setglobal(ctx, "args", args_array);
            if(opts.codeline)
            {
                ape_context_executesource(ctx, opts.codeline, strlen(opts.codeline), true);
            }
            else
            {
                filename = fx.positional[0];
                ape_context_executefile(ctx, filename);
            }
            if(ape_context_haserrors(ctx))
            {
                print_errors(ctx);
            }
        }
        else
        {
            #if defined(HAVE_READLINE)
                do_repl(ctx);
            #else
                fprintf(stderr, "no repl support compiled in\n");
            #endif
        }
    }
    ape_context_destroy(ctx);
    if(cmdfailed)
    {
        return 1;
    }
    return 0;
}

