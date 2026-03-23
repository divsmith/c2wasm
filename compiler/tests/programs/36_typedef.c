/* EXPECT_EXIT: 42 */
typedef int size_t;
typedef char byte;
typedef int *intptr;

struct Point {
    int x;
    int y;
};
typedef struct Point Point;

size_t get_size(void) {
    return 8;
}

int main(void) {
    size_t n;
    byte b;
    Point p;
    intptr q;
    byte arr[4];

    n = 10;
    if (n != 10) return 1;

    b = 65;
    if (b != 65) return 2;

    p.x = 3;
    p.y = 4;
    if (p.x != 3) return 3;
    if (p.y != 4) return 4;

    if (get_size() != 8) return 5;

    n = n + 32;
    if (n != 42) return 6;

    q = malloc(4);
    *q = 42;
    if (*q != 42) return 7;

    arr[0] = 1;
    arr[1] = 2;
    if (arr[0] + arr[1] != 3) return 8;

    return 42;
}
