/* EXPECT_EXIT: 42 */
struct Point {
    int x;
    int y;
};

int main(void) {
    int a;
    char b;

    if (sizeof(int) != 4) return 1;
    if (sizeof(char) != 1) return 2;
    if (sizeof(struct Point) != 8) return 3;
    if (sizeof(int *) != 4) return 4;
    if (sizeof(char *) != 4) return 5;
    a = 42;
    if (sizeof(a) != 4) return 6;
    b = 'x';
    if (sizeof(b) != 1) return 7;
    return 42;
}
