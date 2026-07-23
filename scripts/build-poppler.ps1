# Feather PDF Pro — build Poppler's Qt6 bindings for Windows/MSVC.
#
# Poppler ships no Windows binaries and vcpkg's poppler[qt] would drag in a
# second, vcpkg-built Qt. This script builds poppler-qt6 once against YOUR Qt
# (the official binaries) with its C dependencies taken from vcpkg, and
# installs the result into <repo>/deps/poppler — which CMakePresets.json puts
# on CMAKE_PREFIX_PATH.
#
# Prerequisites: Visual Studio 2022 (C++ workload), CMake, git,
#   Qt 6.x MSVC kit, vcpkg at C:\vcpkg with the deps below installed:
#     vcpkg install freetype libjpeg-turbo openjpeg libpng tiff lcms zlib --triplet x64-windows
#
# Usage (from the repo root, in "Developer PowerShell for VS 2022" or any
# PowerShell with cmake on PATH):
#   .\scripts\build-poppler.ps1 [-QtDir C:/Qt/6.8.3/msvc2022_64] [-VcpkgRoot C:/vcpkg]

param(
    [string]$QtDir = "C:/Qt/6.8.3/msvc2022_64",
    [string]$VcpkgRoot = "C:/vcpkg",
    [string]$PopplerTag = "poppler-25.07.0"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$src = Join-Path $repo "deps/poppler-src"
$build = Join-Path $repo "deps/poppler-build"
$prefix = Join-Path $repo "deps/poppler"

if (-not (Test-Path "$QtDir/lib/cmake/Qt6")) {
    throw "Qt not found at $QtDir - pass -QtDir pointing at your MSVC kit."
}
if (-not (Test-Path "$VcpkgRoot/scripts/buildsystems/vcpkg.cmake")) {
    throw "vcpkg not found at $VcpkgRoot - pass -VcpkgRoot."
}

if (-not (Test-Path $src)) {
    git clone --depth 1 --branch $PopplerTag https://gitlab.freedesktop.org/poppler/poppler.git $src
    if ($LASTEXITCODE -ne 0) { throw "Cloning poppler failed." }
}

# Qt6 bindings on; every backend Feather doesn't use off. The win32 font
# backend means no fontconfig dependency. NSS/GPGME signature backends are off
# for now (Feather's signing roadmap moves to Windows CNG); the Qt API still
# compiles and reports signatures as unverifiable.
cmake -S $src -B $build -G "Visual Studio 17 2022" -A x64 `
    "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot/scripts/buildsystems/vcpkg.cmake" `
    "-DVCPKG_TARGET_TRIPLET=x64-windows" `
    "-DCMAKE_PREFIX_PATH=$QtDir" `
    "-DCMAKE_INSTALL_PREFIX=$prefix" `
    -DCMAKE_BUILD_TYPE=Release `
    -DENABLE_QT6=ON -DENABLE_QT5=OFF -DENABLE_GLIB=OFF -DENABLE_CPP=OFF `
    -DENABLE_UTILS=ON `
    -DENABLE_NSS3=OFF -DENABLE_GPGME=OFF -DENABLE_LIBCURL=OFF -DENABLE_BOOST=OFF `
    -DENABLE_LCMS=ON -DENABLE_LIBTIFF=ON -DENABLE_LIBOPENJPEG=openjpeg2 `
    -DENABLE_DCTDECODER=libjpeg -DFONT_CONFIGURATION=win32 `
    -DBUILD_QT6_TESTS=OFF -DBUILD_GTK_TESTS=OFF -DBUILD_CPP_TESTS=OFF `
    -DBUILD_MANUAL_TESTS=OFF -DENABLE_QT6_TESTS=OFF
if ($LASTEXITCODE -ne 0) { throw "Configuring poppler failed." }

cmake --build $build --config Release --target install
if ($LASTEXITCODE -ne 0) { throw "Building poppler failed." }

Write-Host ""
Write-Host "Poppler-Qt6 installed to $prefix"
Write-Host "Now configure Feather:  cmake --preset windows-msvc"
