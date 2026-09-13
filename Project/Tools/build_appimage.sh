#!/usr/bin/env bash
# Package an already-built Linux game as a single-file AppImage.
#
# This does not build the game. Point it at a finished LinuxRelease directory
# and it produces Kusane-x86_64.AppImage, which runs on any reasonably recent
# x86_64 desktop without installation.
#
# Usage:
#   Project/Tools/build_appimage.sh [build-dir] [output-dir]
#
# Defaults to Project/Build/LinuxRelease and a build/ directory alongside it.
#
# The payload is staged the same way the Windows installer stages it, so the
# two ship identical content: source art whose cooked output exists is left
# out, and audio is losslessly recompressed. That needs `soundfile`; without
# it the audio step is skipped and the result is about 125 MiB larger.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${1:-$ROOT/Project/Build/LinuxRelease}"
OUT="${2:-$ROOT/Project/Build/appimage}"
GAME="$BUILD/Kusane"

[ -x "$GAME" ] || { echo "no game binary at $GAME" >&2; exit 1; }

# objdump decides which library name the loader will ask for and whether every
# dependency is satisfied. Without it the SONAME link is never made and the
# dependency check reads an empty list and passes, which is the silent version
# of shipping a package that does not start.
command -v objdump >/dev/null || {
    echo "objdump is required (install binutils)" >&2
    exit 1
}

# Everything is built in a work directory that can be large: the staged payload
# is over 3 GiB, so this must not land on a small tmpfs.
WORK="$OUT/work"
STAGE="$WORK/stage"
APPDIR="$WORK/AppDir"
rm -rf "$WORK"
mkdir -p "$STAGE" "$OUT"

# Stage flat first, with the binary beside Resources, because
# compress_audio.py checks for that shape before it will run. The AppDir
# layout is built from it afterwards.
echo "staging resources"
python3 "$ROOT/Project/Tools/stage_resources.py" \
    --resources "$ROOT/Project/Resources" \
    --destination "$STAGE/Resources"
cp "$GAME" "$BUILD"/libEngine.so "$BUILD"/libfmod.so.* "$STAGE/"
mkdir -p "$STAGE/ProjectSettings"
cp "$ROOT/Project/ProjectSettings/TagsAndLayers.json" "$STAGE/ProjectSettings/"

if python3 -c "import soundfile" 2>/dev/null; then
    echo "compressing audio"
    python3 "$ROOT/Project/Tools/compress_audio.py" --game-directory "$STAGE"
else
    echo "soundfile not installed, skipping audio compression (~125 MiB larger)" >&2
fi

mkdir -p "$APPDIR"/usr/{bin,lib,share/applications,share/kusane} \
         "$APPDIR/usr/share/icons/hicolor/256x256/apps"
mv "$STAGE/Kusane" "$APPDIR/usr/bin/"
mv "$STAGE"/libEngine.so "$STAGE"/libfmod.so.* "$APPDIR/usr/lib/"
mv "$STAGE/Resources" "$STAGE/ProjectSettings" "$APPDIR/usr/share/kusane/"

# The build directory holds libfmod.so.14.9, but the engine was linked against
# the SONAME, libfmod.so.14, and that is the name the loader asks for. Without
# it the game starts only on a machine that happens to have FMOD at the absolute
# path baked into libEngine.so's RUNPATH, which means the developer's own, and
# fails everywhere else with "cannot open shared object file".
for lib in "$APPDIR"/usr/lib/libfmod.so.*; do
    [ -e "$lib" ] || continue
    soname=$(objdump -p "$lib" 2>/dev/null | awk '/SONAME/{print $2; exit}')
    if [ -n "$soname" ] && [ "$soname" != "$(basename "$lib")" ]; then
        ln -sf "$(basename "$lib")" "$APPDIR/usr/lib/$soname"
        echo "linked $soname -> $(basename "$lib")"
    fi
done

# libGLU comes in through glfw3's vcpkg config and nothing in the engine calls
# a glu function, but the binary hard-links it, so a machine without it cannot
# start the game at all. It is a utility library over GL with no driver
# coupling, so unlike libGLX and libGLdispatch it is safe to carry. Those stay
# on the host, where they have to match the graphics driver.
GLU=$(ldconfig -p 2>/dev/null | awk '/libGLU\.so\.1 /{print $NF; exit}')
if [ -n "$GLU" ] && [ -e "$GLU" ]; then
    cp -L "$GLU" "$APPDIR/usr/lib/libGLU.so.1"
    echo "bundled $GLU"
else
    echo "libGLU.so.1 not found on this machine, the image will need it present" >&2
    exit 1
fi

ICON="$APPDIR/usr/share/icons/hicolor/256x256/apps/kusane.png"
ICO="$ROOT/Installer/INSTALLERFILES/SetupIcon.ico"
# Pillow where it exists, ImageMagick otherwise. An AppImage without an icon
# still runs but shows a blank tile in every launcher, so this is not optional.
if python3 -c "import PIL" 2>/dev/null; then
    python3 -c "import sys; from PIL import Image; Image.open(sys.argv[1]).convert('RGBA').resize((256,256), Image.LANCZOS).save(sys.argv[2])" "$ICO" "$ICON"
elif command -v magick >/dev/null; then
    magick "${ICO}[0]" -resize 256x256 "$ICON"
elif command -v convert >/dev/null; then
    convert "${ICO}[0]" -resize 256x256 "$ICON"
else
    echo "need Pillow or ImageMagick to convert the icon" >&2; exit 1
fi

cat > "$APPDIR/usr/share/applications/kusane.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Kusane
Comment=A DigiPen student game by Team Marbles
Exec=Kusane
Icon=kusane
Categories=Game;
Terminal=false
DESKTOP

cp "$APPDIR/usr/share/icons/hicolor/256x256/apps/kusane.png" "$APPDIR/kusane.png"
cp "$APPDIR/usr/share/applications/kusane.desktop" "$APPDIR/kusane.desktop"

# The game resolves Resources and ProjectSettings against its working
# directory, so this moves there before exec. Settings and logs go to the
# user's profile, which is what lets the payload sit on a read-only mount.
cat > "$APPDIR/AppRun" <<'APPRUN'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib:$LD_LIBRARY_PATH"
cd "$HERE/usr/share/kusane" || exit 1
exec "$HERE/usr/bin/Kusane" "$@"
APPRUN
chmod +x "$APPDIR/AppRun"

# The Windows job counts cooked assets before it builds the installer, because
# the game has no raw-image fallback on desktop and a cook that silently did
# nothing produces a package that looks the right size and renders untextured.
# The same applies here, and this is the last moment the payload can be read,
# since the work directory goes away below.
echo "verifying the payload"
fail=0
for path in usr/bin/Kusane AppRun usr/share/kusane/Resources \
            usr/share/kusane/ProjectSettings/TagsAndLayers.json \
            usr/share/applications/kusane.desktop kusane.png; do
    [ -e "$APPDIR/$path" ] || { echo "  missing $path" >&2; fail=1; }
done
[ -x "$APPDIR/usr/bin/Kusane" ] || { echo "  usr/bin/Kusane is not executable" >&2; fail=1; }
[ -x "$APPDIR/AppRun" ] || { echo "  AppRun is not executable" >&2; fail=1; }
# Check the libraries the loader will actually ask for, by name, rather than
# checking that something fmod-shaped is present. Globbing for libfmod.so.*
# passed while the one name the loader wanted was absent.
# Kept on several lines for reading. The unquoted expansion below re-joins it
# with single spaces, because the pattern match tests for a space either side of
# a name and a newline is not a space.
allowed_from_host="libc.so.6 libm.so.6 libdl.so.2 librt.so.1 libpthread.so.0
libstdc++.so.6 libgcc_s.so.1 ld-linux-x86-64.so.2 libGL.so.1 libGLX.so.0
libOpenGL.so.0 libGLdispatch.so.0 libX11.so.6 libXext.so.6 libxcb.so.1
libXau.so.6 libXdmcp.so.6 libXcursor.so.1 libXi.so.6 libXinerama.so.1
libXrandr.so.2 libXrender.so.1 libXfixes.so.3 libxkbcommon.so.0 libwayland-client.so.0"

for binary in "$APPDIR/usr/bin/Kusane" "$APPDIR"/usr/lib/*.so*; do
    [ -f "$binary" ] || continue
    for need in $(objdump -p "$binary" 2>/dev/null | awk '/NEEDED/{print $2}'); do
        # Written as if-then because set -e treats a false "test && continue"
        # as a failed statement and aborts the build.
        if [ -e "$APPDIR/usr/lib/$need" ]; then continue; fi
        case " $(echo $allowed_from_host) " in
            *" $need "*) continue ;;
        esac
        echo "  $(basename "$binary") needs $need, which is neither bundled nor a standard system library" >&2
        fail=1
    done
done

for ext in dds mesh font; do
    count=$(find "$APPDIR/usr/share/kusane/Resources" -type f -name "*.$ext" 2>/dev/null | wc -l)
    echo "  .$ext files: $count"
    [ "$count" -gt 0 ] || { echo "  no .$ext in the payload, the cook did not land" >&2; fail=1; }
done
[ "$fail" -eq 0 ] || { echo "payload is incomplete, refusing to package" >&2; exit 1; }

TOOL="$WORK/appimagetool"
curl -sL -o "$TOOL" \
  https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
chmod +x "$TOOL"

echo "building the image"
ARCH=x86_64 "$TOOL" --comp zstd "$APPDIR" "$OUT/Kusane-x86_64.AppImage"
rm -rf "$WORK"
ls -la "$OUT/Kusane-x86_64.AppImage"
