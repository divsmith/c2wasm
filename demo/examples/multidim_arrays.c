// Multi-dimensional (2D) arrays
int main() {
    int grid[3][4];
    int i;
    int j;

    /* Fill grid: grid[i][j] = i*10 + j */
    for (i = 0; i < 3; i = i + 1)
        for (j = 0; j < 4; j = j + 1)
            grid[i][j] = i * 10 + j;

    /* Print grid */
    for (i = 0; i < 3; i = i + 1) {
        for (j = 0; j < 4; j = j + 1)
            printf("%d ", grid[i][j]);
        printf("\n");
    }

    /* 2D array with brace initializer */
    int m[2][3] = {{1, 2, 3}, {4, 5, 6}};
    printf("m[1][2] = %d\n", m[1][2]);
    return 0;
}
