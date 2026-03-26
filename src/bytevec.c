/* bytevec.c — Growable byte buffer implementation */

void bv_init(struct ByteVec *v, int cap) {
    v->data = (char *)malloc(cap);
    v->len = 0;
    v->cap = cap;
}

struct ByteVec *bv_new(int cap) {
    struct ByteVec *v;
    v = (struct ByteVec *)malloc(sizeof(struct ByteVec));
    bv_init(v, cap);
    return v;
}

void bv_reset(struct ByteVec *v) {
    v->len = 0;
}

void bv_push(struct ByteVec *v, int b) {
    char *nd;
    if (v->len >= v->cap) {
        nd = (char *)malloc(v->cap * 2);
        memcpy(nd, v->data, v->len);
        v->data = nd;
        v->cap = v->cap * 2;
    }
    v->data[v->len] = (char)(b & 0xFF);
    v->len++;
}

void bv_append(struct ByteVec *dst, struct ByteVec *src) {
    int i;
    for (i = 0; i < src->len; i++) {
        bv_push(dst, src->data[i] & 0xFF);
    }
}

void bv_u32(struct ByteVec *v, int val) {
    int b;
    if (val < 0) val = 0;
    do {
        b = val & 0x7F;
        val = val >> 7;
        if (val != 0) b = b | 0x80;
        bv_push(v, b);
    } while (val != 0);
}

void bv_i32(struct ByteVec *v, int val) {
    int b;
    int more;
    more = 1;
    while (more) {
        b = val & 0x7F;
        val = val >> 7;
        if ((val == 0 && (b & 0x40) == 0) || (val == -1 && (b & 0x40) != 0)) {
            more = 0;
        } else {
            b = b | 0x80;
        }
        bv_push(v, b);
    }
}

void bv_name(struct ByteVec *v, char *s, int slen) {
    int i;
    bv_u32(v, slen);
    for (i = 0; i < slen; i++) {
        bv_push(v, s[i] & 0xFF);
    }
}

void bv_section(struct ByteVec *dst, int id, struct ByteVec *content) {
    bv_push(dst, id);
    bv_u32(dst, content->len);
    bv_append(dst, content);
}
