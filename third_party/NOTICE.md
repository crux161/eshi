# Third-party attributions

Eshi/Larimar is MIT licensed. This file records the third-party components a
build may fetch or link, and the obligations that come with them. Nothing here
is checked into this repository; `third_party/` holds fetched distributions and
is ignored by git.

## Google Filament

- **Version:** v1.75.0, pinned by `scripts/vendor_filament.sh`, which verifies
  the release archive's SHA-256 before extracting it.
- **Upstream:** https://github.com/google/filament
- **License:** Apache License 2.0. The distribution's own `LICENSE` file is at
  `third_party/filament/v1.75.0/LICENSE`, and `zig build larimar
  -Dfilament=true` installs a copy to `zig-out/share/licenses/filament/LICENSE`
  so it travels with any binary that links it.
- **What is used:**
  - `bin/matc` compiles the material definitions `eshi-matgen` emits from
    shader sources. It runs at build time only and is not redistributed.
  - `lib/<arch>/*.a` are statically linked into Brush-grade builds when
    `-Dfilament=true` is passed. Ordinary Ink/Paper builds link none of it.
  - `include/gltfio` is Filament's glTF loader, which PLAN Step 5 uses rather
    than writing a second parser.

Apache-2.0 §4 requires that redistributions carry the license and retain
attribution notices. Any published Larimar artifact that links Filament must
ship `share/licenses/filament/LICENSE`; the build produces it in the same step
that produces the binary, so the two cannot separate.

Filament is not built from source in ordinary consumer builds — see
[`docs/larimar/ARCHITECTURE.md`](../docs/larimar/ARCHITECTURE.md) §6.8 for why.
