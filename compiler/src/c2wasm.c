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
    TOK_AMP, TOK_BANG, TOK_PIPE, TOK_TILDE,
    TOK_PIPE_PIPE, TOK_AMP_AMP,
    TOK_EQ, TOK_PLUS_EQ, TOK_MINUS_EQ,
    TOK_EQ_EQ, TOK_BANG_EQ,
    TOK_LT, TOK_GT, TOK_LT_EQ, TOK_GT_EQ,
    TOK_LSHIFT, TOK_RSHIFT,
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

/* ================================================================
 * Macro table for #define
 * ================================================================ */

#define MAX_MACROS 256
static struct { char name[128]; int value; } macros[MAX_MACROS];
static int nmacros;

static int find_macro(const char *name) {
    for (int i = 0; i < nmacros; i++)
        if (!strcmp(macros[i].name, name)) return i;
    return -1;
}

static Token next_token(void) {
    skip_ws();
    Token t;
    memset(&t, 0, sizeof(t));
    t.line = L.line;
    t.col  = L.col;

    if (L.pos >= src_len) { t.kind = TOK_EOF; return t; }
    char c = lp();

    /* preprocessor directives */
    if (c == '#') {
        la();
        while (lp() == ' ' || lp() == '\t') la();
        int s = L.pos;
        while (isalpha(lp())) la();
        int dlen = L.pos - s;
        if (dlen == 6 && !memcmp(src + s, "define", 6)) {
            while (lp() == ' ' || lp() == '\t') la();
            char name[128];
            int ni = 0;
            while (isalnum(lp()) || lp() == '_') {
                if (ni < 127) name[ni++] = la(); else la();
            }
            name[ni] = '\0';
            while (lp() == ' ' || lp() == '\t') la();
            int neg = 0;
            if (lp() == '-') { neg = 1; la(); while (lp() == ' ') la(); }
            int val = 0;
            if (lp() == '0' && (L.pos+1 < src_len) && (src[L.pos+1] == 'x' || src[L.pos+1] == 'X')) {
                la(); la();
                while (isxdigit(lp())) {
                    char d = la();
                    val = val * 16 + (d <= '9' ? d - '0' : (d | 32) - 'a' + 10);
                }
            } else {
                while (isdigit(lp())) val = val * 10 + (la() - '0');
            }
            if (neg) val = -val;
            strncpy(macros[nmacros].name, name, 127);
            macros[nmacros].value = val;
            nmacros++;
        }
        /* skip to end of line (handles #include and unknown directives) */
        while (L.pos < src_len && lp() != '\n') la();
        return next_token(); /* recurse to get real token */
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
        /* macro substitution: if identifier matches a #define, return int literal */
        if (t.kind == TOK_IDENT) {
            int mi = find_macro(t.text);
            if (mi >= 0) {
                t.kind = TOK_INT_LIT;
                t.int_val = macros[mi].value;
            }
        }
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
        else t.kind = TOK_PIPE;
        break;
    case '~':
        t.kind = TOK_TILDE;
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
        else if (lp() == '<') { la(); t.kind = TOK_LSHIFT; }
        else t.kind = TOK_LT;
        break;
    case '>':
        if (lp() == '=') { la(); t.kind = TOK_GT_EQ; }
        else if (lp() == '>') { la(); t.kind = TOK_RSHIFT; }
        else t.kind = TOK_GT;
        break;
    default:
        error(t.line, t.col, "unexpected character");
    }
    return t;
}

/* ================================================================
 * String Literal Table (populated during parsing, emitted during codegen)
 * ================================================================ */

#define MAX_STRINGS 512
#define MAX_STR_DATA 512
static struct {
    char data[MAX_STR_DATA];
    int len;
    int offset;
} str_table[MAX_STRINGS];
static int nstrings;
static int data_ptr = 1024; /* static data starts at 1024 (0-1023 reserved for I/O) */

static int add_string(const char *data, int len) {
    int id = nstrings++;
    if (len >= MAX_STR_DATA) len = MAX_STR_DATA - 1;
    memcpy(str_table[id].data, data, len);
    str_table[id].data[len] = '\0';
    str_table[id].len = len;
    str_table[id].offset = data_ptr;
    data_ptr += len + 1;
    return id;
}

/* ================================================================
 * Struct Type Registry
 * ================================================================ */

#define MAX_STRUCTS 64
#define MAX_FIELDS 32

typedef struct {
    char name[128];
    struct { char name[128]; int offset; } fields[MAX_FIELDS];
    int nfields;
    int size;
} StructDef;

static StructDef structs_reg[MAX_STRUCTS];
static int nstructs;

static StructDef *find_struct(const char *name) {
    for (int i = 0; i < nstructs; i++)
        if (!strcmp(structs_reg[i].name, name)) return &structs_reg[i];
    return NULL;
}

static int resolve_field_offset(const char *field_name) {
    for (int i = 0; i < nstructs; i++)
        for (int j = 0; j < structs_reg[i].nfields; j++)
            if (!strcmp(structs_reg[i].fields[j].name, field_name))
                return structs_reg[i].fields[j].offset;
    return -1;
}

/* ================================================================
 * Global Variable Table
 * ================================================================ */

#define MAX_GLOBALS 256
static struct { char name[128]; int init_val; int is_char; } globals_tbl[MAX_GLOBALS];
static int nglobals;

static int find_global(const char *name) {
    for (int i = 0; i < nglobals; i++)
        if (!strcmp(globals_tbl[i].name, name)) return i;
    return -1;
}

/* ================================================================
 * Function Signature Table (tracks void vs non-void return)
 * ================================================================ */

#define MAX_FUNC_SIGS 256
static struct { char name[128]; int is_void; } func_sigs[MAX_FUNC_SIGS];
static int nfunc_sigs;

static int func_is_void(const char *name) {
    for (int i = 0; i < nfunc_sigs; i++)
        if (!strcmp(func_sigs[i].name, name)) return func_sigs[i].is_void;
    return 0; /* unknown = assume non-void */
}

/* ================================================================
 * AST
 * ================================================================ */

typedef struct Node Node;

enum {
    ND_PROGRAM, ND_FUNC, ND_BLOCK, ND_RETURN,
    ND_INT_LIT, ND_BINARY, ND_UNARY,
    ND_VAR_DECL, ND_IDENT, ND_ASSIGN,
    ND_IF, ND_WHILE, ND_FOR,
    ND_BREAK, ND_CONTINUE, ND_EXPR_STMT,
    ND_CALL,
    ND_STR_LIT,
    ND_MEMBER, ND_SIZEOF,
    ND_SUBSCRIPT,
};

struct Node {
    int kind;
    int line, col;
    union {
        struct { Node **decls; int ndecls; }                    program;
        struct { char name[128]; int ret_type;
                 char param_names[8][128]; int param_count;
                 Node *body; /* NULL for prototypes */ }    func;
        struct { Node **stmts; int nstmts; }                    block;
        struct { Node *expr; }                                  ret;
        struct { int val; }                                     lit;
        struct { int op; Node *left; Node *right; }             binary;
        struct { int op; Node *operand; }                       unary;
        struct { char name[128]; Node *init; int is_char; }       var_decl;
        struct { char name[128]; }                              ident;
        struct { Node *target; Node *value; int op; }           assign;
        struct { Node *cond; Node *then_b; Node *else_b; }     if_s;
        struct { Node *cond; Node *body; }                      while_s;
        struct { Node *init; Node *cond; Node *step; Node *body; } for_s;
        struct { Node *expr; }                                  expr_stmt;
        struct { char name[128]; Node **args; int nargs; }      call;
        struct { int id; int len; }                             str_lit;
        struct { Node *base; char field[128]; int is_arrow; }   member;
        struct { char type_name[128]; }                         sizeof_expr;
        struct { Node *base; Node *index; }                     subscript;
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

static int is_type_token(void) {
    return at(TOK_INT) || at(TOK_CHAR_KW) || at(TOK_VOID) || at(TOK_STRUCT);
}

/* Forward declarations */
static Node *parse_expr(void);
static Node *parse_expr_bp(int min_bp);
static Node *parse_stmt(void);
static Node *parse_block(void);

/* --- Expression parsing: precedence climbing --- */

static int prefix_bp(int op) {
    if (op == TOK_MINUS || op == TOK_BANG) return 25;
    if (op == TOK_STAR || op == TOK_AMP) return 25;
    if (op == TOK_TILDE) return 25;
    if (op == TOK_PLUS_PLUS || op == TOK_MINUS_MINUS) return 25;
    return -1;
}

/* Returns left binding power, or -1 if not an infix op */
static int infix_bp(int op, int *rbp) {
    switch (op) {
    case TOK_EQ: case TOK_PLUS_EQ:
    case TOK_MINUS_EQ:                     *rbp = 1;  return 2;  /* right-assoc */
    case TOK_PIPE_PIPE:                    *rbp = 5;  return 4;
    case TOK_AMP_AMP:                      *rbp = 7;  return 6;
    case TOK_PIPE:                         *rbp = 9;  return 8;
    case TOK_AMP:                          *rbp = 11; return 10; /* bitwise AND */
    case TOK_EQ_EQ: case TOK_BANG_EQ:      *rbp = 13; return 12;
    case TOK_LT: case TOK_GT:
    case TOK_LT_EQ: case TOK_GT_EQ:       *rbp = 15; return 14;
    case TOK_LSHIFT: case TOK_RSHIFT:      *rbp = 17; return 16;
    case TOK_PLUS: case TOK_MINUS:         *rbp = 19; return 18;
    case TOK_STAR: case TOK_SLASH:
    case TOK_PERCENT:                      *rbp = 21; return 20;
    case TOK_DOT: case TOK_ARROW:          *rbp = 30; return 29;
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
    if (at(TOK_CHAR_LIT)) {
        Node *n = node_new(ND_INT_LIT, cur.line, cur.col);
        n->lit.val = cur.int_val;
        advance_tok();
        return n;
    }
    if (at(TOK_SIZEOF)) {
        int line = cur.line, col = cur.col;
        advance_tok();
        expect(TOK_LPAREN, "expected '(' after sizeof");
        Node *n = node_new(ND_SIZEOF, line, col);
        if (at(TOK_STRUCT)) {
            advance_tok();
            if (at(TOK_IDENT)) {
                strncpy(n->sizeof_expr.type_name, cur.text,
                        sizeof(n->sizeof_expr.type_name) - 1);
                advance_tok();
            }
        } else if (at(TOK_INT) || at(TOK_CHAR_KW) || at(TOK_VOID)) {
            strncpy(n->sizeof_expr.type_name, cur.text,
                    sizeof(n->sizeof_expr.type_name) - 1);
            advance_tok();
            while (at(TOK_STAR)) advance_tok();
        }
        expect(TOK_RPAREN, "expected ')' after sizeof");
        return n;
    }
    if (at(TOK_STR_LIT)) {
        Node *n = node_new(ND_STR_LIT, cur.line, cur.col);
        n->str_lit.id = add_string(cur.text, cur.int_val);
        n->str_lit.len = cur.int_val;
        advance_tok();
        return n;
    }
    if (at(TOK_IDENT)) {
        Node *n = node_new(ND_IDENT, cur.line, cur.col);
        strncpy(n->ident.name, cur.text, sizeof(n->ident.name) - 1);
        advance_tok();
        /* function call: ident '(' args ')' */
        if (at(TOK_LPAREN)) {
            advance_tok();
            Node *c = node_new(ND_CALL, n->line, n->col);
            strncpy(c->call.name, n->ident.name, sizeof(c->call.name) - 1);
            NList args = {0};
            if (!at(TOK_RPAREN)) {
                nlist_push(&args, parse_expr());
                while (at(TOK_COMMA)) {
                    advance_tok();
                    nlist_push(&args, parse_expr());
                }
            }
            expect(TOK_RPAREN, "expected ')' after arguments");
            c->call.args  = args.items;
            c->call.nargs = args.count;
            return c;
        }
        return n;
    }
    if (at(TOK_LPAREN)) {
        advance_tok();
        /* type cast: (int)expr, (char *)expr, (struct Name *)expr — no-op, all i32 */
        if (is_type_token()) {
            if (at(TOK_STRUCT)) { advance_tok(); if (at(TOK_IDENT)) advance_tok(); }
            else advance_tok();
            while (at(TOK_STAR)) advance_tok();
            expect(TOK_RPAREN, "expected ')' after cast type");
            return parse_expr_bp(25); /* same precedence as prefix unary */
        }
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
        /* prefix ++/-- desugar to compound assignment */
        if (op == TOK_PLUS_PLUS || op == TOK_MINUS_MINUS) {
            Node *one = node_new(ND_INT_LIT, line, col);
            one->lit.val = 1;
            left = node_new(ND_ASSIGN, line, col);
            left->assign.target = operand;
            left->assign.value  = one;
            left->assign.op = (op == TOK_PLUS_PLUS) ? TOK_PLUS_EQ : TOK_MINUS_EQ;
        } else {
            left = node_new(ND_UNARY, line, col);
            left->unary.op = op;
            left->unary.operand = operand;
        }
    } else {
        left = parse_atom();
    }

    /* infix operators */
    for (;;) {
        /* array subscript: arr[idx] → ND_SUBSCRIPT node */
        if (at(TOK_LBRACKET) && 29 >= min_bp) {
            int line = cur.line, col = cur.col;
            advance_tok(); /* skip '[' */
            Node *idx = parse_expr();
            expect(TOK_RBRACKET, "expected ']'");
            Node *n = node_new(ND_SUBSCRIPT, line, col);
            n->subscript.base = left;
            n->subscript.index = idx;
            left = n;
            continue;
        }

        int rbp;
        int lbp = infix_bp(cur.kind, &rbp);
        if (lbp < 0 || lbp < min_bp) break;
        int op = cur.kind;
        int line = cur.line, col = cur.col;
        advance_tok();

        /* member access: . and -> consume a field name, not an expression */
        if (op == TOK_DOT || op == TOK_ARROW) {
            if (!at(TOK_IDENT)) error(line, col, "expected field name");
            Node *n = node_new(ND_MEMBER, line, col);
            n->member.base = left;
            strncpy(n->member.field, cur.text, sizeof(n->member.field) - 1);
            n->member.is_arrow = (op == TOK_ARROW);
            advance_tok();
            left = n;
            continue;
        }

        Node *right = parse_expr_bp(rbp);

        /* assignment operators produce ND_ASSIGN */
        if (op == TOK_EQ || op == TOK_PLUS_EQ || op == TOK_MINUS_EQ) {
            int valid = (left->kind == ND_IDENT) ||
                        (left->kind == ND_UNARY && left->unary.op == TOK_STAR) ||
                        (left->kind == ND_MEMBER) ||
                        (left->kind == ND_SUBSCRIPT);
            if (!valid)
                error(line, col, "invalid assignment target");
            Node *n = node_new(ND_ASSIGN, line, col);
            n->assign.target = left;
            n->assign.value  = right;
            n->assign.op     = op;
            left = n;
        } else {
            Node *n = node_new(ND_BINARY, line, col);
            n->binary.op    = op;
            n->binary.left  = left;
            n->binary.right = right;
            left = n;
        }
    }
    return left;
}

static Node *parse_expr(void) { return parse_expr_bp(0); }

/* --- Statements --- */

static Node *parse_var_decl(void) {
    int line = cur.line, col = cur.col;
    int is_char = 0;
    if (at(TOK_STRUCT)) {
        advance_tok(); /* skip 'struct' */
        advance_tok(); /* skip struct name */
    } else {
        if (at(TOK_CHAR_KW)) is_char = 1;
        advance_tok(); /* skip type keyword */
    }
    while (at(TOK_STAR)) advance_tok();
    if (!at(TOK_IDENT)) error(cur.line, cur.col, "expected variable name");
    Node *n = node_new(ND_VAR_DECL, line, col);
    strncpy(n->var_decl.name, cur.text, sizeof(n->var_decl.name) - 1);
    n->var_decl.is_char = is_char;
    advance_tok();
    if (at(TOK_EQ)) {
        advance_tok();
        n->var_decl.init = parse_expr();
    }
    expect(TOK_SEMI, "expected ';' after declaration");
    return n;
}

static Node *parse_if(void) {
    int line = cur.line, col = cur.col;
    advance_tok(); /* skip 'if' */
    expect(TOK_LPAREN, "expected '(' after 'if'");
    Node *cond = parse_expr();
    expect(TOK_RPAREN, "expected ')'");
    Node *then_b = parse_stmt();
    Node *else_b = NULL;
    if (at(TOK_ELSE)) {
        advance_tok();
        else_b = parse_stmt();
    }
    Node *n = node_new(ND_IF, line, col);
    n->if_s.cond   = cond;
    n->if_s.then_b = then_b;
    n->if_s.else_b = else_b;
    return n;
}

static Node *parse_while(void) {
    int line = cur.line, col = cur.col;
    advance_tok();
    expect(TOK_LPAREN, "expected '(' after 'while'");
    Node *cond = parse_expr();
    expect(TOK_RPAREN, "expected ')'");
    Node *body = parse_stmt();
    Node *n = node_new(ND_WHILE, line, col);
    n->while_s.cond = cond;
    n->while_s.body = body;
    return n;
}

static Node *parse_for(void) {
    int line = cur.line, col = cur.col;
    advance_tok();
    expect(TOK_LPAREN, "expected '(' after 'for'");

    /* init */
    Node *init = NULL;
    if (is_type_token()) {
        init = parse_var_decl();
    } else if (!at(TOK_SEMI)) {
        init = node_new(ND_EXPR_STMT, cur.line, cur.col);
        init->expr_stmt.expr = parse_expr();
        expect(TOK_SEMI, "expected ';'");
    } else {
        advance_tok(); /* skip ';' */
    }

    /* cond */
    Node *cond = NULL;
    if (!at(TOK_SEMI)) cond = parse_expr();
    expect(TOK_SEMI, "expected ';'");

    /* step */
    Node *step = NULL;
    if (!at(TOK_RPAREN)) step = parse_expr();
    expect(TOK_RPAREN, "expected ')'");

    Node *body = parse_stmt();
    Node *n = node_new(ND_FOR, line, col);
    n->for_s.init = init;
    n->for_s.cond = cond;
    n->for_s.step = step;
    n->for_s.body = body;
    return n;
}

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
    if (at(TOK_IF))       return parse_if();
    if (at(TOK_WHILE))    return parse_while();
    if (at(TOK_FOR))      return parse_for();
    if (at(TOK_BREAK)) {
        int line = cur.line, col = cur.col;
        advance_tok();
        expect(TOK_SEMI, "expected ';' after break");
        return node_new(ND_BREAK, line, col);
    }
    if (at(TOK_CONTINUE)) {
        int line = cur.line, col = cur.col;
        advance_tok();
        expect(TOK_SEMI, "expected ';' after continue");
        return node_new(ND_CONTINUE, line, col);
    }
    if (at(TOK_LBRACE)) return parse_block();
    if (is_type_token()) return parse_var_decl();

    /* expression statement */
    int line = cur.line, col = cur.col;
    Node *expr = parse_expr();
    expect(TOK_SEMI, "expected ';'");
    Node *n = node_new(ND_EXPR_STMT, line, col);
    n->expr_stmt.expr = expr;
    return n;
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
    /* returns 0=int, 1=void, 2=char for now; struct treated as int (all ptrs are i32) */
    if (at(TOK_INT))     { advance_tok(); return 0; }
    if (at(TOK_VOID))    { advance_tok(); return 1; }
    if (at(TOK_CHAR_KW)) { advance_tok(); return 2; }
    if (at(TOK_STRUCT))  { advance_tok(); if (at(TOK_IDENT)) advance_tok(); return 0; }
    error(cur.line, cur.col, "expected type");
    return 0;
}

static Node *parse_func(void) {
    int line = cur.line, col = cur.col;
    int ret = parse_type();
    while (at(TOK_STAR)) advance_tok(); /* skip ptr star on return type */
    if (!at(TOK_IDENT)) error(cur.line, cur.col, "expected function name");
    Node *n = node_new(ND_FUNC, line, col);
    strncpy(n->func.name, cur.text, sizeof(n->func.name) - 1);
    n->func.ret_type = ret;
    /* register in function signature table */
    if (nfunc_sigs < MAX_FUNC_SIGS) {
        strncpy(func_sigs[nfunc_sigs].name, cur.text, 127);
        func_sigs[nfunc_sigs].is_void = (ret == 1);
        nfunc_sigs++;
    }
    advance_tok();
    expect(TOK_LPAREN, "expected '('");

    /* parameters */
    n->func.param_count = 0;
    if (!at(TOK_RPAREN)) {
        if (at(TOK_VOID)) {
            advance_tok();
        } else {
            parse_type();
            while (at(TOK_STAR)) advance_tok();
            if (at(TOK_IDENT)) {
                strncpy(n->func.param_names[n->func.param_count], cur.text, 127);
                n->func.param_count++;
                advance_tok();
            }
            while (at(TOK_COMMA)) {
                advance_tok();
                parse_type();
                while (at(TOK_STAR)) advance_tok();
                if (at(TOK_IDENT)) {
                    strncpy(n->func.param_names[n->func.param_count], cur.text, 127);
                    n->func.param_count++;
                    advance_tok();
                }
            }
        }
    }
    expect(TOK_RPAREN, "expected ')'");

    if (at(TOK_SEMI)) {
        advance_tok();
        n->func.body = NULL;
    } else {
        n->func.body = parse_block();
    }
    return n;
}

/* --- Top-level struct definition --- */

static void parse_struct_def(void) {
    advance_tok(); /* skip 'struct' */
    if (!at(TOK_IDENT)) error(cur.line, cur.col, "expected struct name");
    if (nstructs >= MAX_STRUCTS) error(cur.line, cur.col, "too many structs");
    StructDef *sd = &structs_reg[nstructs++];
    strncpy(sd->name, cur.text, sizeof(sd->name) - 1);
    sd->nfields = 0;
    advance_tok(); /* skip name */
    expect(TOK_LBRACE, "expected '{' in struct definition");
    int offset = 0;
    while (!at(TOK_RBRACE) && !at(TOK_EOF)) {
        /* field: type *name ; */
        if (at(TOK_STRUCT)) {
            advance_tok();
            if (at(TOK_IDENT)) advance_tok();
        } else {
            parse_type();
        }
        while (at(TOK_STAR)) advance_tok();
        if (!at(TOK_IDENT)) error(cur.line, cur.col, "expected field name");
        strncpy(sd->fields[sd->nfields].name, cur.text,
                sizeof(sd->fields[sd->nfields].name) - 1);
        sd->fields[sd->nfields].offset = offset;
        sd->nfields++;
        offset += 4;
        advance_tok();
        expect(TOK_SEMI, "expected ';' after field");
    }
    expect(TOK_RBRACE, "expected '}'");
    sd->size = offset;
    expect(TOK_SEMI, "expected ';' after struct definition");
}

/* --- Parse top-level global variable --- */

static void parse_global_var(void) {
    int is_char = 0;
    if (at(TOK_STRUCT)) { advance_tok(); if (at(TOK_IDENT)) advance_tok(); }
    else {
        if (at(TOK_CHAR_KW)) is_char = 1;
        advance_tok(); /* skip type */
    }
    while (at(TOK_STAR)) advance_tok();
    if (!at(TOK_IDENT)) error(cur.line, cur.col, "expected variable name");
    if (nglobals >= MAX_GLOBALS) error(cur.line, cur.col, "too many globals");
    strncpy(globals_tbl[nglobals].name, cur.text, 127);
    globals_tbl[nglobals].init_val = 0;
    globals_tbl[nglobals].is_char = is_char;
    advance_tok();
    if (at(TOK_EQ)) {
        advance_tok();
        if (at(TOK_INT_LIT)) {
            globals_tbl[nglobals].init_val = cur.int_val;
            advance_tok();
        } else if (at(TOK_MINUS) || at(TOK_CHAR_LIT)) {
            /* handle negative literals or char lit */
            int neg = 0;
            if (at(TOK_MINUS)) { neg = 1; advance_tok(); }
            globals_tbl[nglobals].init_val = neg ? -cur.int_val : cur.int_val;
            advance_tok();
        }
    }
    nglobals++;
    expect(TOK_SEMI, "expected ';' after global declaration");
}

static Node *parse_program(void) {
    Node *prog = node_new(ND_PROGRAM, 1, 1);
    NList decls = {0};
    while (!at(TOK_EOF)) {
        /* struct definition: struct Name { ... }; */
        if (at(TOK_STRUCT)) {
            int sp = L.pos, sl = L.line, sc = L.col;
            Token st = cur;
            advance_tok(); /* skip 'struct' */
            int is_def = 0;
            if (at(TOK_IDENT)) {
                advance_tok();
                is_def = at(TOK_LBRACE);
            }
            L.pos = sp; L.line = sl; L.col = sc;
            cur = st;
            if (is_def) { parse_struct_def(); continue; }
        }

        /* Peek ahead to distinguish function from global variable */
        {
            int sp = L.pos, sl = L.line, sc = L.col;
            Token st = cur;
            /* skip type (including struct Name) */
            if (at(TOK_STRUCT)) { advance_tok(); if (at(TOK_IDENT)) advance_tok(); }
            else advance_tok();
            while (at(TOK_STAR)) advance_tok();
            /* skip name */
            int is_func = 0;
            if (at(TOK_IDENT)) { advance_tok(); is_func = at(TOK_LPAREN); }
            L.pos = sp; L.line = sl; L.col = sc;
            cur = st;
            if (is_func) {
                nlist_push(&decls, parse_func());
            } else {
                parse_global_var();
            }
        }
    }
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

/* --- Local variable tracking --- */

#define MAX_LOCALS 256
static char local_names[MAX_LOCALS][128];
static int local_is_char[MAX_LOCALS];
static int nlocals;

static int find_local(const char *name) {
    for (int i = 0; i < nlocals; i++)
        if (!strcmp(local_names[i], name)) return i;
    return -1;
}

static void add_local(const char *name, int is_char) {
    if (find_local(name) >= 0) return;
    if (nlocals >= MAX_LOCALS) { fprintf(stderr, "too many locals\n"); exit(1); }
    strncpy(local_names[nlocals], name, 127);
    local_names[nlocals][127] = '\0';
    local_is_char[nlocals] = is_char;
    nlocals++;
}

static void collect_locals(Node *n) {
    if (!n) return;
    switch (n->kind) {
    case ND_VAR_DECL:  add_local(n->var_decl.name, n->var_decl.is_char); break;
    case ND_BLOCK:
        for (int i = 0; i < n->block.nstmts; i++)
            collect_locals(n->block.stmts[i]);
        break;
    case ND_IF:
        collect_locals(n->if_s.then_b);
        collect_locals(n->if_s.else_b);
        break;
    case ND_WHILE:  collect_locals(n->while_s.body); break;
    case ND_FOR:
        collect_locals(n->for_s.init);
        collect_locals(n->for_s.body);
        break;
    default: break;
    }
}

/* --- Loop label management --- */

static int label_cnt;
#define MAX_LOOP_DEPTH 64
static int brk_lbl[MAX_LOOP_DEPTH];
static int cont_lbl[MAX_LOOP_DEPTH];
static int loop_sp;

/* --- Variable type lookup: returns element size (1 for char, 4 for int/ptr) --- */

static int var_elem_size(const char *name) {
    int li = find_local(name);
    if (li >= 0) return local_is_char[li] ? 1 : 4;
    int gi = find_global(name);
    if (gi >= 0) return globals_tbl[gi].is_char ? 1 : 4;
    return 4; /* default: 4-byte elements */
}

/* infer element size from an expression (for subscript/deref) */
static int expr_elem_size(Node *n) {
    if (n->kind == ND_IDENT) return var_elem_size(n->ident.name);
    return 4;
}

/* --- Expression codegen: leaves one i32 on the WASM stack --- */

static void gen_expr(Node *n) {
    switch (n->kind) {
    case ND_INT_LIT:
        emit("i32.const %d", n->lit.val);
        break;
    case ND_IDENT:
        if (find_global(n->ident.name) >= 0)
            emit("global.get $%s", n->ident.name);
        else
            emit("local.get $%s", n->ident.name);
        break;
    case ND_ASSIGN: {
        Node *tgt = n->assign.target;
        if (tgt->kind == ND_IDENT) {
            const char *name = tgt->ident.name;
            int is_global = (find_global(name) >= 0);
            const char *get = is_global ? "global.get" : "local.get";
            if (n->assign.op == TOK_EQ) {
                gen_expr(n->assign.value);
            } else if (n->assign.op == TOK_PLUS_EQ) {
                emit("%s $%s", get, name);
                gen_expr(n->assign.value);
                emit("i32.add");
            } else if (n->assign.op == TOK_MINUS_EQ) {
                emit("%s $%s", get, name);
                gen_expr(n->assign.value);
                emit("i32.sub");
            }
            if (is_global) {
                emit("local.set $__atmp");
                emit("local.get $__atmp");
                emit("global.set $%s", name);
                emit("local.get $__atmp");
            } else {
                emit("local.tee $%s", name);
            }
        } else if (tgt->kind == ND_UNARY && tgt->unary.op == TOK_STAR) {
            /* *p = val  or  *p += val */
            int esz = expr_elem_size(tgt->unary.operand);
            const char *load_op = (esz == 1) ? "i32.load8_u" : "i32.load";
            const char *store_op = (esz == 1) ? "i32.store8" : "i32.store";
            if (n->assign.op == TOK_EQ) {
                gen_expr(n->assign.value);
            } else if (n->assign.op == TOK_PLUS_EQ) {
                gen_expr(tgt->unary.operand);
                emit("%s", load_op);
                gen_expr(n->assign.value);
                emit("i32.add");
            } else if (n->assign.op == TOK_MINUS_EQ) {
                gen_expr(tgt->unary.operand);
                emit("%s", load_op);
                gen_expr(n->assign.value);
                emit("i32.sub");
            }
            emit("local.set $__atmp");
            gen_expr(tgt->unary.operand);
            emit("local.get $__atmp");
            emit("%s", store_op);
            emit("local.get $__atmp");
        } else if (tgt->kind == ND_MEMBER) {
            int off = resolve_field_offset(tgt->member.field);
            if (off < 0) error(tgt->line, tgt->col, "unknown struct field");
            if (n->assign.op == TOK_EQ) {
                gen_expr(n->assign.value);
            } else if (n->assign.op == TOK_PLUS_EQ) {
                gen_expr(tgt->member.base);
                if (off > 0) { emit("i32.const %d", off); emit("i32.add"); }
                emit("i32.load");
                gen_expr(n->assign.value);
                emit("i32.add");
            } else if (n->assign.op == TOK_MINUS_EQ) {
                gen_expr(tgt->member.base);
                if (off > 0) { emit("i32.const %d", off); emit("i32.add"); }
                emit("i32.load");
                gen_expr(n->assign.value);
                emit("i32.sub");
            }
            emit("local.set $__atmp");
            gen_expr(tgt->member.base);
            if (off > 0) { emit("i32.const %d", off); emit("i32.add"); }
            emit("local.get $__atmp");
            emit("i32.store");
            emit("local.get $__atmp");
        } else if (tgt->kind == ND_SUBSCRIPT) {
            int esz = expr_elem_size(tgt->subscript.base);
            const char *load_op = (esz == 1) ? "i32.load8_u" : "i32.load";
            const char *store_op = (esz == 1) ? "i32.store8" : "i32.store";
            if (n->assign.op == TOK_EQ) {
                gen_expr(n->assign.value);
            } else if (n->assign.op == TOK_PLUS_EQ) {
                gen_expr(tgt->subscript.base);
                gen_expr(tgt->subscript.index);
                if (esz > 1) { emit("i32.const %d", esz); emit("i32.mul"); }
                emit("i32.add");
                emit("%s", load_op);
                gen_expr(n->assign.value);
                emit("i32.add");
            } else if (n->assign.op == TOK_MINUS_EQ) {
                gen_expr(tgt->subscript.base);
                gen_expr(tgt->subscript.index);
                if (esz > 1) { emit("i32.const %d", esz); emit("i32.mul"); }
                emit("i32.add");
                emit("%s", load_op);
                gen_expr(n->assign.value);
                emit("i32.sub");
            }
            emit("local.set $__atmp");
            gen_expr(tgt->subscript.base);
            gen_expr(tgt->subscript.index);
            if (esz > 1) { emit("i32.const %d", esz); emit("i32.mul"); }
            emit("i32.add");
            emit("local.get $__atmp");
            emit("%s", store_op);
            emit("local.get $__atmp");
        }
        break;
    }
    case ND_UNARY:
        if (n->unary.op == TOK_MINUS) {
            emit("i32.const 0");
            gen_expr(n->unary.operand);
            emit("i32.sub");
        } else if (n->unary.op == TOK_BANG) {
            gen_expr(n->unary.operand);
            emit("i32.eqz");
        } else if (n->unary.op == TOK_TILDE) {
            emit("i32.const -1");
            gen_expr(n->unary.operand);
            emit("i32.xor");
        } else if (n->unary.op == TOK_STAR) {
            int esz = expr_elem_size(n->unary.operand);
            gen_expr(n->unary.operand);
            emit(esz == 1 ? "i32.load8_u" : "i32.load");
        } else if (n->unary.op == TOK_AMP) {
            error(n->line, n->col, "cannot take address of this expression");
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
            emit("i32.and");
            break;
        case TOK_PIPE_PIPE:
            emit("i32.or");
            break;
        case TOK_AMP:     emit("i32.and"); break;
        case TOK_PIPE:    emit("i32.or"); break;
        case TOK_LSHIFT:  emit("i32.shl"); break;
        case TOK_RSHIFT:  emit("i32.shr_s"); break;
        default:
            error(n->line, n->col, "unsupported binary operator");
        }
        break;
    case ND_CALL:
        if (!strcmp(n->call.name, "printf")) {
            /* compile-time printf lowering */
            if (n->call.nargs < 1 || n->call.args[0]->kind != ND_STR_LIT)
                error(n->line, n->col, "printf requires string literal format");
            int sid = n->call.args[0]->str_lit.id;
            char *fmt = str_table[sid].data;
            int flen = str_table[sid].len;
            int ai = 1;
            for (int fi = 0; fi < flen; fi++) {
                if (fmt[fi] == '%' && fi + 1 < flen) {
                    fi++;
                    switch (fmt[fi]) {
                    case 'd':
                        if (ai >= n->call.nargs) error(n->line, n->col, "printf: missing arg for %d");
                        gen_expr(n->call.args[ai++]);
                        emit("call $__print_int");
                        break;
                    case 's':
                        if (ai >= n->call.nargs) error(n->line, n->col, "printf: missing arg for %s");
                        gen_expr(n->call.args[ai++]);
                        emit("call $__print_str");
                        break;
                    case 'c':
                        if (ai >= n->call.nargs) error(n->line, n->col, "printf: missing arg for %c");
                        gen_expr(n->call.args[ai++]);
                        emit("call $putchar");
                        emit("drop");
                        break;
                    case 'x':
                        if (ai >= n->call.nargs) error(n->line, n->col, "printf: missing arg for %x");
                        gen_expr(n->call.args[ai++]);
                        emit("call $__print_hex");
                        break;
                    case '%':
                        emit("i32.const 37");
                        emit("call $putchar");
                        emit("drop");
                        break;
                    default:
                        error(n->line, n->col, "unsupported printf format");
                    }
                } else {
                    emit("i32.const %d", (unsigned char)fmt[fi]);
                    emit("call $putchar");
                    emit("drop");
                }
            }
            emit("i32.const 0");
        } else if (!strcmp(n->call.name, "putchar")) {
            gen_expr(n->call.args[0]);
            emit("call $putchar");
        } else if (!strcmp(n->call.name, "getchar")) {
            emit("call $getchar");
        } else if (!strcmp(n->call.name, "exit")) {
            gen_expr(n->call.args[0]);
            emit("call $__proc_exit");
            emit("i32.const 0");
        } else if (!strcmp(n->call.name, "malloc")) {
            gen_expr(n->call.args[0]);
            emit("call $malloc");
        } else if (!strcmp(n->call.name, "free")) {
            if (n->call.nargs > 0) gen_expr(n->call.args[0]);
            else emit("i32.const 0");
            emit("call $free");
            emit("i32.const 0");
        } else if (!strcmp(n->call.name, "strlen")) {
            gen_expr(n->call.args[0]);
            emit("call $strlen");
        } else if (!strcmp(n->call.name, "strcmp")) {
            gen_expr(n->call.args[0]);
            gen_expr(n->call.args[1]);
            emit("call $strcmp");
        } else if (!strcmp(n->call.name, "strncpy")) {
            gen_expr(n->call.args[0]);
            gen_expr(n->call.args[1]);
            gen_expr(n->call.args[2]);
            emit("call $strncpy");
        } else if (!strcmp(n->call.name, "memcpy")) {
            gen_expr(n->call.args[0]);
            gen_expr(n->call.args[1]);
            gen_expr(n->call.args[2]);
            emit("call $memcpy");
        } else if (!strcmp(n->call.name, "memset")) {
            gen_expr(n->call.args[0]);
            gen_expr(n->call.args[1]);
            gen_expr(n->call.args[2]);
            emit("call $memset");
        } else if (!strcmp(n->call.name, "memcmp")) {
            gen_expr(n->call.args[0]);
            gen_expr(n->call.args[1]);
            gen_expr(n->call.args[2]);
            emit("call $memcmp");
        } else {
            for (int i = 0; i < n->call.nargs; i++)
                gen_expr(n->call.args[i]);
            emit("call $%s", n->call.name);
            if (func_is_void(n->call.name))
                emit("i32.const 0"); /* void call in expr context: push dummy */
        }
        break;
    case ND_STR_LIT:
        emit("i32.const %d", str_table[n->str_lit.id].offset);
        break;
    case ND_MEMBER: {
        int off = resolve_field_offset(n->member.field);
        if (off < 0) error(n->line, n->col, "unknown struct field");
        gen_expr(n->member.base);
        if (off > 0) { emit("i32.const %d", off); emit("i32.add"); }
        emit("i32.load");
        break;
    }
    case ND_SIZEOF: {
        StructDef *sd = find_struct(n->sizeof_expr.type_name);
        if (sd) {
            emit("i32.const %d", sd->size);
        } else {
            /* int, char, void*, any pointer — all 4 bytes */
            emit("i32.const 4");
        }
        break;
    }
    case ND_SUBSCRIPT: {
        int esz = expr_elem_size(n->subscript.base);
        gen_expr(n->subscript.base);
        gen_expr(n->subscript.index);
        if (esz > 1) { emit("i32.const %d", esz); emit("i32.mul"); }
        emit("i32.add");
        emit(esz == 1 ? "i32.load8_u" : "i32.load");
        break;
    }
    default:
        error(n->line, n->col, "unsupported expression in codegen");
    }
}

/* --- Statement codegen --- */

static void gen_stmt(Node *n);

static void gen_body(Node *n) {
    if (n->kind == ND_BLOCK) {
        for (int i = 0; i < n->block.nstmts; i++)
            gen_stmt(n->block.stmts[i]);
    } else {
        gen_stmt(n);
    }
}

static void gen_stmt(Node *n) {
    switch (n->kind) {
    case ND_RETURN:
        if (n->ret.expr)
            gen_expr(n->ret.expr);
        emit("return");
        break;

    case ND_VAR_DECL:
        if (n->var_decl.init) {
            gen_expr(n->var_decl.init);
            emit("local.set $%s", n->var_decl.name);
        }
        break;

    case ND_EXPR_STMT:
        gen_expr(n->expr_stmt.expr);
        emit("drop");
        break;

    case ND_IF:
        gen_expr(n->if_s.cond);
        if (n->if_s.else_b) {
            emit("(if");
            indent_level++;
            emit("(then");
            indent_level++;
            gen_body(n->if_s.then_b);
            indent_level--;
            emit(")");
            emit("(else");
            indent_level++;
            gen_body(n->if_s.else_b);
            indent_level--;
            emit(")");
            indent_level--;
            emit(")");
        } else {
            emit("(if");
            indent_level++;
            emit("(then");
            indent_level++;
            gen_body(n->if_s.then_b);
            indent_level--;
            emit(")");
            indent_level--;
            emit(")");
        }
        break;

    case ND_WHILE: {
        int lbl = label_cnt++;
        brk_lbl[loop_sp] = lbl;
        cont_lbl[loop_sp] = lbl;
        loop_sp++;
        emit("(block $brk_%d", lbl);
        indent_level++;
        emit("(loop $lp_%d", lbl);
        indent_level++;
        gen_expr(n->while_s.cond);
        emit("i32.eqz");
        emit("br_if $brk_%d", lbl);
        emit("(block $cont_%d", lbl);
        indent_level++;
        gen_body(n->while_s.body);
        indent_level--;
        emit(")");
        emit("br $lp_%d", lbl);
        indent_level--;
        emit(")");
        indent_level--;
        emit(")");
        loop_sp--;
        break;
    }

    case ND_FOR: {
        if (n->for_s.init)
            gen_stmt(n->for_s.init);
        int lbl = label_cnt++;
        brk_lbl[loop_sp] = lbl;
        cont_lbl[loop_sp] = lbl;
        loop_sp++;
        emit("(block $brk_%d", lbl);
        indent_level++;
        emit("(loop $lp_%d", lbl);
        indent_level++;
        if (n->for_s.cond) {
            gen_expr(n->for_s.cond);
            emit("i32.eqz");
            emit("br_if $brk_%d", lbl);
        }
        emit("(block $cont_%d", lbl);
        indent_level++;
        gen_body(n->for_s.body);
        indent_level--;
        emit(")");
        if (n->for_s.step) {
            gen_expr(n->for_s.step);
            emit("drop");
        }
        emit("br $lp_%d", lbl);
        indent_level--;
        emit(")");
        indent_level--;
        emit(")");
        loop_sp--;
        break;
    }

    case ND_BREAK:
        if (loop_sp <= 0) error(n->line, n->col, "break outside loop");
        emit("br $brk_%d", brk_lbl[loop_sp - 1]);
        break;

    case ND_CONTINUE:
        if (loop_sp <= 0) error(n->line, n->col, "continue outside loop");
        emit("br $cont_%d", cont_lbl[loop_sp - 1]);
        break;

    case ND_BLOCK:
        for (int i = 0; i < n->block.nstmts; i++)
            gen_stmt(n->block.stmts[i]);
        break;

    default:
        error(n->line, n->col, "unsupported statement in codegen");
    }
}

/* --- Function codegen --- */

static void gen_func(Node *n) {
    if (!n->func.body) return; /* skip prototypes */

    nlocals = 0;
    label_cnt = 0;
    loop_sp = 0;
    collect_locals(n->func.body);

    /* Build signature with params */
    const char *ret_sig = n->func.ret_type == 1 ? "" : " (result i32)";
    printf("  (func $%s", n->func.name);
    for (int i = 0; i < n->func.param_count; i++)
        printf(" (param $%s i32)", n->func.param_names[i]);
    printf("%s\n", ret_sig);

    indent_level = 2;
    for (int i = 0; i < nlocals; i++)
        emit("(local $%s i32)", local_names[i]);
    emit("(local $__atmp i32)"); /* temp for pointer assignment */
    Node *body = n->func.body;
    for (int i = 0; i < body->block.nstmts; i++)
        gen_stmt(body->block.stmts[i]);
    if (n->func.ret_type != 1)
        emit("i32.const 0");
    indent_level = 1;
    emit(")");
}

/* --- WAT escape helper for data section --- */
static void emit_wat_string(const char *data, int len) {
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)data[i];
        if (c >= 32 && c < 127 && c != '"' && c != '\\')
            putchar(c);
        else
            printf("\\%02x", c);
    }
}

/* --- Module codegen --- */

static void gen_module(Node *prog) {
    emit("(module");
    indent_level++;

    /* WASI imports */
    emit("(import \"wasi_snapshot_preview1\" \"proc_exit\" (func $__proc_exit (param i32)))");
    emit("(import \"wasi_snapshot_preview1\" \"fd_write\" (func $__fd_write (param i32 i32 i32 i32) (result i32)))");
    emit("(import \"wasi_snapshot_preview1\" \"fd_read\" (func $__fd_read (param i32 i32 i32 i32) (result i32)))");
    emit("");

    /* memory */
    emit("(memory (export \"memory\") 1)");
    emit("");

    /* static data section (string literals) */
    for (int i = 0; i < nstrings; i++) {
        printf("  (data (i32.const %d) \"", str_table[i].offset);
        emit_wat_string(str_table[i].data, str_table[i].len);
        printf("\\00\")\n"); /* null terminator */
    }
    if (nstrings > 0) emit("");

    /* heap pointer — starts after static data */
    emit("(global $__heap_ptr (mut i32) (i32.const %d))", data_ptr);

    /* user global variables */
    for (int gi = 0; gi < nglobals; gi++)
        emit("(global $%s (mut i32) (i32.const %d))",
             globals_tbl[gi].name, globals_tbl[gi].init_val);
    emit("");

    /* --- Runtime helper functions --- */

    /* putchar: write one byte to stdout, return the char */
    emit("(func $putchar (param $ch i32) (result i32)");
    emit("  (i32.store8 (i32.const 0) (local.get $ch))");
    emit("  (i32.store (i32.const 4) (i32.const 0))");
    emit("  (i32.store (i32.const 8) (i32.const 1))");
    emit("  (drop (call $__fd_write (i32.const 1) (i32.const 4) (i32.const 1) (i32.const 12)))");
    emit("  (local.get $ch)");
    emit(")");

    /* getchar: read one byte from stdin, return -1 on EOF */
    emit("(func $getchar (result i32)");
    emit("  (i32.store (i32.const 4) (i32.const 0))");
    emit("  (i32.store (i32.const 8) (i32.const 1))");
    emit("  (drop (call $__fd_read (i32.const 0) (i32.const 4) (i32.const 1) (i32.const 12)))");
    emit("  (if (result i32) (i32.eqz (i32.load (i32.const 12)))");
    emit("    (then (i32.const -1))");
    emit("    (else (i32.load8_u (i32.const 0)))");
    emit("  )");
    emit(")");

    /* __print_int: print signed integer to stdout */
    emit("(func $__print_int (param $n i32)");
    emit("  (local $buf i32)");
    emit("  (local $len i32)");
    emit("  (if (i32.lt_s (local.get $n) (i32.const 0))");
    emit("    (then");
    emit("      (drop (call $putchar (i32.const 45)))");
    emit("      (local.set $n (i32.sub (i32.const 0) (local.get $n)))");
    emit("    )");
    emit("  )");
    emit("  (if (i32.eqz (local.get $n))");
    emit("    (then (drop (call $putchar (i32.const 48))) (return))");
    emit("  )");
    emit("  (local.set $buf (i32.const 48))");
    emit("  (local.set $len (i32.const 0))");
    emit("  (block $done (loop $digit");
    emit("    (br_if $done (i32.eqz (local.get $n)))");
    emit("    (local.set $buf (i32.sub (local.get $buf) (i32.const 1)))");
    emit("    (i32.store8 (local.get $buf) (i32.add (i32.const 48) (i32.rem_u (local.get $n) (i32.const 10))))");
    emit("    (local.set $n (i32.div_u (local.get $n) (i32.const 10)))");
    emit("    (local.set $len (i32.add (local.get $len) (i32.const 1)))");
    emit("    (br $digit)");
    emit("  ))");
    emit("  (block $pd (loop $pl");
    emit("    (br_if $pd (i32.eqz (local.get $len)))");
    emit("    (drop (call $putchar (i32.load8_u (local.get $buf))))");
    emit("    (local.set $buf (i32.add (local.get $buf) (i32.const 1)))");
    emit("    (local.set $len (i32.sub (local.get $len) (i32.const 1)))");
    emit("    (br $pl)");
    emit("  ))");
    emit(")");

    /* __print_str: print null-terminated string */
    emit("(func $__print_str (param $ptr i32)");
    emit("  (local $ch i32)");
    emit("  (block $done (loop $next");
    emit("    (local.set $ch (i32.load8_u (local.get $ptr)))");
    emit("    (br_if $done (i32.eqz (local.get $ch)))");
    emit("    (drop (call $putchar (local.get $ch)))");
    emit("    (local.set $ptr (i32.add (local.get $ptr) (i32.const 1)))");
    emit("    (br $next)");
    emit("  ))");
    emit(")");

    /* __print_hex: print unsigned integer in hex */
    emit("(func $__print_hex (param $n i32)");
    emit("  (local $buf i32)");
    emit("  (local $len i32)");
    emit("  (local $d i32)");
    emit("  (if (i32.eqz (local.get $n))");
    emit("    (then (drop (call $putchar (i32.const 48))) (return))");
    emit("  )");
    emit("  (local.set $buf (i32.const 48))");
    emit("  (local.set $len (i32.const 0))");
    emit("  (block $done (loop $digit");
    emit("    (br_if $done (i32.eqz (local.get $n)))");
    emit("    (local.set $buf (i32.sub (local.get $buf) (i32.const 1)))");
    emit("    (local.set $d (i32.and (local.get $n) (i32.const 15)))");
    emit("    (if (i32.lt_u (local.get $d) (i32.const 10))");
    emit("      (then (i32.store8 (local.get $buf) (i32.add (i32.const 48) (local.get $d))))");
    emit("      (else (i32.store8 (local.get $buf) (i32.add (i32.const 87) (local.get $d))))");
    emit("    )");
    emit("    (local.set $n (i32.shr_u (local.get $n) (i32.const 4)))");
    emit("    (local.set $len (i32.add (local.get $len) (i32.const 1)))");
    emit("    (br $digit)");
    emit("  ))");
    emit("  (block $pd (loop $pl");
    emit("    (br_if $pd (i32.eqz (local.get $len)))");
    emit("    (drop (call $putchar (i32.load8_u (local.get $buf))))");
    emit("    (local.set $buf (i32.add (local.get $buf) (i32.const 1)))");
    emit("    (local.set $len (i32.sub (local.get $len) (i32.const 1)))");
    emit("    (br $pl)");
    emit("  ))");
    emit(")");
    emit("");

    /* malloc: bump allocator */
    emit("(func $malloc (param $size i32) (result i32)");
    emit("  (local $ptr i32)");
    emit("  (local.set $ptr (global.get $__heap_ptr))");
    emit("  (global.set $__heap_ptr (i32.add (global.get $__heap_ptr)");
    emit("    (i32.and (i32.add (local.get $size) (i32.const 7)) (i32.const -8))))");
    emit("  (local.get $ptr)");
    emit(")");

    /* free: no-op */
    emit("(func $free (param $ptr i32))");

    /* strlen: count bytes until NUL */
    emit("(func $strlen (param $s i32) (result i32)");
    emit("  (local $n i32)");
    emit("  (local.set $n (i32.const 0))");
    emit("  (block $done (loop $next");
    emit("    (br_if $done (i32.eqz (i32.load8_u (i32.add (local.get $s) (local.get $n)))))");
    emit("    (local.set $n (i32.add (local.get $n) (i32.const 1)))");
    emit("    (br $next)");
    emit("  ))");
    emit("  (local.get $n)");
    emit(")");

    /* strcmp: lexicographic compare, returns <0, 0, or >0 */
    emit("(func $strcmp (param $a i32) (param $b i32) (result i32)");
    emit("  (local $ca i32) (local $cb i32)");
    emit("  (block $done (loop $next");
    emit("    (local.set $ca (i32.load8_u (local.get $a)))");
    emit("    (local.set $cb (i32.load8_u (local.get $b)))");
    emit("    (br_if $done (i32.ne (local.get $ca) (local.get $cb)))");
    emit("    (br_if $done (i32.eqz (local.get $ca)))");
    emit("    (local.set $a (i32.add (local.get $a) (i32.const 1)))");
    emit("    (local.set $b (i32.add (local.get $b) (i32.const 1)))");
    emit("    (br $next)");
    emit("  ))");
    emit("  (i32.sub (local.get $ca) (local.get $cb))");
    emit(")");

    /* strncpy: copy up to n bytes from src to dst */
    emit("(func $strncpy (param $dst i32) (param $src i32) (param $n i32) (result i32)");
    emit("  (local $i i32)");
    emit("  (local.set $i (i32.const 0))");
    emit("  (block $done (loop $next");
    emit("    (br_if $done (i32.ge_u (local.get $i) (local.get $n)))");
    emit("    (i32.store8 (i32.add (local.get $dst) (local.get $i))");
    emit("      (i32.load8_u (i32.add (local.get $src) (local.get $i))))");
    emit("    (br_if $done (i32.eqz (i32.load8_u (i32.add (local.get $src) (local.get $i)))))");
    emit("    (local.set $i (i32.add (local.get $i) (i32.const 1)))");
    emit("    (br $next)");
    emit("  ))");
    emit("  (local.get $dst)");
    emit(")");

    /* memcpy: copy n bytes from src to dst */
    emit("(func $memcpy (param $dst i32) (param $src i32) (param $n i32) (result i32)");
    emit("  (local $i i32)");
    emit("  (local.set $i (i32.const 0))");
    emit("  (block $done (loop $next");
    emit("    (br_if $done (i32.ge_u (local.get $i) (local.get $n)))");
    emit("    (i32.store8 (i32.add (local.get $dst) (local.get $i))");
    emit("      (i32.load8_u (i32.add (local.get $src) (local.get $i))))");
    emit("    (local.set $i (i32.add (local.get $i) (i32.const 1)))");
    emit("    (br $next)");
    emit("  ))");
    emit("  (local.get $dst)");
    emit(")");

    /* memset: fill n bytes at dst with val */
    emit("(func $memset (param $dst i32) (param $val i32) (param $n i32) (result i32)");
    emit("  (local $i i32)");
    emit("  (local.set $i (i32.const 0))");
    emit("  (block $done (loop $next");
    emit("    (br_if $done (i32.ge_u (local.get $i) (local.get $n)))");
    emit("    (i32.store8 (i32.add (local.get $dst) (local.get $i)) (local.get $val))");
    emit("    (local.set $i (i32.add (local.get $i) (i32.const 1)))");
    emit("    (br $next)");
    emit("  ))");
    emit("  (local.get $dst)");
    emit(")");

    /* memcmp: compare n bytes */
    emit("(func $memcmp (param $a i32) (param $b i32) (param $n i32) (result i32)");
    emit("  (local $i i32) (local $ca i32) (local $cb i32)");
    emit("  (local.set $i (i32.const 0))");
    emit("  (block $done (loop $next");
    emit("    (br_if $done (i32.ge_u (local.get $i) (local.get $n)))");
    emit("    (local.set $ca (i32.load8_u (i32.add (local.get $a) (local.get $i))))");
    emit("    (local.set $cb (i32.load8_u (i32.add (local.get $b) (local.get $i))))");
    emit("    (if (i32.ne (local.get $ca) (local.get $cb))");
    emit("      (then (return (i32.sub (local.get $ca) (local.get $cb))))");
    emit("    )");
    emit("    (local.set $i (i32.add (local.get $i) (i32.const 1)))");
    emit("    (br $next)");
    emit("  ))");
    emit("  (i32.const 0)");
    emit(")");
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
