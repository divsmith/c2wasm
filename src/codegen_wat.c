/* codegen_wat.c — WAT text format code generator */

int indent_level;

void emit_indent(void) {
    int i;
    for (i = 0; i < indent_level; i++) {
        out("  ");
    }
}

void emit_load(int esz) {
    if (esz == 1) { out("i32.load8_u\n"); }
    else if (esz == 2) { out("i32.load16_s\n"); }
    else { out("i32.load\n"); }
}

void emit_store(int esz) {
    if (esz == 1) { out("i32.store8\n"); }
    else if (esz == 2) { out("i32.store16\n"); }
    else { out("i32.store\n"); }
}

/* --- Expression codegen --- */

/* Forward declarations for dispatch table refactoring */
void gen_expr(struct Node *n);
void gen_stmt(struct Node *n);

typedef void (*GenExprFn)(struct Node *);
typedef void (*GenStmtFn)(struct Node *);



GenExprFn *gen_expr_tbl;
GenStmtFn *gen_stmt_tbl;

void gen_expr_int_lit(struct Node *n) {
    emit_indent();
    out("i32.const "); out_d(n->ival); out("\n");
    last_expr_is_float = 0;
}

void gen_expr_float_lit(struct Node *n) {
    emit_indent();
    out("f64.const "); out(n->sval); out("\n");
    last_expr_is_float = 2;
}

void gen_expr_cast(struct Node *n) {
    gen_expr(n->c0);
    if (n->ival >= 1 && !last_expr_is_float) {
        /* cast to float/double, expr is int */
        emit_indent();
        out("f64.convert_i32_s\n");
        last_expr_is_float = 2;
    } else if (n->ival >= 1 && last_expr_is_float) {
        /* cast to float/double, already float — no-op */
        last_expr_is_float = 2;
    } else if (n->ival == 0 && last_expr_is_float) {
        /* cast to int, expr is float */
        emit_indent();
        out("i32.trunc_f64_s\n");
        last_expr_is_float = 0;
    }
    /* cast to int when already int — no-op */
}

void gen_expr_ident(struct Node *n) {
    int vf;
    vf = var_is_float(n->sval);
    if (find_global(n->sval) >= 0) {
        emit_indent();
        out("global.get $"); out(n->sval); out("\n");
    } else {
        emit_indent();
        out("local.get $"); out(n->sval); out("\n");
    }
    last_expr_is_float = vf;
}

void emit_bitwise_assign_op(int op) {
    switch (op) {
    case TOK_PIPE_EQ:   out("i32.or\n");  break;
    case TOK_AMP_EQ:    out("i32.and\n"); break;
    case TOK_CARET_EQ:  out("i32.xor\n"); break;
    case TOK_LSHIFT_EQ: out("i32.shl\n"); break;
    default:            out("i32.shr_s\n"); break;
    }
}

void gen_expr_assign(struct Node *n) {
    struct Node *tgt;
    char *name;
    int is_global;
    int off;
    int esz;

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
                out("f64.convert_i32_s\n");
                last_expr_is_float = 2;
            } else if (!tgt_float && last_expr_is_float) {
                emit_indent();
                out("i32.trunc_f64_s\n");
                last_expr_is_float = 0;
            }
        } else if (n->ival == TOK_PLUS_EQ) {
            emit_indent();
            if (is_global) {
                out("global.get $"); out(name); out("\n");
            } else {
                out("local.get $"); out(name); out("\n");
            }
            gen_expr(n->c1);
            if (tgt_float) {
                if (!last_expr_is_float) {
                    emit_indent();
                    out("f64.convert_i32_s\n");
                }
                emit_indent();
                out("f64.add\n");
                last_expr_is_float = 2;
            } else {
                emit_indent();
                out("i32.add\n");
                last_expr_is_float = 0;
            }
        } else if (n->ival == TOK_MINUS_EQ) {
            emit_indent();
            if (is_global) {
                out("global.get $"); out(name); out("\n");
            } else {
                out("local.get $"); out(name); out("\n");
            }
            gen_expr(n->c1);
            if (tgt_float) {
                if (!last_expr_is_float) {
                    emit_indent();
                    out("f64.convert_i32_s\n");
                }
                emit_indent();
                out("f64.sub\n");
                last_expr_is_float = 2;
            } else {
                emit_indent();
                out("i32.sub\n");
                last_expr_is_float = 0;
            }
        } else if (n->ival == TOK_STAR_EQ || n->ival == TOK_SLASH_EQ ||
                   n->ival == TOK_PERCENT_EQ) {
            emit_indent();
            if (is_global) {
                out("global.get $"); out(name); out("\n");
            } else {
                out("local.get $"); out(name); out("\n");
            }
            gen_expr(n->c1);
            if (tgt_float && n->ival != TOK_PERCENT_EQ) {
                if (!last_expr_is_float) {
                    emit_indent();
                    out("f64.convert_i32_s\n");
                }
                emit_indent();
                if (n->ival == TOK_STAR_EQ) {
                    out("f64.mul\n");
                } else {
                    out("f64.div\n");
                }
                last_expr_is_float = 2;
            } else {
                emit_indent();
                if (n->ival == TOK_STAR_EQ) {
                    out("i32.mul\n");
                } else if (n->ival == TOK_SLASH_EQ) {
                    if (expr_is_unsigned(tgt)) {
                        out("i32.div_u\n");
                    } else {
                        out("i32.div_s\n");
                    }
                } else {
                    if (expr_is_unsigned(tgt)) {
                        out("i32.rem_u\n");
                    } else {
                        out("i32.rem_s\n");
                    }
                }
                last_expr_is_float = 0;
            }
        } else if (n->ival == TOK_PIPE_EQ || n->ival == TOK_AMP_EQ ||
                   n->ival == TOK_CARET_EQ || n->ival == TOK_LSHIFT_EQ ||
                   n->ival == TOK_RSHIFT_EQ) {
            emit_indent();
            if (is_global) {
                out("global.get $"); out(name); out("\n");
            } else {
                out("local.get $"); out(name); out("\n");
            }
            gen_expr(n->c1);
            emit_indent();
            emit_bitwise_assign_op(n->ival);
            last_expr_is_float = 0;
        }
        if (is_global) {
            if (tgt_float) {
                emit_indent();
                out("local.set $__ftmp\n");
                emit_indent();
                out("local.get $__ftmp\n");
                emit_indent();
                out("global.set $"); out(name); out("\n");
                emit_indent();
                out("local.get $__ftmp\n");
            } else {
                emit_indent();
                out("local.set $__atmp\n");
                emit_indent();
                out("local.get $__atmp\n");
                emit_indent();
                out("global.set $"); out(name); out("\n");
                emit_indent();
                out("local.get $__atmp\n");
            }
        } else {
            emit_indent();
            out("local.tee $"); out(name); out("\n");
        }
        last_expr_is_float = tgt_float;
    } else if (tgt->kind == ND_UNARY && tgt->ival == TOK_STAR) {
        esz = expr_elem_size(tgt->c0);
        if (n->ival == TOK_EQ) {
            gen_expr(n->c1);
        } else if (n->ival == TOK_PLUS_EQ) {
            gen_expr(tgt->c0);
            emit_indent();
            emit_load(esz);
            gen_expr(n->c1);
            emit_indent();
            out("i32.add\n");
        } else if (n->ival == TOK_MINUS_EQ) {
            gen_expr(tgt->c0);
            emit_indent();
            emit_load(esz);
            gen_expr(n->c1);
            emit_indent();
            out("i32.sub\n");
        } else if (n->ival == TOK_STAR_EQ || n->ival == TOK_SLASH_EQ ||
                   n->ival == TOK_PERCENT_EQ) {
            gen_expr(tgt->c0);
            emit_indent();
            emit_load(esz);
            gen_expr(n->c1);
            emit_indent();
            if (n->ival == TOK_STAR_EQ) {
                out("i32.mul\n");
            } else if (n->ival == TOK_SLASH_EQ) {
                out("i32.div_s\n");
            } else {
                out("i32.rem_s\n");
            }
        } else if (n->ival == TOK_PIPE_EQ || n->ival == TOK_AMP_EQ ||
                   n->ival == TOK_CARET_EQ || n->ival == TOK_LSHIFT_EQ ||
                   n->ival == TOK_RSHIFT_EQ) {
            gen_expr(tgt->c0);
            emit_indent();
            emit_load(esz);
            gen_expr(n->c1);
            emit_indent();
            emit_bitwise_assign_op(n->ival);
        }
        emit_indent();
        if (last_expr_is_float) {
            out("local.set $__ftmp\n");
            gen_expr(tgt->c0);
            emit_indent();
            out("local.get $__ftmp\n");
            emit_indent();
            out("f64.store\n");
            emit_indent();
            out("local.get $__ftmp\n");
        } else {
            out("local.set $__atmp\n");
            gen_expr(tgt->c0);
            emit_indent();
            out("local.get $__atmp\n");
            emit_indent();
            emit_store(esz);
            emit_indent();
            out("local.get $__atmp\n");
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
                out("i32.const "); out_d(off); out("\n");
                emit_indent();
                out("i32.add\n");
            }
            emit_indent();
            out("i32.load\n");
            gen_expr(n->c1);
            emit_indent();
            out("i32.add\n");
        } else if (n->ival == TOK_MINUS_EQ) {
            gen_expr(tgt->c0);
            if (off > 0) {
                emit_indent();
                out("i32.const "); out_d(off); out("\n");
                emit_indent();
                out("i32.add\n");
            }
            emit_indent();
            out("i32.load\n");
            gen_expr(n->c1);
            emit_indent();
            out("i32.sub\n");
        } else if (n->ival == TOK_STAR_EQ || n->ival == TOK_SLASH_EQ ||
                   n->ival == TOK_PERCENT_EQ) {
            gen_expr(tgt->c0);
            if (off > 0) {
                emit_indent();
                out("i32.const "); out_d(off); out("\n");
                emit_indent();
                out("i32.add\n");
            }
            emit_indent();
            out("i32.load\n");
            gen_expr(n->c1);
            emit_indent();
            if (n->ival == TOK_STAR_EQ) {
                out("i32.mul\n");
            } else if (n->ival == TOK_SLASH_EQ) {
                out("i32.div_s\n");
            } else {
                out("i32.rem_s\n");
            }
        } else if (n->ival == TOK_PIPE_EQ || n->ival == TOK_AMP_EQ ||
                   n->ival == TOK_CARET_EQ || n->ival == TOK_LSHIFT_EQ ||
                   n->ival == TOK_RSHIFT_EQ) {
            gen_expr(tgt->c0);
            if (off > 0) {
                emit_indent();
                out("i32.const "); out_d(off); out("\n");
                emit_indent();
                out("i32.add\n");
            }
            emit_indent();
            out("i32.load\n");
            gen_expr(n->c1);
            emit_indent();
            emit_bitwise_assign_op(n->ival);
        }
        emit_indent();
        out("local.set $__atmp\n");
        gen_expr(tgt->c0);
        if (off > 0) {
            emit_indent();
            out("i32.const "); out_d(off); out("\n");
            emit_indent();
            out("i32.add\n");
        }
        emit_indent();
        out("local.get $__atmp\n");
        emit_indent();
        out("i32.store\n");
        emit_indent();
        out("local.get $__atmp\n");
    } else if (tgt->kind == ND_SUBSCRIPT) {
        esz = expr_elem_size(tgt->c0);
        if (n->ival == TOK_EQ) {
            gen_expr(n->c1);
        } else if (n->ival == TOK_PLUS_EQ) {
            gen_expr(tgt->c0);
            gen_expr(tgt->c1);
            if (esz > 1) {
                emit_indent();
                out("i32.const "); out_d(esz); out("\n");
                emit_indent();
                out("i32.mul\n");
            }
            emit_indent();
            out("i32.add\n");
            emit_indent();
            emit_load(esz);
            gen_expr(n->c1);
            emit_indent();
            out("i32.add\n");
        } else if (n->ival == TOK_MINUS_EQ) {
            gen_expr(tgt->c0);
            gen_expr(tgt->c1);
            if (esz > 1) {
                emit_indent();
                out("i32.const "); out_d(esz); out("\n");
                emit_indent();
                out("i32.mul\n");
            }
            emit_indent();
            out("i32.add\n");
            emit_indent();
            emit_load(esz);
            gen_expr(n->c1);
            emit_indent();
            out("i32.sub\n");
        } else if (n->ival == TOK_STAR_EQ || n->ival == TOK_SLASH_EQ ||
                   n->ival == TOK_PERCENT_EQ) {
            gen_expr(tgt->c0);
            gen_expr(tgt->c1);
            if (esz > 1) {
                emit_indent();
                out("i32.const "); out_d(esz); out("\n");
                emit_indent();
                out("i32.mul\n");
            }
            emit_indent();
            out("i32.add\n");
            emit_indent();
            emit_load(esz);
            gen_expr(n->c1);
            emit_indent();
            if (n->ival == TOK_STAR_EQ) {
                out("i32.mul\n");
            } else if (n->ival == TOK_SLASH_EQ) {
                out("i32.div_s\n");
            } else {
                out("i32.rem_s\n");
            }
        } else if (n->ival == TOK_PIPE_EQ || n->ival == TOK_AMP_EQ ||
                   n->ival == TOK_CARET_EQ || n->ival == TOK_LSHIFT_EQ ||
                   n->ival == TOK_RSHIFT_EQ) {
            gen_expr(tgt->c0);
            gen_expr(tgt->c1);
            if (esz > 1) {
                emit_indent();
                out("i32.const "); out_d(esz); out("\n");
                emit_indent();
                out("i32.mul\n");
            }
            emit_indent();
            out("i32.add\n");
            emit_indent();
            emit_load(esz);
            gen_expr(n->c1);
            emit_indent();
            emit_bitwise_assign_op(n->ival);
        }
        emit_indent();
        out("local.set $__atmp\n");
        gen_expr(tgt->c0);
        gen_expr(tgt->c1);
        if (esz > 1) {
            emit_indent();
            out("i32.const "); out_d(esz); out("\n");
            emit_indent();
            out("i32.mul\n");
        }
        emit_indent();
        out("i32.add\n");
        emit_indent();
        out("local.get $__atmp\n");
        emit_indent();
        emit_store(esz);
        emit_indent();
        out("local.get $__atmp\n");
    }
}

void gen_expr_unary(struct Node *n) {
    int esz;

    switch (n->ival) {
    case TOK_MINUS:
        gen_expr(n->c0);
        if (last_expr_is_float) {
            emit_indent();
            out("f64.neg\n");
        } else {
            /* save value, push 0, push value, sub */
            emit_indent();
            out("local.set $__atmp\n");
            emit_indent();
            out("i32.const 0\n");
            emit_indent();
            out("local.get $__atmp\n");
            emit_indent();
            out("i32.sub\n");
        }
        break;
    case TOK_BANG:
        gen_expr(n->c0);
        if (last_expr_is_float) {
            emit_indent();
            out("f64.const 0\n");
            emit_indent();
            out("f64.eq\n");
            last_expr_is_float = 0;
        } else {
            emit_indent();
            out("i32.eqz\n");
        }
        break;
    case TOK_TILDE:
        emit_indent();
        out("i32.const -1\n");
        gen_expr(n->c0);
        emit_indent();
        out("i32.xor\n");
        last_expr_is_float = 0;
        break;
    case TOK_STAR:
        esz = expr_elem_size(n->c0);
        gen_expr(n->c0);
        if (esz == 8) {
            emit_indent();
            out("f64.load\n");
            last_expr_is_float = 2;
        } else {
            last_expr_is_float = 0;
            emit_indent();
            emit_load(esz);
        }
        break;
    case TOK_AMP:
        error(n->nline, n->ncol, "cannot take address of this expression");
        break;
    }
}

void gen_expr_binary(struct Node *n) {
    int left_float;
    int right_float;
    int op_float;
    /* comma operator: evaluate left, discard, evaluate right */
    if (n->ival == TOK_COMMA) {
        gen_expr(n->c0);
        emit_indent();
        out("drop\n");
        gen_expr(n->c1);
        return;
    }
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
            out("f64.convert_i32_s\n");
        } else if (!left_float && right_float) {
            /* stack: [i32, f64] — need to swap and convert left */
            emit_indent();
            out("local.set $__ftmp\n");
            emit_indent();
            out("f64.convert_i32_s\n");
            emit_indent();
            out("local.get $__ftmp\n");
        }
    }
    if (op_float) {
        emit_indent();
        switch (n->ival) {
        case TOK_PLUS:  out("f64.add\n"); last_expr_is_float = 2; break;
        case TOK_MINUS: out("f64.sub\n"); last_expr_is_float = 2; break;
        case TOK_STAR:  out("f64.mul\n"); last_expr_is_float = 2; break;
        case TOK_SLASH: out("f64.div\n"); last_expr_is_float = 2; break;
        case TOK_EQ_EQ:   out("f64.eq\n"); last_expr_is_float = 0; break;
        case TOK_BANG_EQ: out("f64.ne\n"); last_expr_is_float = 0; break;
        case TOK_LT:    out("f64.lt\n"); last_expr_is_float = 0; break;
        case TOK_GT:    out("f64.gt\n"); last_expr_is_float = 0; break;
        case TOK_LT_EQ: out("f64.le\n"); last_expr_is_float = 0; break;
        case TOK_GT_EQ: out("f64.ge\n"); last_expr_is_float = 0; break;
        default: error(n->nline, n->ncol, "unsupported float binary operator");
        }
    } else {
        emit_indent();
        switch (n->ival) {
        case TOK_PLUS:    out("i32.add\n"); break;
        case TOK_MINUS:   out("i32.sub\n"); break;
        case TOK_STAR:    out("i32.mul\n"); break;
        case TOK_SLASH:   out(expr_is_unsigned(n->c0) ? "i32.div_u\n" : "i32.div_s\n"); break;
        case TOK_PERCENT: out(expr_is_unsigned(n->c0) ? "i32.rem_u\n" : "i32.rem_s\n"); break;
        case TOK_EQ_EQ:   out("i32.eq\n"); break;
        case TOK_BANG_EQ: out("i32.ne\n"); break;
        case TOK_LT:      out(expr_is_unsigned(n->c0) ? "i32.lt_u\n" : "i32.lt_s\n"); break;
        case TOK_GT:      out(expr_is_unsigned(n->c0) ? "i32.gt_u\n" : "i32.gt_s\n"); break;
        case TOK_LT_EQ:   out(expr_is_unsigned(n->c0) ? "i32.le_u\n" : "i32.le_s\n"); break;
        case TOK_GT_EQ:   out(expr_is_unsigned(n->c0) ? "i32.ge_u\n" : "i32.ge_s\n"); break;
        case TOK_AMP_AMP:
        case TOK_AMP:     out("i32.and\n"); break;
        case TOK_PIPE_PIPE:
        case TOK_PIPE:    out("i32.or\n"); break;
        case TOK_LSHIFT:  out("i32.shl\n"); break;
        case TOK_RSHIFT:  out(expr_is_unsigned(n->c0) ? "i32.shr_u\n" : "i32.shr_s\n"); break;
        case TOK_CARET:   out("i32.xor\n"); break;
        default: error(n->nline, n->ncol, "unsupported binary operator");
        }
        last_expr_is_float = 0;
    }
}

void gen_expr_call(struct Node *n) {
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
                    gen_expr(n->list[ai]);
                    ai++;
                    emit_indent();
                    out("call $__print_int\n");
                } else if (fmt[fi] == 's') {
                    if (ai >= n->ival2) error(n->nline, n->ncol, "printf: missing arg for %s");
                    gen_expr(n->list[ai]);
                    ai++;
                    emit_indent();
                    out("call $__print_str\n");
                } else if (fmt[fi] == 'c') {
                    if (ai >= n->ival2) error(n->nline, n->ncol, "printf: missing arg for %c");
                    gen_expr(n->list[ai]);
                    ai++;
                    emit_indent();
                    out("call $putchar\n");
                    emit_indent();
                    out("drop\n");
                } else if (fmt[fi] == 'x') {
                    if (ai >= n->ival2) error(n->nline, n->ncol, "printf: missing arg for %x");
                    gen_expr(n->list[ai]);
                    ai++;
                    emit_indent();
                    out("call $__print_hex\n");
                } else if (fmt[fi] == 'f') {
                    if (ai >= n->ival2) error(n->nline, n->ncol, "printf: missing arg for %f");
                    gen_expr(n->list[ai]);
                    if (!last_expr_is_float) {
                        emit_indent();
                        out("f64.convert_i32_s\n");
                    }
                    ai++;
                    emit_indent();
                    out("call $__print_float\n");
                } else if (fmt[fi] == '%') {
                    emit_indent();
                    out("i32.const 37\n");
                    emit_indent();
                    out("call $putchar\n");
                    emit_indent();
                    out("drop\n");
                } else {
                    error(n->nline, n->ncol, "unsupported printf format");
                }
            } else {
                emit_indent();
                out("i32.const "); out_d(fmt[fi] & 255); out("\n");
                emit_indent();
                out("call $putchar\n");
                emit_indent();
                out("drop\n");
            }
        }
        emit_indent();
        out("i32.const 0\n");
        last_expr_is_float = 0;
    } else if (strcmp(n->sval, "putchar") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $putchar\n");
    } else if (strcmp(n->sval, "getchar") == 0) {
        emit_indent();
        out("call $getchar\n");
    } else if (strcmp(n->sval, "exit") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $__proc_exit\n");
        emit_indent();
        out("i32.const 0\n");
    } else if (strcmp(n->sval, "malloc") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $malloc\n");
    } else if (strcmp(n->sval, "free") == 0) {
        if (n->ival2 > 0) {
            gen_expr(n->list[0]);
        } else {
            emit_indent();
            out("i32.const 0\n");
        }
        emit_indent();
        out("call $free\n");
        emit_indent();
        out("i32.const 0\n");
    } else if (strcmp(n->sval, "strlen") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $strlen\n");
    } else if (strcmp(n->sval, "strcmp") == 0) {
        gen_expr(n->list[0]);
        gen_expr(n->list[1]);
        emit_indent();
        out("call $strcmp\n");
    } else if (strcmp(n->sval, "strncpy") == 0) {
        gen_expr(n->list[0]);
        gen_expr(n->list[1]);
        gen_expr(n->list[2]);
        emit_indent();
        out("call $strncpy\n");
    } else if (strcmp(n->sval, "memcpy") == 0) {
        gen_expr(n->list[0]);
        gen_expr(n->list[1]);
        gen_expr(n->list[2]);
        emit_indent();
        out("call $memcpy\n");
    } else if (strcmp(n->sval, "memset") == 0) {
        gen_expr(n->list[0]);
        gen_expr(n->list[1]);
        gen_expr(n->list[2]);
        emit_indent();
        out("call $memset\n");
    } else if (strcmp(n->sval, "memcmp") == 0) {
        gen_expr(n->list[0]);
        gen_expr(n->list[1]);
        gen_expr(n->list[2]);
        emit_indent();
        out("call $memcmp\n");
    /* --- new libc builtins --- */
    } else if (strcmp(n->sval, "isdigit") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $isdigit\n");
    } else if (strcmp(n->sval, "isalpha") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $isalpha\n");
    } else if (strcmp(n->sval, "isalnum") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $isalnum\n");
    } else if (strcmp(n->sval, "isspace") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $isspace\n");
    } else if (strcmp(n->sval, "isupper") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $isupper\n");
    } else if (strcmp(n->sval, "islower") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $islower\n");
    } else if (strcmp(n->sval, "isprint") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $isprint\n");
    } else if (strcmp(n->sval, "ispunct") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $ispunct\n");
    } else if (strcmp(n->sval, "isxdigit") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $isxdigit\n");
    } else if (strcmp(n->sval, "toupper") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $toupper\n");
    } else if (strcmp(n->sval, "tolower") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $tolower\n");
    } else if (strcmp(n->sval, "abs") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $abs\n");
    } else if (strcmp(n->sval, "atoi") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $atoi\n");
    } else if (strcmp(n->sval, "puts") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $puts\n");
    } else if (strcmp(n->sval, "srand") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $srand\n");
        emit_indent();
        out("i32.const 0\n");
    } else if (strcmp(n->sval, "rand") == 0) {
        emit_indent();
        out("call $rand\n");
    } else if (strcmp(n->sval, "strcpy") == 0) {
        gen_expr(n->list[0]);
        gen_expr(n->list[1]);
        emit_indent();
        out("call $strcpy\n");
    } else if (strcmp(n->sval, "strcat") == 0) {
        gen_expr(n->list[0]);
        gen_expr(n->list[1]);
        emit_indent();
        out("call $strcat\n");
    } else if (strcmp(n->sval, "strchr") == 0) {
        gen_expr(n->list[0]);
        gen_expr(n->list[1]);
        emit_indent();
        out("call $strchr\n");
    } else if (strcmp(n->sval, "strrchr") == 0) {
        gen_expr(n->list[0]);
        gen_expr(n->list[1]);
        emit_indent();
        out("call $strrchr\n");
    } else if (strcmp(n->sval, "strstr") == 0) {
        gen_expr(n->list[0]);
        gen_expr(n->list[1]);
        emit_indent();
        out("call $strstr\n");
    } else if (strcmp(n->sval, "calloc") == 0) {
        gen_expr(n->list[0]);
        gen_expr(n->list[1]);
        emit_indent();
        out("call $calloc\n");
    } else if (strcmp(n->sval, "strncmp") == 0) {
        gen_expr(n->list[0]);
        gen_expr(n->list[1]);
        gen_expr(n->list[2]);
        emit_indent();
        out("call $strncmp\n");
    } else if (strcmp(n->sval, "strncat") == 0) {
        gen_expr(n->list[0]);
        gen_expr(n->list[1]);
        gen_expr(n->list[2]);
        emit_indent();
        out("call $strncat\n");
    } else if (strcmp(n->sval, "memmove") == 0) {
        gen_expr(n->list[0]);
        gen_expr(n->list[1]);
        gen_expr(n->list[2]);
        emit_indent();
        out("call $memmove\n");
    } else if (strcmp(n->sval, "memchr") == 0) {
        gen_expr(n->list[0]);
        gen_expr(n->list[1]);
        gen_expr(n->list[2]);
        emit_indent();
        out("call $memchr\n");
    } else if (strcmp(n->sval, "strtol") == 0) {
        gen_expr(n->list[0]);
        gen_expr(n->list[1]);
        gen_expr(n->list[2]);
        emit_indent();
        out("call $strtol\n");
    } else if (strcmp(n->sval, "__open_file") == 0) {
        gen_expr(n->list[0]);
        gen_expr(n->list[1]);
        emit_indent();
        out("call $__open_file\n");
    } else if (strcmp(n->sval, "__read_file") == 0) {
        gen_expr(n->list[0]);
        gen_expr(n->list[1]);
        gen_expr(n->list[2]);
        emit_indent();
        out("call $__read_file\n");
    } else if (strcmp(n->sval, "__close_file") == 0) {
        gen_expr(n->list[0]);
        emit_indent();
        out("call $__close_file\n");
        emit_indent();
        out("i32.const 0\n");
    } else {
        for (i = 0; i < n->ival2; i++) {
            gen_expr(n->list[i]);
            /* convert param if needed */
            if (func_param_is_float(n->sval, i) && !last_expr_is_float) {
                emit_indent();
                out("f64.convert_i32_s\n");
            } else if (!func_param_is_float(n->sval, i) && last_expr_is_float) {
                emit_indent();
                out("i32.trunc_f64_s\n");
            }
        }
        emit_indent();
        out("call $"); out(n->sval); out("\n");
        if (func_is_void(n->sval)) {
            emit_indent();
            out("i32.const 0\n");
            last_expr_is_float = 0;
        } else {
            last_expr_is_float = func_ret_is_float(n->sval);
        }
    }
}

void gen_expr_str_lit(struct Node *n) {
    emit_indent();
    out("i32.const "); out_d(str_table[n->ival]->offset); out("\n");
    last_expr_is_float = 0;
}

void gen_expr_call_indirect(struct Node *n) {
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
    out("call_indirect (type $__fntype_"); out_d(ci_np); out("_"); out(n->ival3 ? "void" : "i32"); out(")\n");
    if (n->ival3) {
        /* void function — push dummy i32 */
        emit_indent();
        out("i32.const 0\n");
    }
    last_expr_is_float = 0;
}

void gen_expr_member(struct Node *n) {
    int off;

    off = resolve_field_offset(n->sval);
    if (off < 0) error(n->nline, n->ncol, "unknown struct field");
    gen_expr(n->c0);
    if (off > 0) {
        emit_indent();
        out("i32.const "); out_d(off); out("\n");
        emit_indent();
        out("i32.add\n");
    }
    emit_indent();
    out("i32.load\n");
}

void gen_expr_sizeof(struct Node *n) {
    struct StructDef *sd;
    int sz;
    if (n->ival == 1) {
        sz = 4; /* pointer type */
    } else if (n->c0 != (struct Node *)0) {
        /* sizeof(expr): infer size from variable */
        sz = n->c0->kind == ND_IDENT ? var_elem_size(n->c0->sval) : 4;
    } else if (n->sval != (char *)0 && strcmp(n->sval, "char") == 0) {
        sz = 1;
    } else if (n->sval != (char *)0) {
        sd = find_struct(n->sval);
        sz = sd != (struct StructDef *)0 ? sd->size : 4;
    } else if (n->ival2 > 0) {
        sz = n->ival2;
    } else {
        sz = 4;
    }
    emit_indent();
    out("i32.const "); out_d(sz); out("\n");
}

void gen_expr_subscript(struct Node *n) {
    int esz;

    esz = expr_elem_size(n->c0);
    gen_expr(n->c0);
    gen_expr(n->c1);
    if (esz > 1) {
        emit_indent();
        out("i32.const "); out_d(esz); out("\n");
        emit_indent();
        out("i32.mul\n");
    }
    emit_indent();
    out("i32.add\n");
    emit_indent();
    emit_load(esz);
}

void gen_expr_post_inc_dec(struct Node *n) {
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
            emit_indent(); out("global.get $"); out(pname); out("\n");
            emit_indent(); out("local.set $__atmp\n");
            emit_indent(); out("global.get $"); out(pname); out("\n");
            emit_indent(); out("i32.const 1\n");
            emit_indent();
            out(n->kind == ND_POST_INC ? "i32.add\n" : "i32.sub\n");
            emit_indent(); out("global.set $"); out(pname); out("\n");
            emit_indent(); out("local.get $__atmp\n");
        } else {
            emit_indent(); out("local.get $"); out(pname); out("\n");
            emit_indent(); out("local.get $"); out(pname); out("\n");
            emit_indent(); out("i32.const 1\n");
            emit_indent();
            out(n->kind == ND_POST_INC ? "i32.add\n" : "i32.sub\n");
            emit_indent(); out("local.set $"); out(pname); out("\n");
        }
    } else if (tgt2->kind == ND_UNARY && tgt2->ival == TOK_STAR) {
        /* NOTE: tgt2->c0 evaluated 3x (save old val, store addr, reload).
           Correct only when pointer expression has no side effects. */
        pesz = expr_elem_size(tgt2->c0);
        gen_expr(tgt2->c0);
        emit_indent();
        emit_load(pesz);
        emit_indent(); out("local.set $__atmp\n");
        gen_expr(tgt2->c0);
        gen_expr(tgt2->c0);
        emit_indent();
        emit_load(pesz);
        emit_indent(); out("i32.const 1\n");
        emit_indent();
        out(n->kind == ND_POST_INC ? "i32.add\n" : "i32.sub\n");
        emit_indent();
        emit_store(pesz);
        emit_indent(); out("local.get $__atmp\n");
    } else if (tgt2->kind == ND_SUBSCRIPT) {
        /* NOTE: tgt2->c0 and tgt2->c1 each evaluated 3x (save old val, store addr, reload).
           Correct only when the array and index expressions have no side effects. */
        pesz = expr_elem_size(tgt2->c0);
        gen_expr(tgt2->c0); gen_expr(tgt2->c1);
        if (pesz > 1) { emit_indent(); out("i32.const "); out_d(pesz); out("\n"); emit_indent(); out("i32.mul\n"); }
        emit_indent(); out("i32.add\n");
        emit_indent();
        emit_load(pesz);
        emit_indent(); out("local.set $__atmp\n");
        gen_expr(tgt2->c0); gen_expr(tgt2->c1);
        if (pesz > 1) { emit_indent(); out("i32.const "); out_d(pesz); out("\n"); emit_indent(); out("i32.mul\n"); }
        emit_indent(); out("i32.add\n");
        gen_expr(tgt2->c0); gen_expr(tgt2->c1);
        if (pesz > 1) { emit_indent(); out("i32.const "); out_d(pesz); out("\n"); emit_indent(); out("i32.mul\n"); }
        emit_indent(); out("i32.add\n");
        emit_indent();
        emit_load(pesz);
        emit_indent(); out("i32.const 1\n");
        emit_indent();
        out(n->kind == ND_POST_INC ? "i32.add\n" : "i32.sub\n");
        emit_indent();
        emit_store(pesz);
        emit_indent(); out("local.get $__atmp\n");
    } else if (tgt2->kind == ND_MEMBER) {
        poff = resolve_field_offset(tgt2->sval);
        if (poff < 0) error(tgt2->nline, tgt2->ncol, "unknown struct field");
        gen_expr(tgt2->c0);
        if (poff > 0) { emit_indent(); out("i32.const "); out_d(poff); out("\n"); emit_indent(); out("i32.add\n"); }
        emit_indent(); out("i32.load\n");
        emit_indent(); out("local.set $__atmp\n");
        gen_expr(tgt2->c0);
        if (poff > 0) { emit_indent(); out("i32.const "); out_d(poff); out("\n"); emit_indent(); out("i32.add\n"); }
        gen_expr(tgt2->c0);
        if (poff > 0) { emit_indent(); out("i32.const "); out_d(poff); out("\n"); emit_indent(); out("i32.add\n"); }
        emit_indent(); out("i32.load\n");
        emit_indent(); out("i32.const 1\n");
        emit_indent();
        out(n->kind == ND_POST_INC ? "i32.add\n" : "i32.sub\n");
        emit_indent(); out("i32.store\n");
        emit_indent(); out("local.get $__atmp\n");
    } else {
        error(n->nline, n->ncol, "unsupported post-inc/dec target");
    }
}

void gen_expr_ternary(struct Node *n) {
    gen_expr(n->c0);
    /* both branches produce i32; compiler is uniformly i32 throughout */
    emit_indent();
    out("(if (result i32)\n");
    indent_level++;
    emit_indent();
    out("(then\n");
    indent_level++;
    gen_expr(n->c1);
    indent_level--;
    emit_indent();
    out(")\n");
    emit_indent();
    out("(else\n");
    indent_level++;
    gen_expr(n->c2);
    indent_level--;
    emit_indent();
    out(")\n");
    indent_level--;
    emit_indent();
    out(")\n");
}

void gen_expr(struct Node *n) {
    GenExprFn fn;
    if (n->kind < 0 || n->kind >= ND_COUNT) {
        error(n->nline, n->ncol, "unsupported expression in codegen");
    }
    fn = gen_expr_tbl[n->kind];
    fn(n);
}

/* --- Statement codegen --- */


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

void gen_stmt_return(struct Node *n) {
    if (n->c0 != (struct Node *)0) {
        gen_expr(n->c0);
    }
    emit_indent();
    out("return\n");
}

void gen_stmt_var_decl(struct Node *n) {
    int bsz;
    int vd_is_flt;
    vd_is_flt = n->ival3 >> 4;
    if (n->ival > 0) {
        /* Array: allocate n->ival elements of elem_size bytes */
        bsz = n->ival * n->ival2;
        emit_indent();
        out("i32.const "); out_d(bsz); out("\n");
        emit_indent();
        out("call $malloc\n");
        emit_indent();
        out("local.set $"); out(n->sval); out("\n");
    } else if (n->c0 != (struct Node *)0) {
        gen_expr(n->c0);
        /* type conversion if needed */
        if (vd_is_flt && !last_expr_is_float) {
            emit_indent();
            out("f64.convert_i32_s\n");
        } else if (!vd_is_flt && last_expr_is_float) {
            emit_indent();
            out("i32.trunc_f64_s\n");
        }
        emit_indent();
        out("local.set $"); out(n->sval); out("\n");
    }
}

void gen_stmt_expr_stmt(struct Node *n) {
    gen_expr(n->c0);
    emit_indent();
    out("drop\n");
}

void gen_stmt_if(struct Node *n) {
    gen_expr(n->c0);
    if (last_expr_is_float) {
        /* convert float condition to boolean: f64 != 0.0 */
        emit_indent();
        out("f64.const 0\n");
        emit_indent();
        out("f64.ne\n");
    }
    if (n->c2 != (struct Node *)0) {
        emit_indent();
        out("(if\n");
        indent_level++;
        emit_indent();
        out("(then\n");
        indent_level++;
        gen_body(n->c1);
        indent_level--;
        emit_indent();
        out(")\n");
        emit_indent();
        out("(else\n");
        indent_level++;
        gen_body(n->c2);
        indent_level--;
        emit_indent();
        out(")\n");
        indent_level--;
        emit_indent();
        out(")\n");
    } else {
        emit_indent();
        out("(if\n");
        indent_level++;
        emit_indent();
        out("(then\n");
        indent_level++;
        gen_body(n->c1);
        indent_level--;
        emit_indent();
        out(")\n");
        indent_level--;
        emit_indent();
        out(")\n");
    }
}

void gen_stmt_while(struct Node *n) {
    int lbl;

    lbl = label_cnt;
    label_cnt++;
    brk_lbl[loop_sp] = lbl;
    cont_lbl[loop_sp] = lbl;
    loop_sp++;
    emit_indent();
    out("(block $brk_"); out_d(lbl); out("\n");
    indent_level++;
    emit_indent();
    out("(loop $lp_"); out_d(lbl); out("\n");
    indent_level++;
    gen_expr(n->c0);
    if (last_expr_is_float) {
        emit_indent();
        out("f64.const 0\n");
        emit_indent();
        out("f64.ne\n");
    }
    emit_indent();
    out("i32.eqz\n");
    emit_indent();
    out("br_if $brk_"); out_d(lbl); out("\n");
    emit_indent();
    out("(block $cont_"); out_d(lbl); out("\n");
    indent_level++;
    gen_body(n->c1);
    indent_level--;
    emit_indent();
    out(")\n");
    emit_indent();
    out("br $lp_"); out_d(lbl); out("\n");
    indent_level--;
    emit_indent();
    out(")\n");
    indent_level--;
    emit_indent();
    out(")\n");
    loop_sp--;
}

void gen_stmt_for(struct Node *n) {
    int lbl;

    if (n->c0 != (struct Node *)0) {
        gen_stmt(n->c0);
    }
    lbl = label_cnt;
    label_cnt++;
    brk_lbl[loop_sp] = lbl;
    cont_lbl[loop_sp] = lbl;
    loop_sp++;
    emit_indent();
    out("(block $brk_"); out_d(lbl); out("\n");
    indent_level++;
    emit_indent();
    out("(loop $lp_"); out_d(lbl); out("\n");
    indent_level++;
    if (n->c1 != (struct Node *)0) {
        gen_expr(n->c1);
        if (last_expr_is_float) {
            emit_indent();
            out("f64.const 0\n");
            emit_indent();
            out("f64.ne\n");
        }
        emit_indent();
        out("i32.eqz\n");
        emit_indent();
        out("br_if $brk_"); out_d(lbl); out("\n");
    }
    emit_indent();
    out("(block $cont_"); out_d(lbl); out("\n");
    indent_level++;
    gen_body(n->c3);
    indent_level--;
    emit_indent();
    out(")\n");
    if (n->c2 != (struct Node *)0) {
        gen_expr(n->c2);
        emit_indent();
        out("drop\n");
    }
    emit_indent();
    out("br $lp_"); out_d(lbl); out("\n");
    indent_level--;
    emit_indent();
    out(")\n");
    indent_level--;
    emit_indent();
    out(")\n");
    loop_sp--;
}

void gen_stmt_do_while(struct Node *n) {
    int lbl;

    lbl = label_cnt;
    label_cnt++;
    brk_lbl[loop_sp] = lbl;
    cont_lbl[loop_sp] = lbl;
    loop_sp++;
    emit_indent();
    out("(block $brk_"); out_d(lbl); out("\n");
    indent_level++;
    emit_indent();
    out("(loop $lp_"); out_d(lbl); out("\n");
    indent_level++;
    emit_indent();
    out("(block $cont_"); out_d(lbl); out("\n");
    indent_level++;
    gen_body(n->c0);
    indent_level--;
    emit_indent();
    out(")\n");
    gen_expr(n->c1);
    if (last_expr_is_float) {
        emit_indent();
        out("f64.const 0\n");
        emit_indent();
        out("f64.ne\n");
    }
    emit_indent();
    out("br_if $lp_"); out_d(lbl); out("\n");
    indent_level--;
    emit_indent();
    out(")\n");
    indent_level--;
    emit_indent();
    out(")\n");
    loop_sp--;
}

void gen_stmt_break(struct Node *n) {
    if (loop_sp <= 0) error(n->nline, n->ncol, "break outside loop");
    emit_indent();
    out("br $brk_"); out_d(brk_lbl[loop_sp - 1]); out("\n");
}

void gen_stmt_continue(struct Node *n) {
    if (loop_sp <= 0) error(n->nline, n->ncol, "continue outside loop");
    if (cont_lbl[loop_sp - 1] < 0) error(n->nline, n->ncol, "continue not inside a loop");
    emit_indent();
    out("br $cont_"); out_d(cont_lbl[loop_sp - 1]); out("\n");
}

void gen_stmt_block(struct Node *n) {
    int i;

    for (i = 0; i < n->ival2; i++) {
        gen_stmt(n->list[i]);
    }
}

void gen_stmt_switch(struct Node *n) {
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
    out("local.set $__stmp\n");

    /* outer break block */
    emit_indent();
    out("(block $brk_"); out_d(sw_lbl); out("\n");
    indent_level++;

    /* default target block (outermost) */
    emit_indent();
    out("(block $sw"); out_d(sw_lbl); out("_dflt\n");
    indent_level++;

    /* open case blocks in reverse order: first case = innermost */
    for (k = nc - 1; k >= 0; k--) {
        emit_indent();
        out("(block $sw"); out_d(sw_lbl); out("_c"); out_d(k); out("\n");
        indent_level++;
    }

    /* dispatch: compare and branch for each case */
    for (k = 0; k < nc; k++) {
        emit_indent(); out("local.get $__stmp\n");
        emit_indent(); out("i32.const "); out_d(case_vals[k]); out("\n");
        emit_indent(); out("i32.eq\n");
        emit_indent(); out("br_if $sw"); out_d(sw_lbl); out("_c"); out_d(k); out("\n");
    }
    emit_indent();
    if (has_dflt) {
        out("br $sw"); out_d(sw_lbl); out("_dflt\n");
    } else {
        out("br $brk_"); out_d(sw_lbl); out("\n");
    }

    /* close case blocks in forward order and emit case bodies */
    for (k = 0; k < nc; k++) {
        indent_level--;
        emit_indent(); out(")\n");
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
    emit_indent(); out(")\n");

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
    emit_indent(); out(")\n");

    loop_sp--;
}

void gen_stmt(struct Node *n) {
    GenStmtFn fn;
    if (n->kind < 0 || n->kind >= ND_COUNT) {
        error(n->nline, n->ncol, "unsupported statement in codegen");
    }
    fn = gen_stmt_tbl[n->kind];
    fn(n);
}

void gen_expr_error(struct Node *n) {
    error(n->nline, n->ncol, "unsupported expression in codegen");
}

void gen_stmt_error(struct Node *n) {
    error(n->nline, n->ncol, "unsupported statement in codegen");
}

void init_gen_expr_tbl(void) {
    int i;
    gen_expr_tbl = (GenExprFn *)malloc(ND_COUNT * sizeof(void *));
    for (i = 0; i < ND_COUNT; i++) {
        gen_expr_tbl[i] = gen_expr_error;
    }
    gen_expr_tbl[ND_INT_LIT] = gen_expr_int_lit;
    gen_expr_tbl[ND_FLOAT_LIT] = gen_expr_float_lit;
    gen_expr_tbl[ND_CAST] = gen_expr_cast;
    gen_expr_tbl[ND_IDENT] = gen_expr_ident;
    gen_expr_tbl[ND_ASSIGN] = gen_expr_assign;
    gen_expr_tbl[ND_UNARY] = gen_expr_unary;
    gen_expr_tbl[ND_BINARY] = gen_expr_binary;
    gen_expr_tbl[ND_CALL] = gen_expr_call;
    gen_expr_tbl[ND_STR_LIT] = gen_expr_str_lit;
    gen_expr_tbl[ND_CALL_INDIRECT] = gen_expr_call_indirect;
    gen_expr_tbl[ND_MEMBER] = gen_expr_member;
    gen_expr_tbl[ND_SIZEOF] = gen_expr_sizeof;
    gen_expr_tbl[ND_SUBSCRIPT] = gen_expr_subscript;
    gen_expr_tbl[ND_POST_INC] = gen_expr_post_inc_dec;
    gen_expr_tbl[ND_POST_DEC] = gen_expr_post_inc_dec;
    gen_expr_tbl[ND_TERNARY] = gen_expr_ternary;
}

void init_gen_stmt_tbl(void) {
    int i;
    gen_stmt_tbl = (GenStmtFn *)malloc(ND_COUNT * sizeof(void *));
    for (i = 0; i < ND_COUNT; i++) {
        gen_stmt_tbl[i] = gen_stmt_error;
    }
    gen_stmt_tbl[ND_RETURN] = gen_stmt_return;
    gen_stmt_tbl[ND_VAR_DECL] = gen_stmt_var_decl;
    gen_stmt_tbl[ND_EXPR_STMT] = gen_stmt_expr_stmt;
    gen_stmt_tbl[ND_IF] = gen_stmt_if;
    gen_stmt_tbl[ND_WHILE] = gen_stmt_while;
    gen_stmt_tbl[ND_FOR] = gen_stmt_for;
    gen_stmt_tbl[ND_DO_WHILE] = gen_stmt_do_while;
    gen_stmt_tbl[ND_BREAK] = gen_stmt_break;
    gen_stmt_tbl[ND_CONTINUE] = gen_stmt_continue;
    gen_stmt_tbl[ND_BLOCK] = gen_stmt_block;
    gen_stmt_tbl[ND_SWITCH] = gen_stmt_switch;
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

    out("  (func $"); out(n->sval);
    for (i = 0; i < n->ival2; i++) {
        if (local_vars[i]->lv_is_float) {
            out(" (param $"); out(n->list[i]->sval); out(" f64)");
        } else {
            out(" (param $"); out(n->list[i]->sval); out(" i32)");
        }
    }
    if (n->ival == 1) {
        /* void */
    } else if (ret_float) {
        out(" (result f64)");
    } else {
        out(" (result i32)");
    }
    out("\n");

    indent_level = 2;
    /* emit only non-param locals */
    has_any_float = 0;
    for (i = nparam_locals; i < nlocals; i++) {
        emit_indent();
        if (local_vars[i]->lv_is_float) {
            out("(local $"); out(local_vars[i]->name); out(" f64)\n");
            has_any_float = 1;
        } else {
            out("(local $"); out(local_vars[i]->name); out(" i32)\n");
        }
    }
    /* check params for float too */
    for (i = 0; i < nparam_locals; i++) {
        if (local_vars[i]->lv_is_float) has_any_float = 1;
    }
    emit_indent();
    out("(local $__atmp i32)\n");
    emit_indent();
    out("(local $__stmp i32)\n");
    emit_indent();
    out("(local $__ftmp f64)\n");
    body = n->c0;
    for (i = 0; i < body->ival2; i++) {
        gen_stmt(body->list[i]);
    }
    if (n->ival != 1) {
        if (ret_float) {
            emit_indent();
            out("f64.const 0\n");
        } else {
            emit_indent();
            out("i32.const 0\n");
        }
    }
    indent_level = 1;
    emit_indent();
    out(")\n");
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
            out_c(c);
        } else {
            out_c('\\');
            hi = (c >> 4) & 15;
            lo = c & 15;
            if (hi >= 10) {
                out_c('a' + hi - 10);
            } else {
                out_c('0' + hi);
            }
            if (lo >= 10) {
                out_c('a' + lo - 10);
            } else {
                out_c('0' + lo);
            }
        }
    }
}

/* --- Module codegen --- */

void gen_module(struct Node *prog) {
    int i;
    int gi;

    emit_indent();
    out("(module\n");
    indent_level++;

    /* WASI imports */
    emit_indent();
    out("(import \"wasi_snapshot_preview1\" \"proc_exit\" (func $__proc_exit (param i32)))\n");
    emit_indent();
    out("(import \"wasi_snapshot_preview1\" \"fd_write\" (func $__fd_write (param i32 i32 i32 i32) (result i32)))\n");
    emit_indent();
    out("(import \"wasi_snapshot_preview1\" \"fd_read\" (func $__fd_read (param i32 i32 i32 i32) (result i32)))\n");
    emit_indent();
    out("(import \"wasi_snapshot_preview1\" \"path_open\" (func $__path_open (param i32 i32 i32 i32 i32 i64 i64 i32 i32) (result i32)))\n");
    emit_indent();
    out("(import \"wasi_snapshot_preview1\" \"fd_close\" (func $__fd_close (param i32) (result i32)))\n");
    emit_indent();
    out("\n");

    /* memory */
    emit_indent();
    out("(memory (export \"memory\") 512)\n");
    emit_indent();
    out("\n");

    /* function pointer type declarations and table */
    if (fn_table_count > 0) {
        int fti;
        int need_types[17]; /* need_types[nparams] = bitmask: bit0=i32 result, bit1=void */
        for (fti = 0; fti < 17; fti++) need_types[fti] = 0;
        /* scan fnptr_vars to collect needed type signatures */
        for (fti = 0; fti < nfnptr_vars; fti++) {
            int np;
            int vd;
            np = fnptr_vars[fti]->fp_nparams;
            vd = fnptr_vars[fti]->fp_is_void;
            if (np <= 16) {
                need_types[np] = need_types[np] | (1 << vd);
            }
        }
        /* emit type declarations */
        for (fti = 0; fti <= 16; fti++) {
            if (need_types[fti] & 1) {
                int pi;
                emit_indent();
                out("(type $__fntype_"); out_d(fti); out("_i32 (func");
                for (pi = 0; pi < fti; pi++) out(" (param i32)");
                out(" (result i32)))\n");
            }
            if (need_types[fti] & 2) {
                int pi;
                emit_indent();
                out("(type $__fntype_"); out_d(fti); out("_void (func");
                for (pi = 0; pi < fti; pi++) out(" (param i32)");
                out("))\n");
            }
        }
        /* table and elem */
        emit_indent();
        out("(table "); out_d(fn_table_count); out(" funcref)\n");
        emit_indent();
        out("(elem (i32.const 0)");
        for (fti = 0; fti < fn_table_count; fti++) {
            out(" $"); out(fn_table_names[fti]);
        }
        out(")\n");
        emit_indent();
        out("\n");
    }

    /* static data section */
    for (i = 0; i < nstrings; i++) {
        out("  (data (i32.const "); out_d(str_table[i]->offset); out(") \"");
        emit_wat_string(str_table[i]->data, str_table[i]->len);
        out("\\00\")\n");
    }
    if (nstrings > 0) {
        emit_indent();
        out("\n");
    }

    /* heap pointer */
    emit_indent();
    out("(global $__heap_ptr (mut i32) (i32.const "); out_d(data_ptr); out("))\n");
    emit_indent();
    out("(global $__free_list (mut i32) (i32.const 0))\n");
    emit_indent();
    out("(global $__rand_seed (mut i32) (i32.const 1))\n");

    /* user global variables */
    for (gi = 0; gi < nglobals; gi++) {
        emit_indent();
        if (globals_tbl[gi]->gv_is_float) {
            if (globals_tbl[gi]->gv_float_init != (char *)0) {
                out("(global $"); out(globals_tbl[gi]->name); out(" (mut f64) (f64.const "); out(globals_tbl[gi]->gv_float_init); out("))\n");
            } else {
                out("(global $"); out(globals_tbl[gi]->name); out(" (mut f64) (f64.const "); out_d(globals_tbl[gi]->init_val); out("))\n");
            }
        } else {
            out("(global $"); out(globals_tbl[gi]->name); out(" (mut i32) (i32.const "); out_d(globals_tbl[gi]->init_val); out("))\n");
        }
    }
    emit_indent();
    out("\n");

    /* Runtime helper functions */
    emit_indent();
    out("(func $putchar (param $ch i32) (result i32)\n");
    emit_indent();
    out("  (i32.store8 (i32.const 0) (local.get $ch))\n");
    emit_indent();
    out("  (i32.store (i32.const 4) (i32.const 0))\n");
    emit_indent();
    out("  (i32.store (i32.const 8) (i32.const 1))\n");
    emit_indent();
    out("  (drop (call $__fd_write (i32.const 1) (i32.const 4) (i32.const 1) (i32.const 12)))\n");
    emit_indent();
    out("  (local.get $ch)\n");
    emit_indent();
    out(")\n");

    emit_indent();
    out("(func $getchar (result i32)\n");
    emit_indent();
    out("  (i32.store (i32.const 4) (i32.const 0))\n");
    emit_indent();
    out("  (i32.store (i32.const 8) (i32.const 1))\n");
    emit_indent();
    out("  (drop (call $__fd_read (i32.const 0) (i32.const 4) (i32.const 1) (i32.const 12)))\n");
    emit_indent();
    out("  (if (result i32) (i32.eqz (i32.load (i32.const 12)))\n");
    emit_indent();
    out("    (then (i32.const -1))\n");
    emit_indent();
    out("    (else (i32.load8_u (i32.const 0)))\n");
    emit_indent();
    out("  )\n");
    emit_indent();
    out(")\n");

    emit_indent();
    out("(func $__print_int (param $n i32)\n");
    emit_indent();
    out("  (local $buf i32)\n");
    emit_indent();
    out("  (local $len i32)\n");
    emit_indent();
    out("  (if (i32.lt_s (local.get $n) (i32.const 0))\n");
    emit_indent();
    out("    (then\n");
    emit_indent();
    out("      (drop (call $putchar (i32.const 45)))\n");
    emit_indent();
    out("      (local.set $n (i32.sub (i32.const 0) (local.get $n)))\n");
    emit_indent();
    out("    )\n");
    emit_indent();
    out("  )\n");
    emit_indent();
    out("  (if (i32.eqz (local.get $n))\n");
    emit_indent();
    out("    (then (drop (call $putchar (i32.const 48))) (return))\n");
    emit_indent();
    out("  )\n");
    emit_indent();
    out("  (local.set $buf (i32.const 48))\n");
    emit_indent();
    out("  (local.set $len (i32.const 0))\n");
    emit_indent();
    out("  (block $done (loop $digit\n");
    emit_indent();
    out("    (br_if $done (i32.eqz (local.get $n)))\n");
    emit_indent();
    out("    (local.set $buf (i32.sub (local.get $buf) (i32.const 1)))\n");
    emit_indent();
    out("    (i32.store8 (local.get $buf) (i32.add (i32.const 48) (i32.rem_u (local.get $n) (i32.const 10))))\n");
    emit_indent();
    out("    (local.set $n (i32.div_u (local.get $n) (i32.const 10)))\n");
    emit_indent();
    out("    (local.set $len (i32.add (local.get $len) (i32.const 1)))\n");
    emit_indent();
    out("    (br $digit)\n");
    emit_indent();
    out("  ))\n");
    emit_indent();
    out("  (block $pd (loop $pl\n");
    emit_indent();
    out("    (br_if $pd (i32.eqz (local.get $len)))\n");
    emit_indent();
    out("    (drop (call $putchar (i32.load8_u (local.get $buf))))\n");
    emit_indent();
    out("    (local.set $buf (i32.add (local.get $buf) (i32.const 1)))\n");
    emit_indent();
    out("    (local.set $len (i32.sub (local.get $len) (i32.const 1)))\n");
    emit_indent();
    out("    (br $pl)\n");
    emit_indent();
    out("  ))\n");
    emit_indent();
    out(")\n");

    emit_indent();
    out("(func $__print_str (param $ptr i32)\n");
    emit_indent();
    out("  (local $ch i32)\n");
    emit_indent();
    out("  (block $done (loop $next\n");
    emit_indent();
    out("    (local.set $ch (i32.load8_u (local.get $ptr)))\n");
    emit_indent();
    out("    (br_if $done (i32.eqz (local.get $ch)))\n");
    emit_indent();
    out("    (drop (call $putchar (local.get $ch)))\n");
    emit_indent();
    out("    (local.set $ptr (i32.add (local.get $ptr) (i32.const 1)))\n");
    emit_indent();
    out("    (br $next)\n");
    emit_indent();
    out("  ))\n");
    emit_indent();
    out(")\n");

    emit_indent();
    out("(func $__print_hex (param $n i32)\n");
    emit_indent();
    out("  (local $buf i32)\n");
    emit_indent();
    out("  (local $len i32)\n");
    emit_indent();
    out("  (local $d i32)\n");
    emit_indent();
    out("  (if (i32.eqz (local.get $n))\n");
    emit_indent();
    out("    (then (drop (call $putchar (i32.const 48))) (return))\n");
    emit_indent();
    out("  )\n");
    emit_indent();
    out("  (local.set $buf (i32.const 48))\n");
    emit_indent();
    out("  (local.set $len (i32.const 0))\n");
    emit_indent();
    out("  (block $done (loop $digit\n");
    emit_indent();
    out("    (br_if $done (i32.eqz (local.get $n)))\n");
    emit_indent();
    out("    (local.set $buf (i32.sub (local.get $buf) (i32.const 1)))\n");
    emit_indent();
    out("    (local.set $d (i32.and (local.get $n) (i32.const 15)))\n");
    emit_indent();
    out("    (if (i32.lt_u (local.get $d) (i32.const 10))\n");
    emit_indent();
    out("      (then (i32.store8 (local.get $buf) (i32.add (i32.const 48) (local.get $d))))\n");
    emit_indent();
    out("      (else (i32.store8 (local.get $buf) (i32.add (i32.const 87) (local.get $d))))\n");
    emit_indent();
    out("    )\n");
    emit_indent();
    out("    (local.set $n (i32.shr_u (local.get $n) (i32.const 4)))\n");
    emit_indent();
    out("    (local.set $len (i32.add (local.get $len) (i32.const 1)))\n");
    emit_indent();
    out("    (br $digit)\n");
    emit_indent();
    out("  ))\n");
    emit_indent();
    out("  (block $pd (loop $pl\n");
    emit_indent();
    out("    (br_if $pd (i32.eqz (local.get $len)))\n");
    emit_indent();
    out("    (drop (call $putchar (i32.load8_u (local.get $buf))))\n");
    emit_indent();
    out("    (local.set $buf (i32.add (local.get $buf) (i32.const 1)))\n");
    emit_indent();
    out("    (local.set $len (i32.sub (local.get $len) (i32.const 1)))\n");
    emit_indent();
    out("    (br $pl)\n");
    emit_indent();
    out("  ))\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* __print_float: prints f64 with 6 decimal places */
    emit_indent();
    out("(func $__print_float (param $v f64)\n");
    emit_indent();
    out("  (local $int_part i32)\n");
    emit_indent();
    out("  (local $frac_part i32)\n");
    emit_indent();
    out("  (local $frac f64)\n");
    emit_indent();
    out("  (local $i i32)\n");
    emit_indent();
    out("  ;; handle negative\n");
    emit_indent();
    out("  (if (f64.lt (local.get $v) (f64.const 0))\n");
    emit_indent();
    out("    (then\n");
    emit_indent();
    out("      (drop (call $putchar (i32.const 45)))\n");
    emit_indent();
    out("      (local.set $v (f64.neg (local.get $v)))\n");
    emit_indent();
    out("    )\n");
    emit_indent();
    out("  )\n");
    emit_indent();
    out("  ;; integer part\n");
    emit_indent();
    out("  (local.set $int_part (i32.trunc_f64_s (local.get $v)))\n");
    emit_indent();
    out("  (call $__print_int (local.get $int_part))\n");
    emit_indent();
    out("  ;; dot\n");
    emit_indent();
    out("  (drop (call $putchar (i32.const 46)))\n");
    emit_indent();
    out("  ;; fractional part: 6 digits\n");
    emit_indent();
    out("  (local.set $frac (f64.sub (local.get $v) (f64.convert_i32_s (local.get $int_part))))\n");
    emit_indent();
    out("  (local.set $frac (f64.mul (local.get $frac) (f64.const 1000000)))\n");
    emit_indent();
    out("  (local.set $frac (f64.add (local.get $frac) (f64.const 0.5)))\n");
    emit_indent();
    out("  (local.set $frac_part (i32.trunc_f64_s (local.get $frac)))\n");
    emit_indent();
    out("  ;; print with leading zeros (6 digits)\n");
    emit_indent();
    out("  (local.set $i (i32.const 100000))\n");
    emit_indent();
    out("  (block $done (loop $lp\n");
    emit_indent();
    out("    (br_if $done (i32.eqz (local.get $i)))\n");
    emit_indent();
    out("    (drop (call $putchar (i32.add (i32.const 48) (i32.div_u (local.get $frac_part) (local.get $i)))))\n");
    emit_indent();
    out("    (local.set $frac_part (i32.rem_u (local.get $frac_part) (local.get $i)))\n");
    emit_indent();
    out("    (local.set $i (i32.div_u (local.get $i) (i32.const 10)))\n");
    emit_indent();
    out("    (br $lp)\n");
    emit_indent();
    out("  ))\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    emit_indent();
    out("(func $malloc (param $size i32) (result i32)\n");
    emit_indent();
    out("  (local $total i32)\n");
    emit_indent();
    out("  (local $cur i32)\n");
    emit_indent();
    out("  (local $prev i32)\n");
    emit_indent();
    out("  (local $rest i32)\n");
    emit_indent();
    out("  (local $ptr i32)\n");
    emit_indent();
    out("  (local.set $total\n");
    emit_indent();
    out("    (i32.and (i32.add (local.get $size) (i32.const 15)) (i32.const -8)))\n");
    emit_indent();
    out("  (local.set $prev (i32.const 0))\n");
    emit_indent();
    out("  (local.set $cur (global.get $__free_list))\n");
    emit_indent();
    out("  (block $done\n");
    emit_indent();
    out("    (loop $search\n");
    emit_indent();
    out("      (br_if $done (i32.eqz (local.get $cur)))\n");
    emit_indent();
    out("      (if (i32.ge_u (i32.load (local.get $cur)) (local.get $total))\n");
    emit_indent();
    out("        (then\n");
    emit_indent();
    out("          (if (i32.ge_u (i32.load (local.get $cur)) (i32.add (local.get $total) (i32.const 16)))\n");
    emit_indent();
    out("            (then\n");
    emit_indent();
    out("              (local.set $rest (i32.add (local.get $cur) (local.get $total)))\n");
    emit_indent();
    out("              (i32.store (local.get $rest)\n");
    emit_indent();
    out("                (i32.sub (i32.load (local.get $cur)) (local.get $total)))\n");
    emit_indent();
    out("              (i32.store offset=4 (local.get $rest)\n");
    emit_indent();
    out("                (i32.load offset=4 (local.get $cur)))\n");
    emit_indent();
    out("              (i32.store (local.get $cur) (local.get $total))\n");
    emit_indent();
    out("              (if (local.get $prev)\n");
    emit_indent();
    out("                (then\n");
    emit_indent();
    out("                  (i32.store offset=4 (local.get $prev) (local.get $rest)))\n");
    emit_indent();
    out("                (else\n");
    emit_indent();
    out("                  (global.set $__free_list (local.get $rest))))\n");
    emit_indent();
    out("            )\n");
    emit_indent();
    out("            (else\n");
    emit_indent();
    out("              (if (local.get $prev)\n");
    emit_indent();
    out("                (then\n");
    emit_indent();
    out("                  (i32.store offset=4 (local.get $prev)\n");
    emit_indent();
    out("                    (i32.load offset=4 (local.get $cur))))\n");
    emit_indent();
    out("                (else\n");
    emit_indent();
    out("                  (global.set $__free_list\n");
    emit_indent();
    out("                    (i32.load offset=4 (local.get $cur)))))\n");
    emit_indent();
    out("          ))\n");
    emit_indent();
    out("          (return (i32.add (local.get $cur) (i32.const 8)))\n");
    emit_indent();
    out("        )\n");
    emit_indent();
    out("      )\n");
    emit_indent();
    out("      (local.set $prev (local.get $cur))\n");
    emit_indent();
    out("      (local.set $cur (i32.load offset=4 (local.get $cur)))\n");
    emit_indent();
    out("      (br $search)\n");
    emit_indent();
    out("    )\n");
    emit_indent();
    out("  )\n");
    emit_indent();
    out("  (local.set $ptr (global.get $__heap_ptr))\n");
    emit_indent();
    out("  (i32.store (local.get $ptr) (local.get $total))\n");
    emit_indent();
    out("  (global.set $__heap_ptr (i32.add (local.get $ptr) (local.get $total)))\n");
    emit_indent();
    out("  (i32.add (local.get $ptr) (i32.const 8))\n");
    emit_indent();
    out(")\n");

    emit_indent();
    out("(func $free (param $ptr i32)\n");
    emit_indent();
    out("  (local $block i32)\n");
    emit_indent();
    out("  (local $cur i32)\n");
    emit_indent();
    out("  (local $prev i32)\n");
    emit_indent();
    out("  (local $next_blk i32)\n");
    emit_indent();
    out("  (if (i32.eqz (local.get $ptr)) (then (return)))\n");
    emit_indent();
    out("  (local.set $block (i32.sub (local.get $ptr) (i32.const 8)))\n");
    emit_indent();
    out("  (local.set $prev (i32.const 0))\n");
    emit_indent();
    out("  (local.set $cur (global.get $__free_list))\n");
    emit_indent();
    out("  (block $found\n");
    emit_indent();
    out("    (loop $scan\n");
    emit_indent();
    out("      (br_if $found (i32.eqz (local.get $cur)))\n");
    emit_indent();
    out("      (br_if $found (i32.gt_u (local.get $cur) (local.get $block)))\n");
    emit_indent();
    out("      (local.set $prev (local.get $cur))\n");
    emit_indent();
    out("      (local.set $cur (i32.load offset=4 (local.get $cur)))\n");
    emit_indent();
    out("      (br $scan)\n");
    emit_indent();
    out("    )\n");
    emit_indent();
    out("  )\n");
    emit_indent();
    out("  (i32.store offset=4 (local.get $block) (local.get $cur))\n");
    emit_indent();
    out("  (if (local.get $prev)\n");
    emit_indent();
    out("    (then (i32.store offset=4 (local.get $prev) (local.get $block)))\n");
    emit_indent();
    out("    (else (global.set $__free_list (local.get $block)))\n");
    emit_indent();
    out("  )\n");
    emit_indent();
    out("  (if (i32.and\n");
    emit_indent();
    out("        (i32.ne (local.get $cur) (i32.const 0))\n");
    emit_indent();
    out("        (i32.eq (i32.add (local.get $block) (i32.load (local.get $block)))\n");
    emit_indent();
    out("                (local.get $cur)))\n");
    emit_indent();
    out("    (then\n");
    emit_indent();
    out("      (i32.store (local.get $block)\n");
    emit_indent();
    out("        (i32.add (i32.load (local.get $block)) (i32.load (local.get $cur))))\n");
    emit_indent();
    out("      (i32.store offset=4 (local.get $block)\n");
    emit_indent();
    out("        (i32.load offset=4 (local.get $cur)))\n");
    emit_indent();
    out("    )\n");
    emit_indent();
    out("  )\n");
    emit_indent();
    out("  (if (i32.and\n");
    emit_indent();
    out("        (i32.ne (local.get $prev) (i32.const 0))\n");
    emit_indent();
    out("        (i32.eq (i32.add (local.get $prev) (i32.load (local.get $prev)))\n");
    emit_indent();
    out("                (local.get $block)))\n");
    emit_indent();
    out("    (then\n");
    emit_indent();
    out("      (i32.store (local.get $prev)\n");
    emit_indent();
    out("        (i32.add (i32.load (local.get $prev)) (i32.load (local.get $block))))\n");
    emit_indent();
    out("      (i32.store offset=4 (local.get $prev)\n");
    emit_indent();
    out("        (i32.load offset=4 (local.get $block)))\n");
    emit_indent();
    out("    )\n");
    emit_indent();
    out("  )\n");
    emit_indent();
    out(")\n");

    emit_indent();
    out("(func $strlen (param $s i32) (result i32)\n");
    emit_indent();
    out("  (local $n i32)\n");
    emit_indent();
    out("  (local.set $n (i32.const 0))\n");
    emit_indent();
    out("  (block $done (loop $next\n");
    emit_indent();
    out("    (br_if $done (i32.eqz (i32.load8_u (i32.add (local.get $s) (local.get $n)))))\n");
    emit_indent();
    out("    (local.set $n (i32.add (local.get $n) (i32.const 1)))\n");
    emit_indent();
    out("    (br $next)\n");
    emit_indent();
    out("  ))\n");
    emit_indent();
    out("  (local.get $n)\n");
    emit_indent();
    out(")\n");

    emit_indent();
    out("(func $strcmp (param $a i32) (param $b i32) (result i32)\n");
    emit_indent();
    out("  (local $ca i32) (local $cb i32)\n");
    emit_indent();
    out("  (block $done (loop $next\n");
    emit_indent();
    out("    (local.set $ca (i32.load8_u (local.get $a)))\n");
    emit_indent();
    out("    (local.set $cb (i32.load8_u (local.get $b)))\n");
    emit_indent();
    out("    (br_if $done (i32.ne (local.get $ca) (local.get $cb)))\n");
    emit_indent();
    out("    (br_if $done (i32.eqz (local.get $ca)))\n");
    emit_indent();
    out("    (local.set $a (i32.add (local.get $a) (i32.const 1)))\n");
    emit_indent();
    out("    (local.set $b (i32.add (local.get $b) (i32.const 1)))\n");
    emit_indent();
    out("    (br $next)\n");
    emit_indent();
    out("  ))\n");
    emit_indent();
    out("  (i32.sub (local.get $ca) (local.get $cb))\n");
    emit_indent();
    out(")\n");

    emit_indent();
    out("(func $strncpy (param $dst i32) (param $src i32) (param $n i32) (result i32)\n");
    emit_indent();
    out("  (local $i i32)\n");
    emit_indent();
    out("  (local.set $i (i32.const 0))\n");
    emit_indent();
    out("  (block $done (loop $next\n");
    emit_indent();
    out("    (br_if $done (i32.ge_u (local.get $i) (local.get $n)))\n");
    emit_indent();
    out("    (i32.store8 (i32.add (local.get $dst) (local.get $i))\n");
    emit_indent();
    out("      (i32.load8_u (i32.add (local.get $src) (local.get $i))))\n");
    emit_indent();
    out("    (br_if $done (i32.eqz (i32.load8_u (i32.add (local.get $src) (local.get $i)))))\n");
    emit_indent();
    out("    (local.set $i (i32.add (local.get $i) (i32.const 1)))\n");
    emit_indent();
    out("    (br $next)\n");
    emit_indent();
    out("  ))\n");
    emit_indent();
    out("  (local.get $dst)\n");
    emit_indent();
    out(")\n");

    emit_indent();
    out("(func $memcpy (param $dst i32) (param $src i32) (param $n i32) (result i32)\n");
    emit_indent();
    out("  (local $i i32)\n");
    emit_indent();
    out("  (local.set $i (i32.const 0))\n");
    emit_indent();
    out("  (block $done (loop $next\n");
    emit_indent();
    out("    (br_if $done (i32.ge_u (local.get $i) (local.get $n)))\n");
    emit_indent();
    out("    (i32.store8 (i32.add (local.get $dst) (local.get $i))\n");
    emit_indent();
    out("      (i32.load8_u (i32.add (local.get $src) (local.get $i))))\n");
    emit_indent();
    out("    (local.set $i (i32.add (local.get $i) (i32.const 1)))\n");
    emit_indent();
    out("    (br $next)\n");
    emit_indent();
    out("  ))\n");
    emit_indent();
    out("  (local.get $dst)\n");
    emit_indent();
    out(")\n");

    emit_indent();
    out("(func $memset (param $dst i32) (param $val i32) (param $n i32) (result i32)\n");
    emit_indent();
    out("  (local $i i32)\n");
    emit_indent();
    out("  (local.set $i (i32.const 0))\n");
    emit_indent();
    out("  (block $done (loop $next\n");
    emit_indent();
    out("    (br_if $done (i32.ge_u (local.get $i) (local.get $n)))\n");
    emit_indent();
    out("    (i32.store8 (i32.add (local.get $dst) (local.get $i)) (local.get $val))\n");
    emit_indent();
    out("    (local.set $i (i32.add (local.get $i) (i32.const 1)))\n");
    emit_indent();
    out("    (br $next)\n");
    emit_indent();
    out("  ))\n");
    emit_indent();
    out("  (local.get $dst)\n");
    emit_indent();
    out(")\n");

    emit_indent();
    out("(func $memcmp (param $a i32) (param $b i32) (param $n i32) (result i32)\n");
    emit_indent();
    out("  (local $i i32) (local $ca i32) (local $cb i32)\n");
    emit_indent();
    out("  (local.set $i (i32.const 0))\n");
    emit_indent();
    out("  (block $done (loop $next\n");
    emit_indent();
    out("    (br_if $done (i32.ge_u (local.get $i) (local.get $n)))\n");
    emit_indent();
    out("    (local.set $ca (i32.load8_u (i32.add (local.get $a) (local.get $i))))\n");
    emit_indent();
    out("    (local.set $cb (i32.load8_u (i32.add (local.get $b) (local.get $i))))\n");
    emit_indent();
    out("    (if (i32.ne (local.get $ca) (local.get $cb))\n");
    emit_indent();
    out("      (then (return (i32.sub (local.get $ca) (local.get $cb))))\n");
    emit_indent();
    out("    )\n");
    emit_indent();
    out("    (local.set $i (i32.add (local.get $i) (i32.const 1)))\n");
    emit_indent();
    out("    (br $next)\n");
    emit_indent();
    out("  ))\n");
    emit_indent();
    out("  (i32.const 0)\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* --- new libc helper functions --- */

    /* isdigit */
    emit_indent();
    out("(func $isdigit (param $c i32) (result i32)\n");
    emit_indent();
    out("  (i32.and\n");
    emit_indent();
    out("    (i32.ge_u (local.get $c) (i32.const 48))\n");
    emit_indent();
    out("    (i32.le_u (local.get $c) (i32.const 57)))\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* isalpha */
    emit_indent();
    out("(func $isalpha (param $c i32) (result i32)\n");
    emit_indent();
    out("  (i32.or\n");
    emit_indent();
    out("    (i32.and (i32.ge_u (local.get $c) (i32.const 65)) (i32.le_u (local.get $c) (i32.const 90)))\n");
    emit_indent();
    out("    (i32.and (i32.ge_u (local.get $c) (i32.const 97)) (i32.le_u (local.get $c) (i32.const 122))))\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* isalnum */
    emit_indent();
    out("(func $isalnum (param $c i32) (result i32)\n");
    emit_indent();
    out("  (i32.or (call $isdigit (local.get $c)) (call $isalpha (local.get $c)))\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* isspace */
    emit_indent();
    out("(func $isspace (param $c i32) (result i32)\n");
    emit_indent();
    out("  (i32.or\n");
    emit_indent();
    out("    (i32.or\n");
    emit_indent();
    out("      (i32.eq (local.get $c) (i32.const 32))\n");
    emit_indent();
    out("      (i32.eq (local.get $c) (i32.const 9)))\n");
    emit_indent();
    out("    (i32.or\n");
    emit_indent();
    out("      (i32.eq (local.get $c) (i32.const 10))\n");
    emit_indent();
    out("      (i32.or\n");
    emit_indent();
    out("        (i32.eq (local.get $c) (i32.const 13))\n");
    emit_indent();
    out("        (i32.eq (local.get $c) (i32.const 12)))))\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* isupper */
    emit_indent();
    out("(func $isupper (param $c i32) (result i32)\n");
    emit_indent();
    out("  (i32.and (i32.ge_u (local.get $c) (i32.const 65)) (i32.le_u (local.get $c) (i32.const 90)))\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* islower */
    emit_indent();
    out("(func $islower (param $c i32) (result i32)\n");
    emit_indent();
    out("  (i32.and (i32.ge_u (local.get $c) (i32.const 97)) (i32.le_u (local.get $c) (i32.const 122)))\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* isprint */
    emit_indent();
    out("(func $isprint (param $c i32) (result i32)\n");
    emit_indent();
    out("  (i32.and (i32.ge_u (local.get $c) (i32.const 32)) (i32.le_u (local.get $c) (i32.const 126)))\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* ispunct */
    emit_indent();
    out("(func $ispunct (param $c i32) (result i32)\n");
    emit_indent();
    out("  (i32.and (call $isprint (local.get $c))\n");
    emit_indent();
    out("    (i32.and (i32.eqz (call $isalnum (local.get $c)))\n");
    emit_indent();
    out("             (i32.ne (local.get $c) (i32.const 32))))\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* isxdigit */
    emit_indent();
    out("(func $isxdigit (param $c i32) (result i32)\n");
    emit_indent();
    out("  (i32.or (call $isdigit (local.get $c))\n");
    emit_indent();
    out("    (i32.or\n");
    emit_indent();
    out("      (i32.and (i32.ge_u (local.get $c) (i32.const 65)) (i32.le_u (local.get $c) (i32.const 70)))\n");
    emit_indent();
    out("      (i32.and (i32.ge_u (local.get $c) (i32.const 97)) (i32.le_u (local.get $c) (i32.const 102)))))\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* toupper */
    emit_indent();
    out("(func $toupper (param $c i32) (result i32)\n");
    emit_indent();
    out("  (if (result i32) (call $islower (local.get $c))\n");
    emit_indent();
    out("    (then (i32.sub (local.get $c) (i32.const 32)))\n");
    emit_indent();
    out("    (else (local.get $c)))\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* tolower */
    emit_indent();
    out("(func $tolower (param $c i32) (result i32)\n");
    emit_indent();
    out("  (if (result i32) (call $isupper (local.get $c))\n");
    emit_indent();
    out("    (then (i32.add (local.get $c) (i32.const 32)))\n");
    emit_indent();
    out("    (else (local.get $c)))\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* abs */
    emit_indent();
    out("(func $abs (param $n i32) (result i32)\n");
    emit_indent();
    out("  (if (result i32) (i32.lt_s (local.get $n) (i32.const 0))\n");
    emit_indent();
    out("    (then (i32.sub (i32.const 0) (local.get $n)))\n");
    emit_indent();
    out("    (else (local.get $n)))\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* atoi */
    emit_indent();
    out("(func $atoi (param $s i32) (result i32)\n");
    emit_indent();
    out("  (local $n i32) (local $neg i32) (local $c i32)\n");
    emit_indent();
    out("  (local.set $n (i32.const 0))\n");
    emit_indent();
    out("  (local.set $neg (i32.const 0))\n");
    emit_indent();
    out("  (block $dsp (loop $sp\n");
    emit_indent();
    out("    (local.set $c (i32.load8_u (local.get $s)))\n");
    emit_indent();
    out("    (br_if $dsp (i32.ne (local.get $c) (i32.const 32)))\n");
    emit_indent();
    out("    (local.set $s (i32.add (local.get $s) (i32.const 1)))\n");
    emit_indent();
    out("    (br $sp)))\n");
    emit_indent();
    out("  (local.set $c (i32.load8_u (local.get $s)))\n");
    emit_indent();
    out("  (if (i32.eq (local.get $c) (i32.const 45))\n");
    emit_indent();
    out("    (then (local.set $neg (i32.const 1))\n");
    emit_indent();
    out("           (local.set $s (i32.add (local.get $s) (i32.const 1)))))\n");
    emit_indent();
    out("  (if (i32.eq (local.get $c) (i32.const 43))\n");
    emit_indent();
    out("    (then (local.set $s (i32.add (local.get $s) (i32.const 1)))))\n");
    emit_indent();
    out("  (block $done (loop $dig\n");
    emit_indent();
    out("    (local.set $c (i32.load8_u (local.get $s)))\n");
    emit_indent();
    out("    (br_if $done (i32.or (i32.lt_u (local.get $c) (i32.const 48)) (i32.gt_u (local.get $c) (i32.const 57))))\n");
    emit_indent();
    out("    (local.set $n (i32.add (i32.mul (local.get $n) (i32.const 10)) (i32.sub (local.get $c) (i32.const 48))))\n");
    emit_indent();
    out("    (local.set $s (i32.add (local.get $s) (i32.const 1)))\n");
    emit_indent();
    out("    (br $dig)))\n");
    emit_indent();
    out("  (if (result i32) (local.get $neg) (then (i32.sub (i32.const 0) (local.get $n))) (else (local.get $n)))\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* srand */
    emit_indent();
    out("(func $srand (param $seed i32)\n");
    emit_indent();
    out("  (global.set $__rand_seed (local.get $seed))\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* rand */
    emit_indent();
    out("(func $rand (result i32)\n");
    emit_indent();
    out("  (global.set $__rand_seed\n");
    emit_indent();
    out("    (i32.add (i32.mul (global.get $__rand_seed) (i32.const 1103515245)) (i32.const 12345)))\n");
    emit_indent();
    out("  (i32.and (i32.shr_u (global.get $__rand_seed) (i32.const 16)) (i32.const 32767))\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* calloc */
    emit_indent();
    out("(func $calloc (param $nmemb i32) (param $size i32) (result i32)\n");
    emit_indent();
    out("  (local $ptr i32) (local $total i32)\n");
    emit_indent();
    out("  (local.set $total (i32.mul (local.get $nmemb) (local.get $size)))\n");
    emit_indent();
    out("  (local.set $ptr (call $malloc (local.get $total)))\n");
    emit_indent();
    out("  (drop (call $memset (local.get $ptr) (i32.const 0) (local.get $total)))\n");
    emit_indent();
    out("  (local.get $ptr)\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* strcpy */
    emit_indent();
    out("(func $strcpy (param $dst i32) (param $src i32) (result i32)\n");
    emit_indent();
    out("  (local $d i32)\n");
    emit_indent();
    out("  (local.set $d (local.get $dst))\n");
    emit_indent();
    out("  (block $done (loop $copy\n");
    emit_indent();
    out("    (i32.store8 (local.get $d) (i32.load8_u (local.get $src)))\n");
    emit_indent();
    out("    (br_if $done (i32.eqz (i32.load8_u (local.get $src))))\n");
    emit_indent();
    out("    (local.set $d (i32.add (local.get $d) (i32.const 1)))\n");
    emit_indent();
    out("    (local.set $src (i32.add (local.get $src) (i32.const 1)))\n");
    emit_indent();
    out("    (br $copy)))\n");
    emit_indent();
    out("  (local.get $dst)\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* strcat */
    emit_indent();
    out("(func $strcat (param $dst i32) (param $src i32) (result i32)\n");
    emit_indent();
    out("  (local $d i32)\n");
    emit_indent();
    out("  (local.set $d (i32.add (local.get $dst) (call $strlen (local.get $dst))))\n");
    emit_indent();
    out("  (drop (call $strcpy (local.get $d) (local.get $src)))\n");
    emit_indent();
    out("  (local.get $dst)\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* strchr */
    emit_indent();
    out("(func $strchr (param $s i32) (param $c i32) (result i32)\n");
    emit_indent();
    out("  (local $ch i32)\n");
    emit_indent();
    out("  (block $done (loop $scan\n");
    emit_indent();
    out("    (local.set $ch (i32.load8_u (local.get $s)))\n");
    emit_indent();
    out("    (if (i32.eq (local.get $ch) (local.get $c)) (then (return (local.get $s))))\n");
    emit_indent();
    out("    (br_if $done (i32.eqz (local.get $ch)))\n");
    emit_indent();
    out("    (local.set $s (i32.add (local.get $s) (i32.const 1)))\n");
    emit_indent();
    out("    (br $scan)))\n");
    emit_indent();
    out("  (i32.const 0)\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* strrchr */
    emit_indent();
    out("(func $strrchr (param $s i32) (param $c i32) (result i32)\n");
    emit_indent();
    out("  (local $last i32) (local $ch i32)\n");
    emit_indent();
    out("  (local.set $last (i32.const 0))\n");
    emit_indent();
    out("  (block $done (loop $scan\n");
    emit_indent();
    out("    (local.set $ch (i32.load8_u (local.get $s)))\n");
    emit_indent();
    out("    (if (i32.eq (local.get $ch) (local.get $c)) (then (local.set $last (local.get $s))))\n");
    emit_indent();
    out("    (br_if $done (i32.eqz (local.get $ch)))\n");
    emit_indent();
    out("    (local.set $s (i32.add (local.get $s) (i32.const 1)))\n");
    emit_indent();
    out("    (br $scan)))\n");
    emit_indent();
    out("  (local.get $last)\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* strstr */
    emit_indent();
    out("(func $strstr (param $hay i32) (param $needle i32) (result i32)\n");
    emit_indent();
    out("  (local $h i32) (local $n i32)\n");
    emit_indent();
    out("  (if (i32.eqz (i32.load8_u (local.get $needle))) (then (return (local.get $hay))))\n");
    emit_indent();
    out("  (block $notfound (loop $outer\n");
    emit_indent();
    out("    (br_if $notfound (i32.eqz (i32.load8_u (local.get $hay))))\n");
    emit_indent();
    out("    (local.set $h (local.get $hay))\n");
    emit_indent();
    out("    (local.set $n (local.get $needle))\n");
    emit_indent();
    out("    (block $nomatch (loop $inner\n");
    emit_indent();
    out("      (if (i32.eqz (i32.load8_u (local.get $n))) (then (return (local.get $hay))))\n");
    emit_indent();
    out("      (br_if $nomatch (i32.ne (i32.load8_u (local.get $h)) (i32.load8_u (local.get $n))))\n");
    emit_indent();
    out("      (local.set $h (i32.add (local.get $h) (i32.const 1)))\n");
    emit_indent();
    out("      (local.set $n (i32.add (local.get $n) (i32.const 1)))\n");
    emit_indent();
    out("      (br $inner)))\n");
    emit_indent();
    out("    (local.set $hay (i32.add (local.get $hay) (i32.const 1)))\n");
    emit_indent();
    out("    (br $outer)))\n");
    emit_indent();
    out("  (i32.const 0)\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* strncmp */
    emit_indent();
    out("(func $strncmp (param $a i32) (param $b i32) (param $n i32) (result i32)\n");
    emit_indent();
    out("  (local $i i32) (local $ca i32) (local $cb i32)\n");
    emit_indent();
    out("  (local.set $i (i32.const 0))\n");
    emit_indent();
    out("  (block $done (loop $cmp\n");
    emit_indent();
    out("    (br_if $done (i32.ge_u (local.get $i) (local.get $n)))\n");
    emit_indent();
    out("    (local.set $ca (i32.load8_u (local.get $a)))\n");
    emit_indent();
    out("    (local.set $cb (i32.load8_u (local.get $b)))\n");
    emit_indent();
    out("    (if (i32.ne (local.get $ca) (local.get $cb))\n");
    emit_indent();
    out("      (then (return (i32.sub (local.get $ca) (local.get $cb)))))\n");
    emit_indent();
    out("    (br_if $done (i32.eqz (local.get $ca)))\n");
    emit_indent();
    out("    (local.set $a (i32.add (local.get $a) (i32.const 1)))\n");
    emit_indent();
    out("    (local.set $b (i32.add (local.get $b) (i32.const 1)))\n");
    emit_indent();
    out("    (local.set $i (i32.add (local.get $i) (i32.const 1)))\n");
    emit_indent();
    out("    (br $cmp)))\n");
    emit_indent();
    out("  (i32.const 0)\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* strncat */
    emit_indent();
    out("(func $strncat (param $dst i32) (param $src i32) (param $n i32) (result i32)\n");
    emit_indent();
    out("  (local $d i32) (local $i i32)\n");
    emit_indent();
    out("  (local.set $d (i32.add (local.get $dst) (call $strlen (local.get $dst))))\n");
    emit_indent();
    out("  (local.set $i (i32.const 0))\n");
    emit_indent();
    out("  (block $done (loop $cp\n");
    emit_indent();
    out("    (br_if $done (i32.ge_u (local.get $i) (local.get $n)))\n");
    emit_indent();
    out("    (br_if $done (i32.eqz (i32.load8_u (local.get $src))))\n");
    emit_indent();
    out("    (i32.store8 (local.get $d) (i32.load8_u (local.get $src)))\n");
    emit_indent();
    out("    (local.set $d (i32.add (local.get $d) (i32.const 1)))\n");
    emit_indent();
    out("    (local.set $src (i32.add (local.get $src) (i32.const 1)))\n");
    emit_indent();
    out("    (local.set $i (i32.add (local.get $i) (i32.const 1)))\n");
    emit_indent();
    out("    (br $cp)))\n");
    emit_indent();
    out("  (i32.store8 (local.get $d) (i32.const 0))\n");
    emit_indent();
    out("  (local.get $dst)\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* memmove */
    emit_indent();
    out("(func $memmove (param $dst i32) (param $src i32) (param $n i32) (result i32)\n");
    emit_indent();
    out("  (local $i i32)\n");
    emit_indent();
    out("  (if (i32.le_u (local.get $dst) (local.get $src))\n");
    emit_indent();
    out("    (then (drop (call $memcpy (local.get $dst) (local.get $src) (local.get $n))))\n");
    emit_indent();
    out("    (else\n");
    emit_indent();
    out("      (local.set $i (local.get $n))\n");
    emit_indent();
    out("      (block $done (loop $bk\n");
    emit_indent();
    out("        (br_if $done (i32.eqz (local.get $i)))\n");
    emit_indent();
    out("        (local.set $i (i32.sub (local.get $i) (i32.const 1)))\n");
    emit_indent();
    out("        (i32.store8 (i32.add (local.get $dst) (local.get $i))\n");
    emit_indent();
    out("                    (i32.load8_u (i32.add (local.get $src) (local.get $i))))\n");
    emit_indent();
    out("        (br $bk)))))\n");
    emit_indent();
    out("  (local.get $dst)\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* memchr */
    emit_indent();
    out("(func $memchr (param $s i32) (param $c i32) (param $n i32) (result i32)\n");
    emit_indent();
    out("  (local $i i32)\n");
    emit_indent();
    out("  (local.set $i (i32.const 0))\n");
    emit_indent();
    out("  (block $done (loop $scan\n");
    emit_indent();
    out("    (br_if $done (i32.ge_u (local.get $i) (local.get $n)))\n");
    emit_indent();
    out("    (if (i32.eq (i32.load8_u (i32.add (local.get $s) (local.get $i))) (local.get $c))\n");
    emit_indent();
    out("      (then (return (i32.add (local.get $s) (local.get $i)))))\n");
    emit_indent();
    out("    (local.set $i (i32.add (local.get $i) (i32.const 1)))\n");
    emit_indent();
    out("    (br $scan)))\n");
    emit_indent();
    out("  (i32.const 0)\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* puts */
    emit_indent();
    out("(func $puts (param $s i32) (result i32)\n");
    emit_indent();
    out("  (local $c i32)\n");
    emit_indent();
    out("  (block $done (loop $pr\n");
    emit_indent();
    out("    (local.set $c (i32.load8_u (local.get $s)))\n");
    emit_indent();
    out("    (br_if $done (i32.eqz (local.get $c)))\n");
    emit_indent();
    out("    (drop (call $putchar (local.get $c)))\n");
    emit_indent();
    out("    (local.set $s (i32.add (local.get $s) (i32.const 1)))\n");
    emit_indent();
    out("    (br $pr)))\n");
    emit_indent();
    out("  (drop (call $putchar (i32.const 10)))\n");
    emit_indent();
    out("  (i32.const 0)\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* strtol (simplified: base 10 only, ignores endptr) */
    emit_indent();
    out("(func $strtol (param $s i32) (param $endptr i32) (param $base i32) (result i32)\n");
    emit_indent();
    out("  (call $atoi (local.get $s))\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* file I/O helpers for #include support */
    emit_indent();
    out("(func $__open_file (param $path i32) (param $path_len i32) (result i32)\n");
    emit_indent();
    out("  (if (result i32) (i32.eqz (call $__path_open\n");
    emit_indent();
    out("    (i32.const 3)\n");
    emit_indent();
    out("    (i32.const 0)\n");
    emit_indent();
    out("    (local.get $path)\n");
    emit_indent();
    out("    (local.get $path_len)\n");
    emit_indent();
    out("    (i32.const 0)\n");
    emit_indent();
    out("    (i64.const 2)\n");
    emit_indent();
    out("    (i64.const 0)\n");
    emit_indent();
    out("    (i32.const 0)\n");
    emit_indent();
    out("    (i32.const 28)\n");
    emit_indent();
    out("  ))\n");
    emit_indent();
    out("    (then (i32.load (i32.const 28)))\n");
    emit_indent();
    out("    (else (i32.const -1))\n");
    emit_indent();
    out("  )\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    emit_indent();
    out("(func $__read_file (param $fd i32) (param $buf i32) (param $max_len i32) (result i32)\n");
    emit_indent();
    out("  (i32.store (i32.const 16) (local.get $buf))\n");
    emit_indent();
    out("  (i32.store (i32.const 20) (local.get $max_len))\n");
    emit_indent();
    out("  (if (result i32) (i32.eqz (call $__fd_read (local.get $fd) (i32.const 16) (i32.const 1) (i32.const 24)))\n");
    emit_indent();
    out("    (then (i32.load (i32.const 24)))\n");
    emit_indent();
    out("    (else (i32.const -1))\n");
    emit_indent();
    out("  )\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    emit_indent();
    out("(func $__close_file (param $fd i32)\n");
    emit_indent();
    out("  (drop (call $__fd_close (local.get $fd)))\n");
    emit_indent();
    out(")\n");
    emit_indent();
    out("\n");

    /* user functions */
    for (i = 0; i < prog->ival2; i++) {
        gen_func(prog->list[i]);
    }

    emit_indent();
    out("\n");

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
            out("(func $__init\n");
            indent_level++;
            emit_indent();
            out("(local $__ptr i32)\n");
            for (gi2 = 0; gi2 < nglobals; gi2++) {
                if (globals_tbl[gi2]->gv_arr_len > 0 && globals_tbl[gi2]->gv_arr_str_ids != (int *)0) {
                    int ai2;
                    /* allocate arr_len * 4 bytes */
                    emit_indent();
                    out("i32.const "); out_d(globals_tbl[gi2]->gv_arr_len * 4); out("\n");
                    emit_indent();
                    out("call $malloc\n");
                    emit_indent();
                    out("local.tee $__ptr\n");
                    emit_indent();
                    out("global.set $"); out(globals_tbl[gi2]->name); out("\n");
                    /* store each element */
                    for (ai2 = 0; ai2 < globals_tbl[gi2]->gv_arr_len; ai2++) {
                        emit_indent();
                        out("local.get $__ptr\n");
                        if (ai2 > 0) {
                            emit_indent();
                            out("i32.const "); out_d(ai2 * 4); out("\n");
                            emit_indent();
                            out("i32.add\n");
                        }
                        emit_indent();
                        out("i32.const "); out_d(str_table[globals_tbl[gi2]->gv_arr_str_ids[ai2]]->offset); out("\n");
                        emit_indent();
                        out("i32.store\n");
                    }
                }
            }
            indent_level--;
            emit_indent();
            out(")\n");
        }
        emit_indent();
        out("(func $_start (export \"_start\")\n");
        indent_level++;
        if (need_init) {
            emit_indent();
            out("call $__init\n");
        }
        emit_indent();
        out("call $main\n");
        emit_indent();
        out("call $__proc_exit\n");
        indent_level--;
        emit_indent();
        out(")\n");
    }

    indent_level--;
    emit_indent();
    out(")\n");
}

