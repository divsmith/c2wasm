/* EXPECT_EXIT: 42 */
/* Test function-like macros: basic, multi-param, nested, stringify, token paste */

#define DOUBLE(x) ((x) * 2)
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ADD(a, b) ((a) + (b))
#define ANSWER() 42
#define STR(x) #x
#define PASTE(a, b) a##b
#define SQUARE(x) ((x) * (x))
#define INC(x) ((x) + 1)

int main() {
    int result;
    int xy;
    int err;
    err = 0;

    /* basic single-param */
    if (DOUBLE(21) != 42) err = 1;

    /* multi-param */
    if (MAX(10, 20) != 20) err = 2;
    if (MAX(30, 5) != 30) err = 3;
    if (ADD(17, 8) != 25) err = 4;

    /* zero-arg macro */
    if (ANSWER() != 42) err = 5;

    /* nested macros */
    if (ADD(DOUBLE(3), DOUBLE(4)) != 14) err = 6;
    if (MAX(DOUBLE(5), ADD(2, 1)) != 10) err = 7;

    /* nested macros (different names) */
    if (SQUARE(3) != 9) err = 8;
    if (INC(ADD(0, 1)) != 2) err = 9;

    /* stringify */
    printf("STR test: %s\n", STR(hello));

    /* token paste */
    xy = 99;
    if (PASTE(x, y) != 99) err = 10;

    /* macro not expanded without parens */
    result = DOUBLE(0);
    if (result != 0) err = 11;

    if (err != 0) {
        printf("FAIL: err=%d\n", err);
        return err;
    }
    printf("All function-like macro tests passed\n");
    return 42;
}
