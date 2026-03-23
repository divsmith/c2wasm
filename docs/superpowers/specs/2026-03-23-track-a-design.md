# Track A: C89 Language Features — Design Spec

**Date**: 2026-03-23  
**Scope**: Implement A1–A9 from `roadmap.md` sequentially in a single compiler source file (`compiler/src/c2wasm.c`, currently 2838 lines). All 30 existing tests must remain green after every feature.

---

## Context

`c2wasm` is a single-file C→WAT compiler (~2838 lines) that maps all types to `i32`, uses a bump allocator, and lowers `printf` at compile time. It uses `#define` constants throughout (no real enums in source) to stay within its own supported subset. All 30 tests currently pass.

The implementation order follows the roadmap's suggested priority:
**A1 → A2 → A3 → A4 → A6 → A5 → A7 → A8 → A9**

Each feature gets a test program before moving to the next. Tests run with `make test` in `compiler/`.

---

## A1 — Missing Operators

### New tokens
Append to the `#define` block:
- `TOK_CARET` — `^` (bitwise XOR)
- `TOK_PIPE_EQ` — `|=`
- `TOK_AMP_EQ` — `&=`
- `TOK_CARET_EQ` — `^=`
- `TOK_LSHIFT_EQ` — `<<=`
- `TOK_RSHIFT_EQ` — `>>=`

### Lexer changes
- `^` → `TOK_CARET` (or `^=` → `TOK_CARET_EQ`)
- `|=` → `TOK_PIPE_EQ` (before `|` → `TOK_PIPE`)
- `&=` → `TOK_AMP_EQ` (before `&` → `TOK_AMP`)
- `<<=` → `TOK_LSHIFT_EQ` (before `<<`)
- `>>=` → `TOK_RSHIFT_EQ` (before `>>`)

### Parser changes
- `infix_bp`: `TOK_CARET` gets precedence 9 (between `|`=8 and `&`=10, matching C standard)
- `infix_bp`: new compound assignments added alongside `TOK_PLUS_EQ`/`TOK_MINUS_EQ` at precedence 2 (lowest, right-assoc)
- Assignment target validation: add `TOK_PIPE_EQ`, `TOK_AMP_EQ`, `TOK_CARET_EQ`, `TOK_LSHIFT_EQ`, `TOK_RSHIFT_EQ` to the compound-assignment check at line 1095

### Post-increment / post-decrement
New node kinds: `ND_POST_INC`, `ND_POST_DEC`.

In `parse_expr_bp`'s infix loop, after the standard `infix_bp` dispatch: if `TOK_PLUS_PLUS` or `TOK_MINUS_MINUS` is seen at precedence ≥ min_bp (postfix binds tighter than prefix, use precedence 27), wrap the current `left` node.

Codegen — four target kinds mirroring the compound-assignment pattern:

**Local ident** (`a++`):
```
local.get $a       ← push old value (expression result left on stack)
local.get $a
i32.const 1
i32.add
local.set $a       ← store incremented; old value already on stack
```

**Global ident** (`g++`):
```
global.get $g      ← save old to $__atmp
local.set $__atmp
global.get $g
i32.const 1
i32.add
local.set $__atmp2  ← need second temp... actually:
```
Simpler: use `$__atmp` for old value, emit global read twice:
```
global.get $g          ← old value on stack
local.set $__atmp
global.get $g
i32.const 1
i32.add
global.set $g          ← store incremented
local.get $__atmp      ← push old value (expression result)
```

**Pointer dereference** (`*p++`) — target is `ND_UNARY+TOK_STAR`:
```
<gen addr of p>        ← compute address
i32.load8_u / i32.load ← load old value
local.set $__atmp
<gen addr of p>        ← recompute address
<gen addr of p>
i32.load8_u / i32.load ← reload
i32.const 1
i32.add
i32.store8 / i32.store ← store incremented
local.get $__atmp      ← push old value
```

**Subscript** (`arr[i]++`) and **member** (`s.f++`) — same save/reload pattern as pointer dereference using `$__atmp`.

### Codegen changes
- `ND_BINARY`: add `TOK_CARET` → `i32.xor`
- `ND_ASSIGN`: extend all four target-kind branches (`ND_IDENT`, `ND_UNARY+TOK_STAR`, `ND_MEMBER`, `ND_SUBSCRIPT`) with the five new compound operators, using the same load/op/store pattern as `+=`/`-=`

### Test
`compiler/tests/programs/30_operators.c` — exercises `^`, `a++`, `a--`, `|=`, `&=`, `^=`, `<<=`, `>>=`. Uses `// EXPECT_EXIT: 42`.

---

## A2 — `do-while`

### New token
`TOK_DO` — keyword `do`.

### New node
`ND_DO_WHILE`: `c0=body`, `c1=condition`.

### Parser
In `parse_stmt`: when `at(TOK_DO)`, consume `do`, parse body (`parse_stmt`), expect `while`, `(`, parse condition, `)`, `;`. Return `ND_DO_WHILE`.

### `collect_locals`
Add `ND_DO_WHILE` case: scan `c0` (body).

### Codegen
```
(block $brk_N          ← break exits here
  (loop $lp_N          ← loop back-edge
    (block $cont_N     ← continue lands here, falls into condition
      <body>
    )
    <condition>
    br_if $lp_N        ← repeat if condition true
  )
)
```
Pushes `brk_lbl[loop_sp]` and `cont_lbl[loop_sp]` before emitting, pops after.

### Test
`compiler/tests/programs/31_do_while.c` — basic loop, break, continue. `// EXPECT_EXIT: 42`.

---

## A3 — Ternary Operator

### New tokens
`TOK_QUESTION` (`?`), `TOK_COLON` (`:`).

### Lexer
- `:` → `TOK_COLON` (simple single-character token, added to the lexer's operator dispatch; reused by A4 for `case:` and `default:`).

### New node
`ND_TERNARY`: `c0=condition`, `c1=then_expr`, `c2=else_expr`.

### Parser
In `parse_expr_bp`'s infix loop, before the standard `infix_bp` dispatch, add:
```c
if (at(TOK_QUESTION) && 3 >= min_bp) {
    advance_tok();
    then_e = parse_expr_bp(0);
    expect(TOK_COLON, "expected ':'");
    else_e = parse_expr_bp(3);   /* right-assoc */
    n = node_new(ND_TERNARY, ...);
    n->c0 = left; n->c1 = then_e; n->c2 = else_e;
    left = n; continue;
}
```
Precedence 3 places ternary just above assignment (2) and below `||` (4).

### Codegen
Emits a value-returning WAT `if`:
```
<condition>
(if (result i32)
  (then <then_expr>)
  (else <else_expr>)
)
```

### Test
`compiler/tests/programs/32_ternary.c` — basic `? :`, nested ternary, ternary as argument. `// EXPECT_EXIT: 42`.

---

## A4 — `switch` / `case` / `default`

### New tokens
`TOK_SWITCH`, `TOK_CASE`, `TOK_DEFAULT`. (`TOK_COLON` added in A3.)

### New nodes
- `ND_SWITCH`: `c0=switch_expr`, `list=flat statement array`, `ival2=count`
- `ND_CASE`: `ival=case_integer_value`
- `ND_DEFAULT`: no extra fields

### Parser
`parse_stmt` dispatches to `parse_switch` on `TOK_SWITCH`. `parse_switch`:
1. Consume `switch`, `(`, parse expr, `)`.
2. Expect `{`. Collect statements until `}` using a variant of `parse_stmt` that also recognizes `case N:` and `default:` as statement-list entries.
3. Return `ND_SWITCH`.

Inside the switch body, `parse_stmt` is extended: `TOK_CASE` → consume `case`, parse integer literal, expect `:`, return `ND_CASE`. `TOK_DEFAULT` → consume `default`, expect `:`, return `ND_DEFAULT`.

`break` inside a switch uses the existing `brk_lbl` stack — switches push their own break label.

### `collect_locals`
`ND_SWITCH` arm: scan all statements in `list` for `ND_VAR_DECL`.

### Codegen — two-pass
**Pre-scan**: iterate `n->list` to collect `case_vals[]` (in appearance order) and `has_default`.

**Emit** (cases in source order, blocks in reverse-nested order):
```
(block $brk_N
  local.set $__stmp    ← save switch expression
  (block $sw_dflt      ← branch here = jump to default/end
    (block $sw_cN      ← outermost case block = last case
      ...
      (block $sw_c1    ← innermost case block = first case
        ;; dispatch: if-chain
        local.get $__stmp  i32.const V1  i32.eq  br_if $sw_c1
        local.get $__stmp  i32.const V2  i32.eq  br_if $sw_c2
        ...
        br $sw_dflt    ← no match → default/end
      )
      ;; case 1 body (landed by br $sw_c1 jumping to END of $sw_c1)
      ;; break: br $brk_N   fallthrough: natural
    )
    ;; case 2 body (landed by br $sw_c2)
    ...
  )
  ;; default body (landed by br $sw_dflt, or absent = fall off end of sw_dflt)
)
```

An if-chain dispatch (not `br_table`) handles non-consecutive case values without gaps. Fallthrough is natural: no `break` means execution continues into the next case body block.

`break` inside any case: `br $brk_N`. `continue` is NOT redefined by switch (it still refers to the enclosing loop).

### Break label stack
Before emitting blocks, push `brk_lbl[loop_sp] = N` and increment `loop_sp`. Do NOT set `cont_lbl` — switches are not loop contexts and `continue` must pass through to the enclosing loop's `cont_lbl`. After emitting all blocks, decrement `loop_sp`.

### Test
`compiler/tests/programs/33_switch.c` — basic cases, fallthrough, break, default, nested loops+break. `// EXPECT_EXIT: 42`.

---

## A5 — `sizeof` (fix + complete)

`TOK_SIZEOF` and `ND_SIZEOF` already exist. Two fixes and one extension:

### Fixes
1. **`sizeof(char)` codegen**: currently returns 4. Fix: check `n->sval` for `"char"` → emit `i32.const 1`.
2. **Pointer types**: parser skips `*` stars but doesn't record them. Use `n->ival` as a pointer flag (1 if any `*` consumed). In codegen, check `n->ival == 1` **before** the struct lookup — pointer-to-struct (`sizeof(struct S *)`) must return 4, not the struct size.

### Extension: `sizeof expr` (best-effort)
After `sizeof`, if `(` is present AND the next token is a type keyword (checked by a lookahead call to `is_type_token()` after peeking one ahead), use the existing type-form path. Otherwise treat as expr-form:
- If `(` is present but not followed by a type keyword: parse expression inside parens normally.
- If no `(`: parse expression at high binding power.

Store the expression in `c0`. Infer size:
- `ND_IDENT` → look up var type (char → 1, else → 4)
- `ND_INT_LIT`, `ND_STR_LIT`, `ND_CALL`, etc. → 4
- anything else → 4

### Codegen
```
sizeof(int)      → i32.const 4
sizeof(char)     → i32.const 1
sizeof(T*)       → i32.const 4  (pointer flag set)
sizeof(struct S) → i32.const S.size
sizeof expr      → inferred constant
```

### Test
`compiler/tests/programs/34_sizeof.c` — `sizeof(int)`, `sizeof(char)`, `sizeof(struct ...)`, pointer size. `// EXPECT_EXIT: 42`.

---

## A6 — `const` Qualifier

### New token
`TOK_CONST` — keyword `const`.

### Changes
- `is_type_token()`: if at `TOK_CONST`, look ahead one token — return true if the next token is a valid type keyword (or typedef alias, after A8).
- `parse_type()`: if at `TOK_CONST`, advance and continue.
- `parse_var_decl()`: skip leading `TOK_CONST` before and/or after the base type.
- Function parameter parsing: same skip.
- No AST node, no enforcement.

### Test
Extend an existing test with `const int` and `const char *` declarations — no new numbered test file. The const keyword only affects parsing; no new behavior to validate beyond "it compiles and produces correct results."

---

## A7 — `enum`

### New token
`TOK_ENUM` — keyword `enum`.

### New data structure
```c
struct EnumConst {
    char *name;
    int val;
};
struct EnumConst *enum_consts[MAX_ENUM_CONSTS];  /* #define MAX_ENUM_CONSTS 512 */
int nenum_consts;
```

### Parsing enum declarations
At top level (and inside function bodies before declarations), when `TOK_ENUM` is seen and followed by `{` (with optional tag name between):
1. Skip optional tag name.
2. Consume `{`.
3. Parse `NAME [= int_literal]` pairs separated by `,`. Auto-increment from previous value starting at 0.
4. Register each name in `enum_consts` table.
5. Consume `}` then `;`.

### Enum as type
`is_type_token()`: returns true if at `TOK_ENUM`.  
`parse_type()`: consume `enum`, skip optional tag name, return type code 0 (int).  
`parse_var_decl()`: handles `enum Tag varname;` by treating `enum Tag` as type int.

### Enum constants in expressions
In `parse_atom()`, when `TOK_IDENT` is seen: before creating `ND_IDENT`, check `enum_consts` table. If found, return `ND_INT_LIT` with the stored value.

### Test
`compiler/tests/programs/35_enum.c` — declare enum with explicit and implicit values, use constants in expressions and as variable type. `// EXPECT_EXIT: 42`.

---

## A8 — `typedef`

### New token
`TOK_TYPEDEF` — keyword `typedef`.

### New data structure
```c
struct TypeAlias {
    char *alias;
    char *canonical;  /* "int", "char", "void", or struct tag name */
    int is_ptr;
};
struct TypeAlias *type_aliases[MAX_TYPE_ALIASES];  /* #define MAX_TYPE_ALIASES 128 */
int ntype_aliases;
```

### Parsing typedef declarations
At top level, when `TOK_TYPEDEF` is seen:
1. Parse base type (handles `int`, `char`, `void`, `struct Tag`, or an existing alias name).
2. Consume any `*` stars — set `is_ptr = 1` if any.
3. Consume the new alias name (identifier).
4. Expect `;`.
5. Register in `type_aliases`.

### Type resolution
- `is_type_token()`: returns true if `TOK_IDENT` and the name is a known typedef alias.
- `parse_type()`: if at a typedef alias, consume it and return the resolved type code.
- `parse_var_decl()`: same.
- Function parameter parsing: same.

### typedef chain
Single-level resolution is sufficient. `typedef int size_t;` then `typedef size_t my_size;` resolves through one lookup.

### Test
`compiler/tests/programs/36_typedef.c` — `typedef int size_t;`, `typedef struct Foo Foo;`, `typedef char* string;`, use in declarations and function parameters. `// EXPECT_EXIT: 42`.

---

## A9 — Array Initializers

### Parser changes
`parse_var_decl`: after the variable name (identifier), if `[` is seen, consume the array size expression (integer literal or `]` for implicit size from string), store size in `n->ival` (reusing the field). After `]`, if `=` is seen:
- `{expr, expr, ...}`: parse initializer list into `n->list` / `n->ival2` (count). Node kind stays `ND_VAR_DECL`; use a flag (e.g., `n->ival2 = -count` or a dedicated flag field) to distinguish array-init from scalar-init.
- `"string literal"`: parse the string literal as the initializer (char array).

### Codegen — local arrays

**With initializer `{e1, e2, e3}`**:
```
i32.const <N * elem_size>
call $malloc
local.set $arr

local.get $arr             ← arr[0] = e1
<e1>
i32.store

local.get $arr
i32.const 4
i32.add                    ← arr[1] = e2
<e2>
i32.store
...
```

**With string literal `= "hello"`**:
```
i32.const <string_data_offset>
local.set $s
```

**Without initializer `int arr[N];`**:
```
i32.const <N * elem_size>
call $malloc
local.set $arr
```

### Codegen — global arrays
Global arrays with **all-constant initializers**: emit element stores via a module-level `$__init` function called before `main`. For example, global `int g[3] = {10,20,30}` allocates from the bump heap at startup and stores three constants. Global arrays without initializers: allocate from bump heap in `$__init`. Global arrays with **non-constant initializers** (expressions involving variables): not supported in this implementation — emit a compiler error.

### `collect_locals`
Array variables are still just `i32` locals (they hold a pointer). No change needed.

### Test
`compiler/tests/programs/37_array_init.c` — `int arr[3] = {1,2,3}`, `char s[] = "hello"`, `int buf[10]` (no init). `// EXPECT_EXIT: 42`.

---

## Cross-Cutting Constraints

- **Self-hosting**: all new syntax in user programs must compile with the compiler's own already-supported subset (which it is — `#define` constants are used throughout, no new C syntax in compiler source).
- **No new global variable kinds**: all new tables (`enum_consts`, `type_aliases`) follow the existing `malloc(N * sizeof(void*))` pattern.
- **All 30 existing tests remain green** after every individual feature.
- **Test numbering**: 30 (operators), 31 (do-while), 32 (ternary), 33 (switch), 34 (sizeof), 35 (enum), 36 (typedef), 37 (array_init). The `const` feature (A6) has no new test file — it is validated by the existing tests compiling cleanly after the keyword is recognized.
- **Implementation order**: A1→A2→A3→A4→A6→A5→A7→A8→A9 (const before sizeof because the fix is trivial; const before enum/typedef since those tests may use `const`).
