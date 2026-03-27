/* codegen.h — Shared codegen declarations */

struct LocalVar {
    char *name;
    int lv_elem_size;     /* 1=char, 2=short, 4=int */
    int lv_is_unsigned;
    int lv_is_float;      /* 0=int, 1=float(f32), 2=double(f64) */
    int lv_arr_dim2;      /* inner dimension for 2D arrays, 0 for 1D */
};

struct StaticLocalMap {
    char *local_name;
    char *global_name;
};

/* Shared codegen functions */
void init_local_tracking(void);
int find_local(char *name);
void add_local(char *name, int elem_size, int is_unsigned, int is_float);
void add_local_dim2(char *name, int dim2);
void collect_locals(struct Node *n);
void init_loop_labels(void);
void init_static_map(void);
void init_goto_labels(void);
char *find_static_global(char *name);
int var_elem_size(char *name);
int var_is_unsigned(char *name);
int var_arr_dim2(char *name);
int expr_elem_size(struct Node *n);
int expr_is_unsigned(struct Node *n);
int var_is_float(char *name);
