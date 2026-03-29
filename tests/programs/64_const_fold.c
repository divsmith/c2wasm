int main() {
    /* arithmetic */
    int a = 2 + 3;
    int b = 10 - 3;
    int c = 4 * 5;
    int d = 10 / 2;
    int e = 10 % 3;
    /* bitwise */
    int f = 0xFF & 0x0F;
    int g = 0xF0 | 0x0F;
    int h = 0xFF ^ 0xF0;
    int sh = 1 << 4;
    int sr = 256 >> 3;
    /* comparisons folded to 0/1 */
    int eq = (5 == 5);
    int ne = (5 != 3);
    int lt = (3 < 5);
    /* unary */
    int neg = -7;
    int bnot = ~0;
    int lnot = !0;
    /* nested: folds recursively because each binary folds to a literal */
    int nested = (2 + 3) * 4;
    printf("%d %d %d %d %d\n", a, b, c, d, e);
    printf("%d %d %d %d %d\n", f, g, h, sh, sr);
    printf("%d %d %d\n", eq, ne, lt);
    printf("%d %d %d\n", neg, bnot, lnot);
    printf("%d\n", nested);
    return 0;
}
