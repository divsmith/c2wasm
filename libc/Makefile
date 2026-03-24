# Build libc.wasm using wasi-sdk
# Usage: make [WASI_SDK=/path/to/wasi-sdk]

# Auto-detect wasi-sdk location
WASI_SDK ?= $(shell \
  if [ -d "/opt/wasi-sdk" ]; then echo "/opt/wasi-sdk"; \
  elif [ -d "$(HOME)/.local/wasi-sdk" ]; then echo "$(HOME)/.local/wasi-sdk"; \
  else echo ""; fi)

CC = $(WASI_SDK)/bin/clang
SYSROOT = $(WASI_SDK)/share/wasi-sysroot

CFLAGS = -O2 --target=wasm32-wasi \
         --sysroot=$(SYSROOT) \
         -nostdlib \
         -Wno-incompatible-library-redeclaration \
         -Wl,--no-entry \
         -Wl,--export-all \
         -Wl,--allow-undefined \
         -mbulk-memory

OUTPUT = ../demo/libc.wasm

all: $(OUTPUT)

$(OUTPUT): libc.c
	@if [ -z "$(WASI_SDK)" ]; then \
		echo "Error: wasi-sdk not found. Set WASI_SDK=/path/to/wasi-sdk"; \
		exit 1; \
	fi
	@mkdir -p ../demo
	$(CC) $(CFLAGS) -o $@ $<
	@echo "Built $@ ($$(wc -c < $@ | tr -d ' ') bytes)"

clean:
	rm -f $(OUTPUT)

.PHONY: all clean
