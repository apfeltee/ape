
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#if __has_include(<readline/readline.h>)
#include <readline/readline.h>
#include <readline/history.h>
#else
    #define NO_READLINE
#endif

#include "ape.h"

// endtypes

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
    bool debug;
    int n_paths;
    const char** paths;
    const char* codeline;
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

static void print_errors(ApeContext_t *ape)
{
    int i;
    int count;
    char *err_str;
    const ApeError_t *err;
    count = context_errorcount(ape);
    for (i = 0; i < count; i++)
    {
        err = context_get_error(ape, i);
        err_str = context_error_tostring(ape, err);
        fprintf(stderr, "%s", err_str);
        context_free_allocated(ape, err_str);
    }
}

static ApeObject_t exit_repl(ApeContext_t *ape, void *data, int argc, ApeObject_t *args)
{
    bool *exit_repl;
    (void)ape;
    (void)argc;
    (void)args;
    exit_repl = (bool*)data;
    *exit_repl = true;
    return object_make_null();
}

#if !defined(NO_READLINE)
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

static void do_repl(ApeContext_t* ape)
{
    size_t len;
    char *line;
    char *object_str;
    ApeObject_t res;
    context_set_replmode(ape, true);
    context_set_timeout(ape, 100.0);
    while(true)
    {
        line = readline(">> ");
        if(!line || !notjustspace(line))
        {
            continue;
        }
        add_history(line);
        res = context_executesource(ape, line);
        if (context_has_errors(ape))
        {
            print_errors(ape);
            free(line);
        }
        else
        {
            object_str = object_value_serialize(&ape->alloc, res, &len);
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
        "\n"
    );
}

static bool parse_options(Options_t* opts, Flag_t* flags, int fcnt)
{
    int i;
    opts->codeline = NULL;
    opts->package = NULL;
    opts->filename = NULL;
    opts->debug = false;
    for(i=0; i<fcnt; i++)
    {
        switch(flags[i].flag)
        {
            case 'h':
                {
                    show_usage();
                }
                break;
            case 'e':
                {
                    if(flags[i].value == NULL)
                    {
                        fprintf(stderr, "flag '-e' expects a value.\n");
                        return false;
                    }
                    opts->codeline = flags[i].value;
                }
                break;
            case 'd':
                {
                    opts->debug = true;
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

int main(int argc, char *argv[])
{
    int i;
    bool replexit;
    const char* filename;
    FlagContext_t fx;
    Options_t opts;
    ApeContext_t *ape;
    ApeObject_t args_array;
    replexit = false;
    populate_flags(argc, 1, argv, "epI", &fx);
    ape = context_make();
    if(!parse_options(&opts, fx.flags, fx.fcnt))
    {
        fprintf(stderr, "failed to process command line flags.\n");
        return 1;
    }
    context_set_nativefunction(ape, "exit", exit_repl, &replexit);
    if((fx.poscnt > 0) || (opts.codeline != NULL))
    {
        args_array = object_make_array(ape->mem);
        for(i=0; i<fx.poscnt; i++)
        {
            object_array_pushstring(args_array, fx.positional[i]);
        }
        context_set_global(ape, "args", args_array);
        if(opts.codeline)
        {
            context_executesource(ape, opts.codeline);
        }
        else
        {
            filename = fx.positional[0];
            context_executefile(ape, filename);
        }
        if(context_has_errors(ape))
        {
            print_errors(ape);
        }
    }
    else
    {
        #if !defined(NO_READLINE)
            do_repl(ape);
        #else
            fprintf(stderr, "no repl support compiled in\n");
        #endif
    }
    context_destroy(ape);
    return 0;
}

