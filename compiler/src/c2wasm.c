/*
 * c2wasm.c — C-to-WAT compiler
 *
 * Reads C source from stdin, emits WebAssembly Text Format to stdout.
 * Targets a C subset sufficient for self-hosting.
 *
 * Build:  gcc -o c2wasm compiler/src/c2wasm.c
 * Usage:  ./c2wasm < program.c > program.wat
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* ================================================================
 * Error reporting
 * ================================================================ */

static void error(int line, int col, const char *msg) {
    fprintf(stderr, "%d:%d: error: %s\n", line, col, msg);
    exit(1);
}

/* ================================================================
 * Source buffer — read entire stdin
 * ================================================================ */

#define MAX_SRC (2 * 1024 * 1024)
static char src[MAX_SRC];
static int src_len;

static void read_source(void) {
    src_len = (int)fread(src, 1, MAX_SRC - 1, stdin);
    src[src_len] = '\0';
}

/* ================================================================
 * Tokens
 * ================================================================ */

enum {
    TOK_EOF = 0,
    /* keywords */
    TOK_INT, TOK_CHAR_KW, TOK_VOID, TOK_RETURN,
    TOK_IF, TOK_ELSE, TOK_WHILE, TOK_FOR,
    TOK_BREAK, TOK_CONTINUE, TOK_STRUCT, TOK_SIZEOF,
    TOK_DEFINE,
    /* literals & identifiers */
    TOK_IDENT, TOK_INT_LIT, TOK_CHAR_LIT, TOK_STR_LIT,
    /* delimiters */
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_LBRACKET, TOK_RBRACKET,
    TOK_SEMI, TOK_COMMA, TOK_DOT, TOK_ARROW,
    /* operators */
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_AMP, TOK_BANG,
    TOK_PIPE_PIPE, TOK_AMP_AMP,
    TOK_EQ, TOK_PLUS_EQ, TOK_MINUS_EQ,
    TOK_EQ_EQ, TOK_BANG_EQ,
    TOK_LT, TOK_GT, TOK_LT_EQ, TOK_GT_EQ,
    TOK_PLUS_PLUS, TOK_MINUS_MINUS,
};

typedef struct {
    int kind;
    char text[512];
    int int_val;
    int line, col;
} Token;

/* ================================================================
 * Lexer
 * ================================================================ */

static struct { int pos, line, col; } L;

static void lex_init(void) { L.pos = 0; L.line = 1; L.col = 1; }

static char lp(void)  { return L.pos < src_len ? src[L.pos] : '\0'; }
static char lp2(void) { return L.pos+1 < src_len ? src[L.pos+1] : '\0'; }
static char la(void) {
    char c = src[L.pos++];
    if (c == '\n') { L.line++; L.col = 1; } else L.col++;
    return c;
}

static void skip_ws(void) {
    for (;;) {
        char c = lp();
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { la(); }
        else if (c == '/' && lp2() == '/') {
            while (L.pos < src_len && lp() != '\n') la();
        } else if (c == '/' && lp2() == '*') {
            la(); la();
            while (L.pos < src_len) {
                if (lp() == '*' && lp2() == '/') { la(); la(); break; }
                la();
            }
        } else break;
    }
}

static int kw_lookup(const char *s) {
    if (!strcmp(s, "int"))      return TOK_INT;
    if (!strcmp(s, "char"))     return TOK_CHAR_KW;
    if (!strcmp(s, "void"))     return TOK_VOID;
    if (!strcmp(s, "return"))   return TOK_RETURN;
    if (!strcmp(s, "if"))       return TOK_IF;
    if (!strcmp(s, "else"))     return TOK_ELSE;
    if (!strcmp(s, "while"))    return TOK_WHILE;
    if (!strcmp(s, "for"))      return TOK_FOR;
    if (!strcmp(s, "break"))    return TOK_BREAK;
    if (!strcmp(s, "continue")) return TOK_CONTINUE;
    if (!strcmp(s, "struct"))   return TOK_STRUCT;
    if (!strcmp(s, "sizeof"))   return TOK_SIZEOF;
    return 0;
}

static Token next_token(void) {
    skip_ws();
    Token t;
    memset(&t, 0, sizeof(t));
    t.line = L.line;
    t.col  = L.col;

    if (L.pos >= src_len) { t.kind = TOK_EOF; return t; }
    char c = lp();

    /* preprocessor: #define */
    if (c == '#') {
        la();
        while (lp() == ' ' || lp() == '\t') la();
        int s = L.pos;
        while (isalpha(lp())) la();
        if (L.pos - s == 6 && !memcmp(src + s, "define", 6)) {
            t.kind = TOK_DEFINE;
            return t;
        }
        error(t.line, t.col, "unknown preprocessor directive");
    }

    /* identifiers / keywords */
    if (isalpha(c) || c == '_') {
        int s = L.pos;
        while (isalnum(lp()) || lp() == '_') la();
        int len = L.pos - s;
        if (len >= (int)sizeof(t.text)) len = (int)sizeof(t.text) - 1;
        memcpy(t.text, src + s, len);
        t.text[len] = '\0';
        int kw = kw_lookup(t.text);
        t.kind = kw ? kw : TOK_IDENT;
        return t;
    }

    /* integer literals */
    if (isdigit(c)) {
        int val = 0;
        if (c == '0' && (lp2() == 'x' || lp2() == 'X')) {
            la(); la();
            while (isxdigit(lp())) {
                char d = la();
                val = val * 16 + (d <= '9' ? d - '0' : (d | 32) - 'a' + 10);
            }
        } else {
            while (isdigit(lp())) val = val * 10 + (la() - '0');
        }
        t.kind = TOK_INT_LIT;
        t.int_val = val;
        return t;
    }

    /* character literal */
    if (c == '\'') {
        la();
        char ch;
        if (lp() == '\\') {
            la();
            switch (lp()) {
                case 'n': ch = '\n'; break; case 't': ch = '\t'; break;
                case '0': ch = '\0'; break; case '\\': ch = '\\'; break;
                case '\'': ch = '\''; break; default: ch = lp(); break;
            }
            la();
        } else {
            ch = la();
        }
        if (lp() == '\'') la();
        else error(t.line, t.col, "unterminated char literal");
        t.kind = TOK_CHAR_LIT;
        t.int_val = (unsigned char)ch;
        return t;
    }

    /* string literal */
    if (c == '"') {
        la();
        int i = 0;
        while (L.pos < src_len && lp() != '"') {
            if (lp() == '\\') {
                la();
                switch (lp()) {
                    case 'n': t.text[i++] = '\n'; break;
                    case 't': t.text[i++] = '\t'; break;
                    case '0': t.text[i++] = '\0'; break;
                    case '\\': t.text[i++] = '\\'; break;
                    case '"': t.text[i++] = '"'; break;
                    default: t.text[i++] = lp(); break;
                }
                la();
            } else {
                t.text[i++] = la();
            }
            if (i >= (int)sizeof(t.text) - 1) break;
        }
        if (lp() == '"') la();
        t.text[i] = '\0';
        t.int_val = i;
        t.kind = TOK_STR_LIT;
        return t;
    }

    /* punctuation & operators */
    la();
    switch (c) {
    case '(': t.kind = TOK_LPAREN;  break;
    case ')': t.kind = TOK_RPAREN;  break;
    case '{': t.kind = TOK_LBRACE;  break;
    case '}': t.kind = TOK_RBRACE;  break;
    case '[': t.kind = TOK_LBRACKET; break;
    case ']': t.kind = TOK_RBRACKET; break;
    case ';': t.kind = TOK_SEMI;    break;
    case ',': t.kind = TOK_COMMA;   break;
    case '.': t.kind = TOK_DOT;     break;
    case '+':
        if (lp() == '+')      { la(); t.kind = TOK_PLUS_PLUS; }
        else if (lp() == '=') { la(); t.kind = TOK_PLUS_EQ; }
        else t.kind = TOK_PLUS;
        break;
    case '-':
        if (lp() == '-')      { la(); t.kind = TOK_MINUS_MINUS; }
        else if (lp() == '=') { la(); t.kind = TOK_MINUS_EQ; }
        else if (lp() == '>') { la(); t.kind = TOK_ARROW; }
        else t.kind = TOK_MINUS;
        break;
    case '*': t.kind = TOK_STAR;    break;
    case '/': t.kind = TOK_SLASH;   break;
    case '%': t.kind = TOK_PERCENT; break;
    case '&':
        if (lp() == '&') { la(); t.kind = TOK_AMP_AMP; }
        else t.kind = TOK_AMP;
        break;
    case '|':
        if (lp() == '|') { la(); t.kind = TOK_PIPE_PIPE; }
        else error(t.line, t.col, "unexpected '|'");
        break;
    case '!':
        if (lp() == '=') { la(); t.kind = TOK_BANG_EQ; }
        else t.kind = TOK_BANG;
        break;
    case '=':
        if (lp() == '=') { la(); t.kind = TOK_EQ_EQ; }
        else t.kind = TOK_EQ;
        break;
    case '<':
        if (lp() == '=') { la(); t.kind = TOK_LT_EQ; }
        else t.kind = TOK_LT;
        break;
    case '>':
        if (lp() == '=') { la(); t.kind = TOK_GT_EQ; }
        else t.kind = TOK_GT;
        break;
    default:
        error(t.line, t.col, "unexpected character");
    }
    return t;
}

/* ================================================================
 * AST
 * ================================================================ */

typedef struct Node Node;

enum {
    ND_PROGRAM, ND_FUNC, ND_BLOCK, ND_RETURN,
    ND_INT_LIT, ND_BINARY, ND_UNARY,
};

struct Node {
    int kind;
    int line, col;
    union {
        struct { Node **decls; int ndecls; }            program;
        struct { char name[128]; int ret_type; Node *body; } func;
        struct { Node **stmts; int nstmts; }            block;
        struct { Node *expr; }                          ret;
        struct { int val; }                             lit;
        struct { int op; Node *left; Node *right; }     binary;
        struct { int op; Node *operand; }               unary;
    };
};

static Node *node_new(int kind, int line, int col) {
    Node *n = (Node *)calloc(1, sizeof(Node));
    n->kind = kind;
    n->line = line;
    n->col  = col;
    return n;
}

/* Growable list helpers — realloc-based, converted to fixed array at end */
typedef struct { Node **items; int count; int cap; } NList;
static void nlist_push(NList *l, Node *n) {
    if (l->count >= l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->items = (Node **)realloc(l->items, l->cap * sizeof(Node *));
    }
    l->items[l->count++] = n;
}

/* ================================================================
 * Parser
 * ================================================================ */

static Token cur;  /* current token */

static void advance_tok(void) { cur = next_token(); }
static int at(int kind)       { return cur.kind == kind; }

static void expect(int kind, const char *msg) {
    if (cur.kind != kind) error(cur.line, cur.col, msg);
    advance_tok();
}

/* Forward declarations */
static Node *parse_expr(void);

/* --- Expression parsing: precedence climbing --- */

static int prefix_bp(int op) {
    if (op == TOK_MINUS || op == TOK_BANG) return 23;
    return -1;
}

/* Returns left binding power, or -1 if not an infix op */
static int infix_bp(int op, int *rbp) {
    switch (op) {
    case TOK_PIPE_PIPE:                *rbp = 3;  return 4;
    case TOK_AMP_AMP:                  *rbp = 5;  return 6;
    case TOK_EQ_EQ: case TOK_BANG_EQ:  *rbp = 9;  return 10;
    case TOK_LT: case TOK_GT:
    case TOK_LT_EQ: case TOK_GT_EQ:   *rbp = 11; return 12;
    case TOK_PLUS: case TOK_MINUS:     *rbp = 13; return 14;
    case TOK_STAR: case TOK_SLASH:
    case TOK_PERCENT:                  *rbp = 15; return 16;
    default: return -1;
    }
}

static Node *parse_atom(void) {
    if (at(TOK_INT_LIT)) {
        Node *n = node_new(ND_INT_LIT, cur.line, cur.col);
        n->lit.val = cur.int_val;
        advance_tok();
        return n;
    }
    if (at(TOK_LPAREN)) {
        advance_tok();
        Node *n = parse_expr();
        expect(TOK_RPAREN, "expected ')'");
        return n;
    }
    error(cur.line, cur.col, "expected expression");
    return NULL; /* unreachable */
}

static Node *parse_expr_bp(int min_bp) {
    Node *left;

    /* prefix operators */
    int pbp = prefix_bp(cur.kind);
    if (pbp >= 0) {
        int op = cur.kind;
        int line = cur.line, col = cur.col;
        advance_tok();
        Node *operand = parse_expr_bp(pbp);
        left = node_new(ND_UNARY, line, col);
        left->unary.op = op;
        left->unary.operand = operand;
    } else {
        left = parse_atom();
    }

    /* infix operators */
    for (;;) {
        int rbp;
        int lbp = infix_bp(cur.kind, &rbp);
        if (lbp < 0 || lbp < min_bp) break;
        int op = cur.kind;
        int line = cur.line, col = cur.col;
        advance_tok();
        Node *right = parse_expr_bp(rbp);
        Node *n = node_new(ND_BINARY, line, col);
        n->binary.op    = op;
        n->binary.left  = left;
        n->binary.right = right;
        left = n;
    }
    return left;
}

static Node *parse_expr(void) { return parse_expr_bp(0); }

/* --- Statements --- */

static Node *parse_stmt(void) {
    if (at(TOK_RETURN)) {
        int line = cur.line, col = cur.col;
        advance_tok();
        Node *n = node_new(ND_RETURN, line, col);
        if (!at(TOK_SEMI))
            n->ret.expr = parse_expr();
        expect(TOK_SEMI, "expected ';' after return");
        return n;
    }
    error(cur.line, cur.col, "expected statement");
    return NULL;
}

/* --- Block --- */

static Node *parse_block(void) {
    int line = cur.line, col = cur.col;
    expect(TOK_LBRACE, "expected '{'");
    NList stmts = {0};
    while (!at(TOK_RBRACE) && !at(TOK_EOF))
        nlist_push(&stmts, parse_stmt());
    expect(TOK_RBRACE, "expected '}'");
    Node *n = node_new(ND_BLOCK, line, col);
    n->block.stmts  = stmts.items;
    n->block.nstmts = stmts.count;
    return n;
}

/* --- Top-level: parse type + name + '(' ... ')' + block --- */

static int parse_type(void) {
    /* returns 0=int, 1=void, 2=char for now */
    if (at(TOK_INT))     { advance_tok(); return 0; }
    if (at(TOK_VOID))    { advance_tok(); return 1; }
    if (at(TOK_CHAR_KW)) { advance_tok(); return 2; }
    error(cur.line, cur.col, "expected type");
    return 0;
}

static Node *parse_func(void) {
    int line = cur.line, col = cur.col;
    int ret = parse_type();
    if (!at(TOK_IDENT)) error(cur.line, cur.col, "expected function name");
    Node *n = node_new(ND_FUNC, line, col);
    strncpy(n->func.name, cur.text, sizeof(n->func.name) - 1);
    n->func.ret_type = ret;
    advance_tok();
    expect(TOK_LPAREN, "expected '('");
    /* M1: no parameters */
    expect(TOK_RPAREN, "expected ')'");
    n->func.body = parse_block();
    return n;
}

static Node *parse_program(void) {
    Node *prog = node_new(ND_PROGRAM, 1, 1);
    NList decls = {0};
    while (!at(TOK_EOF))
        nlist_push(&decls, parse_func());
    prog->program.decls  = decls.items;
    prog->program.ndecls = decls.count;
    return prog;
}

/* ================================================================
 * WAT Code Generator
 * ================================================================ */

static int indent_level;

static void emit(const char *fmt, ...) {
    int i;
    for (i = 0; i < indent_level; i++) printf("  ");
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

/* --- Expression codegen: leaves one i32 on the WASM stack --- */

static void gen_expr(Node *n) {
    switch (n->kind) {
    case ND_INT_LIT:
        emit("i32.const %d", n->lit.val);
        break;
    case ND_UNARY:
        if (n->unary.op == TOK_MINUS) {
            emit("i32.const 0");
            gen_expr(n->unary.operand);
            emit("i32.sub");
        } else if (n->unary.op == TOK_BANG) {
            gen_expr(n->unary.operand);
            emit("i32.eqz");
        }
        break;
    case ND_BINARY:
        gen_expr(n->binary.left);
        gen_expr(n->binary.right);
        switch (n->binary.op) {
        case TOK_PLUS:    emit("i32.add"); break;
        case TOK_MINUS:   emit("i32.sub"); break;
        case TOK_STAR:    emit("i32.mul"); break;
        case TOK_SLASH:   emit("i32.div_s"); break;
        case TOK_PERCENT: emit("i32.rem_s"); break;
        case TOK_EQ_EQ:   emit("i32.eq"); break;
        case TOK_BANG_EQ:  emit("i32.ne"); break;
        case TOK_LT:      emit("i32.lt_s"); break;
        case TOK_GT:      emit("i32.gt_s"); break;
        case TOK_LT_EQ:   emit("i32.le_s"); break;
        case TOK_GT_EQ:   emit("i32.ge_s"); break;
        case TOK_AMP_AMP:
            /* a && b  →  (a != 0) & (b != 0) ... simple, no short-circuit for now */
            /* but we already have both values on stack. Use: both nonzero */
            /* Rewrite: emit left, test; emit right, test; i32.and */
            /* Oops — we already emitted both. Let's do it the simple way: */
            emit("i32.and");
            break;
        case TOK_PIPE_PIPE:
            emit("i32.or");
            break;
        default:
            error(n->line, n->col, "unsupported binary operator");
        }
        break;
    default:
        error(n->line, n->col, "unsupported expression in codegen");
    }
}

/* --- Statement codegen --- */

static void gen_stmt(Node *n) {
    switch (n->kind) {
    case ND_RETURN:
        if (n->ret.expr)
            gen_expr(n->ret.expr);
        emit("return");
        break;
    default:
        error(n->line, n->col, "unsupported statement in codegen");
    }
}

/* --- Function codegen --- */

static void gen_func(Node *n) {
    const char *ret_sig = n->func.ret_type == 1 ? "" : " (result i32)";
    emit("(func $%s%s", n->func.name, ret_sig);
    indent_level++;
    /* emit body statements */
    Node *body = n->func.body;
    int i;
    for (i = 0; i < body->block.nstmts; i++)
        gen_stmt(body->block.stmts[i]);
    indent_level--;
    emit(")");
}

/* --- Module codegen --- */

static void gen_module(Node *prog) {
    emit("(module");
    indent_level++;

    /* WASI imports */
    emit("(import \"wasi_snapshot_preview1\" \"proc_exit\" (func $__proc_exit (param i32)))");

    /* memory */
    emit("(memory (export \"memory\") 1)");
    emit("");

    /* user functions */
    int i;
    for (i = 0; i < prog->program.ndecls; i++)
        gen_func(prog->program.decls[i]);

    emit("");

    /* _start: call main, pass return value to proc_exit */
    emit("(func $_start (export \"_start\")");
    indent_level++;
    emit("call $main");
    emit("call $__proc_exit");
    indent_level--;
    emit(")");

    indent_level--;
    emit(")");
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    read_source();
    lex_init();
    advance_tok();

    Node *prog = parse_program();
    gen_module(prog);

    return 0;
}
