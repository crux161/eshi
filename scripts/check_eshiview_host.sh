#!/usr/bin/env bash
# Drives the macOS EshiView texture host under leak and race diagnostics.
#
# Two passes over the same harness, because the two questions need different
# builds: an uninstrumented binary under `leaks --atExit`, which reports every
# surface the adapter failed to release, and a ThreadSanitizer build, which
# reports every unsynchronised access between the platform thread and the raster
# queue. See bindings/dart/larimar/macos/diagnostics/texture_host_main.mm.
#
# The engine-level half of this gate lives in check_eshiview_lifecycle.sh.
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
plugin_root="$repository_root/bindings/dart/larimar/macos"
artifact_dir="${LARIMAR_ARTIFACTS:-$repository_root/build/larimar-diagnostics}"
cxx="${CXX:-clang++}"
cycles="${LARIMAR_HOST_CYCLES:-24}"

if [ "$(uname -s)" != "Darwin" ]; then
  echo "check_eshiview_host.sh: the EshiView texture host is macOS-only." >&2
  exit 1
fi

# The harness links the same engine framework the plugin compiles against. A
# fresh checkout may not have it yet, so precache before giving up: an absent
# framework is a setup problem, not a diagnostic result, and must not be
# reported as either a pass or a failure.
flutter_root="${FLUTTER_ROOT:-}"
if [ -z "$flutter_root" ] && command -v flutter >/dev/null 2>&1; then
  flutter_bin="$(command -v flutter)"
  while [ -L "$flutter_bin" ]; do flutter_bin="$(readlink "$flutter_bin")"; done
  flutter_root="$(cd "$(dirname "$flutter_bin")/.." && pwd)"
fi

# The copy embedded in a built .app is stripped of its headers, so only the
# SDK's xcframework can be compiled against.
sdk_framework="$flutter_root/bin/cache/artifacts/engine/darwin-x64/FlutterMacOS.xcframework/macos-arm64_x86_64"
framework_dir=""
if [ -d "$sdk_framework/FlutterMacOS.framework/Headers" ]; then
  framework_dir="$sdk_framework"
elif command -v flutter >/dev/null 2>&1; then
  flutter precache --macos >/dev/null
  [ -d "$sdk_framework/FlutterMacOS.framework/Headers" ] && framework_dir="$sdk_framework"
fi

if [ -z "$framework_dir" ]; then
  echo "check_eshiview_host.sh: FlutterMacOS.framework not found; run 'flutter precache --macos'." >&2
  exit 1
fi

mkdir -p "$artifact_dir"
build_dir=$(mktemp -d "${TMPDIR:-/tmp}/larimar-eshiview.XXXXXX")
trap 'rm -rf "$build_dir"' EXIT

build_harness() {
  "$cxx" -std=c++11 -fobjc-arc -O1 -g -fno-omit-frame-pointer \
    -Wall -Wextra -Wno-nullability-completeness \
    -I"$plugin_root/larimar/Sources/larimar/include" \
    -F"$framework_dir" -Wl,-rpath,"$framework_dir" \
    -framework FlutterMacOS -framework Foundation -framework CoreVideo \
    -framework Metal -framework IOSurface -framework CoreGraphics -framework ImageIO \
    "$plugin_root/larimar/Sources/larimar/LarimarPlugin.mm" \
    "$plugin_root/diagnostics/texture_host_main.mm" \
    "$@"
}

leak_report="$artifact_dir/eshiview-host-leaks.txt"
race_report="$artifact_dir/eshiview-host-race.txt"

build_harness -o "$build_dir/texture_host"
build_harness -fsanitize=thread -o "$build_dir/texture_host_tsan"

# Pass 1. `leaks --atExit` reports what the process still owns when it exits, so
# a surface the adapter forgot to release shows up as a rooted CVPixelBuffer.
set +e
LARIMAR_HOST_CYCLES="$cycles" leaks --atExit -- "$build_dir/texture_host" \
  2>&1 | tee "$leak_report"
leak_status=${PIPESTATUS[0]}
set -e
if [ "$leak_status" -ne 0 ]; then
  echo "check_eshiview_host.sh: the texture host leaked; see $leak_report" >&2
  exit 1
fi
# `leaks --atExit` reports its own verdict and discards the child's exit code,
# so the harness's lifecycle verdict has to be read out of the report.
if ! grep -qE '^\[host\] status=(ok|skipped-no-metal)$' "$leak_report"; then
  echo "check_eshiview_host.sh: the harness did not report a clean lifecycle; see $leak_report" >&2
  exit 1
fi

# Pass 2. Same cycles, this time with the platform thread and the raster queue
# under ThreadSanitizer.
set +e
LARIMAR_HOST_CYCLES="$cycles" \
  TSAN_OPTIONS="halt_on_error=0 exitcode=66 second_deadlock_stack=1" \
  "$build_dir/texture_host_tsan" 2>&1 | tee "$race_report"
race_status=${PIPESTATUS[0]}
set -e
if [ "$race_status" -eq 66 ]; then
  echo "check_eshiview_host.sh: ThreadSanitizer reported a data race; see $race_report" >&2
  exit 1
fi
if [ "$race_status" -ne 0 ]; then
  echo "check_eshiview_host.sh: the sanitized harness failed; see $race_report" >&2
  exit "$race_status"
fi

echo "check_eshiview_host.sh: $cycles cycles clean under leaks and ThreadSanitizer."
echo "  $leak_report"
echo "  $race_report"
