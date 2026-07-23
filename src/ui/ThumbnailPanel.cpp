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

#include "ui/ThumbnailPanel.h"

#include "ui/Theme.h"

#include <QAbstractListModel>
#include <QApplication>
#include <QDropEvent>
#include <QHash>
#include <QListView>
#include <QPainter>
#include <QPainterPath>
#include <QPdfDocument>
#include <QPdfDocumentRenderOptions>
#include <QPdfPageRenderer>
#include <QSet>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <QVector>

namespace {
constexpr int kThumbWidth = 140; // logical px
constexpr int kPad = 12;         // around each thumbnail
constexpr int kNumberArea = 18;  // page-number strip below the thumbnail
} // namespace

// ── Model ────────────────────────────────────────────────────────────────────
// One row per page. The decoration is rendered lazily and off-thread; data()
// requests a render only for rows the view actually asks about (i.e. visible).
class ThumbnailModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit ThumbnailModel(QObject* parent) : QAbstractListModel(parent) {
        m_renderer = new QPdfPageRenderer(this);
        m_renderer->setRenderMode(QPdfPageRenderer::RenderMode::MultiThreaded);
        connect(m_renderer, &QPdfPageRenderer::pageRendered, this, &ThumbnailModel::onRendered);
    }

    void setDocument(QPdfDocument* doc) {
        beginResetModel();
        m_doc = doc;
        m_renderer->setDocument(doc);
        m_cache.clear();
        m_pending.clear();
        m_reqRow.clear();
        endResetModel();
    }

    // The arrangement from the façade: order[i] = original page in display slot i.
    void setArrangement(const QVector<int>& order, const QVector<int>& rotations) {
        beginResetModel();
        m_order = order;
        m_rot = rotations;
        m_rot.resize(m_order.size());
        m_cache.clear();
        m_pending.clear();
        m_reqRow.clear();
        endResetModel();
    }

    void setRotation(int slot, int degrees) {
        if (slot < 0 || slot >= m_rot.size())
            return;
        m_rot[slot] = degrees;
        m_cache.remove(slot);
        m_pending.remove(slot);
        const QModelIndex idx = index(slot);
        emit dataChanged(idx, idx, {Qt::DecorationRole});
    }

    int rowCount(const QModelIndex& parent = {}) const override {
        if (parent.isValid() || !m_doc)
            return 0;
        return m_order.size();
    }

    // Logical size of a thumbnail for display slot `row`, with rotation applied.
    QSize thumbSize(int row) const {
        if (!m_doc || row < 0 || row >= m_order.size())
            return {kThumbWidth, kThumbWidth};
        QSizeF pt = m_doc->pagePointSize(m_order[row]);
        const int rot = (row < m_rot.size()) ? m_rot[row] : 0;
        if (rot == 90 || rot == 270)
            pt.transpose();
        if (pt.width() <= 0 || pt.height() <= 0)
            return {kThumbWidth, kThumbWidth};
        const int h = qRound(kThumbWidth * pt.height() / pt.width());
        return {kThumbWidth, qMax(20, h)};
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || !m_doc)
            return {};
        const int row = index.row();
        if (role == Qt::DecorationRole) {
            if (auto it = m_cache.constFind(row); it != m_cache.constEnd())
                return *it;
            const_cast<ThumbnailModel*>(this)->request(row); // lazy: only visible rows reach here
            return {};
        }
        return {};
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override {
        Qt::ItemFlags f = QAbstractListModel::flags(index) | Qt::ItemIsDropEnabled;
        if (index.isValid())
            f |= Qt::ItemIsDragEnabled;
        return f;
    }

private slots:
    void onRendered(int, QSize, const QImage& image, QPdfDocumentRenderOptions, quint64 id) {
        const int row = m_reqRow.take(id);
        if (image.isNull() || row < 0 || row >= m_order.size())
            return;
        // QtPdf renders a transparent page background; flatten onto white so the
        // thumbnail looks like paper instead of showing the panel through it.
        QImage flat(image.size(), QImage::Format_RGB32);
        flat.fill(Qt::white);
        QPainter pp(&flat);
        pp.drawImage(0, 0, image);
        pp.end();
        QPixmap pm = QPixmap::fromImage(flat);
        pm.setDevicePixelRatio(dpr());
        m_cache.insert(row, pm);
        m_pending.remove(row);
        const QModelIndex idx = index(row);
        emit dataChanged(idx, idx, {Qt::DecorationRole});
    }

private:
    static qreal dpr() { return qApp ? qApp->devicePixelRatio() : 1.0; }

    void request(int row) {
        if (m_pending.contains(row) || m_cache.contains(row) || !m_doc)
            return;
        if (row < 0 || row >= m_order.size())
            return;
        m_pending.insert(row);
        QPdfDocumentRenderOptions opts;
        switch ((row < m_rot.size()) ? m_rot[row] : 0) {
        case 90: opts.setRotation(QPdfDocumentRenderOptions::Rotation::Clockwise90); break;
        case 180: opts.setRotation(QPdfDocumentRenderOptions::Rotation::Clockwise180); break;
        case 270: opts.setRotation(QPdfDocumentRenderOptions::Rotation::Clockwise270); break;
        default: break;
        }
        const quint64 reqId = m_renderer->requestPage(m_order[row], thumbSize(row) * dpr(), opts);
        m_reqRow.insert(reqId, row);
    }

    QPdfDocument* m_doc = nullptr;
    QPdfPageRenderer* m_renderer = nullptr;
    QVector<int> m_order;
    QVector<int> m_rot;
    QHash<int, QPixmap> m_cache;
    QSet<int> m_pending;
    QHash<quint64, int> m_reqRow;
};

// ── Delegate ─────────────────────────────────────────────────────────────────
// Paints the thumbnail centered with a hairline border (2px accent when it is
// the current page) and the page number beneath it.
class ThumbnailDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void setCurrentPage(int page) { m_current = page; }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex& index) const override {
        const auto* model = qobject_cast<const ThumbnailModel*>(index.model());
        const QSize ts = model ? model->thumbSize(index.row()) : QSize(kThumbWidth, kThumbWidth);
        return {kThumbWidth + 2 * kPad, ts.height() + kNumberArea + 2 * kPad};
    }

    void paint(QPainter* p, const QStyleOptionViewItem& opt,
               const QModelIndex& index) const override {
        const auto& pal = Theme::instance().palette();
        const auto* model = qobject_cast<const ThumbnailModel*>(index.model());
        const QSize ts = model ? model->thumbSize(index.row()) : QSize(kThumbWidth, kThumbWidth);
        const bool current = index.row() == m_current;

        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);

        const QRect cell = opt.rect;
        const QRect thumb(cell.left() + (cell.width() - kThumbWidth) / 2, cell.top() + kPad,
                          kThumbWidth, ts.height());

        // The thumbnail (or a sunken placeholder until it renders).
        const QVariant deco = index.data(Qt::DecorationRole);
        if (deco.canConvert<QPixmap>() && !deco.value<QPixmap>().isNull()) {
            p->drawPixmap(thumb, deco.value<QPixmap>());
        } else {
            p->fillRect(thumb, pal.sunken);
        }

        // Border - 2px accent for the current page, hairline otherwise.
        QPen pen(current ? pal.accent : pal.hairline, current ? 2 : 1);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        p->drawRect(thumb.adjusted(0, 0, -1, -1));

        // Page number.
        p->setPen(current ? pal.accent : pal.dim);
        QFont f = opt.font;
        f.setPointSizeF(f.pointSizeF() - 0.5);
        if (current)
            f.setWeight(QFont::DemiBold);
        p->setFont(f);
        const QRect numRect(cell.left(), thumb.bottom() + 2, cell.width(), kNumberArea);
        p->drawText(numRect, Qt::AlignHCenter | Qt::AlignVCenter,
                    QString::number(index.row() + 1));

        p->restore();
    }

private:
    int m_current = -1;
};

// A list view that lets pages be dragged to reorder. The move is not performed
// in the model - it is announced (moveRequested) so the façade can do it as an
// undoable command and propagate the new arrangement back.
class ThumbnailListView : public QListView {
    Q_OBJECT

public:
    using QListView::QListView;

signals:
    void moveRequested(int from, int to);

protected:
    void startDrag(Qt::DropActions actions) override {
        m_dragRow = currentIndex().row();
        QListView::startDrag(actions);
    }
    void dropEvent(QDropEvent* event) override {
        int to = indexAt(event->position().toPoint()).row();
        if (to < 0)
            to = model()->rowCount() - 1; // dropped past the last row → move to end
        const int from = m_dragRow;
        m_dragRow = -1;
        event->acceptProposedAction();
        if (from >= 0 && to >= 0 && from != to)
            emit moveRequested(from, to);
    }

private:
    int m_dragRow = -1;
};

// ── Panel ────────────────────────────────────────────────────────────────────
ThumbnailPanel::ThumbnailPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("ThumbnailPanel");

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    auto* view = new ThumbnailListView(this);
    m_view = view;
    m_model = new ThumbnailModel(this);
    m_delegate = new ThumbnailDelegate(this);
    m_view->setModel(m_model);
    m_view->setItemDelegate(m_delegate);
    m_view->setObjectName("ThumbnailList");
    m_view->setUniformItemSizes(false);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection); // needed for drag
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setMouseTracking(true);
    // Drag a thumbnail to reorder the pages.
    m_view->setDragDropMode(QAbstractItemView::InternalMove);
    m_view->setDragEnabled(true);
    m_view->setAcceptDrops(true);
    m_view->setDropIndicatorShown(true);
    m_view->setDefaultDropAction(Qt::MoveAction);
    col->addWidget(m_view);

    connect(m_view, &QListView::clicked, this,
            [this](const QModelIndex& i) { emit pageActivated(i.row()); });
    connect(view, &ThumbnailListView::moveRequested, this, &ThumbnailPanel::pageMoved);
    connect(&Theme::instance(), &Theme::changed, this, [this] { m_view->viewport()->update(); });
}

void ThumbnailPanel::setDocument(QPdfDocument* doc) {
    m_model->setDocument(doc);
    m_delegate->setCurrentPage(0);
    m_view->scrollToTop();
}

void ThumbnailPanel::setArrangement(const QVector<int>& order, const QVector<int>& rotations) {
    m_model->setArrangement(order, rotations);
}

void ThumbnailPanel::setRotation(int slot, int degrees) {
    m_model->setRotation(slot, degrees);
}

void ThumbnailPanel::clear() {
    m_model->setDocument(nullptr);
}

void ThumbnailPanel::setCurrentPage(int page) {
    m_delegate->setCurrentPage(page);
    const QModelIndex idx = m_model->index(page);
    if (idx.isValid())
        m_view->scrollTo(idx, QAbstractItemView::EnsureVisible);
    m_view->viewport()->update();
}

#include "ThumbnailPanel.moc"
