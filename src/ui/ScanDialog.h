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
#include <QString>

#include "backends/Scanner.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

// Configures a scan: which device, resolution, colour mode, how many pages, and
// whether to run OCR afterwards. The caller drives Scanner + Converter once the
// dialog is accepted. The device list is fetched up front and can be refreshed.
class ScanDialog : public QDialog {
    Q_OBJECT

public:
    explicit ScanDialog(QWidget* parent = nullptr);

    QString deviceName() const;     // SANE device id ("" → default device)
    int resolution() const;         // DPI
    Scanner::Mode mode() const;     // Color / Gray
    int pageCount() const;
    bool runOcr() const;
    QString ocrLanguage() const;    // Tesseract code, valid only when runOcr()

private:
    void refreshDevices();

    QComboBox* m_device = nullptr;
    QPushButton* m_refresh = nullptr;
    QComboBox* m_resolution = nullptr;
    QComboBox* m_mode = nullptr;
    QSpinBox* m_pages = nullptr;
    QCheckBox* m_ocr = nullptr;
    QComboBox* m_ocrLang = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_scan = nullptr;
};
