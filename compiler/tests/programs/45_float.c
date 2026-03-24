/* EXPECT_EXIT: 42 */
/* SKIP_BINARY */
int main() {
    double d;
    double d2;
    double d3;
    int i;

    /* float literal assignment */
    d = 3.14;
    d2 = 2.0;

    /* addition */
    d3 = d + d2;
    if (d3 < 5.0) return 1;
    if (d3 > 6.0) return 2;

    /* multiplication */
    d3 = d * d2;
    if (d3 < 6.0) return 3;
    if (d3 > 7.0) return 4;

    /* subtraction */
    d3 = d - d2;
    if (d3 < 1.0) return 5;
    if (d3 > 2.0) return 6;

    /* division */
    d3 = d / d2;
    if (d3 < 1.0) return 7;
    if (d3 > 2.0) return 8;

    /* comparison */
    if (d == d2) return 9;
    if (d != d) return 10;

    /* int to float cast */
    i = 42;
    d = (double)i;
    if (d < 41.0) return 11;
    if (d > 43.0) return 12;

    /* float to int cast */
    d = 3.7;
    i = (int)d;
    if (i != 3) return 13;

    /* implicit int-to-float at assignment */
    d = 10;
    if (d < 9.0) return 14;
    if (d > 11.0) return 15;

    /* mixed int+float arithmetic */
    d = 3.14;
    d3 = d + 1;
    if (d3 < 4.0) return 16;
    if (d3 > 5.0) return 17;

    /* negative float via f64.neg */
    d = 0.0 - 3.14;
    if (d > 0.0 - 3.0) return 18;

    /* printf %f */
    printf("pi = %f\n", 3.14159);

    return 42;
}
