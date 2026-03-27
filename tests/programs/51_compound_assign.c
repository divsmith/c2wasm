// EXPECT_EXIT: 42
int main() {
    int a;
    int b;
    int c;

    a = 6;
    a *= 7;
    if (a != 42) return 1;

    b = 100;
    b /= 5;
    if (b != 20) return 2;

    c = 47;
    c %= 10;
    if (c != 7) return 3;

    // Test with pointers (via malloc)
    {
        int *p;
        p = malloc(4);
        *p = 3;
        *p *= 14;
        if (*p != 42) return 4;

        *p = 84;
        *p /= 2;
        if (*p != 42) return 5;

        *p = 107;
        *p %= 50;
        if (*p != 7) return 6;
        free(p);
    }

    // Test with arrays
    {
        int arr[3];
        arr[0] = 6;
        arr[1] = 100;
        arr[2] = 47;

        arr[0] *= 7;
        arr[1] /= 5;
        arr[2] %= 10;

        if (arr[0] != 42) return 7;
        if (arr[1] != 20) return 8;
        if (arr[2] != 7) return 9;
    }

    // Test chaining
    {
        int x;
        x = 2;
        x *= 3;
        x *= 7;
        if (x != 42) return 10;
    }

    return 42;
}
