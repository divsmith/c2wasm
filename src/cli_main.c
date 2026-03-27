/* cli_main.c — Native/WASI CLI entry point
 *
 * Provides main(int argc, char **argv) with flag parsing.
 * The unity build's main(void) is renamed to _c2wasm_main via
 * -Dmain=_c2wasm_main at compile time.
 *
 * NOT part of the unity build — self-compiled WASM uses main(void) directly.
 */

#include <stdio.h>
#include <string.h>

extern int binary_mode;
int _c2wasm_main(void);

int main(int argc, char **argv) {
    int i;
    binary_mode = 0;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-b") == 0) {
            binary_mode = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: c2wasm [options] < input.c\n");
            printf("  -b       Output WASM binary instead of WAT text\n");
            printf("  -h       Show this help message\n");
            return 0;
        } else {
            printf("Unknown option: %s\n", argv[i]);
            printf("Usage: c2wasm [options] < input.c\n");
            return 1;
        }
    }
    return _c2wasm_main();
}
