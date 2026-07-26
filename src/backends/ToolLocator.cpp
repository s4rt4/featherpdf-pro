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

#include "backends/ToolLocator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

// The conventional install roots; covers 64-bit and 32-bit-on-64 installs.
QStringList programFilesRoots() {
    QStringList roots;
    for (const char* var : {"ProgramFiles", "ProgramFiles(x86)", "ProgramW6432"}) {
        const QString v = qEnvironmentVariable(var);
        if (!v.isEmpty() && !roots.contains(v))
            roots << v;
    }
    if (roots.isEmpty())
        roots << QStringLiteral("C:/Program Files") << QStringLiteral("C:/Program Files (x86)");
    return roots;
}

QString existing(const QString& path) {
    return QFileInfo::exists(path) ? QDir::toNativeSeparators(path) : QString();
}

// First hit among <root>/<sub> for every Program Files root.
QString inProgramFiles(const QString& sub) {
    for (const QString& root : programFilesRoots()) {
        const QString hit = existing(root + QLatin1Char('/') + sub);
        if (!hit.isEmpty())
            return hit;
    }
    return QString();
}

// Tools we may ship next to feather-pdf-pro.exe (installer-bundled copies win
// over anything the user has on PATH, so the app works out of the box).
QString bundled(const QString& relative) {
    return existing(QCoreApplication::applicationDirPath() + QStringLiteral("/tools/") + relative);
}

bool underWindowsDir(const QString& path) {
    const QString winDir = qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"));
    return QDir::cleanPath(path).startsWith(QDir::cleanPath(QDir::fromNativeSeparators(winDir)),
                                            Qt::CaseInsensitive);
}

} // namespace

namespace ToolLocator {

QString soffice() {
    QString s = QStandardPaths::findExecutable(QStringLiteral("soffice"));
    if (!s.isEmpty())
        return s;
#ifdef Q_OS_WIN
    // The installer records its location; UNO InstallPath points at <root>\program.
    for (const QString& key :
         {QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\LibreOffice\\UNO\\InstallPath"),
          QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\LibreOffice\\UNO\\InstallPath")}) {
        const QString dir =
            QSettings(key, QSettings::NativeFormat).value(QStringLiteral(".")).toString();
        if (!dir.isEmpty()) {
            const QString hit =
                existing(QDir::fromNativeSeparators(dir) + QStringLiteral("/soffice.exe"));
            if (!hit.isEmpty())
                return hit;
        }
    }
    return inProgramFiles(QStringLiteral("LibreOffice/program/soffice.exe"));
#else
    return QStandardPaths::findExecutable(QStringLiteral("libreoffice"));
#endif
}

QString ghostscript() {
#ifdef Q_OS_WIN
    for (const char* name : {"gswin64c", "gswin32c", "gs"}) {
        const QString hit = QStandardPaths::findExecutable(QString::fromLatin1(name));
        if (!hit.isEmpty())
            return hit;
    }
    // Not on PATH: scan C:\Program Files\gs\gs<version>\bin, newest first.
    for (const QString& root : programFilesRoots()) {
        QDir gsRoot(root + QStringLiteral("/gs"));
        const QStringList versions =
            gsRoot.entryList({QStringLiteral("gs*")}, QDir::Dirs, QDir::Name | QDir::Reversed);
        for (const QString& v : versions) {
            for (const char* name : {"gswin64c.exe", "gswin32c.exe"}) {
                const QString hit =
                    existing(gsRoot.filePath(v + QStringLiteral("/bin/") + QLatin1String(name)));
                if (!hit.isEmpty())
                    return hit;
            }
        }
    }
    return QString();
#else
    QString gs = QStandardPaths::findExecutable(QStringLiteral("gs"));
    if (gs.isEmpty())
        gs = QStandardPaths::findExecutable(QStringLiteral("ghostscript"));
    return gs;
#endif
}

QString tesseract() {
    const QString shipped = bundled(QStringLiteral("tesseract/tesseract.exe"));
    if (!shipped.isEmpty())
        return shipped;
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("tesseract"));
    if (!onPath.isEmpty())
        return onPath;
    // The UB Mannheim installer's default location.
    return inProgramFiles(QStringLiteral("Tesseract-OCR/tesseract.exe"));
}

QString tesseractDataDir() {
    if (bundled(QStringLiteral("tesseract/tesseract.exe")).isEmpty())
        return QString();
    const QString dir = QCoreApplication::applicationDirPath() +
                        QStringLiteral("/tools/tesseract/tessdata");
    return QFileInfo::exists(dir) ? QDir::toNativeSeparators(dir) : QString();
}

QString pdfimages() {
    const QString shipped = bundled(QStringLiteral("poppler/pdfimages.exe"));
    if (!shipped.isEmpty())
        return shipped;
    return QStandardPaths::findExecutable(QStringLiteral("pdfimages"));
}

QString verapdf() {
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("verapdf"));
    if (!onPath.isEmpty())
        return onPath;
    return inProgramFiles(QStringLiteral("veraPDF/verapdf.bat"));
}

// True when `exe` actually runs and reports a version. A PATH hit is not
// enough: server bundles (Laragon/XAMPP Apache) ship an openssl.exe whose
// libssl often doesn't match, and such a copy dies on launch with a loader
// error instead of working. main() sets SEM_FAILCRITICALERRORS, so a broken
// candidate fails here with an exit code rather than a modal error box.
bool opensslWorks(const QString& exe) {
#ifdef Q_OS_WIN
    // Suppress the loader's modal error box for the child while probing; set
    // here too because test binaries don't go through the app's main().
    const UINT oldMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
#endif
    QProcess p;
    p.start(exe, {QStringLiteral("version")});
    bool ok = p.waitForFinished(5000);
    if (!ok)
        p.kill();
    ok = ok && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
#ifdef Q_OS_WIN
    SetErrorMode(oldMode);
#endif
    return ok;
}

QString openssl() {
    // Probing spawns a process, so resolve once per run.
    static const QString cached = [] {
        QStringList candidates;
        const QString onPath = QStandardPaths::findExecutable(QStringLiteral("openssl"));
        if (!onPath.isEmpty())
            candidates << onPath;
        // Git for Windows ships a full openssl most Windows dev machines already have.
        for (const QString& sub : {QStringLiteral("Git/mingw64/bin/openssl.exe"),
                                   QStringLiteral("Git/usr/bin/openssl.exe")}) {
            const QString hit = inProgramFiles(sub);
            if (!hit.isEmpty())
                candidates << hit;
        }
        for (const QString& candidate : candidates)
            if (opensslWorks(candidate))
                return candidate;
        return QString();
    }();
    return cached;
}

QString nssTool(const QString& name) {
    const QString shipped = bundled(QStringLiteral("nss/") + name + QStringLiteral(".exe"));
    if (!shipped.isEmpty())
        return shipped;
    const QString onPath = QStandardPaths::findExecutable(name);
#ifdef Q_OS_WIN
    // System32's certutil.exe is Microsoft's unrelated certificate tool; an
    // NSS lookup must never return it.
    if (!onPath.isEmpty() && !underWindowsDir(onPath))
        return onPath;
    return QString();
#else
    return onPath;
#endif
}

} // namespace ToolLocator
