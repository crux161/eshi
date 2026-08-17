#!/bin/sh
# Asserts that every gallery shader reaches the GPU backend instead of falling
# back to the CPU renderer.
#
# The fallback added in renderer_gl.h is deliberately quiet at runtime — a
# shader the backend cannot build should still produce a video rather than
# black frames or a crash. That is right for users and useless for CI, which
# needs the failure to be loud. `--validate` bridges the two: it initializes
# the renderer, reports which backend came up, and exits non-zero when a
# requested GPU backend fell through.
#
# Usage:
#   scripts/check_gpu_shaders.sh [bin-dir]
#
# Environment:
#   ESHI_VALIDATE_ARGS  Extra arguments, e.g. "--res 64x64".
set -eu

BIN_DIR="${1:-zig-out/bin}"
EXTRA_ARGS="${ESHI_VALIDATE_ARGS:---res 64x64}"

if [ ! -d "$BIN_DIR" ]; then
    echo "no such bin directory: $BIN_DIR" >&2
    echo "build first, e.g. 'zig build examples -Dopengl=true'" >&2
    exit 2
fi

passed=0
failed=0
missing=0
failed_names=""

# The gallery is authored in both C++ and Zig. The Zig example still produces
# a normal executable with a portable GPU companion, so validate it by binary
# name exactly like the C++ examples instead of silently leaving it outside the
# gate.
for source in examples/*.cpp examples/*.zig; do
    [ -e "$source" ] || continue
    name=$(basename "$source")
    name=${name%.*}
    binary="$BIN_DIR/$name"

    if [ ! -x "$binary" ]; then
        echo "  skip $name (not built)"
        missing=$((missing + 1))
        continue
    fi

    # Sequential on purpose.
    #
    # Running these concurrently makes unrelated shaders report empty output
    # and appear to fail: twenty simultaneous GL contexts contend badly enough
    # to look like compile errors. Six shaders "failed" that way during
    # development and all passed when run one at a time. Do not parallelize
    # this loop to speed it up — you will be debugging phantoms.
    status=0
    output=$("$binary" --gpu --validate $EXTRA_ARGS 2>&1) || status=$?

    # Require the marker, not just a zero exit.
    #
    # A binary predating --validate ignores the unknown flag, renders its full
    # 240 frames and exits 0 — which an exit-code-only check reports as a pass.
    # A stale build directory then produces a green run that proves nothing.
    # Demanding the line the flag is supposed to print closes that hole.
    backend=$(printf '%s\n' "$output" | sed -n 's/^\[validate\] ok [^:]*: backend=//p')

    if [ "$status" -eq 0 ] && [ -n "$backend" ]; then
        echo "  ok   $name ($backend)"
        passed=$((passed + 1))
    elif [ "$status" -eq 0 ]; then
        echo "  FAIL $name: no [validate] output — binary predates --validate?"
        failed=$((failed + 1))
        failed_names="$failed_names $name"
    else
        echo "  FAIL $name"
        printf '%s\n' "$output" | grep -iE 'error' | head -3 | sed 's/^/         /'
        failed=$((failed + 1))
        failed_names="$failed_names $name"
    fi

    # main.cpp names its output after argv[0]; --validate exits before
    # encoding, but clean up anyway in case that changes.
    rm -f "$name.mp4"
done

echo
echo "passed=$passed failed=$failed skipped=$missing"

if [ "$failed" -ne 0 ]; then
    echo "shaders that did not reach the GPU backend:$failed_names" >&2
    exit 1
fi

if [ "$passed" -eq 0 ]; then
    echo "no shaders were validated — is $BIN_DIR populated?" >&2
    exit 2
fi
