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

#include "backends/StampLibrary.h"

class QComboBox;
class QLineEdit;
class QCheckBox;
class QLabel;

// Picks a rubber-stamp preset and its dynamic add-ons (current date and/or a
// name). After it's accepted the user drags to place the stamp on the page.
class StampDialog : public QDialog {
    Q_OBJECT

public:
    explicit StampDialog(QWidget* parent = nullptr);

    StampLibrary::Preset preset() const;
    bool includeDate() const;       // append today's date to the caption
    QString name() const;           // the name add-on, or empty when omitted

private:
    void syncRows(); // show the name field only when the toggle is on
    void refreshPreview();

    QComboBox* m_preset = nullptr;
    QCheckBox* m_date = nullptr;
    QCheckBox* m_useName = nullptr;
    QLineEdit* m_name = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_preview = nullptr;
};
