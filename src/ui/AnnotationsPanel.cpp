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

#include "ui/AnnotationsPanel.h"

#include "ui/Theme.h"

#include <QFutureWatcher>
#include <QLabel>
#include <QListWidget>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <poppler-annotation.h>
#include <poppler-qt6.h>

namespace {
// A readable name for the markup subtypes we surface.
QString kindLabel(Poppler::Annotation::SubType t) {
    switch (t) {
    case Poppler::Annotation::AText: return QObject::tr("Note");
    case Poppler::Annotation::ALine: return QObject::tr("Line");
    case Poppler::Annotation::AGeom: return QObject::tr("Shape");
    case Poppler::Annotation::AHighlight: return QObject::tr("Highlight");
    case Poppler::Annotation::AStamp: return QObject::tr("Stamp");
    case Poppler::Annotation::AInk: return QObject::tr("Ink");
    case Poppler::Annotation::ACaret: return QObject::tr("Caret");
    case Poppler::Annotation::AFileAttachment: return QObject::tr("File attachment");
    case Poppler::Annotation::ASound: return QObject::tr("Sound");
    case Poppler::Annotation::AMovie: return QObject::tr("Movie");
    case Poppler::Annotation::AScreen: return QObject::tr("Screen");
    case Poppler::Annotation::ARichMedia: return QObject::tr("Rich media");
    default: return QObject::tr("Annotation");
    }
}

// Scan a document for markup annotations off the UI thread. Loads its own
// Poppler document so it shares nothing with the rest of the app.
QVector<AnnotationEntry> scanAnnotations(const QString& path) {
    QVector<AnnotationEntry> out;
    auto doc = Poppler::Document::load(path);
    if (!doc || doc->isLocked())
        return out;

    const int pages = doc->numPages();
    for (int i = 0; i < pages; ++i) {
        auto page = doc->page(i);
        if (!page)
            continue;
        for (const auto& ann : page->annotations()) {
            if (!ann)
                continue;
            const Poppler::Annotation::SubType t = ann->subType();
            // Skip links and form widgets - they're navigation/structure, not
            // the user-authored markup this panel is about.
            if (t == Poppler::Annotation::ALink || t == Poppler::Annotation::AWidget)
                continue;
            AnnotationEntry e;
            e.page = i;
            e.kind = kindLabel(t);
            e.text = ann->contents().simplified();
            out.push_back(e);
        }
    }
    return out;
}
} // namespace

AnnotationsPanel::AnnotationsPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("AnnotationsPanel"));

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    m_stack = new QStackedWidget(this);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("AnnotationsList"));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setWordWrap(true);

    m_empty = new QLabel(tr("This document has no annotations."), this);
    m_empty->setObjectName(QStringLiteral("OutlineEmpty"));
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);
    m_empty->setContentsMargins(16, 16, 16, 16);

    m_busy = new QLabel(tr("Scanning for annotations…"), this);
    m_busy->setObjectName(QStringLiteral("OutlineEmpty"));
    m_busy->setAlignment(Qt::AlignCenter);
    m_busy->setWordWrap(true);
    m_busy->setContentsMargins(16, 16, 16, 16);

    m_stack->addWidget(m_list);  // 0
    m_stack->addWidget(m_empty); // 1
    m_stack->addWidget(m_busy);  // 2
    col->addWidget(m_stack);

    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        bool ok = false;
        const int page = item->data(Qt::UserRole).toInt(&ok);
        if (ok && page >= 0)
            emit annotationActivated(page);
    });
}

void AnnotationsPanel::setDocumentPath(const QString& path) {
    if (path == m_path)
        return;
    m_path = path;
    m_dirty = true;
    ++m_scanGen; // any in-flight scan is now stale
    if (isVisible())
        rebuild();
}

void AnnotationsPanel::clear() {
    m_path.clear();
    m_dirty = false;
    ++m_scanGen;
    m_list->clear();
    m_stack->setCurrentWidget(m_empty);
}

void AnnotationsPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (m_dirty)
        rebuild();
}

void AnnotationsPanel::rebuild() {
    m_dirty = false;
    m_list->clear();

    if (m_path.isEmpty()) {
        m_stack->setCurrentWidget(m_empty);
        return;
    }

    m_stack->setCurrentWidget(m_busy);
    const int gen = ++m_scanGen;
    const QString path = m_path;

    auto* watcher = new QFutureWatcher<QVector<AnnotationEntry>>(this);
    connect(watcher, &QFutureWatcher<QVector<AnnotationEntry>>::finished, this,
            [this, watcher, gen] {
                const QVector<AnnotationEntry> entries = watcher->result();
                watcher->deleteLater();
                if (gen != m_scanGen) // a newer document/scan superseded this one
                    return;
                populate(entries);
            });
    watcher->setFuture(QtConcurrent::run(scanAnnotations, path));
}

void AnnotationsPanel::populate(const QVector<AnnotationEntry>& entries) {
    m_list->clear();
    if (entries.isEmpty()) {
        m_stack->setCurrentWidget(m_empty);
        return;
    }

    const QColor dim = Theme::instance().palette().dim;
    for (const AnnotationEntry& e : entries) {
        const QString head = tr("Page %1 · %2").arg(e.page + 1).arg(e.kind);
        auto* item = new QListWidgetItem(m_list);
        item->setText(e.text.isEmpty() ? head : tr("%1\n%2").arg(head, e.text));
        item->setData(Qt::UserRole, e.page);
        item->setIcon(Theme::instance().icon(QStringLiteral("comment"), dim));
        if (!e.text.isEmpty())
            item->setToolTip(e.text);
    }
    m_stack->setCurrentWidget(m_list);
}
