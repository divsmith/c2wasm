// Extended libc: realloc, sprintf, qsort, bsearch

int cmp(int *a, int *b) {
    return *a - *b;
}

int main() {
    int *arr;
    int i;
    char buf[128];

    /* realloc: grow an allocation */
    arr = malloc(3 * sizeof(int));
    arr[0] = 50; arr[1] = 10; arr[2] = 30;
    arr = realloc(arr, 5 * sizeof(int));
    arr[3] = 40; arr[4] = 20;
    printf("before sort:");
    for (i = 0; i < 5; i++) printf(" %d", arr[i]);
    printf("\n");

    /* qsort with function pointer comparator */
    qsort(arr, 5, sizeof(int), cmp);
    printf("after sort: ");
    for (i = 0; i < 5; i++) printf(" %d", arr[i]);
    printf("\n");

    /* sprintf into buffer */
    sprintf(buf, "sum=%d", arr[0]+arr[1]+arr[2]+arr[3]+arr[4]);
    printf("sprintf: %s\n", buf);

    /* bsearch for a value */
    int key[1];
    int *found;
    key[0] = 30;
    found = bsearch(key, arr, 5, sizeof(int), cmp);
    if (found)
        printf("found %d\n", *found);

    free(arr);
    return 0;
}
