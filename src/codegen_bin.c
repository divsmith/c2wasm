/* codegen_bin.c — WASM binary emitter */

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
    int bt_nparams;
    int bt_has_result;
    int result_is_float;
    int *bt_pif;  /* malloc'd array of per-param float flags */
};

struct BinTypeEntry **bin_types;
int bin_ntypes;

int bin_find_or_add_type_f(int nparams, int *pif, int has_result, int rif) {
    int i;
    int j;
    int match;
    for (i = 0; i < bin_ntypes; i++) {
        if (bin_types[i]->bt_nparams != nparams) continue;
        if (bin_types[i]->bt_has_result != has_result) continue;
        if (bin_types[i]->result_is_float != rif) continue;
        match = 1;
        for (j = 0; j < nparams && j < 16; j++) {
            if (bin_types[i]->bt_pif[j] != pif[j]) { match = 0; break; }
        }
        if (match) return i;
    }
    if (bin_ntypes >= MAX_BIN_TYPES) {
        printf("too many binary types\n");
        exit(1);
    }
    bin_types[bin_ntypes] = (struct BinTypeEntry *)malloc(sizeof(struct BinTypeEntry));
    bin_types[bin_ntypes]->bt_nparams = nparams;
    bin_types[bin_ntypes]->bt_has_result = has_result;
    bin_types[bin_ntypes]->result_is_float = rif;
    bin_types[bin_ntypes]->bt_pif = (int *)malloc(16 * sizeof(int));
    for (j = 0; j < nparams && j < 16; j++) {
        bin_types[bin_ntypes]->bt_pif[j] = pif[j];
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
    int bf_nparams;
    int bf_has_result;
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
    bin_funcs[bin_nfuncs]->bf_nparams = nparams;
    bin_funcs[bin_nfuncs]->bf_has_result = has_result;
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
    bin_funcs[bin_nfuncs]->bf_nparams = nparams;
    bin_funcs[bin_nfuncs]->bf_has_result = has_result;
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

/* --- Binary expression codegen --- */

typedef void (*GenExprBinFn)(struct ByteVec *, struct Node *);
GenExprBinFn *gen_expr_bin_tbl;

void gen_expr_bin(struct ByteVec *o, struct Node *n);

void gen_expr_bin_int_lit(struct ByteVec *o, struct Node *n);
void gen_expr_bin_float_lit(struct ByteVec *o, struct Node *n);
void gen_expr_bin_cast(struct ByteVec *o, struct Node *n);
void gen_expr_bin_ident(struct ByteVec *o, struct Node *n);
void gen_expr_bin_assign(struct ByteVec *o, struct Node *n);
void gen_expr_bin_unary(struct ByteVec *o, struct Node *n);
void gen_expr_bin_binary(struct ByteVec *o, struct Node *n);
void gen_expr_bin_call(struct ByteVec *o, struct Node *n);
void gen_expr_bin_str_lit(struct ByteVec *o, struct Node *n);
void gen_expr_bin_call_indirect(struct ByteVec *o, struct Node *n);
void gen_expr_bin_member(struct ByteVec *o, struct Node *n);
void gen_expr_bin_sizeof(struct ByteVec *o, struct Node *n);
void gen_expr_bin_subscript(struct ByteVec *o, struct Node *n);
void gen_expr_bin_post_inc_dec(struct ByteVec *o, struct Node *n);
void gen_expr_bin_ternary(struct ByteVec *o, struct Node *n);
void gen_expr_bin_error(struct ByteVec *o, struct Node *n);

void gen_expr_bin_int_lit(struct ByteVec *o, struct Node *n) {
    bv_push(o, 0x41); bv_i32(o, n->ival);
    bin_last_float = 0;
}

void gen_expr_bin_float_lit(struct ByteVec *o, struct Node *n) {
    emit_f64_const_bin(o, n->sval);
    bin_last_float = 2;
}

void gen_expr_bin_cast(struct ByteVec *o, struct Node *n) {
    gen_expr_bin(o, n->c0);
    if (n->ival >= 1 && !bin_last_float) {
        bv_push(o, 0xB7); /* f64.convert_i32_s */
        bin_last_float = 2;
    } else if (n->ival == 0 && bin_last_float) {
        bv_push(o, 0xAA); /* i32.trunc_f64_s */
        bin_last_float = 0;
    }
}

void gen_expr_bin_ident(struct ByteVec *o, struct Node *n) {
    if (find_global(n->sval) >= 0) {
        bv_push(o, 0x23); bv_u32(o, bin_global_idx(n->sval));
    } else {
        bv_push(o, 0x20); bv_u32(o, find_local(n->sval));
    }
    bin_last_float = var_is_float(n->sval);
}

void gen_expr_bin_assign(struct ByteVec *o, struct Node *n) {
    struct Node *tgt;
    char *name;
    int is_global;
    int off;
    int esz;
    int li;

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
}

void gen_expr_bin_unary(struct ByteVec *o, struct Node *n) {
    int esz;

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
}

void gen_expr_bin_binary(struct ByteVec *o, struct Node *n) {
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
}

void gen_expr_bin_call(struct ByteVec *o, struct Node *n) {
    char *fmt;
    int flen;
    int ai;
    int fi;
    int sid;
    int i;

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
}

void gen_expr_bin_str_lit(struct ByteVec *o, struct Node *n) {
    bv_push(o, 0x41); bv_i32(o, str_table[n->ival]->offset);
    bin_last_float = 0;
}

void gen_expr_bin_call_indirect(struct ByteVec *o, struct Node *n) {
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
}

void gen_expr_bin_member(struct ByteVec *o, struct Node *n) {
    int off;

    off = resolve_field_offset(n->sval);
    if (off < 0) error(n->nline, n->ncol, "unknown struct field");
    gen_expr_bin(o, n->c0);
    if (off > 0) { bv_push(o, 0x41); bv_i32(o, off); bv_push(o, 0x6A); }
    bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0);
    bin_last_float = 0;
}

void gen_expr_bin_sizeof(struct ByteVec *o, struct Node *n) {
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
}

void gen_expr_bin_subscript(struct ByteVec *o, struct Node *n) {
    int esz;

    esz = expr_elem_size(n->c0);
    gen_expr_bin(o, n->c0);
    gen_expr_bin(o, n->c1);
    if (esz > 1) { bv_push(o, 0x41); bv_i32(o, esz); bv_push(o, 0x6C); }
    bv_push(o, 0x6A);
    if (esz == 1) { bv_push(o, 0x2D); bv_u32(o, 0); bv_u32(o, 0); }
    else if (esz == 2) { bv_push(o, 0x2E); bv_u32(o, 1); bv_u32(o, 0); }
    else { bv_push(o, 0x28); bv_u32(o, 2); bv_u32(o, 0); }
    bin_last_float = 0;
}

void gen_expr_bin_post_inc_dec(struct ByteVec *o, struct Node *n) {
    int li;

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
}

void gen_expr_bin_ternary(struct ByteVec *o, struct Node *n) {
    gen_expr_bin(o, n->c0);
    bv_push(o, 0x04); bv_push(o, 0x7F); bin_lbl_sp++;
    gen_expr_bin(o, n->c1);
    bv_push(o, 0x05);
    gen_expr_bin(o, n->c2);
    bv_push(o, 0x0B); bin_lbl_sp--;
    bin_last_float = 0;
}

void gen_expr_bin_error(struct ByteVec *o, struct Node *n) {
    error(n->nline, n->ncol, "unsupported expression in binary codegen");
}

void init_gen_expr_bin_tbl(void) {
    int tbl_i;
    gen_expr_bin_tbl = (GenExprBinFn *)malloc(ND_COUNT * sizeof(GenExprBinFn));
    for (tbl_i = 0; tbl_i < ND_COUNT; tbl_i++) {
        gen_expr_bin_tbl[tbl_i] = gen_expr_bin_error;
    }
    gen_expr_bin_tbl[ND_ASSIGN] = gen_expr_bin_assign;
    gen_expr_bin_tbl[ND_BINARY] = gen_expr_bin_binary;
    gen_expr_bin_tbl[ND_CALL] = gen_expr_bin_call;
    gen_expr_bin_tbl[ND_CALL_INDIRECT] = gen_expr_bin_call_indirect;
    gen_expr_bin_tbl[ND_CAST] = gen_expr_bin_cast;
    gen_expr_bin_tbl[ND_FLOAT_LIT] = gen_expr_bin_float_lit;
    gen_expr_bin_tbl[ND_IDENT] = gen_expr_bin_ident;
    gen_expr_bin_tbl[ND_INT_LIT] = gen_expr_bin_int_lit;
    gen_expr_bin_tbl[ND_MEMBER] = gen_expr_bin_member;
    gen_expr_bin_tbl[ND_POST_DEC] = gen_expr_bin_post_inc_dec;
    gen_expr_bin_tbl[ND_POST_INC] = gen_expr_bin_post_inc_dec;
    gen_expr_bin_tbl[ND_SIZEOF] = gen_expr_bin_sizeof;
    gen_expr_bin_tbl[ND_STR_LIT] = gen_expr_bin_str_lit;
    gen_expr_bin_tbl[ND_SUBSCRIPT] = gen_expr_bin_subscript;
    gen_expr_bin_tbl[ND_TERNARY] = gen_expr_bin_ternary;
    gen_expr_bin_tbl[ND_UNARY] = gen_expr_bin_unary;
}

void gen_expr_bin(struct ByteVec *o, struct Node *n) {
    GenExprBinFn efn;
    if (n->kind < 0 || n->kind >= ND_COUNT) {
        error(n->nline, n->ncol, "unsupported expression in binary codegen");
    }
    efn = gen_expr_bin_tbl[n->kind];
    efn(o, n);
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

/* --- Binary statement codegen --- */

typedef void (*GenStmtBinFn)(struct ByteVec *, struct Node *);
GenStmtBinFn *gen_stmt_bin_tbl;

void gen_stmt_bin_return(struct ByteVec *o, struct Node *n);
void gen_stmt_bin_var_decl(struct ByteVec *o, struct Node *n);
void gen_stmt_bin_expr_stmt(struct ByteVec *o, struct Node *n);
void gen_stmt_bin_if(struct ByteVec *o, struct Node *n);
void gen_stmt_bin_while(struct ByteVec *o, struct Node *n);
void gen_stmt_bin_for(struct ByteVec *o, struct Node *n);
void gen_stmt_bin_do_while(struct ByteVec *o, struct Node *n);
void gen_stmt_bin_break(struct ByteVec *o, struct Node *n);
void gen_stmt_bin_continue(struct ByteVec *o, struct Node *n);
void gen_stmt_bin_block(struct ByteVec *o, struct Node *n);
void gen_stmt_bin_switch(struct ByteVec *o, struct Node *n);
void gen_stmt_bin_error(struct ByteVec *o, struct Node *n);

void gen_stmt_bin_return(struct ByteVec *o, struct Node *n) {
    if (n->c0 != (struct Node *)0) {
        gen_expr_bin(o, n->c0);
    }
    bv_push(o, 0x0F);
}

void gen_stmt_bin_var_decl(struct ByteVec *o, struct Node *n) {
    int bsz;
    int decl_float;

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
}

void gen_stmt_bin_expr_stmt(struct ByteVec *o, struct Node *n) {
    gen_expr_bin(o, n->c0);
    bv_push(o, 0x1A);
}

void gen_stmt_bin_if(struct ByteVec *o, struct Node *n) {
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
}

void gen_stmt_bin_while(struct ByteVec *o, struct Node *n) {
    int lbl;

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
}

void gen_stmt_bin_for(struct ByteVec *o, struct Node *n) {
    int lbl;

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
}

void gen_stmt_bin_do_while(struct ByteVec *o, struct Node *n) {
    int lbl;

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
}

void gen_stmt_bin_break(struct ByteVec *o, struct Node *n) {
    if (loop_sp <= 0) error(n->nline, n->ncol, "break outside loop");
    bv_push(o, 0x0C); bv_u32(o, bin_lbl_sp - bin_brk_at[loop_sp - 1] - 1);
}

void gen_stmt_bin_continue(struct ByteVec *o, struct Node *n) {
    if (loop_sp <= 0) error(n->nline, n->ncol, "continue outside loop");
    if (cont_lbl[loop_sp - 1] < 0) error(n->nline, n->ncol, "continue not inside a loop");
    bv_push(o, 0x0C); bv_u32(o, bin_lbl_sp - bin_cont_at[loop_sp - 1] - 1);
}

void gen_stmt_bin_block(struct ByteVec *o, struct Node *n) {
    int i;

    for (i = 0; i < n->ival2; i++) {
        gen_stmt_bin(o, n->list[i]);
    }
}

void gen_stmt_bin_switch(struct ByteVec *o, struct Node *n) {
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
}

void gen_stmt_bin_error(struct ByteVec *o, struct Node *n) {
    error(n->nline, n->ncol, "unsupported statement in binary codegen");
}

void init_gen_stmt_bin_tbl(void) {
    int tbl_i;
    gen_stmt_bin_tbl = (GenStmtBinFn *)malloc(ND_COUNT * sizeof(GenStmtBinFn));
    for (tbl_i = 0; tbl_i < ND_COUNT; tbl_i++) {
        gen_stmt_bin_tbl[tbl_i] = gen_stmt_bin_error;
    }
    gen_stmt_bin_tbl[ND_BLOCK] = gen_stmt_bin_block;
    gen_stmt_bin_tbl[ND_BREAK] = gen_stmt_bin_break;
    gen_stmt_bin_tbl[ND_CONTINUE] = gen_stmt_bin_continue;
    gen_stmt_bin_tbl[ND_DO_WHILE] = gen_stmt_bin_do_while;
    gen_stmt_bin_tbl[ND_EXPR_STMT] = gen_stmt_bin_expr_stmt;
    gen_stmt_bin_tbl[ND_FOR] = gen_stmt_bin_for;
    gen_stmt_bin_tbl[ND_IF] = gen_stmt_bin_if;
    gen_stmt_bin_tbl[ND_RETURN] = gen_stmt_bin_return;
    gen_stmt_bin_tbl[ND_SWITCH] = gen_stmt_bin_switch;
    gen_stmt_bin_tbl[ND_VAR_DECL] = gen_stmt_bin_var_decl;
    gen_stmt_bin_tbl[ND_WHILE] = gen_stmt_bin_while;
}

void gen_stmt_bin(struct ByteVec *o, struct Node *n) {
    GenStmtBinFn sfn;
    if (n->kind < 0 || n->kind >= ND_COUNT) {
        error(n->nline, n->ncol, "unsupported statement in binary codegen");
    }
    sfn = gen_stmt_bin_tbl[n->kind];
    sfn(o, n);
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
        bv_u32(sec, bin_types[i]->bt_nparams);
        for (j = 0; j < bin_types[i]->bt_nparams; j++) {
            if (j < 16 && bin_types[i]->bt_pif[j]) {
                bv_push(sec, 0x7C); /* f64 */
            } else {
                bv_push(sec, 0x7F); /* i32 */
            }
        }
        if (bin_types[i]->bt_has_result) {
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

