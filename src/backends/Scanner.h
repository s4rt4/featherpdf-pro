// Feather PDF — light on the system, full-featured on PDF.
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

#include <QList>
#include <QString>
#include <QStringList>

// Scanner access via SANE's `scanimage` command (the standard Linux scanning
// stack). Lists devices and scans one or more pages to PNG files; the caller
// then assembles them into a PDF (Converter::imagesToPdf) and may add an OCR
// text layer. Pure subprocess — no SANE library link, matching how the rest of
// the app shells out to LibreOffice / Tesseract / openssl.
class Scanner {
public:
    enum class Mode { Color, Gray };

    struct Device {
        QString name;        // SANE device id, e.g. "epson2:libusb:001:004"
        QString vendor;      // e.g. "Epson"
        QString model;       // e.g. "Perfection V39"
        QString type;        // e.g. "flatbed scanner"
        QString label() const; // human label for a combo box
    };

    // True if `scanimage` is on PATH (the only runtime dependency).
    static bool isAvailable();

    // Enumerate connected scanners (`scanimage -f`). Empty list with empty
    // *error means "none found"; a non-empty *error means the query failed.
    static QList<Device> devices(QString* error);

    // Parse the lines emitted by `scanimage -f '%d|%v|%m|%t%n'`. Exposed so the
    // parsing can be unit-tested without hardware.
    static QList<Device> parseDeviceList(const QString& raw);

    // Scan `pageCount` page(s) from `device` at `dpi`/`mode` into `outDir`,
    // returning the produced PNG paths in page order. On failure returns an
    // empty list and fills *error. Blocks until scanning finishes (it is slow);
    // call from a busy-cursor context. `device` empty → SANE's default device.
    static QStringList scanPages(const QString& device, int dpi, Mode mode, int pageCount,
                                 const QString& outDir, QString* error);
};
