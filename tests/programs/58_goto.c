/* EXPECT_EXIT: 42 */
int main() {
    int x;
    int sum;
    int i;

    /* Forward goto */
    x = 0;
    goto skip;
    x = 999;
skip:
    printf("forward goto: x = %d\n", x);

    /* Backward goto (loop) */
    sum = 0;
    i = 1;
loop_start:
    if (i <= 5) {
        sum = sum + i;
        i = i + 1;
        goto loop_start;
    }
    printf("backward goto sum = %d\n", sum);

    /* Goto in nested if */
    x = 10;
    if (x > 5) {
        goto was_big;
    } else {
        goto was_small;
    }
was_big:
    printf("was big\n");
    goto done;
was_small:
    printf("was small\n");
done:
    printf("done, x = %d\n", x);

    return 42;
}
