#!/bin/sh
# Fetches the pinned Filament distribution into third_party/filament.
#
# Filament is a large CMake project with its own toolchain expectations, and
# building it from source would dominate this build — so consumers get the
# official prebuilt release instead, at a version this repository pins and a
# checksum it verifies. See docs/larimar/ARCHITECTURE.md §6.8.
#
# The same distribution serves three purposes: `matc` compiles the materials
# eshi-matgen emits, the static libraries back the Brush tier, and `gltfio`
# is what will own glTF parsing in PLAN Step 5.
set -eu

FILAMENT_VERSION="${FILAMENT_VERSION:-v1.75.0}"
FILAMENT_VENDOR_DIR="${FILAMENT_VENDOR_DIR:-third_party/filament/$FILAMENT_VERSION}"
FILAMENT_BASE_URL="${FILAMENT_BASE_URL:-https://github.com/google/filament/releases/download}"
FORCE=0

# Checksums for the archives this version pins. Upgrading Filament means
# changing the version and both of these in one commit, with a full gate run —
# never one without the other.
SHA256_MAC="731b5fe71844e82e2ee45b891b05b9c07599fabccd297033275d90baefe97021"
SHA256_LINUX="c5d2e0f692e5fb98ed029a5a3a52c8174660d02844d5db5a804dc5264bbab6d1"

usage() {
    cat <<EOF
Usage: ./scripts/vendor_filament.sh [--force]

Fetch the pinned Filament distribution ($FILAMENT_VERSION) into
$FILAMENT_VENDOR_DIR, verifying its SHA-256.

Environment:
  FILAMENT_VERSION     Release tag. Default: $FILAMENT_VERSION
  FILAMENT_VENDOR_DIR  Destination. Default: third_party/filament/<version>
  FILAMENT_BASE_URL    Release download base. Default: GitHub releases.

Options:
  -f, --force          Replace an existing distribution.
  -h, --help           Show this help.
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        -f|--force) FORCE=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

case "$(uname -s)" in
    Darwin) platform="mac"; expected_sha="$SHA256_MAC"; library_arch="arm64" ;;
    Linux)
        if [ "$(uname -m)" != "x86_64" ]; then
            echo "vendor_filament.sh: only x86_64 is pinned on Linux; this host is $(uname -m)." >&2
            exit 1
        fi
        platform="linux"; expected_sha="$SHA256_LINUX"; library_arch="x86_64"
        ;;
    *)
        echo "vendor_filament.sh: unsupported host $(uname -s); RC0 targets macOS and Linux." >&2
        exit 1
        ;;
esac

# A distribution is "there" only if the pieces this repository uses are there.
# A half-extracted directory should re-fetch rather than fail later inside a
# build with a confusing missing-file error.
is_complete() {
    [ -x "$FILAMENT_VENDOR_DIR/bin/matc" ] &&
        [ -f "$FILAMENT_VENDOR_DIR/lib/$library_arch/libfilament.a" ] &&
        [ -f "$FILAMENT_VENDOR_DIR/include/gltfio/AssetLoader.h" ] &&
        [ -f "$FILAMENT_VENDOR_DIR/LICENSE" ]
}

if is_complete && [ "$FORCE" -ne 1 ]; then
    echo "Filament $FILAMENT_VERSION is already vendored at $FILAMENT_VENDOR_DIR."
    echo "Use './scripts/vendor_filament.sh --force' to replace it."
    exit 0
fi

archive="filament-$FILAMENT_VERSION-$platform.tgz"
url="$FILAMENT_BASE_URL/$FILAMENT_VERSION/$archive"
tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/eshi-filament.XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT HUP INT TERM

echo "Fetching $archive..."
curl -fsSL -o "$tmp_dir/$archive" "$url"

if command -v shasum >/dev/null 2>&1; then
    actual_sha="$(shasum -a 256 "$tmp_dir/$archive" | cut -d' ' -f1)"
else
    actual_sha="$(sha256sum "$tmp_dir/$archive" | cut -d' ' -f1)"
fi
if [ "$actual_sha" != "$expected_sha" ]; then
    echo "vendor_filament.sh: checksum mismatch for $archive" >&2
    echo "  expected $expected_sha" >&2
    echo "  actual   $actual_sha" >&2
    exit 1
fi

mkdir -p "$tmp_dir/extract"
# The archive's single top-level `filament/` directory is stripped, so the
# vendor directory *is* the distribution root that -Dfilament-path expects.
tar xzf "$tmp_dir/$archive" -C "$tmp_dir/extract" --strip-components=1

rm -rf "$FILAMENT_VENDOR_DIR"
mkdir -p "$(dirname "$FILAMENT_VENDOR_DIR")"
mv "$tmp_dir/extract" "$FILAMENT_VENDOR_DIR"

if ! is_complete; then
    echo "vendor_filament.sh: $archive extracted without the expected layout." >&2
    exit 1
fi

echo "Vendored Filament $FILAMENT_VERSION into $FILAMENT_VENDOR_DIR."
echo "  zig build larimar -Dfilament=true"
echo "  (override with -Dfilament-path=/path/to/filament)"
