// EXPECT_EXIT: 42

int increment(void) {
    static int n = 0;
    n = n + 1;
    return n;
}

int main() {
    register int a;
    auto int b;
    volatile int c;
    int r1;
    int r2;
    int r3;

    a = 10;
    b = 20;
    c = 12;
    if (a + b + c != 42) return 1;

    r1 = increment();
    r2 = increment();
    r3 = increment();
    if (r1 != 1) return 2;
    if (r2 != 2) return 3;
    if (r3 != 3) return 4;

    return 42;
}
