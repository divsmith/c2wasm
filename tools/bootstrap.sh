#!/bin/bash
set -eo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/src/c2wasm.c"
BIN="$ROOT/build/c2wasm"
TMP="$ROOT/.tmp"

mkdir -p "$TMP"

echo "=== Self-hosting bootstrap ==="

# Build native compiler
echo "[1/5] Building native compiler..."
(cd "$ROOT" && make clean && make) >/dev/null 2>&1

# Stage 1: native compiler compiles itself
echo "[2/5] Stage 1: native compiler -> s1.wat"
"$BIN" < "$SRC" > "$TMP/s1.wat"
wat2wasm "$TMP/s1.wat" -o "$TMP/s1.wasm"
echo "       $(wc -l < "$TMP/s1.wat") lines of WAT"

# Stage 2: WASM compiler compiles itself
echo "[3/5] Stage 2: WASM compiler -> s2.wat"
wasmtime "$TMP/s1.wasm" < "$SRC" > "$TMP/s2.wat"
echo "       $(wc -l < "$TMP/s2.wat") lines of WAT"

# Verify fixed point
echo "[4/5] Verifying fixed point (s1.wat == s2.wat)..."
if diff -q "$TMP/s1.wat" "$TMP/s2.wat" >/dev/null 2>&1; then
    echo "       PASS: s1.wat and s2.wat are identical"
else
    echo "       FAIL: s1.wat and s2.wat differ!"
    diff "$TMP/s1.wat" "$TMP/s2.wat" | head -20
    exit 1
fi

# Run test suite
echo "[5/5] Running test suite..."
bash "$ROOT/tests/run_tests.sh" 2>&1 | tail -1

echo ""
echo "=== Bootstrap successful! ==="
