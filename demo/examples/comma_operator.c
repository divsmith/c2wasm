// Comma operator and for-loop idioms
int main() {
    int x;
    int y;
    int i;
    int j;

    // Comma evaluates left, discards, returns right
    x = (1, 2, 42);
    printf("(1, 2, 42) = %d\n", x);

    // Multiple variables in for-loop
    printf("i, j:\n");
    for (i = 0, j = 10; i < 5; i++, j--) {
        printf("  %d, %d\n", i, j);
    }

    // Side effects in comma expression
    x = 0;
    y = (x = 10, x + 32);
    printf("x=%d, y=%d\n", x, y);

    return 0;
}
