
#include "ape.h"

bool object_value_isnumber(ApeObject_t o)
{
    return (o.handle & OBJECT_PATTERN) != OBJECT_PATTERN;
}


bool object_value_isnumeric(ApeObject_t obj)
{
    ApeObjectType_t type;
    type = object_value_type(obj);
    return type == APE_OBJECT_NUMBER || type == APE_OBJECT_BOOL;
}

bool object_value_isnull(ApeObject_t obj)
{
    return object_value_type(obj) == APE_OBJECT_NULL;
}

bool object_value_iscallable(ApeObject_t obj)
{
    ApeObjectType_t type;
    type = object_value_type(obj);
    return type == APE_OBJECT_NATIVE_FUNCTION || type == APE_OBJECT_FUNCTION;
}

bool object_value_isallocated(ApeObject_t object)
{
    return (object.handle & OBJECT_ALLOCATED_HEADER) == OBJECT_ALLOCATED_HEADER;
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

bool object_value_asbool(ApeObject_t obj)
{
    if(object_value_isnumber(obj))
    {
        return obj.handle;
    }
    return obj.handle & (~OBJECT_HEADER_MASK);
}

ApeFloat_t object_value_asnumber(ApeObject_t obj)
{
    if(object_value_isnumber(obj))
    {// todo: optimise? always return number?
        return obj.number;
    }
    return (ApeFloat_t)(obj.handle & (~OBJECT_HEADER_MASK));
}

ApeFunction_t* object_value_asfunction(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_FUNCTION);
    data = object_value_allocated_data(object);
    return &data->function;
}

ApeNativeFunction_t* object_value_asnativefunction(ApeObject_t obj)
{
    ApeObjectData_t* data;
    data = object_value_allocated_data(obj);
    return &data->native_function;
}

ApeExternalData_t* object_value_asexternal(ApeObject_t object)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_EXTERNAL);
    data = object_value_allocated_data(object);
    return &data->external;
}

ApeObjectType_t object_value_type(ApeObject_t obj)
{
    uint64_t tag;
    if(object_value_isnumber(obj))
    {
        return APE_OBJECT_NUMBER;
    }
    tag = (obj.handle >> 48) & 0x7;
    switch(tag)
    {
        case 0:
            {
                return APE_OBJECT_NONE;
            }
            break;
        case 1:
            {
                return APE_OBJECT_BOOL;
            }
            break;
        case 2:
            {
                return APE_OBJECT_NULL;
            }
            break;
        case 4:
            {
                ApeObjectData_t* data = object_value_allocated_data(obj);
                return data->type;
            }
            break;
        default:
            {
            }
            break;
    }
    return APE_OBJECT_NONE;
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

uint64_t object_value_typetag(ApeObjectType_t type)
{
    switch(type)
    {
        case APE_OBJECT_NONE:
            {
                return 0;
            }
            break;
        case APE_OBJECT_BOOL:
            {
                return 1;
            }
            break;
        case APE_OBJECT_NULL:
            {
                return 2;
            }
            break;
        default:
            {
            }
            break;
    }
    return 4;
}

ApeObject_t object_make_from_data(ApeObjectType_t type, ApeObjectData_t* data)
{
    uint64_t type_tag;
    ApeObject_t object;
    object.handle = OBJECT_PATTERN;
    type_tag = object_value_typetag(type) & 0x7;
    // assumes no pointer exceeds 48 bits
    object.handle |= (type_tag << 48);
    object.handle |= (uintptr_t)data;
    return object;
}

ApeObject_t object_make_number(ApeFloat_t val)
{
    ApeObject_t o = { .number = val };
    if((o.handle & OBJECT_PATTERN) == OBJECT_PATTERN)
    {
        o.handle = 0x7ff8000000000000;
    }
    return o;
}

ApeObject_t object_make_bool(bool val)
{
    return (ApeObject_t){ .handle = OBJECT_BOOL_HEADER | val };
}

ApeObject_t object_make_null()
{
    return (ApeObject_t){ .handle = OBJECT_NULL_PATTERN };
}

ApeObject_t object_make_external(ApeGCMemory_t* mem, void* data)
{
    ApeObjectData_t* obj;
    obj = gcmem_alloc_object_data(mem, APE_OBJECT_EXTERNAL);
    if(!obj)
    {
        return object_make_null();
    }
    obj->external.data = data;
    obj->external.data_destroy_fn = NULL;
    obj->external.data_copy_fn = NULL;
    return object_make_from_data(APE_OBJECT_EXTERNAL, obj);
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
                    compilation_result_destroy(data->function.comp_result);
                }
                if(function_freevalsallocated(&data->function))
                {
                    allocator_free(data->mem->alloc, data->function.free_vals_allocated);
                }
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


ApeGCMemory_t* object_value_getmem(ApeObject_t obj)
{
    ApeObjectData_t* data;
    data = object_value_allocated_data(obj);
    return data->mem;
}


#define CHECK_TYPE(t)                                    \
    do                                                   \
    {                                                    \
        if((type & t) == t)                              \
        {                                                \
            if(in_between)                               \
            {                                            \
                strbuf_append(res, "|");                 \
            }                                            \
            strbuf_append(res, object_value_typename(t)); \
            in_between = true;                           \
        }                                                \
    } while(0)

char* object_value_typeunionname(ApeAllocator_t* alloc, const ApeObjectType_t type)
{
    bool in_between;
    ApeStringBuffer_t* res;
    if(type == APE_OBJECT_ANY || type == APE_OBJECT_NONE || type == APE_OBJECT_FREED)
    {
        return util_strdup(alloc, object_value_typename(type));
    }
    res = strbuf_make(alloc);
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
    return strbuf_getstringanddestroy(res);
}

char* object_value_serialize(ApeAllocator_t* alloc, ApeObject_t object, ApeSize_t* lendest)
{
    ApeSize_t l;
    char* string;
    ApeStringBuffer_t* buf;
    buf = strbuf_make(alloc);
    if(!buf)
    {
        return NULL;
    }
    object_tostring(object, buf, true);
    l = buf->len;
    string = strbuf_getstringanddestroy(buf);
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

ApeObject_t object_get_kv_pair_at(ApeGCMemory_t* mem, ApeObject_t object, int ix)
{
    ApeObject_t key;
    ApeObject_t val;
    ApeObject_t res;
    ApeObject_t key_obj;
    ApeObject_t val_obj;
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(object) == APE_OBJECT_MAP);
    data = object_value_allocated_data(object);
    if(ix >= valdict_count(data->map))
    {
        return object_make_null();
    }
    key = object_map_getkeyat(object, ix);
    val = object_map_getvalueat(object, ix);
    res = object_make_map(mem);
    if(object_value_isnull(res))
    {
        return object_make_null();
    }
    key_obj = object_make_string(mem, "key");
    if(object_value_isnull(key_obj))
    {
        return object_make_null();
    }
    object_map_setvalue(res, key_obj, key);
    val_obj = object_make_string(mem, "value");
    if(object_value_isnull(val_obj))
    {
        return object_make_null();
    }
    object_map_setvalue(res, val_obj, val);
    return res;
}


// INTERNAL
ApeObject_t object_value_internal_copydeep(ApeGCMemory_t* mem, ApeObject_t obj, ApeValDictionary_t * copies)
{
    ApeSize_t i;
    int len;
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
    copy = object_make_null();
    type = object_value_type(obj);
    switch(type)
    {
        case APE_OBJECT_FREED:
        case APE_OBJECT_ANY:
        case APE_OBJECT_NONE:
            {
                APE_ASSERT(false);
                copy = object_make_null();
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
                copy = object_make_string(mem, str);
            }
            break;

        case APE_OBJECT_FUNCTION:
            {
                function = object_value_asfunction(obj);
                bytecode_copy = NULL;
                src_positions_copy = NULL;
                comp_res_copy = NULL;
                bytecode_copy = (ApeUShort_t*)allocator_malloc(mem->alloc, sizeof(ApeUShort_t) * function->comp_result->count);
                if(!bytecode_copy)
                {
                    return object_make_null();
                }
                memcpy(bytecode_copy, function->comp_result->bytecode, sizeof(ApeUShort_t) * function->comp_result->count);
                src_positions_copy = (ApePosition_t*)allocator_malloc(mem->alloc, sizeof(ApePosition_t) * function->comp_result->count);
                if(!src_positions_copy)
                {
                    allocator_free(mem->alloc, bytecode_copy);
                    return object_make_null();
                }
                memcpy(src_positions_copy, function->comp_result->src_positions, sizeof(ApePosition_t) * function->comp_result->count);
                // todo: add compilation result copy function
                comp_res_copy = compilation_result_make(mem->alloc, bytecode_copy, src_positions_copy, function->comp_result->count);
                if(!comp_res_copy)
                {
                    allocator_free(mem->alloc, src_positions_copy);
                    allocator_free(mem->alloc, bytecode_copy);
                    return object_make_null();
                }
                copy = object_make_function(mem, object_function_getname(obj), comp_res_copy, true, function->num_locals, function->num_args, 0);
                if(object_value_isnull(copy))
                {
                    compilation_result_destroy(comp_res_copy);
                    return object_make_null();
                }
                ok = valdict_set(copies, &obj, &copy);
                if(!ok)
                {
                    return object_make_null();
                }
                function_copy = object_value_asfunction(copy);
                if(function_freevalsallocated(function))
                {
                    function_copy->free_vals_allocated = (ApeObject_t*)allocator_malloc(mem->alloc, sizeof(ApeObject_t) * function->free_vals_count);
                    if(!function_copy->free_vals_allocated)
                    {
                        return object_make_null();
                    }
                }
                function_copy->free_vals_count = function->free_vals_count;
                for(i = 0; i < function->free_vals_count; i++)
                {
                    free_val = object_function_getfreeval(obj, i);
                    free_val_copy = object_value_internal_copydeep(mem, free_val, copies);
                    if(!object_value_isnull(free_val) && object_value_isnull(free_val_copy))
                    {
                        return object_make_null();
                    }
                    object_set_function_free_val(copy, i, free_val_copy);
                }
            }
            break;
        case APE_OBJECT_ARRAY:
            {
                len = object_array_getlength(obj);
                copy = object_make_arraycapacity(mem, len);
                if(object_value_isnull(copy))
                {
                    return object_make_null();
                }
                ok = valdict_set(copies, &obj, &copy);
                if(!ok)
                {
                    return object_make_null();
                }
                for(i = 0; i < len; i++)
                {
                    item = object_array_getvalue(obj, i);
                    item_copy = object_value_internal_copydeep(mem, item, copies);
                    if(!object_value_isnull(item) && object_value_isnull(item_copy))
                    {
                        return object_make_null();
                    }
                    ok = object_array_pushvalue(copy, item_copy);
                    if(!ok)
                    {
                        return object_make_null();
                    }
                }
            }
            break;
        case APE_OBJECT_MAP:
            {
                copy = object_make_map(mem);
                if(object_value_isnull(copy))
                {
                    return object_make_null();
                }
                ok = valdict_set(copies, &obj, &copy);
                if(!ok)
                {
                    return object_make_null();
                }
                for(i = 0; i < object_map_getlength(obj); i++)
                {
                    key = object_map_getkeyat(obj, i);
                    val = object_map_getvalueat(obj, i);
                    key_copy = object_value_internal_copydeep(mem, key, copies);
                    if(!object_value_isnull(key) && object_value_isnull(key_copy))
                    {
                        return object_make_null();
                    }
                    val_copy = object_value_internal_copydeep(mem, val, copies);
                    if(!object_value_isnull(val) && object_value_isnull(val_copy))
                    {
                        return object_make_null();
                    }
                    ok = object_map_setvalue(copy, key_copy, val_copy);
                    if(!ok)
                    {
                        return object_make_null();
                    }
                }
            }
            break;
        case APE_OBJECT_EXTERNAL:
            {
                copy = object_value_copyflat(mem, obj);
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


ApeObject_t object_value_copydeep(ApeGCMemory_t* mem, ApeObject_t obj)
{
    ApeObject_t res;
    ApeValDictionary_t* copies;
    copies = valdict_make(mem->alloc, ApeObject_t, ApeObject_t);
    if(!copies)
    {
        return object_make_null();
    }
    res = object_value_internal_copydeep(mem, obj, copies);
    valdict_destroy(copies);
    return res;
}

ApeObject_t object_value_copyflat(ApeGCMemory_t* mem, ApeObject_t obj)
{
    ApeSize_t i;
    int len;
    bool ok;
    const char* str;
    void* data_copy;
    ApeObject_t item;
    ApeObject_t copy;
    ApeObject_t key;
    ApeObject_t val;
    ApeObjectType_t type;
    ApeExternalData_t* external;
    copy = object_make_null();
    type = object_value_type(obj);
    switch(type)
    {
        case APE_OBJECT_ANY:
        case APE_OBJECT_FREED:
        case APE_OBJECT_NONE:
            {
                APE_ASSERT(false);
                copy = object_make_null();
            }
            break;
        case APE_OBJECT_NUMBER:
        case APE_OBJECT_BOOL:
        case APE_OBJECT_NULL:
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
                copy = object_make_string(mem, str);
            }
            break;
        case APE_OBJECT_ARRAY:
            {
                len = object_array_getlength(obj);
                copy = object_make_arraycapacity(mem, len);
                if(object_value_isnull(copy))
                {
                    return object_make_null();
                }
                for(i = 0; i < len; i++)
                {
                    item = object_array_getvalue(obj, i);
                    ok = object_array_pushvalue(copy, item);
                    if(!ok)
                    {
                        return object_make_null();
                    }
                }
            }
            break;
        case APE_OBJECT_MAP:
            {
                copy = object_make_map(mem);
                for(i = 0; i < object_map_getlength(obj); i++)
                {
                    key = (ApeObject_t)object_map_getkeyat(obj, i);
                    val = (ApeObject_t)object_map_getvalueat(obj, i);
                    ok = object_map_setvalue(copy, key, val);
                    if(!ok)
                    {
                        return object_make_null();
                    }
                }
            }
            break;
        case APE_OBJECT_EXTERNAL:
            {
                copy = object_make_external(mem, NULL);
                if(object_value_isnull(copy))
                {
                    return object_make_null();
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
        leftval = object_value_asnumber(a);
        rightval = object_value_asnumber(b);
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

ApeObjectData_t* object_value_allocated_data(ApeObject_t object)
{
    APE_ASSERT(object_value_isallocated(object) || object_value_type(object) == APE_OBJECT_NULL);
    return (ApeObjectData_t*)(object.handle & ~OBJECT_HEADER_MASK);
}
