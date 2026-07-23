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

#include "backends/Watermarker.h"

class QLineEdit;
class QSpinBox;
class QComboBox;

// Collects Bates numbering settings (prefix/suffix, start, digits, corner).
class BatesDialog : public QDialog {
    Q_OBJECT

public:
    explicit BatesDialog(QWidget* parent = nullptr);

    Watermarker::BatesOptions options() const;

private:
    QLineEdit* m_prefix = nullptr;
    QLineEdit* m_suffix = nullptr;
    QSpinBox* m_start = nullptr;
    QSpinBox* m_digits = nullptr;
    QComboBox* m_corner = nullptr;
};
