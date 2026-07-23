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
class QPushButton;

// A themed prompt for the password that opens an encrypted document. Matches the
// app's other dialogs (vs. the dated native QInputDialog). Reusable across retry
// attempts: call setError() to show an "incorrect password" note and re-exec.
class PasswordDialog : public QDialog {
    Q_OBJECT

public:
    explicit PasswordDialog(const QString& fileName, QWidget* parent = nullptr);

    QString password() const;
    void setError(const QString& message); // shows a retry message and clears the field

private:
    QLineEdit* m_password = nullptr;
    QLabel* m_error = nullptr;
    QPushButton* m_open = nullptr;
};
