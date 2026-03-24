/* EXPECT_EXIT: 42 */
int main(void) {
    int i;
    int sum;

    /* basic do-while */
    i = 0;
    sum = 0;
    do {
        sum = sum + i;
        i = i + 1;
    } while (i < 5);
    if (sum != 10) return 1;

    /* do-while executes body at least once */
    i = 100;
    do {
        i = i + 1;
    } while (0);
    if (i != 101) return 2;

    /* break in do-while */
    i = 0;
    do {
        i = i + 1;
        if (i == 3) break;
    } while (i < 10);
    if (i != 3) return 3;

    /* continue in do-while */
    i = 0;
    sum = 0;
    do {
        i = i + 1;
        if (i == 3) continue;
        sum = sum + i;
    } while (i < 5);
    /* sum = 1+2+4+5 = 12 (skipped 3) */
    if (sum != 12) return 4;

    return 42;
}
