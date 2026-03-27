/* assembler_main.c — Standalone WAT-to-WASM assembler
 *
 * Reads WAT text from stdin and writes WASM binary to stdout.
 * Used in the browser demo to convert custom compiler WAT output to
 * runnable WASM binary. Built with wasi-sdk → demo/assembler.wasm.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *strdupn(char *s, int maxlen) {
    char *dst;
    dst = (char *)malloc(maxlen + 1);
    strncpy(dst, s, maxlen);
    dst[maxlen] = '\0';
    return dst;
}

#include "bytevec.h"
#include "bytevec.c"
#include "assembler.h"
#include "assembler.c"

int main() {
    struct ByteVec *input;
    int ch;

    input = bv_new(65536);
    while ((ch = getchar()) != -1) {
        bv_push(input, ch);
    }

    if (input->len == 0) {
        printf("Error: no input\n");
        return 1;
    }

    assemble_wat(input);
    return 0;
}
