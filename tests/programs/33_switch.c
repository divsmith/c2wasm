/* EXPECT_EXIT: 42 */
int main(void) {
    int x;
    int r;
    int i;

    /* basic switch with break */
    x = 2;
    r = 0;
    switch (x) {
        case 1:
            r = 10;
            break;
        case 2:
            r = 20;
            break;
        case 3:
            r = 30;
            break;
        default:
            r = 99;
            break;
    }
    if (r != 20) return 1;

    /* default case */
    x = 99;
    r = 0;
    switch (x) {
        case 1: r = 1; break;
        case 2: r = 2; break;
        default: r = 42; break;
    }
    if (r != 42) return 2;

    /* fallthrough */
    x = 1;
    r = 0;
    switch (x) {
        case 1:
            r = r + 1;
        case 2:
            r = r + 2;
            break;
        case 3:
            r = 99;
            break;
    }
    if (r != 3) return 3;

    /* switch inside loop with break/continue */
    r = 0;
    for (i = 0; i < 5; i++) {
        switch (i) {
            case 2:
                continue;
            case 4:
                break;
            default:
                r = r + i;
                break;
        }
    }
    /* r = 0+1+3 = 4 (skipped i=2 via continue, i=4 break exits switch only) */
    if (r != 4) return 4;

    return 42;
}
