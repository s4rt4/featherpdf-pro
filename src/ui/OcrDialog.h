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
#include <QStringList>

class QComboBox;
class QCheckBox;

// Picks the language and the pre-processing clean-up for text recognition (OCR).
class OcrDialog : public QDialog {
    Q_OBJECT

public:
    OcrDialog(const QStringList& languageCodes, QWidget* parent = nullptr);

    QString language() const; // selected Tesseract code, e.g. "eng" (also the fallback)

    bool deskew() const;       // straighten skewed scans
    bool despeckle() const;    // drop isolated specks
    bool binarize() const;     // grayscale + Otsu black/white
    bool autoLanguage() const; // detect the script and pick a language

private:
    QComboBox* m_lang = nullptr;
    QCheckBox* m_deskew = nullptr;
    QCheckBox* m_despeckle = nullptr;
    QCheckBox* m_binarize = nullptr;
    QCheckBox* m_autoLang = nullptr;
};
