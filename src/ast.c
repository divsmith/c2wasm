/* ast.c — AST node constructors and helpers */

struct Node *node_new(int kind, int line, int col) {
    struct Node *n;
    n = (struct Node *)malloc(sizeof(struct Node));
    memset(n, 0, sizeof(struct Node));
    n->kind = kind;
    n->nline = line;
    n->ncol = col;
    return n;
}

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
