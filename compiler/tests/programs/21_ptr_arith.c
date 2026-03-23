// Tests pointer arithmetic and array-like usage
int main() {
    int *arr = malloc(20);
    int i;
    // Store values: arr[i] = i * 10
    for (i = 0; i < 5; i = i + 1) {
        *(arr + i * 4) = i * 10;
    }
    // Read them back
    for (i = 0; i < 5; i = i + 1) {
        printf("%d ", *(arr + i * 4));
    }
    printf("\n");
    return 0;
}
