
#include <math.h>
#include "ape.h"

static ApeObject_t cfn_sqrt(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat_t arg = ape_object_value_asnumber(args[0]);
    ApeFloat_t res = sqrt(arg);
    return ape_object_make_number(vm->context, res);
}

static ApeObject_t cfn_pow(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat_t arg1 = ape_object_value_asnumber(args[0]);
    ApeFloat_t arg2 = ape_object_value_asnumber(args[1]);
    ApeFloat_t res = pow(arg1, arg2);
    return ape_object_make_number(vm->context, res);
}

static ApeObject_t cfn_sin(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat_t arg = ape_object_value_asnumber(args[0]);
    ApeFloat_t res = sin(arg);
    return ape_object_make_number(vm->context, res);
}

static ApeObject_t cfn_cos(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat_t arg = ape_object_value_asnumber(args[0]);
    ApeFloat_t res = cos(arg);
    return ape_object_make_number(vm->context, res);
}

static ApeObject_t cfn_tan(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat_t arg = ape_object_value_asnumber(args[0]);
    ApeFloat_t res = tan(arg);
    return ape_object_make_number(vm->context, res);
}

static ApeObject_t cfn_log(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat_t arg = ape_object_value_asnumber(args[0]);
    ApeFloat_t res = log(arg);
    return ape_object_make_number(vm->context, res);
}

static ApeObject_t cfn_ceil(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat_t arg = ape_object_value_asnumber(args[0]);
    ApeFloat_t res = ceil(arg);
    return ape_object_make_number(vm->context, res);
}

static ApeObject_t cfn_floor(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat_t arg = ape_object_value_asnumber(args[0]);
    ApeFloat_t res = floor(arg);
    return ape_object_make_number(vm->context, res);
}

static ApeObject_t cfn_abs(ApeVM_t* vm, void* data, ApeSize_t argc, ApeObject_t* args)
{
    (void)data;
    if(!APE_CHECK_ARGS(vm, true, argc, args, APE_OBJECT_NUMBER))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat_t arg = ape_object_value_asnumber(args[0]);
    ApeFloat_t res = fabs(arg);
    return ape_object_make_number(vm->context, res);
}


void ape_builtins_install_math(ApeVM_t* vm)
{
    static ApeNativeItem_t g_core_mathfuncs[]=
    {
        { "sqrt", cfn_sqrt },
        { "pow", cfn_pow },
        { "sin", cfn_sin },
        { "cos", cfn_cos },
        { "tan", cfn_tan },
        { "log", cfn_log },
        { "ceil", cfn_ceil },
        { "floor", cfn_floor },
        { "abs", cfn_abs },
        {NULL, NULL},
    };
    ape_builtins_setup_namespace(vm, "Math", g_core_mathfuncs);
}

