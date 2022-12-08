
// this file is intended to be processed via mksanity.h.
// it generates sanity.ape. this step is currently needed because ape
// has (as of now) insufficient mixin capabilities. :((((

const trailing_comma_array = [
    1,
    2,
    3,
]

println("Object.length(trailing_comma_array)=", Object.length(trailing_comma_array));
check(Object.length(trailing_comma_array), 3)

const trailing_comma_dict = {
    a: 1,
    b: 2,
    c: 3,
}

check(Object.length(trailing_comma_dict), 3)

// tests a case where sp is incremented over already freed objects
// and gc tries to mark them
function test_gc_fail(a, b, c) { const x = []; const y = []; const z = []; }
test_gc_fail(1, 2, 3)
test_gc_fail(1, 2, 3)



function fun() {
    const arr = [1, 2, 3]
    const dict = {"a": 1, "b": 2}
    const str = "lorem ipsum"
    dict["a"] = 1
// comment 1 dict["a"] // test
// comment 2
    const res = function(a, b, c) {
        const res_str = tostring(a) + tostring(b) + tostring(c)
        return res_str; // comment
    }
    return res(1, 2, 3)
}

println(fun())

function make_person(name) {
    return {
        name: name,
        hello: function() {
            println("hello " + this.name)
        },
        make_hello: function()
        {
            return "hello "+ this.name
        }
    }
}

const person = make_person("Krzysztof")
person.hello()


check(Object.keys({"a": 1, "b": 2})[1], "b")
check(tostring(1), "1")

const popular_names = ["Krzysztof", "Zbigniew", "Grzegorz"]
for (name in popular_names) {
    println(name)
    if (name == "Zbigniew") {
        break
    }
}

{
    println("args: ", 1, [1, 2, 3], {"a": 1}, "lorem", true, function() {}, println)
    if (true) {}
    if (false) {}
    var i = 0
    var j = 0
    while ((i+=1) < 10) { break; }
    while ((j+=1) < 10) { }
}

const val = 123

check("abc"[0], "a")
checknot("abc"[0], "b")

// there's an issue with object_deep_copy() that throws off valgrind
//const person1 = make_person("Krzysztof")
//const person2 = deep_copy(person1)
//person2.name = "Mati"
//check(person1.make_hello(), "hello Krzysztof")
//check(person2.make_hello(), "hello Mati")


function contains_item(to_find, items) {
    for (item in items) {
        if (item == to_find) {
            return true
        }
    }
    return false
}

const cities = ["Kraków", "Warsaw", "Gdańsk"]
if (contains_item("Kraków", cities)) {
    println("found!")
}

function block_test() {
    var x = 0
    {
        x = 1
    }
    return x
}

check(block_test(), 1)

const big_array = array(1000)
check(Object.length(big_array), 1000)

function return_no_semicolon() { return }

// operator overloading test
function vec2(x, y) {
    return {
        x: x,
        y: y,
        __operator_add__: function(a, b) {
            return vec2(a.x + b.x, a.y + b.y)
        },
        __operator_sub__: function(a, b) {
            return vec2(a.x - b.x, a.y - b.y)
        },

        __operator_minus__: function(a) { return vec2(-a.x, -a.y) },
        __operator_mul__: function(a, b) {
            if (Object.isnumber(a)) {
                return vec2(b.x * a, b.y * a)
            }
            else if (Object.isnumber(b) == true)
            {
                return vec2(a.x * b, a.y * b)
            }
            else {
                return vec2(a.x * b.x, a.y * b.y)
            }
        },
    }
}

const va = vec2(1, 2)
const vb = vec2(3, 4)

var v = va + vb
check(v.x, 4)
check(v.y, 6)

v = va * 2
check(v.x, 2)
check(v.y, 4)

v = -va
check(v.x, -1)
check(v.y, -2)

check((1|2), 3)
check((1&2), 0)
check((1^3), 2)
check(( 1 << 16), 65536)
check((-1 << 2), -4)
check(( 8 >> 1), 4)
check((-8 >> 1), -4)

function recover_test_1() {
    recover (e) {
        return 1
    }
    return 2
}

check(recover_test_1(), 2)

function recover_test_2() {
    recover (e) {
        return 1
    }
    return crash()
}

check(recover_test_2(), 1)

function recover_test_3() {
    recover (e) {
        return 1
    }

    recover (e) {
        return 2
    }

    function recover_inner() {
        return crash()
    }

    return recover_inner()
}

check(recover_test_3(), 2)

function recover_test_4() {
    recover (e) {
        return 2
    }
    
    function recover_test_inner() {
        recover (e) {
            return crash()
        }

        return crash()
    }
    return recover_test_inner()
}

check(recover_test_4(), 2)

var this_test = {
    inner: {
        name: "lorem",
        get_name: function() {
            return this.name
        }
    },
    get_inner_name: function() {
        return this.inner.get_name()
    }
}

check(this_test.get_inner_name(), "lorem")

function add(x, y) { return x + y }
var templ_var = 3
check(`foo${templ_var}bar`, "foo3bar")
check(`lorem${add(`${add(`x`, "y")}`, `z`)}ipsum`, "loremxyzipsum")
check(`${4 * 2}`, "8")
check(`${{}}`, "{}")
check(`foo\${x}bar`, "foo${x}bar")
check(`${1} ${2}`, "1 2")

function test_ternary(x) {
    var res = x == 1 ? 1 : 2;
    return res;
}
check(test_ternary(1), 1)
check(test_ternary(3), 2)
check((true ? true ? 1 : 2 : 3), 1)
check((true ? false ? 1 : 2 : 3), 2)
check(add(true ? 1 : 2, true ? 3 : 5), 4)
check(add(true ? 1 : 2, 3), 4)
check(add(1, true ? 2 : 3), 3)

var test_obj = {
    fun: function() { return 2}
}

check(test_obj.fun(), 2)
checknot(test_obj["fun"], 2)
check(test_obj["fun"](), 2)

function get_test_arr() {
    var test_arr = [
        function() { return 0 },
        function() { return 1 },
        function() { return 2 },
    ]
    return test_arr
}

var fun_i = 0
check(get_test_arr()[fun_i++](), 0)
check(get_test_arr()[fun_i++](), 1)
check(get_test_arr()[fun_i++](), 2)

check(get_test_arr()[--fun_i](), 2)
check(get_test_arr()[--fun_i](), 1)
check(get_test_arr()[--fun_i](), 0)

{
    var a = 10
    var b = a++ + ++a + ++a +a
    check(b, 48)
}

{
    var a = 0
    var b = 0
    check((a-- - --b), 1)
}

{
    var obj = { foo: 1 }
    obj.foo++
    check(obj.foo, 2)
    check(obj.foo++, 2)
    check(obj.foo, 3)
    check(--obj.foo, 2)
    check(obj.foo--, 2)
    check(obj.foo, 1)
}

check(1, 1)
checknot(1, "1")
checknot(1, {})
check("a" + "b", "ab")

{
    var n = 256
    var str = ""
    for (var i = 0; i < n; i++) {
        str += "x"
    }
    println("str = ", Object.length(str))
    //check(Object.length(str), n)
    for (var i = 0; i < n; i++) {
        assert(str[i] == "x")
    }
}

check(reverse("abc"), "cba")
check(reverse("abcd"), "dcba")

check(slice("abc", 1), "bc")
check(slice("abc", -1), "c")

check(concat("abc", "def"), "abcdef")

println("all is well")
