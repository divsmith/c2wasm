// EXPECT_EXIT: 42
/* Test unsigned arithmetic and comparisons */

int main() {
    unsigned int a;
    unsigned int b;
    unsigned int c;
    unsigned char uc;
    int result;

    a = 10;
    b = 3;

    /* Basic unsigned arithmetic */
    c = a / b;    /* 3 (unsigned division) */
    if (c != 3) return 1;

    c = a % b;    /* 1 (unsigned modulo) */
    if (c != 1) return 2;

    /* Unsigned comparison */
    if (a < b) return 3;   /* 10 < 3 should be false */
    if (b > a) return 4;   /* 3 > 10 should be false */
    if (a <= b) return 5;

    /* Unsigned char */
    uc = 200;
    if (uc != 200) return 6;

    /* Unsigned right shift (logical, not arithmetic) */
    a = 0 - 1;  /* 0xFFFFFFFF = 4294967295 unsigned */
    b = a >> 28; /* should be 15, not -1 (logical shift) */
    if (b != 15) return 7;

    /* Signed vs unsigned - signed should use signed ops by default */
    {
        int sa;
        int sb;
        sa = 0 - 5;  /* -5 */
        sb = 3;
        if (sa > sb) return 8;   /* -5 > 3 should be false (signed) */
    }

    return 42;
}
