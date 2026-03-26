/* ast.h — AST node and list types */

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

/* Growable list helper */
struct NList {
    struct Node **items;
    int count;
    int cap;
};

struct Node *node_new(int kind, int line, int col);
void nlist_push(struct NList *l, struct Node *n);
