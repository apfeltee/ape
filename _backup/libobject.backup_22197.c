
#include <inttypes.h>
#include "ape.h"

ApeObject_t object_make_number(ApeContext_t* ctx, ApeFloat_t val)
{
    ApeObjectData_t* data;
    data = gcmem_get_object_data_from_pool(ctx->mem, APE_OBJECT_NUMBER);
    if(!data)
    {
        data = gcmem_alloc_object_data(ctx->mem, APE_OBJECT_NUMBER);
    }

    if(!data)
    {
        fprintf(stderr, "object_make_number: failed to get data?\n");
        return object_make_null(ctx);
    }
    data->numval = val;
    return object_make_from_data(ctx, APE_OBJECT_NUMBER, data);
}

ApeObject_t object_make_bool(ApeContext_t* ctx, bool val)
{
    ApeObjectData_t* data;
    data = gcmem_get_object_data_from_pool(ctx->mem, APE_OBJECT_BOOL);
    if(!data)
    {
        data = gcmem_alloc_object_data(ctx->mem, APE_OBJECT_BOOL);
    }
    if(!data)
    {
        fprintf(stderr, "object_make_bool: failed to get data?\n");
        return object_make_null(ctx);
    }
    data->boolval = val;
    return object_make_from_data(ctx, APE_OBJECT_BOOL, data);
}

ApeObject_t object_make_null(ApeContext_t* ctx)
{
    ApeObjectData_t* data;
    data = gcmem_get_object_data_from_pool(ctx->mem, APE_OBJECT_NULL);
    if(!data)
    {
        data = gcmem_alloc_object_data(ctx->mem, APE_OBJECT_NULL);
    }
    if(!data)
    {
        fprintf(stderr, "internal error: failed to alloc object data for null");
        abort();
    }
    return object_make_from_data(ctx, APE_OBJECT_NULL, data);
}

ApeObject_t object_make_external(ApeContext_t* ctx, void* ptr)
{
    ApeObjectData_t* data;
    data = gcmem_alloc_object_data(ctx->mem, APE_OBJECT_EXTERNAL);
    if(!data)
    {
        return object_make_null(ctx);
    }
    data->external.data = ptr;
    data->external.data_destroy_fn = NULL;
    data->external.data_copy_fn = NULL;
    return object_make_from_data(ctx, APE_OBJECT_EXTERNAL, data);
}


bool object_value_ishashable(ApeObject_t obj)
{
    ApeObjectType_t type;
    type = object_value_type(obj);
    switch(type)
    {
        case APE_OBJECT_STRING:
            {
                return true;
            }
            break;
        case APE_OBJECT_NUMBER:
            {
                return true;
            }
            break;
        case APE_OBJECT_BOOL:
            {
                return true;
            }
            break;
        default:
            {
            }
            break;
    }
    return false;

}

void object_tostring(ApeObject_t obj, ApeWriter_t* buf, bool quote_str)
{
    const char* sdata;
    ApeSize_t slen;
    ApeSize_t i;
    ApeFloat_t fltnum;
    ApeObject_t iobj;
    ApeObject_t key;
    ApeObject_t val;
    ApeTraceback_t* traceback;
    ApeObjectType_t type;
    const ApeFunction_t* compfunc;
    type = object_value_type(obj);
    switch(type)
    {
        case APE_OBJECT_FREED:
            {
                ape_writer_append(buf, "FREED");
            }
            break;
        case APE_OBJECT_NONE:
            {
                ape_writer_append(buf, "NONE");
            }
            break;
        case APE_OBJECT_NUMBER:
            {
                fltnum = object_value_asnumber(obj);
                #if 1
                if(fltnum == ((ApeInt_t)fltnum))
                {
                    //ape_writer_appendf(buf, "%" PRId64, (ApeInt_t)fltnum);
                    ape_writer_appendf(buf, "%" PRIi64, (ApeInt_t)fltnum);
                }
                else
                {
                    ape_writer_appendf(buf, "%1.10g", fltnum);
                }
                #else
                    ape_writer_appendf(buf, "%1.10g", fltnum);
                #endif
            }
            break;
        case APE_OBJECT_BOOL:
            {
                ape_writer_append(buf, object_value_asbool(obj) ? "true" : "false");
            }
            break;
        case APE_OBJECT_STRING:
            {
                sdata = object_string_getdata(obj);
                slen = object_string_getlength(obj);
                if(quote_str)
                {
                    ape_writer_appendf(buf, "\"%s\"", sdata);
                }
                else
                {
                    ape_writer_appendn(buf, sdata, slen);
                }
            }
            break;
        case APE_OBJECT_NULL:
            {
                ape_writer_append(buf, "null");
            }
            break;
        case APE_OBJECT_FUNCTION:
            {
                compfunc = object_value_asfunction(obj);
                ape_writer_appendf(buf, "CompiledFunction: %s\n", object_function_getname(obj));
                code_tostring(compfunc->comp_result->bytecode, compfunc->comp_result->srcpositions, compfunc->comp_result->count, buf);
            }
            break;
        case APE_OBJECT_ARRAY:
            {
                ape_writer_append(buf, "[");
                for(i = 0; i < object_array_getlength(obj); i++)
                {
                    iobj = object_array_getvalue(obj, i);
                    object_tostring(iobj, buf, true);
                    if(i < (object_array_getlength(obj) - 1))
                    {
                        ape_writer_append(buf, ", ");
                    }
                }
                ape_writer_append(buf, "]");
            }
            break;
        case APE_OBJECT_MAP:
            {
                ape_writer_append(buf, "{");
                for(i = 0; i < object_map_getlength(obj); i++)
                {
                    key = object_map_getkeyat(obj, i);
                    val = object_map_getvalueat(obj, i);
                    object_tostring(key, buf, true);
                    ape_writer_append(buf, ": ");
                    object_tostring(val, buf, true);
                    if(i < (object_map_getlength(obj) - 1))
                    {
                        ape_writer_append(buf, ", ");
                    }
                }
                ape_writer_append(buf, "}");
            }
            break;
        case APE_OBJECT_NATIVE_FUNCTION:
            {
                ape_writer_append(buf, "NATIVE_FUNCTION");
            }
            break;
        case APE_OBJECT_EXTERNAL:
            {
                ape_writer_append(buf, "EXTERNAL");
            }
            break;
        case APE_OBJECT_ERROR:
            {
                ape_writer_appendf(buf, "ERROR: %s\n", object_value_geterrormessage(obj));
                traceback = object_value_geterrortraceback(obj);
                APE_ASSERT(traceback);
                if(traceback)
                {
                    ape_writer_append(buf, "Traceback:\n");
                    traceback_tostring(traceback, buf);
                }
            }
            break;
        case APE_OBJECT_ANY:
            {
                APE_ASSERT(false);
            }
            break;
    }
}


const char* object_value_typename(const ApeObjectType_t type)
{
    switch(type)
    {
        case APE_OBJECT_NONE:
            return "NONE";
        case APE_OBJECT_FREED:
            return "NONE";
        case APE_OBJECT_NUMBER:
            return "NUMBER";
        case APE_OBJECT_BOOL:
            return "BOOL";
        case APE_OBJECT_STRING:
            return "STRING";
        case APE_OBJECT_NULL:
            return "NULL";
        case APE_OBJECT_NATIVE_FUNCTION:
            return "NATIVE_FUNCTION";
        case APE_OBJECT_ARRAY:
            return "ARRAY";
        case APE_OBJECT_MAP:
            return "MAP";
        case APE_OBJECT_FUNCTION:
            return "FUNCTION";
        case APE_OBJECT_EXTERNAL:
            return "EXTERNAL";
        case APE_OBJECT_ERROR:
            return "ERROR";
        case APE_OBJECT_ANY:
            return "ANY";
    }
    return "NONE";
}


void object_data_deinit(ApeObjectData_t* data)
{
    switch(data->type)
    {
        case APE_OBJECT_FREED:
            {
                APE_ASSERT(false);
                return;
            }
            break;
        case APE_OBJECT_STRING:
            {
                if(data->string.is_allocated)
                {
                    allocator_free(data->mem->alloc, data->string.value_allocated);
                }
            }
            break;
        case APE_OBJECT_FUNCTION:
            {
                if(data->function.owns_data)
                {
                    allocator_free(data->mem->alloc, data->function.name);
                    ape_compresult_destroy(data->function.comp_result);
                }
                allocator_free(data->mem->alloc, data->function.free_vals_allocated);
            }
            break;
        case APE_OBJECT_ARRAY:
            {
                array_destroy(data->array);
            }
            break;
        case APE_OBJECT_MAP:
            {
                valdict_destroy(data->map);
            }
            break;
        case APE_OBJECT_NATIVE_FUNCTION:
            {
                allocator_free(data->mem->alloc, data->native_function.name);
            }
            break;
        case APE_OBJECT_EXTERNAL:
            {
                if(data->external.data_destroy_fn)
                {
                    data->external.data_destroy_fn(data->external.data);
                }
            }
            break;
        case APE_OBJECT_ERROR:
            {
                allocator_free(data->mem->alloc, data->error.message);
                traceback_destroy(data->error.traceback);
            }
            break;
        default:
            {
            }
            break;
    }
    data->type = APE_OBJECT_FREED;
}

#define CHECK_TYPE(t)                                    \
    do                                                   \
    {                                                    \
        if((type & t) == t)                              \
        {                                                \
            if(in_between)                               \
            {                                            \
                ape_writer_append(res, "|");                 \
            }                                            \
            ape_writer_append(res, object_value_typename(t)); \
            in_between = true;                           \
        }                                                \
    } while(0)

char* object_value_typeunionname(ApeContext_t* ctx, const ApeObjectType_t type)
{
    bool in_between;
    ApeWriter_t* res;
    if(type == APE_OBJECT_ANY || type == APE_OBJECT_NONE || type == APE_OBJECT_FREED)
    {
        return util_strdup(ctx, object_value_typename(type));
    }
    res = ape_make_writer(ctx);
    if(!res)
    {
        return NULL;
    }
    in_between = false;
    CHECK_TYPE(APE_OBJECT_NUMBER);
    CHECK_TYPE(APE_OBJECT_BOOL);
    CHECK_TYPE(APE_OBJECT_STRING);
    CHECK_TYPE(APE_OBJECT_NULL);
    CHECK_TYPE(APE_OBJECT_NATIVE_FUNCTION);
    CHECK_TYPE(APE_OBJECT_ARRAY);
    CHECK_TYPE(APE_OBJECT_MAP);
    CHECK_TYPE(APE_OBJECT_FUNCTION);
    CHECK_TYPE(APE_OBJECT_EXTERNAL);
    CHECK_TYPE(APE_OBJECT_ERROR);
    return ape_writer_getstringanddestroy(res);
}

char* object_value_serialize(ApeContext_t* ctx, ApeObject_t object, ApeSize_t* lendest)
{
    ApeSize_t l;
    char* string;
    ApeWriter_t* buf;
    buf = ape_make_writer(ctx);
    if(!buf)
    {
        return NULL;
    }
    object_tostring(object, buf, true);
    l = buf->len;
    string = ape_writer_getstringanddestroy(buf);
    if(lendest != NULL)
    {
        *lendest = l;
    }
    return string;
}

bool object_value_setexternaldata(ApeObject_t object, void* ext_data)
{
    ApeExternalData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_EXTERNAL);
    data = object_value_asexternal(object);
    if(!data)
    {
        return false;
    }
    data->data = ext_data;
    return true;
}

bool object_value_setexternalcopy(ApeObject_t object, ApeDataCallback_t copy_fn)
{
    ApeExternalData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_EXTERNAL);
    data = object_value_asexternal(object);
    if(!data)
    {
        return false;
    }
    data->data_copy_fn = copy_fn;
    return true;
}

ApeObject_t object_get_kv_pair_at(ApeContext_t* ctx, ApeObject_t object, int ix)
{
    ApeObject_t key;
    ApeObject_t val;
    ApeObject_t res;
    ApeObject_t key_obj;
    ApeObject_t val_obj;
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_MAP);
    data = object_value_allocated_data(object);
    if(ix >= (ApeInt_t)valdict_count(data->map))
    {
        return object_make_null(ctx);
    }
    key = object_map_getkeyat(object, ix);
    val = object_map_getvalueat(object, ix);
    res = object_make_map(ctx);
    if(object_value_isnull(res))
    {
        return object_make_null(ctx);
    }
    key_obj = object_make_string(ctx, "key");
    if(object_value_isnull(key_obj))
    {
        return object_make_null(ctx);
    }
    object_map_setvalue(res, key_obj, key);
    val_obj = object_make_string(ctx, "value");
    if(object_value_isnull(val_obj))
    {
        return object_make_null(ctx);
    }
    object_map_setvalue(res, val_obj, val);
    return res;
}


// INTERNAL
ApeObject_t object_value_internal_copydeep(ApeContext_t* ctx, ApeObject_t obj, ApeValDictionary_t * copies)
{
    ApeSize_t i;
    ApeSize_t len;
    bool ok;
    const char* str;
    ApeUShort_t* bytecode_copy;
    ApeFunction_t* function_copy;
    ApeFunction_t* function;
    ApePosition_t* src_positions_copy;
    ApeCompilationResult_t* comp_res_copy;
    ApeObject_t free_val;
    ApeObject_t free_val_copy;
    ApeObject_t item;
    ApeObject_t item_copy;
    ApeObject_t key;
    ApeObject_t val;
    ApeObject_t key_copy;
    ApeObject_t val_copy;
    ApeObject_t copy;
    ApeObject_t* copy_ptr;
    ApeObjectType_t type;
    copy_ptr = (ApeObject_t*)valdict_get(copies, &obj);
    if(copy_ptr)
    {
        return *copy_ptr;
    }
    copy = object_make_null(ctx);
    type = object_value_type(obj);
    switch(type)
    {
        case APE_OBJECT_FREED:
        case APE_OBJECT_ANY:
        case APE_OBJECT_NONE:
            {
                APE_ASSERT(false);
                copy = object_make_null(ctx);
            }
            break;
        case APE_OBJECT_NUMBER:
        case APE_OBJECT_BOOL:
        case APE_OBJECT_NULL:
        case APE_OBJECT_NATIVE_FUNCTION:
            {
                copy = obj;
            }
            break;
        case APE_OBJECT_STRING:
            {
                str = object_string_getdata(obj);
                copy = object_make_string(ctx, str);
            }
            break;

        case APE_OBJECT_FUNCTION:
            {
                function = object_value_asfunction(obj);
                bytecode_copy = NULL;
                src_positions_copy = NULL;
                comp_res_copy = NULL;
                bytecode_copy = (ApeUShort_t*)allocator_malloc(&ctx->alloc, sizeof(ApeUShort_t) * function->comp_result->count);
                if(!bytecode_copy)
                {
                    return object_make_null(ctx);
                }
                memcpy(bytecode_copy, function->comp_result->bytecode, sizeof(ApeUShort_t) * function->comp_result->count);
                src_positions_copy = (ApePosition_t*)allocator_malloc(&ctx->alloc, sizeof(ApePosition_t) * function->comp_result->count);
                if(!src_positions_copy)
                {
                    allocator_free(&ctx->alloc, bytecode_copy);
                    return object_make_null(ctx);
                }
                memcpy(src_positions_copy, function->comp_result->srcpositions, sizeof(ApePosition_t) * function->comp_result->count);
                // todo: add compilation result copy function
                comp_res_copy = ape_make_compresult(ctx, bytecode_copy, src_positions_copy, function->comp_result->count);
                if(!comp_res_copy)
                {
                    allocator_free(&ctx->alloc, src_positions_copy);
                    allocator_free(&ctx->alloc, bytecode_copy);
                    return object_make_null(ctx);
                }
                copy = object_make_function(ctx, object_function_getname(obj), comp_res_copy, true, function->num_locals, function->num_args, 0);
                if(object_value_isnull(copy))
                {
                    ape_compresult_destroy(comp_res_copy);
                    return object_make_null(ctx);
                }
                ok = valdict_set(copies, &obj, &copy);
                if(!ok)
                {
                    return object_make_null(ctx);
                }
                function_copy = object_value_asfunction(copy);
                function_copy->free_vals_allocated = (ApeObject_t*)allocator_malloc(&ctx->alloc, sizeof(ApeObject_t) * function->free_vals_count);
                if(!function_copy->free_vals_allocated)
                {
                    return object_make_null(ctx);
                }
                function_copy->free_vals_count = function->free_vals_count;
                for(i = 0; i < function->free_vals_count; i++)
                {
                    free_val = object_function_getfreeval(obj, i);
                    free_val_copy = object_value_internal_copydeep(ctx, free_val, copies);
                    if(!object_value_isnull(free_val) && object_value_isnull(free_val_copy))
                    {
                        return object_make_null(ctx);
                    }
                    object_set_function_free_val(copy, i, free_val_copy);
                }
            }
            break;
        case APE_OBJECT_ARRAY:
            {
                len = object_array_getlength(obj);
                copy = object_make_arraycapacity(ctx, len);
                if(object_value_isnull(copy))
                {
                    return object_make_null(ctx);
                }
                ok = valdict_set(copies, &obj, &copy);
                if(!ok)
                {
                    return object_make_null(ctx);
                }
                for(i = 0; i < len; i++)
                {
                    item = object_array_getvalue(obj, i);
                    item_copy = object_value_internal_copydeep(ctx, item, copies);
                    if(!object_value_isnull(item) && object_value_isnull(item_copy))
                    {
                        return object_make_null(ctx);
                    }
                    ok = object_array_pushvalue(copy, item_copy);
                    if(!ok)
                    {
                        return object_make_null(ctx);
                    }
                }
            }
            break;
        case APE_OBJECT_MAP:
            {
                copy = object_make_map(ctx);
                if(object_value_isnull(copy))
                {
                    return object_make_null(ctx);
                }
                ok = valdict_set(copies, &obj, &copy);
                if(!ok)
                {
                    return object_make_null(ctx);
                }
                for(i = 0; i < object_map_getlength(obj); i++)
                {
                    key = object_map_getkeyat(obj, i);
                    val = object_map_getvalueat(obj, i);
                    key_copy = object_value_internal_copydeep(ctx, key, copies);
                    if(!object_value_isnull(key) && object_value_isnull(key_copy))
                    {
                        return object_make_null(ctx);
                    }
                    val_copy = object_value_internal_copydeep(ctx, val, copies);
                    if(!object_value_isnull(val) && object_value_isnull(val_copy))
                    {
                        return object_make_null(ctx);
                    }
                    ok = object_map_setvalue(copy, key_copy, val_copy);
                    if(!ok)
                    {
                        return object_make_null(ctx);
                    }
                }
            }
            break;
        case APE_OBJECT_EXTERNAL:
            {
                copy = object_value_copyflat(ctx, obj);
            }
            break;
        case APE_OBJECT_ERROR:
            {
                copy = obj;
            }
            break;
    }
    return copy;
}

bool object_value_wrapequals(const ApeObject_t* a_ptr, const ApeObject_t* b_ptr)
{
    ApeObject_t a = *a_ptr;
    ApeObject_t b = *b_ptr;
    return object_value_equals(a, b);
}

unsigned long object_value_hash(ApeObject_t* obj_ptr)
{
    bool bval;
    ApeFloat_t val;
    ApeObject_t obj;
    ApeObjectType_t type;
    obj = *obj_ptr;
    type = object_value_type(obj);
    switch(type)
    {
        case APE_OBJECT_NUMBER:
            {
                val = object_value_asnumber(obj);
                return util_hashdouble(val);
            }
            break;
        case APE_OBJECT_BOOL:
            {
                bval = object_value_asbool(obj);
                return bval;
            }
            break;
        case APE_OBJECT_STRING:
            {
                return object_string_gethash(obj);
            }
            break;
        default:
            {
            }
            break;
    }
    return 0;
}


ApeObject_t object_value_copydeep(ApeContext_t* ctx, ApeObject_t obj)
{
    ApeObject_t res;
    ApeValDictionary_t* copies;
    copies = valdict_make(ctx, ApeObject_t, ApeObject_t);
    if(!copies)
    {
        return object_make_null(ctx);
    }
    res = object_value_internal_copydeep(ctx, obj, copies);
    valdict_destroy(copies);
    return res;
}

ApeObject_t object_value_copyflat(ApeContext_t* ctx, ApeObject_t obj)
{
    ApeSize_t i;
    ApeSize_t len;
    bool ok;
    const char* str;
    void* data_copy;
    ApeObject_t item;
    ApeObject_t copy;
    ApeObject_t key;
    ApeObject_t val;
    ApeObjectType_t type;
    ApeExternalData_t* external;
    copy = object_make_null(ctx);
    type = object_value_type(obj);
    switch(type)
    {
        case APE_OBJECT_ANY:
        case APE_OBJECT_FREED:
        case APE_OBJECT_NONE:
            {
                APE_ASSERT(false);
                copy = object_make_null(ctx);
            }
            break;
        case APE_OBJECT_NUMBER:
            {
                copy = object_make_number(ctx, object_value_asnumber(obj));
            }
            break;
        case APE_OBJECT_BOOL:
            {
                copy = object_make_bool(ctx, object_value_asbool(obj));
            }
            break;
        case APE_OBJECT_NULL:
            {
                copy = object_make_number(ctx, object_value_asnumber(obj));
            }
            break;
        case APE_OBJECT_FUNCTION:
        case APE_OBJECT_NATIVE_FUNCTION:
        case APE_OBJECT_ERROR:
            {
                copy = obj;
            }
            break;
        case APE_OBJECT_STRING:
            {
                str = object_string_getdata(obj);
                copy = object_make_string(ctx, str);
            }
            break;
        case APE_OBJECT_ARRAY:
            {
                len = object_array_getlength(obj);
                copy = object_make_arraycapacity(ctx, len);
                if(object_value_isnull(copy))
                {
                    return object_make_null(ctx);
                }
                for(i = 0; i < len; i++)
                {
                    item = object_array_getvalue(obj, i);
                    ok = object_array_pushvalue(copy, item);
                    if(!ok)
                    {
                        return object_make_null(ctx);
                    }
                }
            }
            break;
        case APE_OBJECT_MAP:
            {
                copy = object_make_map(ctx);
                for(i = 0; i < object_map_getlength(obj); i++)
                {
                    key = object_map_getkeyat(obj, i);
                    val = object_map_getvalueat(obj, i);
                    ok = object_map_setvalue(copy, key, val);
                    if(!ok)
                    {
                        return object_make_null(ctx);
                    }
                }
            }
            break;
        case APE_OBJECT_EXTERNAL:
            {
                copy = object_make_external(ctx, NULL);
                if(object_value_isnull(copy))
                {
                    return object_make_null(ctx);
                }
                external = object_value_asexternal(obj);
                data_copy = NULL;
                if(external->data_copy_fn)
                {
                    data_copy = external->data_copy_fn(external->data);
                }
                else
                {
                    data_copy = external->data;
                }
                object_value_setexternaldata(copy, data_copy);
                object_value_setexternaldestroy(copy, external->data_destroy_fn);
                object_value_setexternalcopy(copy, external->data_copy_fn);
            }
            break;
    }
    return copy;
}

ApeFloat_t object_value_asnumerica(ApeObject_t obj, ApeObjectType_t t)
{
    if(t == APE_OBJECT_NUMBER)
    {
        return object_value_asnumber(obj);
    }
    else if(t == APE_OBJECT_BOOL)
    {
        return object_value_asbool(obj);
    }
    else if(t == APE_OBJECT_NULL)
    {
        /* fallthrough */
    }
    return 0;
}

ApeFloat_t object_value_asnumeric(ApeObject_t obj)
{
    return object_value_asnumerica(obj, object_value_type(obj));
}

ApeFloat_t object_value_compare(ApeObject_t a, ApeObject_t b, bool* out_ok)
{
    bool isnumeric;
    int a_len;
    int b_len;
    unsigned long a_hash;
    unsigned long b_hash;
    intptr_t a_data_val;
    intptr_t b_data_val;
    const char* a_string;
    const char* b_string;
    ApeFloat_t leftval;
    ApeFloat_t rightval;
    ApeObjectType_t a_type;
    ApeObjectType_t b_type;
    if(a.handle == b.handle)
    {
        return 0;
    }
    *out_ok = true;
    a_type = object_value_type(a);
    b_type = object_value_type(b);
    isnumeric = (
        (a_type == APE_OBJECT_NUMBER || a_type == APE_OBJECT_BOOL || a_type == APE_OBJECT_NULL) &&
        (b_type == APE_OBJECT_NUMBER || b_type == APE_OBJECT_BOOL || b_type == APE_OBJECT_NULL)
    );
    if(isnumeric)
    {
        leftval = object_value_asnumerica(a, a_type);
        rightval = object_value_asnumerica(b, b_type);
        return leftval - rightval;
    }
    else if(a_type == b_type && a_type == APE_OBJECT_STRING)
    {
        a_len = object_string_getlength(a);
        b_len = object_string_getlength(b);
        if(a_len != b_len)
        {
            return a_len - b_len;
        }
        a_hash = object_string_gethash(a);
        b_hash = object_string_gethash(b);
        if(a_hash != b_hash)
        {
            return a_hash - b_hash;
        }
        a_string = object_string_getdata(a);
        b_string = object_string_getdata(b);
        return strcmp(a_string, b_string);
    }
    else if((object_value_isallocated(a) || object_value_isnull(a)) && (object_value_isallocated(b) || object_value_isnull(b)))
    {
        a_data_val = (intptr_t)object_value_allocated_data(a);
        b_data_val = (intptr_t)object_value_allocated_data(b);
        return (ApeFloat_t)(a_data_val - b_data_val);
    }
    else
    {
        *out_ok = false;
    }
    return 1;
}

bool object_value_equals(ApeObject_t a, ApeObject_t b)
{
    bool ok;
    ApeFloat_t res;
    ApeObjectType_t a_type;
    ApeObjectType_t b_type;
    a_type = object_value_type(a);
    b_type = object_value_type(b);
    if(a_type != b_type)
    {
        return false;
    }
    ok = false;
    res = object_value_compare(a, b, &ok);
    return APE_DBLEQ(res, 0);
}

bool object_value_setexternaldestroy(ApeObject_t object, ApeDataCallback_t destroy_fn)
{
    ApeExternalData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_EXTERNAL);
    data = object_value_asexternal(object);
    if(!data)
    {
        return false;
    }
    data->data_destroy_fn = destroy_fn;
    return true;
}
