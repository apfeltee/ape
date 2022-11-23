
#include "ape.h"

ApeObject_t
object_make_function(ApeGCMemory_t* mem, const char* name, ApeCompilationResult_t* comp_res, bool owns_data, int num_locals, int num_args, ApeSize_t free_vals_count)
{
    ApeObjectData_t* data;
    data = gcmem_alloc_object_data(mem, APE_OBJECT_FUNCTION);
    if(!data)
    {
        return object_make_null();
    }
    if(owns_data)
    {
        data->function.name = name ? util_strdup(mem->alloc, name) : util_strdup(mem->alloc, "anonymous");
        if(!data->function.name)
        {
            return object_make_null();
        }
    }
    else
    {
        data->function.const_name = name ? name : "anonymous";
    }
    data->function.comp_result = comp_res;
    data->function.owns_data = owns_data;
    data->function.num_locals = num_locals;
    data->function.num_args = num_args;
    if(free_vals_count >= APE_ARRAY_LEN(data->function.free_vals_buf))
    {
        data->function.free_vals_allocated = (ApeObject_t*)allocator_malloc(mem->alloc, sizeof(ApeObject_t) * free_vals_count);
        if(!data->function.free_vals_allocated)
        {
            return object_make_null();
        }
    }
    data->function.free_vals_count = free_vals_count;
    return object_make_from_data(APE_OBJECT_FUNCTION, data);
}

ApeObject_t object_make_native_function_memory(ApeGCMemory_t* mem, const char* name, ApeNativeFunc_t fn, void* data, int data_len)
{
    ApeObjectData_t* obj;
    if(data_len > NATIVE_FN_MAX_DATA_LEN)
    {
        return object_make_null();
    }
    obj = gcmem_alloc_object_data(mem, APE_OBJECT_NATIVE_FUNCTION);
    if(!obj)
    {
        return object_make_null();
    }
    obj->native_function.name = util_strdup(mem->alloc, name);
    if(!obj->native_function.name)
    {
        return object_make_null();
    }
    obj->native_function.native_funcptr = fn;
    if(data)
    {
        //memcpy(obj->native_function.data, data, data_len);
        obj->native_function.data = data;
    }
    obj->native_function.data_len = data_len;
    return object_make_from_data(APE_OBJECT_NATIVE_FUNCTION, obj);
}


bool function_freevalsallocated(ApeFunction_t* fun)
{
    return fun->free_vals_count >= APE_ARRAY_LEN(fun->free_vals_buf);
}

const char* object_function_getname(ApeObject_t obj)
{
    ApeObjectData_t* data;
    APE_ASSERT(object_value_type(obj) == APE_OBJECT_FUNCTION);
    data = object_value_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return NULL;
    }
    if(data->function.owns_data)
    {
        return data->function.name;
    }
    return data->function.const_name;
}

ApeObject_t object_function_getfreeval(ApeObject_t obj, int ix)
{
    ApeObjectData_t* data;
    ApeFunction_t* fun;
    APE_ASSERT(object_value_type(obj) == APE_OBJECT_FUNCTION);
    data = object_value_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return object_make_null();
    }
    fun = &data->function;
    APE_ASSERT(ix >= 0 && ix < fun->free_vals_count);
    if(ix < 0 || ix >= fun->free_vals_count)
    {
        return object_make_null();
    }
    if(function_freevalsallocated(fun))
    {
        return fun->free_vals_allocated[ix];
    }
    return fun->free_vals_buf[ix];
}

void object_set_function_free_val(ApeObject_t obj, int ix, ApeObject_t val)
{
    ApeObjectData_t* data;
    ApeFunction_t* fun;
    APE_ASSERT(object_value_type(obj) == APE_OBJECT_FUNCTION);
    data = object_value_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return;
    }
    fun = &data->function;
    APE_ASSERT(ix >= 0 && ix < fun->free_vals_count);
    if(ix < 0 || ix >= fun->free_vals_count)
    {
        return;
    }
    if(function_freevalsallocated(fun))
    {
        fun->free_vals_allocated[ix] = val;
    }
    else
    {
        fun->free_vals_buf[ix] = val;
    }
}



