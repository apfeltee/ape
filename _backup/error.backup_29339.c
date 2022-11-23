
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

void errors_initerrors(ApeErrorList_t* errors)
{
    memset(errors, 0, sizeof(ApeErrorList_t));
    errors->count = 0;
}

void errors_destroyerrors(ApeErrorList_t* errors)
{
    errors_clear(errors);
}

void errors_add_error(ApeErrorList_t* errors, ApeErrorType_t type, ApePosition_t pos, const char* message)
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

void errors_add_errorf(ApeErrorList_t* errors, ApeErrorType_t type, ApePosition_t pos, const char* format, ...)
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
    errors_add_error(errors, type, pos, res);
}

void errors_clear(ApeErrorList_t* errors)
{
    ApeSize_t i;
    ApeError_t* error;
    for(i = 0; i < errors_get_count(errors); i++)
    {
        error = errors_get(errors, i);
        if(error->traceback)
        {
            traceback_destroy(error->traceback);
        }
    }
    errors->count = 0;
}

ApeSize_t errors_get_count(const ApeErrorList_t* errors)
{
    return errors->count;
}

ApeError_t* errors_get(ApeErrorList_t* errors, int ix)
{
    if(ix >= errors->count)
    {
        return NULL;
    }
    return &errors->errors[ix];
}

const ApeError_t* errors_getat(const ApeErrorList_t* errors, int ix)
{
    if(ix >= errors->count)
    {
        return NULL;
    }
    return &errors->errors[ix];
}

ApeError_t* errors_get_last_error(ApeErrorList_t* errors)
{
    if(errors->count <= 0)
    {
        return NULL;
    }
    return &errors->errors[errors->count - 1];
}

bool errors_has_errors(const ApeErrorList_t* errors)
{
    return errors_get_count(errors) > 0;
}

const char* ape_error_get_message(const ApeError_t* ae)
{
    const ApeError_t* error = (const ApeError_t*)ae;
    return error->message;
}

const char* ape_error_get_filepath(const ApeError_t* ae)
{
    const ApeError_t* error = (const ApeError_t*)ae;
    if(!error->pos.file)
    {
        return NULL;
    }
    return error->pos.file->path;
}

const char* ape_error_get_line(const ApeError_t* ae)
{
    const ApeError_t* error = (const ApeError_t*)ae;
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

int ape_error_get_line_number(const ApeError_t* ae)
{
    const ApeError_t* error = (const ApeError_t*)ae;
    if(error->pos.line < 0)
    {
        return -1;
    }
    return error->pos.line + 1;
}

int ape_error_get_column_number(const ApeError_t* ae)
{
    const ApeError_t* error = (const ApeError_t*)ae;
    if(error->pos.column < 0)
    {
        return -1;
    }
    return error->pos.column + 1;
}

ApeErrorType_t ape_error_get_type(const ApeError_t* ae)
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

const char* ape_error_get_type_string(const ApeError_t* error)
{
    return ape_errortype_tostring(ape_error_get_type(error));
}

const ApeTraceback_t* ape_error_get_traceback(const ApeError_t* ae)
{
    const ApeError_t* error = (const ApeError_t*)ae;
    return (const ApeTraceback_t*)error->traceback;
}



const char* object_get_error_message(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_ERROR);
    data = object_value_allocated_data(object);
    return data->error.message;
}

void object_set_error_traceback(ApeObject_t object, ApeTraceback_t* traceback)
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

ApeTraceback_t* object_get_error_traceback(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_ERROR);
    data = object_value_allocated_data(object);
    return data->error.traceback;
}



