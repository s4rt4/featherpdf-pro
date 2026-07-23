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

#include "backends/Splitter.h"

class QRadioButton;
class QSpinBox;
class QLineEdit;

// Chooses how to split a document: one file per page, every N pages, or by ranges.
class SplitDialog : public QDialog {
    Q_OBJECT

public:
    explicit SplitDialog(int pageCount, QWidget* parent = nullptr);

    Splitter::Mode mode() const;
    int everyN() const;
    QString ranges() const;

private:
    QRadioButton* m_each = nullptr;
    QRadioButton* m_everyN = nullptr;
    QRadioButton* m_ranges = nullptr;
    QSpinBox* m_n = nullptr;
    QLineEdit* m_rangesText = nullptr;
};
