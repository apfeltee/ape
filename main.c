
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <readline/readline.h>
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

static ape_object_t exit_repl(ApeContext_t *ape, void *data, int argc, ape_object_t *args)
{
    bool *exit_repl = (bool*)data;
    *exit_repl = true;
    return ape_object_make_null();
}

int main(int argc, char *argv[])
{
    int i;
    char *line;
    char *object_str;
    ApeContext_t *ape;
    ape_object_t res;
    ape_object_t args_array;
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
        ape_set_repl_mode(ape, true);
        ape_set_timeout(ape, 100.0);
        while(true)
        {
            line = readline(">> ");
            if (!line)
            {
                continue;
            }
            res = ape_execute(ape, line);
            if (ape_has_errors(ape))
            {
                print_errors(ape);
                free(line);
                continue;
            }
            object_str = ape_object_serialize(ape, res);
            puts(object_str);
            free(object_str);
        }
    }
    ape_destroy(ape);
    return 0;
}

