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

#include "backends/ImageExporter.h"

#include <QDialog>
#include <QVector>

class QComboBox;
class QLineEdit;
class QSpinBox;

// A themed prompt for rendering pages to image files: pick a format, a DPI, and
// an optional page range (blank = every page).
class ExportImageDialog : public QDialog {
    Q_OBJECT

public:
    explicit ExportImageDialog(int pageCount, int current, QWidget* parent = nullptr);

    ImageExporter::Format format() const;
    int dpi() const;
    // 0-based page indices to render, in page order; blank field = all pages.
    QVector<int> selectedPages() const;

private:
    int m_pageCount;
    QComboBox* m_format = nullptr;
    QSpinBox* m_dpi = nullptr;
    QLineEdit* m_pages = nullptr;
};
