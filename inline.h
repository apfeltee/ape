
#pragma once
/*
* this file contains code that is intended to be inlined.
* it is *not* required in production code; only ape.h is.
*/

#include "ape.h"

static APE_INLINE int ape_util_doubletoint(double n)
{
    if(n == 0)
    {
        return 0;
    }
    if(isnan(n))
    {
        return 0;
    }
    if(n < 0)
    {
        n = -floor(-n);
    }
    else
    {
        n = floor(n);
    }
    if(n < INT_MIN)
    {
        return INT_MIN;
    }
    if(n > INT_MAX)
    {
        return INT_MAX;
    }
    return (int)n;
}

static APE_INLINE int ape_util_numbertoint32(double n)
{
    double two32 = 4294967296.0;
    double two31 = 2147483648.0;

    if(!isfinite(n) || n == 0)
    {
        return 0;
    }
    n = fmod(n, two32);
    n = n >= 0 ? floor(n) : ceil(n) + two32;
    if(n >= two31)
    {
        return n - two32;
    }
    return n;
}

static APE_INLINE unsigned int ape_util_numbertouint32(double n)
{
    return (unsigned int)ape_util_numbertoint32(n);
}

// fixme
static APE_INLINE ApeUInt_t ape_util_floattouint(ApeFloat_t val)
{
    return val;
    union
    {
        ApeUInt_t fltcast_uint64;
        ApeFloat_t fltcast_double;
    } temp = { .fltcast_double = val };
    return temp.fltcast_uint64;
}

static APE_INLINE ApeFloat_t ape_util_uinttofloat(ApeUInt_t val)
{
    return val;
    union
    {
        ApeUInt_t fltcast_uint64;
        ApeFloat_t fltcast_double;
    } temp = { .fltcast_uint64 = val };
    return temp.fltcast_double;
}




