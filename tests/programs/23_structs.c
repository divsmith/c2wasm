// Tests struct definition, malloc, sizeof, and member access via ->
struct Point {
    int x;
    int y;
};

int main() {
    struct Point *p = malloc(sizeof(struct Point));
    p->x = 10;
    p->y = 20;
    printf("(%d, %d)\n", p->x, p->y);
    printf("size=%d\n", sizeof(struct Point));
    p->x += 5;
    printf("after += : (%d, %d)\n", p->x, p->y);
    return 0;
}
