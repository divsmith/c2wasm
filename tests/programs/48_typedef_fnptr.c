/* EXPECT_EXIT: 42 */
typedef int (*BinOp)(int, int);
typedef void (*VoidFn)(int);

int g_val;

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }

void set_g(int v) { g_val = v; }

int apply(BinOp fn, int x, int y) { return fn(x, y); }

int main() {
    BinOp op;
    VoidFn vf;
    int r;

    /* basic typedef fnptr local */
    op = add;
    r = op(10, 20);
    if (r != 30) return 1;

    /* reassign */
    op = sub;
    r = op(50, 8);
    if (r != 42) return 2;

    /* pass typedef fnptr as param */
    r = apply(mul, 6, 7);
    if (r != 42) return 3;

    r = apply(add, 20, 22);
    if (r != 42) return 4;

    /* void return typedef fnptr */
    g_val = 0;
    vf = set_g;
    vf(99);
    if (g_val != 99) return 5;

    return 42;
}
