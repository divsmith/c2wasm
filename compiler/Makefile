CC     ?= gcc
CFLAGS  = -Wall -Wextra -std=c11 -pedantic -O2
SRC     = src/c2wasm.c
BIN     = c2wasm
DEMO   = ../demo

.PHONY: all clean test wasm serve

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

test: $(BIN)
	bash tests/run_tests.sh

wasm: $(SRC)
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
