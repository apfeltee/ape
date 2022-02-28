
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

#define PROMPT ">> "

// INTERNAL
static void print_errors(ApeContext_t *ape)
{
    int i;
    int count;
    char *err_str;
    const error_t *err;
    count = ape_errors_count(ape);
    for (i = 0; i < count; i++)
    {
        err = ape_get_error(ape, i);
        err_str = ape_error_serialize(ape, err);
        fprintf(stderr, "%s", err_str);
        ape_free_allocated(ape, err_str);
    }
}

static ApeObject_t exit_repl(ApeContext_t *ape, void *data, int argc, ApeObject_t *args)
{
    bool *exit_repl = (bool*)data;
    *exit_repl = true;
    return ape_object_make_null();
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

void do_repl(ApeContext_t* ape)
{
    size_t len;
    char *line;
    char *object_str;
    ApeObject_t res;
    ape_set_repl_mode(ape, true);
    ape_set_timeout(ape, 100.0);
    while(true)
    {
        line = readline(">> ");
        if(!line || !notjustspace(line))
        {
            continue;
        }
        res = ape_execute(ape, line);
        if (ape_has_errors(ape))
        {
            print_errors(ape);
            free(line);
        }
        else
        {
            add_history(line);
            object_str = ape_object_serialize(ape, res, &len);
            printf("%.*s\n", (int)len, object_str);
            free(object_str);
        }
    }
}
#endif
int main(int argc, char *argv[])
{
    int i;
    ApeContext_t *ape;
    ApeObject_t args_array;
    ape = ape_make();
    ape_set_native_function(ape, "exit", exit_repl, &exit);
    if(argc > 1)
    {
        args_array = ape_object_make_array(ape);
        for (i = 1; i < argc; i++)
        {
            ape_object_add_array_string(args_array, argv[i]);
        }
        ape_set_global_constant(ape, "args", args_array);
        ape_execute_file(ape, argv[1]);
        if(ape_has_errors(ape))
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
    ape_destroy(ape);
    return 0;
}

