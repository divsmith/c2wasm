#!/bin/bash
# Test runner for c2wasm compiler
# Each .c file has a first-line comment: // EXPECT_EXIT: N
set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
COMPILER="$SCRIPT_DIR/../c2wasm"
PROG_DIR="$SCRIPT_DIR/programs"
TMP_DIR="${TMPDIR:-/tmp}/c2wasm-tests"
mkdir -p "$TMP_DIR"

PASS=0
FAIL=0
TOTAL=0

for src in "$PROG_DIR"/*.c; do
    name=$(basename "$src" .c)
    TOTAL=$((TOTAL + 1))

    # Extract expected exit code from first line
    expected=$(head -1 "$src" | grep -o 'EXPECT_EXIT: [0-9]*' | grep -o '[0-9]*')
    if [ -z "$expected" ]; then expected=0; fi

    # Compile C → WAT
    "$COMPILER" < "$src" > "$TMP_DIR/${name}.wat" 2>"$TMP_DIR/${name}.compile_err"
    if [ $? -ne 0 ]; then
        echo "FAIL: $name (compile error)"
        cat "$TMP_DIR/${name}.compile_err" | head -5
        FAIL=$((FAIL + 1))
        continue
    fi

    # Assemble WAT → WASM
    wat2wasm "$TMP_DIR/${name}.wat" -o "$TMP_DIR/${name}.wasm" 2>"$TMP_DIR/${name}.asm_err"
    if [ $? -ne 0 ]; then
        echo "FAIL: $name (wat2wasm error)"
        cat "$TMP_DIR/${name}.asm_err" | head -5
        FAIL=$((FAIL + 1))
        continue
    fi

    # Run with wasmtime
    wasmtime run "$TMP_DIR/${name}.wasm" 2>"$TMP_DIR/${name}.run_err"
    actual=$?

    if [ "$actual" -eq "$expected" ]; then
        echo "PASS: $name (exit $actual)"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $name (expected exit $expected, got $actual)"
        FAIL=$((FAIL + 1))
    fi
done

echo ""
echo "Results: $PASS/$TOTAL passed, $FAIL failed"
exit $FAIL
