'use strict';
// A small, SANDBOXED Lua 5.1 interpreter for running preserved place scripts. Pure JS, no deps.
// STATUS: UNIT-TESTED only (tests/lua.test.js). Semantics follow Lua 5.1 as used by 2007-2015 Roblox, but this is NOT Roblox's Lua (Roblox used a modified 5.1 VM);
// differences are UNKNOWN beyond what tests show.
// SAFETY MODEL: scripts only see what the host puts in the global table. There is NO io/os.execute/require/loadstring/getfenv/setfenv/dofile/debug here.
// Every resume slice has a step budget (runaway loops abort the script, not the server). Scripts are cooperative generators scheduled by `Scheduler`.

class LuaError extends Error { constructor(value, traceback) { super(typeof value === 'string' ? value : 'lua error'); this.value = value; this.lua = true; this.traceback = traceback; } }
class BudgetError extends Error { constructor() { super('script exceeded its execution budget'); this.budget = true; } }

// ---------------------------------------------------------------- lexer
const KEYWORDS = new Set(['and', 'break', 'do', 'else', 'elseif', 'end', 'false', 'for', 'function', 'if', 'in', 'local', 'nil', 'not', 'or', 'repeat', 'return', 'then', 'true', 'until', 'while']);
function lex(src, chunk) {
  const toks = []; let i = 0, line = 1; const n = src.length;
  const err = m => { throw new LuaError(`${chunk}:${line}: ${m}`); };
  const longBracket = () => { // at '[', returns level or -1
    let j = i + 1, lvl = 0; while (src[j] === '=') { lvl++; j++; } return src[j] === '[' ? lvl : -1;
  };
  const readLong = lvl => {
    i += lvl + 2; if (src[i] === '\r') i++; if (src[i] === '\n') { line++; i++; }
    const close = ']' + '='.repeat(lvl) + ']'; const end = src.indexOf(close, i); if (end < 0) err('unfinished long string');
    const s = src.slice(i, end); for (const ch of s) if (ch === '\n') line++; i = end + close.length; return s;
  };
  while (i < n) {
    const c = src[i];
    if (c === '\n') { line++; i++; continue; }
    if (c === ' ' || c === '\t' || c === '\r' || c === '\f' || c === '\v') { i++; continue; }
    if (c === '-' && src[i + 1] === '-') {
      i += 2; if (src[i] === '[') { const l = longBracket(); if (l >= 0) { readLong(l); continue; } }
      while (i < n && src[i] !== '\n') i++; continue;
    }
    if (/[A-Za-z_]/.test(c)) { let j = i + 1; while (j < n && /[A-Za-z0-9_]/.test(src[j])) j++; const w = src.slice(i, j); toks.push({ t: KEYWORDS.has(w) ? w : 'name', v: w, line }); i = j; continue; }
    if (/[0-9]/.test(c) || (c === '.' && /[0-9]/.test(src[i + 1] || ''))) {
      let j = i; if (c === '0' && /[xX]/.test(src[i + 1] || '')) { j += 2; while (/[0-9a-fA-F]/.test(src[j] || '')) j++; toks.push({ t: 'number', v: parseInt(src.slice(i + 2, j), 16), line }); i = j; continue; }
      while (/[0-9]/.test(src[j] || '')) j++; if (src[j] === '.') { j++; while (/[0-9]/.test(src[j] || '')) j++; }
      if (/[eE]/.test(src[j] || '')) { let k = j + 1; if (/[+-]/.test(src[k] || '')) k++; if (/[0-9]/.test(src[k] || '')) { while (/[0-9]/.test(src[k] || '')) k++; j = k; } }
      const v = Number(src.slice(i, j)); if (Number.isNaN(v)) err('malformed number'); toks.push({ t: 'number', v, line }); i = j; continue;
    }
    if (c === '"' || c === "'") {
      let j = i + 1, out = '';
      for (;;) {
        if (j >= n || src[j] === '\n') err('unfinished string'); const d = src[j];
        if (d === c) { j++; break; }
        if (d === '\\') {
          j++; const e = src[j];
          const map = { n: '\n', t: '\t', r: '\r', a: '\x07', b: '\b', f: '\f', v: '\v', '\\': '\\', '"': '"', "'": "'", '\n': '\n' };
          if (e in map) { out += map[e]; if (e === '\n') line++; j++; }
          else if (/[0-9]/.test(e)) { let k = j, num = ''; while (k < j + 3 && /[0-9]/.test(src[k] || '')) num += src[k++]; const code = +num; if (code > 255) err('escape sequence too large'); out += String.fromCharCode(code); j = k; }
          else { out += e; j++; }
        } else { out += d; j++; }
      }
      toks.push({ t: 'string', v: out, line }); i = j; continue;
    }
    if (c === '[') { const l = longBracket(); if (l >= 0) { const l0 = line; const s = readLong(l); toks.push({ t: 'string', v: s, line: l0 }); continue; } }
    const three = src.substr(i, 3), two = src.substr(i, 2);
    if (three === '...') { toks.push({ t: '...', line }); i += 3; continue; }
    if (['==', '~=', '<=', '>=', '..'].includes(two)) { toks.push({ t: two, line }); i += 2; continue; }
    if ('+-*/%^#<>=(){}[];:,.'.includes(c)) { toks.push({ t: c, line }); i++; continue; }
    err(`unexpected symbol near '${c}'`);
  }
  toks.push({ t: 'eof', line }); return toks;
}

// ---------------------------------------------------------------- parser (AST)
function parse(src, chunk = '?') {
  const toks = lex(src, chunk); let p = 0;
  const peek = () => toks[p], next = () => toks[p++]; const err = (m, tk = peek()) => { throw new LuaError(`${chunk}:${tk.line}: ${m} near '${tk.v !== undefined ? tk.v : tk.t}'`); };
  const check = t => peek().t === t; const accept = t => { if (check(t)) { p++; return true; } return false; };
  const expect = (t, what) => { if (!check(t)) err(`'${what || t}' expected`); return next(); };
  const scopes = []; const isLocal = n => scopes.some(sc => sc.has(n)); const declare = n => { scopes[scopes.length - 1].add(n); };
  const isBlockEnd = () => ['eof', 'end', 'else', 'elseif', 'until'].includes(peek().t);
  function block(names = [], keep = false) { scopes.push(new Set(names)); const body = blockBody(); if (!keep) scopes.pop(); return body; }
  function blockBody() { const body = []; while (!isBlockEnd()) { if (check('return')) { const line = next().line; const vals = (isBlockEnd() || check(';')) ? [] : exprList(); accept(';'); body.push({ k: 'return', vals, line }); break; } if (check('break')) { body.push({ k: 'break', line: next().line }); accept(';'); break; } const s = statement(); if (s) body.push(s); accept(';'); } return body; }
  function statement() {
    const tk = peek(), line = tk.line;
    switch (tk.t) {
      case 'do': { next(); const b = block(); expect('end'); return { k: 'do', body: b, line }; }
      case 'while': { next(); const cond = expr(); expect('do'); const b = block(); expect('end'); return { k: 'while', cond, body: b, line }; }
      case 'repeat': { next(); const b = block([], true); expect('until'); const cond = expr(); scopes.pop(); return { k: 'repeat', body: b, cond, line }; }
      case 'if': { next(); const clauses = []; let cond = expr(); expect('then'); clauses.push({ cond, body: block() }); let orelse = null; for (;;) { if (accept('elseif')) { cond = expr(); expect('then'); clauses.push({ cond, body: block() }); } else if (accept('else')) { orelse = block(); expect('end'); break; } else { expect('end'); break; } } return { k: 'if', clauses, orelse, line }; }
      case 'for': {
        next(); const n1 = expect('name').v;
        if (check('=')) { next(); const a = expr(); expect(','); const b = expr(); const c = accept(',') ? expr() : null; expect('do'); const body = block([n1]); expect('end'); return { k: 'fornum', v: n1, a, b, c, body, line }; }
        const names = [n1]; while (accept(',')) names.push(expect('name').v); expect('in'); const exprs = exprList(); expect('do'); const body = block(names); expect('end'); return { k: 'forin', names, exprs, body, line };
      }
      case 'function': { next(); let target = { k: 'name', v: expect('name').v, line }; let isMethod = false; while (check('.') || check(':')) { const colon = next().t === ':'; const key = expect('name').v; target = { k: 'index', obj: target, key: { k: 'string', v: key }, line }; if (colon) { isMethod = true; break; } } const f = funcBody(isMethod, line, target.k === 'name' ? target.v : target.key.v); return { k: 'assign', targets: [target], vals: [f], line }; }
      case 'local': {
        next(); if (accept('function')) { const name = expect('name').v; declare(name); const f = funcBody(false, line, name); return { k: 'localfunc', name, f, line }; }
        const names = [expect('name').v]; while (accept(',')) names.push(expect('name').v); const vals = accept('=') ? exprList() : []; names.forEach(declare); return { k: 'local', names, vals, line };
      }
      default: {
        const e = suffixedExpr(); if (check('=') || check(',')) { const targets = [e]; while (accept(',')) targets.push(suffixedExpr()); expect('='); const vals = exprList(); for (const t of targets) if (t.k !== 'name' && t.k !== 'index') err('syntax error'); return { k: 'assign', targets, vals, line }; }
        if (e.k !== 'call' && e.k !== 'mcall') err('syntax error'); return { k: 'exprstat', e, line };
      }
    }
  }
  function funcBody(isMethod, line, name) { expect('('); const params = isMethod ? ['self'] : []; let vararg = false; if (!check(')')) { do { if (check('...')) { next(); vararg = true; break; } params.push(expect('name').v); } while (accept(',')); } expect(')'); const body = block(params); expect('end'); return { k: 'function', params, vararg, body, line, name }; }
  function exprList() { const l = [expr()]; while (accept(',')) l.push(expr()); return l; }
  function primary() {
    const tk = peek();
    if (tk.t === 'name') { next(); return { k: 'name', v: tk.v, line: tk.line, local: isLocal(tk.v) }; }
    if (tk.t === '(') { next(); const e = expr(); expect(')'); return { k: 'paren', e, line: tk.line }; }
    err('unexpected symbol');
  }
  function suffixedExpr() {
    let e = primary();
    for (;;) {
      const tk = peek();
      if (tk.t === '.') { next(); e = { k: 'index', obj: e, key: { k: 'string', v: expect('name').v }, line: tk.line }; }
      else if (tk.t === '[') { next(); const key = expr(); expect(']'); e = { k: 'index', obj: e, key, line: tk.line }; }
      else if (tk.t === ':') { next(); const name = expect('name').v; e = { k: 'mcall', obj: e, name, args: callArgs(), line: tk.line }; }
      else if (tk.t === '(' || tk.t === 'string' || tk.t === '{') e = { k: 'call', f: e, args: callArgs(), line: tk.line };
      else return e;
    }
  }
  function callArgs() { const tk = peek(); if (tk.t === 'string') { next(); return [{ k: 'string', v: tk.v }]; } if (tk.t === '{') return [tableCons()]; expect('('); if (accept(')')) return []; const a = exprList(); expect(')'); return a; }
  function tableCons() {
    const line = expect('{').line; const arr = [], hash = [], order = [];
    while (!check('}')) {
      if (check('[')) { next(); const key = expr(); expect(']'); expect('='); hash.push([key, expr()]); order.push('h'); }
      else if (check('name') && toks[p + 1].t === '=') { const key = { k: 'string', v: next().v }; next(); hash.push([key, expr()]); order.push('h'); }
      else { arr.push(expr()); order.push('a'); }
      if (!accept(',') && !accept(';')) break;
    }
    expect('}'); return { k: 'table', arr, hash, line };
  }
  function simple() {
    const tk = peek();
    switch (tk.t) {
      case 'number': next(); return { k: 'number', v: tk.v };
      case 'string': next(); return { k: 'string', v: tk.v };
      case 'nil': next(); return { k: 'nil' }; case 'true': next(); return { k: 'true' }; case 'false': next(); return { k: 'false' };
      case '...': next(); return { k: 'vararg', line: tk.line };
      case 'function': { next(); return funcBody(false, tk.line, '?'); }
      case '{': return tableCons();
      default: return suffixedExpr();
    }
  }
  const BIN = { or: [1, 1], and: [2, 2], '<': [3, 3], '>': [3, 3], '<=': [3, 3], '>=': [3, 3], '~=': [3, 3], '==': [3, 3], '..': [5, 4], '+': [6, 6], '-': [6, 6], '*': [7, 7], '/': [7, 7], '%': [7, 7], '^': [10, 9] };
  function expr(limit = 0) {
    let left; const tk = peek();
    if (tk.t === 'not' || tk.t === '-' || tk.t === '#') { next(); const operand = expr(8); left = { k: 'unop', op: tk.t, e: operand, line: tk.line }; } else left = simple();
    for (;;) { const op = peek().t; const pr = BIN[op]; if (!pr || pr[0] <= limit) break; const line = next().line; const right = expr(pr[1]); left = { k: 'binop', op, l: left, r: right, line }; }
    return left;
  }
  scopes.push(new Set()); const body = blockBody(); scopes.pop(); if (!check('eof')) err("'<eof>' expected"); return { k: 'chunk', body, chunk };
}

// ---------------------------------------------------------------- values
class LuaTable {
  constructor() { this.arr = []; this.hash = new Map(); this.meta = null; }
  get(k) { if (typeof k === 'number' && Number.isInteger(k) && k >= 1 && k <= this.arr.length) return this.arr[k - 1]; return this.hash.get(k); }
  set(k, v) {
    if (typeof k === 'number' && Number.isInteger(k) && k >= 1) {
      if (k <= this.arr.length) { this.arr[k - 1] = v; if (v === undefined && k === this.arr.length) { while (this.arr.length && this.arr[this.arr.length - 1] === undefined) this.arr.pop(); } return; }
      if (k === this.arr.length + 1) { if (v === undefined) { this.hash.delete(k); return; } this.arr.push(v); this.hash.delete(k); while (this.hash.has(this.arr.length + 1)) { const nk = this.arr.length + 1; this.arr.push(this.hash.get(nk)); this.hash.delete(nk); } return; }
    }
    if (v === undefined) this.hash.delete(k); else this.hash.set(k, v);
  }
  length() { return this.arr.length; }
  next(k) { // returns [key,value] after k or undefined
    const keys = [...Array(this.arr.length).keys()].map(i => i + 1); let idx; const hk = [...this.hash.keys()];
    if (k === undefined) idx = -1; else if (typeof k === 'number' && k >= 1 && k <= this.arr.length && Number.isInteger(k)) idx = k - 1; else { const h = hk.indexOf(k); if (h < 0) throw new LuaError("invalid key to 'next'"); idx = this.arr.length + h; }
    const all = keys.concat(hk); for (let j = idx + 1; j < all.length; j++) { const key = all[j]; const v = this.get(key); if (v !== undefined) return [key, v]; } return undefined;
  }
}
class LuaFunction { constructor(node, scope, interp) { this.node = node; this.scope = scope; this.interp = interp; } }
class Scope { constructor(parent) { this.vars = new Map(); this.parent = parent; } find(n) { for (let s = this; s; s = s.parent) if (s.vars.has(n)) return s; return null; } }

const typeOf = v => v === undefined || v === null ? 'nil' : typeof v === 'boolean' ? 'boolean' : typeof v === 'number' ? 'number' : typeof v === 'string' ? 'string' : v instanceof LuaTable ? 'table' : (v instanceof LuaFunction || typeof v === 'function') ? 'function' : v && v.luaThread ? 'thread' : 'userdata';
function fmtNum(n) { if (Number.isInteger(n) && Math.abs(n) < 1e15) return String(n); if (n !== n) return 'nan'; if (n === Infinity) return 'inf'; if (n === -Infinity) return '-inf'; let s = n.toPrecision(14); if (s.includes('e')) { let [m, e] = s.split('e'); if (m.includes('.')) m = m.replace(/0+$/, '').replace(/\.$/, ''); const sign = e[0] === '-' ? '-' : '+'; e = e.replace(/^[+-]/, ''); return `${m}e${sign}${e.padStart(2, '0')}`; } if (s.includes('.')) s = s.replace(/0+$/, '').replace(/\.$/, ''); return s; }
function toNumber(v) { if (typeof v === 'number') return v; if (typeof v === 'string') { const t = v.trim(); if (t === '') return undefined; if (/^0[xX][0-9a-fA-F]+$/.test(t)) return parseInt(t, 16); if (/^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$/.test(t)) return Number(t); } return undefined; }
const truthy = v => v !== undefined && v !== null && v !== false;

// ---------------------------------------------------------------- interpreter
class Interp {
  constructor({ budget = 2_000_000, print = () => {} } = {}) { this.globals = new LuaTable(); this.budget = budget; this.steps = 0; this.print = print; this.stringMeta = new LuaTable(); this.current = null; installStdlib(this); }
  tick() { if (++this.steps > this.budget) throw new BudgetError(); }
  err(msg, line, chunk) { return new LuaError(`${chunk || this.chunk || '?'}:${line || 0}: ${msg}`); }
  tostr(v) { if (v === undefined || v === null) return 'nil'; if (typeof v === 'string') return v; if (typeof v === 'number') return fmtNum(v); if (typeof v === 'boolean') return String(v); const mt = this.getmeta(v); if (mt) { const f = mt.get('__tostring'); if (f) { const r = this.callSync(f, [v]); return this.tostr(r[0]); } } if (v instanceof LuaTable) return 'table: 0x' + (v.id || (v.id = Math.random().toString(16).slice(2, 10))); if (v instanceof LuaFunction || typeof v === 'function') return 'function: 0x' + (v.id || (v.id = Math.random().toString(16).slice(2, 10))); if (v && v.luaToString) return v.luaToString(); return 'userdata'; }
  getmeta(v) { if (v instanceof LuaTable) return v.meta; if (typeof v === 'string') return this.stringMeta; return v && v.luaMeta ? v.luaMeta : null; }
  // -- indexing with metamethods
  *index(o, k, line, chunk, desc) {
    for (let depth = 0; depth < 100; depth++) {
      if (o instanceof LuaTable) { const v = o.get(k); if (v !== undefined) return v; const mt = o.meta; const h = mt ? mt.get('__index') : undefined; if (h === undefined) return undefined; if (typeof h === 'function' || h instanceof LuaFunction) { const r = yield* this.call(h, [o, k]); return r[0]; } o = h; continue; }
      if (typeof o === 'string') { return this.stringMeta.get('__index').get(k); }
      if (o && typeof o.luaGet === 'function') { const r = o.luaGet(k, this); if (r && typeof r.next === 'function') return yield* r; return r; }
      throw this.err(`attempt to index ${desc ? desc + ' (a ' + typeOf(o) + ' value)' : 'a ' + typeOf(o) + ' value'}`, line, chunk);
    }
    throw this.err("loop in gettable", line, chunk);
  }
  *setindex(o, k, v, line, chunk, desc) {
    for (let depth = 0; depth < 100; depth++) {
      if (o instanceof LuaTable) { const mt = o.meta; const h = mt ? mt.get('__newindex') : undefined; if (h === undefined || o.get(k) !== undefined) { if (k === undefined) throw this.err('table index is nil', line, chunk); if (typeof k === 'number' && k !== k) throw this.err('table index is NaN', line, chunk); o.set(k, v); return; } if (typeof h === 'function' || h instanceof LuaFunction) { yield* this.call(h, [o, k, v]); return; } o = h; continue; }
      if (o && typeof o.luaSet === 'function') { const r = o.luaSet(k, v, this); if (r && typeof r.next === 'function') yield* r; return; }
      throw this.err(`attempt to index ${desc ? desc + ' (a ' + typeOf(o) + ' value)' : 'a ' + typeOf(o) + ' value'}`, line, chunk);
    }
  }
  // -- calling
  *call(f, args, line, chunk, desc) {
    if (f instanceof LuaFunction) return yield* this.callLua(f, args);
    if (typeof f === 'function') { const r = f(args, this); if (r && typeof r.next === 'function') return yield* r; return r || []; }
    const mt = this.getmeta(f); const h = mt ? mt.get('__call') : undefined; if (h !== undefined) return yield* this.call(h, [f, ...args], line, chunk);
    throw this.err(`attempt to call ${desc ? desc + ' (a ' + typeOf(f) + ' value)' : 'a ' + typeOf(f) + ' value'}`, line, chunk);
  }
  callSync(f, args) { const g = this.call(f, args); let r = g.next(); while (!r.done) r = g.next(); return r.value; }
  *callLua(fn, args) {
    const node = fn.node; const scope = new Scope(fn.scope); const chunk = node.chunk || fn.chunk;
    for (let i = 0; i < node.params.length; i++) scope.vars.set(node.params[i], { v: args[i] });
    const va = node.vararg ? args.slice(node.params.length) : null; const ctx = { va, chunk: fn.chunk, fname: node.name, env: fn.env };
    const r = yield* this.block(node.body, scope, ctx); if (r && r.ret) return r.ret; return [];
  }
  *block(stmts, scope, ctx) { for (const s of stmts) { const r = yield* this.stmt(s, scope, ctx); if (r) return r; } return undefined; }
  *stmt(s, scope, ctx) {
    this.tick(); this.curLine = s.line; this.curChunk = ctx.chunk;
    switch (s.k) {
      case 'local': { const vals = yield* this.evalList(s.vals, scope, ctx); for (let i = 0; i < s.names.length; i++) scope.vars.set(s.names[i], { v: vals[i] }); return; }
      case 'localfunc': { const cell = { v: undefined }; scope.vars.set(s.name, cell); cell.v = new LuaFunction(s.f, scope, this); cell.v.chunk = ctx.chunk; cell.v.env = ctx.env; return; }
      case 'assign': {
        // evaluate all RHS first (Lua evaluates expressions before assignment)
        const vals = yield* this.evalList(s.vals, scope, ctx);
        const refs = []; for (const t of s.targets) { if (t.k === 'index') refs.push([yield* this.eval(t.obj, scope, ctx), yield* this.eval(t.key, scope, ctx), t]); else refs.push([null, null, t]); }
        for (let i = 0; i < s.targets.length; i++) { const [o, k, t] = refs[i]; const v = vals[i]; if (t.k === 'name') this.assignName(t.v, v, scope, ctx.env); else yield* this.setindex(o, k, v, t.line, ctx.chunk, describe(t.obj)); }
        return;
      }
      case 'exprstat': yield* this.eval(s.e, scope, ctx, true); return;
      case 'do': return yield* this.block(s.body, new Scope(scope), ctx);
      case 'if': { for (const c of s.clauses) if (truthy(yield* this.eval(c.cond, scope, ctx))) return yield* this.block(c.body, new Scope(scope), ctx); if (s.orelse) return yield* this.block(s.orelse, new Scope(scope), ctx); return; }
      case 'while': { while (truthy(yield* this.eval(s.cond, scope, ctx))) { this.tick(); const r = yield* this.block(s.body, new Scope(scope), ctx); if (r) { if (r.brk) break; return r; } } return; }
      case 'repeat': { for (;;) { this.tick(); const inner = new Scope(scope); const r = yield* this.block(s.body, inner, ctx); if (r) { if (r.brk) break; return r; } if (truthy(yield* this.eval(s.cond, inner, ctx))) break; } return; }
      case 'fornum': {
        const a = toNumber(yield* this.eval(s.a, scope, ctx)), b = toNumber(yield* this.eval(s.b, scope, ctx)), c = s.c ? toNumber(yield* this.eval(s.c, scope, ctx)) : 1;
        if (a === undefined) throw this.err("'for' initial value must be a number", s.line, ctx.chunk); if (b === undefined) throw this.err("'for' limit must be a number", s.line, ctx.chunk); if (c === undefined) throw this.err("'for' step must be a number", s.line, ctx.chunk);
        if (c === 0) throw this.err("'for' step is zero", s.line, ctx.chunk);
        for (let i = a; c > 0 ? i <= b : i >= b; i += c) { this.tick(); const inner = new Scope(scope); inner.vars.set(s.v, { v: i }); const r = yield* this.block(s.body, inner, ctx); if (r) { if (r.brk) break; return r; } }
        return;
      }
      case 'forin': {
        const init = yield* this.evalList(s.exprs, scope, ctx); const f = init[0]; const st = init[1]; let ctl = init[2];
        for (;;) { this.tick(); const rs = yield* this.call(f, [st, ctl], s.line, ctx.chunk, 'for iterator'); if (rs[0] === undefined) break; ctl = rs[0]; const inner = new Scope(scope); s.names.forEach((nm, i) => inner.vars.set(nm, { v: rs[i] })); const r = yield* this.block(s.body, inner, ctx); if (r) { if (r.brk) break; return r; } }
        return;
      }
      case 'return': { const vals = yield* this.evalList(s.vals, scope, ctx, true); return { ret: vals }; }
      case 'break': return { brk: true };
      default: throw new Error('unknown stmt ' + s.k);
    }
  }
  assignName(name, v, scope, env) { const sc = scope.find(name); if (sc) sc.vars.get(name).v = v; else (env || this.globals).set(name, v); }
  *evalList(exprs, scope, ctx) {
    const out = []; for (let i = 0; i < exprs.length; i++) { const e = exprs[i]; if (i === exprs.length - 1 && (e.k === 'call' || e.k === 'mcall' || e.k === 'vararg')) { const r = yield* this.evalMulti(e, scope, ctx); for (const x of r) out.push(x); } else out.push(yield* this.eval(e, scope, ctx)); }
    return out;
  }
  *evalMulti(e, scope, ctx) {
    if (e.k === 'vararg') return ctx.va ? ctx.va.slice() : [];
    if (e.k === 'call') { const f = yield* this.eval(e.f, scope, ctx); const args = yield* this.evalList(e.args, scope, ctx); return yield* this.call(f, args, e.line, ctx.chunk, describe(e.f)); }
    if (e.k === 'mcall') { const o = yield* this.eval(e.obj, scope, ctx); const f = yield* this.index(o, e.name, e.line, ctx.chunk, describe(e.obj)); const args = yield* this.evalList(e.args, scope, ctx); return yield* this.call(f, [o, ...args], e.line, ctx.chunk, `method '${e.name}'`); }
    return [yield* this.eval(e, scope, ctx)];
  }
  *eval(e, scope, ctx, stmt) {
    switch (e.k) {
      case 'nil': return undefined; case 'true': return true; case 'false': return false; case 'number': case 'string': return e.v;
      case 'vararg': return ctx.va ? ctx.va[0] : undefined;
      case 'name': { const sc = scope.find(e.v); if (sc) return sc.vars.get(e.v).v; const env = ctx.env; const gv = env.get(e.v); return gv === undefined && env.base ? env.base.get(e.v) : gv; }
      case 'paren': return yield* this.eval(e.e, scope, ctx);
      case 'index': { const o = yield* this.eval(e.obj, scope, ctx); const k = yield* this.eval(e.key, scope, ctx); return yield* this.index(o, k, e.line, ctx.chunk, describe(e.obj)); }
      case 'call': case 'mcall': { const r = yield* this.evalMulti(e, scope, ctx); return r[0]; }
      case 'function': { const f = new LuaFunction(e, scope, this); f.chunk = ctx.chunk; f.env = ctx.env; return f; }
      case 'table': { const t = new LuaTable(); let n = 1; const total = e.arr.length;
        for (let i = 0; i < e.arr.length; i++) { const x = e.arr[i]; if (i === total - 1 && (x.k === 'call' || x.k === 'mcall' || x.k === 'vararg')) { for (const v of yield* this.evalMulti(x, scope, ctx)) t.set(n++, v); } else t.set(n++, yield* this.eval(x, scope, ctx)); }
        for (const [kx, vx] of e.hash) { const k = yield* this.eval(kx, scope, ctx); if (k === undefined) throw this.err('table index is nil', e.line, ctx.chunk); t.set(k, yield* this.eval(vx, scope, ctx)); }
        return t; }
      case 'unop': { const v = yield* this.eval(e.e, scope, ctx); return yield* this.unop(e.op, v, e, ctx); }
      case 'binop': {
        if (e.op === 'and') { const l = yield* this.eval(e.l, scope, ctx); return truthy(l) ? yield* this.eval(e.r, scope, ctx) : l; }
        if (e.op === 'or') { const l = yield* this.eval(e.l, scope, ctx); return truthy(l) ? l : yield* this.eval(e.r, scope, ctx); }
        const l = yield* this.eval(e.l, scope, ctx); const r = yield* this.eval(e.r, scope, ctx); return yield* this.binop(e.op, l, r, e, ctx);
      }
      default: throw new Error('unknown expr ' + e.k);
    }
  }
  *unop(op, v, e, ctx) {
    if (op === 'not') return !truthy(v);
    if (op === '-') { const n = toNumber(v); if (n !== undefined && typeof v !== 'boolean') return -n; const mt = this.getmeta(v); const h = mt && mt.get('__unm'); if (h) return (yield* this.call(h, [v, v]))[0]; throw this.err(`attempt to perform arithmetic on ${describe(e.e) ? describe(e.e) + ' (a ' + typeOf(v) + ' value)' : 'a ' + typeOf(v) + ' value'}`, e.line, ctx.chunk); }
    if (op === '#') { if (typeof v === 'string') return v.length; if (v instanceof LuaTable) return v.length(); if (v && typeof v.luaLen === 'function') return v.luaLen(); throw this.err(`attempt to get length of a ${typeOf(v)} value`, e.line, ctx.chunk); }
  }
  *binop(op, l, r, e, ctx) {
    switch (op) {
      case '+': case '-': case '*': case '/': case '%': case '^': {
        const a = typeof l === 'number' ? l : (typeof l === 'string' ? toNumber(l) : undefined), b = typeof r === 'number' ? r : (typeof r === 'string' ? toNumber(r) : undefined);
        if (a === undefined || b === undefined) { const key = { '+': '__add', '-': '__sub', '*': '__mul', '/': '__div', '%': '__mod', '^': '__pow' }[op]; const h = (this.getmeta(l) && this.getmeta(l).get(key)) || (this.getmeta(r) && this.getmeta(r).get(key)); if (h) return (yield* this.call(h, [l, r]))[0]; const bad = a === undefined ? e.l : e.r; const val = a === undefined ? l : r; throw this.err(`attempt to perform arithmetic on ${describe(bad) ? describe(bad) + ' (a ' + typeOf(val) + ' value)' : 'a ' + typeOf(val) + ' value'}`, e.line, ctx.chunk); }
        switch (op) { case '+': return a + b; case '-': return a - b; case '*': return a * b; case '/': return a / b; case '%': return b === 0 ? NaN : a - Math.floor(a / b) * b; case '^': return Math.pow(a, b); }
        break;
      }
      case '..': { if ((typeof l === 'string' || typeof l === 'number') && (typeof r === 'string' || typeof r === 'number')) return this.tostr(l) + this.tostr(r); const h = (this.getmeta(l) && this.getmeta(l).get('__concat')) || (this.getmeta(r) && this.getmeta(r).get('__concat')); if (h) return (yield* this.call(h, [l, r]))[0]; const bad = (typeof l === 'string' || typeof l === 'number') ? e.r : e.l; const val = (typeof l === 'string' || typeof l === 'number') ? r : l; throw this.err(`attempt to concatenate ${describe(bad) ? describe(bad) + ' (a ' + typeOf(val) + ' value)' : 'a ' + typeOf(val) + ' value'}`, e.line, ctx.chunk); }
      case '==': return l === r || (l instanceof LuaTable && r instanceof LuaTable && l.meta && l.meta.get('__eq') && truthy((yield* this.call(l.meta.get('__eq'), [l, r]))[0])) || (l !== undefined && l !== null && r !== undefined && r !== null && typeof l === 'object' && typeof r === 'object' && typeof l.luaEq === 'function' && l.luaEq(r));
      case '~=': return !(yield* this.binop('==', l, r, e, ctx));
      case '<': case '>': case '<=': case '>=': {
        let a = l, b = r; if (op === '>' || op === '>=') { a = r; b = l; } const lt = op === '<' || op === '>';
        if (typeof a === 'number' && typeof b === 'number') return lt ? a < b : a <= b; if (typeof a === 'string' && typeof b === 'string') return lt ? a < b : a <= b;
        const h = (this.getmeta(a) && this.getmeta(a).get(lt ? '__lt' : '__le')) || (this.getmeta(b) && this.getmeta(b).get(lt ? '__lt' : '__le')); if (h) return truthy((yield* this.call(h, [a, b]))[0]);
        if (typeOf(l) === typeOf(r)) throw this.err(`attempt to compare two ${typeOf(l)} values`, e.line, ctx.chunk); throw this.err(`attempt to compare ${typeOf(l)} with ${typeOf(r)}`, e.line, ctx.chunk);
      }
    }
  }
  // -- public API
  // A per-script environment: writes land in the script's own table, reads fall back to the shared sandbox globals (like Roblox's per-script environments).
  makeEnv() { const t = new LuaTable(); t.base = this.globals; return t; }
  load(src, chunk = 'script', env = this.globals) { const ast = parse(src, chunk); const fnNode = { k: 'function', params: [], vararg: true, body: ast.body, name: chunk, chunk }; const f = new LuaFunction(fnNode, new Scope(null), this); f.chunk = chunk; f.env = env; return f; }
}
function describe(e) { if (!e) return ''; if (e.k === 'name') return `${e.local ? 'local' : 'global'} '${e.v}'`; if (e.k === 'index' && e.key.k === 'string') return `field '${e.key.v}'`; if (e.k === 'mcall') return `method '${e.name}'`; return ''; }

// ---------------------------------------------------------------- scheduler (cooperative threads; virtual time)
class Scheduler {
  constructor(interp, { onError = () => {} } = {}) { this.interp = interp; this.now = 0; this.threads = []; this.onError = onError; interp.scheduler = this; this.seq = 0; }
  spawn(fn, args = [], name = 'thread') { const t = { id: ++this.seq, name, gen: this.interp.call(fn, args), wake: this.now, dead: false, sent: undefined }; this.threads.push(t); return t; }
  spawnGen(gen, name = 'thread') { const t = { id: ++this.seq, name, gen, wake: this.now, dead: false, sent: undefined }; this.threads.push(t); return t; }
  spawnDelayed(fn, args, delay, name) { const t = this.spawn(fn, args, name); t.wake = this.now + delay; return t; }
  resume(t) {
    this.interp.steps = 0;
    try { const r = t.gen.next(t.sent); if (r.done) { t.dead = true; return; } const y = r.value; if (y && y.wait !== undefined) { t.wake = this.now + Math.max(y.wait, 0.0167); t.sent = [this.now - (y.start || this.now) + Math.max(y.wait, 0.0167), this.now]; t.startedWait = this.now; } else { t.wake = this.now; } }
    catch (e) { t.dead = true; if (e && (e.lua || e.budget)) this.onError(t, e); else this.onError(t, { lua: true, value: 'internal sandbox error (' + (e && e.message) + '): script stopped' }); }
  }
  // advance virtual time by dt, running every thread that becomes due (each thread runs until its next wait).
  step(dt) { const target = this.now + dt; for (;;) { const due = this.threads.filter(t => !t.dead && t.wake <= target).sort((a, b) => a.wake - b.wake || a.id - b.id)[0]; if (!due) break; this.now = Math.max(this.now, due.wake); this.resume(due); } this.now = target; this.threads = this.threads.filter(t => !t.dead); }
  get alive() { return this.threads.filter(t => !t.dead).length; }
}

// ---------------------------------------------------------------- stdlib
function installStdlib(I) {
  const G = I.globals; const T = () => new LuaTable();
  const def = (tbl, name, fn) => tbl.set(name, fn);
  const argErr = (i, fname, msg) => new LuaError(`bad argument #${i} to '${fname}' (${msg})`);
  const chkNum = (v, i, fname) => { const n = toNumber(v); if (n === undefined) throw argErr(i, fname, `number expected, got ${v === undefined ? 'no value' : typeOf(v)}`); return n; };
  const chkStr = (v, i, fname) => { if (typeof v === 'string') return v; if (typeof v === 'number') return fmtNum(v); throw argErr(i, fname, `string expected, got ${v === undefined ? 'no value' : typeOf(v)}`); };
  const chkTab = (v, i, fname) => { if (!(v instanceof LuaTable)) throw argErr(i, fname, `table expected, got ${v === undefined ? 'no value' : typeOf(v)}`); return v; };
  G.set('_G', T()); G.set('shared', T()); G.set('_VERSION', 'Lua 5.1 (BLOXEN sandbox)');
  def(G, 'print', a => { I.print(a.map(x => I.tostr(x)).join('\t')); return []; });
  def(G, 'type', a => { if (a.length === 0) throw argErr(1, 'type', 'value expected'); return [typeOf(a[0])]; });
  def(G, 'tostring', a => [I.tostr(a[0])]);
  def(G, 'tonumber', a => { if (a[1] !== undefined) { const base = chkNum(a[1], 2, 'tonumber'); const n = parseInt(String(a[0]).trim(), base); return [Number.isNaN(n) ? undefined : n]; } return [toNumber(a[0])]; });
  def(G, 'rawget', a => [chkTab(a[0], 1, 'rawget').get(a[1])]); def(G, 'rawset', a => { chkTab(a[0], 1, 'rawset').set(a[1], a[2]); return [a[0]]; }); def(G, 'rawequal', a => [a[0] === a[1]]);
  def(G, 'setmetatable', a => { const t = chkTab(a[0], 1, 'setmetatable'); if (a[1] !== undefined && !(a[1] instanceof LuaTable)) throw argErr(2, 'setmetatable', 'nil or table expected'); if (t.meta && t.meta.get('__metatable') !== undefined) throw new LuaError('cannot change a protected metatable'); t.meta = a[1] || null; return [t]; });
  def(G, 'getmetatable', a => { const m = I.getmeta(a[0]); if (m && m.get('__metatable') !== undefined) return [m.get('__metatable')]; return [m || undefined]; });
  def(G, 'assert', a => { if (!truthy(a[0])) throw new LuaError(a[1] !== undefined ? a[1] : 'assertion failed!'); return a; });
  def(G, 'error', a => { let v = a[0]; if (typeof v === 'string' && (a[1] === undefined || a[1] > 0)) v = `${I.curChunk || 'script'}:${I.curLine || 0}: ${v}`; throw new LuaError(v); });
  def(G, 'next', a => { const r = chkTab(a[0], 1, 'next').next(a[1]); return r || [undefined]; });
  def(G, 'pairs', a => { const t = chkTab(a[0], 1, 'pairs'); return [(x) => { const r = x[0].next(x[1]); return r || [undefined]; }, t, undefined]; });
  def(G, 'ipairs', a => { const t = chkTab(a[0], 1, 'ipairs'); return [(x) => { const i = x[1] + 1; const v = x[0].get(i); return v === undefined ? [undefined] : [i, v]; }, t, 0]; });
  def(G, 'select', a => { if (a[0] === '#') return [a.length - 1]; let n = chkNum(a[0], 1, 'select'); if (n < 0) n = a.length + n; else if (n === 0) throw argErr(1, 'select', 'index out of range'); return a.slice(Math.max(n, 1)); });
  def(G, 'unpack', a => { const t = chkTab(a[0], 1, 'unpack'); const i = a[1] === undefined ? 1 : a[1]; const j = a[2] === undefined ? t.length() : a[2]; if (j - i >= 8000) throw new LuaError('too many results to unpack'); const out = []; for (let k = i; k <= j; k++) out.push(t.get(k)); return out; });
  G.set('pcall', function* (a) { try { const r = yield* I.call(a[0], a.slice(1)); return [true, ...r]; } catch (e) { if (e && e.lua) return [false, e.value]; throw e; } });
  G.set('xpcall', function* (a) { try { const r = yield* I.call(a[0], []); return [true, ...r]; } catch (e) { if (e && e.lua) { const h = yield* I.call(a[1], [e.value]); return [false, ...h]; } throw e; } });
  // math
  const M = T(); G.set('math', M); M.set('pi', Math.PI); M.set('huge', Infinity);
  for (const f of ['abs', 'ceil', 'floor', 'sqrt', 'sin', 'cos', 'tan', 'asin', 'acos', 'atan', 'exp', 'log']) def(M, f, a => [Math[f](chkNum(a[0], 1, f))]);
  def(M, 'log10', a => [Math.log10(chkNum(a[0], 1, 'log10'))]); def(M, 'atan2', a => [Math.atan2(chkNum(a[0], 1, 'atan2'), chkNum(a[1], 2, 'atan2'))]); def(M, 'pow', a => [Math.pow(chkNum(a[0], 1, 'pow'), chkNum(a[1], 2, 'pow'))]);
  def(M, 'fmod', a => [chkNum(a[0], 1, 'fmod') % chkNum(a[1], 2, 'fmod')]); def(M, 'deg', a => [chkNum(a[0], 1, 'deg') * 180 / Math.PI]); def(M, 'rad', a => [chkNum(a[0], 1, 'rad') * Math.PI / 180]);
  def(M, 'max', a => { let m = chkNum(a[0], 1, 'max'); for (let i = 1; i < a.length; i++) m = Math.max(m, chkNum(a[i], i + 1, 'max')); return [m]; }); def(M, 'min', a => { let m = chkNum(a[0], 1, 'min'); for (let i = 1; i < a.length; i++) m = Math.min(m, chkNum(a[i], i + 1, 'min')); return [m]; });
  def(M, 'modf', a => { const x = chkNum(a[0], 1, 'modf'); const ip = x < 0 ? Math.ceil(x) : Math.floor(x); return [ip, x - ip]; });
  // deterministic PRNG (xorshift32) so runs are reproducible; seedable.
  let seed = 0x2545F491; const rnd = () => { seed ^= seed << 13; seed >>>= 0; seed ^= seed >>> 17; seed ^= seed << 5; seed >>>= 0; return seed / 4294967296; };
  def(M, 'randomseed', a => { seed = (Math.floor(chkNum(a[0], 1, 'randomseed')) >>> 0) || 1; return []; });
  def(M, 'random', a => { const r = rnd(); if (a.length === 0) return [r]; const m = Math.floor(chkNum(a[0], 1, 'random')); if (a.length === 1) { if (m < 1) throw argErr(1, 'random', 'interval is empty'); return [Math.floor(r * m) + 1]; } const n = Math.floor(chkNum(a[1], 2, 'random')); if (m > n) throw argErr(2, 'random', 'interval is empty'); return [Math.floor(r * (n - m + 1)) + m]; });
  // string
  const S = T(); G.set('string', S); const sidx = T(); I.stringMeta.set('__index', S);
  const posrel = (p, len) => p >= 0 ? p : (-p > len ? 0 : len + p + 1);
  def(S, 'len', a => [chkStr(a[0], 1, 'len').length]);
  def(S, 'sub', a => { const s = chkStr(a[0], 1, 'sub'); let i = posrel(a[1] === undefined ? 1 : Math.floor(chkNum(a[1], 2, 'sub')), s.length), j = posrel(a[2] === undefined ? -1 : Math.floor(chkNum(a[2], 3, 'sub')), s.length); if (i < 1) i = 1; if (j > s.length) j = s.length; return [i > j ? '' : s.slice(i - 1, j)]; });
  def(S, 'upper', a => [chkStr(a[0], 1, 'upper').toUpperCase()]); def(S, 'lower', a => [chkStr(a[0], 1, 'lower').toLowerCase()]);
  def(S, 'rep', a => { const s = chkStr(a[0], 1, 'rep'); const n = Math.floor(chkNum(a[1], 2, 'rep')); if (n * s.length > 1e6) throw new LuaError('string.rep result too large'); return [n > 0 ? s.repeat(n) : '']; });
  def(S, 'reverse', a => [[...chkStr(a[0], 1, 'reverse')].reverse().join('')]);
  def(S, 'byte', a => { const s = chkStr(a[0], 1, 'byte'); const i = posrel(a[1] === undefined ? 1 : a[1], s.length), j = posrel(a[2] === undefined ? i : a[2], s.length); const out = []; for (let k = Math.max(i, 1); k <= Math.min(j, s.length); k++) out.push(s.charCodeAt(k - 1)); return out; });
  def(S, 'char', a => [a.map((c, i) => String.fromCharCode(chkNum(c, i + 1, 'char'))).join('')]);
  def(S, 'format', a => [sformat(I, chkStr(a[0], 1, 'format'), a.slice(1), chkNum, chkStr)]);
  def(S, 'find', a => strFind(I, a, true)); def(S, 'match', a => strFind(I, a, false));
  def(S, 'gmatch', a => { const s = chkStr(a[0], 1, 'gmatch'), p = chkStr(a[1], 2, 'gmatch'); let pos = 0; return [() => { while (pos <= s.length) { const m = patMatch(s, p, pos); if (m) { const start = pos; pos = m.end > pos ? m.end : pos + 1; return capsOf(s, m, start, true); } pos++; } return [undefined]; }]; });
  S.set('gsub', function* (a) {
    const s = chkStr(a[0], 1, 'gsub'), p = chkStr(a[1], 2, 'gsub'), repl = a[2]; const maxN = a[3] === undefined ? Infinity : a[3]; let out = '', pos = 0, n = 0; const anchor = p[0] === '^';
    while (n < maxN) { const m = patMatch(s, p, pos); if (m) { n++; const whole = s.slice(pos, m.end); const caps = capsOf(s, m, pos, true); let rv;
        if (typeof repl === 'string' || typeof repl === 'number') rv = String(repl).replace(/%([0-9%])/g, (_, d) => d === '%' ? '%' : d === '0' ? whole : String(caps[+d - 1] === undefined ? '' : caps[+d - 1]));
        else if (repl instanceof LuaTable) { const v = repl.get(caps[0]); rv = v === undefined || v === false ? whole : I.tostr(v); }
        else { const r = yield* I.call(repl, caps); rv = r[0] === undefined || r[0] === false ? whole : I.tostr(r[0]); }
        out += rv; if (m.end > pos) pos = m.end; else { if (pos < s.length) out += s[pos]; pos++; } } else { if (pos < s.length) out += s[pos]; pos++; }
      if (pos > s.length || anchor) break; }
    if (pos < s.length) out += s.slice(pos); return [out, n];
  });
  // table
  const Tb = T(); G.set('table', Tb);
  def(Tb, 'insert', a => { const t = chkTab(a[0], 1, 'insert'); if (a.length === 2) { t.set(t.length() + 1, a[1]); } else { const pos = Math.floor(chkNum(a[1], 2, 'insert')); const n = t.length(); for (let i = n; i >= pos; i--) t.set(i + 1, t.get(i)); t.set(pos, a[2]); } return []; });
  def(Tb, 'remove', a => { const t = chkTab(a[0], 1, 'remove'); const n = t.length(); const pos = a[1] === undefined ? n : Math.floor(chkNum(a[1], 2, 'remove')); if (n === 0) return [undefined]; const v = t.get(pos); for (let i = pos; i < n; i++) t.set(i, t.get(i + 1)); t.set(n, undefined); return [v]; });
  def(Tb, 'concat', a => { const t = chkTab(a[0], 1, 'concat'); const sep = a[1] === undefined ? '' : chkStr(a[1], 2, 'concat'); const i = a[2] === undefined ? 1 : a[2], j = a[3] === undefined ? t.length() : a[3]; const out = []; for (let k = i; k <= j; k++) { const v = t.get(k); if (typeof v !== 'string' && typeof v !== 'number') throw new LuaError(`invalid value (at index ${k}) in table for 'concat'`); out.push(I.tostr(v)); } return [out.join(sep)]; });
  def(Tb, 'getn', a => [chkTab(a[0], 1, 'getn').length()]);
  Tb.set('sort', function* (a) { const t = chkTab(a[0], 1, 'sort'); const cmp = a[1]; const arr = t.arr.slice(); // merge sort with (possibly yielding) comparator
    const lt = function* (x, y) { if (cmp === undefined) { if (typeof x === typeof y && (typeof x === 'number' || typeof x === 'string')) return x < y; throw new LuaError(`attempt to compare ${typeOf(x)} with ${typeOf(y)}`); } return truthy((yield* I.call(cmp, [x, y]))[0]); };
    const msort = function* (xs) { if (xs.length < 2) return xs; const mid = xs.length >> 1; const l = yield* msort(xs.slice(0, mid)), r = yield* msort(xs.slice(mid)); const out = []; let i = 0, j = 0; while (i < l.length && j < r.length) { if (yield* lt(r[j], l[i])) out.push(r[j++]); else out.push(l[i++]); } return out.concat(l.slice(i), r.slice(j)); };
    const sorted = yield* msort(arr); t.arr = sorted; return []; });
  // os (safe subset only)
  const O = T(); G.set('os', O); def(O, 'time', () => [Math.floor((I.clockMs ? I.clockMs() : Date.now()) / 1000)]); def(O, 'clock', () => [(I.clockMs ? I.clockMs() : Date.now()) / 1000]); def(O, 'date', a => [new Date(I.clockMs ? I.clockMs() : Date.now()).toISOString()]);
  // coroutine (built on generators; wait() inside a coroutine propagates to the scheduler)
  const C = T(); G.set('coroutine', C);
  const mkco = f => ({ luaThread: true, f, gen: null, status: 'suspended', luaToString() { return 'thread: 0x' + (this.id || (this.id = Math.random().toString(16).slice(2, 10))); } });
  // A wait() inside a coroutine suspends only that coroutine (as in Roblox): the resumer gets control back immediately and the scheduler
  // continues the coroutine later as a background thread. While it runs in the background it cannot be resumed again.
  const driveBackground = function* (co, firstYield) {
    try {
      let sent = yield firstYield;
      for (;;) { const r = co.gen.next(sent); if (r.done) { co.status = 'dead'; co.bg = false; return; } const y = r.value; if (y && y.coYield) { co.status = 'suspended'; co.bg = false; return; } sent = yield y; }
    } catch (e) { co.status = 'dead'; co.bg = false; throw e; }
  };
  const resume = function* (co, args) {
    if (co.status === 'dead') return [false, 'cannot resume dead coroutine']; if (co.status === 'running' || co.bg) return [false, 'cannot resume non-suspended coroutine'];
    const prev = I.currentCo; I.currentCo = co; co.status = 'running'; if (prev) prev.status = 'normal';
    try {
      if (!co.gen) co.gen = I.call(co.f, args); const r0 = co.started ? co.gen.next(args) : co.gen.next(); co.started = true; const r = r0;
      if (r.done) { co.status = 'dead'; return [true, ...r.value]; } const y = r.value;
      if (y && y.coYield) { co.status = 'suspended'; return [true, ...y.values]; }
      // wait(): hand the rest of the coroutine to the scheduler
      co.status = 'suspended'; co.bg = true; I.scheduler.spawnGen(driveBackground(co, y), 'coroutine'); return [true];
    } catch (e) { co.status = 'dead'; if (e && e.lua) return [false, e.value]; throw e; } finally { I.currentCo = prev; if (prev) prev.status = 'running'; }
  };
  def(C, 'create', a => { if (!(a[0] instanceof LuaFunction) && typeof a[0] !== 'function') throw argErr(1, 'create', 'Lua function expected'); return [mkco(a[0])]; });
  C.set('resume', function* (a) { return yield* resume(a[0], a.slice(1)); });
  C.set('yield', function* (a) { if (!I.currentCo) throw new LuaError('attempt to yield across metamethod/C-call boundary'); const sent = yield { coYield: true, values: a }; return sent || []; });
  def(C, 'status', a => [a[0].status]); def(C, 'running', () => [I.currentCo || undefined]);
  def(C, 'wrap', a => { const co = mkco(a[0]); return [function* (args) { const r = yield* resume(co, args); if (r[0] === false) throw new LuaError(r[1]); return r.slice(1); }]; });
  // Roblox-flavoured globals that do not touch the engine (engine adds more)
  G.set('wait', function* (a) { const t = a[0] === undefined ? 0.03 : chkNum(a[0], 1, 'wait'); const r = yield { wait: t }; return r || [t, I.scheduler ? I.scheduler.now : 0]; });
  G.set('Wait', G.get('wait'));
  def(G, 'tick', () => [(I.scheduler ? I.scheduler.now : 0) + (I.epoch || 0)]); def(G, 'time', () => [I.scheduler ? I.scheduler.now : 0]); def(G, 'elapsedTime', () => [I.scheduler ? I.scheduler.now : 0]);
  const spawnFn = a => { I.scheduler.spawn(a[0], [], 'spawn'); return []; }; def(G, 'spawn', spawnFn); def(G, 'Spawn', spawnFn);
  const delayFn = a => { I.scheduler.spawnDelayed(a[1], [], Math.max(chkNum(a[0], 1, 'delay'), 0), 'delay'); return []; }; def(G, 'delay', delayFn); def(G, 'Delay', delayFn);
}

// ---- Lua patterns (port of lstrlib.c matching, 5.1)
const L_ESC = '%'; const MAXCAP = 32;
function patMatch(s, p, init) {
  const ms = { s, p, level: 0, capture: [], depth: 0 }; let pi = 0; let si = init; let anchor = false; if (p[0] === '^') { anchor = true; pi = 1; }
  const e = doMatch(ms, si, pi); if (e === -1) return null; return { end: e, caps: ms.capture.slice(0, ms.level), anchor };
}
function classEnd(ms, pi) { const p = ms.p; if (pi >= p.length) throw new LuaError('malformed pattern (ends with \'%\')'); const c = p[pi++]; if (c === L_ESC) { if (pi >= p.length) throw new LuaError("malformed pattern (ends with '%')"); return pi + 1; } if (c === '[') { if (p[pi] === '^') pi++; do { if (pi >= p.length) throw new LuaError("malformed pattern (missing ']')"); const cc = p[pi++]; if (cc === L_ESC) pi++; } while (p[pi] !== ']' || pi >= p.length); return pi + 1; } return pi; }
function singleClass(c, cl) { const code = c.charCodeAt(0); let res; switch (cl.toLowerCase()) { case 'a': res = /[A-Za-z]/.test(c); break; case 'c': res = code < 32 || code === 127; break; case 'd': res = /[0-9]/.test(c); break; case 'l': res = /[a-z]/.test(c); break; case 'p': res = /[!-\/:-@\[-`{-~]/.test(c); break; case 's': res = /[ \t\n\r\f\v]/.test(c); break; case 'u': res = /[A-Z]/.test(c); break; case 'w': res = /[A-Za-z0-9]/.test(c); break; case 'x': res = /[0-9A-Fa-f]/.test(c); break; case 'z': res = code === 0; break; default: return cl === c; } return cl >= 'A' && cl <= 'Z' ? !res : res; }
function matchClassSet(ms, c, pi, ep) { // pi points at '[', ep at ']'
  const p = ms.p; let sig = true; pi++; if (p[pi] === '^') { sig = false; pi++; }
  while (pi < ep) { if (p[pi] === L_ESC) { pi++; if (singleClass(c, p[pi])) return sig; pi++; } else if (p[pi + 1] === '-' && pi + 2 < ep) { if (p[pi] <= c && c <= p[pi + 2]) return sig; pi += 3; } else { if (p[pi] === c) return sig; pi++; } }
  return !sig;
}
function singleMatch(ms, si, pi, ep) { if (si >= ms.s.length) return false; const c = ms.s[si]; const pc = ms.p[pi]; switch (pc) { case '.': return true; case L_ESC: return singleClass(c, ms.p[pi + 1]); case '[': return matchClassSet(ms, c, pi, ep - 1); default: return pc === c; } }
function doMatch(ms, si, pi) {
  if (++ms.depth > 200) throw new LuaError('pattern too complex'); try {
    const p = ms.p, s = ms.s;
    for (;;) {
      if (pi >= p.length) return si;
      switch (p[pi]) {
        case '(': if (p[pi + 1] === ')') return startCapture(ms, si, pi + 2, -2); return startCapture(ms, si, pi + 1, -1);
        case ')': return endCapture(ms, si, pi + 1);
        case '$': if (pi + 1 === p.length) return si === s.length ? si : -1; break;
        case L_ESC: { const nx = p[pi + 1];
          if (nx === 'b') { return matchBalance(ms, si, pi + 2); }
          if (nx === 'f') { pi += 2; if (p[pi] !== '[') throw new LuaError("missing '[' after '%f' in pattern"); const ep = classEnd(ms, pi); const prev = si === 0 ? '\0' : s[si - 1]; const cur = si < s.length ? s[si] : '\0'; if (!matchClassSet(ms, prev, pi, ep - 1) && matchClassSet(ms, cur, pi, ep - 1)) { pi = ep; continue; } return -1; }
          if (nx >= '0' && nx <= '9') { const l = +nx - 1; if (l < 0 || l >= ms.level || ms.capture[l].len === -1) throw new LuaError('invalid capture index'); const cap = s.substr(ms.capture[l].init, ms.capture[l].len); if (s.startsWith(cap, si)) { si += cap.length; pi += 2; continue; } return -1; }
        } break;
      }
      const ep = classEnd(ms, pi); const epc = p[ep];
      if (epc === '?') { if (singleMatch(ms, si, pi, ep)) { const r = doMatch(ms, si + 1, ep + 1); if (r !== -1) return r; } pi = ep + 1; continue; }
      if (epc === '+') { return singleMatch(ms, si, pi, ep) ? maxExpand(ms, si + 1, pi, ep) : -1; }
      if (epc === '*') return maxExpand(ms, si, pi, ep);
      if (epc === '-') return minExpand(ms, si, pi, ep);
      if (!singleMatch(ms, si, pi, ep)) return -1; si++; pi = ep;
    }
  } finally { ms.depth--; }
}
function maxExpand(ms, si, pi, ep) { let i = 0; while (singleMatch(ms, si + i, pi, ep)) i++; while (i >= 0) { const r = doMatch(ms, si + i, ep + 1); if (r !== -1) return r; i--; } return -1; }
function minExpand(ms, si, pi, ep) { for (;;) { const r = doMatch(ms, si, ep + 1); if (r !== -1) return r; if (singleMatch(ms, si, pi, ep)) si++; else return -1; } }
function startCapture(ms, si, pi, what) { if (ms.level >= MAXCAP) throw new LuaError('too many captures'); ms.capture[ms.level] = { init: si, len: what }; ms.level++; const r = doMatch(ms, si, pi); if (r === -1) ms.level--; return r; }
function endCapture(ms, si, pi) { let l = -1; for (let i = ms.level - 1; i >= 0; i--) if (ms.capture[i].len === -1) { l = i; break; } if (l < 0) throw new LuaError('invalid pattern capture'); ms.capture[l].len = si - ms.capture[l].init; const r = doMatch(ms, si, pi); if (r === -1) ms.capture[l].len = -1; return r; }
function matchBalance(ms, si, pi) { const p = ms.p, s = ms.s; if (pi + 1 >= p.length) throw new LuaError("unbalanced pattern"); if (si >= s.length || s[si] !== p[pi]) return -1; const b = p[pi], e = p[pi + 1]; let cont = 1, i = si + 1; while (i < s.length) { if (s[i] === e) { if (--cont === 0) { return doMatch(ms, i + 1, pi + 2); } } else if (s[i] === b) cont++; i++; } return -1; }
function capsOf(s, m, start, wholeIfNone) { if (m.caps.length === 0) return wholeIfNone ? [s.slice(start, m.end)] : []; return m.caps.map(c => c.len === -2 ? c.init + 1 : s.substr(c.init, c.len)); }
function strFind(I, a, isFind) {
  const name = isFind ? 'find' : 'match'; const s = typeof a[0] === 'string' ? a[0] : typeof a[0] === 'number' ? fmtNum(a[0]) : (() => { throw new LuaError(`bad argument #1 to '${name}' (string expected, got ${typeOf(a[0])})`); })(); const p = typeof a[1] === 'string' ? a[1] : typeof a[1] === 'number' ? fmtNum(a[1]) : (() => { throw new LuaError(`bad argument #2 to '${name}' (string expected, got ${typeOf(a[1])})`); })();
  let init = a[2] === undefined ? 1 : Math.floor(toNumber(a[2])); if (init < 0) init = Math.max(s.length + init + 1, 1); if (init < 1) init = 1; if (init > s.length + 1) return [undefined];
  if (isFind && (truthy(a[3]) || !/[\^$*+?.()\[\]%-]/.test(p))) { const idx = s.indexOf(p, init - 1); return idx < 0 ? [undefined] : [idx + 1, idx + p.length]; }
  const anchor = p[0] === '^'; let pos = init - 1;
  do { const m = patMatch(s, p, pos); if (m) { const caps = capsOf(s, m, pos, !isFind); return isFind ? [pos + 1, m.end, ...caps] : caps; } pos++; } while (pos <= s.length && !anchor);
  return [undefined];
}
function sformat(I, fmt, args, chkNum, chkStr) {
  let ai = 0; return fmt.replace(/%([-+ #0]*)(\d+)?(?:\.(\d+))?([a-zA-Z%])/g, (m, flags, width, prec, conv) => {
    if (conv === '%') return '%'; const arg = args[ai++]; let out;
    switch (conv) {
      case 'd': case 'i': { const n = Math.trunc(chkNum(arg, ai, 'format')); out = String(Math.abs(n)); if (prec !== undefined) out = out.padStart(+prec, '0'); out = (n < 0 ? '-' : flags.includes('+') ? '+' : flags.includes(' ') ? ' ' : '') + out; break; }
      case 'u': out = String(Math.max(0, Math.trunc(chkNum(arg, ai, 'format')))); break;
      case 'c': out = String.fromCharCode(chkNum(arg, ai, 'format')); break;
      case 'x': out = (Math.trunc(chkNum(arg, ai, 'format')) >>> 0).toString(16); break; case 'X': out = (Math.trunc(chkNum(arg, ai, 'format')) >>> 0).toString(16).toUpperCase(); break; case 'o': out = (Math.trunc(chkNum(arg, ai, 'format')) >>> 0).toString(8); break;
      case 'f': case 'F': { const n = chkNum(arg, ai, 'format'); out = n.toFixed(prec === undefined ? 6 : +prec); if (flags.includes('+') && n >= 0) out = '+' + out; break; }
      case 'e': case 'E': { const n = chkNum(arg, ai, 'format'); out = n.toExponential(prec === undefined ? 6 : +prec).replace(/e([+-])(\d)$/, 'e$10$2'); if (conv === 'E') out = out.toUpperCase(); break; }
      case 'g': case 'G': { const n = chkNum(arg, ai, 'format'); out = fmtNum(n); break; }
      case 's': { out = I.tostr(arg); if (prec !== undefined) out = out.slice(0, +prec); break; }
      case 'q': out = '"' + String(chkStr(arg, ai, 'format')).replace(/[\\"\n\r]/g, c => c === '\n' ? '\\n' : c === '\r' ? '\\r' : '\\' + c) + '"'; break;
      default: throw new LuaError(`invalid option '%${conv}' to 'format'`);
    }
    if (width !== undefined && out.length < +width) out = flags.includes('-') ? out.padEnd(+width) : (flags.includes('0') && 'dioxXfFeEgG'.includes(conv) ? out.padStart(+width, '0') : out.padStart(+width));
    return out;
  });
}

module.exports = { Interp, Scheduler, LuaTable, LuaFunction, LuaError, BudgetError, parse, typeOf, toNumber, truthy, fmtNum };
