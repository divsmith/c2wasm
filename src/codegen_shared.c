/* codegen_shared.c — Local variable tracking, loop labels, type helpers */

/* --- Local variable tracking --- */

struct LocalVar **local_vars;
int nlocals;

/* --- Static local variable mapping --- */
struct StaticLocalMap **static_map;
int nstatic_map;
char *codegen_func_name;

void init_local_tracking(void) {
    local_vars = (struct LocalVar **)malloc(MAX_LOCALS * sizeof(void *));
    nlocals = 0;
}

void init_static_map(void) {
    static_map = (struct StaticLocalMap **)malloc(MAX_STATIC_LOCALS * sizeof(void *));
    nstatic_map = 0;
    codegen_func_name = (char *)0;
}

int find_local(char *name) {
    int i;
    for (i = 0; i < nlocals; i++) {
        if (strcmp(local_vars[i]->name, name) == 0) return i;
    }
    return -1;
}

char *find_static_global(char *name) {
    int i;
    for (i = 0; i < nstatic_map; i++) {
        if (strcmp(static_map[i]->local_name, name) == 0) return static_map[i]->global_name;
    }
    return (char *)0;
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
    local_vars[nlocals]->lv_arr_dim2 = 0;
    nlocals++;
}

void add_local_dim2(char *name, int dim2) {
    int li;
    li = find_local(name);
    if (li >= 0) {
        local_vars[li]->lv_arr_dim2 = dim2;
    }
}

void collect_locals(struct Node *n) {
    int i;
    if (n == (struct Node *)0) return;
    switch (n->kind) {
    case ND_VAR_DECL:
        if (n->ival3 & 0x100) {
            /* static local: add to static_map, not to locals */
            if (nstatic_map < MAX_STATIC_LOCALS) {
                char *gn;
                gn = mangle_static(codegen_func_name, n->sval);
                static_map[nstatic_map] = (struct StaticLocalMap *)malloc(sizeof(struct StaticLocalMap));
                static_map[nstatic_map]->local_name = n->sval;
                static_map[nstatic_map]->global_name = gn;
                nstatic_map++;
            }
        } else if (n->ival3 & 0x200) {
            /* extern local: skip entirely */
        } else {
            add_local(n->sval, n->ival2, n->ival3 & 0xF, (n->ival3 >> 4) & 0xF);
            if ((n->ival3 >> 16) & 0xFFFF) {
                add_local_dim2(n->sval, (n->ival3 >> 16) & 0xFFFF);
            }
        }
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
    char *sg;
    sg = find_static_global(name);
    if (sg != (char *)0) {
        gi = find_global(sg);
        if (gi >= 0) return globals_tbl[gi]->gv_elem_size;
        return 4;
    }
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
    char *sg;
    sg = find_static_global(name);
    if (sg != (char *)0) {
        gi = find_global(sg);
        if (gi >= 0) return globals_tbl[gi]->gv_is_unsigned;
        return 0;
    }
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

int var_arr_dim2(char *name) {
    int li;
    li = find_local(name);
    if (li >= 0) {
        return local_vars[li]->lv_arr_dim2;
    }
    /* TODO: also check globals for 2D global arrays */
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
    char *sg;
    sg = find_static_global(name);
    if (sg != (char *)0) {
        gi = find_global(sg);
        if (gi >= 0) return globals_tbl[gi]->gv_is_float;
        return 0;
    }
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
