/* ================================================================
 * Source buffer — read entire stdin via getchar loop
 * ================================================================ */

char *src;
int src_len;

void read_source(void) {
    int c;
    int cap;
    src = (char *)malloc(MAX_SRC);
    src_len = 0;
    cap = MAX_SRC;
    c = getchar();
    while (c != -1) {
        if (src_len >= cap - 1) {
            break;
        }
        src[src_len] = (char)c;
        src_len++;
        c = getchar();
    }
    src[src_len] = '\0';
}

