#!/bin/bash
# build-bundle.sh --platform linux|windows --out DIR: build SDLPoP2 for the platform and put what a user downloads in
# DIR, flat (the files unpack straight into the game's folder): the executable, SDLPoP2.ini, README.md, LICENSE
# and BUILD.txt (the commit and the toolchain it was built with). CI and the releases use it
# (.github/workflows); it needs meson, ninja and, for Windows, mingw-w64.
#   linux:   SDL2 built in from the WrapDB wrap (--force-fallback-for=sdl2): the executable needs only the C library
#   windows: cross-compiled with cross/mingw-w64.ini (static), stripped
set -e
platform=; out=
while [ $# -gt 0 ]; do
	case $1 in
	--platform) platform=$2; shift 2 ;;
	--out) out=$2; shift 2 ;;
	*) echo "build-bundle.sh: unknown argument $1" >&2; exit 2 ;;
	esac
done
[ "$platform" = linux ] || [ "$platform" = windows ] || { echo "usage: build-bundle.sh --platform linux|windows --out DIR" >&2; exit 2; }
[ -n "$out" ] || { echo "build-bundle.sh: --out DIR is required" >&2; exit 2; }
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
build=build-bundle-$platform
if [ "$platform" = linux ]; then
	[ -f $build/build.ninja ] || meson setup $build --buildtype=release --force-fallback-for=sdl2
	meson compile -C $build
	exe=$build/sdl/sdlpop2; cc=$(cc --version | head -1)
	# nothing but the C library (SDL loads X11 / Wayland / ALSA / PulseAudio at run time)
	if ldd "$exe" | grep -vE "linux-vdso|libc\.so|libm\.so|ld-linux"; then echo "build-bundle.sh: unexpected shared library dependency" >&2; exit 1; fi
else
	[ -f $build/build.ninja ] || meson setup $build --buildtype=release --cross-file cross/mingw-w64.ini
	meson compile -C $build
	exe=$build/sdl/sdlpop2.exe; cc=$(x86_64-w64-mingw32-gcc --version | head -1)
fi
rm -rf "$out"; mkdir -p "$out"
cp "$exe" SDLPoP2.ini README.md LICENSE "$out/"
if [ "$platform" = windows ]; then x86_64-w64-mingw32-strip "$out/sdlpop2.exe"; else strip "$out/sdlpop2"; fi
{
	echo "SDLPoP2 ($platform x86-64)"
	echo "commit:   $(git rev-parse HEAD 2>/dev/null || echo unknown)"
	echo "date:     $(git log -1 --format=%cd --date=iso-strict 2>/dev/null || echo unknown)"
	echo "compiler: $cc"
	echo "meson:    $(meson --version)"
	echo "SDL2:     $(sed -n 's/^directory = SDL2-//p' extern/sdl2.wrap) (WrapDB, static)"
} > "$out/BUILD.txt"
ls -la "$out"
