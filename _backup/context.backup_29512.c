
#include "ape.h"

//implcontext
ApeContext_t* context_make(void)
{
    return context_make_ex(NULL, NULL, NULL);
}

ApeContext_t* context_make_ex(ApeMemAllocFunc_t malloc_fn, ApeMemFreeFunc_t free_fn, void* ctx)
{
    ApeContext_t* ape;
    ApeAllocator_t custom_alloc;
    custom_alloc = allocator_make((ApeMemAllocFunc_t)malloc_fn, (ApeMemFreeFunc_t)free_fn, ctx);
    ape = (ApeContext_t*)allocator_malloc(&custom_alloc, sizeof(ApeContext_t));
    if(!ape)
    {
        return NULL;
    }
    memset(ape, 0, sizeof(ApeContext_t));
    ape->alloc = allocator_make(util_default_malloc, util_default_free, ape);
    ape->custom_allocator = custom_alloc;
    context_set_defaultconfig(ape);
    errors_initerrors(&ape->errors);
    ape->mem = gcmem_make(&ape->alloc);
    if(!ape->mem)
    {
        goto err;
    }
    ape->files = ptrarray_make(&ape->alloc);
    if(!ape->files)
    {
        goto err;
    }
    ape->global_store = global_store_make(&ape->alloc, ape->mem);
    if(!ape->global_store)
    {
        goto err;
    }
    ape->compiler = compiler_make(&ape->alloc, &ape->config, ape->mem, &ape->errors, ape->files, ape->global_store);
    if(!ape->compiler)
    {
        goto err;
    }
    ape->vm = vm_make(&ape->alloc, &ape->config, ape->mem, &ape->errors, ape->global_store);
    if(!ape->vm)
    {
        goto err;
    }
    builtins_install(ape->vm);
    return ape;
err:
    context_deinit(ape);
    allocator_free(&custom_alloc, ape);
    return NULL;
}

void context_destroy(ApeContext_t* ape)
{
    if(!ape)
    {
        return;
    }
    context_deinit(ape);
    ApeAllocator_t alloc = ape->alloc;
    allocator_free(&alloc, ape);
}

void context_free_allocated(ApeContext_t* ape, void* ptr)
{
    allocator_free(&ape->alloc, ptr);
}

void context_set_replmode(ApeContext_t* ape, bool enabled)
{
    ape->config.repl_mode = enabled;
}

bool context_set_timeout(ApeContext_t* ape, ApeFloat_t max_execution_time_ms)
{
    if(!util_timer_supported())
    {
        ape->config.max_execution_time_ms = 0;
        ape->config.max_execution_time_set = false;
        return false;
    }

    if(max_execution_time_ms >= 0)
    {
        ape->config.max_execution_time_ms = max_execution_time_ms;
        ape->config.max_execution_time_set = true;
    }
    else
    {
        ape->config.max_execution_time_ms = 0;
        ape->config.max_execution_time_set = false;
    }
    return true;
}

void context_set_stdoutwrite(ApeContext_t* ape, ApeIOStdoutWriteFunc_t stdout_write, void* context)
{
    ape->config.stdio.write.write = stdout_write;
    ape->config.stdio.write.context = context;
}

void context_set_filewrite(ApeContext_t* ape, ApeIOWriteFunc_t file_write, void* context)
{
    ape->config.fileio.write_file.write_file = file_write;
    ape->config.fileio.write_file.context = context;
}

void context_set_fileread(ApeContext_t* ape, ApeIOReadFunc_t file_read, void* context)
{
    ape->config.fileio.read_file.read_file = file_read;
    ape->config.fileio.read_file.context = context;
}

ApeObject_t context_executesource(ApeContext_t* ape, const char* code)
{
    bool ok;
    ApeObject_t objres;
    ApeCompilationResult_t* comp_res;
    context_resetstate(ape);
    comp_res = compiler_compile(ape->compiler, code);
    if(!comp_res || errors_get_count(&ape->errors) > 0)
    {
        goto err;
    }
    ok = vm_run(ape->vm, comp_res, compiler_get_constants(ape->compiler));
    if(!ok || errors_get_count(&ape->errors) > 0)
    {
        goto err;
    }
    APE_ASSERT(ape->vm->sp == 0);
    objres = vm_get_last_popped(ape->vm);
    if(object_value_type(objres) == APE_OBJECT_NONE)
    {
        goto err;
    }
    compilation_result_destroy(comp_res);
    return objres;

err:
    compilation_result_destroy(comp_res);
    return object_make_null();
}

ApeObject_t context_executefile(ApeContext_t* ape, const char* path)
{
    bool ok;
    ApeObject_t objres;
    ApeCompilationResult_t* comp_res;
    context_resetstate(ape);
    comp_res = compiler_compile_file(ape->compiler, path);
    if(!comp_res || errors_get_count(&ape->errors) > 0)
    {
        goto err;
    }
    ok = vm_run(ape->vm, comp_res, compiler_get_constants(ape->compiler));
    if(!ok || errors_get_count(&ape->errors) > 0)
    {
        goto err;
    }
    APE_ASSERT(ape->vm->sp == 0);
    objres = vm_get_last_popped(ape->vm);
    if(object_value_type(objres) == APE_OBJECT_NONE)
    {
        goto err;
    }
    compilation_result_destroy(comp_res);

    return objres;

err:
    compilation_result_destroy(comp_res);
    return object_make_null();
}



bool context_has_errors(const ApeContext_t* ape)
{
    return context_errorcount(ape) > 0;
}

ApeSize_t context_errorcount(const ApeContext_t* ape)
{
    return errors_get_count(&ape->errors);
}

void context_clearerrors(ApeContext_t* ape)
{
    errors_clear(&ape->errors);
}

const ApeError_t* context_get_error(const ApeContext_t* ape, int index)
{
    return (const ApeError_t*)errors_getat(&ape->errors, index);
}

bool context_set_nativefunction(ApeContext_t* ape, const char* name, ApeWrappedNativeFunc_t fn, void* data)
{
    ApeObject_t obj;
    obj = context_makenamednative(ape, name, fn, data);
    if(object_value_isnull(obj))
    {
        return false;
    }
    return context_set_global(ape, name, obj);
}

bool context_set_global(ApeContext_t* ape, const char* name, ApeObject_t obj)
{
    return global_store_set(ape->global_store, name, obj);
}

char* context_error_tostring(ApeContext_t* ape, const ApeError_t* err)
{
    const char* type_str = ape_error_get_type_string(err);
    const char* filename = ape_error_get_filepath(err);
    const char* line = ape_error_get_line(err);
    int line_num = ape_error_get_line_number(err);
    int col_num = ape_error_get_column_number(err);
    ApeStringBuffer_t* buf = strbuf_make(&ape->alloc);
    if(!buf)
    {
        return NULL;
    }
    if(line)
    {
        strbuf_append(buf, line);
        strbuf_append(buf, "\n");
        if(col_num >= 0)
        {
            for(int j = 0; j < (col_num - 1); j++)
            {
                strbuf_append(buf, " ");
            }
            strbuf_append(buf, "^\n");
        }
    }
    strbuf_appendf(buf, "%s ERROR in \"%s\" on %d:%d: %s\n", type_str, filename, line_num, col_num, ape_error_get_message(err));
    const ApeTraceback_t* traceback = ape_error_get_traceback(err);
    if(traceback)
    {
        strbuf_appendf(buf, "traceback:\n");
        traceback_tostring((const ApeTraceback_t*)ape_error_get_traceback(err), buf);
    }
    if(strbuf_failed(buf))
    {
        strbuf_destroy(buf);
        return NULL;
    }
    return strbuf_get_string_and_destroy(buf);
}

void context_deinit(ApeContext_t* ape)
{
    vm_destroy(ape->vm);
    compiler_destroy(ape->compiler);
    global_store_destroy(ape->global_store);
    gcmem_destroy(ape->mem);
    ptrarray_destroy_with_items(ape->files, (ApeDataCallback_t)compiled_file_destroy);
    errors_destroyerrors(&ape->errors);
}

static ApeObject_t vmpriv_wrapnativefunc(ApeVM_t* vm, void* data, int argc, ApeObject_t* args)
{
    ApeObject_t objres;
    ApeNativeFuncWrapper_t* wrapper;
    (void)vm;
    wrapper = (ApeNativeFuncWrapper_t*)data;
    APE_ASSERT(vm == wrapper->ape->vm);
    objres = wrapper->wrapped_funcptr(wrapper->ape, wrapper->data, argc, (ApeObject_t*)args);
    if(context_has_errors(wrapper->ape))
    {
        return object_make_null();
    }
    return objres;
}

ApeObject_t context_makenamednative(ApeContext_t* ape, const char* name, ApeWrappedNativeFunc_t fn, void* data)
{
    ApeNativeFuncWrapper_t wrapper;
    memset(&wrapper, 0, sizeof(ApeNativeFuncWrapper_t));
    wrapper.wrapped_funcptr = fn;
    wrapper.ape = ape;
    wrapper.data = data;
    ApeObject_t wrapper_native_function = object_make_native_function_memory(ape->mem, name, vmpriv_wrapnativefunc, &wrapper, sizeof(wrapper));
    if(object_value_isnull(wrapper_native_function))
    {
        return object_make_null();
    }
    return wrapper_native_function;
}

void context_resetstate(ApeContext_t* ape)
{
    context_clearerrors(ape);
    vm_reset(ape->vm);
}

void context_set_defaultconfig(ApeContext_t* ape)
{
    memset(&ape->config, 0, sizeof(ApeConfig_t));
    context_set_replmode(ape, false);
    context_set_timeout(ape, -1);
    context_set_fileread(ape, util_default_readfile, ape);
    context_set_filewrite(ape, util_default_writefile, ape);
    context_set_stdoutwrite(ape, util_default_stdoutwrite, ape);
}

