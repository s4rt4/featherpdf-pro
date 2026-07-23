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

class QDoubleSpinBox;
class QLineEdit;
class QRadioButton;

// Crops pages by trimming a margin off each edge (in millimetres, in the page's
// displayed orientation). The crop can target every page or a typed page list.
class CropDialog : public QDialog {
    Q_OBJECT

public:
    explicit CropDialog(int pageCount, QWidget* parent = nullptr);

    // Margins in points (72pt = 1in), ready for PdfEditor::CropMargins.
    double leftPt() const;
    double rightPt() const;
    double topPt() const;
    double bottomPt() const;

    // True if every margin is zero (nothing to do).
    bool isEmptyCrop() const;

    // 0-based display slots to crop; empty means every page.
    QVector<int> selectedSlots() const;

private:
    int m_pageCount;
    QDoubleSpinBox* m_top = nullptr;
    QDoubleSpinBox* m_bottom = nullptr;
    QDoubleSpinBox* m_left = nullptr;
    QDoubleSpinBox* m_right = nullptr;
    QRadioButton* m_allPages = nullptr;
    QRadioButton* m_somePages = nullptr;
    QLineEdit* m_rangeText = nullptr;
};
