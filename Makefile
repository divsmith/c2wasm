CC       ?= gcc
CFLAGS    = -Wall -Wextra -std=c11 -pedantic -O2
SRC       = src/c2wasm.c
FILE_IO   = src/file_io.c
CLI_MAIN  = src/cli_main.c
BIN       = build/c2wasm
DEMO      = demo

# Locate wasi-sdk: prefer env override, then /opt, then ~/.local
ifeq ($(WASI_SDK),)
  ifneq ($(wildcard /opt/wasi-sdk/bin/clang),)
    WASI_SDK := /opt/wasi-sdk
  else ifneq ($(wildcard $(HOME)/.local/wasi-sdk/bin/clang),)
    WASI_SDK := $(HOME)/.local/wasi-sdk
  endif
endif

# Include WASM targets in 'all' only when wasi-sdk is available
ifneq ($(WASI_SDK),)
  WASM_TARGETS = $(DEMO)/compiler.wasm $(DEMO)/assembler.wasm
endif

BUNDLE_SOURCES = $(addprefix src/, c2wasm.c constants.h util.c source.c \
  lexer.h lexer.c types.h types.c ast.h ast.c parser.h parser.c \
  codegen.h codegen_shared.c dce.c bytevec.h bytevec.c output.h output.c \
  codegen_wat.c assembler.h assembler.c main.c)

.PHONY: all clean test test-binary test-pipeline bootstrap bootstrap-binary bundle-source serve

all: $(BIN) $(BIN)-asm $(WASM_TARGETS) $(DEMO)/compiler-source.js
ifeq ($(WASI_SDK),)
	@echo ""
	@echo "Note: wasi-sdk not found — skipping demo/compiler.wasm build."
	@echo "      Install it to /opt/wasi-sdk or ~/.local/wasi-sdk to enable."
endif

$(BIN): $(SRC) $(FILE_IO) $(CLI_MAIN)
	mkdir -p $(dir $(BIN))
	$(CC) $(CFLAGS) -Dmain=_c2wasm_main -c $(SRC) -o build/_core.o
	$(CC) $(CFLAGS) -o $@ build/_core.o $(FILE_IO) $(CLI_MAIN)
	@rm -f build/_core.o

$(BIN)-asm: src/assembler_main.c
	mkdir -p $(dir $(BIN))
	$(CC) $(CFLAGS) -Isrc -o $@ $<

$(DEMO)/compiler.wasm: $(SRC) $(FILE_IO) $(CLI_MAIN)
	mkdir -p $(DEMO)
	$(WASI_SDK)/bin/clang -Dmain=_c2wasm_main -c $(SRC) -Isrc -O2 \
		--sysroot=$(WASI_SDK)/share/wasi-sysroot -o build/_wasm_core.o
	$(WASI_SDK)/bin/clang -o $@ \
		build/_wasm_core.o $(FILE_IO) $(CLI_MAIN) -Isrc -O2 \
		--sysroot=$(WASI_SDK)/share/wasi-sysroot
	@rm -f build/_wasm_core.o

$(DEMO)/assembler.wasm: src/assembler_main.c
	mkdir -p $(DEMO)
	$(WASI_SDK)/bin/clang src/assembler_main.c -Isrc -O2 -o $@ \
		--sysroot=$(WASI_SDK)/share/wasi-sysroot

test: $(BIN)
	bash tests/run_tests.sh
	@if command -v wat2wasm >/dev/null 2>&1 && command -v wasmtime >/dev/null 2>&1; then \
		echo ""; \
		echo "=== Running bootstrap validation ==="; \
		bash tools/bootstrap.sh; \
	else \
		echo ""; \
		echo "Skipping bootstrap (wat2wasm/wasmtime not found)"; \
	fi

test-binary: $(BIN)
	bash tests/run_tests.sh --binary
	@if command -v wat2wasm >/dev/null 2>&1 && command -v wasmtime >/dev/null 2>&1; then \
		echo ""; \
		echo "=== Running binary bootstrap validation ==="; \
		bash tools/bootstrap_binary.sh; \
	else \
		echo ""; \
		echo "Skipping binary bootstrap (wat2wasm/wasmtime not found)"; \
	fi

test-pipeline: $(BIN) $(BIN)-asm
	bash tests/run_tests.sh --pipeline

bootstrap: $(BIN)
	bash tools/bootstrap.sh

bootstrap-binary: $(BIN)
	bash tools/bootstrap_binary.sh

bundle-source: $(DEMO)/compiler-source.js

$(DEMO)/compiler-source.js: $(BUNDLE_SOURCES)
	node tools/bundle-source.js

serve:
	cd $(DEMO) && python3 server.py

clean:
	rm -f $(BIN) $(BIN)-asm $(DEMO)/compiler.wasm $(DEMO)/assembler.wasm
