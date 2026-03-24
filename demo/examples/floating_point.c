/* Demonstrates float and double arithmetic */

/* Approximate pi using Leibniz series: pi/4 = 1 - 1/3 + 1/5 - 1/7 + ... */
double approx_pi(int terms) {
    double sum;
    double sign;
    double denom;
    int i;
    sum = 0.0;
    sign = 1.0;
    denom = 1.0;
    for (i = 0; i < terms; i = i + 1) {
        sum = sum + sign / denom;
        sign = sign * -1.0;
        denom = denom + 2.0;
    }
    return sum * 4.0;
}

/* Newton's method square root approximation */
double my_sqrt(double n) {
    double x;
    double x1;
    int i;
    if (n <= 0.0) return 0.0;
    x = n / 2.0;
    for (i = 0; i < 20; i = i + 1) {
        x1 = (x + n / x) / 2.0;
        if (x1 < 0.0) x1 = x1 * -1.0;
        x = x1;
    }
    return x;
}

/* Celsius to Fahrenheit */
double c_to_f(double c) {
    return c * 9.0 / 5.0 + 32.0;
}

/* Fahrenheit to Celsius */
double f_to_c(double f) {
    return (f - 32.0) * 5.0 / 9.0;
}

int main() {
    double pi;
    double sq2;
    double sq3;
    int i;

    /* Pi approximation */
    printf("Pi approximations (Leibniz series):\n");
    pi = approx_pi(10);
    printf("  10 terms:    %f\n", pi);
    pi = approx_pi(100);
    printf("  100 terms:   %f\n", pi);
    pi = approx_pi(1000);
    printf("  1000 terms:  %f\n", pi);

    /* Square roots */
    printf("\nSquare root (Newton's method):\n");
    sq2 = my_sqrt(2.0);
    printf("  sqrt(2)  = %f\n", sq2);
    sq3 = my_sqrt(3.0);
    printf("  sqrt(3)  = %f\n", sq3);
    printf("  sqrt(16) = %f\n", my_sqrt(16.0));
    printf("  sqrt(0)  = %f\n", my_sqrt(0.0));

    /* Temperature conversion */
    printf("\nTemperature conversions:\n");
    printf("  0 C  = %f F\n", c_to_f(0.0));
    printf("  100 C = %f F\n", c_to_f(100.0));
    printf("  98.6 F = %f C\n", f_to_c(98.6));
    printf("  -40 C = %f F (same as -40 F)\n", c_to_f(-40.0));

    /* Mixed int/float arithmetic */
    printf("\nMixed arithmetic:\n");
    printf("  5 / 2 (int) = %d\n", 5 / 2);
    printf("  5.0 / 2 (float) = %f\n", 5.0 / 2);
    printf("  (double)5 / 2 = %f\n", (double)5 / 2);

    /* Float comparisons */
    printf("\nFloat comparisons:\n");
    printf("  0.1 + 0.2 == 0.3: %d\n", 0.1 + 0.2 == 0.3);
    printf("  1.0 < 2.0: %d\n", 1.0 < 2.0);
    printf("  -1.5 < 0.0: %d\n", -1.5 < 0.0);

    return 0;
}
