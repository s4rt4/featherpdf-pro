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

#include "ui/WatermarkDialog.h"

#include "ui/Theme.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

WatermarkDialog::WatermarkDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Watermark"));

    const Theme::Palette& p = Theme::instance().palette();
    QColor ctlBorder = p.text;
    ctlBorder.setAlpha(46);
    const auto css = [](const QColor& c) {
        return c.alpha() == 255 ? c.name(QColor::HexRgb)
                                : QStringLiteral("rgba(%1,%2,%3,%4)")
                                      .arg(c.red())
                                      .arg(c.green())
                                      .arg(c.blue())
                                      .arg(QString::number(c.alphaF(), 'f', 3));
    };
    const QString chev = Theme::instance().symbolicIconPath(QStringLiteral("chevron-down"), p.dim);
    const QString chevUp = Theme::instance().symbolicIconPath(QStringLiteral("chevron-up"), p.dim);
    setStyleSheet(
        QStringLiteral(
            "QLabel#Hint { color:%2; }"
            "QLineEdit, QSpinBox { background:%1; border:1px solid %3; border-radius:8px;"
            " padding:6px 9px; color:%4; selection-background-color:%5; selection-color:#FFFFFF; }"
            "QLineEdit:focus, QSpinBox:focus { border:1px solid %5; }"
            "QSpinBox::up-button, QSpinBox::down-button { background:transparent; border:none;"
            " width:18px; }"
            "QSpinBox::up-arrow { image:url(%6); width:11px; height:11px; }"
            "QSpinBox::down-arrow { image:url(%7); width:11px; height:11px; }")
            .arg(css(p.surface), css(p.dim), css(ctlBorder), css(p.text), css(p.accent), chevUp,
                 chev));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(14);

    auto* hint = new QLabel(tr("Stamp every page with text or an image."), this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    // Text / Image mode.
    auto* modeRow = new QHBoxLayout;
    m_modeText = new QRadioButton(tr("Text"), this);
    m_modeImage = new QRadioButton(tr("Image"), this);
    m_modeText->setChecked(true);
    modeRow->addWidget(m_modeText);
    modeRow->addWidget(m_modeImage);
    modeRow->addStretch(1);
    root->addLayout(modeRow);

    // ── Text rows ─────────────────────────────────────────────────────────────
    m_text = new QLineEdit(tr("CONFIDENTIAL"), this);
    m_text->setMinimumWidth(260);
    m_colors = {QColor(120, 120, 120), QColor(200, 0, 0), QColor(40, 90, 200),
                QColor(30, 140, 70)};
    auto* swatchRow = new QHBoxLayout;
    swatchRow->setSpacing(8);
    const QString textColor = p.text.name();
    for (int i = 0; i < m_colors.size(); ++i) {
        auto* sw = new QPushButton(this);
        sw->setCheckable(true);
        sw->setCursor(Qt::PointingHandCursor);
        sw->setFixedSize(22, 22);
        sw->setStyleSheet(
            QStringLiteral("QPushButton { background:%1; border:1px solid rgba(0,0,0,0.25);"
                           " border-radius:5px; } QPushButton:checked { border:2px solid %2; }")
                .arg(m_colors[i].name(), textColor));
        connect(sw, &QPushButton::clicked, this, [this, i] { selectSwatch(i); });
        m_swatches.append(sw);
        swatchRow->addWidget(sw);
    }
    swatchRow->addStretch(1);
    m_size = new QSpinBox(this);
    m_size->setRange(8, 200);
    m_size->setValue(64);
    m_size->setSuffix(QStringLiteral(" pt"));

    m_textRows = new QWidget(this);
    auto* tf = new QFormLayout(m_textRows);
    tf->setContentsMargins(0, 0, 0, 0);
    tf->setSpacing(10);
    tf->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    tf->addRow(tr("Text"), m_text);
    tf->addRow(tr("Colour"), swatchRow);
    tf->addRow(tr("Size"), m_size);
    root->addWidget(m_textRows);

    // ── Image rows ────────────────────────────────────────────────────────────
    m_imagePath = new QLineEdit(this);
    m_imagePath->setPlaceholderText(tr("Choose a PNG or JPEG…"));
    auto* browse = new QPushButton(QStringLiteral("…"), this);
    browse->setObjectName(QStringLiteral("GhostBtn"));
    browse->setFixedWidth(36);
    browse->setCursor(Qt::PointingHandCursor);
    auto* imgRow = new QHBoxLayout;
    imgRow->setSpacing(6);
    imgRow->addWidget(m_imagePath, 1);
    imgRow->addWidget(browse);
    m_scale = new QSpinBox(this);
    m_scale->setRange(5, 100);
    m_scale->setValue(50);
    m_scale->setSuffix(QStringLiteral(" % width"));

    m_imageRows = new QWidget(this);
    auto* imf = new QFormLayout(m_imageRows);
    imf->setContentsMargins(0, 0, 0, 0);
    imf->setSpacing(10);
    imf->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    imf->addRow(tr("Image"), imgRow);
    imf->addRow(tr("Size"), m_scale);
    root->addWidget(m_imageRows);

    // ── Common ────────────────────────────────────────────────────────────────
    m_opacity = new QSpinBox(this);
    m_opacity->setRange(5, 100);
    m_opacity->setValue(30);
    m_opacity->setSuffix(QStringLiteral(" %"));
    m_rotation = new QSpinBox(this);
    m_rotation->setRange(-90, 90);
    m_rotation->setValue(45);
    m_rotation->setSuffix(QStringLiteral("°"));
    auto* common = new QFormLayout;
    common->setSpacing(10);
    common->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    common->addRow(tr("Opacity"), m_opacity);
    common->addRow(tr("Rotation"), m_rotation);
    root->addLayout(common);
    root->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    m_apply = buttons->addButton(tr("Add Watermark"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    m_apply->setObjectName(QStringLiteral("Share"));
    m_apply->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    connect(browse, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(
            this, tr("Choose watermark image"), QString(),
            tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"));
        if (!f.isEmpty())
            m_imagePath->setText(f);
        updateMode();
    });
    auto revalidate = [this] { updateMode(); };
    connect(m_modeText, &QRadioButton::toggled, this, revalidate);
    connect(m_text, &QLineEdit::textChanged, this, revalidate);
    connect(m_imagePath, &QLineEdit::textChanged, this, revalidate);

    selectSwatch(0);
    updateMode();
    resize(450, 0);
}

void WatermarkDialog::updateMode() {
    const bool image = m_modeImage->isChecked();
    m_textRows->setVisible(!image);
    m_imageRows->setVisible(image);
    const bool ok = image ? !m_imagePath->text().trimmed().isEmpty()
                          : !m_text->text().trimmed().isEmpty();
    m_apply->setEnabled(ok);
}

void WatermarkDialog::selectSwatch(int i) {
    for (int j = 0; j < m_swatches.size(); ++j)
        m_swatches[j]->setChecked(j == i);
    if (i >= 0 && i < m_colors.size())
        m_color = m_colors[i];
}

bool WatermarkDialog::useImage() const { return m_modeImage->isChecked(); }
QString WatermarkDialog::imagePath() const { return m_imagePath->text().trimmed(); }
double WatermarkDialog::scale() const { return m_scale->value() / 100.0; }
QString WatermarkDialog::text() const { return m_text->text().trimmed(); }
double WatermarkDialog::opacity() const { return m_opacity->value() / 100.0; }
double WatermarkDialog::fontSize() const { return m_size->value(); }
double WatermarkDialog::rotation() const { return m_rotation->value(); }
