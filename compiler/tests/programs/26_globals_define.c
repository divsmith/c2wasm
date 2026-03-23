// Tests #define, global variables, and type casts
#define MAX_SIZE 10
#define MAGIC 0xff

int counter;
int total = 0;

void bump(int n) {
    counter += n;
    total += n;
}

int main() {
    counter = 0;
    bump(5);
    bump(3);
    printf("counter=%d total=%d\n", counter, total);
    printf("MAX_SIZE=%d MAGIC=%d\n", MAX_SIZE, MAGIC);

    // type cast (no-op, but should parse)
    int *p = malloc(4);
    *p = 42;
    int val = (int)*p;
    printf("cast=%d\n", val);

    // cast in expression
    char *s = (char *)malloc(8);
    *(s) = 65;
    printf("char=%c\n", (int)*s);

    return 0;
}
