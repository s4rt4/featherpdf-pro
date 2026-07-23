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
#include <QStringList>

// The headless command-line interface. When the first argument is one of the
// known sub-commands (or a help/version flag), main() runs the CLI instead of
// the GUI. Every command is a thin wrapper over an existing backend, so the CLI
// and the app share exactly one implementation per operation.
namespace Cli {

// True if `firstArg` names a sub-command or a top-level CLI flag, i.e. the
// process should run headless rather than open the GUI. An empty string, or a
// path to a file to open, returns false.
bool isCommand(const QString& firstArg);

// Run the CLI. `args` is the full argument list (QCoreApplication::arguments()),
// including the program name at index 0. Returns the process exit code:
// 0 success, 1 operation failed, 2 usage error.
int run(const QStringList& args);

} // namespace Cli
