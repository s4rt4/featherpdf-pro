# Feather PDF Pro — light on the system, full-featured on PDF.
# Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).

cmake_minimum_required(VERSION 3.21)

project(FeatherPDF
    VERSION 1.0.1
    DESCRIPTION "Native, open-source PDF tool for Windows"
    HOMEPAGE_URL "https://github.com/s4rt4/featherpdf-pro"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to a sensible build type for single-config generators.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Core/Gui/Widgets (shell), Pdf/PdfWidgets (PDFium rendering), Svg (symbolic
# icons), Concurrent (off-UI-thread work), PrintSupport, LinguistTools (i18n).
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Pdf PdfWidgets Svg PrintSupport Concurrent TextToSpeech LinguistTools)

# Tests are built by default; turn off with -DBUILD_TESTING=OFF.
option(BUILD_TESTING "Build the test suite" ON)

# ── QPDF (lossless structure writer) ──
# vcpkg ships a CMake config package; pkg-config stays as a fallback for
# environments that have it (MSYS2, Linux).
find_package(qpdf CONFIG QUIET)
if(qpdf_FOUND)
    add_library(feather_qpdf INTERFACE)
    target_link_libraries(feather_qpdf INTERFACE qpdf::libqpdf)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(QPDF REQUIRED IMPORTED_TARGET libqpdf)
    add_library(feather_qpdf INTERFACE)
    target_link_libraries(feather_qpdf INTERFACE PkgConfig::QPDF)
endif()
add_library(feather::qpdf ALIAS feather_qpdf)

# ── Poppler-Qt6 (semantic layer — annotations, forms, signatures) ──
# Poppler ships no CMake config, and pkg-config isn't native on Windows, so
# find the headers and import libraries directly. scripts/build-poppler.ps1
# produces a prefix (deps/poppler) that CMAKE_PREFIX_PATH picks up here.
find_path(POPPLER_QT6_INCLUDE_DIR poppler-qt6.h PATH_SUFFIXES poppler/qt6)
find_library(POPPLER_QT6_LIBRARY NAMES poppler-qt6)
if(POPPLER_QT6_INCLUDE_DIR AND POPPLER_QT6_LIBRARY)
    add_library(feather_poppler_qt6 INTERFACE)
    target_include_directories(feather_poppler_qt6 INTERFACE ${POPPLER_QT6_INCLUDE_DIR})
    target_link_libraries(feather_poppler_qt6 INTERFACE ${POPPLER_QT6_LIBRARY})
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(POPPLER_QT6 REQUIRED IMPORTED_TARGET poppler-qt6)
    add_library(feather_poppler_qt6 INTERFACE)
    target_link_libraries(feather_poppler_qt6 INTERFACE PkgConfig::POPPLER_QT6)
endif()
add_library(feather::poppler-qt6 ALIAS feather_poppler_qt6)

qt_standard_project_setup()

# qt_standard_project_setup() turns on AUTOMOC/AUTOUIC but not AUTORCC; we embed
# the symbolic-icon resource via a .qrc, so enable it explicitly.
set(CMAKE_AUTORCC ON)

add_subdirectory(src)

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    enable_testing()
    add_subdirectory(tests)
endif()

# The app links the MSVC runtime dynamically, so the redistributable DLLs have
# to travel with it — a machine without the Visual C++ redistributable installed
# otherwise gets "VCRUNTIME140.dll was not found" instead of an app.
if(WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION bin)
    include(InstallRequiredSystemLibraries)
endif()

# ── Packaging: `cpack` from the build dir → feather-pdf-pro-<ver>-win64.zip ──
# (The installer is built from the same staged tree; see packaging/windows.)
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "feather-pdf-pro")
set(CPACK_PACKAGE_VENDOR "Feather PDF contributors")
set(CPACK_PACKAGE_CONTACT "s4rt4 <noreply@github.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_DESCRIPTION "Feather PDF Pro - light on the system, full-featured on PDF.
A native, open-source PDF application for Windows: view, edit, sign,
annotate, OCR, and convert PDFs. Runs headless as a CLI too.")
set(CPACK_PACKAGE_HOMEPAGE_URL ${PROJECT_HOMEPAGE_URL})
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
set(CPACK_PACKAGE_FILE_NAME "feather-pdf-pro-${PROJECT_VERSION}-win64")

include(CPack)
