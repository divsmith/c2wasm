// EXPECT_EXIT: 7
// (10 - 3) * 2 / 2 = 7 (tests parens and multiple ops)
int main() {
    return (10 - 3) * 2 / 2;
}
