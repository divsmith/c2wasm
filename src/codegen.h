/* codegen.h — Shared codegen declarations */

struct LocalVar {
    char *name;
    int lv_elem_size;     /* 1=char, 2=short, 4=int */
    int lv_is_unsigned;
    int lv_is_float;      /* 0=int, 1=float(f32), 2=double(f64) */
};

/* Shared codegen functions */
void init_local_tracking(void);
int find_local(char *name);
void add_local(char *name, int elem_size, int is_unsigned, int is_float);
void collect_locals(struct Node *n);
void init_loop_labels(void);
int var_elem_size(char *name);
int var_is_unsigned(char *name);
int expr_elem_size(struct Node *n);
int expr_is_unsigned(struct Node *n);
int var_is_float(char *name);
