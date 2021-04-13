
#include <stdio.h>
#include "ape.h"

static void print_errors(ape_t *ape, const char *line)
{
    int i;
    int count;
    char* err_str;
    const ape_error_t* err;
    count = ape_errors_count(ape);
    for (i = 0; i < count; i++) {
        err = ape_get_error(ape, i);
        err_str = ape_error_serialize(err);
        puts(err_str);
        free(err_str);
    }
}

int main(int argc, char* argv[])
{
    const char* filename;
    ape_t *ape;
    if(argc > 1)
    {
        filename = argv[1];
        ape = ape_make();
        ape_object_t res = ape_execute_file(ape, filename);
        if (ape_has_errors(ape))
        {
            print_errors(ape, filename);
        }
        ape_destroy(ape);
    }
    else
    {
        fprintf(stderr, "missing filename\n");
        return 1;
    }
    return 0;
}

