/* bytevec.h — Growable byte buffer */

struct ByteVec {
    char *data;
    int len;
    int cap;
};

void bv_init(struct ByteVec *v, int cap);
struct ByteVec *bv_new(int cap);
void bv_reset(struct ByteVec *v);
void bv_push(struct ByteVec *v, int b);
void bv_append(struct ByteVec *dst, struct ByteVec *src);
void bv_u32(struct ByteVec *v, int val);
void bv_i32(struct ByteVec *v, int val);
void bv_name(struct ByteVec *v, char *s, int slen);
void bv_section(struct ByteVec *dst, int id, struct ByteVec *content);
