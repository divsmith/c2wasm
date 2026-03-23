/*
 * c2wasm.c — C-to-WAT compiler (self-hosting version)
 *
 * Reads C source from stdin, emits WebAssembly Text Format to stdout.
 * Written using ONLY the C subset that this compiler supports.
 *
 * Build:  gcc -o c2wasm compiler/src/c2wasm.c
 * Usage:  ./c2wasm < program.c > program.wat
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Constants (replacing enums with #define)
 * ================================================================ */

/* Token kinds */
#define TOK_EOF 0
#define TOK_INT 1
#define TOK_CHAR_KW 2
#define TOK_VOID 3
#define TOK_RETURN 4
#define TOK_IF 5
#define TOK_ELSE 6
#define TOK_WHILE 7
#define TOK_FOR 8
#define TOK_BREAK 9
#define TOK_CONTINUE 10
#define TOK_STRUCT 11
#define TOK_SIZEOF 12
#define TOK_DEFINE 13
#define TOK_IDENT 14
#define TOK_INT_LIT 15
#define TOK_CHAR_LIT 16
#define TOK_STR_LIT 17
#define TOK_LPAREN 18
#define TOK_RPAREN 19
#define TOK_LBRACE 20
#define TOK_RBRACE 21
#define TOK_LBRACKET 22
#define TOK_RBRACKET 23
#define TOK_SEMI 24
#define TOK_COMMA 25
#define TOK_DOT 26
#define TOK_ARROW 27
#define TOK_PLUS 28
#define TOK_MINUS 29
#define TOK_STAR 30
#define TOK_SLASH 31
#define TOK_PERCENT 32
#define TOK_AMP 33
#define TOK_BANG 34
#define TOK_PIPE 35
#define TOK_TILDE 36
#define TOK_PIPE_PIPE 37
#define TOK_AMP_AMP 38
#define TOK_EQ 39
#define TOK_PLUS_EQ 40
#define TOK_MINUS_EQ 41
#define TOK_EQ_EQ 42
#define TOK_BANG_EQ 43
#define TOK_LT 44
#define TOK_GT 45
#define TOK_LT_EQ 46
#define TOK_GT_EQ 47
#define TOK_LSHIFT 48
#define TOK_RSHIFT 49
#define TOK_PLUS_PLUS 50
#define TOK_MINUS_MINUS 51
#define TOK_CARET 52
#define TOK_PIPE_EQ 53
#define TOK_AMP_EQ 54
#define TOK_CARET_EQ 55
#define TOK_LSHIFT_EQ 56
#define TOK_RSHIFT_EQ 57
#define TOK_DO 58
#define TOK_QUESTION 59
#define TOK_COLON 60 /* used by ternary ?: and by switch case/default labels */
#define TOK_SWITCH 61
#define TOK_CASE 62
#define TOK_DEFAULT 63
#define TOK_CONST 64
#define TOK_ENUM 65

/* Node kinds */
#define ND_PROGRAM 0
#define ND_FUNC 1
#define ND_BLOCK 2
#define ND_RETURN 3
#define ND_INT_LIT 4
#define ND_BINARY 5
#define ND_UNARY 6
#define ND_VAR_DECL 7
#define ND_IDENT 8
#define ND_ASSIGN 9
#define ND_IF 10
#define ND_WHILE 11
#define ND_FOR 12
#define ND_BREAK 13
#define ND_CONTINUE 14
#define ND_EXPR_STMT 15
#define ND_CALL 16
#define ND_STR_LIT 17
#define ND_MEMBER 18
#define ND_SIZEOF 19
#define ND_SUBSCRIPT 20
#define ND_POST_INC 21
#define ND_POST_DEC 22
#define ND_DO_WHILE 23
#define ND_TERNARY 24
#define ND_SWITCH 25
#define ND_CASE 26
#define ND_DEFAULT 27

/* Limits */
#define MAX_SRC 2097152
#define MAX_MACROS 256
#define MAX_STRINGS 512
#define MAX_STR_DATA 512
#define MAX_STRUCTS 64
#define MAX_FIELDS 32
#define MAX_GLOBALS 256
#define MAX_FUNC_SIGS 256
#define MAX_LOCALS 256
#define MAX_LOOP_DEPTH 64
#define MAX_CASES 256
#define MAX_ENUM_CONSTS 512

/* ================================================================
 * Error reporting
 * ================================================================ */

void error(int line, int col, char *msg) {
    printf("%d:%d: error: %s\n", line, col, msg);
    exit(1);
}

char *strdupn(char *s, int maxlen) {
    char *dst;
    dst = (char *)malloc(maxlen + 1);
    strncpy(dst, s, maxlen);
    dst[maxlen] = '\0';
    return dst;
}

/* ================================================================
 * Source buffer — read entire stdin via getchar loop
 * ================================================================ */

char *src;
int src_len;

void read_source(void) {
    int c;
    int cap;
    src = (char *)malloc(MAX_SRC);
    src_len = 0;
    cap = MAX_SRC;
    c = getchar();
    while (c != -1) {
        if (src_len >= cap - 1) {
            break;
        }
        src[src_len] = (char)c;
        src_len = src_len + 1;
        c = getchar();
    }
    src[src_len] = '\0';
}

/* ================================================================
 * Token struct
 * ================================================================ */

struct Token {
    int kind;
    char *text;
    int int_val;
    int line;
    int col;
};

/* ================================================================
 * Lexer state
 * ================================================================ */

int lex_pos;
int lex_line;
int lex_col;

void lex_init(void) {
    lex_pos = 0;
    lex_line = 1;
    lex_col = 1;
}

char lp(void) {
    if (lex_pos < src_len) {
        return src[lex_pos];
    }
    return '\0';
}

char lp2(void) {
    if (lex_pos + 1 < src_len) {
        return src[lex_pos + 1];
    }
    return '\0';
}

char la(void) {
    char c;
    c = src[lex_pos];
    lex_pos = lex_pos + 1;
    if (c == '\n') {
        lex_line = lex_line + 1;
        lex_col = 1;
    } else {
        lex_col = lex_col + 1;
    }
    return c;
}

void skip_ws(void) {
    char c;
    for (;;) {
        c = lp();
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            la();
        } else if (c == '/' && lp2() == '/') {
            while (lex_pos < src_len && lp() != '\n') {
                la();
            }
        } else if (c == '/' && lp2() == '*') {
            la();
            la();
            while (lex_pos < src_len) {
                if (lp() == '*' && lp2() == '/') {
                    la();
                    la();
                    break;
                }
                la();
            }
        } else {
            break;
        }
    }
}

/* Manual is_alpha check (replaces isalpha) */
int is_alpha(char c) {
    if (c >= 'a' && c <= 'z') return 1;
    if (c >= 'A' && c <= 'Z') return 1;
    return 0;
}

/* Manual is_digit check (replaces isdigit) */
int is_digit(char c) {
    if (c >= '0' && c <= '9') return 1;
    return 0;
}

/* Manual is_alnum check (replaces isalnum) */
int is_alnum(char c) {
    if (is_alpha(c)) return 1;
    if (is_digit(c)) return 1;
    return 0;
}

/* Manual is_xdigit check (replaces isxdigit) */
int is_xdigit(char c) {
    if (is_digit(c)) return 1;
    if (c >= 'a' && c <= 'f') return 1;
    if (c >= 'A' && c <= 'F') return 1;
    return 0;
}

int kw_lookup(char *s) {
    if (strcmp(s, "int") == 0) return TOK_INT;
    if (strcmp(s, "char") == 0) return TOK_CHAR_KW;
    if (strcmp(s, "void") == 0) return TOK_VOID;
    if (strcmp(s, "return") == 0) return TOK_RETURN;
    if (strcmp(s, "if") == 0) return TOK_IF;
    if (strcmp(s, "else") == 0) return TOK_ELSE;
    if (strcmp(s, "while") == 0) return TOK_WHILE;
    if (strcmp(s, "do") == 0) return TOK_DO;
    if (strcmp(s, "for") == 0) return TOK_FOR;
    if (strcmp(s, "break") == 0) return TOK_BREAK;
    if (strcmp(s, "continue") == 0) return TOK_CONTINUE;
    if (strcmp(s, "switch") == 0) return TOK_SWITCH;
    if (strcmp(s, "case") == 0) return TOK_CASE;
    if (strcmp(s, "default") == 0) return TOK_DEFAULT;
    if (strcmp(s, "const") == 0) return TOK_CONST;
    if (strcmp(s, "enum") == 0) return TOK_ENUM;
    if (strcmp(s, "struct") == 0) return TOK_STRUCT;
    if (strcmp(s, "sizeof") == 0) return TOK_SIZEOF;
    return 0;
}

/* ================================================================
 * Macro table for #define
 * ================================================================ */

struct Macro {
    char *name;
    int value;
};

struct Macro **macros;
int nmacros;

void init_macros(void) {
    macros = (struct Macro **)malloc(MAX_MACROS * sizeof(void *));
    nmacros = 0;
}

int find_macro(char *name) {
    int i;
    for (i = 0; i < nmacros; i = i + 1) {
        if (strcmp(macros[i]->name, name) == 0) return i;
    }
    return -1;
}

/* ================================================================
 * String Literal Table
 * ================================================================ */

struct StringEntry {
    char *data;
    int len;
    int offset;
};

struct StringEntry **str_table;
int nstrings;
int data_ptr;

void init_strings(void) {
    str_table = (struct StringEntry **)malloc(MAX_STRINGS * sizeof(void *));
    nstrings = 0;
    data_ptr = 1024;
}

int add_string(char *data, int len) {
    int id;
    char *__s;
    id = nstrings;
    nstrings = nstrings + 1;
    if (len >= MAX_STR_DATA) {
        len = MAX_STR_DATA - 1;
    }
    str_table[id] = (struct StringEntry *)malloc(sizeof(struct StringEntry));
    __s = (char *)malloc(len + 1);
    str_table[id]->data = __s;
    memcpy(__s, data, len);
    __s[len] = '\0';
    str_table[id]->len = len;
    str_table[id]->offset = data_ptr;
    data_ptr = data_ptr + len + 1;
    return id;
}

/* ================================================================
 * Next Token
 * ================================================================ */

struct Token *next_token(void) {
    struct Token *t;
    char c;
    int s;
    int len;
    int kw;
    int mi;
    int val;
    int neg;
    char d;
    char ch;
    int i;
    char *name;
    int ni;
    int dlen;
    char *__s;

    skip_ws();
    t = (struct Token *)malloc(sizeof(struct Token));
    memset(t, 0, sizeof(struct Token));
    t->line = lex_line;
    t->col = lex_col;

    if (lex_pos >= src_len) {
        t->kind = TOK_EOF;
        return t;
    }
    c = lp();

    /* preprocessor directives */
    if (c == '#') {
        la();
        while (lp() == ' ' || lp() == '\t') la();
        s = lex_pos;
        while (is_alpha(lp())) la();
        dlen = lex_pos - s;
        if (dlen == 6 && memcmp(src + s, "define", 6) == 0) {
            while (lp() == ' ' || lp() == '\t') la();
            name = (char *)malloc(128);
            ni = 0;
            while (is_alnum(lp()) || lp() == '_') {
                if (ni < 127) {
                    name[ni] = la();
                    ni = ni + 1;
                } else {
                    la();
                }
            }
            name[ni] = '\0';
            while (lp() == ' ' || lp() == '\t') la();
            neg = 0;
            if (lp() == '-') {
                neg = 1;
                la();
                while (lp() == ' ') la();
            }
            val = 0;
            if (lp() == '0' && (lex_pos + 1 < src_len) && (src[lex_pos + 1] == 'x' || src[lex_pos + 1] == 'X')) {
                la();
                la();
                while (is_xdigit(lp())) {
                    d = la();
                    if (d <= '9') {
                        val = val * 16 + (d - '0');
                    } else {
                        val = val * 16 + ((d | 32) - 'a' + 10);
                    }
                }
            } else {
                while (is_digit(lp())) {
                    val = val * 10 + (la() - '0');
                }
            }
            if (neg) val = -val;
            macros[nmacros] = (struct Macro *)malloc(sizeof(struct Macro));
            macros[nmacros]->name = name;
            macros[nmacros]->value = val;
            nmacros = nmacros + 1;
        }
        /* skip to end of line */
        while (lex_pos < src_len && lp() != '\n') la();
        return next_token();
    }

    /* identifiers / keywords */
    if (is_alpha(c) || c == '_') {
        s = lex_pos;
        while (is_alnum(lp()) || lp() == '_') la();
        len = lex_pos - s;
        if (len >= 511) len = 511;
        __s = (char *)malloc(len + 1);
        t->text = __s;
        memcpy(__s, src + s, len);
        __s[len] = '\0';
        kw = kw_lookup(t->text);
        if (kw != 0) {
            t->kind = kw;
        } else {
            t->kind = TOK_IDENT;
        }
        /* macro substitution */
        if (t->kind == TOK_IDENT) {
            mi = find_macro(t->text);
            if (mi >= 0) {
                t->kind = TOK_INT_LIT;
                t->int_val = macros[mi]->value;
            }
        }
        return t;
    }

    /* integer literals */
    if (is_digit(c)) {
        val = 0;
        if (c == '0' && (lp2() == 'x' || lp2() == 'X')) {
            la();
            la();
            while (is_xdigit(lp())) {
                d = la();
                if (d <= '9') {
                    val = val * 16 + (d - '0');
                } else {
                    val = val * 16 + ((d | 32) - 'a' + 10);
                }
            }
        } else {
            while (is_digit(lp())) {
                val = val * 10 + (la() - '0');
            }
        }
        t->kind = TOK_INT_LIT;
        t->int_val = val;
        return t;
    }

    /* character literal */
    if (c == '\'') {
        la();
        if (lp() == '\\') {
            la();
            ch = lp();
            if (ch == 'n') {
                ch = '\n';
            } else if (ch == 'r') {
                ch = '\r';
            } else if (ch == 't') {
                ch = '\t';
            } else if (ch == '0') {
                ch = '\0';
            } else if (ch == '\\') {
                ch = '\\';
            } else if (ch == '\'') {
                ch = '\'';
            }
            la();
        } else {
            ch = la();
        }
        if (lp() == '\'') {
            la();
        } else {
            error(t->line, t->col, "unterminated char literal");
        }
        t->kind = TOK_CHAR_LIT;
        t->int_val = ch & 255;
        return t;
    }

    /* string literal */
    if (c == '"') {
        la();
        __s = (char *)malloc(512);
        t->text = __s;
        i = 0;
        while (lex_pos < src_len && lp() != '"') {
            if (lp() == '\\') {
                la();
                ch = lp();
                if (ch == 'n') {
                    __s[i] = '\n';
                    i = i + 1;
                } else if (ch == 'r') {
                    __s[i] = '\r';
                    i = i + 1;
                } else if (ch == 't') {
                    __s[i] = '\t';
                    i = i + 1;
                } else if (ch == '0') {
                    __s[i] = '\0';
                    i = i + 1;
                } else if (ch == '\\') {
                    __s[i] = '\\';
                    i = i + 1;
                } else if (ch == '"') {
                    __s[i] = '"';
                    i = i + 1;
                } else {
                    __s[i] = lp();
                    i = i + 1;
                }
                la();
            } else {
                __s[i] = la();
                i = i + 1;
            }
            if (i >= 511) break;
        }
        if (lp() == '"') la();
        __s[i] = '\0';
        t->int_val = i;
        t->kind = TOK_STR_LIT;
        return t;
    }

    /* punctuation & operators */
    la();
    if (c == '(') {
        t->kind = TOK_LPAREN;
    } else if (c == ')') {
        t->kind = TOK_RPAREN;
    } else if (c == '{') {
        t->kind = TOK_LBRACE;
    } else if (c == '}') {
        t->kind = TOK_RBRACE;
    } else if (c == '[') {
        t->kind = TOK_LBRACKET;
    } else if (c == ']') {
        t->kind = TOK_RBRACKET;
    } else if (c == ';') {
        t->kind = TOK_SEMI;
    } else if (c == ',') {
        t->kind = TOK_COMMA;
    } else if (c == '.') {
        t->kind = TOK_DOT;
    } else if (c == '+') {
        if (lp() == '+') {
            la();
            t->kind = TOK_PLUS_PLUS;
        } else if (lp() == '=') {
            la();
            t->kind = TOK_PLUS_EQ;
        } else {
            t->kind = TOK_PLUS;
        }
    } else if (c == '-') {
        if (lp() == '-') {
            la();
            t->kind = TOK_MINUS_MINUS;
        } else if (lp() == '=') {
            la();
            t->kind = TOK_MINUS_EQ;
        } else if (lp() == '>') {
            la();
            t->kind = TOK_ARROW;
        } else {
            t->kind = TOK_MINUS;
        }
    } else if (c == '*') {
        t->kind = TOK_STAR;
    } else if (c == '/') {
        t->kind = TOK_SLASH;
    } else if (c == '%') {
        t->kind = TOK_PERCENT;
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
    } else if (c == '^') {
        if (lp() == '=') {
            la();
            t->kind = TOK_CARET_EQ;
        } else {
            t->kind = TOK_CARET;
        }
    } else if (c == '~') {
        t->kind = TOK_TILDE;
    } else if (c == '!') {
        if (lp() == '=') {
            la();
            t->kind = TOK_BANG_EQ;
        } else {
            t->kind = TOK_BANG;
        }
    } else if (c == '=') {
        if (lp() == '=') {
            la();
            t->kind = TOK_EQ_EQ;
        } else {
            t->kind = TOK_EQ;
        }
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
    } else if (c == '?') {
        t->kind = TOK_QUESTION;
    } else if (c == ':') {
        t->kind = TOK_COLON;
    } else {
        error(t->line, t->col, "unexpected character");
    }
    return t;
}

/* ================================================================
 * Struct Type Registry
 * ================================================================ */

struct StructField {
    char *name;
    int fld_offset;
};

struct StructDef {
    char *name;
    struct StructField **fields;
    int nfields;
    int size;
};

struct StructDef **structs_reg;
int nstructs;

void init_structs(void) {
    structs_reg = (struct StructDef **)malloc(MAX_STRUCTS * sizeof(void *));
    nstructs = 0;
}

struct StructDef *find_struct(char *name) {
    int i;
    for (i = 0; i < nstructs; i = i + 1) {
        if (strcmp(structs_reg[i]->name, name) == 0) return structs_reg[i];
    }
    return (struct StructDef *)0;
}

int resolve_field_offset(char *field_name) {
    int i;
    int j;
    for (i = 0; i < nstructs; i = i + 1) {
        for (j = 0; j < structs_reg[i]->nfields; j = j + 1) {
            if (strcmp(structs_reg[i]->fields[j]->name, field_name) == 0) {
                return structs_reg[i]->fields[j]->fld_offset;
            }
        }
    }
    return -1;
}

/* ================================================================
 * Global Variable Table
 * ================================================================ */

struct GlobalVar {
    char *name;
    int init_val;
    int gv_is_char;
};

struct GlobalVar **globals_tbl;
int nglobals;

void init_globals(void) {
    globals_tbl = (struct GlobalVar **)malloc(MAX_GLOBALS * sizeof(void *));
    nglobals = 0;
}

int find_global(char *name) {
    int i;
    for (i = 0; i < nglobals; i = i + 1) {
        if (strcmp(globals_tbl[i]->name, name) == 0) return i;
    }
    return -1;
}

/* ================================================================
 * Function Signature Table
 * ================================================================ */

struct FuncSig {
    char *name;
    int is_void;
};

struct FuncSig **func_sigs;
int nfunc_sigs;

void init_func_sigs(void) {
    func_sigs = (struct FuncSig **)malloc(MAX_FUNC_SIGS * sizeof(void *));
    nfunc_sigs = 0;
}

int func_is_void(char *name) {
    int i;
    for (i = 0; i < nfunc_sigs; i = i + 1) {
        if (strcmp(func_sigs[i]->name, name) == 0) return func_sigs[i]->is_void;
    }
    return 0;
}

/* ================================================================
 * Enum Constant Table
 * ================================================================ */

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

/* ================================================================
 * AST Node (flat struct instead of union)
 * ================================================================ */

struct Node {
    int kind;
    int nline;
    int ncol;
    int ival;
    int ival2;
    char *sval;
    struct Node *c0;
    struct Node *c1;
    struct Node *c2;
    struct Node *c3;
    struct Node **list;
};

struct Node *node_new(int kind, int line, int col) {
    struct Node *n;
    n = (struct Node *)malloc(sizeof(struct Node));
    memset(n, 0, sizeof(struct Node));
    n->kind = kind;
    n->nline = line;
    n->ncol = col;
    return n;
}

/* Growable list helper */
struct NList {
    struct Node **items;
    int count;
    int cap;
};

void nlist_push(struct NList *l, struct Node *n) {
    struct Node **new_items;
    int new_cap;
    int i;
    if (l->count >= l->cap) {
        if (l->cap == 0) {
            new_cap = 8;
        } else {
            new_cap = l->cap * 2;
        }
        new_items = (struct Node **)malloc(new_cap * sizeof(struct Node *));
        for (i = 0; i < l->count; i = i + 1) {
            new_items[i] = l->items[i];
        }
        if (l->items != (struct Node **)0) {
            free(l->items);
        }
        l->items = new_items;
        l->cap = new_cap;
    }
    l->items[l->count] = n;
    l->count = l->count + 1;
}

/* ================================================================
 * Parser
 * ================================================================ */

struct Token *cur;

void advance_tok(void) {
    cur = next_token();
}

int at(int kind) {
    return cur->kind == kind;
}

void expect(int kind, char *msg) {
    if (cur->kind != kind) error(cur->line, cur->col, msg);
    advance_tok();
}

int is_type_token(void) {
    if (at(TOK_CONST)) return 1;
    if (at(TOK_INT)) return 1;
    if (at(TOK_CHAR_KW)) return 1;
    if (at(TOK_VOID)) return 1;
    if (at(TOK_STRUCT)) return 1;
    if (at(TOK_ENUM)) return 1;
    return 0;
}

/* Forward declarations */
struct Node *parse_expr(void);
struct Node *parse_expr_bp(int min_bp);
struct Node *parse_stmt(void);
struct Node *parse_block(void);

/* Precedence climbing helpers */
int prefix_bp(int op) {
    if (op == TOK_MINUS || op == TOK_BANG) return 25;
    if (op == TOK_STAR || op == TOK_AMP) return 25;
    if (op == TOK_TILDE) return 25;
    if (op == TOK_PLUS_PLUS || op == TOK_MINUS_MINUS) return 25;
    return -1;
}

int last_rbp;

int infix_bp(int op) {
    if (op == TOK_EQ || op == TOK_PLUS_EQ || op == TOK_MINUS_EQ ||
        op == TOK_PIPE_EQ || op == TOK_AMP_EQ || op == TOK_CARET_EQ ||
        op == TOK_LSHIFT_EQ || op == TOK_RSHIFT_EQ) {
        last_rbp = 1;
        return 2;
    }
    if (op == TOK_PIPE_PIPE) {
        last_rbp = 5;
        return 4;
    }
    if (op == TOK_AMP_AMP) {
        last_rbp = 7;
        return 6;
    }
    if (op == TOK_PIPE) {
        last_rbp = 9;
        return 8;
    }
    if (op == TOK_CARET) {
        last_rbp = 10;
        return 9;
    }
    if (op == TOK_AMP) {
        last_rbp = 11;
        return 10;
    }
    if (op == TOK_EQ_EQ || op == TOK_BANG_EQ) {
        last_rbp = 13;
        return 12;
    }
    if (op == TOK_LT || op == TOK_GT || op == TOK_LT_EQ || op == TOK_GT_EQ) {
        last_rbp = 15;
        return 14;
    }
    if (op == TOK_LSHIFT || op == TOK_RSHIFT) {
        last_rbp = 17;
        return 16;
    }
    if (op == TOK_PLUS || op == TOK_MINUS) {
        last_rbp = 19;
        return 18;
    }
    if (op == TOK_STAR || op == TOK_SLASH || op == TOK_PERCENT) {
        last_rbp = 21;
        return 20;
    }
    if (op == TOK_DOT || op == TOK_ARROW) {
        last_rbp = 30;
        return 29;
    }
    return -1;
}

struct Node *parse_atom(void) {
    struct Node *n;
    struct Node *c;
    struct NList *args;
    int line;
    int col;
    int is_ptr;

    if (at(TOK_INT_LIT)) {
        n = node_new(ND_INT_LIT, cur->line, cur->col);
        n->ival = cur->int_val;
        advance_tok();
        return n;
    }
    if (at(TOK_CHAR_LIT)) {
        n = node_new(ND_INT_LIT, cur->line, cur->col);
        n->ival = cur->int_val;
        advance_tok();
        return n;
    }
    if (at(TOK_SIZEOF)) {
        line = cur->line;
        col = cur->col;
        advance_tok();
        n = node_new(ND_SIZEOF, line, col);
        is_ptr = 0;
        if (at(TOK_LPAREN)) {
            advance_tok(); /* consume '(' */
            while (at(TOK_CONST)) advance_tok(); /* skip const */
            if (at(TOK_STRUCT)) {
                advance_tok();
                if (at(TOK_IDENT)) {
                    n->sval = strdupn(cur->text, 127);
                    advance_tok();
                }
                while (at(TOK_STAR)) { is_ptr = 1; advance_tok(); }
            } else if (at(TOK_INT) || at(TOK_CHAR_KW) || at(TOK_VOID)) {
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
            /* sizeof expr without parens */
            n->c0 = parse_expr_bp(25);
        }
        n->ival = is_ptr;
        return n;
    }
    if (at(TOK_STR_LIT)) {
        n = node_new(ND_STR_LIT, cur->line, cur->col);
        n->ival = add_string(cur->text, cur->int_val);
        n->ival2 = cur->int_val;
        advance_tok();
        return n;
    }
    if (at(TOK_IDENT)) {
        int eci;
        eci = find_enum_const(cur->text);
        if (eci >= 0) {
            n = node_new(ND_INT_LIT, cur->line, cur->col);
            n->ival = enum_consts[eci]->val;
            advance_tok();
            return n;
        }
        n = node_new(ND_IDENT, cur->line, cur->col);
        n->sval = strdupn(cur->text, 127);
        advance_tok();
        /* function call: ident '(' args ')' */
        if (at(TOK_LPAREN)) {
            advance_tok();
            c = node_new(ND_CALL, n->nline, n->ncol);
            c->sval = n->sval;
            args = (struct NList *)malloc(sizeof(struct NList));
            args->items = (struct Node **)0;
            args->count = 0;
            args->cap = 0;
            if (!at(TOK_RPAREN)) {
                nlist_push(args, parse_expr());
                while (at(TOK_COMMA)) {
                    advance_tok();
                    nlist_push(args, parse_expr());
                }
            }
            expect(TOK_RPAREN, "expected ')' after arguments");
            c->list = args->items;
            c->ival2 = args->count;
            return c;
        }
        return n;
    }
    if (at(TOK_LPAREN)) {
        advance_tok();
        /* type cast */
        if (is_type_token()) {
            while (at(TOK_CONST)) advance_tok();
            if (at(TOK_STRUCT)) {
                advance_tok();
                if (at(TOK_IDENT)) advance_tok();
            } else {
                advance_tok();
            }
            while (at(TOK_STAR)) advance_tok();
            expect(TOK_RPAREN, "expected ')' after cast type");
            return parse_expr_bp(25);
        }
        n = parse_expr();
        expect(TOK_RPAREN, "expected ')'");
        return n;
    }
    error(cur->line, cur->col, "expected expression");
    return (struct Node *)0;
}

struct Node *parse_expr_bp(int min_bp) {
    struct Node *left;
    struct Node *right;
    struct Node *operand;
    struct Node *one;
    struct Node *idx;
    struct Node *base;
    struct Node *mem;
    struct Node *tgt;
    struct Node *asgn;
    struct Node *bin;
    int pbp;
    int op;
    int line;
    int col;
    int rbp;
    int lbp;
    int valid;

    /* prefix operators */
    pbp = prefix_bp(cur->kind);
    if (pbp >= 0) {
        op = cur->kind;
        line = cur->line;
        col = cur->col;
        advance_tok();
        operand = parse_expr_bp(pbp);
        /* prefix ++/-- desugar to compound assignment */
        if (op == TOK_PLUS_PLUS || op == TOK_MINUS_MINUS) {
            one = node_new(ND_INT_LIT, line, col);
            one->ival = 1;
            left = node_new(ND_ASSIGN, line, col);
            left->c0 = operand;
            left->c1 = one;
            if (op == TOK_PLUS_PLUS) {
                left->ival = TOK_PLUS_EQ;
            } else {
                left->ival = TOK_MINUS_EQ;
            }
        } else {
            left = node_new(ND_UNARY, line, col);
            left->ival = op;
            left->c0 = operand;
        }
    } else {
        left = parse_atom();
    }

    /* infix operators */
    for (;;) {
        /* array subscript */
        if (at(TOK_LBRACKET) && 29 >= min_bp) {
            line = cur->line;
            col = cur->col;
            advance_tok();
            idx = parse_expr();
            expect(TOK_RBRACKET, "expected ']'");
            base = left;
            left = node_new(ND_SUBSCRIPT, line, col);
            left->c0 = base;
            left->c1 = idx;
            continue;
        }

        /* postfix ++ and -- */
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

        /* ternary ? : */
        /* ternary bp=3: above assignment (2), below logical-or (4); right-assoc */
        if (at(TOK_QUESTION) && 3 >= min_bp) {
            struct Node *then_e;
            struct Node *else_e;
            struct Node *tern;
            int tline;
            int tcol;
            tline = cur->line;
            tcol = cur->col;
            advance_tok();
            then_e = parse_expr_bp(0);
            expect(TOK_COLON, "expected ':' in ternary");
            else_e = parse_expr_bp(3);
            tern = node_new(ND_TERNARY, tline, tcol);
            tern->c0 = left;
            tern->c1 = then_e;
            tern->c2 = else_e;
            left = tern;
            continue;
        }

        lbp = infix_bp(cur->kind);
        rbp = last_rbp;
        if (lbp < 0 || lbp < min_bp) break;
        op = cur->kind;
        line = cur->line;
        col = cur->col;
        advance_tok();

        /* member access */
        if (op == TOK_DOT || op == TOK_ARROW) {
            if (!at(TOK_IDENT)) error(line, col, "expected field name");
            mem = node_new(ND_MEMBER, line, col);
            mem->c0 = left;
            mem->sval = strdupn(cur->text, 127);
            if (op == TOK_ARROW) {
                mem->ival2 = 1;
            } else {
                mem->ival2 = 0;
            }
            advance_tok();
            left = mem;
            continue;
        }

        right = parse_expr_bp(rbp);

        /* assignment operators */
        if (op == TOK_EQ || op == TOK_PLUS_EQ || op == TOK_MINUS_EQ ||
            op == TOK_PIPE_EQ || op == TOK_AMP_EQ || op == TOK_CARET_EQ ||
            op == TOK_LSHIFT_EQ || op == TOK_RSHIFT_EQ) {
            tgt = left;
            valid = 0;
            if (tgt->kind == ND_IDENT) valid = 1;
            if (tgt->kind == ND_UNARY && tgt->ival == TOK_STAR) valid = 1;
            if (tgt->kind == ND_MEMBER) valid = 1;
            if (tgt->kind == ND_SUBSCRIPT) valid = 1;
            if (!valid) error(line, col, "invalid assignment target");
            asgn = node_new(ND_ASSIGN, line, col);
            asgn->c0 = tgt;
            asgn->c1 = right;
            asgn->ival = op;
            left = asgn;
        } else {
            bin = node_new(ND_BINARY, line, col);
            bin->ival = op;
            bin->c0 = left;
            bin->c1 = right;
            left = bin;
        }
    }
    return left;
}

struct Node *parse_expr(void) {
    return parse_expr_bp(0);
}

/* --- Statements --- */

struct Node *parse_var_decl(void) {
    struct Node *n;
    int line;
    int col;
    int is_char;

    line = cur->line;
    col = cur->col;
    is_char = 0;
    while (at(TOK_CONST)) advance_tok();
    if (at(TOK_STRUCT)) {
        advance_tok();
        advance_tok();
    } else {
        if (at(TOK_CHAR_KW)) is_char = 1;
        advance_tok();
    }
    while (at(TOK_STAR)) advance_tok();
    if (!at(TOK_IDENT)) error(cur->line, cur->col, "expected variable name");
    n = node_new(ND_VAR_DECL, line, col);
    n->sval = strdupn(cur->text, 127);
    n->ival2 = is_char;
    advance_tok();
    if (at(TOK_EQ)) {
        advance_tok();
        n->c0 = parse_expr();
    }
    expect(TOK_SEMI, "expected ';' after declaration");
    return n;
}

struct Node *parse_if(void) {
    struct Node *n;
    struct Node *cond;
    struct Node *then_b;
    struct Node *else_b;
    int line;
    int col;

    line = cur->line;
    col = cur->col;
    advance_tok();
    expect(TOK_LPAREN, "expected '(' after 'if'");
    cond = parse_expr();
    expect(TOK_RPAREN, "expected ')'");
    then_b = parse_stmt();
    else_b = (struct Node *)0;
    if (at(TOK_ELSE)) {
        advance_tok();
        else_b = parse_stmt();
    }
    n = node_new(ND_IF, line, col);
    n->c0 = cond;
    n->c1 = then_b;
    n->c2 = else_b;
    return n;
}

struct Node *parse_while(void) {
    struct Node *n;
    struct Node *cond;
    struct Node *body;
    int line;
    int col;

    line = cur->line;
    col = cur->col;
    advance_tok();
    expect(TOK_LPAREN, "expected '(' after 'while'");
    cond = parse_expr();
    expect(TOK_RPAREN, "expected ')'");
    body = parse_stmt();
    n = node_new(ND_WHILE, line, col);
    n->c0 = cond;
    n->c1 = body;
    return n;
}

struct Node *parse_for(void) {
    struct Node *n;
    struct Node *init;
    struct Node *cond;
    struct Node *step;
    struct Node *body;
    int line;
    int col;

    line = cur->line;
    col = cur->col;
    advance_tok();
    expect(TOK_LPAREN, "expected '(' after 'for'");

    init = (struct Node *)0;
    if (is_type_token()) {
        init = parse_var_decl();
    } else if (!at(TOK_SEMI)) {
        init = node_new(ND_EXPR_STMT, cur->line, cur->col);
        init->c0 = parse_expr();
        expect(TOK_SEMI, "expected ';'");
    } else {
        advance_tok();
    }

    cond = (struct Node *)0;
    if (!at(TOK_SEMI)) cond = parse_expr();
    expect(TOK_SEMI, "expected ';'");

    step = (struct Node *)0;
    if (!at(TOK_RPAREN)) step = parse_expr();
    expect(TOK_RPAREN, "expected ')'");

    body = parse_stmt();
    n = node_new(ND_FOR, line, col);
    n->c0 = init;
    n->c1 = cond;
    n->c2 = step;
    n->c3 = body;
    return n;
}

struct Node *parse_do_while(void) {
    struct Node *n;
    struct Node *body;
    struct Node *cond;
    int line;
    int col;

    line = cur->line;
    col = cur->col;
    advance_tok(); /* consume 'do' */
    body = parse_stmt();
    if (!at(TOK_WHILE)) error(cur->line, cur->col, "expected 'while' after do body");
    advance_tok(); /* consume 'while' */
    expect(TOK_LPAREN, "expected '(' after while");
    cond = parse_expr();
    expect(TOK_RPAREN, "expected ')'");
    expect(TOK_SEMI, "expected ';' after do-while");
    n = node_new(ND_DO_WHILE, line, col);
    n->c0 = body;
    n->c1 = cond;
    return n;
}

struct Node *parse_switch(void) {
    struct Node *n;
    struct Node *expr;
    int line;
    int col;
    struct NList *stmts;

    line = cur->line;
    col = cur->col;
    advance_tok();
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

struct Node *parse_stmt(void) {
    struct Node *n;
    struct Node *expr;
    int line;
    int col;

    if (at(TOK_RETURN)) {
        line = cur->line;
        col = cur->col;
        advance_tok();
        n = node_new(ND_RETURN, line, col);
        if (!at(TOK_SEMI)) {
            n->c0 = parse_expr();
        }
        expect(TOK_SEMI, "expected ';' after return");
        return n;
    }
    if (at(TOK_IF)) return parse_if();
    if (at(TOK_DO)) return parse_do_while();
    if (at(TOK_SWITCH)) return parse_switch();
    if (at(TOK_CASE)) {
        int cv;
        struct Node *cn;
        advance_tok();
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
        advance_tok();
        expect(TOK_COLON, "expected ':' after default");
        return node_new(ND_DEFAULT, cur->line, cur->col);
    }
    if (at(TOK_WHILE)) return parse_while();
    if (at(TOK_FOR)) return parse_for();
    if (at(TOK_BREAK)) {
        line = cur->line;
        col = cur->col;
        advance_tok();
        expect(TOK_SEMI, "expected ';' after break");
        return node_new(ND_BREAK, line, col);
    }
    if (at(TOK_CONTINUE)) {
        line = cur->line;
        col = cur->col;
        advance_tok();
        expect(TOK_SEMI, "expected ';' after continue");
        return node_new(ND_CONTINUE, line, col);
    }
    if (at(TOK_LBRACE)) return parse_block();
    if (is_type_token()) return parse_var_decl();

    line = cur->line;
    col = cur->col;
    expr = parse_expr();
    expect(TOK_SEMI, "expected ';'");
    n = node_new(ND_EXPR_STMT, line, col);
    n->c0 = expr;
    return n;
}

struct Node *parse_block(void) {
    struct Node *n;
    struct NList *stmts;
    int line;
    int col;

    line = cur->line;
    col = cur->col;
    expect(TOK_LBRACE, "expected '{'");
    stmts = (struct NList *)malloc(sizeof(struct NList));
    stmts->items = (struct Node **)0;
    stmts->count = 0;
    stmts->cap = 0;
    while (!at(TOK_RBRACE) && !at(TOK_EOF)) {
        nlist_push(stmts, parse_stmt());
    }
    expect(TOK_RBRACE, "expected '}'");
    n = node_new(ND_BLOCK, line, col);
    n->list = stmts->items;
    n->ival2 = stmts->count;
    return n;
}

/* --- Top-level --- */

int parse_type(void) {
    while (at(TOK_CONST)) advance_tok();
    if (at(TOK_INT)) {
        advance_tok();
        return 0;
    }
    if (at(TOK_VOID)) {
        advance_tok();
        return 1;
    }
    if (at(TOK_CHAR_KW)) {
        advance_tok();
        return 2;
    }
    if (at(TOK_STRUCT)) {
        advance_tok();
        if (at(TOK_IDENT)) advance_tok();
        return 0;
    }
    if (at(TOK_ENUM)) {
        advance_tok();
        if (at(TOK_IDENT)) advance_tok();
        return 0;
    }
    error(cur->line, cur->col, "expected type");
    return 0;
}

struct Node *parse_func(void) {
    struct Node *n;
    struct Node *pn;
    struct NList *params;
    int line;
    int col;
    int ret;
    int pty;

    line = cur->line;
    col = cur->col;
    ret = parse_type();
    while (at(TOK_STAR)) advance_tok();
    if (!at(TOK_IDENT)) error(cur->line, cur->col, "expected function name");
    n = node_new(ND_FUNC, line, col);
    n->sval = strdupn(cur->text, 127);
    n->ival = ret;

    /* register in function signature table */
    if (nfunc_sigs < MAX_FUNC_SIGS) {
        func_sigs[nfunc_sigs] = (struct FuncSig *)malloc(sizeof(struct FuncSig));
        func_sigs[nfunc_sigs]->name = strdupn(cur->text, 127);
        if (ret == 1) {
            func_sigs[nfunc_sigs]->is_void = 1;
        } else {
            func_sigs[nfunc_sigs]->is_void = 0;
        }
        nfunc_sigs = nfunc_sigs + 1;
    }
    advance_tok();
    expect(TOK_LPAREN, "expected '('");

    /* parameters — track char* type via ival2 */
    params = (struct NList *)malloc(sizeof(struct NList));
    params->items = (struct Node **)0;
    params->count = 0;
    params->cap = 0;
    if (!at(TOK_RPAREN)) {
        if (at(TOK_VOID)) {
            advance_tok();
        } else {
            pty = parse_type();
            while (at(TOK_STAR)) advance_tok();
            if (at(TOK_IDENT)) {
                pn = node_new(ND_IDENT, cur->line, cur->col);
                pn->sval = strdupn(cur->text, 127);
                if (pty == 2) { pn->ival2 = 1; } else { pn->ival2 = 0; }
                nlist_push(params, pn);
                advance_tok();
            }
            while (at(TOK_COMMA)) {
                advance_tok();
                pty = parse_type();
                while (at(TOK_STAR)) advance_tok();
                if (at(TOK_IDENT)) {
                    pn = node_new(ND_IDENT, cur->line, cur->col);
                    pn->sval = strdupn(cur->text, 127);
                    if (pty == 2) { pn->ival2 = 1; } else { pn->ival2 = 0; }
                    nlist_push(params, pn);
                    advance_tok();
                }
            }
        }
    }
    expect(TOK_RPAREN, "expected ')'");
    n->list = params->items;
    n->ival2 = params->count;

    if (at(TOK_SEMI)) {
        advance_tok();
        n->c0 = (struct Node *)0;
    } else {
        n->c0 = parse_block();
    }
    return n;
}

void parse_struct_def(void) {
    struct StructDef *sd;
    int offset;

    advance_tok();
    if (!at(TOK_IDENT)) error(cur->line, cur->col, "expected struct name");
    if (nstructs >= MAX_STRUCTS) error(cur->line, cur->col, "too many structs");
    structs_reg[nstructs] = (struct StructDef *)malloc(sizeof(struct StructDef));
    sd = structs_reg[nstructs];
    nstructs = nstructs + 1;
    sd->name = strdupn(cur->text, 127);
    sd->nfields = 0;
    sd->fields = (struct StructField **)malloc(MAX_FIELDS * sizeof(void *));
    advance_tok();
    expect(TOK_LBRACE, "expected '{' in struct definition");
    offset = 0;
    while (!at(TOK_RBRACE) && !at(TOK_EOF)) {
        if (at(TOK_STRUCT)) {
            advance_tok();
            if (at(TOK_IDENT)) advance_tok();
        } else {
            parse_type();
        }
        while (at(TOK_STAR)) advance_tok();
        if (!at(TOK_IDENT)) error(cur->line, cur->col, "expected field name");
        sd->fields[sd->nfields] = (struct StructField *)malloc(sizeof(struct StructField));
        sd->fields[sd->nfields]->name = strdupn(cur->text, 127);
        sd->fields[sd->nfields]->fld_offset = offset;
        sd->nfields = sd->nfields + 1;
        offset = offset + 4;
        advance_tok();
        expect(TOK_SEMI, "expected ';' after field");
    }
    expect(TOK_RBRACE, "expected '}'");
    sd->size = offset;
    expect(TOK_SEMI, "expected ';' after struct definition");
}

void parse_global_var(void) {
    int is_char;
    int neg;

    is_char = 0;
    while (at(TOK_CONST)) advance_tok();
    if (at(TOK_STRUCT)) {
        advance_tok();
        if (at(TOK_IDENT)) advance_tok();
    } else {
        if (at(TOK_CHAR_KW)) is_char = 1;
        advance_tok();
    }
    while (at(TOK_STAR)) advance_tok();
    if (!at(TOK_IDENT)) error(cur->line, cur->col, "expected variable name");
    if (nglobals >= MAX_GLOBALS) error(cur->line, cur->col, "too many globals");
    globals_tbl[nglobals] = (struct GlobalVar *)malloc(sizeof(struct GlobalVar));
    globals_tbl[nglobals]->name = strdupn(cur->text, 127);
    globals_tbl[nglobals]->init_val = 0;
    globals_tbl[nglobals]->gv_is_char = is_char;
    advance_tok();
    if (at(TOK_EQ)) {
        advance_tok();
        if (at(TOK_INT_LIT)) {
            globals_tbl[nglobals]->init_val = cur->int_val;
            advance_tok();
        } else if (at(TOK_MINUS) || at(TOK_CHAR_LIT)) {
            neg = 0;
            if (at(TOK_MINUS)) {
                neg = 1;
                advance_tok();
            }
            if (neg) {
                globals_tbl[nglobals]->init_val = -cur->int_val;
            } else {
                globals_tbl[nglobals]->init_val = cur->int_val;
            }
            advance_tok();
        }
    }
    nglobals = nglobals + 1;
    expect(TOK_SEMI, "expected ';' after global declaration");
}

void parse_enum_def(void) {
    int val;
    char *ename;
    struct EnumConst *ec;

    advance_tok(); /* consume 'enum' */
    if (at(TOK_IDENT)) advance_tok(); /* skip optional tag */
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
            ec = (struct EnumConst *)malloc(sizeof(struct EnumConst));
            ec->name = ename;
            ec->val = val;
            enum_consts[nenum_consts] = ec;
            nenum_consts = nenum_consts + 1;
        }
        val = val + 1;
        if (at(TOK_COMMA)) advance_tok();
    }
    expect(TOK_RBRACE, "expected '}'");
    expect(TOK_SEMI, "expected ';' after enum");
}

struct Node *parse_program(void) {
    struct Node *prog;
    struct NList *decls;
    int sp;
    int sl;
    int sc;
    struct Token *st;
    int is_def;
    int is_func;

    prog = node_new(ND_PROGRAM, 1, 1);
    decls = (struct NList *)malloc(sizeof(struct NList));
    decls->items = (struct Node **)0;
    decls->count = 0;
    decls->cap = 0;
    while (!at(TOK_EOF)) {
        /* enum definition */
        if (at(TOK_ENUM)) {
            int sp2;
            int sl2;
            int sc2;
            struct Token *st2;
            int is_enum_def;
            sp2 = lex_pos; sl2 = lex_line; sc2 = lex_col; st2 = cur;
            advance_tok(); /* look past 'enum' */
            if (at(TOK_IDENT)) advance_tok(); /* look past tag */
            is_enum_def = at(TOK_LBRACE);
            lex_pos = sp2; lex_line = sl2; lex_col = sc2; cur = st2;
            if (is_enum_def) {
                parse_enum_def();
                continue;
            }
        }

        /* struct definition */
        if (at(TOK_STRUCT)) {
            sp = lex_pos;
            sl = lex_line;
            sc = lex_col;
            st = cur;
            advance_tok();
            is_def = 0;
            if (at(TOK_IDENT)) {
                advance_tok();
                is_def = at(TOK_LBRACE);
            }
            lex_pos = sp;
            lex_line = sl;
            lex_col = sc;
            cur = st;
            if (is_def) {
                parse_struct_def();
                continue;
            }
        }

        /* Peek ahead to distinguish function from global variable */
        sp = lex_pos;
        sl = lex_line;
        sc = lex_col;
        st = cur;
        if (at(TOK_STRUCT) || at(TOK_ENUM)) {
            advance_tok();
            if (at(TOK_IDENT)) advance_tok();
        } else {
            while (at(TOK_CONST)) advance_tok();
            advance_tok();
        }
        while (at(TOK_STAR)) advance_tok();
        is_func = 0;
        if (at(TOK_IDENT)) {
            advance_tok();
            is_func = at(TOK_LPAREN);
        }
        lex_pos = sp;
        lex_line = sl;
        lex_col = sc;
        cur = st;
        if (is_func) {
            nlist_push(decls, parse_func());
        } else {
            parse_global_var();
        }
    }
    prog->list = decls->items;
    prog->ival2 = decls->count;
    return prog;
}

/* ================================================================
 * WAT Code Generator
 * ================================================================ */

int indent_level;

void emit_indent(void) {
    int i;
    for (i = 0; i < indent_level; i = i + 1) {
        printf("  ");
    }
}

/* --- Local variable tracking --- */

struct LocalVar {
    char *name;
    int lv_is_char;
};

struct LocalVar **local_vars;
int nlocals;

void init_local_tracking(void) {
    local_vars = (struct LocalVar **)malloc(MAX_LOCALS * sizeof(void *));
    nlocals = 0;
}

int find_local(char *name) {
    int i;
    for (i = 0; i < nlocals; i = i + 1) {
        if (strcmp(local_vars[i]->name, name) == 0) return i;
    }
    return -1;
}

void add_local(char *name, int is_char) {
    if (find_local(name) >= 0) return;
    if (nlocals >= MAX_LOCALS) {
        printf("too many locals\n");
        exit(1);
    }
    local_vars[nlocals] = (struct LocalVar *)malloc(sizeof(struct LocalVar));
    local_vars[nlocals]->name = strdupn(name, 127);
    local_vars[nlocals]->lv_is_char = is_char;
    nlocals = nlocals + 1;
}

void collect_locals(struct Node *n) {
    int i;
    if (n == (struct Node *)0) return;
    if (n->kind == ND_VAR_DECL) {
        add_local(n->sval, n->ival2);
    } else if (n->kind == ND_BLOCK) {
        for (i = 0; i < n->ival2; i = i + 1) {
            collect_locals(n->list[i]);
        }
    } else if (n->kind == ND_IF) {
        collect_locals(n->c1);
        collect_locals(n->c2);
    } else if (n->kind == ND_WHILE) {
        collect_locals(n->c1);
    } else if (n->kind == ND_FOR) {
        collect_locals(n->c0);
        collect_locals(n->c3);
    } else if (n->kind == ND_DO_WHILE) {
        collect_locals(n->c0); /* condition (c1) cannot contain declarations */
    } else if (n->kind == ND_SWITCH) {
        int i;
        for (i = 0; i < n->ival2; i = i + 1) {
            collect_locals(n->list[i]);
        }
    } else if (n->kind == ND_POST_INC || n->kind == ND_POST_DEC) {
        collect_locals(n->c0);
    }
}

/* --- Loop label management --- */

int label_cnt;
int *brk_lbl;
int *cont_lbl;
int loop_sp;

void init_loop_labels(void) {
    brk_lbl = (int *)malloc(MAX_LOOP_DEPTH * sizeof(int));
    cont_lbl = (int *)malloc(MAX_LOOP_DEPTH * sizeof(int));
    loop_sp = 0;
}

/* --- Variable type lookup --- */

int var_elem_size(char *name) {
    int li;
    int gi;
    li = find_local(name);
    if (li >= 0) {
        if (local_vars[li]->lv_is_char) return 1;
        return 4;
    }
    gi = find_global(name);
    if (gi >= 0) {
        if (globals_tbl[gi]->gv_is_char) return 1;
        return 4;
    }
    return 4;
}

int expr_elem_size(struct Node *n) {
    if (n->kind == ND_IDENT) return var_elem_size(n->sval);
    return 4;
}

/* --- Expression codegen --- */

void gen_expr(struct Node *n) {
    struct Node *tgt;
    char *name;
    int is_global;
    int off;
    int esz;
    char *fmt;
    int flen;
    int ai;
    int fi;
    int sid;
    int i;

    if (n->kind == ND_INT_LIT) {
        emit_indent();
        printf("i32.const %d\n", n->ival);
    } else if (n->kind == ND_IDENT) {
        if (find_global(n->sval) >= 0) {
            emit_indent();
            printf("global.get $%s\n", n->sval);
        } else {
            emit_indent();
            printf("local.get $%s\n", n->sval);
        }
    } else if (n->kind == ND_ASSIGN) {
        tgt = n->c0;
        if (tgt->kind == ND_IDENT) {
            name = tgt->sval;
            is_global = (find_global(name) >= 0);
            if (n->ival == TOK_EQ) {
                gen_expr(n->c1);
            } else if (n->ival == TOK_PLUS_EQ) {
                emit_indent();
                if (is_global) {
                    printf("global.get $%s\n", name);
                } else {
                    printf("local.get $%s\n", name);
                }
                gen_expr(n->c1);
                emit_indent();
                printf("i32.add\n");
            } else if (n->ival == TOK_MINUS_EQ) {
                emit_indent();
                if (is_global) {
                    printf("global.get $%s\n", name);
                } else {
                    printf("local.get $%s\n", name);
                }
                gen_expr(n->c1);
                emit_indent();
                printf("i32.sub\n");
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
            }
            if (is_global) {
                emit_indent();
                printf("local.set $__atmp\n");
                emit_indent();
                printf("local.get $__atmp\n");
                emit_indent();
                printf("global.set $%s\n", name);
                emit_indent();
                printf("local.get $__atmp\n");
            } else {
                emit_indent();
                printf("local.tee $%s\n", name);
            }
        } else if (tgt->kind == ND_UNARY && tgt->ival == TOK_STAR) {
            esz = expr_elem_size(tgt->c0);
            if (n->ival == TOK_EQ) {
                gen_expr(n->c1);
            } else if (n->ival == TOK_PLUS_EQ) {
                gen_expr(tgt->c0);
                emit_indent();
                if (esz == 1) {
                    printf("i32.load8_u\n");
                } else {
                    printf("i32.load\n");
                }
                gen_expr(n->c1);
                emit_indent();
                printf("i32.add\n");
            } else if (n->ival == TOK_MINUS_EQ) {
                gen_expr(tgt->c0);
                emit_indent();
                if (esz == 1) {
                    printf("i32.load8_u\n");
                } else {
                    printf("i32.load\n");
                }
                gen_expr(n->c1);
                emit_indent();
                printf("i32.sub\n");
            } else if (n->ival == TOK_PIPE_EQ || n->ival == TOK_AMP_EQ ||
                       n->ival == TOK_CARET_EQ || n->ival == TOK_LSHIFT_EQ ||
                       n->ival == TOK_RSHIFT_EQ) {
                gen_expr(tgt->c0);
                emit_indent();
                if (esz == 1) {
                    printf("i32.load8_u\n");
                } else {
                    printf("i32.load\n");
                }
                gen_expr(n->c1);
                emit_indent();
                if (n->ival == TOK_PIPE_EQ) { printf("i32.or\n"); }
                else if (n->ival == TOK_AMP_EQ) { printf("i32.and\n"); }
                else if (n->ival == TOK_CARET_EQ) { printf("i32.xor\n"); }
                else if (n->ival == TOK_LSHIFT_EQ) { printf("i32.shl\n"); }
                else { printf("i32.shr_s\n"); }
            }
            emit_indent();
            printf("local.set $__atmp\n");
            gen_expr(tgt->c0);
            emit_indent();
            printf("local.get $__atmp\n");
            emit_indent();
            if (esz == 1) {
                printf("i32.store8\n");
            } else {
                printf("i32.store\n");
            }
            emit_indent();
            printf("local.get $__atmp\n");
        } else if (tgt->kind == ND_MEMBER) {
            off = resolve_field_offset(tgt->sval);
            if (off < 0) error(tgt->nline, tgt->ncol, "unknown struct field");
            if (n->ival == TOK_EQ) {
                gen_expr(n->c1);
            } else if (n->ival == TOK_PLUS_EQ) {
                gen_expr(tgt->c0);
                if (off > 0) {
                    emit_indent();
                    printf("i32.const %d\n", off);
                    emit_indent();
                    printf("i32.add\n");
                }
                emit_indent();
                printf("i32.load\n");
                gen_expr(n->c1);
                emit_indent();
                printf("i32.add\n");
            } else if (n->ival == TOK_MINUS_EQ) {
                gen_expr(tgt->c0);
                if (off > 0) {
                    emit_indent();
                    printf("i32.const %d\n", off);
                    emit_indent();
                    printf("i32.add\n");
                }
                emit_indent();
                printf("i32.load\n");
                gen_expr(n->c1);
                emit_indent();
                printf("i32.sub\n");
            } else if (n->ival == TOK_PIPE_EQ || n->ival == TOK_AMP_EQ ||
                       n->ival == TOK_CARET_EQ || n->ival == TOK_LSHIFT_EQ ||
                       n->ival == TOK_RSHIFT_EQ) {
                gen_expr(tgt->c0);
                if (off > 0) {
                    emit_indent();
                    printf("i32.const %d\n", off);
                    emit_indent();
                    printf("i32.add\n");
                }
                emit_indent();
                printf("i32.load\n");
                gen_expr(n->c1);
                emit_indent();
                if (n->ival == TOK_PIPE_EQ) { printf("i32.or\n"); }
                else if (n->ival == TOK_AMP_EQ) { printf("i32.and\n"); }
                else if (n->ival == TOK_CARET_EQ) { printf("i32.xor\n"); }
                else if (n->ival == TOK_LSHIFT_EQ) { printf("i32.shl\n"); }
                else { printf("i32.shr_s\n"); }
            }
            emit_indent();
            printf("local.set $__atmp\n");
            gen_expr(tgt->c0);
            if (off > 0) {
                emit_indent();
                printf("i32.const %d\n", off);
                emit_indent();
                printf("i32.add\n");
            }
            emit_indent();
            printf("local.get $__atmp\n");
            emit_indent();
            printf("i32.store\n");
            emit_indent();
            printf("local.get $__atmp\n");
        } else if (tgt->kind == ND_SUBSCRIPT) {
            esz = expr_elem_size(tgt->c0);
            if (n->ival == TOK_EQ) {
                gen_expr(n->c1);
            } else if (n->ival == TOK_PLUS_EQ) {
                gen_expr(tgt->c0);
                gen_expr(tgt->c1);
                if (esz > 1) {
                    emit_indent();
                    printf("i32.const %d\n", esz);
                    emit_indent();
                    printf("i32.mul\n");
                }
                emit_indent();
                printf("i32.add\n");
                emit_indent();
                if (esz == 1) {
                    printf("i32.load8_u\n");
                } else {
                    printf("i32.load\n");
                }
                gen_expr(n->c1);
                emit_indent();
                printf("i32.add\n");
            } else if (n->ival == TOK_MINUS_EQ) {
                gen_expr(tgt->c0);
                gen_expr(tgt->c1);
                if (esz > 1) {
                    emit_indent();
                    printf("i32.const %d\n", esz);
                    emit_indent();
                    printf("i32.mul\n");
                }
                emit_indent();
                printf("i32.add\n");
                emit_indent();
                if (esz == 1) {
                    printf("i32.load8_u\n");
                } else {
                    printf("i32.load\n");
                }
                gen_expr(n->c1);
                emit_indent();
                printf("i32.sub\n");
            } else if (n->ival == TOK_PIPE_EQ || n->ival == TOK_AMP_EQ ||
                       n->ival == TOK_CARET_EQ || n->ival == TOK_LSHIFT_EQ ||
                       n->ival == TOK_RSHIFT_EQ) {
                gen_expr(tgt->c0);
                gen_expr(tgt->c1);
                if (esz > 1) {
                    emit_indent();
                    printf("i32.const %d\n", esz);
                    emit_indent();
                    printf("i32.mul\n");
                }
                emit_indent();
                printf("i32.add\n");
                emit_indent();
                if (esz == 1) {
                    printf("i32.load8_u\n");
                } else {
                    printf("i32.load\n");
                }
                gen_expr(n->c1);
                emit_indent();
                if (n->ival == TOK_PIPE_EQ) { printf("i32.or\n"); }
                else if (n->ival == TOK_AMP_EQ) { printf("i32.and\n"); }
                else if (n->ival == TOK_CARET_EQ) { printf("i32.xor\n"); }
                else if (n->ival == TOK_LSHIFT_EQ) { printf("i32.shl\n"); }
                else { printf("i32.shr_s\n"); }
            }
            emit_indent();
            printf("local.set $__atmp\n");
            gen_expr(tgt->c0);
            gen_expr(tgt->c1);
            if (esz > 1) {
                emit_indent();
                printf("i32.const %d\n", esz);
                emit_indent();
                printf("i32.mul\n");
            }
            emit_indent();
            printf("i32.add\n");
            emit_indent();
            printf("local.get $__atmp\n");
            emit_indent();
            if (esz == 1) {
                printf("i32.store8\n");
            } else {
                printf("i32.store\n");
            }
            emit_indent();
            printf("local.get $__atmp\n");
        }
    } else if (n->kind == ND_UNARY) {
        if (n->ival == TOK_MINUS) {
            emit_indent();
            printf("i32.const 0\n");
            gen_expr(n->c0);
            emit_indent();
            printf("i32.sub\n");
        } else if (n->ival == TOK_BANG) {
            gen_expr(n->c0);
            emit_indent();
            printf("i32.eqz\n");
        } else if (n->ival == TOK_TILDE) {
            emit_indent();
            printf("i32.const -1\n");
            gen_expr(n->c0);
            emit_indent();
            printf("i32.xor\n");
        } else if (n->ival == TOK_STAR) {
            esz = expr_elem_size(n->c0);
            gen_expr(n->c0);
            emit_indent();
            if (esz == 1) {
                printf("i32.load8_u\n");
            } else {
                printf("i32.load\n");
            }
        } else if (n->ival == TOK_AMP) {
            error(n->nline, n->ncol, "cannot take address of this expression");
        }
    } else if (n->kind == ND_BINARY) {
        gen_expr(n->c0);
        gen_expr(n->c1);
        emit_indent();
        if (n->ival == TOK_PLUS) {
            printf("i32.add\n");
        } else if (n->ival == TOK_MINUS) {
            printf("i32.sub\n");
        } else if (n->ival == TOK_STAR) {
            printf("i32.mul\n");
        } else if (n->ival == TOK_SLASH) {
            printf("i32.div_s\n");
        } else if (n->ival == TOK_PERCENT) {
            printf("i32.rem_s\n");
        } else if (n->ival == TOK_EQ_EQ) {
            printf("i32.eq\n");
        } else if (n->ival == TOK_BANG_EQ) {
            printf("i32.ne\n");
        } else if (n->ival == TOK_LT) {
            printf("i32.lt_s\n");
        } else if (n->ival == TOK_GT) {
            printf("i32.gt_s\n");
        } else if (n->ival == TOK_LT_EQ) {
            printf("i32.le_s\n");
        } else if (n->ival == TOK_GT_EQ) {
            printf("i32.ge_s\n");
        } else if (n->ival == TOK_AMP_AMP) {
            printf("i32.and\n");
        } else if (n->ival == TOK_PIPE_PIPE) {
            printf("i32.or\n");
        } else if (n->ival == TOK_AMP) {
            printf("i32.and\n");
        } else if (n->ival == TOK_PIPE) {
            printf("i32.or\n");
        } else if (n->ival == TOK_LSHIFT) {
            printf("i32.shl\n");
        } else if (n->ival == TOK_RSHIFT) {
            printf("i32.shr_s\n");
        } else if (n->ival == TOK_CARET) {
            printf("i32.xor\n");
        } else {
            error(n->nline, n->ncol, "unsupported binary operator");
        }
    } else if (n->kind == ND_CALL) {
        if (strcmp(n->sval, "printf") == 0) {
            if (n->ival2 < 1 || n->list[0]->kind != ND_STR_LIT) {
                error(n->nline, n->ncol, "printf requires string literal format");
            }
            sid = n->list[0]->ival;
            fmt = str_table[sid]->data;
            flen = str_table[sid]->len;
            ai = 1;
            for (fi = 0; fi < flen; fi = fi + 1) {
                if (fmt[fi] == '%' && fi + 1 < flen) {
                    fi = fi + 1;
                    if (fmt[fi] == 'd') {
                        if (ai >= n->ival2) error(n->nline, n->ncol, "printf: missing arg for %d");
                        gen_expr(n->list[ai]);
                        ai = ai + 1;
                        emit_indent();
                        printf("call $__print_int\n");
                    } else if (fmt[fi] == 's') {
                        if (ai >= n->ival2) error(n->nline, n->ncol, "printf: missing arg for %s");
                        gen_expr(n->list[ai]);
                        ai = ai + 1;
                        emit_indent();
                        printf("call $__print_str\n");
                    } else if (fmt[fi] == 'c') {
                        if (ai >= n->ival2) error(n->nline, n->ncol, "printf: missing arg for %c");
                        gen_expr(n->list[ai]);
                        ai = ai + 1;
                        emit_indent();
                        printf("call $putchar\n");
                        emit_indent();
                        printf("drop\n");
                    } else if (fmt[fi] == 'x') {
                        if (ai >= n->ival2) error(n->nline, n->ncol, "printf: missing arg for %x");
                        gen_expr(n->list[ai]);
                        ai = ai + 1;
                        emit_indent();
                        printf("call $__print_hex\n");
                    } else if (fmt[fi] == '%') {
                        emit_indent();
                        printf("i32.const 37\n");
                        emit_indent();
                        printf("call $putchar\n");
                        emit_indent();
                        printf("drop\n");
                    } else {
                        error(n->nline, n->ncol, "unsupported printf format");
                    }
                } else {
                    emit_indent();
                    printf("i32.const %d\n", fmt[fi] & 255);
                    emit_indent();
                    printf("call $putchar\n");
                    emit_indent();
                    printf("drop\n");
                }
            }
            emit_indent();
            printf("i32.const 0\n");
        } else if (strcmp(n->sval, "putchar") == 0) {
            gen_expr(n->list[0]);
            emit_indent();
            printf("call $putchar\n");
        } else if (strcmp(n->sval, "getchar") == 0) {
            emit_indent();
            printf("call $getchar\n");
        } else if (strcmp(n->sval, "exit") == 0) {
            gen_expr(n->list[0]);
            emit_indent();
            printf("call $__proc_exit\n");
            emit_indent();
            printf("i32.const 0\n");
        } else if (strcmp(n->sval, "malloc") == 0) {
            gen_expr(n->list[0]);
            emit_indent();
            printf("call $malloc\n");
        } else if (strcmp(n->sval, "free") == 0) {
            if (n->ival2 > 0) {
                gen_expr(n->list[0]);
            } else {
                emit_indent();
                printf("i32.const 0\n");
            }
            emit_indent();
            printf("call $free\n");
            emit_indent();
            printf("i32.const 0\n");
        } else if (strcmp(n->sval, "strlen") == 0) {
            gen_expr(n->list[0]);
            emit_indent();
            printf("call $strlen\n");
        } else if (strcmp(n->sval, "strcmp") == 0) {
            gen_expr(n->list[0]);
            gen_expr(n->list[1]);
            emit_indent();
            printf("call $strcmp\n");
        } else if (strcmp(n->sval, "strncpy") == 0) {
            gen_expr(n->list[0]);
            gen_expr(n->list[1]);
            gen_expr(n->list[2]);
            emit_indent();
            printf("call $strncpy\n");
        } else if (strcmp(n->sval, "memcpy") == 0) {
            gen_expr(n->list[0]);
            gen_expr(n->list[1]);
            gen_expr(n->list[2]);
            emit_indent();
            printf("call $memcpy\n");
        } else if (strcmp(n->sval, "memset") == 0) {
            gen_expr(n->list[0]);
            gen_expr(n->list[1]);
            gen_expr(n->list[2]);
            emit_indent();
            printf("call $memset\n");
        } else if (strcmp(n->sval, "memcmp") == 0) {
            gen_expr(n->list[0]);
            gen_expr(n->list[1]);
            gen_expr(n->list[2]);
            emit_indent();
            printf("call $memcmp\n");
        } else {
            for (i = 0; i < n->ival2; i = i + 1) {
                gen_expr(n->list[i]);
            }
            emit_indent();
            printf("call $%s\n", n->sval);
            if (func_is_void(n->sval)) {
                emit_indent();
                printf("i32.const 0\n");
            }
        }
    } else if (n->kind == ND_STR_LIT) {
        emit_indent();
        printf("i32.const %d\n", str_table[n->ival]->offset);
    } else if (n->kind == ND_MEMBER) {
        off = resolve_field_offset(n->sval);
        if (off < 0) error(n->nline, n->ncol, "unknown struct field");
        gen_expr(n->c0);
        if (off > 0) {
            emit_indent();
            printf("i32.const %d\n", off);
            emit_indent();
            printf("i32.add\n");
        }
        emit_indent();
        printf("i32.load\n");
    } else if (n->kind == ND_SIZEOF) {
        struct StructDef *sd;
        int sz;
        if (n->ival == 1) {
            sz = 4; /* pointer type */
        } else if (n->c0 != (struct Node *)0) {
            /* sizeof(expr): infer size from variable */
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
    } else if (n->kind == ND_SUBSCRIPT) {
        esz = expr_elem_size(n->c0);
        gen_expr(n->c0);
        gen_expr(n->c1);
        if (esz > 1) {
            emit_indent();
            printf("i32.const %d\n", esz);
            emit_indent();
            printf("i32.mul\n");
        }
        emit_indent();
        printf("i32.add\n");
        emit_indent();
        if (esz == 1) {
            printf("i32.load8_u\n");
        } else {
            printf("i32.load\n");
        }
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
                emit_indent(); printf("global.get $%s\n", pname);
                emit_indent(); printf("local.set $__atmp\n");
                emit_indent(); printf("global.get $%s\n", pname);
                emit_indent(); printf("i32.const 1\n");
                emit_indent();
                if (n->kind == ND_POST_INC) { printf("i32.add\n"); } else { printf("i32.sub\n"); }
                emit_indent(); printf("global.set $%s\n", pname);
                emit_indent(); printf("local.get $__atmp\n");
            } else {
                emit_indent(); printf("local.get $%s\n", pname);
                emit_indent(); printf("local.get $%s\n", pname);
                emit_indent(); printf("i32.const 1\n");
                emit_indent();
                if (n->kind == ND_POST_INC) { printf("i32.add\n"); } else { printf("i32.sub\n"); }
                emit_indent(); printf("local.set $%s\n", pname);
            }
        } else if (tgt2->kind == ND_UNARY && tgt2->ival == TOK_STAR) {
            /* NOTE: tgt2->c0 evaluated 3x (save old val, store addr, reload).
               Correct only when pointer expression has no side effects. */
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
            /* NOTE: tgt2->c0 and tgt2->c1 each evaluated 3x (save old val, store addr, reload).
               Correct only when the array and index expressions have no side effects. */
            pesz = expr_elem_size(tgt2->c0);
            gen_expr(tgt2->c0); gen_expr(tgt2->c1);
            if (pesz > 1) { emit_indent(); printf("i32.const %d\n", pesz); emit_indent(); printf("i32.mul\n"); }
            emit_indent(); printf("i32.add\n");
            emit_indent();
            if (pesz == 1) { printf("i32.load8_u\n"); } else { printf("i32.load\n"); }
            emit_indent(); printf("local.set $__atmp\n");
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
            poff = resolve_field_offset(tgt2->sval);
            if (poff < 0) error(tgt2->nline, tgt2->ncol, "unknown struct field");
            gen_expr(tgt2->c0);
            if (poff > 0) { emit_indent(); printf("i32.const %d\n", poff); emit_indent(); printf("i32.add\n"); }
            emit_indent(); printf("i32.load\n");
            emit_indent(); printf("local.set $__atmp\n");
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
    } else if (n->kind == ND_TERNARY) {
        gen_expr(n->c0);
        /* both branches produce i32; compiler is uniformly i32 throughout */
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
    } else {
        error(n->nline, n->ncol, "unsupported expression in codegen");
    }
}

/* --- Statement codegen --- */

void gen_stmt(struct Node *n);

void gen_body(struct Node *n) {
    int i;
    if (n->kind == ND_BLOCK) {
        for (i = 0; i < n->ival2; i = i + 1) {
            gen_stmt(n->list[i]);
        }
    } else {
        gen_stmt(n);
    }
}

void gen_stmt(struct Node *n) {
    int lbl;
    int i;

    if (n->kind == ND_RETURN) {
        if (n->c0 != (struct Node *)0) {
            gen_expr(n->c0);
        }
        emit_indent();
        printf("return\n");
    } else if (n->kind == ND_VAR_DECL) {
        if (n->c0 != (struct Node *)0) {
            gen_expr(n->c0);
            emit_indent();
            printf("local.set $%s\n", n->sval);
        }
    } else if (n->kind == ND_EXPR_STMT) {
        gen_expr(n->c0);
        emit_indent();
        printf("drop\n");
    } else if (n->kind == ND_IF) {
        gen_expr(n->c0);
        if (n->c2 != (struct Node *)0) {
            emit_indent();
            printf("(if\n");
            indent_level = indent_level + 1;
            emit_indent();
            printf("(then\n");
            indent_level = indent_level + 1;
            gen_body(n->c1);
            indent_level = indent_level - 1;
            emit_indent();
            printf(")\n");
            emit_indent();
            printf("(else\n");
            indent_level = indent_level + 1;
            gen_body(n->c2);
            indent_level = indent_level - 1;
            emit_indent();
            printf(")\n");
            indent_level = indent_level - 1;
            emit_indent();
            printf(")\n");
        } else {
            emit_indent();
            printf("(if\n");
            indent_level = indent_level + 1;
            emit_indent();
            printf("(then\n");
            indent_level = indent_level + 1;
            gen_body(n->c1);
            indent_level = indent_level - 1;
            emit_indent();
            printf(")\n");
            indent_level = indent_level - 1;
            emit_indent();
            printf(")\n");
        }
    } else if (n->kind == ND_WHILE) {
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
        gen_expr(n->c0);
        emit_indent();
        printf("i32.eqz\n");
        emit_indent();
        printf("br_if $brk_%d\n", lbl);
        emit_indent();
        printf("(block $cont_%d\n", lbl);
        indent_level = indent_level + 1;
        gen_body(n->c1);
        indent_level = indent_level - 1;
        emit_indent();
        printf(")\n");
        emit_indent();
        printf("br $lp_%d\n", lbl);
        indent_level = indent_level - 1;
        emit_indent();
        printf(")\n");
        indent_level = indent_level - 1;
        emit_indent();
        printf(")\n");
        loop_sp = loop_sp - 1;
    } else if (n->kind == ND_FOR) {
        if (n->c0 != (struct Node *)0) {
            gen_stmt(n->c0);
        }
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
        if (n->c1 != (struct Node *)0) {
            gen_expr(n->c1);
            emit_indent();
            printf("i32.eqz\n");
            emit_indent();
            printf("br_if $brk_%d\n", lbl);
        }
        emit_indent();
        printf("(block $cont_%d\n", lbl);
        indent_level = indent_level + 1;
        gen_body(n->c3);
        indent_level = indent_level - 1;
        emit_indent();
        printf(")\n");
        if (n->c2 != (struct Node *)0) {
            gen_expr(n->c2);
            emit_indent();
            printf("drop\n");
        }
        emit_indent();
        printf("br $lp_%d\n", lbl);
        indent_level = indent_level - 1;
        emit_indent();
        printf(")\n");
        indent_level = indent_level - 1;
        emit_indent();
        printf(")\n");
        loop_sp = loop_sp - 1;
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
    } else if (n->kind == ND_BREAK) {
        if (loop_sp <= 0) error(n->nline, n->ncol, "break outside loop");
        emit_indent();
        printf("br $brk_%d\n", brk_lbl[loop_sp - 1]);
    } else if (n->kind == ND_CONTINUE) {
        if (loop_sp <= 0) error(n->nline, n->ncol, "continue outside loop");
        if (cont_lbl[loop_sp - 1] < 0) error(n->nline, n->ncol, "continue not inside a loop");
        emit_indent();
        printf("br $cont_%d\n", cont_lbl[loop_sp - 1]);
    } else if (n->kind == ND_BLOCK) {
        for (i = 0; i < n->ival2; i = i + 1) {
            gen_stmt(n->list[i]);
        }
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
        int si;
        int last_case_pos;

        nc = 0;
        dflt_pos = -1;
        has_dflt = 0;
        for (si = 0; si < n->ival2; si = si + 1) {
            if (n->list[si]->kind == ND_CASE) {
                if (nc >= MAX_CASES) {
                    error(n->nline, n->ncol, "too many cases in switch");
                }
                case_vals[nc] = n->list[si]->ival;
                case_start[nc] = si;
                nc = nc + 1;
            } else if (n->list[si]->kind == ND_DEFAULT) {
                dflt_pos = si;
                has_dflt = 1;
            }
        }

        /* enforce: default must be last (limitation of WAT codegen) */
        if (has_dflt) {
            last_case_pos = (nc > 0) ? case_start[nc - 1] : -1;
            if (dflt_pos < last_case_pos) {
                error(n->c0->nline, n->c0->ncol,
                      "default must appear after all case labels");
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

        /* save switch value */
        gen_expr(n->c0);
        emit_indent();
        printf("local.set $__stmp\n");

        /* outer break block */
        emit_indent();
        printf("(block $brk_%d\n", sw_lbl);
        indent_level = indent_level + 1;

        /* default target block (outermost) */
        emit_indent();
        printf("(block $sw%d_dflt\n", sw_lbl);
        indent_level = indent_level + 1;

        /* open case blocks in reverse order: first case = innermost */
        for (k = nc - 1; k >= 0; k = k - 1) {
            emit_indent();
            printf("(block $sw%d_c%d\n", sw_lbl, k);
            indent_level = indent_level + 1;
        }

        /* dispatch: compare and branch for each case */
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

        /* close case blocks in forward order and emit case bodies */
        for (k = 0; k < nc; k = k + 1) {
            indent_level = indent_level - 1;
            emit_indent(); printf(")\n");
            if (k + 1 < nc) {
                next_start = case_start[k + 1];
            } else if (has_dflt) {
                next_start = dflt_pos;
            } else {
                next_start = n->ival2;
            }
            for (j = case_start[k] + 1; j < next_start; j = j + 1) {
                if (n->list[j]->kind == ND_CASE) continue;
                if (n->list[j]->kind == ND_DEFAULT) continue;
                gen_stmt(n->list[j]);
            }
        }

        /* close default target block */
        indent_level = indent_level - 1;
        emit_indent(); printf(")\n");

        /* emit default body */
        if (has_dflt) {
            for (j = dflt_pos + 1; j < n->ival2; j = j + 1) {
                if (n->list[j]->kind == ND_CASE) continue;
                if (n->list[j]->kind == ND_DEFAULT) continue;
                gen_stmt(n->list[j]);
            }
        }

        /* close break block */
        indent_level = indent_level - 1;
        emit_indent(); printf(")\n");

        loop_sp = loop_sp - 1;
    } else {
        error(n->nline, n->ncol, "unsupported statement in codegen");
    }
}

/* --- Function codegen --- */

void gen_func(struct Node *n) {
    struct Node *body;
    int i;
    int nparam_locals;
    char *ret_sig;

    if (n->c0 == (struct Node *)0) return;

    nlocals = 0;
    label_cnt = 0;
    loop_sp = 0;
    /* register params as locals for is_char tracking */
    for (i = 0; i < n->ival2; i = i + 1) {
        add_local(n->list[i]->sval, n->list[i]->ival2);
    }
    nparam_locals = nlocals;
    collect_locals(n->c0);

    if (n->ival == 1) {
        ret_sig = "";
    } else {
        ret_sig = " (result i32)";
    }
    printf("  (func $%s", n->sval);
    for (i = 0; i < n->ival2; i = i + 1) {
        printf(" (param $%s i32)", n->list[i]->sval);
    }
    printf("%s\n", ret_sig);

    indent_level = 2;
    /* emit only non-param locals */
    for (i = nparam_locals; i < nlocals; i = i + 1) {
        emit_indent();
        printf("(local $%s i32)\n", local_vars[i]->name);
    }
    emit_indent();
    printf("(local $__atmp i32)\n");
    emit_indent();
    printf("(local $__stmp i32)\n");
    body = n->c0;
    for (i = 0; i < body->ival2; i = i + 1) {
        gen_stmt(body->list[i]);
    }
    if (n->ival != 1) {
        emit_indent();
        printf("i32.const 0\n");
    }
    indent_level = 1;
    emit_indent();
    printf(")\n");
}

/* --- WAT escape helper --- */

void emit_wat_string(char *data, int len) {
    int i;
    int c;
    int hi;
    int lo;
    for (i = 0; i < len; i = i + 1) {
        c = data[i] & 255;
        if (c >= 32 && c < 127 && c != '"' && c != '\\') {
            putchar(c);
        } else {
            putchar('\\');
            hi = (c >> 4) & 15;
            lo = c & 15;
            if (hi >= 10) {
                putchar('a' + hi - 10);
            } else {
                putchar('0' + hi);
            }
            if (lo >= 10) {
                putchar('a' + lo - 10);
            } else {
                putchar('0' + lo);
            }
        }
    }
}

/* --- Module codegen --- */

void gen_module(struct Node *prog) {
    int i;
    int gi;

    emit_indent();
    printf("(module\n");
    indent_level = indent_level + 1;

    /* WASI imports */
    emit_indent();
    printf("(import \"wasi_snapshot_preview1\" \"proc_exit\" (func $__proc_exit (param i32)))\n");
    emit_indent();
    printf("(import \"wasi_snapshot_preview1\" \"fd_write\" (func $__fd_write (param i32 i32 i32 i32) (result i32)))\n");
    emit_indent();
    printf("(import \"wasi_snapshot_preview1\" \"fd_read\" (func $__fd_read (param i32 i32 i32 i32) (result i32)))\n");
    emit_indent();
    printf("\n");

    /* memory */
    emit_indent();
    printf("(memory (export \"memory\") 256)\n");
    emit_indent();
    printf("\n");

    /* static data section */
    for (i = 0; i < nstrings; i = i + 1) {
        printf("  (data (i32.const %d) \"", str_table[i]->offset);
        emit_wat_string(str_table[i]->data, str_table[i]->len);
        printf("\\00\")\n");
    }
    if (nstrings > 0) {
        emit_indent();
        printf("\n");
    }

    /* heap pointer */
    emit_indent();
    printf("(global $__heap_ptr (mut i32) (i32.const %d))\n", data_ptr);

    /* user global variables */
    for (gi = 0; gi < nglobals; gi = gi + 1) {
        emit_indent();
        printf("(global $%s (mut i32) (i32.const %d))\n",
               globals_tbl[gi]->name, globals_tbl[gi]->init_val);
    }
    emit_indent();
    printf("\n");

    /* Runtime helper functions */
    emit_indent();
    printf("(func $putchar (param $ch i32) (result i32)\n");
    emit_indent();
    printf("  (i32.store8 (i32.const 0) (local.get $ch))\n");
    emit_indent();
    printf("  (i32.store (i32.const 4) (i32.const 0))\n");
    emit_indent();
    printf("  (i32.store (i32.const 8) (i32.const 1))\n");
    emit_indent();
    printf("  (drop (call $__fd_write (i32.const 1) (i32.const 4) (i32.const 1) (i32.const 12)))\n");
    emit_indent();
    printf("  (local.get $ch)\n");
    emit_indent();
    printf(")\n");

    emit_indent();
    printf("(func $getchar (result i32)\n");
    emit_indent();
    printf("  (i32.store (i32.const 4) (i32.const 0))\n");
    emit_indent();
    printf("  (i32.store (i32.const 8) (i32.const 1))\n");
    emit_indent();
    printf("  (drop (call $__fd_read (i32.const 0) (i32.const 4) (i32.const 1) (i32.const 12)))\n");
    emit_indent();
    printf("  (if (result i32) (i32.eqz (i32.load (i32.const 12)))\n");
    emit_indent();
    printf("    (then (i32.const -1))\n");
    emit_indent();
    printf("    (else (i32.load8_u (i32.const 0)))\n");
    emit_indent();
    printf("  )\n");
    emit_indent();
    printf(")\n");

    emit_indent();
    printf("(func $__print_int (param $n i32)\n");
    emit_indent();
    printf("  (local $buf i32)\n");
    emit_indent();
    printf("  (local $len i32)\n");
    emit_indent();
    printf("  (if (i32.lt_s (local.get $n) (i32.const 0))\n");
    emit_indent();
    printf("    (then\n");
    emit_indent();
    printf("      (drop (call $putchar (i32.const 45)))\n");
    emit_indent();
    printf("      (local.set $n (i32.sub (i32.const 0) (local.get $n)))\n");
    emit_indent();
    printf("    )\n");
    emit_indent();
    printf("  )\n");
    emit_indent();
    printf("  (if (i32.eqz (local.get $n))\n");
    emit_indent();
    printf("    (then (drop (call $putchar (i32.const 48))) (return))\n");
    emit_indent();
    printf("  )\n");
    emit_indent();
    printf("  (local.set $buf (i32.const 48))\n");
    emit_indent();
    printf("  (local.set $len (i32.const 0))\n");
    emit_indent();
    printf("  (block $done (loop $digit\n");
    emit_indent();
    printf("    (br_if $done (i32.eqz (local.get $n)))\n");
    emit_indent();
    printf("    (local.set $buf (i32.sub (local.get $buf) (i32.const 1)))\n");
    emit_indent();
    printf("    (i32.store8 (local.get $buf) (i32.add (i32.const 48) (i32.rem_u (local.get $n) (i32.const 10))))\n");
    emit_indent();
    printf("    (local.set $n (i32.div_u (local.get $n) (i32.const 10)))\n");
    emit_indent();
    printf("    (local.set $len (i32.add (local.get $len) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $digit)\n");
    emit_indent();
    printf("  ))\n");
    emit_indent();
    printf("  (block $pd (loop $pl\n");
    emit_indent();
    printf("    (br_if $pd (i32.eqz (local.get $len)))\n");
    emit_indent();
    printf("    (drop (call $putchar (i32.load8_u (local.get $buf))))\n");
    emit_indent();
    printf("    (local.set $buf (i32.add (local.get $buf) (i32.const 1)))\n");
    emit_indent();
    printf("    (local.set $len (i32.sub (local.get $len) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $pl)\n");
    emit_indent();
    printf("  ))\n");
    emit_indent();
    printf(")\n");

    emit_indent();
    printf("(func $__print_str (param $ptr i32)\n");
    emit_indent();
    printf("  (local $ch i32)\n");
    emit_indent();
    printf("  (block $done (loop $next\n");
    emit_indent();
    printf("    (local.set $ch (i32.load8_u (local.get $ptr)))\n");
    emit_indent();
    printf("    (br_if $done (i32.eqz (local.get $ch)))\n");
    emit_indent();
    printf("    (drop (call $putchar (local.get $ch)))\n");
    emit_indent();
    printf("    (local.set $ptr (i32.add (local.get $ptr) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $next)\n");
    emit_indent();
    printf("  ))\n");
    emit_indent();
    printf(")\n");

    emit_indent();
    printf("(func $__print_hex (param $n i32)\n");
    emit_indent();
    printf("  (local $buf i32)\n");
    emit_indent();
    printf("  (local $len i32)\n");
    emit_indent();
    printf("  (local $d i32)\n");
    emit_indent();
    printf("  (if (i32.eqz (local.get $n))\n");
    emit_indent();
    printf("    (then (drop (call $putchar (i32.const 48))) (return))\n");
    emit_indent();
    printf("  )\n");
    emit_indent();
    printf("  (local.set $buf (i32.const 48))\n");
    emit_indent();
    printf("  (local.set $len (i32.const 0))\n");
    emit_indent();
    printf("  (block $done (loop $digit\n");
    emit_indent();
    printf("    (br_if $done (i32.eqz (local.get $n)))\n");
    emit_indent();
    printf("    (local.set $buf (i32.sub (local.get $buf) (i32.const 1)))\n");
    emit_indent();
    printf("    (local.set $d (i32.and (local.get $n) (i32.const 15)))\n");
    emit_indent();
    printf("    (if (i32.lt_u (local.get $d) (i32.const 10))\n");
    emit_indent();
    printf("      (then (i32.store8 (local.get $buf) (i32.add (i32.const 48) (local.get $d))))\n");
    emit_indent();
    printf("      (else (i32.store8 (local.get $buf) (i32.add (i32.const 87) (local.get $d))))\n");
    emit_indent();
    printf("    )\n");
    emit_indent();
    printf("    (local.set $n (i32.shr_u (local.get $n) (i32.const 4)))\n");
    emit_indent();
    printf("    (local.set $len (i32.add (local.get $len) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $digit)\n");
    emit_indent();
    printf("  ))\n");
    emit_indent();
    printf("  (block $pd (loop $pl\n");
    emit_indent();
    printf("    (br_if $pd (i32.eqz (local.get $len)))\n");
    emit_indent();
    printf("    (drop (call $putchar (i32.load8_u (local.get $buf))))\n");
    emit_indent();
    printf("    (local.set $buf (i32.add (local.get $buf) (i32.const 1)))\n");
    emit_indent();
    printf("    (local.set $len (i32.sub (local.get $len) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $pl)\n");
    emit_indent();
    printf("  ))\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    emit_indent();
    printf("(func $malloc (param $size i32) (result i32)\n");
    emit_indent();
    printf("  (local $ptr i32)\n");
    emit_indent();
    printf("  (local.set $ptr (global.get $__heap_ptr))\n");
    emit_indent();
    printf("  (global.set $__heap_ptr (i32.add (global.get $__heap_ptr)\n");
    emit_indent();
    printf("    (i32.and (i32.add (local.get $size) (i32.const 7)) (i32.const -8))))\n");
    emit_indent();
    printf("  (local.get $ptr)\n");
    emit_indent();
    printf(")\n");

    emit_indent();
    printf("(func $free (param $ptr i32))\n");

    emit_indent();
    printf("(func $strlen (param $s i32) (result i32)\n");
    emit_indent();
    printf("  (local $n i32)\n");
    emit_indent();
    printf("  (local.set $n (i32.const 0))\n");
    emit_indent();
    printf("  (block $done (loop $next\n");
    emit_indent();
    printf("    (br_if $done (i32.eqz (i32.load8_u (i32.add (local.get $s) (local.get $n)))))\n");
    emit_indent();
    printf("    (local.set $n (i32.add (local.get $n) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $next)\n");
    emit_indent();
    printf("  ))\n");
    emit_indent();
    printf("  (local.get $n)\n");
    emit_indent();
    printf(")\n");

    emit_indent();
    printf("(func $strcmp (param $a i32) (param $b i32) (result i32)\n");
    emit_indent();
    printf("  (local $ca i32) (local $cb i32)\n");
    emit_indent();
    printf("  (block $done (loop $next\n");
    emit_indent();
    printf("    (local.set $ca (i32.load8_u (local.get $a)))\n");
    emit_indent();
    printf("    (local.set $cb (i32.load8_u (local.get $b)))\n");
    emit_indent();
    printf("    (br_if $done (i32.ne (local.get $ca) (local.get $cb)))\n");
    emit_indent();
    printf("    (br_if $done (i32.eqz (local.get $ca)))\n");
    emit_indent();
    printf("    (local.set $a (i32.add (local.get $a) (i32.const 1)))\n");
    emit_indent();
    printf("    (local.set $b (i32.add (local.get $b) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $next)\n");
    emit_indent();
    printf("  ))\n");
    emit_indent();
    printf("  (i32.sub (local.get $ca) (local.get $cb))\n");
    emit_indent();
    printf(")\n");

    emit_indent();
    printf("(func $strncpy (param $dst i32) (param $src i32) (param $n i32) (result i32)\n");
    emit_indent();
    printf("  (local $i i32)\n");
    emit_indent();
    printf("  (local.set $i (i32.const 0))\n");
    emit_indent();
    printf("  (block $done (loop $next\n");
    emit_indent();
    printf("    (br_if $done (i32.ge_u (local.get $i) (local.get $n)))\n");
    emit_indent();
    printf("    (i32.store8 (i32.add (local.get $dst) (local.get $i))\n");
    emit_indent();
    printf("      (i32.load8_u (i32.add (local.get $src) (local.get $i))))\n");
    emit_indent();
    printf("    (br_if $done (i32.eqz (i32.load8_u (i32.add (local.get $src) (local.get $i)))))\n");
    emit_indent();
    printf("    (local.set $i (i32.add (local.get $i) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $next)\n");
    emit_indent();
    printf("  ))\n");
    emit_indent();
    printf("  (local.get $dst)\n");
    emit_indent();
    printf(")\n");

    emit_indent();
    printf("(func $memcpy (param $dst i32) (param $src i32) (param $n i32) (result i32)\n");
    emit_indent();
    printf("  (local $i i32)\n");
    emit_indent();
    printf("  (local.set $i (i32.const 0))\n");
    emit_indent();
    printf("  (block $done (loop $next\n");
    emit_indent();
    printf("    (br_if $done (i32.ge_u (local.get $i) (local.get $n)))\n");
    emit_indent();
    printf("    (i32.store8 (i32.add (local.get $dst) (local.get $i))\n");
    emit_indent();
    printf("      (i32.load8_u (i32.add (local.get $src) (local.get $i))))\n");
    emit_indent();
    printf("    (local.set $i (i32.add (local.get $i) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $next)\n");
    emit_indent();
    printf("  ))\n");
    emit_indent();
    printf("  (local.get $dst)\n");
    emit_indent();
    printf(")\n");

    emit_indent();
    printf("(func $memset (param $dst i32) (param $val i32) (param $n i32) (result i32)\n");
    emit_indent();
    printf("  (local $i i32)\n");
    emit_indent();
    printf("  (local.set $i (i32.const 0))\n");
    emit_indent();
    printf("  (block $done (loop $next\n");
    emit_indent();
    printf("    (br_if $done (i32.ge_u (local.get $i) (local.get $n)))\n");
    emit_indent();
    printf("    (i32.store8 (i32.add (local.get $dst) (local.get $i)) (local.get $val))\n");
    emit_indent();
    printf("    (local.set $i (i32.add (local.get $i) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $next)\n");
    emit_indent();
    printf("  ))\n");
    emit_indent();
    printf("  (local.get $dst)\n");
    emit_indent();
    printf(")\n");

    emit_indent();
    printf("(func $memcmp (param $a i32) (param $b i32) (param $n i32) (result i32)\n");
    emit_indent();
    printf("  (local $i i32) (local $ca i32) (local $cb i32)\n");
    emit_indent();
    printf("  (local.set $i (i32.const 0))\n");
    emit_indent();
    printf("  (block $done (loop $next\n");
    emit_indent();
    printf("    (br_if $done (i32.ge_u (local.get $i) (local.get $n)))\n");
    emit_indent();
    printf("    (local.set $ca (i32.load8_u (i32.add (local.get $a) (local.get $i))))\n");
    emit_indent();
    printf("    (local.set $cb (i32.load8_u (i32.add (local.get $b) (local.get $i))))\n");
    emit_indent();
    printf("    (if (i32.ne (local.get $ca) (local.get $cb))\n");
    emit_indent();
    printf("      (then (return (i32.sub (local.get $ca) (local.get $cb))))\n");
    emit_indent();
    printf("    )\n");
    emit_indent();
    printf("    (local.set $i (i32.add (local.get $i) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $next)\n");
    emit_indent();
    printf("  ))\n");
    emit_indent();
    printf("  (i32.const 0)\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* user functions */
    for (i = 0; i < prog->ival2; i = i + 1) {
        gen_func(prog->list[i]);
    }

    emit_indent();
    printf("\n");

    /* _start */
    emit_indent();
    printf("(func $_start (export \"_start\")\n");
    indent_level = indent_level + 1;
    emit_indent();
    printf("call $main\n");
    emit_indent();
    printf("call $__proc_exit\n");
    indent_level = indent_level - 1;
    emit_indent();
    printf(")\n");

    indent_level = indent_level - 1;
    emit_indent();
    printf(")\n");
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    struct Node *prog;

    init_macros();
    init_strings();
    init_structs();
    init_globals();
    init_func_sigs();
    init_enum_consts();
    init_local_tracking();
    init_loop_labels();

    read_source();
    lex_init();
    advance_tok();

    prog = parse_program();
    indent_level = 0;
    gen_module(prog);

    return 0;
}
