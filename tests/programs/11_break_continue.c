// EXPECT_EXIT: 16
// Tests break and continue: sum odd numbers 1..10
int main() {
    int sum = 0;
    int i = 0;
    while (i < 10) {
        i = i + 1;
        if (i % 2 == 0) {
            continue;
        }
        sum = sum + i;
        if (sum > 10) {
            break;
        }
    }
    return sum;
}
