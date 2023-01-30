
#include "inline.h"

ApeContext* ape_make_context()
{
    return ape_make_contextex(NULL, NULL, NULL);
}

ApeContext* ape_make_contextex(ApeMemAllocFunc malloc_fn, ApeMemFreeFunc free_fn, void* optr)
{
    ApeContext* ctx;
    ApeAllocator custalloc;
    ApeAllocator thisalloc;
    ctx = NULL;

    ape_make_allocator(ctx, &custalloc, malloc_fn, free_fn, optr);
    ctx = (ApeContext*)ape_allocator_alloc(&custalloc, sizeof(ApeContext));
    //ctx = (ApeContext*)malloc(sizeof(ApeContext));
    memset(ctx, 0, sizeof(ApeContext));
    ape_make_allocator(ctx, &thisalloc, ape_mem_defaultmalloc, ape_mem_defaultfree, optr);
    //custalloc = thisalloc;
    thisalloc = custalloc;
    ctx->alloc = thisalloc;
    ctx->custom_allocator = custalloc;
    fprintf(stderr, "ctx=%p, ctx->alloc=%p\n", ctx, &ctx->alloc);
    

    ape_context_setdefaultconfig(ctx);
    ctx->debugwriter = ape_make_writerio(ctx, stderr, false, true);
    ctx->stdoutwriter = ape_make_writerio(ctx, stdout, false, true);
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
    ctx->pseudoclasses = ape_make_ptrarray(ctx);
    ctx->classmapping = ape_make_strdict(ctx, NULL, NULL);
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
    ctx->vm = ape_make_vm(ctx, &ctx->config, ctx->mem, &ctx->errors, ctx->globalstore);
    if(!ctx->vm)
    {
        goto err;
    }
    ctx->vm->context = ctx;
    ctx->objstringfuncs = ape_make_strdict(ctx, NULL, NULL);
    ctx->objarrayfuncs = ape_make_strdict(ctx, NULL, NULL);

    ape_builtins_install(ctx->vm);
    return ctx;
err:
    ape_context_deinit(ctx);
    ape_allocator_free(&custalloc, ctx);
    return NULL;
}

void ape_context_destroy(ApeContext* ctx)
{
    ApeAllocator alloc;
    if(!ctx)
    {
        return;
    }
    ape_context_deinit(ctx);
    alloc = ctx->alloc;
    ape_allocator_free(&alloc, ctx);
    //free(ctx);
    ape_allocator_destroy(&alloc);
}

void ape_context_deinit(ApeContext* ctx)
{
    ape_writer_destroy(ctx->debugwriter);
    ape_writer_destroy(ctx->stdoutwriter);
    ape_strdict_destroy(ctx->objstringfuncs);
    ape_strdict_destroy(ctx->objarrayfuncs);
    ape_vm_destroy(ctx->vm);
    ape_compiler_destroy(ctx->compiler);
    ape_globalstore_destroy(ctx->globalstore);
    ape_gcmem_destroy(ctx->mem);
    ape_strdict_destroy(ctx->classmapping);
    ape_ptrarray_destroywithitems(ctx, ctx->pseudoclasses, (ApeDataCallback)ape_pseudoclass_destroy);
    ape_ptrarray_destroywithitems(ctx, ctx->files, (ApeDataCallback)ape_compfile_destroy);
    ape_errorlist_destroy(&ctx->errors);
}

void ape_context_freeallocated(ApeContext* ctx, void* ptr)
{
    ape_allocator_free(&ctx->alloc, ptr);
}

void ape_context_debugvalue(ApeContext* ctx, const char* name, ApeObject val)
{
    char* extobj;
    const char* tnameobj;
    const char* finaltype;
    (void)finaltype;
    tnameobj = ape_object_value_typename(val.type);
    extobj = ape_object_value_typeunionname(ctx, val.type);
    finaltype = tnameobj;
    ape_writer_appendf(ctx->debugwriter, "[DEBUG] %s", name);
    if(extobj)
    {
        if(strcmp(extobj, tnameobj) != 0)
        {
            ape_writer_appendf(ctx->debugwriter, "(obj-union-type=%s)", extobj);
        }
    }
    ape_writer_appendf(ctx->debugwriter, " (type=%s) = ", tnameobj);        
    ape_tostring_object(ctx->debugwriter, val, true);
    ape_writer_append(ctx->debugwriter, "\n");
    if(extobj)
    {
        ape_allocator_free(&ctx->alloc, extobj);
    }
}

bool ape_context_settimeout(ApeContext* ctx, ApeFloat max_execution_time_ms)
{
    ctx->config.max_execution_time_ms = max_execution_time_ms;
    ctx->config.max_execution_time_set = false;
    return false;
}

void ape_context_setstdoutwrite(ApeContext* ctx, ApeIOStdoutWriteFunc stdout_write, void* ptr)
{
    (void)ptr;
    ctx->config.stdio.write = stdout_write;
}

void ape_context_setfilewrite(ApeContext* ctx, ApeIOWriteFunc file_write, void* ptr)
{
    (void)ptr;
    ctx->config.fileio.fnwritefile = file_write;
}

void ape_context_setfileread(ApeContext* ctx, ApeIOReadFunc file_read, void* ptr)
{
    (void)ptr;
    ctx->config.fileio.fnreadfile = file_read;
}

void ape_context_dumpast(ApeContext* ctx, ApePtrArray* statements)
{
    ApeWriter* strbuf;
    strbuf = ape_make_writerio(ctx, stdout, false, true);
    fprintf(stderr, "parsed AST:\n");
    ape_tostring_exprlist(strbuf, statements);
    fprintf(stderr, "\n");
    ape_writer_destroy(strbuf);
}

void ape_context_dumpbytecode(ApeContext* ctx, ApeAstCompResult* cres)
{
    ApeWriter* strbuf;
    strbuf = ape_make_writerio(ctx, stdout, false, true);
    fprintf(stderr, "bytecode:\n");
    ape_tostring_compresult(strbuf, cres, false);
    fprintf(stderr, "\n");
    ape_writer_destroy(strbuf);
}

ApeObject ape_context_executesource(ApeContext* ctx, const char* code, size_t clen, bool alsoreset)
{
    bool ok;
    ApeObject objres;
    ApeAstCompResult* cres;
    if(alsoreset)
    {
        ape_context_resetstate(ctx);
    }
    cres = ape_compiler_compilesource(ctx->compiler, code, clen);
    if(!cres || ape_errorlist_count(&ctx->errors) > 0)
    {
        goto err;
    }
    if(ctx->config.dumpbytecode)
    {
        ape_context_dumpbytecode(ctx, cres);
        if(!ctx->config.runafterdump)
        {
            ape_compresult_destroy(cres);
            return ape_object_make_null(ctx);
        }
    }
    ok = ape_vm_run(ctx->vm, cres, ape_compiler_getconstants(ctx->compiler));
    if(!ok || ape_errorlist_count(&ctx->errors) > 0)
    {
        goto err;
    }
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

ApeObject ape_context_executefile(ApeContext* ctx, const char* path)
{
    bool ok;
    ApeObject objres;
    ApeAstCompResult* cres;
    ape_context_resetstate(ctx);
    cres = ape_compiler_compilefile(ctx->compiler, path);
    if(!cres || ape_errorlist_count(&ctx->errors) > 0)
    {
        goto err;
    }
    if(ctx->config.dumpbytecode)
    {
        ape_context_dumpbytecode(ctx, cres);
        if(!ctx->config.runafterdump)
        {
            ape_compresult_destroy(cres);
            return ape_object_make_null(ctx);
        }
    }
    ok = ape_vm_run(ctx->vm, cres, ape_compiler_getconstants(ctx->compiler));
    if(!ok || ape_errorlist_count(&ctx->errors) > 0)
    {
        goto err;
    }
    //APE_ASSERT(ctx->vm->stackptr == 0);
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

bool ape_context_haserrors(ApeContext* ctx)
{
    return ape_context_errorcount(ctx) > 0;
}

ApeSize ape_context_errorcount(ApeContext* ctx)
{
    return ape_errorlist_count(&ctx->errors);
}

void ape_context_clearerrors(ApeContext* ctx)
{
    ape_errorlist_clear(&ctx->errors);
}

ApeError* ape_context_geterror(ApeContext* ctx, int index)
{
    return ape_errorlist_getat(&ctx->errors, index);
}

bool ape_context_setglobal(ApeContext* ctx, const char* name, ApeObject obj)
{
    return ape_globalstore_set(ctx->globalstore, name, obj);
}

char* ape_context_errortostring(ApeContext* ctx, ApeError* err)
{
    int j;
    int lineno;
    int colno;
    const char* typstr;
    const char* filename;
    const char* linesrc;
    ApeWriter* buf;
    ApeTraceback* traceback;
    typstr = ape_error_gettypestring(err);
    filename = ape_error_getfile(err);
    linesrc = ape_error_getsource(err);
    lineno = ape_error_getline(err);
    colno = ape_error_getcolumn(err);
    buf = ape_make_writer(ctx);
    if(!buf)
    {
        return NULL;
    }
    if(linesrc)
    {
        ape_writer_append(buf, linesrc);
        ape_writer_append(buf, "\n");
        if(colno >= 0)
        {
            for(j = 0; j < (colno - 1); j++)
            {
                ape_writer_append(buf, " ");
            }
            ape_writer_append(buf, "^\n");
        }
    }
    ape_writer_appendf(buf, "%s ERROR in \"%s\" on %d:%d: %s\n", typstr, filename, lineno, colno, ape_error_getmessage(err));
    traceback = ape_error_gettraceback(err);
    if(traceback)
    {
        ape_writer_appendf(buf, "traceback:\n");
        ape_tostring_traceback(buf, ape_error_gettraceback(err));
    }
    if(ape_writer_failed(buf))
    {
        ape_writer_destroy(buf);
        return NULL;
    }
    return ape_writer_getstringanddestroy(buf);
}

static ApeObject ape_context_wrapnativefunc(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    ApeObject objres;
    ApeNativeFuncWrapper* wrapper;
    (void)vm;
    wrapper = (ApeNativeFuncWrapper*)data;
    wrapper->context = vm->context;
    objres = wrapper->wrappedfnptr(wrapper->context, wrapper->data, argc, (ApeObject*)args);
    if(ape_context_haserrors(wrapper->context))
    {
        return ape_object_make_null(vm->context);
    }
    return objres;
}

ApeObject ape_context_makenamednative(ApeContext* ctx, const char* name, ApeWrappedNativeFunc fn, void* data)
{
    ApeObject wrobj;
    ApeNativeFuncWrapper wrapper;
    memset(&wrapper, 0, sizeof(ApeNativeFuncWrapper));
    wrapper.wrappedfnptr = fn;
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

bool ape_context_setnativefunction(ApeContext* ctx, const char* name, ApeWrappedNativeFunc fn, void* data)
{
    ApeObject obj;
    obj = ape_context_makenamednative(ctx, name, fn, data);
    if(ape_object_value_isnull(obj))
    {
        return false;
    }
    return ape_context_setglobal(ctx, name, obj);
}

void ape_context_resetstate(ApeContext* ctx)
{
    ape_context_clearerrors(ctx);
    ape_vm_reset(ctx->vm);
}

void ape_context_setdefaultconfig(ApeContext* ctx)
{
    memset(&ctx->config, 0, sizeof(ApeConfig));
    ctx->config.runafterdump = false;
    ctx->config.dumpbytecode = false;
    ctx->config.dumpast = false;
    ctx->config.dumpstack = false;
    ctx->config.replmode = false;
    ape_context_settimeout(ctx, -1);
    ape_context_setfileread(ctx, ape_util_default_readfile, ctx);
    ape_context_setfilewrite(ctx, ape_util_default_writefile, ctx);
    ape_context_setstdoutwrite(ctx, ape_util_default_stdoutwrite, ctx);
}

