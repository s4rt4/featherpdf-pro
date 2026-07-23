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

#include "ui/PageView.h"

#include "ui/Theme.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfDocumentRenderOptions>
#include <QPdfLink>
#include <QPdfPageRenderer>
#include <QPdfSearchModel>
#include <QScrollBar>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace {
constexpr int kMargin = 18;       // top/bottom/side margin around the page column
constexpr int kSpacing = 14;      // gap between rows
constexpr int kPairGap = 4;       // gap between the two pages of a facing row
constexpr int kMaxCache = 48;     // rendered pages kept in memory
constexpr double kZoomStep = 1.2;
constexpr double kMinZoom = 0.1;
constexpr double kMaxZoom = 8.0;
} // namespace

PageView::PageView(QWidget* parent) : QAbstractScrollArea(parent) {
    setFrameShape(QFrame::NoFrame);
    viewport()->setMouseTracking(true);
    viewport()->installEventFilter(this); // redaction drag (QAbstractScrollArea routes here)
    verticalScrollBar()->setSingleStep(48);
    horizontalScrollBar()->setSingleStep(48);

    m_renderer = new QPdfPageRenderer(this);
    m_renderer->setRenderMode(QPdfPageRenderer::RenderMode::MultiThreaded);
    connect(m_renderer, &QPdfPageRenderer::pageRendered, this,
            [this](int, QSize, const QImage& image, QPdfDocumentRenderOptions, quint64 id) {
                // Drop results from a superseded generation (zoom/rotation/edit).
                const quint64 g = m_reqGen.take(id);
                const int slot = m_reqSlot.take(id);
                if (image.isNull() || g != m_gen || slot < 0)
                    return;
                m_pending.remove(slot);
                QPixmap pm = QPixmap::fromImage(image);
                pm.setDevicePixelRatio(devicePixelRatioF());
                m_cache.insert(slot, pm);
                m_lru.removeAll(slot);
                m_lru.append(slot);
                ++m_renderedCount;
                dropStaleCache();
                viewport()->update();
            });

    m_search = new QPdfSearchModel(this);
    auto emitCount = [this] {
        emit searchResultsChanged(searchResultCount());
        viewport()->update();
    };
    connect(m_search, &QAbstractItemModel::rowsInserted, this, emitCount);
    connect(m_search, &QAbstractItemModel::rowsRemoved, this, emitCount);
    connect(m_search, &QAbstractItemModel::modelReset, this, emitCount);
    connect(m_search, &QAbstractItemModel::layoutChanged, this, emitCount);
}

PageView::~PageView() = default;

void PageView::setDocument(QPdfDocument* doc) {
    m_doc = doc;
    m_cache.clear();
    m_lru.clear();
    m_pending.clear();
    m_reqGen.clear();
    ++m_gen;
    m_currentPage = 0;
    m_currentResult = -1;
    m_renderedCount = 0;
    // Edit-mode marks are tied to this document's slots - reset for the new one.
    m_dragging = false;
    m_dragSlot = -1;
    if (!m_redactions.isEmpty()) {
        m_redactions.clear();
        emit redactionsChanged(0);
    }
    if (!m_highlights.isEmpty()) {
        m_highlights.clear();
        emit highlightsChanged(0);
    }
    if (!m_notes.isEmpty()) {
        m_notes.clear();
        emit notesChanged(0);
    }
    if (!m_inks.isEmpty()) {
        m_inks.clear();
        emit inksChanged(0);
    }
    m_currentStroke.clear();

    m_renderer->setDocument(doc);
    m_search->setSearchString(QString());
    m_search->setDocument(doc);

    // Read every page size once (cheap - no rendering). This is what lets the
    // whole layout be arithmetic, so opening is instant at any page count.
    m_pageSizes.clear();
    m_maxPageWPoints = 1.0;
    if (doc) {
        const int n = doc->pageCount();
        m_pageSizes.reserve(n);
        for (int i = 0; i < n; ++i) {
            const QSizeF s = doc->pagePointSize(i);
            m_pageSizes.append(s);
            m_maxPageWPoints = std::max(m_maxPageWPoints, s.width());
        }
    }
    // Identity arrangement until the façade pushes its edit model.
    const int n = m_pageSizes.size();
    m_order.resize(n);
    for (int i = 0; i < n; ++i)
        m_order[i] = i;
    m_rot.assign(n, 0);

    relayout();
    verticalScrollBar()->setValue(0);
    emit zoomChanged(m_zoom);
    emit currentPageChanged(0);
    viewport()->update();
}

void PageView::buildRows() {
    m_rows.clear();
    const int n = pageCount();
    m_pageRow.assign(n, -1);
    if (n == 0) {
        m_maxRowWPoints = 1.0;
        return;
    }

    if (m_mode == LayoutMode::TwoUp) {
        for (int i = 0; i < n; i += 2) {
            QList<int> row{i};
            if (i + 1 < n)
                row.append(i + 1);
            m_rows.append(row);
        }
    } else if (m_mode == LayoutMode::SinglePage) {
        m_rows.append(QList<int>{std::clamp(m_currentPage, 0, n - 1)});
    } else { // Continuous
        for (int i = 0; i < n; ++i)
            m_rows.append(QList<int>{i});
    }

    m_maxRowWPoints = 1.0;
    for (int r = 0; r < m_rows.size(); ++r) {
        double w = 0;
        for (int slot : m_rows[r]) {
            m_pageRow[slot] = r;
            const int rot = rotationOf(slot);
            const QSizeF& s = sizeOf(slot);
            w += (rot == 90 || rot == 270) ? s.height() : s.width(); // effective width
        }
        m_maxRowWPoints = std::max(m_maxRowWPoints, w);
    }
}

double PageView::rowHeightPx(int row) const {
    double h = 0;
    for (int p : m_rows[row])
        h = std::max(h, pageHeightPx(p));
    return h;
}

double PageView::rowContentWidthPx(int row) const {
    double w = 0;
    for (int p : m_rows[row])
        w += pageWidthPx(p);
    return w + (m_rows[row].size() - 1) * kPairGap;
}

double PageView::contentWidthPx() const {
    return m_maxRowWPoints * m_zoom + (kPairGap + 2 * kMargin);
}

void PageView::relayout() {
    buildRows();
    const int rows = m_rows.size();
    m_rowTop.resize(rows);
    double y = kMargin;
    for (int r = 0; r < rows; ++r) {
        m_rowTop[r] = y;
        y += rowHeightPx(r) + kSpacing;
    }
    const double contentH = (rows > 0) ? (m_rowTop[rows - 1] + rowHeightPx(rows - 1) + kMargin) : 0.0;
    const double contentW = contentWidthPx();

    const int vpH = viewport()->height();
    const int vpW = viewport()->width();
    verticalScrollBar()->setRange(0, std::max(0, int(std::ceil(contentH)) - vpH));
    verticalScrollBar()->setPageStep(vpH);
    horizontalScrollBar()->setRange(0, std::max(0, int(std::ceil(contentW)) - vpW));
    horizontalScrollBar()->setPageStep(vpW);
}

int PageView::rowAtContentY(double y) const {
    const int n = m_rows.size();
    if (n == 0)
        return 0;
    int lo = 0, hi = n - 1, ans = 0;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (m_rowTop[mid] <= y) {
            ans = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return ans;
}

QRect PageView::pageRect(int page) const {
    if (page < 0 || page >= m_pageRow.size())
        return {};
    const int r = m_pageRow[page];
    if (r < 0)
        return {};
    const double offY = verticalScrollBar()->value();
    const double offX = horizontalScrollBar()->value();
    const double centerSpace = std::max(contentWidthPx(), double(viewport()->width()));
    double x = (centerSpace - rowContentWidthPx(r)) / 2.0 - offX;
    for (int p : m_rows[r]) {
        const double pw = pageWidthPx(p);
        if (p == page)
            return QRect(qRound(x), qRound(m_rowTop[r] - offY), qRound(pw), qRound(pageHeightPx(p)));
        x += pw + kPairGap;
    }
    return {};
}

QPointF PageView::normInSlot(int slot, const QPoint& vpPt) const {
    const QRect r = pageRect(slot);
    if (r.width() <= 0 || r.height() <= 0)
        return {};
    return QPointF(std::clamp(double(vpPt.x() - r.x()) / r.width(), 0.0, 1.0),
                   std::clamp(double(vpPt.y() - r.y()) / r.height(), 0.0, 1.0));
}

void PageView::setRedactionMode(bool on) {
    if (m_redactMode == on)
        return;
    m_redactMode = on;
    if (on) {
        m_highlightMode = false; // the drag modes are mutually exclusive
        m_fieldMode = false;
        m_snapshotMode = false;
        m_measureMode = false;
    }
    m_dragging = false;
    m_dragSlot = -1;
    viewport()->setCursor(dragModeActive() ? Qt::CrossCursor : Qt::ArrowCursor);
    viewport()->update();
}

void PageView::setHighlightMode(bool on) {
    if (m_highlightMode == on)
        return;
    m_highlightMode = on;
    if (on) {
        m_redactMode = false;
        m_fieldMode = false;
        m_snapshotMode = false;
        m_measureMode = false;
    }
    m_dragging = false;
    m_dragSlot = -1;
    viewport()->setCursor(dragModeActive() ? Qt::CrossCursor : Qt::ArrowCursor);
    viewport()->update();
}

void PageView::setFieldPlacementMode(bool on) {
    if (m_fieldMode == on)
        return;
    m_fieldMode = on;
    if (on) {
        m_redactMode = false;
        m_highlightMode = false;
        m_snapshotMode = false;
        m_measureMode = false;
    }
    m_dragging = false;
    m_dragSlot = -1;
    viewport()->setCursor(dragModeActive() ? Qt::CrossCursor : Qt::ArrowCursor);
    viewport()->update();
}

void PageView::setSnapshotMode(bool on) {
    if (m_snapshotMode == on)
        return;
    m_snapshotMode = on;
    if (on) {
        m_redactMode = false;
        m_highlightMode = false;
        m_fieldMode = false;
        m_measureMode = false;
    }
    m_dragging = false;
    m_dragSlot = -1;
    viewport()->setCursor(dragModeActive() ? Qt::CrossCursor : Qt::ArrowCursor);
    viewport()->update();
}

void PageView::setMeasureMode(bool on) {
    if (m_measureMode == on)
        return;
    m_measureMode = on;
    if (on) {
        m_redactMode = false; // measuring is mutually exclusive with the drag modes
        m_highlightMode = false;
        m_fieldMode = false;
        m_snapshotMode = false;
    }
    m_measurePoints.clear();
    m_measureSlot = -1;
    m_measureDone = false;
    m_dragging = false;
    m_dragSlot = -1;
    viewport()->setCursor((m_measureMode || dragModeActive()) ? Qt::CrossCursor : Qt::ArrowCursor);
    viewport()->update();
}

void PageView::setMeasureKind(MeasureKind kind) {
    if (m_measureKind == kind)
        return;
    m_measureKind = kind;
    // Switching the kind starts a fresh measurement (point counts differ).
    m_measurePoints.clear();
    m_measureSlot = -1;
    m_measureDone = false;
    viewport()->update();
}

void PageView::setMeasureUnit(Measurer::Unit unit) {
    m_measureUnit = unit;
    viewport()->update(); // only the label changes; the points stand
}

void PageView::clearMeasure() {
    m_measurePoints.clear();
    m_measureSlot = -1;
    m_measureDone = false;
    viewport()->update();
}

QSizeF PageView::displayedPointSize(int slot) const {
    const int r = rotationOf(slot);
    const QSizeF& s = sizeOf(slot);
    return (r == 90 || r == 270) ? QSizeF(s.height(), s.width()) : s;
}

void PageView::finishMeasure() {
    if (m_measurePoints.isEmpty() || m_measureDone)
        return;
    const int needed = (m_measureKind == MeasureKind::Distance) ? 2 : 3;
    if (m_measurePoints.size() >= needed)
        m_measureDone = true;
    viewport()->update();
}

int PageView::redactionCount() const {
    int n = 0;
    for (const auto& rects : m_redactions)
        n += rects.size();
    return n;
}

int PageView::highlightCount() const {
    int n = 0;
    for (const auto& rects : m_highlights)
        n += rects.size();
    return n;
}

int PageView::noteCount() const {
    int n = 0;
    for (const auto& list : m_notes)
        n += list.size();
    return n;
}

int PageView::inkCount() const {
    int n = 0;
    for (const auto& list : m_inks)
        n += list.size();
    return n;
}

int PageView::shapeCount() const {
    int n = 0;
    for (const auto& list : m_shapes)
        n += list.size();
    return n;
}

void PageView::setAnnotationTool(AnnotTool tool) {
    m_annotTool = tool;
}

void PageView::addNote(int slot, const QPointF& pos, const QString& text) {
    if (slot < 0 || slot >= pageCount() || text.isEmpty())
        return;
    m_notes[slot].append(qMakePair(pos, text));
    emit notesChanged(noteCount());
    viewport()->update();
}

void PageView::addTextBox(int slot, const QRectF& rect, const QString& text) {
    if (slot < 0 || slot >= pageCount() || text.isEmpty())
        return;
    ShapeMark sm{AnnotTool::TextBox, rect, m_highlightColor};
    sm.text = text;
    m_shapes[slot].append(sm);
    emit shapesChanged(shapeCount());
    viewport()->update();
}

void PageView::addRedactions(const QHash<int, QList<QRectF>>& marks) {
    bool any = false;
    for (auto it = marks.constBegin(); it != marks.constEnd(); ++it) {
        if (it.value().isEmpty())
            continue;
        m_redactions[it.key()] += it.value();
        any = true;
    }
    if (!any)
        return;
    emit redactionsChanged(redactionCount());
    viewport()->update();
}

void PageView::clearRedactions() {
    if (m_redactions.isEmpty())
        return;
    m_redactions.clear();
    m_dragging = false;
    m_dragSlot = -1;
    emit redactionsChanged(0);
    viewport()->update();
}

void PageView::clearAnnotations() {
    const bool hadHi = !m_highlights.isEmpty();
    const bool hadNotes = !m_notes.isEmpty();
    const bool hadInk = !m_inks.isEmpty();
    const bool hadShapes = !m_shapes.isEmpty();
    m_highlights.clear();
    m_notes.clear();
    m_inks.clear();
    m_shapes.clear();
    m_currentStroke.clear();
    m_dragging = false;
    m_dragSlot = -1;
    if (hadHi)
        emit highlightsChanged(0);
    if (hadNotes)
        emit notesChanged(0);
    if (hadInk)
        emit inksChanged(0);
    if (hadShapes)
        emit shapesChanged(0);
    viewport()->update();
}

bool PageView::eventFilter(QObject* obj, QEvent* event) {
    if (obj != viewport())
        return QAbstractScrollArea::eventFilter(obj, event);
    if (m_measureMode)
        return handleMeasureEvent(event);
    if (!dragModeActive())
        return QAbstractScrollArea::eventFilter(obj, event);

    const bool inkTool = m_highlightMode && m_annotTool == AnnotTool::Ink;

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(event);
        // Right-click removes the topmost mark under the cursor (any tool).
        if (me->button() == Qt::RightButton) {
            for (int slot = 0; slot < pageCount(); ++slot)
                if (pageRect(slot).contains(me->pos()))
                    return deleteMarkAt(slot, normInSlot(slot, me->pos()));
            break;
        }
        if (me->button() != Qt::LeftButton)
            break;
        for (int slot = 0; slot < pageCount(); ++slot) {
            if (pageRect(slot).contains(me->pos())) {
                m_dragging = true;
                m_dragSlot = slot;
                m_dragStart = normInSlot(slot, me->pos());
                m_dragCur = m_dragStart;
                m_dragNorm = QRectF(m_dragStart, QSizeF(0, 0));
                m_currentStroke.clear();
                if (inkTool)
                    m_currentStroke.append(m_dragStart);
                viewport()->update();
                return true;
            }
        }
        break;
    }
    case QEvent::MouseMove: {
        if (!m_dragging)
            break;
        auto* me = static_cast<QMouseEvent*>(event);
        const QPointF cur = normInSlot(m_dragSlot, me->pos());
        m_dragCur = cur;
        if (inkTool)
            m_currentStroke.append(cur);
        else
            m_dragNorm = QRectF(m_dragStart, cur).normalized();
        viewport()->update();
        return true;
    }
    case QEvent::MouseButtonRelease: {
        if (!m_dragging)
            break;
        m_dragging = false;
        if (inkTool) {
            if (m_currentStroke.size() >= 2) {
                m_inks[m_dragSlot].append(qMakePair(m_currentStroke, m_highlightColor));
                emit inksChanged(inkCount());
            }
            m_currentStroke.clear();
        } else if (m_highlightMode && m_annotTool == AnnotTool::Note) {
            // A note is a single point - drop it where the press landed.
            emit noteRequested(m_dragSlot, m_dragStart);
        } else if (m_fieldMode) {
            // A field placement is one-shot: report the rect, don't store it.
            if (m_dragNorm.width() > 0.004 && m_dragNorm.height() > 0.004)
                emit fieldRectDrawn(m_dragSlot, m_dragNorm);
        } else if (m_snapshotMode) {
            // A snapshot is one-shot too: report the region, don't store it.
            if (m_dragNorm.width() > 0.004 && m_dragNorm.height() > 0.004)
                emit snapshotRegion(m_dragSlot, m_dragNorm);
        } else if (m_highlightMode &&
                   (m_annotTool == AnnotTool::Line || m_annotTool == AnnotTool::Arrow)) {
            // A line is validated by its length, not a bounding rect.
            const QPointF d = m_dragCur - m_dragStart;
            if (d.x() * d.x() + d.y() * d.y() > 0.0001) {
                m_shapes[m_dragSlot].append(
                    ShapeMark{m_annotTool, QRectF(), m_highlightColor, m_dragStart, m_dragCur});
                emit shapesChanged(shapeCount());
            }
        } else if (m_dragNorm.width() > 0.004 && m_dragNorm.height() > 0.004) {
            // Ignore accidental tiny marks (a click without a real drag).
            if (m_redactMode) {
                m_redactions[m_dragSlot].append(m_dragNorm);
                emit redactionsChanged(redactionCount());
            } else if (m_highlightMode && (m_annotTool == AnnotTool::Underline ||
                                           m_annotTool == AnnotTool::StrikeOut ||
                                           m_annotTool == AnnotTool::Rectangle)) {
                m_shapes[m_dragSlot].append(ShapeMark{m_annotTool, m_dragNorm, m_highlightColor});
                emit shapesChanged(shapeCount());
            } else if (m_highlightMode && m_annotTool == AnnotTool::TextBox) {
                emit textBoxRequested(m_dragSlot, m_dragNorm);
            } else if (m_highlightMode) {
                m_highlights[m_dragSlot].append(qMakePair(m_dragNorm, m_highlightColor));
                emit highlightsChanged(highlightCount());
            }
        }
        m_dragSlot = -1;
        viewport()->update();
        return true;
    }
    default:
        break;
    }
    return QAbstractScrollArea::eventFilter(obj, event);
}

bool PageView::deleteMarkAt(int slot, const QPointF& norm) {
    // Notes first (small targets), then ink strokes, then highlights.
    if (auto it = m_notes.find(slot); it != m_notes.end()) {
        for (int i = it->size() - 1; i >= 0; --i) {
            const QPointF d = (*it)[i].first - norm;
            if (std::abs(d.x()) < 0.02 && d.y() > -0.001 && d.y() < 0.03) {
                it->removeAt(i);
                if (it->isEmpty())
                    m_notes.erase(it);
                emit notesChanged(noteCount());
                viewport()->update();
                return true;
            }
        }
    }
    if (auto it = m_inks.find(slot); it != m_inks.end()) {
        for (int i = it->size() - 1; i >= 0; --i) {
            for (const QPointF& p : (*it)[i].first) {
                if (std::abs(p.x() - norm.x()) < 0.012 && std::abs(p.y() - norm.y()) < 0.012) {
                    it->removeAt(i);
                    if (it->isEmpty())
                        m_inks.erase(it);
                    emit inksChanged(inkCount());
                    viewport()->update();
                    return true;
                }
            }
        }
    }
    if (auto it = m_highlights.find(slot); it != m_highlights.end()) {
        for (int i = it->size() - 1; i >= 0; --i) {
            if ((*it)[i].first.contains(norm)) {
                it->removeAt(i);
                if (it->isEmpty())
                    m_highlights.erase(it);
                emit highlightsChanged(highlightCount());
                viewport()->update();
                return true;
            }
        }
    }
    if (auto it = m_shapes.find(slot); it != m_shapes.end()) {
        for (int i = it->size() - 1; i >= 0; --i) {
            // Markup lines are thin, so match a slightly grown rect.
            if ((*it)[i].rect.adjusted(-0.005, -0.005, 0.005, 0.005).contains(norm)) {
                it->removeAt(i);
                if (it->isEmpty())
                    m_shapes.erase(it);
                emit shapesChanged(shapeCount());
                viewport()->update();
                return true;
            }
        }
    }
    return false;
}

bool PageView::handleMeasureEvent(QEvent* event) {
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() != Qt::LeftButton)
            break;
        // A click after a finished measurement starts a fresh one.
        if (m_measureDone) {
            m_measurePoints.clear();
            m_measureSlot = -1;
            m_measureDone = false;
        }
        // Measuring is constrained to a single page: find the one under the cursor.
        int slot = -1;
        for (int s = 0; s < pageCount(); ++s)
            if (pageRect(s).contains(me->pos())) {
                slot = s;
                break;
            }
        if (slot < 0)
            break;
        if (m_measurePoints.isEmpty())
            m_measureSlot = slot;
        else if (slot != m_measureSlot)
            break; // ignore clicks on another page mid-measurement
        const QPointF n = normInSlot(slot, me->pos());
        m_measurePoints.append(n);
        m_measureCursor = n;
        if (m_measureKind == MeasureKind::Distance && m_measurePoints.size() >= 2)
            m_measureDone = true;
        viewport()->update();
        return true;
    }
    case QEvent::MouseMove: {
        if (m_measurePoints.isEmpty() || m_measureDone)
            break;
        auto* me = static_cast<QMouseEvent*>(event);
        m_measureCursor = normInSlot(m_measureSlot, me->pos());
        viewport()->update();
        return true;
    }
    case QEvent::MouseButtonDblClick: {
        // Double-click finishes a polygon (the preceding press added its last vertex).
        if (m_measureKind != MeasureKind::Distance) {
            finishMeasure();
            return true;
        }
        break;
    }
    default:
        break;
    }
    return QAbstractScrollArea::eventFilter(viewport(), event);
}

void PageView::updateCurrentPage() {
    if (m_rows.isEmpty())
        return;
    const double focusY = verticalScrollBar()->value() + viewport()->height() * 0.4;
    const int r = rowAtContentY(focusY);
    const int p = m_rows[r].isEmpty() ? 0 : m_rows[r].first();
    if (p != m_currentPage) {
        m_currentPage = p;
        emit currentPageChanged(p);
    }
}

QSize PageView::pixelSize(int page) const {
    const qreal dpr = devicePixelRatioF();
    return QSize(qRound(pageWidthPx(page) * dpr), qRound(pageHeightPx(page) * dpr));
}

void PageView::request(int slot) {
    if (!m_doc || m_pending.contains(slot) || m_cache.contains(slot))
        return;
    const int orig = originalOf(slot);
    if (orig < 0)
        return;
    m_pending.insert(slot);
    QPdfDocumentRenderOptions opts;
    // PDFium hides annotations unless asked - render them so highlights, notes,
    // and any existing markup are visible in the viewer.
    opts.setRenderFlags(QPdfDocumentRenderOptions::RenderFlag::Annotations);
    switch (rotationOf(slot)) {
    case 90: opts.setRotation(QPdfDocumentRenderOptions::Rotation::Clockwise90); break;
    case 180: opts.setRotation(QPdfDocumentRenderOptions::Rotation::Clockwise180); break;
    case 270: opts.setRotation(QPdfDocumentRenderOptions::Rotation::Clockwise270); break;
    default: break;
    }
    const quint64 id = m_renderer->requestPage(orig, pixelSize(slot), opts);
    m_reqGen.insert(id, m_gen);
    m_reqSlot.insert(id, slot);
}

void PageView::dropStaleCache() {
    while (m_lru.size() > kMaxCache) {
        const int victim = m_lru.takeFirst();
        m_cache.remove(victim);
    }
}

void PageView::invalidateRenders(bool dropCache) {
    // In-flight requests are now from a superseded generation.
    ++m_gen;
    if (dropCache) {
        m_cache.clear();
        m_lru.clear();
    }
    m_pending.clear();
    m_reqGen.clear();
    m_reqSlot.clear();
}

void PageView::setArrangement(const QVector<int>& order, const QVector<int>& rotations) {
    m_order = order;
    m_rot = rotations;
    m_rot.resize(m_order.size());
    m_currentPage = std::clamp(m_currentPage, 0, std::max(0, pageCount() - 1));
    invalidateRenders();
    relayout();
    updateCurrentPage();
    viewport()->update();
}

void PageView::setRotation(int slot, int degrees) {
    if (slot < 0 || slot >= m_rot.size())
        return;
    m_rot[slot] = ((degrees % 360) + 360) % 360;
    // Only this slot's pixmap is wrong; keep the rest so the view doesn't flash.
    m_cache.remove(slot);
    m_lru.removeAll(slot);
    invalidateRenders(/*dropCache=*/false);
    relayout();
    viewport()->update();
}

void PageView::paintEvent(QPaintEvent*) {
    const auto& pal = Theme::instance().palette();
    QPainter p(viewport());
    p.setRenderHint(QPainter::SmoothPixmapTransform, true); // clean scaled previews
    p.fillRect(viewport()->rect(), pal.sunken);

    if (!m_doc || m_rows.isEmpty())
        return;

    const double offY = verticalScrollBar()->value();
    const double offX = horizontalScrollBar()->value();
    const int vpH = viewport()->height();
    const double centerSpace = std::max(contentWidthPx(), double(viewport()->width()));

    auto drawPage = [&](const QRect& r, int slot) {
        const int page = originalOf(slot); // search/highlights are keyed by the original page
        // The page's soft shadow - the only prominent shadow (ui-guidelines §4).
        p.setPen(Qt::NoPen);
        for (int s = 6; s >= 1; --s) {
            p.setBrush(QColor(0, 0, 0, 6));
            p.drawRoundedRect(r.adjusted(-s, -s + 2, s, s + 2), 3, 3);
        }
        p.fillRect(r, QColor(255, 255, 255));
        if (auto it = m_cache.constFind(slot); it != m_cache.constEnd()) {
            p.drawPixmap(r, *it);
            m_lru.removeAll(slot);
            m_lru.append(slot);
        } else {
            request(slot);
        }
        const QList<QPdfLink> results = m_search->resultsOnPage(page);
        if (!results.isEmpty()) {
            QColor hi = pal.accent;
            hi.setAlpha(70);
            p.setPen(Qt::NoPen);
            p.setBrush(hi);
            for (const QPdfLink& link : results)
                for (const QRectF& rp : link.rectangles())
                    p.drawRect(QRectF(r.x() + rp.x() * m_zoom, r.y() + rp.y() * m_zoom,
                                      rp.width() * m_zoom, rp.height() * m_zoom));
        }
        // Edit-mode overlays. Redaction marks are opaque-ish red (what gets
        // removed); highlight marks are translucent yellow (what gets annotated).
        // The live drag is outlined in the active mode's colour.
        const auto drawMark = [&](const QRectF& nrm, const QColor& fill, bool active) {
            const QRectF box(r.x() + nrm.x() * r.width(), r.y() + nrm.y() * r.height(),
                             nrm.width() * r.width(), nrm.height() * r.height());
            p.setBrush(fill);
            p.setPen(active ? QPen(fill.darker(130), 1, Qt::DashLine) : QPen(Qt::NoPen));
            p.drawRect(box);
        };
        const QColor redactFill(200, 30, 30, 120);
        if (auto it = m_redactions.constFind(slot); it != m_redactions.constEnd())
            for (const QRectF& nrm : *it)
                drawMark(nrm, redactFill, false);
        if (auto it = m_highlights.constFind(slot); it != m_highlights.constEnd())
            for (const auto& mark : *it) {
                QColor fill = mark.second;
                fill.setAlpha(90);
                drawMark(mark.first, fill, false);
            }
        // Note markers - a small yellow card at each anchor point.
        if (auto it = m_notes.constFind(slot); it != m_notes.constEnd()) {
            for (const auto& note : *it) {
                const QRectF box(r.x() + note.first.x() * r.width(),
                                 r.y() + note.first.y() * r.height(), 16, 16);
                p.setBrush(QColor(255, 214, 0));
                p.setPen(QPen(QColor(90, 70, 0), 1));
                p.drawRoundedRect(box, 3, 3);
            }
        }
        // Ink strokes - freehand polylines in their colour.
        const auto drawStroke = [&](const QPolygonF& stroke, const QColor& c) {
            if (stroke.size() < 2)
                return;
            QPolygonF pts;
            pts.reserve(stroke.size());
            for (const QPointF& n : stroke)
                pts.append(QPointF(r.x() + n.x() * r.width(), r.y() + n.y() * r.height()));
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(c, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPolyline(pts);
        };
        if (auto it = m_inks.constFind(slot); it != m_inks.constEnd())
            for (const auto& ink : *it)
                drawStroke(ink.first, ink.second);
        // Vector markup/shapes: markup lines, boxes, lines/arrows, and text boxes.
        const auto toPt = [&](const QPointF& n) {
            return QPointF(r.x() + n.x() * r.width(), r.y() + n.y() * r.height());
        };
        const auto drawArrowHead = [&](const QPointF& from, const QPointF& tip, const QColor& c) {
            QLineF line(tip, from);
            const double len = 10.0;
            const double ang = std::atan2(line.dy(), line.dx());
            const double spread = 0.45;
            QPointF h1 = tip + QPointF(std::cos(ang + spread) * len, std::sin(ang + spread) * len);
            QPointF h2 = tip + QPointF(std::cos(ang - spread) * len, std::sin(ang - spread) * len);
            QPolygonF head{tip, h1, h2};
            p.setBrush(c);
            p.setPen(QPen(c, 1.0));
            p.drawPolygon(head);
        };
        if (auto it = m_shapes.constFind(slot); it != m_shapes.constEnd())
            for (const ShapeMark& sm : *it) {
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(sm.color, 1.6));
                if (sm.kind == AnnotTool::Line || sm.kind == AnnotTool::Arrow) {
                    const QPointF a = toPt(sm.a), b = toPt(sm.b);
                    p.drawLine(a, b);
                    if (sm.kind == AnnotTool::Arrow)
                        drawArrowHead(a, b, sm.color);
                    continue;
                }
                const QRectF box(r.x() + sm.rect.x() * r.width(), r.y() + sm.rect.y() * r.height(),
                                 sm.rect.width() * r.width(), sm.rect.height() * r.height());
                if (sm.kind == AnnotTool::Underline)
                    p.drawLine(QPointF(box.left(), box.bottom()), box.bottomRight());
                else if (sm.kind == AnnotTool::StrikeOut)
                    p.drawLine(QPointF(box.left(), box.center().y()),
                               QPointF(box.right(), box.center().y()));
                else if (sm.kind == AnnotTool::TextBox) {
                    p.drawRect(box);
                    p.setPen(sm.color);
                    p.drawText(box.adjusted(3, 2, -3, -2), Qt::AlignTop | Qt::TextWordWrap, sm.text);
                } else // Rectangle
                    p.drawRect(box);
            }
        // Live preview for the line/arrow tools (the rect preview covers the rest).
        if (m_dragging && slot == m_dragSlot && m_highlightMode &&
            (m_annotTool == AnnotTool::Line || m_annotTool == AnnotTool::Arrow)) {
            QColor c = m_highlightColor;
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(c, 1.6, Qt::DashLine));
            p.drawLine(toPt(m_dragStart), toPt(m_dragCur));
        }
        if (m_dragging && slot == m_dragSlot && !m_currentStroke.isEmpty())
            drawStroke(m_currentStroke, m_highlightColor);
        const bool dragRect =
            m_dragging && slot == m_dragSlot && m_dragNorm.isValid() &&
            !(m_highlightMode &&
              (m_annotTool == AnnotTool::Note || m_annotTool == AnnotTool::Ink ||
               m_annotTool == AnnotTool::Line || m_annotTool == AnnotTool::Arrow));
        if (dragRect) {
            QColor dragFill = (m_fieldMode || m_snapshotMode) ? QColor(17, 124, 111) // feather teal
                              : m_highlightMode               ? m_highlightColor
                                                              : QColor(200, 30, 30);
            dragFill.setAlpha(110);
            drawMark(m_dragNorm, dragFill, true);
        }

        // Measure overlay: live segments, vertex dots, and the running value, on
        // the page that owns the current measurement.
        if (m_measureMode && slot == m_measureSlot && !m_measurePoints.isEmpty()) {
            const QColor accent = pal.accent;
            const auto toVp = [&](const QPointF& n) {
                return QPointF(r.x() + n.x() * r.width(), r.y() + n.y() * r.height());
            };
            // The committed vertices, and the set used for live values/preview.
            QVector<QPointF> vp;
            vp.reserve(m_measurePoints.size());
            for (const QPointF& n : m_measurePoints)
                vp.append(toVp(n));
            QVector<QPointF> live = m_measurePoints;
            if (!m_measureDone)
                live.append(m_measureCursor);

            // Translucent fill for an in-progress / finished area polygon.
            if (m_measureKind == MeasureKind::Area && live.size() >= 3) {
                QPolygonF poly;
                for (const QPointF& n : live)
                    poly << toVp(n);
                QColor fill = accent;
                fill.setAlpha(40);
                p.setPen(Qt::NoPen);
                p.setBrush(fill);
                p.drawPolygon(poly);
            }

            // Committed segments (solid).
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(accent, 1.8));
            for (int i = 1; i < vp.size(); ++i)
                p.drawLine(vp[i - 1], vp[i]);
            if (m_measureKind == MeasureKind::Area && m_measureDone && vp.size() >= 3)
                p.drawLine(vp.last(), vp.first());

            // Live rubber segment(s) to the cursor while still placing points.
            if (!m_measureDone) {
                const QPointF cur = toVp(m_measureCursor);
                p.setPen(QPen(accent, 1.4, Qt::DashLine));
                p.drawLine(vp.last(), cur);
                if (m_measureKind == MeasureKind::Area && vp.size() >= 2)
                    p.drawLine(cur, vp.first()); // preview the closing edge
            }

            // Vertex dots.
            p.setPen(Qt::NoPen);
            p.setBrush(accent);
            for (const QPointF& q : vp)
                p.drawEllipse(q, 3.0, 3.0);

            // The running value, in the chosen unit.
            const QSizeF pagePts = displayedPointSize(slot);
            const QVector<QPointF>& valSet = m_measureDone ? m_measurePoints : live;
            QString label;
            if (m_measureKind == MeasureKind::Distance)
                label = Measurer::formatLength(Measurer::distance(valSet, pagePts, m_measureUnit),
                                               m_measureUnit);
            else if (m_measureKind == MeasureKind::Perimeter)
                label = Measurer::formatLength(
                    Measurer::perimeter(valSet, pagePts, m_measureUnit, m_measureDone),
                    m_measureUnit);
            else
                label = Measurer::formatArea(Measurer::area(valSet, pagePts, m_measureUnit),
                                             m_measureUnit);

            const QPointF anchor = m_measureDone ? vp.last() : toVp(m_measureCursor);
            QFont f = p.font();
            f.setPointSizeF(9);
            p.setFont(f);
            const QFontMetrics fm(f);
            QRectF chip(anchor.x() + 10, anchor.y() - fm.height() - 8,
                        fm.horizontalAdvance(label) + 14, fm.height() + 6);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 180));
            p.drawRoundedRect(chip, 4, 4);
            p.setPen(QColor(255, 255, 255));
            p.drawText(chip, Qt::AlignCenter, label);
        }

        p.setBrush(Qt::NoBrush);
        p.setPen(pal.hairline);
        p.drawRect(r);
    };

    int first = rowAtContentY(offY);
    while (first > 0 && m_rowTop[first] > offY)
        --first;

    for (int r = first; r < m_rows.size(); ++r) {
        const double rowYtop = m_rowTop[r] - offY;
        if (rowYtop > vpH)
            break;
        if (rowYtop + rowHeightPx(r) < 0)
            continue;
        double x = (centerSpace - rowContentWidthPx(r)) / 2.0 - offX;
        for (int page : m_rows[r]) {
            const QRect rect(qRound(x), qRound(rowYtop), qRound(pageWidthPx(page)),
                             qRound(pageHeightPx(page)));
            drawPage(rect, page);
            x += pageWidthPx(page) + kPairGap;
        }
    }

    // The active match, emphasised. link.page() is an original page; find the
    // display slot that shows it.
    if (m_currentResult >= 0) {
        const QPdfLink link = m_search->resultAtIndex(m_currentResult);
        const int slot = m_order.indexOf(link.page());
        const QRect r = slot >= 0 ? pageRect(slot) : QRect();
        if (!r.isNull()) {
            QColor hi = pal.accent;
            hi.setAlpha(150);
            p.setBrush(hi);
            p.setPen(QPen(pal.accent, 1));
            for (const QRectF& rp : link.rectangles())
                p.drawRect(QRectF(r.x() + rp.x() * m_zoom, r.y() + rp.y() * m_zoom,
                                  rp.width() * m_zoom, rp.height() * m_zoom));
        }
    }

    if (m_hud) {
        const QString text =
            QStringLiteral("page %1/%2  zoom %3%  dpr %4  cache %5  pending %6  rendered %7")
                .arg(m_currentPage + 1)
                .arg(pageCount())
                .arg(qRound(m_zoom * 100))
                .arg(devicePixelRatioF(), 0, 'f', 2)
                .arg(m_cache.size())
                .arg(m_pending.size())
                .arg(m_renderedCount);
        QFont f("monospace", 9);
        p.setFont(f);
        const QRect box(8, 8, QFontMetrics(f).horizontalAdvance(text) + 16, 22);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 160));
        p.drawRoundedRect(box, 5, 5);
        p.setPen(QColor(255, 255, 255));
        p.drawText(box, Qt::AlignCenter, text);
    }
}

void PageView::resizeEvent(QResizeEvent*) {
    relayout();
    updateCurrentPage();
}

void PageView::scrollContentsBy(int, int) {
    updateCurrentPage();
    viewport()->update();
}

void PageView::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        const int dy = event->angleDelta().y();
        if (dy != 0)
            setZoom(m_zoom * (dy > 0 ? kZoomStep : 1.0 / kZoomStep));
        event->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

void PageView::keyPressEvent(QKeyEvent* event) {
    // While measuring, Esc finishes the current polygon (or abandons a partial one).
    if (m_measureMode && event->key() == Qt::Key_Escape) {
        if (m_measureDone || m_measurePoints.isEmpty()) {
            clearMeasure();
        } else {
            finishMeasure();
            if (!m_measureDone)
                clearMeasure();
        }
        return;
    }

    if (event->key() == Qt::Key_D && (event->modifiers() & Qt::ControlModifier) &&
        (event->modifiers() & Qt::ShiftModifier)) {
        setHudVisible(!m_hud);
        return;
    }

    QScrollBar* v = verticalScrollBar();
    switch (event->key()) {
    case Qt::Key_PageDown:
        v->triggerAction(QAbstractSlider::SliderPageStepAdd);
        return;
    case Qt::Key_PageUp:
        v->triggerAction(QAbstractSlider::SliderPageStepSub);
        return;
    case Qt::Key_Space:
        v->triggerAction((event->modifiers() & Qt::ShiftModifier)
                             ? QAbstractSlider::SliderPageStepSub
                             : QAbstractSlider::SliderPageStepAdd);
        return;
    case Qt::Key_Down:
        v->triggerAction(QAbstractSlider::SliderSingleStepAdd);
        return;
    case Qt::Key_Up:
        v->triggerAction(QAbstractSlider::SliderSingleStepSub);
        return;
    case Qt::Key_Home:
        if (event->modifiers() & Qt::ControlModifier) {
            v->setValue(v->minimum());
            return;
        }
        goToPage(0);
        return;
    case Qt::Key_End:
        if (event->modifiers() & Qt::ControlModifier) {
            v->setValue(v->maximum());
            return;
        }
        goToPage(pageCount() - 1);
        return;
    default:
        break;
    }
    QAbstractScrollArea::keyPressEvent(event);
}

void PageView::goToPage(int page) {
    if (pageCount() == 0)
        return;
    page = std::clamp(page, 0, pageCount() - 1);
    if (m_mode == LayoutMode::SinglePage) {
        m_currentPage = page;
        relayout();
        verticalScrollBar()->setValue(0);
        emit currentPageChanged(page);
        viewport()->update();
        return;
    }
    const int r = m_pageRow[page];
    verticalScrollBar()->setValue(qRound(m_rowTop[r] - kMargin));
}

void PageView::setLayoutMode(LayoutMode mode) {
    if (mode == m_mode)
        return;
    const int keep = m_currentPage;
    m_mode = mode;
    relayout();
    goToPage(keep);
    emit layoutModeChanged(mode);
    viewport()->update();
}

void PageView::setZoom(double zoom) {
    zoom = std::clamp(zoom, kMinZoom, kMaxZoom);
    if (qFuzzyCompare(zoom, m_zoom))
        return;

    // Preserve the reading position: keep whatever row sits at the viewport top.
    const double topY = verticalScrollBar()->value();
    const int r = rowAtContentY(topY);
    const double frac = rowHeightPx(r) > 0 ? (topY - m_rowTop[r]) / rowHeightPx(r) : 0.0;

    m_zoom = zoom;
    // Keep the old pixmaps - drawn scaled as a preview - so zooming never flashes
    // white; the crisp render at the new size replaces them as it arrives.
    invalidateRenders(/*dropCache=*/false);
    relayout();

    if (r < m_rowTop.size())
        verticalScrollBar()->setValue(qRound(m_rowTop[r] + frac * rowHeightPx(r)));

    emit zoomChanged(m_zoom);
    updateCurrentPage();
    viewport()->update();
}

void PageView::zoomIn() {
    setZoom(m_zoom * kZoomStep);
}

void PageView::zoomOut() {
    setZoom(m_zoom / kZoomStep);
}

void PageView::zoomActualSize() {
    setZoom(1.0);
}

void PageView::fitToWidth() {
    if (pageCount() == 0)
        return;
    setZoom((viewport()->width() - 2.0 * kMargin) / m_maxRowWPoints);
}

void PageView::fitWholePage() {
    if (pageCount() == 0)
        return;
    const QSizeF s = sizeOf(m_currentPage);
    const double zW = (viewport()->width() - 2.0 * kMargin) / s.width();
    const double zH = (viewport()->height() - 2.0 * kMargin) / s.height();
    setZoom(std::min(zW, zH));
}

int PageView::search(const QString& query) {
    m_currentResult = -1;
    m_search->setSearchString(query);
    return searchResultCount();
}

void PageView::clearSearch() {
    m_currentResult = -1;
    m_search->setSearchString(QString());
    viewport()->update();
}

void PageView::showSearchResult(int index) {
    const int count = searchResultCount();
    if (count <= 0)
        return;
    m_currentResult = ((index % count) + count) % count;
    const QPdfLink link = m_search->resultAtIndex(m_currentResult);
    const int slot = m_order.indexOf(link.page()); // original page → display slot
    if (slot >= 0) {
        if (m_mode == LayoutMode::SinglePage) {
            m_currentPage = slot;
            relayout();
            emit currentPageChanged(m_currentPage);
        }
        const int r = (slot < m_pageRow.size()) ? m_pageRow[slot] : 0;
        const double y = m_rowTop[std::max(0, r)] + link.location().y() * m_zoom - 40;
        verticalScrollBar()->setValue(qRound(std::max(0.0, y)));
    }
    viewport()->update();
}

int PageView::searchResultCount() const {
    return m_search->rowCount(QModelIndex());
}

void PageView::setHudVisible(bool on) {
    m_hud = on;
    viewport()->update();
}
