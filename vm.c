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

static const ApePosition g_vmpriv_srcposinvalid = { NULL, -1, -1 };


void ape_vm_setstackpointer(ApeVM *vm, int new_sp);
bool ape_vm_tryoverloadoperator(ApeVM *vm, ApeObject left, ApeObject right, ApeOpByte op, bool *out_overload_found);

/*
* todo: these MUST reflect the order of ApeOpcodeValue.
* meaning its prone to break terribly if and/or when it is changed.
*/
static ApeOpcodeDef g_definitions[APE_OPCODE_MAX + 1] =
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
    { "op(~)", 0, {0}},
    { "op(|)", 0, { 0 } },
    { "op(^)", 0, { 0 } },
    { "op(&)", 0, { 0 } },
    { "op(<<)", 0, { 0 } },
    { "op(>>)", 0, { 0 } },
    { "import", 1, {1} },
    { "invalid_max", 0, { 0 } },
};

ApeOpcodeDef* ape_vm_opcodefind(ApeOpByte op)
{
    if(op <= APE_OPCODE_NONE || op >= APE_OPCODE_MAX)
    {
        return NULL;
    }
    return &g_definitions[op];
}

const char* ape_vm_opcodename(ApeOpByte op)
{
    if(op <= APE_OPCODE_NONE || op >= APE_OPCODE_MAX)
    {
        return NULL;
    }
    return g_definitions[op].name;
}

void ape_vm_adderrorv(ApeVM* vm, ApeErrorType etype, const char* fmt, va_list va)
{
    ape_errorlist_addformatv(vm->errors, etype, ape_frame_srcposition(vm->currentframe), fmt, va);
}

void ape_vm_adderror(ApeVM* vm, ApeErrorType etype, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    ape_vm_adderrorv(vm, etype, fmt, va);
    va_end(va);
}


ApeGlobalStore* ape_make_globalstore(ApeContext* ctx, ApeGCMemory* mem)
{
    ApeSize i;
    ApeSize bcount;
    bool ok;
    const char* name;
    ApeObject builtin;
    ApeGlobalStore* store;
    store = (ApeGlobalStore*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeGlobalStore));
    if(!store)
    {
        return NULL;
    }
    memset(store, 0, sizeof(ApeGlobalStore));
    store->context = ctx;
    store->named = ape_make_strdict(ctx, (ApeDataCallback)ape_symbol_copy, (ApeDataCallback)ape_symbol_destroy);
    
    if(!store->named)
    {
        goto err;
    }
    store->objects = ape_make_valarray(ctx, sizeof(ApeObject));
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

void ape_globalstore_destroy(ApeGlobalStore* store)
{
    ApeContext* ctx;
    if(!store)
    {
        return;
    }
    ctx = store->context;
    ape_strdict_destroywithitems(ctx, store->named);
    ape_valarray_destroy(store->objects);
    ape_allocator_free(&ctx->alloc, store);
}

ApeSymbol* ape_globalstore_getsymbol(ApeGlobalStore* store, const char* name)
{
    return (ApeSymbol*)ape_strdict_getbyname(store->named, name);
}

bool ape_globalstore_set(ApeGlobalStore* store, const char* name, ApeObject object)
{
    ApeInt ix;
    bool ok;
    ApeSymbol* symbol;
    const ApeSymbol* existing_symbol;
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

ApeObject ape_globalstore_getat(ApeGlobalStore* store, int ix, bool* out_ok)
{
    ApeObject* res = (ApeObject*)ape_valarray_get(store->objects, ix);
    if(!res)
    {
        *out_ok = false;
        return ape_object_make_null(store->context);
    }
    *out_ok = true;
    return *res;
}

ApeObject* ape_globalstore_getobjectdata(ApeGlobalStore* store)
{
    return (ApeObject*)ape_valarray_data(store->objects);
}

ApeSize ape_globalstore_getobjectcount(ApeGlobalStore* store)
{
    return ape_valarray_count(store->objects);
}


#define APPEND_BYTE(n)                           \
    do                                           \
    {                                            \
        val = (ApeUShort)(operands[i] >> (n * 8)); \
        ok = ape_valarray_push(res, &val);               \
        if(!ok)                                  \
        {                                        \
            return 0;                            \
        }                                        \
    } while(0)

int ape_make_code(ApeOpByte op, ApeSize operands_count, ApeOpByte* operands, ApeValArray* res)
{
    ApeSize i;
    int width;
    int instr_len;
    bool ok;
    ApeUShort val;
    ApeOpcodeDef* def;
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


ApeOpcodeValue ape_frame_readopcode(ApeFrame* frame)
{
    frame->srcip = frame->ip;
    return (ApeOpcodeValue)ape_frame_readuint8(frame);
}

ApeOpByte ape_frame_readuint64(ApeFrame* frame)
{
    ApeOpByte res;
    const ApeUShort* data;
    data = frame->bytecode + frame->ip;
    frame->ip += 8;
    res = 0;
    res |= (ApeOpByte)data[7];
    res |= (ApeOpByte)data[6] << 8;
    res |= (ApeOpByte)data[5] << 16;
    res |= (ApeOpByte)data[4] << 24;
    res |= (ApeOpByte)data[3] << 32;
    res |= (ApeOpByte)data[2] << 40;
    res |= (ApeOpByte)data[1] << 48;
    res |= (ApeOpByte)data[0] << 56;
    return res;
}

uint16_t ape_frame_readuint16(ApeFrame* frame)
{
    const ApeUShort* data;
    data = frame->bytecode + frame->ip;
    frame->ip += 2;
    return (data[0] << 8) | data[1];
}

ApeUShort ape_frame_readuint8(ApeFrame* frame)
{
    const ApeUShort* data;
    data = frame->bytecode + frame->ip;
    frame->ip++;
    return data[0];
}

ApePosition ape_frame_srcposition(const ApeFrame* frame)
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

ApeObject ape_vm_getlastpopped(ApeVM* vm)
{
    return vm->lastpopped;
}

bool ape_vm_haserrors(ApeVM* vm)
{
    return ape_errorlist_count(vm->errors) > 0;
}

bool ape_vm_setglobal(ApeVM* vm, ApeSize ix, ApeObject val)
{
    ape_valdict_set(vm->globalobjects, &ix, &val);
    return true;
}

ApeObject ape_vm_getglobal(ApeVM* vm, ApeSize ix)
{
    ApeObject* rt;
    rt = (ApeObject*)ape_valdict_getbykey(vm->globalobjects, &ix);
    if(rt == NULL)
    {
        return ape_object_make_null(vm->context);
    }
    return *rt;
}

void ape_vm_setstackpointer(ApeVM* vm, int new_sp)
{
    int count;
    size_t bytescount;
    ApeSize i;
    ApeSize idx;
    idx = vm->stackptr;
    /* to avoid gcing freed objects */
    if((ApeInt)new_sp > (ApeInt)idx)
    {
        count = new_sp - idx;
        bytescount = count * sizeof(ApeObject);
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
            ApeObject nullval = ape_object_make_null(vm->context);
            ape_valdict_set(vm->stackobjects, &i, &nullval);
        }
    }
    vm->stackptr = new_sp;
}

void ape_vm_pushstack(ApeVM* vm, ApeObject obj)
{
    ApeSize idx;
#if defined(APE_DEBUG) && (APE_DEBUG == 1)
    if(vm->currentframe)
    {
        ApeFrame* frame = vm->currentframe;
        ApeScriptFunction* currfunction = ape_object_value_asscriptfunction(frame->function);
        int nl = currfunction->numlocals;
        APE_ASSERT(vm->stackptr >= (frame->basepointer + nl));
    }
#endif
    idx = vm->stackptr;
    ape_valdict_set(vm->stackobjects, &idx, &obj);
    vm->stackptr++;
}

ApeObject ape_vm_popstack(ApeVM* vm)
{
    ApeSize idx;
    ApeObject* ptr;
    ApeObject objres;
#if defined(APE_DEBUG) && (APE_DEBUG == 1)
    if(vm->stackptr == 0)
    {
        ape_errorlist_add(vm->errors, APE_ERROR_RUNTIME, ape_frame_srcposition(vm->currentframe), "stack underflow");
        APE_ASSERT(false);
        return ape_object_make_null(vm->context);
    }
    if(vm->currentframe)
    {
        ApeFrame* frame = vm->currentframe;
        ApeScriptFunction* currfunction = ape_object_value_asscriptfunction(frame->function);
        int nl = currfunction->numlocals;
        APE_ASSERT((vm->stackptr - 1) >= (frame->basepointer + nl));
    }
#endif
    vm->stackptr--;
    idx = vm->stackptr-0;
    ptr = (ApeObject*)ape_valdict_getbykey(vm->stackobjects, &idx);
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

ApeObject ape_vm_getstack(ApeVM* vm, int nth_item)
{
    ApeSize ix;
    ApeObject* p;
    ix = vm->stackptr - 1 - nth_item;
    p = (ApeObject*)ape_valdict_getbykey(vm->stackobjects, &ix);
    if(p == NULL)
    {
        return ape_object_make_null(vm->context);
    }

    return *p;
}

void ape_vm_thispush(ApeVM* vm, ApeObject obj)
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

ApeObject ape_vm_thisget(ApeVM* vm, int nth_item)
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

ApeObject ape_vm_thispop(ApeVM* vm)
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

ApeObject ape_vm_thisparent(ApeVM* vm, bool letfail)
{
    ApeSize ofs;
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

void ape_vm_dumpstack(ApeVM* vm)
{
    ApeInt i;
    ApeSize key;
    ApeWriter* wr;
    ApeSize* keys;
    ApeObject* vals;
    keys = ((ApeSize*)(vm->stackobjects->keys));
    vals = ((ApeObject*)(vm->stackobjects->values));
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

ApeObject ape_vm_callnativefunction(ApeVM* vm, ApeObject callee, ApePosition src_pos, int argc, ApeObject* args)
{
    ApeError* err;
    ApeObject objres;
    ApeObjType restype;
    ApeNativeFunction* nfunc;
    ApeTraceback* traceback;
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

bool ape_vm_callobjectargs(ApeVM* vm, ApeObject callee, ApeInt nargs, ApeObject* args)
{
    bool ok;
    ApeSize idx;
    ApeInt ofs;
    ApeInt actualargs;
    ApeObjType calleetype;
    ApeScriptFunction* scriptcallee;
    ApeFrame framecallee;
    ApeObject* fwdargs;
    ApeObject* stackpos;
    ApeObject* stackvals;
    ApeObject objres;
    const char* calleetypename;
    (void)scriptcallee;
    calleetype = ape_object_value_type(callee);
    if(calleetype == APE_OBJECT_SCRIPTFUNCTION)
    {
        scriptcallee = ape_object_value_asscriptfunction(callee);
        #if 0
        if(nargs != -1)
        {
            if(nargs != (ApeInt)scriptcallee->numargs)
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
        stackvals = ((ApeObject*)(vm->stackobjects->values));
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

bool ape_vm_callobjectstack(ApeVM* vm, ApeObject callee, ApeInt nargs)
{
    return ape_vm_callobjectargs(vm, callee, nargs, NULL);
}

bool ape_vm_checkassign(ApeVM* vm, ApeObject oldval, ApeObject newval)
{
    ApeObjType oldtype;
    ApeObjType newtype;
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

bool ape_vm_tryoverloadoperator(ApeVM* vm, ApeObject left, ApeObject right, ApeOpByte op, bool* out_overload_found)
{
    int numop;
    ApeObject key;
    ApeObject callee;
    ApeObjType lefttype;
    ApeObjType righttype;
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

ApeVM* ape_make_vm(ApeContext* ctx, const ApeConfig* config, ApeGCMemory* mem, ApeErrorList* errors, ApeGlobalStore* global_store)
{
    ApeSize i;
    ApeVM* vm;
    ApeObject key_obj;
    vm = (ApeVM*)ape_allocator_alloc(&ctx->alloc, sizeof(ApeVM));
    if(!vm)
    {
        return NULL;
    }
    memset(vm, 0, sizeof(ApeVM));
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
    vm->globalobjects = ape_make_valdict(ctx, sizeof(ApeSize), sizeof(ApeObject));
    vm->stackobjects = ape_make_valdict(ctx, sizeof(ApeSize), sizeof(ApeObject));
    vm->lastframe = NULL;
    vm->frameobjects = da_make(ctx, vm->frameobjects, 0, sizeof(ApeFrame));
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

void ape_vm_destroy(ApeVM* vm)
{
    ApeContext* ctx;
    ApeFrame* popped;
    (void)popped;
    if(!vm)
    {
        return;
    }
    ctx = vm->context;
    ape_valdict_destroy(vm->globalobjects);
    ape_valdict_destroy(vm->stackobjects);
    fprintf(stderr, "deqlist_count(vm->frameobjects)=%d\n", da_count(vm->frameobjects));
    if(da_count(vm->frameobjects) != 0)
    {
        while(da_count(vm->frameobjects) != 0)
        {
            popped = (ApeFrame*)da_get(vm->frameobjects, da_count(vm->frameobjects)-1);
            if(popped != NULL)
            {
                //if(popped->allocated)
                {
                    ape_allocator_free(&vm->context->alloc, popped);
                }
            }
            da_pop(vm->frameobjects);
        }
    }
    da_destroy(vm->context, vm->frameobjects);
    ape_allocator_free(&ctx->alloc, vm);
}

void ape_vm_reset(ApeVM* vm)
{
    vm->stackptr = 0;
    vm->thisptr = 0;
    while(vm->countframes > 0)
    {
        ape_vm_framepop(vm);
    }
}


bool ape_vm_frameinit(ApeFrame* frame, ApeObject funcobj, int bptr)
{
    ApeScriptFunction* function;
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
ApeFrame* ape_frame_update(ApeVM* vm, ApeFrame* rt, ApeFrame* from)
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
ApeFrame* ape_frame_copyalloc(ApeVM* vm, ApeFrame* from)
{
    ApeFrame* rt;
    rt = (ApeFrame*)ape_allocator_alloc(&vm->context->alloc, sizeof(ApeFrame));
    ape_frame_update(vm, rt, from);
    return rt;
}

bool ape_vm_framepush(ApeVM* vm, ApeFrame frame)
{
    ApeFrame* pf;
    ApeFrame* tmp;
    ApeScriptFunction* fn;
    /*
    * when a new frame exceeds existing the frame list, then
    * allocate a new one from $frame ...
    * ... otherwise, copy data from $frame, and stuff it back into the frame list.
    * this saves a considerable amount of memory by reusing existing memory.
    */
    if(vm->countframes >= da_count(vm->frameobjects))
    {
        pf = ape_frame_copyalloc(vm, &frame);
        da_push(vm->context, vm->frameobjects, pf);
    }
    else
    {
        tmp = (ApeFrame*)da_get(vm->frameobjects, vm->countframes);
        pf = ape_frame_update(vm, tmp, &frame);
        da_set(vm->frameobjects, vm->countframes, pf);
    }
    vm->lastframe = pf;
    vm->currentframe = (ApeFrame*)da_get(vm->frameobjects, vm->countframes);
    vm->countframes++;
    fn = ape_object_value_asscriptfunction(frame.function);
    ape_vm_setstackpointer(vm, frame.basepointer + fn->numlocals);
    return true;
}

bool ape_vm_framepop(ApeVM* vm)
{
    ApeFrame* popped;
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
    popped = (ApeFrame*)da_get(vm->frameobjects, vm->countframes);
    /*
    * don't deallocate $popped here - they'll be deallocated in ape_vm_destroy.
    */
    vm->currentframe = (ApeFrame*)da_get(vm->frameobjects, vm->countframes - 1);
    return true;
}

void ape_vm_collectgarbage(ApeVM* vm, ApeValArray* constants, bool alsostack)
{
    ApeSize i;
    ApeFrame* frame;
    ape_gcmem_unmarkall(vm->mem);
    ape_gcmem_markobjlist(ape_globalstore_getobjectdata(vm->globalstore), ape_globalstore_getobjectcount(vm->globalstore));
    if(constants != NULL)
    {
        ape_gcmem_markobjlist((ApeObject*)ape_valarray_data(constants), ape_valarray_count(constants));
    }
    ape_gcmem_markobjlist((ApeObject*)(vm->globalobjects->values), vm->globalobjects->count);
    for(i = 0; i < vm->countframes; i++)
    {
        frame = (ApeFrame*)da_get(vm->frameobjects, i);
        ape_gcmem_markobject(frame->function);
    }
    if(alsostack)
    {
        if(vm->stackptr > 0)
        {
            ape_gcmem_markobjlist(((ApeObject*)(vm->stackobjects->values)), vm->stackptr);
        }
        ape_gcmem_markobjlist(vm->thisobjects, vm->thisptr);
    }
    ape_gcmem_markobject(vm->lastpopped);
    ape_gcmem_markobjlist(vm->overloadkeys, APE_OPCODE_MAX);
    ape_gcmem_sweep(vm->mem);
}

bool ape_vm_run(ApeVM* vm, ApeAstCompResult* comp_res, ApeValArray * constants)
{
    bool res;
    int old_sp;
    int old_this_sp;
    ApeSize old_frames_count;
    ApeObject main_fn;
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
    res = ape_vm_execfunc(vm, main_fn, constants);
    while(vm->countframes > old_frames_count)
    {
        ape_vm_framepop(vm);
    }
    APE_ASSERT(vm->stackptr == old_sp);
    vm->thisptr = old_this_sp;
    return res;
}

ApeObject ape_object_string_copy(ApeContext* ctx, ApeObject obj)
{
    return ape_object_make_stringlen(ctx, ape_object_string_getdata(obj), ape_object_string_getlength(obj));
}

bool ape_vm_appendstring(ApeVM* vm, ApeObject left, ApeObject right, ApeObjType lefttype, ApeObjType righttype)
{
    bool ok;
    const char* leftstr;
    const char* rightstr;
    ApeObject objres;
    ApeInt buflen;
    ApeInt leftlen;
    ApeInt rightlen;
    ApeWriter* allbuf;
    ApeWriter* tostrbuf;
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
        #if 1
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
        #else
            if(ape_object_value_isstring())
        #endif
        ape_vm_pushstack(vm, objres);
    }
    return true;
}


bool ape_vm_getindex(ApeVM* vm, ApeObject left, ApeObject index, ApeObjType lefttype, ApeObjType indextype)
{
    bool canindex;
    unsigned long nhash;
    const char* str;
    const char* idxname;
    const char* indextn;
    const char* lefttn;
    ApeSize idxlen;
    ApeInt ix;
    ApeInt leftlen;
    ApeObject objres;
    (void)idxname;
    canindex = false;
    /*
    * todo: object method lookup could be implemented here
    */
    {
        ApeObject objfn;
        ApeObject objval;
        ApeObjMemberItem* afn;
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
        if(ix >= 0 && ix < (ApeInt)ape_object_array_getlength(left))
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

bool ape_vm_math(ApeVM* vm, ApeObject left, ApeObject right, ApeOpcodeValue opcode)
{
    bool ok;
    bool isfixed;
    bool needtwo;
    bool overloadfound;
    const char* lefttn;
    const char* righttn;
    const char* opcname;
    ApeFloat resfloat;
    ApeInt resfixed;
    ApeInt leftint;
    ApeInt rightint;
    ApeObjType lefttype;
    ApeObjType righttype;
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
    needtwo = true;
    lefttype = ape_object_value_type(left);
    righttype = ape_object_value_type(right);
    if(ape_object_value_isnumeric(left) && ape_object_value_isnumeric(right))
    {
        resfloat = 0;
        resfixed = 0;
        //fprintf(stderr, "ape_vm_math: right.type=%s left.type=%s\n", ape_object_value_typename(right.type), ape_object_value_typename(left.type));
        switch(opcode)
        {
            case APE_OPCODE_ADD:
                {
                    ApeFloat rightval = ape_object_value_asnumber(right);
                    ApeFloat leftval = ape_object_value_asnumber(left);        
                    resfloat = leftval + rightval;
                }
                break;
            case APE_OPCODE_SUB:
                {
                    ApeFloat rightval = ape_object_value_asnumber(right);
                    ApeFloat leftval = ape_object_value_asnumber(left);
                    resfloat = leftval - rightval;
                }
                break;
            case APE_OPCODE_MUL:
                {
                    ApeFloat rightval = ape_object_value_asnumber(right);
                    ApeFloat leftval = ape_object_value_asnumber(left);
                    resfloat = leftval * rightval;
                }
                break;
            case APE_OPCODE_DIV:
                {
                    ApeFloat rightval = ape_object_value_asnumber(right);
                    ApeFloat leftval = ape_object_value_asnumber(left);
                    resfloat = leftval / rightval;
                }
                break;
            case APE_OPCODE_MOD:
                {
                    if(ape_object_value_isfloatnumber(left))
                    {
                        ApeFloat rightval = ape_object_value_asnumber(right);
                        ApeFloat leftval = ape_object_value_asnumber(left);
                        leftint = ape_util_numbertoint32(leftval);
                        rightint = ape_util_numbertoint32(rightval);                    

                        resfloat = fmod(leftval, rightval);
                        isfixed = false;
                    }
                    else
                    {
                        ApeInt rightval = ape_object_value_asfixednumber(right);
                        ApeInt leftval = ape_object_value_asfixednumber(left);
                        resfixed = (leftval % rightval);
                        isfixed = true;
                    }
                }
                break;
            case APE_OPCODE_BITOR:
                {
                    ApeInt rightval = ape_object_value_asfixednumber(right);
                    ApeInt leftval = ape_object_value_asfixednumber(left);
                    leftint = (leftval);
                    rightint = (rightval);
                    //fprintf(stderr, "BITOR: rightval=%ld leftval=%ld leftint=%ld rightint=%ld\n", rightval, leftval, leftint, rightint);
                    resfixed = (leftint | rightint);
                    isfixed = true;
                }
                break;
            case APE_OPCODE_BITXOR:
                {
                    ApeInt rightval = ape_object_value_asnumber(right);
                    ApeInt leftval = ape_object_value_asnumber(left);
                    leftint = leftval;
                    rightint = rightval;
                    resfixed = (leftint ^ rightint);
                    isfixed = true;
                }
                break;
            case APE_OPCODE_BITAND:
                {
                    ApeInt rightval = ape_object_value_asnumber(right);
                    ApeInt leftval = ape_object_value_asnumber(left);
                    leftint = (leftval);
                    rightint = (rightval);
                    resfixed = (ApeInt)(leftint & rightint);
                    isfixed = true;
                }
                break;
            case APE_OPCODE_LEFTSHIFT:
                {
                    ApeFloat rightval = ape_object_value_asnumber(right);
                    ApeFloat leftval = ape_object_value_asnumber(left);
                    int uleft = ape_util_numbertoint32(leftval);
                    unsigned int uright = ape_util_numbertouint32(rightval);
                    resfixed = (uleft << (uright & 0x1F));
                    isfixed = true;
                }
                break;
            case APE_OPCODE_RIGHTSHIFT:
                {
                    ApeFloat rightval = ape_object_value_asnumber(right);
                    ApeFloat leftval = ape_object_value_asnumber(left);
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
        ok = ape_vm_appendstring(vm, left, right, lefttype, righttype);
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


bool ape_vmdo_mkconstant(ApeVM* vm)
{
    ApeUInt ixconst;
    ApeObject* vconst;
    ixconst = ape_frame_readuint16(vm->currentframe);
    vconst = (ApeObject*)ape_valarray_get(vm->estate.constants, ixconst);
    if(!vconst)
    {
        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "constant at %d not found", ixconst);
        return false;
    }
    ape_vm_pushstack(vm, *vconst);
    return true;
}

bool ape_vmdo_binary(ApeVM* vm)
{
    bool ok;
    ApeObject left;
    ApeObject right;
    right = ape_vm_popstack(vm);
    left = ape_vm_popstack(vm);
    ok = ape_vm_math(vm, left, right, vm->estate.opcode);
    if(!ok)
    {
        return false;
    }
    return true;
}

bool ape_vmdo_compareequal(ApeVM* vm)
{
    bool ok;
    bool isoverloaded;
    ApeFloat cres;
    const char* lefttypestr;
    const char* righttypestr;
    ApeObject left;
    ApeObject right;
    ApeObject objres;
    right = ape_vm_popstack(vm);
    left = ape_vm_popstack(vm);
    isoverloaded = false;
    ok = ape_vm_tryoverloadoperator(vm, left, right, APE_OPCODE_COMPAREPLAIN, &isoverloaded);
    if(!ok)
    {
        return false;
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
        if(ok || vm->estate.opcode == APE_OPCODE_COMPAREEQUAL)
        {
            objres = ape_object_make_floatnumber(vm->context, cres);
            ape_vm_pushstack(vm, objres);
        }
        else
        {
            righttypestr = ape_object_value_typename(ape_object_value_type(right));
            lefttypestr = ape_object_value_typename(ape_object_value_type(left));
            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "cannot compare %s and %s", lefttypestr, righttypestr);
            return false;
        }
    }
    return true;
}

bool ape_vmdo_comparelogical(ApeVM* vm)
{
    ApeFloat cres;
    ApeObject objres;
    ApeObject value;
    value = ape_vm_popstack(vm);
    cres = ape_object_value_asfixednumber(value);
    bool resval = false;
    //fprintf(stderr, "comparison: value.type=%s n=%d\n", ape_object_value_typename(ape_object_value_type(value)), ape_object_value_asfixednumber(value));
    switch(vm->estate.opcode)
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
    return true;
}

bool ape_vmdo_unary(ApeVM* vm)
{
    bool ok;
    bool isfixed;
    ApeInt vi;
    ApeFloat vf;
    ApeObjType opertype;
    const char* optypestr;
    ApeObject objres;
    ApeObject operand;
    operand = ape_vm_popstack(vm);
    opertype = ape_object_value_type(operand);
    if(ape_object_type_isnumeric(opertype))
    {
        isfixed = ape_object_value_isfixednumber(operand);
        switch(vm->estate.opcode)
        {
            case APE_OPCODE_MINUS:
                {
                    if(isfixed)
                    {
                        vi = ape_object_value_asfixednumber(operand);
                        vi = -vi;
                        objres = ape_object_make_fixednumber(vm->context, vi);
                    }
                    else
                    {
                        vf =  ape_object_value_asfloatnumber(operand);
                        vf = -vf;
                        objres = ape_object_make_floatnumber(vm->context, vf);
                    }
                }
                break;
            case APE_OPCODE_BITNOT:
                {
                    if(isfixed)
                    {
                        vi = ape_object_value_asfixednumber(operand);
                    }
                    else
                    {
                        vi = ape_object_value_asfloatnumber(operand);
                    }
                    objres = ape_object_make_fixednumber(vm->context, ~vi);
                }
                break;
            case APE_OPCODE_NOT:
                {
                    if(isfixed)
                    {
                        vi = ape_object_value_asfixednumber(operand);
                        vi = !vi;
                        objres = ape_object_make_fixednumber(vm->context, vi);
                    }
                    else
                    {
                        vf =  ape_object_value_asfloatnumber(operand);
                        vf = !vf;
                        objres = ape_object_make_floatnumber(vm->context, vf);
                    }
                }
                break;
            default:
                break;
        }
        ape_vm_pushstack(vm, objres);
    }
    else
    {
        bool overloadfound = false;
        ok = ape_vm_tryoverloadoperator(vm, operand, ape_object_make_null(vm->context), vm->estate.opcode, &overloadfound);
        if(!ok)
        {
            return false;
        }
        if(!overloadfound)
        {
            optypestr = ape_object_value_typename(opertype);
            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "invalid operand type, got %s", optypestr);
            return false;
        }
    }
    return true;
}

bool ape_vmdo_jumpiffalse(ApeVM* vm)
{
    ApeInt pos;
    ApeObject testobj;
    pos = ape_frame_readuint16(vm->currentframe);
    testobj = ape_vm_popstack(vm);
    if(!ape_object_value_asbool(testobj))
    {
        vm->currentframe->ip = pos;
    }
    return true;
}

bool ape_vmdo_jumpiftrue(ApeVM* vm)
{
    ApeInt pos;
    ApeObject testobj;
    pos = ape_frame_readuint16(vm->currentframe);
    testobj = ape_vm_popstack(vm);
    if(ape_object_value_asbool(testobj))
    {
        vm->currentframe->ip = pos;
    }
    return true;
}

bool ape_vmdo_jumpunconditional(ApeVM* vm)
{
    ApeInt pos;
    pos = ape_frame_readuint16(vm->currentframe);
    vm->currentframe->ip = pos;
    return true;
}

bool ape_vmdo_setrecover(ApeVM* vm)
{
    ApeUInt recip;
    recip = ape_frame_readuint16(vm->currentframe);
    vm->currentframe->recoverip = recip;
    return true;
}

bool ape_vmdo_mknumber(ApeVM* vm)
{
    ApeFloat valdouble;
    ApeObject objval;
    /* FIXME: why does ape_util_uinttofloat break things here? */
    uint64_t val = ape_frame_readuint64(vm->currentframe);
    #if 1
    valdouble = ape_util_uinttofloat(val);
    #else
    valdouble = val;
    #endif
    //fprintf(stderr, "valdouble=%g\n", valdouble);
    if(((ApeInt)valdouble) == valdouble)
    {
        objval = ape_object_make_fixednumber(vm->context, valdouble);
    }
    else
    {
        objval = ape_object_make_floatnumber(vm->context, valdouble);
    }
    //objval.handle->datatype = APE_OBJECT_NUMBER;
    ape_vm_pushstack(vm, objval);
    return true;
}

bool ape_vmdo_mkarray(ApeVM* vm)
{
    bool ok;
    ApeSize i;
    ApeSize idx;
    ApeUInt count;
    ApeObject arrayobj;
    ApeObject* items;
    count = ape_frame_readuint16(vm->currentframe);
    arrayobj = ape_object_make_arraycapacity(vm->context, count);
    if(ape_object_value_isnull(arrayobj))
    {
        return false;
    }
    idx = vm->stackptr;
    items = ((ApeObject*)(vm->stackobjects->values)) + idx - count;
    for(i = 0; i < count; i++)
    {
        ApeObject item = items[i];
        ok = ape_object_array_pushvalue(arrayobj, item);
        if(!ok)
        {
            return false;
        }
    }
    ape_vm_setstackpointer(vm, vm->stackptr - count);
    ape_vm_pushstack(vm, arrayobj);
    return true;
}

bool ape_vmdo_mkfunction(ApeVM* vm)
{
    const char* ctypn;
    ApeSize i;
    ApeSize idx;
    ApeUInt ixconst;
    ApeUShort numfree;
    ApeObjType constype;
    ApeObject freeval;
    ApeObject funcobj;
    ApeObject* vconst;
    ixconst = ape_frame_readuint16(vm->currentframe);
    numfree = ape_frame_readuint8(vm->currentframe);
    vconst = (ApeObject*)ape_valarray_get(vm->estate.constants, ixconst);
    if(!vconst)
    {
        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "constant %d not found", ixconst);
        return false;
    }
    constype = ape_object_value_type(*vconst);
    if(constype != APE_OBJECT_SCRIPTFUNCTION)
    {
        ctypn = ape_object_value_typename(constype);
        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "%s is not a function", ctypn);
        return false;
    }
    const ApeScriptFunction* constfn = ape_object_value_asscriptfunction(*vconst);
    funcobj = ape_object_make_function(vm->context,
        ape_object_function_getname(*vconst),
        constfn->compiledcode,
        false,
        constfn->numlocals,
        constfn->numargs, numfree);
    if(ape_object_value_isnull(funcobj))
    {
        return false;
    }
    for(i = 0; i < numfree; i++)
    {
        idx = vm->stackptr - numfree + i;
        freeval = *(ApeObject*)ape_valdict_getbykey(vm->stackobjects, &idx);
        ape_object_function_setfreeval(funcobj, i, freeval);
    }
    ape_vm_setstackpointer(vm, vm->stackptr - numfree);
    ape_vm_pushstack(vm, funcobj);
    return true;
}

bool ape_vmdo_mapstart(ApeVM* vm)
{
    ApeUInt count;
    ApeObject map_obj;
    count = ape_frame_readuint16(vm->currentframe);
    map_obj = ape_object_make_mapcapacity(vm->context, count);
    if(ape_object_value_isnull(map_obj))
    {
        return false;
    }
    ape_vm_thispush(vm, map_obj);
    return true;
}

bool ape_vmdo_mapend(ApeVM* vm)
{
    bool ok;
    ApeSize i;
    ApeSize idx;
    ApeSize tmpi;
    ApeUInt itmcount;
    ApeUInt kvpcount;
    ApeSize kvstart;
    ApeObjType keytype;
    const char* keytypename;
    ApeObject key;
    ApeObject map_obj;
    ApeObject objval;
    ApeObject* stackvals;
    kvpcount = ape_frame_readuint16(vm->currentframe);
    itmcount = kvpcount * 2;
    map_obj = ape_vm_thispop(vm);
    stackvals = ((ApeObject*)(vm->stackobjects->values));
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
        key = *(ApeObject*)ape_valdict_getbykey(vm->stackobjects, &tmpi);
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
            return false;
        }
        tmpi = kvstart+i+1;
        objval = *(ApeObject*)ape_valdict_getbykey(vm->stackobjects, &tmpi);
        ok = ape_object_map_setvalue(map_obj, key, objval);
        if(!ok)
        {
            return false;
        }
    }
    ape_vm_setstackpointer(vm, vm->stackptr - itmcount);
    ape_vm_pushstack(vm, map_obj);
    return true;
}

bool ape_vmdo_getindex(ApeVM* vm)
{
    bool ok;
    ApeObjType indextype;
    ApeObjType lefttype;
    ApeObject index;
    ApeObject left;
    index = ape_vm_popstack(vm);
    left = ape_vm_popstack(vm);
    lefttype = ape_object_value_type(left);
    indextype = ape_object_value_type(index);
    ok = ape_vm_getindex(vm, left, index, lefttype, indextype);
    if(!ok)
    {
        return false;
    }
    return true;
}

bool ape_vmdo_setindex(ApeVM* vm)
{
    int ix;
    bool ok;
    const char* indextn;
    const char* lefttn;
    ApeObjType lefttype;
    ApeObjType indextype;
    ApeObject index;
    ApeObject left;
    ApeObject newvalue;
    ApeObject oldvalue;
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
        return false;
    }
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
        ok = ape_object_array_setat(left, ix, newvalue);
        if(!ok)
        {
            lefttn = ape_object_value_typename(lefttype);
            indextn = ape_object_value_typename(indextype);
            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "setting array item failed (index %d out of bounds of %d)", ix, ape_object_array_getlength(left));
            return false;
        }
    }
    else if(lefttype == APE_OBJECT_MAP)
    {
        oldvalue = ape_object_map_getvalueobject(left, index);
        if(!ape_vm_checkassign(vm, oldvalue, newvalue))
        {
            return false;
        }
        ok = ape_object_map_setvalue(left, index, newvalue);
        if(!ok)
        {
            return false;
        }
    }
    return true;
}

bool ape_vmdo_getvalueat(ApeVM* vm)
{
    int ix;
    int leftlen;
    const char* str;
    const char* indextn;
    const char* lefttn;
    ApeObjType lefttype;
    ApeObjType indextype;
    ApeObject index;
    ApeObject left;
    ApeObject objres;
    index = ape_vm_popstack(vm);
    left = ape_vm_popstack(vm);
    lefttype = ape_object_value_type(left);
    indextype = ape_object_value_type(index);
    if(lefttype != APE_OBJECT_ARRAY && lefttype != APE_OBJECT_MAP && lefttype != APE_OBJECT_STRING)
    {
        lefttn = ape_object_value_typename(lefttype);
        indextn = ape_object_value_typename(indextype);
        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "type %s is not indexable (in APE_OPCODE_GETVALUEAT)", lefttn);
        return false;
    }
    objres = ape_object_make_null(vm->context);
    if(!ape_object_type_isnumeric(indextype))
    {
        lefttn = ape_object_value_typename(lefttype);
        indextn = ape_object_value_typename(indextype);
        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "cannot index %s with %s", lefttn, indextn);
        return false;
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
    return true;
}

bool ape_vmdo_defmoduleglobal(ApeVM* vm)
{
    int ix;
    ApeObject objval;
    ix = ape_frame_readuint16(vm->currentframe);
    objval = ape_vm_popstack(vm);
    ape_vm_setglobal(vm, ix, objval);
    return true;
}

bool ape_vmdo_setmoduleglobal(ApeVM* vm)
{
    int ix;
    ApeObject newvalue;
    ApeObject oldvalue;
    ix = ape_frame_readuint16(vm->currentframe);
    newvalue = ape_vm_popstack(vm);
    oldvalue = ape_vm_getglobal(vm, ix);
    if(!ape_vm_checkassign(vm, oldvalue, newvalue))
    {
        return false;
    }
    ape_vm_setglobal(vm, ix, newvalue);
    return true;
}

bool ape_vmdo_getmoduleglobal(ApeVM* vm)
{
    int ix;
    ApeObject global;
    ix = ape_frame_readuint16(vm->currentframe);
    global = ape_vm_getglobal(vm, ix);
    ape_vm_pushstack(vm, global);
    return true;
}

bool ape_vmdo_len(ApeVM* vm)
{
    int len;
    ApeObjType type;
    const char* ctypn;
    ApeObject objval;
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
        return false;
    }
    ape_vm_pushstack(vm, ape_object_make_fixednumber(vm->context, len));
    return true;
}

bool ape_vmdo_getthis(ApeVM* vm)
{
    ApeObject objval;
    #if 0
        objval = ape_vm_thispop(vm);
    #else
        objval = ape_vm_thisget(vm, 0);                        
    #endif
    ape_vm_pushstack(vm, objval);
    return true;
}

bool ape_vmdo_call(ApeVM* vm)
{
    bool ok;
    ApeUShort nargs;
    ApeObject callee;
    nargs = ape_frame_readuint8(vm->currentframe);
    callee = ape_vm_getstack(vm, nargs);
    ok = ape_vm_callobjectstack(vm, callee, nargs);
    if(!ok)
    {
        return false;
    }
    return true;
}

bool ape_vmdo_returnvalue(ApeVM* vm)
{
    bool ok;
    ApeObject objres;
    objres = ape_vm_popstack(vm);
    ok = ape_vm_framepop(vm);
    if(!ok)
    {
        return false;
    }
    ape_vm_pushstack(vm, objres);
    return true;
}

bool ape_vmdo_returnnothing(ApeVM* vm)
{
    bool ok;
    ok = ape_vm_framepop(vm);
    ape_vm_pushstack(vm, ape_object_make_null(vm->context));
    if(!ok)
    {
        ape_vm_popstack(vm);
        return false;
    }
    return true;
}

bool ape_vmdo_deflocal(ApeVM* vm)
{
    ApeInt pos;
    ApeSize idx;
    ApeObject popped;
    pos = ape_frame_readuint8(vm->currentframe);
    popped = ape_vm_popstack(vm);
    idx = vm->currentframe->basepointer + pos; 
    ape_valdict_set(vm->stackobjects, &idx, &popped);
    return true;
}

bool ape_vmdo_setlocal(ApeVM* vm)
{
    ApeInt pos;
    ApeSize idx;
    ApeObject newvalue;
    ApeObject oldvalue;
    pos = ape_frame_readuint8(vm->currentframe);
    newvalue = ape_vm_popstack(vm);
    idx = vm->currentframe->basepointer + pos;
    oldvalue = *(ApeObject*)ape_valdict_getbykey(vm->stackobjects, &idx);
    if(!ape_vm_checkassign(vm, oldvalue, newvalue))
    {
        return false;
    }
    idx = vm->currentframe->basepointer + pos;
    ape_valdict_set(vm->stackobjects, &idx, &newvalue);
    return true;
}

bool ape_vmdo_getlocal(ApeVM* vm)
{
    ApeInt pos;
    ApeSize idx;
    ApeObject objval;
    pos = ape_frame_readuint8(vm->currentframe);
    idx = vm->currentframe->basepointer + pos;
    void* tmp = ape_valdict_getbykey(vm->stackobjects, &idx);
    if(tmp != NULL)
    {
        objval = *(ApeObject*)tmp;
        ape_vm_pushstack(vm, objval);
    }
    return true;
}

bool ape_vmdo_getcontextglobal(ApeVM* vm)
{
    int ix;
    bool ok;
    ApeObject objval;
    ix = ape_frame_readuint16(vm->currentframe);
    ok = false;
    objval = ape_globalstore_getat(vm->globalstore, ix, &ok);
    if(!ok)
    {
        ape_vm_adderror(vm, APE_ERROR_RUNTIME, "global value %d not found", ix);
        return false;
    }
    ape_vm_pushstack(vm, objval);
    return true;
}

bool ape_vmdo_getfree(ApeVM* vm)
{
    ApeUShort ixfree;
    ApeObject objval;
    ixfree = ape_frame_readuint8(vm->currentframe);
    objval = ape_object_function_getfreeval(vm->currentframe->function, ixfree);
    ape_vm_pushstack(vm, objval);
    return true;
}

bool ape_vmdo_setfree(ApeVM* vm)
{
    ApeUShort ixfree;
    ApeObject objval;
    ixfree = ape_frame_readuint8(vm->currentframe);
    objval = ape_vm_popstack(vm);
    ape_object_function_setfreeval(vm->currentframe->function, ixfree, objval);
    return true;
}

bool ape_vmdo_currentfunction(ApeVM* vm)
{
    ApeObject currfunction;
    currfunction = vm->currentframe->function;
    ape_vm_pushstack(vm, currfunction);
    return true;
}

#define ape_vmexec_prim(fn) \
    if(!((fn)(vm))) \
    { \
        goto fail; \
    }

bool ape_vm_execfunc(ApeVM* vm, ApeObject function, ApeValArray * constants)
{
    /*
    * this is stupid, but old ANSI compilers require decls.
    * that being said, ape_vm_execfunc should be split
    * into smaller functions
    */
    bool ok;
    int ixrecover;
    ApeInt ui;
    ApeObject errobj;
    ApeSize i;
    ApeError* err;
    ApeScriptFunction* scriptfunc;

    vm->estate.constants = constants;
    #if 0
    if(vm->running)
    {
        ape_errorlist_add(vm->errors, APE_ERROR_USER, g_vmpriv_srcposinvalid, "VM is already executing code");
        return false;
    }
    #endif
    scriptfunc = ape_object_value_asscriptfunction(function);
    ok = false;
    ok = ape_vm_frameinit(&vm->estate.newframe, function, vm->stackptr - scriptfunc->numargs);
    if(!ok)
    {
        return false;
    }
    ok = ape_vm_framepush(vm, vm->estate.newframe);
    if(!ok)
    {
        ape_errorlist_add(vm->errors, APE_ERROR_USER, g_vmpriv_srcposinvalid, "pushing frame failed");
        return false;
    }
    vm->running = true;
    vm->lastpopped = ape_object_make_null(vm->context);
    while(vm->currentframe->ip < (ApeInt)vm->currentframe->bcsize)
    {
        vm->estate.opcode = ape_frame_readopcode(vm->currentframe);
        switch(vm->estate.opcode)
        {
            case APE_OPCODE_CONSTANT:
                {
                    ape_vmexec_prim(ape_vmdo_mkconstant);
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
                    ape_vmexec_prim(ape_vmdo_binary);
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
                    ape_vmexec_prim(ape_vmdo_compareequal);
                }
                break;

            case APE_OPCODE_ISEQUAL:
            case APE_OPCODE_NOTEQUAL:
            case APE_OPCODE_GREATERTHAN:
            case APE_OPCODE_GREATEREQUAL:
                {
                    ape_vmexec_prim(ape_vmdo_comparelogical);
                }
                break;
            case APE_OPCODE_MINUS:
            case APE_OPCODE_BITNOT:
            case APE_OPCODE_NOT:
                {
                    ape_vmexec_prim(ape_vmdo_unary);
                }
                break;
            case APE_OPCODE_JUMP:
                {
                    ape_vmexec_prim(ape_vmdo_jumpunconditional);
                }
                break;
            case APE_OPCODE_JUMPIFFALSE:
                {
                    ape_vmexec_prim(ape_vmdo_jumpiffalse);
                }
                break;
            case APE_OPCODE_JUMPIFTRUE:
                {
                    ape_vmexec_prim(ape_vmdo_jumpiftrue);
                }
                break;
            case APE_OPCODE_NULL:
                {
                    ape_vm_pushstack(vm, ape_object_make_null(vm->context));
                }
                break;
            case APE_OPCODE_DEFMODULEGLOBAL:
                {
                    ape_vmexec_prim(ape_vmdo_defmoduleglobal);
                }
                break;
            case APE_OPCODE_SETMODULEGLOBAL:
                {
                    ape_vmexec_prim(ape_vmdo_setmoduleglobal);
                }
                break;
            case APE_OPCODE_GETMODULEGLOBAL:
                {
                    ape_vmexec_prim(ape_vmdo_getmoduleglobal);
                }
                break;
            case APE_OPCODE_MKARRAY:
                {
                    ape_vmexec_prim(ape_vmdo_mkarray);
                }
                break;
            case APE_OPCODE_MAPSTART:
                {
                    ape_vmexec_prim(ape_vmdo_mapstart);
                }
                break;
            case APE_OPCODE_MAPEND:
                {
                    ape_vmexec_prim(ape_vmdo_mapend);
                }
                break;
            case APE_OPCODE_GETTHIS:
                {
                    ape_vmexec_prim(ape_vmdo_getthis);
                }
                break;
            case APE_OPCODE_GETINDEX:
                {
                    ape_vmexec_prim(ape_vmdo_getindex);
                }
                break;
            case APE_OPCODE_GETVALUEAT:
                {
                    ape_vmexec_prim(ape_vmdo_getvalueat);
                }
                break;
            case APE_OPCODE_CALL:
                {
                    ape_vmexec_prim(ape_vmdo_call);
                }
                break;
            case APE_OPCODE_RETURNVALUE:
                {
                    if(!ape_vmdo_returnvalue(vm))
                    {
                        goto end;
                    }
                }
                break;
            case APE_OPCODE_RETURNNOTHING:
                {
                    if(!ape_vmdo_returnnothing(vm))
                    {
                        goto end;
                    }
                }
                break;
            case APE_OPCODE_DEFLOCAL:
                {
                    ape_vmexec_prim(ape_vmdo_deflocal);
                }
                break;
            case APE_OPCODE_SETLOCAL:
                {
                    ape_vmexec_prim(ape_vmdo_setlocal);
                }
                break;
            case APE_OPCODE_GETLOCAL:
                {
                    ape_vmexec_prim(ape_vmdo_getlocal);
                }
                break;
            case APE_OPCODE_GETCONTEXTGLOBAL:
                {
                    ape_vmexec_prim(ape_vmdo_getcontextglobal);
                }
                break;
            case APE_OPCODE_MKFUNCTION:
                {
                    ape_vmexec_prim(ape_vmdo_mkfunction);
                }
                break;
            case APE_OPCODE_GETFREE:
                {
                    ape_vmexec_prim(ape_vmdo_getfree);
                }
                break;
            case APE_OPCODE_SETFREE:
                {
                    ape_vmexec_prim(ape_vmdo_setfree);
                }
                break;
            case APE_OPCODE_CURRENTFUNCTION:
                {
                    ape_vmexec_prim(ape_vmdo_currentfunction);
                }
                break;
            case APE_OPCODE_SETINDEX:
                {
                    ape_vmexec_prim(ape_vmdo_setindex);
                }
                break;
            case APE_OPCODE_DUP:
                {
                    ApeObject objval;
                    objval = ape_vm_getstack(vm, 0);
                    ape_vm_pushstack(vm, ape_object_value_copyflat(vm->context, objval));
                }
                break;
            case APE_OPCODE_LEN:
                {
                    ape_vmexec_prim(ape_vmdo_len);
                }
                break;
            case APE_OPCODE_MKNUMBER:
                {
                    ape_vmexec_prim(ape_vmdo_mknumber);
                }
                break;
            case APE_OPCODE_SETRECOVER:
                {
                    ape_vmexec_prim(ape_vmdo_setrecover);
                }
                break;
            case APE_OPCODE_IMPORT:
                {
                    
                }
                break;
            default:
                {
                    APE_ASSERT(false);
                    ape_vm_adderror(vm, APE_ERROR_RUNTIME, "unknown opcode: 0x%x", vm->estate.opcode);
                    goto fail;
                }
                break;
        }
    fail:
        if(ape_errorlist_count(vm->errors) > 0)
        {
            err = ape_errorlist_lasterror(vm->errors);
            if(err->errtype == APE_ERROR_RUNTIME && ape_errorlist_count(vm->errors) == 1)
            {
                ixrecover = -1;
                for(ui = vm->countframes - 1; ui >= 0; ui--)
                {
                    vm->estate.frame = (ApeFrame*)da_get(vm->frameobjects, ui);
                    if(vm->estate.frame->recoverip >= 0 && !vm->estate.frame->isrecovering)
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
                    while((ApeInt)vm->countframes > (ApeInt)(ixrecover + 1))
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
            ape_vm_collectgarbage(vm, vm->estate.constants, true);
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
    ape_vm_collectgarbage(vm, vm->estate.constants, true);
    vm->running = false;
    return ape_errorlist_count(vm->errors) == 0;
}


