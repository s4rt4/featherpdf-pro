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

#include <QHash>
#include <QString>
#include <QVariant>
#include <QWidget>

class QStackedWidget;
class QLabel;
class QToolButton;
class QWidget;

// The Forms rail panel: lists a document's fillable AcroForm fields with inline
// editors (text / checkbox / dropdown). Save writes the values into the document
// via Poppler and the caller opens the result. Built lazily on first show.
class FormPanel : public QWidget {
    Q_OBJECT

public:
    explicit FormPanel(QWidget* parent = nullptr);

    void setDocumentPath(const QString& path);
    void clear();
    void reload(); // re-read fields from the current path (after an authoring edit)

signals:
    // Emitted when Save is pressed; values are keyed by Poppler field id.
    void saveRequested(const QHash<int, QVariant>& values);
    void addFieldRequested();                       // "Add field…" pressed
    void prepareFormRequested();                    // "Prepare Form" (auto-detect) pressed
    void moveFieldRequested(const QString& name);    // reposition an existing field
    void deleteFieldRequested(const QString& name);  // remove an existing field

protected:
    void showEvent(QShowEvent* event) override;

private:
    void rebuild();

    QString m_path;
    bool m_dirty = false;
    QHash<int, QVariant> m_values; // field id → current editor value

    QStackedWidget* m_stack = nullptr;
    QWidget* m_form = nullptr;        // scroll content holding the editors
    QWidget* m_fieldHost = nullptr;   // re-created each rebuild
    QLabel* m_empty = nullptr;
    QToolButton* m_save = nullptr;
    QToolButton* m_add = nullptr;
    QToolButton* m_prepare = nullptr;
};
