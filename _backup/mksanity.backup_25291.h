
// run this file as:
// gcc -E -P mksanity.h -o sanity.ape

#define check_wrap(a, b, op) \
    _check_result = (a op b);\
    println(`checking (${#a} ${#op} ${b}) = ${_check_result}`); \
    assert(_check_result);

#define check(a, b) check_wrap(a, b, ==)
#define checknot(a, b) check_wrap(a, b, !=)

"do not edit this file directly, edit testinput.inc!";
var _check_result = 0;
#include "testinput.inc"