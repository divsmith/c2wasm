// EXPECT_EXIT: 120
// Tests multiple functions and calls: factorial(5) = 120
int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int main() {
    return factorial(5);
}
