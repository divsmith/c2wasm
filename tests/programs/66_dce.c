// EXPECT_EXIT: 42
/* test dead code elimination — unused functions are not emitted */

/* This function IS used — should be kept */
int used_helper(int x) {
    return x * 2;
}

/* These functions are NOT called from main — should be eliminated */
int dead_func_a(int x) {
    return x + 100;
}

int dead_func_b(void) {
    return dead_func_a(1);
}

int dead_func_c(int x, int y) {
    return dead_func_b() + x + y;
}

int main() {
    printf("%d\n", used_helper(21));
    return 42;
}
