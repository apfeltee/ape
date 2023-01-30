
#include "inline.h"

/*
* TODO: strings are internally allocated outside of the mempool.
* this is fine for now, but is less than ideal...
*/

char* ape_object_string_getinternalobjdata(ApeGCObjData* data)
{
    APE_ASSERT(data->datatype == APE_OBJECT_STRING);
    return data->valstring.valalloc;
}

const char* ape_object_string_getdata(ApeObject object)
{
    ApeGCObjData* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_STRING);
    data = ape_object_value_allocated_data(object);
    return ape_object_string_getinternalobjdata(data);
}

char* ape_object_string_getmutable(ApeObject object)
{
    ApeGCObjData* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_STRING);
    data = ape_object_value_allocated_data(object);
    return ape_object_string_getinternalobjdata(data);
}

ApeObject ape_object_make_string(ApeContext* ctx, const char* string)
{
    return ape_object_make_stringlen(ctx, string, strlen(string));
}

ApeObject ape_object_make_stringlen(ApeContext* ctx, const char* string, ApeSize len)
{
    bool ok;
    ApeObject res;
    res = ape_object_make_stringcapacity(ctx, len);
    if(ape_object_value_isnull(res))
    {
        return res;
    }
    ok = ape_object_string_append(ctx, res, string, len);
    if(!ok)
    {
        return ape_object_make_null(ctx);
    }
    return res;
}

ApeObject ape_object_make_stringcapacity(ApeContext* ctx, ApeSize capacity)
{
    bool ok;
    ApeGCObjData* data;
    data = ape_object_make_objdata(ctx, APE_OBJECT_STRING);
    if(!data)
    {
        return ape_object_make_null(ctx);
    }
    data->valstring.hash = 0;
    data->valstring.valalloc = NULL;
    ok = ape_object_string_reservecapacity(ctx, data, capacity);
    if(!ok)
    {
        return ape_object_make_null(ctx);
    }
    return object_make_from_data(ctx, APE_OBJECT_STRING, data);
}

bool ape_object_string_reservecapacity(ApeContext* ctx, ApeGCObjData *data, ApeSize capacity)
{
    (void)ctx;
    if(data->valstring.valalloc == NULL)
    {
        data->valstring.valalloc = ds_newempty(ctx);
    }
    if(capacity >= ds_getavailable(data->valstring.valalloc))
    {
        //data->valstring.valalloc = ds_makeroomfor(data->valstring.valalloc, capacity+1);
    }
    return true;
}

ApeSize ape_object_string_getlength(ApeObject object)
{
    ApeGCObjData* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_STRING);
    data = ape_object_value_allocated_data(object);
    if(data->valstring.valalloc == NULL)
    {
        return 0;
    }
    return ds_getlength(data->valstring.valalloc);
}

void ape_object_string_setlength(ApeObject object, ApeSize len)
{
    ApeGCObjData* data;
    APE_ASSERT(ape_object_value_type(object) == APE_OBJECT_STRING);
    data = ape_object_value_allocated_data(object);
    ds_setlength(data->valstring.valalloc, len);
}

bool ape_object_string_append(ApeContext* ctx, ApeObject obj, const char* src, ApeSize len)
{
    ApeGCObjData* data;
    APE_ASSERT(ape_object_value_type(obj) == APE_OBJECT_STRING);
    data = ape_object_value_allocated_data(obj);
    data->valstring.valalloc = ds_appendlen(data->valstring.valalloc, src, len, ctx);
    return true;
}

unsigned long ape_object_string_gethash(ApeObject obj)
{
    const char* rawstr;
    ApeSize rawlen;
    ApeGCObjData* data;
    APE_ASSERT(ape_object_value_type(obj) == APE_OBJECT_STRING);
    data = ape_object_value_allocated_data(obj);
    if(data->valstring.hash == 0)
    {
        rawstr = ape_object_string_getdata(obj);
        rawlen = ape_object_string_getlength(obj);
        data->valstring.hash = ape_util_hashstring(rawstr, rawlen);
        if(data->valstring.hash == 0)
        {
            data->valstring.hash = 1;
        }
    }
    return data->valstring.hash;
}

static ApeObject objfn_string_length(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    ApeInt len;
    ApeObject self;
    (void)vm;
    (void)data;
    (void)argc;
    (void)args;
    /* fixme: value is passed incorrectly? */
    self = ape_vm_thispop(vm);
    len = ape_object_string_getlength(self);
    return ape_object_make_floatnumber(vm->context, len);
}

static ApeObject objfn_string_substr(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    ApeInt begin;
    ApeInt end;
    ApeInt len;
    ApeInt nlen;
    ApeObject self;
    ApeArgCheck check;
    const char* str;
    (void)data;
    self = ape_vm_thispop(vm);
    ape_args_init(vm, &check, "substr", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);        
    }
    str = ape_object_string_getdata(self);
    len = ape_object_string_getlength(self);
    begin = ape_object_value_asnumber(args[0]);
    end = len;
    if(ape_args_checkoptional(&check, 1, APE_OBJECT_NUMERIC, true))
    {
        end = ape_object_value_asnumber(args[1]);
    }
    if(begin < 0)
    {
        begin = len + begin;
        if(begin < 0)
        {
            begin = 0;
        }
    }
    /* fixme: this is actually incorrect */
    if((begin >= len))
    {
        return ape_object_make_null(vm->context);
    }
    if(end >= len)
    {
        end = len;
    }
    nlen = end - begin;
    fprintf(stderr, "substr: len=%d, begin=%d, end=%d, nlen=%d\n", (int)len, (int)begin, (int)end, (int)nlen);
    return ape_object_make_stringlen(vm->context, str+begin, nlen);
}

static ApeObject objfn_string_split(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    char c;
    const char* inpstr;
    const char* delimstr;
    char* itm;
    ApeSize i;
    ApeSize inplen;
    ApeSize delimlen;
    ApeObject arr;
    ApeObject self;
    ApePtrArray* parr;
    ApeArgCheck check;
    (void)data;
    delimstr = "";
    delimlen = 0;
    ape_args_init(vm, &check, "split", argc, args);
    self = ape_vm_thispop(vm);
    inpstr = ape_object_string_getdata(self);
    if(ape_args_checkoptional(&check, 0, APE_OBJECT_STRING, true))
    {
        delimstr = ape_object_string_getdata(args[0]);
        delimlen = ape_object_string_getlength(args[0]);
    }
    arr = ape_object_make_array(vm->context);
    if(delimlen == 0)
    {
        inplen = ape_object_string_getlength(self);
        for(i=0; i<inplen; i++)
        {
            c = inpstr[i];
            ape_object_array_pushvalue(arr, ape_object_make_stringlen(vm->context, &c, 1));
        }
    }
    else
    {
        parr = ape_util_splitstring(vm->context, inpstr, delimstr);
        for(i=0; i<ape_ptrarray_count(parr); i++)
        {
            itm = (char*)ape_ptrarray_get(parr, i);
            ape_object_array_pushvalue(arr, ape_object_make_string(vm->context, itm));
            ape_allocator_free(&vm->context->alloc, (void*)itm);
        }
        ape_ptrarray_destroy(parr);
    }
    return arr;
}

static ApeObject objfn_string_indexof(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    char findme;
    const char* findstr;
    const char* inpstr;
    ApeSize i;
    ApeSize inplen;
    ApeSize findlen;
    ApeObject self;
    ApeObjType styp;
    ApeArgCheck check;
    (void)data;
    self = ape_vm_thispop(vm);
    ape_args_init(vm, &check, "index", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_STRING | APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);
    }
    findme = -1;
    inpstr = ape_object_string_getdata(self);
    inplen = ape_object_string_getlength(self);
    styp = ape_object_value_type(args[0]);
    if(styp == APE_OBJECT_STRING)
    {
        findstr = ape_object_string_getdata(args[0]);
        findlen = ape_object_string_getlength(args[0]);
        if(findlen == 0)
        {
            return ape_object_make_floatnumber(vm->context, -1);
        }
        findme = findstr[0];
    }
    else if(ape_object_type_isnumber(styp))
    {
        findme = ape_object_value_asnumber(args[0]);
        if(findme == -1)
        {
            return ape_object_make_floatnumber(vm->context, -1);
        }
    }
    for(i=0; i<inplen; i++)
    {
        if(inpstr[i] == findme)
        {
            return ape_object_make_floatnumber(vm->context, i);
        }
    }
    return ape_object_make_floatnumber(vm->context, -1);
}


static ApeObject objfn_string_charat(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    char ch;
    const char* inpstr;
    ApeSize idx;
    ApeSize inplen;
    ApeObject self;
    ApeArgCheck check;
    (void)data;
    (void)inplen;
    ape_args_init(vm, &check, "charAt", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);
    }
    idx = ape_object_value_asnumber(args[0]);
    self = ape_vm_thispop(vm);
    inpstr = ape_object_string_getdata(self);
    inplen = ape_object_string_getlength(self);
    ch = inpstr[idx];
    return ape_object_make_stringlen(vm->context, &ch, 1);
}


static ApeObject objfn_string_byteat(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    char ch;
    const char* inpstr;
    ApeSize idx;
    ApeSize inplen;
    ApeObject self;
    ApeArgCheck check;
    (void)data;
    (void)inplen;
    ape_args_init(vm, &check, "charAt", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);
    }
    idx = ape_object_value_asnumber(args[0]);
    self = ape_vm_thispop(vm);
    inpstr = ape_object_string_getdata(self);
    inplen = ape_object_string_getlength(self);
    ch = inpstr[idx];
    return ape_object_make_floatnumber(vm->context, ch);
}

ApeObject ape_builtins_stringformat(ApeContext* ctx, const char* fmt, ApeSize fmtlen, ApeSize argc, ApeObject* args)
{
    char cch;
    char pch;
    char nch;
    char chval;
    ApeSize i;
    ApeSize idx;
    ApeObject srt;
    ApeArgCheck check;
    ApeWriter* buf;
    (void)pch;
    idx = 0;
    cch = -1;
    pch = -1;
    nch = -1;
    buf = ape_make_writercapacity(ctx, fmtlen + 10);
    ape_args_init(ctx->vm, &check, "format-string", argc, args);
    for(i=0; i<fmtlen; i++)
    {
        pch = cch;
        cch = fmt[i];
        if((i + 1) < fmtlen)
        {
            nch = fmt[i + 1];
        }
        if((cch == '%') && (nch != -1))
        {
            switch(nch)
            {
                case '%':
                    {
                        i++;
                        ape_writer_appendlen(buf, &nch, 1);
                    }
                    break;
                case 'd':
                case 'g':
                case 'i':
                    {
                        i++;
                        if(!ape_args_check(&check, idx, APE_OBJECT_NUMERIC))
                        {
                            return ape_object_make_null(ctx);
                        }
                        ape_tostring_object(buf, args[idx], false);
                        idx++;
                    }
                    break;
                case 'c':
                    {
                        i++;
                        if(!ape_args_check(&check, idx, APE_OBJECT_NUMERIC))
                        {
                            return ape_object_make_null(ctx);
                        }
                        chval = ape_object_value_asnumber(args[idx]);
                        ape_writer_appendlen(buf, &chval, 1);
                        idx++;
                    }
                    break;
                case 's':
                    {
                        i++;
                        if(!ape_args_check(&check, idx, APE_OBJECT_ANY))
                        {
                            return ape_object_make_null(ctx);
                        }
                        ape_tostring_object(buf, args[idx], false);
                        idx++;
                    }
                    break;
                case 'p':
                case 'q':
                case 'o':
                    {
                        i++;
                        if(!ape_args_check(&check, idx, APE_OBJECT_ANY))
                        {
                            return ape_object_make_null(ctx);
                        }
                        ape_tostring_object(buf, args[idx], true);
                        idx++;
                    }
                    break;
                default:
                    {
                        ape_vm_adderror(ctx->vm, APE_ERROR_RUNTIME, "invalid format flag '%%c'", nch);
                    }
                    break;
            }
        }
        else
        {
            ape_writer_appendlen(buf, &cch, 1);
        }
    }
    srt = ape_object_make_stringlen(ctx, ape_writer_getdata(buf), ape_writer_getlength(buf));
    ape_writer_destroy(buf);
    return srt;

}

static ApeObject objfn_string_format(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    const char* inpstr;
    ApeSize inplen;
    ApeObject self;
    (void)data;
    self = ape_vm_thispop(vm);
    inpstr = ape_object_string_getdata(self);
    inplen = ape_object_string_getlength(self);
    return ape_builtins_stringformat(vm->context, inpstr, inplen, argc, args);
}

static ApeObject objfn_string_reverse(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    char ch;
    ApeInt i;
    const char* inpstr;
    ApeSize inplen;
    ApeObject self;
    ApeObject res;
    (void)data;
    (void)argc;
    (void)args;
    self = ape_vm_thispop(vm);
    inpstr = ape_object_string_getdata(self);
    inplen = ape_object_string_getlength(self);
    res = ape_object_make_string(vm->context, "");
    i = inplen-1;
    while(true)
    {
        ch = inpstr[i];
        ape_object_string_append(vm->context, res, &ch, 1);
        i--;
        if(i < 0)
        {
            break;
        }
    }
    return res;
}


static ApeObject cfn_string_chr(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    char c;
    char buf[2];
    ApeFloat val;
    (void)data;
    ApeArgCheck check;
    ape_args_init(vm, &check, "chr", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_NUMERIC))
    {
        return ape_object_make_null(vm->context);
    }
    val = ape_object_value_asnumber(args[0]);
    c = (char)val;
    buf[0] = c;
    buf[1] = '\0';
    return ape_object_make_stringlen(vm->context, buf, 1);
}

static ApeObject cfn_string_ord(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    const char* str;
    (void)data;
    ApeArgCheck check;
    ape_args_init(vm, &check, "sqrt", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_STRING | APE_OBJECT_NULL))
    {
        return ape_object_make_null(vm->context);
    }
    if(ape_object_value_isnull(args[0]))
    {
        return ape_object_make_floatnumber(vm->context, 0);
    }
    str = ape_object_string_getdata(args[0]);
    return ape_object_make_floatnumber(vm->context, str[0]);
}

static ApeObject cfn_string_join(ApeVM* vm, void* data, ApeSize argc, ApeObject* args)
{
    const char* sstr;
    const char* bstr;
    ApeSize i;
    ApeSize slen;
    ApeSize alen;
    ApeSize blen;
    ApeObject rt;
    ApeObject arritem;
    ApeObject arrobj;
    ApeObject sjoin;
    ApeWriter* wr;
    ApeArgCheck check;
    (void)data;
    sstr = "";
    slen = 0;
    ape_args_init(vm, &check, "String.join", argc, args);
    if(!ape_args_check(&check, 0, APE_OBJECT_ARRAY))
    {
        return ape_object_make_null(vm->context);        
    }
    arrobj = args[0];
    if(ape_args_checkoptional(&check, 1, APE_OBJECT_STRING, true))
    {
        sjoin = args[1];
        if(ape_object_value_type(sjoin) != APE_OBJECT_STRING)
        {
            ape_vm_adderror(vm, APE_ERROR_RUNTIME, "String.join expects second argument to be a string");
            return ape_object_make_null(vm->context);
        }
        sstr = ape_object_string_getdata(sjoin);
        slen = ape_object_string_getlength(sjoin);
    }
    alen = ape_object_array_getlength(arrobj);
    wr = ape_make_writer(vm->context);
    for(i=0; i<alen; i++)
    {
        arritem = ape_object_array_getvalue(arrobj, i);
        ape_tostring_object(wr, arritem, false);
        if((i + 1) < alen)
        {
            ape_writer_appendlen(wr, sstr, slen);
        }
    }
    bstr = ape_writer_getdata(wr);
    blen = ape_writer_getlength(wr);
    rt = ape_object_make_stringlen(vm->context, bstr, blen);
    ape_writer_destroy(wr);
    return rt;
}

void ape_builtins_install_string(ApeVM* vm)
{
    ApeSize i;
    ApePseudoClass* psc;
    static const char classname[] = "String";
    static ApeNativeItem staticfuncs[]=
    {
        {"join", cfn_string_join},
        {"chr", cfn_string_chr},
        {"ord", cfn_string_ord},

        /* js-isms */
        {"fromCharCode", cfn_string_chr},

        #if 0
        {"trim", cfn_string_trim},
        #endif
        {NULL, NULL},
    };

    static ApeObjMemberItem memberfuncs[]=
    {
        {"length", false, objfn_string_length},
        {"split", true, objfn_string_split},
        {"index", true, objfn_string_indexof},
        {"substr", true, objfn_string_substr},
        {"substring", true, objfn_string_substr},
        {"slice", true, objfn_string_substr},
        /* js-isms */
        {"charAt", true, objfn_string_charat},
        {"charCodeAt", true, objfn_string_byteat},
        {"indexOf", true, objfn_string_indexof},

        /* utilities */
        {"format", true, objfn_string_format},
        {"reverse", true, objfn_string_reverse},
        /* TODO: implement me! */
        #if 0
        /* {"", true, objfn_string_}, */
        {"upper", true, objfn_string_toupper},
        {"lower", true, objfn_string_tolower},
        {"toupper", true, objfn_string_toupper},
        {"tolower", true, objfn_string_tolower},
        {"strip", true, objfn_string_stripall},
        {"rstrip", true, objfn_string_stripright},
        {"lstrip", true, objfn_string_stripleft},
        {"match", true, objfn_string_matchrx},
        {"contains", true, objfn_string_contains},
        #endif
        {NULL, false, NULL},
    };
    ape_builtins_setup_namespace(vm, classname, staticfuncs);
    psc = ape_context_make_pseudoclass(vm->context, vm->context->objstringfuncs, APE_OBJECT_STRING, classname);
    for(i=0; memberfuncs[i].name != NULL; i++)
    {
        ape_pseudoclass_setmethod(psc, memberfuncs[i].name, &memberfuncs[i]);
    }
}
