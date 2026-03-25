// EXPECT_EXIT: 55
// Tests for loop: sum 1..10
int main() {
    int sum = 0;
    int i;
    for (i = 1; i <= 10; i = i + 1) {
        sum = sum + i;
    }
    return sum;
}
