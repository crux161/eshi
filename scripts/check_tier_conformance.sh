#!/usr/bin/env bash
# Compares the render tiers frame by frame against Ink.
#
# The claim this checks is the project's oracle: one material source, executed
# by the CPU tier and compiled by the GPU tiers, produces the same picture to
# within one least-significant bit. It was measured by hand once and written
# into a table in docs/larimar/ARCHITECTURE.md; a number nobody can re-derive
# stops being true quietly, so this makes it a command.
#
# Digests are the wrong instrument here and deliberately unused: `--hash`
# answers "is this tier deterministic?", which is a strict equality that tiers
# are not expected to satisfy against each other. Raw frames and a bounded pixel
# difference answer "do these tiers agree?"
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
pong="$repository_root/zig-out/bin/pong"
ppmdiff="$repository_root/zig-out/bin/eshi-ppmdiff"
frames="${LARIMAR_CONFORMANCE_FRAMES:-60}"
resolution="${LARIMAR_CONFORMANCE_RES:-320x180}"
allowed="${LARIMAR_CONFORMANCE_LSB:-1}"
seed="${LARIMAR_CONFORMANCE_SEED:-42}"

for binary in "$pong" "$ppmdiff"; do
  if [ ! -x "$binary" ]; then
    echo "check_tier_conformance.sh: $binary is missing; run 'zig build larimar' and 'zig build tools'." >&2
    exit 1
  fi
done

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/larimar-conformance.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT

# Renders one scene on one tier and reports which backend actually ran. A grade
# whose backend is unavailable falls back to Ink, and comparing Ink to Ink would
# pass while proving nothing — so the caller checks what came back.
render() {
  local grade="$1" scene="$2" directory="$3"
  mkdir -p "$directory"
  local scene_flag=""
  [ "$scene" = "gallery" ] && scene_flag="--gallery"
  local output
  output=$("$pong" $scene_flag --grade "$grade" --res "$resolution" --frames "$frames" \
    --seed "$seed" --dump "$directory" 2>&1) || {
    echo "$output" >&2
    return 1
  }
  printf '%s\n' "$output" | sed -n 's/.*backend=\([a-z][a-z]*\).*/\1/p' | head -1
}

compared=0
skipped=0

for scene in pong gallery; do
  ink_dir="$work_dir/$scene-ink"
  ink_backend=$(render ink "$scene" "$ink_dir")
  if [ "$ink_backend" != "ink" ]; then
    echo "check_tier_conformance.sh: the ink grade selected '$ink_backend'; the reference tier is not itself." >&2
    exit 1
  fi

  for grade in paper brush; do
    tier_dir="$work_dir/$scene-$grade"
    backend=$(render "$grade" "$scene" "$tier_dir") || {
      echo "check_tier_conformance.sh: $scene failed to render at $grade." >&2
      exit 1
    }
    if [ "$backend" = "ink" ]; then
      echo "  $scene $grade: no backend on this host (fell back to Ink); skipped"
      skipped=$((skipped + 1))
      continue
    fi
    printf '  %s %s (%s) vs ink: ' "$scene" "$grade" "$backend"
    "$ppmdiff" "$ink_dir" "$tier_dir" "$allowed"
    compared=$((compared + 1))
  done
done

if [ "$compared" -eq 0 ]; then
  echo "check_tier_conformance.sh: no GPU tier was available; nothing was compared." >&2
  exit 1
fi

echo "check_tier_conformance.sh: $compared tier comparisons within $allowed LSB at $resolution over $frames frames ($skipped skipped)."
