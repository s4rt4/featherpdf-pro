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

class QButtonGroup;

// Gathers the app's persisted preferences in one themed dialog: appearance
// (theme) and the default view applied to newly opened documents.
class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    enum class Theme { System, Light, Dark };

    PreferencesDialog(QWidget* parent = nullptr);

    Theme theme() const;
    QString layout() const;   // "single" | "continuous" | "twoup"
    QString zoom() const;     // "fitwidth" | "fitpage" | "actual"
    QString language() const; // "system" | "en" | "id"

private:
    QButtonGroup* m_theme = nullptr;
    QButtonGroup* m_layout = nullptr;
    QButtonGroup* m_zoom = nullptr;
    QButtonGroup* m_language = nullptr;
};
