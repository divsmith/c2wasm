// EXPECT_EXIT: 25
// Tests multiple functions with multiple parameters
int add(int a, int b) {
    return a + b;
}

int mul(int a, int b) {
    return a * b;
}

int main() {
    int x = add(3, 4);
    int y = mul(x, 3);
    return y + add(1, 3);
}
