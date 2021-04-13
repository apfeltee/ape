#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <math.h>

#ifndef APE_AMALGAMATED
#include "vm.h"

#include "code.h"
#include "compiler.h"
#include "traceback.h"
#include "builtins.h"
#include "gc.h"
#endif

static void set_sp(vm_t *vm, int new_sp);
static void stack_push(vm_t *vm, object_t obj);
static object_t stack_pop(vm_t *vm);
static object_t stack_get(vm_t *vm, int nth_item);

static void this_stack_push(vm_t *vm, object_t obj);
static object_t this_stack_pop(vm_t *vm);
static object_t this_stack_get(vm_t *vm, int nth_item);

static bool push_frame(vm_t *vm, frame_t frame);
static bool pop_frame(vm_t *vm);
static void run_gc(vm_t *vm, array(object_t) *constants);
static bool call_object(vm_t *vm, object_t callee, int num_args);
static object_t call_native_function(vm_t *vm, object_t callee, src_pos_t src_pos, int argc, object_t *args);
static bool check_assign(vm_t *vm, object_t old_value, object_t new_value);
static bool try_overload_operator(vm_t *vm, object_t left, object_t right, opcode_t op, bool *out_overload_found);

vm_t *vm_make(const ape_config_t *config, gcmem_t *mem, ptrarray(errortag_t) *errors) {
    vm_t *vm = ape_malloc(sizeof(vm_t));
    memset(vm, 0, sizeof(vm_t));
    vm->config = config;
    vm->mem = mem;
    vm->globals_count = 0;
    vm->sp = 0;
    vm->this_sp = 0;
    vm->frames_count = 0;
    vm->native_functions = array_make(object_t);
    vm->errors = errors;
    vm->runtime_error = NULL;
    vm->last_popped = object_make_null();
    vm->running = false;

    for (int i = 0; i < builtins_count(); i++) {
        object_t builtin = object_make_native_function(vm->mem, builtins_get_name(i), builtins_get_fn(i), vm);
        array_add(vm->native_functions, &builtin);
    }

    for (int i = 0; i < OPCODE_MAX; i++) {
        vm->operator_oveload_keys[i] = object_make_null();
    }
#define SET_OPERATOR_OVERLOAD_KEY(op, key) do {\
    object_t key_obj = object_make_string(vm->mem, key);\
    vm->operator_oveload_keys[op] = key_obj;\
} while (0)
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_ADD,     "__operator_add__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_SUB,     "__operator_sub__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_MUL,     "__operator_mul__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_DIV,     "__operator_div__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_MOD,     "__operator_mod__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_OR,      "__operator_or__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_XOR,     "__operator_xor__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_AND,     "__operator_and__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_LSHIFT,  "__operator_lshift__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_RSHIFT,  "__operator_rshift__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_MINUS,   "__operator_minus__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_BANG,    "__operator_bang__");
    SET_OPERATOR_OVERLOAD_KEY(OPCODE_COMPARE, "__cmp__");
#undef SET_OPERATOR_OVERLOAD_KEY

    return vm;
}

void vm_destroy(vm_t *vm) {
    if (!vm) {
        return;
    }
    array_destroy(vm->native_functions);
    ape_free(vm);
}

void vm_reset(vm_t *vm) {
    vm->sp = 0;
    vm->this_sp = 0;
    while (vm->frames_count > 0) {
        pop_frame(vm);
    }
}

bool vm_run(vm_t *vm, compilation_result_t *comp_res, array(object_t) *constants) {
    int old_sp = vm->sp;
    int old_this_sp = vm->this_sp;
    int old_frames_count = vm->frames_count;
    object_t main_fn = object_make_function(vm->mem, "main", comp_res, false, 0, 0, 0);
    stack_push(vm, main_fn);
    bool res = vm_execute_function(vm, main_fn, constants);
    while (vm->frames_count > old_frames_count) {
        pop_frame(vm);
    }
    APE_ASSERT(vm->sp == old_sp);
    vm->this_sp = old_this_sp;
    return res;
}

object_t vm_call(vm_t *vm, array(object_t) *constants, object_t callee, int argc, object_t *args) {
    object_type_t type = object_get_type(callee);
    if (type == OBJECT_FUNCTION) {
        int old_sp = vm->sp;
        int old_this_sp = vm->this_sp;
        int old_frames_count = vm->frames_count;
        stack_push(vm, callee);
        for (int i = 0; i < argc; i++) {
            stack_push(vm, args[i]);
        }
        bool ok = vm_execute_function(vm, callee, constants);
        if (!ok) {
            return object_make_null();
        }
        while (vm->frames_count > old_frames_count) {
            pop_frame(vm);
        }
        APE_ASSERT(vm->sp == old_sp);
        vm->this_sp = old_this_sp;
        return vm_get_last_popped(vm);
    } else if (type == OBJECT_NATIVE_FUNCTION) {
        return call_native_function(vm, callee, src_pos_invalid, argc, args);
    } else {
        errortag_t *err = error_make(ERROR_USER, src_pos_invalid, "Object is not callable");
        ptrarray_add(vm->errors, err);
        return object_make_null();
    }
}

bool vm_execute_function(vm_t *vm, object_t function, array(object_t) *constants) {
    if (vm->running) {
        errortag_t *err = error_make(ERROR_USER, src_pos_invalid, "VM is already executing code");
        ptrarray_add(vm->errors, err);
        return false;
    }

    function_t *function_function = object_get_function(function); // naming is hard
    frame_t new_frame;
    frame_init(&new_frame, function, vm->sp - function_function->num_args);
    bool ok = push_frame(vm, new_frame);
    if (!ok) {
        errortag_t *err = error_make(ERROR_USER, src_pos_invalid, "Pushing frame failed");
        ptrarray_add(vm->errors, err);
        return false;
    }

    vm->running = true;
    vm->last_popped = object_make_null();

    int ticks_between_gc = 0;
    if (vm->config) {
        ticks_between_gc = vm->config->gc_interval;
    };

    int ticks_since_gc = 0;

    while (vm->current_frame->ip < vm->current_frame->bytecode_size) {
        opcode_val_t opcode = frame_read_opcode(vm->current_frame);
        switch (opcode) {
            case OPCODE_CONSTANT: {
                uint16_t constant_ix = frame_read_uint16(vm->current_frame);
                object_t *constant = array_get(constants, constant_ix);
                if (!constant) {
                    errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "Constant at %d not found", constant_ix);
                    vm_set_runtime_error(vm, err);
                    goto err;
                }
                stack_push(vm, *constant);
                break;
            }
            case OPCODE_ADD:
            case OPCODE_SUB:
            case OPCODE_MUL:
            case OPCODE_DIV:
            case OPCODE_MOD:
            case OPCODE_OR:
            case OPCODE_XOR:
            case OPCODE_AND:
            case OPCODE_LSHIFT:
            case OPCODE_RSHIFT:
            {
                object_t right = stack_pop(vm);
                object_t left = stack_pop(vm);
                object_type_t left_type = object_get_type(left);
                object_type_t right_type = object_get_type(right);
                if (object_is_numeric(left) && object_is_numeric(right)) {
                    double right_val = object_get_number(right);
                    double left_val = object_get_number(left);

                    int64_t left_val_int = left_val;
                    int64_t right_val_int = right_val;

                    double res = 0;
                    switch (opcode) {
                        case OPCODE_ADD:    res = left_val + right_val; break;
                        case OPCODE_SUB:    res = left_val - right_val; break;
                        case OPCODE_MUL:    res = left_val * right_val; break;
                        case OPCODE_DIV:    res = left_val / right_val; break;
                        case OPCODE_MOD:    res = fmod(left_val, right_val); break;
                        case OPCODE_OR:     res = left_val_int | right_val_int; break;
                        case OPCODE_XOR:    res = left_val_int ^ right_val_int; break;
                        case OPCODE_AND:    res = left_val_int & right_val_int; break;
                        case OPCODE_LSHIFT: res = left_val_int << right_val_int; break;
                        case OPCODE_RSHIFT: res = left_val_int >> right_val_int; break;
                        default: APE_ASSERT(false); break;
                    }
                    stack_push(vm, object_make_number(res));
                } else if (left_type == OBJECT_STRING  && right_type == OBJECT_STRING && opcode == OPCODE_ADD) {
                    const char* right_val = object_get_string(right);
                    const char* left_val = object_get_string(left);
                    object_t res_obj = object_make_stringf(vm->mem, "%s%s", left_val, right_val);
                    stack_push(vm, res_obj);
                } else {
                    bool overload_found = false;
                    bool ok = try_overload_operator(vm, left, right, opcode, &overload_found);
                    if (!ok) {
                        goto err;
                    }
                    if (!overload_found) {
                        const char *opcode_name = opcode_get_name(opcode);
                        const char *left_type_name = object_get_type_name(left_type);
                        const char *right_type_name = object_get_type_name(right_type);
                        errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                                   "Invalid operand types for %s, got %s and %s",
                                                   opcode_name, left_type_name, right_type_name);
                        vm_set_runtime_error(vm, err);
                        goto err;
                    }
                }
                break;
            }
            case OPCODE_POP: {
                stack_pop(vm);
                break;
            }
            case OPCODE_TRUE: {
                stack_push(vm, object_make_bool(true));
                break;
            }
            case OPCODE_FALSE: {
                stack_push(vm, object_make_bool(false));
                break;
            }
            case OPCODE_COMPARE: {
                object_t right = stack_pop(vm);
                object_t left = stack_pop(vm);
                bool is_overloaded = false;
                bool ok = try_overload_operator(vm, left, right, OPCODE_COMPARE, &is_overloaded);
                if (!ok) {
                    goto err;
                }
                if (!is_overloaded) {
                    double comparison_res = object_compare(left, right);
                    object_t res = object_make_number(comparison_res);
                    stack_push(vm, res);
                }
                break;
            }
            case OPCODE_EQUAL:
            case OPCODE_NOT_EQUAL:
            case OPCODE_GREATER_THAN:
            case OPCODE_GREATER_THAN_EQUAL:
            {
                object_t value = stack_pop(vm);
                double comparison_res = object_get_number(value);
                bool res_val = false;
                switch (opcode) {
                    case OPCODE_EQUAL: res_val = APE_DBLEQ(comparison_res, 0); break;
                    case OPCODE_NOT_EQUAL: res_val = !APE_DBLEQ(comparison_res, 0); break;
                    case OPCODE_GREATER_THAN: res_val = comparison_res > 0; break;
                    case OPCODE_GREATER_THAN_EQUAL: {
                        res_val = comparison_res > 0 || APE_DBLEQ(comparison_res, 0);
                        break;
                    }
                    default: APE_ASSERT(false); break;
                }
                object_t res = object_make_bool(res_val);
                stack_push(vm, res);
                break;
            }
            case OPCODE_MINUS:
            {
                object_t operand = stack_pop(vm);
                object_type_t operand_type = object_get_type(operand);
                if (operand_type == OBJECT_NUMBER) {
                    double val = object_get_number(operand);
                    object_t res = object_make_number(-val);
                    stack_push(vm, res);
                } else {
                    bool overload_found = false;
                    bool ok = try_overload_operator(vm, operand, object_make_null(), OPCODE_MINUS, &overload_found);
                    if (!ok) {
                        goto err;
                    }
                    if (!overload_found) {
                        const char *operand_type_string = object_get_type_name(operand_type);
                        errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                                   "Invalid operand type for MINUS, got %s",
                                                   operand_type_string);
                        vm_set_runtime_error(vm, err);
                        goto err;
                    }
                }
                break;
            }
            case OPCODE_BANG: {
                object_t operand = stack_pop(vm);
                object_type_t type = object_get_type(operand);
                if (type == OBJECT_BOOL) {
                    object_t res = object_make_bool(!object_get_bool(operand));
                    stack_push(vm, res);
                } else if (type == OBJECT_NULL) {
                    object_t res = object_make_bool(true);
                    stack_push(vm, res);
                } else {
                    bool overload_found = false;
                    bool ok = try_overload_operator(vm, operand, object_make_null(), OPCODE_BANG, &overload_found);
                    if (!ok) {
                        goto err;
                    }
                    if (!overload_found) {
                        object_t res = object_make_bool(false);
                        stack_push(vm, res);
                    }
                }
                break;
            }
            case OPCODE_JUMP: {
                uint16_t pos = frame_read_uint16(vm->current_frame);
                vm->current_frame->ip = pos;
                break;
            }
            case OPCODE_JUMP_IF_FALSE: {
                uint16_t pos = frame_read_uint16(vm->current_frame);
                object_t test = stack_pop(vm);
                if (!object_get_bool(test)) {
                    vm->current_frame->ip = pos;
                }
                break;
            }
            case OPCODE_JUMP_IF_TRUE: {
                uint16_t pos = frame_read_uint16(vm->current_frame);
                object_t test = stack_pop(vm);
                if (object_get_bool(test)) {
                    vm->current_frame->ip = pos;
                }
                break;
            }
            case OPCODE_NULL: {
                stack_push(vm, object_make_null());
                break;
            }
            case OPCODE_DEFINE_GLOBAL: {
                uint16_t ix = frame_read_uint16(vm->current_frame);
                object_t value = stack_pop(vm);
                vm_set_global(vm, ix, value);
                break;
            }
            case OPCODE_SET_GLOBAL: {
                uint16_t ix = frame_read_uint16(vm->current_frame);
                object_t new_value = stack_pop(vm);
                object_t old_value = vm_get_global(vm, ix);
                if (!check_assign(vm, old_value, new_value)) {
                    goto err;
                }
                vm_set_global(vm, ix, new_value);
                break;
            }
            case OPCODE_GET_GLOBAL: {
                uint16_t ix = frame_read_uint16(vm->current_frame);
                object_t global = vm->globals[ix];
                stack_push(vm, global);
                break;
            }
            case OPCODE_ARRAY: {
                uint16_t count = frame_read_uint16(vm->current_frame);
                object_t array_obj = object_make_array_with_capacity(vm->mem, count);
                object_t *items = vm->stack + vm->sp - count;
                for (int i = 0; i < count; i++) {
                    object_t item = items[i];
                    object_add_array_value(array_obj, item);
                }
                set_sp(vm, vm->sp - count);
                stack_push(vm, array_obj);
                break;
            }
            case OPCODE_MAP_START: {
                uint16_t count = frame_read_uint16(vm->current_frame);
                object_t map_obj = object_make_map_with_capacity(vm->mem, count);
                this_stack_push(vm, map_obj);
                break;
            }
            case OPCODE_MAP_END: {
                uint16_t count = frame_read_uint16(vm->current_frame);
                object_t map_obj = this_stack_pop(vm);
                object_t *kvpairs = vm->stack + vm->sp - count;
                for (int i = 0; i < count; i += 2) {
                    object_t key = kvpairs[i];
                    if (!object_is_hashable(key)) {
                        object_type_t key_type = object_get_type(key);
                        const char *key_type_name = object_get_type_name(key_type);
                        errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                                   "Key of type %s is not hashable", key_type_name);
                        vm_set_runtime_error(vm, err);
                        goto err;
                    }

                    object_t val = kvpairs[i + 1];
                    object_set_map_value(map_obj, key, val);
                }
                set_sp(vm, vm->sp - count);
                stack_push(vm, map_obj);
                break;
            }
            case OPCODE_GET_THIS: {
                object_t obj = this_stack_get(vm, 0);
                stack_push(vm, obj);
                break;
            }
            case OPCODE_GET_INDEX: {
                object_t index = stack_pop(vm);
                object_t left = stack_pop(vm);
                object_type_t left_type = object_get_type(left);
                object_type_t index_type = object_get_type(index);
                const char *left_type_name = object_get_type_name(left_type);
                const char *index_type_name = object_get_type_name(index_type);

                if (left_type != OBJECT_ARRAY && left_type != OBJECT_MAP && left_type != OBJECT_STRING) {
                    errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "Type %s is not indexable", left_type_name);
                    vm_set_runtime_error(vm, err);
                    goto err;
                }

                object_t res = object_make_null();

                if (left_type == OBJECT_ARRAY) {
                    if (index_type != OBJECT_NUMBER) {
                        errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                                  "Cannot index %s with %s", left_type_name, index_type_name);
                        vm_set_runtime_error(vm, err);
                        goto err;
                    }
                    int ix = (int)object_get_number(index);
                    if (ix < 0) {
                        ix = object_get_array_length(left) + ix;
                    }
                    if (ix >= 0 && ix < object_get_array_length(left)) {
                        res = object_get_array_value_at(left, ix);
                    }
                } else if (left_type == OBJECT_MAP) {
                    res = object_get_map_value(left, index);
                } else if (left_type == OBJECT_STRING) {
                    const char *str = object_get_string(left);
                    int ix = (int)object_get_number(index);
                    if (ix >= 0 && ix < (int)strlen(str)) {
                        char res_str[2] = {str[ix], '\0'};
                        res = object_make_string(vm->mem, res_str);
                    }
                }
                stack_push(vm, res);
                break;
            }
            case OPCODE_GET_VALUE_AT: {
                object_t index = stack_pop(vm);
                object_t left = stack_pop(vm);
                object_type_t left_type = object_get_type(left);
                object_type_t index_type = object_get_type(index);
                const char *left_type_name = object_get_type_name(left_type);
                const char *index_type_name = object_get_type_name(index_type);

                if (left_type != OBJECT_ARRAY && left_type != OBJECT_MAP && left_type != OBJECT_STRING) {
                    errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                               "Type %s is not indexable", left_type_name);
                    vm_set_runtime_error(vm, err);
                    goto err;
                }

                object_t res = object_make_null();
                if (index_type != OBJECT_NUMBER) {
                    errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                               "Cannot index %s with %s", left_type_name, index_type_name);
                    vm_set_runtime_error(vm, err);
                    goto err;
                }
                int ix = (int)object_get_number(index);

                if (left_type == OBJECT_ARRAY) {
                    res = object_get_array_value_at(left, ix);
                } else if (left_type == OBJECT_MAP) {
                    res = object_get_kv_pair_at(vm->mem, left, ix);
                } else if (left_type == OBJECT_STRING) {
                    const char *str = object_get_string(left);
                    int ix = (int)object_get_number(index);
                    if (ix >= 0 && ix < (int)strlen(str)) {
                        char res_str[2] = {str[ix], '\0'};
                        res = object_make_string(vm->mem, res_str);
                    }
                }
                stack_push(vm, res);
                break;
            }
            case OPCODE_CALL: {
                uint8_t num_args = frame_read_uint8(vm->current_frame);
                object_t callee = stack_get(vm, num_args);
                bool ok = call_object(vm, callee, num_args);
                if (!ok) {
                    goto err;
                }
                break;
            }
            case OPCODE_RETURN_VALUE: {
                object_t res = stack_pop(vm);
                bool ok = pop_frame(vm);
                if (!ok) {
                    goto end;
                }
                stack_push(vm, res);
                break;
            }
            case OPCODE_RETURN: {
                bool ok = pop_frame(vm);
                stack_push(vm, object_make_null());
                if (!ok) {
                    stack_pop(vm);
                    goto end;
                }
                break;
            }
            case OPCODE_DEFINE_LOCAL: {
                uint8_t pos = frame_read_uint8(vm->current_frame);
                vm->stack[vm->current_frame->base_pointer + pos] = stack_pop(vm);
                break;
            }
            case OPCODE_SET_LOCAL: {
                uint8_t pos = frame_read_uint8(vm->current_frame);
                object_t new_value = stack_pop(vm);
                object_t old_value = vm->stack[vm->current_frame->base_pointer + pos];
                if (!check_assign(vm, old_value, new_value)) {
                    goto err;
                }
                vm->stack[vm->current_frame->base_pointer + pos] = new_value;
                break;
            }
            case OPCODE_GET_LOCAL: {
                uint8_t pos = frame_read_uint8(vm->current_frame);
                object_t val = vm->stack[vm->current_frame->base_pointer + pos];
                stack_push(vm, val);
                break;
            }
            case OPCODE_GET_NATIVE_FUNCTION: {
                uint16_t ix = frame_read_uint16(vm->current_frame);
                object_t *val = array_get(vm->native_functions, ix);
                if (!val) {
                    errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame), "Native function %d not found", ix);
                    vm_set_runtime_error(vm, err);
                    goto err;
                }
                stack_push(vm, *val);
                break;
            }
            case OPCODE_FUNCTION: {
                uint16_t constant_ix = frame_read_uint16(vm->current_frame);
                uint8_t num_free = frame_read_uint8(vm->current_frame);
                object_t *constant = array_get(constants, constant_ix);
                if (!constant) {
                    errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame), "Constant %d not found", constant_ix);
                    vm_set_runtime_error(vm, err);
                    goto err;
                }
                object_type_t constant_type = object_get_type(*constant);
                if (constant_type != OBJECT_FUNCTION) {
                    const char *type_name = object_get_type_name(constant_type);
                    errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame), "%s is not a function", type_name);
                    vm_set_runtime_error(vm, err);
                    goto err;
                }

                const function_t *constant_function = object_get_function(*constant);
                object_t function_obj = object_make_function(vm->mem, object_get_function_name(*constant),
                                                            constant_function->comp_result, false,
                                                            constant_function->num_locals, constant_function->num_args,
                                                            num_free);
                for (int i = 0; i < num_free; i++) {
                    object_t free_val = vm->stack[vm->sp - num_free + i];
                    object_set_function_free_val(function_obj, i, free_val);
                }
                set_sp(vm, vm->sp - num_free);
                stack_push(vm, function_obj);
                break;
            }
            case OPCODE_GET_FREE: {
                uint8_t free_ix = frame_read_uint8(vm->current_frame);
                object_t val = object_get_function_free_val(vm->current_frame->function, free_ix);
                stack_push(vm, val);
                break;
            }
            case OPCODE_SET_FREE: {
                uint8_t free_ix = frame_read_uint8(vm->current_frame);
                object_t val = stack_pop(vm);
                object_set_function_free_val(vm->current_frame->function, free_ix, val);
                break;
            }
            case OPCODE_CURRENT_FUNCTION: {
                object_t current_function = vm->current_frame->function;
                stack_push(vm, current_function);
                break;
            }
            case OPCODE_SET_INDEX: {
                object_t index = stack_pop(vm);
                object_t left = stack_pop(vm);
                object_t new_value = stack_pop(vm);
                object_type_t left_type = object_get_type(left);
                object_type_t index_type = object_get_type(index);
                const char *left_type_name = object_get_type_name(left_type);
                const char *index_type_name = object_get_type_name(index_type);

                if (left_type != OBJECT_ARRAY && left_type != OBJECT_MAP) {
                    errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                              "Type %s is not indexable", left_type_name);
                    vm_set_runtime_error(vm, err);
                    goto err;
                }

                if (left_type == OBJECT_ARRAY) {
                    if (index_type != OBJECT_NUMBER) {
                        errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                                  "Cannot index %s with %s", left_type_name, index_type_name);
                        vm_set_runtime_error(vm, err);
                        goto err;
                    }
                    int ix = (int)object_get_number(index);
                    bool ok = object_set_array_value_at(left, ix, new_value);
                    if (!ok) {
                        errortag_t *err = error_make(ERROR_RUNTIME, frame_src_position(vm->current_frame), "Setting array item failed (out of bounds?)");
                        vm_set_runtime_error(vm, err);
                        goto err;
                    }
                } else if (left_type == OBJECT_MAP) {
                    object_t old_value = object_get_map_value(left, index);
                    if (!check_assign(vm, old_value, new_value)) {
                        goto err;
                    }
                    object_set_map_value(left, index, new_value);
                }
                break;
            }
            case OPCODE_DUP: {
                object_t val = stack_get(vm, 0);
                stack_push(vm, val);
                break;
            }
            case OPCODE_LEN: {
                object_t val = stack_pop(vm);
                int len = 0;
                object_type_t type = object_get_type(val);
                if (type == OBJECT_ARRAY) {
                    len = object_get_array_length(val);
                } else if (type == OBJECT_MAP) {
                    len = object_get_map_length(val);
                } else if (type == OBJECT_STRING) {
                    const char *str = object_get_string(val);
                    len = (int)strlen(str);
                } else {
                    const char *type_name = object_get_type_name(type);
                    errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame), "Cannot get length of %s", type_name);
                    vm_set_runtime_error(vm, err);
                    goto err;
                }
                stack_push(vm, object_make_number(len));
                break;
            }
            case OPCODE_NUMBER: {
                uint64_t val = frame_read_uint64(vm->current_frame);
                double val_double = ape_uint64_to_double(val);
                object_t obj = object_make_number(val_double);
                stack_push(vm, obj);
                break;
            }
            case OPCODE_SET_RECOVER: {
                uint16_t recover_ip = frame_read_uint16(vm->current_frame);
                vm->current_frame->recover_ip = recover_ip;
                break;
            }
            default: {
                APE_ASSERT(false);
                errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame), "Unknown opcode: 0x%x", opcode);
                vm_set_runtime_error(vm, err);
                goto err;
            }
        }
    err:
        if (vm->runtime_error != NULL) {
            int recover_frame_ix = -1;
            for (int i = vm->frames_count - 1; i >= 0; i--) {
                frame_t *frame = &vm->frames[i];
                if (frame->recover_ip >= 0 && !frame->is_recovering) {
                    recover_frame_ix = i;
                    break;
                }
            }
            if (recover_frame_ix < 0) {
                goto end;
            } else {
                errortag_t *err = vm->runtime_error;
                if (!err->traceback) {
                    err->traceback = traceback_make();
                }
                traceback_append_from_vm(err->traceback, vm);
                while (vm->frames_count > (recover_frame_ix + 1)) {
                    pop_frame(vm);
                }
                object_t err_obj = object_make_error(vm->mem, err->message);
                object_set_error_traceback(err_obj, err->traceback);
                err->traceback = NULL;
                stack_push(vm, err_obj);
                vm->current_frame->ip = vm->current_frame->recover_ip;
                vm->current_frame->is_recovering = true;
                error_destroy(vm->runtime_error);
                vm->runtime_error = NULL;
            }
        }
        if (ticks_between_gc >= 0 && ticks_since_gc >= ticks_between_gc) {
            run_gc(vm, constants);
            ticks_since_gc = 0;
        } else {
            ticks_since_gc++;
        }
    }

end:
    if (vm->runtime_error) {
        errortag_t *err = vm->runtime_error;
        if (!err->traceback) {
            err->traceback = traceback_make();
        }
        traceback_append_from_vm(err->traceback, vm);
        ptrarray_add(vm->errors, vm->runtime_error);
        vm->runtime_error = NULL;
    }

    run_gc(vm, constants);

    vm->running = false;
    return ptrarray_count(vm->errors) == 0;
}

object_t vm_get_last_popped(vm_t *vm) {
    return vm->last_popped;
}

bool vm_has_errors(vm_t *vm) {
    return vm->runtime_error != NULL || ptrarray_count(vm->errors) > 0;
}

void vm_set_global(vm_t *vm, int ix, object_t val) {
#ifdef APE_DEBUG
    if (ix >= VM_MAX_GLOBALS) {
        APE_ASSERT(false);
        errortag_t *err = error_make(ERROR_RUNTIME, frame_src_position(vm->current_frame), "Global write out of range");
        vm_set_runtime_error(vm, err);
        return;
    }
#endif
    vm->globals[ix] = val;
    if (ix >= vm->globals_count) {
        vm->globals_count = ix + 1;
    }
}

object_t vm_get_global(vm_t *vm, int ix) {
#ifdef APE_DEBUG
    if (ix >= VM_MAX_GLOBALS) {
        APE_ASSERT(false);
        errortag_t *err = error_make(ERROR_RUNTIME, frame_src_position(vm->current_frame), "Global read out of range");
        vm_set_runtime_error(vm, err);
        return object_make_null();
    }
#endif
    return vm->globals[ix];
}

void vm_set_runtime_error(vm_t *vm, errortag_t *error) {
    APE_ASSERT(vm->running);
    if (error) {
        APE_ASSERT(vm->runtime_error == NULL);
    }
    vm->runtime_error = error;
}

// INTERNAL
static void set_sp(vm_t *vm, int new_sp) {
    if (new_sp > vm->sp) { // to avoid gcing freed objects
        int count = new_sp - vm->sp;
        size_t bytes_count = count * sizeof(object_t);
        memset(vm->stack + vm->sp, 0, bytes_count);
    }
    vm->sp = new_sp;
}

static void stack_push(vm_t *vm, object_t obj) {
#ifdef APE_DEBUG
    if (vm->sp >= VM_STACK_SIZE) {
        APE_ASSERT(false);
        errortag_t *err = error_make(ERROR_RUNTIME, frame_src_position(vm->current_frame), "Stack overflow");
        vm_set_runtime_error(vm, err);
        return;
    }
    if (vm->current_frame) {
        frame_t *frame = vm->current_frame;
        function_t *current_function = object_get_function(frame->function);
        int num_locals = current_function->num_locals;
        APE_ASSERT(vm->sp >= (frame->base_pointer + num_locals));
    }
#endif
    vm->stack[vm->sp] = obj;
    vm->sp++;
}

static object_t stack_pop(vm_t *vm) {
#ifdef APE_DEBUG
    if (vm->sp == 0) {
        errortag_t *err = error_make(ERROR_RUNTIME, frame_src_position(vm->current_frame), "Stack underflow");
        vm_set_runtime_error(vm, err);
        APE_ASSERT(false);
        return object_make_null();
    }
    if (vm->current_frame) {
        frame_t *frame = vm->current_frame;
        function_t *current_function = object_get_function(frame->function);
        int num_locals = current_function->num_locals;
        APE_ASSERT((vm->sp - 1) >= (frame->base_pointer + num_locals));
    }
#endif
    vm->sp--;
    object_t res = vm->stack[vm->sp];
    vm->last_popped = res;
    return res;
}

static object_t stack_get(vm_t *vm, int nth_item) {
    int ix = vm->sp - 1 - nth_item;
#ifdef APE_DEBUG
    if (ix < 0 || ix >= VM_STACK_SIZE) {
        errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                  "Invalid stack index: %d", nth_item);
        vm_set_runtime_error(vm, err);
        APE_ASSERT(false);
        return object_make_null();
    }
#endif
    return vm->stack[ix];
}

static void this_stack_push(vm_t *vm, object_t obj) {
#ifdef APE_DEBUG
    if (vm->this_sp >= VM_THIS_STACK_SIZE) {
        APE_ASSERT(false);
        errortag_t *err = error_make(ERROR_RUNTIME, frame_src_position(vm->current_frame), "this stack overflow");
        vm_set_runtime_error(vm, err);
        return;
    }
#endif
    vm->this_stack[vm->this_sp] = obj;
    vm->this_sp++;
}

static object_t this_stack_pop(vm_t *vm) {
#ifdef APE_DEBUG
    if (vm->this_sp == 0) {
        errortag_t *err = error_make(ERROR_RUNTIME, frame_src_position(vm->current_frame), "this stack underflow");
        vm_set_runtime_error(vm, err);
        APE_ASSERT(false);
        return object_make_null();
    }
#endif
    vm->this_sp--;
    return vm->this_stack[vm->this_sp];
}

static object_t this_stack_get(vm_t *vm, int nth_item) {
    int ix = vm->this_sp - 1 - nth_item;
#ifdef APE_DEBUG
    if (ix < 0 || ix >= VM_THIS_STACK_SIZE) {
        errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                   "Invalid this stack index: %d", nth_item);
        vm_set_runtime_error(vm, err);
        APE_ASSERT(false);
        return object_make_null();
    }
#endif
    return vm->this_stack[ix];
}

static bool push_frame(vm_t *vm, frame_t frame) {
    if (vm->frames_count >= VM_MAX_FRAMES) {
        APE_ASSERT(false);
        return false;
    }
    vm->frames[vm->frames_count] = frame;
    vm->current_frame = &vm->frames[vm->frames_count];
    vm->frames_count++;
    function_t *frame_function = object_get_function(frame.function);
    set_sp(vm, frame.base_pointer + frame_function->num_locals);
    return true;
}

static bool pop_frame(vm_t *vm) {
    set_sp(vm, vm->current_frame->base_pointer - 1);
    if (vm->frames_count <= 0) {
        APE_ASSERT(false);
        vm->current_frame = NULL;
        return false;
    }
    vm->frames_count--;
    if (vm->frames_count == 0) {
        vm->current_frame = NULL;
        return false;
    }
    vm->current_frame = &vm->frames[vm->frames_count - 1];
    return true;
}

static void run_gc(vm_t *vm, array(object_t) *constants) {
    gc_unmark_all(vm->mem);
    gc_mark_objects(array_data(vm->native_functions), array_count(vm->native_functions));
    gc_mark_objects(array_data(constants), array_count(constants));
    gc_mark_objects(vm->globals, vm->globals_count);
    for (int i = 0; i < vm->frames_count; i++) {
        frame_t *frame = &vm->frames[i];
        gc_mark_object(frame->function);
    }
    gc_mark_objects(vm->stack, vm->sp);
    gc_mark_objects(vm->this_stack, vm->this_sp);
    gc_mark_object(vm->last_popped);
    gc_mark_objects(vm->operator_oveload_keys, OPCODE_MAX);
    gc_sweep(vm->mem);
}

static bool call_object(vm_t *vm, object_t callee, int num_args) {
    object_type_t callee_type = object_get_type(callee);
    if (callee_type == OBJECT_FUNCTION) {
        function_t *callee_function = object_get_function(callee);
        if (num_args != callee_function->num_args) {
            errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                       "Invalid number of arguments to \"%s\", expected %d, got %d",
                                       object_get_function_name(callee), callee_function->num_args, num_args);
            vm_set_runtime_error(vm, err);
            return false;
        }
        frame_t callee_frame;
        frame_init(&callee_frame, callee, vm->sp - num_args);
        bool ok = push_frame(vm, callee_frame);
        if (!ok) {
            errortag_t *err = error_make(ERROR_RUNTIME, src_pos_invalid, "Pushing frame failed in call_object");
            vm_set_runtime_error(vm, err);
            return false;
        }
    } else if (callee_type == OBJECT_NATIVE_FUNCTION) {
        object_t *stack_pos = vm->stack + vm->sp - num_args;
        object_t res = call_native_function(vm, callee, frame_src_position(vm->current_frame), num_args, stack_pos);
        if (vm_has_errors(vm)) {
            return false;
        }
        set_sp(vm, vm->sp - num_args - 1);
        stack_push(vm, res);
    } else {
        const char *callee_type_name = object_get_type_name(callee_type);
        errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                   "%s object is not callable", callee_type_name);
        vm_set_runtime_error(vm, err);
        return false;
    }
    return true;
}

static object_t call_native_function(vm_t *vm, object_t callee, src_pos_t src_pos, int argc, object_t *args) {
    native_function_t *bn = object_get_native_function(callee);
    object_t res = bn->fn(vm, bn->data, argc, args);
    if (vm->runtime_error != NULL && !APE_STREQ(bn->name, "crash")) {
        errortag_t *err = vm->runtime_error;
        err->pos = src_pos;
        err->traceback = traceback_make();
        traceback_append(err->traceback, bn->name, src_pos_invalid);
        return object_make_null();
    }
    object_type_t res_type = object_get_type(res);
    if (res_type == OBJECT_ERROR) {
        traceback_t *traceback = traceback_make();
         // error builtin is treated in a special way
        if (!APE_STREQ(bn->name, "error")) {
            traceback_append(traceback, bn->name, src_pos_invalid);
        }
        traceback_append_from_vm(traceback, vm);
        object_set_error_traceback(res, traceback);
    }
    return res;
}

static bool check_assign(vm_t *vm, object_t old_value, object_t new_value) {
    object_type_t old_value_type = object_get_type(old_value);
    object_type_t new_value_type = object_get_type(new_value);
    if (old_value_type == OBJECT_NULL || new_value_type == OBJECT_NULL) {
        return true;
    }
    if (old_value_type != new_value_type) {
        errortag_t *err = error_makef(ERROR_RUNTIME, frame_src_position(vm->current_frame),
                                   "Trying to assign variable of type %s to %s",
                                   object_get_type_name(new_value_type),
                                   object_get_type_name(old_value_type)
                                   );
        vm_set_runtime_error(vm, err);
        return false;
    }
    return true;
}

static bool try_overload_operator(vm_t *vm, object_t left, object_t right, opcode_t op, bool *out_overload_found) {
    *out_overload_found = false;
    object_type_t left_type = object_get_type(left);
    object_type_t right_type = object_get_type(right);
    if (left_type != OBJECT_MAP && right_type != OBJECT_MAP) {
        *out_overload_found = false;
        return true;
    }

    int num_operands = 2;
    if (op == OPCODE_MINUS || op == OPCODE_BANG) {
        num_operands = 1;
    }

    object_t key = vm->operator_oveload_keys[op];
    object_t callee = object_make_null();
    if (left_type == OBJECT_MAP) {
        callee = object_get_map_value(left, key);
    }
    if (!object_is_callable(callee)) {
        if (right_type == OBJECT_MAP) {
            callee = object_get_map_value(right, key);
        }

        if (!object_is_callable(callee)) {
            *out_overload_found = false;
            return true;
        }
    }

    *out_overload_found = true;

    stack_push(vm, callee);
    stack_push(vm, left);
    if (num_operands == 2) {
        stack_push(vm, right);
    }
    bool ok = call_object(vm, callee, num_operands);
    return ok;
}

