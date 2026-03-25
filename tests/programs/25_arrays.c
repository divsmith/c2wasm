// Tests array subscript operator []
int main() {
    int *arr = malloc(20);
    int i;
    for (i = 0; i < 5; i = i + 1) {
        arr[i] = i * 10;
    }
    for (i = 0; i < 5; i = i + 1) {
        printf("%d ", arr[i]);
    }
    printf("\n");
    // Test assignment via subscript
    arr[2] = 99;
    printf("arr[2]=%d\n", arr[2]);
    // Test += on subscript
    arr[0] += 7;
    printf("arr[0]=%d\n", arr[0]);
    return 0;
}
