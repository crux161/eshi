#!/usr/bin/env bash
# Emits a Filament material for every shader in the corpus and compiles it.
#
# The gallery is the conformance corpus for the C++ shader subset — 20 programs
# that already run on Ink, OpenGL, and Metal. This asks the fourth target the
# same question, and it is a real question: a `.mat` fragment block is not a
# free-form shader, and the two limits below were found by running this, not by
# reading the specification.
#
# A shader may only fail to emit if it is listed here with its reason. That list
# is the point: it turns "some shaders don't work on Filament" into a fixed,
# named set that cannot grow without someone editing this file.
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
matgen="$repository_root/zig-out/bin/eshi-matgen"
# The pinned distribution scripts/vendor_filament.sh fetches.
filament_path="${FILAMENT_PATH:-$repository_root/third_party/filament/v1.75.0}"
matc="$filament_path/bin/matc"

# Shaders the material domain cannot express, and why. Both refuse loudly with
# a diagnostic naming the cause; see docs/larimar/SHADER_SUBSET.md.
expected_refusals="warp rainforest"

if [ ! -x "$matgen" ]; then
  echo "check_materials.sh: $matgen is missing; run 'zig build tools'." >&2
  exit 1
fi

# matc ships with a Filament distribution, which an ordinary checkout does not
# have — vendoring one is a PLAN Step 5 bullet. Until then the emitter half runs
# everywhere and the compile half runs wherever Filament is installed. Say which
# happened rather than reporting a weaker check as the full one.
compile_materials=1
if [ ! -x "$matc" ]; then
  compile_materials=0
  echo "check_materials.sh: no matc at $matc; checking emission only."
  echo "  Set FILAMENT_PATH to a Filament distribution to also compile the materials."
fi

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/larimar-materials.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT

compiled=0
emitted=0
refused=0
status=0

for source in "$repository_root"/examples/*.cpp "$repository_root"/examples/pong/pong.gpu.cpp; do
  name=$(basename "$source" .cpp)
  name=${name%.gpu}
  # Pong is the one shader with a uniform block; the gallery reads none.
  uniform_floats=0
  case "$source" in
    */pong.gpu.cpp) uniform_floats=64 ;;
  esac

  if ! "$matgen" "$source" --uniform-floats "$uniform_floats" \
      -o "$work_dir/$name.mat" 2>"$work_dir/$name.refusal"; then
    case " $expected_refusals " in
      *" $name "*)
        echo "  refused  $name — $(sed -n 's/.*: //p' "$work_dir/$name.refusal" | head -1 | cut -c1-60)…"
        refused=$((refused + 1))
        ;;
      *)
        echo "::error::$name no longer emits a Filament material:" >&2
        cat "$work_dir/$name.refusal" >&2
        status=1
        ;;
    esac
    continue
  fi

  case " $expected_refusals " in
    *" $name "*)
      echo "::error::$name now emits a material but is still listed as a refusal; update the list." >&2
      status=1
      ;;
  esac

  if [ "$compile_materials" -eq 0 ]; then
    emitted=$((emitted + 1))
    continue
  fi
  if "$matc" -p all -a all -o "$work_dir/$name.filamat" "$work_dir/$name.mat" \
      >"$work_dir/$name.matc" 2>&1; then
    compiled=$((compiled + 1))
  else
    echo "::error::matc rejected the material generated from $name:" >&2
    sed -n '1,10p' "$work_dir/$name.matc" >&2
    status=1
  fi
done

if [ "$compile_materials" -eq 1 ]; then
  echo "check_materials.sh: $compiled materials compiled, $refused refused for documented reasons."
else
  echo "check_materials.sh: $emitted materials emitted (not compiled), $refused refused for documented reasons."
fi
exit "$status"
