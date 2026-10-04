#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
version=$(sed -n '1p' "$root/VERSION")

if [ "$(uname -s)" != "Haiku" ]; then
    echo "error: HPKG candidates must be built on Haiku" >&2
    exit 1
fi
if ! command -v package >/dev/null 2>&1; then
    echo "error: Haiku's package command is unavailable" >&2
    exit 1
fi

architecture=$(getarch 2>/dev/null || uname -m)
stage="${TMPDIR:-/tmp}/vhs-package-$$"
output_directory="$root/dist"
output="$output_directory/vhs-$version-1-$architecture.hpkg"
trap 'rm -rf "$stage"' EXIT HUP INT TERM

mkdir -p "$stage/bin" "$stage/data/man/man1"
mkdir -p "$stage/data/bash-completion/completions"
mkdir -p "$stage/data/zsh/site-functions"
mkdir -p "$stage/data/fish/vendor_completions.d"
mkdir -p "$stage/documentation/packages/vhs/examples" "$output_directory"

make -C "$root" clean all test
cp "$root/vhs" "$stage/bin/vhs"
cp "$root/docs/vhs.1" "$stage/data/man/man1/vhs.1"
cp "$root/completions/vhs.bash" "$stage/data/bash-completion/completions/vhs"
cp "$root/completions/_vhs" "$stage/data/zsh/site-functions/_vhs"
cp "$root/completions/vhs.fish" "$stage/data/fish/vendor_completions.d/vhs.fish"
cp "$root/README.md" "$root/CHANGELOG.md" "$root/docs/ROADMAP.md" \
    "$root/docs/RELEASE.md" \
    "$stage/documentation/packages/vhs/"
cp "$root"/examples/*.tape "$stage/documentation/packages/vhs/examples/"

cat > "$stage/.PackageInfo" <<EOF
name vhs
version $version-1
architecture $architecture
summary "Terminal GIF and video recorder driven by tape scripts"
description "A native Haiku implementation of the VHS tape workflow for text, PNG, GIF, MP4 and WebM terminal recordings."
vendor "Sikosis"
packager "Sikosis"
copyrights { "2026 VHS for Haiku contributors" }
licenses { "MIT" }
provides {
    vhs = $version
    cmd:vhs = $version
}
requires {
    haiku
    cmd:bash
    cmd:ffmpeg
}
EOF

package create -C "$stage" "$output"
echo "Created $output"
