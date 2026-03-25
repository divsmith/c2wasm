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
 * Constants
 * ================================================================ */

/* Token kinds */
enum {
    TOK_EOF = 0,
    TOK_INT = 1,
    TOK_CHAR_KW = 2,
    TOK_VOID = 3,
    TOK_RETURN = 4,
    TOK_IF = 5,
    TOK_ELSE = 6,
    TOK_WHILE = 7,
    TOK_FOR = 8,
    TOK_BREAK = 9,
    TOK_CONTINUE = 10,
    TOK_STRUCT = 11,
    TOK_SIZEOF = 12,
    TOK_DEFINE = 13,
    TOK_IDENT = 14,
    TOK_INT_LIT = 15,
    TOK_CHAR_LIT = 16,
    TOK_STR_LIT = 17,
    TOK_LPAREN = 18,
    TOK_RPAREN = 19,
    TOK_LBRACE = 20,
    TOK_RBRACE = 21,
    TOK_LBRACKET = 22,
    TOK_RBRACKET = 23,
    TOK_SEMI = 24,
    TOK_COMMA = 25,
    TOK_DOT = 26,
    TOK_ARROW = 27,
    TOK_PLUS = 28,
    TOK_MINUS = 29,
    TOK_STAR = 30,
    TOK_SLASH = 31,
    TOK_PERCENT = 32,
    TOK_AMP = 33,
    TOK_BANG = 34,
    TOK_PIPE = 35,
    TOK_TILDE = 36,
    TOK_PIPE_PIPE = 37,
    TOK_AMP_AMP = 38,
    TOK_EQ = 39,
    TOK_PLUS_EQ = 40,
    TOK_MINUS_EQ = 41,
    TOK_EQ_EQ = 42,
    TOK_BANG_EQ = 43,
    TOK_LT = 44,
    TOK_GT = 45,
    TOK_LT_EQ = 46,
    TOK_GT_EQ = 47,
    TOK_LSHIFT = 48,
    TOK_RSHIFT = 49,
    TOK_PLUS_PLUS = 50,
    TOK_MINUS_MINUS = 51,
    TOK_CARET = 52,
    TOK_PIPE_EQ = 53,
    TOK_AMP_EQ = 54,
    TOK_CARET_EQ = 55,
    TOK_LSHIFT_EQ = 56,
    TOK_RSHIFT_EQ = 57,
    TOK_DO = 58,
    TOK_QUESTION = 59,
    TOK_COLON = 60, /* used by ternary ?: and by switch case/default labels */
    TOK_SWITCH = 61,
    TOK_CASE = 62,
    TOK_DEFAULT = 63,
    TOK_CONST = 64,
    TOK_ENUM = 65,
    TOK_TYPEDEF = 66,
    TOK_UNSIGNED = 67,
    TOK_SIGNED = 68,
    TOK_SHORT = 69,
    TOK_LONG = 70,
    TOK_FLOAT_LIT = 71,
    TOK_FLOAT = 72,
    TOK_DOUBLE = 73
};

/* Node kinds */
enum {
    ND_PROGRAM = 0,
    ND_FUNC = 1,
    ND_BLOCK = 2,
    ND_RETURN = 3,
    ND_INT_LIT = 4,
    ND_BINARY = 5,
    ND_UNARY = 6,
    ND_VAR_DECL = 7,
    ND_IDENT = 8,
    ND_ASSIGN = 9,
    ND_IF = 10,
    ND_WHILE = 11,
    ND_FOR = 12,
    ND_BREAK = 13,
    ND_CONTINUE = 14,
    ND_EXPR_STMT = 15,
    ND_CALL = 16,
    ND_STR_LIT = 17,
    ND_MEMBER = 18,
    ND_SIZEOF = 19,
    ND_SUBSCRIPT = 20,
    ND_POST_INC = 21,
    ND_POST_DEC = 22,
    ND_DO_WHILE = 23,
    ND_TERNARY = 24,
    ND_SWITCH = 25,
    ND_CASE = 26,
    ND_DEFAULT = 27,
    ND_FLOAT_LIT = 28,
    ND_CAST = 29,
    ND_CALL_INDIRECT = 30
};

/* Limits */
#define MAX_SRC 2097152
#define MAX_MACROS 256
#define MAX_STRINGS 2048
#define MAX_STR_DATA 512
#define MAX_STRUCTS 64
#define MAX_FIELDS 32
#define MAX_GLOBALS 256
#define MAX_FUNC_SIGS 256
#define MAX_LOCALS 256
#define MAX_LOOP_DEPTH 64
#define MAX_CASES 256
#define MAX_ENUM_CONSTS 512
#define MAX_TYPE_ALIASES 128

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
        src_len++;
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
    lex_pos++;
    if (c == '\n') {
        lex_line++;
        lex_col = 1;
    } else {
        lex_col++;
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
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/* Manual is_digit check (replaces isdigit) */
int is_digit(char c) {
    return c >= '0' && c <= '9';
}

/* Manual is_alnum check (replaces isalnum) */
int is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

/* Manual is_xdigit check (replaces isxdigit) */
int is_xdigit(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

#define MAX_KW 32
char **kw_words;
int *kw_toks;
int nkw;

void kw_add(char *word, int tok) {
    kw_words[nkw] = word;
    kw_toks[nkw] = tok;
    nkw++;
}

void init_kw_table(void) {
    kw_words = (char **)malloc(MAX_KW * sizeof(char *));
    kw_toks = (int *)malloc(MAX_KW * sizeof(int));
    nkw = 0;
    kw_add("int", TOK_INT);
    kw_add("char", TOK_CHAR_KW);
    kw_add("void", TOK_VOID);
    kw_add("return", TOK_RETURN);
    kw_add("if", TOK_IF);
    kw_add("else", TOK_ELSE);
    kw_add("while", TOK_WHILE);
    kw_add("do", TOK_DO);
    kw_add("for", TOK_FOR);
    kw_add("break", TOK_BREAK);
    kw_add("continue", TOK_CONTINUE);
    kw_add("switch", TOK_SWITCH);
    kw_add("case", TOK_CASE);
    kw_add("default", TOK_DEFAULT);
    kw_add("const", TOK_CONST);
    kw_add("enum", TOK_ENUM);
    kw_add("typedef", TOK_TYPEDEF);
    kw_add("struct", TOK_STRUCT);
    kw_add("sizeof", TOK_SIZEOF);
    kw_add("unsigned", TOK_UNSIGNED);
    kw_add("signed", TOK_SIGNED);
    kw_add("short", TOK_SHORT);
    kw_add("long", TOK_LONG);
    kw_add("float", TOK_FLOAT);
    kw_add("double", TOK_DOUBLE);
}

int kw_lookup(char *s) {
    int i;
    for (i = 0; i < nkw; i++) {
        if (strcmp(s, kw_words[i]) == 0) return kw_toks[i];
    }
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
    for (i = 0; i < nmacros; i++) {
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
    if (nstrings >= MAX_STRINGS) {
        error(0, 0, "too many string literals");
    }
    id = nstrings;
    nstrings++;
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
    data_ptr += len + 1;
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
                    ni++;
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
            nmacros++;
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

    /* integer and float literals */
    if (is_digit(c)) {
        char fbuf[128];
        int flen;
        int is_flt;
        val = 0;
        flen = 0;
        is_flt = 0;
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
            t->kind = TOK_INT_LIT;
            t->int_val = val;
            return t;
        }
        /* decimal: accumulate digits into fbuf and val */
        while (is_digit(lp())) {
            fbuf[flen++] = lp();
            val = val * 10 + (la() - '0');
        }
        /* check for float: dot or exponent */
        if (lp() == '.' || lp() == 'e' || lp() == 'E') {
            is_flt = 1;
            if (lp() == '.') {
                fbuf[flen++] = la();
                while (is_digit(lp())) {
                    fbuf[flen++] = la();
                }
            }
            if (lp() == 'e' || lp() == 'E') {
                fbuf[flen++] = la();
                if (lp() == '+' || lp() == '-') {
                    fbuf[flen++] = la();
                }
                while (is_digit(lp())) {
                    fbuf[flen++] = la();
                }
            }
            /* consume optional f/F suffix */
            if (lp() == 'f' || lp() == 'F') la();
        }
        if (is_flt) {
            fbuf[flen] = '\0';
            t->text = strdupn(fbuf, flen);
            t->kind = TOK_FLOAT_LIT;
            return t;
        }
        /* consume optional L/l suffix for long */
        if (lp() == 'l' || lp() == 'L') la();
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
                    i++;
                } else if (ch == 'r') {
                    __s[i] = '\r';
                    i++;
                } else if (ch == 't') {
                    __s[i] = '\t';
                    i++;
                } else if (ch == '0') {
                    __s[i] = '\0';
                    i++;
                } else if (ch == '\\') {
                    __s[i] = '\\';
                    i++;
                } else if (ch == '"') {
                    __s[i] = '"';
                    i++;
                } else {
                    __s[i] = lp();
                    i++;
                }
                la();
            } else {
                __s[i] = la();
                i++;
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
    for (i = 0; i < nstructs; i++) {
        if (strcmp(structs_reg[i]->name, name) == 0) return structs_reg[i];
    }
    return (struct StructDef *)0;
}

int resolve_field_offset(char *field_name) {
    int i;
    int j;
    for (i = 0; i < nstructs; i++) {
        for (j = 0; j < structs_reg[i]->nfields; j++) {
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
    int gv_elem_size;     /* 1=char, 2=short, 4=int */
    int gv_is_unsigned;
    int gv_is_float;      /* 0=int, 1=float(f32), 2=double(f64) */
    char *gv_float_init;  /* float literal text for init, or NULL */
    int gv_arr_len;       /* >0 if array with brace init */
    int *gv_arr_str_ids;  /* string table IDs for {str,...} init, or NULL */
};

struct GlobalVar **globals_tbl;
int nglobals;

void init_globals(void) {
    globals_tbl = (struct GlobalVar **)malloc(MAX_GLOBALS * sizeof(void *));
    nglobals = 0;
}

int find_global(char *name) {
    int i;
    for (i = 0; i < nglobals; i++) {
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
    int ret_is_float;     /* 0=i32, 1=float(f32), 2=double(f64) */
    int nparam;
    int *param_is_float;  /* array: 0=i32, 1=float, 2=double per param */
};

struct FuncSig **func_sigs;
int nfunc_sigs;

void init_func_sigs(void) {
    func_sigs = (struct FuncSig **)malloc(MAX_FUNC_SIGS * sizeof(void *));
    nfunc_sigs = 0;
}

int func_is_void(char *name) {
    int i;
    for (i = 0; i < nfunc_sigs; i++) {
        if (strcmp(func_sigs[i]->name, name) == 0) return func_sigs[i]->is_void;
    }
    return 0;
}

int func_ret_is_float(char *name) {
    int i;
    for (i = 0; i < nfunc_sigs; i++) {
        if (strcmp(func_sigs[i]->name, name) == 0) return func_sigs[i]->ret_is_float;
    }
    return 0;
}

int func_param_is_float(char *name, int idx) {
    int i;
    for (i = 0; i < nfunc_sigs; i++) {
        if (strcmp(func_sigs[i]->name, name) == 0) {
            if (idx < func_sigs[i]->nparam) return func_sigs[i]->param_is_float[idx];
            return 0;
        }
    }
    return 0;
}

/* ================================================================
 * Function Pointer Registry
 * ================================================================ */

struct FnPtrVar {
    char *name;
    int nparams;
    int is_void;  /* 0=returns i32, 1=returns void */
};

#define MAX_FNPTR_VARS 512
#define MAX_FN_TABLE   512

struct FnPtrVar **fnptr_vars;
int nfnptr_vars;

char **fn_table_names;
int fn_table_count;

void init_fnptr_registry(void) {
    fnptr_vars = (struct FnPtrVar **)malloc(MAX_FNPTR_VARS * sizeof(void *));
    nfnptr_vars = 0;
    fn_table_names = (char **)malloc(MAX_FN_TABLE * sizeof(void *));
    fn_table_count = 0;
}

void register_fnptr_var(char *name, int nparams, int is_void) {
    if (nfnptr_vars >= MAX_FNPTR_VARS) return;
    fnptr_vars[nfnptr_vars] = (struct FnPtrVar *)malloc(sizeof(struct FnPtrVar));
    fnptr_vars[nfnptr_vars]->name = strdupn(name, 127);
    fnptr_vars[nfnptr_vars]->nparams = nparams;
    fnptr_vars[nfnptr_vars]->is_void = is_void;
    nfnptr_vars++;
}

struct FnPtrVar *find_fnptr_var(char *name) {
    int i;
    for (i = 0; i < nfnptr_vars; i++) {
        if (strcmp(fnptr_vars[i]->name, name) == 0) return fnptr_vars[i];
    }
    return (struct FnPtrVar *)0;
}

void fn_table_add(char *name) {
    int i;
    for (i = 0; i < fn_table_count; i++) {
        if (strcmp(fn_table_names[i], name) == 0) return;
    }
    if (fn_table_count < MAX_FN_TABLE) {
        fn_table_names[fn_table_count++] = name;
    }
}

int fn_table_idx(char *name) {
    int i;
    for (i = 0; i < fn_table_count; i++) {
        if (strcmp(fn_table_names[i], name) == 0) return i;
    }
    return -1;
}

int is_known_func(char *name) {
    int i;
    for (i = 0; i < nfunc_sigs; i++) {
        if (strcmp(func_sigs[i]->name, name) == 0) return 1;
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
    for (i = 0; i < nenum_consts; i++) {
        if (strcmp(enum_consts[i]->name, name) == 0) return i;
    }
    return -1;
}

struct TypeAlias {
    char *alias;
    int resolved_kind; /* 0=int, 1=void, 2=char */
    int is_ptr;
    int is_fnptr;      /* 1 if typedef for function pointer type */
    int fnptr_nparams;
    int fnptr_is_void;
};

struct TypeAlias **type_aliases;
int ntype_aliases;
int last_type_is_ptr;
int last_type_is_unsigned;
int last_type_elem_size;  /* 1=char, 2=short, 4=int/long */
int last_type_is_float;   /* 0=not float, 1=float(f32), 2=double(f64) */
int last_type_is_fnptr;   /* 1 if resolved from fnptr typedef */
int last_type_fnptr_nparams;
int last_type_fnptr_is_void;

void init_type_aliases(void) {
    type_aliases = (struct TypeAlias **)malloc(MAX_TYPE_ALIASES * sizeof(void *));
    ntype_aliases = 0;
    last_type_is_ptr = 0;
    last_type_is_float = 0;
}

int find_type_alias(char *name) {
    int i;
    for (i = 0; i < ntype_aliases; i++) {
        if (strcmp(type_aliases[i]->alias, name) == 0) return i;
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
    int ival3;
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
        for (i = 0; i < l->count; i++) {
            new_items[i] = l->items[i];
        }
        if (l->items != (struct Node **)0) {
            free(l->items);
        }
        l->items = new_items;
        l->cap = new_cap;
    }
    l->items[l->count] = n;
    l->count++;
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
    if (at(TOK_UNSIGNED)) return 1;
    if (at(TOK_SIGNED)) return 1;
    if (at(TOK_SHORT)) return 1;
    if (at(TOK_LONG)) return 1;
    if (at(TOK_FLOAT)) return 1;
    if (at(TOK_DOUBLE)) return 1;
    if (at(TOK_IDENT) && find_type_alias(cur->text) >= 0) return 1;
    return 0;
}

/* Forward declarations */
struct Node *parse_expr(void);
struct Node *parse_expr_bp(int min_bp);
struct Node *parse_stmt(void);
struct Node *parse_block(void);

/* Precedence climbing helpers */
int prefix_bp(int op) {
    switch (op) {
    case TOK_MINUS:
    case TOK_BANG:
    case TOK_STAR:
    case TOK_AMP:
    case TOK_TILDE:
    case TOK_PLUS_PLUS:
    case TOK_MINUS_MINUS:
        return 25;
    default:
        return -1;
    }
}

int last_rbp;

int infix_bp(int op) {
    switch (op) {
    case TOK_EQ:
    case TOK_PLUS_EQ:
    case TOK_MINUS_EQ:
    case TOK_PIPE_EQ:
    case TOK_AMP_EQ:
    case TOK_CARET_EQ:
    case TOK_LSHIFT_EQ:
    case TOK_RSHIFT_EQ:
        last_rbp = 1;
        return 2;
    case TOK_PIPE_PIPE:
        last_rbp = 5;
        return 4;
    case TOK_AMP_AMP:
        last_rbp = 7;
        return 6;
    case TOK_PIPE:
        last_rbp = 9;
        return 8;
    case TOK_CARET:
        last_rbp = 10;
        return 9;
    case TOK_AMP:
        last_rbp = 11;
        return 10;
    case TOK_EQ_EQ:
    case TOK_BANG_EQ:
        last_rbp = 13;
        return 12;
    case TOK_LT:
    case TOK_GT:
    case TOK_LT_EQ:
    case TOK_GT_EQ:
        last_rbp = 15;
        return 14;
    case TOK_LSHIFT:
    case TOK_RSHIFT:
        last_rbp = 17;
        return 16;
    case TOK_PLUS:
    case TOK_MINUS:
        last_rbp = 19;
        return 18;
    case TOK_STAR:
    case TOK_SLASH:
    case TOK_PERCENT:
        last_rbp = 21;
        return 20;
    case TOK_DOT:
    case TOK_ARROW:
        last_rbp = 30;
        return 29;
    default:
        return -1;
    }
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
    if (at(TOK_FLOAT_LIT)) {
        n = node_new(ND_FLOAT_LIT, cur->line, cur->col);
        n->sval = strdupn(cur->text, 127);
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
            } else if (at(TOK_UNSIGNED) || at(TOK_SIGNED) || at(TOK_SHORT) || at(TOK_LONG) ||
                       at(TOK_INT) || at(TOK_CHAR_KW) || at(TOK_VOID) ||
                       at(TOK_FLOAT) || at(TOK_DOUBLE)) {
                {
                    int sz_esz;
                    sz_esz = 4;
                    /* consume modifiers */
                    for (;;) {
                        if (at(TOK_UNSIGNED) || at(TOK_SIGNED)) {
                            advance_tok();
                        } else if (at(TOK_SHORT)) {
                            sz_esz = 2;
                            advance_tok();
                        } else if (at(TOK_LONG)) {
                            sz_esz = 4;
                            advance_tok();
                        } else {
                            break;
                        }
                    }
                    if (at(TOK_CHAR_KW)) {
                        sz_esz = 1;
                        n->sval = strdupn("char", 127);
                        advance_tok();
                    } else if (at(TOK_INT)) {
                        /* elem size already set by modifiers */
                        n->sval = (char *)0;
                        advance_tok();
                    } else if (at(TOK_VOID)) {
                        n->sval = strdupn("void", 127);
                        advance_tok();
                    } else if (at(TOK_FLOAT)) {
                        sz_esz = 4;
                        n->sval = (char *)0;
                        advance_tok();
                    } else if (at(TOK_DOUBLE)) {
                        sz_esz = 8;
                        n->sval = (char *)0;
                        advance_tok();
                    } else {
                        /* bare modifiers: unsigned, short, long */
                        n->sval = (char *)0;
                    }
                    n->ival2 = sz_esz;
                    while (at(TOK_CONST)) advance_tok();
                    while (at(TOK_STAR)) { is_ptr = 1; advance_tok(); }
                }
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
            struct FnPtrVar *fpv;
            fpv = find_fnptr_var(n->sval);
            advance_tok();
            if (fpv != (struct FnPtrVar *)0) {
                /* indirect call through function pointer variable */
                c = node_new(ND_CALL_INDIRECT, n->nline, n->ncol);
                c->c0 = n; /* callee expression (ND_IDENT for the fnptr var) */
                c->ival = fpv->nparams;
                c->ival3 = fpv->is_void;
            } else {
                c = node_new(ND_CALL, n->nline, n->ncol);
                c->sval = n->sval;
            }
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
        /* function name used as a value (not called) → table index */
        if (is_known_func(n->sval)) {
            fn_table_add(n->sval);
            n->kind = ND_INT_LIT;
            n->ival = fn_table_idx(n->sval);
        }
        return n;
    }
    if (at(TOK_LPAREN)) {
        advance_tok();
        /* type cast */
        if (is_type_token()) {
            int cast_to_float;
            int cast_to_int;
            cast_to_float = 0;
            cast_to_int = 0;
            while (at(TOK_CONST)) advance_tok();
            if (at(TOK_FLOAT)) {
                cast_to_float = 1;
                advance_tok();
            } else if (at(TOK_DOUBLE)) {
                cast_to_float = 2;
                advance_tok();
            } else if (at(TOK_STRUCT)) {
                advance_tok();
                if (at(TOK_IDENT)) advance_tok();
            } else {
                /* int, char, void, unsigned, etc — cast to int */
                if (at(TOK_INT) || at(TOK_CHAR_KW) || at(TOK_VOID) ||
                    at(TOK_UNSIGNED) || at(TOK_SIGNED) || at(TOK_SHORT) || at(TOK_LONG)) {
                    cast_to_int = 1;
                }
                advance_tok();
                /* consume trailing int after short/long/unsigned/signed */
                if (at(TOK_INT)) advance_tok();
            }
            while (at(TOK_STAR)) { cast_to_float = 0; cast_to_int = 0; advance_tok(); }
            expect(TOK_RPAREN, "expected ')' after cast type");
            if (cast_to_float) {
                n = node_new(ND_CAST, cur->line, cur->col);
                n->ival = cast_to_float; /* 1=float, 2=double */
                n->c0 = parse_expr_bp(25);
                return n;
            }
            if (cast_to_int) {
                /* ND_CAST to int — only matters when expr is float */
                n = node_new(ND_CAST, cur->line, cur->col);
                n->ival = 0; /* 0=int */
                n->c0 = parse_expr_bp(25);
                return n;
            }
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
    struct Node *blk;
    struct Node *size_node;
    struct Node *idx_lit;
    struct Node *id_node;
    struct Node *sub_node;
    struct Node *asgn_node;
    struct Node *estmt;
    struct NList *blk_stmts;
    struct NList *init_elems;
    int line;
    int col;
    int vd_elem_size;
    int vd_is_unsigned;
    int vd_is_float;
    int vd_is_void;
    int is_ptr;
    int ptr_depth_vd;
    int arr_size;
    int ei;
    int tai_vd;
    int had_mod_vd;

    line = cur->line;
    col = cur->col;
    vd_elem_size = 4;
    vd_is_unsigned = 0;
    vd_is_float = 0;
    vd_is_void = 0;
    is_ptr = 0;
    ptr_depth_vd = 0;
    arr_size = 0;
    tai_vd = -1;
    had_mod_vd = 0;
    blk = (struct Node *)0;
    size_node = (struct Node *)0;
    idx_lit = (struct Node *)0;
    id_node = (struct Node *)0;
    sub_node = (struct Node *)0;
    asgn_node = (struct Node *)0;
    estmt = (struct Node *)0;
    blk_stmts = (struct NList *)0;
    init_elems = (struct NList *)0;
    ei = 0;
    while (at(TOK_CONST)) advance_tok();
    if (at(TOK_STRUCT)) {
        advance_tok();
        advance_tok();
    } else if (at(TOK_ENUM)) {
        advance_tok();
        if (at(TOK_IDENT)) advance_tok();
    } else if (at(TOK_FLOAT)) {
        vd_is_float = 1;
        vd_elem_size = 4;
        advance_tok();
    } else if (at(TOK_DOUBLE)) {
        vd_is_float = 2;
        vd_elem_size = 8;
        advance_tok();
    } else {
        /* consume sign/size modifiers */
        for (;;) {
            if (at(TOK_UNSIGNED)) {
                vd_is_unsigned = 1;
                had_mod_vd = 1;
                advance_tok();
            } else if (at(TOK_SIGNED)) {
                vd_is_unsigned = 0;
                had_mod_vd = 1;
                advance_tok();
            } else if (at(TOK_SHORT)) {
                vd_elem_size = 2;
                had_mod_vd = 1;
                advance_tok();
            } else if (at(TOK_LONG)) {
                vd_elem_size = 4;
                had_mod_vd = 1;
                advance_tok();
            } else {
                break;
            }
        }
        if (at(TOK_CHAR_KW)) {
            vd_elem_size = 1;
            advance_tok();
        } else if (at(TOK_INT)) {
            advance_tok();
        } else if (at(TOK_VOID)) {
            vd_is_void = 1;
            advance_tok();
        } else if (at(TOK_IDENT)) {
            tai_vd = find_type_alias(cur->text);
            if (tai_vd >= 0 && type_aliases[tai_vd]->resolved_kind == 2) {
                vd_elem_size = 1;
            }
            if (tai_vd >= 0 && type_aliases[tai_vd]->is_ptr) {
                is_ptr = 1;
            }
            if (tai_vd >= 0 && type_aliases[tai_vd]->is_fnptr) {
                /* fnptr typedef: next token is the variable name */
                advance_tok(); /* consume typedef name */
                if (!at(TOK_IDENT)) error(cur->line, cur->col, "expected variable name");
                n = node_new(ND_VAR_DECL, line, col);
                n->sval = strdupn(cur->text, 127);
                n->ival2 = 4;
                n->ival3 = 0;
                advance_tok();
                register_fnptr_var(n->sval, type_aliases[tai_vd]->fnptr_nparams, type_aliases[tai_vd]->fnptr_is_void);
                if (at(TOK_EQ)) {
                    advance_tok();
                    n->c0 = parse_expr();
                }
                expect(TOK_SEMI, "expected ';' after declaration");
                return n;
            }
            if (tai_vd >= 0 || !had_mod_vd) {
                advance_tok();
            }
        }
    }
    while (at(TOK_STAR)) { is_ptr = 1; ptr_depth_vd++; vd_is_float = 0; advance_tok(); }
    if (last_type_is_ptr) { is_ptr = 1; ptr_depth_vd++; }
    if (ptr_depth_vd >= 2) vd_elem_size = 4;
    /* function pointer: type (*name)(params) or type (*name[N])(params) */
    if (at(TOK_LPAREN)) {
        struct Node *fp_sz;
        int fp_nparams;
        int fp_arr;
        fp_nparams = 0;
        fp_arr = 0;
        fp_sz = (struct Node *)0;
        advance_tok(); /* consume '(' */
        while (at(TOK_STAR)) advance_tok(); /* consume '*' */
        if (!at(TOK_IDENT)) error(cur->line, cur->col, "expected variable name in function pointer");
        n = node_new(ND_VAR_DECL, line, col);
        n->sval = strdupn(cur->text, 127);
        n->ival2 = 4; /* pointer-sized */
        n->ival3 = 0;
        advance_tok();
        /* optional array: (*name[N]) */
        if (at(TOK_LBRACKET)) {
            advance_tok();
            fp_sz = parse_expr();
            fp_arr = fp_sz->ival;
            n->ival = fp_arr;
            expect(TOK_RBRACKET, "expected ']'");
        }
        expect(TOK_RPAREN, "expected ')' after function pointer name");
        /* parse parameter type list */
        expect(TOK_LPAREN, "expected '(' for function pointer parameters");
        while (!at(TOK_RPAREN) && !at(TOK_EOF)) {
            while (at(TOK_CONST)) advance_tok();
            if (at(TOK_VOID)) {
                advance_tok();
                /* void with no stars = no params; void* = one param */
                if (at(TOK_STAR)) {
                    while (at(TOK_STAR)) advance_tok();
                    if (at(TOK_IDENT)) advance_tok();
                    fp_nparams++;
                }
            } else {
                if (at(TOK_STRUCT)) { advance_tok(); if (at(TOK_IDENT)) advance_tok(); }
                else if (at(TOK_UNSIGNED) || at(TOK_SIGNED) || at(TOK_SHORT) || at(TOK_LONG)) {
                    advance_tok();
                    if (at(TOK_INT)) advance_tok();
                }
                if (at(TOK_INT) || at(TOK_CHAR_KW) || at(TOK_FLOAT) || at(TOK_DOUBLE) || at(TOK_IDENT)) {
                    advance_tok();
                }
                while (at(TOK_STAR)) advance_tok();
                if (at(TOK_IDENT)) advance_tok();
                fp_nparams++;
            }
            if (at(TOK_COMMA)) advance_tok();
        }
        expect(TOK_RPAREN, "expected ')' after function pointer parameters");
        register_fnptr_var(n->sval, fp_nparams, vd_is_void);
        /* optional initializer */
        if (at(TOK_EQ)) {
            advance_tok();
            n->c0 = parse_expr();
        }
        expect(TOK_SEMI, "expected ';' after function pointer declaration");
        return n;
    }
    if (!at(TOK_IDENT)) error(cur->line, cur->col, "expected variable name");
    n = node_new(ND_VAR_DECL, line, col);
    n->sval = strdupn(cur->text, 127);
    n->ival2 = vd_elem_size;
    n->ival3 = vd_is_unsigned | (vd_is_float << 4);
    advance_tok();
    /* check for array size: int name[N] */
    if (at(TOK_LBRACKET)) {
        advance_tok(); /* consume '[' */
        size_node = parse_expr();
        arr_size = size_node->ival;
        n->ival = arr_size;
        expect(TOK_RBRACKET, "expected ']'");
    }
    /* brace initializer: int name[N] = {e0, e1, ...} */
    if (arr_size > 0 && at(TOK_EQ)) {
        advance_tok(); /* consume '=' */
        expect(TOK_LBRACE, "expected '{'");
        init_elems = (struct NList *)malloc(sizeof(struct NList));
        init_elems->items = (struct Node **)0;
        init_elems->count = 0;
        init_elems->cap = 0;
        while (!at(TOK_RBRACE) && !at(TOK_EOF)) {
            nlist_push(init_elems, parse_expr());
            if (at(TOK_COMMA)) advance_tok();
        }
        expect(TOK_RBRACE, "expected '}'");
        expect(TOK_SEMI, "expected ';'");
        /* build ND_BLOCK: [var_decl] + [assignment stmts per element] */
        blk_stmts = (struct NList *)malloc(sizeof(struct NList));
        blk_stmts->items = (struct Node **)0;
        blk_stmts->count = 0;
        blk_stmts->cap = 0;
        nlist_push(blk_stmts, n);
        for (ei = 0; ei < init_elems->count; ei++) {
            idx_lit = node_new(ND_INT_LIT, line, col);
            idx_lit->ival = ei;
            id_node = node_new(ND_IDENT, line, col);
            id_node->sval = n->sval;
            sub_node = node_new(ND_SUBSCRIPT, line, col);
            sub_node->c0 = id_node;
            sub_node->c1 = idx_lit;
            asgn_node = node_new(ND_ASSIGN, line, col);
            asgn_node->ival = TOK_EQ;
            asgn_node->c0 = sub_node;
            asgn_node->c1 = init_elems->items[ei];
            estmt = node_new(ND_EXPR_STMT, line, col);
            estmt->c0 = asgn_node;
            nlist_push(blk_stmts, estmt);
        }
        blk = node_new(ND_BLOCK, line, col);
        blk->list = blk_stmts->items;
        blk->ival2 = blk_stmts->count;
        return blk;
    }
    /* scalar initializer: int name = expr */
    if (arr_size == 0 && at(TOK_EQ)) {
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
        int eci_case;
        struct Node *cn;
        advance_tok();
        eci_case = -1;
        if (at(TOK_IDENT)) {
            eci_case = find_enum_const(cur->text);
        }
        if (eci_case >= 0) {
            cv = enum_consts[eci_case]->val;
            advance_tok();
        } else {
            if (!at(TOK_INT_LIT) && !at(TOK_CHAR_LIT)) {
                error(cur->line, cur->col, "expected integer constant in case");
            }
            cv = cur->int_val;
            advance_tok();
        }
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
    int taidx;
    int had_modifier;
    last_type_is_ptr = 0;
    last_type_is_unsigned = 0;
    last_type_elem_size = 4;
    last_type_is_float = 0;
    last_type_is_fnptr = 0;
    last_type_fnptr_nparams = 0;
    last_type_fnptr_is_void = 0;
    while (at(TOK_CONST)) advance_tok();
    if (at(TOK_IDENT)) {
        taidx = find_type_alias(cur->text);
        if (taidx >= 0) {
            last_type_is_ptr = type_aliases[taidx]->is_ptr;
            if (type_aliases[taidx]->resolved_kind == 2) last_type_elem_size = 1;
            if (type_aliases[taidx]->is_fnptr) {
                last_type_is_fnptr = 1;
                last_type_fnptr_nparams = type_aliases[taidx]->fnptr_nparams;
                last_type_fnptr_is_void = type_aliases[taidx]->fnptr_is_void;
            }
            advance_tok();
            return type_aliases[taidx]->resolved_kind;
        }
    }
    if (at(TOK_FLOAT)) {
        last_type_is_float = 1;
        last_type_elem_size = 4;
        advance_tok();
        return 0;
    }
    if (at(TOK_DOUBLE)) {
        last_type_is_float = 2;
        last_type_elem_size = 8;
        advance_tok();
        return 0;
    }
    /* consume sign/size modifiers */
    had_modifier = 0;
    for (;;) {
        if (at(TOK_UNSIGNED)) {
            last_type_is_unsigned = 1;
            had_modifier = 1;
            advance_tok();
        } else if (at(TOK_SIGNED)) {
            last_type_is_unsigned = 0;
            had_modifier = 1;
            advance_tok();
        } else if (at(TOK_SHORT)) {
            last_type_elem_size = 2;
            had_modifier = 1;
            advance_tok();
            if (at(TOK_INT)) advance_tok();
            if (had_modifier) return 0;
        } else if (at(TOK_LONG)) {
            last_type_elem_size = 4;
            had_modifier = 1;
            advance_tok();
            if (at(TOK_INT)) advance_tok();
            if (had_modifier) return 0;
        } else {
            break;
        }
    }
    if (at(TOK_INT)) {
        advance_tok();
        return 0;
    }
    if (at(TOK_VOID)) {
        advance_tok();
        return 1;
    }
    if (at(TOK_CHAR_KW)) {
        last_type_elem_size = 1;
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
    if (had_modifier) return 0;
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
    int ret_float;
    int sig_idx;
    int pi;

    line = cur->line;
    col = cur->col;
    ret = parse_type();
    ret_float = last_type_is_float;
    while (at(TOK_STAR)) { ret_float = 0; advance_tok(); }
    if (!at(TOK_IDENT)) error(cur->line, cur->col, "expected function name");
    n = node_new(ND_FUNC, line, col);
    n->sval = strdupn(cur->text, 127);
    n->ival = ret;
    n->ival3 = ret_float; /* store return float type in ival3 */

    /* register in function signature table */
    sig_idx = -1;
    if (nfunc_sigs < MAX_FUNC_SIGS) {
        sig_idx = nfunc_sigs;
        func_sigs[nfunc_sigs] = (struct FuncSig *)malloc(sizeof(struct FuncSig));
        func_sigs[nfunc_sigs]->name = strdupn(cur->text, 127);
        if (ret == 1) {
            func_sigs[nfunc_sigs]->is_void = 1;
        } else {
            func_sigs[nfunc_sigs]->is_void = 0;
        }
        func_sigs[nfunc_sigs]->ret_is_float = ret_float;
        func_sigs[nfunc_sigs]->nparam = 0;
        func_sigs[nfunc_sigs]->param_is_float = (int *)malloc(MAX_LOCALS * sizeof(int));
        nfunc_sigs++;
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
            while (at(TOK_STAR)) { last_type_is_float = 0; advance_tok(); }
            /* function pointer parameter: type (*name)(params) */
            if (at(TOK_LPAREN)) {
                int fparam_np;
                int fparam_void;
                fparam_np = 0;
                fparam_void = (pty == 1) ? 1 : 0; /* 1 = void return */
                advance_tok(); /* consume '(' */
                while (at(TOK_STAR)) advance_tok();
                if (!at(TOK_IDENT)) error(cur->line, cur->col, "expected fnptr param name");
                pn = node_new(ND_IDENT, cur->line, cur->col);
                pn->sval = strdupn(cur->text, 127);
                pn->ival2 = 4;
                pn->ival3 = 0;
                advance_tok();
                expect(TOK_RPAREN, "expected ')' in fnptr param");
                expect(TOK_LPAREN, "expected '(' for fnptr param types");
                while (!at(TOK_RPAREN) && !at(TOK_EOF)) {
                    while (at(TOK_CONST)) advance_tok();
                    if (at(TOK_VOID)) { advance_tok(); if (at(TOK_STAR)) { while (at(TOK_STAR)) advance_tok(); fparam_np++; } }
                    else {
                        if (at(TOK_STRUCT)) { advance_tok(); if (at(TOK_IDENT)) advance_tok(); }
                        else if (at(TOK_UNSIGNED)||at(TOK_SIGNED)||at(TOK_SHORT)||at(TOK_LONG)) { advance_tok(); if (at(TOK_INT)) advance_tok(); }
                        if (at(TOK_INT)||at(TOK_CHAR_KW)||at(TOK_FLOAT)||at(TOK_DOUBLE)||at(TOK_IDENT)) advance_tok();
                        while (at(TOK_STAR)) advance_tok();
                        if (at(TOK_IDENT)) advance_tok();
                        fparam_np++;
                    }
                    if (at(TOK_COMMA)) advance_tok();
                }
                expect(TOK_RPAREN, "expected ')' after fnptr param types");
                register_fnptr_var(pn->sval, fparam_np, fparam_void);
                if (sig_idx >= 0) {
                    func_sigs[sig_idx]->param_is_float[func_sigs[sig_idx]->nparam] = 0;
                    func_sigs[sig_idx]->nparam++;
                }
                nlist_push(params, pn);
            } else if (at(TOK_IDENT)) {
                pn = node_new(ND_IDENT, cur->line, cur->col);
                pn->sval = strdupn(cur->text, 127);
                pn->ival2 = last_type_is_fnptr ? 4 : last_type_elem_size;
                pn->ival3 = last_type_is_unsigned | (last_type_is_float << 4);
                if (last_type_is_fnptr) {
                    register_fnptr_var(pn->sval, last_type_fnptr_nparams, last_type_fnptr_is_void);
                }
                if (sig_idx >= 0) {
                    func_sigs[sig_idx]->param_is_float[func_sigs[sig_idx]->nparam] = last_type_is_float;
                    func_sigs[sig_idx]->nparam++;
                }
                nlist_push(params, pn);
                advance_tok();
            }
            while (at(TOK_COMMA)) {
                advance_tok();
                pty = parse_type();
                while (at(TOK_STAR)) { last_type_is_float = 0; advance_tok(); }
                /* function pointer parameter in subsequent position */
                if (at(TOK_LPAREN)) {
                    int fparam_np2;
                    int fparam_void2;
                    fparam_np2 = 0;
                    fparam_void2 = (pty == 1) ? 1 : 0;
                    advance_tok();
                    while (at(TOK_STAR)) advance_tok();
                    if (!at(TOK_IDENT)) error(cur->line, cur->col, "expected fnptr param name");
                    pn = node_new(ND_IDENT, cur->line, cur->col);
                    pn->sval = strdupn(cur->text, 127);
                    pn->ival2 = 4;
                    pn->ival3 = 0;
                    advance_tok();
                    expect(TOK_RPAREN, "expected ')' in fnptr param");
                    expect(TOK_LPAREN, "expected '(' for fnptr param types");
                    while (!at(TOK_RPAREN) && !at(TOK_EOF)) {
                        while (at(TOK_CONST)) advance_tok();
                        if (at(TOK_VOID)) { advance_tok(); if (at(TOK_STAR)) { while (at(TOK_STAR)) advance_tok(); fparam_np2++; } }
                        else {
                            if (at(TOK_STRUCT)) { advance_tok(); if (at(TOK_IDENT)) advance_tok(); }
                            else if (at(TOK_UNSIGNED)||at(TOK_SIGNED)||at(TOK_SHORT)||at(TOK_LONG)) { advance_tok(); if (at(TOK_INT)) advance_tok(); }
                            if (at(TOK_INT)||at(TOK_CHAR_KW)||at(TOK_FLOAT)||at(TOK_DOUBLE)||at(TOK_IDENT)) advance_tok();
                            while (at(TOK_STAR)) advance_tok();
                            if (at(TOK_IDENT)) advance_tok();
                            fparam_np2++;
                        }
                        if (at(TOK_COMMA)) advance_tok();
                    }
                    expect(TOK_RPAREN, "expected ')' after fnptr param types");
                    register_fnptr_var(pn->sval, fparam_np2, fparam_void2);
                    if (sig_idx >= 0) {
                        func_sigs[sig_idx]->param_is_float[func_sigs[sig_idx]->nparam] = 0;
                        func_sigs[sig_idx]->nparam++;
                    }
                    nlist_push(params, pn);
                } else if (at(TOK_IDENT)) {
                    pn = node_new(ND_IDENT, cur->line, cur->col);
                    pn->sval = strdupn(cur->text, 127);
                    pn->ival2 = last_type_is_fnptr ? 4 : last_type_elem_size;
                    pn->ival3 = last_type_is_unsigned | (last_type_is_float << 4);
                    if (last_type_is_fnptr) {
                        register_fnptr_var(pn->sval, last_type_fnptr_nparams, last_type_fnptr_is_void);
                    }
                    if (sig_idx >= 0) {
                        func_sigs[sig_idx]->param_is_float[func_sigs[sig_idx]->nparam] = last_type_is_float;
                        func_sigs[sig_idx]->nparam++;
                    }
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
    nstructs++;
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
        sd->nfields++;
        offset += 4;
        advance_tok();
        expect(TOK_SEMI, "expected ';' after field");
    }
    expect(TOK_RBRACE, "expected '}'");
    sd->size = offset;
    expect(TOK_SEMI, "expected ';' after struct definition");
}

void parse_global_var(void) {
    int gv_es;
    int gv_uns;
    int gv_flt;
    int is_ptr;
    int ptr_depth;
    int neg;
    int tai_gv;
    int had_mod_gv;

    gv_es = 4;
    gv_uns = 0;
    gv_flt = 0;
    is_ptr = 0;
    ptr_depth = 0;
    tai_gv = -1;
    had_mod_gv = 0;
    while (at(TOK_CONST)) advance_tok();
    if (at(TOK_STRUCT) || at(TOK_ENUM)) {
        advance_tok();
        if (at(TOK_IDENT)) advance_tok();
    } else if (at(TOK_FLOAT)) {
        gv_flt = 1;
        gv_es = 4;
        advance_tok();
    } else if (at(TOK_DOUBLE)) {
        gv_flt = 2;
        gv_es = 8;
        advance_tok();
    } else {
        for (;;) {
            if (at(TOK_UNSIGNED)) {
                gv_uns = 1;
                had_mod_gv = 1;
                advance_tok();
            } else if (at(TOK_SIGNED)) {
                gv_uns = 0;
                had_mod_gv = 1;
                advance_tok();
            } else if (at(TOK_SHORT)) {
                gv_es = 2;
                had_mod_gv = 1;
                advance_tok();
            } else if (at(TOK_LONG)) {
                gv_es = 4;
                had_mod_gv = 1;
                advance_tok();
            } else {
                break;
            }
        }
        if (at(TOK_CHAR_KW)) {
            gv_es = 1;
            advance_tok();
        } else if (at(TOK_INT)) {
            advance_tok();
        } else if (at(TOK_VOID)) {
            advance_tok();
        } else if (at(TOK_IDENT)) {
            tai_gv = find_type_alias(cur->text);
            if (tai_gv >= 0 && type_aliases[tai_gv]->resolved_kind == 2) {
                gv_es = 1;
            }
            if (tai_gv >= 0 && type_aliases[tai_gv]->is_ptr) {
                is_ptr = 1;
            }
            if (tai_gv >= 0 || !had_mod_gv) {
                advance_tok();
            }
        }
    }
    while (at(TOK_STAR)) { is_ptr = 1; ptr_depth++; gv_flt = 0; advance_tok(); }
    if (last_type_is_ptr) { is_ptr = 1; ptr_depth++; }
    /* double+ pointer: subscript element is pointer-sized */
    if (ptr_depth >= 2) gv_es = 4;
    /* global function pointer: type (*name)(params) */
    if (at(TOK_LPAREN)) {
        int gfp_nparams;
        int gfp_is_void;
        char *gfp_name;
        gfp_nparams = 0;
        gfp_is_void = 0;
        advance_tok(); /* consume '(' */
        while (at(TOK_STAR)) advance_tok();
        if (!at(TOK_IDENT)) error(cur->line, cur->col, "expected variable name in function pointer");
        gfp_name = strdupn(cur->text, 127);
        advance_tok();
        expect(TOK_RPAREN, "expected ')' after function pointer name");
        expect(TOK_LPAREN, "expected '(' for function pointer parameters");
        while (!at(TOK_RPAREN) && !at(TOK_EOF)) {
            while (at(TOK_CONST)) advance_tok();
            if (at(TOK_VOID)) {
                advance_tok();
                if (at(TOK_STAR)) { while (at(TOK_STAR)) advance_tok(); if (at(TOK_IDENT)) advance_tok(); gfp_nparams++; }
            } else {
                if (at(TOK_STRUCT)) { advance_tok(); if (at(TOK_IDENT)) advance_tok(); }
                else if (at(TOK_UNSIGNED) || at(TOK_SIGNED) || at(TOK_SHORT) || at(TOK_LONG)) {
                    advance_tok(); if (at(TOK_INT)) advance_tok();
                }
                if (at(TOK_INT) || at(TOK_CHAR_KW) || at(TOK_FLOAT) || at(TOK_DOUBLE) || at(TOK_IDENT)) advance_tok();
                while (at(TOK_STAR)) advance_tok();
                if (at(TOK_IDENT)) advance_tok();
                gfp_nparams++;
            }
            if (at(TOK_COMMA)) advance_tok();
        }
        expect(TOK_RPAREN, "expected ')' after function pointer parameters");
        /* detect void return */
        if (at(TOK_VOID)) gfp_is_void = 1;
        register_fnptr_var(gfp_name, gfp_nparams, gfp_is_void);
        /* register as global with i32 value */
        if (nglobals >= MAX_GLOBALS) error(cur->line, cur->col, "too many globals");
        globals_tbl[nglobals] = (struct GlobalVar *)malloc(sizeof(struct GlobalVar));
        globals_tbl[nglobals]->name = gfp_name;
        globals_tbl[nglobals]->init_val = 0;
        globals_tbl[nglobals]->gv_elem_size = 4;
        globals_tbl[nglobals]->gv_is_unsigned = 0;
        globals_tbl[nglobals]->gv_is_float = 0;
        globals_tbl[nglobals]->gv_float_init = (char *)0;
        globals_tbl[nglobals]->gv_arr_len = 0;
        globals_tbl[nglobals]->gv_arr_str_ids = (int *)0;
        if (at(TOK_EQ)) {
            advance_tok();
            if (at(TOK_IDENT)) {
                fn_table_add(cur->text);
                globals_tbl[nglobals]->init_val = fn_table_idx(cur->text);
                advance_tok();
            } else if (at(TOK_INT_LIT)) {
                globals_tbl[nglobals]->init_val = cur->int_val;
                advance_tok();
            }
        }
        nglobals++;
        expect(TOK_SEMI, "expected ';' after global function pointer declaration");
        return;
    }
    if (!at(TOK_IDENT)) error(cur->line, cur->col, "expected variable name");
    if (nglobals >= MAX_GLOBALS) error(cur->line, cur->col, "too many globals");
    globals_tbl[nglobals] = (struct GlobalVar *)malloc(sizeof(struct GlobalVar));
    globals_tbl[nglobals]->name = strdupn(cur->text, 127);
    globals_tbl[nglobals]->init_val = 0;
    globals_tbl[nglobals]->gv_elem_size = gv_es;
    globals_tbl[nglobals]->gv_is_unsigned = gv_uns;
    globals_tbl[nglobals]->gv_is_float = gv_flt;
    globals_tbl[nglobals]->gv_float_init = (char *)0;
    globals_tbl[nglobals]->gv_arr_len = 0;
    globals_tbl[nglobals]->gv_arr_str_ids = (int *)0;
    advance_tok();
    /* array: name[] = { ... } or name[N] = { ... } */
    if (at(TOK_LBRACKET)) {
        int ga_size;
        ga_size = 0;
        advance_tok(); /* consume '[' */
        if (at(TOK_INT_LIT)) {
            ga_size = cur->int_val;
            advance_tok();
        }
        expect(TOK_RBRACKET, "expected ']'");
        if (is_ptr) globals_tbl[nglobals]->gv_elem_size = 4;
        if (at(TOK_EQ)) {
            advance_tok();
            expect(TOK_LBRACE, "expected '{'");
            {
                int *sa_ids;
                int sa_count;
                int sa_cap;
                sa_count = 0;
                sa_cap = 64;
                sa_ids = (int *)malloc(sa_cap * sizeof(int));
                while (!at(TOK_RBRACE) && !at(TOK_EOF)) {
                    if (at(TOK_STR_LIT)) {
                        sa_ids[sa_count++] = add_string(cur->text, cur->int_val);
                        advance_tok();
                    } else if (at(TOK_IDENT)) {
                        /* function name or enum const in array init */
                        int eci_arr;
                        eci_arr = find_enum_const(cur->text);
                        if (eci_arr >= 0) {
                            sa_ids[sa_count++] = enum_consts[eci_arr]->val;
                        } else if (is_known_func(cur->text)) {
                            fn_table_add(cur->text);
                            sa_ids[sa_count++] = fn_table_idx(cur->text);
                        } else {
                            sa_ids[sa_count++] = 0;
                        }
                        advance_tok();
                    } else if (at(TOK_INT_LIT)) {
                        sa_ids[sa_count++] = cur->int_val;
                        advance_tok();
                    } else {
                        advance_tok(); /* skip unknown */
                    }
                    if (at(TOK_COMMA)) advance_tok();
                }
                expect(TOK_RBRACE, "expected '}'");
                if (ga_size == 0) ga_size = sa_count;
                globals_tbl[nglobals]->gv_arr_len = ga_size;
                globals_tbl[nglobals]->gv_arr_str_ids = sa_ids;
            }
        }
        nglobals++;
        expect(TOK_SEMI, "expected ';' after global array declaration");
        return;
    }
    if (at(TOK_EQ)) {
        advance_tok();
        if (at(TOK_FLOAT_LIT)) {
            globals_tbl[nglobals]->gv_float_init = strdupn(cur->text, 127);
            advance_tok();
        } else if (at(TOK_INT_LIT)) {
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
        } else if (at(TOK_IDENT)) {
            int eci_g;
            eci_g = find_enum_const(cur->text);
            if (eci_g >= 0) {
                globals_tbl[nglobals]->init_val = enum_consts[eci_g]->val;
            } else {
                error(cur->line, cur->col, "unknown initializer");
            }
            advance_tok();
        }
    }
    nglobals++;
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
                if (!at(TOK_INT_LIT)) error(cur->line, cur->col, "expected integer after '-' in enum");
                val = -cur->int_val;
            } else {
                if (!at(TOK_INT_LIT)) error(cur->line, cur->col, "expected integer constant in enum");
                val = cur->int_val;
            }
            advance_tok();
        }
        if (nenum_consts < MAX_ENUM_CONSTS) {
            ec = (struct EnumConst *)malloc(sizeof(struct EnumConst));
            ec->name = ename;
            ec->val = val;
            enum_consts[nenum_consts] = ec;
            nenum_consts++;
        }
        val++;
        if (at(TOK_COMMA)) advance_tok();
    }
    expect(TOK_RBRACE, "expected '}'");
    expect(TOK_SEMI, "expected ';' after enum");
}

void parse_typedef(void) {
    int rk;
    int is_ptr;
    char *alias_name;
    int tai;
    struct TypeAlias *ta;

    advance_tok(); /* consume 'typedef' */
    while (at(TOK_CONST)) advance_tok();
    rk = 0;
    is_ptr = 0;
    if (at(TOK_STRUCT)) {
        advance_tok();
        if (at(TOK_IDENT)) {
            advance_tok();
        }
        if (at(TOK_LBRACE)) {
            error(cur->line, cur->col, "typedef with inline struct body not supported; define struct separately");
        }
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
        tai = find_type_alias(cur->text);
        if (tai >= 0) {
            rk = type_aliases[tai]->resolved_kind;
            is_ptr = type_aliases[tai]->is_ptr;
        }
        advance_tok();
    } else {
        error(cur->line, cur->col, "expected type in typedef");
    }
    while (at(TOK_STAR)) { is_ptr = 1; advance_tok(); }
    /* function pointer typedef: typedef type (*Alias)(params) */
    if (at(TOK_LPAREN)) {
        int td_nparams;
        int td_is_void;
        td_nparams = 0;
        td_is_void = (rk == 1) ? 1 : 0;
        advance_tok(); /* consume '(' */
        while (at(TOK_STAR)) advance_tok();
        if (!at(TOK_IDENT)) error(cur->line, cur->col, "expected alias name in function pointer typedef");
        alias_name = strdupn(cur->text, 127);
        advance_tok();
        expect(TOK_RPAREN, "expected ')' in function pointer typedef");
        expect(TOK_LPAREN, "expected '(' for function pointer typedef parameters");
        while (!at(TOK_RPAREN) && !at(TOK_EOF)) {
            while (at(TOK_CONST)) advance_tok();
            if (at(TOK_VOID)) { advance_tok(); if (at(TOK_STAR)) { while (at(TOK_STAR)) advance_tok(); if (at(TOK_IDENT)) advance_tok(); td_nparams++; } }
            else {
                if (at(TOK_STRUCT)) { advance_tok(); if (at(TOK_IDENT)) advance_tok(); }
                else if (at(TOK_UNSIGNED)||at(TOK_SIGNED)||at(TOK_SHORT)||at(TOK_LONG)) { advance_tok(); if (at(TOK_INT)) advance_tok(); }
                if (at(TOK_INT)||at(TOK_CHAR_KW)||at(TOK_FLOAT)||at(TOK_DOUBLE)||at(TOK_IDENT)) advance_tok();
                while (at(TOK_STAR)) advance_tok();
                if (at(TOK_IDENT)) advance_tok();
                td_nparams++;
            }
            if (at(TOK_COMMA)) advance_tok();
        }
        expect(TOK_RPAREN, "expected ')' after function pointer typedef parameters");
        expect(TOK_SEMI, "expected ';' after typedef");
        if (ntype_aliases < MAX_TYPE_ALIASES) {
            ta = (struct TypeAlias *)malloc(sizeof(struct TypeAlias));
            ta->alias = alias_name;
            ta->resolved_kind = rk;
            ta->is_ptr = 1;
            ta->is_fnptr = 1;
            ta->fnptr_nparams = td_nparams;
            ta->fnptr_is_void = td_is_void;
            type_aliases[ntype_aliases] = ta;
            ntype_aliases++;
        }
        return;
    }
    if (!at(TOK_IDENT)) error(cur->line, cur->col, "expected alias name in typedef");
    alias_name = strdupn(cur->text, 127);
    advance_tok();
    expect(TOK_SEMI, "expected ';' after typedef");
    if (ntype_aliases < MAX_TYPE_ALIASES) {
        ta = (struct TypeAlias *)malloc(sizeof(struct TypeAlias));
        ta->alias = alias_name;
        ta->resolved_kind = rk;
        ta->is_ptr = is_ptr;
        ta->is_fnptr = 0;
        ta->fnptr_nparams = 0;
        ta->fnptr_is_void = 0;
        type_aliases[ntype_aliases] = ta;
        ntype_aliases++;
    }
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
        /* typedef declaration */
        if (at(TOK_TYPEDEF)) {
            parse_typedef();
            continue;
        }
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
    for (i = 0; i < indent_level; i++) {
        printf("  ");
    }
}

/* --- Local variable tracking --- */

struct LocalVar {
    char *name;
    int lv_elem_size;     /* 1=char, 2=short, 4=int */
    int lv_is_unsigned;
    int lv_is_float;      /* 0=int, 1=float(f32), 2=double(f64) */
};

struct LocalVar **local_vars;
int nlocals;

void init_local_tracking(void) {
    local_vars = (struct LocalVar **)malloc(MAX_LOCALS * sizeof(void *));
    nlocals = 0;
}

int find_local(char *name) {
    int i;
    for (i = 0; i < nlocals; i++) {
        if (strcmp(local_vars[i]->name, name) == 0) return i;
    }
    return -1;
}

void add_local(char *name, int elem_size, int is_unsigned, int is_float) {
    if (find_local(name) >= 0) return;
    if (nlocals >= MAX_LOCALS) {
        printf("too many locals\n");
        exit(1);
    }
    local_vars[nlocals] = (struct LocalVar *)malloc(sizeof(struct LocalVar));
    local_vars[nlocals]->name = strdupn(name, 127);
    local_vars[nlocals]->lv_elem_size = elem_size;
    local_vars[nlocals]->lv_is_unsigned = is_unsigned;
    local_vars[nlocals]->lv_is_float = is_float;
    nlocals++;
}

void collect_locals(struct Node *n) {
    int i;
    if (n == (struct Node *)0) return;
    switch (n->kind) {
    case ND_VAR_DECL:
        add_local(n->sval, n->ival2, n->ival3 & 0xF, n->ival3 >> 4);
        break;
    case ND_BLOCK:
        for (i = 0; i < n->ival2; i++) {
            collect_locals(n->list[i]);
        }
        break;
    case ND_IF:
        collect_locals(n->c1);
        collect_locals(n->c2);
        break;
    case ND_WHILE:
        collect_locals(n->c1);
        break;
    case ND_FOR:
        collect_locals(n->c0);
        collect_locals(n->c3);
        break;
    case ND_DO_WHILE:
        collect_locals(n->c0); /* condition (c1) cannot contain declarations */
        break;
    case ND_SWITCH:
        for (i = 0; i < n->ival2; i++) {
            collect_locals(n->list[i]);
        }
        break;
    case ND_POST_INC:
    case ND_POST_DEC:
        collect_locals(n->c0);
        break;
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
        return local_vars[li]->lv_elem_size;
    }
    gi = find_global(name);
    if (gi >= 0) {
        return globals_tbl[gi]->gv_elem_size;
    }
    return 4;
}

int var_is_unsigned(char *name) {
    int li;
    int gi;
    li = find_local(name);
    if (li >= 0) {
        return local_vars[li]->lv_is_unsigned;
    }
    gi = find_global(name);
    if (gi >= 0) {
        return globals_tbl[gi]->gv_is_unsigned;
    }
    return 0;
}

int expr_elem_size(struct Node *n) {
    if (n->kind == ND_IDENT) return var_elem_size(n->sval);
    if (n->kind == ND_BINARY && (n->ival == TOK_PLUS || n->ival == TOK_MINUS)) {
        if (n->c0->kind == ND_IDENT) return var_elem_size(n->c0->sval);
    }
    return 4;
}

int expr_is_unsigned(struct Node *n) {
    if (n->kind == ND_IDENT) return var_is_unsigned(n->sval);
    return 0;
}

int var_is_float(char *name) {
    int li;
    int gi;
    li = find_local(name);
    if (li >= 0) {
        return local_vars[li]->lv_is_float;
    }
    gi = find_global(name);
    if (gi >= 0) {
        return globals_tbl[gi]->gv_is_float;
    }
    return 0;
}

int last_expr_is_float; /* 0=i32, 1=f32, 2=f64 — set by gen_expr */

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

    switch (n->kind) {
    case ND_INT_LIT:
        emit_indent();
        printf("i32.const %d\n", n->ival);
        last_expr_is_float = 0;
        break;
    case ND_FLOAT_LIT:
        emit_indent();
        printf("f64.const %s\n", n->sval);
        last_expr_is_float = 2;
        break;
    case ND_CAST:
        gen_expr(n->c0);
        if (n->ival >= 1 && !last_expr_is_float) {
            /* cast to float/double, expr is int */
            emit_indent();
            printf("f64.convert_i32_s\n");
            last_expr_is_float = 2;
        } else if (n->ival >= 1 && last_expr_is_float) {
            /* cast to float/double, already float — no-op */
            last_expr_is_float = 2;
        } else if (n->ival == 0 && last_expr_is_float) {
            /* cast to int, expr is float */
            emit_indent();
            printf("i32.trunc_f64_s\n");
            last_expr_is_float = 0;
        }
        /* cast to int when already int — no-op */
        break;
    case ND_IDENT: {
        int vf;
        vf = var_is_float(n->sval);
        if (find_global(n->sval) >= 0) {
            emit_indent();
            printf("global.get $%s\n", n->sval);
        } else {
            emit_indent();
            printf("local.get $%s\n", n->sval);
        }
        last_expr_is_float = vf;
        break;
    }
    case ND_ASSIGN:
        tgt = n->c0;
        if (tgt->kind == ND_IDENT) {
            int tgt_float;
            name = tgt->sval;
            is_global = (find_global(name) >= 0);
            tgt_float = var_is_float(name);
            if (n->ival == TOK_EQ) {
                gen_expr(n->c1);
                /* insert float/int conversion if needed */
                if (tgt_float && !last_expr_is_float) {
                    emit_indent();
                    printf("f64.convert_i32_s\n");
                    last_expr_is_float = 2;
                } else if (!tgt_float && last_expr_is_float) {
                    emit_indent();
                    printf("i32.trunc_f64_s\n");
                    last_expr_is_float = 0;
                }
            } else if (n->ival == TOK_PLUS_EQ) {
                emit_indent();
                if (is_global) {
                    printf("global.get $%s\n", name);
                } else {
                    printf("local.get $%s\n", name);
                }
                gen_expr(n->c1);
                if (tgt_float) {
                    if (!last_expr_is_float) {
                        emit_indent();
                        printf("f64.convert_i32_s\n");
                    }
                    emit_indent();
                    printf("f64.add\n");
                    last_expr_is_float = 2;
                } else {
                    emit_indent();
                    printf("i32.add\n");
                    last_expr_is_float = 0;
                }
            } else if (n->ival == TOK_MINUS_EQ) {
                emit_indent();
                if (is_global) {
                    printf("global.get $%s\n", name);
                } else {
                    printf("local.get $%s\n", name);
                }
                gen_expr(n->c1);
                if (tgt_float) {
                    if (!last_expr_is_float) {
                        emit_indent();
                        printf("f64.convert_i32_s\n");
                    }
                    emit_indent();
                    printf("f64.sub\n");
                    last_expr_is_float = 2;
                } else {
                    emit_indent();
                    printf("i32.sub\n");
                    last_expr_is_float = 0;
                }
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
                last_expr_is_float = 0;
            }
            if (is_global) {
                if (tgt_float) {
                    emit_indent();
                    printf("local.set $__ftmp\n");
                    emit_indent();
                    printf("local.get $__ftmp\n");
                    emit_indent();
                    printf("global.set $%s\n", name);
                    emit_indent();
                    printf("local.get $__ftmp\n");
                } else {
                    emit_indent();
                    printf("local.set $__atmp\n");
                    emit_indent();
                    printf("local.get $__atmp\n");
                    emit_indent();
                    printf("global.set $%s\n", name);
                    emit_indent();
                    printf("local.get $__atmp\n");
                }
            } else {
                emit_indent();
                printf("local.tee $%s\n", name);
            }
            last_expr_is_float = tgt_float;
        } else if (tgt->kind == ND_UNARY && tgt->ival == TOK_STAR) {
            esz = expr_elem_size(tgt->c0);
            if (n->ival == TOK_EQ) {
                gen_expr(n->c1);
            } else if (n->ival == TOK_PLUS_EQ) {
                gen_expr(tgt->c0);
                emit_indent();
                if (esz == 1) {
                    printf("i32.load8_u\n");
                } else if (esz == 2) {
                    printf("i32.load16_s\n");
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
                } else if (esz == 2) {
                    printf("i32.load16_s\n");
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
                } else if (esz == 2) {
                    printf("i32.load16_s\n");
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
            if (last_expr_is_float) {
                printf("local.set $__ftmp\n");
                gen_expr(tgt->c0);
                emit_indent();
                printf("local.get $__ftmp\n");
                emit_indent();
                printf("f64.store\n");
                emit_indent();
                printf("local.get $__ftmp\n");
            } else {
                printf("local.set $__atmp\n");
                gen_expr(tgt->c0);
                emit_indent();
                printf("local.get $__atmp\n");
                emit_indent();
                if (esz == 1) {
                    printf("i32.store8\n");
                } else if (esz == 2) {
                    printf("i32.store16\n");
                } else {
                    printf("i32.store\n");
                }
                emit_indent();
                printf("local.get $__atmp\n");
            }
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
                } else if (esz == 2) {
                    printf("i32.load16_s\n");
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
                } else if (esz == 2) {
                    printf("i32.load16_s\n");
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
                } else if (esz == 2) {
                    printf("i32.load16_s\n");
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
            } else if (esz == 2) {
                printf("i32.store16\n");
            } else {
                printf("i32.store\n");
            }
            emit_indent();
            printf("local.get $__atmp\n");
        }
        break;
    case ND_UNARY:
        if (n->ival == TOK_MINUS) {
            gen_expr(n->c0);
            if (last_expr_is_float) {
                emit_indent();
                printf("f64.neg\n");
            } else {
                /* save value, push 0, push value, sub */
                emit_indent();
                printf("local.set $__atmp\n");
                emit_indent();
                printf("i32.const 0\n");
                emit_indent();
                printf("local.get $__atmp\n");
                emit_indent();
                printf("i32.sub\n");
            }
        } else if (n->ival == TOK_BANG) {
            gen_expr(n->c0);
            if (last_expr_is_float) {
                emit_indent();
                printf("f64.const 0\n");
                emit_indent();
                printf("f64.eq\n");
                last_expr_is_float = 0;
            } else {
                emit_indent();
                printf("i32.eqz\n");
            }
        } else if (n->ival == TOK_TILDE) {
            emit_indent();
            printf("i32.const -1\n");
            gen_expr(n->c0);
            emit_indent();
            printf("i32.xor\n");
            last_expr_is_float = 0;
        } else if (n->ival == TOK_STAR) {
            esz = expr_elem_size(n->c0);
            gen_expr(n->c0);
            if (esz == 8) {
                emit_indent();
                printf("f64.load\n");
                last_expr_is_float = 2;
            } else {
                last_expr_is_float = 0;
                emit_indent();
                if (esz == 1) {
                    printf("i32.load8_u\n");
                } else if (esz == 2) {
                    printf("i32.load16_s\n");
                } else {
                    printf("i32.load\n");
                }
            }
        } else if (n->ival == TOK_AMP) {
            error(n->nline, n->ncol, "cannot take address of this expression");
        }
        break;
    case ND_BINARY: {
        int left_float;
        int right_float;
        int op_float;
        gen_expr(n->c0);
        left_float = last_expr_is_float;
        gen_expr(n->c1);
        right_float = last_expr_is_float;
        op_float = 0;
        if (left_float || right_float) {
            op_float = 2;
            /* promote: if either is float, both should be f64 */
            if (left_float && !right_float) {
                /* stack: [f64, i32] — convert top (right) from i32 to f64 */
                emit_indent();
                printf("f64.convert_i32_s\n");
            } else if (!left_float && right_float) {
                /* stack: [i32, f64] — need to swap and convert left */
                emit_indent();
                printf("local.set $__ftmp\n");
                emit_indent();
                printf("f64.convert_i32_s\n");
                emit_indent();
                printf("local.get $__ftmp\n");
            }
        }
        if (op_float) {
            emit_indent();
            if (n->ival == TOK_PLUS) {
                printf("f64.add\n");
                last_expr_is_float = 2;
            } else if (n->ival == TOK_MINUS) {
                printf("f64.sub\n");
                last_expr_is_float = 2;
            } else if (n->ival == TOK_STAR) {
                printf("f64.mul\n");
                last_expr_is_float = 2;
            } else if (n->ival == TOK_SLASH) {
                printf("f64.div\n");
                last_expr_is_float = 2;
            } else if (n->ival == TOK_EQ_EQ) {
                printf("f64.eq\n");
                last_expr_is_float = 0;
            } else if (n->ival == TOK_BANG_EQ) {
                printf("f64.ne\n");
                last_expr_is_float = 0;
            } else if (n->ival == TOK_LT) {
                printf("f64.lt\n");
                last_expr_is_float = 0;
            } else if (n->ival == TOK_GT) {
                printf("f64.gt\n");
                last_expr_is_float = 0;
            } else if (n->ival == TOK_LT_EQ) {
                printf("f64.le\n");
                last_expr_is_float = 0;
            } else if (n->ival == TOK_GT_EQ) {
                printf("f64.ge\n");
                last_expr_is_float = 0;
            } else {
                error(n->nline, n->ncol, "unsupported float binary operator");
            }
        } else {
            emit_indent();
            if (n->ival == TOK_PLUS) {
                printf("i32.add\n");
            } else if (n->ival == TOK_MINUS) {
                printf("i32.sub\n");
            } else if (n->ival == TOK_STAR) {
                printf("i32.mul\n");
            } else if (n->ival == TOK_SLASH) {
                if (expr_is_unsigned(n->c0)) { printf("i32.div_u\n"); } else { printf("i32.div_s\n"); }
            } else if (n->ival == TOK_PERCENT) {
                if (expr_is_unsigned(n->c0)) { printf("i32.rem_u\n"); } else { printf("i32.rem_s\n"); }
            } else if (n->ival == TOK_EQ_EQ) {
                printf("i32.eq\n");
            } else if (n->ival == TOK_BANG_EQ) {
                printf("i32.ne\n");
            } else if (n->ival == TOK_LT) {
                if (expr_is_unsigned(n->c0)) { printf("i32.lt_u\n"); } else { printf("i32.lt_s\n"); }
            } else if (n->ival == TOK_GT) {
                if (expr_is_unsigned(n->c0)) { printf("i32.gt_u\n"); } else { printf("i32.gt_s\n"); }
            } else if (n->ival == TOK_LT_EQ) {
                if (expr_is_unsigned(n->c0)) { printf("i32.le_u\n"); } else { printf("i32.le_s\n"); }
            } else if (n->ival == TOK_GT_EQ) {
                if (expr_is_unsigned(n->c0)) { printf("i32.ge_u\n"); } else { printf("i32.ge_s\n"); }
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
                if (expr_is_unsigned(n->c0)) { printf("i32.shr_u\n"); } else { printf("i32.shr_s\n"); }
            } else if (n->ival == TOK_CARET) {
                printf("i32.xor\n");
            } else {
                error(n->nline, n->ncol, "unsupported binary operator");
            }
            last_expr_is_float = 0;
        }
        break;
    }
    case ND_CALL:
        if (strcmp(n->sval, "printf") == 0) {
            if (n->ival2 < 1 || n->list[0]->kind != ND_STR_LIT) {
                error(n->nline, n->ncol, "printf requires string literal format");
            }
            sid = n->list[0]->ival;
            fmt = str_table[sid]->data;
            flen = str_table[sid]->len;
            ai = 1;
            for (fi = 0; fi < flen; fi++) {
                if (fmt[fi] == '%' && fi + 1 < flen) {
                    fi++;
                    if (fmt[fi] == 'd') {
                        if (ai >= n->ival2) error(n->nline, n->ncol, "printf: missing arg for %d");
                        gen_expr(n->list[ai]);
                        ai++;
                        emit_indent();
                        printf("call $__print_int\n");
                    } else if (fmt[fi] == 's') {
                        if (ai >= n->ival2) error(n->nline, n->ncol, "printf: missing arg for %s");
                        gen_expr(n->list[ai]);
                        ai++;
                        emit_indent();
                        printf("call $__print_str\n");
                    } else if (fmt[fi] == 'c') {
                        if (ai >= n->ival2) error(n->nline, n->ncol, "printf: missing arg for %c");
                        gen_expr(n->list[ai]);
                        ai++;
                        emit_indent();
                        printf("call $putchar\n");
                        emit_indent();
                        printf("drop\n");
                    } else if (fmt[fi] == 'x') {
                        if (ai >= n->ival2) error(n->nline, n->ncol, "printf: missing arg for %x");
                        gen_expr(n->list[ai]);
                        ai++;
                        emit_indent();
                        printf("call $__print_hex\n");
                    } else if (fmt[fi] == 'f') {
                        if (ai >= n->ival2) error(n->nline, n->ncol, "printf: missing arg for %f");
                        gen_expr(n->list[ai]);
                        if (!last_expr_is_float) {
                            emit_indent();
                            printf("f64.convert_i32_s\n");
                        }
                        ai++;
                        emit_indent();
                        printf("call $__print_float\n");
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
            last_expr_is_float = 0;
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
        /* --- new libc builtins --- */
        } else if (strcmp(n->sval, "isdigit") == 0) {
            gen_expr(n->list[0]);
            emit_indent();
            printf("call $isdigit\n");
        } else if (strcmp(n->sval, "isalpha") == 0) {
            gen_expr(n->list[0]);
            emit_indent();
            printf("call $isalpha\n");
        } else if (strcmp(n->sval, "isalnum") == 0) {
            gen_expr(n->list[0]);
            emit_indent();
            printf("call $isalnum\n");
        } else if (strcmp(n->sval, "isspace") == 0) {
            gen_expr(n->list[0]);
            emit_indent();
            printf("call $isspace\n");
        } else if (strcmp(n->sval, "isupper") == 0) {
            gen_expr(n->list[0]);
            emit_indent();
            printf("call $isupper\n");
        } else if (strcmp(n->sval, "islower") == 0) {
            gen_expr(n->list[0]);
            emit_indent();
            printf("call $islower\n");
        } else if (strcmp(n->sval, "isprint") == 0) {
            gen_expr(n->list[0]);
            emit_indent();
            printf("call $isprint\n");
        } else if (strcmp(n->sval, "ispunct") == 0) {
            gen_expr(n->list[0]);
            emit_indent();
            printf("call $ispunct\n");
        } else if (strcmp(n->sval, "isxdigit") == 0) {
            gen_expr(n->list[0]);
            emit_indent();
            printf("call $isxdigit\n");
        } else if (strcmp(n->sval, "toupper") == 0) {
            gen_expr(n->list[0]);
            emit_indent();
            printf("call $toupper\n");
        } else if (strcmp(n->sval, "tolower") == 0) {
            gen_expr(n->list[0]);
            emit_indent();
            printf("call $tolower\n");
        } else if (strcmp(n->sval, "abs") == 0) {
            gen_expr(n->list[0]);
            emit_indent();
            printf("call $abs\n");
        } else if (strcmp(n->sval, "atoi") == 0) {
            gen_expr(n->list[0]);
            emit_indent();
            printf("call $atoi\n");
        } else if (strcmp(n->sval, "puts") == 0) {
            gen_expr(n->list[0]);
            emit_indent();
            printf("call $puts\n");
        } else if (strcmp(n->sval, "srand") == 0) {
            gen_expr(n->list[0]);
            emit_indent();
            printf("call $srand\n");
            emit_indent();
            printf("i32.const 0\n");
        } else if (strcmp(n->sval, "rand") == 0) {
            emit_indent();
            printf("call $rand\n");
        } else if (strcmp(n->sval, "strcpy") == 0) {
            gen_expr(n->list[0]);
            gen_expr(n->list[1]);
            emit_indent();
            printf("call $strcpy\n");
        } else if (strcmp(n->sval, "strcat") == 0) {
            gen_expr(n->list[0]);
            gen_expr(n->list[1]);
            emit_indent();
            printf("call $strcat\n");
        } else if (strcmp(n->sval, "strchr") == 0) {
            gen_expr(n->list[0]);
            gen_expr(n->list[1]);
            emit_indent();
            printf("call $strchr\n");
        } else if (strcmp(n->sval, "strrchr") == 0) {
            gen_expr(n->list[0]);
            gen_expr(n->list[1]);
            emit_indent();
            printf("call $strrchr\n");
        } else if (strcmp(n->sval, "strstr") == 0) {
            gen_expr(n->list[0]);
            gen_expr(n->list[1]);
            emit_indent();
            printf("call $strstr\n");
        } else if (strcmp(n->sval, "calloc") == 0) {
            gen_expr(n->list[0]);
            gen_expr(n->list[1]);
            emit_indent();
            printf("call $calloc\n");
        } else if (strcmp(n->sval, "strncmp") == 0) {
            gen_expr(n->list[0]);
            gen_expr(n->list[1]);
            gen_expr(n->list[2]);
            emit_indent();
            printf("call $strncmp\n");
        } else if (strcmp(n->sval, "strncat") == 0) {
            gen_expr(n->list[0]);
            gen_expr(n->list[1]);
            gen_expr(n->list[2]);
            emit_indent();
            printf("call $strncat\n");
        } else if (strcmp(n->sval, "memmove") == 0) {
            gen_expr(n->list[0]);
            gen_expr(n->list[1]);
            gen_expr(n->list[2]);
            emit_indent();
            printf("call $memmove\n");
        } else if (strcmp(n->sval, "memchr") == 0) {
            gen_expr(n->list[0]);
            gen_expr(n->list[1]);
            gen_expr(n->list[2]);
            emit_indent();
            printf("call $memchr\n");
        } else if (strcmp(n->sval, "strtol") == 0) {
            gen_expr(n->list[0]);
            gen_expr(n->list[1]);
            gen_expr(n->list[2]);
            emit_indent();
            printf("call $strtol\n");
        } else {
            for (i = 0; i < n->ival2; i++) {
                gen_expr(n->list[i]);
                /* convert param if needed */
                if (func_param_is_float(n->sval, i) && !last_expr_is_float) {
                    emit_indent();
                    printf("f64.convert_i32_s\n");
                } else if (!func_param_is_float(n->sval, i) && last_expr_is_float) {
                    emit_indent();
                    printf("i32.trunc_f64_s\n");
                }
            }
            emit_indent();
            printf("call $%s\n", n->sval);
            if (func_is_void(n->sval)) {
                emit_indent();
                printf("i32.const 0\n");
                last_expr_is_float = 0;
            } else {
                last_expr_is_float = func_ret_is_float(n->sval);
            }
        }
        break;
    case ND_STR_LIT:
        emit_indent();
        printf("i32.const %d\n", str_table[n->ival]->offset);
        last_expr_is_float = 0;
        break;
    case ND_CALL_INDIRECT: {
        int ci_i;
        int ci_np;
        ci_np = n->ival; /* declared param count */
        /* push arguments */
        for (ci_i = 0; ci_i < n->ival2; ci_i++) {
            gen_expr(n->list[ci_i]);
        }
        /* push table index (the callee expression) */
        gen_expr(n->c0);
        /* emit call_indirect with matching type signature */
        emit_indent();
        printf("call_indirect (type $__fntype_%d_%s)\n",
               ci_np, n->ival3 ? "void" : "i32");
        if (n->ival3) {
            /* void function — push dummy i32 */
            emit_indent();
            printf("i32.const 0\n");
        }
        last_expr_is_float = 0;
        break;
    }
    case ND_MEMBER:
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
        break;
    case ND_SIZEOF: {
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
        } else if (n->ival2 > 0) {
            sz = n->ival2;
        } else {
            sz = 4;
        }
        emit_indent();
        printf("i32.const %d\n", sz);
        break;
    }
    case ND_SUBSCRIPT:
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
        } else if (esz == 2) {
            printf("i32.load16_s\n");
        } else {
            printf("i32.load\n");
        }
        break;
    case ND_POST_INC:
    case ND_POST_DEC: {
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
            if (pesz == 1) { printf("i32.load8_u\n"); } else if (pesz == 2) { printf("i32.load16_s\n"); } else { printf("i32.load\n"); }
            emit_indent(); printf("local.set $__atmp\n");
            gen_expr(tgt2->c0);
            gen_expr(tgt2->c0);
            emit_indent();
            if (pesz == 1) { printf("i32.load8_u\n"); } else if (pesz == 2) { printf("i32.load16_s\n"); } else { printf("i32.load\n"); }
            emit_indent(); printf("i32.const 1\n");
            emit_indent();
            if (n->kind == ND_POST_INC) { printf("i32.add\n"); } else { printf("i32.sub\n"); }
            emit_indent();
            if (pesz == 1) { printf("i32.store8\n"); } else if (pesz == 2) { printf("i32.store16\n"); } else { printf("i32.store\n"); }
            emit_indent(); printf("local.get $__atmp\n");
        } else if (tgt2->kind == ND_SUBSCRIPT) {
            /* NOTE: tgt2->c0 and tgt2->c1 each evaluated 3x (save old val, store addr, reload).
               Correct only when the array and index expressions have no side effects. */
            pesz = expr_elem_size(tgt2->c0);
            gen_expr(tgt2->c0); gen_expr(tgt2->c1);
            if (pesz > 1) { emit_indent(); printf("i32.const %d\n", pesz); emit_indent(); printf("i32.mul\n"); }
            emit_indent(); printf("i32.add\n");
            emit_indent();
            if (pesz == 1) { printf("i32.load8_u\n"); } else if (pesz == 2) { printf("i32.load16_s\n"); } else { printf("i32.load\n"); }
            emit_indent(); printf("local.set $__atmp\n");
            gen_expr(tgt2->c0); gen_expr(tgt2->c1);
            if (pesz > 1) { emit_indent(); printf("i32.const %d\n", pesz); emit_indent(); printf("i32.mul\n"); }
            emit_indent(); printf("i32.add\n");
            gen_expr(tgt2->c0); gen_expr(tgt2->c1);
            if (pesz > 1) { emit_indent(); printf("i32.const %d\n", pesz); emit_indent(); printf("i32.mul\n"); }
            emit_indent(); printf("i32.add\n");
            emit_indent();
            if (pesz == 1) { printf("i32.load8_u\n"); } else if (pesz == 2) { printf("i32.load16_s\n"); } else { printf("i32.load\n"); }
            emit_indent(); printf("i32.const 1\n");
            emit_indent();
            if (n->kind == ND_POST_INC) { printf("i32.add\n"); } else { printf("i32.sub\n"); }
            emit_indent();
            if (pesz == 1) { printf("i32.store8\n"); } else if (pesz == 2) { printf("i32.store16\n"); } else { printf("i32.store\n"); }
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
        break;
    }
    case ND_TERNARY:
        gen_expr(n->c0);
        /* both branches produce i32; compiler is uniformly i32 throughout */
        emit_indent();
        printf("(if (result i32)\n");
        indent_level++;
        emit_indent();
        printf("(then\n");
        indent_level++;
        gen_expr(n->c1);
        indent_level--;
        emit_indent();
        printf(")\n");
        emit_indent();
        printf("(else\n");
        indent_level++;
        gen_expr(n->c2);
        indent_level--;
        emit_indent();
        printf(")\n");
        indent_level--;
        emit_indent();
        printf(")\n");
        break;
    default:
        error(n->nline, n->ncol, "unsupported expression in codegen");
    }
}

/* --- Statement codegen --- */

void gen_stmt(struct Node *n);

void gen_body(struct Node *n) {
    int i;
    if (n->kind == ND_BLOCK) {
        for (i = 0; i < n->ival2; i++) {
            gen_stmt(n->list[i]);
        }
    } else {
        gen_stmt(n);
    }
}

void gen_stmt(struct Node *n) {
    int lbl;
    int i;
    int bsz;

    switch (n->kind) {
    case ND_RETURN:
        if (n->c0 != (struct Node *)0) {
            gen_expr(n->c0);
        }
        emit_indent();
        printf("return\n");
        break;
    case ND_VAR_DECL: {
        int vd_is_flt;
        vd_is_flt = n->ival3 >> 4;
        if (n->ival > 0) {
            /* Array: allocate n->ival elements of elem_size bytes */
            bsz = n->ival * n->ival2;
            emit_indent();
            printf("i32.const %d\n", bsz);
            emit_indent();
            printf("call $malloc\n");
            emit_indent();
            printf("local.set $%s\n", n->sval);
        } else if (n->c0 != (struct Node *)0) {
            gen_expr(n->c0);
            /* type conversion if needed */
            if (vd_is_flt && !last_expr_is_float) {
                emit_indent();
                printf("f64.convert_i32_s\n");
            } else if (!vd_is_flt && last_expr_is_float) {
                emit_indent();
                printf("i32.trunc_f64_s\n");
            }
            emit_indent();
            printf("local.set $%s\n", n->sval);
        }
        break;
    }
    case ND_EXPR_STMT:
        gen_expr(n->c0);
        emit_indent();
        printf("drop\n");
        break;
    case ND_IF:
        gen_expr(n->c0);
        if (last_expr_is_float) {
            /* convert float condition to boolean: f64 != 0.0 */
            emit_indent();
            printf("f64.const 0\n");
            emit_indent();
            printf("f64.ne\n");
        }
        if (n->c2 != (struct Node *)0) {
            emit_indent();
            printf("(if\n");
            indent_level++;
            emit_indent();
            printf("(then\n");
            indent_level++;
            gen_body(n->c1);
            indent_level--;
            emit_indent();
            printf(")\n");
            emit_indent();
            printf("(else\n");
            indent_level++;
            gen_body(n->c2);
            indent_level--;
            emit_indent();
            printf(")\n");
            indent_level--;
            emit_indent();
            printf(")\n");
        } else {
            emit_indent();
            printf("(if\n");
            indent_level++;
            emit_indent();
            printf("(then\n");
            indent_level++;
            gen_body(n->c1);
            indent_level--;
            emit_indent();
            printf(")\n");
            indent_level--;
            emit_indent();
            printf(")\n");
        }
        break;
    case ND_WHILE:
        lbl = label_cnt;
        label_cnt++;
        brk_lbl[loop_sp] = lbl;
        cont_lbl[loop_sp] = lbl;
        loop_sp++;
        emit_indent();
        printf("(block $brk_%d\n", lbl);
        indent_level++;
        emit_indent();
        printf("(loop $lp_%d\n", lbl);
        indent_level++;
        gen_expr(n->c0);
        if (last_expr_is_float) {
            emit_indent();
            printf("f64.const 0\n");
            emit_indent();
            printf("f64.ne\n");
        }
        emit_indent();
        printf("i32.eqz\n");
        emit_indent();
        printf("br_if $brk_%d\n", lbl);
        emit_indent();
        printf("(block $cont_%d\n", lbl);
        indent_level++;
        gen_body(n->c1);
        indent_level--;
        emit_indent();
        printf(")\n");
        emit_indent();
        printf("br $lp_%d\n", lbl);
        indent_level--;
        emit_indent();
        printf(")\n");
        indent_level--;
        emit_indent();
        printf(")\n");
        loop_sp--;
        break;
    case ND_FOR:
        if (n->c0 != (struct Node *)0) {
            gen_stmt(n->c0);
        }
        lbl = label_cnt;
        label_cnt++;
        brk_lbl[loop_sp] = lbl;
        cont_lbl[loop_sp] = lbl;
        loop_sp++;
        emit_indent();
        printf("(block $brk_%d\n", lbl);
        indent_level++;
        emit_indent();
        printf("(loop $lp_%d\n", lbl);
        indent_level++;
        if (n->c1 != (struct Node *)0) {
            gen_expr(n->c1);
            if (last_expr_is_float) {
                emit_indent();
                printf("f64.const 0\n");
                emit_indent();
                printf("f64.ne\n");
            }
            emit_indent();
            printf("i32.eqz\n");
            emit_indent();
            printf("br_if $brk_%d\n", lbl);
        }
        emit_indent();
        printf("(block $cont_%d\n", lbl);
        indent_level++;
        gen_body(n->c3);
        indent_level--;
        emit_indent();
        printf(")\n");
        if (n->c2 != (struct Node *)0) {
            gen_expr(n->c2);
            emit_indent();
            printf("drop\n");
        }
        emit_indent();
        printf("br $lp_%d\n", lbl);
        indent_level--;
        emit_indent();
        printf(")\n");
        indent_level--;
        emit_indent();
        printf(")\n");
        loop_sp--;
        break;
    case ND_DO_WHILE:
        lbl = label_cnt;
        label_cnt++;
        brk_lbl[loop_sp] = lbl;
        cont_lbl[loop_sp] = lbl;
        loop_sp++;
        emit_indent();
        printf("(block $brk_%d\n", lbl);
        indent_level++;
        emit_indent();
        printf("(loop $lp_%d\n", lbl);
        indent_level++;
        emit_indent();
        printf("(block $cont_%d\n", lbl);
        indent_level++;
        gen_body(n->c0);
        indent_level--;
        emit_indent();
        printf(")\n");
        gen_expr(n->c1);
        if (last_expr_is_float) {
            emit_indent();
            printf("f64.const 0\n");
            emit_indent();
            printf("f64.ne\n");
        }
        emit_indent();
        printf("br_if $lp_%d\n", lbl);
        indent_level--;
        emit_indent();
        printf(")\n");
        indent_level--;
        emit_indent();
        printf(")\n");
        loop_sp--;
        break;
    case ND_BREAK:
        if (loop_sp <= 0) error(n->nline, n->ncol, "break outside loop");
        emit_indent();
        printf("br $brk_%d\n", brk_lbl[loop_sp - 1]);
        break;
    case ND_CONTINUE:
        if (loop_sp <= 0) error(n->nline, n->ncol, "continue outside loop");
        if (cont_lbl[loop_sp - 1] < 0) error(n->nline, n->ncol, "continue not inside a loop");
        emit_indent();
        printf("br $cont_%d\n", cont_lbl[loop_sp - 1]);
        break;
    case ND_BLOCK:
        for (i = 0; i < n->ival2; i++) {
            gen_stmt(n->list[i]);
        }
        break;
    case ND_SWITCH: {
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
        for (si = 0; si < n->ival2; si++) {
            if (n->list[si]->kind == ND_CASE) {
                if (nc >= MAX_CASES) {
                    error(n->nline, n->ncol, "too many cases in switch");
                }
                case_vals[nc] = n->list[si]->ival;
                case_start[nc] = si;
                nc++;
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
        label_cnt++;
        brk_lbl[loop_sp] = sw_lbl;
        if (loop_sp > 0) {
            cont_lbl[loop_sp] = cont_lbl[loop_sp - 1];
        } else {
            cont_lbl[loop_sp] = -1;
        }
        loop_sp++;

        /* save switch value */
        gen_expr(n->c0);
        emit_indent();
        printf("local.set $__stmp\n");

        /* outer break block */
        emit_indent();
        printf("(block $brk_%d\n", sw_lbl);
        indent_level++;

        /* default target block (outermost) */
        emit_indent();
        printf("(block $sw%d_dflt\n", sw_lbl);
        indent_level++;

        /* open case blocks in reverse order: first case = innermost */
        for (k = nc - 1; k >= 0; k--) {
            emit_indent();
            printf("(block $sw%d_c%d\n", sw_lbl, k);
            indent_level++;
        }

        /* dispatch: compare and branch for each case */
        for (k = 0; k < nc; k++) {
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
        for (k = 0; k < nc; k++) {
            indent_level--;
            emit_indent(); printf(")\n");
            if (k + 1 < nc) {
                next_start = case_start[k + 1];
            } else if (has_dflt) {
                next_start = dflt_pos;
            } else {
                next_start = n->ival2;
            }
            for (j = case_start[k] + 1; j < next_start; j++) {
                if (n->list[j]->kind == ND_CASE) continue;
                if (n->list[j]->kind == ND_DEFAULT) continue;
                gen_stmt(n->list[j]);
            }
        }

        /* close default target block */
        indent_level--;
        emit_indent(); printf(")\n");

        /* emit default body */
        if (has_dflt) {
            for (j = dflt_pos + 1; j < n->ival2; j++) {
                if (n->list[j]->kind == ND_CASE) continue;
                if (n->list[j]->kind == ND_DEFAULT) continue;
                gen_stmt(n->list[j]);
            }
        }

        /* close break block */
        indent_level--;
        emit_indent(); printf(")\n");

        loop_sp--;
        break;
    }
    default:
        error(n->nline, n->ncol, "unsupported statement in codegen");
    }
}

/* --- Function codegen --- */

void gen_func(struct Node *n) {
    struct Node *body;
    int i;
    int nparam_locals;
    int ret_float;
    int has_any_float;

    if (n->c0 == (struct Node *)0) return;

    nlocals = 0;
    label_cnt = 0;
    loop_sp = 0;
    ret_float = n->ival3; /* 0=int, 1=float, 2=double */
    /* register params as locals for is_char tracking */
    for (i = 0; i < n->ival2; i++) {
        add_local(n->list[i]->sval, n->list[i]->ival2, n->list[i]->ival3 & 0xF, n->list[i]->ival3 >> 4);
    }
    nparam_locals = nlocals;
    collect_locals(n->c0);

    printf("  (func $%s", n->sval);
    for (i = 0; i < n->ival2; i++) {
        if (local_vars[i]->lv_is_float) {
            printf(" (param $%s f64)", n->list[i]->sval);
        } else {
            printf(" (param $%s i32)", n->list[i]->sval);
        }
    }
    if (n->ival == 1) {
        /* void */
    } else if (ret_float) {
        printf(" (result f64)");
    } else {
        printf(" (result i32)");
    }
    printf("\n");

    indent_level = 2;
    /* emit only non-param locals */
    has_any_float = 0;
    for (i = nparam_locals; i < nlocals; i++) {
        emit_indent();
        if (local_vars[i]->lv_is_float) {
            printf("(local $%s f64)\n", local_vars[i]->name);
            has_any_float = 1;
        } else {
            printf("(local $%s i32)\n", local_vars[i]->name);
        }
    }
    /* check params for float too */
    for (i = 0; i < nparam_locals; i++) {
        if (local_vars[i]->lv_is_float) has_any_float = 1;
    }
    emit_indent();
    printf("(local $__atmp i32)\n");
    emit_indent();
    printf("(local $__stmp i32)\n");
    emit_indent();
    printf("(local $__ftmp f64)\n");
    body = n->c0;
    for (i = 0; i < body->ival2; i++) {
        gen_stmt(body->list[i]);
    }
    if (n->ival != 1) {
        if (ret_float) {
            emit_indent();
            printf("f64.const 0\n");
        } else {
            emit_indent();
            printf("i32.const 0\n");
        }
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
    for (i = 0; i < len; i++) {
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
    indent_level++;

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

    /* function pointer type declarations and table */
    if (fn_table_count > 0) {
        int fti;
        int need_types[17]; /* need_types[nparams] = bitmask: bit0=i32 result, bit1=void */
        for (fti = 0; fti < 17; fti++) need_types[fti] = 0;
        /* scan fnptr_vars to collect needed type signatures */
        for (fti = 0; fti < nfnptr_vars; fti++) {
            int np;
            int vd;
            np = fnptr_vars[fti]->nparams;
            vd = fnptr_vars[fti]->is_void;
            if (np <= 16) {
                need_types[np] = need_types[np] | (1 << vd);
            }
        }
        /* emit type declarations */
        for (fti = 0; fti <= 16; fti++) {
            if (need_types[fti] & 1) {
                int pi;
                emit_indent();
                printf("(type $__fntype_%d_i32 (func", fti);
                for (pi = 0; pi < fti; pi++) printf(" (param i32)");
                printf(" (result i32)))\n");
            }
            if (need_types[fti] & 2) {
                int pi;
                emit_indent();
                printf("(type $__fntype_%d_void (func", fti);
                for (pi = 0; pi < fti; pi++) printf(" (param i32)");
                printf("))\n");
            }
        }
        /* table and elem */
        emit_indent();
        printf("(table %d funcref)\n", fn_table_count);
        emit_indent();
        printf("(elem (i32.const 0)");
        for (fti = 0; fti < fn_table_count; fti++) {
            printf(" $%s", fn_table_names[fti]);
        }
        printf(")\n");
        emit_indent();
        printf("\n");
    }

    /* static data section */
    for (i = 0; i < nstrings; i++) {
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
    emit_indent();
    printf("(global $__free_list (mut i32) (i32.const 0))\n");
    emit_indent();
    printf("(global $__rand_seed (mut i32) (i32.const 1))\n");

    /* user global variables */
    for (gi = 0; gi < nglobals; gi++) {
        emit_indent();
        if (globals_tbl[gi]->gv_is_float) {
            if (globals_tbl[gi]->gv_float_init != (char *)0) {
                printf("(global $%s (mut f64) (f64.const %s))\n",
                       globals_tbl[gi]->name, globals_tbl[gi]->gv_float_init);
            } else {
                printf("(global $%s (mut f64) (f64.const %d))\n",
                       globals_tbl[gi]->name, globals_tbl[gi]->init_val);
            }
        } else {
            printf("(global $%s (mut i32) (i32.const %d))\n",
                   globals_tbl[gi]->name, globals_tbl[gi]->init_val);
        }
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

    /* __print_float: prints f64 with 6 decimal places */
    emit_indent();
    printf("(func $__print_float (param $v f64)\n");
    emit_indent();
    printf("  (local $int_part i32)\n");
    emit_indent();
    printf("  (local $frac_part i32)\n");
    emit_indent();
    printf("  (local $frac f64)\n");
    emit_indent();
    printf("  (local $i i32)\n");
    emit_indent();
    printf("  ;; handle negative\n");
    emit_indent();
    printf("  (if (f64.lt (local.get $v) (f64.const 0))\n");
    emit_indent();
    printf("    (then\n");
    emit_indent();
    printf("      (drop (call $putchar (i32.const 45)))\n");
    emit_indent();
    printf("      (local.set $v (f64.neg (local.get $v)))\n");
    emit_indent();
    printf("    )\n");
    emit_indent();
    printf("  )\n");
    emit_indent();
    printf("  ;; integer part\n");
    emit_indent();
    printf("  (local.set $int_part (i32.trunc_f64_s (local.get $v)))\n");
    emit_indent();
    printf("  (call $__print_int (local.get $int_part))\n");
    emit_indent();
    printf("  ;; dot\n");
    emit_indent();
    printf("  (drop (call $putchar (i32.const 46)))\n");
    emit_indent();
    printf("  ;; fractional part: 6 digits\n");
    emit_indent();
    printf("  (local.set $frac (f64.sub (local.get $v) (f64.convert_i32_s (local.get $int_part))))\n");
    emit_indent();
    printf("  (local.set $frac (f64.mul (local.get $frac) (f64.const 1000000)))\n");
    emit_indent();
    printf("  (local.set $frac (f64.add (local.get $frac) (f64.const 0.5)))\n");
    emit_indent();
    printf("  (local.set $frac_part (i32.trunc_f64_s (local.get $frac)))\n");
    emit_indent();
    printf("  ;; print with leading zeros (6 digits)\n");
    emit_indent();
    printf("  (local.set $i (i32.const 100000))\n");
    emit_indent();
    printf("  (block $done (loop $lp\n");
    emit_indent();
    printf("    (br_if $done (i32.eqz (local.get $i)))\n");
    emit_indent();
    printf("    (drop (call $putchar (i32.add (i32.const 48) (i32.div_u (local.get $frac_part) (local.get $i)))))\n");
    emit_indent();
    printf("    (local.set $frac_part (i32.rem_u (local.get $frac_part) (local.get $i)))\n");
    emit_indent();
    printf("    (local.set $i (i32.div_u (local.get $i) (i32.const 10)))\n");
    emit_indent();
    printf("    (br $lp)\n");
    emit_indent();
    printf("  ))\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    emit_indent();
    printf("(func $malloc (param $size i32) (result i32)\n");
    emit_indent();
    printf("  (local $total i32)\n");
    emit_indent();
    printf("  (local $cur i32)\n");
    emit_indent();
    printf("  (local $prev i32)\n");
    emit_indent();
    printf("  (local $rest i32)\n");
    emit_indent();
    printf("  (local $ptr i32)\n");
    emit_indent();
    printf("  (local.set $total\n");
    emit_indent();
    printf("    (i32.and (i32.add (local.get $size) (i32.const 15)) (i32.const -8)))\n");
    emit_indent();
    printf("  (local.set $prev (i32.const 0))\n");
    emit_indent();
    printf("  (local.set $cur (global.get $__free_list))\n");
    emit_indent();
    printf("  (block $done\n");
    emit_indent();
    printf("    (loop $search\n");
    emit_indent();
    printf("      (br_if $done (i32.eqz (local.get $cur)))\n");
    emit_indent();
    printf("      (if (i32.ge_u (i32.load (local.get $cur)) (local.get $total))\n");
    emit_indent();
    printf("        (then\n");
    emit_indent();
    printf("          (if (i32.ge_u (i32.load (local.get $cur)) (i32.add (local.get $total) (i32.const 16)))\n");
    emit_indent();
    printf("            (then\n");
    emit_indent();
    printf("              (local.set $rest (i32.add (local.get $cur) (local.get $total)))\n");
    emit_indent();
    printf("              (i32.store (local.get $rest)\n");
    emit_indent();
    printf("                (i32.sub (i32.load (local.get $cur)) (local.get $total)))\n");
    emit_indent();
    printf("              (i32.store offset=4 (local.get $rest)\n");
    emit_indent();
    printf("                (i32.load offset=4 (local.get $cur)))\n");
    emit_indent();
    printf("              (i32.store (local.get $cur) (local.get $total))\n");
    emit_indent();
    printf("              (if (local.get $prev)\n");
    emit_indent();
    printf("                (then\n");
    emit_indent();
    printf("                  (i32.store offset=4 (local.get $prev) (local.get $rest)))\n");
    emit_indent();
    printf("                (else\n");
    emit_indent();
    printf("                  (global.set $__free_list (local.get $rest))))\n");
    emit_indent();
    printf("            )\n");
    emit_indent();
    printf("            (else\n");
    emit_indent();
    printf("              (if (local.get $prev)\n");
    emit_indent();
    printf("                (then\n");
    emit_indent();
    printf("                  (i32.store offset=4 (local.get $prev)\n");
    emit_indent();
    printf("                    (i32.load offset=4 (local.get $cur))))\n");
    emit_indent();
    printf("                (else\n");
    emit_indent();
    printf("                  (global.set $__free_list\n");
    emit_indent();
    printf("                    (i32.load offset=4 (local.get $cur)))))\n");
    emit_indent();
    printf("          ))\n");
    emit_indent();
    printf("          (return (i32.add (local.get $cur) (i32.const 8)))\n");
    emit_indent();
    printf("        )\n");
    emit_indent();
    printf("      )\n");
    emit_indent();
    printf("      (local.set $prev (local.get $cur))\n");
    emit_indent();
    printf("      (local.set $cur (i32.load offset=4 (local.get $cur)))\n");
    emit_indent();
    printf("      (br $search)\n");
    emit_indent();
    printf("    )\n");
    emit_indent();
    printf("  )\n");
    emit_indent();
    printf("  (local.set $ptr (global.get $__heap_ptr))\n");
    emit_indent();
    printf("  (i32.store (local.get $ptr) (local.get $total))\n");
    emit_indent();
    printf("  (global.set $__heap_ptr (i32.add (local.get $ptr) (local.get $total)))\n");
    emit_indent();
    printf("  (i32.add (local.get $ptr) (i32.const 8))\n");
    emit_indent();
    printf(")\n");

    emit_indent();
    printf("(func $free (param $ptr i32)\n");
    emit_indent();
    printf("  (local $block i32)\n");
    emit_indent();
    printf("  (local $cur i32)\n");
    emit_indent();
    printf("  (local $prev i32)\n");
    emit_indent();
    printf("  (local $next_blk i32)\n");
    emit_indent();
    printf("  (if (i32.eqz (local.get $ptr)) (then (return)))\n");
    emit_indent();
    printf("  (local.set $block (i32.sub (local.get $ptr) (i32.const 8)))\n");
    emit_indent();
    printf("  (local.set $prev (i32.const 0))\n");
    emit_indent();
    printf("  (local.set $cur (global.get $__free_list))\n");
    emit_indent();
    printf("  (block $found\n");
    emit_indent();
    printf("    (loop $scan\n");
    emit_indent();
    printf("      (br_if $found (i32.eqz (local.get $cur)))\n");
    emit_indent();
    printf("      (br_if $found (i32.gt_u (local.get $cur) (local.get $block)))\n");
    emit_indent();
    printf("      (local.set $prev (local.get $cur))\n");
    emit_indent();
    printf("      (local.set $cur (i32.load offset=4 (local.get $cur)))\n");
    emit_indent();
    printf("      (br $scan)\n");
    emit_indent();
    printf("    )\n");
    emit_indent();
    printf("  )\n");
    emit_indent();
    printf("  (i32.store offset=4 (local.get $block) (local.get $cur))\n");
    emit_indent();
    printf("  (if (local.get $prev)\n");
    emit_indent();
    printf("    (then (i32.store offset=4 (local.get $prev) (local.get $block)))\n");
    emit_indent();
    printf("    (else (global.set $__free_list (local.get $block)))\n");
    emit_indent();
    printf("  )\n");
    emit_indent();
    printf("  (if (i32.and\n");
    emit_indent();
    printf("        (i32.ne (local.get $cur) (i32.const 0))\n");
    emit_indent();
    printf("        (i32.eq (i32.add (local.get $block) (i32.load (local.get $block)))\n");
    emit_indent();
    printf("                (local.get $cur)))\n");
    emit_indent();
    printf("    (then\n");
    emit_indent();
    printf("      (i32.store (local.get $block)\n");
    emit_indent();
    printf("        (i32.add (i32.load (local.get $block)) (i32.load (local.get $cur))))\n");
    emit_indent();
    printf("      (i32.store offset=4 (local.get $block)\n");
    emit_indent();
    printf("        (i32.load offset=4 (local.get $cur)))\n");
    emit_indent();
    printf("    )\n");
    emit_indent();
    printf("  )\n");
    emit_indent();
    printf("  (if (i32.and\n");
    emit_indent();
    printf("        (i32.ne (local.get $prev) (i32.const 0))\n");
    emit_indent();
    printf("        (i32.eq (i32.add (local.get $prev) (i32.load (local.get $prev)))\n");
    emit_indent();
    printf("                (local.get $block)))\n");
    emit_indent();
    printf("    (then\n");
    emit_indent();
    printf("      (i32.store (local.get $prev)\n");
    emit_indent();
    printf("        (i32.add (i32.load (local.get $prev)) (i32.load (local.get $block))))\n");
    emit_indent();
    printf("      (i32.store offset=4 (local.get $prev)\n");
    emit_indent();
    printf("        (i32.load offset=4 (local.get $block)))\n");
    emit_indent();
    printf("    )\n");
    emit_indent();
    printf("  )\n");
    emit_indent();
    printf(")\n");

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

    /* --- new libc helper functions --- */

    /* isdigit */
    emit_indent();
    printf("(func $isdigit (param $c i32) (result i32)\n");
    emit_indent();
    printf("  (i32.and\n");
    emit_indent();
    printf("    (i32.ge_u (local.get $c) (i32.const 48))\n");
    emit_indent();
    printf("    (i32.le_u (local.get $c) (i32.const 57)))\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* isalpha */
    emit_indent();
    printf("(func $isalpha (param $c i32) (result i32)\n");
    emit_indent();
    printf("  (i32.or\n");
    emit_indent();
    printf("    (i32.and (i32.ge_u (local.get $c) (i32.const 65)) (i32.le_u (local.get $c) (i32.const 90)))\n");
    emit_indent();
    printf("    (i32.and (i32.ge_u (local.get $c) (i32.const 97)) (i32.le_u (local.get $c) (i32.const 122))))\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* isalnum */
    emit_indent();
    printf("(func $isalnum (param $c i32) (result i32)\n");
    emit_indent();
    printf("  (i32.or (call $isdigit (local.get $c)) (call $isalpha (local.get $c)))\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* isspace */
    emit_indent();
    printf("(func $isspace (param $c i32) (result i32)\n");
    emit_indent();
    printf("  (i32.or\n");
    emit_indent();
    printf("    (i32.or\n");
    emit_indent();
    printf("      (i32.eq (local.get $c) (i32.const 32))\n");
    emit_indent();
    printf("      (i32.eq (local.get $c) (i32.const 9)))\n");
    emit_indent();
    printf("    (i32.or\n");
    emit_indent();
    printf("      (i32.eq (local.get $c) (i32.const 10))\n");
    emit_indent();
    printf("      (i32.or\n");
    emit_indent();
    printf("        (i32.eq (local.get $c) (i32.const 13))\n");
    emit_indent();
    printf("        (i32.eq (local.get $c) (i32.const 12)))))\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* isupper */
    emit_indent();
    printf("(func $isupper (param $c i32) (result i32)\n");
    emit_indent();
    printf("  (i32.and (i32.ge_u (local.get $c) (i32.const 65)) (i32.le_u (local.get $c) (i32.const 90)))\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* islower */
    emit_indent();
    printf("(func $islower (param $c i32) (result i32)\n");
    emit_indent();
    printf("  (i32.and (i32.ge_u (local.get $c) (i32.const 97)) (i32.le_u (local.get $c) (i32.const 122)))\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* isprint */
    emit_indent();
    printf("(func $isprint (param $c i32) (result i32)\n");
    emit_indent();
    printf("  (i32.and (i32.ge_u (local.get $c) (i32.const 32)) (i32.le_u (local.get $c) (i32.const 126)))\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* ispunct */
    emit_indent();
    printf("(func $ispunct (param $c i32) (result i32)\n");
    emit_indent();
    printf("  (i32.and (call $isprint (local.get $c))\n");
    emit_indent();
    printf("    (i32.and (i32.eqz (call $isalnum (local.get $c)))\n");
    emit_indent();
    printf("             (i32.ne (local.get $c) (i32.const 32))))\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* isxdigit */
    emit_indent();
    printf("(func $isxdigit (param $c i32) (result i32)\n");
    emit_indent();
    printf("  (i32.or (call $isdigit (local.get $c))\n");
    emit_indent();
    printf("    (i32.or\n");
    emit_indent();
    printf("      (i32.and (i32.ge_u (local.get $c) (i32.const 65)) (i32.le_u (local.get $c) (i32.const 70)))\n");
    emit_indent();
    printf("      (i32.and (i32.ge_u (local.get $c) (i32.const 97)) (i32.le_u (local.get $c) (i32.const 102)))))\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* toupper */
    emit_indent();
    printf("(func $toupper (param $c i32) (result i32)\n");
    emit_indent();
    printf("  (if (result i32) (call $islower (local.get $c))\n");
    emit_indent();
    printf("    (then (i32.sub (local.get $c) (i32.const 32)))\n");
    emit_indent();
    printf("    (else (local.get $c)))\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* tolower */
    emit_indent();
    printf("(func $tolower (param $c i32) (result i32)\n");
    emit_indent();
    printf("  (if (result i32) (call $isupper (local.get $c))\n");
    emit_indent();
    printf("    (then (i32.add (local.get $c) (i32.const 32)))\n");
    emit_indent();
    printf("    (else (local.get $c)))\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* abs */
    emit_indent();
    printf("(func $abs (param $n i32) (result i32)\n");
    emit_indent();
    printf("  (if (result i32) (i32.lt_s (local.get $n) (i32.const 0))\n");
    emit_indent();
    printf("    (then (i32.sub (i32.const 0) (local.get $n)))\n");
    emit_indent();
    printf("    (else (local.get $n)))\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* atoi */
    emit_indent();
    printf("(func $atoi (param $s i32) (result i32)\n");
    emit_indent();
    printf("  (local $n i32) (local $neg i32) (local $c i32)\n");
    emit_indent();
    printf("  (local.set $n (i32.const 0))\n");
    emit_indent();
    printf("  (local.set $neg (i32.const 0))\n");
    emit_indent();
    printf("  (block $dsp (loop $sp\n");
    emit_indent();
    printf("    (local.set $c (i32.load8_u (local.get $s)))\n");
    emit_indent();
    printf("    (br_if $dsp (i32.ne (local.get $c) (i32.const 32)))\n");
    emit_indent();
    printf("    (local.set $s (i32.add (local.get $s) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $sp)))\n");
    emit_indent();
    printf("  (local.set $c (i32.load8_u (local.get $s)))\n");
    emit_indent();
    printf("  (if (i32.eq (local.get $c) (i32.const 45))\n");
    emit_indent();
    printf("    (then (local.set $neg (i32.const 1))\n");
    emit_indent();
    printf("           (local.set $s (i32.add (local.get $s) (i32.const 1)))))\n");
    emit_indent();
    printf("  (if (i32.eq (local.get $c) (i32.const 43))\n");
    emit_indent();
    printf("    (then (local.set $s (i32.add (local.get $s) (i32.const 1)))))\n");
    emit_indent();
    printf("  (block $done (loop $dig\n");
    emit_indent();
    printf("    (local.set $c (i32.load8_u (local.get $s)))\n");
    emit_indent();
    printf("    (br_if $done (i32.or (i32.lt_u (local.get $c) (i32.const 48)) (i32.gt_u (local.get $c) (i32.const 57))))\n");
    emit_indent();
    printf("    (local.set $n (i32.add (i32.mul (local.get $n) (i32.const 10)) (i32.sub (local.get $c) (i32.const 48))))\n");
    emit_indent();
    printf("    (local.set $s (i32.add (local.get $s) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $dig)))\n");
    emit_indent();
    printf("  (if (result i32) (local.get $neg) (then (i32.sub (i32.const 0) (local.get $n))) (else (local.get $n)))\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* srand */
    emit_indent();
    printf("(func $srand (param $seed i32)\n");
    emit_indent();
    printf("  (global.set $__rand_seed (local.get $seed))\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* rand */
    emit_indent();
    printf("(func $rand (result i32)\n");
    emit_indent();
    printf("  (global.set $__rand_seed\n");
    emit_indent();
    printf("    (i32.add (i32.mul (global.get $__rand_seed) (i32.const 1103515245)) (i32.const 12345)))\n");
    emit_indent();
    printf("  (i32.and (i32.shr_u (global.get $__rand_seed) (i32.const 16)) (i32.const 32767))\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* calloc */
    emit_indent();
    printf("(func $calloc (param $nmemb i32) (param $size i32) (result i32)\n");
    emit_indent();
    printf("  (local $ptr i32) (local $total i32)\n");
    emit_indent();
    printf("  (local.set $total (i32.mul (local.get $nmemb) (local.get $size)))\n");
    emit_indent();
    printf("  (local.set $ptr (call $malloc (local.get $total)))\n");
    emit_indent();
    printf("  (drop (call $memset (local.get $ptr) (i32.const 0) (local.get $total)))\n");
    emit_indent();
    printf("  (local.get $ptr)\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* strcpy */
    emit_indent();
    printf("(func $strcpy (param $dst i32) (param $src i32) (result i32)\n");
    emit_indent();
    printf("  (local $d i32)\n");
    emit_indent();
    printf("  (local.set $d (local.get $dst))\n");
    emit_indent();
    printf("  (block $done (loop $copy\n");
    emit_indent();
    printf("    (i32.store8 (local.get $d) (i32.load8_u (local.get $src)))\n");
    emit_indent();
    printf("    (br_if $done (i32.eqz (i32.load8_u (local.get $src))))\n");
    emit_indent();
    printf("    (local.set $d (i32.add (local.get $d) (i32.const 1)))\n");
    emit_indent();
    printf("    (local.set $src (i32.add (local.get $src) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $copy)))\n");
    emit_indent();
    printf("  (local.get $dst)\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* strcat */
    emit_indent();
    printf("(func $strcat (param $dst i32) (param $src i32) (result i32)\n");
    emit_indent();
    printf("  (local $d i32)\n");
    emit_indent();
    printf("  (local.set $d (i32.add (local.get $dst) (call $strlen (local.get $dst))))\n");
    emit_indent();
    printf("  (drop (call $strcpy (local.get $d) (local.get $src)))\n");
    emit_indent();
    printf("  (local.get $dst)\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* strchr */
    emit_indent();
    printf("(func $strchr (param $s i32) (param $c i32) (result i32)\n");
    emit_indent();
    printf("  (local $ch i32)\n");
    emit_indent();
    printf("  (block $done (loop $scan\n");
    emit_indent();
    printf("    (local.set $ch (i32.load8_u (local.get $s)))\n");
    emit_indent();
    printf("    (if (i32.eq (local.get $ch) (local.get $c)) (then (return (local.get $s))))\n");
    emit_indent();
    printf("    (br_if $done (i32.eqz (local.get $ch)))\n");
    emit_indent();
    printf("    (local.set $s (i32.add (local.get $s) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $scan)))\n");
    emit_indent();
    printf("  (i32.const 0)\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* strrchr */
    emit_indent();
    printf("(func $strrchr (param $s i32) (param $c i32) (result i32)\n");
    emit_indent();
    printf("  (local $last i32) (local $ch i32)\n");
    emit_indent();
    printf("  (local.set $last (i32.const 0))\n");
    emit_indent();
    printf("  (block $done (loop $scan\n");
    emit_indent();
    printf("    (local.set $ch (i32.load8_u (local.get $s)))\n");
    emit_indent();
    printf("    (if (i32.eq (local.get $ch) (local.get $c)) (then (local.set $last (local.get $s))))\n");
    emit_indent();
    printf("    (br_if $done (i32.eqz (local.get $ch)))\n");
    emit_indent();
    printf("    (local.set $s (i32.add (local.get $s) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $scan)))\n");
    emit_indent();
    printf("  (local.get $last)\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* strstr */
    emit_indent();
    printf("(func $strstr (param $hay i32) (param $needle i32) (result i32)\n");
    emit_indent();
    printf("  (local $h i32) (local $n i32)\n");
    emit_indent();
    printf("  (if (i32.eqz (i32.load8_u (local.get $needle))) (then (return (local.get $hay))))\n");
    emit_indent();
    printf("  (block $notfound (loop $outer\n");
    emit_indent();
    printf("    (br_if $notfound (i32.eqz (i32.load8_u (local.get $hay))))\n");
    emit_indent();
    printf("    (local.set $h (local.get $hay))\n");
    emit_indent();
    printf("    (local.set $n (local.get $needle))\n");
    emit_indent();
    printf("    (block $nomatch (loop $inner\n");
    emit_indent();
    printf("      (if (i32.eqz (i32.load8_u (local.get $n))) (then (return (local.get $hay))))\n");
    emit_indent();
    printf("      (br_if $nomatch (i32.ne (i32.load8_u (local.get $h)) (i32.load8_u (local.get $n))))\n");
    emit_indent();
    printf("      (local.set $h (i32.add (local.get $h) (i32.const 1)))\n");
    emit_indent();
    printf("      (local.set $n (i32.add (local.get $n) (i32.const 1)))\n");
    emit_indent();
    printf("      (br $inner)))\n");
    emit_indent();
    printf("    (local.set $hay (i32.add (local.get $hay) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $outer)))\n");
    emit_indent();
    printf("  (i32.const 0)\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* strncmp */
    emit_indent();
    printf("(func $strncmp (param $a i32) (param $b i32) (param $n i32) (result i32)\n");
    emit_indent();
    printf("  (local $i i32) (local $ca i32) (local $cb i32)\n");
    emit_indent();
    printf("  (local.set $i (i32.const 0))\n");
    emit_indent();
    printf("  (block $done (loop $cmp\n");
    emit_indent();
    printf("    (br_if $done (i32.ge_u (local.get $i) (local.get $n)))\n");
    emit_indent();
    printf("    (local.set $ca (i32.load8_u (local.get $a)))\n");
    emit_indent();
    printf("    (local.set $cb (i32.load8_u (local.get $b)))\n");
    emit_indent();
    printf("    (if (i32.ne (local.get $ca) (local.get $cb))\n");
    emit_indent();
    printf("      (then (return (i32.sub (local.get $ca) (local.get $cb)))))\n");
    emit_indent();
    printf("    (br_if $done (i32.eqz (local.get $ca)))\n");
    emit_indent();
    printf("    (local.set $a (i32.add (local.get $a) (i32.const 1)))\n");
    emit_indent();
    printf("    (local.set $b (i32.add (local.get $b) (i32.const 1)))\n");
    emit_indent();
    printf("    (local.set $i (i32.add (local.get $i) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $cmp)))\n");
    emit_indent();
    printf("  (i32.const 0)\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* strncat */
    emit_indent();
    printf("(func $strncat (param $dst i32) (param $src i32) (param $n i32) (result i32)\n");
    emit_indent();
    printf("  (local $d i32) (local $i i32)\n");
    emit_indent();
    printf("  (local.set $d (i32.add (local.get $dst) (call $strlen (local.get $dst))))\n");
    emit_indent();
    printf("  (local.set $i (i32.const 0))\n");
    emit_indent();
    printf("  (block $done (loop $cp\n");
    emit_indent();
    printf("    (br_if $done (i32.ge_u (local.get $i) (local.get $n)))\n");
    emit_indent();
    printf("    (br_if $done (i32.eqz (i32.load8_u (local.get $src))))\n");
    emit_indent();
    printf("    (i32.store8 (local.get $d) (i32.load8_u (local.get $src)))\n");
    emit_indent();
    printf("    (local.set $d (i32.add (local.get $d) (i32.const 1)))\n");
    emit_indent();
    printf("    (local.set $src (i32.add (local.get $src) (i32.const 1)))\n");
    emit_indent();
    printf("    (local.set $i (i32.add (local.get $i) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $cp)))\n");
    emit_indent();
    printf("  (i32.store8 (local.get $d) (i32.const 0))\n");
    emit_indent();
    printf("  (local.get $dst)\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* memmove */
    emit_indent();
    printf("(func $memmove (param $dst i32) (param $src i32) (param $n i32) (result i32)\n");
    emit_indent();
    printf("  (local $i i32)\n");
    emit_indent();
    printf("  (if (i32.le_u (local.get $dst) (local.get $src))\n");
    emit_indent();
    printf("    (then (drop (call $memcpy (local.get $dst) (local.get $src) (local.get $n))))\n");
    emit_indent();
    printf("    (else\n");
    emit_indent();
    printf("      (local.set $i (local.get $n))\n");
    emit_indent();
    printf("      (block $done (loop $bk\n");
    emit_indent();
    printf("        (br_if $done (i32.eqz (local.get $i)))\n");
    emit_indent();
    printf("        (local.set $i (i32.sub (local.get $i) (i32.const 1)))\n");
    emit_indent();
    printf("        (i32.store8 (i32.add (local.get $dst) (local.get $i))\n");
    emit_indent();
    printf("                    (i32.load8_u (i32.add (local.get $src) (local.get $i))))\n");
    emit_indent();
    printf("        (br $bk)))))\n");
    emit_indent();
    printf("  (local.get $dst)\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* memchr */
    emit_indent();
    printf("(func $memchr (param $s i32) (param $c i32) (param $n i32) (result i32)\n");
    emit_indent();
    printf("  (local $i i32)\n");
    emit_indent();
    printf("  (local.set $i (i32.const 0))\n");
    emit_indent();
    printf("  (block $done (loop $scan\n");
    emit_indent();
    printf("    (br_if $done (i32.ge_u (local.get $i) (local.get $n)))\n");
    emit_indent();
    printf("    (if (i32.eq (i32.load8_u (i32.add (local.get $s) (local.get $i))) (local.get $c))\n");
    emit_indent();
    printf("      (then (return (i32.add (local.get $s) (local.get $i)))))\n");
    emit_indent();
    printf("    (local.set $i (i32.add (local.get $i) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $scan)))\n");
    emit_indent();
    printf("  (i32.const 0)\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* puts */
    emit_indent();
    printf("(func $puts (param $s i32) (result i32)\n");
    emit_indent();
    printf("  (local $c i32)\n");
    emit_indent();
    printf("  (block $done (loop $pr\n");
    emit_indent();
    printf("    (local.set $c (i32.load8_u (local.get $s)))\n");
    emit_indent();
    printf("    (br_if $done (i32.eqz (local.get $c)))\n");
    emit_indent();
    printf("    (drop (call $putchar (local.get $c)))\n");
    emit_indent();
    printf("    (local.set $s (i32.add (local.get $s) (i32.const 1)))\n");
    emit_indent();
    printf("    (br $pr)))\n");
    emit_indent();
    printf("  (drop (call $putchar (i32.const 10)))\n");
    emit_indent();
    printf("  (i32.const 0)\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* strtol (simplified: base 10 only, ignores endptr) */
    emit_indent();
    printf("(func $strtol (param $s i32) (param $endptr i32) (param $base i32) (result i32)\n");
    emit_indent();
    printf("  (call $atoi (local.get $s))\n");
    emit_indent();
    printf(")\n");
    emit_indent();
    printf("\n");

    /* user functions */
    for (i = 0; i < prog->ival2; i++) {
        gen_func(prog->list[i]);
    }

    emit_indent();
    printf("\n");

    /* _start — calls __init if needed, then main */
    {
        int need_init;
        int gi2;
        need_init = 0;
        for (gi2 = 0; gi2 < nglobals; gi2++) {
            if (globals_tbl[gi2]->gv_arr_len > 0 && globals_tbl[gi2]->gv_arr_str_ids != (int *)0) {
                need_init = 1;
            }
        }
        if (need_init) {
            emit_indent();
            printf("(func $__init\n");
            indent_level++;
            emit_indent();
            printf("(local $__ptr i32)\n");
            for (gi2 = 0; gi2 < nglobals; gi2++) {
                if (globals_tbl[gi2]->gv_arr_len > 0 && globals_tbl[gi2]->gv_arr_str_ids != (int *)0) {
                    int ai2;
                    /* allocate arr_len * 4 bytes */
                    emit_indent();
                    printf("i32.const %d\n", globals_tbl[gi2]->gv_arr_len * 4);
                    emit_indent();
                    printf("call $malloc\n");
                    emit_indent();
                    printf("local.tee $__ptr\n");
                    emit_indent();
                    printf("global.set $%s\n", globals_tbl[gi2]->name);
                    /* store each element */
                    for (ai2 = 0; ai2 < globals_tbl[gi2]->gv_arr_len; ai2++) {
                        emit_indent();
                        printf("local.get $__ptr\n");
                        if (ai2 > 0) {
                            emit_indent();
                            printf("i32.const %d\n", ai2 * 4);
                            emit_indent();
                            printf("i32.add\n");
                        }
                        emit_indent();
                        printf("i32.const %d\n", str_table[globals_tbl[gi2]->gv_arr_str_ids[ai2]]->offset);
                        emit_indent();
                        printf("i32.store\n");
                    }
                }
            }
            indent_level--;
            emit_indent();
            printf(")\n");
        }
        emit_indent();
        printf("(func $_start (export \"_start\")\n");
        indent_level++;
        if (need_init) {
            emit_indent();
            printf("call $__init\n");
        }
        emit_indent();
        printf("call $main\n");
        emit_indent();
        printf("call $__proc_exit\n");
        indent_level--;
        emit_indent();
        printf(")\n");
    }

    indent_level--;
    emit_indent();
    printf(")\n");
}

/* ================================================================
 * WASM Binary Emitter
 * ================================================================ */

int binary_mode = 0;

/* --- Growable byte buffer --- */

struct ByteVec {
    char *data;
    int len;
    int cap;
};

void bv_init(struct ByteVec *v, int cap) {
    v->data = (char *)malloc(cap);
    v->len = 0;
    v->cap = cap;
}

struct ByteVec *bv_new(int cap) {
    struct ByteVec *v;
    v = (struct ByteVec *)malloc(sizeof(struct ByteVec));
    bv_init(v, cap);
    return v;
}

void bv_reset(struct ByteVec *v) {
    v->len = 0;
}

void bv_push(struct ByteVec *v, int b) {
    char *nd;
    if (v->len >= v->cap) {
        nd = (char *)malloc(v->cap * 2);
        memcpy(nd, v->data, v->len);
        v->data = nd;
        v->cap = v->cap * 2;
    }
    v->data[v->len] = (char)(b & 0xFF);
    v->len++;
}

void bv_append(struct ByteVec *dst, struct ByteVec *src) {
    int i;
    for (i = 0; i < src->len; i++) {
        bv_push(dst, src->data[i] & 0xFF);
    }
}

void bv_u32(struct ByteVec *v, int val) {
    int b;
    if (val < 0) val = 0;
    do {
        b = val & 0x7F;
        val = val >> 7;
        if (val != 0) b = b | 0x80;
        bv_push(v, b);
    } while (val != 0);
}

void bv_i32(struct ByteVec *v, int val) {
    int b;
    int more;
    more = 1;
    while (more) {
        b = val & 0x7F;
        val = val >> 7;
        if ((val == 0 && (b & 0x40) == 0) || (val == -1 && (b & 0x40) != 0)) {
            more = 0;
        } else {
            b = b | 0x80;
        }
        bv_push(v, b);
    }
}

void bv_name(struct ByteVec *v, char *s, int slen) {
    int i;
    bv_u32(v, slen);
    for (i = 0; i < slen; i++) {
        bv_push(v, s[i] & 0xFF);
    }
}

void bv_section(struct ByteVec *out, int id, struct ByteVec *content) {
    bv_push(out, id);
    bv_u32(out, content->len);
    bv_append(out, content);
}

/* --- Binary type deduplication --- */

#define MAX_BIN_TYPES 64
#define MAX_TYPE_PARAMS 16

struct BinTypeEntry {
    int nparams;
    int has_result;
    int result_is_float;
    int *param_is_float;  /* malloc'd array of per-param float flags */
};

struct BinTypeEntry **bin_types;
int bin_ntypes;

int bin_find_or_add_type_f(int nparams, int *pif, int has_result, int rif) {
    int i;
    int j;
    int match;
    for (i = 0; i < bin_ntypes; i++) {
        if (bin_types[i]->nparams != nparams) continue;
        if (bin_types[i]->has_result != has_result) continue;
        if (bin_types[i]->result_is_float != rif) continue;
        match = 1;
        for (j = 0; j < nparams && j < 16; j++) {
            if (bin_types[i]->param_is_float[j] != pif[j]) { match = 0; break; }
        }
        if (match) return i;
    }
    if (bin_ntypes >= MAX_BIN_TYPES) {
        printf("too many binary types\n");
        exit(1);
    }
    bin_types[bin_ntypes] = (struct BinTypeEntry *)malloc(sizeof(struct BinTypeEntry));
    bin_types[bin_ntypes]->nparams = nparams;
    bin_types[bin_ntypes]->has_result = has_result;
    bin_types[bin_ntypes]->result_is_float = rif;
    bin_types[bin_ntypes]->param_is_float = (int *)malloc(16 * sizeof(int));
    for (j = 0; j < nparams && j < 16; j++) {
        bin_types[bin_ntypes]->param_is_float[j] = pif[j];
    }
    bin_ntypes++;
    return bin_ntypes - 1;
}

int bin_find_or_add_type(int nparams, int has_result) {
    int zeros[16];
    int j;
    for (j = 0; j < 16; j++) zeros[j] = 0;
    return bin_find_or_add_type_f(nparams, zeros, has_result, 0);
}

/* --- Binary function index table --- */

#define MAX_BIN_FUNCS 512

struct BinFuncEntry {
    char *name;
    int idx;
    int nparams;
    int has_result;
    int type_idx;
};

struct BinFuncEntry **bin_funcs;
int bin_nfuncs;

int bin_add_func(char *name, int nparams, int has_result) {
    int ti;
    if (bin_nfuncs >= MAX_BIN_FUNCS) {
        printf("too many binary functions\n");
        exit(1);
    }
    ti = bin_find_or_add_type(nparams, has_result);
    bin_funcs[bin_nfuncs] = (struct BinFuncEntry *)malloc(sizeof(struct BinFuncEntry));
    bin_funcs[bin_nfuncs]->name = name;
    bin_funcs[bin_nfuncs]->idx = bin_nfuncs;
    bin_funcs[bin_nfuncs]->nparams = nparams;
    bin_funcs[bin_nfuncs]->has_result = has_result;
    bin_funcs[bin_nfuncs]->type_idx = ti;
    bin_nfuncs++;
    return bin_nfuncs - 1;
}

int bin_add_func_f(char *name, int nparams, int *pif, int has_result, int rif) {
    int ti;
    if (bin_nfuncs >= MAX_BIN_FUNCS) {
        printf("too many binary functions\n");
        exit(1);
    }
    ti = bin_find_or_add_type_f(nparams, pif, has_result, rif);
    bin_funcs[bin_nfuncs] = (struct BinFuncEntry *)malloc(sizeof(struct BinFuncEntry));
    bin_funcs[bin_nfuncs]->name = name;
    bin_funcs[bin_nfuncs]->idx = bin_nfuncs;
    bin_funcs[bin_nfuncs]->nparams = nparams;
    bin_funcs[bin_nfuncs]->has_result = has_result;
    bin_funcs[bin_nfuncs]->type_idx = ti;
    bin_nfuncs++;
    return bin_nfuncs - 1;
}

int bin_find_func(char *name) {
    int i;
    for (i = 0; i < bin_nfuncs; i++) {
        if (strcmp(bin_funcs[i]->name, name) == 0) return i;
    }
    return 0;
}

/* --- Binary global index --- */

int bin_global_idx(char *name) {
    int i;
    if (strcmp(name, "__heap_ptr") == 0) return 0;
    if (strcmp(name, "__free_list") == 0) return 1;
    if (strcmp(name, "__rand_seed") == 0) return 2;
    for (i = 0; i < nglobals; i++) {
        if (strcmp(globals_tbl[i]->name, name) == 0) return i + 3;
    }
    return 0;
}

/* --- Binary label depth tracking --- */

int bin_lbl_sp;
int *bin_brk_at;
int *bin_cont_at;
int *bin_loop_at;

int bin_last_float; /* 0=i32, 2=f64 - set by gen_expr_bin */

int find_funcsig(char *name) {
    int i;
    for (i = 0; i < nfunc_sigs; i++) {
        if (strcmp(func_sigs[i]->name, name) == 0) return i;
    }
    return -1;
}

/* --- IEEE 754 float helpers --- */

double parse_float_text(char *s) {
    double result;
    double frac_mult;
    int sign;
    int exp_val;
    int exp_sign;
    int i;
    result = 0.0;
    sign = 1;
    exp_val = 0;
    exp_sign = 1;
    i = 0;
    if (s[i] == '-') { sign = -1; i = i + 1; }
    else if (s[i] == '+') { i = i + 1; }
    while (s[i] >= '0' && s[i] <= '9') {
        result = result * 10.0 + (double)(s[i] - '0');
        i = i + 1;
    }
    if (s[i] == '.') {
        i = i + 1;
        frac_mult = 0.1;
        while (s[i] >= '0' && s[i] <= '9') {
            result = result + (double)(s[i] - '0') * frac_mult;
            frac_mult = frac_mult * 0.1;
            i = i + 1;
        }
    }
    if (s[i] == 'e' || s[i] == 'E') {
        i = i + 1;
        if (s[i] == '-') { exp_sign = -1; i = i + 1; }
        else if (s[i] == '+') { i = i + 1; }
        while (s[i] >= '0' && s[i] <= '9') {
            exp_val = exp_val * 10 + (s[i] - '0');
            i = i + 1;
        }
    }
    if (exp_sign > 0) {
        while (exp_val > 0) { result = result * 10.0; exp_val = exp_val - 1; }
    } else {
        while (exp_val > 0) { result = result * 0.1; exp_val = exp_val - 1; }
    }
    if (sign < 0) { result = result * -1.0; }
    return result;
}

void emit_f64_const_bin(struct ByteVec *o, char *s) {
    double *p;
    char *b;
    int i;
    p = (double *)malloc(8);
    *p = parse_float_text(s);
    b = (char *)p;
    bv_push(o, 0x44);
    for (i = 0; i < 8; i++) {
        bv_push(o, b[i] & 255);
    }
}

/* --- Binary expression codegen --- */

void gen_expr_bin(struct ByteVec *o, struct Node *n) {
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
    int li;

    switch (n->kind) {
    case ND_INT_LIT:
        bv_push(o, 0x41); bv_i32(o, n->ival);
        bin_last_float = 0;
        break;
    case ND_FLOAT_LIT:
        emit_f64_const_bin(o, n->sval);
        bin_last_float = 2;
        break;
    case ND_CAST:
        gen_expr_bin(o, n->c0);
        if (n->ival >= 1 && !bin_last_float) {
            bv_push(o, 0xB7); /* f64.convert_i32_s */
            bin_last_float = 2;
        } else if (n->ival == 0 && bin_last_float) {
            bv_push(o, 0xAA); /* i32.trunc_f64_s */
            bin_last_float = 0;
        }
        break;
    case ND_IDENT:
        if (find_global(n->sval) >= 0) {
            bv_push(o, 0x23); bv_u32(o, bin_global_idx(n->sval));
        } else {
            bv_push(o, 0x20); bv_u32(o, find_local(n->sval));
        }
        bin_last_float = var_is_float(n->sval);
        break;
    case ND_ASSIGN:
        tgt = n->c0;
        if (tgt->kind == ND_IDENT) {
            int tgt_float;
            int ftmp_li;
            int atmp_li;
            name = tgt->sval;
            is_global = (find_global(name) >= 0);
            tgt_float = var_is_float(name);
            if (n->ival == TOK_EQ) {
                gen_expr_bin(o, n->c1);
                /* convert if needed */
                if (tgt_float && !bin_last_float) {
                    bv_push(o, 0xB7); /* f64.convert_i32_s */
                    bin_last_float = 2;
                } else if (!tgt_float && bin_last_float) {
                    bv_push(o, 0xAA); /* i32.trunc_f64_s */
                    bin_last_float = 0;
                }
            } else if (n->ival == TOK_PLUS_EQ) {
                if (is_global) {
                    bv_push(o, 0x23); bv_u32(o, bin_global_idx(name));
                } else {
                    bv_push(o, 0x20); bv_u32(o, find_local(name));
                }
                gen_expr_bin(o, n->c1);
                if (tgt_float && !bin_last_float) { bv_push(o, 0xB7); } /* int->f64 */
                else if (!tgt_float && bin_last_float) { bv_push(o, 0xAA); } /* f64->int */
                if (tgt_float) { bv_push(o, 0xA0); } else { bv_push(o, 0x6A); }
            } else if (n->ival == TOK_MINUS_EQ) {
                if (is_global) {
                    bv_push(o, 0x23); bv_u32(o, bin_global_idx(name));
                } else {
                    bv_push(o, 0x20); bv_u32(o, find_local(name));
                }
                gen_expr_bin(o, n->c1);
                if (tgt_float && !bin_last_float) { bv_push(o, 0xB7); } /* int->f64 */
                else if (!tgt_float && bin_last_float) { bv_push(o, 0xAA); } /* f64->int */
                if (tgt_float) { bv_push(o, 0xA1); } else { bv_push(o, 0x6B); }
            } else if (n->ival == TOK_PIPE_EQ || n->ival == TOK_AMP_EQ ||
                       n->ival == TOK_CARET_EQ || n->ival == TOK_LSHIFT_EQ ||
                       n->ival == TOK_RSHIFT_EQ) {
                if (is_global) {
                    bv_push(o, 0x23); bv_u32(o, bin_global_idx(name));
                } else {
                    bv_push(o, 0x20); bv_u32(o, find_local(name));
                }
                gen_expr_bin(o, n->c1);
                if (n->ival == TOK_PIPE_EQ) { bv_push(o, 0x72); }
                else if (n->ival == TOK_AMP_EQ) { bv_push(o, 0x71); }
                else if (n->ival == TOK_CARET_EQ) { bv_push(o, 0x73); }
                else if (n->ival == TOK_LSHIFT_EQ) { bv_push(o, 0x74); }
                else { bv_push(o, 0x75); }
            }
            if (is_global) {
                if (tgt_float) {
                    ftmp_li = find_local("__ftmp");
                    bv_push(o, 0x21); bv_u32(o, ftmp_li);
                    bv_push(o, 0x20); bv_u32(o, ftmp_li);
                    bv_push(o, 0x24); bv_u32(o, bin_global_idx(name));
                    bv_push(o, 0x20); bv_u32(o, ftmp_li);
                } else {
                    atmp_li = find_local("__atmp");
                    bv_push(o, 0x21); bv_u32(o, atmp_li);
                    bv_push(o, 0x20); bv_u32(o, atmp_li);
                    bv_push(o, 0x24); bv_u32(o, bin_global_idx(name));
                    bv_push(o, 0x20); bv_u32(o, atmp_li);
                }
            } else {
                if (tgt_float) {
                    bv_push(o, 0x21); bv_u32(o, find_local(name)); /* local.set */
                    bv_push(o, 0x20); bv_u32(o, find_local(name)); /* local.get */
                } else {
                    bv_push(o, 0x22); bv_u32(o, find_local(name)); /* local.tee */
                }
            }
            bin_last_float = tgt_float;
        } else if (tgt->kind == ND_UNARY && tgt->ival == TOK_STAR) {
            esz = expr_elem_size(tgt->c0);
            if (n->ival == TOK_EQ) {
                gen_expr_bin(o, n->c1);
            } else if (n->ival == TOK_PLUS_EQ) {
                gen_expr_bin(o, tgt->c0);
                if (esz == 1) { bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0); }
                else if (esz == 2) { bv_push(o, 0x2E); bv_u32(o, 1); bv_u32(o, 0); }
                else { bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0); }
                gen_expr_bin(o, n->c1);
                bv_push(o, 0x6A);
            } else if (n->ival == TOK_MINUS_EQ) {
                gen_expr_bin(o, tgt->c0);
                if (esz == 1) { bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0); }
                else if (esz == 2) { bv_push(o, 0x2E); bv_u32(o, 1); bv_u32(o, 0); }
                else { bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0); }
                gen_expr_bin(o, n->c1);
                bv_push(o, 0x6B);
            } else if (n->ival == TOK_PIPE_EQ || n->ival == TOK_AMP_EQ ||
                       n->ival == TOK_CARET_EQ || n->ival == TOK_LSHIFT_EQ ||
                       n->ival == TOK_RSHIFT_EQ) {
                gen_expr_bin(o, tgt->c0);
                if (esz == 1) { bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0); }
                else if (esz == 2) { bv_push(o, 0x2E); bv_u32(o, 1); bv_u32(o, 0); }
                else { bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0); }
                gen_expr_bin(o, n->c1);
                if (n->ival == TOK_PIPE_EQ) { bv_push(o, 0x72); }
                else if (n->ival == TOK_AMP_EQ) { bv_push(o, 0x71); }
                else if (n->ival == TOK_CARET_EQ) { bv_push(o, 0x73); }
                else if (n->ival == TOK_LSHIFT_EQ) { bv_push(o, 0x74); }
                else { bv_push(o, 0x75); }
            }
            li = find_local("__atmp");
            bv_push(o, 0x21); bv_u32(o, li);
            gen_expr_bin(o, tgt->c0);
            bv_push(o, 0x20); bv_u32(o, li);
            if (esz == 1) { bv_push(o, 0x3A); bv_u32(o, 0); bv_u32(o, 0); }
            else if (esz == 2) { bv_push(o, 0x3B); bv_u32(o, 1); bv_u32(o, 0); }
            else { bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 0); }
            bv_push(o, 0x20); bv_u32(o, li);
        } else if (tgt->kind == ND_MEMBER) {
            off = resolve_field_offset(tgt->sval);
            if (off < 0) error(tgt->nline, tgt->ncol, "unknown struct field");
            if (n->ival == TOK_EQ) {
                gen_expr_bin(o, n->c1);
            } else if (n->ival == TOK_PLUS_EQ) {
                gen_expr_bin(o, tgt->c0);
                if (off > 0) { bv_push(o, 0x41); bv_i32(o, off); bv_push(o, 0x6A); }
                bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0);
                gen_expr_bin(o, n->c1);
                bv_push(o, 0x6A);
            } else if (n->ival == TOK_MINUS_EQ) {
                gen_expr_bin(o, tgt->c0);
                if (off > 0) { bv_push(o, 0x41); bv_i32(o, off); bv_push(o, 0x6A); }
                bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0);
                gen_expr_bin(o, n->c1);
                bv_push(o, 0x6B);
            } else if (n->ival == TOK_PIPE_EQ || n->ival == TOK_AMP_EQ ||
                       n->ival == TOK_CARET_EQ || n->ival == TOK_LSHIFT_EQ ||
                       n->ival == TOK_RSHIFT_EQ) {
                gen_expr_bin(o, tgt->c0);
                if (off > 0) { bv_push(o, 0x41); bv_i32(o, off); bv_push(o, 0x6A); }
                bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0);
                gen_expr_bin(o, n->c1);
                if (n->ival == TOK_PIPE_EQ) { bv_push(o, 0x72); }
                else if (n->ival == TOK_AMP_EQ) { bv_push(o, 0x71); }
                else if (n->ival == TOK_CARET_EQ) { bv_push(o, 0x73); }
                else if (n->ival == TOK_LSHIFT_EQ) { bv_push(o, 0x74); }
                else { bv_push(o, 0x75); }
            }
            li = find_local("__atmp");
            bv_push(o, 0x21); bv_u32(o, li);
            gen_expr_bin(o, tgt->c0);
            if (off > 0) { bv_push(o, 0x41); bv_i32(o, off); bv_push(o, 0x6A); }
            bv_push(o, 0x20); bv_u32(o, li);
            bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 0);
            bv_push(o, 0x20); bv_u32(o, li);
        } else if (tgt->kind == ND_SUBSCRIPT) {
            esz = expr_elem_size(tgt->c0);
            if (n->ival == TOK_EQ) {
                gen_expr_bin(o, n->c1);
            } else if (n->ival == TOK_PLUS_EQ) {
                gen_expr_bin(o, tgt->c0);
                gen_expr_bin(o, tgt->c1);
                if (esz > 1) { bv_push(o, 0x41); bv_i32(o, esz); bv_push(o, 0x6C); }
                bv_push(o, 0x6A);
                if (esz == 1) { bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0); }
                else if (esz == 2) { bv_push(o, 0x2E); bv_u32(o, 1); bv_u32(o, 0); }
                else { bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0); }
                gen_expr_bin(o, n->c1);
                bv_push(o, 0x6A);
            } else if (n->ival == TOK_MINUS_EQ) {
                gen_expr_bin(o, tgt->c0);
                gen_expr_bin(o, tgt->c1);
                if (esz > 1) { bv_push(o, 0x41); bv_i32(o, esz); bv_push(o, 0x6C); }
                bv_push(o, 0x6A);
                if (esz == 1) { bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0); }
                else if (esz == 2) { bv_push(o, 0x2E); bv_u32(o, 1); bv_u32(o, 0); }
                else { bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0); }
                gen_expr_bin(o, n->c1);
                bv_push(o, 0x6B);
            } else if (n->ival == TOK_PIPE_EQ || n->ival == TOK_AMP_EQ ||
                       n->ival == TOK_CARET_EQ || n->ival == TOK_LSHIFT_EQ ||
                       n->ival == TOK_RSHIFT_EQ) {
                gen_expr_bin(o, tgt->c0);
                gen_expr_bin(o, tgt->c1);
                if (esz > 1) { bv_push(o, 0x41); bv_i32(o, esz); bv_push(o, 0x6C); }
                bv_push(o, 0x6A);
                if (esz == 1) { bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0); }
                else if (esz == 2) { bv_push(o, 0x2E); bv_u32(o, 1); bv_u32(o, 0); }
                else { bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0); }
                gen_expr_bin(o, n->c1);
                if (n->ival == TOK_PIPE_EQ) { bv_push(o, 0x72); }
                else if (n->ival == TOK_AMP_EQ) { bv_push(o, 0x71); }
                else if (n->ival == TOK_CARET_EQ) { bv_push(o, 0x73); }
                else if (n->ival == TOK_LSHIFT_EQ) { bv_push(o, 0x74); }
                else { bv_push(o, 0x75); }
            }
            li = find_local("__atmp");
            bv_push(o, 0x21); bv_u32(o, li);
            gen_expr_bin(o, tgt->c0);
            gen_expr_bin(o, tgt->c1);
            if (esz > 1) { bv_push(o, 0x41); bv_i32(o, esz); bv_push(o, 0x6C); }
            bv_push(o, 0x6A);
            bv_push(o, 0x20); bv_u32(o, li);
            if (esz == 1) { bv_push(o, 0x3A); bv_u32(o, 0); bv_u32(o, 0); }
            else if (esz == 2) { bv_push(o, 0x3B); bv_u32(o, 1); bv_u32(o, 0); }
            else { bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 0); }
            bv_push(o, 0x20); bv_u32(o, li);
        }
        break;
    case ND_UNARY:
        if (n->ival == TOK_MINUS) {
            int atmp_neg;
            gen_expr_bin(o, n->c0);
            if (bin_last_float) {
                bv_push(o, 0x9A); /* f64.neg */
            } else {
                atmp_neg = find_local("__atmp");
                bv_push(o, 0x21); bv_u32(o, atmp_neg); /* local.set __atmp */
                bv_push(o, 0x41); bv_i32(o, 0);        /* i32.const 0 */
                bv_push(o, 0x20); bv_u32(o, atmp_neg); /* local.get __atmp */
                bv_push(o, 0x6B);                      /* i32.sub */
                bin_last_float = 0;
            }
        } else if (n->ival == TOK_BANG) {
            gen_expr_bin(o, n->c0);
            bv_push(o, 0x45);
        } else if (n->ival == TOK_TILDE) {
            bv_push(o, 0x41); bv_i32(o, -1);
            gen_expr_bin(o, n->c0);
            bv_push(o, 0x73);
        } else if (n->ival == TOK_STAR) {
            esz = expr_elem_size(n->c0);
            gen_expr_bin(o, n->c0);
            if (esz == 1) { bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0); }
            else if (esz == 2) { bv_push(o, 0x2E); bv_u32(o, 1); bv_u32(o, 0); }
            else { bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0); }
        } else if (n->ival == TOK_AMP) {
            error(n->nline, n->ncol, "cannot take address of this expression");
        }
        break;
    case ND_BINARY: {
        int left_float;
        int right_float;
        int ftmp_bin;
        gen_expr_bin(o, n->c0);
        left_float = bin_last_float;
        gen_expr_bin(o, n->c1);
        right_float = bin_last_float;
        if (left_float || right_float) {
            if (left_float && !right_float) {
                /* stack: [f64, i32] - convert right(i32) to f64 */
                bv_push(o, 0xB7); /* f64.convert_i32_s */
            } else if (!left_float && right_float) {
                /* stack: [i32, f64] - save right, convert left, restore right */
                ftmp_bin = find_local("__ftmp");
                bv_push(o, 0x21); bv_u32(o, ftmp_bin); /* local.set __ftmp = right */
                bv_push(o, 0xB7); /* f64.convert_i32_s converts left */
                bv_push(o, 0x20); bv_u32(o, ftmp_bin); /* local.get __ftmp */
            }
            if (n->ival == TOK_PLUS) { bv_push(o, 0xA0); bin_last_float = 2; }
            else if (n->ival == TOK_MINUS) { bv_push(o, 0xA1); bin_last_float = 2; }
            else if (n->ival == TOK_STAR) { bv_push(o, 0xA2); bin_last_float = 2; }
            else if (n->ival == TOK_SLASH) { bv_push(o, 0xA3); bin_last_float = 2; }
            else if (n->ival == TOK_EQ_EQ) { bv_push(o, 0x61); bin_last_float = 0; }
            else if (n->ival == TOK_BANG_EQ) { bv_push(o, 0x62); bin_last_float = 0; }
            else if (n->ival == TOK_LT) { bv_push(o, 0x63); bin_last_float = 0; }
            else if (n->ival == TOK_GT) { bv_push(o, 0x64); bin_last_float = 0; }
            else if (n->ival == TOK_LT_EQ) { bv_push(o, 0x65); bin_last_float = 0; }
            else if (n->ival == TOK_GT_EQ) { bv_push(o, 0x66); bin_last_float = 0; }
            else { error(n->nline, n->ncol, "unsupported float binary op"); }
        } else {
            if (n->ival == TOK_PLUS) { bv_push(o, 0x6A); }
            else if (n->ival == TOK_MINUS) { bv_push(o, 0x6B); }
            else if (n->ival == TOK_STAR) { bv_push(o, 0x6C); }
            else if (n->ival == TOK_SLASH) { if (expr_is_unsigned(n->c0)) { bv_push(o, 0x6E); } else { bv_push(o, 0x6D); } }
            else if (n->ival == TOK_PERCENT) { if (expr_is_unsigned(n->c0)) { bv_push(o, 0x70); } else { bv_push(o, 0x6F); } }
            else if (n->ival == TOK_EQ_EQ) { bv_push(o, 0x46); }
            else if (n->ival == TOK_BANG_EQ) { bv_push(o, 0x47); }
            else if (n->ival == TOK_LT) { if (expr_is_unsigned(n->c0)) { bv_push(o, 0x49); } else { bv_push(o, 0x48); } }
            else if (n->ival == TOK_GT) { if (expr_is_unsigned(n->c0)) { bv_push(o, 0x4B); } else { bv_push(o, 0x4A); } }
            else if (n->ival == TOK_LT_EQ) { if (expr_is_unsigned(n->c0)) { bv_push(o, 0x4D); } else { bv_push(o, 0x4C); } }
            else if (n->ival == TOK_GT_EQ) { if (expr_is_unsigned(n->c0)) { bv_push(o, 0x4F); } else { bv_push(o, 0x4E); } }
            else if (n->ival == TOK_AMP_AMP) { bv_push(o, 0x71); }
            else if (n->ival == TOK_PIPE_PIPE) { bv_push(o, 0x72); }
            else if (n->ival == TOK_AMP) { bv_push(o, 0x71); }
            else if (n->ival == TOK_PIPE) { bv_push(o, 0x72); }
            else if (n->ival == TOK_LSHIFT) { bv_push(o, 0x74); }
            else if (n->ival == TOK_RSHIFT) { if (expr_is_unsigned(n->c0)) { bv_push(o, 0x76); } else { bv_push(o, 0x75); } }
            else if (n->ival == TOK_CARET) { bv_push(o, 0x73); }
            else { error(n->nline, n->ncol, "unsupported binary operator"); }
            bin_last_float = 0;
        }
        break;
    }
    case ND_CALL:
        if (strcmp(n->sval, "printf") == 0) {
            if (n->ival2 < 1 || n->list[0]->kind != ND_STR_LIT) {
                error(n->nline, n->ncol, "printf requires string literal format");
            }
            sid = n->list[0]->ival;
            fmt = str_table[sid]->data;
            flen = str_table[sid]->len;
            ai = 1;
            for (fi = 0; fi < flen; fi++) {
                if (fmt[fi] == '%' && fi + 1 < flen) {
                    fi++;
                    if (fmt[fi] == 'd') {
                        if (ai >= n->ival2) error(n->nline, n->ncol, "printf: missing arg for %d");
                        gen_expr_bin(o, n->list[ai]);
                        ai++;
                        bv_push(o, 0x10); bv_u32(o, bin_find_func("__print_int"));
                    } else if (fmt[fi] == 's') {
                        if (ai >= n->ival2) error(n->nline, n->ncol, "printf: missing arg for %s");
                        gen_expr_bin(o, n->list[ai]);
                        ai++;
                        bv_push(o, 0x10); bv_u32(o, bin_find_func("__print_str"));
                    } else if (fmt[fi] == 'c') {
                        if (ai >= n->ival2) error(n->nline, n->ncol, "printf: missing arg for %c");
                        gen_expr_bin(o, n->list[ai]);
                        ai++;
                        bv_push(o, 0x10); bv_u32(o, bin_find_func("putchar"));
                        bv_push(o, 0x1A);
                    } else if (fmt[fi] == 'x') {
                        if (ai >= n->ival2) error(n->nline, n->ncol, "printf: missing arg for %x");
                        gen_expr_bin(o, n->list[ai]);
                        ai++;
                        bv_push(o, 0x10); bv_u32(o, bin_find_func("__print_hex"));
                    } else if (fmt[fi] == 'f') {
                        if (ai >= n->ival2) error(n->nline, n->ncol, "printf: missing arg for %f");
                        gen_expr_bin(o, n->list[ai]);
                        ai++;
                        if (!bin_last_float) {
                            bv_push(o, 0xB7); /* f64.convert_i32_s */
                        }
                        bv_push(o, 0x10); bv_u32(o, bin_find_func("__print_float"));
                    } else if (fmt[fi] == '%') {
                        bv_push(o, 0x41); bv_i32(o, 37);
                        bv_push(o, 0x10); bv_u32(o, bin_find_func("putchar"));
                        bv_push(o, 0x1A);
                    } else {
                        error(n->nline, n->ncol, "unsupported printf format");
                    }
                } else {
                    bv_push(o, 0x41); bv_i32(o, fmt[fi] & 255);
                    bv_push(o, 0x10); bv_u32(o, bin_find_func("putchar"));
                    bv_push(o, 0x1A);
                }
            }
            bv_push(o, 0x41); bv_i32(o, 0);
            bin_last_float = 0;
        } else if (strcmp(n->sval, "putchar") == 0) {
            gen_expr_bin(o, n->list[0]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("putchar"));
        } else if (strcmp(n->sval, "getchar") == 0) {
            bv_push(o, 0x10); bv_u32(o, bin_find_func("getchar"));
        } else if (strcmp(n->sval, "exit") == 0) {
            gen_expr_bin(o, n->list[0]);
            bv_push(o, 0x10); bv_u32(o, 0);
            bv_push(o, 0x41); bv_i32(o, 0);
        } else if (strcmp(n->sval, "malloc") == 0) {
            gen_expr_bin(o, n->list[0]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("malloc"));
        } else if (strcmp(n->sval, "free") == 0) {
            if (n->ival2 > 0) {
                gen_expr_bin(o, n->list[0]);
            } else {
                bv_push(o, 0x41); bv_i32(o, 0);
            }
            bv_push(o, 0x10); bv_u32(o, bin_find_func("free"));
            bv_push(o, 0x41); bv_i32(o, 0);
        } else if (strcmp(n->sval, "strlen") == 0) {
            gen_expr_bin(o, n->list[0]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("strlen"));
        } else if (strcmp(n->sval, "strcmp") == 0) {
            gen_expr_bin(o, n->list[0]);
            gen_expr_bin(o, n->list[1]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("strcmp"));
        } else if (strcmp(n->sval, "strncpy") == 0) {
            gen_expr_bin(o, n->list[0]);
            gen_expr_bin(o, n->list[1]);
            gen_expr_bin(o, n->list[2]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("strncpy"));
        } else if (strcmp(n->sval, "memcpy") == 0) {
            gen_expr_bin(o, n->list[0]);
            gen_expr_bin(o, n->list[1]);
            gen_expr_bin(o, n->list[2]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("memcpy"));
        } else if (strcmp(n->sval, "memset") == 0) {
            gen_expr_bin(o, n->list[0]);
            gen_expr_bin(o, n->list[1]);
            gen_expr_bin(o, n->list[2]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("memset"));
        } else if (strcmp(n->sval, "memcmp") == 0) {
            gen_expr_bin(o, n->list[0]);
            gen_expr_bin(o, n->list[1]);
            gen_expr_bin(o, n->list[2]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("memcmp"));
        /* --- new libc builtins (binary) --- */
        } else if (strcmp(n->sval, "isdigit") == 0) {
            gen_expr_bin(o, n->list[0]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("isdigit"));
        } else if (strcmp(n->sval, "isalpha") == 0) {
            gen_expr_bin(o, n->list[0]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("isalpha"));
        } else if (strcmp(n->sval, "isalnum") == 0) {
            gen_expr_bin(o, n->list[0]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("isalnum"));
        } else if (strcmp(n->sval, "isspace") == 0) {
            gen_expr_bin(o, n->list[0]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("isspace"));
        } else if (strcmp(n->sval, "isupper") == 0) {
            gen_expr_bin(o, n->list[0]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("isupper"));
        } else if (strcmp(n->sval, "islower") == 0) {
            gen_expr_bin(o, n->list[0]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("islower"));
        } else if (strcmp(n->sval, "isprint") == 0) {
            gen_expr_bin(o, n->list[0]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("isprint"));
        } else if (strcmp(n->sval, "ispunct") == 0) {
            gen_expr_bin(o, n->list[0]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("ispunct"));
        } else if (strcmp(n->sval, "isxdigit") == 0) {
            gen_expr_bin(o, n->list[0]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("isxdigit"));
        } else if (strcmp(n->sval, "toupper") == 0) {
            gen_expr_bin(o, n->list[0]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("toupper"));
        } else if (strcmp(n->sval, "tolower") == 0) {
            gen_expr_bin(o, n->list[0]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("tolower"));
        } else if (strcmp(n->sval, "abs") == 0) {
            gen_expr_bin(o, n->list[0]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("abs"));
        } else if (strcmp(n->sval, "atoi") == 0) {
            gen_expr_bin(o, n->list[0]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("atoi"));
        } else if (strcmp(n->sval, "puts") == 0) {
            gen_expr_bin(o, n->list[0]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("puts"));
        } else if (strcmp(n->sval, "srand") == 0) {
            gen_expr_bin(o, n->list[0]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("srand"));
            bv_push(o, 0x41); bv_i32(o, 0);
        } else if (strcmp(n->sval, "rand") == 0) {
            bv_push(o, 0x10); bv_u32(o, bin_find_func("rand"));
        } else if (strcmp(n->sval, "strcpy") == 0) {
            gen_expr_bin(o, n->list[0]);
            gen_expr_bin(o, n->list[1]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("strcpy"));
        } else if (strcmp(n->sval, "strcat") == 0) {
            gen_expr_bin(o, n->list[0]);
            gen_expr_bin(o, n->list[1]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("strcat"));
        } else if (strcmp(n->sval, "strchr") == 0) {
            gen_expr_bin(o, n->list[0]);
            gen_expr_bin(o, n->list[1]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("strchr"));
        } else if (strcmp(n->sval, "strrchr") == 0) {
            gen_expr_bin(o, n->list[0]);
            gen_expr_bin(o, n->list[1]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("strrchr"));
        } else if (strcmp(n->sval, "strstr") == 0) {
            gen_expr_bin(o, n->list[0]);
            gen_expr_bin(o, n->list[1]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("strstr"));
        } else if (strcmp(n->sval, "calloc") == 0) {
            gen_expr_bin(o, n->list[0]);
            gen_expr_bin(o, n->list[1]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("calloc"));
        } else if (strcmp(n->sval, "strncmp") == 0) {
            gen_expr_bin(o, n->list[0]);
            gen_expr_bin(o, n->list[1]);
            gen_expr_bin(o, n->list[2]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("strncmp"));
        } else if (strcmp(n->sval, "strncat") == 0) {
            gen_expr_bin(o, n->list[0]);
            gen_expr_bin(o, n->list[1]);
            gen_expr_bin(o, n->list[2]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("strncat"));
        } else if (strcmp(n->sval, "memmove") == 0) {
            gen_expr_bin(o, n->list[0]);
            gen_expr_bin(o, n->list[1]);
            gen_expr_bin(o, n->list[2]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("memmove"));
        } else if (strcmp(n->sval, "memchr") == 0) {
            gen_expr_bin(o, n->list[0]);
            gen_expr_bin(o, n->list[1]);
            gen_expr_bin(o, n->list[2]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("memchr"));
        } else if (strcmp(n->sval, "strtol") == 0) {
            gen_expr_bin(o, n->list[0]);
            gen_expr_bin(o, n->list[1]);
            gen_expr_bin(o, n->list[2]);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("strtol"));
        } else {
            for (i = 0; i < n->ival2; i++) {
                gen_expr_bin(o, n->list[i]);
                if (func_param_is_float(n->sval, i) && !bin_last_float) {
                    bv_push(o, 0xB7); /* f64.convert_i32_s */
                } else if (!func_param_is_float(n->sval, i) && bin_last_float) {
                    bv_push(o, 0xAA); /* i32.trunc_f64_s */
                }
            }
            bv_push(o, 0x10); bv_u32(o, bin_find_func(n->sval));
            if (func_is_void(n->sval)) {
                bv_push(o, 0x41); bv_i32(o, 0);
                bin_last_float = 0;
            } else {
                bin_last_float = func_ret_is_float(n->sval);
            }
        }
        break;
    case ND_STR_LIT:
        bv_push(o, 0x41); bv_i32(o, str_table[n->ival]->offset);
        bin_last_float = 0;
        break;
    case ND_CALL_INDIRECT: {
        int ci_i;
        int ci_np;
        int ci_type_idx;
        ci_np = n->ival;
        /* push arguments */
        for (ci_i = 0; ci_i < n->ival2; ci_i++) {
            gen_expr_bin(o, n->list[ci_i]);
        }
        /* push table index */
        gen_expr_bin(o, n->c0);
        /* call_indirect type_idx table_idx(0) */
        ci_type_idx = bin_find_or_add_type(ci_np, n->ival3 ? 0 : 1);
        bv_push(o, 0x11); /* call_indirect */
        bv_u32(o, ci_type_idx);
        bv_u32(o, 0); /* table index 0 */
        if (n->ival3) {
            /* void fn — push dummy i32 */
            bv_push(o, 0x41); bv_i32(o, 0);
        }
        bin_last_float = 0;
        break;
    }
    case ND_MEMBER:
        off = resolve_field_offset(n->sval);
        if (off < 0) error(n->nline, n->ncol, "unknown struct field");
        gen_expr_bin(o, n->c0);
        if (off > 0) { bv_push(o, 0x41); bv_i32(o, off); bv_push(o, 0x6A); }
        bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0);
        bin_last_float = 0;
        break;
    case ND_SIZEOF: {
        struct StructDef *sd;
        int sz;
        if (n->ival == 1) {
            sz = 4;
        } else if (n->c0 != (struct Node *)0) {
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
        } else if (n->ival2 > 0) {
            sz = n->ival2;
        } else {
            sz = 4;
        }
        bv_push(o, 0x41); bv_i32(o, sz);
        bin_last_float = 0;
        break;
    }
    case ND_SUBSCRIPT:
        esz = expr_elem_size(n->c0);
        gen_expr_bin(o, n->c0);
        gen_expr_bin(o, n->c1);
        if (esz > 1) { bv_push(o, 0x41); bv_i32(o, esz); bv_push(o, 0x6C); }
        bv_push(o, 0x6A);
        if (esz == 1) { bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0); }
        else if (esz == 2) { bv_push(o, 0x2E); bv_u32(o, 1); bv_u32(o, 0); }
        else { bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0); }
        bin_last_float = 0;
        break;
    case ND_POST_INC:
    case ND_POST_DEC: {
        struct Node *tgt2;
        char *pname;
        int pis_global;
        int pesz;
        int poff;
        int atmp;
        atmp = find_local("__atmp");
        tgt2 = n->c0;
        if (tgt2->kind == ND_IDENT) {
            pname = tgt2->sval;
            pis_global = (find_global(pname) >= 0);
            if (pis_global) {
                bv_push(o, 0x23); bv_u32(o, bin_global_idx(pname));
                bv_push(o, 0x21); bv_u32(o, atmp);
                bv_push(o, 0x23); bv_u32(o, bin_global_idx(pname));
                bv_push(o, 0x41); bv_i32(o, 1);
                if (n->kind == ND_POST_INC) { bv_push(o, 0x6A); } else { bv_push(o, 0x6B); }
                bv_push(o, 0x24); bv_u32(o, bin_global_idx(pname));
                bv_push(o, 0x20); bv_u32(o, atmp);
            } else {
                li = find_local(pname);
                bv_push(o, 0x20); bv_u32(o, li);
                bv_push(o, 0x20); bv_u32(o, li);
                bv_push(o, 0x41); bv_i32(o, 1);
                if (n->kind == ND_POST_INC) { bv_push(o, 0x6A); } else { bv_push(o, 0x6B); }
                bv_push(o, 0x21); bv_u32(o, li);
            }
        } else if (tgt2->kind == ND_UNARY && tgt2->ival == TOK_STAR) {
            pesz = expr_elem_size(tgt2->c0);
            gen_expr_bin(o, tgt2->c0);
            if (pesz == 1) { bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0); }
            else if (pesz == 2) { bv_push(o, 0x2E); bv_u32(o, 1); bv_u32(o, 0); }
            else { bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0); }
            bv_push(o, 0x21); bv_u32(o, atmp);
            gen_expr_bin(o, tgt2->c0);
            gen_expr_bin(o, tgt2->c0);
            if (pesz == 1) { bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0); }
            else if (pesz == 2) { bv_push(o, 0x2E); bv_u32(o, 1); bv_u32(o, 0); }
            else { bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0); }
            bv_push(o, 0x41); bv_i32(o, 1);
            if (n->kind == ND_POST_INC) { bv_push(o, 0x6A); } else { bv_push(o, 0x6B); }
            if (pesz == 1) { bv_push(o, 0x3A); bv_u32(o, 0); bv_u32(o, 0); }
            else if (pesz == 2) { bv_push(o, 0x3B); bv_u32(o, 1); bv_u32(o, 0); }
            else { bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 0); }
            bv_push(o, 0x20); bv_u32(o, atmp);
        } else if (tgt2->kind == ND_SUBSCRIPT) {
            pesz = expr_elem_size(tgt2->c0);
            gen_expr_bin(o, tgt2->c0); gen_expr_bin(o, tgt2->c1);
            if (pesz > 1) { bv_push(o, 0x41); bv_i32(o, pesz); bv_push(o, 0x6C); }
            bv_push(o, 0x6A);
            if (pesz == 1) { bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0); }
            else if (pesz == 2) { bv_push(o, 0x2E); bv_u32(o, 1); bv_u32(o, 0); }
            else { bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0); }
            bv_push(o, 0x21); bv_u32(o, atmp);
            gen_expr_bin(o, tgt2->c0); gen_expr_bin(o, tgt2->c1);
            if (pesz > 1) { bv_push(o, 0x41); bv_i32(o, pesz); bv_push(o, 0x6C); }
            bv_push(o, 0x6A);
            gen_expr_bin(o, tgt2->c0); gen_expr_bin(o, tgt2->c1);
            if (pesz > 1) { bv_push(o, 0x41); bv_i32(o, pesz); bv_push(o, 0x6C); }
            bv_push(o, 0x6A);
            if (pesz == 1) { bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0); }
            else if (pesz == 2) { bv_push(o, 0x2E); bv_u32(o, 1); bv_u32(o, 0); }
            else { bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0); }
            bv_push(o, 0x41); bv_i32(o, 1);
            if (n->kind == ND_POST_INC) { bv_push(o, 0x6A); } else { bv_push(o, 0x6B); }
            if (pesz == 1) { bv_push(o, 0x3A); bv_u32(o, 0); bv_u32(o, 0); }
            else if (pesz == 2) { bv_push(o, 0x3B); bv_u32(o, 1); bv_u32(o, 0); }
            else { bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 0); }
            bv_push(o, 0x20); bv_u32(o, atmp);
        } else if (tgt2->kind == ND_MEMBER) {
            poff = resolve_field_offset(tgt2->sval);
            if (poff < 0) error(tgt2->nline, tgt2->ncol, "unknown struct field");
            gen_expr_bin(o, tgt2->c0);
            if (poff > 0) { bv_push(o, 0x41); bv_i32(o, poff); bv_push(o, 0x6A); }
            bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0);
            bv_push(o, 0x21); bv_u32(o, atmp);
            gen_expr_bin(o, tgt2->c0);
            if (poff > 0) { bv_push(o, 0x41); bv_i32(o, poff); bv_push(o, 0x6A); }
            gen_expr_bin(o, tgt2->c0);
            if (poff > 0) { bv_push(o, 0x41); bv_i32(o, poff); bv_push(o, 0x6A); }
            bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0);
            bv_push(o, 0x41); bv_i32(o, 1);
            if (n->kind == ND_POST_INC) { bv_push(o, 0x6A); } else { bv_push(o, 0x6B); }
            bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 0);
            bv_push(o, 0x20); bv_u32(o, atmp);
        } else {
            error(n->nline, n->ncol, "unsupported post-inc/dec target");
        }
        bin_last_float = 0;
        break;
    }
    case ND_TERNARY:
        gen_expr_bin(o, n->c0);
        bv_push(o, 0x04); bv_push(o, 0x7F); bin_lbl_sp++;
        gen_expr_bin(o, n->c1);
        bv_push(o, 0x05);
        gen_expr_bin(o, n->c2);
        bv_push(o, 0x0B); bin_lbl_sp--;
        bin_last_float = 0;
        break;
    default:
        error(n->nline, n->ncol, "unsupported expression in codegen");
    }
}

/* --- Binary statement codegen --- */

void gen_stmt_bin(struct ByteVec *o, struct Node *n);

void gen_body_bin(struct ByteVec *o, struct Node *n) {
    int i;
    if (n->kind == ND_BLOCK) {
        for (i = 0; i < n->ival2; i++) {
            gen_stmt_bin(o, n->list[i]);
        }
    } else {
        gen_stmt_bin(o, n);
    }
}

void gen_stmt_bin(struct ByteVec *o, struct Node *n) {
    int lbl;
    int i;
    int bsz;
    int decl_float;

    switch (n->kind) {
    case ND_RETURN:
        if (n->c0 != (struct Node *)0) {
            gen_expr_bin(o, n->c0);
        }
        bv_push(o, 0x0F);
        break;
    case ND_VAR_DECL:
        if (n->ival > 0) {
            bsz = n->ival * n->ival2;
            bv_push(o, 0x41); bv_i32(o, bsz);
            bv_push(o, 0x10); bv_u32(o, bin_find_func("malloc"));
            bv_push(o, 0x21); bv_u32(o, find_local(n->sval));
        } else if (n->c0 != (struct Node *)0) {
            decl_float = var_is_float(n->sval);
            gen_expr_bin(o, n->c0);
            if (decl_float && !bin_last_float) {
                bv_push(o, 0xB7); /* f64.convert_i32_s */
            } else if (!decl_float && bin_last_float) {
                bv_push(o, 0xAA); /* i32.trunc_f64_s */
            }
            bv_push(o, 0x21); bv_u32(o, find_local(n->sval));
        }
        break;
    case ND_EXPR_STMT:
        gen_expr_bin(o, n->c0);
        bv_push(o, 0x1A);
        break;
    case ND_IF:
        gen_expr_bin(o, n->c0);
        if (n->c2 != (struct Node *)0) {
            bv_push(o, 0x04); bv_push(o, 0x40); bin_lbl_sp++;
            gen_body_bin(o, n->c1);
            bv_push(o, 0x05);
            gen_body_bin(o, n->c2);
            bv_push(o, 0x0B); bin_lbl_sp--;
        } else {
            bv_push(o, 0x04); bv_push(o, 0x40); bin_lbl_sp++;
            gen_body_bin(o, n->c1);
            bv_push(o, 0x0B); bin_lbl_sp--;
        }
        break;
    case ND_WHILE:
        lbl = label_cnt;
        label_cnt++;
        brk_lbl[loop_sp] = lbl;
        cont_lbl[loop_sp] = lbl;
        bin_brk_at[loop_sp] = bin_lbl_sp;
        bv_push(o, 0x02); bv_push(o, 0x40); bin_lbl_sp++;
        bin_loop_at[loop_sp] = bin_lbl_sp;
        bv_push(o, 0x03); bv_push(o, 0x40); bin_lbl_sp++;
        loop_sp++;
        gen_expr_bin(o, n->c0);
        bv_push(o, 0x45);
        bv_push(o, 0x0D); bv_u32(o, bin_lbl_sp - bin_brk_at[loop_sp - 1] - 1);
        bin_cont_at[loop_sp - 1] = bin_lbl_sp;
        bv_push(o, 0x02); bv_push(o, 0x40); bin_lbl_sp++;
        gen_body_bin(o, n->c1);
        bv_push(o, 0x0B); bin_lbl_sp--;
        bv_push(o, 0x0C); bv_u32(o, bin_lbl_sp - bin_loop_at[loop_sp - 1] - 1);
        bv_push(o, 0x0B); bin_lbl_sp--;
        bv_push(o, 0x0B); bin_lbl_sp--;
        loop_sp--;
        break;
    case ND_FOR:
        if (n->c0 != (struct Node *)0) {
            gen_stmt_bin(o, n->c0);
        }
        lbl = label_cnt;
        label_cnt++;
        brk_lbl[loop_sp] = lbl;
        cont_lbl[loop_sp] = lbl;
        bin_brk_at[loop_sp] = bin_lbl_sp;
        bv_push(o, 0x02); bv_push(o, 0x40); bin_lbl_sp++;
        bin_loop_at[loop_sp] = bin_lbl_sp;
        bv_push(o, 0x03); bv_push(o, 0x40); bin_lbl_sp++;
        loop_sp++;
        if (n->c1 != (struct Node *)0) {
            gen_expr_bin(o, n->c1);
            bv_push(o, 0x45);
            bv_push(o, 0x0D); bv_u32(o, bin_lbl_sp - bin_brk_at[loop_sp - 1] - 1);
        }
        bin_cont_at[loop_sp - 1] = bin_lbl_sp;
        bv_push(o, 0x02); bv_push(o, 0x40); bin_lbl_sp++;
        gen_body_bin(o, n->c3);
        bv_push(o, 0x0B); bin_lbl_sp--;
        if (n->c2 != (struct Node *)0) {
            gen_expr_bin(o, n->c2);
            bv_push(o, 0x1A);
        }
        bv_push(o, 0x0C); bv_u32(o, bin_lbl_sp - bin_loop_at[loop_sp - 1] - 1);
        bv_push(o, 0x0B); bin_lbl_sp--;
        bv_push(o, 0x0B); bin_lbl_sp--;
        loop_sp--;
        break;
    case ND_DO_WHILE:
        lbl = label_cnt;
        label_cnt++;
        brk_lbl[loop_sp] = lbl;
        cont_lbl[loop_sp] = lbl;
        bin_brk_at[loop_sp] = bin_lbl_sp;
        bv_push(o, 0x02); bv_push(o, 0x40); bin_lbl_sp++;
        bin_loop_at[loop_sp] = bin_lbl_sp;
        bv_push(o, 0x03); bv_push(o, 0x40); bin_lbl_sp++;
        bin_cont_at[loop_sp] = bin_lbl_sp;
        bv_push(o, 0x02); bv_push(o, 0x40); bin_lbl_sp++;
        loop_sp++;
        gen_body_bin(o, n->c0);
        bv_push(o, 0x0B); bin_lbl_sp--;
        gen_expr_bin(o, n->c1);
        bv_push(o, 0x0D); bv_u32(o, bin_lbl_sp - bin_loop_at[loop_sp - 1] - 1);
        bv_push(o, 0x0B); bin_lbl_sp--;
        bv_push(o, 0x0B); bin_lbl_sp--;
        loop_sp--;
        break;
    case ND_BREAK:
        if (loop_sp <= 0) error(n->nline, n->ncol, "break outside loop");
        bv_push(o, 0x0C); bv_u32(o, bin_lbl_sp - bin_brk_at[loop_sp - 1] - 1);
        break;
    case ND_CONTINUE:
        if (loop_sp <= 0) error(n->nline, n->ncol, "continue outside loop");
        if (cont_lbl[loop_sp - 1] < 0) error(n->nline, n->ncol, "continue not inside a loop");
        bv_push(o, 0x0C); bv_u32(o, bin_lbl_sp - bin_cont_at[loop_sp - 1] - 1);
        break;
    case ND_BLOCK:
        for (i = 0; i < n->ival2; i++) {
            gen_stmt_bin(o, n->list[i]);
        }
        break;
    case ND_SWITCH: {
        int case_vals[256];
        int case_start[256];
        int case_depth[256];
        int nc;
        int dflt_pos;
        int has_dflt;
        int k;
        int j;
        int next_start;
        int sw_lbl;
        int si;
        int last_case_pos;
        int dflt_depth;

        nc = 0;
        dflt_pos = -1;
        has_dflt = 0;
        for (si = 0; si < n->ival2; si++) {
            if (n->list[si]->kind == ND_CASE) {
                if (nc >= MAX_CASES) {
                    error(n->nline, n->ncol, "too many cases in switch");
                }
                case_vals[nc] = n->list[si]->ival;
                case_start[nc] = si;
                nc++;
            } else if (n->list[si]->kind == ND_DEFAULT) {
                dflt_pos = si;
                has_dflt = 1;
            }
        }

        if (has_dflt) {
            last_case_pos = (nc > 0) ? case_start[nc - 1] : -1;
            if (dflt_pos < last_case_pos) {
                error(n->c0->nline, n->c0->ncol,
                      "default must appear after all case labels");
            }
        }

        sw_lbl = label_cnt;
        label_cnt++;
        brk_lbl[loop_sp] = sw_lbl;
        if (loop_sp > 0) {
            cont_lbl[loop_sp] = cont_lbl[loop_sp - 1];
            bin_cont_at[loop_sp] = bin_cont_at[loop_sp - 1];
        } else {
            cont_lbl[loop_sp] = -1;
        }

        gen_expr_bin(o, n->c0);
        bv_push(o, 0x21); bv_u32(o, find_local("__stmp"));

        bin_brk_at[loop_sp] = bin_lbl_sp;
        bv_push(o, 0x02); bv_push(o, 0x40); bin_lbl_sp++;

        dflt_depth = bin_lbl_sp;
        bv_push(o, 0x02); bv_push(o, 0x40); bin_lbl_sp++;

        for (k = nc - 1; k >= 0; k--) {
            case_depth[k] = bin_lbl_sp;
            bv_push(o, 0x02); bv_push(o, 0x40); bin_lbl_sp++;
        }

        loop_sp++;

        for (k = 0; k < nc; k++) {
            bv_push(o, 0x20); bv_u32(o, find_local("__stmp"));
            bv_push(o, 0x41); bv_i32(o, case_vals[k]);
            bv_push(o, 0x46);
            bv_push(o, 0x0D); bv_u32(o, bin_lbl_sp - case_depth[k] - 1);
        }
        if (has_dflt) {
            bv_push(o, 0x0C); bv_u32(o, bin_lbl_sp - dflt_depth - 1);
        } else {
            bv_push(o, 0x0C); bv_u32(o, bin_lbl_sp - bin_brk_at[loop_sp - 1] - 1);
        }

        for (k = 0; k < nc; k++) {
            bv_push(o, 0x0B); bin_lbl_sp--;
            if (k + 1 < nc) {
                next_start = case_start[k + 1];
            } else if (has_dflt) {
                next_start = dflt_pos;
            } else {
                next_start = n->ival2;
            }
            for (j = case_start[k] + 1; j < next_start; j++) {
                if (n->list[j]->kind == ND_CASE) continue;
                if (n->list[j]->kind == ND_DEFAULT) continue;
                gen_stmt_bin(o, n->list[j]);
            }
        }

        bv_push(o, 0x0B); bin_lbl_sp--;

        if (has_dflt) {
            for (j = dflt_pos + 1; j < n->ival2; j++) {
                if (n->list[j]->kind == ND_CASE) continue;
                if (n->list[j]->kind == ND_DEFAULT) continue;
                gen_stmt_bin(o, n->list[j]);
            }
        }

        bv_push(o, 0x0B); bin_lbl_sp--;

        loop_sp--;
        break;
    }
    default:
        error(n->nline, n->ncol, "unsupported statement in codegen");
    }
}

/* --- Binary function codegen --- */

void gen_func_bin(struct ByteVec *cs, struct Node *n) {
    struct ByteVec *fb;
    struct Node *body;
    int i;
    int nparam_locals;
    int nextra;

    if (n->c0 == (struct Node *)0) return;

    nlocals = 0;
    label_cnt = 0;
    loop_sp = 0;
    bin_lbl_sp = 0;

    for (i = 0; i < n->ival2; i++) {
        add_local(n->list[i]->sval, n->list[i]->ival2, n->list[i]->ival3 & 0xF, n->list[i]->ival3 >> 4);
    }
    nparam_locals = nlocals;
    collect_locals(n->c0);
    add_local("__atmp", 4, 0, 0);
    add_local("__stmp", 4, 0, 0);
    add_local("__ftmp", 4, 0, 2); /* f64 local for float stack manipulation */

    fb = bv_new(4096);

    nextra = nlocals - nparam_locals;
    if (nextra > 0) {
        bv_u32(fb, nextra);
        for (i = nparam_locals; i < nlocals; i++) {
            bv_u32(fb, 1);
            if (local_vars[i]->lv_is_float) {
                bv_push(fb, 0x7C); /* f64 */
            } else {
                bv_push(fb, 0x7F); /* i32 */
            }
        }
    } else {
        bv_u32(fb, 0);
    }

    body = n->c0;
    for (i = 0; i < body->ival2; i++) {
        gen_stmt_bin(fb, body->list[i]);
    }

    if (n->ival != 1) {
        if (func_ret_is_float(n->sval)) {
            /* f64.const 0.0 — 8 zero bytes IEEE 754 */
            bv_push(fb, 0x44);
            bv_push(fb, 0); bv_push(fb, 0); bv_push(fb, 0); bv_push(fb, 0);
            bv_push(fb, 0); bv_push(fb, 0); bv_push(fb, 0); bv_push(fb, 0);
        } else {
            bv_push(fb, 0x41); bv_i32(fb, 0);
        }
    }

    bv_push(fb, 0x0B);

    bv_u32(cs, fb->len);
    bv_append(cs, fb);
}

/* --- Runtime helper binary encoders --- */

void emit_helper_putchar(struct ByteVec *o) {
    /* (param $ch i32) (result i32), 0 extra locals */
    bv_u32(o, 0);
    /* i32.store8 [0] = ch */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x3A); bv_u32(o, 0); bv_u32(o, 0);
    /* i32.store [4] = 0 (iov.base) */
    bv_push(o, 0x41); bv_i32(o, 4);
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 0);
    /* i32.store [8] = 1 (iov.len) */
    bv_push(o, 0x41); bv_i32(o, 8);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 0);
    /* drop(call fd_write(1, 4, 1, 12)) */
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x41); bv_i32(o, 4);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x41); bv_i32(o, 12);
    bv_push(o, 0x10); bv_u32(o, 1);
    bv_push(o, 0x1A);
    /* return ch */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x0B);
}

void emit_helper_getchar(struct ByteVec *o) {
    /* (result i32), 0 params, 0 extra locals */
    bv_u32(o, 0);
    /* i32.store [4] = 0 (iov.base) */
    bv_push(o, 0x41); bv_i32(o, 4);
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 0);
    /* i32.store [8] = 1 (iov.len) */
    bv_push(o, 0x41); bv_i32(o, 8);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 0);
    /* drop(call fd_read(0, 4, 1, 12)) */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 4);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x41); bv_i32(o, 12);
    bv_push(o, 0x10); bv_u32(o, 2);
    bv_push(o, 0x1A);
    /* if (i32.eqz (i32.load [12])) then -1 else i32.load8_u [0] */
    bv_push(o, 0x41); bv_i32(o, 12);
    bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0);
    bv_push(o, 0x45);
    bv_push(o, 0x04); bv_push(o, 0x7F);
    bv_push(o, 0x41); bv_i32(o, -1);
    bv_push(o, 0x05);
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
}

void emit_helper_print_int(struct ByteVec *o) {
    /* (param $n i32), 2 extra locals: $buf(1), $len(2) */
    bv_u32(o, 1); bv_u32(o, 2); bv_push(o, 0x7F);
    /* if (n < 0) then putchar(45); n = 0-n */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x48);
    bv_push(o, 0x04); bv_push(o, 0x40);
    bv_push(o, 0x41); bv_i32(o, 45);
    bv_push(o, 0x10); bv_u32(o, 3);
    bv_push(o, 0x1A);
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x6B);
    bv_push(o, 0x21); bv_u32(o, 0);
    bv_push(o, 0x0B);
    /* if (n == 0) then putchar(48); return */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x45);
    bv_push(o, 0x04); bv_push(o, 0x40);
    bv_push(o, 0x41); bv_i32(o, 48);
    bv_push(o, 0x10); bv_u32(o, 3);
    bv_push(o, 0x1A);
    bv_push(o, 0x0F);
    bv_push(o, 0x0B);
    /* buf = 48; len = 0 */
    bv_push(o, 0x41); bv_i32(o, 48);
    bv_push(o, 0x21); bv_u32(o, 1);
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 2);
    /* block $done { loop $digit */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* br_if $done (i32.eqz n) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x45);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* buf = buf - 1 */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6B);
    bv_push(o, 0x21); bv_u32(o, 1);
    /* i32.store8 [buf] = 48 + (n rem_u 10) */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x41); bv_i32(o, 48);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 10);
    bv_push(o, 0x70);
    bv_push(o, 0x6A);
    bv_push(o, 0x3A); bv_u32(o, 0); bv_u32(o, 0);
    /* n = n div_u 10 */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 10);
    bv_push(o, 0x6E);
    bv_push(o, 0x21); bv_u32(o, 0);
    /* len = len + 1 */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 2);
    /* br $digit */
    bv_push(o, 0x0C); bv_u32(o, 0);
    /* end loop, end block */
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* print loop: block $pd { loop $pl */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* br_if $pd (i32.eqz len) */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x45);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* drop(call putchar(i32.load8_u [buf])) */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x10); bv_u32(o, 3);
    bv_push(o, 0x1A);
    /* buf = buf + 1 */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 1);
    /* len = len - 1 */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6B);
    bv_push(o, 0x21); bv_u32(o, 2);
    /* br $pl */
    bv_push(o, 0x0C); bv_u32(o, 0);
    /* end loop, end block */
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
}

void emit_helper_print_str(struct ByteVec *o) {
    /* (param $ptr i32), 1 extra local: $ch(1) */
    bv_u32(o, 1); bv_u32(o, 1); bv_push(o, 0x7F);
    /* block $done { loop $next */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* ch = i32.load8_u [ptr] */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 1);
    /* br_if $done (i32.eqz ch) */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x45);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* drop(call putchar(ch)) */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x10); bv_u32(o, 3);
    bv_push(o, 0x1A);
    /* ptr = ptr + 1 */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 0);
    /* br $next */
    bv_push(o, 0x0C); bv_u32(o, 0);
    /* end loop, end block */
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
}

void emit_helper_print_hex(struct ByteVec *o) {
    /* (param $n i32), 3 extra locals: $buf(1), $len(2), $d(3) */
    bv_u32(o, 1); bv_u32(o, 3); bv_push(o, 0x7F);
    /* if (n == 0) then putchar(48); return */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x45);
    bv_push(o, 0x04); bv_push(o, 0x40);
    bv_push(o, 0x41); bv_i32(o, 48);
    bv_push(o, 0x10); bv_u32(o, 3);
    bv_push(o, 0x1A);
    bv_push(o, 0x0F);
    bv_push(o, 0x0B);
    /* buf = 48; len = 0 */
    bv_push(o, 0x41); bv_i32(o, 48);
    bv_push(o, 0x21); bv_u32(o, 1);
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 2);
    /* block $done { loop $digit */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* br_if $done (i32.eqz n) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x45);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* buf = buf - 1 */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6B);
    bv_push(o, 0x21); bv_u32(o, 1);
    /* d = n & 15 */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 15);
    bv_push(o, 0x71);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* if (d < 10) store8 [buf] = 48+d  else store8 [buf] = 87+d */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x41); bv_i32(o, 10);
    bv_push(o, 0x49);
    bv_push(o, 0x04); bv_push(o, 0x40);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x41); bv_i32(o, 48);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x6A);
    bv_push(o, 0x3A); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x05);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x41); bv_i32(o, 87);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x6A);
    bv_push(o, 0x3A); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x0B);
    /* n = n shr_u 4 */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 4);
    bv_push(o, 0x76);
    bv_push(o, 0x21); bv_u32(o, 0);
    /* len = len + 1 */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 2);
    /* br $digit */
    bv_push(o, 0x0C); bv_u32(o, 0);
    /* end loop, end block */
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* print loop */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x45);
    bv_push(o, 0x0D); bv_u32(o, 1);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x10); bv_u32(o, 3);
    bv_push(o, 0x1A);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 1);
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6B);
    bv_push(o, 0x21); bv_u32(o, 2);
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
}

void emit_helper_malloc(struct ByteVec *o) {
    /* (param $size i32) (result i32)
       locals: $total=1, $cur=2, $prev=3, $rest=4, $ptr=5 */
    bv_u32(o, 1); bv_u32(o, 5); bv_push(o, 0x7F);

    /* total = (size + 15) & -8 */
    bv_push(o, 0x20); bv_u32(o, 0);   /* local.get $size */
    bv_push(o, 0x41); bv_i32(o, 15);  /* i32.const 15 */
    bv_push(o, 0x6A);                 /* i32.add */
    bv_push(o, 0x41); bv_i32(o, -8);  /* i32.const -8 */
    bv_push(o, 0x71);                 /* i32.and */
    bv_push(o, 0x21); bv_u32(o, 1);   /* local.set $total */

    /* prev = 0 */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 3);   /* local.set $prev */

    /* cur = global.get __free_list */
    bv_push(o, 0x23); bv_u32(o, 1);   /* global.get $__free_list */
    bv_push(o, 0x21); bv_u32(o, 2);   /* local.set $cur */

    /* block $done */
    bv_push(o, 0x02); bv_push(o, 0x40);
    /* loop $search */
    bv_push(o, 0x03); bv_push(o, 0x40);

    /* br_if $done (i32.eqz cur) — depth 1 from inside loop */
    bv_push(o, 0x20); bv_u32(o, 2);   /* local.get $cur */
    bv_push(o, 0x45);                 /* i32.eqz */
    bv_push(o, 0x0D); bv_u32(o, 1);   /* br_if 1 ($done) */

    /* if (i32.ge_u cur.size total) */
    bv_push(o, 0x20); bv_u32(o, 2);   /* local.get $cur */
    bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0);  /* i32.load offset=0 */
    bv_push(o, 0x20); bv_u32(o, 1);   /* local.get $total */
    bv_push(o, 0x4F);                 /* i32.ge_u */
    bv_push(o, 0x04); bv_push(o, 0x40); /* if void */

      /* if (cur.size >= total + 16) — can split */
      bv_push(o, 0x20); bv_u32(o, 2);
      bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0);
      bv_push(o, 0x20); bv_u32(o, 1);
      bv_push(o, 0x41); bv_i32(o, 16);
      bv_push(o, 0x6A);
      bv_push(o, 0x4F);               /* i32.ge_u */
      bv_push(o, 0x04); bv_push(o, 0x40); /* if void (split) */

        /* rest = cur + total */
        bv_push(o, 0x20); bv_u32(o, 2);
        bv_push(o, 0x20); bv_u32(o, 1);
        bv_push(o, 0x6A);
        bv_push(o, 0x21); bv_u32(o, 4); /* local.set $rest */

        /* rest.size = cur.size - total */
        bv_push(o, 0x20); bv_u32(o, 4); /* addr: rest */
        bv_push(o, 0x20); bv_u32(o, 2);
        bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0); /* cur.size */
        bv_push(o, 0x20); bv_u32(o, 1);
        bv_push(o, 0x6B);               /* i32.sub */
        bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 0); /* i32.store offset=0 */

        /* rest.next = cur.next */
        bv_push(o, 0x20); bv_u32(o, 4); /* addr: rest */
        bv_push(o, 0x20); bv_u32(o, 2);
        bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 4); /* cur.next */
        bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 4); /* i32.store offset=4 */

        /* cur.size = total */
        bv_push(o, 0x20); bv_u32(o, 2);
        bv_push(o, 0x20); bv_u32(o, 1);
        bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 0);

        /* if prev → prev.next=rest else free_list=rest */
        bv_push(o, 0x20); bv_u32(o, 3);
        bv_push(o, 0x04); bv_push(o, 0x40); /* if void */
          bv_push(o, 0x20); bv_u32(o, 3);
          bv_push(o, 0x20); bv_u32(o, 4);
          bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 4);
        bv_push(o, 0x05);               /* else */
          bv_push(o, 0x20); bv_u32(o, 4);
          bv_push(o, 0x24); bv_u32(o, 1); /* global.set __free_list */
        bv_push(o, 0x0B);               /* end if (prev) */

      bv_push(o, 0x05);                 /* else — no split */

        /* if prev → prev.next=cur.next else free_list=cur.next */
        bv_push(o, 0x20); bv_u32(o, 3);
        bv_push(o, 0x04); bv_push(o, 0x40);
          bv_push(o, 0x20); bv_u32(o, 3);
          bv_push(o, 0x20); bv_u32(o, 2);
          bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 4);
          bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 4);
        bv_push(o, 0x05);               /* else */
          bv_push(o, 0x20); bv_u32(o, 2);
          bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 4);
          bv_push(o, 0x24); bv_u32(o, 1); /* global.set __free_list */
        bv_push(o, 0x0B);               /* end if (prev) */

      bv_push(o, 0x0B);                 /* end if (split/no-split) */

      /* return cur + 8 */
      bv_push(o, 0x20); bv_u32(o, 2);
      bv_push(o, 0x41); bv_i32(o, 8);
      bv_push(o, 0x6A);
      bv_push(o, 0x0F);                 /* return */

    bv_push(o, 0x0B);                   /* end if (cur.size >= total) */

    /* prev = cur */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x21); bv_u32(o, 3);

    /* cur = cur.next */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 4);
    bv_push(o, 0x21); bv_u32(o, 2);

    /* br $search (br 0) */
    bv_push(o, 0x0C); bv_u32(o, 0);

    bv_push(o, 0x0B);                   /* end loop */
    bv_push(o, 0x0B);                   /* end block */

    /* Bump-allocate fallback: ptr = heap_ptr */
    bv_push(o, 0x23); bv_u32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 5);     /* local.set $ptr */

    /* ptr.size = total */
    bv_push(o, 0x20); bv_u32(o, 5);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 0);

    /* heap_ptr += total */
    bv_push(o, 0x20); bv_u32(o, 5);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x24); bv_u32(o, 0);     /* global.set __heap_ptr */

    /* return ptr + 8 */
    bv_push(o, 0x20); bv_u32(o, 5);
    bv_push(o, 0x41); bv_i32(o, 8);
    bv_push(o, 0x6A);
    bv_push(o, 0x0B);                   /* end func */
}

void emit_helper_free(struct ByteVec *o) {
    /* (param $ptr i32)
       locals: $block=1, $cur=2, $prev=3, $next_blk=4 */
    bv_u32(o, 1); bv_u32(o, 4); bv_push(o, 0x7F);

    /* if ptr == 0, return */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x45);                 /* i32.eqz */
    bv_push(o, 0x04); bv_push(o, 0x40);
      bv_push(o, 0x0F);               /* return */
    bv_push(o, 0x0B);                 /* end if */

    /* block = ptr - 8 */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 8);
    bv_push(o, 0x6B);                 /* i32.sub */
    bv_push(o, 0x21); bv_u32(o, 1);   /* local.set $block */

    /* prev = 0 */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 3);

    /* cur = free_list */
    bv_push(o, 0x23); bv_u32(o, 1);
    bv_push(o, 0x21); bv_u32(o, 2);

    /* block $found */
    bv_push(o, 0x02); bv_push(o, 0x40);
    /* loop $scan */
    bv_push(o, 0x03); bv_push(o, 0x40);

    /* br_if $found (i32.eqz cur) — depth 1 */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x45);
    bv_push(o, 0x0D); bv_u32(o, 1);

    /* br_if $found (i32.gt_u cur block) — depth 1 */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x4D);                 /* i32.gt_u */
    bv_push(o, 0x0D); bv_u32(o, 1);

    /* prev = cur */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x21); bv_u32(o, 3);

    /* cur = cur.next */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 4);
    bv_push(o, 0x21); bv_u32(o, 2);

    /* br $scan (br 0) */
    bv_push(o, 0x0C); bv_u32(o, 0);

    bv_push(o, 0x0B);                 /* end loop */
    bv_push(o, 0x0B);                 /* end block */

    /* block.next = cur */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 4);

    /* if prev → prev.next=block else free_list=block */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x04); bv_push(o, 0x40);
      bv_push(o, 0x20); bv_u32(o, 3);
      bv_push(o, 0x20); bv_u32(o, 1);
      bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 4);
    bv_push(o, 0x05);                 /* else */
      bv_push(o, 0x20); bv_u32(o, 1);
      bv_push(o, 0x24); bv_u32(o, 1); /* global.set __free_list */
    bv_push(o, 0x0B);                 /* end if */

    /* Coalesce with next: if cur!=0 && block+block.size==cur */
    bv_push(o, 0x20); bv_u32(o, 2);   /* cur */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x47);                 /* i32.ne */
    bv_push(o, 0x20); bv_u32(o, 1);   /* block */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0); /* block.size */
    bv_push(o, 0x6A);                 /* add */
    bv_push(o, 0x20); bv_u32(o, 2);   /* cur */
    bv_push(o, 0x46);                 /* i32.eq */
    bv_push(o, 0x71);                 /* i32.and */
    bv_push(o, 0x04); bv_push(o, 0x40);
      /* block.size += cur.size */
      bv_push(o, 0x20); bv_u32(o, 1);
      bv_push(o, 0x20); bv_u32(o, 1);
      bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0);
      bv_push(o, 0x20); bv_u32(o, 2);
      bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0);
      bv_push(o, 0x6A);
      bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 0);
      /* block.next = cur.next */
      bv_push(o, 0x20); bv_u32(o, 1);
      bv_push(o, 0x20); bv_u32(o, 2);
      bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 4);
      bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 4);
    bv_push(o, 0x0B);                 /* end if */

    /* Coalesce with prev: if prev!=0 && prev+prev.size==block */
    bv_push(o, 0x20); bv_u32(o, 3);   /* prev */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x47);                 /* i32.ne */
    bv_push(o, 0x20); bv_u32(o, 3);   /* prev */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0); /* prev.size */
    bv_push(o, 0x6A);                 /* add */
    bv_push(o, 0x20); bv_u32(o, 1);   /* block */
    bv_push(o, 0x46);                 /* i32.eq */
    bv_push(o, 0x71);                 /* i32.and */
    bv_push(o, 0x04); bv_push(o, 0x40);
      /* prev.size += block.size */
      bv_push(o, 0x20); bv_u32(o, 3);
      bv_push(o, 0x20); bv_u32(o, 3);
      bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0);
      bv_push(o, 0x20); bv_u32(o, 1);
      bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0);
      bv_push(o, 0x6A);
      bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 0);
      /* prev.next = block.next */
      bv_push(o, 0x20); bv_u32(o, 3);
      bv_push(o, 0x20); bv_u32(o, 1);
      bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 4);
      bv_push(o, 0x36); bv_u32(o, 2); bv_u32(o, 4);
    bv_push(o, 0x0B);                 /* end if */

    bv_push(o, 0x0B);                 /* end func */
}

void emit_helper_strlen(struct ByteVec *o) {
    /* (param $s i32) (result i32), 1 extra local: $n(1) */
    bv_u32(o, 1); bv_u32(o, 1); bv_push(o, 0x7F);
    /* n = 0 */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 1);
    /* block $done { loop $next */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* br_if $done (i32.eqz (i32.load8_u (s + n))) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x45);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* n = n + 1 */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 1);
    /* br $next */
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* return n */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x0B);
}

void emit_helper_strcmp(struct ByteVec *o) {
    /* (param $a i32) (param $b i32) (result i32), 2 extra locals: $ca(2), $cb(3) */
    bv_u32(o, 1); bv_u32(o, 2); bv_push(o, 0x7F);
    /* block $done { loop $next */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* ca = i32.load8_u [a] */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 2);
    /* cb = i32.load8_u [b] */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* br_if $done (ca != cb) */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x47);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* br_if $done (ca == 0) */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x45);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* a = a + 1 */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 0);
    /* b = b + 1 */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 1);
    /* br $next */
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* return ca - cb */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x6B);
    bv_push(o, 0x0B);
}

void emit_helper_strncpy(struct ByteVec *o) {
    /* (param $dst i32) (param $src i32) (param $n i32) (result i32), 1 extra local: $i(3) */
    bv_u32(o, 1); bv_u32(o, 1); bv_push(o, 0x7F);
    /* i = 0 */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* block $done { loop $next */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* br_if $done (i >= n) */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x4F);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* i32.store8 [dst+i] = i32.load8_u [src+i] */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x6A);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x6A);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x3A); bv_u32(o, 0); bv_u32(o, 0);
    /* br_if $done (i32.eqz (i32.load8_u [src+i])) */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x6A);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x45);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* i = i + 1 */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* br $next */
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* return dst */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x0B);
}

void emit_helper_memcpy(struct ByteVec *o) {
    /* (param $dst i32) (param $src i32) (param $n i32) (result i32), 1 extra local: $i(3) */
    bv_u32(o, 1); bv_u32(o, 1); bv_push(o, 0x7F);
    /* i = 0 */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* block $done { loop $next */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* br_if $done (i >= n) */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x4F);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* i32.store8 [dst+i] = i32.load8_u [src+i] */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x6A);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x6A);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x3A); bv_u32(o, 0); bv_u32(o, 0);
    /* i = i + 1 */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* br $next */
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* return dst */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x0B);
}

void emit_helper_memset(struct ByteVec *o) {
    /* (param $dst i32) (param $val i32) (param $n i32) (result i32), 1 extra local: $i(3) */
    bv_u32(o, 1); bv_u32(o, 1); bv_push(o, 0x7F);
    /* i = 0 */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* block $done { loop $next */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* br_if $done (i >= n) */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x4F);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* i32.store8 [dst+i] = val */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x6A);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x3A); bv_u32(o, 0); bv_u32(o, 0);
    /* i = i + 1 */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* br $next */
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* return dst */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x0B);
}

void emit_helper_memcmp(struct ByteVec *o) {
    /* (param $a i32) (param $b i32) (param $n i32) (result i32)
       3 extra locals: $i(3), $ca(4), $cb(5) */
    bv_u32(o, 1); bv_u32(o, 3); bv_push(o, 0x7F);
    /* i = 0 */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* block $done { loop $next */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* br_if $done (i >= n) */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x4F);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* ca = i32.load8_u [a + i] */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x6A);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 4);
    /* cb = i32.load8_u [b + i] */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x6A);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 5);
    /* if (ca != cb) then return ca - cb */
    bv_push(o, 0x20); bv_u32(o, 4);
    bv_push(o, 0x20); bv_u32(o, 5);
    bv_push(o, 0x47);
    bv_push(o, 0x04); bv_push(o, 0x40);
    bv_push(o, 0x20); bv_u32(o, 4);
    bv_push(o, 0x20); bv_u32(o, 5);
    bv_push(o, 0x6B);
    bv_push(o, 0x0F);
    bv_push(o, 0x0B);
    /* i = i + 1 */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* br $next */
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* return 0 */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x0B);
}

void emit_helper_isdigit(struct ByteVec *o) {
    /* (param $c i32) (result i32), no extra locals */
    bv_u32(o, 0);
    /* (i32.ge_u c 48) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 48);
    bv_push(o, 0x4F);
    /* (i32.le_u c 57) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 57);
    bv_push(o, 0x4D);
    /* i32.and */
    bv_push(o, 0x71);
    bv_push(o, 0x0B);
}

void emit_helper_isalpha(struct ByteVec *o) {
    /* (param $c i32) (result i32), no extra locals */
    bv_u32(o, 0);
    /* (i32.ge_u c 65) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 65);
    bv_push(o, 0x4F);
    /* (i32.le_u c 90) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 90);
    bv_push(o, 0x4D);
    bv_push(o, 0x71);
    /* (i32.ge_u c 97) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 97);
    bv_push(o, 0x4F);
    /* (i32.le_u c 122) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 122);
    bv_push(o, 0x4D);
    bv_push(o, 0x71);
    /* i32.or */
    bv_push(o, 0x72);
    bv_push(o, 0x0B);
}

void emit_helper_isalnum(struct ByteVec *o) {
    /* (param $c i32) (result i32), no extra locals */
    bv_u32(o, 0);
    /* call isdigit(c) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x10); bv_u32(o, bin_find_func("isdigit"));
    /* call isalpha(c) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x10); bv_u32(o, bin_find_func("isalpha"));
    /* i32.or */
    bv_push(o, 0x72);
    bv_push(o, 0x0B);
}

void emit_helper_isspace(struct ByteVec *o) {
    /* (param $c i32) (result i32), no extra locals */
    bv_u32(o, 0);
    /* (i32.eq c 32) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 32);
    bv_push(o, 0x46);
    /* (i32.eq c 9) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 9);
    bv_push(o, 0x46);
    bv_push(o, 0x72);
    /* (i32.eq c 10) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 10);
    bv_push(o, 0x46);
    /* (i32.eq c 13) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 13);
    bv_push(o, 0x46);
    /* (i32.eq c 12) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 12);
    bv_push(o, 0x46);
    bv_push(o, 0x72);
    bv_push(o, 0x72);
    bv_push(o, 0x72);
    bv_push(o, 0x0B);
}

void emit_helper_isupper(struct ByteVec *o) {
    /* (param $c i32) (result i32), no extra locals */
    bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 65);
    bv_push(o, 0x4F);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 90);
    bv_push(o, 0x4D);
    bv_push(o, 0x71);
    bv_push(o, 0x0B);
}

void emit_helper_islower(struct ByteVec *o) {
    /* (param $c i32) (result i32), no extra locals */
    bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 97);
    bv_push(o, 0x4F);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 122);
    bv_push(o, 0x4D);
    bv_push(o, 0x71);
    bv_push(o, 0x0B);
}

void emit_helper_isprint(struct ByteVec *o) {
    /* (param $c i32) (result i32), no extra locals */
    bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 32);
    bv_push(o, 0x4F);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 126);
    bv_push(o, 0x4D);
    bv_push(o, 0x71);
    bv_push(o, 0x0B);
}

void emit_helper_ispunct(struct ByteVec *o) {
    /* (param $c i32) (result i32), no extra locals */
    bv_u32(o, 0);
    /* call isprint(c) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x10); bv_u32(o, bin_find_func("isprint"));
    /* i32.eqz (call isalnum(c)) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x10); bv_u32(o, bin_find_func("isalnum"));
    bv_push(o, 0x45);
    /* i32.ne c 32 */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 32);
    bv_push(o, 0x47);
    bv_push(o, 0x71);
    bv_push(o, 0x71);
    bv_push(o, 0x0B);
}

void emit_helper_isxdigit(struct ByteVec *o) {
    /* (param $c i32) (result i32), no extra locals */
    bv_u32(o, 0);
    /* call isdigit(c) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x10); bv_u32(o, bin_find_func("isdigit"));
    /* (i32.and (i32.ge_u c 65) (i32.le_u c 70)) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 65);
    bv_push(o, 0x4F);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 70);
    bv_push(o, 0x4D);
    bv_push(o, 0x71);
    /* (i32.and (i32.ge_u c 97) (i32.le_u c 102)) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 97);
    bv_push(o, 0x4F);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 102);
    bv_push(o, 0x4D);
    bv_push(o, 0x71);
    bv_push(o, 0x72);
    bv_push(o, 0x72);
    bv_push(o, 0x0B);
}

void emit_helper_toupper(struct ByteVec *o) {
    /* (param $c i32) (result i32), no extra locals */
    bv_u32(o, 0);
    /* call islower(c) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x10); bv_u32(o, bin_find_func("islower"));
    /* if (result i32) */
    bv_push(o, 0x04); bv_push(o, 0x7F);
    /* then: c - 32 */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 32);
    bv_push(o, 0x6B);
    /* else */
    bv_push(o, 0x05);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
}

void emit_helper_tolower(struct ByteVec *o) {
    /* (param $c i32) (result i32), no extra locals */
    bv_u32(o, 0);
    /* call isupper(c) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x10); bv_u32(o, bin_find_func("isupper"));
    /* if (result i32) */
    bv_push(o, 0x04); bv_push(o, 0x7F);
    /* then: c + 32 */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 32);
    bv_push(o, 0x6A);
    /* else */
    bv_push(o, 0x05);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
}

void emit_helper_abs(struct ByteVec *o) {
    /* (param $n i32) (result i32), no extra locals */
    bv_u32(o, 0);
    /* i32.lt_s n 0 */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x48);
    /* if (result i32) */
    bv_push(o, 0x04); bv_push(o, 0x7F);
    /* then: 0 - n */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x6B);
    /* else: n */
    bv_push(o, 0x05);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
}

void emit_helper_atoi(struct ByteVec *o) {
    /* (param $s i32) (result i32), 3 extra locals: $n(1), $neg(2), $c(3) */
    bv_u32(o, 1); bv_u32(o, 3); bv_push(o, 0x7F);
    /* n = 0 */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 1);
    /* neg = 0 */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 2);
    /* block $dsp { loop $sp -- skip spaces */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* c = load8_u(s) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* br_if $dsp (c != 32) */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x41); bv_i32(o, 32);
    bv_push(o, 0x47);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* s = s + 1 */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 0);
    /* br $sp */
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* c = load8_u(s) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* if (c == 45) { neg = 1; s++ } */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x41); bv_i32(o, 45);
    bv_push(o, 0x46);
    bv_push(o, 0x04); bv_push(o, 0x40);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x21); bv_u32(o, 2);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 0);
    bv_push(o, 0x0B);
    /* if (c == 43) { s++ } */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x41); bv_i32(o, 43);
    bv_push(o, 0x46);
    bv_push(o, 0x04); bv_push(o, 0x40);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 0);
    bv_push(o, 0x0B);
    /* block $done { loop $dig -- digit loop */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* c = load8_u(s) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* br_if $done (i32.or (c < 48) (c > 57)) */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x41); bv_i32(o, 48);
    bv_push(o, 0x49);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x41); bv_i32(o, 57);
    bv_push(o, 0x4B);
    bv_push(o, 0x72);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* n = n*10 + (c - 48) */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x41); bv_i32(o, 10);
    bv_push(o, 0x6C);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x41); bv_i32(o, 48);
    bv_push(o, 0x6B);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 1);
    /* s = s + 1 */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 0);
    /* br $dig */
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* if (result i32) neg (then 0-n) (else n) */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x04); bv_push(o, 0x7F);
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x6B);
    bv_push(o, 0x05);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
}

void emit_helper_puts(struct ByteVec *o) {
    /* (param $s i32) (result i32), 1 extra local: $c(1) */
    bv_u32(o, 1); bv_u32(o, 1); bv_push(o, 0x7F);
    /* block $done { loop $pr */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* c = load8_u(s) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 1);
    /* br_if $done (eqz c) */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x45);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* drop(call putchar(c)) */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x10); bv_u32(o, bin_find_func("putchar"));
    bv_push(o, 0x1A);
    /* s = s + 1 */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 0);
    /* br $pr */
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* drop(call putchar(10)) */
    bv_push(o, 0x41); bv_i32(o, 10);
    bv_push(o, 0x10); bv_u32(o, bin_find_func("putchar"));
    bv_push(o, 0x1A);
    /* return 0 */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x0B);
}

void emit_helper_srand(struct ByteVec *o) {
    /* (param $seed i32), no result, no extra locals */
    bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x24); bv_u32(o, bin_global_idx("__rand_seed"));
    bv_push(o, 0x0B);
}

void emit_helper_rand(struct ByteVec *o) {
    /* (result i32), no params, no extra locals */
    bv_u32(o, 0);
    /* seed = seed * 1103515245 + 12345 */
    bv_push(o, 0x23); bv_u32(o, bin_global_idx("__rand_seed"));
    bv_push(o, 0x41); bv_i32(o, 1103515245);
    bv_push(o, 0x6C);
    bv_push(o, 0x41); bv_i32(o, 12345);
    bv_push(o, 0x6A);
    bv_push(o, 0x24); bv_u32(o, bin_global_idx("__rand_seed"));
    /* return (seed >> 16) & 0x7fff */
    bv_push(o, 0x23); bv_u32(o, bin_global_idx("__rand_seed"));
    bv_push(o, 0x41); bv_i32(o, 16);
    bv_push(o, 0x76);
    bv_push(o, 0x41); bv_i32(o, 32767);
    bv_push(o, 0x71);
    bv_push(o, 0x0B);
}

void emit_helper_calloc(struct ByteVec *o) {
    /* (param $nmemb i32) (param $size i32) (result i32), 2 extra locals: $ptr(2), $total(3) */
    bv_u32(o, 1); bv_u32(o, 2); bv_push(o, 0x7F);
    /* total = nmemb * size */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x6C);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* ptr = malloc(total) */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x10); bv_u32(o, bin_find_func("malloc"));
    bv_push(o, 0x21); bv_u32(o, 2);
    /* drop(memset(ptr, 0, total)) */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x10); bv_u32(o, bin_find_func("memset"));
    bv_push(o, 0x1A);
    /* return ptr */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x0B);
}

void emit_helper_strcpy(struct ByteVec *o) {
    /* (param $dst i32) (param $src i32) (result i32), 1 extra local: $d(2) */
    bv_u32(o, 1); bv_u32(o, 1); bv_push(o, 0x7F);
    /* d = dst */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 2);
    /* block $done { loop $copy */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* store8 d, load8_u(src) */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x3A); bv_u32(o, 0); bv_u32(o, 0);
    /* br_if $done (eqz load8_u(src)) */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x45);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* d = d + 1 */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 2);
    /* src = src + 1 */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 1);
    /* br $copy */
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* return dst */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x0B);
}

void emit_helper_strcat(struct ByteVec *o) {
    /* (param $dst i32) (param $src i32) (result i32), 1 extra local: $d(2) */
    bv_u32(o, 1); bv_u32(o, 1); bv_push(o, 0x7F);
    /* d = dst + strlen(dst) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x10); bv_u32(o, bin_find_func("strlen"));
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 2);
    /* drop(strcpy(d, src)) */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x10); bv_u32(o, bin_find_func("strcpy"));
    bv_push(o, 0x1A);
    /* return dst */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x0B);
}

void emit_helper_strchr(struct ByteVec *o) {
    /* (param $s i32) (param $c i32) (result i32), 1 extra local: $ch(2) */
    bv_u32(o, 1); bv_u32(o, 1); bv_push(o, 0x7F);
    /* block $done { loop $scan */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* ch = load8_u(s) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 2);
    /* if (ch == c) return s */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x46);
    bv_push(o, 0x04); bv_push(o, 0x40);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x0F);
    bv_push(o, 0x0B);
    /* br_if $done (eqz ch) */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x45);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* s = s + 1 */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 0);
    /* br $scan */
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* return 0 */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x0B);
}

void emit_helper_strrchr(struct ByteVec *o) {
    /* (param $s i32) (param $c i32) (result i32), 2 extra locals: $last(2), $ch(3) */
    bv_u32(o, 1); bv_u32(o, 2); bv_push(o, 0x7F);
    /* last = 0 */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 2);
    /* block $done { loop $scan */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* ch = load8_u(s) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* if (ch == c) last = s */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x46);
    bv_push(o, 0x04); bv_push(o, 0x40);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 2);
    bv_push(o, 0x0B);
    /* br_if $done (eqz ch) */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x45);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* s = s + 1 */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 0);
    /* br $scan */
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* return last */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x0B);
}

void emit_helper_strstr(struct ByteVec *o) {
    /* (param $hay i32) (param $needle i32) (result i32), 2 extra locals: $h(2), $n(3) */
    bv_u32(o, 1); bv_u32(o, 2); bv_push(o, 0x7F);
    /* if (eqz load8_u(needle)) return hay */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x45);
    bv_push(o, 0x04); bv_push(o, 0x40);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x0F);
    bv_push(o, 0x0B);
    /* block $notfound { loop $outer */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* br_if $notfound (eqz load8_u(hay)) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x45);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* h = hay */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 2);
    /* n = needle */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* block $nomatch { loop $inner */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* if (eqz load8_u(n)) return hay */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x45);
    bv_push(o, 0x04); bv_push(o, 0x40);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x0F);
    bv_push(o, 0x0B);
    /* br_if $nomatch (ne load8_u(h) load8_u(n)) */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x47);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* h = h + 1 */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 2);
    /* n = n + 1 */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* br $inner */
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* hay = hay + 1 */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 0);
    /* br $outer */
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* return 0 */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x0B);
}

void emit_helper_strncmp(struct ByteVec *o) {
    /* (param $a i32) (param $b i32) (param $n i32) (result i32)
       3 extra locals: $i(3), $ca(4), $cb(5) */
    bv_u32(o, 1); bv_u32(o, 3); bv_push(o, 0x7F);
    /* i = 0 */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* block $done { loop $cmp */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* br_if $done (i >= n) */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x4F);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* ca = load8_u(a) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 4);
    /* cb = load8_u(b) */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 5);
    /* if (ca != cb) return ca - cb */
    bv_push(o, 0x20); bv_u32(o, 4);
    bv_push(o, 0x20); bv_u32(o, 5);
    bv_push(o, 0x47);
    bv_push(o, 0x04); bv_push(o, 0x40);
    bv_push(o, 0x20); bv_u32(o, 4);
    bv_push(o, 0x20); bv_u32(o, 5);
    bv_push(o, 0x6B);
    bv_push(o, 0x0F);
    bv_push(o, 0x0B);
    /* br_if $done (eqz ca) */
    bv_push(o, 0x20); bv_u32(o, 4);
    bv_push(o, 0x45);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* a = a + 1 */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 0);
    /* b = b + 1 */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 1);
    /* i = i + 1 */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* br $cmp */
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* return 0 */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x0B);
}

void emit_helper_strncat(struct ByteVec *o) {
    /* (param $dst i32) (param $src i32) (param $n i32) (result i32)
       2 extra locals: $d(3), $i(4) */
    bv_u32(o, 1); bv_u32(o, 2); bv_push(o, 0x7F);
    /* d = dst + strlen(dst) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x10); bv_u32(o, bin_find_func("strlen"));
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* i = 0 */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 4);
    /* block $done { loop $cp */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* br_if $done (i >= n) */
    bv_push(o, 0x20); bv_u32(o, 4);
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x4F);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* br_if $done (eqz load8_u(src)) */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x45);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* store8 d, load8_u(src) */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x3A); bv_u32(o, 0); bv_u32(o, 0);
    /* d = d + 1 */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* src = src + 1 */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 1);
    /* i = i + 1 */
    bv_push(o, 0x20); bv_u32(o, 4);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 4);
    /* br $cp */
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* store8 d, 0 */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x3A); bv_u32(o, 0); bv_u32(o, 0);
    /* return dst */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x0B);
}

void emit_helper_memmove(struct ByteVec *o) {
    /* (param $dst i32) (param $src i32) (param $n i32) (result i32), 1 extra local: $i(3) */
    bv_u32(o, 1); bv_u32(o, 1); bv_push(o, 0x7F);
    /* if (dst <= src) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x4D);
    bv_push(o, 0x04); bv_push(o, 0x40);
    /* then: drop(memcpy(dst, src, n)) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x10); bv_u32(o, bin_find_func("memcpy"));
    bv_push(o, 0x1A);
    /* else: backward copy */
    bv_push(o, 0x05);
    /* i = n */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* block $done { loop $bk */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* br_if $done (eqz i) */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x45);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* i = i - 1 */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6B);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* store8(dst+i, load8_u(src+i)) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x6A);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x6A);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x3A); bv_u32(o, 0); bv_u32(o, 0);
    /* br $bk */
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* end if */
    bv_push(o, 0x0B);
    /* return dst */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x0B);
}

void emit_helper_memchr(struct ByteVec *o) {
    /* (param $s i32) (param $c i32) (param $n i32) (result i32), 1 extra local: $i(3) */
    bv_u32(o, 1); bv_u32(o, 1); bv_push(o, 0x7F);
    /* i = 0 */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* block $done { loop $scan */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* br_if $done (i >= n) */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x4F);
    bv_push(o, 0x0D); bv_u32(o, 1);
    /* if (load8_u(s+i) == c) return s+i */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x6A);
    bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x46);
    bv_push(o, 0x04); bv_push(o, 0x40);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x6A);
    bv_push(o, 0x0F);
    bv_push(o, 0x0B);
    /* i = i + 1 */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x41); bv_i32(o, 1);
    bv_push(o, 0x6A);
    bv_push(o, 0x21); bv_u32(o, 3);
    /* br $scan */
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B);
    bv_push(o, 0x0B);
    /* return 0 */
    bv_push(o, 0x41); bv_i32(o, 0);
    bv_push(o, 0x0B);
}

void emit_helper_strtol(struct ByteVec *o) {
    /* (param $s i32) (param $endptr i32) (param $base i32) (result i32), no extra locals */
    bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x10); bv_u32(o, bin_find_func("atoi"));
    bv_push(o, 0x0B);
}

void emit_helper_print_float(struct ByteVec *o) {
    int j;
    /* (param $v f64) -> void
       extra locals: 3 i32 (int_part=1, frac_part=2, i=3), 1 f64 (frac=4) */
    bv_u32(o, 2); /* 2 local groups */
    bv_u32(o, 3); bv_push(o, 0x7F); /* 3 i32 */
    bv_u32(o, 1); bv_push(o, 0x7C); /* 1 f64 */

    /* if (v < 0.0) { putchar('-'); v = -v; } */
    bv_push(o, 0x20); bv_u32(o, 0); /* local.get $v */
    bv_push(o, 0x44); /* f64.const 0.0 */
    for (j = 0; j < 8; j++) bv_push(o, 0x00);
    bv_push(o, 0x63); /* f64.lt */
    bv_push(o, 0x04); bv_push(o, 0x40); /* if void */
      bv_push(o, 0x41); bv_i32(o, 45); /* i32.const '-' */
      bv_push(o, 0x10); bv_u32(o, bin_find_func("putchar"));
      bv_push(o, 0x1A); /* drop */
      bv_push(o, 0x20); bv_u32(o, 0); /* local.get $v */
      bv_push(o, 0x9A); /* f64.neg */
      bv_push(o, 0x21); bv_u32(o, 0); /* local.set $v */
    bv_push(o, 0x0B); /* end if */

    /* int_part = i32.trunc_f64_s(v) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0xAA); /* i32.trunc_f64_s */
    bv_push(o, 0x21); bv_u32(o, 1); /* local.set $int_part */

    /* __print_int(int_part) */
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0x10); bv_u32(o, bin_find_func("__print_int"));

    /* putchar('.') */
    bv_push(o, 0x41); bv_i32(o, 46);
    bv_push(o, 0x10); bv_u32(o, bin_find_func("putchar"));
    bv_push(o, 0x1A);

    /* frac = v - f64.convert_i32_s(int_part) */
    bv_push(o, 0x20); bv_u32(o, 0);
    bv_push(o, 0x20); bv_u32(o, 1);
    bv_push(o, 0xB7); /* f64.convert_i32_s */
    bv_push(o, 0xA1); /* f64.sub */
    /* frac *= 1000000.0 - bytes: 00 00 00 00 80 84 2E 41 */
    bv_push(o, 0x44);
    bv_push(o, 0x00); bv_push(o, 0x00); bv_push(o, 0x00); bv_push(o, 0x00);
    bv_push(o, 0x80); bv_push(o, 0x84); bv_push(o, 0x2E); bv_push(o, 0x41);
    bv_push(o, 0xA2); /* f64.mul */
    /* frac += 0.5 - bytes: 00 00 00 00 00 00 E0 3F */
    bv_push(o, 0x44);
    bv_push(o, 0x00); bv_push(o, 0x00); bv_push(o, 0x00); bv_push(o, 0x00);
    bv_push(o, 0x00); bv_push(o, 0x00); bv_push(o, 0xE0); bv_push(o, 0x3F);
    bv_push(o, 0xA0); /* f64.add */
    bv_push(o, 0x21); bv_u32(o, 4); /* local.set $frac */

    /* frac_part = i32.trunc_f64_s(frac) */
    bv_push(o, 0x20); bv_u32(o, 4);
    bv_push(o, 0xAA); /* i32.trunc_f64_s */
    bv_push(o, 0x21); bv_u32(o, 2); /* local.set $frac_part */

    /* i = 100000 */
    bv_push(o, 0x41); bv_i32(o, 100000);
    bv_push(o, 0x21); bv_u32(o, 3); /* local.set $i */

    /* block $done { loop $lp { */
    bv_push(o, 0x02); bv_push(o, 0x40);
    bv_push(o, 0x03); bv_push(o, 0x40);
    /* br_if $done if i == 0 */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x45); /* i32.eqz */
    bv_push(o, 0x0D); bv_u32(o, 1); /* br_if depth 1 = $done */
    /* putchar('0' + frac_part / i) */
    bv_push(o, 0x41); bv_i32(o, 48);
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x6E); /* i32.div_u */
    bv_push(o, 0x6A); /* i32.add */
    bv_push(o, 0x10); bv_u32(o, bin_find_func("putchar"));
    bv_push(o, 0x1A); /* drop */
    /* frac_part %= i */
    bv_push(o, 0x20); bv_u32(o, 2);
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x70); /* i32.rem_u */
    bv_push(o, 0x21); bv_u32(o, 2);
    /* i /= 10 */
    bv_push(o, 0x20); bv_u32(o, 3);
    bv_push(o, 0x41); bv_i32(o, 10);
    bv_push(o, 0x6E); /* i32.div_u */
    bv_push(o, 0x21); bv_u32(o, 3);
    /* br $lp */
    bv_push(o, 0x0C); bv_u32(o, 0);
    bv_push(o, 0x0B); /* end loop */
    bv_push(o, 0x0B); /* end block */

    bv_push(o, 0x0B); /* end function */
}

/* --- Binary module codegen --- */

void gen_module_bin(struct Node *prog) {
    struct ByteVec *out;
    struct ByteVec *sec;
    struct ByteVec *fb;
    int i;
    int j;
    int nlocal_funcs;
    int pif1[1];
    int pif2[16];
    int fi2;
    int j2;
    int ret_flt;
    int nparams2;

    out = bv_new(65536);
    sec = bv_new(32768);
    fb = bv_new(8192);

    bin_types = (struct BinTypeEntry **)malloc(MAX_BIN_TYPES * sizeof(void *));
    bin_ntypes = 0;
    bin_funcs = (struct BinFuncEntry **)malloc(MAX_BIN_FUNCS * sizeof(void *));
    bin_nfuncs = 0;

    bin_brk_at = (int *)malloc(MAX_LOOP_DEPTH * sizeof(int));
    bin_cont_at = (int *)malloc(MAX_LOOP_DEPTH * sizeof(int));
    bin_loop_at = (int *)malloc(MAX_LOOP_DEPTH * sizeof(int));

    /* Register all functions */
    bin_add_func("__proc_exit", 1, 0);
    bin_add_func("__fd_write", 4, 1);
    bin_add_func("__fd_read", 4, 1);
    bin_add_func("putchar", 1, 1);
    bin_add_func("getchar", 0, 1);
    bin_add_func("__print_int", 1, 0);
    bin_add_func("__print_str", 1, 0);
    bin_add_func("__print_hex", 1, 0);
    bin_add_func("malloc", 1, 1);
    bin_add_func("free", 1, 0);
    bin_add_func("strlen", 1, 1);
    bin_add_func("strcmp", 2, 1);
    bin_add_func("strncpy", 3, 1);
    bin_add_func("memcpy", 3, 1);
    bin_add_func("memset", 3, 1);
    bin_add_func("memcmp", 3, 1);
    /* new libc helpers */
    bin_add_func("isdigit", 1, 1);
    bin_add_func("isalpha", 1, 1);
    bin_add_func("isalnum", 1, 1);
    bin_add_func("isspace", 1, 1);
    bin_add_func("isupper", 1, 1);
    bin_add_func("islower", 1, 1);
    bin_add_func("isprint", 1, 1);
    bin_add_func("ispunct", 1, 1);
    bin_add_func("isxdigit", 1, 1);
    bin_add_func("toupper", 1, 1);
    bin_add_func("tolower", 1, 1);
    bin_add_func("abs", 1, 1);
    bin_add_func("atoi", 1, 1);
    bin_add_func("puts", 1, 1);
    bin_add_func("srand", 1, 0);
    bin_add_func("rand", 0, 1);
    bin_add_func("calloc", 2, 1);
    bin_add_func("strcpy", 2, 1);
    bin_add_func("strcat", 2, 1);
    bin_add_func("strchr", 2, 1);
    bin_add_func("strrchr", 2, 1);
    bin_add_func("strstr", 2, 1);
    bin_add_func("strncmp", 3, 1);
    bin_add_func("strncat", 3, 1);
    bin_add_func("memmove", 3, 1);
    bin_add_func("memchr", 3, 1);
    bin_add_func("strtol", 3, 1);
    pif1[0] = 1; /* f64 param */
    bin_add_func_f("__print_float", 1, pif1, 0, 0);
    for (i = 0; i < prog->ival2; i++) {
        fi2 = find_funcsig(prog->list[i]->sval);
        nparams2 = prog->list[i]->ival2;
        ret_flt = 0;
        for (j2 = 0; j2 < 16; j2++) pif2[j2] = 0;
        if (fi2 >= 0) {
            ret_flt = func_sigs[fi2]->ret_is_float ? 1 : 0;
            for (j2 = 0; j2 < nparams2 && j2 < 16; j2++) {
                pif2[j2] = func_sigs[fi2]->param_is_float[j2] ? 1 : 0;
            }
        }
        bin_add_func_f(prog->list[i]->sval, nparams2, pif2,
                       (prog->list[i]->ival != 1) ? 1 : 0, ret_flt);
    }
    bin_add_func("_start", 0, 0);

    /* Magic + version */
    bv_push(out, 0x00); bv_push(out, 0x61);
    bv_push(out, 0x73); bv_push(out, 0x6D);
    bv_push(out, 0x01); bv_push(out, 0x00);
    bv_push(out, 0x00); bv_push(out, 0x00);

    /* Type section (id=1) */
    bv_reset(sec);
    bv_u32(sec, bin_ntypes);
    for (i = 0; i < bin_ntypes; i++) {
        bv_push(sec, 0x60);
        bv_u32(sec, bin_types[i]->nparams);
        for (j = 0; j < bin_types[i]->nparams; j++) {
            if (j < 16 && bin_types[i]->param_is_float[j]) {
                bv_push(sec, 0x7C); /* f64 */
            } else {
                bv_push(sec, 0x7F); /* i32 */
            }
        }
        if (bin_types[i]->has_result) {
            bv_u32(sec, 1);
            if (bin_types[i]->result_is_float) {
                bv_push(sec, 0x7C); /* f64 */
            } else {
                bv_push(sec, 0x7F); /* i32 */
            }
        } else {
            bv_u32(sec, 0);
        }
    }
    bv_section(out, 1, sec);

    /* Import section (id=2) */
    bv_reset(sec);
    bv_u32(sec, 3);
    bv_name(sec, "wasi_snapshot_preview1", 22);
    bv_name(sec, "proc_exit", 9);
    bv_push(sec, 0x00);
    bv_u32(sec, bin_funcs[0]->type_idx);
    bv_name(sec, "wasi_snapshot_preview1", 22);
    bv_name(sec, "fd_write", 8);
    bv_push(sec, 0x00);
    bv_u32(sec, bin_funcs[1]->type_idx);
    bv_name(sec, "wasi_snapshot_preview1", 22);
    bv_name(sec, "fd_read", 7);
    bv_push(sec, 0x00);
    bv_u32(sec, bin_funcs[2]->type_idx);
    bv_section(out, 2, sec);

    /* Function section (id=3) */
    nlocal_funcs = bin_nfuncs - 3;
    bv_reset(sec);
    bv_u32(sec, nlocal_funcs);
    for (i = 3; i < bin_nfuncs; i++) {
        bv_u32(sec, bin_funcs[i]->type_idx);
    }
    bv_section(out, 3, sec);

    /* Table section (id=4) — only if function pointers are used */
    if (fn_table_count > 0) {
        bv_reset(sec);
        bv_u32(sec, 1); /* one table */
        bv_push(sec, 0x70); /* funcref */
        bv_push(sec, 0x00); /* limits: min only */
        bv_u32(sec, fn_table_count);
        bv_section(out, 4, sec);
    }

    /* Memory section (id=5) */
    bv_reset(sec);
    bv_u32(sec, 1);
    bv_push(sec, 0x00);
    bv_u32(sec, 256);
    bv_section(out, 5, sec);

    /* Global section (id=6) */
    bv_reset(sec);
    bv_u32(sec, 3 + nglobals);
    bv_push(sec, 0x7F); bv_push(sec, 0x01);
    bv_push(sec, 0x41); bv_i32(sec, data_ptr); bv_push(sec, 0x0B);
    /* __free_list = 0 */
    bv_push(sec, 0x7F); bv_push(sec, 0x01);
    bv_push(sec, 0x41); bv_i32(sec, 0); bv_push(sec, 0x0B);
    /* __rand_seed = 1 */
    bv_push(sec, 0x7F); bv_push(sec, 0x01);
    bv_push(sec, 0x41); bv_i32(sec, 1); bv_push(sec, 0x0B);
    for (i = 0; i < nglobals; i++) {
        if (globals_tbl[i]->gv_is_float) {
            int gk;
            bv_push(sec, 0x7C); bv_push(sec, 0x01); /* f64, mutable */
            bv_push(sec, 0x44); /* f64.const */
            for (gk = 0; gk < 8; gk++) bv_push(sec, 0x00); /* 0.0 */
            bv_push(sec, 0x0B);
        } else {
            bv_push(sec, 0x7F); bv_push(sec, 0x01);
            bv_push(sec, 0x41); bv_i32(sec, globals_tbl[i]->init_val); bv_push(sec, 0x0B);
        }
    }
    bv_section(out, 6, sec);

    /* Export section (id=7) */
    bv_reset(sec);
    bv_u32(sec, 2);
    bv_name(sec, "memory", 6);
    bv_push(sec, 0x02);
    bv_u32(sec, 0);
    bv_name(sec, "_start", 6);
    bv_push(sec, 0x00);
    bv_u32(sec, bin_find_func("_start"));
    bv_section(out, 7, sec);

    /* Element section (id=9) — only if function pointers are used */
    if (fn_table_count > 0) {
        int eli;
        bv_reset(sec);
        bv_u32(sec, 1); /* one elem segment */
        bv_u32(sec, 0); /* table index 0 */
        bv_push(sec, 0x41); bv_i32(sec, 0); bv_push(sec, 0x0B); /* offset = i32.const 0; end */
        bv_u32(sec, fn_table_count);
        for (eli = 0; eli < fn_table_count; eli++) {
            bv_u32(sec, bin_find_func(fn_table_names[eli]));
        }
        bv_section(out, 9, sec);
    }

    /* Code section (id=10) */
    bv_reset(sec);
    bv_u32(sec, nlocal_funcs);
    bv_reset(fb); emit_helper_putchar(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_getchar(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_print_int(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_print_str(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_print_hex(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_malloc(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_free(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_strlen(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_strcmp(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_strncpy(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_memcpy(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_memset(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_memcmp(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_isdigit(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_isalpha(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_isalnum(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_isspace(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_isupper(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_islower(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_isprint(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_ispunct(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_isxdigit(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_toupper(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_tolower(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_abs(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_atoi(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_puts(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_srand(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_rand(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_calloc(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_strcpy(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_strcat(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_strchr(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_strrchr(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_strstr(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_strncmp(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_strncat(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_memmove(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_memchr(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_strtol(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    bv_reset(fb); emit_helper_print_float(fb); bv_u32(sec, fb->len); bv_append(sec, fb);
    for (i = 0; i < prog->ival2; i++) {
        gen_func_bin(sec, prog->list[i]);
    }
    /* _start body */
    bv_reset(fb);
    {
        int need_init_bin;
        int gi3;
        need_init_bin = 0;
        for (gi3 = 0; gi3 < nglobals; gi3++) {
            if (globals_tbl[gi3]->gv_arr_len > 0 && globals_tbl[gi3]->gv_arr_str_ids != (int *)0) {
                need_init_bin = 1;
            }
        }
        if (need_init_bin) {
            bv_u32(fb, 1); /* 1 local declaration group */
            bv_u32(fb, 1); /* 1 local of type i32 */
            bv_push(fb, 0x7F);
            for (gi3 = 0; gi3 < nglobals; gi3++) {
                if (globals_tbl[gi3]->gv_arr_len > 0 && globals_tbl[gi3]->gv_arr_str_ids != (int *)0) {
                    int ai3;
                    /* malloc(arr_len * 4) */
                    bv_push(fb, 0x41); bv_i32(fb, globals_tbl[gi3]->gv_arr_len * 4);
                    bv_push(fb, 0x10); bv_u32(fb, bin_find_func("malloc"));
                    /* tee to local 0, then set global */
                    bv_push(fb, 0x22); bv_u32(fb, 0); /* local.tee 0 */
                    bv_push(fb, 0x24); bv_u32(fb, bin_global_idx(globals_tbl[gi3]->name));
                    /* store each element */
                    for (ai3 = 0; ai3 < globals_tbl[gi3]->gv_arr_len; ai3++) {
                        bv_push(fb, 0x20); bv_u32(fb, 0); /* local.get 0 */
                        if (ai3 > 0) {
                            bv_push(fb, 0x41); bv_i32(fb, ai3 * 4);
                            bv_push(fb, 0x6A); /* i32.add */
                        }
                        bv_push(fb, 0x41); bv_i32(fb, str_table[globals_tbl[gi3]->gv_arr_str_ids[ai3]]->offset);
                        bv_push(fb, 0x36); bv_u32(fb, 2); bv_u32(fb, 0); /* i32.store align=2 offset=0 */
                    }
                }
            }
        } else {
            bv_u32(fb, 0); /* no locals */
        }
    }
    bv_push(fb, 0x10); bv_u32(fb, bin_find_func("main"));
    bv_push(fb, 0x10); bv_u32(fb, 0);
    bv_push(fb, 0x0B);
    bv_u32(sec, fb->len);
    bv_append(sec, fb);
    bv_section(out, 10, sec);

    /* Data section (id=11) */
    if (nstrings > 0) {
        bv_reset(sec);
        bv_u32(sec, nstrings);
        for (i = 0; i < nstrings; i++) {
            bv_push(sec, 0x00);
            bv_push(sec, 0x41); bv_i32(sec, str_table[i]->offset); bv_push(sec, 0x0B);
            bv_u32(sec, str_table[i]->len + 1);
            for (j = 0; j < str_table[i]->len; j++) {
                bv_push(sec, str_table[i]->data[j] & 0xFF);
            }
            bv_push(sec, 0x00);
        }
        bv_section(out, 11, sec);
    }

    /* Write binary to stdout */
    for (i = 0; i < out->len; i++) {
        putchar(out->data[i] & 0xFF);
    }
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    struct Node *prog;

    init_kw_table();
    init_macros();
    init_strings();
    init_structs();
    init_globals();
    init_func_sigs();
    init_fnptr_registry();
    init_enum_consts();
    init_type_aliases();
    init_local_tracking();
    init_loop_labels();

    read_source();
    lex_init();
    advance_tok();

    prog = parse_program();
    if (binary_mode) {
        gen_module_bin(prog);
    } else {
        indent_level = 0;
        gen_module(prog);
    }

    return 0;
}
