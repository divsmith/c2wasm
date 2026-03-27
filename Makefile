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

.PHONY: all clean test test-binary test-pipeline bootstrap wasm wasm-wasi wasm-assembler serve

all: $(BIN)

$(BIN): $(SRC) $(FILE_IO) $(CLI_MAIN)
	mkdir -p $(dir $(BIN))
	$(CC) $(CFLAGS) -Dmain=_c2wasm_main -c $(SRC) -o build/_core.o
	$(CC) $(CFLAGS) -o $@ build/_core.o $(FILE_IO) $(CLI_MAIN)
	@rm -f build/_core.o

$(BIN)-asm: src/assembler_main.c
	mkdir -p $(dir $(BIN))
	$(CC) $(CFLAGS) -Isrc -o $@ $<

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

test-pipeline: $(BIN) $(BIN)-asm
	bash tests/run_tests.sh --pipeline

bootstrap: $(BIN)
	bash tools/bootstrap.sh

# ── WASM build targets ──

wasm-wasi: $(SRC) $(CLI_MAIN)
	@if [ -z "$(WASI_SDK)" ]; then \
		echo "Error: wasi-sdk not found. Install it and set WASI_SDK=/path/to/wasi-sdk, or place it at /opt/wasi-sdk or ~/.local/wasi-sdk"; \
		exit 1; \
	fi
	mkdir -p $(DEMO)
	$(WASI_SDK)/bin/clang -Dmain=_c2wasm_main -c $(SRC) -Isrc -O2 \
		--sysroot=$(WASI_SDK)/share/wasi-sysroot -o build/_wasm_core.o
	$(WASI_SDK)/bin/clang -o $(DEMO)/compiler.wasm \
		build/_wasm_core.o $(FILE_IO) $(CLI_MAIN) -Isrc -O2 \
		--sysroot=$(WASI_SDK)/share/wasi-sysroot
	@rm -f build/_wasm_core.o

wasm: wasm-wasi wasm-assembler

wasm-assembler: src/assembler_main.c
	@if [ -z "$(WASI_SDK)" ]; then \
		echo "Error: wasi-sdk not found. Install it and set WASI_SDK=/path/to/wasi-sdk, or place it at /opt/wasi-sdk or ~/.local/wasi-sdk"; \
		exit 1; \
	fi
	mkdir -p $(DEMO)
	$(WASI_SDK)/bin/clang src/assembler_main.c -Isrc -O2 -o $(DEMO)/assembler.wasm \
		--sysroot=$(WASI_SDK)/share/wasi-sysroot

bundle-source:
	node tools/bundle-source.js

serve:
	cd $(DEMO) && python3 server.py

clean:
	rm -f $(BIN) $(BIN)-asm
