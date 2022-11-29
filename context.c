
#include "ape.h"

//implcontext
ApeContext_t* ape_make_context()
{
    return ape_make_contextex(NULL, NULL, NULL);
}

ApeContext_t* ape_make_contextex(ApeMemAllocFunc_t malloc_fn, ApeMemFreeFunc_t free_fn, void* optr)
{
    ApeContext_t* ctx;
    ApeAllocator_t custom_alloc;
    custom_alloc = ape_make_allocator(malloc_fn, free_fn, optr);
    ctx = (ApeContext_t*)ape_allocator_alloc(&custom_alloc, sizeof(ApeContext_t));
    if(!ctx)
    {
        return NULL;
    }
    memset(ctx, 0, sizeof(ApeContext_t));
    // NB. this currently does not work. need to figure this out eventually.
    {
        //ctx->alloc = ape_make_allocator(ape_mem_defaultmalloc, ape_mem_defaultfree, ctx);
        ctx->alloc = ape_make_allocator(NULL, NULL, ctx);
        ctx->custom_allocator = custom_alloc;
    }
    ape_context_setdefaultconfig(ctx);
    ape_errorlist_initerrors(&ctx->errors);
    ctx->mem = ape_make_gcmem(ctx);
    if(!ctx->mem)
    {
        goto err;
    }
    ctx->files = ape_make_ptrarray(ctx);
    if(!ctx->files)
    {
        goto err;
    }
    ctx->globalstore = ape_make_globalstore(ctx, ctx->mem);
    if(!ctx->globalstore)
    {
        goto err;
    }
    ctx->compiler = ape_compiler_make(ctx, &ctx->config, ctx->mem, &ctx->errors, ctx->files, ctx->globalstore);
    if(!ctx->compiler)
    {
        goto err;
    }
    ctx->vm = ape_vm_make(ctx, &ctx->config, ctx->mem, &ctx->errors, ctx->globalstore);
    if(!ctx->vm)
    {
        goto err;
    }
    ctx->vm->context = ctx;
    ape_builtins_install(ctx->vm);
    return ctx;
err:
    ape_context_deinit(ctx);
    ape_allocator_free(&custom_alloc, ctx);
    return NULL;
}

void ape_context_destroy(ApeContext_t* ctx)
{
    if(!ctx)
    {
        return;
    }
    ape_context_deinit(ctx);
    ApeAllocator_t alloc = ctx->alloc;
    ape_allocator_free(&alloc, ctx);
}

void ape_context_freeallocated(ApeContext_t* ctx, void* ptr)
{
    ape_allocator_free(&ctx->alloc, ptr);
}

void ape_context_setreplmode(ApeContext_t* ctx, bool enabled)
{
    ctx->config.replmode = enabled;
}

bool ape_context_settimeout(ApeContext_t* ctx, ApeFloat_t max_execution_time_ms)
{
    if(!ape_util_timersupported())
    {
        ctx->config.max_execution_time_ms = 0;
        ctx->config.max_execution_time_set = false;
        return false;
    }
    if(max_execution_time_ms >= 0)
    {
        ctx->config.max_execution_time_ms = max_execution_time_ms;
        ctx->config.max_execution_time_set = true;
    }
    else
    {
        ctx->config.max_execution_time_ms = 0;
        ctx->config.max_execution_time_set = false;
    }
    return true;
}

void ape_context_setstdoutwrite(ApeContext_t* ctx, ApeIOStdoutWriteFunc_t stdout_write, void* context)
{
    ctx->config.stdio.write.write = stdout_write;
    ctx->config.stdio.write.context = context;
}

void ape_context_setfilewrite(ApeContext_t* ctx, ApeIOWriteFunc_t file_write, void* context)
{
    ctx->config.fileio.write_file.write_file = file_write;
    ctx->config.fileio.write_file.context = context;
}

void ape_context_setfileread(ApeContext_t* ctx, ApeIOReadFunc_t file_read, void* context)
{
    ctx->config.fileio.read_file.read_file = file_read;
    ctx->config.fileio.read_file.context = context;
}

void ape_context_dumpast(ApeContext_t* ctx, ApePtrArray_t* statements)
{
    ApeWriter_t* strbuf;
    strbuf = ape_make_writerio(ctx, stderr, false, true);
    fprintf(stderr, "parsed AST:\n");
    ape_tostring_stmtlist(statements, strbuf);
    fprintf(stderr, "\n");
    ape_writer_destroy(strbuf);
}

void ape_context_dumpbytecode(ApeContext_t* ctx, ApeCompResult_t* cres)
{
    ApeWriter_t* strbuf;
    strbuf = ape_make_writerio(ctx, stderr, false, true);
    fprintf(stderr, "bytecode:\n");
    ape_tostring_compresult(cres, strbuf);
    fprintf(stderr, "\n");
    ape_writer_destroy(strbuf);
}

ApeObject_t ape_context_executesource(ApeContext_t* ctx, const char* code, bool alsoreset)
{
    bool ok;
    ApeObject_t objres;
    ApeCompResult_t* cres;
    if(alsoreset)
    {
        ape_context_resetstate(ctx);
    }
    cres = ape_compiler_compilesource(ctx->compiler, code);
    if(!cres || ape_errorlist_count(&ctx->errors) > 0)
    {
        goto err;
    }
    if(ctx->config.dumpbytecode)
    {
        ape_context_dumpbytecode(ctx, cres);
    }
    ok = ape_vm_run(ctx->vm, cres, ape_compiler_getconstants(ctx->compiler));
    if(!ok || ape_errorlist_count(&ctx->errors) > 0)
    {
        goto err;
    }
    //APE_ASSERT(ctx->vm->sp == 0);
    objres = ape_vm_getlastpopped(ctx->vm);
    if(ape_object_value_type(objres) == APE_OBJECT_NONE)
    {
        goto err;
    }
    ape_compresult_destroy(cres);
    return objres;

err:
    ape_compresult_destroy(cres);
    return ape_object_make_null(ctx);
}

ApeObject_t ape_context_executefile(ApeContext_t* ctx, const char* path)
{
    bool ok;
    ApeObject_t objres;
    ApeCompResult_t* cres;
    ape_context_resetstate(ctx);
    cres = ape_compiler_compilefile(ctx->compiler, path);
    if(!cres || ape_errorlist_count(&ctx->errors) > 0)
    {
        goto err;
    }
    if(ctx->config.dumpbytecode)
    {
        ape_context_dumpbytecode(ctx, cres);
    }
    ok = ape_vm_run(ctx->vm, cres, ape_compiler_getconstants(ctx->compiler));
    if(!ok || ape_errorlist_count(&ctx->errors) > 0)
    {
        goto err;
    }
    APE_ASSERT(ctx->vm->sp == 0);
    objres = ape_vm_getlastpopped(ctx->vm);
    if(ape_object_value_type(objres) == APE_OBJECT_NONE)
    {
        goto err;
    }
    ape_compresult_destroy(cres);
    return objres;
err:
    ape_compresult_destroy(cres);
    return ape_object_make_null(ctx);
}

bool ape_context_haserrors(ApeContext_t* ctx)
{
    return ape_context_errorcount(ctx) > 0;
}

ApeSize_t ape_context_errorcount(ApeContext_t* ctx)
{
    return ape_errorlist_count(&ctx->errors);
}

void ape_context_clearerrors(ApeContext_t* ctx)
{
    ape_errorlist_clear(&ctx->errors);
}

ApeError_t* ape_context_geterror(ApeContext_t* ctx, int index)
{
    return ape_errorlist_getat(&ctx->errors, index);
}

bool ape_context_setnativefunction(ApeContext_t* ctx, const char* name, ApeWrappedNativeFunc_t fn, void* data)
{
    ApeObject_t obj;
    obj = ape_context_makenamednative(ctx, name, fn, data);
    if(ape_object_value_isnull(obj))
    {
        return false;
    }
    return ape_context_setglobal(ctx, name, obj);
}

bool ape_context_setglobal(ApeContext_t* ctx, const char* name, ApeObject_t obj)
{
    return ape_globalstore_set(ctx->globalstore, name, obj);
}

char* ape_context_errortostring(ApeContext_t* ctx, ApeError_t* err)
{
    const char* type_str = ape_error_gettypestring(err);
    const char* filename = ape_error_getfile(err);
    const char* line = ape_error_getsource(err);
    int line_num = ape_error_getline(err);
    int col_num = ape_error_getcolumn(err);
    ApeWriter_t* buf = ape_make_writer(ctx);
    if(!buf)
    {
        return NULL;
    }
    if(line)
    {
        ape_writer_append(buf, line);
        ape_writer_append(buf, "\n");
        if(col_num >= 0)
        {
            for(int j = 0; j < (col_num - 1); j++)
            {
                ape_writer_append(buf, " ");
            }
            ape_writer_append(buf, "^\n");
        }
    }
    ape_writer_appendf(buf, "%s ERROR in \"%s\" on %d:%d: %s\n", type_str, filename, line_num, col_num, ape_error_getmessage(err));
    ApeTraceback_t* traceback = ape_error_gettraceback(err);
    if(traceback)
    {
        ape_writer_appendf(buf, "traceback:\n");
        ape_tostring_traceback(ape_error_gettraceback(err), buf);
    }
    if(ape_writer_failed(buf))
    {
        ape_writer_destroy(buf);
        return NULL;
    }
    return ape_writer_getstringanddestroy(buf);
}

void ape_context_deinit(ApeContext_t* ctx)
{
    ape_vm_destroy(ctx->vm);
    ape_compiler_destroy(ctx->compiler);
    ape_globalstore_destroy(ctx->globalstore);
    ape_gcmem_destroy(ctx->mem);
    ape_ptrarray_destroywithitems(ctx->files, (ApeDataCallback_t)compiled_file_destroy);
    ape_errorlist_destroy(&ctx->errors);
}

static ApeObject_t ape_context_wrapnativefunc(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeObject_t objres;
    ApeNativeFuncWrapper_t* wrapper;
    (void)vm;
    wrapper = (ApeNativeFuncWrapper_t*)data;
    wrapper->context = vm->context;
    //APE_ASSERT(vm == wrapper->context->vm);
    objres = wrapper->wrapped_funcptr(wrapper->context, wrapper->data, argc, (ApeObject_t*)args);
    if(ape_context_haserrors(wrapper->context))
    {
        return ape_object_make_null(vm->context);
    }
    return objres;
}

ApeObject_t ape_context_makenamednative(ApeContext_t* ctx, const char* name, ApeWrappedNativeFunc_t fn, void* data)
{
    ApeObject_t wrobj;
    ApeNativeFuncWrapper_t wrapper;
    memset(&wrapper, 0, sizeof(ApeNativeFuncWrapper_t));
    wrapper.wrapped_funcptr = fn;
    wrapper.context = ctx;
    wrapper.context->vm = ctx->vm;
    wrapper.data = data;
    wrobj = ape_object_make_nativefuncmemory(ctx, name, ape_context_wrapnativefunc, &wrapper, sizeof(wrapper));
    if(ape_object_value_isnull(wrobj))
    {
        return ape_object_make_null(ctx);
    }
    return wrobj;
}

void ape_context_resetstate(ApeContext_t* ctx)
{
    ape_context_clearerrors(ctx);
    ape_vm_reset(ctx->vm);
}

void ape_context_setdefaultconfig(ApeContext_t* ctx)
{
    memset(&ctx->config, 0, sizeof(ApeConfig_t));
    ctx->config.dumpbytecode = false;
    ctx->config.dumpast = false;
    ctx->config.dumpstack = false;
    ape_context_setreplmode(ctx, false);
    ape_context_settimeout(ctx, -1);
    ape_context_setfileread(ctx, ape_util_default_readfile, ctx);
    ape_context_setfilewrite(ctx, ape_util_default_writefile, ctx);
    ape_context_setstdoutwrite(ctx, ape_util_default_stdoutwrite, ctx);
}

