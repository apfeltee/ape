
#include "inline.h"

ApePseudoClass* ape_make_pseudoclass(ApeContext* ctx, ApeStrDict* dictref, const char* classname)
{
    ApePseudoClass* psc;
    psc = (ApePseudoClass*)ape_allocator_alloc(&ctx->alloc, sizeof(ApePseudoClass));
    memset(psc, 0, sizeof(ApePseudoClass));
    psc->context = ctx;
    psc->classname = classname;
    psc->fndictref = dictref;
    return psc;
}

void* ape_pseudoclass_destroy(ApeContext* ctx, ApePseudoClass* psc)
{
    ape_allocator_free(&ctx->alloc, psc);
    psc = NULL;
    return NULL;
}

bool ape_pseudoclass_setmethod(ApePseudoClass* psc, const char* name, ApeObjMemberItem* itm)
{
    return ape_strdict_set(psc->fndictref, name, itm);
}

ApeObjMemberItem* ape_pseudoclass_getmethodbyhash(ApePseudoClass* psc, const char* name, unsigned long hash)
{
    void* raw;
    ApeObjMemberItem* aom;
    raw = ape_strdict_getbyhash(psc->fndictref, name, hash);
    aom = (ApeObjMemberItem*)raw;
    return aom;
}

ApeObjMemberItem* ape_pseudoclass_getmethodbyname(ApePseudoClass* psc, const char* name)
{
    unsigned long hs;
    ApeSize nlen;
    nlen = strlen(name);
    hs = ape_util_hashstring(name, nlen);
    return ape_pseudoclass_getmethodbyhash(psc, name, hs);
}

/** TODO: ctx->pseudoclasses should perhaps be a StrDict, for better lookup? */
ApePseudoClass* ape_context_make_pseudoclass(ApeContext* ctx, ApeStrDict* dictref, ApeObjType typ, const char* classname)
{
    bool ok;
    const char* stag;
    ApePseudoClass* psc;
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

ApePseudoClass* ape_context_findpseudoclassbytype(ApeContext* ctx, ApeObjType typ)
{
    void* raw;
    const char* stag;
    ApePseudoClass* psc;
    /* if(typ == APE_OBJECT_) */
    stag = ape_object_value_typename(typ);
    raw = ape_strdict_getbyname(ctx->classmapping, stag);
    if(raw == NULL)
    {
        return NULL;
    }
    psc = (ApePseudoClass*)raw;
    return psc;
}

ApeObjMemberItem* ape_builtin_find_objectfunc(ApeContext* ctx, ApeStrDict* dict, const char* idxname, unsigned long idxhash)
{
    ApeObjMemberItem* p;
    (void)ctx;
    p = (ApeObjMemberItem*)ape_strdict_getbyhash(dict, idxname, idxhash);
    if(p != NULL)
    {
        return p;
    }
    return NULL;
}

ApeObjMemberItem* builtin_get_object(ApeContext* ctx, ApeObjType objt, const char* idxname, unsigned long idxhash)
{
    ApePseudoClass* psc;
    ApeObjMemberItem* aom;
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


