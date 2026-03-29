// EXPECT_EXIT: 42
/* regression: DCE must scan argument expressions of indirect calls */

int get_value(void) {
    return 42;
}

int identity(int x) {
    return x;
}

int main() {
    int (*fp)(int);
    fp = identity;
    return fp(get_value());
}
