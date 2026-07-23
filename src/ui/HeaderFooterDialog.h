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

#include "backends/Watermarker.h"

#include <QDialog>

class QLineEdit;
class QSpinBox;

// Collects header/footer text for the six page slots plus font size and the
// starting page number.
class HeaderFooterDialog : public QDialog {
    Q_OBJECT

public:
    explicit HeaderFooterDialog(QWidget* parent = nullptr);

    Watermarker::HeaderFooterOptions options() const;
    bool anyText() const;

private:
    QLineEdit* m_hL = nullptr;
    QLineEdit* m_hC = nullptr;
    QLineEdit* m_hR = nullptr;
    QLineEdit* m_fL = nullptr;
    QLineEdit* m_fC = nullptr;
    QLineEdit* m_fR = nullptr;
    QSpinBox* m_fontSize = nullptr;
    QSpinBox* m_start = nullptr;
};
