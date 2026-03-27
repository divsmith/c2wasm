CC       ?= gcc
CFLAGS    = -Wall -Wextra -std=c11 -pedantic -O2
SRC       = src/c2wasm.c
FILE_IO   = src/file_io.c
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

.PHONY: all clean test test-binary bootstrap wasm wasm-wasi wasm-wat wasm-emcc serve

all: $(BIN)

$(BIN): $(SRC) $(FILE_IO)
	mkdir -p $(dir $(BIN))
	$(CC) $(CFLAGS) -o $@ $(SRC) $(FILE_IO)

$(BIN)-bin: $(SRC) $(FILE_IO)
	mkdir -p $(dir $(BIN))
	sed 's/int binary_mode = 0;/int binary_mode = 1;/' < $(SRC) | $(CC) $(CFLAGS) -Isrc -o $@ -x c - $(FILE_IO)

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

test-binary: $(BIN)-bin
	bash tests/run_tests.sh --binary

bootstrap: $(BIN)
	bash tools/bootstrap.sh

# ── WASM build targets ──

wasm-wasi: $(SRC)
	@if [ -z "$(WASI_SDK)" ]; then \
		echo "Error: wasi-sdk not found. Install it and set WASI_SDK=/path/to/wasi-sdk, or place it at /opt/wasi-sdk or ~/.local/wasi-sdk"; \
		exit 1; \
	fi
	mkdir -p $(DEMO)
	sed 's/int binary_mode = 0;/int binary_mode = 1;/' < $(SRC) | \
		$(WASI_SDK)/bin/clang -x c - $(FILE_IO) -Isrc -O2 -o $(DEMO)/compiler.wasm \
		--sysroot=$(WASI_SDK)/share/wasi-sysroot

wasm-wat: $(SRC)
	@if [ -z "$(WASI_SDK)" ]; then \
		echo "Error: wasi-sdk not found. Install it and set WASI_SDK=/path/to/wasi-sdk, or place it at /opt/wasi-sdk or ~/.local/wasi-sdk"; \
		exit 1; \
	fi
	mkdir -p $(DEMO)
	$(WASI_SDK)/bin/clang $(SRC) $(FILE_IO) -O2 -o $(DEMO)/compiler.wasm \
		--sysroot=$(WASI_SDK)/share/wasi-sysroot

wasm-emcc: $(SRC)
	mkdir -p $(DEMO)
	emcc $(SRC) $(FILE_IO) -O2 -o $(DEMO)/compiler.js \
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
	rm -f $(BIN) $(BIN)-bin
