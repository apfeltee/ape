
#include "inline.h"

ApeTraceback_t* ape_make_traceback(ApeContext_t* ctx)
{
    ApeTraceback_t* traceback;
    traceback = (ApeTraceback_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeTraceback_t));
    if(!traceback)
    {
        return NULL;
    }
    memset(traceback, 0, sizeof(ApeTraceback_t));
    traceback->context = ctx;
    traceback->items = ape_make_valarray(ctx, sizeof(ApeTracebackItem_t));
    if(!traceback->items)
    {
        ape_traceback_destroy(traceback);
        return NULL;
    }
    return traceback;
}

void ape_traceback_destroy(ApeTraceback_t* traceback)
{
    ApeSize_t i;
    ApeTracebackItem_t* item;
    if(!traceback)
    {
        return;
    }
    for(i = 0; i < ape_valarray_count(traceback->items); i++)
    {
        item = (ApeTracebackItem_t*)ape_valarray_get(traceback->items, i);
        ape_allocator_free(&traceback->context->alloc, item->function_name);
    }
    ape_valarray_destroy(traceback->items);
    ape_allocator_free(&traceback->context->alloc, traceback);
}

bool ape_traceback_append(ApeTraceback_t* traceback, const char* function_name, ApePosition_t pos)
{
    bool ok;
    ApeTracebackItem_t item;
    item.function_name = ape_util_strdup(traceback->context, function_name);
    if(!item.function_name)
    {
        return false;
    }
    item.pos = pos;
    ok = ape_valarray_add(traceback->items, &item);
    if(!ok)
    {
        ape_allocator_free(&traceback->context->alloc, item.function_name);
        return false;
    }
    return true;
}

bool ape_traceback_appendfromvm(ApeTraceback_t* traceback, ApeVM_t* vm)
{
    ApeInt_t i;
    bool ok;
    ApeFrame_t* frame;
    for(i = vm->countframes - 1; i >= 0; i--)
    {
        frame = (ApeFrame_t*)deqlist_get(vm->frameobjects, i);        
        ok = ape_traceback_append(traceback, ape_object_function_getname(frame->function), ape_frame_srcposition(frame));
        if(!ok)
        {
            return false;
        }
    }
    return true;
}

const char* ape_traceback_itemgetfilepath(ApeTracebackItem_t* item)
{
    if(!item->pos.file)
    {
        return NULL;
    }
    return item->pos.file->path;
}

bool ape_tostring_traceback(ApeWriter_t* buf, ApeTraceback_t* traceback)
{
    ApeSize_t i;
    ApeSize_t depth;
    depth = ape_valarray_count(traceback->items);
    for(i = 0; i < depth; i++)
    {
        ApeTracebackItem_t* item = (ApeTracebackItem_t*)ape_valarray_get(traceback->items, i);
        const char* filename = ape_traceback_itemgetfilepath(item);
        if(item->pos.line >= 0 && item->pos.column >= 0)
        {
            ape_writer_appendf(buf, "%s in %s on %d:%d\n", item->function_name, filename, item->pos.line, item->pos.column);
        }
        else
        {
            ape_writer_appendf(buf, "%s\n", item->function_name);
        }
    }
    return !ape_writer_failed(buf);
}

const char* ape_tostring_errortype(ApeErrorType_t type)
{
    switch(type)
    {
        case APE_ERROR_PARSING:
            return "PARSING";
        case APE_ERROR_COMPILATION:
            return "COMPILATION";
        case APE_ERROR_RUNTIME:
            return "RUNTIME";
        case APE_ERROR_TIMEOUT:
            return "TIMEOUT";
        case APE_ERROR_ALLOCATION:
            return "ALLOCATION";
        case APE_ERROR_USER:
            return "USER";
        default:
            return "NONE";
    }
}

ApeErrorType_t ape_error_gettype(ApeError_t* error)
{
    switch(error->errtype)
    {
        case APE_ERROR_NONE:
            return APE_ERROR_NONE;
        case APE_ERROR_PARSING:
            return APE_ERROR_PARSING;
        case APE_ERROR_COMPILATION:
            return APE_ERROR_COMPILATION;
        case APE_ERROR_RUNTIME:
            return APE_ERROR_RUNTIME;
        case APE_ERROR_TIMEOUT:
            return APE_ERROR_TIMEOUT;
        case APE_ERROR_ALLOCATION:
            return APE_ERROR_ALLOCATION;
        case APE_ERROR_USER:
            return APE_ERROR_USER;
        default:
            return APE_ERROR_NONE;
    }
}

const char* ape_error_gettypestring(ApeError_t* error)
{
    return ape_tostring_errortype(ape_error_gettype(error));
}

ApeObject_t ape_object_make_error(ApeContext_t* ctx, const char* error)
{
    char* error_str;
    ApeObject_t res;
    error_str = ape_util_strdup(ctx, error);
    if(!error_str)
    {
        return ape_object_make_null(ctx);
    }
    res = ape_object_make_error_nocopy(ctx, error_str);
    if(ape_object_value_isnull(res))
    {
        ape_allocator_free(&ctx->alloc, error_str);
        return ape_object_make_null(ctx);
    }
    return res;
}

ApeObject_t ape_object_make_error_nocopy(ApeContext_t* ctx, char* error)
{
    ApeGCObjData_t* data;
    data = ape_gcmem_allocobjdata(ctx->vm->mem, APE_OBJECT_ERROR);
    if(!data)
    {
        return ape_object_make_null(ctx);
    }
    data->valerror.message = error;
    data->valerror.traceback = NULL;
    return object_make_from_data(ctx, APE_OBJECT_ERROR, data);
}

void ape_errorlist_initerrors(ApeErrorList_t* errors)
{
    memset(errors, 0, sizeof(ApeErrorList_t));
    errors->count = 0;
}

void ape_errorlist_destroy(ApeErrorList_t* errors)
{
    ape_errorlist_clear(errors);
}

void ape_errorlist_add(ApeErrorList_t* errors, ApeErrorType_t type, ApePosition_t pos, const char* message)
{
    int len;
    int to_copy;
    ApeError_t err;
    if(errors->count >= APE_CONF_SIZE_ERRORS_MAXCOUNT)
    {
        return;
    }
    memset(&err, 0, sizeof(ApeError_t));
    err.errtype = type;
    len = (int)strlen(message);
    to_copy = len;
    if(to_copy >= (APE_CONF_SIZE_ERROR_MAXMSGLENGTH - 1))
    {
        to_copy = APE_CONF_SIZE_ERROR_MAXMSGLENGTH - 1;
    }
    memcpy(err.message, message, to_copy);
    err.message[to_copy] = '\0';
    err.pos = pos;
    err.traceback = NULL;
    errors->errors[errors->count] = err;
    errors->count++;
}

void ape_errorlist_addformat(ApeErrorList_t* errors, ApeErrorType_t type, ApePosition_t pos, const char* format, ...)
{
    int to_write;
    int written;
    char res[APE_CONF_SIZE_ERROR_MAXMSGLENGTH];
    va_list args;
    (void)to_write;
    (void)written;
    va_start(args, format);
    to_write = vsnprintf(NULL, 0, format, args);
    va_end(args);
    va_start(args, format);
    written = vsnprintf(res, APE_CONF_SIZE_ERROR_MAXMSGLENGTH, format, args);
    APE_ASSERT(to_write == written);
    va_end(args);
    ape_errorlist_add(errors, type, pos, res);
}

void ape_errorlist_addformatv(ApeErrorList_t* errors, ApeErrorType_t type, ApePosition_t pos, const char* format, va_list va)
{
    int to_write;
    int written;
    char res[APE_CONF_SIZE_ERROR_MAXMSGLENGTH];
    va_list copy;
    (void)to_write;
    (void)written;
    va_copy(copy, va);
    to_write = vsnprintf(NULL, 0, format, copy);
    written = vsnprintf(res, APE_CONF_SIZE_ERROR_MAXMSGLENGTH, format, va);
    APE_ASSERT(to_write == written);
    ape_errorlist_add(errors, type, pos, res);
}

void ape_errorlist_clear(ApeErrorList_t* errors)
{
    ApeSize_t i;
    ApeError_t* error;
    for(i = 0; i < ape_errorlist_count(errors); i++)
    {
        error = ape_errorlist_getat(errors, i);
        if(error->traceback)
        {
            ape_traceback_destroy(error->traceback);
        }
    }
    errors->count = 0;
}

ApeSize_t ape_errorlist_count(ApeErrorList_t* errors)
{
    return errors->count;
}

ApeError_t* ape_errorlist_getat(ApeErrorList_t* errors, ApeInt_t ix)
{
    if(ix >= (ApeInt_t)errors->count)
    {
        return NULL;
    }
    return &errors->errors[ix];
}

ApeError_t* ape_errorlist_lasterror(ApeErrorList_t* errors)
{
    if(errors->count <= 0)
    {
        return NULL;
    }
    return &errors->errors[errors->count - 1];
}

bool ape_errorlist_haserrors(ApeErrorList_t* errors)
{
    return ape_errorlist_count(errors) > 0;
}

const char* ape_error_getmessage(ApeError_t* error)
{
    return error->message;
}

const char* ape_error_getfile(ApeError_t* error)
{
    if(!error->pos.file)
    {
        return NULL;
    }
    return error->pos.file->path;
}

const char* ape_error_getsource(ApeError_t* error)
{
    if(!error->pos.file)
    {
        return NULL;
    }
    ApePtrArray_t* lines = error->pos.file->lines;
    if(error->pos.line >= (ApeInt_t)ape_ptrarray_count(lines))
    {
        return NULL;
    }
    const char* line = (const char*)ape_ptrarray_get(lines, error->pos.line);
    return line;
}

int ape_error_getline(ApeError_t* error)
{
    if(error->pos.line < 0)
    {
        return -1;
    }
    return error->pos.line + 1;
}

int ape_error_getcolumn(ApeError_t* error)
{
    if(error->pos.column < 0)
    {
        return -1;
    }
    return error->pos.column + 1;
}

ApeTraceback_t* ape_error_gettraceback(ApeError_t* error)
{
    return error->traceback;
}

const char* ape_object_value_geterrormessage(ApeObject_t object)
{
    ApeGCObjData_t* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_ERROR);
    data = ape_object_value_allocated_data(object);
    return data->valerror.message;
}

void ape_object_value_seterrortraceback(ApeObject_t object, ApeTraceback_t* traceback)
{
    ApeGCObjData_t* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_ERROR);
    if(ape_object_value_type(object) != APE_OBJECT_ERROR)
    {
        return;
    }
    data = ape_object_value_allocated_data(object);
    APE_ASSERT(data->valerror.traceback == NULL);
    data->valerror.traceback = traceback;
}

ApeTraceback_t* ape_object_value_geterrortraceback(ApeObject_t object)
{
    ApeGCObjData_t* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_ERROR);
    data = ape_object_value_allocated_data(object);
    return data->valerror.traceback;
}
