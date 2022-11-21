struct VM
{
    public:
        VM* vm_make(Allocator* alloc, const Config* config, GCMemory* mem, ErrorList* errors, GlobalStore* global_store)
        {
            VM* vm = (VM*)alloc->allocate(sizeof(VM));
            if(!vm)
            {
                return NULL;
            }
            memset(vm, 0, sizeof(VM));
            vm->alloc = alloc;
            vm->config = config;
            vm->mem = mem;
            vm->errors = errors;
            vm->global_store = global_store;
            vm->globals_count = 0;
            vm->sp = 0;
            vm->this_sp = 0;
            vm->frames_count = 0;
            vm->last_popped = Object::makeNull();
            vm->running = false;

            for(int i = 0; i < OPCODE_MAX; i++)
            {
                vm->operator_oveload_keys[i] = Object::makeNull();
            }
        #define SET_OPERATOR_OVERLOAD_KEY(op, key)                   \
            do                                                       \
            {                                                        \
                Object key_obj = Object::makeString(vm->mem, key); \
                if(key_obj.isNull())                          \
                {                                                    \
                    goto err;                                        \
                }                                                    \
                vm->operator_oveload_keys[op] = key_obj;             \
            } while(0)
            SET_OPERATOR_OVERLOAD_KEY(OPCODE_ADD, "__operator_add__");
            SET_OPERATOR_OVERLOAD_KEY(OPCODE_SUB, "__operator_sub__");
            SET_OPERATOR_OVERLOAD_KEY(OPCODE_MUL, "__operator_mul__");
            SET_OPERATOR_OVERLOAD_KEY(OPCODE_DIV, "__operator_div__");
            SET_OPERATOR_OVERLOAD_KEY(OPCODE_MOD, "__operator_mod__");
            SET_OPERATOR_OVERLOAD_KEY(OPCODE_OR, "__operator_or__");
            SET_OPERATOR_OVERLOAD_KEY(OPCODE_XOR, "__operator_xor__");
            SET_OPERATOR_OVERLOAD_KEY(OPCODE_AND, "__operator_and__");
            SET_OPERATOR_OVERLOAD_KEY(OPCODE_LSHIFT, "__operator_lshift__");
            SET_OPERATOR_OVERLOAD_KEY(OPCODE_RSHIFT, "__operator_rshift__");
            SET_OPERATOR_OVERLOAD_KEY(OPCODE_MINUS, "__operator_minus__");
            SET_OPERATOR_OVERLOAD_KEY(OPCODE_BANG, "__operator_bang__");
            SET_OPERATOR_OVERLOAD_KEY(OPCODE_COMPARE, "__cmp__");
        #undef SET_OPERATOR_OVERLOAD_KEY

            return vm;
        err:
            vm_destroy(vm);
            return NULL;
        }

    public:
        Allocator* alloc;
        const Config* config;
        GCMemory* mem;
        ErrorList* errors;
        GlobalStore* global_store;
        Object globals[VM_MAX_GLOBALS];
        int globals_count;
        Object stack[VM_STACK_SIZE];
        int sp;
        Object this_stack[VM_THIS_STACK_SIZE];
        int this_sp;
        Frame frames[VM_MAX_FRAMES];
        int frames_count;
        Object last_popped;
        Frame* current_frame;
        bool running;
        Object operator_oveload_keys[OPCODE_MAX];

    public:

        void vm_destroy()
        {
            if(!this)
            {
                return;
            }
            this->alloc->release(this);
        }

        void vm_reset()
        {
            this->sp = 0;
            this->this_sp = 0;
            while(this->frames_count > 0)
            {
                pop_frame(this);
            }
        }

        bool vm_run(CompilationResult* comp_res, Array * constants)
        {
        #ifdef APE_DEBUG
            int old_sp = this->sp;
        #endif
            int old_this_sp = this->this_sp;
            int old_frames_count = this->frames_count;
            Object main_fn = Object::makeFunction(this->mem, "main", comp_res, false, 0, 0, 0);
            if(main_fn.isNull())
            {
                return false;
            }
            stack_push(this, main_fn);
            bool res = vm_execute_function(this, main_fn, constants);
            while(this->frames_count > old_frames_count)
            {
                pop_frame(this);
            }
            APE_ASSERT(this->sp == old_sp);
            this->this_sp = old_this_sp;
            return res;
        }

        Object vm_call(Array * constants, Object callee, int argc, Object* args)
        {
            ObjectType type = callee.type();
            if(type == APE_OBJECT_FUNCTION)
            {
        #ifdef APE_DEBUG
                int old_sp = this->sp;
        #endif
                int old_this_sp = this->this_sp;
                int old_frames_count = this->frames_count;
                stack_push(this, callee);
                for(int i = 0; i < argc; i++)
                {
                    stack_push(this, args[i]);
                }
                bool ok = vm_execute_function(this, callee, constants);
                if(!ok)
                {
                    return Object::makeNull();
                }
                while(this->frames_count > old_frames_count)
                {
                    pop_frame(this);
                }
                APE_ASSERT(this->sp == old_sp);
                this->this_sp = old_this_sp;
                return vm_get_last_popped(this);
            }
            else if(type == APE_OBJECT_NATIVE_FUNCTION)
            {
                return call_native_function(this, callee, src_pos_invalid, argc, args);
            }
            else
            {
                errors_add_error(this->errors, APE_ERROR_USER, src_pos_invalid, "Object is not callable");
                return Object::makeNull();
            }
        }

        Object vm_get_last_popped()
        {
            return this->last_popped;
        }

        bool vm_has_errors()
        {
            return errors_get_count(this->errors) > 0;
        }

        bool vm_set_global(int ix, Object val)
        {
            if(ix >= VM_MAX_GLOBALS)
            {
                APE_ASSERT(false);
                errors_add_error(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "Global write out of range");
                return false;
            }
            this->globals[ix] = val;
            if(ix >= this->globals_count)
            {
                this->globals_count = ix + 1;
            }
            return true;
        }

        Object vm_get_global(int ix)
        {
            if(ix >= VM_MAX_GLOBALS)
            {
                APE_ASSERT(false);
                errors_add_error(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "Global read out of range");
                return Object::makeNull();
            }
            return this->globals[ix];
        }

        // INTERNAL
        void set_sp(int new_sp)
        {
            if(new_sp > this->sp)
            {// to avoid gcing freed objects
                int cn = new_sp - this->sp;
                size_t bytes_count = cn * sizeof(Object);
                memset(this->stack + this->sp, 0, bytes_count);
            }
            this->sp = new_sp;
        }

        void stack_push(Object obj)
        {
        #ifdef APE_DEBUG
            if(this->sp >= VM_STACK_SIZE)
            {
                APE_ASSERT(false);
                errors_add_error(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "Stack overflow");
                return;
            }
            if(this->current_frame)
            {
                Frame* frame = this->current_frame;
                ScriptFunction* current_function = object_get_function(frame->function);
                int num_locals = current_function->num_locals;
                APE_ASSERT(this->sp >= (frame->base_pointer + num_locals));
            }
        #endif
            this->stack[this->sp] = obj;
            this->sp++;
        }

        Object stack_pop()
        {
        #ifdef APE_DEBUG
            if(this->sp == 0)
            {
                errors_add_error(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "Stack underflow");
                APE_ASSERT(false);
                return Object::makeNull();
            }
            if(this->current_frame)
            {
                Frame* frame = this->current_frame;
                ScriptFunction* current_function = object_get_function(frame->function);
                int num_locals = current_function->num_locals;
                APE_ASSERT((this->sp - 1) >= (frame->base_pointer + num_locals));
            }
        #endif
            this->sp--;
            Object res = this->stack[this->sp];
            this->last_popped = res;
            return res;
        }

        Object stack_get(int nth_item)
        {
            int ix = this->sp - 1 - nth_item;
        #ifdef APE_DEBUG
            if(ix < 0 || ix >= VM_STACK_SIZE)
            {
                errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "Invalid stack index: %d", nth_item);
                APE_ASSERT(false);
                return Object::makeNull();
            }
        #endif
            return this->stack[ix];
        }

        void this_stack_push(Object obj)
        {
        #ifdef APE_DEBUG
            if(this->this_sp >= VM_THIS_STACK_SIZE)
            {
                APE_ASSERT(false);
                errors_add_error(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "this stack overflow");
                return;
            }
        #endif
            this->this_stack[this->this_sp] = obj;
            this->this_sp++;
        }

        Object this_stack_pop()
        {
        #ifdef APE_DEBUG
            if(this->this_sp == 0)
            {
                errors_add_error(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "this stack underflow");
                APE_ASSERT(false);
                return Object::makeNull();
            }
        #endif
            this->this_sp--;
            return this->this_stack[this->this_sp];
        }

        Object this_stack_get(int nth_item)
        {
            int ix = this->this_sp - 1 - nth_item;
        #ifdef APE_DEBUG
            if(ix < 0 || ix >= VM_THIS_STACK_SIZE)
            {
                errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "Invalid this stack index: %d", nth_item);
                APE_ASSERT(false);
                return Object::makeNull();
            }
        #endif
            return this->this_stack[ix];
        }

        bool push_frame(Frame frame)
        {
            if(this->frames_count >= VM_MAX_FRAMES)
            {
                APE_ASSERT(false);
                return false;
            }
            this->frames[this->frames_count] = frame;
            this->current_frame = &this->frames[this->frames_count];
            this->frames_count++;
            ScriptFunction* frame_function = object_get_function(frame.function);
            set_sp(this, frame.base_pointer + frame_function->num_locals);
            return true;
        }

        bool pop_frame()
        {
            set_sp(this, this->current_frame->base_pointer - 1);
            if(this->frames_count <= 0)
            {
                APE_ASSERT(false);
                this->current_frame = NULL;
                return false;
            }
            this->frames_count--;
            if(this->frames_count == 0)
            {
                this->current_frame = NULL;
                return false;
            }
            this->current_frame = &this->frames[this->frames_count - 1];
            return true;
        }

        void run_gc(Array * constants)
        {
            this->mem->unmarkAll();
            GCMemory::markObjects(global_store_get_object_data(this->global_store), global_store_get_object_count(this->global_store));
            GCMemory::markObjects((Object*)constants->data(), constants->count());
            GCMemory::markObjects(this->globals, this->globals_count);
            for(int i = 0; i < this->frames_count; i++)
            {
                Frame* frame = &this->frames[i];
                GCMemory::markObject(frame->function);
            }
            GCMemory::markObjects(this->stack, this->sp);
            GCMemory::markObjects(this->this_stack, this->this_sp);
            GCMemory::markObject(this->last_popped);
            GCMemory::markObjects(this->operator_oveload_keys, OPCODE_MAX);
            this->mem->sweep();
        }

        bool call_object(Object callee, int num_args)
        {
            ObjectType callee_type = callee.type();
            if(callee_type == APE_OBJECT_FUNCTION)
            {
                ScriptFunction* callee_function = object_get_function(callee);
                if(num_args != callee_function->num_args)
                {
                    errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                      "Invalid number of arguments to \"%s\", expected %d, got %d",
                                      object_get_function_name(callee), callee_function->num_args, num_args);
                    return false;
                }
                Frame callee_frame;
                bool ok = frame_init(&callee_frame, callee, this->sp - num_args);
                if(!ok)
                {
                    errors_add_error(this->errors, APE_ERROR_RUNTIME, src_pos_invalid, "Frame init failed in call_object");
                    return false;
                }
                ok = push_frame(this, callee_frame);
                if(!ok)
                {
                    errors_add_error(this->errors, APE_ERROR_RUNTIME, src_pos_invalid, "Pushing frame failed in call_object");
                    return false;
                }
            }
            else if(callee_type == APE_OBJECT_NATIVE_FUNCTION)
            {
                Object* stack_pos = this->stack + this->sp - num_args;
                Object res = call_native_function(this, callee, frame_src_position(this->current_frame), num_args, stack_pos);
                if(vm_has_errors(this))
                {
                    return false;
                }
                set_sp(this, this->sp - num_args - 1);
                stack_push(this, res);
            }
            else
            {
                const char* callee_type_name = object_get_type_name(callee_type);
                errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "%s object is not callable", callee_type_name);
                return false;
            }
            return true;
        }

        Object call_native_function(Object callee, Position src_pos, int argc, Object* args)
        {
            NativeFunction* native_fun = object_get_native_function(callee);
            Object res = native_fun->native_funcptr(this, native_fun->data, argc, args);
            if(errors_has_errors(this->errors) && !APE_STREQ(native_fun->name, "crash"))
            {
                Error* err = errors_get_last_error(this->errors);
                err->pos = src_pos;
                err->traceback = traceback_make(this->alloc);
                if(err->traceback)
                {
                    traceback_append(err->traceback, native_fun->name, src_pos_invalid);
                }
                return Object::makeNull();
            }
            ObjectType res_type = res.type();
            if(res_type == APE_OBJECT_ERROR)
            {
                Traceback* traceback = traceback_make(this->alloc);
                if(traceback)
                {
                    // error builtin is treated in a special way
                    if(!APE_STREQ(native_fun->name, "error"))
                    {
                        traceback_append(traceback, native_fun->name, src_pos_invalid);
                    }
                    traceback_append_from_vm(traceback, this);
                    object_set_error_traceback(res, traceback);
                }
            }
            return res;
        }

        bool check_assign(Object old_value, Object new_value)
        {
            ObjectType old_value_type;
            ObjectType new_value_type;
            (void)this;
            old_value_type = old_value.type();
            new_value_type = new_value.type();
            if(old_value_type == APE_OBJECT_NULL || new_value_type == APE_OBJECT_NULL)
            {
                return true;
            }
            if(old_value_type != new_value_type)
            {
                /*
                errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "Trying to assign variable of type %s to %s",
                                  object_get_type_name(new_value_type), object_get_type_name(old_value_type));
                return false;
                */
            }
            return true;
        }

        bool try_overload_operator(Object left, Object right, opcode_t op, bool* out_overload_found)
        {
            int num_operands;
            Object key;
            Object callee;
            ObjectType left_type;
            ObjectType right_type;
            *out_overload_found = false;
            left_type = left.type();
            right_type = right.type();
            if(left_type != APE_OBJECT_MAP && right_type != APE_OBJECT_MAP)
            {
                *out_overload_found = false;
                return true;
            }
            num_operands = 2;
            if(op == OPCODE_MINUS || op == OPCODE_BANG)
            {
                num_operands = 1;
            }
            key = this->operator_oveload_keys[op];
            callee = Object::makeNull();
            if(left_type == APE_OBJECT_MAP)
            {
                callee = object_get_map_value_object(left, key);
            }
            if(!callee.isCallable())
            {
                if(right_type == APE_OBJECT_MAP)
                {
                    callee = object_get_map_value_object(right, key);
                }
                if(!callee.isCallable())
                {
                    *out_overload_found = false;
                    return true;
                }
            }
            *out_overload_found = true;
            stack_push(this, callee);
            stack_push(this, left);
            if(num_operands == 2)
            {
                stack_push(this, right);
            }
            return call_object(this, callee, num_operands);
        }


        bool vm_execute_function(Object function, Array * constants)
        {
            if(this->running)
            {
                errors_add_error(this->errors, APE_ERROR_USER, src_pos_invalid, "VM is already executing code");
                return false;
            }

            ScriptFunction* function_function = object_get_function(function);// naming is hard
            Frame new_frame;
            bool ok = false;
            ok = frame_init(&new_frame, function, this->sp - function_function->num_args);
            if(!ok)
            {
                return false;
            }
            ok = push_frame(this, new_frame);
            if(!ok)
            {
                errors_add_error(this->errors, APE_ERROR_USER, src_pos_invalid, "Pushing frame failed");
                return false;
            }

            this->running = true;
            this->last_popped = Object::makeNull();

            bool check_time = false;
            double max_exec_time_ms = 0;
            if(this->config)
            {
                check_time = this->config->max_execution_time_set;
                max_exec_time_ms = this->config->max_execution_time_ms;
            }
            unsigned time_check_interval = 1000;
            unsigned time_check_counter = 0;
            Timer timer;
            memset(&timer, 0, sizeof(Timer));
            if(check_time)
            {
                timer = ape_timer_start();
            }

            while(this->current_frame->ip < this->current_frame->bytecode_size)
            {
                OpcodeValue opcode = frame_read_opcode(this->current_frame);
                switch(opcode)
                {
                    case OPCODE_CONSTANT:
                        {
                            uint16_t constant_ix = frame_read_uint16(this->current_frame);
                            Object* constant = (Object*)constants->get(constant_ix);
                            if(!constant)
                            {
                                errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                  "Constant at %d not found", constant_ix);
                                goto err;
                            }
                            stack_push(this, *constant);
                        }
                        break;

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
                            Object right = stack_pop(this);
                            Object left = stack_pop(this);
                            ObjectType left_type = left.type();
                            ObjectType right_type = right.type();
                            if(left.isNumeric() && right.isNumeric())
                            {
                                double right_val = object_get_number(right);
                                double left_val = object_get_number(left);

                                int64_t left_val_int = (int64_t)left_val;
                                int64_t right_val_int = (int64_t)right_val;

                                double res = 0;
                                switch(opcode)
                                {
                                    case OPCODE_ADD:
                                        res = left_val + right_val;
                                        break;
                                    case OPCODE_SUB:
                                        res = left_val - right_val;
                                        break;
                                    case OPCODE_MUL:
                                        res = left_val * right_val;
                                        break;
                                    case OPCODE_DIV:
                                        res = left_val / right_val;
                                        break;
                                    case OPCODE_MOD:
                                        res = fmod(left_val, right_val);
                                        break;
                                    case OPCODE_OR:
                                        res = (double)(left_val_int | right_val_int);
                                        break;
                                    case OPCODE_XOR:
                                        res = (double)(left_val_int ^ right_val_int);
                                        break;
                                    case OPCODE_AND:
                                        res = (double)(left_val_int & right_val_int);
                                        break;
                                    case OPCODE_LSHIFT:
                                        res = (double)(left_val_int << right_val_int);
                                        break;
                                    case OPCODE_RSHIFT:
                                        res = (double)(left_val_int >> right_val_int);
                                        break;
                                    default:
                                        APE_ASSERT(false);
                                        break;
                                }
                                stack_push(this, Object::makeNumber(res));
                            }
                            else if(left_type == APE_OBJECT_STRING && right_type == APE_OBJECT_STRING && opcode == OPCODE_ADD)
                            {
                                int left_len = (int)object_get_string_length(left);
                                int right_len = (int)object_get_string_length(right);

                                if(left_len == 0)
                                {
                                    stack_push(this, right);
                                }
                                else if(right_len == 0)
                                {
                                    stack_push(this, left);
                                }
                                else
                                {
                                    const char* left_val = object_get_string(left);
                                    const char* right_val = object_get_string(right);

                                    Object res = Object::makeStringCapacity(this->mem, left_len + right_len);
                                    if(res.isNull())
                                    {
                                        goto err;
                                    }

                                    ok = object_string_append(res, left_val, left_len);
                                    if(!ok)
                                    {
                                        goto err;
                                    }

                                    ok = object_string_append(res, right_val, right_len);
                                    if(!ok)
                                    {
                                        goto err;
                                    }
                                    stack_push(this, res);
                                }
                            }
                            else if((left_type == APE_OBJECT_ARRAY) && opcode == OPCODE_ADD)
                            {
                                object_add_array_value(left, right);
                                stack_push(this, left);
                            }
                            else
                            {
                                bool overload_found = false;
                                bool ok = try_overload_operator(this, left, right, opcode, &overload_found);
                                if(!ok)
                                {
                                    goto err;
                                }
                                if(!overload_found)
                                {
                                    const char* opcode_name = opcode_get_name(opcode);
                                    const char* left_type_name = object_get_type_name(left_type);
                                    const char* right_type_name = object_get_type_name(right_type);
                                    errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                      "Invalid operand types for %s, got %s and %s", opcode_name, left_type_name,
                                                      right_type_name);
                                    goto err;
                                }
                            }
                        }
                        break;

                    case OPCODE_POP:
                        {
                            stack_pop(this);
                        }
                        break;

                    case OPCODE_TRUE:
                        {
                            stack_push(this, Object::makeBool(true));
                        }
                        break;

                    case OPCODE_FALSE:
                    {
                        stack_push(this, Object::makeBool(false));
                        break;
                    }
                    case OPCODE_COMPARE:
                    case OPCODE_COMPARE_EQ:
                        {
                            Object right = stack_pop(this);
                            Object left = stack_pop(this);
                            bool is_overloaded = false;
                            bool ok = try_overload_operator(this, left, right, OPCODE_COMPARE, &is_overloaded);
                            if(!ok)
                            {
                                goto err;
                            }
                            if(!is_overloaded)
                            {
                                double comparison_res = object_compare(left, right, &ok);
                                if(ok || opcode == OPCODE_COMPARE_EQ)
                                {
                                    Object res = Object::makeNumber(comparison_res);
                                    stack_push(this, res);
                                }
                                else
                                {
                                    const char* right_type_string = object_get_type_name(right.type());
                                    const char* left_type_string = object_get_type_name(left.type());
                                    errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                      "Cannot compare %s and %s", left_type_string, right_type_string);
                                    goto err;
                                }
                            }
                        }
                        break;

                    case OPCODE_EQUAL:
                    case OPCODE_NOT_EQUAL:
                    case OPCODE_GREATER_THAN:
                    case OPCODE_GREATER_THAN_EQUAL:
                        {
                            Object value = stack_pop(this);
                            double comparison_res = object_get_number(value);
                            bool res_val = false;
                            switch(opcode)
                            {
                                case OPCODE_EQUAL:
                                    res_val = APE_DBLEQ(comparison_res, 0);
                                    break;
                                case OPCODE_NOT_EQUAL:
                                    res_val = !APE_DBLEQ(comparison_res, 0);
                                    break;
                                case OPCODE_GREATER_THAN:
                                    res_val = comparison_res > 0;
                                    break;
                                case OPCODE_GREATER_THAN_EQUAL:
                                {
                                    res_val = comparison_res > 0 || APE_DBLEQ(comparison_res, 0);
                                    break;
                                }
                                default:
                                    APE_ASSERT(false);
                                    break;
                            }
                            Object res = Object::makeBool(res_val);
                            stack_push(this, res);
                        }
                        break;

                    case OPCODE_MINUS:
                        {
                            Object operand = stack_pop(this);
                            ObjectType operand_type = operand.type();
                            if(operand_type == APE_OBJECT_NUMBER)
                            {
                                double val = object_get_number(operand);
                                Object res = Object::makeNumber(-val);
                                stack_push(this, res);
                            }
                            else
                            {
                                bool overload_found = false;
                                bool ok = try_overload_operator(this, operand, Object::makeNull(), OPCODE_MINUS, &overload_found);
                                if(!ok)
                                {
                                    goto err;
                                }
                                if(!overload_found)
                                {
                                    const char* operand_type_string = object_get_type_name(operand_type);
                                    errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                      "Invalid operand type for MINUS, got %s", operand_type_string);
                                    goto err;
                                }
                            }
                        }
                        break;

                    case OPCODE_BANG:
                        {
                            Object operand = stack_pop(this);
                            ObjectType type = operand.type();
                            if(type == APE_OBJECT_BOOL)
                            {
                                Object res = Object::makeBool(!object_get_bool(operand));
                                stack_push(this, res);
                            }
                            else if(type == APE_OBJECT_NULL)
                            {
                                Object res = Object::makeBool(true);
                                stack_push(this, res);
                            }
                            else
                            {
                                bool overload_found = false;
                                bool ok = try_overload_operator(this, operand, Object::makeNull(), OPCODE_BANG, &overload_found);
                                if(!ok)
                                {
                                    goto err;
                                }
                                if(!overload_found)
                                {
                                    Object res = Object::makeBool(false);
                                    stack_push(this, res);
                                }
                            }
                        }
                        break;

                    case OPCODE_JUMP:
                        {
                            uint16_t pos = frame_read_uint16(this->current_frame);
                            this->current_frame->ip = pos;
                        }
                        break;

                    case OPCODE_JUMP_IF_FALSE:
                        {
                            uint16_t pos = frame_read_uint16(this->current_frame);
                            Object test = stack_pop(this);
                            if(!object_get_bool(test))
                            {
                                this->current_frame->ip = pos;
                            }
                        }
                        break;

                    case OPCODE_JUMP_IF_TRUE:
                        {
                            uint16_t pos = frame_read_uint16(this->current_frame);
                            Object test = stack_pop(this);
                            if(object_get_bool(test))
                            {
                                this->current_frame->ip = pos;
                            }
                        }
                        break;

                    case OPCODE_NULL:
                        {
                            stack_push(this, Object::makeNull());
                        }
                        break;

                    case OPCODE_DEFINE_MODULE_GLOBAL:
                    {
                        uint16_t ix = frame_read_uint16(this->current_frame);
                        Object value = stack_pop(this);
                        vm_set_global(this, ix, value);
                        break;
                    }
                    case OPCODE_SET_MODULE_GLOBAL:
                        {
                            uint16_t ix = frame_read_uint16(this->current_frame);
                            Object new_value = stack_pop(this);
                            Object old_value = vm_get_global(this, ix);
                            if(!check_assign(this, old_value, new_value))
                            {
                                goto err;
                            }
                            vm_set_global(this, ix, new_value);
                        }
                        break;

                    case OPCODE_GET_MODULE_GLOBAL:
                        {
                            uint16_t ix = frame_read_uint16(this->current_frame);
                            Object global = this->globals[ix];
                            stack_push(this, global);
                        }
                        break;

                    case OPCODE_ARRAY:
                        {
                            uint16_t cn = frame_read_uint16(this->current_frame);
                            Object array_obj = Object::makeArray(this->mem, cn);
                            if(array_obj.isNull())
                            {
                                goto err;
                            }
                            Object* items = this->stack + this->sp - cn;
                            for(int i = 0; i < cn; i++)
                            {
                                Object item = items[i];
                                ok = object_add_array_value(array_obj, item);
                                if(!ok)
                                {
                                    goto err;
                                }
                            }
                            set_sp(this, this->sp - cn);
                            stack_push(this, array_obj);
                        }
                        break;

                    case OPCODE_MAP_START:
                        {
                            uint16_t cn = frame_read_uint16(this->current_frame);
                            Object map_obj = Object::makeMap(this->mem, cn);
                            if(map_obj.isNull())
                            {
                                goto err;
                            }
                            this_stack_push(this, map_obj);
                        }
                        break;

                    case OPCODE_MAP_END:
                        {
                            uint16_t kvp_count = frame_read_uint16(this->current_frame);
                            uint16_t items_count = kvp_count * 2;
                            Object map_obj = this_stack_pop(this);
                            Object* kv_pairs = this->stack + this->sp - items_count;
                            for(int i = 0; i < items_count; i += 2)
                            {
                                Object key = kv_pairs[i];
                                if(!key.isHashable())
                                {
                                    ObjectType key_type = key.type();
                                    const char* key_type_name = object_get_type_name(key_type);
                                    errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                      "Key of type %s is not hashable", key_type_name);
                                    goto err;
                                }
                                Object val = kv_pairs[i + 1];
                                ok = object_set_map_value_object(map_obj, key, val);
                                if(!ok)
                                {
                                    goto err;
                                }
                            }
                            set_sp(this, this->sp - items_count);
                            stack_push(this, map_obj);
                        }
                        break;

                    case OPCODE_GET_THIS:
                        {
                            Object obj = this_stack_get(this, 0);
                            stack_push(this, obj);
                        }
                        break;

                    case OPCODE_GET_INDEX:
                        {
                            #if 0
                            const char* idxname;
                            #endif
                            Object index = stack_pop(this);
                            Object left = stack_pop(this);
                            ObjectType left_type = left.type();
                            ObjectType index_type = index.type();
                            const char* left_type_name = object_get_type_name(left_type);
                            const char* index_type_name = object_get_type_name(index_type);
                            /*
                            * todo: object method lookup could be implemented here
                            */
                            #if 0
                            {
                                int argc;
                                Object args[10];
                                NativeFNCallback afn;
                                argc = 0;
                                if(index_type == APE_OBJECT_STRING)
                                {
                                    idxname = object_get_string(index);
                                    fprintf(stderr, "index is a string: name=%s\n", idxname);
                                    if((afn = builtin_get_object(left_type, idxname)) != NULL)
                                    {
                                        fprintf(stderr, "got a callback: afn=%p\n", afn);
                                        //typedef Object (*NativeFNCallback)(VM*, void*, int, Object*);
                                        args[argc] = left;
                                        argc++;
                                        stack_push(this, afn(this, NULL, argc, args));
                                        break;
                                    }
                                }
                            }
                            #endif

                            if(left_type != APE_OBJECT_ARRAY && left_type != APE_OBJECT_MAP && left_type != APE_OBJECT_STRING)
                            {
                                errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                    "Type %s is not indexable (in OPCODE_GET_INDEX)", left_type_name);
                                goto err;
                            }
                            Object res = Object::makeNull();

                            if(left_type == APE_OBJECT_ARRAY)
                            {
                                if(index_type != APE_OBJECT_NUMBER)
                                {
                                    errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                      "Cannot index %s with %s", left_type_name, index_type_name);
                                    goto err;
                                }
                                int ix = (int)object_get_number(index);
                                if(ix < 0)
                                {
                                    ix = object_get_array_length(left) + ix;
                                }
                                if(ix >= 0 && ix < object_get_array_length(left))
                                {
                                    res = object_get_array_value(left, ix);
                                }
                            }
                            else if(left_type == APE_OBJECT_MAP)
                            {
                                res = object_get_map_value_object(left, index);
                            }
                            else if(left_type == APE_OBJECT_STRING)
                            {
                                const char* str = object_get_string(left);
                                int left_len = object_get_string_length(left);
                                int ix = (int)object_get_number(index);
                                if(ix >= 0 && ix < left_len)
                                {
                                    char res_str[2] = { str[ix], '\0' };
                                    res = Object::makeString(this->mem, res_str);
                                }
                            }
                            stack_push(this, res);
                        }
                        break;

                    case OPCODE_GET_VALUE_AT:
                    {
                        Object index = stack_pop(this);
                        Object left = stack_pop(this);
                        ObjectType left_type = left.type();
                        ObjectType index_type = index.type();
                        const char* left_type_name = object_get_type_name(left_type);
                        const char* index_type_name = object_get_type_name(index_type);

                        if(left_type != APE_OBJECT_ARRAY && left_type != APE_OBJECT_MAP && left_type != APE_OBJECT_STRING)
                        {
                            errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                              "Type %s is not indexable (in OPCODE_GET_VALUE_AT)", left_type_name);
                            goto err;
                        }

                        Object res = Object::makeNull();
                        if(index_type != APE_OBJECT_NUMBER)
                        {
                            errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                              "Cannot index %s with %s", left_type_name, index_type_name);
                            goto err;
                        }
                        int ix = (int)object_get_number(index);

                        if(left_type == APE_OBJECT_ARRAY)
                        {
                            res = object_get_array_value(left, ix);
                        }
                        else if(left_type == APE_OBJECT_MAP)
                        {
                            res = object_get_kv_pair_at(this->mem, left, ix);
                        }
                        else if(left_type == APE_OBJECT_STRING)
                        {
                            const char* str = object_get_string(left);
                            int left_len = object_get_string_length(left);
                            int ix = (int)object_get_number(index);
                            if(ix >= 0 && ix < left_len)
                            {
                                char res_str[2] = { str[ix], '\0' };
                                res = Object::makeString(this->mem, res_str);
                            }
                        }
                        stack_push(this, res);
                        break;
                    }
                    case OPCODE_CALL:
                    {
                        uint8_t num_args = frame_read_uint8(this->current_frame);
                        Object callee = stack_get(this, num_args);
                        bool ok = call_object(this, callee, num_args);
                        if(!ok)
                        {
                            goto err;
                        }
                        break;
                    }
                    case OPCODE_RETURN_VALUE:
                    {
                        Object res = stack_pop(this);
                        bool ok = pop_frame(this);
                        if(!ok)
                        {
                            goto end;
                        }
                        stack_push(this, res);
                        break;
                    }
                    case OPCODE_RETURN:
                    {
                        bool ok = pop_frame(this);
                        stack_push(this, Object::makeNull());
                        if(!ok)
                        {
                            stack_pop(this);
                            goto end;
                        }
                        break;
                    }
                    case OPCODE_DEFINE_LOCAL:
                    {
                        uint8_t pos = frame_read_uint8(this->current_frame);
                        this->stack[this->current_frame->base_pointer + pos] = stack_pop(this);
                        break;
                    }
                    case OPCODE_SET_LOCAL:
                    {
                        uint8_t pos = frame_read_uint8(this->current_frame);
                        Object new_value = stack_pop(this);
                        Object old_value = this->stack[this->current_frame->base_pointer + pos];
                        if(!check_assign(this, old_value, new_value))
                        {
                            goto err;
                        }
                        this->stack[this->current_frame->base_pointer + pos] = new_value;
                        break;
                    }
                    case OPCODE_GET_LOCAL:
                    {
                        uint8_t pos = frame_read_uint8(this->current_frame);
                        Object val = this->stack[this->current_frame->base_pointer + pos];
                        stack_push(this, val);
                        break;
                    }
                    case OPCODE_GET_APE_GLOBAL:
                    {
                        uint16_t ix = frame_read_uint16(this->current_frame);
                        bool ok = false;
                        Object val = global_store_get_object_at(this->global_store, ix, &ok);
                        if(!ok)
                        {
                            errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                              "Global value %d not found", ix);
                            goto err;
                        }
                        stack_push(this, val);
                        break;
                    }
                    case OPCODE_FUNCTION:
                        {
                            uint16_t constant_ix = frame_read_uint16(this->current_frame);
                            uint8_t num_free = frame_read_uint8(this->current_frame);
                            Object* constant = (Object*)constants->get(constant_ix);
                            if(!constant)
                            {
                                errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                  "Constant %d not found", constant_ix);
                                goto err;
                            }
                            ObjectType constant_type = (*constant).type();
                            if(constant_type != APE_OBJECT_FUNCTION)
                            {
                                const char* type_name = object_get_type_name(constant_type);
                                errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                  "%s is not a function", type_name);
                                goto err;
                            }

                            const ScriptFunction* constant_function = object_get_function(*constant);
                            Object function_obj
                            = Object::makeFunction(this->mem, object_get_function_name(*constant), constant_function->comp_result,
                                                   false, constant_function->num_locals, constant_function->num_args, num_free);
                            if(function_obj.isNull())
                            {
                                goto err;
                            }
                            for(int i = 0; i < num_free; i++)
                            {
                                Object free_val = this->stack[this->sp - num_free + i];
                                object_set_function_free_val(function_obj, i, free_val);
                            }
                            set_sp(this, this->sp - num_free);
                            stack_push(this, function_obj);
                        }
                        break;
                    case OPCODE_GET_FREE:
                        {
                            uint8_t free_ix = frame_read_uint8(this->current_frame);
                            Object val = object_get_function_free_val(this->current_frame->function, free_ix);
                            stack_push(this, val);
                        }
                        break;
                    case OPCODE_SET_FREE:
                        {
                            uint8_t free_ix = frame_read_uint8(this->current_frame);
                            Object val = stack_pop(this);
                            object_set_function_free_val(this->current_frame->function, free_ix, val);
                        }
                        break;
                    case OPCODE_CURRENT_FUNCTION:
                        {
                            Object current_function = this->current_frame->function;
                            stack_push(this, current_function);
                        }
                        break;
                    case OPCODE_SET_INDEX:
                        {
                            Object index = stack_pop(this);
                            Object left = stack_pop(this);
                            Object new_value = stack_pop(this);
                            ObjectType left_type = left.type();
                            ObjectType index_type = index.type();
                            const char* left_type_name = object_get_type_name(left_type);
                            const char* index_type_name = object_get_type_name(index_type);

                            if(left_type != APE_OBJECT_ARRAY && left_type != APE_OBJECT_MAP)
                            {
                                errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                  "Type %s is not indexable (in OPCODE_SET_INDEX)", left_type_name);
                                goto err;
                            }

                            if(left_type == APE_OBJECT_ARRAY)
                            {
                                if(index_type != APE_OBJECT_NUMBER)
                                {
                                    errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                      "Cannot index %s with %s", left_type_name, index_type_name);
                                    goto err;
                                }
                                int ix = (int)object_get_number(index);
                                ok = object_set_array_value_at(left, ix, new_value);
                                if(!ok)
                                {
                                    errors_add_error(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                     "Setting array item failed (out of bounds?)");
                                    goto err;
                                }
                            }
                            else if(left_type == APE_OBJECT_MAP)
                            {
                                Object old_value = object_get_map_value_object(left, index);
                                if(!check_assign(this, old_value, new_value))
                                {
                                    goto err;
                                }
                                ok = object_set_map_value_object(left, index, new_value);
                                if(!ok)
                                {
                                    goto err;
                                }
                            }
                        }
                        break;
                    case OPCODE_DUP:
                        {
                            Object val = stack_get(this, 0);
                            stack_push(this, val);
                        }
                        break;
                    case OPCODE_LEN:
                        {
                            Object val = stack_pop(this);
                            int len = 0;
                            ObjectType type = val.type();
                            if(type == APE_OBJECT_ARRAY)
                            {
                                len = object_get_array_length(val);
                            }
                            else if(type == APE_OBJECT_MAP)
                            {
                                len = object_get_map_length(val);
                            }
                            else if(type == APE_OBJECT_STRING)
                            {
                                len = object_get_string_length(val);
                            }
                            else
                            {
                                const char* type_name = object_get_type_name(type);
                                errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame),
                                                  "Cannot get length of %s", type_name);
                                goto err;
                            }
                            stack_push(this, Object::makeNumber(len));
                        }
                        break;

                    case OPCODE_NUMBER:
                        {
                            uint64_t val = frame_read_uint64(this->current_frame);
                            double val_double = ape_uint64_to_double(val);
                            Object obj = Object::makeNumber(val_double);
                            stack_push(this, obj);
                        }
                        break;
                    case OPCODE_SET_RECOVER:
                        {
                            uint16_t recover_ip = frame_read_uint16(this->current_frame);
                            this->current_frame->recover_ip = recover_ip;
                        }
                        break;
                    default:
                        {
                            APE_ASSERT(false);
                            errors_add_errorf(this->errors, APE_ERROR_RUNTIME, frame_src_position(this->current_frame), "Unknown opcode: 0x%x", opcode);
                            goto err;
                        }
                        break;
                }
                if(check_time)
                {
                    time_check_counter++;
                    if(time_check_counter > time_check_interval)
                    {
                        int elapsed_ms = (int)ape_timer_get_elapsed_ms(&timer);
                        if(elapsed_ms > max_exec_time_ms)
                        {
                            errors_add_errorf(this->errors, APE_ERROR_TIMEOUT, frame_src_position(this->current_frame),
                                              "Execution took more than %1.17g ms", max_exec_time_ms);
                            goto err;
                        }
                        time_check_counter = 0;
                    }
                }
            err:
                if(errors_get_count(this->errors) > 0)
                {
                    Error* err = errors_get_last_error(this->errors);
                    if(err->type == APE_ERROR_RUNTIME && errors_get_count(this->errors) == 1)
                    {
                        int recover_frame_ix = -1;
                        for(int i = this->frames_count - 1; i >= 0; i--)
                        {
                            Frame* frame = &this->frames[i];
                            if(frame->recover_ip >= 0 && !frame->is_recovering)
                            {
                                recover_frame_ix = i;
                                break;
                            }
                        }
                        if(recover_frame_ix < 0)
                        {
                            goto end;
                        }
                        else
                        {
                            if(!err->traceback)
                            {
                                err->traceback = traceback_make(this->alloc);
                            }
                            if(err->traceback)
                            {
                                traceback_append_from_vm(err->traceback, this);
                            }
                            while(this->frames_count > (recover_frame_ix + 1))
                            {
                                pop_frame(this);
                            }
                            Object err_obj = Object::makeError(this->mem, err->message);
                            if(!err_obj.isNull())
                            {
                                object_set_error_traceback(err_obj, err->traceback);
                                err->traceback = NULL;
                            }
                            stack_push(this, err_obj);
                            this->current_frame->ip = this->current_frame->recover_ip;
                            this->current_frame->is_recovering = true;
                            errors_clear(this->errors);
                        }
                    }
                    else
                    {
                        goto end;
                    }
                }
                if(this->mem->shouldSweep())
                {
                    run_gc(this, constants);
                }
            }

        end:
            if(errors_get_count(this->errors) > 0)
            {
                Error* err = errors_get_last_error(this->errors);
                if(!err->traceback)
                {
                    err->traceback = traceback_make(this->alloc);
                }
                if(err->traceback)
                {
                    traceback_append_from_vm(err->traceback, this);
                }
            }

            run_gc(this, constants);

            this->running = false;
            return errors_get_count(this->errors) == 0;
        }

};

