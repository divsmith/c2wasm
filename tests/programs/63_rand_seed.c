/* 63_rand_seed.c — Verify rand() returns values in the expected range [0, 32767]
 * and that consecutive calls produce distinct values (LCG always advances). */

int main() {
    int i;
    int prev;
    int cur;
    prev = rand();
    if (prev < 0 || prev > 32767) return 1;
    for (i = 0; i < 10; i++) {
        cur = rand();
        if (cur < 0 || cur > 32767) return 2;
        if (cur == prev) return 3;
        prev = cur;
    }
    return 0;
}
