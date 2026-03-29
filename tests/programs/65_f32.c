/* test f32 (float) type support */

float add_f32(float a, float b) {
    return a + b;
}

float scale(float x, float factor) {
    return x * factor;
}

double to_double(float x) {
    return (double)x;
}

float from_double(double x) {
    return (float)x;
}

int float_to_int(float x) {
    return (int)x;
}

float int_to_float(int x) {
    return (float)x;
}

int main() {
    float a;
    float b;
    float c;
    double d;
    int n;

    a = 1.5f;
    b = 2.5f;
    c = add_f32(a, b);
    printf("%d\n", (int)c);

    c = scale(3.0f, 2.0f);
    printf("%d\n", (int)c);

    d = to_double(1.5f);
    printf("%d\n", (int)(d * 2.0));

    c = from_double(4.75);
    printf("%d\n", (int)c);

    n = float_to_int(7.9f);
    printf("%d\n", n);

    c = int_to_float(5);
    printf("%d\n", (int)c);

    /* mixed float + double */
    d = a + 1.0;
    printf("%d\n", (int)d);

    /* float array */
    float arr[4];
    arr[0] = 1.0f;
    arr[1] = 2.0f;
    arr[2] = 3.0f;
    arr[3] = 4.0f;
    printf("%d\n", (int)(arr[0] + arr[1] + arr[2] + arr[3]));

    return 0;
}
