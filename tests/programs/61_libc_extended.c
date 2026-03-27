/* Test realloc, sprintf, qsort, bsearch */

int cmp(int *a, int *b) {
    return *a - *b;
}

int main() {
    int *p;
    int i;
    char buf[128];
    int n;
    int *found;
    int key[1];

    /* realloc: grow */
    p = malloc(3 * sizeof(int));
    p[0] = 100;
    p[1] = 200;
    p[2] = 300;
    p = realloc(p, 5 * sizeof(int));
    p[3] = 400;
    p[4] = 500;
    printf("realloc: %d %d %d %d %d\n", p[0], p[1], p[2], p[3], p[4]);

    /* realloc NULL = malloc */
    free(p);
    p = realloc(0, 2 * sizeof(int));
    p[0] = 77;
    p[1] = 88;
    printf("realloc null: %d %d\n", p[0], p[1]);
    free(p);

    /* sprintf */
    n = sprintf(buf, "val=%d str=%s hex=%x pct=%%", 42, "hi", 255);
    printf("sprintf: %s (len %d)\n", buf, n);

    /* qsort */
    p = malloc(6 * sizeof(int));
    p[0] = 9;
    p[1] = 3;
    p[2] = 7;
    p[3] = 1;
    p[4] = 5;
    p[5] = 2;
    qsort(p, 6, sizeof(int), cmp);
    printf("sorted:");
    for (i = 0; i < 6; i++) {
        printf(" %d", p[i]);
    }
    printf("\n");

    /* bsearch: found */
    key[0] = 5;
    found = bsearch(key, p, 6, sizeof(int), cmp);
    if (found) {
        printf("found %d\n", *found);
    } else {
        printf("not found\n");
    }

    /* bsearch: not found */
    key[0] = 4;
    found = bsearch(key, p, 6, sizeof(int), cmp);
    if (found) {
        printf("found %d\n", *found);
    } else {
        printf("not found\n");
    }

    free(p);
    return 0;
}
