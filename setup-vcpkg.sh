#!/usr/bin/env bash
set -euo pipefail

VCPKG_COMMIT="2d6a6cf3ac9a7cc93942c3d289a2f9c661a6f4a7"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "$SCRIPT_DIR"

echo "========================================"
echo "GAM300 vcpkg Setup Script (Linux)"
echo "========================================"
echo

echo "[1/3] Checking required tools..."
for tool in git git-lfs cmake ninja pkg-config; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: '$tool' is required but was not found in PATH."
        echo "Install git, git-lfs, cmake, ninja, and pkgconf-pkg-config, then run this script again."
        exit 1
    fi
done

missing_modules=()
for module in x11 xcursor xinerama xrandr xi xext xrender xfixes xxf86vm glu; do
    if ! pkg-config --exists "$module"; then
        missing_modules+=("$module")
    fi
done

if [ "${#missing_modules[@]}" -ne 0 ]; then
    echo "ERROR: Missing Linux desktop development packages for GLFW: ${missing_modules[*]}"
    distro_id=""
    if [ -r /etc/os-release ]; then
        # shellcheck disable=SC1091
        . /etc/os-release
        distro_id="${ID:-}"
    fi

    if [ "$distro_id" = "fedora" ]; then
        echo "Fedora is the currently validated Linux host. Install the missing packages with:"
        echo "  sudo dnf install -y libX11-devel libXcursor-devel libXinerama-devel libXrandr-devel libXi-devel libXext-devel libXrender-devel libXfixes-devel libXxf86vm-devel mesa-libGLU-devel"
    else
        echo "Install the equivalent development packages for X11, Xcursor, Xinerama, Xrandr, Xi, Xext, Xrender, Xfixes, Xxf86vm, and GLU."
        echo "Fedora package names are documented in README.md; other distributions are not validated yet."
    fi
    exit 1
fi

if [ -d "vcpkg" ]; then
    echo "vcpkg directory already exists. Leaving it in place."
else
    echo "[2/3] Cloning vcpkg repository..."
    git clone https://github.com/Microsoft/vcpkg.git
fi

echo "[2.5/3] Checking out pinned vcpkg version (${VCPKG_COMMIT})..."
git -C vcpkg fetch --tags
git -C vcpkg checkout "$VCPKG_COMMIT"

echo "[3/3] Bootstrapping vcpkg..."
./vcpkg/bootstrap-vcpkg.sh -disableMetrics

echo
echo "========================================"
echo "SUCCESS! vcpkg has been set up successfully."
echo
echo "Linux uses CMake + Ninja through Project/CMakeUserPresets.json."
echo "Next commands:"
echo "  cd Project"
echo "  cmake --preset linux-editor-debug"
echo "  cmake --build --preset linux-editor-debug"
echo "========================================"
