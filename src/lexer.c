/* lexer.c — Include stack, lexer, macros, string table, tokenizer */

/* --- Include stack --- */

struct IncludeStack **inc_stack;
int inc_depth;

char **inc_files;
int ninc_files;

void init_includes(void) {
    inc_stack = (struct IncludeStack **)malloc(MAX_INCLUDE_DEPTH * sizeof(void *));
    inc_depth = 0;
    inc_files = (char **)malloc(MAX_INCLUDES * sizeof(char *));
    ninc_files = 0;
}

int already_included(char *fname) {
    int i;
    for (i = 0; i < ninc_files; i++) {
        if (strcmp(inc_files[i], fname) == 0) return 1;
    }
    return 0;
}

void mark_included(char *fname) {
    if (ninc_files < MAX_INCLUDES) {
        inc_files[ninc_files] = fname;
        ninc_files++;
    }
}

/* ================================================================
 * Lexer state
 * ================================================================ */

int lex_pos;
int lex_line;
int lex_col;

/* Preprocessor conditional stack */
int *pp_stack;
int pp_sp;

void lex_init(void) {
    lex_pos = 0;
    lex_line = 1;
    lex_col = 1;
    pp_stack = (int *)malloc(MAX_PP_DEPTH * sizeof(int));
    pp_sp = 0;
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

int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

void include_push(void) {
    struct IncludeStack *frame;
    if (inc_depth >= MAX_INCLUDE_DEPTH) {
        error(lex_line, lex_col, "#include nested too deep");
    }
    frame = (struct IncludeStack *)malloc(sizeof(struct IncludeStack));
    frame->is_src = src;
    frame->is_src_len = src_len;
    frame->is_lex_pos = lex_pos;
    frame->is_lex_line = lex_line;
    frame->is_lex_col = lex_col;
    inc_stack[inc_depth] = frame;
    inc_depth++;
}

int include_pop(void) {
    struct IncludeStack *frame;
    if (inc_depth <= 0) return 0;
    inc_depth--;
    frame = inc_stack[inc_depth];
    src = frame->is_src;
    src_len = frame->is_src_len;
    lex_pos = frame->is_lex_pos;
    lex_line = frame->is_lex_line;
    lex_col = frame->is_lex_col;
    return 1;
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
    kw_add("register", TOK_REGISTER);
    kw_add("auto", TOK_AUTO);
    kw_add("volatile", TOK_VOLATILE);
    kw_add("static", TOK_STATIC);
    kw_add("extern", TOK_EXTERN);
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

struct Macro **macros;
int nmacros;

void init_macros(void) {
    macros = (struct Macro **)malloc(MAX_MACROS * sizeof(void *));
    nmacros = 0;
    /* predefined macro: __STDC__ */
    macros[0] = (struct Macro *)malloc(sizeof(struct Macro));
    macros[0]->name = strdupn("__STDC__", 8);
    macros[0]->value = 1;
    nmacros = 1;
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
 * Preprocessor #if expression evaluator
 * ================================================================ */

int pp_expr(void);

void pp_skip_hws(void) {
    while (lp() == ' ' || lp() == '\t') la();
}

int pp_primary(void) {
    int val;
    char nm[128];
    int ni;
    int mi;
    int has_paren;

    pp_skip_hws();

    if (lp() == '(') {
        la();
        val = pp_expr();
        pp_skip_hws();
        if (lp() == ')') la();
        return val;
    }

    if (is_digit(lp())) {
        val = 0;
        if (lp() == '0' && (lp2() == 'x' || lp2() == 'X')) {
            la(); la();
            while (is_xdigit(lp())) {
                val = val * 16 + hex_val(la());
            }
        } else {
            while (is_digit(lp())) {
                val = val * 10 + (la() - '0');
            }
        }
        if (lp() == 'L' || lp() == 'l') la();
        return val;
    }

    if (lp() == '\'') {
        la();
        if (lp() == '\\') {
            la();
            val = la();
            if (val == 'n') val = 10;
            else if (val == 't') val = 9;
            else if (val == '0') val = 0;
        } else {
            val = la();
        }
        if (lp() == '\'') la();
        return val;
    }

    if (is_alpha(lp()) || lp() == '_') {
        ni = 0;
        while (is_alnum(lp()) || lp() == '_') {
            if (ni < 127) {
                nm[ni] = la();
                ni++;
            } else {
                la();
            }
        }
        nm[ni] = '\0';

        if (strcmp(nm, "defined") == 0) {
            pp_skip_hws();
            has_paren = 0;
            if (lp() == '(') {
                la();
                has_paren = 1;
                pp_skip_hws();
            }
            ni = 0;
            while (is_alnum(lp()) || lp() == '_') {
                if (ni < 127) {
                    nm[ni] = la();
                    ni++;
                } else {
                    la();
                }
            }
            nm[ni] = '\0';
            if (has_paren) {
                pp_skip_hws();
                if (lp() == ')') la();
            }
            if (find_macro(nm) >= 0) return 1;
            return 0;
        }

        if (strcmp(nm, "__LINE__") == 0) return lex_line;

        mi = find_macro(nm);
        if (mi >= 0) return macros[mi]->value;
        return 0;
    }

    return 0;
}

int pp_unary(void) {
    pp_skip_hws();
    if (lp() == '!') {
        la();
        return !pp_unary();
    }
    if (lp() == '-') {
        la();
        return -pp_unary();
    }
    if (lp() == '+') {
        la();
        return pp_unary();
    }
    return pp_primary();
}

int pp_mul(void) {
    int l;
    int r;
    char op;
    l = pp_unary();
    for (;;) {
        pp_skip_hws();
        op = lp();
        if (op == '*') {
            la();
            r = pp_unary();
            l = l * r;
        } else if (op == '/' && lp2() != '*') {
            la();
            r = pp_unary();
            if (r != 0) l = l / r;
        } else if (op == '%') {
            la();
            r = pp_unary();
            if (r != 0) l = l % r;
        } else {
            break;
        }
    }
    return l;
}

int pp_add(void) {
    int l;
    int r;
    char op;
    l = pp_mul();
    for (;;) {
        pp_skip_hws();
        op = lp();
        if (op == '+') {
            la();
            r = pp_mul();
            l = l + r;
        } else if (op == '-') {
            la();
            r = pp_mul();
            l = l - r;
        } else {
            break;
        }
    }
    return l;
}

int pp_rel(void) {
    int l;
    int r;
    l = pp_add();
    for (;;) {
        pp_skip_hws();
        if (lp() == '<' && lp2() == '=') {
            la(); la();
            r = pp_add();
            l = (l <= r);
        } else if (lp() == '>' && lp2() == '=') {
            la(); la();
            r = pp_add();
            l = (l >= r);
        } else if (lp() == '<') {
            la();
            r = pp_add();
            l = (l < r);
        } else if (lp() == '>') {
            la();
            r = pp_add();
            l = (l > r);
        } else {
            break;
        }
    }
    return l;
}

int pp_eq(void) {
    int l;
    int r;
    l = pp_rel();
    for (;;) {
        pp_skip_hws();
        if (lp() == '=' && lp2() == '=') {
            la(); la();
            r = pp_rel();
            l = (l == r);
        } else if (lp() == '!' && lp2() == '=') {
            la(); la();
            r = pp_rel();
            l = (l != r);
        } else {
            break;
        }
    }
    return l;
}

int pp_and(void) {
    int l;
    int r;
    l = pp_eq();
    for (;;) {
        pp_skip_hws();
        if (lp() == '&' && lp2() == '&') {
            la(); la();
            r = pp_eq();
            l = l && r;
        } else {
            break;
        }
    }
    return l;
}

int pp_expr(void) {
    int l;
    int r;
    l = pp_and();
    for (;;) {
        pp_skip_hws();
        if (lp() == '|' && lp2() == '|') {
            la(); la();
            r = pp_and();
            l = l || r;
        } else {
            break;
        }
    }
    return l;
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
    int sc_pos;
    int sc_line;
    int sc_col;

    skip_ws();
    t = (struct Token *)malloc(sizeof(struct Token));
    memset(t, 0, sizeof(struct Token));
    t->line = lex_line;
    t->col = lex_col;

    if (lex_pos >= src_len) {
        if (include_pop()) {
            return next_token();
        }
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

        /* --- conditional directives (always processed, even when skipping) --- */
        if (dlen == 5 && memcmp(src + s, "ifdef", 5) == 0) {
            if (pp_sp >= MAX_PP_DEPTH) error(lex_line, lex_col, "#ifdef nested too deep");
            while (lp() == ' ' || lp() == '\t') la();
            name = (char *)malloc(128);
            ni = 0;
            while (is_alnum(lp()) || lp() == '_') {
                if (ni < 127) { name[ni] = la(); ni++; } else { la(); }
            }
            name[ni] = '\0';
            if (pp_sp > 0 && pp_stack[pp_sp - 1] != PP_ACTIVE) {
                pp_stack[pp_sp] = PP_DONE;
            } else {
                pp_stack[pp_sp] = (find_macro(name) >= 0) ? PP_ACTIVE : PP_SKIPPING;
            }
            pp_sp++;
            while (lex_pos < src_len && lp() != '\n') la();
            return next_token();
        }
        if (dlen == 6 && memcmp(src + s, "ifndef", 6) == 0) {
            if (pp_sp >= MAX_PP_DEPTH) error(lex_line, lex_col, "#ifndef nested too deep");
            while (lp() == ' ' || lp() == '\t') la();
            name = (char *)malloc(128);
            ni = 0;
            while (is_alnum(lp()) || lp() == '_') {
                if (ni < 127) { name[ni] = la(); ni++; } else { la(); }
            }
            name[ni] = '\0';
            if (pp_sp > 0 && pp_stack[pp_sp - 1] != PP_ACTIVE) {
                pp_stack[pp_sp] = PP_DONE;
            } else {
                pp_stack[pp_sp] = (find_macro(name) >= 0) ? PP_SKIPPING : PP_ACTIVE;
            }
            pp_sp++;
            while (lex_pos < src_len && lp() != '\n') la();
            return next_token();
        }
        if (dlen == 2 && memcmp(src + s, "if", 2) == 0) {
            if (pp_sp >= MAX_PP_DEPTH) error(lex_line, lex_col, "#if nested too deep");
            if (pp_sp > 0 && pp_stack[pp_sp - 1] != PP_ACTIVE) {
                pp_stack[pp_sp] = PP_DONE;
            } else {
                val = pp_expr();
                pp_stack[pp_sp] = (val != 0) ? PP_ACTIVE : PP_SKIPPING;
            }
            pp_sp++;
            while (lex_pos < src_len && lp() != '\n') la();
            return next_token();
        }
        if (dlen == 4 && memcmp(src + s, "elif", 4) == 0) {
            if (pp_sp <= 0) error(lex_line, lex_col, "#elif without matching #if");
            if (pp_stack[pp_sp - 1] == PP_SKIPPING) {
                val = pp_expr();
                pp_stack[pp_sp - 1] = (val != 0) ? PP_ACTIVE : PP_SKIPPING;
            } else if (pp_stack[pp_sp - 1] == PP_ACTIVE) {
                pp_stack[pp_sp - 1] = PP_DONE;
            }
            /* PP_DONE stays PP_DONE */
            while (lex_pos < src_len && lp() != '\n') la();
            return next_token();
        }
        if (dlen == 4 && memcmp(src + s, "else", 4) == 0) {
            if (pp_sp <= 0) error(lex_line, lex_col, "#else without matching #if");
            if (pp_stack[pp_sp - 1] == PP_SKIPPING) {
                pp_stack[pp_sp - 1] = PP_ACTIVE;
            } else if (pp_stack[pp_sp - 1] == PP_ACTIVE) {
                pp_stack[pp_sp - 1] = PP_DONE;
            }
            /* PP_DONE stays PP_DONE */
            while (lex_pos < src_len && lp() != '\n') la();
            return next_token();
        }
        if (dlen == 5 && memcmp(src + s, "endif", 5) == 0) {
            if (pp_sp <= 0) error(lex_line, lex_col, "#endif without matching #if");
            pp_sp--;
            while (lex_pos < src_len && lp() != '\n') la();
            return next_token();
        }

        /* --- if skipping, ignore all other directives --- */
        if (pp_sp > 0 && pp_stack[pp_sp - 1] != PP_ACTIVE) {
            while (lex_pos < src_len && lp() != '\n') la();
            return next_token();
        }

        /* --- active-only directives --- */
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
        } else if (dlen == 7 && memcmp(src + s, "include", 7) == 0) {
            /* #include "filename" */
            char *inc_name;
            int inc_ni;
            int inc_fd;
            char *inc_buf;
            int inc_nread;
            while (lp() == ' ' || lp() == '\t') la();
            if (lp() == '"') {
                la();
                inc_name = (char *)malloc(256);
                inc_ni = 0;
                while (lp() != '"' && lp() != '\n' && lex_pos < src_len) {
                    if (inc_ni < 255) {
                        inc_name[inc_ni] = la();
                        inc_ni++;
                    } else {
                        error(lex_line, lex_col, "#include filename too long");
                    }
                }
                inc_name[inc_ni] = '\0';
                if (lp() == '"') la();
                /* skip to end of line */
                while (lex_pos < src_len && lp() != '\n') la();
                if (!already_included(inc_name)) {
                    mark_included(inc_name);
                    inc_fd = __open_file(inc_name, inc_ni);
                    if (inc_fd < 0) {
                        error(lex_line, lex_col, "cannot open include file");
                    }
                    inc_buf = (char *)malloc(MAX_INCLUDE_SRC);
                    inc_nread = __read_file(inc_fd, inc_buf, MAX_INCLUDE_SRC - 1);
                    __close_file(inc_fd);
                    if (inc_nread < 0) inc_nread = 0;
                    inc_buf[inc_nread] = '\0';
                    include_push();
                    src = inc_buf;
                    src_len = inc_nread;
                    lex_pos = 0;
                    lex_line = 1;
                    lex_col = 1;
                }
                return next_token();
            } else if (lp() == '<') {
                /* skip system includes — handled by host compiler */
                while (lex_pos < src_len && lp() != '\n') la();
                return next_token();
            }
        } else if (dlen == 5 && memcmp(src + s, "undef", 5) == 0) {
            while (lp() == ' ' || lp() == '\t') la();
            name = (char *)malloc(128);
            ni = 0;
            while (is_alnum(lp()) || lp() == '_') {
                if (ni < 127) { name[ni] = la(); ni++; } else { la(); }
            }
            name[ni] = '\0';
            mi = find_macro(name);
            if (mi >= 0) {
                for (i = mi; i < nmacros - 1; i++) {
                    macros[i] = macros[i + 1];
                }
                nmacros--;
            }
        } else if (dlen == 5 && memcmp(src + s, "error", 5) == 0) {
            while (lp() == ' ' || lp() == '\t') la();
            s = lex_pos;
            while (lex_pos < src_len && lp() != '\n') la();
            len = lex_pos - s;
            __s = (char *)malloc(len + 16);
            memcpy(__s, "#error ", 7);
            memcpy(__s + 7, src + s, len);
            __s[7 + len] = '\0';
            error(lex_line, lex_col, __s);
        }
        /* #pragma and unknown directives: silently skip */
        /* skip to end of line */
        while (lex_pos < src_len && lp() != '\n') la();
        return next_token();
    }

    /* preprocessor: skip non-directive tokens when in a false branch */
    if (pp_sp > 0 && pp_stack[pp_sp - 1] != PP_ACTIVE) {
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
            } else if (strcmp(t->text, "__LINE__") == 0) {
                t->kind = TOK_INT_LIT;
                t->int_val = t->line;
            } else if (strcmp(t->text, "__FILE__") == 0) {
                t->kind = TOK_STR_LIT;
                t->text = strdupn("stdin", 5);
                t->int_val = 5;
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
                ch = '\n'; la();
            } else if (ch == 'r') {
                ch = '\r'; la();
            } else if (ch == 't') {
                ch = '\t'; la();
            } else if (ch == '\\') {
                ch = '\\'; la();
            } else if (ch == '\'') {
                ch = '\''; la();
            } else if (ch == '"') {
                ch = '"'; la();
            } else if (ch == 'a') {
                ch = 7; la();
            } else if (ch == 'b') {
                ch = 8; la();
            } else if (ch == 'f') {
                ch = 12; la();
            } else if (ch == 'v') {
                ch = 11; la();
            } else if (ch == '?') {
                ch = 63; la();
            } else if (ch == 'x') {
                la(); /* consume 'x' */
                ch = 0;
                if (is_xdigit(lp())) { ch = hex_val(la()); }
                if (is_xdigit(lp())) { ch = ch * 16 + hex_val(la()); }
            } else if (ch >= '0' && ch <= '7') {
                ch = ch - '0';
                la();
                if (lp() >= '0' && lp() <= '7') { ch = ch * 8 + (la() - '0'); }
                if (lp() >= '0' && lp() <= '7') { ch = ch * 8 + (la() - '0'); }
            } else {
                la();
            }
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
        __s = (char *)malloc(2048);
        t->text = __s;
        i = 0;
        for (;;) {
            /* parse one string segment */
            while (lex_pos < src_len && lp() != '"') {
                if (lp() == '\\') {
                    la();
                    ch = lp();
                    if (ch == 'n') {
                        __s[i] = '\n'; i++; la();
                    } else if (ch == 'r') {
                        __s[i] = '\r'; i++; la();
                    } else if (ch == 't') {
                        __s[i] = '\t'; i++; la();
                    } else if (ch == '\\') {
                        __s[i] = '\\'; i++; la();
                    } else if (ch == '"') {
                        __s[i] = '"'; i++; la();
                    } else if (ch == '\'') {
                        __s[i] = '\''; i++; la();
                    } else if (ch == 'a') {
                        __s[i] = 7; i++; la();
                    } else if (ch == 'b') {
                        __s[i] = 8; i++; la();
                    } else if (ch == 'f') {
                        __s[i] = 12; i++; la();
                    } else if (ch == 'v') {
                        __s[i] = 11; i++; la();
                    } else if (ch == '?') {
                        __s[i] = 63; i++; la();
                    } else if (ch == 'x') {
                        la(); /* consume 'x' */
                        ch = 0;
                        if (is_xdigit(lp())) { ch = hex_val(la()); }
                        if (is_xdigit(lp())) { ch = ch * 16 + hex_val(la()); }
                        __s[i] = ch; i++;
                    } else if (ch >= '0' && ch <= '7') {
                        ch = ch - '0';
                        la();
                        if (lp() >= '0' && lp() <= '7') { ch = ch * 8 + (la() - '0'); }
                        if (lp() >= '0' && lp() <= '7') { ch = ch * 8 + (la() - '0'); }
                        __s[i] = ch; i++;
                    } else {
                        __s[i] = lp(); i++; la();
                    }
                } else {
                    __s[i] = la(); i++;
                }
                if (i >= 2046) break;
            }
            if (lp() == '"') la();
            /* check for adjacent string literal concatenation */
            sc_pos = lex_pos;
            sc_line = lex_line;
            sc_col = lex_col;
            while (lex_pos < src_len && (lp() == ' ' || lp() == '\t' || lp() == '\n' || lp() == '\r')) {
                la();
            }
            if (lex_pos < src_len && lp() == '"') {
                la(); /* consume opening quote of next segment */
            } else {
                /* not a string — restore position */
                lex_pos = sc_pos;
                lex_line = sc_line;
                lex_col = sc_col;
                break;
            }
        }
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
        if (lp() == '=') {
            la();
            t->kind = TOK_STAR_EQ;
        } else {
            t->kind = TOK_STAR;
        }
    } else if (c == '/') {
        if (lp() == '=') {
            la();
            t->kind = TOK_SLASH_EQ;
        } else {
            t->kind = TOK_SLASH;
        }
    } else if (c == '%') {
        if (lp() == '=') {
            la();
            t->kind = TOK_PERCENT_EQ;
        } else {
            t->kind = TOK_PERCENT;
        }
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

