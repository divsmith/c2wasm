#!/bin/bash
set -eo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/src/c2wasm.c"
BIN="$ROOT/build/c2wasm"
TMP="$ROOT/.tmp"

mkdir -p "$TMP"

echo "=== Binary self-hosting bootstrap ==="

echo "[1/4] Building native compiler..."
(cd "$ROOT" && make clean && make) >/dev/null 2>&1

echo "[2/4] Stage 1: native compiler -> s1.wat -> s1.wasm"
(cd "$ROOT/src" && "$BIN" < c2wasm.c > "$TMP/s1-binary.wat")
wat2wasm "$TMP/s1-binary.wat" -o "$TMP/s1-binary.wasm"
echo "       $(wc -l < "$TMP/s1-binary.wat") lines of WAT"

echo "[3/4] Stage 2: WASM compiler -> s2.wasm via -b"
(cd "$ROOT/src" && wasmtime --dir=. "$TMP/s1-binary.wasm" -- -b < c2wasm.c > "$TMP/s2-binary.wasm")
echo "       $(wc -c < "$TMP/s2-binary.wasm") bytes of WASM"

echo "[4/4] Smoke test: self-hosted binary compiler -> 01_return42.wasm"
(cd "$ROOT" && wasmtime --dir=src --dir=tests "$TMP/s2-binary.wasm" -- -b < tests/programs/01_return42.c > "$TMP/01_return42-selfhost.wasm")
set +e
wasmtime run "$TMP/01_return42-selfhost.wasm" >/dev/null
EXIT_CODE=$?
set -e
if [ "$EXIT_CODE" -ne 42 ]; then
    echo "       FAIL: expected exit 42, got $EXIT_CODE"
    exit 1
fi
echo "       PASS: 01_return42.wasm exits 42"

echo ""
echo "=== Binary bootstrap successful! ==="
