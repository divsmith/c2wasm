/* Demonstrates unsigned, short, and long integer types */

int main() {
    unsigned int u;
    int s;
    unsigned short us;
    short ss;
    long l;
    unsigned int max_u;
    unsigned int bits;
    int i;

    /* Unsigned vs signed: same bit pattern, different interpretation */
    u = 4294967295; /* 0xFFFFFFFF = max unsigned */
    s = -1;
    printf("unsigned max: %d (as signed: %d)\n", (int)u, s);
    printf("unsigned > 0: %d\n", u > 0);       /* 1: unsigned wraps, still > 0 */
    printf("signed > 0: %d\n", s > 0);         /* 0: signed -1 < 0 */

    /* Unsigned comparisons */
    printf("\nUnsigned comparison:\n");
    max_u = 4294967295;
    printf("  max_uint > 100: %d\n", max_u > (unsigned int)100);

    /* Short integer: 16-bit storage */
    printf("\nShort integers:\n");
    ss = 32767; /* max short */
    printf("  max short: %d\n", ss);
    ss = ss + 1; /* overflow */
    printf("  max short + 1: %d\n", ss); /* wraps to -32768 */

    us = 65535; /* max unsigned short */
    printf("  max unsigned short: %d\n", (int)us);
    us = us + 1; /* wraps to 0 */
    printf("  max unsigned short + 1: %d\n", (int)us);

    /* Bit manipulation with unsigned */
    printf("\nBit manipulation:\n");
    bits = 0;
    for (i = 0; i < 8; i = i + 1) {
        bits = bits | (1 << i);
    }
    printf("  bits set 0-7: %d (0x%x)\n", (int)bits, (int)bits);
    bits = bits >> 1;
    printf("  logical right shift: %d (0x%x)\n", (int)bits, (int)bits);

    /* Long (same as int in c2wasm, but valid syntax) */
    printf("\nLong integers:\n");
    l = 2147483647; /* max int = max long in c2wasm */
    printf("  max long: %d\n", (int)l);

    return 0;
}
