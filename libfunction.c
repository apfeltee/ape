
#include "ape.h"

ApeObject_t ape_object_make_function(ApeContext_t* ctx, const char* name, ApeCompResult_t* cres, bool wdata, ApeInt_t nloc, ApeInt_t nargs, ApeSize_t fvcount)
{
    ApeObjData_t* data;
    data = ape_gcmem_allocobjdata(ctx->mem, APE_OBJECT_SCRIPTFUNCTION);
    if(!data)
    {
        return ape_object_make_null(ctx);
    }
    if(wdata)
    {
        data->valscriptfunc.name = name ? ape_util_strdup(ctx, name) : ape_util_strdup(ctx, "anonymous");
        if(!data->valscriptfunc.name)
        {
            return ape_object_make_null(ctx);
        }
    }
    else
    {
        data->valscriptfunc.const_name = name ? name : "anonymous";
    }
    data->valscriptfunc.compiledcode = cres;
    data->valscriptfunc.owns_data = wdata;
    data->valscriptfunc.numlocals = nloc;
    data->valscriptfunc.numargs = nargs;
    if(((ApeInt_t)fvcount) >= 0)
    {
        data->valscriptfunc.freevals = (ApeObject_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeObject_t) * fvcount);
        if(!data->valscriptfunc.freevals)
        {
            return ape_object_make_null(ctx);
        }
    }
    data->valscriptfunc.numfreevals = fvcount;
    return object_make_from_data(ctx, APE_OBJECT_SCRIPTFUNCTION, data);
}

ApeObject_t ape_object_make_nativefuncmemory(ApeContext_t* ctx, const char* name, ApeNativeFuncPtr_t fn, void* data, ApeSize_t dlen)
{
    ApeObjData_t* obj;
    if(dlen > APE_CONF_SIZE_NATFN_MAXDATALEN)
    {
        return ape_object_make_null(ctx);
    }
    obj = ape_gcmem_allocobjdata(ctx->mem, APE_OBJECT_NATIVEFUNCTION);
    if(!obj)
    {
        return ape_object_make_null(ctx);
    }
    obj->valnatfunc.name = ape_util_strdup(ctx, name);
    if(!obj->valnatfunc.name)
    {
        return ape_object_make_null(ctx);
    }
    obj->valnatfunc.nativefnptr = fn;
    if(data)
    {
        #if 0
        memcpy(obj->valnatfunc.data, data, dlen);
        #else
        obj->valnatfunc.dataptr = data;
        #endif
    }
    obj->valnatfunc.datalen = dlen;
    return object_make_from_data(ctx, APE_OBJECT_NATIVEFUNCTION, obj);
}

const char* ape_object_function_getname(ApeObject_t obj)
{
    ApeObjData_t* data;
    APE_ASSERT(ape_object_value_type(obj) == APE_OBJECT_SCRIPTFUNCTION);
    data = ape_object_value_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return NULL;
    }
    if(data->valscriptfunc.owns_data)
    {
        return data->valscriptfunc.name;
    }
    return data->valscriptfunc.const_name;
}

ApeObject_t ape_object_function_getfreeval(ApeObject_t obj, ApeInt_t ix)
{
    ApeObjData_t* data;
    ApeScriptFunction_t* fun;
    APE_ASSERT(ape_object_value_type(obj) == APE_OBJECT_SCRIPTFUNCTION);
    data = ape_object_value_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return ape_object_make_null(data->context);
    }
    fun = &data->valscriptfunc;
    APE_ASSERT((ix >= 0) && (ix < (ApeInt_t)fun->numfreevals));
    if((ix < 0) || (ix >= (ApeInt_t)fun->numfreevals))
    {
        return ape_object_make_null(data->context);
    }
    return fun->freevals[ix];

}

void ape_object_function_setfreeval(ApeObject_t obj, ApeInt_t ix, ApeObject_t val)
{
    ApeObjData_t* data;
    ApeScriptFunction_t* fun;
    APE_ASSERT(ape_object_value_type(obj) == APE_OBJECT_SCRIPTFUNCTION);
    data = ape_object_value_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return;
    }
    fun = &data->valscriptfunc;
    APE_ASSERT((ix >= 0) && (ix < (ApeInt_t)fun->numfreevals));
    if((ix < 0) || (ix >= (ApeInt_t)fun->numfreevals))
    {
        return;
    }
    fun->freevals[ix] = val;
}



