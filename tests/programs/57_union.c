/* EXPECT_EXIT: 42 */
union Data {
    int i;
    char c;
};

int main() {
    union Data *dp;

    dp = (union Data *)malloc(sizeof(union Data));
    dp->i = 100;
    printf("dp->i = %d\n", dp->i);
    dp->c = 65;
    printf("dp->c = %c\n", dp->c);

    /* All members share same memory - writing int then reading char */
    dp->i = 0x41;
    printf("dp->c as char = %c\n", dp->c);

    /* Sizeof union = max member size */
    printf("sizeof(union Data) = %d\n", sizeof(union Data));

    free(dp);
    return 42;
}
