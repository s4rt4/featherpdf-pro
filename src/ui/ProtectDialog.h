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

class QLineEdit;
class QLabel;
class QCheckBox;
class QPushButton;

// Collects how to encrypt a document (AES-256): an optional open password and
// per-recipient permissions (printing / copying / editing). At least a password
// or one restriction is required. The dialog only gathers the choice - the
// caller performs the encryption.
class ProtectDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProtectDialog(const QString& docName, QWidget* parent = nullptr);

    QString password() const; // open password (may be empty)
    bool allowPrinting() const;
    bool allowCopying() const;
    bool allowEditing() const;

private:
    void validate();

    QLineEdit* m_password = nullptr;
    QLineEdit* m_confirm = nullptr;
    QCheckBox* m_allowPrint = nullptr;
    QCheckBox* m_allowCopy = nullptr;
    QCheckBox* m_allowEdit = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_protect = nullptr;
};
