
#include "inline.h"

ApeObject ape_object_make_function(ApeContext* ctx, const char* name, ApeAstCompResult* cres, bool wdata, ApeInt nloc, ApeInt nargs, ApeSize fvcount)
{
    ApeSize actualfvcount;
    ApeGCObjData* data;
    data = ape_object_make_objdata(ctx, APE_OBJECT_SCRIPTFUNCTION);
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
    if(((ApeInt)fvcount) > 0)
    {
        actualfvcount = fvcount;
        if(fvcount == 0)
        {
            actualfvcount = 1;
        }
        //fprintf(stderr, "ape_object_make_function: sizeof(ApeObject)=%d actualfvcount=%d rs=%d\n", sizeof(ApeObject), actualfvcount, sizeof(ApeObject)*actualfvcount);
        data->valscriptfunc.freevals = (ApeObject*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeObject) * actualfvcount);
        if(!data->valscriptfunc.freevals)
        {
            return ape_object_make_null(ctx);
        }
    }
    data->valscriptfunc.numfreevals = fvcount;
    return object_make_from_data(ctx, APE_OBJECT_SCRIPTFUNCTION, data);
}

ApeObject ape_object_make_nativefuncmemory(ApeContext* ctx, const char* name, ApeNativeFuncPtr fn, void* data, ApeSize dlen)
{
    ApeGCObjData* obj;
    if(dlen > APE_CONF_SIZE_NATFN_MAXDATALEN)
    {
        return ape_object_make_null(ctx);
    }
    obj = ape_object_make_objdata(ctx, APE_OBJECT_NATIVEFUNCTION);
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

const char* ape_object_function_getname(ApeObject obj)
{
    ApeGCObjData* data;
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

ApeObject ape_object_function_getfreeval(ApeObject obj, ApeInt ix)
{
    ApeGCObjData* data;
    ApeScriptFunction* fun;
    APE_ASSERT(ape_object_value_type(obj) == APE_OBJECT_SCRIPTFUNCTION);
    data = ape_object_value_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return ape_object_make_null(data->context);
    }
    fun = &data->valscriptfunc;
    APE_ASSERT((ix >= 0) && (ix < (ApeInt)fun->numfreevals));
    if((ix < 0) || (ix >= (ApeInt)fun->numfreevals))
    {
        return ape_object_make_null(data->context);
    }
    return fun->freevals[ix];

}

void ape_object_function_setfreeval(ApeObject obj, ApeInt ix, ApeObject val)
{
    ApeGCObjData* data;
    ApeScriptFunction* fun;
    APE_ASSERT(ape_object_value_type(obj) == APE_OBJECT_SCRIPTFUNCTION);
    data = ape_object_value_allocated_data(obj);
    APE_ASSERT(data);
    if(!data)
    {
        return;
    }
    fun = &data->valscriptfunc;
    APE_ASSERT((ix >= 0) && (ix < (ApeInt)fun->numfreevals));
    if((ix < 0) || (ix >= (ApeInt)fun->numfreevals))
    {
        return;
    }
    fun->freevals[ix] = val;
}



