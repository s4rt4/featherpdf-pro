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
class QTimer;

// A transient bottom-center confirmation (ui-guidelines §9). Parented to a host
// widget; it keeps itself centered and auto-dismisses. Used for honest "this
// arrives in a later milestone" notices and, later, real confirmations like
// "3 pages merged".
class Toast : public QWidget {
    Q_OBJECT

public:
    explicit Toast(QWidget* host);

    void show(const QString& message, int milliseconds = 2600);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void reposition();

    QLabel* m_label = nullptr;
    QTimer* m_timer = nullptr;
};
