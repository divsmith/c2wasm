/* output.c — WAT output abstraction implementation */

void out(char *s) {
    if (wat_output) {
        while (*s) {
            bv_push(wat_output, *s);
            s = s + 1;
        }
    } else {
        printf("%s", s);
    }
}

void out_d(int val) {
    char buf[12];
    int i;
    int neg;
    int d;
    if (!wat_output) {
        printf("%d", val);
        return;
    }
    if (val == 0) {
        bv_push(wat_output, '0');
        return;
    }
    neg = 0;
    if (val < 0) {
        neg = 1;
        /* Handle INT_MIN: -2147483648 / 10 works in C89 */
        val = -val;
        if (val < 0) {
            /* INT_MIN edge: write it literally */
            out("-2147483648");
            return;
        }
    }
    i = 0;
    while (val > 0) {
        d = val % 10;
        buf[i] = (char)('0' + d);
        i = i + 1;
        val = val / 10;
    }
    if (neg) {
        bv_push(wat_output, '-');
    }
    while (i > 0) {
        i = i - 1;
        bv_push(wat_output, buf[i]);
    }
}

void out_c(int ch) {
    if (wat_output) {
        bv_push(wat_output, ch);
    } else {
        putchar(ch);
    }
}

void out_x(int val) {
    char hx[9];
    int i;
    int d;
    if (!wat_output) {
        printf("%x", val);
        return;
    }
    if (val == 0) {
        bv_push(wat_output, '0');
        return;
    }
    i = 0;
    while (val > 0 && i < 8) {
        d = val & 0xF;
        if (d < 10) {
            hx[i] = (char)('0' + d);
        } else {
            hx[i] = (char)('a' + d - 10);
        }
        i = i + 1;
        val = val >> 4;
    }
    while (i > 0) {
        i = i - 1;
        bv_push(wat_output, hx[i]);
    }
}
