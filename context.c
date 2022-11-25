
#include "ape.h"

//implcontext
ApeContext_t* context_make()
{
    return context_make_ex(NULL, NULL, NULL);
}

ApeContext_t* context_make_ex(ApeMemAllocFunc_t malloc_fn, ApeMemFreeFunc_t free_fn, void* optr)
{
    ApeContext_t* ctx;
    ApeAllocator_t custom_alloc;


    custom_alloc = allocator_make(malloc_fn, free_fn, optr);

    ctx = (ApeContext_t*)allocator_malloc(&custom_alloc, sizeof(ApeContext_t));

    if(!ctx)
    {
        return NULL;
    }

    memset(ctx, 0, sizeof(ApeContext_t));

    {

        //ctx->alloc = allocator_make(util_default_malloc, util_default_free, ctx);
        ctx->alloc = allocator_make(NULL, NULL, ctx);

        ctx->custom_allocator = custom_alloc;

    }

    context_setdefaultconfig(ctx);
    errorlist_initerrors(&ctx->errors);
    ctx->mem = gcmem_make(ctx);
    if(!ctx->mem)
    {
        goto err;
    }
    ctx->files = ptrarray_make(ctx);
    if(!ctx->files)
    {
        goto err;
    }
    ctx->global_store = global_store_make(ctx, ctx->mem);
    if(!ctx->global_store)
    {
        goto err;
    }
    ctx->compiler = compiler_make(ctx, &ctx->config, ctx->mem, &ctx->errors, ctx->files, ctx->global_store);
    if(!ctx->compiler)
    {
        goto err;
    }
    ctx->vm = vm_make(ctx, &ctx->config, ctx->mem, &ctx->errors, ctx->global_store);
    if(!ctx->vm)
    {
        goto err;
    }
    ctx->vm->context = ctx;
    builtins_install(ctx->vm);
    return ctx;
err:
    context_deinit(ctx);
    allocator_free(&custom_alloc, ctx);
    return NULL;
}

void context_destroy(ApeContext_t* ctx)
{
    if(!ctx)
    {
        return;
    }
    context_deinit(ctx);
    ApeAllocator_t alloc = ctx->alloc;
    allocator_free(&alloc, ctx);
}

void context_freeallocated(ApeContext_t* ctx, void* ptr)
{
    allocator_free(&ctx->alloc, ptr);
}

void context_setreplmode(ApeContext_t* ctx, bool enabled)
{
    ctx->config.repl_mode = enabled;
}

bool context_settimeout(ApeContext_t* ctx, ApeFloat_t max_execution_time_ms)
{
    if(!util_timer_supported())
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

void context_setstdoutwrite(ApeContext_t* ctx, ApeIOStdoutWriteFunc_t stdout_write, void* context)
{
    ctx->config.stdio.write.write = stdout_write;
    ctx->config.stdio.write.context = context;
}

void context_setfilewrite(ApeContext_t* ctx, ApeIOWriteFunc_t file_write, void* context)
{
    ctx->config.fileio.write_file.write_file = file_write;
    ctx->config.fileio.write_file.context = context;
}

void context_setfileread(ApeContext_t* ctx, ApeIOReadFunc_t file_read, void* context)
{
    ctx->config.fileio.read_file.read_file = file_read;
    ctx->config.fileio.read_file.context = context;
}

void context_dumpbytecode(ApeContext_t* ctx, ApeCompilationResult_t* cres)
{
    ApeWriter_t* strbuf;
    strbuf = writer_makeio(ctx, stderr, false, true);
    compresult_tostring(cres, strbuf);
    writer_destroy(strbuf);
}

ApeObject_t context_executesource(ApeContext_t* ctx, const char* code, bool alsoreset)
{
    bool ok;
    ApeObject_t objres;
    ApeCompilationResult_t* cres;
    if(alsoreset)
    {
        context_resetstate(ctx);
    }
    cres = compiler_compile(ctx->compiler, code);
    if(!cres || errorlist_count(&ctx->errors) > 0)
    {
        goto err;
    }
    if(ctx->config.dumpbytecode)
    {
        context_dumpbytecode(ctx, cres);
    }
    ok = vm_run(ctx->vm, cres, compiler_get_constants(ctx->compiler));
    if(!ok || errorlist_count(&ctx->errors) > 0)
    {
        goto err;
    }
    APE_ASSERT(ctx->vm->sp == 0);
    objres = vm_get_last_popped(ctx->vm);
    if(object_value_type(objres) == APE_OBJECT_NONE)
    {
        goto err;
    }
    compilation_result_destroy(cres);
    return objres;

err:
    compilation_result_destroy(cres);
    return object_make_null(ctx);
}

ApeObject_t context_executefile(ApeContext_t* ctx, const char* path)
{
    bool ok;
    ApeObject_t objres;
    ApeCompilationResult_t* cres;
    context_resetstate(ctx);
    cres = compiler_compile_file(ctx->compiler, path);
    if(!cres || errorlist_count(&ctx->errors) > 0)
    {
        goto err;
    }
    if(ctx->config.dumpbytecode)
    {
        context_dumpbytecode(ctx, cres);
    }
    ok = vm_run(ctx->vm, cres, compiler_get_constants(ctx->compiler));
    if(!ok || errorlist_count(&ctx->errors) > 0)
    {
        goto err;
    }
    APE_ASSERT(ctx->vm->sp == 0);
    objres = vm_get_last_popped(ctx->vm);
    if(object_value_type(objres) == APE_OBJECT_NONE)
    {
        goto err;
    }
    compilation_result_destroy(cres);

    return objres;

err:
    compilation_result_destroy(cres);
    return object_make_null(ctx);
}

bool context_haserrors(ApeContext_t* ctx)
{
    return context_errorcount(ctx) > 0;
}

ApeSize_t context_errorcount(ApeContext_t* ctx)
{
    return errorlist_count(&ctx->errors);
}

void context_clearerrors(ApeContext_t* ctx)
{
    errorlist_clear(&ctx->errors);
}

ApeError_t* context_geterror(ApeContext_t* ctx, int index)
{
    return errorlist_getat(&ctx->errors, index);
}

bool context_setnativefunction(ApeContext_t* ctx, const char* name, ApeWrappedNativeFunc_t fn, void* data)
{
    ApeObject_t obj;
    obj = context_makenamednative(ctx, name, fn, data);
    if(object_value_isnull(obj))
    {
        return false;
    }
    return context_setglobal(ctx, name, obj);
}

bool context_setglobal(ApeContext_t* ctx, const char* name, ApeObject_t obj)
{
    return global_store_set(ctx->global_store, name, obj);
}

char* context_errortostring(ApeContext_t* ctx, ApeError_t* err)
{
    const char* type_str = error_gettypestring(err);
    const char* filename = error_getfile(err);
    const char* line = error_getsource(err);
    int line_num = error_getline(err);
    int col_num = error_getcolumn(err);
    ApeWriter_t* buf = writer_make(ctx);
    if(!buf)
    {
        return NULL;
    }
    if(line)
    {
        writer_append(buf, line);
        writer_append(buf, "\n");
        if(col_num >= 0)
        {
            for(int j = 0; j < (col_num - 1); j++)
            {
                writer_append(buf, " ");
            }
            writer_append(buf, "^\n");
        }
    }
    writer_appendf(buf, "%s ERROR in \"%s\" on %d:%d: %s\n", type_str, filename, line_num, col_num, error_getmessage(err));
    ApeTraceback_t* traceback = error_gettraceback(err);
    if(traceback)
    {
        writer_appendf(buf, "traceback:\n");
        traceback_tostring(error_gettraceback(err), buf);
    }
    if(writer_failed(buf))
    {
        writer_destroy(buf);
        return NULL;
    }
    return writer_getstringanddestroy(buf);
}

void context_deinit(ApeContext_t* ctx)
{
    vm_destroy(ctx->vm);
    compiler_destroy(ctx->compiler);
    global_store_destroy(ctx->global_store);
    gcmem_destroy(ctx->mem);
    ptrarray_destroywithitems(ctx->files, (ApeDataCallback_t)compiled_file_destroy);
    errorlist_destroy(&ctx->errors);
}

static ApeObject_t context_wrapnativefunc(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    ApeObject_t objres;
    ApeNativeFuncWrapper_t* wrapper;
    (void)vm;
    wrapper = (ApeNativeFuncWrapper_t*)data;
    APE_ASSERT(vm == wrapper->context->vm);
    objres = wrapper->wrapped_funcptr(wrapper->context, wrapper->data, argc, (ApeObject_t*)args);
    if(context_haserrors(wrapper->context))
    {
        return object_make_null(vm->context);
    }
    return objres;
}

ApeObject_t context_makenamednative(ApeContext_t* ctx, const char* name, ApeWrappedNativeFunc_t fn, void* data)
{
    ApeNativeFuncWrapper_t wrapper;
    memset(&wrapper, 0, sizeof(ApeNativeFuncWrapper_t));
    wrapper.wrapped_funcptr = fn;
    wrapper.context = ctx;
    wrapper.data = data;
    ApeObject_t wrapper_native_function = object_make_native_function_memory(ctx, name, context_wrapnativefunc, &wrapper, sizeof(wrapper));
    if(object_value_isnull(wrapper_native_function))
    {
        return object_make_null(ctx);
    }
    return wrapper_native_function;
}

void context_resetstate(ApeContext_t* ctx)
{
    context_clearerrors(ctx);
    vm_reset(ctx->vm);
}

void context_setdefaultconfig(ApeContext_t* ctx)
{
    memset(&ctx->config, 0, sizeof(ApeConfig_t));
    ctx->config.dumpbytecode = false;
    ctx->config.dumpast = false;
    context_setreplmode(ctx, false);
    context_settimeout(ctx, -1);
    context_setfileread(ctx, util_default_readfile, ctx);
    context_setfilewrite(ctx, util_default_writefile, ctx);
    context_setstdoutwrite(ctx, util_default_stdoutwrite, ctx);
}

