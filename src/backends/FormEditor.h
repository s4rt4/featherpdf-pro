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

#include <QRectF>
#include <QString>
#include <QStringList>

// The form-authoring backend (QPDF). Creates AcroForm fields — the complement to
// FormFiller, which fills existing ones. A field is written as a combined
// field/widget annotation on a page and registered in the catalog's /AcroForm;
// /NeedAppearances is set so any viewer regenerates the field's appearance.
class FormEditor {
public:
    enum class Type { Text, CheckBox, Dropdown, PushButton, Radio };

    // Optional input formatting/validation for a Text field. Each non-None value
    // writes the standard Acrobat field-format JavaScript (/AA /F format + /K
    // keystroke, the AFNumber_/AFPercent_/AFDate_/AFTime_ helpers) so conforming
    // viewers format and validate as the user types. Our own viewer fills the
    // raw value; the actions travel with the document for Acrobat/Foxit/etc.
    enum class Format { None, Number, Currency, Percent, Date, Time };

    struct NewField {
        Type type = Type::Text;
        QString name;            // field name (/T); should be unique in the document
        QString defaultValue;    // Text: initial value; PushButton: caption
        bool checked = false;    // CheckBox: initial state
        QStringList options;     // Dropdown: choices
        int page = 0;            // 0-based page to place it on
        QRectF rect;             // normalized [0,1], top-left origin (displayed page)
        Format format = Format::None; // Text only: input format / validation
    };

    // Add `field` to `inputPath` and write `outputPath`. Lossless apart from the
    // new field. Temp file + atomic rename, so `outputPath` may equal `inputPath`.
    // Returns true on success; on failure fills *error with a friendly message.
    // (For Type::Radio use addRadioGroup instead — a radio set needs one widget
    // per option.)
    static bool addField(const QString& inputPath, const QString& outputPath,
                         const NewField& field, QString* error);

    // Add several fields in one pass (one QPDF read/write), used by Prepare Form.
    // Type::Radio entries are skipped (use addRadioGroup). Temp file + atomic
    // rename. Returns true on success; *count (if given) gets the number written.
    static bool addFields(const QString& inputPath, const QString& outputPath,
                          const QList<NewField>& fields, int* count, QString* error);

    // Remove the top-level field named `name` (and its widget annotations, incl.
    // every kid of a radio group) from `inputPath`, writing `outputPath`. Temp
    // file + atomic rename. Returns true on success (false if no such field).
    static bool deleteField(const QString& inputPath, const QString& outputPath,
                            const QString& name, QString* error);

    // Reposition the single-widget field named `name` to `normRect` (normalized
    // [0,1], top-left origin, on the field's own page) and regenerate its
    // appearance at the new size. Radio groups (multiple widgets) are rejected.
    // Temp file + atomic rename. Returns true on success.
    static bool setFieldRect(const QString& inputPath, const QString& outputPath,
                             const QString& name, const QRectF& normRect, QString* error);

    // Add a radio-button group named `name` with one button per entry in
    // `options`, all on page `page`. `firstRect` (normalized [0,1], top-left
    // origin) positions the first button; the rest tile downward at the same
    // size. Written as a parent /Btn field with a widget kid per option. Temp
    // file + atomic rename. Returns true on success; on failure fills *error.
    static bool addRadioGroup(const QString& inputPath, const QString& outputPath,
                              const QString& name, const QStringList& options, int page,
                              const QRectF& firstRect, QString* error);
};
