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

#include "ui/LinksDialog.h"

#include "ui/Theme.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

LinksDialog::LinksDialog(const QList<LinkEditor::Link>& links, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Edit Links"));

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
                       "QLabel#Dim { color:%1; }"
                       "QCheckBox, QLabel { color:%2; }"
                       "QScrollArea { border:none; }"
                       "QLineEdit { background:%3; border:1px solid %4; border-radius:8px;"
                       " padding:5px 8px; color:%2; }"
                       "QLineEdit:focus { border:1px solid %5; }")
            .arg(css(p.dim), css(p.text), css(p.surface), css(ctlBorder), css(p.accent)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(8);

    if (links.isEmpty()) {
        auto* empty = new QLabel(tr("This document has no links."), this);
        empty->setObjectName(QStringLiteral("Hint"));
        root->addWidget(empty);
    } else {
        auto* hint = new QLabel(tr("Edit a link's URL, or tick Delete to remove it."), this);
        hint->setObjectName(QStringLiteral("Hint"));
        hint->setWordWrap(true);
        root->addWidget(hint);
        root->addSpacing(4);

        auto* scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        auto* body = new QWidget(scroll);
        auto* grid = new QGridLayout(body);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(10);
        grid->setVerticalSpacing(8);
        grid->setColumnStretch(1, 1);

        int r = 0;
        for (const LinkEditor::Link& link : links) {
            Row row;
            row.link = link;

            auto* page = new QLabel(tr("Page %1").arg(link.page + 1), body);
            grid->addWidget(page, r, 0);

            if (link.isUri) {
                row.url = new QLineEdit(link.uri, body);
                grid->addWidget(row.url, r, 1);
            } else {
                auto* ro = new QLabel(tr("internal link"), body);
                ro->setObjectName(QStringLiteral("Dim"));
                grid->addWidget(ro, r, 1);
            }

            row.del = new QCheckBox(tr("Delete"), body);
            grid->addWidget(row.del, r, 2);

            m_rows.push_back(row);
            ++r;
        }
        body->setLayout(grid);
        scroll->setWidget(body);
        root->addWidget(scroll, 1);
        resize(560, qMin(520, 160 + links.size() * 44));
    }

    auto* buttons = new QDialogButtonBox(this);
    auto* apply = buttons->addButton(tr("Apply"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    apply->setObjectName(QStringLiteral("Share"));
    apply->setCursor(Qt::PointingHandCursor);
    apply->setEnabled(!links.isEmpty());
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

QList<LinkEditor::Edit> LinksDialog::edits() const {
    QList<LinkEditor::Edit> out;
    for (const Row& row : m_rows) {
        LinkEditor::Edit e;
        e.objId = row.link.objId;
        e.objGen = row.link.objGen;
        if (row.del && row.del->isChecked()) {
            e.remove = true;
            out.push_back(e);
            continue;
        }
        if (row.url) {
            const QString text = row.url->text().trimmed();
            if (!text.isEmpty() && text != row.link.uri) {
                e.newUri = text;
                out.push_back(e);
            }
        }
    }
    return out;
}
