// Storage classes: static, register, auto, volatile, extern

int counter(void) {
    static int n = 0;
    n = n + 1;
    return n;
}

int accumulator(int val) {
    static int total = 0;
    total = total + val;
    return total;
}

int main() {
    register int i;

    printf("Counter demo (static local variable):\n");
    for (i = 0; i < 5; i++) {
        printf("  call %d: counter() = %d\n", i + 1, counter());
    }

    printf("\nAccumulator demo:\n");
    printf("  add 10: total = %d\n", accumulator(10));
    printf("  add 20: total = %d\n", accumulator(20));
    printf("  add 12: total = %d\n", accumulator(12));
    printf("  Final total: %d\n", accumulator(0));

    return 0;
}
