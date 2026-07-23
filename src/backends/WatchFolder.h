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

#include "backends/BatchRunner.h"

#include <QList>
#include <QString>

// Folder automation: apply a saved Batch action to PDFs that land in a watched
// directory. processPending() does one full pass (scan + process), so it's both
// testable and reusable; the CLI `watch` command calls it on a file-system
// watcher loop to run as a daemon.
class WatchFolder {
public:
    struct Counts {
        int processed = 0;
        int failed = 0;
    };

    // Run `steps` over every *.pdf currently in `inDir` (non-recursive): write
    // the result to `outDir`/<name>.pdf and move the source into `doneDir` so it
    // is not seen again. Files modified within `quietMs` of now are skipped (a
    // writer may still be filling them). `outDir`/`doneDir` are created if
    // missing and must differ from `inDir`. Returns false and sets *error on a
    // directory problem; per-file failures are counted in Counts, not fatal.
    static bool processPending(const QString& inDir, const QString& outDir,
                               const QString& doneDir, const QList<BatchRunner::Step>& steps,
                               int quietMs, Counts* counts, QString* error);
};
