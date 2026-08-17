#!/bin/sh
set -eu

SUMI_REPO="${SUMI_REPO:-https://github.com/crux161/libsumi.git}"
SUMI_VENDOR_DIR="${SUMI_VENDOR_DIR:-libsumi}"
SUMI_REF="${SUMI_REF:-}"
FORCE=0

usage() {
    cat <<EOF
Usage: ./vendor.sh [--force]

Fetch libsumi into ${SUMI_VENDOR_DIR}.

Environment:
  SUMI_REPO        Repository URL. Default: ${SUMI_REPO}
  SUMI_VENDOR_DIR  Destination directory. Default: ${SUMI_VENDOR_DIR}
  SUMI_REF         Optional branch, tag, or commit to checkout.

Options:
  -f, --force      Replace an existing ${SUMI_VENDOR_DIR} directory.
  -h, --help       Show this help.
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        -f|--force)
            FORCE=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

case "$SUMI_VENDOR_DIR" in
    ""|"/"|".")
        echo "Refusing unsafe SUMI_VENDOR_DIR: $SUMI_VENDOR_DIR" >&2
        exit 1
        ;;
esac

if [ -e "$SUMI_VENDOR_DIR" ] && [ "$FORCE" -ne 1 ]; then
    if [ -f "$SUMI_VENDOR_DIR/include/sumi/sumi.h" ]; then
        echo "libsumi is already available at $SUMI_VENDOR_DIR."
        echo "Use './vendor.sh --force' to replace it."
        exit 0
    fi

    echo "$SUMI_VENDOR_DIR already exists, but it does not look like libsumi." >&2
    echo "Move it aside or rerun with --force to replace it." >&2
    exit 1
fi

tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/eshi-libsumi.XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT HUP INT TERM

echo "Fetching libsumi from $SUMI_REPO..."
git clone --depth 1 "$SUMI_REPO" "$tmp_dir"

if [ -n "$SUMI_REF" ]; then
    echo "Checking out libsumi ref $SUMI_REF..."
    git -C "$tmp_dir" fetch --depth 1 origin "$SUMI_REF"
    git -C "$tmp_dir" checkout --detach FETCH_HEAD
fi

rm -rf "$tmp_dir/.git"

if [ "$FORCE" -eq 1 ] && [ -e "$SUMI_VENDOR_DIR" ]; then
    rm -rf "$SUMI_VENDOR_DIR"
fi

parent_dir="$(dirname "$SUMI_VENDOR_DIR")"
if [ "$parent_dir" != "." ]; then
    mkdir -p "$parent_dir"
fi

mv "$tmp_dir" "$SUMI_VENDOR_DIR"
trap - EXIT HUP INT TERM

echo "Vendored libsumi into $SUMI_VENDOR_DIR."
echo "Run 'make' to build Eshi, or override with SUMI_PATH=/path/to/libsumi."
