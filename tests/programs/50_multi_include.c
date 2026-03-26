// EXPECT_EXIT: 42
/* Test multiple includes and include-once behavior */
#include "50_helper1.h"
#include "50_helper2.h"

int main(void) {
    int a;
    int b;
    a = helper1(5);
    b = helper2(5);
    printf("helper1(5) = %d\n", a);
    printf("helper2(5) = %d\n", b);
    printf("square(3) = %d\n", square(3));
    if (a != 125) return 1;
    if (b != -75) return 2;
    return 42;
}
