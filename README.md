# c2wasm

**A self-hosting C compiler that targets WebAssembly, running entirely in your browser.**

[![Live Demo](https://img.shields.io/badge/demo-live-brightgreen)](https://divsmith.github.io/c2wasm/)
[![GitHub](https://img.shields.io/badge/github-c2wasm-blue)](https://github.com/divsmith/c2wasm)

---

## [→ Try the Live Demo](https://divsmith.github.io/c2wasm/)

Write C in the browser. Click **Compile & Run**. See the output instantly — no servers, no plugins, no waiting. Everything happens in your browser tab.

---

## What is this?

c2wasm is a complete C compiler written in a curated subset of C — and that subset is exactly what the compiler itself can compile. This makes it **self-hosting**: the compiler can compile its own source code.

The browser demo shows the full pipeline:

```
C source  →  [c2wasm compiler]  →  WASM binary  →  runs in browser
```

Everything runs client-side. The compiler itself was compiled to a 229 KB WASM binary using [wasi-sdk](https://github.com/WebAssembly/wasi-sdk) (standard WASI target). The compiler can also output WAT (WebAssembly Text Format) for inspection.

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
| Types | `int`, `char`, `void`, `float`, `double`, `unsigned`, `short`, `long`, `T*`, `T[N]`, `struct { ... }` |
| Literals | Integer (decimal/hex), character, string, float (`3.14`, `1e5`, `.5f`) |
| Arithmetic | `+`, `-`, `*`, `/`, `%` (integer and floating-point) |
| Comparison | `==`, `!=`, `<`, `<=`, `>`, `>=` |
| Logical | `&&`, `\|\|`, `!` |
| Assignment | `=`, `+=`, `-=` |
| Pointer ops | `*` (deref), `&` (address-of), `[]` (subscript), `.`, `->` |
| Prefix | `++`, `--`, cast `(T)` |
| Statements | `if`/`else`, `while`, `for`, `do`/`while`, `switch`/`case`, `return`, `break`, `continue` |
| Functions | Recursion, forward declarations |
| Memory | `malloc`, `free` (free-list allocator with coalescing), `calloc` |
| Built-ins | `exit`, `putchar`, `getchar`, `printf` (`%d %s %c %x %f`), `puts` |
| libc | `strlen`, `strcmp`, `strcpy`, `strcat`, `strchr`, `strstr`, `memcpy`, `memset`, `memmove`, `atoi`, `abs`, `rand`/`srand`, `isdigit`, `isalpha`, `toupper`, `tolower`, and more |
| Preprocessor | `#define NAME integer-literal`, `enum`, `typedef` |

Integer types map to `i32`. Float types use `f64` in WASM (WAT mode). Unsigned types use appropriate unsigned WASM opcodes.

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
│ Code Gen     │  WAT text mode or WASM binary mode
└────┬─────────┘
     │
     ▼
  stdout (WAT text or WASM binary)
```

The compiler has two output modes:
- **WAT mode** (`build/c2wasm`): Emits WebAssembly Text Format — human-readable, useful for debugging
- **Binary mode** (`build/c2wasm-bin`): Emits WASM binary directly — no external assembler needed

### Browser Integration

- **Compiler → WASM**: compiled with [wasi-sdk](https://github.com/WebAssembly/wasi-sdk) to a standalone 229 KB `.wasm` file (no JavaScript runtime bundled)
- **Stdin/stdout redirection**: a minimal WASI shim (`compiler-api.js`) feeds C source as stdin bytes and captures output from stdout
- **Direct binary output**: the browser compiler emits WASM binary directly — no `wabt.js` assembler needed
- **Execution**: `WebAssembly.instantiate` with a WASI shim; `fd_write`/`fd_read` capture stdout and supply pre-buffered stdin; programs using `getchar` read from the stdin input box in the demo UI

### Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| Integer types → `i32`, float types → `f64` | Simplicity; covers all common use cases |
| Dual output: WAT + binary | WAT is readable; binary eliminates external assembler |
| `printf` lowered at compile time | Format strings expanded to `putchar`/helper calls; no variadic WASM needed |
| Free-list allocator with coalescing | Real `free()` enables long-running programs and libc functions |
| 40+ inline libc functions | No external linking needed; every program is self-contained |
| Single-file compiler | Maximizes self-hosting elegance |

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

There are **three binaries** in this project:

| Binary | What it is | How to build |
|--------|-----------|-------------|
| `build/c2wasm` | Native compiler — WAT output (for debugging) | `cd compiler && make` |
| `build/c2wasm-bin` | Native compiler — binary output (for tests) | `cd compiler && make ../build/c2wasm-bin` |
| `demo/compiler.wasm` | Browser compiler — binary output (for the demo) | `WASI_SDK=... make wasm` |

`demo/compiler.wasm` is gitignored — a fresh clone won't have it. You only need to rebuild it if you're working on the demo or changed the compiler and want to update the deployed binary.

### Prerequisites

| Tool | Required for |
|------|-------------|
| GCC | Building the native compiler |
| `wat2wasm` + `wasmtime` | Running tests and bootstrap |
| [wasi-sdk v25](https://github.com/WebAssembly/wasi-sdk/releases/tag/wasi-sdk-25) | Rebuilding `demo/compiler.wasm` |
| Python 3 | `make serve` |

Install wasi-sdk v25 from the [releases page](https://github.com/WebAssembly/wasi-sdk/releases/tag/wasi-sdk-25). Extract it to `/opt/wasi-sdk` or `~/.local/wasi-sdk` — the Makefile checks both automatically.

```bash
# macOS ARM64 (Apple Silicon)
curl -LO https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-25/wasi-sdk-25.0-arm64-macos.tar.gz
tar xzf wasi-sdk-25.0-arm64-macos.tar.gz
mv wasi-sdk-25.0-arm64-macos ~/.local/wasi-sdk      # no sudo needed

# macOS x86_64 (Intel)
curl -LO https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-25/wasi-sdk-25.0-x86_64-macos.tar.gz
tar xzf wasi-sdk-25.0-x86_64-macos.tar.gz
mv wasi-sdk-25.0-x86_64-macos ~/.local/wasi-sdk

# Linux x86_64
curl -LO https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-25/wasi-sdk-25.0-x86_64-linux.tar.gz
tar xzf wasi-sdk-25.0-x86_64-linux.tar.gz
mv wasi-sdk-25.0-x86_64-linux ~/.local/wasi-sdk
```

You can also use a different path — just pass it explicitly: `WASI_SDK=/your/path make wasm`.

### Build the native compiler

```bash
cd compiler
make           # builds build/c2wasm from src/c2wasm.c
make clean     # force a clean rebuild next time
```

> `make` prints `Nothing to be done for 'all'` when `build/c2wasm` is already up to date — that's correct. Run `make clean && make` to force a full rebuild.

### Run the tests

Requires `wat2wasm` and `wasmtime` on `PATH`:

```bash
cd compiler
make test         # WAT path: all 39 tests + bootstrap self-hosting check
make test-binary  # binary path: all 39 tests (only needs wasmtime, not wat2wasm)
make bootstrap    # run the 3-stage bootstrap check alone
```

### Rebuild the browser compiler

Only needed when you change `compiler/src/c2wasm.c` and want to update the demo:

```bash
cd compiler && make wasm
# auto-detects wasi-sdk at /opt/wasi-sdk or ~/.local/wasi-sdk
# produces demo/compiler.wasm (229 KB)
```


### Serve the demo locally

```bash
cd compiler && make serve
# open http://localhost:8080
```

> `make serve` uses `demo/server.py` instead of `python3 -m http.server` because the demo requires
> `Cross-Origin-Opener-Policy` and `Cross-Origin-Embedder-Policy` headers to enable `SharedArrayBuffer`
> (interactive stdin). On GitHub Pages these headers are injected by `demo/coi-serviceworker.js`.

### Test Programs

`compiler/tests/programs/` contains 39 progressive test programs, from basic returns through structs, strings, switch/case, enums, typedef, and array initializers.

---

## Project Structure

```
wasm-c/
├── compiler/
│   ├── src/c2wasm.c          ← the compiler (~5,000 lines of C)
│   ├── Makefile
│   └── tests/
│       ├── run_tests.sh      ← test runner (supports --binary flag)
│       └── programs/         ← 39 test programs + expected output
├── demo/
│   ├── index.html            ← browser UI
│   ├── main.js               ← editor, pipeline, localStorage file management
│   ├── compiler-api.js       ← WASI shim — loads and runs compiler.wasm
│   ├── compiler.wasm         ← compiled compiler (wasi-sdk, 229 KB)
│   ├── wasm-worker.js        ← Web Worker for running compiled programs
│   └── style.css             ← VS Code-inspired dark theme
├── tools/
│   └── bootstrap.sh          ← 3-stage self-hosting verification
└── .github/
    └── workflows/
        └── pages.yml         ← auto-deploy demo/ to GitHub Pages (wasi-sdk)
```

---

## Contributing

The compiler is intentionally written in its own supported subset — if you add a language feature, you must be able to write the implementation using only features the compiler already supports.

Issues and PRs welcome.

---

## License

MIT
