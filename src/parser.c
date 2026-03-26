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
                c->ival = fpv->fp_nparams;
                c->ival3 = fpv->fp_is_void;
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

