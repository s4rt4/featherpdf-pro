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
class QRadioButton;

// Chooses which pages of an already-picked source PDF to insert into the current
// document, and where. The user types a page list like "1-3, 5" (1-based, over
// the source) and picks a position relative to the current page.
class InsertDialog : public QDialog {
    Q_OBJECT

public:
    // `sourceName` is shown in the prompt; `sourcePageCount` bounds the range
    // field; `currentPage` (1-based) and `docPageCount` drive the position
    // choices and the resulting insert slot.
    InsertDialog(const QString& sourceName, int sourcePageCount, int currentPage, int docPageCount,
                 QWidget* parent = nullptr);

    // 0-based source pages to insert, in typed order; empty if none in range.
    QVector<int> sourcePages() const;

    // The display slot the chosen pages should land before (0..docPageCount).
    int insertAtSlot() const;

private:
    int m_sourcePageCount;
    int m_currentPage;   // 1-based
    int m_docPageCount;
    QLineEdit* m_pages = nullptr;
    QRadioButton* m_atBeginning = nullptr;
    QRadioButton* m_beforeCurrent = nullptr;
    QRadioButton* m_afterCurrent = nullptr;
    QRadioButton* m_atEnd = nullptr;
};
