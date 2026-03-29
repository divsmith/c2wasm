/* dce.c — Dead code elimination: BFS reachability from main */

/* dce_live[i] = 1 if prog->list[i] is reachable */
int *dce_live;
int dce_nfuncs;
struct Node **dce_funcs;

int dce_find(char *name) {
    int i;
    int first;
    if (name == (char *)0) return -1;
    first = -1;
    for (i = 0; i < dce_nfuncs; i++) {
        if (strcmp(dce_funcs[i]->sval, name) == 0) {
            if (first < 0) first = i;
            if (dce_funcs[i]->c0 != (struct Node *)0) return i;
        }
    }
    return first;
}

void dce_scan(struct Node *n);

void dce_mark(char *name) {
    int idx;
    idx = dce_find(name);
    if (idx < 0) return;
    if (dce_live[idx]) return;
    dce_live[idx] = 1;
    dce_scan(dce_funcs[idx]->c0);
}

void dce_scan(struct Node *n) {
    int i;
    if (n == (struct Node *)0) return;
    switch (n->kind) {
    case ND_CALL:
        dce_mark(n->sval);
        for (i = 0; i < n->ival2; i++) dce_scan(n->list[i]);
        return;
    case ND_CALL_INDIRECT:
        dce_scan(n->c0);
        for (i = 0; i < n->ival2; i++) dce_scan(n->list[i]);
        return;
    case ND_BLOCK:
        for (i = 0; i < n->ival2; i++) dce_scan(n->list[i]);
        return;
    case ND_SWITCH:
        dce_scan(n->c0);
        for (i = 0; i < n->ival2; i++) dce_scan(n->list[i]);
        return;
    case ND_IF:
        dce_scan(n->c0); dce_scan(n->c1); dce_scan(n->c2);
        return;
    case ND_WHILE:
        dce_scan(n->c0); dce_scan(n->c1);
        return;
    case ND_FOR:
        dce_scan(n->c0); dce_scan(n->c1); dce_scan(n->c2); dce_scan(n->c3);
        return;
    case ND_DO_WHILE:
        dce_scan(n->c0); dce_scan(n->c1);
        return;
    case ND_BINARY:
    case ND_ASSIGN:
        dce_scan(n->c0); dce_scan(n->c1);
        return;
    case ND_UNARY:
    case ND_RETURN:
    case ND_EXPR_STMT:
    case ND_CAST:
    case ND_SIZEOF:
    case ND_VA_ARG:
        dce_scan(n->c0);
        return;
    case ND_VAR_DECL:
        dce_scan(n->c0); /* optional initializer */
        return;
    case ND_POST_INC:
    case ND_POST_DEC:
        dce_scan(n->c0);
        return;
    case ND_SUBSCRIPT:
    case ND_MEMBER:
        dce_scan(n->c0); dce_scan(n->c1);
        return;
    case ND_TERNARY:
        dce_scan(n->c0); dce_scan(n->c1); dce_scan(n->c2);
        return;
    default:
        /* leaf nodes: ND_IDENT, ND_INT_LIT, ND_FLOAT_LIT, ND_STR_LIT,
           ND_BREAK, ND_CONTINUE, ND_GOTO, ND_LABEL, ND_CASE, ND_DEFAULT,
           ND_CALL_INDIRECT (indirect calls — handled via fn_table) */
        return;
    }
}

void dce_run(struct Node *prog) {
    int i;
    dce_nfuncs = prog->ival2;
    dce_funcs = prog->list;
    dce_live = (int *)malloc(dce_nfuncs * sizeof(int));
    for (i = 0; i < dce_nfuncs; i++) {
        dce_live[i] = 0;
    }
    /* Always keep main */
    dce_mark("main");
    /* Always keep __init if present */
    dce_mark("__init");
    /* Keep any function whose address is taken (fn_table entries) */
    for (i = 0; i < fn_table_count; i++) {
        dce_mark(fn_table_names[i]);
    }
}

int dce_is_live(char *name) {
    int idx;
    idx = dce_find(name);
    if (idx < 0) return 1; /* unknown: keep it */
    return dce_live[idx];
}
