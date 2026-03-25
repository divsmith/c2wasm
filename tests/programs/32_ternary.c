/* EXPECT_EXIT: 42 */
int main(void) {
    int a;
    int b;
    int r;

    /* basic ternary */
    a = 5;
    r = (a > 3) ? 10 : 20;
    if (r != 10) return 1;

    r = (a < 3) ? 10 : 20;
    if (r != 20) return 2;

    /* nested ternary */
    a = 2;
    r = (a == 1) ? 100 : (a == 2) ? 200 : 300;
    if (r != 200) return 3;

    /* ternary as argument */
    b = 0;
    b = b + ((a > 0) ? 40 : 0);
    b = b + ((a > 0) ? 2 : 0);
    if (b != 42) return 4;

    return 42;
}
