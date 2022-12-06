
#include <inttypes.h>
#include "ape.h"

ApeObject_t ape_object_make_number(ApeContext_t* ctx, ApeFloat_t val)
{
    ApeObjData_t* data;
    data = ape_gcmem_getfrompool(ctx->mem, APE_OBJECT_NUMBER);
    if(!data)
    {
        data = ape_gcmem_allocobjdata(ctx->mem, APE_OBJECT_NUMBER);
    }
    if(!data)
    {
        fprintf(stderr, "ape_object_make_number: failed to get data?\n");
        return ape_object_make_null(ctx);
    }
    data->valnum = val;
    return object_make_from_data(ctx, APE_OBJECT_NUMBER, data);
}

ApeObject_t ape_object_make_bool(ApeContext_t* ctx, bool val)
{
    ApeObjData_t* data;
    data = ape_gcmem_getfrompool(ctx->mem, APE_OBJECT_BOOL);
    if(!data)
    {
        data = ape_gcmem_allocobjdata(ctx->mem, APE_OBJECT_BOOL);
    }
    if(!data)
    {
        fprintf(stderr, "ape_object_make_bool: failed to get data?\n");
        return ape_object_make_null(ctx);
    }
    data->valbool = val;
    return object_make_from_data(ctx, APE_OBJECT_BOOL, data);
}

ApeObject_t ape_object_make_null(ApeContext_t* ctx)
{
    ApeObjData_t* data;
    data = ape_gcmem_getfrompool(ctx->mem, APE_OBJECT_NULL);
    if(!data)
    {
        data = ape_gcmem_allocobjdata(ctx->mem, APE_OBJECT_NULL);
    }
    if(!data)
    {
        fprintf(stderr, "internal error: failed to alloc object data for null");
        abort();
    }
    return object_make_from_data(ctx, APE_OBJECT_NULL, data);
}

ApeObject_t ape_object_make_external(ApeContext_t* ctx, void* ptr)
{
    ApeObjData_t* data;
    data = ape_gcmem_allocobjdata(ctx->mem, APE_OBJECT_EXTERNAL);
    if(!data)
    {
        return ape_object_make_null(ctx);
    }
    data->valextern.data = ptr;
    data->valextern.data_destroy_fn = NULL;
    data->valextern.data_copy_fn = NULL;
    return object_make_from_data(ctx, APE_OBJECT_EXTERNAL, data);
}


bool ape_object_value_ishashable(ApeObject_t obj)
{
    ApeObjType_t type;
    type = ape_object_value_type(obj);
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

void ape_tostring_quotechar(ApeWriter_t* buf, int ch)
{
    switch(ch)
    {
        case '\'':
            {
                ape_writer_append(buf, "\\\'");
            }
            break;
        case '\"':
            {
                ape_writer_append(buf, "\\\"");
            }
            break;
        case '\\':
            {
                ape_writer_append(buf, "\\\\");
            }
            break;
        case '\b':
            {
                ape_writer_append(buf, "\\b");
            }
            break;
        case '\f':
            {
                ape_writer_append(buf, "\\f");
            }
            break;
        case '\n':
            {
                ape_writer_append(buf, "\\n");
            }
            break;
        case '\r':
            {
                ape_writer_append(buf, "\\r");
            }
            break;
        case '\t':
            {
                ape_writer_append(buf, "\\t");
            }
            break;
        default:
            {
                /*
                static const char* const hexchars = "0123456789ABCDEF";
                os << '\\';
                if(ch <= 255)
                {
                    os << 'x';
                    os << hexchars[(ch >> 4) & 0xf];
                    os << hexchars[ch & 0xf];
                }
                else
                {
                    os << 'u';
                    os << hexchars[(ch >> 12) & 0xf];
                    os << hexchars[(ch >> 8) & 0xf];
                    os << hexchars[(ch >> 4) & 0xf];
                    os << hexchars[ch & 0xf];
                }
                */
                ape_writer_appendf(buf, "\\x%02x", (unsigned char)ch);
            }
            break;
    }
}

void ape_tostring_quotestring(ApeWriter_t* buf, const char* str, ApeSize_t len, bool withquotes)
{
    int bch;
    char ch;
    ApeSize_t i;
    if(withquotes)
    {
        ape_writer_append(buf, "\"");
    }
    for(i=0; i<len; i++)
    {
        bch = str[i];
        if((bch < 32) || (bch > 127) || (bch == '\"') || (bch == '\\'))
        {
            ape_tostring_quotechar(buf, bch);
        }
        else
        {
            ch = bch;
            ape_writer_appendlen(buf, &ch, 1);
        }
    }
    if(withquotes)
    {
        ape_writer_append(buf, "\"");
    }
}

void ape_tostring_object(ApeWriter_t* buf, ApeObject_t obj, bool quote_str)
{
    const char* sdata;
    ApeSize_t slen;
    ApeSize_t i;
    ApeFloat_t fltnum;
    ApeObject_t iobj;
    ApeObject_t key;
    ApeObject_t val;
    ApeTraceback_t* traceback;
    ApeObjType_t type;
    const ApeScriptFunction_t* compfunc;
    type = ape_object_value_type(obj);
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
                fltnum = ape_object_value_asnumber(obj);
                #if 1
                if(fltnum == ((ApeInt_t)fltnum))
                {
                    #if 0
                    ape_writer_appendf(buf, "%" PRId64, (ApeInt_t)fltnum);
                    #else
                    ape_writer_appendf(buf, "%" PRIi64, (ApeInt_t)fltnum);
                    #endif
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
                ape_writer_append(buf, ape_object_value_asbool(obj) ? "true" : "false");
            }
            break;
        case APE_OBJECT_STRING:
            {
                sdata = ape_object_string_getdata(obj);
                slen = ape_object_string_getlength(obj);
                if(quote_str)
                {
                    ape_tostring_quotestring(buf, sdata, slen, true);
                }
                else
                {
                    ape_writer_appendlen(buf, sdata, slen);
                }
            }
            break;
        case APE_OBJECT_NULL:
            {
                ape_writer_append(buf, "null");
            }
            break;
        case APE_OBJECT_SCRIPTFUNCTION:
            {
                compfunc = ape_object_value_asfunction(obj);
                ape_writer_appendf(buf, "CompiledFunction: %s\n", ape_object_function_getname(obj));
                ape_tostring_bytecode(buf, compfunc->compiledcode->bytecode, compfunc->compiledcode->srcpositions, compfunc->compiledcode->count);
            }
            break;
        case APE_OBJECT_ARRAY:
            {
                ape_writer_append(buf, "[");
                for(i = 0; i < ape_object_array_getlength(obj); i++)
                {
                    iobj = ape_object_array_getvalue(obj, i);
                    ape_tostring_object(buf, iobj, true);
                    if(i < (ape_object_array_getlength(obj) - 1))
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
                for(i = 0; i < ape_object_map_getlength(obj); i++)
                {
                    key = ape_object_map_getkeyat(obj, i);
                    val = ape_object_map_getvalueat(obj, i);
                    ape_tostring_object(buf, key, true);
                    ape_writer_append(buf, ": ");
                    ape_tostring_object(buf, val, true);
                    if(i < (ape_object_map_getlength(obj) - 1))
                    {
                        ape_writer_append(buf, ", ");
                    }
                }
                ape_writer_append(buf, "}");
            }
            break;
        case APE_OBJECT_NATIVEFUNCTION:
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
                ape_writer_appendf(buf, "ERROR: %s\n", ape_object_value_geterrormessage(obj));
                traceback = ape_object_value_geterrortraceback(obj);
                APE_ASSERT(traceback);
                if(traceback)
                {
                    ape_writer_append(buf, "Traceback:\n");
                    ape_tostring_traceback(buf, traceback);
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


const char* ape_object_value_typename(const ApeObjType_t type)
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
        case APE_OBJECT_NATIVEFUNCTION:
            return "NATIVE_FUNCTION";
        case APE_OBJECT_ARRAY:
            return "ARRAY";
        case APE_OBJECT_MAP:
            return "MAP";
        case APE_OBJECT_SCRIPTFUNCTION:
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


void ape_object_data_deinit(ApeObjData_t* data)
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
                if(data->valstring.is_allocated)
                {
                    ape_allocator_free(data->mem->alloc, data->valstring.value_allocated);
                }
            }
            break;
        case APE_OBJECT_SCRIPTFUNCTION:
            {
                if(data->valscriptfunc.owns_data)
                {
                    ape_allocator_free(data->mem->alloc, data->valscriptfunc.name);
                    ape_compresult_destroy(data->valscriptfunc.compiledcode);
                }
                ape_allocator_free(data->mem->alloc, data->valscriptfunc.freevals);
            }
            break;
        case APE_OBJECT_ARRAY:
            {
                ape_valarray_destroy(data->valarray);
            }
            break;
        case APE_OBJECT_MAP:
            {
                ape_valdict_destroy(data->valmap);
            }
            break;
        case APE_OBJECT_NATIVEFUNCTION:
            {
                ape_allocator_free(data->mem->alloc, data->valnatfunc.name);
            }
            break;
        case APE_OBJECT_EXTERNAL:
            {
                if(data->valextern.data_destroy_fn)
                {
                    data->valextern.data_destroy_fn(data->valextern.data);
                }
            }
            break;
        case APE_OBJECT_ERROR:
            {
                ape_allocator_free(data->mem->alloc, data->valerror.message);
                ape_traceback_destroy(data->valerror.traceback);
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
            ape_writer_append(res, ape_object_value_typename(t)); \
            in_between = true;                           \
        }                                                \
    } while(0)

char* ape_object_value_typeunionname(ApeContext_t* ctx, const ApeObjType_t type)
{
    bool in_between;
    ApeWriter_t* res;
    if(type == APE_OBJECT_ANY || type == APE_OBJECT_NONE || type == APE_OBJECT_FREED)
    {
        return ape_util_strdup(ctx, ape_object_value_typename(type));
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
    CHECK_TYPE(APE_OBJECT_NATIVEFUNCTION);
    CHECK_TYPE(APE_OBJECT_ARRAY);
    CHECK_TYPE(APE_OBJECT_MAP);
    CHECK_TYPE(APE_OBJECT_SCRIPTFUNCTION);
    CHECK_TYPE(APE_OBJECT_EXTERNAL);
    CHECK_TYPE(APE_OBJECT_ERROR);
    return ape_writer_getstringanddestroy(res);
}

char* ape_object_value_serialize(ApeContext_t* ctx, ApeObject_t object, ApeSize_t* lendest)
{
    ApeSize_t l;
    char* string;
    ApeWriter_t* buf;
    buf = ape_make_writer(ctx);
    if(!buf)
    {
        return NULL;
    }
    ape_tostring_object(buf, object, true);
    l = buf->datalength;
    string = ape_writer_getstringanddestroy(buf);
    if(lendest != NULL)
    {
        *lendest = l;
    }
    return string;
}

bool ape_object_extern_setdestroyfunc(ApeObject_t object, ApeDataCallback_t destroy_fn)
{
    ApeExternalData_t* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_EXTERNAL);
    data = ape_object_value_asexternal(object);
    if(!data)
    {
        return false;
    }
    data->data_destroy_fn = destroy_fn;
    return true;
}


bool ape_object_extern_setdata(ApeObject_t object, void* ext_data)
{
    ApeExternalData_t* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_EXTERNAL);
    data = ape_object_value_asexternal(object);
    if(!data)
    {
        return false;
    }
    data->data = ext_data;
    return true;
}

bool ape_object_extern_setcopyfunc(ApeObject_t object, ApeDataCallback_t copy_fn)
{
    ApeExternalData_t* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_EXTERNAL);
    data = ape_object_value_asexternal(object);
    if(!data)
    {
        return false;
    }
    data->data_copy_fn = copy_fn;
    return true;
}

ApeObject_t ape_object_getkvpairat(ApeContext_t* ctx, ApeObject_t object, int ix)
{
    ApeObject_t key;
    ApeObject_t val;
    ApeObject_t res;
    ApeObject_t key_obj;
    ApeObject_t val_obj;
    ApeObjData_t* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_MAP);
    data = ape_object_value_allocated_data(object);
    if(ix >= (ApeInt_t)ape_valdict_count(data->valmap))
    {
        return ape_object_make_null(ctx);
    }
    key = ape_object_map_getkeyat(object, ix);
    val = ape_object_map_getvalueat(object, ix);
    res = ape_object_make_map(ctx);
    if(ape_object_value_isnull(res))
    {
        return ape_object_make_null(ctx);
    }
    key_obj = ape_object_make_string(ctx, "key");
    if(ape_object_value_isnull(key_obj))
    {
        return ape_object_make_null(ctx);
    }
    ape_object_map_setvalue(res, key_obj, key);
    val_obj = ape_object_make_string(ctx, "value");
    if(ape_object_value_isnull(val_obj))
    {
        return ape_object_make_null(ctx);
    }
    ape_object_map_setvalue(res, val_obj, val);
    return res;
}

ApeObject_t ape_object_value_internalcopydeep(ApeContext_t* ctx, ApeObject_t obj, ApeValDict_t * copies)
{
    ApeSize_t i;
    ApeSize_t len;
    bool ok;
    const char* str;
    ApeUShort_t* bytecode_copy;
    ApeScriptFunction_t* function_copy;
    ApeScriptFunction_t* function;
    ApePosition_t* src_positions_copy;
    ApeCompResult_t* comp_res_copy;
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
    ApeObjType_t type;
    copy_ptr = (ApeObject_t*)ape_valdict_getbykey(copies, &obj);
    if(copy_ptr)
    {
        return *copy_ptr;
    }
    copy = ape_object_make_null(ctx);
    type = ape_object_value_type(obj);
    switch(type)
    {
        case APE_OBJECT_FREED:
        case APE_OBJECT_ANY:
        case APE_OBJECT_NONE:
            {
                APE_ASSERT(false);
                copy = ape_object_make_null(ctx);
            }
            break;
        case APE_OBJECT_NUMBER:
        case APE_OBJECT_BOOL:
        case APE_OBJECT_NULL:
        case APE_OBJECT_NATIVEFUNCTION:
            {
                copy = obj;
            }
            break;
        case APE_OBJECT_STRING:
            {
                str = ape_object_string_getdata(obj);
                copy = ape_object_make_string(ctx, str);
            }
            break;

        case APE_OBJECT_SCRIPTFUNCTION:
            {
                function = ape_object_value_asfunction(obj);
                bytecode_copy = NULL;
                src_positions_copy = NULL;
                comp_res_copy = NULL;
                bytecode_copy = (ApeUShort_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeUShort_t) * function->compiledcode->count);
                if(!bytecode_copy)
                {
                    return ape_object_make_null(ctx);
                }
                memcpy(bytecode_copy, function->compiledcode->bytecode, sizeof(ApeUShort_t) * function->compiledcode->count);
                src_positions_copy = (ApePosition_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApePosition_t) * function->compiledcode->count);
                if(!src_positions_copy)
                {
                    ape_allocator_free(&ctx->alloc, bytecode_copy);
                    return ape_object_make_null(ctx);
                }
                memcpy(src_positions_copy, function->compiledcode->srcpositions, sizeof(ApePosition_t) * function->compiledcode->count);
                /* todo: add compilation result copy function */
                comp_res_copy = ape_make_compresult(ctx, bytecode_copy, src_positions_copy, function->compiledcode->count);
                if(!comp_res_copy)
                {
                    ape_allocator_free(&ctx->alloc, src_positions_copy);
                    ape_allocator_free(&ctx->alloc, bytecode_copy);
                    return ape_object_make_null(ctx);
                }
                copy = ape_object_make_function(ctx, ape_object_function_getname(obj), comp_res_copy, true, function->numlocals, function->numargs, 0);
                if(ape_object_value_isnull(copy))
                {
                    ape_compresult_destroy(comp_res_copy);
                    return ape_object_make_null(ctx);
                }
                ok = ape_valdict_set(copies, &obj, &copy);
                if(!ok)
                {
                    return ape_object_make_null(ctx);
                }
                function_copy = ape_object_value_asfunction(copy);
                function_copy->freevals = (ApeObject_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeObject_t) * function->numfreevals);
                if(!function_copy->freevals)
                {
                    return ape_object_make_null(ctx);
                }
                function_copy->numfreevals = function->numfreevals;
                for(i = 0; i < function->numfreevals; i++)
                {
                    free_val = ape_object_function_getfreeval(obj, i);
                    free_val_copy = ape_object_value_internalcopydeep(ctx, free_val, copies);
                    if(!ape_object_value_isnull(free_val) && ape_object_value_isnull(free_val_copy))
                    {
                        return ape_object_make_null(ctx);
                    }
                    ape_object_function_setfreeval(copy, i, free_val_copy);
                }
            }
            break;
        case APE_OBJECT_ARRAY:
            {
                len = ape_object_array_getlength(obj);
                copy = ape_object_make_arraycapacity(ctx, len);
                if(ape_object_value_isnull(copy))
                {
                    return ape_object_make_null(ctx);
                }
                ok = ape_valdict_set(copies, &obj, &copy);
                if(!ok)
                {
                    return ape_object_make_null(ctx);
                }
                for(i = 0; i < len; i++)
                {
                    item = ape_object_array_getvalue(obj, i);
                    item_copy = ape_object_value_internalcopydeep(ctx, item, copies);
                    if(!ape_object_value_isnull(item) && ape_object_value_isnull(item_copy))
                    {
                        return ape_object_make_null(ctx);
                    }
                    ok = ape_object_array_pushvalue(copy, item_copy);
                    if(!ok)
                    {
                        return ape_object_make_null(ctx);
                    }
                }
            }
            break;
        case APE_OBJECT_MAP:
            {
                copy = ape_object_make_map(ctx);
                if(ape_object_value_isnull(copy))
                {
                    return ape_object_make_null(ctx);
                }
                ok = ape_valdict_set(copies, &obj, &copy);
                if(!ok)
                {
                    return ape_object_make_null(ctx);
                }
                for(i = 0; i < ape_object_map_getlength(obj); i++)
                {
                    key = ape_object_map_getkeyat(obj, i);
                    val = ape_object_map_getvalueat(obj, i);
                    key_copy = ape_object_value_internalcopydeep(ctx, key, copies);
                    if(!ape_object_value_isnull(key) && ape_object_value_isnull(key_copy))
                    {
                        return ape_object_make_null(ctx);
                    }
                    val_copy = ape_object_value_internalcopydeep(ctx, val, copies);
                    if(!ape_object_value_isnull(val) && ape_object_value_isnull(val_copy))
                    {
                        return ape_object_make_null(ctx);
                    }
                    ok = ape_object_map_setvalue(copy, key_copy, val_copy);
                    if(!ok)
                    {
                        return ape_object_make_null(ctx);
                    }
                }
            }
            break;
        case APE_OBJECT_EXTERNAL:
            {
                copy = ape_object_value_copyflat(ctx, obj);
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

bool ape_object_value_wrapequals(const ApeObject_t* a_ptr, const ApeObject_t* b_ptr)
{
    ApeObject_t a = *a_ptr;
    ApeObject_t b = *b_ptr;
    return ape_object_value_equals(a, b);
}

unsigned long ape_object_value_hash(ApeObject_t* obj_ptr)
{
    bool bval;
    ApeFloat_t val;
    ApeObject_t obj;
    ApeObjType_t type;
    obj = *obj_ptr;
    type = ape_object_value_type(obj);
    switch(type)
    {
        case APE_OBJECT_NUMBER:
            {
                val = ape_object_value_asnumber(obj);
                return ape_util_hashfloat(val);
            }
            break;
        case APE_OBJECT_BOOL:
            {
                bval = ape_object_value_asbool(obj);
                return bval;
            }
            break;
        case APE_OBJECT_STRING:
            {
                return ape_object_string_gethash(obj);
            }
            break;
        default:
            {
            }
            break;
    }
    return 0;
}


ApeObject_t ape_object_value_copydeep(ApeContext_t* ctx, ApeObject_t obj)
{
    ApeObject_t res;
    ApeValDict_t* copies;
    copies = ape_make_valdict(ctx, ApeObject_t, ApeObject_t);
    if(!copies)
    {
        return ape_object_make_null(ctx);
    }
    res = ape_object_value_internalcopydeep(ctx, obj, copies);
    ape_valdict_destroy(copies);
    return res;
}

ApeObject_t ape_object_value_copyflat(ApeContext_t* ctx, ApeObject_t obj)
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
    ApeObjType_t type;
    ApeExternalData_t* external;
    copy = ape_object_make_null(ctx);
    type = ape_object_value_type(obj);
    switch(type)
    {
        case APE_OBJECT_ANY:
        case APE_OBJECT_FREED:
        case APE_OBJECT_NONE:
            {
                APE_ASSERT(false);
                copy = ape_object_make_null(ctx);
            }
            break;
        case APE_OBJECT_NUMBER:
            {
                copy = ape_object_make_number(ctx, ape_object_value_asnumber(obj));
            }
            break;
        case APE_OBJECT_BOOL:
            {
                copy = ape_object_make_bool(ctx, ape_object_value_asbool(obj));
            }
            break;
        case APE_OBJECT_NULL:
            {
                copy = ape_object_make_number(ctx, ape_object_value_asnumber(obj));
            }
            break;
        case APE_OBJECT_SCRIPTFUNCTION:
        case APE_OBJECT_NATIVEFUNCTION:
        case APE_OBJECT_ERROR:
            {
                copy = obj;
            }
            break;
        case APE_OBJECT_STRING:
            {
                str = ape_object_string_getdata(obj);
                copy = ape_object_make_string(ctx, str);
            }
            break;
        case APE_OBJECT_ARRAY:
            {
                len = ape_object_array_getlength(obj);
                copy = ape_object_make_arraycapacity(ctx, len);
                if(ape_object_value_isnull(copy))
                {
                    return ape_object_make_null(ctx);
                }
                for(i = 0; i < len; i++)
                {
                    item = ape_object_array_getvalue(obj, i);
                    ok = ape_object_array_pushvalue(copy, item);
                    if(!ok)
                    {
                        return ape_object_make_null(ctx);
                    }
                }
            }
            break;
        case APE_OBJECT_MAP:
            {
                copy = ape_object_make_map(ctx);
                for(i = 0; i < ape_object_map_getlength(obj); i++)
                {
                    key = ape_object_map_getkeyat(obj, i);
                    val = ape_object_map_getvalueat(obj, i);
                    ok = ape_object_map_setvalue(copy, key, val);
                    if(!ok)
                    {
                        return ape_object_make_null(ctx);
                    }
                }
            }
            break;
        case APE_OBJECT_EXTERNAL:
            {
                copy = ape_object_make_external(ctx, NULL);
                if(ape_object_value_isnull(copy))
                {
                    return ape_object_make_null(ctx);
                }
                external = ape_object_value_asexternal(obj);
                data_copy = NULL;
                if(external->data_copy_fn)
                {
                    data_copy = external->data_copy_fn(external->data);
                }
                else
                {
                    data_copy = external->data;
                }
                ape_object_extern_setdata(copy, data_copy);
                ape_object_extern_setdestroyfunc(copy, external->data_destroy_fn);
                ape_object_extern_setcopyfunc(copy, external->data_copy_fn);
            }
            break;
    }
    return copy;
}

ApeFloat_t ape_object_value_asnumerica(ApeObject_t obj, ApeObjType_t t)
{
    if(t == APE_OBJECT_NUMBER)
    {
        return ape_object_value_asnumber(obj);
    }
    else if(t == APE_OBJECT_BOOL)
    {
        return ape_object_value_asbool(obj);
    }
    else if(t == APE_OBJECT_NULL)
    {
        /* fallthrough */
    }
    return 0;
}

ApeFloat_t ape_object_value_asnumeric(ApeObject_t obj)
{
    return ape_object_value_asnumerica(obj, ape_object_value_type(obj));
}

ApeFloat_t ape_object_value_compare(ApeObject_t a, ApeObject_t b, bool* out_ok)
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
    ApeObjType_t a_type;
    ApeObjType_t b_type;
    if(a.handle == b.handle)
    {
        return 0;
    }
    *out_ok = true;
    a_type = ape_object_value_type(a);
    b_type = ape_object_value_type(b);
    isnumeric = (
        (a_type == APE_OBJECT_NUMBER || a_type == APE_OBJECT_BOOL || a_type == APE_OBJECT_NULL) &&
        (b_type == APE_OBJECT_NUMBER || b_type == APE_OBJECT_BOOL || b_type == APE_OBJECT_NULL)
    );
    if(isnumeric)
    {
        leftval = ape_object_value_asnumerica(a, a_type);
        rightval = ape_object_value_asnumerica(b, b_type);
        return leftval - rightval;
    }
    else if(a_type == b_type && a_type == APE_OBJECT_STRING)
    {
        a_len = ape_object_string_getlength(a);
        b_len = ape_object_string_getlength(b);
        if(a_len != b_len)
        {
            return a_len - b_len;
        }
        a_hash = ape_object_string_gethash(a);
        b_hash = ape_object_string_gethash(b);
        if(a_hash != b_hash)
        {
            return a_hash - b_hash;
        }
        a_string = ape_object_string_getdata(a);
        b_string = ape_object_string_getdata(b);
        return strcmp(a_string, b_string);
    }
    else if((ape_object_value_isallocated(a) || ape_object_value_isnull(a)) && (ape_object_value_isallocated(b) || ape_object_value_isnull(b)))
    {
        a_data_val = (intptr_t)ape_object_value_allocated_data(a);
        b_data_val = (intptr_t)ape_object_value_allocated_data(b);
        return (ApeFloat_t)(a_data_val - b_data_val);
    }
    else
    {
        *out_ok = false;
    }
    return 1;
}

bool ape_object_value_equals(ApeObject_t a, ApeObject_t b)
{
    bool ok;
    ApeFloat_t res;
    ApeObjType_t a_type;
    ApeObjType_t b_type;
    a_type = ape_object_value_type(a);
    b_type = ape_object_value_type(b);
    if(a_type != b_type)
    {
        return false;
    }
    ok = false;
    res = ape_object_value_compare(a, b, &ok);
    return APE_DBLEQ(res, 0);
}


