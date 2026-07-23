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
#include <QStringList>

#include "backends/FormEditor.h"

class QComboBox;
class QLineEdit;
class QCheckBox;
class QLabel;

// Collects the properties of a new AcroForm field. The field is placed on the
// current page (a sensible default box); position editing is a future step.
class FormFieldDialog : public QDialog {
    Q_OBJECT

public:
    explicit FormFieldDialog(QWidget* parent = nullptr);

    FormEditor::Type fieldType() const;
    QString fieldName() const;
    QString defaultValue() const;
    bool checked() const;
    QStringList options() const; // dropdown choices, split from a comma list
    FormEditor::Format format() const; // Text input format / validation

private:
    void syncRows(); // show only the rows relevant to the chosen type

    QComboBox* m_type = nullptr;
    QLineEdit* m_name = nullptr;
    QLineEdit* m_default = nullptr;
    QCheckBox* m_checked = nullptr;
    QLineEdit* m_options = nullptr;
    QComboBox* m_format = nullptr;
    QLabel* m_defaultLabel = nullptr;
    QLabel* m_optionsLabel = nullptr;
    QLabel* m_formatLabel = nullptr;
};
