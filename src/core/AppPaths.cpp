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

#include "core/AppPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {
QString g_portableData; // empty unless running portable
} // namespace

namespace AppPaths {

void init() {
    // The exe lives in <root>/bin, and the marker sits beside bin in <root>.
    const QDir root(QCoreApplication::applicationDirPath() + QStringLiteral("/.."));
    if (!QFileInfo::exists(root.filePath(QStringLiteral("portable.txt"))))
        return;
    g_portableData = QDir::cleanPath(root.absoluteFilePath(QStringLiteral("data")));
    QDir().mkpath(g_portableData);
    // Every QSettings() in the app is default-constructed, so switching the
    // default format moves them all from the registry into an INI file under
    // data/ (as data/<organization>/<application>.ini).
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, g_portableData);
}

bool isPortable() {
    return !g_portableData.isEmpty();
}

QString dataDir() {
    return isPortable() ? g_portableData
                        : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString cacheDir() {
    return isPortable() ? g_portableData + QStringLiteral("/cache")
                        : QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

} // namespace AppPaths
