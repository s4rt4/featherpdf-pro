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

#include "ui/SnapshotDialog.h"

#include "ui/Theme.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

SnapshotDialog::SnapshotDialog(const QImage& image, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Snapshot"));

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
    setStyleSheet(QStringLiteral("QLabel#Hint { color:%1; font-size:12px; }"
                                 "QLabel { color:%2; }"
                                 "QFrame#Preview { background:%3; border:1px solid %4;"
                                 " border-radius:8px; }")
                      .arg(css(p.dim), css(p.text), css(p.surface), css(ctlBorder)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(8);

    auto* hint = new QLabel(tr("Copied to the clipboard. Save it as a PNG too?"), this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);
    root->addSpacing(4);

    // A bounded preview of the captured region, centred on a panel.
    auto* panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("Preview"));
    auto* panelCol = new QVBoxLayout(panel);
    panelCol->setContentsMargins(12, 12, 12, 12);
    auto* preview = new QLabel(panel);
    preview->setAlignment(Qt::AlignCenter);
    QPixmap pm = QPixmap::fromImage(image);
    if (!pm.isNull())
        preview->setPixmap(pm.scaled(QSize(360, 280), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    panelCol->addWidget(preview);
    root->addWidget(panel, 1);
    root->addSpacing(4);

    auto* buttons = new QDialogButtonBox(this);
    auto* save = buttons->addButton(tr("Save as PNG…"), QDialogButtonBox::AcceptRole);
    buttons->addButton(tr("Close"), QDialogButtonBox::RejectRole);
    save->setObjectName(QStringLiteral("Share")); // accent-filled primary
    save->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    resize(420, 0);
}
