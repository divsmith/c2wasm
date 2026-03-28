// String Arrays
// Features: global char* array literals, string functions

char *colors[] = {"red", "green", "blue", "yellow", "purple"};
char *sizes[]  = {"small", "medium", "large"};

int main() {
    int i;
    int j;
    printf("Colors:\n");
    for (i = 0; i < 5; i = i + 1) {
        printf("  [%d] %s (len=%d)\n", i, colors[i], strlen(colors[i]));
    }
    printf("\nSizes:\n");
    for (i = 0; i < 3; i = i + 1) {
        printf("  [%d] %s\n", i, sizes[i]);
    }
    printf("\nSome combinations:\n");
    for (i = 0; i < 3; i = i + 1) {
        for (j = 0; j < 3; j = j + 1) {
            printf("  %s %s\n", sizes[i], colors[j]);
        }
    }
    return 0;
}
