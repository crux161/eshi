#!/usr/bin/env bash
# Runs the macOS EshiView lifecycle loop inside a real Flutter engine, snapshots
# the process from outside, and keeps the composited frames it captured.
#
# The snapshots have to come from here rather than from the test: the reference
# application runs under the App Sandbox, and a sandboxed process cannot open
# its own task port, so `leaks` refuses to examine it from within. The test
# instead asks for a snapshot by dropping a request file in its container and
# waits for this script to acknowledge it.
#
# The host-free half of this gate — ThreadSanitizer and `leaks --atExit` over
# the adapter itself — lives in check_eshiview_host.sh.
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
app_root="$repository_root/examples/larimar_flutter"
artifact_dir="${LARIMAR_ARTIFACTS:-$repository_root/build/larimar-diagnostics}"
cycles="${LARIMAR_LIFECYCLE_CYCLES:-12}"
# Foundation and AppKit leak a little on their own during a run; the gate is
# about surfaces, which are three orders of magnitude larger than this.
tolerance="${LARIMAR_LEAK_TOLERANCE:-65536}"

if [ "$(uname -s)" != "Darwin" ]; then
  echo "check_eshiview_lifecycle.sh: EshiView is macOS-only." >&2
  exit 1
fi

mkdir -p "$artifact_dir"
log="$artifact_dir/eshiview-lifecycle.log"
: >"$log"

cd "$app_root"
flutter test integration_test/eshiview_lifecycle_test.dart -d macos \
  --reporter expanded \
  --dart-define=LARIMAR_LEAK_HARNESS=true \
  --dart-define=LARIMAR_LIFECYCLE_CYCLES="$cycles" >>"$log" 2>&1 &
test_pid=$!

container_tmp=""
snapshots=""

snapshot() {
  local phase="$1"
  local app_pid
  app_pid=$(pgrep -n -f 'larimar_flutter.app/Contents/MacOS/larimar_flutter' || true)
  if [ -z "$app_pid" ]; then
    echo "check_eshiview_lifecycle.sh: no running application to snapshot for '$phase'." >&2
    return 1
  fi
  leaks "$app_pid" >"$artifact_dir/eshiview-$phase-leaks.txt" 2>&1 || true
  footprint "$app_pid" >"$artifact_dir/eshiview-$phase-footprint.txt" 2>&1 || true
  snapshots="$snapshots $phase"
}

while kill -0 "$test_pid" 2>/dev/null; do
  if [ -z "$container_tmp" ]; then
    container_tmp=$(sed -n 's/^larimar-tmp: //p' "$log" | head -1 | tr -d '\r')
    sleep 0.2
    continue
  fi
  request="$container_tmp/larimar-lifecycle.request"
  if [ -f "$request" ]; then
    phase=$(tr -d '[:space:]' <"$request")
    rm -f "$request"
    snapshot "$phase" || true
    printf '%s' "$phase" >"$container_tmp/larimar-lifecycle.ack"
  fi
  sleep 0.2
done

set +e
wait "$test_pid"
test_status=$?
set -e

tail -20 "$log"

# Keep whatever the run captured, whether or not it passed: a failed frame is
# evidence too.
if [ -n "$container_tmp" ]; then
  while IFS= read -r captured; do
    if [ -f "$captured" ]; then cp "$captured" "$artifact_dir/"; fi
  done < <(sed -n 's/^larimar-artifact: //p' "$log" | tr -d '\r')
fi

if [ "$test_status" -ne 0 ]; then
  echo "check_eshiview_lifecycle.sh: the lifecycle test failed; see $log" >&2
  exit "$test_status"
fi

if grep -q '^larimar-skip: ' "$log"; then
  echo "check_eshiview_lifecycle.sh: skipped — $(sed -n 's/^larimar-skip: //p' "$log" | head -1)."
  exit 0
fi

leaked_bytes() {
  # "Process 1234: 7 leaks for 320 total leaked bytes."
  sed -n 's/.*for \([0-9][0-9]*\) total leaked bytes.*/\1/p' "$1" | tail -1
}

baseline_report="$artifact_dir/eshiview-baseline-leaks.txt"
final_report="$artifact_dir/eshiview-final-leaks.txt"
for report in "$baseline_report" "$final_report"; do
  if [ ! -s "$report" ]; then
    echo "check_eshiview_lifecycle.sh: missing $report; the snapshots never ran." >&2
    exit 1
  fi
done

baseline_bytes=$(leaked_bytes "$baseline_report")
final_bytes=$(leaked_bytes "$final_report")
if [ -z "$baseline_bytes" ] || [ -z "$final_bytes" ]; then
  echo "check_eshiview_lifecycle.sh: could not read leaked byte counts from the reports." >&2
  exit 1
fi

growth=$((final_bytes - baseline_bytes))
echo "check_eshiview_lifecycle.sh: $cycles cycles;$snapshots snapshots kept."
echo "  leaked bytes after cycle 1: $baseline_bytes"
echo "  leaked bytes after cycle $cycles: $final_bytes (growth $growth, tolerance $tolerance)"
if [ "$growth" -gt "$tolerance" ]; then
  echo "::error::EshiView lifecycle leaked $growth bytes across $((cycles - 1)) cycles." >&2
  exit 1
fi
echo "  artifacts in $artifact_dir"
