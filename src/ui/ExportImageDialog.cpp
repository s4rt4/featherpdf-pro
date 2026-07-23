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

#include "ui/ExportImageDialog.h"

#include "ui/Theme.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

ExportImageDialog::ExportImageDialog(int pageCount, int current, QWidget* parent)
    : QDialog(parent), m_pageCount(qMax(0, pageCount)) {
    setWindowTitle(tr("Export Pages as Images"));

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
    setStyleSheet(
        QStringLiteral("QLabel#Hint { color:%1; font-size:12px; }"
                       "QLabel { color:%2; }"
                       "QComboBox, QSpinBox, QLineEdit { background:%3; border:1px solid %4;"
                       " border-radius:8px; padding:5px 8px; color:%2; }"
                       "QComboBox:focus, QSpinBox:focus, QLineEdit:focus { border:1px solid %5; }")
            .arg(css(p.dim), css(p.text), css(p.surface), css(ctlBorder), css(p.accent)));
    setStyleSheet(styleSheet() + Theme::instance().spinBoxArrowQss());

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(8);

    auto* hint = new QLabel(tr("Render pages of this document to image files."), this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);
    root->addSpacing(4);

    auto* form = new QFormLayout;
    form->setSpacing(8);

    m_format = new QComboBox(this);
    m_format->addItem(tr("PNG image (*.png)"), int(ImageExporter::Format::Png));
    m_format->addItem(tr("JPEG image (*.jpg)"), int(ImageExporter::Format::Jpeg));
    m_format->addItem(tr("TIFF image (*.tiff)"), int(ImageExporter::Format::Tiff));
    form->addRow(tr("Format:"), m_format);

    m_dpi = new QSpinBox(this);
    m_dpi->setRange(36, 1200);
    m_dpi->setValue(150);
    m_dpi->setSingleStep(50);
    m_dpi->setSuffix(tr(" DPI"));
    form->addRow(tr("Resolution:"), m_dpi);

    m_pages = new QLineEdit(this);
    m_pages->setPlaceholderText(tr("all pages, or e.g. 1-3, 5, 8-10"));
    form->addRow(tr("Pages:"), m_pages);

    root->addLayout(form);
    root->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    auto* run = buttons->addButton(tr("Export"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    run->setObjectName(QStringLiteral("Share")); // accent-filled primary
    run->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    if (current >= 1 && current <= m_pageCount)
        m_pages->setPlaceholderText(
            tr("all pages, current is %1").arg(current));

    resize(420, 0);
}

ImageExporter::Format ExportImageDialog::format() const {
    return ImageExporter::Format(m_format->currentData().toInt());
}

int ExportImageDialog::dpi() const { return m_dpi->value(); }

QVector<int> ExportImageDialog::selectedPages() const {
    // Blank field means every page.
    const QString text = m_pages->text().trimmed();
    if (text.isEmpty()) {
        QVector<int> all;
        all.reserve(m_pageCount);
        for (int i = 0; i < m_pageCount; ++i)
            all.push_back(i);
        return all;
    }

    // Parse "1-3, 5, 8-10" into sorted, unique 0-based indices (mirrors Extract/
    // Split syntax). Out-of-range numbers are skipped.
    QVector<int> out;
    for (QString part : text.split(',', Qt::SkipEmptyParts)) {
        part = part.trimmed();
        if (part.contains('-')) {
            const QStringList e = part.split('-');
            if (e.size() != 2)
                continue;
            bool ok1 = false, ok2 = false;
            int a = e[0].trimmed().toInt(&ok1), b = e[1].trimmed().toInt(&ok2);
            if (!ok1 || !ok2)
                continue;
            if (a > b)
                std::swap(a, b);
            for (int n = a; n <= b; ++n)
                if (n >= 1 && n <= m_pageCount)
                    out.push_back(n - 1);
        } else {
            bool ok = false;
            const int n = part.toInt(&ok);
            if (ok && n >= 1 && n <= m_pageCount)
                out.push_back(n - 1);
        }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}
