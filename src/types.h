/* types.h — Type registry struct definitions */

struct StructField {
    char *name;
    int fld_offset;
};

struct StructDef {
    char *name;
    struct StructField **fields;
    int nfields;
    int size;
};

struct GlobalVar {
    char *name;
    int init_val;
    int gv_elem_size;
    int gv_is_unsigned;
    int gv_is_float;
    char *gv_float_init;
    int gv_arr_len;
    int *gv_arr_str_ids;
};

struct FuncSig {
    char *name;
    int is_void;
    int ret_is_float;
    int nparam;
    int *param_is_float;
};

struct FnPtrVar {
    char *name;
    int fp_nparams;
    int fp_is_void;
};

#define MAX_FNPTR_VARS 512
#define MAX_FN_TABLE   512

struct EnumConst {
    char *name;
    int val;
};

struct TypeAlias {
    char *alias;
    int resolved_kind;
    int is_ptr;
    int is_fnptr;
    int fnptr_nparams;
    int fnptr_is_void;
};

/* Type registry functions */
void init_structs(void);
struct StructDef *find_struct(char *name);
int resolve_field_offset(char *field_name);
void init_globals(void);
int find_global(char *name);
void init_func_sigs(void);
int func_is_void(char *name);
int func_ret_is_float(char *name);
int func_param_is_float(char *name, int idx);
void init_fnptr_registry(void);
void register_fnptr_var(char *name, int nparams, int is_void);
struct FnPtrVar *find_fnptr_var(char *name);
void fn_table_add(char *name);
int fn_table_idx(char *name);
int is_known_func(char *name);
void init_enum_consts(void);
int find_enum_const(char *name);
void init_type_aliases(void);
int find_type_alias(char *name);
