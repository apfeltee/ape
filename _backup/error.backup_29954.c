
#include "ape.h"

ApeObject_t object_make_error(ApeGCMemory_t* mem, const char* error)
{
    char* error_str;
    ApeObject_t res;
    error_str = util_strdup(mem->alloc, error);
    if(!error_str)
    {
        return object_make_null();
    }
    res = object_make_error_no_copy(mem, error_str);
    if(object_value_isnull(res))
    {
        allocator_free(mem->alloc, error_str);
        return object_make_null();
    }
    return res;
}

ApeObject_t object_make_error_no_copy(ApeGCMemory_t* mem, char* error)
{
    ApeObjectData_t* data;
    data = gcmem_alloc_object_data(mem, APE_OBJECT_ERROR);
    if(!data)
    {
        return object_make_null();
    }
    data->error.message = error;
    data->error.traceback = NULL;
    return object_make_from_data(APE_OBJECT_ERROR, data);
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
    if(errors->count >= ERRORS_MAX_COUNT)
    {
        return;
    }
    memset(&err, 0, sizeof(ApeError_t));
    err.type = type;
    len = (int)strlen(message);
    to_copy = len;
    if(to_copy >= (APE_ERROR_MESSAGE_MAX_LENGTH - 1))
    {
        to_copy = APE_ERROR_MESSAGE_MAX_LENGTH - 1;
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
    char res[APE_ERROR_MESSAGE_MAX_LENGTH];
    va_list args;
    (void)to_write;
    (void)written;
    va_start(args, format);
    to_write = vsnprintf(NULL, 0, format, args);
    va_end(args);
    va_start(args, format);
    written = vsnprintf(res, APE_ERROR_MESSAGE_MAX_LENGTH, format, args);
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

ApeSize_t errorlist_count(const ApeErrorList_t* errors)
{
    return errors->count;
}

const ApeError_t* errorlist_getat(const ApeErrorList_t* errors, int ix)
{
    if(ix >= errors->count)
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

bool errorlist_haserrors(const ApeErrorList_t* errors)
{
    return errorlist_count(errors) > 0;
}

const char* error_getmessage(const ApeError_t* ae)
{
    const ApeError_t* error = (const ApeError_t*)ae;
    return error->message;
}

const char* error_getfile(const ApeError_t* ae)
{
    const ApeError_t* error = (const ApeError_t*)ae;
    if(!error->pos.file)
    {
        return NULL;
    }
    return error->pos.file->path;
}

const char* error_getsource(const ApeError_t* error)
{
    if(!error->pos.file)
    {
        return NULL;
    }
    ApePtrArray_t* lines = error->pos.file->lines;
    if(error->pos.line >= ptrarray_count(lines))
    {
        return NULL;
    }
    const char* line = (const char*)ptrarray_get(lines, error->pos.line);
    return line;
}

int error_getline(const ApeError_t* ae)
{
    const ApeError_t* error = (const ApeError_t*)ae;
    if(error->pos.line < 0)
    {
        return -1;
    }
    return error->pos.line + 1;
}

int error_getcolumn(const ApeError_t* ae)
{
    const ApeError_t* error = (const ApeError_t*)ae;
    if(error->pos.column < 0)
    {
        return -1;
    }
    return error->pos.column + 1;
}

ApeErrorType_t error_gettype(const ApeError_t* ae)
{
    const ApeError_t* error = (const ApeError_t*)ae;
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

const char* error_gettypestring(const ApeError_t* error)
{
    return ape_errortype_tostring(error_gettype(error));
}

const ApeTraceback_t* error_gettraceback(const ApeError_t* ae)
{
    const ApeError_t* error = (const ApeError_t*)ae;
    return (const ApeTraceback_t*)error->traceback;
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



