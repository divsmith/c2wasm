# c2wasm — Copilot Instructions

## Project Overview

c2wasm is a **self-hosting** C-to-WebAssembly compiler written in a curated C89 subset — the same subset the compiler itself can compile. It reads C from stdin and emits WAT (WebAssembly Text Format) to stdout. In the browser, the compiler is shipped as Emscripten WASM; [wabt.js](https://github.com/WebAssembly/wabt) assembles WAT to binary and runs it on the page.

See [README.md](../README.md) for the full feature list and architecture diagram.

---

## Build & Test

All commands run from `compiler/`:

```bash
cd compiler
make              # build native binary → compiler/c2wasm
make test         # run test suite (requires wat2wasm + wasmtime on PATH)
make wasm         # rebuild compiler.js via Emscripten (requires emcc)
make serve        # serve demo/ at http://localhost:8080 (requires Python 3)
make clean        # remove binary
```

**Quick smoke test without wat2wasm/wasmtime:**
```bash
cd compiler
echo 'int main() { return 42; }' | ./c2wasm   # prints WAT to stdout
```

**Test program anatomy** (`compiler/tests/programs/`):
- Files are numbered `NN_name.c`; tests run in glob order.
- Optional `// EXPECT_EXIT: N` on line 1 overrides the expected exit code (default 0).
- Optional `NN_name.expected` file is diffed against stdout.

---

## Architecture

Pipeline: **Lexer → Parser → Type Checker → Code Gen** — all in `compiler/src/c2wasm.c` (~2,800 lines).

| Layer | Key detail |
|-------|-----------|
| Lexer | Tokenizes identifiers, keywords, literals, operators |
| Parser | Recursive descent; Pratt-style operator precedence |
| Type Checker | Resolves symbols, computes struct offsets, annotates AST |
| Code Gen | Single-pass WAT emission; `printf` lowered to `putchar` calls |

**All types map to `i32` internally.** No floats, no 64-bit types.

---

## Critical Constraint: Self-Hosting

> **The compiler is written using only the C subset it can compile.**

When adding a language feature, the *implementation* of that feature in `c2wasm.c` must use only constructs the compiler already supports. New features must be added in dependency order.

Supported C subset at a glance: `int`, `char`, `void`, pointers, 1D arrays, structs, arithmetic/comparison/logical/bitwise operators, `if`/`else`, `while`, `for`, `return`, `break`, `continue`, `#define NAME integer-literal`, `malloc`/`free`/`exit`/`putchar`/`printf` builtins.

---

## Key Conventions

- **No new files** for compiler features — everything lives in `compiler/src/c2wasm.c`.
- **Add a test program** for every new language feature. Number it sequentially (e.g. `35_enum.c`). Add a `.expected` file if the program produces stdout.
- **Update `README.md`** for relevant changes in features, usage, or behavior so docs stay in sync with code.
- **`#define` only** — no enums, no `typedef` until those features are implemented and self-hosting verified.
- Token kinds, AST node kinds, and other constants are `#define` integer literals (not enums).
- `free()` is a no-op; use bump allocation only.

---

## Roadmap

Active development is tracked in [roadmap.md](../roadmap.md) and detailed specs live in [docs/superpowers/](../docs/superpowers/). Track A adds missing C89 features in dependency order:

A1 (missing operators) → A2 (do-while) → A3 (ternary) → A4 (switch) → A5 (sizeof) → A6 (enum) → A7 (typedef) → A8 (array initializers) → …

---

## External Dependencies

| Tool | Purpose | Required for |
|------|---------|-------------|
| GCC | Build native compiler | `make`, `make test` |
| `wat2wasm` | Assemble WAT → WASM | `make test` |
| `wasmtime` | Run WASM binaries | `make test` |
| `emcc` | Rebuild `compiler.js` | `make wasm` only |
| Python 3 | Serve demo | `make serve` only |

If `wat2wasm`/`wasmtime` are unavailable, you can still build and inspect WAT output manually.

---

## Browser Demo

`demo/` is deployed via GitHub Pages (`.github/workflows/pages.yml`). No build step — edit HTML/CSS/JS directly.

Key files:
- `demo/index.html` — Monaco editor + UI layout
- `demo/main.js` — compile pipeline, localStorage file management
- `demo/compiler-api.js` — Emscripten wrapper
- `demo/compiler.js` — compiled compiler (commit only when rebuilding intentionally)
