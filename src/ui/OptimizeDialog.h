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

#include "backends/Optimizer.h"

#include <QDialog>

class QCheckBox;
class QSpinBox;

// Shows the audit (what's taking up space) and lets the user choose how to shrink
// the document: image resolution cap, font unembedding, and stream compression.
class OptimizeDialog : public QDialog {
    Q_OBJECT

public:
    OptimizeDialog(const Optimizer::Audit& audit, QWidget* parent = nullptr);

    Optimizer::Options options() const;

private:
    QCheckBox* m_downsample = nullptr;
    QSpinBox* m_dpi = nullptr;
    QCheckBox* m_dropFonts = nullptr;
    QCheckBox* m_compress = nullptr;
};
