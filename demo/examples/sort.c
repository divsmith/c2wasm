// Bubble sort
void swap(int *arr, int i, int j) {
    int tmp = arr[i];
    arr[i] = arr[j];
    arr[j] = tmp;
}

void bubble_sort(int *arr, int n) {
    int i;
    int j;
    for (i = 0; i < n - 1; i = i + 1) {
        for (j = 0; j < n - 1 - i; j = j + 1) {
            if (arr[j] > arr[j + 1]) {
                swap(arr, j, j + 1);
            }
        }
    }
}

int main() {
    int *arr = malloc(28);
    int i;
    arr[0] = 64;
    arr[1] = 34;
    arr[2] = 25;
    arr[3] = 12;
    arr[4] = 22;
    arr[5] = 11;
    arr[6] = 90;

    printf("Before: ");
    for (i = 0; i < 7; i = i + 1) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    bubble_sort(arr, 7);

    printf("After:  ");
    for (i = 0; i < 7; i = i + 1) {
        printf("%d ", arr[i]);
    }
    printf("\n");
    return 0;
}
