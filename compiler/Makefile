CC       ?= gcc
CFLAGS    = -Wall -Wextra -std=c11 -pedantic -O2
SRC       = src/c2wasm.c
BIN       = ../build/c2wasm
DEMO      = ../demo
WASI_SDK ?= /opt/wasi-sdk

.PHONY: all clean test test-node test-sh bootstrap wasm wasm-wasi serve

all: $(BIN)

$(BIN): $(SRC)
	mkdir -p $(dir $(BIN))
	$(CC) $(CFLAGS) -o $@ $<

# ── Test targets ──
# Prefer Node.js runner (only needs npm install); fall back to shell + native tools.

test: $(BIN)
	@if command -v node >/dev/null 2>&1 && [ -d ../node_modules/wabt ]; then \
		node tests/run_tests.js; \
		echo ""; \
		echo "=== Running bootstrap validation ==="; \
		node ../tools/bootstrap.js; \
	elif command -v wat2wasm >/dev/null 2>&1 && command -v wasmtime >/dev/null 2>&1; then \
		bash tests/run_tests.sh; \
		echo ""; \
		echo "=== Running bootstrap validation ==="; \
		bash ../tools/bootstrap.sh; \
	else \
		echo "Error: need either (node + npm install) or (wat2wasm + wasmtime)"; \
		exit 1; \
	fi

test-node: $(BIN)
	node tests/run_tests.js

test-sh: $(BIN)
	bash tests/run_tests.sh

bootstrap: $(BIN)
	@if command -v node >/dev/null 2>&1 && [ -d ../node_modules/wabt ]; then \
		node ../tools/bootstrap.js; \
	else \
		bash ../tools/bootstrap.sh; \
	fi

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
