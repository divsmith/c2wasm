// Collatz sequence (3n+1 problem)
// Features: do-while, ternary (?:), post-increment (count++)

int steps(int n) {
    int count = 0;
    do {
        n = (n % 2 == 0) ? n / 2 : n * 3 + 1;
        count++;
    } while (n != 1);
    return count;
}

int main() {
    int n;
    int s;
    int max_steps = 0;
    int max_n = 0;
    for (n = 1; n <= 20; n++) {
        s = steps(n);
        printf("collatz(%d) = %d steps\n", n, s);
        if (s > max_steps) {
            max_steps = s;
            max_n = n;
        }
    }
    printf("Longest: %d with %d steps\n", max_n, max_steps);
    return 0;
}
