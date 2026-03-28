// Function-like macros with stringify and token paste
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define DOUBLE(x) ((x) * 2)
#define STR(x) #x
#define PASTE(a, b) a##b

int main() {
    int val1;
    int val2;
    int xy;

    val1 = MAX(10, 20);
    val2 = DOUBLE(val1);
    printf("MAX(10,20) = %d\n", val1);
    printf("DOUBLE(%d) = %d\n", val1, val2);

    /* Nested macro calls */
    printf("MAX(DOUBLE(3), 5) = %d\n", MAX(DOUBLE(3), 5));

    /* Stringify operator */
    printf("STR(hello) = %s\n", STR(hello));

    /* Token paste operator */
    xy = 42;
    printf("PASTE(x,y) = %d\n", PASTE(x, y));

    return 0;
}
