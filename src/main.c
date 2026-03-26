/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    struct Node *prog;

    init_kw_table();
    init_macros();
    init_includes();
    init_strings();
    init_structs();
    init_globals();
    init_func_sigs();
    init_fnptr_registry();
    init_enum_consts();
    init_type_aliases();
    init_local_tracking();
    init_loop_labels();
    init_gen_expr_tbl();
    init_gen_stmt_tbl();
    init_gen_expr_bin_tbl();
    init_gen_stmt_bin_tbl();

    read_source();
    lex_init();
    advance_tok();

    prog = parse_program();
    if (binary_mode) {
        wat_output = bv_new(65536);
        indent_level = 0;
        gen_module(prog);
        assemble_wat(wat_output);
    } else {
        indent_level = 0;
        gen_module(prog);
    }

    return 0;
}
