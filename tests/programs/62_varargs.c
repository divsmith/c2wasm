/* Variadic functions: va_list, va_start, va_arg, va_end */

int sum(int n, ...) {
    va_list ap;
    int total;
    int i;
    total = 0;
    va_start(ap, n);
    for (i = 0; i < n; i++) {
        total = total + va_arg(ap, int);
    }
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

/* Forward-declared variadic function */
int count_positives(int n, ...);

int my_printf(char *fmt, ...) {
    va_list ap;
    int i;
    int count;
    count = 0;
    va_start(ap, fmt);
    for (i = 0; fmt[i] != 0; i++) {
        if (fmt[i] == '%' && fmt[i + 1] == 'd') {
            printf("%d", va_arg(ap, int));
            i++;
            count++;
        } else if (fmt[i] == '%' && fmt[i + 1] == 's') {
            printf("%s", va_arg(ap, char *));
            i++;
            count++;
        } else {
            putchar(fmt[i]);
        }
    }
    va_end(ap);
    return count;
}

int main() {
    int s;
    int m;

    /* basic sum */
    s = sum(4, 10, 20, 30, 40);
    printf("sum(4, 10,20,30,40) = %d\n", s);

    /* sum of 1 element */
    printf("sum(1, 99) = %d\n", sum(1, 99));

    /* sum of 0 elements */
    printf("sum(0) = %d\n", sum(0));

    /* max_of */
    m = max_of(5, 3, 7, 2, 9, 1);
    printf("max_of(5, 3,7,2,9,1) = %d\n", m);

    /* forward-declared */
    printf("count_positives = %d\n", count_positives(6, -3, 5, -1, 8, 0, 12));

    /* my_printf: user-defined variadic printf */
    my_printf("Hello %s, age %d\n", "Alice", 30);

    return 0;
}

int count_positives(int n, ...) {
    va_list ap;
    int count;
    int val;
    int i;
    count = 0;
    va_start(ap, n);
    for (i = 0; i < n; i++) {
        val = va_arg(ap, int);
        if (val > 0) count++;
    }
    va_end(ap);
    return count;
}
