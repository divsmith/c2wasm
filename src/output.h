/* output.h — WAT output abstraction */

/* When NULL, out_* write to stdout. When set, they append to the buffer. */
struct ByteVec *wat_output;

void out(char *s);
void out_d(int val);
void out_c(int ch);
void out_x(int val);
