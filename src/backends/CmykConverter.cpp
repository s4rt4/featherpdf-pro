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

#include "backends/CmykConverter.h"

#include <QFile>
#include <QProcess>
#include <QStandardPaths>

namespace {
QString ghostscript() {
    QString gs = QStandardPaths::findExecutable(QStringLiteral("gs"));
    if (gs.isEmpty())
        gs = QStandardPaths::findExecutable(QStringLiteral("ghostscript"));
    return gs;
}
} // namespace

bool CmykConverter::isAvailable() {
    return !ghostscript().isEmpty();
}

bool CmykConverter::toCmyk(const QString& inputPath, const QString& outputPath, QString* error) {
    const QString gs = ghostscript();
    if (gs.isEmpty()) {
        if (error)
            *error = QStringLiteral("Ghostscript isn't installed, so colors can't be converted to "
                                    "CMYK. Install the 'ghostscript' package and try again.");
        return false;
    }

    // pdfwrite rewrites every colorspace to the process model. ColorConversionStrategy=CMYK
    // forces RGB/Gray content (and images/shadings) through to DeviceCMYK; the matching
    // ProcessColorModel keeps the device itself in CMYK so nothing is converted back.
    QProcess proc;
    proc.start(gs, {QStringLiteral("-q"), QStringLiteral("-dBATCH"), QStringLiteral("-dNOPAUSE"),
                    QStringLiteral("-dSAFER"), QStringLiteral("-sDEVICE=pdfwrite"),
                    QStringLiteral("-sProcessColorModel=DeviceCMYK"),
                    QStringLiteral("-sColorConversionStrategy=CMYK"),
                    QStringLiteral("-dEncodeColorImages=true"),
                    QStringLiteral("-o"), outputPath, inputPath});
    if (!proc.waitForStarted(15000)) {
        if (error)
            *error = QStringLiteral("Ghostscript couldn't be started.");
        return false;
    }
    if (!proc.waitForFinished(180000) || proc.exitStatus() != QProcess::NormalExit ||
        proc.exitCode() != 0) {
        if (error) {
            const QString detail = QString::fromUtf8(proc.readAllStandardError()).trimmed();
            *error = detail.isEmpty()
                         ? QStringLiteral("Ghostscript couldn't convert the document.")
                         : QStringLiteral("Ghostscript couldn't convert the document:\n%1")
                               .arg(detail);
        }
        return false;
    }
    if (!QFile::exists(outputPath)) {
        if (error)
            *error = QStringLiteral("The CMYK PDF wasn't produced.");
        return false;
    }
    return true;
}
