
#include "ape.h"

ApeObject_t
ape_object_make_function(ApeContext_t* ctx, const char* name, ApeCompilationResult_t* comp_res, bool owns_data, ApeInt_t num_locals, ApeInt_t num_args, ApeSize_t free_vals_count)
{
    ApeObjectData_t* data;
    data = ape_gcmem_allocobjdata(ctx->mem, APE_OBJECT_FUNCTION);
    if(!data)
    {
        return ape_object_make_null(ctx);
    }
    if(owns_data)
    {
        data->function.name = name ? ape_util_strdup(ctx, name) : ape_util_strdup(ctx, "anonymous");
        if(!data->function.name)
        {
            return ape_object_make_null(ctx);
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
    if(free_vals_count >= 0)
    {
        data->function.free_vals_allocated = (ApeObject_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeObject_t) * free_vals_count);
        if(!data->function.free_vals_allocated)
        {
            return ape_object_make_null(ctx);
        }
    }
    data->function.free_vals_count = free_vals_count;
    return object_make_from_data(ctx, APE_OBJECT_FUNCTION, data);
}

ApeObject_t ape_object_make_nativefuncmemory(ApeContext_t* ctx, const char* name, ApeNativeFunc_t fn, void* data, ApeSize_t data_len)
{
    ApeObjectData_t* obj;
    if(data_len > APE_CONF_SIZE_NATFN_MAXDATALEN)
    {
        return ape_object_make_null(ctx);
    }
    obj = ape_gcmem_allocobjdata(ctx->mem, APE_OBJECT_NATIVE_FUNCTION);
    if(!obj)
    {
        return ape_object_make_null(ctx);
    }
    obj->native_function.name = ape_util_strdup(ctx, name);
    if(!obj->native_function.name)
    {
        return ape_object_make_null(ctx);
    }
    obj->native_function.native_funcptr = fn;
    if(data)
    {
        //memcpy(obj->native_function.data, data, data_len);
        obj->native_function.data = data;
    }
    obj->native_function.data_len = data_len;
    return object_make_from_data(ctx, APE_OBJECT_NATIVE_FUNCTION, obj);
}



const char* ape_object_function_getname(ApeObject_t obj)
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

ApeObject_t ape_object_function_getfreeval(ApeObject_t obj, ApeInt_t ix)
{
    ApeObjectData_t* data;
    ApeFunction_t* fun;
    APE_ASSERT(object_value_type(obj) == APE_OBJECT_FUNCTION);
    data = object_value_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return ape_object_make_null(data->context);
    }
    fun = &data->function;
    APE_ASSERT((ix >= 0) && (ix < (ApeInt_t)fun->free_vals_count));
    if((ix < 0) || (ix >= (ApeInt_t)fun->free_vals_count))
    {
        return ape_object_make_null(data->context);
    }
    return fun->free_vals_allocated[ix];

}

void ape_object_function_setfreeval(ApeObject_t obj, ApeInt_t ix, ApeObject_t val)
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
    APE_ASSERT((ix >= 0) && (ix < (ApeInt_t)fun->free_vals_count));
    if((ix < 0) || (ix >= (ApeInt_t)fun->free_vals_count))
    {
        return;
    }
    fun->free_vals_allocated[ix] = val;
}



