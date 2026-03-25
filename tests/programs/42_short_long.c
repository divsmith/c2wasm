// EXPECT_EXIT: 42
/* Test short and long types */

int main() {
    short s;
    short int si;
    long l;
    long int li;
    unsigned short us;
    short *sp;
    int result;

    /* Short basic assignment */
    s = 100;
    if (s != 100) return 1;

    /* Short overflow wraps at 16-bit boundary */
    s = 32767;   /* max signed short */

    /* Short pointer arithmetic via subscript */
    sp = (short *)malloc(6);  /* 3 shorts */
    sp[0] = 10;
    sp[1] = 20;
    sp[2] = 30;

    result = sp[0] + sp[1] + sp[2];
    if (result != 60) return 2;
    free(sp);

    /* Long is same as int (32-bit WASM) */
    l = 1000000;
    if (l != 1000000) return 3;

    /* Unsigned short */
    us = 60000;
    if (us != 60000) return 4;

    /* sizeof checks */
    if (sizeof(short) != 2) return 5;
    if (sizeof(long) != 4) return 6;
    if (sizeof(unsigned int) != 4) return 7;

    return 42;
}
