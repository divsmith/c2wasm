// Variadic functions: va_list, va_start, va_arg, va_end

int sum(int n, ...) {
    va_list ap;
    int total;
    int i;
    total = 0;
    va_start(ap, n);
    for (i = 0; i < n; i++)
        total = total + va_arg(ap, int);
    va_end(ap);
    return total;
}

int max_of(int n, ...) {
    va_list ap;
    int best;
    int val;
    int i;
    va_start(ap, n);
    best = va_arg(ap, int);
    for (i = 1; i < n; i++) {
        val = va_arg(ap, int);
        if (val > best) best = val;
    }
    va_end(ap);
    return best;
}

int main() {
    printf("sum(3, 10, 20, 30) = %d\n", sum(3, 10, 20, 30));
    printf("sum(5, 1,2,3,4,5) = %d\n", sum(5, 1, 2, 3, 4, 5));
    printf("max of (3,7,2,9,1) = %d\n", max_of(5, 3, 7, 2, 9, 1));
    return 0;
}
