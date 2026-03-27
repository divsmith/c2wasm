// EXPECT_EXIT: 42
int main() {
    int x;
    int y;
    int z;

    // Basic comma operator — evaluates left, discards, returns right
    x = (1, 2, 42);
    if (x != 42) return 1;

    // Comma in for loop
    y = 0;
    z = 0;
    {
        int i;
        int j;
        for (i = 0, j = 10; i < 5; i++, j--) {
            y = y + i;
            z = z + j;
        }
    }
    // y = 0+1+2+3+4 = 10
    // z = 10+9+8+7+6 = 40
    if (y != 10) return 2;
    if (z != 40) return 3;

    // Side effects in comma expression
    x = 0;
    y = (x = 10, x + 32);
    if (x != 10) return 4;
    if (y != 42) return 5;

    return 42;
}
