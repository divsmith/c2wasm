/* EXPECT_EXIT: 42 */

int main(void) {
    int a[3] = {1, 2, 3};
    int b[5] = {10, 20, 30, 40, 50};
    int i;
    int sum;

    sum = 0;
    i = 0;
    while (i < 3) {
        sum = sum + a[i];
        i = i + 1;
    }
    if (sum != 6) return 1;

    sum = 0;
    i = 0;
    while (i < 5) {
        sum = sum + b[i];
        i = i + 1;
    }
    if (sum != 150) return 2;

    return 42;
}
