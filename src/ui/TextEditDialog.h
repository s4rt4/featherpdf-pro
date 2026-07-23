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

#include "backends/TextEditor.h"

class QListWidget;
class QPlainTextEdit;

// Lists the editable text boxes (added via the Text tool) and lets the user
// rewrite or delete the selected one. The chosen action is read back after exec.
class TextEditDialog : public QDialog {
    Q_OBJECT

public:
    enum class Action { None, Save, Delete };

    explicit TextEditDialog(const QList<TextEditor::TextBox>& boxes, QWidget* parent = nullptr);

    Action action() const { return m_action; }
    int selectedIndex() const;
    QString text() const;

private:
    void syncToSelection();

    QList<TextEditor::TextBox> m_boxes;
    QListWidget* m_list = nullptr;
    QPlainTextEdit* m_editor = nullptr;
    Action m_action = Action::None;
};
