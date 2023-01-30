
#include <inttypes.h>
#include "inline.h"

ApeGCObjData* ape_object_make_objdata(ApeContext* ctx, ApeObjType type)
{
    ApeGCObjData* data;
    data = ape_gcmem_getfrompool(ctx->mem, type);
    if(!data)
    {
        data = ape_gcmem_allocobjdata(ctx->mem, type);
    }
    if(!data)
    {
        fprintf(stderr, "ape_object_make_objdata: failed to get data?\n");
        return NULL;
    }
    data->datatype = type;
    return data;
}

/* defined in libstring.c */
ApeObject ape_object_make_string(ApeContext* ctx, const char* string);
ApeObject ape_object_make_stringlen(ApeContext* ctx, const char* string, ApeSize len);
ApeObject ape_object_make_stringcapacity(ApeContext* ctx, ApeSize capacity);

/* defined in libarray.c*/
ApeObject ape_object_make_array(ApeContext* ctx);
ApeObject ape_object_make_arraycapacity(ApeContext* ctx, unsigned capacity);

/* defined in libmap.c */
ApeObject ape_object_make_map(ApeContext* ctx);
ApeObject ape_object_make_mapcapacity(ApeContext* ctx, unsigned capacity);

/* defined in libfunction */
ApeObject ape_object_make_function(ApeContext* ctx, const char* name, ApeAstCompResult* cres, bool wdata, ApeInt nloc, ApeInt nargs, ApeSize fvcount);
ApeObject ape_object_make_nativefuncmemory(ApeContext* ctx, const char* name, ApeNativeFuncPtr fn, void* data, ApeSize dlen);


ApeObject ape_object_make_external(ApeContext* ctx, void* ptr)
{
    ApeGCObjData* data;
    data = ape_object_make_objdata(ctx, APE_OBJECT_EXTERNAL);
    if(!data)
    {
        return ape_object_make_null(ctx);
    }
    data->valextern.data = ptr;
    data->valextern.fndestroy = NULL;
    data->valextern.fncopy = NULL;
    return object_make_from_data(ctx, APE_OBJECT_EXTERNAL, data);
}

bool ape_object_value_ishashable(ApeObject obj)
{
    ApeObjType type;
    type = ape_object_value_type(obj);
    switch(type)
    {
        case APE_OBJECT_STRING:
            {
                return true;
            }
            break;
        case APE_OBJECT_FLOATNUMBER:
        case APE_OBJECT_FIXEDNUMBER:
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

void ape_tostring_quotechar(ApeWriter* buf, int ch)
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

void ape_tostring_quotestring(ApeWriter* buf, const char* str, ApeSize len, bool withquotes)
{
    int bch;
    char ch;
    ApeSize i;
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

void ape_tostring_object(ApeWriter* buf, ApeObject obj, bool quote_str)
{
    const char* sdata;
    const char* fname;
    ApeSize slen;
    ApeSize i;
    ApeFloat fltnum;
    ApeObject iobj;
    ApeObject key;
    ApeObject val;
    ApeTraceback* traceback;
    ApeObjType type;
    ApeValArray* arr;
    const ApeScriptFunction* compfunc;
    (void)compfunc;
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
        case APE_OBJECT_FIXEDNUMBER:
            {
                ape_writer_appendf(buf, "%" PRIi64, ape_object_value_asfixednumber(obj));
            }
            break;
        case APE_OBJECT_FLOATNUMBER:
            {
                fltnum = ape_object_value_asfloatnumber(obj);
                #if 1
                if(fltnum == ((ApeInt)fltnum))
                {
                    #if 0
                    ape_writer_appendf(buf, "%" PRId64, (ApeInt)fltnum);
                    #else
                    ape_writer_appendf(buf, "%" PRIi64, (ApeInt)fltnum);
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
                compfunc = ape_object_value_asscriptfunction(obj);
                fname = ape_object_function_getname(obj);
                #if 0
                    ape_writer_appendf(buf, "CompiledFunction: %s ", fname);
                    if(strcmp(fname, "__main__") == 0)
                    {
                        ape_writer_append(buf, "<entry point>");
                    }
                    else
                    {
                        ape_tostring_bytecode(buf, compfunc->compiledcode->bytecode, compfunc->compiledcode->srcpositions, compfunc->compiledcode->count, true);
                    }
                #else
                    ape_writer_appendf(buf, "<function '%s'>", fname);
                #endif
            }
            break;
        case APE_OBJECT_ARRAY:
            {
                arr = ape_object_array_getarray(obj);
                ape_writer_append(buf, "[");
                for(i = 0; i < ape_object_array_getlength(obj); i++)
                {
                    iobj = ape_object_array_getvalue(obj, i);
                    if(ape_object_value_isarray(iobj) && (ape_object_array_getarray(iobj) == arr))
                    {
                        ape_writer_append(buf, "<recursion>");
                    }
                    else
                    {
                        ape_tostring_object(buf, iobj, true);
                    }
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
        case APE_OBJECT_NUMERIC:
            {
                APE_ASSERT(false);
            }
            break;
        
    }
}

const char* ape_object_value_typename(const ApeObjType type)
{
    switch(type)
    {
        case APE_OBJECT_NONE:
            return "NONE";
        case APE_OBJECT_FREED:
            return "NONE";
        case APE_OBJECT_FIXEDNUMBER:
            return "FIXEDNUMBER";
        case APE_OBJECT_FLOATNUMBER:
            return "FLOATNUMBER";
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
        default:
            break;
    }
    return "NONE";
}

void ape_object_data_deinit(ApeContext* ctx, ApeGCObjData* data)
{
    if(data == NULL)
    {
        return;
    }
    switch(data->datatype)
    {
        case APE_OBJECT_FREED:
            {
                //APE_ASSERT(false);
                return;
            }
            break;
        case APE_OBJECT_STRING:
            {
                //ape_allocator_free(&ctx->alloc, data->valstring.valalloc);
                ds_destroy(data->valstring.valalloc, ctx);
                //data->valstring.valalloc = NULL;
            }
            break;
        case APE_OBJECT_SCRIPTFUNCTION:
            {
                if(data->valscriptfunc.owns_data)
                {
                    ape_allocator_free(&ctx->alloc, data->valscriptfunc.name);
                    ape_compresult_destroy(data->valscriptfunc.compiledcode);
                }
                ape_allocator_free(&ctx->alloc, data->valscriptfunc.freevals);
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
                ape_allocator_free(&ctx->alloc, data->valnatfunc.name);
            }
            break;
        case APE_OBJECT_EXTERNAL:
            {
                if(data->valextern.fndestroy)
                {
                    data->valextern.fndestroy(ctx, data->valextern.data);
                }
            }
            break;
        case APE_OBJECT_ERROR:
            {
                ape_allocator_free(&ctx->alloc, data->valerror.message);
                ape_traceback_destroy(data->valerror.traceback);
            }
            break;
        default:
            {
            }
            break;
    }
    data->datatype = APE_OBJECT_FREED;
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

char* ape_object_value_typeunionname(ApeContext* ctx, const ApeObjType type)
{
    bool in_between;
    ApeWriter* res;
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
    CHECK_TYPE(APE_OBJECT_FLOATNUMBER);
    CHECK_TYPE(APE_OBJECT_FIXEDNUMBER);
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

char* ape_object_value_serialize(ApeContext* ctx, ApeObject object, ApeSize* lendest)
{
    ApeSize l;
    char* string;
    ApeWriter* buf;
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

bool ape_object_extern_setdestroyfunc(ApeObject object, ApeDataCallback destroy_fn)
{
    ApeExternalData* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_EXTERNAL);
    data = ape_object_value_asexternal(object);
    if(!data)
    {
        return false;
    }
    data->fndestroy = destroy_fn;
    return true;
}


bool ape_object_extern_setdata(ApeObject object, void* ext_data)
{
    ApeExternalData* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_EXTERNAL);
    data = ape_object_value_asexternal(object);
    if(!data)
    {
        return false;
    }
    data->data = ext_data;
    return true;
}

bool ape_object_extern_setcopyfunc(ApeObject object, ApeDataCallback copy_fn)
{
    ApeExternalData* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_EXTERNAL);
    data = ape_object_value_asexternal(object);
    if(!data)
    {
        return false;
    }
    data->fncopy = copy_fn;
    return true;
}

ApeObject ape_object_getkvpairat(ApeContext* ctx, ApeObject object, int ix)
{
    ApeObject key;
    ApeObject val;
    ApeObject res;
    ApeObject key_obj;
    ApeObject val_obj;
    ApeGCObjData* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_MAP);
    data = ape_object_value_allocated_data(object);
    if(ix >= (ApeInt)ape_valdict_count(data->valmap))
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

ApeObject ape_object_value_internalcopydeep(ApeContext* ctx, ApeObject obj, ApeValDict* copies)
{
    ApeSize i;
    ApeSize len;
    bool ok;
    const char* str;
    ApeUShort* bytecode_copy;
    ApeScriptFunction* function_copy;
    ApeScriptFunction* function;
    ApePosition* src_positions_copy;
    ApeAstCompResult* comp_res_copy;
    ApeObject free_val;
    ApeObject free_val_copy;
    ApeObject item;
    ApeObject item_copy;
    ApeObject key;
    ApeObject val;
    ApeObject key_copy;
    ApeObject val_copy;
    ApeObject copy;
    ApeObject* copy_ptr;
    ApeObjType type;
    if(copies != NULL)
    {
        copy_ptr = (ApeObject*)ape_valdict_getbykey(copies, &obj);
        if(copy_ptr)
        {
            //ape_context_debugvalue(ctx, "internalcopydeep:copy_ptr", *copy_ptr);
            return *copy_ptr;
        }
    }
    copy = ape_object_make_null(ctx);
    type = ape_object_value_type(obj);
    //ape_context_debugvalue(ctx, "internalcopydeep:obj", obj);
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
        case APE_OBJECT_FLOATNUMBER:
        case APE_OBJECT_FIXEDNUMBER:
        case APE_OBJECT_BOOL:
        case APE_OBJECT_NULL:
        case APE_OBJECT_NATIVEFUNCTION:
            {
                copy.handle = obj.handle;
                copy.type = type;
                copy.handle->datatype = type;
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
                function = ape_object_value_asscriptfunction(obj);
                bytecode_copy = NULL;
                src_positions_copy = NULL;
                comp_res_copy = NULL;
                bytecode_copy = (ApeUShort*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeUShort) * function->compiledcode->count);
                if(!bytecode_copy)
                {
                    return ape_object_make_null(ctx);
                }
                memcpy(bytecode_copy, function->compiledcode->bytecode, sizeof(ApeUShort) * function->compiledcode->count);
                src_positions_copy = (ApePosition*)ape_allocator_alloc(&ctx->alloc, sizeof(ApePosition) * function->compiledcode->count);
                if(!src_positions_copy)
                {
                    ape_allocator_free(&ctx->alloc, bytecode_copy);
                    return ape_object_make_null(ctx);
                }
                memcpy(src_positions_copy, function->compiledcode->srcpositions, sizeof(ApePosition) * function->compiledcode->count);
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
                function_copy = ape_object_value_asscriptfunction(copy);
                function_copy->freevals = (ApeObject*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeObject) * function->numfreevals);
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
        case APE_OBJECT_NUMERIC:
            {
                APE_ASSERT(false);
            }
            break;
    }
    return copy;
}


ApeObject ape_object_value_copydeep(ApeContext* ctx, ApeObject obj)
{
    ApeObject res;
    ApeValDict* copies;
    copies = ape_make_valdict(ctx, sizeof(ApeObject), sizeof(ApeObject));
    if(!copies)
    {
        return ape_object_make_null(ctx);
    }
    res = ape_object_value_internalcopydeep(ctx, obj, copies);
    ape_valdict_destroy(copies);
    return res;
}

ApeObject ape_object_value_copyflat(ApeContext* ctx, ApeObject obj)
{
    ApeSize i;
    ApeSize len;
    bool ok;
    const char* str;
    void* data_copy;
    ApeObject item;
    ApeObject copy;
    ApeObject key;
    ApeObject val;
    ApeObjType type;
    ApeExternalData* external;
    copy = ape_object_make_null(ctx);
    type = ape_object_value_type(obj);
    //ape_context_debugvalue(ctx, "copyflat:obj", obj);
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
        case APE_OBJECT_FIXEDNUMBER:
            {
                copy = ape_object_make_fixednumber(ctx, ape_object_value_asfixednumber(obj));
            }
            break;
        case APE_OBJECT_FLOATNUMBER:
            {
                copy = ape_object_make_floatnumber(ctx, ape_object_value_asfloatnumber(obj));
            }
            break;
        case APE_OBJECT_BOOL:
            {
                copy = ape_object_make_bool(ctx, ape_object_value_asbool(obj));
            }
            break;
        case APE_OBJECT_NULL:
            {
                //copy = ape_object_make_floatnumber(ctx, ape_object_value_asnumber(obj));
                copy = ape_object_make_fixednumber(ctx, 0);
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
                len = ape_object_string_getlength(obj);
                copy = ape_object_make_stringlen(ctx, str, len);
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
                if(external->fncopy)
                {
                    data_copy = external->fncopy(ctx, external->data);
                }
                else
                {
                    data_copy = external->data;
                }
                ape_object_extern_setdata(copy, data_copy);
                ape_object_extern_setdestroyfunc(copy, external->fndestroy);
                ape_object_extern_setcopyfunc(copy, external->fncopy);
            }
            break;
        case APE_OBJECT_NUMERIC:
            {
                APE_ASSERT(false);
            }
            break;
    }
    return copy;
}


bool ape_object_value_wrapequals(const ApeObject* a_ptr, const ApeObject* b_ptr)
{
    ApeObject a = *a_ptr;
    ApeObject b = *b_ptr;
    return ape_object_value_equals(a, b);
}

unsigned long ape_object_value_hash(ApeObject* obj_ptr)
{
    bool bval;
    ApeFloat val;
    ApeObject obj;
    ApeObjType type;
    obj = *obj_ptr;
    type = ape_object_value_type(obj);
    switch(type)
    {
        case APE_OBJECT_FIXEDNUMBER:
        case APE_OBJECT_FLOATNUMBER:
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

ApeFloat ape_object_value_asnumerica(ApeObject obj, ApeObjType t)
{
    if(ape_object_type_isnumber(t))
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

ApeFloat ape_object_value_asnumeric(ApeObject obj)
{
    return ape_object_value_asnumerica(obj, ape_object_value_type(obj));
}

/*
* return value meanings:
* 0 === definitely equal
* -1 === definitely not equal
* n === as used in VM
*/
ApeFloat ape_object_value_compare(ApeObject a, ApeObject b, bool* out_ok)
{
    int a_len;
    int b_len;
    unsigned long a_hash;
    unsigned long b_hash;
    intptr_t a_data_val;
    intptr_t b_data_val;
    const char* a_string;
    const char* b_string;
    ApeObjType a_type;
    ApeObjType b_type;
    if((a.handle != NULL) && (b.handle != NULL))
    {
        if(a.handle == b.handle)
        {
            return 0;
        }
    }
    *out_ok = true;
    a_type = ape_object_value_type(a);
    b_type = ape_object_value_type(b);
    /* (null == null)*/
    if(ape_object_value_isnull(a) && ape_object_value_isnull(b))
    {
        return 0;
    }
    /* (val == null) - only null is null */
    if(!ape_object_value_isnull(a) && ape_object_value_isnull(b))
    {
        return -1;
    }
    /* (null == val) - only null is null, again */
    if(ape_object_value_isnull(a) && !ape_object_value_isnull(b))
    {
        return -1;
    }
    if(ape_object_type_isnumeric(a_type) && ape_object_type_isnumeric(b_type))
    {
        #if 1
            #if 0
                ApeFloat leftval = ape_object_value_asnumerica(a, a_type);
                ApeFloat rightval = ape_object_value_asnumerica(b, b_type);
            #else
                ApeFloat leftval = ape_object_value_asnumber(a);
                ApeFloat rightval = ape_object_value_asnumber(b);

            #endif
            return leftval - rightval;
        #else
            if(ape_object_value_isfixednumber(a) && ape_object_value_isfixednumber(b))
            {
                return (ape_object_value_asfixednumber(a) - ape_object_value_asfixednumber(b));
            }
            else if(ape_object_value_isfloatnumber(a) && ape_object_value_isfloatnumber(b))
            {
                return (ape_object_value_asfloatnumber(a) - ape_object_value_asfloatnumber(b));
            }
            else
            {
                return (ape_object_value_asnumerica(a, a_type) - ape_object_value_asnumerica(b, b_type));
            }
        #endif
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
        return (ApeFloat)(a_data_val - b_data_val);
    }
    else
    {
        *out_ok = false;
    }
    return 1;
}

bool ape_object_value_equals(ApeObject a, ApeObject b)
{
    bool ok;
    ApeFloat res;
    ApeObjType a_type;
    ApeObjType b_type;
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
