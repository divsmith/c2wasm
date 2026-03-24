CC       ?= gcc
CFLAGS    = -Wall -Wextra -std=c11 -pedantic -O2
SRC       = src/c2wasm.c
BIN       = ../build/c2wasm
DEMO      = ../demo
WASI_SDK ?= /opt/wasi-sdk

.PHONY: all clean test bootstrap wasm wasm-wasi wasm-emcc serve

all: $(BIN)

$(BIN): $(SRC)
	mkdir -p $(dir $(BIN))
	$(CC) $(CFLAGS) -o $@ $<

test: $(BIN)
	bash tests/run_tests.sh
	@if command -v wat2wasm >/dev/null 2>&1 && command -v wasmtime >/dev/null 2>&1; then \
		echo ""; \
		echo "=== Running bootstrap validation ==="; \
		bash ../tools/bootstrap.sh; \
	else \
		echo ""; \
		echo "Skipping bootstrap (wat2wasm/wasmtime not found)"; \
	fi

bootstrap: $(BIN)
	bash ../tools/bootstrap.sh

# ── WASM build targets ──

wasm-wasi: $(SRC)
	mkdir -p $(DEMO)
	$(WASI_SDK)/bin/clang $(SRC) -O2 -o $(DEMO)/compiler.wasm \
		--sysroot=$(WASI_SDK)/share/wasi-sysroot

wasm-emcc: $(SRC)
	mkdir -p $(DEMO)
	emcc $(SRC) -O2 -o $(DEMO)/compiler.js \
		-s MODULARIZE=1 \
		-s EXPORT_NAME=C2WasmModule \
		-s EXPORTED_RUNTIME_METHODS='["callMain","FS"]' \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s EXIT_RUNTIME=0 \
		-s SINGLE_FILE=1

wasm: wasm-wasi

serve:
	cd $(DEMO) && python3 server.py

clean:
	rm -f $(BIN)
