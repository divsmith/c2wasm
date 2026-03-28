/* random_numbers.c — Pseudo-random number generation
 *
 * Demonstrates rand() and srand(). The PRNG is seeded from OS entropy
 * (via WASI random_get) at startup, so output differs on each run.
 * Call srand() with a fixed seed for reproducible sequences.
 */

int main() {
    int i;
    int val;

    printf("--- Random sequence (OS-seeded) ---\n");
    for (i = 0; i < 8; i++) {
        val = rand();
        printf("  rand() = %d\n", val);
    }

    printf("\n--- Reproducible sequence (srand(42)) ---\n");
    srand(42);
    for (i = 0; i < 5; i++) {
        printf("  rand() = %d\n", rand());
    }

    printf("\nDone.\n");
    return 0;
}
