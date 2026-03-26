/*
 * file_io.c — Native file I/O for #include support
 *
 * Provides __open_file, __read_file, __close_file using POSIX/C stdio.
 * These are linked into the native (GCC) build of c2wasm.
 * When c2wasm compiles itself to WASM, these are replaced by WAT
 * helper functions that call WASI path_open/fd_read/fd_close.
 */

#include <stdio.h>
#include <string.h>

static FILE *fd_table[64];

int __open_file(char *path, int path_len) {
    char buf[1024];
    FILE *f;
    int fd;
    if (path_len >= 1024) path_len = 1023;
    memcpy(buf, path, path_len);
    buf[path_len] = '\0';
    f = fopen(buf, "r");
    if (!f) return -1;
    /* find a free slot, reusing closed entries */
    for (fd = 4; fd < 64; fd++) {
        if (!fd_table[fd]) {
            fd_table[fd] = f;
            return fd;
        }
    }
    fclose(f);
    return -1;
}

int __read_file(int fd, char *buf, int max_len) {
    if (fd < 4 || fd >= 64 || !fd_table[fd]) return -1;
    return (int)fread(buf, 1, max_len, fd_table[fd]);
}

void __close_file(int fd) {
    if (fd >= 4 && fd < 64 && fd_table[fd]) {
        fclose(fd_table[fd]);
        fd_table[fd] = NULL;
    }
}
