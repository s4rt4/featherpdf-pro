// Feather PDF Pro — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <QString>

// Finds the external programs Feather shells out to. On Windows most of them
// are never on PATH — they live under Program Files or are recorded in the
// registry — and one (certutil) collides with an unrelated Microsoft tool in
// System32, so every call site goes through here instead of using
// QStandardPaths::findExecutable directly.
//
// Every function returns the absolute path of the executable, or an empty
// string when the tool isn't installed. Results are not cached: each lookup
// is a handful of stat calls, and not caching means a tool installed while
// the app is running is picked up immediately.
namespace ToolLocator {

// LibreOffice's soffice binary: PATH, then the LibreOffice registry keys,
// then the conventional Program Files locations.
QString soffice();

// Ghostscript console binary: gswin64c/gswin32c on Windows (gs elsewhere),
// scanning C:\Program Files\gs\gs<version>\bin when not on PATH.
QString ghostscript();

// Tesseract OCR: a copy bundled by the installer (tools/tesseract next to the
// exe), then PATH, then the standard Tesseract-OCR install directory.
QString tesseract();

// The tessdata directory that belongs to the bundled Tesseract, or empty when
// tesseract() resolves to a system install. A bundled (vcpkg-built) Tesseract
// has a compile-time data path that points at the build machine, so callers
// must pass this via TESSDATA_PREFIX; system installs find their own data.
QString tesseractDataDir();

// Poppler's pdfimages utility: PATH, then next to any bundled poppler tools.
QString pdfimages();

// veraPDF validator: PATH (the Windows installer provides verapdf.bat, which
// findExecutable resolves via PATHEXT), then its default install directory.
QString verapdf();

// OpenSSL CLI: PATH, then the copy that ships inside Git for Windows.
QString openssl();

// NSS tools (certutil, modutil, pk12util). On Windows a lookup by name finds
// C:\Windows\System32\certutil.exe — Microsoft's certificate tool, same name,
// entirely different program — so results inside the Windows directory are
// rejected and known NSS install locations are searched instead.
QString nssTool(const QString& name);

} // namespace ToolLocator
