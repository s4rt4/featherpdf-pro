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

#include <QWidget>

class QLabel;
class QPushButton;

// The redaction action strip, shown beneath the command bar while redaction mode
// is active: a hint, a live mark count, and Apply / Clear / Done. Drawing the
// marks happens in the page view; this bar only drives the actions.
class RedactionBar : public QWidget {
    Q_OBJECT

public:
    explicit RedactionBar(QWidget* parent = nullptr);

    void setCount(int count); // updates the label and enables/disables Apply

signals:
    void applyRequested();
    void clearRequested();
    void doneRequested();

private:
    QLabel* m_label = nullptr;
    QPushButton* m_apply = nullptr;
    QPushButton* m_clear = nullptr;
};
