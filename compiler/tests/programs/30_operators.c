/* EXPECT_EXIT: 42 */
int main(void) {
    int a;
    int b;
    int r;

    /* XOR binary */
    a = 15;
    b = 9;
    r = a ^ b;
    if (r != 6) return 1;

    /* post-increment */
    a = 5;
    r = a++;
    if (r != 5) return 2;
    if (a != 6) return 3;

    /* post-decrement */
    a = 10;
    r = a--;
    if (r != 10) return 4;
    if (a != 9) return 5;

    /* |= */
    a = 12;
    a |= 3;
    if (a != 15) return 6;

    /* &= */
    a = 15;
    a &= 6;
    if (a != 6) return 7;

    /* ^= */
    a = 15;
    a ^= 9;
    if (a != 6) return 8;

    /* <<= */
    a = 1;
    a <<= 3;
    if (a != 8) return 9;

    /* >>= */
    a = 16;
    a >>= 2;
    if (a != 4) return 10;

    return 42;
}
