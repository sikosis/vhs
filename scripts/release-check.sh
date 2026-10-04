#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
version=$(sed -n '1p' "$root/VERSION")
cmake_build="${TMPDIR:-/tmp}/vhs-cmake-$$"
install_root="${TMPDIR:-/tmp}/vhs-install-$$"

cleanup()
{
    rm -rf "$cmake_build" "$install_root"
    make -C "$root" clean >/dev/null
}
trap cleanup EXIT HUP INT TERM

case "$version" in
    *.*) ;;
    *) echo "error: VERSION must contain a two-part decimal version" >&2; exit 1 ;;
esac

make -C "$root" clean all test
"$root/vhs" --version | grep "^vhs v$version$"
"$root/vhs" check "$root/examples/pty.tape"
"$root/vhs" check "$root/examples/png.tape"
"$root/vhs" check "$root/examples/animation.tape"

make -C "$root" install DESTDIR="$install_root" PREFIX=/boot/system/non-packaged
test -x "$install_root/boot/system/non-packaged/bin/vhs"
test -f "$install_root/boot/system/non-packaged/share/man/man1/vhs.1"

if command -v cmake >/dev/null 2>&1; then
    cmake -S "$root" -B "$cmake_build" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$cmake_build"
    ctest --test-dir "$cmake_build" --output-on-failure
    DESTDIR="$install_root/cmake" cmake --install "$cmake_build" \
        --prefix /boot/system/non-packaged
    test -x "$install_root/cmake/boot/system/non-packaged/bin/vhs"
fi

if [ "$(uname -s)" = "Haiku" ]; then
    sh "$root/packaging/build-hpkg.sh"
else
    echo "note: skipping HPKG creation outside Haiku"
fi

echo "VHS v$version release checks passed"
