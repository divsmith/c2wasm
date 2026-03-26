// EXPECT_EXIT: 42
int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }
int identity(int x) { return x; }
int double_it(int x) { return x + x; }

int apply(int (*fn)(int, int), int x, int y) {
    return fn(x, y);
}

int main() {
    int (*op)(int, int);
    int result;
    int (*unary)(int);

    /* basic function pointer call */
    op = add;
    result = op(10, 20);
    if (result != 30) return 1;

    /* reassign and call */
    op = sub;
    result = op(50, 8);
    if (result != 42) return 2;

    /* passing function pointer as argument */
    result = apply(mul, 6, 7);
    if (result != 42) return 3;

    result = apply(add, 20, 22);
    if (result != 42) return 4;

    /* unary function pointer */
    unary = identity;
    result = unary(42);
    if (result != 42) return 5;

    unary = double_it;
    result = unary(21);
    if (result != 42) return 6;

    return 42;
}
