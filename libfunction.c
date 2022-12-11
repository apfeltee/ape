
#include "inline.h"

const char* ape_object_function_getname(ApeObject_t obj)
{
    ApeGCObjData_t* data;
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
    ApeGCObjData_t* data;
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
    ApeGCObjData_t* data;
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



