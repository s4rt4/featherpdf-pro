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

#include <QString>

// Where Feather keeps its own files. Normally that is the per-user locations
// (QSettings in the registry, AppData, LocalAppData cache). When a file named
// `portable.txt` sits next to the `bin` folder — the portable ZIP ships one —
// everything goes under `<that folder>/data` instead, so the app leaves no
// trace on the machine and carries its settings along on a USB stick.
namespace AppPaths {

// Decide the mode and point QSettings at the right store. Call once, right
// after the organization/application names are set and before any QSettings.
void init();

bool isPortable();

QString dataDir();  // actions and other user data
QString cacheDir(); // regenerable files (covers, tinted icons)

} // namespace AppPaths
