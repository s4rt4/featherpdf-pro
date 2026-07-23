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

#include "ui/PrintDialog.h"

#include "ui/Theme.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QFutureWatcher>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfDocumentRenderOptions>
#include <QPrinterInfo>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStringList>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace {
constexpr int kPreviewW = 360;
constexpr int kPreviewH = 480;
constexpr int kOptionsW = 318;

// A QSS-safe colour literal, preserving alpha (mirrors Theme's internal css()).
QString css(const QColor& c) {
    if (c.alpha() == 255)
        return c.name(QColor::HexRgb);
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red())
        .arg(c.green())
        .arg(c.blue())
        .arg(QString::number(c.alphaF(), 'f', 3));
}

// QSS can't tint an SVG (lucide's stroke colour is baked in), so bake a tinted
// PNG of `name` once to the cache dir and hand back a path usable in url(...).
// Used for the combo/spin chevrons so they recolour with the theme.
QString tintedIconPath(const QString& name, const QColor& color) {
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/ctl-icons";
    QDir().mkpath(dir);
    const QString path = dir + '/' + name + '-' + color.name(QColor::HexArgb).mid(1) + ".png";
    if (!QFileInfo::exists(path)) {
        const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
        const QPixmap pm = Theme::instance().icon(name, color).pixmap(QSize(28, 28) * dpr);
        pm.save(path, "PNG");
    }
    return path;
}

// A small uppercase section header, matching the rail/tools-pane heads.
QLabel* sectionHead(const QString& text, QWidget* parent) {
    auto* l = new QLabel(text.toUpper(), parent);
    l->setObjectName(QStringLiteral("SectionHead"));
    return l;
}
} // namespace

PrintDialog::PrintDialog(QPdfDocument* pdf, QVector<int> order, QVector<int> rotations,
                         const QString& docName, int currentSlot, QWidget* parent)
    : QDialog(parent), m_pdf(pdf), m_order(std::move(order)), m_rot(std::move(rotations)),
      m_pageCount(m_order.size()), m_currentSlot(currentSlot) {
    setWindowTitle(tr("Print"));
    m_rot.resize(m_pageCount);

    const Theme::Palette& p = Theme::instance().palette();
    QColor ctlBorder = p.text; // a control outline that's visible but quiet
    ctlBorder.setAlpha(46);

    // Dialog-scoped sheet: themes the plain form controls (combos, fields,
    // radios, checkbox, spin) without leaking into the rest of the app, since a
    // widget's style sheet only cascades to its own children.
    const QString chevDown = tintedIconPath(QStringLiteral("chevron-down"), p.dim);
    const QString chevUp = tintedIconPath(QStringLiteral("chevron-up"), p.dim);
    setStyleSheet(
        QStringLiteral(
            "QLabel#SectionHead { color:%2; font-size:11px; font-weight:700; letter-spacing:.6px; }"
            "QLabel#FieldLabel { color:%3; }"
            // Combo + line edit + spin share one calm field look.
            "QComboBox, QLineEdit, QSpinBox { background:%1; border:1px solid %4;"
            " border-radius:8px; padding:6px 9px; color:%3; selection-background-color:%5;"
            " selection-color:#FFFFFF; min-height:18px; }"
            "QComboBox:focus, QLineEdit:focus, QSpinBox:focus { border:1px solid %5; }"
            "QLineEdit:disabled { color:%2; background:%6; }"
            "QComboBox::drop-down { border:none; width:26px; }"
            "QComboBox::down-arrow { image:url(%7); width:13px; height:13px; }"
            "QComboBox QAbstractItemView { background:%1; border:1px solid %4; border-radius:8px;"
            " padding:4px; outline:0; selection-background-color:%8; selection-color:%3; }"
            "QSpinBox::up-button, QSpinBox::down-button { background:transparent; border:none;"
            " width:20px; }"
            "QSpinBox::up-arrow { image:url(%9); width:11px; height:11px; }"
            "QSpinBox::down-arrow { image:url(%7); width:11px; height:11px; }"
            // Plain round radio; the selected dot is the branding accent.
            "QRadioButton, QCheckBox { color:%3; spacing:9px; }"
            "QRadioButton::indicator { width:16px; height:16px; border:1px solid %4;"
            " border-radius:9px; background:%1; }"
            "QRadioButton::indicator:checked { border:1px solid %5;"
            " background: qradialgradient(cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5,"
            " stop:0 %5, stop:0.5 %5, stop:0.56 %1, stop:1 %1); }"
            "QCheckBox::indicator { width:16px; height:16px; border:1px solid %4;"
            " border-radius:5px; background:%1; }"
            "QCheckBox::indicator:checked { border:1px solid %5; background:%5; }"
            // Small ghost buttons (browse, preview nav) inherit the neutral look
            // from the global sheet; only make them compact here.
            "QPushButton#GhostBtn { padding:3px; min-width:0; }"
            "QLabel#PreviewCounter { color:%3; font-family:'Source Code Pro',monospace;"
            " font-size:12px; }")
            .arg(css(p.surface), css(p.dim), css(p.text), css(ctlBorder), css(p.accent),
                 css(p.canvas), chevDown, css(p.accentTint), chevUp));

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(buildOptionsPane(docName)); // options on the left (Acrobat-style)
    root->addWidget(buildPreviewPane(), 1);      // preview fills the right

    // Discover system printers off the UI thread; add them when ready.
    auto* watcher = new QFutureWatcher<QStringList>(this);
    connect(watcher, &QFutureWatcher<QStringList>::finished, this, [this, watcher] {
        const QStringList names = watcher->result();
        watcher->deleteLater();
        if (m_printer->count() > 1 && !m_printer->itemData(1).isValid())
            m_printer->removeItem(1); // drop the "Searching…" placeholder
        for (const QString& n : names)
            m_printer->addItem(n, n);
    });
    watcher->setFuture(QtConcurrent::run([] {
        QStringList names;
        const QString def = QPrinterInfo::defaultPrinterName();
        for (const QPrinterInfo& pi : QPrinterInfo::availablePrinters())
            names << pi.printerName();
        if (!def.isEmpty() && names.removeAll(def) > 0)
            names.prepend(def); // default first
        return names;
    }));

    updateOutputRowVisibility();
    rebuildSelection();
    resize(kOptionsW + kPreviewW + 96, 560);
}

QWidget* PrintDialog::buildOptionsPane(const QString& docName) {
    auto* host = new QWidget(this);
    host->setObjectName(QStringLiteral("PrintOptions"));
    host->setFixedWidth(kOptionsW);
    host->setStyleSheet(
        QStringLiteral("#PrintOptions { background:%1; border-right:1px solid %2; }")
            .arg(css(Theme::instance().palette().surface),
                 css(Theme::instance().palette().hairline)));

    auto* opts = new QVBoxLayout(host);
    opts->setContentsMargins(22, 22, 22, 18);
    opts->setSpacing(9);

    // ── Destination ─────────────────────────────────────────────────────────
    opts->addWidget(sectionHead(tr("Destination"), host));
    m_printer = new QComboBox(host);
    m_printer->addItem(tr("Print to File (PDF)"), QString());
    m_printer->addItem(tr("Searching for printers…"));
    m_printer->setItemData(1, false, Qt::UserRole - 1); // disable the "searching" item
    opts->addWidget(m_printer);

    // Output file row (only for Print to File).
    m_outputRow = new QWidget(host);
    auto* outRow = new QHBoxLayout(m_outputRow);
    outRow->setContentsMargins(0, 0, 0, 0);
    outRow->setSpacing(6);
    const QString base = QFileInfo(docName).completeBaseName();
    m_outputFile = new QLineEdit(
        QDir(QDir::homePath()).filePath((base.isEmpty() ? tr("document") : base) + ".pdf"),
        m_outputRow);
    auto* browse = new QPushButton(QStringLiteral("…"), m_outputRow);
    browse->setObjectName(QStringLiteral("GhostBtn"));
    browse->setFixedWidth(36);
    browse->setCursor(Qt::PointingHandCursor);
    outRow->addWidget(m_outputFile);
    outRow->addWidget(browse);
    opts->addWidget(m_outputRow);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getSaveFileName(this, tr("Save PDF as"), m_outputFile->text(),
                                                       tr("PDF documents (*.pdf)"));
        if (!f.isEmpty())
            m_outputFile->setText(f);
    });

    // ── Pages ───────────────────────────────────────────────────────────────
    opts->addSpacing(8);
    opts->addWidget(sectionHead(tr("Pages"), host));
    m_rangeAll = new QRadioButton(tr("All %1 pages").arg(m_pageCount), host);
    m_rangeCurrent = new QRadioButton(tr("Current page"), host);
    m_rangeCustom = new QRadioButton(tr("Range"), host);
    m_rangeAll->setChecked(true);
    auto* rangeGroup = new QButtonGroup(this);
    for (QRadioButton* r : {m_rangeAll, m_rangeCurrent, m_rangeCustom})
        rangeGroup->addButton(r);
    opts->addWidget(m_rangeAll);
    opts->addWidget(m_rangeCurrent);

    auto* customRow = new QHBoxLayout;
    customRow->setSpacing(8);
    customRow->addWidget(m_rangeCustom);
    m_rangeText = new QLineEdit(host);
    m_rangeText->setPlaceholderText(tr("e.g. 1-5, 8, 11-13"));
    m_rangeText->setEnabled(false);
    customRow->addWidget(m_rangeText, 1);
    opts->addLayout(customRow);

    // Subset - print all / only odd / only even pages within the range.
    auto* subsetRow = new QHBoxLayout;
    subsetRow->setSpacing(8);
    auto* subsetLabel = new QLabel(tr("Subset"), host);
    subsetLabel->setObjectName(QStringLiteral("FieldLabel"));
    subsetRow->addWidget(subsetLabel);
    m_subset = new QComboBox(host);
    m_subset->addItem(tr("All pages in range"));
    m_subset->addItem(tr("Odd pages only"));
    m_subset->addItem(tr("Even pages only"));
    subsetRow->addWidget(m_subset, 1);
    opts->addLayout(subsetRow);

    // ── Copies & colour ───────────────────────────────────────────────────────
    opts->addSpacing(8);
    opts->addWidget(sectionHead(tr("Copies & Colour"), host));
    auto* copiesRow = new QHBoxLayout;
    auto* copiesLabel = new QLabel(tr("Copies"), host);
    copiesLabel->setObjectName(QStringLiteral("FieldLabel"));
    copiesRow->addWidget(copiesLabel);
    m_copies = new QSpinBox(host);
    m_copies->setRange(1, 99);
    m_copies->setFixedWidth(72);
    copiesRow->addStretch(1);
    copiesRow->addWidget(m_copies);
    opts->addLayout(copiesRow);

    m_grayscale = new QCheckBox(tr("Print in grayscale"), host);
    connect(m_grayscale, &QCheckBox::toggled, this, [this] { showPreviewAt(m_previewPos); });
    opts->addWidget(m_grayscale);

    opts->addStretch(1);

    auto* buttons = new QDialogButtonBox(host);
    auto* printBtn = buttons->addButton(tr("Print"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    printBtn->setObjectName(QStringLiteral("Share")); // reuse the accent-filled primary style
    printBtn->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    opts->addWidget(buttons);

    // React to option changes.
    auto refresh = [this] { rebuildSelection(); };
    connect(m_rangeAll, &QRadioButton::toggled, this, refresh);
    connect(m_rangeCurrent, &QRadioButton::toggled, this, refresh);
    connect(m_rangeCustom, &QRadioButton::toggled, this, [this, refresh](bool on) {
        m_rangeText->setEnabled(on);
        if (on)
            m_rangeText->setFocus();
        refresh();
    });
    connect(m_rangeText, &QLineEdit::textChanged, this, refresh);
    connect(m_subset, &QComboBox::currentIndexChanged, this, refresh);
    connect(m_printer, &QComboBox::currentIndexChanged, this,
            [this] { updateOutputRowVisibility(); });

    return host;
}

QWidget* PrintDialog::buildPreviewPane() {
    auto* pane = new QFrame(this);
    pane->setStyleSheet(
        QStringLiteral("background:%1;").arg(css(Theme::instance().palette().sunken)));
    auto* pv = new QVBoxLayout(pane);
    pv->setContentsMargins(28, 24, 28, 18);
    pv->setSpacing(16);

    pv->addStretch(1);
    m_preview = new QLabel(pane);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumSize(kPreviewW / 2, kPreviewH / 2);
    // The page is the one element that gets a prominent shadow (ui-guidelines).
    auto* shadow = new QGraphicsDropShadowEffect(m_preview);
    shadow->setBlurRadius(30);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 56));
    m_preview->setGraphicsEffect(shadow);
    pv->addWidget(m_preview, 0, Qt::AlignCenter);
    pv->addStretch(1);

    auto* navRow = new QHBoxLayout;
    navRow->setSpacing(10);
    navRow->addStretch(1);
    m_prevPreview = new QPushButton(QStringLiteral("‹"), pane);
    m_nextPreview = new QPushButton(QStringLiteral("›"), pane);
    m_previewCounter = new QLabel(pane);
    m_previewCounter->setObjectName(QStringLiteral("PreviewCounter"));
    m_previewCounter->setAlignment(Qt::AlignCenter);
    m_previewCounter->setMinimumWidth(120);
    for (QPushButton* b : {m_prevPreview, m_nextPreview}) {
        b->setObjectName(QStringLiteral("GhostBtn"));
        b->setFixedSize(30, 26);
        b->setCursor(Qt::PointingHandCursor);
    }
    navRow->addWidget(m_prevPreview);
    navRow->addWidget(m_previewCounter);
    navRow->addWidget(m_nextPreview);
    navRow->addStretch(1);
    pv->addLayout(navRow);

    connect(m_prevPreview, &QPushButton::clicked, this,
            [this] { showPreviewAt(m_previewPos - 1); });
    connect(m_nextPreview, &QPushButton::clicked, this,
            [this] { showPreviewAt(m_previewPos + 1); });

    return pane;
}

void PrintDialog::updateOutputRowVisibility() {
    const bool toFile = m_printer->currentData().toString().isEmpty();
    m_outputRow->setVisible(toFile);
}

QList<int> PrintDialog::parseRange(const QString& text) const {
    QList<int> pages;
    const QStringList parts = text.split(',', Qt::SkipEmptyParts);
    for (QString part : parts) {
        part = part.trimmed();
        if (part.contains('-')) {
            const QStringList ends = part.split('-');
            if (ends.size() != 2)
                continue;
            bool ok1 = false, ok2 = false;
            int a = ends[0].trimmed().toInt(&ok1);
            int b = ends[1].trimmed().toInt(&ok2);
            if (!ok1 || !ok2)
                continue;
            if (a > b)
                std::swap(a, b);
            for (int pg = a; pg <= b; ++pg)
                if (pg >= 1 && pg <= m_pageCount)
                    pages << (pg - 1);
        } else {
            bool ok = false;
            const int pg = part.toInt(&ok);
            if (ok && pg >= 1 && pg <= m_pageCount)
                pages << (pg - 1);
        }
    }
    return pages;
}

void PrintDialog::rebuildSelection() {
    m_selection.clear();
    if (m_rangeCurrent->isChecked()) {
        if (m_currentSlot >= 0 && m_currentSlot < m_pageCount)
            m_selection << m_currentSlot;
    } else if (m_rangeCustom->isChecked()) {
        m_selection = parseRange(m_rangeText->text());
    } else {
        for (int i = 0; i < m_pageCount; ++i)
            m_selection << i;
    }

    // Subset filter: keep only odd / even page numbers (1-based) within the range.
    const int subset = m_subset ? m_subset->currentIndex() : 0;
    if (subset == 1 || subset == 2) {
        const bool wantOdd = (subset == 1);
        QList<int> filtered;
        for (int slot : std::as_const(m_selection)) {
            const bool isOdd = ((slot + 1) % 2) == 1;
            if (isOdd == wantOdd)
                filtered << slot;
        }
        m_selection = filtered;
    }

    // The Print button needs a non-empty selection.
    if (auto* box = findChild<QDialogButtonBox*>()) {
        for (QAbstractButton* b : box->buttons()) {
            if (box->buttonRole(b) == QDialogButtonBox::AcceptRole)
                b->setEnabled(!m_selection.isEmpty());
        }
    }
    showPreviewAt(0);
}

void PrintDialog::showPreviewAt(int positionInSelection) {
    if (m_selection.isEmpty()) {
        m_preview->setPixmap({});
        m_preview->setText(tr("No pages selected"));
        m_preview->setFixedSize(kPreviewW / 2, kPreviewH / 2);
        m_previewCounter->clear();
        m_prevPreview->setEnabled(false);
        m_nextPreview->setEnabled(false);
        return;
    }
    m_previewPos = qBound(0, positionInSelection, m_selection.size() - 1);
    const int slot = m_selection[m_previewPos];
    const QPixmap pm = renderSlot(slot, kPreviewW, kPreviewH);
    m_preview->setPixmap(pm);
    // Size the label to the page so the drop shadow hugs the sheet, not a box.
    m_preview->setFixedSize(pm.isNull() ? QSize(kPreviewW / 2, kPreviewH / 2)
                                        : pm.deviceIndependentSize().toSize());
    m_previewCounter->setText(tr("%1 / %2  ·  page %3")
                                  .arg(m_previewPos + 1)
                                  .arg(m_selection.size())
                                  .arg(slot + 1));
    m_prevPreview->setEnabled(m_previewPos > 0);
    m_nextPreview->setEnabled(m_previewPos < m_selection.size() - 1);
}

QPixmap PrintDialog::renderSlot(int slot, int maxW, int maxH) const {
    if (!m_pdf || slot < 0 || slot >= m_order.size())
        return {};
    const int orig = m_order[slot];
    const int rot = m_rot.value(slot, 0);
    QSizeF pt = m_pdf->pagePointSize(orig);
    if (rot == 90 || rot == 270)
        pt.transpose();
    if (pt.width() <= 0 || pt.height() <= 0)
        return {};

    const qreal dpr = devicePixelRatioF();
    const double scale = std::min(maxW / pt.width(), maxH / pt.height());
    const QSize px(qRound(pt.width() * scale * dpr), qRound(pt.height() * scale * dpr));

    QPdfDocumentRenderOptions opts;
    opts.setRenderFlags(QPdfDocumentRenderOptions::RenderFlag::Annotations);
    switch (rot) {
    case 90: opts.setRotation(QPdfDocumentRenderOptions::Rotation::Clockwise90); break;
    case 180: opts.setRotation(QPdfDocumentRenderOptions::Rotation::Clockwise180); break;
    case 270: opts.setRotation(QPdfDocumentRenderOptions::Rotation::Clockwise270); break;
    default: break;
    }
    const QImage rendered = m_pdf->render(orig, px, opts);
    // The render has a transparent background, so always flatten onto white -
    // the sheet looks like real paper, and grayscale won't turn it solid black.
    QImage img(rendered.size(), QImage::Format_RGB32);
    img.fill(Qt::white);
    if (!rendered.isNull()) {
        QPainter pp(&img);
        pp.drawImage(0, 0, rendered);
        pp.end();
    }
    // Mirror the grayscale option so the preview shows what will actually print.
    if (m_grayscale && m_grayscale->isChecked())
        img = img.convertToFormat(QImage::Format_Grayscale8);
    QPixmap pm = QPixmap::fromImage(img);
    pm.setDevicePixelRatio(dpr);
    return pm;
}

QString PrintDialog::selectedPrinter() const {
    return m_printer->currentData().toString();
}

QString PrintDialog::outputFile() const {
    return m_outputFile->text();
}

QList<int> PrintDialog::selectedSlots() const {
    return m_selection;
}

int PrintDialog::copies() const {
    return m_copies->value();
}

bool PrintDialog::grayscale() const {
    return m_grayscale->isChecked();
}
