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

#include <QVector>
#include <QWidget>

class QLineEdit;
class QLabel;
class QToolButton;

// The Find bar (ui-guidelines §7 find group). A slim strip beneath the command
// toolbar: a query field, a live "n of m" count, previous/next, and close.
// Enter finds the next match, Shift+Enter the previous, Esc closes.
class FindBar : public QWidget {
    Q_OBJECT

public:
    explicit FindBar(QWidget* parent = nullptr);

    void activate();                          // show, focus, select the query
    void setResult(int current, int total);   // current is 1-based; 0 total → "No results"
    QString query() const;

signals:
    void queryChanged(const QString& text);
    void findNext();
    void findPrevious();
    void closed();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    QToolButton* addButton(const QString& iconName, const QString& tip);
    void refreshIcons();

    QLineEdit* m_input = nullptr;
    QLabel* m_count = nullptr;
    QVector<QPair<QToolButton*, QString>> m_iconButtons;
};
