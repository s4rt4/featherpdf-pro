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
#include <QStringList>

class QListWidget;
class QPushButton;

// Collects an ordered list of PDF files to merge into one. The list is
// reorderable (drag or the up/down buttons); files are combined top to bottom.
// The dialog only gathers the choice - the caller runs the merge and opens the
// result.
class CombineDialog : public QDialog {
    Q_OBJECT

public:
    // `initialPath` (if non-empty) seeds the list with the current document.
    explicit CombineDialog(const QString& initialPath, QWidget* parent = nullptr);

    QStringList paths() const; // ordered, top to bottom

private:
    void addFiles();
    void removeSelected();
    void moveSelected(int delta);
    void addPath(const QString& path);
    void updateState();

    QListWidget* m_list = nullptr;
    QPushButton* m_combine = nullptr;
    QPushButton* m_remove = nullptr;
    QPushButton* m_up = nullptr;
    QPushButton* m_down = nullptr;
};
