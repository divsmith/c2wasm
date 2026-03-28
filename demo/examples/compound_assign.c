// Compound assignment operators: *=, /=, %=
int main() {
    int a;
    int b;
    int c;

    a = 6;
    a *= 7;
    printf("6 *= 7  = %d\n", a);

    b = 100;
    b /= 3;
    printf("100 /= 3 = %d\n", b);

    c = 47;
    c %= 10;
    printf("47 %%= 10 = %d\n", c);

    // Works with arrays too
    {
        int arr[3];
        arr[0] = 2;
        arr[1] = 100;
        arr[2] = 17;
        arr[0] *= 21;
        arr[1] /= 4;
        arr[2] %= 5;
        printf("arr: %d, %d, %d\n", arr[0], arr[1], arr[2]);
    }

    return 0;
}
