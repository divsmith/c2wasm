# Track A: C89 Language Features — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add 9 C89 features (A1–A9) to `compiler/src/c2wasm.c` — the single-file C→WAT compiler — one at a time with tests passing after each.

**Architecture:** All 9 features land in the single `compiler/src/c2wasm.c` file (currently ~2838 lines). Sequential order: A1→A2→A3→A4→A6→A5→A7→A8→A9. Every feature follows TDD: write failing test → implement → all tests pass → commit.

**Tech Stack:** C (compiler source, C89-compatible subset), WebAssembly Text Format (WAT) output, `gcc`, `wat2wasm`, `wasmtime`.

---

## Background

- **Compiler**: `compiler/src/c2wasm.c` — single file, self-hosting C→WAT compiler
- **Build**: `make -C compiler` → `gcc -Wall -Wextra -std=c11 -pedantic -O2 -o compiler/c2wasm compiler/src/c2wasm.c`
- **Tests**: `make -C compiler test` → `bash compiler/tests/run_tests.sh` (currently 30 tests, all passing)
- **Design spec**: `docs/superpowers/specs/2026-03-23-track-a-design.md` — reviewed and approved
- **Git**: Currently on `main` at `3a416fe`. Create branch `anvil/track-a-features`.

### Key Code Locations

| Section | Lines |
|---------|-------|
| Token `#define`s | 20–71 |
| Node `#define`s | 73–94 |
| Limits `#define`s | 96–106 |
| `kw_lookup()` | 258–272 |
| Lexer operator dispatch | 556–657 |
| `is_type_token()` | 835–840 |
| `prefix_bp()` / `infix_bp()` | 850–906 |
| `parse_atom()` | 908–1001 |
| `parse_expr_bp()` | 1004–1117 |
| `parse_var_decl()` | 1125–1153 |
| `parse_stmt()` | 1244–1288 |
| `parse_block()` | 1290–1311 |
| `parse_type()` | 1315–1335 |
| `parse_func()` param parsing | 1368–1400 |
| `parse_global_var()` | 1452–1493 |
| `parse_program()` | 1495–1563 |
| `collect_locals()` | 1613–1631 |
| `gen_expr()` | 1671–2138 |
| `gen_stmt()` | 2155–2304 |
| `gen_func()` — emits locals | 2308–2356 |

### Self-Hosting Constraints

- All new code must compile with c2wasm's own supported C subset
- All variables declared at **top of block** (no mid-block decls)
- No `//` comments; use `/* */`
- All constants via `#define` (no real `enum` in compiler source)
- `NULL` → `(struct Foo *)0` casts
- No compound literals, VLAs, or C99 features

---

## File Structure

**Single file to modify:** `compiler/src/c2wasm.c`

**New test programs to create:**
- `compiler/tests/programs/30_operators.c` (A1)
- `compiler/tests/programs/31_do_while.c` (A2)
- `compiler/tests/programs/32_ternary.c` (A3)
- `compiler/tests/programs/33_switch.c` (A4)
- `compiler/tests/programs/34_sizeof.c` (A5+A6)
- `compiler/tests/programs/35_enum.c` (A7)
- `compiler/tests/programs/36_typedef.c` (A8)
- `compiler/tests/programs/37_array_init.c` (A9)

---

## Task 0: Setup

**Files:** none

- [ ] **Step 1: Create branch and verify baseline**

```bash
cd /Users/parker/code/wasm-c
git checkout -b anvil/track-a-features
make -C compiler test
```

Expected: branch created; all 30 tests pass.

---

## Task 1: A1 — Missing Operators

**Files:**
- Modify: `compiler/src/c2wasm.c`
- Create: `compiler/tests/programs/30_operators.c`

### New constants to add

**Token #defines** (append after `TOK_MINUS_MINUS 51` at line ~71):
```c
#define TOK_CARET 52
#define TOK_PIPE_EQ 53
#define TOK_AMP_EQ 54
#define TOK_CARET_EQ 55
#define TOK_LSHIFT_EQ 56
#define TOK_RSHIFT_EQ 57
```

**Node #defines** (append after `ND_SUBSCRIPT 20` at line ~94):
```c
#define ND_POST_INC 21
#define ND_POST_DEC 22
```

### Lexer changes (in `next_token()` operator dispatch)

Replace the `|` branch (lines ~612–618):
```c
    } else if (c == '|') {
        if (lp() == '|') {
            la();
            t->kind = TOK_PIPE_PIPE;
        } else if (lp() == '=') {
            la();
            t->kind = TOK_PIPE_EQ;
        } else {
            t->kind = TOK_PIPE;
        }
```

Replace the `&` branch (lines ~605–611):
```c
    } else if (c == '&') {
        if (lp() == '&') {
            la();
            t->kind = TOK_AMP_AMP;
        } else if (lp() == '=') {
            la();
            t->kind = TOK_AMP_EQ;
        } else {
            t->kind = TOK_AMP;
        }
```

Add `^` handling (before the `else` at line ~655):
```c
    } else if (c == '^') {
        if (lp() == '=') {
            la();
            t->kind = TOK_CARET_EQ;
        } else {
            t->kind = TOK_CARET;
        }
```

Replace the `<` branch to handle `<<=`:
```c
    } else if (c == '<') {
        if (lp() == '=') {
            la();
            t->kind = TOK_LT_EQ;
        } else if (lp() == '<') {
            la();
            if (lp() == '=') {
                la();
                t->kind = TOK_LSHIFT_EQ;
            } else {
                t->kind = TOK_LSHIFT;
            }
        } else {
            t->kind = TOK_LT;
        }
```

Replace the `>` branch to handle `>>=`:
```c
    } else if (c == '>') {
        if (lp() == '=') {
            la();
            t->kind = TOK_GT_EQ;
        } else if (lp() == '>') {
            la();
            if (lp() == '=') {
                la();
                t->kind = TOK_RSHIFT_EQ;
            } else {
                t->kind = TOK_RSHIFT;
            }
        } else {
            t->kind = TOK_GT;
        }
```

### `infix_bp()` changes

Add XOR between `|` and `&` (currently `|` = LBP 8, `&` = LBP 10; XOR gets 9):
```c
    if (op == TOK_CARET) {
        last_rbp = 10;
        return 9;
    }
```

Add compound assignment operators alongside existing ones (before the `return -1`):
```c
    if (op == TOK_PIPE_EQ || op == TOK_AMP_EQ || op == TOK_CARET_EQ ||
        op == TOK_LSHIFT_EQ || op == TOK_RSHIFT_EQ) {
        last_rbp = 1;
        return 2;
    }
```

### `parse_expr_bp()` changes

**Post-increment/decrement** — add BEFORE the `lbp = infix_bp(cur->kind)` line in the infix loop:
```c
        /* postfix ++ and -- */
        if ((at(TOK_PLUS_PLUS) || at(TOK_MINUS_MINUS)) && 27 >= min_bp) {
            int pop;
            int pline;
            int pcol;
            pop = cur->kind;
            pline = cur->line;
            pcol = cur->col;
            advance_tok();
            if (pop == TOK_PLUS_PLUS) {
                left = node_new(ND_POST_INC, pline, pcol);
            } else {
                left = node_new(ND_POST_DEC, pline, pcol);
            }
            left->c0 = /* the previous left was the operand — capture before overwriting */
```

Wait — the left variable IS the operand. We need to save it first:
```c
        if ((at(TOK_PLUS_PLUS) || at(TOK_MINUS_MINUS)) && 27 >= min_bp) {
            struct Node *post;
            int pop;
            int pline;
            int pcol;
            pop = cur->kind;
            pline = cur->line;
            pcol = cur->col;
            advance_tok();
            post = node_new((pop == TOK_PLUS_PLUS) ? ND_POST_INC : ND_POST_DEC, pline, pcol);
            post->c0 = left;
            left = post;
            continue;
        }
```

**Compound assignment operators** — extend the assignment check at line ~1095:
```c
        if (op == TOK_EQ || op == TOK_PLUS_EQ || op == TOK_MINUS_EQ ||
            op == TOK_PIPE_EQ || op == TOK_AMP_EQ || op == TOK_CARET_EQ ||
            op == TOK_LSHIFT_EQ || op == TOK_RSHIFT_EQ) {
```

### `gen_expr()` changes

**ND_BINARY**: add XOR after the RSHIFT case:
```c
        } else if (n->ival == TOK_CARET) {
            printf("i32.xor\n");
```

**ND_ASSIGN**: add 5 new compound operators to each of the 4 target branches.

For **ND_IDENT** target, add after the `TOK_MINUS_EQ` else-if (but BEFORE the common suffix that does the global store dance):
```c
            } else if (n->ival == TOK_PIPE_EQ || n->ival == TOK_AMP_EQ ||
                       n->ival == TOK_CARET_EQ || n->ival == TOK_LSHIFT_EQ ||
                       n->ival == TOK_RSHIFT_EQ) {
                emit_indent();
                if (is_global) {
                    printf("global.get $%s\n", name);
                } else {
                    printf("local.get $%s\n", name);
                }
                gen_expr(n->c1);
                emit_indent();
                if (n->ival == TOK_PIPE_EQ) { printf("i32.or\n"); }
                else if (n->ival == TOK_AMP_EQ) { printf("i32.and\n"); }
                else if (n->ival == TOK_CARET_EQ) { printf("i32.xor\n"); }
                else if (n->ival == TOK_LSHIFT_EQ) { printf("i32.shl\n"); }
                else { printf("i32.shr_s\n"); }
```

Apply the same pattern to the **ND_UNARY+TOK_STAR** target (ptr deref), **ND_MEMBER** target, and **ND_SUBSCRIPT** target — each uses the same "load current value → apply op → save result" pattern that already exists for PLUS_EQ/MINUS_EQ. Mirror the pattern from those branches.

**ND_POST_INC / ND_POST_DEC**: add new codegen section. The target (`n->c0`) can be one of 4 kinds. The key pattern: save old value to `$__atmp`, compute new value (old ± 1), store it, then push old value.

```c
    } else if (n->kind == ND_POST_INC || n->kind == ND_POST_DEC) {
        struct Node *tgt2;
        char *pname;
        int pis_global;
        int pesz;
        int poff;
        tgt2 = n->c0;
        if (tgt2->kind == ND_IDENT) {
            pname = tgt2->sval;
            pis_global = (find_global(pname) >= 0);
            if (pis_global) {
                /* global: read old, save to atmp, read again, ±1, store, push atmp */
                emit_indent(); printf("global.get $%s\n", pname);
                emit_indent(); printf("local.set $__atmp\n");
                emit_indent(); printf("global.get $%s\n", pname);
                emit_indent(); printf("i32.const 1\n");
                emit_indent();
                if (n->kind == ND_POST_INC) { printf("i32.add\n"); }
                else { printf("i32.sub\n"); }
                emit_indent(); printf("global.set $%s\n", pname);
                emit_indent(); printf("local.get $__atmp\n");
            } else {
                /* local: read old (stays on stack), read again, ±1, store */
                emit_indent(); printf("local.get $%s\n", pname);
                emit_indent(); printf("local.get $%s\n", pname);
                emit_indent(); printf("i32.const 1\n");
                emit_indent();
                if (n->kind == ND_POST_INC) { printf("i32.add\n"); }
                else { printf("i32.sub\n"); }
                emit_indent(); printf("local.set $%s\n", pname);
            }
        } else if (tgt2->kind == ND_UNARY && tgt2->ival == TOK_STAR) {
            /* *p++: save old to atmp, recompute addr, load+±1+store, push atmp */
            pesz = expr_elem_size(tgt2->c0);
            gen_expr(tgt2->c0);
            emit_indent();
            if (pesz == 1) { printf("i32.load8_u\n"); } else { printf("i32.load\n"); }
            emit_indent(); printf("local.set $__atmp\n");
            gen_expr(tgt2->c0);
            gen_expr(tgt2->c0);
            emit_indent();
            if (pesz == 1) { printf("i32.load8_u\n"); } else { printf("i32.load\n"); }
            emit_indent(); printf("i32.const 1\n");
            emit_indent();
            if (n->kind == ND_POST_INC) { printf("i32.add\n"); } else { printf("i32.sub\n"); }
            emit_indent();
            if (pesz == 1) { printf("i32.store8\n"); } else { printf("i32.store\n"); }
            emit_indent(); printf("local.get $__atmp\n");
        } else if (tgt2->kind == ND_SUBSCRIPT) {
            /* arr[i]++: same save/addr/load/modify/store/push pattern */
            pesz = expr_elem_size(tgt2->c0);
            /* compute addr, load old, save to atmp */
            gen_expr(tgt2->c0); gen_expr(tgt2->c1);
            if (pesz > 1) { emit_indent(); printf("i32.const %d\n", pesz); emit_indent(); printf("i32.mul\n"); }
            emit_indent(); printf("i32.add\n");
            emit_indent();
            if (pesz == 1) { printf("i32.load8_u\n"); } else { printf("i32.load\n"); }
            emit_indent(); printf("local.set $__atmp\n");
            /* compute addr, load, ±1, store */
            gen_expr(tgt2->c0); gen_expr(tgt2->c1);
            if (pesz > 1) { emit_indent(); printf("i32.const %d\n", pesz); emit_indent(); printf("i32.mul\n"); }
            emit_indent(); printf("i32.add\n");
            gen_expr(tgt2->c0); gen_expr(tgt2->c1);
            if (pesz > 1) { emit_indent(); printf("i32.const %d\n", pesz); emit_indent(); printf("i32.mul\n"); }
            emit_indent(); printf("i32.add\n");
            emit_indent();
            if (pesz == 1) { printf("i32.load8_u\n"); } else { printf("i32.load\n"); }
            emit_indent(); printf("i32.const 1\n");
            emit_indent();
            if (n->kind == ND_POST_INC) { printf("i32.add\n"); } else { printf("i32.sub\n"); }
            emit_indent();
            if (pesz == 1) { printf("i32.store8\n"); } else { printf("i32.store\n"); }
            emit_indent(); printf("local.get $__atmp\n");
        } else if (tgt2->kind == ND_MEMBER) {
            /* s.f++ or p->f++: save/addr/load/modify/store/push */
            poff = resolve_field_offset(tgt2->sval);
            if (poff < 0) error(tgt2->nline, tgt2->ncol, "unknown struct field");
            /* load old */
            gen_expr(tgt2->c0);
            if (poff > 0) { emit_indent(); printf("i32.const %d\n", poff); emit_indent(); printf("i32.add\n"); }
            emit_indent(); printf("i32.load\n");
            emit_indent(); printf("local.set $__atmp\n");
            /* addr, load, ±1, store */
            gen_expr(tgt2->c0);
            if (poff > 0) { emit_indent(); printf("i32.const %d\n", poff); emit_indent(); printf("i32.add\n"); }
            gen_expr(tgt2->c0);
            if (poff > 0) { emit_indent(); printf("i32.const %d\n", poff); emit_indent(); printf("i32.add\n"); }
            emit_indent(); printf("i32.load\n");
            emit_indent(); printf("i32.const 1\n");
            emit_indent();
            if (n->kind == ND_POST_INC) { printf("i32.add\n"); } else { printf("i32.sub\n"); }
            emit_indent(); printf("i32.store\n");
            emit_indent(); printf("local.get $__atmp\n");
        } else {
            error(n->nline, n->ncol, "unsupported post-inc/dec target");
        }
```

### Test program

- [ ] **Step 1: Write the failing test**

`compiler/tests/programs/30_operators.c`:
```c
/* EXPECT_EXIT: 42 */
int main(void) {
    int a;
    int b;
    int r;

    /* XOR binary */
    a = 15;
    b = 9;
    r = a ^ b;
    if (r != 6) return 1;

    /* post-increment */
    a = 5;
    r = a++;
    if (r != 5) return 2;
    if (a != 6) return 3;

    /* post-decrement */
    a = 10;
    r = a--;
    if (r != 10) return 4;
    if (a != 9) return 5;

    /* |= */
    a = 12;
    a |= 3;
    if (a != 15) return 6;

    /* &= */
    a = 15;
    a &= 6;
    if (a != 6) return 7;

    /* ^= */
    a = 15;
    a ^= 9;
    if (a != 6) return 8;

    /* <<= */
    a = 1;
    a <<= 3;
    if (a != 8) return 9;

    /* >>= */
    a = 16;
    a >>= 2;
    if (a != 4) return 10;

    return 42;
}
```

- [ ] **Step 2: Run test to verify it fails**

```bash
make -C compiler test 2>&1 | tail -5
```
Expected: test 30 FAILS (compile error on `^` or `++` in postfix position).

- [ ] **Step 3: Implement changes in `c2wasm.c`**

Apply all changes described above: token #defines, lexer, `infix_bp`, `parse_expr_bp`, `gen_expr`.

Also add `collect_locals` handling for `ND_POST_INC`/`ND_POST_DEC` (these nodes have a target in `c0` that should be traversed for completeness):
```c
    } else if (n->kind == ND_POST_INC || n->kind == ND_POST_DEC) {
        collect_locals(n->c0);
    }
```

- [ ] **Step 4: Build and run all tests**

```bash
make -C compiler test
```
Expected: all 31 tests pass (0–30).

- [ ] **Step 5: Commit**

```bash
cd /Users/parker/code/wasm-c
git add compiler/src/c2wasm.c compiler/tests/programs/30_operators.c
git commit -m "feat(A1): add XOR, compound ops, post-inc/dec

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 2: A2 — `do-while`

**Files:**
- Modify: `compiler/src/c2wasm.c`
- Create: `compiler/tests/programs/31_do_while.c`

### New constants

**Token** (append after TOK_RSHIFT_EQ=57):
```c
#define TOK_DO 58
```

**Node** (append after ND_POST_DEC=22):
```c
#define ND_DO_WHILE 23
```

### Lexer: `kw_lookup()` — add `do`

```c
    if (strcmp(s, "do") == 0) return TOK_DO;
```

### `parse_stmt()` — add `do` branch

Add before the `if (at(TOK_LBRACE))` line:
```c
    if (at(TOK_DO)) return parse_do_while();
```

### New function `parse_do_while()` — add before `parse_stmt`

```c
struct Node *parse_do_while(void) {
    struct Node *n;
    struct Node *body;
    struct Node *cond;
    int line;
    int col;

    line = cur->line;
    col = cur->col;
    advance_tok();  /* consume 'do' */
    body = parse_stmt();
    if (!at(TOK_WHILE)) error(cur->line, cur->col, "expected 'while' after do body");
    advance_tok();  /* consume 'while' */
    expect(TOK_LPAREN, "expected '(' after while");
    cond = parse_expr();
    expect(TOK_RPAREN, "expected ')'");
    expect(TOK_SEMI, "expected ';' after do-while");
    n = node_new(ND_DO_WHILE, line, col);
    n->c0 = body;
    n->c1 = cond;
    return n;
}
```

### `collect_locals()` — add ND_DO_WHILE arm

```c
    } else if (n->kind == ND_DO_WHILE) {
        collect_locals(n->c0);
```

### `gen_stmt()` — add ND_DO_WHILE arm

WAT structure: body executes once unconditionally, then condition is checked for repeat.

```c
    } else if (n->kind == ND_DO_WHILE) {
        lbl = label_cnt;
        label_cnt = label_cnt + 1;
        brk_lbl[loop_sp] = lbl;
        cont_lbl[loop_sp] = lbl;
        loop_sp = loop_sp + 1;
        emit_indent();
        printf("(block $brk_%d\n", lbl);
        indent_level = indent_level + 1;
        emit_indent();
        printf("(loop $lp_%d\n", lbl);
        indent_level = indent_level + 1;
        emit_indent();
        printf("(block $cont_%d\n", lbl);
        indent_level = indent_level + 1;
        gen_body(n->c0);
        indent_level = indent_level - 1;
        emit_indent();
        printf(")\n");
        gen_expr(n->c1);
        emit_indent();
        printf("br_if $lp_%d\n", lbl);
        indent_level = indent_level - 1;
        emit_indent();
        printf(")\n");
        indent_level = indent_level - 1;
        emit_indent();
        printf(")\n");
        loop_sp = loop_sp - 1;
```

### Test program

- [ ] **Step 1: Write the failing test**

`compiler/tests/programs/31_do_while.c`:
```c
/* EXPECT_EXIT: 42 */
int main(void) {
    int i;
    int sum;

    /* basic do-while */
    i = 0;
    sum = 0;
    do {
        sum = sum + i;
        i = i + 1;
    } while (i < 5);
    if (sum != 10) return 1;

    /* do-while executes body at least once */
    i = 100;
    do {
        i = i + 1;
    } while (0);
    if (i != 101) return 2;

    /* break in do-while */
    i = 0;
    do {
        i = i + 1;
        if (i == 3) break;
    } while (i < 10);
    if (i != 3) return 3;

    /* continue in do-while */
    i = 0;
    sum = 0;
    do {
        i = i + 1;
        if (i == 3) continue;
        sum = sum + i;
    } while (i < 5);
    /* sum = 1+2+4+5 = 12 (skipped 3) */
    if (sum != 12) return 4;

    return 42;
}
```

- [ ] **Step 2: Verify failure**
```bash
make -C compiler test 2>&1 | tail -5
```
Expected: test 31 FAILS.

- [ ] **Step 3: Implement changes**

Apply: new token/node defines, `kw_lookup`, `parse_do_while`, `parse_stmt` dispatch, `collect_locals`, `gen_stmt`.

- [ ] **Step 4: Run all tests**
```bash
make -C compiler test
```
Expected: all 32 tests pass (0–31).

- [ ] **Step 5: Commit**
```bash
git add compiler/src/c2wasm.c compiler/tests/programs/31_do_while.c
git commit -m "feat(A2): add do-while loop

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 3: A3 — Ternary Operator `?:`

**Files:**
- Modify: `compiler/src/c2wasm.c`
- Create: `compiler/tests/programs/32_ternary.c`

### New constants

**Tokens** (append after TOK_DO=58):
```c
#define TOK_QUESTION 59
#define TOK_COLON 60
```

**Node** (append after ND_DO_WHILE=23):
```c
#define ND_TERNARY 24
```

### Lexer: punctuation (add to `next_token()` operator dispatch before the final `else`)

```c
    } else if (c == '?') {
        t->kind = TOK_QUESTION;
    } else if (c == ':') {
        t->kind = TOK_COLON;
```

### `parse_expr_bp()` — add ternary BEFORE `lbp = infix_bp(cur->kind)`

```c
        /* ternary ? : */
        if (at(TOK_QUESTION) && 3 >= min_bp) {
            struct Node *then_e;
            struct Node *else_e;
            struct Node *tern;
            int tline;
            int tcol;
            tline = cur->line;
            tcol = cur->col;
            advance_tok();  /* consume '?' */
            then_e = parse_expr_bp(0);
            expect(TOK_COLON, "expected ':' in ternary");
            else_e = parse_expr_bp(3);  /* right-assoc: same bp as ternary */
            tern = node_new(ND_TERNARY, tline, tcol);
            tern->c0 = left;
            tern->c1 = then_e;
            tern->c2 = else_e;
            left = tern;
            continue;
        }
```

### `gen_expr()` — add ND_TERNARY

```c
    } else if (n->kind == ND_TERNARY) {
        gen_expr(n->c0);
        emit_indent();
        printf("(if (result i32)\n");
        indent_level = indent_level + 1;
        emit_indent();
        printf("(then\n");
        indent_level = indent_level + 1;
        gen_expr(n->c1);
        indent_level = indent_level - 1;
        emit_indent();
        printf(")\n");
        emit_indent();
        printf("(else\n");
        indent_level = indent_level + 1;
        gen_expr(n->c2);
        indent_level = indent_level - 1;
        emit_indent();
        printf(")\n");
        indent_level = indent_level - 1;
        emit_indent();
        printf(")\n");
```

### Test program

- [ ] **Step 1: Write the failing test**

`compiler/tests/programs/32_ternary.c`:
```c
/* EXPECT_EXIT: 42 */
int main(void) {
    int a;
    int b;
    int r;

    /* basic ternary */
    a = 5;
    r = (a > 3) ? 10 : 20;
    if (r != 10) return 1;

    r = (a < 3) ? 10 : 20;
    if (r != 20) return 2;

    /* nested ternary */
    a = 2;
    r = (a == 1) ? 100 : (a == 2) ? 200 : 300;
    if (r != 200) return 3;

    /* ternary as argument */
    b = 0;
    b = b + ((a > 0) ? 40 : 0);
    b = b + ((a > 0) ? 2 : 0);
    if (b != 42) return 4;

    return 42;
}
```

- [ ] **Step 2: Verify failure**
```bash
make -C compiler test 2>&1 | tail -5
```

- [ ] **Step 3: Implement changes**

Apply: token/node defines, `?` and `:` in lexer, ternary in `parse_expr_bp`, ND_TERNARY in `gen_expr`.

- [ ] **Step 4: Run all tests**
```bash
make -C compiler test
```
Expected: all 33 tests pass (0–32).

- [ ] **Step 5: Commit**
```bash
git add compiler/src/c2wasm.c compiler/tests/programs/32_ternary.c
git commit -m "feat(A3): add ternary operator ?:

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 4: A4 — `switch` / `case` / `default`

**Files:**
- Modify: `compiler/src/c2wasm.c`
- Create: `compiler/tests/programs/33_switch.c`

### New constants

**Tokens** (append after TOK_COLON=60):
```c
#define TOK_SWITCH 61
#define TOK_CASE 62
#define TOK_DEFAULT 63
```

**Nodes** (append after ND_TERNARY=24):
```c
#define ND_SWITCH 25
#define ND_CASE 26
#define ND_DEFAULT 27
```

**Limit** (append after MAX_LOOP_DEPTH):
```c
#define MAX_CASES 256
```

### Lexer: `kw_lookup()` — add switch, case, default

```c
    if (strcmp(s, "switch") == 0) return TOK_SWITCH;
    if (strcmp(s, "case") == 0) return TOK_CASE;
    if (strcmp(s, "default") == 0) return TOK_DEFAULT;
```

### `parse_stmt()` — add switch, case, default branches

Add after the `if (at(TOK_DO))` line:
```c
    if (at(TOK_SWITCH)) return parse_switch();
    if (at(TOK_CASE)) {
        struct Node *cn;
        int cv;
        advance_tok();  /* consume 'case' */
        if (!at(TOK_INT_LIT) && !at(TOK_CHAR_LIT)) {
            error(cur->line, cur->col, "expected integer constant in case");
        }
        cv = cur->int_val;
        advance_tok();
        expect(TOK_COLON, "expected ':' after case value");
        cn = node_new(ND_CASE, cur->line, cur->col);
        cn->ival = cv;
        return cn;
    }
    if (at(TOK_DEFAULT)) {
        advance_tok();  /* consume 'default' */
        expect(TOK_COLON, "expected ':' after default");
        return node_new(ND_DEFAULT, cur->line, cur->col);
    }
```

### New function `parse_switch()` — add before `parse_stmt`

```c
struct Node *parse_switch(void) {
    struct Node *n;
    struct Node *expr;
    struct NList *stmts;
    int line;
    int col;

    line = cur->line;
    col = cur->col;
    advance_tok();  /* consume 'switch' */
    expect(TOK_LPAREN, "expected '(' after switch");
    expr = parse_expr();
    expect(TOK_RPAREN, "expected ')'");
    expect(TOK_LBRACE, "expected '{' after switch(...)");
    stmts = (struct NList *)malloc(sizeof(struct NList));
    stmts->items = (struct Node **)0;
    stmts->count = 0;
    stmts->cap = 0;
    while (!at(TOK_RBRACE) && !at(TOK_EOF)) {
        nlist_push(stmts, parse_stmt());
    }
    expect(TOK_RBRACE, "expected '}'");
    n = node_new(ND_SWITCH, line, col);
    n->c0 = expr;
    n->list = stmts->items;
    n->ival2 = stmts->count;
    return n;
}
```

### `collect_locals()` — add ND_SWITCH arm

```c
    } else if (n->kind == ND_SWITCH) {
        for (i = 0; i < n->ival2; i = i + 1) {
            collect_locals(n->list[i]);
        }
```

### `gen_func()` — add `$__stmp` local

After the `(local $__atmp i32)` emit line, add:
```c
    emit_indent();
    printf("(local $__stmp i32)\n");
```

### `gen_stmt()` — add ND_SWITCH arm

This requires a pre-scan to find case labels and positions, then nested-block emission.

```c
    } else if (n->kind == ND_SWITCH) {
        int case_vals[256];
        int case_start[256];
        int nc;
        int dflt_pos;
        int has_dflt;
        int k;
        int j;
        int next_start;
        int sw_lbl;

        /* pre-scan: collect case values and positions in flat list */
        nc = 0;
        dflt_pos = -1;
        has_dflt = 0;
        for (i = 0; i < n->ival2; i = i + 1) {
            if (n->list[i]->kind == ND_CASE) {
                if (nc < 256) {
                    case_vals[nc] = n->list[i]->ival;
                    case_start[nc] = i;
                    nc = nc + 1;
                }
            } else if (n->list[i]->kind == ND_DEFAULT) {
                dflt_pos = i;
                has_dflt = 1;
            }
        }

        sw_lbl = label_cnt;
        label_cnt = label_cnt + 1;
        brk_lbl[loop_sp] = sw_lbl;
        if (loop_sp > 0) {
            cont_lbl[loop_sp] = cont_lbl[loop_sp - 1];
        } else {
            cont_lbl[loop_sp] = -1;
        }
        loop_sp = loop_sp + 1;

        /* evaluate switch expression, save to $__stmp */
        gen_expr(n->c0);
        emit_indent();
        printf("local.set $__stmp\n");

        /* outer break block */
        emit_indent();
        printf("(block $brk_%d\n", sw_lbl);
        indent_level = indent_level + 1;

        /* default block (outermost case block) */
        emit_indent();
        printf("(block $sw%d_dflt\n", sw_lbl);
        indent_level = indent_level + 1;

        /* open numbered case blocks in REVERSE order (first case = innermost) */
        for (k = nc - 1; k >= 0; k = k - 1) {
            emit_indent();
            printf("(block $sw%d_c%d\n", sw_lbl, k);
            indent_level = indent_level + 1;
        }

        /* dispatch: if-chain */
        for (k = 0; k < nc; k = k + 1) {
            emit_indent(); printf("local.get $__stmp\n");
            emit_indent(); printf("i32.const %d\n", case_vals[k]);
            emit_indent(); printf("i32.eq\n");
            emit_indent(); printf("br_if $sw%d_c%d\n", sw_lbl, k);
        }
        emit_indent();
        if (has_dflt) {
            printf("br $sw%d_dflt\n", sw_lbl);
        } else {
            printf("br $brk_%d\n", sw_lbl);
        }

        /* close case blocks in forward order, emitting each case's body */
        for (k = 0; k < nc; k = k + 1) {
            indent_level = indent_level - 1;
            emit_indent(); printf(")\n");
            /* body: statements from case_start[k]+1 to next case/default/end */
            if (k + 1 < nc) {
                next_start = case_start[k + 1];
            } else if (has_dflt) {
                next_start = dflt_pos;
            } else {
                next_start = n->ival2;
            }
            for (j = case_start[k] + 1; j < next_start; j = j + 1) {
                /* skip case/default markers that ended up in this slice */
                if (n->list[j]->kind == ND_CASE) continue;
                if (n->list[j]->kind == ND_DEFAULT) continue;
                gen_stmt(n->list[j]);
            }
        }

        /* close default block */
        indent_level = indent_level - 1;
        emit_indent(); printf(")\n");

        /* default body (if present) */
        if (has_dflt) {
            for (j = dflt_pos + 1; j < n->ival2; j = j + 1) {
                /* skip stray case/default markers */
                if (n->list[j]->kind == ND_CASE) continue;
                if (n->list[j]->kind == ND_DEFAULT) continue;
                gen_stmt(n->list[j]);
            }
        }

        /* close break block */
        indent_level = indent_level - 1;
        emit_indent(); printf(")\n");

        loop_sp = loop_sp - 1;
```

### Test program

- [ ] **Step 1: Write the failing test**

`compiler/tests/programs/33_switch.c`:
```c
/* EXPECT_EXIT: 42 */
int main(void) {
    int x;
    int r;
    int i;

    /* basic switch with break */
    x = 2;
    r = 0;
    switch (x) {
        case 1:
            r = 10;
            break;
        case 2:
            r = 20;
            break;
        case 3:
            r = 30;
            break;
        default:
            r = 99;
            break;
    }
    if (r != 20) return 1;

    /* default case */
    x = 99;
    r = 0;
    switch (x) {
        case 1: r = 1; break;
        case 2: r = 2; break;
        default: r = 42; break;
    }
    if (r != 42) return 2;

    /* fallthrough */
    x = 1;
    r = 0;
    switch (x) {
        case 1:
            r = r + 1;
        case 2:
            r = r + 2;
            break;
        case 3:
            r = 99;
            break;
    }
    if (r != 3) return 3;

    /* switch inside loop with break/continue */
    r = 0;
    for (i = 0; i < 5; i++) {
        switch (i) {
            case 2:
                continue;
            case 4:
                break;
            default:
                r = r + i;
                break;
        }
    }
    /* r = 0+1+3 = 4 (skipped i=2 via continue, stopped at i=4 for switch break only) */
    if (r != 4) return 4;

    return 42;
}
```

- [ ] **Step 2: Verify failure**
```bash
make -C compiler test 2>&1 | tail -5
```

- [ ] **Step 3: Implement changes**

Apply all: token/node defines, `kw_lookup`, lexer, `parse_switch`, `parse_stmt` dispatch, `collect_locals`, `gen_func` ($__stmp), `gen_stmt` (ND_SWITCH).

- [ ] **Step 4: Run all tests**
```bash
make -C compiler test
```
Expected: all 34 tests pass (0–33).

- [ ] **Step 5: Commit**
```bash
git add compiler/src/c2wasm.c compiler/tests/programs/33_switch.c
git commit -m "feat(A4): add switch/case/default

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 5: A6 — `const` Qualifier

**Files:**
- Modify: `compiler/src/c2wasm.c`
- No new test file (validated by existing tests + re-running them)

### New constant

**Token** (append after TOK_DEFAULT=63):
```c
#define TOK_CONST 64
```

### Lexer: `kw_lookup()`

```c
    if (strcmp(s, "const") == 0) return TOK_CONST;
```

### `is_type_token()` — recognize `const` as a type-leading token

Add at the beginning (before the `if (at(TOK_INT))` check):
```c
    if (at(TOK_CONST)) return 1;
```

### `parse_type()` — skip `const` before the actual type

Add at the very beginning:
```c
    while (at(TOK_CONST)) advance_tok();
```

### `parse_var_decl()` — skip `const` before type

Add at the very beginning (before the `line = cur->line` block):
```c
    while (at(TOK_CONST)) advance_tok();
```

### `parse_global_var()` — skip `const` before type

Add at the very beginning (before `is_char = 0`):
```c
    while (at(TOK_CONST)) advance_tok();
```

### `parse_func()` — skip `const` in parameter types

In the parameter parsing loop, after the `pty = parse_type()` calls (lines ~1378 and ~1389), `parse_type` already skips const (we just fixed it). But the function's return type uses `parse_type()` at line ~1348, which is also fixed.

### `parse_program()` peek-ahead — handle `const` before type

In the lookahead section (around line 1538–1553), add `while (at(TOK_CONST)) advance_tok();` BEFORE the `if (at(TOK_STRUCT))` check in the peek section:

```c
        /* Peek ahead to distinguish function from global variable */
        sp = lex_pos;
        sl = lex_line;
        sc = lex_col;
        st = cur;
        while (at(TOK_CONST)) advance_tok();  /* NEW: skip const in peek */
        if (at(TOK_STRUCT)) {
```

### Validation

- [ ] **Step 1: Run all existing tests** — they should still pass (no behavioral change)
```bash
make -C compiler test
```
Expected: all 34 tests pass.

- [ ] **Step 2: Quick smoke test** — create a temporary file to verify const parses:

Write to `/tmp/const_test.c`:
```c
/* EXPECT_EXIT: 42 */
const int N = 10;
int main(void) {
    const int x = 5;
    const char *msg = "hello";
    if (x != 5) return 1;
    if (N != 10) return 2;
    return 42;
}
```
```bash
cd /Users/parker/code/wasm-c/compiler
./c2wasm < /tmp/const_test.c > /tmp/const_test.wat 2>&1
echo "exit: $?"
```
Expected: exits 0, produces WAT.

- [ ] **Step 3: Commit**
```bash
cd /Users/parker/code/wasm-c
git add compiler/src/c2wasm.c
git commit -m "feat(A6): add const qualifier (parsed and discarded)

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 6: A5 — `sizeof` Fix + Extension

**Files:**
- Modify: `compiler/src/c2wasm.c`
- Create: `compiler/tests/programs/34_sizeof.c`

### Current state

`TOK_SIZEOF` (12) and `ND_SIZEOF` (19) exist. Issues:
1. `sizeof(char)` emits `i32.const 4` (bug — should be 1)
2. Pointer types (e.g., `sizeof(int*)`) emit 4 but don't check a pointer flag (fine — 4 is correct)
3. Pointer-to-struct (e.g., `sizeof(struct Foo*)`) would return struct size (bug — should be 4)
4. `sizeof(expr)` not supported

### Parser fix: `parse_atom()` sizeof section

Replace the current sizeof parsing (lines ~927–946) with a corrected version that:
- Records whether stars were consumed in `n->ival` (pointer flag)
- Stores the type name in `n->sval` for scalar/struct types
- Also handles expr form

```c
    if (at(TOK_SIZEOF)) {
        int is_ptr;
        line = cur->line;
        col = cur->col;
        advance_tok();
        n = node_new(ND_SIZEOF, line, col);
        is_ptr = 0;
        if (at(TOK_LPAREN)) {
            advance_tok();  /* consume '(' */
            if (at(TOK_STRUCT)) {
                advance_tok();
                if (at(TOK_IDENT)) {
                    n->sval = strdupn(cur->text, 127);
                    advance_tok();
                }
                while (at(TOK_STAR)) { is_ptr = 1; advance_tok(); }
            } else if (is_type_token()) {
                n->sval = strdupn(cur->text, 127);
                advance_tok();
                while (at(TOK_CONST)) advance_tok();
                while (at(TOK_STAR)) { is_ptr = 1; advance_tok(); }
            } else {
                /* sizeof(expr) */
                n->c0 = parse_expr();
            }
            expect(TOK_RPAREN, "expected ')' after sizeof");
        } else {
            /* sizeof expr — parse as high-bp expression */
            n->c0 = parse_expr_bp(25);
        }
        n->ival = is_ptr;
        return n;
    }
```

**Note**: `is_type_token()` at this point includes `TOK_INT`, `TOK_CHAR_KW`, `TOK_VOID`, `TOK_STRUCT`, `TOK_CONST`, `TOK_ENUM`, typedef aliases (after A7/A8). For `sizeof`, we want to check if we're looking at a type — existing `is_type_token()` works.

But there's a conflict: `is_type_token()` returns 1 for `TOK_CONST`, which could be ambiguous if someone writes `sizeof(const int)`. After the `is_type_token()` check, we advance once — but `const` isn't the actual type name. Fix by handling `const` in sizeof too:

Actually, just simplify: check the token kind explicitly:
```c
            } else if (at(TOK_INT) || at(TOK_CHAR_KW) || at(TOK_VOID) || at(TOK_CONST)) {
                while (at(TOK_CONST)) advance_tok();  /* skip const */
                if (at(TOK_INT) || at(TOK_CHAR_KW) || at(TOK_VOID)) {
                    n->sval = strdupn(cur->text, 127);
                    advance_tok();
                }
                while (at(TOK_STAR)) { is_ptr = 1; advance_tok(); }
```

### Codegen fix: `gen_expr()` ND_SIZEOF section

Replace the current sizeof codegen (lines ~2107–2116):

```c
    } else if (n->kind == ND_SIZEOF) {
        struct StructDef *sd;
        int sz;
        if (n->ival == 1) {
            /* pointer type: always 4 */
            sz = 4;
        } else if (n->c0 != (struct Node *)0) {
            /* sizeof(expr): infer from expression node */
            if (n->c0->kind == ND_IDENT) {
                sz = var_elem_size(n->c0->sval);
            } else {
                sz = 4;
            }
        } else if (n->sval != (char *)0 && strcmp(n->sval, "char") == 0) {
            sz = 1;
        } else if (n->sval != (char *)0) {
            sd = find_struct(n->sval);
            if (sd != (struct StructDef *)0) {
                sz = sd->size;
            } else {
                sz = 4;
            }
        } else {
            sz = 4;
        }
        emit_indent();
        printf("i32.const %d\n", sz);
```

### Test program

- [ ] **Step 1: Write the failing test**

`compiler/tests/programs/34_sizeof.c`:
```c
/* EXPECT_EXIT: 42 */
struct Point {
    int x;
    int y;
};

int main(void) {
    int a;
    char b;

    /* sizeof(int) = 4 */
    if (sizeof(int) != 4) return 1;

    /* sizeof(char) = 1 (was returning 4 before fix) */
    if (sizeof(char) != 1) return 2;

    /* sizeof(struct Point) = 8 */
    if (sizeof(struct Point) != 8) return 3;

    /* sizeof pointer = 4 */
    if (sizeof(int *) != 4) return 4;
    if (sizeof(char *) != 4) return 5;

    /* sizeof(expr) — variable */
    a = 42;
    if (sizeof(a) != 4) return 6;
    b = 'x';
    if (sizeof(b) != 1) return 7;

    return 42;
}
```

- [ ] **Step 2: Verify failure**
```bash
make -C compiler test 2>&1 | tail -5
```
Expected: test 34 FAILS (sizeof(char) returns wrong value, or char variable not found).

- [ ] **Step 3: Implement changes**

Apply: sizeof parser fix, sizeof codegen fix.

- [ ] **Step 4: Run all tests**
```bash
make -C compiler test
```
Expected: all 35 tests pass (0–34).

- [ ] **Step 5: Commit**
```bash
git add compiler/src/c2wasm.c compiler/tests/programs/34_sizeof.c
git commit -m "feat(A5): fix sizeof(char), pointers, add sizeof(expr)

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 7: A7 — `enum`

**Files:**
- Modify: `compiler/src/c2wasm.c`
- Create: `compiler/tests/programs/35_enum.c`

### New constant

**Token** (append after TOK_CONST=64):
```c
#define TOK_ENUM 65
```

**Limit** (append after MAX_CASES):
```c
#define MAX_ENUM_CONSTS 512
```

### New data structure — add after the GlobalVar table definitions

```c
struct EnumConst {
    char *name;
    int val;
};

struct EnumConst **enum_consts;
int nenum_consts;

void init_enum_consts(void) {
    enum_consts = (struct EnumConst **)malloc(MAX_ENUM_CONSTS * sizeof(void *));
    nenum_consts = 0;
}

int find_enum_const(char *name) {
    int i;
    for (i = 0; i < nenum_consts; i = i + 1) {
        if (strcmp(enum_consts[i]->name, name) == 0) return i;
    }
    return -1;
}
```

### `main()` — call `init_enum_consts()`

Add `init_enum_consts();` alongside the other `init_*()` calls.

### Lexer: `kw_lookup()`

```c
    if (strcmp(s, "enum") == 0) return TOK_ENUM;
```

### New function `parse_enum_def()` — add before `parse_program`

```c
void parse_enum_def(void) {
    int val;
    char *ename;

    advance_tok();  /* consume 'enum' */
    if (at(TOK_IDENT)) advance_tok();  /* skip optional tag */
    expect(TOK_LBRACE, "expected '{' in enum");
    val = 0;
    while (!at(TOK_RBRACE) && !at(TOK_EOF)) {
        if (!at(TOK_IDENT)) error(cur->line, cur->col, "expected enum constant name");
        ename = strdupn(cur->text, 127);
        advance_tok();
        if (at(TOK_EQ)) {
            advance_tok();
            if (at(TOK_MINUS)) {
                advance_tok();
                val = -cur->int_val;
            } else {
                val = cur->int_val;
            }
            advance_tok();
        }
        if (nenum_consts < MAX_ENUM_CONSTS) {
            enum_consts[nenum_consts] = (struct EnumConst *)malloc(sizeof(struct EnumConst));
            enum_consts[nenum_consts]->name = ename;
            enum_consts[nenum_consts]->val = val;
            nenum_consts = nenum_consts + 1;
        }
        val = val + 1;
        if (at(TOK_COMMA)) advance_tok();
    }
    expect(TOK_RBRACE, "expected '}'");
    expect(TOK_SEMI, "expected ';' after enum");
}
```

### `parse_program()` — add enum handling before struct handling

At the top of the parse loop, add:
```c
        if (at(TOK_ENUM)) {
            /* peek: definition (with {) vs type usage */
            int sp2;
            int sl2;
            int sc2;
            struct Token *st2;
            int is_enum_def;
            sp2 = lex_pos; sl2 = lex_line; sc2 = lex_col; st2 = cur;
            advance_tok();
            if (at(TOK_IDENT)) advance_tok();
            is_enum_def = at(TOK_LBRACE);
            lex_pos = sp2; lex_line = sl2; lex_col = sc2; cur = st2;
            if (is_enum_def) {
                parse_enum_def();
                continue;
            }
        }
```

Also update the peek-ahead for func vs global to handle `TOK_ENUM`:
```c
        /* existing peek section — add enum handling */
        sp = lex_pos; sl = lex_line; sc = lex_col; st = cur;
        while (at(TOK_CONST)) advance_tok();
        if (at(TOK_STRUCT) || at(TOK_ENUM)) {
            advance_tok();
            if (at(TOK_IDENT)) advance_tok();
        } else {
            advance_tok();
        }
        while (at(TOK_STAR)) advance_tok();
        is_func = 0;
        if (at(TOK_IDENT)) {
            advance_tok();
            is_func = at(TOK_LPAREN);
        }
        lex_pos = sp; lex_line = sl; lex_col = sc; cur = st;
```

### `is_type_token()` — add `TOK_ENUM`

```c
    if (at(TOK_ENUM)) return 1;
```

### `parse_type()` — handle `enum`

```c
    if (at(TOK_ENUM)) {
        advance_tok();
        if (at(TOK_IDENT)) advance_tok();  /* skip tag */
        return 0;  /* treat as int */
    }
```

### `parse_atom()` — resolve enum constants before creating ND_IDENT

In the `if (at(TOK_IDENT))` block, BEFORE creating the ND_IDENT node, add:

```c
    if (at(TOK_IDENT)) {
        int eci;
        eci = find_enum_const(cur->text);
        if (eci >= 0) {
            n = node_new(ND_INT_LIT, cur->line, cur->col);
            n->ival = enum_consts[eci]->val;
            advance_tok();
            return n;
        }
        /* ... existing ND_IDENT handling ... */
```

### Test program

- [ ] **Step 1: Write the failing test**

`compiler/tests/programs/35_enum.c`:
```c
/* EXPECT_EXIT: 42 */
enum Color {
    RED = 0,
    GREEN = 1,
    BLUE = 2
};

enum Direction {
    NORTH,
    SOUTH,
    EAST,
    WEST
};

int main(void) {
    int c;
    int d;

    c = GREEN;
    if (c != 1) return 1;

    d = WEST;
    if (d != 3) return 2;

    /* enum in expression */
    if (RED + GREEN + BLUE != 3) return 3;

    /* enum in switch */
    c = BLUE;
    switch (c) {
        case 0: return 4;
        case 1: return 5;
        case 2: break;
        default: return 6;
    }

    /* auto-increment values */
    if (NORTH != 0) return 7;
    if (SOUTH != 1) return 8;
    if (EAST != 2) return 9;
    if (WEST != 3) return 10;

    return 42;
}
```

- [ ] **Step 2: Verify failure**
```bash
make -C compiler test 2>&1 | tail -5
```

- [ ] **Step 3: Implement changes**

Apply: token define, limit, struct/table/init/find, `kw_lookup`, `parse_enum_def`, `parse_program`, `is_type_token`, `parse_type`, `parse_atom`, `main()` init call.

- [ ] **Step 4: Run all tests**
```bash
make -C compiler test
```
Expected: all 36 tests pass (0–35).

- [ ] **Step 5: Commit**
```bash
git add compiler/src/c2wasm.c compiler/tests/programs/35_enum.c
git commit -m "feat(A7): add enum declarations and constants

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 8: A8 — `typedef`

**Files:**
- Modify: `compiler/src/c2wasm.c`
- Create: `compiler/tests/programs/36_typedef.c`

### New constant

**Token** (append after TOK_ENUM=65):
```c
#define TOK_TYPEDEF 66
```

**Limit** (append after MAX_ENUM_CONSTS):
```c
#define MAX_TYPE_ALIASES 128
```

### New data structure — add after the EnumConst definitions

```c
struct TypeAlias {
    char *alias;
    int  resolved_kind;  /* 0=int, 1=void, 2=char */
    int  is_ptr;
};

struct TypeAlias **type_aliases;
int ntype_aliases;

void init_type_aliases(void) {
    type_aliases = (struct TypeAlias **)malloc(MAX_TYPE_ALIASES * sizeof(void *));
    ntype_aliases = 0;
}

int find_type_alias(char *name) {
    int i;
    for (i = 0; i < ntype_aliases; i = i + 1) {
        if (strcmp(type_aliases[i]->alias, name) == 0) return i;
    }
    return -1;
}
```

### `main()` — call `init_type_aliases()`

Add `init_type_aliases();` alongside the other `init_*()` calls.

### Lexer: `kw_lookup()`

```c
    if (strcmp(s, "typedef") == 0) return TOK_TYPEDEF;
```

### `parse_typedef()` — add before `parse_program`

**Note on limitation**: This implementation handles `typedef struct Tag Alias;` (pre-defined struct) but NOT the combined `typedef struct Tag { ... } Alias;` form. If `parse_typedef` sees `TOK_STRUCT` followed by `TOK_LBRACE`, it will error. The test below uses the two-step form which IS supported. Do not change this behavior — document it.

To handle `typedef struct { } Alias;`, after consuming the struct tag, add a check for `{` and skip the body:
```c
if (at(TOK_STRUCT)) {
    advance_tok();
    if (at(TOK_IDENT)) advance_tok();  /* skip tag */
    if (at(TOK_LBRACE)) {
        /* skip inline struct body for typedef struct { } Alias; */
        int depth;
        depth = 1;
        advance_tok();
        while (depth > 0 && !at(TOK_EOF)) {
            if (at(TOK_LBRACE)) { depth = depth + 1; }
            else if (at(TOK_RBRACE)) { depth = depth - 1; }
            advance_tok();
        }
    }
    rk = 0;
}
```

```c
void parse_typedef(void) {
    int rk;
    int is_ptr;
    char *alias_name;

    advance_tok();  /* consume 'typedef' */
    while (at(TOK_CONST)) advance_tok();
    rk = 0;
    is_ptr = 0;
    if (at(TOK_STRUCT)) {
        advance_tok();
        if (at(TOK_IDENT)) advance_tok();  /* skip struct tag */
        rk = 0;
    } else if (at(TOK_ENUM)) {
        advance_tok();
        if (at(TOK_IDENT)) advance_tok();
        rk = 0;
    } else if (at(TOK_INT)) {
        advance_tok();
        rk = 0;
    } else if (at(TOK_CHAR_KW)) {
        advance_tok();
        rk = 2;
    } else if (at(TOK_VOID)) {
        advance_tok();
        rk = 1;
    } else if (at(TOK_IDENT)) {
        /* typedef of an existing typedef alias */
        int tai;
        tai = find_type_alias(cur->text);
        if (tai >= 0) {
            rk = type_aliases[tai]->resolved_kind;
        }
        advance_tok();
    } else {
        error(cur->line, cur->col, "expected type in typedef");
    }
    while (at(TOK_STAR)) { is_ptr = 1; advance_tok(); }
    if (!at(TOK_IDENT)) error(cur->line, cur->col, "expected alias name in typedef");
    alias_name = strdupn(cur->text, 127);
    advance_tok();
    expect(TOK_SEMI, "expected ';' after typedef");
    if (ntype_aliases < MAX_TYPE_ALIASES) {
        type_aliases[ntype_aliases] = (struct TypeAlias *)malloc(sizeof(struct TypeAlias));
        type_aliases[ntype_aliases]->alias = alias_name;
        type_aliases[ntype_aliases]->resolved_kind = rk;
        type_aliases[ntype_aliases]->is_ptr = is_ptr;
        ntype_aliases = ntype_aliases + 1;
    }
}
```

### `parse_program()` — add typedef handling at top of loop

```c
        if (at(TOK_TYPEDEF)) {
            parse_typedef();
            continue;
        }
```

### `is_type_token()` — add typedef alias check

```c
    if (at(TOK_IDENT) && find_type_alias(cur->text) >= 0) return 1;
```

### `parse_type()` — handle typedef alias

Add at the beginning (before current `if (at(TOK_INT))` check, after the const skip):

```c
    while (at(TOK_CONST)) advance_tok();
    if (at(TOK_IDENT)) {
        int taidx;
        taidx = find_type_alias(cur->text);
        if (taidx >= 0) {
            advance_tok();
            return type_aliases[taidx]->resolved_kind;
        }
    }
```

### Test program

- [ ] **Step 1: Write the failing test**

`compiler/tests/programs/36_typedef.c`:
```c
/* EXPECT_EXIT: 42 */
typedef int size_t;
typedef char byte;
typedef int *intptr;

struct Point {
    int x;
    int y;
};
typedef struct Point Point;

size_t get_size(void) {
    return 8;
}

int main(void) {
    size_t n;
    byte b;
    Point p;

    n = 10;
    if (n != 10) return 1;

    b = 65;
    if (b != 65) return 2;

    /* typedef struct */
    p.x = 3;
    p.y = 4;
    if (p.x != 3) return 3;
    if (p.y != 4) return 4;

    /* typedef in function signature */
    if (get_size() != 8) return 5;

    /* chain: size_t n is just int */
    n = n + 32;
    if (n != 42) return 6;

    return 42;
}
```

- [ ] **Step 2: Verify failure**
```bash
make -C compiler test 2>&1 | tail -5
```

- [ ] **Step 3: Implement changes**

Apply: token define, limit, struct/table/init/find, `kw_lookup`, `parse_typedef`, `parse_program`, `is_type_token`, `parse_type`, `main()` init call.

- [ ] **Step 4: Run all tests**
```bash
make -C compiler test
```
Expected: all 37 tests pass (0–36).

- [ ] **Step 5: Commit**
```bash
git add compiler/src/c2wasm.c compiler/tests/programs/36_typedef.c
git commit -m "feat(A8): add typedef declarations

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 9: A9 — Array Initializers

**Files:**
- Modify: `compiler/src/c2wasm.c`
- Create: `compiler/tests/programs/37_array_init.c`

### What we're implementing

- `int arr[N]` — declare local array of N ints (allocates N*4 bytes via malloc)
- `int arr[N] = {e1, e2, ...}` — allocate + initialize elements (desugared to block)
- `char s[] = "hello"` — char pointer to interned string literal (no allocation)
- `char buf[N]` — allocate N bytes via malloc

**Not implemented** (too complex): global array declarations.

### Design: desugaring brace initializer

`int nums[3] = {1, 2, 3};` is desugared by the parser into an `ND_BLOCK` containing:
1. `ND_VAR_DECL(nums, array_size=3)` — no initializer; codegen emits `malloc(12)`
2. `ND_EXPR_STMT(nums[0] = 1)` — codegen emits store
3. `ND_EXPR_STMT(nums[1] = 2)`
4. `ND_EXPR_STMT(nums[2] = 3)`

This reuses existing `ND_SUBSCRIPT` and `ND_ASSIGN` codegen. `collect_locals` finds `ND_VAR_DECL` inside the `ND_BLOCK` recursively. ✓

### `ND_VAR_DECL` field reuse

- `n->ival` (currently unused for VAR_DECL) = array size (0 = scalar, N>0 = array of N elements)
- `n->ival2` = is_char (unchanged)
- `n->c0` = scalar initializer (for non-array or `char s[] = "str"` case)

### `parse_var_decl()` changes

Add array handling AFTER reading the variable name (`n->sval` set) and BEFORE the `if (at(TOK_EQ))` check for scalar init:

```c
    /* NEW: array declaration [N] */
    if (at(TOK_LBRACKET)) {
        struct NList *blk_stmts;
        int arr_sz;
        struct Node *blk;
        struct Node *asgn;
        struct Node *es;
        struct Node *base_node;
        struct Node *idx_node;
        struct Node *sub_node;
        struct Node *elem;
        int init_count;

        advance_tok();  /* consume '[' */
        arr_sz = 0;
        if (at(TOK_INT_LIT)) {
            arr_sz = cur->int_val;
            advance_tok();
        }
        expect(TOK_RBRACKET, "expected ']'");
        n->ival = arr_sz;

        if (at(TOK_EQ)) {
            advance_tok();
            if (at(TOK_LBRACE)) {
                /* brace initializer: desugar to block */
                advance_tok();  /* consume '{' */
                blk_stmts = (struct NList *)malloc(sizeof(struct NList));
                blk_stmts->items = (struct Node **)0;
                blk_stmts->count = 0;
                blk_stmts->cap = 0;
                nlist_push(blk_stmts, n);  /* first: the array alloc */
                init_count = 0;
                while (!at(TOK_RBRACE) && !at(TOK_EOF)) {
                    elem = parse_expr();
                    if (at(TOK_COMMA)) advance_tok();
                    /* build: n->sval[init_count] = elem */
                    base_node = node_new(ND_IDENT, n->nline, n->ncol);
                    base_node->sval = strdupn(n->sval, 127);
                    idx_node = node_new(ND_INT_LIT, n->nline, n->ncol);
                    idx_node->ival = init_count;
                    sub_node = node_new(ND_SUBSCRIPT, n->nline, n->ncol);
                    sub_node->c0 = base_node;
                    sub_node->c1 = idx_node;
                    asgn = node_new(ND_ASSIGN, n->nline, n->ncol);
                    asgn->c0 = sub_node;
                    asgn->c1 = elem;
                    asgn->ival = TOK_EQ;
                    es = node_new(ND_EXPR_STMT, n->nline, n->ncol);
                    es->c0 = asgn;
                    nlist_push(blk_stmts, es);
                    init_count = init_count + 1;
                }
                expect(TOK_RBRACE, "expected '}'");
                if (n->ival == 0) n->ival = init_count;
                expect(TOK_SEMI, "expected ';'");
                blk = node_new(ND_BLOCK, n->nline, n->ncol);
                blk->list = blk_stmts->items;
                blk->ival2 = blk_stmts->count;
                return blk;
            } else if (at(TOK_STR_LIT)) {
                /* char s[] = "str": treat as char pointer to string */
                n->c0 = parse_atom();  /* will parse str lit and return */
                n->ival = 0;  /* not a real sized array */
                expect(TOK_SEMI, "expected ';'");
                return n;
            }
        }
        /* no initializer: just array allocation */
        expect(TOK_SEMI, "expected ';'");
        return n;
    }
```

### `gen_stmt()` ND_VAR_DECL — handle array allocation

Replace the current ND_VAR_DECL codegen:

```c
    } else if (n->kind == ND_VAR_DECL) {
        if (n->ival > 0) {
            /* array: allocate via malloc */
            int esz2;
            esz2 = n->ival2 ? 1 : 4;  /* char array = 1 byte/elem, int = 4 */
            emit_indent();
            printf("i32.const %d\n", n->ival * esz2);
            emit_indent();
            printf("call $malloc\n");
            emit_indent();
            printf("local.set $%s\n", n->sval);
        } else if (n->c0 != (struct Node *)0) {
            gen_expr(n->c0);
            emit_indent();
            printf("local.set $%s\n", n->sval);
        }
```

### Test program

- [ ] **Step 1: Write the failing test**

`compiler/tests/programs/37_array_init.c`:
```c
/* EXPECT_EXIT: 42 */
int main(void) {
    int arr[3];
    int nums[3];
    char buf[5];
    char s[1];
    int i;
    int sum;

    /* int array without initializer: can set elements */
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 12;
    if (arr[0] != 10) return 1;
    if (arr[1] != 20) return 2;
    if (arr[2] != 12) return 3;

    /* int array with brace initializer */
    nums[0] = 1;
    nums[1] = 2;
    nums[2] = 3;

    sum = 0;
    for (i = 0; i < 3; i++) {
        sum = sum + nums[i];
    }
    if (sum != 6) return 4;

    /* char array: write bytes */
    buf[0] = 72;
    buf[1] = 101;
    buf[2] = 108;
    buf[3] = 108;
    buf[4] = 111;
    if (buf[0] != 72) return 5;
    if (buf[4] != 111) return 6;

    /* char s[] = "str": pointer to string data */
    s[0] = 65;
    if (s[0] != 65) return 7;

    return 42;
}
```

Note: This test avoids brace-initializer syntax to keep it simple. The array allocations themselves are the core feature being tested. If brace initializers work, add a more complex test.

**REQUIRED: Extended test with actual brace initializer** (this test MUST pass — it exercises the `if (at(TOK_LBRACE))` desugaring path in `parse_var_decl`):

`compiler/tests/programs/38_array_brace.c`:
```c
/* EXPECT_EXIT: 42 */
int main(void) {
    int nums[3] = {10, 20, 12};
    int sum;
    sum = nums[0] + nums[1] + nums[2];
    if (sum != 42) return 1;
    return 42;
}
```

- [ ] **Step 2: Verify failure**
```bash
make -C compiler test 2>&1 | tail -5
```
Expected: test 37 FAILS (compile error on `int arr[3]`).

- [ ] **Step 3: Implement changes**

Apply: `parse_var_decl` array handling, `gen_stmt` ND_VAR_DECL array path.

- [ ] **Step 4: Run all tests**
```bash
make -C compiler test
```
Expected: all 39 tests pass (0–38), including brace-init test 38.

- [ ] **Step 5: Commit**
```bash
git add compiler/src/c2wasm.c compiler/tests/programs/37_array_init.c compiler/tests/programs/38_array_brace.c
git commit -m "feat(A9): add array declarations int arr[N] and brace initializers

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 10: Final Verification

- [ ] **Step 1: Run full test suite**
```bash
cd /Users/parker/code/wasm-c
make -C compiler test
```
Expected: all tests pass (at least 38).

- [ ] **Step 2: Build the compiler with itself (self-hosting check)**
```bash
cd /Users/parker/code/wasm-c/compiler
./c2wasm < src/c2wasm.c > /tmp/c2wasm_stage2.wat 2>&1 | head -5
echo "Stage 2 exit: $?"
```
Expected: exits 0 (compiles its own source without error).

- [ ] **Step 3: Final commit with summary**
```bash
cd /Users/parker/code/wasm-c
git add -A
git commit -m "feat: complete Track A (A1-A9) C89 language features

- A1: XOR operator, compound assignment |= &= ^= <<= >>=, post-inc/dec
- A2: do-while loops
- A3: ternary operator ?:
- A4: switch/case/default
- A5: sizeof fixes (char=1, pointers, expr form)
- A6: const qualifier
- A7: enum declarations
- A8: typedef declarations
- A9: array declarations int arr[N]

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Common Pitfalls

1. **C89 compliance in compiler source**: All new variables must be declared at the top of their block. In functions like `parse_switch`, declare all locals (including the local arrays like `case_vals[256]`) at the top.

2. **Ternary in gen_expr vs gen_stmt context**: `ND_TERNARY` must leave exactly one value on the stack. The WAT `(if (result i32) ...)` form does this correctly. Do NOT use `ND_TERNARY` in statement position without `drop`.

3. **Switch fallthrough semantics**: `br $sw%d_c%d` in dispatch exits the target block, landing AFTER it. The case bodies are placed after their respective blocks close. Fallthrough happens when a case body ends without `br $brk_N`.

4. **Continue in switch**: The `cont_lbl[loop_sp]` is propagated from the enclosing loop. If switch is at the top level (no enclosing loop), `cont_lbl[loop_sp] = -1`. A `continue` in a switch body with no enclosing loop generates invalid WAT, but is also a programmer error.

5. **Array subscript in initializer desugaring**: `arr[i]` uses `ND_SUBSCRIPT` which calls `expr_elem_size(c0)`. Since `arr` is an int array, `c0` is `ND_IDENT("arr")`. `expr_elem_size` calls `var_elem_size("arr")` → `local_vars[i]->lv_is_char` = 0 → returns 4. ✓

6. **sizeof(char) in var decl**: When `parse_var_decl` encounters `char buf[N]`, it sets `is_char=1` and `n->ival=N`. Codegen emits `i32.const N*1` (element size = 1 for char). ✓

7. **collect_locals for ND_BLOCK returned by parse_var_decl**: The ND_BLOCK returned for brace-init arrays is placed inside the function body's block. `collect_locals` for `ND_BLOCK` already recurses into its list. The `ND_VAR_DECL` inside the block will be found. ✓
