// EXPECT_EXIT: 42
/* Test basic #include support */
#include "49_include.h"

int main(void) {
    int a;
    int b;
    a = add(3, 4);
    b = multiply(a, 6);
    printf("add(3,4) = %d\n", a);
    printf("multiply(7,6) = %d\n", b);
    if (b != 42) return 1;
    return 42;
}
