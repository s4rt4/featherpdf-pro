# Feather PDF — light on the system, full-featured on PDF.
# Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).

cmake_minimum_required(VERSION 3.21)

project(FeatherPDF
    VERSION 0.0.1
    DESCRIPTION "Native, open-source PDF tool for Linux"
    HOMEPAGE_URL "https://github.com/s4rt4/featherpdf-linux"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to a sensible build type for single-config generators.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# M0 needs: Core/Gui/Widgets (shell), Pdf/PdfWidgets (PDFium rendering),
# Svg (symbolic icons), Concurrent (off-UI-thread work). Poppler/QPDF are added
# when their modules land.
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Pdf PdfWidgets Svg PrintSupport Concurrent LinguistTools)

# Tests are built by default; turn off with -DBUILD_TESTING=OFF.
option(BUILD_TESTING "Build the test suite" ON)

# QPDF (lossless structure writer) ships no CMake config, so use pkg-config.
find_package(PkgConfig REQUIRED)
pkg_check_modules(QPDF REQUIRED IMPORTED_TARGET libqpdf)

# Poppler-Qt6 (semantic layer — annotations, attachments, optional content). Its
# CMake config is unreliable across distros, so use pkg-config like QPDF.
pkg_check_modules(POPPLER_QT6 REQUIRED IMPORTED_TARGET poppler-qt6)

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

# ── Packaging: `cpack -G RPM` / `cpack -G DEB` from the build dir ──
set(CPACK_PACKAGE_NAME "feather-pdf")
set(CPACK_PACKAGE_VENDOR "Feather PDF contributors")
set(CPACK_PACKAGE_CONTACT "s4rt4 <noreply@github.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_DESCRIPTION "Feather PDF - light on the system, full-featured on PDF.
A native, open-source PDF application for Linux: view, edit, sign,
annotate, OCR, and convert PDFs. Runs headless as a CLI too.")
set(CPACK_PACKAGE_HOMEPAGE_URL ${PROJECT_HOMEPAGE_URL})
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
set(CPACK_STRIP_FILES ON)

set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)   # auto-derive library Depends
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)  # feather-pdf_<ver>_amd64.deb

set(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Publishing")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)     # feather-pdf-<ver>.x86_64.rpm
set(CPACK_RPM_PACKAGE_AUTOREQ ON)        # auto-derive library Requires
# Don't claim ownership of standard dirs other packages own.
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    /usr/share/applications /usr/share/metainfo /usr/share/thumbnailers
    /usr/share/icons /usr/share/icons/hicolor /usr/share/icons/hicolor/scalable
    /usr/share/icons/hicolor/scalable/apps /usr/share/kio
    /usr/share/kio/servicemenus /usr/share/nautilus-python
    /usr/share/nautilus-python/extensions)

include(CPack)
