/* EXPECT_EXIT: 42 */

int sum_arr(int *a, int n) {
    int i;
    int s;
    i = 0;
    s = 0;
    while (i < n) {
        s = s + a[i];
        i = i + 1;
    }
    return s;
}

int main(void) {
    int a[5];
    int b[3];
    char msg[4];
    int r;

    a[0] = 10; a[1] = 20; a[2] = 30; a[3] = 40; a[4] = 50;
    if (sum_arr(a, 5) != 150) return 1;

    b[0] = 100; b[1] = 200; b[2] = 300;
    if (sum_arr(b, 3) != 600) return 2;

    msg[0] = 72; msg[1] = 105; msg[2] = 33; msg[3] = 0;
    if (msg[0] != 72) return 3;

    r = a[2] + b[1];
    if (r != 230) return 4;

    return 42;
}
