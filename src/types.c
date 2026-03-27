/* types.c — Type registry implementations */

/* ================================================================
 * Struct Type Registry
 * ================================================================ */

struct StructDef **structs_reg;
int nstructs;

void init_structs(void) {
    structs_reg = (struct StructDef **)malloc(MAX_STRUCTS * sizeof(void *));
    nstructs = 0;
}

struct StructDef *find_struct(char *name) {
    int i;
    for (i = 0; i < nstructs; i++) {
        if (strcmp(structs_reg[i]->name, name) == 0) return structs_reg[i];
    }
    return (struct StructDef *)0;
}

int resolve_field_offset(char *field_name) {
    int i;
    int j;
    for (i = 0; i < nstructs; i++) {
        for (j = 0; j < structs_reg[i]->nfields; j++) {
            if (strcmp(structs_reg[i]->fields[j]->name, field_name) == 0) {
                return structs_reg[i]->fields[j]->fld_offset;
            }
        }
    }
    return -1;
}

/* ================================================================
 * Global Variable Table
 * ================================================================ */

struct GlobalVar **globals_tbl;
int nglobals;

void init_globals(void) {
    globals_tbl = (struct GlobalVar **)malloc(MAX_GLOBALS * sizeof(void *));
    nglobals = 0;
}

int find_global(char *name) {
    int i;
    for (i = 0; i < nglobals; i++) {
        if (strcmp(globals_tbl[i]->name, name) == 0) return i;
    }
    return -1;
}

/* ================================================================
 * Function Signature Table
 * ================================================================ */

struct FuncSig **func_sigs;
int nfunc_sigs;

void init_func_sigs(void) {
    func_sigs = (struct FuncSig **)malloc(MAX_FUNC_SIGS * sizeof(void *));
    nfunc_sigs = 0;
}

int func_is_void(char *name) {
    int i;
    for (i = 0; i < nfunc_sigs; i++) {
        if (strcmp(func_sigs[i]->name, name) == 0) return func_sigs[i]->is_void;
    }
    return 0;
}

int func_ret_is_float(char *name) {
    int i;
    for (i = 0; i < nfunc_sigs; i++) {
        if (strcmp(func_sigs[i]->name, name) == 0) return func_sigs[i]->ret_is_float;
    }
    return 0;
}

int func_param_is_float(char *name, int idx) {
    int i;
    for (i = 0; i < nfunc_sigs; i++) {
        if (strcmp(func_sigs[i]->name, name) == 0) {
            if (idx < func_sigs[i]->nparam) return func_sigs[i]->param_is_float[idx];
            return 0;
        }
    }
    return 0;
}

int func_is_variadic(char *name) {
    int i;
    for (i = 0; i < nfunc_sigs; i++) {
        if (strcmp(func_sigs[i]->name, name) == 0) return func_sigs[i]->is_variadic;
    }
    return 0;
}

/* ================================================================
 * Function Pointer Registry
 * ================================================================ */

struct FnPtrVar **fnptr_vars;
int nfnptr_vars;

char **fn_table_names;
int fn_table_count;

void init_fnptr_registry(void) {
    fnptr_vars = (struct FnPtrVar **)malloc(MAX_FNPTR_VARS * sizeof(void *));
    nfnptr_vars = 0;
    fn_table_names = (char **)malloc(MAX_FN_TABLE * sizeof(void *));
    fn_table_count = 0;
}

void register_fnptr_var(char *name, int nparams, int is_void) {
    if (nfnptr_vars >= MAX_FNPTR_VARS) return;
    fnptr_vars[nfnptr_vars] = (struct FnPtrVar *)malloc(sizeof(struct FnPtrVar));
    fnptr_vars[nfnptr_vars]->name = strdupn(name, 127);
    fnptr_vars[nfnptr_vars]->fp_nparams = nparams;
    fnptr_vars[nfnptr_vars]->fp_is_void = is_void;
    nfnptr_vars++;
}

struct FnPtrVar *find_fnptr_var(char *name) {
    int i;
    for (i = nfnptr_vars - 1; i >= 0; i--) {
        if (strcmp(fnptr_vars[i]->name, name) == 0) return fnptr_vars[i];
    }
    return (struct FnPtrVar *)0;
}

void fn_table_add(char *name) {
    int i;
    for (i = 0; i < fn_table_count; i++) {
        if (strcmp(fn_table_names[i], name) == 0) return;
    }
    if (fn_table_count < MAX_FN_TABLE) {
        fn_table_names[fn_table_count] = name;
        fn_table_count++;
    }
}

int fn_table_idx(char *name) {
    int i;
    for (i = 0; i < fn_table_count; i++) {
        if (strcmp(fn_table_names[i], name) == 0) return i;
    }
    return -1;
}

int is_known_func(char *name) {
    int i;
    for (i = 0; i < nfunc_sigs; i++) {
        if (strcmp(func_sigs[i]->name, name) == 0) return 1;
    }
    return 0;
}

/* ================================================================
 * Enum Constant Table
 * ================================================================ */

struct EnumConst **enum_consts;
int nenum_consts;

void init_enum_consts(void) {
    enum_consts = (struct EnumConst **)malloc(MAX_ENUM_CONSTS * sizeof(void *));
    nenum_consts = 0;
}

int find_enum_const(char *name) {
    int i;
    for (i = 0; i < nenum_consts; i++) {
        if (strcmp(enum_consts[i]->name, name) == 0) return i;
    }
    return -1;
}

struct TypeAlias **type_aliases;
int ntype_aliases;
int last_type_is_ptr;
int last_type_is_unsigned;
int last_type_elem_size;  /* 1=char, 2=short, 4=int/long */
int last_type_is_float;   /* 0=not float, 1=float(f32), 2=double(f64) */
int last_type_is_fnptr;   /* 1 if resolved from fnptr typedef */
int last_type_fnptr_nparams;
int last_type_fnptr_is_void;

void init_type_aliases(void) {
    type_aliases = (struct TypeAlias **)malloc(MAX_TYPE_ALIASES * sizeof(void *));
    ntype_aliases = 0;
    last_type_is_ptr = 0;
    last_type_is_float = 0;
    /* register va_list as built-in alias for int */
    type_aliases[0] = (struct TypeAlias *)malloc(sizeof(struct TypeAlias));
    type_aliases[0]->alias = strdupn("va_list", 7);
    type_aliases[0]->resolved_kind = 0;
    type_aliases[0]->is_ptr = 0;
    type_aliases[0]->is_fnptr = 0;
    type_aliases[0]->fnptr_nparams = 0;
    type_aliases[0]->fnptr_is_void = 0;
    ntype_aliases = 1;
}

int find_type_alias(char *name) {
    int i;
    for (i = 0; i < ntype_aliases; i++) {
        if (strcmp(type_aliases[i]->alias, name) == 0) return i;
    }
    return -1;
}
