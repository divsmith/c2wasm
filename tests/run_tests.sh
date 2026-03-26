#!/bin/bash
# Test runner for c2wasm compiler
# Each .c file can have:
#   // EXPECT_EXIT: N   — check exit code (default: 0)
#   A matching .expected file — check stdout
# Usage: run_tests.sh [--binary]
set -u

BINARY_MODE=0
if [ "${1:-}" = "--binary" ]; then
    BINARY_MODE=1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ "$BINARY_MODE" -eq 1 ]; then
    COMPILER="$SCRIPT_DIR/../build/c2wasm-bin"
else
    COMPILER="$SCRIPT_DIR/../build/c2wasm"
fi
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
    expected_exit=$(head -1 "$src" | grep -o 'EXPECT_EXIT: [0-9]*' | grep -o '[0-9]*')
    if [ -z "$expected_exit" ]; then expected_exit=0; fi

    # Skip tests marked with SKIP_BINARY in binary mode
    if [ "$BINARY_MODE" -eq 1 ]; then
        skip_binary=$(head -5 "$src" | grep -c 'SKIP_BINARY')
        if [ "$skip_binary" -gt 0 ]; then
            echo "SKIP: $name (SKIP_BINARY)"
            TOTAL=$((TOTAL - 1))
            continue
        fi
    fi

    if [ "$BINARY_MODE" -eq 1 ]; then
        # Compile C → WASM binary directly (from programs dir for #include)
        (cd "$PROG_DIR" && "$COMPILER" < "./${name}.c") > "$TMP_DIR/${name}.wasm" 2>"$TMP_DIR/${name}.compile_err"
        if [ $? -ne 0 ]; then
            echo "FAIL: $name (compile error)"
            cat "$TMP_DIR/${name}.compile_err" | head -5
            FAIL=$((FAIL + 1))
            continue
        fi
    else
        # Compile C → WAT (from programs dir for #include)
        (cd "$PROG_DIR" && "$COMPILER" < "./${name}.c") > "$TMP_DIR/${name}.wat" 2>"$TMP_DIR/${name}.compile_err"
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
    fi

    # Run with wasmtime, capture stdout
    actual_output=$(wasmtime run "$TMP_DIR/${name}.wasm" 2>"$TMP_DIR/${name}.run_err")
    actual_exit=$?

    # Check stdout if .expected file exists
    if [ -f "$PROG_DIR/${name}.expected" ]; then
        expected_output=$(cat "$PROG_DIR/${name}.expected")
        if [ "$actual_output" != "$expected_output" ]; then
            echo "FAIL: $name (output mismatch)"
            diff <(echo "$actual_output") "$PROG_DIR/${name}.expected" | head -10
            FAIL=$((FAIL + 1))
            continue
        fi
    fi

    # Check exit code
    if [ "$actual_exit" -eq "$expected_exit" ]; then
        echo "PASS: $name (exit $actual_exit)"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $name (expected exit $expected_exit, got $actual_exit)"
        FAIL=$((FAIL + 1))
    fi
done

echo ""
echo "Results: $PASS/$TOTAL passed, $FAIL failed"
exit $FAIL
