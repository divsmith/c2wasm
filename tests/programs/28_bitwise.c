// EXPECT_EXIT: 42
/* Test bitwise operators: |, &, ~, <<, >> */
int main() {
    int a = 0xF0;
    int b = 0x0F;

    /* OR */
    int or_result = a | b;
    if (or_result != 255) return 1;

    /* AND */
    int and_result = a & b;
    if (and_result != 0) return 2;

    /* NOT */
    int not_result = ~0;
    if (not_result != -1) return 3;

    /* shift left */
    int shl = 1 << 8;
    if (shl != 256) return 4;

    /* shift right */
    int shr = 256 >> 4;
    if (shr != 16) return 5;

    /* combined */
    int x = (1 << 4) | (1 << 2);
    if (x != 20) return 6;

    return 42;
}
