// EXPECT_EXIT: 42
/* regression: DCE must follow calls from function definitions, not just prototypes */

int helper1(void);
int helper2(void);

int main() {
    return helper1();
}

int helper1(void) {
    return helper2();
}

int helper2(void) {
    return 42;
}
