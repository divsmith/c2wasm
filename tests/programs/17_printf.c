// Tests printf with %d format specifier
int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

int main() {
    printf("fib(%d) = %d\n", 10, fib(10));
    return 0;
}
