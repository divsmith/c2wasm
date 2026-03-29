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

Everything runs client-side. The compiler itself was compiled to a ~667 KB WASM binary using [wasi-sdk](https://github.com/WebAssembly/wasi-sdk) (standard WASI target). The compiler can also output WAT (WebAssembly Text Format) for inspection.

---

## Features

- **Write C in the browser** — Monaco editor with C syntax highlighting, error markers, and Ctrl+Enter to compile
- **Instant output** — see your program's console output in the same tab
- **Live compiler editing** — switch to Compiler mode to browse and modify the compiler's own source code, build a custom compiler from your changes using the reference compiler, and use it to compile programs
- **Self-hosting in the browser** — the reference compiler compiles your modified compiler, then your compiler compiles user programs — the full self-hosting pipeline, live
- **Your own files** — create new C files, edit them freely, and they persist in your browser's local storage across sessions
- **Built-in examples** — 26 examples including Hello World, Fibonacci, Linked List, Bubble Sort, floating-point, union types, goto, varargs, preprocessor, and more
- **Self-hosting** — the compiler is written in the same C subset it compiles; see `tools/bootstrap.sh` for the 2-stage bootstrap verification

---

## Supported C Subset

c2wasm supports a carefully chosen subset of C89/C90:

| Feature | Supported |
|---------|-----------|
| Types | `int`, `char`, `void`, `float`, `double`, `unsigned`, `short`, `long`, `T*`, `T[N]`, `T[M][N]`, `struct { ... }`, `union { ... }` |
| Literals | Integer (decimal/hex), character, string, float (`3.14`, `1e5`, `.5f`) |
| Arithmetic | `+`, `-`, `*`, `/`, `%` (integer and floating-point) |
| Comparison | `==`, `!=`, `<`, `<=`, `>`, `>=` |
| Logical | `&&`, `\|\|`, `!` |
| Assignment | `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `\|=`, `&=`, `^=`, `<<=`, `>>=` |
| Pointer ops | `*` (deref), `&` (address-of globals/heap), `[]` (subscript), `.`, `->` |
| Prefix | `++`, `--`, cast `(T)` |
| Statements | `if`/`else`, `while`, `for`, `do`/`while`, `switch`/`case`, `return`, `break`, `continue`, `goto`/labels |
| Functions | Recursion, forward declarations, function pointers (`call_indirect`), variadic (`...`) |
| Memory | `malloc`, `free` (free-list allocator with coalescing), `calloc`, `realloc` |
| Built-ins | `exit`, `putchar`, `getchar`, `printf` (`%d %s %c %x %f %%`), `puts`, `sprintf` (`%d %s %c %x %%`) |
| libc | `strlen`, `strcmp`, `strncmp`, `strcpy`, `strncpy`, `strcat`, `strncat`, `strchr`, `strrchr`, `strstr`, `memcpy`, `memset`, `memmove`, `memcmp`, `memchr`, `atoi`, `strtol`, `abs`, `rand`/`srand`, `isdigit`, `isalpha`, `isalnum`, `isspace`, `isupper`, `islower`, `isprint`, `ispunct`, `isxdigit`, `toupper`, `tolower`, `qsort`, `bsearch`, and more |
| Preprocessor | `#define` (object-like and function-like macros), `#include`, `#ifdef`/`#ifndef`/`#if`/`#elif`/`#else`/`#endif`, `#undef`, `#error`, `#pragma`, `defined()`, `__LINE__`, `__FILE__`, `__STDC__` |
| Initializers | Local/global array initializers, global `char *arr[] = {"..."}` string arrays, 2D array initializers |
| Storage class | `static` (function-scope with persistent state), `extern`, `register`/`auto`/`volatile` (accepted, ignored) |
| Variadic | `va_list`, `va_start`, `va_arg`, `va_end`, `...` in function params |

Integer types map to `i32`. Float types use `f64` in WASM (WAT mode). Unsigned types use appropriate unsigned WASM opcodes.

---

## How It Works

### Compiler Pipeline

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
│ WAT Codegen  │  Emits WAT text (to stdout or ByteVec buffer)
└────┬─────────┘
     │
     ├─── WAT mode ──→ stdout (human-readable WAT)
     │
     └─── Binary mode ──→ [WAT Assembler] ──→ stdout (WASM binary)
```

The compiler has two output modes, selected at runtime:
- **WAT mode** (default): `c2wasm < input.c` emits WebAssembly Text Format — human-readable, useful for debugging
- **Binary mode** (`-b` flag): `c2wasm -b < input.c` generates WAT internally, then assembles it to WASM binary — one unified codegen path

A standalone assembler (`c2wasm-asm`) is also available for the WAT → WASM step:
```
c2wasm < input.c | c2wasm-asm > output.wasm
```

### Browser Integration

- **Compiler → WASM**: compiled with [wasi-sdk](https://github.com/WebAssembly/wasi-sdk) to a standalone ~667 KB `.wasm` file (no JavaScript runtime bundled); its only runtime dependency is the standard `wasi_snapshot_preview1` interface
- **Unified WASI harness** (`wasi-harness.js`): a shared `createWasiHarness()` factory implements the full `wasi_snapshot_preview1` API and is used by both the compiler runner (`compiler-api.js`) and the user-program runner (`wasm-worker.js`)
- **Cryptographic randomness**: `random_get` is implemented with `crypto.getRandomValues()` (Web Crypto API) in both the compiler and user-program harness; generated programs have `rand()` automatically seeded from OS entropy at startup via `random_get`
- **Virtual filesystem**: the WASI harness implements `path_open`/`fd_read`/`fd_close` backed by an in-memory file map, enabling `#include` resolution in the browser for compiling multi-file projects
- **Live compiler editing**: switch to Compiler mode to modify the compiler source; the reference `compiler.wasm` compiles your changes into a custom compiler, which then compiles user programs
- **Direct binary output**: the browser compiler emits WASM binary directly — no `wabt.js` assembler needed
- **Execution**: `WebAssembly.instantiate` with the WASI harness (`wasm-worker.js`); supports stdin/stdout/stderr, `clock_time_get`, `random_get`, environment variables, and arguments; programs using `getchar` read from the stdin input box in the demo UI

### Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| Integer types → `i32`, float types → `f64` | Simplicity; covers all common use cases |
| Dual output: WAT + binary | WAT is readable; binary uses the same WAT codegen path plus an assembler |
| `printf` lowered at compile time | Format strings expanded to `putchar`/helper calls; no variadic WASM needed |
| Free-list allocator with coalescing | Real `free()` enables long-running programs and libc functions |
| 50+ inline libc functions | No external linking needed; every program is self-contained |
| Single-file compiler | Unity build (`c2wasm.c` #includes all files) maximizes self-hosting elegance |

---

## Self-Hosting Verification

The compiler can compile itself. `tools/bootstrap.sh` performs a 2-stage bootstrap with fixed-point verification:

```
Stage 1: GCC compiles c2wasm.c → native binary
         native binary compiles itself → s1.wat → s1.wasm

Stage 2: s1.wasm compiles c2wasm.c → s2.wat

diff s1.wat s2.wat  ← must be empty (fixed point)
```

A clean diff proves that the compiler, when run inside WebAssembly, produces byte-for-byte identical output to the GCC-compiled version. The bootstrap also runs the full test suite as a final step.

> **Note:** WAT-mode self-hosting uses `make bootstrap` to check the WAT fixed point. Binary-mode self-hosting is validated separately via `make bootstrap-binary`, which builds a self-hosted compiler in `-b` mode and uses it to compile a smoke-test program to WASM binary.

---

## Local Development

There are **four binaries** in this project:

| Binary | What it is |
|--------|-----------|
| `build/c2wasm` | Native compiler — WAT output by default, binary with `-b` |
| `build/c2wasm-asm` | Standalone WAT → WASM assembler |
| `demo/compiler.wasm` | Browser compiler — same as native, compiled to WASM |
| `demo/assembler.wasm` | Browser WAT → WASM assembler (used by the demo's custom compiler pipeline) |

`make` builds all four (`compiler.wasm` and `assembler.wasm` require wasi-sdk).

`demo/compiler.wasm` and `demo/assembler.wasm` are gitignored — a fresh clone won't have them. You only need to rebuild them if you're working on the demo or changed the compiler and want to update the deployed binary.

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

### Build everything

```bash
make        # builds native compiler, native assembler, and demo/compiler.wasm (if wasi-sdk found)
make clean  # remove all build artifacts
```

> Without wasi-sdk, `make` still builds the native binaries and prints a notice. Run `make clean && make` to force a full rebuild.

### Run the tests

Requires `wat2wasm` and `wasmtime` on `PATH`:

```bash
make test            # WAT path: all 67 tests + bootstrap self-hosting check
make test-binary     # binary path: all 67 tests + binary bootstrap check
make test-pipeline   # pipeline path: c2wasm | c2wasm-asm (tests the toolchain pipeline)
make bootstrap       # run the 2-stage WAT bootstrap check alone
make bootstrap-binary # run the binary self-hosting bootstrap alone
```

### Rebuild the browser compiler

Only needed when you change `src/c2wasm.c` and want to update the demo:

```bash
make        # rebuilds compiler.wasm if source files changed (requires wasi-sdk)
```


### Serve the demo locally

```bash
make serve
# open http://localhost:8080
```

> `make serve` uses `demo/server.py` instead of `python3 -m http.server` because the demo requires
> `Cross-Origin-Opener-Policy` and `Cross-Origin-Embedder-Policy` headers to enable `SharedArrayBuffer`
> (interactive stdin). On GitHub Pages these headers are injected by `demo/coi-serviceworker.js`.

### Test Programs

`tests/programs/` contains 62 progressive test programs, from basic returns through structs, strings, switch/case, enums, typedef, function pointers, floats, multi-file includes, unions, goto/labels, function-like macros, 2D arrays, and varargs.

---

## Project Structure

```
c2wasm/
├── src/
│   ├── c2wasm.c              ← unity build entry (~39 lines, #includes everything)
│   ├── constants.h           ← limits, token/node kinds, forward decls (~156 lines)
│   ├── util.c                ← error reporting, string helpers (~46 lines)
│   ├── source.c              ← source buffer, read_source() (~25 lines)
│   ├── lexer.h / lexer.c     ← tokenizer, #include preprocessing (~1421 lines)
│   ├── types.h / types.c     ← type registries (~295 lines)
│   ├── ast.h / ast.c         ← AST node definition (~61 lines)
│   ├── parser.h / parser.c   ← recursive descent parser (~1933 lines)
│   ├── codegen.h             ← shared codegen declarations (~31 lines)
│   ├── codegen_shared.c      ← local tracking, loop labels, goto state machine (~219 lines)
│   ├── bytevec.h / bytevec.c ← growable byte buffer (~95 lines)
│   ├── output.h / output.c   ← WAT output abstraction (~98 lines)
│   ├── codegen_wat.c         ← WAT text emitter (~3920 lines)
│   ├── assembler.h / .c      ← WAT → WASM binary assembler (~1853 lines)
│   ├── file_io.c             ← native file I/O for #include (~45 lines)
│   ├── cli_main.c            ← native/WASI CLI wrapper with -b flag (~34 lines)
│   ├── assembler_main.c      ← standalone WAT → WASM assembler entry (~41 lines)
│   └── main.c                ← compiler entry point, mode dispatch (~42 lines)
├── libc/
│   ├── libc.h                ← standard library declarations for C programs
│   ├── libc.c                ← implementations of extended libc functions (~569 lines)
│   └── Makefile              ← builds demo/libc.wasm via wasi-sdk
├── tests/
│   ├── run_tests.sh          ← test runner (supports --binary and --pipeline flags)
│   └── programs/             ← 62 test programs + expected output
├── demo/
│   ├── index.html            ← browser UI
│   ├── main.js               ← editor, pipeline, localStorage file management
│   ├── compiler-api.js       ← WASI shim — loads and runs compiler.wasm (reference + custom modes)
│   ├── compiler-source.js    ← bundled compiler source files for in-browser editing
│   ├── compiler.wasm         ← compiled compiler (wasi-sdk, ~667 KB)
│   ├── assembler.wasm        ← compiled WAT→WASM assembler (wasi-sdk)
│   ├── examples/             ← 26 built-in C example programs
│   ├── wasm-worker.js        ← Web Worker for running compiled programs
│   └── style.css             ← VS Code-inspired dark theme
├── tools/
│   ├── bootstrap.sh          ← 2-stage self-hosting verification
│   └── bundle-source.js      ← bundles src/ files into demo/compiler-source.js
├── Makefile                  ← build, test, wasm, serve targets
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
