CC     ?= gcc
CFLAGS  = -Wall -Wextra -std=c11 -pedantic -O2
SRC     = src/c2wasm.c
BIN     = ../build/c2wasm
DEMO    = ../demo

.PHONY: all clean test wasm serve bootstrap

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

wasm: $(SRC)
	mkdir -p $(DEMO)
	emcc $(SRC) -O2 -o $(DEMO)/compiler.js \
		-s MODULARIZE=1 \
		-s EXPORT_NAME=C2WasmModule \
		-s EXPORTED_RUNTIME_METHODS='["callMain","FS"]' \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s EXIT_RUNTIME=0 \
		-s SINGLE_FILE=1

serve:
	cd $(DEMO) && python3 -m http.server 8080

clean:
	rm -f $(BIN)
