
#include <math.h>
#include "inline.h"

static ApeObject cfn_sqrt(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    (void)data;
    ApeArgCheck check;
    ape_args_init(vm, &check, "sqrt", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat arg = ape_object_value_asnumber(args[0]);
    ApeFloat res = sqrt(arg);
    return ape_object_make_floatnumber(vm->context, res);
}

static ApeObject cfn_pow(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    (void)data;
    ApeArgCheck check;
    ape_args_init(vm, &check, "pow", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC) && !ape_args_check(&check, 1, APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat arg1 = ape_object_value_asnumber(args[0]);
    ApeFloat arg2 = ape_object_value_asnumber(args[1]);
    ApeFloat res = pow(arg1, arg2);
    return ape_object_make_floatnumber(vm->context, res);
}

static ApeObject cfn_sin(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    (void)data;
    ApeArgCheck check;
    ape_args_init(vm, &check, "sin", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat arg = ape_object_value_asnumber(args[0]);
    ApeFloat res = sin(arg);
    return ape_object_make_floatnumber(vm->context, res);
}

static ApeObject cfn_cos(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    (void)data;
    ApeArgCheck check;
    ape_args_init(vm, &check, "cos", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat arg = ape_object_value_asnumber(args[0]);
    ApeFloat res = cos(arg);
    return ape_object_make_floatnumber(vm->context, res);
}

static ApeObject cfn_tan(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    (void)data;
    ApeArgCheck check;
    ape_args_init(vm, &check, "tan", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat arg = ape_object_value_asnumber(args[0]);
    ApeFloat res = tan(arg);
    return ape_object_make_floatnumber(vm->context, res);
}

static ApeObject cfn_log(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    (void)data;
    ApeArgCheck check;
    ape_args_init(vm, &check, "log", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat arg = ape_object_value_asnumber(args[0]);
    ApeFloat res = log(arg);
    return ape_object_make_floatnumber(vm->context, res);
}

static ApeObject cfn_ceil(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    (void)data;
    ApeArgCheck check;
    ape_args_init(vm, &check, "ceil", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat arg = ape_object_value_asnumber(args[0]);
    ApeFloat res = ceil(arg);
    return ape_object_make_floatnumber(vm->context, res);
}

static ApeObject cfn_floor(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    (void)data;
    ApeArgCheck check;
    ape_args_init(vm, &check, "floor", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat arg = ape_object_value_asnumber(args[0]);
    ApeFloat res = floor(arg);
    return ape_object_make_floatnumber(vm->context, res);
}

static ApeObject cfn_abs(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    (void)data;
    ApeArgCheck check;
    ape_args_init(vm, &check, "abs", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);
    }
    ApeFloat arg = ape_object_value_asnumber(args[0]);
    ApeFloat res = fabs(arg);
    return ape_object_make_floatnumber(vm->context, res);
}


void ape_builtins_install_math(ApeVM* vm)
{
    static ApeNativeItem g_core_mathfuncs[]=
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

