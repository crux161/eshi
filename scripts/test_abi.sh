#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "$0")/.." && pwd)
temp_dir=$(mktemp -d "${TMPDIR:-/tmp}/larimar-abi.XXXXXX")
trap 'rm -rf "$temp_dir"' EXIT

cxx=${CXX:-clang++}
cc=${CC:-clang}

case "$(uname -s)" in
  Darwin)
    library="$temp_dir/libeshi_core.dylib"
    shared_flags=(-dynamiclib)
    ;;
  Linux)
    library="$temp_dir/libeshi_core.so"
    shared_flags=(-shared)
    ;;
  *)
    echo "unsupported ABI-test host: $(uname -s)" >&2
    exit 2
    ;;
esac

"$cxx" -std=c++11 -O2 -fPIC -Wall -Wextra "${shared_flags[@]}" \
  -I"$repo_dir/core/include" \
  "$repo_dir/core/src/abi.cpp" \
  "$repo_dir/core/src/world.cpp" \
  "$repo_dir/core/src/scene.cpp" \
  "$repo_dir/core/src/render/registry.cpp" \
  "$repo_dir/core/src/render/ink.cpp" \
  -o "$library"

"$cc" -std=c99 -pedantic-errors -Wall -Wextra -Werror \
  -I"$repo_dir/core/include" \
  "$repo_dir/core/tests/test_abi.c" \
  "$library" \
  -o "$temp_dir/test-abi"

"$temp_dir/test-abi"
