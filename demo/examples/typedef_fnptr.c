// Typedef Function Pointers
// Features: typedef for function pointer types

typedef int (*BinOp)(int, int);

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }

int apply(BinOp fn, int x, int y) {
    return fn(x, y);
}

int fold(BinOp fn, int *data, int n) {
    int result;
    int i;
    result = data[0];
    for (i = 1; i < n; i = i + 1) {
        result = fn(result, data[i]);
    }
    return result;
}

int main() {
    BinOp op;
    int *nums;
    int i;

    printf("Typedef'd function pointer:\n");
    op = add; printf("  op=add: op(10, 5) = %d\n", op(10, 5));
    op = sub; printf("  op=sub: op(10, 5) = %d\n", op(10, 5));
    op = mul; printf("  op=mul: op(6, 7)  = %d\n", op(6, 7));

    printf("\nPass BinOp as argument:\n");
    printf("  apply(add, 20, 22) = %d\n", apply(add, 20, 22));
    printf("  apply(mul, 6, 7)   = %d\n", apply(mul, 6, 7));

    printf("\nFold over [1, 2, 3, 4, 5]:\n");
    nums = malloc(20);
    for (i = 0; i < 5; i = i + 1) {
        nums[i] = i + 1;
    }
    printf("  sum:     %d\n", fold(add, nums, 5));
    printf("  product: %d\n", fold(mul, nums, 5));

    return 0;
}
