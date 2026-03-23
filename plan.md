# Plan: Self-Hosting C → WASM Compiler + Browser Demo

## Summary
Build `c2wasm` — a C compiler written in a carefully chosen C subset large enough that the compiler can compile itself ("self-hosting"). The compiler reads C source from stdin and emits WAT (WebAssembly Text Format) to stdout. It is then compiled to WASM via Emscripten and powers a polished browser demo: users write C in a Monaco editor, the embedded compiler converts it to WAT, wabt.js assembles it to WASM binary, and the program runs live with stdout shown on-screen.

## Decisions
- **Output format**: WAT (not binary) — far simpler to generate; wabt.js handles assembly in the browser
- **All C types → WASM i32**: massive codegen simplification; no float support in scope
- **printf lowered at compile time**: parse format string literal → expand to putchar/int-to-string calls; avoids variadic WASM
- **free() is no-op**: bump allocator only; sufficient for demo programs
- **Single-file compiler** (`c2wasm.c`): maximizes self-hosting elegance, easy to bootstrap
- **C89-style declarations** (at top of block): simpler to parse
- **No preprocessor** except `#define NAME integer-literal`
- **Demo stack**: Monaco CDN + wabt.js CDN + Emscripten output; no bundler

## Supported C Subset (SPEC.md)
- Types: `int`, `char`, `void`, pointer (`T*`), 1D array (`T[N]`), `struct { ... }`
- Literals: integer (`42`), character (`'a'`), string (`"hello"`)
- Declarations: globals, locals, function prototypes/definitions, struct type defs
- Operators: arithmetic, comparison, logical (`&&`, `||`, `!`), assignment (`=`, `+=`, `-=`), dereference (`*`), address-of (`&`), subscript (`[]`), member (`.`, `->`), prefix `++`/`--`, cast `(T)`
- Statements: `if`/`else`, `while`, `for`, `return`, `break`, `continue`, block `{ }`
- Built-ins: `malloc`, `free` (no-op), `exit`, `putchar`, `getchar`, `printf` (`%d`, `%s`, `%c`, `%x`)

## Project Structure
```
wasm-c/
├── SPEC.md                       # C subset formal specification
├── README.md
├── compiler/
│   ├── src/
│   │   └── c2wasm.c             # The compiler — single file, ~2500-4000 lines
│   ├── Makefile                 # native + wasm targets
│   └── tests/
│       ├── run_tests.sh
│       └── programs/            # progressive test C programs
│           ├── hello.c
│           ├── fibonacci.c
│           ├── pointers.c
│           ├── structs.c
│           ├── linked_list.c
│           └── sort.c
├── tools/
│   └── bootstrap.sh             # 3-stage self-hosting verification
└── demo/
    ├── index.html
    ├── main.js                  # orchestration
    ├── compiler-api.js          # Emscripten stdio wrappers
    ├── style.css
    └── examples/
        ├── hello.c
        ├── fibonacci.c
        └── sort.c
    # Emscripten output (generated, not committed):
    # compiler.js + compiler.wasm
```

## Phase 0 — Specification
- Write `SPEC.md` with the exact grammar (BNF or prose) and built-in list
- This is the contract: every feature listed must be supported AND the compiler source may only use listed features
- *Dependency for all other phases*

## Phase 1 — Lexer
- `Token` struct: kind (enum), lexeme (char*), int_val, line, col
- `Lexer` struct: source pointer, current pos, line, col
- `next_token(Lexer*) → Token` — handles all token types in SPEC
- Standalone test: lex a file, print tokens to verify

## Phase 2 — AST + Parser
- `ASTNode` tagged-union struct (kind enum + union of node-specific structs)
- Node kinds: Program, FuncDecl, StructDecl, VarDecl, Block, If, While, For, Return, Break, Continue, ExprStmt, plus expression nodes (Assign, Binary, Unary, Call, Subscript, Member, Deref, AddrOf, Cast, Ident, IntLit, CharLit, StrLit)
- Recursive descent parser — one function per grammar rule
- Expression parsing: Pratt / precedence climbing for operator precedence
- Arena-based allocation: `arena_alloc(Arena*, size_t)` — single malloc for the whole parse
- Standalone test: parse and pretty-print AST

## Phase 3 — Symbol Table + Type Resolution
- Scoped symbol table: linked list of scope frames, each frame a name→Symbol hash map
- `Symbol` struct: name, type descriptor, offset (for struct fields), is_global
- `Type` descriptor: kind (INT/CHAR/VOID/PTR/ARRAY/STRUCT), pointee/element type, struct_name, array_len
- Pass 1: collect all global decls (functions, structs, globals) — enables forward references
- Pass 2: walk AST, resolve identifiers, annotate each node with type, compute struct field offsets (4-byte aligned)
- Type checks: assignment compatibility, call argument count/type

## Phase 4 — WAT Code Generator
- Maintain a label counter for unique WASM block labels
- **Static data**: first pass collects all string literals, assigns offsets starting at 1024; emit as WAT `(data (i32.const N) "...")` sections
- **Runtime preamble** emitted into every module:
  - `(memory 1)`
  - `(global $heap_ptr (mut i32) (i32.const <after_static_data>))`
  - `(func $malloc (param $n i32) (result i32) ...)` — bump allocator
  - `(func $free (param $p i32))` — no-op
  - `(import "env" "putchar" ...)`, `(import "env" "getchar" ...)`, `(import "env" "exit" ...)`
- **printf lowering**: scan format string literal at codegen time, generate sequence of `putchar`/helper calls — no variadic needed
- **int_to_str helper**: emit a WAT helper function for %d formatting
- **Locals**: each local var → `(local $name i32)` in WASM function
- **Globals**: C globals → `(global $name (mut i32) (i32.const 0))`
- **Control flow**:
  - `if/else` → `(if (then ...) (else ...))`
  - `while` → `(block $brk (loop $cont ... (br_if $brk (cond_false)) ... (br $cont)))`
  - `for` → desugar to while
  - `break`/`continue` → `(br $brk)` / `(br $cont)`
- **Struct field access**: `i32.load (i32.add base field_offset)` / `i32.store`
- **Array subscript**: `i32.load (i32.add base (i32.mul idx elem_size))`
- **char load/store**: use `i32.load8_s` / `i32.store8`
- **Pointer arithmetic**: `p + n` → `base + n * sizeof(*p)`
- **Casts**: mostly no-op (all i32); `(char)int` → `i32.and (i32.const 255)`
- Output: printf/putchar the WAT text to stdout

## Phase 5 — Native Testing
- `Makefile`: `make native` → `gcc compiler/src/c2wasm.c -o c2wasm`
- `run_tests.sh`:
  - For each `tests/programs/*.c`: run `./c2wasm < prog.c > prog.wat`, run `wat2wasm prog.wat`, run `wasmtime prog.wasm`, diff against `prog.expected`
- Progressive test programs (each exercises new features):
  1. `hello.c` — putchar, string literal, main returning 0
  2. `fibonacci.c` — recursion, int arithmetic
  3. `pointers.c` — pointer dereference, address-of, pointer arithmetic
  4. `structs.c` — struct definition, stack and heap allocation, member access
  5. `linked_list.c` — malloc, struct pointers, while loop, NULL checks
  6. `sort.c` — arrays, nested loops, function calls

## Phase 6 — Self-Hosting Verification
- Audit `c2wasm.c` against SPEC.md — every feature used must be listed
- `tools/bootstrap.sh`:
  1. `gcc c2wasm.c -o c2wasm_s0` (stage 0 = GCC bootstrap)
  2. `./c2wasm_s0 < c2wasm.c > s1.wat && wat2wasm s1.wat -o s1.wasm`
  3. `wasmtime s1.wasm < c2wasm.c > s2.wat`
  4. `diff s1.wat s2.wat` — must be empty (fixed point = self-hosting proven)

## Phase 7 — Emscripten Build
- Command:
  ```
  emcc compiler/src/c2wasm.c -O2 -o demo/compiler.js \
    -s MODULARIZE=1 -s EXPORT_NAME=C2WasmModule \
    -s EXPORTED_RUNTIME_METHODS='["callMain","FS"]'
  ```
- `demo/compiler-api.js`:
  - Lazy-init `C2WasmModule()`
  - `compile(cSource: string): Promise<string>`:
    - Set `Module.stdin` to char-by-char reader over cSource
    - Set `Module.print` to accumulate stdout string
    - Call `Module.callMain([])`
    - Return accumulated WAT string
- Verify all 6 test programs compile correctly via Emscripten build

## Phase 8 — Demo Page
- **Layout**: two-column — Monaco editor (left, 60%) + output panel (right, 40%)
- **Monaco**: CDN via require.js, C language mode, vs-dark theme, pre-loaded example program
- **Output panel**: two tabs — "Console" (user program stdout) | "WAT" (compiler output)
- **Example selector**: dropdown (Hello World / Fibonacci / Linked List / Bubble Sort)
- **Compile & Run button**: state machine — idle → compiling (C→WAT) → assembling (WAT→WASM) → running → done/error; each step with visual indicator
- **Error display**: compiler errors shown in Monaco gutter markers + console tab
- **wabt.js**: lazy-loaded from CDN, cached after first load
- **Pipeline** (all in main.js):
  1. `compile(cSource)` via `compiler-api.js` → WAT string
  2. `wabt.parseWat(...).toBinary({})` → `Uint8Array`
  3. `WebAssembly.instantiate(bytes, { env: { putchar, getchar, exit } })` → instance
  4. Call `instance.exports.main()`; capture putchar output; display in console tab
- **Styling**: dark VS Code-inspired theme, Inter + JetBrains Mono fonts, subtle animations, responsive
- **Header**: project name + tagline + GitHub link
- **No bundler**: all vanilla JS + CDN; optionally wrap with Vite for production build

## Verification
1. `cd compiler && make test` — all 6 test programs pass (`run_tests.sh` green)
2. `./tools/bootstrap.sh` — stage 1 WAT == stage 2 WAT (self-hosting proven)
3. `make wasm` — Emscripten build succeeds, `demo/compiler.js` + `demo/compiler.wasm` produced
4. Open `demo/index.html` in browser — compile Fibonacci, verify output matches `fib(10) = 55`
5. Open demo — compile `linked_list.c` — exercises malloc + struct pointers + loops
6. Open demo — introduce a C syntax error — verify error message appears in Monaco + console

## Further Considerations
1. **Error recovery**: Simple "error + exit(1)" on first syntax error is fine v1. The error message must include line:col. Multiple errors = stretch goal.
2. **Hosting**: GitHub Pages (fully static). CI via GitHub Actions running `make test` + `bootstrap.sh` on every push. Needs `wabt` and `wasmtime` on the runner.
3. **Scope boundary**: No float/double, no unsigned, no function pointers, no multi-dim arrays, no variadic user functions, no `switch` (use if/else chains in compiler source), no standard library beyond the 5 listed built-ins.
