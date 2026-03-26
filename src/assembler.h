/* assembler.h — WAT text to WASM binary assembler */

void assemble_wat(struct ByteVec *input);
void assemble_wat_to_bv(struct ByteVec *input, struct ByteVec *output);
