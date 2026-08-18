#!/usr/bin/env bash
# Renders the reference glTF asset and proves something was actually drawn.
#
# "It loaded" and "it rendered" are different claims, and a 3D backend can
# satisfy the first while producing a cleared frame — a camera pointing the
# wrong way, an instance never added to the scene, geometry uploaded and never
# referenced. So this renders the asset twice: once with no instances placed,
# once with one. If the two frames match, nothing reached the screen.
#
# It also checks the failure paths, because an asset pipeline that reports
# success on a missing file is worse than one that cannot load at all.
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
pong="$repository_root/zig-out/bin/pong"
ppmdiff="$repository_root/zig-out/bin/eshi-ppmdiff"
asset="${LARIMAR_ASSET:-$repository_root/zig-out/share/eshi/larimar_logo.glb}"
resolution="${LARIMAR_ASSET_RES:-320x180}"

for binary in "$pong" "$ppmdiff"; do
  if [ ! -x "$binary" ]; then
    echo "check_asset_render.sh: $binary is missing; run 'zig build larimar tools -Dfilament=true'." >&2
    exit 1
  fi
done
if [ ! -f "$asset" ]; then
  echo "check_asset_render.sh: $asset is missing; run 'zig build shaders'." >&2
  exit 1
fi

# 3D geometry needs the Filament backend. A build or a machine without one is a
# skip that says so, the same rule the Metal shader validation follows.
probe=$("$pong" --grade brush --asset "$asset" --asset-instances 0 --res 64x36 \
  --frames 1 --hash 2>&1 || true)
case "$probe" in
  *"backend=filament"*) ;;
  *)
    echo "check_asset_render.sh: no Filament backend on this host; skipping."
    echo "  (build with -Dfilament=true after ./scripts/vendor_filament.sh)"
    exit 0
    ;;
esac

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/larimar-asset.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT

render() {
  local instances="$1" directory="$2"
  mkdir -p "$directory"
  "$pong" --grade brush --asset "$asset" --asset-instances "$instances" \
    --res "$resolution" --frames 2 --dump "$directory" >"$work_dir/render-$instances.log" 2>&1
}

render 0 "$work_dir/empty"
render 1 "$work_dir/drawn"

# ppmdiff exits non-zero when frames differ by more than the allowance, so a
# zero allowance turns it into "are these identical?" — and here they must not
# be.
if "$ppmdiff" "$work_dir/empty" "$work_dir/drawn" 0 >/dev/null 2>&1; then
  echo "::error::the asset render is identical to an empty scene; nothing was drawn" >&2
  exit 1
fi
echo "  asset renders geometry the empty scene does not"

# gltfio drops images it has no decoder for, mentions it once on stderr, and
# renders the model black — which a digest cannot tell from a dark material.
# The greybox has no textures, so only a real model exposed this; the check
# stays because the next asset will have them.
if grep -q "Missing texture provider" "$work_dir"/render-*.log; then
  echo "::error::the loader has no decoder for a texture this asset uses:" >&2
  grep -h "Missing texture provider" "$work_dir"/render-*.log | sort -u >&2
  exit 1
fi
echo "  every texture the asset references has a decoder"

# One upload, several instances: the asset count stays at one.
instanced=$("$pong" --grade brush --asset "$asset" --asset-instances 3 \
  --res 64x36 --frames 1 --hash 2>&1)
case "$instanced" in
  *"instances=3 assets=1"*) echo "  three instances share one uploaded asset" ;;
  *)
    echo "::error::three instances did not share one asset:" >&2
    printf '%s\n' "$instanced" | grep -m1 "eshi/host" >&2 || true
    exit 1
    ;;
esac

check_failure() {
  local description="$1" path="$2" expected="$3"
  local output
  if output=$("$pong" --grade brush --asset "$path" --res 64x36 --frames 1 --hash 2>&1); then
    echo "::error::$description was accepted; it must fail" >&2
    exit 1
  fi
  case "$output" in
    *"$expected"*) echo "  $description refused: $expected" ;;
    *)
      echo "::error::$description failed without saying why:" >&2
      printf '%s\n' "$output" >&2
      exit 1
      ;;
  esac
}

check_failure "a missing asset" "$work_dir/absent.glb" "cannot read asset"
head -c 512 /dev/urandom >"$work_dir/corrupt.glb"
check_failure "a corrupt asset" "$work_dir/corrupt.glb" "not a glTF this loader accepts"

# The capability ladder, stated by the engine rather than assumed by the host.
if ink=$("$pong" --grade ink --asset "$asset" --res 64x36 --frames 1 --hash 2>&1); then
  echo "::error::the Ink tier claimed to load a 3D asset" >&2
  exit 1
fi
case "$ink" in
  *"unsupported grade or feature"*) echo "  the Ink tier reports no 3D scene" ;;
  *) echo "::error::the Ink tier failed for the wrong reason:" >&2; printf '%s\n' "$ink" >&2; exit 1 ;;
esac

echo "check_asset_render.sh: the reference asset loads, instances, and draws."
