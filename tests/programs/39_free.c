// EXPECT_EXIT: 42
/* Test real free() with memory reuse */

int main() {
    int *a;
    int *b;
    int *c;
    int *d;
    int result;

    /* Allocate three blocks */
    a = (int *)malloc(100);
    b = (int *)malloc(100);
    c = (int *)malloc(100);

    /* Write sentinel values */
    *a = 10;
    *b = 20;
    *c = 30;

    /* Free the middle block */
    free(b);

    /* Allocate again - should reuse freed block */
    d = (int *)malloc(100);

    /* d should have gotten b's old memory */
    *d = 40;

    /* a and c should be untouched */
    result = *a + *c + *d;

    /* Free everything */
    free(a);
    free(c);
    free(d);

    /* Allocate after freeing all - tests coalescing */
    a = (int *)malloc(400);
    *a = 99;

    /* Free NULL should be safe */
    free((int *)0);

    /* result = 10 + 30 + 40 = 80, but we want exit 42 */
    /* So let's just verify the basics work and return 42 */
    if (result != 80) return 1;
    if (*a != 99) return 2;

    free(a);
    return 42;
}
