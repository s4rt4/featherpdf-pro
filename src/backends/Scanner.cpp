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

#include "backends/Scanner.h"

#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QProcess>
#include <QStandardPaths>

namespace {
constexpr char kScanimage[] = "scanimage";

QString program() {
    return QStandardPaths::findExecutable(QString::fromLatin1(kScanimage));
}
} // namespace

QString Scanner::Device::label() const {
    QString human = QStringLiteral("%1 %2").arg(vendor, model).trimmed();
    if (human.isEmpty())
        human = name;
    if (!type.isEmpty())
        human += QStringLiteral(" (%1)").arg(type);
    return human;
}

bool Scanner::isAvailable() {
    return !program().isEmpty();
}

QList<Scanner::Device> Scanner::parseDeviceList(const QString& raw) {
    QList<Device> out;
    const QStringList lines = raw.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const QStringList parts = line.split('|');
        if (parts.isEmpty() || parts.first().trimmed().isEmpty())
            continue;
        Device d;
        d.name = parts.value(0).trimmed();
        d.vendor = parts.value(1).trimmed();
        d.model = parts.value(2).trimmed();
        d.type = parts.value(3).trimmed();
        out.append(d);
    }
    return out;
}

QList<Scanner::Device> Scanner::devices(QString* error) {
    const QString exe = program();
    if (exe.isEmpty()) {
        if (error)
            *error = QObject::tr("scanimage (SANE) isn't installed.");
        return {};
    }

    QProcess proc;
    // One line per device: device|vendor|model|type
    proc.start(exe, {QStringLiteral("-f"), QStringLiteral("%d|%v|%m|%t%n")});
    if (!proc.waitForFinished(30000)) {
        proc.kill();
        if (error)
            *error = QObject::tr("Listing scanners timed out.");
        return {};
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        if (error)
            *error = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        return {};
    }
    return parseDeviceList(QString::fromUtf8(proc.readAllStandardOutput()));
}

QStringList Scanner::scanPages(const QString& device, int dpi, Mode mode, int pageCount,
                               const QString& outDir, QString* error) {
    const QString exe = program();
    if (exe.isEmpty()) {
        if (error)
            *error = QObject::tr("scanimage (SANE) isn't installed.");
        return {};
    }
    const int pages = qMax(1, pageCount);

    QStringList args;
    if (!device.isEmpty())
        args << QStringLiteral("-d") << device;
    args << QStringLiteral("--format=png")
         << QStringLiteral("--resolution") << QString::number(qBound(50, dpi, 1200))
         << QStringLiteral("--mode") << (mode == Mode::Color ? QStringLiteral("Color")
                                                             : QStringLiteral("Gray"));

    const QDir dir(outDir);
    QStringList produced;

    if (pages == 1) {
        // Single page straight to a file (batch is overkill and needs an ADF).
        const QString out = dir.filePath(QStringLiteral("scan-1.png"));
        QStringList one = args;
        one << QStringLiteral("-o") << out;
        QProcess proc;
        proc.start(exe, one);
        if (!proc.waitForFinished(180000)) {
            proc.kill();
            if (error)
                *error = QObject::tr("Scanning timed out.");
            return {};
        }
        if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0 ||
            !QFileInfo::exists(out)) {
            if (error)
                *error = QString::fromUtf8(proc.readAllStandardError()).trimmed();
            return {};
        }
        produced << out;
        return produced;
    }

    // Multiple pages via batch mode (typically a document feeder).
    QStringList batch = args;
    batch << QStringLiteral("--batch=") + dir.filePath(QStringLiteral("scan-%d.png"))
          << QStringLiteral("--batch-count") << QString::number(pages);
    QProcess proc;
    proc.start(exe, batch);
    // Allow generous time: many pages at high DPI.
    if (!proc.waitForFinished(600000)) {
        proc.kill();
        if (error)
            *error = QObject::tr("Scanning timed out.");
        return {};
    }
    // scanimage batch returns non-zero once the feeder is empty; accept whatever
    // pages it actually wrote.
    for (int i = 1; i <= pages; ++i) {
        const QString f = dir.filePath(QStringLiteral("scan-%1.png").arg(i));
        if (QFileInfo::exists(f))
            produced << f;
    }
    if (produced.isEmpty() && error)
        *error = QString::fromUtf8(proc.readAllStandardError()).trimmed();
    return produced;
}
