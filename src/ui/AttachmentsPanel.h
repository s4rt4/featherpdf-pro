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

#include <QList>
#include <QString>
#include <QWidget>
#include <memory>

namespace Poppler {
class Document;
class EmbeddedFile;
} // namespace Poppler

class QListWidget;
class QStackedWidget;
class QLabel;

// The Attachments rail panel: lists the document's embedded files (PDF file
// attachments) via Poppler, and extracts any of them to disk on double-click.
// Read from the file on disk; content is built lazily the first time the panel
// is shown so opening a document stays instant.
class AttachmentsPanel : public QWidget {
    Q_OBJECT

public:
    explicit AttachmentsPanel(QWidget* parent = nullptr);
    ~AttachmentsPanel() override;

    void setDocumentPath(const QString& path);
    void clear();

protected:
    void showEvent(QShowEvent* event) override;

private:
    void rebuild();
    void saveRow(int row);

    QString m_path;
    bool m_dirty = false;
    std::unique_ptr<Poppler::Document> m_doc; // kept alive so EmbeddedFile data stays valid
    QList<Poppler::EmbeddedFile*> m_files;

    QListWidget* m_list = nullptr;
    QStackedWidget* m_stack = nullptr;
    QLabel* m_empty = nullptr;
};
