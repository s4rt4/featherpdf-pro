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

#include "backends/WatchFolder.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

bool WatchFolder::processPending(const QString& inDir, const QString& outDir,
                                 const QString& doneDir, const QList<BatchRunner::Step>& steps,
                                 int quietMs, Counts* counts, QString* error) {
    const auto fail = [&](const QString& m) {
        if (error)
            *error = m;
        return false;
    };
    QDir in(inDir);
    if (!in.exists())
        return fail(QObject::tr("The watched folder %1 doesn't exist.").arg(inDir));

    const QString outD = outDir.isEmpty() ? in.filePath(QStringLiteral("_out")) : outDir;
    const QString doneD = doneDir.isEmpty() ? in.filePath(QStringLiteral("_done")) : doneDir;
    // Output and done must sit apart from the input, or results would be picked
    // up and processed again.
    const QString inCanon = QFileInfo(inDir).canonicalFilePath();
    for (const QString& d : {outD, doneD}) {
        if (!QDir().mkpath(d))
            return fail(QObject::tr("Couldn't create %1.").arg(d));
        if (QFileInfo(d).canonicalFilePath() == inCanon)
            return fail(QObject::tr("The output and done folders must differ from the input."));
    }

    Counts c;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QFileInfoList entries =
        in.entryInfoList({QStringLiteral("*.pdf")}, QDir::Files, QDir::Name);
    for (const QFileInfo& fi : entries) {
        // Skip a file that was just touched - a writer may still be filling it.
        if (quietMs > 0 && fi.lastModified().toMSecsSinceEpoch() > now - quietMs)
            continue;
        const QString src = fi.absoluteFilePath();
        const QString out = QDir(outD).filePath(fi.fileName());
        QFile::remove(out); // overwrite a stale result of the same name
        QString why;
        if (!BatchRunner::runFile(src, out, steps, &why)) {
            ++c.failed;
            continue;
        }
        // Retire the source so the next scan ignores it; keep both copies if a
        // same-named file was processed before.
        QString dst = QDir(doneD).filePath(fi.fileName());
        if (QFileInfo::exists(dst))
            dst = QDir(doneD).filePath(QStringLiteral("%1-%2.pdf")
                                           .arg(fi.completeBaseName())
                                           .arg(now));
        if (!QFile::rename(src, dst))
            QFile::remove(src); // last resort: at least don't reprocess it
        ++c.processed;
    }
    if (counts)
        *counts = c;
    return true;
}
