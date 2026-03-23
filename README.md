# c2wasm

**A self-hosting C compiler that targets WebAssembly, running entirely in your browser.**

[![Live Demo](https://img.shields.io/badge/demo-live-brightgreen)](https://parker.github.io/c2wasm/)
[![GitHub](https://img.shields.io/badge/github-c2wasm-blue)](https://github.com/parker/c2wasm)

---

## [→ Try the Live Demo](https://parker.github.io/c2wasm/)

Write C in the browser. Click **Compile & Run**. See the output instantly — no servers, no plugins, no waiting. Everything happens in your browser tab.

---

## What is this?

c2wasm is a complete C compiler written in a curated subset of C — and that subset is exactly what the compiler itself can compile. This makes it **self-hosting**: the compiler can compile its own source code.

The browser demo shows the full pipeline:

```
C source  →  [c2wasm compiler]  →  WAT (WebAssembly Text)  →  [wabt.js]  →  WASM binary  →  runs in browser
```

Everything runs client-side. The compiler itself was compiled to WebAssembly with Emscripten and ships as a single JS file.

---

## Features

- **Write C in the browser** — Monaco editor with C syntax highlighting, error markers, and Ctrl+Enter to compile
- **Instant output** — see your program's console output in the same tab
- **WAT inspector** — view the generated WebAssembly Text Format to see exactly what the compiler emitted
- **Your own files** — create new C files, edit them freely, and they persist in your browser's local storage across sessions
- **Built-in examples** — Hello World, Fibonacci, Linked List traversal, and Bubble Sort to get started
- **Self-hosting** — the compiler is written in the same C subset it compiles; see `tools/bootstrap.sh` for the 3-stage verification

---

## Supported C Subset

c2wasm supports a carefully chosen subset of C89/C90:

| Feature | Supported |
|---------|-----------|
| Types | `int`, `char`, `void`, `T*`, `T[N]`, `struct { ... }` |
| Literals | Integer (decimal/hex), character, string |
| Arithmetic | `+`, `-`, `*`, `/`, `%` |
| Comparison | `==`, `!=`, `<`, `<=`, `>`, `>=` |
| Logical | `&&`, `\|\|`, `!` |
| Assignment | `=`, `+=`, `-=` |
| Pointer ops | `*` (deref), `&` (address-of), `[]` (subscript), `.`, `->` |
| Prefix | `++`, `--`, cast `(T)` |
| Statements | `if`/`else`, `while`, `for`, `return`, `break`, `continue` |
| Functions | Recursion, forward declarations |
| Built-ins | `malloc`, `free` (no-op), `exit`, `putchar`, `getchar`, `printf` (`%d %s %c %x`) |
| Preprocessor | `#define NAME integer-literal` only |

All types map to `i32` internally. No `float`, no `double`.

---

## How It Works

### Compiler Pipeline (inside `compiler/src/c2wasm.c`)

```
Source text
    │
    ▼
┌─────────┐
│  Lexer  │  Tokenizes: identifiers, keywords, numbers, strings, operators
└────┬────┘
     │ Token stream
     ▼
┌──────────┐
│  Parser  │  Recursive descent; Pratt-style expression precedence
└────┬─────┘
     │ AST
     ▼
┌──────────────┐
│ Type Checker │  Resolves symbols, computes struct field offsets, annotates AST
└────┬─────────┘
     │ Typed AST
     ▼
┌──────────────┐
│ Code Gen     │  Single-pass WAT emission; printf lowered to putchar calls
└────┬─────────┘
     │ WAT text
     ▼
  stdout
```

### Browser Integration

- **Compiler → WASM**: compiled with Emscripten (`emcc -s SINGLE_FILE=1`), embedded in `compiler.js`
- **Stdin/stdout redirection**: Emscripten callbacks feed C source character-by-character and capture WAT output
- **WAT → WASM binary**: [wabt.js](https://github.com/WebAssembly/wabt) assembles WAT to a binary in the browser
- **Execution**: `WebAssembly.instantiate` with a minimal WASI shim; `fd_write` captures `putchar` calls

### Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| All types → `i32` | Maximum simplicity; supports all example programs |
| Output WAT, not binary | WAT is readable; lets you see exactly what the compiler did |
| `printf` lowered at compile time | Format strings expanded to `putchar` calls; no variadic WASM needed |
| Bump allocator, `free` is a no-op | Sufficient for the C subset; keeps codegen simple |
| Single-file compiler | Maximizes self-hosting elegance; 2,800 lines of C |

---

## Self-Hosting Verification

The compiler can compile itself. `tools/bootstrap.sh` performs a 3-stage bootstrap:

```
Stage 1: GCC compiles c2wasm.c → native binary (s1)
Stage 2: s1 compiles c2wasm.c → WAT → WASM (s2)
Stage 3: s2 compiles c2wasm.c → WAT (s3.wat)

diff s1.wat s3.wat  ← must be empty
```

A clean diff proves that the compiler, when run inside WebAssembly, produces byte-for-byte identical output to the GCC-compiled version.

---

## Local Development

### Prerequisites

- GCC (for native build and tests)
- Python 3 (for `make serve`)
- Emscripten (`emcc`) — only needed to rebuild `compiler.js`

### Build & Test

```bash
# Build the native compiler
cd compiler
make

# Run the test suite (30 programs, from return-0 to structs+strings)
make test

# Rebuild the WASM compiler (requires emcc)
make wasm

# Serve the demo locally
make serve
# then open http://localhost:8080
```

### Test Programs

`compiler/tests/programs/` contains 30 progressive test programs:

```
00_return0.c        → 10_while.c         → 20_struct_field.c
01_return42.c       → 11_for.c           → 21_struct_ptr.c
02_arithmetic.c     → 12_nested_loop.c   → 22_struct_malloc.c
03_variables.c      → 13_array.c         → 23_linked_list.c
04_if_else.c        → 14_pointer.c       → 24_recursive_sum.c
05_comparison.c     → 15_address_of.c    → 25_bubble_sort.c
06_function.c       → 16_global.c        → 26_string_ops.c
07_recursion.c      → 17_printf_d.c      → 27_multiarg_printf.c
08_fibonacci.c      → 18_putchar.c       → 28_break_continue.c
09_multiple_funcs.c → 19_scanf_sim.c     → 29_char_string.c
```

---

## Project Structure

```
wasm-c/
├── compiler/
│   ├── src/c2wasm.c          ← the compiler (2,800 lines of C)
│   ├── Makefile
│   └── tests/
│       ├── run_tests.sh
│       └── programs/         ← 30 test programs + expected output
├── demo/
│   ├── index.html            ← browser UI
│   ├── main.js               ← editor, pipeline, localStorage file management
│   ├── compiler-api.js       ← Emscripten wrapper
│   ├── compiler.js           ← compiled compiler (Emscripten, SINGLE_FILE=1)
│   └── style.css             ← VS Code-inspired dark theme
├── tools/
│   └── bootstrap.sh          ← 3-stage self-hosting verification
└── .github/
    └── workflows/
        └── pages.yml         ← auto-deploy demo/ to GitHub Pages
```

---

## Contributing

The compiler is intentionally written in its own supported subset — if you add a language feature, you must be able to write the implementation using only features the compiler already supports.

Issues and PRs welcome.

---

## License

MIT
