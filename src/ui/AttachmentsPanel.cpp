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

#include "ui/AttachmentsPanel.h"

#include "ui/Theme.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <poppler-qt6.h>

AttachmentsPanel::AttachmentsPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("AttachmentsPanel"));

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    m_stack = new QStackedWidget(this);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("AttachmentsList"));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setWordWrap(true);

    m_empty = new QLabel(tr("This document has no attachments."), this);
    m_empty->setObjectName(QStringLiteral("OutlineEmpty"));
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);
    m_empty->setContentsMargins(16, 16, 16, 16);

    m_stack->addWidget(m_list);  // 0
    m_stack->addWidget(m_empty); // 1
    col->addWidget(m_stack);

    connect(m_list, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* item) { saveRow(m_list->row(item)); });
}

AttachmentsPanel::~AttachmentsPanel() = default;

void AttachmentsPanel::setDocumentPath(const QString& path) {
    if (path == m_path && m_doc)
        return;
    m_path = path;
    m_dirty = true;
    if (isVisible())
        rebuild();
}

void AttachmentsPanel::clear() {
    m_path.clear();
    m_dirty = false;
    m_files.clear();
    m_doc.reset();
    m_list->clear();
    m_stack->setCurrentWidget(m_empty);
}

void AttachmentsPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (m_dirty)
        rebuild();
}

void AttachmentsPanel::rebuild() {
    m_dirty = false;
    m_files.clear();
    m_list->clear();
    m_doc.reset();

    if (m_path.isEmpty()) {
        m_stack->setCurrentWidget(m_empty);
        return;
    }

    m_doc = Poppler::Document::load(m_path);
    if (!m_doc || m_doc->isLocked() || !m_doc->hasEmbeddedFiles()) {
        m_stack->setCurrentWidget(m_empty);
        return;
    }

    m_files = m_doc->embeddedFiles();
    const QLocale locale;
    for (Poppler::EmbeddedFile* f : std::as_const(m_files)) {
        if (!f)
            continue;
        const QString name = f->name().isEmpty() ? tr("(unnamed attachment)") : f->name();
        const QString size = f->size() >= 0 ? locale.formattedDataSize(f->size()) : QString();
        auto* item = new QListWidgetItem(m_list);
        item->setText(size.isEmpty() ? name : tr("%1\n%2").arg(name, size));
        item->setIcon(Theme::instance().icon(QStringLiteral("attachment"),
                                              Theme::instance().palette().dim));
        QString tip = tr("%1 - double-click to save").arg(name);
        if (!f->description().isEmpty())
            tip += QStringLiteral("\n") + f->description();
        item->setToolTip(tip);
    }
    m_stack->setCurrentWidget(m_files.isEmpty() ? static_cast<QWidget*>(m_empty)
                                                : static_cast<QWidget*>(m_list));
}

void AttachmentsPanel::saveRow(int row) {
    if (row < 0 || row >= m_files.size())
        return;
    Poppler::EmbeddedFile* f = m_files.at(row);
    if (!f)
        return;

    const QString suggested =
        QDir(QDir::homePath()).filePath(f->name().isEmpty() ? tr("attachment") : f->name());
    const QString out = QFileDialog::getSaveFileName(this, tr("Save attachment"), suggested);
    if (out.isEmpty())
        return;

    QFile file(out);
    if (!file.open(QIODevice::WriteOnly) || file.write(f->data()) < 0) {
        QMessageBox::warning(this, tr("Save attachment"),
                             tr("Could not write “%1”.").arg(out));
        return;
    }
}
