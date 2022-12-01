

#include <stdarg.h>
#include <sys/stat.h>
#if __has_include(<dirent.h>)
    #include <dirent.h>
#else
    #define CORE_NODIRENT
#endif
#include "ape.h"

static ApeObject_t cfn_file_write(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING, APE_OBJECT_STRING))
    {
        return ape_object_make_null(vm->context);
    }

    const ApeConfig_t* config = vm->config;

    if(!config->fileio.iowriter.fnwritefile)
    {
        return ape_object_make_null(vm->context);
    }

    const char* path = ape_object_string_getdata(args[0]);
    const char* string = ape_object_string_getdata(args[1]);
    ApeSize_t string_len = ape_object_string_getlength(args[1]);

    ApeSize_t written = (ApeSize_t)config->fileio.iowriter.fnwritefile(config->fileio.iowriter.context, path, string, string_len);

    return ape_object_make_number(vm->context, written);
}

static ApeObject_t cfn_file_read(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
    {
        return ape_object_make_null(vm->context);
    }

    const ApeConfig_t* config = vm->config;

    if(!config->fileio.ioreader.fnreadfile)
    {
        return ape_object_make_null(vm->context);
    }

    const char* path = ape_object_string_getdata(args[0]);

    char* contents = config->fileio.ioreader.fnreadfile(config->fileio.ioreader.context, path);
    if(!contents)
    {
        return ape_object_make_null(vm->context);
    }
    ApeObject_t res = ape_object_make_string(vm->context, contents);
    ape_allocator_free(vm->alloc, contents);
    return res;
}

static ApeObject_t cfn_file_isfile(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    const char* path;
    struct stat st;
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
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

static ApeObject_t cfn_file_isdirectory(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    const char* path;
    struct stat st;
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
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

#if defined(__linux__)
static ApeObject_t timespec_to_map(ApeVM_t* vm, struct timespec ts)
{
    ApeObject_t map;
    map = ape_object_make_map(vm->context);
    ape_object_map_setnamednumber(map, "sec", ts.tv_sec);
    ape_object_map_setnamednumber(map, "nsec", ts.tv_nsec);
    return map;
}
#endif

#define for_field(f, val, mapfn, retfn) \
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
            mapfn(map, f, val); \
        } \
    }

#define for_field_string(f, val) \
    for_field(f, val, ape_object_map_setnamedstring, ape_object_make_string)

#define for_field_number(f, val) \
    for_field(f, val, ape_object_map_setnamednumber, ape_object_make_number)

static ApeObject_t cfn_file_stat(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    bool specificfield;
    const char* path;
    const char* field;
    struct stat st;
    ApeObject_t objsecond;
    ApeObject_t objpath;
    ApeObject_t map;
    (void)data;
    field = NULL;
    specificfield = false;
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
        for_field_string("name", path);
        //ape_object_array_pushstring(ary, dent->d_name);
        for_field_string("name", path);
        for_field_string("path", path);
        for_field_number("dev", st.st_dev);
        for_field_number("inode", st.st_ino);
        for_field_number("mode", st.st_mode);
        for_field_number("nlink", st.st_nlink);
        for_field_number("uid", st.st_uid);
        for_field_number("gid", st.st_gid);
        for_field_number("rdev", st.st_rdev);
        for_field_number("size", st.st_size);
        #if defined(__linux__)
        for_field_number("blksize", st.st_blksize);
        for_field_number("blocks", st.st_blocks);
        if(map.handle != NULL)
        {
            ape_object_map_setnamedvalue(map, "atim", timespec_to_map(vm, st.st_atim));
            ape_object_map_setnamedvalue(map, "mtim", timespec_to_map(vm, st.st_mtim));
            ape_object_map_setnamedvalue(map, "ctim", timespec_to_map(vm, st.st_ctim));
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

static ApeObject_t cfn_dir_readdir(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    bool isdir;
    bool isfile;
    const char* path;
    DIR* hnd;
    struct dirent* dent;
    ApeObject_t ary;
    ApeObject_t subm;
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_STRING))
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
        isfile = (dent->d_type == DT_REG);
        isdir = (dent->d_type == DT_DIR);
        subm = ape_object_make_map(vm->context);
        //ape_object_array_pushstring(ary, dent->d_name);
        ape_object_map_setnamedstring(subm, "name", dent->d_name);
        ape_object_map_setnamednumber(subm, "ino", dent->d_ino);
        ape_object_map_setnamednumber(subm, "type", dent->d_type);
        ape_object_map_setnamedbool(subm, "isfile", isfile);
        ape_object_map_setnamedbool(subm, "isdir", isdir);
        ape_object_array_pushvalue(ary, subm);
    }
    closedir(hnd);
    return ary;
}

#endif

void ape_builtins_install_io(ApeVM_t* vm)
{
    static ApeNativeItem_t g_core_filefuncs[]=
    {
        {"read", cfn_file_read},
        {"write", cfn_file_write},
        {"isfile", cfn_file_isfile},
        {"isdirectory", cfn_file_isdirectory},
        {"stat", cfn_file_stat},
        {NULL, NULL},
    };

    static ApeNativeItem_t g_core_dirfuncs[]=
    {
        // filesystem
        #if !defined(CORE_NODIRENT)
        {"read", cfn_dir_readdir},
        #endif
        {NULL, NULL},
    };

    ape_builtins_setup_namespace(vm, "Dir", g_core_dirfuncs);
    ape_builtins_setup_namespace(vm, "File", g_core_filefuncs);
}

