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

#include "backends/FormEditor.h"

#include <QList>
#include <QString>

// "Prepare Form" auto-detection (Poppler). Scans a flat PDF for the visual cues
// of a fillable form — underscore fill-in lines ("Name: ______") become Text
// fields, and checkbox markers ("[ ]", "☐") become CheckBox fields — and returns
// ready-to-place FormEditor::NewField records. Names are derived from the nearby
// label text and an input Format is guessed from label keywords (date / amount /
// percent). The caller reviews and edits the list before FormEditor::addFields
// writes them; detection itself never modifies the document.
class FormDetector {
public:
    static QList<FormEditor::NewField> detect(const QString& inputPath, QString* error);
};
