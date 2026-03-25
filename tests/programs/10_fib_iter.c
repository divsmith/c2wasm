// EXPECT_EXIT: 55
// Tests iterative fibonacci: fib(10) = 55
int main() {
    int n = 10;
    int a = 0;
    int b = 1;
    int i = 0;
    while (i < n) {
        int t = a + b;
        a = b;
        b = t;
        i = i + 1;
    }
    return a;
}
