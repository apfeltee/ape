

function append(arr, thing)
{
    arr.push(thing);
}

function len(thing)
{
    return thing.length;
}

function ord(s)
{
    return s.charCodeAt(0);
}

function bitnot(n)
{
    return (~n);
}

function substr(s, a, b=null)
{
    if(b == null)
    {
        return s.substring(a);
    }
    return s.substring(a, b);
}

function println(...args)
{
    console.log(...args);
}

// this jank is needed so we don't accidentally override something in the actual target script.
var __noderun_data = {};
__noderun_data.process = process;
__noderun_data.libfs = require("fs");

// [1] is *this* file. would obviously result in an infinite loop.
if(__noderun_data.process.argv.length > 2)
{
    __noderun_data.inputfile = __noderun_data.process.argv[2];
    eval(__noderun_data.libfs.readFileSync(__noderun_data.inputfile).toString());
}


