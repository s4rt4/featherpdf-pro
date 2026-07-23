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
#include <QVector>

class QLineEdit;

// A themed prompt for extracting a subset of pages into a new PDF. The user
// types a page list like "1-3, 5, 8-10" (1-based, over the current display
// arrangement); the dialog hands back the matching 0-based display indices in
// the order they were typed.
class ExtractDialog : public QDialog {
    Q_OBJECT

public:
    // `pageCount` total pages in the current arrangement; `current` the 1-based
    // page to preselect in the field.
    ExtractDialog(int pageCount, int current, QWidget* parent = nullptr);

    // 0-based display indices to extract, in typed order; empty if the field
    // names no page in range.
    QVector<int> selectedDisplayIndices() const;

private:
    int m_pageCount;
    QLineEdit* m_pages = nullptr;
};
