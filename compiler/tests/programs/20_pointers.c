// Tests pointer dereference and malloc
int main() {
    int *p = malloc(4);
    *p = 42;
    printf("*p = %d\n", *p);
    int *q = malloc(4);
    *q = *p + 8;
    printf("*q = %d\n", *q);
    return 0;
}
