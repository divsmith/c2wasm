// Reads a name from stdin and prints a greeting.
// Type your name in the terminal below, then press Enter!
int main() {
    char name[64];
    int i;
    int c;
    printf("What is your name? ");
    i = 0;
    while (i < 63) {
        c = getchar();
        if (c == -1 || c == 10) {
            break;
        }
        name[i] = c;
        i = i + 1;
    }
    name[i] = 0;
    printf("Hello, %s!\n", name);
    return 0;
}
