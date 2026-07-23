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

#include "ui/Viewport.h"

#include "core/FeatherDocument.h"
#include "ui/PageView.h"

#include <QVBoxLayout>

Viewport::Viewport(QWidget* parent) : QWidget(parent), m_view(new PageView(this)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    connect(m_view, &PageView::zoomChanged, this,
            [this](double f) { emit zoomChanged(f); });
    connect(m_view, &PageView::currentPageChanged, this,
            [this](int page) { emit currentPageChanged(page); });
    connect(m_view, &PageView::searchResultsChanged, this,
            [this](int count) { emit searchResultsChanged(count); });
    connect(m_view, &PageView::layoutModeChanged, this,
            [this](PageView::LayoutMode m) { emit layoutModeChanged(m); });
    connect(m_view, &PageView::redactionsChanged, this,
            [this](int count) { emit redactionsChanged(count); });
    connect(m_view, &PageView::highlightsChanged, this,
            [this](int count) { emit highlightsChanged(count); });
    connect(m_view, &PageView::notesChanged, this,
            [this](int count) { emit notesChanged(count); });
    connect(m_view, &PageView::inksChanged, this,
            [this](int count) { emit inksChanged(count); });
    connect(m_view, &PageView::shapesChanged, this,
            [this](int count) { emit shapesChanged(count); });
    connect(m_view, &PageView::noteRequested, this,
            [this](int slot, QPointF pos) { emit noteRequested(slot, pos); });
    connect(m_view, &PageView::textBoxRequested, this,
            [this](int slot, QRectF rect) { emit textBoxRequested(slot, rect); });
    connect(m_view, &PageView::fieldRectDrawn, this,
            [this](int slot, QRectF rect) { emit fieldRectDrawn(slot, rect); });
    connect(m_view, &PageView::snapshotRegion, this,
            [this](int slot, QRectF rect) { emit snapshotRegion(slot, rect); });
}

Viewport::~Viewport() = default;

void Viewport::setRedactionMode(bool on) { m_view->setRedactionMode(on); }
bool Viewport::redactionMode() const { return m_view->redactionMode(); }
void Viewport::setFieldPlacementMode(bool on) { m_view->setFieldPlacementMode(on); }
bool Viewport::fieldPlacementMode() const { return m_view->fieldPlacementMode(); }
void Viewport::setSnapshotMode(bool on) { m_view->setSnapshotMode(on); }
bool Viewport::snapshotMode() const { return m_view->snapshotMode(); }
void Viewport::setMeasureMode(bool on) { m_view->setMeasureMode(on); }
bool Viewport::measureMode() const { return m_view->measureMode(); }
void Viewport::setMeasureKind(PageView::MeasureKind kind) { m_view->setMeasureKind(kind); }
void Viewport::setMeasureUnit(Measurer::Unit unit) { m_view->setMeasureUnit(unit); }
void Viewport::clearMeasure() { m_view->clearMeasure(); }
QHash<int, QList<QRectF>> Viewport::redactionMarks() const { return m_view->redactionMarks(); }
void Viewport::addRedactions(const QHash<int, QList<QRectF>>& marks) {
    m_view->addRedactions(marks);
}
int Viewport::redactionCount() const { return m_view->redactionCount(); }
void Viewport::clearRedactions() { m_view->clearRedactions(); }

void Viewport::setHighlightMode(bool on) { m_view->setHighlightMode(on); }
bool Viewport::highlightMode() const { return m_view->highlightMode(); }
void Viewport::setAnnotationTool(PageView::AnnotTool tool) { m_view->setAnnotationTool(tool); }
void Viewport::setHighlightColor(const QColor& color) { m_view->setHighlightColor(color); }
QHash<int, QList<QPair<QRectF, QColor>>> Viewport::highlightMarks() const {
    return m_view->highlightMarks();
}
QHash<int, QList<QPair<QPointF, QString>>> Viewport::noteMarks() const {
    return m_view->noteMarks();
}
QHash<int, QList<QPair<QPolygonF, QColor>>> Viewport::inkMarks() const {
    return m_view->inkMarks();
}
QHash<int, QList<PageView::ShapeMark>> Viewport::shapeMarks() const {
    return m_view->shapeMarks();
}
int Viewport::highlightCount() const { return m_view->highlightCount(); }
int Viewport::noteCount() const { return m_view->noteCount(); }
int Viewport::inkCount() const { return m_view->inkCount(); }
int Viewport::shapeCount() const { return m_view->shapeCount(); }
void Viewport::addTextBox(int slot, const QRectF& rect, const QString& text) {
    m_view->addTextBox(slot, rect, text);
}
void Viewport::addNote(int slot, const QPointF& pos, const QString& text) {
    m_view->addNote(slot, pos, text);
}
void Viewport::clearAnnotations() { m_view->clearAnnotations(); }

void Viewport::setDocument(FeatherDocument* doc) {
    if (m_doc)
        disconnect(m_doc, nullptr, this, nullptr);
    m_doc = doc;
    m_view->setDocument(doc ? doc->pdf() : nullptr);
    if (doc) {
        m_view->setArrangement(doc->pageOrder(), doc->rotations());
        // Follow façade edits: a single slot's rotation, or a structure change.
        connect(doc, &FeatherDocument::pageEdited, this,
                [this, doc](int slot) { m_view->setRotation(slot, doc->rotation(slot)); });
        connect(doc, &FeatherDocument::arrangementChanged, this, [this, doc] {
            m_view->setArrangement(doc->pageOrder(), doc->rotations());
            emit pageCountChanged(pageCount());
            emit currentPageChanged(currentPage());
        });
    }
    emit pageCountChanged(pageCount());
    emit currentPageChanged(currentPage());
    emit zoomChanged(zoomFactor());
}

void Viewport::clear() {
    m_doc = nullptr;
    m_view->setDocument(nullptr);
    emit pageCountChanged(0);
}

bool Viewport::hasDocument() const {
    return m_doc != nullptr && m_doc->isLoaded();
}

double Viewport::zoomFactor() const {
    return m_view->zoom();
}

void Viewport::zoomIn() {
    m_view->zoomIn();
}

void Viewport::zoomOut() {
    m_view->zoomOut();
}

void Viewport::zoomActualSize() {
    m_view->zoomActualSize();
}

void Viewport::fitToWidth() {
    m_view->fitToWidth();
}

void Viewport::fitWholePage() {
    m_view->fitWholePage();
}

void Viewport::setZoomFactor(double factor) {
    m_view->setZoom(factor);
}

void Viewport::setLayoutMode(PageView::LayoutMode mode) {
    m_view->setLayoutMode(mode);
}

PageView::LayoutMode Viewport::layoutMode() const {
    return m_view->layoutMode();
}

int Viewport::currentPage() const {
    return m_view->currentPage();
}

int Viewport::pageCount() const {
    return m_doc ? m_doc->pageCount() : 0;
}

void Viewport::goToPage(int page) {
    m_view->goToPage(page);
}

int Viewport::search(const QString& query) {
    return m_view->search(query);
}

void Viewport::clearSearch() {
    m_view->clearSearch();
}

void Viewport::showSearchResult(int index) {
    m_view->showSearchResult(index);
}

int Viewport::searchResultCount() const {
    return m_view->searchResultCount();
}
