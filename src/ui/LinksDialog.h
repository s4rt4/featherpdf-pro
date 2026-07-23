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

#include "backends/LinkEditor.h"

#include <QDialog>
#include <QList>
#include <QVector>

class QCheckBox;
class QLineEdit;

// Lists every hyperlink in the document so URLs can be edited and links deleted.
class LinksDialog : public QDialog {
    Q_OBJECT

public:
    explicit LinksDialog(const QList<LinkEditor::Link>& links, QWidget* parent = nullptr);

    // The changes the user made (edited URLs + deletions); empty if nothing changed.
    QList<LinkEditor::Edit> edits() const;

private:
    struct Row {
        LinkEditor::Link link;
        QLineEdit* url = nullptr; // null for non-URI (read-only) links
        QCheckBox* del = nullptr;
    };
    QVector<Row> m_rows;
};
