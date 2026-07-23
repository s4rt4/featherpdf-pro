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

#include <QDialog>
#include <QList>

#include "backends/FormEditor.h"

class QTableWidget;

// Reviews the fields FormDetector found before they are written. The user can
// untick any row (Include), rename it, change its Type (Text / Check box) and,
// for Text, pick an input Format. selectedFields() returns the edited, ticked
// rows ready for FormEditor::addFields. Themed per the app's dialog conventions.
class PrepareFormDialog : public QDialog {
    Q_OBJECT

public:
    explicit PrepareFormDialog(const QList<FormEditor::NewField>& detected,
                               QWidget* parent = nullptr);

    QList<FormEditor::NewField> selectedFields() const;

private:
    QList<FormEditor::NewField> m_fields; // detection results, in row order
    QTableWidget* m_table = nullptr;
};
