// goto and labels
int main() {
    int i;
    int sum;

    /* Forward goto */
    goto start;
    printf("this is skipped\n");
start:
    printf("jumped to start\n");

    /* Backward goto as loop */
    sum = 0;
    i = 1;
add_next:
    if (i <= 10) {
        sum = sum + i;
        i = i + 1;
        goto add_next;
    }
    printf("sum 1..10 = %d\n", sum);

    /* Multiple labels */
    goto end;
    printf("this is also skipped\n");
end:
    printf("done\n");
    return 0;
}
