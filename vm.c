/*
* this file contains the VM of ape.
* it also contains some functions that probably should be in their own files,
* but that's legacy from the original code (where everything was stuffed into one file).
*/

/*
SPDX-License-Identifier: MIT
ape
https://github.com/kgabis/ape
Copyright (c) 2021 Krzysztof Gabis

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "inline.h"

static const ApePosition_t g_vmpriv_srcposinvalid = { NULL, -1, -1 };


void ape_vm_setstackpointer(ApeVM_t *vm, int new_sp);
bool ape_vm_tryoverloadoperator(ApeVM_t *vm, ApeObject_t left, ApeObject_t right, ApeOpByte_t op, bool *out_overload_found);

/*
* todo: these MUST reflect the order of ApeOpcodeValue_t.
* meaning its prone to break terribly if and/or when it is changed.
*/
static ApeOpcodeDef_t g_definitions[APE_OPCODE_MAX + 1] =
{
    { "none", 0, { 0 } },
    { "constant", 1, { 2 } },
    { "op(+)", 0, { 0 } },
    { "pop", 0, { 0 } },
    { "sub", 0, { 0 } },
    { "op(*)", 0, { 0 } },
    { "op(/)", 0, { 0 } },
    { "op(%)", 0, { 0 } },
    { "true", 0, { 0 } },
    { "false", 0, { 0 } },
    { "compare", 0, { 0 } },
    { "compareeq", 0, { 0 } },
    { "equal", 0, { 0 } },
    { "notequal", 0, { 0 } },
    { "greaterthan", 0, { 0 } },
    { "greaterequal", 0, { 0 } },
    { "op(-)", 0, { 0 } },
    { "op(!)", 0, { 0 } },
    { "jump", 1, { 2 } },
    { "jumpiffalse", 1, { 2 } },
    { "jumpiftrue", 1, { 2 } },
    { "null", 0, { 0 } },
    { "getmoduleglobal", 1, { 2 } },
    { "setmoduleglobal", 1, { 2 } },
    { "definemoduleglobal", 1, { 2 } },
    { "array", 1, { 2 } },
    { "mapstart", 1, { 2 } },
    { "mapend", 1, { 2 } },
    { "getthis", 0, { 0 } },
    { "getindex", 0, { 0 } },
    { "setindex", 0, { 0 } },
    { "getvalue_at", 0, { 0 } },
    { "call", 1, { 1 } },
    { "returnvalue", 0, { 0 } },
    { "return", 0, { 0 } },
    { "getlocal", 1, { 1 } },
    { "definelocal", 1, { 1 } },
    { "setlocal", 1, { 1 } },
    { "getcontextglobal", 1, { 2 } },
    { "function", 2, { 2, 1 } },
    { "getfree", 1, { 1 } },
    { "setfree", 1, { 1 } },
    { "currentfunction", 0, { 0 } },
    { "dup", 0, { 0 } },
    { "number", 1, { 8 } },
    { "len", 0, { 0 } },
    { "setrecover", 1, { 2 } },
    { "op(|)", 0, { 0 } },
    { "op(^)", 0, { 0 } },
    { "op(&)", 0, { 0 } },
    { "op(<<)", 0, { 0 } },
    { "op(>>)", 0, { 0 } },
    { "import", 1, {1} },
    { "invalid_max", 0, { 0 } },
};

ApeOpcodeDef_t* ape_vm_opcodefind(ApeOpByte_t op)
{
    if(op <= APE_OPCODE_NONE || op >= APE_OPCODE_MAX)
    {
        return NULL;
    }
    return &g_definitions[op];
}

const char* ape_vm_opcodename(ApeOpByte_t op)
{
    if(op <= APE_OPCODE_NONE || op >= APE_OPCODE_MAX)
    {
        return NULL;
    }
    return g_definitions[op].name;
}

void ape_vm_adderrorv(ApeVM_t* vm, ApeErrorType_t etype, const char* fmt, va_list va)
{
    ape_errorlist_addformatv(vm->errors, etype, ape_frame_srcposition(vm->currentframe), fmt, va);
}

void ape_vm_adderror(ApeVM_t* vm, ApeErrorType_t etype, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    ape_vm_adderrorv(vm, etype, fmt, va);
    va_end(va);
}


ApeGlobalStore_t* ape_make_globalstore(ApeContext_t* ctx, ApeGCMemory_t* mem)
{
    ApeSize_t i;
    ApeSize_t bcount;
    bool ok;
    const char* name;
    ApeObject_t builtin;
    ApeGlobalStore_t* store;
    store = (ApeGlobalStore_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeGlobalStore_t));
    if(!store)
    {
        return NULL;
    }
    memset(store, 0, sizeof(ApeGlobalStore_t));
    store->context = ctx;
    store->named = ape_make_strdict(ctx, (ApeDataCallback_t)ape_symbol_copy, (ApeDataCallback_t)ape_symbol_destroy);
    
    if(!store->named)
    {
        goto err;
    }
    store->objects = ape_make_valarray(ctx, sizeof(ApeObject_t));
    if(!store->objects)
    {
        goto err;
    }
    if(mem)
    {
        bcount = ape_builtins_count();
        for(i = 0; i < bcount; i++)
        {
            name = ape_builtins_getname(i);
            builtin = ape_object_make_nativefuncmemory(ctx, name, ape_builtins_getfunc(i), NULL, 0);
            if(ape_object_value_isnull(builtin))
            {
                goto err;
            }
            ok = ape_globalstore_set(store, name, builtin);
            if(!ok)
            {
                goto err;
            }
        }
    }
    return store;
err:
    ape_globalstore_destroy(store);
    return NULL;
}

void ape_globalstore_destroy(ApeGlobalStore_t* store)
{
    ApeContext_t* ctx;
    if(!store)
    {
        return;
    }
    ctx = store->context;
    ape_strdict_destroywithitems(ctx, store->named);
    ape_valarray_destroy(store->objects);
    ape_allocator_free(&ctx->alloc, store);
}

ApeSymbol_t* ape_globalstore_getsymbol(ApeGlobalStore_t* store, const char* name)
{
    return (ApeSymbol_t*)ape_strdict_getbyname(store->named, name);
}

bool ape_globalstore_set(ApeGlobalStore_t* store, const char* name, ApeObject_t object)
{
    ApeInt_t ix;
    bool ok;
    ApeSymbol_t* symbol;
    const ApeSymbol_t* existing_symbol;
    existing_symbol = ape_globalstore_getsymbol(store, name);
    if(existing_symbol)
    {
        ok = ape_valarray_set(store->objects, existing_symbol->index, &object);
        return ok;
    }
    ix = ape_valarray_count(store->objects);
    ok = ape_valarray_push(store->objects, &object);
    if(!ok)
    {
        return false;
    }
    symbol = ape_make_symbol(store->context, name, APE_SYMBOL_CONTEXTGLOBAL, ix, false);
    if(!symbol)
    {
        goto err;
    }
    ok = ape_strdict_set(store->named, name, symbol);
    if(!ok)
    {
        ape_symbol_destroy(store->context, symbol);
        goto err;
    }
    return true;
err:
    ape_valarray_pop(store->objects);
    return false;
}

ApeObject_t ape_globalstore_getat(ApeGlobalStore_t* store, int ix, bool* out_ok)
{
    ApeObject_t* res = (ApeObject_t*)ape_valarray_get(store->objects, ix);
    if(!res)
    {
        *out_ok = false;
        return ape_object_make_null(store->context);
    }
    *out_ok = true;
    return *res;
}

ApeObject_t* ape_globalstore_getobjectdata(ApeGlobalStore_t* store)
{
    return (ApeObject_t*)ape_valarray_data(store->objects);
}

ApeSize_t ape_globalstore_getobjectcount(ApeGlobalStore_t* store)
{
    return ape_valarray_count(store->objects);
}


#define APPEND_BYTE(n)                           \
    do                                           \
    {                                            \
        val = (ApeUShort_t)(operands[i] >> (n * 8)); \
        ok = ape_valarray_push(res, &val);               \
        if(!ok)                                  \
        {                                        \
            return 0;                            \
        }                                        \
    } while(0)

int ape_make_code(ApeOpByte_t op, ApeSize_t operands_count, ApeOpByte_t* operands, ApeValArray_t* res)
{
    ApeSize_t i;
    int width;
    int instr_len;
    bool ok;
    ApeUShort_t val;
    ApeOpcodeDef_t* def;
    def = ape_vm_opcodefind(op);
    if(!def)
    {
        return 0;
    }
    instr_len = 1;
    for(i = 0; i < def->operandcount; i++)
    {
        instr_len += def->operandwidths[i];
    }
    val = op;
    ok = false;
    ok = ape_valarray_push(res, &val);
    if(!ok)
    {
        return 0;
    }
    for(i = 0; i < operands_count; i++)
    {
        width = def->operandwidths[i];
        switch(width)
        {
            case 1:
                {
                    APPEND_BYTE(0);
                }
                break;
            case 2:
                {
                    APPEND_BYTE(1);
                    APPEND_BYTE(0);
                }
                break;
            case 4:
                {
                    APPEND_BYTE(3);
                    APPEND_BYTE(2);
                    APPEND_BYTE(1);
                    APPEND_BYTE(0);
                }
                break;
            case 8:
                {
                    APPEND_BYTE(7);
                    APPEND_BYTE(6);
                    APPEND_BYTE(5);
                    APPEND_BYTE(4);
                    APPEND_BYTE(3);
                    APPEND_BYTE(2);
                    APPEND_BYTE(1);
                    APPEND_BYTE(0);
                }
                break;
            default:
                {
                    APE_ASSERT(false);
                }
                break;
        }
    }
    return instr_len;
}
#undef APPEND_BYTE


ApeOpcodeValue_t ape_frame_readopcode(ApeFrame_t* frame)
{
    frame->srcip = frame->ip;
    return (ApeOpcodeValue_t)ape_frame_readuint8(frame);
}

ApeOpByte_t ape_frame_readuint64(ApeFrame_t* frame)
{
    ApeOpByte_t res;
    const ApeUShort_t* data;
    data = frame->bytecode + frame->ip;
    frame->ip += 8;
    res = 0;
    res |= (ApeOpByte_t)data[7];
    res |= (ApeOpByte_t)data[6] << 8;
    res |= (ApeOpByte_t)data[5] << 16;
    res |= (ApeOpByte_t)data[4] << 24;
    res |= (ApeOpByte_t)data[3] << 32;
    res |= (ApeOpByte_t)data[2] << 40;
    res |= (ApeOpByte_t)data[1] << 48;
    res |= (ApeOpByte_t)data[0] << 56;
    return res;
}

uint16_t ape_frame_readuint16(ApeFrame_t* frame)
{
    const ApeUShort_t* data;
    data = frame->bytecode + frame->ip;
    frame->ip += 2;
    return (data[0] << 8) | data[1];
}

ApeUShort_t ape_frame_readuint8(ApeFrame_t* frame)
{
    const ApeUShort_t* data;
    data = frame->bytecode + frame->ip;
    frame->ip++;
    return data[0];
}

ApePosition_t ape_frame_srcposition(const ApeFrame_t* frame)
{
    if(frame != NULL)
    {
        if(frame->srcpositions != NULL)
        {
            return frame->srcpositions[frame->srcip];
        }
    }
    return g_vmpriv_srcposinvalid;
}

ApeObject_t ape_vm_getlastpopped(ApeVM_t* vm)
{
    return vm->lastpopped;
}

bool ape_vm_haserrors(ApeVM_t* vm)
{
    return ape_errorlist_count(vm->errors) > 0;
}

bool ape_vm_setglobal(ApeVM_t* vm, ApeSize_t ix, ApeObject_t val)
{
    ape_valdict_set(vm->globalobjects, &ix, &val);
    return true;
}

ApeObject_t ape_vm_getglobal(ApeVM_t* vm, ApeSize_t ix)
{
    ApeObject_t* rt;
    rt = (ApeObject_t*)ape_valdict_getbykey(vm->globalobjects, &ix);
    if(rt == NULL)
    {
        return ape_object_make_null(vm->context);
    }
    return *rt;
}

void ape_vm_setstackpointer(ApeVM_t* vm, int new_sp)
{
    int count;
    size_t bytescount;
    ApeSize_t i;
    ApeSize_t idx;
    idx = vm->stackptr;
    /* to avoid gcing freed objects */
    if((ApeInt_t)new_sp > (ApeInt_t)idx)
    {
        count = new_sp - idx;
        bytescount = count * sizeof(ApeObject_t);
        /*
        * TODO:FIXME: this is probably where the APE_OBJECT_NUMBER might come from.
        * but it's not really obvious, tbh.
        */
        /*
        //memset(vm->stackobjects + vm->stackptr, 0, bytescount);
        //ape_valdict_clear(vm->stackobjects);
        //memset(vm->stackobjects->values + vm->stackptr, 0, bytescount);
        */
        for(i=vm->stackptr; i<bytescount; i++)
        {
            ApeObject_t nullval = ape_object_make_null(vm->context);
            ape_valdict_set(vm->stackobjects, &i, &nullval);
        }
    }
    vm->stackptr = new_sp;
}

void ape_vm_pushstack(ApeVM_t* vm, ApeObject_t obj)
{
    ApeSize_t idx;
#if defined(APE_DEBUG) && (APE_DEBUG == 1)
    if(vm->currentframe)
    {
        ApeFrame_t* frame = vm->currentframe;
        ApeScriptFunction_t* currfunction = ape_object_value_asscriptfunction(frame->function);
        int nl = currfunction->numlocals;
        APE_ASSERT(vm->stackptr >= (frame->basepointer + nl));
    }
#endif
    idx = vm->stackptr;
    ape_valdict_set(vm->stackobjects, &idx, &obj);
    vm->stackptr++;
}

ApeObject_t ape_vm_popstack(ApeVM_t* vm)
{
    ApeSize_t idx;
    ApeObject_t* ptr;
    ApeObject_t objres;
#if defined(APE_DEBUG) && (APE_DEBUG == 1)
    if(vm->stackptr == 0)
    {
        ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe), "stack underflow");
        APE_ASSERT(false);
        return ape_object_make_null(vm->context);
    }
    if(vm->currentframe)
    {
        ApeFrame_t* frame = vm->currentframe;
        ApeScriptFunction_t* currfunction = ape_object_value_asscriptfunction(frame->function);
        int nl = currfunction->numlocals;
        APE_ASSERT((vm->stackptr - 1) >= (frame->basepointer + nl));
    }
#endif
    vm->stackptr--;
    idx = vm->stackptr-0;
    ptr = (ApeObject_t*)ape_valdict_getbykey(vm->stackobjects, &idx);
    if(ptr == NULL)
    {
        return ape_object_make_null(vm->context);
    }
    objres = *ptr;
    /*
    * TODO:FIXME: somewhere along the line, objres.handle->datatype is either not set, or set incorrectly.
    * this right here works, but it shouldn't be necessary.
    * specifically, objres.handle->datatype gets ***sometimes*** set to APE_OBJECT_NONE, and
    * so far i've only observed this when objres.type==APE_OBJECT_NUMBER.
    * very strange, very weird, very heisenbug-ish.
    */
    if(objres.handle != NULL)
    {
        objres.handle->datatype = objres.type;
    }
    vm->lastpopped = objres;
    return objres;
}

ApeObject_t ape_vm_getstack(ApeVM_t* vm, int nth_item)
{
    ApeSize_t ix;
    ApeObject_t* p;
    ix = vm->stackptr - 1 - nth_item;
    p = (ApeObject_t*)ape_valdict_getbykey(vm->stackobjects, &ix);
    if(p == NULL)
    {
        return ape_object_make_null(vm->context);
    }

    return *p;
}

void ape_vm_thispush(ApeVM_t* vm, ApeObject_t obj)
{
#if defined(APE_DEBUG) && (APE_DEBUG == 1)
    if(vm->thisptr >= APE_CONF_SIZE_VM_THISSTACK)
    {
        APE_ASSERT(false);
        ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe), "this stack overflow");
        return;
    }
#endif
    vm->thisobjects[vm->thisptr] = obj;
    vm->thisptr++;
}

ApeObject_t ape_vm_thisget(ApeVM_t* vm, int nth_item)
{
    int ix;
    ix = vm->thisptr - 1 - nth_item;
    if(ix < 0)
    {
        ix = nth_item;
    }
#if defined(APE_DEBUG) && (APE_DEBUG == 1)
    if(ix < 0 || ix >= APE_CONF_SIZE_VM_THISSTACK)
    {
        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "invalid this shared->stack index: %d", nth_item);
        APE_ASSERT(false);
        return ape_object_make_null(vm->context);
    }
#endif
    return vm->thisobjects[ix];
}

ApeObject_t ape_vm_thispop(ApeVM_t* vm)
{
    if(vm->thisptr > 0)
    {
        vm->thisptr--;
    }
    else
    {
        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "this stack underflow");
        return ape_object_make_null(vm->context);
    }
    return ape_vm_thisget(vm, vm->thisptr);
}

ApeObject_t ape_vm_thisparent(ApeVM_t* vm, bool letfail)
{
    ApeSize_t ofs;
    if(vm->thisptr == 0)
    {
        if(letfail)
        {
            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "this stack underflow");
        }
        return ape_object_make_null(vm->context);
    }
    ofs = vm->thisptr - 1;
    return ape_vm_thisget(vm, ofs);
}

void ape_vm_dumpstack(ApeVM_t* vm)
{
    ApeInt_t i;
    ApeSize_t key;
    ApeWriter_t* wr;
    ApeSize_t* keys;
    ApeObject_t* vals;
    keys = ((ApeSize_t*)(vm->stackobjects->keys));
    vals = ((ApeObject_t*)(vm->stackobjects->values));
    wr = ape_make_writerio(vm->context, stderr, false, true);
    {
        for(i=0; i<vm->stackptr; i++)
        {
            key = keys[i];
            fprintf(stderr, "vm->stack[%d] = (%s) = [[", (int)key, ape_object_value_typename(ape_object_value_type(vals[i])));
            ape_tostring_object(wr, vals[i], true);
            fprintf(stderr, "]]\n");
        }
    }
    ape_writer_destroy(wr);
}

ApeObject_t ape_vm_callnativefunction(ApeVM_t* vm, ApeObject_t callee, ApePosition_t src_pos, int argc, ApeObject_t* args)
{
    ApeError_t* err;
    ApeObject_t objres;
    ApeObjType_t restype;
    ApeNativeFunction_t* nfunc;
    ApeTraceback_t* traceback;
    nfunc = ape_object_value_asnativefunction(callee);
    objres = nfunc->nativefnptr(vm, nfunc->dataptr, argc, args);
    if(ape_errorlist_haserrors(vm->errors) && !APE_STREQ(nfunc->name, "crash"))
    {
        err = ape_errorlist_lasterror(vm->errors);
        err->pos = src_pos;
        err->traceback = ape_make_traceback(vm->context);
        if(err->traceback)
        {
            ape_traceback_append(err->traceback, nfunc->name, g_vmpriv_srcposinvalid);
        }
        return ape_object_make_null(vm->context);
    }
    restype = ape_object_value_type(objres);
    if(restype == APE_OBJECT_ERROR)
    {
        traceback = ape_make_traceback(vm->context);
        if(traceback)
        {
            /* error builtin is treated in a special way */
            if(!APE_STREQ(nfunc->name, "error"))
            {
                ape_traceback_append(traceback, nfunc->name, g_vmpriv_srcposinvalid);
            }
            ape_traceback_appendfromvm(traceback, vm);
            ape_object_value_seterrortraceback(objres, traceback);
        }
    }
    return objres;
}

bool ape_vm_callobjectargs(ApeVM_t* vm, ApeObject_t callee, ApeInt_t nargs, ApeObject_t* args)
{
    bool ok;
    ApeSize_t idx;
    ApeInt_t ofs;
    ApeInt_t actualargs;
    ApeObjType_t calleetype;
    ApeScriptFunction_t* scriptcallee;
    ApeFrame_t framecallee;
    ApeObject_t* fwdargs;
    ApeObject_t* stackpos;
    ApeObject_t* stackvals;
    ApeObject_t objres;
    const char* calleetypename;
    (void)scriptcallee;
    calleetype = ape_object_value_type(callee);
    if(calleetype == APE_OBJECT_SCRIPTFUNCTION)
    {
        scriptcallee = ape_object_value_asscriptfunction(callee);
        #if 0
        if(nargs != -1)
        {
            if(nargs != (ApeInt_t)scriptcallee->numargs)
            {
                ape_vm_adderror(vm, APE_ERROR_RUNTIME, "function '%s' expect %d arguments, but got %d",
                                  ape_object_function_getname(callee), scriptcallee->numargs, nargs);
                return false;
            }
        }
        #endif
        ofs = nargs;
        if(nargs == -1)
        {
            ofs = 0;
        }
        ok = ape_vm_frameinit(&framecallee, callee, vm->stackptr - ofs);
        if(!ok)
        {
            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "frame init failed in ape_vm_callobjectargs");
            return false;
        }
        ok = ape_vm_framepush(vm, framecallee);
        if(!ok)
        {
            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "pushing frame failed in ape_vm_callobjectargs");
            return false;
        }
    }
    else if(calleetype == APE_OBJECT_NATIVEFUNCTION)
    {
        ofs = nargs;
        if(nargs == -1)
        {
            ofs = 0;
        }
        idx = vm->stackptr;
        stackvals = ((ApeObject_t*)(vm->stackobjects->values));
        stackpos = stackvals + idx - ofs;
        if(args == NULL)
        {
            fwdargs = stackpos;
        }
        else
        {
            fwdargs = args;
        }
        if(vm->context->config.dumpstack)
        {
            ape_vm_dumpstack(vm);
        }
        actualargs = nargs;
        if(nargs == -1)
        {
            actualargs = vm->stackptr - ofs;
        }
        objres = ape_vm_callnativefunction(vm, callee, ape_frame_srcposition(vm->currentframe), actualargs, fwdargs);
        if(ape_vm_haserrors(vm))
        {
            return false;
        }
        ape_vm_setstackpointer(vm, vm->stackptr - ofs - 1);
        ape_vm_pushstack(vm, objres);
    }
    else
    {
        calleetypename = ape_object_value_typename(calleetype);
        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "%s object is not callable", calleetypename);
        return false;
    }
    return true;
}

bool ape_vm_callobjectstack(ApeVM_t* vm, ApeObject_t callee, ApeInt_t nargs)
{
    return ape_vm_callobjectargs(vm, callee, nargs, NULL);
}

bool ape_vm_checkassign(ApeVM_t* vm, ApeObject_t oldval, ApeObject_t newval)
{
    ApeObjType_t oldtype;
    ApeObjType_t newtype;
    (void)vm;
    oldtype = ape_object_value_type(oldval);
    newtype = ape_object_value_type(newval);
    if(oldtype == APE_OBJECT_NULL || newtype == APE_OBJECT_NULL)
    {
        return true;
    }
    if(oldtype != newtype)
    {
        /*
        * this is redundant, and frankly, unnecessary.
        */
        /*
        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "trying to assign variable of type %s to %s",
                          ape_object_value_typename(newtype), ape_object_value_typename(oldtype));
        return false;
        */
    }
    return true;
}

bool ape_vm_tryoverloadoperator(ApeVM_t* vm, ApeObject_t left, ApeObject_t right, ApeOpByte_t op, bool* out_overload_found)
{
    int numop;
    ApeObject_t key;
    ApeObject_t callee;
    ApeObjType_t lefttype;
    ApeObjType_t righttype;
    *out_overload_found = false;
    lefttype = ape_object_value_type(left);
    righttype = ape_object_value_type(right);
    if(lefttype != APE_OBJECT_MAP && righttype != APE_OBJECT_MAP)
    {
        *out_overload_found = false;
        return true;
    }
    numop = 2;
    if(op == APE_OPCODE_MINUS || op == APE_OPCODE_NOT)
    {
        numop = 1;
    }
    key = vm->overloadkeys[op];
    callee = ape_object_make_null(vm->context);
    if(lefttype == APE_OBJECT_MAP)
    {
        callee = ape_object_map_getvalueobject(left, key);
    }
    if(!ape_object_value_iscallable(callee))
    {
        if(righttype == APE_OBJECT_MAP)
        {
            callee = ape_object_map_getvalueobject(right, key);
        }
        if(!ape_object_value_iscallable(callee))
        {
            *out_overload_found = false;
            return true;
        }
    }
    *out_overload_found = true;
    ape_vm_pushstack(vm, callee);
    ape_vm_pushstack(vm, left);
    if(numop == 2)
    {
        ape_vm_pushstack(vm, right);
    }
    return ape_vm_callobjectstack(vm, callee, numop);
}


#define SET_OPERATOR_OVERLOAD_KEY(op, key)                   \
    do                                                       \
    {                                                        \
        key_obj = ape_object_make_string(ctx, key); \
        if(ape_object_value_isnull(key_obj))                          \
        {                                                    \
            goto err;                                        \
        }                                                    \
        vm->overloadkeys[op] = key_obj;             \
    } while(0)

ApeVM_t* ape_make_vm(ApeContext_t* ctx, const ApeConfig_t* config, ApeGCMemory_t* mem, ApeErrorList_t* errors, ApeGlobalStore_t* global_store)
{
    ApeSize_t i;
    ApeVM_t* vm;
    ApeObject_t key_obj;
    vm = (ApeVM_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeVM_t));
    if(!vm)
    {
        return NULL;
    }
    memset(vm, 0, sizeof(ApeVM_t));
    vm->context = ctx;
    vm->config = config;
    vm->mem = mem;
    vm->errors = errors;
    vm->globalstore = global_store;
    vm->stackptr = 0;
    vm->thisptr = 0;
    vm->countframes = 0;
    vm->lastpopped = ape_object_make_null(ctx);
    vm->running = false;
    vm->globalobjects = ape_make_valdict(ctx, sizeof(ApeSize_t), sizeof(ApeObject_t));
    vm->stackobjects = ape_make_valdict(ctx, sizeof(ApeSize_t), sizeof(ApeObject_t));
    vm->lastframe = NULL;
    vm->frameobjects = deqlist_create_empty(ctx, sizeof(ApeFrame_t));
    for(i = 0; i < APE_OPCODE_MAX; i++)
    {
        vm->overloadkeys[i] = ape_object_make_null(ctx);
    }
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_ADD, "__operator_add__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_SUB, "__operator_sub__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_MUL, "__operator_mul__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_DIV, "__operator_div__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_MOD, "__operator_mod__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_BITOR, "__operator_or__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_BITXOR, "__operator_xor__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_BITAND, "__operator_and__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_LEFTSHIFT, "__operator_lshift__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_RIGHTSHIFT, "__operator_rshift__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_MINUS, "__operator_minus__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_NOT, "__operator_bang__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_COMPAREPLAIN, "__cmp__");
    #if 1
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_GETINDEX, "__getindex__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_SETINDEX, "__setindex__");
    SET_OPERATOR_OVERLOAD_KEY(APE_OPCODE_CALL, "__call__");
    #endif
    return vm;
err:
    ape_vm_destroy(vm);
    return NULL;
}
#undef SET_OPERATOR_OVERLOAD_KEY

void ape_vm_destroy(ApeVM_t* vm)
{
    ApeContext_t* ctx;
    ApeFrame_t* popped;
    (void)popped;
    if(!vm)
    {
        return;
    }
    ctx = vm->context;
    ape_valdict_destroy(vm->globalobjects);
    ape_valdict_destroy(vm->stackobjects);
    fprintf(stderr, "deqlist_count(vm->frameobjects)=%d\n", deqlist_count(vm->frameobjects));
    if(deqlist_count(vm->frameobjects) != 0)
    {
        while(deqlist_count(vm->frameobjects) != 0)
        {
            popped = (ApeFrame_t*)deqlist_get(vm->frameobjects, deqlist_count(vm->frameobjects)-1);
            if(popped != NULL)
            {
                //if(popped->allocated)
                {
                    ape_allocator_free(&vm->context->alloc, popped);
                }
            }
            deqlist_pop(&vm->frameobjects);
        }
    }
    deqlist_destroy(vm->frameobjects);
    ape_allocator_free(&ctx->alloc, vm);
}

void ape_vm_reset(ApeVM_t* vm)
{
    vm->stackptr = 0;
    vm->thisptr = 0;
    while(vm->countframes > 0)
    {
        ape_vm_framepop(vm);
    }
}


bool ape_vm_frameinit(ApeFrame_t* frame, ApeObject_t funcobj, int bptr)
{
    ApeScriptFunction_t* function;
    if(ape_object_value_type(funcobj) != APE_OBJECT_SCRIPTFUNCTION)
    {
        return false;
    }
    function = ape_object_value_asscriptfunction(funcobj);
    frame->function = funcobj;
    frame->ip = 0;
    frame->basepointer = bptr;
    frame->srcip = 0;
    frame->bytecode = function->compiledcode->bytecode;
    frame->srcpositions = function->compiledcode->srcpositions;
    frame->bcsize = function->compiledcode->count;
    frame->recoverip = -1;
    frame->isrecovering = false;
    return true;
}

/*
* this function updates an existing stack-alloc'd frame.
* since frames only hold primitive data (integrals and pointers), NO copying
* beyond primitives is done here.
* if a frame needs to copy something, do so in ape_frame_copyalloc.
*/
ApeFrame_t* ape_frame_update(ApeVM_t* vm, ApeFrame_t* rt, ApeFrame_t* from)
{
    (void)vm;
    rt->allocated = true;
    rt->function = from->function;
    rt->srcpositions = from->srcpositions;
    rt->bytecode = from->bytecode;
    rt->ip = from->ip;
    rt->basepointer = from->basepointer;
    rt->srcip = from->srcip;
    rt->bcsize = from->bcsize;
    rt->recoverip = from->recoverip;
    rt->isrecovering = from->isrecovering;
    return rt;
}

/*
* creates a new frame with data provided by an existing frame.
*/
ApeFrame_t* ape_frame_copyalloc(ApeVM_t* vm, ApeFrame_t* from)
{
    ApeFrame_t* rt;
    rt = (ApeFrame_t*)ape_allocator_alloc(&vm->context->alloc, sizeof(ApeFrame_t));
    ape_frame_update(vm, rt, from);
    return rt;
}

bool ape_vm_framepush(ApeVM_t* vm, ApeFrame_t frame)
{
    ApeFrame_t* pf;
    ApeFrame_t* tmp;
    ApeScriptFunction_t* fn;
    /*
    * when a new frame exceeds existing the frame list, then
    * allocate a new one from $frame ...
    * ... otherwise, copy data from $frame, and stuff it back into the frame list.
    * this saves a considerable amount of memory by reusing existing memory.
    */
    if(vm->countframes >= deqlist_count(vm->frameobjects))
    {
        pf = ape_frame_copyalloc(vm, &frame);
        deqlist_push(&vm->frameobjects, pf);
    }
    else
    {
        tmp = (ApeFrame_t*)deqlist_get(vm->frameobjects, vm->countframes);
        pf = ape_frame_update(vm, tmp, &frame);
        deqlist_set(vm->frameobjects, vm->countframes, pf);
    }
    vm->lastframe = pf;
    vm->currentframe = (ApeFrame_t*)deqlist_get(vm->frameobjects, vm->countframes);
    vm->countframes++;
    fn = ape_object_value_asscriptfunction(frame.function);
    ape_vm_setstackpointer(vm, frame.basepointer + fn->numlocals);
    return true;
}

bool ape_vm_framepop(ApeVM_t* vm)
{
    ApeFrame_t* popped;
    (void)popped;
    ape_vm_setstackpointer(vm, vm->currentframe->basepointer - 1);
    if(vm->countframes <= 0)
    {
        APE_ASSERT(false);
        vm->currentframe = NULL;
        return false;
    }
    vm->countframes--;
    if(vm->countframes == 0)
    {
        vm->currentframe = NULL;
        return false;
    }
    popped = (ApeFrame_t*)deqlist_get(vm->frameobjects, vm->countframes);
    /*
    * don't deallocate $popped here - they'll be deallocated in ape_vm_destroy.
    */
    vm->currentframe = (ApeFrame_t*)deqlist_get(vm->frameobjects, vm->countframes - 1);
    return true;
}

void ape_vm_collectgarbage(ApeVM_t* vm, ApeValArray_t* constants, bool alsostack)
{
    ApeSize_t i;
    ApeFrame_t* frame;
    ape_gcmem_unmarkall(vm->mem);
    ape_gcmem_markobjlist(ape_globalstore_getobjectdata(vm->globalstore), ape_globalstore_getobjectcount(vm->globalstore));
    if(constants != NULL)
    {
        ape_gcmem_markobjlist((ApeObject_t*)ape_valarray_data(constants), ape_valarray_count(constants));
    }
    ape_gcmem_markobjlist((ApeObject_t*)(vm->globalobjects->values), vm->globalobjects->count);
    for(i = 0; i < vm->countframes; i++)
    {
        frame = (ApeFrame_t*)deqlist_get(vm->frameobjects, i);
        ape_gcmem_markobject(frame->function);
    }
    if(alsostack)
    {
        ape_gcmem_markobjlist(((ApeObject_t*)(vm->stackobjects->values)), vm->stackptr);
        ape_gcmem_markobjlist(vm->thisobjects, vm->thisptr);
    }
    ape_gcmem_markobject(vm->lastpopped);
    ape_gcmem_markobjlist(vm->overloadkeys, APE_OPCODE_MAX);
    ape_gcmem_sweep(vm->mem);
}

bool ape_vm_run(ApeVM_t* vm, ApeAstCompResult_t* comp_res, ApeValArray_t * constants)
{
    bool res;
    int old_sp;
    int old_this_sp;
    ApeSize_t old_frames_count;
    ApeObject_t main_fn;
    (void)old_sp;
    old_sp = vm->stackptr;
    old_this_sp = vm->thisptr;
    old_frames_count = vm->countframes;
    main_fn = ape_object_make_function(vm->context, "__main__", comp_res, false, 0, 0, 0);
    if(ape_object_value_isnull(main_fn))
    {
        return false;
    }
    ape_vm_pushstack(vm, main_fn);
    res = ape_vmdo_executefunction(vm, main_fn, constants);
    while(vm->countframes > old_frames_count)
    {
        ape_vm_framepop(vm);
    }
    APE_ASSERT(vm->stackptr == old_sp);
    vm->thisptr = old_this_sp;
    return res;
}

ApeObject_t ape_object_string_copy(ApeContext_t* ctx, ApeObject_t obj)
{
    return ape_object_make_stringlen(ctx, ape_object_string_getdata(obj), ape_object_string_getlength(obj));
}

bool ape_vmdo_appendstring(ApeVM_t* vm, ApeObject_t left, ApeObject_t right, ApeObjType_t lefttype, ApeObjType_t righttype)
{
    bool ok;
    const char* leftstr;
    const char* rightstr;
    ApeObject_t objres;
    ApeInt_t buflen;
    ApeInt_t leftlen;
    ApeInt_t rightlen;
    ApeWriter_t* allbuf;
    ApeWriter_t* tostrbuf;
    (void)lefttype;
    if(righttype == APE_OBJECT_STRING)
    {
        leftlen = (int)ape_object_string_getlength(left);
        rightlen = (int)ape_object_string_getlength(right);
        /* avoid doing unnecessary copying by reusing the origin as-is */
        if(leftlen == 0)
        {
            ape_vm_pushstack(vm, right);
        }
        else if(rightlen == 0)
        {
            ape_vm_pushstack(vm, left);
        }
        else
        {
            leftstr = ape_object_string_getdata(left);
            rightstr = ape_object_string_getdata(right);
            objres = ape_object_make_stringcapacity(vm->context, leftlen + rightlen);
            if(ape_object_value_isnull(objres))
            {
                return false;
            }

            ok = ape_object_string_append(vm->context, objres, leftstr, leftlen);
            if(!ok)
            {
                return false;
            }
            ok = ape_object_string_append(vm->context, objres, rightstr, rightlen);
            if(!ok)
            {
                return false;
            }
            ape_vm_pushstack(vm, objres);
        }
    }
    else
    {
        /*
        * when 'right' is not a string, stringify it, and create a new string.
        * in short, this does 'left = left + tostring(right)'
        * TODO: probably really inefficient.
        */
        allbuf = ape_make_writer(vm->context);
        tostrbuf = ape_make_writer(vm->context);
        ape_writer_appendlen(allbuf, ape_object_string_getdata(left), ape_object_string_getlength(left));
        ape_tostring_object(tostrbuf, right, false);
        ape_writer_appendlen(allbuf, ape_writer_getdata(tostrbuf), ape_writer_getlength(tostrbuf));
        buflen = ape_writer_getlength(allbuf);
        objres = ape_object_make_stringcapacity(vm->context, buflen);
        ok = ape_object_string_append(vm->context, objres, ape_writer_getdata(allbuf), buflen);
        ape_writer_destroy(tostrbuf);
        ape_writer_destroy(allbuf);
        if(!ok)
        {
            return false;
        }

        ape_vm_pushstack(vm, objres);
    }
    return true;
}


bool ape_vmdo_getindex(ApeVM_t* vm, ApeObject_t left, ApeObject_t index, ApeObjType_t lefttype, ApeObjType_t indextype)
{
    bool canindex;
    unsigned long nhash;
    const char* str;
    const char* idxname;
    const char* indextn;
    const char* lefttn;
    ApeSize_t idxlen;
    ApeInt_t ix;
    ApeInt_t leftlen;
    ApeObject_t objres;
    (void)idxname;
    canindex = false;
    /*
    * todo: object method lookup could be implemented here
    */
    {
        ApeObject_t objfn;
        ApeObject_t objval;
        ApeObjMemberItem_t* afn;
        if(indextype == APE_OBJECT_STRING)
        {
            idxname = ape_object_string_getdata(index);
            idxlen = ape_object_string_getlength(index);
            nhash = ape_util_hashstring(idxname, idxlen);
            if((afn = builtin_get_object(vm->context, lefttype, idxname, nhash)) != NULL)
            {
                objval = ape_object_make_null(vm->context);
                if(afn->isfunction)
                {
                    /*
                    * "normal" functions are pushed as generic, run-of-the-mill functions, except
                    * that "this" is also pushed.
                    */
                    objfn = ape_object_make_nativefuncmemory(vm->context, idxname, afn->fn, NULL, 0);
                    ape_vm_thispush(vm, left);
                    objval = objfn;
                }
                else
                {
                    /*
                    * "non" functions (pseudo functions) like "length" receive no arguments whatsover,
                    * only "this" is pushed.
                    */
                    ape_vm_thispush(vm, left);
                    objval = afn->fn(vm, NULL, 0, NULL);
                }
                ape_vm_pushstack(vm, objval);
                return true;
            }
        }
    }
    canindex = (
        (lefttype == APE_OBJECT_ARRAY) ||
        (lefttype == APE_OBJECT_MAP) ||
        (lefttype == APE_OBJECT_STRING)
    );
    if(!canindex)
    {
        lefttn = ape_object_value_typename(lefttype);
        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "cannot index type '%s' (in APE_OPCODE_GETINDEX)", lefttn);
        return false;
    }
    objres = ape_object_make_null(vm->context);
    if(lefttype == APE_OBJECT_ARRAY)
    {
        if(!ape_object_type_isnumeric(indextype))
        {
            lefttn = ape_object_value_typename(lefttype);
            indextn = ape_object_value_typename(indextype);
            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "cannot index %s with %s", lefttn, indextn);
            return false;
        }
        ix = (int)ape_object_value_asnumber(index);
        if(ix < 0)
        {
            ix = ape_object_array_getlength(left) + ix;
        }
        if(ix >= 0 && ix < (ApeInt_t)ape_object_array_getlength(left))
        {
            objres = ape_object_array_getvalue(left, ix);
        }
    }
    else if(lefttype == APE_OBJECT_MAP)
    {
        objres = ape_object_map_getvalueobject(left, index);
    }
    else if(lefttype == APE_OBJECT_STRING)
    {
        str = ape_object_string_getdata(left);
        leftlen = ape_object_string_getlength(left);
        ix = (int)ape_object_value_asnumber(index);
        if(ix >= 0 && ix < leftlen)
        {
            char res_str[2] = { str[ix], '\0' };
            objres = ape_object_make_string(vm->context, res_str);
        }
    }
    ape_vm_pushstack(vm, objres);
    return true;
}

bool ape_vmdo_math(ApeVM_t* vm, ApeObject_t left, ApeObject_t right, ApeOpcodeValue_t opcode)
{
    bool ok;
    bool isfixed;
    bool overloadfound;
    const char* lefttn;
    const char* righttn;
    const char* opcname;
    ApeFloat_t resfloat;
    ApeInt_t resfixed;
    ApeInt_t leftint;
    ApeInt_t rightint;
    ApeObjType_t lefttype;
    ApeObjType_t righttype;
    isfixed = false;
    /* NULL to 0 coercion */
    if(ape_object_value_isnumeric(left) && ape_object_value_isnull(right))
    {
        if(ape_object_value_isfixednumber(left))
        {
            right = ape_object_make_fixednumber(vm->context, 0);
        }
        else
        {
            right = ape_object_make_floatnumber(vm->context, 0);
        }
    }
    if(ape_object_value_isnumeric(right) && ape_object_value_isnull(left))
    {
        if(ape_object_value_isfixednumber(right))
        {
            left = ape_object_make_fixednumber(vm->context, 0);
        }
        else
        {
            left = ape_object_make_floatnumber(vm->context, 0);
        }
    }
    lefttype = ape_object_value_type(left);
    righttype = ape_object_value_type(right);
    if(ape_object_value_isnumeric(left) && ape_object_value_isnumeric(right))
    {
        resfloat = 0;
        resfixed = 0;
        //fprintf(stderr, "ape_vmdo_math: right.type=%s left.type=%s\n", ape_object_value_typename(right.type), ape_object_value_typename(left.type));
        switch(opcode)
        {
            case APE_OPCODE_ADD:
                {
                    ApeFloat_t rightval = ape_object_value_asnumber(right);
                    ApeFloat_t leftval = ape_object_value_asnumber(left);        
                    resfloat = leftval + rightval;
                }
                break;
            case APE_OPCODE_SUB:
                {
                    ApeFloat_t rightval = ape_object_value_asnumber(right);
                    ApeFloat_t leftval = ape_object_value_asnumber(left);
                    resfloat = leftval - rightval;
                }
                break;
            case APE_OPCODE_MUL:
                {
                    ApeFloat_t rightval = ape_object_value_asnumber(right);
                    ApeFloat_t leftval = ape_object_value_asnumber(left);
                    resfloat = leftval * rightval;
                }
                break;
            case APE_OPCODE_DIV:
                {
                    ApeFloat_t rightval = ape_object_value_asnumber(right);
                    ApeFloat_t leftval = ape_object_value_asnumber(left);
                    resfloat = leftval / rightval;
                }
                break;
            case APE_OPCODE_MOD:
                {
                    if(ape_object_value_isfloatnumber(left))
                    {
                        ApeFloat_t rightval = ape_object_value_asnumber(right);
                        ApeFloat_t leftval = ape_object_value_asnumber(left);
                        leftint = ape_util_numbertoint32(leftval);
                        rightint = ape_util_numbertoint32(rightval);                    

                        resfloat = fmod(leftval, rightval);
                        isfixed = false;
                    }
                    else
                    {
                        ApeInt_t rightval = ape_object_value_asfixednumber(right);
                        ApeInt_t leftval = ape_object_value_asfixednumber(left);
                        resfixed = (leftval % rightval);
                        isfixed = true;
                    }
                }
                break;
            case APE_OPCODE_BITOR:
                {
                    ApeInt_t rightval = ape_object_value_asfixednumber(right);
                    ApeInt_t leftval = ape_object_value_asfixednumber(left);
                    leftint = (leftval);
                    rightint = (rightval);
                    //fprintf(stderr, "BITOR: rightval=%ld leftval=%ld leftint=%ld rightint=%ld\n", rightval, leftval, leftint, rightint);
                    resfixed = (leftint | rightint);
                    isfixed = true;
                }
                break;
            case APE_OPCODE_BITXOR:
                {
                    ApeInt_t rightval = ape_object_value_asnumber(right);
                    ApeInt_t leftval = ape_object_value_asnumber(left);
                    leftint = leftval;
                    rightint = rightval;
                    resfixed = (leftint ^ rightint);
                    isfixed = true;
                }
                break;
            case APE_OPCODE_BITAND:
                {
                    ApeInt_t rightval = ape_object_value_asnumber(right);
                    ApeInt_t leftval = ape_object_value_asnumber(left);
                    leftint = (leftval);
                    rightint = (rightval);
                    resfixed = (ApeInt_t)(leftint & rightint);
                    isfixed = true;
                }
                break;
            case APE_OPCODE_LEFTSHIFT:
                /*
                {
                    int uleft = ape_util_numbertoint32(leftval);
                    unsigned int uright = ape_util_numbertouint32(rightval);
                    bigres = (uleft << (uright & 0x1F));
                }
                */
                {
                    ApeFloat_t rightval = ape_object_value_asnumber(right);
                    ApeFloat_t leftval = ape_object_value_asnumber(left);
                    int uleft = ape_util_numbertoint32(leftval);
                    unsigned int uright = ape_util_numbertouint32(rightval);
                    resfixed = (uleft << (uright & 0x1F));
                    isfixed = true;
                }
                break;
            case APE_OPCODE_RIGHTSHIFT:
                /*
                {
                    int uleft = ape_util_numbertoint32(leftval);
                    unsigned int uright = ape_util_numbertouint32(rightval);
                    bigres = (uleft >> (uright & 0x1F));
                }
                */
                {
                    ApeFloat_t rightval = ape_object_value_asnumber(right);
                    ApeFloat_t leftval = ape_object_value_asnumber(left);
                    int uleft = ape_util_numbertoint32(leftval);
                    unsigned int uright = ape_util_numbertouint32(rightval);
                    resfixed = (uleft >> (uright & 0x1F));
                    isfixed = true;
                }
                break;
            default:
                {
                    APE_ASSERT(false);
                }
                break;
        }
        if(isfixed)
        {
            ape_vm_pushstack(vm, ape_object_make_fixednumber(vm->context, resfixed));
        }
        else
        {
            ape_vm_pushstack(vm, ape_object_make_floatnumber(vm->context, resfloat));
        }
    }
    else if(lefttype == APE_OBJECT_STRING /*&& ((righttype == APE_OBJECT_STRING) || (righttype == APE_OBJECT_NUMBER))*/ && opcode == APE_OPCODE_ADD)
    {
        ok = ape_vmdo_appendstring(vm, left, right, lefttype, righttype);
        if(!ok)
        {
            return false;
        }
    }
    else if((lefttype == APE_OBJECT_ARRAY) && opcode == APE_OPCODE_ADD)
    {
        ape_object_array_pushvalue(left, right);
        ape_vm_pushstack(vm, left);
    }
    else
    {
        overloadfound = false;
        ok = ape_vm_tryoverloadoperator(vm, left, right, opcode, &overloadfound);
        if(!ok)
        {
            return false;
        }
        if(!overloadfound)
        {
            opcname = ape_vm_opcodename(opcode);
            lefttn = ape_object_value_typename(lefttype);
            righttn = ape_object_value_typename(righttype);
            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "invalid operand types for %s, got %s and %s", opcname, lefttn, righttn);
            return false;
        }
    }
    return true;
}


/*
* TODO: there is A LOT of code in this function.
* some could be easily split into other functions.
*/
bool ape_vmdo_executefunction(ApeVM_t* vm, ApeObject_t function, ApeValArray_t * constants)
{
    /*
    * this is stupid, but old ANSI compilers require decls.
    * that being said, ape_vmdo_executefunction should be split
    * into smaller functions
    */
    bool isoverloaded;
    bool ok;
    bool overloadfound;
    bool resval;
    const ApeScriptFunction_t* constfn;
    const char* indextn;
    const char* keytypename;
    const char* lefttypestr;
    const char* lefttn;
    const char* optypestr;
    const char* righttypestr;
    const char* str;
    const char* ctypn;
    ApeSize_t i;
    int ix;
    int leftlen;
    int len;
    int ixrecover;
    ApeInt_t ui;
    ApeUInt_t ixconst;
    ApeUInt_t count;
    ApeUInt_t itmcount;
    ApeUInt_t kvpcount;
    ApeUInt_t recip;
    ApeInt_t pos;
    ApeUShort_t ixfree;
    ApeUShort_t nargs;
    ApeUShort_t numfree;
    ApeSize_t idx;
    ApeSize_t tmpi;
    ApeSize_t kvstart;
    ApeFloat_t cres;
    ApeFloat_t valdouble;
    ApeObjType_t constype;
    ApeObjType_t indextype;
    ApeObjType_t keytype;
    ApeObjType_t lefttype;
    ApeObjType_t opertype;
    ApeObjType_t type;
    ApeObject_t arrayobj;
    ApeObject_t callee;
    ApeObject_t currfunction;
    ApeObject_t errobj;
    ApeObject_t freeval;
    ApeObject_t funcobj;
    ApeObject_t global;
    ApeObject_t index;
    ApeObject_t key;
    ApeObject_t left;
    ApeObject_t map_obj;
    ApeObject_t newvalue;
    ApeObject_t objres;
    ApeObject_t objval;
    ApeObject_t oldvalue;
    ApeObject_t operand;
    ApeObject_t right;
    ApeObject_t testobj;
    ApeObject_t value;
    ApeObject_t popped;
    ApeObject_t* stackvals;
    ApeObject_t* vconst;
    ApeObject_t* items;
    ApeOpcodeValue_t opcode;
    ApeFrame_t* frame;
    ApeError_t* err;
    ApeFrame_t newframe;
    ApeScriptFunction_t* scriptfunc;
    (void)popped;
    (void)stackvals;
    #if 0
    if(vm->running)
    {
        ape_errorlist_add(vm->errors, APE_ERROR_USER, g_vmpriv_srcposinvalid, "VM is already executing code");
        return false;
    }
    #endif
    scriptfunc = ape_object_value_asscriptfunction(function);
    ok = false;
    ok = ape_vm_frameinit(&newframe, function, vm->stackptr - scriptfunc->numargs);
    if(!ok)
    {
        return false;
    }
    ok = ape_vm_framepush(vm, newframe);
    if(!ok)
    {
        ape_errorlist_add(vm->errors, APE_ERROR_USER, g_vmpriv_srcposinvalid, "pushing frame failed");
        return false;
    }
    vm->running = true;
    vm->lastpopped = ape_object_make_null(vm->context);
    while(vm->currentframe->ip < (ApeInt_t)vm->currentframe->bcsize)
    {
        opcode = ape_frame_readopcode(vm->currentframe);
        switch(opcode)
        {
            case APE_OPCODE_CONSTANT:
                {
                    ixconst = ape_frame_readuint16(vm->currentframe);
                    vconst = (ApeObject_t*)ape_valarray_get(constants, ixconst);
                    if(!vconst)
                    {
                        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "constant at %d not found", ixconst);
                        goto err;
                    }
                    ape_vm_pushstack(vm, *vconst);
                }
                break;
            case APE_OPCODE_ADD:
            case APE_OPCODE_SUB:
            case APE_OPCODE_MUL:
            case APE_OPCODE_DIV:
            case APE_OPCODE_MOD:
            case APE_OPCODE_BITOR:
            case APE_OPCODE_BITXOR:
            case APE_OPCODE_BITAND:
            case APE_OPCODE_LEFTSHIFT:
            case APE_OPCODE_RIGHTSHIFT:
                {
                    right = ape_vm_popstack(vm);
                    left = ape_vm_popstack(vm);
                    ok = ape_vmdo_math(vm, left, right, opcode);
                    if(!ok)
                    {
                        goto err;
                    }
                }
                break;
            case APE_OPCODE_POP:
                {
                    ape_vm_popstack(vm);
                }
                break;
            case APE_OPCODE_TRUE:
                {
                    ape_vm_pushstack(vm, ape_object_make_bool(vm->context, true));
                }
                break;
            case APE_OPCODE_FALSE:
                {
                    ape_vm_pushstack(vm, ape_object_make_bool(vm->context, false));
                }
                break;
            case APE_OPCODE_COMPAREPLAIN:
            case APE_OPCODE_COMPAREEQUAL:
                {
                    right = ape_vm_popstack(vm);
                    left = ape_vm_popstack(vm);
                    isoverloaded = false;
                    ok = ape_vm_tryoverloadoperator(vm, left, right, APE_OPCODE_COMPAREPLAIN, &isoverloaded);
                    if(!ok)
                    {
                        goto err;
                    }
                    /**
                    * TODO:NOTE:WARNING:
                    * objres absolutely positively MUST be a floating point, even though
                    * it seems unnecessary. no idea why this would break comparison,
                    * but it does. so, no touchy.
                    */
                    if(!isoverloaded)
                    {
                        cres = ape_object_value_compare(left, right, &ok);
                        if(ok || opcode == APE_OPCODE_COMPAREEQUAL)
                        {
                            objres = ape_object_make_floatnumber(vm->context, cres);
                            ape_vm_pushstack(vm, objres);
                        }
                        else
                        {
                            righttypestr = ape_object_value_typename(ape_object_value_type(right));
                            lefttypestr = ape_object_value_typename(ape_object_value_type(left));
                            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "cannot compare %s and %s", lefttypestr, righttypestr);
                            goto err;
                        }
                    }
                }
                break;

            case APE_OPCODE_ISEQUAL:
            case APE_OPCODE_NOTEQUAL:
            case APE_OPCODE_GREATERTHAN:
            case APE_OPCODE_GREATEREQUAL:
                {
                    value = ape_vm_popstack(vm);
                    cres = ape_object_value_asfixednumber(value);
                    resval = false;
                    //fprintf(stderr, "comparison: value.type=%s n=%d\n", ape_object_value_typename(ape_object_value_type(value)), ape_object_value_asfixednumber(value));
                    switch(opcode)
                    {
                        case APE_OPCODE_ISEQUAL:
                            {                                
                                if(ape_object_value_isfixednumber(value))
                                {
                                    resval = (cres == 0);
                                }
                                else
                                {
                                    resval = APE_DBLEQ(cres, 0);
                                }
                            }
                            break;
                        case APE_OPCODE_NOTEQUAL:
                            {
                                if(ape_object_value_isfixednumber(value))
                                {
                                    resval = !(cres == 0);
                                }
                                else
                                {
                                    resval = !APE_DBLEQ(cres, 0);
                                }
                            }
                            break;
                        case APE_OPCODE_GREATERTHAN:
                            {
                                resval = cres > 0;
                            }
                            break;
                        case APE_OPCODE_GREATEREQUAL:
                            {
                                if(ape_object_value_isfixednumber(value))
                                {
                                    resval = ((cres > 0) || (cres == 0));
                                }
                                else
                                {
                                    resval = cres > 0 || APE_DBLEQ(cres, 0);
                                }
                            }
                            break;
                        default:
                            {
                                APE_ASSERT(false);
                            }
                            break;
                    }
                    objres = ape_object_make_bool(vm->context, resval);
                    ape_vm_pushstack(vm, objres);
                }
                break;
            case APE_OPCODE_MINUS:
                {
                    operand = ape_vm_popstack(vm);
                    opertype = ape_object_value_type(operand);
                    if(ape_object_type_isnumeric(opertype))
                    {
                        if(ape_object_value_isfixednumber(operand))
                        {
                            ApeInt_t vi = ape_object_value_asfixednumber(operand);
                            objres = ape_object_make_fixednumber(vm->context, -vi);
                        }
                        else
                        {
                            ApeFloat_t vi = ape_object_value_asfloatnumber(operand);
                            objres = ape_object_make_floatnumber(vm->context, -vi);
                        }
                        ape_vm_pushstack(vm, objres);
                    }
                    else
                    {
                        overloadfound = false;
                        ok = ape_vm_tryoverloadoperator(vm, operand, ape_object_make_null(vm->context), APE_OPCODE_MINUS, &overloadfound);
                        if(!ok)
                        {
                            goto err;
                        }
                        if(!overloadfound)
                        {
                            optypestr = ape_object_value_typename(opertype);
                            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "invalid operand type for MINUS, got %s", optypestr);
                            goto err;
                        }
                    }
                }
                break;
            case APE_OPCODE_NOT:
                {
                    operand = ape_vm_popstack(vm);
                    type = ape_object_value_type(operand);
                    if(type == APE_OBJECT_BOOL)
                    {
                        objres = ape_object_make_bool(vm->context, !ape_object_value_asbool(operand));
                        ape_vm_pushstack(vm, objres);
                    }
                    else if(type == APE_OBJECT_NULL)
                    {
                        objres = ape_object_make_bool(vm->context, true);
                        ape_vm_pushstack(vm, objres);
                    }
                    else
                    {
                        overloadfound = false;
                        ok = ape_vm_tryoverloadoperator(vm, operand, ape_object_make_null(vm->context), APE_OPCODE_NOT, &overloadfound);
                        if(!ok)
                        {
                            goto err;
                        }
                        if(!overloadfound)
                        {
                            ApeObject_t objres = ape_object_make_bool(vm->context, false);
                            ape_vm_pushstack(vm, objres);
                        }
                    }
                }
                break;
            case APE_OPCODE_JUMP:
                {
                    pos = ape_frame_readuint16(vm->currentframe);
                    vm->currentframe->ip = pos;
                }
                break;
            case APE_OPCODE_JUMPIFFALSE:
                {
                    pos = ape_frame_readuint16(vm->currentframe);
                    testobj = ape_vm_popstack(vm);
                    if(!ape_object_value_asbool(testobj))
                    {
                        vm->currentframe->ip = pos;
                    }
                }
                break;
            case APE_OPCODE_JUMPIFTRUE:
                {
                    pos = ape_frame_readuint16(vm->currentframe);
                    testobj = ape_vm_popstack(vm);
                    if(ape_object_value_asbool(testobj))
                    {
                        vm->currentframe->ip = pos;
                    }
                }
                break;
            case APE_OPCODE_NULL:
                {
                    ape_vm_pushstack(vm, ape_object_make_null(vm->context));
                }
                break;
            case APE_OPCODE_DEFMODULEGLOBAL:
                {
                    ix = ape_frame_readuint16(vm->currentframe);
                    objval = ape_vm_popstack(vm);
                    ape_vm_setglobal(vm, ix, objval);
                }
                break;
            case APE_OPCODE_SETMODULEGLOBAL:
                {
                    ix = ape_frame_readuint16(vm->currentframe);
                    newvalue = ape_vm_popstack(vm);
                    oldvalue = ape_vm_getglobal(vm, ix);
                    if(!ape_vm_checkassign(vm, oldvalue, newvalue))
                    {
                        goto err;
                    }
                    ape_vm_setglobal(vm, ix, newvalue);
                }
                break;
            case APE_OPCODE_GETMODULEGLOBAL:
                {
                    ix = ape_frame_readuint16(vm->currentframe);
                    global = ape_vm_getglobal(vm, ix);
                    ape_vm_pushstack(vm, global);
                }
                break;
            case APE_OPCODE_MKARRAY:
                {
                    count = ape_frame_readuint16(vm->currentframe);
                    arrayobj = ape_object_make_arraycapacity(vm->context, count);
                    if(ape_object_value_isnull(arrayobj))
                    {
                        goto err;
                    }
                    idx = vm->stackptr;
                    items = ((ApeObject_t*)(vm->stackobjects->values)) + idx - count;
                    for(i = 0; i < count; i++)
                    {
                        ApeObject_t item = items[i];
                        ok = ape_object_array_pushvalue(arrayobj, item);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                    ape_vm_setstackpointer(vm, vm->stackptr - count);
                    ape_vm_pushstack(vm, arrayobj);
                }
                break;
            case APE_OPCODE_MAPSTART:
                {
                    count = ape_frame_readuint16(vm->currentframe);
                    map_obj = ape_object_make_mapcapacity(vm->context, count);
                    if(ape_object_value_isnull(map_obj))
                    {
                        goto err;
                    }
                    ape_vm_thispush(vm, map_obj);
                }
                break;
            case APE_OPCODE_MAPEND:
                {
                    kvpcount = ape_frame_readuint16(vm->currentframe);
                    itmcount = kvpcount * 2;
                    map_obj = ape_vm_thispop(vm);
                    stackvals = ((ApeObject_t*)(vm->stackobjects->values));
                    idx = vm->stackptr;
                    /*
                    * previously, extracting key->value pairs was as straight-forward
                    * as stackobjects[N] for the key and stackobjects[N+1] for the value.
                    * with stackobjects being a dict, their locations (indices) are
                    * the keys in stackobjects, and kvstart is the offset in which
                    * the active opcode operates in.
                    * thus, simplified:
                    *   key = stackobjects[(((stackposition - length(items)) + index) + 0)]
                    *   val = stackobjects[(((stackposition - length(items)) + index) + 1)]
                    *
                    * this also means that lookups aren't *quite* linear anymore.
                    * in terms of performance, the difference is negligible.
                    */
                    kvstart = (idx - itmcount);
                    for(i = 0; i < itmcount; i += 2)
                    {
                        tmpi = kvstart+i;
                        key = *(ApeObject_t*)ape_valdict_getbykey(vm->stackobjects, &tmpi);
                        /*
                        * NB. this only goes for literals.
                        * maps can have anything as a key, i.e.:
                        *   foo = {}
                        *   foo[function(){}] = "bar"
                        * would be perfectly valid.
                        */
                        if(!ape_object_value_ishashable(key))
                        {
                            keytype = ape_object_value_type(key);
                            keytypename = ape_object_value_typename(keytype);
                            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "key of type %s is not hashable", keytypename);
                            goto err;
                        }
                        tmpi = kvstart+i+1;
                        objval = *(ApeObject_t*)ape_valdict_getbykey(vm->stackobjects, &tmpi);
                        ok = ape_object_map_setvalue(map_obj, key, objval);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                    ape_vm_setstackpointer(vm, vm->stackptr - itmcount);
                    ape_vm_pushstack(vm, map_obj);
                }
                break;
            case APE_OPCODE_GETTHIS:
                {
                    #if 0
                        objval = ape_vm_thispop(vm);
                    #else
                        objval = ape_vm_thisget(vm, 0);                        
                    #endif
                    ape_vm_pushstack(vm, objval);
                }
                break;
            case APE_OPCODE_GETINDEX:
                {
                    index = ape_vm_popstack(vm);
                    left = ape_vm_popstack(vm);
                    lefttype = ape_object_value_type(left);
                    indextype = ape_object_value_type(index);
                    ok = ape_vmdo_getindex(vm, left, index, lefttype, indextype);
                    if(!ok)
                    {
                        goto err;
                    }
                }
                break;
            case APE_OPCODE_GETVALUEAT:
                {
                    index = ape_vm_popstack(vm);
                    left = ape_vm_popstack(vm);
                    lefttype = ape_object_value_type(left);
                    indextype = ape_object_value_type(index);
                    if(lefttype != APE_OBJECT_ARRAY && lefttype != APE_OBJECT_MAP && lefttype != APE_OBJECT_STRING)
                    {
                        lefttn = ape_object_value_typename(lefttype);
                        indextn = ape_object_value_typename(indextype);
                        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "type %s is not indexable (in APE_OPCODE_GETVALUEAT)", lefttn);
                        goto err;
                    }
                    objres = ape_object_make_null(vm->context);
                    if(!ape_object_type_isnumeric(indextype))
                    {
                        lefttn = ape_object_value_typename(lefttype);
                        indextn = ape_object_value_typename(indextype);
                        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "cannot index %s with %s", lefttn, indextn);
                        goto err;
                    }
                    ix = (int)ape_object_value_asnumber(index);
                    if(lefttype == APE_OBJECT_ARRAY)
                    {
                        objres = ape_object_array_getvalue(left, ix);
                    }
                    else if(lefttype == APE_OBJECT_MAP)
                    {
                        objres = ape_object_getkvpairat(vm->context, left, ix);
                    }
                    else if(lefttype == APE_OBJECT_STRING)
                    {
                        str = ape_object_string_getdata(left);
                        leftlen = ape_object_string_getlength(left);
                        ix = (int)ape_object_value_asnumber(index);
                        if(ix >= 0 && ix < leftlen)
                        {
                            char res_str[2] = { str[ix], '\0' };
                            objres = ape_object_make_string(vm->context, res_str);
                        }
                    }
                    ape_vm_pushstack(vm, objres);
                }
                break;
            case APE_OPCODE_CALL:
                {
                    nargs = ape_frame_readuint8(vm->currentframe);
                    callee = ape_vm_getstack(vm, nargs);
                    ok = ape_vm_callobjectstack(vm, callee, nargs);
                    if(!ok)
                    {
                        goto err;
                    }
                }
                break;
            case APE_OPCODE_RETURNVALUE:
                {
                    objres = ape_vm_popstack(vm);
                    ok = ape_vm_framepop(vm);
                    if(!ok)
                    {
                        goto end;
                    }
                    ape_vm_pushstack(vm, objres);
                }
                break;
            case APE_OPCODE_RETURNNOTHING:
                {
                    ok = ape_vm_framepop(vm);
                    ape_vm_pushstack(vm, ape_object_make_null(vm->context));
                    if(!ok)
                    {
                        ape_vm_popstack(vm);
                        goto end;
                    }
                }
                break;
            case APE_OPCODE_DEFLOCAL:
                {
                    pos = ape_frame_readuint8(vm->currentframe);
                    popped = ape_vm_popstack(vm);
                    idx = vm->currentframe->basepointer + pos; 
                    ape_valdict_set(vm->stackobjects, &idx, &popped);
                }
                break;
            case APE_OPCODE_SETLOCAL:
                {
                    pos = ape_frame_readuint8(vm->currentframe);
                    newvalue = ape_vm_popstack(vm);
                    idx = vm->currentframe->basepointer + pos;
                    oldvalue = *(ApeObject_t*)ape_valdict_getbykey(vm->stackobjects, &idx);
                    if(!ape_vm_checkassign(vm, oldvalue, newvalue))
                    {
                        goto err;
                    }
                    idx = vm->currentframe->basepointer + pos;
                    ape_valdict_set(vm->stackobjects, &idx, &newvalue);
                }
                break;
            case APE_OPCODE_GETLOCAL:
                {
                    pos = ape_frame_readuint8(vm->currentframe);
                    idx = vm->currentframe->basepointer + pos;
                    objval = *(ApeObject_t*)ape_valdict_getbykey(vm->stackobjects, &idx);
                    ape_vm_pushstack(vm, objval);
                }
                break;
            case APE_OPCODE_GETCONTEXTGLOBAL:
                {
                    ix = ape_frame_readuint16(vm->currentframe);
                    ok = false;
                    objval = ape_globalstore_getat(vm->globalstore, ix, &ok);
                    if(!ok)
                    {
                        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "global value %d not found", ix);
                        goto err;
                    }
                    ape_vm_pushstack(vm, objval);
                }
                break;
            case APE_OPCODE_MKFUNCTION:
                {
                    ixconst = ape_frame_readuint16(vm->currentframe);
                    numfree = ape_frame_readuint8(vm->currentframe);
                    vconst = (ApeObject_t*)ape_valarray_get(constants, ixconst);
                    if(!vconst)
                    {
                        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "constant %d not found", ixconst);
                        goto err;
                    }
                    constype = ape_object_value_type(*vconst);
                    if(constype != APE_OBJECT_SCRIPTFUNCTION)
                    {
                        ctypn = ape_object_value_typename(constype);
                        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "%s is not a function", ctypn);
                        goto err;
                    }
                    constfn = ape_object_value_asscriptfunction(*vconst);
                    funcobj = ape_object_make_function(vm->context,
                        ape_object_function_getname(*vconst),
                        constfn->compiledcode,
                        false,
                        constfn->numlocals,
                        constfn->numargs, numfree);
                    if(ape_object_value_isnull(funcobj))
                    {
                        goto err;
                    }
                    for(i = 0; i < numfree; i++)
                    {
                        idx = vm->stackptr - numfree + i;
                        freeval = *(ApeObject_t*)ape_valdict_getbykey(vm->stackobjects, &idx);
                        ape_object_function_setfreeval(funcobj, i, freeval);
                    }
                    ape_vm_setstackpointer(vm, vm->stackptr - numfree);
                    ape_vm_pushstack(vm, funcobj);
                }
                break;
            case APE_OPCODE_GETFREE:
                {
                    ixfree = ape_frame_readuint8(vm->currentframe);
                    objval = ape_object_function_getfreeval(vm->currentframe->function, ixfree);
                    ape_vm_pushstack(vm, objval);
                }
                break;
            case APE_OPCODE_SETFREE:
                {
                    ixfree = ape_frame_readuint8(vm->currentframe);
                    objval = ape_vm_popstack(vm);
                    ape_object_function_setfreeval(vm->currentframe->function, ixfree, objval);
                }
                break;
            case APE_OPCODE_CURRENTFUNCTION:
                {
                    currfunction = vm->currentframe->function;
                    ape_vm_pushstack(vm, currfunction);
                }
                break;
            case APE_OPCODE_SETINDEX:
                {
                    index = ape_vm_popstack(vm);
                    left = ape_vm_popstack(vm);
                    newvalue = ape_vm_popstack(vm);
                    lefttype = ape_object_value_type(left);
                    indextype = ape_object_value_type(index);
                    if(lefttype != APE_OBJECT_ARRAY && lefttype != APE_OBJECT_MAP)
                    {
                        lefttn = ape_object_value_typename(lefttype);
                        indextn = ape_object_value_typename(indextype);
                        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "type %s is not indexable (in APE_OPCODE_SETINDEX)", lefttn);
                        goto err;
                    }
                    if(lefttype == APE_OBJECT_ARRAY)
                    {
                        if(!ape_object_type_isnumeric(indextype))
                        {
                            lefttn = ape_object_value_typename(lefttype);
                            indextn = ape_object_value_typename(indextype);
                            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "cannot index %s with %s", lefttn, indextn);
                            goto err;
                        }
                        ix = (int)ape_object_value_asnumber(index);
                        ok = ape_object_array_setat(left, ix, newvalue);
                        if(!ok)
                        {
                            lefttn = ape_object_value_typename(lefttype);
                            indextn = ape_object_value_typename(indextype);
                            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "setting array item failed (index %d out of bounds of %d)", ix, ape_object_array_getlength(left));
                            goto err;
                        }
                    }
                    else if(lefttype == APE_OBJECT_MAP)
                    {
                        oldvalue = ape_object_map_getvalueobject(left, index);
                        if(!ape_vm_checkassign(vm, oldvalue, newvalue))
                        {
                            goto err;
                        }
                        ok = ape_object_map_setvalue(left, index, newvalue);
                        if(!ok)
                        {
                            goto err;
                        }
                    }
                }
                break;
            case APE_OPCODE_DUP:
                {
                    objval = ape_vm_getstack(vm, 0);
                    ape_vm_pushstack(vm, ape_object_value_copyflat(vm->context, objval));
                }
                break;
            case APE_OPCODE_LEN:
                {
                    objval = ape_vm_popstack(vm);
                    len = 0;
                    type = ape_object_value_type(objval);
                    if(type == APE_OBJECT_ARRAY)
                    {
                        len = ape_object_array_getlength(objval);
                    }
                    else if(type == APE_OBJECT_MAP)
                    {
                        len = ape_object_map_getlength(objval);
                    }
                    else if(type == APE_OBJECT_STRING)
                    {
                        len = ape_object_string_getlength(objval);
                    }
                    else
                    {
                        ctypn = ape_object_value_typename(type);
                        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "cannot get length of %s", ctypn);
                        goto err;
                    }
                    ape_vm_pushstack(vm, ape_object_make_fixednumber(vm->context, len));
                }
                break;
            case APE_OPCODE_MKNUMBER:
                {
                    /* FIXME: why does ape_util_uinttofloat break things here? */
                    uint64_t val = ape_frame_readuint64(vm->currentframe);
                    #if 1
                    valdouble = ape_util_uinttofloat(val);
                    #else
                    valdouble = val;
                    #endif
                    //fprintf(stderr, "valdouble=%g\n", valdouble);
                    if(((ApeInt_t)valdouble) == valdouble)
                    {
                        objval = ape_object_make_fixednumber(vm->context, valdouble);
                    }
                    else
                    {
                        objval = ape_object_make_floatnumber(vm->context, valdouble);
                    }
                    //objval.handle->datatype = APE_OBJECT_NUMBER;
                    ape_vm_pushstack(vm, objval);
                }
                break;
            case APE_OPCODE_SETRECOVER:
                {
                    recip = ape_frame_readuint16(vm->currentframe);
                    vm->currentframe->recoverip = recip;
                }
                break;
            case APE_OPCODE_IMPORT:
                {
                    
                }
                break;
            default:
                {
                    APE_ASSERT(false);
                    ape_vm_adderror(vm, APE_ERROR_RUNTIME, "unknown opcode: 0x%x", opcode);
                    goto err;
                }
                break;
        }
    err:
        if(ape_errorlist_count(vm->errors) > 0)
        {
            err = ape_errorlist_lasterror(vm->errors);
            if(err->errtype == APE_ERROR_RUNTIME && ape_errorlist_count(vm->errors) == 1)
            {
                ixrecover = -1;
                for(ui = vm->countframes - 1; ui >= 0; ui--)
                {
                    frame = (ApeFrame_t*)deqlist_get(vm->frameobjects, ui);
                    if(frame->recoverip >= 0 && !frame->isrecovering)
                    {
                        ixrecover = ui;
                        break;
                    }
                }
                if(ixrecover < 0)
                {
                    goto end;
                }
                else
                {
                    if(!err->traceback)
                    {
                        err->traceback = ape_make_traceback(vm->context);
                    }
                    if(err->traceback)
                    {
                        ape_traceback_appendfromvm(err->traceback, vm);
                    }
                    while((ApeInt_t)vm->countframes > (ApeInt_t)(ixrecover + 1))
                    {
                        ape_vm_framepop(vm);
                    }
                    errobj = ape_object_make_error(vm->context, err->message);
                    if(!ape_object_value_isnull(errobj))
                    {
                        ape_object_value_seterrortraceback(errobj, err->traceback);
                        err->traceback = NULL;
                    }
                    ape_vm_pushstack(vm, errobj);
                    vm->currentframe->ip = vm->currentframe->recoverip;
                    vm->currentframe->isrecovering = true;
                    ape_errorlist_clear(vm->errors);
                }
            }
            else
            {
                goto end;
            }
        }
        if(ape_gcmem_shouldsweep(vm->mem))
        {
            ape_vm_collectgarbage(vm, constants, true);
        }
    }
end:
    if(ape_errorlist_count(vm->errors) > 0)
    {
        err = ape_errorlist_lasterror(vm->errors);
        if(!err->traceback)
        {
            err->traceback = ape_make_traceback(vm->context);
        }
        if(err->traceback)
        {
            ape_traceback_appendfromvm(err->traceback, vm);
        }
    }
    ape_vm_collectgarbage(vm, constants, true);
    vm->running = false;
    return ape_errorlist_count(vm->errors) == 0;
}
