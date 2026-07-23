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

#include <QStringList>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;

// The measure action strip, shown beneath the command bar while measure mode is
// active: a mode selector (Distance / Perimeter / Area), a unit selector (mm /
// cm / in), a usage hint, and Clear / Done. The page view draws the live overlay
// and computes the values; this bar only drives the settings and actions.
class MeasureBar : public QWidget {
    Q_OBJECT

public:
    explicit MeasureBar(QWidget* parent = nullptr);

signals:
    void modeChanged(int mode); // 0 = Distance, 1 = Perimeter, 2 = Area
    void unitChanged(int unit); // 0 = Millimeter, 1 = Centimeter, 2 = Inch
    void clearRequested();
    void doneRequested();

private:
    void applyIcons(); // re-tint the mode icons (checked = accent, rest = text)

    QVector<QPushButton*> m_modes; // mode buttons, in modeChanged() index order
    QStringList m_modeIcons;       // symbolic icon name per mode (parallel to m_modes)
    QVector<QPushButton*> m_units; // unit buttons, in unitChanged() index order
    QPushButton* m_info = nullptr;
    QPushButton* m_clear = nullptr;
};
