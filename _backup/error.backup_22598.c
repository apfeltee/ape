
#include "ape.h"


ApeTraceback_t* traceback_make(ApeContext_t* ctx)
{
    ApeTraceback_t* traceback;
    traceback = (ApeTraceback_t*)allocator_malloc(&ctx->alloc, sizeof(ApeTraceback_t));
    if(!traceback)
    {
        return NULL;
    }
    memset(traceback, 0, sizeof(ApeTraceback_t));
    traceback->context = ctx;
    traceback->alloc = &ctx->alloc;
    traceback->items = array_make(ctx, ApeTracebackItem_t);
    if(!traceback->items)
    {
        traceback_destroy(traceback);
        return NULL;
    }
    return traceback;
}

void traceback_destroy(ApeTraceback_t* traceback)
{
    ApeSize_t i;
    ApeTracebackItem_t* item;
    if(!traceback)
    {
        return;
    }
    for(i = 0; i < array_count(traceback->items); i++)
    {
        item = (ApeTracebackItem_t*)array_get(traceback->items, i);
        allocator_free(traceback->alloc, item->function_name);
    }
    array_destroy(traceback->items);
    allocator_free(traceback->alloc, traceback);
}

bool traceback_append(ApeTraceback_t* traceback, const char* function_name, ApePosition_t pos)
{
    bool ok;
    ApeTracebackItem_t item;
    item.function_name = ape_util_strdup(traceback->context, function_name);
    if(!item.function_name)
    {
        return false;
    }
    item.pos = pos;
    ok = array_add(traceback->items, &item);
    if(!ok)
    {
        allocator_free(traceback->alloc, item.function_name);
        return false;
    }
    return true;
}

bool traceback_append_from_vm(ApeTraceback_t* traceback, ApeVM_t* vm)
{
    ApeInt_t i;
    bool ok;
    ApeFrame_t* frame;
    for(i = vm->frames_count - 1; i >= 0; i--)
    {
        frame = &vm->frames[i];
        ok = traceback_append(traceback, object_function_getname(frame->function), ape_frame_srcposition(frame));
        if(!ok)
        {
            return false;
        }
    }
    return true;
}

const char* traceback_item_get_filepath(ApeTracebackItem_t* item)
{
    if(!item->pos.file)
    {
        return NULL;
    }
    return item->pos.file->path;
}

bool traceback_tostring(ApeTraceback_t* traceback, ApeWriter_t* buf)
{
    ApeSize_t i;
    ApeSize_t depth;
    depth = array_count(traceback->items);
    for(i = 0; i < depth; i++)
    {
        ApeTracebackItem_t* item = (ApeTracebackItem_t*)array_get(traceback->items, i);
        const char* filename = traceback_item_get_filepath(item);
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

const char* errortype_tostring(ApeErrorType_t type)
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

ApeErrorType_t error_gettype(ApeError_t* error)
{
    switch(error->type)
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

const char* error_gettypestring(ApeError_t* error)
{
    return errortype_tostring(error_gettype(error));
}

ApeObject_t object_make_error(ApeContext_t* ctx, const char* error)
{
    char* error_str;
    ApeObject_t res;
    error_str = ape_util_strdup(ctx, error);
    if(!error_str)
    {
        return object_make_null(ctx);
    }
    res = object_make_error_no_copy(ctx, error_str);
    if(object_value_isnull(res))
    {
        allocator_free(&ctx->alloc, error_str);
        return object_make_null(ctx);
    }
    return res;
}

ApeObject_t object_make_error_no_copy(ApeContext_t* ctx, char* error)
{
    ApeObjectData_t* data;
    data = gcmem_alloc_object_data(ctx->vm->mem, APE_OBJECT_ERROR);
    if(!data)
    {
        return object_make_null(ctx);
    }
    data->error.message = error;
    data->error.traceback = NULL;
    return object_make_from_data(ctx, APE_OBJECT_ERROR, data);
}

void errorlist_initerrors(ApeErrorList_t* errors)
{
    memset(errors, 0, sizeof(ApeErrorList_t));
    errors->count = 0;
}

void errorlist_destroy(ApeErrorList_t* errors)
{
    errorlist_clear(errors);
}

void errorlist_add(ApeErrorList_t* errors, ApeErrorType_t type, ApePosition_t pos, const char* message)
{
    int len;
    int to_copy;
    ApeError_t err;
    if(errors->count >= APE_CONF_SIZE_ERRORS_MAXCOUNT)
    {
        return;
    }
    memset(&err, 0, sizeof(ApeError_t));
    err.type = type;
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

void errorlist_addformat(ApeErrorList_t* errors, ApeErrorType_t type, ApePosition_t pos, const char* format, ...)
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
    errorlist_add(errors, type, pos, res);
}

void errorlist_clear(ApeErrorList_t* errors)
{
    ApeSize_t i;
    ApeError_t* error;
    for(i = 0; i < errorlist_count(errors); i++)
    {
        error = errorlist_getat(errors, i);
        if(error->traceback)
        {
            traceback_destroy(error->traceback);
        }
    }
    errors->count = 0;
}

ApeSize_t errorlist_count(ApeErrorList_t* errors)
{
    return errors->count;
}

ApeError_t* errorlist_getat(ApeErrorList_t* errors, ApeInt_t ix)
{
    if(ix >= (ApeInt_t)errors->count)
    {
        return NULL;
    }
    return &errors->errors[ix];
}

ApeError_t* errorlist_lasterror(ApeErrorList_t* errors)
{
    if(errors->count <= 0)
    {
        return NULL;
    }
    return &errors->errors[errors->count - 1];
}

bool errorlist_haserrors(ApeErrorList_t* errors)
{
    return errorlist_count(errors) > 0;
}

const char* error_getmessage(ApeError_t* error)
{
    return error->message;
}

const char* error_getfile(ApeError_t* error)
{
    if(!error->pos.file)
    {
        return NULL;
    }
    return error->pos.file->path;
}

const char* error_getsource(ApeError_t* error)
{
    if(!error->pos.file)
    {
        return NULL;
    }
    ApePtrArray_t* lines = error->pos.file->lines;
    if(error->pos.line >= (ApeInt_t)ptrarray_count(lines))
    {
        return NULL;
    }
    const char* line = (const char*)ptrarray_get(lines, error->pos.line);
    return line;
}

int error_getline(ApeError_t* error)
{
    if(error->pos.line < 0)
    {
        return -1;
    }
    return error->pos.line + 1;
}

int error_getcolumn(ApeError_t* error)
{
    if(error->pos.column < 0)
    {
        return -1;
    }
    return error->pos.column + 1;
}

ApeTraceback_t* error_gettraceback(ApeError_t* error)
{
    return error->traceback;
}

const char* object_value_geterrormessage(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_ERROR);
    data = object_value_allocated_data(object);
    return data->error.message;
}

void object_value_seterrortraceback(ApeObject_t object, ApeTraceback_t* traceback)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_ERROR);
    if(object_value_type(object) != APE_OBJECT_ERROR)
    {
        return;
    }
    data = object_value_allocated_data(object);
    APE_ASSERT(data->error.traceback == NULL);
    data->error.traceback = traceback;
}

ApeTraceback_t* object_value_geterrortraceback(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_ERROR);
    data = object_value_allocated_data(object);
    return data->error.traceback;
}



