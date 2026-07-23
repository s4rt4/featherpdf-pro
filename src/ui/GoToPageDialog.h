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

class QSpinBox;

// A themed "go to page" prompt (replacing the dated native QInputDialog).
class GoToPageDialog : public QDialog {
    Q_OBJECT

public:
    // `pageCount` total pages, `current` the 1-based page to preselect.
    GoToPageDialog(int pageCount, int current, QWidget* parent = nullptr);

    int selectedPage() const; // 1-based

private:
    QSpinBox* m_spin = nullptr;
};
