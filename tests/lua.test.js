'use strict';
// UNIT tests for the sandboxed Lua interpreter (src/script/lua.js). Expected outputs were derived from Lua 5.1 semantics by hand, not by running real Lua.
const test = require('node:test'); const assert = require('node:assert/strict');
const { Interp, Scheduler, BudgetError } = require('../src/script/lua');

function run(src, { steps = 0, dt = 0.1, budget } = {}) {
  const out = []; const errors = []; const I = new Interp({ print: s => out.push(s), budget }); const S = new Scheduler(I, { onError: (t, e) => errors.push(e.value || e.message) });
  S.spawn(I.load(src, 'test'), [], 'main'); S.step(0); for (let i = 0; i < steps; i++) S.step(dt);
  return { out, errors, I, S };
}
const P = (src, o) => { const r = run(src, o); assert.deepEqual(r.errors, [], 'unexpected error: ' + r.errors); return r.out; };

test('arithmetic, precedence, concat, tostring of numbers', () => {
  assert.deepEqual(P('print(1+2*3, 2^3^2, -2^2, 7%3, -7%3, 7/2, 10-4-3, "a".."b".."c", 1 .. 2, 0x10, 1e2, 3.5)'), ['7\t512\t-4\t1\t2\t3.5\t3\tabc12\t16\t100\t3.5'.replace('abc12', 'abc\t12')]);
  assert.deepEqual(P('print(1/3, 100000000000000, 2^53, 1e100, 0.1+0.2, 1/0, -1/0)'), ['0.33333333333333\t100000000000000\t9.007199254741e+15\t1e+100\t0.3\tinf\t-inf']);
});
test('comparison, logic, truthiness, nil handling', () => {
  assert.deepEqual(P('print(1<2, "a"<"b", 1==1.0, "1"==1, nil==false, not nil, nil and 1, false or "x", 0 and "zero")'), ['true\ttrue\ttrue\tfalse\tfalse\ttrue\tnil\tx\tzero']);
});
test('locals, closures, upvalues shared between closures', () => {
  assert.deepEqual(P(`local function counter() local n=0 return function() n=n+1 return n end, function() return n end end
    local inc,get=counter() inc() inc() print(get()) local x=1 do local x=2 end print(x)`), ['2', '1']);
});
test('loops: while, repeat scope, numeric for with negative step, break, nested', () => {
  assert.deepEqual(P(`local s=0 for i=1,10 do if i%2==0 then s=s+i end end print(s)
    for i=3,1,-1 do io=i end print(io)
    local n=0 repeat local k=n n=n+1 until k>=2 print(n)
    local c=0 while true do c=c+1 if c>4 then break end end print(c)`), ['30', '1', '3', '5']);
});
test('tables, length, pairs order (array then hash), ipairs stops at nil, constructors', () => {
  assert.deepEqual(P(`local t={10,20,30,x=1,[5]=50} t[4]=40 print(#t, t[5]) local ks={} for k,v in pairs({1,2,3}) do ks[#ks+1]=k..":"..v end print(table.concat(ks,","))
    local c=0 for i,v in ipairs({1,2,nil,4}) do c=c+1 end print(c) local u={...} print(#u)`), ['5\t50', '1:1,2:2,3:3', '2', '0']);
});
test('multiple returns, varargs, select, unpack, adjust rules', () => {
  assert.deepEqual(P(`local function f() return 1,2,3 end local a,b,c,d=f() print(a,b,c,d) print((f())) print(f(),f())
    local function v(...) return select("#",...), ... end print(v(nil,nil)) print(select(2,"a","b","c")) print(unpack({1,2,3}))
    local t={f(),f()} print(#t)`), ['1\t2\t3\tnil', '1', '1\t1\t2\t3', '2\tnil\tnil', 'b\tc', '1\t2\t3', '4']);
});
test('methods, self, metatables (__index, __newindex, __call, __add, __tostring, __eq, __lt, __concat)', () => {
  assert.deepEqual(P(`local A={} A.__index=A function A.new(v) return setmetatable({v=v},A) end function A:get() return self.v end
    local o=A.new(7) print(o:get())
    A.__add=function(a,b) return A.new(a.v+b.v) end A.__tostring=function(a) return "A("..a.v..")" end print(tostring(o+o))
    A.__eq=function(a,b) return a.v==b.v end print(A.new(1)==A.new(1)) A.__lt=function(a,b) return a.v<b.v end print(A.new(1)<A.new(2))
    A.__call=function(self,x) return x*2 end print(o(21)) A.__concat=function(a,b) return "cat" end print(o.."x")
    local log={} local p=setmetatable({}, {__newindex=function(t,k,v) rawset(t,k,v*2) end}) p.a=4 print(p.a)`), ['7', 'A(14)', 'true', 'true', '42', 'cat', '8']);
});
test('pcall / error / assert / error values / runtime error messages with line numbers', () => {
  assert.deepEqual(P(`print(pcall(function() error("boom") end)) print(pcall(function() error({code=1}) end)) print(pcall(error)) print(select("#", pcall(function() return 1,2 end)))
    print(pcall(function() local x=nil; return x.y end)) print(pcall(function() return undefinedfn() end)) print(pcall(function() return 1+{} end)) print(pcall(function() return "a"<1 end))
    print(pcall(function() return "a".. nil end)) print(pcall(assert,false,"msg")) print(pcall(assert,1==1,"ok"))`).map(s => s.replace(/table: 0x[0-9a-f]+/, 'table')),
    ['false\ttest:1: boom', 'false\ttable', 'false\tnil', '3', "false\ttest:2: attempt to index local 'x' (a nil value)",
     "false\ttest:2: attempt to call global 'undefinedfn' (a nil value)", 'false\ttest:2: attempt to perform arithmetic on a table value', 'false\ttest:2: attempt to compare string with number',
     'false\ttest:3: attempt to concatenate a nil value', 'false\tmsg', 'true\ttrue\tok']);
});
test('string library: sub/len/upper/rep/byte/char/reverse/format/find/match/gmatch/gsub with patterns', () => {
  assert.deepEqual(P(`print(("hello"):sub(2,4), ("hello"):sub(-3), #"abc", ("Ab"):upper(), ("ab"):rep(3), ("a"):byte(), string.char(72,105), ("abc"):reverse())
    print(string.format("%d|%5.2f|%s|%05d|%x|%-4s|%%", 42, 3.14159, "hi", 7, 255, "a"))
    print(string.find("hello world","o w")) print(string.find("hello","l+")) print(string.match("key=value","(%w+)=(%w+)")) print(string.match("abc123","%d+")) print(string.find("a.b",".",1,true))
    for w in string.gmatch("one two  three","%a+") do io=(io or "")..w.."," end print(io)
    print(string.gsub("hello world","o","0")) print(string.gsub("abc","%w","%0%0")) print(string.gsub("$name is $age","%$(%w+)",{name="Bob",age=5}))
    print(string.gsub("abc","b",function(c) return c:upper() end)) print(string.match("  trim  ","^%s*(.-)%s*$")) print(string.match("f(a(b)c)d","%b()")) print(string.find("abc","b",-1))`),
    ['ell\tllo\t3\tAB\tababab\t97\tHi\tcba', '42| 3.14|hi|00007|ff|a   |%', '5\t7', '3\t4', 'key\tvalue', '123', '2\t2', 'one,two,three,', 'hell0 w0rld\t2', 'aabbcc\t3', 'Bob is 5\t2', 'aBc\t1', 'trim', '(a(b)c)', 'nil']);
});
test('table library: insert/remove/concat/sort with comparator', () => {
  assert.deepEqual(P(`local t={5,2,8,1} table.sort(t) print(table.concat(t,",")) table.sort(t,function(a,b) return a>b end) print(table.concat(t,","))
    table.insert(t,9) table.insert(t,1,0) print(table.concat(t,",")) print(table.remove(t), table.remove(t,1), #t) print(table.getn(t))`), ['1,2,5,8', '8,5,2,1', '0,8,5,2,1,9', '9\t0\t4', '4']);
});
test('math library incl. deterministic random', () => {
  assert.deepEqual(P('print(math.floor(3.7), math.ceil(3.2), math.max(1,5,3), math.min(4,2), math.abs(-3), math.sqrt(16), math.fmod(7,3), math.huge)'), ['3\t4\t5\t2\t3\t4\t1\tinf']);
  const a = P('math.randomseed(42) print(math.random(1,100), math.random(1,100), math.random(5))'); const b = P('math.randomseed(42) print(math.random(1,100), math.random(1,100), math.random(5))'); assert.deepEqual(a, b);
  assert.ok(P('for i=1,200 do local r=math.random() assert(r>=0 and r<1) local n=math.random(3,5) assert(n>=3 and n<=5) end print("ok")')[0] === 'ok');
});
test('wait() is cooperative, virtual-time scheduled, and interleaves threads deterministically', () => {
  const r = run(`spawn(function() for i=1,3 do print("A"..i) wait(1) end end) spawn(function() wait(0.5) for i=1,3 do print("B"..i) wait(1) end end) delay(2.2, function() print("D") end)`, { steps: 40, dt: 0.1 });
  assert.deepEqual(r.out, ['A1', 'B1', 'A2', 'B2', 'A3', 'D', 'B3'].sort((x, y) => 0) && ['A1', 'B1', 'A2', 'B2', 'A3', 'D', 'B3']);
  assert.equal(r.S.alive, 0);
});
test('coroutines: create/resume/yield/status/wrap, wait inside coroutine', () => {
  assert.deepEqual(P(`local co=coroutine.create(function(a,b) local c=coroutine.yield(a+b) local d,e=coroutine.yield(c*2) return d+e end)
    print(coroutine.resume(co,1,2)) print(coroutine.status(co)) print(coroutine.resume(co,10)) print(coroutine.resume(co,3,4)) print(coroutine.status(co)) print(coroutine.resume(co))
    local g=coroutine.wrap(function() for i=1,3 do coroutine.yield(i) end end) print(g(),g(),g())`), ['true\t3', 'suspended', 'true\t20', 'true\t7', 'dead', 'false\tcannot resume dead coroutine', '1\t2\t3']);
  const r = run(`local co=coroutine.wrap(function() print("in") wait(1) print("after") end) co() print("out")`, { steps: 15 }); assert.deepEqual(r.out, ['in', 'out', 'after']); // Roblox behaviour: wait() inside a coroutine does not block the resumer
});
test('goto-less syntax edge cases: comments, long strings, escapes, semicolons, method on string literal, call sugar', () => {
  assert.deepEqual(P(`--[[ multi
line ]] print("a\\tb\\65\\"q\\"") --[==[ x ]==] print([[long
string]]) ; print(#"\\0ab") local t={f=function(s) return s end} print(t.f"sugar", t.f{1}[1]) print(("x"):rep(2)) print(type(print), type(nil), type({}), type("s"), type(2), type(true), type(function()end))`),
    ['a\tbA"q"', 'long\nstring', '3', 'sugar\t1', 'xx', 'function\tnil\ttable\tstring\tnumber\tboolean\tfunction']);
});
test('syntax errors report line numbers and do not execute', () => {
  assert.throws(() => run('print("never")\nlocal x = = 2'), e => /test:2: unexpected symbol/.test(e.value));
  assert.throws(() => new Interp().load('x = = 1', 'chunk'), e => /chunk:1: unexpected symbol/.test(e.value));
  assert.throws(() => new Interp().load('for i=1 do end', 'c'), e => /c:1:/.test(e.value));
  assert.throws(() => new Interp().load('print("a', 'c'), e => /unfinished string/.test(e.value));
});
test('SANDBOX: no dangerous globals exist', () => {
  const r = P('for _,n in ipairs({"io","require","loadstring","load","dofile","loadfile","getfenv","setfenv","debug","package","collectgarbage","module","newproxy","os_execute"}) do if _G[n]~=nil then print("LEAK "..n) end end print(type(os), os.execute, os.exit, os.remove, os.getenv, os.rename, os.tmpname)');
  assert.deepEqual(r, ['table\tnil\tnil\tnil\tnil\tnil\tnil']);
});
test('SANDBOX: runaway loops are aborted by the step budget and do not hang or kill other threads', () => {
  const out = []; const I = new Interp({ print: s => out.push(s), budget: 50_000 }); const errs = []; const S = new Scheduler(I, { onError: (t, e) => errs.push(e) });
  S.spawn(I.load('while true do end', 'spin'), [], 'spin'); S.spawn(I.load('print("other ok")', 'ok'), [], 'ok'); S.step(0.1);
  assert.equal(errs.length, 1); assert.ok(errs[0] instanceof BudgetError); assert.deepEqual(out, ['other ok']);
  // a pcall must not be able to swallow the budget error
  const I2 = new Interp({ budget: 20_000 }); const E2 = []; const S2 = new Scheduler(I2, { onError: (t, e) => E2.push(e) }); S2.spawn(I2.load('pcall(function() while true do end end) print("escaped")', 'p'), [], 'p'); S2.step(0.1); assert.ok(E2[0] instanceof BudgetError);
  // huge allocations are refused
  assert.deepEqual(run('print(pcall(string.rep, "x", 1e9))').out[0].startsWith('false'), true);
});
test('SANDBOX: a script error is contained and reported; scheduler keeps running other threads', () => {
  const r = run('spawn(function() error("x") end) spawn(function() wait(1) print("alive") end)', { steps: 15 }); assert.deepEqual(r.out, ['alive']); assert.equal(r.errors.length, 1);
});
test('Roblox-style 2007 idioms: lowercase methods on engine objects are the engine layer\'s job; here only global wait/spawn/delay/tick exist', () => {
  assert.deepEqual(P('print(type(wait), type(spawn), type(delay), type(tick), type(Spawn), type(Delay), type(Wait))'), ['function\tfunction\tfunction\tfunction\tfunction\tfunction\tfunction']);
});
