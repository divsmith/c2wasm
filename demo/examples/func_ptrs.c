// Function Pointers
// Features: function pointer variables, callbacks

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }

int apply(int (*fn)(int, int), int x, int y) {
    return fn(x, y);
}

int main() {
    int (*op)(int, int);

    printf("Assign and call via pointer:\n");
    op = add; printf("  op=add: op(10, 5) = %d\n", op(10, 5));
    op = sub; printf("  op=sub: op(10, 5) = %d\n", op(10, 5));
    op = mul; printf("  op=mul: op(6, 7)  = %d\n", op(6, 7));

    printf("\nPass function as argument:\n");
    printf("  apply(add, 20, 22) = %d\n", apply(add, 20, 22));
    printf("  apply(mul, 6, 7)   = %d\n", apply(mul, 6, 7));
    printf("  apply(sub, 100, 58) = %d\n", apply(sub, 100, 58));

    printf("\nSwap operations:\n");
    op = add; printf("  add(3, 4) = %d\n", op(3, 4));
    op = mul; printf("  mul(3, 4) = %d\n", op(3, 4));

    return 0;
}
