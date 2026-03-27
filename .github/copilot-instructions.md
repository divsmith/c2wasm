# c2wasm — Copilot Instructions

## Project Overview

c2wasm is a **self-hosting** C-to-WebAssembly compiler written in a curated C89 subset — the same subset the compiler itself can compile. It reads C from stdin and emits WAT (WebAssembly Text Format) or WASM binary to stdout. In the browser, the compiler is shipped as a WASI WASM binary built with [wasi-sdk](https://github.com/WebAssembly/wasi-sdk).

See [README.md](../README.md) for the full feature list and architecture diagram.

---

## Build & Test

All commands run from the project root:

```bash
make              # build native binary → build/c2wasm
make test         # run test suite + bootstrap (requires wat2wasm + wasmtime on PATH)
make test-binary  # run test suite in binary mode (assembler path)
make wasm         # rebuild compiler.wasm via wasi-sdk (requires wasi-sdk)
make serve        # serve demo/ at http://localhost:8080 (requires Python 3)
make clean        # remove binary
```

**Quick smoke test without wat2wasm/wasmtime:**
```bash
echo 'int main() { return 42; }' | ./build/c2wasm   # prints WAT to stdout
```

**Test program anatomy** (`tests/programs/`):
- Files are numbered `NN_name.c`; tests run in glob order.
- Optional `// EXPECT_EXIT: N` on line 1 overrides the expected exit code (default 0).
- Optional `NN_name.expected` file is diffed against stdout.

---

## Architecture

The compiler is a **unity build**: `src/c2wasm.c` `#include`s all source files. The pipeline is:

**Lexer → Parser → Type Checker → WAT Codegen → (optional) WAT Assembler**

| Layer | File(s) | Key detail |
|-------|---------|-----------|
| Lexer | `lexer.h/c` | Tokenizes identifiers, keywords, literals, operators; handles `#include` |
| Parser | `parser.h/c` | Recursive descent; Pratt-style operator precedence |
| Type Checker | `types.h/c` | Resolves symbols, computes struct offsets, annotates AST |
| WAT Codegen | `codegen_wat.c` | WAT emission via `out()`/`out_d()`/`out_c()` abstraction |
| Assembler | `assembler.h/c` | Two-pass WAT text → WASM binary encoder (used in binary mode) |
| Output | `output.h/c` | Abstraction: writes to stdout (WAT mode) or ByteVec buffer (binary mode) |

In binary mode, WAT codegen writes to a ByteVec buffer, then the assembler converts it to WASM binary. Both modes share identical codegen.

Integer types map to `i32`. Float types use `f64`. Unsigned types use appropriate unsigned WASM opcodes.

---

## Critical Constraint: Self-Hosting

> **The compiler is written using only the C subset it can compile.**

When adding a language feature, the *implementation* must use only constructs the compiler already supports. New features must be added in dependency order.

**Not supported in compiler source**: `goto`, `fprintf`, `stderr`, `unsigned`, `long`, `static`, `const`, `union`, system headers.

Supported C subset at a glance: `int`, `char`, `void`, pointers, 1D arrays, structs, `enum`, `typedef`, arithmetic/comparison/logical/bitwise operators, `if`/`else`, `while`, `for`, `do`/`while`, `switch`/`case`, `return`, `break`, `continue`, `#define`, `#include "file"`, `malloc`/`free`/`exit`/`putchar`/`printf` builtins.

---

## Key Conventions

- **Unity build** — `src/c2wasm.c` `#include`s all `.c` files. Add new files to the include chain there.
- **Add a test program** for every new language feature. Number it sequentially (e.g. `51_feature.c`). Add a `.expected` file if the program produces stdout.
- **Add a demo example** for every new language feature: add an entry to `EXAMPLES` in `demo/main.js` and a matching `<option>` in the `<optgroup label="Examples">` in `demo/index.html`. The example must produce visible `printf` output, compile cleanly through the native binary (`./build/c2wasm`), and run correctly under `wasmtime`.
- **Update `README.md`** for relevant changes in features, usage, or behavior so docs stay in sync with code.
- Token kinds, AST node kinds, and other constants are `#define` integer literals in `constants.h`.
- `free()` uses a free-list allocator with coalescing.
- **Test in browser** by building and running the serve command and using agent-browser to validate the demo still works.

---

## Roadmap

Active development is tracked in [roadmap.md](../roadmap.md) and detailed specs live in [docs/superpowers/](../docs/superpowers/). Track A adds missing C89 features in dependency order:

A1 (missing operators) → A2 (do-while) → A3 (ternary) → A4 (switch) → A5 (sizeof) → A6 (enum) → A7 (typedef) → A8 (array initializers) → …

---

## External Dependencies

| Tool | Purpose | Required for |
|------|---------|-------------|
| GCC/Clang | Build native compiler | `make`, `make test` |
| `wat2wasm` | Assemble WAT → WASM | `make test` (WAT mode) |
| `wasmtime` | Run WASM binaries | `make test`, `make test-binary` |
| [wasi-sdk](https://github.com/WebAssembly/wasi-sdk) | Rebuild `compiler.wasm` | `make wasm` only |
| Python 3 | Serve demo | `make serve` only |

If `wat2wasm`/`wasmtime` are unavailable, you can still build and inspect WAT output manually.

---

## Browser Demo

`demo/` is deployed via GitHub Pages (`.github/workflows/pages.yml`). No build step — edit HTML/CSS/JS directly.

Key files:
- `demo/index.html` — Monaco editor + UI layout
- `demo/main.js` — compile pipeline, localStorage file management
- `demo/compiler-api.js` — WASI shim
- `demo/compiler.wasm` — compiled compiler (wasi-sdk binary mode)
