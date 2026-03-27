/* EXPECT_EXIT: 42 */
/* Test multi-dimensional (2D) arrays: declaration, access, initializers */

int sum_2d(int *data, int rows, int cols) {
    int i;
    int j;
    int s;
    s = 0;
    for (i = 0; i < rows; i = i + 1)
        for (j = 0; j < cols; j = j + 1)
            s = s + data[i * cols + j];
    return s;
}

int main() {
    int a[2][3] = {{10, 20, 30}, {40, 50, 60}};
    int b[3][4];
    int c[2][2];
    int i;
    int j;
    int err;
    err = 0;

    /* Test brace initializer */
    if (a[0][0] != 10) err = 1;
    if (a[0][1] != 20) err = 2;
    if (a[0][2] != 30) err = 3;
    if (a[1][0] != 40) err = 4;
    if (a[1][1] != 50) err = 5;
    if (a[1][2] != 60) err = 6;

    /* Test manual fill */
    for (i = 0; i < 3; i = i + 1)
        for (j = 0; j < 4; j = j + 1)
            b[i][j] = i * 100 + j;

    if (b[0][0] != 0) err = 7;
    if (b[2][3] != 203) err = 8;
    if (b[1][2] != 102) err = 9;

    /* Test compound assignment on 2D array */
    c[0][0] = 10;
    c[0][1] = 20;
    c[1][0] = 30;
    c[1][1] = 40;
    c[0][0] += 5;
    c[1][1] -= 10;
    if (c[0][0] != 15) err = 10;
    if (c[1][1] != 30) err = 11;

    /* Test flat initializer for 2D */
    /* int d[2][3] = {1,2,3,4,5,6}; also works via flat init */

    /* Print some values for expected output */
    printf("a[1][2]=%d\n", a[1][2]);
    printf("b[2][3]=%d\n", b[2][3]);
    printf("c[0][0]=%d c[1][1]=%d\n", c[0][0], c[1][1]);

    if (err != 0) {
        printf("FAIL: err=%d\n", err);
        return err;
    }
    printf("All 2D array tests passed\n");
    return 42;
}
