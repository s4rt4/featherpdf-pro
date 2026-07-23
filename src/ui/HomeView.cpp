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

#include "ui/HomeView.h"

#include "ui/Theme.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>
#include <algorithm>

namespace {
constexpr auto kRecentFilesKey = "recentFiles";
constexpr int kMaxCards = 20;

// First local PDF in a drag payload, or empty.
QString pdfFromMime(const QMimeData* mime) {
    if (!mime || !mime->hasUrls())
        return {};
    for (const QUrl& url : mime->urls()) {
        if (!url.isLocalFile())
            continue;
        const QString path = url.toLocalFile();
        if (path.endsWith(".pdf", Qt::CaseInsensitive))
            return path;
    }
    return {};
}
} // namespace

// A single recent-file card: icon chip + name + dim path, clickable.
class RecentCard : public QWidget {
    Q_OBJECT

public:
    RecentCard(const QString& path, QWidget* parent) : QWidget(parent), m_path(path) {
        setObjectName("RecentCard");
        setAttribute(Qt::WA_StyledBackground, true);
        setCursor(Qt::PointingHandCursor);
        setMinimumWidth(160);

        auto* row = new QHBoxLayout(this);
        row->setContentsMargins(12, 11, 14, 11);
        row->setSpacing(12);

        m_chip = new QLabel(this);
        m_chip->setObjectName("CardChip");
        m_chip->setFixedSize(36, 36);
        m_chip->setAlignment(Qt::AlignCenter);
        row->addWidget(m_chip);

        const QFileInfo fi(path);
        m_fullName = fi.fileName();
        m_fullDir = fi.absolutePath();
        auto* texts = new QVBoxLayout;
        texts->setContentsMargins(0, 0, 0, 0);
        texts->setSpacing(2);
        m_name = new QLabel(m_fullName, this);
        m_name->setObjectName("CardName");
        m_dir = new QLabel(m_fullDir, this);
        m_dir->setObjectName("CardDir");
        m_dir->setToolTip(path);
        // Don't let the (possibly very long) path force the card wide - the labels
        // take whatever width the layout gives and elide to fit.
        for (QLabel* l : {m_name, m_dir})
            l->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        texts->addWidget(m_name);
        texts->addWidget(m_dir);
        row->addLayout(texts, 1);

        refresh();
    }

    void refresh() {
        const auto& pal = Theme::instance().palette();
        m_chip->setStyleSheet(
            QStringLiteral("background:%1; border-radius:9px;").arg(pal.accentTint.name()));
        m_chip->setPixmap(Theme::instance().icon("file", pal.accent).pixmap(18, 18));
        m_name->setStyleSheet(
            QStringLiteral("color:%1; font-size:14px; font-weight:600;").arg(pal.text.name()));
        m_dir->setStyleSheet(QStringLiteral("color:%1; font-size:12px;").arg(pal.dim.name()));
    }

signals:
    void activated(const QString& path);

protected:
    void mouseReleaseEvent(QMouseEvent*) override { emit activated(m_path); }
    void resizeEvent(QResizeEvent*) override { elide(); }

private:
    void elide() {
        m_name->setText(m_name->fontMetrics().elidedText(m_fullName, Qt::ElideRight,
                                                         m_name->width()));
        m_dir->setText(
            m_dir->fontMetrics().elidedText(m_fullDir, Qt::ElideMiddle, m_dir->width()));
    }

    QString m_path;
    QString m_fullName;
    QString m_fullDir;
    QLabel* m_chip = nullptr;
    QLabel* m_name = nullptr;
    QLabel* m_dir = nullptr;
};

// A square tile for the card/grid view: a big icon over the file name (no path).
class RecentTile : public QWidget {
    Q_OBJECT

public:
    RecentTile(const QString& path, QWidget* parent) : QWidget(parent), m_path(path) {
        setObjectName("RecentTile");
        setAttribute(Qt::WA_StyledBackground, true);
        setCursor(Qt::PointingHandCursor);
        setFixedSize(150, 142);
        setToolTip(path);

        auto* col = new QVBoxLayout(this);
        col->setContentsMargins(10, 16, 10, 12);
        col->setSpacing(12);

        m_chip = new QLabel(this);
        m_chip->setObjectName("TileChip");
        m_chip->setFixedSize(54, 54);
        m_chip->setAlignment(Qt::AlignCenter);
        col->addWidget(m_chip, 0, Qt::AlignHCenter);

        m_fullName = QFileInfo(path).fileName();
        m_name = new QLabel(m_fullName, this);
        m_name->setObjectName("TileName");
        m_name->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        m_name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        col->addWidget(m_name, 1);

        refresh();
    }

    void refresh() {
        const auto& pal = Theme::instance().palette();
        m_chip->setStyleSheet(
            QStringLiteral("background:%1; border-radius:12px;").arg(pal.accentTint.name()));
        m_chip->setPixmap(Theme::instance().icon("file", pal.accent).pixmap(28, 28));
        m_name->setStyleSheet(
            QStringLiteral("color:%1; font-size:13px; font-weight:600;").arg(pal.text.name()));
    }

signals:
    void activated(const QString& path);

protected:
    void mouseReleaseEvent(QMouseEvent*) override { emit activated(m_path); }
    void resizeEvent(QResizeEvent*) override {
        m_name->setText(m_name->fontMetrics().elidedText(m_fullName, Qt::ElideMiddle,
                                                         m_name->width()));
    }

private:
    QString m_path;
    QString m_fullName;
    QLabel* m_chip = nullptr;
    QLabel* m_name = nullptr;
};

HomeView::HomeView(QWidget* parent) : QWidget(parent) {
    setObjectName("HomeView");
    setAcceptDrops(true);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Sidebar ──────────────────────────────────────────────────────────────
    auto* sidebar = new QWidget(this);
    sidebar->setObjectName("HomeSidebar");
    sidebar->setFixedWidth(244);
    auto* side = new QVBoxLayout(sidebar);
    side->setContentsMargins(18, 22, 18, 18);
    side->setSpacing(0);

    auto* brand = new QHBoxLayout;
    brand->setSpacing(10);
    m_logo = new QLabel(sidebar);
    brand->addWidget(m_logo);
    m_brandName = new QLabel(tr("Feather PDF"), sidebar);
    m_brandName->setObjectName("HomeBrandName");
    brand->addWidget(m_brandName, 1);
    side->addLayout(brand);
    side->addSpacing(26);

    m_openBtn = new QPushButton(tr("Open"), sidebar);
    m_openBtn->setObjectName("HomeOpen");
    m_openBtn->setCursor(Qt::PointingHandCursor);
    m_openBtn->setIconSize(QSize(16, 16));
    connect(m_openBtn, &QPushButton::clicked, this, &HomeView::openRequested);
    side->addWidget(m_openBtn);
    side->addSpacing(10);

    m_recentNav = new QPushButton(tr("Recent files"), sidebar);
    m_recentNav->setObjectName("HomeNavItem");
    m_recentNav->setCheckable(true);
    m_recentNav->setChecked(true); // the only content view for now
    m_recentNav->setIconSize(QSize(16, 16));
    side->addWidget(m_recentNav);

    side->addStretch(1);
    root->addWidget(sidebar);

    // ── Main area ────────────────────────────────────────────────────────────
    auto* main = new QWidget(this);
    auto* mainCol = new QVBoxLayout(main);
    mainCol->setContentsMargins(36, 28, 36, 28);
    mainCol->setSpacing(16);

    auto* header = new QHBoxLayout;
    m_mainTitle = new QLabel(tr("Recent files"), main);
    m_mainTitle->setObjectName("HomeMainTitle");
    header->addWidget(m_mainTitle);
    header->addStretch(1);
    // List / card view toggle.
    m_listBtn = new QPushButton(main);
    m_gridBtn = new QPushButton(main);
    for (QPushButton* b : {m_listBtn, m_gridBtn}) {
        b->setObjectName("HomeViewToggle");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedSize(32, 30);
        b->setIconSize(QSize(16, 16));
        header->addWidget(b);
    }
    m_listBtn->setToolTip(tr("List view"));
    m_gridBtn->setToolTip(tr("Card view"));
    m_listBtn->setChecked(true);
    connect(m_listBtn, &QPushButton::clicked, this, [this] { setViewMode(ViewMode::List); });
    connect(m_gridBtn, &QPushButton::clicked, this, [this] { setViewMode(ViewMode::Grid); });
    mainCol->addLayout(header);

    m_empty = new QLabel(tr("No recent files yet. Open a document or drop a PDF here."), main);
    m_empty->setObjectName("OutlineEmpty");
    m_empty->setWordWrap(true);
    mainCol->addWidget(m_empty);

    m_scroll = new QScrollArea(main);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mainCol->addWidget(m_scroll, 1);

    root->addWidget(main, 1);

    rebuildIcons();
    refresh();
    connect(&Theme::instance(), &Theme::changed, this, [this] {
        rebuildIcons();
        refresh(); // rebuild cards/tiles so their palette-derived styles follow the theme
        update();
    });
}

void HomeView::rebuildIcons() {
    const auto& pal = Theme::instance().palette();
    // The recents scroll sits on the canvas backdrop (so surface cards stand out);
    // its viewport otherwise paints the palette Base (surface).
    if (m_scroll) {
        m_scroll->viewport()->setAutoFillBackground(true);
        QPalette vp = m_scroll->viewport()->palette();
        vp.setColor(QPalette::Window, pal.canvas);
        vp.setColor(QPalette::Base, pal.canvas);
        m_scroll->viewport()->setPalette(vp);
    }
    m_logo->setPixmap(Theme::instance().brandLogo(30));
    m_openBtn->setIcon(Theme::instance().icon("folder-open", QColor(255, 255, 255)));
    m_recentNav->setIcon(
        Theme::instance().icon("file", m_recentNav->isChecked() ? pal.accent : pal.text));
    m_listBtn->setIcon(Theme::instance().icon(
        "view-list", m_viewMode == ViewMode::List ? pal.accent : pal.dim));
    m_gridBtn->setIcon(Theme::instance().icon(
        "view-grid", m_viewMode == ViewMode::Grid ? pal.accent : pal.dim));
}

void HomeView::setViewMode(ViewMode mode) {
    m_viewMode = mode;
    m_listBtn->setChecked(mode == ViewMode::List);
    m_gridBtn->setChecked(mode == ViewMode::Grid);
    rebuildIcons();
    refresh();
}

int HomeView::gridColumns() const {
    // 150px tiles + 12px spacing.
    const int avail = m_scroll ? m_scroll->viewport()->width() : width();
    return std::max(1, (avail + 12) / 162);
}

void HomeView::refresh() {
    QSettings settings;
    QStringList files = settings.value(kRecentFilesKey).toStringList();
    // Only offer files that still exist.
    files.erase(std::remove_if(files.begin(), files.end(),
                               [](const QString& p) { return !QFileInfo::exists(p); }),
                files.end());
    if (files.size() > kMaxCards)
        files = files.mid(0, kMaxCards);

    m_empty->setVisible(files.isEmpty());

    // Rebuild the card container with the layout for the current view mode.
    auto* container = new QWidget(m_scroll);
    const bool grid = m_viewMode == ViewMode::Grid;
    m_lastColumns = grid ? gridColumns() : 1;

    if (grid) {
        auto* g = new QGridLayout(container);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(12);
        g->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        for (int i = 0; i < files.size(); ++i) {
            auto* tile = new RecentTile(files[i], container);
            connect(tile, &RecentTile::activated, this, &HomeView::openPathRequested);
            g->addWidget(tile, i / m_lastColumns, i % m_lastColumns, Qt::AlignLeft | Qt::AlignTop);
        }
        g->setColumnStretch(m_lastColumns, 1); // push fixed-size tiles to the left
    } else {
        auto* v = new QVBoxLayout(container);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(8);
        v->setAlignment(Qt::AlignTop);
        for (const QString& path : files) {
            auto* card = new RecentCard(path, container);
            connect(card, &RecentCard::activated, this, &HomeView::openPathRequested);
            v->addWidget(card);
        }
    }

    m_scroll->setWidget(container); // deletes the previous container
    m_cards = container;
}

void HomeView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    // In card view, re-flow when the fitting column count changes.
    if (m_viewMode == ViewMode::Grid && gridColumns() != m_lastColumns)
        refresh();
}

void HomeView::dragEnterEvent(QDragEnterEvent* event) {
    if (!pdfFromMime(event->mimeData()).isEmpty()) {
        m_dragActive = true;
        update();
        event->acceptProposedAction();
    }
}

void HomeView::dragLeaveEvent(QDragLeaveEvent*) {
    m_dragActive = false;
    update();
}

void HomeView::dropEvent(QDropEvent* event) {
    m_dragActive = false;
    update();
    const QString path = pdfFromMime(event->mimeData());
    if (!path.isEmpty()) {
        event->acceptProposedAction();
        emit openPathRequested(path);
    }
}

void HomeView::paintEvent(QPaintEvent*) {
    const auto& pal = Theme::instance().palette();
    QPainter p(this);
    p.fillRect(rect(), pal.canvas);

    // While a PDF is dragged over, show an accent dashed invitation frame.
    if (m_dragActive) {
        p.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(pal.accent, 2, Qt::DashLine);
        p.setPen(pen);
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()).adjusted(14, 14, -14, -14), 16, 16);
        p.drawPath(path);
    }
}

#include "HomeView.moc"
