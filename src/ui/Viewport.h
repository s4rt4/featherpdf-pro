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

#include <QWidget>

#include "ui/PageView.h"

class FeatherDocument;

// The document viewport: where the page floats on the canvas.
//
// A thin wrapper over PageView - the custom continuous render pipeline (off-UI-
// thread render, LRU cache, arithmetic layout). Keeping that behind this stable
// interface means MainWindow never sees the renderer change.
class Viewport : public QWidget {
    Q_OBJECT

public:
    explicit Viewport(QWidget* parent = nullptr);
    ~Viewport() override;

    void setDocument(FeatherDocument* doc);
    void clear();
    bool hasDocument() const;

    // Zoom - every path (buttons, Ctrl+wheel, fit) funnels through here so the
    // reading position is preserved consistently, per the saran notes.
    void zoomIn();
    void zoomOut();
    void zoomActualSize(); // 100%
    void fitToWidth();
    void fitWholePage();
    void setZoomFactor(double factor); // for presets
    double zoomFactor() const;

    void setLayoutMode(PageView::LayoutMode mode);
    PageView::LayoutMode layoutMode() const;

    int currentPage() const;
    int pageCount() const;
    void goToPage(int page); // 0-based

    // Text search. The query highlights all matches on every page; the result
    // count updates live as the background search progresses (searchResults
    // Changed). showSearchResult cycles the highlighted/active match.
    int search(const QString& query); // sets the query, returns current count
    void clearSearch();
    void showSearchResult(int index); // wraps; navigates to and highlights it
    int searchResultCount() const;

    // Redaction (forwarded to PageView). Marks are slot → normalized rects.
    void setRedactionMode(bool on);
    bool redactionMode() const;
    QHash<int, QList<QRectF>> redactionMarks() const;
    void addRedactions(const QHash<int, QList<QRectF>>& marks);
    int redactionCount() const;
    void clearRedactions();

    // Form-field placement (forwarded to PageView): drag a rectangle to place a
    // new field; the drawn rect arrives via fieldRectDrawn.
    void setFieldPlacementMode(bool on);
    bool fieldPlacementMode() const;

    // Snapshot region select (forwarded to PageView): drag one rectangle; the drawn
    // region arrives via snapshotRegion, then the mode ends.
    void setSnapshotMode(bool on);
    bool snapshotMode() const;

    // Measure tools (forwarded to PageView): click points on a page; a live
    // overlay reports the distance / perimeter / area in the chosen unit.
    void setMeasureMode(bool on);
    bool measureMode() const;
    void setMeasureKind(PageView::MeasureKind kind);
    void setMeasureUnit(Measurer::Unit unit);
    void clearMeasure();

    // Annotation authoring (forwarded to PageView).
    void setHighlightMode(bool on);
    bool highlightMode() const;
    void setAnnotationTool(PageView::AnnotTool tool);
    void setHighlightColor(const QColor& color);
    QHash<int, QList<QPair<QRectF, QColor>>> highlightMarks() const;
    QHash<int, QList<QPair<QPointF, QString>>> noteMarks() const;
    QHash<int, QList<QPair<QPolygonF, QColor>>> inkMarks() const;
    QHash<int, QList<PageView::ShapeMark>> shapeMarks() const;
    int highlightCount() const;
    int noteCount() const;
    int inkCount() const;
    int shapeCount() const;
    void addNote(int slot, const QPointF& pos, const QString& text);
    void addTextBox(int slot, const QRectF& rect, const QString& text);
    void clearAnnotations();

signals:
    void zoomChanged(double factor);
    void currentPageChanged(int page); // 0-based
    void pageCountChanged(int count);
    void searchResultsChanged(int count);
    void layoutModeChanged(PageView::LayoutMode mode);
    void redactionsChanged(int count);
    void highlightsChanged(int count);
    void notesChanged(int count);
    void inksChanged(int count);
    void shapesChanged(int count);
    void noteRequested(int slot, QPointF normPos);
    void textBoxRequested(int slot, QRectF normRect);
    void fieldRectDrawn(int slot, QRectF normRect); // form field placement finished
    void snapshotRegion(int slot, QRectF normRect); // snapshot region selected

private:
    FeatherDocument* m_doc = nullptr;
    PageView* m_view;
};
