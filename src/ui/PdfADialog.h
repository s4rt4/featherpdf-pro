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

class QLabel;
class QPlainTextEdit;

// Moves a PDF toward PDF/A-1b and runs an optional veraPDF preflight, showing the
// result inline. Self-contained: it owns the convert/validate work so the calling
// window only has to hand it the source file.
class PdfADialog : public QDialog {
    Q_OBJECT

public:
    explicit PdfADialog(const QString& inputPath, QWidget* parent = nullptr);

private slots:
    void doConvert();
    void doValidate();

private:
    void report(const QString& text);

    QString m_inputPath;  // the source PDF
    QString m_lastOutput; // last PDF/A copy written this session (what Validate checks)
    QLabel* m_status = nullptr;
    QPlainTextEdit* m_report = nullptr;
};
