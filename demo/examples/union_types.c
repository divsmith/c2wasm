// Union types
union Number {
    int i;
    char c;
};

int main() {
    union Number *np;

    np = (union Number *)malloc(sizeof(union Number));
    np->i = 100;
    printf("np->i = %d\n", np->i);
    np->c = 65;
    printf("np->c = %c\n", np->c);

    /* Union members overlap in memory */
    np->i = 0x41;
    printf("np->c as char = %c\n", np->c);

    printf("sizeof(union Number) = %d\n", sizeof(union Number));
    free(np);
    return 0;
}
