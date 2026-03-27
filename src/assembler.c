/* assembler.c — WAT text to WASM binary assembler */

/* ================================================================
 * WAT Token Types
 * ================================================================ */

#define WTOK_LPAREN  1
#define WTOK_RPAREN  2
#define WTOK_KW      3
#define WTOK_NAME    4
#define WTOK_INT     5
#define WTOK_FLOAT   6
#define WTOK_STRING  7
#define WTOK_EOF     8

/* ================================================================
 * Assembler Limits
 * ================================================================ */

#define ASM_MAX_TYPES    128
#define ASM_MAX_FUNCS    512
#define ASM_MAX_GLOBALS  256
#define ASM_MAX_DATA     2048
#define ASM_MAX_LOCALS   512
#define ASM_MAX_LABELS   64
#define ASM_MAX_TYPE_NAMES 64
#define ASM_MAX_ELEM     256
#define ASM_MAX_PARAMS   16

/* ================================================================
 * Tokenizer State
 * ================================================================ */

char *asm_src;
int asm_src_len;
int asm_pos;
int asm_tok;
char *asm_tok_str;
int asm_tok_int;

/* Memory instruction offset (parsed from offset=N tokens) */
int asm_mem_offset;

/* Save/restore for lookahead */
int asm_saved_pos;
int asm_saved_tok;
char *asm_saved_str;
int asm_saved_int;

void asm_error(char *msg) {
    printf("assembler error: %s\n", msg);
    exit(1);
}

void asm_save(void) {
    asm_saved_pos = asm_pos;
    asm_saved_tok = asm_tok;
    asm_saved_str = asm_tok_str;
    asm_saved_int = asm_tok_int;
}

void asm_restore(void) {
    asm_pos = asm_saved_pos;
    asm_tok = asm_saved_tok;
    asm_tok_str = asm_saved_str;
    asm_tok_int = asm_saved_int;
}

void asm_skip_ws(void) {
    int c;
    while (asm_pos < asm_src_len) {
        c = asm_src[asm_pos] & 0xFF;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            asm_pos++;
        } else if (c == ';' && asm_pos + 1 < asm_src_len && (asm_src[asm_pos + 1] & 0xFF) == ';') {
            while (asm_pos < asm_src_len && (asm_src[asm_pos] & 0xFF) != '\n') asm_pos++;
        } else {
            break;
        }
    }
}

void asm_next(void) {
    int c;
    int start;
    int len;
    int is_float;
    int p;
    int sign;
    asm_skip_ws();
    if (asm_pos >= asm_src_len) { asm_tok = WTOK_EOF; return; }
    c = asm_src[asm_pos] & 0xFF;

    if (c == '(') { asm_tok = WTOK_LPAREN; asm_pos++; return; }
    if (c == ')') { asm_tok = WTOK_RPAREN; asm_pos++; return; }

    /* String literal */
    if (c == '"') {
        asm_pos++;
        start = asm_pos;
        while (asm_pos < asm_src_len && (asm_src[asm_pos] & 0xFF) != '"') {
            if ((asm_src[asm_pos] & 0xFF) == '\\') asm_pos++;
            asm_pos++;
        }
        len = asm_pos - start;
        asm_tok_str = strdupn(asm_src + start, len);
        asm_tok = WTOK_STRING;
        if (asm_pos < asm_src_len) asm_pos++;
        return;
    }

    /* $name */
    if (c == '$') {
        asm_pos++;
        start = asm_pos;
        while (asm_pos < asm_src_len) {
            c = asm_src[asm_pos] & 0xFF;
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
                c == '(' || c == ')') break;
            asm_pos++;
        }
        len = asm_pos - start;
        asm_tok_str = strdupn(asm_src + start, len);
        asm_tok = WTOK_NAME;
        return;
    }

    /* Number: starts with digit or '-' followed by digit */
    {
    int is_number;
    is_number = 0;
    if (c >= '0' && c <= '9') {
        is_number = 1;
    } else if (c == '-' && asm_pos + 1 < asm_src_len) {
        int nc;
        nc = asm_src[asm_pos + 1] & 0xFF;
        if (nc >= '0' && nc <= '9') {
            is_number = 1;
        }
    }

    if (is_number) {
        /* Number parsing */
        start = asm_pos;
        is_float = 0;
        if ((asm_src[asm_pos] & 0xFF) == '-') asm_pos++;

        /* Check for hex 0x */
        if (asm_pos < asm_src_len && (asm_src[asm_pos] & 0xFF) == '0' &&
            asm_pos + 1 < asm_src_len && (asm_src[asm_pos + 1] & 0xFF) == 'x') {
            asm_pos = asm_pos + 2;
            asm_tok_int = 0;
            while (asm_pos < asm_src_len) {
                c = asm_src[asm_pos] & 0xFF;
                if (c >= '0' && c <= '9') { asm_tok_int = asm_tok_int * 16 + (c - '0'); asm_pos++; }
                else if (c >= 'a' && c <= 'f') { asm_tok_int = asm_tok_int * 16 + 10 + (c - 'a'); asm_pos++; }
                else if (c >= 'A' && c <= 'F') { asm_tok_int = asm_tok_int * 16 + 10 + (c - 'A'); asm_pos++; }
                else break;
            }
            if ((asm_src[start] & 0xFF) == '-') asm_tok_int = -asm_tok_int;
            asm_tok = WTOK_INT;
            return;
        }

        /* Decimal number */
        while (asm_pos < asm_src_len && (asm_src[asm_pos] & 0xFF) >= '0' && (asm_src[asm_pos] & 0xFF) <= '9') asm_pos++;
        if (asm_pos < asm_src_len && (asm_src[asm_pos] & 0xFF) == '.') {
            is_float = 1;
            asm_pos++;
            while (asm_pos < asm_src_len && (asm_src[asm_pos] & 0xFF) >= '0' && (asm_src[asm_pos] & 0xFF) <= '9') asm_pos++;
        }
        if (asm_pos < asm_src_len && ((asm_src[asm_pos] & 0xFF) == 'e' || (asm_src[asm_pos] & 0xFF) == 'E')) {
            is_float = 1;
            asm_pos++;
            if (asm_pos < asm_src_len && ((asm_src[asm_pos] & 0xFF) == '+' || (asm_src[asm_pos] & 0xFF) == '-')) asm_pos++;
            while (asm_pos < asm_src_len && (asm_src[asm_pos] & 0xFF) >= '0' && (asm_src[asm_pos] & 0xFF) <= '9') asm_pos++;
        }
        len = asm_pos - start;
        if (is_float) {
            asm_tok_str = strdupn(asm_src + start, len);
            asm_tok = WTOK_FLOAT;
        } else {
            asm_tok_int = 0;
            p = start;
            sign = 1;
            if ((asm_src[p] & 0xFF) == '-') { sign = -1; p++; }
            while (p < asm_pos) {
                asm_tok_int = asm_tok_int * 10 + ((asm_src[p] & 0xFF) - '0');
                p++;
            }
            asm_tok_int = asm_tok_int * sign;
            asm_tok = WTOK_INT;
        }
        return;
    }
    }

    /* Keyword */
    start = asm_pos;
    while (asm_pos < asm_src_len) {
        c = asm_src[asm_pos] & 0xFF;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
            c == '(' || c == ')') break;
        asm_pos++;
    }
    len = asm_pos - start;
    asm_tok_str = strdupn(asm_src + start, len);
    asm_tok = WTOK_KW;
}

/* ================================================================
 * Module State
 * ================================================================ */

struct AsmType {
    int nparams;
    int *param_types;
    int nresults;
    int result_type;
};

struct AsmFunc {
    char *name;
    int type_idx;
    int is_import;
};

struct AsmImport {
    char *module_name;
    char *field_name;
    int func_idx;
};

struct AsmGlobal {
    char *name;
    int val_type;
    int is_mutable;
    struct ByteVec *init_expr;
};

struct AsmData {
    int offset;
    struct ByteVec *bytes;
};

struct AsmType **asm_types;
int asm_ntypes;

char **asm_type_names;
int *asm_type_indices;
int asm_ntype_names;

struct AsmFunc **asm_funcs;
int asm_nfuncs;
int asm_nimports;

struct AsmImport **asm_imports;

struct AsmGlobal **asm_globals;
int asm_nglobals;

struct AsmData **asm_data_segs;
int asm_ndata;

int asm_memory_pages;
char *asm_mem_export_name;

int asm_has_table;
int asm_table_size;
int *asm_elem_indices;
int asm_nelem;

/* Export tracking */
char *asm_func_export_name;
int asm_func_export_idx;

/* Per-function local tracking */
char **asm_local_names;
int *asm_local_types;
int asm_nlocals;

/* Label stack for block/loop/if */
char **asm_label_stack;
int asm_label_sp;

/* Function bodies for deferred encoding */
struct ByteVec **asm_func_bodies;

/* ================================================================
 * Name Resolution
 * ================================================================ */

int asm_find_func(char *name) {
    int i;
    for (i = 0; i < asm_nfuncs; i++) {
        if (strcmp(asm_funcs[i]->name, name) == 0) return i;
    }
    printf("assembler error: unknown function $%s\n", name);
    exit(1);
    return 0;
}

int asm_find_global(char *name) {
    int i;
    for (i = 0; i < asm_nglobals; i++) {
        if (strcmp(asm_globals[i]->name, name) == 0) return i;
    }
    printf("assembler error: unknown global $%s\n", name);
    exit(1);
    return 0;
}

int asm_find_type_by_name(char *name) {
    int i;
    for (i = 0; i < asm_ntype_names; i++) {
        if (strcmp(asm_type_names[i], name) == 0) return asm_type_indices[i];
    }
    printf("assembler error: unknown type $%s\n", name);
    exit(1);
    return 0;
}

int asm_find_local(char *name) {
    int i;
    for (i = 0; i < asm_nlocals; i++) {
        if (strcmp(asm_local_names[i], name) == 0) return i;
    }
    printf("assembler error: unknown local $%s\n", name);
    exit(1);
    return 0;
}

int asm_find_label(char *name) {
    int i;
    for (i = asm_label_sp - 1; i >= 0; i--) {
        if (asm_label_stack[i] != (char *)0 && strcmp(asm_label_stack[i], name) == 0) {
            return asm_label_sp - 1 - i;
        }
    }
    printf("assembler error: unknown label $%s\n", name);
    exit(1);
    return 0;
}

void asm_push_label(char *name) {
    if (asm_label_sp >= ASM_MAX_LABELS) asm_error("label stack overflow");
    asm_label_stack[asm_label_sp] = name;
    asm_label_sp++;
}

void asm_pop_label(void) {
    if (asm_label_sp > 0) asm_label_sp--;
}

/* ================================================================
 * Type Deduplication
 * ================================================================ */

int asm_find_or_add_type(int nparams, int *ptypes, int nresults, int rtype) {
    int i;
    int j;
    int match;
    struct AsmType *t;
    for (i = 0; i < asm_ntypes; i++) {
        if (asm_types[i]->nparams != nparams) continue;
        if (asm_types[i]->nresults != nresults) continue;
        if (nresults > 0 && asm_types[i]->result_type != rtype) continue;
        match = 1;
        for (j = 0; j < nparams; j++) {
            if (asm_types[i]->param_types[j] != ptypes[j]) { match = 0; break; }
        }
        if (match) return i;
    }
    if (asm_ntypes >= ASM_MAX_TYPES) asm_error("too many types");
    t = (struct AsmType *)malloc(sizeof(struct AsmType));
    t->nparams = nparams;
    t->param_types = (int *)malloc(ASM_MAX_PARAMS * sizeof(int));
    for (j = 0; j < nparams && j < ASM_MAX_PARAMS; j++) {
        t->param_types[j] = ptypes[j];
    }
    t->nresults = nresults;
    t->result_type = rtype;
    asm_types[asm_ntypes] = t;
    asm_ntypes++;
    return asm_ntypes - 1;
}

int asm_parse_valtype(char *s) {
    if (strcmp(s, "i32") == 0) return 0x7F;
    if (strcmp(s, "i64") == 0) return 0x7E;
    if (strcmp(s, "f64") == 0) return 0x7C;
    if (strcmp(s, "f32") == 0) return 0x7D;
    printf("assembler error: unknown valtype %s\n", s);
    exit(1);
    return 0x7F;
}

/* Parse (param ...) and (result ...) to build a type signature.
 * Returns type index. Leaves tokenizer past the closing ) of func sig. */
int asm_parse_func_sig(int *out_nparams) {
    int ptypes[16];
    int nparams;
    int nresults;
    int rtype;

    nparams = 0;
    nresults = 0;
    rtype = 0;

    /* Parse param and result declarations until we see something else */
    while (asm_tok == WTOK_LPAREN) {
        asm_save();
        asm_next(); /* keyword after ( */
        if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "param") == 0) {
            asm_next();
            /* Skip optional $name */
            if (asm_tok == WTOK_NAME) asm_next();
            /* Parse one or more types */
            while (asm_tok == WTOK_KW && (strcmp(asm_tok_str, "i32") == 0 ||
                   strcmp(asm_tok_str, "i64") == 0 || strcmp(asm_tok_str, "f64") == 0 ||
                   strcmp(asm_tok_str, "f32") == 0)) {
                if (nparams < ASM_MAX_PARAMS) {
                    ptypes[nparams] = asm_parse_valtype(asm_tok_str);
                    nparams++;
                }
                asm_next();
            }
            /* expect ) */
            if (asm_tok == WTOK_RPAREN) asm_next();
        } else if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "result") == 0) {
            asm_next();
            if (asm_tok == WTOK_KW) {
                rtype = asm_parse_valtype(asm_tok_str);
                nresults = 1;
                asm_next();
            }
            if (asm_tok == WTOK_RPAREN) asm_next();
        } else {
            asm_restore();
            break;
        }
    }

    if (out_nparams != (int *)0) *out_nparams = nparams;
    return asm_find_or_add_type(nparams, ptypes, nresults, rtype);
}

/* ================================================================
 * Float Parsing (IEEE 754)
 * ================================================================ */

double asm_parse_float_text(char *s) {
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
    if (sign < 0) result = result * -1.0;
    return result;
}

void asm_emit_f64(struct ByteVec *code, char *s) {
    double *p;
    char *b;
    int i;
    p = (double *)malloc(8);
    *p = asm_parse_float_text(s);
    b = (char *)p;
    for (i = 0; i < 8; i++) {
        bv_push(code, b[i] & 0xFF);
    }
}

/* ================================================================
 * String Decoding (WAT escape sequences)
 * ================================================================ */

int asm_hex_digit(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return 0;
}

void asm_decode_string(char *s, int slen, struct ByteVec *dst) {
    int i;
    int c;
    i = 0;
    while (i < slen) {
        c = s[i] & 0xFF;
        if (c == '\\' && i + 2 < slen) {
            int c1;
            int c2;
            c1 = s[i + 1] & 0xFF;
            c2 = s[i + 2] & 0xFF;
            if (c1 == '\\') {
                bv_push(dst, '\\');
                i = i + 2;
            } else if (c1 == '"') {
                bv_push(dst, '"');
                i = i + 2;
            } else if (c1 == 'n') {
                bv_push(dst, 10);
                i = i + 2;
            } else if (c1 == 't') {
                bv_push(dst, 9);
                i = i + 2;
            } else if (c1 == 'r') {
                bv_push(dst, 13);
                i = i + 2;
            } else {
                /* hex escape \HH */
                bv_push(dst, asm_hex_digit(c1) * 16 + asm_hex_digit(c2));
                i = i + 3;
            }
        } else {
            bv_push(dst, c);
            i = i + 1;
        }
    }
}

/* ================================================================
 * First Pass — Collect Function Names
 * ================================================================ */

void asm_first_pass(void) {
    int depth;
    depth = 0;

    while (1) {
        asm_next();
        if (asm_tok == WTOK_EOF) break;
        if (asm_tok == WTOK_LPAREN) {
            depth++;
            continue;
        }
        if (asm_tok == WTOK_RPAREN) {
            depth--;
            continue;
        }
        if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "func") == 0) {
            asm_next();
            if (asm_tok == WTOK_NAME) {
                int is_imp;
                is_imp = (depth >= 3) ? 1 : 0;
                if (asm_nfuncs >= ASM_MAX_FUNCS) asm_error("too many functions");
                asm_funcs[asm_nfuncs] = (struct AsmFunc *)malloc(sizeof(struct AsmFunc));
                asm_funcs[asm_nfuncs]->name = asm_tok_str;
                asm_funcs[asm_nfuncs]->type_idx = -1;
                asm_funcs[asm_nfuncs]->is_import = is_imp;
                asm_nfuncs++;
                if (is_imp) asm_nimports++;
            }
        }
    }
}

/* ================================================================
 * Instruction Encoding
 * ================================================================ */

int asm_mem_align(char *instr) {
    if (strcmp(instr, "i32.load") == 0) return 2;
    if (strcmp(instr, "i32.store") == 0) return 2;
    if (strcmp(instr, "i32.load8_u") == 0) return 0;
    if (strcmp(instr, "i32.store8") == 0) return 0;
    if (strcmp(instr, "i32.load16_s") == 0) return 1;
    if (strcmp(instr, "i32.store16") == 0) return 1;
    if (strcmp(instr, "f64.load") == 0) return 3;
    if (strcmp(instr, "f64.store") == 0) return 3;
    return 0;
}

/* Parse offset=N from a keyword token like "offset=4" */
int asm_parse_offset_val(char *s) {
    int i;
    int val;
    /* skip past "offset=" */
    i = 7;
    val = 0;
    while (s[i] >= '0' && s[i] <= '9') {
        val = val * 10 + (s[i] - '0');
        i++;
    }
    return val;
}

/* Check if current token is offset=N, consume it and set asm_mem_offset */
void asm_check_offset(void) {
    if (asm_tok == WTOK_KW && asm_tok_str[0] == 'o' &&
        strncmp(asm_tok_str, "offset=", 7) == 0) {
        asm_mem_offset = asm_parse_offset_val(asm_tok_str);
        asm_next();
    }
}

void asm_encode_mem_instr(struct ByteVec *code, int opcode, char *instr) {
    bv_push(code, opcode);
    bv_u32(code, asm_mem_align(instr));
    bv_u32(code, asm_mem_offset);
    asm_mem_offset = 0;
}

void asm_encode_instr(char *kw, struct ByteVec *code) {
    /* Constants */
    if (strcmp(kw, "i32.const") == 0) {
        bv_push(code, 0x41);
        if (asm_tok == WTOK_INT) { bv_i32(code, asm_tok_int); asm_next(); }
        else asm_error("expected integer after i32.const");
        return;
    }
    if (strcmp(kw, "i64.const") == 0) {
        bv_push(code, 0x42);
        if (asm_tok == WTOK_INT) { bv_i32(code, asm_tok_int); asm_next(); }
        else asm_error("expected integer after i64.const");
        return;
    }
    if (strcmp(kw, "f64.const") == 0) {
        bv_push(code, 0x44);
        if (asm_tok == WTOK_FLOAT) { asm_emit_f64(code, asm_tok_str); asm_next(); }
        else if (asm_tok == WTOK_INT) {
            /* Integer literal used as float, e.g. f64.const 0 */
            char buf[20];
            int v;
            int bi;
            v = asm_tok_int;
            bi = 0;
            if (v < 0) { buf[bi] = '-'; bi++; v = -v; }
            if (v == 0) { buf[bi] = '0'; bi++; }
            else {
                char tmp[12];
                int ti;
                ti = 0;
                while (v > 0) { tmp[ti] = '0' + (v % 10); ti++; v = v / 10; }
                while (ti > 0) { ti--; buf[bi] = tmp[ti]; bi++; }
            }
            buf[bi] = '.';
            bi++;
            buf[bi] = '0';
            bi++;
            buf[bi] = 0;
            asm_emit_f64(code, buf);
            asm_next();
        }
        else asm_error("expected number after f64.const");
        return;
    }

    /* Local/global variable access */
    if (strcmp(kw, "local.get") == 0) {
        bv_push(code, 0x20);
        if (asm_tok == WTOK_NAME) { bv_u32(code, asm_find_local(asm_tok_str)); asm_next(); }
        else if (asm_tok == WTOK_INT) { bv_u32(code, asm_tok_int); asm_next(); }
        else asm_error("expected local name/index");
        return;
    }
    if (strcmp(kw, "local.set") == 0) {
        bv_push(code, 0x21);
        if (asm_tok == WTOK_NAME) { bv_u32(code, asm_find_local(asm_tok_str)); asm_next(); }
        else if (asm_tok == WTOK_INT) { bv_u32(code, asm_tok_int); asm_next(); }
        else asm_error("expected local name/index");
        return;
    }
    if (strcmp(kw, "local.tee") == 0) {
        bv_push(code, 0x22);
        if (asm_tok == WTOK_NAME) { bv_u32(code, asm_find_local(asm_tok_str)); asm_next(); }
        else if (asm_tok == WTOK_INT) { bv_u32(code, asm_tok_int); asm_next(); }
        else asm_error("expected local name/index");
        return;
    }
    if (strcmp(kw, "global.get") == 0) {
        bv_push(code, 0x23);
        if (asm_tok == WTOK_NAME) { bv_u32(code, asm_find_global(asm_tok_str)); asm_next(); }
        else if (asm_tok == WTOK_INT) { bv_u32(code, asm_tok_int); asm_next(); }
        else asm_error("expected global name/index");
        return;
    }
    if (strcmp(kw, "global.set") == 0) {
        bv_push(code, 0x24);
        if (asm_tok == WTOK_NAME) { bv_u32(code, asm_find_global(asm_tok_str)); asm_next(); }
        else if (asm_tok == WTOK_INT) { bv_u32(code, asm_tok_int); asm_next(); }
        else asm_error("expected global name/index");
        return;
    }

    /* Call */
    if (strcmp(kw, "call") == 0) {
        bv_push(code, 0x10);
        if (asm_tok == WTOK_NAME) { bv_u32(code, asm_find_func(asm_tok_str)); asm_next(); }
        else if (asm_tok == WTOK_INT) { bv_u32(code, asm_tok_int); asm_next(); }
        else asm_error("expected function name/index");
        return;
    }
    if (strcmp(kw, "call_indirect") == 0) {
        int type_idx;
        /* Expect (type $name) */
        if (asm_tok != WTOK_LPAREN) asm_error("expected (type ...) after call_indirect");
        asm_next(); /* "type" keyword */
        asm_next(); /* $name */
        type_idx = asm_tok == WTOK_NAME ? asm_find_type_by_name(asm_tok_str) : asm_tok_int;
        asm_next(); /* ) */
        if (asm_tok == WTOK_RPAREN) asm_next();
        bv_push(code, 0x11);
        bv_u32(code, type_idx);
        bv_u32(code, 0); /* table index 0 */
        return;
    }

    /* Branch */
    if (strcmp(kw, "br") == 0) {
        bv_push(code, 0x0C);
        if (asm_tok == WTOK_NAME) { bv_u32(code, asm_find_label(asm_tok_str)); asm_next(); }
        else if (asm_tok == WTOK_INT) { bv_u32(code, asm_tok_int); asm_next(); }
        else asm_error("expected label after br");
        return;
    }
    if (strcmp(kw, "br_if") == 0) {
        bv_push(code, 0x0D);
        if (asm_tok == WTOK_NAME) { bv_u32(code, asm_find_label(asm_tok_str)); asm_next(); }
        else if (asm_tok == WTOK_INT) { bv_u32(code, asm_tok_int); asm_next(); }
        else asm_error("expected label after br_if");
        return;
    }

    /* Memory instructions */
    if (strcmp(kw, "i32.load") == 0) { asm_encode_mem_instr(code, 0x28, kw); return; }
    if (strcmp(kw, "f64.load") == 0) { asm_encode_mem_instr(code, 0x2B, kw); return; }
    if (strcmp(kw, "i32.load8_u") == 0) { asm_encode_mem_instr(code, 0x2D, kw); return; }
    if (strcmp(kw, "i32.load16_s") == 0) { asm_encode_mem_instr(code, 0x2E, kw); return; }
    if (strcmp(kw, "i32.store") == 0) { asm_encode_mem_instr(code, 0x36, kw); return; }
    if (strcmp(kw, "f64.store") == 0) { asm_encode_mem_instr(code, 0x39, kw); return; }
    if (strcmp(kw, "i32.store8") == 0) { asm_encode_mem_instr(code, 0x3A, kw); return; }
    if (strcmp(kw, "i32.store16") == 0) { asm_encode_mem_instr(code, 0x3B, kw); return; }

    /* Simple opcodes (no immediate) */
    if (strcmp(kw, "drop") == 0) { bv_push(code, 0x1A); return; }
    if (strcmp(kw, "return") == 0) { bv_push(code, 0x0F); return; }
    if (strcmp(kw, "i32.eqz") == 0) { bv_push(code, 0x45); return; }
    if (strcmp(kw, "i32.eq") == 0) { bv_push(code, 0x46); return; }
    if (strcmp(kw, "i32.ne") == 0) { bv_push(code, 0x47); return; }
    if (strcmp(kw, "i32.lt_s") == 0) { bv_push(code, 0x48); return; }
    if (strcmp(kw, "i32.lt_u") == 0) { bv_push(code, 0x49); return; }
    if (strcmp(kw, "i32.gt_s") == 0) { bv_push(code, 0x4A); return; }
    if (strcmp(kw, "i32.gt_u") == 0) { bv_push(code, 0x4B); return; }
    if (strcmp(kw, "i32.le_s") == 0) { bv_push(code, 0x4C); return; }
    if (strcmp(kw, "i32.le_u") == 0) { bv_push(code, 0x4D); return; }
    if (strcmp(kw, "i32.ge_s") == 0) { bv_push(code, 0x4E); return; }
    if (strcmp(kw, "i32.ge_u") == 0) { bv_push(code, 0x4F); return; }
    if (strcmp(kw, "i32.add") == 0) { bv_push(code, 0x6A); return; }
    if (strcmp(kw, "i32.sub") == 0) { bv_push(code, 0x6B); return; }
    if (strcmp(kw, "i32.mul") == 0) { bv_push(code, 0x6C); return; }
    if (strcmp(kw, "i32.div_s") == 0) { bv_push(code, 0x6D); return; }
    if (strcmp(kw, "i32.div_u") == 0) { bv_push(code, 0x6E); return; }
    if (strcmp(kw, "i32.rem_s") == 0) { bv_push(code, 0x6F); return; }
    if (strcmp(kw, "i32.rem_u") == 0) { bv_push(code, 0x70); return; }
    if (strcmp(kw, "i32.and") == 0) { bv_push(code, 0x71); return; }
    if (strcmp(kw, "i32.or") == 0) { bv_push(code, 0x72); return; }
    if (strcmp(kw, "i32.xor") == 0) { bv_push(code, 0x73); return; }
    if (strcmp(kw, "i32.shl") == 0) { bv_push(code, 0x74); return; }
    if (strcmp(kw, "i32.shr_s") == 0) { bv_push(code, 0x75); return; }
    if (strcmp(kw, "i32.shr_u") == 0) { bv_push(code, 0x76); return; }
    if (strcmp(kw, "f64.eq") == 0) { bv_push(code, 0x61); return; }
    if (strcmp(kw, "f64.ne") == 0) { bv_push(code, 0x62); return; }
    if (strcmp(kw, "f64.lt") == 0) { bv_push(code, 0x63); return; }
    if (strcmp(kw, "f64.gt") == 0) { bv_push(code, 0x64); return; }
    if (strcmp(kw, "f64.le") == 0) { bv_push(code, 0x65); return; }
    if (strcmp(kw, "f64.ge") == 0) { bv_push(code, 0x66); return; }
    if (strcmp(kw, "f64.neg") == 0) { bv_push(code, 0x9A); return; }
    if (strcmp(kw, "f64.add") == 0) { bv_push(code, 0xA0); return; }
    if (strcmp(kw, "f64.sub") == 0) { bv_push(code, 0xA1); return; }
    if (strcmp(kw, "f64.mul") == 0) { bv_push(code, 0xA2); return; }
    if (strcmp(kw, "f64.div") == 0) { bv_push(code, 0xA3); return; }
    if (strcmp(kw, "i32.trunc_f64_s") == 0) { bv_push(code, 0xAA); return; }
    if (strcmp(kw, "f64.convert_i32_s") == 0) { bv_push(code, 0xB7); return; }

    printf("assembler error: unknown instruction '%s'\n", kw);
    exit(1);
}

/* ================================================================
 * Expression Parser (recursive, handles folded + flat WAT)
 * ================================================================ */

void asm_parse_expr(struct ByteVec *code);

/* Parse a block type: (result valtype) or nothing (void 0x40) */
int asm_parse_blocktype(void) {
    if (asm_tok == WTOK_LPAREN) {
        asm_save();
        asm_next();
        if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "result") == 0) {
            int bt;
            asm_next();
            bt = asm_parse_valtype(asm_tok_str);
            asm_next(); /* skip type */
            if (asm_tok == WTOK_RPAREN) asm_next();
            return bt;
        }
        asm_restore();
    }
    return 0x40; /* void block type */
}

/* Check if current ( starts a then/else clause */
int asm_peek_is_then_or_else(void) {
    int result;
    if (asm_tok != WTOK_LPAREN) return 0;
    asm_save();
    asm_next();
    result = (asm_tok == WTOK_KW &&
              (strcmp(asm_tok_str, "then") == 0 || strcmp(asm_tok_str, "else") == 0));
    asm_restore();
    return result;
}

void asm_parse_folded_block(struct ByteVec *code) {
    /* Current token is the keyword: block/loop */
    char *kw;
    int opcode;
    int bt;
    char *label;
    kw = asm_tok_str;
    opcode = (strcmp(kw, "block") == 0) ? 0x02 : 0x03;
    asm_next();

    /* Optional label */
    label = (char *)0;
    if (asm_tok == WTOK_NAME) {
        label = asm_tok_str;
        asm_next();
    }

    bt = asm_parse_blocktype();
    bv_push(code, opcode);
    bv_push(code, bt);
    asm_push_label(label);

    /* Parse body until ) */
    while (asm_tok != WTOK_RPAREN && asm_tok != WTOK_EOF) {
        asm_parse_expr(code);
    }

    bv_push(code, 0x0B); /* end */
    asm_pop_label();
    if (asm_tok == WTOK_RPAREN) asm_next();
}

void asm_parse_folded_if(struct ByteVec *code) {
    /* Current token is "if" keyword */
    int bt;
    char *label;
    asm_next(); /* past "if" */

    /* Optional label for if (rare but possible) */
    label = (char *)0;
    if (asm_tok == WTOK_NAME) {
        label = asm_tok_str;
        asm_next();
    }

    /* Parse block type */
    bt = asm_parse_blocktype();

    /* Parse condition expressions (everything before (then ...)) */
    while (asm_tok != WTOK_EOF && asm_tok != WTOK_RPAREN) {
        if (asm_peek_is_then_or_else()) break;
        asm_parse_expr(code);
    }

    /* Emit if instruction */
    bv_push(code, 0x04);
    bv_push(code, bt);
    asm_push_label(label);

    /* Parse (then ...) */
    if (asm_tok == WTOK_LPAREN) {
        asm_next(); /* ( */
        if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "then") == 0) {
            asm_next(); /* skip "then" */
            while (asm_tok != WTOK_RPAREN && asm_tok != WTOK_EOF) {
                asm_parse_expr(code);
            }
            if (asm_tok == WTOK_RPAREN) asm_next(); /* close then */
        }
    }

    /* Parse optional (else ...) */
    if (asm_tok == WTOK_LPAREN) {
        asm_save();
        asm_next();
        if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "else") == 0) {
            asm_next(); /* skip "else" */
            bv_push(code, 0x05); /* else opcode */
            while (asm_tok != WTOK_RPAREN && asm_tok != WTOK_EOF) {
                asm_parse_expr(code);
            }
            if (asm_tok == WTOK_RPAREN) asm_next(); /* close else */
        } else {
            asm_restore();
        }
    }

    bv_push(code, 0x0B); /* end */
    asm_pop_label();
    if (asm_tok == WTOK_RPAREN) asm_next(); /* close if */
}

void asm_parse_expr(struct ByteVec *code) {
    char *kw;

    if (asm_tok == WTOK_LPAREN) {
        /* Folded expression */
        asm_next(); /* keyword */
        if (asm_tok != WTOK_KW) {
            /* Unexpected — skip to matching ) */
            int depth;
            depth = 1;
            while (depth > 0 && asm_tok != WTOK_EOF) {
                asm_next();
                if (asm_tok == WTOK_LPAREN) depth++;
                if (asm_tok == WTOK_RPAREN) depth--;
            }
            if (asm_tok == WTOK_RPAREN) asm_next();
            return;
        }

        kw = asm_tok_str;

        /* Control flow: block, loop, if */
        if (strcmp(kw, "block") == 0 || strcmp(kw, "loop") == 0) {
            asm_parse_folded_block(code);
            return;
        }
        if (strcmp(kw, "if") == 0) {
            asm_parse_folded_if(code);
            return;
        }

        /* Regular folded instruction: (instr [immediate] [sub-exprs] ) */
        asm_next(); /* move past keyword to first arg */

        /* Check for offset=N on memory instructions */
        asm_check_offset();

        /* Consume any immediate (before sub-expressions) */
        /* For instructions like (call $fn ...) or (local.get $x), the immediate
         * comes right after the keyword. For (i32.const 42), same thing.
         * For call_indirect, the (type $t) is an immediate group. */
        if (strcmp(kw, "call_indirect") == 0) {
            /* Handle (call_indirect (type $name) sub-exprs...) */
            /* First parse sub-expressions (operands on stack) */
            /* But call_indirect is special: (type $name) is an immediate,
             * and the table index is on the stack before call_indirect */
            /* Process sub-exprs first, then encode */
            /* Actually in folded form, sub-exprs come after the immediate */
            /* Let me handle this properly */
            int type_idx;
            type_idx = -1;
            if (asm_tok == WTOK_LPAREN) {
                asm_save();
                asm_next();
                if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "type") == 0) {
                    asm_next(); /* $name */
                    if (asm_tok == WTOK_NAME) type_idx = asm_find_type_by_name(asm_tok_str);
                    else type_idx = asm_tok_int;
                    asm_next(); /* ) */
                    if (asm_tok == WTOK_RPAREN) asm_next();
                } else {
                    asm_restore();
                }
            }
            /* Parse operand sub-expressions */
            while (asm_tok == WTOK_LPAREN) {
                asm_parse_expr(code);
            }
            bv_push(code, 0x11);
            bv_u32(code, type_idx);
            bv_u32(code, 0);
            if (asm_tok == WTOK_RPAREN) asm_next();
            return;
        }

        /* For instructions with named/int immediates, collect them */
        /* But also need to handle sub-expressions that come after */
        {
            char *saved_kw;
            int has_name_imm;
            char *name_imm;
            int has_int_imm;
            int int_imm;
            int has_float_imm;
            char *float_imm;
            int saved_mem_offset;

            saved_kw = kw;
            has_name_imm = 0;
            name_imm = (char *)0;
            has_int_imm = 0;
            int_imm = 0;
            has_float_imm = 0;
            float_imm = (char *)0;
            saved_mem_offset = asm_mem_offset;

            /* Check if there's an immediate before sub-expressions */
            if (asm_tok == WTOK_NAME) {
                has_name_imm = 1;
                name_imm = asm_tok_str;
                asm_next();
            } else if (asm_tok == WTOK_INT && strcmp(saved_kw, "i32.const") != 0 &&
                       strcmp(saved_kw, "i64.const") != 0 && strcmp(saved_kw, "f64.const") != 0) {
                /* Some instructions take int immediates */
            } else if (asm_tok == WTOK_INT || asm_tok == WTOK_FLOAT) {
                /* Constant instructions */
                if (asm_tok == WTOK_INT) { has_int_imm = 1; int_imm = asm_tok_int; }
                else { has_float_imm = 1; float_imm = asm_tok_str; }
                asm_next();
            }

            /* Parse sub-expressions (operands) */
            while (asm_tok == WTOK_LPAREN) {
                asm_parse_expr(code);
            }

            /* Restore mem offset (sub-expressions may have overwritten it) */
            asm_mem_offset = saved_mem_offset;

            /* Encode instruction with saved immediates.
             * asm_encode_instr reads from asm_tok and may call asm_next(),
             * so save/restore the full tokenizer state around the call. */
            {
                int enc_save_pos;
                int enc_save_tok;
                char *enc_save_str;
                int enc_save_int;

                enc_save_pos = asm_pos;
                enc_save_tok = asm_tok;
                enc_save_str = asm_tok_str;
                enc_save_int = asm_tok_int;

                if (has_name_imm) {
                    asm_tok = WTOK_NAME;
                    asm_tok_str = name_imm;
                } else if (has_int_imm) {
                    asm_tok = WTOK_INT;
                    asm_tok_int = int_imm;
                } else if (has_float_imm) {
                    asm_tok = WTOK_FLOAT;
                    asm_tok_str = float_imm;
                }

                asm_encode_instr(saved_kw, code);

                asm_pos = enc_save_pos;
                asm_tok = enc_save_tok;
                asm_tok_str = enc_save_str;
                asm_tok_int = enc_save_int;
            }

            if (asm_tok == WTOK_RPAREN) asm_next();
        }
        return;
    }

    /* Flat instruction */
    if (asm_tok == WTOK_KW) {
        kw = asm_tok_str;

        /* Flat control flow */
        if (strcmp(kw, "block") == 0 || strcmp(kw, "loop") == 0) {
            int opcode;
            int bt;
            char *label;
            opcode = (strcmp(kw, "block") == 0) ? 0x02 : 0x03;
            asm_next();
            label = (char *)0;
            if (asm_tok == WTOK_NAME) { label = asm_tok_str; asm_next(); }
            bt = asm_parse_blocktype();
            bv_push(code, opcode);
            bv_push(code, bt);
            asm_push_label(label);
            return;
        }
        if (strcmp(kw, "if") == 0) {
            int bt;
            asm_next();
            bt = asm_parse_blocktype();
            bv_push(code, 0x04);
            bv_push(code, bt);
            asm_push_label((char *)0);
            return;
        }
        if (strcmp(kw, "else") == 0) {
            bv_push(code, 0x05);
            asm_next();
            return;
        }
        if (strcmp(kw, "end") == 0) {
            bv_push(code, 0x0B);
            asm_pop_label();
            asm_next();
            return;
        }
        if (strcmp(kw, "then") == 0) {
            /* "then" in flat form is a no-op */
            asm_next();
            return;
        }

        /* Regular flat instruction */
        asm_next(); /* advance past keyword to potential immediate */
        asm_check_offset();
        asm_encode_instr(kw, code);
        return;
    }

    /* Skip anything else (shouldn't happen) */
    asm_next();
}

/* ================================================================
 * Function Body Parser
 * ================================================================ */

void asm_parse_func_body(int func_idx, struct ByteVec *body) {
    struct ByteVec *code;
    int nparams;
    int nlocals_only;
    int i;
    int group_type;
    int group_count;
    int ngroups;
    struct ByteVec *locals_buf;
    (void)func_idx;

    code = bv_new(4096);
    locals_buf = bv_new(256);

    /* Parse params and locals to build local name table */
    asm_nlocals = 0;
    nparams = 0;

    /* Parse (param ...) declarations */
    while (asm_tok == WTOK_LPAREN) {
        asm_save();
        asm_next();
        if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "param") == 0) {
            asm_next();
            if (asm_tok == WTOK_NAME) {
                /* Named param: (param $name type) */
                char *pname;
                int ptype;
                pname = asm_tok_str;
                asm_next();
                ptype = asm_parse_valtype(asm_tok_str);
                asm_next();
                if (asm_nlocals < ASM_MAX_LOCALS) {
                    asm_local_names[asm_nlocals] = pname;
                    asm_local_types[asm_nlocals] = ptype;
                    asm_nlocals++;
                    nparams++;
                }
            } else {
                /* Unnamed params: (param i32 i32 ...) */
                while (asm_tok == WTOK_KW && (strcmp(asm_tok_str, "i32") == 0 ||
                       strcmp(asm_tok_str, "i64") == 0 || strcmp(asm_tok_str, "f64") == 0)) {
                    if (asm_nlocals < ASM_MAX_LOCALS) {
                        asm_local_names[asm_nlocals] = (char *)0;
                        asm_local_types[asm_nlocals] = asm_parse_valtype(asm_tok_str);
                        asm_nlocals++;
                        nparams++;
                    }
                    asm_next();
                }
            }
            if (asm_tok == WTOK_RPAREN) asm_next();
        } else {
            asm_restore();
            break;
        }
    }

    /* Parse (result ...) — skip it, already handled in type */
    if (asm_tok == WTOK_LPAREN) {
        asm_save();
        asm_next();
        if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "result") == 0) {
            asm_next(); /* valtype */
            asm_next(); /* ) */
            if (asm_tok == WTOK_RPAREN) asm_next();
        } else {
            asm_restore();
        }
    }

    /* Parse (local ...) declarations */
    while (asm_tok == WTOK_LPAREN) {
        asm_save();
        asm_next();
        if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "local") == 0) {
            asm_next();
            if (asm_tok == WTOK_NAME) {
                /* Named local: (local $name type) */
                char *lname;
                int ltype;
                lname = asm_tok_str;
                asm_next();
                ltype = asm_parse_valtype(asm_tok_str);
                asm_next();
                if (asm_nlocals < ASM_MAX_LOCALS) {
                    asm_local_names[asm_nlocals] = lname;
                    asm_local_types[asm_nlocals] = ltype;
                    asm_nlocals++;
                }
            } else {
                /* Unnamed locals: (local i32 ...) */
                while (asm_tok == WTOK_KW) {
                    if (asm_nlocals < ASM_MAX_LOCALS) {
                        asm_local_names[asm_nlocals] = (char *)0;
                        asm_local_types[asm_nlocals] = asm_parse_valtype(asm_tok_str);
                        asm_nlocals++;
                    }
                    asm_next();
                }
            }
            if (asm_tok == WTOK_RPAREN) asm_next();
        } else {
            asm_restore();
            break;
        }
    }

    /* Encode local declarations (grouped by consecutive same type) */
    nlocals_only = asm_nlocals - nparams;
    if (nlocals_only > 0) {
        /* Count groups of consecutive same-type locals */
        ngroups = 0;
        i = nparams;
        while (i < asm_nlocals) {
            group_type = asm_local_types[i];
            group_count = 0;
            while (i < asm_nlocals && asm_local_types[i] == group_type) {
                group_count++;
                i++;
            }
            bv_u32(locals_buf, group_count);
            bv_push(locals_buf, group_type);
            ngroups++;
        }
        bv_u32(code, ngroups);
        bv_append(code, locals_buf);
    } else {
        bv_u32(code, 0);
    }

    /* Reset label stack */
    asm_label_sp = 0;

    /* Parse instruction body */
    {
        while (asm_tok != WTOK_RPAREN && asm_tok != WTOK_EOF) {
            asm_parse_expr(code);
        }
    }

    /* end opcode */
    bv_push(code, 0x0B);

    /* Write to body: size prefix + code */
    bv_u32(body, code->len);
    bv_append(body, code);
}

/* ================================================================
 * Second Pass — Parse and Encode Module
 * ================================================================ */

void asm_second_pass(void) {
    struct ByteVec *wasm;
    struct ByteVec *sec;
    struct ByteVec *code_sec;
    int nlocal_funcs;
    int i;
    int j;

    wasm = bv_new(65536);
    sec = bv_new(32768);
    code_sec = bv_new(32768);

    /* Initialize module state */
    asm_nglobals = 0;
    asm_ndata = 0;
    asm_ntypes = 0;
    asm_ntype_names = 0;
    asm_memory_pages = 256;
    asm_mem_export_name = (char *)0;
    asm_has_table = 0;
    asm_table_size = 0;
    asm_nelem = 0;
    asm_func_export_name = (char *)0;
    asm_func_export_idx = -1;

    /* Skip past (module */
    asm_next(); /* ( */
    asm_next(); /* module */
    asm_next(); /* first form */

    /* Process all top-level forms */
    while (asm_tok != WTOK_RPAREN && asm_tok != WTOK_EOF) {
        if (asm_tok == WTOK_LPAREN) {
            asm_next(); /* keyword */

            if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "import") == 0) {
                /* (import "module" "field" (func $name (param ...) (result ...))) */
                char *mod_name;
                char *field_name;
                int imp_idx;
                int type_idx;
                asm_next(); /* module string */
                mod_name = asm_tok_str;
                asm_next(); /* field string */
                field_name = asm_tok_str;
                asm_next(); /* ( */
                asm_next(); /* "func" */
                asm_next(); /* $name */
                /* Find the import index from first pass */
                imp_idx = asm_find_func(asm_tok_str);
                asm_next(); /* past name */
                /* Parse type signature */
                type_idx = asm_parse_func_sig((int *)0);
                asm_funcs[imp_idx]->type_idx = type_idx;
                /* Record import info */
                asm_imports[imp_idx] = (struct AsmImport *)malloc(sizeof(struct AsmImport));
                asm_imports[imp_idx]->module_name = mod_name;
                asm_imports[imp_idx]->field_name = field_name;
                asm_imports[imp_idx]->func_idx = imp_idx;
                /* Skip to end of import form */
                while (asm_tok != WTOK_EOF) {
                    if (asm_tok == WTOK_RPAREN) { asm_next(); break; }
                    asm_next();
                }
                /* One more ) for the outer import */
                if (asm_tok == WTOK_RPAREN) asm_next();

            } else if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "memory") == 0) {
                /* (memory (export "memory") 512) or (memory 512) */
                asm_next();
                if (asm_tok == WTOK_LPAREN) {
                    /* (export "name") */
                    asm_next(); /* "export" */
                    asm_next(); /* string */
                    asm_mem_export_name = asm_tok_str;
                    asm_next(); /* ) */
                    if (asm_tok == WTOK_RPAREN) asm_next();
                }
                if (asm_tok == WTOK_INT) {
                    asm_memory_pages = asm_tok_int;
                    asm_next();
                }
                if (asm_tok == WTOK_RPAREN) asm_next();

            } else if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "type") == 0) {
                /* (type $name (func (param ...) (result ...))) */
                char *tname;
                int tidx;
                asm_next(); /* $name */
                tname = asm_tok_str;
                asm_next(); /* ( */
                asm_next(); /* "func" */
                asm_next(); /* past "func" to params */
                tidx = asm_parse_func_sig((int *)0);
                /* Register named type */
                if (asm_ntype_names < ASM_MAX_TYPE_NAMES) {
                    asm_type_names[asm_ntype_names] = tname;
                    asm_type_indices[asm_ntype_names] = tidx;
                    asm_ntype_names++;
                }
                /* Skip to end: ) for func, ) for type */
                if (asm_tok == WTOK_RPAREN) asm_next();
                if (asm_tok == WTOK_RPAREN) asm_next();

            } else if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "table") == 0) {
                /* (table N funcref) */
                asm_next();
                if (asm_tok == WTOK_INT) {
                    asm_has_table = 1;
                    asm_table_size = asm_tok_int;
                    asm_next();
                }
                /* skip "funcref" and ) */
                if (asm_tok == WTOK_KW) asm_next();
                if (asm_tok == WTOK_RPAREN) asm_next();

            } else if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "elem") == 0) {
                /* (elem (i32.const 0) $fn1 $fn2 ...) */
                asm_next();
                /* Skip (i32.const 0) */
                if (asm_tok == WTOK_LPAREN) {
                    asm_next(); /* "i32.const" */
                    asm_next(); /* 0 */
                    asm_next(); /* ) */
                    if (asm_tok == WTOK_RPAREN) asm_next();
                }
                /* Parse function names */
                asm_nelem = 0;
                while (asm_tok == WTOK_NAME) {
                    if (asm_nelem < ASM_MAX_ELEM) {
                        asm_elem_indices[asm_nelem] = asm_find_func(asm_tok_str);
                        asm_nelem++;
                    }
                    asm_next();
                }
                if (asm_tok == WTOK_RPAREN) asm_next();

            } else if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "data") == 0) {
                /* (data (i32.const N) "string") */
                int offset;
                struct AsmData *d;
                asm_next();
                /* (i32.const N) */
                if (asm_tok == WTOK_LPAREN) {
                    asm_next(); /* "i32.const" */
                    asm_next(); /* N */
                    offset = asm_tok_int;
                    asm_next(); /* ) */
                    if (asm_tok == WTOK_RPAREN) asm_next();
                } else {
                    offset = 0;
                }
                /* String */
                if (asm_tok == WTOK_STRING) {
                    d = (struct AsmData *)malloc(sizeof(struct AsmData));
                    d->offset = offset;
                    d->bytes = bv_new(256);
                    asm_decode_string(asm_tok_str, strlen(asm_tok_str), d->bytes);
                    if (asm_ndata < ASM_MAX_DATA) {
                        asm_data_segs[asm_ndata] = d;
                        asm_ndata++;
                    }
                    asm_next();
                }
                if (asm_tok == WTOK_RPAREN) asm_next();

            } else if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "global") == 0) {
                /* (global $name (mut type) (type.const val)) */
                char *gname;
                int gtype;
                int is_mut;
                struct AsmGlobal *g;
                struct ByteVec *init;
                asm_next(); /* $name */
                gname = asm_tok_str;
                asm_next();

                /* (mut type) or just type */
                is_mut = 0;
                gtype = 0x7F;
                if (asm_tok == WTOK_LPAREN) {
                    asm_next();
                    if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "mut") == 0) {
                        is_mut = 1;
                        asm_next();
                        gtype = asm_parse_valtype(asm_tok_str);
                        asm_next();
                    } else {
                        gtype = asm_parse_valtype(asm_tok_str);
                        asm_next();
                    }
                    if (asm_tok == WTOK_RPAREN) asm_next();
                }

                /* Init expression: (type.const val) */
                init = bv_new(16);
                if (asm_tok == WTOK_LPAREN) {
                    asm_next(); /* instruction like i32.const */
                    if (asm_tok == WTOK_KW) {
                        char *init_kw;
                        init_kw = asm_tok_str;
                        asm_next(); /* value */
                        if (strcmp(init_kw, "i32.const") == 0) {
                            bv_push(init, 0x41);
                            bv_i32(init, asm_tok_int);
                        } else if (strcmp(init_kw, "f64.const") == 0) {
                            bv_push(init, 0x44);
                            if (asm_tok == WTOK_FLOAT) {
                                asm_emit_f64(init, asm_tok_str);
                            } else {
                                /* Integer as float */
                                char buf[20];
                                int v;
                                int bi;
                                v = asm_tok_int;
                                bi = 0;
                                if (v < 0) { buf[bi] = '-'; bi++; v = -v; }
                                if (v == 0) { buf[bi] = '0'; bi++; }
                                else {
                                    char tmp[12];
                                    int ti;
                                    ti = 0;
                                    while (v > 0) { tmp[ti] = '0' + (v % 10); ti++; v = v / 10; }
                                    while (ti > 0) { ti--; buf[bi] = tmp[ti]; bi++; }
                                }
                                buf[bi] = '.';
                                bi++;
                                buf[bi] = '0';
                                bi++;
                                buf[bi] = 0;
                                asm_emit_f64(init, buf);
                            }
                        }
                        asm_next(); /* past value */
                    }
                    if (asm_tok == WTOK_RPAREN) asm_next();
                }
                bv_push(init, 0x0B); /* end init expr */

                g = (struct AsmGlobal *)malloc(sizeof(struct AsmGlobal));
                g->name = gname;
                g->val_type = gtype;
                g->is_mutable = is_mut;
                g->init_expr = init;
                if (asm_nglobals < ASM_MAX_GLOBALS) {
                    asm_globals[asm_nglobals] = g;
                    asm_nglobals++;
                }
                if (asm_tok == WTOK_RPAREN) asm_next();

            } else if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "func") == 0) {
                /* (func $name [(export "name")] (param ...) (result ...) (local ...) body) */
                char *fname;
                int fidx;
                int type_idx;
                asm_next(); /* $name */
                fname = asm_tok_str;
                fidx = asm_find_func(fname);
                asm_next();

                /* Check for inline (export "name") */
                if (asm_tok == WTOK_LPAREN) {
                    asm_save();
                    asm_next();
                    if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "export") == 0) {
                        asm_next(); /* string */
                        asm_func_export_name = asm_tok_str;
                        asm_func_export_idx = fidx;
                        asm_next(); /* ) */
                        if (asm_tok == WTOK_RPAREN) asm_next();
                    } else {
                        asm_restore();
                    }
                }

                /* Parse type signature (but don't consume body) */
                {
                    int ptypes[16];
                    int np;
                    int nr;
                    int rt;
                    np = 0;
                    nr = 0;
                    rt = 0;
                    /* Peek at params/result without consuming them */
                    asm_save();
                    while (asm_tok == WTOK_LPAREN) {
                        int inner_saved_pos;
                        int inner_saved_tok;
                        char *inner_saved_str;
                        int inner_saved_int;
                        inner_saved_pos = asm_pos;
                        inner_saved_tok = asm_tok;
                        inner_saved_str = asm_tok_str;
                        inner_saved_int = asm_tok_int;
                        asm_next();
                        if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "param") == 0) {
                            asm_next();
                            if (asm_tok == WTOK_NAME) asm_next();
                            while (asm_tok == WTOK_KW && (strcmp(asm_tok_str, "i32") == 0 ||
                                   strcmp(asm_tok_str, "i64") == 0 || strcmp(asm_tok_str, "f64") == 0)) {
                                if (np < ASM_MAX_PARAMS) { ptypes[np] = asm_parse_valtype(asm_tok_str); np++; }
                                asm_next();
                            }
                            if (asm_tok == WTOK_RPAREN) asm_next();
                        } else if (asm_tok == WTOK_KW && strcmp(asm_tok_str, "result") == 0) {
                            asm_next();
                            rt = asm_parse_valtype(asm_tok_str);
                            nr = 1;
                            asm_next();
                            if (asm_tok == WTOK_RPAREN) asm_next();
                        } else {
                            /* Not param or result — restore and stop */
                            asm_pos = inner_saved_pos;
                            asm_tok = inner_saved_tok;
                            asm_tok_str = inner_saved_str;
                            asm_tok_int = inner_saved_int;
                            break;
                        }
                    }
                    type_idx = asm_find_or_add_type(np, ptypes, nr, rt);
                    asm_funcs[fidx]->type_idx = type_idx;
                    asm_restore();
                }

                /* Parse function body and encode it */
                asm_parse_func_body(fidx, code_sec);

                if (asm_tok == WTOK_RPAREN) asm_next();

            } else {
                /* Unknown top-level form — skip */
                int depth;
                depth = 1;
                while (depth > 0 && asm_tok != WTOK_EOF) {
                    asm_next();
                    if (asm_tok == WTOK_LPAREN) depth++;
                    if (asm_tok == WTOK_RPAREN) depth--;
                }
                if (asm_tok == WTOK_RPAREN) asm_next();
            }
        } else {
            asm_next(); /* skip non-( tokens at top level */
        }
    }

    /* ================================================================
     * Encode WASM binary
     * ================================================================ */

    nlocal_funcs = asm_nfuncs - asm_nimports;

    /* Magic + version */
    bv_push(wasm, 0x00); bv_push(wasm, 0x61);
    bv_push(wasm, 0x73); bv_push(wasm, 0x6D);
    bv_push(wasm, 0x01); bv_push(wasm, 0x00);
    bv_push(wasm, 0x00); bv_push(wasm, 0x00);

    /* Section 1: Type */
    bv_reset(sec);
    bv_u32(sec, asm_ntypes);
    for (i = 0; i < asm_ntypes; i++) {
        bv_push(sec, 0x60); /* func type */
        bv_u32(sec, asm_types[i]->nparams);
        for (j = 0; j < asm_types[i]->nparams; j++) {
            bv_push(sec, asm_types[i]->param_types[j]);
        }
        if (asm_types[i]->nresults > 0) {
            bv_u32(sec, 1);
            bv_push(sec, asm_types[i]->result_type);
        } else {
            bv_u32(sec, 0);
        }
    }
    bv_section(wasm, 1, sec);

    /* Section 2: Import */
    bv_reset(sec);
    bv_u32(sec, asm_nimports);
    for (i = 0; i < asm_nimports; i++) {
        bv_name(sec, asm_imports[i]->module_name, strlen(asm_imports[i]->module_name));
        bv_name(sec, asm_imports[i]->field_name, strlen(asm_imports[i]->field_name));
        bv_push(sec, 0x00); /* import kind: func */
        bv_u32(sec, asm_funcs[i]->type_idx);
    }
    bv_section(wasm, 2, sec);

    /* Section 3: Function */
    bv_reset(sec);
    bv_u32(sec, nlocal_funcs);
    for (i = asm_nimports; i < asm_nfuncs; i++) {
        bv_u32(sec, asm_funcs[i]->type_idx);
    }
    bv_section(wasm, 3, sec);

    /* Section 4: Table (optional) */
    if (asm_has_table) {
        bv_reset(sec);
        bv_u32(sec, 1);
        bv_push(sec, 0x70); /* funcref */
        bv_push(sec, 0x00); /* limits: min only */
        bv_u32(sec, asm_table_size);
        bv_section(wasm, 4, sec);
    }

    /* Section 5: Memory */
    bv_reset(sec);
    bv_u32(sec, 1);
    bv_push(sec, 0x00); /* limits: min only */
    bv_u32(sec, asm_memory_pages);
    bv_section(wasm, 5, sec);

    /* Section 6: Global */
    if (asm_nglobals > 0) {
        bv_reset(sec);
        bv_u32(sec, asm_nglobals);
        for (i = 0; i < asm_nglobals; i++) {
            bv_push(sec, asm_globals[i]->val_type);
            bv_push(sec, asm_globals[i]->is_mutable ? 0x01 : 0x00);
            bv_append(sec, asm_globals[i]->init_expr);
        }
        bv_section(wasm, 6, sec);
    }

    /* Section 7: Export */
    {
        int nexports;
        nexports = 0;
        if (asm_mem_export_name != (char *)0) nexports++;
        if (asm_func_export_name != (char *)0) nexports++;

        if (nexports > 0) {
            bv_reset(sec);
            bv_u32(sec, nexports);
            if (asm_mem_export_name != (char *)0) {
                bv_name(sec, asm_mem_export_name, strlen(asm_mem_export_name));
                bv_push(sec, 0x02); /* memory export */
                bv_u32(sec, 0);
            }
            if (asm_func_export_name != (char *)0) {
                bv_name(sec, asm_func_export_name, strlen(asm_func_export_name));
                bv_push(sec, 0x00); /* func export */
                bv_u32(sec, asm_func_export_idx);
            }
            bv_section(wasm, 7, sec);
        }
    }

    /* Section 9: Element (optional) */
    if (asm_nelem > 0) {
        bv_reset(sec);
        bv_u32(sec, 1); /* one elem segment */
        bv_u32(sec, 0); /* table 0, active mode */
        bv_push(sec, 0x41); bv_i32(sec, 0); bv_push(sec, 0x0B); /* offset = 0; end */
        bv_u32(sec, asm_nelem);
        for (i = 0; i < asm_nelem; i++) {
            bv_u32(sec, asm_elem_indices[i]);
        }
        bv_section(wasm, 9, sec);
    }

    /* Section 10: Code */
    {
        struct ByteVec *code_full;
        code_full = bv_new(code_sec->len + 8);
        bv_u32(code_full, nlocal_funcs);
        bv_append(code_full, code_sec);
        bv_section(wasm, 10, code_full);
    }

    /* Section 11: Data */
    if (asm_ndata > 0) {
        bv_reset(sec);
        bv_u32(sec, asm_ndata);
        for (i = 0; i < asm_ndata; i++) {
            bv_push(sec, 0x00); /* active, memory 0 */
            bv_push(sec, 0x41); bv_i32(sec, asm_data_segs[i]->offset); bv_push(sec, 0x0B);
            bv_u32(sec, asm_data_segs[i]->bytes->len);
            bv_append(sec, asm_data_segs[i]->bytes);
        }
        bv_section(wasm, 11, sec);
    }

    /* Write binary to stdout */
    for (i = 0; i < wasm->len; i++) {
        putchar(wasm->data[i] & 0xFF);
    }
}

/* ================================================================
 * Entry Point
 * ================================================================ */

void assemble_wat(struct ByteVec *wat) {
    /* Initialize tokenizer */
    asm_src = wat->data;
    asm_src_len = wat->len;
    asm_pos = 0;

    /* Allocate module state */
    asm_types = (struct AsmType **)malloc(ASM_MAX_TYPES * sizeof(void *));
    asm_ntypes = 0;
    asm_type_names = (char **)malloc(ASM_MAX_TYPE_NAMES * sizeof(char *));
    asm_type_indices = (int *)malloc(ASM_MAX_TYPE_NAMES * sizeof(int));
    asm_ntype_names = 0;

    asm_funcs = (struct AsmFunc **)malloc(ASM_MAX_FUNCS * sizeof(void *));
    asm_nfuncs = 0;
    asm_nimports = 0;
    asm_imports = (struct AsmImport **)malloc(ASM_MAX_FUNCS * sizeof(void *));

    asm_globals = (struct AsmGlobal **)malloc(ASM_MAX_GLOBALS * sizeof(void *));
    asm_nglobals = 0;

    asm_data_segs = (struct AsmData **)malloc(ASM_MAX_DATA * sizeof(void *));
    asm_ndata = 0;

    asm_elem_indices = (int *)malloc(ASM_MAX_ELEM * sizeof(int));
    asm_nelem = 0;

    asm_local_names = (char **)malloc(ASM_MAX_LOCALS * sizeof(char *));
    asm_local_types = (int *)malloc(ASM_MAX_LOCALS * sizeof(int));
    asm_nlocals = 0;

    asm_label_stack = (char **)malloc(ASM_MAX_LABELS * sizeof(char *));
    asm_label_sp = 0;

    /* First pass: collect function names */
    asm_first_pass();

    /* Reset tokenizer for second pass */
    asm_pos = 0;

    /* Second pass: parse and encode */
    asm_second_pass();
}
