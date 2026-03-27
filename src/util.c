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

/* Build mangled name: __static_{func}_{var} */
char *mangle_static(char *func, char *var) {
    char *r;
    int fl;
    int vl;
    int i;
    int j;
    fl = 0;
    while (func[fl]) fl++;
    vl = 0;
    while (var[vl]) vl++;
    r = (char *)malloc(9 + fl + 1 + vl + 1);
    j = 0;
    r[j] = '_'; j++;
    r[j] = '_'; j++;
    r[j] = 's'; j++;
    r[j] = 't'; j++;
    r[j] = 'a'; j++;
    r[j] = 't'; j++;
    r[j] = 'i'; j++;
    r[j] = 'c'; j++;
    r[j] = '_'; j++;
    for (i = 0; i < fl; i++) { r[j] = func[i]; j++; }
    r[j] = '_'; j++;
    for (i = 0; i < vl; i++) { r[j] = var[i]; j++; }
    r[j] = 0;
    return r;
}

