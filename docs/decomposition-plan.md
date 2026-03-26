# Plan: Decompose c2wasm.c — #include Support + Unified Pipeline + Multi-File Split

## Problem

`src/c2wasm.c` is 9,595 lines — a single monolithic file containing the entire compiler: lexer, parser, type system, WAT codegen, and a *duplicate* binary WASM emitter. This makes the codebase hard to navigate, reason about, and maintain. Roadmap item #14 calls for splitting it into logical components while maintaining the self-hosting requirement.

## Approach

Instead of concatenation, we implement **proper `#include` support** in the compiler's preprocessor, enabling a real multi-file C project layout with headers. We also **unify the codegen pipeline** by building an internal WAT → binary assembler, eliminating the ~3,500-line binary codegen that duplicates the WAT codegen logic.

Three enablers, then the decomposition:
1. **#include preprocessing** — enables multi-file compilation for self-hosting
2. **WAT assembler** — replaces the duplicate binary codegen with source → WAT → binary
3. **Multi-file split** — proper `.h`/`.c` decomposition using #include

---

## Current State Analysis

### Compiler sections (line counts):
| Section | Lines | Notes |
|---------|-------|-------|
| Constants (enums, limits) | 130 | Token kinds, node kinds, MAX_* |
| Utilities (error, strdupn) | 15 | |
| Source buffer | 23 | read_source() reads stdin |
| Lexer | 600 | Token struct, keywords, macros, strings, next_token |
| Type registries | 265 | StructDef, GlobalVar, FuncSig, FnPtrVar, EnumConst, TypeAlias |
| AST Node | 60 | Flat struct (no union) |
| Parser | 1,762 | Recursive descent, Pratt precedence |
| WAT Code Generator | 3,141 | 953 printf calls, inline libc helpers |
| Binary WASM Emitter | 3,544 | Parallel implementation of WAT codegen |
| Main | 35 | |
| **Total** | **9,595** | |

### Key findings:
- **953 printf calls** in WAT codegen use only 4 format specifiers: `%d` (58), `%s` (34), `%x` (1), `%c` (1). The rest are plain strings.
- **Binary codegen is a near-complete duplicate** of WAT codegen: same AST traversal, same builtins, just emitting bytes instead of text.
- **~115 global variables** shared across the compiler.
- **229+ occurrences** of `struct Node *` — prime typedef candidate.
- **WASI imports** already include `fd_read` and `fd_write`. Adding `path_open` and `fd_close` enables file I/O.
- **Preprocessor** currently only handles `#define NAME value`. Adding `#include "file"` extends the same dispatch point.
- **Self-hosting bootstrap** uses WAT mode only (native → s1.wat → s2.wat). Binary mode is not part of the bootstrap.
- The compiler already uses **enums, typedefs, switch, ternary** in its own source — all self-hostable.

---

## Phase 1: #include Support

**Goal:** Enable `#include "file.h"` in the compiler's preprocessor so the compiler can compile multi-file projects, including itself.

### 1a. File I/O Builtins

The compiler needs to read files from disk. Currently it only reads stdin. We add:

**WASI imports** (in gen_module WAT output):
```wat
(import "wasi_snapshot_preview1" "path_open" (func $__path_open ...))
(import "wasi_snapshot_preview1" "fd_close" (func $__fd_close (param i32) (result i32)))
```

**Builtin helper functions** in the WAT codegen (like putchar, malloc):
- `$__open_file (param $path i32) (param $path_len i32) (result i32)` — wraps `path_open`, returns fd
- `$__read_file (param $fd i32) (param $buf i32) (param $max_len i32) (result i32)` — wraps `fd_read`, returns bytes read
- `$__close_file (param $fd i32)` — wraps `fd_close`

These are exposed to C code as: `int __open_file(char *path, int len)`, `int __read_file(int fd, char *buf, int max_len)`, `void __close_file(int fd)`.

For the native GCC build: implement these as thin wrappers around `fopen`/`fread`/`fclose` (the native build links against system libc, so they just work as regular extern functions).

### 1b. #include Preprocessing

Extend the preprocessor (currently at line ~439 in next_token):

```
#include "filename.h"    — search relative to current file's directory
```

**Implementation:**
- **Source stack**: Push current `src`/`src_len`/`lex_pos`/`lex_line`/`lex_col` onto a stack, load the included file, continue lexing. Pop when EOF reached.
- **Include-once**: Track included filenames in a list. Skip if already seen. (Equivalent to `#pragma once` — avoids need for `#ifndef`/`#endif` guards for now.)
- **Include path**: Initially just relative to the source file. Can add `-I` flag later.
- **Stack depth limit**: Prevent infinite recursion (e.g., max 32 levels).

**Changes to compiler source:**
- Add `struct IncludeStack` with saved lexer state (~20 lines)
- Add include tracking array (~15 lines)
- Extend preprocessor dispatch in `next_token` to handle `#include` (~60 lines)
- Add file reading logic (~40 lines)
- Total: ~135 lines

### 1c. Test Programs

- `47_include.c` + `47_include.h` — test basic #include with shared declarations
- `48_multi_include.c` + helper headers — test multiple includes, include-once behavior
- Verify in both native and WASM (wasmtime with `--dir=.`)

### 1d. Binary Codegen File I/O

Since the bootstrap uses WAT mode only, we defer adding file I/O builtins to the binary codegen. Binary mode will still work for single-file programs. Tests using #include get `SKIP_BINARY` markers. The binary codegen is removed entirely in Phase 3 anyway.

---

## Phase 2: Multi-File Decomposition

**Goal:** Split c2wasm.c into proper `.h`/`.c` files using `#include`.

### Proposed file structure:

```
src/
├── c2wasm.c             (~30 lines)  - Unity build entry: #includes everything
├── constants.h          (~130 lines) - Token/node kind enums, MAX_* limits,
│                                        forward struct declarations, typedefs
├── util.h / util.c      (~40 lines)  - error(), strdupn()
├── source.h / source.c  (~25 lines)  - src buffer, read_source()
├── lexer.h / lexer.c    (~600 lines) - Token struct, lexer state, keywords,
│                                        macros, strings, next_token(), #include
├── types.h / types.c    (~265 lines) - StructDef, GlobalVar, FuncSig, FnPtrVar,
│                                        EnumConst, TypeAlias registries
├── ast.h / ast.c        (~60 lines)  - Node struct, NList, node_new(), helpers
├── parser.h / parser.c  (~1760 lines)- Forward decls, recursive descent, Pratt
├── codegen.h            (~30 lines)  - Shared codegen declarations (LocalVar, etc.)
├── codegen_shared.c     (~150 lines) - LocalVar, loop labels, elem_size helpers
├── codegen_wat.c        (~3000 lines)- WAT emitter, dispatch tables, builtin helpers
├── codegen_bin.c        (~3500 lines)- ByteVec, binary emitter (temporary, removed Phase 4)
└── main.c               (~35 lines)  - main(), init calls, mode dispatch
```

### Header design:

Headers contain:
- Struct definitions (so other files can use them)
- Function prototypes
- Extern declarations for shared globals
- Typedefs

Source files contain:
- `#include` of needed headers
- Global variable definitions
- Function implementations

### Build model — unity build with #include:

The root **`src/c2wasm.c`** `#include`s everything in order:

```c
/* c2wasm.c — single-translation-unit build entry point */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "util.h"
#include "util.c"
#include "source.h"
#include "source.c"
#include "lexer.h"
#include "lexer.c"
#include "types.h"
#include "types.c"
#include "ast.h"
#include "ast.c"
#include "parser.h"
#include "parser.c"
#include "codegen.h"
#include "codegen_shared.c"
#include "codegen_wat.c"
#include "codegen_bin.c"
#include "main.c"
```

This means:
- **GCC/wasi-sdk**: `#include` handled natively by the host compiler.
- **Self-hosting**: The compiler's own `#include` support (Phase 1) processes the same file.
- **One translation unit**: No linker needed — same compilation model as today.
- **Binary mode sed trick**: Still works (operates on `codegen_bin.c` or `main.c`).

### Typedefs (biggest simplification):

Add to `constants.h`:
```c
typedef struct Node Node;
typedef struct Token Token;
typedef struct ByteVec ByteVec;
typedef struct StructDef StructDef;
typedef struct GlobalVar GlobalVar;
typedef struct FuncSig FuncSig;
typedef struct FnPtrVar FnPtrVar;
typedef struct LocalVar LocalVar;
```

Mechanically replace `struct Node *` → `Node *` etc. across all files (**229+** occurrences for Node alone, **80+** for ByteVec).

### Migration strategy:

1. Create `c2wasm.c` entry point that just `#include`s the current monolith (no split yet — sanity check).
2. Extract headers first (struct defs, prototypes) — declarations only, no behavior change.
3. Extract `.c` files one section at a time, from bottom up (main → codegen → parser → ...).
4. After each extraction: verify with `make test`.
5. After all extractions: run `make bootstrap` for self-hosting verification.

---

## Phase 3: Output Abstraction

**Goal:** Decouple WAT codegen from stdout so its output can be captured for the assembler.

Replace the 953 `printf` calls in WAT codegen with output functions:

```c
/* Output abstraction — writes to stdout or ByteVec buffer */
ByteVec *wat_output;  /* NULL = write to stdout */

void out(char *s);          /* emit string */
void out_d(int val);        /* emit decimal int */
void out_c(int ch);         /* emit single char */
void out_x(int val);        /* emit hex int */
```

Transformation patterns (mechanical):
```c
/* Plain string (860 of 953 calls): */
printf("  (i32.load8_u (local.get $ptr))\n");
→ out("  (i32.load8_u (local.get $ptr))\n");

/* String with %d (58 calls): */
printf("(i32.const %d)\n", val);
→ out("(i32.const "); out_d(val); out(")\n");

/* String with %s (34 calls): */
printf("(local $%s i32)\n", name);
→ out("(local $"); out(name); out(" i32)\n");
```

When `wat_output` is NULL: `out()` calls `printf`, etc.
When `wat_output` is set: functions append to the ByteVec buffer.

---

## Phase 4: WAT → Binary Assembler

**Goal:** Replace the 3,544-line binary codegen with a WAT text parser + binary encoder, creating the unified source → WAT → binary pipeline.

### Architecture:

```
Source → [Lexer] → [Parser] → AST → [WAT Codegen] → WAT text in ByteVec
                                                           │
                                                    ┌──────┴──────┐
                                                    │             │
                                              WAT mode      Binary mode
                                                    │             │
                                              write to      [WAT Assembler]
                                               stdout            │
                                                           write binary
                                                            to stdout
```

### Components:

**WAT Tokenizer** (~150 lines):
- Tokenize S-expressions: `(`, `)`, keywords, `$name` identifiers, integers, floats, strings

**WAT Parser** (~500 lines):
- Parse module structure: imports, types, functions, memory, globals, data, exports, tables
- Parse instruction sequences (only the subset our compiler generates)
- Build an intermediate module representation

**Binary Encoder** (~500 lines):
- Reuse existing `ByteVec`, `bv_u32` (LEB128), `bv_i32`, `bv_section`, `bv_name` primitives
- Encode WASM sections: type, import, function, table, memory, global, export, element, code, data
- Resolve `$name` references to numeric indices

**Total**: ~1,150 lines new code

**Standalone mode**: Also expose as `c2wasm --assemble` that reads WAT from stdin and writes binary to stdout (roadmap item #15 foundation).

### Pipeline wiring:

```c
if (binary_mode) {
    wat_output = bv_new(65536);  /* capture WAT to buffer */
    gen_module(prog);            /* existing WAT codegen */
    assemble_wat(wat_output);    /* new assembler → binary stdout */
} else {
    wat_output = NULL;           /* write directly to stdout */
    gen_module(prog);
}
```

### Validation:

Before removing binary codegen, validate the assembler produces **byte-identical output** for all test programs against the old binary codegen.

---

## Phase 5: Remove Binary Codegen

Delete:
- `gen_module_bin` and all `gen_*_bin` functions (~3,544 lines)
- `BinTypeEntry`, `BinFuncEntry`, `bin_*` globals
- Binary dispatch tables

Keep:
- `ByteVec` and encoding primitives (used by the assembler)
- `binary_mode` flag (now controls WAT-to-buffer vs WAT-to-stdout)

**Net result**: ~3,544 lines removed, ~1,150 lines added = **~2,400 fewer lines**.

The `codegen_bin.c` file is replaced by `assembler.h` / `assembler.c`.

---

## Phase 6: Simplification with New Features

Apply across all files now that they're small and manageable:

1. **Typedefs** (already done in Phase 2)
2. **Switch statements**: Convert remaining if-else chains in parser/codegen to switch where the discriminant is a token kind or node kind
3. **Ternary**: Replace simple `if (x) a = 1; else a = 0;` → `a = x ? 1 : 0;`
4. **Clean up shared state**: Ensure globals are in the right files, minimize cross-file dependencies

---

## Phase 7: Documentation & CI

- Update `README.md` with new project structure, build instructions, architecture diagram
- Update `.github/copilot-instructions.md` to reflect multi-file layout
- Update `tools/bootstrap.sh` for multi-file source (may need `--dir=.` for wasmtime)
- Update CI workflows if they reference `src/c2wasm.c` directly
- Mark roadmap items #14 (multi-file) and #15 (toolchain) as done

---

## Final File Structure

```
src/
├── c2wasm.c             (~30 lines)  - Unity build entry: #includes everything
├── constants.h          (~140 lines) - Enums, limits, forward decls, typedefs
├── util.h / util.c      (~40 lines)  - Error reporting, string helpers
├── source.h / source.c  (~25 lines)  - Source buffer, read_source()
├── lexer.h / lexer.c    (~640 lines) - Tokenizer, #include preprocessing
├── types.h / types.c    (~265 lines) - All type registries
├── ast.h / ast.c        (~60 lines)  - AST node definition
├── parser.h / parser.c  (~1760 lines)- Recursive descent parser
├── codegen.h            (~30 lines)  - Shared codegen declarations
├── codegen_shared.c     (~150 lines) - Local tracking, loop labels, helpers
├── codegen_wat.c        (~3000 lines)- WAT emitter (uses out() abstraction)
├── assembler.h / .c     (~1150 lines)- WAT parser + binary encoder
└── main.c               (~35 lines)  - Entry point, mode dispatch
```

**Total: ~7,325 lines across 20 files** (down from 9,595 in 1 file)

---

## Risks & Mitigations

| Risk | Mitigation |
|------|-----------|
| #include self-hosting | Test with GCC first (native #include), then verify self-hosted bootstrap. Phase 1 adds this before Phase 2 uses it. |
| Assembler output mismatch | Byte-for-byte validation against old binary codegen before removal. Run full test suite in binary mode. |
| 953 printf replacements | Mechanical transformation with clear patterns. Script-assisted where possible. Run tests after each batch. |
| WASI file I/O complexity | WASI `path_open` is well-documented. `fd_read`/`fd_close` already imported or straightforward. Incremental testing with wasmtime. |
| Include-once vs include guards | Include-once is simpler and sufficient. `#ifndef`/`#endif` can be added later under C89 track. |
| Browser demo with #include | Demo compiles single-file user programs (no #include needed). compiler.wasm is built by wasi-sdk which handles #include natively. No demo changes needed. |

---

## What's NOT in Scope

- `#ifndef`/`#ifdef`/`#endif` conditional compilation (future, under C89 track)
- System includes (`#include <stdlib.h>`) in the compiler's own preprocessor — host compiler handles these
- Separate compilation + linking (roadmap #16)
- Changes to the browser demo UI
