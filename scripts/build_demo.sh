#!/usr/bin/env bash
# Renders the demo a human can actually watch, into build/demo.
#
# Every gate in this repository answers a machine's question — does it compile,
# do the tiers agree to 1 LSB, did the surfaces get reclaimed. None of them
# answers "does it look right", and that question is not optional for a renderer.
# This produces the artifacts to look at: the same game on every tier the host
# can offer, plus the Flutter application when it has been built.
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
demo_dir="${LARIMAR_DEMO_DIR:-$repository_root/build/demo}"
pong="$repository_root/zig-out/bin/pong"
resolution="${LARIMAR_DEMO_RES:-960x540}"
pong_frames="${LARIMAR_DEMO_FRAMES:-600}"
gallery_frames="${LARIMAR_DEMO_GALLERY_FRAMES:-240}"
seed="${LARIMAR_DEMO_SEED:-42}"

if [ ! -x "$pong" ]; then
  echo "build_demo.sh: $pong is missing; run 'zig build larimar'." >&2
  exit 1
fi

mkdir -p "$demo_dir"
rendered=()

render() {
  local scene="$1" grade="$2"
  local scene_flag=""
  [ "$scene" = "ripple" ] && scene_flag="--gallery"
  local output="$demo_dir/$scene-$grade.mp4"
  local frames="$pong_frames"
  [ "$scene" = "ripple" ] && frames="$gallery_frames"

  local log
  # A tier the host cannot offer is a skip, not a failure: this machine may have
  # no GPU, and a demo that refuses to render anything is worse than a partial
  # one that says which parts are missing.
  if ! log=$("$pong" $scene_flag --grade "$grade" --res "$resolution" --frames "$frames" \
      --seed "$seed" --out "$output" 2>&1); then
    printf '  %-7s %-6s skipped — %s\n' "$scene" "$grade" \
      "$(printf '%s' "$log" | grep -m1 -E 'could not bind|no Metal|failed' | cut -c1-70)"
    rm -f "$output"
    return 0
  fi
  local backend
  backend=$(printf '%s' "$log" | sed -n 's/.*backend=\([a-z][a-z]*\).*/\1/p' | head -1)
  if [ "$grade" != "ink" ] && [ "$backend" = "ink" ]; then
    printf '  %-7s %-6s skipped — no backend on this host\n' "$scene" "$grade"
    rm -f "$output"
    return 0
  fi
  printf '  %-7s %-6s %s (%s)\n' "$scene" "$grade" "$(basename "$output")" "$backend"
  rendered+=("$scene-$grade.mp4")
}

echo "Rendering $resolution demos into $demo_dir"
for scene in pong ripple; do
  for grade in ink paper brush; do
    render "$scene" "$grade"
  done
done

# The Flutter application is the other half of what there is to look at, and it
# is built by a different toolchain.
#
# It is built here rather than copied from wherever an artifact happens to be,
# because `flutter test -d macos` writes its own bundle to exactly that path:
# the integration test becomes the application's entry point. Shipping that
# artifact produces an app that runs the test suite, tears the widget tree down
# when it finishes, and leaves a blank window — which is precisely what it did.
flutter_app_dir="$repository_root/examples/larimar_flutter"
flutter_app="$flutter_app_dir/build/macos/Build/Products/Release/larimar_flutter.app"
if command -v flutter >/dev/null 2>&1; then
  printf '  flutter        building… '
  if (cd "$flutter_app_dir" && flutter build macos --release -t lib/main.dart >"$demo_dir/flutter-build.log" 2>&1); then
    rm -rf "$demo_dir/LarimarFirstLight.app"
    cp -R "$flutter_app" "$demo_dir/LarimarFirstLight.app"
    rm -f "$demo_dir/flutter-build.log"
    echo "LarimarFirstLight.app"
  else
    echo "failed — see $demo_dir/flutter-build.log"
  fi
else
  echo "  flutter        not installed — skipping LarimarFirstLight.app"
fi

if [ -f "$repository_root/docs/larimar/media/eshiview-pong-01.png" ]; then
  cp "$repository_root/docs/larimar/media/eshiview-pong-0"*.png "$demo_dir/"
fi

cat >"$demo_dir/README.md" <<EOF
# Larimar demo build

Rendered by \`scripts/build_demo.sh\` at ${resolution}, seed ${seed}.

## What to watch

| File | What it should show |
|---|---|
| \`pong-ink.mp4\` | Pong on the CPU tier. Two glowing paddles, a ball that rebounds and scores, a dashed centre line. |
| \`pong-paper.mp4\` | The same game through OpenGL. Should be indistinguishable from Ink. |
| \`pong-brush.mp4\` | The same game through Metal or Filament, from a material generated out of the same shader source. Also indistinguishable. |
| \`ripple-*.mp4\` | The gallery shader on each tier: concentric rings — a blue core, orange and pink bands, a yellow rim — pulsing outward. |
| \`LarimarFirstLight.app\` | The Flutter application: animated Pong composited *under* ordinary Flutter UI through an external texture. The header counts frames presented and frames the engine composited; both should climb together. |
| \`eshiview-pong-0*.png\` | The view's surface read straight back from the host, twenty frames apart: what Larimar drew, before Flutter composited it. |

"Indistinguishable" is meant literally and is checked, not assumed:
\`scripts/check_tier_conformance.sh\` compares the tiers frame by frame and
requires them to agree to within one least-significant bit per channel.

## Interactive

\`\`\`sh
zig-out/bin/pong --live                 # CPU tier, a window you can play
zig-out/bin/pong --live --grade brush   # the GPU tier
zig-out/bin/pong --live --gallery       # the shader instead of the game
\`\`\`

W/S move the left paddle, Up/Down the right one, Escape quits. These run from any
directory — the shader sources install to \`zig-out/share/eshi/shaders\`, and
\`ESHI_SHADER_DIR\` overrides where they are read from.
EOF

echo
echo "build_demo.sh: ${#rendered[@]} clips in $demo_dir"
echo "  open $demo_dir/README.md for what each one should show"
