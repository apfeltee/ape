
#include "inline.h"

ApePseudoClass_t* ape_make_pseudoclass(ApeContext_t* ctx, ApeStrDict_t* dictref, const char* classname)
{
    ApePseudoClass_t* psc;
    psc = (ApePseudoClass_t*)ape_allocator_alloc(&ctx->alloc, sizeof(ApePseudoClass_t));
    memset(psc, 0, sizeof(ApePseudoClass_t));
    psc->context = ctx;
    psc->classname = classname;
    psc->fndictref = dictref;
    return psc;
}

void* ape_pseudoclass_destroy(ApePseudoClass_t* psc)
{
    ApeContext_t* ctx;
    ctx = psc->context;
    ape_allocator_free(&ctx->alloc, psc);
    psc = NULL;
    return NULL;
}

bool ape_pseudoclass_setmethod(ApePseudoClass_t* psc, const char* name, ApeObjMemberItem_t* itm)
{
    return ape_strdict_set(psc->fndictref, name, itm);
}

ApeObjMemberItem_t* ape_pseudoclass_getmethodbyhash(ApePseudoClass_t* psc, const char* name, unsigned long hash)
{
    void* raw;
    ApeObjMemberItem_t* aom;
    raw = ape_strdict_getbyhash(psc->fndictref, name, hash);
    aom = (ApeObjMemberItem_t*)raw;
    return aom;
}

ApeObjMemberItem_t* ape_pseudoclass_getmethodbyname(ApePseudoClass_t* psc, const char* name)
{
    unsigned long hs;
    ApeSize_t nlen;
    nlen = strlen(name);
    hs = ape_util_hashstring(name, nlen);
    return ape_pseudoclass_getmethodbyhash(psc, name, hs);
}

/** TODO: ctx->pseudoclasses should perhaps be a StrDict, for better lookup? */
ApePseudoClass_t* ape_context_make_pseudoclass(ApeContext_t* ctx, ApeStrDict_t* dictref, ApeObjType_t typ, const char* classname)
{
    bool ok;
    const char* stag;
    ApePseudoClass_t* psc;
    stag = ape_object_value_typename(typ);
    psc = ape_make_pseudoclass(ctx, dictref, classname);
    ape_ptrarray_push(ctx->pseudoclasses, &psc);
    ok = ape_strdict_set(ctx->classmapping, stag, psc);
    if(!ok)
    {
        fprintf(stderr, "failed to set classmapping?\n");
    }
    return psc;
}

ApePseudoClass_t* ape_context_findpseudoclassbytype(ApeContext_t* ctx, ApeObjType_t typ)
{
    void* raw;
    const char* stag;
    ApePseudoClass_t* psc;
    /* if(typ == APE_OBJECT_) */
    stag = ape_object_value_typename(typ);
    raw = ape_strdict_getbyname(ctx->classmapping, stag);
    if(raw == NULL)
    {
        return NULL;
    }
    psc = (ApePseudoClass_t*)raw;
    return psc;
}

ApeObjMemberItem_t* ape_builtin_find_objectfunc(ApeContext_t* ctx, ApeStrDict_t* dict, const char* idxname, unsigned long idxhash)
{
    ApeObjMemberItem_t* p;
    (void)ctx;
    p = (ApeObjMemberItem_t*)ape_strdict_getbyhash(dict, idxname, idxhash);
    if(p != NULL)
    {
        return p;
    }
    return NULL;
}

ApeObjMemberItem_t* builtin_get_object(ApeContext_t* ctx, ApeObjType_t objt, const char* idxname, unsigned long idxhash)
{
    ApePseudoClass_t* psc;
    ApeObjMemberItem_t* aom;
    /* todo: currently maps are explicitly not supported. */
    if(objt == APE_OBJECT_MAP)
    {
        return NULL;
    }
    psc = ape_context_findpseudoclassbytype(ctx, objt);
    if(psc == NULL)
    {
        fprintf(stderr, "failed to get pseudo class for type '%s' and name '%s'\n", ape_object_value_typename(objt), idxname);
        return NULL;
    }
    aom = ape_pseudoclass_getmethodbyhash(psc, idxname, idxhash);
    if(aom == NULL)
    {
        fprintf(stderr, "failed to find method '%s' for this type", idxname);
        return NULL;
    }
    return aom;
}


