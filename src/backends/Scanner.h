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

// Scanner access via WIA (Windows Image Acquisition, the standard Windows
// scanning stack). Lists devices and scans one or more pages to PNG files; the
// caller then assembles them into a PDF (Converter::imagesToPdf) and may add
// an OCR text layer. Talks COM directly (IWiaDevMgr2 / IWiaTransfer) — no
// extra runtime dependency beyond Windows itself.
class Scanner {
public:
    enum class Mode { Color, Gray };

    struct Device {
        QString name;        // WIA device id, e.g. "{6BDD1FC6-…}\\0001"
        QString vendor;      // e.g. "Epson"
        QString model;       // e.g. "Perfection V39"
        QString type;        // e.g. "scanner"
        QString label() const; // human label for a combo box
    };

    // True if the WIA runtime is reachable. It ships with Windows, so false
    // effectively means the Windows Image Acquisition service is disabled.
    static bool isAvailable();

    // Human word for an STI device type id (StiDeviceTypeScanner → "scanner").
    // Exposed so the mapping can be unit-tested without hardware.
    static QString typeLabel(int stiDeviceType);

    // Enumerate connected scanners. Empty list with empty *error means "none
    // found"; a non-empty *error means the query itself failed.
    static QList<Device> devices(QString* error);

    // Scan `pageCount` page(s) from `device` at `dpi`/`mode` into `outDir`,
    // returning the produced PNG paths in page order. Multiple pages use the
    // document feeder when the scanner has one, else repeated flatbed passes.
    // On failure returns an empty list and fills *error. Blocks until scanning
    // finishes (it is slow); call from a busy-cursor context. `device` empty →
    // the first scanner found.
    static QStringList scanPages(const QString& device, int dpi, Mode mode, int pageCount,
                                 const QString& outDir, QString* error);
};
