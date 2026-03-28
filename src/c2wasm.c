/*
 * c2wasm.c — C-to-WAT compiler (self-hosting version)
 *
 * Reads C source from stdin, emits WebAssembly Text Format to stdout.
 * Written using ONLY the C subset that this compiler supports.
 *
 * Build:  make
 * Usage:  ./build/c2wasm < program.c > program.wat
 *
 * Unity build: this file #includes the entire compiler.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int binary_mode = 0;
int debug_mode = 0;

#include "constants.h"
#include "util.c"
#include "source.c"
#include "lexer.h"
#include "lexer.c"
#include "types.h"
#include "types.c"
#include "ast.h"
#include "ast.c"
#include "parser.h"
#include "parser.c"
#include "codegen.h"
#include "codegen_shared.c"
#include "bytevec.h"
#include "bytevec.c"
#include "output.h"
#include "output.c"
#include "codegen_wat.c"
#include "assembler.h"
#include "assembler.c"
#include "main.c"
