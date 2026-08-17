#!/usr/bin/env bash
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
package_root="$repository_root/bindings/dart/larimar"
dart_command="${DART_BIN:-dart}"

cd "$package_root"
"$dart_command" run ffigen --config ffigen.yaml
git -C "$repository_root" diff --exit-code -- \
  bindings/dart/larimar/lib/src/generated/larimar_bindings_generated.dart
