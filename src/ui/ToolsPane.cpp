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

#include "ui/ToolsPane.h"

#include "ui/CustomizeToolsDialog.h"
#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QScrollArea>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>

// One tool row: accent-tinted icon chip + label + optional expand chevron.
// Hover background comes from the #ToolsPane QSS. Clicking emits activated().
class ToolRow : public QWidget {
    Q_OBJECT

public:
    ToolRow(QString id, const QString& label, QString iconName, bool expandable, QWidget* parent)
        : QWidget(parent), m_id(std::move(id)), m_iconName(std::move(iconName)) {
        setObjectName("ToolRow");
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_StyledBackground, true);

        setToolTip(label); // shown when collapsed to icon-only

        m_row = new QHBoxLayout(this);
        m_row->setContentsMargins(16, 9, 16, 9);
        m_row->setSpacing(11);

        m_row->addStretch(0); // leading spacer (index 0): only active when compact

        m_chip = new QLabel(this);
        m_chip->setObjectName("ToolChip");
        m_chip->setFixedSize(26, 26);
        m_chip->setAlignment(Qt::AlignCenter);
        m_row->addWidget(m_chip);

        m_name = new QLabel(label, this);
        m_row->addWidget(m_name, 1);

        if (expandable) {
            m_chev = new QLabel(this);
            m_chev->setFixedSize(14, 14);
            m_chev->setAlignment(Qt::AlignCenter);
            m_row->addWidget(m_chev);
        }

        m_row->addStretch(0); // trailing spacer: only active when compact
        m_trailIdx = m_row->count() - 1;
    }

    // Icon-only: hide the label and chevron, centre the chip with the leading
    // and trailing spacers so it stays clear of the pane's scrollbar.
    void setCompact(bool compact) {
        m_name->setVisible(!compact);
        if (m_chev)
            m_chev->setVisible(!compact);
        m_row->setContentsMargins(compact ? 0 : 16, 9, compact ? 0 : 16, 9);
        m_row->setStretch(0, compact ? 1 : 0);
        m_row->setStretch(m_trailIdx, compact ? 1 : 0);
    }

    void refresh() {
        const auto& pal = Theme::instance().palette();
        m_chip->setStyleSheet(
            QStringLiteral("background:%1; border-radius:7px;")
                .arg(pal.accentTint.name()));
        m_chip->setPixmap(Theme::instance().icon(m_iconName, pal.accent).pixmap(15, 15));
        if (m_chev)
            m_chev->setPixmap(Theme::instance().icon("chevron-right", pal.dim).pixmap(14, 14));
    }

    QString id() const { return m_id; }

signals:
    void activated(const QString& id);

protected:
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && rect().contains(e->pos()))
            emit activated(m_id);
        QWidget::mouseReleaseEvent(e);
    }

private:
    QString m_id;
    QString m_iconName;
    QHBoxLayout* m_row = nullptr;
    QLabel* m_chip = nullptr;
    QLabel* m_name = nullptr;
    QLabel* m_chev = nullptr;
    int m_trailIdx = -1;
};

ToolsPane::ToolsPane(QWidget* parent) : QWidget(parent) {
    setObjectName("ToolsPane");
    setFixedWidth(232);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Header: the TOOLS label and a collapse/expand toggle.
    auto* header = new QWidget(this);
    auto* headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(16, 12, 8, 8);
    headerRow->setSpacing(0);
    m_head = new QLabel(tr("TOOLS"), header);
    m_head->setObjectName("PaneHead");
    headerRow->addWidget(m_head);
    headerRow->addStretch(1);
    m_toggle = new QToolButton(header);
    m_toggle->setObjectName("ToolsCollapse");
    m_toggle->setCursor(Qt::PointingHandCursor);
    m_toggle->setAutoRaise(true);
    m_toggle->setFixedSize(26, 26);
    m_toggle->setToolTip(tr("Collapse the tools pane"));
    connect(m_toggle, &QToolButton::clicked, this, [this] { setCollapsed(!m_collapsed); });
    headerRow->addWidget(m_toggle);
    outer->addWidget(header);

    // The tool rows live in a scroll area so the pane can shrink vertically. A
    // pane that demanded the full height of all rows gave the window a huge
    // minimum height, which blocked GNOME window tiling/snapping.
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("ToolsScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->viewport()->setStyleSheet(QStringLiteral("background:transparent;"));
    auto* rowsHost = new QWidget(scroll);
    rowsHost->setObjectName("ToolsRowsHost");
    rowsHost->setStyleSheet(QStringLiteral("#ToolsRowsHost { background:transparent; }"));
    auto* col = new QVBoxLayout(rowsHost);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);
    m_col = col;
    scroll->setWidget(rowsHost);
    outer->addWidget(scroll, 1);

    struct Def {
        const char* id;
        const char* label;
        const char* icon;
        bool expandable;
    };
    const Def defs[] = {
        {"export", QT_TR_NOOP("Export"), "export", true},
        {"create", QT_TR_NOOP("Create"), "create", true},
        {"scan", QT_TR_NOOP("Scan"), "scan", false},
        {"ocr", QT_TR_NOOP("Recognize text"), "search", false},
        {"edit", QT_TR_NOOP("Edit text"), "edit", false},
        {"read-aloud", QT_TR_NOOP("Read aloud"), "volume-2", false},
        {"forms", QT_TR_NOOP("Forms"), "form", false},
        {"prepare-form", QT_TR_NOOP("Prepare form"), "wand", false},
        {"comment", QT_TR_NOOP("Comment"), "comment", false},
        {"combine", QT_TR_NOOP("Combine"), "combine", true},
        {"split", QT_TR_NOOP("Split"), "split", false},
        {"compare", QT_TR_NOOP("Compare"), "compare", false},
        {"optimize", QT_TR_NOOP("Optimize"), "optimize", false},
        {"cmyk", QT_TR_NOOP("RGB to CMYK"), "cmyk", false},
        {"watermark", QT_TR_NOOP("Watermark"), "watermark", false},
        {"bates", QT_TR_NOOP("Bates numbering"), "bates", false},
        {"protect", QT_TR_NOOP("Protect"), "lock", false},
        {"flatten", QT_TR_NOOP("Flatten"), "layers", false},
        {"organize", QT_TR_NOOP("Organize"), "organize", true},
        {"redact", QT_TR_NOOP("Redact"), "redact", false},
        {"snapshot", QT_TR_NOOP("Snapshot"), "crop", false},
        {"measure", QT_TR_NOOP("Measure"), "ruler", false},
        {"sign", QT_TR_NOOP("Sign"), "sign", false},
        {"stamp", QT_TR_NOOP("Stamp"), "stamp", false},
        {"pdfa", QT_TR_NOOP("PDF/A"), "badge-check", false},
        {"batch", QT_TR_NOOP("Batch / Action"), "gear", false},
    };
    for (const Def& d : defs) {
        m_catalog.append({QString::fromLatin1(d.id), tr(d.label), QString::fromLatin1(d.icon),
                          d.expandable});
        auto* r = new ToolRow(d.id, tr(d.label), d.icon, d.expandable, this);
        connect(r, &ToolRow::activated, this, &ToolsPane::toolActivated);
        m_rows.append(r);
        col->addWidget(r);
    }

    col->addStretch(1);

    m_customize = new QPushButton(tr("Customize tools"), this);
    m_customize->setObjectName("CustomizeTools");
    m_customize->setCursor(Qt::PointingHandCursor);
    connect(m_customize, &QPushButton::clicked, this, &ToolsPane::openCustomize);
    outer->addWidget(m_customize);

    loadOrder();
    applyOrder();
    if (QSettings().value(QStringLiteral("toolsPane/collapsed"), false).toBool())
        setCollapsed(true);
    refreshIcons();
    connect(&Theme::instance(), &Theme::changed, this, &ToolsPane::refreshIcons);
}

QStringList ToolsPane::defaultOrder() const {
    QStringList ids;
    for (const ToolDef& d : m_catalog)
        ids << d.id;
    return ids;
}

void ToolsPane::loadOrder() {
    const QStringList saved = QSettings().value(QStringLiteral("toolsPane/visible")).toStringList();
    // Keep only known ids; fall back to showing everything when nothing is saved.
    QStringList valid;
    const QStringList all = defaultOrder();
    for (const QString& id : saved)
        if (all.contains(id) && !valid.contains(id))
            valid << id;
    m_order = saved.isEmpty() ? all : valid;
}

void ToolsPane::saveOrder() const {
    QSettings().setValue(QStringLiteral("toolsPane/visible"), m_order);
}

void ToolsPane::applyOrder() {
    QHash<QString, ToolRow*> byId;
    for (ToolRow* r : m_rows)
        byId.insert(r->id(), r);

    // Detach every row, then re-insert the visible ones in order after the header.
    for (ToolRow* r : m_rows) {
        m_col->removeWidget(r);
        r->hide();
    }
    int idx = 0; // rows host holds only rows (+ a trailing stretch)
    for (const QString& id : m_order) {
        ToolRow* r = byId.value(id, nullptr);
        if (!r)
            continue;
        m_col->insertWidget(idx++, r);
        r->show();
        r->setCompact(m_collapsed);
    }
}

void ToolsPane::openCustomize() {
    QVector<CustomizeToolsDialog::Tool> catalog;
    for (const ToolDef& d : m_catalog)
        catalog.append({d.id, d.label, d.icon});

    CustomizeToolsDialog dialog(catalog, m_order, defaultOrder(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_order = dialog.visibleOrderedIds();
    applyOrder();
    saveOrder();
}

void ToolsPane::setCollapsed(bool collapsed) {
    m_collapsed = collapsed;
    QSettings().setValue(QStringLiteral("toolsPane/collapsed"), collapsed);
    setFixedWidth(collapsed ? 52 : 232);
    m_head->setVisible(!collapsed);
    for (ToolRow* r : m_rows)
        r->setCompact(collapsed);
    m_customize->setText(collapsed ? QString() : tr("Customize tools"));
    m_customize->setToolTip(tr("Customize tools"));
    m_toggle->setToolTip(collapsed ? tr("Expand the tools pane") : tr("Collapse the tools pane"));
    refreshIcons(); // re-point the toggle chevron
}

void ToolsPane::refreshIcons() {
    const auto& pal = Theme::instance().palette();
    for (ToolRow* r : m_rows)
        r->refresh();
    if (m_customize) {
        m_customize->setIcon(Theme::instance().icon("sliders", pal.dim));
        m_customize->setIconSize(QSize(14, 14));
    }
    if (m_toggle) {
        // Pane sits on the right: chevron-right collapses, chevron-left expands.
        m_toggle->setIcon(
            Theme::instance().icon(m_collapsed ? "chevron-left" : "chevron-right", pal.dim));
        m_toggle->setIconSize(QSize(15, 15));
    }
}

#include "ToolsPane.moc"
