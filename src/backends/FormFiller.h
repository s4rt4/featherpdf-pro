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

#include <QHash>
#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVariant>

// The form-filling backend (Poppler-Qt6). Reads a document's AcroForm fields and
// writes user-entered values back, saving so any viewer (including our QtPdf one
// on reopen) shows the filled form - Poppler regenerates field appearances on save.
class FormFiller {
public:
    enum class Kind { Text, CheckBox, ComboBox, Radio, ListBox, Unsupported };

    struct Field {
        int id = -1;          // Poppler field id (stable within a document load)
        int page = 0;         // original 0-based page index
        QString name;         // fully-qualified field name
        QRectF rect;          // normalized [0,1] page rect
        Kind kind = Kind::Unsupported;
        bool readOnly = false;
        QString textValue;    // current text (Text)
        bool checked = false; // current state (CheckBox)
        QStringList options;  // available choices (ComboBox)
        QString currentChoice;
    };

    // Read all fillable fields, ordered by page then position.
    static QList<Field> read(const QString& path);

    // Apply `values` (keyed by Field::id) to `inputPath` and write `outputPath`.
    // Text→QString, CheckBox→bool, ComboBox→QString (chosen option). Temp file +
    // atomic rename. Returns true on success; on failure fills *error.
    static bool save(const QString& inputPath, const QString& outputPath,
                     const QHash<int, QVariant>& values, QString* error);
};
