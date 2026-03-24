# wasm-c Roadmap Plan

## Context

c2wasm is a self-hosting, single-file C compiler (~2,838 lines) that targets WebAssembly Text (WAT). It supports a curated subset of C89 and is compiled to WASM for the browser using Emscripten. The project already passes 30 test programs and can self-host.

Two directions of interest:
1. **Expand C89 language coverage** — fill in missing standard features
2. **Reduce external dependencies** — particularly emcc (Emscripten), wat2wasm, and wasmtime

---

## Track A: C89 Language Features

Projects are ordered by size and dependency (smaller/easier first, each builds on the last).

---

### A1 — Missing Operators (small)

**What:** Fill in C89 operators not yet implemented.

- Bitwise XOR: `^`
- Post-increment / post-decrement: `a++`, `a--`
- Remaining compound assignments: `|=`, `&=`, `^=`, `<<=`, `>>=`

**Why small:** Each is a 1–3 line addition to the lexer/parser/codegen. Post-increment requires a new AST node kind that loads, emits old value, then increments. XOR and compound assignments mirror the existing `&`, `|`, `+=`, `-=` paths exactly.

**Test:** Add a test program `30_operators.c` exercising all new operators.

---

### A2 — `do-while` Loop (small)

**What:** Add `do { body } while (cond);` statement.

**Why small:** Structurally identical to `while` in the AST — just emit the body first, then test the condition and branch back. Reuses the existing break/continue label infrastructure.

**Test:** Add `31_do_while.c`.

---

### A3 — Ternary Operator (small–medium)

**What:** Add `condition ? then_expr : else_expr`.

**Why:** Very common C89 idiom; blocks self-hosting of code that uses it. Needs a new AST node (`NODE_TERNARY`) and code-gen that emits an `if`/`else` block returning a value.

**Test:** Add `32_ternary.c`.

---

### A4 — `switch` / `case` / `default` (medium)

**What:** Full `switch (expr) { case N: ... break; default: ... }` statement.

**Why:** One of the most-used C89 control-flow constructs. Currently forces callers to use if/else chains. Fallthrough semantics must be respected.

**Approach:**
- New AST nodes: `NODE_SWITCH`, `NODE_CASE`, `NODE_DEFAULT`
- In codegen: emit a `block`/`br_table` sequence mapping case values to WASM branch labels
- `break` inside switch uses the outer block's exit label (already handled by break-label stack)

**Test:** Add `33_switch.c` covering: basic cases, fallthrough, break, default.

---

### A5 — `sizeof` Operator (small–medium)

**What:** `sizeof(type)` and `sizeof expr` — evaluate to an integer constant at compile time.

**Why:** Required to write generic memory code (like custom allocators). Also enables correct struct/array sizing without hardcoding.

**Approach:**
- Extend parser to recognize `sizeof` keyword
- `sizeof(type)` resolves type size at compile time (int→4, char→1, T*→4, struct→existing field-offset logic)
- `sizeof expr` resolves from the expression's type
- Emits `(i32.const N)` — no runtime cost

**Test:** Add `34_sizeof.c`.

---

### A6 — `const` Qualifier (trivial)

**What:** Parse `const` as a type qualifier and silently ignore it (no enforcement).

**Why:** Needed to compile standard library headers and third-party code that uses `const`. No enforcement is required for self-hosting.

**Approach:** Consume `const` token wherever a type qualifier is valid; treat the underlying type normally.

**Test:** Extend an existing test to declare `const int` variables.

---

### A7 — `enum` (small–medium)

**What:** `enum Color { RED = 0, GREEN, BLUE };` — named integer constants.

**Why:** Very common in idiomatic C89. Equivalent to a set of `#define` values but scoped and type-safe in spirit.

**Approach:**
- New struct `EnumDef` holding name → int value mappings (auto-incrementing from previous)
- Enum names become compile-time integer constants, emitted as `(i32.const N)`
- Enum type is treated as `int`

**Test:** Add `35_enum.c`.

---

### A8 — `typedef` (medium)

**What:** `typedef int size_t;`, `typedef struct Foo Foo;`

**Why:** Essential for idiomatic C89 and for using standard type aliases (`size_t`, `ptrdiff_t`, etc.). Also allows opaque struct types.

**Approach:**
- New alias table `TypeAlias[]` mapping alias name → canonical type string
- Parser resolves typedef names to their base type before building AST nodes
- `typedef struct S S;` allows using `S` as a type name

**Test:** Add `36_typedef.c`.

---

### A9 — Array Initializers (medium)

**What:** `int arr[3] = {1, 2, 3};` and `char s[] = "hello";`

**Why:** Very common C89 pattern; currently requires manually assigning each element.

**Approach:**
- Extend declaration parser to accept `= { expr, expr, ... }` for arrays
- Emit element assignments in order as part of function prologue (for locals) or data section (for globals)
- For `char[] = "string"`, emit as string data

**Test:** Add `37_array_init.c`.

---

## Track B: Dependency Reduction

---

### B1 — Replace `wat2wasm` + `wasmtime` with a Single Tool (small–medium)

**Current state:** The test runner (`run_tests.sh`) shells out to `wat2wasm` (WABT) then `wasmtime`. Both must be installed separately.

**Goal:** Replace both with a single, more widely available tool — or a Node.js script using `wabt` npm + WASM execution.

**Option 1 (recommended): `wasm-tools` + `wasmer`**
- `wasm-tools` (Bytecodealliance) replaces `wat2wasm`
- `wasmer` is more widely packaged than `wasmtime` and supports WASI
- Update `run_tests.sh` to use `wasm-tools parse` + `wasmer run`

**Option 2: Node.js script**
- Single `run_tests.js` using `npm i -g wabt` + Node's built-in `WebAssembly` API
- Eliminates two separate installs in favor of one (`node`)
- Slightly heavier but more portable

**Deliverable:** Update `run_tests.sh` (or add `run_tests.js`) so `make test` works with the new toolchain. Update README install instructions.

---

### B2 — Replace `emcc` with `wasi-sdk` / `zig cc` (medium–large)

**Current state:** `make wasm` requires Emscripten (`emcc`) and uses Emscripten-specific runtime APIs (`FS`, `callMain`, module factory). The output `compiler.js` is ~1.2MB and bundles the Emscripten runtime.

**Goal:** Replace `emcc` with a lighter-weight WASI-compatible compiler (`wasi-sdk` or `zig cc --target=wasm32-wasi`), producing a smaller, dependency-free WASM binary.

**Approach:**
1. Adapt `c2wasm.c` to use WASI I/O (stdin/stdout via file descriptors 0/1) instead of Emscripten's libc shim — this should already work since the compiler uses `getchar`/`putchar` which map cleanly to WASI `fd_read`/`fd_write`
2. Add a `wasi-sdk` build target to the Makefile
3. Replace `compiler-api.js` with a WASI shim (either hand-written or using a small library like `@nicolo-ribaudo/wasi-shim` or the browser WASI polyfill)
4. Remove dependency on Emscripten's `FS` and `callMain`; instead pipe source code through WASM memory directly
5. Expected output size: ~100–200KB (vs 1.2MB Emscripten bundle)

**Note:** Keep `emcc` as an optional/legacy target. The GitHub Actions workflow can keep using emcc or switch to wasi-sdk.

**Test:** `make wasm-wasi` produces a working `compiler.wasm`. Demo page works with updated `compiler-api.js`.

---

### B3 — WASM Binary Emitter (stretch / large)

**What:** Instead of outputting WAT text, have the compiler emit a valid WASM binary directly.

**Impact:**
- Eliminates `wat2wasm` / `wasm-tools` entirely from tests
- Eliminates `wabt.js` from the browser demo
- Compiler becomes fully self-contained: C → WASM, no intermediate tools

**Why it's large:** WASM binary format requires encoding types, functions, globals, memory, imports/exports, and code sections in LEB128 format. ~400–800 lines of new codegen code.

**Approach:**
- Add a `--binary` flag (or make it the default)
- New file or section in `c2wasm.c`: `emit_wasm_binary()` mirrors `emit_wat()` but writes bytes
- Tests: compare binary output against `wat2wasm` reference output on the first run, then use the binary directly

**This is a long-term stretch goal.** Tackle after A-series features are stable.

---

## Suggested Order of Execution

| Priority | Project | Size | Value |
|----------|---------|------|-------|
| 1 | A1 — Missing Operators | Small | High (unblocks real programs) |
| 2 | A2 — do-while | Small | Medium |
| 3 | A3 — Ternary | Small | High |
| 4 | A4 — switch/case | Medium | High |
| 5 | A6 — const (parse only) | Trivial | Medium |
| 6 | A5 — sizeof | Medium | Medium |
| 7 | A7 — enum | Medium | Medium |
| 8 | A8 — typedef | Medium | Medium |
| 9 | A9 — Array initializers | Medium | Medium |
| 10 | B1 — Replace wat2wasm+wasmtime | Medium | High |
| 11 | B2 — Replace emcc | Large | High |
| 12 | B3 — Binary emitter | Large | Very High (stretch) |

---

## Notes

- Every A-series project must keep the compiler **self-hosting**: any new syntax added must be expressible in the subset the compiler already supports (or in the subset as expanded by earlier projects in this list).
- Track A projects should be done sequentially; each test program should be added and passing before the next project starts.
- B1 can be done independently of A-series work.
- B2 depends on B1 (cleaner test toolchain makes B2 easier to validate).
