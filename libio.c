

#include <stdarg.h>
#include <sys/stat.h>
#if defined(__has_include)
    #if __has_include(<dirent.h>)
        #include <dirent.h>
    #else
        #if !defined(__linux__) && !defined(__unix__)
            #define CORE_NODIRENT
        #endif
    #endif
#endif
#if !defined(CORE_NODIRENT)
    #include <dirent.h>
#endif
#include "inline.h"

#if !defined(S_IFMT)
    #define S_IFMT  00170000
#endif
#if !defined(S_IFSOCK)
    #define S_IFSOCK 0140000
#endif
#if !defined(S_IFLNK)
    #define S_IFLNK  0120000
#endif
#if !defined(S_IFREG)
    #define S_IFREG  0100000
#endif
#if !defined(S_IFBLK)
    #define S_IFBLK  0060000
#endif
#if !defined(S_IFDIR)
    #define S_IFDIR  0040000
#endif
#if !defined(S_IFCHR)
    #define S_IFCHR  0020000
#endif
#if !defined(S_IFIFO)
    #define S_IFIFO  0010000
#endif

#if !defined(DT_DIR)
    #define DT_DIR 4
#endif

#if !defined(DT_REG)
    #define DT_REG 8
#endif

static ApeObject cfn_file_write(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    bool havethismuch;
    const char* string;
    const char* path;
    ApeSize slen;
    ApeSize written;
    ApeSize thismuch;
    const ApeConfig* config;
    ApeArgCheck check;
    (void)data;
    havethismuch = false;
    thismuch = 0;
    ape_args_init(vm, &check, "write", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_STRING) && !ape_args_check(&check, 1, APE_OBJECT_STRING))
    {
        return ape_object_make_null(vm->context);
    }
    if(ape_args_checkoptional(&check, 2, APE_OBJECT_NUMERIC, false))
    {
        havethismuch = true;
        thismuch = ape_object_value_asnumber(args[2]);
    }
    config = vm->config;
    if(!config->fileio.fnwritefile)
    {
        return ape_object_make_null(vm->context);
    }
    path = ape_object_string_getdata(args[0]);
    string = ape_object_string_getdata(args[1]);
    slen = ape_object_string_getlength(args[1]);
    if(havethismuch)
    {
        if(thismuch >= slen)
        {
            havethismuch = false;
        }
    }
    if(havethismuch)
    {
        written = (ApeSize)config->fileio.fnwritefile(vm->context, path, string, thismuch);
    }
    else
    {
        written = (ApeSize)config->fileio.fnwritefile(vm->context, path, string, slen);
    }
    return ape_object_make_floatnumber(vm->context, written);
}

static ApeObject cfn_file_read(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    size_t len;
    char* contents;
    const char* path;
    long int thismuch;
    const ApeConfig* config;
    ApeArgCheck check;
    thismuch = -1;
    (void)data;
    ape_args_init(vm, &check, "read", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_STRING))
    {
        return ape_object_make_null(vm->context);
    }
    if(ape_args_checkoptional(&check, 1, APE_OBJECT_NUMERIC, false))
    {
        thismuch = ape_object_value_asnumber(args[1]);
    }
    config = vm->config;
    if(!config->fileio.fnreadfile)
    {
        return ape_object_make_null(vm->context);
    }
    path = ape_object_string_getdata(args[0]);
    fprintf(stderr, "thismuch=%ld\n", thismuch);
    contents = config->fileio.fnreadfile(vm->context, path, thismuch, &len);
    if(!contents)
    {
        return ape_object_make_null(vm->context);
    }
    ApeObject res = ape_object_make_stringlen(vm->context, contents, len);
    ape_allocator_free(&vm->context->alloc, contents);
    return res;
}

static ApeObject cfn_file_isfile(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    const char* path;
    struct stat st;
    (void)data;
    ApeArgCheck check;
    ape_args_init(vm, &check, "isfile", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_STRING))
    {
        return ape_object_make_bool(vm->context, false);
    }
    path = ape_object_string_getdata(args[0]);
    if(stat(path, &st) != -1)
    {
        if((st.st_mode & S_IFMT) == S_IFREG)
        {
            return ape_object_make_bool(vm->context, true);
        }
    }
    return ape_object_make_bool(vm->context, false);
}

static ApeObject cfn_file_isdirectory(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    const char* path;
    struct stat st;
    (void)data;
    ApeArgCheck check;
    ape_args_init(vm, &check, "isdirectory", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_STRING))
    {
        return ape_object_make_bool(vm->context, false);
    }
    path = ape_object_string_getdata(args[0]);
    if(stat(path, &st) != -1)
    {
        if((st.st_mode & S_IFMT) == S_IFDIR)
        {
            return ape_object_make_bool(vm->context, true);
        }
    }
    return ape_object_make_bool(vm->context, false);
}

/* strict ansi does not have timespec */
#if defined(__linux__) && !defined(APE_CCENV_ANSIMODE)
static ApeObject timespec_to_map(ApeVM* vm, struct timespec ts)
{
    ApeObject map;
    map = ape_object_make_map(vm->context);
    ape_object_map_setnamednumber(vm->context, map, "sec", ts.tv_sec);
    ape_object_map_setnamednumber(vm->context, map, "nsec", ts.tv_nsec);
    return map;
}
#endif

#define for_field(ctx, f, val, mapfn, retfn) \
    { \
        if(specificfield) \
        { \
            if((field != NULL) && (strcmp(field, f) == 0)) \
            { \
                return retfn(vm->context, (val)); \
            } \
        } \
        else \
        { \
            mapfn(ctx, map, f, val); \
        } \
    }

#define for_field_string(ctx, f, val) \
    for_field(ctx, f, val, ape_object_map_setnamedstring, ape_object_make_string)

#define for_field_number(ctx, f, val) \
    for_field(ctx, f, val, ape_object_map_setnamednumber, ape_object_make_floatnumber)

static ApeObject cfn_file_stat(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    bool specificfield;
    const char* path;
    const char* field;
    struct stat st;
    ApeContext* ctx;
    ApeObject objsecond;
    ApeObject objpath;
    ApeObject map;
    (void)data;
    field = NULL;
    specificfield = false;
    ctx = vm->context;
    map.type = APE_OBJECT_NULL;
    map.handle = NULL;
    if((argc == 0) || !ape_object_value_isstring(args[0]))
    {
        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "File.stat expects at least one string argument");
        return ape_object_make_null(vm->context);
    }
    if(argc > 1)
    {
        objsecond = args[1];
        if(!ape_object_value_isstring(objsecond))
        {
            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "File.stat expects second argument to be a string");
            return ape_object_make_null(vm->context);
        }
        specificfield = true;
    }
    objpath = args[0];
    path = ape_object_string_getdata(objpath);
    if(stat(path, &st) != -1)
    {
        if(specificfield)
        {
            field = ape_object_string_getdata(objsecond);
        }
        else
        {
            map = ape_object_make_map(vm->context);
        }
        for_field_string(ctx, "name", path);
        for_field_string(ctx, "name", path);
        for_field_string(ctx, "path", path);
        for_field_number(ctx, "dev", st.st_dev);
        for_field_number(ctx, "inode", st.st_ino);
        for_field_number(ctx, "mode", st.st_mode);
        for_field_number(ctx, "nlink", st.st_nlink);
        for_field_number(ctx, "uid", st.st_uid);
        for_field_number(ctx, "gid", st.st_gid);
        for_field_number(ctx, "rdev", st.st_rdev);
        for_field_number(ctx, "size", st.st_size);
        #if defined(__linux__) && !defined(APE_CCENV_ANSIMODE)
        for_field_number(ctx, "blksize", st.st_blksize);
        for_field_number(ctx, "blocks", st.st_blocks);
        if(map.handle != NULL)
        {
            ape_object_map_setnamedvalue(ctx, map, "atim", timespec_to_map(vm, st.st_atim));
            ape_object_map_setnamedvalue(ctx, map, "mtim", timespec_to_map(vm, st.st_mtim));
            ape_object_map_setnamedvalue(ctx, map, "ctim", timespec_to_map(vm, st.st_ctim));
        }
        #endif
        if(map.handle != NULL)
        {
            return map;
        }
        else
        {
            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "File.stat secondary field '%s' does not exist", field);
        }
        return ape_object_make_null(vm->context);
    }
    return ape_object_make_null(vm->context);
}

#if !defined(CORE_NODIRENT)

static ApeObject cfn_dir_readdir(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    const char* path;
    DIR* hnd;
    struct dirent* dent;
    ApeContext* ctx;
    ApeObject ary;
    ApeObject subm;
    (void)data;
    ctx = vm->context;
    ApeArgCheck check;
    ape_args_init(vm, &check, "readdir", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_STRING))
    {
        return ape_object_make_null(vm->context);
    }
    path = ape_object_string_getdata(args[0]);
    hnd = opendir(path);
    if(hnd == NULL)
    {
        return ape_object_make_null(vm->context);
    }
    ary = ape_object_make_array(vm->context);
    while(true)
    {
        dent = readdir(hnd);
        if(dent == NULL)
        {
            break;
        }
        #if 1
            subm = ape_object_make_string(ctx, dent->d_name);
        #else
        isfile = (dent->d_type == DT_REG);
        isdir = (dent->d_type == DT_DIR);
        subm = ape_object_make_map(vm->context);
        ape_object_map_setnamedstring(ctx, subm, "name", dent->d_name);
        ape_object_map_setnamednumber(ctx, subm, "ino", dent->d_ino);
        ape_object_map_setnamednumber(ctx, subm, "type", dent->d_type);
        ape_object_map_setnamedbool(ctx, subm, "isfile", isfile);
        ape_object_map_setnamedbool(ctx, subm, "isdir", isdir);
        #endif
        ape_object_array_pushvalue(ary, subm);
    }
    closedir(hnd);
    return ary;
}

#endif

void ape_builtins_install_io(ApeVM* vm)
{
    static ApeNativeItem g_core_filefuncs[]=
    {
        {"read", cfn_file_read},
        {"write", cfn_file_write},
        {"isfile", cfn_file_isfile},
        {"isdirectory", cfn_file_isdirectory},
        {"stat", cfn_file_stat},
        {NULL, NULL},
    };

    static ApeNativeItem g_core_dirfuncs[]=
    {
        #if !defined(CORE_NODIRENT)
        {"read", cfn_dir_readdir},
        #endif
        {NULL, NULL},
    };

    ape_builtins_setup_namespace(vm, "Dir", g_core_dirfuncs);
    ape_builtins_setup_namespace(vm, "File", g_core_filefuncs);
}

