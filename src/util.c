/* ================================================================
 * Error reporting
 * ================================================================ */

void error(int line, int col, char *msg) {
    printf("%d:%d: error: %s\n", line, col, msg);
    exit(1);
}

char *strdupn(char *s, int maxlen) {
    char *dst;
    dst = (char *)malloc(maxlen + 1);
    strncpy(dst, s, maxlen);
    dst[maxlen] = '\0';
    return dst;
}

