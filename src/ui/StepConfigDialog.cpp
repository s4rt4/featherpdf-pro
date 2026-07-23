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

#include "ui/StepConfigDialog.h"

#include "ui/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

using Op = BatchRunner::Op;

StepConfigDialog::StepConfigDialog(Op op, const QVariantMap& p, QWidget* parent)
    : QDialog(parent), m_op(op) {
    setWindowTitle(BatchRunner::opLabel(op));

    const Theme::Palette& pal = Theme::instance().palette();
    QColor ctlBorder = pal.text;
    ctlBorder.setAlpha(46);
    const auto css = [](const QColor& c) {
        return c.alpha() == 255 ? c.name(QColor::HexRgb)
                                : QStringLiteral("rgba(%1,%2,%3,%4)")
                                      .arg(c.red())
                                      .arg(c.green())
                                      .arg(c.blue())
                                      .arg(QString::number(c.alphaF(), 'f', 3));
    };
    const QString chevDown = Theme::instance().symbolicIconPath(QStringLiteral("chevron-down"), pal.dim);
    const QString chevUp = Theme::instance().symbolicIconPath(QStringLiteral("chevron-up"), pal.dim);
    setStyleSheet(
        QStringLiteral(
            "QLabel#Hint { color:%2; }"
            "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox { background:%1; border:1px solid %3;"
            " border-radius:8px; padding:6px 9px; color:%4; selection-background-color:%5;"
            " selection-color:#FFFFFF; }"
            "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus"
            " { border:1px solid %5; }"
            "QComboBox::drop-down { border:none; width:24px; }"
            "QComboBox::down-arrow { image:url(%6); width:12px; height:12px; }"
            "QComboBox QAbstractItemView { background:%1; border:1px solid %3; border-radius:8px;"
            " padding:4px; outline:0; selection-background-color:%7; selection-color:%4; }"
            // Both spin-box flavours need their arrows restyled explicitly, or Qt
            // drops the native arrows once the base widget is themed.
            "QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button,"
            " QDoubleSpinBox::down-button { background:transparent; border:none; width:18px; }"
            "QSpinBox::up-arrow, QDoubleSpinBox::up-arrow { image:url(%8); width:11px; height:11px; }"
            "QSpinBox::down-arrow, QDoubleSpinBox::down-arrow { image:url(%6); width:11px;"
            " height:11px; }"
            "QCheckBox { color:%4; spacing:8px; }"
            "QCheckBox::indicator { width:16px; height:16px; border:1px solid %3; border-radius:5px;"
            " background:%1; }"
            "QCheckBox::indicator:checked { border:1px solid %5; background:%5; }")
            .arg(css(pal.surface), css(pal.dim), css(ctlBorder), css(pal.text), css(pal.accent),
                 chevDown, css(pal.accentTint), chevUp));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(14);

    auto* form = new QFormLayout;
    form->setSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    switch (op) {
    case Op::Optimize:
        addInt(form, QStringLiteral("dpi"), tr("Downsample images to (DPI)"),
               p.value(QStringLiteral("dpi"), 150).toInt(), 0, 1200);
        addBool(form, QStringLiteral("unembedFonts"), tr("Unembed fonts"),
                p.value(QStringLiteral("unembedFonts")).toBool());
        addBool(form, QStringLiteral("compress"), tr("Recompress and pack objects"),
                p.value(QStringLiteral("compress"), true).toBool());
        break;
    case Op::Watermark:
        addText(form, QStringLiteral("text"), tr("Text"), p.value(QStringLiteral("text")).toString(),
                tr("e.g. CONFIDENTIAL"));
        addDouble(form, QStringLiteral("opacity"), tr("Opacity"),
                  p.value(QStringLiteral("opacity"), 0.30).toDouble(), 0.05, 1.0, 0.05);
        addDouble(form, QStringLiteral("fontSize"), tr("Font size"),
                  p.value(QStringLiteral("fontSize"), 64.0).toDouble(), 8, 200, 2);
        addDouble(form, QStringLiteral("angle"), tr("Angle"),
                  p.value(QStringLiteral("angle"), 45.0).toDouble(), -90, 90, 5);
        break;
    case Op::Bates:
        addText(form, QStringLiteral("prefix"), tr("Prefix"),
                p.value(QStringLiteral("prefix")).toString(), tr("e.g. ABC-"));
        addText(form, QStringLiteral("suffix"), tr("Suffix"),
                p.value(QStringLiteral("suffix")).toString());
        addInt(form, QStringLiteral("start"), tr("Start number"),
               p.value(QStringLiteral("start"), 1).toInt(), 0, 1000000000);
        addInt(form, QStringLiteral("digits"), tr("Digits"),
               p.value(QStringLiteral("digits"), 6).toInt(), 1, 12);
        break;
    case Op::HeaderFooter:
        addText(form, QStringLiteral("headerLeft"), tr("Header left"),
                p.value(QStringLiteral("headerLeft")).toString());
        addText(form, QStringLiteral("headerCenter"), tr("Header center"),
                p.value(QStringLiteral("headerCenter")).toString());
        addText(form, QStringLiteral("headerRight"), tr("Header right"),
                p.value(QStringLiteral("headerRight")).toString());
        addText(form, QStringLiteral("footerLeft"), tr("Footer left"),
                p.value(QStringLiteral("footerLeft")).toString());
        addText(form, QStringLiteral("footerCenter"), tr("Footer center"),
                p.value(QStringLiteral("footerCenter")).toString(), tr("e.g. Page {n} of {p}"));
        addText(form, QStringLiteral("footerRight"), tr("Footer right"),
                p.value(QStringLiteral("footerRight")).toString());
        addInt(form, QStringLiteral("startNumber"), tr("First {n} value"),
               p.value(QStringLiteral("startNumber"), 1).toInt(), 0, 1000000000);
        break;
    case Op::Sanitize:
        addBool(form, QStringLiteral("metadata"), tr("Remove metadata"),
                p.value(QStringLiteral("metadata"), true).toBool());
        addBool(form, QStringLiteral("attachments"), tr("Remove embedded files"),
                p.value(QStringLiteral("attachments"), true).toBool());
        addBool(form, QStringLiteral("scripts"), tr("Remove scripts & actions"),
                p.value(QStringLiteral("scripts"), true).toBool());
        break;
    case Op::Encrypt:
        addText(form, QStringLiteral("password"), tr("Open password"),
                p.value(QStringLiteral("password")).toString(), tr("optional"));
        addBool(form, QStringLiteral("allowPrint"), tr("Allow printing"),
                p.value(QStringLiteral("allowPrint"), true).toBool());
        addBool(form, QStringLiteral("allowCopy"), tr("Allow copying"),
                p.value(QStringLiteral("allowCopy"), true).toBool());
        addBool(form, QStringLiteral("allowEdit"), tr("Allow editing"),
                p.value(QStringLiteral("allowEdit"), true).toBool());
        break;
    case Op::Rotate:
        addAngle(form, QStringLiteral("angle"), tr("Rotate by"),
                 p.value(QStringLiteral("angle"), 90).toInt());
        break;
    case Op::Flatten:
        addBool(form, QStringLiteral("raster"), tr("Rasterize pages (strongest)"),
                p.value(QStringLiteral("raster")).toBool());
        addInt(form, QStringLiteral("dpi"), tr("Raster DPI"),
               p.value(QStringLiteral("dpi"), 150).toInt(), 72, 600);
        break;
    case Op::PdfA: {
        auto* note = new QLabel(tr("Tags the file toward PDF/A-1b. No options."), this);
        note->setObjectName(QStringLiteral("Hint"));
        note->setWordWrap(true);
        root->addWidget(note);
        break;
    }
    case Op::Ocr:
        addText(form, QStringLiteral("language"), tr("Language"),
                p.value(QStringLiteral("language"), QStringLiteral("eng")).toString(),
                tr("e.g. eng or eng+ind"));
        addBool(form, QStringLiteral("autoLanguage"), tr("Detect language automatically"),
                p.value(QStringLiteral("autoLanguage")).toBool());
        addBool(form, QStringLiteral("deskew"), tr("Deskew"),
                p.value(QStringLiteral("deskew"), true).toBool());
        addBool(form, QStringLiteral("despeckle"), tr("Despeckle"),
                p.value(QStringLiteral("despeckle"), true).toBool());
        addBool(form, QStringLiteral("binarize"), tr("Binarize"),
                p.value(QStringLiteral("binarize"), true).toBool());
        break;
    }
    root->addLayout(form);

    const int fieldHeight = fontMetrics().height() + 14;
    for (QLineEdit* le : findChildren<QLineEdit*>())
        le->setMinimumHeight(fieldHeight);

    root->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    auto* ok = buttons->addButton(tr("Done"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    ok->setObjectName(QStringLiteral("Share"));
    ok->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    setMinimumWidth(380);
}

void StepConfigDialog::addText(QFormLayout* form, const QString& key, const QString& label,
                               const QString& value, const QString& placeholder) {
    auto* e = new QLineEdit(value, this);
    if (!placeholder.isEmpty())
        e->setPlaceholderText(placeholder);
    if (key == QLatin1String("password"))
        e->setEchoMode(QLineEdit::Password);
    m_editors.insert(key, e);
    form->addRow(label, e);
}

void StepConfigDialog::addInt(QFormLayout* form, const QString& key, const QString& label, int value,
                              int min, int max) {
    auto* e = new QSpinBox(this);
    e->setRange(min, max);
    e->setValue(value);
    m_editors.insert(key, e);
    form->addRow(label, e);
}

void StepConfigDialog::addDouble(QFormLayout* form, const QString& key, const QString& label,
                                 double value, double min, double max, double step) {
    auto* e = new QDoubleSpinBox(this);
    e->setRange(min, max);
    e->setSingleStep(step);
    e->setValue(value);
    m_editors.insert(key, e);
    form->addRow(label, e);
}

void StepConfigDialog::addBool(QFormLayout* form, const QString& key, const QString& label,
                               bool value) {
    auto* e = new QCheckBox(label, this);
    e->setChecked(value);
    m_editors.insert(key, e);
    form->addRow(QString(), e);
}

void StepConfigDialog::addAngle(QFormLayout* form, const QString& key, const QString& label,
                                int value) {
    auto* e = new QComboBox(this);
    e->addItem(tr("90° clockwise"), 90);
    e->addItem(tr("180°"), 180);
    e->addItem(tr("270° clockwise"), 270);
    const int idx = e->findData(value);
    e->setCurrentIndex(idx < 0 ? 0 : idx);
    m_editors.insert(key, e);
    form->addRow(label, e);
}

QVariantMap StepConfigDialog::params() const {
    QVariantMap out;
    for (auto it = m_editors.constBegin(); it != m_editors.constEnd(); ++it) {
        const QString& key = it.key();
        QWidget* w = it.value();
        if (auto* c = qobject_cast<QCheckBox*>(w))
            out[key] = c->isChecked();
        else if (auto* s = qobject_cast<QSpinBox*>(w))
            out[key] = s->value();
        else if (auto* d = qobject_cast<QDoubleSpinBox*>(w))
            out[key] = d->value();
        else if (auto* cb = qobject_cast<QComboBox*>(w))
            out[key] = cb->currentData();
        else if (auto* le = qobject_cast<QLineEdit*>(w))
            out[key] = le->text();
    }
    return out;
}
