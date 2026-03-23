// Fibonacci sequence
int fib(int n) {
    if (n <= 1) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

int main() {
    int i;
    for (i = 0; i <= 10; i = i + 1) {
        printf("fib(%d) = %d\n", i, fib(i));
    }
    return 0;
}
