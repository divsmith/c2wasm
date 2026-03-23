// EXPECT_EXIT: 9
// Tests nested if/else and compound assignment
int main() {
    int x = 5;
    int y = 10;
    int result = 0;

    if (x < y) {
        if (x > 3) {
            result = x + y;
        } else {
            result = y - x;
        }
    }

    x += 1;
    result -= x;
    return result;
}
