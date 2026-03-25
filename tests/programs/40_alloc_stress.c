// EXPECT_EXIT: 42
/* Stress test: many alloc/free cycles to verify no OOM */

int main() {
    int i;
    int *ptrs[10];
    int sum;

    /* Phase 1: allocate and free in a loop */
    i = 0;
    while (i < 1000) {
        int *p;
        p = (int *)malloc(64);
        *p = i;
        free(p);
        i = i + 1;
    }

    /* Phase 2: allocate 10 blocks, free every other one, re-allocate */
    i = 0;
    while (i < 10) {
        ptrs[i] = (int *)malloc(32);
        *ptrs[i] = i * 10;
        i = i + 1;
    }

    /* Free even indices */
    i = 0;
    while (i < 10) {
        free(ptrs[i]);
        i = i + 2;
    }

    /* Re-allocate into freed slots */
    i = 0;
    while (i < 10) {
        ptrs[i] = (int *)malloc(32);
        *ptrs[i] = 100 + i;
        i = i + 2;
    }

    /* Verify odd indices are untouched */
    sum = 0;
    i = 1;
    while (i < 10) {
        sum = sum + *ptrs[i];
        i = i + 2;
    }
    /* odd indices: 10+30+50+70+90 = 250 */
    if (sum != 250) return 1;

    /* Phase 3: free all and do a large allocation */
    i = 0;
    while (i < 10) {
        free(ptrs[i]);
        i = i + 1;
    }

    /* Large allocation after freeing everything - coalescing should help */
    {
        int *big;
        big = (int *)malloc(2048);
        *big = 42;
        if (*big != 42) return 2;
        free(big);
    }

    return 42;
}
