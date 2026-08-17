#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "$0")/.." && pwd)
temp_dir=$(mktemp -d "${TMPDIR:-/tmp}/larimar-sanitize.XXXXXX")
trap 'rm -rf "$temp_dir"' EXIT

cxx=${CXX:-clang++}

"$cxx" -std=c++11 -O1 -g -fno-omit-frame-pointer \
  -fsanitize=address,undefined -fno-sanitize-recover=all \
  -Wall -Wextra -I"$repo_dir/core/include" \
  "$repo_dir/core/src/abi.cpp" \
  "$repo_dir/core/src/world.cpp" \
  "$repo_dir/core/src/scene.cpp" \
  "$repo_dir/core/src/render/registry.cpp" \
  "$repo_dir/core/src/render/transpile.cpp" \
  "$repo_dir/core/src/render/ink.cpp" \
  "$repo_dir/core/tests/test_core.cpp" \
  -o "$temp_dir/eshi-core-sanitized"

cd "$repo_dir"
"$temp_dir/eshi-core-sanitized"
