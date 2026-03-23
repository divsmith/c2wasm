CC     ?= gcc
CFLAGS  = -Wall -Wextra -std=c11 -pedantic -O2
SRC     = src/c2wasm.c
BIN     = c2wasm

.PHONY: all clean test

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

test: $(BIN)
	bash tests/run_tests.sh

clean:
	rm -f $(BIN)
