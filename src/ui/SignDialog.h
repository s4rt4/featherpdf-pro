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

class QCheckBox;
class QComboBox;
class QLineEdit;
class QRadioButton;

// Collects the choices for digitally signing the document: which certificate,
// an optional reason and location, the key password, an optional graphical
// (image) appearance, and an optional RFC 3161 trusted timestamp. The signature
// is placed on the current page by the caller.
class SignDialog : public QDialog {
    Q_OBJECT

public:
    SignDialog(const QStringList& certificates, QWidget* parent = nullptr);

    QString certificate() const;
    QString reason() const;
    QString location() const;
    QString password() const;

    // Empty unless the user chose a graphical signature with a valid image file.
    QString imagePath() const;
    // Whether to request a trusted timestamp, and the TSA URL to use.
    bool wantsTimestamp() const;
    QString tsaUrl() const;

private:
    QComboBox* m_cert = nullptr;
    QLineEdit* m_reason = nullptr;
    QLineEdit* m_location = nullptr;
    QLineEdit* m_password = nullptr;
    QRadioButton* m_apprText = nullptr;
    QRadioButton* m_apprImage = nullptr;
    QLineEdit* m_imagePath = nullptr;
    QCheckBox* m_timestamp = nullptr;
    QLineEdit* m_tsaUrl = nullptr;
};
